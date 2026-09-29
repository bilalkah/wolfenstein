# The 3D renderer

## Purpose

`Renderer3D` turns the camera's rays and sights into the picture: the sky
and floor, a textured strip for every wall column, pictures on walls
(bullet marks, the crack of a secret wall, pages of intel), the sprites of
enemies, lamps, pickups, puffs and projectiles, the weapon in hand, and the
HUD. Two lesser views share its interface: `Renderer2D`, the developer's
top-down view, and `Minimap`, the map in the corner (or large, with M).

Code: `src/Graphics/` (`renderer_3d.cpp`, `renderer_2d.cpp`,
`minimap.cpp`, `quad_batch.cpp`, `renderer_interface.cpp`).

<figure markdown="span">
  ![Rail yard: walls, a soldier and a zombie, the HUD](../assets/screenshots/combat-rail-yard.png){ width="640" }
  <figcaption>Everything on this screen is either a wall strip, a sprite, the weapon or HUD text, all drawn through one sorted queue.</figcaption>
</figure>

## Concepts

### The painter's algorithm

Something near must be drawn over something far. A rasteriser usually
keeps a depth buffer. A raycaster has an easier structure: every wall
column has a single distance, and every sprite has one too. So the
renderer can use the [painter's algorithm](https://en.wikipedia.org/wiki/Painter%27s_algorithm):
collect everything to draw with its distance, sort from far to near, and
draw in that order, each thing over what is behind it. Because walls are
drawn **per column**, a sprite half hidden by a pillar is covered exactly
by the pillar's nearer columns, with no per-pixel depth test.

### Billboards

A sprite is a flat picture turned to face the viewer, scaled by its
distance like a wall: an object `h` units tall at distance \(d\) is
\(h \cdot P / d\) pixels tall, standing on the floor (or raised by its
elevation). \(d\) is how far in front of the eye its **centre** stands,
along the view (`Camera2D::Sight::distance`). A sprite the viewer stands
in, less than 0.25 units from its centre (a lamp does not block the way),
is not drawn: it would cover the screen. Enemies have eight pictures, one per side they can be seen
from; which one is shown depends on the angle between the enemy's facing
and the viewer (see [Textures and animation](assets.md)).

### Decals

A decal is a small picture stuck on a wall face. The renderer knows each
wall column's texture coordinate \(u\), so a decal covering \(u \in [a,
a + w)\) of a face appears in exactly the columns whose \(u\) falls inside
it. Rather than drawing a sliver in every one of those columns, the
renderer remembers the first and last column the decal touched and draws
**one textured quad** through their ends (a face is planar, so its edges
are straight lines on screen).

## How it is implemented here

### The render queue

Every thing to draw becomes a `RenderCommand`:

