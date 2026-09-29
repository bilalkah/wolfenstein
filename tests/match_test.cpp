// A multiplayer game end to end, in one process: a server and two players'
// games, their messages carried in memory, late by as many ticks as a test
// asks. Welcomed, each player walks its own player at once, where the
// server agrees it goes, and sees the other where the server had it a
// moment ago; bumping into the other, what it foresaw differently is put
// right; a player leaving is gone from the other's game.

#include "Client/match_client.h"
#include "Server/game_server.h"
#include "test_services.h"
#include <deque>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <vector>

namespace wolfenstein {
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
			inbox_.Put(message);
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
	MatchTest() {
		auto created = GameServer::Create(RESOURCE_DIR, "bazaar.json", loop_);
		EXPECT_TRUE(created) << (created ? "" : created.error());
		if (created) {
			server_ = std::move(*created);
		}
		loop_.server = server_.get();
	}

	PlayerGame& Join(ClientId id) {
		auto loader = SceneLoader::Open(RESOURCE_DIR);
		EXPECT_TRUE(loader);
		players_.push_back(std::make_unique<PlayerGame>(PlayerGame{
			.world = std::make_unique<World>(testing::TestTextures(),
											 std::move(*loader),
											 std::make_unique<SoundManager>()),
			.match = std::make_unique<MatchClient>(loop_.Connect(id), "p"),
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
}  // namespace wolfenstein
