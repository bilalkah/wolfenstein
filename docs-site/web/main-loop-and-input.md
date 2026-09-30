# Main loop, input and pointer lock

## Who owns the loop

A native program runs `while (running) { frame(); }` for as long as it
likes. A browser page cannot: JavaScript (and WebAssembly) must return to
the browser's event loop regularly, or the page freezes. Emscripten's
answer is `emscripten_set_main_loop`: register a function, and the browser
calls it once per animation frame (`requestAnimationFrame`).

```cpp title="src/Core/src/game.cpp"
void Game::Run() {
#ifdef __EMSCRIPTEN__
    // Loaded: the page stops watching for a start that failed (see
    // web/shell.html)
    EM_ASM(if (Module.onGameReady) Module.onGameReady(););
    // The browser drives the loop: one Tick per animation frame
    emscripten_set_main_loop_arg(
        [](void* game) {
            if (!static_cast<Game*>(game)->Tick()) {
                emscripten_cancel_main_loop();
            }
        },
        this, 0, true);
#else
    while (Tick()) {}
#endif
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/src/game.cpp#L547-L563){ .excerpt-source }

- `_arg` passes `this` through a `void*`, so a capture-free lambda works
  as the C callback.
- A frame rate of `0` means "use `requestAnimationFrame`": the game draws
  at the display's rate, and the browser pauses it in a hidden tab. With
  SDL 3 that holds only while the renderer's vsync is on: SDL 3 paces the
  page's loop by it, and runs it on `setTimeout`, as fast as it can go,
  when it is off. `RendererContext` asks for vsync in the browser (see
  [Moving to SDL 3](../features/sdl3.md)).
- `simulate_infinite_loop = true` makes the call not return: Emscripten
  unwinds the stack by throwing, so `main()` never runs past `Game::Run`,
  and objects on `main`'s stack (the `Game`) stay alive while the loop
  runs.
- `Tick()` returning false cancels the loop.

Because the whole game is built around one `Tick()` per frame (the native
loop is the same function in a `while`), nothing else changes. The fixed
time step decouples the simulation from however often the browser calls.
The native build caps frames at 120 Hz by sleeping; the web build must not
sleep (it would block the page), so that code is compiled out.

## Capturing the mouse: pointer lock

Mouse look needs relative motion without a cursor hitting the screen's
edge. Browsers offer the **Pointer Lock API**: `canvas.requestPointerLock()`
hides the cursor and reports raw motion (`movementX`, `movementY`). The
browser grants it **only during a user gesture** (a click), and releases
it when the user presses Esc, without passing the key to the page.

The game asks for it through SDL (`SDL_SetRelativeMouseMode(SDL_TRUE)` when
play starts), and follows the browser's rules:

- Until the lock is granted, the game shows **"Click to play"**; the click
  that takes the lock is not a shot (fire is armed only once the button has
  been up with the mouse captured).
- Losing the lock after having had it **pauses** the game, since that is
  all an Esc press looks like from inside the page
  (`emscripten_get_pointerlock_status` in `Game::CheckGameEvent`).

The page also asks for **raw motion**, free of the operating system's
pointer acceleration, where the browser offers it:

```javascript title="web/shell.html"
  const lockPointer = canvas.requestPointerLock.bind(canvas);
  canvas.requestPointerLock = (options) => {
    let request;
    try {
      request = lockPointer({ ...options, unadjustedMovement: true });
    } catch {
      return lockPointer(options);
    }
    // Browsers that take options answer with a promise: turned down (raw
    // motion not offered here), the plain lock instead, still in the click
    return request?.catch?.(() => lockPointer(options)) ?? request;
  };
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/web/shell.html#L449-L460){ .excerpt-source }

SDL calls `canvas.requestPointerLock()` without options, so the page wraps
the method: it asks for `unadjustedMovement`, and if the browser refuses
(not every browser or platform supports it), falls back to the plain lock
**within the same click**, where it is still allowed.

## The canvas and its scaling

The canvas keeps the game's 4:3 frame (1200 by 900 pixels) and is scaled
by CSS to the largest size that fits the page:
`width: min(100vw, calc(100vh * 4 / 3)); aspect-ratio: 4 / 3`.
Inside an `<iframe>`, `vw` and `vh` are the frame's own viewport, so an
embedded game scales to its frame (letterboxed in black). This site links
to the game's own page instead, where it fills the tab.

SDL's web backend scales pointer-lock motion by the canvas's pixels over
its size on the page, which would make the mouse faster in a smaller
window; `Game::CanvasStretch()` undoes it (see [Input](../engine/input.md)).

## Keys and focus

- Keyboard events go to the focused document. The page focuses the canvas
  on a click (`canvas.addEventListener('mousedown', () => canvas.focus())`);
  in an iframe, clicking the game gives the frame focus.
- SDL's web keyboard handler calls `preventDefault()` on the arrow keys
  and Space, so they **do not scroll** the page around an embedded game. This was checked in headless Chrome with the game in a
  tall page: after clicking into the frame, ArrowDown, Space and ArrowUp
  did not move the page's scroll position, while Page Down and End (keys
  the game does not use) still scrolled the outer page.
- The game binds no Ctrl combinations, so browser shortcuts are left
  alone.
- The canvas swallows the context menu (`oncontextmenu="event.preventDefault()"`).

## Pitfalls

- **Esc cannot be a game key on the web**: the browser takes it to release
  pointer lock. The game treats the lock's loss as Esc.
- **Pointer lock needs a gesture.** A lock requested outside a click is
  refused; after a pause, the player clicks the game to resume capture.
- **A frozen tab is a long frame.** When the tab is hidden, the browser
  stops calling the loop; the fixed step drops the missing time (at most a
  quarter of a second is simulated) instead of fast-forwarding.

## Possible improvements

- A fullscreen key calling `canvas.requestFullscreen()` inside a key
  gesture; for now the game fills the browser tab, not the screen.
