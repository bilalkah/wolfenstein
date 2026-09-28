#include "State/weapon_state.h"
#include "SoundManager/sound_manager.h"
#include "State/state.h"
#include "Strike/weapon.h"
#include "TextureManager/texture_manager.h"
#include <algorithm>
#include <cstddef>
#include <memory>

namespace wolfenstein {

void WeaponState::Reset() {
	animation_.Reset();
}

int WeaponState::GetCurrentFrame() const {
	return animation_.GetCurrentFrame();
}

// ########################################### LoadedState ###########################################

void LoadedState::Update(const double& delta_time) {
	if (trigger_pulled_) {
		animation_.Update(delta_time);
		trigger_pull_time_ += delta_time;
		if (trigger_pull_time_ >= fire_rate_) {
			trigger_pulled_ = false;
			animation_.Reset();
			if (context_->GetAmmo() == 0 && !context_->IsMelee()) {
				context_->TransitionTo(WeaponStateType::OutOfAmmo);
				return;
			}
		}
	}
}

void LoadedState::OnEnter() {
	WeaponState::OnEnter();
	trigger_pulled_ = false;
	trigger_pull_time_ = 0.0;
}

WeaponStateType LoadedState::GetType() const {
	return WeaponStateType::Loaded;
}

void LoadedState::OnContextSet() {
	fire_rate_ = context_->GetAttackSpeed();
	animation_ =
		LoopedAnimation(context_->GetTextures(), context_->GetWeaponName(),
						"loaded", fire_rate_);
}

bool LoadedState::PullTrigger() {
	if (trigger_pulled_) {
		return false;
	}
	context_->PlaySound(context_->GetShotSound());
	trigger_pulled_ = true;
	trigger_pull_time_ = 0;
	context_->DecreaseAmmo();
	return true;
}

// ########################################### OutOfAmmoState ###########################################

void OutOfAmmoState::Update(const double& delta_time) {
	if (trigger_pulled_) {
		animation_.Update(delta_time);
		trigger_pull_time_ += delta_time;
		if (trigger_pull_time_ >= fire_rate_) {
			trigger_pulled_ = false;
			animation_.Reset();
		}
	}
}

void OutOfAmmoState::OnEnter() {
	WeaponState::OnEnter();
	trigger_pulled_ = false;
	trigger_pull_time_ = 0.0;
}

WeaponStateType OutOfAmmoState::GetType() const {
	return WeaponStateType::OutOfAmmo;
}

void OutOfAmmoState::OnContextSet() {
	fire_rate_ = context_->GetAttackSpeed();
	animation_ =
		LoopedAnimation(context_->GetTextures(), context_->GetWeaponName(),
						"outofammo", fire_rate_);
}

bool OutOfAmmoState::PullTrigger() {
	if (!trigger_pulled_) {
		trigger_pulled_ = true;
		trigger_pull_time_ = 0;
		context_->PlaySound(SoundEffect::DryFire);
	}
	return false;
}

// ########################################### ReloadingState ###########################################

void ReloadingState::Update(const double& delta_time) {
	animation_.Update(delta_time);
	reload_time_ += delta_time;
	if (reload_time_ >= reload_speed_) {
		animation_.Reset();
		context_->FinishReload();
		context_->TransitionTo(WeaponStateType::Loaded);
		return;
	}
}

void ReloadingState::OnEnter() {
	WeaponState::OnEnter();
	reload_time_ = 0.0;
}

WeaponStateType ReloadingState::GetType() const {
	return WeaponStateType::Reloading;
}

void ReloadingState::OnContextSet() {
	reload_speed_ = context_->GetReloadSpeed();
	animation_ =
		LoopedAnimation(context_->GetTextures(), context_->GetWeaponName(),
						"reload", reload_speed_);
}

// ########################################### RaisingState ###########################################

void RaisingState::Update(const double& delta_time) {
	animation_.Update(delta_time);
	time_ += delta_time;
	if (time_ >= seconds_) {
		context_->TransitionTo(context_->GetAmmo() == 0 && !context_->IsMelee()
								   ? WeaponStateType::OutOfAmmo
								   : WeaponStateType::Loaded);
	}
}

void RaisingState::OnEnter() {
	WeaponState::OnEnter();
	time_ = 0.0;
}

WeaponStateType RaisingState::GetType() const {
	return WeaponStateType::Raising;
}

void RaisingState::OnContextSet() {
	// Its own clip, if its art has one ("<weapon>_raise"), else the end of
	// its reload
	auto frames = LoopedAnimation::FindClip(context_->GetTextures(),
											context_->GetWeaponName(), "raise");
	if (frames.empty()) {
		const auto reload = LoopedAnimation::Clip(
			context_->GetTextures(), context_->GetWeaponName(), "reload");
		frames = reload.last(
			std::max<std::size_t>(reload.size() / kShareOfReload, 1));
	}
	seconds_ = context_->GetRaiseSeconds();
	animation_ =
		LoopedAnimation(frames, seconds_ / static_cast<double>(frames.size()));
}

// ########################################### LoweringState ###########################################

void LoweringState::Update(const double& delta_time) {
	// To its last frame, and there it stays
	if (animated_ && !IsDown()) {
		animation_.Update(delta_time);
	}
	time_ += delta_time;
}

void LoweringState::OnEnter() {
	WeaponState::OnEnter();
	time_ = 0.0;
}

WeaponStateType LoweringState::GetType() const {
	return WeaponStateType::Lowering;
}

void LoweringState::OnContextSet() {
	seconds_ = context_->GetLowerSeconds();
	const auto frames = LoopedAnimation::FindClip(
		context_->GetTextures(), context_->GetWeaponName(), "lower");
	animated_ = !frames.empty();
	// Without a clip of its own it just goes, on the frame it had
	animation_ =
		animated_ ? LoopedAnimation(
						frames, seconds_ / static_cast<double>(frames.size()))
				  : LoopedAnimation(LoopedAnimation::Clip(
										context_->GetTextures(),
										context_->GetWeaponName(), "loaded"),
									seconds_);
}

}  // namespace wolfenstein
