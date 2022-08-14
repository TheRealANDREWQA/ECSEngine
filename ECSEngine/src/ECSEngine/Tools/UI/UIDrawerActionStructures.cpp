#include "ecspch.h"
#include "UIDrawerActionStructures.h"

namespace ECSEngine {

	namespace Tools {
		
		void UIDrawerTextInput::SetZoomFactor(float2 zoom)
		{
			name.SetZoomFactor(zoom);
		}
		
		bool UIDrawerTextInput::IsTheSameData(const UIDrawerTextInput* other) const
		{
			return this == other;
		}

		float2 UIDrawerTextInput::GetZoom() const
		{
			return current_zoom;
		}

		float2 UIDrawerTextInput::GetInverseZoom() const {
			return inverse_zoom;
		}

		float UIDrawerTextInput::GetZoomX() const {
			return current_zoom.x;
		}

		float UIDrawerTextInput::GetZoomY() const {
			return current_zoom.y;
		}

		float UIDrawerTextInput::GetInverseZoomX() const {
			return inverse_zoom.x;
		}

		float UIDrawerTextInput::GetInverseZoomY() const {
			return inverse_zoom.y;
		}

		CapacityStream<UISpriteVertex>* UIDrawerTextInput::TextStream()
		{
			return name.TextStream();
		}

		unsigned int* UIDrawerTextInput::NonNameTextSize()
		{
			return &vertices.size;
		}

		CapacityStream<UISpriteVertex>* UIDrawerTextInput::NonNameTextStream()
		{
			return &vertices;
		}

		float UIDrawerTextInput::GetLowestX() const
		{
			return name.GetLowestX();
		}

		float UIDrawerTextInput::GetLowestY() const
		{
			return name.GetLowestY();
		}

		float2 UIDrawerTextInput::GetLowest() const
		{
			return name.GetLowest();
		}

		float2* UIDrawerTextInput::TextPosition()
		{
			return name.TextPosition();
		}

		float2* UIDrawerTextInput::TextScale()
		{
			return name.TextScale();
		}

		UISpriteVertex* UIDrawerTextInput::TextBuffer()
		{
			return name.TextBuffer();
		}

		unsigned int* UIDrawerTextInput::TextSize()
		{
			return name.TextSize();
		}

		unsigned int UIDrawerTextInput::GetSpritePositionFromMouse(float2 mouse_position) const
		{
			if (sprite_render_offset > 0) {
				mouse_position.x += vertices[sprite_render_offset * 6 - 5].position.x - vertices[0].position.x;
			}
			if (mouse_position.x < vertices[0].position.x) {
				return sprite_render_offset;
			}
			for (size_t index = 1; index < vertices.size; index += 6) {
				if (mouse_position.x < vertices[index].position.x) {
					return (index - 1) / 6;
				}
			}
			return text->size;

		}

		void UIDrawerTextInput::SetSpritePositionFromMouse(float2 mouse_position) {
			current_sprite_position = GetSpritePositionFromMouse(mouse_position);
		}

		unsigned int UIDrawerTextInput::GetVisibleSpriteCount(float right_bound) const {
			size_t index;
			if (sprite_render_offset > 0) {
				right_bound += vertices[sprite_render_offset * 6 - 5].position.x - vertices[0].position.x + character_spacing;
			}
			for (index = sprite_render_offset; index < text->size; index++) {
				unsigned int sprite_index = index * 6;
				if (vertices[sprite_index + 1].position.x > right_bound)
					break;
			}
			return index - sprite_render_offset;
		}

		float2 UIDrawerTextInput::GetCaretPosition() const
		{
			float2 caret_position;
			if (current_sprite_position > 0) {
				if (sprite_render_offset == 0) {
					caret_position = { vertices[current_sprite_position * 6 - 5].position.x, -vertices[0].position.y };
				}
				else {
					caret_position = { vertices[current_sprite_position * 6 - 5].position.x - vertices[sprite_render_offset * 6 - 5].position.x + vertices[0].position.x, -vertices[0].position.y };
				}
			}
			else {
				caret_position = { position.x + padding.x, position.y + padding.y };
			}
			return caret_position;
		}

