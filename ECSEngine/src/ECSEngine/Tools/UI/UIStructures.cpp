#include "ecspch.h"
#include "UIStructures.h"
#include "../../Rendering/ColorUtilities.h"
#include "../../Rendering/Graphics.h"
#include "../../Containers/SoA.h"

#include "UIHelpers.h"

namespace ECSEngine {

	namespace Tools { 

		float2 UIVisibleDockspaceRegion::GetPosition() {
			float dockspace_mask = GetDockspaceMaskFromType(type);
			return float2(
				dockspace->transform.position.x + dockspace_mask * dockspace->borders[border_index].position,
				dockspace->transform.position.y + (1.0f - dockspace_mask) * dockspace->borders[border_index].position
			);
		}

		float2 UIVisibleDockspaceRegion::GetScale() {
			float dockspace_mask = GetDockspaceMaskFromType(type);
			return float2(
				dockspace->transform.scale.x * (1.0f - dockspace_mask) + (dockspace->borders[border_index + 1].position - dockspace->borders[border_index].position) * dockspace_mask,
				dockspace->transform.scale.y * dockspace_mask + (dockspace->borders[border_index + 1].position - dockspace->borders[border_index].position) * (1.0f - dockspace_mask)
			);
		}

		bool UIHandler::Add(float _position_x, float _position_y, float _scale_x, float _scale_y, UIActionHandler handler, UIHandlerCopyBuffers _copy_function)
		{
			if (position_x.size == position_x.capacity) {
				return false;
			}
			unsigned int size = position_x.size;
			position_x[size] = _position_x;
			position_y[size] = _position_y;
			scale_x[size] = _scale_x;
			scale_y[size] = _scale_y;
			action[size] = handler;
			copy_function[size] = _copy_function;
			position_x.size++;
			return true;
		}

		bool UIHandler::Add(float2 position, float2 scale, UIActionHandler handler, UIHandlerCopyBuffers copy_function) {
			return Add(position.x, position.y, scale.x, scale.y, handler, copy_function);
		}

		unsigned int UIHandler::AddResizable(AllocatorPolymorphic allocator, float2 position, float2 scale, UIActionHandler handler, UIHandlerCopyBuffers copy_function) {
			unsigned int current_size = position_x.size;
			if (!Add(position, scale, handler, copy_function)) {
				Resize(allocator, (float)position_x.capacity * ECS_RESIZABLE_STREAM_FACTOR + 2.0f);
				Add(position, scale, handler, copy_function);
			}
			return current_size;
		}

		void UIHandler::Clip(unsigned int first_action_index, unsigned int last_action_index, const Rectangle2D& clip_rectangle) {
			last_action_index = position_x.size;

			// PERFORMANCE TODO: At the moment, we do not have SIMD variants, if need be, those can be implemented
			for (unsigned int index = first_action_index; index < last_action_index; index++) {
				Rectangle2D current_rectangle = Rectangle2D::FromScale({ position_x[index], position_y[index] }, { scale_x[index], scale_y[index] });
				current_rectangle = ClipRectangle(current_rectangle, clip_rectangle);
				position_x[index] = current_rectangle.top_left.x;
				position_y[index] = current_rectangle.top_left.y;
				scale_x[index] = current_rectangle.GetScale().x;
				scale_y[index] = current_rectangle.GetScale().y;
			}
		}

		UIHandler UIHandler::Copy(AllocatorPolymorphic allocator, void** coalesced_action_data) const
		{
			UIHandler copy = *this;
			SoACopy(allocator, position_x.size, position_x.size, &copy.position_x.buffer, &copy.position_y, &copy.scale_x, &copy.scale_y, &copy.action, &copy.copy_function);
			copy.position_x.capacity = position_x.size;

			if (coalesced_action_data != nullptr) {
				size_t total_data_size = 0;
				for (unsigned int index = 0; index < copy.position_x.size; index++) {
					total_data_size += copy.action[index].data_size;
				}

				void* allocation = nullptr;
				if (total_data_size > 0) {
					allocation = AllocateEx(allocator, total_data_size);
					uintptr_t ptr = (uintptr_t)allocation;
					for (unsigned int index = 0; index < copy.position_x.size; index++) {
						if (copy.action[index].data_size > 0) {
							void* previous_data = copy.action[index].data;
							copy.action[index].data = (void*)ptr;
							memcpy(copy.action[index].data, previous_data, copy.action[index].data_size);
							ptr += copy.action[index].data_size;
						}
					}
				}
				*coalesced_action_data = allocation;
			}
			else {
				for (unsigned int index = 0; index < copy.position_x.size; index++) {
					copy.action[index].data = CopyNonZeroEx(allocator, copy.action[index].data, copy.action[index].data_size);
				}
			}

			return copy;
		}

		void UIHandler::CopyData(
			const UIHandler* other,
			AllocatorPolymorphic buffers_allocator,
			AllocatorPolymorphic handler_data_allocator,
			AllocatorPolymorphic handler_system_data_allocator
		)
		{
			Reset();
			if (position_x.capacity < other->position_x.size) {
				Resize(buffers_allocator, other->position_x.size);
			}

			SoACopyDataOnly(
				other->position_x.size,
				position_x.buffer,
				other->position_x.buffer,
				position_y,
				other->position_y,
				scale_x,
				other->scale_x,
				scale_y,
				other->scale_y,
				action,
				other->action,
				copy_function,
				other->copy_function
			);

			position_x.size = other->position_x.size;

			for (unsigned int index = 0; index < position_x.size; index++) {
				AllocatorPolymorphic allocator = handler_data_allocator;
				if (action[index].phase == ECS_UI_DRAW_SYSTEM) {
					if (handler_system_data_allocator.allocator != nullptr) {
						allocator = handler_system_data_allocator;
					}
				}
				action[index].data = CopyNonZero(allocator, action[index].data, action[index].data_size);
			}
		}

