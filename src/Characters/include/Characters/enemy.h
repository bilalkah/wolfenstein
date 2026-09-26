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
};

// An enemy type as config.json describes it
struct EnemyConfig
{
	std::string type;  // names its animation clips: "<type>_walk"
	double translation_speed{};
	double width{};
	double height{};
	StateConfig behaviour;
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
	void SetDeath();
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
	const Ray& GetCrosshairRay() const;
	const SimpleWeapon& GetWeapon() const;

  private:
	void Move(double delta_time);

	Scene& scene_;
	bool is_attacked_{};
	bool is_alive_{};
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
