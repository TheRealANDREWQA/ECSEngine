#pragma once
#include "../Core.h"
#include "Mouse.h"
#include "Keyboard.h"
#include "../Utilities/Serialization/DeltaStateSerializationForward.h"

namespace ECSEngine {

	// All of the functions bellow use the buffer capacity parameter to know if to not overwrite the boundaries
	// When serializing and deserializing. All functions return true if the data could be read/written, else false.

	struct World;

	enum ECS_INPUT_SERIALIZE_TYPE : unsigned char {
		ECS_INPUT_SERIALIZE_MOUSE,
		ECS_INPUT_SERIALIZE_KEYBOARD,
		ECS_INPUT_SERIALIZE_COUNT
	};

	typedef DeltaStateGenericHeader InputSerializationHeader;

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns the current input serialization version. It will be at max a byte.
	ECSENGINE_API unsigned char SerializeInputVersion();

	// Returns a header that should be written in the serialized range
	ECSENGINE_API DeltaStateGenericHeader GetInputSerializeHeader();

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

	// It assumes that the prefix byte was read
	// Uses delta encoding in order to reduce the memory footprint of the deserialization
	// Returns true if it succeeded, else false
	ECSENGINE_API bool DeserializeMouseInputDelta(const Mouse* previous_state, Mouse* current_state, ReadInstrument* read_instrument, const InputSerializationHeader& header);

	// It assumes that the prefix byte was read
	// Returns true if it succeeded, else false
	ECSENGINE_API bool DeserializeMouseInput(Mouse* state, ReadInstrument* read_instrument, const InputSerializationHeader& header);

	// -----------------------------------------------------------------------------------------------------------------------------

	// It assumes that the prefix byte was read
	// Uses delta encoding in order to reduce the memory footprint of the deserialization.
	// Returns true if it succeeded, else false.
	ECSENGINE_API bool DeserializeKeyboardInputDelta(const Keyboard* previous_state, Keyboard* current_state, ReadInstrument* read_instrument, const InputSerializationHeader& header);

	// It assumes that the prefix byte was read
	// Returns true if it succeeded, else false
	ECSENGINE_API bool DeserializeKeyboardInput(Keyboard* state, ReadInstrument* read_instrument, const InputSerializationHeader& header);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Returns the input type that was read, or COUNT if no input could be read
	// The last parameter can be used to prevent reading input types that you do not expect
	ECSENGINE_API ECS_INPUT_SERIALIZE_TYPE DeserializeInputDelta(
		const Mouse* previous_mouse,
		Mouse* current_mouse,
		const Keyboard* previous_keyboard,
		Keyboard* current_keyboard,
		ReadInstrument* read_instrument,
		const InputSerializationHeader& header,
		Stream<ECS_INPUT_SERIALIZE_TYPE> accepted_input_types = {}
	);

	// Returns the input type that was read, or COUNT if no input could be read
	// The last parameter can be used to prevent reading input types that you do not expect
	ECSENGINE_API ECS_INPUT_SERIALIZE_TYPE DeserializeInput(
		Mouse* mouse,
		Keyboard* keyboard,
		ReadInstrument* read_instrument,
		const InputSerializationHeader& header,
		Stream<ECS_INPUT_SERIALIZE_TYPE> accepted_input_types = {}
	);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Sets the necessary info for the writer to be initialized as an input delta writer - outside the runtime context
	// The mouse and keyboard must be stable for the entire duration of the writer
	ECSENGINE_API void SetInputDeltaWriterInitializeInfo(DeltaStateWriterInitializeFunctorInfo& info, const Mouse* mouse, const Keyboard* keyboard, AllocatorPolymorphic temporary_allocator);
	
	// Sets the necessary info for the writer to be initialized as an input delta writer - for a simulation world
	ECSENGINE_API void SetInputDeltaWriterWorldInitializeInfo(DeltaStateWriterInitializeFunctorInfo& info, const World* world, AllocatorPolymorphic temporary_allocator);

	// Sets the necessary info for the writer to be initialized as an input delta writer - outside the runtime context
	// The mouse and keyboard must be stable for the entire duration of the reader
	ECSENGINE_API void SetInputDeltaReaderInitializeInfo(DeltaStateReaderInitializeFunctorInfo& info, Mouse* mouse, Keyboard* keyboard, AllocatorPolymorphic temporary_allocator);

	// Sets the necessary info for the writer to be initialized as an input delta writer - outside the runtime context
	ECSENGINE_API void SetInputDeltaReaderWorldInitializeInfo(DeltaStateReaderInitializeFunctorInfo& info, World* world, AllocatorPolymorphic temporary_allocator);
	
	// -----------------------------------------------------------------------------------------------------------------------------

}