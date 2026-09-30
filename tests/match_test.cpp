// A multiplayer game end to end, in one process: a server and two players'
// games, their messages carried in memory, late by as many ticks as a test
// asks. Welcomed, each player walks its own player at once, where the
// server agrees it goes, and sees the other where the server had it a
// moment ago; bumping into the other, what it foresaw differently is put
// right; a player leaving is gone from the other's game. A kill is seen in
// both games, the fallen player down in its own and back where the server
// brings it; what a player carries agrees with the server's; a pickup
// taken is gone in every game.

#include "Client/match_client.h"
#include "Server/game_server.h"
#include "test_services.h"
#include <bit>
#include <deque>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace karakale {
namespace {

// Carries messages both ways, each `delay` ticks late
class Loopback : public Outbox
{
  public:
	class Link : public net::Connection
	{
	  public:
		Link(Loopback& loop, ClientId id) : loop_(loop), id_(id) {}
		State GetState() const override { return state; }
		bool Send(std::span<const std::uint8_t> message) override {
			loop_.to_server_.push_back({loop_.now_ + loop_.delay,
										id_,
										{message.begin(), message.end()}});
			return true;
		}
		void Deliver(std::span<const std::uint8_t> message) {
			inbox_.Put(message, loop_.Seconds());
		}
		State state = State::Open;

	  private:
		Loopback& loop_;
		ClientId id_;
	};

	std::unique_ptr<net::Connection> Connect(ClientId id) {
		auto link = std::make_unique<Link>(*this, id);
		links_[id] = link.get();
		server->Connect(id);
		return link;
	}
	void Disconnect(ClientId id) {
		links_.erase(id);
		server->Disconnect(id);
	}
	void Send(ClientId id, std::span<const std::uint8_t> message) override {
		to_players_.push_back(
			{now_ + delay, id, {message.begin(), message.end()}});
	}
	void Close(ClientId id) override {
		if (const auto link = links_.find(id); link != links_.end()) {
			link->second->state = net::Connection::State::Closed;
		}
	}
	// One tick on: what is due arrives, the server ticks
	void Tick() {
		++now_;
		Deliver(to_server_, [this](const Message& m) {
			server->Receive(m.client, m.bytes);
		});
		server->Tick();
		Deliver(to_players_, [this](const Message& m) {
			if (const auto link = links_.find(m.client); link != links_.end()) {
				link->second->Deliver(m.bytes);
			}
		});
	}

	// The time the messages are carried in, in seconds
	double Seconds() const { return now_ * net::kTickSeconds; }

	GameServer* server = nullptr;
	int delay = 0;

  private:
	struct Message
	{
		int due;
		ClientId client;
		std::vector<std::uint8_t> bytes;
	};
	template <typename Arrive>
	void Deliver(std::deque<Message>& queue, Arrive arrive) {
		while (!queue.empty() && queue.front().due <= now_) {
			arrive(queue.front());
			queue.pop_front();
		}
	}

	int now_ = 0;
	std::deque<Message> to_server_;
	std::deque<Message> to_players_;
	std::map<ClientId, Link*> links_;
};

// A player's game: its own world, and its side of the match
struct PlayerGame
{
	std::unique_ptr<World> world;
	std::unique_ptr<MatchClient> match;
	PlayerCommand command;

	void Tick() const {
		match->Poll(*world);
		if (match->GetState() != MatchClient::State::Playing) {
			return;
		}
		match->BeforeTick(*world, command);
		world->CurrentLevel().Update(net::kTickSeconds);
		match->AfterTick(*world);
	}
	Player& Me() const { return world->GetPlayer(); }
};

class MatchTest : public ::testing::Test
{
  protected:
	MatchTest() { Play({}); }

