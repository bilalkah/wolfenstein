#include "Server/match_rules.h"
#include <algorithm>
#include <limits>

namespace wolfenstein {

MatchRules::MatchRules(World& world, const MatchSettings& settings)
	: world_(world), settings_(settings) {
	Restart();
}

void MatchRules::Restart() {
	phase_ = net::MatchPhase::Playing;
	elapsed_ = 0.0;
	winner_.reset();
	comeback_ = {};
	// Every pickup back where it lay; a gun race leaves the weapons and
	// their rounds out, each player carrying only its step's
	const bool race = settings_.mode == net::MatchMode::GunRace;
	for (Pickup* pickup : world_.CurrentLevel().GetPickups()) {
		const PickupEffect& effect = pickup->GetEffect();
		if (race && (effect.weapons != 0 || effect.ammo_boxes > 0)) {
			pickup->Take();
		}
		else if (!pickup->IsDrop()) {
			pickup->Restore();
		}
	}
	for (std::size_t slot = 0; slot < standings_.size(); ++slot) {
		if (standings_[slot].present) {
			standings_[slot] = {.present = true};
			Respawn(slot);
		}
	}
}

void MatchRules::Join(std::size_t slot) {
	standings_[slot] = {.present = true};
	Respawn(slot);
}

void MatchRules::Leave(std::size_t slot) {
	standings_[slot] = {};
	// No one left: the next to come starts a match afresh
	if (std::ranges::none_of(standings_, &Standing::present)) {
		Restart();
	}
}

bool MatchRules::Tick(double seconds, std::span<const MatchEvent> events,
					  std::span<const bool> wants_back) {
	bool changed = false;
	const auto pickups = world_.CurrentLevel().GetPickups();
	for (const MatchEvent& event : events) {
		if (event.type == MatchEvent::Type::Kill &&
			phase_ == net::MatchPhase::Playing) {
			CountKill(event);
			changed = true;
		}
		else if (event.type == MatchEvent::Type::Pickup &&
				 event.other < std::min(pickups.size(), kPickups)) {
			comeback_[event.other] = ComebackSeconds(*pickups[event.other]);
		}
	}
	for (std::size_t i = 0; i < std::min(pickups.size(), kPickups); ++i) {
		if (comeback_[i] > 0.0) {
			comeback_[i] -= seconds;
			if (comeback_[i] <= 0.0) {
				comeback_[i] = 0.0;
				pickups[i]->Restore();
			}
		}
	}
	for (std::size_t slot = 0; slot < standings_.size(); ++slot) {
		Standing& standing = standings_[slot];
		Player* player = world_.FindPlayer(slot);
		if (!standing.present || player == nullptr) {
			continue;
		}
		if (player->IsAlive()) {
			// A gun race's weapon never runs dry
			if (settings_.mode == net::MatchMode::GunRace) {
				Weapon& weapon = player->GetWeapon(player->HeldWeapon());
				const WeaponConfig& config = weapon.GetConfig();
				if (weapon.GetReserve() < config.ammo_capacity) {
					weapon.SetRounds(weapon.GetAmmo(), config.reserve_max);
				}
			}
			continue;
		}
		standing.down += seconds;
		const bool asking = slot < wants_back.size() && wants_back[slot];
		if (standing.down >= settings_.respawn_seconds ||
			(standing.down >= settings_.respawn_early && asking)) {
			Respawn(slot);
		}
	}
	if (std::ranges::any_of(standings_, &Standing::present)) {
		elapsed_ += seconds;
	}
	if (phase_ == net::MatchPhase::Playing &&
		elapsed_ >= settings_.time_limit) {
		End(Leader());
		changed = true;
	}
	else if (phase_ == net::MatchPhase::Intermission &&
			 elapsed_ >= settings_.intermission_seconds) {
		Restart();
		changed = true;
	}
	return changed;
}

double MatchRules::SecondsLeft() const {
	const double length = phase_ == net::MatchPhase::Playing
							  ? settings_.time_limit
							  : settings_.intermission_seconds;
	return std::max(length - elapsed_, 0.0);
}

Position2D MatchRules::FarthestSpawn(std::size_t slot) const {
	const auto spawns = world_.Spawns();
	if (spawns.empty()) {
		return world_.SpawnFor(slot);
	}
	double best = -1.0;
	std::size_t chosen = 0;
	for (std::size_t k = 0; k < spawns.size(); ++k) {
		const std::size_t i = (k + spawn_turn_) % spawns.size();
		double nearest = std::numeric_limits<double>::max();
		for (std::size_t other = 0; other < standings_.size(); ++other) {
			const Player* player = world_.FindPlayer(other);
			if (other != slot && player != nullptr && player->IsAlive()) {
				nearest = std::min(nearest,
								   player->GetPose().Distance(spawns[i].pose));
			}
		}
		if (nearest > best) {
			best = nearest;
			chosen = i;
		}
	}
	return spawns[chosen];
}

void MatchRules::CountKill(const MatchEvent& kill) {
	Standing& fallen = standings_[kill.other];
	++fallen.deaths;
	fallen.down = 0.0;
	Standing& killer = standings_[kill.slot];
	if (!killer.present) {
		return;	 // gone while its rocket flew
	}
	if (kill.slot == kill.other) {
		--killer.frags;
		return;
	}
	++killer.frags;
	if (settings_.mode == net::MatchMode::GunRace) {
		++killer.step;
		if (killer.step >= world_.Config().gun_race.size()) {
			End(kill.slot);
			return;
		}
		ArmForStep(kill.slot);
	}
	else if (killer.frags >= settings_.frag_limit) {
		End(kill.slot);
	}
}

void MatchRules::Respawn(std::size_t slot) {
	Player* player = world_.FindPlayer(slot);
	if (player == nullptr) {
		return;
	}
	player->Revive(FarthestSpawn(slot));
	++spawn_turn_;
	player->Protect(phase_ == net::MatchPhase::Intermission
						? SecondsLeft() + 1.0
						: settings_.protection_seconds);
	standings_[slot].down = 0.0;
	if (settings_.mode == net::MatchMode::GunRace) {
		ArmForStep(slot);
	}
}

void MatchRules::ArmForStep(std::size_t slot) {
	const auto& ladder = world_.Config().gun_race;
	Player* player = world_.FindPlayer(slot);
	if (ladder.empty() || player == nullptr) {
		return;
	}
	player->Arm(ladder[std::min(standings_[slot].step, ladder.size() - 1)]);
}

void MatchRules::End(std::optional<std::size_t> winner) {
	phase_ = net::MatchPhase::Intermission;
	elapsed_ = 0.0;
	winner_ = winner;
	// No one is hurt while the result shows
	for (std::size_t slot = 0; slot < standings_.size(); ++slot) {
		if (Player* player = world_.FindPlayer(slot)) {
			player->Protect(settings_.intermission_seconds + 1.0);
		}
	}
}

std::optional<std::size_t> MatchRules::Leader() const {
	const bool race = settings_.mode == net::MatchMode::GunRace;
	const auto ahead = [race](const Standing& a, const Standing& b) {
		if (race && a.step != b.step) {
			return a.step > b.step;
		}
		return a.frags > b.frags;
	};
	std::optional<std::size_t> leader;
	bool tied = false;
	for (std::size_t slot = 0; slot < standings_.size(); ++slot) {
		const Standing& standing = standings_[slot];
		if (!standing.present) {
			continue;
		}
		if (!leader || ahead(standing, standings_[*leader])) {
			leader = slot;
			tied = false;
		}
		else if (!ahead(standings_[*leader], standing)) {
			tied = true;
		}
	}
	return tied ? std::nullopt : leader;
}

double MatchRules::ComebackSeconds(const Pickup& pickup) const {
	constexpr double kWeapon = 20.0;
	constexpr double kLargeHealth = 30.0;
	constexpr double kOther = 15.0;
	constexpr double kLarge = 50.0;
	const PickupEffect& effect = pickup.GetEffect();
	if (effect.weapons != 0) {
		return kWeapon;
	}
	return effect.health >= kLarge ? kLargeHealth : kOther;
}

}  // namespace wolfenstein
