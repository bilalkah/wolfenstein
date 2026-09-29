#include "Core/level_data.h"
#include "GameMap/map.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

// Two numbers, [a, b]: a damage at point blank and at range, a range
std::pair<double, double> ToPair(const json& pair) {
	return {pair.at(0).get<double>(), pair.at(1).get<double>()};
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

// A sound, by the name config.json gives it ("sound": "pistol"): its file's
// name in assets/sounds
SoundEffect ToSound(const json& sound) {
	static constexpr std::array<std::pair<std::string_view, SoundEffect>, 26>
		kNames{{{"pistol", SoundEffect::PistolShot},
				{"mp5", SoundEffect::SmgShot},
				{"shotgun", SoundEffect::Shotgun},
				{"npc_attack", SoundEffect::NpcAttack},
				{"npc_pain", SoundEffect::NpcPain},
				{"npc_death", SoundEffect::NpcDeath},
				{"enemy_alert", SoundEffect::EnemyAlert},
				{"demon_attack", SoundEffect::DemonAttack},
				{"demon_alert", SoundEffect::DemonAlert},
				{"demon_pain", SoundEffect::DemonPain},
				{"demon_death", SoundEffect::DemonDeath},
				{"caco_alert", SoundEffect::CacoAlert},
				{"caco_death", SoundEffect::CacoDeath},
				{"cyber_alert", SoundEffect::CyberAlert},
				{"cyber_death", SoundEffect::CyberDeath},
				{"zombie_alert", SoundEffect::ZombieAlert},
				{"zombie_death", SoundEffect::ZombieDeath},
				{"super_shotgun", SoundEffect::SuperShotgun},
				{"super_shotgun_reload", SoundEffect::SuperShotgunReload},
				{"saw_up", SoundEffect::SawUp},
				{"saw", SoundEffect::Saw},
				{"saw_hit", SoundEffect::SawHit},
				{"rocket_launch", SoundEffect::RocketLaunch},
				{"rocket_burst", SoundEffect::RocketBurst},
				{"plasma", SoundEffect::Plasma},
				{"plasma_burst", SoundEffect::PlasmaBurst}}};
	const auto name = sound.get<std::string>();
	const auto found =
		std::ranges::find(kNames, std::string_view(name),
						  &std::pair<std::string_view, SoundEffect>::first);
	if (found == kNames.end()) {
		throw json::other_error::create(508, "unknown sound \"" + name + "\"",
										&sound);
	}
	return found->second;
}

// The sound `object` names under `key`, if it names one
std::optional<SoundEffect> ToOptionalSound(const json& object,
										   const char* key) {
	if (!object.contains(key)) {
		return std::nullopt;
	}
	return ToSound(object.at(key));
}

// An enemy type's voice: each of its sounds config.json names, the
// others as every enemy's
EnemySounds ToEnemySounds(const json& enemy) {
	EnemySounds sounds;
	if (!enemy.contains("sounds")) {
		return sounds;
	}
	const auto& named = enemy.at("sounds");
	for (auto [key, target] :
		 {std::pair{"attack", &sounds.attack},
		  std::pair{"alert", &sounds.alert}, std::pair{"pain", &sounds.pain},
		  std::pair{"death", &sounds.death}}) {
		if (named.contains(key)) {
			*target = ToSound(named.at(key));
		}
	}
	return sounds;
}

// What a weapon fires, if its shots fly ("projectile": {...})
std::optional<ProjectileConfig> ToProjectile(const json& weapon) {
	if (!weapon.contains("projectile")) {
		return std::nullopt;
	}
	const auto& projectile = weapon.at("projectile");
	return ProjectileConfig{
		.name = projectile.at("name").get<std::string>(),
		.speed = projectile.at("speed").get<double>(),
		.radius = projectile.value("radius", 0.1),
		.width = projectile.at("width").get<double>(),
		.height = projectile.at("height").get<double>(),
		.burst_width = projectile.at("burst_width").get<double>(),
		.burst_height = projectile.at("burst_height").get<double>(),
		.splash_radius = projectile.value("splash_radius", 0.0),
		.splash_damage = projectile.contains("splash_damage")
							 ? ToPair(projectile.at("splash_damage"))
							 : std::pair{0.0, 0.0},
		.burst_sound = ToOptionalSound(projectile, "burst_sound"),
		.noise_range = projectile.value("noise_range", 0)};
}

// Pages of story: [{"title": ..., "text": ...}]
std::vector<StoryText> ToStory(const json& pages) {
	std::vector<StoryText> story;
	for (const auto& page : pages) {
		story.push_back({.title = page.value("title", std::string{}),
						 .text = page.at("text").get<std::string>()});
	}
	return story;
}

// The campaign: its opening and ending, and its chapters, whose levels
// are added to `levels` in turn
Campaign ToCampaign(const json& campaign, std::vector<std::string>& levels) {
	Campaign parsed{.opening = campaign.contains("opening")
								   ? ToStory(campaign.at("opening"))
								   : std::vector<StoryText>{},
					.chapters = {},
					.ending = campaign.contains("ending")
								  ? ToStory(campaign.at("ending"))
								  : std::vector<StoryText>{}};
	for (const auto& chapter : campaign.at("chapters")) {
		const auto& files = chapter.at("levels");
		if (files.empty()) {
			throw json::other_error::create(512, "a chapter with no levels",
											&chapter);
		}
		parsed.chapters.push_back(
			{.title = chapter.at("title").get<std::string>(),
			 .name = chapter.at("name").get<std::string>(),
			 .text = chapter.value("text", std::string{}),
			 .first_level = levels.size(),
			 .level_count = files.size()});
		for (const auto& file : files) {
			levels.push_back(file.get<std::string>());
		}
	}
	return parsed;
}

WeaponConfig ToWeapon(const json& weapon) {
	const auto& reserve = weapon.at("reserve");
	return {.weapon_name = weapon.at("name").get<std::string>(),
			.label = weapon.at("label").get<std::string>(),
			.start = weapon.value("start", false),
			.ammo_capacity = weapon.at("ammo").get<std::size_t>(),
			.reserve_start = reserve.at("start").get<std::size_t>(),
			.reserve_max = reserve.at("max").get<std::size_t>(),
			.box_rounds = reserve.at("box").get<std::size_t>(),
			.attack_damage = ToPair(weapon.at("damage")),
			.attack_range = weapon.at("range").get<double>(),
			.attack_speed = weapon.at("attack_speed").get<double>(),
			.reload_speed = weapon.at("reload_speed").get<double>(),
			.falloff = ToFalloff(weapon.at("falloff")),
			.kick = weapon.value("kick", 0.0),
			.pellets = weapon.value("pellets", std::size_t{1}),
			.spread = weapon.value("spread", 0.0) * std::numbers::pi / 180.0,
			.raise_seconds = weapon.value("raise_seconds", 0.4),
			.lower_seconds = weapon.value("lower_seconds", 0.25),
			.shot_sound = ToOptionalSound(weapon, "sound"),
			.noise_range = weapon.value("noise_range", 0),
			.hit_sound = ToOptionalSound(weapon, "hit_sound"),
			.raise_sound = ToOptionalSound(weapon, "raise_sound"),
			.reload_sound = ToOptionalSound(weapon, "reload_sound"),
			.reload_after_shot = weapon.value("reload_after_shot", false),
			.projectile = ToProjectile(weapon)};
}

// The key a pickup is ("key": "gold"), as a KeyBit, or 0
std::uint8_t ToKeys(const json& pickup) {
	if (!pickup.contains("key")) {
		return 0;
	}
	const auto& key = pickup.at("key");
	const auto name = key.get<std::string>();
	if (name == "gold") {
		return KeyBit(KeyColour::Gold);
	}
	if (name == "silver") {
		return KeyBit(KeyColour::Silver);
	}
	throw json::other_error::create(505, "unknown key \"" + name + "\"", &key);
}

// A pickup gives health, ammo boxes, a key or a weapon (by name, as a bit
// of its index in `weapons`), or some of them; what it does not give is 0
PickupConfig ToPickup(const json& pickup,
					  const std::vector<WeaponConfig>& weapons) {
	PickupConfig config{
		.texture = pickup.at("texture").get<std::string>(),
		.width = pickup.at("width").get<double>(),
		.height = pickup.at("height").get<double>(),
		.effect = {.health = pickup.value("health", 0.0),
				   .ammo_boxes = pickup.value("ammo_boxes", std::size_t{0}),
				   .keys = ToKeys(pickup),
				   .weapons = 0,
				   .box_share = pickup.value("box_share", 1.0)}};
	if (pickup.contains("weapon")) {
		const auto& weapon = pickup.at("weapon");
		const auto name = weapon.get<std::string>();
		const auto found =
			std::ranges::find(weapons, name, &WeaponConfig::weapon_name);
		if (found == weapons.end()) {
			throw json::other_error::create(
				506, "unknown weapon \"" + name + "\"", &weapon);
		}
		config.effect.weapons = static_cast<std::uint8_t>(
			1U << static_cast<unsigned>(found - weapons.begin()));
	}
	if (config.effect.health <= 0.0 && config.effect.ammo_boxes == 0 &&
		config.effect.keys == 0 && config.effect.weapons == 0) {
		throw json::other_error::create(503, "a pickup that gives nothing",
										&pickup);
	}
	return config;
}

// Hit zones as config.json gives them ("hit_zones"), over `defaults`
HitZones ToHitZones(const json& zones, HitZones defaults) {
	return {.head_share = zones.value("head_share", defaults.head_share),
			.head_damage = zones.value("head_damage", defaults.head_damage),
			.leg_share = zones.value("leg_share", defaults.leg_share),
			.leg_damage = zones.value("leg_damage", defaults.leg_damage)};
}

// What an enemy type may drop ("drops": [{"pickup": "clip", "chance":
// 0.7}]), each with a chance from 0 to 1 (1 if it names none)
std::vector<EnemyDrop> ToDrops(const json& enemy) {
	std::vector<EnemyDrop> drops;
	if (!enemy.contains("drops")) {
		return drops;
	}
	const auto& listed = enemy.at("drops");
	for (const auto& drop : listed) {
		EnemyDrop parsed{.pickup = drop.at("pickup").get<std::string>(),
						 .chance = drop.value("chance", 1.0)};
		if (parsed.chance < 0.0 || parsed.chance > 1.0) {
			throw json::other_error::create(
				510, "a drop's chance is from 0 to 1", &drop);
		}
		drops.push_back(std::move(parsed));
	}
	if (drops.size() > Enemy::kMaxDrops) {
		throw json::other_error::create(511,
										"an enemy drops at most " +
											std::to_string(Enemy::kMaxDrops) +
											" things",
										&listed);
	}
	return drops;
}

EnemyConfig ToEnemy(const std::string& type, const json& enemy,
					const HitZones& zones) {
	const auto& weapon = enemy.at("weapon");
	const auto& ai = enemy.at("ai");
	const auto width = enemy.at("width").get<double>();
	// How far from the player it fights ("range": [near, far])
	const auto range =
		ai.contains("range") ? ToPair(ai.at("range")) : std::pair{0.0, 1.5};
	if (range.first < 0.0 || range.second < range.first) {
		throw json::other_error::create(
			509, "an enemy's range is [near, far], near <= far",
			&ai.at("range"));
	}
	return {.type = type,
			.translation_speed = enemy.at("t_speed").get<double>(),
			.width = width,
			.height = enemy.at("height").get<double>(),
			.radius = enemy.value("radius", width / 2),
			.health = enemy.value("health", 100.0),
			.behaviour = {.idle_frame_seconds =
							  ai.at("idle_frame_seconds").get<double>(),
						  .follow_range = ai.at("follow_range").get<double>(),
						  .pain_cooldown_seconds =
							  ai.value("pain_cooldown_seconds", 1.2),
						  .alert_seconds = ai.value("alert_seconds", 10.0),
						  .patrol_pace = ai.value("patrol_pace", 0.5),
						  .cry_range = ai.value("cry_range", 6),
						  .near_range = range.first,
						  .far_range = range.second,
						  .sidestep = ai.value("sidestep", 0.0),
						  .retreat_below = ai.value("retreat_below", 0.0)},
			.hit_zones = enemy.contains("hit_zones")
							 ? ToHitZones(enemy.at("hit_zones"), zones)
							 : zones,
			.drops = ToDrops(enemy),
			.weapon = {.weapon_name = weapon.at("name").get<std::string>(),
					   .attack_damage = ToPair(weapon.at("damage")),
					   .attack_range = weapon.at("range").get<double>(),
					   .attack_speed = weapon.at("attack_speed").get<double>(),
					   .attack_rate = weapon.at("attack_rate").get<double>(),
					   .noise_range = weapon.value("noise_range", 0)},
			.sounds = ToEnemySounds(enemy)};
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

const Chapter* Campaign::ChapterOpenedBy(std::size_t level) const {
	const auto found =
		std::ranges::find(chapters, level, &Chapter::first_level);
	return found == chapters.end() ? nullptr : &*found;
}

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
		const HitZones zones = root.contains("hit_zones")
								   ? ToHitZones(root.at("hit_zones"), {})
								   : HitZones{};
		for (const auto& [type, enemy] : root.at("config_enemy").items()) {
			config.enemies.emplace(type, ToEnemy(type, enemy, zones));
		}
		for (const auto& weapon : root.at("weapons")) {
			config.weapons.push_back(ToWeapon(weapon));
		}
		// A weapon is a bit of a player's set: there is room for eight
		if (config.weapons.empty() || config.weapons.size() > 8) {
			throw json::other_error::create(507, "one to eight weapons", &root);
		}
		for (const auto& [type, pickup] : root.at("pickups").items()) {
			config.pickups.emplace(type, ToPickup(pickup, config.weapons));
		}
		// The campaign's levels: its chapters' in turn, or a plain list
		if (root.contains("campaign")) {
			config.campaign = ToCampaign(root.at("campaign"), config.levels);
		}
		else {
			config.levels = root.at("levels").get<std::vector<std::string>>();
		}
		if (config.levels.empty()) {
			throw json::other_error::create(502, "no levels listed", &root);
		}
		config.benchmark_level = root.at("benchmark_level").get<std::string>();
		if (root.contains("arenas")) {
			config.arenas = root.at("arenas").get<std::vector<std::string>>();
		}
		for (const auto& name : root.value("gun_race", json::array())) {
			const WeaponConfig* weapon =
				config.FindWeapon(name.get<std::string>());
			if (weapon == nullptr) {
				throw json::other_error::create(
					508, "gun_race: no weapon " + name.get<std::string>(),
					&root);
			}
			config.gun_race.push_back(
				static_cast<std::size_t>(weapon - config.weapons.data()));
		}
		config.menu_music = root.value("menu_music", std::string{});
		for (const auto& difficulty : root.at("difficulties")) {
			config.difficulties.push_back(
				{.name = difficulty.at("name").get<std::string>(),
				 .label = difficulty.at("label").get<std::string>(),
				 .description = difficulty.at("description").get<std::string>(),
				 .enemy_damage = difficulty.at("enemy_damage").get<double>(),
				 .enemy_health = difficulty.at("enemy_health").get<double>(),
				 .supplies = difficulty.at("supplies").get<double>(),
				 .attackers = difficulty.value("attackers", 3)});
		}
		if (config.FindDifficulty("normal") == nullptr) {
			throw json::other_error::create(
				504, "no \"normal\" difficulty listed", &root);
		}
		config.player = ToStats(root.at("player_config"));
		const auto& figure = root.at("player_config").at("figure");
		config.player_figure = {.clips = figure.at("clips").get<std::string>(),
								.width = figure.at("width").get<double>(),
								.height = figure.at("height").get<double>(),
								.zones = zones};
		const auto& light = root.at("config_dynamic").at("light");
		config.light = {
			.animation_speed = light.at("animation_speed").get<double>(),
			.width = light.at("width").get<double>(),
			.height = light.at("height").get<double>(),
			.radius = light.at("radius").get<double>()};
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
		level_.objectives.reserve(4);
		level_.secrets.reserve(4);
		level_.intel.reserve(4);
		level_.spawns.reserve(16);
	}

	const std::string& Error() const { return error_; }

	bool null() override { return Scalar("null"); }
	bool boolean(bool value) override {
		const Frame& frame = Top();
		if (frame.kind == Kind::Enemy && frame.key == Key::Target) {
			level_.enemies.back().target = value;
			return true;
		}
		return Scalar("a boolean");
	}
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
				if (frame.key == Key::Briefing) {
					return take(level_.briefing, 0);
				}
				if (frame.key == Key::Debrief) {
					return take(level_.debrief, 0);
				}
				if (frame.key == Key::Music) {
					return take(level_.music, 0);
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
			case Kind::Intel:
				if (frame.key == Key::Title) {
					return take(level_.intel.back().title, kTitle);
				}
				if (frame.key == Key::Text) {
					return take(level_.intel.back().text, kText);
				}
				break;
			case Kind::Objective:
				if (frame.key == Key::Type) {
					Objective& objective = level_.objectives.back();
					if (value == "kill_all") {
						objective.type = Objective::Type::KillAll;
					}
					else if (value == "kill_targets") {
						objective.type = Objective::Type::KillTargets;
					}
					else {
						return Fail("unknown objective \"" + value + "\"");
					}
					frame.seen |= kType;
					return true;
				}
				if (frame.key == Key::Text) {
					return take(level_.objectives.back().text, kText);
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
			case Kind::Objectives:
				level_.objectives.emplace_back();
				return Push(Kind::Objective);
			case Kind::Secrets:
				level_.secrets.emplace_back();
				return Push(Kind::Secret);
			case Kind::Pages:
				level_.intel.emplace_back();
				return Push(Kind::Intel);
			case Kind::Spawns:
				level_.spawns.emplace_back();
				return PushPosition(Kind::Spawns);
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
		if (frame.kind == Kind::Root && frame.key == Key::Objectives) {
			return Push(Kind::Objectives);
		}
		if (frame.kind == Kind::Root && frame.key == Key::Secrets) {
			return Push(Kind::Secrets);
		}
		if (frame.kind == Kind::Root && frame.key == Key::Intel) {
			return Push(Kind::Pages);
		}
		if (frame.kind == Kind::Root && frame.key == Key::Spawns) {
			return Push(Kind::Spawns);
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
			case Kind::Objective:
				return Require(frame, kType, "objective type") &&
					   Require(frame, kText, "objective text");
			case Kind::Secret: {
				const SecretSpawn& secret = level_.secrets.back();
				if (std::abs(secret.dx) + std::abs(secret.dy) != 1) {
					return Fail("a secret moves one cell along x or y");
				}
				return Require(frame, kX | kY, "secret x and y");
			}
			case Kind::Intel: {
				const IntelSpawn& page = level_.intel.back();
				if (std::abs(page.dx) + std::abs(page.dy) != 1) {
					return Fail("a page of intel faces along x or y");
				}
				return Require(frame, kX | kY, "intel x and y") &&
					   Require(frame, kTitle | kText, "intel title and text");
			}
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
		Objectives,
		Objective,
		Secrets,
		Secret,
		Pages,	// the intel list
		Intel,
		Spawns,
		Position,
		Skip,  // a value the game does not read
	};
	enum class Key : std::uint8_t {
		None,
		Map,
		Name,
		Briefing,
		Debrief,
		Music,
		Player,
		Enemies,
		DynamicObjects,
		Pickups,
		Objectives,
		Secrets,
		Intel,
		Spawns,
		Type,
		Title,
		Text,
		Target,
		PatrolRadius,
		Dx,
		Dy,
		Position,
		X,
		Y,
		Theta,
		Other,	// a key the game does not read; its value is skipped
	};
	// Fields seen in an object, as bits
	static constexpr std::uint8_t kMap = 1, kPlayer = 2, kEnemies = 4,
								  kObjects = 8, kType = 16, kPosition = 32,
								  kText = 64, kTitle = 128, kX = 1, kY = 2,
								  kTheta = 4;

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
				if (name == "briefing")
					return Key::Briefing;
				if (name == "debrief")
					return Key::Debrief;
				if (name == "music")
					return Key::Music;
				if (name == "player")
					return Key::Player;
				if (name == "enemies")
					return Key::Enemies;
				if (name == "dynamicObjects")
					return Key::DynamicObjects;
				if (name == "pickups")
					return Key::Pickups;
				if (name == "objectives")
					return Key::Objectives;
				if (name == "secrets")
					return Key::Secrets;
				if (name == "intel")
					return Key::Intel;
				if (name == "spawns")
					return Key::Spawns;
				break;
			case Kind::Player:
				if (name == "position")
					return Key::Position;
				break;
			case Kind::Intel:
				if (name == "title")
					return Key::Title;
				if (name == "text")
					return Key::Text;
				[[fallthrough]];
			case Kind::Secret:
				if (name == "x")
					return Key::X;
				if (name == "y")
					return Key::Y;
				if (name == "dx")
					return Key::Dx;
				if (name == "dy")
					return Key::Dy;
				break;
			case Kind::Objective:
				if (name == "type")
					return Key::Type;
				if (name == "text")
					return Key::Text;
				break;
			case Kind::Enemy:
				if (name == "target")
					return Key::Target;
				if (name == "patrol_radius")
					return Key::PatrolRadius;
				[[fallthrough]];
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
		if (owner == Kind::Player || owner == Kind::Spawns) {
			Position2D& position =
				owner == Kind::Player ? level_.player : level_.spawns.back();
			frame.x = &position.pose.x;
			frame.y = &position.pose.y;
			frame.theta = &position.theta;
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
		if (frame.kind == Kind::Enemy && frame.key == Key::PatrolRadius) {
			if (value < 0.0) {
				return Fail("a patrol radius cannot be negative");
			}
			level_.enemies.back().patrol_radius = value;
			return true;
		}
		if (frame.kind == Kind::Secret) {
			SecretSpawn& secret = level_.secrets.back();
			if (TakeCell(frame, value, secret.x, secret.y, secret.dx,
						 secret.dy)) {
				return true;
			}
		}
		if (frame.kind == Kind::Intel) {
			IntelSpawn& page = level_.intel.back();
			if (TakeCell(frame, value, page.x, page.y, page.dx, page.dy)) {
				return true;
			}
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

	// A wall cell and a way along x or y (a secret's, a page of intel's):
	// the field the frame's key names, if it is one of them
	static bool TakeCell(Frame& frame, double value, int& x, int& y, int& dx,
						 int& dy) {
		const int number = static_cast<int>(value);
		switch (frame.key) {
			case Key::X:
				x = number;
				frame.seen |= kX;
				return true;
			case Key::Y:
				y = number;
				frame.seen |= kY;
				return true;
			case Key::Dx:
				dx = number;
				return true;
			case Key::Dy:
				dy = number;
				return true;
			default:
				return false;
		}
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
