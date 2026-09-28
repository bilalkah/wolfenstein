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
#include <numbers>
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
	static_assert(kDecals >= Scene::kWallMarks + 4,
				  "room for every bullet mark and a few secret walls");
	// A command per wall column (2 px wide), one a decal, and the weapon,
	// crosshair and overlays. The level's objects are added by
	// ReserveObjects.
	constexpr std::size_t kOverlays = 8;
	render_queue_.reserve(
		static_cast<std::size_t>(context_->GetConfig().width / 2) + kDecals +
		kOverlays);
	hud_digits_ = context_->Textures().GetTextureCollection("digits");
	sky_texture_ = context_->Textures().GetTextureId("sky");
	far_texture_ = context_->Textures().GetTextureId("solid_black");
	crosshair_texture_ = context_->Textures().GetTextureId("crosshair");
	damage_texture_ = context_->Textures().GetTextureId("damage_taken");
	mark_texture_ = context_->Textures().GetTextureId("secret_mark");
	bullet_mark_texture_ = context_->Textures().GetTextureId("bullet_mark");
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

	// Where a dead player's view is drawn to be rolled over; without render
	// targets they fall without rolling
	fallen_view_.reset(
		SDL_CreateTexture(context_->GetRenderer(), SDL_PIXELFORMAT_RGBA8888,
						  SDL_TEXTUREACCESS_TARGET, context_->GetConfig().width,
						  context_->GetConfig().height));
}

void Renderer3D::ReserveObjects(std::size_t objects) {
	render_queue_.reserve(render_queue_.capacity() + objects);
}

void Renderer3D::Enqueue(int texture_id, const SDL_Rect& src_rect,
						 const SDL_Rect& dest_rect, double distance,
						 bool mirrored) {
	render_queue_.push_back({texture_id, src_rect, dest_rect, distance,
							 static_cast<std::uint32_t>(render_queue_.size()),
							 nullptr, mirrored});
}

void Renderer3D::RenderScene(double delta_time) {
	ScopedTimer render_timer(ProfileSection::Render);
	render_queue_.clear();
	// Looking up, or a shot's kick, drops the world down the screen
	const Player& player = scene_->GetPlayer();
	pixels_per_unit_ =
		PixelsPerUnit(context_->GetConfig(), context_->GetCamera().GetFov());
	horizon_shift_ = static_cast<int>((player.GetPitch() + player.GetKick()) *
									  pixels_per_unit_);
	eye_height_ = player.GetEyeHeight();
	// Falling dead, the world is drawn aside to be rolled over
	const double fall = player.GetDeathFall();
	falling_ = fall > 0.0 && fallen_view_ != nullptr;
	if (falling_) {
		SDL_SetRenderTarget(context_->GetRenderer(), fallen_view_.get());
	}
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
		RenderHitMarker();
	}
	if (falling_) {
		SDL_SetRenderTarget(context_->GetRenderer(), nullptr);
		RenderFallen(fall);
		RenderDamage();
	}
	ScopedTimer timer(ProfileSection::RenderHud);
	RenderHUD(delta_time);
}

void Renderer3D::RenderBackground() {
	const auto& config = context_->GetConfig();
	auto* renderer = context_->GetRenderer();
	const auto& sky = context_->Textures().GetTexture(sky_texture_);
	const int horizon = config.height / 2 + horizon_shift_;

	// The sky reaches from the horizon to the top of the view looking as
	// far up as the player can: looking up shows more of it, never past it.
	// Its size stays the same whichever way the player looks (its
	// proportions kept); it turns with the view, a panorama repeating every
	// sky width.
	const int sky_height =
		config.height / 2 +
		static_cast<int>(Player::kMaxPitch * pixels_per_unit_);
	const int sky_width = sky.width * sky_height / sky.height;
	const double pixels_per_radian =
		config.width / context_->GetCamera().GetFov();
	const double turned =
		context_->GetCamera().GetPosition().theta * pixels_per_radian;
	const int offset =
		static_cast<int>(std::fmod(turned, sky_width) + sky_width) % sky_width;
	const int sky_top = horizon - sky_height;
	for (int x = -offset; x < config.width; x += sky_width) {
		const SDL_Rect band{x, sky_top, sky_width, sky_height};
		SDL_RenderCopy(renderer, sky.texture, nullptr, &band);
	}
	// Higher still (a shot's kick on top of looking fully up): the colour
	// of its top edge, into which the edge fades
	if (sky_top > 0) {
		const SDL_Color top = sky.top_colour;
		SDL_SetRenderDrawColor(renderer, top.r, top.g, top.b, 255);
		const SDL_Rect above{0, 0, config.width, sky_top};
		SDL_RenderFillRect(renderer, &above);

		const auto from = static_cast<float>(sky_top);
		const auto to =
			from + static_cast<float>(std::min(sky_top, sky_height / 3));
		const auto right = static_cast<float>(config.width);
		const SDL_Color opaque{top.r, top.g, top.b, 255};
		const SDL_Color clear{top.r, top.g, top.b, 0};
		const std::array<SDL_Vertex, 4> fade{{{{0.0F, from}, opaque, {}},
											  {{right, from}, opaque, {}},
											  {{0.0F, to}, clear, {}},
											  {{right, to}, clear, {}}}};
		constexpr std::array<int, 6> kTriangles{0, 1, 2, 1, 3, 2};
		SDL_BlendMode mode = SDL_BLENDMODE_NONE;
		SDL_GetRenderDrawBlendMode(renderer, &mode);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_RenderGeometry(renderer, nullptr, fade.data(),
						   static_cast<int>(fade.size()), kTriangles.data(),
						   static_cast<int>(kTriangles.size()));
		SDL_SetRenderDrawBlendMode(renderer, mode);
	}

	SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
	const SDL_Rect ground{0, horizon, config.width, config.height - horizon};
	SDL_RenderFillRect(renderer, &ground);
}

