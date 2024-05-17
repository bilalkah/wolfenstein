#include "render/render_3d_simple.h"
#include "character/player.h"
#include "render/utils.h"

namespace wolfenstein {

Render3DSimple::Render3DSimple(RenderConfig config) : config_(config) {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
	}

	window_ = SDL_CreateWindow("Some Game", SDL_WINDOWPOS_UNDEFINED,
							   SDL_WINDOWPOS_UNDEFINED, config.width,
							   config.height, SDL_WINDOW_SHOWN);
	if (window_ == nullptr) {
		SDL_Log("Unable to create window: %s", SDL_GetError());
	}

	renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
	if (renderer_ == nullptr) {
		SDL_Log("Unable to create renderer: %s", SDL_GetError());
	}

	// SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);

	color_map_[1] = {0, 255, 0, 255};
	color_map_[2] = {0, 0, 255, 255};
	color_map_[3] = {255, 255, 0, 255};
	color_map_[4] = {255, 0, 255, 255};
	color_map_[5] = {0, 255, 255, 255};
}

Render3DSimple::~Render3DSimple() {
	SDL_DestroyRenderer(renderer_);
	SDL_DestroyWindow(window_);
	SDL_Quit();
}

void Render3DSimple::Render(
	std::vector<std::shared_ptr<ICharacter>> characters) {
	ClearScreen();
	RenderPlayer(characters);
	SDL_RenderPresent(renderer_);
}

void Render3DSimple::SetMap(std::shared_ptr<wolfenstein::Map> map) {
	map_ = map;
}

void Render3DSimple::RenderPlayer(
	std::vector<std::shared_ptr<ICharacter>> characters) {
	if (map_ == nullptr) {
		return;
	}

	const auto player = std::dynamic_pointer_cast<Player>(characters[0]);
	const auto rays = player->GetRays();
	const auto map = map_->GetMap();
	int start_x = 0;
	for (const auto& ray : *rays) {
		if (!ray.is_hit) {
			continue;
		}
		auto distance = ray.perpendicular_distance *
						std::cos(player->GetPosition().theta - ray.theta);

		auto line_height = static_cast<int>(config_.height / distance);

		int draw_start = -line_height / 2 + config_.height / 2;
		int draw_end = line_height / 2 + config_.height / 2;

		if (draw_start < 0) {
			draw_start = 0;
		}
		if (draw_end >= config_.height) {
			draw_end = config_.height - 1;
		}
		if (ray.is_hit_vertical) {
			SetDrawColor({255, 255, 255, 255});
		}
		else 
		{
			SetDrawColor(color_map_[ray.wall_id]);
		}
		DrawLine({start_x, draw_start}, {start_x, draw_end});
		DrawLine({start_x + 1, draw_start}, {start_x + 1, draw_end});
		start_x += 2;
	}
}

void Render3DSimple::ClearScreen() {
	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
	SDL_RenderClear(renderer_);
}

void Render3DSimple::SetDrawColor(SDL_Color color) {
	SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

void Render3DSimple::DrawPixel(vector2i point) {
	SDL_RenderDrawPoint(renderer_, point.x, point.y);
}

void Render3DSimple::DrawLine(vector2i start, vector2i end) {
	SDL_RenderDrawLine(renderer_, start.x, start.y, end.x, end.y);
}

void Render3DSimple::DrawCircle(vector2i center, int radius) {
	const auto circle_points = GenerateCirclePoints(center, radius, 20);
	for (unsigned int i = 0; i < circle_points.size(); i++) {
		DrawPixel(circle_points[i]);
	}
}

void Render3DSimple::DrawRectangle(vector2i start, vector2i end) {
	SDL_Rect rect{start.x, start.y, end.x - start.x, end.y - start.y};
	SDL_RenderDrawRect(renderer_, &rect);
}

void Render3DSimple::DrawFilledRectangle(vector2i start, vector2i end) {
	SDL_Rect rect{start.x, start.y, end.x - start.x, end.y - start.y};
	SDL_RenderFillRect(renderer_, &rect);
}

}  // namespace wolfenstein
