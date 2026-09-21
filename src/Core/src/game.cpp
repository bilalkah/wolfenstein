#include "Animation/looped_animation.h"
#include "Camera/single_raycaster.h"
#include "Characters/enemy.h"
#include "Core/game.h"
#include "Core/scene_loader.h"
#include "GameObjects/dynamic_object.h"
#include "GameObjects/static_object.h"
#include "Math/vector.h"
#include "NavigationManager/navigation_manager.h"
#include "Profiler/profiler.h"
#include "ShootingManager/shooting_manager.h"
#include "SoundManager/sound_manager.h"
#include "State/enemy_state.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
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
#endif

namespace wolfenstein {

namespace {

// In the browser Esc is reserved for releasing the pointer lock, so it must not
// also close the game there
bool IsQuitEvent(const SDL_Event& event) {
	if (event.type == SDL_QUIT) {
		return true;
	}
#ifdef __EMSCRIPTEN__
	return false;
#else
	return event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE;
#endif
}

}  // namespace

Game::Game(GeneralConfig& config)
	: config_(config),
	  is_running_(false),
	  is_menu_(true),
	  is_result_(false),
	  render_type_(RenderType::TEXTURE) {
	const auto init_start = std::chrono::steady_clock::now();
	Init();
	const std::chrono::duration<double, std::milli> init_time =
		std::chrono::steady_clock::now() - init_start;
	Profiler::GetInstance().AddStartupTime(init_time.count());
	// Hide cursor
	SDL_ShowCursor(SDL_DISABLE);
}

Game::~Game() {}

void Game::Init() {

	Camera2DConfig camera_config = {config_.screen_width, config_.fov,
									config_.view_distance};
	auto camera_ = std::make_shared<Camera2D>(camera_config);

	RenderConfig render_config = {config_.screen_width, config_.screen_height,
								  config_.padding,		config_.scale,
								  config_.fps,			config_.view_distance,
								  config_.fov,			config_.fullscreen};

	renderer_context_ = std::make_shared<RendererContext>(
		"Wolfenstein", render_config, *camera_);

	renderer_ = std::make_unique<Renderer3D>(renderer_context_);
	menu_ = std::make_unique<Menu>(renderer_context_);

	CharacterConfig player_config = {Position2D({3, 1.5}, 1.50), 2.0, 0.4, 0.4,
									 1.0};
	player_ = std::make_shared<Player>(player_config, camera_);

	scene_ = SceneLoader::GetInstance().Load("level1.json", player_);
	renderer_->SetScene(scene_);

	camera_->SetPositionPtr(player_->GetPositionPtr());
	NavigationManager::GetInstance().SetPositionPtr(player_->GetPositionPtr());
	SingleRayCasterService::GetInstance().SetDestinationPtr(
		player_->GetPositionPtr());
}

void Game::CheckMenuEvent() {

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// When user close the window
		if (IsQuitEvent(event)) {
			is_running_ = false;
			is_menu_ = false;
		}

		// When user press a key
		if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_SPACE) {
				player_->SetWeapon(menu_->GetSelectedWeapon());
				is_menu_ = false;
				is_running_ = true;
				SDL_SetRelativeMouseMode(SDL_TRUE);
			}
			// left arrow
			if (event.key.keysym.sym == SDLK_LEFT) {
				menu_->ChangeSelection(-1);
			}
			// right arrow
			if (event.key.keysym.sym == SDLK_RIGHT) {
				menu_->ChangeSelection(1);
			}
		}
	}
}

void Game::CheckGameEvent() {

	SDL_Event event;
	while (SDL_PollEvent(&event)) {

		// When user close the window
		if (IsQuitEvent(event)) {
			is_running_ = false;
			is_menu_ = false;
		}

		// When user press a key
		if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_p &&
				render_type_ == RenderType::TEXTURE) {
				render_type_ = RenderType::LINE;
				renderer_ = std::make_unique<Renderer2D>(renderer_context_);
				renderer_->SetScene(scene_);
			}
			else if (event.key.keysym.sym == SDLK_p &&
					 render_type_ == RenderType::LINE) {
				render_type_ = RenderType::TEXTURE;
				renderer_ = std::make_unique<Renderer3D>(renderer_context_);
				renderer_->SetScene(scene_);
			}
		}
	}
}

