#pragma once

#include <memory>
#include <deque>
#include <string>

#include "ShaderProgram.h"
#include "../core/Scene.h"
#include "UniformBuffer.h"
#include "Mesh.h"
#include "GBuffer.h"
#include "CSMBuffer.h"

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void Render(bool isHidden, const std::shared_ptr<Scene>& scene);

    void SetViewportSize(int32_t width, int32_t height);

    enum class ShaderType : uint64_t {
        ALL      = ~0U,
	    DEBUG    = 1U << 0,
		MESH     = 1U << 1,
		LIGHTING = 1U << 2,
		CSM      = 1U << 3
    };

    void ReloadShaders(ShaderType shaders = ShaderType::ALL);
private:
    void LoadShaderFromFile(std::string file);
    void SetupUniforms();
    bool m_shadersLoading = false;
    std::deque<std::string> m_shaderLoadingQueue;
    std::deque<std::string> m_shaderLoadingOutput;
    std::deque<std::unique_ptr<ShaderProgram>*> m_shaderLoadingPrograms;

    uint32_t m_nextUniformBindingIndex = 0;
    uint32_t GetNextUniformBindingIndex() { return m_nextUniformBindingIndex++; }

    int32_t m_viewportWidth, m_viewportHeight;
    GBuffer m_gbuffer;

    int32_t m_csmWidth, m_csmHeight;
    CSMBuffer m_csmbuffer;

    std::unique_ptr<ShaderProgram> m_debugProgram;
    std::unique_ptr<ShaderProgram> m_meshProgram;
    std::unique_ptr<ShaderProgram> m_lightingProgram;
    std::vector<std::unique_ptr<ShaderProgram>> m_csmPrograms;

    constexpr static inline uint32_t m_matricesPerUniformBuffer = 256;
    constexpr static inline uint32_t m_maxCSMFrustums = 8;
    constexpr static inline uint32_t m_materialsPerUniformBuffer = 1024;

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

    UniformBuffer<void> m_modelUniform; // uses dynamic amount of glm::mat4's
    UniformBuffer<CameraUniform> m_cameraUniform;
    UniformBuffer<CSMUniform> m_csmUniform;
    UniformBuffer<LightingInfoUniform> m_lightingInfoUniform;
    UniformBuffer<MaterialUniform> m_materialUniform;
};