	// A server afresh, playing by `settings` on `arenas`; before anyone
	// joins
	void Play(const MatchSettings& settings,
			  std::vector<std::string> arenas = {"bazaar.json"}) {
		server_.reset();
		auto created = GameServer::Create(RESOURCE_DIR, std::move(arenas),
										  loop_, settings);
		EXPECT_TRUE(created) << (created ? "" : created.error());
		if (created) {
			server_ = std::move(*created);
		}
		loop_.server = server_.get();
	}
	// The player in `slot` stands at `at`, on the server and in its own game
	void Place(std::size_t slot, PlayerGame& game, const Position2D& at) {
		server_->GetWorld().FindPlayer(slot)->SetPosition(at);
		game.Me().SetPosition(at);
	}
	// Ticks until `done`, at most `most`; whether it came
	template <typename Done>
	bool TicksUntil(Done done, int most) {
		for (int i = 0; i < most; ++i) {
			if (done()) {
				return true;
			}
			Ticks(1);
		}
		return done();
	}

	PlayerGame& Join(ClientId id) {
		auto loader = SceneLoader::Open(RESOURCE_DIR);
		EXPECT_TRUE(loader);
		players_.push_back(std::make_unique<PlayerGame>(PlayerGame{
			.world = std::make_unique<World>(testing::TestTextures(),
											 std::move(*loader),
											 std::make_unique<SoundManager>()),
			.match = std::make_unique<MatchClient>(
				loop_.Connect(id), "p", [this] { return loop_.Seconds(); }),
			.command = {}}));
		return *players_.back();
	}
	void Ticks(int count) {
		for (int i = 0; i < count; ++i) {
			for (const auto& player : players_) {
				player->Tick();
			}
			loop_.Tick();
		}
	}
	// Until every player is in the game and has heard where the others are
	// (snapshots come every other tick, as late as the messages are)
	void Settle() {
		for (int i = 0; i < 60; ++i) {
			Ticks(1);
			if (std::ranges::all_of(players_, [](const auto& player) {
					return player->match->GetState() ==
						   MatchClient::State::Playing;
				})) {
				Ticks(2 * loop_.delay + 4);
				return;
			}
		}
		ADD_FAILURE() << "not every player got in";
	}
	const Player& OnServer(std::size_t slot) {
		return *server_->GetWorld().FindPlayer(slot);
	}
	static PlayerCommand Walking(double theta, std::int8_t forward = 1) {
		return {.forward = forward, .has_view = true, .view_theta = theta};
	}

