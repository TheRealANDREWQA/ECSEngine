#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../../../Dependencies/DirectXTK/Inc/Mouse.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	namespace HID {

		using MouseButtonState = DirectX::Mouse::ButtonStateTracker::ButtonState;

		struct ECSENGINE_API MouseState {
			bool LeftButton() const;

			bool RightButton() const;

			bool MiddleButton() const;

			bool XButton1() const;

			bool XButton2() const;

			int2 Position() const;

			int MouseWheelScroll() const;

			int2 PreviousPosition() const;

			int PreviousScroll() const;

			int2 PositionDelta()const;

			int ScrollDelta() const;

			void SetPreviousPosition();

			void SetPreviousScroll();

			DirectX::Mouse::State state;
			int2 previous_position;
			int previous_scroll;
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

			int2 GetPosition() const;

			int2 GetPreviousPosition() const;

			int2 GetPositionDelta() const;

			int GetPreviousScroll() const;

			int GetScrollDelta() const;

			bool IsCursorVisible() const;

			void UpdateTracker();

			void UpdateState();

			void SetAbsoluteMode();

			void SetRelativeMode();

			void SetCursorVisible();

			void SetCursorInvisible();

			void SetPreviousPositionAndScroll();

			void ResetCursorWheel();

			void ResetTracker();

			void Procedure(const MouseProcedureInfo& info);

		//private:
			std::unique_ptr<DirectX::Mouse> m_implementation;
			MouseState m_state;
			MouseTracker m_tracker;
		};

	}

}

