#pragma once

#include <glm/glm.hpp>

constexpr inline glm::mat4 computeModelMatrix(
	const glm::vec3& globalPos, const glm::vec3& localPos,
	const glm::vec3& globalRot, const glm::vec3& localRot,
	const glm::vec3& scale) noexcept {
	glm::mat4 model{ 1 };
	model = glm::translate(model, globalPos);
	model = glm::rotate(model, globalRot.z, glm::vec3(0, 0, 1));
	model = glm::rotate(model, globalRot.y, glm::vec3(0, 1, 0));
	model = glm::rotate(model, globalRot.x, glm::vec3(1, 0, 0));
	model = glm::translate(model, localPos);
	model = glm::rotate(model, localRot.x, glm::vec3(1, 0, 0));
	model = glm::rotate(model, localRot.y, glm::vec3(0, 1, 0));
	model = glm::rotate(model, localRot.z, glm::vec3(0, 0, 1));
	model = glm::scale(model, scale);
	return model;
}