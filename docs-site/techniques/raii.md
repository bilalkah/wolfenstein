# RAII and custom deleters

## The technique

**Resource Acquisition Is Initialisation**: a resource (memory, a file, a
GPU texture, a lock, a timer's start time) is acquired in a constructor
and released in the matching destructor. Because C++ runs destructors
deterministically when an object goes out of scope, including when an
exception unwinds, the resource cannot leak and cannot be released twice
(if copying is forbidden or correct).

`std::unique_ptr<T, Deleter>` turns any C-style handle with a free
function into an RAII object: the deleter is called on the pointer when
the `unique_ptr` is destroyed or reset. With a stateless deleter type (not
a function pointer), the `unique_ptr` is no bigger than a raw pointer.

## Where it appears here

### SDL objects held by `unique_ptr`

```cpp title="src/Graphics/include/Graphics/renderer_3d.h"
    struct TextureDeleter
    {
        void operator()(SDL_Texture* texture) const noexcept;
    };
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/include/Graphics/renderer_3d.h#L83-L86){ .excerpt-source }

`Renderer3D` holds the pre-rendered FPS digits and the off-screen texture
the dying view is drawn into as
`std::unique_ptr<SDL_Texture, TextureDeleter>`; the deleter calls
`SDL_DestroyTexture`. Replacing a texture is `reset(new_texture)`, and
nothing can forget to free the old one.

### Owners of C libraries

`RendererContext` owns the SDL window, renderer, font and the texture
manager, and its destructor releases them **in the right order**:
textures first ("Textures belong to the renderer: they go first"), then
the renderer, the font, the window, and finally `TTF_Quit` and `SDL_Quit`.
`SoundManager` does the same for the audio device, chunks and music.
Because these classes are pinned (copy and move deleted), there is exactly
one owner of each resource.

### Scoped timers

`ScopedTimer` is RAII applied to measurement: the constructor records the
time and the allocation count, the destructor adds the difference to its
profiler section. A section is timed by declaring a variable:

```cpp
{
    ScopedTimer timer(ProfileSection::RenderWalls);
    RenderWalls();
}   // the time and allocations of RenderWalls() are recorded here
```

See [Profiling and testing](../engine/profiling.md).

### Declaration order as teardown order

Members are destroyed in reverse declaration order, so declaring owners
before borrowers makes teardown correct with no destructor code at all.
`Game` and `World` both rely on it, with comments saying so (see
[Ownership and lifetimes](../architecture/ownership.md)).

## Pitfalls

- A class with a hand-written destructor that frees a resource must also
  forbid (or correctly implement) copying; the engine deletes copy and
  move on every such class, and `tests/type_traits_test.cpp` checks it.
- RAII releases at scope end: a `ScopedTimer` declared at the top of a
  function times the whole function, including the parts after the
  interesting one. The game opens explicit `{ ... }` blocks to time
  exactly what it means.
