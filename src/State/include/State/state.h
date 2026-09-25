/**
 * @file state.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-09-05
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STATE_INCLUDE_STATE_STATE_H_
#define STATE_INCLUDE_STATE_STATE_H_

#include <utility>

namespace wolfenstein {

template <typename T>
struct StateType;

// A state of an owner T (an enemy, a weapon). The owner owns its states
// through a StateMachine; the state refers back to the owner with a
// non-owning pointer, which is valid because the owner outlives every state
// it owns. (A shared_ptr back to the owner formed a reference cycle that
// leaked every owner and state.)
template <typename T>
class State
{
  public:
	virtual ~State() = default;

  protected:
	// Copies and moves only through derived classes: copying through the
	// base would slice off the derived part
	State() = default;
	State(const State&) = default;
	State& operator=(const State&) = default;
	State(State&&) = default;
	State& operator=(State&&) = default;

  public:
	void SetContext(T& context) {
		context_ = &context;
		OnContextSet();
	};
	// Called once, when the owner hands the state its context: read
	// configuration, build animations
	virtual void OnContextSet() { /* Do nothing */ };
	// Called every time the state becomes current: reset per-visit data
	virtual void OnEnter() { Reset(); }
	virtual void Update(const double& delta_time) = 0;
	virtual void Reset() = 0;
	virtual int GetCurrentFrame() const = 0;
	virtual StateType<T>::Type GetType() const = 0;

  protected:
	T* context_ = nullptr;
};

// Tracks which of its owner's states is current. The owner keeps every
// state it can be in as a member, for its whole lifetime, so a transition
// switches a pointer: nothing is allocated or destroyed.
//
// A transition requested while the current state is running, i.e. from
// inside its Update, takes effect only after Update returns, so a state never
// sees itself replaced mid-update. Transitions requested from outside take
// effect immediately. Entering a state calls its OnEnter.
//
// The machine points into its owner, so neither may be copied or moved;
// owners holding a StateMachine are therefore pinned too.
template <typename S>
class StateMachine
{
  public:
	StateMachine() = default;
	StateMachine(const StateMachine&) = delete;
	StateMachine& operator=(const StateMachine&) = delete;
	StateMachine(StateMachine&&) = delete;
	StateMachine& operator=(StateMachine&&) = delete;
	~StateMachine() = default;

	void TransitionTo(S& state) {
		if (updating_) {
			pending_ = &state;
			return;
		}
		Enter(state);
	}

	void Update(double delta_time) {
		updating_ = true;
		current_->Update(delta_time);
		updating_ = false;
		if (pending_ != nullptr) {
			Enter(*std::exchange(pending_, nullptr));
		}
	}

	S& Current() { return *current_; }
	const S& Current() const { return *current_; }

  private:
	void Enter(S& state) {
		current_ = &state;
		current_->OnEnter();
	}

	S* current_ = nullptr;
	S* pending_ = nullptr;
	bool updating_ = false;
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_STATE_H_
