#pragma once
#include "../../Core.h"
#include "../Mouse.h"
#include "../Keyboard.h"

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	ECSENGINE_API void SerializeMouseInput(uintptr_t& buffer, const Mouse* state);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	ECSENGINE_API void SerializeKeyboardInput(uintptr_t& buffer, const Keyboard* state);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	ECSENGINE_API void SerializeInput(
		uintptr_t& buffer,
		const Mouse* mouse,
		const Keyboard* keyboard
	);

	// -----------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void DeserializeMouseInput(uintptr_t& buffer, Mouse* state);

	// -----------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void DeserializeKeyboardInput(uintptr_t& buffer, Keyboard* state);

	// -----------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void DeserializeInput(
		uintptr_t& buffer,
		Mouse* mouse_state,
		Keyboard* keyboard_state
	);

	// -----------------------------------------------------------------------------------------------------------------------------

}