// A multiplayer game as the server runs it, played with no network: a
// player says hello and gets a slot and the level (or is turned away: another
// version, no slot free); its commands move its player, one a tick, each
// once, and a late one is held, not pressed again; every other tick every
// player hears how the players stand; a player leaving frees its slot. What
// a game would not send, no hello in time, or a flood of messages ends the
// connection. A tick allocates nothing.

#include "Server/game_server.h"
#include "Profiler/profiler.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <numbers>
#include <ranges>
#include <string_view>
#include <variant>
#include <vector>

namespace karakale {
namespace {

// What the server sent, and to whom, and whom it closed
class RecordingOutbox : public Outbox
{
  public:
	struct Sent
	{
		ClientId client;
		net::Message message;
	};
	void Send(ClientId client, std::span<const std::uint8_t> message) override {
		++sent_count;
		if (!recording) {
			return;
		}
		const auto decoded = net::Decode(message);
		if (!decoded) {
			ADD_FAILURE() << "the server sends what it can read";
			return;
		}
		sent.push_back({client, *decoded});
	}
	void Close(ClientId client) override { closed.push_back(client); }

	// The last message of type T sent to `client`; a failure if none was
	template <typename T>
	T Last(ClientId client) const {
		for (const Sent& s : std::views::reverse(sent)) {
			if (s.client == client && std::holds_alternative<T>(s.message)) {
				return std::get<T>(s.message);
			}
		}
		ADD_FAILURE() << "nothing of the kind was sent";
		return T{};
	}
	template <typename T>
	std::size_t Count(ClientId client) const {
		return static_cast<std::size_t>(
			std::ranges::count_if(sent, [&](const Sent& s) {
				return s.client == client &&
					   std::holds_alternative<T>(s.message);
			}));
	}

	bool recording = true;
	std::size_t sent_count = 0;
	std::vector<Sent> sent;
	std::vector<ClientId> closed;
};

class GameServerTest : public ::testing::Test
{
  protected:
	GameServerTest() { Play({}); }

	// A server afresh, playing by `settings`
	void Play(const MatchSettings& settings) {
		server_.reset();
		auto created = GameServer::Create(RESOURCE_DIR, {"bazaar.json"},
										  outbox_, settings);
		EXPECT_TRUE(created) << (created ? "" : created.error());
		if (created) {
			server_ = std::move(*created);
		}
	}
	Scene& Level() { return server_->GetWorld().CurrentLevel(); }
	Player& Mutable(std::size_t slot) {
		return *server_->GetWorld().FindPlayer(slot);
	}
	// `killer` kills `victim` outright, as a shot would
	void Kill(std::size_t killer, std::size_t victim) {
		Mutable(victim).Protect(0.0);
		Level().HurtPlayer(Mutable(victim), 1000.0, killer, 0);
	}

	void Message(ClientId client, const net::Message& message) {
		std::array<std::uint8_t, net::kMaxMessage> buffer{};
		const std::size_t size = net::Encode(message, buffer);
		ASSERT_GT(size, 0u);
		server_->Receive(client, std::span(buffer).first(size));
	}
	// A connection that says hello (as `name`); its slot
	std::size_t Join(ClientId client, std::string_view name = "p") {
		server_->Connect(client);
		Message(client, net::Hello{.name = net::PlayerName(name)});
		return outbox_.Last<net::Welcome>(client).slot;
	}
	// Commands numbered from `first`, forward, facing `theta`
	void Walk(ClientId client, std::uint32_t first, std::uint8_t count,
			  double theta) {
		net::Input input{.count = count};
		for (std::uint8_t i = 0; i < count; ++i) {
			input.commands[i] = {
				.sequence = first + i,
				.command = {
					.forward = 1, .has_view = true, .view_theta = theta}};
		}
		Message(client, input);
	}
	bool Closed(ClientId client) const {
		return std::ranges::find(outbox_.closed, client) !=
			   outbox_.closed.end();
	}
	void Ticks(int count) {
		for (int i = 0; i < count; ++i) {
			server_->Tick();
		}
	}
	const Player& PlayerIn(std::size_t slot) {
		return *server_->GetWorld().FindPlayer(slot);
	}

