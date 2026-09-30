// The game's multiplayer server: its matches (the open one, and private
// rooms: "/create" makes one, "/room/CODE" joins it), their players
// connected by WebSocket. uWebSockets carries the messages; the Lobby hands
// them to each room's GameServer.
//
//   karakale-server [--port 8080] [--level bazaar.json,warehouse.json]
//                   [--assets DIR]
//                   [--mode deathmatch|gunrace] [--frags 20]
//                   [--minutes 10]
//
// It speaks plain ws://: in front of it on the internet a proxy (Caddy)
// holds the certificate and passes wss:// on. GET /health answers "ok".
// At most Addresses::kPerAddress connections come from one address (behind
// the proxy, the one it names); more are refused with 429.

#include "Server/addresses.h"
#include "Server/lobby.h"
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
#include <utility>
#include <vector>

namespace {

using karakale::Addresses;
using karakale::ClientId;
using karakale::GameServer;
using karakale::Lobby;

struct Connection
{
	ClientId id{};
	Lobby::Request request;	 // a new room, one by its code, or the open one
	std::string address;	 // what it counts under (Addresses::KeyOf)
};
using Socket = uWS::WebSocket<false, true, Connection>;

// The open connections, by id, for the game to send to. Closing waits until
// the game is done with the message or tick that asked for it: uWebSockets
// closes straight away, and would tell the game so while it is still busy.
class Sockets : public karakale::Outbox
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
	Lobby* lobby = nullptr;
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
		ticker->lobby->Tick();
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
	karakale::MatchSettings settings;
	const std::string_view mode = Option(argc, argv, "--mode", "deathmatch");
	if (mode == "gunrace") {
		settings.mode = karakale::net::MatchMode::GunRace;
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
	auto created = Lobby::Create(
		[&] { return GameServer::Create(assets, arenas, sockets, settings); });
	if (!created) {
		std::cerr << "Cannot start the game: " << created.error() << '\n';
		return EXIT_FAILURE;
	}
	Lobby& lobby = **created;

	std::uint32_t next_id = 1;
	Addresses addresses;
	uWS::App app;
	app.get("/health",
			[](auto* response, auto* /*request*/) { response->end("ok"); });
	app.ws<Connection>(
		"/*",
		{.compression = uWS::DISABLED,
		 .maxPayloadLength = karakale::net::kMaxMessage,
		 .idleTimeout = 30,
		 .maxBackpressure = 64 * 1024,
		 .closeOnBackpressureLimit = true,
		 // The room the address names goes with the connection, and whom
		 // it comes from: one who has too many open already is refused
		 .upgrade =
			 [&](auto* response, auto* request, auto* context) {
				 std::string address =
					 Addresses::KeyOf(response->getRemoteAddress(),
									  request->getHeader("x-forwarded-for"));
				 if (!addresses.Allows(address)) {
					 response->writeStatus("429 Too Many Requests")
						 ->end("Too many connections from one address\n");
					 return;
				 }
				 response->template upgrade<Connection>(
					 {.id = ClientId{next_id++},
					  .request = Lobby::RequestOf(request->getUrl()),
					  .address = std::move(address)},
					 request->getHeader("sec-websocket-key"),
					 request->getHeader("sec-websocket-protocol"),
					 request->getHeader("sec-websocket-extensions"), context);
			 },
		 .open =
			 [&](Socket* socket) {
				 const Connection& connection = *socket->getUserData();
				 addresses.Open(connection.address);
				 sockets.Add(connection.id, socket);
				 // No room of its code, or none can be made: it is told why,
				 // and goes
				 if (const auto joined = lobby.Connect(
						 connection.id, connection.request, connection.address);
					 !joined) {
					 std::array<std::uint8_t, karakale::net::kMaxMessage>
						 reject{};
					 const std::size_t size = karakale::net::Encode(
						 karakale::net::Reject{.reason = joined.error()},
						 reject);
					 sockets.Send(connection.id, std::span(reject).first(size));
					 sockets.Close(connection.id);
				 }
				 sockets.CloseAsked();
			 },
		 .message =
			 [&](Socket* socket, std::string_view message, uWS::OpCode opcode) {
				 const ClientId id = socket->getUserData()->id;
				 if (opcode != uWS::OpCode::BINARY) {
					 sockets.Close(id);
				 }
				 else {
					 lobby.Receive(
						 id, std::span(reinterpret_cast<const std::uint8_t*>(
										   message.data()),
									   message.size()));
				 }
				 sockets.CloseAsked();
			 },
		 .close =
			 [&](Socket* socket, int /*code*/, std::string_view /*reason*/) {
				 const Connection& connection = *socket->getUserData();
				 lobby.Disconnect(connection.id);
				 sockets.Remove(connection.id);
				 addresses.Close(connection.address);
			 }});
	app.listen(*port, [port, mode](auto* listening) {
		if (listening == nullptr) {
			std::cerr << "Cannot listen on port " << *port << '\n';
			std::exit(EXIT_FAILURE);
		}
		std::cout << "Serving " << mode << " on port " << *port << '\n'
				  << std::flush;
	});

	Ticker ticker{.lobby = &lobby,
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
