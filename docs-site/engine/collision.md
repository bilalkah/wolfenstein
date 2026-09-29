# Collision and movement

## Purpose

Collision keeps bodies where they may be: out of walls and closed doors,
and out of each other and of solid things like lamps. It also decides how a
blocked body *moves on*: sliding along a wall or round an enemy instead of
stopping dead, which is what makes movement feel smooth.

Code: `src/CollisionManager/` and the `Move` functions of the characters
(`Player::Move`, `Enemy::Move`, `Enemy::Slide`, `Enemy::KeepApart`).

## Concepts

### Two shapes for two jobs

- Against the **grid**, a body is an axis-aligned **square** of half-width
  \(r\). Squares against grid cells are cheap and exact: a square overlaps
  a wall cell if one of its corners is in it (for squares smaller than a
  cell).
- Against **other bodies** (enemies, the player, lamps), a body is a
  **circle**. Two circles of radii \(r_1, r_2\) overlap when their centres
  are closer than \(r_1 + r_2\), and pushing one out along the line between
  the centres is the smallest correction.

### Sliding

A body moving diagonally into a wall should keep the part of its motion
along the wall. Testing the two axes **separately** does that for free:
move in \(x\) if \(x\) alone is clear, then in \(y\) if \(y\) alone is
clear. A body pressing into a wall on its \(x\) side keeps moving in \(y\).
For circles, pushing the end point out of the other body onto its edge
keeps the tangential part of the step, so the body slides round it.

## How it is implemented here

### Walls: the leading edge

