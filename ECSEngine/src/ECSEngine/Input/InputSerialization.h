#pragma once
#include "../Core.h"
#include "Mouse.h"
#include "Keyboard.h"

namespace ECSEngine {

	// All of the functions bellow use the buffer capacity parameter to know if to not overwrite the boundaries
	// When serializing and deserializing. All functions return true if the data could be read/written, else false.

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	ECSENGINE_API bool SerializeMouseInput(uintptr_t& buffer, const Mouse* state, size_t buffer_capacity);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	ECSENGINE_API bool SerializeKeyboardInput(uintptr_t& buffer, const Keyboard* state, size_t buffer_capacity);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	ECSENGINE_API bool SerializeInput(
		uintptr_t& buffer,
		const Mouse* mouse,
		const Keyboard* keyboard,
		size_t buffer_capacity
	);

	// -----------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool DeserializeMouseInput(uintptr_t& buffer, Mouse* state, size_t buffer_capacity);

	// -----------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool DeserializeKeyboardInput(uintptr_t& buffer, Keyboard* state, size_t buffer_capacity);

	// -----------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool DeserializeInput(
		uintptr_t& buffer,
		Mouse* mouse_state,
		Keyboard* keyboard_state,
		size_t buffer_capacity
	);

	// -----------------------------------------------------------------------------------------------------------------------------

}