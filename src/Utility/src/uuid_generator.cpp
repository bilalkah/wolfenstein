#include <Utility/uuid_generator.h>

namespace wolfenstein {

UuidGenerator& UuidGenerator::GetInstance() {
	static UuidGenerator instance;
	return instance;
}

UuidGenerator::UuidGenerator() : generator_(13U) {}

UUIDv4::UUID UuidGenerator::GenerateUuid() {
	return generator_.getUUID();
}

}  // namespace wolfenstein
