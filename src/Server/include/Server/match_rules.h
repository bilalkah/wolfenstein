/**
 * @file match_rules.h
 * @brief How a multiplayer match is won: frags, coming back, the clock
 */

#ifndef SERVER_INCLUDE_SERVER_MATCH_RULES_H_
#define SERVER_INCLUDE_SERVER_MATCH_RULES_H_

#include "Core/scene.h"
#include "Core/world.h"
#include "Net/protocol.h"
#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace wolfenstein {

// How a match is played
struct MatchSettings
{
	net::MatchMode mode = net::MatchMode::Deathmatch;
	// Deathmatch: the first to this many frags wins
	int frag_limit = 20;
	// The match ends this long after it began: the most frags win (in a
	// gun race, the furthest up the ladder)
	double time_limit = 600.0;
	// Down this long, a player comes back; down this long at least, it may
	// come back sooner by firing
	double respawn_seconds = 3.0;
	double respawn_early = 1.0;
	// Back in, it cannot be hurt this long, or until it fires
	double protection_seconds = 1.5;
	// The result shows this long before the next match
	double intermission_seconds = 10.0;
};

// A player's standing in the match
struct Standing
{
	bool present = false;
	int frags = 0;
	int deaths = 0;
	std::size_t step = 0;  // a gun race's: how far up the ladder
	double down = 0.0;	   // seconds since it fell
};

// The rules of a free-for-all match, applied to the world after each tick.
// Every kill counts a frag for the killer and a death for the fallen (a
// player killing itself loses a frag). A player down comes back after a
// while, whole, carrying what a game starts with, at the spawn point
// furthest from the others, shielded for a moment. Pickups come back a
// while after they are taken. In a deathmatch the first to the frag limit
// wins; in a gun race each kill takes the killer up a ladder of weapons,
// every other weapon and its rounds left out of the level, and a kill with
// the last wins. Past the time limit the leader wins. The result shows a
// while, everyone shielded, then the next match begins.
class MatchRules
{
  public:
	// Pickups whose comeback is counted, at most (a snapshot tells of as
	// many)
	static constexpr std::size_t kPickups = net::kMaxPickups;

	// Borrows the world, which outlives it, its match started
	MatchRules(World& world, const MatchSettings& settings);

	// A player came in (the world has it): no score yet, placed where no
	// one is near, shielded
	void Join(std::size_t slot);
	void Leave(std::size_t slot);
	// After a tick `seconds` long in which `events` happened: kills are
	// counted, pickups taken go, players down long enough (or asking,
	// `wants_back`, by slot) come back, pickups come back and the clock
	// runs. True if a standing or the phase changed.
	bool Tick(double seconds, std::span<const MatchEvent> events,
			  std::span<const bool> wants_back);
	// Everyone's score back to none, everyone whole, the pickups back: the
	// next match
	void Restart();

	const MatchSettings& Settings() const { return settings_; }
	net::MatchPhase Phase() const { return phase_; }
	// Of the match, or of the intermission
	double SecondsLeft() const;
	std::optional<std::size_t> Winner() const { return winner_; }
	const Standing& StandingOf(std::size_t slot) const {
		return standings_[slot];
	}
	// Where a player comes back: the spawn point furthest from the living
	// others, by the one nearest it
	Position2D FarthestSpawn(std::size_t slot) const;

  private:
	void CountKill(const MatchEvent& kill);
	// The player comes back in: whole, where no one is near, shielded
	void Respawn(std::size_t slot);
	// A gun race's player carries its step's weapon
	void ArmForStep(std::size_t slot);
	void End(std::optional<std::size_t> winner);
	// The leader: the most frags (a gun race: furthest up, then frags)
	std::optional<std::size_t> Leader() const;
	// How long pickup `pickup` is gone once taken
	double ComebackSeconds(const Pickup& pickup) const;

	World& world_;
	MatchSettings settings_;
	std::array<Standing, Scene::kMaxPlayers> standings_{};
	// Seconds until each pickup comes back; 0 lying there, or gone for
	// good
	std::array<double, kPickups> comeback_{};
	net::MatchPhase phase_ = net::MatchPhase::Playing;
	double elapsed_ = 0.0;	// of the match, or of the intermission
	std::optional<std::size_t> winner_;
	// Turns through the spawn points when many are as far from everyone
	std::size_t spawn_turn_ = 0;
};

}  // namespace wolfenstein

#endif	// SERVER_INCLUDE_SERVER_MATCH_RULES_H_
