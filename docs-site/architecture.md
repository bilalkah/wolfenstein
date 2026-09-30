# Architecture

Two programs share one engine: the game, `karakale`, and the
multiplayer server, `karakale-server`. Each directory under `src/` is a
small static library.

## Two halves

- **Simulation** (`World`, `Scene`, player, enemies, map, pathfinding,
  weapons, collision): advances in fixed ticks from `PlayerCommand`s and
  knows nothing of the screen, keyboard or network.
- **Presentation** (`Game`, renderers, menus, HUD): reads input into
  commands, and draws the simulation's state as often as the display
  allows.

The same simulation runs in three places: a single-player game, a
player's game during a match, and the server.

## Modules

```mermaid
flowchart TB
    subgraph presentation["Presentation"]
        game["Core: Game<br/>loop, states, input"]
        gfx["Graphics<br/>3D and 2D views, map, menus, HUD"]
        ui["UI<br/>immediate-mode widgets"]
    end
    subgraph simulation["Simulation"]
        world["Core: World, Scene"]
        chars["Characters<br/>Player, Enemy"]
        ai["State, NavigationManager<br/>AI states, A*"]
        combat["Strike, ShootingManager<br/>weapons, shots"]
        physics["CollisionManager, GameMap, Camera<br/>bodies, grid, rays"]
    end
    subgraph multiplayer["Multiplayer"]
        client["Client: MatchClient"]
        server["Server: GameServer, MatchRules, Lobby"]
        net["Net: protocol, Connection"]
    end
    subgraph services["Services"]
        svc["SoundManager, TextureManager, Animation,<br/>Settings, Allocators, Profiler, TimeManager"]
    end
    game --> gfx --> ui
    game --> world
    game --> client --> world
    client --> net
    server --> world
    server --> net
    world --> chars --> ai & combat & physics
    gfx --> physics
    world & gfx --> svc
```

## Ownership

One owner for everything, so lifetimes are plain:

```mermaid
flowchart LR
    game["Game"] --> world["World<br/>content, sound, players"]
    game --> views["Renderers, Camera, Menu"]
    world --> arena["Level arena<br/>(one block, reset per level)"]
    world --> scene["Scene<br/>the current level"]
    scene --> pools["Pools: enemies, lamps, pickups"]
    scene --> map["Map, doors, navigation grid"]
    views -. borrow .-> scene
```

- The `World` owns the content (read at startup), the sound, up to eight
  players and the current `Scene`.
- The `Scene` lives in the level arena; its objects live in fixed pools.
- Renderers, the camera and the enemies **borrow** the scene; nothing is
  shared or reference-counted.

## A frame

```mermaid
flowchart TD
    input["SDL events and state"] --> command["PlayerCommand<br/>(view angles turned this frame)"]
    command --> step{"FixedStep:<br/>ticks due?"}
    step -->|"0 or more ticks of 1/60 s"| update["Scene::Update<br/>players, enemies, projectiles, doors"]
    step --> camera["Camera: cast the rays,<br/>place the sprites"]
    update --> camera
    camera --> queue["Render queue: wall strips,<br/>sprites, decals"]
    queue --> sort["Sort far to near"]
    sort --> draw["Draw, then HUD and menus"]
```

- The view is drawn `alpha` of the way between the last two ticks, so
  motion is smooth at any frame rate.
- In a match, `MatchClient` wraps each tick: it sends the command before
  and places the other players after (see [Multiplayer](multiplayer.md)).

## Content

```mermaid
flowchart LR
    config["config.json<br/>weapons, enemies, pickups, campaign"] --> loader["SceneLoader<br/>(once, at startup)"]
    levels["levels/*.json + maps/*.txt"] --> loader
    loader --> prepared["Prepared levels<br/>and their memory sizes"]
    prepared --> scene["Scene<br/>built in the arena per level"]
```

Everything a level needs is read and sized at startup, so starting a level
allocates nothing.
