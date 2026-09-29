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
inline constexpr std::uint16_t kProtocolVersion = 2;
// A tick, on the server and in a player's game alike: a command each
inline constexpr double kTickSeconds = 1.0 / 60.0;
// The most bytes a message takes
inline constexpr std::size_t kMaxMessage = 512;
// Players a game holds at most
inline constexpr std::size_t kMaxPlayers = 8;
// Commands each input message carries: the newest and the three before it,
// so a message lost costs no command
inline constexpr std::size_t kInputCommands = 4;
// Weapons a player carries at most, and pickups a level's snapshot tells of
inline constexpr std::size_t kMaxWeapons = 8;
inline constexpr std::size_t kMaxPickups = 64;
// Events one message carries at most: a busy tick's take several
inline constexpr std::size_t kMaxEvents = 40;

enum class MessageType : std::uint8_t {
	Hello = 1,	// player to server, first
	Welcome,	// server to player: in, with a slot
	Reject,		// server to player: not in, and why
	Input,		// player to server: its latest commands
	Snapshot,	// server to player: the players as they stand
	Events,		// server to player: what happened in a tick
	Scores,		// server to player: how the match stands
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
// A player's latest commands, oldest first, their numbers one after another;
// `seen` is the server tick its game showed the others at as it made the
// newest (a shot is judged against them as they were then)
struct Input
{
	std::uint8_t count = 0;
	std::uint32_t seen = 0;
	std::array<NumberedCommand, kInputCommands> commands{};

	friend bool operator==(const Input&, const Input&) = default;
};

// A player as the server has them
struct PlayerState
{
	std::uint8_t slot = 0;
	bool alive = true;
	bool shielded = false;	// just come in, and not to be hurt yet
	vector2d pose{};
	double theta = 0.0;
	double pitch = 0.0;
	std::uint8_t health = 0;
	std::uint8_t weapon = 0;  // the one in hand, by index
};
// A weapon's rounds: in the magazine, and in reserve
struct Rounds
{
	std::uint8_t ammo = 0;
	std::uint16_t reserve = 0;

	friend bool operator==(const Rounds&, const Rounds&) = default;
};
// What the receiving player carries, which the others need not know: its
// weapons, a bit each, and each one's rounds
struct Inventory
{
	std::uint8_t owned = 0;
	std::uint8_t count = 0;	 // weapons, the rounds of each below
	std::array<Rounds, kMaxWeapons> rounds{};

	friend bool operator==(const Inventory&, const Inventory&) = default;
};
// How the players stand after the server's tick `tick`; `ack` is the last
// of the receiving player's commands applied by then, and `own` what it
// carries; `taken`, a bit per pickup of the level's first `pickups`, those
// not lying there
struct Snapshot
{
	std::uint32_t tick = 0;
	std::uint32_t ack = 0;
	std::uint8_t count = 0;
	std::array<PlayerState, kMaxPlayers> players{};
	Inventory own{};
	std::uint8_t pickups = 0;
	std::uint64_t taken = 0;
};

// Something that happened in the game, as the players are told
enum class EventType : std::uint8_t {
	Shot,	 // `slot` fired `weapon` from `pose`
	Launch,	 // `slot` launched `weapon`'s projectile from `pose`, `theta`
	Hurt,	 // `slot` hurt `other` (at `pose`) with `weapon`
	Kill,	 // `slot` killed `other` with `weapon` (itself: its own blast)
	Pickup,	 // `slot` took pickup `other` (by index)
};
struct Event
{
	EventType type = EventType::Shot;
	std::uint8_t slot = 0;
	std::uint8_t other = 0;
	std::uint8_t weapon = 0;
	vector2d pose{};
	double theta = 0.0;
};
// What happened in the server's tick `tick`, in order
struct Events
{
	std::uint32_t tick = 0;
	std::uint8_t count = 0;
	std::array<Event, kMaxEvents> events{};
};

enum class MatchMode : std::uint8_t { Deathmatch, GunRace };
// Playing, or the match over and its result showing until the next
enum class MatchPhase : std::uint8_t { Playing, Intermission };
// A player's standing in the match
struct Score
{
	std::uint8_t slot = 0;
	PlayerName name{};
	std::int16_t frags = 0;
	std::uint16_t deaths = 0;
	std::uint8_t step = 0;	// a gun race's: how far up its ladder

	friend bool operator==(const Score&, const Score&) = default;
};
// How the match stands: sent as it changes, and every second for the clock
inline constexpr std::uint8_t kNoWinner = 0xFF;
struct Scores
{
	MatchMode mode = MatchMode::Deathmatch;
	MatchPhase phase = MatchPhase::Playing;
	std::uint8_t frag_limit = 0;
	std::uint16_t seconds_left = 0;	 // of the match, or of the intermission
	std::uint8_t winner = kNoWinner;
	std::uint8_t count = 0;
	std::array<Score, kMaxPlayers> players{};

	friend bool operator==(const Scores&, const Scores&) = default;
};

using Message =
	std::variant<Hello, Welcome, Reject, Input, Snapshot, Events, Scores>;

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
