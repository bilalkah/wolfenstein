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

#include <cstddef>
#include <numbers>
#include <span>
#include <string_view>

namespace wolfenstein {

struct Settings
{
	static constexpr double kMinMouseSensitivity = 0.25;
	static constexpr double kMaxMouseSensitivity = 3.0;
	// Degrees across the screen
	static constexpr double kMinFov = 60.0;
	static constexpr double kMaxFov = 100.0;

	double mouse_sensitivity = 1.0;	 // multiplier on the base turn rate
	bool invert_mouse_y = false;	 // the mouse pushed away looks down
	double fov = kMinFov;			 // degrees across the screen
	double volume = 0.8;			 // master volume, 0 to 1
	// Shares of the master volume, 0 to 1
	double music_volume = 1.0;
	double effects_volume = 1.0;
	bool show_fps = true;

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

}  // namespace wolfenstein

#endif	// SETTINGS_INCLUDE_SETTINGS_SETTINGS_H
