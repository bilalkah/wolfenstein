/**
 * @file ui.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Small immediate-mode UI toolkit on top of SDL_Renderer
 * @version 0.1
 * @date 2026-09-25
 *
 * @copyright Copyright (c) 2026
 *
 * Screens call the widget functions every frame; each call draws the widget
 * and reports interaction. Focusable widgets are numbered in call order, so
 * keyboard focus moves through them in the order they are drawn.
 */

#ifndef UI_INCLUDE_UI_UI_H
#define UI_INCLUDE_UI_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace wolfenstein::ui {

namespace color {
inline constexpr SDL_Color kText{236, 229, 216, 255};
inline constexpr SDL_Color kMuted{158, 148, 136, 255};
inline constexpr SDL_Color kAccent{200, 36, 28, 255};
inline constexpr SDL_Color kAccentBright{255, 92, 64, 255};
inline constexpr SDL_Color kPanel{16, 12, 10, 215};
inline constexpr SDL_Color kPanelFocused{48, 18, 14, 235};
inline constexpr SDL_Color kBorder{92, 80, 70, 255};
inline constexpr SDL_Color kTrack{60, 52, 46, 255};
}  // namespace color

enum class FontStyle : std::uint8_t {
	Title,	  // display font, huge
	Heading,  // display font
	Button,	  // display font
	Body,	  // text font
	Small,	  // text font
	Count
};

enum class Align : std::uint8_t { Left, Center, Right };

// Input collected from the frame's SDL events
class Input
{
  public:
	void BeginFrame();
	void Handle(const SDL_Event& event);

	int mouse_x = 0;
	int mouse_y = 0;
	bool mouse_moved = false;
	bool mouse_pressed = false;	  // went down this frame
	bool mouse_released = false;  // went up this frame
	bool mouse_down = false;
	bool previous = false;	// up arrow, or W
	bool next = false;		// down arrow, or S
	bool left = false;
	bool right = false;
	bool activate = false;	// Enter or Space
	bool back = false;		// Esc or Backspace
};

class Ui
{
  public:
	// Fonts: display font for titles and buttons, text font for everything else
	Ui(SDL_Renderer* renderer, const std::string& display_font_path,
	   const std::string& text_font_path);
	~Ui();
	// Owns fonts and cached text textures
	Ui(const Ui&) = delete;
	Ui& operator=(const Ui&) = delete;
	Ui(Ui&&) = delete;
	Ui& operator=(Ui&&) = delete;

	void BeginFrame(const Input& input);
	void EndFrame();
	// Puts focus back on the first widget, e.g. when a screen opens
	void ResetFocus(int index = 0);
	// Moves keyboard focus by delta widgets, wrapping around (up/down arrows
	// do this automatically; screens can map other keys to it)
	void MoveFocus(int delta);

	void FillRect(const SDL_Rect& rect, SDL_Color color);
	void DrawRect(const SDL_Rect& rect, SDL_Color color, int thickness = 1);
	// Draws text anchored at (x, y) by its top edge and the given alignment,
	// and returns its size
	SDL_Point Text(std::string_view text, int x, int y, FontStyle style,
				   SDL_Color color = color::kText, Align align = Align::Left);
	SDL_Point MeasureText(std::string_view text, FontStyle style);

	// Widgets return true when activated (click, Enter or Space)
	bool Button(std::string_view label, const SDL_Rect& rect);
	// Adjusted with left/right while focused, or by clicking/dragging the
	// track; returns true when the value changed
	bool Slider(std::string_view label, std::string_view value_text,
				const SDL_Rect& rect, double& value, double min, double max,
				double step);
	bool Toggle(std::string_view label, const SDL_Rect& rect, bool& value);
	// A focusable area whose contents the caller draws; `focused` reports
	// whether it has focus this frame
	bool Selectable(const SDL_Rect& rect, bool& focused);

	bool IsFocused(int index) const { return index == focus_index_; }

  private:
	struct CachedText
	{
		SDL_Texture* texture = nullptr;
		int width = 0;
		int height = 0;
	};

	// Registers the next focusable widget and returns its index, moving focus
	// to it when the mouse hovers it
	int NextWidget(const SDL_Rect& rect);
	bool Contains(const SDL_Rect& rect, int x, int y) const;
	const CachedText& GetText(std::string_view text, FontStyle style,
							  SDL_Color color);

	SDL_Renderer* renderer_;
	std::array<TTF_Font*, static_cast<std::size_t>(FontStyle::Count)> fonts_{};
	std::unordered_map<std::string, CachedText> text_cache_;
	Input input_;
	int focus_index_ = 0;
	int widget_count_ = 0;
	int previous_widget_count_ = 0;
	int pressed_widget_ = -1;  // widget the mouse went down on
	bool focus_reset_ = false;
};

}  // namespace wolfenstein::ui

#endif	// UI_INCLUDE_UI_UI_H
