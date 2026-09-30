/**
 * @file addresses.h
 * @brief How many connections come from each address
 */

#ifndef SERVER_INCLUDE_SERVER_ADDRESSES_H_
#define SERVER_INCLUDE_SERVER_ADDRESSES_H_

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace wolfenstein {

// The connections open from each address, at most kPerAddress: players on
// one network share its address (a home, a school), so several may come
// from one, but no one fills the server alone. An IPv6 address counts by
// its network, its first 64 bits: a home is given a whole network of them.
class Addresses
{
  public:
	static constexpr std::size_t kPerAddress = 8;

	// What a connection counts under: `peer`, the address it came from (4 or
	// 16 bytes, as the socket has it), or, when that is a proxy on this
	// machine or its private network (Caddy, in front), the last address
	// `forwarded` names (its X-Forwarded-For: whom the proxy had the
	// connection from). "" if there is no address: those count together.
	static std::string KeyOf(std::string_view peer, std::string_view forwarded);

	// Whether another connection may come from `key`
	bool Allows(std::string_view key) const;
	// A connection from `key` opened, or closed
	void Open(const std::string& key);
	void Close(std::string_view key);
	std::size_t CountOf(std::string_view key) const;

  private:
	std::map<std::string, std::size_t, std::less<>> open_;
};

}  // namespace wolfenstein

#endif	// SERVER_INCLUDE_SERVER_ADDRESSES_H_
