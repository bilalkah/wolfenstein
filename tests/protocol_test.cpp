// What the server and its players say to each other: every message comes
// back as it was sent, packed small; positions and angles within a
// packing step; and a message cut short, of an unknown type, claiming more
// than it may or with a field no game writes (a command no game makes, a
// name that is not printable text) is refused whole. A game sends its
// name's printable characters. Packing and reading allocate nothing.

#include "Net/protocol.h"
#include "Profiler/profiler.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <numbers>
#include <variant>

namespace karakale::net {
namespace {

// The message as it comes out at the other end
template <typename Body>
Body RoundTrip(const Body& body, std::size_t* size = nullptr) {
	std::array<std::uint8_t, kMaxMessage> buffer{};
	const std::size_t written = Encode(body, buffer);
	EXPECT_GT(written, 0u);
	if (size != nullptr) {
		*size = written;
	}
	const auto decoded = Decode(std::span(buffer).first(written));
	EXPECT_TRUE(decoded.has_value());
	EXPECT_TRUE(decoded && std::holds_alternative<Body>(*decoded));
	return decoded && std::holds_alternative<Body>(*decoded)
			   ? std::get<Body>(*decoded)
			   : Body{};
}

PlayerCommand AnyCommand() {
	return {.forward = 1,
			.strafe = -1,
			.fire = true,
			.use = true,
			.weapon = 3,
			.cycle = -1,
			.has_view = true,
			.view_theta = 1.25,
			.view_pitch = -0.2};
}

TEST(Protocol, HelloWelcomeAndRejectComeBackAsSent) {
	const Hello hello{.name = PlayerName("bilal")};
	EXPECT_EQ(RoundTrip(hello), hello);
	const Welcome welcome{
		.slot = 5, .tick = 123456, .level = LevelName("a.json")};
	EXPECT_EQ(RoundTrip(welcome), welcome);
	const Reject reject{.reason = RejectReason::Version};
	EXPECT_EQ(RoundTrip(reject), reject);
}

// A command's buttons and moves as they were, its view within a packing
// step; commands numbered one after another from the first
TEST(Protocol, AnInputCarriesItsCommandsInOrder) {
	Input input{.count = 3};
	for (std::uint32_t i = 0; i < 3; ++i) {
		input.commands[i] = {.sequence = 41 + i, .command = AnyCommand()};
	}
	input.commands[1].command.fire = false;
	std::size_t size = 0;
	const Input back = RoundTrip(input, &size);
	EXPECT_LE(size, 40u) << "small enough to send 60 times a second";
	ASSERT_EQ(back.count, 3u);
	for (std::size_t i = 0; i < 3; ++i) {
		const PlayerCommand& sent = input.commands[i].command;
		const PlayerCommand& got = back.commands[i].command;
		EXPECT_EQ(back.commands[i].sequence, 41 + i);
		EXPECT_EQ(got.forward, sent.forward);
		EXPECT_EQ(got.strafe, sent.strafe);
		EXPECT_EQ(got.fire, sent.fire);
		EXPECT_EQ(got.use, sent.use);
		EXPECT_EQ(got.reload, sent.reload);
		EXPECT_EQ(got.weapon, sent.weapon);
		EXPECT_EQ(got.cycle, sent.cycle);
		EXPECT_TRUE(got.has_view);
		EXPECT_NEAR(got.view_theta, sent.view_theta, 1e-4);
		EXPECT_NEAR(got.view_pitch, sent.view_pitch, 1e-4);
	}
}

// Every player of a full game, what the receiver carries and which of a
// level's pickups are gone, in a message under 140 bytes long
TEST(Protocol, ASnapshotCarriesEveryPlayer) {
	Snapshot snapshot{.tick = 9000,
					  .ack = 77,
					  .count = kMaxPlayers,
					  .own = {.owned = 0b1011, .count = kMaxWeapons},
					  .pickups = kMaxPickups,
					  .taken = (std::uint64_t{1} << 63U) | 5U};
	for (std::size_t i = 0; i < kMaxWeapons; ++i) {
		snapshot.own.rounds[i] = {
			.ammo = static_cast<std::uint8_t>(i),
			.reserve = static_cast<std::uint16_t>(300 + i)};
	}
	for (std::size_t i = 0; i < kMaxPlayers; ++i) {
		snapshot.players[i] = {.slot = static_cast<std::uint8_t>(i),
							   .alive = i % 3 != 0,
							   .shielded = i % 2 == 0,
							   .pose = {1.5 + static_cast<double>(i), 37.123},
							   .theta = -2.0 + static_cast<double>(i),
							   .pitch = 0.1,
							   .health = static_cast<std::uint8_t>(10 * i),
							   .weapon = static_cast<std::uint8_t>(i % 7)};
	}
	std::size_t size = 0;
	const Snapshot back = RoundTrip(snapshot, &size);
	EXPECT_LE(size, 140u);
	EXPECT_EQ(back.tick, 9000u);
	EXPECT_EQ(back.ack, 77u);
	EXPECT_EQ(back.own, snapshot.own);
	EXPECT_EQ(back.pickups, kMaxPickups);
	EXPECT_EQ(back.taken, snapshot.taken);
	ASSERT_EQ(back.count, kMaxPlayers);
	for (std::size_t i = 0; i < kMaxPlayers; ++i) {
		const PlayerState& sent = snapshot.players[i];
		const PlayerState& got = back.players[i];
		EXPECT_EQ(got.slot, sent.slot);
		EXPECT_EQ(got.alive, sent.alive);
		EXPECT_EQ(got.shielded, sent.shielded);
		EXPECT_EQ(got.pose.x, QuantisePosition(sent.pose.x));
		EXPECT_EQ(got.pose.y, QuantisePosition(sent.pose.y));
		EXPECT_EQ(got.theta, QuantiseAngle(sent.theta));
		EXPECT_EQ(got.pitch, QuantisePitch(sent.pitch));
		EXPECT_EQ(got.health, sent.health);
		EXPECT_EQ(got.weapon, sent.weapon);
	}
}

// A tick's events in order, each as it happened, positions and angles
// within a packing step
TEST(Protocol, EventsComeBackInOrder) {
	Events events{.tick = 321, .count = kMaxEvents};
	for (std::size_t i = 0; i < kMaxEvents; ++i) {
		events.events[i] = {.type = static_cast<EventType>(i % 5),
							.slot = static_cast<std::uint8_t>(i % kMaxPlayers),
							.other = static_cast<std::uint8_t>((i + 1) % 8),
							.weapon = static_cast<std::uint8_t>(i % 7),
							.pose = {2.0 + static_cast<double>(i) * 0.3, 5.25},
							.theta = 0.1 * static_cast<double>(i)};
	}
	std::size_t size = 0;
	const Events back = RoundTrip(events, &size);
	EXPECT_LE(size, kMaxMessage);
	EXPECT_EQ(back.tick, 321u);
	ASSERT_EQ(back.count, kMaxEvents);
	for (std::size_t i = 0; i < kMaxEvents; ++i) {
		const Event& sent = events.events[i];
		const Event& got = back.events[i];
		EXPECT_EQ(got.type, sent.type);
		EXPECT_EQ(got.slot, sent.slot);
		EXPECT_EQ(got.other, sent.other);
		EXPECT_EQ(got.weapon, sent.weapon);
		EXPECT_EQ(got.pose.x, QuantisePosition(sent.pose.x));
		EXPECT_EQ(got.pose.y, QuantisePosition(sent.pose.y));
		EXPECT_EQ(got.theta, QuantiseAngle(sent.theta));
	}
}

TEST(Protocol, ScoresComeBackAsSent) {
	Scores scores{.mode = MatchMode::GunRace,
				  .phase = MatchPhase::Intermission,
				  .frag_limit = 25,
				  .seconds_left = 599,
				  .winner = 3,
				  .count = kMaxPlayers};
	for (std::size_t i = 0; i < kMaxPlayers; ++i) {
		scores.players[i] = {
			.slot = static_cast<std::uint8_t>(i),
			.name = PlayerName("sixteen letters!"),
			.frags = static_cast<std::int16_t>(static_cast<int>(i) - 2),
			.deaths = static_cast<std::uint16_t>(i * 3),
			.step = static_cast<std::uint8_t>(i)};
	}
	std::size_t size = 0;
	EXPECT_EQ(RoundTrip(scores, &size), scores);
	EXPECT_LE(size, 200u);
}

TEST(Protocol, PingsComeBackAsSent) {
	const Ping ping{.stamp = 0xFFFFFFF0, .rtt = 43};
	EXPECT_EQ(RoundTrip(ping), ping);
	const Pong pong{.stamp = 7};
	EXPECT_EQ(RoundTrip(pong), pong);
	Pings pings{.count = kMaxPlayers};
	for (std::size_t i = 0; i < kMaxPlayers; ++i) {
		pings.players[i] = {.slot = static_cast<std::uint8_t>(i),
							.rtt = static_cast<std::uint16_t>(20 * i)};
	}
	EXPECT_EQ(RoundTrip(pings), pings);
}

// Within half a packing step: a 512th of a cell, a 131072nd of a turn; an
// angle comes back the same way round, whatever turn it was given in
TEST(Protocol, PackingStaysWithinAStep) {
	for (const double x : {0.0, 0.3, 17.999, 63.5, 200.25}) {
		EXPECT_NEAR(QuantisePosition(x), x, 1.0 / 512 + 1e-12) << x;
	}
	EXPECT_EQ(QuantisePosition(-3.0), 0.0) << "clamped to the level";
	constexpr double kTurn = 2.0 * std::numbers::pi;
	for (const double theta : {0.0, 1.0, -1.0, 7.0, -7.0}) {
		const double back = QuantiseAngle(theta);
		EXPECT_NEAR(std::remainder(back - theta, kTurn), 0.0,
					kTurn / 131072 + 1e-12)
			<< theta;
	}
	EXPECT_NEAR(QuantisePitch(0.4), 0.4, 1e-4);
	EXPECT_NEAR(QuantisePitch(2.0), 0.5, 1e-4) << "clamped";
}

TEST(Protocol, ANameLongerThanRoomIsCut) {
	const PlayerName name("a name that does not fit in sixteen");
	EXPECT_EQ(name.View(), "a name that does");
}

// Every part of a message cut off is refused, and so is a byte too many
TEST(Protocol, AMessageCutShortOrTooLongIsRefused) {
	Snapshot snapshot{.tick = 1, .ack = 1, .count = 2};
	std::array<std::uint8_t, kMaxMessage> buffer{};
	const std::size_t size = Encode(snapshot, buffer);
	ASSERT_GT(size, 0u);
	for (std::size_t cut = 0; cut < size; ++cut) {
		EXPECT_FALSE(Decode(std::span(buffer).first(cut))) << cut;
	}
	EXPECT_FALSE(Decode(std::span(buffer).first(size + 1)));
}

TEST(Protocol, WhatCannotBeRightIsRefused) {
	// An unknown type
	EXPECT_FALSE(Decode(std::array<std::uint8_t, 1>{0}));
	EXPECT_FALSE(Decode(std::array<std::uint8_t, 1>{99}));
	// More players than a game holds
	std::array<std::uint8_t, kMaxMessage> buffer{};
	Snapshot snapshot{.count = 1};
	std::size_t size = Encode(snapshot, buffer);
	buffer[9] = kMaxPlayers + 1;  // after type, tick and ack: the count
	EXPECT_FALSE(Decode(std::span(buffer).first(size)));
	// A slot past the last
	buffer[9] = 1;
	buffer[10] = kMaxPlayers;
	EXPECT_FALSE(Decode(std::span(buffer).first(size)));
	// More commands than an input carries
	size = Encode(
		Input{.count = 1, .commands = {{{.sequence = 1, .command = {}}}}},
		buffer);
	buffer[1] = kInputCommands + 1;
	EXPECT_FALSE(Decode(std::span(buffer).first(size)));
	// A name claiming more than a name holds
	size = Encode(Hello{.name = PlayerName("x")}, buffer);
	buffer[3] = PlayerName::kCapacity + 1;
	EXPECT_FALSE(Decode(std::span(buffer).first(size)));
	// No such reason
	size = Encode(Reject{}, buffer);
	buffer[1] = 0;
	EXPECT_FALSE(Decode(std::span(buffer).first(size)));
	// No such event, or one by a slot past the last
	size = Encode(Events{.count = 1}, buffer);
	buffer[6] = 99;	 // after type, tick and count: the event's type
	EXPECT_FALSE(Decode(std::span(buffer).first(size)));
	buffer[6] = 0;
	buffer[7] = kMaxPlayers;
	EXPECT_FALSE(Decode(std::span(buffer).first(size)));
	// A ping of a slot past the last
	size = Encode(Pings{.count = 1}, buffer);
	buffer[2] = kMaxPlayers;  // after type and count: the slot
	EXPECT_FALSE(Decode(std::span(buffer).first(size)));
	// A winner past the last slot
	size = Encode(Scores{}, buffer);
	buffer[6] = kMaxPlayers;  // after type, mode, phase, limit and clock
	EXPECT_FALSE(Decode(std::span(buffer).first(size)));
}

// Keys held -1, 0 or 1, the buttons a game has, a weapon it could carry or
// none, a pitch as it is packed: anything else, no game made
TEST(Protocol, ACommandNoGameMakesIsRefused) {
	std::array<std::uint8_t, kMaxMessage> buffer{};
	const std::size_t size =
		Encode(Input{.count = 1,
					 .commands = {{{.sequence = 1, .command = AnyCommand()}}}},
			   buffer);
	ASSERT_TRUE(Decode(std::span(buffer).first(size)));
	// After type, count, first number and tick seen: the command, its keys,
	// buttons, weapon, wheel, then its view
	constexpr std::size_t kForward = 10;
	constexpr std::size_t kStrafe = 11;
	constexpr std::size_t kTurn = 12;
	constexpr std::size_t kButtons = 13;
	constexpr std::size_t kWeapon = 14;
	constexpr std::size_t kCycle = 15;
	constexpr std::size_t kPitch = 18;
	const auto read = [&](std::size_t at, std::uint8_t value) {
		auto changed = buffer;
		changed[at] = value;
		return Decode(std::span(changed).first(size)).has_value();
	};
	EXPECT_FALSE(read(kForward, 2));
	EXPECT_FALSE(read(kStrafe, 0xFE)) << "-2";
	EXPECT_FALSE(read(kTurn, 0x80));
	EXPECT_FALSE(read(kButtons, 0x10)) << "a button no game has";
	EXPECT_FALSE(read(kWeapon, kMaxWeapons));
	EXPECT_FALSE(read(kWeapon, 0xFE)) << "-2";
	EXPECT_FALSE(read(kCycle, 2));
	auto pitch = buffer;
	pitch[kPitch] = 0x00;
	pitch[kPitch + 1] = 0x80;  // -32768, a step past the lowest
	EXPECT_FALSE(Decode(std::span(pitch).first(size)));
	// What a game does make
	EXPECT_TRUE(read(kForward, 0xFF)) << "backwards";
	EXPECT_TRUE(read(kWeapon, 0xFF)) << "no weapon chosen";
	EXPECT_TRUE(read(kWeapon, kMaxWeapons - 1));
	EXPECT_TRUE(read(kButtons, 0x0F)) << "every button";
}

// An input carries a command at least, numbered from 1, the last of them
// with a number
TEST(Protocol, AnInputIsNumberedFromOne) {
	std::array<std::uint8_t, kMaxMessage> buffer{};
	std::size_t size = Encode(Input{}, buffer);
	EXPECT_FALSE(Decode(std::span(buffer).first(size))) << "no command";
	size = Encode(
		Input{.count = 1, .commands = {{{.sequence = 0, .command = {}}}}},
		buffer);
	EXPECT_FALSE(Decode(std::span(buffer).first(size))) << "numbered 0";
	Input past{.count = 2};
	past.commands[0].sequence = 0xFFFFFFFF;
	size = Encode(past, buffer);
	EXPECT_FALSE(Decode(std::span(buffer).first(size))) << "past the last";
	past.commands[0].sequence = 0xFFFFFFFE;
	size = Encode(past, buffer);
	EXPECT_TRUE(Decode(std::span(buffer).first(size))) << "up to the last";
}

// A name is printable text, at least a character: none, a control
// character or a byte past ASCII (what the font does not draw) is refused
TEST(Protocol, ANameIsPrintableText) {
	std::array<std::uint8_t, kMaxMessage> buffer{};
	std::size_t size = Encode(Hello{}, buffer);
	EXPECT_FALSE(Decode(std::span(buffer).first(size))) << "no name";
	size = Encode(Hello{.name = PlayerName("ann")}, buffer);
	ASSERT_TRUE(Decode(std::span(buffer).first(size)));
	constexpr std::size_t kFirstLetter = 4;	 // after type, version, length
	for (const int letter : {0x00, 0x07, 0x7F, 0xC5}) {
		auto changed = buffer;
		changed[kFirstLetter] = static_cast<std::uint8_t>(letter);
		EXPECT_FALSE(Decode(std::span(changed).first(size))) << letter;
	}
	// A score's name likewise
	Scores scores{.count = 1};
	scores.players[0].name = PlayerName("a\tb");
	size = Encode(scores, buffer);
	EXPECT_FALSE(Decode(std::span(buffer).first(size)));
}

// A game sends what the server takes: the printable characters, cut to
// fit; a name with none is PLAYER
TEST(Protocol, AGameSendsItsNamesPrintableCharacters) {
	EXPECT_EQ(MakeName("ann").View(), "ann");
	EXPECT_EQ(MakeName("Bilal \xC5\x9E!").View(), "Bilal !");
	EXPECT_EQ(MakeName("\t\xC5\x9E\n").View(), "PLAYER");
	EXPECT_EQ(MakeName("").View(), "PLAYER");
	EXPECT_EQ(MakeName("a name that does not fit in sixteen").View(),
			  "a name that does");
}

TEST(Protocol, AMessageTooBigForItsBufferIsNotWritten) {
	std::array<std::uint8_t, 8> small{};
	EXPECT_EQ(Encode(Snapshot{.count = kMaxPlayers}, small), 0u);
}

#ifdef KARAKALE_COUNTS_ALLOCATIONS
TEST(Protocol, PackingAndReadingAllocateNothing) {
	std::array<std::uint8_t, kMaxMessage> buffer{};
	const auto before = AllocationStats::count;
	for (int i = 0; i < 100; ++i) {
		const std::size_t size =
			Encode(Snapshot{.tick = static_cast<std::uint32_t>(i),
							.count = kMaxPlayers},
				   buffer);
		EXPECT_TRUE(Decode(std::span(buffer).first(size)));
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

}  // namespace
}  // namespace karakale::net
