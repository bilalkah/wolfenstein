#include "Core/scene.h"
#include "CollisionManager/collision_manager.h"
#include "NavigationManager/navigation_manager.h"

namespace wolfenstein {

void Scene::AddObject(std::shared_ptr<IGameObject> object) {
	objects.push_back(object);
	if (object->GetObjectType() == ObjectType::CHARACTER_ENEMY) {
		auto enemy = std::dynamic_pointer_cast<Enemy>(object);
		enemies.push_back(enemy);
	}
}

void Scene::SetMap(std::shared_ptr<Map> map) {
	this->map = map;
}

void Scene::SetPlayer(std::shared_ptr<Player> player) {
	this->player = player;
}

void Scene::Update(double delta_time) {
	player->Update(delta_time);
	// for (auto& enemy : enemies) {
	// 	enemy->SetNextPose(NavigationManager::GetInstance().FindPath(
	// 		enemy->GetPosition(), player->GetPosition(), enemy->GetId()));
	// }
	for (auto& object : objects) {
		object->Update(delta_time);
	}
}

std::vector<std::shared_ptr<IGameObject>> Scene::GetObjects() const {
	return objects;
}

std::vector<std::shared_ptr<Enemy>> Scene::GetEnemies() const {
	return enemies;
}

std::shared_ptr<Map> Scene::GetMap() const {
	return map;
}

std::shared_ptr<Player> Scene::GetPlayer() const {
	return player;
}

}  // namespace wolfenstein
