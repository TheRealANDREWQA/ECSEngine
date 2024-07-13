#pragma once
#include "../Core.h"
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

		ECS_INLINE void AttachToWindow(void* window_handle) {
			m_window_handle = window_handle;
		}

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
			if (!m_has_wrapped) {
				return m_current_position - m_previous_position;
			}
			else {
				return m_before_wrap_position - m_previous_position;
			}
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

		ECS_INLINE bool IsWrap() const {
			return m_wrap_position;
		}

		ECS_INLINE bool IsVisible() const {
			return m_is_visible;
		}

		ECS_INLINE bool HasWrapped() const {
			return m_has_wrapped;
		}

		void SetCursorVisibility(bool visible, bool pin_mouse_on_invisible = true);

		void SetPreviousPositionAndScroll();

		void SetPosition(int x, int y);

		ECS_INLINE void AddDelta(int x, int y) {
			m_current_position += int2(x, y);
		}

		void ActivateWrap(uint2 pixel_bounds);

		ECS_INLINE void DeactivateWrap() {
			m_wrap_position = false;
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

		void UpdateFromOther(const Mouse* other_mouse);

		void Procedure(const MouseProcedureInfo& info);

		void Reset();

		void* m_window_handle;
		int2 m_previous_position;
		int m_previous_scroll;
		int2 m_current_position;
		int m_current_scroll;
		uint2 m_wrap_pixel_bounds;
		// This is set to true when the mouse has wrapped in order to not
		// have the delta explode
		int2 m_before_wrap_position;
		bool m_get_raw_input;
		bool m_is_visible;
		bool m_wrap_position;
		bool m_has_wrapped;
		bool m_pin_mouse;
	};

}

