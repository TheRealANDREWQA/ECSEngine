#pragma once
#include "../Core.h"
#include "ecspch.h"

namespace ECSEngine {

	enum ECS_TIMER_DURATION : unsigned char {
		ECS_TIMER_DURATION_NS, // Nanoseconds
		ECS_TIMER_DURATION_US, // Microseconds
		ECS_TIMER_DURATION_MS, // Milliseconds
		ECS_TIMER_DURATION_S // Seconds
	};

	struct ECSENGINE_API Timer
	{
		ECS_INLINE Timer() {
			m_start = std::chrono::high_resolution_clock::now();
			m_marker = m_start;
		}

		// Increases/Decreases the start point by the given amount
		void DelayStart(int64_t duration, ECS_TIMER_DURATION type);

		// Increases/Decreases the marker point by the given amount
		void DelayMarker(int64_t nanoseconds, ECS_TIMER_DURATION type);

		void SetNewStart();

		void SetMarker();

		size_t GetDuration(ECS_TIMER_DURATION yep) const;

		size_t GetDurationSinceMarker(ECS_TIMER_DURATION type) const;

		std::chrono::high_resolution_clock::time_point m_start;
		std::chrono::high_resolution_clock::time_point m_marker;
	};

}

