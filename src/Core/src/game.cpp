#include "Core/game.h"
#include "Animation/time_based_single_animation.h"
#include "Animation/walk_animation.h"
#include "Characters/enemy.h"
#include "GameObjects/dynamic_object.h"
#include "GameObjects/static_object.h"
#include "Graphics/renderer.h"
#include "Math/vector.h"
#include "NavigationManager/navigation_manager.h"
#include <SDL2/SDL_video.h>
#include <functional>
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
	NavigationManager::GetInstance().InitManager(map_);

	scene_ = std::make_shared<Scene>();
	scene_->SetMap(map_);

	time_manager_ = std::make_shared<TimeManager>();

	RenderConfig render_config = {config_.screen_width, config_.screen_height,
								  config_.padding,		config_.scale,
								  config_.fps,			config_.view_distance,
								  config_.fov,			config_.fullscreen};
	renderer_ = std::make_shared<Renderer>("Wolfenstein", render_config);

	Camera2DConfig camera_config = {config_.screen_width, config_.fov,
									config_.view_distance};
	camera_ = std::make_shared<Camera2D>(camera_config);

	CharacterConfig player_config = {Position2D({3, 1.5}, 1.50), 2.0, 0.4};
	player_ = std::make_shared<Player>(player_config);

	player_->SubscribeToPlayerPosition(std::bind(
		[this](Position2D position) { camera_->SetPosition(position); },
		std::placeholders::_1));

	std::vector<int> caco_walk_tex_ids = {38, 39, 40};
	const WalkAnimation walk_animation(caco_walk_tex_ids, 0.2);
	auto enemy = std::make_shared<Enemy>(
		CharacterConfig(Position2D({8, 7}, 1.50), 0.8, 0.4), 0.4, 0.8,
		walk_animation);

	std::vector<int> tex_ids = {9, 10, 11, 12};
	const auto animation_green_light = TBSAnimation(tex_ids, 0.1);
	std::vector<int> tex_ids_2 = {13, 14, 15, 16};
	const auto animation_red_light = TBSAnimation(tex_ids_2, 0.1);

	std::vector<int> tex_ids2 = {86, 87, 88, 89};
	scene_->SetPlayer(player_);
	scene_->AddObject(enemy);

	scene_->AddObject(
		std::make_shared<StaticObject>(vector2d(14.5, 9), 8, 0.2, 0.5));

	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(12.1, 8.15),
		std::make_shared<TBSAnimation>(animation_red_light), 0.2, 0.9));
	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(10.9, 8.15),
		std::make_shared<TBSAnimation>(animation_red_light), 0.2, 0.9));

	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(9.9, 10.9),
		std::make_shared<TBSAnimation>(animation_green_light), 0.2, 0.9));
	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(9.9, 13.10),
		std::make_shared<TBSAnimation>(animation_green_light), 0.2, 0.9));
	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(12.1, 13.1),
		std::make_shared<TBSAnimation>(animation_green_light), 0.2, 0.9));
	scene_->AddObject(std::make_shared<DynamicObject>(
		vector2d(12.1, 10.9),
		std::make_shared<TBSAnimation>(animation_green_light), 0.2, 0.9));
	is_running_ = true;
	time_manager_->InitClock();
}

void Game::CheckEvent() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// When user close the window
		if (event.type == SDL_QUIT ||
			(event.type == SDL_EventType::SDL_KEYDOWN &&
			 event.key.keysym.sym == SDLK_ESCAPE)) {
			is_running_ = false;
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
			}
			else if (event.key.keysym.sym == SDLK_p &&
					 render_type_ == RenderType::LINE) {
				render_type_ = RenderType::TEXTURE;
			}
		}
	}
}

void Game::Run() {
	while (is_running_) {
		CheckEvent();
		time_manager_->CalculateDeltaTime();
		scene_->Update(time_manager_->GetDeltaTime());
		camera_->Update(scene_);
		switch (render_type_) {
			case RenderType::TEXTURE:
				renderer_->RenderScene(scene_, camera_);
				break;
			case RenderType::LINE:
				renderer_->RenderScene2D(scene_, camera_);
				break;
		}
	}
}

}  // namespace wolfenstein
