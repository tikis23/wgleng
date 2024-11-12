#pragma once

#include <btBulletDynamicsCommon.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <string>

#include "../rendering/Mesh.h"
#include "EntityFlags.h"

// may not be present
struct MeshComponent {
	Mesh mesh;
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale{1};
	uint8_t highlightId{0}; // resets every frame
	bool hidden = false;    // resets every frame
	bool hiddenPersistent = false; // needs to be reset manually
};

// may not be present
struct TransformComponent {
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale{1};
};

// may not be present
struct RigidBodyComponent {
	btRigidBody* body;
};

// may not be present
struct TagComponent {
	std::string tag;
};

// guaranteed to be present
struct FlagComponent {
	entityFlagType flags;
};

// may not be present
struct PlayerComponent {};
