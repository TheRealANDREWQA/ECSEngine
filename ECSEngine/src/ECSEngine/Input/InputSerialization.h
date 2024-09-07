#pragma once
#include "../Core.h"
#include "Mouse.h"
#include "Keyboard.h"

namespace ECSEngine {

	// All of the functions bellow use the buffer capacity parameter to know if to not overwrite the boundaries
	// When serializing and deserializing. All functions return true if the data could be read/written, else false.

	// A header to be written at the start of an input file or a file that records inputs.
	struct InputSerializationHeader {
		unsigned char version;
		unsigned char reserved[7];
	};

	// A helper structure that helps in writing input serialization data in an efficient
	struct ECSENGINE_API InputSerializationState {
		void Write(const Mouse* current_mouse, const Keyboard* keyboard, float elapsed_seconds);

		struct DeltaStateInfo {
			unsigned int count;
			unsigned int file_offset;
		};

		AllocatorPolymorphic allocator;
		// Maintain the entire state in a chunked fashion such that we eliminate the need to
		// Copy the data with a normal array.
		ResizableStream<CapacityStream<void>> entire_state_chunks;
		// Maintain the delta states in between the entire states, chunked for the same reason
		ResizableStream<CapacityStream<void>> delta_state_chunks;
		// Info about the delta states, specifically, information to help with seeking
		// The number of entries in this array must be the same as the entire_state_count
		ResizableStream<DeltaStateInfo> delta_state_count;
		size_t entire_state_count;
		// The "previous" values, which lag one frame behind the actual values
		Mouse mouse;
		Keyboard keyboard;
		// How many seconds it takes to write a new entire state again, which helps with seeking time
		float entire_state_write_seconds_tick;
		// The elapsed second duration of the last entire write
		float last_entire_state_write_seconds;
	};

	struct InputSerializedType {
		bool success;
		// This flag indicates whether or not the read type is a mouse or a keyboard
		bool is_mouse;
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

	// Returns true if it succeeded (i.e. there is enough capacity in the buffer), else false
	ECSENGINE_API bool ReadInputSerializationHeader(uintptr_t& buffer, size_t& buffer_capacity, InputSerializationHeader& header);

	// Returns true if it succeeded (i.e. there is enough capacity in the buffer), else false.
	// It reads only a single entry, either a mouse or a keyboard, and updates only that entry. For example, if a mouse is read,
	// the keyboard elapsed seconds and the keyboard are not modified at all
	ECSENGINE_API InputSerializedType ReadInputs(InputSerializationState& state, Mouse* mouse, Keyboard* keyboard, uintptr_t& buffer, size_t& buffer_capacity, float& mouse_elapsed_seconds, float& keyboard_elapsed_seconds, unsigned char version);

	// Returns true if it succeeded (i.e. there is enough capacity in the buffer), else false
	// When an entry is written, it will write a prefix byte which indicates whether or not the current input is a mouse or keyboard input
	ECSENGINE_API bool WriteInputs(InputSerializationState& state, const Mouse* current_mouse, const Keyboard* keyboard, uintptr_t& buffer, size_t& buffer_capacity, float elapsed_seconds);

	// Returns true if it succeeded (i.e. there is enough capacity in the buffer), else false
	ECSENGINE_API bool WriteInputSerializationHeader(uintptr_t& buffer, size_t& buffer_capacity);

	// -----------------------------------------------------------------------------------------------------------------------------

}