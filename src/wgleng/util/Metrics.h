#pragma once

#include <stdint.h>

#include "Timer.h"

enum class Metric : uint32_t {
	FRAME_TOTAL = 1 << 0,
	//===========================//
	LOGIC_TOTAL = 1 << 1,
	SCRIPTS     = 1 << 2,
	PHYICS      = 1 << 3,
	//===========================//
	RENDER_TOTAL    = 1 << 4,
	UPDATE_UNIFORMS = 1 << 5,
	UPDATE_MESHES   = 1 << 6,
	RENDER_SHADOWS  = 1 << 7,
	RENDER_MESHES   = 1 << 8,
	RENDER_TEXT     = 1 << 9,
	RENDER_LIGHTING = 1 << 10,

	//===========================//
	METRIC_COUNT = 11,
	ALL_METRICS  = (1 << METRIC_COUNT) - 1,
};

class Metrics {
private:
	static inline const char* m_metricNames[static_cast<uint32_t>(Metric::METRIC_COUNT)] = {
		"(mix) Frame total",
		//===========================//
		"(cpu) Logic Total",
		"(cpu) Scripts",
		"(cpu) Physics",
		//===========================//
		"(mix) Render Total",
		"(gpu) Update uniforms",
		"(mix) Update meshes",
		"(gpu) Render shadows",
		"(gpu) Render meshes",
		"(gpu) Render text",
		"(gpu) Render lighting",
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

private:
	constexpr static uint32_t GetIndex(Metric m);

private:
	static inline uint32_t m_enabled = 0;
	static inline TimePoint m_lastDurationUpdate = TimePoint();

	struct DurationMeasurement {
		DurationMeasurement()
			: start{TimePoint()}, accum{0}, count{0}, avg{0} {}

		TimePoint start;
		TimeDuration accum;
		int count;
		double avg;

		void update() {
			avg = accum.fMilli() / count;
			accum = 0;
			count = 0;
		}
	};
	struct MeasurementData {
		DurationMeasurement duration;
	};
	static inline MeasurementData m_data[static_cast<uint32_t>(Metric::METRIC_COUNT)] = {};
};