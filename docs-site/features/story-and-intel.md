# Story and intel

| | |
| --- | --- |
| **When** | 26 to 29 September 2026 |
| **Commits** | `0bf1592` Brief the player before each level, `5880c6e` Tell a story between the levels, `5880c6e` Find intel: pages of the story lying in the levels, `5880c6e` fifteen levels in three chapters, `5880c6e` Hang intel on the walls |
| **Code today** | `Core/story.h`, `Menu::DrawStoryPage`, `DrawDocument`, `Scene::ReadIntel`, the intel decals in `Renderer3D::RenderIntel` |

## Problem

Give the campaign a reason: why the player is in the valley, what happened
to the garrison, what the castle is counting down to. Tell it without
cutscenes, in the game's own screens, and reward exploring.

## Constraints

- All text is data (in `config.json` and the level files).
- Drawing story pages and intel allocates nothing.
- Every word must be drawable with the glyphs the menu rasterises.

## Approach

- **Briefings** (`0bf1592`): a few lines before each level, with its
  objectives.
- **The story between levels** (`5880c6e`): "A new game opens with three
  pages of it ... The levels fall into three chapters (the valley, the
  works, the castle), each opened by a card. After a level's results comes a
  word on what was learnt there, and the last level is followed by the
  ending before the victory screen."
- **Intel** (`5880c6e`, then `5880c6e`): pages found in the levels (a letter
  home, the chaplain's diary, the second team's notebook, orders), first as
  pickups on the floor, then "a picture on a wall's face, drawn as the
  walls' other decals are. The player reads it by walking up to it and
  looking at it, and again by using it; once read it hangs there dimmer."

<figure markdown="span">
  ![A page of intel on a wall](../assets/screenshots/intel-wall.png){ width="480" }
  <figcaption>A page of intel hanging on a wall; walking up to it opens it.</figcaption>
</figure>

<figure markdown="span">
  ![A chapter card](../assets/screenshots/chapter.png){ width="480" }
  <figcaption>The first chapter's card, between the opening and the first briefing.</figcaption>
</figure>

## C++ techniques used

- [Non-owning views](../techniques/views.md): `StoryPage` is three
  `std::string_view`s into the config, written into a fixed array of
  eight.
- A fixed `std::array<WallIntel, 8>` per level; decals keyed per page in
  the renderer.

## Key code

- [`StoryBefore` and `StoryAtTheEnd`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/include/Core/story.h)
- `Scene::ReadIntel`, `FindIntel` and `ShowIntel` in
  [`scene.cpp`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/src/scene.cpp)
- `Renderer3D::RenderIntel` in
  [`renderer_3d.cpp`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/src/renderer_3d.cpp)

## Pitfalls

- A wall texture runs right to left on two of a cell's four faces; the
  intel decal is flipped there so the picture reads the same way round on
  every wall.
- Moving intel from pickups to walls changed the pickups' indices in every
  level, so the save format went from 3 to 4.

## What I'd change

- A way to re-read every page found so far, from the pause menu.
