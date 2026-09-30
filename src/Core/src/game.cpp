#include "Core/game.h"
#include "Animation/looped_animation.h"
#include "Camera/raycaster.h"
#include "Characters/enemy.h"
#include "GameObjects/dynamic_object.h"
#include "GameObjects/static_object.h"
#include "Math/vector.h"
#include "Profiler/profiler.h"
#include "Settings/saved_game.h"
#include "Settings/settings.h"
#include "ShootingManager/shooting_manager.h"
#include "SoundManager/sound_manager.h"
#include "State/enemy_state.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <numbers>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace karakale {

namespace {

#ifdef __EMSCRIPTEN__
EM_JS_DEPS(page, "$stringToNewUTF8");
// clang-format off
EM_JS(char*, PageAddress, (), {
	return stringToNewUTF8(location.origin + location.pathname);
});
// clang-format on
#endif

// The page this game is played on (a native game: the web game's)
std::string GamePage() {
#ifdef __EMSCRIPTEN__
	char* page = PageAddress();
	std::string address(page);
	std::free(page);
	return address;
#else
	// A native game's invites open the web game
	return "https://bilalkah.github.io/karakale/play/";
#endif
}

// Time between the end of a level (or death) and what follows it
constexpr double kEndOfLevelDelay = 2.0;
// A cleared level: a moment to take it in, then a fade to black, the next
// level built at black, and a fade back in under its title
constexpr double kClearedPause = 1.0;
constexpr double kFadeOutSeconds = 0.8;
// The results screen ignores input this long, so a held key or button does
// not skip it; scripted runs move on by themselves after a second
constexpr double kStatsInputDelay = 0.6;
constexpr double kScriptedStatsSeconds = 1.0;
// A scripted run turns each page of story quickly: it only needs them drawn
constexpr double kScriptedStoryPageSeconds = 0.1;
constexpr double kFadeInSeconds = 1.0;
// The level's title stays up a little longer than the fade, then fades too
constexpr double kBannerSeconds = 2.4;
constexpr double kBannerFadeSeconds = 0.6;
// Frames the soak session spends at the main menu before starting a game
constexpr int kSoakMenuFrames = 60;
// Wall-free route through level1's map as (row, column) waypoints: the
// benchmark's player walks it at normal speed facing forward, then walks
// it back
constexpr std::array<std::pair<double, double>, 7> kBenchmarkRoute = {{
	{3.0, 1.5},
	{9.5, 1.5},
	{9.5, 13.5},
	{16.5, 13.5},
	{16.5, 8.5},
	{23.5, 8.5},
	{23.5, 1.5},
}};

// Scripted runs strike walls as a player's shots do, so their frames draw
// bullet marks too: a level shot from `from` at `theta`
void StrikeWall(Scene& scene, const vector2d& from, double theta) {
	constexpr double kReach = 10.0;	 // as far as a shot carries
	MarkWall(scene,
			 CastRay(scene.GetMap(), Position2D(from, theta), theta, kReach),
			 0.0);
}

}  // namespace

Game::Game(GeneralConfig& config) : config_(config) {
	const auto init_start = std::chrono::steady_clock::now();
	Init();
	const std::chrono::duration<double, std::milli> init_time =
		std::chrono::steady_clock::now() - init_start;
	Profiler::GetInstance().AddStartupTime(init_time.count());
}

