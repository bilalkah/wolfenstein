/**
 * @file player_figure.h
 * @brief Another player, as the others see them
 */

#ifndef CHARACTERS_INCLUDE_CHARACTERS_PLAYER_FIGURE_H_
#define CHARACTERS_INCLUDE_CHARACTERS_PLAYER_FIGURE_H_

#include "Animation/looped_animation.h"
#include "GameObjects/game_object.h"
#include <cstdint>
#include <span>
#include <string_view>

namespace wolfenstein {

class Player;
class TextureManager;

// Another player as the rest see them: a character that turns (a
// soldier's pictures), standing, walking or falling as the player does,
// tinted its slot's colour. The scene keeps one for each player slot among
// its objects from when the level loads, so a player joining takes a place
// already there, and the camera draws them like any other object. The
// player the level is seen from is not drawn: the eye is inside it.
class PlayerFigure : public IGameObject
{
  public:
	// Shows no one until it has a look and a player
	PlayerFigure() = default;

	// Its pictures: the clips "<clips>_idle", "<clips>_walk", "<clips>_attack"
	// (seen from 8 sides) and "<clips>_death", drawn `width` across and
	// `height` tall (a wall is 1)
	void SetLook(const TextureManager& textures, std::string_view clips,
				 double width, double height);
	// The player it shows (nullptr: none), and whether that player is the
	// one the level is seen from, and so not drawn
	void Show(const Player* player, bool viewer);
	void SetTint(Tint tint) { tint_ = tint; }
	const Player* Shown() const { return player_; }
	bool IsWalking() const { return walking_; }
	// Its player fired: it shows shooting for a moment
	void Fire();
	bool IsFiring() const { return firing_ > 0.0; }

	// Walks while its player moves, from one tick to the next
	void Update(double delta_time) override;
	void SetPose(const vector2d& /*pose*/) override {}	// its player's
	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;
	vector2d GetRenderPose(double alpha) const override;
	bool IsVisible() const override;
	int GetTextureId() const override;
	Appearance SeenFrom(const vector2d& viewer) const override;
	double GetWidth() const override { return width_; }
	double GetHeight() const override { return height_; }

  private:
	const Player* player_ = nullptr;
	bool viewer_ = false;
	bool has_look_ = false;
	Tint tint_{};
	double width_ = 0.0;
	double height_ = 0.0;
	LoopedAnimation idle_;
	LoopedAnimation walk_;
	LoopedAnimation attack_;
	double firing_ = 0.0;  // seconds of shooting left to show
	std::span<const std::uint16_t> death_;
	// Where its player stood at the last update: moving since, it walks
	vector2d last_pose_{};
	bool walking_ = false;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_CHARACTERS_PLAYER_FIGURE_H_
