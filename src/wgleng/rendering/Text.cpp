#include "Text.h"

#include <GLES3/gl3.h>
#include <charconv>

#include "fonts/arial.h"

void Text::Init() {
	if (const FT_Error error = FT_Init_FreeType(&m_ftlib)) {
		printf("Failed to initialize FreeType library: %d\n", error);
		return;
	}

	LoadFont("arial", {Arial_ttf, Arial_ttf_len});
}
void Text::Deinit() {
	FT_Done_FreeType(m_ftlib);
}

void Text::LoadFont(std::string_view name, std::span<unsigned char> fontData) {
	FT_Face face;
	if (const FT_Error error = FT_New_Memory_Face(m_ftlib, fontData.data(), fontData.size(), 0, &face)) {
		printf("Failed to load font %s: %d\n", name.data(), error);
		return;
	}
	int scale = 48;
	FT_Set_Pixel_Sizes(face, 0, scale);
	// only load first 128 ASCII chars
	std::shared_ptr<fontData_t>& font = m_fonts[std::string(name)];
	font.reset(new fontData_t);
	font->fontName = name;
	font->fontScale = scale;
	font->chars.resize(128);
	std::vector<std::vector<unsigned char>> bitmaps(font->chars.size());
	for (unsigned char c = 0; c < 128; c++) {
		if (const FT_Error error = FT_Load_Char(face, c, FT_LOAD_RENDER)) {
			printf("Failed to load Glyph %d: %d\n", c, error);
			continue;
		}
		FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF);
		charData_t& charData = font->chars[c];
		charData.size = {face->glyph->bitmap.width, face->glyph->bitmap.rows};
		charData.bearing = {face->glyph->bitmap_left, face->glyph->bitmap_top};
		charData.advance = face->glyph->advance.x;
		charData.textureId = c;

		auto& buff = bitmaps[c];
		buff.resize(charData.size.x * charData.size.y);
		std::copy_n(face->glyph->bitmap.buffer, buff.size(), buff.data());

		font->maxCharSize = glm::max(font->maxCharSize, charData.size);
	}
	FT_Done_Face(face);

	// resize bitmaps to max size
	std::vector<unsigned char> newBuff(font->maxCharSize.x * font->maxCharSize.y * font->chars.size());
	for (int i = 0; i < font->chars.size(); i++) {
		auto& charData = font->chars[i];
		auto& buff = bitmaps[i];
		glm::ivec2 oldSize = charData.size;
		uint64_t offset = i * font->maxCharSize.x * font->maxCharSize.y;
		for (int y = 0; y < oldSize.y; y++) {
			std::copy_n(buff.begin() + y * oldSize.x, oldSize.x, newBuff.begin() + offset + y * font->maxCharSize.x);
		}
	}

	// gen textures
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(1, &font->fontTexture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, font->fontTexture);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, font->maxCharSize.x, font->maxCharSize.y,
		font->chars.size(), 0, GL_RED, GL_UNSIGNED_BYTE, newBuff.data());
}
Text::fontData_t::~fontData_t() {
	if (fontTexture == 0) return;
	glDeleteTextures(1, &fontTexture);
}

void Text::UnloadFont(std::string_view name) {
	m_fonts.erase(std::string(name));
}

std::shared_ptr<DrawableText> Text::CreateText(std::string_view fontName, std::string_view text, float wrap) {
	std::shared_ptr<DrawableText> ret;
	auto it = m_fonts.find(std::string(fontName));
	if (it == m_fonts.end() || !(it->second)) {
		printf("Font %s not loaded\n", fontName.data());
		return ret;
	}
	ret.reset(new DrawableText(it->second, text, wrap));
	return ret;
}

