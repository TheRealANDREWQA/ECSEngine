#include "ecspch.h"
#include "Mouse.h"

namespace ECSEngine {

	void Mouse::AttachToProcess(const MouseProcessAttachment& info) {
		//m_implementation->SetWindow(info.hWnd);
	}

	void Mouse::DisableRawInput()
	{
		m_get_raw_input = false;
	}

	void Mouse::EnableRawInput()
	{
		m_get_raw_input = true;
	}

	Mouse::Mouse() {
		Reset();
	}

	int Mouse::GetScrollValue() const
	{
		return m_current_scroll;
	}

	int2 Mouse::GetPreviousPosition() const
	{
		return m_previous_position;
	}

	int Mouse::GetPreviousScroll() const
	{
		return m_previous_scroll;
	}

	int2 Mouse::GetPositionDelta() const
	{
		return m_current_position - m_previous_position;
	}
		
	int Mouse::GetScrollDelta() const
	{
		return m_current_scroll - m_previous_scroll;
	}

	bool Mouse::GetRawInputStatus() const
	{
		return m_get_raw_input;
	}

	int2 Mouse::GetPosition() const
	{
		return m_current_position;
	}

	void Mouse::AddDelta(int x, int y)
	{
		m_current_position += int2(x, y);
	}

	void Mouse::ResetCursorWheel() {
		m_current_scroll = 0;
	}

	void Mouse::SetCursorWheel(int value)
	{
		m_current_scroll = value;
	}

	void Mouse::Update()
	{
		Tick();
		SetPreviousPositionAndScroll();
	}

	void Mouse::Reset() {
		memset(m_states, ECS_BUTTON_RAISED, sizeof(m_states));
		m_current_position = { 0, 0 };
		m_current_scroll = 0;
		m_previous_position = { 0, 0 };
		m_previous_scroll = 0;
		m_get_raw_input = false;
	}

	void Mouse::SetCursorVisibility(bool visible) {
		CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
		if (!GetCursorInfo(&info))
		{
			ECS_ASSERT(false);
		}

		bool is_visible = (info.flags & CURSOR_SHOWING) != 0;
		if (is_visible != visible)
		{
			ShowCursor(visible);
		}
	}

	void Mouse::SetPreviousPositionAndScroll()
	{
		m_previous_scroll = m_current_scroll;
		m_previous_position = m_current_position;
	}

	void Mouse::SetPosition(int x, int y)
	{
		m_current_position = { x, y };
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
		}
	}

}