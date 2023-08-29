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

	struct ECSENGINE_API MouseProcedureInfo {
		UINT message;
		WPARAM wParam;
		LPARAM lParam;
	};

	struct ECSENGINE_API Mouse : ButtonInput<ECS_MOUSE_BUTTON, ECS_MOUSE_BUTTON_COUNT>
	{
		Mouse();

		Mouse(const Mouse& other) = default;
		Mouse& operator =(const Mouse& other) = default;

		ECS_INLINE void DisableRawInput() {
			m_get_raw_input = false;
		}

		ECS_INLINE void EnableRawInput() {
			m_get_raw_input = true;
		}

		ECS_INLINE int GetScrollValue() const {
			return m_current_scroll;
		}

		ECS_INLINE int2 GetPosition() const {
			return m_current_position;
		}

		ECS_INLINE int2 GetPreviousPosition() const {
			return m_previous_position;
		}

		ECS_INLINE int2 GetPositionDelta() const {
			return m_current_position - m_previous_position;
		}

		ECS_INLINE int GetPreviousScroll() const {
			return m_previous_scroll;
		}

		ECS_INLINE int GetScrollDelta() const {
			return m_current_scroll - m_previous_scroll;
		}

		ECS_INLINE bool GetRawInputStatus() const {
			return m_get_raw_input;
		}

		void SetCursorVisibility(bool visible);

		void SetPreviousPositionAndScroll();

		ECS_INLINE void SetPosition(int x, int y) {
			m_current_position = { x, y };
		}

		ECS_INLINE void AddDelta(int x, int y) {
			m_current_position += int2(x, y);
		}

		ECS_INLINE void ResetCursorWheel() {
			m_current_scroll = 0;
		}
		
		ECS_INLINE void SetCursorWheel(int value) {
			m_current_scroll = value;
		}

		ECS_INLINE void Update() {
			Tick();
			SetPreviousPositionAndScroll();
		}

		void Procedure(const MouseProcedureInfo& info);

		void Reset();

		int2 m_previous_position;
		int m_previous_scroll;
		int2 m_current_position;
		int m_current_scroll;
		bool m_get_raw_input;
	};

}

