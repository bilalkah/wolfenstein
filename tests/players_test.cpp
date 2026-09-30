// Several players share a level, each in a slot of its own: each acts on
// its own commands, in slot order; they stop against each other's bodies;
// any of them takes pickups and opens doors, and only the viewer is told
// what came of it; the others see them as figures in their slot's colour,
// and the viewer's own figure is not drawn.

#include "Core/scene.h"
#include "Core/world.h"
#include "test_map.h"
#include "test_services.h"
#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace karakale {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kFacingDown = std::numbers::pi / 2;  // towards +y

// An open room with three players in it, side by side facing down it: the
// viewer in slot 0, the others in slots 1 and 3
class PlayersTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1, .pickups = 2};

	PlayersTest()
		: map_(testing::WriteMapFile(
				   "karakale_players_test.txt",
				   {"33333333", "30000003", "30000003", "30000003", "30000003",
					"30000003", "33333333"})
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_) {
		for (std::size_t i = 0; i < players_.size(); ++i) {
			CharacterConfig config(
				Position2D({1.5 + 2.0 * static_cast<double>(i), 1.5},
						   kFacingDown),
				2.0, 0.4, 0.4, 1.0);
			players_[i] = std::make_unique<Player>(
				config, testing::Weapon("mp5"), testing::TestTextures(),
				testing::TestSound());
		}
		scene_.SetPlayer(*players_[0]);
		scene_.SetPlayer(*players_[1], 1);
		scene_.SetPlayer(*players_[2], 3);
		const FigureStats& figure = testing::GameData().player_figure;
		scene_.SetPlayerLook(figure.clips, figure.width, figure.height);
	}

	void Run(double seconds) {
		for (double t = 0.0; t < seconds; t += kTick) {
			scene_.Update(kTick);
		}
	}
	// The figures drawn: the players the viewer sees
	std::vector<const PlayerFigure*> VisibleFigures() const {
		std::vector<const PlayerFigure*> figures;
		for (const IGameObject* object : scene_.GetObjects()) {
			if (object->GetObjectType() == ObjectType::CHARACTER_PLAYER &&
				object->IsVisible()) {
				figures.push_back(dynamic_cast<const PlayerFigure*>(object));
			}
		}
		return figures;
	}
	const PlayerFigure* FigureOf(const Player& player) const {
		for (const IGameObject* object : scene_.GetObjects()) {
			if (object->GetObjectType() == ObjectType::CHARACTER_PLAYER) {
				const auto* figure = dynamic_cast<const PlayerFigure*>(object);
				if (figure->Shown() == &player) {
					return figure;
				}
			}
		}
		return nullptr;
	}

	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	std::array<std::unique_ptr<Player>, 3> players_;
};

TEST_F(PlayersTest, EachPlayerTakesItsSlot) {
	const auto slots = scene_.GetPlayers();
	ASSERT_EQ(slots.size(), Scene::kMaxPlayers);
	EXPECT_EQ(slots[0], players_[0].get());
	EXPECT_EQ(slots[1], players_[1].get());
	EXPECT_EQ(slots[2], nullptr);
	EXPECT_EQ(slots[3], players_[2].get());
	EXPECT_EQ(&scene_.GetPlayer(), players_[0].get()) << "slot 0 is seen from";
}

TEST_F(PlayersTest, EachPlayerMovesOnItsOwnCommands) {
	scene_.FinishLoading();
	players_[0]->SetCommand({.forward = 1});
	players_[2]->SetCommand({.forward = -1});
	Run(0.5);
	EXPECT_NEAR(players_[0]->GetPose().y, 2.5, 0.05) << "walked a cell";
	EXPECT_DOUBLE_EQ(players_[1]->GetPose().y, 1.5) << "stood still";
	EXPECT_LT(players_[2]->GetPose().y, 1.5) << "backed off";
}

// Walking straight at another player, a body stops against the other's
// and does not push it
TEST_F(PlayersTest, PlayersStopAgainstEachOther) {
	players_[1]->SetPosition(Position2D({1.5, 3.5}, -kFacingDown));
	scene_.FinishLoading();
	players_[0]->SetCommand({.forward = 1});
	const double reach =
		players_[0]->GetWidth() / 2 + players_[1]->GetWidth() / 2;
	double closest = 1e9;
	for (double t = 0.0; t < 2.0; t += kTick) {
		scene_.Update(kTick);
		closest = std::min(
			closest, players_[0]->GetPose().Distance(players_[1]->GetPose()));
	}
	EXPECT_GE(closest, reach - 1e-9);
	EXPECT_LT(closest, reach + 0.05) << "it did come up against it";
	EXPECT_DOUBLE_EQ(players_[1]->GetPose().y, 3.5);
}

