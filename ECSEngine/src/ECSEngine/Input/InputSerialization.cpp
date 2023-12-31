#include "ecspch.h"
#include "InputSerialization.h"
#include "../Utilities/Serialization/SerializationHelpers.h"

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------------------------

	// Pad the struct and only write the first 14 bytes
	struct SerializeMouse {
		int x_position;
		int y_position;
		int mouse_wheel;
		int left_button : 2;
		int right_button : 2;
		int middle_button : 2;
		int x1 : 2;
		int x2 : 2;
		int padding0 : 6;
		int padding1 : 16;
	};

	struct SerializeKeyboard {
		ButtonInput<ECS_KEY, ECS_KEY_COUNT> button_state;
		bool process_characters;
		unsigned short queue_count;
	};

	void SerializeMouseInput(uintptr_t& buffer, const Mouse* state)
	{
		SerializeMouse* serialize_mouse = (SerializeMouse*)buffer;
		serialize_mouse->left_button = state->IsDown(ECS_MOUSE_LEFT);
		serialize_mouse->right_button = state->IsDown(ECS_MOUSE_RIGHT);
		serialize_mouse->middle_button = state->IsDown(ECS_MOUSE_MIDDLE);
		serialize_mouse->x1 = state->IsDown(ECS_MOUSE_X1);
		serialize_mouse->x2 = state->IsDown(ECS_MOUSE_X2);
		serialize_mouse->x_position = state->GetPosition().x;
		serialize_mouse->y_position = state->GetPosition().y;
		serialize_mouse->mouse_wheel = state->GetScrollValue();

		memcpy((void*)buffer, serialize_mouse, 14);

		// Update the value of the buffer
		buffer += 14;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void SerializeKeyboardInput(uintptr_t& buffer, const Keyboard* state)
	{
		// Blit the state, then the boolean flag and the queue count
		Write<true>(&buffer, state->m_states, sizeof(state->m_states));
		Write<true>(&buffer, &state->m_process_characters, sizeof(state->m_process_characters));
		unsigned short queue_size = (unsigned short)state->m_character_queue.GetSize();
		Write<true>(&buffer, &queue_size, sizeof(queue_size));
		state->m_character_queue.CopyTo(buffer);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void SerializeInput(
		uintptr_t& buffer, 
		const Mouse* mouse,
		const Keyboard* keyboard
	)
	{
		SerializeMouseInput(buffer, mouse);
		SerializeKeyboardInput(buffer, keyboard);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void DeserializeMouseInput(uintptr_t& buffer, Mouse* new_state)
	{
		SerializeMouse* data = (SerializeMouse*)buffer;
		new_state->SetButton(ECS_MOUSE_LEFT, (ECS_BUTTON_STATE)data->left_button);
		new_state->SetButton(ECS_MOUSE_RIGHT, (ECS_BUTTON_STATE)data->right_button);
		new_state->SetButton(ECS_MOUSE_MIDDLE, (ECS_BUTTON_STATE)data->middle_button);
		new_state->SetButton(ECS_MOUSE_X1, (ECS_BUTTON_STATE)data->x1);
		new_state->SetButton(ECS_MOUSE_X2, (ECS_BUTTON_STATE)data->x2);
		new_state->SetPosition(data->x_position, data->y_position);
		new_state->SetCursorWheel(data->mouse_wheel);
		
		// Only the add the actual write part
		buffer += 14;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void DeserializeKeyboardInput(uintptr_t& buffer, Keyboard* keyboard_state)
	{
		SerializeKeyboard* data = (SerializeKeyboard*)buffer;
		buffer += sizeof(*data);

		memcpy(keyboard_state->m_states, data->button_state.m_states, sizeof(keyboard_state->m_states));
		if (data->process_characters) {
			keyboard_state->CaptureCharacters();
		}
		else {
			keyboard_state->DoNotCaptureCharacters();
		}

		keyboard_state->m_character_queue.ResizeNoCopy(data->queue_count);
		memcpy(keyboard_state->m_character_queue.GetQueue()->buffer, (void*)buffer, sizeof(char) * data->queue_count);
		buffer += sizeof(char) * data->queue_count;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void DeserializeInput(
		uintptr_t& buffer, 
		Mouse* mouse_state,
		Keyboard* keyboard_state
	)
	{
		DeserializeMouseInput(buffer, mouse_state);
		DeserializeKeyboardInput(buffer, keyboard_state);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	// -----------------------------------------------------------------------------------------------------------------------------

}