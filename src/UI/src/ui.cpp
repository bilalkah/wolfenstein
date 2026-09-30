#include "UI/ui.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace karakale::ui {

namespace {

// The characters rasterised at startup, as Latin-1 codes
constexpr int kFirstPrintable = 32;
constexpr int kLastPrintable = 126;
constexpr std::uint8_t kMiddleDot = 0xB7;  // "·"
constexpr std::uint8_t kDegree = 0xB0;	   // "°"
constexpr std::uint8_t kUnknown = '?';

// The next character of UTF-8 text as a Latin-1 code; characters outside
// Latin-1 become kUnknown
std::uint8_t NextCode(std::string_view text, std::size_t& i) {
	const auto lead = static_cast<std::uint8_t>(text[i++]);
	if (lead < 0x80) {
		return lead;
	}
	// Two-byte sequences 0xC2/0xC3 xx cover U+0080 to U+00FF
	if ((lead == 0xC2 || lead == 0xC3) && i < text.size()) {
		const auto trail = static_cast<std::uint8_t>(text[i++]);
		return static_cast<std::uint8_t>(((lead & 0x1F) << 6) | (trail & 0x3F));
	}
	// Skip the rest of a longer sequence
	while (i < text.size() &&
		   (static_cast<std::uint8_t>(text[i]) & 0xC0) == 0x80) {
		++i;
	}
	return kUnknown;
}

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

// SDL draws at fractional positions; the toolkit lays out in whole pixels
SDL_FRect ToFRect(const SDL_Rect& rect) {
	SDL_FRect result{};
	SDL_RectToFRect(&rect, &result);
	return result;
}

}  // namespace

#ifdef __EMSCRIPTEN__
namespace {

// The browser's clipboard. SDL has none on the web, and keeps Ctrl with a
// key from the browser, so it never pastes. While a text field has focus
// (its text shown to the page), Ctrl or Cmd with C, X or V goes to the
// browser instead of SDL, and the browser's copy, cut and paste events
// carry the field's text out and what was pasted in, which the field takes
// on its next frame.
EM_JS_DEPS(clipboard, "$stringToUTF8,$UTF8ToString");
// The bodies are JavaScript, so clang-format must not touch them
// clang-format off
EM_JS(void, InstallClipboard, (), {
	if (Module.karakaleClipboard) {
		return;
	}
	const clip = Module.karakaleClipboard = {text: null, pasted: String(), cut: false};
	window.addEventListener('keydown', (event) => {
		if (clip.text !== null && (event.ctrlKey || event.metaKey) &&
			['c', 'v', 'x'].includes(event.key.toLowerCase())) {
			event.stopPropagation();
		}
	}, true);
	document.addEventListener('paste', (event) => {
		if (clip.text !== null) {
			clip.pasted += event.clipboardData.getData('text');
			event.preventDefault();
		}
	});
	const copy = (event, cut) => {
		if (clip.text !== null) {
			event.clipboardData.setData('text/plain', clip.text);
			event.preventDefault();
			clip.cut = clip.cut || cut;
		}
	};
	document.addEventListener('copy', (event) => copy(event, false));
	document.addEventListener('cut', (event) => copy(event, true));
});

EM_JS(void, ShowFieldText, (const char* text), {
	Module.karakaleClipboard.text = text ? UTF8ToString(text) : null;
});

EM_JS(int, TakePasted, (char* out, int size), {
	const clip = Module.karakaleClipboard;
	const written = stringToUTF8(clip.pasted, out, size);
	clip.pasted = String();
	return written;
});

EM_JS(int, TakeCut, (), {
	const clip = Module.karakaleClipboard;
	const cut = clip.cut;
	clip.cut = false;
	return cut ? 1 : 0;
});

// Writing needs a page served over https (or from localhost); elsewhere
// (a LAN address) the older way, through a selected text area
EM_JS(void, WriteClipboard, (const char* text), {
	const value = UTF8ToString(text);
	const fallback = () => {
		const area = document.createElement('textarea');
		area.value = value;
		document.body.appendChild(area);
		area.select();
		try { document.execCommand('copy'); } catch (e) {}
		area.remove();
	};
	if (navigator.clipboard && window.isSecureContext) {
		navigator.clipboard.writeText(value).catch(fallback);
	}
	else {
		fallback();
	}
});
// clang-format on

}  // namespace
#endif

