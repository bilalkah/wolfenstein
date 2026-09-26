#include "GameObjects/pickup.h"

namespace wolfenstein {

Pickup::Pickup(const vector2d& pose, int texture_id, double width,
			   double height, PickupEffect effect)
	: pose_(pose),
	  texture_id_(texture_id),
	  width_(width),
	  height_(height),
	  effect_(effect) {}

}  // namespace wolfenstein
