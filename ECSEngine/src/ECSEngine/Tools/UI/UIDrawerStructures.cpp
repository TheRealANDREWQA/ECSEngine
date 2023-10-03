#include "ecspch.h"
#include "UIDrawerStructures.h"
#include "UIHelpers.h"
#include "UIDrawConfig.h"
#include "../../Allocators/ResizableLinearAllocator.h"

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
			FindWhitespaceCharacters(temp_stream, characters, parse_token);

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
				float character_value = ConvertCharactersToFloat(characters);
				float* value = (float*)_value;
				*value = character_value;
			};

			auto to_string = [](CapacityStream<char>& characters, const void* _value, void* extra_data) {
				characters.size = 0;
				ConvertFloatToChars(characters, *(const float*)_value, *(unsigned int*)extra_data);
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
				double character_value = ConvertCharactersToDouble(characters);
				double* value = (double*)_value;
				*value = character_value;
			};

			auto to_string = [](CapacityStream<char>& characters, const void* _value, void* extra_data) {
				characters.size = 0;
				ConvertDoubleToChars(characters, *(const double*)_value, *(unsigned int*)extra_data);
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

		const void* UIDrawerLabelHierarchyDataAllocateLabel(
			Stream<char>& copy_label,
			UISystem* system,
			unsigned int window_index,
			unsigned int dynamic_index, 
			const void* label, 
			unsigned int label_size
		) {
			if (label_size == 0) {
				copy_label = *(Stream<char>*)label;
				copy_label = StringCopy(GetAllocatorPolymorphic(system->m_memory), copy_label);
				system->AddWindowMemoryResource(copy_label.buffer, window_index);
				if (dynamic_index != -1) {
					system->AddWindowDynamicElementAllocation(window_index, dynamic_index, copy_label.buffer);
				}
				return &copy_label;
			}
			return label;
		}

		void UIDrawerLabelHierarchyData::AddSelection(const void* label, ActionData* action_data)
		{
			UISystem* system = action_data->system;

			// Change the selected label now
			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			Stream<char> copy_label = { nullptr, 0 };
			label = UIDrawerLabelHierarchyDataAllocateLabel(copy_label, system, window_index, dynamic_index, label, label_size);
			unsigned int copy_size = CopySize();

			if (selected_labels.size > 0) {
				void* new_allocation = system->m_memory->Allocate(copy_size * (selected_labels.size + 1));
				selected_labels.CopyTo(new_allocation, copy_size);

				system->ReplaceWindowMemoryResource(window_index, selected_labels.buffer, new_allocation);
				if (dynamic_index != -1) {
					system->ReplaceWindowDynamicResourceAllocation(window_index, dynamic_index, selected_labels.buffer, new_allocation);
				}

				system->m_memory->Deallocate(selected_labels.buffer);
				selected_labels.buffer = new_allocation;
				selected_labels.SetElement(selected_labels.size, label, copy_size);
			}
			else {
				void* new_allocation = system->m_memory->Allocate(sizeof(Stream<char>));
				selected_labels.InitializeFromBuffer(new_allocation, 1);
				selected_labels.SetElement(0, label, copy_size);

				system->AddWindowMemoryResource(new_allocation, window_index);
				if (dynamic_index != -1) {
					system->AddWindowDynamicElementAllocation(window_index, dynamic_index, new_allocation);
				}
			}

			selected_labels.size++;
			TriggerSelectable(action_data);
		}

		void UIDrawerLabelHierarchyData::AddOpenedLabel(UISystem* system, const void* label)
		{
			unsigned int copy_size = CopySize();

			// Allocate the label into the opened stream
			void* new_buffer_allocation = system->m_memory->Allocate(copy_size * (opened_labels.size + 1));
			unsigned int dynamic_index = DynamicIndex(system, window_index);

			const void* old_buffer = opened_labels.buffer;
			if (opened_labels.size > 0) {
				memcpy(new_buffer_allocation, old_buffer, copy_size * opened_labels.size);
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

			opened_labels.buffer = new_buffer_allocation;

			Stream<char> copy_label = { nullptr, 0 };
			label = UIDrawerLabelHierarchyDataAllocateLabel(copy_label, system, window_index, dynamic_index, label, label_size);
			opened_labels.AddElement(label, copy_size);
		}

		void UIDrawerLabelHierarchyData::ChangeSelection(const void* label, ActionData* action_data)
		{
			unsigned int copy_size = CopySize();

			UISystem* system = action_data->system;

			// Change the selected label now
			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			Stream<char> copy_label = { nullptr, 0 };
			label = UIDrawerLabelHierarchyDataAllocateLabel(copy_label, system, window_index, dynamic_index, label, label_size);

			if (selected_labels.size > 0) {
				if (label_size == 0) {
					Stream<Stream<char>> char_labels = selected_labels.AsIs<Stream<char>>();
					// Keep the buffer allocation, just deallocate all the others
					for (size_t index = 0; index < char_labels.size; index++) {
						system->RemoveWindowBufferFromAll(window_index, char_labels[index].buffer, dynamic_index);
					}
				}
				// For the non label just leave it as it is, don't deallocate
			}
			else {			
				// Need to allocate the buffer first
				selected_labels.buffer = system->m_memory->Allocate(copy_size);
				system->AddWindowMemoryResource(selected_labels.buffer, window_index);
				if (dynamic_index != -1) {
					system->AddWindowDynamicElementAllocation(window_index, dynamic_index, selected_labels.buffer);
				}
			}

			selected_labels.size = 1;
			selected_labels.SetElement(0, label, copy_size);
			SetFirstSelectedLabel(label);

			TriggerSelectable(action_data);
		}

		void UIDrawerLabelHierarchyData::ChangeSelection(Stream<void> labels, ActionData* action_data) {
			UISystem* system = action_data->system;

			// Change the selected label now
			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			unsigned int copy_size = CopySize();

			if (selected_labels.size > 0) {
				if (label_size == 0) {
					Stream<Stream<char>> char_selected_labels = selected_labels.AsIs<Stream<char>>();
					for (size_t index = 0; index < char_selected_labels.size; index++) {
						system->RemoveWindowBufferFromAll(window_index, char_selected_labels[index].buffer, dynamic_index);
					}

				}
				//system->m_memory->Deallocate(selected_labels.buffer);
				system->RemoveWindowBufferFromAll(window_index, selected_labels.buffer, dynamic_index);
			}

			if (labels.size > 0) {
				// Need to allocate the buffer first
				selected_labels.buffer = system->m_memory->Allocate(copy_size * labels.size);
				system->AddWindowMemoryResource(selected_labels.buffer, window_index);
				if (dynamic_index != -1) {
					system->AddWindowDynamicElementAllocation(window_index, dynamic_index, selected_labels.buffer);
				}

				if (label_size == 0) {
					Stream<Stream<char>> char_labels = labels.AsIs<Stream<char>>();
					Stream<Stream<char>> char_selected_labels = selected_labels.AsIs<Stream<char>>();
					for (size_t index = 0; index < char_labels.size; index++) {
						char_selected_labels[index] = StringCopy(GetAllocatorPolymorphic(system->m_memory), char_labels[index]);
						system->AddWindowMemoryResource(char_selected_labels[index].buffer, window_index);
						if (dynamic_index != -1) {
							system->AddWindowDynamicElementAllocation(window_index, dynamic_index, char_selected_labels[index].buffer);
						}
					}
				}
				else {
					selected_labels.CopySlice(0, labels.buffer, copy_size * labels.size);
				}
				SetFirstSelectedLabel(labels.buffer);
			}
			else {
				first_selected_label.size = 0;
				selected_labels = { nullptr, 0 };
			}

			selected_labels.size = labels.size;

			TriggerSelectable(action_data);
		}

		void UIDrawerLabelHierarchyData::RemoveSelection(const void* label, ActionData* action_data)
		{
			UISystem* system = action_data->system;

			// Change the selected label now
			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			unsigned int copy_size = CopySize();

			unsigned int index = 0;
			if (label_size == 0) {
				index = FindString(*(Stream<char>*)label, selected_labels.AsIs<Stream<char>>());
				ECS_ASSERT(index != -1);
				system->RemoveWindowBufferFromAll(window_index, selected_labels.AsIs<Stream<char>>()[index].buffer, dynamic_index);
			}
			else {
				index = SearchBytesEx(selected_labels.buffer, selected_labels.size, label, copy_size);
				ECS_ASSERT(index != -1);
			}

			if (selected_labels.size > 1) {
				selected_labels.RemoveSwapBack(index, copy_size);
				// Don't deallocate it
			}
			else {
				// Remove the allocation
				system->RemoveWindowBufferFromAll(window_index, selected_labels.buffer, dynamic_index);
				selected_labels.size = 0;
			}

			TriggerSelectable(action_data);
		}

		void UIDrawerLabelHierarchyData::RemoveOpenedLabel(UISystem* system, const void* label)
		{
			unsigned int copy_size = CopySize();
			unsigned int dynamic_index = DynamicIndex(system, window_index);

			unsigned int opened_index = 0;
			if (label_size == 0) {
				Stream<Stream<char>> char_opened_labels = opened_labels.AsIs<Stream<char>>();
				opened_index = FindString(*(Stream<char>*)label, char_opened_labels);
				ECS_ASSERT(opened_index != -1);
				// Deallocate the string
				system->RemoveWindowBufferFromAll(window_index, char_opened_labels[opened_index].buffer, dynamic_index);
			}
			else {
				opened_index = SearchBytesEx(opened_labels.buffer, opened_labels.size, label, copy_size);
				ECS_ASSERT(opened_index != -1);
			}


			const void* old_buffer = opened_labels.buffer;
			// Remove it or replace it
			opened_labels.RemoveSwapBack(opened_index, copy_size);
			if (opened_labels.size > 0) {
				// Replace
				void* new_buffer_allocation = system->m_memory->Allocate(copy_size * opened_labels.size);
				opened_labels.CopyTo(new_buffer_allocation, copy_size);
				system->ReplaceWindowBufferFromAll(window_index, old_buffer, new_buffer_allocation, dynamic_index);
				opened_labels.buffer = new_buffer_allocation;
			}
			else {
				// Remove
				system->RemoveWindowBufferFromAll(window_index, old_buffer, dynamic_index);
			}
		}

		void UIDrawerLabelHierarchyData::ResetCopiedLabels(ActionData* action_data)
		{
			UISystem* system = action_data->system;

			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			// There is only a single coalesced allocation
			if (copied_labels.size > 0) {
				system->RemoveWindowBufferFromAll(window_index, copied_labels.buffer, dynamic_index);
			}
			copied_labels.size = 0;
		}

		void UIDrawerLabelHierarchyData::ClearSelection(ActionData* action_data)
		{
			UISystem* system = action_data->system;

			// Change the selected label now
			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			if (label_size == 0) {
				Stream<Stream<char>> char_selected_labels = selected_labels.AsIs<Stream<char>>();
				for (size_t index = 0; index < char_selected_labels.size; index++) {
					system->RemoveWindowBufferFromAll(window_index, char_selected_labels[index].buffer, dynamic_index);
				}
			}

			system->RemoveWindowBufferFromAll(window_index, selected_labels.buffer, dynamic_index);
			selected_labels.size = 0;

			TriggerSelectable(action_data);
		}

		bool UIDrawerLabelHierarchyData::CompareFirstLabel(const void* label) const
		{
			if (label_size == 0) {
				return *(Stream<char>*)label == first_selected_label.AsIs<char>();
			}
			else {
				return memcmp(label, first_selected_label.buffer, label_size) == 0;
			}
		}

		bool UIDrawerLabelHierarchyData::CompareLastLabel(const void* label) const
		{
			if (label_size == 0) {
				return *(Stream<char>*)label == last_selected_label.AsIs<char>();
			}
			else {
				return memcmp(label, last_selected_label.buffer, label_size) == 0;
			}
		}

		bool UIDrawerLabelHierarchyData::CompareLabels(const void* first, const void* second) const
		{
			if (label_size == 0) {
				return *(Stream<char>*)first == *(Stream<char>*)second;
			}
			else {
				return memcmp(first, second, label_size) == 0;
			}
		}

		unsigned int UIDrawerLabelHierarchyData::DynamicIndex(const UISystem* system, unsigned int window_index) const
		{
			unsigned int index = -1;
			if (identifier.size > 0) {
				index = system->GetWindowDynamicElement(window_index, identifier);
			}
			return index;
		}

		unsigned int FindLabel(Stream<void> labels, const void* label, unsigned int label_size) {
			if (label_size == 0) {
				return FindString(*(Stream<char>*)label, labels.AsIs<Stream<char>>());
			}
			else {
				return SearchBytesEx(labels.buffer, labels.size, label, label_size);
			}
		}

		unsigned int UIDrawerLabelHierarchyData::FindSelectedLabel(const void* label) const
		{
			return FindLabel(selected_labels, label, label_size);
		}

		unsigned int UIDrawerLabelHierarchyData::FindOpenedLabel(const void* label) const
		{
			return FindLabel(opened_labels, label, label_size);
		}

		unsigned int UIDrawerLabelHierarchyData::FindCopiedLabel(const void* label) const
		{
			return FindLabel(copied_labels, label, label_size);
		}

		unsigned int UIDrawerLabelHierarchyData::CopySize() const
		{
			return label_size == 0 ? sizeof(Stream<char>) : label_size;
		}

		void UIDrawerLabelHierarchyData::RecordSelection(ActionData* action_data)
		{
			UISystem* system = action_data->system;

			unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, identifier);

			// Deallocate these if they were not
			if (copied_labels.size > 0) {
				system->RemoveWindowBufferFromAll(window_index, copied_labels.buffer, dynamic_index);
			}

			unsigned int copy_size = CopySize();

			size_t total_size = copy_size * selected_labels.size;
			if (label_size == 0) {
				Stream<Stream<char>> char_selected_labels = selected_labels.AsIs<Stream<char>>();
				for (size_t index = 0; index < char_selected_labels.size; index++) {
					total_size += char_selected_labels[index].size * sizeof(char);
				}
			}

			void* allocation = system->m_memory->Allocate(total_size);
			uintptr_t ptr = (uintptr_t)allocation;

			copied_labels.size = selected_labels.size;
			copied_labels.buffer = (void*)ptr;
			ptr += copy_size * selected_labels.size;

			if (label_size == 0) {
				Stream<Stream<char>> char_selected_labels = selected_labels.AsIs<Stream<char>>();
				Stream<Stream<char>> char_opened_labels = opened_labels.AsIs<Stream<char>>();
				for (size_t index = 0; index < char_selected_labels.size; index++) {
					char_opened_labels[index].InitializeAndCopy(ptr, char_selected_labels[index]);
				}
			}
			else {
				copied_labels.CopySlice(0, selected_labels.buffer, selected_labels.size * copy_size);
			}

			system->AddWindowMemoryResource(copied_labels.buffer, window_index);
			if (dynamic_index != -1) {
				system->AddWindowDynamicElementAllocation(window_index, dynamic_index, copied_labels.buffer);
			}
		}

		void UIDrawerLabelHierarchyData::SetSelectionCut(bool is_cut) {
			is_selection_cut = is_cut;
		}

		void UIDrawerLabelHierarchySetCapacityLabel(CapacityStream<void>& label_to_set, const void* untyped_label, unsigned int label_size) {
			if (label_size == 0) {
				Stream<char> string = *(Stream<char>*)untyped_label;
				label_to_set.CopyOther(string.buffer, string.size);
			}
			else {
				label_to_set.CopyOther(untyped_label, label_size);
			}
		}

		void UIDrawerLabelHierarchyData::SetFirstSelectedLabel(const void* label)
		{
			UIDrawerLabelHierarchySetCapacityLabel(first_selected_label, label, label_size);
		}

		void UIDrawerLabelHierarchyData::SetLastSelectedLabel(const void* label)
		{
			UIDrawerLabelHierarchySetCapacityLabel(last_selected_label, label, label_size);
		}

		void UIDrawerLabelHierarchyData::SetHoveredLabel(const void* label)
		{
			UIDrawerLabelHierarchySetCapacityLabel(hovered_label, label, label_size);
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
				action_copy_data.destination_label = selected_labels.size == 0 ? nullptr : selected_labels.buffer;
				action_copy_data.source_labels = copied_labels;

				// Check to see if the destination is included in the source labels
				// If they are then set the destination to { nullptr, 0 }
				if (action_copy_data.destination_label != nullptr) {
					unsigned int destination_index = 0;
					if (label_size == 0) {
						destination_index = FindString(*(Stream<char>*)action_copy_data.destination_label, copied_labels.AsIs<Stream<char>>());
					}
					else {
						destination_index = SearchBytesEx(copied_labels.buffer, copied_labels.size, action_copy_data.destination_label, CopySize());
					}

					if (destination_index != -1) {
						action_copy_data.destination_label = nullptr;
					}
				}

				action_data->data = &action_copy_data;
				copy_action(action_data);
			}
		}

		void UIDrawerLabelHierarchyData::TriggerCut(ActionData* action_data) {
			if (cut_action != nullptr) {
				UIDrawerLabelHierarchyCutData action_cut_data;
				action_cut_data.data = cut_data;
				action_cut_data.destination_label = selected_labels.size == 0 ? nullptr : selected_labels.buffer;
				action_cut_data.source_labels = copied_labels;

				// Check to see if the destination is included in the source labels
				// If they are then set the destination to { nullptr, 0 }
				if (action_cut_data.destination_label != nullptr) {
					unsigned int destination_index = 0;
					if (label_size == 0) {
						destination_index = FindString(*(Stream<char>*)action_cut_data.destination_label, copied_labels.AsIs<Stream<char>>());
					}
					else {
						destination_index = SearchBytesEx(copied_labels.buffer, copied_labels.size, action_cut_data.destination_label, CopySize());
					}

					if (destination_index != -1) {
						action_cut_data.destination_label = nullptr;
					}
				}

				action_data->data = &action_cut_data;
				cut_action(action_data);
				// Clear the cut labels
				ResetCopiedLabels(action_data);
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

		void UIDrawerLabelHierarchyData::TriggerDoubleClick(ActionData* action_data)
		{
			if (double_click_action != nullptr) {
				// Call the double click action - if any
				UIDrawerLabelHierarchyDoubleClickData action_double_click_data;
				action_double_click_data.data = double_click_data;
				action_double_click_data.label = selected_labels.buffer;
				action_data->data = &action_double_click_data;
				double_click_action(action_data);
			}
		}

		void UIDrawerLabelHierarchyData::TriggerDrag(ActionData* action_data)
		{
			if (drag_action != nullptr) {
				Stream<char> hovered_char_label = hovered_label.AsIs<char>();
				void* untyped_label = &hovered_char_label;
				if (label_size != 0) {
					if (hovered_char_label.size > 0) {
						untyped_label = hovered_label.buffer;
					}
					else {
						untyped_label = nullptr;
					}
				}
				else {
					untyped_label = hovered_char_label.size == 0 ? nullptr : &hovered_char_label;
				}

				bool is_not_same_hover = untyped_label == nullptr ? true : FindSelectedLabel(untyped_label) == -1;
				if (!reject_same_label_drag || is_not_same_hover) {
					UIDrawerLabelHierarchyDragData action_drag_data;
					action_drag_data.data = drag_data;
					action_drag_data.destination_label = untyped_label;
					action_drag_data.source_labels = selected_labels;

					action_data->data = &action_drag_data;
					drag_action(action_data);

					if (untyped_label != nullptr) {
						// Add the label to the opened_labels
						AddOpenedLabel(action_data->system, untyped_label);
					}
				}
			}
		}

		void UIDrawerLabelHierarchyData::UpdateMonitorSelection(UIConfigLabelHierarchyMonitorSelection* monitor_selection) const
		{
			Stream<void> final_selection = selected_labels;
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 4);

			// Check the callback
			if (monitor_selection->callback.action != nullptr) {
				ActionData dummy_data;
				dummy_data.buffers = nullptr;
				dummy_data.counts = nullptr;
				dummy_data.system = nullptr;
				// We can reference it directly
				dummy_data.data = monitor_selection->callback.data;

				Stream<void> user_selected_labels = selected_labels.Copy(GetAllocatorPolymorphic(&stack_allocator), label_size == 0 ? sizeof(Stream<char>) : label_size);

				UIConfigLabelHierarchyMonitorSelectionInfo monitor_info;
				monitor_info.monitor_selection = monitor_selection;
				monitor_info.selected_labels = user_selected_labels;
				dummy_data.additional_data = &monitor_info;
				monitor_selection->callback.action(&dummy_data);

				final_selection = monitor_info.selected_labels;
				if (monitor_info.override_labels.size > 0) {
					final_selection = monitor_info.override_labels;
				}
			}

			// Deallocate the current labels
			monitor_selection->Deallocate(label_size != 0);

			if (label_size == 0) {
				// Strings
				Stream<Stream<char>> char_selected_labels = final_selection.AsIs<Stream<char>>();
				if (monitor_selection->is_capacity_selection) {
					CapacityStream<Stream<char>>* destination_labels = (CapacityStream<Stream<char>>*)monitor_selection->capacity_selection;
					ECS_ASSERT(char_selected_labels.size <= destination_labels->capacity);

					destination_labels->size = char_selected_labels.size;
					for (size_t index = 0; index < char_selected_labels.size; index++) {
						destination_labels->buffer[index].InitializeAndCopy(monitor_selection->Allocator(), char_selected_labels[index]);
					}
				}
				else {
					monitor_selection->resizable_selection->Resize(char_selected_labels.size);
					ResizableStream<Stream<char>>* destination_labels = (ResizableStream<Stream<char>>*)monitor_selection->resizable_selection;
					for (size_t index = 0; index < char_selected_labels.size; index++) {
						destination_labels->buffer[index].InitializeAndCopy(monitor_selection->Allocator(), char_selected_labels[index]);
					}
				}
			}
			else {
				// Blittable data
				if (monitor_selection->is_capacity_selection) {
					monitor_selection->capacity_selection->CopyOther(final_selection, label_size);
				}
				else {
					monitor_selection->resizable_selection->CopyOther(final_selection, label_size);
				}
			}

			if (monitor_selection->boolean_changed_flag) {
				*monitor_selection->is_changed = true;
			}
			else {
				// Increase the counter by 2 in order to have all windows be notified
				// Of the change - if there are windows A and B and B updates afterwards
				// the count it will be then decremented and A will not see the change
				(*monitor_selection->is_changed_counter) += 2;
			}
		}

		void UIDrawerLabelHierarchyRightClickData::GetLabel(void* storage) const
		{
			UIDrawerLabelHierarchyGetEmbeddedLabel(this, storage);
		}

		unsigned int UIDrawerLabelHierarchyRightClickData::WriteLabel(const void* untyped_label)
		{
			return UIDrawerLabelHierarchySetEmbeddedLabel(this, untyped_label);
		}

		unsigned int UIDrawerMenuButtonData::Write() const
		{
			unsigned int write_size = sizeof(*this);

			if (descriptor.window_name.size > 0) {
				memcpy(OffsetPointer(this, write_size), descriptor.window_name.buffer, descriptor.window_name.size);
				write_size += descriptor.window_name.size;
			}

			if (descriptor.window_data_size > 0) {
				memcpy(OffsetPointer(this, write_size), descriptor.window_data, descriptor.window_data_size);
				write_size += descriptor.window_data_size;
			}

			if (descriptor.private_action_data_size > 0) {
				memcpy(OffsetPointer(this, write_size), descriptor.private_action_data, descriptor.private_action_data_size);
				write_size += descriptor.private_action_data_size;
			}

			if (descriptor.destroy_action_data_size > 0) {
				memcpy(OffsetPointer(this, write_size), descriptor.destroy_action_data, descriptor.destroy_action_data_size);
				write_size += descriptor.destroy_action_data_size;
			}

			return write_size;
		}

		void UIDrawerMenuButtonData::Read()
		{
			unsigned int write_size = sizeof(*this);

			if (descriptor.window_name.size > 0) {
				descriptor.window_name.buffer = (char*)OffsetPointer(this, write_size);
				write_size += descriptor.window_name.size;
			}

			if (descriptor.window_data_size > 0) {
				descriptor.window_data = OffsetPointer(this, write_size);
				write_size += descriptor.window_data_size;
			}

			if (descriptor.private_action_data_size > 0) {
				descriptor.private_action_data = OffsetPointer(this, write_size);
				write_size += descriptor.private_action_data_size;
			}

			if (descriptor.destroy_action_data_size > 0) {
				descriptor.destroy_action_data = OffsetPointer(this, write_size);
				write_size += descriptor.destroy_action_data_size;
			}
		}

		UIDrawerMenuState* UIDrawerMenuDrawWindowData::GetState() const {
			if (submenu_offsets[0] == (unsigned char)-1) {
				return &menu->state;
			}

			UIDrawerMenuState* state = &menu->state.submenues[submenu_offsets[0]];
			for (unsigned char index = 1; index < 8; index++) {
				if (submenu_offsets[index] == (unsigned char)-1) {
					return state;
				}
				state = &state->submenues[submenu_offsets[index]];
			}

			return state;
		}

		unsigned char UIDrawerMenuDrawWindowData::GetLastPosition() const {
			unsigned char index = 0;
			for (; index < 8; index++) {
				if (submenu_offsets[index] == (unsigned char)-1) {
					return index;
				}
			}

			ECS_ASSERT(false, "Too many submenues");
			return 8;
		}

		void UIConfigLabelHierarchyMonitorSelection::Deallocate(bool is_blittable) {
			AllocatorPolymorphic allocator = Allocator();

			if (!is_blittable) {
				// Strings
				Stream<void> selection = Selection();
				Stream<Stream<char>> selection_strings = selection.AsIs<Stream<char>>();
				for (size_t index = 0; index < selection.size; index++) {
					selection_strings[index].Deallocate(allocator);
				}
			}

			if (is_capacity_selection) {
				capacity_selection->size = 0;
			}
			else {
				resizable_selection->FreeBuffer();
			}
		}

	}

}