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
#include <memory>
#include <string>
namespace wolfenstein {

class TextureManager;

struct WeaponConfig
{
	WeaponConfig(std::string weapon_name, size_t ammo_capacity,
				 double attack_damage_max, double attack_damage_min,
				 double attack_range, double attack_speed, double reload_speed)
		: weapon_name(weapon_name),
		  ammo_capacity(ammo_capacity),
		  attack_damage(attack_damage_max, attack_damage_min),
		  attack_range(attack_range),
		  attack_speed(attack_speed),
		  reload_speed(reload_speed) {}
	std::string weapon_name;
	size_t ammo_capacity{};
	std::pair<double, double> attack_damage;
	double attack_range{};
	double attack_speed{};
	double reload_speed{};
};

// Pinned (not copyable or movable): its states point back to it
class Weapon
{
  public:
	// Borrows the textures its animations play from and the sound it plays;
	// both outlive it
	Weapon(std::string weapon_name, const TextureManager& textures,
		   SoundManager& sound);
	const TextureManager& GetTextures() const { return textures_; }

	// Pulls the trigger; true if a shot was fired (the caller resolves it)
	bool Attack();
	void Update(double delta_time);
	void Charge();
	void Reload();
	void TransitionTo(WeaponStateType type);

	void SetAmmo(size_t ammo);
	void IncreaseAmmo();
	void IncreaseAmmo(size_t amount);
	void DecreaseAmmo();
	void DecreaseAmmo(size_t amount);

	size_t GetAmmo() const;
	size_t GetAmmoCapacity() const;
	std::pair<double, double> GetAttackDamage() const;
	double GetAttackRange() const;
	double GetAttackSpeed() const;
	double GetReloadSpeed() const;
	const std::string& GetWeaponName() const;
	// Plays on the weapon's own channel
	void PlaySound(SoundEffect effect) {
		sound_.PlayEffect(sound_channel_, effect);
	}
	int GetTextureId() const;

  private:
	const TextureManager& textures_;
	SoundManager& sound_;
	WeaponConfig weapon_properties_;
	SoundChannel sound_channel_;
	size_t ammo_{};
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
