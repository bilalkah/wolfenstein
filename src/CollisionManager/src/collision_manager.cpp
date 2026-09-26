#include "CollisionManager/collision_manager.h"

namespace wolfenstein {

bool CheckWallCollision(const Map& map, const vector2d& pose,
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

	return map.IsBlocked(vector2d{px, py});
}

bool CheckObjectCollision(std::span<IGameObject* const> objects,
						  const IGameObject* self, const vector2d& from,
						  const vector2d& to, double radius,
						  bool ignore_enemies) {
	for (const IGameObject* object : objects) {
		const double solid = object->GetCollisionRadius();
		if (object == self || solid <= 0.0 ||
			(ignore_enemies &&
			 object->GetObjectType() == ObjectType::CHARACTER_ENEMY)) {
			continue;
		}
		const vector2d centre = object->GetPose();
		const double reach = radius + solid;
		const double after = centre.Distance(to);
		if (after < reach && after < centre.Distance(from)) {
			return true;
		}
	}
	return false;
}

}  // namespace wolfenstein