void Game::Init() {
	// The player's view is set with the other settings, below
	Camera2DConfig camera_config = {config_.screen_width, config_.base_fov,
									config_.view_distance};
	camera_ = std::make_unique<Camera2D>(camera_config);

	RenderConfig render_config = {config_.screen_width, config_.screen_height,
								  config_.padding,		config_.scale,
								  config_.fps,			config_.view_distance,
								  config_.base_fov,		config_.fullscreen};

	renderer_context_ = std::make_unique<RendererContext>(
		"Last Signal from Karakale", render_config, *camera_);
	auto world = World::Create(renderer_context_->Textures(), RESOURCE_DIR);
	if (!world) {
		std::cerr << "Cannot start the game: " << world.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	world_ = std::move(*world);
	// Room for the largest level's objects, so switching levels does not
	// grow the camera's per-object views
	camera_->ReserveViews(world_->LargestLevelObjects());
	for (const DifficultyConfig& difficulty : world_->Config().difficulties) {
		difficulty_choices_.push_back(
			{.label = difficulty.label, .description = difficulty.description});
	}
	menu_ = std::make_unique<Menu>(*renderer_context_, difficulty_choices_);
	// A saved game made with other content (fewer levels, weapons or
	// difficulties) cannot be gone on with
	saved_game_ = SavedGame::Load();
	const GameConfig& config = world_->Config();
	if (saved_game_ &&
		(saved_game_->level >= config.levels.size() ||
		 saved_game_->weapon >= config.weapons.size() ||
		 saved_game_->difficulty >= config.difficulties.size())) {
		saved_game_.reset();
	}
	DescribeSavedGame();
	renderer_3d_ = std::make_unique<Renderer3D>(*renderer_context_);
	renderer_3d_->ReserveObjects(world_->LargestLevelObjects());
	renderer_2d_ = std::make_unique<Renderer2D>(*renderer_context_);
	minimap_ = std::make_unique<Minimap>(*renderer_context_);
	ApplySettings();
	// The game opens on the main menu, to its theme
	world_->Sound().PlayMusic(world_->Config().menu_music);
}

// The world has just replaced its level, destroying the previous one: every
// view is pointed at the new one before anything draws again
void Game::ShowLevel() {
	renderer_3d_->SetScene(world_->CurrentLevel());
	renderer_2d_->SetScene(world_->CurrentLevel());
	minimap_->SetScene(world_->CurrentLevel());
	// Loading is not a frame: the next frame starts timing from here
	clock_.Restart();
	step_.Reset();
	pending_ = {};
	// Looking where the level (or the saved game) put the player
	const Player& player = world_->GetPlayer();
	view_.Reset(player.GetPosition().theta, player.GetPitch());
}

void Game::NewGame(std::string_view weapon_name, std::string_view level,
				   std::size_t difficulty_index) {
	// Scripted runs play the normal game, so their results stay comparable
	const auto& difficulties = world_->Config().difficulties;
	const std::string_view difficulty =
		IsScripted() || difficulty_index >= difficulties.size()
			? std::string_view("normal")
			: std::string_view(difficulties[difficulty_index].name);
	// Each game played rolls its own drops; a scripted run always the same
	const auto seed =
		IsScripted()
			? std::uint64_t{0}
			: static_cast<std::uint64_t>(
				  std::chrono::system_clock::now().time_since_epoch().count());
	if (auto started = world_->NewGame(weapon_name, level, difficulty, seed);
		!started) {
		std::cerr << "Cannot start a game: " << started.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	BeginGame(/*brief=*/true);
}

void Game::ContinueSavedGame() {
	if (!saved_game_) {
		return;
	}
	if (auto started = world_->ContinueGame(*saved_game_); !started) {
		std::cerr << "Cannot continue the game: " << started.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	BeginGame(/*brief=*/false);
}

// A game has just started: the views follow it, it fades in under its level's
// title, and a campaign is saved as it starts
void Game::BeginGame(bool brief) {
	render_type_ = RenderType::TEXTURE;
	renderer_ = renderer_3d_.get();
	map_expanded_ = false;
	ShowLevel();

	renderer_result_.reset();
	result_delay_time_ = 0.0;
	cleared_time_ = 0.0;
	// A new game opens with the first level's briefing, a continued one
	// fades in under its level's title; the benchmark measures the plain
	// game from its first frame
	fade_ = IsBenchmark() ? Fade::None : brief ? Fade::Briefing : Fade::In;
	fade_banner_ = true;
	fade_time_ = 0.0;
	// A new campaign opens with its story, before the first briefing
	if (brief && !IsBenchmark() && world_->InCampaign()) {
		TellStory(StoryBefore(world_->Config().campaign, world_->LevelIndex(),
							  /*new_game=*/true, story_),
				  /*ends_campaign=*/false);
	}
	SaveProgress();
}

// The campaign as it stands; scripted runs leave the player's saved game
// alone. Allocates nothing: it happens while the game runs.
void Game::SaveProgress() {
	if (IsScripted()) {
		return;
	}
	const auto saved = world_->Capture();
	if (!saved) {
		return;
	}
	saved->Save();
	saved_game_ = saved;
	since_save_ = 0.0;
	DescribeSavedGame();
}

// Saves now and then while nothing is fighting the player, so a later
// session goes on from about where this one stopped, never mid-fight
void Game::AutoSave(double delta_time) {
	constexpr double kAutoSaveSeconds = 5.0;
	since_save_ += delta_time;
	if (since_save_ >= kAutoSaveSeconds && fade_ == Fade::None &&
		!renderer_result_ &&
		world_->CurrentLevel().GetNumberOfAliveEnemies() > 0 &&
		world_->IsQuiet()) {
		SaveProgress();
	}
}

// Offers the saved game on the main menu, by its level and difficulty
void Game::DescribeSavedGame() {
	if (!saved_game_) {
		menu_->SetSavedGame({});
		return;
	}
	const GameConfig& config = world_->Config();
	const PreparedLevel* level = world_->FindCampaignLevel(saved_game_->level);
	const ui::FixedText<96> description(
		"LEVEL {} · {} · {}", saved_game_->level + 1,
		level != nullptr ? std::string_view(level->data.name) : "",
		std::string_view(config.difficulties[saved_game_->difficulty].label));
	menu_->SetSavedGame(description);
}

void Game::EnterPlaying() {
	state_ = GameState::Playing;
	had_pointer_lock_ = false;
	fire_armed_ = false;
	clicked_ = false;
	CaptureMouse(true);
	// Drop mouse motion that happened in the menu
	SDL_GetRelativeMouseState(nullptr, nullptr);
}

void Game::Pause() {
	state_ = GameState::Paused;
	CaptureMouse(false);
	menu_->SetMatch(InMatch() ? MatchMenuOf() : MatchMenu{});
	menu_->Open(MenuScreen::Pause);
}

MatchMenu Game::MatchMenuOf() const {
	const net::Room& room = match_->GetRoom();
	MatchMenu menu{.playing = true,
				   .code = room.code.View(),
				   .host = match_->IsHost(),
				   .paused = room.paused,
				   .arena_count = std::min<std::size_t>(
					   room.count, MatchMenu{}.arenas.size()),
				   .arena = room.arena};
	for (std::size_t i = 0; i < menu.arena_count; ++i) {
		menu.arenas[i] = room.arenas[i].View();
	}
	return menu;
}

void Game::CaptureMouse(bool captured) {
	SDL_SetWindowRelativeMouseMode(renderer_context_->GetWindow(), captured);
}

void Game::HandleMenuAction(const MenuAction& action) {
	switch (action.type) {
		case MenuAction::Type::None:
			break;
		case MenuAction::Type::StartGame:
			NewGame({}, {}, action.difficulty);
			EnterPlaying();
			break;
		case MenuAction::Type::Resume:
			EnterPlaying();
			break;
		case MenuAction::Type::QuitToMenu:
			// Leaving a match leaves its server
			match_.reset();
			menu_->SetMatch({});
			// Where the player left off, unless in the middle of a fight
			if (world_->IsQuiet() && fade_ == Fade::None && !renderer_result_) {
				SaveProgress();
			}
			state_ = GameState::Menu;
			CaptureMouse(false);
			menu_->Open(MenuScreen::Main);
			world_->Sound().PlayMusic(world_->Config().menu_music);
			break;
		case MenuAction::Type::Quit:
			running_ = false;
			break;
		case MenuAction::Type::SettingsChanged:
			ApplySettings();
			break;
		case MenuAction::Type::Continue:
			ContinueSavedGame();
			EnterPlaying();
			break;
		case MenuAction::Type::Join:
		case MenuAction::Type::Create: {
			const Settings& settings = Settings::Get();
			const std::string_view name = settings.player_name.Empty()
											  ? std::string_view("PLAYER")
											  : settings.player_name.View();
			const bool create = action.type == MenuAction::Type::Create;
			Connect(settings.server.View(), create ? "" : settings.room.View(),
					name, create);
			break;
		}
		case MenuAction::Type::PauseMatch:
			if (match_) {
				match_->Control(net::ControlAction::Pause);
			}
			break;
		case MenuAction::Type::ResumeMatch:
			if (match_) {
				match_->Control(net::ControlAction::Resume);
			}
			break;
		case MenuAction::Type::RestartMatch:
			if (match_) {
				match_->Control(net::ControlAction::Restart, action.arena);
				EnterPlaying();
			}
			break;
		case MenuAction::Type::CopyInvite:
			if (match_) {
				ui::CopyText(InviteLink(GamePage(), server_,
										match_->GetRoom().code.View()));
			}
			break;
	}
}

std::string MatchUrl(std::string_view server, std::string_view room) {
	std::string url(server);
	while (!url.empty() && url.back() == '/') {
		url.pop_back();
	}
	// A room is its code's letters and digits, in capitals
	std::string code;
	for (const char c : room) {
		if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
			code.push_back(
				static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
		}
	}
	if (!code.empty()) {
		url += "/room/" + code;
	}
	return url;
}

std::string CreateUrl(std::string_view server) {
	return MatchUrl(server, "") + "/create";
}

std::string InviteLink(std::string_view page, std::string_view server,
					   std::string_view code) {
	// The server's address as a query must have it: letters, digits and
	// "-._~" as they are, the rest as %XX
	std::string link(page);
	link += "?server=";
	for (const char c : server) {
		const auto byte = static_cast<unsigned char>(c);
		if (std::isalnum(byte) != 0 || c == '-' || c == '.' || c == '_' ||
			c == '~') {
			link.push_back(c);
		}
		else {
			link += std::format("%{:02X}", byte);
		}
	}
	return std::format("{}&room={}", link, code);
}

void Game::ApplySettings() {
	const Settings& settings = Settings::Get();
	world_->Sound().SetVolume(settings.volume, settings.music_volume,
							  settings.effects_volume);
	// Scripted runs keep the base view, so their results stay comparable
	camera_->SetFov(IsScripted() ? config_.base_fov : settings.FovRadians());
}

void Game::Present() {
	SDL_RenderPresent(renderer_context_->GetRenderer());
}

void Game::StartBenchmark(int frames) {
	constexpr double kFrameTime = 1.0 / 60.0;
	benchmark_frames_ = frames;
	ApplySettings();  // scripted now: the base view
	clock_.SetFixedDeltaTime(kFrameTime);
	Profiler::GetInstance().Enable(static_cast<std::size_t>(frames));
	// Loading the level counts towards startup, as it did before the menu
	// started games on demand
	const auto load_start = std::chrono::steady_clock::now();
	const auto load_allocations = AllocationStats::count;
	const auto load_bytes = AllocationStats::bytes;
	NewGame("mp5", world_->Config().benchmark_level);
	const std::chrono::duration<double, std::milli> load_time =
		std::chrono::steady_clock::now() - load_start;
	Profiler::GetInstance().AddStartupTime(load_time.count());
	Profiler::GetInstance().SetLevelLoad(
		load_time.count(), AllocationStats::count - load_allocations,
		AllocationStats::bytes - load_bytes);
	// The walls beside the route shot at, left and right, three times a
	// stretch: as many marks as a level keeps, passed close and far
	Scene& level = world_->CurrentLevel();
	for (std::size_t i = 1; i < kBenchmarkRoute.size(); ++i) {
		const auto [x0, y0] = kBenchmarkRoute[i - 1];
		const auto [x1, y1] = kBenchmarkRoute[i];
		const double along = std::atan2(y1 - y0, x1 - x0);
		for (const double t : {0.25, 0.5, 0.75}) {
			const vector2d from{x0 + (x1 - x0) * t, y0 + (y1 - y0) * t};
			StrikeWall(level, from, along + std::numbers::pi / 2);
			StrikeWall(level, from, along - std::numbers::pi / 2);
		}
	}
	state_ = GameState::Playing;
}

// Plays a scripted session through everything a player can reach once a
// game has started, and reports every allocation it makes: there must be
// none. Like a player, it starts at the main menu; startup and the menu
// before the first game (when, on the web, the audio device makes its first
// callbacks) are not counted.
void Game::StartSoak(int frames) {
	soak_frames_ = frames + kSoakMenuFrames;
	ApplySettings();  // scripted now: the base view
	clock_.SetFixedDeltaTime(1.0 / 60.0);
	state_ = GameState::Menu;
	menu_->Open(MenuScreen::Main);
}

// The session, by frame. Each step drives the game through the same calls a
// player's input would; allocations are counted per phase
void Game::SoakStep() {
	struct Phase
	{
		int start;
		const char* name;
	};
	static constexpr std::array<Phase, 12> kPhases = {{
		{0, "play_3d"},
		{90, "view_2d"},
		{135, "map"},
		{180, "pause"},
		{240, "settings"},
		{300, "play"},
		{330, "pickup"},
		{345, "door"},
		{360, "level_transition"},
		{700, "death"},
		{900, "new_game"},
		{960, "play_again"},
	}};
	// The main menu, then the player starts a game: counting begins there
	if (soak_frame_ < kSoakMenuFrames) {
		++soak_frame_;
		return;
	}
	if (soak_frame_ == kSoakMenuFrames) {
		soak_allocations_ = AllocationStats::count;
		soak_bytes_ = AllocationStats::bytes;
		HandleMenuAction(
			{.type = MenuAction::Type::StartGame, .difficulty = 0});
		// The walls round the player shot at, all the marks a level keeps:
		// drawn from the first frame of play
		const vector2d from = world_->GetPlayer().GetPose();
		constexpr int kShots = static_cast<int>(Scene::kWallMarks);
		for (int shot = 0; shot < kShots; ++shot) {
			StrikeWall(world_->CurrentLevel(), from,
					   2.0 * std::numbers::pi * shot / kShots);
		}
	}
	const int frame = soak_frame_++ - kSoakMenuFrames;
	if (soak_first_allocation_ < 0 &&
		AllocationStats::count != soak_allocations_) {
		// Allocated during the previous frame
		soak_first_allocation_ = frame - 1;
	}
	soak_max_level_ = std::max(soak_max_level_, world_->LevelNumber());
	soak_saw_result_ = soak_saw_result_ || state_ == GameState::Result;
	soak_saw_stats_ = soak_saw_stats_ || fade_ == Fade::Stats;
	soak_saw_briefing_ = soak_saw_briefing_ || fade_ == Fade::Briefing;
	soak_saw_story_ = soak_saw_story_ || fade_ == Fade::Story;
	for (std::size_t i = 0; i < kPhases.size(); ++i) {
		if (frame == kPhases[i].start) {
			soak_phase_ = i;
			soak_phase_start_[i] = AllocationStats::count;
		}
	}

	if (frame == 90) {
		render_type_ = RenderType::LINE;
		renderer_ = renderer_2d_.get();
	}
	else if (frame == 135) {
		// Back in 3D, with the map large (M)
		render_type_ = RenderType::TEXTURE;
		renderer_ = renderer_3d_.get();
		map_expanded_ = true;
	}
	else if (frame == 180) {
		map_expanded_ = false;
		Pause();
	}
	else if (frame == 240) {
		menu_->Open(MenuScreen::Settings);
	}
	else if (frame > 240 && frame < 300) {
		// Nudge a setting every frame: its value is redrawn as new text
		SDL_Event right{};
		right.type = SDL_EVENT_KEY_DOWN;
		right.key.key = SDLK_RIGHT;
		menu_->HandleEvent(right);
	}
	else if (frame == 300) {
		HandleMenuAction({.type = MenuAction::Type::Resume, .difficulty = 0});
	}
	else if (frame == 310) {
		// Handed every weapon, the player draws the shotgun, then goes back
		// to the pistol
		Player& player = world_->GetPlayer();
		player.SetOwnedWeapons(0xFF);
		player.SelectWeapon(player.WeaponCount() - 1);
	}
	else if (frame == 320) {
		world_->GetPlayer().SelectWeapon(0);
	}
	else if (frame == 330) {
		// Hurt, the player steps onto the level's first pickup: taken next
		// tick, with its sound and flash
		Player& player = world_->GetPlayer();
		const auto pickups = world_->CurrentLevel().GetPickups();
		if (!pickups.empty()) {
			player.DecreaseHealth(30.0);
			player.SetPosition(Position2D(pickups.front()->GetPose(),
										  player.GetPosition().theta));
		}
	}
	else if (frame == 340) {
		const auto pickups = world_->CurrentLevel().GetPickups();
		soak_took_pickup_ = !pickups.empty() && pickups.front()->IsTaken();
	}
	else if (frame == 345) {
		// A door starts sliding open, drawn as it moves
		if (!world_->CurrentLevel().GetMap().GetDoors().empty()) {
			world_->CurrentLevel().OpenDoor(0);
		}
		// ... and a secret starts sliding back
		if (!world_->CurrentLevel().GetMap().GetPushWalls().empty()) {
			world_->CurrentLevel().GetMap().Push(0);
		}
	}
	else if (frame == 355) {
		const auto doors = world_->CurrentLevel().GetMap().GetDoors();
		soak_opened_door_ = !doors.empty() && doors.front().openness > 0.0;
		const auto secrets = world_->CurrentLevel().GetMap().GetPushWalls();
		soak_found_secret_ = !secrets.empty() && secrets.front().offset > 0.0;
	}
	else if (frame == 360) {
		// As if every enemy were shot: the level is cleared and the game
		// fades into the next one
		Scene& level = world_->CurrentLevel();
		for (Enemy* enemy : level.GetEnemies()) {
			if (enemy->GetHealth() > 0) {
				enemy->DecreaseHealth(enemy->GetHealth() + 1.0);
				enemy->SetAttacked(true);
				level.DecreaseAliveEnemies();
			}
		}
	}
	else if (frame == 362) {
		// ... and the player takes the way out
		world_->CurrentLevel().UseExit();
	}
	else if (frame == 700) {
		world_->GetPlayer().DecreaseHealth(1000.0);
	}
	else if (frame == 900) {
		HandleMenuAction(
			{.type = MenuAction::Type::StartGame, .difficulty = 0});
	}
	else if (frame >= soak_frames_ - kSoakMenuFrames) {
		std::cout
			<< "SOAK_RESULT {\"frames\":" << frame
			<< ",\"max_level\":" << soak_max_level_
			<< ",\"saw_result\":" << (soak_saw_result_ ? "true" : "false")
			<< ",\"took_pickup\":" << (soak_took_pickup_ ? "true" : "false")
			<< ",\"opened_door\":" << (soak_opened_door_ ? "true" : "false")
			<< ",\"saw_stats\":" << (soak_saw_stats_ ? "true" : "false")
			<< ",\"found_secret\":" << (soak_found_secret_ ? "true" : "false")
			<< ",\"saw_briefing\":" << (soak_saw_briefing_ ? "true" : "false")
			<< ",\"saw_story\":" << (soak_saw_story_ ? "true" : "false")
			<< ",\"first_allocating_frame\":" << soak_first_allocation_
			<< ",\"allocations\":" << AllocationStats::count - soak_allocations_
			<< ",\"bytes\":" << AllocationStats::bytes - soak_bytes_
			<< ",\"phases\":{";
		for (std::size_t i = 0; i < kPhases.size(); ++i) {
			const std::uint64_t end = i + 1 < kPhases.size()
										  ? soak_phase_start_[i + 1]
										  : AllocationStats::count;
			std::cout << (i > 0 ? "," : "") << '"' << kPhases[i].name
					  << "\":" << end - soak_phase_start_[i];
		}
		std::cout << "}}\n" << std::flush;
		running_ = false;
	}
}

bool Game::IsScripted() const {
	return IsBenchmark() || soak_frames_ > 0;
}

bool Game::IsBenchmark() const {
	return benchmark_frames_ > 0;
}

void Game::Run() {
#ifdef __EMSCRIPTEN__
	// Loaded: the page stops watching for a start that failed (see
	// web/shell.html)
	EM_ASM(if (Module.onGameReady) Module.onGameReady(););
	// The browser drives the loop: one Tick per animation frame
	emscripten_set_main_loop_arg(
		[](void* game) {
			if (!static_cast<Game*>(game)->Tick()) {
				emscripten_cancel_main_loop();
			}
		},
		this, 0, true);
#else
	while (Tick()) {}
#endif
}

bool Game::Tick() {
	if (soak_frames_ > 0) {
		SoakStep();
	}
	switch (state_) {
		case GameState::Menu:
			MenuTick();
			break;
		case GameState::Joining:
			JoiningTick();
			break;
		case GameState::Playing:
			GameTick();
			break;
		case GameState::Paused:
			PausedTick();
			break;
		case GameState::Result:
			ResultTick();
			break;
	}
	return running_;
}

bool Game::PollMenuEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			running_ = false;
			return false;
		}
		// Where the mouse is on the picture, which SDL scales to the window
		SDL_ConvertEventToRenderCoordinates(renderer_context_->GetRenderer(),
											&event);
		menu_->HandleEvent(event);
	}
	return true;
}