```cpp title="src/Graphics/include/Graphics/renderer_3d.h"
    struct RenderCommand
    {
        int texture_id = 0;
        SDL_Rect src_rect{};
        SDL_Rect dest_rect{};
        double distance = 0.0;
        std::uint32_t order = 0;
        const std::array<SDL_Vertex, 4>* quad = nullptr;
        bool mirrored = false;    // drawn flipped left to right
    };
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/include/Graphics/renderer_3d.h#L39-L48){ .excerpt-source }

A command is a rectangle of a texture to copy into a rectangle of the
screen, or (for decals) a quad of four vertices. The queue is a
`std::vector` that is cleared each frame but keeps its capacity: its size
is reserved at startup for a command per wall column, every decal and
every object of the largest level, so queuing never allocates.

### One frame of drawing

```cpp title="src/Graphics/src/renderer_3d.cpp"
void Renderer3D::RenderScene(double delta_time) {
    ScopedTimer render_timer(ProfileSection::Render);
    render_queue_.clear();
    // Looking up, or a shot's kick, drops the world down the screen
    const Player& player = scene_->GetPlayer();
    pixels_per_unit_ =
        PixelsPerUnit(context_->GetConfig(), context_->GetCamera().GetFov());
    horizon_shift_ =
        static_cast<int>(context_->GetCamera().GetPitch() * pixels_per_unit_);
    eye_height_ = player.GetEyeHeight();
    // Falling dead, the world is drawn aside to be rolled over
    const double fall = player.GetDeathFall();
    falling_ = fall > 0.0 && fallen_view_ != nullptr;
    if (falling_) {
        SDL_SetRenderTarget(context_->GetRenderer(), fallen_view_.get());
    }
    ClearScreen();
    RenderBackground();
    {
        ScopedTimer timer(ProfileSection::RenderWalls);
        RenderWalls();
    }
    {
        ScopedTimer timer(ProfileSection::RenderObjects);
        RenderObjects();
        RenderWeapon();
    }
    {
        ScopedTimer timer(ProfileSection::RenderDraw);
        RenderTextures();
        RenderHitMarker();
    }
    if (falling_) {
        SDL_SetRenderTarget(context_->GetRenderer(), nullptr);
        RenderFallen(fall);
        RenderDamage();
    }
    ScopedTimer timer(ProfileSection::RenderHud);
    RenderHUD(delta_time);
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/src/renderer_3d.cpp#L109-L148){ .excerpt-source }

1. **Horizon.** Looking up or down, and a shot's kick, do not tilt the
   camera; they slide the whole world down or up the screen by
   `pitch * pixels_per_unit` pixels (a "y-shearing" look, as in *Doom*).
2. **Background.** The sky picture, repeated round the turn, from the
   horizon up; a flat colour above it when looking higher than the picture
   reaches, fading into it; the floor a flat grey.
3. **Walls.** A command per ray: a one-texel-wide strip of the wall's
   texture into a two-pixel-wide column, at the ray's distance. Decals on
   the face a column shows are gathered as it goes (`RenderWallMarks`,
   `RenderIntel`, the secret crack) and queued as quads at the end.
4. **Objects.** A command per visible object, sized by its distance and
   height, raised by its elevation, mirrored if the camera says so (a body
   lying dead, seen from behind).
5. **Weapon.** The crosshair and the weapon's current frame, queued at
   distance 0 so they sort in front of everything.
6. **Draw.** Sort the queue from far to near and draw it.
7. **Dying.** While the player falls dead, everything above was drawn into
   an off-screen texture, which is now drawn rotated up to 80 degrees and
   scaled to cover the screen: the view rolls onto its side.
8. **HUD.** Health, keys, rounds in the magazine and in reserve, the FPS
   counter; digits from pre-rendered textures.

### Heights and the horizon

```cpp title="src/Graphics/src/renderer_3d.cpp"
std::tuple<int, int, int> Renderer3D::CalculateVerticalSlice(
    const double& distance) {
    const auto& config_ = context_->GetConfig();
    // Near zero distance the height tends to infinity, and converting that to
    // an int is undefined: clamp the distance and the result
    constexpr double kNearest = 0.01;
    constexpr double kTallest = 32.0;  // screen heights
    const double height =
        std::min(pixels_per_unit_ / std::max(distance, kNearest),
                 config_.height * kTallest);
    auto line_height = static_cast<int>(height);
    // A wall spans the floor to a wall's height; the eye is eye_height_ up
    // it, level with the horizon
    const int horizon = config_.height / 2 + horizon_shift_;
    int draw_start = horizon - static_cast<int>((1.0 - eye_height_) * height);
    int draw_end = horizon + static_cast<int>(eye_height_ * height);
    return std::make_tuple(line_height, draw_start, draw_end);
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/src/renderer_3d.cpp#L498-L515){ .excerpt-source }

### Sorting and drawing

```cpp title="src/Graphics/src/renderer_3d.cpp"
void Renderer3D::RenderTextures() {
    // Back to front; ties keep submission order. std::sort needs no buffer
    // (std::stable_sort would allocate one), the order field makes it stable
    std::ranges::sort(render_queue_, [](const RenderCommand& lhs,
                                        const RenderCommand& rhs) static {
        if (lhs.distance != rhs.distance) {
            return lhs.distance > rhs.distance;
        }
        return lhs.order < rhs.order;
    });
    auto* renderer = context_->GetRenderer();
    // Two triangles a quad: top left, bottom left, top right; and bottom
    // left, bottom right, top right
    static constexpr std::array<int, 6> kQuadTriangles{0, 1, 2, 1, 3, 2};
    for (const RenderCommand& command : render_queue_) {
        const auto& texture =
            context_->Textures().GetTexture(command.texture_id);
        if (command.quad != nullptr) {
            SDL_RenderGeometry(renderer, texture.texture, command.quad->data(),
                               static_cast<int>(command.quad->size()),
                               kQuadTriangles.data(),
                               static_cast<int>(kQuadTriangles.size()));
        }
        else if (command.mirrored) {
            SDL_RenderCopyEx(renderer, texture.texture, &command.src_rect,
                             &command.dest_rect, 0.0, nullptr,
                             SDL_FLIP_HORIZONTAL);
        }
        else {
            SDL_RenderCopy(renderer, texture.texture, &command.src_rect,
                           &command.dest_rect);
        }
    }
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/src/renderer_3d.cpp#L644-L677){ .excerpt-source }

The comparator is a C++23 **static lambda** (`static` after the parameter
list): it captures nothing, so it has no `this` to pass. `std::stable_sort`
would have kept ties in order but may allocate a buffer; the `order`
field makes the unstable `std::ranges::sort` stable instead.

### The sky

The sky is a panorama that turns with the view. To join up seamlessly
wherever the player looks, it must repeat a **whole number** of times in a
full turn:

```cpp title="src/Graphics/include/Graphics/renderer_interface.h"
inline SkyLayout LaySky(double theta, double pixels_per_radian,
                        double natural_width) {
    const double turn = 2.0 * std::numbers::pi * pixels_per_radian;
    const double repeats = std::max(1.0, std::round(turn / natural_width));
    const double width = turn / repeats;
    double offset = std::fmod(theta * pixels_per_radian, width);
    if (offset < 0.0) {
        offset += width;
    }
    return {.width = width, .offset = offset};
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/include/Graphics/renderer_interface.h#L76-L86){ .excerpt-source }

A full turn is \(2\pi \cdot\) (pixels per radian) wide, at the rate the
middle of the picture moves when turning (`PixelsPerRadian`,
\(\text{width} / 2\tan(\text{FOV}/2)\)), so the sky keeps pace with the
walls in front of the player; the picture's
natural width at the sky's height is rounded to the nearest whole fraction
of that. The comment above `SkyLayout` records the bug this fixed: with a
part of a repeat left over, the sky jumped where the view's angle wrapped
round (`d41e706`).

### Decals as quads

`AddDecalColumn` keeps, per decal, its first and last column, their texture
coordinates and the column before the last; `EnqueueDecals` turns each into
one `SDL_RenderGeometry` quad at the nearest column's distance. Pages of
intel are drawn dimmer once read (the vertex colour), and are turned to
read left to right on the faces whose texture runs right to left. Each
decal is identified by a key (a bullet mark's index, a secret's face, a
page of intel's index), so a decal seen across many columns is merged.

### The 2D view and the map

`Renderer2D` (P, with `?debug`) and `Minimap` draw rectangles, lines and
triangles. SDL's own `SDL_RenderDrawLine` and friends build temporary
arrays on the heap, so both go through `QuadBatch`, which collects
vertices in storage sized once and draws them in a single
`SDL_RenderGeometry` call.

<figure markdown="span">
  ![The whole first level on the large map](../assets/screenshots/map.png){ width="560" }
  <figcaption>The map (M): explored cells only, doors in blue, the player's arrow, pickups and keys.</figcaption>
</figure>

### Warming up at startup

SDL grows its command pool and vertex buffer the first time a frame queues
more than any before, and GPU drivers compile shaders on a draw's first
use. `RendererContext` therefore issues 4096 throwaway copies, one of each
kind of draw and one batch as large as the 2D view ever draws, then clears
and presents, all at startup. Without it, the first busy frame of play
would allocate inside SDL and stutter.

## Design decisions and trade-offs

- **SDL_Renderer, not raw OpenGL.** The same code draws with Metal,
  OpenGL, Direct3D or WebGL, and the web build needed no renderer changes.
  The cost is one draw command per wall column (600 at the default
  width) and per sprite, which SDL batches internally.
- **Painter's algorithm, no depth buffer.** Natural for columns and
  billboards. The flat floor and ceiling are drawn first under everything.
- **Commands carry distances, not layers.** The weapon and crosshair use
  distance 0 and the damage overlay \(-1\), which sorts them last.
- **Decals merged into quads.** A decal used to be a sliver per column,
  "a draw call and two texture switches a column", and "a frame full of
  marks took a quarter longer" (comment on `Decal`, commit `7524277`).

## Pitfalls

- **Affine texture mapping on decals.** `SDL_RenderGeometry` interpolates
  texture coordinates linearly across the quad; on a wall seen at a slant
  the correct mapping is not linear in screen \(x\), so a decal on a
  slanted wall is very slightly squeezed towards its far end. The decals
  are small, so it is barely visible.
- **A sprite's distance is its centre's.** Until `fab3414` it was taken
  at the picture's left edge, which up close is far off to the side and so
  seems much nearer. An enemy's picture is wider than its body (a
  soldier's, 1.12 units against 0.33), so walking up to one it was drawn
  too tall, and a step closer (about 0.42 units, straight ahead) it
  vanished: its left edge seemed nearer than 0.25 units, where a sprite
  the viewer stands in is dropped.
- **A sprite beside the eye reaches far off the screen.** One edge can lie
  almost at a right angle to the view, hundreds of thousands of pixels out,
  and a renderer that draws in software builds an image the size of the
  whole rectangle first: in CI's benchmark that ran out of memory.
  `ClipToScreen` cuts each sprite to the part on the screen, at whole
  texels so the picture keeps its scale, before it is queued.
- **A sprite is sorted by one distance.** `RenderObjects` queues the whole
  sprite at its centre's distance. Against wall columns
  that is right almost always, since each column sorts on its own; but a
  sprite standing right against a wall, at nearly the wall's distance, can
  be covered by columns it should stand in front of, or the reverse.
- **The render queue holds pointers into `decal_quads_`.** Commands for
  decals point at quads owned by the renderer; they are valid for the
  frame only, which is all the queue lives for.

## Possible improvements

- Draw wall columns as one batched `SDL_RenderGeometry` call per texture
  (all columns of a texture in one vertex array), cutting hundreds of
  draw commands to a handful.
- Textured floors and ceilings would need a per-row pass (as in
  Lode Vandevenne's [floor casting](https://lodev.org/cgtutor/raycasting2.html))
  or a shader.
- Distance fog or shading by distance, via the texture colour modulation
  already used for read intel.
