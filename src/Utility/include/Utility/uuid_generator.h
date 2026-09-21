/**
 * @file uuid_generator.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-08-15
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef UTILITY_INCLUDE_UTILITY_UUID_GENERATOR_H
#define UTILITY_INCLUDE_UTILITY_UUID_GENERATOR_H

#include <cstdint>
#include <string>

namespace wolfenstein {

// Generates ids that are unique within a run. A plain counter replaces the
// previous uuid_v4 dependency, which only compiled on x86 (SSE2).
class UuidGenerator
{
  public:
	static UuidGenerator& GetInstance();
	UuidGenerator(const UuidGenerator&) = delete;
	UuidGenerator& operator=(const UuidGenerator&) = delete;
	~UuidGenerator() = default;

	std::string GenerateUuid();

  private:
	UuidGenerator() = default;
	uint64_t next_id_{0};
};

}  // namespace wolfenstein

#endif	// UTILITY_INCLUDE_UTILITY_UUID_GENERATOR_H
