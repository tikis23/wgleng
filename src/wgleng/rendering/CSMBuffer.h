#pragma once

#include <GLES3/gl3.h>
#include <vector>
#include <glm/glm.hpp>
#include <memory>

#include "../core/Camera.h"

class CSMBuffer {
public:
    CSMBuffer(uint32_t width, uint32_t height, const std::vector<float>& cascadeSplits);
    ~CSMBuffer();
    CSMBuffer(const CSMBuffer&) = delete;
    CSMBuffer& operator=(const CSMBuffer&) = delete;
    CSMBuffer(CSMBuffer&&) = delete;
    CSMBuffer& operator=(CSMBuffer&&) = delete;
 
    uint32_t GetFrustumCount() const { return m_cascadeSplits.size(); }
    // each cascade is the max shadow distance for that cascade, so at least 1 cascade is required
    void SetCascades(const std::vector<float>& cascadeSplits);
    const std::vector<float>& GetCascades() const { return m_cascadeSplits; }
    void Resize(uint32_t width, uint32_t height);

    std::vector<glm::mat4> GetLightSpaceMatrices(const std::shared_ptr<Camera>& camera, const glm::vec3& lightDir) const;

    const std::vector<GLuint> GetFBOs() const { return m_fbos; }
    GLuint GetDepthTextureArray() const { return m_texDepth; }

private:
    void Create();
    void Destroy();

    uint32_t m_width, m_height;
    std::vector<float> m_cascadeSplits;
    std::vector<GLuint> m_fbos;
    GLuint m_texDepth;
};