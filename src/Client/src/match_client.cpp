#include "Client/match_client.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <utility>
#include <variant>

namespace wolfenstein {

MatchClient::MatchClient(std::unique_ptr<net::Connection> connection,
						 std::string_view name)
	: connection_(std::move(connection)), name_(name) {}

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
	while (const auto size = connection_->Receive(buffer_)) {
		const auto message = net::Decode(std::span(buffer_).first(*size));
		if (!message) {
			continue;  // not the game's: nothing to do with it
		}
		if (const auto* welcome = std::get_if<net::Welcome>(&*message);
			welcome != nullptr && state_ == State::Joining) {
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
	slot_ = welcome.slot;
	sequence_ = 0;
	history_ = {};
	snapshot_count_ = 0;
	snapshot_next_ = 0;
	latest_tick_ = welcome.tick;
	ticks_since_ = 0;
	state_ = State::Playing;
}

void MatchClient::BeforeTick(World& world, const PlayerCommand& command) {
	if (state_ != State::Playing) {
		return;
	}
	++sequence_;
	history_[sequence_ % kHistory] = {
		.sequence = sequence_, .command = command, .reached = {}};
	// The newest command and the three before it, oldest first
	net::Input input{.count = static_cast<std::uint8_t>(std::min<std::uint32_t>(
						 net::kInputCommands, sequence_))};
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
	history_[sequence_ % kHistory].reached = world.GetPlayer().GetPosition();
	++ticks_since_;
	if (snapshot_count_ == 0) {
		return;
	}
	const double tick = static_cast<double>(latest_tick_) + ticks_since_ -
						static_cast<double>(kInterpolationTicks);
	for (std::size_t slot = 0; slot < Scene::kMaxPlayers; ++slot) {
		Player* player = world.FindPlayer(slot);
		if (slot == slot_ || player == nullptr || !player->IsPuppet()) {
			continue;
		}
		if (const auto state = Interpolate(slot, tick)) {
			player->Follow(Position2D(state->pose, state->theta), state->pitch,
						   state->health, state->alive);
		}
	}
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
		if (snapshot.players[i].slot == slot_) {
			Reconcile(world, snapshot.players[i], snapshot.ack);
		}
	}
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
	// No command of ours applied yet, or too long ago to replay from: where
	// the server has it, looking where it looks
	if (ack == 0 || at.sequence != ack) {
		if (player.GetPose().Distance(state.pose) > kTolerance) {
			++corrections_;
			player.Correct(Position2D(state.pose, player.GetPosition().theta));
		}
		return;
	}
	if (at.reached.pose.Distance(state.pose) <= kTolerance) {
		return;	 // predicted right
	}
	++corrections_;
	player.Correct(Position2D(state.pose, at.reached.theta));
	for (std::uint32_t sequence = ack + 1; sequence <= sequence_; ++sequence) {
		Sent& sent = history_[sequence % kHistory];
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

void MatchClient::Send(const net::Message& message) {
	std::array<std::uint8_t, net::kMaxMessage> buffer{};
	const std::size_t size = net::Encode(message, buffer);
	if (size > 0) {
		connection_->Send(std::span(buffer).first(size));
	}
}

}  // namespace wolfenstein
