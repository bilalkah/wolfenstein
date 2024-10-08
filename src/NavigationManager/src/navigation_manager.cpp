#include "NavigationManager/navigation_manager.h"
#include "Math/vector.h"
#include "NavigationManager/navigation_helper.h"
#include <vector>

namespace wolfenstein {

NavigationManager* NavigationManager::instance_ = nullptr;

NavigationManager& NavigationManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new NavigationManager();
	}
	return *instance_;
}

void NavigationManager::InitManager(std::shared_ptr<Map> map) {
	map_ = map;
	path_planner_ = std::make_shared<planning::grid_base::AStar>(0.6, 4);
}

//@Note apply caching mechanism later
vector2d NavigationManager::FindPath(Position2D start, Position2D end,
									 std::string id) {
	if (start.pose.Distance(end.pose) < 0.1) {
		return start.pose;
	}
	if (start.pose.Distance(end.pose) > 5) {
		paths_[id] = {start.pose};
		return start.pose;
	}
	const auto res = map_->GetResolution();

	planning::Node start_node = FromVector2d(start.pose / res);
	planning::Node end_node = FromVector2d(end.pose / res);
	planning::Path path =
		path_planner_->FindPath(start_node, end_node, map_->GetPathFinderMap());
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

std::vector<vector2d> NavigationManager::GetPath(std::string id) {
	return paths_[id];
}

}  // namespace wolfenstein