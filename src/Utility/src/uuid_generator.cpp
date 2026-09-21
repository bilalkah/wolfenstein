#include "Utility/uuid_generator.h"

namespace wolfenstein {

UuidGenerator& UuidGenerator::GetInstance() {
	static UuidGenerator instance;
	return instance;
}

std::string UuidGenerator::GenerateUuid() {
	return std::to_string(next_id_++);
}

}  // namespace wolfenstein
