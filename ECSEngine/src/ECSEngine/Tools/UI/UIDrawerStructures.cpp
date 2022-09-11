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
				min = std::min(min, text_vertices[index].position.x);
			}
			return min;
		}

		float UIDrawerTextElement::GetLowestY() const {
			float min = text_vertices[0].position.y;
			for (size_t index = 6; index < text_vertices.size; index += 6) {
				min = std::min(min, text_vertices[index].position.y);
			}
			return min;
		}

		float2 UIDrawerTextElement::GetLowest() const {
			float2 min = { text_vertices[0].position.x, text_vertices[0].position.y };
			for (size_t index = 6; index < text_vertices.size; index += 6) {
				min.x = std::min(min.x, text_vertices[index].position.x);
				min.y = std::min(min.y, text_vertices[index].position.y);
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
			bounds.x = std::min(bounds.x, text_sprites_bounds.x);
			bounds.x = std::min(bounds.x, sprites_bounds.x);
			bounds.y = std::max(bounds.y, text_sprites_bounds.y);
			bounds.y = std::max(bounds.y, sprites_bounds.y);

			hierarchy_extra->row_y_scale = bounds.y - bounds.x;
		}

		void UIDrawerSentenceBase::SetWhitespaceCharacters(Stream<char> characters, char parse_token)
		{
			unsigned int temp_chars[256];
			Stream<unsigned int> temp_stream = { temp_chars, 0 };
			function::FindWhitespaceCharacters(temp_stream, characters, parse_token);

			for (size_t index = 0; index < temp_stream.size; index++) {
				whitespace_characters[index].position = temp_stream[index];
				whitespace_characters[index].type = characters[temp_stream[index]];
			}
			whitespace_characters.size = temp_stream.size;
			whitespace_characters.Add({ (unsigned short)characters.size, parse_token });
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

		void UIDrawerLabelHierarchyData::AddSelection(Stream<char> label, ActionData* action_data)
		{
			UISystem* system = action_data->system;

			unsigned int window_index = system->GetWindowIndexFromBorder(action_data->dockspace, action_data->border_index);
			// Change the selected label now
			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			label = function::StringCopy(GetAllocatorPolymorphic(system->m_memory), label);
			system->AddWindowMemoryResource(label.buffer, window_index);
			if (dynamic_index != -1) {
				system->AddWindowDynamicElementAllocation(window_index, dynamic_index, label.buffer);
			}

			if (selected_labels.size > 0) {
				void* new_allocation = system->m_memory->Allocate(sizeof(Stream<char>) * (selected_labels.size + 1));
				selected_labels.CopyTo(new_allocation);
				
				system->ReplaceWindowMemoryResource(window_index, selected_labels.buffer, new_allocation);
				if (dynamic_index != -1) {
					system->ReplaceWindowDynamicResourceAllocation(window_index, dynamic_index, selected_labels.buffer, new_allocation);
				}

				system->m_memory->Deallocate(selected_labels.buffer);
				selected_labels.buffer = (Stream<char>*)new_allocation;
				selected_labels[selected_labels.size] = label;
			}
			else {
				void* new_allocation = system->m_memory->Allocate(sizeof(Stream<char>));
				selected_labels.InitializeFromBuffer(new_allocation, 1);
				selected_labels[0] = label;

				system->AddWindowMemoryResource(new_allocation, window_index);
				if (dynamic_index != -1) {
					system->AddWindowDynamicElementAllocation(window_index, dynamic_index, new_allocation);
				}
			}

			selected_labels.size++;
			TriggerSelectable(action_data);
		}

		void UIDrawerLabelHierarchyData::AddOpenedLabel(UISystem* system, unsigned int window_index, Stream<char> label)
		{
			// Allocate the label into the opened stream
			void* new_buffer_allocation = system->m_memory->Allocate(sizeof(Stream<char>) * (opened_labels.size + 1));
			unsigned int dynamic_index = DynamicIndex(system, window_index);

			const void* old_buffer = opened_labels.buffer;
			if (opened_labels.size > 0) {
				memcpy(new_buffer_allocation, old_buffer, sizeof(Stream<char>) * opened_labels.size);
				// Replace the buffer
				system->ReplaceWindowBufferFromAll(window_index, old_buffer, new_buffer_allocation, dynamic_index);
			}
			else {
				// Must add this allocation
				system->AddWindowMemoryResource(new_buffer_allocation, window_index);
				if (dynamic_index != -1) {
					system->AddWindowDynamicElementAllocation(window_index, dynamic_index, new_buffer_allocation);
				}
			}

			opened_labels.buffer = (Stream<char>*)new_buffer_allocation;
			label = function::StringCopy(GetAllocatorPolymorphic(system->m_memory), label);
			opened_labels.Add(label);
			system->AddWindowMemoryResource(label.buffer, window_index);
			if (dynamic_index != -1) {
				system->AddWindowDynamicElementAllocation(window_index, dynamic_index, label.buffer);
			}
		}

		void UIDrawerLabelHierarchyData::ChangeSelection(Stream<char> label, ActionData* action_data)
		{
			UISystem* system = action_data->system;

			unsigned int window_index = system->GetWindowIndexFromBorder(action_data->dockspace, action_data->border_index);
			// Change the selected label now
			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			if (selected_labels.size > 0) {
				// Keep the buffer allocation, just deallocate all the others
				for (size_t index = 1; index < selected_labels.size; index++) {
					system->RemoveWindowBufferFromAll(window_index, selected_labels[index].buffer, dynamic_index);
				}

				// Replace the first one
				void* old_buffer = selected_labels[0].buffer;
				selected_labels[0] = function::StringCopy(GetAllocatorPolymorphic(system->m_memory), label);
				system->ReplaceWindowBufferFromAll(window_index, old_buffer, selected_labels[0].buffer, dynamic_index);
			}
			else {
				// Need to allocate the buffer first
				selected_labels.buffer = (Stream<char>*)system->m_memory->Allocate(sizeof(Stream<char>));
				selected_labels[0] = function::StringCopy(GetAllocatorPolymorphic(system->m_memory), label);

				system->AddWindowMemoryResource(selected_labels.buffer, window_index);
				system->AddWindowMemoryResource(selected_labels[0].buffer, window_index);
				if (dynamic_index != -1) {
					system->AddWindowDynamicElementAllocation(window_index, dynamic_index, selected_labels.buffer);
					system->AddWindowDynamicElementAllocation(window_index, dynamic_index, selected_labels[0].buffer);
				}
			}

			selected_labels.size = 1;
			first_selected.Copy(label);

			TriggerSelectable(action_data);
		}

		void UIDrawerLabelHierarchyData::ChangeSelection(Stream<Stream<char>> labels, ActionData* action_data) {
			UISystem* system = action_data->system;

			unsigned int window_index = system->GetWindowIndexFromBorder(action_data->dockspace, action_data->border_index);
			// Change the selected label now
			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			if (selected_labels.size > 0) {
				// Keep the buffer allocation, just deallocate all the others
				for (size_t index = 0; index < selected_labels.size; index++) {
					system->RemoveWindowBufferFromAll(window_index, selected_labels[index].buffer, dynamic_index);
				}

				// Replace the first one
				system->RemoveWindowBufferFromAll(window_index, selected_labels.buffer, dynamic_index);
			}

			// Need to allocate the buffer first
			selected_labels.buffer = (Stream<char>*)system->m_memory->Allocate(labels.MemoryOf(labels.size));
			system->AddWindowMemoryResource(selected_labels.buffer, window_index);
			if (dynamic_index != -1) {
				system->AddWindowDynamicElementAllocation(window_index, dynamic_index, selected_labels.buffer);
			}

			for (size_t index = 0; index < labels.size; index++) {
				selected_labels[index] = function::StringCopy(GetAllocatorPolymorphic(system->m_memory), labels[index]);
				system->AddWindowMemoryResource(selected_labels[index].buffer, window_index);
				if (dynamic_index != -1) {
					system->AddWindowDynamicElementAllocation(window_index, dynamic_index, selected_labels[index].buffer);
				}
			}

			selected_labels.size = labels.size;
			first_selected.Copy(labels[0]);

			TriggerSelectable(action_data);
		}

		void UIDrawerLabelHierarchyData::RemoveSelection(Stream<char> label, ActionData* action_data)
		{
			UISystem* system = action_data->system;

			unsigned int window_index = system->GetWindowIndexFromBorder(action_data->dockspace, action_data->border_index);
			// Change the selected label now
			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			unsigned int index = function::FindString(label, selected_labels);
			ECS_ASSERT(index != -1);

			system->RemoveWindowBufferFromAll(window_index, selected_labels[index].buffer, dynamic_index);

			if (selected_labels.size > 1) {
				selected_labels.RemoveSwapBack(index);
				// Don't deallocate it
			}
			else {
				// Remove the allocation
				system->RemoveWindowBufferFromAll(window_index, selected_labels.buffer, dynamic_index);
				selected_labels.size = 0;
			}

			TriggerSelectable(action_data);
		}

		void UIDrawerLabelHierarchyData::RemoveOpenedLabel(UISystem* system, unsigned int window_index, Stream<char> label)
		{
			unsigned int opened_index = function::FindString(label, opened_labels);
			unsigned int dynamic_index = DynamicIndex(system, window_index);

			const void* old_buffer = opened_labels.buffer;
			// Remove it or replace it
			opened_labels.RemoveSwapBack(opened_index);
			if (opened_labels.size > 0) {
				// Replace
				void* new_buffer_allocation = system->m_memory->Allocate(sizeof(Stream<char>) * opened_labels.size);
				opened_labels.CopyTo(new_buffer_allocation);
				system->ReplaceWindowBufferFromAll(window_index, old_buffer, new_buffer_allocation, dynamic_index);
				opened_labels.buffer = (Stream<char>*)new_buffer_allocation;
			}
			else {
				// Remove
				system->RemoveWindowBufferFromAll(window_index, old_buffer, dynamic_index);
			}
		}

		void UIDrawerLabelHierarchyData::ClearSelection(ActionData* action_data)
		{
			UISystem* system = action_data->system;

			unsigned int window_index = system->GetWindowIndexFromBorder(action_data->dockspace, action_data->border_index);
			// Change the selected label now
			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			for (size_t index = 0; index < selected_labels.size; index++) {
				system->RemoveWindowBufferFromAll(window_index, selected_labels[index].buffer, dynamic_index);
			}

			system->RemoveWindowBufferFromAll(window_index, selected_labels.buffer, dynamic_index);
			selected_labels.size = 0;

			TriggerSelectable(action_data);
		}

		unsigned int UIDrawerLabelHierarchyData::DynamicIndex(const UISystem* system, unsigned int window_index) const
		{
			unsigned int index = -1;
			if (identifier.size > 0) {
				index = system->GetWindowDynamicElement(window_index, identifier);
			}
			return index;
		}

		void UIDrawerLabelHierarchyData::RecordSelection(ActionData* action_data)
		{
			UISystem* system = action_data->system;
			unsigned int window_index = system->GetWindowIndexFromBorder(action_data->dockspace, action_data->border_index);

			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			// Deallocate these if they were not
			if (copied_labels.size > 0) {
				system->RemoveWindowBufferFromAll(window_index, copied_labels.buffer, dynamic_index);
			}

			size_t total_size = StreamDeepCopySize(selected_labels);
			void* allocation = system->m_memory->Allocate(total_size);
			uintptr_t ptr = (uintptr_t)allocation;
			copied_labels = StreamDeepCopy(selected_labels, ptr);
			system->AddWindowMemoryResource(copied_labels.buffer, window_index);
			if (dynamic_index != -1) {
				system->AddWindowDynamicElementAllocation(window_index, dynamic_index, copied_labels.buffer);
			}
		}

		void UIDrawerLabelHierarchyData::SetSelectionCut(bool is_cut) {
			is_selection_cut = is_cut;
		}

		void UIDrawerLabelHierarchyData::TriggerSelectable(ActionData* action_data) {
			if (selectable_action != nullptr) {
				UIDrawerLabelHierarchySelectableData current_data;
				current_data.data = selectable_data;
				current_data.labels = selected_labels;
				action_data->data = &current_data;
				selectable_action(action_data);
			}
		}

		void UIDrawerLabelHierarchyData::TriggerCopy(ActionData* action_data) {
			if (copy_action != nullptr) {
				UIDrawerLabelHierarchyCopyData action_copy_data;
				action_copy_data.data = copy_data;
				action_copy_data.destination_label = selected_labels.size == 0 ? Stream<char>(nullptr, 0) : selected_labels[0];
				action_copy_data.source_labels = copied_labels;

				// Check to see if the destination is included in the source labels
				// If they are then set the destination to { nullptr, 0 }
				unsigned int destination_index = function::FindString(action_copy_data.destination_label, copied_labels);
				if (destination_index != -1) {
					action_copy_data.destination_label = { nullptr, 0 };
				}

				action_data->data = &action_copy_data;

				copy_action(action_data);
			}
		}

		void UIDrawerLabelHierarchyData::TriggerCut(ActionData* action_data) {
			if (cut_action != nullptr) {
				UIDrawerLabelHierarchyCutData action_cut_data;
				action_cut_data.data = cut_data;
				action_cut_data.destination_label = selected_labels.size == 0 ? Stream<char>(nullptr, 0) : selected_labels[0];
				action_cut_data.source_labels = copied_labels;
				action_data->data = &action_cut_data;

				// Check to see if the destination is included in the source labels
				// If they are then set the destination to { nullptr, 0 }
				unsigned int destination_index = function::FindString(action_cut_data.destination_label, copied_labels);
				if (destination_index != -1) {
					action_cut_data.destination_label = { nullptr, 0 };
				}

				cut_action(action_data);
			}
		}

		void UIDrawerLabelHierarchyData::TriggerDelete(ActionData* action_data) {
			if (delete_action != nullptr) {
				UIDrawerLabelHierarchyDeleteData action_delete_data;
				action_delete_data.data = delete_data;
				action_delete_data.source_labels = selected_labels;
				action_data->data = &action_delete_data;

				delete_action(action_data);
				ClearSelection(action_data);
			}
		}

	}

}