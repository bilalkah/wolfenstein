// The matches one server holds: a room's code from the address, friends in
// the same room in the same match and apart from the open one's, no more
// rooms than the server holds, and a room gone a while after its last
// player left, the open one never

#include "Server/lobby.h"
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <variant>

namespace wolfenstein {
namespace {

// What was sent, the last welcome to each; whom it closed
class Welcomes : public Outbox
{
  public:
	void Send(ClientId client, std::span<const std::uint8_t> message) override {
		const auto decoded = net::Decode(message);
		if (decoded && std::holds_alternative<net::Welcome>(*decoded)) {
			slots[client] = std::get<net::Welcome>(*decoded).slot;
		}
	}
	void Close(ClientId client) override { closed[client] = true; }

	std::map<ClientId, std::size_t> slots;
	std::map<ClientId, bool> closed;
};

class LobbyTest : public ::testing::Test
{
  protected:
	LobbyTest() {
		auto created = Lobby::Create([this] {
			++made_;
			return GameServer::Create(RESOURCE_DIR, {"bazaar.json"}, outbox_);
		});
		EXPECT_TRUE(created) << (created ? "" : created.error());
		if (created) {
			lobby_ = std::move(*created);
		}
	}
	// A connection to `room` that says hello; whether it got in
	bool Join(ClientId client, const std::string& room) {
		if (!lobby_->Connect(client, room)) {
			return false;
		}
		std::array<std::uint8_t, net::kMaxMessage> buffer{};
		const std::size_t size =
			net::Encode(net::Hello{.name = net::PlayerName("p")}, buffer);
		lobby_->Receive(client, std::span(buffer).first(size));
		return outbox_.slots.contains(client);
	}

	Welcomes outbox_;
	std::unique_ptr<Lobby> lobby_;
	int made_ = 0;
};

TEST(LobbyRoom, TheCodeIsItsLettersAndDigitsInCapitals) {
	EXPECT_EQ(Lobby::RoomOf("/"), "");
	EXPECT_EQ(Lobby::RoomOf(""), "");
	EXPECT_EQ(Lobby::RoomOf("/room/friday"), "FRIDAY");
	EXPECT_EQ(Lobby::RoomOf("/room/b-2%207"), "B2207");
	EXPECT_EQ(Lobby::RoomOf("/room/"), "") << "no code: the open room";
	EXPECT_EQ(Lobby::RoomOf("/other/x"), "");
	EXPECT_EQ(Lobby::RoomOf("/room/abcdefghijklmnopq"), "ABCDEFGHIJKL");
}

// Two in a room share its match; one in the open room is in another
TEST_F(LobbyTest, FriendsInARoomPlayTogether) {
	ASSERT_TRUE(Join(ClientId{1}, "FRIDAY"));
	ASSERT_TRUE(Join(ClientId{2}, "FRIDAY"));
	ASSERT_TRUE(Join(ClientId{3}, ""));
	EXPECT_EQ(outbox_.slots[ClientId{1}], 0u);
	EXPECT_EQ(outbox_.slots[ClientId{2}], 1u) << "the same match";
	EXPECT_EQ(outbox_.slots[ClientId{3}], 0u) << "another match";
	EXPECT_EQ(lobby_->Find("FRIDAY")->PlayerCount(), 2u);
	EXPECT_EQ(lobby_->Find("")->PlayerCount(), 1u);
	EXPECT_EQ(lobby_->RoomCount(), 1u);
	EXPECT_EQ(made_, 2) << "the open room, then FRIDAY's";
}

TEST_F(LobbyTest, NoMoreRoomsThanItHolds) {
	for (std::size_t i = 0; i < Lobby::kMaxRooms; ++i) {
		ASSERT_TRUE(lobby_->Connect(ClientId{static_cast<std::uint32_t>(i + 1)},
									"R" + std::to_string(i)));
	}
	EXPECT_FALSE(lobby_->Connect(ClientId{100}, "ONEMORE"));
	EXPECT_TRUE(lobby_->Connect(ClientId{101}, "R3")) << "one already there";
	EXPECT_TRUE(lobby_->Connect(ClientId{102}, "")) << "the open one";
}

// Empty, a room closes after a while, its code free for a new one; the
// open room stays
TEST_F(LobbyTest, AnEmptyRoomCloses) {
	ASSERT_TRUE(Join(ClientId{1}, "GONE"));
	ASSERT_TRUE(Join(ClientId{2}, ""));
	lobby_->Disconnect(ClientId{1});
	lobby_->Disconnect(ClientId{2});
	const auto ticks =
		static_cast<int>(Lobby::kEmptySeconds / GameServer::kTickSeconds);
	for (int i = 0; i < ticks - 10; ++i) {
		lobby_->Tick();
	}
	EXPECT_NE(lobby_->Find("GONE"), nullptr) << "not yet";
	for (int i = 0; i < 20; ++i) {
		lobby_->Tick();
	}
	EXPECT_EQ(lobby_->Find("GONE"), nullptr);
	EXPECT_NE(lobby_->Find(""), nullptr);
	EXPECT_EQ(lobby_->RoomCount(), 0u);
}

}  // namespace
}  // namespace wolfenstein
