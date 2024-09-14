/**
 * @file state.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-09-05
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STATE_INCLUDE_STATE_STATE_H_
#define STATE_INCLUDE_STATE_STATE_H_

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace wolfenstein {

template <typename T>
class State
{
  public:
	virtual ~State() = default;
	void SetContext(std::shared_ptr<T> context) { context_ = context; }
	virtual void Update(const double& delta_time) = 0;
	virtual void Reset() = 0;
	virtual int GetCurrentFrame() const = 0;

  protected:
	std::shared_ptr<T> context_;
	std::unordered_map<std::string,
					   std::unordered_map<std::string, std::string>>
		enemy_data_{{"soldier",
					 {{"idle", "soldier_idle"},
					  {"walk", "soldier_walk"},
					  {"attack", "soldier_attack"},
					  {"death", "soldier_death"},
					  {"pain", "soldier_pain"}}},
					{"caco_demon",
					 {{"idle", "caco_demon_idle"},
					  {"walk", "caco_demon_walk"},
					  {"attack", "caco_demon_attack"},
					  {"death", "caco_demon_death"},
					  {"pain", "caco_demon_pain"}}},
					{"cyber_demon",
					 {{"idle", "cyber_demon_idle"},
					  {"walk", "cyber_demon_walk"},
					  {"attack", "cyber_demon_attack"},
					  {"death", "cyber_demon_death"},
					  {"pain", "cyber_demon_pain"}}}};
	std::unordered_map<std::string,
					   std::unordered_map<std::string, std::string>>
		weapon_data_{{"mp5",
					  {{"loaded", "mp5_loaded"},
					   {"outofammo", "mp5_outofammo"},
					   {"reload", "mp5_reload"}}}};
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_STATE_H_
