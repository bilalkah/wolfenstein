#include "Graphics/renderer_2d.h"
#include "NavigationManager/navigation_manager.h"

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

void Renderer2D::RenderScene(const std::shared_ptr<Scene>& scene_ptr) {
	ClearScreen();
	RenderMap(scene_ptr->GetMap());
	RenderPlayer(scene_ptr->GetPlayer());
	RenderObjects(scene_ptr->GetObjects());
	RenderPaths(scene_ptr->GetEnemies());
	RenderCrosshairs(scene_ptr->GetEnemies());
	SDL_RenderPresent(context_->GetRenderer());
}

void Renderer2D::RenderMap(const std::shared_ptr<Map> map_ptr) {
	const auto& map = map_ptr->GetMap();
	const auto& size_x = map_ptr->GetSizeX();
	const auto& size_y = map_ptr->GetSizeY();
	const auto config = context_->GetConfig();

	SetDrawColor({162, 117, 76, 255});
	for (int i = 0; i < size_x; i++) {
		for (int j = 0; j < size_y; j++) {
			if (map[i][j] != 0) {
				DrawFilledRectangle(
					{config.scale * i, config.scale * j},
					{config.scale * (i + 1), config.scale * (j + 1)});
			}
		}
	}
}

void Renderer2D::RenderPlayer(const std::shared_ptr<Player> player_ptr) {
	const auto config = context_->GetConfig();
	const auto camera_ptr = context_->GetCamera();

	const auto position = player_ptr->GetPosition();
	const auto crosshair_ray = camera_ptr->GetCrosshairRay();

	SetDrawColor({00, 0xA5, 0, 1});
	const auto rays = *camera_ptr->GetRays();
	for (unsigned int i = 0; i < rays.size(); i++) {
		if (!rays[i].is_hit || i % 3 != 0) {
			continue;
		}
		const auto start = ToVector2i(position.pose * config.scale);
		const auto end = ToVector2i((rays[i].hit_point) * config.scale);
		DrawLine(start, end);
	}

	SetDrawColor({255, 0, 0, 255});
	const auto circle_points =
		GenerateCirclePoints(ToVector2i(position.pose * config.scale),
							 config.scale * player_ptr->GetWidth() / 2, 20);
	for (unsigned int i = 0; i < circle_points.size(); i++) {
		SDL_RenderDrawPoint(context_->GetRenderer(), circle_points[i].x,
							circle_points[i].y);
	}
	DrawLine(ToVector2i(crosshair_ray->origin * config.scale),
			 ToVector2i(crosshair_ray->hit_point * config.scale));
}

void Renderer2D::RenderObjects(
	const std::vector<std::shared_ptr<IGameObject>>& objects) {
	const auto config = context_->GetConfig();
	const auto camera_ptr = context_->GetCamera();
	auto renderer_ = context_->GetRenderer();

	for (const auto& object : objects) {

		SetDrawColor({0xFF, 0xA5, 0, 255});
		const auto object_pose = object->GetPose();
		const auto w = object->GetWidth();

		const auto object_points = GenerateCirclePoints(
			ToVector2i(object_pose * config.scale), config.scale * w / 2, 20);
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
		DrawLine(ToVector2i(left_vertex * config.scale),
				 ToVector2i(right_vertex * config.scale));
		const auto left_point =
			GenerateCirclePoints(ToVector2i(left_vertex * config.scale), 1, 20);
		SetDrawColor({0xFF, 0, 0, 255});
		for (unsigned int i = 0; i < left_point.size(); i++) {
			SDL_RenderDrawPoint(renderer_, left_point[i].x, left_point[i].y);
		}

		const auto right_point = GenerateCirclePoints(
			ToVector2i(right_vertex * config.scale), 1, 20);
		SetDrawColor({0, 0xFF, 0, 255});
		for (unsigned int i = 0; i < right_point.size(); i++) {
			SDL_RenderDrawPoint(renderer_, right_point[i].x, right_point[i].y);
		}
	}
}

void Renderer2D::RenderPaths(
	const std::vector<std::shared_ptr<Enemy>>& enemies) {
	const auto config = context_->GetConfig();
	SetDrawColor({0, 0, 255, 255});
	for (const auto& enemy : enemies) {
		const auto path =
			NavigationManager::GetInstance().GetPath(enemy->GetId());
		if (path.size() < 2) {
			continue;
		}
		for (unsigned int i = 0; i < path.size() - 1; i++) {
			DrawLine(ToVector2i(path[i] * config.scale),
					 ToVector2i(path[i + 1] * config.scale));
		}
	}
}

void Renderer2D::RenderCrosshairs(
	const std::vector<std::shared_ptr<Enemy>>& enemies) {
	const auto config = context_->GetConfig();
	SetDrawColor({255, 0, 255, 255});
	for (const auto& enemy : enemies) {
		const auto crosshair_ray = enemy->GetCrosshairRay();
		if (!crosshair_ray.is_hit) {
			continue;
		}
		DrawLine(ToVector2i(enemy->GetPose() * config.scale),
				 ToVector2i((crosshair_ray.hit_point) * config.scale));
	}
}

void Renderer2D::DrawLine(vector2i start, vector2i end) {
	SDL_RenderDrawLine(context_->GetRenderer(), start.x, start.y, end.x, end.y);
}

void Renderer2D::SetDrawColor(SDL_Color color) {
	SDL_SetRenderDrawColor(context_->GetRenderer(), color.r, color.g, color.b,
						   color.a);
}

void Renderer2D::DrawFilledRectangle(vector2i start, vector2i end) {
	SDL_Rect rect{start.x, start.y, end.x - start.x, end.y - start.y};
	SDL_RenderFillRect(context_->GetRenderer(), &rect);
}

}  // namespace wolfenstein
