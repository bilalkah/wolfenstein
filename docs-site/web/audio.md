# Audio in the browser

## How SDL plays sound on the web

SDL2's Emscripten backend opens its audio device on the **Web Audio API**:
it creates an `AudioContext` and drives the mixer from a
`ScriptProcessorNode`, whose callback runs **on the main thread** and asks
SDL (and so SDL_mixer, and so the game's `SpatialMixer`) for the next
buffer of samples. The game asks for 44.1 kHz stereo 16-bit with 2048-frame
buffers (`Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)`); a browser may
run its audio context at another rate (48 kHz is common), and SDL converts.

The browser console notes, when the game starts, that `ScriptProcessorNode`
is deprecated in favour of `AudioWorkletNode`. That is SDL2's choice of
API, not the game's; it still works in every current browser.

## Unlocking audio

Browsers start an `AudioContext` **suspended** until the page receives a
user gesture (their autoplay policy). The page resumes SDL's context on
the first key press or click:

```javascript title="web/shell.html"
  function resumeAudio() {
    const ctx = Module.SDL2 && Module.SDL2.audioContext;
    if (ctx && ctx.state === 'suspended') ctx.resume();
  }
  for (const type of ['keydown', 'mousedown']) {
    document.addEventListener(type, resumeAudio);
  }
  canvas.addEventListener('mousedown', () => canvas.focus());
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/web/shell.html#L437-L444){ .excerpt-source }

The menu is navigated with the keyboard or the mouse, so the first action
in the menu unlocks the sound; the menu's music starts then. In an iframe,
the gesture must reach the frame (click into the game).

## Positional sound

The spatial mixer works the same in the browser: SDL_mixer calls its
post-mix hook on the audio callback, which, on the web, is the main
thread; the lock-free command ring between the game and the mixer costs
nothing there, and would be ready for a threaded audio backend. See
[Audio](../engine/audio.md).

## Pitfalls

- **No sound until a gesture.** A page that plays before any click is
  silent; that is by design of the browsers.
- **Main-thread audio.** A long frame on the main thread (a slow level
  load, a big garbage collection in the page) delays the audio callback
  and can cause a click in the sound. The game's frames are short and
  allocate nothing, which helps.
- **Music is MP3,** decoded by SDL_mixer's port (`-sSDL2_MIXER_FORMATS=mp3`).

## Possible improvements

- An SDL version (or a custom backend) on `AudioWorklet`, taking audio off
  the main thread; with no `SharedArrayBuffer` on GitHub Pages, it would
  have to communicate by message passing.
