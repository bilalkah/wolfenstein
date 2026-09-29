// The game's multiplayer server: one match on an arena, its players
// connected by WebSocket. uWebSockets carries the messages; GameServer
// plays the game.
//
//   wolfenstein-server [--port 8080] [--level bazaar.json,warehouse.json]
//                      [--assets DIR]
//                      [--mode deathmatch|gunrace] [--frags 20]
//                      [--minutes 10]
//
// It speaks plain ws://: in front of it on the internet a proxy (Caddy)
// holds the certificate and passes wss:// on. GET /health answers "ok".

#include "Server/game_server.h"
#include <App.h>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace {

using wolfenstein::ClientId;
using wolfenstein::GameServer;

struct Connection
{
	ClientId id{};
};
using Socket = uWS::WebSocket<false, true, Connection>;

// The open connections, by id, for the game to send to. Closing waits until
// the game is done with the message or tick that asked for it: uWebSockets
// closes straight away, and would tell the game so while it is still busy.
class Sockets : public wolfenstein::Outbox
{
  public:
	void Add(ClientId id, Socket* socket) { open_.push_back({id, socket}); }
	void Remove(ClientId id) {
		std::erase_if(open_, [id](const Open& open) { return open.id == id; });
	}
	void Send(ClientId id, std::span<const std::uint8_t> message) override {
		if (Socket* socket = Find(id)) {
			socket->send(
				std::string_view(reinterpret_cast<const char*>(message.data()),
								 message.size()),
				uWS::OpCode::BINARY);
		}
	}
	void Close(ClientId id) override { closing_.push_back(id); }
	// Closes what the game asked to close
	void CloseAsked() {
		// Closing calls back into Remove: work on a copy
		std::vector<ClientId> closing;
		closing.swap(closing_);
		for (const ClientId id : closing) {
			if (Socket* socket = Find(id)) {
				socket->end(1000);
			}
		}
	}

  private:
	struct Open
	{
		ClientId id;
		Socket* socket;
	};
	Socket* Find(ClientId id) {
		for (const Open& open : open_) {
			if (open.id == id) {
				return open.socket;
			}
		}
		return nullptr;
	}

	std::vector<Open> open_;
	std::vector<ClientId> closing_;
};

// Moves the game on by as many ticks as time has passed, called often
struct Ticker
{
	GameServer* server = nullptr;
	Sockets* sockets = nullptr;
	std::chrono::steady_clock::time_point next;
};

void OnTimer(us_timer_t* timer) {
	Ticker* ticker = *static_cast<Ticker**>(us_timer_ext(timer));
	const auto tick =
		std::chrono::duration_cast<std::chrono::steady_clock::duration>(
			std::chrono::duration<double>(GameServer::kTickSeconds));
	const auto now = std::chrono::steady_clock::now();
	// Far behind (the machine was busy): the lost time is let go, not caught
	// up in a burst
	constexpr int kMostTicks = 5;
	if (now - ticker->next > tick * kMostTicks) {
		ticker->next = now;
	}
	while (ticker->next <= now) {
		ticker->server->Tick();
		ticker->next += tick;
	}
	ticker->sockets->CloseAsked();
}

// The value after `name` among the arguments, if given
std::string_view Option(int argc, char** argv, std::string_view name,
						std::string_view fallback) {
	for (int i = 1; i + 1 < argc; ++i) {
		if (name == argv[i]) {
			return argv[i + 1];
		}
	}
	return fallback;
}

// A whole number from `lowest` to `highest`, or nothing
std::optional<int> Number(std::string_view text, int lowest, int highest) {
	int value = 0;
	if (std::from_chars(text.data(), text.data() + text.size(), value).ec !=
			std::errc{} ||
		value < lowest || value > highest) {
		return std::nullopt;
	}
	return value;
}

}  // namespace

