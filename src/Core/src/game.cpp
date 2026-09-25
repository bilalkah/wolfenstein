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

constexpr int kGameOverTexture = 10;
constexpr int kWinTexture = 11;
// Time between the end of a level (or death) and what follows it
constexpr double kEndOfLevelDelay = 2.0;

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
	camera_ = std::make_shared<Camera2D>(camera_config);

	RenderConfig render_config = {config_.screen_width, config_.screen_height,
								  config_.padding,		config_.scale,
								  config_.fps,			config_.view_distance,
								  config_.fov,			config_.fullscreen};

	renderer_context_ = std::make_shared<RendererContext>(
		"Wolfenstein", render_config, *camera_);
	auto world = World::Create(renderer_context_->Textures(), RESOURCE_DIR);
	if (!world) {
		std::cerr << "Cannot start the game: " << world.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	world_ = std::move(*world);
	menu_ = std::make_unique<Menu>(renderer_context_, world_->Sound());
	renderer_3d_ = std::make_unique<Renderer3D>(renderer_context_);
	renderer_2d_ = std::make_unique<Renderer2D>(renderer_context_);
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

void Game::NewGame(const std::string& weapon_name) {
	if (auto started = world_->NewGame(weapon_name); !started) {
		std::cerr << "Cannot start a game: " << started.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	render_type_ = RenderType::TEXTURE;
	renderer_ = renderer_3d_.get();
	ShowLevel();

	renderer_result_.reset();
	level_transition_time_ = 0.0;
	result_delay_time_ = 0.0;
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
	NewGame("mp5");
	const std::chrono::duration<double, std::milli> load_time =
		std::chrono::steady_clock::now() - load_start;
	Profiler::GetInstance().AddStartupTime(load_time.count());
	Profiler::GetInstance().SetLevelLoad(
		load_time.count(), AllocationStats::count - load_allocations,
		AllocationStats::bytes - load_bytes);
	state_ = GameState::Playing;
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
	renderer_result_->Render(clock_.DeltaTime());
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
	// In the browser requestAnimationFrame already paces the frames
	clock_.SleepForHz(config_.fps);
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
	world_->GetPlayer().SetCommand(IsBenchmark() ? PlayerCommand{}
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
		if (IsBenchmark()) {
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
	if (!IsBenchmark()) {
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
		renderer_result_ = std::make_unique<RendererResult>(renderer_context_,
															kGameOverTexture);
	}
	if (world_->CurrentLevel().GetNumberOfAliveEnemies() == 0 &&
		!renderer_result_) {
		if (world_->HasNextLevel()) {
			level_transition_time_ += delta_time;
			if (level_transition_time_ >= kEndOfLevelDelay) {
				if (auto next = world_->NextLevel(); !next) {
					std::cerr << "Cannot load the next level: " << next.error()
							  << '\n';
					std::exit(EXIT_FAILURE);
				}
				ShowLevel();
				level_transition_time_ = 0.0;
			}
		}
		else {
			renderer_result_ = std::make_unique<RendererResult>(
				renderer_context_, kWinTexture);
		}
	}

	if (renderer_result_) {
		result_delay_time_ += delta_time;
		if (result_delay_time_ >= kEndOfLevelDelay) {
			state_ = GameState::Result;
			SDL_SetRelativeMouseMode(SDL_FALSE);
			menu_->Open(MenuScreen::Result);
		}
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
				  << std::endl;
		running_ = false;
	}
}

}  // namespace wolfenstein
