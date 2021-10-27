#pragma once
#include "UIDrawerActions.h"
#include "UIResourcePaths.h"

namespace ECSEngine {

	namespace Tools {

		ECS_CONTAINERS;

		// --------------------------------------------------------------------------------------------------------------

		// Filter will contain a static method that will validate characters
		// It must take as parameters the character and it's type for faster parsing
		template<typename Filter>
		void TextInputAction(ActionData* action_data) {
			constexpr size_t revert_command_stack_size = 512;
			UI_UNPACK_ACTION_DATA;

			UIDrawerTextInput* data = (UIDrawerTextInput*)_data;

			UIDrawerTextInput* additional_data = (UIDrawerTextInput*)_additional_data;
			system->m_focused_window_data.clean_up_call_general = true;

			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL && !keyboard_tracker->IsKeyPressed(HID::Key::Enter)) {
				keyboard->CaptureCharacters();
				unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
				data->is_currently_selected = true;

				auto left_arrow_lambda = [&]() {
					if (keyboard->IsKeyUp(HID::Key::LeftShift) && data->current_sprite_position > data->current_selection) {
						unsigned int difference = data->current_sprite_position - data->current_selection;
						data->current_sprite_position = data->current_selection;
						if (data->sprite_render_offset < difference) {
							data->sprite_render_offset = 0;
						}
						else {
							data->sprite_render_offset -= difference;
						}
					}
					else {
						if (data->current_sprite_position > 0) {
							if (data->current_sprite_position == data->sprite_render_offset + 1 && data->sprite_render_offset > 0)
								data->sprite_render_offset--;
							data->current_sprite_position--;
						}
						else {
							if (data->sprite_render_offset == 1)
								data->sprite_render_offset--;
						}
						if (keyboard->IsKeyUp(HID::Key::LeftShift)) {
							data->current_selection = data->current_sprite_position;
						}
					}
					data->caret_start = std::chrono::high_resolution_clock::now();
					data->is_caret_display = true;
				};
				auto right_arrow_lambda = [&]() {
					unsigned int visible_sprites = data->GetVisibleSpriteCount(data->bound);
					if (keyboard->IsKeyUp(HID::Key::LeftShift) && data->current_sprite_position < data->current_selection) {
						data->current_sprite_position = data->current_selection;
						unsigned int visible_sprites = data->GetVisibleSpriteCount(data->bound);
						while (data->sprite_render_offset + visible_sprites < data->current_sprite_position) {
							data->sprite_render_offset++;
							visible_sprites = data->GetVisibleSpriteCount(data->bound);
						}
					}
					else {
						if (data->current_sprite_position < data->text->size) {
							if (data->current_sprite_position == data->sprite_render_offset + visible_sprites - 1 && data->sprite_render_offset + visible_sprites < data->text->size) {
								data->sprite_render_offset++;
								visible_sprites = data->GetVisibleSpriteCount(data->bound);
								data->current_sprite_position = data->sprite_render_offset + visible_sprites - 1;
							}
							else {
								data->current_sprite_position++;
							}
						}
						if (keyboard->IsKeyUp(HID::Key::LeftShift)) {
							data->current_selection = data->current_sprite_position;
						}
					}
					data->caret_start = std::chrono::high_resolution_clock::now();
					data->is_caret_display = true;
				};

				unsigned char command[128];
				UIDrawerTextInputRemoveCommandInfo* command_info = (UIDrawerTextInputRemoveCommandInfo*)command;

				unsigned int data_size = sizeof(UIDrawerTextInputRemoveCommandInfo);
				unsigned int* backspace_text_count = &command_info->text_count;
				unsigned int* backspace_text_position = &command_info->text_position;
				char* info_text = (char*)((uintptr_t)command_info + data_size);
				char* backspace_text = info_text;
				command_info->input = data;

				bool is_backspace_lambda = false;
				auto backspace_lambda = [&]() {
					is_backspace_lambda = data->Backspace(backspace_text_count, backspace_text_position, backspace_text);
				};

				if (data->filter_character_count > 0) {
					size_t valid_characters = 0;

					float push_offsets[256] = { 0 };
					for (size_t index = 0; index < data->filter_character_count; index++) {
						CharacterType type = CharacterType::Unknown;

						size_t current_index = data->filter_characters_start + index;
						system->FindCharacterUVFromAtlas(data->text->buffer[current_index], type);
						bool is_valid = Filter::Filter(data->text->buffer[current_index], type);
						size_t current_sprite_index = current_index * 6;

						if (index > 0) {
							push_offsets[index] = push_offsets[index - 1] + !is_valid * (data->vertices[current_sprite_index + 1].position.x - data->vertices[current_sprite_index].position.x);
						}

						if (is_valid && index != valid_characters) {
							size_t filter_index = data->filter_characters_start + valid_characters;
							size_t filter_sprite_index = filter_index * 6;
							data->text->buffer[data->filter_characters_start + valid_characters] = data->text->buffer[current_index];

							for (size_t subindex = 0; subindex < 6; subindex++) {
								data->vertices[filter_sprite_index + subindex] = data->vertices[current_sprite_index + subindex];
								data->vertices[filter_sprite_index + subindex].position.x -= push_offsets[index - 1];
							}
						}
						valid_characters += is_valid;
					}
					push_offsets[0] = 0.0f;

					size_t push_count = data->filter_character_count - valid_characters;
					if (push_count > 0) {
						for (size_t index = data->filter_characters_start + data->filter_character_count; index < data->text->size; index++) {
							data->text->buffer[index - push_count] = data->text->buffer[index];

							for (size_t subindex = 0; subindex < 6; subindex++) {
								data->vertices[(index - push_count) * 6 + subindex] = data->vertices[index * 6];
								data->vertices[(index - push_count) * 6 + subindex].position.x -= push_offsets[data->filter_character_count];
							}
						}
						data->text->size -= push_count;
						data->vertices.size -= push_count * 6;

						HandlerCommand last_command;
						system->GetLastRevertCommand(last_command, window_index);

						if (last_command.type == HandlerCommandType::TextReplace) {
							UIDrawerTextInputReplaceCommandInfo* command = (UIDrawerTextInputReplaceCommandInfo*)last_command.handler.data;
							command->text_count -= push_count;
						}
					}

					data->filter_characters_start = 0;
					data->filter_character_count = 0;
					data->current_selection = function::Select<unsigned int>(data->current_selection > data->text->size, data->text->size, data->current_selection);
					data->current_sprite_position = function::Select<unsigned int>(data->current_sprite_position > data->text->size, data->text->size, data->current_sprite_position);
				}

				if (keyboard->IsKeyUp(HID::Key::LeftControl) && keyboard->IsKeyUp(HID::Key::RightControl)) {
					char characters[64];
					size_t character_count = 0;
					while (keyboard->GetCharacter(characters[character_count++])) {
						CharacterType type;
						system->FindCharacterUVFromAtlas(characters[character_count - 1], type);
						if (type == CharacterType::Unknown) {
							character_count--;
						}
						else {
							character_count -= function::Select(Filter::Filter(characters[character_count - 1], type), 0, 1);
						}
					}
					character_count--;

					if (character_count > 0) {
						if (data->current_sprite_position != data->current_selection) {
							backspace_lambda();

							size_t total_size = sizeof(UIDrawerTextInputRemoveCommandInfo) + *backspace_text_count + 1;
							UIDrawerTextInputRemoveCommandInfo* remove_info = (UIDrawerTextInputRemoveCommandInfo*)system->m_memory->Allocate(total_size);
							system->AddWindowMemoryResource(remove_info, window_index);

							remove_info->input = data;
							remove_info->text_position = *backspace_text_position;
							remove_info->text_count = *backspace_text_count;
							remove_info->deallocate_data = true;
							remove_info->deallocate_buffer = remove_info;
							unsigned char* info_text = (unsigned char*)((uintptr_t)remove_info + sizeof(UIDrawerTextInputRemoveCommandInfo));
							memcpy(info_text, backspace_text, *backspace_text_count);
							info_text[*backspace_text_count] = '\0';

							AddWindowHandleCommand(system, window_index, TextInputRevertRemoveText, remove_info, total_size, HandlerCommandType::TextRemove);
						}

						size_t text_size = data->text->size;

						if (data->text->size + character_count <= data->text->capacity) {
							unsigned char revert_info_data[revert_command_stack_size];
							UIDrawerTextInputAddCommandInfo* revert_info = (UIDrawerTextInputAddCommandInfo*)revert_info_data;
							revert_info->input = data;
							revert_info->text_count = character_count;
							revert_info->text_position = data->current_sprite_position;

							AddWindowHandleCommand(system, window_index, TextInputRevertAddText, revert_info, sizeof(UIDrawerTextInputAddCommandInfo), HandlerCommandType::TextAdd);

							for (int64_t index = text_size - 1; index >= data->current_sprite_position; index--) {
								size_t sprite_index = index * 6;
								size_t new_sprite_index = sprite_index + character_count * 6;
								data->vertices[new_sprite_index] = data->vertices[sprite_index];
								data->vertices[new_sprite_index + 1] = data->vertices[sprite_index + 1];
								data->vertices[new_sprite_index + 2] = data->vertices[sprite_index + 2];
								data->vertices[new_sprite_index + 3] = data->vertices[sprite_index + 3];
								data->vertices[new_sprite_index + 4] = data->vertices[sprite_index + 4];
								data->vertices[new_sprite_index + 5] = data->vertices[sprite_index + 5];
								data->text->buffer[index + character_count] = data->text->buffer[index];
							}

							bool is_last_position = data->text->size == data->current_sprite_position;
							strncpy((char*)data->text->buffer + data->current_sprite_position, (const char*)characters, character_count);
							data->text->size += character_count;

							float2 position;

							float character_spacing = data->character_spacing;
							float2 padding = { data->padding.x, data->padding.y };
							if (data->current_sprite_position != 0) {
								const float2* current_position = &data->vertices[(data->current_sprite_position - 1) * 6 + 1].position;
								position = { current_position->x + character_spacing, -current_position->y };
							}
							else {
								position = { data->position.x + padding.x, data->position.y + padding.y };
							}
							system->ConvertCharactersToTextSprites(
								(const char*)characters,
								position,
								data->vertices.buffer,
								character_count,
								data->text_color,
								data->current_sprite_position * 6,
								data->font_size,
								character_spacing
							);
							float2 text_span = GetTextSpan(Stream<UISpriteVertex>(data->vertices.buffer + data->current_sprite_position * 6, character_count * 6));

							size_t index = 0;
							for (; index < character_count; index++) {
								size_t sprite_index = (index + data->current_sprite_position) * 6;
								if (data->vertices[sprite_index + 1].position.x > data->bound) {
									break;
								}
							}
							data->sprite_render_offset += character_count - index;
							data->current_sprite_position += character_count;

							if (is_last_position) {
								unsigned int visible_sprites = data->GetVisibleSpriteCount(data->bound);
								while (data->sprite_render_offset + visible_sprites < data->current_sprite_position) {
									data->sprite_render_offset++;
									visible_sprites = data->GetVisibleSpriteCount(data->bound);
								}
							}

							text_span.x += character_spacing;
							for (size_t index = data->current_sprite_position; index < data->text->size; index++) {
								size_t sprite_index = index * 6;
								for (size_t subindex = sprite_index; subindex < sprite_index + 6; subindex++) {
									data->vertices[subindex].position.x += text_span.x;
								}
							}
							data->vertices.size = data->text->size * 6;

						}

						data->current_selection = data->current_sprite_position;
						data->text->buffer[data->text->size] = '\0';
					}
				}

				HID::Key repeat_key = HID::Key::None;
				if (!is_backspace_lambda) {
					data->RepeatKeyAction(system, keyboard_tracker, keyboard, HID::Key::Back, repeat_key, backspace_lambda);
					data->RepeatKeyAction(system, keyboard_tracker, keyboard, HID::Key::Delete, repeat_key, backspace_lambda);

					if (is_backspace_lambda) {
						size_t total_size = sizeof(UIDrawerTextInputRemoveCommandInfo) + *backspace_text_count + 1;
						UIDrawerTextInputRemoveCommandInfo* remove_info = (UIDrawerTextInputRemoveCommandInfo*)system->m_memory->Allocate(total_size);
						system->AddWindowMemoryResource(remove_info, window_index);

						remove_info->input = data;
						remove_info->text_count = *backspace_text_count;
						remove_info->text_position = data->current_sprite_position;
						remove_info->deallocate_data = true;
						remove_info->deallocate_buffer = remove_info;
						unsigned char* info_text = (unsigned char*)((uintptr_t)remove_info + sizeof(UIDrawerTextInputRemoveCommandInfo));
						memcpy((void*)((uintptr_t)remove_info + sizeof(UIDrawerTextInputRemoveCommandInfo)), backspace_text, *backspace_text_count);
						info_text[*backspace_text_count] = '\0';

						AddWindowHandleCommand(system, window_index, TextInputRevertRemoveText, remove_info, total_size, HandlerCommandType::TextRemove);
					}
				}

				if (!data->suppress_arrow_movement) {
					if (data->repeat_key == HID::Key::Right && keyboard->IsKeyUp(HID::Key::Right)) {
						data->repeat_key = HID::Key::None;
						if (keyboard->IsKeyDown(HID::Key::Left)) {
							data->repeat_key_count = 0;
							data->repeat_key = HID::Key::Left;
							data->key_repeat_start = std::chrono::high_resolution_clock::now();
						}
					}
					else if (data->repeat_key == HID::Key::Left && keyboard->IsKeyUp(HID::Key::Left)) {
						data->repeat_key = HID::Key::None;
						if (keyboard->IsKeyDown(HID::Key::Right)) {
							data->repeat_key_count = 0;
							data->repeat_key = HID::Key::Right;
							data->key_repeat_start = std::chrono::high_resolution_clock::now();
						}
					}
					data->RepeatKeyAction(system, keyboard_tracker, keyboard, HID::Key::Left, repeat_key, left_arrow_lambda);
					data->RepeatKeyAction(system, keyboard_tracker, keyboard, HID::Key::Right, repeat_key, right_arrow_lambda);
				}

				if (keyboard->IsKeyDown(HID::Key::LeftControl)) {
					if (keyboard_tracker->IsKeyPressed(HID::Key::X)) {
						data->CopyCharacters(system);
						backspace_lambda();

						size_t total_size = sizeof(UIDrawerTextInputRemoveCommandInfo) + *backspace_text_count + 1;
						UIDrawerTextInputRemoveCommandInfo* remove_info = (UIDrawerTextInputRemoveCommandInfo*)system->m_memory->Allocate(total_size);
						system->AddWindowMemoryResource(remove_info, window_index);

						remove_info->input = data;
						remove_info->text_count = *backspace_text_count;
						remove_info->text_position = data->current_sprite_position;
						remove_info->deallocate_data = true;
						remove_info->deallocate_buffer = remove_info;
						unsigned char* info_text = (unsigned char*)((uintptr_t)remove_info + sizeof(UIDrawerTextInputRemoveCommandInfo));
						memcpy(info_text, backspace_text, *backspace_text_count);
						info_text[*backspace_text_count] = '\0';

						AddWindowHandleCommand(system, window_index, TextInputRevertRemoveText, remove_info, total_size, HandlerCommandType::TextRemove);
					}
					else if (keyboard_tracker->IsKeyPressed(HID::Key::C)) {
						data->CopyCharacters(system);
					}
					else if (keyboard_tracker->IsKeyPressed(HID::Key::V)) {
						char characters[256];
						unsigned int character_count = system->m_application->CopyTextFromClipboard(characters, 256);

						if (data->text->size + character_count <= data->text->capacity) {
							char deleted_characters[revert_command_stack_size];
							unsigned int deleted_character_count = 0;
							unsigned int delete_position = 0;
							data->PasteCharacters(characters, character_count, system, window_index, deleted_characters, &deleted_character_count, &delete_position);
							data->current_selection = delete_position;

							size_t total_size = sizeof(UIDrawerTextInputReplaceCommandInfo) + deleted_character_count + 1;
							UIDrawerTextInputReplaceCommandInfo* replace_info = (UIDrawerTextInputReplaceCommandInfo*)system->m_memory->Allocate(total_size);
							system->AddWindowMemoryResource(replace_info, window_index);

							replace_info->input = data;
							replace_info->replaced_text_count = deleted_character_count;
							char* info_text = (char*)((uintptr_t)replace_info + sizeof(UIDrawerTextInputReplaceCommandInfo));
							replace_info->text_position = delete_position;
							replace_info->text_count = character_count;
							replace_info->deallocate_buffer = replace_info;
							memcpy(info_text, deleted_characters, deleted_character_count);
							info_text[deleted_character_count] = '\0';

							AddWindowHandleCommand(system, window_index, TextInputRevertReplaceText, replace_info, total_size, HandlerCommandType::TextReplace);
						}
					}
					else if (keyboard_tracker->IsKeyPressed(HID::Key::A)) {
						data->current_selection = 0;
						data->current_sprite_position = data->text->size;
					}
				}

				size_t caret_tick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - data->caret_start).count();
				if (caret_tick >= system->m_descriptors.misc.text_input_caret_display_time) {
					data->is_caret_display = !data->is_caret_display;
					data->caret_start = std::chrono::high_resolution_clock::now();
				}

