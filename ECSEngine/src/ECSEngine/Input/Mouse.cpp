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
		m_restore_position = { 0, 0 };
	}

	void Mouse::SetCursorVisibility(bool visible, bool restore_position) {
		if (m_is_visible == visible) {
			return;
		}

		ShowCursor(visible);
		m_is_visible = visible;

		if (!visible) {
			m_restore_position = m_current_position;
			// Enable clipping
			OS::ClipCursorToRectangle(OS::GetOSWindowPositionClient(m_window_handle), OS::GetOSWindowSizeClient(m_window_handle));
		}
		else {
			if (restore_position) {
				SetPosition(m_restore_position.x, m_restore_position.y);
			}
			// Disable clipping
			OS::ClipCursorToRectangle({ 0,0 }, { 0, 0 });
		}
	}

	void Mouse::SetPreviousPositionAndScroll()
	{
		m_previous_position = m_current_position;
		m_previous_scroll = m_current_scroll;
		m_has_wrapped = false;
	}

	void Mouse::SetPosition(int x, int y) {
		m_current_position = OS::SetCursorPositionRelative(m_window_handle, { (unsigned int)x, (unsigned int)y });
	}

	static void HandleWrapping(Mouse* mouse) {
		// When it hits the end of the client region it will be 1 less smaller than the value
		// Use ints everywhere because that is the same the native API
		int2 window_size = mouse->m_wrap_pixel_bounds;
		window_size.x--;
		window_size.y--;

		int2 new_position = mouse->m_current_position;;
		int2 current_position = mouse->m_current_position;
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
			new_position = { current_position.x, window_size.y + current_position.y - 1 };
		}

		if (new_position.x != current_position.x || new_position.y != current_position.y) {
			mouse->m_before_wrap_position = current_position;
			mouse->m_has_wrapped = true;
			mouse->SetPosition(new_position.x, new_position.y);
		}
	}

	void Mouse::UpdateFromOther(const Mouse* other_mouse) {
		ButtonInput<ECS_MOUSE_BUTTON, ECS_MOUSE_BUTTON_COUNT>::UpdateFromOther(other_mouse);
		// Update the position and the scroll value as is for the moment
		// And for the previous value, since those are used for computing
		// The delta. Also, the wrapper state needs to be carried across
		m_current_position = other_mouse->m_current_position;
		m_current_scroll = other_mouse->m_current_scroll;
		m_previous_position = other_mouse->m_previous_position;
		m_previous_scroll = other_mouse->m_previous_scroll;
		m_has_wrapped = other_mouse->m_has_wrapped;
		m_before_wrap_position = other_mouse->m_before_wrap_position;
		m_restore_position = other_mouse->m_restore_position;
	}

	void Mouse::Procedure(const MouseProcedureInfo& info) {
		// Raw input handled separately
		if (info.message == WM_INPUT && m_get_raw_input) {
			// We still need to process raw input messages in the message pump because it can happen that
			// After the raw input retrieval new raw input messages are being added. In order to not add filtering to the message pump,
			// Process them here
			RAWINPUT raw_input;
			unsigned int size = sizeof(raw_input);
			if (GetRawInputData((HRAWINPUT)info.lParam, RID_INPUT, &raw_input, &size, sizeof(RAWINPUTHEADER)) != size) {
				return;
			}
			HandleMouseRawInput(this, &raw_input, 1);

		}
		else {
			if (info.message != WM_CHAR || !m_get_raw_input) {
				switch (info.message)
				{
				case WM_MOUSEMOVE:
					break;

				case WM_LBUTTONDOWN:
				{
					UpdateButton(ECS_MOUSE_LEFT, false);
					SetCapture((HWND)m_window_handle);
				}
					break;

				case WM_LBUTTONUP:
				{
					UpdateButton(ECS_MOUSE_LEFT, true);
					ReleaseCapture();
				}
					break;

				case WM_RBUTTONDOWN:
				{
					UpdateButton(ECS_MOUSE_RIGHT, false);
					SetCapture((HWND)m_window_handle);
				}
					break;

				case WM_RBUTTONUP:
				{
					UpdateButton(ECS_MOUSE_RIGHT, true);
					ReleaseCapture();
				}
					break;

				case WM_MBUTTONDOWN:
				{
					UpdateButton(ECS_MOUSE_MIDDLE, false);
					SetCapture((HWND)m_window_handle);
				}
					break;

				case WM_MBUTTONUP:
				{
					UpdateButton(ECS_MOUSE_MIDDLE, true);
					ReleaseCapture();
				}
					break;

				case WM_MOUSEWHEEL:
					m_current_scroll += GET_WHEEL_DELTA_WPARAM(info.wParam);
					return;

				case WM_XBUTTONDOWN:
				{
					switch (GET_XBUTTON_WPARAM(info.wParam))
					{
					case XBUTTON1:
						UpdateButton(ECS_MOUSE_X1, false);
						break;

					case XBUTTON2:
						UpdateButton(ECS_MOUSE_X2, false);
						break;
					}
					SetCapture((HWND)m_window_handle);
				}
					break;
				case WM_XBUTTONUP:
				{
					switch (GET_XBUTTON_WPARAM(info.wParam))
					{
					case XBUTTON1:
						UpdateButton(ECS_MOUSE_X1, true);
						break;

					case XBUTTON2:
						UpdateButton(ECS_MOUSE_X2, true);
						break;
					}
					ReleaseCapture();
				}
					break;

				default:
					// Not a mouse message, so exit
					return;
				}
			}

			// All mouse messages provide a new pointer position
			int x = GET_X_LPARAM(info.lParam);
			int y = GET_Y_LPARAM(info.lParam);

			m_current_position = { x, y };
			if (m_wrap_position) {
				HandleWrapping(this);
			}
		}
	}

	void ProcessMouseRawInput(Mouse* mouse) {
		// Handle the raw input messages - using buffered reads such that we have good performance on high polling rate mices
		const size_t MAX_RAW_INPUTS = 128;
		RAWINPUT raw_inputs[MAX_RAW_INPUTS];
		UINT raw_inputs_size = sizeof(raw_inputs);
		UINT read_size = GetRawInputBuffer(raw_inputs, &raw_inputs_size, sizeof(RAWINPUTHEADER));
		while (read_size == MAX_RAW_INPUTS) {
			HandleMouseRawInput(mouse, raw_inputs, read_size);
			raw_inputs_size = sizeof(raw_inputs);
			read_size = GetRawInputBuffer(raw_inputs, &raw_inputs_size, sizeof(RAWINPUTHEADER));
		}
		HandleMouseRawInput(mouse, raw_inputs, read_size);
	}

	void HandleMouseRawInput(Mouse* mouse, void* raw_input_structures, size_t structure_count) {
		RAWINPUT* raw_inputs = (RAWINPUT*)raw_input_structures;
		for (size_t index = 0; index < structure_count; index++) {
			if (raw_inputs[index].header.dwType == RIM_TYPEMOUSE) {
				// Handle the raw input
				if (raw_inputs[index].data.mouse.lLastX != 0 || raw_inputs[index].data.mouse.lLastY != 0) {
					mouse->AddDelta(raw_inputs[index].data.mouse.lLastX, raw_inputs[index].data.mouse.lLastY);
					if (mouse->m_wrap_position) {
						HandleWrapping(mouse);
					}
				}

				// Handle the button states
				auto handle_flag = [&](int os_flag, ECS_MOUSE_BUTTON button, bool is_released) {
					if (raw_inputs[index].data.mouse.usButtonFlags & os_flag) {
						mouse->UpdateButton(button, is_released);
					}
				};

				handle_flag(RI_MOUSE_LEFT_BUTTON_DOWN, ECS_MOUSE_LEFT, false);
				handle_flag(RI_MOUSE_LEFT_BUTTON_UP, ECS_MOUSE_LEFT, true);
				handle_flag(RI_MOUSE_MIDDLE_BUTTON_DOWN, ECS_MOUSE_MIDDLE, false);
				handle_flag(RI_MOUSE_MIDDLE_BUTTON_UP, ECS_MOUSE_MIDDLE, true);
				handle_flag(RI_MOUSE_RIGHT_BUTTON_DOWN, ECS_MOUSE_RIGHT, false);
				handle_flag(RI_MOUSE_RIGHT_BUTTON_UP, ECS_MOUSE_RIGHT, true);

				// Handle the scroll wheel
				if (raw_inputs[index].data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
					mouse->AddWheelDelta((short)raw_inputs[index].data.mouse.usButtonData);
				}
			}
		}
	}

}