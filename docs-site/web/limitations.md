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
| **Deprecated audio node** | SDL2's web audio uses `ScriptProcessorNode` | A console warning; works in current browsers |
| **Keyboard and mouse only** | The game reads no touch or gamepad input | Phones and tablets cannot play |
| **Timers are coarse** | Without cross-origin isolation, `performance.now()` is rounded (the benchmark's own server sets COOP/COEP to get finer timing) | Profiling in a normal browser tab is less precise |

## After a new deployment

GitHub Pages serves every file with `Cache-Control: max-age=600`. The page
downloads `index.wasm` and `index.data` fresh (checked against the server
with a `HEAD` request, and kept in IndexedDB), but `index.html` and
`index.js` may come from the browser's HTTP cache for up to ten minutes
after a deployment. `index.js` holds Emscripten's runtime glue and the
table of files inside `index.data`, so a cached `index.js` from the
previous build with a fresh `index.wasm` and `index.data` from the new one
may fail to start. The page's recovery (clear the kept copies, reload
once) does not help while the browser still holds the old script.

A hard reload (Ctrl+Shift+R, or Cmd+Shift+R) fetches everything afresh;
otherwise the problem clears within ten minutes.

!!! todo "Bilal: decide how to version the web build"
    A lasting fix would make the page load a script that always matches
    the data it downloads: for example, the Pages workflow could add the
    commit's hash to the file names (`index.<sha>.js` and so on) and to the
    page's references, or the page could fetch `index.js` itself with
    `cache: 'no-store'` as it does `index.wasm`. Not done yet: it
    changes the loading path, which deserves a careful test across a real
    deployment.
