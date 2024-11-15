#pragma once

#include <deque>
#include <string>

#include "../core/Scene.h"
#include "CSMBuffer.h"
#include "GBuffer.h"
#include "Mesh.h"
#include "ShaderProgram.h"
#include "UniformBuffer.h"

class Renderer {
public:
	Renderer();
	~Renderer();
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer&&) = delete;

	void SetViewportSize(int32_t width, int32_t height);

	void Render(bool isHidden, const std::shared_ptr<Scene>& scene);

	enum class ShaderType : uint64_t {
		ALL      = ~0U,
		DEBUG    = 1U << 0,
		MESH     = 1U << 1,
		LIGHTING = 1U << 2,
		CSM      = 1U << 3,
		TEXT     = 1U << 4
	};

	void ReloadShaders(ShaderType shaders = ShaderType::ALL);
	void ShowWireframe(bool show) { m_showWireframe = show; }
	bool IsWireframeShown() const { return m_showWireframe; }

private:
	// options
	bool m_showWireframe = false;
	int32_t m_viewportWidth, m_viewportHeight;
	int32_t m_csmWidth, m_csmHeight;

	constexpr static inline uint32_t m_matricesPerUniformBuffer = 256;
	constexpr static inline uint32_t m_maxCSMFrustums = 4;
	constexpr static inline uint32_t m_materialsPerUniformBuffer = 1024;

	// buffers
	GBuffer m_gbuffer;
	CSMBuffer m_csmbuffer;

	// shaders
	void LoadShaderFromFile(const std::string& file);
	void SetupUniforms();
	bool m_shadersLoading = false;
	std::deque<std::string> m_shaderLoadingQueue;
	std::deque<std::string> m_shaderLoadingOutput;
	std::deque<std::unique_ptr<ShaderProgram>*> m_shaderLoadingPrograms;

	std::unique_ptr<ShaderProgram> m_debugProgram;
	std::unique_ptr<ShaderProgram> m_meshProgram;
	std::unique_ptr<ShaderProgram> m_lightingProgram;
	std::unique_ptr<ShaderProgram> m_textProgram;
	std::vector<std::unique_ptr<ShaderProgram>> m_csmPrograms;

	// uniforms
	uint32_t m_nextUniformBindingIndex = 0;
	uint32_t GetNextUniformBindingIndex() { return m_nextUniformBindingIndex++; }

	struct MeshBatch {
		Mesh mesh;
		uint32_t instanceOffset;
		uint32_t instanceCount;
	};
	struct CameraUniform {
		glm::mat4 projxview;
		glm::vec2 nearFarPlane;
		float p1, p2;
	};
	struct CSMUniform {
		glm::mat4 lightSpaceMatrices[m_maxCSMFrustums];
	};
	struct LightingInfoUniform {
		glm::vec3 sunlightDir;
		float p1;
		glm::vec4 sunlightColor; // rgb-color, a-intensity
		glm::vec3 cameraPos;
		float p2;
		glm::vec4 viewportSize_nearFarPlane;
		glm::mat4 invProjView;
	};
	struct MaterialUniform {
		Material materials[m_materialsPerUniformBuffer];
	};
	struct TextUniform {
		glm::mat4 projxview;
		glm::mat4 model;
	};

	UniformBuffer<void> m_modelUniform; // uses dynamic amount of glm::mat4's
	UniformBuffer<CameraUniform> m_cameraUniform;
	UniformBuffer<CSMUniform> m_csmUniform;
	UniformBuffer<LightingInfoUniform> m_lightingInfoUniform;
	UniformBuffer<MaterialUniform> m_materialUniform;
	UniformBuffer<TextUniform> m_textUniform;

	// render steps
	std::size_t m_materialCount{0};
	void UpdateUniforms(const std::shared_ptr<Scene>& scene);

	struct RenderableState {
		std::vector<MeshBatch> batches;
	} m_renderableMeshes;
	void UpdateRenderableMeshes(const std::shared_ptr<Scene>& scene);
	void RenderShadowMaps() const;
	void RenderMeshes() const;
	void RenderDebug(const std::shared_ptr<Scene>& scene) const;
	void RenderText(const std::shared_ptr<Scene>& scene);
	void RenderLighting() const;
};
