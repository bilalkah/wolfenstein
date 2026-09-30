#include "Client/match_client.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <utility>
#include <variant>

namespace karakale {

MatchClient::MatchClient(std::unique_ptr<net::Connection> connection,
						 std::string_view name, Clock clock)
	: connection_(std::move(connection)),
	  name_(net::MakeName(name)),
	  clock_(std::move(clock)) {}

void MatchClient::Poll(World& world) {
	if (connection_ == nullptr) {
		state_ = State::Closed;
		return;
	}
	if (state_ == State::Connecting &&
		connection_->GetState() == net::Connection::State::Open) {
		Send(net::Hello{.name = name_});
		state_ = State::Joining;
	}
	while (const auto received = connection_->Receive(buffer_)) {
		const auto message =
			net::Decode(std::span(buffer_).first(received->size));
		if (!message) {
			continue;  // not the game's: nothing to do with it
		}
		if (const auto* welcome = std::get_if<net::Welcome>(&*message);
			welcome != nullptr &&
			(state_ == State::Joining || state_ == State::Playing)) {
			OnWelcome(world, *welcome);
		}
		else if (const auto* reject = std::get_if<net::Reject>(&*message)) {
			state_ = State::Rejected;
			reason_ = reject->reason;
		}
		else if (const auto* snapshot = std::get_if<net::Snapshot>(&*message);
				 snapshot != nullptr && state_ == State::Playing) {
			OnSnapshot(world, *snapshot);
		}
		else if (const auto* events = std::get_if<net::Events>(&*message);
				 events != nullptr && state_ == State::Playing) {
			OnEvents(world, *events);
		}
		else if (const auto* scores = std::get_if<net::Scores>(&*message);
				 scores != nullptr && state_ == State::Playing) {
			scores_ = *scores;
		}
		else if (const auto* pong = std::get_if<net::Pong>(&*message)) {
			OnPong(*pong, received->arrived);
		}
		else if (const auto* pings = std::get_if<net::Pings>(&*message)) {
			server_pings_ = true;
			pings_ = {};
			for (const net::PlayerPing& ping :
				 std::span(pings->players).first(pings->count)) {
				pings_[ping.slot] = ping.rtt;
			}
		}
	}
	if (state_ == State::Playing) {
		KeepTime();
	}
	if (connection_->GetState() == net::Connection::State::Closed &&
		state_ != State::Rejected) {
		state_ = State::Closed;
	}
}

void MatchClient::OnWelcome(World& world, const net::Welcome& welcome) {
	if (auto started = world.NewMatch(welcome.level.View(), welcome.slot);
		!started) {
		std::cerr << "Cannot start the match: " << started.error() << '\n';
		state_ = State::Closed;
		return;
	}
	// The server judges what happens to the players here
	world.CurrentLevel().SetJudging(false);
	// Welcomed again, to the next arena: the commands go on numbered as
	// they were (the server has them so), but where they took the player
	// is in the last arena
	if (state_ == State::Playing) {
		new_level_ = true;
	}
	else {
		sequence_ = 0;
		scores_ = {};
		kill_count_ = 0;
	}
	slot_ = welcome.slot;
	history_ = {};
	snapshot_count_ = 0;
	snapshot_next_ = 0;
	latest_tick_ = welcome.tick;
	ticks_since_ = 0;
	shown_tick_ = welcome.tick;
	revived_ = false;
	state_ = State::Playing;
}

std::string_view MatchClient::NameOf(std::size_t slot) const {
	const auto players = std::span(scores_.players).first(scores_.count);
	const auto found = std::ranges::find(
		players, static_cast<std::uint8_t>(slot), &net::Score::slot);
	return found != players.end() ? found->name.View() : std::string_view();
}

void MatchClient::BeforeTick(World& world, const PlayerCommand& command) {
	if (state_ != State::Playing) {
		return;
	}
	++sequence_;
	history_[sequence_ % kHistory] = {.sequence = sequence_,
									  .command = command,
									  .reached = {},
									  .carried = {}};
	// The newest command and the three before it, oldest first, and when
	// the others were as the player saw them
	net::Input input{.count = static_cast<std::uint8_t>(std::min<std::uint32_t>(
						 net::kInputCommands, sequence_)),
					 .seen = shown_tick_};
	const std::uint32_t first = sequence_ - input.count + 1;
	for (std::uint8_t i = 0; i < input.count; ++i) {
		const Sent& sent = history_[(first + i) % kHistory];
		input.commands[i] = {.sequence = first + i, .command = sent.command};
	}
	Send(input);
	world.GetPlayer().SetCommand(command);
}

void MatchClient::AfterTick(World& world) {
	if (state_ != State::Playing) {
		return;
	}
	Sent& sent = history_[sequence_ % kHistory];
	sent.reached = world.GetPlayer().GetPosition();
	sent.carried = CarriedBy(world.GetPlayer());
	++ticks_since_;
	// Each kill in the feed a tick older; the oldest gone once old enough
	for (Kill& kill : std::span(kills_).first(kill_count_)) {
		kill.age += net::kTickSeconds;
	}
	while (kill_count_ > 0 && kills_[0].age >= kKillSeconds) {
		std::ranges::copy(std::span(kills_).subspan(1, kill_count_ - 1),
						  kills_.begin());
		--kill_count_;
	}
	if (snapshot_count_ == 0) {
		return;
	}
	const double tick = static_cast<double>(latest_tick_) + ticks_since_ -
						static_cast<double>(kInterpolationTicks);
	shown_tick_ = static_cast<std::uint32_t>(std::max(std::lround(tick), 0L));
	for (std::size_t slot = 0; slot < Scene::kMaxPlayers; ++slot) {
		Player* player = world.FindPlayer(slot);
		if (slot == slot_ || player == nullptr || !player->IsPuppet()) {
			continue;
		}
		if (const auto state = Interpolate(slot, tick)) {
			player->Follow(Position2D(state->pose, state->theta), state->pitch,
						   state->health, state->alive);
			// Shielded until the next placing, a tick on
			player->Protect(state->shielded ? 2 * net::kTickSeconds : 0.0);
		}
	}
}

MatchClient::Carried MatchClient::CarriedBy(const Player& player) {
	Carried carried{.owned = player.GetOwnedWeapons(),
					.held = static_cast<std::uint8_t>(player.HeldWeapon())};
	for (std::size_t i = 0;
		 i < std::min(player.WeaponCount(), net::kMaxWeapons); ++i) {
		const Weapon& weapon = player.GetWeapon(i);
		carried.rounds[i] = {
			.ammo = static_cast<std::uint8_t>(
				std::min<std::size_t>(weapon.GetAmmo(), 0xFF)),
			.reserve = static_cast<std::uint16_t>(
				std::min<std::size_t>(weapon.GetReserve(), 0xFFFF))};
	}
	return carried;
}

void MatchClient::OnSnapshot(World& world, const net::Snapshot& snapshot) {
	if (snapshot_count_ > 0 && snapshot.tick <= latest_tick_) {
		return;	 // older than one already come
	}
	snapshots_[snapshot_next_] = snapshot;
	snapshot_next_ = (snapshot_next_ + 1) % kSnapshots;
	snapshot_count_ = std::min(snapshot_count_ + 1, kSnapshots);
	latest_tick_ = snapshot.tick;
	ticks_since_ = 0;
	Attend(world, snapshot);
	for (std::size_t i = 0; i < snapshot.count; ++i) {
		const net::PlayerState& state = snapshot.players[i];
		if (state.slot == slot_) {
			Revive(world, state);
			Reconcile(world, state, snapshot.ack);
			Restock(world, snapshot.own, state.weapon, snapshot.ack);
		}
	}
	ShowPickups(world, snapshot);
}

void MatchClient::Revive(World& world, const net::PlayerState& state) {
	Player& player = world.GetPlayer();
	const bool back = state.alive && !player.IsAlive();
	player.TakeVitals(state.health, state.alive);
	player.Protect(state.shielded ? 2 * net::kTickSeconds : 0.0);
	if (back) {
		// Where the server brought it in, looking where it looks there: not
		// drawn sliding across the level to it
		player.SetPosition(Position2D(state.pose, state.theta));
		revived_ = true;
	}
}

void MatchClient::Restock(World& world, const net::Inventory& own,
						  std::uint8_t held, std::uint32_t ack) {
	Player& player = world.GetPlayer();
	const auto weapons = std::min<std::size_t>(
		{own.count, player.WeaponCount(), net::kMaxWeapons});
	const Sent& at = history_[ack % kHistory];
	// Nothing foreseen to compare with: the server's, as it is
	const bool compare = ack != 0 && at.sequence == ack;
	const Carried foreseen = compare ? at.carried : CarriedBy(player);
	// The weapons the server's differ in, taken or given now
	const auto differ = static_cast<std::uint8_t>(foreseen.owned ^ own.owned);
	const auto fix_owned = [&](std::uint8_t owned) {
		return static_cast<std::uint8_t>((owned & ~differ) |
										 (own.owned & differ));
	};
	if (differ != 0) {
		player.SetOwnedWeapons(fix_owned(player.GetOwnedWeapons()));
	}
	// Each weapon's rounds, by how far the server's are from the foreseen
	std::array<int, net::kMaxWeapons> ammo_off{};
	std::array<int, net::kMaxWeapons> reserve_off{};
	for (std::size_t i = 0; i < weapons; ++i) {
		ammo_off[i] = own.rounds[i].ammo - foreseen.rounds[i].ammo;
		reserve_off[i] = own.rounds[i].reserve - foreseen.rounds[i].reserve;
		if (ammo_off[i] != 0 || reserve_off[i] != 0) {
			Weapon& weapon = player.GetWeapon(i);
			weapon.SetRounds(
				static_cast<std::size_t>(std::max(
					static_cast<int>(weapon.GetAmmo()) + ammo_off[i], 0)),
				static_cast<std::size_t>(std::max(
					static_cast<int>(weapon.GetReserve()) + reserve_off[i],
					0)));
		}
	}
	// Another weapon in hand than foreseen: the server's (a new one given)
	const bool swapped = held != foreseen.held && held < player.WeaponCount();
	if (swapped) {
		player.TakeInHand(held);
	}
	if (!compare) {
		return;
	}
	// What the commands since foresaw, made good the same way, so the next
	// snapshot is not put right twice
	for (std::uint32_t sequence = ack + 1; sequence <= sequence_; ++sequence) {
		Carried& later = history_[sequence % kHistory].carried;
		later.owned = fix_owned(later.owned);
		for (std::size_t i = 0; i < weapons; ++i) {
			later.rounds[i].ammo = static_cast<std::uint8_t>(
				std::clamp(later.rounds[i].ammo + ammo_off[i], 0, 0xFF));
			later.rounds[i].reserve = static_cast<std::uint16_t>(std::clamp(
				later.rounds[i].reserve + reserve_off[i], 0, 0xFFFF));
		}
		if (swapped) {
			later.held = held;
		}
	}
}

void MatchClient::ShowPickups(World& world, const net::Snapshot& snapshot) {
	const auto pickups = world.CurrentLevel().GetPickups();
	for (std::size_t i = 0;
		 i < std::min<std::size_t>(snapshot.pickups, pickups.size()); ++i) {
		const bool taken = (snapshot.taken >> i & 1U) != 0;
		if (taken && !pickups[i]->IsTaken()) {
			pickups[i]->Take();
		}
		else if (!taken && pickups[i]->IsTaken()) {
			pickups[i]->Restore();
		}
	}
}

void MatchClient::OnEvents(World& world, const net::Events& events) {
	Scene& scene = world.CurrentLevel();
	const auto& arsenal = world.Config().weapons;
	// A shot's sound, one at a time for each player
	constexpr std::uint32_t kShotSources = 0x20000;
	for (const net::Event& event :
		 std::span(events.events).first(events.count)) {
		const bool mine = event.slot == slot_;
		Player* player = world.FindPlayer(event.slot);
		const WeaponConfig* weapon =
			event.weapon < arsenal.size() ? &arsenal[event.weapon] : nullptr;
		switch (event.type) {
			case net::EventType::Shot:
			case net::EventType::Launch:
				// The local player's own shots were seen and heard at once
				if (mine || player == nullptr || weapon == nullptr) {
					break;
				}
				scene.FigureFired(event.slot);
				if (weapon->shot_sound) {
					scene.PlaySoundAt(*weapon->shot_sound, player->GetPose(),
									  kShotSources + event.slot);
				}
				// Its rocket flies from where the player is seen, harmless
				// here: the server judges where it bursts
				if (event.type == net::EventType::Launch &&
					weapon->projectile) {
					scene.Launch(*weapon->projectile, player->GetPose(),
								 event.theta, 0.0, event.slot, event.weapon);
				}
				break;
			case net::EventType::Hurt: {
				// Blood where the one struck is seen, unless the local player
				// struck it (it saw that already) or is it
				const Player* struck = world.FindPlayer(event.other);
				if (!mine && event.other != slot_ && struck != nullptr) {
					constexpr double kChest = 0.45;
					scene.ShowImpact(Scene::Impact::Blood, struck->GetPose(),
									 kChest);
				}
				break;
			}
			case net::EventType::Kill:
				NoteKill(event);
				break;
			case net::EventType::Pickup: {
				const auto pickups = scene.GetPickups();
				if (event.other < pickups.size()) {
					pickups[event.other]->Take();
					if (mine) {
						world.GetPlayer().ShowPickup(
							pickups[event.other]->GetEffect());
					}
				}
				break;
			}
		}
	}
}

void MatchClient::NoteKill(const net::Event& event) {
	if (kill_count_ == kKillFeed) {
		std::ranges::copy(std::span(kills_).subspan(1), kills_.begin());
		--kill_count_;
	}
	kills_[kill_count_++] = {.killer = event.slot,
							 .victim = event.other,
							 .weapon = event.weapon,
							 .age = 0.0};
}

void MatchClient::Attend(World& world, const net::Snapshot& snapshot) {
	std::array<bool, Scene::kMaxPlayers> present{};
	for (std::size_t i = 0; i < snapshot.count; ++i) {
		present[snapshot.players[i].slot] = true;
	}
	for (std::size_t slot = 0; slot < Scene::kMaxPlayers; ++slot) {
		if (slot == slot_) {
			continue;
		}
		const Player* player = world.FindPlayer(slot);
		if (present[slot] && player == nullptr) {
			if (world.JoinPlayer(slot)) {
				world.FindPlayer(slot)->SetPuppet(true);
			}
		}
		else if (!present[slot] && player != nullptr) {
			world.LeavePlayer(slot);
		}
	}
}

void MatchClient::Reconcile(World& world, const net::PlayerState& state,
							std::uint32_t ack) {
	Player& player = world.GetPlayer();
	const Sent& at = history_[ack % kHistory];
	// What was foreseen for command `ack`; not remembered (none of ours
	// applied yet, too long ago, or in the last arena): where the player
	// stands now
	const bool remembered = ack != 0 && at.sequence == ack;
	const vector2d foreseen = remembered ? at.reached.pose : player.GetPose();
	if (foreseen.Distance(state.pose) <= kTolerance) {
		return;	 // predicted right
	}
	++corrections_;
	player.Correct(Position2D(state.pose, state.theta));
	// The commands since, those remembered, played again from there
	const std::uint32_t oldest =
		sequence_ >= kHistory ? sequence_ - kHistory + 1 : 1;
	for (std::uint32_t sequence = std::max(ack + 1, oldest);
		 sequence <= sequence_; ++sequence) {
		Sent& sent = history_[sequence % kHistory];
		if (sent.sequence != sequence) {
			continue;
		}
		player.Replay(sent.command, net::kTickSeconds);
		sent.reached = player.GetPosition();
	}
}

std::optional<net::PlayerState> MatchClient::Interpolate(std::size_t slot,
														 double tick) const {
	const net::PlayerState* before = nullptr;
	const net::PlayerState* after = nullptr;
	double before_tick = 0.0;
	double after_tick = 0.0;
	for (std::size_t i = 0; i < snapshot_count_; ++i) {
		const net::Snapshot& snapshot = snapshots_[i];
		const auto found = std::ranges::find(
			snapshot.players.begin(), snapshot.players.begin() + snapshot.count,
			static_cast<std::uint8_t>(slot), &net::PlayerState::slot);
		if (found == snapshot.players.begin() + snapshot.count) {
			continue;
		}
		const auto at = static_cast<double>(snapshot.tick);
		if (at <= tick && (before == nullptr || at > before_tick)) {
			before = &*found;
			before_tick = at;
		}
		if (at >= tick && (after == nullptr || at < after_tick)) {
			after = &*found;
			after_tick = at;
		}
	}
	if (before == nullptr || after == nullptr || after_tick == before_tick) {
		// Past the newest, or before the oldest: as the nearest has it
		const net::PlayerState* nearest = before != nullptr ? before : after;
		return nearest != nullptr ? std::optional(*nearest) : std::nullopt;
	}
	const double t = (tick - before_tick) / (after_tick - before_tick);
	net::PlayerState state = *before;
	state.pose = before->pose + (after->pose - before->pose) * t;
	const double turn =
		std::remainder(after->theta - before->theta, 2.0 * std::numbers::pi);
	state.theta = before->theta + turn * t;
	state.pitch = before->pitch + (after->pitch - before->pitch) * t;
	return state;
}

void MatchClient::KeepTime() {
	const double now = clock_();
	if (!server_pings_ || (pinged_ && now - last_ping_ < kPingSeconds)) {
		return;
	}
	pinged_ = true;
	last_ping_ = now;
	constexpr double kMilliseconds = 1000.0;
	Send(net::Ping{.stamp = static_cast<std::uint32_t>(
					   static_cast<std::uint64_t>(now * kMilliseconds)),
				   .rtt = rtt_});
}

void MatchClient::OnPong(const net::Pong& pong, double arrived) {
	constexpr double kMilliseconds = 1000.0;
	const auto now = static_cast<std::uint32_t>(
		static_cast<std::uint64_t>(arrived * kMilliseconds));
	// The clock's milliseconds wrap round: the difference does not
	const std::uint32_t took = now - pong.stamp;
	constexpr std::uint32_t kLongest = 9999;
	rtt_ = static_cast<std::uint16_t>(
		std::clamp<std::uint32_t>(took, 1, kLongest));
}

void MatchClient::Send(const net::Message& message) {
	std::array<std::uint8_t, net::kMaxMessage> buffer{};
	const std::size_t size = net::Encode(message, buffer);
	if (size > 0) {
		connection_->Send(std::span(buffer).first(size));
	}
}

}  // namespace karakale
