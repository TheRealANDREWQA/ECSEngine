#include "ecspch.h"
#include "UIDrawerActions.h"
#include "UIDrawer.h"
#include "UIResourcePaths.h"
#include "../../Utilities/Mouse.h"
#include "../../Utilities/Keyboard.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "UIOSActions.h"
#include "UIDrawerWindows.h"

namespace ECSEngine {

	namespace Tools {

#define EXPORT(function, integer) template ECSENGINE_API void function<integer>(ActionData*);

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
			window_data->commit_cursor = ECS_CURSOR_TYPE::ECS_CURSOR_DEFAULT;

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
			ECS_UI_HANDLER_COMMAND_TYPE command_type
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
			float dimming_factor = keyboard->IsKeyDown(HID::Key::LeftShift) ? 0.1f : 1.0f;

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
			slider->slider_position = slider->slider_position < 0.0f ? 0.0f : slider->slider_position;
			slider->slider_position = slider->slider_position > 1.0f ? 1.0f : slider->slider_position;

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

			action_data->data = data->slider;
			SliderCopyPaste(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void SliderMouseDraggable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SliderMouseDraggableData* data = (SliderMouseDraggableData*)_data;
			UIDrawerSlider* slider = data->slider;

			float initial_position = slider->slider_position;
			float dimming_factor = 1.0f;
			if (keyboard->IsKeyDown(HID::Key::LeftShift)) {
				dimming_factor = 0.2f;
			}
			if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
				dimming_factor = 5.0f;
			}

			if (mouse_tracker->LeftButton() == MBPRESSED) {
				slider->interpolate_value = true;
			}
			else if (mouse_tracker->LeftButton() == MBRELEASED) {
				slider->interpolate_value = false;
			}

			float factor = slider->is_vertical ? mouse_delta.y : mouse_delta.x;
			if (data->interpolate_bounds) {
				slider->slider_position += factor / slider->current_scale.x * dimming_factor;

				slider->slider_position = slider->slider_position < 0.0f ? 0.0f : slider->slider_position;
				slider->slider_position = slider->slider_position > 1.0f ? 1.0f : slider->slider_position;
			}
			else {
				slider->slider_position += factor * dimming_factor * 150.0f;
			}

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
			input->current_sprite_position = input->current_sprite_position < input->sprite_render_offset ? input->sprite_render_offset : input->current_sprite_position;
			input->current_sprite_position = input->current_sprite_position > input->sprite_render_offset + visible_sprites ? input->sprite_render_offset + visible_sprites : input->current_sprite_position;

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
				default_handler_data->ChangeCursor(ECS_CURSOR_IBEAM);
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

			Color converted_color = HexToRGB(input->hex_characters);
			converted_color.alpha = input->hsv.alpha;
			if (converted_color != *input->rgb) {
				*input->rgb = converted_color;
				input->hsv = RGBToHSV(*input->rgb);
			}

