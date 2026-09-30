# Multiplayer

| | |
| --- | --- |
| **When** | 30 September 2026 |
| **Commits** | `5bca75b` eight players in a level, `9c6e388` the protocol, `a6cdb55` the server, `728a115` playing a match, `1ed5290` combat and the gun race, `54c60fc` the Warehouse and rotation, `ae5cbde` the menu, rooms and coming back, `50490d3` footsteps, callouts, the shield shown, `65ba253` hosting on the internet, `84e5cd1` the server beside the page, `60a1902` ping |
| **Code today** | [Multiplayer](../multiplayer/index.md) |

## Problem

A deathmatch for friends: up to eight players, free for all, in the
browser and the native game together, on a server that is cheap to run.

## Constraints

- One simulation: the server and the players' games run the same
  `World`, as the engine was built to (a comment in `World` had noted that
  "a server can create worlds of their own").
- Browsers: no raw sockets, so WebSocket; a page served over https may
  open only `wss://`.
- No allocation once a game runs, on the server included.
- Cheap: a very basic game should cost very little to host.

## Approach

1. **Eight players in a level** (`5bca75b`): "A scene now has kMaxPlayers
   slots and a viewer, the player the level is seen and heard from …
   alone, the player is in slot 0 and is the viewer, so single player
   plays as it did." The others are drawn as the soldier's figure, tinted
   their slot's colour.
2. **The protocol** (`9c6e388`): hello, welcome, reject, input and
   snapshot, packed by hand, read and written without allocating.
3. **The server** (`a6cdb55`): `GameServer` runs one match knowing nothing
   of sockets; `server/main.cpp` puts it behind uWebSockets; a Dockerfile
   builds and runs it. The first arena, the Bazaar, is drawn by
   `scripts/make_arenas.py`.
4. **Playing a match** (`728a115`): the game joins with `--connect` (or
   `?server=`); its own player is foreseen and put right, the others
   placed 100 ms behind. The native game's WebSocket is IXWebSocket; the
   browser's its own.
5. **Fighting** (`1ed5290`): shots judged in hindsight on the server, a
   player's game judging nothing about the players; `MatchRules` with
   frags, coming back, pickups returning, both modes; events and scores in
   protocol 2; the match's HUD.
6. **A second arena** (`54c60fc`): the Warehouse, and the server playing
   its arenas in turn.
7. **Joining from the menu, rooms and coming back** (`ae5cbde`): the
   MULTIPLAYER screen, text fields in the UI toolkit, rooms by address,
   rejoining by itself after a drop and the score kept by name.
8. **Feel** (`50490d3`): the others' footsteps, "YOU FRAGGED ANN", a
   shielded player shown paler.
9. **Hosting** (`65ba253`, `84e5cd1`): Caddy in front on the internet,
   `wss://` for the native game, the page offering the server beside it.
10. **Ping** (`60a1902`): measured once a second, timed from when the
    answer arrived, every player's shown on the scoreboard.

## C++ techniques used

- A `std::variant` of message types, written and read by `std::visit`
  and a switch on the type byte, into fixed buffers (`ByteWriter`,
  `ByteReader`, `FixedString`).
- An interface for what the server's game needs of the transport
  (`Outbox`) and of lag compensation (`Hindsight`), so tests play whole
  matches in one process.
- `std::function` for the clock a `MatchClient` times its pings with, so
  a test can hand it the simulated time.
- Fixed rings everywhere a stream is kept: commands sent, snapshots, the
  inbox, where every player stood.

## Key code

- `MatchClient::Reconcile` and `MatchClient::Restock`: putting a player
  right.
- `GameServer::Tick` and `GameServer::Seen`: a tick, and judging in
  hindsight.
- `MatchRules::Tick`: the rules.

## Pitfalls

- **Both sides must run the same code;** the version turns away another.
- **A server's World has no window**: its textures are sizes and masks
  only (`TextureManager::Load` with no renderer), enough to judge shots.

## What I'd change

- Bots, so a match can be played alone or topped up.
- Co-op: the campaign's levels, played together.
