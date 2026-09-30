# Prediction, lag and who decides

## Purpose

A message takes time to reach the server and come back: a few
milliseconds on a local network, 30 to 150 over the internet. If a
player's game waited for the server to say where its player went, every
move would lag by that round trip; if each game decided for itself, two
players would disagree about who shot whom. This page is about how the
work is split so that neither happens.

Code: `src/Client/src/match_client.cpp`, `src/Server/src/game_server.cpp`,
`Scene::SetJudging` and `Scene::HurtPlayer` in `src/Core/src/scene.cpp`.

## Concepts

### An authoritative server

The server runs the match; the players' games only send what their players
want to do (commands) and show what the server says happened. A cheating
game can lie about its commands but not about their results: it cannot
walk through a wall or say it hit. It is also the only way eight games
agree: there is one world that counts.

### Client-side prediction and reconciliation

A player's game runs the same simulation for its own player, applying each
command the moment it is made, and remembers where each command took it.
When the server's snapshot says where the player stood after command *n*,
the game compares that with what it foresaw for *n*: the same, nothing to
do; different (bumped by another player, a door the server saw closing),
it puts the player where the server had it and **plays the commands since
*n* again** from there. Most of the time the prediction is exact, since
both sides run the same code on the same commands.

### Entity interpolation

The other players are known only from snapshots, which come 30 times a
second, unevenly. Drawn where the newest snapshot has them, they would
jump. Each game instead shows them where the snapshots had them a little
in the past (**100 ms** here, six ticks), between the two snapshots round
that moment, so they always move smoothly between known places.

### Lag compensation

A shooter sees the others 100 ms late, and its shot takes half a round trip
to reach the server; by then the target has moved on. The server keeps
where everyone stood for the last half second and judges each shot
against the others **where the shooter's game showed them** when it fired.
A shot aimed true hits, whatever the delay; the price is that a target can
be hit a moment after ducking behind a wall, which players accept in
every such game.

## How it is implemented here

### Who decides what

| | The player's game | The server |
| --- | --- | --- |
| Its own player's moves | Foresees them, at once | Decides them |
| Its own shots | Shows them at once: the sound, the blood, the hit marker | Judges them, in hindsight |
| Health, dying, coming back | Takes them from snapshots | Decides them |
| Weapons and rounds carried | Foresees them (firing spends rounds), puts them right | Decides them |
| Pickups | Takes nothing itself; hides what the server says was taken | Decides who took what |
| The others | Shows them as the snapshots had them, 100 ms back | Moves them by their commands |

A scene that does not decide is **not judging** (`Scene::SetJudging`): a
player's game sets it on its level as it joins. Its own player's shots
still find the other players, and show blood and the hit marker at once,
but hurt no one:

