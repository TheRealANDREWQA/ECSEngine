#include "ecspch.h"
#include "InputSerialization.h"
#include "../Utilities/Serialization/SerializationHelpers.h"
#include "../Utilities/Serialization/SerializeIntVariableLength.h"
#include "../Utilities/Utilities.h"
#include "../Utilities/ReaderWriterInterface.h"
#include "../Utilities/Serialization/DeltaStateSerialization.h"
#include "../ECS/World.h"

#define VERSION 0

#define MAX_KEYBOARD_KEY_DELTA_COUNT (ECS_KEY_COUNT / 2)
#define MAX_KEYBOARD_CHARACTER_QUEUE_DELTA_COUNT 32
#define MAX_KEYBOARD_CHARACTER_QUEUE_ENTIRE_COUNT 64
#define MAX_KEYBOARD_ALPHANUMERIC_DELTA_COUNT 32
#define MAX_KEYBOARD_ALPHANUMERIC_ENTIRE_COUNT 64

#define MOUSE_RAW_STATE_BIT ECS_BIT(0)
#define MOUSE_IS_VISIBLE_BIT ECS_BIT(1)
#define MOUSE_WRAP_POSITION_BIT ECS_BIT(2)
#define MOUSE_HAS_WRAPPED_BIT ECS_BIT(3)
#define MOUSE_DELTA_X_BIT ECS_BIT(4)
#define MOUSE_DELTA_Y_BIT ECS_BIT(5)
#define MOUSE_DELTA_SCROLL_BIT ECS_BIT(6)

#define KEYBOARD_DELTA_CHARACTER_QUEUE_BIT ECS_BIT(0)
#define KEYBOARD_DELTA_PROCESS_CHARACTERS_BIT ECS_BIT(1)
#define KEYBOARD_DELTA_PUSHED_CHARACTER_COUNT_BIT ECS_BIT(2)
#define KEYBOARD_DELTA_CHANGED_KEYS_BIT ECS_BIT(3)
#define KEYBOARD_DELTA_ALPHANUMERIC_BIT ECS_BIT(4)

namespace ECSEngine {

	// This is the structure that is mimicked by the serialize mouse delta function
	//struct SerializeMouseDelta {
	//	float elapsed_seconds;
	//	int x_delta;
	//	int y_delta;
	//	int wheel_delta;
	//	int buttons : 1 [ECS_MOUSE_BUTTON_COUNT];
	//};

	// This is the structure that is mimicked by the serialize mouse (entire) function
	//struct SerializeMouseEntire {
	//	float elapsed_seconds;
	//	int x_position;
	//	int y_position;
	//	int mouse_wheel;
	//	int buttons : 2[ECS_MOUSE_BUTTON_COUNT]; // Store the explicit state, because we can't reproduce this only from a boolean
	//};

	// This is the structure that is mimicked by the serialize keyboard delta function
	//struct SerializeKeyboardDelta {
	//	float elapsed_seconds;
	//	bool difference_bit_mask;
	//	unsigned int pushed_character_count;
	//	KeyboardCharacterQueue character_queue;
	//	Stream<ECS_KEY> keys; // These are the keys that have changed, without storing their explicit state
	//  Stream<AlphanumericKeys> alphanumeric_keys; // This is not the delta, but the actual values, since coming up with a way to encode the delta
	//												// Would be less efficient than storing the values themselves
	//};

	// This is the structure that is mimicked by the serialize keyboard (entire) function
	//struct SerializeKeyboard {
	//	float elapsed_seconds;
	//	bool process_characters;
	//	unsigned int pushed_character_count;
	//	KeyboardCharacterQueue character_queue;
	//  Stream<AlphanumericKeys> alphanumeric_keys;
	//	int button_states : 2[ECS_KEY_COUNT];
	//};

	// -----------------------------------------------------------------------------------------------------------------------------

	static unsigned char GetMouseButtonStates(const Mouse* mouse) {
		// Each button state is a bit in this entry, starting from the LSB and going upwards
		unsigned char button_states = 0;
		for (unsigned char index = 0; index < ECS_MOUSE_BUTTON_COUNT; index++) {
			button_states |= (unsigned char)mouse->IsDown((ECS_MOUSE_BUTTON)index) << index;
		}
		return button_states;
	};

	struct MouseDeltaValues {
		int2 position_delta;
		int scroll_delta;
	};

