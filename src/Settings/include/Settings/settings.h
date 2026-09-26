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

namespace wolfenstein {

struct Settings
{
	static constexpr double kMinMouseSensitivity = 0.25;
	static constexpr double kMaxMouseSensitivity = 3.0;

	double mouse_sensitivity = 1.0;	 // multiplier on the base turn rate
	double volume = 0.8;			 // master volume, 0 to 1
	bool show_fps = true;

	// Process-wide settings, loaded from storage on first use
	static Settings& Get();

	// Web builds keep settings in localStorage, native builds in a file in
	// SDL's per-user preferences directory
	void Load();
	void Save() const;
};

}  // namespace wolfenstein

#endif	// SETTINGS_INCLUDE_SETTINGS_SETTINGS_H
