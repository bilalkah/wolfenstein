# Atomics and a lock-free ring

## The technique

When exactly one thread writes items and exactly one other thread reads
them, a **single-producer, single-consumer (SPSC) ring buffer** needs no
lock:

- a fixed array of slots, and two counters: how many items were written
  (`sent`) and how many read (`taken`);
- the producer writes the item into slot `sent % N`, then **publishes** it
  by storing `sent + 1` with *release* ordering;
- the consumer loads `sent` with *acquire* ordering, which guarantees it
  sees every item written before that store, reads them, then stores
  `taken` with release ordering, telling the producer those slots are free.

With free-running unsigned counters, `sent - taken` is the number of items
in flight even after the counters wrap round (unsigned arithmetic is
modulo \(2^{32}\)), as long as \(N\) divides \(2^{32}\): a power of two.
Neither side ever waits; the producer gives up (or retries) when the ring
is full.

The memory orders are the essential part: without release/acquire, the
consumer could see the new counter before the item's contents (the
compiler or the CPU may reorder the writes).

## Where it appears here

The spatial mixer (see [Audio](../engine/audio.md)) receives commands from
the game thread ("play this sound from here", "the listener moved") and
applies them on the audio thread, which owns the voices:

```cpp title="src/SoundManager/src/spatial_mixer.cpp"
bool SpatialMixer::Send(const Command& command) {
    const std::uint32_t sent = sent_.load(std::memory_order_relaxed);
    if (sent - taken_.load(std::memory_order_acquire) == kCommands) {
        return false;
    }
    commands_[sent % kCommands] = command;
    sent_.store(sent + 1, std::memory_order_release);
    return true;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/SoundManager/src/spatial_mixer.cpp#L60-L68){ .excerpt-source }

- `sent_` is loaded **relaxed** by its only writer: a thread always sees
  its own latest store.
- `taken_` is loaded **acquire**: the slot being reused was fully read
  before the consumer released it.
- `sent_` is stored **release** after the command is written into its
  slot.

The consumer, at the top of `SpatialMixer::Mix`, loads `sent_` with
acquire, takes every command up to it, and stores `taken_` with release.
The ring holds 256 commands (`kCommands`, "a power of two: many frames'
commands between two mixes"). The effects volume is a separate
`std::atomic<float>`, loaded relaxed: a slightly late volume change is
harmless.

## Why not a mutex

The audio callback must never block: if it waits for the game thread
(which might be mid-frame), the sound glitches. A mutex would also make
the game wait for the audio thread. The ring makes both sides wait-free.
On the web the audio callback happens to run on the main thread, so there
is no concurrency at all; the same code works either way.

## Pitfalls

- **Exactly one producer and one consumer.** Two threads calling `Send`
  would race on `sent_`. Only the game thread sends.
- **A full ring loses commands** (`Send` returns false). 256 is far more
  than the game sends between two mixes.
- The `Command` is copied into its slot with ordinary stores; that is safe
  only because the release store of `sent_` comes after it.
