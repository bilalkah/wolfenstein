/**
 * @file renderer_3d.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-12-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GRAPHICS_INCLUDE_GRAPHICS_RENDERER_3D_H_
#define GRAPHICS_INCLUDE_GRAPHICS_RENDERER_3D_H_

#include "Graphics/renderer_interface.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <tuple>
#include <vector>

namespace wolfenstein {

class Renderer3D : public IRenderer
{
  public:
	explicit Renderer3D(RendererContext& context);
	// Room for this many objects' draw commands on top of the walls', so a
	// frame never grows the queue (the largest level's count, once)
	void ReserveObjects(std::size_t objects);
	void RenderScene(double delta_time) override;

  private:
	// One textured rectangle to draw, or, if `quad` is set, a textured
	// quad (four corners: top left, bottom left, top right, bottom right).
	// The queue is drawn back to front; `order` keeps equal distances in
	// submission order, deterministically.
	struct RenderCommand
	{
		int texture_id = 0;
		SDL_Rect src_rect{};
		SDL_Rect dest_rect{};
		double distance = 0.0;
		std::uint32_t order = 0;
		const std::array<SDL_Vertex, 4>* quad = nullptr;
		bool mirrored = false;	// drawn flipped left to right
	};
	// A picture on a wall's face (a bullet mark, a secret wall's crack),
	// gathered over the wall columns it shows on and drawn as one quad:
	// drawn a strip a column, it took a draw call and two texture switches
	// a column, a dozen or more a mark, and a frame full of marks took a
	// quarter longer. A planar face's edges are straight on screen, so the
	// quad through its first and last columns' ends is where they were.
	struct Decal
	{
		int texture_id = 0;
		std::uint32_t key = 0;	// which picture, while the frame is drawn
		int columns = 0;
		int first_x = 0;
		int last_x = 0;
		// Across the picture's texture (0 to 1) at the first and last
		// columns, and the column before the last
		double first_u = 0.0;
		double last_u = 0.0;
		double before_last_u = 0.0;
		int first_top = 0;
		int first_height = 0;
		int last_top = 0;
		int last_height = 0;
		double distance = 0.0;	   // its nearest column's
		std::uint8_t shade = 255;  // 255 as the texture is, darker below
	};
	// Pictures on walls one frame shows, at most: every bullet mark
	// (Scene::kWallMarks), a few secret walls' faces and every page of intel
	// (Scene::kIntel)
	static constexpr std::size_t kDecals = 48;
	// Keys: a bullet mark's is its index; a secret wall's face's is past
	// every mark's, and a page of intel's past those
	static constexpr std::uint32_t kCrackKeys = 1U << 16;
	static constexpr std::uint32_t kIntelKeys = 1U << 17;

	struct TextureDeleter
	{
		void operator()(SDL_Texture* texture) const noexcept;
	};

	void Enqueue(int texture_id, const SDL_Rect& src_rect,
				 const SDL_Rect& dest_rect, double distance,
				 bool mirrored = false);
	void RenderBackground();
	void RenderWalls();
	void RenderIfRayHit(const int& horizontal_slice, const Ray& ray);
	void RenderIfRayHitNot(const int& horizontal_slice);
	void RenderObjects();
	int CalculateHorizontalSlice(const double& angle);
	std::tuple<int, int, int> CalculateVerticalSlice(const double& distance);
	// The bullet marks on the wall a column shows, `across` its face
	void RenderWallMarks(int horizontal_slice, const Ray& ray, double across,
						 int draw_start, int line_height, double distance);
	// The pages of intel on the wall a column shows, `across` its face
	void RenderIntel(int horizontal_slice, const Ray& ray, double across,
					 int draw_start, int line_height, double distance);
	// Adds a wall column to the decal `key` (made on its first column):
	// at `u` across its texture, from `top` down `height` pixels
	void AddDecalColumn(int texture_id, std::uint32_t key, int x, double u,
						int top, int height, double distance,
						std::uint8_t shade = 255);
	// Queues one quad a decal, once the walls are done
	void EnqueueDecals();
	void RenderWeapon();
	// The damage overlay, over the whole screen, fading after a hit
	void RenderDamage();
	// Four ticks round the crosshair as a shot hits, red for a headshot
	void RenderHitMarker();
	// A dead player's view, drawn into fallen_view_, rolled onto its side
	// as far as `fall` (0 to 1) and scaled to cover the screen
	void RenderFallen(double fall);
	void RenderTextures();
	void RenderHUD(double delta_time);
	void RenderFps(double delta_time);

	// Reused every frame: clear() keeps the capacity, so after the first
	// frame queueing never allocates
	std::vector<RenderCommand> render_queue_;
	// The bullet marks on the wall face the last column showed: the next
	// column most often shows the same face, and needs look at no others
	struct FaceMarks
	{
		int x = 0;
		int y = 0;
		std::uint8_t face = 0;
		bool valid = false;	 // not yet gathered this frame
		std::array<std::uint8_t, kDecals> marks{};
		std::size_t count = 0;
	};
	FaceMarks face_marks_;
	// This frame's decals, and their quads (which the queue points at)
	std::array<Decal, kDecals> decals_{};
	std::size_t decal_count_ = 0;
	std::array<std::array<SDL_Vertex, 4>, kDecals> decal_quads_{};

	// The FPS counter is drawn from the digits 0-9, rasterised once here:
	// a changing value costs no text rendering, texture or allocation
	struct Glyph
	{
		std::unique_ptr<SDL_Texture, TextureDeleter> texture;
		int width = 0;
		int height = 0;
	};
	std::array<Glyph, 10> fps_digits_;
	// Textures looked up by name once, when the renderer is built
	std::span<const std::uint16_t> hud_digits_;
	int sky_texture_ = 0;
	int far_texture_ = 0;  // where no wall is in view
	int crosshair_texture_ = 0;
	int damage_texture_ = 0;
	// By lock: none, gold, silver
	std::array<int, 3> door_textures_{};
	std::array<int, 3> key_textures_{};	 // held keys on the HUD
	int mark_texture_ = 0;				 // on secret walls
	int bullet_mark_texture_ = 0;		 // where shots struck walls
	int intel_texture_ = 0;				 // a page of intel on a wall
	// How far the world is slid down the screen this frame, in pixels: the
	// player looking up, and a shot's kick
	int horizon_shift_ = 0;
	// Screen pixels a unit is tall a unit away, in this frame's view
	double pixels_per_unit_ = 0.0;
	// Where the eye is, in walls above the floor (half way, standing)
	double eye_height_ = 0.5;
	// The world as a dead player sees it, drawn here to be rolled onto its
	// side; made once, with the renderer (null where targets are missing)
	std::unique_ptr<SDL_Texture, TextureDeleter> fallen_view_;
	bool falling_ = false;	// drawing into fallen_view_ this frame
	int shown_fps_ = 0;
	double fps_elapsed_ = 0.0;
	int fps_frames_ = 0;
};	// class Renderer3D

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_3D_H_
