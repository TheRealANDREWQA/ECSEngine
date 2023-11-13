#include "ecspch.h"
#include "GPUStats.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Utilities/File.h"
#include "../Utilities/StringUtilities.h"
#include "../Utilities/ParsingUtilities.h"
#include "../Utilities/ByteUnits.h"

#include "../../Dependencies/NVAPI/nvapi.h"

namespace ECSEngine {

	/*
		At the moment, this works only for Nvidia cards only
	*/
	static NvPhysicalGpuHandle gpu_handle;

	bool InitializeGPUStatsRecording() {
		// Initialize NVAPI
		NvAPI_Status status = NvAPI_Initialize();
		if (status != NVAPI_OK) {
			return false;
		}

		// Retrieve the physical gpu handle into a global variable such that
		// We can reference it later on without having to re-enumerate
		NvU32 gpu_handle_count = 1;
		status = NvAPI_EnumPhysicalGPUs(&gpu_handle, &gpu_handle_count);
		if (status != NVAPI_OK) {
			NvAPI_Unload();
			return false;
		}
		return true;
	}

	void FreeGPUStatsRecording()
	{
		NvAPI_Unload();
	}

	bool GetGPUStats(GPUStats* stats) {
		NV_GPU_MEMORY_INFO_EX memory_info;
		memory_info.version = NV_GPU_MEMORY_INFO_EX_VER;
		NvAPI_Status status = NvAPI_GPU_GetMemoryInfoEx(gpu_handle, &memory_info);
		if (status != NVAPI_OK) {
			return false;
		}

		NV_GPU_CLOCK_FREQUENCIES frequencies;
		frequencies.version = NV_GPU_CLOCK_FREQUENCIES_VER;
		frequencies.ClockType = NV_GPU_CLOCK_FREQUENCIES_CURRENT_FREQ;
		frequencies.reserved = 0;
		frequencies.reserved1 = 0;
		status = NvAPI_GPU_GetAllClockFrequencies(gpu_handle, &frequencies);
		if (status != NVAPI_OK) {
			return false;
		}

		NV_GPU_DYNAMIC_PSTATES_INFO_EX pstate_ex;
		pstate_ex.version = NV_GPU_DYNAMIC_PSTATES_INFO_EX_VER;
		status = NvAPI_GPU_GetDynamicPstatesInfoEx(gpu_handle, &pstate_ex);
		if (status != NVAPI_OK) {
			return false;
		}

		NV_GPU_THERMAL_SETTINGS thermal_settings;
		thermal_settings.version = NV_GPU_THERMAL_SETTINGS_VER;
		status = NvAPI_GPU_GetThermalSettings(gpu_handle, NVAPI_THERMAL_TARGET_ALL, &thermal_settings);
		if (status != NVAPI_OK) {
			return false;
		}

		stats->used_memory = ConvertToByteUnit(memory_info.availableDedicatedVideoMemory - memory_info.curAvailableDedicatedVideoMemory, ECS_BYTE_TO_MB);
		stats->utilization = pstate_ex.utilization[0].bIsPresent ? pstate_ex.utilization[0].percentage : 0;
		stats->temperature_core = thermal_settings.count > 0 ? thermal_settings.sensor[0].currentTemp : 0;
		// These are in kHz and we want MHz
		stats->clock_core = (frequencies.domain[NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS].bIsPresent ? 
			frequencies.domain[NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS].frequency : 0) / ECS_KB_10;
		stats->clock_memory = (frequencies.domain[NVAPI_GPU_PUBLIC_CLOCK_MEMORY].bIsPresent ?
			frequencies.domain[NVAPI_GPU_PUBLIC_CLOCK_MEMORY].frequency : 0) / ECS_KB_10;

		// At the moment there is no power API
		stats->power_draw = 0;

		return true;
	}

	Statistic<float>* FillGPUStatsStatistics(
		const Reflection::ReflectionManager* reflection_manager, 
		const GPUStats* stats, 
		Statistic<float>* statistics
	)
	{
		return FillStatisticsForReflectionType(reflection_manager, STRING(GPUStats), stats, statistics);
	}

}