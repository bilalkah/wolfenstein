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

#ifndef GAME_OBJECTS_DYNAMIC_OBJECT_H
#define GAME_OBJECTS_DYNAMIC_OBJECT_H

#include "GameObjects/game_object.h"

namespace wolfenstein {

class DynamicObject : public IGameObject
{
  public:
	explicit DynamicObject(const vector2d& pose_, std::string texture_path_);
	~DynamicObject();

	void Update(double delta_time) override;

	void SetPose(const vector2d& pose) override;
	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;

  protected:
	// Dynamic object specific data
	vector2d pose;
	std::string texture_path;
};
}  // namespace wolfenstein

#endif	// GAME_OBJECTS_DYNAMIC_OBJECT_H
