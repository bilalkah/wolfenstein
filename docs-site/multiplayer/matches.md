# Matches, modes and rooms

## Purpose

How a match is played and won, on which arenas, and how one server holds
several matches: the rules the server applies after each tick, the two
modes, the arenas and how they are drawn, the rooms, and a player coming
back after its connection dropped.

Code: `src/Server/src/match_rules.cpp`, `src/Server/src/lobby.cpp`,
`src/Server/src/game_server.cpp`, `scripts/make_arenas.py`,
`assets/levels/config.json` (`arenas`, `gun_race`).

## The rules

`MatchRules` reads what happened in each tick (the scene records the kills
and the pickups taken as events) and applies the match's rules:

- **A frag for each kill,** a death for the fallen; a player killing
  itself (its own rocket) loses a frag.
- **Coming back.** A player down comes back after three seconds, or
  after one if it fires; whole, carrying what a game starts with, at the
  spawn point farthest from the other living players, and shielded for
  one and a half seconds or until it fires.
- **Pickups come back** a while after they are taken: a gun after 20
  seconds, a large medkit after 30, the rest after 15.
- **The end.** In a deathmatch the first to the frag limit (20) wins;
  past the time limit (10 minutes) the leader wins, or it is a draw. The
  result then shows for ten seconds, everyone shielded, and the next
  match begins on the next arena, the scores none again.

The spawn point is the one whose nearest living player is farthest away;
when several are equally far (no one else alive), they are taken in turn:

```cpp title="src/Server/src/match_rules.cpp"
Position2D MatchRules::FarthestSpawn(std::size_t slot) const {
    const auto spawns = world_.Spawns();
    if (spawns.empty()) {
        return world_.SpawnFor(slot);
    }
    double best = -1.0;
    std::size_t chosen = 0;
    for (std::size_t k = 0; k < spawns.size(); ++k) {
        const std::size_t i = (k + spawn_turn_) % spawns.size();
        double nearest = std::numeric_limits<double>::max();
        for (std::size_t other = 0; other < standings_.size(); ++other) {
            const Player* player = world_.FindPlayer(other);
            if (other != slot && player != nullptr && player->IsAlive()) {
                nearest = std::min(nearest,
                                   player->GetPose().Distance(spawns[i].pose));
            }
        }
        if (nearest > best) {
            best = nearest;
            chosen = i;
        }
    }
    return spawns[chosen];
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/60a190225b296a849c4d27c806ea9186e85a324b/src/Server/src/match_rules.cpp#L127-L150){ .excerpt-source }

### The gun race

Each kill takes the killer up a ladder of weapons, set in `config.json`:
pistol, machine gun, shotgun, super shotgun, plasma rifle, rocket
launcher, chainsaw. A player carries only its step's weapon, whose
rounds never run out; the level's weapons and rounds are left out, the
medkits stay. A kill with the last weapon wins; past the time limit the
player furthest up wins. A player coming back carries its step's weapon.

### Playing the rules

Each player's game is told the scores (`Scores`, on a change and every
second) and the kills (`Events`), and shows them: the frags, the place and
the clock under the corner map, the latest kills at the top left, "YOU
FRAGGED ANN" across the view for a kill it had a hand in, and the
scoreboard while Tab is held and while the result shows. Down, "Click to
come back" says how.

## The arenas

Two arenas are drawn for multiplayer, each with more spawn points than
players, and guns, rounds and medkits spread through it:

- **The Bazaar** (28 × 36): two long streets west and east behind the
  houses, the market between them with its hall, stalls and a block of
  stalls in the middle, doors between the streets and the market, an
  alley along the south.
- **The Warehouse** (30 × 36): offices and a yard to the north, the
  warehouse with its stacks of crates in the middle, a corridor down its
  west side and the loading dock to its east, the old tunnel along the
  south.

`scripts/make_arenas.py` draws each one from a picture made of
characters (walls of three textures, pillars, doors, floor, spawn points,
pickups, lamps) and writes the map and the level file, checking first
that the border is solid, every open cell can be reached from every
other, each door hangs in a wall, there are at least eight spawn points,
no two closer than four cells, with nothing a body bumps into beside them,
and that the arena's name is not a campaign level's. The server plays
the arenas it is given in turn, one match each, bringing everyone into
the next and welcoming them to it; a player's game starts the new level
and points its views at it.

## Rooms

One server holds several matches: the **open** one, always there, and a
**room** for each group that names one. The address says which:
`ws://host:8080` joins the open match, `ws://host:8080/room/FRIDAY` the
room FRIDAY (its code is the letters and digits typed, in capitals, twelve
at most). A room is made as its first player comes and closed a minute
after its last one leaves; a server holds sixteen at most. Each room is a
`GameServer` of its own, with its own world, rules and arena rotation;
the `Lobby` hands each connection's messages to its room's.

## Coming back after a drop

When the connection is lost in the middle of a match, the game says so
and joins again by itself, five times, two seconds apart; Esc gives up.
The server keeps the score of the last eight players who left, and one
coming back under the same name within a minute, in the same match, has
its score back. A connection can die without the server hearing of it
for a while; a player saying hello under the name of one not heard from
for five seconds replaces it.

## Design decisions and trade-offs

- **Spawn far from the living, not at random.** Being shot the moment one
  comes back is the worst feeling in a deathmatch; the brief shield covers
  the rest.
- **Rooms by address.** No lobby screen or room list to build: a word
  shared among friends is the room.
- **The score kept by name.** Without accounts, the name is what a player
  is known by; a stranger taking a name that just left can take its
  score, which on a friends' server is a joke, not a threat.

## Pitfalls

- **The last to leave ends the match.** A match with no one left starts
  afresh, so a player alone who drops loses its score.
- **The ladder must be in the configuration.** A gun race server refuses
  to start without `gun_race` in `config.json`.

## Possible improvements

- Weapons dropped where a player falls.
- Bots to fill a match.
- A spectator's view while down.
