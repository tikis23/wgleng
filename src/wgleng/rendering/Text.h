#pragma once

#include <ft2build.h>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include FT_FREETYPE_H

class DrawableText;
class Text {
public:
	Text() = delete;

	static void Init();
	static void Deinit();
	static void LoadFont(std::string_view name, std::span<unsigned char> fontData, int size = 48);
	static void UnloadFont(std::string_view name);

	// To use '$' in text, use "$$". To set highlightId use "$<highlightId>".
	static std::shared_ptr<DrawableText> CreateText(std::string_view fontName, std::string_view text, float wrap = 0);
private:
	friend class DrawableText;
	static inline FT_Library m_ftlib;

	struct charData_t {
		uint32_t textureId;
		glm::ivec2 size;
		glm::ivec2 bearing;
		uint32_t advance;
	};
	struct fontData_t {
		fontData_t() = default;
		~fontData_t();
		std::string fontName;
		std::vector<charData_t> chars;
		glm::ivec2 maxCharSize{0, 0};
		uint32_t fontTexture{0};
		float fontScale{0};
	};
	static inline std::unordered_map<std::string, std::shared_ptr<fontData_t>> m_fonts;
};

class DrawableText {
public:
	DrawableText() = delete;
	~DrawableText();

	uint32_t GetVAO() const { return m_vao; }
	uint32_t GetFontTexture() const { return m_fontData->fontTexture; }
	uint32_t GetDrawCount() const { return m_drawCount; }
	const std::string& GetFontName() const { return m_fontData->fontName; }

	glm::vec2 GetTextSize() const { return m_textSize; }

	// if true, text is 2D
	// if false, text is 3D
	// true by default
	bool useOrtho = true;
	glm::vec3 position{0, 0, 0};
	glm::vec3 rotation{0, 0, 0};
	glm::vec3 scale{1, 1, 1};

	// useful for 2D text. Ignored by 3D text.
	// if true, position is in range [0, 1]
	// if false, position is in world space / screen space
	// false by default
	bool normalizedCoordinates = false;

	// if true, use transform matrix for rendering. Pos, rot, and scale may still be used as local transforms.
	// if false, use pos, rot, and scale for rendering. Transform matrix will be ignored.
	// false by default
	bool useTransform = false;
	glm::mat4 transform{1};

private:
	friend class Text;
	DrawableText(const std::shared_ptr<Text::fontData_t>& fontDataPtr, std::string_view text, float wrap);

	glm::vec2 m_textSize{};

	struct textVertex_t {
		glm::vec2 position;
		glm::vec2 uv;
		uint32_t textureId;
		uint8_t highlightId;
	};

	std::shared_ptr<Text::fontData_t> m_fontData;
	uint32_t m_vao;
	uint32_t m_vbo;
	uint32_t m_drawCount;
};
