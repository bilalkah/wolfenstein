#include <GameObjects/dynamic_object.h>
#include <Utility/uuid_generator.h>

namespace wolfenstein {

DynamicObject::DynamicObject(const vector2d& pose_, std::string texture_path_)
	: pose(pose_), texture_path(texture_path_) {
	id = UuidGenerator::GetInstance().GenerateUuid().bytes();
}

DynamicObject::~DynamicObject() {}

void DynamicObject::Update(double delta_time) {
	(void)delta_time;
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

std::string DynamicObject::GetId() const {
	return id;
}

}  // namespace wolfenstein
