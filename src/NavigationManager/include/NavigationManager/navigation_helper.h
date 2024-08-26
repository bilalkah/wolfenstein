/**
 * @file navigation_helper.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-08-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_NAVIGATION_HELPER_H
#define NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_NAVIGATION_HELPER_H

#include "Math/vector.h"
#include "path-planning/planning/utility/data_types.h"

namespace wolfenstein {

inline vector2d FromNode(const planning::Node& node) {
	return vector2d(node.x_, node.y_);
}

inline planning::Node FromVector2d(const vector2d& v) {
	return planning::Node(v.x, v.y);
}

}  // namespace wolfenstein

#endif	// NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_NAVIGATION_HELPER_H