void Game::MenuTick() {
	if (!PollMenuEvents()) {
		return;
	}
	clock_.Tick();
	SDL_SetRenderDrawColor(renderer_context_->GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(renderer_context_->GetRenderer());
	const auto action = menu_->Update(clock_.DeltaTime());
	Present();
	HandleMenuAction(action);
}

void Game::Connect(std::string_view server, std::string_view room,
				   std::string_view name, bool create) {
	server_ = server;
	server_url_ = create ? CreateUrl(server) : MatchUrl(server, room);
	room_code_ = {};
	player_name_ = name;
	rejoining_ = false;
	match_ =
		std::make_unique<MatchClient>(net::Connection::Open(server_url_), name);
	state_ = GameState::Joining;
}

void Game::JoiningTick() {
	// Esc gives up joining, back to the menu
	bool leave = false;
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			running_ = false;
			return;
		}
		leave = leave || (event.type == SDL_EVENT_KEY_DOWN &&
						  event.key.key == SDLK_ESCAPE);
	}
	if (leave) {
		HandleMenuAction({.type = MenuAction::Type::QuitToMenu});
		return;
	}
	clock_.Tick();
	// Lost in the middle of a match: joining again, a while apart
	if (rejoining_ && match_ &&
		match_->GetState() == MatchClient::State::Closed &&
		rejoin_tries_ < kRejoinTries) {
		rejoin_wait_ -= clock_.DeltaTime();
		if (rejoin_wait_ <= 0.0) {
			++rejoin_tries_;
			rejoin_wait_ = kRejoinSeconds;
			match_ = std::make_unique<MatchClient>(
				net::Connection::Open(server_url_), player_name_);
		}
	}
	if (match_) {
		match_->Poll(*world_);
		if (match_->GetState() == MatchClient::State::Playing) {
			BeginMatch();
			return;
		}
	}
	SDL_SetRenderDrawColor(renderer_context_->GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(renderer_context_->GetRenderer());
	const MatchClient::State state =
		match_ ? match_->GetState() : MatchClient::State::Closed;
	if (state == MatchClient::State::Rejected) {
		switch (match_->Reason()) {
			case net::RejectReason::Full:
				menu_->DrawNotice("The game is full  ·  Esc to go back");
				break;
			case net::RejectReason::NoRoom:
				menu_->DrawNotice("No game has that code  ·  Esc to go back");
				break;
			case net::RejectReason::Busy:
				menu_->DrawNotice(
					"The server cannot make another room now  ·  Esc to go "
					"back");
				break;
			case net::RejectReason::Version:
				menu_->DrawNotice(
					"The server plays another version  ·  Esc to go back");
				break;
		}
	}
	else if (rejoining_ && (state != MatchClient::State::Closed ||
							rejoin_tries_ < kRejoinTries)) {
		menu_->DrawNotice(
			"The connection was lost: joining again  ·  Esc to "
			"go back");
	}
	else if (state == MatchClient::State::Closed) {
		menu_->DrawNotice(ui::FixedText<192>(
			"Cannot reach {}  ·  Esc to go back", server_url_));
	}
	else {
		menu_->DrawNotice(ui::FixedText<160>("Joining {}", server_url_));
	}
	Present();
}

