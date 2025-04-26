#include "ecspch.h"
#include "GPUStats.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Utilities/File.h"
#include "../Utilities/StringUtilities.h"
#include "../Utilities/ParsingUtilities.h"
#include "../Utilities/ByteUnits.h"

#include "../../Dependencies/NVAPI/nvapi.h"
#include "../../Dependencies/ADLX/SDK/ADLXHelper/Windows/Cpp/ADLXHelper.h"
#include "../../Dependencies/ADLX/SDK/Include/ADLX.h"
#include "../../Dependencies/ADLX/SDK/Include/IPerformanceMonitoring.h"
#include "../../Dependencies/ADLX/SDK/Include/IPerformanceMonitoring2.h"

namespace ECSEngine {

	/*
		At the moment, this works only for Nvidia and AMD cards only (no Intel).
	*/

	struct NvidiaData {
		NvPhysicalGpuHandle gpu_handle;
	};

	struct AMDData {
		ADLXHelper helper;
		adlx::IADLXGPUPtr gpu;
		adlx::IADLXPerformanceMonitoringServicesPtr performance_services;
		// These are cached at initialization time
		bool is_gpu_usage_supported = false;
		bool is_gpu_used_memory_supported = false;
		bool is_gpu_temperature_supported = false;
		bool is_gpu_clock_core_supported = false;
		bool is_gpu_clock_memory_supported = false;
		bool is_power_draw_supported = false;
	};

