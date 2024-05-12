#ifndef BASE_MACROS_H
#define BASE_MACROS_H

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

#endif	// BASE_MACROS_H