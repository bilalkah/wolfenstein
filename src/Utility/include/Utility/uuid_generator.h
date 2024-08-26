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

#include "uuid_v4/uuid_v4.h"

namespace wolfenstein {

class UuidGenerator
{
  public:
	static UuidGenerator& GetInstance();
	UuidGenerator(const UuidGenerator&) = delete;
	UuidGenerator& operator=(const UuidGenerator&) = delete;
	~UuidGenerator() = default;

	UUIDv4::UUID GenerateUuid();

  private:
	UuidGenerator();
	UUIDv4::UUIDGenerator<std::mt19937_64> generator_;
	static UuidGenerator* instance_;
};

}  // namespace wolfenstein

#endif	// UTILITY_INCLUDE_UTILITY_UUID_GENERATOR_H