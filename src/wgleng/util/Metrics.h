#pragma once

#include <stdint.h>
#include <variant>

#include "Timer.h"

enum class Metric : uint32_t {
	FRAME_TOTAL = 1 << 0,
	LOGIC_TOTAL = 1 << 1,
	SCRIPTS     = 1 << 2,
	PHYICS      = 1 << 3,
	//===========================//
	RENDER_TOTAL    = 1 << 4,
	UPDATE_MESHES   = 1 << 5,
	UPDATE_UNIFORMS = 1 << 6,
	RENDER_SHADOWS  = 1 << 7,
	RENDER_MESHES   = 1 << 8,
	RENDER_TEXT     = 1 << 9,
	RENDER_LIGHTING = 1 << 10,
	//===========================//
	ENTITY_COUNT     = 1 << 11,
	DRAWN_ENTITES    = 1 << 12,
	SHADOW_ENTITES   = 1 << 13,
	MESH_ENTITES     = 1 << 14,
	TRIANGLES_TOTAL  = 1 << 15,
	TRIANGLES_SHADOW = 1 << 16,
	TRIANGLES_MESHES = 1 << 17,
	//===========================//
	METRIC_COUNT = 18,
	ALL_METRICS  = (1 << METRIC_COUNT) - 1,
};

class Metrics {
private:
	static inline const char* m_metricNames[static_cast<uint32_t>(Metric::METRIC_COUNT)] = {
		"(mix) Frame total ",
		"   (cpu) Logic Total ",
		"   (cpu) Scripts     ",
		"   (cpu) Physics     ",
		//===========================//
		"(mix) Render Total ",
		"   (cpu) Update meshes   ",
		"   (gpu) Update uniforms ",
		"   (gpu) Render shadows  ",
		"   (gpu) Render meshes   ",
		"   (gpu) Render text     ",
		"   (gpu) Render lighting ",
		//===========================//
		"(info) entity count  ",
		"(info) drawn entites ",
		"   (info) shadow entities ",
		"   (info) mesh entities   ",
		"(info) triangle count ",
		"   (info) shadow triangles ",
		"   (info) mesh triangles   ",
	};

public:
	Metrics() = delete;

	static void Show();

	static void Enable(Metric m) {
		m_enabled |= static_cast<uint32_t>(m);
	}
	static void Disable(Metric m) {
		m_enabled &= ~static_cast<uint32_t>(m);
	}
	static bool IsEnabled(Metric m) {
		return (m_enabled & static_cast<uint32_t>(m)) == static_cast<uint32_t>(m);
	}

	static void MeasureDurationStart(Metric m);
	static void MeasureDurationStop(Metric m, bool waitForGpu = false);
	static void SetStaticMetric(Metric m, const std::variant<int64_t, uint64_t, double, std::string>& val);
	static void ResetStaticMetric(Metric m);

private:
	constexpr static uint32_t GetIndex(Metric m);

private:
	static inline uint32_t m_enabled = 0;
	static inline TimePoint m_lastDurationUpdate = TimePoint();

	struct DurationMeasurement {
		DurationMeasurement()
			: start{TimePoint()}, accum{0}, count{0}, avg{0}, active{false} {}

		TimePoint start;
		TimeDuration accum;
		int count;
		double avg;
		bool active;

		void update() {
			if (count == 0) {
				active = false;
				return;
			}
			active = true;
			avg = accum.fMilli() / count;
			accum = 0;
			count = 0;
		}
	};
	struct MeasurementData {
		DurationMeasurement duration;
		std::optional<std::variant<int64_t, uint64_t, double, std::string>> staticMetric;
	};
	static inline MeasurementData m_data[static_cast<uint32_t>(Metric::METRIC_COUNT)] = {};
};
