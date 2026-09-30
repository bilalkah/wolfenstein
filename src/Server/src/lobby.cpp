#include "Server/lobby.h"
#include <cctype>
#include <utility>

namespace wolfenstein {

std::expected<std::unique_ptr<Lobby>, std::string> Lobby::Create(Factory make) {
	auto open = make();
	if (!open) {
		return std::unexpected(open.error());
	}
	return std::make_unique<Lobby>(std::move(make), std::move(*open));
}

Lobby::Lobby(Factory make, std::unique_ptr<GameServer> open)
	: make_(std::move(make)) {
	rooms_[""] = Room{.server = std::move(open)};
}

std::string Lobby::RoomOf(std::string_view path) {
	constexpr std::string_view kRooms = "/room/";
	if (!path.starts_with(kRooms)) {
		return {};
	}
	std::string code;
	for (const char c : path.substr(kRooms.size())) {
		if (code.size() == kMaxCode) {
			break;
		}
		if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
			code.push_back(
				static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
		}
	}
	return code;
}

bool Lobby::Connect(ClientId client, const std::string& room) {
	auto found = rooms_.find(room);
	if (found == rooms_.end()) {
		if (RoomCount() >= kMaxRooms) {
			return false;
		}
		auto made = make_();
		if (!made) {
			return false;
		}
		found = rooms_.emplace(room, Room{.server = std::move(*made)}).first;
	}
	Room& chosen = found->second;
	++chosen.connections;
	chosen.empty_for = 0.0;
	clients_[client] = room;
	chosen.server->Connect(client);
	return true;
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

}  // namespace wolfenstein
