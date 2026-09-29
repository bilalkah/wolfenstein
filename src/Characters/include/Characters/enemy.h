/**
 * @file enemy.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CHARACTERS_INCLUDE_ENEMY_H_
#define CHARACTERS_INCLUDE_ENEMY_H_

#include "Camera/ray.h"
#include "Characters/character.h"
#include "GameObjects/game_object.h"
#include "GameObjects/pickup.h"
#include "Math/vector.h"
#include "SoundManager/sound_manager.h"
#include "State/enemy_state.h"
#include "Strike/simple_weapon.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace wolfenstein {

class Scene;

// How an enemy type behaves
struct StateConfig
{
	double idle_frame_seconds{};  // how long each idle frame shows
	double follow_range{};		  // how near the player must be to be chased
	// A hit makes it flinch (its pain) at most once in this long; hits in
	// between only hurt it, so steady fire cannot keep it from shooting back
	double pain_cooldown_seconds{1.2};
	// Heard gunfire keeps it hunting the player this long, seen or not
	double alert_seconds{10.0};
	// Walking its patrol, it goes at this share of its hunting speed
	double patrol_pace{0.5};
	// Cells its cry carries when it is shot (Scene::MakeNoise): those near
	// it come hunting, however far away the shot was fired from
	int cry_range{6};
	// How far from the player it likes to fight, in sight of them: from
	// further than far_range it closes in, and from nearer than near_range
	// it backs away, facing them
	double near_range{0.0};
	double far_range{1.5};
	// How far it steps aside between shots, the player in its sights: hard
	// to hit, and hard to tell which way it goes next. 0 stands its ground.
	double sidestep{0.0};
	// Below this share of its health it breaks off, once, and runs for
	// cover out of the player's sight; 0 fights to the end
	double retreat_below{0.0};
};

// Where a shot strikes a figure changes what it does: the head is the top
// head_share of the figure (its visible part, in the frame shown), the legs
// the bottom leg_share, and the rest the body
struct HitZones
{
	double head_share = 0.2;
	double head_damage = 2.0;
	double leg_share = 0.45;
	double leg_damage = 0.6;

	enum class Zone : std::uint8_t { Head, Body, Legs };
	// The zone `down` the figure (0 at its top, 1 at its feet)
	Zone ZoneAt(double down) const {
		if (down < head_share) {
			return Zone::Head;
		}
		return down > 1.0 - leg_share ? Zone::Legs : Zone::Body;
	}
	// What a hit there does, as a share of the weapon's damage
	double Scale(Zone zone) const {
		switch (zone) {
			case Zone::Head:
				return head_damage;
			case Zone::Legs:
				return leg_damage;
			case Zone::Body:
				return 1.0;
		}
		return 1.0;
	}
};

// What an enemy type sounds like: shooting (or biting), its cry as it
// notices the player, hurt, and dying
struct EnemySounds
{
	SoundEffect attack = SoundEffect::NpcAttack;
	SoundEffect alert = SoundEffect::EnemyAlert;
	SoundEffect pain = SoundEffect::NpcPain;
	SoundEffect death = SoundEffect::NpcDeath;
};

// Something an enemy type may carry, and drop where it dies: a pickup, and
// the chance one of its kind carries it (before the difficulty's supplies
// scale it)
struct EnemyDrop
{
	std::string pickup;	 // "clip"
	double chance = 1.0;
};

// An enemy type as config.json describes it
struct EnemyConfig
{
	std::string type;  // names its animation clips: "<type>_walk"
	double translation_speed{};
	// Its picture's size, wide enough for every frame (lying dead, aiming
	// to the side)
	double width{};
	double height{};
	// Its body's, what bumps into things
	double radius{};
	double health = 100.0;	// before the difficulty scales it
	StateConfig behaviour;
	HitZones hit_zones;
	// What it may drop where it dies, each with its own chance
	std::vector<EnemyDrop> drops;
	SimpleWeaponConfig weapon;
	EnemySounds sounds;
};

// Pinned (not copyable or movable): its states point back to it. Lives in
// its scene's pool and borrows the scene (map, player, navigation), which
// outlives it.
class Enemy : public ICharacter, public IGameObject
{
  public:
	// Placed at `position`, as a level file spawns it
	Enemy(Scene& scene, const EnemyConfig& config, const Position2D& position);
	Scene& GetScene() { return scene_; }
	void Update(double delta_time) override;
	void TransitionTo(EnemyStateType type);
	EnemyStateType GetStateType() const;
	bool IsPlayerInShootingRange() const;
	bool IsAttacked() const;
	bool IsAlive() const;
	void Shoot();

	void SetNextPose(vector2d pose);
	void SetAttacked(bool value);
	// Takes the hit it was dealt, if any: true if it flinches, which it
	// does at most once every pain_cooldown_seconds, and always for the
	// killing hit
	bool TakeHit();
	// Out of its pain, it shoots back as soon as it can: true once after a
	// flinch
	bool TakeRetaliation();
	// Heard the player (a shot): it hunts them for alert_seconds, whether
	// or not it sees them
	void Alert();
	// The most pickups one enemy carries
	static constexpr std::size_t kMaxDrops = 4;
	// Gives it a pickup to carry, dropped where it dies: hidden until then
	// (Pickup::MakeDrop). False, and not carried, past kMaxDrops.
	bool AddDrop(Pickup& drop);
	std::span<Pickup* const> GetDrops() const {
		return std::span(drops_).first(drop_count_);
	}
	bool IsAlerted() const { return alerted_for_ > 0.0; }
	// Forgets what it heard: it found no way to it
	void LoseTrail() { alerted_for_ = 0.0; }
	// Whether, not yet hunting, it becomes aware of the player: it heard
	// gunfire, or it sees them near, on any side
	bool NoticesPlayer() const;
	// How near it notices a player it sees, and how far it hunts one it has
	// lost sight of: a little past its follow range
	double SightRange() const { return config_.behaviour.follow_range + 2.0; }
	// While it knows of no player it wanders within `radius` of where it
	// stands now (its post), to spots it sees from there: never through a
	// wall into the next room. 0 stands guard.
	void SetPatrolRadius(double radius);
	bool Patrols() const { return patrol_radius_ > 0.0; }
	double GetPatrolRadius() const { return patrol_radius_; }
	const vector2d& GetPost() const { return post_; }
	// Chooses the next spot to wander to: open floor within its radius, in
	// sight of its post, a little way from where it is; its post if no
	// spot will do. The choice is the same every run, from a generator of
	// its own.
	void PickWaypoint();
	const vector2d& Waypoint() const { return waypoint_; }
	// Its speed, as a share of its hunting speed: slower on patrol
	void SetPace(double pace) { pace_ = pace; }
	// Whether it is on its way somewhere this tick
	bool IsMoving() const { return !(next_pose == position_.pose); }
	// Standing guard: it looks one way, then another, about the way it
	// keeps watch (as it was placed)
	void LookAround();
	// Fighting, it keeps facing the player as it moves (backing away);
	// else it faces the way it walks. Cleared by every change of state.
	void SetFacePlayer(bool face) { face_player_ = face; }
	// Where to close in to on a player it sees: at its range, on its own
	// side of them, turned as far round from the others engaged as it can
	// be, so that a group spreads round the player rather than files up to
	// them. On open floor, in the player's sight and never in a doorway;
	// the player, if no such spot will do.
	vector2d ApproachSpot() const;
	// Standing in a doorway: it never stops there to fight, blocking the
	// way for the others
	bool InDoorway() const;
	// Another enemy engaged with the player stands at its elbow: at its
	// range it moves round to a side of its own
	bool IsBunched() const;
	// Where to back away to from a player too near: a step straight away,
	// or turned aside if a wall is behind, onto open floor in sight of the
	// player. Where it stands if there is none (its back to the wall).
	vector2d BackOffSpot() const;
	// After a shot: a spot its sidestep away, across the line to the
	// player, onto open floor still in sight of them. Mostly the other side
	// from last time; the nearer side or a half step where a wall is in
	// the way; none if it cannot step aside at all (or does not).
	void PlanSidestep();
	// Stepping aside, for a moment at most (a step it cannot finish)
	bool IsSidestepping() const { return sidestep_left_ > 0.0; }
	const vector2d& SidestepSpot() const { return sidestep_to_; }
	void EndSidestep() { sidestep_left_ = 0.0; }
	// Badly hurt and not yet done hiding (Retreat): it runs for cover, or
	// back to it after a flinch on the way
	bool WantsToRetreat() const;
	// Chooses where to hide: the nearest spot it can reach, on open floor
	// out of the player's sight and not towards them. False if there is
	// none, and then it is done with retreating: it fights on.
	bool FindCover();
	const vector2d& Cover() const { return cover_; }
	// Done hiding, or found, or cornered: it fights to the end now
	void EndRetreat() { retreat_ = Retreat::Done; }
	void SetDeath();
	// Lying dead as a saved game left it: straight to the end of its death,
	// in silence
	void RestoreDead();
	// Not engaged with the player: standing guard, walking its patrol, or
	// dead and down
	bool IsCalm() const;
	// Solid while it stands: once shot down it can be walked over
	double GetCollisionRadius() const override {
		return is_alive_ && health_ > 0.0 ? radius_ : 0.0;
	}
	// Its body's radius, dead or alive
	double GetRadius() const { return radius_; }
	// Which of its 8 views someone at `viewer` sees: 0 its front, going
	// round from its front-left to 7, its front-right (4 its back)
	std::size_t ViewFrom(const vector2d& viewer) const;
	// Alive, the frame for the side the viewer sees. Falling and dead there
	// is one frame, drawn from its front: it lies across the way it faced
	// (the player who killed it), so from behind it is the mirror image,
	// and from its head or its feet it is narrow. It stays put, then, as the
	// viewer walks round it.
	Appearance SeenFrom(const vector2d& viewer) const override;
	// Turns it towards the player (to shoot)
	void FacePlayer();
	// One of the enemies a level's objective asks the player to kill
	bool IsTarget() const { return target_; }
	void SetTarget(bool target) { target_ = target; }
	void SetPose(const vector2d& pose) override;
	void SetPosition(const Position2D position) override;
	void IncreaseHealth(double amount) override;
	void DecreaseHealth(double amount) override;
	double GetHealth() const override;

	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;
	vector2d GetRenderPose(double alpha) const override;
	const Position2D& GetPosition() const override { return position_; }
	const std::string& GetBotName() const;
	// Plays on the enemy's own channel, cutting off its previous sound
	void PlaySound(SoundEffect effect);
	int GetTextureId() const override;
	double GetWidth() const override;
	double GetHeight() const override;
	const StateConfig& GetStateConfig() const { return config_.behaviour; }
	const EnemySounds& GetSounds() const { return config_.sounds; }
	const HitZones& GetHitZones() const { return config_.hit_zones; }
	const Ray& GetCrosshairRay() const;
	const SimpleWeapon& GetWeapon() const;

  private:
	void Move(double delta_time);
	// Moves it by `step` as far as the walls let it, sliding along them
	void Slide(const vector2d& step);
	// Eases it out of any other living enemy it stands in
	void KeepApart(double delta_time);
	// Open floor, off the walls by more than a body's width
	bool IsOpenFloor(const vector2d& at) const;
	// Its own generator's next number, in [0, 1): the same run after run
	double NextRandom();

	Scene& scene_;
	bool is_attacked_{};
	bool retaliating_{};
	double since_flinch_{1e9};	// seconds since it last flinched
	double alerted_for_{};		// seconds still to hunt what it heard
	std::array<Pickup*, kMaxDrops> drops_{};
	std::size_t drop_count_{};
	bool is_alive_{};
	bool silent_{};	 // while being restored
	bool target_{};
	double translation_speed_{};
	double width{};
	double height{};
	double radius_{};
	double pace_{1.0};
	bool face_player_{};
	vector2d sidestep_to_;
	double sidestep_left_{};  // seconds it may still take to get there
	double sidestep_side_{1.0};
	// Running for cover, once in its life
	enum class Retreat : std::uint8_t { None, Running, Done };
	Retreat retreat_{Retreat::None};
	vector2d cover_;
	double max_health_{};
	vector2d post_;
	double patrol_radius_{};
	vector2d waypoint_;
	std::uint32_t random_{1};  // xorshift state, never 0
	double watch_theta_{};	   // the way it keeps watch
	std::size_t look_{};	   // which way it looks, of its turns
	double health_{};
	Position2D position_;
	vector2d next_pose;
	vector2d previous_pose_;
	// Borrowed from the game config, which outlives every enemy
	const EnemyConfig& config_;
	Ray crosshair_ray;
	EnemyState& StateFor(EnemyStateType type);
	// The direction to `viewer`, turned from the way it faces: 0 straight
	// ahead, -pi to pi
	double TurnedFrom(const vector2d& viewer) const;

	// Every state the enemy can be in, set up once: transitions allocate
	// nothing
	IdleState idle_state_;
	PatrolState patrol_state_;
	WalkState walk_state_;
	AttackState attack_state_;
	PainState pain_state_;
	DeathState death_state_;
	RetreatState retreat_state_;
	StateMachine<EnemyState> state_machine_;
	SimpleWeapon weapon_;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_ENEMY_H_
