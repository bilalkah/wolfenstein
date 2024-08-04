#include "CollisionManager/collision_manager.h"
#include <cassert>

namespace wolfenstein {

CollisionManager* CollisionManager::instance_ = nullptr;

CollisionManager& CollisionManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new CollisionManager();
	}
	return *instance_;
}

void CollisionManager::InitManager(std::shared_ptr<Map> map_ptr) {
	map_ptr_ = map_ptr;
	map_ = map_ptr->GetMap();
}

bool CollisionManager::CheckWallCollision(const vector2d& pose,
										  const vector2d& delta_pose) {
	assert(map_ptr_ != nullptr);
	auto px = pose.x;
	auto py = pose.y;

	if (delta_pose.x > 0) {
		px += kCollisionDistance;
	}
	else if (delta_pose.x < 0) {
		px -= kCollisionDistance;
	}

	if (delta_pose.y > 0) {
		py += kCollisionDistance;
	}
	else if (delta_pose.y < 0) {
		py -= kCollisionDistance;
	}

	return map_[px][py] != 0;
}

}  // namespace wolfenstein
