/**
 * @file protocol.h
 * @brief What the game's server and its players say to each other
 */

#ifndef NET_INCLUDE_NET_PROTOCOL_H_
#define NET_INCLUDE_NET_PROTOCOL_H_

#include "Characters/player_command.h"
#include "Math/vector.h"
#include "Net/bytes.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

namespace wolfenstein::net {

// The server decides what happens; its players send what they do (their
// commands) and it sends back how the level stands (snapshots). Each
// message is one WebSocket binary frame: a type byte, then its fields,
// little-endian, positions and angles packed into whole numbers.

// Both sides must speak the same: a player of another version is turned
// away
inline constexpr std::uint16_t kProtocolVersion = 1;
// A tick, on the server and in a player's game alike: a command each
inline constexpr double kTickSeconds = 1.0 / 60.0;
// The most bytes a message takes
inline constexpr std::size_t kMaxMessage = 512;
// Players a game holds at most
inline constexpr std::size_t kMaxPlayers = 8;
// Commands each input message carries: the newest and the three before it,
// so a message lost costs no command
inline constexpr std::size_t kInputCommands = 4;

enum class MessageType : std::uint8_t {
	Hello = 1,	// player to server, first
	Welcome,	// server to player: in, with a slot
	Reject,		// server to player: not in, and why
	Input,		// player to server: its latest commands
	Snapshot,	// server to player: the players as they stand
};

using PlayerName = FixedString<16>;
using LevelName = FixedString<32>;

struct Hello
{
	std::uint16_t version = kProtocolVersion;
	PlayerName name{};

	friend bool operator==(const Hello&, const Hello&) = default;
};

struct Welcome
{
	std::uint16_t version = kProtocolVersion;
	std::uint8_t slot = 0;	 // the player's, 0 to kMaxPlayers - 1
	std::uint32_t tick = 0;	 // the server's, as it is now
	LevelName level{};		 // the level file to load ("arena.json")

	friend bool operator==(const Welcome&, const Welcome&) = default;
};

enum class RejectReason : std::uint8_t {
	Version = 1,  // it speaks another version of the protocol
	Full,		  // every slot is taken
};
struct Reject
{
	RejectReason reason = RejectReason::Full;

	friend bool operator==(const Reject&, const Reject&) = default;
};

// A player's command for one tick, numbered from 1 in the order the player
// made them. The view goes as set outright (has_view): mouse motion itself
// (look, look_up) is not sent.
struct NumberedCommand
{
	std::uint32_t sequence = 0;
	PlayerCommand command;

	friend bool operator==(const NumberedCommand&,
						   const NumberedCommand&) = default;
};
// A player's latest commands, oldest first, their numbers one after another
struct Input
{
	std::uint8_t count = 0;
	std::array<NumberedCommand, kInputCommands> commands{};

	friend bool operator==(const Input&, const Input&) = default;
};

// A player as the server has them
struct PlayerState
{
	std::uint8_t slot = 0;
	bool alive = true;
	vector2d pose{};
	double theta = 0.0;
	double pitch = 0.0;
	std::uint8_t health = 0;
	std::uint8_t weapon = 0;  // the one in hand, by index
};
// How the players stand after the server's tick `tick`; `ack` is the last
// of the receiving player's commands applied by then
struct Snapshot
{
	std::uint32_t tick = 0;
	std::uint32_t ack = 0;
	std::uint8_t count = 0;
	std::array<PlayerState, kMaxPlayers> players{};
};

using Message = std::variant<Hello, Welcome, Reject, Input, Snapshot>;

// Writes `message` to `out`; its length, or 0 if it does not fit
std::size_t Encode(const Message& message, std::span<std::uint8_t> out);
// Reads a message; nothing if it is malformed, of an unknown type or has
// bytes left over
std::optional<Message> Decode(std::span<const std::uint8_t> data);

// How positions and angles are packed, and so how close what a player is
// sent comes to the server's own: a 256th of a cell, a 65536th of a turn
double QuantisePosition(double coordinate);
double QuantiseAngle(double theta);
double QuantisePitch(double pitch);

}  // namespace wolfenstein::net

#endif	// NET_INCLUDE_NET_PROTOCOL_H_
