#include "Graphics/renderer_3d.h"
#include "Camera/ray.h"
#include "Profiler/profiler.h"
#include "Settings/settings.h"
#include "TextureManager/texture_manager.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
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

Renderer3D::Renderer3D(RendererContext& context) : IRenderer(context) {
	// Up to two commands per wall column (2 px wide): the wall and, on a
	// secret wall, its mark; the weapon, crosshair and overlays. The level's
	// objects are added by ReserveObjects.
	constexpr std::size_t kOverlays = 8;
	render_queue_.reserve(
		static_cast<std::size_t>(context_->GetConfig().width) + kOverlays);
	hud_digits_ = context_->Textures().GetTextureCollection("digits");
	sky_texture_ = context_->Textures().GetTextureId("sky");
	far_texture_ = context_->Textures().GetTextureId("solid_black");
	crosshair_texture_ = context_->Textures().GetTextureId("crosshair");
	damage_texture_ = context_->Textures().GetTextureId("damage_taken");
	mark_texture_ = context_->Textures().GetTextureId("secret_mark");
	door_textures_ = {context_->Textures().GetTextureId("door"),
					  context_->Textures().GetTextureId("door_gold"),
					  context_->Textures().GetTextureId("door_silver")};
	key_textures_ = {0, context_->Textures().GetTextureId("gold_key"),
					 context_->Textures().GetTextureId("silver_key")};

	const SDL_Color white{255, 255, 255, 255};
	for (int digit = 0; digit < 10; ++digit) {
		const std::array<char, 2> text{static_cast<char>('0' + digit), '\0'};
		SDL_Surface* surface =
			TTF_RenderText_Solid(context_->GetFont(), text.data(), white);
		if (surface == nullptr) {
			std::cerr << "Failed to render FPS digit: " << TTF_GetError()
					  << '\n';
			continue;
		}
		auto& glyph = fps_digits_[static_cast<std::size_t>(digit)];
		glyph.texture.reset(
			SDL_CreateTextureFromSurface(context_->GetRenderer(), surface));
		glyph.width = surface->w;
		glyph.height = surface->h;
		SDL_FreeSurface(surface);
		// Drawn once now, so its first draw in play costs nothing extra
		const SDL_Rect pixel{0, 0, 1, 1};
		SDL_RenderCopy(context_->GetRenderer(), glyph.texture.get(), nullptr,
					   &pixel);
	}
	SDL_RenderFlush(context_->GetRenderer());
}

void Renderer3D::ReserveObjects(std::size_t objects) {
	render_queue_.reserve(render_queue_.capacity() + objects);
}

void Renderer3D::Enqueue(int texture_id, const SDL_Rect& src_rect,
						 const SDL_Rect& dest_rect, double distance) {
	render_queue_.push_back({texture_id, src_rect, dest_rect, distance,
							 static_cast<std::uint32_t>(render_queue_.size())});
}

void Renderer3D::RenderScene(double delta_time) {
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
	RenderHUD(delta_time);
}

void Renderer3D::RenderBackground() {
	const auto& config = context_->GetConfig();
	auto renderer_ = context_->GetRenderer();
	// Render sky
	const auto& sky_texture = context_->Textures().GetTexture(sky_texture_);
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
	// A door slid part open shows the rest of its texture
	hit_point = std::fmod(hit_point, 1.0) - ray.texture_shift;
	// A wall ray's id is the map cell it hit; the manifest gives its texture
	const auto cell = static_cast<std::uint16_t>(ray.wall_id);
	const int wall_texture =
		Map::IsDoorCell(cell)
			? door_textures_[static_cast<std::size_t>(
				  scene_->GetMap().GetDoors()[cell - Map::kDoorCell].lock)]
			: context_->Textures().GetWallTexture(ray.wall_id);
	const auto& texture = context_->Textures().GetTexture(wall_texture);
	const auto texture_height = texture.height;
	const auto texture_width = texture.width;
	int texture_point = static_cast<int>(hit_point * texture_width);

	// One texel wide: two would squeeze a pair of texels into the column,
	// which stripes a low-resolution texture seen up close
	SDL_Rect src_rect = {texture_point, 0, 1, texture_height};
	SDL_Rect dest_rect = {horizontal_slice, draw_start, 2, line_height};
	Enqueue(wall_texture, src_rect, dest_rect, distance);

	// A secret wall not yet pushed gives itself away to a careful eye: a
	// faint crack over the lower middle of its face
	const vector2d inside = ray.hit_point + ray.direction * 1e-4;
	if (scene_->GetMap().FindPushWall(static_cast<int>(std::floor(inside.x)),
									  static_cast<int>(std::floor(inside.y))) !=
		nullptr) {
		constexpr double kLeft = 0.3;
		constexpr double kWidth = 0.4;
		constexpr double kTop = 0.55;
		constexpr double kHeight = 0.28;
		const double across = (hit_point - kLeft) / kWidth;
		if (across >= 0.0 && across < 1.0) {
			const auto& mark = context_->Textures().GetTexture(mark_texture_);
			const SDL_Rect mark_src{static_cast<int>(across * mark.width), 0, 1,
									mark.height};
			const SDL_Rect mark_dest{
				horizontal_slice,
				draw_start + static_cast<int>(kTop * line_height), 2,
				static_cast<int>(kHeight * line_height)};
			Enqueue(mark_texture_, mark_src, mark_dest, distance);
		}
	}
}

