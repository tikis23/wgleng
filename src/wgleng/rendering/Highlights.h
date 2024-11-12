#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <stdint.h>
#include <string_view>
#include <glm/glm.hpp>

class Highlights {
public:
	Highlights() = delete;
	~Highlights() = delete;
	struct highlight {
		glm::vec3 color;
	};

	static uint8_t AddHighlight(std::string_view name, const highlight& hl);
	static uint8_t AddHighlight(const highlight& hl);
	static uint8_t GetHighlightId(std::string_view name);
	static highlight GetHighlight(std::string_view name);
	static highlight GetHighlight(uint8_t highlightId);
	static const std::vector<highlight>& GetHighlights();

	static bool GetClosestHighlightId(const glm::vec3& color, float epsilon, uint8_t& highlightId);

	static bool HasChanged(bool reset = false);

	static void Init();
	static void Deinit();
	static void Clear();

private:
	static inline constexpr uint32_t m_maxHighlights = 256;
	static inline bool m_changed = false;
	static inline std::unordered_map<std::string, uint8_t> m_highlightIds;
	static inline std::vector<highlight> m_highlights;
};