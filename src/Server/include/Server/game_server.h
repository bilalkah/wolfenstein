/**
 * @file game_server.h
 * @brief A multiplayer game as the server runs it
 */

#ifndef SERVER_INCLUDE_SERVER_GAME_SERVER_H_
#define SERVER_INCLUDE_SERVER_GAME_SERVER_H_

#include "Core/world.h"
#include "Net/protocol.h"
#include "Server/match_rules.h"
#include "TextureManager/texture_manager.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace wolfenstein {

// A connection, as the transport numbers it
enum class ClientId : std::uint32_t {};

// Where the server's messages go: the transport's connections
class Outbox
{
  public:
	virtual ~Outbox() = default;
	// One message, whole, to `client`
	virtual void Send(ClientId client,
					  std::span<const std::uint8_t> message) = 0;
	// Ends the connection, after what was sent to it
	virtual void Close(ClientId client) = 0;

  protected:
	Outbox() = default;
	Outbox(const Outbox&) = default;
	Outbox& operator=(const Outbox&) = default;
	Outbox(Outbox&&) = default;
	Outbox& operator=(Outbox&&) = default;
};

// One multiplayer game, on one arena, as the server runs it: it lets
// players in (a slot each, while one is free), takes their commands, moves
// the world on a tick at a time with each player's next command, plays the
// match by its rules, and tells every player how the players stand, what
// happened (shots, hits, kills, pickups) and the scores. It knows nothing
// of sockets: the transport hands it each connection's messages and it
// answers through an Outbox, so a test plays it with no network at all.
//
// A player's commands are applied one a tick, in their order, as they
// come: a player whose next command is late goes on doing what it last did
// (holding the trigger, not pressing a key again); one far ahead is caught
// up, so its commands are never long behind.
//
// A shot is judged against the others where the shooter's game showed
// them as it fired (each command says when that was): the server keeps
// where everyone stood for the last kRewind ticks.
//
// A connection is taken only for what a player's game sends, when it sends
// it: a hello first, once, then commands and pings. Anything else ends it:
// bytes that are not a message of the protocol (net::Decode), a message only
// the server sends, commands skipping ahead or going back, more messages
// than a game sends, or no hello in time.
class GameServer : private Hindsight
{
  public:
	static constexpr double kTickSeconds = net::kTickSeconds;
	// A snapshot every this many ticks: 30 a second
	static constexpr std::uint32_t kSnapshotEvery = 2;
	// Connections held at once: the players, and a few more waiting to say
	// hello or to be turned away
	static constexpr std::size_t kMaxConnections = 16;
	// A player's commands waiting at most; past this, it is caught up
	static constexpr std::size_t kQueue = 32;
	static constexpr std::size_t kMaxQueued = 8;
	// How far back a shot may be judged: half a second
	static constexpr std::uint32_t kRewind = 30;
	// A player who comes back under the same name this soon after leaving,
	// in the same match, has its score back (its connection dropped)
	static constexpr double kComebackSeconds = 60.0;
	// A player not heard from this long is gone, if someone says hello
	// under its name (it is coming back on a new connection)
	static constexpr double kStaleSeconds = 5.0;
	// The scores go out when they change, and this often anyway (the
	// clock), each player's round trip with them
	static constexpr std::uint32_t kScoresEvery = 60;
	// A connection says hello this soon after it opens, or it goes
	static constexpr double kHelloSeconds = 10.0;
	// Messages a connection may send, as a budget: a game sends a command a
	// tick and a ping a second, and the budget grows by kMessagesPerTick a
	// tick, up to kMessageBurst (the half minute of messages a stalled
	// network lets through at once). One that spends it all is flooding the
	// server, and goes.
	static constexpr std::uint32_t kMessagesPerTick = 2;
	static constexpr std::uint32_t kMessageBurst = 2000;
	// The longest round trip a player is shown to have, in ms, whatever it
	// says
	static constexpr std::uint16_t kMaxRtt = 9999;

	// Loads the game's content from `asset_dir`, drawing nothing, and starts
	// a match on the first of `arenas` (none: the configuration's) with no
	// one in it yet, played by `settings`; each match after is played on
	// the next, round and round
	static std::expected<std::unique_ptr<GameServer>, std::string> Create(
		const std::string& asset_dir, std::vector<std::string> arenas,
		Outbox& outbox, const MatchSettings& settings = {});
	// Its world's match started on the first of `arenas`; borrows the
	// outbox, which outlives it
	GameServer(std::unique_ptr<TextureManager> textures,
			   std::unique_ptr<World> world, std::vector<std::string> arenas,
			   Outbox& outbox, const MatchSettings& settings = {});
	// Pinned: the world borrows the textures, the level this
	GameServer(const GameServer&) = delete;
	GameServer& operator=(const GameServer&) = delete;
	GameServer(GameServer&&) = delete;
	GameServer& operator=(GameServer&&) = delete;
	~GameServer() override = default;

