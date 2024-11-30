#include "NavigationManager/navigation_manager.h"
#include "Characters/enemy.h"
#include "Map/map.h"
#include "Math/vector.h"
#include "NavigationManager/navigation_helper.h"
#include "common_planning.h"
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

void NavigationManager::InitManager(
	std::shared_ptr<Map> map, std::vector<std::shared_ptr<Enemy>> enemies) {
	map_ = map;
	path_planner_ = std::make_shared<planning::grid_base::AStar>(0.6, 4);
	enemies_ = enemies;
}

//@Note apply caching mechanism later
vector2d NavigationManager::FindPath(Position2D start, Position2D end,
									 std::string id) {
	const auto res = map_->GetResolution();
	if (start.pose.Distance(end.pose) < (res * 0.9)) {
		paths_[id] = {start.pose};
		return start.pose;
	}

	planning::Node start_node = FromVector2d(start.pose / res);
	planning::Node end_node = FromVector2d(end.pose / res);

	std::shared_ptr path_map =
		std::make_shared<planning::Map>(*(map_->GetPathFinderMap()));
	ApplyDynamicObjects(path_map);
	path_map->SetNodeState(start_node, planning::NodeState::kStart);
	planning::Path path =
		path_planner_->FindPath(start_node, end_node, path_map);

	if (path.empty()) {
		paths_[id] = {start.pose};
		return start.pose;
	}
	std::vector<vector2d> path_vector;
	path.erase(path.begin());
	for (auto node : path) {
		path_vector.push_back(FromNode(node) * res + vector2d{0.25, 0.25});
	}
	paths_[id] = path_vector;

	if (path_vector.empty()) {
		return start.pose;
	}
	path_vector.erase(path_vector.begin());
	auto next = path_vector.front();

	return next;
}

vector2d NavigationManager::FindPathToPlayer(Position2D start, std::string id) {
	return FindPath(start, player_position_, id);
}

std::vector<vector2d> NavigationManager::GetPath(std::string id) {
	return paths_[id];
}

void NavigationManager::ResetPath(std::string id) {
	paths_[id].clear();
}

void NavigationManager::SubscribePlayerPosition(const Position2D& position) {
	player_position_ = position;
}

double NavigationManager::EuclideanDistanceToPlayer(
	const Position2D& position) {
	return player_position_.pose.Distance(position.pose);
}

double NavigationManager::ManhattanDistanceToPlayer(
	const Position2D& position) {
	return player_position_.pose.MDistance(position.pose);
}

void NavigationManager::ApplyDynamicObjects(
	std::shared_ptr<planning::Map> path_map) {
	const auto res = map_->GetResolution();

	for (const auto& e : enemies_) {
		path_map->SetNodeState(FromVector2d(e->GetPose() / res),
							   planning::NodeState::kOccupied);
		const auto found = paths_.find(e->GetId());
		if (found != paths_.end()) {
			if (found->second.size() > 0) {
				path_map->SetNodeState(
					FromVector2d(found->second.front() / res),
					planning::NodeState::kOccupied);
			}
		}
	}
}

}  // namespace wolfenstein