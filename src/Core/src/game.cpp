#include "Core/game.h"
#include "Graphics/renderer.h"
#include "Math/vector.h"
#include <functional>

namespace wolfenstein {

Game::Game(GeneralConfig& config) : config_(config) {
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

	Camera2DConfig camera_config = {config_.screen_width / 2, config_.fov,
									config_.view_distance};
	camera_ = std::make_shared<Camera2D>(camera_config);

	CharacterConfig player_config = {Position2D({3, 1.5}, 1.50), 2.0, 5.5};
	player_ = std::make_shared<Player>(player_config);

	auto camera_position_updator = [this](Position2D position) {
		camera_->SetPosition(position);
	};

	player_->SetCameraPositionUpdator(
		std::bind(camera_position_updator, std::placeholders::_1));
	std::shared_ptr<StaticObject> candlebra =
		std::make_shared<StaticObject>(vector2d(3.5, 1.5), 6, 0.1, 0.1);

	scene_->AddObject(player_);
	scene_->AddObject(candlebra);

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
	}
}

void Game::Run() {
	while (is_running_) {
		CheckEvent();
		time_manager_->CalculateDeltaTime();
		scene_->Update(time_manager_->GetDeltaTime());
		renderer_->RenderScene(scene_, camera_);
	}
}

}  // namespace wolfenstein
