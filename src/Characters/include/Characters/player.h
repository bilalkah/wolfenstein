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
#include "GameMap/map.h"
#include "GameObjects/game_object.h"
#include "GameObjects/pickup.h"
#include "SoundManager/sound_manager.h"
#include "Strike/weapon.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>

namespace wolfenstein {

class Scene;
struct Ray;
// Player.h
class Player : public ICharacter, public IGameObject
{
  public:
	// The most weapons a player can carry: one bit each in a set
	static constexpr std::size_t kMaxWeapons = 8;

	// Carries a weapon for each of `arsenal` (the configuration's, in slot
	// order; at most kMaxWeapons), built in place: holds those marked to
	// start with and `first`, which is in hand. Borrows the arsenal, the
	// textures and the sound, which outlive it.
	Player(CharacterConfig& config, std::span<const WeaponConfig> arsenal,
		   std::size_t first, const TextureManager& textures,
		   SoundManager& sound);
	// Carrying the one weapon
	Player(CharacterConfig& config, const WeaponConfig& weapon,
		   const TextureManager& textures, SoundManager& sound);
	// Pinned: its weapons' states point back to the weapons inside it
	Player(const Player&) = delete;
	Player& operator=(const Player&) = delete;
	Player(Player&&) = delete;
	Player& operator=(Player&&) = delete;
	~Player() override = default;

	void Update(double delta_time) override;

	// The player outlives levels; each level's scene hands itself over here,
	// with the player's slot in it, and stays valid until the next one does
	void EnterScene(Scene& scene, std::size_t slot = 0) {
		scene_ = &scene;
		slot_ = slot;
	}
	// Its slot in the level: 0 alone, the server's in a match
	std::size_t Slot() const { return slot_; }
	// What to do from the next update on; the player reads no input device
	void SetCommand(const PlayerCommand& command);
	// Driven from outside (another player in a networked game, placed where
	// the server's snapshots put it): an update only keeps where it was, to
	// draw it moving smoothly, and plays out a fall; Follow moves it
	void SetPuppet(bool puppet) { puppet_ = puppet; }
	bool IsPuppet() const { return puppet_; }
	// A puppet goes where the server has it: there, looking so, this
	// healthy, alive or not (falling as it dies, whole again as it comes
	// back)
	void Follow(const Position2D& position, double pitch, double health,
				bool alive);
	// The local player's move under `command` again, and nothing else: no
	// shot, no sound (a networked game putting it where the server says it
	// was, then replaying the commands the server has not seen yet)
	void Replay(const PlayerCommand& command, double delta_time);
	// Puts it where the server has it, without drawing it there as a jump
	// from where it was: as far as the next update, it is drawn moving
	void Correct(const Position2D& position);
	// Health and life as the server has them, for the local player of a
	// match: a drop hurts as a hit does (the flash, a cry), and down, it
	// falls; back up, it is whole where Revive (or the caller) puts it
	void TakeVitals(double health, bool alive);
	// Back in the game after falling (a match's respawn): at `position`,
	// whole, carrying only what a game starts with, rounds full
	void Revive(const Position2D& position);
	// Carries only the weapon `index`, coming up into hand, its magazine
	// full and its reserve at its most (a gun race's next weapon)
	void Arm(std::size_t index);
	// Shots and blasts leave it unhurt for `seconds` (a match's spawn
	// protection), or until it fires
	void Protect(double seconds) { protection_ = seconds; }
	bool IsProtected() const { return protection_ > 0.0; }
	// Whether the player is trying to open what is in front of them
	bool IsUsing() const { return command_.use; }
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
	// Takes what a pickup gives, scaled by `supplies` (the difficulty's);
	// false, leaving it lying, if the player has no use for it (full health,
	// a full reserve)
	bool TryPickUp(const PickupEffect& effect, double supplies = 1.0);
	// Shows and sounds taking what a pickup gives, as TryPickUp does (a
	// match's server says it was taken)
	void ShowPickup(const PickupEffect& effect);
	// Health and the weapon in hand's rounds as a saved game left them,
	// within their limits
	void Restore(double health, std::size_t ammo, std::size_t reserve);
	// The keys held, as KeyBit()s; each level's keys open its own doors, so
	// a level starts with none
	std::uint8_t GetKeys() const { return keys_; }
	bool HasKey(KeyColour key) const { return (keys_ & KeyBit(key)) != 0; }
	void SetKeys(std::uint8_t keys) { keys_ = keys; }
	// Seconds since the player was last hurt (or since it was made)
	double SecondsSinceHurt() const { return since_hurt_; }
	bool IsDamaged() const;
	bool IsAlive() const;
	// The weapon in hand
	const Weapon& GetWeapon() const;
	// The weapons: every one of the arsenal, held or not
	std::size_t WeaponCount() const { return weapon_count_; }
	const Weapon& GetWeapon(std::size_t index) const;
	Weapon& GetWeapon(std::size_t index);
	std::size_t HeldWeapon() const { return held_; }
	// The weapons carried, a bit per index
	std::uint8_t GetOwnedWeapons() const { return owned_; }
	bool Owns(std::size_t index) const {
		return index < weapon_count_ && (owned_ >> index & 1U) != 0;
	}
	void SetOwnedWeapons(std::uint8_t owned);
	// Takes the weapon `index` in hand, if carried: the one in hand goes
	// down first, then it comes up
	void SelectWeapon(std::size_t index);
	// Has the weapon `index` in hand at once, ready (a saved game's)
	void TakeInHand(std::size_t index);
	// The weapon coming into hand once the one in hand is down, if any
	std::optional<std::size_t> ComingWeapon() const { return coming_; }
	// Where to draw the view `alpha` of the way from the previous tick
	Position2D GetRenderPosition(double alpha) const;
	// How far the last shot's kick still jolts the view, a share of the
	// screen's height easing back to 0; and as drawn `alpha` of the way
	// from the previous tick
	double GetKick() const { return kick_; }
	double GetRenderKick(double alpha) const {
		return previous_kick_ + (kick_ - previous_kick_) * alpha;
	}
	// How far up (+) or down the player looks: the view slides by this share
	// of the screen's height, and a shot `d` away flies at 0.5 + pitch * d
	// (a wall is 1 high, the eye half way up it)
	double GetPitch() const { return pitch_; }
	// The furthest the player looks up or down
	static constexpr double kMaxPitch = 0.4;
	// A footstep every stride walked, one foot then the other
	static constexpr double kStride = 0.9;
	// How fast the turning keys turn the player, radians a second
	static constexpr double kKeyboardTurnSpeed = 2.5;
	// The hit marker round the crosshair after a shot hit: how strongly it
	// shows (1 as it hits, fading to 0), and whether it was a headshot
	double GetHitMarker() const;
	bool IsHeadshotMarker() const { return headshot_; }
	// A shot of theirs hit (a projectile, bursting later than it was fired)
	void NoteHit(bool head = false) {
		since_hit_ = 0.0;
		headshot_ = head;
	}
	// How far through its fall a dead player is: 0 as it dies, 1 on the
	// floor (after kFallSeconds, sooner at first, as things fall)
	double GetDeathFall() const;
	static constexpr double kFallSeconds = 0.9;
	// Where the eye is, in walls above the floor: half way up, and lower as
	// a dead player falls
	double GetEyeHeight() const;
	// Opacity of the damage overlay, fading out after a hit
	std::uint8_t GetDamageAlpha() const { return damage_animation_.GetAlpha(); }
	// Opacity of the flash after taking a pickup, 0 when none is showing
	std::uint8_t GetPickupAlpha() const {
		return picked_up_ ? pickup_animation_.GetAlpha() : 0;
	}

