#include "Graphics/minimap.h"
#include "Core/scene.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace wolfenstein {

namespace {

// Walls by map cell value (the wall texture): concrete, brick, moss, demon
// faces, eagle banners
constexpr std::array<SDL_Color, 6> kWallColours{{
	{150, 150, 150, 235},
	{150, 150, 150, 235},
	{176, 74, 58, 235},
	{100, 132, 84, 235},
	{156, 52, 64, 235},
	{84, 100, 156, 235},
}};
constexpr SDL_Color kFloor{56, 56, 60, 215};
constexpr SDL_Color kPanel{0, 0, 0, 150};
constexpr SDL_Color kFrame{210, 190, 150, 170};
constexpr SDL_Color kPlayer{255, 214, 64, 255};
constexpr SDL_Color kDoor{120, 150, 196, 255};
constexpr SDL_Color kGold{232, 188, 64, 255};
constexpr SDL_Color kSilver{214, 220, 230, 255};
constexpr SDL_Color kHealth{230, 40, 40, 255};
constexpr SDL_Color kHealthBack{245, 245, 238, 255};
constexpr SDL_Color kAmmo{212, 168, 60, 255};
constexpr SDL_Color kAmmoEdge{60, 68, 36, 255};

}  // namespace

Minimap::Minimap(RendererContext& context)
	: context_(context), batch_(context.GetRenderer()) {}

void Minimap::Render(const Position2D& player, bool expanded) {
	if (scene_ == nullptr) {
		return;
	}
	const Map& map = scene_->GetMap();
	const int size_x = map.GetSizeX();
	const int size_y = map.GetSizeY();
	const auto& config = context_.GetConfig();
	const auto width = static_cast<float>(config.width);
	const auto height = static_cast<float>(config.height);

	// The panel: a corner of the screen, or most of it
	const float side = expanded ? std::min(width, height) * 0.85f
								: std::min(width, height) * 0.3f;
	const float margin = expanded ? 0.0f : 12.0f;
	const float panel_x = expanded ? (width - side) / 2 : width - side - margin;
	const float panel_y = expanded ? (height - side) / 2 : margin;
	const float padding = side * 0.04f;
	const float cell =
		std::floor(std::min((side - 2 * padding) / static_cast<float>(size_x),
							(side - 2 * padding) / static_cast<float>(size_y)));
	// The level centred in the panel
	const float origin_x =
		panel_x + (side - cell * static_cast<float>(size_x)) / 2;
	const float origin_y =
		panel_y + (side - cell * static_cast<float>(size_y)) / 2;

	batch_.SetColor(kFrame);
	batch_.AddRect(panel_x - 2, panel_y - 2, panel_x + side + 2,
				   panel_y + side + 2);
	batch_.SetColor(kPanel);
	batch_.AddRect(panel_x, panel_y, panel_x + side, panel_y + side);

	const auto cells = map.GetCells();
	for (int x = 0; x < size_x; ++x) {
		for (int y = 0; y < size_y; ++y) {
			if (!scene_->IsExplored(x, y)) {
				continue;
			}
			const std::uint16_t wall =
				cells[static_cast<std::size_t>(x), static_cast<std::size_t>(y)];
			const float left = origin_x + static_cast<float>(x) * cell;
			const float top = origin_y + static_cast<float>(y) * cell;
			if (Map::IsDoorCell(wall)) {
				// Floor, and the door across it: shorter as it slides open
				batch_.SetColor(kFloor);
				batch_.AddRect(left, top, left + cell, top + cell);
				const Door& door = map.GetDoors()[wall - Map::kDoorCell];
				const float shut =
					cell * static_cast<float>(1.0 - door.openness);
				const float half = std::max(cell * 0.15f, 1.0f);
				batch_.SetColor(door.lock == KeyColour::Gold	 ? kGold
								: door.lock == KeyColour::Silver ? kSilver
																 : kDoor);
				if (door.across_x) {  // the plane x + 0.5, along y
					const float middle = left + cell / 2;
					batch_.AddRect(middle - half, top + cell - shut,
								   middle + half, top + cell);
				}
				else {
					const float middle = top + cell / 2;
					batch_.AddRect(left + cell - shut, middle - half,
								   left + cell, middle + half);
				}
				continue;
			}
			batch_.SetColor(wall == 0 ? kFloor
									  : kWallColours[std::min<std::size_t>(
											wall, kWallColours.size() - 1)]);
			batch_.AddRect(left, top, left + cell, top + cell);
		}
	}

	// Pickups still lying where the player has looked: a red cross on white
	// for health, a brass box for ammunition
	const float mark = std::max(std::floor(cell * 0.8f), 4.0f);
	for (const Pickup* pickup : scene_->GetPickups()) {
		const vector2d pose = pickup->GetPose();
		if (pickup->IsTaken() ||
			!scene_->IsExplored(static_cast<int>(std::floor(pose.x)),
								static_cast<int>(std::floor(pose.y)))) {
			continue;
		}
		const float cx = origin_x + static_cast<float>(pose.x) * cell;
		const float cy = origin_y + static_cast<float>(pose.y) * cell;
		const float half = mark / 2;
		if (const std::uint8_t keys = pickup->GetEffect().keys; keys != 0) {
			// A key: a diamond in its metal
			batch_.SetColor((keys & KeyBit(KeyColour::Gold)) != 0 ? kGold
																  : kSilver);
			batch_.AddQuad({cx, cy - half}, {cx + half, cy}, {cx, cy + half},
						   {cx - half, cy});
		}
		else if (pickup->GetEffect().health > 0.0) {
			const float arm = std::max(std::floor(mark / 3), 1.0f) / 2;
			batch_.SetColor(kHealthBack);
			batch_.AddRect(cx - half, cy - half, cx + half, cy + half);
			batch_.SetColor(kHealth);
			batch_.AddRect(cx - half + 1, cy - arm, cx + half - 1, cy + arm);
			batch_.AddRect(cx - arm, cy - half + 1, cx + arm, cy + half - 1);
		}
		else {
			batch_.SetColor(kAmmoEdge);
			batch_.AddRect(cx - half, cy - half, cx + half, cy + half);
			batch_.SetColor(kAmmo);
			batch_.AddRect(cx - half + 1, cy - half + 1, cx + half - 1,
						   cy + half - 1);
		}
	}

	// The player: an arrow pointing where they face
	const float px = origin_x + static_cast<float>(player.pose.x) * cell;
	const float py = origin_y + static_cast<float>(player.pose.y) * cell;
	const float size = std::max(cell * 0.9f, 5.0f);
	const auto corner = [&](double angle, float length) {
		return SDL_FPoint{px + length * static_cast<float>(std::cos(angle)),
						  py + length * static_cast<float>(std::sin(angle))};
	};
	constexpr double kBack = std::numbers::pi * 0.8;
	batch_.SetColor(kPlayer);
	batch_.AddTriangle(corner(player.theta, size * 1.2f),
					   corner(player.theta + kBack, size),
					   corner(player.theta - kBack, size));

	SDL_Renderer* renderer = context_.GetRenderer();
	SDL_BlendMode previous = SDL_BLENDMODE_NONE;
	SDL_GetRenderDrawBlendMode(renderer, &previous);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	batch_.Flush();
	SDL_SetRenderDrawBlendMode(renderer, previous);
}

}  // namespace wolfenstein
