# Playing in the browser

| | |
| --- | --- |
| **When** | 21 to 29 September 2026 |
| **Commits** | `903f2a3` integrate wasm to play in browser, `e8894ae` Load the web build in parallel chunks with a progress bar, `711f255` Keep the downloaded game files in the browser between visits, `d8acce1` Heal a broken download instead of showing a black screen, `de89d1c` Resume stalled downloads instead of starting chunks over, `de89d1c` Download in resumable pieces, and reset connections that stop, `73aaf65` Load the game from a server that compresses it (GitHub Pages) |
| **Code today** | [The web build](../web/index.md) |

## Problem

Make the desktop game playable in a browser with no install, fast to load,
robust on flaky networks (it was played over Wi-Fi from a LAN server), and
hostable as static files.

## Constraints

- One code base: no web-only fork of the game.
- No threads (GitHub Pages cannot send the headers `SharedArrayBuffer`
  needs).
- About 18 MB of game to download.

## Approach

1. **The port** (`903f2a3`): Emscripten, SDL2 ports, the assets preloaded
   into `index.data`, `emscripten_set_main_loop_arg` driving `Tick`.
2. **Parallel chunks and a progress bar** (`e8894ae`): "On some networks
   one connection stalls while others run at full speed, and a single
   stalled download froze loading for minutes."
3. **Keeping the files** (`711f255`): IndexedDB, "the Cache API needs
   https, and the game is also played from http:// LAN addresses"; "A
   reload loads in under a second".
4. **Healing** (`d8acce1`): "A download that straddled a rebuild could be
   kept under the new build's version with some chunks of the old, and the
   game then quit silently at startup, every visit." Chunks are checked to
   come from one build; a game that does not start clears the copies and
   reloads once.
5. **Resuming** (`de89d1c`, `de89d1c`): stalled pieces continue from the
   byte they reached on a new connection; "Through a test server freezing
   60% of transfers at 150 KB/s a connection, the game loads in 84 s where
   it used to fail."
6. **Compressing servers** (`73aaf65`, while writing these docs): GitHub
   Pages gzips `.wasm` and `.data`, which broke the byte-range download;
   compressed files now come whole.

## C++ techniques used

- `EM_JS` and `EM_ASM` for the few places C++ talks to JavaScript
  (storage, the ready signal).
- `#ifdef __EMSCRIPTEN__` in five places only (see
  [Platform layer](../engine/platform.md#what-differs-on-the-web)).
- [Replacing the allocator](../techniques/allocation-counting.md) at the
  `malloc` level to count the browser build's allocations.

## Key code

- [`web/shell.html`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/web/shell.html):
  the page, its downloader and its hooks.
- [`Game::Run`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/game.cpp#L547-L563)

## Pitfalls

- **Servers differ**: ranges, compression, validators and caching headers
  each change what the loader must do (see
  [Downloading and caching](../web/loading-and-caching.md)).
- **Stale scripts after a deployment** on hosts with long cache lifetimes
  (see [Limitations](../web/limitations.md#after-a-new-deployment)).
- **Pointer lock and Esc**: the browser owns Esc; losing the lock is the
  pause.

## What I'd change

- Version the build's file names (or query strings) per deployment, so a
  cached page can never meet a new wasm.
- Split `index.data` so the first level can start before every asset has
  arrived.
