#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	namespace OS {

		struct MonitorInfo {
			int2 origin;
			int2 size;
			// These 2 values are for the work area - the area that excludes 
			// System UI, like the Windows taskbar
			int2 work_origin;
			int2 work_size;
			bool primary_display;
		};

		// Must return true to continue, else false
		// The origin and size parameters are in virtual monitor space,
		// The space that encompasses all monitors
		typedef bool (*EnumerateMonitorCallback)(const MonitorInfo* monitor_info, void* monitor_handle, void* user_data);

		ECSENGINE_API bool EnumerateMonitors(EnumerateMonitorCallback callback, void* user_data);

		// Returns the monitor handle of the monitor where the cursor is located
		ECSENGINE_API void* GetMonitorFromCursor();

		// Returns a monitor info structure for the given monitor handle
		// If it fails, the size field will be { 0, 0 } (can use the function
		// RetrieveMonitorInfoSuccess)
		ECSENGINE_API MonitorInfo RetrieveMonitorInfo(void* monitor_handle);
	
		ECS_INLINE bool RetrieveMonitorInfoSuccess(const MonitorInfo* monitor_info) {
			return monitor_info->size != uint2(0, 0);
		}

	}

}