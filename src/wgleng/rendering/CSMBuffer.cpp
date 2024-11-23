#include "CSMBuffer.h"

#include <array>
#include <glm/ext.hpp>
#include <iostream>

#define DEBUG_FRUSTUMS 0

#if DEBUG_FRUSTUMS
#include "../io/Input.h"
#include "Debug.h"
#endif

CSMBuffer::CSMBuffer(uint32_t width, uint32_t height, const std::vector<float>& cascadeSplits)
	: m_width{width}, m_height{height}, m_cascadeSplits{cascadeSplits} {
	Create();
}
CSMBuffer::~CSMBuffer() {
	Destroy();
}

void CSMBuffer::SetCascades(const std::vector<float>& cascadeSplits) {
	m_cascadeSplits = cascadeSplits;
	Destroy();
	Create();
}

void CSMBuffer::Resize(uint32_t width, uint32_t height) {
	m_width = width;
	m_height = height;
	Destroy();
	Create();
}

std::vector<glm::vec4> GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
	const glm::mat4 invProjView = glm::inverse(proj * view);

	std::vector<glm::vec4> frustumCorners;
	for (int x = 0; x < 2; x++) {
		for (int y = 0; y < 2; y++) {
			for (int z = 0; z < 2; z++) {
				glm::vec3 xyz = glm::vec3{x, y, z} * 2.f - 1.f;
				const glm::vec4 pt = invProjView * glm::vec4(xyz, 1.0f);
				frustumCorners.push_back(pt / pt.w);
			}
		}
	}
	return frustumCorners;
}
std::vector<glm::mat4> CSMBuffer::GetLightSpaceMatrices(const std::shared_ptr<Camera>& camera, const glm::vec3& lightDir) const {
#if DEBUG_FRUSTUMS
	static std::vector<std::pair<glm::mat4, glm::mat4>> testMats;
	if (Input::JustPressed(SDL_SCANCODE_G)) {
		testMats.clear();
		testMats.emplace_back(camera->GetViewMatrix(), camera->GetProjectionMatrix());
	}
	// draw frustums
	for (int i = 0; i < testMats.size(); i++) {
		const auto& [view, proj] = testMats[i];
		glm::vec3 col{
			i * 127 % 256 / 255.0f,
			i * 151 % 256 / 255.0f,
			i * 197 % 256 / 255.0f
		};
		col[i % 3] = 1.f;
		const auto fc = GetFrustumCornersWorldSpace(proj, view);
		DebugDraw::DrawLine(fc[0], fc[1], col);
		DebugDraw::DrawLine(fc[1], fc[3], col);
		DebugDraw::DrawLine(fc[3], fc[2], col);
		DebugDraw::DrawLine(fc[2], fc[0], col);
		DebugDraw::DrawLine(fc[4], fc[5], col);
		DebugDraw::DrawLine(fc[5], fc[7], col);
		DebugDraw::DrawLine(fc[7], fc[6], col);
		DebugDraw::DrawLine(fc[6], fc[4], col);
		DebugDraw::DrawLine(fc[0], fc[4], col);
		DebugDraw::DrawLine(fc[1], fc[5], col);
		DebugDraw::DrawLine(fc[2], fc[6], col);
		DebugDraw::DrawLine(fc[3], fc[7], col);
	}
#endif

	std::vector<glm::mat4> lightSpaceMatrices(GetFrustumCount());
	for (int i = 0; i < GetFrustumCount(); i++) {
		const float nearPlane = i == 0 ? camera->GetNearPlane() : m_cascadeSplits[i - 1];
		const float farPlane = m_cascadeSplits[i];
		glm::mat4 proj = glm::perspective(glm::radians(camera->GetFov()), camera->GetAspectRatio(), nearPlane, farPlane);
		const auto frustumCorners = GetFrustumCornersWorldSpace(proj, camera->GetViewMatrix());

		glm::vec3 center{0};
		for (const auto& v : frustumCorners) center += glm::vec3(v);
		center /= frustumCorners.size();
		glm::mat4 lightView = glm::lookAt(center + lightDir, center, camera->GetUp());

		float maxX = std::numeric_limits<float>::lowest();
		float minX = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();
		float minY = std::numeric_limits<float>::max();
		float maxZ = std::numeric_limits<float>::lowest();
		float minZ = std::numeric_limits<float>::max();
		for (const auto& v : frustumCorners) {
			const auto trf = lightView * v;
			maxX = std::max(maxX, trf.x);
			minX = std::min(minX, trf.x);
			maxY = std::max(maxY, trf.y);
			minY = std::min(minY, trf.y);
			maxZ = std::max(maxZ, trf.z);
			minZ = std::min(minZ, trf.z);
		}

		// adjust to make nicer/worse shadows
		float zMult = 1.f;
		if (minZ < 0) minZ *= zMult;
		else minZ /= zMult;
		zMult = 10.f;
		if (maxZ < 0) maxZ /= zMult;
		else maxZ *= zMult;

		const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, -maxZ, -minZ);
		lightSpaceMatrices[i] = lightProjection * lightView;

#if DEBUG_FRUSTUMS
		if (Input::JustPressed(SDL_SCANCODE_G)) testMats.emplace_back(lightView, lightProjection);
#endif
	}

	return lightSpaceMatrices;
}

void CSMBuffer::Create() {
	if (GetFrustumCount() == 0) {
		printf("Error: CSMBuffer requires at least one cascade.\n");
		return;
	}
	m_fbos.resize(GetFrustumCount());

	glGenTextures(1, &m_texDepth);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_texDepth);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	std::tuple<int, int, int> depthFormats[] = {
		{GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT},
		{GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV},
		{GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT},
		{GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8},
		{GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT},
	};
	bool bufferValid = false;
	for (auto&& [d_internal_format, d_format, d_type] : depthFormats) {
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, d_internal_format, m_width, m_height, GetFrustumCount(), 0, d_format, d_type, 0);

		for (auto& fbo : m_fbos) glDeleteFramebuffers(1, &fbo);
		glGenFramebuffers(GetFrustumCount(), m_fbos.data());
		bool success = true;
		for (int i = 0; i < m_fbos.size(); i++) {
			glBindFramebuffer(GL_FRAMEBUFFER, m_fbos[i]);
			glDrawBuffers(0, nullptr);
			glReadBuffer(GL_NONE);
			glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_texDepth, 0, i);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				success = false;
				break;
			}
		}
		if (success) {
			bufferValid = true;
			break;
		}
	}
	if (!bufferValid) printf("Error: Failed to create valid CSMBuffer.\n");
}
void CSMBuffer::Destroy() {
	glDeleteFramebuffers(m_fbos.size(), m_fbos.data());
	m_fbos.clear();
	glDeleteTextures(1, &m_texDepth);
	m_texDepth = 0;
}
