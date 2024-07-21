#pragma once
#include <string.h>

namespace ECSEngine {

	enum ECS_BUTTON_STATE : unsigned char {
		// Last and current frame was not pressed
		ECS_BUTTON_RAISED,
		// Last and current frame was pressed
		ECS_BUTTON_HELD,
		// Last frame was not, current frame is pressed
		ECS_BUTTON_PRESSED,
		// Last frame was, current frame is not pressed
		ECS_BUTTON_RELEASED,
		ECS_BUTTON_STATE_COUNT
	};

	template<typename ButtonType, size_t max_count>
	struct ButtonInput {
		// Returns true if the current button is held or pressed else false
		ECS_INLINE bool IsDown(ButtonType button) const {
			ECS_BUTTON_STATE state = Get(button);
			return state == ECS_BUTTON_HELD || state == ECS_BUTTON_PRESSED;
		}

		// Returns true if the current button is up else false
		ECS_INLINE bool IsUp(ButtonType button) const {
			ECS_BUTTON_STATE state = Get(button);
			return state == ECS_BUTTON_RAISED || state == ECS_BUTTON_RELEASED;
		}

		ECS_INLINE bool IsHeld(ButtonType button) const {
			return Get(button) == ECS_BUTTON_HELD;
		}

		ECS_INLINE bool IsRaised(ButtonType button) const {
			return Get(button) == ECS_BUTTON_RAISED;
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
			memset(m_states, ECS_BUTTON_RAISED, sizeof(m_states));
		}

		// Updates the state of a button
		ECS_INLINE void UpdateButton(ButtonType button, bool is_released) {
			ECS_BUTTON_STATE current_state = Get(button);
			if (is_released) {
				m_states[(unsigned int)button] = current_state != ECS_BUTTON_RELEASED ? ECS_BUTTON_RELEASED : ECS_BUTTON_RAISED;
			}
			else {
				// This was added as a special case for debugging
				// When hitting a breakpoint during a hold action,
				// The release event is not registered since the process
				// Is being debugged. This results in very annoying situations
				// Where the application is unusable. This is meant to fix it
				if (current_state == ECS_BUTTON_HELD || current_state == ECS_BUTTON_PRESSED) {
					m_states[(unsigned int)button] = ECS_BUTTON_RELEASED;
				}
				else {
					m_states[(unsigned int)button] = current_state != ECS_BUTTON_PRESSED ? ECS_BUTTON_PRESSED : ECS_BUTTON_HELD;
				}
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
					m_states[index] = ECS_BUTTON_HELD;
				}

				if (m_states[index] == ECS_BUTTON_RELEASED) {
					m_states[index] = ECS_BUTTON_RAISED;
				}
			}
		}

		// It will take over all updates from the other input into this one
		// This function does not tick
		void UpdateFromOther(const ButtonInput<ButtonType, max_count>* other_input) {
			memcpy(m_states, other_input->m_states, sizeof(ECS_BUTTON_STATE) * max_count);
		}

		// It will update all buttons that are pressed to be released
		// This function does not tick
		void UpdateRelease() {
			for (size_t index = 0; index < max_count; index++) {
				if (m_states[index] == ECS_BUTTON_HELD || m_states[index] == ECS_BUTTON_PRESSED) {
					UpdateButton((ButtonType)index, true);
				}
			}
		}

		ECS_BUTTON_STATE m_states[max_count] = { ECS_BUTTON_RAISED };
	};

}