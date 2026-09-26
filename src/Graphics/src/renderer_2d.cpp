#include "Graphics/renderer_2d.h"
#include "NavigationManager/navigation_manager.h"
#include <array>
#include <cmath>
#include <numbers>

namespace wolfenstein {

namespace {
constexpr int kCirclePoints = 20;

}  // namespace

Renderer2D::Renderer2D(RendererContext& context)
	: IRenderer(context), batch_(context.GetRenderer()) {}

void Renderer2D::RenderScene(double /*delta_time*/) {
	ClearScreen();
	RenderMap();
	RenderPlayer();
	RenderObjects();
	RenderPaths();
	RenderCrosshairs();
	batch_.Flush();
}

void Renderer2D::RenderMap() {
	const auto cells = scene_->GetMap().GetCells();
	const auto size_x = static_cast<int>(cells.extent(0));
	const auto size_y = static_cast<int>(cells.extent(1));
	const auto& config = context_->GetConfig();

	SetDrawColor({162, 117, 76, 255});
	for (int i = 0; i < size_x; i++) {
		for (int j = 0; j < size_y; j++) {
			if (cells[static_cast<std::size_t>(i),
					  static_cast<std::size_t>(j)] != 0) {
				DrawFilledRectangle(
					{config.scale * i, config.scale * j},
					{config.scale * (i + 1), config.scale * (j + 1)});
			}
		}
	}
}

void Renderer2D::RenderPlayer() {
	const auto& player_ptr = scene_->GetPlayer();
	const auto& config = context_->GetConfig();
	const auto& camera_ptr = context_->GetCamera();

	const auto& position = player_ptr.GetPosition();
	const auto& crosshair_ray = camera_ptr.GetCrosshairRay();

	SetDrawColor({00, 0xA5, 0, 1});
	const auto& rays = camera_ptr.GetRays();
	for (unsigned int i = 0; i < rays.size(); i++) {
		if (!rays[i].is_hit || i % 3 != 0) {
			continue;
		}
		const auto start = ToVector2i(position.pose * config.scale);
		const auto end = ToVector2i((rays[i].hit_point) * config.scale);
		DrawLine(start, end);
	}

	SetDrawColor({255, 0, 0, 255});
	DrawCircle(ToVector2i(position.pose * config.scale),
			   static_cast<int>(config.scale * player_ptr.GetWidth() / 2));
	DrawLine(ToVector2i(crosshair_ray.origin * config.scale),
			 ToVector2i(crosshair_ray.hit_point * config.scale));
}

void Renderer2D::RenderObjects() {
	const auto& objects = scene_->GetObjects();
	const auto& config = context_->GetConfig();
	const auto& camera_ptr = context_->GetCamera();

	for (const auto& object : objects) {
		if (!object->IsVisible()) {
			continue;
		}

		SetDrawColor({0xFF, 0xA5, 0, 255});
		const auto object_pose = object->GetPose();
		const auto w = object->GetWidth();

		DrawCircle(ToVector2i(object_pose * config.scale),
				   static_cast<int>(config.scale * w / 2));

		const auto object_angle =
			std::atan2(object_pose.y - camera_ptr.GetPosition().pose.y,
					   object_pose.x - camera_ptr.GetPosition().pose.x);

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
		SetDrawColor({0xFF, 0, 0, 255});
		DrawCircle(ToVector2i(left_vertex * config.scale), 1);
		SetDrawColor({0, 0xFF, 0, 255});
		DrawCircle(ToVector2i(right_vertex * config.scale), 1);
	}
}

void Renderer2D::RenderPaths() {
	const auto& enemies = scene_->GetEnemies();
	const auto& config = context_->GetConfig();
	SetDrawColor({0, 0, 255, 255});
	for (const auto& enemy : enemies) {
		const auto path = scene_->GetNavigation().GetPath(enemy->GetId());
		for (std::size_t i = 1; i < path.size(); ++i) {
			DrawLine(ToVector2i(NavigationManager::CellCentre(path[i - 1]) *
								config.scale),
					 ToVector2i(NavigationManager::CellCentre(path[i]) *
								config.scale));
		}
	}
}

void Renderer2D::RenderCrosshairs() {
	const auto& enemies = scene_->GetEnemies();
	const auto& config = context_->GetConfig();
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

void Renderer2D::SetDrawColor(SDL_Color color) {
	batch_.SetColor(color);
}

void Renderer2D::DrawFilledRectangle(vector2i start, vector2i end) {
	const auto x0 = static_cast<float>(start.x);
	const auto y0 = static_cast<float>(start.y);
	const auto x1 = static_cast<float>(end.x);
	const auto y1 = static_cast<float>(end.y);
	batch_.AddQuad({x0, y0}, {x1, y0}, {x1, y1}, {x0, y1});
}

// A one-pixel-wide quad along the segment
void Renderer2D::DrawLine(vector2i start, vector2i end) {
	const auto dx = static_cast<float>(end.x - start.x);
	const auto dy = static_cast<float>(end.y - start.y);
	const float length = std::hypot(dx, dy);
	if (length == 0.0f) {
		return;
	}
	const float nx = -dy / length * 0.5f;
	const float ny = dx / length * 0.5f;
	const auto x0 = static_cast<float>(start.x);
	const auto y0 = static_cast<float>(start.y);
	const auto x1 = static_cast<float>(end.x);
	const auto y1 = static_cast<float>(end.y);
	batch_.AddQuad({x0 + nx, y0 + ny}, {x1 + nx, y1 + ny}, {x1 - nx, y1 - ny},
				   {x0 - nx, y0 - ny});
}

// kCirclePoints one-pixel dots around the centre
void Renderer2D::DrawCircle(vector2i center, int radius) {
	const double increment = 2 * std::numbers::pi / kCirclePoints;
	for (int i = 0; i < kCirclePoints; ++i) {
		const double angle = i * increment;
		const auto x = static_cast<float>(center.x + radius * std::cos(angle));
		const auto y = static_cast<float>(center.y + radius * std::sin(angle));
		batch_.AddQuad({x, y}, {x + 1, y}, {x + 1, y + 1}, {x, y + 1});
	}
}

}  // namespace wolfenstein