void Renderer3D::RenderWalls() {
	const auto& camera_ptr = context_->GetCamera();
	const auto& rays = camera_ptr.GetRays();

	decal_count_ = 0;
	face_marks_.valid = false;	// the marks may have changed since
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
	EnqueueDecals();
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

	RenderWallMarks(horizontal_slice, ray, hit_point, draw_start, line_height,
					distance);

	// A secret wall not yet pushed gives itself away to a careful eye: a
	// faint crack over the lower middle of its face
	const vector2d inside = ray.hit_point + ray.direction * 1e-4;
	const Map& map = scene_->GetMap();
	if (const PushWall* secret =
			map.FindPushWall(static_cast<int>(std::floor(inside.x)),
							 static_cast<int>(std::floor(inside.y)))) {
		constexpr double kLeft = 0.3;
		constexpr double kWidth = 0.4;
		constexpr double kTop = 0.55;
		constexpr double kHeight = 0.28;
		const double across = (hit_point - kLeft) / kWidth;
		if (across >= 0.0 && across < 1.0) {
			const auto index =
				static_cast<std::uint32_t>(secret - map.GetPushWalls().data());
			AddDecalColumn(mark_texture_, kCrackKeys + index * 4 + HitFace(ray),
						   horizontal_slice, across,
						   draw_start + static_cast<int>(kTop * line_height),
						   static_cast<int>(kHeight * line_height), distance);
		}
	}
}

void Renderer3D::RenderWallMarks(int horizontal_slice, const Ray& ray,
								 double across, int draw_start, int line_height,
								 double distance) {
	const auto marks = scene_->GetWallMarks();
	if (marks.empty()) {
		return;
	}
	// A mark is this share of a wall across, and as tall
	constexpr double kSize = 0.075;
	const auto [x, y] = HitCell(ray);
	const std::uint8_t face = HitFace(ray);
	FaceMarks& on_face = face_marks_;
	if (!on_face.valid || on_face.x != x || on_face.y != y ||
		on_face.face != face) {
		on_face = {.x = x,
				   .y = y,
				   .face = face,
				   .valid = true,
				   .marks = {},
				   .count = 0};
		for (std::size_t i = 0; i < marks.size(); ++i) {
			if (marks[i].x == x && marks[i].y == y && marks[i].face == face) {
				on_face.marks[on_face.count++] = static_cast<std::uint8_t>(i);
			}
		}
	}
	for (std::size_t k = 0; k < on_face.count; ++k) {
		const std::size_t i = on_face.marks[k];
		const Scene::WallMark& mark = marks[i];
		const double left = mark.across - kSize / 2;
		const double part = (across - left) / kSize;
		if (part < 0.0 || part >= 1.0) {
			continue;
		}
		AddDecalColumn(
			bullet_mark_texture_, static_cast<std::uint32_t>(i),
			horizontal_slice, part,
			draw_start +
				static_cast<int>((mark.down - kSize / 2) * line_height),
			std::max(1, static_cast<int>(kSize * line_height)), distance);
	}
}