		void UIDrawerTextInput::SetNewZoom(float2 new_zoom)
		{
			character_spacing *= inverse_zoom.x * new_zoom.x;
			font_size.x *= inverse_zoom.x * new_zoom.x;
			font_size.y *= inverse_zoom.y * new_zoom.y;
			padding.x *= inverse_zoom.x * new_zoom.x;
			padding.y *= inverse_zoom.y * new_zoom.y;
			solid_color_y_scale *= inverse_zoom.y * new_zoom.y;
			inverse_zoom = { 1.0f / new_zoom.x, 1.0f / new_zoom.y };
			current_zoom = new_zoom;
		}

		void UIDrawerTextInput::PasteCharacters(
			const char* characters,
			unsigned int character_count,
			UISystem* system,
			unsigned int window_index,
			char* deleted_characters,
			unsigned int* deleted_character_count,
			unsigned int* delete_position
		)
		{
			trigger_callback = true;

			if (current_selection > current_sprite_position) {
				*delete_position = current_sprite_position;
			}
			else {
				*delete_position = current_selection;
			}

			if (current_selection != current_sprite_position) {
				UIDrawerTextInputRemoveCommandInfo command;
				auto command_info = &command;

				unsigned int data_size = sizeof(UIDrawerTextInputRemoveCommandInfo);
				unsigned int* text_count = deleted_character_count;
				unsigned int* text_position = &command_info->text_position;
				unsigned char* info_text = (unsigned char*)((uintptr_t)command_info + data_size);
				char* text = deleted_characters;
				command_info->input = this;

				Backspace(text_count, text_position, text);
			}
			InsertCharacters(characters, character_count, current_sprite_position, system);
		}

		void UIDrawerTextInput::Callback(ActionData* action_data)
		{
			action_data->data = callback_data;
			callback(action_data);
		}

		bool UIDrawerTextInput::HasCallback() const
		{
			return callback != nullptr;
		}

		bool UIDrawerTextInput::Backspace(unsigned int* output_text_count, unsigned int* output_text_position, char* output_text) {
			trigger_callback = true;
			if (current_sprite_position == current_selection && current_sprite_position > 0) {
				*output_text_count = 1;
				*output_text_position = current_sprite_position;

				output_text[0] = text->buffer[current_sprite_position - 1];
				output_text[1] = '\0';

				float deleted_sprite_span = vertices[current_sprite_position * 6 - 5].position.x - vertices[current_sprite_position * 6 - 6].position.x + character_spacing;
				for (size_t index = current_sprite_position; index < text->size; index++) {

					unsigned int sprite_index = index * 6;
					unsigned int destination_index = sprite_index - 6;
					text->buffer[index - 1] = text->buffer[index];
					for (size_t subindex = destination_index; subindex < sprite_index; subindex++) {
						vertices[subindex] = vertices[subindex + 6];
						vertices[subindex].position.x -= deleted_sprite_span;
					}
				}
				current_sprite_position--;
				text->size--;
				vertices.size -= 6;
				current_selection = current_sprite_position;
				if (sprite_render_offset > 0)
					sprite_render_offset--;
				caret_start = std::chrono::high_resolution_clock::now();
				is_caret_display = true;
				text->buffer[text->size] = '\0';
				return true;
			}
			else {
				unsigned int start_iteration, highest_index, lowest_index, displacement;
				if (current_selection < current_sprite_position) {
					highest_index = current_sprite_position * 6 - 5;
					lowest_index = current_selection * 6;
					displacement = current_sprite_position - current_selection;
					start_iteration = current_sprite_position;
				}
				else {
					highest_index = current_selection * 6 - 5;
					lowest_index = current_sprite_position * 6;
					displacement = current_selection - current_sprite_position;
					start_iteration = current_selection;
				}

				if (current_sprite_position > 0 || current_selection > 0) {
					if (current_sprite_position > current_selection) {
						current_sprite_position -= displacement;
					}
					*output_text_count = displacement;
					if (current_sprite_position > current_selection) {
						*output_text_position = current_selection;
					}
					else {
						*output_text_position = current_sprite_position;
					}

					memcpy(output_text, text->buffer + *output_text_position, *output_text_count);
					output_text[*output_text_count] = '\0';

					float deleted_sprite_span = vertices[highest_index].position.x - vertices[lowest_index].position.x /*+ character_spacing*/;
					unsigned int sprite_displacement = displacement * 6;
					for (size_t index = start_iteration; index < text->size; index++) {
						unsigned int sprite_index = index * 6 + 6;
						unsigned int destination_index = sprite_index - 6;
						text->buffer[index - displacement] = text->buffer[index];
						for (size_t subindex = destination_index; subindex < sprite_index; subindex++) {
							vertices[subindex - sprite_displacement] = vertices[subindex];
							vertices[subindex - sprite_displacement].position.x -= deleted_sprite_span;
						}
					}

					text->size -= displacement;
					vertices.size -= sprite_displacement;
					current_selection = current_sprite_position;
					if (sprite_render_offset > 0) {
						if (sprite_render_offset < displacement) {
							sprite_render_offset = 0;
						}
						else {
							sprite_render_offset -= displacement;
						}
					}
					caret_start = std::chrono::high_resolution_clock::now();
					is_caret_display = true;
					return true;
				}
			}
			text->buffer[text->size] = '\0';
			return false;
		}

