#include <GameObjects/dynamic_object.h>
#include <Utility/uuid_generator.h>
#include <vector>

namespace wolfenstein {

DynamicObject::DynamicObject(const vector2d& pose_,
							 const std::shared_ptr<Animation>& animation_,
							 const double width_, const double height_)
	: pose(pose_), animation(animation_), width(width_), height(height_) {
	id = UuidGenerator::GetInstance().GenerateUuid().bytes();
}

DynamicObject::~DynamicObject() {}

void DynamicObject::Update(double delta_time) {
	animation->Update(delta_time);
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

int DynamicObject::GetTextureId() const {
	return animation->GetCurrentFrame();
}

double DynamicObject::GetWidth() const {
	return width;
}
double DynamicObject::GetHeight() const {
	return height;
}

}  // namespace wolfenstein
