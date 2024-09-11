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

	struct ReadInstrument;
	struct WriteInstrument;

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns the current input serialization version. It will be at max a byte.
	ECSENGINE_API unsigned char SerializeInputVersion();

	// Returns a header that should be written in the serialized range such
	ECSENGINE_API InputSerializationHeader GetSerializeInputHeader();

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	// Returns true if it succeeded, else false
	ECSENGINE_API bool SerializeMouseInputDelta(const Mouse* previous_state, const Mouse* current_state, WriteInstrument* write_instrument);

	// Returns true if it succeeded, i.e. there is enough space in the buffer, else false
	ECSENGINE_API bool SerializeMouseInput(const Mouse* state, WriteInstrument* write_instrument);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the serialization
	// Returns true if it succeeded, else false
	ECSENGINE_API bool SerializeKeyboardInputDelta(const Keyboard* previous_state, const Keyboard* current_state, WriteInstrument* write_instrument);

	// Returns true if it succeeded, else false
	ECSENGINE_API bool SerializeKeyboardInput(const Keyboard* state, WriteInstrument* write_instrument);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the deserialization
	// Returns true if it succeeded, else false
	ECSENGINE_API bool DeserializeMouseInputDelta(const Mouse* previous_state, Mouse* current_state, ReadInstrument* read_instrument, const InputSerializationHeader& header);

	// Returns true if it succeeded, else false
	ECSENGINE_API bool DeserializeMouseInput(Mouse* state, ReadInstrument* read_instrument, const InputSerializationHeader& header);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Uses delta encoding in order to reduce the memory footprint of the deserialization
	// Returns true if it succeeded, else false
	ECSENGINE_API bool DeserializeKeyboardInputDelta(const Keyboard* previous_state, Keyboard* current_state, ReadInstrument* read_instrument, const InputSerializationHeader& header);

	// Returns true if it succeeded, else false
	ECSENGINE_API bool DeserializeKeyboardInput(Keyboard* state, ReadInstrument* read_instrument, const InputSerializationHeader& header);

	// -----------------------------------------------------------------------------------------------------------------------------

}