		void UIHandler::Deallocate(AllocatorPolymorphic allocator) const
		{
			if (allocator.allocator == nullptr) {
				if (position_x.capacity > 0) {
					Free(position_x.buffer);
				}
			}
			else {
				DeallocateIfBelongs(allocator, position_x.buffer);
			}
		}

		void UIHandler::Execute(unsigned int action_index, ActionData* action_data) const {
			action_data->data = action[action_index].data;
			action_data->position = { position_x[action_index], position_y[action_index] };
			action_data->scale = { scale_x[action_index], scale_y[action_index] };
			action[action_index].action(action_data);
		}

		void UIHandler::Insert(AllocatorPolymorphic allocator, float2 position, float2 scale, UIActionHandler handler, UIHandlerCopyBuffers _copy_function, unsigned int insert_index)
		{
			unsigned int current_size = position_x.size;
			if (current_size == position_x.capacity) {
				Resize(allocator, position_x.capacity * ECS_RESIZABLE_STREAM_FACTOR + 2);
			}

			SoADisplaceElements(current_size, insert_index, 1, position_x.buffer, position_y, scale_x, scale_y, action, copy_function);
			position_x[insert_index] = position.x;
			position_y[insert_index] = position.y;
			scale_x[insert_index] = scale.x;
			scale_y[insert_index] = scale.y;
			action[insert_index] = handler;
			copy_function[insert_index] = _copy_function;
			position_x.size++;
		}

		void UIHandler::Resize(AllocatorPolymorphic allocator, size_t new_count)
		{
			SoAResize(allocator, position_x.size, new_count, &position_x.buffer, &position_y, &scale_x, &scale_y, &action, &copy_function);
			position_x.capacity = new_count;
		}

		unsigned int UIHandler::ReserveOne(AllocatorPolymorphic allocator)
		{
			if (position_x.size == position_x.capacity) {
				Resize(allocator, position_x.size * ECS_RESIZABLE_STREAM_FACTOR + 2);
			}
			unsigned int index = position_x.size;
			position_x.size++;
			return index;
		}

		void UIHandler::Reset() {
			position_x.size = 0;
		}

		void UIHandler::WriteOnIndex(unsigned int index, float2 position, float2 scale, UIActionHandler handler, UIHandlerCopyBuffers _copy_function)
		{
			position_x[index] = position.x;
			position_y[index] = position.y;
			scale_x[index] = scale.x;
			scale_y[index] = scale.y;
			action[index] = handler;
			copy_function[index] = _copy_function;
		}

		void UIWindowDescriptor::Center(float2 size) {
			initial_size_x = size.x;
			initial_size_y = size.y;
			initial_position_x = AlignMiddle(-1.0f, 2.0f, size.x);
			initial_position_y = AlignMiddle(-1.0f, 2.0f, size.y);
		}

		bool UIFocusedWindowData::ExecuteHoverableHandler(ActionData* action_data)
		{
			if (hoverable_handler.action != nullptr) {
				action_data->data = hoverable_handler.data;
				action_data->position = hoverable_transform.position;
				action_data->scale = hoverable_transform.scale;
				action_data->additional_data = additional_hoverable_data;
				hoverable_handler.action(action_data);
				return true;
			}
			return false;
		}

		bool UIFocusedWindowData::ExecuteClickableHandler(ActionData* action_data, ECS_MOUSE_BUTTON button_type)
		{
			if (clickable_handler[button_type].action != nullptr) {
				action_data->data = clickable_handler[button_type].data;
				action_data->position = mouse_click_transform[button_type].position;
				action_data->scale = mouse_click_transform[button_type].scale;
				clickable_handler[button_type].action(action_data);
				return true;
			}
			return false;
		}

		bool UIFocusedWindowData::ExecuteGeneralHandler(ActionData* action_data)
		{
			if (general_handler.action != nullptr) {
				action_data->data = general_handler.data;
				action_data->position = general_transform.position;
				action_data->scale = general_transform.scale;
				action_data->additional_data = additional_general_data;
				general_handler.action(action_data);
				return true;
			}
			return false;
		}

		void UIFocusedWindowData::ResetHoverableHandler()
		{
			hoverable_handler.action = nullptr;
			hoverable_handler.data = nullptr;
			hoverable_handler.data_size = 0;
			hoverable_handler.phase = ECS_UI_DRAW_NORMAL;
			hoverable_handler_allocator.Clear();
			clean_up_call_hoverable = false;
			//always_hoverable = false;
		}

		void UIFocusedWindowData::ResetClickableHandler(ECS_MOUSE_BUTTON button_type)
		{
			clickable_handler[button_type].action = nullptr;
			clickable_handler[button_type].data = nullptr;
			clickable_handler[button_type].data_size = 0;
			clickable_handler[button_type].phase = ECS_UI_DRAW_NORMAL;
			clickable_handler_allocator[button_type].Clear();
		}

		void UIFocusedWindowData::ResetGeneralHandler()
		{
			general_handler.action = nullptr;
			general_handler.data = nullptr;
			general_handler.data_size = 0;
			general_handler.phase = ECS_UI_DRAW_NORMAL;
			general_handler_allocator.Clear();
			clean_up_call_general = false;
		}

		void* UIFocusedWindowData::ChangeClickable(float2 position, float2 scale, const UIActionHandler* handler, UIHandlerCopyBuffers copy_function, ECS_MOUSE_BUTTON button_type)
		{
			clickable_handler_allocator[button_type].Clear();

			mouse_click_transform[button_type] = { position, scale };
			clickable_handler[button_type] = *handler;
			clickable_handler[button_type].data = CopyNonZero(&clickable_handler_allocator[button_type], handler->data, handler->data_size);
			if (copy_function) {
				copy_function(clickable_handler[button_type].data, &clickable_handler_allocator[button_type]);
			}

			return clickable_handler[button_type].data;
		}