// A player out of the level is walked through
TEST_F(PlayersTest, ALeavingPlayerIsGone) {
	scene_.FinishLoading();
	players_[1]->SetPosition(Position2D({1.5, 3.5}, 0.0));
	scene_.RemovePlayer(1);
	EXPECT_EQ(scene_.GetPlayers()[1], nullptr);
	players_[0]->SetCommand({.forward = 1});
	players_[1]->SetCommand({.forward = 1});
	Run(1.5);
	EXPECT_GT(players_[0]->GetPose().y, 4.0) << "through where it stood";
	EXPECT_DOUBLE_EQ(players_[1]->GetPose().x, 1.5) << "not played any more";
	EXPECT_EQ(FigureOf(*players_[1]), nullptr);
}

// Every slot has a figure among the level's objects, from when it loads:
// the others' are drawn, each tinted, the viewer's not
TEST_F(PlayersTest, TheOthersAreSeenAsFiguresInTheirColours) {
	const std::size_t before = scene_.GetObjects().size();
	scene_.FinishLoading();
	EXPECT_EQ(
		scene_.GetObjects().size(),
		before + Scene::kEffects + Scene::kProjectiles + Scene::kMaxPlayers);

	const auto seen = VisibleFigures();
	ASSERT_EQ(seen.size(), 2u);
	EXPECT_EQ(seen[0]->Shown(), players_[1].get());
	EXPECT_EQ(seen[1]->Shown(), players_[2].get());
	EXPECT_EQ(seen[0]->GetPose().x, players_[1]->GetPose().x);
	const IGameObject::Tint first = seen[0]->SeenFrom({0.0, 0.0}).tint;
	const IGameObject::Tint second = seen[1]->SeenFrom({0.0, 0.0}).tint;
	EXPECT_NE(first, IGameObject::Tint{}) << "tinted";
	EXPECT_NE(first, second) << "each its own colour";

	// Seen from slot 1, it is the others that are drawn
	scene_.SetViewer(1);
	EXPECT_EQ(&scene_.GetPlayer(), players_[1].get());
	const auto from_one = VisibleFigures();
	ASSERT_EQ(from_one.size(), 2u);
	EXPECT_EQ(from_one[0]->Shown(), players_[0].get());
}

TEST_F(PlayersTest, AFigureWalksWhileItsPlayerMovesAndFallsWhenItDies) {
	scene_.FinishLoading();
	const PlayerFigure& figure = *FigureOf(*players_[1]);
	Run(0.1);
	EXPECT_FALSE(figure.IsWalking());
	players_[1]->SetCommand({.forward = 1});
	Run(0.1);
	EXPECT_TRUE(figure.IsWalking());
	players_[1]->SetCommand({});
	Run(0.1);
	EXPECT_FALSE(figure.IsWalking());

	// Dead, it shows the soldier's fall, frame by frame to the floor
	const auto death = testing::TestTextures().GetTextureCollection(
		testing::GameData().player_figure.clips + "_death");
	players_[1]->DecreaseHealth(1000.0);
	EXPECT_EQ(figure.SeenFrom({1.5, 1.5}).texture_id, death.front());
	Run(Player::kFallSeconds + 0.1);
	EXPECT_EQ(figure.SeenFrom({1.5, 1.5}).texture_id, death.back());
}

TEST_F(PlayersTest, AnotherPlayerTakesWhatItStandsOn) {
	ASSERT_TRUE(
		scene_.AddPickup(players_[1]->GetPose(), 0, 0.3, 0.3,
						 testing::GameData().pickups.at("medkit").effect));
	scene_.FinishLoading();
	players_[0]->DecreaseHealth(50.0);
	players_[1]->DecreaseHealth(50.0);
	scene_.Update(kTick);
	EXPECT_TRUE(scene_.GetPickups().front()->IsTaken());
	EXPECT_GT(players_[1]->GetHealth(), 50.0);
	EXPECT_DOUBLE_EQ(players_[0]->GetHealth(), 50.0);
}

