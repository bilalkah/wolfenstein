#include "Settings/settings.h"
#include "Settings/storage.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>

namespace wolfenstein {

namespace {

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

void Settings::Parse(std::string_view text) {
	while (!text.empty()) {
		const auto end = text.find('\n');
		const std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view{}
											 : text.substr(end + 1);
		const auto separator = line.find('=');
		double number = 0.0;
		if (separator == std::string_view::npos ||
			!ParseDouble(line.substr(separator + 1), number)) {
			continue;
		}
		const std::string_view key = line.substr(0, separator);
		if (key == "mouse_sensitivity") {
			mouse_sensitivity =
				std::clamp(number, kMinMouseSensitivity, kMaxMouseSensitivity);
		}
		else if (key == "invert_mouse_y") {
			invert_mouse_y = number != 0.0;
		}
		else if (key == "fov") {
			fov = std::clamp(number, kMinFov, kMaxFov);
		}
		else if (key == "volume") {
			volume = std::clamp(number, 0.0, 1.0);
		}
		else if (key == "music_volume") {
			music_volume = std::clamp(number, 0.0, 1.0);
		}
		else if (key == "effects_volume") {
			effects_volume = std::clamp(number, 0.0, 1.0);
		}
		else if (key == "show_fps") {
			show_fps = number != 0.0;
		}
	}
}

std::size_t Settings::Format(std::span<char> out) const {
	RecordWriter writer(out);
	writer.Line("mouse_sensitivity", mouse_sensitivity)
		.Line("invert_mouse_y", invert_mouse_y ? 1 : 0)
		.Line("fov", fov)
		.Line("volume", volume)
		.Line("music_volume", music_volume)
		.Line("effects_volume", effects_volume)
		.Line("show_fps", show_fps ? 1 : 0);
	return writer.Text().size();
}

void Settings::Load() {
	Parse(ReadRecord(Record::Settings));
}

// Formats into a buffer on the stack: no allocation while the game runs
void Settings::Save() const {
	std::array<char, 256> buffer{};
	const std::size_t size = Format(buffer);
	if (size > 0) {
		WriteRecord(Record::Settings, std::string_view(buffer.data(), size));
	}
}

}  // namespace wolfenstein
