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
#include <cstddef>
#include <cstdint>
#include <string>
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
	// The pickup it drops where it dies ("clip"), if any
	std::string drop;
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
	// The pickup it drops where it dies, if it carries one: hidden until
	// then (Pickup::MakeDrop)
	void SetDrop(Pickup& drop) { drop_ = &drop; }
	bool IsAlerted() const { return alerted_for_ > 0.0; }
	// Forgets what it heard: it found no way to it
	void LoseTrail() { alerted_for_ = 0.0; }
	// Whether, not yet hunting, it becomes aware of the player: it heard
	// gunfire, or it sees them near, on any side
	bool NoticesPlayer() const;
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

	Scene& scene_;
	bool is_attacked_{};
	bool retaliating_{};
	double since_flinch_{1e9};	// seconds since it last flinched
	double alerted_for_{};		// seconds still to hunt what it heard
	Pickup* drop_ = nullptr;
	bool is_alive_{};
	bool silent_{};	 // while being restored
	bool target_{};
	double translation_speed_{};
	double width{};
	double height{};
	double radius_{};
	double pace_{1.0};
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
	SoundChannel sound_channel_;
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
	StateMachine<EnemyState> state_machine_;
	SimpleWeapon weapon_;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_ENEMY_H_
