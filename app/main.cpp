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
#include <cstdlib>
#include <string_view>

int main(int argc, char** argv) {
	using namespace wolfenstein;
	GeneralConfig config(1200, 900, 0, 20, 120, 15.0, ToRadians(60.0), false);

	Game game(config);
	// --benchmark [frames]: run the performance benchmark instead of the game
	if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
		constexpr int kDefaultFrames = 2000;
		const int frames = argc > 2 ? std::atoi(argv[2]) : 0;
		game.StartBenchmark(frames > 0 ? frames : kDefaultFrames);
	}
	game.Run();

	return 0;
}