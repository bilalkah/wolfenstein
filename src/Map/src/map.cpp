#include "Map/map.h"
#include <iostream>
#include <string>

namespace wolfenstein {

Map::Map(std::string map_path, double resolution) : res(resolution) {
	LoadMap(map_path);
	MapToPathFinderMap();
}

void Map::LoadMap(std::string map_path) {
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

void Map::MapToPathFinderMap() {
	path_finder_map_ =
		std::make_shared<planning::Map>(size_x_ / res, size_y_ / res);
	for (uint16_t i = 0; i < size_x_; i++) {
		for (uint16_t j = 0; j < size_y_; j++) {
			if (map_[i][j] == 0) {
				for (uint16_t k = 0; k < (1 / res); k++) {
					for (uint16_t l = 0; l < (1 / res); l++) {
						path_finder_map_->SetNodeState(
							planning::Node(i / res + k, j / res + l),
							planning::NodeState::kFree);
					}
				}
			}
		}
	}
}

const MapRaw& Map::GetRawMap() const {
	return map_;
}

std::shared_ptr<planning::Map> Map::GetPathFinderMap() {
	return path_finder_map_;
}

const uint16_t Map::GetSizeX() const {
	return size_x_;
}
const uint16_t Map::GetSizeY() const {
	return size_y_;
}

double Map::GetResolution() {
	return res;
}

const std::vector<uint16_t>& Map::operator[](size_t i) const {
	return map_[i];
}

}  // namespace wolfenstein
