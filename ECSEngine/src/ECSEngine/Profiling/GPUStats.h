// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "Statistic.h"

namespace ECSEngine {

	struct ECS_REFLECT GPUStats {
		ECS_INLINE GPUStats() {
			memset(this, 0, sizeof(*this));
		}

		// Memory - expressed in MB
		size_t used_memory;

		// Utilization - expressed in percentages 0 - 100
		unsigned char utilization;

		// Temperature - expressed in Celsius degrees
		unsigned char temperature_core;

		// Clocks - expressed in MHz
		unsigned int clock_core;
		unsigned int clock_memory;

		// Power - expressed in W
		float power_draw;
	};

	ECSENGINE_API bool InitializeGPUStatsRecording();

	ECSENGINE_API void FreeGPUStatsRecording();

	ECSENGINE_API bool GetGPUStats(GPUStats* stats);

	namespace Reflection {
		struct ReflectionManager;
	}

	// Must be kept in sync with the number of fields in the GPU stats
	ECS_INLINE constexpr size_t GetGPUStatsCount() {
		return 6;
	}

	ECSENGINE_API Statistic<float>* FillGPUStatsStatistics(
		const Reflection::ReflectionManager* reflection_manager,
		const GPUStats* stats,
		Statistic<float>* statistics
	);

}