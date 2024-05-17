/**
 * @file i_base.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Base interface for all classes
 * @version 0.1
 * @date 2024-05-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BASE_I_BASE_H
#define BASE_I_BASE_H

namespace wolfenstein {

class ICopyable
{
  public:
	ICopyable(const ICopyable&) = default;
	ICopyable& operator=(const ICopyable&) = default;

	virtual ~ICopyable() = default;
};

class IMoveOnly
{
  public:
	IMoveOnly(IMoveOnly&&) = default;
	IMoveOnly& operator=(IMoveOnly&&) = default;

	IMoveOnly(const IMoveOnly&) = delete;
	IMoveOnly& operator=(const IMoveOnly&) = delete;

	virtual ~IMoveOnly() = default;
};

class INotCopyableOrMovable
{
  public:
	INotCopyableOrMovable() = default;

	INotCopyableOrMovable(const INotCopyableOrMovable&) = delete;
	INotCopyableOrMovable& operator=(const INotCopyableOrMovable&) = delete;

	INotCopyableOrMovable(INotCopyableOrMovable&&) = delete;
	INotCopyableOrMovable& operator=(INotCopyableOrMovable&&) = delete;

	virtual ~INotCopyableOrMovable() = default;
};

}  // namespace wolfenstein

#endif	// BASE_I_BASE_H