				if (data->repeat_key != HID::Key::Apps && data->repeat_key != HID::Key::BrowserFavorites) {
					data->repeat_key = repeat_key;
				}
			}
			else if (keyboard_tracker->IsKeyPressed(HID::Key::Enter)) {
				keyboard->DoNotCaptureCharacters();
				data->is_caret_display = false;
				data->current_selection = data->current_sprite_position;
				data->is_currently_selected = false;

				if (data->callback != nullptr) {
					action_data->data = data->callback_data;
					data->callback(action_data);
				}

				system->DeallocateGeneralHandler();
				system->m_focused_window_data.ChangeGeneralHandler({ 0.0f, 0.0f }, { 0.0f, 0.0f }, nullptr, nullptr, 0, UIDrawPhase::Normal);
			}
			else if (!UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
				keyboard->DoNotCaptureCharacters();
				data->is_caret_display = false;
				data->current_selection = data->current_sprite_position;
				data->is_currently_selected = false;

				if (data->callback != nullptr) {
					action_data->data = data->callback_data;
					data->callback(action_data);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename SliderType, typename Filter>
		void SliderEnterValues(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerSlider* data = (UIDrawerSlider*)_data;
			UIDrawerSlider* additional_data = (UIDrawerSlider*)_additional_data;;
			system->m_focused_window_data.clean_up_call_general = true;

			auto terminate_lambda = [&]() {
				if (data->text_input_counter > 0) {
					bool is_valid_data = SliderType::ValidateTextInput(data->characters);
					if (is_valid_data) {
						SliderType::ConvertTextInput(data->characters, data->value_to_change);
						if (is_valid_data) {
							action_data->data = data->text_input;
							TextInputAction<Filter>(action_data);
						}
					}
					data->text_input_counter = 0;
					system->m_keyboard->DoNotCaptureCharacters();
					return is_valid_data;
				}
				return false;
			};

			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
				if (UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
					if (data->text_input_counter == 0) {
						size_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - additional_data->enter_value_click).count();
						if (duration < system->m_descriptors.misc.slider_enter_value_duration) {
							data->text_input_counter = 1;
							system->m_keyboard->CaptureCharacters();
						}
						data->enter_value_click = std::chrono::high_resolution_clock::now();
					}
				}
				else if (data->text_input_counter == 0 && mouse_tracker->LeftButton() == MBPRESSED) {
					data->enter_value_click = std::chrono::high_resolution_clock::now();
				}
				else if (keyboard_tracker->IsKeyPressed(HID::Key::Enter)) {
					bool is_valid_data = terminate_lambda();
					system->DeallocateGeneralHandler();
					system->m_focused_window_data.ResetGeneralHandler();
				}
				else {
					action_data->data = data->text_input;
					TextInputAction<Filter>(action_data);

					if (keyboard->IsKeyDown(HID::Key::LeftControl) && keyboard_tracker->IsKeyPressed(HID::Key::C)) {
						system->m_application->WriteTextToClipboard(data->characters.buffer);
					}
				}
			}
			else if (!UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
				terminate_lambda();
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename Color>
		void ColorInputSVRectangleClickableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ColorInputHSVGradientInfo* data = (ColorInputHSVGradientInfo*)_data;
			float x_factor = function::InverseLerp(position.x, position.x + scale.x, mouse_position.x);
			x_factor = function::Select(x_factor > 1.0f, 1.0f, x_factor);
			x_factor = function::Select(x_factor < 0.0f, 0.0f, x_factor);

			float y_factor = function::InverseLerp(position.y, position.y + scale.y, mouse_position.y);
			y_factor = function::Select(y_factor > 1.0f, 1.0f, y_factor);
			y_factor = function::Select(y_factor < 0.0f, 0.0f, y_factor);

			data->input->hsv.saturation = x_factor * Color::GetRange();
			data->input->hsv.value = (1.0f - y_factor) * Color::GetRange();
			*data->input->rgb = HSVToRGB(data->input->hsv);

			data->input->Callback(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename Color>
		void ColorInputHRectangleClickableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ColorInputHSVGradientInfo* info = (ColorInputHSVGradientInfo*)_data;

			float factor = function::InverseLerp(info->gradient_position.y, info->gradient_position.y + info->gradient_scale.y, mouse_position.y);
			factor = function::Select(factor > 1.0f, 1.0f, factor);
			factor = function::Select(factor < 0.0f, 0.0f, factor);

			info->input->hsv.hue = factor * Color::GetRange();
			*info->input->rgb = HSVToRGB(info->input->hsv);

			info->input->Callback(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename Color>
		void ColorInputARectangleClickableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ColorInputHSVGradientInfo* info = (ColorInputHSVGradientInfo*)_data;

			float factor = function::InverseLerp(info->gradient_position.y, info->gradient_position.y + info->gradient_scale.y, mouse_position.y);
			factor = function::Select(factor > 1.0f, 1.0f, factor);
			factor = function::Select(factor < 0.0f, 0.0f, factor);

			info->input->rgb->alpha = (1.0f - factor) * Color::GetRange();
			info->input->hsv.alpha = info->input->rgb->alpha;

			info->input->Callback(action_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<bool initializer>
		void ColorInputWindowDraw(void* window_data, void* drawer_descriptor) {
			UI_PREPARE_DRAWER(initializer);

			constexpr size_t rectangle_configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_CLICKABLE_ACTION;
			constexpr size_t circle_configuration = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE;
#define TRIANGLE_CONFIGURATION UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_CLICKABLE_ACTION | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_FIT_SPACE
			constexpr float rectangle_y_scale = 0.55f;
			constexpr float hue_alpha_x_scale = 0.065f;
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

				drawer.SpriteRectangle<TRIANGLE_CONFIGURATION | UI_CONFIG_LATE_DRAW>(triangle1_config, ECS_TOOLS_UI_TEXTURE_TRIANGLE, ECS_COLOR_WHITE, float2(1.0f, 0.0f), float2(0.0f, 1.0f));
				drawer.SpriteRectangle<TRIANGLE_CONFIGURATION | UI_CONFIG_LATE_DRAW>(triangle2_config, ECS_TOOLS_UI_TEXTURE_TRIANGLE, ECS_COLOR_WHITE, float2(0.0f, 1.0f), float2(1.0f, 0.0f));
			};
#undef TRIANGLE_CONFIGURATION

#pragma region Initialization
			drawer.DisablePaddingForRenderSliders();
			drawer.DisablePaddingForRenderRegion();
			drawer.DisableZoom();
			drawer.SetDrawMode(UIDrawerMode::Indent);

			UIDrawerColorInputWindowData* data = (UIDrawerColorInputWindowData*)window_data;
			UIDrawerColorInput* main_input = data->input;
			UIDrawerColorInput* input;
			Color hsv_color;

			if constexpr (!initializer) {
				input = (UIDrawerColorInput*)drawer.GetResource("ColorInput");
				input->color_picker_initial_color = main_input->color_picker_initial_color;
				input->callback = main_input->callback;
				input->a_slider->changed_value_callback = main_input->callback;
				input->b_slider->changed_value_callback = main_input->callback;
				input->g_slider->changed_value_callback = main_input->callback;
				input->r_slider->changed_value_callback = main_input->callback;
				input->h_slider->changed_value_callback = main_input->callback;
				input->s_slider->changed_value_callback = main_input->callback;
				input->v_slider->changed_value_callback = main_input->callback;
				main_input->hsv = input->hsv;
				input->rgb = main_input->rgb;
			}
			else {
				input = main_input;
			}

			hsv_color = input->hsv;

#pragma endregion

#pragma region Saturation-Value Rectangle
			constexpr size_t gradient_count = 15;
			UIDrawConfig sv_config;

			UIConfigWindowDependentSize sv_size;
			sv_size.type = WindowSizeTransformType::Both;
			sv_size.scale_factor = { rectangle_y_scale, rectangle_y_scale };

			ColorInputHSVGradientInfo h_info;
			h_info.input = input;

			UIConfigClickableAction sv_clickable_action;
			sv_clickable_action.handler.action = ColorInputSVRectangleClickableAction<Color>;
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
			float2 sv_scale = drawer.GetWindowSizeScaleElement(WindowSizeTransformType::Both, { rectangle_y_scale, rectangle_y_scale });
			drawer.Gradient<rectangle_configuration>(sv_config, sv_colors, 15, 15);

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
			drawer.SpriteRectangle<circle_configuration>(circle_config, ECS_TOOLS_UI_TEXTURE_FILLED_CIRCLE, filled_circle_color, float2(0.0f, 0.0f), float2(1.0f, 1.0f));
			drawer.SpriteRectangle<circle_configuration>(circle_config, ECS_TOOLS_UI_TEXTURE_BOLD_CIRCLE);
			drawer.Indent();
#pragma endregion

#pragma region Hue Rectangle
			UIDrawConfig h_config;
			UIConfigWindowDependentSize h_size;
			h_size.type = WindowSizeTransformType::Both;
			h_size.scale_factor = { hue_alpha_x_scale, rectangle_y_scale };

			float2 h_gradient_position = drawer.GetCurrentPositionNonOffset();
			float2 gradient_scale = drawer.GetWindowSizeScaleElement(WindowSizeTransformType::Both, { hue_alpha_x_scale, rectangle_y_scale });

			h_info.gradient_position = h_gradient_position;
			h_info.gradient_scale = gradient_scale;
			UIConfigClickableAction h_action;
			h_action.handler.action = ColorInputHRectangleClickableAction<Color>;
			h_action.handler.data = &h_info;
			h_action.handler.data_size = sizeof(h_info);

			h_config.AddFlags(h_size, h_action);

			drawer.SpriteRectangle<rectangle_configuration>(h_config, ECS_TOOLS_UI_TEXTURE_HUE_GRADIENT);

			triangle_lambda(h_gradient_position, gradient_scale, drag_triangle_size, static_cast<float>(hsv_color.hue) / Color::GetRange(), h_action);
			drawer.Indent();
#pragma endregion

#pragma region Alpha rectangle
			UIDrawConfig a1_config;
			UIDrawConfig a2_config;
			UIConfigWindowDependentSize a_size;
			a_size.type = WindowSizeTransformType::Both;
			a_size.scale_factor = { hue_alpha_x_scale, rectangle_y_scale };

			float2 a_gradient_position = drawer.GetCurrentPositionNonOffset();
			ColorInputHSVGradientInfo a_gradient_info;
			a_gradient_info.gradient_position = a_gradient_position;
			a_gradient_info.gradient_scale = gradient_scale;
			a_gradient_info.input = input;

			UIConfigClickableAction a_action;
			a_action.handler.action = ColorInputARectangleClickableAction<Color>;
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

			drawer.Gradient<rectangle_configuration | UI_CONFIG_SPRITE_GRADIENT | UI_CONFIG_DO_NOT_ADVANCE>(a1_config, a_colors, 1, 10);
			drawer.Gradient<rectangle_configuration | UI_CONFIG_SPRITE_GRADIENT>(a2_config, a_colors + 2, 1, 10);

			triangle_lambda(a_gradient_position, gradient_scale, drag_triangle_size, 1.0f - static_cast<float>(input->rgb->alpha) / Color::GetRange(), a_action);

#pragma endregion

#pragma region Current And Previous Color
			constexpr float text_offset = 0.01f;
			constexpr float current_color_scale = 0.18f;
			constexpr float solid_color_y_scale = 0.18f;
			constexpr size_t text_configuration = UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_CACHE;

			UIDrawConfig current_config;
			drawer.Indent();
			float2 current_position = drawer.GetCurrentPosition();
			current_position.y += text_offset;

			float2 text_span;
			drawer.Text<text_configuration>(current_config, "Current", current_position, text_span);
			drawer.OffsetY(text_span.y + 2 * text_offset);
			current_position.y += text_span.y + text_offset;

			UIConfigWindowDependentSize solid_color_transform;
			solid_color_transform.type = WindowSizeTransformType::Both;
			solid_color_transform.scale_factor = { current_color_scale, solid_color_y_scale };

			float2 rectangle_scale = drawer.GetWindowSizeScaleElement(WindowSizeTransformType::Both, { current_color_scale, solid_color_y_scale });
			current_config.AddFlag(solid_color_transform);

			filled_circle_color.alpha = hsv_color.alpha;
			drawer.AlphaColorRectangle<text_configuration | UI_CONFIG_WINDOW_DEPENDENT_SIZE>(current_config, filled_circle_color);
			drawer.OffsetY(rectangle_scale.y + text_offset);
			current_position.y += rectangle_scale.y + text_offset;

			drawer.Text<text_configuration>(current_config, "Previous", current_position, text_span);
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

			drawer.AlphaColorRectangle<text_configuration | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_CLICKABLE_ACTION>(previous_config, input->color_picker_initial_color);
			drawer.NextRow();
#pragma endregion

#pragma region Sliders
			constexpr size_t color_input_configuration =
				UI_CONFIG_WINDOW_DEPENDENT_SIZE
				| UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER
				| UI_CONFIG_COLOR_INPUT_RGB_SLIDERS
				| UI_CONFIG_COLOR_INPUT_HSV_SLIDERS
				| UI_CONFIG_COLOR_INPUT_NO_NAME
				| UI_CONFIG_COLOR_INPUT_DO_NOT_CHOOSE_COLOR
				| UI_CONFIG_COLOR_INPUT_HEX_INPUT
				| UI_CONFIG_SLIDER_MOUSE_DRAGGABLE
				| UI_CONFIG_SLIDER_ENTER_VALUES;

			UIDrawConfig color_input_config;
			UIConfigWindowDependentSize color_input_transform;
			color_input_transform.type = WindowSizeTransformType::Horizontal;
			color_input_transform.scale_factor = { 0.28f, 1.0f };

			color_input_config.AddFlag(color_input_transform);

			drawer.ColorInput<color_input_configuration>(color_input_config, "ColorInput", main_input->rgb);
#pragma endregion

		}

		// --------------------------------------------------------------------------------------------------------------

		template<bool dummy = true>
		void ColorInputCreateWindow(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			system->DestroyWindowIfFound("ColorInputWindow");

			UIDrawerColorInput* data = (UIDrawerColorInput*)_data;

			data->color_picker_initial_color = *data->rgb;
			Color* hsv_color = &data->hsv;
			UIWindowDescriptor window_descriptor;
			window_descriptor.draw = ColorInputWindowDraw<false>;
			window_descriptor.initialize = ColorInputWindowDraw<true>;

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

			void* dummy_allocation = system->m_resources.thread_resources[0].temp_allocator.Allocate(1, 1);

			// if it is desired to be destroyed when going out of focus
			UIPopUpWindowData system_handler_data;
			system_handler_data.is_fixed = true;
			system_handler_data.is_initialized = true;
			system_handler_data.destroy_at_first_click = false;
			system_handler_data.flag_destruction = (bool*)dummy_allocation;
			system_handler_data.name = "ColorInputWindow";
			system_handler_data.reset_when_window_is_destroyed = true;
			UIActionHandler handler;
			handler.action = PopUpWindowSystemHandler;
			handler.data = &system_handler_data;
			handler.data_size = sizeof(system_handler_data);
			system->PushSystemHandler(handler);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<bool initializer, size_t configuration>
		void ComboBoxWindowDraw(void* window_data, void* drawer_descriptor) {
			UI_PREPARE_DRAWER(initializer);

			UIDrawerComboBoxClickable* data = (UIDrawerComboBoxClickable*)window_data;
			if constexpr (!initializer) {
				drawer.DisablePaddingForRenderRegion();
				drawer.DisableZoom();
				constexpr size_t new_configuration = function::RemoveFlag(function::RemoveFlag(configuration, UI_CONFIG_LATE_DRAW), UI_CONFIG_SYSTEM_DRAW);
				drawer.ComboBoxDropDownDrawer<new_configuration>(data->config, data->box);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void ComboBoxClickable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerComboBoxClickable* clickable_data = (UIDrawerComboBoxClickable*)_data;
			UIDrawerComboBox* data = clickable_data->box;
			if (mouse_tracker->LeftButton() == MBPRESSED) {
				data->initial_click_state = data->is_opened;
			}
			else if (mouse_tracker->LeftButton() == MBRELEASED) {
				data->is_opened = !data->initial_click_state;

				if (IsPointInRectangle(mouse_position, position, scale)) {
					if (data->is_opened) {
						data->has_been_destroyed = false;

						UIWindowDescriptor window_descriptor;
						window_descriptor.draw = ComboBoxWindowDraw<false, configuration>;
						window_descriptor.initialize = ComboBoxWindowDraw<true, configuration>;
						window_descriptor.initial_position_x = position.x;
						window_descriptor.initial_size_x = scale.x;
						if (position.y + scale.y + ECS_TOOLS_UI_COMBO_BOX_PADDING > 1.0f) {
							window_descriptor.initial_position_y = position.y - scale.y * data->label_display_count - ECS_TOOLS_UI_COMBO_BOX_PADDING;
						}
						else {
							window_descriptor.initial_position_y = position.y + scale.y + ECS_TOOLS_UI_COMBO_BOX_PADDING;
						}
						window_descriptor.initial_size_y = scale.y * data->label_display_count;
						window_descriptor.window_name = "ComboBoxOpened";
						window_descriptor.window_data = clickable_data;
						window_descriptor.window_data_size = sizeof(UIDrawerComboBoxClickable);

						system->CreateWindowAndDockspace(window_descriptor, UI_DOCKSPACE_FIXED 
							| UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X | UI_DOCKSPACE_BORDER_FLAG_NO_TITLE | UI_DOCKSPACE_BORDER_FLAG_COLLAPSED_REGION_HEADER);

						UIPopUpWindowData handler_data;
						handler_data.is_fixed = true;
						handler_data.is_initialized = true;
						handler_data.destroy_at_first_click = false;
						handler_data.flag_destruction = &data->has_been_destroyed;
						handler_data.name = "ComboBoxOpened";

						UIActionHandler handler;
						handler.action = PopUpWindowSystemHandler;
						handler.data = &handler_data;
						handler.data_size = sizeof(handler_data);
						system->PushSystemHandler(handler);
					}
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		template<bool initializer>
		void MenuDraw(void* window_data, void* drawer_descriptor) {
			UI_PREPARE_DRAWER(initializer);

			UIDrawerMenuDrawWindowData* data = (UIDrawerMenuDrawWindowData*)window_data;
			if constexpr (initializer) {
				drawer.DisableZoom();
				drawer.SetRowPadding(0.0f);
				drawer.SetNextRowYOffset(0.0f);
			}
			else {
#pragma region Drawer disables

				drawer.DisablePaddingForRenderRegion();
				drawer.DisablePaddingForRenderSliders();
				drawer.SetDrawMode(UIDrawerMode::NextRow);

#pragma endregion

#pragma region Config preparations

				UIDrawConfig config;
				float2 region_scale = drawer.GetRegionScale();
				float2 default_element_scale = drawer.GetElementDefaultScale();

				UIConfigRelativeTransform transform;
				transform.scale.x = region_scale.x / default_element_scale.x;
				transform.offset.y = 0.001f;
				config.AddFlag(transform);

				// Must be replicated inside lambdas
				constexpr size_t configuration = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGNMENT
					| UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_DO_NOT_ADVANCE;

				UIConfigHoverableAction hoverable;
				UIDrawerSubmenuHoverable hover_data;
				hover_data.state = data->state;
				hover_data.menu = data->menu;
				hoverable.handler = { MenuSubmenuHoverable, &hover_data, sizeof(hover_data), UIDrawPhase::System };

				UIConfigGeneralAction general;
				UIDrawerMenuGeneralData general_data;
				general_data.menu = data->menu;
				general_data.destroy_state = data->state;
				general_data.destroy_scale = { region_scale.x, default_element_scale.y };
				general.handler = { MenuGeneral, &general_data, sizeof(general_data), UIDrawPhase::System };
				config.AddFlag(general);

#pragma endregion

				UIConfigClickableAction clickable;

				unsigned int current_word_start = 0;
				auto draw_label = [&](size_t index, const unsigned short* substream, char* characters) {
					constexpr size_t configuration = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X
						| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_LABEL_TRANSPARENT
						| UI_CONFIG_DO_NOT_ADVANCE;

					// if it is not the last one, change the \n into a \0 in order to get correctly parsed
					// and reverse it after drawing
					if (index < data->state->row_count - 1) {
						unsigned int new_line_character = substream[index];
						characters[new_line_character] = '\0';
						drawer.TextLabel<configuration>(config, characters + current_word_start);
						characters[new_line_character] = '\n';
					}
					// if last, no need to change the null terminator character
					else {
						drawer.TextLabel<configuration>(config, characters + current_word_start);
					}
					current_word_start = substream[index] + 1;
				};

				auto draw_label_unavailable = [&](size_t index, const unsigned short* substream, char* characters) {
					constexpr size_t configuration = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X 
						| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_LABEL_TRANSPARENT
						| UI_CONFIG_DO_NOT_ADVANCE;

					// if it is not the last one, change the \n into a \0 in order to get correctly parsed
					// and reverse it after drawing
					if (index < data->state->row_count - 1) {
						unsigned int new_line_character = substream[index];
						characters[new_line_character] = '\0';
						drawer.TextLabel<configuration | UI_CONFIG_UNAVAILABLE_TEXT>(config, characters + current_word_start);
						characters[new_line_character] = '\n';
					}
					// if last, no need to change the null terminator character
					else {
						drawer.TextLabel<configuration | UI_CONFIG_UNAVAILABLE_TEXT>(config, characters + current_word_start);
					}
					current_word_start = substream[index] + 1;
				};

#pragma region Left characters and >

				UIConfigTextAlignment alignment;
				alignment.horizontal = TextAlignment::Left;
				alignment.vertical = TextAlignment::Middle;
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
						system->PushSystemHandler({ MenuCleanupSystemHandler, &cleanup_data, sizeof(cleanup_data) });
						system->DeallocateGeneralHandler();
						system->m_focused_window_data.ResetGeneralHandler();
					}
				};

				unsigned char current_line_index = 0;
				for (size_t index = 0; index < data->state->row_count; index++) {
					float2 current_position = drawer.GetCurrentPositionNonOffset();

					general_data.menu_initializer_index = index;
					general_data.destroy_position = current_position;
					hover_data.row_index = index;
					auto system = drawer.GetSystem();
					Color arrow_color = system->m_descriptors.misc.menu_arrow_color;
					if (data->state->unavailables == nullptr || (data->state->unavailables != nullptr && !data->state->unavailables[index])) {
						ClickableWrapperData wrapper_data;
						wrapper_data.action = data->state->click_handlers[index].action;
						wrapper_data.data = data->state->click_handlers[index].data;
						wrapper_data.menu = data->menu;
						clickable.handler = { clickable_wrapper, &wrapper_data, sizeof(wrapper_data), data->state->click_handlers[index].phase };

						if (data->state->row_has_submenu != nullptr && data->state->row_has_submenu[index] == true) {
							clickable.handler = { SkipAction, nullptr, 0 };
						}
						config.AddFlag(clickable);
						config.AddFlag(hoverable);

						draw_label(index, data->state->left_row_substreams, data->state->left_characters);

						drawer.Rectangle<UI_CONFIG_HOVERABLE_ACTION | UI_CONFIG_CLICKABLE_ACTION
								| UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_GENERAL_ACTION>(
									config
									);
						config.flag_count -= 2;
					}
					else {
						config.AddFlag(hoverable);
						
						draw_label_unavailable(index, data->state->left_row_substreams, data->state->left_characters);
						
						drawer.Rectangle<UI_CONFIG_HOVERABLE_ACTION | UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_GENERAL_ACTION>(
							config
						);
						config.flag_count--;

						arrow_color = system->m_descriptors.misc.menu_unavailable_arrow_color;
					}

					if (data->state->row_has_submenu != nullptr && data->state->row_has_submenu[index]) {
						auto buffers = drawer.GetBuffers();
						auto counts = drawer.GetCounts();
						float2 sign_scale = system->GetTextSpan(">", 1, system->m_descriptors.font.size * ECS_TOOLS_UI_FONT_X_FACTOR, system->m_descriptors.font.size, system->m_descriptors.font.character_spacing);
						system->ConvertCharactersToTextSprites(
							">",
							{ current_position.x + region_scale.x - sign_scale.x * 2.0f, AlignMiddle(current_position.y, default_element_scale.y, sign_scale.y) },
							(UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE],
							1,
							arrow_color,
							counts[ECS_TOOLS_UI_TEXT_SPRITE],
							{ system->m_descriptors.font.size * ECS_TOOLS_UI_FONT_X_FACTOR, system->m_descriptors.font.size },
							system->m_descriptors.font.character_spacing
						);
						counts[ECS_TOOLS_UI_TEXT_SPRITE] += 6;
					}

					if (current_line_index < data->state->separation_line_count) {
						if (index == data->state->separation_lines[current_line_index]) {
							UIDrawConfig line_config;
							UIConfigAbsoluteTransform transform;
							transform.scale.x = drawer.region_scale.x - 2.0f * system->m_descriptors.misc.tool_tip_padding.x;
							transform.scale.y = ECS_TOOLS_UI_ONE_PIXEL_Y;
							transform.position = drawer.GetCurrentPosition();
							transform.position.x += system->m_descriptors.misc.tool_tip_padding.x;

							line_config.AddFlag(transform);
							drawer.CrossLine<UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_CROSS_LINE_DO_NOT_INFER
							 | UI_CONFIG_DO_NOT_ADVANCE>(line_config);
							current_line_index++;
						}
					}
				}

#pragma endregion

#pragma region Right characters

				if (data->state->right_characters != nullptr) {
					// for text alignment
					config.flag_count--;
					alignment.horizontal = TextAlignment::Right;
					config.AddFlag(alignment);

					drawer.SetCurrentY(drawer.region_position.y);
					drawer.NextRow();
					current_word_start = 0;
					for (size_t index = 0; index < data->state->row_count; index++) {
						float2 current_position = drawer.GetCurrentPositionNonOffset();

						auto system = drawer.GetSystem();
						if (data->state->unavailables == nullptr || (data->state->unavailables != nullptr && !data->state->unavailables[index])) {
							draw_label(index, data->state->right_row_substreams, data->state->right_characters);
						}
						else {
							draw_label_unavailable(index, data->state->right_row_substreams, data->state->right_characters);
						}
						drawer.current_row_y_scale = drawer.layout.default_element_y;
						drawer.NextRow();
					}

				}

#pragma endregion
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		template<bool dummy = true>
		void MenuSubmenuHoverable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerSubmenuHoverable* data = (UIDrawerSubmenuHoverable*)_data;
			UIDrawerSubmenuHoverable* additional_data = nullptr;
			system->m_focused_window_data.clean_up_call_hoverable = true;
			system->m_focused_window_data.always_hoverable = true;

			if (_additional_data != nullptr) {
				additional_data = (UIDrawerSubmenuHoverable*)_additional_data;
			}

			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
				if (!UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
					if (data->state->windows->size > data->state->submenu_index) {
						UIDrawerMenuCleanupSystemHandlerData cleanup_data;
						unsigned int last_index = data->state->submenu_index /*+ 1*/ +
							((data->state->row_has_submenu != nullptr) && data->state->row_has_submenu[data->row_index]) *
							((data->state->unavailables == nullptr) || (!data->state->unavailables[data->row_index]));
						unsigned int first_index = data->state->submenu_index + 1;
						if (first_index < data->state->windows->size) {
							cleanup_data.window_count = data->state->windows->size - first_index;
							for (size_t index = 0; index < cleanup_data.window_count; index++) {
								cleanup_data.window_names[index] = data->state->windows->buffer[first_index + index].name;
							}
							data->state->windows->size = first_index;
							//ECS_ASSERT(data->state->windows->size == first_index);
						}
						else {
							cleanup_data.window_count = 0;
						}
						
						system->PushSystemHandler({ MenuCleanupSystemHandler, &cleanup_data, sizeof(cleanup_data) });
					}
					if (data->state->row_has_submenu != nullptr && data->state->row_has_submenu[data->row_index]) {
						UITooltipBaseData base;
						float y_text_span = system->GetTextSpriteYScale(system->m_descriptors.font.size);
						base.next_row_offset = system->m_descriptors.window_layout.default_element_y - y_text_span;

						float2 window_size;
						if (data->state->right_characters == nullptr) {
							window_size = system->DrawToolTipSentenceSize(data->state->submenues[data->row_index].left_characters, &base);
						}
						else {
							window_size = system->DrawToolTipSentenceWithTextToRightSize(data->state->submenues[data->row_index].left_characters, data->state->submenues[data->row_index].right_characters, &base);
						}

						window_size.y -= 2.0f * system->m_descriptors.misc.tool_tip_padding.y + base.next_row_offset;
						window_size.x -= 2.0f * system->m_descriptors.misc.tool_tip_padding.x - 2.0f * system->m_descriptors.element_descriptor.label_horizontal_padd - system->m_descriptors.misc.menu_x_padd;

						float arrow_span = system->GetTextSpan(">", 1, system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.font.size), system->m_descriptors.font.size, system->m_descriptors.font.character_spacing).x;
						window_size.x += (data->state->row_has_submenu != nullptr) * (arrow_span + system->m_descriptors.element_descriptor.label_horizontal_padd);

						UIWindowDescriptor submenu_descriptor;
						submenu_descriptor.draw = MenuDraw<false>;
						submenu_descriptor.initialize = MenuDraw<true>;
						submenu_descriptor.initial_position_x = position.x + scale.x;
						submenu_descriptor.initial_position_y = position.y;

						if (submenu_descriptor.initial_position_x + window_size.x > 1.0f) {
							submenu_descriptor.initial_position_x = 1.0f - window_size.x;
						}
						if (submenu_descriptor.initial_position_y + window_size.y > 1.0f) {
							submenu_descriptor.initial_position_y = 1.0f - window_size.y;
						}

						UIDrawerMenuDrawWindowData window_data;
						window_data.menu = data->menu;
						window_data.state = &data->state->submenues[data->row_index];
						submenu_descriptor.initial_size_x = window_size.x;
						submenu_descriptor.initial_size_y = window_size.y;
						submenu_descriptor.window_data = &window_data;
						submenu_descriptor.window_data_size = sizeof(window_data);

						submenu_descriptor.window_name = data->state->submenues[data->row_index].left_characters;

						system->CreateWindowAndDockspace(submenu_descriptor,
							UI_DOCKSPACE_FIXED | UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X | UI_DOCKSPACE_BORDER_FLAG_COLLAPSED_REGION_HEADER 
							| UI_DOCKSPACE_BORDER_FLAG_NO_TITLE | UI_DOCKSPACE_POP_UP_WINDOW);
						data->state->windows->Add({ submenu_descriptor.window_name, {submenu_descriptor.initial_position_x, submenu_descriptor.initial_position_y}, window_size });
					}
				}

				if (data->state->unavailables == nullptr || (!data->state->unavailables[data->row_index] && data->state->unavailables != nullptr)) {				
					float y_text_size = system->GetTextSpriteYScale(system->m_descriptors.font.size);
					float2 font_size = { system->m_descriptors.font.size * ECS_TOOLS_UI_FONT_X_FACTOR, system->m_descriptors.font.size };

					float2 text_position = {
						position.x + system->m_descriptors.element_descriptor.label_horizontal_padd,
						AlignMiddle(position.y, scale.y, y_text_size)
					};
				
					unsigned int first_character = 0;
					if (data->row_index != 0) {
						first_character = data->state->left_row_substreams[data->row_index - 1] + 1;
					}
					system->ConvertCharactersToTextSprites(
						data->state->left_characters + first_character,
						text_position,
						(UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE],
						data->state->left_row_substreams[data->row_index] - first_character,
						system->m_descriptors.color_theme.default_text,
						counts[ECS_TOOLS_UI_TEXT_SPRITE],
						font_size,
						system->m_descriptors.font.character_spacing
					);
					counts[ECS_TOOLS_UI_TEXT_SPRITE] += (data->state->left_row_substreams[data->row_index] - first_character) * 6;

					if (data->state->right_characters != nullptr) {
						first_character = 0;
						if (data->row_index != 0) {
							first_character = data->state->right_row_substreams[data->row_index - 1] + 1;
						}
						float x_span = system->GetTextSpan(
							data->state->right_characters + first_character,
							data->state->right_row_substreams[data->row_index] - first_character,
							font_size.x, 
							font_size.y, 
							system->m_descriptors.font.character_spacing
						).x;

						float2 right_text_position = { 
							position.x + scale.x - system->m_descriptors.element_descriptor.label_horizontal_padd - x_span, 
							text_position.y 
						};
						system->ConvertCharactersToTextSprites(
							data->state->right_characters + first_character,
							right_text_position,
							(UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE],
							data->state->right_row_substreams[data->row_index] - first_character,
							system->m_descriptors.color_theme.default_text,
							counts[ECS_TOOLS_UI_TEXT_SPRITE],
							font_size,
							system->m_descriptors.font.character_spacing
						);
						counts[ECS_TOOLS_UI_TEXT_SPRITE] += (data->state->right_row_substreams[data->row_index] - first_character) * 6;
					}

					if (data->state->row_has_submenu != nullptr && data->state->row_has_submenu[data->row_index]) {
						// The compiler messes up the registers and overwrites the font size in distribution
						// so do a rewrite of the values
						font_size = { system->m_descriptors.font.size * ECS_TOOLS_UI_FONT_X_FACTOR, system->m_descriptors.font.size };
						float2 arrow_size = system->GetTextSpan(">", 1, font_size.x, font_size.y, system->m_descriptors.font.character_spacing);
						float2 arrow_position = { position.x + scale.x - arrow_size.x * 2.0f, AlignMiddle(position.y, system->m_descriptors.window_layout.default_element_y, arrow_size.y) };

						system->ConvertCharactersToTextSprites(
							">",
							arrow_position,
							(UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE],
							1,
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

					if (data->row_index == data->state->row_count - 1) {
						action_data->scale.y -= system->m_descriptors.dockspaces.border_size * 1.5f;
					}
					else if (data->row_index == 0) {
						action_data->scale.y -= system->m_descriptors.dockspaces.border_size;
						action_data->position.y += system->m_descriptors.dockspaces.border_size;
					}

					DefaultHoverableAction(action_data);
				}
			}
			else {
				if (!UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
					if (additional_data == nullptr || (additional_data != nullptr && (additional_data->state->submenu_index <= data->state->submenu_index))) {
						/*UIDrawerMenuCleanupSystemHandlerData cleanup_data;
						unsigned int first_index = data->state->submenu_index + 1;
						unsigned int last_index = data->state->submenu_index + 1 + 
							((data->state->row_has_submenu != nullptr) && data->state->row_has_submenu[data->row_index]) * 
							((data->state->unavailables == nullptr) || (!data->state->unavailables[data->row_index]));
						cleanup_data.window_count = last_index - first_index;
						for (size_t index = 0; index < cleanup_data.window_count; index++) {
							cleanup_data.window_names[index] = data->state->menu_names->buffer[first_index + index];
						}
						system->PushSystemHandler({ MenuCleanupSystemHandler, &cleanup_data, sizeof(cleanup_data) });*/
					}
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		template<bool dummy = true>
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
					if (menu->state.right_characters == nullptr) {
						window_dimensions = system->DrawToolTipSentenceSize(menu->state.left_characters, &tool_tip_data);
					}
					else {
						window_dimensions = system->DrawToolTipSentenceWithTextToRightSize(menu->state.left_characters, menu->state.right_characters, &tool_tip_data);
					}
					window_dimensions.y -= 2.0f * system->m_descriptors.misc.tool_tip_padding.y + tool_tip_data.next_row_offset * 0.5f;
					window_dimensions.x -= 2.0f * system->m_descriptors.misc.tool_tip_padding.x - 2.0f * system->m_descriptors.element_descriptor.label_horizontal_padd - system->m_descriptors.misc.menu_x_padd;

					float arrow_span = system->GetTextSpan(">", 1, system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.font.size), system->m_descriptors.font.size, system->m_descriptors.font.character_spacing).x;
					window_dimensions.x += (menu->state.row_has_submenu != nullptr) * (arrow_span + system->m_descriptors.element_descriptor.label_horizontal_padd);

					UIWindowDescriptor window_descriptor;
					window_descriptor.draw = MenuDraw<false>;
					window_descriptor.initialize = MenuDraw<true>;
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
					window_data.state = &menu->state;
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
					system->PushSystemHandler({ MenuCleanupSystemHandler, &cleanup_data, sizeof(cleanup_data) });
					/*for (int64_t index = (int64_t)menu->window_indices.size - 1; index >= 0; index--) {
						system->DestroyPopUpWindow(menu->window_indices[index], true);
					}
					menu->window_indices.size = 0;*/
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		// Must have a dummy template parameter in order to have the drawer recognized
		template<bool dummy = true>
		void RightClickMenu(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerMenuRightClickData* data = (UIDrawerMenuRightClickData*)_data;
			if (mouse_tracker->RightButton() == MBRELEASED) {
				if (data->action != nullptr) {
					action_data->data = data->action_data;
					data->action(action_data);
				}

				if (system->GetWindowFromName(data->state.left_characters) != -1) {
					// mouse position -2.0f -2.0f ensures that it will not fall into an existing 
					// window and avoid destroy the windows
					system->HandleFocusedWindowCleanupGeneral({-2.0f, -2.0f}, thread_id);
					system->m_focused_window_data.ResetGeneralHandler();
					// The window destruction handler
					system->HandleSystemHandler();
					// The release resources handler
					system->HandleSystemHandler();
				}

				unsigned int window_index = data->window_index;
				UIDrawerDescriptor descriptor = system->GetDrawerDescriptor(window_index);
				descriptor.do_not_initialize_viewport_sliders = true;
				UIDrawer<true> drawer = UIDrawer<true>(
					descriptor,
					nullptr
				);

				drawer.Menu(data->name, &data->state);

				/*UIDrawerMenuCleanupSystemHandlerData cleanup_data;
				cleanup_data.window_count = internal_data->general_data.menu->window_names.size;
				for (int64_t index = 0; index < cleanup_data.window_count; index++) {
					cleanup_data.window_names[index] = internal_data->general_data.menu->window_names[index];
				}
				system->PushSystemHandler({ MenuCleanupSystemHandler, &cleanup_data, sizeof(cleanup_data) });*/

				/*for (int64_t index = (int64_t)internal_data->general_data.menu->window_indices.size - 1; index >= 0; index--) {
					system->DestroyPopUpWindow(internal_data->general_data.menu->window_indices[index], true);
				}*/

				UIDrawerMenuGeneralData* general_data = (UIDrawerMenuGeneralData*)system->m_memory->Allocate(sizeof(UIDrawerMenuGeneralData));
				general_data->menu_initializer_index = 255;
				general_data->initialize_from_right_click = true;
				general_data->menu = (UIDrawerMenu*)drawer.GetResource(data->name);
				general_data->is_opened_when_clicked = false;

				system->SetNewFocusedDockspaceRegion(dockspace, border_index, dockspace_type);
				system->SetNewFocusedDockspace(dockspace, dockspace_type);

				// call the general handler if it wants to have destruction
				system->HandleFocusedWindowCleanupGeneral(mouse_position, thread_id);

				UIActionHandler handler = { MenuGeneral, general_data, sizeof(*general_data), UIDrawPhase::System };
				system->m_focused_window_data.ChangeGeneralHandler(position, scale, &handler);

				action_data->data = general_data;
				action_data->position.x = mouse_position.x;
				// small padd in order to have IsPointInRectangle detect it
				action_data->position.y = mouse_position.y - action_data->scale.y + 0.0001f;
				MenuGeneral(action_data);

				UIDrawerRightClickMenuSystemHandlerData handler_data;
				handler_data.menu_resource_name = data->name;
				handler_data.parent_window_name = system->m_windows[data->window_index].name;
				handler_data.menu_window_name = data->state.left_characters;
				system->PushSystemHandler({ RightClickMenuReleaseResource, &handler_data, sizeof(handler_data) });
			}

		}

		// --------------------------------------------------------------------------------------------------------------
		
		template<bool initialize, typename DataType>
		void FilterMenuDrawInternal(void* window_data, void* drawer_descriptor) {
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
			if (!data->draw_all) {
				if (should_notify) {
					drawer.StateTable<UI_CONFIG_STATE_TABLE_NO_NAME | UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE>(config, data->name, data->labels, data->states);
				}
				else {
					drawer.StateTable<UI_CONFIG_STATE_TABLE_NO_NAME>(config, data->name, data->labels, data->states);
				}
			}
			else {
				if (should_notify) {
					drawer.StateTable<UI_CONFIG_STATE_TABLE_NO_NAME | UI_CONFIG_STATE_TABLE_ALL | UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE>(config, data->name, data->labels, data->states);
				}
				else {
					drawer.StateTable<UI_CONFIG_STATE_TABLE_NO_NAME | UI_CONFIG_STATE_TABLE_ALL>(config, data->name, data->labels, data->states);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void FilterMenuDraw(void* window_data, void* drawer_descriptor) {
			FilterMenuDrawInternal<initialize, UIDrawerFilterMenuData>(window_data, drawer_descriptor);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<bool initialize>
		void FilterMenuSinglePointerDraw(void* window_data, void* drawer_descriptor) {
			FilterMenuDrawInternal<initialize, UIDrawerFilterMenuSinglePointerData>(window_data, drawer_descriptor);
		}

		// --------------------------------------------------------------------------------------------------------------

	}

}