	// A connection opened: once it says hello it is a player. With every
	// connection taken it is closed at once.
	void Connect(ClientId client);
	// One whole message from `client`; what a player's game would not send
	// then closes the connection
	void Receive(ClientId client, std::span<const std::uint8_t> message);
	// The connection is gone: its player leaves
	void Disconnect(ClientId client);
	// Moves the game on one tick, and every kSnapshotEvery ticks tells every
	// player how things stand
	void Tick();

	std::uint32_t CurrentTick() const { return tick_; }
	std::size_t PlayerCount() const;
	World& GetWorld() { return *world_; }
	const MatchRules& Rules() const { return rules_; }

  private:
	// A command waiting, and the server tick the player's game showed the
	// others at as it was made
	struct Queued
	{
		net::NumberedCommand numbered;
		std::uint32_t seen = 0;
	};
	struct Client
	{
		bool open = false;
		bool closing = false;  // told to go: nothing more it sends is read
		ClientId id{};
		std::optional<std::size_t> slot{};	// a player once it said hello
		net::PlayerName name{};
		// Commands come in numbered: the last one applied, the newest taken
		// in, and those still to apply, oldest first (a ring from `head`)
		std::uint32_t applied = 0;
		std::uint32_t received = 0;
		std::array<Queued, kQueue> queue{};
		std::size_t head = 0;
		std::size_t queued = 0;
		PlayerCommand last{};
		std::uint32_t seen = 0;	   // the last command's
		std::uint32_t heard = 0;   // the tick its last message came
		std::uint16_t rtt = 0;	   // its round trip, in ms, as it last said
		std::uint32_t opened = 0;  // the tick it connected at
		std::uint32_t budget = kMessageBurst;  // messages it may yet send
	};

	// A player gone, whose score waits a while for it to come back
	struct Departed
	{
		net::PlayerName name{};
		Standing standing{};
		std::size_t match = 0;	 // MatchRules::MatchNumber
		std::uint32_t tick = 0;	 // when it left
	};

	// Where `target` stood when `shooter`'s game showed it (Hindsight)
	std::optional<vector2d> Seen(std::size_t shooter,
								 std::size_t target) const override;
	// Where everyone stands after this tick, for shots judged later
	void Remember();
	// The next match on the next arena: everyone in it, and welcomed to it
	void NextArena();
	// A new level's: its events recorded, its shots judged in hindsight
	void Watch(Scene& scene);
	void SendEvents(std::span<const MatchEvent> events);
	void SendScores();

	Client* Find(ClientId id);
	// Ends the connection: the transport is told to close it, and nothing
	// more from it is read
	void Close(Client& client);
	// Takes `message` from `client`; false if it is not what a player's game
	// sends then
	bool Take(Client& client, const net::Message& message);
	void Hello(Client& client, const net::Hello& hello);
	bool Input(Client& client, const net::Input& input);
	// The next command for `client`'s player this tick
	PlayerCommand NextCommand(Client& client);
	void SendSnapshots();
	void Send(ClientId client, const net::Message& message);

	void Broadcast(const net::Message& message);

	std::unique_ptr<TextureManager> textures_;
	std::unique_ptr<World> world_;
	std::vector<std::string> arenas_;
	std::size_t arena_ = 0;	 // the one played
	net::LevelName level_;
	Outbox& outbox_;
	MatchRules rules_;
	std::array<Client, kMaxConnections> clients_{};
	std::uint32_t tick_ = 0;
	// Where each slot's player stood after each of the last kRewind ticks
	// (nothing: not there, or down), by tick
	using Places = std::array<std::optional<vector2d>, Scene::kMaxPlayers>;
	std::array<Places, kRewind> past_{};
	// Each slot's: the tick its game showed the others at, for the command
	// being applied; and whether it asks to come back (firing while down)
	std::array<std::uint32_t, Scene::kMaxPlayers> seen_{};
	std::array<bool, Scene::kMaxPlayers> wants_back_{};
	bool scores_changed_ = true;
	// The last players gone, the oldest first to make way
	std::array<Departed, Scene::kMaxPlayers> departed_{};
	std::size_t next_departed_ = 0;
	std::array<std::uint8_t, net::kMaxMessage> buffer_{};
};

}  // namespace wolfenstein

#endif	// SERVER_INCLUDE_SERVER_GAME_SERVER_H_