void Game::BeginMatch() {
	rejoining_ = false;
	render_type_ = RenderType::TEXTURE;
	renderer_ = renderer_3d_.get();
	map_expanded_ = false;
	ShowLevel();
	renderer_result_.reset();
	fade_ = Fade::None;
	fade_time_ = 0.0;
	EnterPlaying();
}

void Game::TickMatch(const PlayerCommand& command, int ticks) {
	match_->Poll(*world_);
	// The next match, on another arena: the views onto it
	if (match_->TakeNewLevel()) {
		ShowLevel();
	}
	// A room made here has its code now: lost, it is joined again by it
	if (const net::RoomCode& code = match_->GetRoom().code;
		!code.View().empty() && !(code == room_code_)) {
		room_code_ = code;
		server_url_ = MatchUrl(server_, code.View());
	}
	// Paused by the host: no one moves, this player neither
	for (int tick = 0;
		 tick < ticks && match_->GetState() == MatchClient::State::Playing &&
		 !match_->Paused();
		 ++tick) {
		match_->BeforeTick(*world_, tick == 0 ? command : Repeated(command));
		world_->CurrentLevel().Update(step_.TickSeconds());
		match_->AfterTick(*world_);
	}
	// Back in the game: looking where the server brought it in looking
	if (match_->GetState() == MatchClient::State::Playing &&
		match_->TakeRevived()) {
		view_.Reset(world_->GetPlayer().GetPosition().theta, 0.0);
	}
	// The server gone: why, on the joining screen; lost, it joins again
	if (match_->GetState() != MatchClient::State::Playing) {
		CaptureMouse(false);
		state_ = GameState::Joining;
		if (match_->GetState() == MatchClient::State::Closed) {
			rejoining_ = true;
			rejoin_tries_ = 0;
			rejoin_wait_ = kRejoinSeconds;
		}
	}
}

