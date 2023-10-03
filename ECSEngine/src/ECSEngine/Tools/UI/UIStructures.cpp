#include "ecspch.h"
#include "UIStructures.h"
#include "../../Rendering/ColorUtilities.h"
#include "../../Rendering/Graphics.h"

#include "UIHelpers.h"

namespace ECSEngine {

	namespace Tools {

		UISpriteVertex::UISpriteVertex() : position(0.0f, 0.0f), uvs(0.0f, 0.0f) {}

		UISpriteVertex::UISpriteVertex(float position_x, float position_y, float uv_u, float uv_v, Color _color)
			: position(position_x, position_y), uvs(uv_u, uv_v), color(_color) {}

		UISpriteVertex::UISpriteVertex(float2 _position, float2 _uvs, Color _color)
			: position(_position), uvs(_uvs), color(_color) {}

		void UISpriteVertex::SetTransform(float position_x, float position_y)
		{
			position.x = position_x;
			position.y = position_y;
		}

		void UISpriteVertex::SetTransform(float2 _position)
		{
			position = { _position.x, -_position.y };
		}

		void UISpriteVertex::SetColor(Color _color) {
			color = _color;
		}

		void UISpriteVertex::SetUV(float2 _uvs) {
			uvs = _uvs;
		}

		UIVertexColor::UIVertexColor() : position(0.0f, 0.0f), color((unsigned char)0, 0, 0, 255) {}

		UIVertexColor::UIVertexColor(float position_x, float position_y, Color _color) : position(position_x, position_y), color(_color) {}

		UIVertexColor::UIVertexColor(float2 _position, Color _color) : position(_position), color(_color) {}

		void UIVertexColor::SetTransform(float position_x, float position_y)
		{
			position.x = position_x;
			position.y = position_y;
		}

		void UIVertexColor::SetTransform(float2 _position)
		{
			position = { _position.x, -_position.y };
		}

		void UIVertexColor::SetColor(Color _color) {
			color = _color;
		}

		UIElementTransform::UIElementTransform() : position(0.0f, 0.0f), scale(1.0f, 1.0f) {}

		UIElementTransform::UIElementTransform(float position_x, float position_y, float scale_x, float scale_y)
			: position(position_x, position_y), scale(scale_x, scale_y) {}

		UIElementTransform::UIElementTransform(float2 _position, float2 _scale) : position(_position), scale(_scale) {}

		bool BorderHover::IsBottom() {
			return (value & 0x01) != 0;
		}

		bool BorderHover::IsTop() {
			return (value & 0x02) != 0;
		}

		bool BorderHover::IsLeft() {
			return (value & 0x04) != 0;
		}

		bool BorderHover::IsRight() {
			return (value & 0x08) != 0;
		}

		void BorderHover::SetBottom() {
			value |= 0x01;
		}

		void BorderHover::SetTop() {
			value |= 0x02;
		}

		void BorderHover::SetLeft() {
			value |= 0x04;
		}

		void BorderHover::SetRight() {
			value |= 0x08;
		}

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

		bool UIHandler::Add(float _position_x, float _position_y, float _scale_x, float _scale_y, UIActionHandler handler)
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
			position_x.size++;
			return true;
		}

		bool UIHandler::Add(float2 position, float2 scale, UIActionHandler handler) {
			return Add(position.x, position.y, scale.x, scale.y, handler);
		}

		unsigned int UIHandler::AddResizable(AllocatorPolymorphic allocator, float2 position, float2 scale, UIActionHandler handler) {
			unsigned int current_size = position_x.size;
			if (!Add(position, scale, handler)) {
				Resize(allocator, position_x.capacity * ECS_RESIZABLE_STREAM_FACTOR + 2);
				Add(position, scale, handler);
			}
			return current_size;
		}

		void UIHandler::Execute(unsigned int action_index, ActionData* action_data) const {
			action_data->data = action[action_index].data;
			action_data->position = { position_x[action_index], position_y[action_index] };
			action_data->scale = { scale_x[action_index], scale_y[action_index] };
			action[action_index].action(action_data);
		}

