#include "UI/ui.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <iostream>

namespace wolfenstein::ui {

namespace {

// Text textures are created once and reused; the cache is cleared if a screen
// ever produces this many distinct strings
constexpr std::size_t kMaxCachedTexts = 512;

struct FontSpec
{
	bool display;
	int size;
};

constexpr std::array<FontSpec, static_cast<std::size_t>(FontStyle::Count)>
	kFontSpecs = {{
		{true, 92},	  // Title
		{true, 46},	  // Heading
		{true, 30},	  // Button
		{false, 26},  // Body
		{false, 20},  // Small
	}};

}  // namespace

void Input::BeginFrame() {
	mouse_moved = false;
	mouse_pressed = false;
	mouse_released = false;
	previous = next = left = right = activate = back = false;
}

void Input::Handle(const SDL_Event& event) {
	switch (event.type) {
		case SDL_MOUSEMOTION:
			mouse_x = event.motion.x;
			mouse_y = event.motion.y;
			mouse_moved = true;
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (event.button.button == SDL_BUTTON_LEFT) {
				mouse_x = event.button.x;
				mouse_y = event.button.y;
				mouse_pressed = true;
				mouse_down = true;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			if (event.button.button == SDL_BUTTON_LEFT) {
				mouse_x = event.button.x;
				mouse_y = event.button.y;
				mouse_released = true;
				mouse_down = false;
			}
			break;
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym) {
				case SDLK_UP:
				case SDLK_w:
					previous = true;
					break;
				case SDLK_DOWN:
				case SDLK_s:
					next = true;
					break;
				case SDLK_LEFT:
				case SDLK_a:
					left = true;
					break;
				case SDLK_RIGHT:
				case SDLK_d:
					right = true;
					break;
				case SDLK_RETURN:
				case SDLK_KP_ENTER:
				case SDLK_SPACE:
					activate = true;
					break;
				case SDLK_ESCAPE:
				case SDLK_BACKSPACE:
					back = true;
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
}

Ui::Ui(SDL_Renderer* renderer, const std::string& display_font_path,
	   const std::string& text_font_path)
	: renderer_(renderer) {
	for (std::size_t i = 0; i < kFontSpecs.size(); ++i) {
		const auto& spec = kFontSpecs[i];
		const auto& path = spec.display ? display_font_path : text_font_path;
		fonts_[i] = TTF_OpenFont(path.c_str(), spec.size);
		if (fonts_[i] == nullptr) {
			std::cerr << "Failed to load font " << path << ": "
					  << TTF_GetError() << std::endl;
			std::exit(EXIT_FAILURE);
		}
	}
}

Ui::~Ui() {
	for (auto& [key, text] : text_cache_) {
		SDL_DestroyTexture(text.texture);
	}
	for (TTF_Font* font : fonts_) {
		TTF_CloseFont(font);
	}
}

void Ui::BeginFrame(const Input& input) {
	input_ = input;
	widget_count_ = 0;
	SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

	// Keyboard navigation uses the widget count of the previous frame, since
	// this frame's widgets have not been declared yet
	if (input_.previous) {
		MoveFocus(-1);
	}
	if (input_.next) {
		MoveFocus(1);
	}
}

void Ui::MoveFocus(int delta) {
	if (previous_widget_count_ > 0) {
		focus_index_ = ((focus_index_ + delta) % previous_widget_count_ +
						previous_widget_count_) %
					   previous_widget_count_;
	}
}

void Ui::EndFrame() {
	// After a focus reset this frame's widgets belonged to the screen being
	// left, so their count must not clamp or wrap the new screen's focus
	if (focus_reset_) {
		previous_widget_count_ = 0;
		focus_reset_ = false;
	}
	else {
		previous_widget_count_ = widget_count_;
		if (widget_count_ > 0) {
			focus_index_ = std::clamp(focus_index_, 0, widget_count_ - 1);
		}
	}
	if (input_.mouse_released) {
		pressed_widget_ = -1;
	}
}

void Ui::ResetFocus(int index) {
	focus_index_ = index;
	pressed_widget_ = -1;
	focus_reset_ = true;
}

void Ui::FillRect(const SDL_Rect& rect, SDL_Color c) {
	SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
	SDL_RenderFillRect(renderer_, &rect);
}

void Ui::DrawRect(const SDL_Rect& rect, SDL_Color c, int thickness) {
	SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
	for (int i = 0; i < thickness; ++i) {
		const SDL_Rect r{rect.x + i, rect.y + i, rect.w - 2 * i,
						 rect.h - 2 * i};
		SDL_RenderDrawRect(renderer_, &r);
	}
}

std::size_t Ui::TextKeyHash::operator()(const TextKeyView& key) const noexcept {
	const std::size_t text = std::hash<std::string_view>{}(key.text);
	// 64 bits even where size_t is 32 (wasm32)
	const std::uint64_t style_and_colour =
		(static_cast<std::uint64_t>(key.style) << 32) | key.rgba;
	// Combines the two as boost::hash_combine does
	return text ^ (std::hash<std::uint64_t>{}(style_and_colour) + 0x9e3779b9 +
				   (text << 6) + (text >> 2));
}

const Ui::CachedText& Ui::GetText(std::string_view text, FontStyle style,
								  SDL_Color c) {
	const std::uint32_t rgba = (std::uint32_t{c.r} << 24) |
							   (std::uint32_t{c.g} << 16) |
							   (std::uint32_t{c.b} << 8) | c.a;
	const TextKeyView key{style, rgba, text};
	if (const auto it = text_cache_.find(key); it != text_cache_.end()) {
		return it->second;
	}
	if (text_cache_.size() >= kMaxCachedTexts) {
		for (auto& [cached_key, cached] : text_cache_) {
			SDL_DestroyTexture(cached.texture);
		}
		text_cache_.clear();
	}

	CachedText cached;
	std::string owned(text);
	SDL_Surface* surface = TTF_RenderUTF8_Blended(
		fonts_[static_cast<std::size_t>(style)], owned.c_str(), c);
	if (surface != nullptr) {
		cached.texture = SDL_CreateTextureFromSurface(renderer_, surface);
		cached.width = surface->w;
		cached.height = surface->h;
		SDL_FreeSurface(surface);
	}
	return text_cache_.emplace(TextKey{style, rgba, std::move(owned)}, cached)
		.first->second;
}

SDL_Point Ui::MeasureText(std::string_view text, FontStyle style) {
	const auto& cached = GetText(text, style, color::kText);
	return {cached.width, cached.height};
}

SDL_Point Ui::Text(std::string_view text, int x, int y, FontStyle style,
				   SDL_Color c, Align align) {
	if (text.empty()) {
		return {0, 0};
	}
	const auto& cached = GetText(text, style, c);
	if (cached.texture == nullptr) {
		return {0, 0};
	}
	int left = x;
	if (align == Align::Center) {
		left = x - cached.width / 2;
	}
	else if (align == Align::Right) {
		left = x - cached.width;
	}
	const SDL_Rect dest{left, y, cached.width, cached.height};
	SDL_RenderCopy(renderer_, cached.texture, nullptr, &dest);
	return {cached.width, cached.height};
}

bool Ui::Contains(const SDL_Rect& rect, int x, int y) const {
	return x >= rect.x && x < rect.x + rect.w && y >= rect.y &&
		   y < rect.y + rect.h;
}

int Ui::NextWidget(const SDL_Rect& rect) {
	const int index = widget_count_++;
	const bool hovered = Contains(rect, input_.mouse_x, input_.mouse_y);
	if (hovered && (input_.mouse_moved || input_.mouse_pressed)) {
		focus_index_ = index;
	}
	if (hovered && input_.mouse_pressed) {
		pressed_widget_ = index;
	}
	return index;
}

bool Ui::Button(std::string_view label, const SDL_Rect& rect) {
	const int index = NextWidget(rect);
	const bool focused = IsFocused(index);
	const bool clicked = input_.mouse_released && pressed_widget_ == index &&
						 Contains(rect, input_.mouse_x, input_.mouse_y);

	FillRect(rect, focused ? color::kPanelFocused : color::kPanel);
	DrawRect(rect, focused ? color::kAccent : color::kBorder, focused ? 2 : 1);
	if (focused) {
		// Accent bar on the left edge of the focused button
		FillRect({rect.x, rect.y, 6, rect.h}, color::kAccent);
	}
	const auto size = MeasureText(label, FontStyle::Button);
	Text(label, rect.x + rect.w / 2, rect.y + (rect.h - size.y) / 2,
		 FontStyle::Button, focused ? color::kText : color::kMuted,
		 Align::Center);

	return clicked || (focused && input_.activate);
}

bool Ui::Slider(std::string_view label, std::string_view value_text,
				const SDL_Rect& rect, double& value, double min, double max,
				double step) {
	const int index = NextWidget(rect);
	const bool focused = IsFocused(index);
	const double before = value;

	constexpr int kPadding = 24;
	const SDL_Rect track{rect.x + rect.w / 2, rect.y + rect.h / 2 - 3,
						 rect.w / 2 - kPadding - 90, 6};

	if (focused && input_.left) {
		value -= step;
	}
	if (focused && input_.right) {
		value += step;
	}
	// Clicking or dragging on the track sets the value directly
	if (pressed_widget_ == index && input_.mouse_down && track.w > 0) {
		const double t = std::clamp(
			static_cast<double>(input_.mouse_x - track.x) / track.w, 0.0, 1.0);
		value = min + t * (max - min);
		value = std::round(value / step) * step;
	}
	value = std::clamp(value, min, max);

	FillRect(rect, focused ? color::kPanelFocused : color::kPanel);
	DrawRect(rect, focused ? color::kAccent : color::kBorder, focused ? 2 : 1);
	const auto label_size = MeasureText(label, FontStyle::Body);
	Text(label, rect.x + kPadding, rect.y + (rect.h - label_size.y) / 2,
		 FontStyle::Body, focused ? color::kText : color::kMuted);

	const double t = (value - min) / (max - min);
	FillRect(track, color::kTrack);
	FillRect({track.x, track.y, static_cast<int>(track.w * t), track.h},
			 color::kAccent);
	const int knob_x = track.x + static_cast<int>(track.w * t);
	FillRect({knob_x - 7, track.y - 9, 14, 24},
			 focused ? color::kAccentBright : color::kText);

	Text(value_text, rect.x + rect.w - kPadding,
		 rect.y + (rect.h - label_size.y) / 2, FontStyle::Body, color::kText,
		 Align::Right);
	return value != before;
}

bool Ui::Toggle(std::string_view label, const SDL_Rect& rect, bool& value) {
	const int index = NextWidget(rect);
	const bool focused = IsFocused(index);
	const bool clicked = input_.mouse_released && pressed_widget_ == index &&
						 Contains(rect, input_.mouse_x, input_.mouse_y);
	const bool changed =
		clicked ||
		(focused && (input_.activate || input_.left || input_.right));
	if (changed) {
		value = !value;
	}

	constexpr int kPadding = 24;
	FillRect(rect, focused ? color::kPanelFocused : color::kPanel);
	DrawRect(rect, focused ? color::kAccent : color::kBorder, focused ? 2 : 1);
	const auto label_size = MeasureText(label, FontStyle::Body);
	Text(label, rect.x + kPadding, rect.y + (rect.h - label_size.y) / 2,
		 FontStyle::Body, focused ? color::kText : color::kMuted);

	// Switch: a pill with the knob on the right when on
	const SDL_Rect pill{rect.x + rect.w - kPadding - 64,
						rect.y + rect.h / 2 - 14, 64, 28};
	FillRect(pill, value ? color::kAccent : color::kTrack);
	FillRect({value ? pill.x + pill.w - 26 : pill.x + 4, pill.y + 4, 22, 20},
			 color::kText);
	Text(value ? "On" : "Off", pill.x - 14,
		 rect.y + (rect.h - label_size.y) / 2, FontStyle::Body, color::kText,
		 Align::Right);
	return changed;
}

bool Ui::Selectable(const SDL_Rect& rect, bool& focused) {
	const int index = NextWidget(rect);
	focused = IsFocused(index);
	const bool clicked = input_.mouse_released && pressed_widget_ == index &&
						 Contains(rect, input_.mouse_x, input_.mouse_y);
	return clicked || (focused && input_.activate);
}

}  // namespace wolfenstein::ui
