#pragma once
#include "Mouse.h"
#include "Keyboard.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	struct ECSENGINE_API InputMappingButton {
		ECS_INLINE InputMappingButton() : exclude(false), is_key(false), mouse_button(ECS_MOUSE_BUTTON_COUNT), state(ECS_BUTTON_STATE_COUNT) {}

		// This overload does not take into consideration disabled keys/buttons
		bool IsTriggered(const Mouse* mouse, const Keyboard* keyboard) const;

		// This overload takes into consideration disabled keys/buttons
		bool IsTriggered(const Mouse* mouse, const Keyboard* keyboard, const ECS_KEY* disabled_keys, size_t disabled_key_count, const ECS_MOUSE_BUTTON* disabled_mouse_buttons, size_t disabled_mouse_button_count) const;

		bool IsDisabled(const ECS_KEY* disabled_keys, size_t disabled_key_count, const ECS_MOUSE_BUTTON* disabled_mouse_buttons, size_t disabled_mouse_button_count) const;

		ECS_INLINE bool IsInitialized() const {
			if (is_key) {
				return key != ECS_KEY_NONE;
			}
			else {
				return mouse_button != ECS_MOUSE_BUTTON_COUNT;
			}
		}

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
		ECS_INLINE InputMappingElement() : first(InputMappingButton{}), second(InputMappingButton{}), third(InputMappingButton{}) {}

		ECS_INLINE bool IsInitialized() const {
			return first.IsInitialized();
		}

		ECS_INLINE bool IsTriggered(const Mouse* mouse, const Keyboard* keyboard) const {
			return first.IsTriggered(mouse, keyboard) && second.IsTriggered(mouse, keyboard) && third.IsTriggered(mouse, keyboard);
		}

		ECS_INLINE bool IsTriggered(const Mouse* mouse, const Keyboard* keyboard, const ECS_KEY* disabled_keys, size_t disabled_key_count, const ECS_MOUSE_BUTTON* disabled_mouse_buttons, size_t disabled_mouse_button_count) const {
			return first.IsTriggered(mouse, keyboard, disabled_keys, disabled_key_count, disabled_mouse_buttons, disabled_mouse_button_count)
				&& second.IsTriggered(mouse, keyboard, disabled_keys, disabled_key_count, disabled_mouse_buttons, disabled_mouse_button_count)
				&& third.IsTriggered(mouse, keyboard, disabled_keys, disabled_key_count, disabled_mouse_buttons, disabled_mouse_button_count);
		}

		ECS_INLINE void SetFirstKey(ECS_KEY key, ECS_BUTTON_STATE state, bool exclude = false) {
			first.SetKey(key, state, exclude);
		}

		ECS_INLINE InputMappingElement& SetFirstKeyPressed(ECS_KEY key) {
			SetFirstKey(key, ECS_BUTTON_PRESSED);
			return *this;
		}

		ECS_INLINE InputMappingElement& SetSecondKey(ECS_KEY key, ECS_BUTTON_STATE state, bool exclude = false) {
			second.SetKey(key, state, exclude);
			return *this;
		}

		ECS_INLINE InputMappingElement& SetThirdKey(ECS_KEY key, ECS_BUTTON_STATE state, bool exclude = false) {
			third.SetKey(key, state, exclude);
			return *this;
		}

		ECS_INLINE InputMappingElement& SetFirstMouse(ECS_MOUSE_BUTTON button, ECS_BUTTON_STATE state, bool exclude = false) {
			first.SetMouse(button, state, exclude);
			return *this;
		}

		ECS_INLINE InputMappingElement& SetFirstMousePressed(ECS_MOUSE_BUTTON button) {
			SetFirstMouse(button, ECS_BUTTON_PRESSED);
			return *this;
		}

		ECS_INLINE InputMappingElement& SetSecondMouse(ECS_MOUSE_BUTTON button, ECS_BUTTON_STATE state, bool exclude = false) {
			second.SetMouse(button, state, exclude);
			return *this;
		}

		ECS_INLINE InputMappingElement& SetThirdMouse(ECS_MOUSE_BUTTON button, ECS_BUTTON_STATE state, bool exclude = false) {
			third.SetMouse(button, state, exclude);
			return *this;
		}

		ECS_INLINE InputMappingElement& SetExcludeCtrl() {
			if (second.state == ECS_BUTTON_STATE_COUNT) {
				SetSecondKey(ECS_KEY_LEFT_CTRL, ECS_BUTTON_HELD, true);
			}
			else {
				SetThirdKey(ECS_KEY_LEFT_CTRL, ECS_BUTTON_HELD, true);
			}
			return *this;
		}

		ECS_INLINE InputMappingElement& SetExcludeShift() {
			if (second.state == ECS_BUTTON_STATE_COUNT) {
				SetSecondKey(ECS_KEY_LEFT_SHIFT, ECS_BUTTON_HELD, true);
			}
			else {
				SetThirdKey(ECS_KEY_LEFT_SHIFT, ECS_BUTTON_HELD, true);
			}
			return *this;
		}

		ECS_INLINE InputMappingElement& SetExcludeAlt() {
			if (second.state == ECS_BUTTON_STATE_COUNT) {
				SetSecondKey(ECS_KEY_LEFT_ALT, ECS_BUTTON_HELD, true);
			}
			else {
				SetThirdKey(ECS_KEY_LEFT_SHIFT, ECS_BUTTON_HELD, true);
			}
			return *this;
		}

		ECS_INLINE InputMappingElement& SetCtrlWith(ECS_KEY key, ECS_BUTTON_STATE state) {
			SetFirstKey(ECS_KEY_LEFT_CTRL, ECS_BUTTON_HELD);
			SetSecondKey(key, state);
			return *this;
		}

		ECS_INLINE InputMappingElement& SetShiftWith(ECS_KEY key, ECS_BUTTON_STATE state) {
			SetFirstKey(ECS_KEY_LEFT_SHIFT, ECS_BUTTON_HELD);
			SetSecondKey(key, state);
			return *this;
		}

		ECS_INLINE InputMappingElement& SetAltWith(ECS_KEY key, ECS_BUTTON_STATE state) {
			SetFirstKey(ECS_KEY_LEFT_ALT, ECS_BUTTON_HELD);
			SetSecondKey(key, state);
			return *this;
		}

		InputMappingButton first;
		InputMappingButton second;
		InputMappingButton third;
	};

	struct ECSENGINE_API InputMapping {
		void Initialize(AllocatorPolymorphic allocator, size_t count);

		bool IsTriggered(unsigned int index) const;

		// Returns true if the given key is disabled, else false
		bool IsKeyDisabled(ECS_KEY key) const;

		// Returns true if the given mouse button is disabled, else false
		bool IsMouseButtonDisabled(ECS_MOUSE_BUTTON button) const;

		void ChangeMapping(unsigned int index, InputMappingElement mapping_element);

		// Replaces all elements with the ones given
		void ChangeMapping(const InputMappingElement* elements);

		// Disables a key. Can be called multiple times on the same key, respecting the number
		// Of disable, requiring the same number of calls to re-enable key to make it active again
		void DisableKey(ECS_KEY key);

		// Disables a mouse button. Can be called multiple times on the same button, respecting the number
		// Of disable, requiring the same number of calls to re-enable mouse button to make it active again
		void DisableMouseButton(ECS_MOUSE_BUTTON button);

		// Reenables a key. If a key has been disabled multiple times, this "counter-acts" only a single disable call.
		// You need to call this function the same number of times as the disable calls to make the key active again
		void ReenableKey(ECS_KEY key);

		// Reenables a mouse button. If a button has been disabled multiple times, this "counter-acts" only a single disable call.
		// You need to call this function the same number of times as the disable calls to make the button active again
		void ReenableMouseButton(ECS_MOUSE_BUTTON mouse_button);

		Stream<InputMappingElement> mappings;
		
		struct {
			// This is a limited amount of buttons that can be used to disable certain buttons from being counted
			// As being pressed/held.
			ECS_KEY disabled_keys[15];
			unsigned char disabled_key_count;
			ECS_MOUSE_BUTTON disabled_mouse_buttons[7];
			unsigned char disabled_mouse_button_count;
		};

		const Mouse* mouse;
		const Keyboard* keyboard;
	};

}