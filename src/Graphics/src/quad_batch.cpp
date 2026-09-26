#include "Graphics/quad_batch.h"

namespace wolfenstein {

QuadBatch::QuadBatch(SDL_Renderer* renderer) : renderer_(renderer) {
	vertices_.reserve(kVertexCapacity);
	indices_.reserve(kVertexCapacity / 4 * 6);
}

void QuadBatch::MakeRoom(std::size_t vertices) {
	if (vertices_.size() + vertices > kVertexCapacity) {
		Flush();
	}
}

void QuadBatch::AddQuad(SDL_FPoint a, SDL_FPoint b, SDL_FPoint c,
						SDL_FPoint d) {
	MakeRoom(4);
	const int first = static_cast<int>(vertices_.size());
	for (const SDL_FPoint corner : {a, b, c, d}) {
		vertices_.push_back({corner, color_, {0.0f, 0.0f}});
	}
	for (const int corner : {0, 1, 2, 0, 2, 3}) {
		indices_.push_back(first + corner);
	}
}

void QuadBatch::AddTriangle(SDL_FPoint a, SDL_FPoint b, SDL_FPoint c) {
	MakeRoom(3);
	const int first = static_cast<int>(vertices_.size());
	for (const SDL_FPoint corner : {a, b, c}) {
		vertices_.push_back({corner, color_, {0.0f, 0.0f}});
	}
	for (const int corner : {0, 1, 2}) {
		indices_.push_back(first + corner);
	}
}

void QuadBatch::AddRect(float x0, float y0, float x1, float y1) {
	AddQuad({x0, y0}, {x1, y0}, {x1, y1}, {x0, y1});
}

void QuadBatch::Flush() {
	if (vertices_.empty()) {
		return;
	}
	SDL_RenderGeometry(renderer_, nullptr, vertices_.data(),
					   static_cast<int>(vertices_.size()), indices_.data(),
					   static_cast<int>(indices_.size()));
	vertices_.clear();
	indices_.clear();
}

}  // namespace wolfenstein
