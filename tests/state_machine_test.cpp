#include "Profiler/profiler.h"
#include "State/state.h"
#include "Strike/weapon.h"
#include "test_services.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace wolfenstein {

struct TestOwner;

template <>
struct StateType<TestOwner>
{
	enum class Type : std::uint8_t { First, Second };
};

namespace {

using Type = StateType<TestOwner>::Type;

class TestState : public State<TestOwner>
{
  public:
	void Reset() override { updates = 0; }
	int GetCurrentFrame() const override { return updates; }
	int updates = 0;
};

class SecondState : public TestState
{
  public:
	void OnEnter() override;
	void Update(const double&) override { ++updates; }
	Type GetType() const override { return Type::Second; }
};

// Requests a transition from inside its own Update and keeps running: the
// machine must not switch states until Update returns
class FirstState : public TestState
{
  public:
	void Update(const double&) override;
	Type GetType() const override { return Type::First; }
};

}  // namespace

struct TestOwner
{
	TestOwner() {
		first.SetContext(*this);
		second.SetContext(*this);
	}
	FirstState first;
	SecondState second;
	StateMachine<TestState> machine;
	std::vector<std::string> events;
};

namespace {

void SecondState::OnEnter() {
	TestState::OnEnter();
	context_->events.emplace_back("enter second");
}

void FirstState::Update(const double&) {
	context_->machine.TransitionTo(context_->second);
	context_->events.emplace_back(
		&context_->machine.Current() == this ? "first still current" : "?");
}

TEST(StateMachine, TransitionFromInsideUpdateWaitsUntilUpdateReturns) {
	TestOwner owner;
	owner.machine.TransitionTo(owner.first);
	owner.machine.Update(0.016);

	EXPECT_EQ(owner.machine.Current().GetType(), Type::Second);
	const std::vector<std::string> expected = {"first still current",
											   "enter second"};
	EXPECT_EQ(owner.events, expected);
}

TEST(StateMachine, TransitionFromOutsideUpdateIsImmediate) {
	TestOwner owner;
	owner.machine.TransitionTo(owner.second);
	EXPECT_EQ(owner.machine.Current().GetType(), Type::Second);
}

// States are reused, so per-visit data must be reset on every entry
TEST(StateMachine, EnteringAStateResetsIt) {
	TestOwner owner;
	owner.machine.TransitionTo(owner.second);
	owner.machine.Update(0.016);
	owner.machine.Update(0.016);
	EXPECT_EQ(owner.second.updates, 2);

	owner.machine.TransitionTo(owner.first);
	owner.machine.TransitionTo(owner.second);
	EXPECT_EQ(owner.second.updates, 0);
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
// A weapon owns all of its states: reloading and returning to loaded switch
// between them without touching the heap
TEST(StateMachine, WeaponTransitionsDoNotAllocate) {
	Weapon weapon(testing::Weapon("mp5"), testing::TestTextures(),
				  testing::TestSound());
	const auto before = AllocationStats::count;
	for (int i = 0; i < 100; ++i) {
		weapon.Reload();
		weapon.Update(weapon.GetReloadSpeed() + 0.1);  // reload completes
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
	EXPECT_EQ(weapon.GetAmmo(), weapon.GetAmmoCapacity());
}
#endif

}  // namespace
}  // namespace wolfenstein
