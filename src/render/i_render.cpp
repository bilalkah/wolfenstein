#include "render/i_render.h"
#include "render/utils.h"

namespace wolfenstein {

CommonRender::CommonRender(SDL_Renderer* renderer) : renderer_(renderer) {}

void CommonRender::ClearScreen() {
	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
	SDL_RenderClear(renderer_);
}

void CommonRender::SetDrawColor(SDL_Color color) {
	SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

void CommonRender::Render(std::vector<std::shared_ptr<ICharacter>> characters) {
}

SimpleRender::SimpleRender(SDL_Renderer* renderer) : CommonRender(renderer) {}

void SimpleRender::DrawPixel(vector2i point) {
	SDL_RenderDrawPoint(renderer_, point.x, point.y);
}

void SimpleRender::DrawLine(vector2i start, vector2i end) {
	SDL_RenderDrawLine(renderer_, start.x, start.y, end.x, end.y);
}

void SimpleRender::DrawCircle(vector2i center, int radius) {
	const auto circle_points = GenerateCirclePoints(center, radius, 20);
	for (unsigned int i = 0; i < circle_points.size(); i++) {
		DrawPixel(circle_points[i]);
	}
}

void SimpleRender::DrawRectangle(vector2i start, vector2i end) {
	SDL_Rect rect{start.x, start.y, end.x - start.x, end.y - start.y};
	SDL_RenderDrawRect(renderer_, &rect);
}

void SimpleRender::DrawFilledRectangle(vector2i start, vector2i end) {
	SDL_Rect rect{start.x, start.y, end.x - start.x, end.y - start.y};
	SDL_RenderFillRect(renderer_, &rect);
}

}  // namespace wolfenstein
