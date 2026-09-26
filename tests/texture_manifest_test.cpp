#include "TextureManager/texture_manager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace wolfenstein {
namespace {

// Every image the shipped manifest names exists, and the textures the code
// asks for by name are there: a typo fails here, not as a blank texture
TEST(TextureManifest, TheShippedManifestIsComplete) {
	std::ifstream file(std::string(RESOURCE_DIR) + "textures.json");
	const auto manifest = ParseTextureManifest(file);
	ASSERT_TRUE(manifest) << manifest.error();

	const auto exists = [](const std::string& path) {
		return std::filesystem::exists(std::string(RESOURCE_DIR) + path);
	};
	for (const auto& [name, path] : manifest->textures) {
		EXPECT_TRUE(exists(path)) << name << ": " << path;
	}
	for (const auto& path : manifest->walls) {
		EXPECT_TRUE(exists(path)) << path;
	}
	for (const auto& [name, frames] : manifest->clips) {
		for (const auto& path : frames) {
			EXPECT_TRUE(exists(path)) << name << ": " << path;
		}
	}

	for (const char* name : {"sky", "solid_black", "crosshair", "damage_taken",
							 "menu_background", "game_over", "win"}) {
		EXPECT_TRUE(std::ranges::any_of(
			manifest->textures,
			[name](const auto& texture) { return texture.first == name; }))
			<< name;
	}
	EXPECT_EQ(manifest->walls.size(), 5u);	// map cells 1 to 5
}

TEST(TextureManifest, RejectsAClipWithoutFrames) {
	std::istringstream input(
		R"({"textures": {}, "walls": [], "clips": {"empty": []}})");
	const auto manifest = ParseTextureManifest(input);
	ASSERT_FALSE(manifest);
	EXPECT_NE(manifest.error().find("empty"), std::string::npos);
}

}  // namespace
}  // namespace wolfenstein
