# UI, menus and HUD

## Purpose

Everything drawn over or instead of the 3D view: the main menu, choosing a
difficulty, controls, settings, pause, the results of a level, pages of
story, briefings, the kill counter, weapon slots, notices, the current
objective, a page of intel being read. All of it is drawn with SDL's
renderer and TrueType fonts, and none of it may allocate while the game
runs.

Code: `src/UI/` (the toolkit), `src/Graphics/src/renderer_menu.cpp` (the
screens), `src/Graphics/src/renderer_result.cpp`.

<figure markdown="span">
  ![The main menu](../assets/screenshots/menu.png){ width="480" }
  <figcaption>The main menu, drawn with the immediate-mode toolkit.</figcaption>
</figure>

## Concepts

### Immediate-mode UI

In a **retained-mode** UI, you build a tree of widget objects once and the
toolkit keeps them, calling back when something happens. In an
**immediate-mode** UI (the term is Casey Muratori's; Dear ImGui is the
best-known library), you call a function per widget **every frame**, and
the function both draws the widget and returns whether it was activated:

```cpp
if (ui.Button("PLAY", rect)) { /* start a game */ }
```

There is no widget state to keep in sync with the game's; the UI is a
function of the game state, redrawn each frame. Focus for keyboard
navigation is just "the n-th focusable widget called this frame".

### Text without per-frame work

Rendering a string with a TrueType library each frame means rasterising
glyphs into a new surface and uploading a texture: slow, and it
allocates. The usual answer is a **glyph cache**: rasterise each character
once, keep a texture per glyph, and draw strings by copying glyph textures
side by side, tinted to the colour wanted.

## How it is implemented here

### The toolkit

`ui::Ui` offers `Text`, `MeasureText`, `FillRect`, `DrawRect`, `Button`,
`Slider`, `Toggle` and `Selectable`. Screens call them every frame between
`BeginFrame(input)` and `EndFrame()`. Each focusable widget registers
itself (`NextWidget`) and gets the next index; the arrow keys (or W and S)
move focus through them in call order, wrapping round; hovering with the
mouse moves focus too; Enter, Space or a click activates. `Input` collects
the frame's SDL events into a few booleans (previous, next, left, right,
activate, back).

### Glyphs, rasterised once

At startup, for each of five font styles (title, heading and button in the
display font, Black Ops One; body and small text in Roboto), `Ui`
rasterises printable ASCII plus the two symbols the screens use ("·" and
"°") in white, keeps a texture per glyph, and records its advance. Drawing
text is then a `SDL_RenderCopy` per character with the colour set as a
texture colour modulation: any text, any colour, no allocation. A
character outside the set draws as "?", which is why the level-design test
checks every word of the story can be drawn.

### Formatting without allocating

Numbers on screen ("3 / 12", "0.75x", "LEVEL 5 · THE CATACOMBS · NORMAL")
are formatted into a buffer on the stack:

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

`std::format_to_n` writes at most `Capacity` characters and reports how
many the full output would have had; the format string is checked at
compile time (`std::format_string`). See
[Text without allocating](../techniques/allocation-free-text.md).

### The screens

`Menu` owns the screens (`Main`, `DifficultySelect`, `Controls`,
`Settings`, `Pause`, `Result`) and returns a `MenuAction` (start a game
with a difficulty, continue, resume, quit to menu, quit, settings
changed) that `Game::HandleMenuAction` carries out. It also draws, over
the game:

- the **briefing** before a level, with its objectives;
- **pages of story** between levels, with a row of dots for the pages;
- the **results** of a cleared level (kills, supplies, intel, secrets,
  explored, time), with the level's debrief below;
- **notices** ("You need the gold key") and the current **objective** at
  the top;
- a **page of intel**, over the lower part of the view, fading after nine
  seconds.

Long text is broken into lines between words by `DrawWrapped`, which
measures candidate lines with the cached glyph advances and draws each
line as a view into the original string: nothing is copied. Passed
`draw = false`, it only measures, which is how the intel panel is sized to
its text.

<figure markdown="span">
  ![A page of intel read over the view](../assets/screenshots/intel-read.png){ width="560" }
  <figcaption>A page of intel: a panel sized to its text by measuring first, then drawing.</figcaption>
</figure>

### The HUD

The 3D renderer draws the health, keys, rounds and FPS counter from
digit textures (Freedoom's status bar digits, and pre-rendered digits for
the FPS counter); the menu's `DrawEnemyCounter` and `DrawWeaponSlots` add
the kill count and the weapon slots, which light up for the weapons
carried and mark the one in hand (or the one coming).

## Design decisions and trade-offs

- **An in-house toolkit rather than Dear ImGui.** About six hundred lines
  cover exactly what the menus need, drawn with the same SDL renderer, in
  the game's own fonts and style, with no allocation per frame.
- **Screens are functions.** A screen is a member function that lays out
  and handles its widgets each frame; adding one is writing one function.
- **Fonts rasterised at startup, at fixed sizes.** No scaling of text
  beyond what the renderer does to the whole frame.

## Pitfalls

- **Characters outside the set** draw as "?": curly quotes, long dashes and
  accented letters in story text would show wrongly. The level-design test
  (`Story.EveryWordCanBeDrawn`) guards the shipped text.
- **Focus order is call order.** Drawing widgets in a different order
  (conditionally skipping one) shifts every later index; screens reset
  focus when they open (`ResetFocus`).

## Possible improvements

- Glyphs in one atlas texture instead of one texture each, drawn as a
  single `SDL_RenderGeometry` batch per string.
- Localised text would need a larger glyph set (and a way to choose it),
  since the cache is Latin-1 only.