void Renderer3D::AddDecalColumn(int texture_id, std::uint32_t key, int x,
								double u, int top, int height,
								double distance) {
	// A frame shows few: the search is short
	const auto end =
		decals_.begin() + static_cast<std::ptrdiff_t>(decal_count_);
	const auto decal =
		std::ranges::find(decals_.begin(), end, key, &Decal::key);
	if (decal == end) {
		if (decal_count_ == decals_.size()) {
			return;	 // room was set aside; past it they go undrawn
		}
		++decal_count_;
		*decal = {.texture_id = texture_id,
				  .key = key,
				  .columns = 0,
				  .first_x = x,
				  .last_x = x,
				  .first_u = u,
				  .last_u = u,
				  .before_last_u = u,
				  .first_top = top,
				  .first_height = height,
				  .last_top = top,
				  .last_height = height,
				  .distance = distance};
	}
	++decal->columns;
	decal->before_last_u = decal->last_u;
	decal->last_x = x;
	decal->last_u = u;
	decal->last_top = top;
	decal->last_height = height;
	decal->distance = std::min(decal->distance, distance);
}

void Renderer3D::EnqueueDecals() {
	for (std::size_t i = 0; i < decal_count_; ++i) {
		const Decal& decal = decals_[i];
		// The last column reaches as far across the texture again as the
		// one before it did; a single column shows a sliver
		const double step =
			decal.columns > 1 ? decal.last_u - decal.before_last_u : 0.0;
		const auto first_u = static_cast<float>(decal.first_u);
		const auto end_u =
			static_cast<float>(std::clamp(decal.last_u + step, 0.0, 1.0));
		const auto left = static_cast<float>(decal.first_x);
		const auto right = static_cast<float>(decal.last_x + 2);
		const auto corner = [](float x, int y, float u, float v) {
			return SDL_Vertex{.position = {x, static_cast<float>(y)},
							  .color = {255, 255, 255, 255},
							  .tex_coord = {u, v}};
		};
		decal_quads_[i] = {
			corner(left, decal.first_top, first_u, 0.0F),
			corner(left, decal.first_top + decal.first_height, first_u, 1.0F),
			corner(right, decal.last_top, end_u, 0.0F),
			corner(right, decal.last_top + decal.last_height, end_u, 1.0F)};
		render_queue_.push_back(
			{.texture_id = decal.texture_id,
			 .src_rect = {},
			 .dest_rect = {},
			 .distance = decal.distance,
			 .order = static_cast<std::uint32_t>(render_queue_.size()),
			 .quad = &decal_quads_[i]});
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

		const Camera2D::Sight* sight = camera_ptr.FindObject(object->GetId());
		if (sight == nullptr) {
			continue;
		}

		const Ray& first = sight->rays.first;
		const Ray& last = sight->rays.second;
		// A sprite the viewer stands in (lights do not block movement) would
		// cover the screen: it is not drawn
		constexpr double kNearestSprite = 0.25;
		if (first.perpendicular_distance < kNearestSprite) {
			continue;
		}

		auto [line_height, draw_start, draw_end] =
			CalculateVerticalSlice(first.perpendicular_distance);
		// Standing on the floor, or raised off it
		draw_end -= static_cast<int>(line_height * object->GetElevation());
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
				first.perpendicular_distance, sight->mirrored);
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
		std::min(pixels_per_unit_ / std::max(distance, kNearest),
				 config_.height * kTallest);
	auto line_height = static_cast<int>(height);
	// A wall spans the floor to a wall's height; the eye is eye_height_ up
	// it, level with the horizon
	const int horizon = config_.height / 2 + horizon_shift_;
	int draw_start = horizon - static_cast<int>((1.0 - eye_height_) * height);
	int draw_end = horizon + static_cast<int>(eye_height_ * height);
	return std::make_tuple(line_height, draw_start, draw_end);
}

void Renderer3D::RenderHitMarker() {
	const Player& player = scene_->GetPlayer();
	const double shown = player.GetHitMarker();
	if (shown <= 0.0 || !player.IsAlive()) {
		return;
	}
	auto* renderer = context_->GetRenderer();
	const auto& config = context_->GetConfig();
	const int cx = config.width / 2;
	const int cy = config.height / 2;
	// Ticks from a little off the crosshair, outward on its diagonals
	const int from = config.width / 90;
	const int to = config.width / 45;
	const auto alpha = static_cast<std::uint8_t>(255.0 * shown);
	if (player.IsHeadshotMarker()) {
		SDL_SetRenderDrawColor(renderer, 230, 40, 30, alpha);
	}
	else {
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
	}
	SDL_BlendMode mode = SDL_BLENDMODE_NONE;
	SDL_GetRenderDrawBlendMode(renderer, &mode);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	for (const int dx : {-1, 1}) {
		for (const int dy : {-1, 1}) {
			// Two pixels thick
			for (const int thick : {0, 1}) {
				SDL_RenderDrawLine(renderer, cx + dx * from + thick,
								   cy + dy * from, cx + dx * to + thick,
								   cy + dy * to);
			}
		}
	}
	SDL_SetRenderDrawBlendMode(renderer, mode);
}

