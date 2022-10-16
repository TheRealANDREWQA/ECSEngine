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

		unsigned int UIDrawerTextInput::GetSpritePositionFromMouse(const UISystem* system, float2 mouse_position) const
		{
			float2* positions = (float2*)ECS_STACK_ALLOC(sizeof(float2) * text->size);
			float2* scales = (float2*)ECS_STACK_ALLOC(sizeof(float2) * text->size);

			system->GetTextCharacterPositions(*text, font_size, character_spacing, positions, scales, position + padding);

			if (sprite_render_offset > 0) {
				mouse_position.x += positions[sprite_render_offset].x - positions[0].x;
			}

			if (mouse_position.x < positions[0].x) {
				return sprite_render_offset;
			}
			for (size_t index = 1; index < text->size; index++) {
				if (mouse_position.x < positions[index].x) {
					return index;
				}
			}

			return text->size;

		}

		void UIDrawerTextInput::SetSpritePositionFromMouse(const UISystem* system, float2 mouse_position) {
			current_sprite_position = GetSpritePositionFromMouse(system, mouse_position);
		}

		unsigned int UIDrawerTextInput::GetVisibleSpriteCount(const UISystem* system, float right_bound) const {
			size_t index;

			float2* positions = (float2*)ECS_STACK_ALLOC(sizeof(float2) * text->size);
			float2* scales = (float2*)ECS_STACK_ALLOC(sizeof(float2) * text->size);

			system->GetTextCharacterPositions(*text, font_size, character_spacing, positions, scales, position + padding);

			if (sprite_render_offset > 0) {
				right_bound += positions[sprite_render_offset].x - positions[0].x;
			}
			for (index = sprite_render_offset; index < text->size; index++) {
				unsigned int sprite_index = index * 6;
				if (positions[index].x + scales[index].x > right_bound)
					break;
			}
			return index - sprite_render_offset;
		}

		float2 UIDrawerTextInput::GetCaretPosition(const UISystem* system) const
		{
			float2 caret_position;

			float2* positions = (float2*)ECS_STACK_ALLOC(sizeof(float2) * text->size);
			float2* scales = (float2*)ECS_STACK_ALLOC(sizeof(float2) * text->size);

			system->GetTextCharacterPositions(*text, font_size, character_spacing, positions, scales, position + padding);

			if (current_sprite_position == 0) {
				caret_position = position + padding;
			}
			else {
				caret_position.y = positions[0].y;

				if (current_sprite_position == text->size) {
					caret_position.x = positions[current_sprite_position - 1].x + scales[current_sprite_position - 1].x;
				}
				else {
					caret_position.x = positions[current_sprite_position].x;
				}
			}

			if (sprite_render_offset > 0) {
				caret_position.x -= positions[sprite_render_offset].x - positions[0].x;
			}

			caret_position.x = std::min(caret_position.x, bound);
			return caret_position;
		}

		float2 UIDrawerTextInput::GetPositionFromSprite(const UISystem* system, unsigned int index, bool bottom, bool left) const
		{
			unsigned int offset = left == false;
			index += offset;

			float2* positions = (float2*)ECS_STACK_ALLOC(sizeof(float2) * text->size);
			float2* scales = (float2*)ECS_STACK_ALLOC(sizeof(float2) * text->size);

			system->GetTextCharacterPositions(*text, font_size, character_spacing, positions, scales, position + padding);

			float2 sprite_position = { 0.0f, 0.0f };
			if (index < text->size) {
				sprite_position = positions[index];
				if (bottom) {
					sprite_position.y = positions[index].y + scales[index].y;
				}
			}
			else {
				sprite_position = positions[text->size - 1] + scales[text->size - 1];
				if (bottom) {
					sprite_position.y = positions[text->size - 1].y + scales[text->size - 1].y;
				}
				else {
					sprite_position.y = positions[0].y;
				}
			}

			if (sprite_render_offset > 0) {
				sprite_position.x -= positions[sprite_render_offset].x - positions[0].x;
			}

			return sprite_position;
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

				for (size_t index = current_sprite_position; index < text->size; index++) {
					text->buffer[index - 1] = text->buffer[index];
				}
				current_sprite_position--;
				text->size--;
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

					for (size_t index = start_iteration; index < text->size; index++) {
						text->buffer[index - displacement] = text->buffer[index];
					}

					text->size -= displacement;
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

			filter_characters_start = current_sprite_position;
			filter_character_count = character_count;
			text->DisplaceElements(current_sprite_position, character_count);
			memcpy(text->buffer + current_selection, characters, sizeof(unsigned char) * character_count);

			current_sprite_position += character_count;
			text->size += character_count;

			current_selection = current_sprite_position;

			unsigned int visible_sprites = GetVisibleSpriteCount(system, bound);
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

		void LabelHierarchyChangeStateData::GetCurrentLabel(void* storage) const {
			UIDrawerLabelHierarchyGetEmbeddedLabel(this, storage);
		}

		// Returns the total size of the structure with the string embedded
		unsigned int LabelHierarchyChangeStateData::WriteLabel(const void* untyped_data) {
			return UIDrawerLabelHierarchySetEmbeddedLabel(this, untyped_data);
		}

		void LabelHierarchyClickActionData::GetCurrentLabel(void* storage) const {
			UIDrawerLabelHierarchyGetEmbeddedLabel(this, storage);
		}

		// Returns the total size of the structure with the label embedded (both for normal string
		// or untyped data)
		unsigned int LabelHierarchyClickActionData::WriteLabel(const void* untyped_data) {
			return UIDrawerLabelHierarchySetEmbeddedLabel(this, untyped_data);
		}

	}

}