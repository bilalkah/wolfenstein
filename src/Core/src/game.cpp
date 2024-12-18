#include "Animation/looped_animation.h"
#include "Camera/single_raycaster.h"
#include "Characters/enemy.h"
#include "GameObjects/dynamic_object.h"
#include "GameObjects/static_object.h"
#include "Math/vector.h"
#include "NavigationManager/navigation_manager.h"
#include "ShootingManager/shooting_manager.h"
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
	: config_(config), is_running_(false), render_type_(RenderType::TEXTURE) {
	Init();
	// Hide cursor
	SDL_ShowCursor(SDL_DISABLE);
}

Game::~Game() {}

void Game::Init() {

	map_ = std::make_shared<Map>();

	CollisionManager::GetInstance().InitManager(map_);
	SingleRayCasterService::GetInstance().InitService(map_);
	scene_ = std::make_shared<Scene>();
	scene_->SetMap(map_);

	Camera2DConfig camera_config = {config_.screen_width, config_.fov,
									config_.view_distance};
	auto camera_ = std::make_shared<Camera2D>(camera_config, scene_);

	RenderConfig render_config = {config_.screen_width, config_.screen_height,
								  config_.padding,		config_.scale,
								  config_.fps,			config_.view_distance,
								  config_.fov,			config_.fullscreen};

	renderer_context_ = std::make_shared<RendererContext>(
		"Wolfenstein", render_config, camera_);

	renderer_ = std::make_shared<Renderer3D>(renderer_context_);
	renderer_->SetScene(scene_);

	menu_ = std::make_shared<Menu>(renderer_context_);

	CharacterConfig player_config = {Position2D({3, 1.5}, 1.50), 2.0, 0.4, 0.4,
									 1.0};
	player_ = std::make_shared<Player>(player_config, camera_);

	player_->SubscribeToPlayerPosition(std::bind(
		[camera_](Position2D position) { camera_->SetPosition(position); },
		std::placeholders::_1));
	player_->SubscribeToPlayerPosition(std::bind(
		[](Position2D position) {
			NavigationManager::GetInstance().SubscribePlayerPosition(position);
		},
		std::placeholders::_1));

	player_->SubscribeToPlayerPosition(std::bind(
		[](Position2D position) {
			SingleRayCasterService::GetInstance().SubscribePlayerPose(
				position.pose);
		},
		std::placeholders::_1));

	scene_->SetPlayer(player_);

	PrepareEnemies();
	PrepareDynamicObjects();
	PrepareStaticObjects();

	ShootingManager::GetInstance().InitManager(map_, player_,
											   scene_->GetEnemies());
	NavigationManager::GetInstance().InitManager(map_, scene_->GetEnemies());
	TimeManager::GetInstance().InitClock();
	is_running_ = false;
	is_result_ = false;
	is_menu_ = true;
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
				renderer_ = std::make_shared<Renderer2D>(renderer_context_);
				renderer_->SetScene(scene_);
			}
			else if (event.key.keysym.sym == SDLK_p &&
					 render_type_ == RenderType::LINE) {
				render_type_ = RenderType::TEXTURE;
				renderer_ = std::make_shared<Renderer3D>(renderer_context_);
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
			is_running_ = false;
			is_result_ = true;
			renderer_result_ = std::make_shared<RendererResult>(
				renderer_context_,
				TextureManager::GetInstance().GetTexture(10));
		}
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

void Game::PrepareEnemies() {
	auto caco_demon = EnemyFactory::CreateEnemy(
		"caco_demon",
		CharacterConfig(Position2D({13.5, 2.5}, 1.50), 0.8, 0.4, 0.4, 0.8));

	auto soldier = EnemyFactory::CreateEnemy(
		"soldier",
		CharacterConfig(Position2D({9, 7}, 1.50), 0.8, 0.4, 0.3, 0.6));

	auto soldier_2 = EnemyFactory::CreateEnemy(
		"soldier",
		CharacterConfig(Position2D({9, 8}, 1.50), 0.8, 0.4, 0.3, 0.6));

	auto cyber_demon = EnemyFactory::CreateEnemy(
		"cyber_demon",
		CharacterConfig(Position2D({23.0, 4}, 1.50), 0.8, 0.4, 0.5, 1.0));

	auto cyber_demon_2 = EnemyFactory::CreateEnemy(
		"cyber_demon",
		CharacterConfig(Position2D({20.0, 4}, 1.50), 0.8, 0.4, 0.5, 1.0));

	scene_->AddObject(caco_demon);
	scene_->AddObject(soldier);
	scene_->AddObject(soldier_2);
	scene_->AddObject(cyber_demon);
	scene_->AddObject(cyber_demon_2);
}

void Game::PrepareDynamicObjects() {
	const auto animation_green_light = LoopedAnimation(
		TextureManager::GetInstance().GetTextureCollection("green_light"), 0.1);
	const auto animation_red_light = LoopedAnimation(
		TextureManager::GetInstance().GetTextureCollection("red_light"), 0.1);

	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(12.1, 8.15),
		std::make_unique<LoopedAnimation>(animation_red_light), 0.2, 0.9));
	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(10.9, 8.15),
		std::make_unique<LoopedAnimation>(animation_red_light), 0.2, 0.9));

	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(9.9, 10.9),
		std::make_unique<LoopedAnimation>(animation_green_light), 0.2, 0.9));
	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(9.9, 13.10),
		std::make_unique<LoopedAnimation>(animation_green_light), 0.2, 0.9));
	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(12.1, 13.1),
		std::make_unique<LoopedAnimation>(animation_green_light), 0.2, 0.9));
	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(12.1, 10.9),
		std::make_unique<LoopedAnimation>(animation_green_light), 0.2, 0.9));
}

void Game::PrepareStaticObjects() {
	scene_->AddObject(
		std::make_shared<StaticObject>(vector2d(14.5, 9), 8, 0.2, 0.5));
}

}  // namespace wolfenstein
