#include "NavigationManager/navigation_manager.h"
#include "Characters/enemy.h"
#include "Core/scene.h"
#include "Map/map.h"
#include "Math/vector.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace wolfenstein {

NavigationManager* NavigationManager::instance_ = nullptr;

NavigationManager& NavigationManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new NavigationManager();
	}
	return *instance_;
}

NavigationManager::~NavigationManager() {
	delete instance_;
}

void NavigationManager::InitManager(const std::shared_ptr<Scene>& scene) {
	scene_ = scene;
	paths_.clear();

	const Map& map = scene_->GetMap();
	const int cells_per_side = static_cast<int>(1.0 / kCellSize);
	const int height = map.GetSizeX() * cells_per_side;
	const int width = map.GetSizeY() * cells_per_side;
	walls_.assign(static_cast<std::size_t>(height * width), 0);
	for (int x = 0; x < height; ++x) {
		for (int y = 0; y < width; ++y) {
			walls_[static_cast<std::size_t>(x * width + y)] =
				map.IsBlocked(x / cells_per_side, y / cells_per_side) ? 1 : 0;
		}
	}
	path_finder_.SetGrid(height, width, walls_);
}

GridCell NavigationManager::ToCell(const vector2d& position) {
	return {static_cast<int>(std::floor(position.x / kCellSize)),
			static_cast<int>(std::floor(position.y / kCellSize))};
}

vector2d NavigationManager::CellCentre(GridCell cell) {
	return vector2d{(cell.x + 0.5) * kCellSize, (cell.y + 0.5) * kCellSize};
}

void NavigationManager::CollectDynamicObstacles() {
	obstacles_.clear();
	for (const auto& enemy : scene_->GetEnemies()) {
		if (!enemy->IsAlive()) {
			continue;
		}
		obstacles_.push_back(ToCell(enemy->GetPose()));
		const auto found = paths_.find(enemy->GetId());
		if (found != paths_.end() && !found->second.empty()) {
			obstacles_.push_back(ToCell(found->second.front()));
		}
	}
}

// Returns the waypoint the enemy should head for: two cells along the path,
// which smooths the movement, or the last cell of a shorter path. The whole
// remaining path is stored for the enemy.
vector2d NavigationManager::FindPath(Position2D start, Position2D end,
									 const std::string& id) {
	auto& waypoints = paths_[id];
	waypoints.clear();
	if (start.pose.Distance(end.pose) < kCellSize * 0.9) {
		waypoints.push_back(start.pose);
		return start.pose;
	}

	CollectDynamicObstacles();
	if (!path_finder_.FindPath(ToCell(start.pose), ToCell(end.pose), obstacles_,
							   cells_) ||
		cells_.size() < 2) {
		waypoints.push_back(start.pose);
		return start.pose;
	}
	// Skip the start cell: the enemy is already there
	for (std::size_t i = 1; i < cells_.size(); ++i) {
		waypoints.push_back(CellCentre(cells_[i]));
	}
	return waypoints[std::min<std::size_t>(1, waypoints.size() - 1)];
}

vector2d NavigationManager::FindPathToPlayer(Position2D start,
											 const std::string& id) {
	return FindPath(start, *player_position_ptr_, id);
}

const std::vector<vector2d>& NavigationManager::GetPath(const std::string& id) {
	return paths_[id];
}

void NavigationManager::ResetPath(const std::string& id) {
	paths_[id].clear();
}

void NavigationManager::SetPositionPtr(
	const std::shared_ptr<Position2D>& position) {
	player_position_ptr_ = position;
}

double NavigationManager::EuclideanDistanceToPlayer(
	const Position2D& position) {
	return player_position_ptr_->pose.Distance(position.pose);
}

double NavigationManager::ManhattanDistanceToPlayer(
	const Position2D& position) {
	return player_position_ptr_->pose.MDistance(position.pose);
}

}  // namespace wolfenstein
