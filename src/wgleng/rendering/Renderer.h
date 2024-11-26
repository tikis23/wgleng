#pragma once

#include <deque>
#include <string>

#include "../core/Scene.h"
#include "CSMBuffer.h"
#include "FXAABuffer.h"
#include "GBuffer.h"
#include "Mesh.h"
#include "ShaderProgram.h"
#include "UniformBuffer.h"

struct RendererSettings {
	struct ResolutionSettings {
		bool automatic = true;
		int32_t width = 1920;
		int32_t height = 1080;
	} resolution{};
	enum class FXAAPreset {
		OFF, LOW, HIGH
	} fxaa = FXAAPreset::HIGH;
	enum class ShadowPreset {
		OFF, LOW, MEDIUM, HIGH
	} shadows = ShadowPreset::MEDIUM;
	enum class OutlinePreset {
		OFF, ON
	} outlines = OutlinePreset::ON;
}; 

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
		_entt_enum_as_bitmask,
		NONE      = 0,
		ALL      = ~0U,
		DEBUG    = 1U << 0,
		MESH     = 1U << 1,
		LIGHTING = 1U << 2,
		CSM      = 1U << 3,
		TEXT     = 1U << 4,
		FXAA     = 1U << 5,
	};

	const RendererSettings& GetSettings() const { return m_settings; }
	void SetSettings(const RendererSettings& settings, bool force);

	void ReloadShaders(ShaderType shaders = ShaderType::ALL);
	void ShowWireframe(bool show) { m_showWireframe = show; }
	bool IsWireframeShown() const { return m_showWireframe; }

private:
	void CheckExtensionSupport();
	void SetFramebuffer(uint32_t framebuffer);
	uint32_t m_currentFramebuffer = 0;
	void SetRenderSize(uint32_t x, uint32_t y);
	uint32_t m_currentRenderSizeX = 0, m_currentRenderSizeY = 0;
	void SetFaceCullingFront();
	void SetFaceCullingBack();
	bool m_faceCullingFront = false;
private:

	// options
	RendererSettings m_settings{};
	bool m_showWireframe = false;
	int32_t m_viewportWidth, m_viewportHeight;

	constexpr static inline uint32_t m_matricesPerUniformBuffer = 256;
	constexpr static inline uint32_t m_maxCSMFrustums = 4;
	constexpr static inline uint32_t m_materialsPerUniformBuffer = 1024;

	// buffers
	GBuffer m_gbuffer;
	CSMBuffer m_csmbuffer;
	FXAABuffer m_fxaabuffer;

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
	std::unique_ptr<ShaderProgram> m_fxaaProgram;
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
	uint64_t m_totalDrawnTriangleCount{0};
	uint64_t m_totalDrawnEntityCount{0};
	struct RenderableState {
		std::vector<glm::mat4> matricesToUpload;
		std::vector<MeshBatch> worldBatch;
		std::vector<std::vector<MeshBatch>> csmBatches;
	} m_renderableMeshesState;
	void UpdateRenderableMeshes(const std::shared_ptr<Scene>& scene, const std::vector<glm::mat4>& csmMatrices);

	std::size_t m_materialCount{0};
	void UpdateUniforms(const std::shared_ptr<Scene>& scene, const std::vector<glm::mat4>& csmMatrices);

	void RenderShadowMaps();
	void RenderMeshes();
	void RenderDebug(const std::shared_ptr<Scene>& scene) const;
	void RenderText(const std::shared_ptr<Scene>& scene);
	void RenderLighting() const;
	void RenderFXAA() const;
};
