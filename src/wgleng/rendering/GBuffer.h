#pragma once

#include <GLES3/gl3.h>

class GBuffer {
public:
    GBuffer(uint32_t width, uint32_t height);
    ~GBuffer();
    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;
    GBuffer(GBuffer&&) = delete;
    GBuffer& operator=(GBuffer&&) = delete;
 
    void Resize(uint32_t width, uint32_t height);
    GLuint GetFBO() const { return m_fbo; }

    GLuint GetDepthTexture() const { return m_texDepth; }
    GLuint GetMaterialTexture() const { return m_texMaterial; }
    GLuint GetNormalTexture() const { return m_texNormal; }

private:
    void Create();
    void Destroy();

    uint32_t m_width, m_height;
    GLuint m_fbo;
    GLuint m_texDepth;
    GLuint m_texMaterial;
    GLuint m_texNormal;
};