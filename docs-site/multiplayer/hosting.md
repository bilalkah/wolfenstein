# Hosting a game

A match needs a server, `wolfenstein-server`, reachable by every player,
and the players need the game: the web page, or the native game. On a
local network one machine does both; on the internet, `docker/compose.yml`
puts the server and the web game behind Caddy, which holds the
certificate.

## On a local network

Build the server natively and run it (it needs no display or sound):

```bash
cmake --preset native-release
cmake --build --preset native-release --target wolfenstein-server
./build/native-release/bin/wolfenstein-server          # port 8080
```

Serve the web game from the same machine, to the network:

```bash
./scripts/run_web.sh          # builds, then serves on port 8000
```

Everyone opens `http://<that machine's address>:8000`, chooses
**MULTIPLAYER**, types a name and joins: the page, served over plain
http, offers the server on port 8080 of the machine it came from. On
macOS, allow `wolfenstein-server` to accept incoming connections when the
firewall asks.

The native game joins with `--connect`:

```bash
./build/native-release/bin/wolfenstein --connect ws://192.168.0.11:8080 --name ann
```

## The server's options

| Option | Default | What |
| --- | --- | --- |
| `--port` | 8080 | The port to listen on |
| `--level` | every arena in `config.json` | The arenas to play, in turn: `bazaar.json,warehouse.json` |
| `--mode` | `deathmatch` | `deathmatch` or `gunrace` |
| `--frags` | 20 | A deathmatch's frag limit |
| `--minutes` | 10 | A match's time limit |
| `--assets` | the repository's `assets/` | Where the game's content is |

`GET /health` answers `ok`, for a load balancer or a monitor. Each
address path is a match: `/` the open one, `/room/CODE` a room (see
[Rooms](matches.md#rooms)).

## In Docker

`docker/server.Dockerfile` builds the server in one stage and runs it in a
small Ubuntu image with the game's content, as an unprivileged user:

```bash
docker build -f docker/server.Dockerfile -t wolfenstein-server .
docker run --rm -p 8080:8080 wolfenstein-server
```

## On the internet

A page served over https may open only `wss://` WebSockets, so a server
on the internet needs a certificate. `docker/compose.yml` runs three
containers on one machine: the server, the web game, and
[Caddy](https://caddyserver.com), which gets a certificate for the domain
from Let's Encrypt and passes WebSocket upgrades (the open match and every
room) to the server, and everything else to the web game:

```bash
DOMAIN=play.example.com docker compose -f docker/compose.yml up -d --build
```

The domain's DNS must point at the machine, with ports 80 and 443 open.
`MODE`, `FRAGS` and `MINUTES` set how matches are played. The page is then
`https://play.example.com/`, and it offers `wss://play.example.com` as the
server. The native game joins `wss://` servers through the system's TLS
(Apple's on macOS, OpenSSL where it is installed).

A match of eight takes a few percent of a CPU core, a few megabytes and
about 0.6 Mbit/s, so the smallest cloud machine is plenty.

## Testing without a network

`tests/match_test.cpp` plays a server and several players' games in one
process, their messages carried in memory and delivered as many ticks
late as a test asks: a player walks at once and the server agrees, one
bumping into another is put right, a kill is seen in both games, a pickup
taken is gone from every game, the next match is on the next arena, a
player measures its round trip. `tests/game_server_test.cpp` checks the
server alone (commands, rules, judging a shot in hindsight, coming back
after a drop), and that a fight among eight allocates nothing.
`tests/lobby_test.cpp` checks the rooms.
