#include "Settings/settings.h"
#include "Settings/storage.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <sstream>
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

// Stored as "key=value" lines; unknown keys and malformed values are ignored
void Settings::Load() {
	std::istringstream lines(ReadRecord(Record::Settings));
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
		else if (key == "difficulty") {
			difficulty = std::clamp(static_cast<int>(number), 0, 9);
		}
	}
}

// Formats into a buffer on the stack: no allocation while the game runs
void Settings::Save() const {
	std::array<char, 128> buffer{};
	RecordWriter writer(buffer);
	writer.Line("mouse_sensitivity", mouse_sensitivity)
		.Line("volume", volume)
		.Line("show_fps", show_fps ? 1 : 0)
		.Line("difficulty", difficulty);
	if (!writer.Text().empty()) {
		WriteRecord(Record::Settings, writer.Text());
	}
}

}  // namespace wolfenstein
