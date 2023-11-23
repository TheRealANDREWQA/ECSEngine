#include "ecspch.h"
#include "InputMapping.h"

namespace ECSEngine {

	void InputMapping::Initialize(AllocatorPolymorphic allocator, size_t count)
	{
		mappings.Initialize(allocator, count);
	}

	bool InputMapping::IsTriggered(unsigned int index) const
	{
		InputMappingElement element = mappings[index];
		return element.first.IsTriggered(mouse, keyboard) && element.second.IsTriggered(mouse, keyboard) && element.third.IsTriggered(mouse, keyboard);
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

}