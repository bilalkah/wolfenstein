/**
 * @file connection.h
 * @brief A player's game's connection to the server
 */

#ifndef NET_INCLUDE_NET_CONNECTION_H_
#define NET_INCLUDE_NET_CONNECTION_H_

#include "Net/protocol.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>

namespace wolfenstein::net {

// A message taken in: its length, and when it came (seconds, by
// Connection::Now), which a frame taking it later does not change
struct Received
{
	std::size_t size = 0;
	double arrived = 0.0;
};

// Messages come in on the transport's own time (natively, on a thread of
// its own) and wait here, each copied into a slot of its own with when it
// came, until the game takes them, oldest first. Nothing is allocated;
// with every slot full, what comes is dropped (the game takes them every
// frame).
class Inbox
{
  public:
	static constexpr std::size_t kSlots = 64;

	// False if it was dropped: too long, or no slot free
	bool Put(std::span<const std::uint8_t> message, double arrived);
	// The oldest message waiting, copied into `out`; nothing if none
	std::optional<Received> Take(std::span<std::uint8_t> out);

  private:
	std::mutex mutex_;
	std::array<std::array<std::uint8_t, kMaxMessage>, kSlots> slots_{};
	std::array<std::size_t, kSlots> sizes_{};
	std::array<double, kSlots> arrived_{};
	std::size_t head_ = 0;
	std::size_t count_ = 0;
};

// A connection to the game's server, a WebSocket: in the browser its own,
// natively IXWebSocket's. It opens as it is made and never reopens by
// itself.
class Connection
{
  public:
	enum class State : std::uint8_t { Connecting, Open, Closed };

	// Connects to `url` ("ws://host:port"); nullptr if it cannot even try
	static std::unique_ptr<Connection> Open(const std::string& url);
	// The clock messages are stamped with as they come: seconds, steady,
	// from any start
	static double Now();
	virtual ~Connection() = default;
	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;
	Connection(Connection&&) = delete;
	Connection& operator=(Connection&&) = delete;

	virtual State GetState() const = 0;
	// One whole message; false if it could not be sent
	virtual bool Send(std::span<const std::uint8_t> message) = 0;
	// The oldest message come in, copied into `out`; nothing if none
	std::optional<Received> Receive(std::span<std::uint8_t> out) {
		return inbox_.Take(out);
	}

  protected:
	Connection() = default;
	Inbox inbox_;
};

}  // namespace wolfenstein::net

#endif	// NET_INCLUDE_NET_CONNECTION_H_