		UIActionHandler* UIHandler::GetLastHandler() const
		{
			unsigned int index = position_x.size - 1;
			return action + index;
		}

		float2 UIHandler::GetPositionFromIndex(unsigned int index) const
		{
			return { position_x[index], position_y[index] };
		}

		float2 UIHandler::GetScaleFromIndex(unsigned int index) const
		{
			return { scale_x[index], scale_y[index] };
		}

		UIActionHandler UIHandler::GetActionFromIndex(unsigned int index) const
		{
			return action[index];
		}

		unsigned int UIHandler::GetLastHandlerIndex() const
		{
			return position_x.size - 1;
		}

		void UIHandler::Insert(AllocatorPolymorphic allocator, float2 position, float2 scale, UIActionHandler handler, unsigned int insert_index)
		{
			unsigned int current_size = position_x.size;
			if (current_size == position_x.capacity) {
				Resize(allocator, position_x.capacity * ECS_RESIZABLE_STREAM_FACTOR + 2);
			}

			for (unsigned int index = current_size; index > insert_index; index--) {
				unsigned int bellow_index = index - 1;
				position_x[index] = position_x[bellow_index];
				position_y[index] = position_y[bellow_index];
				scale_x[index] = scale_x[bellow_index];
				scale_y[index] = scale_y[bellow_index];
				action[index] = action[bellow_index];
			}
			position_x[insert_index] = position.x;
			position_y[insert_index] = position.y;
			scale_x[insert_index] = scale.x;
			scale_y[insert_index] = scale.y;
			action[insert_index] = handler;
			position_x.size++;
		}

		void UIHandler::Resize(AllocatorPolymorphic allocator, size_t new_count)
		{
			void* allocation = Allocate(allocator, MemoryOf(new_count));
			UIHandler old_handler = *this;
			SetBuffer(allocation, new_count);
			memcpy(position_x.buffer, old_handler.position_x.buffer, sizeof(float) * position_x.size);
			memcpy(position_y, old_handler.position_y, sizeof(float) * position_x.size);
			memcpy(scale_x, old_handler.scale_x, sizeof(float) * position_x.size);
			memcpy(scale_y, old_handler.scale_y, sizeof(float) * position_x.size);
			memcpy(action, old_handler.action, sizeof(UIActionHandler) * position_x.size);
			position_x.capacity = new_count;

			DeallocateIfBelongs(allocator, old_handler.position_x.buffer);
		}

		unsigned int UIHandler::ReserveOne(AllocatorPolymorphic allocator)
		{
			if (position_x.size == position_x.capacity) {
				Resize(allocator, position_x.size * 1.5f);
			}
			unsigned int index = position_x.size;
			position_x.size++;
			return index;
		}

		void UIHandler::Reset() {
			position_x.size = 0;
		}

		void UIHandler::SetBuffer(void* buffer, size_t count) {
			uintptr_t ptr = (uintptr_t)buffer;
			position_x.buffer = (float*)buffer;
			ptr += sizeof(float) * count;
			position_y = (float*)ptr;
			ptr += sizeof(float) * count;
			scale_x = (float*)ptr;
			ptr += sizeof(float) * count;
			scale_y = (float*)ptr;
			ptr += sizeof(float) * count;

			ptr = AlignPointer(ptr, alignof(UIActionHandler));
			action = (UIActionHandler*)ptr;
		}

		size_t UIHandler::MemoryOf(size_t count) {
			return (sizeof(float) * 4 + sizeof(UIActionHandler)) * count + 8;
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
			clean_up_call_hoverable = false;
			always_hoverable = false;
		}

		void UIFocusedWindowData::ResetClickableHandler(ECS_MOUSE_BUTTON button_type)
		{
			clickable_handler[button_type].action = nullptr;
			clickable_handler[button_type].data = nullptr;
			clickable_handler[button_type].data_size = 0;
			clickable_handler[button_type].phase = ECS_UI_DRAW_NORMAL;
		}