		void UIDrawerTextInput::CopyCharacters(UISystem* system) {
			constexpr size_t max_characters = 4096;

			char characters[max_characters];
			unsigned int copy_index, copy_count;
			if (current_sprite_position < current_selection) {
				copy_index = current_sprite_position;
				copy_count = current_selection - current_sprite_position;
			}
			else {
				copy_index = current_selection;
				copy_count = current_sprite_position - current_selection;
			}
			ECS_ASSERT(copy_count < max_characters);
			memcpy(characters, &text->buffer[copy_index], copy_count);
			characters[copy_count] = '\0';
			system->m_application->WriteTextToClipboard(characters);
		}

		void UIDrawerTextInput::DeleteAllCharacters()
		{
			trigger_callback = true;

			text->buffer[0] = '\0';
			text->size = 0;
			vertices.size = 0;
			current_sprite_position = 0;
			current_selection = 0;
			sprite_render_offset = 0;
		}

		void UIDrawerTextInput::InsertCharacters(const char* characters, unsigned int character_count, unsigned int character_position, UISystem* system)
		{
			trigger_callback = true;

			if (character_count + text->size > text->capacity) {
				character_count = text->capacity - text->size;
			}

			float2 current_position;
			if (current_sprite_position > 0) {
				current_position = {
				vertices[current_sprite_position * 6 - 5].position.x + character_spacing,
				-vertices[current_sprite_position * 6 - 6].position.y
				};
			}
			else {
				float y_position = system->AlignMiddleTextY(position.y, solid_color_y_scale, font_size.y, padding.y);
				current_position = {
					position.x + padding.x,
					y_position
				};
			}

			filter_characters_start = current_sprite_position;
			filter_character_count = character_count;
			text->DisplaceElements(current_sprite_position, character_count);
			vertices.DisplaceElements(current_sprite_position * 6, character_count * 6);
			memcpy(text->buffer + current_selection, characters, sizeof(unsigned char) * character_count);
			system->ConvertCharactersToTextSprites(
				{ characters, character_count },
				current_position,
				vertices.buffer,
				text_color,
				current_sprite_position * 6,
				font_size,
				character_spacing
			);

			Stream<UISpriteVertex> new_vertices = Stream<UISpriteVertex>(vertices.buffer + current_sprite_position * 6, character_count * 6);
			float2 text_span = GetTextSpan(new_vertices);
			float translation = text_span.x + character_spacing;
			if (current_sprite_position == 0) {
				translation -= character_spacing;
			}

			current_sprite_position += character_count;
			text->size += character_count;
			vertices.size += character_count * 6;

			current_selection = current_sprite_position;
			for (size_t index = current_sprite_position * 6; index < vertices.size; index++) {
				vertices[index].position.x += translation;
			}

			unsigned int visible_sprites = GetVisibleSpriteCount(bound);
			if (sprite_render_offset + visible_sprites < current_sprite_position) {
				if (character_count >= visible_sprites) {
					sprite_render_offset += character_count - visible_sprites;
				}
				else {
					sprite_render_offset += character_count;
				}
			}
			text->buffer[text->size] = '\0';
		}

