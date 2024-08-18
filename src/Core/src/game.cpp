#include "GameObjects/dynamic_object.h"
#include "GameObjects/static_object.h"
#include <Core/game.h>
#include <Graphics/renderer.h>
#include <Math/vector.h>
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

	scene_ = std::make_shared<Scene>();
	scene_->SetMap(map_);

	time_manager_ = std::make_shared<TimeManager>();

	RenderConfig render_config = {config_.screen_width, config_.screen_height,
								  config_.padding,		config_.scale,
								  config_.fps,			config_.view_distance,
								  config_.fov,			false};
	renderer_ = std::make_shared<Renderer>("Wolfenstein", render_config);

	Camera2DConfig camera_config = {config_.screen_width, config_.fov,
									config_.view_distance};
	camera_ = std::make_shared<Camera2D>(camera_config);

	CharacterConfig player_config = {Position2D({3, 1.5}, 1.50), 2.0, 0.4};
	player_ = std::make_shared<Player>(player_config);

	auto camera_position_updator = [this](Position2D position) {
		camera_->SetPosition(position);
	};

	player_->SetCameraPositionUpdator(
		std::bind(camera_position_updator, std::placeholders::_1));

	std::vector<int> tex_ids = {9, 10, 11, 12};
	const auto green_light =
		std::make_shared<DynamicObject>(vector2d(3.5, 1.9), tex_ids, 0.2, 0.9);
	std::vector<int> tex_ids2 = {86, 87, 88, 89};
	scene_->SetPlayer(player_);
	scene_->AddObject(
		std::make_shared<StaticObject>(vector2d(3.5, 1.5), 8, 0.2, 0.5));
	scene_->AddObject(std::make_shared<DynamicObject>(vector2d(12.5, 9.5),
													  tex_ids2, 0.4, 1.2));
	scene_->AddObject(
		std::make_shared<StaticObject>(vector2d(3.5, 7.5), 8, 0.2, 0.5));
	scene_->AddObject(
		std::make_shared<DynamicObject>(vector2d(3.5, 1.9), tex_ids, 0.2, 0.9));

	scene_->AddObject(std::make_shared<DynamicObject>(vector2d(12.1, 8.15),
													  tex_ids, 0.2, 0.9));
	scene_->AddObject(std::make_shared<DynamicObject>(vector2d(10.9, 8.15),
													  tex_ids, 0.2, 0.9));
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
