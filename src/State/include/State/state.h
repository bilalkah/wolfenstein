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

#include <memory>

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
	void SetContext(T& context) {
		context_ = &context;
		OnContextSet();
	};
	virtual void OnContextSet() { /* Do nothing */ };
	virtual void Update(const double& delta_time) = 0;
	virtual void Reset() = 0;
	virtual int GetCurrentFrame() const = 0;
	virtual StateType<T>::Type GetType() const = 0;

  protected:
	T* context_ = nullptr;
};

// Owns the current state S (derived from State<T>) of an owner T.
//
// A transition requested while the current state is running, i.e. from
// inside its Update, is applied only after Update returns: replacing the
// state right away would destroy the object whose member function is still
// executing. Transitions requested from outside take effect immediately.
//
// The machine keeps a pointer to its owner, so neither may be copied or
// moved; owners holding a StateMachine are therefore pinned too.
template <typename T, typename S = State<T>>
class StateMachine
{
  public:
	explicit StateMachine(T& owner) : owner_(&owner) {}
	StateMachine(const StateMachine&) = delete;
	StateMachine& operator=(const StateMachine&) = delete;
	StateMachine(StateMachine&&) = delete;
	StateMachine& operator=(StateMachine&&) = delete;
	~StateMachine() = default;

	void TransitionTo(std::unique_ptr<S> state) {
		if (updating_) {
			pending_ = std::move(state);
			return;
		}
		Enter(std::move(state));
	}

	void Update(double delta_time) {
		updating_ = true;
		current_->Update(delta_time);
		updating_ = false;
		if (pending_) {
			Enter(std::move(pending_));
		}
	}

	S& Current() { return *current_; }
	const S& Current() const { return *current_; }

  private:
	void Enter(std::unique_ptr<S> state) {
		current_ = std::move(state);
		current_->SetContext(*owner_);
	}

	T* owner_;
	std::unique_ptr<S> current_;
	std::unique_ptr<S> pending_;
	bool updating_ = false;
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_STATE_H_
