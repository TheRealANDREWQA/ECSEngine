#include "ecspch.h"
#include "Timer.h"

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------

	void Timer::DelayStart(size_t nanoseconds)
	{
		m_start += std::chrono::nanoseconds(nanoseconds);
	}

	// -----------------------------------------------------------------------------------------------------------

	void Timer::DelayMarker(size_t nanoseconds)
	{
		m_marker += std::chrono::nanoseconds(nanoseconds);
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

	unsigned long long Timer::GetDuration_ns() const {
		auto end = std::chrono::high_resolution_clock::now();
		size_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
		bool overflow = duration > end.time_since_epoch().count();
		return overflow ? -duration : duration;
	}

	// -----------------------------------------------------------------------------------------------------------

	unsigned long long Timer::GetDurationSinceMarker_ns() const {
		auto end = std::chrono::high_resolution_clock::now();
		size_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_marker).count();
		bool overflow = duration > end.time_since_epoch().count();
		return overflow ? -duration : duration;
	}

	// -----------------------------------------------------------------------------------------------------------

	unsigned long long Timer::GetDuration_ms() const {
		return GetDuration_ns() / 1'000'000;
	}

	// -----------------------------------------------------------------------------------------------------------

	unsigned long long Timer::GetDurationSinceMarker_ms() const {
		return GetDurationSinceMarker_ns() / 1'000'000;
	}

	// -----------------------------------------------------------------------------------------------------------

	unsigned long long Timer::GetDuration_us() const
	{
		return GetDuration_ns() / 1'000;
	}

	// -----------------------------------------------------------------------------------------------------------

	unsigned long long Timer::GetDurationSinceMarker_us() const {
		return GetDurationSinceMarker_ns() / 1'000;
	}

	// -----------------------------------------------------------------------------------------------------------

}