void Renderer3D::RenderDamage() {
	const Player& player = scene_->GetPlayer();
	if (!player.IsDamaged()) {
		return;
	}
	const auto& config = context_->GetConfig();
	auto& damage = context_->Textures().GetTexture(damage_texture_);
	SDL_SetTextureAlphaMod(damage.texture, player.GetDamageAlpha());
	const SDL_Rect source{0, 0, damage.width, damage.height};
	const SDL_Rect screen{0, 0, config.width, config.height};
	if (falling_) {
		SDL_RenderCopy(context_->GetRenderer(), damage.texture, &source,
					   &screen);
	}
	else {
		Enqueue(damage_texture_, source, screen, -1.0);
	}
}

void Renderer3D::RenderFallen(double fall) {
	// Down onto the left side: the view rolls clockwise, the floor coming in
	// from the left
	constexpr double kRollDegrees = 80.0;
	const auto& config = context_->GetConfig();
	const double angle = kRollDegrees * fall;
	const double radians = angle * std::numbers::pi / 180.0;
	const double width = config.width;
	const double height = config.height;
	// Large enough that no corner of the screen is left uncovered
	const double scale = std::max(
		(width * std::cos(radians) + height * std::sin(radians)) / width,
		(width * std::sin(radians) + height * std::cos(radians)) / height);
	const int scaled_width = static_cast<int>(std::ceil(width * scale));
	const int scaled_height = static_cast<int>(std::ceil(height * scale));
	const SDL_Rect cover{(config.width - scaled_width) / 2,
						 (config.height - scaled_height) / 2, scaled_width,
						 scaled_height};
	ClearScreen();
	SDL_RenderCopyEx(context_->GetRenderer(), fallen_view_.get(), nullptr,
					 &cover, angle, nullptr, SDL_FLIP_NONE);
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
	if (player_ptr.IsAlive()) {
		Enqueue(crosshair_texture_, crosshair_src_rect, crosshair_dest_rect,
				0.0);
	}

	auto texture_id = player_ptr.GetTextureId();
	const auto texture_height =
		context_->Textures().GetTexture(texture_id).height;
	const auto texture_width =
		context_->Textures().GetTexture(texture_id).width;
	// A weapon's frames are as wide as the screen, the gun where it is held
	// on it, and stand on its bottom edge
	const double ratio = static_cast<double>(texture_height) / texture_width;
	const int width_slice = config_.width;
	const int height_slice = static_cast<int>(width_slice * ratio);
	SDL_Rect src_rect{0, 0, texture_width, texture_height};
	// A dead player's gun drops out of view as they fall
	const int dropped =
		static_cast<int>(1.2 * height_slice * player_ptr.GetDeathFall());
	SDL_Rect dest_rect{0, config_.height - height_slice + dropped, width_slice,
					   height_slice};
	Enqueue(texture_id, src_rect, dest_rect, 0.0);

	// Check if player is damaged
	// Falling, it goes over the rolled view instead, upright
	if (!falling_) {
		RenderDamage();
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
	// Two triangles a quad: top left, bottom left, top right; and bottom
	// left, bottom right, top right
	static constexpr std::array<int, 6> kQuadTriangles{0, 1, 2, 1, 3, 2};
	for (const RenderCommand& command : render_queue_) {
		const auto& texture =
			context_->Textures().GetTexture(command.texture_id);
		if (command.quad != nullptr) {
			SDL_RenderGeometry(renderer, texture.texture, command.quad->data(),
							   static_cast<int>(command.quad->size()),
							   kQuadTriangles.data(),
							   static_cast<int>(kQuadTriangles.size()));
		}
		else if (command.mirrored) {
			SDL_RenderCopyEx(renderer, texture.texture, &command.src_rect,
							 &command.dest_rect, 0.0, nullptr,
							 SDL_FLIP_HORIZONTAL);
		}
		else {
			SDL_RenderCopy(renderer, texture.texture, &command.src_rect,
						   &command.dest_rect);
		}
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
