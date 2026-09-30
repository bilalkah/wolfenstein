#include "Characters/player_figure.h"
#include "Characters/player.h"
#include <algorithm>
#include <cmath>

namespace karakale {

namespace {

// A stride of the walk, all its frames, as fast as a player runs
constexpr double kWalkCycleSeconds = 0.45;
// A shot, aimed and fired
constexpr double kFireSeconds = 0.25;
// Moving less than this in a tick is standing still
constexpr double kStill = 1e-4;

}  // namespace

void PlayerFigure::SetLook(const TextureManager& textures,
						   std::string_view clips, double width,
						   double height) {
	idle_ = LoopedAnimation(textures, clips, "idle", kWalkCycleSeconds);
	walk_ = LoopedAnimation(textures, clips, "walk", kWalkCycleSeconds);
	attack_ = LoopedAnimation(textures, clips, "attack", kFireSeconds);
	death_ = LoopedAnimation::Clip(textures, clips, "death");
	width_ = width;
	height_ = height;
	has_look_ = true;
}

void PlayerFigure::Show(const Player* player, bool viewer) {
	if (player != player_ && player != nullptr) {
		last_pose_ = player->GetPose();
		walking_ = false;
	}
	player_ = player;
	viewer_ = viewer;
}

void PlayerFigure::Fire() {
	firing_ = kFireSeconds;
	attack_.Reset();
}

void PlayerFigure::Update(double delta_time) {
	if (player_ == nullptr || !has_look_) {
		return;	 // nothing to show, or nothing to show it with
	}
	if (firing_ > 0.0) {
		firing_ = std::max(firing_ - delta_time, 0.0);
		attack_.Update(delta_time);
	}
	const vector2d pose = player_->GetPose();
	walking_ = player_->IsAlive() && pose.Distance(last_pose_) > kStill;
	last_pose_ = pose;
	if (walking_) {
		walk_.Update(delta_time);
	}
	else {
		walk_.Reset();
	}
}

ObjectType PlayerFigure::GetObjectType() const {
	return ObjectType::CHARACTER_PLAYER;
}

vector2d PlayerFigure::GetPose() const {
	return player_ != nullptr ? player_->GetPose() : vector2d{};
}

vector2d PlayerFigure::GetRenderPose(double alpha) const {
	return player_ != nullptr ? player_->GetRenderPosition(alpha).pose
							  : vector2d{};
}

bool PlayerFigure::IsVisible() const {
	return has_look_ && player_ != nullptr && !viewer_;
}

int PlayerFigure::GetTextureId() const {
	return idle_.GetCurrentFrame();
}

IGameObject::Appearance PlayerFigure::SeenFrom(const vector2d& viewer) const {
	const Position2D& at = player_->GetPosition();
	if (player_->IsAlive()) {
		const LoopedAnimation& shown = firing_ > 0.0 ? attack_
									   : walking_	 ? walk_
													 : idle_;
		// Shielded (just back in), it shows paler: shots do it no harm
		const auto pale = [](std::uint8_t c) {
			return static_cast<std::uint8_t>((c + 255) / 2);
		};
		const Tint tint = player_->IsProtected() ? Tint{.r = pale(tint_.r),
														.g = pale(tint_.g),
														.b = pale(tint_.b)}
												 : tint_;
		return {
			.texture_id = shown.GetFrame(SideSeen(at.pose, at.theta, viewer)),
			.width = width_,
			.mirrored = false,
			.tint = tint};
	}
	// Falling, a frame of the fall each share of it; seen end on, a body
	// lying down is this much of its length, as an enemy's is
	const auto last = static_cast<double>(death_.size() - 1);
	const auto frame = static_cast<std::size_t>(
		std::min(std::floor(player_->GetDeathFall() * last + 0.5), last));
	constexpr double kEndOn = 0.4;
	const double across = std::cos(TurnedFrom(at.pose, at.theta, viewer));
	return {.texture_id = death_[frame],
			.width = width_ * (kEndOn + (1.0 - kEndOn) * std::abs(across)),
			.mirrored = across < 0.0,
			.tint = tint_};
}

}  // namespace karakale
