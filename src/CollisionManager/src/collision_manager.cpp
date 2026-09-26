#include "CollisionManager/collision_manager.h"
#include <cmath>

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

vector2d PushOutOf(const vector2d& centre, double solid, const vector2d& from,
				   const vector2d& to, double radius) {
	const double reach = radius + solid;
	const double after = centre.Distance(to);
	if (after >= reach || after >= centre.Distance(from)) {
		return to;	// clear of it, or moving away from it
	}
	// Out along the line from its centre; if the step ends on the centre,
	// back the way it came
	vector2d away = after > 1e-9 ? to - centre : from - centre;
	const double length = std::hypot(away.x, away.y);
	if (length < 1e-9) {
		return from;
	}
	return centre + away * (reach / length);
}

vector2d ResolveObjectCollisions(std::span<IGameObject* const> objects,
								 const IGameObject* self, const vector2d& from,
								 const vector2d& to, double radius,
								 bool ignore_enemies) {
	// Pushed out of one thing, a body can end in another beside it: a
	// second pass settles that
	vector2d end = to;
	for (int pass = 0; pass < 2; ++pass) {
		for (const IGameObject* object : objects) {
			const double solid = object->GetCollisionRadius();
			if (object == self || solid <= 0.0 ||
				(ignore_enemies &&
				 object->GetObjectType() == ObjectType::CHARACTER_ENEMY)) {
				continue;
			}
			end = PushOutOf(object->GetPose(), solid, from, end, radius);
		}
	}
	return end;
}

}  // namespace wolfenstein
