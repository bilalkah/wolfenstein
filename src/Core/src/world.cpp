#include "Core/world.h"
#include "Strike/weapon.h"
#include <iostream>
#include <utility>

namespace wolfenstein {

std::expected<std::unique_ptr<World>, std::string> World::Create(
	const TextureManager& textures, const std::string& asset_dir) {
	auto loader = SceneLoader::Open(asset_dir);
	if (!loader) {
		return std::unexpected(loader.error());
	}
	// The game is playable without sound, so a missing audio device is a
	// warning, not an error
	auto sound = SoundManager::Open(asset_dir + "sounds/");
	if (!sound) {
		std::cerr << "Sound disabled: " << sound.error() << '\n';
		sound = std::make_unique<SoundManager>();
	}
	return std::make_unique<World>(textures, std::move(*loader),
								   std::move(*sound));
}

World::World(const TextureManager& textures, SceneLoader loader,
			 std::unique_ptr<SoundManager> sound)
	: textures_(textures),
	  loader_(std::move(loader)),
	  sound_(std::move(sound)),
	  level_memory_(loader_.LargestLevelMemory()) {}

std::expected<void, std::string> World::NewGame(std::string_view weapon_name,
												std::string_view level,
												std::string_view difficulty) {
	// In hand: the named weapon, or the last of those a game starts with
	const auto& arsenal = loader_.Config().weapons;
	std::size_t first = 0;
	if (weapon_name.empty()) {
		for (std::size_t i = 0; i < arsenal.size(); ++i) {
			first = arsenal[i].start ? i : first;
		}
	}
	else {
		const WeaponConfig* weapon = loader_.Config().FindWeapon(weapon_name);
		if (weapon == nullptr) {
			return std::unexpected("unknown weapon " +
								   std::string(weapon_name));
		}
		first = static_cast<std::size_t>(weapon - arsenal.data());
	}
	const DifficultyConfig* chosen =
		loader_.Config().FindDifficulty(difficulty);
	if (chosen == nullptr) {
		return std::unexpected("unknown difficulty " + std::string(difficulty));
	}
	difficulty_ = chosen;
	// The old level borrows the old player: it goes first
	scene_.reset();
	const CharacterStats& stats = loader_.Config().player;
	// The level sets the start position
	CharacterConfig config(Position2D(), stats.translation_speed,
						   stats.rotation_speed, stats.width, stats.height);
	player_.emplace(config, std::span(arsenal), first, textures_, *sound_);
	level_index_ = 0;
	in_campaign_ = level.empty();
	return StartLevel(in_campaign_
						  ? std::string_view(loader_.Config().levels.front())
						  : level);
}

std::expected<void, std::string> World::ContinueGame(const SavedGame& saved) {
	const GameConfig& config = loader_.Config();
	if (saved.level >= config.levels.size() ||
		saved.weapon >= config.weapons.size() ||
		saved.difficulty >= config.difficulties.size()) {
		return std::unexpected(
			std::string("the saved game does not fit this game's content"));
	}
	if (auto started = NewGame(config.weapons[saved.weapon].weapon_name, {},
							   config.difficulties[saved.difficulty].name);
		!started) {
		return started;
	}
	if (saved.level > 0) {
		level_index_ = saved.level;
		if (auto started = StartLevel(config.levels[saved.level]); !started) {
			return started;
		}
	}
	Player& player = GetPlayer();
	// Every weapon carried, with its rounds, and the one in hand
	player.SetOwnedWeapons(static_cast<std::uint8_t>(saved.weapons));
	for (std::size_t i = 0;
		 i < player.WeaponCount() && i < SavedGame::kMaxWeapons; ++i) {
		player.GetWeapon(i).SetRounds(saved.ammo[i], saved.reserve[i]);
	}
	player.TakeInHand(saved.weapon);
	player.Restore(saved.health, player.GetWeapon().GetAmmo(),
				   player.GetWeapon().GetReserve());
	if (!saved.has_position) {
		return {};	// saved as the level started
	}
	player.SetPosition(Position2D({saved.x, saved.y}, saved.theta));
	Scene& scene = CurrentLevel();
	const auto enemies = scene.GetEnemies();
	for (std::size_t i = 0; i < enemies.size() && i < 64; ++i) {
		if ((saved.killed >> i & 1U) != 0) {
			scene.RestoreKilled(i);
		}
	}
	const auto pickups = scene.GetPickups();
	for (std::size_t i = 0; i < pickups.size() && i < 64; ++i) {
		if ((saved.taken >> i & 1U) != 0) {
			pickups[i]->Take();
		}
	}
	const int size_x = scene.GetMap().GetSizeX();
	const int size_y = scene.GetMap().GetSizeY();
	if (saved.explored_cells ==
		static_cast<std::size_t>(size_x) * static_cast<std::size_t>(size_y)) {
		for (std::size_t cell = 0; cell < saved.explored_cells; ++cell) {
			if ((saved.explored[cell / 8] >> (cell % 8) & 1U) != 0) {
				scene.Explore(static_cast<int>(cell) / size_y,
							  static_cast<int>(cell) % size_y);
			}
		}
	}
	scene.RestoreSeconds(saved.seconds);
	const auto walls = scene.GetMap().GetPushWalls();
	for (std::size_t i = 0; i < walls.size() && i < 64; ++i) {
		if ((saved.secrets >> i & 1U) != 0) {
			scene.GetMap().Push(i, /*finish=*/true);
		}
	}
	player.SetKeys(static_cast<std::uint8_t>(saved.keys));
	return {};
}

std::optional<SavedGame> World::Capture() const {
	if (!in_campaign_ || !scene_ || !player_) {
		return std::nullopt;
	}
	const GameConfig& config = loader_.Config();
	const Position2D& position = player_->GetPosition();
	const Scene& scene = *scene_;
	SavedGame saved{.level = level_index_,
					.weapon = player_->HeldWeapon(),
					.health = player_->GetHealth(),
					.weapons = player_->GetOwnedWeapons(),
					.has_position = true,
					.x = position.pose.x,
					.y = position.pose.y,
					.theta = position.theta,
					.seconds = scene.GetStats().seconds,
					.keys = player_->GetKeys()};
	for (std::size_t i = 0;
		 i < player_->WeaponCount() && i < SavedGame::kMaxWeapons; ++i) {
		saved.ammo[i] = player_->GetWeapon(i).GetAmmo();
		saved.reserve[i] = player_->GetWeapon(i).GetReserve();
	}
	for (std::size_t i = 0; i < config.difficulties.size(); ++i) {
		if (&config.difficulties[i] == difficulty_) {
			saved.difficulty = i;
		}
	}
	const auto enemies = scene.GetEnemies();
	for (std::size_t i = 0; i < enemies.size() && i < 64; ++i) {
		// Falling counts as killed: its killing shot has been counted
		if (enemies[i]->GetHealth() <= 0.0) {
			saved.killed |= std::uint64_t{1} << i;
		}
	}
	const auto pickups = scene.GetPickups();
	for (std::size_t i = 0; i < pickups.size() && i < 64; ++i) {
		if (pickups[i]->IsTaken()) {
			saved.taken |= std::uint64_t{1} << i;
		}
	}
	const auto walls = scene.GetMap().GetPushWalls();
	for (std::size_t i = 0; i < walls.size() && i < 64; ++i) {
		if (walls[i].pushed) {
			saved.secrets |= std::uint64_t{1} << i;
		}
	}
	const int size_x = scene.GetMap().GetSizeX();
	const int size_y = scene.GetMap().GetSizeY();
	const auto cells =
		static_cast<std::size_t>(size_x) * static_cast<std::size_t>(size_y);
	if (cells <= SavedGame::kMaxExploredCells) {
		saved.explored_cells = cells;
		for (int x = 0; x < size_x; ++x) {
			for (int y = 0; y < size_y; ++y) {
				if (scene.IsExplored(x, y)) {
					const auto cell = (static_cast<std::size_t>(x) *
									   static_cast<std::size_t>(size_y)) +
									  static_cast<std::size_t>(y);
					saved.explored[cell / 8] |=
						static_cast<std::uint8_t>(1U << (cell % 8));
				}
			}
		}
	}
	return saved;
}

bool World::IsQuiet() const {
	constexpr double kCalmSeconds = 3.0;
	return scene_ && player_ && player_->IsAlive() &&
		   player_->SecondsSinceHurt() >= kCalmSeconds && scene_->IsQuiet();
}

std::expected<void, std::string> World::NextLevel() {
	if (!HasNextLevel()) {
		return std::unexpected("no next level");
	}
	++level_index_;
	return StartLevel(loader_.Config().levels[level_index_]);
}

bool World::HasNextLevel() const {
	return in_campaign_ && level_index_ + 1 < loader_.Config().levels.size();
}

std::expected<void, std::string> World::StartLevel(
	std::string_view level_file) {
	if (!player_) {
		return std::unexpected("no game started");
	}
	const PreparedLevel* level = loader_.FindLevel(level_file);
	if (level == nullptr) {
		return std::unexpected("unknown level " + std::string(level_file));
	}
	// The previous level lives in the arena: it goes before the arena is
	// reused for the next one
	scene_.reset();
	level_memory_.Reset();
	scene_.emplace(textures_, *sound_, level->map, level->capacity,
				   level_memory_);
	level_ = level;
	scene_->SetDifficulty({.enemy_damage = difficulty_->enemy_damage,
						   .enemy_health = difficulty_->enemy_health,
						   .supplies = difficulty_->supplies});
	return loader_.Populate(*scene_, *level, *player_);
}

}  // namespace wolfenstein
