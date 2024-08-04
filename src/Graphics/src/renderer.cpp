#include "Graphics/renderer.h"
#include "GameObjects/static_object.h"
#include "Map/map.h"
#include "Math/vector.h"
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <TextureManager/texture_manager.h>
#include <cstdlib>

namespace wolfenstein {

Renderer::Renderer(const std::string& window_name, const RenderConfig& config)
	: config_(config) {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	window_ = SDL_CreateWindow(window_name.c_str(), SDL_WINDOWPOS_CENTERED,
							   SDL_WINDOWPOS_CENTERED, config_.width,
							   config_.height, SDL_WINDOW_SHOWN);
	if (window_ == nullptr) {
		SDL_Log("Unable to create window: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
	if (renderer_ == nullptr) {
		SDL_Log("Unable to create renderer: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (config_.fullscreen) {
		SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}

	TextureManager::GetInstance().InitManager(renderer_);
}

Renderer::~Renderer() {
	SDL_DestroyRenderer(renderer_);
	SDL_DestroyWindow(window_);
	SDL_Quit();
}

void Renderer::RenderScene(const std::shared_ptr<Scene>& scene_ptr,
						   const std::shared_ptr<Camera2D>& camera_ptr) {
	RenderQueue render_queue(Compare);
	// ClearScreen();
	RenderBackground();
	RenderWalls(scene_ptr->GetMap(), camera_ptr, render_queue);
	RenderObjects(scene_ptr->GetObjects(), camera_ptr, render_queue);
	RenderTextures(render_queue);
	SDL_RenderPresent(renderer_);
}

void Renderer::RenderBackground() {
	// Render sky
	auto sky_texture = TextureManager::GetInstance().GetTexture(0);
	SDL_Rect src_rect = {0, 0, sky_texture.width, sky_texture.height};
	SDL_Rect dest_rect = {0, 0, config_.width, config_.height / 2};
	SDL_RenderCopy(renderer_, sky_texture.texture, &src_rect, &dest_rect);
	// Render ground black
	SDL_SetRenderDrawColor(renderer_, 50, 50, 50, 255);
	SDL_Rect ground_rect = {0, config_.height / 2, config_.width,
							config_.height / 2};
	SDL_RenderFillRect(renderer_, &ground_rect);
}

void Renderer::RenderWalls(const std::shared_ptr<Map>& map_ptr,
						   const std::shared_ptr<Camera2D>& camera_ptr,
						   RenderQueue& render_queue) {
	camera_ptr->Update(map_ptr);
	const auto rays = camera_ptr->GetRays();

	int horizontal_slice = 0;
	for (const auto& ray : *rays) {
		if (!ray.is_hit) {
			RenderIfRayHitNot(horizontal_slice, render_queue);
		}
		else {
			RenderIfRayHit(horizontal_slice, ray, camera_ptr, render_queue);
		}
		horizontal_slice += 2;
	};
}

void Renderer::RenderIfRayHit(const int horizontal_slice, const Ray& ray,
							  const std::shared_ptr<Camera2D>& camera_ptr,
							  RenderQueue& render_queue) {
	auto distance = ray.perpendicular_distance *
					std::cos(camera_ptr->GetPosition().theta - ray.theta);
	auto line_height = static_cast<int>(config_.height / distance);
	int draw_start = -line_height / 2 + config_.height / 2;
	int draw_end = line_height / 2 + config_.height / 2;

	auto hit_point = ray.is_hit_vertical ? ray.hit_point.y : ray.hit_point.x;
	hit_point = std::fmod(hit_point, 1.0);
	const auto texture_height =
		TextureManager::GetInstance().GetTexture(ray.wall_id).height;
	int texture_point = static_cast<int>(hit_point * texture_height);

	SDL_Rect src_rect = {texture_point, 0, 2, texture_height};
	SDL_Rect dest_rect = {horizontal_slice, draw_start, 2,
						  draw_end - draw_start};
	render_queue.push({ray.wall_id, src_rect, dest_rect, distance});
}

void Renderer::RenderIfRayHitNot(const int horizontal_slice,
								 RenderQueue& render_queue) {
	auto line_height =
		static_cast<int>(config_.height / (config_.view_distance + 0.5));
	int draw_start = -line_height / 2 + config_.height / 2;
	int draw_end = line_height / 2 + config_.height / 2;

	SDL_Rect src_rect = {0, 0, 2,
						 TextureManager::GetInstance().GetTexture(7).height};
	SDL_Rect dest_rect = {horizontal_slice, draw_start, 2,
						  draw_end - draw_start};
	render_queue.push({7, src_rect, dest_rect, config_.view_distance});
}

void Renderer::RenderObjects(
	const std::vector<std::shared_ptr<IGameObject>>& objects,
	const std::shared_ptr<Camera2D>& camera_ptr, RenderQueue& render_queue) {
	for (const auto& object : objects) {
		if (object->GetObjectType() == ObjectType::STATIC_OBJECT) {
			auto static_object =
				std::dynamic_pointer_cast<StaticObject>(object);

			auto object_position = static_object->GetPose();
			auto camera_position = camera_ptr->GetPosition();

			// check if object is in the camera view
			auto object_distance =
				Distance(object_position, camera_position.pose);
			if (object_distance > config_.view_distance) {
				continue;
			}

			// check if object is in the camera fov
			auto object_angle =
				std::atan2(object_position.y - camera_position.pose.y,
						   object_position.x - camera_position.pose.x);

			auto angle_diff = std::abs(camera_position.theta - object_angle);
			if (angle_diff > config_.fov / 2 + +0.174532925) {
				continue;
			}
			auto src_rect = SDL_Rect{0, 0, 897, 1921};
			auto dest_rect = SDL_Rect{
				static_cast<int>(object_position.x * config_.scale),
				static_cast<int>(object_position.y * config_.scale), 200, 200};
			render_queue.push({static_object->GetTextureId(), src_rect,
							   dest_rect, object_distance});
		}
	}
}

void Renderer::RenderTextures(RenderQueue& render_queue) {
	while (!render_queue.empty()) {
		auto renderable_texture = render_queue.top();
		render_queue.pop();
		const auto texture = TextureManager::GetInstance().GetTexture(
			renderable_texture.texture_id);
		SDL_RenderCopy(renderer_, texture.texture, &renderable_texture.src_rect,
					   &renderable_texture.dest_rect);
	}
}

void Renderer::ClearScreen() {
	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
	SDL_RenderClear(renderer_);
}

}  // namespace wolfenstein
