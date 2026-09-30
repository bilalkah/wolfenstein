// How many connections come from each address: an IPv4 address counts as
// itself, an IPv6 one by its network; behind a proxy of the server's own,
// the address it names; at most eight from one, and one closed makes room

#include "Server/addresses.h"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace karakale {
namespace {

// An address as the socket has it: its bytes
template <std::size_t N>
std::string Peer(const std::array<std::uint8_t, N>& bytes) {
	return {bytes.begin(), bytes.end()};
}
const std::string kPublic = Peer(std::array<std::uint8_t, 4>{203, 0, 113, 5});
const std::string kProxy = Peer(std::array<std::uint8_t, 4>{172, 18, 0, 3});

TEST(Addresses, AnIpv4AddressCountsAsItself) {
	EXPECT_EQ(Addresses::KeyOf(kPublic, ""), "203.0.113.5");
	// The same, as a socket listening for both kinds has it
	const std::string mapped = Peer(std::array<std::uint8_t, 16>{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 203, 0, 113, 5});
	EXPECT_EQ(Addresses::KeyOf(mapped, ""), "203.0.113.5");
	EXPECT_EQ(Addresses::KeyOf("", ""), "") << "no address";
}

TEST(Addresses, AnIpv6AddressCountsByItsNetwork) {
	const std::string one = Peer(std::array<std::uint8_t, 16>{
		0x20, 0x01, 0x0d, 0xb8, 0, 1, 0, 2, 0xAA, 0xAA, 0, 0, 0, 0, 0, 1});
	const std::string two = Peer(std::array<std::uint8_t, 16>{
		0x20, 0x01, 0x0d, 0xb8, 0, 1, 0, 2, 0xBB, 0xBB, 0, 0, 0, 0, 0, 2});
	const std::string elsewhere = Peer(std::array<std::uint8_t, 16>{
		0x20, 0x01, 0x0d, 0xb8, 0, 1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1});
	EXPECT_EQ(Addresses::KeyOf(one, ""), "2001:db8:1:2::/64");
	EXPECT_EQ(Addresses::KeyOf(two, ""), "2001:db8:1:2::/64");
	EXPECT_EQ(Addresses::KeyOf(elsewhere, ""), "2001:db8:1:3::/64");
}

// A proxy on the server's own machine or network is believed about whom it
// had the connection from: the last address it names. Anyone else is not:
// they could name any address.
TEST(Addresses, OnlyTheServersOwnProxyIsBelieved) {
	EXPECT_EQ(Addresses::KeyOf(kProxy, "198.51.100.7"), "198.51.100.7");
	EXPECT_EQ(Addresses::KeyOf(kProxy, "10.0.0.1, 198.51.100.7"),
			  "198.51.100.7");
	EXPECT_EQ(Addresses::KeyOf(kProxy, "2001:db8::1"), "2001:db8::/64");
	EXPECT_EQ(Addresses::KeyOf(kProxy, "not an address"), "172.18.0.3");
	EXPECT_EQ(Addresses::KeyOf(kProxy, ""), "172.18.0.3");
	const std::string loopback =
		Peer(std::array<std::uint8_t, 4>{127, 0, 0, 1});
	EXPECT_EQ(Addresses::KeyOf(loopback, "198.51.100.7"), "198.51.100.7");
	EXPECT_EQ(Addresses::KeyOf(kPublic, "198.51.100.7"), "203.0.113.5");
}

TEST(Addresses, EightComeFromOneAddressAndNoMore) {
	Addresses addresses;
	for (std::size_t i = 0; i < Addresses::kPerAddress; ++i) {
		EXPECT_TRUE(addresses.Allows("203.0.113.5"));
		addresses.Open("203.0.113.5");
	}
	EXPECT_FALSE(addresses.Allows("203.0.113.5"));
	EXPECT_TRUE(addresses.Allows("203.0.113.6")) << "another is its own";
	addresses.Close("203.0.113.5");
	EXPECT_TRUE(addresses.Allows("203.0.113.5")) << "one closed makes room";
	for (std::size_t i = 1; i < Addresses::kPerAddress; ++i) {
		addresses.Close("203.0.113.5");
	}
	EXPECT_EQ(addresses.CountOf("203.0.113.5"), 0u);
	addresses.Close("203.0.113.5");
	EXPECT_EQ(addresses.CountOf("203.0.113.5"), 0u) << "never below none";
}

}  // namespace
}  // namespace karakale
