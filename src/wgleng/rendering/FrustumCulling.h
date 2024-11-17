#pragma once

#include <array>
#include <glm/glm.hpp>

class FrustumCulling {
public:
	FrustumCulling(const glm::mat4& projxview);

	bool IsAabbVisible(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const;
	bool IsAabbVisible(const glm::vec3& aabbMin, const glm::vec3& aabbMax, const glm::mat4& model) const;

private:
	std::array<glm::vec4, 6> m_planes;
};
