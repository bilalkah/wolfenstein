/**
 * @file static_object.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Static object class derived from game object
 * @version 0.1
 * @date 2024-07-18
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GAME_OBJECTS_INCLUDE_STATIC_OBJECT_H
#define GAME_OBJECTS_INCLUDE_STATIC_OBJECT_H

#include <Characters/character.h>
#include <GameObjects/game_object.h>
#include <Math/vector.h>

namespace wolfenstein {

class StaticObject : public IGameObject
{
  public:
	explicit StaticObject(const vector2d& pose_, const int texture_id_,
						  const double width_, const double height_);
	~StaticObject();

	void Update(double delta_time) override;

	void SetPose(const vector2d& pose) override;

	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;
	std::string GetId() const override;

	int GetTextureId() const;
	double GetWidth() const;
	double GetHeight() const;

  protected:
	vector2d pose;
	int texture_id;
	double width;
	double height;
	std::string id;
};

}  // namespace wolfenstein

#endif	// GAME_OBJECTS_INCLUDE_STATIC_OBJECT_H