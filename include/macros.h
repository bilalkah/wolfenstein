#include <iostream>

// Debug macro
#ifdef DEBUG
#define DEBUG_MSG(msg)                                                        \
  do                                                                          \
    {                                                                         \
      std::cerr << "[DEBUG] " << msg << std::endl;                            \
    }                                                                         \
  while(0)
#else
#define DEBUG_MSG(msg)                                                        \
  do                                                                          \
    {                                                                         \
    }                                                                         \
  while(0)
#endif

// Stdout macro
#define MSG(msg)                                                              \
  do                                                                          \
    {                                                                         \
      std::cout << msg << std::endl;                                          \
    }                                                                         \
  while(0)
