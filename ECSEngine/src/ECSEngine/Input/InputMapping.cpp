#include "ecspch.h"
#include "InputMapping.h"

namespace ECSEngine {

	static bool IsKeyDisabled(const ECS_KEY* disabled_keys, size_t disabled_key_count, ECS_KEY key) {
		for (size_t index = 0; index < disabled_key_count; index++) {
			if (disabled_keys[index] == key) {
				return true;
			}
		}
		return false;
	}

	static bool IsMouseButtonDisabled(const ECS_MOUSE_BUTTON* disabled_mouse_buttons, size_t disabled_mouse_button_count, ECS_MOUSE_BUTTON button) {
		for (size_t index = 0; index < disabled_mouse_button_count; index++) {
			if (disabled_mouse_buttons[index] == button) {
				return true;
			}
		}
		return false;
	}

	void InputMapping::Initialize(AllocatorPolymorphic allocator, size_t count)
	{
		mappings.Initialize(allocator, count);
		disabled_key_count = 0;
		disabled_mouse_button_count = 0;
	}

	bool InputMapping::IsTriggered(unsigned int index) const
	{
		// For debug purposes, it is left like this
		bool return_value = mappings[index].IsTriggered(mouse, keyboard, disabled_keys, disabled_key_count, disabled_mouse_buttons, disabled_mouse_button_count);
		return return_value;
	}

	void InputMapping::ChangeMapping(unsigned int index, InputMappingElement mapping_element)
	{
		mappings[index] = mapping_element;
	}

	void InputMapping::ChangeMapping(const InputMappingElement* elements)
	{
		for (size_t index = 0; index < mappings.size; index++) {
			ChangeMapping(index, elements[index]);
		}
	}

	bool InputMappingButton::IsTriggered(const Mouse* mouse, const Keyboard* keyboard, const ECS_KEY* disabled_keys, size_t disabled_key_count, const ECS_MOUSE_BUTTON* disabled_mouse_buttons, size_t disabled_mouse_button_count) const {
		if (IsDisabled(disabled_keys, disabled_key_count, disabled_mouse_buttons, disabled_mouse_button_count)) {
			return false;
		}
		return IsTriggered(mouse, keyboard);
	}

	bool InputMappingButton::IsTriggered(const Mouse* mouse, const Keyboard* keyboard) const
	{
		if (is_key) {
			// Return true since this element is not yet initialized
			// Or is unused
			if (key == ECS_KEY_NONE || key == ECS_KEY_COUNT) {
				return !exclude;
			}

			if (state != keyboard->Get(key)) {
				return exclude;
			}
		}
		else {
			// Return true since this element is not yet initialized
			// Or is unused
			if (mouse_button == ECS_MOUSE_BUTTON_COUNT) {
				return !exclude;
			}

			if (state != mouse->Get(mouse_button)) {
				return exclude;
			}
		}

		return !exclude;
	}

	bool InputMappingButton::IsDisabled(const ECS_KEY* disabled_keys, size_t disabled_key_count, const ECS_MOUSE_BUTTON* disabled_mouse_buttons, size_t disabled_mouse_button_count) const {
		if (is_key) {
			return IsKeyDisabled(disabled_keys, disabled_key_count, key);
		}
		else {
			return IsMouseButtonDisabled(disabled_mouse_buttons, disabled_mouse_button_count, mouse_button);
		}
	}

	bool InputMapping::IsKeyDisabled(ECS_KEY key) const {
		return ECSEngine::IsKeyDisabled(disabled_keys, disabled_key_count, key);
	}

	bool InputMapping::IsMouseButtonDisabled(ECS_MOUSE_BUTTON button) const {
		return ECSEngine::IsMouseButtonDisabled(disabled_mouse_buttons, disabled_mouse_button_count, button);
	}

	void InputMapping::DisableKey(ECS_KEY key) {
		ECS_ASSERT(disabled_key_count < ECS_COUNTOF(disabled_keys), "InputMapping: Exceeded the number of disabled keys!");
		disabled_keys[disabled_key_count++] = key;
	}

	void InputMapping::DisableMouseButton(ECS_MOUSE_BUTTON button) {
		ECS_ASSERT(disabled_mouse_button_count < ECS_COUNTOF(disabled_mouse_buttons), "InputMapping: Exceeded the number of disabled mouse buttons!");
		disabled_mouse_buttons[disabled_mouse_button_count++] = button;
	}

	void InputMapping::ReenableKey(ECS_KEY key) {
		for (size_t index = 0; index < disabled_key_count; index++) {
			if (disabled_keys[index] == key) {
				disabled_keys[index] = disabled_keys[--disabled_key_count];
				return;
			}
		}

		ECS_ASSERT(false, "InputMapping: Trying to re-enable a key that was not deactivated!");
	}

	void InputMapping::ReenableMouseButton(ECS_MOUSE_BUTTON mouse_button) {
		for (size_t index = 0; index < disabled_mouse_button_count; index++) {
			if (disabled_mouse_buttons[index] == mouse_button) {
				disabled_mouse_buttons[index] = disabled_mouse_buttons[--disabled_mouse_button_count];
				return;
			}
		}

		ECS_ASSERT(false, "InputMapping: Trying to re-enable a mouse button that was not deactivated!");
	}

}