// The game stays frozen behind the pause, controls and settings screens; a
// match goes on, this player standing where it was
void Game::PausedTick() {
	if (!PollMenuEvents()) {
		return;
	}
	clock_.Tick();
	if (InMatch()) {
		TickMatch(PlayerCommand{.has_view = true,
								.view_theta = view_.Theta(),
								.view_pitch = view_.Pitch()},
				  step_.Advance(clock_.DeltaTime()));
		if (state_ != GameState::Paused) {
			return;
		}
		const double alpha = step_.Alpha();
		camera_->SetPitch(ViewPitch(alpha));
		camera_->Update(ViewPosition(alpha), alpha);
	}
	RenderView(ViewPosition(InMatch() ? step_.Alpha() : 1.0));
	if (InMatch()) {
		menu_->SetMatch(MatchMenuOf());
	}
	const auto action = menu_->Update(clock_.DeltaTime());
	Present();
	HandleMenuAction(action);
}

void Game::ResultTick() {
	if (!PollMenuEvents()) {
		return;
	}
	clock_.Tick();
	if (renderer_result_) {
		renderer_result_->Render(clock_.DeltaTime());
	}
	const auto action = menu_->Update(clock_.DeltaTime());
	Present();
	HandleMenuAction(action);
}

void Game::GameTick() {
	auto& profiler = Profiler::GetInstance();
	profiler.BeginFrame();
	{
		ScopedTimer timer(ProfileSection::Frame);
		UpdateAndRender();
	}
	profiler.EndFrame();

	if (IsBenchmark()) {
		BenchmarkStep();
		return;
	}
	// A match has no end yet: no level cleared, no game over
	if (state_ == GameState::Playing && !InMatch()) {
		CheckGameOver();
	}
#ifndef __EMSCRIPTEN__
	// In the browser requestAnimationFrame already paces the frames; scripted
	// runs step a fixed time and need no pacing
	if (!IsScripted()) {
		clock_.SleepForHz(config_.fps);
	}
#endif
}

// What undoes SDL's web backend scaling the mouse's motion (by the window's
// width over the canvas's on the page): the canvas's width over the
// window's, 1 natively. The window takes the canvas's size on the page as
// the game starts; the page can change it later.
double Game::CanvasStretch() const {
#ifdef __EMSCRIPTEN__
	double width = 0.0;
	double height = 0.0;
	int window_width = 0;
	if (emscripten_get_element_css_size("#canvas", &width, &height) ==
			EMSCRIPTEN_RESULT_SUCCESS &&
		SDL_GetWindowSize(renderer_context_->GetWindow(), &window_width,
						  nullptr) &&
		width > 0.0 && window_width > 0) {
		return width / window_width;
	}
#endif
	return 1.0;
}

MouseLook ToMouseLook(double dx, double dy, const Settings& settings,
					  const GeneralConfig& view) {
	constexpr double kRadiansPerPixel = 0.005;
	const double turn = dx * kRadiansPerPixel * settings.mouse_sensitivity;
	// Looking up slides the view by as many screen pixels as turning the
	// same mouse distance does. In a view fov across, a radian of turn moves
	// the middle of the picture width / (2 tan(fov / 2)) pixels, and a slope
	// of 1 is height tan(base_fov / 2) / tan(fov / 2) pixels up: the fov
	// cancels, so the slope does not depend on it. The mouse pushed away
	// looks up, unless inverted.
	const double up = (settings.invert_mouse_y ? dy : -dy) * kRadiansPerPixel *
					  settings.mouse_sensitivity * view.screen_width /
					  (2 * view.screen_height * std::tan(view.base_fov / 2));
	return {.turn = turn, .up = up};
}

// Reads this frame's player input from the keyboard and mouse
PlayerCommand Game::SampleCommand() {
	const bool* keys = SDL_GetKeyboardState(nullptr);
	const auto axis = [keys](SDL_Scancode positive, SDL_Scancode negative) {
		return static_cast<std::int8_t>(static_cast<int>(keys[positive]) -
										static_cast<int>(keys[negative]));
	};
	PlayerCommand command;
	command.forward = axis(SDL_SCANCODE_W, SDL_SCANCODE_S);
	command.strafe = axis(SDL_SCANCODE_D, SDL_SCANCODE_A);
	command.turn = axis(SDL_SCANCODE_RIGHT, SDL_SCANCODE_LEFT);

	// Relative mouse mode reports motion since the last call, which also
	// works under browser pointer lock (unlike warping the cursor)
	float dx = 0.0F;
	float dy = 0.0F;
	SDL_GetRelativeMouseState(&dx, &dy);
	if (captured_) {
		// SDL's web backend scales the motion by how far the canvas is
		// stretched, which would make the mouse faster in a smaller
		// window: undone here, so a hand's movement turns as far anywhere
		const double stretch = CanvasStretch();
		const MouseLook look = ToMouseLook(dx, dy, Settings::Get(), config_);
		command.look = look.turn * stretch;
		command.look_up = look.up * stretch;
	}

	// Firing waits for the button to come up once the mouse is captured;
	// a click shorter than a frame still fires
	const bool held =
		(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
	fire_armed_ = captured_ && (fire_armed_ || !held);
	command.fire = fire_armed_ && (held || clicked_);
	clicked_ = false;
	command.reload = keys[SDL_SCANCODE_R];
	command.use = keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_SPACE];
	return command;
}

void Game::UpdateAndRender() {
	CheckGameEvent();
	if (state_ != GameState::Playing) {
		return;
	}
	clock_.Tick();
	// The benchmark plays without input, so a run cannot depend on what the
	// keyboard or mouse happen to do
	PlayerCommand command = IsScripted() ? PlayerCommand{} : SampleCommand();
	// The wheel turned since the last frame: a step through the weapons
	command.cycle = static_cast<std::int8_t>(std::clamp(wheel_, -1, 1));
	wheel_ = 0;
	// A number key pressed since: that weapon in hand
	command.weapon = static_cast<std::int8_t>(weapon_key_);
	weapon_key_ = -1;
	// The view turns with the hand at once, this frame; the next tick takes
	// it. Not for a dead player, nor while the level waits behind its
	// results or briefing.
	if (!IsScripted() && world_->GetPlayer().IsAlive() &&
		fade_ != Fade::Stats && fade_ != Fade::Story &&
		fade_ != Fade::Briefing) {
		command = view_.Apply(command, clock_.DeltaTime());
	}

	// The simulation advances in fixed ticks, whatever the frame rate, so
	// the same commands always play out the same way. A long stall (a
	// breakpoint, a hidden browser tab) is dropped rather than caught up in
	// a burst of ticks. A frame with no tick keeps its input for the next.
	pending_ = Gather(pending_, command);
	int ticks = step_.Advance(clock_.DeltaTime());
	if (InMatch()) {
		TickMatch(pending_, ticks);
		if (ticks > 0) {
			pending_ = {};
		}
		ticks = 0;
		if (state_ != GameState::Playing) {
			return;
		}
	}
	if (ticks > 0) {
		world_->GetPlayer().SetCommand(pending_);
		pending_ = {};
	}
	for (; ticks > 0; --ticks) {
		// The level waits behind its results screen and the next briefing
		if (fade_ != Fade::Stats && fade_ != Fade::Story &&
			fade_ != Fade::Briefing) {
			world_->CurrentLevel().Update(step_.TickSeconds());
		}
	}
	// Frames fall between ticks: the view is drawn this far from the last
	// tick towards the next, so motion stays smooth at any frame rate
	const double alpha = step_.Alpha();
	const Position2D eye = ViewPosition(alpha);
	// Sounds from places are heard from where the view is
	world_->Sound().SetListener(eye.pose, eye.theta);
	{
		ScopedTimer timer(ProfileSection::Camera);
		camera_->SetPitch(ViewPitch(alpha));
		camera_->Update(eye, alpha);
		camera_->ExploreView();
	}
	AutoSave(clock_.DeltaTime());
	RenderView(eye);
	DrawTransition();
	ScopedTimer timer(ProfileSection::Present);
	Present();
}

