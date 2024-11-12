#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../rendering/Text.h"
#include "../util/SceneBuilder.h"
#include "../util/Timer.h"
#include "Camera.h"
#include "PhysicsWorld.h"

class Scene {
public:
	Scene();
	virtual ~Scene();
	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&) = delete;
	Scene& operator=(Scene&&) = delete;

	virtual void Update(TimeDuration dt) = 0;

	void SetCamera(const std::shared_ptr<Camera>& camera) { m_camera = camera; }
	const std::shared_ptr<Camera>& GetCamera() const { return m_camera; }

	const PhysicsWorld& GetPhysicsWorld() const { return m_physicsWorld; }

	void AddText(const std::weak_ptr<DrawableText>& text) {
		m_drawableTexts.push_back(text);
	}
	std::vector<std::weak_ptr<DrawableText>>& GetTexts() {
		return m_drawableTexts;
	}

	entt::registry registry;
	glm::vec3 sunlightDir{glm::normalize(glm::vec3{1, 1, 1})};

protected:
	std::shared_ptr<Camera> m_camera;
	PhysicsWorld m_physicsWorld;
	SceneBuilder m_sceneBuilder;

private:
	std::vector<std::weak_ptr<DrawableText>> m_drawableTexts;

private:
	static void OnDestroyRigidBody(entt::registry& reg, entt::entity entity);
	static void OnConstructRigidBody(entt::registry& reg, entt::entity entity);
};
