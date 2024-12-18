/**
 * @file dynamic_object.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-18
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GAME_OBJECTS_INCLUDE_DYNAMIC_OBJECT_H
#define GAME_OBJECTS_INCLUDE_DYNAMIC_OBJECT_H

#include "Animation/animation.h"
#include "GameObjects/game_object.h"
#include <memory>

namespace wolfenstein {

class DynamicObject : public IGameObject
{
  public:
	explicit DynamicObject(const vector2d& pose_,
						   std::unique_ptr<IAnimation> animation_,
						   const double width_, const double height_);
	~DynamicObject();

	void Update(double delta_time) override;

	void SetPose(const vector2d& pose) override;
	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;
	std::string GetId() const override;
	int GetTextureId() const override;
	double GetWidth() const override;
	double GetHeight() const override;

  protected:
	vector2d pose;
	std::unique_ptr<IAnimation> animation;
	double width;
	double height;
	std::string id;
};
}  // namespace wolfenstein

#endif	// GAME_OBJECTS_INCLUDE_DYNAMIC_OBJECT_H
