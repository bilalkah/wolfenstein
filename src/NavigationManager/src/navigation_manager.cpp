#include "NavigationManager/navigation_manager.h"
#include "Characters/enemy.h"
#include "Core/scene.h"
#include "GameMap/map.h"
#include "Math/vector.h"
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace karakale {

namespace {
constexpr int kCellsPerSide =
	static_cast<int>(1.0 / NavigationManager::kCellSize);
}  // namespace

NavigationManager::NavigationManager(const Scene& scene,
									 std::pmr::memory_resource* memory)
	: scene_(scene),
	  // Weight 0.6: f = 0.4 g + 0.6 h, the tuning the game has always used
	  path_finder_(0.6, memory),
	  solids_(memory),
	  routes_(memory),
	  obstacles_(memory),
	  crowded_(memory),
	  cells_(memory) {}

std::size_t NavigationManager::MemoryFor(int map_rows, int map_cols,
										 std::size_t objects,
										 std::size_t enemies) {
	const int height = map_rows * kCellsPerSide;
	const int width = map_cols * kCellsPerSide;
	const auto cells =
		static_cast<std::size_t>(height) * static_cast<std::size_t>(width);
	constexpr std::size_t kPadding = alignof(std::max_align_t);
	return GridPathFinder::MemoryFor(height, width) +
		   objects * (sizeof(Route) + sizeof(Solid)) +
		   (2 + kCrowdedPerRoute) * enemies * sizeof(GridCell) +
		   cells * sizeof(GridCell) + 5 * kPadding;
}

void NavigationManager::Build() {
	// Each map cell splits into kCellsPerSide^2 pathfinding cells, built
	// straight into the path finder's grid. Walls block a route, and locked
	// doors, which enemies cannot open (they open the others as they reach
	// them).
	const Map& map = scene_.GetMap();
	path_finder_.SetGrid(
		map.GetSizeX() * kCellsPerSide, map.GetSizeY() * kCellsPerSide,
		[&](int x, int y) {
			return map.IsWall(x / kCellsPerSide, y / kCellsPerSide) ||
				   map.IsLockedDoor(x / kCellsPerSide, y / kCellsPerSide);
		});

	// What stands on the floor for good (lamps) crowds the cells round it:
	// the cells where the widest enemy's centre could not be. A lamp stands
	// where four cells meet, so it crowds all four, and a route through one
	// of them would aim the enemy at a point it cannot reach.
	double clearance = 0.0;
	for (const Enemy* enemy : scene_.GetEnemies()) {
		clearance = std::max(clearance, enemy->GetRadius());
	}
	solids_.clear();
	solids_.reserve(scene_.GetObjects().size());
	for (const IGameObject* object : scene_.GetObjects()) {
		if (object->GetObjectType() != ObjectType::DYNAMIC_OBJECT ||
			object->GetCollisionRadius() <= 0.0) {
			continue;
		}
		const Solid solid{.centre = object->GetPose(),
						  .radius = object->GetCollisionRadius()};
		solids_.push_back(solid);
		const double reach = solid.radius + clearance;
		const GridCell low = ToCell(solid.centre - vector2d{reach, reach});
		const GridCell high = ToCell(solid.centre + vector2d{reach, reach});
		for (int x = low.x; x <= high.x; ++x) {
			for (int y = low.y; y <= high.y; ++y) {
				if (CellCentre({x, y}).Distance(solid.centre) < reach) {
					path_finder_.SetExtraCost({x, y}, kSqueezeCost);
				}
			}
		}
	}

	// A path visits each free cell at most once, so the scratch path never
	// grows during play
	cells_.reserve(path_finder_.FreeCells());
	obstacles_.reserve(2 * scene_.GetEnemies().size());
	crowded_.reserve(kCrowdedPerRoute * scene_.GetEnemies().size());
	routes_.assign(scene_.GetObjects().size(), Route{});
}

void NavigationManager::RefreshCell(int x, int y) {
	const Map& map = scene_.GetMap();
	// Blocked for good (a wall, a locked door) or for now (a secret sliding
	// through); a door that opens is a way through
	const bool blocked = map.IsWall(x, y) || map.IsLockedDoor(x, y) ||
						 (map.IsBlocked(x, y) && map.FindDoor(x, y) == nullptr);
	for (int dx = 0; dx < kCellsPerSide; ++dx) {
		for (int dy = 0; dy < kCellsPerSide; ++dy) {
			path_finder_.SetBlocked(
				{x * kCellsPerSide + dx, y * kCellsPerSide + dy}, blocked);
		}
	}
}

GridCell NavigationManager::ToCell(const vector2d& position) {
	return {static_cast<int>(std::floor(position.x / kCellSize)),
			static_cast<int>(std::floor(position.y / kCellSize))};
}

vector2d NavigationManager::CellCentre(GridCell cell) {
	return vector2d{(cell.x + 0.5) * kCellSize, (cell.y + 0.5) * kCellSize};
}

void NavigationManager::CollectDynamicObstacles(ObjectId self) {
	obstacles_.clear();
	crowded_.clear();
	for (const auto& enemy : scene_.GetEnemies()) {
		// Its own next cell would stand in its way, turning it aside
		// from its own route every other query
		if (!enemy->IsAlive() || enemy->GetId() == self) {
			continue;
		}
		obstacles_.push_back(ToCell(enemy->GetPose()));
		const Route& route = routes_[ToIndex(enemy->GetId())];
		if (route.size > 0) {
			obstacles_.push_back(route.cells[0]);
		}
		const auto ahead = std::min<std::size_t>(route.size, kCrowdedPerRoute);
		crowded_.insert(
			crowded_.end(), route.cells.begin(),
			route.cells.begin() + static_cast<std::ptrdiff_t>(ahead));
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

	// Round the other enemies if it can; if they fill the way (a doorway),
	// through them, as enemies pass each other
	CollectDynamicObstacles(id);
	const GridCell from = ToCell(start.pose);
	const GridCell to = ToCell(end.pose);
	const bool found =
		path_finder_.FindPath(from, to, obstacles_, cells_, crowded_,
							  kCrowdCost) ||
		(!obstacles_.empty() &&
		 path_finder_.FindPath(from, to, {}, cells_, crowded_, kCrowdCost));
	if (!found || cells_.size() < 2) {
		return stay();
	}
	// Skip the start cell: the enemy is already there
	const auto path = std::span(cells_).subspan(1);
	route.size =
		static_cast<std::uint32_t>(std::min(path.size(), Route::kCapacity));
	std::ranges::copy(path.first(route.size), route.cells.begin());
	const auto objects = scene_.GetObjects();
	const double radius = ToIndex(id) < objects.size()
							  ? objects[ToIndex(id)]->GetCollisionRadius()
							  : 0.0;
	return SteerRound(
		start.pose, CellCentre(path[std::min<std::size_t>(1, path.size() - 1)]),
		radius);
}

vector2d NavigationManager::SteerRound(const vector2d& from, vector2d target,
									   double radius) const {
	// Heading for the edge of a solid's reach would graze it: a little
	// past it
	constexpr double kMargin = 0.02;
	const auto reach_of = [&](const Solid& solid) {
		return solid.radius + radius + kMargin;
	};
	for (const Solid& solid : solids_) {
		const vector2d out = target - solid.centre;
		const double distance = std::hypot(out.x, out.y);
		if (distance < reach_of(solid) && distance > 1e-9) {
			target = solid.centre + out * (reach_of(solid) / distance);
		}
	}

	// The nearest solid the straight way passes within reach of
	const vector2d way = target - from;
	const double length = std::hypot(way.x, way.y);
	if (length < 1e-9) {
		return target;
	}
	const Solid* in_way = nullptr;
	double nearest = length;
	for (const Solid& solid : solids_) {
		const vector2d to_centre = solid.centre - from;
		// How far along the way it comes closest
		const double along =
			(to_centre.x * way.x + to_centre.y * way.y) / length;
		const vector2d closest =
			from + way * (std::clamp(along, 0.0, length) / length);
		if (along > 0.0 && along < nearest &&
			closest.Distance(solid.centre) < reach_of(solid)) {
			in_way = &solid;
			nearest = along;
		}
	}
	if (in_way == nullptr) {
		return target;
	}

	// Round it, turning from its centre towards the target's side: to
	// where the way from here touches its reach, or along its edge if
	// already there
	const vector2d to_centre = in_way->centre - from;
	const double distance = std::hypot(to_centre.x, to_centre.y);
	if (distance < 1e-9) {
		return target;
	}
	const double reach = reach_of(*in_way);
	const double side =
		to_centre.x * way.y - to_centre.y * way.x < 0.0 ? -1.0 : 1.0;
	const double turn = side * std::asin(std::min(1.0, reach / distance));
	const vector2d centre_way = to_centre * (1.0 / distance);
	const vector2d round{
		centre_way.x * std::cos(turn) - centre_way.y * std::sin(turn),
		centre_way.x * std::sin(turn) + centre_way.y * std::cos(turn)};
	const double ahead =
		std::max(std::sqrt(std::max(distance * distance - reach * reach, 0.0)),
				 kCellSize);
	return from + round * ahead;
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

}  // namespace karakale
