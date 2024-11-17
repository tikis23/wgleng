#include "Scene.h"

#include "Components.h"

Scene::Scene()
    : m_sceneBuilder(registry, m_physicsWorld) {
	registry.on_construct<entt::entity>().connect<OnConstructEntity>();
    registry.on_destroy<RigidBodyComponent>().connect<OnDestroyRigidBody>();
	registry.on_construct<RigidBodyComponent>().connect<OnConstructRigidBody>();
	registry.on_update<RigidBodyComponent>().connect<OnConstructRigidBody>();
}
Scene::~Scene() {
    registry.clear();
}

void Scene::OnConstructEntity(entt::registry& reg, entt::entity entity) {
	reg.emplace<FlagComponent>(entity, FlagComponent{ EntityFlags::NONE });
}
void Scene::OnDestroyRigidBody(entt::registry& reg, entt::entity entity) {
	const auto body = reg.get<RigidBodyComponent>(entity).body;
	const auto physicsWorld = static_cast<RigidBodyUserData*>(body->getUserPointer())->physicsWorld;
    physicsWorld->DestroyRigidBody(body);
}
void Scene::OnConstructRigidBody(entt::registry& reg, entt::entity entity) {
	const auto& rigidBody = reg.get<RigidBodyComponent>(entity);
	const auto& flagComp = reg.get<FlagComponent>(entity);
	if (flagComp.flags & EntityFlags::DISABLE_COLLISIONS) PhysicsWorld::BodyDisableCollisions(rigidBody.body);
	if (flagComp.flags & EntityFlags::DISABLE_GRAVITY) PhysicsWorld::BodyDisableGravity(rigidBody.body);
}