		void* UIFocusedWindowData::ChangeGeneralHandler(float2 position, float2 scale, const UIActionHandler* handler, UIHandlerCopyBuffers copy_function)
		{
			general_handler_allocator.Clear();

			general_transform.position = position;
			general_transform.scale = scale;
			general_handler = *handler;
			general_handler.data = CopyNonZero(&general_handler_allocator, handler->data, handler->data_size);
			if (copy_function) {
				copy_function(general_handler.data, &general_handler_allocator);
			}
			clean_up_call_general = false;

			return general_handler.data;
		}

		void* UIFocusedWindowData::ChangeHoverableHandler(float2 position, float2 scale, const UIActionHandler* handler, UIHandlerCopyBuffers copy_function)
		{
			hoverable_handler_allocator.Clear();

			hoverable_transform.position = position;
			hoverable_transform.scale = scale;
			hoverable_handler = *handler;
			hoverable_handler.data = CopyNonZero(&hoverable_handler_allocator, handler->data, handler->data_size);
			if (copy_function) {
				copy_function(hoverable_handler.data, &hoverable_handler_allocator);
			}
			clean_up_call_hoverable = false;

			return hoverable_handler.data;
		}

		void* UIFocusedWindowData::ChangeHoverableHandler(const UIHandler* handler, unsigned int index)
		{
			return ChangeHoverableHandler(
				handler->GetPositionFromIndex(index),
				handler->GetScaleFromIndex(index),
				&handler->GetActionFromIndex(index),
				handler->GetCopyFunctionFromIndex(index)
			);
		}

		void* UIFocusedWindowData::ChangeClickable(
			const UIHandler* handler,
			unsigned int index,
			ECS_MOUSE_BUTTON button_type
		)
		{
			return ChangeClickable(
				handler->GetPositionFromIndex(index),
				handler->GetScaleFromIndex(index),
				&handler->GetActionFromIndex(index),
				handler->GetCopyFunctionFromIndex(index),
				button_type
			);
		}

		void* UIFocusedWindowData::ChangeGeneralHandler(
			const UIHandler* handler,
			unsigned int index
		)
		{
			return ChangeGeneralHandler(
				handler->GetPositionFromIndex(index),
				handler->GetScaleFromIndex(index),
				&handler->GetActionFromIndex(index),
				handler->GetCopyFunctionFromIndex(index)
			);
		}

		void UIDrawResources::Map(void** void_buffers, GraphicsContext* context)
		{
			for (size_t index = 0; index < buffers.size; index++) {
				void_buffers[index] = MapBuffer(buffers[index].buffer, context);
			}
		}

		void UIDrawResources::UnmapNormal(GraphicsContext* context) {
			for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
				UnmapBuffer(buffers[index].buffer, context);
			}
		}

		void UIDrawResources::UnmapLate(GraphicsContext* context) {
			Unmap(context, ECS_TOOLS_UI_MATERIALS, ECS_TOOLS_UI_MATERIALS * 2);
		}

		void UIDrawResources::UnmapAll(GraphicsContext* context) {
			Unmap(context, 0, buffers.size);
		}

		void UIDrawResources::Unmap(GraphicsContext* context, unsigned int starting_index, unsigned int end_index)
		{
			for (size_t index = starting_index; index < end_index; index++) {
				UnmapBuffer(buffers[index].buffer, context);
			}
		}

		void UIDrawResources::ReleaseSpriteTextures()
		{
			for (size_t index = 0; index < sprite_textures.size; index++) {
				for (size_t subindex = 0; subindex < sprite_textures[index].size; subindex++) {
					sprite_textures[index][subindex].view->Release();
				}
			}
		}

		void UIDrawResources::Release(Graphics* graphics)
		{
			graphics->FreeResource(region_viewport_info);
			for (size_t index = 0; index < buffers.size; index++) {
				graphics->FreeResource(buffers[index]);
			}

			for (size_t index = 0; index < sprite_textures.size; index++) {
				sprite_textures[index].FreeBuffer();
			}
		}

		void UIColorThemeDescriptor::SetNewTheme(Color new_theme)
		{
			theme = new_theme;
			title = ToneColor(new_theme, ECS_TOOLS_UI_WINDOW_TITLE_COLOR_FACTOR);
			inactive_title = ToneColor(new_theme, ECS_TOOLS_UI_WINDOW_TITLE_INACTIVE_COLOR_FACTOR);
			region_header_hover_x = ToneColor(new_theme, ECS_TOOLS_UI_WINDOW_REGION_HEADER_COLOR_CLOSE_X_HOVER_COLOR_FACTOR);
			region_header = ToneColor(new_theme, ECS_TOOLS_UI_WINDOW_REGION_HEADER_COLOR_FACTOR);
			docking_gizmo_background = ToneColor(new_theme, ECS_TOOLS_UI_DOCKING_GIZMO_COLOR_FACTOR);
			hovered_borders = ToneColor(new_theme, ECS_TOOLS_UI_WINDOW_BORDER_HOVERED_COLOR_FACTOR);
		}

		HandlerCommand UIDefaultWindowHandler::PushRevertCommand(const HandlerCommand& command)
		{
			HandlerCommand deallocate_data;
			deallocate_data.handler.action = nullptr;
			deallocate_data.handler.data = nullptr;
			if (revert_commands.GetSize() == revert_commands.GetCapacity()) {
				const HandlerCommand* deallocate_command = revert_commands.PeekIntrusive();
				deallocate_data = *deallocate_command;
			}
			revert_commands.Push(command);
			return deallocate_data;
		}

		bool UIDefaultWindowHandler::PopRevertCommand(HandlerCommand& command) {
			return revert_commands.Pop(command);
		}

		bool UIDefaultWindowHandler::PeekRevertCommand(HandlerCommand& command) {
			return revert_commands.Peek(command);
		}

		HandlerCommand* UIDefaultWindowHandler::GetLastCommand() {
			return &last_command;
		}

		void UIDefaultWindowHandler::ChangeLastCommand(const HandlerCommand& command) {
			last_command = command;
		}

		void UIDefaultWindowHandler::ChangeCursor(ECS_CURSOR_TYPE cursor)
		{
			commit_cursor = cursor;
		}

