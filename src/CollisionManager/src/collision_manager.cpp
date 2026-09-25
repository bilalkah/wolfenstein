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

}  // namespace wolfenstein
