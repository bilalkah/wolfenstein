#include "Settings/storage.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace karakale {

namespace {

constexpr std::array<const char*, 2> kNames = {"settings", "progress"};
// Records kept under the game's earlier name are read until it writes its
// own, so a player's settings and saved game come along

}  // namespace

#ifdef __EMSCRIPTEN__
namespace {

const char* NameOf(Record record) {
	return kNames[std::to_underlying(record)];
}

EM_JS_DEPS(record_storage, "$stringToNewUTF8,$UTF8ToString");

// The bodies are JavaScript, so clang-format must not touch them
// clang-format off
EM_JS(char*, ReadStoredRecord, (const char* name), {
	let value = null;
	try {
		value = localStorage.getItem('karakale.' + UTF8ToString(name));
		if (value === null) {
			value = localStorage.getItem('wolfenstein.' + UTF8ToString(name));
		}
	} catch (e) {
	}
	return value === null ? 0 : stringToNewUTF8(value);
});

EM_JS(void, WriteStoredRecord, (const char* name, const char* text), {
	try {
		localStorage.setItem('karakale.' + UTF8ToString(name),
							 UTF8ToString(text));
		localStorage.removeItem('wolfenstein.' + UTF8ToString(name));
	} catch (e) {
	}
});

EM_JS(void, ClearStoredRecord, (const char* name), {
	try {
		localStorage.removeItem('karakale.' + UTF8ToString(name));
		localStorage.removeItem('wolfenstein.' + UTF8ToString(name));
	} catch (e) {
	}
});
// clang-format on

}  // namespace

std::string ReadRecord(Record record) {
	char* text = ReadStoredRecord(NameOf(record));
	if (text == nullptr) {
		return {};
	}
	std::string result(text);
	std::free(text);
	return result;
}

void WriteRecord(Record record, std::string_view text) {
	WriteStoredRecord(NameOf(record), text.data());
}

void ClearRecord(Record record) {
	ClearStoredRecord(NameOf(record));
}
#else
namespace {

constexpr std::string_view kApp = "karakale";
constexpr std::string_view kEarlierApp = "wolfenstein";

// Each record's file, and where the earlier name kept it: worked out once,
// on first use (when the records load at startup), so writing later needs
// no string building
struct Paths
{
	std::array<std::string, kNames.size()> current;
	std::array<std::string, kNames.size()> earlier;
};
const Paths& PathsOf() {
	static const Paths paths = [] {
		Paths result;
		char* made = SDL_GetPrefPath("bilalkah", std::string(kApp).c_str());
		if (made == nullptr) {
			return result;
		}
		// ".../bilalkah/karakale/": the earlier one beside it
		const std::string directory(made);
		SDL_free(made);
		const std::string parent =
			directory.substr(0, directory.size() - kApp.size() - 1);
		const std::string earlier =
			parent + std::string(kEarlierApp) + directory.back();
		for (std::size_t i = 0; i < kNames.size(); ++i) {
			result.current[i] = directory + kNames[i] + ".txt";
			result.earlier[i] = earlier + kNames[i] + ".txt";
		}
		return result;
	}();
	return paths;
}
const std::string& PathOf(Record record) {
	return PathsOf().current[std::to_underlying(record)];
}
const std::string& EarlierPathOf(Record record) {
	return PathsOf().earlier[std::to_underlying(record)];
}

}  // namespace

std::string ReadRecord(Record record) {
	std::ifstream file(PathOf(record));
	if (!file) {
		file.open(EarlierPathOf(record));
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

// POSIX write rather than a file stream: nothing here may allocate
void WriteRecord(Record record, std::string_view text) {
	const std::string& path = PathOf(record);
	if (path.empty()) {
		return;
	}
	const int file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (file < 0) {
		return;
	}
	(void)::write(file, text.data(), text.size());
	::close(file);
}

void ClearRecord(Record record) {
	for (const std::string* path : {&PathOf(record), &EarlierPathOf(record)}) {
		if (!path->empty()) {
			::unlink(path->c_str());
		}
	}
}
#endif

}  // namespace karakale