	// Computes the compressed boolean mask byte and outputs delta values for the integer coordinates
	static unsigned char GetMouseBooleanBits(const Mouse* current_mouse, const Mouse* previous_mouse, MouseDeltaValues& delta_values) {
		unsigned char bits = 0;

		delta_values.position_delta = current_mouse->GetPosition() - previous_mouse->GetPosition();
		delta_values.scroll_delta = current_mouse->GetScrollValue() - previous_mouse->GetScrollValue();

		bits |= (current_mouse->m_get_raw_input != previous_mouse->m_get_raw_input) ? MOUSE_RAW_STATE_BIT : 0;
		bits |= (current_mouse->m_is_visible != previous_mouse->m_is_visible) ? MOUSE_IS_VISIBLE_BIT : 0;
		bits |= (current_mouse->m_wrap_position != previous_mouse->m_wrap_position) ? MOUSE_WRAP_POSITION_BIT : 0;
		bits |= (current_mouse->m_has_wrapped != previous_mouse->m_has_wrapped) ? MOUSE_HAS_WRAPPED_BIT : 0;
		bits |= delta_values.position_delta.x != 0 ? MOUSE_DELTA_X_BIT : 0;
		bits |= delta_values.position_delta.y != 0 ? MOUSE_DELTA_Y_BIT : 0;
		bits |= delta_values.scroll_delta != 0 ? MOUSE_DELTA_SCROLL_BIT : 0;

		return bits;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	unsigned char SerializeInputVersion() {
		return VERSION;
	}

	DeltaStateGenericHeader GetInputSerializeHeader() {
		DeltaStateGenericHeader header;
		memset(&header, 0, sizeof(header));
		header.version = VERSION;
		return header;
	}

	bool SerializeMouseInputDelta(const Mouse* previous_state, const Mouse* current_state, WriteInstrument* write_instrument) {
		// Determine if there is any difference at all between the two states. If there is not, we can simply skip the entry all together.
		unsigned char previous_button_states = GetMouseButtonStates(previous_state);
		unsigned char current_button_states = GetMouseButtonStates(current_state);

		MouseDeltaValues delta_values;
		unsigned char boolean_change_mask = GetMouseBooleanBits(current_state, previous_state, delta_values);
		
		if (boolean_change_mask != 0 || previous_button_states != current_button_states) {
			// One of the values changed
			ECS_INPUT_SERIALIZE_TYPE type = ECS_INPUT_SERIALIZE_MOUSE;
			if (!write_instrument->Write(&type)) {
				return false;
			}

			// Write the button states, then the boolean change mask followed by the variable length deltas.
			if (!write_instrument->Write(&current_button_states)) {
				return false;
			}

			if (!write_instrument->Write(&boolean_change_mask)) {
				return false;
			}

			if (boolean_change_mask & MOUSE_DELTA_X_BIT) {
				if (!SerializeIntVariableLengthBool(write_instrument, delta_values.position_delta.x)) {
					return false;
				}
			}
			if (boolean_change_mask & MOUSE_DELTA_Y_BIT) {
				if (!SerializeIntVariableLengthBool(write_instrument, delta_values.position_delta.y)) {
					return false;
				}
			}
			if (boolean_change_mask & MOUSE_DELTA_SCROLL_BIT) {
				if (!SerializeIntVariableLengthBool(write_instrument, delta_values.scroll_delta)) {
					return false;
				}
			}

			// TODO: We probably don't need these to be written out by deltas, we should let the
			// game logic handle it.

			//// Now the more exotic flags
			//if (boolean_change_mask & MOUSE_IS_VISIBLE_BIT) {
			//	// When this transitions to non visible, write the restore position as well
			//	if (!current_state->IsVisible()) {
			//		if (!SerializeIntVariableLengthBool(write_instrument, current_state->m_restore_position)) {
			//			return false;
			//		}
			//	}
			//}

			//if (boolean_change_mask & MOUSE_WRAP_POSITION_BIT) {
			//	if (current_state->m_wrap_position) {
			//		if (!SerializeIntVariableLengthBool(write_instrument, current_state->m_wrap_pixel_bounds)) {
			//			return false;
			//		}
			//	}
			//}

			//if (boolean_change_mask & MOUSE_HAS_WRAPPED_BIT) {
			//	if (current_state->m_has_wrapped) {
			//		if (!SerializeIntVariableLengthBool(write_instrument, current_state->m_before_wrap_position)) {
			//			return false;
			//		}
			//	}
			//}
		}
		return true;
	}

	bool SerializeMouseInput(const Mouse* state, WriteInstrument* write_instrument)
	{
		ECS_INPUT_SERIALIZE_TYPE type = ECS_INPUT_SERIALIZE_MOUSE;
		if (!write_instrument->Write(&type)) {
			return false;
		}

		// Write the button states and the variable length integers at last
		unsigned short button_states;
		WriteBits(&button_states, 2, ECS_MOUSE_BUTTON_COUNT, [state](size_t index, void* value) {
			unsigned char* byte = (unsigned char*)value;
			*byte = state->m_states[index];
			});
		if (!write_instrument->Write(&button_states)) {
			return false;
		}

		// Write the boolean flags as a single byte
		unsigned char combined_boolean_flags = 0;
		combined_boolean_flags |= state->m_get_raw_input ? MOUSE_RAW_STATE_BIT : 0;
		combined_boolean_flags |= state->m_is_visible ? MOUSE_IS_VISIBLE_BIT : 0;
		combined_boolean_flags |= state->m_wrap_position ? MOUSE_WRAP_POSITION_BIT : 0;
		combined_boolean_flags |= state->m_has_wrapped ? MOUSE_HAS_WRAPPED_BIT : 0;
		if (!write_instrument->Write(&combined_boolean_flags)) {
			return false;
		}

		// Write all integer values with variable length serialization - the only exception are the wrap integers, 
		// Do that only if the wrap position boolean is set
		if (!SerializeIntVariableLengthBoolMultiple(write_instrument, state->GetPosition(), state->GetScrollValue(), state->GetPreviousPosition(), state->GetPreviousScroll())) {
			return false;
		}

		if (state->m_wrap_position) {
			if (!SerializeIntVariableLengthBoolMultiple(write_instrument, state->m_wrap_pixel_bounds, state->m_before_wrap_position)) {
				return false;
			}
		}

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool SerializeKeyboardInputDelta(const Keyboard* previous_state, const Keyboard* current_state, WriteInstrument* write_instrument)
	{
		// Determine if there is any difference between the 2 states
		bool is_process_queue_different = previous_state->m_process_characters != current_state->m_process_characters;
		bool is_pushed_character_count_different = previous_state->m_pushed_character_count != current_state->m_pushed_character_count;
		bool is_queue_different = !previous_state->m_character_queue.Compare(current_state->m_character_queue);
		ECS_STACK_CAPACITY_STREAM(ECS_KEY, changed_keys, ECS_KEY_COUNT);
		GetKeyboardButtonDelta(previous_state, current_state, changed_keys);
		bool is_alphanumeric_different = previous_state->m_alphanumeric_keys.size != current_state->m_alphanumeric_keys.size || memcmp(previous_state->m_alphanumeric_keys.buffer, current_state->m_alphanumeric_keys.buffer, 
			previous_state->m_alphanumeric_keys.MemoryOf(current_state->m_alphanumeric_keys.size)) != 0;

		if (is_process_queue_different || is_pushed_character_count_different || is_queue_different || changed_keys.size > 0 || is_alphanumeric_different) {
			// There is a difference, we must serialize a state

			ECS_INPUT_SERIALIZE_TYPE type = ECS_INPUT_SERIALIZE_KEYBOARD;
			if (!write_instrument->Write(&type)) {
				return false;
			}

			// Write a compressed mask that indicates what deltas there are such that we can skip those that are not needed
			unsigned char difference_bit_mask = 0;
			difference_bit_mask |= is_process_queue_different ? KEYBOARD_DELTA_PROCESS_CHARACTERS_BIT : 0;
			difference_bit_mask |= is_pushed_character_count_different ? KEYBOARD_DELTA_PUSHED_CHARACTER_COUNT_BIT : 0;
			difference_bit_mask |= is_queue_different ? KEYBOARD_DELTA_CHARACTER_QUEUE_BIT : 0;
			difference_bit_mask |= changed_keys.size > 0 ? KEYBOARD_DELTA_CHANGED_KEYS_BIT : 0;
			difference_bit_mask |= is_alphanumeric_different ? KEYBOARD_DELTA_ALPHANUMERIC_BIT : 0;
			if (!write_instrument->Write(&difference_bit_mask)) {
				return false;
			}

			// Continue with the individual values, except for the boolean m_process_characters, which is already transmitted through the bit mask
			if (is_pushed_character_count_different) {
				if (!SerializeIntVariableLengthBool(write_instrument, (int)(current_state->m_pushed_character_count - previous_state->m_pushed_character_count))) {
					return false;
				}
			}

			if (is_queue_different) {
				ECS_ASSERT(current_state->m_character_queue.GetSize() <= MAX_KEYBOARD_CHARACTER_QUEUE_DELTA_COUNT);
				unsigned char queue_size = current_state->m_character_queue.GetSize();
				if (!write_instrument->Write(&queue_size)) {
					return false;
				}
				ECS_STACK_CAPACITY_STREAM(char, character_queue_array, UCHAR_MAX);
				uintptr_t character_queue_ptr = (uintptr_t)character_queue_array.buffer;
				current_state->m_character_queue.CopyTo(character_queue_ptr);
				if (!write_instrument->Write(character_queue_array.buffer, (size_t)queue_size * sizeof(char))) {
					return false;
				}
			}

			if (changed_keys.size > 0) {
				// The states are already compressed enough, since there will be a small number of them
				ECS_ASSERT(changed_keys.size <= MAX_KEYBOARD_KEY_DELTA_COUNT);
				if (!write_instrument->WriteWithSize<unsigned char>(changed_keys)) {
					return false;
				}
			}

			if (is_alphanumeric_different) {
				ECS_ASSERT(current_state->m_alphanumeric_keys.size <= MAX_KEYBOARD_ALPHANUMERIC_DELTA_COUNT);
				if (!write_instrument->WriteWithSize<unsigned char>(current_state->m_alphanumeric_keys)) {
					return false;
				}
			}
		}

		return true;
	}

	bool SerializeKeyboardInput(const Keyboard* state, WriteInstrument* write_instrument)
	{
		ECS_ASSERT(state->m_character_queue.GetSize() <= MAX_KEYBOARD_CHARACTER_QUEUE_ENTIRE_COUNT);
		ECS_ASSERT(state->m_alphanumeric_keys.size <= MAX_KEYBOARD_ALPHANUMERIC_ENTIRE_COUNT);

		ECS_INPUT_SERIALIZE_TYPE input_type = ECS_INPUT_SERIALIZE_KEYBOARD;
		if (!write_instrument->Write(&input_type)) {
			return false;
		}

		if (!write_instrument->Write(&state->m_process_characters)) {
			return false;
		}
		if (!SerializeIntVariableLengthBool(write_instrument, state->m_pushed_character_count)) {
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(char, character_queue_storage, MAX_KEYBOARD_CHARACTER_QUEUE_ENTIRE_COUNT);
		character_queue_storage.size = state->m_character_queue.GetSize();
		ECS_ASSERT(character_queue_storage.size <= character_queue_storage.capacity, "Serializing keyboard failed because the character queue exceeded the capacity!");
		uintptr_t character_queue_storage_ptr = (uintptr_t)character_queue_storage.buffer;
		state->m_character_queue.CopyTo(character_queue_storage_ptr);
		if (!write_instrument->WriteWithSize<unsigned char>(character_queue_storage)) {
			return false;
		}

		ECS_ASSERT(state->m_alphanumeric_keys.size <= MAX_KEYBOARD_ALPHANUMERIC_ENTIRE_COUNT, "Serializing keyboard failed because the alphanumeric key size exceeded the maximum allowed!");
		if (!write_instrument->WriteWithSize<unsigned char>(state->m_alphanumeric_keys)) {
			return false;
		}

		// Pack the key states into a binary stream of 2 bits per entry
		// The storage here is much larger than needed, just to be safe
		ECS_STACK_CAPACITY_STREAM(char, key_state, ECS_KEY_COUNT);
		size_t key_state_byte_count = WriteBits(key_state.buffer, 2, ECS_KEY_COUNT, [state](size_t index, void* value) {
			unsigned char* byte_value = (unsigned char*)value;
			*byte_value = state->m_states[index];
		});
		if (!write_instrument->Write(key_state.buffer, key_state_byte_count)) {
			return false;
		}

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool DeserializeMouseInputDelta(const Mouse* previous_state, Mouse* current_state, ReadInstrument* read_instrument, const InputSerializationHeader& header) 
	{
		if (header.version != VERSION) {
			return false;
		}

		current_state->UpdateFromOther(previous_state);
		// Set the previous mouse position to the current one, before reading the changes, otherwise the
		// Last position won't be updated
		current_state->m_previous_position = current_state->m_current_position;
		current_state->m_previous_scroll = current_state->m_current_scroll;

		// Read the combined bit mask
		unsigned char button_states = 0;
		if (!read_instrument->Read(&button_states)) {
			return false;
		}

		unsigned char boolean_change_mask = 0;
		if (!read_instrument->Read(&boolean_change_mask)) {
			return false;
		}

		// Update the button states of current state based on the bit mask
		for (size_t index = 0; index < ECS_MOUSE_BUTTON_COUNT; index++) {
			if ((button_states & ECS_BIT(index)) != 0) {
				// Flip the state
				current_state->FlipButton((ECS_MOUSE_BUTTON)index);
			}
		}

		// Use the change mask to determine what other data to read
		if (HasFlag(boolean_change_mask, MOUSE_DELTA_X_BIT)) {
			// There is a x delta
			int x_delta = 0;
			if (!DeserializeIntVariableLengthBool(read_instrument, x_delta)) {
				return false;
			}
			current_state->AddDelta(x_delta, 0);
		}
		if (HasFlag(boolean_change_mask, MOUSE_DELTA_Y_BIT)) {
			// There is a y delta
			int y_delta = 0;
			if (!DeserializeIntVariableLengthBool(read_instrument, y_delta)) {
				return false;
			}
			current_state->AddDelta(0, y_delta);
		}
		if (HasFlag(boolean_change_mask, MOUSE_DELTA_SCROLL_BIT)) {
			// There is a scroll delta
			int scroll_delta = 0;
			if (!DeserializeIntVariableLengthBool(read_instrument, scroll_delta)) {
				return false;
			}
			current_state->SetCursorWheel(current_state->GetScrollValue() + scroll_delta);
		}

		//// Update the "exotic" flags
		//if (boolean_change_mask & MOUSE_IS_VISIBLE_BIT) {
		//	current_state->SetCursorVisibility(!current_state->IsVisible(), false);
		//	// When this transitions to non visible, write the restore position as well
		//	if (!current_state->IsVisible()) {
		//		if (!DeserializeIntVariableLengthBool(read_instrument, current_state->m_restore_position)) {
		//			return false;
		//		}
		//	}
		//}

		//if (boolean_change_mask & MOUSE_WRAP_POSITION_BIT) {
		//	current_state->m_wrap_position = !current_state->m_wrap_position;
		//	if (current_state->m_wrap_position) {
		//		if (!DeserializeIntVariableLengthBool(read_instrument, current_state->m_wrap_pixel_bounds)) {
		//			return false;
		//		}
		//	}
		//}

		//if (boolean_change_mask & MOUSE_HAS_WRAPPED_BIT) {
		//	current_state->m_has_wrapped = !current_state->m_has_wrapped;
		//	if (current_state->m_has_wrapped) {
		//		if (!DeserializeIntVariableLengthBool(read_instrument, current_state->m_before_wrap_position)) {
		//			return false;
		//		}
		//	}
		//}

		//if (boolean_change_mask & MOUSE_RAW_STATE_BIT) {
		//	current_state->m_get_raw_input = !current_state->m_get_raw_input;
		//}

		return true;
	}

	bool DeserializeMouseInput(Mouse* state, ReadInstrument* read_instrument, const InputSerializationHeader& header)
	{
		if (header.version != VERSION) {
			return false;
		}

		unsigned short button_states = 0;
		if (!read_instrument->Read(&button_states)) {
			return false;
		}

		memset(state->m_states, 0, sizeof(state->m_states));
		ReadBits(&button_states, 2, ECS_MOUSE_BUTTON_COUNT, [state](size_t index, const void* value) {
			const ECS_BUTTON_STATE* button_value = (const ECS_BUTTON_STATE*)value;
			state->m_states[index] = *button_value;
		});

		unsigned char combined_boolean_flags = 0;
		if (!read_instrument->Read(&combined_boolean_flags)) {
			return false;
		}

		int2 position;
		int scroll_value;
		if (!DeserializeIntVariableLengthBoolMultipleEnsureRange(read_instrument, position, scroll_value, state->m_previous_position, state->m_previous_scroll)) {
			return false;
		}

		state->SetPosition(position.x, position.y);
		state->SetCursorWheel(scroll_value);
		// Modify the m_current_position to be equal to the position values, the call might change the values,
		// But we want m_current_position to be exactly the deserialized value
		state->m_current_position = position;

		// Process the flags at last, after all fields are set
		state->m_get_raw_input = HasFlag(combined_boolean_flags, MOUSE_RAW_STATE_BIT);
		bool is_visible = HasFlag(combined_boolean_flags, MOUSE_IS_VISIBLE_BIT);
		state->m_wrap_position = HasFlag(combined_boolean_flags, MOUSE_WRAP_POSITION_BIT);
		state->m_has_wrapped = HasFlag(combined_boolean_flags, MOUSE_HAS_WRAPPED_BIT);

		if (state->m_wrap_position) {
			if (!DeserializeIntVariableLengthBoolMultipleEnsureRange(read_instrument, state->m_wrap_pixel_bounds, state->m_before_wrap_position)) {
				return false;
			}
		}

		// Set the visible status flag
		state->SetCursorVisibility(is_visible, false);

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool DeserializeKeyboardInputDelta(const Keyboard* previous_state, Keyboard* current_state, ReadInstrument* read_instrument, const InputSerializationHeader& header) 
	{
		if (header.version != VERSION) {
			return false;
		}
		current_state->UpdateFromOther(previous_state);

		unsigned char difference_bit_mask = 0;
		if (!read_instrument->Read(&difference_bit_mask)) {
			return false;
		}

		// Determine what deltas there are present
		
		if (HasFlag(difference_bit_mask, KEYBOARD_DELTA_PUSHED_CHARACTER_COUNT_BIT)) {
			// The pushed character count
			int character_count_delta = 0;
			if (!DeserializeIntVariableLengthBool(read_instrument, character_count_delta)) {
				return false;
			}
			current_state->m_pushed_character_count += character_count_delta;
		}

		if (HasFlag(difference_bit_mask, KEYBOARD_DELTA_PROCESS_CHARACTERS_BIT)) {
			// The boolean process queue is different
			current_state->m_process_characters = !current_state->m_process_characters;
		}

		if (HasFlag(difference_bit_mask, KEYBOARD_DELTA_CHARACTER_QUEUE_BIT)) {
			// The character queue
			current_state->m_character_queue.Reset();
			ECS_ASSERT(current_state->m_character_queue.GetCapacity() >= MAX_KEYBOARD_CHARACTER_QUEUE_DELTA_COUNT);
			unsigned char queue_size = 0;
			if (!read_instrument->ReadWithSize<unsigned char>(current_state->m_character_queue.GetQueue()->buffer, queue_size, MAX_KEYBOARD_CHARACTER_QUEUE_DELTA_COUNT)) {
				return false;
			}
			current_state->m_character_queue.m_queue.size = queue_size;
		}

		if (HasFlag(difference_bit_mask, KEYBOARD_DELTA_CHANGED_KEYS_BIT)) {
			ECS_STACK_CAPACITY_STREAM(ECS_KEY, changed_keys, MAX_KEYBOARD_KEY_DELTA_COUNT);
			if (!read_instrument->ReadWithSize<unsigned char>(changed_keys)) {
				return false;
			}
			for (unsigned char index = 0; index < changed_keys.size; index++) {
				current_state->FlipButton(changed_keys[index]);
			}
		}

		if (HasFlag(difference_bit_mask, KEYBOARD_DELTA_ALPHANUMERIC_BIT)) {
			// The alphanumeric entries
			current_state->m_alphanumeric_keys.Reset();
			ECS_ASSERT(current_state->m_alphanumeric_keys.capacity >= MAX_KEYBOARD_ALPHANUMERIC_DELTA_COUNT);
			if (!read_instrument->ReadWithSize<unsigned char>(current_state->m_alphanumeric_keys)) {
				return false;
			}
		}

		return true;
	}

	bool DeserializeKeyboardInput(Keyboard* state, ReadInstrument* read_instrument, const InputSerializationHeader& header)
	{
		if (header.version != VERSION) {
			return false;
		}
		state->Reset();

		if (!read_instrument->Read(&state->m_process_characters)) {
			return false;
		}

		if (!DeserializeIntVariableLengthBool(read_instrument, state->m_pushed_character_count)) {
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(char, character_queue_storage, MAX_KEYBOARD_CHARACTER_QUEUE_ENTIRE_COUNT);
		if (!read_instrument->ReadWithSize<unsigned char>(character_queue_storage)) {
			return false;
		}
		state->m_character_queue.Reset();
		state->m_character_queue.PushRange(character_queue_storage);

		ECS_ASSERT(state->m_alphanumeric_keys.capacity >= MAX_KEYBOARD_CHARACTER_QUEUE_ENTIRE_COUNT, "Deserializing keyboard: expected larger alphanumeric buffer capacity.");
		if (!read_instrument->ReadWithSize<unsigned char>(state->m_alphanumeric_keys)) {
			return false;
		}

		// Pack the key states into a binary stream of 2 bits per entry
		// The storage here is much larger than needed, just to be safe
		ECS_STACK_CAPACITY_STREAM(char, key_state, ECS_KEY_COUNT);
		// 2 bits per entry, determine the number of bytes
		size_t key_state_byte_count = SlotsFor(ECS_KEY_COUNT * 2, 8);
		if (!read_instrument->Read(key_state.buffer, key_state_byte_count)) {
			return false;
		}

		ReadBits(key_state.buffer, 2, ECS_KEY_COUNT, [state](size_t index, const void* value) {
			const ECS_BUTTON_STATE* button_state = (const ECS_BUTTON_STATE*)value;
			state->m_states[index] = *button_state;
		});
		
		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	ECS_INPUT_SERIALIZE_TYPE DeserializeInputDelta(
		const Mouse* previous_mouse,
		Mouse* current_mouse,
		const Keyboard* previous_keyboard,
		Keyboard* current_keyboard,
		ReadInstrument* read_instrument,
		const InputSerializationHeader& header,
		Stream<ECS_INPUT_SERIALIZE_TYPE> accepted_input_types
	) {
		// Read the input byte
		ECS_INPUT_SERIALIZE_TYPE type = ECS_INPUT_SERIALIZE_COUNT;
		if (!read_instrument->Read(&type)) {
			return ECS_INPUT_SERIALIZE_COUNT;
		}

		bool is_type_found = true;
		if (accepted_input_types.size > 0) {
			is_type_found = accepted_input_types.Find(type) != -1;
		}

		if (is_type_found) {
			if (type == ECS_INPUT_SERIALIZE_MOUSE) {
				if (!DeserializeMouseInputDelta(previous_mouse, current_mouse, read_instrument, header)) {
					return ECS_INPUT_SERIALIZE_COUNT;
				}
				return type;
			}
			else if (type == ECS_INPUT_SERIALIZE_KEYBOARD) {
				if (!DeserializeKeyboardInputDelta(previous_keyboard, current_keyboard, read_instrument, header)) {
					return ECS_INPUT_SERIALIZE_COUNT;
				}
				return type;
			}
		}

		// Unidentified type or not accepted - possibly corrupted data
		return ECS_INPUT_SERIALIZE_COUNT;
	}

	ECS_INPUT_SERIALIZE_TYPE DeserializeInput(
		Mouse* mouse,
		Keyboard* keyboard,
		ReadInstrument* read_instrument,
		const InputSerializationHeader& header,
		Stream<ECS_INPUT_SERIALIZE_TYPE> accepted_input_types
	) {
		// Read the input byte
		ECS_INPUT_SERIALIZE_TYPE type = ECS_INPUT_SERIALIZE_COUNT;
		if (!read_instrument->Read(&type)) {
			return ECS_INPUT_SERIALIZE_COUNT;
		}

		bool is_type_found = true;
		if (accepted_input_types.size > 0) {
			is_type_found = accepted_input_types.Find(type) != -1;
		}

		if (is_type_found) {
			if (type == ECS_INPUT_SERIALIZE_MOUSE) {
				if (!DeserializeMouseInput(mouse, read_instrument, header)) {
					return ECS_INPUT_SERIALIZE_COUNT;
				}
				return type;
			}
			else if (type == ECS_INPUT_SERIALIZE_KEYBOARD) {
				if (!DeserializeKeyboardInput(keyboard, read_instrument, header)) {
					return ECS_INPUT_SERIALIZE_COUNT;
				}
				return type;
			}
		}

		// Unidentified type or unaccepted type - possibly corrupted data
		return ECS_INPUT_SERIALIZE_COUNT;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	// This is the structure that is stored in the delta state writer
	struct DeltaStateWriterData {
		Mouse previous_mouse;
		Keyboard previous_keyboard;
		const Mouse* mouse;
		const Keyboard* keyboard;
	};

	struct DeltaStateReaderData {
		Mouse previous_mouse;
		Keyboard previous_keyboard;
		Mouse* mouse;
		Keyboard* keyboard;
	};

	// This allows the writer to work in a self-contained manner
	struct DeltaStateWriterWorldData : DeltaStateWriterData {
		// Needed for the elapsed seconds
		const World* world;
	};

	static void InputDeltaWriterInitialize(void* user_data, AllocatorPolymorphic allocator) {
		DeltaStateWriterData* delta_state = (DeltaStateWriterData*)user_data;
		delta_state->previous_keyboard = Keyboard(allocator);
	}

	static void InputDeltaWriterDeallocate(void* user_data, AllocatorPolymorphic allocator) {
		DeltaStateWriterData* delta_state = (DeltaStateWriterData*)user_data;
		delta_state->previous_keyboard.Deallocate(allocator);
	}

	static void InputDeltaReaderInitialize(void* user_data, AllocatorPolymorphic allocator) {
		DeltaStateReaderData* delta_state = (DeltaStateReaderData*)user_data;
		delta_state->previous_keyboard = Keyboard(allocator);
	}

	static void InputDeltaReaderDeallocate(void* user_data, AllocatorPolymorphic allocator) {
		DeltaStateReaderData* delta_state = (DeltaStateReaderData*)user_data;
		delta_state->previous_keyboard.Deallocate(allocator);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	static bool InputDeltaWriterDeltaFunction(DeltaStateWriterDeltaFunctionData* function_data) {
		DeltaStateWriterData* data = (DeltaStateWriterData*)function_data->user_data;
		
		if (!SerializeMouseInputDelta(&data->previous_mouse, data->mouse, function_data->write_instrument)) {
			return false;
		}
		data->previous_mouse = *data->mouse;

		if (!SerializeKeyboardInputDelta(&data->previous_keyboard, data->keyboard, function_data->write_instrument)) {
			return false;
		}		
		data->previous_keyboard.CopyOther(data->keyboard);
		
		return true;
	}

	static bool InputDeltaWriterEntireFunction(DeltaStateWriterEntireFunctionData* function_data) {
		DeltaStateWriterData* data = (DeltaStateWriterData*)function_data->user_data;
		
		if (!SerializeMouseInput(data->mouse, function_data->write_instrument)) {
			return false;
		}
		data->previous_mouse = *data->mouse;

		if (!SerializeKeyboardInput(data->keyboard, function_data->write_instrument)) {
			return false;
		}
		data->previous_keyboard.CopyOther(data->keyboard);

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	// The deserialize functor is called with a (const InputSerializationHeader& header, Stream<ECS_INPUT_SERIALIZE_TYPE> accepted_input_types) parameters
	template<typename FunctorData, typename DeserializeFunctor>
	static bool InputDeltaReaderFunctionImpl(FunctorData* function_data, DeserializeFunctor&& deserialize_functor) {
		DeltaStateReaderData* data = (DeltaStateReaderData*)function_data->user_data;

		// If the header does not have the same size, fail
		if (function_data->header.size != sizeof(InputSerializationHeader)) {
			return false;
		}

		// If nothing was written, we can exit
		if (function_data->write_size == 0) {
			return true;
		}

		InputSerializationHeader* header = (InputSerializationHeader*)function_data->header.buffer;

		ECS_STACK_CAPACITY_STREAM(ECS_INPUT_SERIALIZE_TYPE, valid_types, ECS_INPUT_SERIALIZE_COUNT);
		for (size_t index = 0; index < ECS_INPUT_SERIALIZE_COUNT; index++) {
			valid_types[index] = (ECS_INPUT_SERIALIZE_TYPE)index;
		}
		valid_types.size = ECS_INPUT_SERIALIZE_COUNT;

		size_t initial_offset = function_data->read_instrument->GetOffset();
		size_t read_size = 0;
		while (read_size < function_data->write_size && valid_types.size > 0) {
			ECS_INPUT_SERIALIZE_TYPE serialized_type = deserialize_functor(*header, valid_types);
			if (serialized_type == ECS_INPUT_SERIALIZE_COUNT) {
				return false;
			}
			else {
				// Verify that an input of that type is still available
				unsigned int valid_type_index = valid_types.Find(serialized_type);
				if (valid_type_index == -1) {
					// An input is repeated, which means that the data is corrupted.
					return false;
				}
				valid_types.RemoveSwapBack(valid_type_index);

				if (serialized_type == ECS_INPUT_SERIALIZE_MOUSE) {
					data->previous_mouse = *data->mouse;
				}
				else if (serialized_type == ECS_INPUT_SERIALIZE_KEYBOARD) {
					data->previous_keyboard = *data->keyboard;
				}
				else {
					ECS_ASSERT(false);
				}

				size_t current_offset = function_data->read_instrument->GetOffset();
				read_size += current_offset - initial_offset;
				initial_offset = current_offset;
			}
		}

		return true;
	}

	static bool InputDeltaReaderDeltaFunction(DeltaStateReaderDeltaFunctionData* function_data) {
		DeltaStateReaderData* data = (DeltaStateReaderData*)function_data->user_data;
		return InputDeltaReaderFunctionImpl(function_data, [function_data, data](const InputSerializationHeader& header, Stream<ECS_INPUT_SERIALIZE_TYPE> accepted_input_types) {
			return DeserializeInputDelta(&data->previous_mouse, data->mouse, &data->previous_keyboard, data->keyboard, function_data->read_instrument, header, accepted_input_types);
		});
	}

	static bool InputDeltaReaderEntireFunction(DeltaStateReaderEntireFunctionData* function_data) {
		// This is a mirror of the delta function, but instead of the delta function, we use the entire function
		DeltaStateReaderData* data = (DeltaStateReaderData*)function_data->user_data;
		return InputDeltaReaderFunctionImpl(function_data, [function_data, data](const InputSerializationHeader& header, Stream<ECS_INPUT_SERIALIZE_TYPE> accepted_input_types) {
			return DeserializeInput(data->mouse, data->keyboard, function_data->read_instrument, header, accepted_input_types);
		});
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	static float InputDeltaWriterExtractFunction(void* user_data) {
		DeltaStateWriterWorldData* data = (DeltaStateWriterWorldData*)user_data;
		return data->world->elapsed_seconds;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void SetInputDeltaWriterInitializeInfo(DeltaStateWriterInitializeFunctorInfo& info, const Mouse* mouse, const Keyboard* keyboard, CapacityStream<void>& stack_memory) {
		info.delta_function = InputDeltaWriterDeltaFunction;
		info.entire_function = InputDeltaWriterEntireFunction;
		info.self_contained_extract = nullptr;
		info.user_data_allocator_initialize = InputDeltaWriterInitialize;
		info.user_data_allocator_deallocate = InputDeltaWriterDeallocate;

		InputSerializationHeader serialization_header = GetInputSerializeHeader();
		info.header = stack_memory.Add(&serialization_header);

		DeltaStateWriterData writer_data;
		memset(&writer_data, 0, sizeof(writer_data));
		writer_data.keyboard = keyboard;
		writer_data.mouse = mouse;
		info.user_data = stack_memory.Add(&writer_data);
	}

	void SetInputDeltaWriterWorldInitializeInfo(DeltaStateWriterInitializeFunctorInfo& info, const World* world, CapacityStream<void>& stack_memory) {
		// Call the base function, and override the user data and extract function
		SetInputDeltaWriterInitializeInfo(info, world->mouse, world->keyboard, stack_memory);
		info.self_contained_extract = InputDeltaWriterExtractFunction;

		DeltaStateWriterWorldData writer_data;
		ZeroOut(&writer_data);
		writer_data.world = world;
		writer_data.mouse = world->mouse;
		writer_data.keyboard = world->keyboard;
		info.user_data = stack_memory.Add(&writer_data);
	}

	void SetInputDeltaReaderInitializeInfo(DeltaStateReaderInitializeFunctorInfo& info, Mouse* mouse, Keyboard* keyboard, CapacityStream<void>& stack_memory) {
		info.delta_function = InputDeltaReaderDeltaFunction;
		info.entire_function = InputDeltaReaderEntireFunction;
		info.user_data_allocator_initialize = InputDeltaReaderInitialize;
		info.user_data_allocator_deallocate = InputDeltaReaderDeallocate;

		DeltaStateReaderData reader_data;
		ZeroOut(&reader_data);
		reader_data.mouse = mouse;
		reader_data.keyboard = keyboard;
		info.user_data = stack_memory.Add(&reader_data);
	}

	void SetInputDeltaReaderWorldInitializeInfo(DeltaStateReaderInitializeFunctorInfo& info, World* world, CapacityStream<void>& stack_memory) {
		// Here, we can simply forward the parameters
		SetInputDeltaReaderInitializeInfo(info, world->mouse, world->keyboard, stack_memory);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

}