void Game::Run() {
#ifdef __EMSCRIPTEN__
	// The browser drives the loop: one Tick per animation frame
	emscripten_set_main_loop_arg(
		[](void* game_ptr) {
			auto* game = static_cast<Game*>(game_ptr);
			if (!game->Tick()) {
				emscripten_cancel_main_loop();
				// Start over from the menu instead of leaving a frozen canvas
				if (!game->IsBenchmark()) {
					emscripten_run_script("location.reload()");
				}
			}
		},
		this, 0, true);
#else
	while (Tick()) {}
#endif
}

bool Game::Tick() {
	if (is_menu_) {
		MenuTick();
	}
	else if (is_running_) {
		GameTick();
	}
	else if (is_result_) {
		ResultTick();
	}
	return is_menu_ || is_running_ || is_result_;
}

void Game::MenuTick() {
	CheckMenuEvent();
	TimeManager::GetInstance().CalculateDeltaTime();
	menu_->Render();
}

void Game::StartBenchmark(int frames) {
	constexpr double kFrameTime = 1.0 / 60.0;
	benchmark_frames_ = frames;
	TimeManager::GetInstance().SetFixedDeltaTime(kFrameTime);
	Profiler::GetInstance().Enable(frames);
	player_->SetWeapon(menu_->GetSelectedWeapon());
	is_menu_ = false;
	is_running_ = true;
}

bool Game::IsBenchmark() const {
	return benchmark_frames_ > 0;
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
	CheckGameOver();
#ifndef __EMSCRIPTEN__
	// In the browser requestAnimationFrame already paces the frames
	TimeManager::GetInstance().SleepForHz(config_.fps);
#endif
}

void Game::UpdateAndRender() {
	CheckGameEvent();
	TimeManager::GetInstance().CalculateDeltaTime();
	scene_->Update(TimeManager::GetInstance().GetDeltaTime());
	renderer_->RenderScene();
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

	player_->IncreaseHealth(100.0);

	auto& profiler = Profiler::GetInstance();
	const double frame_time = TimeManager::GetInstance().GetDeltaTime();
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
			player_->SetPosition(Position2D(
				{x0 + (x1 - x0) * t, y0 + (y1 - y0) * t},
				std::atan2(direction * (y1 - y0), direction * (x1 - x0))));
			break;
		}
		distance -= length;
	}

	if (profiler.GetFrameCount() >= static_cast<std::size_t>(benchmark_frames_)) {
		std::cout << "BENCHMARK_RESULT " << profiler.ReportJson(kWarmupFrames)
				  << std::endl;
		is_running_ = false;
	}
}

void Game::CheckGameOver() {
	// Check if player is dead
	if (!player_->IsAlive()) {
		renderer_result_ =
			std::make_unique<RendererResult>(renderer_context_, 10);
	}
	if (scene_->GetNumberOfAliveEnemies() == 0) {
		if (scene_->GetNextScene() != "") {
			[this]() {
				static double time_pass = 0;
				time_pass += TimeManager::GetInstance().GetDeltaTime();
				if (time_pass >= 2.0) {
					const auto next = scene_->GetNextScene();
					scene_ = SceneLoader::GetInstance().Load(next, player_);
					renderer_->SetScene(scene_);
					time_pass = 0;
				}
			}();
		}
		else {
			renderer_result_ =
				std::make_unique<RendererResult>(renderer_context_, 11);
		}
	}

	if (renderer_result_) {
		[this]() {
			static double time_pass = 0;
			time_pass += TimeManager::GetInstance().GetDeltaTime();
			if (time_pass >= 2.0) {
				is_running_ = false;
				is_result_ = true;
			}
		}();
	}
}

void Game::ResultTick() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT ||
			(event.type == SDL_EventType::SDL_KEYDOWN &&
			 (event.key.keysym.sym == SDLK_ESCAPE ||
			  event.key.keysym.sym == SDLK_SPACE))) {
			is_result_ = false;
		}
	}

	TimeManager::GetInstance().CalculateDeltaTime();
	renderer_result_->Render();
}

}  // namespace wolfenstein
