#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../../../Dependencies/DirectXTK/Inc/Mouse.h"

namespace ECSEngine {

	namespace HID {

		using MouseButtonState = DirectX::Mouse::ButtonStateTracker::ButtonState;

		struct ECSENGINE_API MouseState {
			bool LeftButton() const;
			bool RightButton() const;
			bool MiddleButton() const;
			bool XButton1() const;
			bool XButton2() const;
			int PositionX() const;
			int PositionY() const;
			int MouseWheelScroll() const;
			int GetPreviousX() const;
			int GetPreviousY() const;
			int GetDeltaX() const;
			int GetDeltaY() const;
			void SetPreviousPosition();

			DirectX::Mouse::State state;
			int m_previous_x;
			int m_previous_y;
		};

		struct ECSENGINE_API MouseTracker {
			void Reset();
			void Update(const MouseState& state);

			MouseButtonState LeftButton() const;
			MouseButtonState RightButton() const;
			MouseButtonState MiddleButton() const;
			MouseButtonState XButton1() const;
			MouseButtonState XButton2() const;

			DirectX::Mouse::ButtonStateTracker tracker;
		};

		struct ECSENGINE_API MouseProcessAttachment {
			HWND hWnd;
		};

		struct ECSENGINE_API MouseProcedureInfo {
			UINT message;
			WPARAM wParam;
			LPARAM lParam;
		};

		#define MBPRESSED HID::MouseButtonState(DirectX::Mouse::ButtonStateTracker::ButtonState::PRESSED)
		#define MBRELEASED HID::MouseButtonState(DirectX::Mouse::ButtonStateTracker::ButtonState::RELEASED)
		#define MBUP HID::MouseButtonState(DirectX::Mouse::ButtonStateTracker::ButtonState::UP)
		#define MBHELD HID::MouseButtonState(DirectX::Mouse::ButtonStateTracker::ButtonState::HELD)

		class ECSENGINE_API Mouse
		{
		public:
			Mouse();

			void AttachToProcess(const MouseProcessAttachment& info);

			MouseState* GetState();

			MouseTracker* GetTracker();

			int GetScrollValue() const;

			int GetPreviousMouseX() const;

			int GetPreviousMouseY() const;

			int GetDeltaX() const;

			int GetDeltaY() const;

			void GetMousePosition(int& x, int& y) const;

			bool IsCursorVisible() const;

			void UpdateTracker();

			void UpdateState();

			void SetAbsoluteMode();

			void SetRelativeMode();

			void SetCursorVisible();

			void SetCursorInvisible();

			void SetPreviousPosition();

			void ResetCursorWheel();

			void ResetTracker();

			void Procedure(const MouseProcedureInfo& info);

		private:
			std::unique_ptr<DirectX::Mouse> m_implementation;
			MouseState m_state;
			MouseTracker m_tracker;
		};

	}

}

