#include "ecspch.h"
#include "UIDrawerActions.h"
#include "UISystem.h"
#include "../../Utilities/Mouse.h"
#include "../../Utilities/Keyboard.h"
#include "../../Utilities/FunctionInterfaces.h"

namespace ECSEngine {

	namespace Tools {

#define EXPORT(function, integer) template ECSENGINE_API void function<integer>(ActionData*);

		ECS_CONTAINERS;

		// --------------------------------------------------------------------------------------------------------------
		
		void WindowHandlerInitializer(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			size_t window_index = *counts;
			UIDefaultWindowHandler* window_data = (UIDefaultWindowHandler*)_data;
			window_data->is_parameter_window_opened = false;
			window_data->scroll = system->m_mouse->GetScrollValue();
			window_data->scroll_factor = 1.0f;
			window_data->allow_zoom = true;
			void* allocation = system->m_memory->Allocate(system->m_descriptors.misc.window_handler_revert_command_count * sizeof(HandlerCommand));
			window_data->revert_commands = Stack<HandlerCommand>(allocation, system->m_descriptors.misc.window_handler_revert_command_count);
			window_data->last_frame = system->GetFrameIndex();
			window_data->commit_cursor = CursorType::Default;

			const char* window_name = (const char*)_additional_data;

			size_t current_window_name_size = strnlen_s(window_name, 256);
			if (current_window_name_size > 11) {
				char parent_window_name[256];
				memcpy(parent_window_name, window_name, current_window_name_size);
				parent_window_name[current_window_name_size - 11] = '\0';
				unsigned int window_index = system->GetWindowFromName(parent_window_name);
				if (window_index != 0xFFFFFFFF) {
					window_data->is_this_parameter_window = true;
				}
				else {
					window_data->is_this_parameter_window = false;
				}
			}
			else {
				window_data->is_this_parameter_window = false;
			}

			const char* string_identifier = "##VerticalSlider";
			size_t string_size = strlen(string_identifier);
			window_data->vertical_slider = system->FindWindowResource(window_index, string_identifier, string_size);
		}
		
		// --------------------------------------------------------------------------------------------------------------

		void AddWindowHandleCommand(
			UISystem* system,
			unsigned int window_index,
			Action action,
			void* data,
			size_t data_size,
			HandlerCommandType command_type
		) {
			auto handler_data = system->GetDefaultWindowHandlerData(window_index);

			void* allocation = system->m_memory->Allocate(data_size);
			memcpy(allocation, data, data_size);
			system->AddWindowMemoryResource(allocation, window_index);

			HandlerCommand command;
			command.type = command_type;
			command.time = std::chrono::high_resolution_clock::now();
			command.handler.action = action;
			command.handler.data = allocation;
			command.handler.data_size = data_size;

			auto deallocate_data = handler_data->PushRevertCommand(command);
			if (deallocate_data.handler.action != nullptr) {
				ActionData clean_up;
				clean_up.border_index = window_index;
				clean_up.buffers = nullptr;
				clean_up.counts = nullptr;
				clean_up.system = system;
				clean_up.data = deallocate_data.handler.data;
				deallocate_data.handler.action(&clean_up);
				system->RemoveWindowMemoryResource(window_index, deallocate_data.handler.data);
				system->m_memory->Deallocate(deallocate_data.handler.data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void SliderAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerSlider* slider = (UIDrawerSlider*)_data;

			float initial_value = slider->slider_position;
			if (mouse_tracker->LeftButton() == MBPRESSED) {
				slider->interpolate_value = true;
				SetCapture((HWND)system->m_application->GetOSWindowHandle());
			}
			else if (mouse_tracker->LeftButton() == MBRELEASED) {
				slider->interpolate_value = false;
				ReleaseCapture();
			}

			float2 previous_mouse = system->m_previous_mouse_position;
			float dimming_factor = function::Select(keyboard->IsKeyDown(HID::Key::LeftShift), 0.1f, 1.0f);

			if (slider->is_vertical) {
				if (mouse_delta.y > 0.0f) {
					if (previous_mouse.y >= slider->current_position.y) {
						slider->slider_position += mouse_delta.y / slider->current_scale.y * dimming_factor;
					}
				}
				else if (mouse_position.y <= slider->current_position.y + slider->current_scale.y) {
					slider->slider_position += mouse_delta.y / slider->current_scale.y * dimming_factor;
				}
			}
			else {
				if (mouse_delta.x > 0.0f) {
					if (previous_mouse.x >= slider->current_position.x) {
						slider->slider_position += mouse_delta.x / slider->current_scale.x * dimming_factor;
					}
				}
				else if (mouse_position.x <= slider->current_position.x + slider->current_scale.x) {
					slider->slider_position += mouse_delta.x / slider->current_scale.x * dimming_factor;
				}
			}
			slider->slider_position = function::Select(slider->slider_position < 0.0f, 0.0f, slider->slider_position);
			slider->slider_position = function::Select(slider->slider_position > 1.0f, 1.0f, slider->slider_position);

			slider->changed_value = initial_value != slider->slider_position;
		}

		// --------------------------------------------------------------------------------------------------------------

		void SliderBringToMouse(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerSliderBringToMouse* data = (UIDrawerSliderBringToMouse*)_data;

			float initial_position = data->slider->slider_position;
			if (mouse_tracker->LeftButton() == MBPRESSED) {
				data->start_point = std::chrono::high_resolution_clock::now();
				if (data->slider->is_vertical) {
					if (mouse_position.y < data->slider->current_position.y + data->slider->current_scale.y * data->slider->slider_position)
						data->position = mouse_position.y;
					else {
						data->position = mouse_position.y - data->slider_length;
					}
				}
				else {
					if (mouse_position.x < data->slider->current_position.x + data->slider->current_scale.x * data->slider->slider_position)
						data->position = mouse_position.x;
					else {
						data->position = mouse_position.x - data->slider_length;
					}
				}
				data->is_finished = false;
				data->slider->interpolate_value = true;
			}
			else if (mouse_tracker->LeftButton() == MBRELEASED) {
				data->slider->interpolate_value = false;
			}

			if (!data->is_finished) {
				if (data->slider->is_vertical) {
					if (data->position < data->slider->current_position.y + data->slider->current_scale.y * data->slider->slider_position) {
						size_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - data->start_point).count();
						if (duration > system->m_descriptors.misc.slider_bring_back_start) {
							data->slider->slider_position -= system->m_descriptors.misc.slider_bring_back_tick;
							data->is_finished = !(data->position < data->slider->current_position.y + data->slider->current_scale.y * data->slider->slider_position);
						}
					}
					else {
						size_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - data->start_point).count();
						if (duration > system->m_descriptors.misc.slider_bring_back_start) {
							data->slider->slider_position += system->m_descriptors.misc.slider_bring_back_tick;
							data->is_finished = (data->position < data->slider->current_position.y + data->slider->current_scale.y * data->slider->slider_position);
						}
					}
				}
				else {
					if (data->position < data->slider->current_position.x + data->slider->current_scale.x * data->slider->slider_position) {
						size_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - data->start_point).count();
						if (duration > system->m_descriptors.misc.slider_bring_back_start) {
							data->slider->slider_position -= system->m_descriptors.misc.slider_bring_back_tick;
							data->is_finished = !(data->position < data->slider->current_position.x + data->slider->current_scale.x * data->slider->slider_position);
						}
					}
					else {
						size_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - data->start_point).count();
						if (duration > system->m_descriptors.misc.slider_bring_back_start) {
							data->slider->slider_position += system->m_descriptors.misc.slider_bring_back_tick;
							data->is_finished = (data->position < data->slider->current_position.x + data->slider->current_scale.x * data->slider->slider_position);
						}
					}
				}
			}

