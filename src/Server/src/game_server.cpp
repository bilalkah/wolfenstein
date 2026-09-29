#include "Server/game_server.h"
#include "Core/scene_loader.h"
#include "SoundManager/sound_manager.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <utility>

namespace wolfenstein {

std::expected<std::unique_ptr<GameServer>, std::string> GameServer::Create(
	const std::string& asset_dir, const std::string& level, Outbox& outbox) {
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
	if (auto started = world->NewMatch(level, std::nullopt); !started) {
		return std::unexpected(started.error());
	}
	return std::make_unique<GameServer>(std::move(*textures), std::move(world),
										level, outbox);
}

GameServer::GameServer(std::unique_ptr<TextureManager> textures,
					   std::unique_ptr<World> world, const std::string& level,
					   Outbox& outbox)
	: textures_(std::move(textures)),
	  world_(std::move(world)),
	  level_(level),
	  outbox_(outbox) {}

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
	*free = Client{.open = true, .id = client};
}

void GameServer::Receive(ClientId id, std::span<const std::uint8_t> message) {
	Client* client = Find(id);
	if (client == nullptr) {
		return;
	}
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
	for (std::size_t slot = 0; slot < Scene::kMaxPlayers; ++slot) {
		if (world_->FindPlayer(slot) == nullptr && world_->JoinPlayer(slot)) {
			client.slot = slot;
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
		client.queue[(client.head + client.queued) % kQueue] = numbered;
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
		const net::NumberedCommand& next = client.queue[client.head];
		client.head = (client.head + 1) % kQueue;
		--client.queued;
		client.applied = next.sequence;
		client.last = next.command;
		return next.command;
	}
	// Late: it goes on as it was, moving and holding the trigger, but a key
	// pressed once is not pressed again
	PlayerCommand held = client.last;
	held.use = false;
	held.reload = false;
	held.weapon = -1;
	held.cycle = 0;
	client.last = held;
	return held;
}

void GameServer::Disconnect(ClientId id) {
	Client* client = Find(id);
	if (client == nullptr) {
		return;
	}
	if (client->slot) {
		world_->LeavePlayer(*client->slot);
	}
	*client = Client{};
}

void GameServer::Tick() {
	for (Client& client : clients_) {
		if (!client.open || !client.slot) {
			continue;
		}
		if (Player* player = world_->FindPlayer(*client.slot)) {
			player->SetCommand(NextCommand(client));
		}
	}
	world_->CurrentLevel().Update(kTickSeconds);
	++tick_;
	if (tick_ % kSnapshotEvery == 0) {
		SendSnapshots();
	}
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
			.pose = at.pose,
			.theta = at.theta,
			.pitch = player->GetPitch(),
			.health = static_cast<std::uint8_t>(
				std::clamp(std::lround(player->GetHealth()), 0L, 255L)),
			.weapon = static_cast<std::uint8_t>(player->HeldWeapon())};
	}
	for (const Client& client : clients_) {
		if (client.open && client.slot) {
			snapshot.ack = client.applied;
			Send(client.id, snapshot);
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
