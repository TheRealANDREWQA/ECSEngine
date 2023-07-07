#pragma once
#include "Mouse.h"
#include "Keyboard.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	enum ECS_BUTTON_STATE : unsigned char {

	};

	struct InputMappingElement {
		bool first_is_key;
		bool second_is_key;
		union {
			ECS_MOUSE_BUTTON first_mouse_button;
			Key first_key;
		};
		union {
			ECS_MOUSE_BUTTON second_mouse_button;
			Key second_key;
		};
	};

	struct ECSENGINE_API InputMapping {


	};

}