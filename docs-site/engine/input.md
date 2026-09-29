# Input

## Purpose

Input turns what the player does with the keyboard and mouse into what
the simulation understands: one `PlayerCommand` per tick. On the way it
must lose nothing (a click shorter than a frame still fires, mouse motion
between ticks still turns), feel immediate (the view follows the hand at
once, at any frame rate), and behave the same natively and in a browser
(pointer lock, scaled canvases).

Code: `Game::CheckGameEvent` and `Game::SampleCommand` in
`src/Core/src/game.cpp`; `src/Characters/include/Characters/player_command.h`
and `view_angles.h`.

## Concepts

### Events and state

SDL offers input two ways. **Events** (`SDL_PollEvent`) report changes:
a key went down, the wheel turned, the window lost focus. **State**
(`SDL_GetKeyboardState`, `SDL_GetMouseState`, `SDL_GetRelativeMouseState`)
reports what is true now: which keys are held, how far the mouse moved
since the last call. Held movement keys are best read as state; one-off
actions (a weapon key, a wheel step, a click) must come from events, or a
press and release between two frames is lost.

### Commands

A **command** is a small value describing intent for one simulation step:
move forward, strafe, turn, fire, use. Keeping the simulation fed only by
commands decouples it from devices: tests, the benchmark, a replay or a
network peer can produce commands just as well. Quake and most networked
shooters since work this way ("user commands").

## How it is implemented here

### The command

`PlayerCommand` holds movement axes (\(-1\), 0, 1), mouse look (radians,
and a slope for looking up), buttons (fire, reload, use), a weapon slot or
wheel step to switch to, and the view's angles set outright (see below).
It has a defaulted `operator==`, so tests can compare commands directly.

### Sampling a frame

