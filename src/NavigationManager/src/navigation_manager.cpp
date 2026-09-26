#include "NavigationManager/navigation_manager.h"
#include "Characters/enemy.h"
#include "Core/scene.h"
#include "GameMap/map.h"
#include "Math/vector.h"
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace wolfenstein {

namespace {
constexpr int kCellsPerSide =
	static_cast<int>(1.0 / NavigationManager::kCellSize);
}  // namespace

NavigationManager::NavigationManager(const Scene& scene,
									 std::pmr::memory_resource* memory)
	: scene_(scene),
	  // Weight 0.6: f = 0.4 g + 0.6 h, the tuning the game has always used
	  path_finder_(0.6, memory),
	  routes_(memory),
	  obstacles_(memory),
	  cells_(memory) {}

std::size_t NavigationManager::MemoryFor(int map_rows, int map_cols,
										 std::size_t objects,
										 std::size_t enemies) {
	const int height = map_rows * kCellsPerSide;
	const int width = map_cols * kCellsPerSide;
	const auto cells =
		static_cast<std::size_t>(height) * static_cast<std::size_t>(width);
	constexpr std::size_t kPadding = alignof(std::max_align_t);
	return GridPathFinder::MemoryFor(height, width) + objects * sizeof(Route) +
		   2 * enemies * sizeof(GridCell) + cells * sizeof(GridCell) +
		   3 * kPadding;
}

void NavigationManager::Build() {
	// Each map cell splits into kCellsPerSide^2 pathfinding cells, built
	// straight into the path finder's grid. Only walls block a route:
	// enemies open the doors on it as they reach them.
	const Map& map = scene_.GetMap();
	path_finder_.SetGrid(map.GetSizeX() * kCellsPerSide,
						 map.GetSizeY() * kCellsPerSide, [&](int x, int y) {
							 return map.IsWall(x / kCellsPerSide,
											   y / kCellsPerSide);
						 });

	// A path visits each free cell at most once, so the scratch path never
	// grows during play
	cells_.reserve(path_finder_.FreeCells());
	obstacles_.reserve(2 * scene_.GetEnemies().size());
	routes_.assign(scene_.GetObjects().size(), Route{});
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
	for (const auto& enemy : scene_.GetEnemies()) {
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
	return FindPath(start, scene_.GetPlayer().GetPosition(), id);
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

double NavigationManager::EuclideanDistanceToPlayer(
	const Position2D& position) const {
	return scene_.GetPlayer().GetPosition().pose.Distance(position.pose);
}

}  // namespace wolfenstein
