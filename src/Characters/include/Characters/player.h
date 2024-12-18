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
#include "Strike/weapon.h"
#include <functional>
#include <memory>

namespace wolfenstein {

class Camera2D;
class Ray;
// Player.h
class Player : public ICharacter, public IGameObject
{
  public:
	explicit Player(CharacterConfig& config, std::shared_ptr<Camera2D>& camera);
	~Player() = default;

	void Update(double delta_time) override;

	void SetWeapon(std::shared_ptr<Weapon> weapon);
	void SetPose(const vector2d& pose) override;
	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;
	void SetPosition(const Position2D position) override;
	void IncreaseHealth(double amount) override;
	void DecreaseHealth(double amount) override;
	double GetHealth() const override;
	Position2D GetPosition() const override;
	std::string GetId() const override;
	int GetTextureId() const override;
	double GetWidth() const override;
	double GetHeight() const override;
	const Ray& GetCrosshairRay() const;
	bool IsDamaged() const;
	bool IsAlive() const;
	const Weapon& GetWeapon() const;
	const std::shared_ptr<Position2D>& GetPositionPtr();
	int GetDamageTextureId() const;

  private:
	void Move(double delta_time);
	void Rotate(double delta_time);
	void ShootOrReload();

	bool is_alive_{true};
	bool damaged_{false};
	double rotation_speed_;
	double translation_speed_;
	double width_;
	double height_;
	double health_;
	std::string id_;
	std::shared_ptr<Position2D> position_ptr_;
	std::shared_ptr<Camera2D> camera_;
	std::shared_ptr<Weapon> weapon_;
	std::unique_ptr<TriggeredSingleAnimation> damage_animation_ptr_;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_PLAYER_H
