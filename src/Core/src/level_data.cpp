#include "Core/level_data.h"
#include <nlohmann/json.hpp>

namespace wolfenstein {

namespace {

using nlohmann::json;

// at() rather than operator[] throughout: a missing key is an error, not a
// null inserted into the tree
Position2D ToPosition(const json& position) {
	return Position2D(
		{position.at("x").get<double>(), position.at("y").get<double>()},
		position.at("theta").get<double>());
}

CharacterStats ToStats(const json& stats) {
	return {.translation_speed = stats.at("t_speed").get<double>(),
			.rotation_speed = stats.at("r_speed").get<double>(),
			.width = stats.at("width").get<double>(),
			.height = stats.at("height").get<double>()};
}

vector2d ToPoint(const json& position) {
	return {position.at("x").get<double>(), position.at("y").get<double>()};
}

// Parses input and converts it with convert, turning every JSON error
// (syntax, missing key, wrong type) into an error message
template <typename Convert>
auto Parse(std::istream& input, Convert convert)
	-> std::expected<decltype(convert(json{})), std::string> {
	try {
		return convert(json::parse(input));
	}
	catch (const json::exception& error) {
		return std::unexpected(error.what());
	}
}

}  // namespace

std::expected<GameConfig, std::string> ParseGameConfig(std::istream& input) {
	return Parse(input, [](const json& root) {
		GameConfig config;
		for (const auto& [type, stats] : root.at("config_enemy").items()) {
			config.enemies.emplace(type, ToStats(stats));
		}
		config.player = ToStats(root.at("player_config"));
		const auto& light = root.at("config_dynamic").at("light");
		config.light = {
			.animation_speed = light.at("animation_speed").get<double>(),
			.width = light.at("width").get<double>(),
			.height = light.at("height").get<double>()};
		return config;
	});
}

std::expected<LevelData, std::string> ParseLevel(std::istream& input) {
	return Parse(input, [](const json& root) {
		LevelData level;
		level.map = root.at("map").get<std::string>();
		level.player = ToPosition(root.at("player").at("position"));
		const auto& enemies = root.at("enemies");
		level.enemies.reserve(enemies.size());
		for (const auto& enemy : enemies) {
			level.enemies.push_back({enemy.at("type").get<std::string>(),
									 ToPosition(enemy.at("position"))});
		}
		const auto& objects = root.at("dynamicObjects");
		level.dynamic_objects.reserve(objects.size());
		for (const auto& object : objects) {
			level.dynamic_objects.push_back(
				{object.at("type").get<std::string>(),
				 ToPoint(object.at("position"))});
		}
		level.next_level = root.value("next_level", std::string());
		return level;
	});
}

}  // namespace wolfenstein
