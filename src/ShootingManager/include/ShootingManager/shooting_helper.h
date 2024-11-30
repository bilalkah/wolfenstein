/**
 * @file shooting_helper.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-11-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef SHOOTING_MANAGER_INCLUDE_SHOOTING_MANAGER_SHOOTING_HELPER_H
#define SHOOTING_MANAGER_INCLUDE_SHOOTING_MANAGER_SHOOTING_HELPER_H

#include <cmath>

namespace wolfenstein {

inline double LinearSlope(const std::pair<double, double> damage_limits,
						  const double range, const double distance) {
	const auto linear_slope_formula =
		[](const std::pair<double, double> damage_limits, const double range,
		   const double distance) {
			return ((range - distance) / range) *
					   (damage_limits.first - damage_limits.second) +
				   damage_limits.second;
		};
	return linear_slope_formula(damage_limits, range, distance);
}

inline double ExponentialSlope(const std::pair<double, double> damage_limits,
							   const double range, const double distance) {

	const auto exp_slope_formula =
		[](const std::pair<double, double> damage_limits, const double range,
		   const double distance) {
			return std::fmin(std::fmax(std::exp((range - distance) / 1.5),
									   damage_limits.second),
							 damage_limits.first);
		};

	return exp_slope_formula(damage_limits, range, distance);
}

}  // namespace wolfenstein

#endif	// SHOOTING_MANAGER_INCLUDE_SHOOTING_MANAGER_SHOOTING_HELPER_H
