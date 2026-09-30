/**
 * @file lobby.h
 * @brief The matches one server holds: the open one, and private rooms
 */

#ifndef SERVER_INCLUDE_SERVER_LOBBY_H_
#define SERVER_INCLUDE_SERVER_LOBBY_H_

#include "Server/game_server.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace wolfenstein {

// The matches one server holds: the open one, always there, and private
// rooms, each made as its first player comes and closed a while after its
// last one leaves. A player's game names the room in the address it joins
// ("/room/CODE"); friends who name the same room play together. Each room
// is a GameServer of its own; the lobby hands each connection's messages
// to its room's, and ticks them all.
class Lobby
{
  public:
	// Private rooms held at once, at most
	static constexpr std::size_t kMaxRooms = 16;
	// A room with no one in it this long closes
	static constexpr double kEmptySeconds = 60.0;
	// A room code's characters, at most
	static constexpr std::size_t kMaxCode = 12;
	// Makes a room's match
	using Factory = std::function<
		std::expected<std::unique_ptr<GameServer>, std::string>()>;

	// Makes the open room at once, with `make`; the error says why it
	// could not
	static std::expected<std::unique_ptr<Lobby>, std::string> Create(
		Factory make);
	explicit Lobby(Factory make, std::unique_ptr<GameServer> open);

	// The room an address's path names: "/room/CODE" the code's letters
	// and digits in capitals (at most kMaxCode); anything else, or no code
	// left, "" (the open room)
	static std::string RoomOf(std::string_view path);
	// A connection opened for the room `room`, made if it is new; false if
	// it cannot be (too many rooms, or its match would not start)
	bool Connect(ClientId client, const std::string& room);
	void Receive(ClientId client, std::span<const std::uint8_t> message);
	void Disconnect(ClientId client);
	// Every room a tick on; those empty long enough close
	void Tick();

	// The private rooms open
	std::size_t RoomCount() const { return rooms_.size() - 1; }
	// A room's match, nullptr if there is none: "" the open one
	GameServer* Find(const std::string& room);

  private:
	struct Room
	{
		std::unique_ptr<GameServer> server;
		std::size_t connections = 0;
		double empty_for = 0.0;	 // seconds, while no one is in it
	};

	Factory make_;
	std::map<std::string, Room> rooms_;
	// Each connection's room
	std::map<ClientId, std::string> clients_;
};

}  // namespace wolfenstein

#endif	// SERVER_INCLUDE_SERVER_LOBBY_H_
