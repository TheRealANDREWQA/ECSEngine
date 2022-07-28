#include "ecspch.h"
#include "UIDrawerStructures.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "UIHelpers.h"
#include "UIDrawConfig.h"

namespace ECSEngine {

	namespace Tools {

		float2* UIDrawerTextElement::TextPosition()
		{
			return &position;
		}

		float2* UIDrawerTextElement::TextScale()
		{
			return &scale;
		}

		UISpriteVertex* UIDrawerTextElement::TextBuffer()
		{
			return text_vertices.buffer;
		}

		unsigned int* UIDrawerTextElement::TextSize()
		{
			return &text_vertices.size;
		}

		CapacityStream<UISpriteVertex>* UIDrawerTextElement::TextStream()
		{
			return &text_vertices;
		}

		float UIDrawerTextElement::GetLowestX() const
		{
			float min = text_vertices[0].position.x;
			for (size_t index = 6; index < text_vertices.size; index += 6) {
				min = function::Select(min > text_vertices[index].position.x, text_vertices[index].position.x, min);
			}
			return min;
		}

		float UIDrawerTextElement::GetLowestY() const {
			float min = text_vertices[0].position.y;
			for (size_t index = 6; index < text_vertices.size; index += 6) {
				min = function::Select(min > text_vertices[index].position.y, text_vertices[index].position.y, min);
			}
			return min;
		}

		float2 UIDrawerTextElement::GetLowest() const {
			float2 min = { text_vertices[0].position.x, text_vertices[0].position.y };
			for (size_t index = 6; index < text_vertices.size; index += 6) {
				min.x = function::Select(min.x > text_vertices[index].position.x, text_vertices[index].position.x, min.x);
				min.y = function::Select(min.y > text_vertices[index].position.y, text_vertices[index].position.y, min.y);
			}
			return min;
		}

		float2 UIDrawerTextElement::GetZoom() const
		{
			return zoom;
		}

		float2 UIDrawerTextElement::GetInverseZoom() const
		{
			return inverse_zoom;
		}

		float UIDrawerTextElement::GetZoomX() const
		{
			return zoom.x;
		}

		float UIDrawerTextElement::GetZoomY() const
		{
			return zoom.y;
		}

		float UIDrawerTextElement::GetInverseZoomX() const
		{
			return inverse_zoom.x;
		}

		float UIDrawerTextElement::GetInverseZoomY() const
		{
			return inverse_zoom.y;
		}

		void UIDrawerTextElement::SetZoomFactor(float2 _zoom)
		{
			zoom = _zoom;
			inverse_zoom = { 1.0f / _zoom.x, 1.0f / _zoom.y };
		}

		void UIDrawerHierarchy::AddNode(const UIDrawerHierarchyNode& node, unsigned int position) {
			nodes.ReserveNewElements(1);
			for (int64_t index = nodes.size - 1; index >= position; index--) {
				nodes[index + 1] = nodes[index];
			}
			memcpy(nodes.buffer + position, &node, sizeof(UIDrawerHierarchyNode));
			nodes.size++;
		}

		void UIDrawerHierarchy::Clear() {
			for (size_t index = 0; index < nodes.size; index++) {
				DeallocateNode(index);
			}
			nodes.FreeBuffer();
			selectable.selected_index = 0;
		}

		void UIDrawerHierarchy::DeallocateThis() {
			if (data_size != 0) {
				system->m_memory->Deallocate(data);
				system->RemoveWindowMemoryResource(window_index, data);
			}

			nodes.FreeBuffer();
			pending_initializations.FreeBuffer();
			system->m_memory->Deallocate(this);
			system->RemoveWindowMemoryResource(window_index, this);
		}

		void UIDrawerHierarchy::DeallocateNode(unsigned int index) {
			for (size_t index = 0; index < nodes[index].internal_allocations.size; index++) {
				system->m_memory->Deallocate(nodes[index].internal_allocations[index]);
				system->RemoveWindowMemoryResource(window_index, nodes[index].internal_allocations[index]);
			}
			nodes[index].internal_allocations.FreeBuffer();
			system->m_memory->Deallocate(nodes[index].name_element.text_vertices.buffer);
		}

		unsigned int UIDrawerHierarchy::FindNode(const char* name) const {
			size_t name_length = strlen(name);

			for (size_t index = 0; index < nodes.size; index++) {
				if (name_length == nodes[index].name_length) {
					if (memcmp(name, nodes[index].name, name_length) == 0) {
						return index;
					}
				}
			}
			return -1;
		}

		void* UIDrawerHierarchy::GetData()
		{
			return data;
		}

		UIDrawerHierarchyNode UIDrawerHierarchy::GetNode(unsigned int index) const
		{
			return nodes[index];
		}

		void UIDrawerHierarchy::SetSelectedNode(unsigned int index)
		{
			selectable.selected_index = index;
		}

		void UIDrawerHierarchy::SetData(void* _data, unsigned int _data_size)
		{
			if (_data_size > 0) {
				data = system->m_memory->Allocate(_data_size);
				system->AddWindowMemoryResource(data, window_index);
				memcpy(data, _data, _data_size);
				data_size = _data_size;
			}
			else {
				data = _data;
			}
		}

		void UIDrawerHierarchy::SetChildData(UIDrawConfig& config)
		{
			UIConfigHierarchyChild child_data;
			child_data.parent = this;
			config.AddFlag(child_data);
		}

