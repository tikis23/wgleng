#include "PhysicsWorld.h"

PhysicsWorld::PhysicsWorld() {
	// collision configuration contains default setup for memory, collision setup. Advanced users can create their own configuration.
	m_collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();

	// use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
	m_dispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfiguration.get());

	// btDbvtBroadphase is a good general purpose broadphase. You can also try out btAxis3Sweep.
	m_broadphase = std::make_unique<btDbvtBroadphase>();

	// the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
	m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();

	dynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(m_dispatcher.get(), m_broadphase.get(),
		m_solver.get(), m_collisionConfiguration.get());

	dynamicsWorld->setGravity(btVector3(0, -30, 0));
}

PhysicsWorld::~PhysicsWorld() {
	for (int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
		btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getMotionState()) {
			delete body->getMotionState();
		}
		dynamicsWorld->removeCollisionObject(obj);
		const auto ptr = static_cast<std::shared_ptr<btCollisionShape>*>(body->getUserPointer());
		delete ptr;
		delete obj;
	}
}
void PhysicsWorld::BodyEnableGravity(btRigidBody* body) {
	body->setFlags(body->getFlags() & ~btRigidBodyFlags::BT_DISABLE_WORLD_GRAVITY);
	const auto& dynamicsWorld = static_cast<RigidBodyUserData*>(body->getUserPointer())->physicsWorld->dynamicsWorld;
	body->setGravity(dynamicsWorld->getGravity());
}
void PhysicsWorld::BodyDisableGravity(btRigidBody* body) {
	body->setFlags(body->getFlags() | btRigidBodyFlags::BT_DISABLE_WORLD_GRAVITY);
	body->setGravity({ 0, 0, 0 });
}
void PhysicsWorld::BodyEnableCollisions(btRigidBody* body) {
	body->setCollisionFlags(body->getCollisionFlags() & ~btRigidBody::CollisionFlags::CF_NO_CONTACT_RESPONSE);
}
void PhysicsWorld::BodyDisableCollisions(btRigidBody* body) {
	body->setCollisionFlags(body->getCollisionFlags() | btRigidBody::CollisionFlags::CF_NO_CONTACT_RESPONSE);
}

template <typename T>
void clearExpiredObjects(std::unordered_map<T, std::weak_ptr<btCollisionShape>>& map) {
	std::vector<T> objectsToRemove;
	for (auto& it : map) {
		if (it.second.expired()) {
			objectsToRemove.push_back(it.first);
		}
	}
	for (auto& obj : objectsToRemove) map.erase(obj);
}

void PhysicsWorld::Update() {
	clearExpiredObjects(m_boxShapes);
	clearExpiredObjects(m_sphereShapes);
	clearExpiredObjects(m_capsuleShapes);
}

void PhysicsWorld::SetTransform(btTransform& transform, const glm::vec3& position, const glm::vec3& rotation) {
	transform.setOrigin(btVector3(position.x, position.y, position.z));

	const glm::quat rot = glm::quat(glm::radians(rotation));
	transform.setRotation(btQuaternion(rot.x, rot.y, rot.z, rot.w));
}

std::shared_ptr<btCollisionShape> PhysicsWorld::GetBoxCollider(const glm::vec3& halfExtents) {
	const glm::vec3& key = halfExtents;
	// check if exists
	const auto it = m_boxShapes.find(key);
	if (it != m_boxShapes.end()) {
		auto ptr = it->second.lock();
		if (ptr) return ptr;
	}
	// create new
	auto ptr = std::make_shared<btBoxShape>(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));
	m_boxShapes[key] = ptr;
	return ptr;
}
std::shared_ptr<btCollisionShape> PhysicsWorld::GetSphereCollider(float radius) {
	const float key = radius;
	// check if exists
	const auto it = m_sphereShapes.find(key);
	if (it != m_sphereShapes.end()) {
		auto ptr = it->second.lock();
		if (ptr) return ptr;
	}
	// create new
	auto ptr = std::make_shared<btSphereShape>(radius);
	m_sphereShapes[key] = ptr;
	return ptr;
}
std::shared_ptr<btCollisionShape> PhysicsWorld::GetCapsuleCollider(float radius, float height) {
	const glm::vec2 key(radius, height);
	// check if exists
	const auto it = m_capsuleShapes.find(key);
	if (it != m_capsuleShapes.end()) {
		auto ptr = it->second.lock();
		if (ptr) return ptr;
	}
	// create new
	auto ptr = std::make_shared<btCapsuleShape>(radius, height);
	m_capsuleShapes[key] = ptr;
	return ptr;
}

