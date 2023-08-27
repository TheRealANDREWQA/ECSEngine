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

	// Inline this function such that it gets inlined for sure in the other functions
	ECS_INLINE size_t GetDurationImpl(std::chrono::high_resolution_clock::time_point time_point, ECS_TIMER_DURATION type) {
		auto end = std::chrono::high_resolution_clock::now();
		size_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - time_point).count();
		bool overflow = duration > end.time_since_epoch().count();
		duration = overflow ? -duration : duration;
		return duration / GetFactor(type);
	}

	// -----------------------------------------------------------------------------------------------------------

	size_t Timer::GetDuration(ECS_TIMER_DURATION type) const {
		return GetDurationImpl(m_start, type);
	}

	// -----------------------------------------------------------------------------------------------------------

	size_t Timer::GetDurationSinceMarker(ECS_TIMER_DURATION type) const {
		return GetDurationImpl(m_marker, type);
	}

	// -----------------------------------------------------------------------------------------------------------

}
