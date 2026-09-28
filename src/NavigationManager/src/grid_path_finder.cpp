#include "NavigationManager/grid_path_finder.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace wolfenstein {

namespace {

constexpr std::array<GridCell, 4> kSteps = {{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}};

// Orders the heap so the entry with the lowest f is on top
constexpr auto kHigherF = [](const auto& lhs, const auto& rhs) {
	return lhs.f > rhs.f;
};

float Heuristic(GridCell from, GridCell to) {
	return static_cast<float>(std::hypot(from.x - to.x, from.y - to.y));
}

}  // namespace

GridPathFinder::GridPathFinder(double heuristic_weight,
							   std::pmr::memory_resource* memory)
	: heuristic_weight_(heuristic_weight),
	  walls_(memory),
	  extra_cost_(memory),
	  blocked_stamp_(memory),
	  seen_stamp_(memory),
	  closed_stamp_(memory),
	  g_(memory),
	  parent_(memory),
	  open_(memory) {}

std::size_t GridPathFinder::MemoryFor(int height, int width) {
	const auto cells =
		static_cast<std::size_t>(height) * static_cast<std::size_t>(width);
	constexpr std::size_t kArrays = 8;
	constexpr std::size_t kPadding = alignof(std::max_align_t);
	return cells * (2 * sizeof(std::uint8_t) + 3 * sizeof(std::uint32_t) +
					sizeof(float) + sizeof(std::int32_t)) +
		   (cells * kSteps.size() + 1) * sizeof(OpenEntry) + kArrays * kPadding;
}

void GridPathFinder::SetGrid(int height, int width,
							 std::span<const std::uint8_t> blocked) {
	SetGrid(height, width, [&](int x, int y) {
		// Widened before multiplying: x * width can overflow an int
		const auto index =
			static_cast<std::size_t>(x) * static_cast<std::size_t>(width) +
			static_cast<std::size_t>(y);
		return index < blocked.size() && blocked[index] != 0;
	});
}

void GridPathFinder::SetExtraCost(GridCell cell, std::uint8_t extra) {
	if (Contains(cell)) {
		extra_cost_[static_cast<std::size_t>(Index(cell))] = extra;
	}
}

std::size_t GridPathFinder::FreeCells() const {
	return static_cast<std::size_t>(std::ranges::count(walls_, 0));
}

void GridPathFinder::Resize(int height, int width) {
	height_ = height;
	width_ = width;
	const auto cells =
		static_cast<std::size_t>(height) * static_cast<std::size_t>(width);
	walls_.assign(cells, 0);
	extra_cost_.assign(cells, 0);
	blocked_stamp_.assign(cells, 0);
	seen_stamp_.assign(cells, 0);
	closed_stamp_.assign(cells, 0);
	g_.assign(cells, 0.0f);
	parent_.assign(cells, -1);
	// Every push relaxes an edge into a not yet expanded cell, and each cell
	// is expanded once, so a query pushes at most (4 edges per cell) + the
	// start: the open list never grows during a query
	open_.reserve(cells * kSteps.size() + 1);
	generation_ = 0;
}

bool GridPathFinder::Contains(GridCell cell) const {
	return cell.x >= 0 && cell.x < height_ && cell.y >= 0 && cell.y < width_;
}

void GridPathFinder::NextGeneration() {
	if (++generation_ == 0) {
		// Wrapped around after 2^32 queries: stale stamps could now collide
		std::ranges::fill(blocked_stamp_, 0);
		std::ranges::fill(seen_stamp_, 0);
		std::ranges::fill(closed_stamp_, 0);
		generation_ = 1;
	}
}

bool GridPathFinder::FindPath(GridCell start, GridCell goal,
							  std::span<const GridCell> extra_blocked,
							  std::pmr::vector<GridCell>& path) {
	path.clear();
	if (!Contains(start) || !Contains(goal)) {
		return false;
	}
	NextGeneration();
	for (const GridCell cell : extra_blocked) {
		if (Contains(cell)) {
			blocked_stamp_[static_cast<std::size_t>(Index(cell))] = generation_;
		}
	}

	const auto weight = static_cast<float>(heuristic_weight_);
	const auto cost = [weight](float g, float h) {
		return (1.0f - weight) * g + weight * h;
	};
	const std::int32_t start_index = Index(start);
	const std::int32_t goal_index = Index(goal);
	const auto passable = [&](std::int32_t index) {
		const auto i = static_cast<std::size_t>(index);
		if (index == start_index || index == goal_index) {
			return true;
		}
		return walls_[i] == 0 && blocked_stamp_[i] != generation_;
	};

	open_.clear();
	const auto start_i = static_cast<std::size_t>(start_index);
	seen_stamp_[start_i] = generation_;
	g_[start_i] = 0.0f;
	parent_[start_i] = -1;
	open_.push_back({cost(0.0f, Heuristic(start, goal)), start_index});

	bool found = false;
	while (!open_.empty()) {
		std::ranges::pop_heap(open_, kHigherF);
		const std::int32_t current = open_.back().index;
		open_.pop_back();
		const auto current_i = static_cast<std::size_t>(current);
		if (closed_stamp_[current_i] == generation_) {
			continue;  // an outdated entry for an already expanded cell
		}
		closed_stamp_[current_i] = generation_;
		if (current == goal_index) {
			found = true;
			break;
		}

		const GridCell cell = Cell(current);
		for (const GridCell step : kSteps) {
			const GridCell neighbour{cell.x + step.x, cell.y + step.y};
			if (!Contains(neighbour)) {
				continue;
			}
			const std::int32_t index = Index(neighbour);
			const auto i = static_cast<std::size_t>(index);
			if (!passable(index) || closed_stamp_[i] == generation_) {
				continue;
			}
			const float next_g =
				g_[current_i] + 1.0f + static_cast<float>(extra_cost_[i]);
			if (seen_stamp_[i] == generation_ && g_[i] <= next_g) {
				continue;  // already reached at least as cheaply
			}
			seen_stamp_[i] = generation_;
			g_[i] = next_g;
			parent_[i] = current;
			open_.push_back({cost(next_g, Heuristic(neighbour, goal)), index});
			std::ranges::push_heap(open_, kHigherF);
		}
	}
	if (!found) {
		return false;
	}

	for (std::int32_t index = goal_index; index != -1;
		 index = parent_[static_cast<std::size_t>(index)]) {
		path.push_back(Cell(index));
	}
	std::ranges::reverse(path);
	return true;
}

}  // namespace wolfenstein
