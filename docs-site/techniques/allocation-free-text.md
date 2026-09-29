# Text without allocating

## The technique

`std::string`, `std::to_string` and `std::format` allocate whenever their
result does not fit the small-string buffer, and "does not fit" depends on
the platform (libc++ keeps 22 characters inline on 64-bit targets, 10 on
wasm32). Two standard facilities write text into **caller-provided
memory** instead:

- `std::format_to_n(out, n, fmt, args...)` (C++20) formats into an output
  iterator, writing at most `n` characters, and returns how many the full
  output would have had;
- `std::to_chars(first, last, value)` (C++17) converts a number into a
  character range with no locale, no allocation and no exceptions,
  returning where it stopped and an error code.

## Where it appears here

### `FixedText`: formatted text on the stack

```cpp title="src/UI/include/UI/ui.h"
template <std::size_t Capacity = 32>
class FixedText
{
  public:
    template <typename... Args>
    explicit FixedText(std::format_string<Args...> format, Args&&... args) {
        const auto result = std::format_to_n(buffer_.data(), Capacity, format,
                                             std::forward<Args>(args)...);
        size_ = std::min(static_cast<std::size_t>(result.size), Capacity);
    }
    std::string_view View() const { return {buffer_.data(), size_}; }
    operator std::string_view() const { return View(); }

  private:
    std::array<char, Capacity> buffer_{};
    std::size_t size_ = 0;
};
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/UI/include/UI/ui.h#L58-L74){ .excerpt-source }

Every number the menus and HUD draw each frame goes through it: the kill
counter, slider values ("0.75x"), the results screen, the description of
the saved game ("LEVEL 5 · THE CATACOMBS · NORMAL", into a
`FixedText<96>`). Output longer than the capacity is cut, never
reallocated. The implicit conversion to `std::string_view` lets it be
passed straight to `Ui::Text`.

### `RecordWriter`: saving without allocating

```cpp title="src/Settings/include/Settings/storage.h"
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
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Settings/include/Settings/storage.h#L47-L65){ .excerpt-source }

Saved games and settings are `key=value` lines written into a
`std::span<char>` the caller owns (a 1 KB array on the stack when saving a
game). The class comment explains the choice of `to_chars`: "std::format
sets up a heap buffer to print a double, while std::to_chars does not." A
line that does not fit makes the writer fail as a whole (`Text()` returns
empty) rather than save a truncated record. Calls chain:
`writer.Line("level", level).Line("weapon", weapon)...`.

`std::to_chars` for `double` (in libc++ since LLVM 14) prints the shortest
representation that reads back to the same value, so a saved position
round-trips exactly.

### Words and lines without copies

`Menu::DrawWrapped` breaks long text into lines between words as
`std::string_view`s into the original text: measuring and drawing never
copy the characters.

## Pitfalls

- `std::format_to_n` still *evaluates* the whole format (it counts what it
  would have written), so formatting a huge value into a small buffer
  costs the full work; the texts here are short.
- A `FixedText` is a temporary buffer: a `string_view` of it must not
  outlive it (take the view within the same statement or scope).
