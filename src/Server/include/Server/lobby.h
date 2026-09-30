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
#include <random>
#include <span>
#include <string>
#include <string_view>

namespace karakale {

// The matches one server holds: the open one, always there, and private
// rooms. A player's game asks, by the address it joins, for a new room
// ("/create"), which the lobby makes with a code of its own and the player
// hosts, or for a room already made ("/room/CODE"), which friends join by
// the code the host passes on. A room closes a while after its last player
// leaves. Each room is a GameServer of its own; the lobby hands each
// connection's messages to its room's, and ticks them all.
class Lobby
{
  public:
	// Private rooms held at once, at most, and made from one address
	static constexpr std::size_t kMaxRooms = 16;
	static constexpr std::size_t kRoomsPerAddress = 2;
	// A room with no one in it this long closes
	static constexpr double kEmptySeconds = 60.0;
	// A room code: this many characters from kCodeLetters, which leave out
	// those read one for another (0 and O, 1, I and L)
	static constexpr std::size_t kCodeLength = 5;
	static constexpr std::string_view kCodeLetters =
		"ABCDEFGHJKMNPQRSTUVWXYZ23456789";
	// What a connection asks for: a new room, the room of a code, or (no
	// code) the open game
	struct Request
	{
		bool create = false;
		std::string code;
	};
	// Makes a room's match
	using Factory = std::function<
		std::expected<std::unique_ptr<GameServer>, std::string>()>;

	// Makes the open room at once, with `make`; the error says why it
	// could not
	static std::expected<std::unique_ptr<Lobby>, std::string> Create(
		Factory make);
	explicit Lobby(Factory make, std::unique_ptr<GameServer> open);

	// What an address's path asks for: "/create" a new room, "/room/CODE"
	// the room of the code's letters and digits, in capitals (at most
	// net::RoomCode's); anything else, or no code left, the open game
	static Request RequestOf(std::string_view path);
	// A connection opened for `request` from `address` (Addresses::KeyOf);
	// the code of the room it is in ("" the open game), or why not: no room
	// has the code (NoRoom), or none can be made (Busy: too many, too many
	// from the address, or its match would not start)
	std::expected<std::string, net::RejectReason> Connect(
		ClientId client, const Request& request, std::string_view address = {});
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
		std::string made_from;	// the address that asked for it
		std::size_t connections = 0;
		double empty_for = 0.0;	 // seconds, while no one is in it
	};

	// A room made for `client`, under a code no room has
	std::expected<std::string, net::RejectReason> Make(
		ClientId client, std::string_view address);

	Factory make_;
	std::mt19937 random_{std::random_device{}()};
	std::map<std::string, Room> rooms_;
	// Each connection's room
	std::map<ClientId, std::string> clients_;
};

}  // namespace karakale

#endif	// SERVER_INCLUDE_SERVER_LOBBY_H_
