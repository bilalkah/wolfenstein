/**
 * @file settings.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief User preferences, persisted between sessions
 * @version 0.1
 * @date 2026-09-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SETTINGS_INCLUDE_SETTINGS_SETTINGS_H
#define SETTINGS_INCLUDE_SETTINGS_SETTINGS_H

#include <array>
#include <cstddef>
#include <numbers>
#include <span>
#include <string_view>

namespace karakale {

// Text a setting holds, kept in place (a name, an address): at most N
// printable characters, what does not fit or cannot be printed left out
template <std::size_t N>
class SettingText
{
  public:
	static constexpr std::size_t kCapacity = N;

	SettingText() = default;
	explicit SettingText(std::string_view text) { Set(text); }
	std::string_view View() const { return {chars_.data(), size_}; }
	bool Empty() const { return size_ == 0; }
	void Set(std::string_view text) {
		size_ = 0;
		for (const char c : text) {
			Append(c);
		}
	}
	// False if it is full, or `c` is not printable
	bool Append(char c) {
		if (size_ == N || c < ' ' || c > '~') {
			return false;
		}
		chars_[size_++] = c;
		return true;
	}
	void EraseLast() {
		if (size_ > 0) {
			--size_;
		}
	}

	friend bool operator==(const SettingText& a, const SettingText& b) {
		return a.View() == b.View();
	}

  private:
	std::array<char, N> chars_{};
	std::size_t size_ = 0;
};

struct Settings
{
	static constexpr double kMinMouseSensitivity = 0.25;
	static constexpr double kMaxMouseSensitivity = 3.0;
	// Degrees across the screen
	static constexpr double kMinFov = 60.0;
	static constexpr double kMaxFov = 80.0;

	double mouse_sensitivity = 1.0;	 // multiplier on the base turn rate
	bool invert_mouse_y = false;	 // the mouse pushed away looks down
	double fov = kMinFov;			 // degrees across the screen
	double volume = 0.8;			 // master volume, 0 to 1
	// Shares of the master volume, 0 to 1
	double music_volume = 1.0;
	double effects_volume = 1.0;
	bool show_fps = true;
	// Multiplayer: the name the others know the player by, the server's
	// address ("ws://host:port"), and the room played in (empty: the
	// server's public one)
	SettingText<16> player_name;
	SettingText<96> server{kDefaultServer};
	SettingText<12> room;
	// Where a game joins, until the player says otherwise: on the web, the
	// game's own server; natively, one on this machine
#ifdef __EMSCRIPTEN__
	static constexpr std::string_view kDefaultServer = "wss://play.kergit.com";
#else
	static constexpr std::string_view kDefaultServer = "ws://localhost:8080";
#endif

	double FovRadians() const { return fov * std::numbers::pi / 180.0; }

	// Process-wide settings, loaded from storage on first use
	static Settings& Get();

	// "key=value" lines: the keys `text` has replace these settings, clamped
	// to their ranges; unknown keys and malformed values are ignored
	void Parse(std::string_view text);
	// Writes them into `out` without allocating; the length, or 0 if they
	// do not fit
	std::size_t Format(std::span<char> out) const;

	// Web builds keep settings in localStorage, native builds in a file in
	// SDL's per-user preferences directory
	void Load();
	void Save() const;
};

}  // namespace karakale

#endif	// SETTINGS_INCLUDE_SETTINGS_SETTINGS_H
