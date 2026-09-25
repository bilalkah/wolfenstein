#include "Core/game.h"
#include "Animation/looped_animation.h"
#include "Characters/enemy.h"
#include "GameObjects/dynamic_object.h"
#include "GameObjects/static_object.h"
#include "Math/vector.h"
#include "Profiler/profiler.h"
#include "Settings/settings.h"
#include "SoundManager/sound_manager.h"
#include "State/enemy_state.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_video.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace wolfenstein {

namespace {

// Time between the end of a level (or death) and what follows it
constexpr double kEndOfLevelDelay = 2.0;
// A cleared level: a moment to take it in, then a fade to black, the next
// level built at black, and a fade back in under its title
constexpr double kClearedPause = 1.0;
constexpr double kFadeOutSeconds = 0.8;
constexpr double kFadeInSeconds = 1.0;
// The level's title stays up a little longer than the fade, then fades too
constexpr double kBannerSeconds = 2.4;
constexpr double kBannerFadeSeconds = 0.6;
// Frames the soak session spends at the main menu before starting a game
constexpr int kSoakMenuFrames = 60;

}  // namespace

Game::Game(GeneralConfig& config) : config_(config) {
	const auto init_start = std::chrono::steady_clock::now();
	Init();
	const std::chrono::duration<double, std::milli> init_time =
		std::chrono::steady_clock::now() - init_start;
	Profiler::GetInstance().AddStartupTime(init_time.count());
}

