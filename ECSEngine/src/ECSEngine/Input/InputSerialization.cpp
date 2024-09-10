#include "ecspch.h"
#include "InputSerialization.h"
#include "../Utilities/Serialization/SerializationHelpers.h"
#include "../Utilities/Serialization/SerializeIntVariableLength.h"
#include "../Utilities/Utilities.h"
#include "../Utilities/InMemoryReaderWriter.h"

#define VERSION 0

#define MAX_KEYBOARD_KEY_DELTA_COUNT (ECS_KEY_COUNT / 2)
#define MAX_KEYBOARD_CHARACTER_QUEUE_DELTA_COUNT 32
#define MAX_KEYBOARD_CHARACTER_QUEUE_ENTIRE_COUNT 64
#define MAX_KEYBOARD_ALPHANUMERIC_DELTA_COUNT 32
#define MAX_KEYBOARD_ALPHANUMERIC_ENTIRE_COUNT 64

#define MOUSE_DELTA_X_BIT ECS_BIT(5)
#define MOUSE_DELTA_Y_BIT ECS_BIT(6)
#define MOUSE_DELTA_SCROLL_BIT ECS_BIT(7)

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

	// -----------------------------------------------------------------------------------------------------------------------------

	unsigned char SerializeInputVersion() {
		return VERSION;
	}

	size_t SerializeMouseInputDeltaMaxSize() {
		return sizeof(float) + 1 + SerializeIntVariableLengthMaxSize<int>() * 3;
	}

	bool SerializeMouseInputDelta(const Mouse* previous_state, const Mouse* current_state, uintptr_t& buffer, size_t& buffer_capacity, float elapsed_seconds) {
		// Determine if there is any difference at all between the two states. If there is not, we can simply skip the entry all together.
		int x_delta = current_state->GetPosition().x - previous_state->GetPosition().x;
		int y_delta = current_state->GetPosition().y - previous_state->GetPosition().y;
		int scroll_delta = current_state->GetScrollValue() - previous_state->GetScrollValue();

		unsigned char previous_button_states = GetMouseButtonStates(previous_state);
		unsigned char current_button_states = GetMouseButtonStates(current_state);
		
		if (x_delta != 0 || y_delta != 0 || scroll_delta != 0 || previous_button_states != current_button_states) {
			// One of the values changed
			InMemoryWriteInstrument write_instrument = { buffer, buffer_capacity };
			
			// There are 3 bits that in the button states that can be used to indicate whether or not the deltas are 0 or not
			current_button_states |= x_delta != 0 ? MOUSE_DELTA_X_BIT : 0;
			current_button_states |= y_delta != 0 ? MOUSE_DELTA_Y_BIT : 0;
			current_button_states |= scroll_delta != 0 ? MOUSE_DELTA_SCROLL_BIT : 0;

			// Write the elapsed seconds first, then the button states (with the additional delta bits) followed by the variable length deltas.
			if (!write_instrument.Write(&elapsed_seconds)) {
				return false;
			}
			if (!write_instrument.Write(&current_button_states)) {
				return false;
			}

			if (x_delta != 0) {
				if (!SerializeIntVariableLengthBool(&write_instrument, x_delta)) {
					return false;
				}
			}
			if (y_delta != 0) {
				if (!SerializeIntVariableLengthBool(&write_instrument, y_delta)) {
					return false;
				}
			}
			if (scroll_delta != 0) {
				if (!SerializeIntVariableLengthBool(&write_instrument, scroll_delta)) {
					return false;
				}
			}
		}
		return true;
	}

	size_t SerializeMouseInputMaxSize() {
		return sizeof(float) + sizeof(unsigned short) + SerializeIntVariableLengthMaxSize<int>() * 3;
	}

	bool SerializeMouseInput(const Mouse* state, uintptr_t& buffer, size_t& buffer_capacity, float elapsed_seconds)
	{
		InMemoryWriteInstrument write_instrument = { buffer, buffer_capacity };

		// Write the elapsed duration first, followed by the button states and the variable length integers at last
		if (!write_instrument.Write(&elapsed_seconds)) {
			return false;
		}

		unsigned short button_states;
		WriteBits(&button_states, 2, ECS_MOUSE_BUTTON_COUNT, [state](size_t index, void* value) {
			unsigned char* byte = (unsigned char*)value;
			*byte = state->m_states[index];
		});
		if (!write_instrument.Write(&button_states)) {
			return false;
		}

		return SerializeIntVariableLengthBoolMultiple(&write_instrument, state->GetPosition().x, state->GetPosition().y, state->GetScrollValue());
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	size_t SerializeKeyboardInputDeltaMaxSize() {
		// Even tho the number can be much larger, use a smaller realistic value,
		// Since changing the state of many keys is not a normal occurence. There is also the
		// Character queue that we must take into consideration, for that use a conservative
		// Estimation of actual usage.
		return sizeof(float) + sizeof(unsigned char) + sizeof(ECS_KEY) * MAX_KEYBOARD_KEY_DELTA_COUNT // The key states
			+ SerializeIntVariableLengthMaxSize<int>() // Pushed character count
			+ sizeof(unsigned char) + sizeof(unsigned char) * MAX_KEYBOARD_CHARACTER_QUEUE_DELTA_COUNT // The character queue
			+ sizeof(unsigned char) + sizeof(Keyboard::AlphanumericKey) * MAX_KEYBOARD_ALPHANUMERIC_DELTA_COUNT; // The alphanumeric entries
	}

	bool SerializeKeyboardInputDelta(const Keyboard* previous_state, const Keyboard* current_state, uintptr_t& buffer, size_t& buffer_capacity, float elapsed_seconds)
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
			InMemoryWriteInstrument write_instrument = { buffer, buffer_capacity };

			if (!write_instrument.Write(&elapsed_seconds)) {
				return false;
			}

			// Write a compressed mask that indicates what deltas there are such that we can skip those that are not needed
			unsigned char difference_bit_mask = 0;
			difference_bit_mask |= is_process_queue_different ? KEYBOARD_DELTA_PROCESS_CHARACTERS_BIT : 0;
			difference_bit_mask |= is_pushed_character_count_different ? KEYBOARD_DELTA_PUSHED_CHARACTER_COUNT_BIT : 0;
			difference_bit_mask |= is_queue_different ? KEYBOARD_DELTA_CHARACTER_QUEUE_BIT : 0;
			difference_bit_mask |= changed_keys.size > 0 ? KEYBOARD_DELTA_CHANGED_KEYS_BIT : 0;
			difference_bit_mask |= is_alphanumeric_different ? KEYBOARD_DELTA_ALPHANUMERIC_BIT : 0;
			if (!write_instrument.Write(&difference_bit_mask)) {
				return false;
			}

			// Continue with the individual values, except for the boolean m_process_characters, which is already transmitted through the bit mask
			if (is_pushed_character_count_different) {
				if (!SerializeIntVariableLengthBool(&write_instrument, (int)(current_state->m_pushed_character_count - previous_state->m_pushed_character_count))) {
					return false;
				}
			}

			if (is_queue_different) {
				ECS_ASSERT(current_state->m_character_queue.GetSize() <= MAX_KEYBOARD_CHARACTER_QUEUE_DELTA_COUNT);
				unsigned char queue_size = current_state->m_character_queue.GetSize();
				if (!write_instrument.Write(&queue_size)) {
					return false;
				}
				ECS_STACK_CAPACITY_STREAM(char, character_queue_array, UCHAR_MAX);
				uintptr_t character_queue_ptr = (uintptr_t)character_queue_array.buffer;
				current_state->m_character_queue.CopyTo(character_queue_ptr);
				if (!write_instrument.Write(character_queue_array.buffer, (size_t)queue_size * sizeof(char))) {
					return false;
				}
			}

			if (changed_keys.size > 0) {
				// The states are already compressed enough, since there will be a small number of them
				ECS_ASSERT(changed_keys.size <= MAX_KEYBOARD_KEY_DELTA_COUNT);
				if (!write_instrument.WriteWithSize<unsigned char>(changed_keys)) {
					return false;
				}
			}

			if (is_alphanumeric_different) {
				ECS_ASSERT(current_state->m_alphanumeric_keys.size <= MAX_KEYBOARD_ALPHANUMERIC_DELTA_COUNT);
				if (!write_instrument.WriteWithSize<unsigned char>(current_state->m_alphanumeric_keys)) {
					return false;
				}
			}
		}

		return true;
	}

	size_t SerializeKeyboardInputMaxSize() {
		return sizeof(float)
			+ sizeof(bool) // The process characters boolean
			+ ECS_KEY_COUNT / 8 * 2 + sizeof(unsigned char) // The key states, + 1 byte for padding
			+ SerializeIntVariableLengthMaxSize<unsigned int>() // Pushed character count
			+ sizeof(unsigned char) + sizeof(unsigned char) * MAX_KEYBOARD_CHARACTER_QUEUE_ENTIRE_COUNT // The character queue
			+ sizeof(unsigned char) + sizeof(Keyboard::AlphanumericKey) * MAX_KEYBOARD_ALPHANUMERIC_ENTIRE_COUNT; // The alphanumeric entries
	}

	bool SerializeKeyboardInput(const Keyboard* state, uintptr_t& buffer, size_t& buffer_capacity, float elapsed_seconds)
	{
		ECS_ASSERT(state->m_character_queue.GetSize() <= MAX_KEYBOARD_CHARACTER_QUEUE_ENTIRE_COUNT);
		ECS_ASSERT(state->m_alphanumeric_keys.size <= MAX_KEYBOARD_ALPHANUMERIC_ENTIRE_COUNT);

		InMemoryWriteInstrument write_instrument = { buffer, buffer_capacity };

		if (!write_instrument.Write(&elapsed_seconds)) {
			return false;
		}
		if (!write_instrument.Write(&state->m_process_characters)) {
			return false;
		}
		if (!SerializeIntVariableLengthBool(&write_instrument, state->m_pushed_character_count)) {
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(char, character_queue_storage, MAX_KEYBOARD_CHARACTER_QUEUE_ENTIRE_COUNT);
		character_queue_storage.size = state->m_character_queue.GetSize();
		ECS_ASSERT(character_queue_storage.size <= character_queue_storage.capacity, "Serializing keyboard failed because the character queue exceeded the capacity!");
		uintptr_t character_queue_storage_ptr = (uintptr_t)character_queue_storage.buffer;
		state->m_character_queue.CopyTo(character_queue_storage_ptr);
		if (!write_instrument.WriteWithSize<unsigned char>(character_queue_storage)) {
			return false;
		}

		ECS_ASSERT(state->m_alphanumeric_keys.size <= MAX_KEYBOARD_ALPHANUMERIC_ENTIRE_COUNT, "Serializing keyboard failed because the alphanumeric key size exceeded the maximum allowed!");
		if (!write_instrument.WriteWithSize<unsigned char>(state->m_alphanumeric_keys)) {
			return false;
		}

		// Pack the key states into a binary stream of 2 bits per entry
		// The storage here is much larger than needed, just to be safe
		ECS_STACK_CAPACITY_STREAM(char, key_state, ECS_KEY_COUNT);
		size_t key_state_byte_count = WriteBits(key_state.buffer, 2, ECS_KEY_COUNT, [state](size_t index, void* value) {
			unsigned char* byte_value = (unsigned char*)value;
			*byte_value = state->m_states[index];
		});
		if (!write_instrument.Write(key_state.buffer, key_state_byte_count)) {
			return false;
		}

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool DeserializeMouseInputDelta(const Mouse* previous_state, Mouse* current_state, uintptr_t& buffer, size_t& buffer_capacity, float& elapsed_seconds, unsigned char version) 
	{
		ECS_ASSERT_FORMAT(version == VERSION, "Deserializing mouse with unknown version {#}: expected {#}.", version, VERSION);
		current_state->UpdateFromOther(previous_state);

		InMemoryReadInstrument read_instrument = { buffer, buffer_capacity };

		if (!read_instrument.Read(&elapsed_seconds)) {
			return false;
		}

		// Read the combined bit mask
		unsigned char button_states = 0;
		if (!read_instrument.Read(&button_states)) {
			return false;
		}

		// Update the button states of current state based on the bit mask
		for (size_t index = 0; index < ECS_MOUSE_BUTTON_COUNT; index++) {
			if ((button_states & ECS_BIT(index)) != 0) {
				// Flip the state
				current_state->FlipButton((ECS_MOUSE_BUTTON)index);
			}
		}

		// Retrieve the upper 3 MSBs in order to determine if we have delta values
		if (HasFlag(button_states, MOUSE_DELTA_X_BIT)) {
			// There is a x delta
			int x_delta = 0;
			if (!DeserializeIntVariableLengthBool(&read_instrument, x_delta)) {
				return false;
			}
			current_state->AddDelta(x_delta, 0);
		}
		if (HasFlag(button_states, MOUSE_DELTA_Y_BIT)) {
			// There is a y delta
			int y_delta = 0;
			if (!DeserializeIntVariableLengthBool(&read_instrument, y_delta)) {
				return false;
			}
			current_state->AddDelta(0, y_delta);
		}
		if (HasFlag(button_states, MOUSE_DELTA_SCROLL_BIT)) {
			// There is a scroll delta
			int scroll_delta = 0;
			if (!DeserializeIntVariableLengthBool(&read_instrument, scroll_delta)) {
				return false;
			}
			current_state->SetCursorWheel(current_state->GetScrollValue() + scroll_delta);
		}

		return true;
	}

	bool DeserializeMouseInput(Mouse* state, uintptr_t& buffer, size_t& buffer_capacity, float& elapsed_seconds, unsigned char version)
	{
		ECS_ASSERT_FORMAT(version == VERSION, "Deserializing mouse with unknown version {#}: expected {#}.", version, VERSION);
		state->Reset();

		InMemoryReadInstrument read_instrument = { buffer, buffer_capacity };

		if (!read_instrument.Read(&elapsed_seconds)) {
			return false;
		}

		unsigned short button_states = 0;
		if (!read_instrument.Read(&button_states)) {
			return false;
		}
		ReadBits(&button_states, 2, ECS_MOUSE_BUTTON_COUNT, [state](size_t index, const void* value) {
			const ECS_BUTTON_STATE* button_value = (const ECS_BUTTON_STATE*)value;
			state->m_states[index] = *button_value;
		});

		int2 position;
		int scroll_value;
		if (!DeserializeIntVariableLengthBoolMultipleEnsureRange(&read_instrument, position.x, position.y, scroll_value)) {
			return false;
		}

		state->SetPosition(position.x, position.y);
		state->SetCursorWheel(scroll_value);

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool DeserializeKeyboardInputDelta(const Keyboard* previous_state, Keyboard* current_state, uintptr_t& buffer, size_t& buffer_capacity, float& elapsed_seconds, unsigned char version) 
	{
		ECS_ASSERT_FORMAT(version == VERSION, "Deserializing keyboard with unknown version {#}: expected {#}.", version, VERSION);
		current_state->UpdateFromOther(previous_state);

		InMemoryReadInstrument read_instrument = { buffer, buffer_capacity };

		if (!read_instrument.Read(&elapsed_seconds)) {
			return false;
		}

		unsigned char difference_bit_mask = 0;
		if (!read_instrument.Read(&difference_bit_mask)) {
			return false;
		}

		// Determine what deltas there are present
		
		if (HasFlag(difference_bit_mask, KEYBOARD_DELTA_PUSHED_CHARACTER_COUNT_BIT)) {
			// The pushed character count
			int character_count_delta = 0;
			if (!DeserializeIntVariableLengthBool(&read_instrument, character_count_delta)) {
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
			if (!read_instrument.ReadWithSize<unsigned char>(current_state->m_character_queue.GetQueue()->buffer, queue_size, MAX_KEYBOARD_CHARACTER_QUEUE_DELTA_COUNT)) {
				return false;
			}
			current_state->m_character_queue.m_queue.size = queue_size;
		}

		if (HasFlag(difference_bit_mask, KEYBOARD_DELTA_CHANGED_KEYS_BIT)) {
			ECS_STACK_CAPACITY_STREAM(ECS_KEY, changed_keys, MAX_KEYBOARD_KEY_DELTA_COUNT);
			if (!read_instrument.ReadWithSize<unsigned char>(changed_keys)) {
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
			if (!read_instrument.ReadWithSize<unsigned char>(current_state->m_alphanumeric_keys)) {
				return false;
			}
		}

		return true;
	}

	bool DeserializeKeyboardInput(Keyboard* state, uintptr_t& buffer, size_t& buffer_capacity, float& elapsed_seconds, unsigned char version)
	{
		ECS_ASSERT_FORMAT(version == VERSION, "Deserializing keyboard with unknown version {#}: expected {#}.", version, VERSION);
		state->Reset();

		InMemoryReadInstrument read_instrument = { buffer, buffer_capacity };

		if (!read_instrument.Read(&elapsed_seconds)) {
			return false;
		}
		if (!read_instrument.Read(&state->m_process_characters)) {
			return false;
		}

		if (!DeserializeIntVariableLengthBool(&read_instrument, state->m_pushed_character_count)) {
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(char, character_queue_storage, MAX_KEYBOARD_CHARACTER_QUEUE_ENTIRE_COUNT);
		if (!read_instrument.ReadWithSize<unsigned char>(character_queue_storage)) {
			return false;
		}
		state->m_character_queue.Reset();
		state->m_character_queue.PushRange(character_queue_storage);

		ECS_ASSERT(state->m_alphanumeric_keys.capacity >= MAX_KEYBOARD_CHARACTER_QUEUE_ENTIRE_COUNT, "Deserializing keyboard: expected larger alphanumeric buffer capacity.");
		if (!read_instrument.ReadWithSize<unsigned char>(state->m_alphanumeric_keys)) {
			return false;
		}

		// Pack the key states into a binary stream of 2 bits per entry
		// The storage here is much larger than needed, just to be safe
		ECS_STACK_CAPACITY_STREAM(char, key_state, ECS_KEY_COUNT);
		// 2 bits per entry, determine the number of bytes
		size_t key_state_byte_count = SlotsFor(ECS_KEY_COUNT * 2, 8);
		if (!read_instrument.Read(key_state.buffer, key_state_byte_count)) {
			return false;
		}

		ReadBits(key_state.buffer, 2, ECS_KEY_COUNT, [state](size_t index, const void* value) {
			const ECS_BUTTON_STATE* button_state = (const ECS_BUTTON_STATE*)value;
			state->m_states[index] = *button_state;
		});
		
		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool DeserializeInputDelta(
		const Mouse* previous_mouse,
		const Keyboard* previous_keyboard,
		Mouse* current_mouse,
		Keyboard* current_keyboard,
		uintptr_t& buffer,
		size_t& buffer_capacity,
		float& mouse_elapsed_seconds,
		float& keyboard_elapsed_seconds,
		unsigned char version
	)
	{
		if (!DeserializeMouseInputDelta(previous_mouse, current_mouse, buffer, buffer_capacity, mouse_elapsed_seconds, version)) {
			return false;
		}
		return DeserializeKeyboardInputDelta(previous_keyboard, current_keyboard, buffer, buffer_capacity, keyboard_elapsed_seconds, version);
	}

	bool DeserializeInput(
		Mouse* mouse_state,
		Keyboard* keyboard_state,
		uintptr_t& buffer, 
		size_t& buffer_capacity,
		float& mouse_elapsed_seconds,
		float& keyboard_elapsed_seconds,
		unsigned char version
	)
	{
		if (!DeserializeMouseInput(mouse_state, buffer, buffer_capacity, mouse_elapsed_seconds, version)) {
			return false;
		}
		return DeserializeKeyboardInput(keyboard_state, buffer, buffer_capacity, keyboard_elapsed_seconds, version);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

}