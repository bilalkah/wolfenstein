#include "render/utils.h"
#include "base/types.h"
#include <vector>

namespace wolfenstein {

std::vector<vector2i> GenerateCirclePoints(vector2i center, int radius,
										   int num_points) {
	const double increment = 2 * M_PI / num_points;

	std::vector<vector2i> points;
	for (int i = 0; i < num_points; ++i) {
		double angle = i * increment;
		int x = static_cast<int>(center.x + radius * std::cos(angle));
		int y = static_cast<int>(center.y + radius * std::sin(angle));
		points.push_back({x, y});
	}

	return points;
}

}  // namespace wolfenstein