			if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
				if (keyboard_tracker->IsKeyPressed(HID::Key::C)) {
					system->m_application->WriteTextToClipboard(data->slider->characters.buffer);
				}
				else if (keyboard_tracker->IsKeyPressed(HID::Key::V)) {
					data->slider->characters.size = system->m_application->CopyTextFromClipboard(data->slider->characters.buffer, data->slider->characters.capacity);
					data->slider->character_value = true;
					data->slider->changed_value = true;
				}
			}

			data->slider->changed_value = initial_position != data->slider->slider_position;
		}

		// --------------------------------------------------------------------------------------------------------------

		void SliderReturnToDefault(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerSlider* data = (UIDrawerSlider*)_data;
			if (mouse_tracker->RightButton()) {
				memcpy(data->value_to_change, data->default_value, data->value_byte_size);

				data->Callback(action_data);
			}

			if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
				if (keyboard_tracker->IsKeyPressed(HID::Key::C)) {
					system->m_application->WriteTextToClipboard(data->characters.buffer);
				}
				else if (keyboard_tracker->IsKeyPressed(HID::Key::V)) {
					data->characters.size = system->m_application->CopyTextFromClipboard(data->characters.buffer, data->characters.capacity);
					data->changed_value = true;
					data->character_value = true;
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void SliderReturnToDefaultMouseDraggable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerSliderReturnToDefaultMouseDraggable* data = (UIDrawerSliderReturnToDefaultMouseDraggable*)_data;
			action_data->data = &data->hoverable_data;
			DefaultHoverableAction(action_data);

			if (mouse_tracker->RightButton()) {
				memcpy(data->slider->value_to_change, data->slider->default_value, data->slider->value_byte_size);
				data->slider->changed_value = true;
			}

			if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
				if (keyboard_tracker->IsKeyPressed(HID::Key::C)) {
					system->m_application->WriteTextToClipboard(data->slider->characters.buffer);
				}
				else if (keyboard_tracker->IsKeyPressed(HID::Key::V)) {
					data->slider->characters.size = system->m_application->CopyTextFromClipboard(data->slider->characters.buffer, data->slider->characters.capacity);
					data->slider->character_value = true;
					data->slider->changed_value = true;
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void SliderMouseDraggable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerSlider** data = (UIDrawerSlider**)_data;
			UIDrawerSlider* slider = *data;

			float initial_position = slider->slider_position;
			float dimming_factor = 1.0f;
			if (keyboard->IsKeyDown(HID::Key::LeftShift)) {
				dimming_factor = 0.1f;
			}

			if (mouse_tracker->LeftButton() == MBPRESSED) {
				slider->interpolate_value = true;
			}
			else if (mouse_tracker->LeftButton() == MBRELEASED) {
				slider->interpolate_value = false;
			}

			if (slider->is_vertical) {
				slider->slider_position += mouse_delta.y / slider->current_scale.y * dimming_factor;
			}
			else {
				slider->slider_position += mouse_delta.x / slider->current_scale.x * dimming_factor;
			}
			slider->slider_position = function::Select(slider->slider_position < 0.0f, 0.0f, slider->slider_position);
			slider->slider_position = function::Select(slider->slider_position > 1.0f, 1.0f, slider->slider_position);

			slider->changed_value = initial_position != slider->slider_position;
		}

		// --------------------------------------------------------------------------------------------------------------

		void TextInputRevertAddText(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
				UIDrawerTextInputAddCommandInfo* data = (UIDrawerTextInputAddCommandInfo*)_data;
				UIDrawerTextInput* input = data->input;

				unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);

				unsigned char bytes[128];
				UIDrawerTextInputRemoveCommandInfo* command_info = (UIDrawerTextInputRemoveCommandInfo*)&bytes;

				unsigned int data_size = sizeof(UIDrawerTextInputRemoveCommandInfo);
				unsigned int* text_count = &command_info->text_count;
				unsigned int* text_position = &command_info->text_position;
				char* text = (char*)((uintptr_t)command_info + data_size);
				command_info->input = input;

				input->current_sprite_position = data->text_position + data->text_count;
				input->current_selection = data->text_position;
				input->Backspace(text_count, text_position, text);

				if (!input->is_currently_selected) {
					input->is_caret_display = false;
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void TextInputRevertRemoveText(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerTextInputRemoveCommandInfo* data = (UIDrawerTextInputRemoveCommandInfo*)_data;
			UIDrawerTextInput* input = data->input;
			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
				unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
				input->current_sprite_position = data->text_position;
				input->current_selection = input->current_sprite_position;
				char deleted_characters[256];
				unsigned int deleted_character_count = 0;
				unsigned int delete_position = 0;

				input->PasteCharacters((char*)((uintptr_t)data + sizeof(UIDrawerTextInputRemoveCommandInfo)), data->text_count, system, window_index, deleted_characters, &deleted_character_count, &delete_position);
				input->current_selection = data->text_position;

				if (!input->is_currently_selected) {
					input->is_caret_display = false;
				}
				if (data->deallocate_data) {
					system->m_memory->Deallocate(data->deallocate_buffer);
					system->RemoveWindowMemoryResource(window_index, data->deallocate_buffer);
				}
			}
			else {
				if (data->deallocate_data) {
					system->m_memory->Deallocate(data->deallocate_buffer);
					// border_index holds the window_index
					system->RemoveWindowMemoryResource(border_index, data->deallocate_buffer);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void TextInputRevertReplaceText(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerTextInputReplaceCommandInfo* data = (UIDrawerTextInputReplaceCommandInfo*)_data;
			UIDrawerTextInput* input = data->input;

			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
				unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
				input->current_sprite_position = data->text_position;
				input->current_selection = data->text_position + data->text_count;

				UIDrawerTextInputAddCommandInfo add_data;
				add_data.input = input;
				add_data.text_count = data->text_count;
				add_data.text_position = data->text_position;

				action_data->data = &add_data;
				TextInputRevertAddText(action_data);

				UIDrawerTextInputRemoveCommandInfo remove_data;
				remove_data.input = input;
				remove_data.text_count = data->replaced_text_count;
				remove_data.text_position = data->text_position;
				remove_data.deallocate_data = false;

				if (!input->is_currently_selected) {
					input->is_caret_display = false;
				}

				action_data->data = &remove_data;
				TextInputRevertRemoveText(action_data);
				input->current_selection = data->text_position;
				system->m_memory->Deallocate(data->deallocate_buffer);
				system->RemoveWindowMemoryResource(window_index, data->deallocate_buffer);
			}
			else {
				system->m_memory->Deallocate(data->deallocate_buffer);
				system->RemoveWindowMemoryResource(border_index, data->deallocate_buffer);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void TextInputClickable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerTextInput* input = (UIDrawerTextInput*)_data;

			if (!input->is_word_selection) {
				input->SetSpritePositionFromMouse(mouse_position);
			}
			if (mouse_tracker->LeftButton() == MBPRESSED) {
				size_t initial_duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - input->word_click_start).count();

				auto check_word_boundary = [&](unsigned int& word_start_index, unsigned int& word_end_index) {
					int64_t index = input->current_sprite_position;
					for (; index >= 0; index--) {
						if (input->text->buffer[index] == ' ') {
							word_start_index = index + 1;
							break;
						}
					}
					if (index < 0) {
						word_start_index = 0;
					}
					for (index = input->current_sprite_position; index < input->text->size; index++) {
						if (input->text->buffer[index] == ' ') {
							word_end_index = index;
							break;
						}
					}
					if (index == input->text->size) {
						word_end_index = input->text->size;
					}
					return word_start_index <= input->char_click_index && input->char_click_index <= word_end_index;
				};
				if (initial_duration > system->m_descriptors.misc.text_input_repeat_start_duration * 2) {
					input->word_click_count = 0;
					input->is_word_selection = false;
				}

				input->current_selection = input->GetSpritePositionFromMouse(mouse_position);
				input->current_sprite_position = input->current_selection;
				input->suppress_arrow_movement = true;
				if (input->word_click_count == 0) {
					if (input->text->buffer[input->current_sprite_position] != ' ') {
						input->word_click_count++;
						input->word_click_start = std::chrono::high_resolution_clock::now();
						input->char_click_index = input->current_sprite_position;
					}
					else {
						input->word_click_count = 0;
					}
				}
				else if (input->word_click_count == 1) {
					size_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - input->word_click_start).count();

					if (duration < system->m_descriptors.misc.text_input_repeat_start_duration) {
						unsigned int word_start_index = 0, word_end_index = 0;
						if (check_word_boundary(word_start_index, word_end_index)) {
							input->current_sprite_position = word_end_index;
							input->current_selection = word_start_index;
							input->is_word_selection = true;
						}
					}
					input->word_click_count++;
				}
				else if (input->word_click_count == 2) {
					size_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - input->word_click_start).count();

					if (duration < system->m_descriptors.misc.text_input_repeat_start_duration * 2) {
						unsigned int word_start_index = 0, word_end_index = 0;
						if (check_word_boundary(word_start_index, word_end_index)) {
							input->current_sprite_position = input->text->size;
							input->current_selection = 0;
							input->word_click_count = 0;
							input->is_word_selection = true;
						}
					}
					else {
						input->word_click_count = 1;
						input->char_click_index = input->current_sprite_position;
						input->word_click_start = std::chrono::high_resolution_clock::now();
					}
				}
			}

			unsigned int visible_sprites = input->GetVisibleSpriteCount(input->bound);
			input->current_sprite_position = function::Select(input->current_sprite_position < input->sprite_render_offset, input->sprite_render_offset, input->current_sprite_position);
			input->current_sprite_position = function::Select(input->current_sprite_position > input->sprite_render_offset + visible_sprites, input->sprite_render_offset + visible_sprites, input->current_sprite_position);

			auto right_action = [&]() {
				if (input->current_sprite_position < input->text->size) {
					input->current_sprite_position++;
					input->sprite_render_offset++;
				}
			};

			auto left_action = [&]() {
				if (input->sprite_render_offset > 0) {
					input->current_sprite_position--;
					input->sprite_render_offset--;
				}
			};

			bool is_action = false;
			if (mouse_position.x > position.x + scale.x) {
				if (input->repeat_key == HID::Key::None) {
					input->key_repeat_start = std::chrono::high_resolution_clock::now();
					input->repeat_key = HID::Key::Apps;
					is_action = true;
					input->repeat_key_count = 0;
					right_action();
				}
				else if (input->repeat_key == HID::Key::Apps) {
					size_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - input->key_repeat_start).count();
					is_action = true;
					if (duration > system->m_descriptors.misc.text_input_repeat_start_duration) {
						duration -= system->m_descriptors.misc.text_input_repeat_start_duration;
						if (duration / system->m_descriptors.misc.text_input_repeat_time > input->repeat_key_count) {
							right_action();
							input->repeat_key_count++;
						}
					}
				}
			}
			else if (mouse_position.x < position.x) {
				if (input->repeat_key == HID::Key::None) {
					input->key_repeat_start = std::chrono::high_resolution_clock::now();
					input->repeat_key = HID::Key::BrowserFavorites;
					is_action = true;
					input->repeat_key_count = 0;
					left_action();
				}
				else if (input->repeat_key == HID::Key::BrowserFavorites) {
					size_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - input->key_repeat_start).count();
					is_action = true;
					if (duration > system->m_descriptors.misc.text_input_repeat_start_duration) {
						duration -= system->m_descriptors.misc.text_input_repeat_start_duration;
						if (duration / system->m_descriptors.misc.text_input_repeat_time > input->repeat_key_count) {
							left_action();
							input->repeat_key_count++;
						}
					}
				}
			}

			if (!is_action) {
				input->repeat_key = HID::Key::None;
			}

			input->caret_start = std::chrono::high_resolution_clock::now();
			input->is_caret_display = true;

			if (mouse_tracker->LeftButton() == MBRELEASED) {
				input->suppress_arrow_movement = false;
				input->repeat_key = HID::Key::None;
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void TextInputHoverable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerTextInput* data = (UIDrawerTextInput*)_data;
			unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
			float2 region_offset = system->GetWindowRenderRegion(window_index);
			if (IsPointInRectangle(mouse_position, position, scale)) {
				system->m_focused_window_data.always_hoverable = true;
				UIDefaultWindowHandler* default_handler_data = system->GetDefaultWindowHandlerData(window_index);
				default_handler_data->ChangeCursor(CursorType::IBeam);

				/*if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
					if (keyboard_tracker->IsKeyPressed(HID::Key::C)) {
						system->m_application->WriteTextToClipboard(data->text->buffer);
					}
					else if (keyboard_tracker->IsKeyPressed(HID::Key::V)) {
						char characters[256];
						unsigned int count = system->m_application->CopyTextFromClipboard(characters, 256);
						count = function::Select(count > data->text->capacity, data->text->capacity, count);
						data->DeleteAllCharacters();
						data->InsertCharacters(characters, count, 0, system);
					}
				}*/
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorInputSliderCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerColorInputSliderCallback* data = (UIDrawerColorInputSliderCallback*)_data;
			if (data->is_rgb) {
				data->input->hsv = RGBToHSV(*data->input->rgb);
			}
			else if (data->is_hsv) {
				*data->input->rgb = HSVToRGB(data->input->hsv);
			}
			else {
				data->input->hsv.alpha = data->input->rgb->alpha;
			}

			data->input->Callback(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorInputHexCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerColorInput* input = (UIDrawerColorInput*)_data;
			unsigned char previous_alpha = input->hsv.alpha;
			*input->rgb = HexToRGB(input->hex_characters);
			input->hsv = RGBToHSV(*input->rgb);
			input->hsv.alpha = previous_alpha;
			input->rgb->alpha = previous_alpha;
			Color back_to_rgb = HSVToRGB(input->hsv);
			input->is_hex_input = true;

			input->Callback(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorInputDefaultColor(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			if (mouse_tracker->RightButton() == MBPRESSED) {
				UIDrawerColorInput* data = (UIDrawerColorInput*)_data;
				*data->rgb = data->default_color;
				data->hsv = RGBToHSV(data->default_color);

				action_data->additional_data_type = ActionAdditionalData::ReturnToDefault;
				action_data->data = data->callback.data;
				data->Callback(action_data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorInputPreviousRectangleClickableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerColorInput* data = (UIDrawerColorInput*)_data;
			data->hsv = RGBToHSV(data->color_picker_initial_color);
			*data->rgb = data->color_picker_initial_color;

			data->Callback(action_data);
			//RGBToHex(data->hex_characters, *data->rgb);
		}

		// --------------------------------------------------------------------------------------------------------------

		void ComboBoxLabelClickable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerComboBoxLabelClickable* data = (UIDrawerComboBoxLabelClickable*)_data;
			*data->box->active_label = data->index;
			data->box->is_opened = false;
			data->box->has_been_destroyed = true;

			if (data->box->callback != nullptr) {
				action_data->data = data->box->callback_data;
				data->box->callback(action_data);
			}

			system->DestroyWindowIfFound("ComboBoxOpened");
		}

		// --------------------------------------------------------------------------------------------------------------

		void BoolClickable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			bool* data = (bool*)_data;
			*data = !(*data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void BoolClickableWithPin(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerBoolClickableWithPinData* data = (UIDrawerBoolClickableWithPinData*)_data;
			unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);

			*data->pointer = !*data->pointer;
			if (data->is_horizontal) {
				system->m_windows[window_index].pin_horizontal_slider_count += data->pin_count;
			}
			if (data->is_vertical) {
				system->m_windows[window_index].pin_vertical_slider_count += data->pin_count;
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void HierarchySelectableClick(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerHierarchySelectableData* data = (UIDrawerHierarchySelectableData*)_data;
			data->hierarchy->selectable.selected_index = data->node_index;
			if (data->hierarchy->selectable.pointer != nullptr) {
				*data->hierarchy->selectable.pointer = data->node_index + data->hierarchy->selectable.offset;
			}
			data->hierarchy->multiple_hierarchy_data->active_hierarchy = data->hierarchy;

			if (data->hierarchy->selectable.callback != nullptr) {
				action_data->additional_data = data->hierarchy;
				action_data->data = data->hierarchy->selectable.callback_data;
				data->hierarchy->selectable.callback(action_data);
			}

			if (IsPointInRectangle(mouse_position, position, system->GetSquareScale(scale.y))) {
				action_data->data = &data->bool_data;
				BoolClickableWithPin(action_data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void HierarchyNodeDrag(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerHierarchyDragNode* data = (UIDrawerHierarchyDragNode*)_data;
			if (!data->has_been_cancelled) {
				if (mouse_tracker->LeftButton() == MBPRESSED) {
					data->selectable_data.hierarchy->selectable.selected_index = data->selectable_data.node_index;
					data->hierarchies_data->active_hierarchy = data->selectable_data.hierarchy;
					action_data->data = &data->selectable_data;
					HierarchySelectableClick(action_data);

					data->timer.SetNewStart();
				}
				else if (mouse_tracker->LeftButton() == MBHELD) {
					if (data->timer.GetDuration_ms() > system->m_descriptors.misc.hierarchy_drag_node_time) {
						size_t index = 0;

						bool has_selected_hierarchy_been_found = false;
						unsigned int selected_hierarchy_index = 0;
						unsigned int selected_node_children_count = 0;
						bool is_child_hierarchy = false;
						while (data->hierarchies_data->hierarchy_transforms.size > index && (data->hierarchies_data->hierarchy_transforms[index].position.y < mouse_position.y || is_child_hierarchy)) {
							if (!has_selected_hierarchy_been_found) {
								if (data->hierarchies_data->elements[index].hierarchy == data->selectable_data.hierarchy && data->hierarchies_data->elements[index].node_index == data->selectable_data.node_index) {
									selected_hierarchy_index = index;
									selected_node_children_count = data->hierarchies_data->elements[index].children_count;
									has_selected_hierarchy_been_found = true;
									is_child_hierarchy = data->hierarchies_data->elements[index].hierarchy != data->hierarchies_data->elements[index + 1].hierarchy;
								}
							}
							else {
								if (index <= selected_hierarchy_index + selected_node_children_count) {
									is_child_hierarchy = true;
								}
								else {
									is_child_hierarchy = false;
								}
							}
							index++;
						}

						float2 region_position = system->GetDockspaceRegionPosition(dockspace, border_index, dockspace_mask);
						float2 region_scale = system->GetDockspaceRegionScale(dockspace, border_index, dockspace_mask);

						index -= index == data->hierarchies_data->hierarchy_transforms.size;
						float2 rectangle_position = data->hierarchies_data->hierarchy_transforms[index].position;
						rectangle_position.y -= 0.5f * system->m_descriptors.misc.rectangle_hierarchy_drag_node_dimension;
						SetSolidColorRectangle(
							rectangle_position,
							{
								region_position.x + region_scale.x - rectangle_position.x - system->m_descriptors.window_layout.next_row_padding,
								system->m_descriptors.misc.rectangle_hierarchy_drag_node_dimension
							},
							system->m_descriptors.color_theme.hierarchy_drag_node_bar,
							buffers,
							counts,
							0
						);

						if (data->previous_index != index) {
							data->timer.SetMarker();
						}
						else if (data->timer.GetDurationSinceMarker_ms() > system->m_descriptors.misc.hierarchy_drag_node_hover_drop) {
							unsigned int element_index = index - (index == data->hierarchies_data->elements.size);
							element_index -= (element_index != 0);
							UIDrawerHierarchy* hovered_hierarchy = (UIDrawerHierarchy*)data->hierarchies_data->elements[element_index].hierarchy;
							// if it is not the current current node hovered and if it is not opened
							if ((hovered_hierarchy != data->selectable_data.hierarchy)
								|| (hovered_hierarchy == data->selectable_data.hierarchy && data->hierarchies_data->elements[element_index].node_index != data->selectable_data.node_index)
								&& !hovered_hierarchy->nodes[data->hierarchies_data->elements[element_index].node_index].state) {
								hovered_hierarchy->nodes[data->hierarchies_data->elements[element_index].node_index].state = true;
							}
						}
						data->previous_index = index;
					}
				}
				else if (mouse_tracker->LeftButton() == MBRELEASED) {
					if (data->timer.GetDuration_ms() > system->m_descriptors.misc.hierarchy_drag_node_time) {
						size_t index = 0;

						bool has_selected_hierarchy_been_found = false;
						unsigned int selected_hierarchy_index = 0;
						unsigned int selected_node_children_count = 0;
						bool is_child_hierarchy = false;
						while (index < data->hierarchies_data->elements.size && (data->hierarchies_data->hierarchy_transforms[index].position.y < mouse_position.y || is_child_hierarchy)) {
							if (!has_selected_hierarchy_been_found) {
								if (data->hierarchies_data->elements[index].hierarchy == data->selectable_data.hierarchy && data->hierarchies_data->elements[index].node_index == data->selectable_data.node_index) {
									selected_hierarchy_index = index;
									selected_node_children_count = data->hierarchies_data->elements[index].children_count;
									has_selected_hierarchy_been_found = true;
									is_child_hierarchy = data->hierarchies_data->elements[index].hierarchy != data->hierarchies_data->elements[index + 1].hierarchy;
								}
							}
							else {
								if (index <= selected_hierarchy_index + selected_node_children_count) {
									is_child_hierarchy = true;
								}
								else {
									is_child_hierarchy = false;
								}
							}
							index++;
						}

						unsigned int element_index = index - (index == data->hierarchies_data->elements.size);
						UIDrawerHierarchy* hovered_hierarchy = (UIDrawerHierarchy*)data->hierarchies_data->elements[element_index].hierarchy;

						UIDrawerHierarchyNode node = data->selectable_data.hierarchy->GetNode(data->selectable_data.node_index);
						hovered_hierarchy->AddNode(node, data->hierarchies_data->elements[element_index].node_index);

						data->hierarchies_data->active_hierarchy = hovered_hierarchy;
						bool is_bumped_up = hovered_hierarchy == data->selectable_data.hierarchy && data->hierarchies_data->elements[element_index].node_index <= data->selectable_data.node_index;
						data->selectable_data.hierarchy->RemoveNodeWithoutDeallocation(data->selectable_data.node_index + is_bumped_up);

						hovered_hierarchy->SetSelectedNode(
							data->hierarchies_data->elements[element_index].node_index - ((data->hierarchies_data->elements[element_index].node_index >= data->selectable_data.node_index + 1) && (hovered_hierarchy == data->selectable_data.hierarchy))
						);
					}
				}

				if (mouse_tracker->RightButton() == MBPRESSED) {
					data->has_been_cancelled = true;
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void GraphHoverable(ActionData* action_data) {
			constexpr size_t tool_tip_character_count = 128;

			UI_UNPACK_ACTION_DATA;

			UIDrawerGraphHoverableData* data = (UIDrawerGraphHoverableData*)_data;

			char tool_tip_characters[tool_tip_character_count];
			Stream<char> tool_tip_stream = Stream<char>(tool_tip_characters, 0);
			function::ConvertIntToCharsFormatted(tool_tip_stream, data->sample_index);
			tool_tip_stream.Add(':');
			tool_tip_stream.Add(' ');
			tool_tip_stream.Add('x');
			tool_tip_stream.Add(' ');
			function::ConvertFloatToChars(tool_tip_stream, data->first_sample_values.x);
			tool_tip_stream.Add(' ');
			tool_tip_stream.Add('y');
			tool_tip_stream.Add(' ');
			function::ConvertFloatToChars(tool_tip_stream, data->first_sample_values.y);
			tool_tip_stream.Add('\n');
			function::ConvertIntToCharsFormatted(tool_tip_stream, data->sample_index + 1);
			tool_tip_stream.Add(':');
			tool_tip_stream.Add(' ');
			tool_tip_stream.Add('x');
			tool_tip_stream.Add(' ');
			function::ConvertFloatToChars(tool_tip_stream, data->second_sample_values.x);
			tool_tip_stream.Add(' ');
			tool_tip_stream.Add('y');
			tool_tip_stream.Add(' ');
			function::ConvertFloatToChars(tool_tip_stream, data->second_sample_values.y);
			data->tool_tip_data.characters = tool_tip_characters;
			data->tool_tip_data.base.offset_scale = { false, false };
			data->tool_tip_data.base.offset = { mouse_position.x - position.x + system->m_descriptors.misc.graph_hover_offset.x, mouse_position.y - position.y + system->m_descriptors.misc.graph_hover_offset.y };

			action_data->data = &data->tool_tip_data;
			TextTooltipHoverable(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void HistogramHoverable(ActionData* action_data) {
			constexpr size_t temp_character_count = 128;
			UI_UNPACK_ACTION_DATA;

			UIDrawerHistogramHoverableData* data = (UIDrawerHistogramHoverableData*)_data;
			char stack_memory[temp_character_count];
			Stream<char> stack_stream = Stream<char>(stack_memory, 0);
			function::ConvertIntToCharsFormatted(stack_stream, data->sample_index);
			stack_stream.Add(':');
			stack_stream.Add(' ');
			stack_stream.Add(' ');
			stack_stream.Add(' ');
			stack_stream.Add(' ');
			function::ConvertFloatToChars(stack_stream, data->sample_value);

			data->tool_tip_data.characters = stack_memory;
			data->tool_tip_data.base.offset_scale = { false, false };
			data->tool_tip_data.base.offset = { mouse_position.x - position.x + system->m_descriptors.misc.histogram_hover_offset.x, mouse_position.y - position.y + system->m_descriptors.misc.histogram_hover_offset.y };

			action_data->data = &data->tool_tip_data;
			TextTooltipHoverable(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		constexpr float INPUT_DRAG_FACTOR = 25.0f;

		void DoubleInputDragValue(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerDoubleInputDragData* data = (UIDrawerDoubleInputDragData*)_data;
			if (mouse_tracker->LeftButton() == MBHELD) {
				double shift_value = function::Select(keyboard->IsKeyDown(HID::Key::LeftShift), 1.0 / 5.0, 1.0);
				double ctrl_value = function::Select(keyboard->IsKeyDown(HID::Key::LeftControl), 5.0, 1.0);
				double amount = (double)mouse_delta.x * (double)INPUT_DRAG_FACTOR * shift_value * ctrl_value;

				*data->number += amount;
				*data->number = function::Clamp(*data->number, data->min, data->max);

				if (amount != 0 && data->number_data.input->callback != nullptr) {
					// The text input must also be updated before it
					char number_characters[64];
					Stream<char> number_characters_stream(number_characters, 0);
					function::ConvertDoubleToChars(number_characters_stream, *data->number, 3);
					data->number_data.input->DeleteAllCharacters();
					data->number_data.input->InsertCharacters(number_characters, number_characters_stream.size, 0, system);

					action_data->data = data->number_data.input->callback_data;
					data->number_data.input->callback(action_data);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void FloatInputDragValue(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerFloatInputDragData* data = (UIDrawerFloatInputDragData*)_data;
			if (mouse_tracker->LeftButton() == MBHELD) {
				float shift_value = function::Select(keyboard->IsKeyDown(HID::Key::LeftShift), 1.0f / 5.0f, 1.0f);
				float ctrl_value = function::Select(keyboard->IsKeyDown(HID::Key::LeftControl), 5.0f, 1.0f);
				float amount = mouse_delta.x * INPUT_DRAG_FACTOR * shift_value * ctrl_value;

				*data->number += amount;
				*data->number = function::Clamp(*data->number, data->min, data->max);

				if (amount != 0.0f && data->number_data.input->callback != nullptr) {
					// The text input must also be updated before it
					char number_characters[64];
					Stream<char> number_characters_stream(number_characters, 0);
					function::ConvertFloatToChars(number_characters_stream, *data->number, 3);
					data->number_data.input->DeleteAllCharacters();
					data->number_data.input->InsertCharacters(number_characters, number_characters_stream.size, 0, system);

					action_data->data = data->number_data.input->callback_data;
					data->number_data.input->callback(action_data);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void SliderCopyPaste(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerSlider* slider = (UIDrawerSlider*)_data;
			if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
				if (keyboard_tracker->IsKeyPressed(HID::Key::C)) {
					system->m_application->WriteTextToClipboard(slider->characters.buffer);
				}
				else if (keyboard_tracker->IsKeyPressed(HID::Key::V)) {
					slider->characters.size = system->m_application->CopyTextFromClipboard(slider->characters.buffer, slider->characters.capacity);
					slider->character_value = true;
					slider->changed_value = true;
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void IntegerInputDragValue(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerIntInputDragData<Integer>* data = (UIDrawerIntInputDragData<Integer>*)_data;
			if (mouse_tracker->LeftButton() == MBPRESSED) {
				data->last_position = mouse_position.x;
			}
			else if (mouse_tracker->LeftButton() == MBHELD) {
				float delta_to_position = mouse_position.x - data->last_position;
				float shift_value = function::Select(keyboard->IsKeyDown(HID::Key::LeftShift), 1.0f / 5.0f, 1.0f);
				float ctrl_value = function::Select(keyboard->IsKeyDown(HID::Key::LeftControl), 5.0f, 1.0f);

				float amount = delta_to_position * INPUT_DRAG_FACTOR * shift_value * ctrl_value;
				bool is_negative = amount < 0.0f;
				Integer value_before = *data->data.number;
				
				if (abs(amount) > 1.0f) {
					*data->data.number += (Integer)amount;
					if (is_negative) {
						if (*data->data.number > value_before) {
							*data->data.number = value_before;
						}
					}
					else {
						if (*data->data.number < value_before) {
							*data->data.number = value_before;
						}
					}
					*data->data.number = function::Clamp(*data->data.number, data->data.min, data->data.max);
					data->last_position = mouse_position.x;

					if (data->data.number_data.input->callback != nullptr) {
						// The text input must also be updated before it
						char number_characters[64];
						Stream<char> number_characters_stream(number_characters, 0);
						function::ConvertIntToChars(number_characters_stream, *data->data.number);
						data->data.number_data.input->DeleteAllCharacters();
						data->data.number_data.input->InsertCharacters(number_characters, number_characters_stream.size, 0, system);

						action_data->data = data->data.number_data.input->callback_data;
						data->data.number_data.input->callback(action_data);
					}
				}
			}
		}

#define EXPORT_THIS(integer) EXPORT(IntegerInputDragValue, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT_THIS);

#undef EXPORT_THIS

		// --------------------------------------------------------------------------------------------------------------

		// if a different submenu is being hovered, delay the destruction so all windows can be rendered during that frame
		void MenuCleanupSystemHandler(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerMenuCleanupSystemHandlerData* data = (UIDrawerMenuCleanupSystemHandlerData*)_data;
			for (int64_t index = 0; index < data->window_count; index++) {
				system->DestroyWindowIfFound(data->window_names[index]);
			}
			system->PopSystemHandler();
		}

		// --------------------------------------------------------------------------------------------------------------

		void RightClickMenuReleaseResource(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerRightClickMenuSystemHandlerData* data = (UIDrawerRightClickMenuSystemHandlerData*)_data;

			if (system->GetWindowFromName(data->menu_window_name) == -1) {
				unsigned int window_index = system->GetWindowFromName(data->parent_window_name);
				ECS_ASSERT(window_index != -1);
				void* resource = system->FindWindowResource(window_index, data->menu_resource_name, strlen(data->menu_resource_name));
				UIDrawerMenu* menu = (UIDrawerMenu*)resource;
				
				system->RemoveWindowMemoryResource(window_index, menu->name);
				system->RemoveWindowMemoryResource(window_index, resource);
				system->RemoveWindowMemoryResource(window_index, menu->windows.buffer);
				
				system->m_memory->Deallocate(resource);
				system->m_memory->Deallocate(menu->name);
				system->m_memory->Deallocate(menu->windows.buffer);

				// Removing the menu drawer element and the menu from the window table
				ResourceIdentifier identifier(data->menu_resource_name, strlen(data->menu_resource_name));

				unsigned int index = system->m_windows[window_index].table.Find(identifier);
				ECS_ASSERT(index != -1);

				// Removing the persistent identifier from the table
				const ResourceIdentifier* identifiers = system->m_windows[window_index].table.GetIdentifiers();
				system->RemoveWindowMemoryResource(window_index, identifiers[index].ptr);
				system->m_memory->Deallocate(identifiers[index].ptr);
				system->m_windows[window_index].table.EraseFromIndex(index);

				char temp_characters[512];
				strcpy(temp_characters, data->menu_resource_name);
				strcat(temp_characters, "##Separate");
				identifier.ptr = (const wchar_t*)temp_characters;
				identifier.size += std::size("##Separate") - 1;

				index = system->m_windows[window_index].table.Find(identifier);
				ECS_ASSERT(index != -1);
				system->RemoveWindowMemoryResource(window_index, identifiers[index].ptr);
				system->m_memory->Deallocate(identifiers[index].ptr);
				system->m_windows[window_index].table.EraseFromIndex(index);

				system->PopSystemHandler();
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void FloatInputCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerFloatInputCallbackData* data = (UIDrawerFloatInputCallbackData*)_data;
			if (data->number_data.input->text->size == 0) {
				data->number_data.input->InsertCharacters("0", 1, 0, system);
			}
			*data->number = function::ConvertCharactersToFloat(*data->number_data.input->text);
			float before_value = *data->number;
			*data->number = function::Clamp(*data->number, data->min, data->max);
			if (before_value != *data->number) {
				data->number_data.input->DeleteAllCharacters();
				char temp_characters[128];
				Stream characters = Stream<char>(temp_characters, 0);
				function::ConvertFloatToChars(characters, *data->number, 3);
				data->number_data.input->InsertCharacters(temp_characters, characters.size, 0, system);
			}

			if (data->number_data.user_action != nullptr) {
				action_data->data = data->number_data.user_action_data;
				data->number_data.user_action(action_data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void DoubleInputCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerDoubleInputCallbackData* data = (UIDrawerDoubleInputCallbackData*)_data;
			if (data->number_data.input->text->size == 0) {
				data->number_data.input->InsertCharacters("0", 1, 0, system);
			}
			*data->number = function::ConvertCharactersToDouble(*data->number_data.input->text);
			double before_value = *data->number;
			*data->number = function::Clamp(*data->number, data->min, data->max);
			if (before_value != *data->number) {
				data->number_data.input->DeleteAllCharacters();
				char temp_characters[128];
				Stream characters = Stream<char>(temp_characters, 0);
				function::ConvertDoubleToChars(characters, *data->number, 3);
				data->number_data.input->InsertCharacters(temp_characters, characters.size, 0, system);
			}

			if (data->number_data.user_action != nullptr) {
				action_data->data = data->number_data.user_action_data;
				data->number_data.user_action(action_data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void IntegerInputCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerIntegerInputCallbackData<Integer>* data = (UIDrawerIntegerInputCallbackData<Integer>*)_data;
			if (data->number_data.input->text->size == 0) {
				data->number_data.input->InsertCharacters("0", 1, 0, system);
			}

			auto revert_characters = [data, system]() {
				data->number_data.input->DeleteAllCharacters();
				char temp_characters[128];
				Stream characters = Stream<char>(temp_characters, 0);
				function::ConvertIntToCharsFormatted(characters, static_cast<int64_t>(*data->number));
				data->number_data.input->InsertCharacters(temp_characters, characters.size, 0, system);
			};

			int64_t number = function::ConvertCharactersToInt<int64_t>(*data->number_data.input->text);
			if (number > (int64_t)data->max || number < (int64_t)data->min) {
				revert_characters();
			}
			else {
				*data->number = number;
				Integer before_value = *data->number;
				*data->number = function::Clamp(*data->number, data->min, data->max);
				if (before_value != *data->number) {
					revert_characters();
				}
			}

			if (data->number_data.user_action != nullptr) {
				action_data->data = data->number_data.user_action_data;
				data->number_data.user_action(action_data);
			}
		}

#define EXPORT_THIS(integer) EXPORT(IntegerInputCallback, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT_THIS);

#undef EXPORT_THIS

		// --------------------------------------------------------------------------------------------------------------

		void FloatInputHoverable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerFloatInputHoverableData* data = (UIDrawerFloatInputHoverableData*)_data;
			if (data->data->number_data.display_range) {
				action_data->data = &data->tool_tip;
				action_data->position = mouse_position;
				TextTooltipHoverable(action_data);
			}

			if (mouse_tracker->RightButton() == MBPRESSED && data->data->number_data.return_to_default) {
				*data->data->number = data->data->default_value;
				data->data->number_data.input->DeleteAllCharacters();
				char temp_chars[128];
				Stream<char> temp_stream = Stream<char>(temp_chars, 0);
				function::ConvertFloatToChars(temp_stream, *data->data->number, 3);
				data->data->number_data.input->InsertCharacters(temp_stream.buffer, temp_stream.size, 0, system);

				// Call the input callback if any
				if (data->data->number_data.input->callback != nullptr) {
					action_data->data = data->data->number_data.input->callback_data;
					data->data->number_data.input->callback(action_data);
				}
			}
		}

		void FloatInputNoNameHoverable(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerFloatInputHoverableData* data = (UIDrawerFloatInputHoverableData*)_data;
			action_data->data = data->data->number_data.input;
			TextInputHoverable(action_data);
			action_data->data = data;
			FloatInputHoverable(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void DoubleInputHoverable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerDoubleInputHoverableData* data = (UIDrawerDoubleInputHoverableData*)_data;
			if (data->data->number_data.display_range) {
				action_data->data = &data->tool_tip;
				action_data->position = mouse_position;
				TextTooltipHoverable(action_data);
			}

			if (mouse_tracker->RightButton() == MBPRESSED && data->data->number_data.return_to_default) {
				*data->data->number = data->data->default_value;
				data->data->number_data.input->DeleteAllCharacters();
				char temp_chars[128];
				Stream<char> temp_stream = Stream<char>(temp_chars, 0);
				function::ConvertDoubleToChars(temp_stream, *data->data->number, 3);
				data->data->number_data.input->InsertCharacters(temp_stream.buffer, temp_stream.size, 0, system);

				// Call the input callback if any
				if (data->data->number_data.input->callback != nullptr) {
					action_data->data = data->data->number_data.input->callback_data;
					data->data->number_data.input->callback(action_data);
				}
			}
		}

		void DoubleInputNoNameHoverable(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerDoubleInputHoverableData* data = (UIDrawerDoubleInputHoverableData*)_data;
			action_data->data = data->data->number_data.input;
			TextInputHoverable(action_data);
			action_data->data = data;
			DoubleInputHoverable(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void IntInputHoverable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerIntInputHoverableData<Integer>* data = (UIDrawerIntInputHoverableData<Integer>*)_data;
			if (data->data->number_data.display_range) {
				action_data->data = &data->tool_tip;
				action_data->position = mouse_position;
				TextTooltipHoverable(action_data);
			}

			if (mouse_tracker->RightButton() == MBPRESSED && data->data->number_data.return_to_default) {
				*data->data->number = data->data->default_value;
				data->data->number_data.input->DeleteAllCharacters();
				char temp_chars[128];
				Stream<char> temp_stream = Stream<char>(temp_chars, 0);
				function::ConvertIntToCharsFormatted(temp_stream, static_cast<int64_t>(*data->data->number));
				data->data->number_data.input->InsertCharacters(temp_stream.buffer, temp_stream.size, 0, system);

				// Call the input callback if any
				if (data->data->number_data.input->callback != nullptr) {
					action_data->data = data->data->number_data.input->callback_data;
					data->data->number_data.input->callback(action_data);
				}
			}
		}

#define EXPORT_THIS(integer) EXPORT(IntInputHoverable, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT_THIS);

#undef EXPORT_THIS

		template<typename Integer>
		void IntInputNoNameHoverable(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerIntInputHoverableData<Integer>* data = (UIDrawerIntInputHoverableData<Integer>*)_data;
			action_data->data = data->data->number_data.input;
			TextInputHoverable(action_data);
			action_data->data = data;
			IntInputHoverable<Integer>(action_data);
		}

#define EXPORT_THIS(integer) EXPORT(IntInputNoNameHoverable, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT_THIS)

#undef EXPORT_THIS

		// --------------------------------------------------------------------------------------------------------------

		void WindowParameterColorInputThemeCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIWindowDrawerDescriptor* descriptor = (UIWindowDrawerDescriptor*)_data;
			descriptor->color_theme.SetNewTheme(descriptor->color_theme.theme);
			descriptor->configured[(unsigned int)UIWindowDrawerDescriptorIndex::ColorTheme] = true;
		}

		// --------------------------------------------------------------------------------------------------------------

		void WindowParameterColorInputCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIWindowDrawerDescriptor* descriptor = (UIWindowDrawerDescriptor*)_data;
			descriptor->configured[(unsigned int)UIWindowDrawerDescriptorIndex::ColorTheme] = true;
		}

		// --------------------------------------------------------------------------------------------------------------

		void WindowParameterLayoutCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIWindowDrawerDescriptor* descriptor = (UIWindowDrawerDescriptor*)_data;
			descriptor->configured[(unsigned int)UIWindowDrawerDescriptorIndex::Layout] = true;
		}

		// --------------------------------------------------------------------------------------------------------------

		void WindowParameterElementDescriptorCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIWindowDrawerDescriptor* descriptor = (UIWindowDrawerDescriptor*)_data;
			descriptor->configured[(unsigned int)UIWindowDrawerDescriptorIndex::Element] = true;
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParameterColorInputThemeCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIColorThemeDescriptor* theme = &system->m_descriptors.color_theme;
			theme->SetNewTheme(theme->theme);
			system->FinalizeColorTheme();
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParameterColorThemeCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIColorThemeDescriptor* theme = &system->m_descriptors.color_theme;
			system->FinalizeColorTheme();
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParameterLayoutCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			system->FinalizeLayout();
		}

		// --------------------------------------------------------------------------------------------------------------

		void SystemParameterElementDescriptorCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			system->FinalizeElementDescriptor();
		}

		// --------------------------------------------------------------------------------------------------------------

		void ChangeStateAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIChangeStateData* data = (UIChangeStateData*)_data;
			if ((*data->state & data->flag) != 0) {
				*data->state &= (~data->flag);
			}
			else {
				*data->state |= data->flag;
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void MenuButtonAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerMenuButtonData* data = (UIDrawerMenuButtonData*)_data;
			unsigned int window_index = system->GetWindowFromName(data->descriptor.window_name);

			if (mouse_tracker->LeftButton() == MBPRESSED) {
				data->is_opened_when_pressed = window_index != -1;
			}
			else if (mouse_tracker->LeftButton() == MBRELEASED && IsPointInRectangle(mouse_position, position, scale)) {
				if (window_index != -1) {
					system->DestroyWindowIfFound(data->descriptor.window_name);
				}
				else if (!data->is_opened_when_pressed) {
					system->CreateWindowAndDockspace(data->descriptor, data->border_flags | UI_DOCKSPACE_FIXED
						| UI_DOCKSPACE_POP_UP_WINDOW | UI_DOCKSPACE_NO_DOCKING | UI_POP_UP_WINDOW_FIT_TO_CONTENT | UI_DOCKSPACE_BORDER_NOTHING);
					system->PopUpSystemHandler(data->descriptor.window_name, true);
				}
			}
		}

		void StateTableAllButtonAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerStateTableAllButtonData* data = (UIDrawerStateTableAllButtonData*)_data;

			if (data->single_pointer) {
				Stream<bool> stream = Stream<bool>(data->states.buffer, data->states.size);
				// blit the inverse bool value to every bool using memset
				memset(stream.buffer, !data->all_true, stream.size);
			}
			else {
				bool new_state = !data->all_true;
				for (size_t index = 0; index < data->states.size; index++) {
					*data->states[index] = new_state;
				}
			}
			if (data->notifier != nullptr) {
				*data->notifier = true;
			}
		}

		void StateTableBoolClickable(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerStateTableBoolClickable* data = (UIDrawerStateTableBoolClickable*)_data;
			*data->state = !*data->state;
			if (data->notifier != nullptr) {
				*data->notifier = true;
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void LabelHierarchySelectable(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerLabelHierarchy* data = (UIDrawerLabelHierarchy*)_data;

			if (mouse_tracker->LeftButton() == MBRELEASED && IsPointInRectangle(mouse_position, position, scale)) {
				data->active_label.Copy(data->selected_label_temporary);
				data->active_label[data->active_label.size] = '\0';

				if (data->selectable_callback != nullptr) {
					action_data->additional_data = data->active_label.buffer;
					action_data->data = data->selectable_callback_data;
					data->selectable_callback(action_data);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void LabelHierarchyChangeState(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			if (IsClickableTrigger(action_data)) {
				UIDrawerLabelHierarchyChangeStateData* data = (UIDrawerLabelHierarchyChangeStateData*)_data;
				ResourceIdentifier identifier(data->label.buffer, data->label.size);

				UIDrawerLabelHierarchyLabelData* node = data->hierarchy->label_states.GetValuePtr(identifier);
				node->state = !node->state;
				
				PinWindowVerticalSliderPosition(system, system->GetWindowIndexFromBorder(dockspace, border_index));

				action_data->data = data->hierarchy;
				LabelHierarchySelectable(action_data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void SetWindowVerticalSliderPosition(UISystem* system, unsigned int window_index, float value)
		{
			UIDefaultWindowHandler* handler_data = (UIDefaultWindowHandler*)system->m_windows[window_index].default_handler.data;
			UIDrawerSlider* slider = (UIDrawerSlider*)handler_data->vertical_slider;
			slider->slider_position = value;
			slider->interpolate_value = true;
			slider->changed_value = true;
		}

		// --------------------------------------------------------------------------------------------------------------

		void PinWindowVerticalSliderPosition(UISystem* system, unsigned int window_index) {
			system->m_windows[window_index].pin_vertical_slider_count++;
		}

		// --------------------------------------------------------------------------------------------------------------

		void PinWindowHorizontalSliderPosition(UISystem* system, unsigned int window_index) {
			system->m_windows[window_index].pin_horizontal_slider_count++;
		}

		// --------------------------------------------------------------------------------------------------------------

		void ArrayDragAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerArrayDragData* data = (UIDrawerArrayDragData*)_data;
			if (mouse_tracker->LeftButton() == MBRELEASED) {
				data->array_data->drag_is_released = true;
			}
			else {
				data->array_data->drag_current_position = mouse_position.y;
				data->array_data->drag_index = data->index;
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorFloatInputCallback(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerColorFloatInputCallbackData* data = (UIDrawerColorFloatInputCallbackData*)_data;
			*data->input->color_float = SDRColorToHDR(data->input->base_color, data->input->intensity);

			if (data->callback != nullptr) {
				action_data->data = data->callback_data;
				data->callback(action_data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorFloatInputIntensityCallback(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerColorFloatInput* data = (UIDrawerColorFloatInput*)_data;
			*data->color_float = SDRColorToHDR(data->base_color, data->intensity);
		
			UIDrawerColorFloatInputCallbackData* color_input_callback_data = (UIDrawerColorFloatInputCallbackData*)data->color_input->callback.data;
			if (color_input_callback_data->callback != nullptr) {
				action_data->data = color_input_callback_data->callback_data;
				color_input_callback_data->callback(action_data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		// --------------------------------------------------------------------------------------------------------------

		// --------------------------------------------------------------------------------------------------------------

#undef EXPORT

	}

}