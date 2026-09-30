#include "Core/world.h"
#include "Strike/weapon.h"
#include <iostream>
#include <utility>

namespace karakale {

std::expected<std::unique_ptr<World>, std::string> World::Create(
	const TextureManager& textures, const std::string& asset_dir) {
	auto loader = SceneLoader::Open(asset_dir);
	if (!loader) {
		return std::unexpected(loader.error());
	}
	// The game is playable without sound, so a missing audio device is a
	// warning, not an error
	auto sound = SoundManager::Open(asset_dir + "sounds/", asset_dir + "music/",
									loader->MusicTracks());
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
												std::string_view difficulty,
												std::uint64_t seed) {
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
	seed_ = seed;
	// The old level borrows the old players: it goes first
	scene_.reset();
	for (std::optional<Player>& player : players_) {
		player.reset();
	}
	first_weapon_ = first;
	local_ = 0;
	MakePlayer(0);
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
	if (auto started =
			NewGame(config.weapons[saved.weapon].weapon_name, {},
					config.difficulties[saved.difficulty].name, saved.seed);
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
			scene.RestoreSecret(i);
		}
	}
	for (std::size_t i = 0; i < scene.GetIntel().size(); ++i) {
		if ((saved.intel >> i & 1U) != 0) {
			scene.RestoreRead(i);
		}
	}
	player.SetKeys(static_cast<std::uint8_t>(saved.keys));
	return {};
}

std::expected<void, std::string> World::NewMatch(
	std::string_view level, std::optional<std::size_t> local_slot) {
	if (local_slot && *local_slot >= players_.size()) {
		return std::unexpected("no player slot " + std::to_string(*local_slot));
	}
	// Everyone starts with the last of the weapons a game starts with
	const auto& arsenal = loader_.Config().weapons;
	std::size_t first = 0;
	for (std::size_t i = 0; i < arsenal.size(); ++i) {
		first = arsenal[i].start ? i : first;
	}
	difficulty_ = loader_.Config().FindDifficulty("normal");
	seed_ = 0;
	scene_.reset();
	for (std::optional<Player>& player : players_) {
		player.reset();
	}
	first_weapon_ = first;
	local_ = local_slot;
	if (local_) {
		MakePlayer(*local_);
	}
	level_index_ = 0;
	in_campaign_ = false;
	return StartLevel(level);
}

std::optional<SavedGame> World::Capture() const {
	const Player* player = LocalPlayer();
	if (!in_campaign_ || !scene_ || player == nullptr) {
		return std::nullopt;
	}
	const GameConfig& config = loader_.Config();
	const Position2D& position = player->GetPosition();
	const Scene& scene = *scene_;
	SavedGame saved{.level = level_index_,
					.weapon = player->HeldWeapon(),
					.seed = seed_,
					.health = player->GetHealth(),
					.weapons = player->GetOwnedWeapons(),
					.has_position = true,
					.x = position.pose.x,
					.y = position.pose.y,
					.theta = position.theta,
					.seconds = scene.GetStats().seconds,
					.keys = player->GetKeys()};
	for (std::size_t i = 0;
		 i < player->WeaponCount() && i < SavedGame::kMaxWeapons; ++i) {
		saved.ammo[i] = player->GetWeapon(i).GetAmmo();
		saved.reserve[i] = player->GetWeapon(i).GetReserve();
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
	const auto pages = scene.GetIntel();
	for (std::size_t i = 0; i < pages.size(); ++i) {
		if (pages[i].read) {
			saved.intel |= std::uint64_t{1} << i;
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
	const Player* player = LocalPlayer();
	return scene_ && player != nullptr && player->IsAlive() &&
		   player->SecondsSinceHurt() >= kCalmSeconds && scene_->IsQuiet();
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
	if (difficulty_ == nullptr) {
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
	Scene& scene = scene_.emplace(textures_, *sound_, level->map,
								  level->capacity, level_memory_);
	level_ = level;
	scene.SetDifficulty({.enemy_damage = difficulty_->enemy_damage,
						 .enemy_health = difficulty_->enemy_health,
						 .supplies = difficulty_->supplies,
						 .attackers = difficulty_->attackers});
	sound_->PlayMusic(level->data.music);
	// Every player comes in where the level brings their slot in, before
	// the level's objects, and the local one's view is the level's
	for (std::size_t slot = 0; slot < players_.size(); ++slot) {
		if (Player* player = FindPlayer(slot)) {
			Admit(scene, *player, slot);
		}
	}
	scene.SetViewer(local_.value_or(0));
	return loader_.Populate(scene, *level, seed_);
}

Player& World::MakePlayer(std::size_t slot) {
	const CharacterStats& stats = loader_.Config().player;
	// The level sets where it stands
	CharacterConfig config(Position2D(), stats.translation_speed,
						   stats.rotation_speed, stats.width, stats.height);
	return players_[slot].emplace(config, std::span(loader_.Config().weapons),
								  first_weapon_, textures_, *sound_);
}

void World::Admit(Scene& scene, Player& player, std::size_t slot) const {
	player.SetPosition(SpawnFor(slot));
	player.IncreaseHealth(100);
	player.SetKeys(0);	// the last level's keys open nothing here
	scene.SetPlayer(player, slot);
}

Position2D World::SpawnFor(std::size_t slot) const {
	const std::vector<Position2D>& spawns = level_->data.spawns;
	return spawns.empty() ? level_->data.player : spawns[slot % spawns.size()];
}

std::expected<void, std::string> World::JoinPlayer(std::size_t slot) {
	if (!scene_) {
		return std::unexpected("no game started");
	}
	if (slot >= players_.size() || players_[slot].has_value()) {
		return std::unexpected("no free player slot " + std::to_string(slot));
	}
	Admit(*scene_, MakePlayer(slot), slot);
	return {};
}

void World::LeavePlayer(std::size_t slot) {
	if (slot >= players_.size() || slot == local_ || !players_[slot]) {
		return;
	}
	// The level borrows it: it lets go first
	if (scene_) {
		scene_->RemovePlayer(slot);
	}
	players_[slot].reset();
}

Player* World::FindPlayer(std::size_t slot) {
	if (slot >= players_.size()) {
		return nullptr;
	}
	std::optional<Player>& player = players_[slot];
	return player.has_value() ? &*player : nullptr;
}

const Player* World::FindPlayer(std::size_t slot) const {
	if (slot >= players_.size()) {
		return nullptr;
	}
	const std::optional<Player>& player = players_[slot];
	return player.has_value() ? &*player : nullptr;
}

}  // namespace karakale