```cpp title="src/Core/src/game.cpp"
PlayerCommand Game::SampleCommand() {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const auto axis = [keys](SDL_Scancode positive, SDL_Scancode negative) {
        return static_cast<std::int8_t>(keys[positive] - keys[negative]);
    };
    PlayerCommand command;
    command.forward = axis(SDL_SCANCODE_W, SDL_SCANCODE_S);
    command.strafe = axis(SDL_SCANCODE_D, SDL_SCANCODE_A);
    command.turn = axis(SDL_SCANCODE_RIGHT, SDL_SCANCODE_LEFT);

    // Relative mouse mode reports motion since the last call, which also
    // works under browser pointer lock (unlike warping the cursor)
    int dx = 0;
    int dy = 0;
    SDL_GetRelativeMouseState(&dx, &dy);
    if (captured_) {
        // SDL's web backend scales the motion by how far the canvas is
        // stretched, which would make the mouse faster in a smaller
        // window: undone here, so a hand's movement turns as far anywhere
        const double stretch = CanvasStretch();
        const MouseLook look = ToMouseLook(dx, dy, Settings::Get(), config_);
        command.look = look.turn * stretch;
        command.look_up = look.up * stretch;
    }

    // Firing waits for the button to come up once the mouse is captured;
    // a click shorter than a frame still fires
    const bool held =
        (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
    fire_armed_ = captured_ && (fire_armed_ || !held);
    command.fire = fire_armed_ && (held || clicked_);
    clicked_ = false;
    command.reload = keys[SDL_SCANCODE_R] != 0;
    command.use = keys[SDL_SCANCODE_E] != 0 || keys[SDL_SCANCODE_SPACE] != 0;
    return command;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/game.cpp#L691-L726){ .excerpt-source }

Scancodes (`SDL_SCANCODE_W`) name physical key positions, so WASD stays
under the same fingers on AZERTY or Dvorak keyboards.

**Arming fire.** The click that captures the mouse (or picks a menu item)
must not also fire. `fire_armed_` becomes true only once the button has
been *up* while the mouse is captured; after that, a held button fires, and
so does a click that went down and up between two frames (`clicked_`,
latched from the event).

### Mouse sensitivity, in pixels on screen

```cpp title="src/Core/src/game.cpp"
MouseLook ToMouseLook(int dx, int dy, const Settings& settings,
                      const GeneralConfig& view) {
    constexpr double kRadiansPerPixel = 0.005;
    const double turn = dx * kRadiansPerPixel * settings.mouse_sensitivity;
    // Looking up slides the view by as many screen pixels as turning the
    // same mouse distance does. A view fov across shows width / fov pixels
    // a radian, and a slope of 1 is height * base_fov / fov pixels up: the
    // fov cancels, so the slope does not depend on it. The mouse pushed
    // away looks up, unless inverted.
    const double up = (settings.invert_mouse_y ? dy : -dy) * kRadiansPerPixel *
                      settings.mouse_sensitivity * view.screen_width /
                      (view.screen_height * view.base_fov);
    return {.turn = turn, .up = up};
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/game.cpp#L675-L688){ .excerpt-source }

A mouse pixel turns 0.005 rad (times the sensitivity setting). Looking up
and down is a *slope* (the view is sheared, see
[The 3D renderer](renderer.md)); the formula makes a vertical mouse
movement slide the picture as many pixels as the same horizontal movement
would, whatever the field of view.

### The view turns at once

```cpp title="src/Characters/include/Characters/view_angles.h"
    PlayerCommand Apply(PlayerCommand command, double seconds) {
        theta_ = SumRadian(
            theta_,
            command.look + command.turn * Player::kKeyboardTurnSpeed * seconds);
        pitch_ = std::clamp(pitch_ + command.look_up, -Player::kMaxPitch,
                            Player::kMaxPitch);
        command.look = 0.0;
        command.look_up = 0.0;
        command.turn = 0;
        command.has_view = true;
        command.view_theta = theta_;
        command.view_pitch = pitch_;
        return command;
    }
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/include/Characters/view_angles.h#L33-L46){ .excerpt-source }

`ViewAngles` belongs to the presentation. Every frame, it adds the
frame's mouse motion and keyboard turning (2.5 rad/s) to the view, which
is drawn from it at once. The command then carries the **resulting
angles**, and the player takes them outright at the next tick. The
alternative, turning the player a tick at a time, drew the view "a tick or
two behind the hand" (`ViewAngles`' comment); a networked game keeps view
angles on the client for the same reason.

### Frames into ticks

```cpp title="src/Characters/include/Characters/player_command.h"
inline PlayerCommand Gather(const PlayerCommand& earlier,
                            const PlayerCommand& later) {
    PlayerCommand gathered = later;
    gathered.look += earlier.look;
    gathered.look_up += earlier.look_up;
    gathered.fire = earlier.fire || later.fire;
    gathered.reload = earlier.reload || later.reload;
    gathered.use = earlier.use || later.use;
    if (later.weapon < 0) {
        gathered.weapon = earlier.weapon;
    }
    if (later.cycle == 0) {
        gathered.cycle = earlier.cycle;
    }
    // The view as it last was
    if (!later.has_view && earlier.has_view) {
        gathered.has_view = true;
        gathered.view_theta = earlier.view_theta;
        gathered.view_pitch = earlier.view_pitch;
    }
    return gathered;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/include/Characters/player_command.h#L54-L75){ .excerpt-source }

On a 240 Hz screen four frames pass between two ticks. `Gather` merges
their commands: mouse motion adds up, a button pressed in any frame
counts, movement is as it last was, a weapon choice waits for the tick.
Before this (`7524277`), a tick took only the last frame's input and "lost
the mouse's motion in the others, turning slower the faster the screen".

### Events that must not be missed

`CheckGameEvent` drains the event queue each frame and latches:

- a left click (`clicked_`), for the fire rule above;
- the wheel (`wheel_`, a step through the weapons carried);
- a number key 1 to 8 (`weapon_key_`);
- Esc (pause), M (the map), P (the 2D view, with `?debug`);
- Enter, Space, E or a click to go on from a results screen, a page of
  story or a briefing (ignored for the first 0.6 s, so a held key does not
  skip the screen);
- losing the window's focus, which pauses.

### Capture: relative mouse mode and pointer lock

Mouse look needs the cursor captured: natively,
`SDL_SetRelativeMouseMode(SDL_TRUE)`; in the browser, SDL asks for
**pointer lock** on the canvas. The browser only grants it on a user
gesture (a click), and releases it on Esc *without passing the key on*, so
on the web losing the lock is what pauses the game. Until the mouse is
captured again, the game shows "Click to play". See
[Main loop, input and pointer lock](../web/main-loop-and-input.md).

### The stretched canvas

SDL's web backend scales relative mouse motion by the ratio of the
canvas's pixels to its size on the page. The page scales the canvas to fit
the window, so without a correction the mouse would turn faster in a
smaller window. `CanvasStretch()` reads the canvas's CSS width
(`emscripten_get_element_css_size`) and undoes the scaling (1 natively).

## Design decisions and trade-offs

- **Commands at the boundary.** Everything device-specific stays in
  `Game`; the simulation sees values.
- **View angles owned by the presentation.** Immediate response, at the
  cost of the simulation taking the view one tick later (which also
  affects shots; see the pitfall below).
- **Keyboard turning in the view, too.** Arrow-key turning is applied per
  frame with the frame's duration, so it is as smooth as the mouse.

## Pitfalls

- **Shots use the previous tick's view.** `Player::Update` fires before it
  takes the command's view angles; see
  [One frame](../architecture/frame-lifecycle.md#pitfalls-in-this-order).
- **Input while scripted.** The benchmark and soak runs ignore devices
  entirely (`IsScripted()`), so a run cannot depend on a stray key.
- **Focus in an iframe.** Embedded in a page, the game gets keys only once
  its frame has focus (after a click). Arrow keys and Space do not scroll
  the page round it (SDL prevents their default action); keys the game does
  not use, such as Page Down, still do.

## Possible improvements

- Rebindable keys, stored with the other settings.
- Gamepad support through `SDL_GameController`; commands already abstract
  the device.
