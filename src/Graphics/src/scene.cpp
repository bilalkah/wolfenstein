#include <Graphics/scene.h>

namespace wolfenstein {

void Scene::AddObject(std::shared_ptr<IGameObject> object) {
	objects.push_back(object);
}

void Scene::SetMap(std::shared_ptr<Map> map) {
	this->map = map;
}

void Scene::Update(double delta_time) {
	for (auto& object : objects) {
		object->Update(delta_time);
	}
}

std::vector<std::shared_ptr<IGameObject>> Scene::GetObjects() const {
	return objects;
}

std::shared_ptr<Map> Scene::GetMap() const {
	return map;
}

}  // namespace wolfenstein
