#include "Graphics/renderer.h"
#include "Camera/ray.h"
#include "Characters/player.h"
#include "GameObjects/static_object.h"
#include "Map/map.h"
#include "Math/vector.h"
#include "NavigationManager/navigation_manager.h"
#include "TextureManager/texture_manager.h"
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <cmath>
#include <cstdlib>
#include <memory>
namespace wolfenstein {

namespace {
std::vector<vector2i> GenerateCirclePoints(vector2i center, int radius,
										   int num_points) {
	const double increment = 2 * M_PI / num_points;

	std::vector<vector2i> points;
	for (int i = 0; i < num_points; ++i) {
		double angle = i * increment;
		int x = static_cast<int>(center.x + radius * std::cos(angle));
		int y = static_cast<int>(center.y + radius * std::sin(angle));
		points.push_back({x, y});
	}

	return points;
}

}  // namespace

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
	ClearScreen();
	RenderBackground();
	RenderWalls(scene_ptr->GetMap(), camera_ptr, render_queue);
	RenderObjects(scene_ptr->GetObjects(), camera_ptr, render_queue);
	RenderWeapon(scene_ptr->GetPlayer(), render_queue);
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

void Renderer::RenderIfRayHitNot(const int horizontal_slice,
								 RenderQueue& render_queue) {
	const auto [line_height, draw_start, draw_end] =
		CalculateVerticalSlice(config_.view_distance);

	SDL_Rect src_rect = {0, 0, 2,
						 TextureManager::GetInstance().GetTexture(7).height};
	SDL_Rect dest_rect = {horizontal_slice, draw_start, 2, line_height};
	render_queue.push({7, src_rect, dest_rect, config_.view_distance});
}

void Renderer::RenderObjects(
	const std::vector<std::shared_ptr<IGameObject>>& objects,
	const std::shared_ptr<Camera2D>& camera_ptr, RenderQueue& render_queue) {
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

		const auto first_slice =
			CalculateHorizontalSlice(first.theta, camera_ptr);

		const auto last_slice =
			CalculateHorizontalSlice(last.theta, camera_ptr);

		SDL_Rect src_rect = {0, 0, texture_width, texture_height};

		SDL_Rect dest_rect = {first_slice, draw_start, last_slice - first_slice,
							  line_height};

		render_queue.push(
			{first.wall_id, src_rect, dest_rect, first.perpendicular_distance});
	}
}

int Renderer::CalculateHorizontalSlice(
	const double& angle, const std::shared_ptr<Camera2D> camera_ptr) {

	const auto horizontal_slice =
		static_cast<int>(angle / camera_ptr->GetDeltaAngle()) * 2 +
		config_.width / 2;

	return horizontal_slice;
}

std::tuple<int, int, int> Renderer::CalculateVerticalSlice(
	const double& distance) {
	auto line_height = static_cast<int>(config_.height / distance);
	int draw_start = -line_height / 2 + config_.height / 2;
	int draw_end = line_height / 2 + config_.height / 2;
	return std::make_tuple(line_height, draw_start, draw_end);
}

