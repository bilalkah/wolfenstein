#include "Strike/shotgun.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <iostream>
#include <thread>

namespace wolfenstein {

Shotgun::Shotgun() : attack_speed_(0.7), attack_damage_(10) {
	auto tex_ids =
		TextureManager::GetInstance().GetTextureCollection("shotgun");
	animation_ =
		std::make_shared<TBSAnimation>(tex_ids, attack_speed_ / tex_ids.size());
	cooldown_ = false;
}

int Shotgun::GetTextureId() const {
	return animation_->GetCurrentFrame();
}

void Shotgun::Attack() {
	// If the weapon is not in cooldown, attack and set the cooldown true
	if (!cooldown_) {
		cooldown_ = true;
		last_attack_time_ = TimeManager::GetInstance().GetCurrentTime();

		// Create a new thread to play the attack animation
		// Use attack speed to calculate the time to wait
		std::thread attack_thread(&Shotgun::AttackAnimation, this);
		attack_thread.detach();
	}
}
// BUG: Segmentation fault if game closes while the thread is running
void Shotgun::AttackAnimation() {
	auto attack_time = last_attack_time_;
	auto current_time = TimeManager::GetInstance().GetCurrentTime();
	while (current_time - last_attack_time_ < attack_speed_) {
		auto time_elapsed = current_time - attack_time;
		animation_->Update(time_elapsed);
		attack_time = current_time;
		current_time = TimeManager::GetInstance().GetCurrentTime();
	}
	animation_->Reset();
	cooldown_ = false;
}

}  // namespace wolfenstein
