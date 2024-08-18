#include <Map/map.h>
#include <string>

namespace wolfenstein {

namespace {

std::string simple_map{
	"3333333333333333"
	"3000000000000003"
	"3000000000000003"
	"3001111000000003"
	"3001110001111003"
	"3001110011000013"
	"3000000001000013"
	"3000000001111003"
	"3000000000000003"
	"3000000000000003"
	"3111111100011003"
	"3000000000011003"
	"3000111100000003"
	"3000100000000003"
	"3000100000000003"
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
}  // namespace

Map::Map() {
	LoadMap();
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

std::vector<std::vector<uint16_t>> Map::GetMap() {
	return map_;
}

uint16_t Map::GetSizeX() {
	return size_x_;
}
uint16_t Map::GetSizeY() {
	return size_y_;
}

}  // namespace wolfenstein
