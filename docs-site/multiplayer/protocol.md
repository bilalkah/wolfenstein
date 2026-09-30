# The protocol

## Purpose

What the server and its players say to each other, and how. Each message
is one WebSocket binary frame: a type byte, then its fields,
little-endian, positions and angles packed into whole numbers. Reading and
writing a message allocates nothing, and a message that makes no sense is
refused whole.

Code: `src/Net/` (`protocol.h`, `bytes.h`, `connection.h`),
`server/main.cpp`.

## The messages

| Message | Way | When | What it carries |
| --- | --- | --- | --- |
| `Hello` | player → server | first | The protocol's version, the player's name (16 characters at most) |
| `Welcome` | server → player | in answer, and at each new arena | The player's slot (0 to 7), the server's tick, the arena's level file |
| `Reject` | server → player | in answer, then closed | Why: another version, or every slot taken |
| `Input` | player → server | every tick | The newest command and the three before it, numbered; the server tick the player saw the others at |
| `Snapshot` | server → player | every other tick | Each player's slot, whether alive and shielded, place, facing, pitch, health and weapon in hand; the last command of the receiver's applied; what the receiver carries; which pickups are gone |
| `Events` | server → players | each tick something happens | Shots, rockets launched, hurts, kills and pickups taken, in order (40 at most a message) |
| `Scores` | server → players | on a change, and every second | The mode, the phase (playing, or the result showing), the frag limit, the seconds left, the winner; each player's name, frags, deaths and gun race step |
| `Ping` | player → server | every second | The player's clock, and the last round trip it measured |
| `Pong` | server → player | at once | The ping's clock, sent back |
| `Pings` | server → players | with the scores | Each player's round trip, as it told the server |

A full snapshot of eight players is under 140 bytes; an input is 50.

### Packing

A position is a 256th of a cell in 16 bits (levels up to 256 cells
across), an angle a 65536th of a turn, the pitch a share of its range in a
signed 16-bit number. The snapshot packs whether a player is alive and
shielded into the top bits of its slot's byte. What the player gets back
is within half a step of what the server had (the tests check it), and
both sides round the same way:

