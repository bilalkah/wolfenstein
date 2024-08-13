#include "Graphics/renderer.h"
#include "Camera/ray.h"
#include "GameObjects/static_object.h"
#include "Map/map.h"
#include "Math/vector.h"
#include <Characters/player.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <TextureManager/texture_manager.h>
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
		if (object->GetObjectType() == ObjectType::STATIC_OBJECT) {
			const auto static_object =
				std::dynamic_pointer_cast<StaticObject>(object);

			const auto object_ray = camera_ptr->CalculateObjectRay(object);
			if (!object_ray.is_hit) {
				continue;
			}

			const auto w = static_object->GetWidth();

			const auto [line_height, draw_start, draw_end] =
				CalculateVerticalSlice(object_ray.perpendicular_distance);

			SDL_Rect src_rect = {0, 0,
								 TextureManager::GetInstance()
									 .GetTexture(object_ray.wall_id)
									 .width,
								 TextureManager::GetInstance()
									 .GetTexture(object_ray.wall_id)
									 .height};
			const auto horizontal_slice =
				CalculateHorizontalSlice(object_ray, camera_ptr);

			SDL_Rect dest_rect = {horizontal_slice, draw_start,
								  static_cast<int>(w * line_height),
								  line_height};
			render_queue.push({object_ray.wall_id, src_rect, dest_rect,
							   object_ray.perpendicular_distance});
		}
	}
}

int Renderer::CalculateHorizontalSlice(
	const Ray& ray, const std::shared_ptr<Camera2D> camera_ptr) {
	auto cam_theta = camera_ptr->GetPosition().theta;
	const auto vector_of_crosshair =
		vector2d{std::cos(cam_theta), std::sin(cam_theta)};
	const auto vector_of_ray =
		vector2d{std::cos(ray.theta), std::sin(ray.theta)};

	const auto angle_between = CalculateAngleBetweenTwoVectorsSigned(
		vector_of_ray, vector_of_crosshair);

	const auto horizontal_slice =
		static_cast<int>(angle_between / camera_ptr->GetDeltaAngle()) * 2 +
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
	RenderObjects(scene_ptr->GetObjects(), camera_ptr);
	SDL_RenderPresent(renderer_);
}

void Renderer::RenderObjects(
	const std::vector<std::shared_ptr<IGameObject>>& objects,
	const std::shared_ptr<Camera2D> camera_ptr) {

	for (const auto& object : objects) {
		if (object->GetObjectType() == ObjectType::CHARACTER_PLAYER) {
			auto player = std::dynamic_pointer_cast<Player>(object);
			const auto position = player->GetPosition();
			const auto crosshair_ray = camera_ptr->GetCrosshairRay();

			SetDrawColor({00, 0xA5, 0, 1});
			const auto rays = *camera_ptr->GetRays();
			for (unsigned int i = 0; i < rays.size(); i++) {
				if (!rays[i].is_hit || i % 3 != 0) {
					continue;
				}
				const auto start = ToVector2i(position.pose * config_.scale);
				const auto end =
					ToVector2i((rays[i].hit_point) * config_.scale);
				DrawLine(start, end);
			}

			SetDrawColor({255, 0, 0, 255});
			const auto circle_points = GenerateCirclePoints(
				ToVector2i(position.pose * config_.scale), 10, 20);
			for (unsigned int i = 0; i < circle_points.size(); i++) {
				SDL_RenderDrawPoint(renderer_, circle_points[i].x,
									circle_points[i].y);
			}
			DrawLine(ToVector2i(position.pose * config_.scale),
					 ToVector2i((crosshair_ray->origin +
								 crosshair_ray->direction * 10) *
								config_.scale));
		}

		if (object->GetObjectType() == ObjectType::STATIC_OBJECT) {
			const auto object_ray = camera_ptr->CalculateObjectRay(object);
			if (!object_ray.is_hit) {
				continue;
			}
			auto cam_theta = camera_ptr->GetPosition().theta;
			const auto vector_of_crosshair =
				vector2d{std::cos(cam_theta), std::sin(cam_theta)};
			const auto vector_of_ray = vector2d{std::cos(object_ray.theta),
												std::sin(object_ray.theta)};

			const auto angle_between = CalculateAngleBetweenTwoVectorsSigned(
				vector_of_ray, vector_of_crosshair);
			if (std::abs(angle_between) > ToRadians(45.0)) {
				continue;
			}

			auto static_object =
				std::dynamic_pointer_cast<StaticObject>(object);
			SetDrawColor({0xFF, 0xA5, 0, 255});
			const auto object_pose = static_object->GetPose();
			const auto w = static_object->GetWidth();

			const auto object_points = GenerateCirclePoints(
				ToVector2i(object_pose * config_.scale), 10, 20);
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
				vector2d{w * std::cos(to_left), w * std::sin(to_left)};
			const auto right_vertex =
				object_pose +
				vector2d{w * std::cos(to_right), w * std::sin(to_right)};
			DrawLine(ToVector2i(left_vertex * config_.scale),
					 ToVector2i(right_vertex * config_.scale));
			const auto left_point = GenerateCirclePoints(
				ToVector2i(left_vertex * config_.scale), 5, 20);
			SetDrawColor({0xFF, 0, 0, 255});
			for (unsigned int i = 0; i < left_point.size(); i++) {
				SDL_RenderDrawPoint(renderer_, left_point[i].x,
									left_point[i].y);
			}

			const auto right_point = GenerateCirclePoints(
				ToVector2i(right_vertex * config_.scale), 5, 20);
			SetDrawColor({0, 0xFF, 0, 255});
			for (unsigned int i = 0; i < right_point.size(); i++) {
				SDL_RenderDrawPoint(renderer_, right_point[i].x,
									right_point[i].y);
			}
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