  private:
	void Move(double delta_time);
	void Rotate(double delta_time);
	void SwitchWeapons();
	void ShootOrReload();

	Scene* scene_ = nullptr;
	std::size_t slot_ = 0;
	PlayerCommand command_;
	bool puppet_ = false;
	bool replaying_ = false;  // quiet: no footsteps
	bool is_alive_{true};
	bool damaged_{false};
	double translation_speed_{};
	double width_{};
	double height_{};
	double health_{};
	double since_hurt_{};
	double kick_{};
	double previous_kick_{};
	double pitch_{};
	double since_death_{};
	double protection_{};	 // seconds of spawn protection left
	double since_hit_{1e9};	 // since a shot of theirs last hit
	bool headshot_ = false;
	std::uint8_t keys_{};
	SoundManager& sound_;
	SoundChannel sound_channel_;
	SoundChannel step_channel_;	 // footsteps, apart from the rest
	double walked_{};			 // since the last footstep
	bool left_foot_ = false;
	Position2D position_;
	Position2D previous_position_;
	std::array<std::optional<Weapon>, kMaxWeapons> weapons_;
	std::size_t weapon_count_ = 0;
	std::uint8_t owned_ = 0;
	std::size_t held_ = 0;
	std::size_t first_ = 0;	 // in hand as a game starts
	std::optional<std::size_t> coming_;
	TriggeredSingleAnimation damage_animation_;
	bool picked_up_{false};
	TriggeredSingleAnimation pickup_animation_;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_PLAYER_H
