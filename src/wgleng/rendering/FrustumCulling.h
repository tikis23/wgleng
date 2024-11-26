#pragma once

#include <vector>
#include <glm/glm.hpp>

class FrustumCulling {
public:
	enum class Plane {
		Left   = 1 << 0,
		Right  = 1 << 1,
		Bottom = 1 << 2,
		Top    = 1 << 3,
		Near   = 1 << 4,
		Far    = 1 << 5,
		None   = 0,
	};
	FrustumCulling(const glm::mat4& projxview, Plane ignoredPlanes = Plane::None);

	bool IsAabbVisible(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const;
	bool IsAabbVisible(const glm::vec3& aabbMin, const glm::vec3& aabbMax, const glm::mat4& model) const;

private:
	std::vector<glm::vec4> m_planes;
};
