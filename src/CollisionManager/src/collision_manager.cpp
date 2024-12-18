#include "CollisionManager/collision_manager.h"
#include "Core/scene.h"
#include <cassert>

namespace wolfenstein {

CollisionManager* CollisionManager::instance_ = nullptr;

CollisionManager& CollisionManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new CollisionManager();
	}
	return *instance_;
}

CollisionManager::~CollisionManager() {
	delete instance_;
}

void CollisionManager::InitManager(const std::shared_ptr<Scene>& scene_ptr) {
	scene_ptr_ = scene_ptr;
}

bool CollisionManager::CheckWallCollision(const vector2d& pose,
										  const vector2d& delta_pose) {
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

	return scene_ptr_->GetMap()[px][py] != 0;
}

}  // namespace wolfenstein