void Game::Init() {
	Camera2DConfig camera_config = {config_.screen_width, config_.fov,
									config_.view_distance};
	camera_ = std::make_unique<Camera2D>(camera_config);

	RenderConfig render_config = {config_.screen_width, config_.screen_height,
								  config_.padding,		config_.scale,
								  config_.fps,			config_.view_distance,
								  config_.fov,			config_.fullscreen};

	renderer_context_ = std::make_unique<RendererContext>(
		"Wolfenstein", render_config, *camera_);
	auto world = World::Create(renderer_context_->Textures(), RESOURCE_DIR);
	if (!world) {
		std::cerr << "Cannot start the game: " << world.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	world_ = std::move(*world);
	// Room for the largest level's objects, so switching levels does not
	// grow the camera's per-object views
	camera_->ReserveViews(world_->LargestLevelObjects());
	menu_ = std::make_unique<Menu>(*renderer_context_, world_->Sound(),
								   world_->Config().weapons);
	renderer_3d_ = std::make_unique<Renderer3D>(*renderer_context_);
	renderer_2d_ = std::make_unique<Renderer2D>(*renderer_context_);
	ApplySettings();
}

// The world has just replaced its level, destroying the previous one: every
// view is pointed at the new one before anything draws again
void Game::ShowLevel() {
	renderer_3d_->SetScene(world_->CurrentLevel());
	renderer_2d_->SetScene(world_->CurrentLevel());
	// Loading is not a frame: the next frame starts timing from here
	clock_.Restart();
	step_.Reset();
}

void Game::NewGame(std::string_view weapon_name, std::string_view level) {
	if (auto started = world_->NewGame(weapon_name, level); !started) {
		std::cerr << "Cannot start a game: " << started.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	render_type_ = RenderType::TEXTURE;
	renderer_ = renderer_3d_.get();
	ShowLevel();

	renderer_result_.reset();
	result_delay_time_ = 0.0;
	cleared_time_ = 0.0;
	// A new game fades in under the first level's title; the benchmark
	// measures the plain game from its first frame
	fade_ = IsBenchmark() ? Fade::None : Fade::In;
	fade_time_ = 0.0;
}

void Game::EnterPlaying() {
	state_ = GameState::Playing;
	had_pointer_lock_ = false;
	SDL_SetRelativeMouseMode(SDL_TRUE);
	// Drop mouse motion that happened in the menu
	SDL_GetRelativeMouseState(nullptr, nullptr);
}

void Game::Pause() {
	state_ = GameState::Paused;
	SDL_SetRelativeMouseMode(SDL_FALSE);
	menu_->Open(MenuScreen::Pause);
}

void Game::HandleMenuAction(const MenuAction& action) {
	switch (action.type) {
		case MenuAction::Type::None:
			break;
		case MenuAction::Type::StartGame:
			NewGame(action.weapon);
			EnterPlaying();
			break;
		case MenuAction::Type::Resume:
			EnterPlaying();
			break;
		case MenuAction::Type::QuitToMenu:
			state_ = GameState::Menu;
			SDL_SetRelativeMouseMode(SDL_FALSE);
			menu_->Open(MenuScreen::Main);
			break;
		case MenuAction::Type::Quit:
			running_ = false;
			break;
		case MenuAction::Type::SettingsChanged:
			ApplySettings();
			break;
	}
}

void Game::ApplySettings() {
	world_->Sound().SetMasterVolume(Settings::Get().volume);
}

void Game::Present() {
	SDL_RenderPresent(renderer_context_->GetRenderer());
}

void Game::StartBenchmark(int frames) {
	constexpr double kFrameTime = 1.0 / 60.0;
	benchmark_frames_ = frames;
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
	state_ = GameState::Playing;
}

// Plays a scripted session through everything a player can reach once a
// game has started, and reports every allocation it makes: there must be
// none. Like a player, it starts at the main menu; startup and the menu
// before the first game (when, on the web, the audio device makes its first
// callbacks) are not counted.
void Game::StartSoak(int frames) {
	soak_frames_ = frames + kSoakMenuFrames;
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
	static constexpr std::array<Phase, 9> kPhases = {{
		{0, "play_3d"},
		{90, "view_2d"},
		{180, "pause"},
		{240, "settings"},
		{300, "play"},
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
			{.type = MenuAction::Type::StartGame,
			 .weapon = world_->Config().weapons.front().weapon_name});
	}
	const int frame = soak_frame_++ - kSoakMenuFrames;
	if (soak_first_allocation_ < 0 &&
		AllocationStats::count != soak_allocations_) {
		// Allocated during the previous frame
		soak_first_allocation_ = frame - 1;
	}
	soak_max_level_ = std::max(soak_max_level_, world_->LevelNumber());
	soak_saw_result_ = soak_saw_result_ || state_ == GameState::Result;
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
	else if (frame == 180) {
		render_type_ = RenderType::TEXTURE;
		renderer_ = renderer_3d_.get();
		Pause();
	}
	else if (frame == 240) {
		menu_->Open(MenuScreen::Settings);
	}
	else if (frame > 240 && frame < 300) {
		// Nudge a setting every frame: its value is redrawn as new text
		SDL_Event right{};
		right.type = SDL_KEYDOWN;
		right.key.keysym.sym = SDLK_RIGHT;
		menu_->HandleEvent(right);
	}
	else if (frame == 300) {
		HandleMenuAction({.type = MenuAction::Type::Resume, .weapon = {}});
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
	else if (frame == 700) {
		world_->GetPlayer().DecreaseHealth(1000.0);
	}
	else if (frame == 900) {
		HandleMenuAction(
			{.type = MenuAction::Type::StartGame,
			 .weapon = world_->Config().weapons.back().weapon_name});
	}
	else if (frame >= soak_frames_ - kSoakMenuFrames) {
		std::cout << "SOAK_RESULT {\"frames\":" << frame
				  << ",\"max_level\":" << soak_max_level_
				  << ",\"saw_result\":" << (soak_saw_result_ ? "true" : "false")
				  << ",\"first_allocating_frame\":" << soak_first_allocation_
				  << ",\"allocations\":"
				  << AllocationStats::count - soak_allocations_
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
		if (event.type == SDL_QUIT) {
			running_ = false;
			return false;
		}
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

// The game stays frozen behind the pause, controls and settings screens
void Game::PausedTick() {
	if (!PollMenuEvents()) {
		return;
	}
	clock_.Tick();
	renderer_->RenderScene(clock_.DeltaTime());
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
	if (state_ == GameState::Playing) {
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

// Reads this frame's player input from the keyboard and mouse
PlayerCommand Game::SampleCommand() const {
	const Uint8* keys = SDL_GetKeyboardState(nullptr);
	const auto axis = [keys](SDL_Scancode positive, SDL_Scancode negative) {
		return static_cast<std::int8_t>(keys[positive] - keys[negative]);
	};
	PlayerCommand command;
	command.forward = axis(SDL_SCANCODE_W, SDL_SCANCODE_S);
	command.strafe = axis(SDL_SCANCODE_D, SDL_SCANCODE_A);
	command.turn = axis(SDL_SCANCODE_RIGHT, SDL_SCANCODE_LEFT);

	// Relative mouse mode reports motion since the last call, which also
	// works under browser pointer lock (unlike warping the cursor)
	constexpr double kRadiansPerPixel = 0.005;
	int dx = 0;
	SDL_GetRelativeMouseState(&dx, nullptr);
	if (SDL_GetRelativeMouseMode()) {
		command.look =
			dx * kRadiansPerPixel * Settings::Get().mouse_sensitivity;
	}

	command.fire =
		(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0 ||
		keys[SDL_SCANCODE_LCTRL] != 0;
	command.reload = keys[SDL_SCANCODE_R] != 0;
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
	world_->GetPlayer().SetCommand(IsScripted() ? PlayerCommand{}
												: SampleCommand());

	// The simulation advances in fixed ticks, whatever the frame rate, so
	// the same commands always play out the same way. A long stall (a
	// breakpoint, a hidden browser tab) is dropped rather than caught up in
	// a burst of ticks.
	for (int ticks = step_.Advance(clock_.DeltaTime()); ticks > 0; --ticks) {
		world_->CurrentLevel().Update(step_.TickSeconds());
	}
	// Frames fall between ticks: the view is drawn this far from the last
	// tick towards the next, so motion stays smooth at any frame rate
	const double alpha = step_.Alpha();
	{
		ScopedTimer timer(ProfileSection::Camera);
		camera_->Update(world_->GetPlayer().GetRenderPosition(alpha), alpha);
	}
	renderer_->RenderScene(clock_.DeltaTime());
	DrawTransition();
	ScopedTimer timer(ProfileSection::Present);
	Present();
}

void Game::CheckGameEvent() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			running_ = false;
			return;
		}
		if (IsScripted()) {
			continue;
		}
		if (event.type == SDL_WINDOWEVENT &&
			event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
			Pause();
			return;
		}
		if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_ESCAPE) {
				Pause();
				return;
			}
			if (event.key.keysym.sym == SDLK_p) {
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

#ifdef __EMSCRIPTEN__
	// Browsers release the pointer lock on Esc without passing the key on, so
	// losing the lock is what pauses the game there
	if (!IsScripted()) {
		EmscriptenPointerlockChangeEvent status;
		if (emscripten_get_pointerlock_status(&status) ==
			EMSCRIPTEN_RESULT_SUCCESS) {
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

void Game::CheckGameOver() {
	const double delta_time = clock_.DeltaTime();
	if (!world_->GetPlayer().IsAlive() && !renderer_result_) {
		renderer_result_.emplace(
			*renderer_context_,
			renderer_context_->Textures().GetTextureId("game_over"));
	}
	if (world_->CurrentLevel().GetNumberOfAliveEnemies() == 0 &&
		!renderer_result_ && fade_ != Fade::Out) {
		cleared_time_ += delta_time;
		if (cleared_time_ >= kClearedPause) {
			if (world_->HasNextLevel()) {
				fade_ = Fade::Out;
				fade_time_ = 0.0;
			}
			else {
				renderer_result_.emplace(
					*renderer_context_,
					renderer_context_->Textures().GetTextureId("win"));
			}
		}
	}
	AdvanceTransition(delta_time);

	if (renderer_result_) {
		result_delay_time_ += delta_time;
		if (result_delay_time_ >= kEndOfLevelDelay) {
			state_ = GameState::Result;
			SDL_SetRelativeMouseMode(SDL_FALSE);
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
		if (auto next = world_->NextLevel(); !next) {
			std::cerr << "Cannot load the next level: " << next.error() << '\n';
			std::exit(EXIT_FAILURE);
		}
		ShowLevel();
		cleared_time_ = 0.0;
		fade_ = Fade::In;
		fade_time_ = 0.0;
	}
	else if (fade_ == Fade::In && fade_time_ >= kBannerSeconds) {
		fade_ = Fade::None;
	}
}

// Draws the fade over the frame: black going up to full while the cleared
// level fades out, then down again, with the next level's title, as it
// fades in
void Game::DrawTransition() {
	if (fade_ == Fade::None) {
		return;
	}
	double black = 0.0;
	if (fade_ == Fade::Out) {
		black = std::min(fade_time_ / kFadeOutSeconds, 1.0);
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
	if (fade_ == Fade::In) {
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
	// Wall-free route through level1's map as (row, column) waypoints; the
	// player walks it at normal speed facing forward, then walks it back
	constexpr std::array<std::pair<double, double>, 7> kRoute = {{
		{3.0, 1.5},
		{9.5, 1.5},
		{9.5, 13.5},
		{16.5, 13.5},
		{16.5, 8.5},
		{23.5, 8.5},
		{23.5, 1.5},
	}};
	constexpr double kWalkSpeed = 2.0;	// map units per second
	constexpr std::size_t kWarmupFrames = 60;

	world_->GetPlayer().IncreaseHealth(100.0);

	auto& profiler = Profiler::GetInstance();
	const double frame_time = clock_.DeltaTime();
	double distance =
		kWalkSpeed * frame_time * static_cast<double>(profiler.GetFrameCount());
	double route_length = 0.0;
	for (std::size_t i = 1; i < kRoute.size(); ++i) {
		route_length += std::hypot(kRoute[i].first - kRoute[i - 1].first,
								   kRoute[i].second - kRoute[i - 1].second);
	}
	// Walk the route forwards, then backwards, and repeat
	const double lap = std::fmod(distance, 2.0 * route_length);
	const bool backwards = lap > route_length;
	distance = backwards ? 2.0 * route_length - lap : lap;

	for (std::size_t i = 1; i < kRoute.size(); ++i) {
		const auto [x0, y0] = kRoute[i - 1];
		const auto [x1, y1] = kRoute[i];
		const double length = std::hypot(x1 - x0, y1 - y0);
		if (distance <= length || i == kRoute.size() - 1) {
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

}  // namespace wolfenstein
