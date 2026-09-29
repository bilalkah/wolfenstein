# Menus and settings

| | |
| --- | --- |
| **When** | 21 to 28 September 2026 |
| **Commits** | `1fd5f0b` Add an in-engine menu system with settings and pause, `7e6900b` Reset weapon previews when their card loses focus, `8d46824` Stop the menu allocating every frame, `113f9ca`/`e0c9c42` difficulty choice, `7524277` Let the player invert the mouse, widen the view, and set music and effects apart, `d41e706` view capped at 80 degrees |
| **Code today** | [UI, menus and HUD](../engine/ui.md), [Settings and saved games](../engine/persistence.md) |

## Problem

The game started with an arrow-key carousel to pick a weapon. It needed
real menus (main, controls, settings, pause, results), the same natively
and in the browser, and settings that persist.

## Constraints

- Drawn with the game's own renderer and fonts (no HTML overlay), so the
  native build gets the same menus.
- Keyboard and mouse both work everywhere.
- Later: nothing may allocate while a menu is shown.

## Approach

- **An immediate-mode toolkit** (`1fd5f0b`): "buttons, sliders, toggles,
  selectable cards) with keyboard focus in draw order, mouse hover and
  click, and a cache of rendered text textures". The game became an
  explicit state machine (menu, playing, paused, result). "Quit is offered
  natively only."
- **Settings persisted**: "saved in localStorage on the web and SDL's
  per-user preferences directory natively".
- **No allocation per frame** (`8d46824`): each label drawn had been
  looked up in a text cache under a `std::format` key, "longer than any
  short-string buffer, so every label drawn allocated"; the cache became
  keyed by style, colour and text through a transparently hashed view, and
  values drawn every frame went into `FixedText` buffers. Later
  (`6ecde29`) the cache gave way to glyphs rasterised at startup.
- **More settings** (`7524277`): inverted mouse, a field of view ("A wider
  view shows everything smaller up and down as it does across, so the
  picture keeps its proportions"), music and effects volumes as shares of
  the master. The field of view was later capped at 80 degrees
  (`d41e706`).
- The weapon-selection screen went away when weapons became things found
  in levels (`0bf0e6e`); a difficulty screen took its place when a new game
  starts (`e0c9c42`).

## C++ techniques used

- [Text without allocating](../techniques/allocation-free-text.md):
  `FixedText` over `std::format_to_n`, `RecordWriter` over `std::to_chars`.
- [Non-owning views](../techniques/views.md): labels and texts as
  `std::string_view`.

## Key code

- [`FixedText`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/UI/include/UI/ui.h#L58-L74)
- [The UI toolkit](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/UI/src/ui.cpp)
- [The menu screens](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/src/renderer_menu.cpp)

## Pitfalls

- A widget's focus index is its position in the frame's calls: skipping a
  widget conditionally shifts the others' indices.
- The glyph cache covers printable ASCII and two symbols; anything else in
  text draws as "?".

- **The field of view is capped at 80 degrees** (it was 100 in
  `7524277`): "wider, the view distorted too much" (`d41e706`). The rays
  were then spread at equal angles, which bent straight walls at wide
  angles; since `dafd4e8` they go through a camera plane and walls stay
  straight (see [Spreading the rays](../engine/raycasting.md#spreading-the-rays)).
  The cap has not been revisited since.

## What I'd change

- Rebindable controls in the settings screen.
- An atlas of glyphs, drawn as one batch per string.
