#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Utilities/BasicTypes.h"
#include "ButtonInput.h"

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

	struct ECSENGINE_API MouseProcessAttachment {
		HWND hWnd;
	};

	struct ECSENGINE_API MouseProcedureInfo {
		UINT message;
		WPARAM wParam;
		LPARAM lParam;
	};

	struct ECSENGINE_API Mouse : ButtonInput<ECS_MOUSE_BUTTON, ECS_MOUSE_BUTTON_COUNT>
	{
	public:
		Mouse();

		Mouse(const Mouse& other) = default;
		Mouse& operator =(const Mouse& other) = default;

		void AttachToProcess(const MouseProcessAttachment& info);

		void DisableRawInput();

		void EnableRawInput();

		int GetScrollValue() const;

		int2 GetPosition() const;

		int2 GetPreviousPosition() const;

		int2 GetPositionDelta() const;

		int GetPreviousScroll() const;

		int GetScrollDelta() const;

		bool GetRawInputStatus() const;

		void SetCursorVisibility(bool visible);

		void SetPreviousPositionAndScroll();

		void SetPosition(int x, int y);

		void AddDelta(int x, int y);

		void ResetCursorWheel();
		
		void SetCursorWheel(int value);

		void Update();

		void Procedure(const MouseProcedureInfo& info);

		void Reset();

		int2 m_previous_position;
		int m_previous_scroll;
		int2 m_current_position;
		int m_current_scroll;
		bool m_get_raw_input;
	};

}

