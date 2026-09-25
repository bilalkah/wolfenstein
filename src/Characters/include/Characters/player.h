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
#include "GameObjects/game_object.h"
#include "SoundManager/sound_manager.h"
#include "Strike/weapon.h"
#include <cstdint>
#include <functional>
#include <memory>

namespace wolfenstein {

class Camera2D;
class Scene;
class Ray;
// Player.h
class Player : public ICharacter, public IGameObject
{
  public:
	// Borrows the sound it plays, which outlives it
	Player(CharacterConfig& config, std::shared_ptr<Camera2D>& camera,
		   std::shared_ptr<Weapon> weapon, SoundManager& sound);

	void Update(double delta_time) override;

	void SetWeapon(std::shared_ptr<Weapon> weapon);
	// The player outlives levels; each level's scene hands itself over here
	// and stays valid until the next one does
	void EnterScene(Scene& scene) { scene_ = &scene; }
	void SetPose(const vector2d& pose) override;
	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;
	void SetPosition(const Position2D position) override;
	void IncreaseHealth(double amount) override;
	void DecreaseHealth(double amount) override;
	double GetHealth() const override;
	Position2D GetPosition() const override;
	int GetTextureId() const override;
	double GetWidth() const override;
	double GetHeight() const override;
	const Ray& GetCrosshairRay() const;
	bool IsDamaged() const;
	bool IsAlive() const;
	const Weapon& GetWeapon() const;
	const std::shared_ptr<Position2D>& GetPositionPtr();
	int GetDamageTextureId() const;
	// Opacity of the damage overlay, fading out after a hit
	std::uint8_t GetDamageAlpha() const { return damage_animation_.GetAlpha(); }

  private:
	void Move(double delta_time);
	void Rotate(double delta_time);
	void ShootOrReload();

	Scene* scene_ = nullptr;
	bool is_alive_{true};
	bool damaged_{false};
	double rotation_speed_{};
	double translation_speed_{};
	double width_{};
	double height_{};
	double health_{};
	double regen_time_{};
	SoundManager& sound_;
	SoundChannel sound_channel_;
	std::shared_ptr<Position2D> position_ptr_;
	std::shared_ptr<Camera2D> camera_;
	std::shared_ptr<Weapon> weapon_;
	TriggeredSingleAnimation damage_animation_;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_PLAYER_H
