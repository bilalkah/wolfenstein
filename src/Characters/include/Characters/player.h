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

#include "Characters/character.h"
#include "GameObjects/game_object.h"
#include "Strike/weapon.h"
#include <functional>
#include <memory>

namespace wolfenstein {

// Player.h
class Player : public ICharacter, public IGameObject
{
  public:
	explicit Player(CharacterConfig& config);
	~Player() = default;

	void Update(double delta_time) override;

	void SetPose(const vector2d& pose) override;
	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;
	void SetPosition(Position2D position) override;
	Position2D GetPosition() const override;
	std::string GetId() const override;
	int GetTextureId() const override;
	double GetWidth() const override;
	double GetHeight() const override;
	void SubscribeToPlayerPosition(std::function<void(Position2D)> subscriber);

  private:
	void Move(double delta_time);
	void Rotate(double delta_time);
	void ShootOrReload();

	Position2D position_;
	double rotation_speed_;
	double translation_speed_;
	double width_{0.4};
	double height_{1.0};
	std::string id_;
	std::shared_ptr<Weapon> weapon_;
	std::vector<std::function<void(Position2D)>> player_position_subscribers_;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_PLAYER_H
