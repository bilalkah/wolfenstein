# Limitations

What the browser build cannot do, or does differently, and why.

| Limitation | Why | Effect |
| --- | --- | --- |
| **Single-threaded** | WebAssembly threads need `SharedArrayBuffer`, which browsers enable only for cross-origin isolated pages (COOP and COEP headers); GitHub Pages cannot send them | The game has no threads of its own anyway; audio runs on the main thread |
| **Esc is not a game key** | The browser takes Esc to release pointer lock and does not pass it on | Losing the lock pauses the game |
| **The mouse needs a click** | Pointer lock is granted only during a user gesture | "Click to play" after a pause, and when the game starts |
| **Sound needs a gesture** | Browsers' autoplay policy suspends audio until the user interacts | The page resumes audio on the first key or click |
| **About 15 MB to download** (as sent compressed) | Every asset is packed into `index.data` and loaded before the game starts | The first visit waits; later visits use the copy kept in IndexedDB |
| **No Quit button** | A page cannot close its own tab | The main menu omits it on the web |
| **Deprecated audio node** | SDL 3's web audio still uses `ScriptProcessorNode` | A console warning; works in current browsers |
| **Keyboard and mouse only** | The game reads no touch or gamepad input | Phones and tablets cannot play |
| **Timers are coarse** | Without cross-origin isolation, `performance.now()` is rounded (the benchmark's own server sets COOP/COEP to get finer timing) | Profiling in a normal browser tab is less precise |

## After a new deployment

GitHub Pages serves every file with `Cache-Control: max-age=600`, so a
returning player's browser may reuse any file it has for up to ten minutes
after a deployment. The game's three files are safe from that:
`index.wasm` and `index.data` are checked against the server on every
visit, and `index.js` is loaded under a name that carries its version (see
[Downloading and caching](loading-and-caching.md#indexjs-by-its-version)),
so all three always come from the same build. Only `index.html` may be up
to ten minutes old; it holds the loader alone, which works with either
build.
