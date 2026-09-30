# Wolfenstein engine

A first-person shooter in the style of *Wolfenstein 3D* and *Doom*, and
the C++23 engine under it. It runs natively and in the browser
(WebAssembly), with a fifteen-level campaign and a deathmatch for up to
eight players.

<a class="md-button md-button--primary" href="play/">Play it in your browser</a>
<a class="md-button" href="https://github.com/bilalkah/wolfenstein">Source on GitHub</a>

<figure markdown="span">
  ![The barracks courtyard: a soldier comes round a pillar](assets/screenshots/combat-barracks.png){ width="640" }
</figure>

## At a glance

| Area | How |
| --- | --- |
| Rendering | A raycaster: one ray per two screen columns walked through the grid (DDA), billboards for sprites, drawn far to near on SDL 3's GPU renderer |
| Simulation | Fixed 60 Hz ticks driven only by commands, so it is deterministic; frames are drawn interpolated between ticks |
| Memory | No heap allocation after startup: one arena per level, fixed pools and buffers; CI fails a build that allocates |
| Enemies | State machines; sight by line-of-sight rays, hearing by a flood fill; weighted A* on a half-cell grid |
| Browser | Emscripten, single-threaded; the 15 MB download is fetched in parallel, resumable pieces and cached in IndexedDB |
| Multiplayer | An authoritative server over WebSocket; client-side prediction, interpolation and lag compensation |

## Controls

| Action | Keys |
| --- | --- |
| Move, turn | W A S D, the mouse (or the arrow keys) |
| Fire, reload | Left click (hold for automatic weapons), R |
| Weapons | 1 to 8, or the mouse wheel |
| Use (doors, secret walls, the exit switch) | E, or Space |
| Map | M |
| Scoreboard, in a match | Tab |
| Pause | Esc |

## This site

- [Stack and why](stack.md): each technology and the reason for it.
- [Architecture](architecture.md): the modules, who owns what, a frame.
- [Problems solved](problems.md): each problem, and how it was solved.
- [Algorithms](algorithms.md): the algorithms used, and how they work.
- [Multiplayer](multiplayer.md): the server, the netcode, the protocol.
- [Building and hosting](building.md): building, testing, running a server.