		bool UIDrawerSlider::IsTheSameData(const UIDrawerSlider* other) const
		{
			return other != nullptr && value_to_change == other->value_to_change;
		}

		float2* UIDrawerSlider::TextPosition()
		{
			return label.TextPosition();
		}

		float2* UIDrawerSlider::TextScale()
		{
			return label.TextScale();
		}

		UISpriteVertex* UIDrawerSlider::TextBuffer()
		{
			return label.TextBuffer();
		}

		unsigned int* UIDrawerSlider::TextSize()
		{
			return label.TextSize();
		}

		CapacityStream<UISpriteVertex>* UIDrawerSlider::TextStream()
		{
			return label.TextStream();
		}

		float2 UIDrawerSlider::GetZoom() const
		{
			return label.GetZoom();
		}

		float2 UIDrawerSlider::GetInverseZoom() const
		{
			return label.GetInverseZoom();
		}

		float UIDrawerSlider::GetZoomX() const
		{
			return label.GetZoomX();
		}

		float UIDrawerSlider::GetZoomY() const
		{
			return label.GetZoomY();
		}

		float UIDrawerSlider::GetInverseZoomX() const
		{
			return label.GetInverseZoomX();
		}

		float UIDrawerSlider::GetInverseZoomY() const
		{
			return label.GetInverseZoomY();
		}

		void UIDrawerSlider::SetZoomFactor(float2 zoom)
		{
			label.SetZoomFactor(zoom);
		}

		void UIDrawerSlider::Callback(ActionData* action_data)
		{
			if (changed_value_callback.action != nullptr) {
				action_data->data = changed_value_callback.data;
				changed_value_callback.action(action_data);
			}
		}

		void UIDrawerColorInput::Callback(ActionData* action_data)
		{
			if (callback.action != nullptr) {
				action_data->data = callback.data;
				callback.action(action_data);
			}
		}

		float2* UIDrawerColorInput::TextPosition()
		{
			return name.TextPosition();
		}

		float2* UIDrawerColorInput::TextScale()
		{
			return name.TextScale();
		}

		UISpriteVertex* UIDrawerColorInput::TextBuffer()
		{
			return name.TextBuffer();
		}

		unsigned int* UIDrawerColorInput::TextSize()
		{
			return name.TextSize();
		}

		CapacityStream<UISpriteVertex>* UIDrawerColorInput::TextStream()
		{
			return name.TextStream();
		}

		float UIDrawerColorInput::GetLowestX() const
		{
			return name.GetLowestX();
		}

		float UIDrawerColorInput::GetLowestY() const
		{
			return name.GetLowestY();
		}

		float2 UIDrawerColorInput::GetLowest() const
		{
			return name.GetLowest();
		}

		float2 UIDrawerColorInput::GetZoom() const
		{
			return name.GetZoom();
		}

		float2 UIDrawerColorInput::GetInverseZoom() const
		{
			return name.GetInverseZoom();
		}

		float UIDrawerColorInput::GetZoomX() const
		{
			return name.GetZoomX();
		}

		float UIDrawerColorInput::GetZoomY() const
		{
			return name.GetZoomY();
		}

		float UIDrawerColorInput::GetInverseZoomX() const
		{
			return name.GetInverseZoomX();
		}

		float UIDrawerColorInput::GetInverseZoomY() const
		{
			return name.GetInverseZoomY();
		}

		void UIDrawerColorInput::SetZoomFactor(float2 zoom)
		{
			name.SetZoomFactor(zoom);
		}

		bool UIDrawerTextInputActionData::IsTheSameData(const UIDrawerTextInputActionData* other) const
		{
			return other != nullptr && input->IsTheSameData(other->input);
		}

		bool UIDrawerSliderEnterValuesData::IsTheSameData(const UIDrawerSliderEnterValuesData* other) const
		{
			return other != nullptr && slider->IsTheSameData(other->slider);
		}

}

}