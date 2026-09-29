# Ranges algorithms

## The technique

C++20's `std::ranges` algorithms take a whole range instead of a pair of
iterators (`std::ranges::sort(v)` rather than `std::sort(v.begin(),
v.end())`), and most accept a **projection**: a function (or a pointer to
a member) applied to each element before comparing or testing it. C++23
added **static lambdas**: a captureless lambda may declare its call
operator `static`, so calling it passes no `this`.

## Where it appears here

About 45 uses of `std::ranges::` in `src/`:

### Sorting the render queue

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
    // ...
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/src/renderer_3d.cpp#L644-L677){ .excerpt-source }

The static lambda and the explicit tie-breaker (`order`, the command's
position in the queue) give the stable order of `std::stable_sort` without
the temporary buffer it may allocate.

### Projections: counting by a member

```cpp title="src/Core/src/scene.cpp"
    const auto read = static_cast<std::size_t>(
        std::ranges::count_if(GetIntel(), &WallIntel::read));
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/src/scene.cpp#L764-L765){ .excerpt-source }

A pointer to a data member is a valid predicate: `&WallIntel::read`
counts the pages read. The same trick appears with
`std::ranges::count_if(secrets, &PushWall::pushed)` and
`std::ranges::find(voices_, nullptr, &Voice::chunk)` (a projection: find
the voice whose `chunk` is null).

### The A* open set

`GridPathFinder` keeps its open set as a binary heap in a vector with
`std::ranges::push_heap` and `std::ranges::pop_heap`, ordered by a lambda
comparing `f` (see [Pathfinding](../engine/navigation.md)); the vector is
reserved to its exact bound, so the heap never allocates.

### Others

- `std::ranges::fill` to clear stamp arrays when the generation counter
  wraps;
- `std::ranges::copy` into fixed routes and stack buffers;
- `std::ranges::min_element` with a projection to find the quietest
  voice;
- `std::ranges::sort(choices, std::greater{})` to rank an enemy's
  candidate bearings, best first (`std::greater{}` is the transparent
  comparator).

## Pitfalls

- `std::ranges::sort` is not stable; code that needs ties in order must
  say how to break them, as the render queue does.
- Projections are applied on every comparison; keep them cheap (member
  pointers are).
