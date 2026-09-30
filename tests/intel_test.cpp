// Pages of intel hang on a level's walls: the player reads one by walking up
// to it and looking at it, and again by using it; the level counts them

#include "Core/scene.h"
#include "Profiler/profiler.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <numbers>

namespace karakale {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kFacingDown = 0.0;					   // towards +x
constexpr double kFacingUp = std::numbers::pi;		   // towards -x
constexpr double kFacingRight = std::numbers::pi / 2;  // towards +y

// A room with a pillar in it, cell (2, 3); a page hangs on the pillar's face
// towards row 1, its middle at (2.0, 3.5)
class IntelTest : public ::testing::Test
{
  protected:
	IntelTest()
		: map_(testing::WriteMapFile(
				   "karakale_intel_test.txt",
				   {"3333333", "3000003", "3003003", "3000003", "3333333"})
				   .string()),
		  arena_(Scene::MemoryFor(map_, {})),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, {},
				 arena_),
		  player_(config_, testing::Weapon("mp5"), testing::TestTextures(),
				  testing::TestSound()) {
		scene_.SetPlayer(player_);
		EXPECT_TRUE(scene_.AddIntel(2, 3, -1, 0));
		scene_.FinishLoading();
	}

	void StandAt(vector2d where, double theta) {
		player_.SetPosition(Position2D(where, theta));
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, kFacingDown), 2.0, 0.4, 0.4,
							1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Player player_;
};

TEST_F(IntelTest, APageIsReadByWalkingUpToItAndLookingAtIt) {
	// Looking at it from across the room: too far to read
	StandAt({1.5, 1.2}, kFacingRight);
	scene_.Update(kTick);
	EXPECT_EQ(scene_.ShownDocument(), -1);
	EXPECT_FALSE(scene_.GetIntel().front().read);

	StandAt({1.3, 3.5}, kFacingDown);
	scene_.Update(kTick);
	EXPECT_EQ(scene_.ShownDocument(), 0);
	EXPECT_TRUE(scene_.GetIntel().front().read);

	// It shows for a while, then goes; read, it does not show by itself again
	for (double shown = 0.0; shown <= Scene::kDocumentSeconds; shown += kTick) {
		scene_.Update(kTick);
	}
	EXPECT_EQ(scene_.ShownDocument(), -1);
}

TEST_F(IntelTest, APageIsNotReadWithTheBackToIt) {
	StandAt({1.3, 3.5}, kFacingUp);
	scene_.Update(kTick);
	EXPECT_EQ(scene_.ShownDocument(), -1);
}

// Its face looks one way: from the far side of the pillar there is only wall
TEST_F(IntelTest, APageIsNotReadFromBehindItsWall) {
	StandAt({3.1, 3.5}, kFacingUp);
	scene_.Update(kTick);
	EXPECT_EQ(scene_.ShownDocument(), -1);
	EXPECT_FALSE(scene_.GetIntel().front().read);
}

TEST_F(IntelTest, UsingAPageReadsItAgain) {
	StandAt({1.3, 3.5}, kFacingDown);
	scene_.Update(kTick);
	for (double shown = 0.0; shown <= Scene::kDocumentSeconds; shown += kTick) {
		scene_.Update(kTick);
	}
	ASSERT_EQ(scene_.ShownDocument(), -1);

	player_.SetCommand(PlayerCommand{.use = true});
	scene_.Update(kTick);
	EXPECT_EQ(scene_.ShownDocument(), 0);
	EXPECT_LT(scene_.DocumentAge(), 2 * kTick);
}

TEST_F(IntelTest, TheLevelCountsItsPagesRead) {
	EXPECT_EQ(scene_.GetStats().documents, 1u);
	EXPECT_EQ(scene_.GetStats().documents_found, 0u);
	StandAt({1.3, 3.5}, kFacingDown);
	scene_.Update(kTick);
	EXPECT_EQ(scene_.GetStats().documents_found, 1u);
}

// As a saved game left it: read, and not shown
TEST_F(IntelTest, APageReadBeforeStaysRead) {
	scene_.RestoreRead(0);
	scene_.RestoreRead(5);	// no such page: nothing happens
	EXPECT_TRUE(scene_.GetIntel().front().read);
	EXPECT_EQ(scene_.ShownDocument(), -1);
}

// A page faces along x or y, and a level holds so many
TEST_F(IntelTest, ALevelHoldsSoManyPages) {
	EXPECT_FALSE(scene_.AddIntel(0, 1, 1, 1));
	EXPECT_FALSE(scene_.AddIntel(0, 1, 0, 0));
	while (scene_.GetIntel().size() < Scene::kIntel) {
		ASSERT_TRUE(scene_.AddIntel(0, 1, 1, 0));
	}
	EXPECT_FALSE(scene_.AddIntel(0, 1, 1, 0));
}

// Each face of a wall is the face a ray from its open side strikes
TEST_F(IntelTest, APageFacesTheWayItLooks) {
	ASSERT_TRUE(scene_.AddIntel(2, 3, 1, 0));
	ASSERT_TRUE(scene_.AddIntel(2, 3, 0, -1));
	ASSERT_TRUE(scene_.AddIntel(2, 3, 0, 1));
	const auto pages = scene_.GetIntel();
	EXPECT_EQ(pages[0].face, 0);  // struck by a ray going +x
	EXPECT_EQ(pages[1].face, 1);
	EXPECT_EQ(pages[2].face, 2);  // struck by a ray going +y
	EXPECT_EQ(pages[3].face, 3);
}

#ifdef KARAKALE_COUNTS_ALLOCATIONS
TEST_F(IntelTest, ReadingAllocatesNothing) {
	StandAt({1.3, 3.5}, kFacingDown);
	const auto before = AllocationStats::count;
	scene_.Update(kTick);
	EXPECT_EQ(AllocationStats::count - before, 0u);
	EXPECT_EQ(scene_.ShownDocument(), 0);
}
#endif

}  // namespace
}  // namespace karakale
