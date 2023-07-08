#include "ecspch.h"
#include "InputMapping.h"

namespace ECSEngine {

	void InputMapping::Initialize(AllocatorPolymorphic allocator, size_t count)
	{
		mappings.Initialize(allocator, count);
	}

	bool InputMapping::IsTriggered(unsigned int index, const Mouse* mouse, const Keyboard* keyboard) const
	{
		InputMappingElement element = mappings[index];
		return element.first.IsTriggered(mouse, keyboard) && element.second.IsTriggered(mouse, keyboard) && element.third.IsTriggered(mouse, keyboard);
	}

	void InputMapping::ChangeMapping(unsigned int index, InputMappingElement mapping_element)
	{
		mappings[index] = mapping_element;
	}

	bool InputMappingButton::IsTriggered(const Mouse* mouse, const Keyboard* keyboard) const
	{
		if (is_key) {
			// Return true since this element is not yet initialized
			// Or is unused
			if (key == ECS_KEY_NONE || key == ECS_KEY_COUNT) {
				return true;
			}

			if (state != keyboard->Get(key)) {
				return false;
			}
		}
		else {
			// Return true since this element is not yet initialized
			// Or is unused
			if (mouse_button == ECS_MOUSE_BUTTON_COUNT) {
				return true;
			}

			if (state != mouse->Get(mouse_button)) {
				return false;
			}
		}

		return true;
	}

}