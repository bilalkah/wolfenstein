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

	// What a hit `down` the figure (0 at its top, 1 at its feet) does, as a
	// share of the weapon's damage
	double Scale(double down) const {
		if (down < head_share) {
			return head_damage;
		}
		return down > 1.0 - leg_share ? leg_damage : 1.0;
	}
};

// An enemy type as config.json describes it
struct EnemyConfig
{
	std::string type;  // names its animation clips: "<type>_walk"
	double translation_speed{};
	double width{};
	double height{};
	double health = 100.0;	// before the difficulty scales it
	StateConfig behaviour;
	HitZones hit_zones;
	// The pickup it drops where it dies ("clip"), if any
	std::string drop;
	SimpleWeaponConfig weapon;
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
	void SetDeath();
	// Lying dead as a saved game left it: straight to the end of its death,
	// in silence
	void RestoreDead();
	// Not engaged with the player: standing idle, or dead and down
	bool IsCalm() const;
	// Solid while it stands: once shot down it can be walked over
	double GetCollisionRadius() const override {
		return is_alive_ && health_ > 0.0 ? width / 2 : 0.0;
	}
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
	double health_{};
	Position2D position_;
	vector2d next_pose;
	vector2d previous_pose_;
	// Borrowed from the game config, which outlives every enemy
	const EnemyConfig& config_;
	SoundChannel sound_channel_;
	Ray crosshair_ray;
	EnemyState& StateFor(EnemyStateType type);

	// Every state the enemy can be in, set up once: transitions allocate
	// nothing
	IdleState idle_state_;
	WalkState walk_state_;
	AttackState attack_state_;
	PainState pain_state_;
	DeathState death_state_;
	StateMachine<EnemyState> state_machine_;
	SimpleWeapon weapon_;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_ENEMY_H_