// NUL-terminated on the stack: nothing allocated
void CopyText(std::string_view text) {
	std::array<char, 512> terminated{};
	const std::size_t size = std::min(text.size(), terminated.size() - 1);
	std::ranges::copy(text.substr(0, size), terminated.begin());
#ifdef __EMSCRIPTEN__
	WriteClipboard(terminated.data());
#else
	SDL_SetClipboardText(terminated.data());
#endif
}

void Input::BeginFrame() {
	mouse_moved = false;
	mouse_pressed = false;
	mouse_released = false;
	previous = next = left = right = activate = back = false;
	typed_size = 0;
	erased = 0;
	copy = cut = erase_all = false;
}

void Input::Type(std::string_view text) {
	for (const char c : text) {
		if (c >= ' ' && c <= '~' && typed_size < typed.size()) {
			typed[typed_size++] = c;
		}
	}
}

void Input::Handle(const SDL_Event& event) {
	switch (event.type) {
		case SDL_EVENT_MOUSE_MOTION:
			mouse_x = static_cast<int>(event.motion.x);
			mouse_y = static_cast<int>(event.motion.y);
			mouse_moved = true;
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if (event.button.button == SDL_BUTTON_LEFT) {
				mouse_x = static_cast<int>(event.button.x);
				mouse_y = static_cast<int>(event.button.y);
				mouse_pressed = true;
				mouse_down = true;
			}
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (event.button.button == SDL_BUTTON_LEFT) {
				mouse_x = static_cast<int>(event.button.x);
				mouse_y = static_cast<int>(event.button.y);
				mouse_released = true;
				mouse_down = false;
			}
			break;
		case SDL_EVENT_TEXT_INPUT:
			if (text_mode && event.text.text != nullptr) {
				Type(event.text.text);
			}
			break;
		case SDL_EVENT_KEY_DOWN:
			HandleKey(event.key);
			break;
		default:
			break;
	}
}

