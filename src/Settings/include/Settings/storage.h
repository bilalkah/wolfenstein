/**
 * @file storage.h
 * @brief Small text records kept between sessions
 */

#ifndef SETTINGS_INCLUDE_SETTINGS_STORAGE_H
#define SETTINGS_INCLUDE_SETTINGS_STORAGE_H

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace karakale {

// Web builds keep each record in localStorage ("karakale.<name>"), native
// builds in a file in SDL's per-user preferences directory ("<name>.txt").
// Storage that cannot be used (blocked site data, no preferences directory)
// reads as empty and ignores writes.
enum class Record : std::uint8_t { Settings, SavedGame };

// The record's text, empty if there is none. Allocates: read at startup.
std::string ReadRecord(Record record);
// Replaces the record. `text` must be NUL-terminated (a stack buffer the
// caller formatted into): writing allocates nothing, so it can happen
// while the game runs.
void WriteRecord(Record record, std::string_view text);
void ClearRecord(Record record);

// Writes "key=value" lines into a caller's buffer, NUL-terminated, without
// allocating: std::format sets up a heap buffer to print a double, while
// std::to_chars does not. A line that does not fit leaves the writer failed.
class RecordWriter
{
  public:
	explicit RecordWriter(std::span<char> out) : out_(out) {
		if (out_.empty()) {
			failed_ = true;
		}
		else {
			out_[0] = '\0';
		}
	}

	template <typename Number>
	RecordWriter& Line(std::string_view key, Number value) {
		Append(key);
		Append("=");
		if (!failed_) {
			// Room for the value and, after the line, the NUL
			char* const end = out_.data() + out_.size() - 1;
			const auto [written, error] =
				std::to_chars(out_.data() + size_, end, value);
			if (error != std::errc{}) {
				failed_ = true;
			}
			else {
				size_ = static_cast<std::size_t>(written - out_.data());
			}
		}
		Append("\n");
		return *this;
	}

	RecordWriter& LineText(std::string_view key, std::string_view value) {
		Append(key);
		Append("=");
		Append(value);
		Append("\n");
		return *this;
	}

	// The lines written, or empty if they did not all fit
	std::string_view Text() const {
		return failed_ ? std::string_view{}
					   : std::string_view(out_.data(), size_);
	}

  private:
	void Append(std::string_view text) {
		if (failed_ || size_ + text.size() + 1 > out_.size()) {
			failed_ = true;
			return;
		}
		text.copy(out_.data() + size_, text.size());
		size_ += text.size();
		out_[size_] = '\0';
	}

	std::span<char> out_;
	std::size_t size_ = 0;
	bool failed_ = false;
};

}  // namespace karakale

#endif	// SETTINGS_INCLUDE_SETTINGS_STORAGE_H
