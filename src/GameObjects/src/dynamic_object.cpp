#include <GameObjects/dynamic_object.h>
#include <Utility/uuid_generator.h>
#include <vector>

namespace wolfenstein {

DynamicObject::DynamicObject(const vector2d& pose_,
							 const std::vector<int> textures_id_,
							 const double width_, const double height_)
	: pose(pose_), textures_id(textures_id_), width(width_), height(height_) {
	id = UuidGenerator::GetInstance().GenerateUuid().bytes();
	texture_index = 0;
	counter = 0;
}

DynamicObject::~DynamicObject() {}

void DynamicObject::Update(double delta_time) {
	counter += delta_time;
	if (counter > 0.1) {
		texture_index = (texture_index + 1) % textures_id.size();
		counter = 0;
	}
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
	return textures_id[texture_index];
}

double DynamicObject::GetWidth() const {
	return width;
}
double DynamicObject::GetHeight() const {
	return height;
}

}  // namespace wolfenstein
