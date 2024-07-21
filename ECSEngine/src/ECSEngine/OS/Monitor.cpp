#include "ecspch.h"
#include "Monitor.h"
#include "../Utilities/Assert.h"
#include "Cursor.h"

namespace ECSEngine {

	namespace OS {

		struct CallbackData {
			EnumerateMonitorCallback callback;
			void* user_data;
			bool stop;
		};

		static BOOL EnumerateDisplayImpl(HMONITOR monitor, HDC unused, LPRECT transform, LPARAM callback_data) {
			CallbackData* data = (CallbackData*)callback_data;
			if (!data->stop) {
				MONITORINFO monitor_info;
				monitor_info.cbSize = sizeof(monitor_info);
				if (GetMonitorInfo(monitor, &monitor_info)) {
					MonitorInfo ecs_monitor_info = { { (unsigned int)transform->left, (unsigned int)transform->top }, { (unsigned int)(transform->right - transform->left), (unsigned int)(transform->bottom - transform->top) }, monitor_info.dwFlags & MONITORINFOF_PRIMARY };
					data->stop = !data->callback(&ecs_monitor_info, monitor, data->user_data);
				}
			}
			return true;
		}

		bool EnumerateMonitors(EnumerateMonitorCallback callback, void* user_data)
		{
			CallbackData callback_data = { callback, user_data, false };
			return EnumDisplayMonitors(NULL, NULL, EnumerateDisplayImpl, (LPARAM)&callback_data);
		}

		struct EnumerateMonitorFromCursorData {
			int2 cursor_position;
			void* monitor_handle;
		};

		static bool EnumerateMonitorFromCursor(const MonitorInfo* monitor_info, void* monitor_handle, void* user_data) {
			EnumerateMonitorFromCursorData* data = (EnumerateMonitorFromCursorData*)user_data;
			// Can use unsigned coordinates since they won't affect the rectangle test
			uint2 cursor_pos = data->cursor_position;
			if (cursor_pos.x >= monitor_info->origin.x && cursor_pos.x <= monitor_info->origin.x + monitor_info->size.x) {
				if (cursor_pos.y >= monitor_info->origin.y && cursor_pos.y <= monitor_info->origin.y + monitor_info->size.y) {
					data->monitor_handle = monitor_handle;
					return false;
				}
			}
			return true;
		}

		void* GetMonitorFromCursor()
		{
			int2 cursor_position = GetCursorPosition();
			// Enumerate all the positions
			EnumerateMonitorFromCursorData enumerate_data = { cursor_position, nullptr };
			bool success = EnumerateMonitors(EnumerateMonitorFromCursor, &enumerate_data);
			GetLastError();
			ECS_ASSERT(success);
			return enumerate_data.monitor_handle;
		}

		MonitorInfo RetrieveMonitorInfo(void* monitor_handle) {
			MONITORINFO monitor_info;
			monitor_info.cbSize = sizeof(monitor_info);
			if (GetMonitorInfo((HMONITOR)monitor_handle, &monitor_info)) {
				return {
					{ (unsigned int)monitor_info.rcMonitor.left, (unsigned int)(monitor_info.rcMonitor.top) },
					{ (unsigned int)(monitor_info.rcMonitor.right - monitor_info.rcMonitor.left), (unsigned int)(monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top) },
					(monitor_info.dwFlags & MONITORINFOF_PRIMARY) != 0
				};
			}
			return { uint2(-1, -1), uint2(0, 0), false };
		}

	}

}