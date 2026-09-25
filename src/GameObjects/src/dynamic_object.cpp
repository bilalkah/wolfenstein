#include "GameObjects/dynamic_object.h"

namespace wolfenstein {

DynamicObject::DynamicObject(const vector2d& pose_,
							 const LoopedAnimation& animation_,
							 const double width_, const double height_)
	: pose(pose_), animation(animation_), width(width_), height(height_) {}

void DynamicObject::Update(double delta_time) {
	animation.Update(delta_time);
}

void DynamicObject::SetPose(const vector2d& pose) {
	this->pose = pose;
}

ObjectType DynamicObject::GetObjectType() const {
	return ObjectType::DYNAMIC_OBJECT;
}

vector2d DynamicObject::GetPose() const {
	return pose;
}

int DynamicObject::GetTextureId() const {
	return animation.GetCurrentFrame();
}

double DynamicObject::GetWidth() const {
	return width;
}
double DynamicObject::GetHeight() const {
	return height;
}

}  // namespace wolfenstein
