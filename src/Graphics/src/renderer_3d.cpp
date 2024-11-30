#include "Graphics/renderer_3d.h"
#include "Camera/ray.h"
#include "TextureManager/texture_manager.h"

namespace wolfenstein {

void Renderer3D::RenderScene(const std::shared_ptr<Scene>& scene_ptr) {
	RenderQueue render_queue(Compare);
	ClearScreen();
	RenderBackground();
	RenderWalls(scene_ptr->GetMap(), render_queue);
	RenderObjects(scene_ptr->GetObjects(), render_queue);
	RenderWeapon(scene_ptr->GetPlayer(), render_queue);
	RenderTextures(render_queue);
	SDL_RenderPresent(context_->GetRenderer());
}

void Renderer3D::RenderBackground() {
	const auto config = context_->GetConfig();
	auto renderer_ = context_->GetRenderer();
	// Render sky
	auto sky_texture = TextureManager::GetInstance().GetTexture(0);
	SDL_Rect src_rect = {0, 0, sky_texture.width, sky_texture.height};
	SDL_Rect dest_rect = {0, 0, config.width, config.height / 2};
	SDL_RenderCopy(renderer_, sky_texture.texture, &src_rect, &dest_rect);
	// Render ground black
	SDL_SetRenderDrawColor(renderer_, 50, 50, 50, 255);
	SDL_Rect ground_rect = {0, config.height / 2, config.width,
							config.height / 2};
	SDL_RenderFillRect(renderer_, &ground_rect);
}

void Renderer3D::RenderWalls(const std::shared_ptr<Map>& map_ptr,
							 RenderQueue& render_queue) {
	const auto camera_ptr = context_->GetCamera();
	const auto rays = camera_ptr->GetRays();

	int horizontal_slice = 0;
	for (const auto& ray : *rays) {
		if (!ray.is_hit) {
			RenderIfRayHitNot(horizontal_slice, render_queue);
		}
		else {
			RenderIfRayHit(horizontal_slice, ray, render_queue);
		}
		horizontal_slice += 2;
	};
}

void Renderer3D::RenderIfRayHit(const int horizontal_slice, const Ray& ray,
								RenderQueue& render_queue) {
	const auto camera_ptr = context_->GetCamera();
	const auto distance = ray.perpendicular_distance *
						  std::cos(camera_ptr->GetPosition().theta - ray.theta);
	const auto [line_height, draw_start, draw_end] =
		CalculateVerticalSlice(distance);

	auto hit_point = ray.is_hit_vertical ? ray.hit_point.y : ray.hit_point.x;
	hit_point = std::fmod(hit_point, 1.0);
	const auto texture_height =
		TextureManager::GetInstance().GetTexture(ray.wall_id).height;
	const auto texture_width =
		TextureManager::GetInstance().GetTexture(ray.wall_id).width;
	int texture_point = static_cast<int>(hit_point * texture_width);

	SDL_Rect src_rect = {texture_point, 0, 2, texture_height};
	SDL_Rect dest_rect = {horizontal_slice, draw_start, 2, line_height};
	render_queue.push({ray.wall_id, src_rect, dest_rect, distance});
}

void Renderer3D::RenderIfRayHitNot(const int horizontal_slice,
								   RenderQueue& render_queue) {
	const auto config_ = context_->GetConfig();
	const auto [line_height, draw_start, draw_end] =
		CalculateVerticalSlice(config_.view_distance);

	SDL_Rect src_rect = {0, 0, 2,
						 TextureManager::GetInstance().GetTexture(7).height};
	SDL_Rect dest_rect = {horizontal_slice, draw_start, 2, line_height};
	render_queue.push({7, src_rect, dest_rect, config_.view_distance});
}

void Renderer3D::RenderObjects(
	const std::vector<std::shared_ptr<IGameObject>>& objects,
	RenderQueue& render_queue) {
	const auto camera_ptr = context_->GetCamera();
	for (const auto& object : objects) {

		const auto ray_pair = camera_ptr->GetObjectRay(object->GetId());
		if (!ray_pair.has_value()) {
			continue;
		}

		const auto first = ray_pair.value().first;
		const auto last = ray_pair.value().second;

		auto [line_height, draw_start, draw_end] =
			CalculateVerticalSlice(first.perpendicular_distance);
		const auto height = object->GetHeight();
		line_height = line_height * height;
		draw_start = draw_end - line_height;

		const auto texture_height =
			TextureManager::GetInstance().GetTexture(first.wall_id).height;
		const auto texture_width =
			TextureManager::GetInstance().GetTexture(first.wall_id).width;

		const auto first_slice = CalculateHorizontalSlice(first.theta);

		const auto last_slice = CalculateHorizontalSlice(last.theta);

		SDL_Rect src_rect = {0, 0, texture_width, texture_height};

		SDL_Rect dest_rect = {first_slice, draw_start, last_slice - first_slice,
							  line_height};

		render_queue.push(
			{first.wall_id, src_rect, dest_rect, first.perpendicular_distance});
	}
}

int Renderer3D::CalculateHorizontalSlice(const double& angle) {
	const auto camera_ptr = context_->GetCamera();
	const auto config_ = context_->GetConfig();
	const auto horizontal_slice =
		static_cast<int>(angle / camera_ptr->GetDeltaAngle()) * 2 +
		config_.width / 2;

	return horizontal_slice;
}

std::tuple<int, int, int> Renderer3D::CalculateVerticalSlice(
	const double& distance) {
	const auto config_ = context_->GetConfig();
	auto line_height = static_cast<int>(config_.height / distance);
	int draw_start = -line_height / 2 + config_.height / 2;
	int draw_end = line_height / 2 + config_.height / 2;
	return std::make_tuple(line_height, draw_start, draw_end);
}

void Renderer3D::RenderWeapon(const std::shared_ptr<Player>& player_ptr,
							  RenderQueue& render_queue) {
	const auto config_ = context_->GetConfig();

	// Render crosshair
	auto crosshair_texture = TextureManager::GetInstance().GetTexture(6);
	const auto crosshair_height = crosshair_texture.height;
	const auto crosshair_width = crosshair_texture.width;
	SDL_Rect crosshair_src_rect{0, 0, crosshair_width, crosshair_height};
	const double crosshair_ratio =
		static_cast<double>(crosshair_height) / crosshair_width;
	const int crosshair_width_slice = config_.width / 40;
	const int crosshair_height_slice = crosshair_width_slice * crosshair_ratio;
	SDL_Rect crosshair_dest_rect{
		config_.width / 2 - crosshair_width_slice / 2,
		config_.height / 2 - crosshair_height_slice / 2, crosshair_width_slice,
		crosshair_height_slice};
	render_queue.push({6, crosshair_src_rect, crosshair_dest_rect, 0.0});

	auto texture_id = player_ptr->GetTextureId();
	const auto texture_height =
		TextureManager::GetInstance().GetTexture(texture_id).height;
	const auto texture_width =
		TextureManager::GetInstance().GetTexture(texture_id).width;
	const double ratio = static_cast<double>(texture_height) / texture_width;
	const int width_slice = config_.width / 1.3;
	const int height_slice = width_slice * ratio;
	SDL_Rect src_rect{0, 0, texture_width, texture_height};
	SDL_Rect dest_rect{config_.width / 2 - width_slice / 2 + 100,
					   config_.height - height_slice, width_slice,
					   height_slice};
	render_queue.push({texture_id, src_rect, dest_rect, 0.0});
}

void Renderer3D::RenderTextures(RenderQueue& render_queue) {
	auto renderer_ = context_->GetRenderer();
	while (!render_queue.empty()) {
		auto renderable_texture = render_queue.top();
		render_queue.pop();
		const auto texture = TextureManager::GetInstance().GetTexture(
			renderable_texture.texture_id);
		SDL_RenderCopy(renderer_, texture.texture, &renderable_texture.src_rect,
					   &renderable_texture.dest_rect);
	}
}

}  // namespace wolfenstein