```cpp title="src/CollisionManager/src/collision_manager.cpp"
bool CheckWallCollision(const Map& map, const vector2d& pose,
                        const vector2d& delta_pose, double radius) {
    // The body is a square `radius` each way from its centre. The
    // edge it moves towards, where the move ends, must be clear at both its
    // corners: checking only its middle let a body cut into a wall's corner
    // coming at it at an angle. Only that edge is checked, so a body can
    // always move away from a wall it touches. The corners stand in a hair,
    // so a body flush with a wall along its side still slides along it.
    const double side = radius * 0.99;
    const vector2d end = pose + delta_pose;
    const auto clear = [&](double x, double y) {
        return !map.IsBlocked(vector2d{x, y});
    };
    if (delta_pose.x != 0.0) {
        const double edge = end.x + std::copysign(radius, delta_pose.x);
        if (!clear(edge, end.y - side) || !clear(edge, end.y + side)) {
            return true;
        }
    }
    if (delta_pose.y != 0.0) {
        const double edge = end.y + std::copysign(radius, delta_pose.y);
        if (!clear(end.x - side, edge) || !clear(end.x + side, edge)) {
            return true;
        }
    }
    return false;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/CollisionManager/src/collision_manager.cpp#L6-L32){ .excerpt-source }

Three details carry the design:

1. Only the edge the body moves **towards** is tested, at both its corners.
   Testing the centre only let a body cut into a wall's corner at an angle;
   testing the whole square would stop a body from ever leaving a wall it
   touches.
2. The corners are pulled in by 1% (`side = radius * 0.99`), so a body
   flush against a wall along its side is not "in" it and can slide.
3. `IsBlocked` treats doors less than 80% open and sliding secrets as
   walls, so doors and secrets need no collision code of their own.

### Bodies: pushed out, round

```cpp title="src/CollisionManager/src/collision_manager.cpp"
vector2d PushOutOf(const vector2d& centre, double solid, const vector2d& from,
                   const vector2d& to, double radius) {
    const double reach = radius + solid;
    const double after = centre.Distance(to);
    if (after >= reach || after >= centre.Distance(from)) {
        return to;    // clear of it, or moving away from it
    }
    // Out along the line from its centre; if the step ends on the centre,
    // back the way it came
    vector2d away = after > 1e-9 ? to - centre : from - centre;
    const double length = std::hypot(away.x, away.y);
    if (length < 1e-9) {
        return from;
    }
    return centre + away * (reach / length);
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/CollisionManager/src/collision_manager.cpp#L34-L49){ .excerpt-source }

A step that ends inside another body is moved out onto its edge, straight
away from its centre. A step **away** from a body (the end further from
its centre than the start) is let through even if it overlaps, so nothing
can get stuck where it stands. `ResolveObjectCollisions` applies this to
every solid object in the level, twice: pushed out of one thing, a body can
end up in the next one beside it.

### The player's move

```cpp title="src/Characters/src/player.cpp"
void Player::Move(double delta_time) {
    const double speed = translation_speed_ * delta_time;
    const vector2d facing{std::cos(position_.theta), std::sin(position_.theta)};
    const vector2d right{-facing.y, facing.x};
    // Forward and sideways at once, no faster than either alone
    vector2d wish = facing * command_.forward + right * command_.strafe;
    const double length = wish.Magnitude();
    if (length > 1.0) {
        wish = wish / length;
    }
    const vector2d delta_movement = wish * speed;
    // As far as it can go: out of the living enemies and lamps it meets
    // (sliding round them), then an axis at a time against the walls
    // (sliding along them)
    const Map& map = scene_->GetMap();
    const vector2d reached =
        ResolveObjectCollisions(scene_->GetObjects(), this, position_.pose,
                                position_.pose + delta_movement, width_ / 2);
    const vector2d step = reached - position_.pose;
    const vector2d before = position_.pose;
    if (!CheckWallCollision(map, position_.pose, {step.x, 0}, width_ / 2)) {
        position_.pose.x += step.x;
    }
    if (!CheckWallCollision(map, position_.pose, {0, step.y}, width_ / 2)) {
        position_.pose.y += step.y;
    }
    // A footstep every stride walked, one foot then the other
    constexpr double kStride = 0.9;
    walked_ += position_.pose.Distance(before);
    if (walked_ >= kStride) {
        walked_ -= kStride;
        left_foot_ = !left_foot_;
        sound_.PlayEffect(step_channel_, left_foot_ ? SoundEffect::StepLeft
                                                    : SoundEffect::StepRight);
    }
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/src/player.cpp#L288-L323){ .excerpt-source }

The wish vector (forward plus strafe) is normalised when both are held, so
diagonal movement is not \(\sqrt{2}\) times faster. Then bodies first,
walls second, one axis at a time.

### Enemies

An enemy moves the same way (`Enemy::Move`), with two differences.

- It is pushed out of lamps and the player, but **not out of other
  enemies**: a group pushing on each other would jam in a doorway.
  Instead, each tick, an enemy standing in another eases out of it:

    ```cpp title="src/Characters/src/enemy.cpp"
    void Enemy::KeepApart(double delta_time) {
        // Standing in another, it eases out of it, a little each tick: a group
        // does not stand on one spot, and two can still squeeze past each other
        // in a doorway (a wall stops the easing, not them)
        constexpr double kEasePerSecond = 4.0;
        vector2d apart{0.0, 0.0};
        for (const Enemy* other : scene_.GetEnemies()) {
            if (other == this || other->GetCollisionRadius() <= 0.0) {
                continue;
            }
            const vector2d gap = position_.pose - other->GetPose();
            const double distance = gap.Magnitude();
            const double overlap = radius_ + other->GetRadius() - distance;
            if (overlap > 0.0) {
                // Exactly on it: out along the way each faces, so they part
                const vector2d away = distance > 1e-6
                                          ? gap / distance
                                          : vector2d{std::cos(position_.theta),
                                                     std::sin(position_.theta)};
                apart = apart + away * (overlap / 2);
            }
        }
        if (apart.x != 0.0 || apart.y != 0.0) {
            // Standing, it stands where it was eased to; walking, it goes on
            const bool standing = next_pose == position_.pose;
            Slide(apart * std::min(kEasePerSecond * delta_time, 1.0));
            if (standing) {
                next_pose = position_.pose;
            }
        }
    }
    ```
    [View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/src/enemy.cpp#L203-L233){ .excerpt-source }

- Each enemy type has its own body radius (from `config.json`), separate
  from its picture's width, which must be wide enough for every frame
  (lying dead, aiming to the side).

A dead enemy's `GetCollisionRadius()` is 0: it can be walked over. Pickups
and effects have radius 0 too; lamps have the radius set for them in
`config.json`.

## Design decisions and trade-offs

- **Squares for walls, circles for bodies.** Each shape is the cheap,
  exact choice against what it collides with.
- **No physics engine, no velocities.** Bodies move by a wished step each
  tick and collision trims it; there is no momentum, friction or
  knock-back. It suits a game of this kind and keeps the simulation
  deterministic and allocation-free.
- **Soft separation between enemies.** Easing apart over time instead of
  hard collision lets crowds flow through doorways, at the price of
  enemies briefly overlapping.

## Pitfalls

- **Tunnelling.** A step longer than a body's size could skip over a thin
  obstacle. Walls are whole cells and steps at 60 Hz are short (the
  player's speed times 1/60 s), so it does not happen for walking;
  projectiles, which are fast, are moved in short steps for that reason.
- **Order of resolution.** Bodies are resolved before walls: a push out of
  an enemy can point into a wall, and then the wall test simply refuses
  that axis. Rarely, the result is a step shorter than it could have been.
- **The two passes are a heuristic.** Three bodies packed round the player
  could, in theory, need more passes; two settle every case the game
  produces.

## Possible improvements

- A spatial grid of bodies (bucketed by cell) would make body checks
  \(O(\text{neighbours})\) instead of \(O(\text{objects})\) per mover;
  with levels of about twenty enemies and a few dozen other objects it is
  not needed yet.
- Knock-back from explosions (the rocket's splash already computes
  distance and falloff) would need a velocity per body.