btRigidBody* PhysicsWorld::CreateRigidBody(entt::entity entity, std::shared_ptr<btCollisionShape> colShape, float mass, const glm::vec3& position,
	const glm::vec3& rotation) {
	btTransform transform;
	SetTransform(transform, position, rotation);

	const bool isDynamic = (mass != 0.f);

	btVector3 localInertia(0, 0, 0);
	if (isDynamic) colShape->calculateLocalInertia(mass, localInertia);

	const auto motionState = new btDefaultMotionState(transform);
	const btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, colShape.get(), localInertia);
	const auto body = new btRigidBody(rbInfo);
	dynamicsWorld->addRigidBody(body);

	const auto userData = new RigidBodyUserData();
	userData->entity = entity;
	userData->collisionShape = std::move(colShape);
	userData->physicsWorld = this;
	body->setUserPointer(userData);
	body->setRollingFriction(0.05f); // stops objects rolling by themselves without any forces

	return body;
}
void PhysicsWorld::DestroyRigidBody(btRigidBody* body) const {
	if (!body) return;
	dynamicsWorld->removeRigidBody(body);
	delete body->getMotionState();
	const auto userData = static_cast<RigidBodyUserData*>(body->getUserPointer());
	delete userData;
	delete body;
}
std::vector<PhysicsWorld::RaycastData> PhysicsWorld::RaycastWorld(const glm::vec3& from, const glm::vec3& to, bool sortByDist) const {
	const btVector3 btFrom(from.x, from.y, from.z);
	const btVector3 btTo(to.x, to.y, to.z);
	btCollisionWorld::AllHitsRayResultCallback rayCallback(btFrom, btTo);
	dynamicsWorld->rayTest(btFrom, btTo, rayCallback);

	std::vector<PhysicsWorld::RaycastData> result;
	result.reserve(rayCallback.m_collisionObjects.size());
	for (int i = 0; i < rayCallback.m_collisionObjects.size(); i++) {
		const auto obj = rayCallback.m_collisionObjects[i];
		const auto userData = static_cast<RigidBodyUserData*>(obj->getUserPointer());
		if (!userData) continue;
		const auto& btHitPoint = rayCallback.m_hitPointWorld[i];
		const auto& btHitNormal = rayCallback.m_hitNormalWorld[i];
		const glm::vec3 hitPoint{btHitPoint.getX(), btHitPoint.getY(), btHitPoint.getZ()};
		const glm::vec3 hitNormal{btHitNormal.getX(), btHitNormal.getY(), btHitNormal.getZ()};
		result.emplace_back(userData->entity, hitPoint, hitNormal);
	}
	if (sortByDist) {
		std::sort(result.begin(), result.end(), [&from](const RaycastData& a, const RaycastData& b) {
			return glm::distance2(from, a.hitPoint) < glm::distance2(from, b.hitPoint);
		});
	}
	return result;
}
void PhysicsWorld::CheckObjectsTouchingGround() const {
	// remove old flags
	for (int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
		const btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
		const auto userData = static_cast<RigidBodyUserData*>(obj->getUserPointer());
		if (userData) userData->onGround = false;
	}

	// get new flags
	constexpr float threshold = 0.5f;
	const int numManifolds = dynamicsWorld->getDispatcher()->getNumManifolds();
	for (int i = 0; i < numManifolds; i++) {
		const auto contactManifold = dynamicsWorld->getDispatcher()->getManifoldByIndexInternal(i);
		const auto objA = contactManifold->getBody0();
		const auto objB = contactManifold->getBody1();

		const int numContacts = contactManifold->getNumContacts();
		for (int j = 0; j < numContacts; j++) {
			btManifoldPoint& point = contactManifold->getContactPoint(j);
			if (point.getDistance() < 0.05f) {
				if (point.m_normalWorldOnB.y() > threshold) {
					const auto userData = static_cast<RigidBodyUserData*>(objA->getUserPointer());
					if (userData) userData->onGround = true;
					break;
				}
				if (point.m_normalWorldOnB.y() < -threshold) {
					const auto userData = static_cast<RigidBodyUserData*>(objB->getUserPointer());
					if (userData) userData->onGround = true;
					break;
				}
			}
		}
	}
}
