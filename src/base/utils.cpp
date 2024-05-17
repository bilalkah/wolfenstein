#include "base/utils.h"
#include <cmath>

namespace wolfenstein {

double ToRadians(const double degree) {
	return degree * M_PI / 180.0;
}

double ToDegrees(const double radian) {
	return radian * 180.0 / M_PI;
}

}  // namespace wolfenstein
