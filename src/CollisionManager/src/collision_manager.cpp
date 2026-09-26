#include "CollisionManager/collision_manager.h"
#include <cmath>

namespace wolfenstein {

bool CheckWallCollision(const Map& map, const vector2d& pose,
						const vector2d& delta_pose) {
	// The body is a square kCollisionDistance each way from its centre. The
	// edge it moves towards, where the move ends, must be clear at both its
	// corners: checking only its middle let a body cut into a wall's corner
	// coming at it at an angle. Only that edge is checked, so a body can
	// always move away from a wall it touches. The corners stand in a hair,
	// so a body flush with a wall along its side still slides along it.
	constexpr double kSide = kCollisionDistance * 0.99;
	const vector2d end = pose + delta_pose;
	const auto clear = [&](double x, double y) {
		return !map.IsBlocked(vector2d{x, y});
	};
	if (delta_pose.x != 0.0) {
		const double edge =
			end.x + std::copysign(kCollisionDistance, delta_pose.x);
		if (!clear(edge, end.y - kSide) || !clear(edge, end.y + kSide)) {
			return true;
		}
	}
	if (delta_pose.y != 0.0) {
		const double edge =
			end.y + std::copysign(kCollisionDistance, delta_pose.y);
		if (!clear(end.x - kSide, edge) || !clear(end.x + kSide, edge)) {
			return true;
		}
	}
	return false;
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
