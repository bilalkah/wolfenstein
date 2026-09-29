/**
 * @file match_client.h
 * @brief A player's side of a multiplayer game
 */

#ifndef CLIENT_INCLUDE_CLIENT_MATCH_CLIENT_H_
#define CLIENT_INCLUDE_CLIENT_MATCH_CLIENT_H_

#include "Characters/player_command.h"
#include "Core/world.h"
#include "Net/connection.h"
#include "Net/protocol.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace wolfenstein {

// A player's side of a multiplayer game. It says hello and, welcomed,
// starts the match in the world (the level the server names, the local
// player in the slot it gives). Each tick it sends the local player's
// command, numbered, with the three before it, and remembers where the
// command took the player: the game moves its own player at once, and
// when the server's snapshot says it stood elsewhere after a command, the
// player is put there and the commands since are played again (movement
// only). The other players are puppets, placed where the snapshots had them
// kInterpolationTicks ago, between two, so they move smoothly however the
// snapshots come.
class MatchClient
{
  public:
	enum class State : std::uint8_t {
		Connecting,	 // waiting for the connection to open
		Joining,	 // hello said, waiting for the server's answer
		Playing,
		Rejected,  // turned away (Reason says why)
		Closed,	   // the connection is gone
	};
	// The others are shown this far behind the newest snapshot, in ticks:
	// two snapshots' worth, and one to spare
	static constexpr std::uint32_t kInterpolationTicks = 6;
	// Commands remembered for replaying, and snapshots for placing others
	static constexpr std::size_t kHistory = 128;
	static constexpr std::size_t kSnapshots = 32;
	// A prediction this far from the server's is corrected
	static constexpr double kTolerance = 0.01;

	// Plays over `connection`, as `name`
	MatchClient(std::unique_ptr<net::Connection> connection,
				std::string_view name);

	// Reads what has come in: the welcome (starting the match in `world`),
	// or the snapshots (correcting the local player, bringing players in
	// and out). Every frame, before the ticks.
	void Poll(World& world);
	// One tick, before the level's update: sends `command`, numbered, and
	// hands it to the local player
	void BeforeTick(World& world, const PlayerCommand& command);
	// After the level's update: remembers where the command took the local
	// player, and places the others as the snapshots had them
	void AfterTick(World& world);

	State GetState() const { return state_; }
	net::RejectReason Reason() const { return reason_; }
	// How many times the local player was put where the server had it
	std::size_t Corrections() const { return corrections_; }

  private:
	struct Sent
	{
		std::uint32_t sequence = 0;
		PlayerCommand command;
		Position2D reached;	 // where the local player was after it
	};

	void OnWelcome(World& world, const net::Welcome& welcome);
	void OnSnapshot(World& world, const net::Snapshot& snapshot);
	// The local player where the server had it after command `ack`, and the
	// commands since played again
	void Reconcile(World& world, const net::PlayerState& state,
				   std::uint32_t ack);
	// The players the snapshot has come in, as puppets; the others go
	void Attend(World& world, const net::Snapshot& snapshot);
	// Where the snapshots had `slot` at server tick `tick` (fractional),
	// between the two round it; nothing if none has it
	std::optional<net::PlayerState> Interpolate(std::size_t slot,
												double tick) const;
	void Send(const net::Message& message);

	std::unique_ptr<net::Connection> connection_;
	net::PlayerName name_;
	State state_ = State::Connecting;
	net::RejectReason reason_ = net::RejectReason::Full;
	std::optional<std::size_t> slot_;
	std::uint32_t sequence_ = 0;  // the last command sent
	std::array<Sent, kHistory> history_{};
	std::array<net::Snapshot, kSnapshots> snapshots_{};
	std::size_t snapshot_count_ = 0;  // held, the newest at snapshot_next_ - 1
	std::size_t snapshot_next_ = 0;
	std::uint32_t latest_tick_ = 0;	 // the newest snapshot's
	std::uint32_t ticks_since_ = 0;	 // local ticks since it came
	std::size_t corrections_ = 0;
	std::array<std::uint8_t, net::kMaxMessage> buffer_{};
};

}  // namespace wolfenstein

#endif	// CLIENT_INCLUDE_CLIENT_MATCH_CLIENT_H_
