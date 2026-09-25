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

  protected:
	// Copies and moves only through derived classes: copying through the
	// base would slice off the derived part
	IStrike() = default;
	IStrike(const IStrike&) = default;
	IStrike& operator=(const IStrike&) = default;
	IStrike(IStrike&&) = default;
	IStrike& operator=(IStrike&&) = default;

  public:
	virtual void Attack() = 0;
};

}  // namespace wolfenstein

#endif	// STRIKE_INCLUDE_STRIKE_STRIKE_H