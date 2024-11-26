#include "FrustumCulling.h"

FrustumCulling::FrustumCulling(const glm::mat4& projxview, Plane ignoredPlanes) {
	const glm::mat4 mat = glm::transpose(projxview);
	m_planes.reserve(6);
	const auto ip = static_cast<std::underlying_type_t<Plane>>(ignoredPlanes);
	if (!(ip & static_cast<std::underlying_type_t<Plane>>(Plane::Left))) m_planes.push_back(mat[3] + mat[0]);
	if (!(ip & static_cast<std::underlying_type_t<Plane>>(Plane::Right))) m_planes.push_back(mat[3] - mat[0]);
	if (!(ip & static_cast<std::underlying_type_t<Plane>>(Plane::Bottom))) m_planes.push_back(mat[3] + mat[1]);
	if (!(ip & static_cast<std::underlying_type_t<Plane>>(Plane::Top))) m_planes.push_back(mat[3] - mat[1]);
	if (!(ip & static_cast<std::underlying_type_t<Plane>>(Plane::Near))) m_planes.push_back(mat[3] + mat[2]);
	if (!(ip & static_cast<std::underlying_type_t<Plane>>(Plane::Far))) m_planes.push_back(mat[3] - mat[2]);
}

bool FrustumCulling::IsAabbVisible(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const {
	for (const auto& g : m_planes) {
		if (
			(glm::dot(g, glm::vec4(aabbMin.x, aabbMin.y, aabbMin.z, 1.0f)) < 0.0) &&
			(glm::dot(g, glm::vec4(aabbMax.x, aabbMin.y, aabbMin.z, 1.0f)) < 0.0) &&
			(glm::dot(g, glm::vec4(aabbMin.x, aabbMax.y, aabbMin.z, 1.0f)) < 0.0) &&
			(glm::dot(g, glm::vec4(aabbMax.x, aabbMax.y, aabbMin.z, 1.0f)) < 0.0) &&
			(glm::dot(g, glm::vec4(aabbMin.x, aabbMin.y, aabbMax.z, 1.0f)) < 0.0) &&
			(glm::dot(g, glm::vec4(aabbMax.x, aabbMin.y, aabbMax.z, 1.0f)) < 0.0) &&
			(glm::dot(g, glm::vec4(aabbMin.x, aabbMax.y, aabbMax.z, 1.0f)) < 0.0) &&
			(glm::dot(g, glm::vec4(aabbMax.x, aabbMax.y, aabbMax.z, 1.0f)) < 0.0)
		) {
			return false;
		}
	}
	return true;
}

bool FrustumCulling::IsAabbVisible(const glm::vec3& aabbMin, const glm::vec3& aabbMax, const glm::mat4& model) const {
	// update min/max with matrix
	const glm::vec3 corners[8] = {
		glm::vec3(model * glm::vec4(aabbMin.x, aabbMin.y, aabbMin.z, 1.0f)),
		glm::vec3(model * glm::vec4(aabbMin.x, aabbMax.y, aabbMin.z, 1.0f)),
		glm::vec3(model * glm::vec4(aabbMin.x, aabbMin.y, aabbMax.z, 1.0f)),
		glm::vec3(model * glm::vec4(aabbMin.x, aabbMax.y, aabbMax.z, 1.0f)),
		glm::vec3(model * glm::vec4(aabbMax.x, aabbMin.y, aabbMin.z, 1.0f)),
		glm::vec3(model * glm::vec4(aabbMax.x, aabbMax.y, aabbMin.z, 1.0f)),
		glm::vec3(model * glm::vec4(aabbMax.x, aabbMin.y, aabbMax.z, 1.0f)),
		glm::vec3(model * glm::vec4(aabbMax.x, aabbMax.y, aabbMax.z, 1.0f))
	};

	glm::vec3 newMin = corners[0];
	glm::vec3 newMax = corners[0];
	for (int i = 1; i < 8; i++) {
		newMin = glm::min(newMin, corners[i]);
		newMax = glm::max(newMax, corners[i]);
	}

	return IsAabbVisible(newMin, newMax);
}
