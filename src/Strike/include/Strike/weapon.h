/**
 * @file weapon.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-08-29
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STRIKE_INCLUDE_STRIKE_WEAPON_H
#define STRIKE_INCLUDE_STRIKE_WEAPON_H

#include "SoundManager/sound_manager.h"
#include "State/weapon_state.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
namespace wolfenstein {

class TextureManager;

// How a weapon's damage falls from point blank to its range
enum class DamageFalloff : std::uint8_t { Linear, Exponential };

// A player weapon as config.json describes it
struct WeaponConfig
{
	std::string weapon_name;	  // names its animation clips: "<name>_reload"
	std::string label;			  // shown in the menu
	std::string description;	  // shown in the menu
	std::size_t ammo_capacity{};  // rounds in a magazine
	// Rounds carried besides the magazine: at the start of a game, at most,
	// and in one ammo box picked up
	std::size_t reserve_start{};
	std::size_t reserve_max{};
	std::size_t box_rounds{};
	std::pair<double, double> attack_damage;  // at point blank, at range
	double attack_range{};
	double attack_speed{};
	double reload_speed{};
	DamageFalloff falloff = DamageFalloff::Linear;
};

// Pinned (not copyable or movable): its states point back to it
class Weapon
{
  public:
	// Borrows its configuration (the World's), the textures its animations
	// play from and the sound it plays: all outlive it
	Weapon(const WeaponConfig& config, const TextureManager& textures,
		   SoundManager& sound);
	const TextureManager& GetTextures() const { return textures_; }

	// Pulls the trigger; true if a shot was fired (the caller resolves it)
	bool Attack();
	void Update(double delta_time);
	// Starts reloading if the magazine has room and rounds are in reserve
	void Reload();
	// Moves rounds from the reserve into the magazine, as many as fit
	void FinishReload();
	void TransitionTo(WeaponStateType type);

	void DecreaseAmmo();
	// The magazine and reserve as given, within the weapon's limits
	void SetRounds(size_t ammo, size_t reserve);
	// Adds the rounds of `boxes` ammo boxes, times `scale`, to the reserve,
	// up to its most; false (nothing taken) if the reserve is already full
	bool AddAmmoBoxes(size_t boxes, double scale = 1.0);

	size_t GetAmmo() const;
	size_t GetAmmoCapacity() const;
	size_t GetReserve() const { return reserve_; }
	std::pair<double, double> GetAttackDamage() const;
	double GetAttackRange() const;
	double GetAttackSpeed() const;
	double GetReloadSpeed() const;
	const std::string& GetWeaponName() const;
	DamageFalloff GetFalloff() const { return config_.falloff; }
	// Plays on the weapon's own channel
	void PlaySound(SoundEffect effect) {
		sound_.PlayEffect(sound_channel_, effect);
	}
	int GetTextureId() const;

  private:
	const TextureManager& textures_;
	SoundManager& sound_;
	// Borrowed from the game config, which outlives every weapon
	const WeaponConfig& config_;
	SoundChannel sound_channel_;
	size_t ammo_{};
	size_t reserve_{};
	WeaponState& StateFor(WeaponStateType type);

	// Every state the weapon can be in, set up once: transitions allocate
	// nothing
	LoadedState loaded_state_;
	OutOfAmmoState out_of_ammo_state_;
	ReloadingState reloading_state_;
	StateMachine<WeaponState> state_machine_;
};

}  // namespace wolfenstein

#endif	// STRIKE_INCLUDE_STRIKE_WEAPON_H
