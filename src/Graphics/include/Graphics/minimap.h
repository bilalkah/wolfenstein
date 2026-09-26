/**
 * @file minimap.h
 * @brief The player's map of the level: only what they have seen of it
 */

#ifndef GRAPHICS_INCLUDE_GRAPHICS_MINIMAP_H_
#define GRAPHICS_INCLUDE_GRAPHICS_MINIMAP_H_

#include "Graphics/quad_batch.h"
#include "Graphics/renderer_interface.h"

namespace wolfenstein {

class Scene;

// Draws the explored cells of the level (floor and walls, coloured by wall
// texture), the pickups seen there and not yet taken, and the player's
// position and facing; enemies and lights are left out. A corner panel, or
// large in the middle of the screen. Drawn on top of the 3D view, in one
// geometry call.
class Minimap
{
  public:
	// Borrows the context, which outlives it
	explicit Minimap(RendererContext& context);

	// Borrows the scene until the next call; the game owns it
	void SetScene(Scene& scene) { scene_ = &scene; }
	// `expanded`: large, in the middle of the screen, rather than in the
	// top right corner
	void Render(const Position2D& player, bool expanded);

  private:
	RendererContext& context_;
	Scene* scene_ = nullptr;
	QuadBatch batch_;
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_MINIMAP_H_
