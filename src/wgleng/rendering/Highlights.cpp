#include "Highlights.h"

uint8_t Highlights::AddHighlight(std::string_view name, const highlight& hl) {
	uint8_t id = GetHighlightId(name);
	if (id != 0) {
		m_changed = true;
		m_highlights[id] = hl;
		return id;
	}
	if (m_highlights.size() + 1 >= m_maxHighlights) return id;

	m_changed = true;
	id = m_highlights.size();
	m_highlightIds[std::string(name)] = id;
	m_highlights.push_back(hl);
	return id;
}

uint8_t Highlights::AddHighlight(const highlight& hl) {
	uint8_t id = 0;
	if (m_highlights.size() + 1 >= m_maxHighlights) return id;
	m_changed = true;

	id = m_highlights.size();
	m_highlights.push_back(hl);
	return id;
}

uint8_t Highlights::GetHighlightId(std::string_view name) {
	auto it = m_highlightIds.find(std::string(name));
	if (it != m_highlightIds.end()) return it->second;
	return 0;
}

Highlights::highlight Highlights::GetHighlight(std::string_view name) {
	uint8_t id = GetHighlightId(name);
	return GetHighlight(id);
}

Highlights::highlight Highlights::GetHighlight(uint8_t highlightId) {
	return m_highlights[highlightId % m_maxHighlights];
}

const std::vector<Highlights::highlight>& Highlights::GetHighlights() {
	return m_highlights;
}

bool Highlights::GetClosestHighlightId(const glm::vec3& color, float epsilon, uint8_t& highlightId) {
	for (unsigned int i = 0; i < m_highlights.size(); i++) {
		const auto& h = m_highlights[i];
		if (glm::distance(h.color, color) < epsilon) {
			highlightId = i;
			return true;
		}
	}
	return false;
}

bool Highlights::HasChanged(bool reset) {
	bool ret = m_changed;
	if (reset) m_changed = false;
	return ret;
}

void Highlights::Init() {
	m_changed = true;
	AddHighlight("default", { {0, 0, 0} });
	AddHighlight("black",   { {-1, -1, -1} });
	AddHighlight("white",   { {1, 1, 1} });
	AddHighlight("red",     { {1, 0, 0} });
	AddHighlight("green",   { {0, 1, 0} });
	AddHighlight("blue",    { {0, 0, 1} });
	AddHighlight("yellow",  { {1, 1, 0} });
	AddHighlight("cyan",    { {0, 1, 1} });
	AddHighlight("magenta", { {1, 0, 1} });
}
void Highlights::Deinit() {
	m_changed = true;
	m_highlightIds.clear();
	m_highlights.clear();
}

void Highlights::Clear() {
	m_changed = true;
	Deinit();
	Init();
}