		void UIDockspaceBorder::DeallocateSnapshot(AllocatorPolymorphic snapshot_allocator, bool free_runnable_allocator)
		{
			if (snapshot.IsValid()) {
				snapshot.Deallocate(snapshot_allocator, {}, false);
				// Clear the runnable data allocator as well
				snapshot_runnable_data_allocator.Clear();
			}
			if (free_runnable_allocator) {
				snapshot_runnable_data_allocator.Free();
			}
		}

		void UIDockspaceBorder::Reset()
		{
			hoverable_handler.Reset();
			for (size_t index = 0; index < ECS_MOUSE_BUTTON_COUNT; index++) {
				clickable_handler[index].Reset();
			}
			general_handler.Reset();
			//draw_resources.ReleaseSpriteTextures();
			for (size_t index = 0; index < draw_resources.sprite_textures.size; index++) {
				draw_resources.sprite_textures[index].Reset();
			}
			for (size_t index = 0; index < draw_resources.sprite_cluster_subtreams.size; index++) {
				draw_resources.sprite_cluster_subtreams[index].Reset();
			}
			// We don't need to deallocate the data pointers since they were allocated
			// Using the temporary allocator
			snapshot_runnables.FreeBuffer();
		}

		size_t UIDockspaceBorder::Serialize(void* buffer) const
		{
			/*
			struct ECSENGINE_API UIDockspaceBorderSerialized {
				float position;
				bool is_dock;
				bool draw_region_header;
				bool draw_close_x;
				bool draw_elements;
				unsigned char active_window;
				unsigned char window_count;
			private:
				unsigned char padding[1];
			};
			*/

			uintptr_t ptr = (uintptr_t)buffer;

			size_t size1 = sizeof(float) + sizeof(bool) * 4 + sizeof(unsigned char);
			memcpy((void*)ptr, this, size1);
			ptr += size1;

			unsigned char* ptr_short = (unsigned char*)ptr;
			*ptr_short = (unsigned char)window_indices.size;
			ptr += sizeof(unsigned char);

			//ECS_ASSERT(window_indices.size > 0, "No windows to serialize");
			memcpy((void*)ptr, window_indices.buffer, sizeof(unsigned short) * window_indices.size);
			ptr += sizeof(unsigned short) * window_indices.size;
			return ptr - (uintptr_t)buffer;
		}

		size_t UIDockspaceBorder::SerializeSize() const
		{
			size_t size = 0;

			size = sizeof(float) + sizeof(bool) * 4 + sizeof(unsigned char);
			size += sizeof(unsigned char);

			//ECS_ASSERT(window_indices.size > 0, "No windows to serialize");
			size += sizeof(unsigned short) * window_indices.size;
			return size;
		}

		size_t UIDockspaceBorder::LoadFromFile(const void* buffer)
		{
			uintptr_t ptr = (uintptr_t)buffer;
			memcpy(this, buffer, sizeof(float) + sizeof(bool) * 4 + sizeof(unsigned char));
			ptr += sizeof(float) + sizeof(bool) * 4 + sizeof(unsigned char);
			const unsigned char* ptr_window_indices_count = (const unsigned char*)ptr;
			window_indices.size = *ptr_window_indices_count;

			ptr += sizeof(unsigned char);
			//ECS_ASSERT(window_indices.size > 0, "No windows to serialize");
			memcpy(window_indices.buffer, (const void*)ptr, sizeof(unsigned short) * window_indices.size);
			ptr += sizeof(unsigned short) * window_indices.size;

			return ptr - (uintptr_t)buffer;
		}

		void UITooltipDrawData::NextRow()
		{
			position.x = initial_position.x + x_padding;
			position.y += current_scale.y;
		}

		void UITooltipDrawData::Indent()
		{
			position.x += indentation;
		}

		void UITooltipDrawData::FinalizeRectangle(float2 position, float2 scale)
		{
			position.x += scale.x;
			max_bounds.x = max(max_bounds.x, position.x);
			max_bounds.y = max(max_bounds.y, position.y);
			current_scale.x = 0.0f;
			current_scale.y = max(current_scale.y, scale.y);
		}

		void UIWindowDrawerDescriptor::UpdateZoom(float2 before_zoom, float2 current_zoom)
		{
			float2 inverse_zoom = float2(1.0f, 1.0f) / before_zoom;
			float2 factor = { inverse_zoom.x * current_zoom.x, inverse_zoom.y * current_zoom.y };

			font.size *= factor.x;
			font.character_spacing *= factor.x;

			layout.default_element_x *= factor.x;
			layout.default_element_y *= factor.y;
			layout.element_indentation *= factor.x;
			layout.next_row_padding *= factor.x;
			layout.next_row_y_offset *= factor.y;
			layout.node_indentation *= factor.x;

			element_descriptor.color_input_padd *= factor.x;
			element_descriptor.combo_box_padding *= factor.x;
			element_descriptor.graph_axis_bump *= factor;
			element_descriptor.graph_axis_value_line_size *= factor;
			element_descriptor.graph_padding *= factor;
			element_descriptor.graph_x_axis_space *= factor.x;
			element_descriptor.histogram_bar_spacing *= factor.x;
			element_descriptor.histogram_padding *= factor;
			element_descriptor.label_padd *= factor;
			element_descriptor.slider_length *= factor;
			element_descriptor.slider_shrink *= factor;
		}

		void UIWindowDrawerDescriptor::ChangeDimensionRatio(float2 current_ratio, float2 new_ratio)
		{
			// Even if the descriptors are not configured, they must still be updated
			element_descriptor.ChangeDimensionRatio(current_ratio, new_ratio);
			font.ChangeDimensionRatio(current_ratio, new_ratio);
			layout.ChangeDimensionRatio(current_ratio, new_ratio);
		}