		void UIDrawerHierarchy::RemoveNode(unsigned int index)
		{
			DeallocateNode(index);
			nodes.Remove(index);

			// if a pending initialization node was bumped back, restore its reference
			for (size_t subindex = 0; subindex < pending_initializations.size; subindex++) {
				if (pending_initializations[subindex].bound_node >= index + 1) {
					pending_initializations[subindex].bound_node--;
					break;
				}
			}
			if (selectable.selected_index == index) {
				selectable.selected_index = 0;
			}
		}

		void UIDrawerHierarchy::RemoveNodeWithoutDeallocation(unsigned int index)
		{
			nodes.Remove(index);
		}

		void* UIDrawerList::AddNode(
			const char* name,
			UINodeDraw draw,
			UINodeDraw initialize
		) {
			return hierarchy.AddNode<true>(name, draw, initialize);
		}

		void UIDrawerList::InitializeNodeYScale(size_t* _counts)
		{
			for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
				counts[index] = _counts[index];
			}
		}

		void UIDrawerList::FinalizeNodeYscale(const void** buffers, size_t* _counts)
		{
			const UIVertexColor* solid_color = (const UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
			const UISpriteVertex* text_sprites = (const UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE];
			const UISpriteVertex* sprites = (const UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE];

			float2 solid_color_bounds = GetSectionYBounds(solid_color, counts[ECS_TOOLS_UI_SOLID_COLOR], _counts[ECS_TOOLS_UI_SOLID_COLOR]);
			float2 text_sprites_bounds = GetRectangleSectionYBounds(text_sprites, counts[ECS_TOOLS_UI_TEXT_SPRITE], _counts[ECS_TOOLS_UI_TEXT_SPRITE]);
			float2 sprites_bounds = GetRectangleSectionYBounds(sprites, counts[ECS_TOOLS_UI_SPRITE], _counts[ECS_TOOLS_UI_SPRITE]);

			float2 bounds = solid_color_bounds;
			bounds.x = function::Select(text_sprites_bounds.x < bounds.x, text_sprites_bounds.x, bounds.x);
			bounds.x = function::Select(sprites_bounds.x < bounds.x, sprites_bounds.x, bounds.x);
			bounds.y = function::Select(text_sprites_bounds.y > bounds.y, text_sprites_bounds.y, bounds.y);
			bounds.y = function::Select(sprites_bounds.y > bounds.y, sprites_bounds.y, bounds.y);

			hierarchy_extra->row_y_scale = bounds.y - bounds.x;
		}

		void UIDrawerSentenceBase::SetWhitespaceCharacters(const char* characters, size_t character_count, char parse_token)
		{
			unsigned int temp_chars[256];
			Stream<unsigned int> temp_stream = { temp_chars, 0 };
			function::FindWhitespaceCharacters(temp_stream, characters, parse_token);

			for (size_t index = 0; index < temp_stream.size; index++) {
				whitespace_characters[index].position = temp_stream[index];
				whitespace_characters[index].type = characters[temp_stream[index]];
			}
			whitespace_characters.size = temp_stream.size;
			whitespace_characters.Add({ (unsigned short)character_count, parse_token });
		}

		UIDrawerSliderFunctions UIDrawerGetFloatSliderFunctions(unsigned int& precision)
		{
			UIDrawerSliderFunctions result;

			auto convert_text_input = [](CapacityStream<char>& characters, void* _value) {
				float character_value = function::ConvertCharactersToFloat(characters);
				float* value = (float*)_value;
				*value = character_value;
			};

			auto to_string = [](CapacityStream<char>& characters, const void* _value, void* extra_data) {
				characters.size = 0;
				function::ConvertFloatToChars(characters, *(const float*)_value, *(unsigned int*)extra_data);
			};

			auto from_float = [](void* value, float float_percentage) {
				float* float_value = (float*)value;
				*float_value = float_percentage;
			};

			auto to_float = [](const void* value) {
				return *(float*)value;
			};

			result.convert_text_input = convert_text_input;
			result.to_string = to_string;
			result.extra_data = &precision;
			result.interpolate = UIDrawerSliderInterpolateImplementation<float>;
			result.is_smaller = UIDrawerSliderIsSmallerImplementation<float>;
			result.percentage = UIDrawerSliderPercentageImplementation<float>;
			result.from_float = from_float;
			result.to_float = to_float;

			return result;
		}

		UIDrawerSliderFunctions UIDrawerGetDoubleSliderFunctions(unsigned int& precision)
		{
			UIDrawerSliderFunctions result;

			auto convert_text_input = [](CapacityStream<char>& characters, void* _value) {
				double character_value = function::ConvertCharactersToDouble(characters);
				double* value = (double*)_value;
				*value = character_value;
			};

			auto to_string = [](CapacityStream<char>& characters, const void* _value, void* extra_data) {
				characters.size = 0;
				function::ConvertDoubleToChars(characters, *(const double*)_value, *(unsigned int*)extra_data);
			};

			auto from_float = [](void* value, float float_percentage) {
				double* double_value = (double*)value;
				*double_value = float_percentage;
			};

			auto to_float = [](const void* value) {
				return (float)(*(double*)value);
			};

			result.convert_text_input = convert_text_input;
			result.to_string = to_string;
			result.extra_data = &precision;
			result.interpolate = UIDrawerSliderInterpolateImplementation<double>;
			result.is_smaller = UIDrawerSliderIsSmallerImplementation<double>;
			result.percentage = UIDrawerSliderPercentageImplementation<double>;
			result.from_float = from_float;
			result.to_float = to_float;

			return result;
		}

	}

}