void Renderer3D::RenderIfRayHitNot(const int& horizontal_slice) {
	const auto& config_ = context_->GetConfig();
	const auto [line_height, draw_start, draw_end] =
		CalculateVerticalSlice(config_.view_distance);

	SDL_Rect src_rect = {0, 0, 2,
						 context_->Textures().GetTexture(far_texture_).height};
	SDL_Rect dest_rect = {horizontal_slice, draw_start, 2, line_height};
	Enqueue(far_texture_, src_rect, dest_rect, config_.view_distance);
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
		// A sprite the viewer stands in (lights do not block movement) would
		// cover the screen: it is not drawn
		constexpr double kNearestSprite = 0.25;
		if (first.perpendicular_distance < kNearestSprite) {
			continue;
		}

		auto [line_height, draw_start, draw_end] =
			CalculateVerticalSlice(first.perpendicular_distance);
		const auto height = object->GetHeight();
		line_height = static_cast<int>(line_height * height);
		draw_start = draw_end - line_height;

		const auto texture_height =
			context_->Textures().GetTexture(first.wall_id).height;
		const auto texture_width =
			context_->Textures().GetTexture(first.wall_id).width;

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
	const auto& config_ = context_->GetConfig();
	const auto horizontal_slice =
		static_cast<int>(angle / camera_ptr.GetDeltaAngle()) * 2 +
		config_.width / 2;

	return horizontal_slice;
}

std::tuple<int, int, int> Renderer3D::CalculateVerticalSlice(
	const double& distance) {
	const auto& config_ = context_->GetConfig();
	// Near zero distance the height tends to infinity, and converting that to
	// an int is undefined: clamp the distance and the result
	constexpr double kNearest = 0.01;
	constexpr double kTallest = 32.0;  // screen heights
	const double height =
		std::min(config_.height / std::max(distance, kNearest),
				 config_.height * kTallest);
	auto line_height = static_cast<int>(height);
	int draw_start = -line_height / 2 + config_.height / 2;
	int draw_end = line_height / 2 + config_.height / 2;
	return std::make_tuple(line_height, draw_start, draw_end);
}

void Renderer3D::RenderWeapon() {
	const auto& player_ptr = scene_->GetPlayer();
	const auto& config_ = context_->GetConfig();

	// Render crosshair
	const auto& crosshair_texture =
		context_->Textures().GetTexture(crosshair_texture_);
	const auto crosshair_height = crosshair_texture.height;
	const auto crosshair_width = crosshair_texture.width;
	SDL_Rect crosshair_src_rect{0, 0, crosshair_width, crosshair_height};
	const double crosshair_ratio =
		static_cast<double>(crosshair_height) / crosshair_width;
	const int crosshair_width_slice = config_.width / 40;
	const int crosshair_height_slice =
		static_cast<int>(crosshair_width_slice * crosshair_ratio);
	SDL_Rect crosshair_dest_rect{
		config_.width / 2 - crosshair_width_slice / 2,
		config_.height / 2 - crosshair_height_slice / 2, crosshair_width_slice,
		crosshair_height_slice};
	Enqueue(crosshair_texture_, crosshair_src_rect, crosshair_dest_rect, 0.0);

	auto texture_id = player_ptr.GetTextureId();
	const auto texture_height =
		context_->Textures().GetTexture(texture_id).height;
	const auto texture_width =
		context_->Textures().GetTexture(texture_id).width;
	const double ratio = static_cast<double>(texture_height) / texture_width;
	const int width_slice = static_cast<int>(config_.width / 1.3);
	const int height_slice = static_cast<int>(width_slice * ratio);
	SDL_Rect src_rect{0, 0, texture_width, texture_height};
	SDL_Rect dest_rect{config_.width / 2 - width_slice / 2 + 100,
					   config_.height - height_slice, width_slice,
					   height_slice};
	Enqueue(texture_id, src_rect, dest_rect, 0.0);

	// Check if player is damaged
	if (player_ptr.IsDamaged()) {
		auto& damage_texture = context_->Textures().GetTexture(damage_texture_);
		SDL_Rect damage_src_rect{0, 0, damage_texture.width,
								 damage_texture.height};
		SDL_Rect damage_dest_rect{0, 0, config_.width, config_.height};
		SDL_SetTextureAlphaMod(damage_texture.texture,
							   player_ptr.GetDamageAlpha());
		Enqueue(damage_texture_, damage_src_rect, damage_dest_rect, -1.0);
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
			context_->Textures().GetTexture(command.texture_id);
		SDL_RenderCopy(renderer, texture.texture, &command.src_rect,
					   &command.dest_rect);
	}
}

