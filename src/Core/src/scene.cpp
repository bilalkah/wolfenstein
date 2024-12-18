#include "Core/scene.h"
namespace wolfenstein {

void Scene::AddObject(std::shared_ptr<IGameObject> object) {
	objects.push_back(object);
	if (object->GetObjectType() == ObjectType::CHARACTER_ENEMY) {
		auto enemy = std::dynamic_pointer_cast<Enemy>(object);
		enemies.push_back(enemy);
		number_of_alive_enemies++;
	}
}

void Scene::SetMap(std::shared_ptr<Map> map) {
	this->map = map;
}

void Scene::SetPlayer(std::shared_ptr<Player>& player) {
	this->player = player;
}

void Scene::SetNextScene(const std::string next_scene) {
	next_scene_str = next_scene;
}

void Scene::DecreaseAliveEnemies() {
	number_of_alive_enemies--;
}

void Scene::Update(double delta_time) {
	for (auto& object : objects) {
		object->Update(delta_time);
	}

	player->Update(delta_time);
}

std::vector<std::shared_ptr<IGameObject>>& Scene::GetObjects() {
	return objects;
}

std::vector<std::shared_ptr<Enemy>>& Scene::GetEnemies() {
	return enemies;
}

const Map& Scene::GetMap() const {
	return *map;
}

Map& Scene::GetMap() {
	return *map;
}

const Player& Scene::GetPlayer() const {
	return *player;
}

Player& Scene::GetPlayer() {
	return *player;
}

size_t Scene::GetNumberOfAliveEnemies() const {
	return number_of_alive_enemies;
}

std::string Scene::GetNextScene() const {
	return next_scene_str;
}

}  // namespace wolfenstein
