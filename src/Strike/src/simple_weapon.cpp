#include "Strike/simple_weapon.h"
#include "ShootingManager/shooting_manager.h"

namespace wolfenstein {

void SimpleWeapon::SetCrosshairRay(Ray ray) {
	crosshair_ray = ray;
}

std::pair<double, double> SimpleWeapon::GetAttackDamage() const {
	return attack_damage;
}

double SimpleWeapon::GetAttackRange() const {
	return attack_range;
}

double SimpleWeapon::GetAttackSpeed() const {
	return attack_speed;
}

double SimpleWeapon::GetAttackRate() const {
	return attack_rate;
}

Ray SimpleWeapon::GetCrosshair() const {
	return crosshair_ray;
}

std::string SimpleWeapon::GetWeaponName() const {
	return weapon_name;
}

Melee::Melee() {
	attack_damage = std::make_pair(17, 8);
	attack_range = 2.0;
	attack_speed = 0.5;
	attack_rate = 1.0;
	weapon_name = "melee";
}

void Melee::Attack() {
	ShootingManager::GetInstance().EnemyShoot(shared_from_this());
}

Rifle::Rifle() {
	attack_damage = std::make_pair(15, 5);
	attack_range = 5.0;
	attack_speed = 0.7;
	attack_rate = 1.0;
	weapon_name = "rifle";
}

void Rifle::Attack() {
	ShootingManager::GetInstance().EnemyShoot(shared_from_this());
}

LaserGun::LaserGun() {
	attack_damage = std::make_pair(19, 10);
	attack_range = 7.0;
	attack_speed = 1.0;
	attack_rate = 1.0;
	weapon_name = "laser gun";
}

void LaserGun::Attack() {
	ShootingManager::GetInstance().EnemyShoot(shared_from_this());
}

}  // namespace wolfenstein
