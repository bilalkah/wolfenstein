// Where a game joins a match: the server's address, with the room's on it
// when one is named, its code in capitals, anything but letters and digits
// left out; the address that makes a room; and the link that brings a
// friend into one

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

TEST(MatchUrl, ARoomIsMadeAtCreate) {
	EXPECT_EQ(CreateUrl("wss://play.kergit.com/"),
			  "wss://play.kergit.com/create");
}

TEST(MatchUrl, AnInviteNamesTheServerAndTheRoom) {
	EXPECT_EQ(InviteLink("https://bilalkah.github.io/karakale/play/",
						 "wss://abcd-443.euw.devtunnels.ms", "K7QX2"),
			  "https://bilalkah.github.io/karakale/play/"
			  "?server=wss%3A%2F%2Fabcd-443.euw.devtunnels.ms&room=K7QX2");
	EXPECT_EQ(
		InviteLink("http://10.0.0.2:8000/", "ws://10.0.0.2:8080", "AB"),
		"http://10.0.0.2:8000/?server=ws%3A%2F%2F10.0.0.2%3A8080&room=AB");
}

}  // namespace
}  // namespace karakale
