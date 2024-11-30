#include "Graphics/renderer_interface.h"
#include "TextureManager/texture_manager.h"

namespace wolfenstein {

RendererContext::RendererContext(const std::string& window_name,
								 const RenderConfig& config,
								 std::shared_ptr<Camera2D>& camera)
	: config_(config), camera_ptr(camera) {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	window_ = SDL_CreateWindow(window_name.c_str(), SDL_WINDOWPOS_CENTERED,
							   SDL_WINDOWPOS_CENTERED, config_.width,
							   config_.height, SDL_WINDOW_SHOWN);
	if (window_ == nullptr) {
		SDL_Log("Unable to create window: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
	if (renderer_ == nullptr) {
		SDL_Log("Unable to create renderer: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (config_.fullscreen) {
		SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}

	TextureManager::GetInstance().InitManager(renderer_);
}

RendererContext::~RendererContext() {
	SDL_DestroyRenderer(renderer_);
	SDL_DestroyWindow(window_);
	SDL_Quit();
}

SDL_Renderer* RendererContext::GetRenderer() const {
	return renderer_;
}

SDL_Window* RendererContext::GetWindow() const {
	return window_;
}

RenderConfig RendererContext::GetConfig() const {
	return config_;
}

std::shared_ptr<Camera2D> RendererContext::GetCamera() const {
	return camera_ptr;
}

IRenderer::IRenderer(std::shared_ptr<RendererContext> context)
	: context_(context) {}

void IRenderer::ClearScreen() {
	SDL_SetRenderDrawColor(context_->GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(context_->GetRenderer());
}

std::shared_ptr<RendererContext> IRenderer::GetContext() const {
	return context_;
}

}  // namespace wolfenstein
