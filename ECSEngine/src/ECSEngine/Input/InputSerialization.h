#pragma once
#include "../Core.h"
#include "Mouse.h"
#include "Keyboard.h"
#include "../Utilities/Serialization/DeltaStateSerialization.h"

namespace ECSEngine {

	// All of the functions bellow use the buffer capacity parameter to know if to not overwrite the boundaries
	// When serializing and deserializing. All functions return true if the data could be read/written, else false.

	// A header to be written at the start of an input file or a file that records inputs.
	struct InputSerializationHeader {
		unsigned char version;
		unsigned char reserved[7];
	};

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns the current input serialization version. It will be at max a byte.
	ECSENGINE_API unsigned char SerializeInputVersion();

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns the number of bytes the mouse delta serialization can use at a maximum
	ECSENGINE_API size_t SerializeMouseInputDeltaMaxSize();

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	// The last argument is the number of seconds that have elapsed since the simulation start
	// Returns true if it succeeded, i.e. there is enough space in the buffer, else false
	ECSENGINE_API bool SerializeMouseInputDelta(const Mouse* previous_state, const Mouse* current_state, uintptr_t& buffer, size_t& buffer_capacity, float elapsed_seconds);

	// Returns the number of bytes the mouse (entire) serialization can use at a maximum
	ECSENGINE_API size_t SerializeMouseInputMaxSize();

	// Returns true if it succeeded, i.e. there is enough space in the buffer, else false
	ECSENGINE_API bool SerializeMouseInput(const Mouse* state, uintptr_t& buffer, size_t& buffer_capacity, float elapsed_seconds);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns the number of bytes the keyboard delta serialization can use at a maximum
	ECSENGINE_API size_t SerializeKeyboardInputDeltaMaxSize();

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	// The last argument is the number of seconds that have elapsed since the simulation start
	// Returns true if it succeeded, i.e. there is enough space in the buffer, else false
	ECSENGINE_API bool SerializeKeyboardInputDelta(const Keyboard* previous_state, const Keyboard* current_state, uintptr_t& buffer, size_t& buffer_capacity, float elapsed_seconds);

	// Returns the number of bytes the keyboard (entire) serialization can use at a maximum
	ECSENGINE_API size_t SerializeKeyboardInputMaxSize();

	// Returns true if it succeeded, i.e. there is enough space in the buffer, else false
	ECSENGINE_API bool SerializeKeyboardInput(const Keyboard* state, uintptr_t& buffer, size_t& buffer_capacity, float elapsed_seconds);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns true if it succeeded (i.e. there is enough capacity in the buffer and the values are valid), else false
	ECSENGINE_API bool DeserializeMouseInputDelta(const Mouse* previous_state, Mouse* current_state, uintptr_t& buffer, size_t& buffer_capacity, float& elapsed_seconds, unsigned char version);

	// Returns true if it succeeded (i.e. there is enough capacity in the buffer and the values are valid), else false
	ECSENGINE_API bool DeserializeMouseInput(Mouse* state, uintptr_t& buffer, size_t& buffer_capacity, float& elapsed_seconds, unsigned char version);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns true if it succeeded (i.e. there is enough capacity in the buffer and the values are valid), else false
	ECSENGINE_API bool DeserializeKeyboardInputDelta(const Keyboard* previous_state, Keyboard* current_state, uintptr_t& buffer, size_t& buffer_capacity, float& elapsed_seconds, unsigned char version);

	// Returns true if it succeeded (i.e. there is enough capacity in the buffer and the values are valid), else false
	ECSENGINE_API bool DeserializeKeyboardInput(Keyboard* state, uintptr_t& buffer, size_t& buffer_capacity, float& elapsed_seconds, unsigned char version);

	// -----------------------------------------------------------------------------------------------------------------------------

}