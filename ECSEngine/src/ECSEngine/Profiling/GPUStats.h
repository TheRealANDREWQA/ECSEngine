// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "Statistic.h"

namespace ECSEngine {

	enum GraphicsVendor : unsigned char {
		ECS_GRAPHICS_VENDOR_NVIDIA,
		ECS_GRAPHICS_VENDOR_AMD,
		ECS_GRAPHICS_VENDOR_INTEL,
		ECS_GRAPHICS_VENDOR_COUNT
	};

	// Tries to determine the GPU vendor for the a device identified with a string description
	// And a vendor_id.
	ECSENGINE_API GraphicsVendor DetermineGraphicsVendor(Stream<char> description, size_t vendor_id);

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

	enum ECS_INITIALIZE_GPU_STATS_STATUS : unsigned char {
		ECS_INITIALIZE_GPU_STATS_OK,
		ECS_INITIALIZE_GPU_STATS_FAILURE,
		ECS_INITIALIZE_GPU_STATS_UNSUPPORTED_VENDOR
	};

	ECSENGINE_API ECS_INITIALIZE_GPU_STATS_STATUS InitializeGPUStatsRecording(Stream<char> gpu_description, size_t vendor_id, size_t device_id);

	ECSENGINE_API void FreeGPUStatsRecording();

	ECSENGINE_API bool GetGPUStats(GPUStats* stats);

	namespace Reflection {
		struct ReflectionManager;
	}

	ECSENGINE_API Statistic<float>* FillGPUStatsStatistics(
		const Reflection::ReflectionManager* reflection_manager,
		const GPUStats* stats,
		Statistic<float>* statistics
	);

}