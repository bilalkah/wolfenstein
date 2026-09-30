#include "Server/lobby.h"
#include <algorithm>
#include <cctype>
#include <utility>

namespace karakale {

std::expected<std::unique_ptr<Lobby>, std::string> Lobby::Create(Factory make) {
	auto open = make();
	if (!open) {
		return std::unexpected(open.error());
	}
	return std::make_unique<Lobby>(std::move(make), std::move(*open));
}

Lobby::Lobby(Factory make, std::unique_ptr<GameServer> open)
	: make_(std::move(make)) {
	rooms_[""] = Room{.server = std::move(open), .made_from = {}};
}

Lobby::Request Lobby::RequestOf(std::string_view path) {
	constexpr std::string_view kCreate = "/create";
	constexpr std::string_view kRooms = "/room/";
	if (path == kCreate) {
		return {.create = true, .code = {}};
	}
	if (!path.starts_with(kRooms)) {
		return {};
	}
	std::string code;
	for (const char c : path.substr(kRooms.size())) {
		if (code.size() == net::RoomCode::kCapacity) {
			break;
		}
		if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
			code.push_back(
				static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
		}
	}
	return {.create = false, .code = code};
}

std::expected<std::string, net::RejectReason> Lobby::Connect(
	ClientId client, const Request& request, std::string_view address) {
	if (request.create) {
		return Make(client, address);
	}
	const auto found = rooms_.find(request.code);
	if (found == rooms_.end()) {
		return std::unexpected(net::RejectReason::NoRoom);
	}
	Room& room = found->second;
	++room.connections;
	room.empty_for = 0.0;
	clients_[client] = request.code;
	room.server->Connect(client);
	return request.code;
}

std::expected<std::string, net::RejectReason> Lobby::Make(
	ClientId client, std::string_view address) {
	const auto made_there =
		std::ranges::count_if(rooms_, [&](const auto& room) {
			return !room.first.empty() && room.second.made_from == address;
		});
	if (RoomCount() >= kMaxRooms ||
		std::cmp_greater_equal(made_there, kRoomsPerAddress)) {
		return std::unexpected(net::RejectReason::Busy);
	}
	// A code no room has: a few tries at most, there being millions
	std::uniform_int_distribution<std::size_t> letter(0,
													  kCodeLetters.size() - 1);
	std::string code;
	do {
		code.clear();
		for (std::size_t i = 0; i < kCodeLength; ++i) {
			code.push_back(kCodeLetters[letter(random_)]);
		}
	} while (rooms_.contains(code));
	auto made = make_();
	if (!made) {
		return std::unexpected(net::RejectReason::Busy);
	}
	(*made)->MakeRoom(net::RoomCode(code), client);
	Room& room = rooms_
					 .emplace(code, Room{.server = std::move(*made),
										 .made_from = std::string(address),
										 .connections = 1})
					 .first->second;
	clients_[client] = code;
	room.server->Connect(client);
	return code;
}

void Lobby::Receive(ClientId client, std::span<const std::uint8_t> message) {
	const auto found = clients_.find(client);
	if (found == clients_.end()) {
		return;
	}
	if (GameServer* server = Find(found->second)) {
		server->Receive(client, message);
	}
}

void Lobby::Disconnect(ClientId client) {
	const auto found = clients_.find(client);
	if (found == clients_.end()) {
		return;
	}
	if (const auto room = rooms_.find(found->second); room != rooms_.end()) {
		room->second.server->Disconnect(client);
		--room->second.connections;
	}
	clients_.erase(found);
}

void Lobby::Tick() {
	for (auto room = rooms_.begin(); room != rooms_.end();) {
		Room& held = room->second;
		held.server->Tick();
		if (held.connections == 0 && !room->first.empty()) {
			held.empty_for += GameServer::kTickSeconds;
			if (held.empty_for >= kEmptySeconds) {
				room = rooms_.erase(room);
				continue;
			}
		}
		++room;
	}
}

GameServer* Lobby::Find(const std::string& room) {
	const auto found = rooms_.find(room);
	return found != rooms_.end() ? found->second.server.get() : nullptr;
}

}  // namespace karakale
