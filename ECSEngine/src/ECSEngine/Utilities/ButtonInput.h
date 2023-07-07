#pragma once
#include <string.h>

namespace ECSEngine {

	enum ECS_BUTTON_STATE : unsigned char {
		ECS_BUTTON_UP,
		ECS_BUTTON_DOWN,
		ECS_BUTTON_PRESSED,
		ECS_BUTTON_RELEASED,
		ECS_BUTTON_STATE_COUNT
	};

	template<typename ButtonType, size_t max_count>
	struct ButtonInput {
		// Returns true if the current button is down else false
		ECS_INLINE bool IsDown(ButtonType button) const {
			ECS_BUTTON_STATE state = Get(button);
			return state == ECS_BUTTON_DOWN || state == ECS_BUTTON_PRESSED;
		}

		// Returns true if the current button is up else false
		ECS_INLINE bool IsUp(ButtonType button) const {
			ECS_BUTTON_STATE state = Get(button);
			return state == ECS_BUTTON_UP || state == ECS_BUTTON_RELEASED;
		}

		ECS_INLINE bool IsPressed(ButtonType button) const {
			return Get(button) == ECS_BUTTON_PRESSED;
		}

		ECS_INLINE bool IsReleased(ButtonType button) const {
			return Get(button) == ECS_BUTTON_RELEASED;
		}

		// Returns the current state of the button taking into consideration previous ticks
		ECS_INLINE ECS_BUTTON_STATE Get(ButtonType button) const {
			return m_states[(int)button];
		}

		ECS_INLINE void Reset() {
			// Can use memset
			memset(m_states, ECS_BUTTON_UP, sizeof(m_states));
		}

		// Updates the state of a button
		ECS_INLINE void UpdateButton(ButtonType button, bool is_released) {
			ECS_BUTTON_STATE current_state = Get(button);
			if (is_released) {
				m_states[(unsigned int)button] = current_state != ECS_BUTTON_RELEASED ? ECS_BUTTON_RELEASED : ECS_BUTTON_UP;
			}
			else {
				m_states[(unsigned int)button] = current_state != ECS_BUTTON_PRESSED ? ECS_BUTTON_PRESSED : ECS_BUTTON_DOWN;
			}
		}

		ECS_INLINE void SetButton(ButtonType button, ECS_BUTTON_STATE state) {
			m_states[(unsigned int)button] = state;
		}

		// Updates the state of the buttons 
		void Tick() {
			// For all buttons that are pressed/released change them into up or down
			for (size_t index = 0; index < max_count; index++) {
				if (m_states[index] == ECS_BUTTON_PRESSED) {
					m_states[index] = ECS_BUTTON_DOWN;
				}

				if (m_states[index] == ECS_BUTTON_RELEASED) {
					m_states[index] = ECS_BUTTON_UP;
				}
			}
		}

		ECS_BUTTON_STATE m_states[max_count] = { ECS_BUTTON_UP };
	};

}