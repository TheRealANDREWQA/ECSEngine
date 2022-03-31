#pragma once
#include "../../Core.h"
#include "../Mouse.h"
#include "../Keyboard.h"

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	ECSENGINE_API void SerializeMouseInput(uintptr_t& buffer, const HID::MouseState* state);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	ECSENGINE_API void SerializeKeyboardInput(uintptr_t& buffer, const HID::KeyboardState* state);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	ECSENGINE_API void SerializeInput(
		uintptr_t& buffer,
		const HID::MouseState* mouse_state,
		const HID::KeyboardState* keyboard_state
	);

	// -----------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void DeserializeMouseInput(uintptr_t& buffer, HID::MouseState* state);

	// -----------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void DeserializeKeyboardInput(uintptr_t& buffer, HID::KeyboardState* state);

	// -----------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void DeserializeInput(
		uintptr_t& buffer,
		HID::MouseState* mouse_state,
		HID::KeyboardState* keyboard_state
	);

	// -----------------------------------------------------------------------------------------------------------------------------

}