DrawableText::DrawableText(const std::shared_ptr<Text::fontData_t>& fontDataPtr, std::string_view text, float wrap)
	: m_fontData{fontDataPtr} {
	std::string str = std::string(text);
	// get highlights
	std::vector<std::pair<int, uint8_t>> highlights;
	for (int i = 0; i < str.size(); i++) {
		char c = str[i];
		// check for highlights
		if (c == '$') {
			str.erase(i, 1);
			if (i >= str.size()) break;
			c = str[i];
			if (c != '<' && c != '$') break;
			if (c == '<') {
				std::string_view highlightStrFrom = std::string_view(str).substr(i + 1);
				auto highlightStrTo = highlightStrFrom.find_first_of('>');
				if (highlightStrTo == std::string_view::npos) break;
				std::string_view highlightStr = highlightStrFrom.substr(0, highlightStrTo);
				uint8_t hl = 0;
				auto result = std::from_chars(highlightStr.data(), highlightStr.data() + highlightStr.size(), hl);
				if (result.ec == std::errc::invalid_argument) {
					break;
				}
				highlights.push_back({i, hl});
				str.erase(highlightStrTo + 2);
			}
		}
	}

	// wrap text
	if (wrap != 0) {
		float maxAdvance = 0;
		for (const auto& c : m_fontData->chars) {
			maxAdvance = glm::max(maxAdvance, static_cast<float>(c.advance >> 6) / m_fontData->fontScale);
		}
		wrap = glm::max(wrap, maxAdvance);

		auto updateHighlightIndexes = [&](int index) {
			for (auto& [i, hl] : highlights) {
				if (i >= index) i++;
			}
		};
		auto getNextWordSize = [&](std::string_view sv) -> std::pair<int, float> {
			int charCount = 0;
			float size = 0;
			for (const char c : sv) {
				if (!std::isalnum(c)) break;
				charCount++;
				if (c >= m_fontData->chars.size()) continue;
				const auto& charData = m_fontData->chars[c];
				size += static_cast<float>(charData.advance >> 6) / m_fontData->fontScale;
			}
			return {charCount, size};
		};
		float x = 0;
		for (int i = 0; i < str.size(); i++) {
			char c = str[i];
			if (c >= m_fontData->chars.size()) continue;
			const auto& charData = m_fontData->chars[c];

			// if newline found, reset x
			if (c == '\n') {
				x = 0;
				continue;
			}

			// get next word size
			auto [charCount, size] = getNextWordSize(std::string_view(str).substr(i));
			if (charCount != 0) {  // word found
				if (size > wrap) { // single word too big
					 if (x != 0) { // if there is something before, add newline
					 	str.insert(i, "\n");
					 	updateHighlightIndexes(i);
					 	x = 0;
					 	i++;
					 }
					 float x2 = 0;
					 for (int j = i; j < i + charCount; j++) {
					 	char c2 = str[j];
					 	if (c2 >= m_fontData->chars.size()) continue;
					 	const auto& charData2 = m_fontData->chars[c2];
					 	x2 += static_cast<float>(charData2.advance >> 6) / m_fontData->fontScale;
					 	if (x2 > wrap) {
					 		str.insert(j, "\n");
					 		updateHighlightIndexes(j);
					 		x = 0;
					 		i = j;
					 		break;
					 	}
					 }
				}
				else if (x + size > wrap) { // adding word would exceed wrap
					if (c == ' ') {
						str[i] = '\n';
					}
					else {
						str.insert(i, "\n");
						updateHighlightIndexes(i);
					}
					x = 0;
				}
				else { // word does not exceed wrap
					x += size;
					i += charCount;
				}
				continue;
			}

			// no word
			size = static_cast<float>(charData.advance >> 6) / m_fontData->fontScale;
			if (x + size > wrap) { // if adding char would exceed wrap, add newline
				if (c == ' ') {
					str[i] = '\n';
				}
				else {
					str.insert(i, "\n");
					updateHighlightIndexes(i);
				}
				x = 0;
				continue;
			}

			// char does not exceed wrap
			x += size;
		}
	}

	// generate vertices
	text = str;
	std::vector<textVertex_t> vertices;
	vertices.reserve(text.size() * 6);
	glm::ivec2 pos{0, 0};
	uint8_t highlightId = 1;
	for (int i = 0; i < text.size(); i++) {
		if (!highlights.empty()) {
			if (highlights.front().first == i) {
				highlightId = highlights.front().second;
				highlights.erase(highlights.begin());
			}
		}

		char c = text[i];
		if (c == '\n') {
			pos.x = 0;
			pos.y -= m_fontData->maxCharSize.y;
			continue;
		}

		if (c >= m_fontData->chars.size()) continue;
		const auto& charData = m_fontData->chars[c];

		// get required values
		const float xPos = pos.x + charData.bearing.x;
		const float yPos = pos.y - (charData.size.y - charData.bearing.y);
		const float w = charData.size.x;
		const float h = charData.size.y;
		const glm::vec2 uv = glm::vec2(charData.size) / glm::vec2(m_fontData->maxCharSize);
		pos.x += (charData.advance >> 6);

		// create verts
		const float scale = 1.f / m_fontData->fontScale;
		vertices.push_back({scale * glm::vec2{xPos, yPos + h}, {0, 0}, charData.textureId, highlightId});
		vertices.push_back({scale * glm::vec2{xPos, yPos}, {0, uv.y}, charData.textureId, highlightId});
		vertices.push_back({scale * glm::vec2{xPos + w, yPos}, {uv.x, uv.y}, charData.textureId, highlightId});

		vertices.push_back({scale * glm::vec2{xPos, yPos + h}, {0, 0}, charData.textureId, highlightId});
		vertices.push_back({scale * glm::vec2{xPos + w, yPos}, {uv.x, uv.y}, charData.textureId, highlightId});
		vertices.push_back({scale * glm::vec2{xPos + w, yPos + h}, {uv.x, 0}, charData.textureId, highlightId});
	}
	// upload verts
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);
	glGenBuffers(1, &m_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(textVertex_t), vertices.data(), GL_STATIC_DRAW);
	m_drawCount = vertices.size();

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(textVertex_t), (void*)offsetof(textVertex_t, position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(textVertex_t), (void*)offsetof(textVertex_t, uv));
	glEnableVertexAttribArray(2);
	glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(textVertex_t), (void*)offsetof(textVertex_t, textureId));
	glEnableVertexAttribArray(3);
	glVertexAttribIPointer(3, 1, GL_UNSIGNED_BYTE, sizeof(textVertex_t), (void*)offsetof(textVertex_t, highlightId));
}
DrawableText::~DrawableText() {
	glDeleteVertexArrays(1, &m_vao);
	glDeleteBuffers(1, &m_vbo);
}
