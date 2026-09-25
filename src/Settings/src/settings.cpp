#include "Settings/settings.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace wolfenstein {

namespace {

#ifdef __EMSCRIPTEN__
EM_JS_DEPS(settings_storage, "$stringToNewUTF8,$UTF8ToString");

// localStorage can be unavailable (e.g. blocked site data); settings then
// simply fall back to their defaults. The bodies are JavaScript, so
// clang-format must not touch them.
// clang-format off
EM_JS(char*, ReadStoredSettings, (), {
	let value = null;
	try {
		value = localStorage.getItem('wolfenstein.settings');
	} catch (e) {
	}
	return value === null ? 0 : stringToNewUTF8(value);
});

EM_JS(void, WriteStoredSettings, (const char* text), {
	try {
		localStorage.setItem('wolfenstein.settings', UTF8ToString(text));
	} catch (e) {
	}
});
// clang-format on

std::string ReadSettingsText() {
	char* text = ReadStoredSettings();
	if (text == nullptr) {
		return {};
	}
	std::string result(text);
	std::free(text);
	return result;
}

// `text` is NUL-terminated
void WriteSettingsText(std::string_view text) {
	WriteStoredSettings(text.data());
}
#else
// Worked out once, on first use (when the settings load at startup), so
// saving later needs no string building
const std::string& SettingsFilePath() {
	static const std::string path = [] {
		char* directory = SDL_GetPrefPath("bilalkah", "wolfenstein");
		if (directory == nullptr) {
			return std::string();
		}
		std::string result = std::string(directory) + "settings.txt";
		SDL_free(directory);
		return result;
	}();
	return path;
}

std::string ReadSettingsText() {
	std::ifstream file(SettingsFilePath());
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

// POSIX write rather than a file stream: saving happens while the game runs
// (leaving the settings screen), where nothing may allocate
void WriteSettingsText(std::string_view text) {
	const std::string& path = SettingsFilePath();
	if (path.empty()) {
		return;
	}
	const int file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (file < 0) {
		return;
	}
	(void)::write(file, text.data(), text.size());
	::close(file);
}
#endif

bool ParseDouble(std::string_view text, double& value) {
	double parsed = 0.0;
	const auto [end, error] =
		std::from_chars(text.data(), text.data() + text.size(), parsed);
	if (error != std::errc{} || end != text.data() + text.size()) {
		return false;
	}
	value = parsed;
	return true;
}

}  // namespace

Settings& Settings::Get() {
	static Settings settings = [] {
		Settings loaded;
		loaded.Load();
		return loaded;
	}();
	return settings;
}

// Stored as "key=value" lines; unknown keys and malformed values are ignored
void Settings::Load() {
	std::istringstream lines(ReadSettingsText());
	std::string line;
	while (std::getline(lines, line)) {
		const auto separator = line.find('=');
		if (separator == std::string::npos) {
			continue;
		}
		const std::string_view key(line.data(), separator);
		const std::string_view value(line.data() + separator + 1,
									 line.size() - separator - 1);
		double number = 0.0;
		if (!ParseDouble(value, number)) {
			continue;
		}
		if (key == "mouse_sensitivity") {
			mouse_sensitivity =
				std::clamp(number, kMinMouseSensitivity, kMaxMouseSensitivity);
		}
		else if (key == "volume") {
			volume = std::clamp(number, 0.0, 1.0);
		}
		else if (key == "show_fps") {
			show_fps = number != 0.0;
		}
	}
}

// Formats into a buffer on the stack: no allocation while the game runs
void Settings::Save() const {
	std::array<char, 128> buffer{};
	const auto written =
		std::format_to_n(buffer.data(), buffer.size() - 1,
						 "mouse_sensitivity={}\nvolume={}\nshow_fps={}\n",
						 mouse_sensitivity, volume, show_fps ? 1 : 0);
	const auto size =
		std::min(static_cast<std::size_t>(written.size), buffer.size() - 1);
	buffer[size] = '\0';
	WriteSettingsText(std::string_view(buffer.data(), size));
}

}  // namespace wolfenstein
