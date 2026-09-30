#include "State/enemy_state.h"
#include "Characters/enemy.h"
#include "Core/scene.h"
#include "NavigationManager/navigation_manager.h"
#include "Profiler/profiler.h"
#include "SoundManager/sound_manager.h"
#include "TextureManager/texture_manager.h"

namespace karakale {

void EnemyState::Reset() {
	animation_.Reset();
}

int EnemyState::GetCurrentFrame() const {
	return animation_.GetCurrentFrame();
}

// ########################################### IdleState ###########################################

void IdleState::Update(const double& delta_time) {
	animation_.Update(delta_time);

	// It sees the player near, or heard them
	if (context_->NoticesPlayer()) {
		context_->PlaySound(context_->GetSounds().alert);
		context_->TransitionTo(EnemyStateType::Walk);
		return;
	}
	if (context_->TakeHit()) {
		context_->TransitionTo(EnemyStateType::Pain);
		return;
	}
	// One that patrols walks on (again, after a hunt it gave up)
	if (context_->Patrols()) {
		context_->TransitionTo(EnemyStateType::Patrol);
		return;
	}
	// A guard stands its ground, looking this way and that now and then
	looking_for_ += delta_time;
	if (looking_for_ >= kLookSeconds) {
		looking_for_ = 0.0;
		context_->LookAround();
	}
}

void IdleState::OnEnter() {
	EnemyState::OnEnter();
	looking_for_ = 0.0;
	// Standing where it stopped
	context_->SetNextPose(context_->GetPosition().pose);
}

void IdleState::OnContextSet() {
	const auto& config = context_->GetStateConfig();
	animation_speed_ = config.idle_frame_seconds;
	// Idle holds each frame for animation_speed_, where other clips spread
	// their duration over all frames
	animation_ =
		LoopedAnimation(LoopedAnimation::Clip(context_->GetScene().Textures(),
											  context_->GetBotName(), "idle"),
						animation_speed_);
}

EnemyStateType IdleState::GetType() const {
	return EnemyStateType::Idle;
}

// ########################################### PatrolState ###########################################

void PatrolState::Update(const double& delta_time) {
	if (context_->NoticesPlayer()) {
		context_->PlaySound(context_->GetSounds().alert);
		context_->TransitionTo(EnemyStateType::Walk);
		return;
	}
	if (context_->TakeHit()) {
		context_->GetScene().GetNavigation().ResetPath(context_->GetId());
		context_->SetNextPose(context_->GetPosition().pose);
		context_->TransitionTo(EnemyStateType::Pain);
		return;
	}
	// At its spot it picks the next and walks on; so too if it finds no
	// way to the spot. Within a pathfinding cell the planner already holds
	// it still, so that is near enough.
	const auto& position = context_->GetPosition();
	auto& navigation = context_->GetScene().GetNavigation();
	const auto way_on = [&] {
		if (position.pose.Distance(context_->Waypoint()) <
			NavigationManager::kCellSize) {
			return position.pose;
		}
		ScopedTimer timer(ProfileSection::Pathfinding);
		return navigation.FindPath(
			position, Position2D(context_->Waypoint(), 0.0), context_->GetId());
	};
	// A spot it finds no way to, it passes over for another at once: it
	// never stops between one and the next
	constexpr int kTries = 4;
	vector2d next = way_on();
	for (int tries = 0; next == position.pose && tries < kTries; ++tries) {
		context_->PickWaypoint();
		next = way_on();
	}
	context_->SetNextPose(next);
	// Its legs move as it does
	if (context_->IsMoving()) {
		animation_.Update(delta_time);
	}
}

void PatrolState::OnContextSet() {
	animation_ =
		LoopedAnimation(context_->GetScene().Textures(), context_->GetBotName(),
						"walk", animation_speed_);
}

void PatrolState::OnEnter() {
	EnemyState::OnEnter();
	context_->SetPace(context_->GetStateConfig().patrol_pace);
	context_->PickWaypoint();
}

EnemyStateType PatrolState::GetType() const {
	return EnemyStateType::Patrol;
}

// ########################################### WalkState ###########################################

void WalkState::Update(const double& delta_time) {
	const auto& bot_position = context_->GetPosition();
	const auto distance =
		context_->GetScene().GetNavigation().EuclideanDistanceToPlayer(
			bot_position);
	if (context_->TakeHit()) {
		context_->GetScene().GetNavigation().ResetPath(context_->GetId());
		context_->SetNextPose(bot_position.pose);
		context_->TransitionTo(EnemyStateType::Pain);
		return;
	}
	// Badly hurt, it breaks off for cover, if it finds any
	if (context_->WantsToRetreat() && context_->FindCover()) {
		context_->EndSidestep();
		context_->TransitionTo(EnemyStateType::Retreat);
		return;
	}
	// Out of sight and far, it gives up, unless it is still hunting what
	// it heard: back to walking about, straight on, or to standing guard
	if (!context_->IsPlayerInShootingRange() && !context_->IsAlerted()) {
		if (distance > context_->SightRange()) {
			context_->GetScene().GetNavigation().ResetPath(context_->GetId());
			context_->TransitionTo(context_->Patrols() ? EnemyStateType::Patrol
													   : EnemyStateType::Idle);
			return;
		}
	}
	// Ready, it shoots when its turn comes: with too many at it already it
	// holds its fire, and moves on meanwhile
	if (distance <= attack_range_ && context_->IsPlayerInShootingRange()) {
		attack_counter_ += delta_time;
		if (attack_counter_ > attack_rate_ &&
			context_->GetScene().MayAttack(*context_)) {
			context_->SetNextPose(bot_position.pose);
			context_->TransitionTo(EnemyStateType::Attack);
			return;
		}
	}
	const auto& tactics = context_->GetStateConfig();
	const bool seen = context_->IsPlayerInShootingRange();
	auto& navigation = context_->GetScene().GetNavigation();
	// Out of sight, or further off than it likes to fight, or in a doorway
	// where it would block the others: it closes in, facing the way it goes.
	// On a player it sees it comes round to its own side of them.
	if (!seen || distance > tactics.far_range ||
		(context_->InDoorway() && distance >= tactics.near_range)) {
		context_->EndSidestep();
		context_->SetFacePlayer(false);
		context_->SetPace(1.0);
		ScopedTimer timer(ProfileSection::Pathfinding);
		auto next_position = bot_position.pose;
		if (seen) {
			next_position = navigation.FindPath(
				bot_position, Position2D(context_->ApproachSpot(), 0.0),
				context_->GetId());
		}
		// Straight for the player: out of sight, or with no way round to
		// its side of them
		if (next_position == bot_position.pose) {
			next_position =
				navigation.FindPathToPlayer(bot_position, context_->GetId());
		}
		// No way to the player, and no sight of them (behind a locked door):
		// it has lost the trail, and goes back to its round at once rather
		// than stand there hunting until what it heard is forgotten. Its
		// last step carries it on meanwhile: it does not stop in between.
		if (next_position == bot_position.pose &&
			!context_->IsPlayerInShootingRange()) {
			context_->LoseTrail();
			context_->GetScene().GetNavigation().ResetPath(context_->GetId());
			context_->TransitionTo(context_->Patrols() ? EnemyStateType::Patrol
													   : EnemyStateType::Idle);
			return;
		}
		context_->SetNextPose(next_position);
	}
	// Nearer than it likes: it backs away, its gun still on the player
	else if (distance < tactics.near_range) {
		constexpr double kBackingPace = 0.75;
		context_->EndSidestep();
		context_->GetScene().GetNavigation().ResetPath(context_->GetId());
		context_->SetFacePlayer(true);
		context_->SetPace(kBackingPace);
		context_->SetNextPose(context_->BackOffSpot());
	}
	// At its range: between shots it steps aside, the player in its
	// sights; bunched up with another, it moves round to a side of its
	// own; else it stands, and turns as the player moves
	else {
		constexpr double kArrived = 0.05;
		vector2d next = bot_position.pose;
		if (context_->IsSidestepping() &&
			bot_position.pose.Distance(context_->SidestepSpot()) > kArrived) {
			navigation.ResetPath(context_->GetId());
			next = context_->SidestepSpot();
		}
		else {
			context_->EndSidestep();
			if (context_->IsBunched()) {
				const vector2d spot = context_->ApproachSpot();
				if (!(spot == context_->GetScene().GetPlayer().GetPose())) {
					ScopedTimer timer(ProfileSection::Pathfinding);
					next = navigation.FindPath(
						bot_position, Position2D(spot, 0.0), context_->GetId());
				}
			}
		}
		if (next == bot_position.pose) {
			navigation.ResetPath(context_->GetId());
			context_->SetNextPose(bot_position.pose);
			context_->FacePlayer();
		}
		else {
			context_->SetFacePlayer(true);
			context_->SetPace(1.0);
			context_->SetNextPose(next);
		}
	}

	// Its legs move as it does: standing to shoot, it does not step
	if (context_->IsMoving()) {
		animation_.Update(delta_time);
	}
}
void WalkState::OnContextSet() {
	attack_rate_ = context_->GetWeapon().GetAttackRate();
	attack_range_ = context_->GetWeapon().GetAttackRange();
	animation_ =
		LoopedAnimation(context_->GetScene().Textures(), context_->GetBotName(),
						"walk", animation_speed_);
}

void WalkState::OnEnter() {
	EnemyState::OnEnter();
	context_->SetPace(1.0);	 // hunting, at its full speed
	// Out of its pain it shoots back at once; else it waits its rate
	attack_counter_ = context_->TakeRetaliation() ? attack_rate_ : 0.0;
	is_attacked_ = false;
}

EnemyStateType WalkState::GetType() const {
	return EnemyStateType::Walk;
}

// ########################################### AttackState ###########################################

void AttackState::Update(const double& delta_time) {
	animation_.Update(delta_time);
	if (attack_counter_ == 0.0) {
		context_->Shoot();
	}
	if (context_->TakeHit()) {
		context_->TransitionTo(EnemyStateType::Pain);
		return;
	}
	if (attack_counter_ > animation_speed_) {
		// Its shot fired, it steps aside before the next
		context_->PlanSidestep();
		context_->TransitionTo(EnemyStateType::Walk);
		return;
	}
	attack_counter_ += delta_time;
}

void AttackState::OnContextSet() {
	animation_speed_ = context_->GetWeapon().GetAttackSpeed();
	animation_ =
		LoopedAnimation(context_->GetScene().Textures(), context_->GetBotName(),
						"attack", animation_speed_);
}

void AttackState::OnEnter() {
	EnemyState::OnEnter();
	attack_counter_ = 0.0;
	// It turns to the player to shoot
	context_->FacePlayer();
	context_->PlaySound(context_->GetSounds().attack);
}

EnemyStateType AttackState::GetType() const {
	return EnemyStateType::Attack;
}

// ########################################### PainState ###########################################

void PainState::Update(const double& delta_time) {
	animation_.Update(delta_time);
	counter += delta_time;
	if (counter > animation_speed_) {
		if (context_->GetHealth() <= 0) {
			context_->GetScene().GetNavigation().ResetPath(context_->GetId());
			context_->TransitionTo(EnemyStateType::Death);
			return;
		}
		context_->SetAttacked(false);
		context_->TransitionTo(EnemyStateType::Walk);
		return;
	}
}

void PainState::OnContextSet() {
	animation_ =
		LoopedAnimation(context_->GetScene().Textures(), context_->GetBotName(),
						"pain", animation_speed_);
}

void PainState::OnEnter() {
	EnemyState::OnEnter();
	counter = 0.0;
	context_->EndSidestep();
	context_->PlaySound(context_->GetSounds().pain);
}

EnemyStateType PainState::GetType() const {
	return EnemyStateType::Pain;
}

// ########################################### DeathState ###########################################

void DeathState::Update(const double& delta_time) {
	if (animation_.IsAnimationFinishedOnce()) {
		if (context_->IsAlive()) {
			context_->SetDeath();
		}
		return;
	}
	animation_.Update(delta_time);
	counter += delta_time;
}

void DeathState::OnContextSet() {
	animation_ =
		LoopedAnimation(context_->GetScene().Textures(), context_->GetBotName(),
						"death", animation_speed_);
}

void DeathState::OnEnter() {
	EnemyState::OnEnter();
	counter = 0.0;
	// It falls facing the player who killed it, who sees the fall as drawn
	context_->FacePlayer();
	context_->PlaySound(context_->GetSounds().death);
}

EnemyStateType DeathState::GetType() const {
	return EnemyStateType::Death;
}

// ########################################### RetreatState ###########################################

void RetreatState::Update(const double& delta_time) {
	const auto& position = context_->GetPosition();
	auto& navigation = context_->GetScene().GetNavigation();
	if (context_->TakeHit()) {
		navigation.ResetPath(context_->GetId());
		context_->SetNextPose(position.pose);
		context_->TransitionTo(EnemyStateType::Pain);
		return;
	}
	const bool seen = context_->IsPlayerInShootingRange();
	const double distance = navigation.EuclideanDistanceToPlayer(position);
	// It comes out hunting: it knows where the player is, seen or not
	const auto fight = [&] {
		context_->EndRetreat();
		context_->Alert();
		navigation.ResetPath(context_->GetId());
		context_->TransitionTo(EnemyStateType::Walk);
	};
	// In its cover it waits, facing the way the player would come; found
	// there, or rested, it fights
	if (position.pose.Distance(context_->Cover()) <
		NavigationManager::kCellSize) {
		navigation.ResetPath(context_->GetId());
		context_->SetNextPose(position.pose);
		context_->FacePlayer();
		hidden_for_ += delta_time;
		if (seen || hidden_for_ >= kHideSeconds) {
			fight();
		}
		return;
	}
	// On its way, caught close, it turns to fight; with no way on, too
	constexpr double kCornered = 1.5;
	vector2d next = position.pose;
	{
		ScopedTimer timer(ProfileSection::Pathfinding);
		next = navigation.FindPath(position, Position2D(context_->Cover(), 0.0),
								   context_->GetId());
	}
	if ((seen && distance < kCornered) || next == position.pose) {
		fight();
		return;
	}
	context_->SetNextPose(next);
	if (context_->IsMoving()) {
		animation_.Update(delta_time);
	}
}

void RetreatState::OnContextSet() {
	animation_ =
		LoopedAnimation(context_->GetScene().Textures(), context_->GetBotName(),
						"walk", animation_speed_);
}

void RetreatState::OnEnter() {
	EnemyState::OnEnter();
	hidden_for_ = 0.0;
	// Running: quicker than it walks
	constexpr double kRunPace = 1.25;
	context_->SetPace(kRunPace);
}

EnemyStateType RetreatState::GetType() const {
	return EnemyStateType::Retreat;
}

}  // namespace karakale
