#include "NavigationManager/grid_path_finder.h"
#include "Profiler/profiler.h"
#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

namespace wolfenstein {
namespace {

// Builds a finder from rows of '.' (free) and '#' (wall)
GridPathFinder MakeFinder(std::initializer_list<std::string_view> rows) {
	const int height = static_cast<int>(rows.size());
	const int width = static_cast<int>(rows.begin()->size());
	std::vector<std::uint8_t> walls;
	for (const auto row : rows) {
		for (const char c : row) {
			walls.push_back(c == '#' ? 1 : 0);
		}
	}
	GridPathFinder finder(0.6);
	finder.SetGrid(height, width, walls);
	return finder;
}

// A path must start and end at the given cells and move one free cell at a
// time
void ExpectValidPath(const std::pmr::vector<GridCell>& path, GridCell start,
					 GridCell goal,
					 std::initializer_list<std::string_view> rows) {
	ASSERT_FALSE(path.empty());
	EXPECT_EQ(path.front(), start);
	EXPECT_EQ(path.back(), goal);
	const std::vector<std::string_view> grid(rows);
	for (std::size_t i = 0; i < path.size(); ++i) {
		const GridCell cell = path[i];
		EXPECT_NE(grid[static_cast<std::size_t>(cell.x)]
					  [static_cast<std::size_t>(cell.y)],
				  '#');
		if (i > 0) {
			const GridCell prev = path[i - 1];
			EXPECT_EQ(std::abs(cell.x - prev.x) + std::abs(cell.y - prev.y), 1);
		}
	}
}

TEST(GridPathFinder, FindsTheStraightPath) {
	auto finder = MakeFinder({".....", ".....", "....."});
	std::pmr::vector<GridCell> path;
	ASSERT_TRUE(finder.FindPath({1, 0}, {1, 4}, {}, path));
	ExpectValidPath(path, {1, 0}, {1, 4}, {".....", ".....", "....."});
	EXPECT_EQ(path.size(), 5u);
}

TEST(GridPathFinder, GoesAroundWalls) {
	const std::initializer_list<std::string_view> rows = {".#...", ".#.#.",
														  "...#."};
	auto finder = MakeFinder(rows);
	std::pmr::vector<GridCell> path;
	ASSERT_TRUE(finder.FindPath({0, 0}, {0, 4}, {}, path));
	ExpectValidPath(path, {0, 0}, {0, 4}, rows);
	EXPECT_EQ(path.size(),
			  9u);	// the only route: down 2, across 2, up 2, across 2
}

TEST(GridPathFinder, ReportsAnUnreachableGoal) {
	auto finder = MakeFinder({"..#..", "..#..", "..#.."});
	std::pmr::vector<GridCell> path{{9, 9}};
	EXPECT_FALSE(finder.FindPath({1, 0}, {1, 4}, {}, path));
	EXPECT_TRUE(path.empty());
}

TEST(GridPathFinder, ExtraBlockedCellsLastOneQuery) {
	const std::initializer_list<std::string_view> rows = {"...", "...", "..."};
	auto finder = MakeFinder(rows);
	std::pmr::vector<GridCell> path;
	const std::vector<GridCell> blocked = {{1, 1}};

	ASSERT_TRUE(finder.FindPath({1, 0}, {1, 2}, blocked, path));
	ExpectValidPath(path, {1, 0}, {1, 2}, rows);
	EXPECT_EQ(path.size(), 5u);	 // detour around the blocked centre

	ASSERT_TRUE(finder.FindPath({1, 0}, {1, 2}, {}, path));
	EXPECT_EQ(path.size(), 3u);	 // the centre is free again
}

// A crowded way (one others are taking) costs more for one query: taken
// only when the way round is much longer
TEST(GridPathFinder, CrowdedCellsCostMoreForOneQuery) {
	// Two ways from the top left to the bottom left: down the left side (4
	// steps), or round by the right (8 steps)
	const std::initializer_list<std::string_view> rows = {"...", ".#.", ".#.",
														  ".#.", "..."};
	auto finder = MakeFinder(rows);
	std::pmr::vector<GridCell> path;
	const std::vector<GridCell> crowded = {{1, 0}, {2, 0}, {3, 0}};

	ASSERT_TRUE(finder.FindPath({0, 0}, {4, 0}, {}, path, crowded, 3.0f));
	ExpectValidPath(path, {0, 0}, {4, 0}, rows);
	EXPECT_EQ(path.size(), 9u)
		<< "round by the right: 4 steps longer beats 9 dearer";

	ASSERT_TRUE(finder.FindPath({0, 0}, {4, 0}, {}, path, crowded, 1.0f));
	EXPECT_EQ(path.size(), 5u) << "3 more is cheaper than 4 more";

	ASSERT_TRUE(finder.FindPath({0, 0}, {4, 0}, {}, path));
	EXPECT_EQ(path.size(), 5u) << "not crowded any more";
}

TEST(GridPathFinder, StartAndGoalAreAlwaysPassable) {
	auto finder = MakeFinder({"#.#"});
	std::pmr::vector<GridCell> path;
	ASSERT_TRUE(finder.FindPath({0, 0}, {0, 2}, {}, path));
	EXPECT_EQ(path.size(), 3u);
}

TEST(GridPathFinder, HandlesTrivialAndOutOfBoundsQueries) {
	auto finder = MakeFinder({"...", "..."});
	std::pmr::vector<GridCell> path;
	ASSERT_TRUE(finder.FindPath({1, 1}, {1, 1}, {}, path));
	EXPECT_EQ(path.size(), 1u);
	EXPECT_FALSE(finder.FindPath({0, 0}, {5, 0}, {}, path));
	EXPECT_FALSE(finder.FindPath({-1, 0}, {1, 1}, {}, path));
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
// The point of the design: after the first query has sized the output
// buffer, queries allocate nothing
TEST(GridPathFinder, QueriesDoNotAllocate) {
	auto finder = MakeFinder(
		{"..........", ".########.", "..........", ".########.", ".........."});
	std::pmr::vector<GridCell> path;
	const std::vector<GridCell> blocked = {{2, 5}};
	ASSERT_TRUE(finder.FindPath({0, 0}, {4, 0}, blocked, path));  // warm-up

	const auto before = AllocationStats::count;
	for (int i = 0; i < 100; ++i) {
		ASSERT_TRUE(finder.FindPath({0, 0}, {4, 0}, blocked, path));
		ASSERT_TRUE(finder.FindPath({4, 9}, {0, 9}, {}, path));
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

}  // namespace
}  // namespace wolfenstein
