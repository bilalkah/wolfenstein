#include "Server/addresses.h"
#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <sys/socket.h>

namespace wolfenstein {

namespace {

// An address as IPv6 has it, an IPv4 one mapped in (::ffff:a.b.c.d)
using Bytes = std::array<std::uint8_t, 16>;
constexpr std::array<std::uint8_t, 12> kMapped{0, 0, 0, 0, 0,	 0,
											   0, 0, 0, 0, 0xFF, 0xFF};
constexpr std::size_t kIpv4At = kMapped.size();

bool IsIpv4(const Bytes& address) {
	return std::ranges::equal(kMapped, std::span(address).first(kIpv4At));
}

// As the socket has it: 4 bytes, or 16
std::optional<Bytes> FromSocket(std::string_view peer) {
	const auto byte = [](char c) {
		return static_cast<std::uint8_t>(c);
	};
	Bytes address{};
	if (peer.size() == address.size() - kIpv4At) {
		std::ranges::copy(kMapped, address.begin());
		std::ranges::transform(peer, address.begin() + kIpv4At, byte);
		return address;
	}
	if (peer.size() == address.size()) {
		std::ranges::transform(peer, address.begin(), byte);
		return address;
	}
	return std::nullopt;
}

// As text: "203.0.113.5", "2001:db8::1"
std::optional<Bytes> FromText(std::string_view text) {
	const auto first = text.find_first_not_of(' ');
	if (first == std::string_view::npos) {
		return std::nullopt;
	}
	text = text.substr(first, text.find_last_not_of(' ') - first + 1);
	const std::string terminated(text);
	Bytes address{};
	if (inet_pton(AF_INET, terminated.c_str(), address.data() + kIpv4At) == 1) {
		std::ranges::copy(kMapped, address.begin());
		return address;
	}
	if (inet_pton(AF_INET6, terminated.c_str(), address.data()) == 1) {
		return address;
	}
	return std::nullopt;
}

// On this machine, or on its private network: a proxy of its own
bool IsLocal(const Bytes& address) {
	if (IsIpv4(address)) {
		const std::uint8_t a = address[kIpv4At];
		const std::uint8_t b = address[kIpv4At + 1];
		return a == 127 || a == 10 || (a == 172 && (b & 0xF0U) == 16) ||
			   (a == 192 && b == 168);
	}
	constexpr Bytes kLoopback{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
	const bool unique_local = (address[0] & 0xFEU) == 0xFC;	 // fc00::/7
	const bool link_local =
		address[0] == 0xFE && (address[1] & 0xC0U) == 0x80;	 // fe80::/10
	return address == kLoopback || unique_local || link_local;
}

// An IPv4 address itself; an IPv6 one's network
std::string Key(Bytes address) {
	std::array<char, INET6_ADDRSTRLEN> text{};
	if (IsIpv4(address)) {
		inet_ntop(AF_INET, address.data() + kIpv4At, text.data(), text.size());
		return text.data();
	}
	// Its network: the first 64 bits
	std::ranges::fill(std::span(address).subspan(8), 0);
	inet_ntop(AF_INET6, address.data(), text.data(), text.size());
	return std::string(text.data()) + "/64";
}

}  // namespace

std::string Addresses::KeyOf(std::string_view peer,
							 std::string_view forwarded) {
	const auto from = FromSocket(peer);
	if (!from) {
		return {};
	}
	// A proxy's own: the address it names last is whom it had the
	// connection from
	if (IsLocal(*from)) {
		const auto comma = forwarded.rfind(',');
		const auto last = comma == std::string_view::npos
							  ? forwarded
							  : forwarded.substr(comma + 1);
		if (const auto client = FromText(last)) {
			return Key(*client);
		}
	}
	return Key(*from);
}

bool Addresses::Allows(std::string_view key) const {
	return CountOf(key) < kPerAddress;
}

void Addresses::Open(const std::string& key) {
	++open_[key];
}

void Addresses::Close(std::string_view key) {
	const auto found = open_.find(key);
	if (found != open_.end() && --found->second == 0) {
		open_.erase(found);
	}
}

std::size_t Addresses::CountOf(std::string_view key) const {
	const auto found = open_.find(key);
	return found != open_.end() ? found->second : 0;
}

}  // namespace wolfenstein
