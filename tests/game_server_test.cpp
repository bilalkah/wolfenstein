// A multiplayer game as the server runs it, played with no network: a
// player says hello and gets a slot and the level (or is turned away: another
// version, no slot free); its commands move its player, one a tick, each
// once, and a late one is held, not pressed again; every other tick every
// player hears how the players stand; a player leaving frees its slot. A
// tick allocates nothing.

#include "Server/game_server.h"
#include "Profiler/profiler.h"
#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <memory>
#include <numbers>
#include <ranges>
#include <variant>
#include <vector>

namespace wolfenstein {
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
	GameServerTest() {
		auto created = GameServer::Create(RESOURCE_DIR, "bazaar.json", outbox_);
		EXPECT_TRUE(created) << (created ? "" : created.error());
		if (created) {
			server_ = std::move(*created);
		}
	}

	void Message(ClientId client, const net::Message& message) {
		std::array<std::uint8_t, net::kMaxMessage> buffer{};
		const std::size_t size = net::Encode(message, buffer);
		ASSERT_GT(size, 0u);
		server_->Receive(client, std::span(buffer).first(size));
	}
	// A connection that says hello; its slot
	std::size_t Join(ClientId client) {
		server_->Connect(client);
		Message(client, net::Hello{.name = net::PlayerName("p")});
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
	// In the level, where its slot comes in
	const Position2D spawn = server_->GetWorld().SpawnFor(1);
	EXPECT_EQ(PlayerIn(1).GetPose().x, spawn.pose.x);
}

TEST_F(GameServerTest, AnotherVersionIsTurnedAway) {
	server_->Connect(ClientId{5});
	Message(ClientId{5}, net::Hello{.version = net::kProtocolVersion + 1});
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
	Message(ClientId{200}, net::Hello{});
	EXPECT_EQ(outbox_.Last<net::Reject>(ClientId{200}).reason,
			  net::RejectReason::Full);
	EXPECT_EQ(server_->PlayerCount(), Scene::kMaxPlayers);
}

TEST_F(GameServerTest, WhatIsNotTheProtocolClosesTheConnection) {
	server_->Connect(ClientId{3});
	const std::array<std::uint8_t, 3> garbage{0xDE, 0xAD, 0x00};
	server_->Receive(ClientId{3}, garbage);
	EXPECT_EQ(outbox_.closed, std::vector<ClientId>{ClientId{3}});
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

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
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
}  // namespace wolfenstein
