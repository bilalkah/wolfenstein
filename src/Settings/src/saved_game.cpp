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

constexpr std::string_view kHexDigits = "0123456789abcdef";

// Bytes as two hex digits each, into `out`; false if it does not fit
bool ToHex(std::span<const std::uint8_t> bytes, std::span<char> out) {
	if (out.size() < bytes.size() * 2) {
		return false;
	}
	for (std::size_t i = 0; i < bytes.size(); ++i) {
		out[2 * i] = kHexDigits[bytes[i] >> 4];
		out[2 * i + 1] = kHexDigits[bytes[i] & 0xF];
	}
	return true;
}

bool FromHex(std::string_view text, std::span<std::uint8_t> bytes) {
	if (text.size() % 2 != 0 || text.size() / 2 > bytes.size()) {
		return false;
	}
	for (std::size_t i = 0; i < text.size(); ++i) {
		const auto digit = kHexDigits.find(text[i]);
		if (digit == std::string_view::npos) {
			return false;
		}
		auto& byte = bytes[i / 2];
		byte =
			static_cast<std::uint8_t>(i % 2 == 0 ? digit << 4 : byte | digit);
	}
	return true;
}

}  // namespace

std::optional<SavedGame> SavedGame::Parse(std::string_view text) {
	SavedGame game;
	// Each field, as it is found; weapons and their rounds are optional
	constexpr unsigned kLevel = 1, kWeapon = 2, kDifficulty = 4, kHealth = 8,
					   kAll = 15, kX = 64, kY = 128, kTheta = 256,
					   kPosition = kX | kY | kTheta;
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
		else if (key == "weapons") {
			read = ParseNumber(value, game.weapons);
		}
		else if (key.starts_with("ammo_") || key.starts_with("reserve_")) {
			// ammo_<weapon> and reserve_<weapon>
			const bool ammo = key.starts_with("ammo_");
			std::size_t index = 0;
			read = ParseNumber(key.substr(ammo ? 5 : 8), index) &&
				   index < kMaxWeapons &&
				   ParseNumber(value,
							   ammo ? game.ammo[index] : game.reserve[index]);
		}
		else if (key == "x" || key == "y" || key == "theta") {
			double& target = key == "x"	  ? game.x
							 : key == "y" ? game.y
										  : game.theta;
			read = ParseNumber(value, target);
			field = key == "x" ? kX : key == "y" ? kY : kTheta;
		}
		else if (key == "seconds") {
			read = ParseNumber(value, game.seconds);
		}
		else if (key == "killed") {
			read = ParseNumber(value, game.killed);
		}
		else if (key == "taken") {
			read = ParseNumber(value, game.taken);
		}
		else if (key == "keys") {
			read = ParseNumber(value, game.keys);
		}
		else if (key == "secrets") {
			read = ParseNumber(value, game.secrets);
		}
		else if (key == "explored_cells") {
			read = ParseNumber(value, game.explored_cells) &&
				   game.explored_cells <= kMaxExploredCells;
		}
		else if (key == "explored") {
			read = FromHex(value, game.explored);
		}
		else {
			continue;  // a key this version does not know
		}
		if (!read) {
			return std::nullopt;
		}
		seen |= field;
	}
	if ((seen & kAll) != kAll || game.health <= 0.0) {
		return std::nullopt;
	}
	// A position is all three coordinates or none
	const unsigned position = seen & kPosition;
	if (position != 0 && position != kPosition) {
		return std::nullopt;
	}
	game.has_position = position == kPosition;
	return game;
}

std::size_t SavedGame::Format(std::span<char> out) const {
	RecordWriter writer(out);
	writer.Line("level", level)
		.Line("weapon", weapon)
		.Line("difficulty", difficulty)
		.Line("health", health)
		.Line("weapons", weapons);
	constexpr std::array<std::string_view, kMaxWeapons> kAmmoKeys{
		"ammo_0", "ammo_1", "ammo_2", "ammo_3",
		"ammo_4", "ammo_5", "ammo_6", "ammo_7"};
	constexpr std::array<std::string_view, kMaxWeapons> kReserveKeys{
		"reserve_0", "reserve_1", "reserve_2", "reserve_3",
		"reserve_4", "reserve_5", "reserve_6", "reserve_7"};
	for (std::size_t i = 0; i < kMaxWeapons; ++i) {
		if ((weapons >> i & 1U) != 0) {
			writer.Line(kAmmoKeys[i], ammo[i])
				.Line(kReserveKeys[i], reserve[i]);
		}
	}
	if (has_position) {
		writer.Line("x", x).Line("y", y).Line("theta", theta);
	}
	writer.Line("seconds", seconds)
		.Line("killed", killed)
		.Line("taken", taken)
		.Line("keys", keys)
		.Line("secrets", secrets)
		.Line("explored_cells", explored_cells);
	// Only the bytes the explored cells use
	std::array<char, kMaxExploredCells / 8 * 2> hex{};
	const std::size_t bytes = (explored_cells + 7) / 8;
	if (ToHex(std::span(explored).first(bytes), hex)) {
		writer.LineText("explored", std::string_view(hex.data(), 2 * bytes));
	}
	if (writer.Text().empty() && !out.empty()) {
		out[0] = '\0';
	}
	return writer.Text().size();
}

std::optional<SavedGame> SavedGame::Load() {
	return Parse(ReadRecord(Record::SavedGame));
}

void SavedGame::Save() const {
	std::array<char, 1024> buffer{};
	const std::size_t size = Format(buffer);
	if (size > 0) {
		WriteRecord(Record::SavedGame, std::string_view(buffer.data(), size));
	}
}

void SavedGame::Clear() {
	ClearRecord(Record::SavedGame);
}

}  // namespace wolfenstein
