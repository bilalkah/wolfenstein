#include "Graphics/renderer_interface.h"
#include "TextureManager/texture_manager.h"

namespace wolfenstein {

RendererContext::RendererContext(const std::string& window_name,
								 const RenderConfig& config, Camera2D& camera)
	: config_(config), camera_ptr(camera) {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (TTF_Init() != 0) {
		SDL_Log("Unable to initialize TTF: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	const auto font_path =
		std::string(RESOURCE_DIR) + "font/EternalAncient.ttf";
	font_ = TTF_OpenFont(font_path.c_str(), 30);  // Font size: 24
	if (!font_) {
		std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
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
	TTF_CloseFont(font_);
	SDL_DestroyWindow(window_);
	TTF_Quit();
	SDL_Quit();
}

SDL_Renderer* RendererContext::GetRenderer() const {
	return renderer_;
}

TTF_Font* RendererContext::GetFont() const {
	return font_;
}

SDL_Window* RendererContext::GetWindow() const {
	return window_;
}

RenderConfig RendererContext::GetConfig() const {
	return config_;
}

const Camera2D& RendererContext::GetCamera() const {
	return camera_ptr;
}

Camera2D& RendererContext::GetCamera() {
	return camera_ptr;
}

IRenderer::IRenderer(std::shared_ptr<RendererContext> context)
	: context_(context) {}

void IRenderer::ClearScreen() {
	SDL_SetRenderDrawColor(context_->GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(context_->GetRenderer());
}

void IRenderer::SetScene(const std::shared_ptr<Scene>& scene_ptr) {
	scene_ = scene_ptr;
	context_->GetCamera().SetScene(scene_ptr);
}

std::shared_ptr<RendererContext> IRenderer::GetContext() const {
	return context_;
}

}  // namespace wolfenstein