			// For debug
			Color back_to_rgb = HSVToRGB(input->hsv);
			input->Callback(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorInputDefaultColor(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			if (mouse_tracker->RightButton() == MBPRESSED) {
				UIDrawerColorInput* data = (UIDrawerColorInput*)_data;
				*data->rgb = data->default_color;
				data->hsv = RGBToHSV(data->default_color);

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
		}

		// --------------------------------------------------------------------------------------------------------------

		void ComboBoxLabelClickable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerComboBoxLabelClickable* data = (UIDrawerComboBoxLabelClickable*)_data;
			*data->box->active_label = data->index;

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
			float dockspace_mask = GetDockspaceMaskFromType(dockspace_type);
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

		constexpr float INPUT_DRAG_FACTOR = 60.0f;

		void DoubleInputDragValue(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerDoubleInputDragData* data = (UIDrawerDoubleInputDragData*)_data;
			if (mouse_tracker->LeftButton() == MBHELD) {
				double shift_value = keyboard->IsKeyDown(HID::Key::LeftShift) ? 1.0 / 5.0 : 1.0;
				double ctrl_value = keyboard->IsKeyDown(HID::Key::LeftControl) ? 5.0 : 1.0;
				double amount = (double)mouse_delta.x * (double)INPUT_DRAG_FACTOR * shift_value * ctrl_value;

				*data->number += amount;
				*data->number = function::Clamp(*data->number, data->min, data->max);

				if (amount != 0 && data->number_data.input->HasCallback()) {
					// The text input must also be updated before it
					char number_characters[64];
					Stream<char> number_characters_stream(number_characters, 0);
					function::ConvertDoubleToChars(number_characters_stream, *data->number, 3);
					data->number_data.input->DeleteAllCharacters();
					data->number_data.input->InsertCharacters(number_characters, number_characters_stream.size, 0, system);

					//data->number_data.input->Callback(action_data);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void FloatInputDragValue(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerFloatInputDragData* data = (UIDrawerFloatInputDragData*)_data;
			if (mouse_tracker->LeftButton() == MBHELD) {
				float shift_value = keyboard->IsKeyDown(HID::Key::LeftShift) ? 1.0f / 5.0f : 1.0f;
				float ctrl_value = keyboard->IsKeyDown(HID::Key::LeftControl) ? 5.0f : 1.0f;
				float amount = mouse_delta.x * INPUT_DRAG_FACTOR * shift_value * ctrl_value;

				*data->number += amount;
				*data->number = function::Clamp(*data->number, data->min, data->max);

				if (amount != 0.0f && data->number_data.input->HasCallback()) {
					// The text input must also be updated before it
					char number_characters[64];
					Stream<char> number_characters_stream(number_characters, 0);
					function::ConvertFloatToChars(number_characters_stream, *data->number, 3);
					data->number_data.input->DeleteAllCharacters();
					data->number_data.input->InsertCharacters(number_characters, number_characters_stream.size, 0, system);

					//data->number_data.input->Callback(action_data);
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
				float shift_value = keyboard->IsKeyDown(HID::Key::LeftShift) ? 1.0f / 5.0f : 1.0f;
				float ctrl_value = keyboard->IsKeyDown(HID::Key::LeftControl) ? 5.0f : 1.0f;

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

					if (data->data.number_data.input->HasCallback()) {
						// The text input must also be updated before it
						char number_characters[64];
						Stream<char> number_characters_stream(number_characters, 0);
						function::ConvertIntToChars(number_characters_stream, *data->data.number);
						data->data.number_data.input->DeleteAllCharacters();
						data->data.number_data.input->InsertCharacters(number_characters, number_characters_stream.size, 0, system);

						//data->data.number_data.input->Callback(action_data);
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
			system->PopFrameHandler();
		}

		// --------------------------------------------------------------------------------------------------------------

		void RightClickMenuReleaseResource(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerRightClickMenuSystemHandlerData* data = (UIDrawerRightClickMenuSystemHandlerData*)_data;

			if (system->GetWindowFromName(data->menu_window_name) == -1) {
				unsigned int window_index = system->GetWindowFromName(data->parent_window_name);
				ECS_ASSERT(window_index != -1);
				void* resource = system->FindWindowResource(window_index, data->menu_resource_name);
				UIDrawerMenu* menu = (UIDrawerMenu*)resource;
				
				system->RemoveWindowMemoryResource(window_index, menu->name);
				system->RemoveWindowMemoryResource(window_index, resource);
				system->RemoveWindowMemoryResource(window_index, menu->windows.buffer);
				
				system->m_memory->Deallocate(resource);
				system->m_memory->Deallocate(menu->name);
				system->m_memory->Deallocate(menu->windows.buffer);

				// Removing the menu drawer element and the menu from the window table
				ResourceIdentifier identifier(data->menu_resource_name);

				unsigned int index = system->m_windows[window_index].table.Find(identifier);
				ECS_ASSERT(index != -1);

				// Removing the persistent identifier from the table
				identifier = system->m_windows[window_index].table.GetIdentifierFromIndex(index);
				system->RemoveWindowMemoryResource(window_index, identifier.ptr);
				system->m_memory->Deallocate(identifier.ptr);
				system->m_windows[window_index].table.EraseFromIndex(index);

				ECS_STACK_CAPACITY_STREAM(char, temp_characters, 512);
				temp_characters.Copy(data->menu_resource_name);
				temp_characters.AddStream("##Separate");
				identifier.ptr = temp_characters.buffer;
				identifier.size = temp_characters.size;

				index = system->m_windows[window_index].table.Find(identifier);
				ECS_ASSERT(index != -1);

				identifier = system->m_windows[window_index].table.GetIdentifierFromIndex(index);
				system->RemoveWindowMemoryResource(window_index, identifier.ptr);
				system->m_memory->Deallocate(identifier.ptr);
				system->m_windows[window_index].table.EraseFromIndex(index);

				system->RemoveFrameHandler(RightClickMenuReleaseResource, data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void FloatInputCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerFloatInputCallbackData* data = (UIDrawerFloatInputCallbackData*)_data;
			
			if (!data->number_data.input->is_currently_selected) {
				float number = 0;
				if (data->number_data.input->text->size > 0) {
					number = function::ConvertCharactersToFloat(*data->number_data.input->text);
				}

				double previous_number = number;
				number = function::Clamp<float>(number, data->min, data->max);
				if (previous_number != number || data->number_data.input->text->size == 0) {
					// Need to update the text input
					data->number_data.input->DeleteAllCharacters();
					ECS_STACK_CAPACITY_STREAM(char, new_characters, 128);
					function::ConvertFloatToChars(new_characters, number, 3);
					data->number_data.input->InsertCharacters(new_characters.buffer, new_characters.size, 0, system);
				}

				*data->number = number;

				if (data->number_data.user_action != nullptr) {
					action_data->data = data->number_data.user_action_data;
					data->number_data.user_action(action_data);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void DoubleInputCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerDoubleInputCallbackData* data = (UIDrawerDoubleInputCallbackData*)_data;

			if (!data->number_data.input->is_currently_selected) {
				double number = 0;
				if (data->number_data.input->text->size > 0) {
					number = function::ConvertCharactersToDouble(*data->number_data.input->text);
				}

				double previous_number = number;
				number = function::Clamp<double>(number, data->min, data->max);
				if (previous_number != number || data->number_data.input->text->size == 0) {
					// Need to update the text input
					data->number_data.input->DeleteAllCharacters();
					ECS_STACK_CAPACITY_STREAM(char, new_characters, 128);
					function::ConvertDoubleToChars(new_characters, number, 3);
					data->number_data.input->InsertCharacters(new_characters.buffer, new_characters.size, 0, system);
				}

				*data->number = number;

				if (data->number_data.user_action != nullptr) {
					action_data->data = data->number_data.user_action_data;
					data->number_data.user_action(action_data);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void IntegerInputCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerIntegerInputCallbackData<Integer>* data = (UIDrawerIntegerInputCallbackData<Integer>*)_data;

			if (!data->number_data.input->is_currently_selected) {
				Integer number = 0;
				if (data->number_data.input->text->size > 0) {
					number = function::ConvertCharactersToIntImpl<Integer, char>(*data->number_data.input->text);
				}

				Integer previous_number = number;
				number = function::Clamp<Integer>(number, data->min, data->max);
				if (previous_number != number || data->number_data.input->text->size == 0) {
					// Need to update the text input
					data->number_data.input->DeleteAllCharacters();
					ECS_STACK_CAPACITY_STREAM(char, new_characters, 128);
					function::ConvertIntToChars(new_characters, (int64_t)number);
					data->number_data.input->InsertCharacters(new_characters.buffer, new_characters.size, 0, system);
				}

				*data->number = number;

				if (data->number_data.user_action != nullptr) {
					action_data->data = data->number_data.user_action_data;
					data->number_data.user_action(action_data);
				}
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
				/*if (data->data->number_data.input->HasCallback()) {
					data->data->number_data.input->Callback(action_data);
				}*/
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
				/*if (data->data->number_data.input->HasCallback()) {
					data->data->number_data.input->Callback(action_data);
				}*/
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
				function::ConvertIntToChars(temp_stream, static_cast<int64_t>(*data->data->number));
				data->data->number_data.input->InsertCharacters(temp_stream.buffer, temp_stream.size, 0, system);

				// Call the input callback if any
				/*if (data->data->number_data.input->HasCallback()) {
					data->data->number_data.input->Callback(action_data);
				}*/
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
			descriptor->configured[(unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COLOR_THEME] = true;
		}

		// --------------------------------------------------------------------------------------------------------------

		void WindowParameterColorInputCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIWindowDrawerDescriptor* descriptor = (UIWindowDrawerDescriptor*)_data;
			descriptor->configured[(unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COLOR_THEME] = true;
		}

		// --------------------------------------------------------------------------------------------------------------

		void WindowParameterLayoutCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIWindowDrawerDescriptor* descriptor = (UIWindowDrawerDescriptor*)_data;
			descriptor->configured[(unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_LAYOUT] = true;
		}

		// --------------------------------------------------------------------------------------------------------------

		void WindowParameterElementDescriptorCallback(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIWindowDrawerDescriptor* descriptor = (UIWindowDrawerDescriptor*)_data;
			descriptor->configured[(unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_ELEMENT] = true;
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

		void ChangeAtomicStateAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIChangeAtomicStateData* data = (UIChangeAtomicStateData*)_data;
			if (function::HasFlagAtomic(*data->state, data->flag)) {
				function::ClearFlagAtomic(*data->state, data->flag);
			}
			else {
				function::SetFlagAtomic(*data->state, data->flag);
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
					system->PopUpFrameHandler(data->descriptor.window_name, true);
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

		void FilesystemHierarchySelectable(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerFilesystemHierarchy* data = (UIDrawerFilesystemHierarchy*)_data;

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

		void FilesystemHierarchyChangeState(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			if (IsClickableTrigger(action_data)) {
				UIDrawerFilesystemHierarchyChangeStateData* data = (UIDrawerFilesystemHierarchyChangeStateData*)_data;
				ResourceIdentifier identifier(data->label.buffer, data->label.size);

				UIDrawerFilesystemHierarchyLabelData* node = data->hierarchy->label_states.GetValuePtr(identifier);
				node->state = !node->state;
				
				PinWindowVerticalSliderPosition(system, system->GetWindowIndexFromBorder(dockspace, border_index));

				action_data->data = data->hierarchy;
				FilesystemHierarchySelectable(action_data);
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

		void TextInputAction(ActionData* action_data) {
			constexpr size_t revert_command_stack_size = 512;
			UI_UNPACK_ACTION_DATA;

			UIDrawerTextInputActionData* data = (UIDrawerTextInputActionData*)_data;
			UIDrawerTextInput* input = data->input;

			UIDrawerTextInputActionData* additional_data = (UIDrawerTextInputActionData*)_additional_data;
			system->m_focused_window_data.clean_up_call_general = true;

			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL && !keyboard_tracker->IsKeyPressed(HID::Key::Enter)) {
				keyboard->CaptureCharacters();
				unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
				input->is_currently_selected = true;

				auto left_arrow_lambda = [&]() {
					if (keyboard->IsKeyUp(HID::Key::LeftShift) && input->current_sprite_position > input->current_selection) {
						unsigned int difference = input->current_sprite_position - input->current_selection;
						input->current_sprite_position = input->current_selection;
						if (input->sprite_render_offset < difference) {
							input->sprite_render_offset = 0;
						}
						else {
							input->sprite_render_offset -= difference;
						}
					}
					else {
						if (input->current_sprite_position > 0) {
							if (input->current_sprite_position == input->sprite_render_offset && input->sprite_render_offset > 0) {
								input->sprite_render_offset--;
							}
							input->current_sprite_position--;
						}
						else {
							if (input->sprite_render_offset == 1)
								input->sprite_render_offset--;
						}
						if (keyboard->IsKeyUp(HID::Key::LeftShift)) {
							input->current_selection = input->current_sprite_position;
						}
					}
					input->caret_start = std::chrono::high_resolution_clock::now();
					input->is_caret_display = true;
				};
				auto right_arrow_lambda = [&]() {
					unsigned int visible_sprites = input->GetVisibleSpriteCount(input->bound);
					if (keyboard->IsKeyUp(HID::Key::LeftShift) && input->current_sprite_position < input->current_selection) {
						input->current_sprite_position = input->current_selection;
						unsigned int visible_sprites = input->GetVisibleSpriteCount(input->bound);
						while (input->sprite_render_offset + visible_sprites < input->current_sprite_position) {
							input->sprite_render_offset++;
							visible_sprites = input->GetVisibleSpriteCount(input->bound);
						}
					}
					else {
						if (input->current_sprite_position < input->text->size) {
							if (input->current_sprite_position == input->sprite_render_offset + visible_sprites - 1 && input->sprite_render_offset + visible_sprites < input->text->size) {
								input->sprite_render_offset++;
								visible_sprites = input->GetVisibleSpriteCount(input->bound);
								input->current_sprite_position = input->sprite_render_offset + visible_sprites - 1;
							}
							else {
								input->current_sprite_position++;
							}
						}
						if (keyboard->IsKeyUp(HID::Key::LeftShift)) {
							input->current_selection = input->current_sprite_position;
						}
					}
					input->caret_start = std::chrono::high_resolution_clock::now();
					input->is_caret_display = true;
				};

				unsigned char command[128];
				UIDrawerTextInputRemoveCommandInfo* command_info = (UIDrawerTextInputRemoveCommandInfo*)command;

				unsigned int data_size = sizeof(UIDrawerTextInputRemoveCommandInfo);
				unsigned int* backspace_text_count = &command_info->text_count;
				unsigned int* backspace_text_position = &command_info->text_position;
				char* info_text = (char*)((uintptr_t)command_info + data_size);
				char* backspace_text = info_text;
				command_info->input = input;

				bool is_backspace_lambda = false;
				auto backspace_lambda = [&]() {
					is_backspace_lambda = input->Backspace(backspace_text_count, backspace_text_position, backspace_text);
				};

				if (input->filter_character_count > 0) {
					size_t valid_characters = 0;

					float push_offsets[256] = { 0 };
					for (size_t index = 0; index < input->filter_character_count; index++) {
						CharacterType type = CharacterType::Unknown;

						size_t current_index = input->filter_characters_start + index;
						system->FindCharacterType(input->text->buffer[current_index], type);
						bool is_valid = data->filter(input->text->buffer[current_index], type);
						size_t current_sprite_index = current_index * 6;

						if (index > 0) {
							push_offsets[index] = push_offsets[index - 1] + !is_valid * (input->vertices[current_sprite_index + 1].position.x - input->vertices[current_sprite_index].position.x);
						}

						if (is_valid && index != valid_characters) {
							size_t filter_index = input->filter_characters_start + valid_characters;
							size_t filter_sprite_index = filter_index * 6;
							input->text->buffer[input->filter_characters_start + valid_characters] = input->text->buffer[current_index];

							for (size_t subindex = 0; subindex < 6; subindex++) {
								input->vertices[filter_sprite_index + subindex] = input->vertices[current_sprite_index + subindex];
								input->vertices[filter_sprite_index + subindex].position.x -= push_offsets[index - 1];
							}
						}
						valid_characters += is_valid;
					}
					push_offsets[0] = 0.0f;

					size_t push_count = input->filter_character_count - valid_characters;
					if (push_count > 0) {
						for (size_t index = input->filter_characters_start + input->filter_character_count; index < input->text->size; index++) {
							input->text->buffer[index - push_count] = input->text->buffer[index];

							for (size_t subindex = 0; subindex < 6; subindex++) {
								input->vertices[(index - push_count) * 6 + subindex] = input->vertices[index * 6];
								input->vertices[(index - push_count) * 6 + subindex].position.x -= push_offsets[input->filter_character_count];
							}
						}
						input->text->size -= push_count;
						input->vertices.size -= push_count * 6;

						HandlerCommand last_command;
						system->GetLastRevertCommand(last_command, window_index);

						if (last_command.type == ECS_UI_HANDLER_COMMAND_TYPE::ECS_UI_HANDLER_COMMAND_TEXT_REPLACE) {
							UIDrawerTextInputReplaceCommandInfo* command = (UIDrawerTextInputReplaceCommandInfo*)last_command.handler.data;
							command->text_count -= push_count;
						}
					}

					input->filter_characters_start = 0;
					input->filter_character_count = 0;
					input->current_selection = input->current_selection > input->text->size ? input->text->size : input->current_selection;
					input->current_sprite_position = input->current_sprite_position > input->text->size ? input->text->size : input->current_sprite_position;
				}

				if (keyboard->IsKeyUp(HID::Key::LeftControl) && keyboard->IsKeyUp(HID::Key::RightControl)) {
					char characters[64];
					size_t character_count = 0;
					while (keyboard->GetCharacter(characters[character_count++])) {
						CharacterType type;
						system->FindCharacterType(characters[character_count - 1], type);
						if (type == CharacterType::Unknown) {
							character_count--;
						}
						else {
							character_count -= data->filter(characters[character_count - 1], type) ? 0 : 1;
						}
					}
					character_count--;

					if (character_count > 0) {
						if (input->current_sprite_position != input->current_selection) {
							backspace_lambda();

							size_t total_size = sizeof(UIDrawerTextInputRemoveCommandInfo) + *backspace_text_count + 1;
							UIDrawerTextInputRemoveCommandInfo* remove_info = (UIDrawerTextInputRemoveCommandInfo*)system->m_memory->Allocate(total_size);
							system->AddWindowMemoryResource(remove_info, window_index);

							remove_info->input = input;
							remove_info->text_position = *backspace_text_position;
							remove_info->text_count = *backspace_text_count;
							remove_info->deallocate_data = true;
							remove_info->deallocate_buffer = remove_info;
							unsigned char* info_text = (unsigned char*)((uintptr_t)remove_info + sizeof(UIDrawerTextInputRemoveCommandInfo));
							memcpy(info_text, backspace_text, *backspace_text_count);
							info_text[*backspace_text_count] = '\0';

							AddWindowHandleCommand(system, window_index, TextInputRevertRemoveText, remove_info, total_size, ECS_UI_HANDLER_COMMAND_TYPE::ECS_UI_HANDLER_COMMAND_TEXT_REMOVE);
						}

						size_t text_size = input->text->size;

						if (input->text->size + character_count <= input->text->capacity) {
							unsigned char revert_info_data[revert_command_stack_size];
							UIDrawerTextInputAddCommandInfo* revert_info = (UIDrawerTextInputAddCommandInfo*)revert_info_data;
							revert_info->input = input;
							revert_info->text_count = character_count;
							revert_info->text_position = input->current_sprite_position;

							AddWindowHandleCommand(system, window_index, TextInputRevertAddText, revert_info, sizeof(UIDrawerTextInputAddCommandInfo), ECS_UI_HANDLER_COMMAND_TYPE::ECS_UI_HANDLER_COMMAND_TEXT_ADD);

							input->InsertCharacters(characters, character_count, input->current_sprite_position, system);
						}

						input->current_selection = input->current_sprite_position;
						input->text->buffer[input->text->size] = '\0';
					}
				}

				HID::Key repeat_key = HID::Key::None;
				if (!is_backspace_lambda) {
					input->RepeatKeyAction(system, keyboard_tracker, keyboard, HID::Key::Back, repeat_key, backspace_lambda);
					input->RepeatKeyAction(system, keyboard_tracker, keyboard, HID::Key::Delete, repeat_key, backspace_lambda);

					if (is_backspace_lambda) {
						size_t total_size = sizeof(UIDrawerTextInputRemoveCommandInfo) + *backspace_text_count + 1;
						UIDrawerTextInputRemoveCommandInfo* remove_info = (UIDrawerTextInputRemoveCommandInfo*)system->m_memory->Allocate(total_size);
						system->AddWindowMemoryResource(remove_info, window_index);

						remove_info->input = input;
						remove_info->text_count = *backspace_text_count;
						remove_info->text_position = input->current_sprite_position;
						remove_info->deallocate_data = true;
						remove_info->deallocate_buffer = remove_info;
						unsigned char* info_text = (unsigned char*)((uintptr_t)remove_info + sizeof(UIDrawerTextInputRemoveCommandInfo));
						memcpy((void*)((uintptr_t)remove_info + sizeof(UIDrawerTextInputRemoveCommandInfo)), backspace_text, *backspace_text_count);
						info_text[*backspace_text_count] = '\0';

						AddWindowHandleCommand(system, window_index, TextInputRevertRemoveText, remove_info, total_size, ECS_UI_HANDLER_COMMAND_TEXT_REMOVE);
					}
				}

				if (!input->suppress_arrow_movement) {
					if (input->repeat_key == HID::Key::Right && keyboard->IsKeyUp(HID::Key::Right)) {
						input->repeat_key = HID::Key::None;
						if (keyboard->IsKeyDown(HID::Key::Left)) {
							input->repeat_key_count = 0;
							input->repeat_key = HID::Key::Left;
							input->key_repeat_start = std::chrono::high_resolution_clock::now();
						}
					}
					else if (input->repeat_key == HID::Key::Left && keyboard->IsKeyUp(HID::Key::Left)) {
						input->repeat_key = HID::Key::None;
						if (keyboard->IsKeyDown(HID::Key::Right)) {
							input->repeat_key_count = 0;
							input->repeat_key = HID::Key::Right;
							input->key_repeat_start = std::chrono::high_resolution_clock::now();
						}
					}
					input->RepeatKeyAction(system, keyboard_tracker, keyboard, HID::Key::Left, repeat_key, left_arrow_lambda);
					input->RepeatKeyAction(system, keyboard_tracker, keyboard, HID::Key::Right, repeat_key, right_arrow_lambda);
				}

				if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
					if (keyboard_tracker->IsKeyPressed(HID::Key::X)) {
						input->CopyCharacters(system);
						backspace_lambda();

						size_t total_size = sizeof(UIDrawerTextInputRemoveCommandInfo) + *backspace_text_count + 1;
						UIDrawerTextInputRemoveCommandInfo* remove_info = (UIDrawerTextInputRemoveCommandInfo*)system->m_memory->Allocate(total_size);
						system->AddWindowMemoryResource(remove_info, window_index);

						remove_info->input = input;
						remove_info->text_count = *backspace_text_count;
						remove_info->text_position = input->current_sprite_position;
						remove_info->deallocate_data = true;
						remove_info->deallocate_buffer = remove_info;
						unsigned char* info_text = (unsigned char*)((uintptr_t)remove_info + sizeof(UIDrawerTextInputRemoveCommandInfo));
						memcpy(info_text, backspace_text, *backspace_text_count);
						info_text[*backspace_text_count] = '\0';

						AddWindowHandleCommand(system, window_index, TextInputRevertRemoveText, remove_info, total_size, ECS_UI_HANDLER_COMMAND_TYPE::ECS_UI_HANDLER_COMMAND_TEXT_REMOVE);
					}
					else if (keyboard_tracker->IsKeyPressed(HID::Key::C)) {
						input->CopyCharacters(system);
					}
					else if (keyboard_tracker->IsKeyPressed(HID::Key::V)) {
						char characters[256];
						unsigned int character_count = system->m_application->CopyTextFromClipboard(characters, 256);

						if (input->text->size + character_count <= input->text->capacity) {
							char deleted_characters[revert_command_stack_size];
							unsigned int deleted_character_count = 0;
							unsigned int delete_position = 0;
							input->PasteCharacters(characters, character_count, system, window_index, deleted_characters, &deleted_character_count, &delete_position);
							input->current_selection = delete_position;

							size_t total_size = sizeof(UIDrawerTextInputReplaceCommandInfo) + deleted_character_count + 1;
							UIDrawerTextInputReplaceCommandInfo* replace_info = (UIDrawerTextInputReplaceCommandInfo*)system->m_memory->Allocate(total_size);
							system->AddWindowMemoryResource(replace_info, window_index);

							replace_info->input = input;
							replace_info->replaced_text_count = deleted_character_count;
							char* info_text = (char*)((uintptr_t)replace_info + sizeof(UIDrawerTextInputReplaceCommandInfo));
							replace_info->text_position = delete_position;
							replace_info->text_count = character_count;
							replace_info->deallocate_buffer = replace_info;
							memcpy(info_text, deleted_characters, deleted_character_count);
							info_text[deleted_character_count] = '\0';

							AddWindowHandleCommand(system, window_index, TextInputRevertReplaceText, replace_info, total_size, ECS_UI_HANDLER_COMMAND_TYPE::ECS_UI_HANDLER_COMMAND_TEXT_REPLACE);
						}
					}
					else if (keyboard_tracker->IsKeyPressed(HID::Key::A)) {
						input->current_selection = 0;
						input->current_sprite_position = input->text->size;
					}
				}

				size_t caret_tick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - input->caret_start).count();
				if (caret_tick >= system->m_descriptors.misc.text_input_caret_display_time) {
					input->is_caret_display = !input->is_caret_display;
					input->caret_start = std::chrono::high_resolution_clock::now();
				}

				if (input->repeat_key != HID::Key::Apps && input->repeat_key != HID::Key::BrowserFavorites) {
					input->repeat_key = repeat_key;
				}
			}
			else if (keyboard_tracker->IsKeyPressed(HID::Key::Enter)) {
				keyboard->DoNotCaptureCharacters();
				input->is_caret_display = false;
				input->current_selection = input->current_sprite_position;
				input->is_currently_selected = false;
				input->trigger_callback = true;

				system->DeallocateGeneralHandler();
				system->m_focused_window_data.ChangeGeneralHandler({ 0.0f, 0.0f }, { 0.0f, 0.0f }, nullptr, nullptr, 0, ECS_UI_DRAW_NORMAL);
			}
			else if (!UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
				keyboard->DoNotCaptureCharacters();
				input->is_caret_display = false;
				input->current_selection = input->current_sprite_position;
				input->is_currently_selected = false;

				/*if (input->HasCallback()) {
					input->Callback(action_data);
				}*/
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void SliderEnterValues(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerSliderEnterValuesData* data = (UIDrawerSliderEnterValuesData*)_data;
			UIDrawerSlider* slider = data->slider;

			UIDrawerSliderEnterValuesData* additional_data = (UIDrawerSliderEnterValuesData*)_additional_data;;
			system->m_focused_window_data.clean_up_call_general = true;

			auto terminate_lambda = [&]() {
				if (slider->text_input_counter > 0) {
					data->convert_input(slider->characters, slider->value_to_change);
					UIDrawerTextInputActionData input_data = { slider->text_input, data->filter_function };
					action_data->data = &input_data;
					TextInputAction(action_data);
					slider->text_input_counter = 0;
					slider->character_value = true;
					system->m_keyboard->DoNotCaptureCharacters();
					return true;
				}
				return false;
			};

			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
				if (UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
					if (slider->text_input_counter == 0) {
						size_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - additional_data->slider->enter_value_click).count();
						if (duration < system->m_descriptors.misc.slider_enter_value_duration) {
							slider->text_input_counter = 1;
							system->m_keyboard->CaptureCharacters();
						}
						slider->enter_value_click = std::chrono::high_resolution_clock::now();
					}
				}
				else if (slider->text_input_counter == 0 && mouse_tracker->LeftButton() == MBPRESSED) {
					slider->enter_value_click = std::chrono::high_resolution_clock::now();
				}
				else if (keyboard_tracker->IsKeyPressed(HID::Key::Enter)) {
					bool is_valid_data = terminate_lambda();
					system->DeallocateGeneralHandler();
					system->m_focused_window_data.ResetGeneralHandler();
				}
				else {
					UIDrawerTextInputActionData input_data = { slider->text_input, data->filter_function };
					action_data->data = &input_data;
					TextInputAction(action_data);

					if (keyboard->IsKeyDown(HID::Key::LeftControl) && keyboard_tracker->IsKeyPressed(HID::Key::C)) {
						system->m_application->WriteTextToClipboard(slider->characters.buffer);
					}
				}
			}
			else if (!UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
				terminate_lambda();
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorInputSVRectangleClickableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ColorInputHSVGradientInfo* data = (ColorInputHSVGradientInfo*)_data;
			float x_factor = function::InverseLerp(position.x, position.x + scale.x, mouse_position.x);
			x_factor = function::Clamp(x_factor, 0.0f, 1.0f);

			float y_factor = function::InverseLerp(position.y, position.y + scale.y, mouse_position.y);
			y_factor = function::Clamp(y_factor, 0.0f, 1.0f);

			data->input->hsv.saturation = x_factor * Color::GetRange();
			data->input->hsv.value = (1.0f - y_factor) * Color::GetRange();
			*data->input->rgb = HSVToRGB(data->input->hsv);

			data->input->Callback(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorInputHRectangleClickableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ColorInputHSVGradientInfo* info = (ColorInputHSVGradientInfo*)_data;

			float factor = function::InverseLerp(info->gradient_position.y, info->gradient_position.y + info->gradient_scale.y, mouse_position.y);
			factor = function::Clamp(factor, 0.0f, 1.0f);

			info->input->hsv.hue = factor * Color::GetRange();
			*info->input->rgb = HSVToRGB(info->input->hsv);

			info->input->Callback(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorInputARectangleClickableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ColorInputHSVGradientInfo* info = (ColorInputHSVGradientInfo*)_data;

			float factor = function::InverseLerp(info->gradient_position.y, info->gradient_position.y + info->gradient_scale.y, mouse_position.y);
			factor = function::Clamp(factor, 0.0f, 1.0f);

			info->input->rgb->alpha = (1.0f - factor) * Color::GetRange();
			info->input->hsv.alpha = info->input->rgb->alpha;

			info->input->Callback(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorInputWindowDraw(void* window_data, void* drawer_descriptor, bool initializer) {
			UI_PREPARE_DRAWER(initializer);

			const size_t RECTANGLE_CONFIGURATION = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_RECTANGLE_CLICKABLE_ACTION;
			const size_t CIRCLE_CONFIGURATION = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE;
			const size_t TRIANGLE_CONFIGURATION = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_RECTANGLE_CLICKABLE_ACTION | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_FIT_SPACE;
			const float RECTANGLE_Y_SCALE = 0.55f;
			const float HUE_ALPHA_X_SCALE = 0.065f;

			float2 drag_triangle_size = drawer.GetSquareScale(0.03f);
			float2 circle_size = drawer.GetSquareScale(0.07f);

			auto triangle_lambda = [&](float2 position, float2 scale, float2 triangle_size, float percentage, UIConfigClickableAction action) {
				UIDrawConfig triangle1_config;
				UIConfigAbsoluteTransform triangle1_transform;
				triangle1_transform.position = { position.x - triangle_size.x * 0.5f, function::Lerp<true>(position.y, scale.y, percentage) - triangle_size.y * 0.5f };
				triangle1_transform.scale = triangle_size;

				triangle1_config.AddFlags(action, triangle1_transform);

				UIDrawConfig triangle2_config;
				UIConfigAbsoluteTransform triangle2_transform;
				triangle2_transform.position = triangle1_transform.position;
				triangle2_transform.position.x += scale.x;
				triangle2_transform.scale = triangle1_transform.scale;

				triangle2_config.AddFlags(action, triangle2_transform);

				drawer.SpriteRectangle(TRIANGLE_CONFIGURATION | UI_CONFIG_LATE_DRAW, triangle1_config, ECS_TOOLS_UI_TEXTURE_TRIANGLE, ECS_COLOR_WHITE, float2(1.0f, 0.0f), float2(0.0f, 1.0f));
				drawer.SpriteRectangle(TRIANGLE_CONFIGURATION | UI_CONFIG_LATE_DRAW, triangle2_config, ECS_TOOLS_UI_TEXTURE_TRIANGLE, ECS_COLOR_WHITE, float2(0.0f, 1.0f), float2(1.0f, 0.0f));
			};
#undef TRIANGLE_CONFIGURATION

#pragma region Initialization
			drawer.DisablePaddingForRenderSliders();
			drawer.DisablePaddingForRenderRegion();
			drawer.DisableZoom();
			drawer.SetDrawMode(ECS_UI_DRAWER_MODE::ECS_UI_DRAWER_INDENT);

			UIDrawerColorInputWindowData* data = (UIDrawerColorInputWindowData*)window_data;
			UIDrawerColorInput* main_input = data->input;
			UIDrawerColorInput* input;
			Color hsv_color;

			if (!initializer) {
				input = (UIDrawerColorInput*)drawer.GetResource("ColorInput");

				input->color_picker_initial_color = main_input->color_picker_initial_color;
				input->callback = main_input->callback;
				/*if (main_input->callback.action != nullptr) {
					if (input->a_slider->changed_value_callback.action == nullptr) {
						input->a_slider->changed_value_callback = main_input->callback;
						input->b_slider->changed_value_callback = main_input->callback;
						input->g_slider->changed_value_callback = main_input->callback;
						input->r_slider->changed_value_callback = main_input->callback;
						input->h_slider->changed_value_callback = main_input->callback;
						input->s_slider->changed_value_callback = main_input->callback;
						input->v_slider->changed_value_callback = main_input->callback;
					}
					else {
						struct WrapperData {
							UIActionHandler first;
							UIActionHandler second;
						};
						auto wrapper = [](ActionData* action_data) {
							UI_UNPACK_ACTION_DATA;

							WrapperData* data = (WrapperData*)_data;
							action_data->data = data->first.data;
							data->first.action(action_data);

							action_data->data = data->second.data;
							data->second.action(action_data);
						};

						if (input->a_slider->changed_value_callback.action != wrapper) {
							UIActionHandler wrapper_callback = { wrapper, drawer.GetMainAllocatorBuffer(sizeof(WrapperData)), sizeof(WrapperData), main_input->callback.phase };
							WrapperData* wrapper_data = (WrapperData*)wrapper_callback.data;
							wrapper_data->first = input->a_slider->changed_value_callback;
							wrapper_data->second = main_input->callback;

							input->a_slider->changed_value_callback = wrapper_callback;
							input->b_slider->changed_value_callback = wrapper_callback;
							input->g_slider->changed_value_callback = wrapper_callback;
							input->r_slider->changed_value_callback = wrapper_callback;
							input->h_slider->changed_value_callback = wrapper_callback;
							input->s_slider->changed_value_callback = wrapper_callback;
							input->v_slider->changed_value_callback = wrapper_callback;
						}
					}
				}*/

				main_input->hsv = input->hsv;
				input->rgb = main_input->rgb;
			}
			else {
				input = main_input;
			}

			hsv_color = input->hsv;

#pragma endregion

#pragma region Saturation-Value Rectangle
			const size_t GRADIENT_COUNT = 15;
			UIDrawConfig sv_config;

			UIConfigWindowDependentSize sv_size;
			sv_size.type = ECS_UI_WINDOW_DEPENDENT_SIZE::ECS_UI_WINDOW_DEPENDENT_BOTH;
			sv_size.scale_factor = { RECTANGLE_Y_SCALE, RECTANGLE_Y_SCALE };

			ColorInputHSVGradientInfo h_info;
			h_info.input = input;

			UIConfigClickableAction sv_clickable_action;
			sv_clickable_action.handler.action = ColorInputSVRectangleClickableAction;
			sv_clickable_action.handler.data = &h_info;
			sv_clickable_action.handler.data_size = sizeof(h_info);

			Color sv_colors[4];
			sv_colors[0] = ECS_COLOR_WHITE;
			sv_colors[1] = HSVToRGB(Color(hsv_color.hue, 255, 255));
			sv_colors[2] = ECS_COLOR_BLACK;
			sv_colors[3] = ECS_COLOR_BLACK;
			sv_config.AddFlags(sv_size, sv_clickable_action);

			drawer.SetCurrentX(drawer.GetNextRowXPosition());
			float2 sv_position = drawer.GetCurrentPositionNonOffset();
			float2 sv_scale = drawer.GetWindowSizeScaleElement(ECS_UI_WINDOW_DEPENDENT_BOTH, { RECTANGLE_Y_SCALE, RECTANGLE_Y_SCALE });
			drawer.Gradient(RECTANGLE_CONFIGURATION, sv_config, sv_colors, 15, 15);

			UIDrawConfig circle_config;

			UIConfigAbsoluteTransform circle_transform;
			circle_transform.position = {
				function::Lerp<true>(sv_position.x, sv_scale.x, static_cast<float>(hsv_color.saturation) / Color::GetRange()),
				function::Lerp<true>(sv_position.y, sv_scale.y, 1.0f - static_cast<float>(hsv_color.value) / Color::GetRange())
			};

			circle_transform.position = CenterRectangle(circle_size, circle_transform.position);
			circle_transform.scale = circle_size;

			circle_config.AddFlag(circle_transform);
			Color filled_circle_color = HSVToRGB(hsv_color);
			filled_circle_color.alpha = 255;
			drawer.SpriteRectangle(CIRCLE_CONFIGURATION, circle_config, ECS_TOOLS_UI_TEXTURE_FILLED_CIRCLE, filled_circle_color, float2(0.0f, 0.0f), float2(1.0f, 1.0f));
			drawer.SpriteRectangle(CIRCLE_CONFIGURATION, circle_config, ECS_TOOLS_UI_TEXTURE_BOLD_CIRCLE);
			drawer.Indent();
#pragma endregion

#pragma region Hue Rectangle
			UIDrawConfig h_config;
			UIConfigWindowDependentSize h_size;
			h_size.type = ECS_UI_WINDOW_DEPENDENT_BOTH;
			h_size.scale_factor = { HUE_ALPHA_X_SCALE, RECTANGLE_Y_SCALE };

			float2 h_gradient_position = drawer.GetCurrentPositionNonOffset();
			float2 gradient_scale = drawer.GetWindowSizeScaleElement(ECS_UI_WINDOW_DEPENDENT_BOTH, { HUE_ALPHA_X_SCALE, RECTANGLE_Y_SCALE });

			h_info.gradient_position = h_gradient_position;
			h_info.gradient_scale = gradient_scale;
			UIConfigClickableAction h_action;
			h_action.handler.action = ColorInputHRectangleClickableAction;
			h_action.handler.data = &h_info;
			h_action.handler.data_size = sizeof(h_info);

			h_config.AddFlags(h_size, h_action);

			drawer.SpriteRectangle(RECTANGLE_CONFIGURATION, h_config, ECS_TOOLS_UI_TEXTURE_HUE_GRADIENT);

			triangle_lambda(h_gradient_position, gradient_scale, drag_triangle_size, static_cast<float>(hsv_color.hue) / Color::GetRange(), h_action);
			drawer.Indent();
#pragma endregion

#pragma region Alpha rectangle
			UIDrawConfig a1_config;
			UIDrawConfig a2_config;
			UIConfigWindowDependentSize a_size;
			a_size.type = ECS_UI_WINDOW_DEPENDENT_BOTH;
			a_size.scale_factor = { HUE_ALPHA_X_SCALE, RECTANGLE_Y_SCALE };

			float2 a_gradient_position = drawer.GetCurrentPositionNonOffset();
			ColorInputHSVGradientInfo a_gradient_info;
			a_gradient_info.gradient_position = a_gradient_position;
			a_gradient_info.gradient_scale = gradient_scale;
			a_gradient_info.input = input;

			UIConfigClickableAction a_action;
			a_action.handler.action = ColorInputARectangleClickableAction;
			a_action.handler.data = &a_gradient_info;
			a_action.handler.data_size = sizeof(a_gradient_info);

			Color a_colors[6];
			a_colors[0] = HSVToRGB(hsv_color);
			a_colors[0].alpha = 255;
			a_colors[1] = a_colors[0];
			a_colors[2] = a_colors[0];
			a_colors[2].alpha = 0;
			a_colors[3] = a_colors[2];
			a_colors[4] = ECS_COLOR_WHITE;
			a_colors[5] = ECS_COLOR_WHITE;

			UIConfigSpriteGradient a1_gradient;
			a1_gradient.texture = ECS_TOOLS_UI_TEXTURE_MASK;
			a1_gradient.top_left_uv = { 0.0f, 0.0f };
			a1_gradient.bottom_right_uv = { 1.0f, 1.0f };

			UIConfigSpriteGradient a2_gradient;
			a2_gradient.texture = ECS_TOOLS_UI_TEXTURE_EMPTY_TEXTURE_TILES_BIG;
			a2_gradient.top_left_uv = { 0.0f, 0.0f };
			a2_gradient.bottom_right_uv = { 1.0f, 5.0f };

			a1_config.AddFlags(a_size, a_action, a1_gradient);
			a2_config.AddFlags(a_size, a_action, a2_gradient);

			drawer.Gradient(RECTANGLE_CONFIGURATION | UI_CONFIG_SPRITE_GRADIENT | UI_CONFIG_DO_NOT_ADVANCE, a1_config, a_colors, 1, 10);
			drawer.Gradient(RECTANGLE_CONFIGURATION | UI_CONFIG_SPRITE_GRADIENT, a2_config, a_colors + 2, 1, 10);

			triangle_lambda(a_gradient_position, gradient_scale, drag_triangle_size, 1.0f - static_cast<float>(input->rgb->alpha) / Color::GetRange(), a_action);

#pragma endregion

#pragma region Current And Previous Color
			const float text_offset = 0.01f;
			const float current_color_scale = 0.18f;
			const float solid_color_y_scale = 0.18f;
			const size_t TEXT_CONFIGURATION = UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE;

			UIDrawConfig current_config;
			drawer.Indent();
			float2 current_position = drawer.GetCurrentPosition();
			current_position.y += text_offset;

			float2 text_span;
			drawer.Text(TEXT_CONFIGURATION, current_config, "Current", current_position, text_span);
			drawer.OffsetY(text_span.y + 2 * text_offset);
			current_position.y += text_span.y + text_offset;

			UIConfigWindowDependentSize solid_color_transform;
			solid_color_transform.type = ECS_UI_WINDOW_DEPENDENT_BOTH;
			solid_color_transform.scale_factor = { current_color_scale, solid_color_y_scale };

			float2 rectangle_scale = drawer.GetWindowSizeScaleElement(ECS_UI_WINDOW_DEPENDENT_BOTH, { current_color_scale, solid_color_y_scale });
			current_config.AddFlag(solid_color_transform);

			filled_circle_color.alpha = input->rgb->alpha;
			drawer.AlphaColorRectangle(TEXT_CONFIGURATION | UI_CONFIG_WINDOW_DEPENDENT_SIZE, current_config, filled_circle_color);
			drawer.OffsetY(rectangle_scale.y + text_offset);
			current_position.y += rectangle_scale.y + text_offset;

			drawer.Text(TEXT_CONFIGURATION, current_config, "Previous", current_position, text_span);
			drawer.OffsetY(text_span.y + text_offset);
			current_position.y += text_span.y + text_offset;

			UIDrawConfig previous_config;

			solid_color_transform.offset.y = text_span.y + text_offset;
			solid_color_transform.scale_factor = { current_color_scale, solid_color_y_scale };

			UIConfigClickableAction previous_action;
			previous_action.handler.action = ColorInputPreviousRectangleClickableAction;
			previous_action.handler.data = input;
			previous_action.handler.data_size = 0;

			previous_config.AddFlags(solid_color_transform, previous_action);

			drawer.AlphaColorRectangle(
				TEXT_CONFIGURATION | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_RECTANGLE_CLICKABLE_ACTION, 
				previous_config, 
				input->color_picker_initial_color
			);
			drawer.NextRow();
#pragma endregion

#pragma region Sliders

			const size_t COLOR_INPUT_CONFIGURATION = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER | UI_CONFIG_COLOR_INPUT_RGB_SLIDERS
				| UI_CONFIG_COLOR_INPUT_HSV_SLIDERS | UI_CONFIG_COLOR_INPUT_NO_NAME | UI_CONFIG_COLOR_INPUT_DO_NOT_CHOOSE_COLOR
				| UI_CONFIG_COLOR_INPUT_HEX_INPUT | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE | UI_CONFIG_SLIDER_ENTER_VALUES | UI_CONFIG_DO_CACHE;

			UIDrawConfig color_input_config;
			UIConfigWindowDependentSize color_input_transform;
			color_input_transform.type = ECS_UI_WINDOW_DEPENDENT_HORIZONTAL;
			color_input_transform.scale_factor = { 0.34f, 1.0f };

			UIConfigSliderMouseDraggable mouse_draggable;
			mouse_draggable.interpolate_bounds = true;

			color_input_config.AddFlag(color_input_transform);
			color_input_config.AddFlag(mouse_draggable);
			drawer.ColorInput(COLOR_INPUT_CONFIGURATION, color_input_config, "ColorInput", main_input->rgb);

#pragma endregion
		}

		// --------------------------------------------------------------------------------------------------------------

		void ColorInputCreateWindow(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			system->DestroyWindowIfFound("ColorInputWindow");

			UIDrawerColorInput* data = (UIDrawerColorInput*)_data;

			data->color_picker_initial_color = *data->rgb;
			Color* hsv_color = &data->hsv;
			UIWindowDescriptor window_descriptor;
			window_descriptor.draw = ColorInputWindowDraw;

			float2 window_position;
			if (position.x + system->m_descriptors.misc.color_input_window_size_x > 0.98f) {
				window_position.x = 0.98f - system->m_descriptors.misc.color_input_window_size_x;
			}
			else {
				window_position.x = position.x;
			}

			if (position.y + system->m_descriptors.misc.color_input_window_size_y > 1.0f - system->m_descriptors.element_descriptor.color_input_padd) {
				window_position.y = position.y - system->m_descriptors.misc.color_input_window_size_y - system->m_descriptors.element_descriptor.color_input_padd;
			}
			else {
				window_position.y = position.y + scale.y + system->m_descriptors.element_descriptor.color_input_padd;
			}

			window_descriptor.initial_position_x = window_position.x;
			window_descriptor.initial_position_y = window_position.y;
			window_descriptor.initial_size_x = system->m_descriptors.misc.color_input_window_size_x;
			window_descriptor.initial_size_y = system->m_descriptors.misc.color_input_window_size_y;
			window_descriptor.private_action = SkipAction;
			window_descriptor.private_action_data = nullptr;

			UIDrawerColorInputWindowData window_data;
			window_data.color = *data->rgb;
			window_data.input = data;

			window_descriptor.window_name = "ColorInputWindow";
			window_descriptor.window_data = &window_data;
			window_descriptor.window_data_size = sizeof(window_data);

			unsigned int window_index = system->CreateWindowAndDockspace(window_descriptor, UI_DOCKSPACE_NO_DOCKING);

			// if it is desired to be destroyed when going out of focus
			UIPopUpWindowData system_handler_data;
			system_handler_data.is_fixed = true;
			system_handler_data.is_initialized = true;
			system_handler_data.destroy_at_first_click = false;
			system_handler_data.name = "ColorInputWindow";
			system_handler_data.reset_when_window_is_destroyed = true;

			UIActionHandler handler;
			handler.action = PopUpWindowSystemHandler;
			handler.data = &system_handler_data;
			handler.data_size = sizeof(system_handler_data);
			system->PushFrameHandler(handler);
		}

		// --------------------------------------------------------------------------------------------------------------

		void ComboBoxWindowDraw(void* window_data, void* drawer_descriptor, bool initializer) {
			UI_PREPARE_DRAWER(initializer);

			UIDrawerComboBoxClickable* data = (UIDrawerComboBoxClickable*)window_data;
			if (!initializer) {
				drawer.DisablePaddingForRenderRegion();
				drawer.DisableZoom();
				
				drawer.SetRowPadding(0.0f);
				drawer.SetNextRowYOffset(0.0f);

				size_t new_configuration = function::ClearFlag(function::ClearFlag(data->configuration, UI_CONFIG_LATE_DRAW), UI_CONFIG_SYSTEM_DRAW);
				new_configuration = function::ClearFlag(new_configuration, UI_CONFIG_ALIGN_TO_ROW_Y);
				drawer.ComboBoxDropDownDrawer(new_configuration, data->config, data->box);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void ComboBoxClickable(ActionData* action_data) {
			constexpr const char* COMBO_BOX_WINDOW_NAME = "ComboBoxOpened";

			UI_UNPACK_ACTION_DATA;

			UIDrawerComboBoxClickable* clickable_data = (UIDrawerComboBoxClickable*)_data;
			UIDrawerComboBox* data = clickable_data->box;

			unsigned int combo_index = system->GetWindowFromName(COMBO_BOX_WINDOW_NAME);
			if (mouse_tracker->LeftButton() == MBPRESSED) {
				clickable_data->is_opened_on_press = combo_index != -1;
			}
			else if (IsClickableTrigger(action_data) && !clickable_data->is_opened_on_press) {
				if (combo_index == -1) {
					UIWindowDescriptor window_descriptor;
					window_descriptor.draw = ComboBoxWindowDraw;
					window_descriptor.initial_position_x = position.x;
					window_descriptor.initial_size_x = scale.x;

					float label_display_size = data->label_y_scale * data->label_display_count;
					float position_displacement = position.y + scale.y + ECS_TOOLS_UI_COMBO_BOX_PADDING;

					if (position_displacement + label_display_size > 1.0f) {
						window_descriptor.initial_position_y = position.y - label_display_size - ECS_TOOLS_UI_COMBO_BOX_PADDING;
					}
					else {
						window_descriptor.initial_position_y = position_displacement;
					}
					window_descriptor.initial_size_y = label_display_size;
					window_descriptor.window_name = COMBO_BOX_WINDOW_NAME;
					window_descriptor.window_data = clickable_data;
					window_descriptor.window_data_size = sizeof(UIDrawerComboBoxClickable);

					system->CreateWindowAndDockspace(window_descriptor, UI_DOCKSPACE_FIXED | UI_DOCKSPACE_POP_UP_WINDOW
						| UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X | UI_DOCKSPACE_BORDER_FLAG_NO_TITLE | UI_DOCKSPACE_BORDER_FLAG_COLLAPSED_REGION_HEADER);

					UIPopUpWindowData handler_data;
					handler_data.is_fixed = true;
					handler_data.is_initialized = true;
					handler_data.destroy_at_first_click = false;
					handler_data.name = COMBO_BOX_WINDOW_NAME;

					UIActionHandler handler;
					handler.action = PopUpWindowSystemHandler;
					handler.data = &handler_data;
					handler.data_size = sizeof(handler_data);
					system->PushFrameHandler(handler);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void MenuDraw(void* window_data, void* drawer_descriptor, bool initializer) {
			UI_PREPARE_DRAWER(initializer);

			UIDrawerMenuDrawWindowData* data = (UIDrawerMenuDrawWindowData*)window_data;
			if (initializer) {
				drawer.DisableZoom();
				drawer.SetRowPadding(0.0f);
				drawer.SetNextRowYOffset(0.0f);
			}
			else {
#pragma region Drawer disables

				drawer.DisablePaddingForRenderRegion();
				drawer.DisablePaddingForRenderSliders();
				drawer.SetDrawMode(ECS_UI_DRAWER_NEXT_ROW);

#pragma endregion

#pragma region Config preparations

				UIDrawerMenuState* state = data->GetState();

				UIDrawConfig config;
				float2 region_scale = drawer.GetRegionScale();
				float2 default_element_scale = drawer.GetElementDefaultScale();

				UIConfigRelativeTransform transform;
				transform.scale.x = drawer.GetRelativeTransformFactors({ region_scale.x, 0.0f }).x;
				config.AddFlag(transform);

				// Must be replicated inside lambdas
				const size_t configuration = UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_DO_NOT_ADVANCE;

				UIConfigHoverableAction hoverable;
				UIDrawerSubmenuHoverable hover_data;
				memcpy(&hover_data.draw_data, data, sizeof(*data));
				hoverable.handler = { MenuSubmenuHoverable, &hover_data, sizeof(hover_data), ECS_UI_DRAW_SYSTEM };

				UIConfigGeneralAction general;
				UIDrawerMenuGeneralData general_data;
				general_data.menu = data->menu;
				general_data.destroy_state = state;
				general_data.destroy_scale = { region_scale.x, default_element_scale.y };
				general.handler = { MenuGeneral, &general_data, sizeof(general_data), ECS_UI_DRAW_SYSTEM };
				config.AddFlag(general);

#pragma endregion

				UIConfigClickableAction clickable;

				unsigned int current_word_start = 0;
				size_t LABEL_CONFIGURATION = UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y 
					| UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_DO_NOT_ADVANCE;

				auto draw_label = [&](size_t index, const unsigned short* substream, Stream<char> characters) {
					drawer.TextLabel(LABEL_CONFIGURATION, config, { characters.buffer + current_word_start, substream[index] - current_word_start });
					current_word_start = substream[index] + 1;
				};

#pragma region Left characters and >

				UIConfigTextAlignment alignment;
				alignment.horizontal = ECS_UI_ALIGN_LEFT;
				alignment.vertical = ECS_UI_ALIGN_MIDDLE;
				config.AddFlag(alignment);

				struct ClickableWrapperData {
					UIDrawerMenu* menu;
					Action action;
					void* data;
				};

				auto clickable_wrapper = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					if (IsClickableTrigger(action_data)) {
						ClickableWrapperData* data = (ClickableWrapperData*)_data;
						action_data->data = data->data;
						data->action(action_data);

						UIDrawerMenuCleanupSystemHandlerData cleanup_data;
						cleanup_data.window_count = data->menu->windows.size;
						for (size_t index = 0; index < cleanup_data.window_count; index++) {
							cleanup_data.window_names[index] = data->menu->windows[index].name;
						}
						system->PushFrameHandler({ MenuCleanupSystemHandler, &cleanup_data, sizeof(cleanup_data) });
						system->DeallocateGeneralHandler();
						system->m_focused_window_data.ResetGeneralHandler();
					}
				};

				unsigned char current_line_index = 0;
				for (size_t index = 0; index < state->row_count; index++) {
					float2 current_position = drawer.GetCurrentPositionNonOffset();

					general_data.menu_initializer_index = index;
					general_data.destroy_position = current_position;
					hover_data.row_index = index;
					auto system = drawer.GetSystem();
					Color arrow_color = system->m_descriptors.misc.menu_arrow_color;
					size_t RECTANGLE_CONFIGURATION = UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_RECTANGLE_GENERAL_ACTION;
					
					if (state->unavailables == nullptr || (state->unavailables != nullptr && !state->unavailables[index])) {
						ClickableWrapperData wrapper_data;
						wrapper_data.action = state->click_handlers[index].action;
						wrapper_data.data = state->click_handlers[index].data;
						wrapper_data.menu = data->menu;
						clickable.handler = { clickable_wrapper, &wrapper_data, sizeof(wrapper_data), state->click_handlers[index].phase };

						if (state->row_has_submenu != nullptr && state->row_has_submenu[index] == true) {
							clickable.handler = { SkipAction, nullptr, 0 };
						}
						config.AddFlag(clickable);

						LABEL_CONFIGURATION = function::ClearFlag(LABEL_CONFIGURATION, UI_CONFIG_UNAVAILABLE_TEXT);
						draw_label(index, state->left_row_substreams, state->left_characters);

						config.AddFlag(hoverable);
						drawer.Rectangle(RECTANGLE_CONFIGURATION | UI_CONFIG_RECTANGLE_HOVERABLE_ACTION | UI_CONFIG_RECTANGLE_CLICKABLE_ACTION, config);
						config.flag_count -= 2;
					}
					else {
						LABEL_CONFIGURATION = function::SetFlag(LABEL_CONFIGURATION, UI_CONFIG_UNAVAILABLE_TEXT);
						draw_label(index, state->left_row_substreams, state->left_characters);

						drawer.Rectangle(RECTANGLE_CONFIGURATION, config);

						arrow_color = system->m_descriptors.misc.menu_unavailable_arrow_color;
					}

					if (state->row_has_submenu != nullptr && state->row_has_submenu[index]) {
						auto buffers = drawer.GetBuffers();
						auto counts = drawer.GetCounts();
						float2 sign_scale = system->GetTextSpan(">", system->m_descriptors.font.size * ECS_TOOLS_UI_FONT_X_FACTOR, system->m_descriptors.font.size, system->m_descriptors.font.character_spacing);
						system->ConvertCharactersToTextSprites(
							">",
							{ current_position.x + region_scale.x - sign_scale.x * 2.0f, AlignMiddle(current_position.y, default_element_scale.y, sign_scale.y) },
							(UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE],
							arrow_color,
							counts[ECS_TOOLS_UI_TEXT_SPRITE],
							{ system->m_descriptors.font.size * ECS_TOOLS_UI_FONT_X_FACTOR, system->m_descriptors.font.size },
							system->m_descriptors.font.character_spacing
						);
						counts[ECS_TOOLS_UI_TEXT_SPRITE] += 6;
					}

					if (current_line_index < state->separation_line_count) {
						if (index == state->separation_lines[current_line_index]) {
							UIDrawConfig line_config;
							UIConfigAbsoluteTransform transform;
							transform.scale.x = drawer.region_scale.x - 2.0f * system->m_descriptors.misc.tool_tip_padding.x;
							transform.scale.y = ECS_TOOLS_UI_ONE_PIXEL_Y;
							transform.position = drawer.GetCurrentPosition();
							transform.position.x += system->m_descriptors.misc.tool_tip_padding.x;

							line_config.AddFlag(transform);
							drawer.CrossLine(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_CROSS_LINE_DO_NOT_INFER | UI_CONFIG_DO_NOT_ADVANCE, line_config);
							current_line_index++;
						}
					}
				}

#pragma endregion

#pragma region Right characters

				if (state->right_characters.size > 0) {
					// for text alignment
					config.flag_count--;
					alignment.horizontal = ECS_UI_ALIGN_RIGHT;
					config.AddFlag(alignment);

					drawer.SetCurrentY(drawer.region_position.y);
					drawer.NextRow();
					current_word_start = 0;
					for (size_t index = 0; index < state->row_count; index++) {
						float2 current_position = drawer.GetCurrentPositionNonOffset();

						auto system = drawer.GetSystem();
						if (state->unavailables == nullptr || (state->unavailables != nullptr && !state->unavailables[index])) {
							LABEL_CONFIGURATION = function::ClearFlag(LABEL_CONFIGURATION, UI_CONFIG_UNAVAILABLE_TEXT);
							draw_label(index, state->right_row_substreams, state->right_characters);
						}
						else {
							LABEL_CONFIGURATION = function::SetFlag(LABEL_CONFIGURATION, UI_CONFIG_UNAVAILABLE_TEXT);
							draw_label(index, state->right_row_substreams, state->right_characters);
						}
						drawer.current_row_y_scale = drawer.layout.default_element_y;
						drawer.NextRow();
					}

				}

#pragma endregion
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void MenuSubmenuHoverable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerSubmenuHoverable* data = (UIDrawerSubmenuHoverable*)_data;
			UIDrawerSubmenuHoverable* additional_data = nullptr;
			system->m_focused_window_data.clean_up_call_hoverable = true;
			system->m_focused_window_data.always_hoverable = true;

			if (_additional_data != nullptr) {
				additional_data = (UIDrawerSubmenuHoverable*)_additional_data;
			}

			UIDrawerMenuState* state = data->draw_data.GetState();
			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
				if (!UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
					if (state->windows->size > state->submenu_index) {
						UIDrawerMenuCleanupSystemHandlerData cleanup_data;
						unsigned int last_index = state->submenu_index +
							((state->row_has_submenu != nullptr) && state->row_has_submenu[data->row_index]) *
							((state->unavailables == nullptr) || (!state->unavailables[data->row_index]));
						unsigned int first_index = state->submenu_index + 1;
						if (first_index < state->windows->size) {
							cleanup_data.window_count = state->windows->size - first_index;
							for (size_t index = 0; index < cleanup_data.window_count; index++) {
								cleanup_data.window_names[index] = state->windows->buffer[first_index + index].name;
							}
							state->windows->size = first_index;
							//ECS_ASSERT(data->state->windows->size == first_index);
						}
						else {
							cleanup_data.window_count = 0;
						}

						system->PushFrameHandler({ MenuCleanupSystemHandler, &cleanup_data, sizeof(cleanup_data) });
					}
					if (state->row_has_submenu != nullptr && state->row_has_submenu[data->row_index]) {
						UITooltipBaseData base;
						float y_text_span = system->GetTextSpriteYScale(system->m_descriptors.font.size);
						base.next_row_offset = system->m_descriptors.window_layout.default_element_y - y_text_span;

						float2 window_size;
						if (state->right_characters.size == 0) {
							window_size = system->DrawToolTipSentenceSize(state->submenues[data->row_index].left_characters, &base);
						}
						else {
							window_size = system->DrawToolTipSentenceWithTextToRightSize(state->submenues[data->row_index].left_characters, state->submenues[data->row_index].right_characters, &base);
						}

						window_size.y -= 2.0f * system->m_descriptors.misc.tool_tip_padding.y + base.next_row_offset;
						window_size.x -= 2.0f * system->m_descriptors.misc.tool_tip_padding.x - 2.0f * system->m_descriptors.element_descriptor.label_padd.x - system->m_descriptors.misc.menu_x_padd;

						float arrow_span = system->GetTextSpan(">", system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.font.size), system->m_descriptors.font.size, system->m_descriptors.font.character_spacing).x;
						window_size.x += (state->row_has_submenu != nullptr) * (arrow_span + system->m_descriptors.element_descriptor.label_padd.x);

						UIWindowDescriptor submenu_descriptor;
						submenu_descriptor.draw = MenuDraw;
						submenu_descriptor.initial_position_x = position.x + scale.x;
						submenu_descriptor.initial_position_y = position.y;

						if (submenu_descriptor.initial_position_x + window_size.x > 1.0f) {
							submenu_descriptor.initial_position_x = 1.0f - window_size.x;
						}
						if (submenu_descriptor.initial_position_y + window_size.y > 1.0f) {
							submenu_descriptor.initial_position_y = 1.0f - window_size.y;
						}

						UIDrawerMenuDrawWindowData window_data;
						memcpy(&window_data, &data->draw_data, sizeof(window_data));
						unsigned char last_position = window_data.GetLastPosition();
						window_data.submenu_offsets[last_position] = data->row_index;
						window_data.submenu_offsets[last_position + 1] = -1;

						submenu_descriptor.initial_size_x = window_size.x;
						submenu_descriptor.initial_size_y = window_size.y;
						submenu_descriptor.window_data = &window_data;
						submenu_descriptor.window_data_size = sizeof(window_data);

						submenu_descriptor.window_name = state->submenues[data->row_index].left_characters;

						system->CreateWindowAndDockspace(submenu_descriptor,
							UI_DOCKSPACE_FIXED | UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X | UI_DOCKSPACE_BORDER_FLAG_COLLAPSED_REGION_HEADER
							| UI_DOCKSPACE_BORDER_FLAG_NO_TITLE | UI_DOCKSPACE_POP_UP_WINDOW);
						state->windows->AddSafe({ submenu_descriptor.window_name, {submenu_descriptor.initial_position_x, submenu_descriptor.initial_position_y}, window_size });
					}
				}

				if (state->unavailables == nullptr || (!state->unavailables[data->row_index] && state->unavailables != nullptr)) {
					float y_text_size = system->GetTextSpriteYScale(system->m_descriptors.font.size);
					float2 font_size = { system->m_descriptors.font.size * ECS_TOOLS_UI_FONT_X_FACTOR, system->m_descriptors.font.size };

					float2 text_position = {
						position.x + system->m_descriptors.element_descriptor.label_padd.x,
						AlignMiddle(position.y, scale.y, y_text_size)
					};

					unsigned int first_character = 0;
					if (data->row_index != 0) {
						first_character = state->left_row_substreams[data->row_index - 1] + 1;
					}
					system->ConvertCharactersToTextSprites(
						{ state->left_characters.buffer + first_character, state->left_row_substreams[data->row_index] - first_character },
						text_position,
						(UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE],
						system->m_descriptors.color_theme.text,
						counts[ECS_TOOLS_UI_TEXT_SPRITE],
						font_size,
						system->m_descriptors.font.character_spacing
					);
					counts[ECS_TOOLS_UI_TEXT_SPRITE] += (state->left_row_substreams[data->row_index] - first_character) * 6;

					if (state->right_characters.size > 0) {
						first_character = 0;
						if (data->row_index != 0) {
							first_character = state->right_row_substreams[data->row_index - 1] + 1;
						}
						float x_span = system->GetTextSpan(
							{ state->right_characters.buffer + first_character, state->right_row_substreams[data->row_index] - first_character },
							font_size.x,
							font_size.y,
							system->m_descriptors.font.character_spacing
						).x;

						float2 right_text_position = {
							position.x + scale.x - system->m_descriptors.element_descriptor.label_padd.x - x_span,
							text_position.y
						};
						system->ConvertCharactersToTextSprites(
							{ state->right_characters.buffer + first_character, state->right_row_substreams[data->row_index] - first_character },
							right_text_position,
							(UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE],
							system->m_descriptors.color_theme.text,
							counts[ECS_TOOLS_UI_TEXT_SPRITE],
							font_size,
							system->m_descriptors.font.character_spacing
						);
						counts[ECS_TOOLS_UI_TEXT_SPRITE] += (state->right_row_substreams[data->row_index] - first_character) * 6;
					}

					if (state->row_has_submenu != nullptr && state->row_has_submenu[data->row_index]) {
						// The compiler messes up the registers and overwrites the font size in distribution
						// so do a rewrite of the values
						font_size = { system->m_descriptors.font.size * ECS_TOOLS_UI_FONT_X_FACTOR, system->m_descriptors.font.size };
						float2 arrow_size = system->GetTextSpan(">", font_size.x, font_size.y, system->m_descriptors.font.character_spacing);
						float2 arrow_position = { position.x + scale.x - arrow_size.x * 2.0f, AlignMiddle(position.y, system->m_descriptors.window_layout.default_element_y, arrow_size.y) };

						system->ConvertCharactersToTextSprites(
							">",
							arrow_position,
							(UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE],
							ECS_TOOLS_UI_MENU_HOVERED_ARROW_COLOR,
							counts[ECS_TOOLS_UI_TEXT_SPRITE],
							font_size,
							system->m_descriptors.font.character_spacing
						);

						counts[ECS_TOOLS_UI_TEXT_SPRITE] += 6;
					}

					UIDefaultHoverableData hoverable_data;
					hoverable_data.colors[0] = system->m_descriptors.color_theme.theme;
					hoverable_data.percentages[0] = 1.25f;

					action_data->data = &hoverable_data;
					float normalized_border_size = system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.dockspaces.border_size);
					action_data->position.x += normalized_border_size;
					action_data->scale.x -= normalized_border_size * 2.0f;

					if (data->row_index == state->row_count - 1) {
						action_data->scale.y -= system->m_descriptors.dockspaces.border_size * 1.5f;
					}
					else if (data->row_index == 0) {
						action_data->scale.y -= system->m_descriptors.dockspaces.border_size;
						action_data->position.y += system->m_descriptors.dockspaces.border_size;
					}

					DefaultHoverableAction(action_data);

					if (state->separation_line_count > 0) {
						unsigned int line_index = 0;
						unsigned int line_position = state->separation_lines[line_index];
						while (line_index < state->separation_line_count && line_position <= data->row_index) {
							if (line_position == data->row_index - 1) {
								SetSolidColorRectangle(
									{ position.x + system->m_descriptors.misc.tool_tip_padding.x, position.y },
									{ scale.x - 2.0f * system->m_descriptors.misc.tool_tip_padding.x, ECS_TOOLS_UI_ONE_PIXEL_Y },
									system->m_descriptors.color_theme.borders,
									(UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR],
									counts[ECS_TOOLS_UI_SOLID_COLOR]
								);
							}
							if (line_position == data->row_index) {
								SetSolidColorRectangle(
									{ position.x + system->m_descriptors.misc.tool_tip_padding.x, position.y + scale.y },
									{ scale.x - 2.0f * system->m_descriptors.misc.tool_tip_padding.x, ECS_TOOLS_UI_ONE_PIXEL_Y },
									system->m_descriptors.color_theme.borders,
									(UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR],
									counts[ECS_TOOLS_UI_SOLID_COLOR]
								);
							}
							line_index++;
							line_position = state->separation_lines[line_index];
						}
					}
				}
			}
			else {
				if (!UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
					/*if (additional_data == nullptr || (additional_data != nullptr && (additional_data->state->submenu_index <= data->state->submenu_index))) {
						UIDrawerMenuCleanupSystemHandlerData cleanup_data;
						unsigned int first_index = data->state->submenu_index + 1;
						unsigned int last_index = data->state->submenu_index + 1 +
							((data->state->row_has_submenu != nullptr) && data->state->row_has_submenu[data->row_index]) *
							((data->state->unavailables == nullptr) || (!data->state->unavailables[data->row_index]));
						cleanup_data.window_count = last_index - first_index;
						for (size_t index = 0; index < cleanup_data.window_count; index++) {
							cleanup_data.window_names[index] = data->state->menu_names->buffer[first_index + index];
						}
						system->PushSystemHandler({ MenuCleanupSystemHandler, &cleanup_data, sizeof(cleanup_data) });
					}*/
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void MenuGeneral(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerMenuGeneralData* data = (UIDrawerMenuGeneralData*)_data;
			system->m_focused_window_data.clean_up_call_general = true;
			UIDrawerMenuGeneralData* additional_data = nullptr;
			if (_additional_data != nullptr) {
				additional_data = (UIDrawerMenuGeneralData*)_additional_data;
			}

			UIDrawerMenu* menu = data->menu;
			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
				if (mouse_tracker->LeftButton() == MBPRESSED) {
					data->is_opened_when_clicked = system->GetWindowFromName(menu->state.left_characters) != -1;
				}

				bool is_left_click_released = mouse_tracker->LeftButton() == MBRELEASED;
				if (data->initialize_from_right_click) {
					is_left_click_released = true;
					data->initialize_from_right_click = false;
				}
				//if (is_left_click_released && data->menu_initializer_index != 255) {
				//	if (data->destroy_state != nullptr && (data->destroy_state->unavailables == nullptr
				//		|| (!data->destroy_state->unavailables[data->menu_initializer_index])) && IsPointInRectangle(mouse_position, data->destroy_position, data->destroy_scale)) {
				//		
				//		UIDrawerMenuCleanupSystemHandlerData cleanup_data;
				//		cleanup_data.window_count = (int64_t)menu->windows.size;
				//		for (int64_t index = 0; index < cleanup_data.window_count; index++) {
				//			cleanup_data.window_names[index] = menu->windows[index].name;
				//		}
				//		menu->windows.size = 0;
				//		system->PushSystemHandler({ MenuCleanupSystemHandler, &cleanup_data, sizeof(cleanup_data) });
				//		/*for (int64_t index = (int64_t)menu->window_indices.size - 1; index >= 0; index--) {
				//			system->DestroyPopUpWindow(menu->window_indices[index], true);
				//		}
				//		menu->window_indices.size = 0;*/
				//	}
				//}
				if (IsPointInRectangle(mouse_position, position, scale) && is_left_click_released
					&& data->menu_initializer_index == 255 && !data->is_opened_when_clicked) {
					UITooltipBaseData tool_tip_data;
					float y_text_span = system->GetTextSpriteYScale(system->m_descriptors.font.size);
					tool_tip_data.next_row_offset = system->m_descriptors.window_layout.default_element_y - y_text_span;

					float2 window_dimensions;
					if (menu->state.right_characters.size == 0) {
						window_dimensions = system->DrawToolTipSentenceSize(menu->state.left_characters, &tool_tip_data);
					}
					else {
						window_dimensions = system->DrawToolTipSentenceWithTextToRightSize(menu->state.left_characters, menu->state.right_characters, &tool_tip_data);
					}
					// The viewport offset must be also substracted
					window_dimensions.y -= 2.0f * system->m_descriptors.misc.tool_tip_padding.y + tool_tip_data.next_row_offset + system->m_descriptors.dockspaces.viewport_padding_y;
					window_dimensions.x -= 2.0f * system->m_descriptors.misc.tool_tip_padding.x - 2.0f * system->m_descriptors.element_descriptor.label_padd.x - system->m_descriptors.misc.menu_x_padd;

					float arrow_span = system->GetTextSpan(">", system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.font.size), system->m_descriptors.font.size, system->m_descriptors.font.character_spacing).x;
					window_dimensions.x += (menu->state.row_has_submenu != nullptr) * (arrow_span + system->m_descriptors.element_descriptor.label_padd.x);

					UIWindowDescriptor window_descriptor;
					window_descriptor.draw = MenuDraw;
					window_descriptor.initial_position_x = position.x;
					window_descriptor.initial_size_x = window_dimensions.x;
					if (position.y + scale.y + window_dimensions.y + ECS_TOOLS_UI_MENU_PADDING > 1.0f) {
						window_descriptor.initial_position_y = 1.0f - window_dimensions.y - ECS_TOOLS_UI_MENU_PADDING;
					}
					else {
						window_descriptor.initial_position_y = position.y + scale.y + ECS_TOOLS_UI_MENU_PADDING;
					}

					UIDrawerMenuDrawWindowData window_data;
					window_data.menu = menu;
					window_data.submenu_offsets[0] = -1;

					window_descriptor.initial_size_y = window_dimensions.y;
					window_descriptor.window_name = menu->state.left_characters;
					window_descriptor.window_data = &window_data;
					window_descriptor.window_data_size = sizeof(window_data);

					system->CreateWindowAndDockspace(window_descriptor, UI_DOCKSPACE_FIXED | UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X
						| UI_DOCKSPACE_BORDER_FLAG_NO_TITLE | UI_DOCKSPACE_BORDER_FLAG_COLLAPSED_REGION_HEADER | UI_DOCKSPACE_POP_UP_WINDOW);
					menu->windows[0] = { window_descriptor.window_name, { window_descriptor.initial_position_x, window_descriptor.initial_position_y }, { window_descriptor.initial_size_x, window_descriptor.initial_size_y } };
					menu->windows.size = 1;
				}
			}
			else {
				// Because of DLL conflicts, the function pointer might point to distinct addresses but 
				// it is the same function, which will not pass the additional data test
				if (additional_data == nullptr || (additional_data != nullptr && additional_data->menu_initializer_index == 255)) {
					// So test every window that has been spawned to see if clicked inside it
					for (size_t index = 0; index < menu->windows.size; index++) {
						if (IsPointInRectangle(mouse_position, menu->windows[index].position, menu->windows[index].scale)) {
							return;
						}
					}

					UIDrawerMenuCleanupSystemHandlerData cleanup_data;
					cleanup_data.window_count = menu->windows.size;
					for (size_t index = 0; index < cleanup_data.window_count; index++) {
						cleanup_data.window_names[index] = menu->windows[index].name;
					}
					system->PushFrameHandler({ MenuCleanupSystemHandler, &cleanup_data, sizeof(cleanup_data) });
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void RightClickMenu(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerMenuRightClickData* data = (UIDrawerMenuRightClickData*)_data;
			if (mouse_tracker->RightButton() == MBRELEASED) {
				if (data->action != nullptr) {
					if (data->is_action_data_ptr) {
						action_data->data = data->action_data_ptr;
					}
					else {
						action_data->data = data->action_data;
					}
					data->action(action_data);
				}

				if (system->GetWindowFromName(data->state.left_characters) != -1) {
					// mouse position -2.0f -2.0f ensures that it will not fall into an existing 
					// window and avoid destroying the windows
					system->HandleFocusedWindowCleanupGeneral({ -2.0f, -2.0f }, 0);
					system->DeallocateGeneralHandler();
					system->m_focused_window_data.ResetGeneralHandler();
					// The window destruction handler and the release handlers are somewhere here
					system->HandleFrameHandlers();
				}

				unsigned int window_index = data->window_index;
				UIDrawerDescriptor descriptor = system->GetDrawerDescriptor(window_index);
				descriptor.do_not_initialize_viewport_sliders = true;
				UIDrawer drawer = UIDrawer(
					descriptor,
					nullptr,
					true
				);

				UIDrawConfig config;
				drawer.Menu(UI_CONFIG_DO_CACHE, config, data->name, &data->state);

				UIDrawerMenuGeneralData* general_data = (UIDrawerMenuGeneralData*)system->m_memory->Allocate(sizeof(UIDrawerMenuGeneralData));
				general_data->menu_initializer_index = 255;
				general_data->initialize_from_right_click = true;
				general_data->menu = (UIDrawerMenu*)drawer.GetResource(data->name);
				general_data->is_opened_when_clicked = false;

				system->SetNewFocusedDockspaceRegion(dockspace, border_index, dockspace_type);
				system->SetNewFocusedDockspace(dockspace, dockspace_type);

				// call the general handler if it wants to have destruction
				system->HandleFocusedWindowCleanupGeneral(mouse_position, 0);

				UIActionHandler handler = { MenuGeneral, general_data, sizeof(*general_data), ECS_UI_DRAW_SYSTEM };
				system->DeallocateGeneralHandler();
				system->m_focused_window_data.ChangeGeneralHandler(position, scale, &handler);

				action_data->data = general_data;
				action_data->position.x = mouse_position.x;
				// small padd in order to have IsPointInRectangle detect it
				action_data->position.y = mouse_position.y - action_data->scale.y + 0.0001f;
				MenuGeneral(action_data);

				UIDrawerRightClickMenuSystemHandlerData release_data;

				release_data.menu_resource_name = data->name;
				release_data.parent_window_name = system->m_windows[data->window_index].name;
				release_data.menu_window_name = data->state.left_characters;

				system->AddFrameHandler({ RightClickMenuReleaseResource, &release_data, sizeof(release_data) });
			}

		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename DataType>
		void FilterMenuDrawInternal(void* window_data, void* drawer_descriptor, bool initialize) {
			UI_PREPARE_DRAWER(initialize);

			DataType* data = (DataType*)window_data;
			drawer.DisablePaddingForRenderRegion();
			drawer.DisablePaddingForRenderSliders();
			drawer.DisableZoom();

			UIDrawConfig config;
			bool should_notify = data->notifier != nullptr;
			if (should_notify) {
				UIConfigStateTableNotify notify;
				notify.notifier = data->notifier;
				config.AddFlag(notify);
			}

			size_t STATE_TABLE_CONFIGURATION = UI_CONFIG_STATE_TABLE_NO_NAME;
			STATE_TABLE_CONFIGURATION |= data->draw_all ? UI_CONFIG_STATE_TABLE_ALL : 0;
			STATE_TABLE_CONFIGURATION |= should_notify ? UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE : 0;
			drawer.StateTable(STATE_TABLE_CONFIGURATION, config, data->name, data->labels, data->states);
		}

		// --------------------------------------------------------------------------------------------------------------

		void FilterMenuDraw(void* window_data, void* drawer_descriptor, bool initialize) {
			FilterMenuDrawInternal<UIDrawerFilterMenuData>(window_data, drawer_descriptor, initialize);
		}

		// --------------------------------------------------------------------------------------------------------------

		void FilterMenuSinglePointerDraw(void* window_data, void* drawer_descriptor, bool initialize) {
			FilterMenuDrawInternal<UIDrawerFilterMenuSinglePointerData>(window_data, drawer_descriptor, initialize);
		}

		// --------------------------------------------------------------------------------------------------------------

		void PathInputFilesystemDraw(void* window_data, void* drawer_descriptor, bool initialize) {
			UI_PREPARE_DRAWER(initialize);

			UIDrawerPathInputFolderWindowData* data = (UIDrawerPathInputFolderWindowData*)window_data;
			// Look to see if the original window still exists
			unsigned int parent_window_index = drawer.system->GetWindowFromName(data->window_bind);
			if (parent_window_index == -1) {
				// It was deleted, delete this one aswell by pushing a frame handler
				drawer.system->PushDestroyWindowHandler(drawer.window_index);
			}
			else {
				UIConfigPathInputCustomFilesystemDrawData draw_data;
				draw_data.drawer = &drawer;
				draw_data.input = data->input;
				draw_data.path = data->path;
				draw_data.should_destroy = false;

				ActionData action_data = drawer.GetDummyActionData();
				action_data.additional_data = &draw_data;
				action_data.data = data->draw_handler.data;
				data->draw_handler.action(&action_data);

				if (draw_data.should_destroy) {
					drawer.system->PushDestroyWindowHandler(drawer.window_index);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void PathInputFolderLaunchCustomFilesystem(ActionData* action_data) {
			const float2 WINDOW_SIZE = { 0.6f, 0.6f };

			UI_UNPACK_ACTION_DATA;

			UIDrawerPathInputFolderActionData* data = (UIDrawerPathInputFolderActionData*)_data;

			size_t window_data_bytes[256];
			UIDrawerPathInputFolderWindowData* window_data = (UIDrawerPathInputFolderWindowData*)window_data_bytes;
			char* child_window_name = (char*)function::OffsetPointer(window_data, sizeof(UIDrawerPathInputFolderWindowData));
			*window_data = { data->input, data->path, data->custom_handler, child_window_name };

			unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
			Stream<char> window_name = system->GetWindowName(window_index);
			window_name.CopyTo(child_window_name);

			UIWindowDescriptor window_descriptor;

			window_descriptor.window_name = "Select a path";
			window_descriptor.draw = PathInputFilesystemDraw;
			window_descriptor.window_data = &window_data;
			window_descriptor.window_data_size = sizeof(*window_data) + window_name.size;

			window_descriptor.initial_size_x = WINDOW_SIZE.x;
			window_descriptor.initial_size_y = WINDOW_SIZE.y;

			system->CreateWindowAndDockspace(window_descriptor, UI_DOCKSPACE_POP_UP_WINDOW | UI_POP_UP_WINDOW_FIT_TO_CONTENT_ADD_RENDER_SLIDER_SIZE 
				| UI_POP_UP_WINDOW_FIT_TO_CONTENT_CENTER | UI_DOCKSPACE_NO_DOCKING);
		}

		void PathInputFolderAction(ActionData* action_data, Action default_action)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerPathInputFolderActionData* data = (UIDrawerPathInputFolderActionData*)_data;

			if (data->custom_handler.action != nullptr) {
				PathInputFolderLaunchCustomFilesystem(action_data);
			}
			else {
				// Call the OS file explorer
				default_action(action_data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void FileInputFolderAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerPathInputFolderActionData* data = (UIDrawerPathInputFolderActionData*)_data;
			OSFileExplorerGetFileData get_data;

			const unsigned int FOLDER_PATH_MAX_SIZE = 512;
			wchar_t folder_path[FOLDER_PATH_MAX_SIZE];

			const unsigned int ERROR_MESSAGE_SIZE = 128;
			char error_message[ERROR_MESSAGE_SIZE];
			get_data.path = *data->path;
			get_data.error_message.InitializeFromBuffer(error_message, 0, ERROR_MESSAGE_SIZE);
			get_data.extensions = data->extensions;

			if (!FileExplorerGetFile(&get_data)) {
				CreateErrorMessageWindow(system, get_data.error_message);
			}
			else {
				bool is_valid = true;
				if (data->roots.size > 0) {
					// Check to see if it belongs to a root
					size_t index = 0;
					for (; index < data->roots.size; index++) {
						if (data->roots[index].size < get_data.path.size && memcmp(get_data.path.buffer, data->roots[index].buffer, sizeof(wchar_t) * data->roots[index].size) == 0) {
							// Exit the loop and signal that a match was found
							index = -2;
						}
					}
					is_valid = index == -1;
				}

				if (is_valid) {
					// Update the input
					data->input->DeleteAllCharacters();
					ECS_STACK_CAPACITY_STREAM(char, temp_conversion_characters, FOLDER_PATH_MAX_SIZE);
					function::ConvertWideCharsToASCII(get_data.path, temp_conversion_characters);
					data->input->InsertCharacters(temp_conversion_characters.buffer, temp_conversion_characters.size, 0, system);
					data->path->size = get_data.path.size;
					if (data->path->size < data->path->capacity) {
						data->path->buffer[data->path->size] = L'\0';
					}
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void DirectoryInputFolderAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerPathInputFolderActionData* data = (UIDrawerPathInputFolderActionData*)_data;
			OSFileExplorerGetDirectoryData get_data;

			const unsigned int FOLDER_PATH_MAX_SIZE = 512;
			wchar_t folder_path[FOLDER_PATH_MAX_SIZE];
			
			const unsigned int ERROR_MESSAGE_SIZE = 128;
			char error_message[ERROR_MESSAGE_SIZE];
			get_data.path = *data->path;
			get_data.path.size = 0;
			get_data.error_message.InitializeFromBuffer(error_message, 0, ERROR_MESSAGE_SIZE);

			if (!FileExplorerGetDirectory(&get_data)) {
				CreateErrorMessageWindow(system, get_data.error_message);
			}
			else {
				bool is_valid = true;
				if (data->roots.size > 0) {
					// Check to see if it belongs to a root
					size_t index = 0;
					for (; index < data->roots.size; index++) {
						if (data->roots[index].size < get_data.path.size && memcmp(get_data.path.buffer, data->roots[index].buffer, sizeof(wchar_t) * data->roots[index].size) == 0) {
							// Exit the loop and signal that a match was found
							index = -2;
						}
					}
					is_valid = index == -1;
				}
				
				if (is_valid) {
					// Update the input and copy the characters
					data->input->DeleteAllCharacters();
					ECS_STACK_CAPACITY_STREAM(char, temp_conversion_characters, FOLDER_PATH_MAX_SIZE);
					function::ConvertWideCharsToASCII(get_data.path, temp_conversion_characters);
					data->input->InsertCharacters(temp_conversion_characters.buffer, temp_conversion_characters.size, 0, system);
					data->path->size = get_data.path.size;
					if (data->path->size < data->path->capacity && data->path->buffer != nullptr) {
						data->path->buffer[data->path->size] = L'\0';
					}
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		struct PathInputFolderWithInputsWindowDrawData {
			Stream<Stream<wchar_t>> files;
			UIDrawerTextInput* input;
			CapacityStream<wchar_t>* path;
		};

		void PathInputFolderWithInputsWindowDraw(void* window_data, void* drawer_descriptor, bool initialize) {
			const char* FILTER_STRING_NAME = "Filter string";
			const size_t FILTER_STRING_CAPACITY = 256;

			UI_PREPARE_DRAWER(initialize);

			PathInputFolderWithInputsWindowDrawData* data = (PathInputFolderWithInputsWindowDrawData*)window_data;

			// Have a small text input for fast filtering
			CapacityStream<char>* filter_string = nullptr;
			unsigned int* active_index = nullptr;
			if (initialize) {
				size_t total_size = sizeof(CapacityStream<char>) + sizeof(char) * FILTER_STRING_CAPACITY + sizeof(unsigned int);
				void* allocation = drawer.GetMainAllocatorBufferAndStoreAsResource(FILTER_STRING_NAME, total_size);
				
				filter_string = (CapacityStream<char>*)allocation;
				filter_string->buffer = (char*)function::OffsetPointer(allocation, sizeof(CapacityStream<char>) + sizeof(unsigned int));
				filter_string->size = 0;
				filter_string->capacity = FILTER_STRING_CAPACITY;

				active_index = (unsigned int*)function::OffsetPointer(allocation, sizeof(CapacityStream<char>));
			}
			else {
				filter_string = (CapacityStream<char>*)drawer.GetResource(FILTER_STRING_NAME);
				active_index = (unsigned int*)function::OffsetPointer(filter_string, sizeof(CapacityStream<char>));
			}

			ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, filtered_files, data->files.size);
			// If the filter string is not empty, filter the files
			if (filter_string->size > 0) {
				ECS_STACK_CAPACITY_STREAM(wchar_t, null_terminated_path, 512);
				ECS_STACK_CAPACITY_STREAM(wchar_t, wide_filter_string, FILTER_STRING_CAPACITY);
				function::ConvertASCIIToWide(wide_filter_string, *filter_string);
				wide_filter_string.Add(L'\0');

				for (unsigned int index = 0; index < data->files.size; index++) {
					// Use wcsstr, check to see if the files is null terminated
					const wchar_t* search_string = data->files[index].buffer;
					if (data->files[index][data->files[index].size] != L'\0') {
						null_terminated_path.Copy(data->files[index]);
						null_terminated_path.Add(L'\0');
						search_string = null_terminated_path.buffer;
					}

					if (wcsstr(search_string, wide_filter_string.buffer) != nullptr) {
						filtered_files.Add(index);
					}
				}
			}
			else {
				// Fill in the filtered_files with the whole indices
				filtered_files.size = data->files.size;
				function::MakeSequence(filtered_files);
			}

			// Display the filter bar
			UIDrawConfig config;
			UIConfigWindowDependentSize dependent_size;
			config.AddFlag(dependent_size);
			
			drawer.TextInput(UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Filter String", filter_string);
			drawer.NextRow();

			struct SelectLabelActionData {
				unsigned int* active_index;
				unsigned int current_index;
			};

			auto select_label_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				SelectLabelActionData* data = (SelectLabelActionData*)_data;
				*data->active_index = data->current_index;
			};

			// Draw each path
			ECS_STACK_CAPACITY_STREAM(char, converted_path, 512);
			for (unsigned int index = 0; index < filtered_files.size; index++) {
				bool is_active = *active_index == filtered_files[index];
				size_t configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE;
				configuration |= is_active ? 0 : UI_CONFIG_LABEL_TRANSPARENT;

				converted_path.size = 0;
				function::ConvertWideCharsToASCII(data->files[filtered_files[index]], converted_path);
				converted_path.AddSafe('\0');

				SelectLabelActionData action_data;
				action_data.active_index = active_index;
				action_data.current_index = filtered_files[index];
				drawer.Button(configuration, config, converted_path.buffer, { select_label_action, &action_data, sizeof(action_data) });
				drawer.NextRow();
			}

			// The ok and cancel buttons
			struct OKButtonData {
				PathInputFolderWithInputsWindowDrawData* draw_data;
				unsigned int* active_index;
			};
			auto ok_button_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				OKButtonData* data = (OKButtonData*)_data;
				if (*data->active_index != -1 && *data->active_index < data->draw_data->files.size) {
					data->draw_data->input->DeleteAllCharacters();

					ECS_STACK_CAPACITY_STREAM(char, converted_stream, 512);
					function::ConvertWideCharsToASCII(data->draw_data->files[*data->active_index], converted_stream);
					data->draw_data->input->InsertCharacters(converted_stream.buffer, converted_stream.size, 0, system);
					data->draw_data->path->Copy(data->draw_data->files[*data->active_index]);
					if (data->draw_data->path->size < data->draw_data->path->capacity) {
						data->draw_data->path->buffer[data->draw_data->path->size] = L'\0';
					}
				}
				CloseXBorderClickableAction(action_data);
			};

			auto cancel_button_action = [](ActionData* action_data) {
				CloseXBorderClickableAction(action_data);
			};

			const char* OK_STRING = "OK";
			UIDrawConfig ok_cancel_config;
			UIConfigAbsoluteTransform absolute_transform;
			absolute_transform.scale = drawer.GetLabelScale(OK_STRING);
			absolute_transform.position = drawer.GetAlignedToBottom(absolute_transform.scale.y);
			ok_cancel_config.AddFlag(absolute_transform);

			OKButtonData ok_data = { data, active_index };
			drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, ok_cancel_config, OK_STRING, { ok_button_action, &ok_data, sizeof(ok_data), ECS_UI_DRAW_PHASE::ECS_UI_DRAW_SYSTEM });
			ok_cancel_config.flag_count--;

			const char* CANCEL_STRING = "Cancel";
			absolute_transform.scale = drawer.GetLabelScale(CANCEL_STRING);
			absolute_transform.position.x = drawer.GetAlignedToRight(absolute_transform.scale.x).x;

			ok_cancel_config.AddFlag(absolute_transform);
			drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, ok_cancel_config, CANCEL_STRING, { cancel_button_action, nullptr, 0, ECS_UI_DRAW_PHASE::ECS_UI_DRAW_SYSTEM });
		}

		// --------------------------------------------------------------------------------------------------------------

		void PathInputFolderWithInputsAction(ActionData* action_data)
		{
			const float2 WINDOW_SIZE = { 0.6f, 0.6f };

			UI_UNPACK_ACTION_DATA;
			UIDrawerPathInputWithInputsActionData* data = (UIDrawerPathInputWithInputsActionData*)_data;

			size_t memory_usage = 0;
			Stream<Stream<wchar_t>> path_stream = { nullptr, 0 };

			const size_t LINEAR_ALLOCATOR_CAPACITY = sizeof(wchar_t) * ECS_KB * 128;
			void* stack_buffer = ECS_STACK_ALLOC(LINEAR_ALLOCATOR_CAPACITY);
			LinearAllocator allocator(stack_buffer, LINEAR_ALLOCATOR_CAPACITY);

			if (data->is_callback) {
				// Call the callback - it should take as parameter a resizable stream ResizbleStream<Stream<wchar_t>> and 
				// allocate the strings from it such that they are visible in this scope
				ResizableStream<Stream<wchar_t>> resizable_path_stream(GetAllocatorPolymorphic(&allocator), 0);

				action_data->data = data->callback_handler.data;
				action_data->additional_data = &resizable_path_stream;
				data->callback_handler.action(action_data);

				memory_usage = allocator.m_top;
				path_stream = { resizable_path_stream.buffer, resizable_path_stream.size };
			}
			else {
				memory_usage = sizeof(Stream<wchar_t>) * data->files.size;
				for (size_t index = 0; index < data->files.size; index++) {
					memory_usage += data->files[index].size * sizeof(wchar_t);
				}

				path_stream = data->files;
			}


			PathInputFolderWithInputsWindowDrawData* window_data;
			// At first just give dummy data. The reference will be restored afterwards
			PathInputFolderWithInputsWindowDrawData dummy_window_data;
			dummy_window_data.input = data->input;
			dummy_window_data.path = data->path;

			UIWindowDescriptor descriptor;
			descriptor.draw = PathInputFolderWithInputsWindowDraw;

			descriptor.window_data = &dummy_window_data;
			descriptor.window_data_size = sizeof(dummy_window_data);

			descriptor.initial_size_x = WINDOW_SIZE.x;
			descriptor.initial_size_y = WINDOW_SIZE.y;

			descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
			descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);

			descriptor.window_name = "Select a path";
			unsigned int window_index = system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_POP_UP_WINDOW | UI_DOCKSPACE_NO_DOCKING);
			// Get the window data
			window_data = (PathInputFolderWithInputsWindowDrawData*)system->GetWindowData(window_index);

			// Coallesce the memory into a single block
			void* persistent_buffer = system->m_memory->Allocate(memory_usage);
			Stream<wchar_t>* paths = (Stream<wchar_t>*)persistent_buffer;

			uintptr_t ptr = (uintptr_t)function::OffsetPointer(persistent_buffer, sizeof(Stream<wchar_t>) * path_stream.size);
			for (size_t index = 0; index < path_stream.size; index++) {
				paths[index].InitializeAndCopy(ptr, path_stream[index]);
			}
			ECS_ASSERT(ptr - (uintptr_t)paths <= memory_usage);
			window_data->files = { paths, path_stream.size };

			// Add the allocation to the window allocation list
			system->AddWindowMemoryResource(persistent_buffer, window_index);
		}

		// --------------------------------------------------------------------------------------------------------------

		void LabelHierarchyChangeState(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			if (IsClickableTrigger(action_data)) {
				LabelHierarchyChangeStateData* data = (LabelHierarchyChangeStateData*)_data;
				Stream<char> label = data->GetCurrentLabel();

				data->hierarchy_data->ChangeSelection(label, action_data);

				unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
				unsigned int dynamic_index = -1;
				if (data->hierarchy_data->identifier.size > 0) {
					dynamic_index = system->GetWindowDynamicElement(window_index, data->hierarchy_data->identifier);
				}

				// Look to see if we need to remove it or add it
				unsigned int opened_index = function::FindString(label, data->hierarchy_data->opened_labels);

				const void* old_buffer = data->hierarchy_data->opened_labels.buffer;
				if (opened_index == -1) {
					// Allocate the label into the opened stream
					void* allocation = system->m_memory->Allocate(label.size * sizeof(char), alignof(char));
					label.CopyTo(allocation);
					data->hierarchy_data->opened_labels.size++;
					void* new_buffer_allocation = system->m_memory->Allocate(sizeof(Stream<char>) * data->hierarchy_data->opened_labels.size);

					if (data->hierarchy_data->opened_labels.size > 1) {
						memcpy(new_buffer_allocation, old_buffer, sizeof(Stream<char>) * (data->hierarchy_data->opened_labels.size - 1));
						// Replace the buffer
						system->ReplaceWindowMemoryResource(window_index, old_buffer, new_buffer_allocation);
						if (dynamic_index != -1) {
							system->ReplaceWindowDynamicResourceAllocation(window_index, dynamic_index, old_buffer, new_buffer_allocation);
						}

						data->hierarchy_data->opened_labels.buffer = (Stream<char>*)new_buffer_allocation;
					}
					else {
						// Must add this allocation
						system->AddWindowMemoryResource(new_buffer_allocation, window_index);
						if (dynamic_index != -1) {
							system->AddWindowDynamicElementAllocation(window_index, dynamic_index, new_buffer_allocation);
						}
					}
				}
				else {
					// Remove it or replace it
					data->hierarchy_data->opened_labels.RemoveSwapBack(opened_index);
					if (data->hierarchy_data->opened_labels.size > 0) {
						// Replace
						void* new_buffer_allocation = system->m_memory->Allocate(sizeof(Stream<char>) * data->hierarchy_data->opened_labels.size);
						data->hierarchy_data->opened_labels.CopyTo(new_buffer_allocation);
						system->ReplaceWindowMemoryResource(window_index, old_buffer, new_buffer_allocation);
						if (dynamic_index != -1) {
							system->ReplaceWindowDynamicResourceAllocation(window_index, dynamic_index, old_buffer, new_buffer_allocation);
						}
						data->hierarchy_data->opened_labels.buffer = (Stream<char>*)new_buffer_allocation;
					}
					else {
						// Remove
						system->RemoveWindowMemoryResource(window_index, data->hierarchy_data->opened_labels.buffer);
						if (dynamic_index != -1) {
							system->RemoveWindowDynamicResourceAllocation(window_index, dynamic_index, data->hierarchy_data->opened_labels.buffer);
						}
					}
				}

				system->m_memory->Deallocate(old_buffer);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void LabelHierarchyRenameLabel(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;
		
			UIDrawerLabelHierarchyData* data = (UIDrawerLabelHierarchyData*)_data;
			UIDrawerLabelHierarchyRenameData rename_data;
			rename_data.data = data->rename_data;
			rename_data.new_label = data->rename_label;
			rename_data.previous_label = data->selected_labels[0];

			action_data->data = &rename_data;
			data->rename_action(action_data);

			data->is_rename_label = false;
		}

		// --------------------------------------------------------------------------------------------------------------

		void LabelHierarchyClickAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			const size_t double_click_milliseconds = 50;
			const size_t drag_milliseconds = 100;

			LabelHierarchyClickActionData* data = (LabelHierarchyClickActionData*)_data;
			LabelHierarchyClickActionData* additional_data = (LabelHierarchyClickActionData*)_additional_data;

			system->SetCleanupGeneralHandler();

			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
				Stream<char> label = data->GetCurrentLabel();

				if (mouse_tracker->LeftButton() == MBPRESSED) {
					if (UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
						// If it is the same selected label, proceed with the double click action
						if (data->hierarchy_data->selected_labels.size == 1 && function::CompareStrings(data->hierarchy_data->selected_labels[0], label)) {
							size_t duration = additional_data->timer.GetDuration_ms();
							if (duration < double_click_milliseconds) {
								if (additional_data->click_count == 0) {
									if (data->hierarchy_data->double_click_action != nullptr) {
										// Call the double click action - if any
										UIDrawerLabelHierarchyDoubleClickData double_click_data;
										double_click_data.data = data->hierarchy_data->double_click_data;
										double_click_data.label = data->hierarchy_data->selected_labels[0];
										action_data->data = &double_click_data;
										data->hierarchy_data->double_click_action(action_data);
									}
									data->click_count = 1;
								}
								else if (additional_data->click_count == 1) {
									// The rename handler can now be called
									if (data->hierarchy_data->rename_action != nullptr) {
										data->hierarchy_data->is_rename_label = true;
									}
									data->click_count = 0;
								}
							}
							else {
								data->click_count = 0;
							}
						}
						else {
							data->click_count = 0;
						}
					}

					// If ctrl is pressed, then add/remove else change the selected label
					// If shift is pressed, activate the determine_selection
					if (keyboard->IsKeyDown(HID::Key::LeftShift)) {
						data->hierarchy_data->determine_selection = true;
						data->hierarchy_data->last_selected.Copy(label);
					}
					else if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
						unsigned int index = function::FindString(label, data->hierarchy_data->selected_labels);
						if (index != -1) {
							// Remove it
							data->hierarchy_data->RemoveSelection(label, action_data);
						}
						else {
							// Add it
							data->hierarchy_data->AddSelection(label, action_data);
						}
					}
					else {
						data->hierarchy_data->ChangeSelection(label, action_data);
					}
				}
				else {
					if (data->hierarchy_data->drag_action != nullptr) {
						if (data->timer.GetDuration_ms() >= drag_milliseconds && !IsPointInRectangle(mouse_position, position, scale)) {
							if (mouse_tracker->LeftButton() == MBHELD) {
								data->hierarchy_data->is_dragging = true;
							}
							else if (mouse_tracker->LeftButton() == MBRELEASED) {
								data->hierarchy_data->is_dragging = false;
								// Call the drag handler now, only if a label if hovered
								if (data->hierarchy_data->hovered_label.size > 0) {
									if (!data->hierarchy_data->reject_same_label_drag || function::FindString(label, data->hierarchy_data->selected_labels) == -1) {
										UIDrawerLabelHierarchyDragData drag_data;
										drag_data.data = data->hierarchy_data->drag_data;
										drag_data.destination_label = data->hierarchy_data->hovered_label;
										drag_data.source_label = data->hierarchy_data->selected_labels;

										action_data->data = &drag_data;
										data->hierarchy_data->drag_action(action_data);
									}
								}
							}
						}
					}
				}
			}
			else {
				data->hierarchy_data->ClearSelection(action_data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void LabelHierarchyRightClickAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerLabelHierarchyRightClickData* data = (UIDrawerLabelHierarchyRightClickData*)_data;

			// Change the selected label to this one
			Stream<char> label = data->GetLabel();
			unsigned int selected_index = function::FindString(label, data->hierarchy->selected_labels);
			if (selected_index == -1) {
				data->hierarchy->ChangeSelection(label, action_data);
			}

			data->data = data->hierarchy->right_click_data;
			data->hierarchy->right_click_action(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		// --------------------------------------------------------------------------------------------------------------

#undef EXPORT

	}

}