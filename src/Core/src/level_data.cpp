#include "Core/level_data.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string_view>
#include <utility>

namespace wolfenstein {

namespace {

using nlohmann::json;

// at() rather than operator[] throughout: a missing key is an error, not a
// null inserted into the tree
CharacterStats ToStats(const json& stats) {
	return {.translation_speed = stats.at("t_speed").get<double>(),
			.rotation_speed = stats.at("r_speed").get<double>(),
			.width = stats.at("width").get<double>(),
			.height = stats.at("height").get<double>()};
}

// [at point blank, at range]
std::pair<double, double> ToDamage(const json& damage) {
	return {damage.at(0).get<double>(), damage.at(1).get<double>()};
}

DamageFalloff ToFalloff(const json& falloff) {
	const auto name = falloff.get<std::string>();
	if (name == "linear") {
		return DamageFalloff::Linear;
	}
	if (name == "exponential") {
		return DamageFalloff::Exponential;
	}
	throw json::other_error::create(
		501, "unknown damage falloff \"" + name + "\"", &falloff);
}

WeaponConfig ToWeapon(const json& weapon) {
	const auto& reserve = weapon.at("reserve");
	return {.weapon_name = weapon.at("name").get<std::string>(),
			.label = weapon.at("label").get<std::string>(),
			.description = weapon.at("description").get<std::string>(),
			.ammo_capacity = weapon.at("ammo").get<std::size_t>(),
			.reserve_start = reserve.at("start").get<std::size_t>(),
			.reserve_max = reserve.at("max").get<std::size_t>(),
			.box_rounds = reserve.at("box").get<std::size_t>(),
			.attack_damage = ToDamage(weapon.at("damage")),
			.attack_range = weapon.at("range").get<double>(),
			.attack_speed = weapon.at("attack_speed").get<double>(),
			.reload_speed = weapon.at("reload_speed").get<double>(),
			.falloff = ToFalloff(weapon.at("falloff"))};
}

// A pickup gives health, ammo boxes or both; what it does not give is 0
PickupConfig ToPickup(const json& pickup) {
	PickupConfig config{
		.texture = pickup.at("texture").get<std::string>(),
		.width = pickup.at("width").get<double>(),
		.height = pickup.at("height").get<double>(),
		.effect = {.health = pickup.value("health", 0.0),
				   .ammo_boxes = pickup.value("ammo_boxes", std::size_t{0})}};
	if (config.effect.health <= 0.0 && config.effect.ammo_boxes == 0) {
		throw json::other_error::create(503, "a pickup that gives nothing",
										&pickup);
	}
	return config;
}

EnemyConfig ToEnemy(const std::string& type, const json& enemy) {
	const auto& weapon = enemy.at("weapon");
	const auto& ai = enemy.at("ai");
	return {.type = type,
			.translation_speed = enemy.at("t_speed").get<double>(),
			.width = enemy.at("width").get<double>(),
			.height = enemy.at("height").get<double>(),
			.behaviour = {.idle_frame_seconds =
							  ai.at("idle_frame_seconds").get<double>(),
						  .follow_range = ai.at("follow_range").get<double>()},
			.weapon = {.weapon_name = weapon.at("name").get<std::string>(),
					   .attack_damage = ToDamage(weapon.at("damage")),
					   .attack_range = weapon.at("range").get<double>(),
					   .attack_speed = weapon.at("attack_speed").get<double>(),
					   .attack_rate = weapon.at("attack_rate").get<double>()}};
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

const WeaponConfig* GameConfig::FindWeapon(std::string_view name) const {
	const auto found =
		std::ranges::find(weapons, name, &WeaponConfig::weapon_name);
	return found == weapons.end() ? nullptr : &*found;
}

const DifficultyConfig* GameConfig::FindDifficulty(
	std::string_view name) const {
	const auto found =
		std::ranges::find(difficulties, name, &DifficultyConfig::name);
	return found == difficulties.end() ? nullptr : &*found;
}

std::expected<GameConfig, std::string> ParseGameConfig(std::istream& input) {
	return Parse(input, [](const json& root) {
		GameConfig config;
		for (const auto& [type, enemy] : root.at("config_enemy").items()) {
			config.enemies.emplace(type, ToEnemy(type, enemy));
		}
		for (const auto& weapon : root.at("weapons")) {
			config.weapons.push_back(ToWeapon(weapon));
		}
		for (const auto& [type, pickup] : root.at("pickups").items()) {
			config.pickups.emplace(type, ToPickup(pickup));
		}
		config.levels = root.at("levels").get<std::vector<std::string>>();
		if (config.levels.empty()) {
			throw json::other_error::create(502, "no levels listed", &root);
		}
		config.benchmark_level = root.at("benchmark_level").get<std::string>();
		for (const auto& difficulty : root.at("difficulties")) {
			config.difficulties.push_back(
				{.name = difficulty.at("name").get<std::string>(),
				 .label = difficulty.at("label").get<std::string>(),
				 .description = difficulty.at("description").get<std::string>(),
				 .enemy_damage = difficulty.at("enemy_damage").get<double>(),
				 .enemy_health = difficulty.at("enemy_health").get<double>(),
				 .supplies = difficulty.at("supplies").get<double>()});
		}
		if (config.FindDifficulty("normal") == nullptr) {
			throw json::other_error::create(
				504, "no \"normal\" difficulty listed", &root);
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

namespace {

// Reads a level file straight into LevelData as the parser walks it (SAX),
// without building a JSON tree: loading a level allocates for the strings and
// spawn lists it keeps, not once per JSON value. Every field is checked when
// its object closes, so a missing or mistyped field is an error that names it.
class LevelReader final : public nlohmann::json_sax<json>
{
  public:
	explicit LevelReader(LevelData& level) : level_(level) {
		// Enough for any level so far; more simply grows the list
		level_.enemies.reserve(32);
		level_.dynamic_objects.reserve(32);
		level_.pickups.reserve(32);
	}

	const std::string& Error() const { return error_; }

	bool null() override { return Scalar("null"); }
	bool boolean(bool /*value*/) override { return Scalar("a boolean"); }
	bool number_integer(number_integer_t value) override {
		return Number(static_cast<double>(value));
	}
	bool number_unsigned(number_unsigned_t value) override {
		return Number(static_cast<double>(value));
	}
	bool number_float(number_float_t value, const string_t& /*text*/) override {
		return Number(value);
	}
	bool binary(binary_t& /*value*/) override { return Scalar("binary data"); }

	bool string(string_t& value) override {
		Frame& frame = Top();
		const auto take = [&](std::string& target, std::uint8_t field) {
			target = std::move(value);
			frame.seen |= field;
			return true;
		};
		switch (frame.kind) {
			case Kind::Skip:
				return true;
			case Kind::Root:
				if (frame.key == Key::Map) {
					return take(level_.map, kMap);
				}
				if (frame.key == Key::Name) {
					return take(level_.name, 0);
				}
				break;
			case Kind::Enemy:
				if (frame.key == Key::Type) {
					return take(level_.enemies.back().type, kType);
				}
				break;
			case Kind::Object:
				if (frame.key == Key::Type) {
					return take(level_.dynamic_objects.back().type, kType);
				}
				break;
			case Kind::Pickup:
				if (frame.key == Key::Type) {
					return take(level_.pickups.back().type, kType);
				}
				break;
			default:
				break;
		}
		if (frame.key == Key::Other) {
			return true;
		}
		return Fail("unexpected string");
	}

	bool start_object(std::size_t /*size*/) override {
		if (depth_ == 0) {
			return Push(Kind::Root);
		}
		const Frame& frame = Top();
		switch (frame.kind) {
			case Kind::Skip:
				return Push(Kind::Skip);
			case Kind::Root:
				if (frame.key == Key::Player) {
					return Push(Kind::Player);
				}
				break;
			case Kind::Player:
			case Kind::Enemy:
			case Kind::Object:
			case Kind::Pickup:
				if (frame.key == Key::Position) {
					return PushPosition(frame.kind);
				}
				break;
			case Kind::Enemies:
				level_.enemies.emplace_back();
				return Push(Kind::Enemy);
			case Kind::Objects:
				level_.dynamic_objects.emplace_back();
				return Push(Kind::Object);
			case Kind::Pickups:
				level_.pickups.emplace_back();
				return Push(Kind::Pickup);
			default:
				break;
		}
		return frame.key == Key::Other ? Push(Kind::Skip)
									   : Fail("unexpected object");
	}

	bool start_array(std::size_t /*size*/) override {
		const Frame& frame = Top();
		if (frame.kind == Kind::Root && frame.key == Key::Enemies) {
			return Push(Kind::Enemies);
		}
		if (frame.kind == Kind::Root && frame.key == Key::DynamicObjects) {
			return Push(Kind::Objects);
		}
		if (frame.kind == Kind::Root && frame.key == Key::Pickups) {
			return Push(Kind::Pickups);
		}
		if (frame.kind == Kind::Skip || frame.key == Key::Other) {
			return Push(Kind::Skip);
		}
		return Fail("unexpected array");
	}

	bool key(string_t& name) override {
		Frame& frame = Top();
		frame.key = KeyFor(frame.kind, name);
		return true;
	}

	bool end_object() override {
		const Frame frame = Pop();
		switch (frame.kind) {
			case Kind::Root:
				return Require(frame, kMap, "map") &&
					   Require(frame, kPlayer, "player") &&
					   Require(frame, kEnemies, "enemies") &&
					   Require(frame, kObjects, "dynamicObjects");
			case Kind::Player:
				Top().seen |= kPlayer;
				return Require(frame, kPosition, "player position");
			case Kind::Enemy:
				return Require(frame, kType, "enemy type") &&
					   Require(frame, kPosition, "enemy position");
			case Kind::Object:
				return Require(frame, kType, "object type") &&
					   Require(frame, kPosition, "object position");
			case Kind::Pickup:
				return Require(frame, kType, "pickup type") &&
					   Require(frame, kPosition, "pickup position");
			case Kind::Position:
				Top().seen |= kPosition;
				return Require(frame, frame.required, "x, y and theta");
			default:
				return true;
		}
	}

	bool end_array() override {
		const Frame frame = Pop();
		if (frame.kind == Kind::Enemies) {
			Top().seen |= kEnemies;
		}
		else if (frame.kind == Kind::Objects) {
			Top().seen |= kObjects;
		}
		return true;
	}

	bool parse_error(std::size_t /*position*/, const std::string& /*token*/,
					 const nlohmann::detail::exception& error) override {
		error_ = error.what();
		return false;
	}

  private:
	enum class Kind : std::uint8_t {
		Root,
		Player,
		Enemies,
		Enemy,
		Objects,
		Object,
		Pickups,
		Pickup,
		Position,
		Skip,  // a value the game does not read
	};
	enum class Key : std::uint8_t {
		None,
		Map,
		Name,
		Player,
		Enemies,
		DynamicObjects,
		Pickups,
		Type,
		Position,
		X,
		Y,
		Theta,
		Other,	// a key the game does not read; its value is skipped
	};
	// Fields seen in an object, as bits
	static constexpr std::uint8_t kMap = 1, kPlayer = 2, kEnemies = 4,
								  kObjects = 8, kType = 16, kPosition = 32,
								  kX = 1, kY = 2, kTheta = 4;

	struct Frame
	{
		Kind kind = Kind::Skip;
		Key key = Key::None;
		std::uint8_t seen = 0;
		std::uint8_t required = 0;	// Position: kX | kY, plus kTheta
		double* x = nullptr;
		double* y = nullptr;
		double* theta = nullptr;
	};

	static Key KeyFor(Kind kind, std::string_view name) {
		switch (kind) {
			case Kind::Root:
				if (name == "map")
					return Key::Map;
				if (name == "name")
					return Key::Name;
				if (name == "player")
					return Key::Player;
				if (name == "enemies")
					return Key::Enemies;
				if (name == "dynamicObjects")
					return Key::DynamicObjects;
				if (name == "pickups")
					return Key::Pickups;
				break;
			case Kind::Player:
				if (name == "position")
					return Key::Position;
				break;
			case Kind::Enemy:
			case Kind::Object:
			case Kind::Pickup:
				if (name == "type")
					return Key::Type;
				if (name == "position")
					return Key::Position;
				break;
			case Kind::Position:
				if (name == "x")
					return Key::X;
				if (name == "y")
					return Key::Y;
				if (name == "theta")
					return Key::Theta;
				break;
			default:
				break;
		}
		return Key::Other;
	}

	bool PushPosition(Kind owner) {
		Frame frame{.kind = Kind::Position, .required = kX | kY | kTheta};
		if (owner == Kind::Player) {
			frame.x = &level_.player.pose.x;
			frame.y = &level_.player.pose.y;
			frame.theta = &level_.player.theta;
		}
		else if (owner == Kind::Enemy) {
			Position2D& position = level_.enemies.back().position;
			frame.x = &position.pose.x;
			frame.y = &position.pose.y;
			frame.theta = &position.theta;
		}
		else {	// objects and pickups have no facing
			vector2d& position = owner == Kind::Pickup
									 ? level_.pickups.back().position
									 : level_.dynamic_objects.back().position;
			frame.x = &position.x;
			frame.y = &position.y;
			frame.required = kX | kY;
		}
		return Push(frame);
	}

	bool Number(double value) {
		Frame& frame = Top();
		if (frame.kind == Kind::Skip || frame.key == Key::Other) {
			return true;
		}
		if (frame.kind == Kind::Position) {
			if (frame.key == Key::X) {
				*frame.x = value;
				frame.seen |= kX;
				return true;
			}
			if (frame.key == Key::Y) {
				*frame.y = value;
				frame.seen |= kY;
				return true;
			}
			if (frame.key == Key::Theta && frame.theta != nullptr) {
				*frame.theta = value;
				frame.seen |= kTheta;
				return true;
			}
		}
		return Fail("unexpected number");
	}

	bool Scalar(const char* what) {
		const Frame& frame = Top();
		if (frame.kind == Kind::Skip || frame.key == Key::Other) {
			return true;
		}
		return Fail(std::string("unexpected ") + what);
	}

	bool Push(Kind kind) { return Push(Frame{.kind = kind}); }
	bool Push(const Frame& frame) {
		if (depth_ == stack_.size()) {
			return Fail("nested too deeply");
		}
		stack_[depth_++] = frame;
		return true;
	}
	Frame Pop() { return stack_[--depth_]; }
	Frame& Top() { return stack_[depth_ - 1]; }

	bool Require(const Frame& frame, std::uint8_t fields, const char* what) {
		if ((frame.seen & fields) == fields) {
			return true;
		}
		return Fail(std::string("missing ") + what);
	}
	bool Fail(std::string message) {
		error_ = std::move(message);
		return false;
	}

	LevelData& level_;
	std::array<Frame, 8> stack_{};
	std::size_t depth_ = 0;
	std::string error_;
};

}  // namespace

std::expected<LevelData, std::string> ParseLevel(std::istream& input) {
	LevelData level;
	LevelReader reader(level);
	if (!json::sax_parse(input, &reader)) {
		return std::unexpected(reader.Error());
	}
	return level;
}

}  // namespace wolfenstein
