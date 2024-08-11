#include "GameObjects/static_object.h"
#include "GameObjects/game_object.h"

namespace wolfenstein {

StaticObject::StaticObject(const vector2d& pose_, const int texture_id_,
						   const double width_, const double height_)
	: pose(pose_), texture_id(texture_id_), width(width_), height(height_) {}

StaticObject::~StaticObject() {}

void StaticObject::Update(double delta_time) {
	(void)delta_time;
}

void StaticObject::Render(Position2D camera_position) {
	(void)camera_position;
	// Render static object
}

void StaticObject::SetPose(const vector2d& pose) {
	this->pose = pose;
}

ObjectType StaticObject::GetObjectType() const {
	return ObjectType::STATIC_OBJECT;
}

vector2d StaticObject::GetPose() const {
	return pose;
}

int StaticObject::GetTextureId() const {
	return texture_id;
}
double StaticObject::GetWidth() const {
	return width;
}
double StaticObject::GetHeight() const {
	return height;
}

}  // namespace wolfenstein