void Renderer3D::RenderHUD(double delta_time) {
	const auto& player = scene_->GetPlayer();
	const auto& config = context_->GetConfig();
	auto& textures = context_->Textures();
	const int digit_width = config.width / 40;

	// Digits stand on the same baseline, `width` wide
	const auto draw_digit = [&](int digit, int x, int width) {
		const auto& texture =
			textures.GetTexture(hud_digits_[static_cast<std::size_t>(digit)]);
		const double ratio =
			static_cast<double>(texture.height) / texture.width;
		const int height = static_cast<int>(width * ratio);
		const SDL_Rect src_rect{0, 0, texture.width, texture.height};
		const SDL_Rect dest_rect{x, config.height - height - 10, width, height};
		SDL_RenderCopy(context_->GetRenderer(), texture.texture, &src_rect,
					   &dest_rect);
	};

	// A gold flash over the view after taking a pickup
	if (const std::uint8_t alpha = player.GetPickupAlpha(); alpha > 0) {
		SDL_Renderer* renderer = context_->GetRenderer();
		SDL_BlendMode previous = SDL_BLENDMODE_NONE;
		SDL_GetRenderDrawBlendMode(renderer, &previous);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 255, 208, 96, alpha);
		const SDL_Rect screen{0, 0, config.width, config.height};
		SDL_RenderFillRect(renderer, &screen);
		SDL_SetRenderDrawBlendMode(renderer, previous);
	}

	std::array<int, 10> digits{};

	// Health, bottom left, followed by a percent sign
	const std::size_t health_digits =
		ToDigits(std::max(0, static_cast<int>(player.GetHealth())), digits);
	int x = config.width / 40;
	for (std::size_t i = 0; i < health_digits; ++i, x += digit_width) {
		draw_digit(digits[i], x, digit_width);
	}
	draw_digit(kPercentDigit, x, digit_width);

	// The keys held, after the health
	x += digit_width * 2;
	for (const KeyColour key : {KeyColour::Gold, KeyColour::Silver}) {
		if (!player.HasKey(key)) {
			continue;
		}
		const auto& texture =
			textures.GetTexture(key_textures_[static_cast<std::size_t>(key)]);
		const int size = digit_width * 3 / 2;
		const SDL_Rect src{0, 0, texture.width, texture.height};
		const SDL_Rect dest{x, config.height - size - 10, size, size};
		SDL_RenderCopy(context_->GetRenderer(), texture.texture, &src, &dest);
		x += size + digit_width / 4;
	}

	// Ammo, bottom right, drawn right to left: the rounds in reserve in
	// smaller digits, then those in the magazine; a blade has none
	const Weapon& weapon = player.GetWeapon();
	if (weapon.IsMelee()) {
		if (Settings::Get().show_fps) {
			RenderFps(delta_time);
		}
		return;
	}
	const int reserve_width = digit_width * 3 / 5;
	const std::size_t reserve_digits =
		ToDigits(static_cast<int>(weapon.GetReserve()), digits);
	x = config.width - config.width / 30;
	for (std::size_t i = reserve_digits; i-- > 0; x -= reserve_width) {
		draw_digit(digits[i], x, reserve_width);
	}
	x -= digit_width;
	const std::size_t ammo_digits =
		ToDigits(static_cast<int>(weapon.GetAmmo()), digits);
	for (std::size_t i = ammo_digits; i-- > 0; x -= digit_width) {
		draw_digit(digits[i], x, digit_width);
	}

	if (Settings::Get().show_fps) {
		RenderFps(delta_time);
	}
}

// Averages the frame rate over kFpsRefreshSeconds so the number is readable,
// and draws it from the pre-rendered digits
void Renderer3D::RenderFps(double delta_time) {
	fps_elapsed_ += delta_time;
	++fps_frames_;
	if (fps_elapsed_ >= kFpsRefreshSeconds) {
		shown_fps_ = static_cast<int>(std::lround(fps_frames_ / fps_elapsed_));
		fps_elapsed_ = 0.0;
		fps_frames_ = 0;
	}
	std::array<int, 10> digits{};
	const std::size_t count = ToDigits(shown_fps_, digits);
	int x = 0;
	for (std::size_t i = 0; i < count; ++i) {
		const auto& glyph = fps_digits_[static_cast<std::size_t>(digits[i])];
		const SDL_Rect dest{x, 0, glyph.width, glyph.height};
		SDL_RenderCopy(context_->GetRenderer(), glyph.texture.get(), nullptr,
					   &dest);
		x += glyph.width;
	}
}

}  // namespace wolfenstein
