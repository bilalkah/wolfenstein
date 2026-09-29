# Storage

The browser build keeps two kinds of data between visits, in two places.

## Settings and the saved game: localStorage, from C++

`src/Settings/src/storage.cpp` reads and writes records through `EM_JS`:
JavaScript function bodies written inside C++ and compiled into the
module, callable from C++ like any `extern "C"` function.

```cpp title="src/Settings/src/storage.cpp"
EM_JS(char*, ReadStoredRecord, (const char* name), {
    let value = null;
    try {
        value = localStorage.getItem('wolfenstein.' + UTF8ToString(name));
    } catch (e) {
    }
    return value === null ? 0 : stringToNewUTF8(value);
});
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Settings/src/storage.cpp#L35-L42){ .excerpt-source }

- `UTF8ToString` reads a C string out of the WebAssembly memory;
  `stringToNewUTF8` allocates a C string in it (with `malloc`) for the
  result, which the C++ side frees.
- Reading happens at startup only (it allocates); writing takes a
  NUL-terminated buffer the caller formatted on the stack, so saving
  during play allocates nothing in C++ (`localStorage.setItem` copies the
  string on the JavaScript side).
- Every access is wrapped in `try`: `localStorage` throws when site data is
  blocked, and the game then plays without settings or saves.
- `EM_JS_DEPS` declares the runtime helpers the functions use, so the
  linker keeps them.

Keys: `wolfenstein.settings` and `wolfenstein.progress`. See
[Settings and saved games](../engine/persistence.md).

## The game's files: IndexedDB, from the page

The page keeps `index.wasm` and `index.data` in an IndexedDB database
(`wolfenstein`, object store `files`), each under its URL with the version
the server reported (size and date, or tag). See
[Downloading and caching](loading-and-caching.md).

## Pitfalls

- **Per origin.** `localStorage` and IndexedDB belong to the page's
  origin. The game on GitHub Pages
  (`https://bilalkah.github.io`) and the same game served on a LAN address
  keep separate settings and saves.
- **Shared origin on GitHub Pages.** Every project site of a GitHub user
  lives under the same origin (`bilalkah.github.io`), so they share
  `localStorage`; the game's keys are prefixed with `wolfenstein.` for
  that reason as much as any.
- **Private windows** may refuse storage or clear it when closed.
