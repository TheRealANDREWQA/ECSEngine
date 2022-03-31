#include "ecspch.h"
#include "InputSerialization.h"

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------------------------

	// Pad the struct and only write the first 13 bytes
	struct SerializeMouse {
		int x_position;
		int y_position;
		int mouse_wheel;
		int left_button : 1;
		int right_button : 1;
		int middle_button : 1;
		int x1 : 1;
		int x2 : 1;
		int mode : 1;
		int padding0 : 2;
		int padding1 : 24;
	};

	void SerializeMouseInput(uintptr_t& buffer, const HID::MouseState* state)
	{
		SerializeMouse* serialize_mouse = (SerializeMouse*)buffer;
		serialize_mouse->left_button = state->state.leftButton;
		serialize_mouse->right_button = state->state.rightButton;
		serialize_mouse->middle_button = state->state.middleButton;
		serialize_mouse->mode = state->state.positionMode;
		serialize_mouse->x1 = state->state.xButton1;
		serialize_mouse->x2 = state->state.xButton2;
		serialize_mouse->x_position = state->Position().x;
		serialize_mouse->y_position = state->Position().y;
		serialize_mouse->mouse_wheel = state->MouseWheelScroll();

		// Update the value of the buffer
		buffer += 13;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void SerializeKeyboardInput(uintptr_t& buffer, const HID::KeyboardState* state)
	{
		// Blit the state directly, it is already compacted into a binary bit field
		memcpy((void*)buffer, state, sizeof(*state));
		buffer += sizeof(*state);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void SerializeInput(
		uintptr_t& buffer, 
		const HID::MouseState* mouse_state,
		const HID::KeyboardState* keyboard_state
	)
	{
		SerializeMouseInput(buffer, mouse_state);
		SerializeKeyboardInput(buffer, keyboard_state);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void DeserializeMouseInput(uintptr_t& buffer, HID::MouseState* new_state)
	{
		SerializeMouse* data = (SerializeMouse*)buffer;
		new_state->state.leftButton = data->left_button;
		new_state->state.rightButton = data->right_button;
		new_state->state.middleButton = data->middle_button;
		new_state->state.xButton1 = data->x1;
		new_state->state.xButton2 = data->x2;
		new_state->state.x = data->x_position;
		new_state->state.y = data->y_position;
		new_state->state.scrollWheelValue = data->mouse_wheel;
		new_state->state.positionMode = (DirectX::Mouse::Mode)data->mode;

		// Only the add the actual write part
		buffer += 13;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void DeserializeKeyboardInput(uintptr_t& buffer, HID::KeyboardState* keyboard_state)
	{
		memcpy(keyboard_state, (void*)buffer, sizeof(*keyboard_state));
		buffer += sizeof(*keyboard_state);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void DeserializeInput(
		uintptr_t& buffer, 
		HID::MouseState* mouse_state,
		HID::KeyboardState* keyboard_state
	)
	{
		DeserializeMouseInput(buffer, mouse_state);
		DeserializeKeyboardInput(buffer, keyboard_state);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	// -----------------------------------------------------------------------------------------------------------------------------

}