// The first objective of the level not yet done, or once they all are, the
// way out; empty for a level with neither (the benchmark's)
std::string_view Game::CurrentObjective() const {
	const Scene& level = world_->CurrentLevel();
	for (const Objective& objective : world_->LevelObjectives()) {
		const bool done = objective.type == Objective::Type::KillAll
							  ? level.GetNumberOfAliveEnemies() == 0
							  : level.TargetsLeft() == 0;
		if (!done) {
			return objective.text;
		}
	}
	return level.GetMap().HasExit() ? std::string_view("Reach the exit")
									: std::string_view();
}

// Where the view is drawn from: the player's place `alpha` of the way from
// the last tick to the next, looking where the input has turned the view
// (a scripted run's player is steered by the script, not the view)
Position2D Game::ViewPosition(double alpha) const {
	Position2D eye = world_->GetPlayer().GetRenderPosition(alpha);
	if (!IsScripted()) {
		eye.theta = view_.Theta();
	}
	return eye;
}

// How far the view is tipped: where the input looks, and a shot's kick
double Game::ViewPitch(double alpha) const {
	const Player& player = world_->GetPlayer();
	return (IsScripted() ? player.GetPitch() : view_.Pitch()) +
		   player.GetRenderKick(alpha);
}

namespace {

std::string_view Ordinal(std::size_t place) {
	constexpr std::array<std::string_view, net::kMaxPlayers> kPlaces{
		"1ST", "2ND", "3RD", "4TH", "5TH", "6TH", "7TH", "8TH"};
	return kPlaces[std::min(place, kPlaces.size()) - 1];
}

SDL_Color ColourOf(std::size_t slot) {
	const IGameObject::Tint tint = Scene::SlotTint(slot);
	return {tint.r, tint.g, tint.b, 255};
}

}  // namespace

void Game::DrawMatchHud() {
	const net::Scores& scores = match_->GetScores();
	const std::size_t local = world_->LocalSlot().value_or(0);
	const bool race = scores.mode == net::MatchMode::GunRace;
	const auto& ladder = world_->Config().gun_race;
	const auto& arsenal = world_->Config().weapons;
	// A player the server named nothing is its colour's number
	std::array<std::optional<ui::FixedText<24>>, net::kMaxPlayers> unnamed{};
	const auto name_of = [&](std::size_t slot) -> std::string_view {
		const std::string_view name = match_->NameOf(slot);
		if (!name.empty() || slot >= unnamed.size()) {
			return name;
		}
		unnamed[slot] = ui::FixedText<24>("PLAYER {}", slot + 1);
		return unnamed[slot]->View();
	};
	// Everyone, the best first
	std::array<ScoreLine, net::kMaxPlayers> lines{};
	std::size_t count = 0;
	for (const net::Score& score :
		 std::span(scores.players).first(scores.count)) {
		// The player's own round trip as it measures it, the others' as
		// they told the server
		const std::optional<int> ping =
			score.slot == local ? match_->Ping() : match_->PingOf(score.slot);
		lines[count++] = {.name = name_of(score.slot),
						  .colour = ColourOf(score.slot),
						  .frags = score.frags,
						  .deaths = score.deaths,
						  .step = score.step + 1,
						  .ping = ping.value_or(0),
						  .local = score.slot == local};
	}
	const auto shown = std::span(lines).first(count);
	std::ranges::stable_sort(shown,
							 [race](const ScoreLine& a, const ScoreLine& b) {
								 if (race && a.step != b.step) {
									 return a.step > b.step;
								 }
								 if (a.frags != b.frags) {
									 return a.frags > b.frags;
								 }
								 return a.deaths < b.deaths;
							 });
	const auto mine = std::ranges::find_if(shown, &ScoreLine::local);
	const std::size_t place =
		static_cast<std::size_t>(mine - shown.begin()) + 1;
	const int seconds = scores.seconds_left;
	const ui::FixedText<16> clock("{}:{:02}", seconds / 60, seconds % 60);
	if (mine != shown.end()) {
		const Player& me = world_->GetPlayer();
		const std::size_t held = me.HeldWeapon();
		const ui::FixedText<16> big =
			race ? ui::FixedText<16>("{}/{}", mine->step, ladder.size())
				 : ui::FixedText<16>("{}", mine->frags);
		const ui::FixedText<48> label =
			race ? ui::FixedText<48>("{} · {}", arsenal[held].label,
									 Ordinal(place))
				 : ui::FixedText<48>("FRAGS · {}", Ordinal(place));
		menu_->DrawMatchStanding(big, label, clock);
	}
	// The latest kills, fading out in their last second
	std::array<KillLine, MatchClient::kKillFeed> kills{};
	std::size_t kill_count = 0;
	for (const MatchClient::Kill& kill : match_->KillFeed()) {
		kills[kill_count++] = {
			.killer = name_of(kill.killer),
			.killer_colour = ColourOf(kill.killer),
			.weapon = kill.killer == kill.victim ? std::string_view("·")
					  : kill.weapon < arsenal.size()
						  ? std::string_view(arsenal[kill.weapon].label)
						  : std::string_view(),
			.victim = name_of(kill.victim),
			.victim_colour = ColourOf(kill.victim),
			.opacity = MatchClient::kKillSeconds - kill.age};
	}
	menu_->DrawKillFeed(std::span(kills).first(kill_count));
	// A private room's code, to pass on; paused, everyone told why
	const net::Room& room = match_->GetRoom();
	if (!room.code.View().empty()) {
		menu_->DrawObjective(
			match_->IsHost()
				? ui::FixedText<80>("ROOM {}  ·  Esc to invite, pause or pick "
									"the arena",
									room.code.View())
				: ui::FixedText<80>("ROOM {}", room.code.View()));
	}
	// (not under the match's own menu, which says so)
	if (room.paused && state_ != GameState::Paused) {
		menu_->DrawLevelBanner(
			"PAUSED",
			match_->IsHost() ? "BY YOU  ·  ESC TO GO ON" : "BY THE HOST", 255);
	}
	// The newest kill, called out for a moment if the local player had a
	// hand in it
	const auto feed = match_->KillFeed();
	constexpr double kCalloutSeconds = 2.0;
	if (!feed.empty() && feed.back().age < kCalloutSeconds &&
		(feed.back().killer == local || feed.back().victim == local)) {
		const MatchClient::Kill& kill = feed.back();
		const ui::FixedText<48> callout =
			kill.killer != local
				? ui::FixedText<48>("{} FRAGGED YOU", name_of(kill.killer))
			: kill.victim == local
				? ui::FixedText<48>("YOU FRAGGED YOURSELF")
				: ui::FixedText<48>("YOU FRAGGED {}", name_of(kill.victim));
		menu_->DrawCallout(callout, kCalloutSeconds - kill.age);
	}
	// The scoreboard while Tab is held, and while the result shows
	const bool over = scores.phase == net::MatchPhase::Intermission;
	if (over || SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_TAB]) {
		const ui::FixedText<48> heading =
			over   ? (scores.winner == net::kNoWinner
						  ? ui::FixedText<48>("A DRAW")
						  : ui::FixedText<48>("{} WINS", name_of(scores.winner)))
			: race ? ui::FixedText<48>("GUN RACE")
				   : ui::FixedText<48>("DEATHMATCH · FIRST TO {}",
									   scores.frag_limit);
		const ui::FixedText<48> footer =
			over ? ui::FixedText<48>("The next match in {}", seconds)
				 : ui::FixedText<48>("{} left", clock.View());
		menu_->DrawScoreboard(heading, shown, race, footer);
	}
	else if (!world_->GetPlayer().IsAlive() && captured_) {
		menu_->DrawNotice("Click to come back");
	}
}

