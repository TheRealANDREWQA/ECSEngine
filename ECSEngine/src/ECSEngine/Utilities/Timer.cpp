#include "ecspch.h"
#include "Timer.h"

namespace ECSEngine {

	void Timer::PrintDuration() const {
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
		// print function
		//ECSENGINE_CORE_INFO("Function {0} took {1} nanoseconds -- {2} milliseconds", m_name, duration, duration / 1'000'000);
	}

	void Timer::PrintDurationSinceMarker() const {
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_marker).count();
		// print function
		//ECSENGINE_CORE_INFO("Function {0} took {1} nanoseconds -- {2} milliseconds", m_name, duration, duration / 1'000'000);
	}

	void Timer::SetNewStart()
	{
		m_start = std::chrono::high_resolution_clock::now();
	}

	void Timer::SetMarker() {
		m_marker = std::chrono::high_resolution_clock::now();
	}

	unsigned long long Timer::GetDuration_ns() const {
		auto end = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
	}

	unsigned long long Timer::GetDurationSinceMarker_ns() const {
		auto end = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_marker).count();
	}

	unsigned long long Timer::GetDuration_ms() const {
		auto end = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start).count();
	}

	unsigned long long Timer::GetDurationSinceMarker_ms() const {
		auto end = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(end - m_marker).count();
	}

	unsigned long long Timer::GetDuration_us() const
	{
		auto end = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count();
	}

	unsigned long long Timer::GetDurationSinceMarker_us() const {
		auto end = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::microseconds>(end - m_marker).count();
	}

}
