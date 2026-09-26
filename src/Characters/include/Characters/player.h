/**
 * @file player.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-18
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CHARACTERS_PLAYER_H
#define CHARACTERS_PLAYER_H

#include "Animation/triggered_single_animation.h"
#include "Characters/character.h"
#include "Characters/player_command.h"
#include "GameObjects/game_object.h"
#include "GameObjects/pickup.h"
#include "SoundManager/sound_manager.h"
#include "Strike/weapon.h"
#include <cstdint>
#include <functional>
#include <memory>

namespace wolfenstein {

class Scene;
struct Ray;
// Player.h
class Player : public ICharacter, public IGameObject
{
  public:
	// Borrows the sound it plays, which outlives it
	// Carries a weapon built from `weapon`, in place. Borrows the textures
	// and sound, which outlive it.
	Player(CharacterConfig& config, const WeaponConfig& weapon,
		   const TextureManager& textures, SoundManager& sound);
	// Pinned: its weapon's states point back to the weapon inside it
	Player(const Player&) = delete;
	Player& operator=(const Player&) = delete;
	Player(Player&&) = delete;
	Player& operator=(Player&&) = delete;
	~Player() override = default;

	void Update(double delta_time) override;

	// The player outlives levels; each level's scene hands itself over here
	// and stays valid until the next one does
	void EnterScene(Scene& scene) { scene_ = &scene; }
	// What to do from the next update on; the player reads no input device
	void SetCommand(const PlayerCommand& command);
	void SetPose(const vector2d& pose) override;
	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;
	void SetPosition(const Position2D position) override;
	void IncreaseHealth(double amount) override;
	void DecreaseHealth(double amount) override;
	double GetHealth() const override;
	const Position2D& GetPosition() const override { return position_; }
	int GetTextureId() const override;
	double GetWidth() const override;
	double GetHeight() const override;
	// Takes what a pickup gives; false, leaving it lying, if the player has
	// no use for it (full health, a full reserve)
	bool TryPickUp(const PickupEffect& effect);
	bool IsDamaged() const;
	bool IsAlive() const;
	const Weapon& GetWeapon() const;
	// Where to draw the view `alpha` of the way from the previous tick
	Position2D GetRenderPosition(double alpha) const;
	// Opacity of the damage overlay, fading out after a hit
	std::uint8_t GetDamageAlpha() const { return damage_animation_.GetAlpha(); }
	// Opacity of the flash after taking a pickup, 0 when none is showing
	std::uint8_t GetPickupAlpha() const {
		return picked_up_ ? pickup_animation_.GetAlpha() : 0;
	}

  private:
	void Move(double delta_time);
	void Rotate(double delta_time);
	void ShootOrReload();

	Scene* scene_ = nullptr;
	PlayerCommand command_;
	bool is_alive_{true};
	bool damaged_{false};
	double translation_speed_{};
	double width_{};
	double height_{};
	double health_{};
	SoundManager& sound_;
	SoundChannel sound_channel_;
	Position2D position_;
	Position2D previous_position_;
	Weapon weapon_;
	TriggeredSingleAnimation damage_animation_;
	bool picked_up_{false};
	TriggeredSingleAnimation pickup_animation_;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_PLAYER_H