void Game::RenderView(const Position2D& eye) {
	// A match's round trip, under the frame rate
	renderer_3d_->SetPing(InMatch() ? match_->Ping().value_or(0) : 0);
	renderer_->RenderScene(clock_.DeltaTime());
	if (render_type_ == RenderType::TEXTURE) {
		minimap_->Render(eye, map_expanded_);
		if (!InMatch()) {
			const LevelStats stats = world_->CurrentLevel().GetStats();
			menu_->DrawEnemyCounter(stats.kills, stats.enemies);
		}
		const Player& player = world_->GetPlayer();
		// The weapon chosen shows at once, while the last one goes down
		menu_->DrawWeaponSlots(
			player.WeaponCount(), player.GetOwnedWeapons(),
			player.ComingWeapon().value_or(player.HeldWeapon()));
		switch (world_->CurrentLevel().GetNotice()) {
			case Scene::Notice::NeedGoldKey:
				menu_->DrawNotice("You need the gold key");
				break;
			case Scene::Notice::NeedSilverKey:
				menu_->DrawNotice("You need the silver key");
				break;
			case Scene::Notice::ExitLocked:
				menu_->DrawNotice("The mission is not done yet");
				break;
			case Scene::Notice::Secret:
				menu_->DrawNotice("You found a secret");
				break;
			case Scene::Notice::None:
				// The mouse not captured yet (after a pause, on the web): a
				// click takes it, and is not a shot
				if (!captured_ && !IsScripted() &&
					state_ == GameState::Playing && fade_ == Fade::None) {
					menu_->DrawNotice("Click to play");
				}
				break;
		}
		if (fade_ == Fade::None && !InMatch()) {
			menu_->DrawObjective(CurrentObjective());
		}
		if (InMatch()) {
			DrawMatchHud();
		}
		// A page of intel just read, over the view, fading at the end
		if (const int document = world_->CurrentLevel().ShownDocument();
			document >= 0) {
			const IntelSpawn& page =
				world_->LevelIntel()[static_cast<std::size_t>(document)];
			const double left =
				Scene::kDocumentSeconds - world_->CurrentLevel().DocumentAge();
			menu_->DrawDocument(page.title, page.text, std::min(left, 1.0));
		}
	}
}

void Game::CheckGameEvent() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			running_ = false;
			return;
		}
		if (IsScripted()) {
			continue;
		}
		if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
			Pause();
			return;
		}
		// Any of these goes on from a level's results, a page of story or
		// a briefing
		if ((fade_ == Fade::Stats || fade_ == Fade::Story ||
			 fade_ == Fade::Briefing) &&
			fade_time_ >= kStatsInputDelay &&
			(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
			 (event.type == SDL_EVENT_KEY_DOWN &&
			  (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER ||
			   event.key.key == SDLK_SPACE || event.key.key == SDLK_E)))) {
			if (fade_ == Fade::Stats) {
				ContinueFromStats();
			}
			else if (fade_ == Fade::Story) {
				NextStoryPage();
			}
			else {
				StartFromBriefing();
			}
			continue;
		}
		if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			// Down the wheel is on to the next weapon, a whole notch at a
			// time
			wheel_ -= event.wheel.integer_y;
		}
		if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
			event.button.button == SDL_BUTTON_LEFT) {
			clicked_ = true;
		}
		if (event.type == SDL_EVENT_KEY_DOWN) {
			const SDL_Scancode key = event.key.scancode;
			if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_8) {
				weapon_key_ = key - SDL_SCANCODE_1;
			}
			if (event.key.key == SDLK_ESCAPE) {
				Pause();
				return;
			}
			if (event.key.key == SDLK_M) {
				map_expanded_ = !map_expanded_;
			}
			if (event.key.key == SDLK_J && debug_view_) {
				JoinStandIn();
			}
			if (event.key.key == SDLK_P && debug_view_) {
				if (render_type_ == RenderType::TEXTURE) {
					render_type_ = RenderType::LINE;
					renderer_ = renderer_2d_.get();
				}
				else {
					render_type_ = RenderType::TEXTURE;
					renderer_ = renderer_3d_.get();
				}
			}
		}
	}

	captured_ = SDL_GetWindowRelativeMouseMode(renderer_context_->GetWindow());
#ifdef __EMSCRIPTEN__
	// Browsers release the pointer lock on Esc without passing the key on, so
	// losing the lock is what pauses the game there
	if (!IsScripted()) {
		EmscriptenPointerlockChangeEvent status;
		if (emscripten_get_pointerlock_status(&status) ==
			EMSCRIPTEN_RESULT_SUCCESS) {
			captured_ = status.isActive != 0;
			if (status.isActive) {
				had_pointer_lock_ = true;
			}
			else if (had_pointer_lock_) {
				Pause();
			}
		}
	}
#endif
}

// Another player joins in the first free slot, a few steps ahead (where
// there is floor), walking round in a small circle: the others' figures
// seen from every side
void Game::JoinStandIn() {
	for (std::size_t slot = 1; slot < Scene::kMaxPlayers; ++slot) {
		if (world_->FindPlayer(slot) != nullptr) {
			continue;
		}
		if (!world_->JoinPlayer(slot)) {
			return;
		}
		const Position2D& eye = world_->GetPlayer().GetPosition();
		const vector2d facing{std::cos(eye.theta), std::sin(eye.theta)};
		vector2d at = eye.pose + facing;
		for (const double ahead : {2.0, 1.5}) {
			const vector2d there = eye.pose + facing * ahead;
			if (!world_->CurrentLevel().GetMap().IsBlocked(there)) {
				at = there;
				break;
			}
		}
		Player& stand_in = *world_->FindPlayer(slot);
		stand_in.SetPosition(Position2D(at, eye.theta + std::numbers::pi));
		stand_in.SetCommand({.forward = 1, .turn = 1});
		return;
	}
}

void Game::CheckGameOver() {
	const double delta_time = clock_.DeltaTime();
	// Dead: once the player has fallen, game over
	const Player& player = world_->GetPlayer();
	if (!player.IsAlive() && player.GetDeathFall() >= 1.0 &&
		!renderer_result_) {
		renderer_result_.emplace(
			*renderer_context_,
			renderer_context_->Textures().GetTextureId("game_over"));
	}
	if (world_->CurrentLevel().IsComplete() && !renderer_result_ &&
		fade_ != Fade::Out && fade_ != Fade::Stats) {
		cleared_time_ += delta_time;
		if (cleared_time_ >= kClearedPause) {
			fade_ = Fade::Out;
			fade_time_ = 0.0;
		}
	}
	AdvanceTransition(delta_time);

	if (renderer_result_) {
		result_delay_time_ += delta_time;
		if (result_delay_time_ >= kEndOfLevelDelay) {
			state_ = GameState::Result;
			CaptureMouse(false);
			menu_->Open(MenuScreen::Result);
		}
	}
}