```cpp title="src/Core/src/scene.cpp"
bool Scene::HurtPlayer(Player& victim, double damage, std::size_t by,
                       std::size_t weapon) {
    if (!victim.IsAlive() || victim.IsProtected() || damage <= 0.0) {
        return false;
    }
    if (!judging_) {
        return true;  // the server judges: only the marker shows, at once
    }
    victim.DecreaseHealth(damage);
    const auto slot = static_cast<std::uint8_t>(by);
    const auto other = static_cast<std::uint8_t>(victim.Slot());
    Record({.type = MatchEvent::Type::Hurt,
            .slot = slot,
            .other = other,
            .weapon = static_cast<std::uint8_t>(weapon),
            .at = victim.GetPose()});
    if (!victim.IsAlive()) {
        Record({.type = MatchEvent::Type::Kill,
                .slot = slot,
                .other = other,
                .weapon = static_cast<std::uint8_t>(weapon),
                .at = victim.GetPose()});
    }
    return true;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/60a190225b296a849c4d27c806ea9186e85a324b/src/Core/src/scene.cpp#L170-L194){ .excerpt-source }

### Commands, one a tick

Each tick, `MatchClient::BeforeTick` numbers the player's command and sends
it with the three before it, so a lost message costs no command (over
WebSocket, which is TCP, messages are not lost, but they can bunch up).
The server keeps each player's commands in a queue and applies one a tick.
When the next has not come, the player goes on as it was, moving and
holding the trigger, but not pressing a key again (`Repeated`); when
commands pile up after a stall, it is caught up to the last two, so what a
player does is never long behind what it sent:

```cpp title="src/Server/src/game_server.cpp"
PlayerCommand GameServer::NextCommand(Client& client) {
    // Far ahead (its commands came in a burst after a stall): caught up to
    // the last two, so what it does is never long behind what it sent
    if (client.queued > kMaxQueued) {
        constexpr std::size_t kKept = 2;
        client.head = (client.head + client.queued - kKept) % kQueue;
        client.queued = kKept;
    }
    if (client.queued > 0) {
        const Queued& next = client.queue[client.head];
        client.head = (client.head + 1) % kQueue;
        --client.queued;
        client.applied = next.numbered.sequence;
        client.last = next.numbered.command;
        client.seen = next.seen;
        return client.last;
    }
    // Late: it goes on as it was, its game a tick on
    client.last = Repeated(client.last);
    ++client.seen;
    return client.last;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/60a190225b296a849c4d27c806ea9186e85a324b/src/Server/src/game_server.cpp#L231-L252){ .excerpt-source }

### Putting the player right

The game remembers the last 128 commands, each with where it took the
player. A snapshot says which command the server applied last (`ack`) and
where the player stood then. Within a hundredth of a cell of the
foresight, nothing is done; otherwise the player is put there and the
remembered commands since are played again, their moves only (no shots, no
footsteps):

```cpp title="src/Client/src/match_client.cpp"
void MatchClient::Reconcile(World& world, const net::PlayerState& state,
                            std::uint32_t ack) {
    Player& player = world.GetPlayer();
    const Sent& at = history_[ack % kHistory];
    // What was foreseen for command `ack`; not remembered (none of ours
    // applied yet, too long ago, or in the last arena): where the player
    // stands now
    const bool remembered = ack != 0 && at.sequence == ack;
    const vector2d foreseen = remembered ? at.reached.pose : player.GetPose();
    if (foreseen.Distance(state.pose) <= kTolerance) {
        return;     // predicted right
    }
    ++corrections_;
    player.Correct(Position2D(state.pose, state.theta));
    // The commands since, those remembered, played again from there
    const std::uint32_t oldest =
        sequence_ >= kHistory ? sequence_ - kHistory + 1 : 1;
    for (std::uint32_t sequence = std::max(ack + 1, oldest);
         sequence <= sequence_; ++sequence) {
        Sent& sent = history_[sequence % kHistory];
        if (sent.sequence != sequence) {
            continue;
        }
        player.Replay(sent.command, net::kTickSeconds);
        sent.reached = player.GetPosition();
    }
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/60a190225b296a849c4d27c806ea9186e85a324b/src/Client/src/match_client.cpp#L387-L413){ .excerpt-source }

The same is done for what the player carries: its weapons and each one's
rounds are remembered after every command, and what the server's differ
from what was foreseen for `ack` (a gun picked up, a gun race's next
weapon, a respawn's fresh loadout) is added to what it carries now, and to
what the commands since foresaw, so the next snapshot is not put right
twice (`MatchClient::Restock`). Health and dying are not foreseen at all:
they come from the snapshots, the damage flash and the cry with them.

### The others, 100 ms back

Snapshots are kept in a ring of 32. After each tick,
`MatchClient::AfterTick` places every other player where the snapshots had
it six ticks before the newest, interpolating position, facing and pitch
between the two snapshots round that tick. These players are **puppets**:
their `Update` does nothing but play a fall, `Follow` sets where they are,
and each stride they cover plays a footstep from where they walk.

### Judging a shot in hindsight

Every command carries the server tick its game showed the others at
(`Input::seen`, for the newest command; each older one is a tick earlier).
After each tick the server remembers where every living player stood, for
30 ticks. The scene asks a `Hindsight` where to find the others when it
resolves a shot, and the server answers from that record:

```cpp title="src/Server/src/game_server.cpp"
void GameServer::Remember() {
    Places& places = past_[tick_ % kRewind];
    for (std::size_t slot = 0; slot < Scene::kMaxPlayers; ++slot) {
        const Player* player = world_->FindPlayer(slot);
        places[slot] = player != nullptr && player->IsAlive()
                           ? std::optional(player->GetPose())
                           : std::nullopt;
    }
}

std::optional<vector2d> GameServer::Seen(std::size_t shooter,
                                         std::size_t target) const {
    // No further back than remembered, and not ahead of now
    const std::uint32_t oldest =
        tick_ >= kRewind - 1 ? tick_ - (kRewind - 1) : 0;
    const std::uint32_t seen = std::clamp(seen_[shooter], oldest, tick_);
    return past_[seen % kRewind][target];
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/60a190225b296a849c4d27c806ea9186e85a324b/src/Server/src/game_server.cpp#L310-L327){ .excerpt-source }

A player not in the record then (not yet in, or already down) cannot be
hit; one down now cannot be hit either, wherever it was seen. Rockets and
plasma bolts are not compensated: they fly in the server's time and hit
what is there.

### Hitting a player

Enemies are hit where their picture is solid (a mask per frame). Players
are hit on a board facing the shooter, a quarter wider than their body,
from the floor to the top of the soldier's picture, and struck in the
head, body or legs by the same zones as the enemies (a headshot does
double damage). Pellets that find a player add up to one hurt, and one
kill. A player just come back is shielded for a moment and shows paler;
its shield goes as soon as it fires.

### What the others do

The server sends what happened each tick (**events**): shots, rockets
launched, players hurt and killed, pickups taken. A game plays the others'
shots from where it shows them, makes their figures fire, launches their
rockets (harmless there: the server judges where they burst), shows the
blood where a player was struck, lists the kills and hides the pickups
taken. Its own shots it has shown already.

### Measuring the round trip

Once the server has shown it answers pings (by sending everyone's), a game
sends a `Ping` with its own clock once a second; the server answers with a
`Pong` at once, not at its next tick. The game times the answer from when
it came in, stamped by the connection as it arrives, not from when the
next frame reads it, which would add up to a frame (16 ms at 60 frames a
second) to every measurement. With its next ping it tells the server the
round trip it measured, and the server sends every player's with the
scores.

## Design decisions and trade-offs

- **The server decides; the game foresees only its own player.** The
  others' moves cannot be foreseen (their commands are not known), and
  foreseeing hits would show kills that did not happen.
- **100 ms of interpolation.** Three snapshots' worth: one late snapshot
  does not leave the others stuck. Less would stutter over the internet;
  more would make them lag behind more than needed.
- **Lag compensation up to half a second.** Longer would let a player with
  a very slow connection hit others long after they took cover.
- **Commands sent four times.** Nearly free (a command is 10 bytes) and it
  keeps a stalled message from costing a command.

## Pitfalls

- **Both sides must run the same code.** A change to movement or
  collision that only one side has shows as constant corrections. The
  protocol's version turns away a game of another version.
- **Replaying plays the moves only.** A replayed command does not shoot or
  play footsteps again; anything else a command does must be left out of
  `Player::Replay` too.
- **A new arena clears what was foreseen.** The commands go on numbered as
  before (the server has them so), but where they took the player was in
  the last arena; the first snapshot there puts the player right once.

## Possible improvements

- Foresee pickups, so a medkit's health shows at once rather than a round
  trip later.
- Adapt the interpolation delay to each connection's jitter.
- Compensate projectiles too, by launching them where the shooter's game
  showed the world.
