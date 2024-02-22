#pragma once
#include "../Core.h"
#include <chrono>

namespace ECSEngine {

	enum ECS_TIMER_DURATION : unsigned char {
		ECS_TIMER_DURATION_NS, // Nanoseconds
		ECS_TIMER_DURATION_US, // Microseconds
		ECS_TIMER_DURATION_MS, // Milliseconds
		ECS_TIMER_DURATION_S // Seconds
	};

	ECSENGINE_API size_t GetTimeFactor(ECS_TIMER_DURATION type);

	ECSENGINE_API float GetTimeFactorFloat(ECS_TIMER_DURATION type);

	ECSENGINE_API float GetTimeFactorFloatInverse(ECS_TIMER_DURATION type);

#define ECS_MINUTES_AS_SECONDS(minutes) (60 * (minutes))
#define ECS_MINUTES_AS_MILLISECONDS(minutes) (1000 * ECS_MINUTES_AS_SECONDS(minutes))

#define ECS_HOURS_AS_SECONDS(hours) (60 * 60 * (hours))
#define ECS_HOURS_AS_MILLISECONDS(hours) (1000 * ECS_HOURS_AS_SECONDS(hours))

#define ECS_DAYS_AS_SECONDS(days) (24 * 60 * 60 * (days))
#define ECS_DAYS_AS_MILLISECONDS(days) (1000 * ECS_DAYS_AS_SECONDS(days))

	struct ECSENGINE_API Timer
	{
		ECS_INLINE Timer() {
			m_start = std::chrono::high_resolution_clock::now();
			m_marker = m_start;
		}

		// Increases/Decreases the start point by the given amount
		ECS_INLINE void DelayStart(int64_t duration, ECS_TIMER_DURATION type) {
			// This is fine even for negative numbers since adding with a negative means adding with a
			// large positive which will yield the correct result
			m_start += std::chrono::nanoseconds((size_t)duration) * GetTimeFactor(type);
		}

		// Increases/Decreases the marker point by the given amount
		ECS_INLINE void DelayMarker(int64_t nanoseconds, ECS_TIMER_DURATION type) {
			// This is fine even for negative numbers since adding with a negative means adding with a
			// large positive which will yield the correct result
			m_marker += std::chrono::nanoseconds((size_t)nanoseconds) * GetTimeFactor(type);
		}

		ECS_INLINE bool IsUninitialized() const {
			size_t start = *(size_t*)&m_start;
			size_t marker = *(size_t*)&m_marker;
			return start == 0 && marker == 0;
		}

		ECS_INLINE void SetNewStart() {
			m_start = std::chrono::high_resolution_clock::now();
		}

		ECS_INLINE void SetMarker() {
			m_marker = std::chrono::high_resolution_clock::now();
		}

		ECS_INLINE void SetUninitialized() {
			// m_start and m_marker are just unsigned long longs but the C++
			// API makes it hard to even set these to 0
			memset(this, 0, sizeof(*this));
		}

		size_t GetDuration(ECS_TIMER_DURATION type) const;

		size_t GetDurationSinceMarker(ECS_TIMER_DURATION type) const;

		float GetDurationFloat(ECS_TIMER_DURATION type) const;

		float GetDurationSinceMarkerFloat(ECS_TIMER_DURATION type) const;

		std::chrono::high_resolution_clock::time_point m_start;
		std::chrono::high_resolution_clock::time_point m_marker;
	};

}

