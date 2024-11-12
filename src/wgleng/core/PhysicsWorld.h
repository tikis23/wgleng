#pragma once

#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <memory>
#include <unordered_map>

#include "entt/entt.hpp"
#include "glm/gtx/norm.hpp"

class PhysicsWorld;
struct RigidBodyUserData {
	entt::entity entity;
	std::shared_ptr<btCollisionShape> collisionShape;
	PhysicsWorld* physicsWorld;
	bool onGround;
};

class PhysicsWorld {
public:
	PhysicsWorld();
	~PhysicsWorld();

	static void BodyEnableGravity(btRigidBody* body);
	static void BodyDisableGravity(btRigidBody* body);
	static void BodyEnableCollisions(btRigidBody* body);
	static void BodyDisableCollisions(btRigidBody* body);

	std::shared_ptr<btCollisionShape> GetBoxCollider(const glm::vec3& halfExtents);
	std::shared_ptr<btCollisionShape> GetSphereCollider(float radius);
	std::shared_ptr<btCollisionShape> GetCapsuleCollider(float radius, float height);
	btRigidBody* CreateRigidBody(entt::entity entity, std::shared_ptr<btCollisionShape> colShape, float mass, const glm::vec3& position,
		const glm::vec3& rotation);
	void DestroyRigidBody(btRigidBody* body) const;

	struct RaycastData {
		entt::entity entity;
		glm::vec3 hitPoint;
		glm::vec3 hitNormal;
	};
	std::vector<RaycastData> RaycastWorld(const glm::vec3& from, const glm::vec3& to, bool sortByDist) const;
	std::vector<RaycastData> RaycastWorld(const glm::vec3& from, const glm::vec3& to, bool sortByDist, auto&& predicate) const {
		const btVector3 btFrom(from.x, from.y, from.z);
		const btVector3 btTo(to.x, to.y, to.z);
		btCollisionWorld::AllHitsRayResultCallback rayCallback(btFrom, btTo);
		dynamicsWorld->rayTest(btFrom, btTo, rayCallback);

		std::vector<RaycastData> result;
		result.reserve(rayCallback.m_collisionObjects.size());
		for (int i = 0; i < rayCallback.m_collisionObjects.size(); i++) {
			const auto obj = rayCallback.m_collisionObjects[i];
			const auto userData = static_cast<RigidBodyUserData*>(obj->getUserPointer());
			if (!userData) continue;
			const btRigidBody* rigidBody = btRigidBody::upcast(obj);
			const auto& btHitPoint = rayCallback.m_hitPointWorld[i];
			const auto& btHitNormal = rayCallback.m_hitNormalWorld[i];
			const glm::vec3 hitPoint{btHitPoint.getX(), btHitPoint.getY(), btHitPoint.getZ()};
			const glm::vec3 hitNormal{btHitNormal.getX(), btHitNormal.getY(), btHitNormal.getZ()};
			if (!predicate(userData->entity, rigidBody, hitPoint, hitNormal)) continue;
			result.emplace_back(userData->entity, hitPoint, hitNormal);
		}
		if (sortByDist) {
			std::sort(result.begin(), result.end(), [&from](const RaycastData& a, const RaycastData& b) {
				return glm::distance2(from, a.hitPoint) < glm::distance2(from, b.hitPoint);
			});
		}
		return result;
	}
	// clears out any shapes that are no longer being used.
	// does not need to be called every frame. can be called every couple of seconds or so.
	void Update();

	void CheckObjectsTouchingGround() const;

	std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld;

private:
	static void SetTransform(btTransform& transform, const glm::vec3& position, const glm::vec3& rotation);

	std::unique_ptr<btDefaultCollisionConfiguration> m_collisionConfiguration;
	std::unique_ptr<btCollisionDispatcher> m_dispatcher;
	std::unique_ptr<btBroadphaseInterface> m_broadphase;
	std::unique_ptr<btSequentialImpulseConstraintSolver> m_solver;

	std::unordered_map<glm::vec3, std::weak_ptr<btCollisionShape>> m_boxShapes;
	std::unordered_map<float, std::weak_ptr<btCollisionShape>> m_sphereShapes;
	std::unordered_map<glm::vec2, std::weak_ptr<btCollisionShape>> m_capsuleShapes;
};
