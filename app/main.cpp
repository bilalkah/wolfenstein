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

int main() {
	using namespace wolfenstein;
	GeneralConfig config(800, 600, 0, 50, 120, 15.0, ToRadians(60.0), false);

	Game game(config);
	game.Run();

	return 0;
}