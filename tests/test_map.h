#ifndef TESTS_TEST_MAP_H
#define TESTS_TEST_MAP_H

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace wolfenstein::testing {

// Writes a map in the game's text format to a temporary file and returns its
// path; rows are indexed by x, columns by y
inline std::filesystem::path WriteMapFile(
	std::string_view name, std::initializer_list<const char*> rows) {
	const auto path = std::filesystem::temp_directory_path() / name;
	std::ofstream file(path);
	file << "height " << rows.size() << "\n";
	file << "width " << std::string_view(*rows.begin()).size() << "\n";
	for (const char* row : rows) {
		file << row << "\n";
	}
	return path;
}

// The same, for rows built in code
inline std::filesystem::path WriteMapFile(
	std::string_view name, const std::vector<std::string>& rows) {
	const auto path = std::filesystem::temp_directory_path() / name;
	std::ofstream file(path);
	file << "height " << rows.size() << "\n";
	file << "width " << rows.front().size() << "\n";
	for (const std::string& row : rows) {
		file << row << "\n";
	}
	return path;
}

}  // namespace wolfenstein::testing

#endif	// TESTS_TEST_MAP_H
