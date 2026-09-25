#include "Graphics/renderer_interface.h"
#include "Graphics/renderer_2d.h"
#include "TextureManager/texture_manager.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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
		std::cerr << "Failed to load font: " << TTF_GetError() << '\n';
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

	const std::string manifest_path =
		std::string(RESOURCE_DIR) + "textures.json";
	std::ifstream manifest_file(manifest_path);
	auto manifest = ParseTextureManifest(manifest_file);
	if (!manifest) {
		std::cerr << manifest_path << ": " << manifest.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	auto textures =
		TextureManager::Load(renderer_, *manifest, std::string(RESOURCE_DIR));
	if (!textures) {
		std::cerr << "Cannot load the textures: " << textures.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	textures_ = std::move(*textures);

	// SDL queues draw calls, and grows its command pool and vertex buffer
	// whenever a frame queues more of them than any frame before: the first
	// frame with more on screen than ever allocated inside SDL. Queuing more
	// copies here than any screen draws (the 2D view, or the menu over a 3D
	// frame, stay below 2000) grows both once, at startup; they keep their
	// capacity. The copies land in the back buffer, which the first frame
	// clears.
	// The copies cycle through every texture, so any work a driver defers to
	// a texture's first draw is done here too
	constexpr int kWarmUpCommands = 4096;
	const int texture_count = textures_->TextureCount();
	const SDL_Rect pixel{0, 0, 1, 1};
	for (int i = 0; i < kWarmUpCommands; ++i) {
		SDL_RenderCopy(renderer_,
					   textures_->GetTexture(i % texture_count).texture, &pixel,
					   &pixel);
	}
	SDL_Texture* warm_up = textures_->GetTexture(0).texture;
	// One of every kind of draw the game makes (tinted and faded copies,
	// blended fills, lines, points), presented once: a GL driver compiles
	// its shaders on first use, and that belongs to startup, not the first
	// frame of play
	SDL_SetTextureColorMod(warm_up, 200, 200, 200);
	SDL_SetTextureAlphaMod(warm_up, 128);
	SDL_RenderCopy(renderer_, warm_up, &pixel, &pixel);
	SDL_SetTextureColorMod(warm_up, 255, 255, 255);
	SDL_SetTextureAlphaMod(warm_up, 255);
	for (const SDL_BlendMode mode : {SDL_BLENDMODE_NONE, SDL_BLENDMODE_BLEND}) {
		SDL_SetRenderDrawBlendMode(renderer_, mode);
		SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 128);
		SDL_RenderFillRect(renderer_, &pixel);
		SDL_RenderDrawLine(renderer_, 0, 0, 1, 1);
		SDL_RenderDrawPoint(renderer_, 0, 0);
	}
	// One batch as large as the 2D view ever draws, growing SDL's vertex
	// buffer for it
	{
		std::vector<SDL_Vertex> vertices(Renderer2D::kVertexCapacity);
		SDL_RenderGeometry(renderer_, nullptr, vertices.data(),
						   static_cast<int>(vertices.size()), nullptr, 0);
	}
	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
	SDL_RenderClear(renderer_);
	SDL_RenderPresent(renderer_);
}

RendererContext::~RendererContext() {
	// Textures belong to the renderer: they go first
	textures_.reset();
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

const Camera2D& RendererContext::GetCamera() const {
	return camera_ptr;
}

Camera2D& RendererContext::GetCamera() {
	return camera_ptr;
}

IRenderer::IRenderer(RendererContext& context) : context_(&context) {}

void IRenderer::ClearScreen() {
	SDL_SetRenderDrawColor(context_->GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(context_->GetRenderer());
}

void IRenderer::SetScene(Scene& scene) {
	scene_ = &scene;
	context_->GetCamera().SetScene(scene);
}

}  // namespace wolfenstein
