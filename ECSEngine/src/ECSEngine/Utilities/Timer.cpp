#include "ecspch.h"
#include "Timer.h"

namespace ECSEngine {

	size_t GetFactor(ECS_TIMER_DURATION type) {
		switch (type) {
		case ECS_TIMER_DURATION_NS:
			return 1;
		case ECS_TIMER_DURATION_US:
			return ECS_KB_10;
		case ECS_TIMER_DURATION_MS:
			return ECS_MB_10;
		case ECS_TIMER_DURATION_S:
			return ECS_GB_10;
		}
	}

	// -----------------------------------------------------------------------------------------------------------

	void Timer::DelayStart(int64_t duration, ECS_TIMER_DURATION type)
	{
		// This is fine even for negative numbers since adding with a negative means adding with a
		// large positive which will yield the correct result
		m_start += std::chrono::nanoseconds((size_t)duration) * GetFactor(type);
	}

	// -----------------------------------------------------------------------------------------------------------

	void Timer::DelayMarker(int64_t duration, ECS_TIMER_DURATION type)
	{
		// This is fine even for negative numbers since adding with a negative means adding with a
		// large positive which will yield the correct result
		m_marker += std::chrono::nanoseconds((size_t)duration) * GetFactor(type);
	}

	// -----------------------------------------------------------------------------------------------------------

	void Timer::SetNewStart()
	{
		m_start = std::chrono::high_resolution_clock::now();
	}

	// -----------------------------------------------------------------------------------------------------------

	void Timer::SetMarker() {
		m_marker = std::chrono::high_resolution_clock::now();
	}

	// -----------------------------------------------------------------------------------------------------------

	size_t Timer::GetDuration(ECS_TIMER_DURATION type) const {
		auto end = std::chrono::high_resolution_clock::now();
		size_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
		bool overflow = duration > end.time_since_epoch().count();
		duration = overflow ? -duration : duration;
		return duration / GetFactor(type);
	}

	// -----------------------------------------------------------------------------------------------------------

	size_t Timer::GetDurationSinceMarker(ECS_TIMER_DURATION type) const {
		auto end = std::chrono::high_resolution_clock::now();
		size_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_marker).count();
		bool overflow = duration > end.time_since_epoch().count();
		duration = overflow ? -duration : duration;
		return duration / GetFactor(type);
	}

	// -----------------------------------------------------------------------------------------------------------

}
