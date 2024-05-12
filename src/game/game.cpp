#include "game/game.h"
#include "base/macros.h"
#include "camera/camera.h"
#include "character/i_character.h"
#include "clock/clock.h"
#include "render/render_2d.h"
#include "render/render_3d_simple.h"
#include <memory>
#include <vector>

namespace wolfenstein {

Game::Game(GeneralConfig& config) : config_(config) {
	Init();
	// Hide cursor
	SDL_ShowCursor(SDL_DISABLE);
}

Game::~Game() {}

void Game::Run() {
	while (is_running_) {
		CheckEvent();
		clock_ptr_->CalculateDeltaTime();
		player_ptr_->Update(clock_ptr_->GetDeltaTime());
		render_ptr_->Render(characters_);
	}
}

void Game::Init() {
	Position2D position({3, 1.5}, 1.50);
	PlayerConfig player_config(position, 2.0, 5.5);
	RenderConfig render_config(config_.screen_width, config_.screen_height,
							   config_.padding, config_.scale, config_.fps,
							   config_.view_distance, config_.fov);

	clock_ptr_ = std::make_shared<Clock>();
	map_ptr_ = std::make_shared<Map>();
	render_ptr_ = std::make_unique<Render3DSimple>(render_config);
	render_ptr_->SetMap(map_ptr_);

	player_ptr_ = std::make_shared<Player>(player_config);
	auto camera = std::make_unique<Camera2D>(Camera2DConfig{
		config_.screen_width / 2, config_.fov, config_.view_distance});
	camera->SetMap(map_ptr_);
	player_ptr_->SetCamera(std::move(camera));
	characters_.push_back(player_ptr_);

	is_running_ = true;
	clock_ptr_->InitClock();
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

}  // namespace wolfenstein
