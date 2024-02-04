#include "ecspch.h"
#include "Mouse.h"
#include "../OS/Cursor.h"
#include "../Utilities/Assert.h"

namespace ECSEngine {

	Mouse::Mouse() {
		Reset();
	}

	void Mouse::ActivateWrap(uint2 pixel_bounds) {
		m_wrap_pixel_bounds = pixel_bounds;
		m_wrap_position = true;
	}

	void Mouse::Reset() {
		memset(m_states, ECS_BUTTON_RAISED, sizeof(m_states));
		m_current_position = { 0, 0 };
		m_current_scroll = 0;
		m_previous_position = { 0, 0 };
		m_previous_scroll = 0;
		m_get_raw_input = false;
		m_is_visible = true;
		m_wrap_position = false;
		m_has_wrapped = false;
		m_window_handle = nullptr;
		m_pin_mouse = false;
	}

	void Mouse::SetCursorVisibility(bool visible, bool pin_mouse_on_invisibility) {
		CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
		if (!GetCursorInfo(&info))
		{
			ECS_ASSERT(false, "Could not change cursor visibility");
		}

		bool is_visible = (info.flags & CURSOR_SHOWING) != 0;
		if (is_visible != visible)
		{
			ShowCursor(visible);
			m_is_visible = visible;
		}

		if (visible) {
			if (pin_mouse_on_invisibility) {
				m_pin_mouse = false;
			}
		}
		else {
			if (pin_mouse_on_invisibility) {
				m_pin_mouse = true;
			}
		}
	}

	void Mouse::SetPreviousPositionAndScroll()
	{
		if (!m_pin_mouse) {
			m_previous_position = m_current_position;
		}
		else {
			SetPosition(m_previous_position.x, m_previous_position.y);
		}
		m_previous_scroll = m_current_scroll;
		m_has_wrapped = false;
	}

	void Mouse::SetPosition(int x, int y) {
		m_current_position = OS::SetCursorPositionRelative(m_window_handle, { (unsigned int)x, (unsigned int)y });
	}

	static void HandleWrapping(Mouse* mouse) {
		// When it hits the end of the client region it will be 1 less smaller than the value
		uint2 window_size = mouse->m_wrap_pixel_bounds;
		window_size.x--;
		window_size.y--;

		uint2 new_position = mouse->m_current_position;;
		uint2 current_position = mouse->m_current_position;
		// Take into account that the raw input might offset these over the boundaries
		if (current_position.x >= window_size.x) {
			new_position = { current_position.x - window_size.x + 1, current_position.y };
		}
		else if (current_position.x <= 0) {
			new_position = { window_size.x + current_position.x - 1, current_position.y };
		}

		if (current_position.y >= window_size.y) {
			new_position = { current_position.x, current_position.y - window_size.y + 1 };
		}
		else if (current_position.y <= 0) {
			new_position = { current_position.x, window_size.y + current_position.x - 1 };
		}

		if (new_position.x != current_position.x || new_position.y != current_position.y) {
			mouse->m_before_wrap_position = current_position;
			mouse->m_has_wrapped = true;
			mouse->SetPosition(new_position.x, new_position.y);
		}
	}

	void Mouse::Procedure(const MouseProcedureInfo& info) {
		// Raw input handled separately
		if (info.message == WM_INPUT && m_get_raw_input) {
			RAWINPUT* data = (RAWINPUT*)ECS_STACK_ALLOC(sizeof(RAWINPUT));
			unsigned int size = 0;
			if (GetRawInputData((HRAWINPUT)info.lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == -1) {
				return;
			}
			// The query failed
			if (GetRawInputData((HRAWINPUT)info.lParam, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER)) != size) {
				return;
			}
			if (data->header.dwType == RIM_TYPEMOUSE && (data->data.mouse.lLastX != 0 || data->data.mouse.lLastY != 0)) {
				AddDelta(data->data.mouse.lLastX, data->data.mouse.lLastY);
				if (m_wrap_position) {
					HandleWrapping(this);
				}
			}
		}
		else {
			if (info.message != WM_CHAR || !m_get_raw_input) {
				switch (info.message)
				{
				case WM_MOUSEMOVE:
					break;

				case WM_LBUTTONDOWN:
					UpdateButton(ECS_MOUSE_LEFT, false);
					break;

				case WM_LBUTTONUP:
					UpdateButton(ECS_MOUSE_LEFT, true);
					break;

				case WM_RBUTTONDOWN:
					UpdateButton(ECS_MOUSE_RIGHT, false);
					break;

				case WM_RBUTTONUP:
					UpdateButton(ECS_MOUSE_RIGHT, true);
					break;

				case WM_MBUTTONDOWN:
					UpdateButton(ECS_MOUSE_MIDDLE, false);
					break;

				case WM_MBUTTONUP:
					UpdateButton(ECS_MOUSE_MIDDLE, true);
					break;

				case WM_MOUSEWHEEL:
					m_current_scroll += GET_WHEEL_DELTA_WPARAM(info.wParam);
					return;

				case WM_XBUTTONDOWN:
					switch (GET_XBUTTON_WPARAM(info.wParam))
					{
					case XBUTTON1:
						UpdateButton(ECS_MOUSE_X1, false);
						break;

					case XBUTTON2:
						UpdateButton(ECS_MOUSE_X2, false);
						break;
					}
					break;
				case WM_XBUTTONUP:
					switch (GET_XBUTTON_WPARAM(info.wParam))
					{
					case XBUTTON1:
						UpdateButton(ECS_MOUSE_X1, true);
						break;

					case XBUTTON2:
						UpdateButton(ECS_MOUSE_X2, true);
						break;
					}
					break;

				default:
					// Not a mouse message, so exit
					return;
				}
			}

			// All mouse messages provide a new pointer position
			int x = LOWORD(info.lParam);
			int y = HIWORD(info.lParam);

			m_current_position = { x, y };
			if (m_wrap_position) {
				HandleWrapping(this);
			}
		}
	}

}