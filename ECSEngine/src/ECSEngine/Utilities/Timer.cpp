#include "ecspch.h"
#include "Timer.h"

namespace ECSEngine {

	size_t GetTimeFactor(ECS_TIMER_DURATION type) {
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

		// Shouldn't be reached
		return 0;
	}

	// This function avoids the int to float conversion penalty
	// (In release and distribution configurations)
	float GetTimeFactorFloat(ECS_TIMER_DURATION type)
	{
		switch (type) {
		case ECS_TIMER_DURATION_NS:
			return 1.0f;
		case ECS_TIMER_DURATION_US:
			return (float)ECS_KB_10;
		case ECS_TIMER_DURATION_MS:
			return (float)ECS_MB_10;
		case ECS_TIMER_DURATION_S:
			return (float)ECS_GB_10;
		}

		// Shouldn't be reached
		return 0.0f;
	}

	float GetTimeFactorFloatInverse(ECS_TIMER_DURATION type) {
		switch (type) {
		case ECS_TIMER_DURATION_NS:
			return 1.0f;
		case ECS_TIMER_DURATION_US:
			return 1.0f / (float)ECS_KB_10;
		case ECS_TIMER_DURATION_MS:
			return 1.0f / (float)ECS_MB_10;
		case ECS_TIMER_DURATION_S:
			return 1.0f / (float)ECS_GB_10;
		}

		// Shouldn't be reached
		return 0.0f;
	}

	// -----------------------------------------------------------------------------------------------------------

	size_t GetDurationNanoseconds(std::chrono::high_resolution_clock::time_point time_point) {
		auto end = std::chrono::high_resolution_clock::now();
		size_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - time_point).count();
		bool overflow = duration > end.time_since_epoch().count();
		duration = overflow ? (size_t)(-(int64_t)duration) : duration;
		return duration;
	}

	// Inline this function such that it gets inlined for sure in the other functions
	ECS_INLINE size_t GetDurationInt(std::chrono::high_resolution_clock::time_point time_point, ECS_TIMER_DURATION type) {
		return GetDurationNanoseconds(time_point) / GetTimeFactor(type);
	}

	ECS_INLINE float GetDurationFloat(std::chrono::high_resolution_clock::time_point time_point, ECS_TIMER_DURATION type) {
		return GetDurationNanoseconds(time_point) * GetTimeFactorFloatInverse(type);
	}

	// -----------------------------------------------------------------------------------------------------------

	size_t Timer::GetDuration(ECS_TIMER_DURATION type) const {
		return GetDurationInt(m_start, type);
	}

	// -----------------------------------------------------------------------------------------------------------

	size_t Timer::GetDurationSinceMarker(ECS_TIMER_DURATION type) const {
		return GetDurationInt(m_marker, type);
	}

	// -----------------------------------------------------------------------------------------------------------

	float Timer::GetDurationFloat(ECS_TIMER_DURATION type) const
	{
		return ECSEngine::GetDurationFloat(m_start, type);
	}

	// -----------------------------------------------------------------------------------------------------------

	float Timer::GetDurationSinceMarkerFloat(ECS_TIMER_DURATION type) const
	{
		return ECSEngine::GetDurationFloat(m_marker, type);
	}

	// -----------------------------------------------------------------------------------------------------------

}
