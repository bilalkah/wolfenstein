/**
 * @file main.cpp
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-02-05
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "Core/game.h"
#include "Settings/settings.h"
#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
	using namespace wolfenstein;
	GeneralConfig config(1200, 900, 0, 20, 120, 15.0, ToRadians(60.0), false);

	Game game(config);
	// --debug (anywhere): P shows the whole level in 2D, enemies and all
	for (int i = 1; i < argc; ++i) {
		if (std::string_view(argv[i]) == "--debug") {
			game.EnableDebugView();
		}
	}
	// --connect ws://host:port [--name NAME]: play a multiplayer game;
	// --default-server URL: the server the multiplayer screen offers until
	// the player names another
	std::string_view server;
	std::string_view name = "player";
	for (int i = 1; i + 1 < argc; ++i) {
		const std::string_view option = argv[i];
		if (option == "--connect") {
			server = argv[i + 1];
		}
		if (option == "--name") {
			name = argv[i + 1];
		}
		if (Settings& settings = Settings::Get();
			option == "--default-server" &&
			settings.server.View() == Settings::kDefaultServer) {
			settings.server.Set(argv[i + 1]);
		}
	}
	// --benchmark [frames]: run the performance benchmark instead of the game
	if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
		constexpr int kDefaultFrames = 2000;
		const int frames = argc > 2 ? std::atoi(argv[2]) : 0;
		game.StartBenchmark(frames > 0 ? frames : kDefaultFrames);
	}
	// --soak [frames]: play a scripted session and report the allocations
	// made after startup (there must be none)
	else if (argc > 1 && std::string_view(argv[1]) == "--soak") {
		constexpr int kMinimumFrames = 1000;
		const int frames = argc > 2 ? std::atoi(argv[2]) : 0;
		game.StartSoak(std::max(frames, kMinimumFrames));
	}
	else if (!server.empty()) {
		game.Connect(std::string(server), name);
	}
	game.Run();

	return 0;
}