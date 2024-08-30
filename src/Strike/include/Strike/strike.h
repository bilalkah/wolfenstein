/**
 * @file strike.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-08-29
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STRIKE_INCLUDE_STRIKE_STRIKE_H
#define STRIKE_INCLUDE_STRIKE_STRIKE_H

namespace wolfenstein {

class IStrike
{
  public:
	virtual ~IStrike() = default;

	virtual void Attack() = 0;
};

}  // namespace wolfenstein

#endif	// STRIKE_INCLUDE_STRIKE_STRIKE_H