// Where a game joins a match: the server's address, with the room's on it
// when one is named, its code in capitals, anything but letters and digits
// left out

#include "Core/game.h"
#include <gtest/gtest.h>

namespace karakale {
namespace {

TEST(MatchUrl, NoRoomIsTheServersOpenGame) {
	EXPECT_EQ(MatchUrl("ws://localhost:8080", ""), "ws://localhost:8080");
	EXPECT_EQ(MatchUrl("wss://play.kergit.com/", ""), "wss://play.kergit.com");
}

TEST(MatchUrl, ARoomIsItsCodeInCapitals) {
	EXPECT_EQ(MatchUrl("wss://play.kergit.com", "friday"),
			  "wss://play.kergit.com/room/FRIDAY");
	EXPECT_EQ(MatchUrl("ws://10.0.0.2:8080//", " b-2 7!"),
			  "ws://10.0.0.2:8080/room/B27");
	EXPECT_EQ(MatchUrl("ws://host:1", "-!-"), "ws://host:1") << "no code left";
}

}  // namespace
}  // namespace karakale
