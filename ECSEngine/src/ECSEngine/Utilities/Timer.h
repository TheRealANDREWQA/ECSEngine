#pragma once
#include "../Core.h"
#include "ecspch.h"

namespace ECSEngine {

	class ECSENGINE_API Timer
	{
	public:
		Timer() : m_name(nullptr) {
			m_start = std::chrono::high_resolution_clock::now();
			m_marker = m_start;
		}
		Timer(const char* name) : m_name(name) {
			m_start = std::chrono::high_resolution_clock::now();
			m_marker = m_start;
		}

		void PrintDuration() const;

		void PrintDurationSinceMarker() const;

		void SetNewStart();

		void SetMarker();

		unsigned long long GetDuration_ns() const;

		unsigned long long GetDuration_us() const;

		unsigned long long GetDuration_ms() const;

		unsigned long long GetDurationSinceMarker_ns() const;

		unsigned long long GetDurationSinceMarker_us() const;

		unsigned long long GetDurationSinceMarker_ms() const;

		~Timer() {
			PrintDuration();
		}

	private:
		std::chrono::high_resolution_clock::time_point m_start;
		std::chrono::high_resolution_clock::time_point m_marker;
		const char* m_name;
	};

}

