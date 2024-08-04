#include "GameObjects/dynamic_object.h"

namespace wolfenstein {

DynamicObject::DynamicObject(const vector2d& pose_, std::string texture_path_)
	: pose(pose_), texture_path(texture_path_) {}

DynamicObject::~DynamicObject() {}

void DynamicObject::Update(double delta_time) {
	(void)delta_time;
	// Update dynamic object specific data
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

}  // namespace wolfenstein