void Renderer::RenderWeapon(const std::shared_ptr<Player>& player_ptr,
							RenderQueue& render_queue) {
	// Render crosshair
	auto crosshair_texture = TextureManager::GetInstance().GetTexture(6);
	const auto crosshair_height = crosshair_texture.height;
	const auto crosshair_width = crosshair_texture.width;
	SDL_Rect crosshair_src_rect{0, 0, crosshair_width, crosshair_height};
	const double crosshair_ratio =
		static_cast<double>(crosshair_height) / crosshair_width;
	const int crosshair_width_slice = config_.width / 40;
	const int crosshair_height_slice = crosshair_width_slice * crosshair_ratio;
	SDL_Rect crosshair_dest_rect{config_.width / 2 - crosshair_width_slice / 2,
								 config_.height / 2 - crosshair_height_slice / 2,
								 crosshair_width_slice, crosshair_height_slice};
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

void Renderer::RenderScene2D(const std::shared_ptr<Scene>& scene_ptr,
							 const std::shared_ptr<Camera2D>& camera_ptr) {
	ClearScreen();
	RenderMap(scene_ptr->GetMap());
	RenderPlayer(scene_ptr->GetPlayer(), camera_ptr);
	RenderObjects(scene_ptr->GetObjects(), camera_ptr);
	RenderPaths(scene_ptr->GetEnemies());
	SDL_RenderPresent(renderer_);
}

void Renderer::RenderPlayer(const std::shared_ptr<Player> player_ptr,
							const std::shared_ptr<Camera2D> camera_ptr) {

	const auto position = player_ptr->GetPosition();
	const auto crosshair_ray = camera_ptr->GetCrosshairRay();

	SetDrawColor({00, 0xA5, 0, 1});
	const auto rays = *camera_ptr->GetRays();
	for (unsigned int i = 0; i < rays.size(); i++) {
		if (!rays[i].is_hit || i % 3 != 0) {
			continue;
		}
		const auto start = ToVector2i(position.pose * config_.scale);
		const auto end = ToVector2i((rays[i].hit_point) * config_.scale);
		DrawLine(start, end);
	}

	SetDrawColor({255, 0, 0, 255});
	const auto circle_points =
		GenerateCirclePoints(ToVector2i(position.pose * config_.scale),
							 config_.scale * player_ptr->GetWidth() / 2, 20);
	for (unsigned int i = 0; i < circle_points.size(); i++) {
		SDL_RenderDrawPoint(renderer_, circle_points[i].x, circle_points[i].y);
	}
	DrawLine(
		ToVector2i(position.pose * config_.scale),
		ToVector2i((crosshair_ray->origin + crosshair_ray->direction * 10) *
				   config_.scale));
}

void Renderer::RenderObjects(
	const std::vector<std::shared_ptr<IGameObject>>& objects,
	const std::shared_ptr<Camera2D> camera_ptr) {

	for (const auto& object : objects) {

		SetDrawColor({0xFF, 0xA5, 0, 255});
		const auto object_pose = object->GetPose();
		const auto w = object->GetWidth();

		const auto object_points = GenerateCirclePoints(
			ToVector2i(object_pose * config_.scale), config_.scale * w / 2, 20);
		for (unsigned int i = 0; i < object_points.size(); i++) {
			SDL_RenderDrawPoint(renderer_, object_points[i].x,
								object_points[i].y);
		}

		const auto object_angle =
			std::atan2(object_pose.y - camera_ptr->GetPosition().pose.y,
					   object_pose.x - camera_ptr->GetPosition().pose.x);

		const auto to_left = SubRadian(object_angle, ToRadians(90.0));
		const auto to_right = SumRadian(object_angle, ToRadians(90.0));
		const auto left_vertex =
			object_pose +
			vector2d{w / 2 * std::cos(to_left), w / 2 * std::sin(to_left)};
		const auto right_vertex =
			object_pose +
			vector2d{w / 2 * std::cos(to_right), w / 2 * std::sin(to_right)};
		DrawLine(ToVector2i(left_vertex * config_.scale),
				 ToVector2i(right_vertex * config_.scale));
		const auto left_point = GenerateCirclePoints(
			ToVector2i(left_vertex * config_.scale), 1, 20);
		SetDrawColor({0xFF, 0, 0, 255});
		for (unsigned int i = 0; i < left_point.size(); i++) {
			SDL_RenderDrawPoint(renderer_, left_point[i].x, left_point[i].y);
		}

		const auto right_point = GenerateCirclePoints(
			ToVector2i(right_vertex * config_.scale), 1, 20);
		SetDrawColor({0, 0xFF, 0, 255});
		for (unsigned int i = 0; i < right_point.size(); i++) {
			SDL_RenderDrawPoint(renderer_, right_point[i].x, right_point[i].y);
		}
	}
}

void Renderer::RenderPaths(const std::vector<std::shared_ptr<Enemy>>& enemies) {
	SetDrawColor({0, 0, 255, 255});
	for (const auto& enemy : enemies) {
		const auto path =
			NavigationManager::GetInstance().GetPath(enemy->GetId());
		if (path.size() < 2) {
			continue;
		}
		for (unsigned int i = 0; i < path.size() - 1; i++) {
			DrawLine(ToVector2i(path[i] * config_.scale),
					 ToVector2i(path[i + 1] * config_.scale));
		}
	}
}

void Renderer::DrawLine(vector2i start, vector2i end) {
	SDL_RenderDrawLine(renderer_, start.x, start.y, end.x, end.y);
}

void Renderer::RenderMap(const std::shared_ptr<Map> map_ptr) {

	const auto& map = map_ptr->GetMap();
	const auto& size_x = map_ptr->GetSizeX();
	const auto& size_y = map_ptr->GetSizeY();

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

void Renderer::SetDrawColor(SDL_Color color) {
	SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

void Renderer::DrawFilledRectangle(vector2i start, vector2i end) {
	SDL_Rect rect{start.x, start.y, end.x - start.x, end.y - start.y};
	SDL_RenderFillRect(renderer_, &rect);
}

}  // namespace wolfenstein
