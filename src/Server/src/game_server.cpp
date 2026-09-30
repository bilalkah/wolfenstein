#include "Server/game_server.h"
#include "Core/scene_loader.h"
#include "SoundManager/sound_manager.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <ranges>
#include <utility>

namespace wolfenstein {

std::expected<std::unique_ptr<GameServer>, std::string> GameServer::Create(
	const std::string& asset_dir, std::vector<std::string> arenas,
	Outbox& outbox, const MatchSettings& settings) {
	auto loader = SceneLoader::Open(asset_dir);
	if (!loader) {
		return std::unexpected(loader.error());
	}
	// The pictures' sizes and masks, which shots are judged against; no
	// textures, as nothing is drawn
	std::ifstream manifest_file(asset_dir + "textures.json");
	auto manifest = ParseTextureManifest(manifest_file);
	if (!manifest) {
		return std::unexpected("textures.json: " + manifest.error());
	}
	auto textures = TextureManager::Load(nullptr, *manifest, asset_dir);
	if (!textures) {
		return std::unexpected(textures.error());
	}
	// Silent: no one listens on a server
	auto world = std::make_unique<World>(**textures, std::move(*loader),
										 std::make_unique<SoundManager>());
	if (arenas.empty()) {
		arenas = world->Config().arenas;
	}
	if (arenas.empty()) {
		return std::unexpected(std::string("no arena to play on"));
	}
	// Each arena started once, the first last, so a later match cannot
	// fail to start
	for (const std::string& arena : std::views::reverse(arenas)) {
		if (auto started = world->NewMatch(arena, std::nullopt); !started) {
			return std::unexpected(started.error());
		}
	}
	if (settings.mode == net::MatchMode::GunRace &&
		world->Config().gun_race.empty()) {
		return std::unexpected(
			std::string("a gun race needs the configuration's gun_race"));
	}
	return std::make_unique<GameServer>(std::move(*textures), std::move(world),
										std::move(arenas), outbox, settings);
}

GameServer::GameServer(std::unique_ptr<TextureManager> textures,
					   std::unique_ptr<World> world,
					   std::vector<std::string> arenas, Outbox& outbox,
					   const MatchSettings& settings)
	: textures_(std::move(textures)),
	  world_(std::move(world)),
	  arenas_(std::move(arenas)),
	  level_(arenas_.front()),
	  outbox_(outbox),
	  rules_(*world_, settings) {
	Watch(world_->CurrentLevel());
}

void GameServer::Watch(Scene& scene) {
	scene.RecordEvents(true);
	scene.SetHindsight(this);
	past_ = {};
	Remember();
}

void GameServer::NextArena() {
	if (arenas_.size() < 2) {
		return;	 // the next match here
	}
	arena_ = (arena_ + 1) % arenas_.size();
	level_ = net::LevelName(arenas_[arena_]);
	// Each arena was started once already: it starts again
	if (auto started = world_->NewMatch(arenas_[arena_], std::nullopt);
		!started) {
		return;
	}
	for (const Client& client : clients_) {
		// No room for it there (cannot be: every slot is free): it goes
		if (client.open && client.slot && !world_->JoinPlayer(*client.slot)) {
			outbox_.Close(client.id);
		}
	}
	Watch(world_->CurrentLevel());
	for (const Client& client : clients_) {
		if (client.open && client.slot) {
			Send(client.id,
				 net::Welcome{.slot = static_cast<std::uint8_t>(*client.slot),
							  .tick = tick_,
							  .level = level_});
		}
	}
}

std::size_t GameServer::PlayerCount() const {
	return static_cast<std::size_t>(
		std::ranges::count_if(clients_, [](const Client& client) {
			return client.open && client.slot.has_value();
		}));
}

GameServer::Client* GameServer::Find(ClientId id) {
	const auto found = std::ranges::find_if(
		clients_, [id](const Client& c) { return c.open && c.id == id; });
	return found != clients_.end() ? &*found : nullptr;
}

void GameServer::Connect(ClientId client) {
	const auto free = std::ranges::find(clients_, false, &Client::open);
	if (free == clients_.end()) {
		outbox_.Close(client);
		return;
	}
	*free = Client{.open = true, .id = client, .heard = tick_};
}

void GameServer::Receive(ClientId id, std::span<const std::uint8_t> message) {
	Client* client = Find(id);
	if (client == nullptr) {
		return;
	}
	client->heard = tick_;
	const auto decoded = net::Decode(message);
	if (!decoded) {
		// Not the game's protocol: whoever it is, they go
		outbox_.Close(id);
		return;
	}
	if (const auto* hello = std::get_if<net::Hello>(&*decoded)) {
		Hello(*client, *hello);
	}
	else if (const auto* input = std::get_if<net::Input>(&*decoded)) {
		Input(*client, *input);
	}
}

void GameServer::Hello(Client& client, const net::Hello& hello) {
	if (client.slot) {
		return;	 // in already
	}
	if (hello.version != net::kProtocolVersion) {
		Send(client.id, net::Reject{.reason = net::RejectReason::Version});
		outbox_.Close(client.id);
		return;
	}
	// A player of this name not heard from for a while: its connection
	// dropped, and this is it coming back; the old one goes first
	const auto stale = static_cast<std::uint32_t>(kStaleSeconds / kTickSeconds);
	for (const Client& other : clients_) {
		if (&other != &client && other.open && other.slot &&
			!hello.name.View().empty() && other.name == hello.name &&
			tick_ - other.heard > stale) {
			const ClientId gone = other.id;
			outbox_.Close(gone);
			Disconnect(gone);
		}
	}
	for (std::size_t slot = 0; slot < Scene::kMaxPlayers; ++slot) {
		if (world_->FindPlayer(slot) == nullptr && world_->JoinPlayer(slot)) {
			client.slot = slot;
			client.name = hello.name;
			client.seen = tick_;
			rules_.Join(slot);
			// Back soon under the same name: its score is its own again
			const auto back =
				static_cast<std::uint32_t>(kComebackSeconds / kTickSeconds);
			for (Departed& gone : departed_) {
				if (!hello.name.View().empty() && gone.name == hello.name &&
					gone.match == rules_.MatchNumber() &&
					tick_ - gone.tick <= back) {
					rules_.Restore(slot, gone.standing);
					gone = {};
					break;
				}
			}
			scores_changed_ = true;
			Send(client.id,
				 net::Welcome{.slot = static_cast<std::uint8_t>(slot),
							  .tick = tick_,
							  .level = level_});
			return;
		}
	}
	Send(client.id, net::Reject{.reason = net::RejectReason::Full});
	outbox_.Close(client.id);
}

void GameServer::Input(Client& client, const net::Input& input) {
	if (!client.slot) {
		return;	 // commands before hello: nothing to apply them to
	}
	if (input.count == 0) {
		return;
	}
	const std::uint32_t newest = input.commands[input.count - 1].sequence;
	for (std::size_t i = 0; i < input.count; ++i) {
		const net::NumberedCommand& numbered = input.commands[i];
		// Each command once: one sent again, or already applied, is not
		if (numbered.sequence <= client.received) {
			continue;
		}
		if (client.queued == kQueue) {
			// Full: the oldest gives way
			client.head = (client.head + 1) % kQueue;
			--client.queued;
		}
		// A tick before the newest, its game showed the others a tick
		// earlier
		client.queue[(client.head + client.queued) % kQueue] = {
			.numbered = numbered,
			.seen = input.seen - (newest - numbered.sequence)};
		++client.queued;
		client.received = numbered.sequence;
	}
}

PlayerCommand GameServer::NextCommand(Client& client) {
	// Far ahead (its commands came in a burst after a stall): caught up to
	// the last two, so what it does is never long behind what it sent
	if (client.queued > kMaxQueued) {
		constexpr std::size_t kKept = 2;
		client.head = (client.head + client.queued - kKept) % kQueue;
		client.queued = kKept;
	}
	if (client.queued > 0) {
		const Queued& next = client.queue[client.head];
		client.head = (client.head + 1) % kQueue;
		--client.queued;
		client.applied = next.numbered.sequence;
		client.last = next.numbered.command;
		client.seen = next.seen;
		return client.last;
	}
	// Late: it goes on as it was, its game a tick on
	client.last = Repeated(client.last);
	++client.seen;
	return client.last;
}

void GameServer::Disconnect(ClientId id) {
	Client* client = Find(id);
	if (client == nullptr) {
		return;
	}
	if (client->slot) {
		departed_[next_departed_] = {
			.name = client->name,
			.standing = rules_.StandingOf(*client->slot),
			.match = rules_.MatchNumber(),
			.tick = tick_};
		next_departed_ = (next_departed_ + 1) % departed_.size();
		world_->LeavePlayer(*client->slot);
		rules_.Leave(*client->slot);
		scores_changed_ = true;
	}
	*client = Client{};
}

void GameServer::Tick() {
	wants_back_ = {};
	for (Client& client : clients_) {
		if (!client.open || !client.slot) {
			continue;
		}
		if (Player* player = world_->FindPlayer(*client.slot)) {
			const PlayerCommand command = NextCommand(client);
			player->SetCommand(command);
			seen_[*client.slot] = client.seen;
			wants_back_[*client.slot] = command.fire || command.use;
		}
	}
	Scene& scene = world_->CurrentLevel();
	scene.Update(kTickSeconds);
	++tick_;
	Remember();
	// What happened since the last tick's were told: this tick's
	scores_changed_ = rules_.Tick(kTickSeconds, scene.Events(), wants_back_) ||
					  scores_changed_;
	SendEvents(scene.Events());
	scene.ClearEvents();
	// The result shown: the next match, on the next arena
	if (rules_.IntermissionOver()) {
		NextArena();
		rules_.Restart();
		scores_changed_ = true;
	}
	if (scores_changed_ || tick_ % kScoresEvery == 0) {
		SendScores();
		scores_changed_ = false;
	}
	if (tick_ % kSnapshotEvery == 0) {
		SendSnapshots();
	}
}

void GameServer::Remember() {
	Places& places = past_[tick_ % kRewind];
	for (std::size_t slot = 0; slot < Scene::kMaxPlayers; ++slot) {
		const Player* player = world_->FindPlayer(slot);
		places[slot] = player != nullptr && player->IsAlive()
						   ? std::optional(player->GetPose())
						   : std::nullopt;
	}
}

std::optional<vector2d> GameServer::Seen(std::size_t shooter,
										 std::size_t target) const {
	// No further back than remembered, and not ahead of now
	const std::uint32_t oldest =
		tick_ >= kRewind - 1 ? tick_ - (kRewind - 1) : 0;
	const std::uint32_t seen = std::clamp(seen_[shooter], oldest, tick_);
	return past_[seen % kRewind][target];
}

void GameServer::SendSnapshots() {
	net::Snapshot snapshot{.tick = tick_};
	for (std::size_t slot = 0; slot < Scene::kMaxPlayers; ++slot) {
		const Player* player = world_->FindPlayer(slot);
		if (player == nullptr) {
			continue;
		}
		const Position2D& at = player->GetPosition();
		snapshot.players[snapshot.count++] = {
			.slot = static_cast<std::uint8_t>(slot),
			.alive = player->IsAlive(),
			.shielded = player->IsProtected(),
			.pose = at.pose,
			.theta = at.theta,
			.pitch = player->GetPitch(),
			.health = static_cast<std::uint8_t>(
				std::clamp(std::lround(player->GetHealth()), 0L, 255L)),
			.weapon = static_cast<std::uint8_t>(player->HeldWeapon())};
	}
	// The pickups not lying there, the same for everyone
	const auto pickups = world_->CurrentLevel().GetPickups();
	snapshot.pickups =
		static_cast<std::uint8_t>(std::min(pickups.size(), net::kMaxPickups));
	for (std::size_t i = 0; i < snapshot.pickups; ++i) {
		if (pickups[i]->IsTaken()) {
			snapshot.taken |= std::uint64_t{1} << i;
		}
	}
	for (const Client& client : clients_) {
		const Player* player = client.open && client.slot
								   ? world_->FindPlayer(*client.slot)
								   : nullptr;
		if (player == nullptr) {
			continue;
		}
		snapshot.ack = client.applied;
		// What it carries, its own to know
		net::Inventory& own = snapshot.own;
		own.owned = player->GetOwnedWeapons();
		own.count = static_cast<std::uint8_t>(
			std::min(player->WeaponCount(), net::kMaxWeapons));
		for (std::size_t i = 0; i < own.count; ++i) {
			const Weapon& weapon = player->GetWeapon(i);
			own.rounds[i] = {
				.ammo = static_cast<std::uint8_t>(
					std::min<std::size_t>(weapon.GetAmmo(), 0xFF)),
				.reserve = static_cast<std::uint16_t>(
					std::min<std::size_t>(weapon.GetReserve(), 0xFFFF))};
		}
		Send(client.id, snapshot);
	}
}

void GameServer::SendEvents(std::span<const MatchEvent> events) {
	// As many messages as it takes, each as full as it goes
	for (std::size_t first = 0; first < events.size();
		 first += net::kMaxEvents) {
		net::Events message{.tick = tick_};
		const std::size_t count =
			std::min(events.size() - first, net::kMaxEvents);
		for (std::size_t i = 0; i < count; ++i) {
			const MatchEvent& event = events[first + i];
			message.events[i] = {
				.type = static_cast<net::EventType>(event.type),
				.slot = event.slot,
				.other = event.other,
				.weapon = event.weapon,
				.pose = event.at,
				.theta = event.theta};
		}
		message.count = static_cast<std::uint8_t>(count);
		Broadcast(message);
	}
}

void GameServer::SendScores() {
	const MatchRules& rules = rules_;
	const MatchSettings& settings = rules.Settings();
	net::Scores scores{
		.mode = settings.mode,
		.phase = rules.Phase(),
		.frag_limit =
			static_cast<std::uint8_t>(std::clamp(settings.frag_limit, 0, 255)),
		.seconds_left = static_cast<std::uint16_t>(
			std::min(std::ceil(rules.SecondsLeft()), 65535.0)),
		.winner = rules.Winner() ? static_cast<std::uint8_t>(*rules.Winner())
								 : net::kNoWinner};
	for (const Client& client : clients_) {
		if (!client.open || !client.slot) {
			continue;
		}
		const Standing& standing = rules.StandingOf(*client.slot);
		scores.players[scores.count++] = {
			.slot = static_cast<std::uint8_t>(*client.slot),
			.name = client.name,
			.frags = static_cast<std::int16_t>(standing.frags),
			.deaths = static_cast<std::uint16_t>(standing.deaths),
			.step = static_cast<std::uint8_t>(standing.step)};
	}
	Broadcast(scores);
}

void GameServer::Broadcast(const net::Message& message) {
	const std::size_t size = net::Encode(message, buffer_);
	if (size == 0) {
		return;
	}
	for (const Client& client : clients_) {
		if (client.open && client.slot) {
			outbox_.Send(client.id, std::span(buffer_).first(size));
		}
	}
}

void GameServer::Send(ClientId client, const net::Message& message) {
	const std::size_t size = net::Encode(message, buffer_);
	if (size > 0) {
		outbox_.Send(client, std::span(buffer_).first(size));
	}
}

}  // namespace wolfenstein
