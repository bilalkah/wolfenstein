#include "State/state.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace wolfenstein {

struct TestOwner;

template <>
struct StateType<TestOwner>
{
	enum class Type { First, Second };
};

struct TestOwner
{
	StateMachine<TestOwner> machine{*this};
	std::vector<const char*> events;
};

namespace {

using Type = StateType<TestOwner>::Type;

class SecondState : public State<TestOwner>
{
  public:
	void OnContextSet() override { context_->events.push_back("enter second"); }
	void Update(const double&) override {}
	void Reset() override {}
	int GetCurrentFrame() const override { return 2; }
	Type GetType() const override { return Type::Second; }
};

// Requests a transition from inside its own Update, then keeps using its own
// members: legal only because the machine defers the transition
class FirstState : public State<TestOwner>
{
  public:
	~FirstState() override { context_->events.push_back("destroy first"); }
	void Update(const double&) override {
		context_->machine.TransitionTo(std::make_unique<SecondState>());
		context_->events.push_back(still_alive_ ? "first still alive" : "?");
	}
	void Reset() override {}
	int GetCurrentFrame() const override { return 1; }
	Type GetType() const override { return Type::First; }

  private:
	bool still_alive_ = true;
};

TEST(StateMachine, TransitionFromInsideUpdateIsDeferredUntilUpdateReturns) {
	TestOwner owner;
	owner.machine.TransitionTo(std::make_unique<FirstState>());
	owner.machine.Update(0.016);

	EXPECT_EQ(owner.machine.Current().GetType(), Type::Second);
	const std::vector<const char*> expected = {"first still alive",
											   "destroy first", "enter second"};
	ASSERT_EQ(owner.events.size(), expected.size());
	for (std::size_t i = 0; i < expected.size(); ++i) {
		EXPECT_STREQ(owner.events[i], expected[i]);
	}
}

TEST(StateMachine, TransitionFromOutsideUpdateIsImmediate) {
	TestOwner owner;
	owner.machine.TransitionTo(std::make_unique<SecondState>());
	EXPECT_EQ(owner.machine.Current().GetType(), Type::Second);
	EXPECT_EQ(owner.machine.Current().GetCurrentFrame(), 2);
}

}  // namespace
}  // namespace wolfenstein
