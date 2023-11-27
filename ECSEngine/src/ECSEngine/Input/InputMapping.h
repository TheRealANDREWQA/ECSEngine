#pragma once
#include "Mouse.h"
#include "Keyboard.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	struct ECSENGINE_API InputMappingButton {
		ECS_INLINE InputMappingButton() : exclude(false), is_key(false), key(ECS_KEY_NONE), state(ECS_BUTTON_STATE_COUNT) {}

		bool IsTriggered(const Mouse* mouse, const Keyboard* keyboard) const;

		ECS_INLINE void SetKey(ECS_KEY set_key, ECS_BUTTON_STATE set_state, bool set_exclude = false) {
			is_key = true;
			key = set_key;
			state = set_state;
			exclude = set_exclude;
		}

		ECS_INLINE void SetMouse(ECS_MOUSE_BUTTON button, ECS_BUTTON_STATE set_state, bool set_exclude = false) {
			is_key = false;
			mouse_button = button;
			state = set_state;
			exclude = set_exclude;
		}

		// If the exclude flag is set to true, then it won't trigger
		// If the key is in that state
		bool exclude;
		bool is_key;
		union {
			ECS_MOUSE_BUTTON mouse_button;
			ECS_KEY key;
		};
		ECS_BUTTON_STATE state;
	};

	// To initialize, you can memset to 0
	struct InputMappingElement {
		ECS_INLINE void SetFirstKey(ECS_KEY key, ECS_BUTTON_STATE state, bool exclude = false) {
			first.SetKey(key, state, exclude);
		}

		ECS_INLINE void SetFirstKeyPressed(ECS_KEY key) {
			SetFirstKey(key, ECS_BUTTON_PRESSED);
		}

		ECS_INLINE void SetSecondKey(ECS_KEY key, ECS_BUTTON_STATE state, bool exclude = false) {
			second.SetKey(key, state, exclude);
		}

		ECS_INLINE void SetThirdKey(ECS_KEY key, ECS_BUTTON_STATE state, bool exclude = false) {
			third.SetKey(key, state, exclude);
		}

		ECS_INLINE void SetFirstMouse(ECS_MOUSE_BUTTON button, ECS_BUTTON_STATE state, bool exclude = false) {
			first.SetMouse(button, state, exclude);
		}

		ECS_INLINE void SetFirstMousePressed(ECS_MOUSE_BUTTON button) {
			SetFirstMouse(button, ECS_BUTTON_PRESSED);
		}

		ECS_INLINE void SetSecondMouse(ECS_MOUSE_BUTTON button, ECS_BUTTON_STATE state, bool exclude = false) {
			second.SetMouse(button, state, exclude);
		}

		ECS_INLINE void SetThirdMouse(ECS_MOUSE_BUTTON button, ECS_BUTTON_STATE state, bool exclude = false) {
			third.SetMouse(button, state, exclude);
		}

		ECS_INLINE void SetExcludeCtrl() {
			if (second.state == ECS_BUTTON_STATE_COUNT) {
				SetSecondKey(ECS_KEY_LEFT_CTRL, ECS_BUTTON_HELD, true);
			}
			else {
				SetThirdKey(ECS_KEY_LEFT_CTRL, ECS_BUTTON_HELD, true);
			}
		}

		ECS_INLINE void SetExcludeShift() {
			if (second.state == ECS_BUTTON_STATE_COUNT) {
				SetSecondKey(ECS_KEY_LEFT_SHIFT, ECS_BUTTON_HELD, true);
			}
			else {
				SetThirdKey(ECS_KEY_LEFT_SHIFT, ECS_BUTTON_HELD, true);
			}
		}

		ECS_INLINE void SetCtrlWith(ECS_KEY key, ECS_BUTTON_STATE state) {
			SetFirstKey(ECS_KEY_LEFT_CTRL, ECS_BUTTON_HELD);
			SetSecondKey(key, state);
		}

		ECS_INLINE void SetShiftWith(ECS_KEY key, ECS_BUTTON_STATE state) {
			SetFirstKey(ECS_KEY_LEFT_SHIFT, ECS_BUTTON_HELD);
			SetSecondKey(key, state);
		}

		InputMappingButton first;
		InputMappingButton second;
		InputMappingButton third;
	};

	struct ECSENGINE_API InputMapping {
		void Initialize(AllocatorPolymorphic allocator, size_t count);

		bool IsTriggered(unsigned int index) const;

		void ChangeMapping(unsigned int index, InputMappingElement mapping_element);

		// Replaces all elements with the ones given
		void ChangeMapping(const InputMappingElement* elements);

		Stream<InputMappingElement> mappings;
		const Mouse* mouse;
		const Keyboard* keyboard;
	};

}