```cpp title="src/Net/src/protocol.cpp"
std::uint16_t PackPosition(double coordinate) {
    return static_cast<std::uint16_t>(
        std::clamp(std::lround(coordinate * kPositionScale), 0L, 0xFFFFL));
}
double UnpackPosition(std::uint16_t packed) {
    return packed / kPositionScale;
}
std::uint16_t PackAngle(double theta) {
    const double turns = theta / kTurn - std::floor(theta / kTurn);
    return static_cast<std::uint16_t>(std::lround(turns * 65536.0) & 0xFFFF);
}
double UnpackAngle(std::uint16_t packed) {
    return packed / 65536.0 * kTurn;
}
std::int16_t PackPitch(double pitch) {
    return static_cast<std::int16_t>(
        std::lround(std::clamp(pitch / kPitchRange, -1.0, 1.0) * 32767.0));
}
double UnpackPitch(std::int16_t packed) {
    return packed / 32767.0 * kPitchRange;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/60a190225b296a849c4d27c806ea9186e85a324b/src/Net/src/protocol.cpp#L18-L38){ .excerpt-source }

### Refusing what makes no sense

`ByteReader` reads past the end as zeros and remembers it failed; `Decode`
then refuses the message, as it does one with bytes left over, an unknown
type, a count larger than the message may hold, or a slot past the last.
The server closes a connection that sends something that is not the
protocol; a player's game passes it by:

```cpp title="src/Net/src/protocol.cpp"
std::optional<Message> Decode(std::span<const std::uint8_t> data) {
    ByteReader reader(data);
    const std::uint8_t type = reader.U8();
    if (!reader.Ok() || type < std::to_underlying(MessageType::Hello) ||
        type > std::to_underlying(MessageType::Pings)) {
        return std::nullopt;
    }
    auto message = ReadBody(static_cast<MessageType>(type), reader);
    if (!message || !reader.Ok() || reader.Remaining() != 0) {
        return std::nullopt;
    }
    return message;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/60a190225b296a849c4d27c806ea9186e85a324b/src/Net/src/protocol.cpp#L409-L421){ .excerpt-source }

### Growing without breaking

Both sides carry the protocol's version (2); the server turns away a game
of another version with a `Reject` that says so, and the game shows it. A
message added since (`Ping`, `Pong`, `Pings`) does not need a new version:
a game that does not know it passes it by, and a game pings only a server
that has sent `Pings`, so older games and servers play on with newer ones.
Changing what an existing message holds does need a new version.

## Carrying the messages

| Side | WebSocket | How messages reach the game |
| --- | --- | --- |
| Server | [uWebSockets](https://github.com/uNetworking/uWebSockets) (on uSockets, built without TLS or compression) | Its event loop calls back for each message; a timer ticks the matches 60 times a second |
| Browser | The browser's own, through `emscripten/websocket.h` | Callbacks between frames, on the page's thread |
| Native game | [IXWebSocket](https://github.com/machinezone/IXWebSocket) | Callbacks on a thread of its own |

Whichever way, a message that comes in is copied into the connection's
**inbox**, a fixed ring of 64 slots of 512 bytes (the most a message
takes), with the time it came; the game takes them at the start of each
frame. Nothing is allocated, and with every slot full, what comes is
dropped (the game empties it every frame):

```cpp title="src/Net/src/connection.cpp"
bool Inbox::Put(std::span<const std::uint8_t> message, double arrived) {
    const std::scoped_lock lock(mutex_);
    if (message.size() > kMaxMessage || count_ == kSlots) {
        return false;
    }
    const std::size_t slot = (head_ + count_) % kSlots;
    std::ranges::copy(message, slots_[slot].begin());
    sizes_[slot] = message.size();
    arrived_[slot] = arrived;
    ++count_;
    return true;
}

std::optional<Received> Inbox::Take(std::span<std::uint8_t> out) {
    const std::scoped_lock lock(mutex_);
    if (count_ == 0) {
        return std::nullopt;
    }
    const std::size_t size = sizes_[head_];
    const std::size_t slot = head_;
    head_ = (head_ + 1) % kSlots;
    --count_;
    if (size > out.size()) {
        return std::nullopt;  // no room for it where it was asked for
    }
    std::copy_n(slots_[slot].begin(), size, out.begin());
    return Received{.size = size, .arrived = arrived_[slot]};
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/60a190225b296a849c4d27c806ea9186e85a324b/src/Net/src/connection.cpp#L14-L41){ .excerpt-source }

The server's side closes a connection only once the game is done with the
message or tick that asked for it (`Sockets::CloseAsked`): uWebSockets
closes at once and would tell the game so while it was still busy. A
connection silent for 30 seconds is closed; one whose sends pile up past
64 KB is closed rather than let grow.

## Design decisions and trade-offs

- **WebSocket, not WebRTC or raw UDP.** Browsers can open WebSockets to
  any server; UDP needs WebRTC, with its signalling and TURN servers. A
  WebSocket is TCP, so a lost packet delays the ones behind it, which the
  commands sent four times and the interpolation delay absorb; on a good
  connection the difference does not show.
- **Libraries for the WebSocket.** The handshake, framing, masking and
  TLS are the same in every game; uWebSockets and IXWebSocket are small,
  fast and used widely, and the game's code starts where the bytes do.
- **Binary and hand-packed.** A snapshot is under 140 bytes 30 times a
  second, each field where the code says; no schema compiler, no
  reflection, no allocation.

## Pitfalls

- **A field added to an existing message needs a new version.** Older
  games would refuse the message (its length would be wrong); the version
  makes them say why.
- **Positions stop at 256 cells.** An arena larger than that needs a
  wider position.
