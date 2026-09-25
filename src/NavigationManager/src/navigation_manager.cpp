#include "NavigationManager/navigation_manager.h"
#include "Characters/enemy.h"
#include "Core/scene.h"
#include "Map/map.h"
#include "Math/vector.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <span>
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

	// Each map cell splits into cells_per_side^2 pathfinding cells, built
	// straight into the path finder's grid
	const Map& map = scene_->GetMap();
	const int cells_per_side = static_cast<int>(1.0 / kCellSize);
	path_finder_.SetGrid(map.GetSizeX() * cells_per_side,
						 map.GetSizeY() * cells_per_side, [&](int x, int y) {
							 return map.IsBlocked(x / cells_per_side,
												  y / cells_per_side);
						 });

	// A path visits each free cell at most once, so the scratch path never
	// grows during play
	cells_.reserve(path_finder_.FreeCells());
	obstacles_.reserve(2 * scene_->GetEnemies().size());
	routes_.assign(scene_->GetObjects().size(), Route{});
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
		const Route& route = routes_[ToIndex(enemy->GetId())];
		if (route.size > 0) {
			obstacles_.push_back(route.cells[0]);
		}
	}
}

// Returns the waypoint the enemy should head for: two cells along the path,
// which smooths the movement, or the last cell of a shorter path. The start
// of the remaining path is stored for the enemy.
vector2d NavigationManager::FindPath(Position2D start, Position2D end,
									 ObjectId id) {
	Route& route = routes_[ToIndex(id)];
	const auto stay = [&] {
		route.cells[0] = ToCell(start.pose);
		route.size = 1;
		return start.pose;
	};
	if (start.pose.Distance(end.pose) < kCellSize * 0.9) {
		return stay();
	}

	CollectDynamicObstacles();
	if (!path_finder_.FindPath(ToCell(start.pose), ToCell(end.pose), obstacles_,
							   cells_) ||
		cells_.size() < 2) {
		return stay();
	}
	// Skip the start cell: the enemy is already there
	const auto path = std::span(cells_).subspan(1);
	route.size =
		static_cast<std::uint32_t>(std::min(path.size(), Route::kCapacity));
	std::ranges::copy(path.first(route.size), route.cells.begin());
	return CellCentre(path[std::min<std::size_t>(1, path.size() - 1)]);
}

vector2d NavigationManager::FindPathToPlayer(Position2D start, ObjectId id) {
	return FindPath(start, *player_position_ptr_, id);
}

std::span<const GridCell> NavigationManager::GetPath(ObjectId id) const {
	const auto index = ToIndex(id);
	if (index >= routes_.size()) {
		return {};
	}
	return std::span(routes_[index].cells).first(routes_[index].size);
}

void NavigationManager::ResetPath(ObjectId id) {
	routes_[ToIndex(id)].size = 0;
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