// Moves the fade between levels on; at full black the next level replaces
// the cleared one (built from data read at startup, so without a hitch)
void Game::AdvanceTransition(double delta_time) {
	if (fade_ == Fade::None) {
		return;
	}
	fade_time_ += delta_time;
	if (fade_ == Fade::Out && fade_time_ >= kFadeOutSeconds) {
		cleared_stats_ = world_->CurrentLevel().GetStats();
		fade_ = Fade::Stats;
		fade_time_ = 0.0;
	}
	else if (fade_ == Fade::Stats && IsScripted() &&
			 fade_time_ >= kScriptedStatsSeconds) {
		ContinueFromStats();
	}
	else if (fade_ == Fade::Story && IsScripted() &&
			 fade_time_ >= kScriptedStoryPageSeconds) {
		NextStoryPage();
	}
	else if (fade_ == Fade::Briefing && IsScripted() &&
			 fade_time_ >= kScriptedStatsSeconds) {
		StartFromBriefing();
	}
	else if (fade_ == Fade::In && fade_time_ >= kBannerSeconds) {
		fade_ = Fade::None;
	}
}

// Draws the fade over the frame: black going up to full while the cleared
// level fades out, then down again, with the next level's title, as it
// fades in
void Game::ContinueFromStats() {
	cleared_time_ = 0.0;
	fade_time_ = 0.0;
	if (!world_->HasNextLevel()) {
		// The campaign is won: nothing is left to go on with, but how it
		// ended
		if (!IsScripted()) {
			SavedGame::Clear();
			saved_game_.reset();
			DescribeSavedGame();
		}
		TellStory(StoryAtTheEnd(world_->Config().campaign, story_),
				  /*ends_campaign=*/true);
		if (fade_ != Fade::Story) {
			ShowVictory();
		}
		return;
	}
	if (auto next = world_->NextLevel(); !next) {
		std::cerr << "Cannot load the next level: " << next.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	ShowLevel();
	fade_ = Fade::Briefing;
	// A level that opens a chapter is told by its card first
	TellStory(StoryBefore(world_->Config().campaign, world_->LevelIndex(),
						  /*new_game=*/false, story_),
			  /*ends_campaign=*/false);
	SaveProgress();
}

void Game::TellStory(std::size_t count, bool ends_campaign) {
	if (count == 0) {
		return;
	}
	story_count_ = count;
	story_page_ = 0;
	story_ends_campaign_ = ends_campaign;
	fade_ = Fade::Story;
	fade_time_ = 0.0;
}

void Game::NextStoryPage() {
	fade_time_ = 0.0;
	if (++story_page_ < story_count_) {
		return;
	}
	story_count_ = 0;
	if (story_ends_campaign_) {
		ShowVictory();
	}
	else {
		fade_ = Fade::Briefing;
	}
}

void Game::ShowVictory() {
	fade_ = Fade::None;
	renderer_result_.emplace(*renderer_context_,
							 renderer_context_->Textures().GetTextureId("win"));
}

void Game::StartFromBriefing() {
	fade_ = Fade::In;
	fade_banner_ = false;
	fade_time_ = 0.0;
	// The briefing was not play: the level's clock and ticks start now
	clock_.Restart();
	step_.Reset();
	pending_ = {};
}

void Game::DrawTransition() {
	if (fade_ == Fade::None) {
		return;
	}
	double black = 0.0;
	if (fade_ == Fade::Out) {
		black = std::min(fade_time_ / kFadeOutSeconds, 1.0);
	}
	else if (fade_ == Fade::Stats || fade_ == Fade::Story ||
			 fade_ == Fade::Briefing) {
		black = 1.0;
	}
	else {
		black = std::max(1.0 - fade_time_ / kFadeInSeconds, 0.0);
	}
	SDL_Renderer* renderer = renderer_context_->GetRenderer();
	if (black > 0.0) {
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0,
							   static_cast<Uint8>(std::lround(black * 255)));
		SDL_RenderFillRect(renderer, nullptr);
	}
	if (fade_ == Fade::Stats) {
		const ui::FixedText<64> heading("LEVEL {} · {}", world_->LevelNumber(),
										world_->LevelName());
		menu_->DrawLevelStats(heading, cleared_stats_, world_->LevelDebrief(),
							  !IsScripted() && fade_time_ >= kStatsInputDelay);
	}
	if (fade_ == Fade::Story && story_page_ < story_count_) {
		const StoryPage& page = story_[story_page_];
		menu_->DrawStoryPage(page.heading, page.title, page.text, story_page_,
							 story_count_,
							 !IsScripted() && fade_time_ >= kStatsInputDelay);
	}
	if (fade_ == Fade::Briefing) {
		const ui::FixedText<64> heading("LEVEL {} · {}", world_->LevelNumber(),
										world_->LevelName());
		// The objectives' texts, pointing into the level's data
		std::array<std::string_view, 4> objectives{};
		std::size_t count = 0;
		for (const Objective& objective : world_->LevelObjectives()) {
			if (count < objectives.size()) {
				objectives[count++] = objective.text;
			}
		}
		menu_->DrawBriefing(heading, world_->LevelBriefing(),
							std::span(objectives).first(count),
							!IsScripted() && fade_time_ >= kStatsInputDelay);
	}
	if (fade_ == Fade::In && fade_banner_) {
		const double banner = std::clamp(
			(kBannerSeconds - fade_time_) / kBannerFadeSeconds, 0.0, 1.0);
		const ui::FixedText<16> title("LEVEL {}", world_->LevelNumber());
		menu_->DrawLevelBanner(title, world_->LevelName(),
							   static_cast<Uint8>(std::lround(banner * 255)));
	}
}

// Keeps the workload realistic and steady for the whole run: the player walks
// a fixed route through level 1 so enemies wake up, chase and attack, and never
// dies (which would stop player and camera updates)
void Game::BenchmarkStep() {
	constexpr double kWalkSpeed = 2.0;	// map units per second
	constexpr std::size_t kWarmupFrames = 60;

	world_->GetPlayer().IncreaseHealth(100.0);

	auto& profiler = Profiler::GetInstance();
	const double frame_time = clock_.DeltaTime();
	double distance =
		kWalkSpeed * frame_time * static_cast<double>(profiler.GetFrameCount());
	double route_length = 0.0;
	for (std::size_t i = 1; i < kBenchmarkRoute.size(); ++i) {
		route_length += std::hypot(
			kBenchmarkRoute[i].first - kBenchmarkRoute[i - 1].first,
			kBenchmarkRoute[i].second - kBenchmarkRoute[i - 1].second);
	}
	// Walk the route forwards, then backwards, and repeat
	const double lap = std::fmod(distance, 2.0 * route_length);
	const bool backwards = lap > route_length;
	distance = backwards ? 2.0 * route_length - lap : lap;

	for (std::size_t i = 1; i < kBenchmarkRoute.size(); ++i) {
		const auto [x0, y0] = kBenchmarkRoute[i - 1];
		const auto [x1, y1] = kBenchmarkRoute[i];
		const double length = std::hypot(x1 - x0, y1 - y0);
		if (distance <= length || i == kBenchmarkRoute.size() - 1) {
			const double t = std::min(distance / length, 1.0);
			const double direction = backwards ? -1.0 : 1.0;
			world_->GetPlayer().SetPosition(Position2D(
				{x0 + (x1 - x0) * t, y0 + (y1 - y0) * t},
				std::atan2(direction * (y1 - y0), direction * (x1 - x0))));
			break;
		}
		distance -= length;
	}

	if (profiler.GetFrameCount() >=
		static_cast<std::size_t>(benchmark_frames_)) {
		std::cout << "BENCHMARK_RESULT " << profiler.ReportJson(kWarmupFrames)
				  << '\n';
		running_ = false;
	}
}

}  // namespace karakale
