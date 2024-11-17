#include "Metrics.h"

#include <bit>
#include <GLES3/gl3.h>

#include "../vendor/imgui/imgui.h"

void Metrics::Show() {
	if (m_enabled == 0) return;
	const auto now = TimePoint();
	bool updateData = false;
	if (now - m_lastDurationUpdate > 500ms) {
		m_lastDurationUpdate = now;
		updateData = true;
	}

	// show window
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0, 0, 0, 0.3f});
	if (ImGui::Begin("Metrics", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize)) {
		for (uint32_t i = 0; i < static_cast<uint32_t>(Metric::METRIC_COUNT); i++) {
			if (!IsEnabled(static_cast<Metric>(1 << i))) continue;
			auto& data = m_data[i];

			// duration data
			if (updateData) data.duration.update();
			if (data.duration.active) {
				ImGui::Text("%s: %.3f ms", m_metricNames[i], data.duration.avg);
			}

			// static data
			if (data.staticMetric.has_value()) {
				std::visit([&]<typename T0>(const T0& val) {
					if constexpr (std::is_same_v<T0, int64_t>) {
						ImGui::Text("%s: %lld", m_metricNames[i], val);
					}
					else if constexpr (std::is_same_v<T0, uint64_t>) {
						ImGui::Text("%s: %llu", m_metricNames[i], val);
					}
					else if constexpr (std::is_same_v<T0, double>) {
						ImGui::Text("%s: %.3f", m_metricNames[i], val);
					}
					else if constexpr (std::is_same_v<T0, std::string>) {
						ImGui::Text("%s: %s", m_metricNames[i], val.c_str());
					}
				}, data.staticMetric.value());
			}
		}
		ImGui::End();
		ImGui::PopStyleColor();
	}
}
void Metrics::MeasureDurationStart(Metric m) {
	if (!IsEnabled(m)) return;
	auto& data = m_data[GetIndex(m)];
	data.duration.start = TimePoint();
}
void Metrics::MeasureDurationStop(Metric m, bool waitForGpu) {
	if (!IsEnabled(m)) return;
	if (waitForGpu) glFinish();
	auto& data = m_data[GetIndex(m)];
	data.duration.accum += TimePoint() - data.duration.start;
	data.duration.count++;
}
void Metrics::SetStaticMetric(Metric m, const std::variant<int64_t, uint64_t, double, std::string>& val) {
	auto& data = m_data[GetIndex(m)];
	data.staticMetric = val;
}
void Metrics::ResetStaticMetric(Metric m) {
	auto& data = m_data[GetIndex(m)];
	data.staticMetric.reset();
}

constexpr uint32_t Metrics::GetIndex(Metric m) {
	return std::bit_width(static_cast<uint32_t>(m)) - 1;
}
