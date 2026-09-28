// Shots that fly: a rocket or a plasma bolt goes straight on at its speed
// and bursts on the first wall or living enemy in its way. A rocket's blast
// hurts whoever is in reach and in sight of the burst, the one who fired it
// too; a bolt hurts only what it strikes.

#include "Core/scene.h"
#include "Profiler/profiler.h"
#include "test_map.h"
#include "test_services.h"
#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kDown = std::numbers::pi / 2;	// along +y

// How many more times `effect` is played while `action` runs
template <typename Action>
std::uint32_t Plays(SoundEffect effect, Action action) {
	const std::uint32_t before = testing::TestSound().PlayCount(effect);
	action();
	return testing::TestSound().PlayCount(effect) - before;
}

const ProjectileConfig& Rocket() {
	static const ProjectileConfig rocket =
		testing::Weapon("rocket_launcher")
			.projectile.value_or(ProjectileConfig{});
	return rocket;
}

// A map file's rows run along x: a hall two cells wide, from y = 1 to its
// end wall at y = 13, and a sealed pocket behind that wall. The player at
// (1.5, 1.5) looks down the hall.
class ProjectileTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 3};

	ProjectileTest() { scene_.SetPlayer(player_); }

	// A soldier standing guard at `at`, looking back up the hall
	Enemy& Add(vector2d at) {
		EXPECT_TRUE(
			scene_.AddEnemy(testing::Enemy("soldier"), Position2D(at, -kDown)));
		return *scene_.GetEnemies().back();
	}
	// Fires the weapon's projectile from where the player stands, as the
	// player looks
	void Fire(std::string_view weapon) {
		const WeaponConfig& config = testing::Weapon(weapon);
		if (!config.projectile) {
			ADD_FAILURE() << weapon << " fires nothing that flies";
			return;
		}
		scene_.Launch(*config.projectile, player_.GetPosition().pose,
					  player_.GetPosition().theta, config.attack_damage.first);
	}
	const Projectile* Flying() const {
		const auto projectiles = scene_.GetProjectiles();
		const auto found = std::ranges::find_if(
			projectiles, [](const Projectile& p) { return p.IsFlying(); });
		return found == projectiles.end() ? nullptr : &*found;
	}
	void Run(double seconds) {
		for (double t = 0.0; t < seconds; t += kTick) {
			scene_.Update(kTick);
		}
	}
	// Until nothing is in flight (at most a few seconds)
	void RunWhileFlying() {
		for (int tick = 0; tick < 300 && Flying() != nullptr; ++tick) {
			scene_.Update(kTick);
		}
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, kDown), 2.0, 0.4, 0.4, 1.0};
	Map map_{testing::WriteMapFile("wolfenstein_projectile_test.txt",
								   {"3333333333333333", "3000000000000303",
									"3000000000000333", "3333333333333333"})
				 .string()};
	memory::MonotonicArena arena_{Scene::MemoryFor(map_, kCapacity)};
	Scene scene_{testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_};
	Player player_{config_, testing::GameData().weapons, 0,
				   testing::TestTextures(), testing::TestSound()};
};

// It sets off from a little ahead of the one firing, and flies straight on
// at its speed
TEST_F(ProjectileTest, ARocketFliesStraightOnAtItsSpeed) {
	scene_.FinishLoading();
	Fire("rocket_launcher");
	const Projectile* rocket = Flying();
	ASSERT_NE(rocket, nullptr);
	const double start = rocket->GetPose().y;
	EXPECT_NEAR(start, 1.8, 1e-9) << "out of the muzzle";
	Run(0.5);
	ASSERT_TRUE(rocket->IsFlying());
	EXPECT_NEAR(rocket->GetPose().y - start, Rocket().speed * 0.5, 0.2);
	EXPECT_DOUBLE_EQ(rocket->GetPose().x, 1.5);
}

// Drawn between ticks, it is where it would be by then
TEST_F(ProjectileTest, ItIsDrawnBetweenTicks) {
	scene_.FinishLoading();
	Fire("rocket_launcher");
	scene_.Update(kTick);
	const Projectile* rocket = Flying();
	ASSERT_NE(rocket, nullptr);
	EXPECT_NEAR(rocket->GetRenderPose(0.5).y,
				rocket->GetPose().y - Rocket().speed * kTick / 2, 1e-9);
	EXPECT_DOUBLE_EQ(rocket->GetRenderPose(1.0).y, rocket->GetPose().y);
}

