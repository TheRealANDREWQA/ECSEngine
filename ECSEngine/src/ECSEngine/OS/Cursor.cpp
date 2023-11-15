#include "ecspch.h"
#include "Cursor.h"

namespace ECSEngine {

	namespace OS {

		// -----------------------------------------------------------------------------------------------------

		uint2 GetCursorPosition(void* window_handle) {
			POINT point;
			ECS_ASSERT(GetCursorPos(&point));
			// Also convert from screen to client position
			ECS_ASSERT(ScreenToClient((HWND)window_handle, &point));
			return { (unsigned int)point.x, (unsigned int)point.y };
		}

		void SetCursorPosition(void* window_handle, uint2 position)
		{
			// We need to convert from client to screen coordinates
			POINT cursor_point = { position.x, position.y };
			ECS_ASSERT(ClientToScreen((HWND)window_handle, &cursor_point));
			ECS_ASSERT(SetCursorPos(cursor_point.x, cursor_point.y));
		}

		uint2 SetCursorPositionRelative(void* window_handle, uint2 position)
		{
			HWND hwnd = (HWND)window_handle;
			RECT window_rect;
			ECS_ASSERT(GetClientRect(hwnd, &window_rect));

			// If we have a top border we need to take that into account
			RECT bounds;
			GetWindowRect(hwnd, &bounds);

			POINT cursor;
			GetCursorPos(&cursor);

			unsigned int width = window_rect.right - window_rect.left;
			unsigned int height = window_rect.bottom - window_rect.top;

			position.x %= width;
			position.y %= height;

			position.x += window_rect.left;
			position.y += window_rect.top;

			SetCursorPosition(window_handle, { position.x, position.y });
			return position;
		}

		// -----------------------------------------------------------------------------------------------------

		uint2 GetOSWindowPosition(void* window_handle)
		{
			HWND hwnd = (HWND)window_handle;
			RECT window_rect;
			ECS_ASSERT(GetClientRect(hwnd, &window_rect));

			return { (unsigned int)window_rect.left, (unsigned int)window_rect.top };
		}

		// -----------------------------------------------------------------------------------------------------

	}

}