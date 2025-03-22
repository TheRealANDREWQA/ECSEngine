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
			float min_value = text_vertices[0].position.x;
			for (size_t index = 6; index < text_vertices.size; index += 6) {
				min_value = min(min_value, text_vertices[index].position.x);
			}
			return min_value;
		}

		float UIDrawerTextElement::GetLowestY() const {
			float min_value = text_vertices[0].position.y;
			for (size_t index = 6; index < text_vertices.size; index += 6) {
				min_value = min(min_value, text_vertices[index].position.y);
			}
			return min_value;
		}

		float2 UIDrawerTextElement::GetLowest() const {
			float2 min_value = { text_vertices[0].position.x, text_vertices[0].position.y };
			for (size_t index = 6; index < text_vertices.size; index += 6) {
				min_value.x = min(min_value.x, text_vertices[index].position.x);
				min_value.y = min(min_value.y, text_vertices[index].position.y);
			}
			return min_value;
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

		void UIDrawerSentenceBase::SetWhitespaceCharacters(Stream<char> characters, char parse_token)
		{
			ECS_STACK_CAPACITY_STREAM(unsigned int, temp_stream, 256);
			FindWhitespaceCharacters(temp_stream, characters, parse_token);

			whitespace_characters.size = 0;
			whitespace_characters.AssertCapacity(temp_stream.size);
			for (size_t index = 0; index < temp_stream.size; index++) {
				whitespace_characters[index].position = temp_stream[index];
				whitespace_characters[index].type = characters[temp_stream[index]];
			}
			whitespace_characters.size = temp_stream.size;
			whitespace_characters.AddAssert({ (unsigned short)characters.size, parse_token });
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

		Stream<char> GetUIDrawerMenuStateRowString(const UIDrawerMenuState* state, unsigned int row_index)
		{
			unsigned int offset = row_index == 0 ? 0 : state->left_row_substreams[row_index - 1] + 1;
			unsigned int size = state->left_row_substreams[row_index] - offset;
			return Stream<char>(state->left_characters.buffer + offset, size);
		}

		size_t UIMenuCalculateStateMemory(const UIDrawerMenuState* state, bool copy_states) {
			size_t total_memory = 0;
			if (copy_states) {
				if (state->unavailables != nullptr) {
					total_memory += state->row_count * sizeof(bool);
				}
				if (state->row_has_submenu != nullptr) {
					total_memory += state->row_count * (sizeof(bool) + sizeof(UIDrawerMenuState));
				}

				total_memory += sizeof(UIActionHandler) * state->row_count;
				// Copy their data as well
				for (unsigned int index = 0; index < state->row_count; index++) {
					// Only if there is no submenu on that handler
					if (state->row_has_submenu == nullptr || !state->row_has_submenu[index]) {
						total_memory += state->click_handlers[index].data_size;
					}
				}
			}
			size_t left_character_count = state->left_characters.size;
			total_memory += left_character_count;

			if (state->right_characters.size > 0) {
				size_t right_character_count = state->right_characters.size;
				total_memory += right_character_count;
				// for the right substream
				if (state->row_count > 1) {
					total_memory += sizeof(unsigned short) * state->row_count;
				}
			}

			// for the left substream
			if (state->row_count > 1) {
				total_memory += sizeof(unsigned short) * state->row_count;
			}
			// for alignment
			total_memory += 8;
			return total_memory;
		}

		size_t UIMenuWalkStatesMemory(const UIDrawerMenuState* state, bool copy_states) {
			size_t total_memory = UIMenuCalculateStateMemory(state, copy_states);
			if (state->row_has_submenu != nullptr) {
				for (size_t index = 0; index < state->row_count; index++) {
					if (state->row_has_submenu[index]) {
						if (state->unavailables == nullptr || !state->unavailables[index]) {
							total_memory += UIMenuCalculateStateMemory(&state->submenues[index], copy_states);
						}
					}
				}
			}
			return total_memory;
		}

		void UIMenuSetStateBuffers(
			UIDrawerMenuState* state,
			uintptr_t& buffer,
			CapacityStream<UIDrawerMenuWindow>* stream,
			unsigned int submenu_index,
			bool copy_states
		) {
			char* left_buffer = (char*)buffer;
			state->left_characters.CopyTo(buffer);
			state->left_characters.buffer = left_buffer;

			size_t right_character_count = state->right_characters.size;
			if (state->right_characters.size > 0) {
				char* new_buffer = (char*)buffer;
				state->right_characters.CopyTo(buffer);
				state->right_characters.buffer = new_buffer;
			}

			buffer = AlignPointer(buffer, alignof(unsigned short));
			state->left_row_substreams = (unsigned short*)buffer;
			buffer += sizeof(unsigned short) * state->row_count;

			if (state->right_characters.size > 0) {
				state->right_row_substreams = (unsigned short*)buffer;
				buffer += sizeof(unsigned short) * state->row_count;
			}

			size_t new_line_count = 0;
			for (size_t index = 0; index < state->left_characters.size; index++) {
				if (state->left_characters[index] == '\n') {
					state->left_row_substreams[new_line_count++] = index;
				}
			}
			state->left_row_substreams[new_line_count] = state->left_characters.size;

			if (state->right_characters.size > 0) {
				new_line_count = 0;
				for (size_t index = 0; index < right_character_count; index++) {
					if (state->right_characters[index] == '\n') {
						state->right_row_substreams[new_line_count++] = index;
					}
				}
				state->right_row_substreams[new_line_count] = right_character_count;
			}

			if (copy_states) {
				if (state->row_has_submenu != nullptr) {
					memcpy((void*)buffer, state->row_has_submenu, sizeof(bool) * state->row_count);
					state->row_has_submenu = (bool*)buffer;
					buffer += sizeof(bool) * state->row_count;

					buffer = AlignPointer(buffer, alignof(UIDrawerMenuState));
					memcpy((void*)buffer, state->submenues, sizeof(UIDrawerMenuState) * state->row_count);
					state->submenues = (UIDrawerMenuState*)buffer;
					buffer += sizeof(UIDrawerMenuState) * state->row_count;
				}

				if (state->unavailables != nullptr) {
					memcpy((void*)buffer, state->unavailables, sizeof(bool) * state->row_count);
					state->unavailables = (bool*)buffer;
					buffer += sizeof(bool) * state->row_count;
				}

				buffer = AlignPointer(buffer, alignof(UIActionHandler));
				memcpy((void*)buffer, state->click_handlers, sizeof(UIActionHandler) * state->row_count);
				state->click_handlers = (UIActionHandler*)buffer;
				buffer += sizeof(UIActionHandler) * state->row_count;

				// Copy the handler data as well
				for (unsigned int index = 0; index < state->row_count; index++) {
					if (state->click_handlers[index].data_size > 0) {
						bool can_copy = state->row_has_submenu == nullptr || !state->row_has_submenu[index];
						if (can_copy) {
							void* current_buffer = (void*)buffer;
							memcpy(current_buffer, state->click_handlers[index].data, state->click_handlers[index].data_size);
							state->click_handlers[index].data = current_buffer;
							buffer += state->click_handlers[index].data_size;
						}
					}
				}
			}

			state->windows = stream;
			state->submenu_index = submenu_index;
		}

		void UIMenuWalkSetStateBuffers(UIDrawerMenuState* state, uintptr_t& buffer, CapacityStream<UIDrawerMenuWindow>* stream, unsigned int submenu_index, bool copy_states)
		{
			UIMenuSetStateBuffers(state, buffer, stream, submenu_index, copy_states);
			if (state->row_has_submenu != nullptr) {
				for (size_t index = 0; index < state->row_count; index++) {
					if (state->row_has_submenu[index]) {
						if (state->unavailables == nullptr || !state->unavailables[index]) {
							UIMenuWalkSetStateBuffers(&state->submenues[index], buffer, stream, submenu_index + 1, copy_states);
						}
					}
				}
			}
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
				copy_label = StringCopy(system->m_memory, copy_label);
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
						char_selected_labels[index] = StringCopy(system->m_memory, char_labels[index]);
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

		unsigned int UIDrawerLabelHierarchyData::DynamicIndex(const UISystem* system, unsigned int _window_index) const
		{
			unsigned int index = -1;
			if (identifier.size > 0) {
				index = system->GetWindowDynamicElement(_window_index, identifier);
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
				void* previous_set_data = action_data->data;

				UIDrawerLabelHierarchySelectableData current_data;
				current_data.data = selectable_data;
				current_data.labels = selected_labels;
				action_data->data = &current_data;
				selectable_action(action_data);

				// Restore the action data, to be safe
				action_data->data = previous_set_data;
			}
		}

		void UIDrawerLabelHierarchyData::TriggerCopy(ActionData* action_data) {
			if (copy_action != nullptr) {
				void* previous_set_data = action_data->data;

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
			
				// Restore the action data, to be safe
				action_data->data = previous_set_data;
			}
		}

		void UIDrawerLabelHierarchyData::TriggerCut(ActionData* action_data) {
			if (cut_action != nullptr) {
				void* previous_set_data = action_data->data;

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

				// Restore the action data, to be safe
				action_data->data = previous_set_data;
			}
		}

		void UIDrawerLabelHierarchyData::TriggerDelete(ActionData* action_data) {
			if (delete_action != nullptr) {
				void* previous_set_data = action_data->data;

				UIDrawerLabelHierarchyDeleteData action_delete_data;
				action_delete_data.data = delete_data;
				action_delete_data.source_labels = selected_labels;
				action_data->data = &action_delete_data;

				delete_action(action_data);
				ClearSelection(action_data);

				// Restore the action data, to be safe
				action_data->data = previous_set_data;
			}
		}

		void UIDrawerLabelHierarchyData::TriggerDoubleClick(ActionData* action_data)
		{
			if (double_click_action != nullptr) {
				void* previous_set_data = action_data->data;

				// Call the double click action - if any
				UIDrawerLabelHierarchyDoubleClickData action_double_click_data;
				action_double_click_data.data = double_click_data;
				action_double_click_data.label = selected_labels.buffer;
				action_data->data = &action_double_click_data;
				double_click_action(action_data);

				// Restore the action data, to be safe
				action_data->data = previous_set_data;
			}
		}

		void UIDrawerLabelHierarchyData::TriggerDrag(ActionData* action_data, bool is_released)
		{
			if (drag_action != nullptr) {
				void* previous_set_data = action_data->data;

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
					action_drag_data.is_released = is_released;
					action_drag_data.has_started = !is_dragging;

					action_data->data = &action_drag_data;
					drag_action(action_data);

					if (untyped_label != nullptr) {
						// Add the label to the opened_labels
						AddOpenedLabel(action_data->system, untyped_label);
					}
				}


				// Restore the action data, to be safe
				action_data->data = previous_set_data;
			}
		}

		void UIDrawerLabelHierarchyData::UpdateMonitorSelection()
		{
			Stream<void> final_selection = selected_labels;
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 4);

			// Check the callback
			if (monitor_selection.callback.action != nullptr) {
				ActionData dummy_data;
				dummy_data.buffers = {};
				dummy_data.system = nullptr;
				// We can reference it directly
				dummy_data.data = monitor_selection.callback.data;

				Stream<void> user_selected_labels = selected_labels.Copy(&stack_allocator, label_size == 0 ? sizeof(Stream<char>) : label_size);

				UIConfigLabelHierarchyMonitorSelectionInfo monitor_info;
				monitor_info.monitor_selection = &monitor_selection;
				monitor_info.selected_labels = user_selected_labels;
				dummy_data.additional_data = &monitor_info;
				monitor_selection.callback.action(&dummy_data);

				final_selection = monitor_info.selected_labels;
				if (monitor_info.override_labels.size > 0) {
					final_selection = monitor_info.override_labels;
				}
			}

			// Deallocate the current labels
			monitor_selection.Deallocate(label_size != 0);

			if (label_size == 0) {
				// Strings
				Stream<Stream<char>> char_selected_labels = final_selection.AsIs<Stream<char>>();
				if (monitor_selection.is_capacity_selection) {
					CapacityStream<Stream<char>>* destination_labels = (CapacityStream<Stream<char>>*)monitor_selection.capacity_selection;
					ECS_ASSERT(char_selected_labels.size <= destination_labels->capacity);

					destination_labels->size = char_selected_labels.size;
					for (size_t index = 0; index < char_selected_labels.size; index++) {
						destination_labels->buffer[index].InitializeAndCopy(monitor_selection.Allocator(), char_selected_labels[index]);
					}
				}
				else {
					monitor_selection.resizable_selection->Resize(char_selected_labels.size);
					ResizableStream<Stream<char>>* destination_labels = (ResizableStream<Stream<char>>*)monitor_selection.resizable_selection;
					for (size_t index = 0; index < char_selected_labels.size; index++) {
						destination_labels->buffer[index].InitializeAndCopy(monitor_selection.Allocator(), char_selected_labels[index]);
					}
				}
			}
			else {
				// Blittable data
				if (monitor_selection.is_capacity_selection) {
					monitor_selection.capacity_selection->CopyOther(final_selection, label_size);
				}
				else {
					monitor_selection.resizable_selection->CopyOther(final_selection, label_size);
				}
			}

			if (monitor_selection.boolean_changed_flag) {
				*monitor_selection.is_changed = true;
			}
			else {
				// Increase the counter by 2 in order to have all windows be notified
				// Of the change - if there are windows A and B and B updates afterwards
				// the count it will be then decremented and A will not see the change
				(*monitor_selection.is_changed_counter) += 2;
			}
		}

		void UIDrawerLabelHierarchyRightClickData::GetLabel(void* storage) const
		{
			UIDrawerLabelHierarchyGetEmbeddedLabel(this, storage);
		}

		unsigned int UIDrawerLabelHierarchyRightClickData::WriteLabel(const void* untyped_label, size_t storage_capacity)
		{
			return UIDrawerLabelHierarchySetEmbeddedLabel(this, untyped_label, storage_capacity);
		}

		unsigned int UIDrawerMenuButtonData::Write(size_t storage_capacity) const
		{
			unsigned int write_size = sizeof(*this);

			if (descriptor.window_name.size > 0) {
				ECS_ASSERT(write_size + descriptor.window_name.size <= storage_capacity);
				memcpy(OffsetPointer(this, write_size), descriptor.window_name.buffer, descriptor.window_name.size);
				write_size += descriptor.window_name.size;
			}

			if (descriptor.window_data_size > 0) {
				ECS_ASSERT(write_size + descriptor.window_data_size <= storage_capacity);
				memcpy(OffsetPointer(this, write_size), descriptor.window_data, descriptor.window_data_size);
				write_size += descriptor.window_data_size;
			}

			if (descriptor.private_action_data_size > 0) {
				ECS_ASSERT(write_size + descriptor.private_action_data_size <= storage_capacity);
				memcpy(OffsetPointer(this, write_size), descriptor.private_action_data, descriptor.private_action_data_size);
				write_size += descriptor.private_action_data_size;
			}

			if (descriptor.destroy_action_data_size > 0) {
				ECS_ASSERT(write_size + descriptor.destroy_action_data_size <= storage_capacity);
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