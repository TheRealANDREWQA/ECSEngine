#pragma once
#include "Mouse.h"
#include "Keyboard.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	struct ECSENGINE_API InputMappingButton {
		ECS_INLINE InputMappingButton() : is_key(false), key(ECS_KEY_NONE) {}

		bool IsTriggered(const Mouse* mouse, const Keyboard* keyboard) const;

		bool is_key;
		union {
			ECS_MOUSE_BUTTON mouse_button;
			ECS_KEY key;
		};
		ECS_BUTTON_STATE state;
	};

	struct InputMappingElement {
		InputMappingButton first;
		InputMappingButton second;
		InputMappingButton third;
	};

	struct ECSENGINE_API InputMapping {
		void Initialize(AllocatorPolymorphic allocator, size_t count);

		bool IsTriggered(unsigned int index, const Mouse* mouse, const Keyboard* keyboard) const;

		void ChangeMapping(unsigned int index, InputMappingElement mapping_element);


		Stream<InputMappingElement> mappings;
	};

}