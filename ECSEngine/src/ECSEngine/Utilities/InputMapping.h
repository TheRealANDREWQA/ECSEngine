#pragma once
#include "Mouse.h"
#include "Keyboard.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	struct ECSENGINE_API InputMappingButton {
		ECS_INLINE InputMappingButton() : is_key(false), key(ECS_KEY_NONE) {}

		bool IsTriggered(const Mouse* mouse, const Keyboard* keyboard) const;

		ECS_INLINE void SetKey(ECS_KEY set_key, ECS_BUTTON_STATE set_state) {
			is_key = true;
			key = set_key;
			state = set_state;
		}

		ECS_INLINE void SetMouse(ECS_MOUSE_BUTTON button, ECS_BUTTON_STATE set_state) {
			is_key = false;
			mouse_button = button;
			state = set_state;
		}

		bool is_key;
		union {
			ECS_MOUSE_BUTTON mouse_button;
			ECS_KEY key;
		};
		ECS_BUTTON_STATE state;
	};

	// To initialize, you can memset to 0
	struct InputMappingElement {
		ECS_INLINE void SetFirstKey(ECS_KEY key, ECS_BUTTON_STATE state) {
			first.SetKey(key, state);
		}

		ECS_INLINE void SetFirstKeyPressed(ECS_KEY key) {
			SetFirstKey(key, ECS_BUTTON_PRESSED);
		}

		ECS_INLINE void SetSecondKey(ECS_KEY key, ECS_BUTTON_STATE state) {
			second.SetKey(key, state);
		}

		ECS_INLINE void SetThirdKey(ECS_KEY key, ECS_BUTTON_STATE state) {
			third.SetKey(key, state);
		}

		ECS_INLINE void SetFirstMouse(ECS_MOUSE_BUTTON button, ECS_BUTTON_STATE state) {
			first.SetMouse(button, state);
		}

		ECS_INLINE void SetFirstMousePressed(ECS_MOUSE_BUTTON button) {
			SetFirstMouse(button, ECS_BUTTON_PRESSED);
		}

		ECS_INLINE void SetSecondMouse(ECS_MOUSE_BUTTON button, ECS_BUTTON_STATE state) {
			second.SetMouse(button, state);
		}

		ECS_INLINE void SetThirdMouse(ECS_MOUSE_BUTTON button, ECS_BUTTON_STATE state) {
			third.SetMouse(button, state);
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