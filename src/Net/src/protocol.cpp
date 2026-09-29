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

// A player in a snapshot is alive: the top bit of its slot's byte
constexpr std::uint8_t kAliveBit = 0x80;

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
		// The slot, and whether alive in the top bit
		out.U8(static_cast<std::uint8_t>(player.slot |
										 (player.alive ? kAliveBit : 0U)));
		out.U16(PackPosition(player.pose.x));
		out.U16(PackPosition(player.pose.y));
		out.U16(PackAngle(player.theta));
		out.I16(PackPitch(player.pitch));
		out.U8(player.health);
		out.U8(player.weapon);
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
				player.slot = slot & static_cast<std::uint8_t>(~kAliveBit);
				player.alive = (slot & kAliveBit) != 0;
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
			return snapshot;
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
		type > std::to_underlying(MessageType::Snapshot)) {
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
