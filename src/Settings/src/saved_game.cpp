#include "Settings/saved_game.h"
#include "Settings/storage.h"
#include <array>
#include <charconv>
#include <string>

namespace wolfenstein {

namespace {

template <typename Number>
bool ParseNumber(std::string_view text, Number& value) {
	Number parsed{};
	const auto [end, error] =
		std::from_chars(text.data(), text.data() + text.size(), parsed);
	if (error != std::errc{} || end != text.data() + text.size()) {
		return false;
	}
	value = parsed;
	return true;
}

}  // namespace

std::optional<SavedGame> SavedGame::Parse(std::string_view text) {
	SavedGame game;
	// Each field, as it is found
	constexpr unsigned kLevel = 1, kWeapon = 2, kDifficulty = 4, kHealth = 8,
					   kAmmo = 16, kReserve = 32, kAll = 63;
	unsigned seen = 0;
	while (!text.empty()) {
		const auto end = text.find('\n');
		const std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view{}
											 : text.substr(end + 1);
		const auto separator = line.find('=');
		if (separator == std::string_view::npos) {
			continue;
		}
		const std::string_view key = line.substr(0, separator);
		const std::string_view value = line.substr(separator + 1);
		bool read = false;
		unsigned field = 0;
		if (key == "level") {
			read = ParseNumber(value, game.level);
			field = kLevel;
		}
		else if (key == "weapon") {
			read = ParseNumber(value, game.weapon);
			field = kWeapon;
		}
		else if (key == "difficulty") {
			read = ParseNumber(value, game.difficulty);
			field = kDifficulty;
		}
		else if (key == "health") {
			read = ParseNumber(value, game.health);
			field = kHealth;
		}
		else if (key == "ammo") {
			read = ParseNumber(value, game.ammo);
			field = kAmmo;
		}
		else if (key == "reserve") {
			read = ParseNumber(value, game.reserve);
			field = kReserve;
		}
		else {
			continue;  // a key this version does not know
		}
		if (!read) {
			return std::nullopt;
		}
		seen |= field;
	}
	if (seen != kAll || game.health <= 0.0) {
		return std::nullopt;
	}
	return game;
}

std::size_t SavedGame::Format(std::span<char> out) const {
	RecordWriter writer(out);
	writer.Line("level", level)
		.Line("weapon", weapon)
		.Line("difficulty", difficulty)
		.Line("health", health)
		.Line("ammo", ammo)
		.Line("reserve", reserve);
	if (writer.Text().empty() && !out.empty()) {
		out[0] = '\0';
	}
	return writer.Text().size();
}

std::optional<SavedGame> SavedGame::Load() {
	return Parse(ReadRecord(Record::SavedGame));
}

void SavedGame::Save() const {
	std::array<char, 256> buffer{};
	const std::size_t size = Format(buffer);
	if (size > 0) {
		WriteRecord(Record::SavedGame, std::string_view(buffer.data(), size));
	}
}

void SavedGame::Clear() {
	ClearRecord(Record::SavedGame);
}

}  // namespace wolfenstein
