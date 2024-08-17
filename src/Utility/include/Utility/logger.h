/**
 * @file logger.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-19
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef UTILITIES_INCLUDE_UTILITIES_LOGGER_H
#define UTILITIES_INCLUDE_UTILITIES_LOGGER_H

// Debug macro
#ifdef DEBUG
#define DEBUG_MSG(msg)                               \
	do {                                             \
		std::cerr << "[DEBUG] " << msg << std::endl; \
	} while (0)
#else
#define DEBUG_MSG(msg) \
	do {               \
	} while (0)
#endif

// Stdout macro
#define MSG(msg)                       \
	do {                               \
		std::cout << msg << std::endl; \
	} while (0)

// Stderr macro
#define ERR(msg)                       \
	do {                               \
		std::cerr << msg << std::endl; \
	} while (0)

// Error macro
#define ERROR(msg)                     \
	do {                               \
		std::cerr << msg << std::endl; \
		exit(EXIT_FAILURE);            \
	} while (0)

// Assert macro
#define ASSERT(condition, msg) \
	do {                       \
		if (!(condition)) {    \
			ERROR(msg);        \
		}                      \
	} while (0)

#endif	// UTILITIES_INCLUDE_UTILITIES_LOGGER_H
