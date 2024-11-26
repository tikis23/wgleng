#pragma once

#include <GLES3/gl3.h>
#include <stdint.h>

class FXAABuffer {
public:
	FXAABuffer() = default;
	FXAABuffer(uint32_t width, uint32_t height);
	~FXAABuffer();
	FXAABuffer(const FXAABuffer&) = delete;
	FXAABuffer& operator=(const FXAABuffer&) = delete;
	FXAABuffer(FXAABuffer&&) = delete;
	FXAABuffer& operator=(FXAABuffer&&) = delete;

	void Resize(uint32_t width, uint32_t height);
	GLuint GetFBO() const { return m_fbo; }

	GLuint GetColorTexture() const { return m_texColor; }

private:
	void Create();
	void Destroy();

	uint32_t m_width, m_height;
	GLuint m_fbo;
	GLuint m_texColor;
};
