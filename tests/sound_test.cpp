// What the game asks the sound to play, and when: doors as they move, an
// enemy's shout as it starts hunting, footsteps as the player walks, and
// each pickup its own sound. The tests' sound is silent but counts. And how
// loud: the music and the effects each a share of the master volume.

#include "Core/scene.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <numbers>
#include <string>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kSouth = std::numbers::pi / 2;	 // towards +y

// How many more times `effect` is played while `action` runs
template <typename Action>
std::uint32_t Plays(SoundEffect effect, Action action) {
	const std::uint32_t before = testing::TestSound().PlayCount(effect);
	action();
	return testing::TestSound().PlayCount(effect) - before;
}

// A corridor along y (a map file's rows run along x) with a door at (1, 4),
// the player at (1.5, 1.5) facing down it
class SoundTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1};

	void Load(bool enemy, const std::string& type = "soldier") {
		if (enemy) {
			ASSERT_TRUE(scene_.AddEnemy(testing::Enemy(type),
										Position2D({1.5, 3.5}, -kSouth)));
		}
		scene_.FinishLoading();
	}
	void Run(double seconds, PlayerCommand command = {}) {
		for (int tick = 0; tick < static_cast<int>(seconds / kTick); ++tick) {
			player_.SetCommand(command);
			scene_.Update(kTick);
		}
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, kSouth), 2.0, 0.4, 0.4, 1.0};
	Player player_{config_, testing::GameData().weapons, 0,
				   testing::TestTextures(), testing::TestSound()};
	Map map_{testing::WriteMapFile("wolfenstein_sound_test.txt",
								   {"3333333333", "3000D00003", "3333333333"})
				 .string()};
	memory::MonotonicArena arena_{Scene::MemoryFor(map_, kCapacity)};
	Scene scene_{testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_};
	bool set_ = (scene_.SetPlayer(player_), true);
};

// A door sounds as it starts to open and as it starts to close, once each
TEST_F(SoundTest, ADoorSoundsAsItMoves) {
	Load(false);
	EXPECT_EQ(Plays(SoundEffect::DoorMove, [&] { scene_.OpenDoor(0); }), 1u);
	EXPECT_EQ(Plays(SoundEffect::DoorMove,
					[&] {
						scene_.OpenDoor(0);	 // already opening
						Run(Scene::kDoorMoveSeconds + 0.1);
					}),
			  0u);
	EXPECT_EQ(Plays(SoundEffect::DoorMove,
					[&] { Run(Scene::kDoorOpenSeconds + 0.2); }),
			  1u)
		<< "closing";
}

// An enemy shouts once, as it sees the player and starts hunting
TEST_F(SoundTest, AnEnemyShoutsAsItStartsHunting) {
	Load(true);
	EXPECT_EQ(Plays(SoundEffect::EnemyAlert, [&] { Run(2.0); }), 1u);
}

// Walking, a footstep every stride, one foot then the other
// Each kind has its own voice: a demon roars as it sees the player, rushes
// in and bites, never with a soldier's shout or shot
TEST_F(SoundTest, EachEnemySoundsItsOwn) {
	Load(true, "demon");
	std::uint32_t roars = 0;
	std::uint32_t bites = 0;
	const std::uint32_t shouts = Plays(SoundEffect::EnemyAlert, [&] {
		roars = Plays(SoundEffect::DemonAlert, [&] {
			bites = Plays(SoundEffect::DemonAttack, [&] { Run(2.0); });
		});
	});
	EXPECT_EQ(roars, 1u);
	EXPECT_GE(bites, 1u);
	EXPECT_EQ(shouts, 0u);
}

TEST_F(SoundTest, WalkingSoundsFootsteps) {
	Load(false);
	const std::uint32_t left =
		testing::TestSound().PlayCount(SoundEffect::StepLeft);
	const std::uint32_t right =
		testing::TestSound().PlayCount(SoundEffect::StepRight);
	Run(1.0, {.forward = 1});  // two units at the player's speed
	const std::uint32_t lefts =
		testing::TestSound().PlayCount(SoundEffect::StepLeft) - left;
	const std::uint32_t rights =
		testing::TestSound().PlayCount(SoundEffect::StepRight) - right;
	EXPECT_EQ(lefts + rights, 2u);
	EXPECT_EQ(lefts, 1u) << "one of each";
	EXPECT_EQ(Plays(SoundEffect::StepLeft, [&] { Run(1.0); }) +
				  Plays(SoundEffect::StepRight, [&] { Run(1.0); }),
			  0u)
		<< "standing still";
}

// Each pickup sounds of what it gave
TEST_F(SoundTest, EachPickupHasItsSound) {
	Load(false);
	EXPECT_EQ(Plays(SoundEffect::AmmoPickup,
					[&] { player_.TryPickUp({.ammo_boxes = 1}); }),
			  1u);
	EXPECT_EQ(
		Plays(SoundEffect::KeyPickup,
			  [&] { player_.TryPickUp({.keys = KeyBit(KeyColour::Gold)}); }),
		1u);
	EXPECT_EQ(Plays(SoundEffect::WeaponPickup,
					[&] { player_.TryPickUp({.weapons = 1U << 1}); }),
			  1u);
	player_.DecreaseHealth(30.0);
	EXPECT_EQ(Plays(SoundEffect::Pickup,
					[&] { player_.TryPickUp({.health = 25.0}); }),
			  1u);
}

// The music and the effects turn down apart, and the master both
TEST(MixerLevels, MusicAndEffectsAreSharesOfTheMaster) {
	const MixerLevels full = ToMixerLevels(1.0, 1.0, 1.0);
	EXPECT_FLOAT_EQ(full.effects, 1.0F) << "as recorded";
	EXPECT_GT(full.music, 0.0F);

	EXPECT_FLOAT_EQ(ToMixerLevels(1.0, 0.0, 1.0).music, 0.0F);
	EXPECT_FLOAT_EQ(ToMixerLevels(1.0, 0.0, 1.0).effects, full.effects)
		<< "no music, the effects as loud";
	EXPECT_FLOAT_EQ(ToMixerLevels(1.0, 1.0, 0.0).effects, 0.0F);
	EXPECT_FLOAT_EQ(ToMixerLevels(1.0, 1.0, 0.0).music, full.music);

	const MixerLevels half = ToMixerLevels(0.5, 1.0, 1.0);
	EXPECT_FLOAT_EQ(half.effects, full.effects / 2);
	EXPECT_FLOAT_EQ(half.music, full.music / 2);
	EXPECT_FLOAT_EQ(ToMixerLevels(0.0, 1.0, 1.0).music, 0.0F);
	EXPECT_FLOAT_EQ(ToMixerLevels(0.0, 1.0, 1.0).effects, 0.0F);
	EXPECT_FLOAT_EQ(ToMixerLevels(2.0, 1.0, 1.0).effects, full.effects)
		<< "beyond full is full";
}

}  // namespace
}  // namespace wolfenstein