		size_t UIWindow::Serialize(void* buffer) const
		{
			/*
			UIWindowDrawerDescriptor descriptors;
			unsigned short name_length;
			*/

			uintptr_t ptr = (uintptr_t)buffer;
			memcpy((void*)ptr, descriptors, sizeof(bool) * ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT);

			// configured descriptors
			ptr += sizeof(bool) * ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT;

			void* descriptor_ptrs[] = {
				&descriptors->color_theme,
				&descriptors->layout,
				&descriptors->font,
				&descriptors->element_descriptor
			};
			size_t descriptor_sizes[] = {
				sizeof(UIColorThemeDescriptor),
				sizeof(UILayoutDescriptor),
				sizeof(UIFontDescriptor),
				sizeof(UIElementDescriptor)
			};

			// descriptors
			for (size_t index = 0; index < ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT; index++) {
				if (descriptors->configured[index]) {
					memcpy((void*)ptr, descriptor_ptrs[index], descriptor_sizes[index]);
					ptr += descriptor_sizes[index];
				}
			}

			// name
			size_t name_length = name.size;
			unsigned short* ptr_name_length = (unsigned short*)ptr;
			*ptr_name_length = (unsigned short)name_length;
			ptr += sizeof(unsigned short);

			memcpy((void*)ptr, name.buffer, sizeof(char) * name_length);
			ptr += sizeof(char) * name_length;
			return ptr - (uintptr_t)buffer;
		}

		size_t UIWindow::SerializeSize() const
		{
			size_t size = 0;

			// configured descriptors
			size += sizeof(bool) * ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT;

			size_t descriptor_sizes[] = {
				sizeof(UIColorThemeDescriptor),
				sizeof(UILayoutDescriptor),
				sizeof(UIFontDescriptor),
				sizeof(UIElementDescriptor)
			};

			// descriptors
			for (size_t index = 0; index < ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT; index++) {
				if (descriptors->configured[index]) {
					size += descriptor_sizes[index];
				}
			}

			// name
			size_t name_length = name.size;
			size += sizeof(unsigned short);
			size += sizeof(char) * name_length;

			return size;
		}

		size_t UIWindow::LoadFromFile(const void* buffer, CapacityStream<char>& name_stack)
		{
			uintptr_t ptr = (uintptr_t)buffer;

			// descriptor configurations
			memcpy(descriptors->configured, (const void*)ptr, sizeof(bool) * ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT);

			void* descriptor_ptrs[] = {
				&descriptors->color_theme,
				&descriptors->layout,
				&descriptors->font,
				&descriptors->element_descriptor
			};
			size_t descriptor_sizes[] = {
				sizeof(UIColorThemeDescriptor),
				sizeof(UILayoutDescriptor),
				sizeof(UIFontDescriptor),
				sizeof(UIElementDescriptor)
			};

			ptr += sizeof(bool) * ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT;

			// configured descriptors
			for (size_t index = 0; index < ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT; index++) {
				if (descriptors->configured[index]) {
					memcpy(descriptor_ptrs[index], (const void*)ptr, descriptor_sizes[index]);
					ptr += descriptor_sizes[index];
				}
			}

			// name
			const unsigned short* name_length = (const unsigned short*)ptr;
			ptr += sizeof(unsigned short);
			name_stack.AddStreamAssert({ (char*)ptr, *name_length });
			name_stack.AddAssert('\0');
			name_stack.size--;
			ptr += sizeof(char) * name_stack.size;

			return ptr - (uintptr_t)buffer;
		}

		size_t UIDockspace::Serialize(void* buffer) const
		{
			uintptr_t ptr = (uintptr_t)buffer;
			memcpy((void*)ptr, this, sizeof(UIElementTransform));
			ptr += sizeof(UIElementTransform);

			unsigned char* ptr_char = (unsigned char*)ptr;
			*ptr_char = (unsigned char)borders.size;
			ptr += sizeof(unsigned char);
			bool* ptr_bool = (bool*)ptr;
			*ptr_bool = allow_docking;
			ptr += sizeof(bool);

			for (size_t index = 0; index < borders.size - 1; index++) {
				size_t bytes_written = borders[index].Serialize((void*)ptr);
				ptr += bytes_written;
			}

			return ptr - (uintptr_t)buffer;
		}

		size_t UIDockspace::SerializeSize() const
		{
			size_t size = 0;
			size += sizeof(UIElementTransform);

			size += sizeof(unsigned char);
			size += sizeof(bool);

			for (size_t index = 0; index < borders.size - 1; index++) {
				size += borders[index].SerializeSize();
			}

			return size;
		}

		size_t UIDockspace::LoadFromFile(const void* buffer, unsigned char& border_count)
		{
			uintptr_t ptr = (uintptr_t)buffer;
			memcpy(this, (const void*)ptr, sizeof(UIElementTransform));

			ptr += sizeof(UIElementTransform);
			ECS_ASSERT(transform.scale.x > 0.0f && transform.scale.y > 0.0f, "Invalid dockspace transform");
			const unsigned char* border_count_ptr = (const unsigned char*)ptr;
			border_count = *border_count_ptr;

			ptr += sizeof(unsigned char);
			ECS_ASSERT(border_count > 0);

			const bool* allow_dock = (const bool*)ptr;
			allow_docking = *allow_dock;
			ptr += sizeof(bool);

			return ptr - (uintptr_t)buffer;
		}

		size_t UIDockspace::LoadBordersFromFile(const void* buffer)
		{
			uintptr_t ptr = (uintptr_t)buffer;
			for (size_t index = 0; index < borders.size - 1; index++) {
				size_t bytes_read = borders[index].LoadFromFile((const void*)ptr);
				ptr += bytes_read;
			}
			return ptr - (uintptr_t)buffer;
		}

		void UIReservedHandler::Write(float2 position, float2 scale, UIActionHandler action_handler, UIHandlerCopyBuffers copy_function)
		{
			action_handler.data = CopyNonZero(allocator, action_handler.data, action_handler.data_size);
			handler->WriteOnIndex(index, position, scale, action_handler, copy_function);
		}

