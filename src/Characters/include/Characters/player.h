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

#include <Characters/character.h>
#include <functional>

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

	void SetCameraPositionUpdator(std::function<void(Position2D)> updator);

  private:
	void Move(double delta_time);
	void Rotate(double delta_time);

	Position2D position_;
	double rotation_speed_;
	double translation_speed_;
	std::string id_;
	std::function<void(Position2D)> camera_position_updator_;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_PLAYER_H
