// Runs once before every test: registers the animation clips that enemies
// and weapons look up. Tests have no renderer, so no image is loaded; the
// clips name placeholder texture ids.

#include "TextureManager/texture_manager.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <string>

namespace wolfenstein {
namespace {

class TestClips : public ::testing::Environment
{
  public:
	void SetUp() override {
		constexpr std::uint16_t kFramesPerClip = 3;
		std::uint16_t next = 0;
		const auto define = [&](const std::string& owner, const char* clip) {
			TextureManager::GetInstance().DefineCollection(
				owner + "_" + clip, next, next + kFramesPerClip);
			next += kFramesPerClip;
		};
		for (const char* enemy : {"soldier", "caco_demon", "cyber_demon"}) {
			for (const char* clip :
				 {"idle", "walk", "attack", "pain", "death"}) {
				define(enemy, clip);
			}
		}
		for (const char* weapon : {"mp5", "shotgun"}) {
			for (const char* clip : {"loaded", "outofammo", "reload"}) {
				define(weapon, clip);
			}
		}
	}
};

const auto* const kTestClips =
	::testing::AddGlobalTestEnvironment(new TestClips);

}  // namespace
}  // namespace wolfenstein
