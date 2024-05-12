/**
 * @file render_2d.cpp
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 2D render class for rendering objects on screen
 * @version 0.1
 * @date 2024-05-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "render/render_2d.h"
#include "base/types.h"
#include "character/player.h"
#include "map/map.h"
#include "render/utils.h"
#include <memory>
#include <vector>

namespace wolfenstein {

Render2D::Render2D(RenderConfig config) : config_(config) {

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

	SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

Render2D::~Render2D() {
	SDL_DestroyRenderer(renderer_);
	SDL_DestroyWindow(window_);
	SDL_Quit();
}

void Render2D::Render(std::vector<std::shared_ptr<ICharacter>> characters) {
	ClearScreen();
	RenderMap();
	RenderPlayer(characters);
	SDL_RenderPresent(renderer_);
}

void Render2D::SetMap(std::shared_ptr<wolfenstein::Map> map) {
	map_ = map;
}

void Render2D::RenderMap() {
	if (map_ == nullptr) {
		return;
	}
	const auto& map = map_->GetMap();
	const auto& size_x = map_->GetSizeX();
	const auto& size_y = map_->GetSizeY();

	SetDrawColor({162, 117, 76, 255});

	for (int i = 0; i < size_x; i++) {
		for (int j = 0; j < size_y; j++) {
			if (map[i][j] != 0) {
				DrawFilledRectangle(
					{config_.scale * i, config_.scale * j},
					{config_.scale * (i + 1), config_.scale * (j + 1)});
			}
		}
	}
}

void Render2D::RenderPlayer(
	std::vector<std::shared_ptr<ICharacter>> characters) {
	const auto player = std::dynamic_pointer_cast<Player>(characters[0]);
	const auto position = player->GetPosition();
	const auto crosshair_ray = player->GetCrosshairRay();

	SetDrawColor({0xFF, 0xA5, 0, 255});
	const auto rays = player->GetRays();
	for (const auto& ray : *rays) {
		if (!ray.is_hit) {
			continue;
		}
		const auto start = ToVector2i(position.pose * config_.scale);
		const auto end = ToVector2i((ray.hit_point) * config_.scale);
		DrawLine(start, end);
	}

	SetDrawColor({255, 0, 0, 255});
	DrawCircle(ToVector2i(position.pose * config_.scale), 10);
	DrawLine(ToVector2i(position.pose * config_.scale),
			 ToVector2i(crosshair_ray->hit_point * config_.scale));
}

void Render2D::ClearScreen() {
	SetDrawColor({0, 0, 0, 255});
	SDL_RenderClear(renderer_);
}

void Render2D::SetDrawColor(SDL_Color color) {
	SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

void Render2D::DrawPixel(vector2i point) {
	SDL_RenderDrawPoint(renderer_, point.x, point.y);
}

void Render2D::DrawLine(vector2i start, vector2i end) {
	SDL_RenderDrawLine(renderer_, start.x, start.y, end.x, end.y);
}

void Render2D::DrawCircle(vector2i center, int radius) {
	const auto circle_points = GenerateCirclePoints(center, radius, 20);
	for (unsigned int i = 0; i < circle_points.size(); i++) {
		DrawPixel(circle_points[i]);
	}
}

void Render2D::DrawRectangle(vector2i start, vector2i end) {
	SDL_Rect rect{start.x, start.y, end.x - start.x, end.y - start.y};
	SDL_RenderDrawRect(renderer_, &rect);
}

void Render2D::DrawFilledRectangle(vector2i start, vector2i end) {
	SDL_Rect rect{start.x, start.y, end.x - start.x, end.y - start.y};
	SDL_RenderFillRect(renderer_, &rect);
}

}  // namespace wolfenstein