	static NvidiaData NVIDIA;
	static AMDData AMD;
	static bool is_AMD_active = false;
	static bool is_NV_active = false;

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsVendor DetermineGraphicsVendor(Stream<char> description, size_t vendor_id) {
		// Try to deduce the vendor from the PCI ID, which can be found at https://github.com/pciutils/pciids/blob/master/pci.ids
		GraphicsVendor vendor_from_id = ECS_GRAPHICS_VENDOR_COUNT;
		switch (vendor_id) {
		case 0x10DE:
			vendor_from_id = ECS_GRAPHICS_VENDOR_NVIDIA;
			break;
		case 0x1002:
			vendor_from_id = ECS_GRAPHICS_VENDOR_AMD;
			break;
		case 0x8086:
			vendor_from_id = ECS_GRAPHICS_VENDOR_INTEL;
			break;
		}

		if (vendor_from_id != ECS_GRAPHICS_VENDOR_COUNT) {
			return vendor_from_id;
		}

		// In case we couldn't determine the vendor from its ID, use a simple string search
		// In the description string
		if (FindFirstToken(description, "AMD").size > 0 || FindFirstToken(description, "Radeon").size > 0) {
			return ECS_GRAPHICS_VENDOR_AMD;
		}

		if (FindFirstToken(description, "NVIDIA").size > 0 || FindFirstToken(description, "Nvidia").size > 0 ||
			FindFirstToken(description, "GTX ").size > 0 || FindFirstToken(description, "RTX ").size > 0) {
			return ECS_GRAPHICS_VENDOR_NVIDIA;
		}

		// At the moment, for intel, search just for the company name, the branding is less consistent
		if (FindFirstToken(description, "Intel").size > 0 || FindFirstToken(description, "INTEL").size > 0) {
			return ECS_GRAPHICS_VENDOR_INTEL;
		}

		return ECS_GRAPHICS_VENDOR_COUNT;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	static bool InitializeNvidia(Stream<char> gpu_description, size_t vendor_id, size_t device_id) {
		// For the moment, assume that there is a single NVIDIA gpu per system

		// Initialize NVAPI
		NvAPI_Status status = NvAPI_Initialize();
		if (status != NVAPI_OK) {
			return false;
		}

		// Retrieve the physical gpu handle into a global variable such that
		// We can reference it later on without having to re-enumerate
		ECS_STACK_CAPACITY_STREAM(NvPhysicalGpuHandle, gpu_handles, 32);
		NvU32 gpu_handle_count = gpu_handles.capacity;;
		status = NvAPI_EnumPhysicalGPUs(gpu_handles.buffer, &gpu_handle_count);
		if (status != NVAPI_OK) {
			NvAPI_Unload();
			return false;
		}

		bool was_found = false;
		for (NvU32 index = 0; index < gpu_handle_count; index++) {
			NvU32 current_device_id;
			NvU32 current_subsys_id;
			NvU32 current_revision_id;
			NvU32 current_ext_device_id;
			if (NvAPI_GPU_GetPCIIdentifiers(gpu_handles[index], &current_device_id, &current_subsys_id, &current_revision_id, &current_ext_device_id) == NVAPI_OK) {
				// The external device ID is the one we are interested in
				if (device_id == current_ext_device_id) {
					NVIDIA.gpu_handle = gpu_handles[index];
					was_found = true;
					break;
				}
			}
		}

		if (!was_found) {
			NvAPI_Unload();
			return false;
		}

		is_NV_active = true;
		return true;
	}

	static bool InitializeAMD(Stream<char> gpu_description, size_t vendor_id, size_t device_id) {
		// This code is adapted from the AMD ADLX Performance Monitoring sample
		if (!ADLX_SUCCEEDED(AMD.helper.Initialize())) {
			return false;
		}

		if (!ADLX_SUCCEEDED(AMD.helper.GetSystemServices()->GetPerformanceMonitoringServices(&AMD.performance_services))) {
			AMD.helper.Terminate();
			return false;
		}

		adlx::IADLXGPUListPtr gpu_list;
		if (!ADLX_SUCCEEDED(AMD.helper.GetSystemServices()->GetGPUs(&gpu_list))) {
			// Clean up the interfaces that were retrieved
			AMD.performance_services.Release();
			AMD.helper.Terminate();
			return false;
		}

		// Determine if we find the GPU that was provided in the GPU list
		bool gpu_was_found = false;
		size_t gpu_count = gpu_list->Size();
		for (size_t index = 0; index < gpu_count; index++) {
			adlx::IADLXGPUPtr gpu;
			if (ADLX_SUCCEEDED(gpu_list->At(index, &gpu))) {
				const char* gpu_device_id_string;
				if (ADLX_SUCCEEDED(gpu->DeviceId(&gpu_device_id_string))) {
					size_t current_gpu_device_id = ConvertHexToInt(Stream<char>(gpu_device_id_string));
					if (current_gpu_device_id == device_id) {
						// Ensure that we can retrieve the metrics support for the GPU
						adlx::IADLXGPUMetricsSupportPtr metrics_support;
						if (!ADLX_SUCCEEDED(AMD.performance_services->GetSupportedGPUMetrics(gpu, &metrics_support))) {
							// Clean up the interfaces that were retrieved
							AMD.performance_services.Release();
							AMD.helper.Terminate();
							return false;
						}

						// We found a match, can exit
						AMD.gpu = gpu;
						gpu_was_found = true;

						// Before exiting, retrieve the supported features
						// Don't check for the result, it is already defaulted to false
						metrics_support->IsSupportedGPUUsage(&AMD.is_gpu_usage_supported);
						metrics_support->IsSupportedGPUVRAM(&AMD.is_gpu_used_memory_supported);
						metrics_support->IsSupportedGPUTemperature(&AMD.is_gpu_temperature_supported);
						metrics_support->IsSupportedGPUClockSpeed(&AMD.is_gpu_clock_core_supported);
						metrics_support->IsSupportedGPUVRAMClockSpeed(&AMD.is_gpu_clock_memory_supported);
						// Use total board power, as the normal power is likely not be available
						metrics_support->IsSupportedGPUTotalBoardPower(&AMD.is_power_draw_supported);

						break;
					}
				}
			}
		}

		if (!gpu_was_found) {
			// Clean up the interfaces that were retrieved
			AMD.performance_services.Release();
			AMD.helper.Terminate();
			return false;
		}

		is_AMD_active = true;
		return true;
	}

	ECS_INITIALIZE_GPU_STATS_STATUS InitializeGPUStatsRecording(Stream<char> gpu_description, size_t vendor_id, size_t device_id) {
		GraphicsVendor vendor = DetermineGraphicsVendor(gpu_description, vendor_id);
		if (vendor == ECS_GRAPHICS_VENDOR_NVIDIA) {
			return InitializeNvidia(gpu_description, vendor_id, device_id) ? ECS_INITIALIZE_GPU_STATS_OK : ECS_INITIALIZE_GPU_STATS_FAILURE;
		}
		else if (vendor == ECS_GRAPHICS_VENDOR_AMD) {
			return InitializeAMD(gpu_description, vendor_id, device_id) ? ECS_INITIALIZE_GPU_STATS_OK : ECS_INITIALIZE_GPU_STATS_FAILURE;
		}
		else {
			// Intel or other vendors not supported at the moment
			return ECS_INITIALIZE_GPU_STATS_UNSUPPORTED_VENDOR;
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	static void FreeNVStats() {
		NvAPI_Unload();
	}

	static void FreeAMDStats() {
		AMD.gpu.Release();
		AMD.performance_services.Release();
		AMD.helper.Terminate();
	}

	void FreeGPUStatsRecording()
	{
		if (is_NV_active) {
			FreeNVStats();
		}
		else if (is_AMD_active) {
			FreeAMDStats();
		}
		else {
			// For Intel or other vendors, there is nothing to be done at the moment
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	static bool GetNVStats(GPUStats* stats) {
		NV_GPU_MEMORY_INFO_EX memory_info;
		memory_info.version = NV_GPU_MEMORY_INFO_EX_VER;
		NvAPI_Status status = NvAPI_GPU_GetMemoryInfoEx(NVIDIA.gpu_handle, &memory_info);
		if (status != NVAPI_OK) {
			return false;
		}

		NV_GPU_CLOCK_FREQUENCIES frequencies;
		frequencies.version = NV_GPU_CLOCK_FREQUENCIES_VER;
		frequencies.ClockType = NV_GPU_CLOCK_FREQUENCIES_CURRENT_FREQ;
		frequencies.reserved = 0;
		frequencies.reserved1 = 0;
		status = NvAPI_GPU_GetAllClockFrequencies(NVIDIA.gpu_handle, &frequencies);
		if (status != NVAPI_OK) {
			return false;
		}

		NV_GPU_DYNAMIC_PSTATES_INFO_EX pstate_ex;
		pstate_ex.version = NV_GPU_DYNAMIC_PSTATES_INFO_EX_VER;
		status = NvAPI_GPU_GetDynamicPstatesInfoEx(NVIDIA.gpu_handle, &pstate_ex);
		if (status != NVAPI_OK) {
			return false;
		}

		NV_GPU_THERMAL_SETTINGS thermal_settings;
		thermal_settings.version = NV_GPU_THERMAL_SETTINGS_VER;
		status = NvAPI_GPU_GetThermalSettings(NVIDIA.gpu_handle, NVAPI_THERMAL_TARGET_ALL, &thermal_settings);
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

	static bool GetAMDStats(GPUStats* stats) {
		adlx::IADLXGPUMetricsPtr gpu_metrics;

		if (!ADLX_SUCCEEDED(AMD.performance_services->GetCurrentGPUMetrics(AMD.gpu, &gpu_metrics))) {
			return false;
		}

		if (AMD.is_gpu_clock_core_supported) {
			adlx_int clock_speed = 0;		
			stats->clock_core = ADLX_SUCCEEDED(gpu_metrics->GPUClockSpeed(&clock_speed)) ? clock_speed : 0;
		}
		else {
			stats->clock_core = 0;
		}
		
		if (AMD.is_gpu_clock_memory_supported) {
			adlx_int clock_speed = 0;
			stats->clock_memory = ADLX_SUCCEEDED(gpu_metrics->GPUVRAMClockSpeed(&clock_speed)) ? clock_speed : 0;
		}
		else {
			stats->clock_memory = 0;
		}

		if (AMD.is_gpu_temperature_supported) {
			adlx_double temperature = 0.0;
			stats->temperature_core = ADLX_SUCCEEDED(gpu_metrics->GPUTemperature(&temperature)) ? (unsigned char)temperature : 0;
		}
		else {
			stats->temperature_core = 0;
		}

		if (AMD.is_gpu_usage_supported) {
			adlx_double usage = 0.0;
			stats->utilization = ADLX_SUCCEEDED(gpu_metrics->GPUUsage(&usage)) ? (unsigned char)usage : 0;
		}

		if (AMD.is_gpu_used_memory_supported) {
			adlx_int size = 0.0;
			stats->used_memory = ADLX_SUCCEEDED(gpu_metrics->GPUVRAM(&size)) ? (size_t)size : 0;
		}
		else {
			stats->used_memory = 0;
		}

		if (AMD.is_power_draw_supported) {
			adlx_double power_draw = 0.0;
			stats->power_draw = ADLX_SUCCEEDED(gpu_metrics->GPUTotalBoardPower(&power_draw)) ? (float)power_draw : 0.0f;
		}
		else {
			stats->power_draw = 0.0f;
		}

		return true;
	}

	bool GetGPUStats(GPUStats* stats) {
		if (is_NV_active) {
			return GetNVStats(stats);
		}
		else if (is_AMD_active) {
			return GetAMDStats(stats);
		}
		else {
			return false;
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Statistic<float>* FillGPUStatsStatistics(
		const Reflection::ReflectionManager* reflection_manager, 
		const GPUStats* stats, 
		Statistic<float>* statistics
	)
	{
		return FillStatisticsForReflectionType(reflection_manager, STRING(GPUStats), stats, statistics);
	}

	// ------------------------------------------------------------------------------------------------------------------------

}