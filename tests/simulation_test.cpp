// The simulation reads no input device: it is driven by PlayerCommands, so a
// script can play it and the same commands always give the same game.

#include "Characters/player_command.h"
#include "Core/world.h"
#include "test_services.h"
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <numbers>
#include <vector>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;

struct Game
{
	std::unique_ptr<World> world;

	Game() {
		auto loader = SceneLoader::Open(RESOURCE_DIR);
		EXPECT_TRUE(loader) << loader.error();
		world =
			std::make_unique<World>(testing::TestTextures(), std::move(*loader),
									std::make_unique<SoundManager>());
		EXPECT_TRUE(world->NewGame("mp5"));
	}

	void Tick(const PlayerCommand& command) const {
		world->GetPlayer().SetCommand(command);
		world->CurrentLevel().Update(kTick);
	}
};

TEST(Simulation, ThePlayerWalksWhereItFaces) {
	Game game;
	const Position2D start = game.world->GetPlayer().GetPosition();
	for (int i = 0; i < 30; ++i) {
		game.Tick({.forward = 1});
	}
	const Position2D end = game.world->GetPlayer().GetPosition();
	const vector2d moved = end.pose - start.pose;
	// Half a second at 2 units per second, along the facing direction
	EXPECT_NEAR(moved.x, std::cos(start.theta) * 1.0, 1e-6);
	EXPECT_NEAR(moved.y, std::sin(start.theta) * 1.0, 1e-6);
	EXPECT_DOUBLE_EQ(end.theta, start.theta);
}

// Mouse look is a distance, not a rate: however many ticks a command is
// held for, it turns the player once
TEST(Simulation, MouseLookIsAppliedOnce) {
	Game game;
	const double start = game.world->GetPlayer().GetPosition().theta;
	game.world->GetPlayer().SetCommand({.look = 0.25});
	for (int i = 0; i < 4; ++i) {
		game.world->CurrentLevel().Update(kTick);
	}
	EXPECT_NEAR(game.world->GetPlayer().GetPosition().theta, start + 0.25,
				1e-9);
}

// A scripted run along the benchmark's wall-free route through level 1:
// turn to face each leg, then walk it, firing, past the first enemies
std::vector<PlayerCommand> Script() {
	constexpr double kWalkSpeed = 2.0;	// the player's speed in config.json
	std::vector<PlayerCommand> script;
	const auto walk = [&](double turn_to_face, double length) {
		script.push_back({.look = turn_to_face});
		const auto ticks =
			static_cast<int>(std::lround(length / (kWalkSpeed * kTick)));
		for (int i = 0; i < ticks; ++i) {
			script.push_back({.forward = 1, .fire = i % 30 < 15});
		}
	};
	walk(-1.5, 6.5);  // the player starts facing 1.5 rad; this leg runs at 0
	walk(std::numbers::pi / 2, 12.0);
	walk(-std::numbers::pi / 2, 7.0);
	return script;
}

// The property multiplayer rests on: the same commands give the same game,
// down to every enemy's position and health
TEST(Simulation, TheSameCommandsGiveTheSameGame) {
	Game first;
	Game second;
	for (const PlayerCommand& command : Script()) {
		first.Tick(command);
		second.Tick(command);
	}
	const Player& a = first.world->GetPlayer();
	const Player& b = second.world->GetPlayer();
	EXPECT_EQ(a.GetPosition().pose.x, b.GetPosition().pose.x);
	EXPECT_EQ(a.GetPosition().pose.y, b.GetPosition().pose.y);
	EXPECT_EQ(a.GetPosition().theta, b.GetPosition().theta);
	EXPECT_EQ(a.GetHealth(), b.GetHealth());

	const auto enemies_a = first.world->CurrentLevel().GetEnemies();
	const auto enemies_b = second.world->CurrentLevel().GetEnemies();
	ASSERT_EQ(enemies_a.size(), enemies_b.size());
	// Two idle runs would match trivially: the script must have woken the
	// enemies up
	const auto awake = std::ranges::count_if(enemies_a, [](const Enemy* enemy) {
		return enemy->GetStateType() != EnemyStateType::Idle;
	});
	EXPECT_GT(awake, 0);
	for (std::size_t i = 0; i < enemies_a.size(); ++i) {
		EXPECT_EQ(enemies_a[i]->GetPose().x, enemies_b[i]->GetPose().x) << i;
		EXPECT_EQ(enemies_a[i]->GetPose().y, enemies_b[i]->GetPose().y) << i;
		EXPECT_EQ(enemies_a[i]->GetHealth(), enemies_b[i]->GetHealth()) << i;
		EXPECT_EQ(enemies_a[i]->GetStateType(), enemies_b[i]->GetStateType())
			<< i;
	}
}

}  // namespace
}  // namespace wolfenstein
