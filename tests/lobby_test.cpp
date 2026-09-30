// The matches one server holds: what an address asks for, a room made with a
// code of the lobby's own and hosted by its maker, friends joining it by the
// code and apart from the open game, a code no room has turned away, no
// more rooms than the server holds (nor than an address may make), and a
// room gone a while after its last player left, the open one never

#include "Server/lobby.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <variant>

namespace karakale {
namespace {

// What was sent: the last welcome and room to each; whom it closed
class Welcomes : public Outbox
{
  public:
	void Send(ClientId client, std::span<const std::uint8_t> message) override {
		const auto decoded = net::Decode(message);
		if (decoded && std::holds_alternative<net::Welcome>(*decoded)) {
			slots[client] = std::get<net::Welcome>(*decoded).slot;
		}
		if (decoded && std::holds_alternative<net::Room>(*decoded)) {
			rooms[client] = std::get<net::Room>(*decoded);
		}
	}
	void Close(ClientId client) override { closed[client] = true; }

	std::map<ClientId, std::size_t> slots;
	std::map<ClientId, net::Room> rooms;
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
	// A connection asking for `request`, from `address`, that says hello
	// once in; the code of its room, or why not
	std::expected<std::string, net::RejectReason> Join(
		ClientId client, const Lobby::Request& request,
		std::string_view address = "198.51.100.1") {
		auto joined = lobby_->Connect(client, request, address);
		if (joined) {
			std::array<std::uint8_t, net::kMaxMessage> buffer{};
			const std::size_t size =
				net::Encode(net::Hello{.name = net::PlayerName("p")}, buffer);
			lobby_->Receive(client, std::span(buffer).first(size));
		}
		return joined;
	}
	std::string Create(ClientId client,
					   std::string_view address = "198.51.100.1") {
		const auto made = Join(client, {.create = true, .code = {}}, address);
		EXPECT_TRUE(made);
		return made.value_or("");
	}

	Welcomes outbox_;
	std::unique_ptr<Lobby> lobby_;
	int made_ = 0;
};

TEST(LobbyRequest, AnAddressAsksForANewRoomARoomOrTheOpenGame) {
	EXPECT_TRUE(Lobby::RequestOf("/create").create);
	EXPECT_FALSE(Lobby::RequestOf("/create/x").create);
	EXPECT_EQ(Lobby::RequestOf("/room/friday").code, "FRIDAY");
	EXPECT_EQ(Lobby::RequestOf("/room/b-2%207").code, "B2207");
	EXPECT_EQ(Lobby::RequestOf("/room/").code, "") << "no code: the open game";
	EXPECT_EQ(Lobby::RequestOf("/").code, "");
	EXPECT_EQ(Lobby::RequestOf("/other/x").code, "");
	EXPECT_EQ(Lobby::RequestOf("/room/abcdefghijk").code, "ABCDEFGH");
}

// The maker gets a code of the lobby's own and hosts the room; a friend
// comes in by the code, into the same match, apart from the open game's
TEST_F(LobbyTest, AMadeRoomIsJoinedByItsCode) {
	const std::string code = Create(ClientId{1});
	ASSERT_EQ(code.size(), Lobby::kCodeLength);
	EXPECT_TRUE(std::ranges::all_of(code, [](char c) {
		return Lobby::kCodeLetters.find(c) != std::string_view::npos;
	}));
	ASSERT_TRUE(Join(ClientId{2}, {.create = false, .code = code}));
	ASSERT_TRUE(Join(ClientId{3}, {}));
	EXPECT_EQ(outbox_.slots[ClientId{1}], 0u);
	EXPECT_EQ(outbox_.slots[ClientId{2}], 1u) << "the same match";
	EXPECT_EQ(outbox_.slots[ClientId{3}], 0u) << "another match";
	const net::Room& room = outbox_.rooms[ClientId{2}];
	EXPECT_EQ(room.code.View(), code);
	EXPECT_EQ(room.host, 0u) << "its maker hosts it";
	EXPECT_EQ(outbox_.rooms[ClientId{3}].host, net::kNoHost)
		<< "the open game has no host";
	EXPECT_EQ(outbox_.rooms[ClientId{3}].code.View(), "");
	EXPECT_EQ(lobby_->Find(code)->PlayerCount(), 2u);
	EXPECT_EQ(lobby_->RoomCount(), 1u);
	EXPECT_EQ(made_, 2) << "the open game, then the room";
}

TEST_F(LobbyTest, ACodeNoRoomHasIsTurnedAway) {
	EXPECT_EQ(Join(ClientId{1}, {.create = false, .code = "NOPE2"}).error(),
			  net::RejectReason::NoRoom);
	EXPECT_EQ(lobby_->RoomCount(), 0u) << "none made for it";
	EXPECT_EQ(made_, 1);
}

// Every room's code its own; no more rooms than the server holds, nor than
// one address may make
TEST_F(LobbyTest, NoMoreRoomsThanItHolds) {
	std::set<std::string> codes;
	for (std::size_t i = 0; i < Lobby::kMaxRooms; ++i) {
		const std::string address = "198.51.100." + std::to_string(i);
		codes.insert(
			Create(ClientId{static_cast<std::uint32_t>(i + 1)}, address));
	}
	EXPECT_EQ(codes.size(), Lobby::kMaxRooms);
	EXPECT_EQ(Join(ClientId{100}, {.create = true, .code = {}}, "203.0.113.9")
				  .error(),
			  net::RejectReason::Busy);
	EXPECT_TRUE(Join(ClientId{101}, {.create = false, .code = *codes.begin()}))
		<< "one already there";
	EXPECT_TRUE(Join(ClientId{102}, {})) << "the open game";
}

TEST_F(LobbyTest, AnAddressMakesAFewRoomsAtMost) {
	for (std::size_t i = 0; i < Lobby::kRoomsPerAddress; ++i) {
		Create(ClientId{static_cast<std::uint32_t>(i + 1)}, "203.0.113.9");
	}
	EXPECT_EQ(
		Join(ClientId{50}, {.create = true, .code = {}}, "203.0.113.9").error(),
		net::RejectReason::Busy);
	EXPECT_TRUE(
		Join(ClientId{51}, {.create = true, .code = {}}, "203.0.113.10"))
		<< "another address";
}

// Empty, a room closes after a while, its code no one's then; the open game
// stays
TEST_F(LobbyTest, AnEmptyRoomCloses) {
	const std::string code = Create(ClientId{1});
	ASSERT_TRUE(Join(ClientId{2}, {}));
	lobby_->Disconnect(ClientId{1});
	lobby_->Disconnect(ClientId{2});
	const auto ticks =
		static_cast<int>(Lobby::kEmptySeconds / GameServer::kTickSeconds);
	for (int i = 0; i < ticks - 10; ++i) {
		lobby_->Tick();
	}
	EXPECT_NE(lobby_->Find(code), nullptr) << "not yet";
	for (int i = 0; i < 20; ++i) {
		lobby_->Tick();
	}
	EXPECT_EQ(lobby_->Find(code), nullptr);
	EXPECT_NE(lobby_->Find(""), nullptr);
	EXPECT_EQ(lobby_->RoomCount(), 0u);
	EXPECT_EQ(Join(ClientId{3}, {.create = false, .code = code}).error(),
			  net::RejectReason::NoRoom);
}

}  // namespace
}  // namespace karakale