		void* UIReservedHandler::WrittenBuffer() const
		{
			return handler->action[index].data;
		}

		bool UIDoubleClickData::IsTheSameData(const UIDoubleClickData* other) const
		{
			if (other != nullptr) {
				if (other->is_identifier_int == is_identifier_int) {
					if (other->is_identifier_int) {
						return identifier == other->identifier;
					}
					else {
						Stream<char> identifier_string = { identifier_char, (size_t)identifier_char_count };
						Stream<char> other_identifier_string = { other->identifier_char, (size_t)other->identifier_char_count };
						return identifier_string == other_identifier_string;
					}
				}
			}
			return false;
		}

		bool UIDockspaceBorderDrawOutputSnapshot::IsValid() const
		{
			for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS * (ECS_TOOLS_UI_PASSES + 1); index++) {
				if (counts[index] != 0) {
					return true;
				}
			}
			return false;
		}

		bool UIDockspaceBorderDrawOutputSnapshot::Restore(UIDockspaceBorderDrawOutputSnapshotRestoreInfo* restore_info)
		{
			// Start with the buffers
			size_t vertex_byte_sizes[ECS_TOOLS_UI_MATERIALS] = {
				sizeof(UIVertexColor),
				sizeof(UISpriteVertex),
				sizeof(UISpriteVertex),
				sizeof(UISpriteVertex),
				sizeof(UIVertexColor)
			};

			for (size_t pass_index = 0; pass_index < ECS_TOOLS_UI_PASSES; pass_index++) {
				size_t offset = pass_index * ECS_TOOLS_UI_MATERIALS;
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
					if (counts[offset + index] > 0) {
						memcpy(restore_info->buffers[offset + index], buffers[offset + index], vertex_byte_sizes[index] * counts[offset + index]);
					}
					restore_info->counts[offset + index] = counts[offset + index];
				}
			}

			for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
				size_t offset = ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES;
				if (counts[offset + index] > 0) {
					size_t previous_count = restore_info->system_counts[index];
					memcpy(
						OffsetPointer(restore_info->system_buffers[index], previous_count * vertex_byte_sizes[index]),
						buffers[offset + index],
						vertex_byte_sizes[index] * counts[offset + index]
					);
				}
				restore_info->system_counts[index] += counts[offset + index];
			}

			
			for (size_t pass_index = 0; pass_index < ECS_TOOLS_UI_PASSES; pass_index++) {
				restore_info->border_cluster_sprite_count[pass_index].AddStream(cluster_sprite_counts[pass_index]);
				
				for (size_t index = 0; index < ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS; index++) {
					size_t current_index = pass_index * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS + index;
					Stream<UISpriteTexture> textures = sprites[current_index];					
					restore_info->border_sprite_textures[current_index].AddStream(sprites[current_index]);
				}
			}

			for (size_t index = 0; index < ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS; index++) {
				size_t offset = ECS_TOOLS_UI_PASSES * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS;
				restore_info->system_sprite_textures[index].AddStream(sprites[offset + index]);
			}
			restore_info->system_cluster_sprite_count[0].AddStream(cluster_sprite_counts[ECS_TOOLS_UI_PASSES]);

			restore_info->hoverable_handler->CopyData(
				&hoverables,
				restore_info->handler_buffer_allocator,
				restore_info->handler_data_allocator,
				restore_info->handler_system_data_allocator
			);
			ForEachMouseButton([&](ECS_MOUSE_BUTTON button_type) {
				restore_info->clickable_handlers[button_type].CopyData(
					clickables + button_type,
					restore_info->handler_buffer_allocator,
					restore_info->handler_data_allocator,
					restore_info->handler_system_data_allocator
				);
				});
			restore_info->general_handler->CopyData(
				&generals,
				restore_info->handler_buffer_allocator,
				restore_info->handler_data_allocator,
				restore_info->handler_system_data_allocator
			);

			bool should_redraw = false;
			// Run the runnables now
			for (size_t index = 0; index < runnables.size; index++) {
				if (runnables[index].draw_phase == ECS_UI_DRAW_SYSTEM) {
					restore_info->runnable_data.buffers = restore_info->system_buffers;
					restore_info->runnable_data.counts = restore_info->system_counts;
				}
				else if (runnables[index].draw_phase == ECS_UI_DRAW_LATE) {
					restore_info->runnable_data.buffers = restore_info->buffers + ECS_TOOLS_UI_MATERIALS;
					restore_info->runnable_data.counts = restore_info->counts + ECS_TOOLS_UI_MATERIALS;
				}
				else {
					restore_info->runnable_data.buffers = restore_info->buffers;
					restore_info->runnable_data.counts = restore_info->counts;
				}
				should_redraw |= runnables[index].function(runnables[index].data, &restore_info->runnable_data);
			}

			return should_redraw;
		}

		bool UIDockspaceBorderDrawOutputSnapshot::ContainsTexture(UISpriteTexture texture) const
		{
			for (size_t index = 0; index < ECS_COUNTOF(sprites); index++) {
				for (size_t subindex = 0; subindex < sprites[index].size; subindex++) {
					if (texture.Interface() == sprites[index][subindex].Interface()) {
						return true;
					}
				}
			}
			return false;
		}

		bool UIDockspaceBorderDrawOutputSnapshot::ReplaceTexture(UISpriteTexture old_texture, UISpriteTexture new_texture)
		{
			bool was_replaced = false;
			for (size_t index = 0; index < ECS_COUNTOF(sprites); index++) {
				for (size_t subindex = 0; subindex < sprites[index].size; subindex++) {
					if (sprites[index][subindex].Interface() == old_texture.Interface()) {
						sprites[index][subindex] = new_texture;
						was_replaced = true;
					}
				}
			}
			return was_replaced;
		}

		void UIDockspaceBorderDrawOutputSnapshot::ConstructFrom(const UIDockspaceBorderDrawOutputSnapshotCreateInfo* info)
		{
			AllocatorPolymorphic allocator = info->allocator;

			// Start with the buffers
			size_t vertex_byte_sizes[ECS_TOOLS_UI_MATERIALS] = {
				sizeof(UIVertexColor),
				sizeof(UISpriteVertex),
				sizeof(UISpriteVertex),
				sizeof(UISpriteVertex),
				sizeof(UIVertexColor)
			};

			for (size_t pass_index = 0; pass_index < ECS_TOOLS_UI_PASSES; pass_index++) {
				size_t offset = pass_index * ECS_TOOLS_UI_MATERIALS;
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
					size_t allocation_size = vertex_byte_sizes[index] * info->counts[offset + index];
					if (allocation_size > 0) {
						buffers[offset + index] = AllocateEx(allocator, allocation_size);
						memcpy(buffers[offset + index], info->buffers[offset + index], allocation_size);
					}
					else {
						buffers[offset + index] = nullptr;
					}
					counts[offset + index] = info->counts[offset + index];
				}
			}

			for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
				size_t offset = ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES;
				size_t count = info->system_counts[index] - info->previous_system_counts[index];
				size_t allocation_size = vertex_byte_sizes[index] * count;
				if (allocation_size > 0) {
					buffers[offset + index] = AllocateEx(allocator, allocation_size);
					size_t buffer_offset = vertex_byte_sizes[index] * info->previous_system_counts[index];
					memcpy(buffers[offset + index], OffsetPointer(info->system_buffers[index], buffer_offset), allocation_size);
				}
				else {
					buffers[offset + index] = nullptr;
				}
				counts[offset + index] = count;
			}

			auto copy_sprite_textures = [&](size_t sprite_count, size_t sprite_offset, size_t pass_index, size_t texture_index_offset) {
				Stream<UISpriteTexture>* current_sprites = &sprites[pass_index * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS + texture_index_offset];
				if (sprite_count > 0) {
					size_t allocation_size = sizeof(UISpriteTexture) * sprite_count;
					current_sprites->buffer = (UISpriteTexture*)AllocateEx(allocator, allocation_size);
					Stream<UISpriteTexture> textures = {};
					if (pass_index == ECS_TOOLS_UI_PASSES) {
						textures = info->system_sprite_textures[texture_index_offset];
					}
					else {
						textures = info->border_sprite_textures[pass_index * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS + texture_index_offset];
					}

					memcpy(current_sprites->buffer, textures.buffer + sprite_offset, allocation_size);
					current_sprites->size = sprite_count;
				}
				else {
					*current_sprites = {};
				}
			};

			for (size_t pass_index = 0; pass_index < ECS_TOOLS_UI_PASSES; pass_index++) {
				size_t sprite_count = info->counts[pass_index * ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SPRITE] / 6;
				copy_sprite_textures(sprite_count, 0, pass_index, ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFER_INDEX);

				cluster_sprite_counts[pass_index] = {};
				// For the cluster sprites, also copy the cluster count
				size_t cluster_count = info->border_cluster_sprite_count[pass_index].size;
				if (cluster_count > 0) {
					cluster_sprite_counts[pass_index].InitializeAndCopy(allocator, info->border_cluster_sprite_count[pass_index]);
				}

				copy_sprite_textures(cluster_count, 0, pass_index, ECS_TOOLS_UI_SPRITE_CLUSTER_TEXTURE_BUFFER_INDEX);
			}

			// For the system draw, just copy the sprite textures
			size_t system_sprite_count = (info->system_counts[ECS_TOOLS_UI_SPRITE] - info->previous_system_counts[ECS_TOOLS_UI_SPRITE]) / 6;
			copy_sprite_textures(
				system_sprite_count, 
				info->previous_system_counts[ECS_TOOLS_UI_SPRITE] / 6, 
				ECS_TOOLS_UI_PASSES, 
				ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFER_INDEX
			);

			// Copy the sprite cluster information
			cluster_sprite_counts[ECS_TOOLS_UI_PASSES] = {};
			sprites[ECS_TOOLS_UI_PASSES * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS + ECS_TOOLS_UI_SPRITE_CLUSTER_TEXTURE_BUFFER_INDEX] = {};
			if (info->system_counts[ECS_TOOLS_UI_SPRITE_CLUSTER] - info->previous_system_counts[ECS_TOOLS_UI_SPRITE_CLUSTER] > 0) {
				Stream<unsigned int> added_clusters_slice = info->system_cluster_sprite_count[0].SliceAt(info->previous_system_cluster_count);
				cluster_sprite_counts[ECS_TOOLS_UI_PASSES].InitializeAndCopy(allocator, added_clusters_slice);
				copy_sprite_textures(added_clusters_slice.size, info->previous_system_cluster_count, 0, ECS_TOOLS_UI_SPRITE_CLUSTER_TEXTURE_BUFFER_INDEX);
			}

			// Copy the handlers now
			hoverables = info->hoverable_handler->Copy(allocator, &hoverables_action_allocated_data);
			ForEachMouseButton([&](ECS_MOUSE_BUTTON button_type) {
				clickables[button_type] = info->clickable_handlers[button_type].Copy(allocator, &clickables_action_allocated_data[button_type]);
			});
			generals = info->general_handler->Copy(allocator, &generals_action_allocated_data);

			// At last the runnables
			runnables.InitializeEx(allocator, info->runnables.size);
			for (size_t index = 0; index < runnables.size; index++) {
				runnables[index] = info->runnables[index];
				runnables[index].data = CopyNonZeroEx(info->runnable_allocator, runnables[index].data, runnables[index].data_size);
			}
		}

		void UIDockspaceBorderDrawOutputSnapshot::Deallocate(AllocatorPolymorphic allocator, AllocatorPolymorphic runnable_allocator, bool deallocate_runnable_data)
		{
			for (size_t index = 0; index < ECS_COUNTOF(counts); index++) {
				if (counts[index] != 0 && buffers[index] != nullptr) {
					DeallocateEx(allocator, buffers[index]);
				}
			}

			for (size_t index = 0; index < ECS_COUNTOF(sprites); index++) {
				if (sprites[index].size > 0 && sprites[index].buffer != nullptr) {
					DeallocateEx(allocator, sprites[index].buffer);
				}
			}

			for (size_t index = 0; index < ECS_COUNTOF(cluster_sprite_counts); index++) {
				if (cluster_sprite_counts[index].size > 0 && cluster_sprite_counts[index].buffer != nullptr) {
					DeallocateEx(allocator, cluster_sprite_counts[index].buffer);
				}
			}

			hoverables.Deallocate(allocator);
			if (hoverables_action_allocated_data != nullptr) {
				DeallocateEx(allocator, hoverables_action_allocated_data);
			}
			ForEachMouseButton([&](ECS_MOUSE_BUTTON button_type) {
				clickables[button_type].Deallocate(allocator);
				if (clickables_action_allocated_data[button_type] != nullptr) {
					DeallocateEx(allocator, clickables_action_allocated_data[button_type]);
				}
				});
			generals.Deallocate(allocator);
			if (generals_action_allocated_data != nullptr) {
				DeallocateEx(allocator, generals_action_allocated_data);
			}

			if (deallocate_runnable_data) {
				for (size_t index = 0; index < runnables.size; index++) {
					if (runnables[index].data_size > 0) {
						DeallocateEx(runnable_allocator, runnables[index].data);
					}
				}
			}
			runnables.DeallocateEx(allocator);
		
			InitializeEmpty();
		}

		template<typename UpdateXFunctor, typename UpdateYFunctor>
		void ChangeStructureDimensionRatio(UpdateXFunctor&& update_x, UpdateYFunctor&& update_y, float2 current_ratio, float2 new_ratio) {
			float2 factor_inverse = current_ratio / new_ratio;
			if (factor_inverse.x != 1.0f) {
				update_x(factor_inverse.x);
			}
			if (factor_inverse.y != 1.0f) {
				update_y(factor_inverse.y);
			}
		}

		void UIElementDescriptor::ChangeDimensionRatio(float2 current_ratio, float2 new_ratio)
		{
			auto update_x = [this](float value) {
				label_padd.x *= value;
				slider_shrink.x *= value;
				slider_length.x *= value;
				color_input_padd *= value;
				combo_box_padding *= value;
				graph_padding.x *= value;
				graph_x_axis_space *= value;
				graph_axis_value_line_size.x *= value;
				graph_axis_bump.x *= value;
				histogram_padding.x *= value;
				histogram_bar_spacing *= value;
			};

			auto update_y = [this](float value) {
				label_padd.y *= value;
				slider_shrink.y *= value;
				slider_length.y *= value;
				graph_padding.y *= value;
				graph_axis_value_line_size.y *= value;
				graph_axis_bump.y *= value;
				graph_sample_circle_size *= value;
				histogram_padding.y *= value;
				histogram_bar_min_scale *= value;
				histogram_bar_spacing *= value;
				menu_button_padding *= value;
				label_list_circle_size *= value;
			};

			ChangeStructureDimensionRatio(update_x, update_y, current_ratio, new_ratio);
		}

		void UIMiscellaneousDescriptor::ChangeDimensionRatio(float2 current_ratio, float2 new_ratio)
		{
			auto update_x = [this](float value) {
				render_slider_vertical_size *= value;
				color_input_window_size_x *= value;
				graph_hover_offset.x *= value;
				histogram_hover_offset.x *= value;
				tool_tip_padding.x *= value;
				menu_x_padd *= value;
			};

			auto update_y = [this](float value) {
				render_slider_horizontal_size *= value;
				graph_hover_offset.y *= value;
				histogram_hover_offset.y *= value;
				tool_tip_padding.y *= value;
			};
			
			ChangeStructureDimensionRatio(update_x, update_y, current_ratio, new_ratio);
		}

		void UILayoutDescriptor::ChangeDimensionRatio(float2 current_ratio, float2 new_ratio)
		{
			auto update_x = [this](float value) {
				default_element_x *= value;
				element_indentation *= value;
				next_row_padding *= value;
				node_indentation *= value;
			};

			auto update_y = [this](float value) {
				default_element_y *= value;
				next_row_y_offset *= value;
			};

			ChangeStructureDimensionRatio(update_x, update_y, current_ratio, new_ratio);
		}

		void UIDockspaceDescriptor::ChangeDimensionRatio(float2 current_ratio, float2 new_ratio)
		{
			auto update_x = [this](float value) {
				border_margin *= value;
				region_header_spacing *= value;
				region_header_padding *= value;
				region_header_added_scale *= value;
				viewport_padding_x *= value;
				close_x_position_x_left *= value;
				close_x_position_x_right *= value;
				close_x_total_x_padding *= value;
			};

			auto update_y = [this](float value) {
				border_size *= value;
				border_minimum_distance *= value;
				mininum_scale *= value;
				viewport_padding_y *= value;
				collapse_triangle_scale *= value;
				close_x_scale_y *= value;
			};

			ChangeStructureDimensionRatio(update_x, update_y, current_ratio, new_ratio);
		}

		void UIFontDescriptor::ChangeDimensionRatio(float2 current_ratio, float2 new_ratio)
		{
			auto update_x = [this](float value) {
				character_spacing *= value;
				size.x *= value;
			};

			auto update_y = [this](float value) {
				size.y *= value;
			};

			ChangeStructureDimensionRatio(update_x, update_y, current_ratio, new_ratio);
		}

		void UISystemDescriptors::ChangeDimensionRatio(float2 current_ratio, float2 new_ratio)
		{
			dockspaces.ChangeDimensionRatio(current_ratio, new_ratio);
			window_layout.ChangeDimensionRatio(current_ratio, new_ratio);
			font.ChangeDimensionRatio(current_ratio, new_ratio);
			misc.ChangeDimensionRatio(current_ratio, new_ratio);
			element_descriptor.ChangeDimensionRatio(current_ratio, new_ratio);
		}

	}

}