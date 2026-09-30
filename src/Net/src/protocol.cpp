#include "Net/protocol.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <type_traits>
#include <utility>

namespace wolfenstein::net {

namespace {

constexpr double kTurn = 2.0 * std::numbers::pi;
// Positions in 256ths of a cell: levels up to 256 cells across
constexpr double kPositionScale = 256.0;
// Pitch as a share of the furthest a view tips, a little past a player's
constexpr double kPitchRange = 0.5;

std::uint16_t PackPosition(double coordinate) {
	return static_cast<std::uint16_t>(
		std::clamp(std::lround(coordinate * kPositionScale), 0L, 0xFFFFL));
}
double UnpackPosition(std::uint16_t packed) {
	return packed / kPositionScale;
}
std::uint16_t PackAngle(double theta) {
	const double turns = theta / kTurn - std::floor(theta / kTurn);
	return static_cast<std::uint16_t>(std::lround(turns * 65536.0) & 0xFFFF);
}
double UnpackAngle(std::uint16_t packed) {
	return packed / 65536.0 * kTurn;
}
std::int16_t PackPitch(double pitch) {
	return static_cast<std::int16_t>(
		std::lround(std::clamp(pitch / kPitchRange, -1.0, 1.0) * 32767.0));
}
double UnpackPitch(std::int16_t packed) {
	return packed / 32767.0 * kPitchRange;
}

// A player in a snapshot is alive, and shielded: the top bits of its slot's
// byte
constexpr std::uint8_t kAliveBit = 0x80;
constexpr std::uint8_t kShieldedBit = 0x40;
constexpr std::uint8_t kSlotBits = 0x3F;

// A command's buttons, a bit each
enum Button : std::uint8_t {
	kFire = 1U << 0U,
	kReload = 1U << 1U,
	kUse = 1U << 2U,
	kHasView = 1U << 3U,
};

void WriteCommand(ByteWriter& out, const PlayerCommand& command) {
	out.I8(command.forward);
	out.I8(command.strafe);
	out.I8(command.turn);
	out.U8(static_cast<std::uint8_t>(
		(command.fire ? kFire : 0U) | (command.reload ? kReload : 0U) |
		(command.use ? kUse : 0U) | (command.has_view ? kHasView : 0U)));
	out.I8(command.weapon);
	out.I8(command.cycle);
	out.U16(PackAngle(command.view_theta));
	out.I16(PackPitch(command.view_pitch));
}

PlayerCommand ReadCommand(ByteReader& in) {
	PlayerCommand command;
	command.forward = std::clamp<std::int8_t>(in.I8(), -1, 1);
	command.strafe = std::clamp<std::int8_t>(in.I8(), -1, 1);
	command.turn = std::clamp<std::int8_t>(in.I8(), -1, 1);
	const std::uint8_t buttons = in.U8();
	command.fire = (buttons & kFire) != 0;
	command.reload = (buttons & kReload) != 0;
	command.use = (buttons & kUse) != 0;
	command.has_view = (buttons & kHasView) != 0;
	command.weapon = in.I8();
	command.cycle = std::clamp<std::int8_t>(in.I8(), -1, 1);
	command.view_theta = UnpackAngle(in.U16());
	command.view_pitch = UnpackPitch(in.I16());
	return command;
}

void Write(ByteWriter& out, const Hello& hello) {
	out.U16(hello.version);
	hello.name.Write(out);
}
void Write(ByteWriter& out, const Welcome& welcome) {
	out.U16(welcome.version);
	out.U8(welcome.slot);
	out.U32(welcome.tick);
	welcome.level.Write(out);
}
void Write(ByteWriter& out, const Reject& reject) {
	out.U8(static_cast<std::uint8_t>(reject.reason));
}
void Write(ByteWriter& out, const Input& input) {
	const std::uint8_t count =
		std::min<std::uint8_t>(input.count, kInputCommands);
	out.U8(count);
	out.U32(count > 0 ? input.commands[0].sequence : 0);
	out.U32(input.seen);
	for (std::size_t i = 0; i < count; ++i) {
		WriteCommand(out, input.commands[i].command);
	}
}
void Write(ByteWriter& out, const Snapshot& snapshot) {
	const std::uint8_t count =
		std::min<std::uint8_t>(snapshot.count, kMaxPlayers);
	out.U32(snapshot.tick);
	out.U32(snapshot.ack);
	out.U8(count);
	for (std::size_t i = 0; i < count; ++i) {
		const PlayerState& player = snapshot.players[i];
		// The slot, and whether alive and shielded in the top bits
		out.U8(static_cast<std::uint8_t>(
			player.slot | (player.alive ? kAliveBit : 0U) |
			(player.shielded ? kShieldedBit : 0U)));
		out.U16(PackPosition(player.pose.x));
		out.U16(PackPosition(player.pose.y));
		out.U16(PackAngle(player.theta));
		out.I16(PackPitch(player.pitch));
		out.U8(player.health);
		out.U8(player.weapon);
	}
	const Inventory& own = snapshot.own;
	const std::uint8_t weapons = std::min<std::uint8_t>(own.count, kMaxWeapons);
	out.U8(own.owned);
	out.U8(weapons);
	for (std::size_t i = 0; i < weapons; ++i) {
		out.U8(own.rounds[i].ammo);
		out.U16(own.rounds[i].reserve);
	}
	// A bit per pickup, eight to a byte
	const std::uint8_t pickups =
		std::min<std::uint8_t>(snapshot.pickups, kMaxPickups);
	out.U8(pickups);
	for (std::size_t byte = 0; byte * 8 < pickups; ++byte) {
		out.U8(static_cast<std::uint8_t>(snapshot.taken >> (byte * 8)));
	}
}
void Write(ByteWriter& out, const Events& events) {
	const std::uint8_t count = std::min<std::uint8_t>(events.count, kMaxEvents);
	out.U32(events.tick);
	out.U8(count);
	for (std::size_t i = 0; i < count; ++i) {
		const Event& event = events.events[i];
		out.U8(std::to_underlying(event.type));
		out.U8(event.slot);
		out.U8(event.other);
		out.U8(event.weapon);
		out.U16(PackPosition(event.pose.x));
		out.U16(PackPosition(event.pose.y));
		out.U16(PackAngle(event.theta));
	}
}
void Write(ByteWriter& out, const Scores& scores) {
	const std::uint8_t count =
		std::min<std::uint8_t>(scores.count, kMaxPlayers);
	out.U8(std::to_underlying(scores.mode));
	out.U8(std::to_underlying(scores.phase));
	out.U8(scores.frag_limit);
	out.U16(scores.seconds_left);
	out.U8(scores.winner);
	out.U8(count);
	for (std::size_t i = 0; i < count; ++i) {
		const Score& score = scores.players[i];
		out.U8(score.slot);
		score.name.Write(out);
		out.I16(score.frags);
		out.U16(score.deaths);
		out.U8(score.step);
	}
}

void Write(ByteWriter& out, const Ping& ping) {
	out.U32(ping.stamp);
	out.U16(ping.rtt);
}
void Write(ByteWriter& out, const Pong& pong) {
	out.U32(pong.stamp);
}
void Write(ByteWriter& out, const Pings& pings) {
	const std::uint8_t count = std::min<std::uint8_t>(pings.count, kMaxPlayers);
	out.U8(count);
	for (std::size_t i = 0; i < count; ++i) {
		out.U8(pings.players[i].slot);
		out.U16(pings.players[i].rtt);
	}
}

std::optional<Message> ReadBody(MessageType type, ByteReader& in) {
	switch (type) {
		case MessageType::Hello: {
			Hello hello;
			hello.version = in.U16();
			hello.name = PlayerName::Read(in);
			return hello;
		}
		case MessageType::Welcome: {
			Welcome welcome;
			welcome.version = in.U16();
			welcome.slot = in.U8();
			welcome.tick = in.U32();
			welcome.level = LevelName::Read(in);
			if (welcome.slot >= kMaxPlayers) {
				in.Fail();
			}
			return welcome;
		}
		case MessageType::Reject: {
			const std::uint8_t reason = in.U8();
			if (reason < std::to_underlying(RejectReason::Version) ||
				reason > std::to_underlying(RejectReason::Full)) {
				in.Fail();
			}
			return Reject{.reason = static_cast<RejectReason>(reason)};
		}
		case MessageType::Input: {
			Input input;
			input.count = in.U8();
			if (input.count > kInputCommands) {
				in.Fail();
				return input;
			}
			const std::uint32_t first = in.U32();
			input.seen = in.U32();
			for (std::size_t i = 0; i < input.count; ++i) {
				input.commands[i] = {
					.sequence = first + static_cast<std::uint32_t>(i),
					.command = ReadCommand(in)};
			}
			return input;
		}
		case MessageType::Snapshot: {
			Snapshot snapshot;
			snapshot.tick = in.U32();
			snapshot.ack = in.U32();
			snapshot.count = in.U8();
			if (snapshot.count > kMaxPlayers) {
				in.Fail();
				return snapshot;
			}
			for (std::size_t i = 0; i < snapshot.count; ++i) {
				PlayerState& player = snapshot.players[i];
				const std::uint8_t slot = in.U8();
				player.slot = slot & kSlotBits;
				player.alive = (slot & kAliveBit) != 0;
				player.shielded = (slot & kShieldedBit) != 0;
				player.pose.x = UnpackPosition(in.U16());
				player.pose.y = UnpackPosition(in.U16());
				player.theta = UnpackAngle(in.U16());
				player.pitch = UnpackPitch(in.I16());
				player.health = in.U8();
				player.weapon = in.U8();
				if (player.slot >= kMaxPlayers) {
					in.Fail();
				}
			}
			Inventory& own = snapshot.own;
			own.owned = in.U8();
			own.count = in.U8();
			if (own.count > kMaxWeapons) {
				in.Fail();
				return snapshot;
			}
			for (std::size_t i = 0; i < own.count; ++i) {
				own.rounds[i].ammo = in.U8();
				own.rounds[i].reserve = in.U16();
			}
			snapshot.pickups = in.U8();
			if (snapshot.pickups > kMaxPickups) {
				in.Fail();
				return snapshot;
			}
			for (std::size_t byte = 0; byte * 8 < snapshot.pickups; ++byte) {
				snapshot.taken |= std::uint64_t{in.U8()} << (byte * 8);
			}
			return snapshot;
		}
		case MessageType::Events: {
			Events events;
			events.tick = in.U32();
			events.count = in.U8();
			if (events.count > kMaxEvents) {
				in.Fail();
				return events;
			}
			for (std::size_t i = 0; i < events.count; ++i) {
				Event& event = events.events[i];
				const std::uint8_t kind = in.U8();
				event.type = static_cast<EventType>(kind);
				event.slot = in.U8();
				event.other = in.U8();
				event.weapon = in.U8();
				event.pose.x = UnpackPosition(in.U16());
				event.pose.y = UnpackPosition(in.U16());
				event.theta = UnpackAngle(in.U16());
				const bool other_is_slot = event.type != EventType::Pickup;
				if (kind > std::to_underlying(EventType::Pickup) ||
					event.slot >= kMaxPlayers ||
					(other_is_slot && event.other >= kMaxPlayers) ||
					event.weapon >= kMaxWeapons) {
					in.Fail();
				}
			}
			return events;
		}
		case MessageType::Scores: {
			Scores scores;
			const std::uint8_t mode = in.U8();
			const std::uint8_t phase = in.U8();
			scores.mode = static_cast<MatchMode>(mode);
			scores.phase = static_cast<MatchPhase>(phase);
			scores.frag_limit = in.U8();
			scores.seconds_left = in.U16();
			scores.winner = in.U8();
			scores.count = in.U8();
			if (mode > std::to_underlying(MatchMode::GunRace) ||
				phase > std::to_underlying(MatchPhase::Intermission) ||
				(scores.winner != kNoWinner && scores.winner >= kMaxPlayers) ||
				scores.count > kMaxPlayers) {
				in.Fail();
				return scores;
			}
			for (std::size_t i = 0; i < scores.count; ++i) {
				Score& score = scores.players[i];
				score.slot = in.U8();
				score.name = PlayerName::Read(in);
				score.frags = in.I16();
				score.deaths = in.U16();
				score.step = in.U8();
				if (score.slot >= kMaxPlayers) {
					in.Fail();
				}
			}
			return scores;
		}
		case MessageType::Ping: {
			Ping ping;
			ping.stamp = in.U32();
			ping.rtt = in.U16();
			return ping;
		}
		case MessageType::Pong:
			return Pong{.stamp = in.U32()};
		case MessageType::Pings: {
			Pings pings;
			pings.count = in.U8();
			if (pings.count > kMaxPlayers) {
				in.Fail();
				return pings;
			}
			for (std::size_t i = 0; i < pings.count; ++i) {
				pings.players[i].slot = in.U8();
				pings.players[i].rtt = in.U16();
				if (pings.players[i].slot >= kMaxPlayers) {
					in.Fail();
				}
			}
			return pings;
		}
	}
	return std::nullopt;
}

}  // namespace

std::size_t Encode(const Message& message, std::span<std::uint8_t> out) {
	ByteWriter writer(out);
	std::visit(
		[&writer](const auto& body) {
			using Body = std::decay_t<decltype(body)>;
			MessageType type = MessageType::Hello;
			if constexpr (std::is_same_v<Body, Welcome>) {
				type = MessageType::Welcome;
			}
			else if constexpr (std::is_same_v<Body, Reject>) {
				type = MessageType::Reject;
			}
			else if constexpr (std::is_same_v<Body, Input>) {
				type = MessageType::Input;
			}
			else if constexpr (std::is_same_v<Body, Snapshot>) {
				type = MessageType::Snapshot;
			}
			else if constexpr (std::is_same_v<Body, Events>) {
				type = MessageType::Events;
			}
			else if constexpr (std::is_same_v<Body, Scores>) {
				type = MessageType::Scores;
			}
			else if constexpr (std::is_same_v<Body, Ping>) {
				type = MessageType::Ping;
			}
			else if constexpr (std::is_same_v<Body, Pong>) {
				type = MessageType::Pong;
			}
			else if constexpr (std::is_same_v<Body, Pings>) {
				type = MessageType::Pings;
			}
			writer.U8(std::to_underlying(type));
			Write(writer, body);
		},
		message);
	return writer.Ok() ? writer.Size() : 0;
}

std::optional<Message> Decode(std::span<const std::uint8_t> data) {
	ByteReader reader(data);
	const std::uint8_t type = reader.U8();
	if (!reader.Ok() || type < std::to_underlying(MessageType::Hello) ||
		type > std::to_underlying(MessageType::Pings)) {
		return std::nullopt;
	}
	auto message = ReadBody(static_cast<MessageType>(type), reader);
	if (!message || !reader.Ok() || reader.Remaining() != 0) {
		return std::nullopt;
	}
	return message;
}

double QuantisePosition(double coordinate) {
	return UnpackPosition(PackPosition(coordinate));
}

double QuantiseAngle(double theta) {
	return UnpackAngle(PackAngle(theta));
}

double QuantisePitch(double pitch) {
	return UnpackPitch(PackPitch(pitch));
}

}  // namespace wolfenstein::net