	RecordingOutbox outbox_;
	std::unique_ptr<GameServer> server_;
};

TEST_F(GameServerTest, AHelloGetsASlotAndTheLevel) {
	EXPECT_EQ(Join(ClientId{11}), 0u);
	EXPECT_EQ(Join(ClientId{12}), 1u);
	const auto welcome = outbox_.Last<net::Welcome>(ClientId{12});
	EXPECT_EQ(welcome.level.View(), "bazaar.json");
	EXPECT_EQ(welcome.version, net::kProtocolVersion);
	EXPECT_EQ(server_->PlayerCount(), 2u);
	// In the level at one of its spawn points, the one furthest from the
	// other player
	const auto spawns = server_->GetWorld().Spawns();
	const vector2d at = PlayerIn(1).GetPose();
	EXPECT_TRUE(std::ranges::any_of(spawns, [&](const Position2D& spawn) {
		return spawn.pose.Distance(at) < 1e-9;
	}));
	for (const Position2D& spawn : spawns) {
		EXPECT_LE(spawn.pose.Distance(PlayerIn(0).GetPose()),
				  at.Distance(PlayerIn(0).GetPose()) + 1e-9);
	}
}

TEST_F(GameServerTest, AnotherVersionIsTurnedAway) {
	server_->Connect(ClientId{5});
	Message(ClientId{5}, net::Hello{.version = net::kProtocolVersion + 1,
									.name = net::PlayerName("p")});
	EXPECT_EQ(outbox_.Last<net::Reject>(ClientId{5}).reason,
			  net::RejectReason::Version);
	EXPECT_EQ(outbox_.closed, std::vector<ClientId>{ClientId{5}});
	EXPECT_EQ(server_->PlayerCount(), 0u);
}

TEST_F(GameServerTest, ANinthPlayerIsTurnedAway) {
	for (std::uint32_t i = 0; i < Scene::kMaxPlayers; ++i) {
		EXPECT_EQ(Join(ClientId{100 + i}), i);
	}
	server_->Connect(ClientId{200});
	Message(ClientId{200}, net::Hello{.name = net::PlayerName("p")});
	EXPECT_EQ(outbox_.Last<net::Reject>(ClientId{200}).reason,
			  net::RejectReason::Full);
	EXPECT_EQ(server_->PlayerCount(), Scene::kMaxPlayers);
}

TEST_F(GameServerTest, WhatIsNotTheProtocolClosesTheConnection) {
	server_->Connect(ClientId{3});
	const std::array<std::uint8_t, 3> garbage{0xDE, 0xAD, 0x00};
	server_->Receive(ClientId{3}, garbage);
	EXPECT_EQ(outbox_.closed, std::vector<ClientId>{ClientId{3}});
	// Nothing it sends after is read
	Message(ClientId{3}, net::Hello{.name = net::PlayerName("p")});
	EXPECT_EQ(outbox_.Count<net::Welcome>(ClientId{3}), 0u);
	EXPECT_EQ(outbox_.closed.size(), 1u) << "closed once";
}

// A game says hello first and once, then sends its commands, each in turn,
// and pings; what only the server sends, it never does
TEST_F(GameServerTest, WhatAGameWouldNotSendClosesTheConnection) {
	Join(ClientId{1});
	Message(ClientId{1}, net::Pong{});
	EXPECT_TRUE(Closed(ClientId{1})) << "what only the server sends";
	Join(ClientId{2});
	Message(ClientId{2}, net::Hello{.name = net::PlayerName("p")});
	EXPECT_TRUE(Closed(ClientId{2})) << "a second hello";
	server_->Connect(ClientId{3});
	Message(ClientId{3}, net::Ping{});
	EXPECT_TRUE(Closed(ClientId{3})) << "a ping before hello";
	server_->Connect(ClientId{4});
	Walk(ClientId{4}, 1, 1, 0.0);
	EXPECT_TRUE(Closed(ClientId{4})) << "commands before hello";
	Join(ClientId{5});
	Walk(ClientId{5}, 1, 4, 0.0);
	Walk(ClientId{5}, 9, 4, 0.0);
	EXPECT_TRUE(Closed(ClientId{5})) << "commands 5 to 8 skipped";
	Join(ClientId{6});
	Walk(ClientId{6}, 1, 4, 0.0);
	Walk(ClientId{6}, 5, 4, 0.0);
	Walk(ClientId{6}, 1, 2, 0.0);
	EXPECT_TRUE(Closed(ClientId{6})) << "commands going back";
	// As a game sends them: each new one with those before it, so each
	// comes more than once
	Join(ClientId{7});
	Walk(ClientId{7}, 1, 1, 0.0);
	Walk(ClientId{7}, 1, 2, 0.0);
	Walk(ClientId{7}, 3, 4, 0.0);
	Walk(ClientId{7}, 3, 4, 0.0);
	Message(ClientId{7}, net::Ping{});
	EXPECT_FALSE(Closed(ClientId{7}));
	EXPECT_EQ(outbox_.Count<net::Pong>(ClientId{7}), 1u);
}

TEST_F(GameServerTest, AConnectionThatSaysNoHelloInTimeGoes) {
	server_->Connect(ClientId{1});
	server_->Connect(ClientId{2});
	const auto ticks = static_cast<int>(
		std::lround(GameServer::kHelloSeconds / GameServer::kTickSeconds));
	Ticks(ticks - 5);
	Message(ClientId{2}, net::Hello{.name = net::PlayerName("p")});
	EXPECT_TRUE(outbox_.closed.empty());
	Ticks(10);
	EXPECT_EQ(outbox_.closed, std::vector<ClientId>{ClientId{1}});
	EXPECT_EQ(server_->PlayerCount(), 1u);
}

// A game sends a command a tick and a ping a second: at that pace its
// budget stays full, so a stall's worth come in at once is taken; more than
// the budget at once is a flood, and ends the connection
TEST_F(GameServerTest, AFloodOfMessagesClosesTheConnection) {
	Join(ClientId{1});
	Join(ClientId{2});
	for (std::uint32_t sequence = 1; sequence <= 600; ++sequence) {
		Walk(ClientId{1}, sequence, 1, 0.0);
		if (sequence % 60 == 0) {
			Message(ClientId{1}, net::Ping{});
		}
		Ticks(1);
	}
	for (std::uint32_t i = 0; i < GameServer::kMessageBurst; ++i) {
		Message(ClientId{1}, net::Ping{});
	}
	EXPECT_FALSE(Closed(ClientId{1}));
	for (std::uint32_t i = 0; i <= GameServer::kMessageBurst; ++i) {
		Message(ClientId{2}, net::Ping{});
	}
	EXPECT_TRUE(Closed(ClientId{2}));
	EXPECT_EQ(outbox_.Count<net::Pong>(ClientId{2}), GameServer::kMessageBurst);
}

// A round trip longer than any is shown as the longest
TEST_F(GameServerTest, ARoundTripIsShownNoLongerThanTheLongest) {
	Join(ClientId{1});
	Message(ClientId{1}, net::Ping{.stamp = 1, .rtt = 60000});
	Ticks(static_cast<int>(GameServer::kScoresEvery));
	const auto pings = outbox_.Last<net::Pings>(ClientId{1});
	ASSERT_EQ(pings.count, 1u);
	EXPECT_EQ(pings.players[0].rtt, GameServer::kMaxRtt);
}

TEST_F(GameServerTest, CommandsMoveTheirPlayerOneATick) {
	const std::size_t slot = Join(ClientId{1});
	const vector2d start = PlayerIn(slot).GetPose();
	const double theta = PlayerIn(slot).GetPosition().theta;
	Walk(ClientId{1}, 1, 4, theta);
	Ticks(2);
	const auto snapshot = outbox_.Last<net::Snapshot>(ClientId{1});
	EXPECT_EQ(snapshot.ack, 2u) << "two ticks, two commands";
	EXPECT_EQ(snapshot.tick, 2u);
	ASSERT_EQ(snapshot.count, 1u);
	EXPECT_GT(PlayerIn(slot).GetPose().Distance(start), 0.05);
	EXPECT_NEAR(snapshot.players[0].pose.x, PlayerIn(slot).GetPose().x,
				1.0 / 256);
	// The same commands again change nothing: each is applied once
	Walk(ClientId{1}, 1, 4, theta);
	Ticks(2);
	EXPECT_EQ(outbox_.Last<net::Snapshot>(ClientId{1}).ack, 4u);
}

// With no command come in, a player goes on as it was, but a key pressed
// once is not pressed again
TEST_F(GameServerTest, ALateCommandIsHeldNotPressedAgain) {
	const std::size_t slot = Join(ClientId{1});
	const double theta = PlayerIn(slot).GetPosition().theta;
	Message(ClientId{1},
			net::Input{.count = 1,
					   .commands = {{{.sequence = 1,
									  .command = {.forward = 1,
												  .use = true,
												  .has_view = true,
												  .view_theta = theta}}}}});
	Ticks(1);
	EXPECT_TRUE(PlayerIn(slot).IsUsing());
	const vector2d after_one = PlayerIn(slot).GetPose();
	Ticks(1);
	EXPECT_FALSE(PlayerIn(slot).IsUsing());
	EXPECT_GT(PlayerIn(slot).GetPose().Distance(after_one), 0.01)
		<< "still walking";
}

// Commands that pile up (after a stall) are caught up to the last two
TEST_F(GameServerTest, AFarAheadPlayerIsCaughtUp) {
	const std::size_t slot = Join(ClientId{1});
	const double theta = PlayerIn(slot).GetPosition().theta;
	for (std::uint32_t first = 1; first <= 17; first += 4) {
		Walk(ClientId{1}, first, 4, theta);
	}
	Ticks(2);
	EXPECT_EQ(outbox_.Last<net::Snapshot>(ClientId{1}).ack, 20u);
}

TEST_F(GameServerTest, SnapshotsGoToEveryPlayerEveryOtherTick) {
	Join(ClientId{1});
	Join(ClientId{2});
	server_->Connect(ClientId{3});	// not said hello: not a player
	Ticks(1);
	EXPECT_EQ(outbox_.Count<net::Snapshot>(ClientId{1}), 0u);
	Ticks(1);
	EXPECT_EQ(outbox_.Count<net::Snapshot>(ClientId{1}), 1u);
	EXPECT_EQ(outbox_.Count<net::Snapshot>(ClientId{2}), 1u);
	EXPECT_EQ(outbox_.Count<net::Snapshot>(ClientId{3}), 0u);
	EXPECT_EQ(outbox_.Last<net::Snapshot>(ClientId{2}).count, 2u);
}

TEST_F(GameServerTest, ALeavingPlayerFreesItsSlot) {
	Join(ClientId{1});
	Join(ClientId{2});
	server_->Disconnect(ClientId{1});
	EXPECT_EQ(server_->PlayerCount(), 1u);
	EXPECT_EQ(server_->GetWorld().FindPlayer(0), nullptr);
	EXPECT_EQ(Join(ClientId{3}), 0u) << "the freed slot, taken again";
}

// Every player told of a kill, and the scores with it: a frag for the
// killer, a death for the fallen; killing itself costs a player a frag
TEST_F(GameServerTest, AKillIsToldAndCounted) {
	Join(ClientId{1});
	Join(ClientId{2});
	Kill(0, 1);
	Ticks(1);
	const auto events = outbox_.Last<net::Events>(ClientId{2});
	ASSERT_GE(events.count, 1u);
	const net::Event& kill = events.events[events.count - 1];
	EXPECT_EQ(kill.type, net::EventType::Kill);
	EXPECT_EQ(kill.slot, 0u);
	EXPECT_EQ(kill.other, 1u);
	const auto scores = outbox_.Last<net::Scores>(ClientId{1});
	ASSERT_EQ(scores.count, 2u);
	EXPECT_EQ(scores.players[0].frags, 1);
	EXPECT_EQ(scores.players[1].deaths, 1u);
	EXPECT_EQ(scores.players[0].name.View(), "p");

	Ticks(300);	 // back in
	Kill(0, 0);
	Ticks(1);
	EXPECT_EQ(server_->Rules().StandingOf(0).frags, 0);
	EXPECT_EQ(server_->Rules().StandingOf(0).deaths, 1);
}

// Down, a player comes back after the respawn time, whole, at the spawn
// point furthest from the others, shielded; firing brings it back sooner
TEST_F(GameServerTest, APlayerDownComesBackFarFromTheOthers) {
	Join(ClientId{1});
	Join(ClientId{2});
	Kill(0, 1);
	const auto respawn = static_cast<int>(
		std::ceil(MatchSettings{}.respawn_seconds / GameServer::kTickSeconds));
	Ticks(respawn - 2);
	EXPECT_FALSE(PlayerIn(1).IsAlive());
	Ticks(4);  // and the snapshot after
	ASSERT_TRUE(PlayerIn(1).IsAlive());
	EXPECT_EQ(PlayerIn(1).GetHealth(), 100.0);
	EXPECT_TRUE(PlayerIn(1).IsProtected());
	const Position2D furthest = server_->Rules().FarthestSpawn(1);
	EXPECT_LT(PlayerIn(1).GetPose().Distance(furthest.pose), 0.1);
	EXPECT_TRUE(outbox_.Last<net::Snapshot>(ClientId{2}).players[1].shielded);

	// Firing, it comes back after the shorter wait
	Ticks(120);
	Kill(0, 1);
	Message(
		ClientId{2},
		net::Input{.count = 1,
				   .commands = {{{.sequence = 1, .command = {.fire = true}}}}});
	const auto early = static_cast<int>(
		std::ceil(MatchSettings{}.respawn_early / GameServer::kTickSeconds));
	Ticks(early + 2);
	EXPECT_TRUE(PlayerIn(1).IsAlive());
}

// A ping is answered at once, before any tick, with its own clock; what
// round trip each player says it has, everyone is told with the scores
TEST_F(GameServerTest, APingIsAnsweredAtOnceAndToldToAll) {
	Join(ClientId{1});
	Join(ClientId{2});
	Ticks(1);
	EXPECT_GE(outbox_.Count<net::Pings>(ClientId{2}), 1u)
		<< "the server shows it answers pings";
	Message(ClientId{1}, net::Ping{.stamp = 1234, .rtt = 57});
	EXPECT_EQ(outbox_.Last<net::Pong>(ClientId{1}).stamp, 1234u);
	Ticks(static_cast<int>(GameServer::kScoresEvery));
	const auto pings = outbox_.Last<net::Pings>(ClientId{2});
	ASSERT_EQ(pings.count, 2u);
	EXPECT_EQ(pings.players[0].slot, 0u);
	EXPECT_EQ(pings.players[0].rtt, 57u);
	EXPECT_EQ(pings.players[1].rtt, 0u) << "not told yet";
}

// A player whose connection dropped, back under its name soon after, has
// its score back; another name, or the next match, starts from none
TEST_F(GameServerTest, APlayerBackSoonHasItsScore) {
	Join(ClientId{1}, "ann");
	Join(ClientId{2}, "bob");
	Join(ClientId{3}, "cat");
	Kill(0, 1);
	Ticks(1);
	server_->Disconnect(ClientId{1});
	Ticks(600);
	const std::size_t slot = Join(ClientId{4}, "ann");
	EXPECT_EQ(server_->Rules().StandingOf(slot).frags, 1);
	server_->Disconnect(ClientId{2});
	EXPECT_EQ(server_->Rules().StandingOf(Join(ClientId{5}, "dan")).deaths, 0)
		<< "not bob";
	// Too late
	server_->Disconnect(ClientId{4});
	Ticks(static_cast<int>(GameServer::kComebackSeconds /
						   GameServer::kTickSeconds) +
		  1);
	EXPECT_EQ(server_->Rules().StandingOf(Join(ClientId{6}, "ann")).frags, 0);
}

// Coming back before the server knew the old connection was gone: the old
// one, silent for a while, goes, and the score comes with its name
TEST_F(GameServerTest, ANewConnectionUnderASilentPlayersNameReplacesIt) {
	Join(ClientId{1}, "ann");
	Join(ClientId{2}, "bob");
	Kill(0, 1);
	Ticks(1);
	// Still heard from: a namesake joins beside it
	EXPECT_EQ(Join(ClientId{3}, "bob"), 2u);
	server_->Disconnect(ClientId{3});
	Ticks(
		static_cast<int>(GameServer::kStaleSeconds / GameServer::kTickSeconds) +
		2);
	const std::size_t slot = Join(ClientId{4}, "ann");
	EXPECT_EQ(slot, 0u) << "the silent ann's slot, freed";
	EXPECT_EQ(server_->Rules().StandingOf(slot).frags, 1);
	EXPECT_NE(std::ranges::find(outbox_.closed, ClientId{1}),
			  outbox_.closed.end());
}

// The first to the frag limit wins: the result shows, no one is hurt, and
// then the next match begins, the scores none again
TEST_F(GameServerTest, TheFragLimitEndsTheMatch) {
	Play({.frag_limit = 2, .intermission_seconds = 5.0});
	Join(ClientId{1});
	Join(ClientId{2});
	Kill(0, 1);
	Ticks(240);
	Kill(0, 1);
	Ticks(1);
	auto scores = outbox_.Last<net::Scores>(ClientId{1});
	EXPECT_EQ(scores.phase, net::MatchPhase::Intermission);
	EXPECT_EQ(scores.winner, 0u);
	EXPECT_FALSE(Level().HurtPlayer(Mutable(0), 10.0, 1, 0))
		<< "no one is hurt while the result shows";
	Ticks(200);
	EXPECT_TRUE(PlayerIn(1).IsAlive()) << "back in, shielded too";
	EXPECT_FALSE(Level().HurtPlayer(Mutable(1), 10.0, 0, 0));
	Ticks(120);
	scores = outbox_.Last<net::Scores>(ClientId{1});
	EXPECT_EQ(scores.phase, net::MatchPhase::Playing);
	EXPECT_EQ(scores.winner, net::kNoWinner);
	EXPECT_EQ(scores.players[0].frags, 0);
}

// Past the time limit the leader wins
TEST_F(GameServerTest, TheClockEndsTheMatch) {
	Play({.time_limit = 2.0});
	Join(ClientId{1});
	Join(ClientId{2});
	Kill(1, 0);
	Ticks(119);
	EXPECT_EQ(server_->Rules().Phase(), net::MatchPhase::Playing);
	Ticks(2);
	EXPECT_EQ(server_->Rules().Phase(), net::MatchPhase::Intermission);
	EXPECT_EQ(server_->Rules().Winner(), 1u);
}

// A gun race: each kill takes the killer up the ladder, carrying its next
// weapon only; a kill with the last wins. The ladder's weapons and their
// rounds are not lying in the level.
TEST_F(GameServerTest, AGunRaceClimbsTheLadder) {
	Play({.mode = net::MatchMode::GunRace});
	const auto& ladder = server_->GetWorld().Config().gun_race;
	ASSERT_GE(ladder.size(), 2u);
	Join(ClientId{1});
	Join(ClientId{2});
	EXPECT_EQ(PlayerIn(0).HeldWeapon(), ladder[0]);
	for (const Pickup* pickup : Level().GetPickups()) {
		const PickupEffect& effect = pickup->GetEffect();
		EXPECT_EQ(pickup->IsTaken(),
				  effect.weapons != 0 || effect.ammo_boxes > 0);
	}
	for (std::size_t step = 1; step < ladder.size(); ++step) {
		Kill(0, 1);
		Ticks(1);
		EXPECT_EQ(server_->Rules().StandingOf(0).step, step);
		EXPECT_EQ(PlayerIn(0).GetOwnedWeapons(), 1U << ladder[step]);
		EXPECT_EQ(PlayerIn(0).HeldWeapon(), ladder[step]);
		Ticks(240);
	}
	EXPECT_EQ(server_->Rules().Phase(), net::MatchPhase::Playing);
	Kill(0, 1);
	Ticks(1);
	EXPECT_EQ(server_->Rules().Phase(), net::MatchPhase::Intermission);
	EXPECT_EQ(server_->Rules().Winner(), 0u);
	EXPECT_EQ(outbox_.Last<net::Scores>(ClientId{2}).mode,
			  net::MatchMode::GunRace);
}

// A pickup taken is gone for everyone, and comes back a while later
TEST_F(GameServerTest, APickupComesBack) {
	Join(ClientId{1});
	Pickup& pickup = *Level().GetPickups()[0];
	ASSERT_NE(pickup.GetEffect().weapons, 0) << "the level's first: a gun";
	Mutable(0).SetPosition(Position2D(pickup.GetPose(), 0.0));
	Ticks(2);
	EXPECT_TRUE(pickup.IsTaken());
	EXPECT_EQ(outbox_.Last<net::Snapshot>(ClientId{1}).taken & 1U, 1U);
	Mutable(0).SetPosition(server_->GetWorld().Spawns()[0]);
	Ticks(19 * 60);
	EXPECT_TRUE(pickup.IsTaken());
	Ticks(62);
	EXPECT_FALSE(pickup.IsTaken());
	EXPECT_EQ(outbox_.Last<net::Snapshot>(ClientId{1}).taken & 1U, 0U);
}

// A shot is judged against the other where the shooter's game showed it:
// placed there a while ago, it is struck there, not where it is now
TEST_F(GameServerTest, AShotStrikesWhereTheShooterSawTheOther) {
	Join(ClientId{1});
	Join(ClientId{2});
	Ticks(120);	 // past the shield
	// b stands in a's line of fire, then steps out of it
	const double facing = std::numbers::pi / 2;	 // along +y
	Mutable(0).SetPosition(Position2D({1.5, 10.5}, facing));
	Mutable(1).SetPosition(Position2D({1.5, 14.5}, -facing));
	Ticks(1);
	const std::uint32_t there = server_->CurrentTick();
	Mutable(1).SetPosition(Position2D({3.5, 14.5}, -facing));
	Ticks(10);
	// a fires, having seen b where it stood
	Message(ClientId{1},
			net::Input{.count = 1,
					   .seen = there,
					   .commands = {{{.sequence = 1,
									  .command = {.fire = true,
												  .has_view = true,
												  .view_theta = facing}}}}});
	Ticks(1);
	EXPECT_LT(PlayerIn(1).GetHealth(), 100.0) << "struck where it was seen";
	// Seen where it is now, somewhere else, the shot misses
	Ticks(60);
	const double health = PlayerIn(1).GetHealth();
	Message(ClientId{1},
			net::Input{.count = 1,
					   .seen = server_->CurrentTick(),
					   .commands = {{{.sequence = 2,
									  .command = {.fire = true,
												  .has_view = true,
												  .view_theta = facing}}}}});
	Ticks(1);
	EXPECT_EQ(PlayerIn(1).GetHealth(), health);
}

#ifdef KARAKALE_COUNTS_ALLOCATIONS
// Players fighting, falling and coming back: the ticks allocate nothing
TEST_F(GameServerTest, AFightAllocatesNothing) {
	for (std::uint32_t i = 0; i < Scene::kMaxPlayers; ++i) {
		Join(ClientId{i + 1});
	}
	outbox_.recording = false;
	Ticks(4);
	const auto before = AllocationStats::count;
	for (std::uint32_t sequence = 1; sequence < 400; sequence += 4) {
		for (std::uint32_t i = 0; i < Scene::kMaxPlayers; ++i) {
			net::Input input{.count = 4, .seen = server_->CurrentTick()};
			for (std::uint32_t k = 0; k < 4; ++k) {
				input.commands[k] = {
					.sequence = sequence + k,
					.command = {.forward = 1,
								.fire = true,
								.weapon = static_cast<std::int8_t>(i % 8),
								.has_view = true,
								.view_theta = 0.8 * i}};
			}
			Message(ClientId{i + 1}, input);
		}
		if (sequence % 40 == 1) {
			Kill(sequence % 8, (sequence + 3) % 8);
		}
		Ticks(4);
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}

TEST_F(GameServerTest, ATickAllocatesNothing) {
	for (std::uint32_t i = 0; i < Scene::kMaxPlayers; ++i) {
		Join(ClientId{i + 1});
	}
	outbox_.recording = false;
	Ticks(4);
	const auto before = AllocationStats::count;
	for (std::uint32_t sequence = 1; sequence < 200; sequence += 4) {
		for (std::uint32_t i = 0; i < Scene::kMaxPlayers; ++i) {
			Walk(ClientId{i + 1}, sequence, 4, 0.5 * i);
		}
		Ticks(4);
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
	EXPECT_GT(outbox_.sent_count, 0u);
}
#endif

}  // namespace
}  // namespace karakale
