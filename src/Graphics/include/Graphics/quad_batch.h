/**
 * @file quad_batch.h
 * @brief Flat-coloured shapes collected and drawn in one geometry call
 */

#ifndef GRAPHICS_INCLUDE_GRAPHICS_QUAD_BATCH_H_
#define GRAPHICS_INCLUDE_GRAPHICS_QUAD_BATCH_H_

#include <SDL3/SDL.h>
#include <cstddef>
#include <vector>

namespace wolfenstein {

// Collects quads and triangles and draws them in one SDL_RenderGeometry
// call: SDL's own rectangle, point and line functions build temporary
// arrays on the heap. Sized once; a full batch is drawn and the next one
// reuses the storage.
class QuadBatch
{
  public:
	// The most vertices one draw call carries; startup grows SDL's vertex
	// buffer to at least this
	static constexpr std::size_t kVertexCapacity = 16384;

	explicit QuadBatch(SDL_Renderer* renderer);

	void SetColor(SDL_Color color) {
		constexpr float kFull = 255.0F;
		color_ = {static_cast<float>(color.r) / kFull,
				  static_cast<float>(color.g) / kFull,
				  static_cast<float>(color.b) / kFull,
				  static_cast<float>(color.a) / kFull};
	}
	// Corners in order round the quad
	void AddQuad(SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_FPoint d);
	void AddTriangle(SDL_FPoint a, SDL_FPoint b, SDL_FPoint c);
	void AddRect(float x0, float y0, float x1, float y1);
	// Draws what was collected, blended with the renderer's draw blend mode
	void Flush();

  private:
	void MakeRoom(std::size_t vertices);

	SDL_Renderer* renderer_;
	std::vector<SDL_Vertex> vertices_;
	std::vector<int> indices_;
	SDL_FColor color_{1.0F, 1.0F, 1.0F, 1.0F};
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_QUAD_BATCH_H_
