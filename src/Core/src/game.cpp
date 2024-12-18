#include "Animation/looped_animation.h"
#include "Camera/single_raycaster.h"
#include "Characters/enemy.h"
#include "Core/game.h"
#include "Core/scene_loader.h"
#include "GameObjects/dynamic_object.h"
#include "GameObjects/static_object.h"
#include "Math/vector.h"
#include "NavigationManager/navigation_manager.h"
#include "ShootingManager/shooting_manager.h"
#include "SoundManager/sound_manager.h"
#include "State/enemy_state.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_video.h>
#include <functional>
#include <memory>
#include <vector>

namespace wolfenstein {

Game::Game(GeneralConfig& config)
	: config_(config),
	  is_running_(false),
	  is_menu_(true),
	  is_result_(false),
	  render_type_(RenderType::TEXTURE) {
	Init();
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
		if (event.type == SDL_QUIT ||
			(event.type == SDL_EventType::SDL_KEYDOWN &&
			 event.key.keysym.sym == SDLK_ESCAPE)) {
			is_running_ = false;
			is_menu_ = false;
		}

		// When user press a key
		if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_SPACE) {
				player_->SetWeapon(menu_->GetSelectedWeapon());
				is_menu_ = false;
				is_running_ = true;
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
		if (event.type == SDL_QUIT ||
			(event.type == SDL_EventType::SDL_KEYDOWN &&
			 event.key.keysym.sym == SDLK_ESCAPE)) {
			is_running_ = false;
			is_menu_ = false;
		}

		// When user alt tab the window
		if (event.type == SDL_WINDOWEVENT) {
			if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
				SDL_ShowCursor(SDL_ENABLE);
			}
			else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
				SDL_ShowCursor(SDL_DISABLE);
				SDL_WarpMouseInWindow(nullptr, 400, 300);
			}
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

	while (is_menu_) {
		CheckMenuEvent();
		TimeManager::GetInstance().CalculateDeltaTime();
		menu_->Render();
		SDL_WarpMouseInWindow(nullptr, 400, 300);
	}

	while (is_running_) {
		CheckGameEvent();
		TimeManager::GetInstance().CalculateDeltaTime();
		scene_->Update(TimeManager::GetInstance().GetDeltaTime());
		renderer_->RenderScene();

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
		TimeManager::GetInstance().SleepForHz(config_.fps);
	}

	while (is_result_) {
		SDL_Event event;
		SDL_PollEvent(&event);
		if (event.type == SDL_QUIT ||
			(event.type == SDL_EventType::SDL_KEYDOWN &&
			 event.key.keysym.sym == SDLK_ESCAPE)) {
			is_result_ = false;
		}

		TimeManager::GetInstance().CalculateDeltaTime();
		renderer_result_->Render();
	}
}

}  // namespace wolfenstein
