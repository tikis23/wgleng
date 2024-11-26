#include "FXAABuffer.h"

#include <iostream>
#include <array>

FXAABuffer::FXAABuffer(uint32_t width, uint32_t height)
    : m_width{width}, m_height{height} {
    Create();
}
FXAABuffer::~FXAABuffer() {
    Destroy();
}

void FXAABuffer::Resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    Destroy();
    Create();
}

void FXAABuffer::Create() {
	glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glGenTextures(1, &m_texColor);

    glBindTexture(GL_TEXTURE_2D, m_texColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texColor, 0);

    constexpr GLuint attachments[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, attachments);

    // Check Status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "error while initializing FXAABuffer: " << glCheckFramebufferStatus(GL_FRAMEBUFFER) << '\n';
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void FXAABuffer::Destroy() {
    glDeleteFramebuffers(1, &m_fbo);
    glDeleteTextures(1, &m_texColor);
    m_fbo = 0;
    m_texColor = 0;
}