#include "render/render_3d_texture.h"
#include "base/macros.h"
#include "character/player.h"
#include "render/i_render.h"
#include "render/utils.h"
#include <SDL2/SDL_image.h>

namespace wolfenstein {

Render3DTexture::Render3DTexture(RenderConfig config) : config_(config) {
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

	std::string texture_path = std::string(RESOURCE_DIR) + "textures/";
	texture_map_[0] =
		IMG_LoadTexture(renderer_, (texture_path + "sky.png").c_str());
	texture_map_[1] =
		IMG_LoadTexture(renderer_, (texture_path + "1.png").c_str());
	texture_map_[2] =
		IMG_LoadTexture(renderer_, (texture_path + "2.png").c_str());
	texture_map_[3] =
		IMG_LoadTexture(renderer_, (texture_path + "3.png").c_str());
	texture_map_[4] =
		IMG_LoadTexture(renderer_, (texture_path + "4.png").c_str());
	texture_map_[5] =
		IMG_LoadTexture(renderer_, (texture_path + "5.png").c_str());
}

Render3DTexture::~Render3DTexture() {
	for (auto& texture : texture_map_) {
		SDL_DestroyTexture(texture.second);
	}
	SDL_DestroyRenderer(renderer_);
	SDL_DestroyWindow(window_);
	SDL_Quit();
}

void Render3DTexture::Render(
	std::vector<std::shared_ptr<ICharacter>> characters) {
	ClearScreen();
	// Render sky
	SDL_Rect sky_rect = {0, 0, config_.width, config_.height / 2};
	SDL_RenderCopy(renderer_, texture_map_[0], nullptr, &sky_rect);

	// Render ground
	SDL_Rect ground_rect = {0, config_.height / 2, config_.width,
							config_.height / 2};
	SDL_SetRenderDrawColor(renderer_, 50, 50, 50, 255);
	SDL_RenderFillRect(renderer_, &ground_rect);

	RenderPlayer(characters);
	SDL_RenderPresent(renderer_);
}

void Render3DTexture::SetMap(std::shared_ptr<wolfenstein::Map> map) {
	map_ = map;
}

void Render3DTexture::RenderPlayer(
	std::vector<std::shared_ptr<ICharacter>> characters) {
	if (map_ == nullptr) {
		return;
	}

	const auto player = std::dynamic_pointer_cast<Player>(characters[0]);
	const auto rays = player->GetRays();
	const auto map = map_->GetMap();
	int start_x = 0;
	const auto texture_size = 1024;
	for (const auto& ray : *rays) {
		if (!ray.is_hit) {
			continue;
		}
		auto distance = ray.perpendicular_distance *
						std::cos(player->GetPosition().theta - ray.theta);

		auto line_height = static_cast<int>(config_.height / distance);

		int draw_start = -line_height / 2 + config_.height / 2;
		int draw_end = line_height / 2 + config_.height / 2;
		const auto height_scale = 1.0 * texture_size / line_height;
		SDL_Rect src_rect;
		if (draw_start < 0) {
			src_rect.y = (0 - draw_start) * height_scale;
			// DEBUG_MSG("draw_start: " << draw_start);
			draw_start = 0;
		}
		else {
			src_rect.y = 0;
		}
		if (draw_end >= config_.height) {
			src_rect.h = config_.height * height_scale;
			// DEBUG_MSG("draw_end: " << draw_end);
			draw_end = config_.height - 1;
		}
		else {
			src_rect.h = texture_size;
		}

		const auto texture = texture_map_[ray.wall_id];
		auto hit_point =
			ray.is_hit_vertical ? ray.hit_point.y : ray.hit_point.x;
		hit_point = std::fmod(hit_point, 1.0);
		int texture_point = static_cast<int>(hit_point * texture_size);
		// src_rect = {texture_point, 0, 2, texture_size};
		src_rect.x = texture_point;
		src_rect.w = 2;
		SDL_Rect dest_rect = {start_x, draw_start, 2, draw_end - draw_start};
		SDL_RenderCopy(renderer_, texture, &src_rect, &dest_rect);
		start_x += 2;
	}
};

void Render3DTexture::ClearScreen() {
	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
	SDL_RenderClear(renderer_);
}

void Render3DTexture::RenderTexture(int texture_id, vector2i start,
									vector2i end) {

};

};	// namespace wolfenstein
