#include "Map/map.h"
#include <string>

namespace wolfenstein {

namespace {

std::string simple_map{
	"3333333333333333"
	"3000000000000003"
	"3000000000000003"
	"3001111000000003"
	"3000000002222003"
	"3001110022000053"
	"3000000002000053"
	"3000000002222003"
	"3000000000000003"
	"3000000000000003"
	"3333333300044003"
	"3000000000044003"
	"3000333300000003"
	"3000300000000003"
	"3000300000000003"
	"3111111111111003"
	"3000000100000003"
	"3000000100000003"
	"3000000100001013"
	"3000000100001003"
	"3111000101101003"
	"3000000100001003"
	"3000000000001103"
	"3000000000001003"
	"3000000000001003"
	"3333333333333333"};

constexpr double res{0.1};
}  // namespace

Map::Map() : size_x_(26), size_y_(16) {
	LoadMap();
	MapToPathFinderMap();
}

void Map::LoadMap() {
	size_x_ = 26;
	size_y_ = 16;
	uint16_t it = 0;
	for (uint16_t i = 0; i < size_x_; i++) {
		std::vector<uint16_t> row;
		for (uint16_t j = 0; j < size_y_; j++) {
			auto c = (simple_map[it]);
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
			++it;
		}
		map_.push_back(row);
	}
}

void Map::MapToPathFinderMap() {
	path_finder_map_ =
		std::make_shared<planning::Map>(size_x_ / res, size_y_ / res);
	for (uint16_t i = 0; i < size_x_; i++) {
		for (uint16_t j = 0; j < size_y_; j++) {
			if (map_[i][j] == 0) {
				for (uint16_t k = 0; k < (1 / res) ; k++) {
					for (uint16_t l = 0; l < (1 / res) ; l++) {
						path_finder_map_->SetNodeState(
							planning::Node(i / res + k, j / res + l),
							planning::NodeState::kFree);
					}
				}
			}
		}
	}
}

std::vector<std::vector<uint16_t>> Map::GetMap() {
	return map_;
}

std::shared_ptr<planning::Map> Map::GetPathFinderMap() {
	return path_finder_map_;
}

uint16_t Map::GetSizeX() {
	return size_x_;
}
uint16_t Map::GetSizeY() {
	return size_y_;
}

}  // namespace wolfenstein
