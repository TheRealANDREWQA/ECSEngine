#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../../../Dependencies/DirectXTK/Inc/Mouse.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	enum ECS_MOUSE_BUTTON : unsigned char {
		ECS_MOUSE_LEFT,
		ECS_MOUSE_RIGHT,
		ECS_MOUSE_MIDDLE,
		ECS_MOUSE_X1,
		ECS_MOUSE_X2,
		ECS_MOUSE_BUTTON_COUNT
	};

	template<typename Functor>
	ECS_INLINE void ForEachMouseButton(Functor&& functor) {
		for (size_t index = 0; index < ECS_MOUSE_BUTTON_COUNT; index++) {
			functor((ECS_MOUSE_BUTTON)index);
		}
	}

	namespace HID {

		typedef DirectX::Mouse::ButtonStateTracker::ButtonState MouseButtonState;

		struct ECSENGINE_API MouseState {
			bool Button(ECS_MOUSE_BUTTON button) const;
			
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

			void SetPosition(int2 position);

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

			MouseButtonState Button(ECS_MOUSE_BUTTON button) const;

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

			Mouse(const Mouse& other) = default;
			Mouse& operator =(const Mouse& other) = default;

			void AttachToProcess(const MouseProcessAttachment& info);

			void DisableRawInput();

			void EnableRawInput();

			MouseState* GetState();

			MouseTracker* GetTracker();

			int GetScrollValue() const;

			int2 GetPosition() const;

			int2 GetPreviousPosition() const;

			int2 GetPositionDelta() const;

			int GetPreviousScroll() const;

			int GetScrollDelta() const;

			bool GetRawInputStatus() const;

			bool IsCursorVisible() const;

			void UpdateTracker();

			void UpdateState();

			void SetAbsoluteMode();

			void SetRelativeMode();

			void SetCursorVisible();

			void SetCursorInvisible();

			void SetPreviousPositionAndScroll();

			void SetPosition(int x, int y);

			void AddDelta(int x, int y);

			void ResetCursorWheel();

			void ResetTracker();

			void Procedure(const MouseProcedureInfo& info);

		//private:
			DirectX::Mouse* m_implementation;
			MouseState m_state;
			MouseTracker m_tracker;
			bool m_get_raw_input;
		};

	}

}

