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

template <typename T>
class State
{
  public:
	virtual ~State() = default;
	void SetContext(std::shared_ptr<T> context) {
		context_ = context;
		OnContextSet();
	};
	virtual void OnContextSet() { /* Do nothing */ };
	virtual void Update(const double& delta_time) = 0;
	virtual void Reset() = 0;
	virtual void TransitionRequest(
		std::shared_ptr<State<T>>&) { /* Do nothing */ };
	virtual int GetCurrentFrame() const = 0;
	virtual StateType<T>::Type GetType() const = 0;

  protected:
	std::shared_ptr<T> context_;
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_STATE_H_
