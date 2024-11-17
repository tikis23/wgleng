#include "FrustumCulling.h"

FrustumCulling::FrustumCulling(const glm::mat4& projxview) {
	const glm::mat4 mat = glm::transpose(projxview);
	m_planes[0] = mat[3] + mat[0]; // left
	m_planes[1] = mat[3] - mat[0]; // right
	m_planes[2] = mat[3] + mat[1]; // bottom
	m_planes[3] = mat[3] - mat[1]; // top
	m_planes[4] = mat[3] + mat[2]; // near
	m_planes[5] = mat[3] - mat[2]; // far
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