// It bursts short of the first wall, once, with the sound of it, and its
// burst shows there
TEST_F(ProjectileTest, ItBurstsShortOfAWall) {
	scene_.FinishLoading();
	const std::uint32_t bursts = Plays(SoundEffect::RocketBurst, [&] {
		Fire("rocket_launcher");
		RunWhileFlying();
	});
	EXPECT_EQ(bursts, 1u);
	EXPECT_EQ(Flying(), nullptr);
	const auto objects = scene_.GetObjects();
	const auto shown =
		std::ranges::find_if(objects, [](const IGameObject* object) {
			return object->GetObjectType() == ObjectType::EFFECT &&
				   object->IsVisible();
		});
	ASSERT_NE(shown, objects.end());
	EXPECT_GT((*shown)->GetPose().y, 12.0);
	EXPECT_LT((*shown)->GetPose().y + Rocket().radius, 13.0);
}

// A direct hit does the rocket's damage and its blast's at once: a soldier
// falls, counted once, and the player sees the hit. The blast hurts another
// beside it, less, further off.
TEST_F(ProjectileTest, ADirectHitAndItsBlast) {
	Enemy& struck = Add({1.5, 10.5});
	Enemy& beside = Add({2.5, 10.9});
	scene_.FinishLoading();
	const std::size_t alive = scene_.GetNumberOfAliveEnemies();
	const double health = beside.GetHealth();
	Fire("rocket_launcher");
	RunWhileFlying();
	EXPECT_LE(struck.GetHealth(), 0.0);
	EXPECT_EQ(scene_.GetNumberOfAliveEnemies(), alive - 1);
	EXPECT_LT(beside.GetHealth(), health);
	EXPECT_GT(beside.GetHealth(), 0.0);
	EXPECT_GT(player_.GetHitMarker(), 0.0);
}

// The blast reaches through open air, not through a wall
TEST_F(ProjectileTest, AWallShieldsFromTheBlast) {
	Enemy& near = Add({2.5, 12.2});
	Enemy& behind = Add({1.5, 14.3});  // in the pocket, in reach
	scene_.FinishLoading();
	const double near_health = near.GetHealth();
	const double behind_health = behind.GetHealth();
	Fire("rocket_launcher");
	RunWhileFlying();
	EXPECT_LT(near.GetHealth(), near_health);
	EXPECT_DOUBLE_EQ(behind.GetHealth(), behind_health);
}

// Fired into a wall at arm's length, a rocket bursts at once, and the
// player caught in it is hurt, by half what the blast does to others: not
// killed outright
TEST_F(ProjectileTest, ThePlayerCaughtInTheirOwnBlastIsHurt) {
	scene_.FinishLoading();
	player_.SetPosition(Position2D({1.5, 12.7}, kDown));
	Fire("rocket_launcher");
	EXPECT_EQ(Flying(), nullptr) << "burst at the muzzle";
	EXPECT_LT(player_.GetHealth(), 100.0);
	EXPECT_GE(player_.GetHealth(), 100.0 - Rocket().splash_damage.first / 2);
}

// A plasma bolt has no blast: it hurts what it strikes, by its damage, and
// nothing beside it
TEST_F(ProjectileTest, APlasmaBoltHurtsOnlyWhatItStrikes) {
	Enemy& struck = Add({1.5, 10.5});
	Enemy& beside = Add({2.5, 10.9});
	scene_.FinishLoading();
	const double health = struck.GetHealth();
	const double beside_health = beside.GetHealth();
	Fire("plasma_rifle");
	RunWhileFlying();
	EXPECT_DOUBLE_EQ(
		struck.GetHealth(),
		health - testing::Weapon("plasma_rifle").attack_damage.first);
	EXPECT_DOUBLE_EQ(beside.GetHealth(), beside_health);
}

// The rocket launcher's trigger launches a rocket, with the sound of it,
// and spends a round
TEST_F(ProjectileTest, TheRocketLauncherFiresRockets) {
	scene_.FinishLoading();
	constexpr std::size_t kLauncher = 5;
	player_.SetOwnedWeapons(0x7F);
	player_.SetCommand({.weapon = kLauncher});
	scene_.Update(kTick);
	player_.SetCommand({});
	Run(1.5);
	ASSERT_EQ(&player_.GetWeapon(), &player_.GetWeapon(kLauncher));
	const std::size_t rounds = player_.GetWeapon().GetAmmo();
	EXPECT_EQ(Plays(SoundEffect::RocketLaunch,
					[&] {
						player_.SetCommand({.fire = true});
						scene_.Update(kTick);
					}),
			  1u);
	EXPECT_NE(Flying(), nullptr);
	EXPECT_EQ(player_.GetWeapon().GetAmmo(), rounds - 1);
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
// Rockets and bolts fired, flying and bursting allocate nothing
TEST_F(ProjectileTest, FiringAllocatesNothing) {
	Add({1.5, 10.5});
	Add({2.5, 12.2});
	scene_.FinishLoading();
	const auto before = AllocationStats::count;
	for (int volley = 0; volley < 40; ++volley) {
		Fire("rocket_launcher");
		Fire("plasma_rifle");
		Run(0.1);
	}
	RunWhileFlying();
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

}  // namespace
}  // namespace wolfenstein
