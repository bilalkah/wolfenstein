# Streaming JSON with SAX

## The technique

A JSON library usually parses a document into a **tree** (a DOM) that the
program then walks: convenient, but every value becomes a node (often an
allocation), and the tree is thrown away once the program has copied what
it needs. A **SAX** (Simple API for XML, borrowed for JSON) parser instead
calls the program back for each event as it reads: an object starts, a
key, a number, a string, an object ends. The program writes each value
straight to where it belongs and builds no tree.

nlohmann/json offers this through `json::sax_parse(input, &handler)` with a
handler derived from `nlohmann::json_sax<json>`, overriding `null`,
`boolean`, `number_integer`, `number_unsigned`, `number_float`, `string`,
`binary`, `start_object`, `key`, `end_object`, `start_array`, `end_array`
and `parse_error`; returning false from any callback stops the parse.

## Where it appears here

Level files are read by `LevelReader`, a SAX handler that fills a
`LevelData` directly:

```cpp title="src/Core/src/level_data.cpp"
// Reads a level file straight into LevelData as the parser walks it (SAX),
// without building a JSON tree: loading a level allocates for the strings and
// spawn lists it keeps, not once per JSON value. Every field is checked when
// its object closes, so a missing or mistyped field is an error that names it.
class LevelReader final : public nlohmann::json_sax<json>
{
  public:
    explicit LevelReader(LevelData& level) : level_(level) {
        // Enough for any level so far; more simply grows the list
        level_.enemies.reserve(32);
        level_.dynamic_objects.reserve(32);
        level_.pickups.reserve(32);
        level_.objectives.reserve(4);
        level_.secrets.reserve(4);
        level_.intel.reserve(4);
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/level_data.cpp#L425-L439){ .excerpt-source }

How it keeps track:

- A fixed **stack of frames** (depth limited) records where in the
  document the parser is: the root, the enemies array, an enemy, its
  position, a pickup, an objective, a secret, a page of intel.
- Each frame remembers the **key** just read (as an enum: `Key::Type`,
  `Key::Position`, ...; unknown keys map to `Key::Other`, whose values are
  skipped, so newer files with extra fields still load).
- Each frame collects a small **bit set of required fields seen**; when
  the object closes, `end_object` checks them and fails with a message
  naming what is missing ("pickup position", "intel title and text").
- A value arriving where it does not belong ("unexpected number") fails
  the parse; the error comes back through `std::expected`.

```cpp title="src/Core/src/level_data.cpp"
std::expected<LevelData, std::string> ParseLevel(std::istream& input) {
    LevelData level;
    LevelReader reader(level);
    if (!json::sax_parse(input, &reader)) {
        return std::unexpected(reader.Error());
    }
    return level;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/level_data.cpp#L959-L966){ .excerpt-source }

The much smaller and more deeply nested `config.json` is still parsed with
the tree API: it is read once, and the tree makes the code for dozens of
fields per enemy and weapon short.

## Pitfalls

- A SAX handler is a hand-written state machine; each new field needs a
  key, a place in `KeyFor`, a case in the value callback and, if required,
  a bit checked at `end_object`. The parser tests
  (`tests/level_data_test.cpp`) cover missing and malformed fields.
- Callbacks receive strings as `string_t&`; moving from them
  (`std::move(value)`) avoids a copy, as the reader does.