// A room with a wall across it, and in the wall a door and a door locked
// with the gold key; the players face the wall (+x)
class PlayersDoorTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{};

	PlayersDoorTest()
		: map_(testing::WriteMapFile("karakale_players_door_test.txt",
									 {"3333333", "3000003", "3000003",
									  "33D3G33", "3000003", "3333333"})
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_) {
		scene_.SetPlayer(viewer_);
		scene_.SetPlayer(other_, 1);
		scene_.FinishLoading();
	}

	CharacterConfig viewer_config_{Position2D({1.5, 1.5}, 0.0), 2.0, 0.4, 0.4,
								   1.0};
	CharacterConfig other_config_{Position2D({2.5, 2.5}, 0.0), 2.0, 0.4, 0.4,
								  1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Player viewer_{viewer_config_, testing::Weapon("mp5"),
				   testing::TestTextures(), testing::TestSound()};
	Player other_{other_config_, testing::Weapon("mp5"),
				  testing::TestTextures(), testing::TestSound()};
};

// Another player opens a door it uses; one it has no key for stays shut,
// and the viewer is not told about someone else's locked door
TEST_F(PlayersDoorTest, AnotherPlayerOpensDoorsUntold) {
	other_.SetCommand({.use = true});
	scene_.Update(kTick);
	other_.SetCommand({});
	for (double t = 0.0; t < Scene::kDoorMoveSeconds + 0.1; t += kTick) {
		scene_.Update(kTick);
	}
	EXPECT_DOUBLE_EQ(scene_.GetMap().GetDoors()[0].openness, 1.0);

	other_.SetPosition(Position2D({2.5, 4.5}, 0.0));
	other_.SetCommand({.use = true});
	scene_.Update(kTick);
	EXPECT_DOUBLE_EQ(scene_.GetMap().GetDoors()[1].openness, 0.0);
	EXPECT_EQ(scene_.GetNotice(), Scene::Notice::None);

	// The viewer, trying it, is told
	other_.SetCommand({});
	viewer_.SetPosition(Position2D({2.5, 4.5}, 0.0));
	other_.SetPosition(Position2D({1.5, 5.5}, 0.0));
	viewer_.SetCommand({.use = true});
	scene_.Update(kTick);
	EXPECT_EQ(scene_.GetNotice(), Scene::Notice::NeedGoldKey);
}

std::unique_ptr<World> MakeWorld() {
	auto loader = SceneLoader::Open(RESOURCE_DIR);
	EXPECT_TRUE(loader) << loader.error();
	return std::make_unique<World>(testing::TestTextures(), std::move(*loader),
								   std::make_unique<SoundManager>());
}

// A player joins the game in a slot of its own, where the level starts;
// it comes along into the next level, until it leaves
TEST(WorldPlayers, AJoinedPlayerComesAlongUntilItLeaves) {
	auto world = MakeWorld();
	EXPECT_FALSE(world->JoinPlayer(1)) << "no game yet";
	ASSERT_TRUE(world->NewGame("mp5"));
	EXPECT_FALSE(world->JoinPlayer(0)) << "the local player's slot";
	EXPECT_FALSE(world->JoinPlayer(Scene::kMaxPlayers));
	const auto joined = world->JoinPlayer(2);
	ASSERT_TRUE(joined) << joined.error();
	Player* other = world->FindPlayer(2);
	ASSERT_NE(other, nullptr);
	EXPECT_EQ(world->CurrentLevel().GetPlayers()[2], other);
	EXPECT_EQ(&world->CurrentLevel().GetPlayer(), &world->GetPlayer());

	other->SetPosition(Position2D({0.0, 0.0}, 0.0));
	ASSERT_TRUE(world->NextLevel());
	EXPECT_EQ(world->CurrentLevel().GetPlayers()[2], other);
	EXPECT_DOUBLE_EQ(other->GetPose().x, world->GetPlayer().GetPose().x)
		<< "at the new level's start";

	world->LeavePlayer(2);
	EXPECT_EQ(world->FindPlayer(2), nullptr);
	EXPECT_EQ(world->CurrentLevel().GetPlayers()[2], nullptr);
}

// The camera and the draw queue are sized once for the largest level: its
// objects, the level's own and every scene's (puffs, projectiles and the
// players' figures), so no level grows them
TEST(WorldPlayers, EveryLevelFitsTheViewsSetAside) {
	auto world = MakeWorld();
	ASSERT_TRUE(world->NewGame("mp5"));
	const std::size_t levels = testing::GameData().levels.size();
	for (std::size_t index = 0; index < levels; ++index) {
		EXPECT_LE(world->CurrentLevel().GetObjects().size(),
				  world->LargestLevelObjects())
			<< "level " << index + 1;
		if (index + 1 < levels) {
			ASSERT_TRUE(world->NextLevel());
		}
	}
}

}  // namespace
}  // namespace karakale