// Anything thrown ends the server with its reason
int main(int argc, char** argv) try {
	const std::string_view port_text = Option(argc, argv, "--port", "8080");
	const std::optional<int> port = Number(port_text, 1, 65535);
	if (!port) {
		std::cerr << "Not a port: " << port_text << '\n';
		return EXIT_FAILURE;
	}
	// The arenas, one after another; none named: every arena the game has
	std::vector<std::string> arenas;
	for (const auto part :
		 std::views::split(Option(argc, argv, "--level", ""), ',')) {
		if (!part.empty()) {
			arenas.emplace_back(std::string_view(part));
		}
	}
	const std::string assets(Option(argc, argv, "--assets", RESOURCE_DIR));
	wolfenstein::MatchSettings settings;
	const std::string_view mode = Option(argc, argv, "--mode", "deathmatch");
	if (mode == "gunrace") {
		settings.mode = wolfenstein::net::MatchMode::GunRace;
	}
	else if (mode != "deathmatch") {
		std::cerr << "No such mode: " << mode << " (deathmatch, gunrace)\n";
		return EXIT_FAILURE;
	}
	const std::string_view frags = Option(argc, argv, "--frags", "20");
	const std::string_view minutes = Option(argc, argv, "--minutes", "10");
	const std::optional<int> frag_limit = Number(frags, 1, 255);
	const std::optional<int> time_limit = Number(minutes, 1, 999);
	if (!frag_limit || !time_limit) {
		std::cerr << "Frags go from 1 to 255, minutes from 1 to 999\n";
		return EXIT_FAILURE;
	}
	settings.frag_limit = *frag_limit;
	settings.time_limit = *time_limit * 60.0;

	Sockets sockets;
	auto created = GameServer::Create(assets, arenas, sockets, settings);
	if (!created) {
		std::cerr << "Cannot start the game: " << created.error() << '\n';
		return EXIT_FAILURE;
	}
	GameServer& server = **created;

	std::uint32_t next_id = 1;
	uWS::App app;
	app.get("/health",
			[](auto* response, auto* /*request*/) { response->end("ok"); });
	app.ws<Connection>(
		"/*",
		{.compression = uWS::DISABLED,
		 .maxPayloadLength = wolfenstein::net::kMaxMessage,
		 .idleTimeout = 30,
		 .maxBackpressure = 64 * 1024,
		 .closeOnBackpressureLimit = true,
		 .open =
			 [&](Socket* socket) {
				 const ClientId id{next_id++};
				 socket->getUserData()->id = id;
				 sockets.Add(id, socket);
				 server.Connect(id);
				 sockets.CloseAsked();
			 },
		 .message =
			 [&](Socket* socket, std::string_view message, uWS::OpCode opcode) {
				 const ClientId id = socket->getUserData()->id;
				 if (opcode != uWS::OpCode::BINARY) {
					 sockets.Close(id);
				 }
				 else {
					 server.Receive(
						 id, std::span(reinterpret_cast<const std::uint8_t*>(
										   message.data()),
									   message.size()));
				 }
				 sockets.CloseAsked();
			 },
		 .close =
			 [&](Socket* socket, int /*code*/, std::string_view /*reason*/) {
				 const ClientId id = socket->getUserData()->id;
				 server.Disconnect(id);
				 sockets.Remove(id);
			 }});
	app.listen(*port, [port, mode](auto* listening) {
		if (listening == nullptr) {
			std::cerr << "Cannot listen on port " << *port << '\n';
			std::exit(EXIT_FAILURE);
		}
		std::cout << "Serving " << mode << " on port " << *port << '\n'
				  << std::flush;
	});

	Ticker ticker{.server = &server,
				  .sockets = &sockets,
				  .next = std::chrono::steady_clock::now()};
	us_timer_t* timer = us_create_timer(
		reinterpret_cast<us_loop_t*>(uWS::Loop::get()), 0, sizeof(Ticker*));
	*static_cast<Ticker**>(us_timer_ext(timer)) = &ticker;
	constexpr int kTimerMs = 4;
	us_timer_set(timer, OnTimer, kTimerMs, kTimerMs);
	app.run();
	return EXIT_SUCCESS;
}
catch (const std::exception& error) {
	std::cerr << "The server stopped: " << error.what() << '\n';
	return EXIT_FAILURE;
}
