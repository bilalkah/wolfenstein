#include "Map/map.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

namespace wolfenstein {

Map::Map(const std::string& map_path) {
	LoadMap(map_path);
}

void Map::LoadMap(const std::string& map_path) {
	std::ifstream infile(map_path);
	std::string line;
	std::getline(infile, line);
	size_x_ = std::stoi(line.substr(std::string("height ").size()));
	std::getline(infile, line);
	size_y_ = std::stoi(line.substr(std::string("width ").size()));

	while (std::getline(infile, line)) {
		std::vector<uint16_t> row;
		for (auto c : line) {
			if (c == '0') {
				row.push_back(0);
			}
			else if (c == '1') {
				row.push_back(1);
			}
			else if (c == '2') {
				row.push_back(2);
			}
			else if (c == '3') {
				row.push_back(3);
			}
			else if (c == '4') {
				row.push_back(4);
			}
			else if (c == '5') {
				row.push_back(5);
			}
		}
		map_.emplace_back(row);
	}
}

const MapRaw& Map::GetRawMap() const {
	return map_;
}

const uint16_t Map::GetSizeX() const {
	return size_x_;
}
const uint16_t Map::GetSizeY() const {
	return size_y_;
}

const std::vector<uint16_t>& Map::operator[](size_t i) const {
	return map_[i];
}

bool Map::Contains(int x, int y) const {
	return x >= 0 && x < size_x_ && y >= 0 && y < size_y_;
}

bool Map::IsBlocked(int x, int y) const {
	return !Contains(x, y) ||
		   map_[static_cast<size_t>(x)][static_cast<size_t>(y)] != 0;
}

bool Map::IsBlocked(const vector2d& position) const {
	return IsBlocked(static_cast<int>(std::floor(position.x)),
					 static_cast<int>(std::floor(position.y)));
}

}  // namespace wolfenstein
