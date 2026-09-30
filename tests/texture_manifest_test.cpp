#include "GameMap/map.h"
#include "TextureManager/texture_manager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace karakale {
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
	// Map cells 1 to 5, and the exit switch
	EXPECT_EQ(manifest->walls.size(), std::size_t{Map::kExitWall});
}

// A clip that turns ("soldier_walk@2") is seen from all 8 sides, each as
// long as its front: an animation playing it expects every side
TEST(TextureManifest, EveryTurningClipHasEverySide) {
	std::ifstream file(std::string(RESOURCE_DIR) + "textures.json");
	const auto manifest = ParseTextureManifest(file);
	ASSERT_TRUE(manifest) << manifest.error();
	const auto find =
		[&](const std::string& name) -> const std::vector<std::string>* {
		const auto clip = std::ranges::find(
			manifest->clips, name, [](const auto& c) { return c.first; });
		return clip == manifest->clips.end() ? nullptr : &clip->second;
	};
	int turning = 0;
	for (const auto& [name, frames] : manifest->clips) {
		if (name.find('@') != std::string::npos || !find(name + "@2")) {
			continue;
		}
		++turning;
		for (int view = 2; view <= 8; ++view) {
			const auto* side = find(name + "@" + std::to_string(view));
			ASSERT_NE(side, nullptr) << name << "@" << view;
			EXPECT_EQ(side->size(), frames.size()) << name << "@" << view;
		}
	}
	EXPECT_GT(turning, 0) << "the enemies turn";
}

TEST(TextureManifest, RejectsAClipWithoutFrames) {
	std::istringstream input(
		R"({"textures": {}, "walls": [], "clips": {"empty": []}})");
	const auto manifest = ParseTextureManifest(input);
	ASSERT_FALSE(manifest);
	EXPECT_NE(manifest.error().find("empty"), std::string::npos);
}

}  // namespace
}  // namespace karakale