void Input::HandleKey(const SDL_KeyboardEvent& event) {
	const SDL_Keycode key = event.key;
	if (text_mode && (event.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0) {
		switch (key) {
			case SDLK_V:
				// On the web the browser pastes instead: the key does not
				// come here (see InstallClipboard)
				if (char* pasted = SDL_GetClipboardText()) {
					Type(pasted);
					SDL_free(pasted);
				}
				break;
			case SDLK_C:
				copy = true;
				break;
			case SDLK_X:
				cut = true;
				break;
			case SDLK_BACKSPACE:
				erase_all = true;
				break;
			default:
				break;
		}
		return;
	}
	// Typing text, the letters and Space are the text's
	if (text_mode && (key == SDLK_W || key == SDLK_A || key == SDLK_S ||
					  key == SDLK_D || key == SDLK_SPACE)) {
		return;
	}
	switch (key) {
		case SDLK_UP:
		case SDLK_W:
			previous = true;
			break;
		case SDLK_DOWN:
		case SDLK_S:
		case SDLK_TAB:
			next = true;
			break;
		case SDLK_LEFT:
		case SDLK_A:
			left = true;
			break;
		case SDLK_RIGHT:
		case SDLK_D:
			right = true;
			break;
		case SDLK_RETURN:
		case SDLK_KP_ENTER:
		case SDLK_SPACE:
			activate = true;
			break;
		case SDLK_BACKSPACE:
			if (text_mode) {
				++erased;
			}
			else {
				back = true;
			}
			break;
		case SDLK_ESCAPE:
			back = true;
			break;
		default:
			break;
	}
}

Ui::Ui(SDL_Renderer* renderer, const std::string& display_font_path,
	   const std::string& text_font_path)
	: renderer_(renderer) {
#ifdef __EMSCRIPTEN__
	InstallClipboard();
#endif
	for (std::size_t i = 0; i < kFontSpecs.size(); ++i) {
		const auto& spec = kFontSpecs[i];
		const auto& path = spec.display ? display_font_path : text_font_path;
		fonts_[i] = TTF_OpenFont(path.c_str(), static_cast<float>(spec.size));
		if (fonts_[i] == nullptr) {
			std::cerr << "Failed to load font " << path << ": "
					  << SDL_GetError() << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	RasteriseGlyphs();
}

void Ui::RasteriseGlyphs() {
	const SDL_Color white{255, 255, 255, 255};
	for (std::size_t style = 0; style < fonts_.size(); ++style) {
		TTF_Font* font = fonts_[style];
		line_heights_[style] = TTF_GetFontHeight(font);
		const auto rasterise = [&](std::uint8_t code) {
			// The character as UTF-8
			std::array<char, 3> text{};
			if (code < 0x80) {
				text[0] = static_cast<char>(code);
			}
			else {
				text[0] = static_cast<char>(0xC0 | (code >> 6));
				text[1] = static_cast<char>(0x80 | (code & 0x3F));
			}
			SDL_Surface* surface =
				TTF_RenderText_Blended(font, text.data(), 0, white);
			if (surface == nullptr) {
				return;
			}
			Glyph& glyph = glyphs_[style][code];
			glyph.texture = SDL_CreateTextureFromSurface(renderer_, surface);
			glyph.width = surface->w;
			glyph.height = surface->h;
			int advance = surface->w;
			if (!TTF_GetGlyphMetrics(font, code, nullptr, nullptr, nullptr,
									 nullptr, &advance)) {
				advance = surface->w;
			}
			glyph.advance = advance;
			SDL_DestroySurface(surface);
		};
		for (int code = kFirstPrintable; code <= kLastPrintable; ++code) {
			rasterise(static_cast<std::uint8_t>(code));
		}
		rasterise(kMiddleDot);
		rasterise(kDegree);
	}
	// Draw each glyph once, tinted, so any work a driver defers to a
	// texture's first draw is done now rather than on a menu's first frame
	const SDL_FRect pixel{0, 0, 1, 1};
	for (const GlyphSet& set : glyphs_) {
		for (const Glyph& glyph : set) {
			if (glyph.texture != nullptr) {
				SDL_SetTextureColorMod(glyph.texture, 255, 255, 255);
				SDL_SetTextureAlphaMod(glyph.texture, 255);
				SDL_RenderTexture(renderer_, glyph.texture, nullptr, &pixel);
			}
		}
	}
	SDL_FlushRenderer(renderer_);
}

const Ui::Glyph& Ui::GlyphFor(FontStyle style, std::uint8_t code) const {
	const GlyphSet& set = glyphs_[static_cast<std::size_t>(style)];
	return set[code].texture != nullptr ? set[code] : set[kUnknown];
}

Ui::~Ui() {
	for (auto& set : glyphs_) {
		for (Glyph& glyph : set) {
			if (glyph.texture != nullptr) {
				SDL_DestroyTexture(glyph.texture);
			}
		}
	}
	for (TTF_Font* font : fonts_) {
		TTF_CloseFont(font);
	}
}

void Ui::BeginFrame(const Input& input) {
	input_ = input;
	widget_count_ = 0;
	field_focused_ = false;
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
	// No text field in focus any more: the browser keeps Ctrl with a key
#ifdef __EMSCRIPTEN__
	if (field_shown_ && !field_focused_) {
		ShowFieldText(nullptr);
		field_shown_ = false;
	}
#endif
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
	const SDL_FRect area = ToFRect(rect);
	SDL_RenderFillRect(renderer_, &area);
}

// Four filled bands rather than SDL_RenderRect, whose outline goes
// through SDL's line drawing, which allocates on some renderers (WebGL)
void Ui::DrawRect(const SDL_Rect& rect, SDL_Color c, int thickness) {
	SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
	const int t = std::min({thickness, rect.w / 2, rect.h / 2});
	const std::array<SDL_Rect, 4> bands = {{
		{rect.x, rect.y, rect.w, t},						   // top
		{rect.x, rect.y + rect.h - t, rect.w, t},			   // bottom
		{rect.x, rect.y + t, t, rect.h - 2 * t},			   // left
		{rect.x + rect.w - t, rect.y + t, t, rect.h - 2 * t},  // right
	}};
	for (const SDL_Rect& band : bands) {
		const SDL_FRect area = ToFRect(band);
		SDL_RenderFillRect(renderer_, &area);
	}
}

SDL_Point Ui::MeasureText(std::string_view text, FontStyle style) {
	int width = 0;
	for (std::size_t i = 0; i < text.size();) {
		width += GlyphFor(style, NextCode(text, i)).advance;
	}
	return {width, line_heights_[static_cast<std::size_t>(style)]};
}

// Draws glyph by glyph from the atlas, tinted to the colour
SDL_Point Ui::Text(std::string_view text, int x, int y, FontStyle style,
				   SDL_Color c, Align align) {
	if (text.empty()) {
		return {0, 0};
	}
	const SDL_Point size = MeasureText(text, style);
	int pen = x;
	if (align == Align::Center) {
		pen = x - size.x / 2;
	}
	else if (align == Align::Right) {
		pen = x - size.x;
	}
	for (std::size_t i = 0; i < text.size();) {
		const Glyph& glyph = GlyphFor(style, NextCode(text, i));
		if (glyph.texture != nullptr) {
			SDL_SetTextureColorMod(glyph.texture, c.r, c.g, c.b);
			SDL_SetTextureAlphaMod(glyph.texture, c.a);
			const SDL_FRect dest = ToFRect({pen, y, glyph.width, glyph.height});
			SDL_RenderTexture(renderer_, glyph.texture, nullptr, &dest);
		}
		pen += glyph.advance;
	}
	return size;
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

TextEdits Ui::TextField(std::string_view label, std::string_view value,
						const SDL_Rect& rect) {
	const int index = NextWidget(rect);
	const bool focused = IsFocused(index);
	constexpr int kPadding = 24;
	constexpr int kCaret = 3;
	FillRect(rect, focused ? color::kPanelFocused : color::kPanel);
	DrawRect(rect, focused ? color::kAccent : color::kBorder, focused ? 2 : 1);
	const auto label_size = MeasureText(label, FontStyle::Body);
	const int y = rect.y + (rect.h - label_size.y) / 2;
	Text(label, rect.x + kPadding, y, FontStyle::Body,
		 focused ? color::kText : color::kMuted);
	// The text in the rest of the box, never past it: too long, it starts
	// from the start, or, focused, is moved along to keep its end and the
	// place of the next letter in sight
	const int left = rect.x + rect.w / 4;
	const int right = rect.x + rect.w - kPadding;
	const int width = MeasureText(value, FontStyle::Body).x;
	const int shift =
		focused ? std::max(0, width + 2 + kCaret - (right - left)) : 0;
	const bool clipped = SDL_RenderClipEnabled(renderer_);
	SDL_Rect previous{};
	SDL_GetRenderClipRect(renderer_, &previous);
	const SDL_Rect inside{left, rect.y, right - left, rect.h};
	SDL_SetRenderClipRect(renderer_, &inside);
	Text(value, left - shift, y, FontStyle::Body, color::kText);
	if (focused) {
		FillRect({left - shift + width + 2, y + 2, kCaret, label_size.y - 4},
				 color::kAccent);
	}
	SDL_SetRenderClipRect(renderer_, clipped ? &previous : nullptr);
	if (!focused) {
		return {};
	}

	field_focused_ = true;
	int erased = input_.erased;
	bool cut = input_.cut;
#ifdef __EMSCRIPTEN__
	// The browser copies the text shown it; what it pasted is typed
	std::array<char, 512> text{};
	const std::size_t size = std::min(value.size(), text.size() - 1);
	std::ranges::copy(value.substr(0, size), text.begin());
	ShowFieldText(text.data());
	field_shown_ = true;
	std::array<char, 256> pasted{};
	const int length =
		TakePasted(pasted.data(), static_cast<int>(pasted.size()));
	input_.Type(
		std::string_view(pasted.data(), static_cast<std::size_t>(length)));
	cut = cut || TakeCut() != 0;
#else
	if (input_.copy || input_.cut) {
		CopyText(value);
	}
#endif
	if (cut || input_.erase_all) {
		erased = static_cast<int>(value.size());
	}
	return {.typed = input_.Typed(), .erased = erased};
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

}  // namespace karakale::ui
