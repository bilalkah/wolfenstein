#include "GameMap/map.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string_view>
#include <utility>

namespace karakale {

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
			// D a door; G and S doors locked with the gold and silver keys
			if (c == 'D' || c == 'G' || c == 'S') {
				const auto column = map.cells_.size() % map.size_y_;
				map.cells_.push_back(
					static_cast<std::uint16_t>(kDoorCell + map.doors_.size()));
				map.doors_.push_back({.x = static_cast<std::uint16_t>(rows),
									  .y = static_cast<std::uint16_t>(column),
									  .lock = c == 'G'	 ? KeyColour::Gold
											  : c == 'S' ? KeyColour::Silver
														 : KeyColour::None});
				continue;
			}
			if (c == 'X') {
				if (map.has_exit_) {
					return std::unexpected(path + ": more than one exit");
				}
				map.has_exit_ = true;
				map.exit_x_ = static_cast<int>(rows);
				map.exit_y_ = static_cast<int>(map.cells_.size() % map.size_y_);
				map.cells_.push_back(kExitWall);
				continue;
			}
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
	// A door stands between two walls facing each other, the way through
	// open on its other sides
	for (Door& door : map.doors_) {
		const int x = door.x;
		const int y = door.y;
		const auto open = [&](int cx, int cy) {
			return !map.IsWall(cx, cy) && map.FindDoor(cx, cy) == nullptr;
		};
		if (map.IsWall(x, y - 1) && map.IsWall(x, y + 1) && open(x - 1, y) &&
			open(x + 1, y)) {
			door.across_x = true;
		}
		else if (map.IsWall(x - 1, y) && map.IsWall(x + 1, y) &&
				 open(x, y - 1) && open(x, y + 1)) {
			door.across_x = false;
		}
		else {
			return std::unexpected(path + ": the door at " + std::to_string(x) +
								   "," + std::to_string(y) +
								   " is not between two walls");
		}
	}
	return map;
}

Map::Map(const Map& other, std::pmr::memory_resource* memory)
	: size_x_(other.size_x_),
	  size_y_(other.size_y_),
	  cells_(other.cells_, memory),
	  doors_(other.doors_, memory),
	  push_walls_(other.push_walls_, memory),
	  has_exit_(other.has_exit_),
	  exit_x_(other.exit_x_),
	  exit_y_(other.exit_y_) {}

namespace {

Map LoadOrExit(const std::string& map_path) {
	auto map = Map::FromFile(map_path);
	if (!map) {
		std::cerr << "Cannot load map: " << map.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	return std::move(*map);
}

}  // namespace

Map::Map(const std::string& map_path) : Map(LoadOrExit(map_path)) {}

bool Map::Contains(int x, int y) const {
	return x >= 0 && std::cmp_less(x, size_x_) && y >= 0 &&
		   std::cmp_less(y, size_y_);
}

bool Map::IsBlocked(int x, int y) const {
	if (!Contains(x, y)) {
		return true;
	}
	// A sliding secret fills every cell of its way until it stops
	for (const PushWall& wall : push_walls_) {
		for (int step = 0; wall.moving && step <= PushWall::kDistance; ++step) {
			if (x == wall.x + step * wall.dx && y == wall.y + step * wall.dy) {
				return true;
			}
		}
	}
	const std::uint16_t cell =
		GetCells()[static_cast<std::size_t>(x), static_cast<std::size_t>(y)];
	if (IsDoorCell(cell)) {
		return doors_[cell - kDoorCell].openness < kPassableOpenness;
	}
	return cell != 0;
}

bool Map::IsWall(int x, int y) const {
	if (!Contains(x, y)) {
		return true;
	}
	const std::uint16_t cell =
		GetCells()[static_cast<std::size_t>(x), static_cast<std::size_t>(y)];
	return cell != 0 && !IsDoorCell(cell);
}

bool Map::AddPushWall(int x, int y, int dx, int dy) {
	if (!Contains(x, y) || !IsWall(x, y)) {
		return false;
	}
	for (int step = 1; step <= PushWall::kDistance; ++step) {
		if (IsBlocked(x + step * dx, y + step * dy)) {
			return false;
		}
	}
	push_walls_.push_back({.x = x,
						   .y = y,
						   .dx = dx,
						   .dy = dy,
						   .texture = GetCells()[static_cast<std::size_t>(x),
												 static_cast<std::size_t>(y)]});
	return true;
}

const PushWall* Map::FindPushWall(int x, int y) const {
	for (const PushWall& wall : push_walls_) {
		if (!wall.pushed && wall.x == x && wall.y == y) {
			return &wall;
		}
	}
	return nullptr;
}

void Map::Push(std::size_t index, bool finish) {
	PushWall& wall = push_walls_[index];
	if (wall.pushed) {
		return;
	}
	wall.pushed = true;
	wall.moving = true;
	// While it slides it is drawn and blocks on its own, not as cells
	cells_[(static_cast<std::size_t>(wall.x) * size_y_) +
		   static_cast<std::size_t>(wall.y)] = 0;
	if (finish) {
		AdvancePushWalls(PushWall::kDistance);
	}
}

void Map::AdvancePushWalls(double cells) {
	constexpr double kEnd = PushWall::kDistance;
	for (PushWall& wall : push_walls_) {
		if (!wall.moving) {
			continue;
		}
		wall.offset = std::min(wall.offset + cells, kEnd);
		if (wall.offset >= kEnd) {
			wall.moving = false;
			const int x = wall.x + PushWall::kDistance * wall.dx;
			const int y = wall.y + PushWall::kDistance * wall.dy;
			cells_[(static_cast<std::size_t>(x) * size_y_) +
				   static_cast<std::size_t>(y)] = wall.texture;
		}
	}
}

bool Map::IsLockedDoor(int x, int y) const {
	const Door* door = FindDoor(x, y);
	return door != nullptr && door->lock != KeyColour::None;
}

const Door* Map::FindDoor(int x, int y) const {
	if (!Contains(x, y)) {
		return nullptr;
	}
	const std::uint16_t cell =
		GetCells()[static_cast<std::size_t>(x), static_cast<std::size_t>(y)];
	return IsDoorCell(cell) ? &doors_[cell - kDoorCell] : nullptr;
}

void Map::SetDoorOpenness(std::size_t door, double openness) {
	doors_[door].openness = std::clamp(openness, 0.0, 1.0);
}

bool Map::IsBlocked(const vector2d& position) const {
	return IsBlocked(static_cast<int>(std::floor(position.x)),
					 static_cast<int>(std::floor(position.y)));
}

}  // namespace karakale
