#include "Graphics/renderer_3d.h"
#include "Camera/ray.h"
#include "Profiler/profiler.h"
#include "Settings/settings.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
namespace wolfenstein {

namespace {

// Seconds between refreshes of the FPS counter
constexpr double kFpsRefreshSeconds = 0.25;
// Collection index of the '%' sign after the health digits
constexpr int kPercentDigit = 10;

// Writes the decimal digits of value (most significant first) into digits
// and returns how many there are; no allocation, unlike building a list
std::size_t ToDigits(int value, std::array<int, 10>& digits) {
	std::size_t count = 0;
	do {
		digits[count++] = value % 10;
		value /= 10;
	} while (value > 0 && count < digits.size());
	std::reverse(digits.begin(), digits.begin() + static_cast<long>(count));
	return count;
}

}  // namespace

void Renderer3D::TextureDeleter::operator()(
	SDL_Texture* texture) const noexcept {
	SDL_DestroyTexture(texture);
}

Renderer3D::Renderer3D(std::shared_ptr<RendererContext> context)
	: IRenderer(std::move(context)) {
	// One command per wall column (2 px wide), plus objects and the weapon
	constexpr std::size_t kExtraCommands = 64;
	render_queue_.reserve(
		static_cast<std::size_t>(context_->GetConfig().width) / 2 +
		kExtraCommands);
}

void Renderer3D::Enqueue(int texture_id, const SDL_Rect& src_rect,
						 const SDL_Rect& dest_rect, double distance) {
	render_queue_.push_back({texture_id, src_rect, dest_rect, distance,
							 static_cast<std::uint32_t>(render_queue_.size())});
}

void Renderer3D::RenderScene() {
	ScopedTimer render_timer(ProfileSection::Render);
	render_queue_.clear();
	ClearScreen();
	RenderBackground();
	{
		ScopedTimer timer(ProfileSection::RenderWalls);
		RenderWalls();
	}
	{
		ScopedTimer timer(ProfileSection::RenderObjects);
		RenderObjects();
		RenderWeapon();
	}
	{
		ScopedTimer timer(ProfileSection::RenderDraw);
		RenderTextures();
	}
	ScopedTimer timer(ProfileSection::RenderHud);
	RenderHUD();
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

void Renderer3D::RenderWalls() {
	const auto& camera_ptr = context_->GetCamera();
	const auto& rays = camera_ptr.GetRays();

	int horizontal_slice = 0;
	for (const auto& ray : rays) {
		if (!ray.is_hit) {
			RenderIfRayHitNot(horizontal_slice);
		}
		else {
			RenderIfRayHit(horizontal_slice, ray);
		}
		horizontal_slice += 2;
	};
}

void Renderer3D::RenderIfRayHit(const int& horizontal_slice, const Ray& ray) {
	const auto& camera_ptr = context_->GetCamera();
	const auto distance = ray.perpendicular_distance *
						  std::cos(camera_ptr.GetPosition().theta - ray.theta);
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
	Enqueue(ray.wall_id, src_rect, dest_rect, distance);
}

void Renderer3D::RenderIfRayHitNot(const int& horizontal_slice) {
	const auto config_ = context_->GetConfig();
	const auto [line_height, draw_start, draw_end] =
		CalculateVerticalSlice(config_.view_distance);

	SDL_Rect src_rect = {0, 0, 2,
						 TextureManager::GetInstance().GetTexture(7).height};
	SDL_Rect dest_rect = {horizontal_slice, draw_start, 2, line_height};
	Enqueue(7, src_rect, dest_rect, config_.view_distance);
}

void Renderer3D::RenderObjects() {
	const auto& objects = scene_->GetObjects();
	const auto& camera_ptr = context_->GetCamera();
	for (const auto& object : objects) {

		const RayPair* rays = camera_ptr.FindObjectRays(object->GetId());
		if (rays == nullptr) {
			continue;
		}

		const Ray& first = rays->first;
		const Ray& last = rays->second;

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

		Enqueue(first.wall_id, src_rect, dest_rect,
				first.perpendicular_distance);
	}
}

int Renderer3D::CalculateHorizontalSlice(const double& angle) {
	const auto& camera_ptr = context_->GetCamera();
	const auto config_ = context_->GetConfig();
	const auto horizontal_slice =
		static_cast<int>(angle / camera_ptr.GetDeltaAngle()) * 2 +
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

void Renderer3D::RenderWeapon() {
	const auto& player_ptr = scene_->GetPlayer();
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
	Enqueue(6, crosshair_src_rect, crosshair_dest_rect, 0.0);

	auto texture_id = player_ptr.GetTextureId();
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
	Enqueue(texture_id, src_rect, dest_rect, 0.0);

	// Check if player is damaged
	if (player_ptr.IsDamaged()) {
		auto& damage_texture = TextureManager::GetInstance().GetTexture(
			player_ptr.GetDamageTextureId());
		SDL_Rect damage_src_rect{0, 0, damage_texture.width,
								 damage_texture.height};
		SDL_Rect damage_dest_rect{0, 0, config_.width, config_.height};
		Enqueue(9, damage_src_rect, damage_dest_rect, -1.0);
	}
}

void Renderer3D::RenderTextures() {
	// Back to front; ties keep submission order. std::sort needs no buffer
	// (std::stable_sort would allocate one), the order field makes it stable
	std::ranges::sort(render_queue_, [](const RenderCommand& lhs,
										const RenderCommand& rhs) static {
		if (lhs.distance != rhs.distance) {
			return lhs.distance > rhs.distance;
		}
		return lhs.order < rhs.order;
	});
	auto* renderer = context_->GetRenderer();
	for (const RenderCommand& command : render_queue_) {
		const auto& texture =
			TextureManager::GetInstance().GetTexture(command.texture_id);
		SDL_RenderCopy(renderer, texture.texture, &command.src_rect,
					   &command.dest_rect);
	}
}

void Renderer3D::RenderHUD() {
	const auto& player = scene_->GetPlayer();
	const auto config = context_->GetConfig();
	auto& textures = TextureManager::GetInstance();
	const auto& digit_textures = textures.GetTextureCollection("digits");
	const int digit_width = config.width / 40;

	const auto draw_digit = [&](int digit, int x) {
		const auto& texture = textures.GetTexture(
			digit_textures[static_cast<std::size_t>(digit)]);
		const double ratio =
			static_cast<double>(texture.height) / texture.width;
		const int height = static_cast<int>(digit_width * ratio);
		const SDL_Rect src_rect{0, 0, texture.width, texture.height};
		const SDL_Rect dest_rect{x, config.height - height - 10, digit_width,
								 height};
		SDL_RenderCopy(context_->GetRenderer(), texture.texture, &src_rect,
					   &dest_rect);
	};

	std::array<int, 10> digits{};

	// Health, bottom left, followed by a percent sign
	const std::size_t health_digits =
		ToDigits(std::max(0, static_cast<int>(player.GetHealth())), digits);
	int x = config.width / 40;
	for (std::size_t i = 0; i < health_digits; ++i, x += digit_width) {
		draw_digit(digits[i], x);
	}
	draw_digit(kPercentDigit, x);

	// Ammo, bottom right, drawn right to left
	const std::size_t ammo_digits =
		ToDigits(static_cast<int>(player.GetWeapon().GetAmmo()), digits);
	x = config.width - config.width / 30;
	for (std::size_t i = ammo_digits; i-- > 0; x -= digit_width) {
		draw_digit(digits[i], x);
	}

	if (Settings::Get().show_fps) {
		RenderFps();
	}
}

// Averages the frame rate over kFpsRefreshSeconds and re-rasterises the text
// only when the shown value changes; every other frame just draws the cached
// texture
void Renderer3D::RenderFps() {
	fps_elapsed_ += TimeManager::GetInstance().GetDeltaTime();
	++fps_frames_;
	if (fps_elapsed_ >= kFpsRefreshSeconds || !fps_texture_) {
		const int fps =
			fps_elapsed_ > 0.0
				? static_cast<int>(std::lround(fps_frames_ / fps_elapsed_))
				: 0;
		fps_elapsed_ = 0.0;
		fps_frames_ = 0;
		if (fps != shown_fps_ || !fps_texture_) {
			shown_fps_ = fps;
			const SDL_Color white{255, 255, 255, 255};
			SDL_Surface* surface = TTF_RenderText_Solid(
				context_->GetFont(), std::to_string(fps).c_str(), white);
			if (surface == nullptr) {
				std::cerr << "Failed to render FPS text: " << TTF_GetError()
						  << std::endl;
				return;
			}
			fps_texture_.reset(
				SDL_CreateTextureFromSurface(context_->GetRenderer(), surface));
			fps_rect_ = {0, 0, surface->w, surface->h};
			SDL_FreeSurface(surface);
		}
	}
	if (fps_texture_) {
		SDL_RenderCopy(context_->GetRenderer(), fps_texture_.get(), nullptr,
					   &fps_rect_);
	}
}

}  // namespace wolfenstein