		void UIFocusedWindowData::ResetGeneralHandler()
		{
			general_handler.action = nullptr;
			general_handler.data = nullptr;
			general_handler.data_size = 0;
			general_handler.phase = ECS_UI_DRAW_NORMAL;
			clean_up_call_general = false;
		}

		void UIFocusedWindowData::ChangeClickable(float2 position, float2 scale, const UIActionHandler* handler, ECS_MOUSE_BUTTON button_type)
		{
			ChangeClickable(position, scale, handler->action, handler->data, handler->data_size, handler->phase, button_type);
		}

		void UIFocusedWindowData::ChangeGeneralHandler(float2 position, float2 scale, const UIActionHandler* handler)
		{
			general_transform.position = position;
			general_transform.scale = scale;
			general_handler = *handler;
			clean_up_call_general = false;
		}

		void UIFocusedWindowData::ChangeHoverableHandler(float2 position, float2 scale, const UIActionHandler* handler)
		{
			hoverable_transform.position = position;
			hoverable_transform.scale = scale;
			hoverable_handler = *handler;
			clean_up_call_hoverable = false;
		}

		void UIFocusedWindowData::ChangeClickable(
			float2 position,
			float2 scale,
			Action action,
			void* data,
			size_t data_size,
			ECS_UI_DRAW_PHASE phase,
			ECS_MOUSE_BUTTON button_type
		)
		{
			ChangeClickable({ position, scale }, action, data, data_size, phase, button_type);
		}

		void UIFocusedWindowData::ChangeHoverableHandler(UIElementTransform transform, Action action, void* data, size_t data_size, ECS_UI_DRAW_PHASE phase)
		{
			hoverable_handler.action = action;
			hoverable_handler.data = data;
			hoverable_handler.phase = phase;
			hoverable_handler.data_size = data_size;
			hoverable_transform = transform;
		}

		void UIFocusedWindowData::ChangeHoverableHandler(
			float2 position, 
			float2 scale, 
			Action action, 
			void* data, 
			size_t data_size,
			ECS_UI_DRAW_PHASE phase
		) {
			ChangeHoverableHandler({ position, scale }, action, data, data_size, phase);
		}

		void UIFocusedWindowData::ChangeHoverableHandler(
			const UIHandler* handler,
			unsigned int index,
			void* data
		) {
			ChangeHoverableHandler(
				{ handler->position_x[index], handler->position_y[index] }, 
				{ handler->scale_x[index], handler->scale_y[index] },
				handler->action[index].action,
				data,
				handler->action[index].data_size,
				handler->action[index].phase
			);
		}

		void UIFocusedWindowData::ChangeClickable(
			UIElementTransform transform,
			Action action,
			void* data,
			size_t data_size,
			ECS_UI_DRAW_PHASE phase,
			ECS_MOUSE_BUTTON button_type
		)
		{
			clickable_handler[button_type].action = action;
			clickable_handler[button_type].data = data;
			clickable_handler[button_type].phase = phase;
			clickable_handler[button_type].data_size = data_size;
			mouse_click_transform[button_type] = transform;
		}

		void UIFocusedWindowData::ChangeClickable(
			const UIHandler* handler,
			unsigned int index,
			void* data,
			ECS_MOUSE_BUTTON button_type
		)
		{
			ChangeClickable(
				{ handler->position_x[index], handler->position_y[index] },
				{ handler->scale_x[index], handler->scale_y[index] },
				handler->action[index].action,
				data,
				handler->action[index].data_size,
				handler->action[index].phase,
				button_type
			);
		}

		void UIFocusedWindowData::ChangeGeneralHandler(
			float2 position,
			float2 scale,
			Action action,
			void* data,
			size_t data_size,
			ECS_UI_DRAW_PHASE phase
		)
		{
			general_handler.action = action;
			general_handler.data = data;
			general_handler.phase = phase;
			general_handler.data_size = data_size;
			general_transform = {
				position,
				scale
			};
		}