	Loopback loop_;
	std::unique_ptr<GameServer> server_;
	std::vector<std::unique_ptr<PlayerGame>> players_;
};

TEST_F(MatchTest, EachSeesTheOtherWhereTheServerHasThem) {
	PlayerGame& a = Join(ClientId{1});
	PlayerGame& b = Join(ClientId{2});
	Settle();
	ASSERT_EQ(a.world->LocalSlot(), 0u);
	ASSERT_EQ(b.world->LocalSlot(), 1u);
	// Each sees the other, a puppet
	ASSERT_NE(b.world->FindPlayer(0), nullptr);
	EXPECT_TRUE(b.world->FindPlayer(0)->IsPuppet());
	EXPECT_FALSE(b.Me().IsPuppet());

	a.command = Walking(a.Me().GetPosition().theta);
	Ticks(40);
	a.command = Walking(a.Me().GetPosition().theta, 0);
	Ticks(20);	// long enough for the others' view to catch up
	const vector2d server_a = OnServer(0).GetPose();
	EXPECT_GT(server_a.Distance(server_->GetWorld().SpawnFor(0).pose), 0.5)
		<< "a walked";
	EXPECT_LT(a.Me().GetPose().Distance(server_a), 0.01) << "as a foresaw";
	EXPECT_LT(b.world->FindPlayer(0)->GetPose().Distance(server_a), 0.01)
		<< "where b sees it";
	EXPECT_EQ(a.match->Corrections(), 0u) << "nothing to put right";
}

// Late messages: a walks at once, and the server, later, agrees
TEST_F(MatchTest, APlayerWalksAtOnceWhateverTheDelay) {
	loop_.delay = 5;  // 83 ms each way
	PlayerGame& a = Join(ClientId{1});
	PlayerGame& b = Join(ClientId{2});
	Settle();
	Ticks(20);
	const vector2d start = a.Me().GetPose();
	a.command = Walking(a.Me().GetPosition().theta);
	Ticks(1);
	EXPECT_GT(a.Me().GetPose().Distance(start), 0.01) << "moved at once";
	Ticks(29);
	a.command = Walking(a.Me().GetPosition().theta, 0);
	Ticks(30);
	EXPECT_LT(a.Me().GetPose().Distance(OnServer(0).GetPose()), 0.01);
	EXPECT_LT(b.world->FindPlayer(0)->GetPose().Distance(OnServer(0).GetPose()),
			  0.01);
	EXPECT_EQ(a.match->Corrections(), 0u);
}

// Walking into the other player, a stops against it, and ends where the
// server has it, whatever it foresaw
TEST_F(MatchTest, BumpingIntoAnotherEndsWhereTheServerSays) {
	loop_.delay = 3;
	PlayerGame& a = Join(ClientId{1});
	PlayerGame& b = Join(ClientId{2});
	Settle();
	// b steps in front of a
	const Position2D a_at = OnServer(0).GetPosition();
	b.command = Walking(0.0, 0);
	Ticks(10);
	b.Me().SetPosition(Position2D(
		a_at.pose + vector2d{std::cos(a_at.theta), std::sin(a_at.theta)} * 1.5,
		0.0));
	server_->GetWorld().FindPlayer(1)->SetPosition(b.Me().GetPosition());
	Ticks(20);
	a.command = Walking(a_at.theta);
	Ticks(60);
	a.command = Walking(a_at.theta, 0);
	Ticks(30);
	const double apart = OnServer(0).GetPose().Distance(OnServer(1).GetPose());
	EXPECT_GE(apart, OnServer(0).GetWidth() - 1e-6) << "not walked through";
	EXPECT_LT(a.Me().GetPose().Distance(OnServer(0).GetPose()), 0.01);
}

// b walks, a stands still: a hears b's footsteps, and none of its own
TEST_F(MatchTest, AnotherPlayersFootstepsAreHeard) {
	PlayerGame& a = Join(ClientId{1});
	PlayerGame& b = Join(ClientId{2});
	Settle();
	const auto steps = [](const PlayerGame& game) {
		const SoundManager& sound = game.world->Sound();
		return sound.PlayCount(SoundEffect::StepLeft) +
			   sound.PlayCount(SoundEffect::StepRight);
	};
	const auto a_before = steps(a);
	const auto b_before = steps(b);
	b.command = Walking(b.Me().GetPosition().theta);
	Ticks(60);
	b.command = Walking(b.Me().GetPosition().theta, 0);
	Ticks(20);
	const auto walked = steps(b) - b_before;
	EXPECT_GE(walked, 1u) << "b hears itself";
	EXPECT_GE(steps(a) - a_before, walked - 1) << "a hears b, as b does";
}

// a shoots b down: b falls in its own game, both are told of the kill and
// the scores, and b comes back where the server brings it in; a's rounds
// in its own game are the server's
TEST_F(MatchTest, AKillIsSeenInBothGames) {
	loop_.delay = 3;
	PlayerGame& a = Join(ClientId{1});
	PlayerGame& b = Join(ClientId{2});
	Settle();
	Ticks(120);	 // past the shield
	constexpr double kAlongY = std::numbers::pi / 2;
	Place(0, a, Position2D({1.5, 10.5}, kAlongY));
	Place(1, b, Position2D({1.5, 13.5}, -kAlongY));
	b.command = Walking(-kAlongY, 0);
	a.command = Walking(kAlongY, 0);
	Ticks(2 * loop_.delay + 4);
	a.command.fire = true;
	ASSERT_TRUE(TicksUntil([&] { return !OnServer(1).IsAlive(); }, 600));
	a.command.fire = false;
	Ticks(2 * loop_.delay + 4);
	EXPECT_FALSE(b.Me().IsAlive()) << "down in its own game";
	ASSERT_FALSE(a.match->KillFeed().empty());
	EXPECT_EQ(a.match->KillFeed().back().killer, 0u);
	EXPECT_EQ(a.match->KillFeed().back().victim, 1u);
	EXPECT_FALSE(b.match->KillFeed().empty());
	const net::Scores& scores = b.match->GetScores();
	ASSERT_EQ(scores.count, 2u);
	EXPECT_EQ(scores.players[0].frags, 1);
	EXPECT_EQ(scores.players[1].deaths, 1u);
	EXPECT_EQ(a.match->NameOf(1), "p");
	// a's rounds as the server has them
	const std::size_t held = a.Me().HeldWeapon();
	EXPECT_EQ(a.Me().GetWeapon(held).GetAmmo(),
			  OnServer(0).GetWeapon(held).GetAmmo());
	EXPECT_EQ(a.Me().GetWeapon(held).GetReserve(),
			  OnServer(0).GetWeapon(held).GetReserve());

	ASSERT_TRUE(TicksUntil([&] { return OnServer(1).IsAlive(); }, 400));
	Ticks(2 * loop_.delay + 4);
	EXPECT_TRUE(b.Me().IsAlive()) << "back in its own game";
	EXPECT_EQ(b.Me().GetHealth(), 100.0);
	EXPECT_LT(b.Me().GetPose().Distance(OnServer(1).GetPose()), 0.01);
	EXPECT_TRUE(b.match->TakeRevived());
	EXPECT_FALSE(b.match->TakeRevived()) << "told once";
}

// Walking onto a gun: the server gives it, the player's own game carries
// it, and the gun is gone from the other's game
TEST_F(MatchTest, APickupTakenIsGoneInEveryGame) {
	loop_.delay = 2;
	PlayerGame& a = Join(ClientId{1});
	PlayerGame& b = Join(ClientId{2});
	Settle();
	const Pickup& gun = *server_->GetWorld().CurrentLevel().GetPickups()[0];
	ASSERT_NE(gun.GetEffect().weapons, 0);
	const auto weapon =
		static_cast<std::size_t>(std::countr_zero(gun.GetEffect().weapons));
	ASSERT_FALSE(a.Me().Owns(weapon));
	Place(0, a, Position2D(gun.GetPose(), 0.0));
	Ticks(2 * loop_.delay + 6);
	EXPECT_TRUE(gun.IsTaken());
	EXPECT_TRUE(OnServer(0).Owns(weapon));
	EXPECT_TRUE(a.Me().Owns(weapon)) << "given in its own game";
	EXPECT_TRUE(b.world->CurrentLevel().GetPickups()[0]->IsTaken());
	EXPECT_TRUE(a.world->CurrentLevel().GetPickups()[0]->IsTaken());
}

// A gun race: the kill gives the killer its next weapon, in its own game
TEST_F(MatchTest, AGunRaceKillArmsTheKillerInItsGame) {
	Play({.mode = net::MatchMode::GunRace});
	loop_.delay = 2;
	PlayerGame& a = Join(ClientId{1});
	Join(ClientId{2});
	Settle();
	const auto& ladder = server_->GetWorld().Config().gun_race;
	EXPECT_EQ(a.Me().HeldWeapon(), ladder[0]);
	Player& b_there = *server_->GetWorld().FindPlayer(1);
	b_there.Protect(0.0);
	server_->GetWorld().CurrentLevel().HurtPlayer(b_there, 1000.0, 0, 0);
	Ticks(2 * loop_.delay + 6);
	EXPECT_EQ(a.Me().HeldWeapon(), ladder[1]);
	EXPECT_EQ(a.Me().GetOwnedWeapons(), 1U << ladder[1]);
	EXPECT_EQ(a.match->GetScores().players[0].step, 1u);
}

// The match over, the next is on the next arena: every game goes there,
// the players in it, and plays on as before
TEST_F(MatchTest, TheNextMatchIsOnTheNextArena) {
	Play({.frag_limit = 1, .intermission_seconds = 1.0},
		 {"bazaar.json", "warehouse.json"});
	loop_.delay = 2;
	PlayerGame& a = Join(ClientId{1});
	PlayerGame& b = Join(ClientId{2});
	Settle();
	EXPECT_EQ(a.world->LevelName(), "THE BAZAAR");
	Player& b_there = *server_->GetWorld().FindPlayer(1);
	b_there.Protect(0.0);
	server_->GetWorld().CurrentLevel().HurtPlayer(b_there, 1000.0, 0, 0);
	ASSERT_TRUE(TicksUntil(
		[&] {
			return server_->Rules().Phase() == net::MatchPhase::Playing &&
				   server_->GetWorld().LevelName() == "THE WAREHOUSE";
		},
		200));
	Ticks(2 * loop_.delay + 6);
	for (PlayerGame* game : {&a, &b}) {
		EXPECT_EQ(game->world->LevelName(), "THE WAREHOUSE");
		EXPECT_TRUE(game->match->TakeNewLevel()) << "told once";
		EXPECT_FALSE(game->match->TakeNewLevel());
		EXPECT_TRUE(game->Me().IsAlive());
		EXPECT_LT(game->Me().GetPose().Distance(
					  OnServer(game->world->LocalSlot().value_or(0)).GetPose()),
				  0.01);
	}
	ASSERT_NE(b.world->FindPlayer(0), nullptr) << "each sees the other there";
	EXPECT_EQ(a.match->GetScores().players[0].frags, 0) << "scores afresh";
	// Walking there as anywhere: at once, where the server agrees
	const std::size_t corrections = a.match->Corrections();
	a.command = Walking(a.Me().GetPosition().theta);
	Ticks(20);
	a.command = Walking(a.Me().GetPosition().theta, 0);
	Ticks(2 * loop_.delay + 6);
	EXPECT_LT(a.Me().GetPose().Distance(OnServer(0).GetPose()), 0.01);
	EXPECT_EQ(a.match->Corrections(), corrections);
}

// Messages late by 5 ticks each way: a player measures its round trip as
// 10 ticks, and the other is told of it
TEST_F(MatchTest, APlayerKnowsItsPingAndTheOthersToo) {
	loop_.delay = 5;
	PlayerGame& a = Join(ClientId{1});
	PlayerGame& b = Join(ClientId{2});
	Settle();
	EXPECT_FALSE(a.match->PingOf(1)) << "not measured yet";
	Ticks(static_cast<int>(3.0 / net::kTickSeconds));
	const double round_trip = 2 * loop_.delay * net::kTickSeconds * 1000.0;
	EXPECT_NEAR(a.match->Ping().value_or(0), round_trip, 2.0);
	EXPECT_NEAR(b.match->PingOf(0).value_or(0), round_trip, 2.0);
	EXPECT_NEAR(a.match->PingOf(1).value_or(0), round_trip, 2.0);
}

TEST_F(MatchTest, APlayerLeavingIsGoneFromTheOthersGame) {
	PlayerGame& a = Join(ClientId{1});
	Join(ClientId{2});
	Settle();
	ASSERT_NE(a.world->FindPlayer(1), nullptr);
	loop_.Disconnect(ClientId{2});
	Ticks(4);
	EXPECT_EQ(a.world->FindPlayer(1), nullptr);
	EXPECT_EQ(a.world->CurrentLevel().GetPlayers()[1], nullptr);
}

}  // namespace
}  // namespace karakale
