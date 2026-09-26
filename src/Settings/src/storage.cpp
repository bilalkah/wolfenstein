#include "Settings/storage.h"
#include <SDL2/SDL.h>
#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace wolfenstein {

namespace {

constexpr std::array<const char*, 2> kNames = {"settings", "progress"};

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
		value = localStorage.getItem('wolfenstein.' + UTF8ToString(name));
	} catch (e) {
	}
	return value === null ? 0 : stringToNewUTF8(value);
});

EM_JS(void, WriteStoredRecord, (const char* name, const char* text), {
	try {
		localStorage.setItem('wolfenstein.' + UTF8ToString(name),
							 UTF8ToString(text));
	} catch (e) {
	}
});

EM_JS(void, ClearStoredRecord, (const char* name), {
	try {
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

// Worked out once, on first use (when the records load at startup), so
// writing later needs no string building
const std::string& PathOf(Record record) {
	static const std::array<std::string, kNames.size()> paths = [] {
		std::array<std::string, kNames.size()> result;
		char* directory = SDL_GetPrefPath("bilalkah", "wolfenstein");
		if (directory == nullptr) {
			return result;
		}
		for (std::size_t i = 0; i < kNames.size(); ++i) {
			result[i] = std::string(directory) + kNames[i] + ".txt";
		}
		SDL_free(directory);
		return result;
	}();
	return paths[std::to_underlying(record)];
}

}  // namespace

std::string ReadRecord(Record record) {
	std::ifstream file(PathOf(record));
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
	const std::string& path = PathOf(record);
	if (!path.empty()) {
		::unlink(path.c_str());
	}
}
#endif

}  // namespace wolfenstein
