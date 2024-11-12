#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <set>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

#include "../core/EntityFlags.h"
#include "../core/PhysicsWorld.h"
#include "Timer.h"

class SceneBuilder {
public:
	SceneBuilder(entt::registry& registry, PhysicsWorld& physicsWorld);

	void AddModel(std::string_view name);
	void Update();
	void Play();
	bool IsPlaying() const { return m_playing; }

	struct State {
		glm::vec3 position{0, 0, 0};
		glm::vec3 rotation{0, 0, 0};
		glm::vec3 scale{1, 1, 1};

		int selectedModel = -1;
		glm::vec3 modelOffset{0, 0, 0};
		glm::vec3 modelRotation{0, 0, 0};
		glm::vec3 modelScale{1, 1, 1};

		std::string tag;
		entityFlagType flags = EntityFlags::NONE;

		int selectedCollider = -1;
		float mass = 0;
		float friction = 0.5f;
		glm::vec3 boxColliderSize{1, 1, 1};
		float sphereColliderRadius = 1;
		float capsuleColliderRadius = 1;
		float capsuleColliderHeight = 1;
	};
	void Load(uint32_t stateCount, const State* states, bool saveable = false);

private:
	void Blink(int32_t entityId, bool clearOthers = false);
	void Load();
	void Save();
	void Reset();
	void RecreateEntity(int32_t entityId, bool useMass = false, bool invisible = false);
	void FlagSelector();
	bool m_playing = false;
	entt::registry& m_registry;
	PhysicsWorld& m_physicsWorld;
	std::set<int32_t> m_selectedEntities;
	std::string m_saveName;
	uint32_t m_stateVersion = 4;
	std::vector<std::string> m_models;
	std::vector<std::string> m_colliders;
	float m_dragSpeed = 0.01f;

	std::vector<std::pair<entt::entity, State>> m_savedStates;
	std::vector<std::pair<int32_t, TimePoint>> m_blinks;
};
