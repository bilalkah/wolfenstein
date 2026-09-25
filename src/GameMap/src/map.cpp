#include "GameMap/map.h"
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string_view>

namespace wolfenstein {

namespace {

// Reads "<key> <number>" from a header line
std::expected<std::uint16_t, std::string> ReadHeader(std::string_view line,
													 std::string_view key) {
	if (!line.starts_with(key) || line.size() <= key.size() ||
		line[key.size()] != ' ') {
		return std::unexpected("expected \"" + std::string(key) + " <n>\"");
	}
	std::uint16_t value = 0;
	const auto digits = line.substr(key.size() + 1);
	const auto [end, error] =
		std::from_chars(digits.data(), digits.data() + digits.size(), value);
	if (error != std::errc{} || end != digits.data() + digits.size() ||
		value == 0) {
		return std::unexpected("bad " + std::string(key) + " \"" +
							   std::string(digits) + "\"");
	}
	return value;
}

}  // namespace

std::expected<Map, std::string> Map::FromFile(const std::string& path) {
	std::ifstream file(path);
	if (!file.is_open()) {
		return std::unexpected("cannot open " + path);
	}
	Map map;
	std::string line;  // reused for every line
	std::getline(file, line);
	const auto height = ReadHeader(line, "height");
	std::getline(file, line);
	const auto width = ReadHeader(line, "width");
	if (!height || !width) {
		return std::unexpected(path + ": " +
							   (!height ? height.error() : width.error()));
	}
	map.size_x_ = *height;
	map.size_y_ = *width;
	map.cells_.reserve(std::size_t{map.size_x_} * map.size_y_);

	std::size_t rows = 0;
	while (std::getline(file, line)) {
		if (line.ends_with('\r')) {
			line.pop_back();
		}
		if (line.empty()) {
			continue;
		}
		if (line.size() != map.size_y_ || rows == map.size_x_) {
			return std::unexpected(path + ": row " + std::to_string(rows) +
								   " does not fit a " +
								   std::to_string(map.size_x_) + "x" +
								   std::to_string(map.size_y_) + " map");
		}
		for (const char c : line) {
			if (c < '0' || c > '5') {
				return std::unexpected(path + ": unknown cell '" +
									   std::string(1, c) + "'");
			}
			map.cells_.push_back(static_cast<std::uint16_t>(c - '0'));
		}
		++rows;
	}
	if (rows != map.size_x_) {
		return std::unexpected(path + ": " + std::to_string(rows) +
							   " rows, expected " +
							   std::to_string(map.size_x_));
	}
	return map;
}

Map::Map(const std::string& map_path) {
	auto map = FromFile(map_path);
	if (!map) {
		std::cerr << "Cannot load map: " << map.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	*this = std::move(*map);
}

bool Map::Contains(int x, int y) const {
	return x >= 0 && x < size_x_ && y >= 0 && y < size_y_;
}

bool Map::IsBlocked(int x, int y) const {
	return !Contains(x, y) || GetCells()[static_cast<std::size_t>(x),
										 static_cast<std::size_t>(y)] != 0;
}

bool Map::IsBlocked(const vector2d& position) const {
	return IsBlocked(static_cast<int>(std::floor(position.x)),
					 static_cast<int>(std::floor(position.y)));
}

}  // namespace wolfenstein