		void UIFocusedWindowData::ChangeGeneralHandler(
			UIElementTransform transform,
			Action action,
			void* data,
			size_t data_size,
			ECS_UI_DRAW_PHASE phase
		)
		{
			general_handler.action = action;
			general_handler.data = data;
			general_handler.phase = phase;
			general_handler.data_size = data_size;
			general_transform = transform;
		}

		void UIFocusedWindowData::ChangeGeneralHandler(
			const UIHandler* handler,
			unsigned int index,
			void* data
		)
		{
			ChangeGeneralHandler(
				{ handler->position_x[index], handler->position_y[index] },
				{ handler->scale_x[index], handler->scale_y[index] },
				handler->action[index].action,
				data, 
				handler->action[index].data_size,
				handler->action[index].phase
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
			draw_resources.sprite_cluster_subtreams.Reset();
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
			max_bounds.x = std::max(max_bounds.x, position.x);
			max_bounds.y = std::max(max_bounds.y, position.y);
			current_scale.x = 0.0f;
			current_scale.y = std::max(current_scale.y, scale.y);
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

		size_t UIWindow::Serialize(void* buffer) const
		{
			/*	
			UIWindowDrawerDescriptor descriptors;
			unsigned short name_length;
			*/

			uintptr_t ptr = (uintptr_t)buffer;
			memcpy((void*)ptr, descriptors, sizeof(bool) * (unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT);
		
			// configured descriptors
			ptr += sizeof(bool) * (unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT;

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
			for (size_t index = 0; index < (unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT; index++) {
				if (descriptors->configured[index]) {
					memcpy((void*)ptr, descriptor_ptrs[index], descriptor_sizes[index]);
					ptr += descriptor_sizes[index];
				}
			}

			// name
			size_t name_length = name_vertex_buffer.size / 6;
			unsigned short* ptr_name_length = (unsigned short*)ptr;
			*ptr_name_length = (unsigned short)name_length;
			ptr += sizeof(unsigned short);

			memcpy((void*)ptr, name.buffer, name_length);
			ptr += name_length;
			return ptr - (uintptr_t)buffer;
		}

		size_t UIWindow::SerializeSize() const
		{
			size_t size = 0;

			// configured descriptors
			size += sizeof(bool) * (unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT;

			size_t descriptor_sizes[] = {
				sizeof(UIColorThemeDescriptor),
				sizeof(UILayoutDescriptor),
				sizeof(UIFontDescriptor),
				sizeof(UIElementDescriptor)
			};

			// descriptors
			for (size_t index = 0; index < (unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT; index++) {
				if (descriptors->configured[index]) {
					size += descriptor_sizes[index];
				}
			}

			// name
			size_t name_length = name_vertex_buffer.size / 6;
			size += sizeof(unsigned short);
			size += name_length;

			return size;
		}

		size_t UIWindow::LoadFromFile(const void* buffer, Stream<char>& name_stack)
		{
			uintptr_t ptr = (uintptr_t)buffer;

			// descriptor configurations
			memcpy(descriptors->configured, (const void*)ptr, sizeof(bool) * (unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT);
			
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

			ptr += sizeof(bool) * (unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT;

			// configured descriptors
			for (size_t index = 0; index < (unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT; index++) {
				if (descriptors->configured[index]) {
					memcpy(descriptor_ptrs[index], (const void*)ptr, descriptor_sizes[index]);
					ptr += descriptor_sizes[index];
				}
			}

			// name
			const unsigned short* name_length = (const unsigned short*)ptr;
			name_stack.size = *name_length;
			ptr += sizeof(unsigned short);
			memcpy(name_stack.buffer, (const void*)ptr, sizeof(char) * name_stack.size);
			name_stack[name_stack.size] = '\0';
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

		void UIReservedHandler::Write(float2 position, float2 scale, UIActionHandler action_handler)
		{
			action_handler.data = CopyNonZero(allocator, action_handler.data, action_handler.data_size);
			handler->action[index] = action_handler;
			handler->position_x[index] = position.x;
			handler->position_y[index] = position.y;
			handler->scale_x[index] = scale.x;
			handler->scale_y[index] = scale.y;
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

	}

}