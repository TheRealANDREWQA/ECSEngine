#include "ecspch.h"
#include "UISystem.h"
#include "UIResourcePaths.h"
#include "../../Rendering/GraphicsHelpers.h"
#include "../../Rendering/Graphics.h"
#include "../../Rendering/Compression/TextureCompression.h"
#include "../../Rendering/ColorMacros.h"
#include "../../Internal/Multithreading/TaskManager.h"
#include "../../Internal/Resources/ResourceManager.h"
#include "../../Internal/InternalStructures.h"
#include "../../Utilities/File.h"
#include "../../Allocators/AllocatorPolymorphic.h"

namespace ECSEngine {

	ECS_MICROSOFT_WRL;

	//using namespace HID;

	namespace Tools {

		struct ProcessTextureData {
			const wchar_t* filename;
			UISpriteTexture* texture;
			UISystem* system;
		};

		// -----------------------------------------------------------------------------------------------------------------------------------

		void ProcessTexture(unsigned int thread_index, World* world, void* data);

		// -----------------------------------------------------------------------------------------------------------------------------------

		template<typename RollbackFunction>
		bool ResizeDockspaceInternal(
			UISystem* system,
			unsigned int dockspace_index,
			float delta_scale,
			ECS_UI_BORDER_TYPE border,
			CapacityStream<UIDockspace>& dockspaces,
			DockspaceType dockspace_type,
			RollbackFunction&& rollback
		) {
			bool should_resize_parent = true;
			unsigned int dock_indices[100];
			int64_t count = 0;
			if (dockspace_type == DockspaceType::FloatingHorizontal || dockspace_type == DockspaceType::Horizontal) {
				for (size_t index = 0; should_resize_parent && index < dockspaces[dockspace_index].borders.size - 1; index++) {
					if (dockspaces[dockspace_index].borders[index].is_dock) {
						should_resize_parent = system->ResizeDockspace(
							dockspaces[dockspace_index].borders[index].window_indices[0],
							delta_scale,
							border,
							DockspaceType::Vertical
						);
						dock_indices[count++] = dockspaces[dockspace_index].borders[index].window_indices[0];
					}
				}
				if (!should_resize_parent) {
					for (int64_t index = 0; index < count - 1; index++) {
						system->ResizeDockspaceUnguarded(
							dock_indices[index],
							-delta_scale,
							border,
							DockspaceType::Vertical
						);
					}
					rollback();
					return false;
				}
			}
			else {
				for (size_t index = 0; should_resize_parent && index < dockspaces[dockspace_index].borders.size - 1; index++) {
					if (dockspaces[dockspace_index].borders[index].is_dock) {
						should_resize_parent = system->ResizeDockspace(
							dockspaces[dockspace_index].borders[index].window_indices[0],
							delta_scale,
							border,
							DockspaceType::Horizontal
						);
						dock_indices[count++] = dockspaces[dockspace_index].borders[index].window_indices[0];
					}
				}
				if (!should_resize_parent) {
					for (int64_t index = 0; index < count - 1; index++) {
						system->ResizeDockspaceUnguarded(
							dock_indices[index],
							-delta_scale,
							border,
							DockspaceType::Horizontal
						);
					}
					rollback();
					return false;
				}
			}
			return true;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIToolsAllocator DefaultUISystemAllocator(GlobalMemoryManager* global_manager)
		{
			return UIToolsAllocator(global_manager, 15'000'000, 8, 512, 5'000'000, 4, 512);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UISystem::UISystem(
			Application* application,
			UIToolsAllocator* memory,
			HID::Keyboard* keyboard,
			HID::Mouse* mouse,
			Graphics* graphics,
			ResourceManager* resource,
			TaskManager* task_manager,
			uint2 window_os_size,
			GlobalMemoryManager* initial_allocator
		) : m_graphics(graphics), m_mouse(mouse), m_mouse_tracker(mouse->GetTracker()), m_keyboard(keyboard), m_keyboard_tracker(keyboard->GetTracker()),
			m_memory(memory), m_resource_manager(resource), m_task_manager(task_manager), m_application(application), m_frame_index(0),
			m_texture_evict_count(0), m_texture_evict_target(60), m_window_os_size(window_os_size)
		{
			// initializing default m_descriptors and materials
			InitializeDefaultDescriptors();
			InitializeMaterials();

			// copying to the startup descriptors
			memcpy(&m_startup_descriptors, &m_descriptors, sizeof(UISystemDescriptors));

			size_t total_memory = 0;
			total_memory += sizeof(UIWindow) * m_descriptors.misc.window_count;
			total_memory += sizeof(UIDockspace) * m_descriptors.dockspaces.count * 4;
			total_memory += sizeof(unsigned short) * m_descriptors.misc.window_count;

			// 4x for dockspace layers, 2x for fixed dockspaces, 2x for background dockspaces
			total_memory += sizeof(UIDockspaceLayer) * m_descriptors.dockspaces.count * 8;
			// 2x for pop up windows
			total_memory += sizeof(unsigned int) * m_descriptors.dockspaces.count * 2;
			total_memory += ThreadSafeQueue<ThreadTask>::MemoryOf(m_descriptors.misc.window_count);

			// symbols in the font atlas; must store top_left and bottom right uvs
			total_memory += sizeof(float2) * m_descriptors.font.symbol_count * 2;

			total_memory += sizeof(UIActionHandler) * ECS_TOOLS_UI_SYSTEM_HANDLER_FRAME_COUNT;

#ifdef ECS_TOOLS_UI_MULTI_THREADED
			total_memory += sizeof(UIRenderThreadResources) * m_task_manager->GetThreadCount();
			total_memory += m_descriptors.misc.thread_temp_memory * m_task_manager->GetThreadCount();
#else
			// 2 times the thread memory for the system allocator and for the normal/late allocator
			total_memory += sizeof(UIRenderThreadResources) + m_descriptors.misc.thread_temp_memory * 2;
#endif

			// material memory
			total_memory += sizeof(VertexShader) * m_descriptors.materials.count;
			total_memory += sizeof(PixelShader) * m_descriptors.materials.count;
			total_memory += sizeof(InputLayout) * m_descriptors.materials.count;
			total_memory += sizeof(SamplerState) * m_descriptors.materials.sampler_count;
			total_memory += sizeof(VertexBuffer) * m_descriptors.materials.count;
			total_memory += sizeof(UIDynamicStream<UISpriteTexture>) * ECS_TOOLS_UI_PASSES;

			// for alignment
			total_memory += 32;

			void* allocation;
			if (initial_allocator == nullptr) {
				allocation = m_memory->Allocate(total_memory, alignof(UIWindow));
			}
			else {
				allocation = initial_allocator->Allocate(total_memory, alignof(UIWindow));
			}
			// for some reason the memory must be 0'ed out before using it
			memset(allocation, 0, total_memory);

			m_thread_tasks = ThreadSafeQueue<ThreadTask>(allocation, m_descriptors.misc.window_count);

			uintptr_t buffer = (uintptr_t)allocation;
			buffer += ThreadSafeQueue<ThreadTask>::MemoryOf(m_descriptors.misc.window_count);

			buffer = function::AlignPointer(buffer, alignof(UIWindow));
			m_windows.InitializeFromBuffer(buffer, 0, m_descriptors.misc.window_count);

			buffer = function::AlignPointer(buffer, alignof(UIDockspaceLayer));
			m_dockspace_layers.InitializeFromBuffer(buffer, 0, m_descriptors.dockspaces.count * 4);
			m_fixed_dockspaces.InitializeFromBuffer(buffer, 0, m_descriptors.dockspaces.count * 2);
			m_pop_up_windows.InitializeFromBuffer(buffer, 0, m_descriptors.dockspaces.count * 2);
			m_background_dockspaces.InitializeFromBuffer(buffer, 0, m_descriptors.dockspaces.count * 2);

			buffer = function::AlignPointer(buffer, alignof(UIDockspace));
			m_horizontal_dockspaces.InitializeFromBuffer(buffer, 0, m_descriptors.dockspaces.count);	
			m_vertical_dockspaces.InitializeFromBuffer(buffer, 0, m_descriptors.dockspaces.count);	
			m_floating_horizontal_dockspaces.InitializeFromBuffer(buffer, 0, m_descriptors.dockspaces.count);
			m_floating_vertical_dockspaces.InitializeFromBuffer(buffer, 0, m_descriptors.dockspaces.count);

			buffer = function::AlignPointer(buffer, alignof(float2));
			m_font_character_uvs = (float2*)buffer;
			buffer += sizeof(float2) * m_descriptors.font.symbol_count * 2;

			buffer = function::AlignPointer(buffer, alignof(UIRenderThreadResources));
			m_resources.thread_resources.buffer = (UIRenderThreadResources*)buffer;
#ifdef ECS_TOOLS_UI_MULTI_THREADED
			m_resources.thread_resources.size = m_task_manager->GetThreadCount();
			buffer += sizeof(UIRenderThreadResources) * m_task_manager->GetThreadCount();
#else
			m_resources.thread_resources.size = 1;
			buffer += sizeof(UIRenderThreadResources);
#endif

			for (size_t index = 0; index < m_resources.thread_resources.size; index++) {
				m_resources.thread_resources[index].deferred_context = m_graphics->CreateDeferredContext();
				m_resources.thread_resources[index].temp_allocator = LinearAllocator((void*)buffer, m_descriptors.misc.thread_temp_memory);
				buffer += m_descriptors.misc.thread_temp_memory;

				m_resources.thread_resources[index].system_temp_allocator = LinearAllocator((void*)buffer, m_descriptors.misc.thread_temp_memory);
				buffer += m_descriptors.misc.thread_temp_memory;
			}

			buffer = function::AlignPointer(buffer, alignof(VertexShader));
			m_resources.vertex_shaders.InitializeFromBuffer(buffer, 0, m_descriptors.materials.count);

			buffer = function::AlignPointer(buffer, alignof(PixelShader));
			m_resources.pixel_shaders.InitializeFromBuffer(buffer, 0, m_descriptors.materials.count);

			buffer = function::AlignPointer(buffer, alignof(InputLayout));
			m_resources.input_layouts.InitializeFromBuffer(buffer, 0, m_descriptors.materials.count);

			buffer = function::AlignPointer(buffer, alignof(SamplerState));
			m_resources.texture_samplers.InitializeFromBuffer(buffer, 0, m_descriptors.materials.sampler_count);


			buffer = function::AlignPointer(buffer, alignof(VertexBuffer));
			m_resources.system_draw.buffers = CapacityStream<VertexBuffer>((void*)buffer, m_descriptors.materials.count, m_descriptors.materials.count);
			for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
				if (index == ECS_TOOLS_UI_SOLID_COLOR || index == ECS_TOOLS_UI_LINE) {
					m_resources.system_draw.buffers[index] = m_graphics->CreateVertexBuffer(sizeof(UIVertexColor), m_descriptors.misc.system_vertex_buffers[index]);
				}
				else if (index == ECS_TOOLS_UI_TEXT_SPRITE || index == ECS_TOOLS_UI_SPRITE || index == ECS_TOOLS_UI_SPRITE_CLUSTER) {
					m_resources.system_draw.buffers[index] = m_graphics->CreateVertexBuffer(sizeof(UISpriteVertex), m_descriptors.misc.system_vertex_buffers[index]);
				}
			}

			buffer += sizeof(VertexBuffer) * m_resources.system_draw.buffers.size;
			m_resources.system_draw.region_viewport_info = m_graphics->CreateConstantBuffer(sizeof(float) * ECS_TOOLS_UI_CONSTANT_BUFFER_FLOAT_SIZE);

			m_resources.system_draw.sprite_textures = CapacityStream<UIDynamicStream<UISpriteTexture>>((void*)buffer, 1, ECS_TOOLS_UI_PASSES);
			m_resources.system_draw.sprite_textures[0] = UIDynamicStream<UISpriteTexture>(GetAllocatorPolymorphic(m_memory), 0);

			buffer += sizeof(UIDynamicStream<UISpriteTexture>) * ECS_TOOLS_UI_PASSES;

			m_frame_handlers.InitializeFromBuffer(buffer, 0, ECS_TOOLS_UI_SYSTEM_HANDLER_FRAME_COUNT);
			
			ReadFontDescriptionFile(L"Resources/FontDescription_v2.txt");

			// loading font atlas; font atlas does not keep track of texture indices
			ResourceManagerTextureDesc texture_descriptor;
			texture_descriptor.context = m_graphics->GetContext();
			ResourceView font_texture = m_resource_manager->LoadTexture(L"Resources/FontSharpened.tiff", &texture_descriptor);
			m_resources.font_texture = font_texture;

			// loading and registering vertex and pixel shaders
			RegisterPixelShaders();
			RegisterVertexShadersAndLayouts();
			RegisterSamplers();

			// event
			m_event = SkipEvent;
			m_event_data = nullptr;

			// focused window data
			m_focused_window_data.hoverable_handler.action = nullptr;
			m_focused_window_data.clickable_handler.action = nullptr;
			m_focused_window_data.general_handler.action = nullptr;
			m_focused_window_data.clickable_handler.phase = ECS_UI_DRAW_NORMAL;
			m_focused_window_data.general_handler.phase = ECS_UI_DRAW_NORMAL;
			m_focused_window_data.clean_up_call_general = false;
			m_focused_window_data.clean_up_call_hoverable = false;
			m_focused_window_data.additional_general_data = nullptr;
			m_focused_window_data.additional_hoverable_data = nullptr;
			m_focused_window_data.locked_window = false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddActionHandler(
			LinearAllocator* allocator,
			UIHandler* handler,
			float2 position,
			float2 scale,
			UIActionHandler action_handler
		) {
			if (action_handler.data_size > 0) {
				void* allocation = AllocateHandlerMemory(allocator, action_handler.data_size, 8, action_handler.data);
				memcpy(allocation, action_handler.data, action_handler.data_size);
				action_handler.data = allocation;
			}
			bool should_resize = !handler->Add(position, scale, action_handler);
			if (should_resize) {
				size_t count = handler->position_x.size * 1.5f;
				size_t size = handler->position_x.size;

				void* allocation = m_memory->Allocate(handler->MemoryOf(count), 8);
				
				uintptr_t buffer = (uintptr_t)allocation;
				uintptr_t position_x_start = buffer;
				buffer += sizeof(float) * count;
				uintptr_t position_y_start = buffer;
				buffer += sizeof(float) * count;
				uintptr_t late_update_start = buffer;
				buffer += sizeof(bool) * count;
				uintptr_t scale_x_start = buffer;
				buffer += sizeof(float) * count;
				uintptr_t scale_y_start = buffer;
				buffer += sizeof(float) * count;
				buffer = function::AlignPointer(buffer, alignof(UIActionHandler));
				uintptr_t action_start = buffer;

				memcpy((void*)position_x_start, handler->position_x.buffer, sizeof(float) * size);
				memcpy((void*)position_y_start, handler->position_y, sizeof(float) * size);
				memcpy((void*)scale_x_start, handler->scale_x, sizeof(float) * size);
				memcpy((void*)scale_y_start, handler->scale_y, sizeof(float) * size);
				memcpy((void*)action_start, handler->action, sizeof(UIActionHandler) * size);

				m_memory->Deallocate(handler->position_x.buffer);

				handler->position_x.capacity = count;
				handler->position_x.buffer = (float*)position_x_start;
				handler->position_y = (float*)position_y_start;
				handler->scale_x = (float*)scale_x_start;
				handler->scale_y = (float*)scale_y_start;
				handler->action = (UIActionHandler*)action_start;
				handler->Add(position, scale, action_handler);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddDefaultHoverable(const UISystemDefaultHoverableData& data)
		{
			AddHoverableToDockspaceRegion(
				data.thread_id,
				data.dockspace,
				data.border_index,
				data.position,
				data.scale,
				&data.data,
				DefaultHoverableAction,
				data.phase
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddHoverableToDockspaceRegion(
			unsigned int thread_id,
			UIDockspace* dockspace,
			unsigned int border_index, 
			float2 position,
			float2 scale,
			UIActionHandler handler
		)
		{
			AddHoverableToDockspaceRegion(
				TemporaryAllocator(thread_id, handler.phase),
				dockspace,
				border_index,
				position,
				scale,
				handler
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddHoverableToDockspaceRegion(
			LinearAllocator* allocator,
			UIDockspace* dockspace,
			unsigned int border_index,
			float2 position,
			float2 scale,
			UIActionHandler handler
		)
		{
			AddActionHandler(
				allocator,
				&dockspace->borders[border_index].hoverable_handler,
				position,
				scale,
				handler
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddDefaultClickable(const UISystemDefaultClickableData& data)
		{
			UIDefaultClickableData default_click;
			default_click.initial_clickable_phase = data.clickable_handler.phase;
			default_click.hoverable_handler = data.hoverable_handler;

			// making this nullptr in order for the write argument to copy correctly
			default_click.hoverable_handler.data = nullptr;
			default_click.click_handler = data.clickable_handler;
			void* clickable_data = data.clickable_handler.data == nullptr ? &default_click : data.clickable_handler.data;

			ECS_UI_DRAW_PHASE clickable_phase = data.clickable_handler.phase;
			if (!data.disable_system_phase_retarget && data.clickable_handler.phase == ECS_UI_DRAW_SYSTEM && data.hoverable_handler.phase != ECS_UI_DRAW_SYSTEM) {
				clickable_phase = data.hoverable_handler.phase;
			}

			AddHoverableToDockspaceRegion(
				data.thread_id,
				data.dockspace,
				data.border_index,
				data.position,
				data.scale,
				data.hoverable_handler
			);

			AddClickableToDockspaceRegion(
				data.thread_id,
				data.dockspace,
				data.border_index,
				data.position,
				data.scale,
				{
					DefaultClickableAction,
					&default_click,
					sizeof(UIDefaultClickableData) + data.hoverable_handler.data_size + data.clickable_handler.data_size,
					clickable_phase
				}
			);
			
			// Manually copy the data into the structure. Offset it such that it remains relocatable
			auto* handler = data.dockspace->borders[data.border_index].clickable_handler.GetLastHandler();
			uintptr_t handler_memory = (uintptr_t)handler->data;
			handler_memory += sizeof(UIDefaultClickableData);
			memcpy((void*)handler_memory, data.hoverable_handler.data, data.hoverable_handler.data_size);
			handler_memory += data.hoverable_handler.data_size;
			memcpy((void*)handler_memory, data.clickable_handler.data, data.clickable_handler.data_size);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddDefaultHoverableClickable(UISystemDefaultHoverableClickableData& data) {
			UISystemDefaultClickableData clickable_data;
			clickable_data.clickable_handler = data.clickable_handler;
			clickable_data.hoverable_handler.action = DefaultHoverableAction;
			clickable_data.hoverable_handler.data = &data.hoverable_data;
			clickable_data.hoverable_handler.data_size = sizeof(UIDefaultHoverableData);
			clickable_data.hoverable_handler.phase = data.hoverable_phase;
			clickable_data.dockspace = data.dockspace;
			clickable_data.position = data.position;
			clickable_data.scale = data.scale;
			clickable_data.thread_id = data.thread_id;
			clickable_data.border_index = data.border_index;
			clickable_data.disable_system_phase_retarget = data.disable_system_phase_retarget;
			AddDefaultClickable(clickable_data);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddClickableToDockspaceRegion(
			unsigned int thread_id,
			UIDockspace* dockspace,
			unsigned int border_index,
			float2 position,
			float2 scale,
			UIActionHandler handler
		)
		{
			AddClickableToDockspaceRegion(
				TemporaryAllocator(thread_id, handler.phase),
				dockspace,
				border_index,
				position,
				scale,
				handler
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddClickableToDockspaceRegion(
			LinearAllocator* allocator,
			UIDockspace* dockspace,
			unsigned int border_index,
			float2 position,
			float2 scale,
			UIActionHandler handler
		)
		{
			AddActionHandler(
				allocator,
				&dockspace->borders[border_index].clickable_handler,
				position,
				scale,
				handler
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddDoubleClickActionToDockspaceRegion(
			unsigned int thread_id, 
			UIDockspace* dockspace, 
			unsigned int border_index, 
			float2 position, 
			float2 scale, 
			unsigned int identifier,
			size_t duration_between_clicks,
			UIActionHandler first_click_handler,
			UIActionHandler second_click_handler
		)
		{
			UIDoubleClickData click_data;
			click_data.first_click_handler = first_click_handler;
			click_data.double_click_handler = second_click_handler;
			click_data.max_duration_between_clicks = duration_between_clicks;
			click_data.first_click_handler.data = nullptr;
			click_data.identifier = identifier;
			void* first_click_data = first_click_handler.data == nullptr ? &click_data : first_click_handler.data;
			void* second_click_data = second_click_handler.data == nullptr ? &click_data : second_click_handler.data;
			ECS_UI_DRAW_PHASE phase = first_click_handler.phase > second_click_handler.phase ? first_click_handler.phase : second_click_handler.phase;
			AddGeneralActionToDockspaceRegion(
				thread_id, 
				dockspace, 
				border_index, 
				position,
				scale, 
				{DoubleClickAction, &click_data, sizeof(click_data) + first_click_handler.data_size + second_click_handler.data_size, phase}
			);

			// Copy the data manually such that it is relocatable
			auto* current_handler = dockspace->borders[border_index].general_handler.GetLastHandler();
			uintptr_t current_memory = (uintptr_t)current_handler->data;
			current_memory += sizeof(click_data);

			memcpy((void*)current_memory, first_click_handler.data, first_click_handler.data_size);
			current_memory += first_click_handler.data_size;
			memcpy((void*)current_memory, second_click_handler.data, second_click_handler.data_size);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddGeneralActionToDockspaceRegion(
			unsigned int thread_id,
			UIDockspace* dockspace,
			unsigned int border_index,
			float2 position,
			float2 scale,
			UIActionHandler handler
		)
		{
			AddGeneralActionToDockspaceRegion(
				TemporaryAllocator(thread_id, handler.phase),
				dockspace,
				border_index,
				position,
				scale,
				handler
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddGeneralActionToDockspaceRegion(
			LinearAllocator* allocator,
			UIDockspace* dockspace,
			unsigned int border_index,
			float2 position,
			float2 scale,
			UIActionHandler handler
		)
		{
			AddActionHandler(
				allocator,
				&dockspace->borders[border_index].general_handler,
				position,
				scale,
				handler
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddWindowToDockspaceRegion(
			unsigned int window_index,
			unsigned int dockspace_index,
			CapacityStream<UIDockspace>& dockspace_stream,
			unsigned char border_index
		) {
			AddWindowToDockspaceRegion(window_index, &dockspace_stream[dockspace_index], border_index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddWindowToDockspaceRegion(unsigned int window_index, UIDockspace* dockspace, unsigned char border_index)
		{
			ECS_ASSERT(dockspace->borders[border_index].window_indices.size < dockspace->borders[border_index].window_indices.capacity);

			dockspace->borders[border_index].active_window = dockspace->borders[border_index].window_indices.size;
			dockspace->borders[border_index].window_indices.Add(window_index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float UISystem::AlignMiddleTextY(float position_y, float scale_y, float font_size_y, float padding)
		{
			float y_sprite_size = GetTextSpriteYScale(font_size_y);
			return AlignMiddle(position_y + padding, scale_y - 2.0f * padding, y_sprite_size);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void* UISystem::AllocateHandlerMemory(unsigned int thread_id, size_t size, size_t alignment, const void* memory_to_copy) {
			return AllocateHandlerMemory(&m_resources.thread_resources[thread_id].temp_allocator, size, alignment, memory_to_copy);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void* UISystem::AllocateHandlerMemory(LinearAllocator* allocator, size_t size, size_t alignment, const void* memory_to_copy) {
			void* allocation = allocator->Allocate(size, alignment);
			memcpy(allocation, memory_to_copy, size);
			return allocation;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddWindowToDockspace(
			unsigned char element_to_add,
			unsigned char element_position,
			unsigned int dockspace_index,
			CapacityStream<UIDockspace>& child_dockspace,
			CapacityStream<UIDockspace>& dockspace_stream,
			float border_width,
			float mask
		) {
			AddWindowToDockspace(element_to_add, element_position, &dockspace_stream[dockspace_index], child_dockspace, border_width, mask);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddWindowToDockspace(
			unsigned char element_index, 
			unsigned char element_position, 
			UIDockspace* dockspace,
			CapacityStream<UIDockspace>& child_dockspaces, 
			float border_width, 
			float mask
		)
		{
			ECS_ASSERT(dockspace->borders.size < dockspace->borders.capacity);

			float dockspace_size;
			float inverse_mask = 1.0f - mask;
			if (element_position == dockspace->borders.size - 1) {
				dockspace_size = dockspace->borders[element_position].position - dockspace->borders[element_position - 1].position;
			}
			else {
				dockspace_size = dockspace->borders[element_position + 1].position - dockspace->borders[element_position].position;
			}

			if (dockspace->borders[element_position].is_dock) {
				AppendDockspaceResize(
					dockspace->borders[element_position].window_indices[0],
					&child_dockspaces,
					dockspace->transform.scale.x * inverse_mask + dockspace->transform.scale.y * mask,
					dockspace->transform.scale.x * inverse_mask + dockspace->transform.scale.y * mask,
					dockspace_size * 0.5f,
					inverse_mask
				);
			}

			dockspace->borders.Add(dockspace->borders[dockspace->borders.size - 1]);

			// bumping all the other borders; -3: -1 for the last border, -1 for not copying the last element, 
			// -1 because the size increased
			for (int64_t index = (int64_t)dockspace->borders.size - 3; index >= element_position; index--) {
				dockspace->borders[index + 1] = dockspace->borders[index];
			}

			if (element_position == dockspace->borders.size - 2 && dockspace->borders.size > 2) {
				dockspace->borders[element_position].position -= dockspace_size * 0.5f;
				if (dockspace->borders[element_position - 1].is_dock) {
					AppendDockspaceResize(
						dockspace->borders[element_position - 1].window_indices[0],
						&child_dockspaces,
						child_dockspaces[dockspace->borders[element_position - 1].window_indices[0]].transform.scale.x * inverse_mask + child_dockspaces[dockspace->borders[element_position - 1].window_indices[0]].transform.scale.y * mask,
						child_dockspaces[dockspace->borders[element_position - 1].window_indices[0]].transform.scale.x * inverse_mask + child_dockspaces[dockspace->borders[element_position - 1].window_indices[0]].transform.scale.y * mask,
						dockspace->borders[element_position].position - dockspace->borders[element_position - 1].position,
						inverse_mask
					);
				}
			}
			else if (dockspace->borders.size == 2) {
				dockspace->borders[0].position = 0.0f;
			}
			else {
				dockspace->borders[element_position + 1].position += dockspace_size * 0.5f;
			}

			CreateDockspaceBorder(
				dockspace,
				element_position,
				dockspace->borders[element_position].position,
				element_index,
				false
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddDockspaceToDockspace(
			unsigned char element_to_add,
			DockspaceType child_type,
			unsigned char element_position,
			unsigned int dockspace_index,
			DockspaceType dockspace_type,
			float border_width,
			float adjust_child_border,
			float mask
		) {
			CapacityStream<UIDockspace>* dockspaces[] = {
				&m_horizontal_dockspaces,
				&m_vertical_dockspaces,
				&m_floating_horizontal_dockspaces,
				&m_floating_vertical_dockspaces
			};
			CapacityStream<UIDockspace>* dockspace_stream = dockspaces[(unsigned int)dockspace_type];
			AddDockspaceToDockspace(
				element_to_add,
				child_type,
				element_position,
				&dockspace_stream->buffer[dockspace_index],
				border_width,
				adjust_child_border,
				mask
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddDockspaceToDockspace(
			unsigned char child_index, 
			DockspaceType child_type,
			unsigned char element_position, 
			UIDockspace* dockspace,
			float border_width, 
			float adjust_dockspace_borders, 
			float mask
		)
		{
			CapacityStream<UIDockspace>* dockspaces[] = {
				&m_horizontal_dockspaces,
				&m_vertical_dockspaces,
				&m_floating_horizontal_dockspaces,
				&m_floating_vertical_dockspaces
			};
			CapacityStream<UIDockspace>* child_stream = dockspaces[(unsigned int)child_type];

			ECS_ASSERT(dockspace->borders.size < dockspace->borders.capacity);

			float dockspace_size;
			float inverse_mask = 1.0f - mask;
			if (element_position == dockspace->borders.size - 1) {
				dockspace_size = dockspace->borders[element_position].position - dockspace->borders[element_position - 1].position;
			}
			else {
				dockspace_size = dockspace->borders[element_position + 1].position - dockspace->borders[element_position].position;
			}

			if (dockspace->borders[element_position].is_dock) {
				AppendDockspaceResize(
					dockspace->borders[element_position].window_indices[0],
					child_stream,
					dockspace->transform.scale.x * inverse_mask + dockspace->transform.scale.y * mask,
					dockspace->transform.scale.x * inverse_mask + dockspace->transform.scale.y * mask,
					dockspace_size * 0.5f,
					inverse_mask
				);
			}

			dockspace->borders.Add(dockspace->borders[dockspace->borders.size - 1]);

			// bumping all the other borders; -3: -1 for the last border, -1 for not copying the last element, 
			// -1 because the size increased
			for (int64_t index = dockspace->borders.size - 3; index >= element_position; index--) {
				dockspace->borders[index + 1] = dockspace->borders[index];
			}

			if (element_position == dockspace->borders.size - 2) {
				dockspace->borders[element_position].position -= dockspace_size * 0.5f;
				if (dockspace->borders[element_position - 1].is_dock) {
					AppendDockspaceResize(
						dockspace->borders[element_position - 1].window_indices[0],
						child_stream,
						child_stream->buffer[dockspace->borders[element_position - 1].window_indices[0]].transform.scale.x * inverse_mask + child_stream->buffer[dockspace->borders[element_position - 1].window_indices[0]].transform.scale.y * mask,
						child_stream->buffer[dockspace->borders[element_position - 1].window_indices[0]].transform.scale.x * inverse_mask + child_stream->buffer[dockspace->borders[element_position - 1].window_indices[0]].transform.scale.y * mask,
						dockspace->borders[element_position].position - dockspace->borders[element_position - 1].position,
						inverse_mask
					);
				}
			}
			else {
				dockspace->borders[element_position + 1].position += dockspace_size * 0.5f;
			}

			CreateDockspaceBorder(
				dockspace,
				element_position,
				dockspace->borders[element_position].position,
				child_index,
				true
			);

			AppendDockspaceResize(
				child_index,
				child_stream,
				dockspace->transform.scale.x * inverse_mask + dockspace->transform.scale.y * mask,
				child_stream->buffer[child_index].transform.scale.x * inverse_mask + child_stream->buffer[child_index].transform.scale.y * mask,
				dockspace->borders[element_position + 1].position - dockspace->borders[element_position].position,
				inverse_mask
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddWindowMemoryResource(void* resource, unsigned int window_index)
		{
			m_windows[window_index].memory_resources.Add(resource);
			size_t size = m_windows[window_index].memory_resources.size;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddWindowMemoryResourceToTable(void* resource, ResourceIdentifier identifier, unsigned int window_index)
		{
			WindowTable* table = GetWindowTable(window_index);

			ECS_ASSERT(table->Find(identifier) == -1);
			InsertIntoDynamicTable(*table, m_memory, resource, identifier);
			//ECS_ASSERT(!table->Insert(resource, identifier));
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		AllocatorPolymorphic UISystem::Allocator() const
		{
			return GetAllocatorPolymorphic(m_memory);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AppendDockspaceResize(
			unsigned char dockspace_index,
			CapacityStream<UIDockspace>* dockspaces,
			float new_scale_to_resize,
			float old_scale,
			float other_scale,
			float mask
		) {
			ECS_ASSERT(dockspace_index < dockspaces->size);
			AppendDockspaceResize(&dockspaces->buffer[dockspace_index], new_scale_to_resize, old_scale, other_scale, mask);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AppendDockspaceResize(UIDockspace* dockspace, float new_scale_to_resize, float old_scale, float other_scale, float mask)
		{
			float inverse_mask = 1.0f - mask;
			float old_x = dockspace->transform.scale.x;
			float old_y = dockspace->transform.scale.y;
			dockspace->transform.scale.x = new_scale_to_resize * mask + other_scale * inverse_mask;
			dockspace->transform.scale.y = new_scale_to_resize * inverse_mask + other_scale * mask;

			float position_factor = new_scale_to_resize / old_scale;
			float old_positions[128];

			for (size_t index = 0; index < dockspace->borders.size; index++) {
				old_positions[index] = dockspace->borders[index].position;
			}
			for (size_t index = 0; index < dockspace->borders.size; index++) {
				dockspace->borders[index].position *= position_factor;
			}
			for (size_t index = 0; index < dockspace->borders.size - 1; index++) {
				if (dockspace->borders[index + 1].position - dockspace->borders[index].position
					< m_descriptors.dockspaces.border_minimum_distance) {
					dockspace->borders[index + 1].position = dockspace->borders[index].position + m_descriptors.dockspaces.border_minimum_distance;
				}
			}

			if (dockspace->borders[dockspace->borders.size - 1].position > new_scale_to_resize) {
				float position_step = new_scale_to_resize / (dockspace->borders.size - 1);
				for (size_t index = 0; index < dockspace->borders.size; index++) {
					dockspace->borders[index].position = position_step * index;
				}
			}
			if (mask == 0.0f) {
				for (size_t index = 0; index < dockspace->borders.size - 1; index++) {
					if (dockspace->borders[index].is_dock) {
						//m_horizontal_dockspaces[dockspaces[dockspace_index].borders[index].window_indices[0]].transform.scale.x = dockspaces[dockspace_index].borders[index + 1].position - dockspaces[dockspace_index].borders[index].position;
						AppendDockspaceResize(
							dockspace->borders[index].window_indices[0],
							&m_horizontal_dockspaces,
							dockspace->transform.scale.x,
							old_x,
							dockspace->borders[index + 1].position - dockspace->borders[index].position,
							inverse_mask
						);
					}
				}
			}
			else {
				for (size_t index = 0; index < dockspace->borders.size - 1; index++) {
					if (dockspace->borders[index].is_dock) {
						//m_vertical_dockspaces[dockspace->borders[index].window_indices[0]].transform.scale.y = dockspace->borders[index + 1].position - dockspace->borders[index].position;
						AppendDockspaceResize(
							dockspace->borders[index].window_indices[0],
							&m_vertical_dockspaces,
							dockspace->transform.scale.y,
							old_y,
							dockspace->borders[index + 1].position - dockspace->borders[index].position,
							inverse_mask
						);
					}
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AppendDockspaceResize(UIDockspace* dockspace, float other_scale, DockspaceType type)
		{
			const float masks[4] = { 1.0f, 0.0f, 1.0f, 0.0f };
			float mask = masks[(unsigned int)type];
			if (type == DockspaceType::Horizontal || type == DockspaceType::FloatingHorizontal) {
				AppendDockspaceResize(dockspace, dockspace->transform.scale.x, dockspace->transform.scale.x, other_scale, mask);
			}
			else if (type == DockspaceType::Vertical || type == DockspaceType::FloatingVertical) {
				AppendDockspaceResize(dockspace, dockspace->transform.scale.y, dockspace->transform.scale.y, other_scale, mask);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddElementToDockspace(
			unsigned char element_to_add,
			bool is_dock,
			unsigned char element_position,
			unsigned int dockspace_index,
			DockspaceType dockspace_type
		) {
			switch (dockspace_type) {
			case DockspaceType::Horizontal:
				if (is_dock) {
					AddDockspaceToDockspace(
						element_to_add,
						DockspaceType::Vertical,
						element_position,
						dockspace_index,
						DockspaceType::Horizontal,
						m_horizontal_dockspaces[dockspace_index].transform.scale.x,
						m_horizontal_dockspaces[dockspace_index].transform.scale.y,
						1.0f
					);
				}
				else {
					AddWindowToDockspace(
						element_to_add,
						element_position,
						dockspace_index,
						m_vertical_dockspaces,
						m_horizontal_dockspaces,
						m_horizontal_dockspaces[dockspace_index].transform.scale.x,
						1.0f
					);
				}
				break;
			case DockspaceType::Vertical:
				if (is_dock) {
					AddDockspaceToDockspace(
						element_to_add,
						DockspaceType::Horizontal,
						element_position,
						dockspace_index,
						DockspaceType::Vertical,
						m_vertical_dockspaces[dockspace_index].transform.scale.y,
						m_vertical_dockspaces[dockspace_index].transform.scale.x,
						0.0f
					);
				}
				else {
					AddWindowToDockspace(
						element_to_add,
						element_position,
						dockspace_index,
						m_horizontal_dockspaces,
						m_vertical_dockspaces,
						m_vertical_dockspaces[dockspace_index].transform.scale.y,
						0.0f
					);
				}
				break;
			case DockspaceType::FloatingHorizontal:
				if (is_dock) {
					AddDockspaceToDockspace(
						element_to_add,
						DockspaceType::Vertical,
						element_position,
						dockspace_index,
						DockspaceType::FloatingHorizontal,
						m_floating_horizontal_dockspaces[dockspace_index].transform.scale.x,
						m_floating_horizontal_dockspaces[dockspace_index].transform.scale.y,
						1.0f
					);
				}
				else {
					AddWindowToDockspace(
						element_to_add,
						element_position,
						dockspace_index,
						m_vertical_dockspaces,
						m_floating_horizontal_dockspaces,
						m_floating_horizontal_dockspaces[dockspace_index].transform.scale.x,
						1.0f
					);
				}
				break;
			case DockspaceType::FloatingVertical:
				if (is_dock) {
					AddDockspaceToDockspace(
						element_to_add,
						DockspaceType::Horizontal,
						element_position,
						dockspace_index,
						DockspaceType::FloatingVertical,
						m_floating_vertical_dockspaces[dockspace_index].transform.scale.y,
						m_floating_vertical_dockspaces[dockspace_index].transform.scale.x,
						0.0f
					);
				}
				else {
					AddWindowToDockspace(
						element_to_add,
						element_position,
						dockspace_index,
						m_horizontal_dockspaces,
						m_floating_vertical_dockspaces,
						m_floating_vertical_dockspaces[dockspace_index].transform.scale.y,
						0.0f
					);
				}
				break;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------
		
		void UISystem::AddElementToDockspace(
			unsigned char element_index,
			bool is_dock, 
			unsigned char element_position,
			DockspaceType dockspace_type,
			UIDockspace* dockspace
		)
		{
			switch (dockspace_type) {
			case DockspaceType::Horizontal:
				if (is_dock) {
					AddDockspaceToDockspace(
						element_index,
						DockspaceType::Vertical,
						element_position,
						dockspace,
						dockspace->transform.scale.x,
						dockspace->transform.scale.y,
						1.0f
					);
				}
				else {
					AddWindowToDockspace(
						element_index,
						element_position,
						dockspace,
						m_vertical_dockspaces,
						dockspace->transform.scale.x,
						1.0f
					);
				}
				break;
			case DockspaceType::Vertical:
				if (is_dock) {
					AddDockspaceToDockspace(
						element_index,
						DockspaceType::Horizontal,
						element_position,
						dockspace,
						dockspace->transform.scale.y,
						dockspace->transform.scale.x,
						0.0f
					);
				}
				else {
					AddWindowToDockspace(
						element_index,
						element_position,
						dockspace,
						m_horizontal_dockspaces,
						dockspace->transform.scale.y,
						0.0f
					);
				}
				break;
			case DockspaceType::FloatingHorizontal:
				if (is_dock) {
					AddDockspaceToDockspace(
						element_index,
						DockspaceType::Vertical,
						element_position,
						dockspace,
						dockspace->transform.scale.x,
						dockspace->transform.scale.y,
						1.0f
					);
				}
				else {
					AddWindowToDockspace(
						element_index,
						element_position,
						dockspace,
						m_vertical_dockspaces,
						dockspace->transform.scale.x,
						1.0f
					);
				}
				break;
			case DockspaceType::FloatingVertical:
				if (is_dock) {
					AddDockspaceToDockspace(
						element_index,
						DockspaceType::Horizontal,
						element_position,
						dockspace,
						dockspace->transform.scale.y,
						dockspace->transform.scale.x,
						0.0f
					);
				}
				else {
					AddWindowToDockspace(
						element_index,
						element_position,
						dockspace,
						m_horizontal_dockspaces,
						dockspace->transform.scale.y,
						0.0f
					);
				}
				break;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddWindowDynamicElement(
			unsigned int window_index, 
			Stream<char> name, 
			Stream<void*> allocations, 
			Stream<ResourceIdentifier> table_resources
		)
		{
			// Calculate the total memory needed
			size_t memory_size = name.size + sizeof(void*) * allocations.size + sizeof(ResourceIdentifier) * table_resources.size;
			for (size_t index = 0; index < table_resources.size; index++) {
				memory_size += table_resources[index].size;
			}
			
			// Make a coallesced allocation
			void* allocation = m_memory->Allocate(memory_size);
			uintptr_t ptr = (uintptr_t)allocation;

			UIWindowDynamicResource dynamic_resource;
			dynamic_resource.element_allocations.InitializeAndCopy(ptr, allocations);
			dynamic_resource.table_resources.InitializeFromBuffer(ptr, table_resources.size);
			dynamic_resource.reference_count = 2;
			
			// Copy the identifiers and place them inside the table resources buffer
			for (size_t index = 0; index < table_resources.size; index++) {
				dynamic_resource.table_resources[index].ptr = (void*)ptr;
				dynamic_resource.table_resources[index].size = table_resources[index].size;
				memcpy((void*)ptr, table_resources[index].ptr, table_resources[index].size);
				ptr += table_resources[index].size;
			}
			// Copy the name
			memcpy((void*)ptr, name.buffer, sizeof(char) * name.size);
			ResourceIdentifier identifier{(void*)ptr, (unsigned int)name.size};
			InsertIntoDynamicTable(m_windows[window_index].dynamic_resources, m_memory, dynamic_resource, identifier);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddWindowDynamicElementAllocation(unsigned int window_index, unsigned int index, void* allocation)
		{
			UIWindowDynamicResource* resource = m_windows[window_index].dynamic_resources.GetValuePtrFromIndex(index);
			resource->added_allocations.size++;
			size_t new_buffer_allocation_size = sizeof(void*) * resource->added_allocations.size;
			size_t buffer_copy_size = new_buffer_allocation_size - sizeof(void*);

			void* new_buffer = m_memory->Allocate(new_buffer_allocation_size);
			memcpy(new_buffer, resource->added_allocations.buffer, buffer_copy_size);

			void* old_buffer = resource->added_allocations.buffer;
			resource->added_allocations.buffer = (void**)new_buffer;
			resource->added_allocations[resource->added_allocations.size - 1] = allocation;

			if (resource->added_allocations.size > 1) {
				ReplaceWindowMemoryResource(window_index, old_buffer, new_buffer);
			}
			else {
				AddWindowMemoryResource(new_buffer, window_index);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::AddFrameHandler(UIActionHandler handler)
		{
			handler.data = function::CopyNonZero(GetAllocatorPolymorphic(m_memory), handler.data, handler.data_size);
			m_frame_handlers.AddSafe(handler);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::BindWindowHandler(Action action, Action data_initializer, size_t data_size)
		{
			m_window_handler.action = action;
			m_window_handler.data = data_initializer;
			m_window_handler.data_size = data_size;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::CalculateDockspaceRegionHeaders(
			unsigned int dockspace_index,
			unsigned int border_index,
			float offset_mask,
			const CapacityStream<UIDockspace>& dockspaces,
			float2* sizes
		) const
		{
			CalculateDockspaceRegionHeaders(&dockspaces[dockspace_index], border_index, offset_mask, sizes);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::CalculateDockspaceRegionHeaders(
			const UIDockspace* dockspace,
			unsigned int border_index,
			float offset_mask,
			float2* sizes
		) const {
			float first_letter_position = 0.0f;
			size_t header_count = dockspace->borders[border_index].window_indices.size;
			float total_header_length = 0.0f;
			float added_scale_and_close_x_padding = m_descriptors.dockspaces.region_header_added_scale + m_descriptors.dockspaces.close_x_total_x_padding;

			// first iteration is executed before the for in order to not add a spacing unnecessary
			const Stream<UISpriteVertex>* text_sprites = &m_windows[dockspace->borders[border_index].window_indices[0]].name_vertex_buffer;
			float last_letter_position = text_sprites->buffer[text_sprites->size - 2].position.x;
			total_header_length += last_letter_position + added_scale_and_close_x_padding;
			

			for (size_t index = 1; index < header_count; index++) {
				const Stream<UISpriteVertex>* text_sprites = &m_windows[dockspace->borders[border_index].window_indices[index]].name_vertex_buffer;
				float last_letter_position = text_sprites->buffer[text_sprites->size - 2].position.x;
				total_header_length += last_letter_position + m_descriptors.dockspaces.region_header_spacing + added_scale_and_close_x_padding;
			}

			float header_fraction = ((dockspace->borders[border_index + 1].position - dockspace->borders[border_index].position - m_descriptors.dockspaces.region_header_padding * 2) * offset_mask
				+ (dockspace->transform.scale.x - m_descriptors.dockspaces.region_header_padding * 2)* (1.0f - offset_mask)) / total_header_length;
			bool is_header_long_enough = header_fraction > 1.0f;
			header_fraction = header_fraction * (1 - is_header_long_enough) + 1.0f * is_header_long_enough;

			float header_offset = m_descriptors.dockspaces.region_header_padding;
			for (size_t index = 0; index < dockspace->borders[border_index].window_indices.size; index++) {
				const Stream<UISpriteVertex>* text_sprites = &m_windows[dockspace->borders[border_index].window_indices[index]].name_vertex_buffer;
				first_letter_position = text_sprites->buffer[0].position.x;
				float last_letter_position = text_sprites->buffer[text_sprites->size - 2].position.x;
				sizes[index] = float2(
					(/*dockspace->transform.position.x + */header_offset /*+ dockspace->borders[border_index].position * offset_mask*/) /** header_fraction*/,
					(last_letter_position - first_letter_position + added_scale_and_close_x_padding) * header_fraction
				);
				header_offset += ((last_letter_position - first_letter_position) + m_descriptors.dockspaces.region_header_spacing + added_scale_and_close_x_padding) * header_fraction;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ChangeWindowNameFromIndex(Stream<char> base_name, unsigned int current_index, unsigned int new_index)
		{
			ECS_STACK_CAPACITY_STREAM(char, full_name, 256);

			full_name.Copy(base_name);
			function::ConvertIntToChars(full_name, current_index);

			unsigned int window_index = GetWindowFromName(full_name);
			ECS_ASSERT(window_index != -1);

			full_name.size = base_name.size;
			function::ConvertIntToChars(full_name, new_index);
			SetWindowName(window_index, full_name);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::CheckDockspaceInnerBorders(
			float2 point_position,
			UIDockspace* dockspace,
			DockspaceType type
		) {
			const float masks[4] = { 1.0f, 0.0f, 1.0f, 0.0f };
			float mask = masks[(unsigned int)type];
			for (size_t index = 1; index < dockspace->borders.size - 1; index++) {
				float2 border_position = GetInnerDockspaceBorderPosition(dockspace, index, mask);
				float2 border_scale = GetInnerDockspaceBorderScale(dockspace, index, mask);
				if (IsPointInRectangle(
					point_position,
					border_position,
					border_scale
				)) {

					UIMoveDockspaceBorderEventData* data = (UIMoveDockspaceBorderEventData*)m_memory->Allocate(sizeof(UIMoveDockspaceBorderEventData));
					data->dockspace = dockspace;
					data->border_index = index;
					data->move_x = DockspaceType::FloatingHorizontal == type || DockspaceType::Horizontal == type;
					data->move_y = DockspaceType::FloatingVertical == type || DockspaceType::Vertical == type;
					data->type = type;
					if (m_mouse_tracker->LeftButton() == MBPRESSED) {
						m_event = MoveDockspaceBorderEvent;
						m_event_data = data;
					}
					else if (m_event == SkipEvent) {
						m_event = HoverInnerDockspaceBorderEvent;
						m_event_data = m_event_data = data;
					}
					else {
						m_memory->Deallocate(data);
					}
					return true;
				}
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::CheckDockspaceInnerBorders(
			float2 point_position,
			unsigned int dockspace_index,
			DockspaceType dockspace_type
		) {
			UIDockspace* dockspaces[4] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			return CheckDockspaceInnerBorders(
				point_position,
				&dockspaces[(unsigned int)dockspace_type][dockspace_index],
				dockspace_type
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::CheckParentInnerBorders(
			float2 mouse_position,
			const unsigned int* dockspace_indices,
			const DockspaceType* dockspace_types,
			unsigned int parent_count
		) {
			for (size_t index = 0; index < parent_count; index++) {
				if (CheckDockspaceInnerBorders(mouse_position, dockspace_indices[index], dockspace_types[index])) {
					return true;
				}
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::Clear()
		{
			auto destroy_type = [this](CapacityStream<UIDockspace>& dockspaces, DockspaceType type) {
				unsigned int delete_count = dockspaces.size;
				for (size_t index = 0; index < delete_count; index++) {
					DestroyDockspace<true, true>(dockspaces[0].borders.buffer, type);
				}
			};
			
			destroy_type(m_floating_horizontal_dockspaces, DockspaceType::FloatingHorizontal);
			destroy_type(m_floating_vertical_dockspaces, DockspaceType::FloatingVertical);

			m_background_dockspaces.size = 0;
			m_fixed_dockspaces.size = 0;
			m_pop_up_windows.size = 0;
			m_dockspace_layers.size = 0;
			m_frame_index = 0;
			m_texture_evict_count = 0;

			DeallocateClickableHandler();
			DeallocateHoverableHandler();
			DeallocateGeneralHandler();

			m_focused_window_data.ResetClickableHandler();
			m_focused_window_data.ResetGeneralHandler();
			m_focused_window_data.ResetHoverableHandler();

			for (size_t index = 0; index < m_frame_handlers.size; index++) {
				if (m_frame_handlers[index].data_size > 0) {
					m_memory->Deallocate(m_frame_handlers[index].data);
				}
			}
			m_frame_handlers.size = 0;

			m_focused_window_data.locked_window = false;

			ECS_ASSERT(m_horizontal_dockspaces.size == 0);
			ECS_ASSERT(m_vertical_dockspaces.size == 0);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ClearFixedDockspace(UIDockspace* dockspace, DockspaceType type)
		{
			for (size_t index = 0; index < dockspace->borders.size - 1; index++) {
				RemoveDockspaceBorder(dockspace, 0, type);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ClearFixedDockspace(UIDockspaceBorder* border, DockspaceType type)
		{
			UIDockspace* dockspaces[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			for (size_t index = 0; index < m_fixed_dockspaces.size; index++) {
				if (dockspaces[(unsigned int)m_fixed_dockspaces[index].type][m_fixed_dockspaces[index].index].borders.buffer == border) {
					ClearFixedDockspace(&dockspaces[(unsigned int)m_fixed_dockspaces[index].type][m_fixed_dockspaces[index].index], type);
					break;
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ConfigureToolTipBase(UITooltipBaseData* data) const
		{
			if (data->default_background) {
				if (data->background_color == ECS_COLOR_BLACK) {
					data->background_color = m_descriptors.misc.tool_tip_background_color;
				}
			}
			if (data->default_border) {
				if (data->border_color == ECS_COLOR_BLACK) {
					data->border_color = m_descriptors.misc.tool_tip_border_color;
				}
				if (data->border_size == float2(0.0f, 0.0f)) {
					data->border_size = GetSquareScale(m_descriptors.dockspaces.border_size);
				}
			}
			if (data->default_font) {
				if (data->font_color == ECS_COLOR_BLACK) {
					data->font_color = m_descriptors.misc.tool_tip_font_color;
				}

				if (data->unavailable_font_color == ECS_COLOR_BLACK) {
					data->unavailable_font_color = m_descriptors.misc.tool_tip_unavailable_font_color;
				}

				if (data->font_size == float2(0.0f, 0.0f)) {
					data->font_size = { m_descriptors.font.size * ECS_TOOLS_UI_FONT_X_FACTOR, m_descriptors.font.size };
				}

				if (data->character_spacing == 0.0f) {
					data->character_spacing = m_descriptors.font.character_spacing;
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ConvertCharactersToTextSprites(
			Stream<char> characters,
			float2 position,
			UISpriteVertex* vertex_buffer,
			Color color,
			unsigned int buffer_offset,
			float2 font_size,
			float character_spacing,
			bool horizontal,
			bool invert_order
		) {
			float position_x = position.x;
			float position_y = position.y;
			float2 atlas_dimensions = m_font_character_uvs[m_descriptors.font.texture_dimensions];
			float2 sprite_scale_factor = { atlas_dimensions.x * font_size.x, atlas_dimensions.y * font_size.y };
			float new_character_spacing;
			if (!horizontal) {
				new_character_spacing = character_spacing * 0.1f;
			}

			for (size_t index = 0; index < characters.size; index++) {
				unsigned int character_uv_index = FindCharacterType(characters[index]);

				size_t buffer_index;
				if (!invert_order) {
					buffer_index = index;
				}
				else {
					buffer_index = characters.size - 1 - index;
				}
				size_t character_index = character_uv_index * 2;
				// they are in uv space
				float2 top_left = m_font_character_uvs[character_index];
				float2 bottom_right = m_font_character_uvs[character_index + 1];
				float2 uv = float2(bottom_right.x - top_left.x, bottom_right.y - top_left.y);

				float2 scale = float2(uv.x * sprite_scale_factor.x, uv.y * sprite_scale_factor.y);
				float2 sprite_position = float2(position_x, position_y);

				size_t vertex_buffer_offset = buffer_offset + buffer_index * 6;
				SetTransformForRectangle(sprite_position, scale, vertex_buffer_offset, vertex_buffer);
				SetUVForRectangle(top_left, bottom_right, vertex_buffer_offset, vertex_buffer);
				SetColorForRectangle(color, vertex_buffer_offset, vertex_buffer);

				if (horizontal) {
					position_x = position_x + scale.x + character_spacing;
				}
				else {
					position_y = position_y + scale.y + new_character_spacing;
				}
			}
		}

		void UISystem::ConvertFloatToTextSprites(UISpriteVertex* vertices, size_t& count, float value, float2 position, size_t precision, Color color, float font_size, float character_spacing, bool horizontal, bool invert_order)
		{
			ConvertFloatToTextSprites(
				vertices,
				count,
				value,
				position,
				font_size * ECS_TOOLS_UI_FONT_X_FACTOR,
				font_size,
				precision,
				color,
				character_spacing,
				horizontal,
				invert_order
			);
		}

		void UISystem::ConvertDoubleToTextSprites(UISpriteVertex* vertices, size_t& count, double value, float2 position, size_t precision, Color color, float font_size, float character_spacing, bool horizontal, bool invert_order)
		{
			ConvertDoubleToTextSprites(
				vertices,
				count,
				value,
				position,
				font_size * ECS_TOOLS_UI_FONT_X_FACTOR,
				font_size,
				precision,
				color,
				character_spacing,
				horizontal,
				invert_order
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ConvertFloatToTextSprites(
			UISpriteVertex* vertices,
			size_t& count,
			float value,
			float2 position,
			float font_size_x,
			float font_size_y,
			size_t precision,
			Color color,
			float character_spacing,
			bool horizontal,
			bool invert_order
		) {
			char temp_chars[64];
			Stream<char> temp_stream(temp_chars, 0);

			function::ConvertFloatToChars(temp_stream, value, precision);

			ConvertCharactersToTextSprites(
				temp_stream,
				position,
				vertices,
				color,
				count,
				{ font_size_x, font_size_y },
				character_spacing,
				horizontal,
				!invert_order
			);
			count += 6 * temp_stream.size;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ConvertDoubleToTextSprites(
			UISpriteVertex* vertices,
			size_t& count,
			double value,
			float2 position,
			float font_size_x,
			float font_size_y,
			size_t precision,
			Color color,
			float character_spacing,
			bool horizontal,
			bool invert_order
		) {
			char temp_chars[64];
			Stream<char> temp_stream(temp_chars, 0);

			function::ConvertDoubleToChars(temp_stream, value, precision);

			ConvertCharactersToTextSprites(
				temp_stream,
				position,
				vertices,
				color,
				count,
				{ font_size_x, font_size_y },
				character_spacing,
				horizontal, 
				!invert_order
			);
			count += 6 * temp_stream.size;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		BorderHover UISystem::CheckDockspaceOuterBorders(
			float2 point_position,
			unsigned int dockspace_index,
			const CapacityStream<UIDockspace>& dockspace_stream
		) const {
			float2 dockspace_position = dockspace_stream[dockspace_index].transform.position;
			float2 dockspace_scale = dockspace_stream[dockspace_index].transform.scale;
			BorderHover initial_hover;
			initial_hover.value = 0;

			float2 border_position = GetOuterDockspaceBorderPosition(&dockspace_stream[dockspace_index], ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP);
			float2 border_scale = GetOuterDockspaceBorderScale(&dockspace_stream[dockspace_index], ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP);

			// top border
			if (IsPointInRectangle(
				point_position,
				border_position,
				border_scale
				)) {
				initial_hover.SetTop();
			}

			// bottom border; must flip scale
			else {
				border_position = GetOuterDockspaceBorderPosition(&dockspace_stream[dockspace_index], ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM);
				border_scale = GetOuterDockspaceBorderScale(&dockspace_stream[dockspace_index], ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM);

				if (IsPointInRectangle(
					point_position,
					border_position,
					border_scale)) {
					initial_hover.SetBottom();
				}
			}

			border_position = GetOuterDockspaceBorderPosition(&dockspace_stream[dockspace_index], ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT);
			border_scale = GetOuterDockspaceBorderScale(&dockspace_stream[dockspace_index], ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT);
			// left border
			if (IsPointInRectangle(
				point_position,
				border_position,
				border_scale
				)) {
				initial_hover.SetLeft();
				return initial_hover;
			}

			else {
				border_position = GetOuterDockspaceBorderPosition(&dockspace_stream[dockspace_index], ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT);
				border_scale = GetOuterDockspaceBorderScale(&dockspace_stream[dockspace_index], ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT);
				// right border
				if (IsPointInRectangle(
					point_position,
					border_position,
					border_scale)) {
					initial_hover.SetRight();
					return initial_hover;
				}
			}
			return initial_hover;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		BorderHover UISystem::CheckDockspaceOuterBorders(
			float2 point_position,
			unsigned int dockspace_index,
			DockspaceType dockspace_type
		) const {
			const CapacityStream<UIDockspace>* dockspaces[4] = { &m_horizontal_dockspaces, &m_vertical_dockspaces, &m_floating_horizontal_dockspaces, &m_floating_vertical_dockspaces };
			return CheckDockspaceOuterBorders(point_position, dockspace_index, *dockspaces[(unsigned int)dockspace_type]);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::CheckDockspaceResize(float2 mouse_position) {
			BorderHover border_hover;
			border_hover.value = 0;
			unsigned int hovered_floating_dockspace = 0xFFFFFFFF;
			for (size_t index = 0; border_hover.value == 0 && index < m_floating_horizontal_dockspaces.size; index++) {
				border_hover = CheckDockspaceOuterBorders(mouse_position, index, m_floating_horizontal_dockspaces);
				hovered_floating_dockspace = index * (border_hover.value != 0);
			}
			if (border_hover.value != 0) {
				const UIDockspace* dockspaces[] = {
					m_horizontal_dockspaces.buffer,
					m_vertical_dockspaces.buffer,
					m_floating_horizontal_dockspaces.buffer,
					m_floating_vertical_dockspaces.buffer
				};
				size_t index = 0;
				for (; index < m_fixed_dockspaces.size; index++) {
					if (&dockspaces[(unsigned int)m_fixed_dockspaces[index].type][m_fixed_dockspaces[index].index] ==
						m_floating_horizontal_dockspaces.buffer + hovered_floating_dockspace) {
						break;
					}
				}
				if (index == m_fixed_dockspaces.size) {
					void* allocation = m_memory->Allocate(sizeof(UIResizeEventData), alignof(UIResizeEventData));
					UIResizeEventData* reinterpretation = (UIResizeEventData*)allocation;
					reinterpretation->border_hover = border_hover;
					reinterpretation->dockspace_index = hovered_floating_dockspace;
					reinterpretation->dockspace_type = DockspaceType::FloatingHorizontal;
					if (m_mouse_tracker->LeftButton() == MBPRESSED) {
						m_event = ResizeDockspaceEvent;
						m_event_data = allocation;
					}
					else if (m_event == SkipEvent) {
						m_event = HoverOuterDockspaceBorderEvent;
						m_event_data = m_event_data = allocation;
					}
					else {
						m_memory->Deallocate(allocation);
					}
					return true;
				}
				return false;
			}
			for (size_t index = 0; border_hover.value == 0 && index < m_floating_vertical_dockspaces.size; index++) {
				border_hover = CheckDockspaceOuterBorders(mouse_position, index, m_floating_vertical_dockspaces);
				hovered_floating_dockspace = index * (border_hover.value != 0);
			}
			if (border_hover.value != 0) {
				const UIDockspace* dockspaces[] = {
					m_horizontal_dockspaces.buffer,
					m_vertical_dockspaces.buffer,
					m_floating_horizontal_dockspaces.buffer,
					m_floating_vertical_dockspaces.buffer
				};
				size_t index = 0;
				for (; index < m_fixed_dockspaces.size; index++) {
					if (&dockspaces[(unsigned int)m_fixed_dockspaces[index].type][m_fixed_dockspaces[index].index] ==
						m_floating_vertical_dockspaces.buffer + hovered_floating_dockspace) {
						break;
					}
				}
				if (index == m_fixed_dockspaces.size) {
					void* allocation = m_memory->Allocate(sizeof(UIResizeEventData), alignof(UIResizeEventData));
					UIResizeEventData* reinterpretation = (UIResizeEventData*)allocation;
					reinterpretation->border_hover = border_hover;
					reinterpretation->dockspace_index = hovered_floating_dockspace;
					reinterpretation->dockspace_type = DockspaceType::FloatingVertical;
					if (m_mouse_tracker->LeftButton() == MBPRESSED) {
						m_event = ResizeDockspaceEvent;
						m_event_data = allocation;
					}
					else if (m_event == SkipEvent) {
						m_event = HoverOuterDockspaceBorderEvent;
						m_event_data = m_event_data = allocation;
					}
					else {
						m_memory->Deallocate(allocation);
					}
					return true;
				}
				return false;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::CheckDockspaceResize(unsigned int dockspace_index, DockspaceType type, float2 mouse_position) {
			BorderHover border_hover;
			border_hover.value = 0;
			border_hover = CheckDockspaceOuterBorders(mouse_position, dockspace_index, type);
			if (border_hover.value != 0) {
				const UIDockspace* dockspaces[] = {
					m_horizontal_dockspaces.buffer,
					m_vertical_dockspaces.buffer,
					m_floating_horizontal_dockspaces.buffer,
					m_floating_vertical_dockspaces.buffer
				};
				size_t index = 0;
				for (; index < m_fixed_dockspaces.size; index++) {
					if (&dockspaces[(unsigned int)m_fixed_dockspaces[index].type][m_fixed_dockspaces[index].index] ==
						&dockspaces[(unsigned int)type][dockspace_index]) {
						break;
					}
				}
				if (index == m_fixed_dockspaces.size) {
					void* allocation = m_memory->Allocate(sizeof(UIResizeEventData), alignof(UIResizeEventData));
					UIResizeEventData* reinterpretation = (UIResizeEventData*)allocation;
					reinterpretation->border_hover = border_hover;
					reinterpretation->dockspace_index = dockspace_index;
					reinterpretation->dockspace_type = type;
					if (m_mouse_tracker->LeftButton() == MBPRESSED) {
						m_event = ResizeDockspaceEvent;
						m_event_data = allocation;
					}
					else if (m_event == SkipEvent) {
						m_event = HoverOuterDockspaceBorderEvent;
						m_event_data = m_event_data = allocation;
					}
					else {
						m_memory->Deallocate(allocation);
					}
					return true;
				}
				return false;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::CreateDockspace(
			UIElementTransform transform, 
			CapacityStream<UIDockspace>& dockspace_stream,
			float border_position,
			unsigned char element, 
			bool is_dock,
			DockspaceType type,
			size_t flags
		) {
			ECS_ASSERT(dockspace_stream.size < dockspace_stream.capacity);
			ECS_ASSERT(m_dockspace_layers.size < m_dockspace_layers.capacity);

			size_t dockspace_index = dockspace_stream.size;
			dockspace_stream.size++;

			// setting transform
			dockspace_stream[dockspace_index].transform = transform;
			
			// flags
			dockspace_stream[dockspace_index].allow_docking = (flags & UI_DOCKSPACE_NO_DOCKING) == 0;
			if (function::HasFlag(flags, UI_DOCKSPACE_BACKGROUND)) {
				m_background_dockspaces.AddSafe({ static_cast<unsigned int>(dockspace_index), type });
			}

			// creating borders
			dockspace_stream[dockspace_index].borders.buffer = (UIDockspaceBorder*)m_memory->Allocate(
				sizeof(UIDockspaceBorder) * m_descriptors.dockspaces.max_border_count, 
				alignof(UIDockspaceBorder)
			);
			memset(dockspace_stream[dockspace_index].borders.buffer, 0, sizeof(UIDockspaceBorder) * m_descriptors.dockspaces.max_border_count);
			dockspace_stream[dockspace_index].borders.capacity = m_descriptors.dockspaces.max_border_count;
			dockspace_stream[dockspace_index].borders.size = 2;

			CreateDockspaceBorder(&dockspace_stream[dockspace_index], 0, 0.0f, element, is_dock, flags);

			if (is_dock) {
				if (type == DockspaceType::FloatingHorizontal || type == DockspaceType::Horizontal) {
					AppendDockspaceResize(
						element,
						&m_vertical_dockspaces,
						dockspace_stream[dockspace_index].transform.scale.y,
						m_vertical_dockspaces[element].transform.scale.y,
						dockspace_stream[dockspace_index].transform.scale.x,
						0.0f
					);
				}
				else if (type == DockspaceType::FloatingVertical || type == DockspaceType::Vertical) {
					AppendDockspaceResize(
						element,
						&m_horizontal_dockspaces,
						dockspace_stream[dockspace_index].transform.scale.y,
						m_horizontal_dockspaces[element].transform.scale.y,
						dockspace_stream[dockspace_index].transform.scale.x,
						1.0f
					);
				}
			}

			// creating second border; this is not meant to hold any windows or docks
			dockspace_stream[dockspace_index].borders[1].position = border_position;
			dockspace_stream[dockspace_index].borders[1].is_dock = false;
			dockspace_stream[dockspace_index].borders[1].clickable_handler.position_x.buffer = nullptr;
			dockspace_stream[dockspace_index].borders[1].hoverable_handler.position_x.buffer = nullptr;
			dockspace_stream[dockspace_index].borders[1].general_handler.position_x.buffer = nullptr;
			dockspace_stream[dockspace_index].borders[1].draw_elements = false;
			dockspace_stream[dockspace_index].borders[1].draw_region_header = false;
			
			if (type == DockspaceType::FloatingHorizontal || type == DockspaceType::FloatingVertical) {
				// bump all other dockspaces and make this as the active one, not using memcpy because of aliasing
				for (int64_t index = m_dockspace_layers.size; index >= 1; index--) {
					m_dockspace_layers[index] = m_dockspace_layers[index - 1];
				}
				m_dockspace_layers[0].index = dockspace_index;
				m_dockspace_layers[0].type = type;
				m_dockspace_layers.size++;

				for (size_t index = 0; index < m_pop_up_windows.size; index++) {
					m_pop_up_windows[index]++;
				}

				UIDockspace* dockspaces[] = {
						m_horizontal_dockspaces.buffer,
						m_vertical_dockspaces.buffer,
						m_floating_horizontal_dockspaces.buffer,
						m_floating_vertical_dockspaces.buffer
				};
				SearchAndSetNewFocusedDockspaceRegion(&dockspaces[(unsigned int)type][dockspace_index], 0, type);
			}

			return dockspace_index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::CreateEmptyDockspace(CapacityStream<UIDockspace>& dockspace_stream, DockspaceType type)
		{
			return CreateDockspace(
				{ {0.0f, 0.0f}, {0.0f, 0.0f} },
				dockspace_stream,
				0.0f,
				0,
				false,
				type
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::CreateFixedDockspace(
			UIElementTransform transform,
			CapacityStream<UIDockspace>& dockspace_stream, 
			float border_position, 
			unsigned char element, 
			bool is_dock,
			DockspaceType type,
			size_t flags
		)
		{
			ECS_ASSERT(dockspace_stream.buffer == m_floating_vertical_dockspaces.buffer || dockspace_stream.buffer == m_floating_horizontal_dockspaces.buffer);

			unsigned int dockspace_index = CreateDockspace(transform, dockspace_stream, border_position, element, is_dock, type, flags);
			m_fixed_dockspaces.AddSafe({ dockspace_index, type });
			return dockspace_index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::CreateDockspace(
			UIElementTransform transform, 
			DockspaceType dockspace_type,
			unsigned char element,
			bool is_dock,
			size_t flags
		) {
			CapacityStream<UIDockspace>* dockspaces[] = {
				&m_horizontal_dockspaces,
				&m_vertical_dockspaces,
				&m_floating_horizontal_dockspaces,
				&m_floating_vertical_dockspaces,
			};
			const float border_position[] = {
				transform.scale.x,
				transform.scale.y,
				transform.scale.x,
				transform.scale.y
			};
			return CreateDockspace(transform, *dockspaces[(unsigned int)dockspace_type], border_position[(unsigned int)dockspace_type], element, is_dock, dockspace_type, flags);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::CreateEmptyDockspace(DockspaceType type)
		{
			return CreateDockspace(
				{ {0.0f, 0.0f}, {0.0f, 0.0f} },
				type,
				0,
				false
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::CreateFixedDockspace(
			UIElementTransform transform,
			DockspaceType dockspace_type,
			unsigned char element,
			bool is_dock,
			size_t flags
		)
		{
			ECS_ASSERT(dockspace_type == DockspaceType::FloatingHorizontal || dockspace_type == DockspaceType::FloatingVertical);

			unsigned int dockspace_index = CreateDockspace(transform, dockspace_type, element, is_dock, flags);
			UIDockspaceLayer layer = { dockspace_index, dockspace_type };
			for (size_t index = 0; index < m_fixed_dockspaces.size; index++) {
				ECS_ASSERT(m_fixed_dockspaces[index].index != layer.index || m_fixed_dockspaces[index].type != layer.type)
			}
			m_fixed_dockspaces.AddSafe(layer);
			return dockspace_index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::CreateDockspace(UIDockspace* dockspace_to_copy, DockspaceType type_to_create)
		{
			CapacityStream<UIDockspace>* dockspaces[] = {
				&m_horizontal_dockspaces,
				&m_vertical_dockspaces,
				&m_floating_horizontal_dockspaces,
				&m_floating_vertical_dockspaces
			};

			ECS_ASSERT(dockspaces[(unsigned int)type_to_create]->size < dockspaces[(unsigned int)type_to_create]->capacity);
			
			// cannot memcpy since they will alias buffers and when deallocating one will conflict with the other
			// so every border must be manually created and copied using the API
			unsigned int new_dockspace_index = CreateDockspace(
				dockspace_to_copy->transform,
				type_to_create,
				dockspace_to_copy->borders[0].window_indices[0],
				dockspace_to_copy->borders[0].is_dock
			);
			
			UIDockspace* new_dockspace = &dockspaces[(unsigned int)type_to_create]->buffer[new_dockspace_index];
			for (size_t index = 1; index < dockspace_to_copy->borders[0].window_indices.size; index++) {
				new_dockspace->borders[0].window_indices[index] = dockspace_to_copy->borders[0].window_indices[index];
			}
			new_dockspace->borders[0].window_indices.size = dockspace_to_copy->borders[0].window_indices.size;
			new_dockspace->borders[0].active_window = dockspace_to_copy->borders[0].active_window;
			new_dockspace->borders[0].draw_elements = dockspace_to_copy->borders[0].draw_elements;
			new_dockspace->borders[0].draw_region_header = dockspace_to_copy->borders[0].draw_region_header;
			new_dockspace->borders[0].draw_close_x = dockspace_to_copy->borders[0].draw_close_x;

			for (size_t index = 1; index < dockspace_to_copy->borders.size - 1; index++) {
				CreateDockspaceBorder(
					new_dockspace, 
					index, 
					dockspace_to_copy->borders[index].position, 
					dockspace_to_copy->borders[index].window_indices[0], 
					dockspace_to_copy->borders[index].is_dock
				);
				for (size_t subindex = 1; subindex < dockspace_to_copy->borders[index].window_indices.size; subindex++) {
					new_dockspace->borders[index].window_indices[subindex] = dockspace_to_copy->borders[index].window_indices[subindex];
				}
				new_dockspace->borders[index].is_dock = dockspace_to_copy->borders[index].is_dock;
				new_dockspace->borders[index].window_indices.size = dockspace_to_copy->borders[index].window_indices.size;
				new_dockspace->borders[index].active_window = dockspace_to_copy->borders[index].active_window;
				new_dockspace->borders[index].draw_elements = dockspace_to_copy->borders[index].draw_elements;
				new_dockspace->borders[index].draw_region_header = dockspace_to_copy->borders[index].draw_region_header;
				new_dockspace->borders[index].draw_close_x = dockspace_to_copy->borders[index].draw_close_x;
			}

			unsigned int last_index = dockspace_to_copy->borders.size - 1;
			new_dockspace->borders[last_index].position = dockspace_to_copy->borders[last_index].position;
			new_dockspace->borders.size = dockspace_to_copy->borders.size;

			return new_dockspace_index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::CreateFixedDockspace(UIDockspace* dockspace_to_copy, DockspaceType type_to_create)
		{
			ECS_ASSERT(type_to_create == DockspaceType::FloatingHorizontal || type_to_create == DockspaceType::FloatingVertical);

			unsigned int dockspace_index = CreateDockspace(dockspace_to_copy, type_to_create);
			m_fixed_dockspaces.AddSafe({ dockspace_index, type_to_create });
			return dockspace_index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::CreateSpriteTexture(Stream<wchar_t> filename, UISpriteTexture* sprite_texture)
		{
			ResourceManagerTextureDesc descriptor;
			ResourceView view = m_resource_manager->LoadTexture<true>(filename, &descriptor);

			Texture2D texture(view.GetResource());
			uint2 dimensions = GetTextureDimensions(texture);

			*sprite_texture = view;

			unsigned int total_pixels = dimensions.x * dimensions.y;
			if (total_pixels > 512 * 512) {
				m_resources.texture_semaphore.Enter();

				ProcessTextureData data;
				data.filename = function::StringCopy(GetAllocatorPolymorphic(&m_resources.thread_resources[0].temp_allocator), filename).buffer;
				data.system = this;
				data.texture = sprite_texture;
				
				ThreadTask process_texture_task;
				process_texture_task = ECS_THREAD_TASK_NAME(ProcessTexture, &data, sizeof(data));
				m_task_manager->AddDynamicTaskAndWake(process_texture_task);
			}	
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::Create_Window(const UIWindowDescriptor& descriptor, bool do_not_initialize_handler) {
			ECS_ASSERT(m_windows.size < m_windows.capacity);

			size_t window_index = m_windows.size;
			m_windows.size++;

			// setting draw function
			m_windows[window_index].draw = descriptor.draw;

			// setting initial transform and render region
			m_windows[window_index].transform.position = float2(descriptor.initial_position_x, descriptor.initial_position_y);
			m_windows[window_index].transform.scale = float2(descriptor.initial_size_x, descriptor.initial_size_y);
			m_windows[window_index].render_region_offset = float2(0.0f, 0.0f);
			m_windows[window_index].drawer_draw_difference = { 0.0f, 0.0f };
			// in order to not accidentally deallocate the name buffer when setting the name 
			m_windows[window_index].name_vertex_buffer.buffer = nullptr;

			void* window_drawer_allocation = m_memory->Allocate(sizeof(UIWindowDrawerDescriptor), alignof(UIWindowDrawerDescriptor));
			m_windows[window_index].descriptors = (UIWindowDrawerDescriptor*)window_drawer_allocation;
			for (size_t index = 0; index < (unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT; index++) {
				m_windows[window_index].descriptors->configured[index] = false;
			}
			memcpy(&m_windows[window_index].descriptors->color_theme, &m_descriptors.color_theme, sizeof(UIColorThemeDescriptor));
			memcpy(&m_windows[window_index].descriptors->element_descriptor, &m_descriptors.element_descriptor, sizeof(UIElementDescriptor));
			memcpy(&m_windows[window_index].descriptors->font, &m_descriptors.font, sizeof(UIFontDescriptor));
			memcpy(&m_windows[window_index].descriptors->layout, &m_descriptors.window_layout, sizeof(UILayoutDescriptor));

			AllocatorPolymorphic polymorphic_memory = GetAllocatorPolymorphic(m_memory);
			// misc stuff
			m_windows[window_index].memory_resources.Initialize(polymorphic_memory, 0);
			m_windows[window_index].dynamic_resources.Initialize(m_memory, 128);
			m_windows[window_index].zoom.x = 1.0f;
			m_windows[window_index].zoom.y = 1.0f;
			m_windows[window_index].pin_horizontal_slider_count = 0;
			m_windows[window_index].pin_vertical_slider_count = 0;
			m_windows[window_index].max_zoom = ECS_TOOLS_UI_WINDOW_MAX_ZOOM;
			m_windows[window_index].min_zoom = ECS_TOOLS_UI_WINDOW_MIN_ZOOM;

			m_windows[window_index].private_handler.action = descriptor.private_action;
			m_windows[window_index].private_handler.data = function::CopyNonZero(Allocator(), descriptor.private_action_data, descriptor.private_action_data_size);
			m_windows[window_index].private_handler.data_size = descriptor.private_action_data_size;

			m_windows[window_index].destroy_handler = { descriptor.destroy_action, descriptor.destroy_action_data, (unsigned int)descriptor.destroy_action_data_size };
			m_windows[window_index].destroy_handler.data = function::CopyNonZero(Allocator(), descriptor.destroy_action_data, descriptor.destroy_action_data_size);

			m_windows[window_index].window_data = function::CopyNonZero(Allocator(), descriptor.window_data, descriptor.window_data_size);
			m_windows[window_index].window_data_size = descriptor.window_data_size;

			// resource table
			unsigned short resource_count = descriptor.resource_count == 0 ? m_descriptors.misc.window_table_default_count : descriptor.resource_count;
			m_windows[window_index].table.Initialize(m_memory, resource_count);
			
			m_windows[window_index].name_vertex_buffer.buffer = nullptr;
			SetWindowName(window_index, descriptor.window_name);
			if (descriptor.draw != nullptr) {
				InitializeWindowDraw(window_index, descriptor.draw);
			}

			m_windows[window_index].default_handler.action = m_window_handler.action;
			m_windows[window_index].default_handler.data = m_memory->Allocate(m_window_handler.data_size);

			if (!do_not_initialize_handler) {
				InitializeWindowDefaultHandler(window_index, descriptor);
			}
			
			return window_index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::CreateEmptyWindow()
		{
			UIWindowDescriptor descriptor;
			descriptor.window_name = "Empty";
			descriptor.draw = nullptr;

			// must allocate handler revert command buffer since if it is being destroyed
			// it will not affect deallocations
			unsigned int window_index = Create_Window(descriptor, true);

			// These will act as placeholders, they will need to be replaced by a proper assignment
			UIDefaultWindowHandler* handler_data = GetDefaultWindowHandlerData(window_index);
			handler_data->revert_commands.Initialize(m_memory, 1);
			return window_index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::CreateWindowAndDockspace(const UIWindowDescriptor& descriptor, size_t additional_flags)
		{
			UIWindowDescriptor temp_descriptor;
			const UIWindowDescriptor* descriptor_to_use = &descriptor;
			if (function::HasFlag(additional_flags, UI_POP_UP_WINDOW_FIT_TO_CONTENT)) {
				memcpy(&temp_descriptor, &descriptor, sizeof(temp_descriptor));
				descriptor_to_use = &temp_descriptor;

				temp_descriptor.initial_position_x = 0.0f;
				temp_descriptor.initial_position_y = 0.0f;
				
				temp_descriptor.initial_size_x = 0.5f;
				temp_descriptor.initial_size_y = 0.5f;
			}

			// create window normally
			unsigned int window_index = Create_Window(*descriptor_to_use);

			// if there are no more floating horizontal dockspaces, make a floating vertical
			DockspaceType type = DockspaceType::FloatingHorizontal;
			if (m_floating_horizontal_dockspaces.size == m_floating_horizontal_dockspaces.capacity) {
				type = DockspaceType::FloatingVertical;
			}

			// create the dockspace according to fixed flag
			unsigned int dockspace_index;
			if (!function::HasFlag(additional_flags, UI_DOCKSPACE_FIXED)) {
				dockspace_index = CreateDockspace(
					{ { descriptor_to_use->initial_position_x, descriptor_to_use->initial_position_y }, { descriptor_to_use->initial_size_x, descriptor_to_use->initial_size_y} },
					type,
					window_index,
					false,
					additional_flags
				);
			}
			else {
				dockspace_index = CreateFixedDockspace(
					{ { descriptor_to_use->initial_position_x, descriptor_to_use->initial_position_y }, {descriptor_to_use->initial_size_x, descriptor_to_use->initial_size_y} },
					type,
					window_index,
					false,
					additional_flags
				);
			}

			// pop up window
			if (function::HasFlag(additional_flags, UI_DOCKSPACE_POP_UP_WINDOW)) {
				// the layer will be bumped back
				m_pop_up_windows.Add((unsigned int)0);
			}

			// locked window
			if (function::HasFlag(additional_flags, UI_DOCKSPACE_LOCK_WINDOW)) {
				m_focused_window_data.locked_window++;

				// Clear hoverable, clickable and general handlers
				if (m_focused_window_data.hoverable_handler.action != nullptr) {
					HandleFocusedWindowCleanupHoverable({ -2.0f, -2.0f }, 0);

					DeallocateHoverableHandler();
					m_focused_window_data.ResetHoverableHandler();
				}
				if (m_focused_window_data.clickable_handler.action != nullptr) {
					DeallocateClickableHandler();
					m_focused_window_data.ResetClickableHandler();
				}
				if (m_focused_window_data.clickable_handler.action != nullptr) {
					HandleFocusedWindowCleanupGeneral({ -2.0f, -2.0f }, 0);

					DeallocateGeneralHandler();
					m_focused_window_data.ResetGeneralHandler();
				}
			}

			// Implement fit to content window
			if ((function::HasFlag(additional_flags, UI_POP_UP_WINDOW_FIT_TO_CONTENT) || function::HasFlag(additional_flags, UI_POP_UP_WINDOW_FIT_TO_CONTENT_ADD_RENDER_SLIDER_SIZE)) != 0) {
				// create dummy buffers for the drawer
				void* buffers[ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES];
				void* system_buffers[ECS_TOOLS_UI_MATERIALS];
				size_t counts[ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES] = { 0 };
				size_t system_counts[ECS_TOOLS_UI_MATERIALS] = { 0 };

				// normal and late buffers
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES; index++) {
					buffers[index] = m_memory->Allocate(m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_SOLID_COLOR] * sizeof(UISpriteVertex));
				}
				// system buffers
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
					system_buffers[index] = m_memory->Allocate(m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_SOLID_COLOR + ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES] * sizeof(UISpriteVertex));
				}

				// set drawer parameters; GetDrawerDescriptor gives nullptrs
				float2 new_scale;
				UIDrawerDescriptor drawer_descriptor = GetDrawerDescriptor(window_index);
				drawer_descriptor.export_scale = &new_scale;
				drawer_descriptor.buffers = buffers;
				drawer_descriptor.counts = counts;
				drawer_descriptor.system_buffers = system_buffers;
				drawer_descriptor.system_counts = system_counts;

				m_windows[window_index].descriptors->layout.next_row_padding *= 0.5f;
				m_windows[window_index].descriptors->layout.next_row_y_offset *= 0.5f;

				descriptor.draw(m_windows[window_index].window_data, &drawer_descriptor, false);
				UIDockspace* dockspace = GetDockspace(dockspace_index, type);
				dockspace->transform.scale = new_scale;
				dockspace->transform.scale.x += function::HasFlag(additional_flags, UI_POP_UP_WINDOW_FIT_TO_CONTENT_ADD_RENDER_SLIDER_SIZE) * m_descriptors.misc.render_slider_vertical_size;
				dockspace->transform.scale.y += function::HasFlag(additional_flags, UI_POP_UP_WINDOW_FIT_TO_CONTENT_ADD_RENDER_SLIDER_SIZE) * m_descriptors.misc.render_slider_horizontal_size;
				
				if (function::HasFlag(additional_flags, UI_POP_UP_WINDOW_FIT_TO_CONTENT_CENTER)) {
					dockspace->transform.position.x = AlignMiddle(-1.0f, 2.0f, dockspace->transform.scale.x);
					dockspace->transform.position.y = AlignMiddle(-1.0f, 2.0f, dockspace->transform.scale.y);
				}

				// patch border position
				if (type == DockspaceType::FloatingHorizontal) {
					dockspace->borders[1].position = dockspace->transform.scale.x;
				}
				else {
					dockspace->borders[1].position = dockspace->transform.scale.y;
				}

				// deallocate normal and late buffers
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES; index++) {
					m_memory->Deallocate(buffers[index]);
				}

				// deallocate system buffers
				for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
					m_memory->Deallocate(system_buffers[index]);
				}
			}
			return window_index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::CullRegionHeader(
			const UIDockspace* dockspace,
			unsigned int border_index, 
			unsigned int window_index_in_region,
			float offset_mask,
			float add_x,
			const float2* sizes, 
			void** buffers, 
			size_t* counts
		)
		{
			UISpriteVertex* reinterpretation = (UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE];
			int64_t index = 0;
			size_t maximum_vertex_count = m_windows[dockspace->borders[border_index].window_indices[window_index_in_region]].name_vertex_buffer.size;
			size_t initial_count = counts[ECS_TOOLS_UI_TEXT_SPRITE];
			float vertex_added_scale = m_descriptors.dockspaces.region_header_added_scale * 0.5f;
			
			float2 region_position = GetDockspaceRegionPosition(dockspace, border_index, offset_mask);
			float font_size_y_scale = GetTextSpriteYScale(m_descriptors.font.size);
			float vertex_y_position = -AlignMiddle(region_position.y, m_descriptors.misc.title_y_scale, font_size_y_scale);
			float vertex_y_translation = vertex_y_position - m_windows[dockspace->borders[border_index].window_indices[window_index_in_region]].name_vertex_buffer[0].position.y;
			for (; index < maximum_vertex_count; index++) {
				size_t buffer_index = index + initial_count;
				if (index % 6 == 1 &&
					m_windows[dockspace->borders[border_index].window_indices[window_index_in_region]].name_vertex_buffer[index].position.x - m_windows[dockspace->borders[border_index].window_indices[window_index_in_region]].name_vertex_buffer[0].position.x > sizes[window_index_in_region].y - vertex_added_scale - m_descriptors.dockspaces.close_x_total_x_padding) {
					counts[ECS_TOOLS_UI_TEXT_SPRITE] -= 1;
					break;
				}
				reinterpretation[buffer_index] = m_windows[dockspace->borders[border_index].window_indices[window_index_in_region]].name_vertex_buffer[index];
				reinterpretation[buffer_index].position.x += add_x + vertex_added_scale;
				reinterpretation[buffer_index].position.y = m_windows[dockspace->borders[border_index].window_indices[window_index_in_region]].name_vertex_buffer[index].position.y
					+ vertex_y_translation;
				reinterpretation[buffer_index].SetColor(m_descriptors.color_theme.text);
				counts[ECS_TOOLS_UI_TEXT_SPRITE]++;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::CullHandler(UIHandler* handler, float top, float left, float bottom, float right) const
		{
			Vec8f tops(top), lefts(left), bottoms(bottom), rights(right);
			size_t regular_size = handler->position_x.size & (-tops.size());
			size_t index = 0;
			for (; index < regular_size; index += tops.size()) {
				Vec8f first_x = Vec8f().load(handler->position_x.buffer + index);
				Vec8f first_y = Vec8f().load(handler->position_y + index);
				Vec8f second_x = Vec8f().load(handler->scale_x + index);
				Vec8f second_y = Vec8f().load(handler->scale_y + index);
				second_x += first_x;
				second_y += first_y;
				first_x = select(first_x < lefts, lefts, first_x);
				first_y = select(first_y < tops, tops, first_y);
				second_x = select(second_x > rights, rights, second_x);
				second_y = select(second_y > bottoms, bottoms, second_y);
				second_x -= first_x;
				second_y -= first_y;
				first_x.store(handler->position_x.buffer + index);
				first_y.store(handler->position_y + index);
				second_x.store(handler->scale_x + index);
				second_y.store(handler->scale_y + index);
			}
			for (; index < handler->position_x.size; index++) {
				handler->position_x[index] = handler->position_x[index] < left ? left : handler->position_x[index];
				handler->position_y[index] = handler->position_y[index] < top ? top : handler->position_y[index];
				handler->scale_x[index] = handler->scale_x[index] + handler->position_x[index] > right ? right - handler->position_x[index] : handler->scale_x[index];
				handler->scale_y[index] = handler->scale_y[index] + handler->position_y[index] > bottom ? bottom - handler->position_y[index] : handler->scale_y[index];
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DeallocateDockspaceBorderResource(UIDockspace* dockspace, unsigned int border_index)
		{
			// releasing graphics objects
			dockspace->borders[border_index].draw_resources.Release(m_graphics);

			// main allocation
			m_memory->Deallocate(dockspace->borders[border_index].draw_resources.buffers.buffer);
			m_memory->Deallocate(dockspace->borders[border_index].hoverable_handler.position_x.buffer);
			m_memory->Deallocate(dockspace->borders[border_index].clickable_handler.position_x.buffer);
			m_memory->Deallocate(dockspace->borders[border_index].general_handler.position_x.buffer);

			// the sprite texture buffer must not be deallocated since it is packed with the whole border allocation
			for (size_t index = 0; index < dockspace->borders[border_index].draw_resources.sprite_textures.size; index++) {
				dockspace->borders[border_index].draw_resources.sprite_textures[index].FreeBuffer();
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DeallocateClickableHandler()
		{
			if (m_focused_window_data.clickable_handler.action != nullptr && m_focused_window_data.clickable_handler.data_size != 0) {
				m_memory->Deallocate(m_focused_window_data.clickable_handler.data);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DeallocateHoverableHandler()
		{
			if (m_focused_window_data.hoverable_handler.action != nullptr && m_focused_window_data.hoverable_handler.data_size != 0) {
				m_memory->Deallocate(m_focused_window_data.hoverable_handler.data);
			}
			m_focused_window_data.always_hoverable = false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DeallocateGeneralHandler() 
		{
			if (m_focused_window_data.general_handler.action != nullptr && m_focused_window_data.general_handler.data_size != 0) {
				m_memory->Deallocate(m_focused_window_data.general_handler.data);
			}
			m_focused_window_data.clean_up_call_general = false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::CreateDockspaceBorder(
			UIDockspace* dockspace, 
			unsigned int border_index, 
			float border_position,
			unsigned char element,
			bool is_dock,
			size_t border_flags
		)
		{
			size_t total_memory = 0;
			// first border required buffers
			unsigned int vertex_buffer_count = m_descriptors.materials.count * ECS_TOOLS_UI_PASSES;
			total_memory += sizeof(unsigned short) * m_descriptors.dockspaces.max_windows_border;
			total_memory += sizeof(VertexBuffer) * vertex_buffer_count;
			total_memory += sizeof(UIDynamicStream<UISpriteTexture>) * ECS_TOOLS_UI_PASSES * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS;
			total_memory += 8;

			void* allocation = m_memory->Allocate(total_memory, alignof(VertexBuffer));
			memset(allocation, 0, total_memory);
			void* hoverable_handler_allocation = m_memory->Allocate(UIHandler::MemoryOf(m_descriptors.dockspaces.border_default_hoverable_handler_count), alignof(UIActionHandler));
			void* clickable_handler_allocation = m_memory->Allocate(UIHandler::MemoryOf(m_descriptors.dockspaces.border_default_clickable_handler_count), alignof(UIActionHandler));
			void* general_handler_allocation = m_memory->Allocate(UIHandler::MemoryOf(m_descriptors.dockspaces.border_default_general_handler_count), alignof(UIActionHandler));

			uintptr_t buffer = (uintptr_t)allocation;

			dockspace->borders[border_index].draw_resources.buffers = CapacityStream<VertexBuffer>(
				(void*)buffer,
				vertex_buffer_count,
				vertex_buffer_count
			);
			buffer += sizeof(VertexBuffer) * vertex_buffer_count;

			dockspace->borders[border_index].draw_resources.sprite_textures = CapacityStream<UIDynamicStream<UISpriteTexture>>(
				(void*)buffer, 
				ECS_TOOLS_UI_PASSES * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS, 
				ECS_TOOLS_UI_PASSES * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS
			);
			buffer += sizeof(UIDynamicStream<UISpriteTexture>) * dockspace->borders[border_index].draw_resources.sprite_textures.capacity;

			for (size_t index = 0; index < dockspace->borders[border_index].draw_resources.sprite_textures.size; index++) {
				dockspace->borders[border_index].draw_resources.sprite_textures[index] = UIDynamicStream<UISpriteTexture>(
					GetAllocatorPolymorphic(m_memory),
					m_descriptors.dockspaces.border_default_sprite_texture_count
				);
			}

			for (size_t index = 0; index < vertex_buffer_count; index++) {
				dockspace->borders[border_index].draw_resources.buffers[index] = m_graphics->CreateVertexBuffer(
					m_descriptors.materials.strides[index],
					m_descriptors.materials.vertex_buffer_count[index]
				);
			}

			dockspace->borders[border_index].draw_resources.sprite_cluster_subtreams = UIDynamicStream<unsigned int>(
				GetAllocatorPolymorphic(m_memory),
				ECS_TOOLS_UI_CLUSTER_SPRITE_SUBSTREAM_INITIAL_COUNT
			);

			dockspace->borders[border_index].window_indices = CapacityStream<unsigned short>(
				(void*)buffer,
				1,
				m_descriptors.dockspaces.max_windows_border
			);

			buffer += sizeof(unsigned short) * m_descriptors.dockspaces.max_windows_border;

			buffer = (uintptr_t)hoverable_handler_allocation;
			dockspace->borders[border_index].hoverable_handler.position_x = CapacityStream<float>((void*)buffer, 0, m_descriptors.dockspaces.border_default_hoverable_handler_count);
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_hoverable_handler_count;
			dockspace->borders[border_index].hoverable_handler.position_y = (float*)buffer;
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_hoverable_handler_count;
			dockspace->borders[border_index].hoverable_handler.scale_x = (float*)buffer;
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_hoverable_handler_count;
			dockspace->borders[border_index].hoverable_handler.scale_y = (float*)buffer;
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_hoverable_handler_count;

			buffer = function::AlignPointer(buffer, alignof(UIActionHandler));

			dockspace->borders[border_index].hoverable_handler.action = (UIActionHandler*)buffer;
			buffer += sizeof(UIActionHandler) * m_descriptors.dockspaces.border_default_hoverable_handler_count;

			buffer = (uintptr_t)clickable_handler_allocation;
			dockspace->borders[border_index].clickable_handler.position_x = CapacityStream<float>((void*)buffer, 0, m_descriptors.dockspaces.border_default_clickable_handler_count);
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_clickable_handler_count;
			dockspace->borders[border_index].clickable_handler.position_y = (float*)buffer;
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_clickable_handler_count;
			dockspace->borders[border_index].clickable_handler.scale_x = (float*)buffer;
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_clickable_handler_count;
			dockspace->borders[border_index].clickable_handler.scale_y = (float*)buffer;
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_clickable_handler_count;

			buffer = function::AlignPointer(buffer, alignof(UIActionHandler));

			dockspace->borders[border_index].clickable_handler.action = (UIActionHandler*)buffer;
			buffer += sizeof(UIActionHandler) * m_descriptors.dockspaces.border_default_clickable_handler_count;

			buffer = (uintptr_t)general_handler_allocation;
			dockspace->borders[border_index].general_handler.position_x = CapacityStream<float>((void*)buffer, 0, m_descriptors.dockspaces.border_default_general_handler_count);
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_general_handler_count;
			dockspace->borders[border_index].general_handler.position_y = (float*)buffer;
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_general_handler_count;
			dockspace->borders[border_index].general_handler.scale_x = (float*)buffer;
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_general_handler_count;
			dockspace->borders[border_index].general_handler.scale_y = (float*)buffer;
			buffer += sizeof(float) * m_descriptors.dockspaces.border_default_general_handler_count;

			buffer = function::AlignPointer(buffer, alignof(UIActionHandler));
			dockspace->borders[border_index].general_handler.action = (UIActionHandler*)buffer;

			dockspace->borders[border_index].draw_resources.region_viewport_info = m_graphics->CreateConstantBuffer(sizeof(float) * ECS_TOOLS_UI_CONSTANT_BUFFER_FLOAT_SIZE);

			// border window info
			dockspace->borders[border_index].position = border_position;
			dockspace->borders[border_index].window_indices[0] = element;
			dockspace->borders[border_index].is_dock = is_dock;
			dockspace->borders[border_index].active_window = 0;
			
			// flags
			if ((border_flags & UI_DOCKSPACE_BORDER_NOTHING) != 0) {
				dockspace->borders[border_index].draw_close_x = false;
				dockspace->borders[border_index].draw_region_header = false;
				dockspace->borders[border_index].draw_elements = false;
			}
			else {
				dockspace->borders[border_index].draw_region_header = (border_flags & UI_DOCKSPACE_BORDER_FLAG_COLLAPSED_REGION_HEADER) == 0;
				dockspace->borders[border_index].draw_close_x = (border_flags & UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X) == 0;
				dockspace->borders[border_index].draw_elements = (border_flags & UI_DOCKSPACE_BORDER_FLAG_NO_TITLE) == 0;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::CreateEmptyDockspaceBorder(UIDockspace* dockspace, unsigned int border_index)
		{
			CreateDockspaceBorder(dockspace, border_index, 0.0f, 0, false);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DeallocateEventData() {
			m_memory->Deallocate(m_event_data);
			m_event_data = nullptr;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DecrementWindowDynamicResource(unsigned int window_index)
		{
			m_windows[window_index].dynamic_resources.ForEachIndex([&](unsigned int index) {
				UIWindowDynamicResource* dynamic_resource = m_windows[window_index].dynamic_resources.GetValuePtrFromIndex(index);
				dynamic_resource->reference_count--;
				if (dynamic_resource->reference_count == 0) {
					// A new dynamic resource will replace this one; so decrement the current index in order to keep
					// checking the right resource
					RemoveWindowDynamicResource(window_index, index);
					return true;
				}
				return false;
			});
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		template<bool destroy_internal_dockspaces, bool deallocate_borders>
		void UISystem::DestroyDockspace(UIDockspaceBorder* border, DockspaceType type) {
			CapacityStream<UIDockspace>* dockspaces[] = {
				&m_horizontal_dockspaces,
				&m_vertical_dockspaces,
				&m_floating_horizontal_dockspaces,
				&m_floating_vertical_dockspaces
			};

			auto dockspace_layers_bump = [&]() {
				if (type == DockspaceType::FloatingHorizontal || type == DockspaceType::FloatingVertical) {
					size_t index = 0;
					for (; index < m_dockspace_layers.size; index++) {
						if (dockspaces[(unsigned int)m_dockspace_layers[index].type]->buffer[m_dockspace_layers[index].index].borders.buffer == border) {
							// bump forward all the other layers
							for (size_t subindex = index; subindex < m_dockspace_layers.size - 1; subindex++) {
								m_dockspace_layers[subindex] = m_dockspace_layers[subindex + 1];
							}
							// restore references for pop up windows
							for (size_t subindex = 0; subindex < m_pop_up_windows.size; subindex++) {
								m_pop_up_windows[subindex] -= m_pop_up_windows[subindex] > index;
							}
							break;
						}
					}
					if (index == 0) {
						SetNewFocusedDockspace(&dockspaces[(unsigned int)m_dockspace_layers[0].type]->buffer[m_dockspace_layers[0].index], m_dockspace_layers[0].type);
					}
					m_dockspace_layers.size--;
				}
			};

			auto deallocate_buffers = [this](UIDockspace* dockspace) {
				if constexpr (deallocate_borders) {
					for (size_t subindex = 0; subindex < dockspace->borders.size - 1; subindex++) {
						if (!dockspace->borders[subindex].is_dock) {
							for (size_t window_index = 0; window_index < dockspace->borders[subindex].window_indices.size; window_index++) {
								DestroyWindow(dockspace->borders[subindex].window_indices[window_index]);
								// The border then must have it's index made an invalid one in order to not affect the 
								// repairing of the window indices - set it to the window size so not other border
								// will be affected
								dockspace->borders[subindex].window_indices[window_index] = m_windows.size;
							}
						}
						DeallocateDockspaceBorderResource(dockspace, subindex);
					}
				}
			};

			// check to see if it is a pop up dockspace or a fixed dockspace
			if (type == DockspaceType::FloatingHorizontal || type == DockspaceType::FloatingVertical) {
				unsigned int pop_index = IsPopUpWindow(border);
				if (pop_index != -1) {
					m_pop_up_windows.RemoveSwapBack(pop_index);
				}

				if (IsFixedDockspace(border)) {
					unsigned int replaced_index = 0;
					for (size_t index = 0; index < m_fixed_dockspaces.size; index++) {
						if (dockspaces[(unsigned int)m_fixed_dockspaces[index].type]->buffer[m_fixed_dockspaces[index].index].borders.buffer == border) {
							replaced_index = m_fixed_dockspaces[index].index;
							m_fixed_dockspaces.RemoveSwapBack(index);
							break;
						}
					}
				}
			}

			ECS_ASSERT(dockspaces[(unsigned int)type]->size > 0);
			if constexpr (!destroy_internal_dockspaces) {
				dockspace_layers_bump();

				for (size_t index = 0; index < dockspaces[(unsigned int)type]->size; index++) {
					if (dockspaces[(unsigned int)type]->buffer[index].borders.buffer == border) {
						UIDockspace* dockspace = &dockspaces[(unsigned int)type]->buffer[index];
						deallocate_buffers(dockspace);

						m_memory->Deallocate(dockspaces[(unsigned int)type]->buffer[index].borders.buffer);
						dockspaces[(unsigned int)type]->RemoveSwapBack(index);

						// repair the references to the last element that swapped the current index
						RepairDockspaceReferences(dockspaces[(unsigned int)type]->size, type, index);
						break;
					}
				}

				if (m_focused_window_data.active_location.dockspace->borders.buffer == border) {
					SetNewFocusedDockspaceRegion(
						&dockspaces[(unsigned int)m_dockspace_layers[0].type]->buffer[m_dockspace_layers[0].index],
						0,
						m_dockspace_layers[0].type
					);
				}
			}
			else {
				UIDockspace* child_dockspaces[] = {
					m_vertical_dockspaces.buffer,
					m_horizontal_dockspaces.buffer,
					m_vertical_dockspaces.buffer,
					m_horizontal_dockspaces.buffer
				};

				DockspaceType child_type[] = {
					DockspaceType::Vertical,
					DockspaceType::Horizontal,
					DockspaceType::Vertical,
					DockspaceType::Horizontal
				};

				dockspace_layers_bump();

				for (size_t index = 0; index < dockspaces[(unsigned int)type]->size; index++) {
					if (dockspaces[(unsigned int)type]->buffer[index].borders.buffer == border) {
						UIDockspace* dockspace = &dockspaces[(unsigned int)type]->buffer[index];
						for (size_t subindex = 0; subindex < dockspace->borders.size; subindex++) {
							if (dockspace->borders[subindex].is_dock) {
								DestroyDockspace<true, deallocate_borders>(
									child_dockspaces[(unsigned int)type][dockspace->borders[subindex].window_indices[0]].borders.buffer,
									child_type[(unsigned int)type]
									);
							}
						}

						deallocate_buffers(dockspace);

						m_memory->Deallocate(dockspaces[(unsigned int)type]->buffer[index].borders.buffer);
						dockspaces[(unsigned int)type]->RemoveSwapBack(index);

						// repair the references to the last element that swapped the current index
						RepairDockspaceReferences(dockspaces[(unsigned int)type]->size, type, index);
						break;
					}
				}

				if (m_focused_window_data.active_location.dockspace->borders.buffer == border) {
					SetNewFocusedDockspaceRegion(
						&dockspaces[(unsigned int)m_dockspace_layers[0].type]->buffer[m_dockspace_layers[0].index],
						0,
						m_dockspace_layers[0].type
					);
				}
			}
			DestroyNonReferencedWindows();
		}

		ECS_TEMPLATE_FUNCTION_DOUBLE_BOOL(void, UISystem::DestroyDockspace, UIDockspaceBorder* border, DockspaceType);

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::DestroyWindow(unsigned int window_index)
		{
			ECS_ASSERT(window_index < m_windows.size);

			// Call the destroy function first
			if (m_windows[window_index].destroy_handler.action != nullptr) {
				ActionData action_data = GetFilledActionData(window_index);

				action_data.data = m_windows[window_index].destroy_handler.data;
				action_data.additional_data = m_windows[window_index].window_data;
				m_windows[window_index].destroy_handler.action(&action_data);
			}
			if (m_windows[window_index].destroy_handler.data_size > 0) {
				m_memory->Deallocate(m_windows[window_index].destroy_handler.data);
			}

			size_t max_dynamic_resource_count = m_windows[window_index].dynamic_resources.GetExtendedCapacity();
			for (size_t index = 0; index < max_dynamic_resource_count; index++) {
				if (m_windows[window_index].dynamic_resources.IsItemAt(index)) {
					RemoveWindowDynamicResource(window_index, index);
				}
			}
			m_memory->Deallocate(m_windows[window_index].dynamic_resources.GetAllocatedBuffer());

			for (size_t index = 0; index < m_windows[window_index].memory_resources.size; index++) {
				m_memory->Deallocate(m_windows[window_index].memory_resources[index]);
			}
			m_windows[window_index].memory_resources.FreeBuffer();
			
			auto default_handler_data = GetDefaultWindowHandlerData(window_index);
			m_memory->Deallocate(default_handler_data->revert_commands.GetAllocatedBuffer());
			m_memory->Deallocate(default_handler_data);

			if (m_windows[window_index].private_handler.data_size != 0) {
				m_memory->Deallocate(m_windows[window_index].private_handler.data);
			}

			if (m_windows[window_index].window_data_size > 0) {
				m_memory->Deallocate(m_windows[window_index].window_data);
			}

			m_memory->Deallocate(m_windows[window_index].table.GetAllocatedBuffer());
			m_memory->Deallocate(m_windows[window_index].name_vertex_buffer.GetAllocatedBuffer());

			m_memory->Deallocate(m_windows[window_index].descriptors);
			
			return RepairWindowReferences(window_index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		template<bool destroy_fixed_dockspace>
		bool UISystem::DestroyWindowIfFound(Stream<char> name)
		{
			unsigned int window_index = GetWindowFromName(name);
			if (window_index != 0xFFFFFFFF) {
				DockspaceType type;
				unsigned int border_index;
				UIDockspace* dockspace = GetDockspaceFromWindow(window_index, border_index, type);
				ECS_ASSERT(dockspace != nullptr);

				if (!destroy_fixed_dockspace) {
					RemoveWindowFromDockspaceRegion(dockspace, type, border_index, window_index);
				}
				else {
					// For pop up window
					if (dockspace->borders.size == 2 && dockspace->borders[0].window_indices.size == 1 && !dockspace->borders[0].is_dock) {
						DestroyDockspace(dockspace->borders.buffer, type);
						//RemoveDockspaceBorder(dockspace, border_index, type);
					}
					else {
						RemoveWindowFromDockspaceRegion(dockspace, type, border_index, window_index);
						DestroyWindow(window_index);
					}
				}
				return true;
			}
			return false;
		}

		ECS_TEMPLATE_FUNCTION_BOOL(bool, UISystem::DestroyWindowIfFound, Stream<char>);

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DestroyNonReferencedWindows()
		{
			bool reference_table[256] = {false};

			auto set_references = [&reference_table](CapacityStream<UIDockspace>& dockspaces) {
				for (size_t index = 0; index < dockspaces.size; index++) {
					UIDockspace* dockspace = &dockspaces[index];
					for (size_t border_index = 0; border_index < dockspace->borders.size; border_index++) {
						if (!dockspace->borders[border_index].is_dock) {
							for (size_t window_index = 0; window_index < dockspace->borders[border_index].window_indices.size; window_index++) {
								reference_table[dockspace->borders[border_index].window_indices[window_index]] = true;
							}
						}
					}
				}
			};
			set_references(m_floating_horizontal_dockspaces);
			set_references(m_floating_vertical_dockspaces);
			set_references(m_horizontal_dockspaces);
			set_references(m_vertical_dockspaces);

			for (int64_t index = 0; index < m_windows.size; index++) {
				if (!reference_table[index]) {
					index -= !DestroyWindow(index);
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::DetectEvents(float2 mouse_position) {
			DockspaceType dockspace_type(DockspaceType::FloatingVertical);
			unsigned int dockspaces_to_search[32];
			DockspaceType dockspace_types[32];
			unsigned int parent_count = 0;
			unsigned int hovered_floating_dockspace = GetFloatingDockspaceIndexFromMouseCoordinates(mouse_position, dockspace_type);
			bool succeded;

			if (hovered_floating_dockspace == 0xFFFFFFFF) {
				if (m_focused_window_data.clean_up_call_general && m_mouse_tracker->LeftButton() == MBPRESSED) {
					HandleFocusedWindowCleanupGeneral(mouse_position, 0);
					m_focused_window_data.ResetGeneralHandler();
				}
				if (m_focused_window_data.clean_up_call_hoverable) {
					HandleFocusedWindowCleanupHoverable(mouse_position, 0);
					m_focused_window_data.ResetHoverableHandler();
				}
				return !CheckDockspaceResize(mouse_position);
			}
			else {
				succeded = CheckDockspaceResize(hovered_floating_dockspace, dockspace_type, mouse_position);
				if (!succeded) {
					DockspaceType copy = dockspace_type;
					unsigned int hovered_dockspace = GetDockspaceIndexFromMouseCoordinatesWithChildren(
						mouse_position,
						dockspace_type,
						dockspaces_to_search,
						dockspace_types,
						parent_count
					);
					succeded = CheckDockspaceInnerBorders(mouse_position, hovered_dockspace, dockspace_type);
					if (!succeded) {
						succeded = CheckParentInnerBorders(mouse_position, dockspaces_to_search, dockspace_types, parent_count);
						if (!succeded) {
							UIDockspace* dockspaces[] = {
								m_horizontal_dockspaces.buffer,
								m_vertical_dockspaces.buffer,
								m_floating_horizontal_dockspaces.buffer,
								m_floating_vertical_dockspaces.buffer
							};
							UIDockspace* hovered_dockspace = &dockspaces[(unsigned int)dockspace_type][hovered_floating_dockspace];
							if (dockspace_type == DockspaceType::FloatingHorizontal || dockspace_type == DockspaceType::FloatingVertical) {
								if (IsEmptyFixedDockspace(hovered_dockspace) && m_mouse_tracker->LeftButton() == MBPRESSED 
									&& m_keyboard->IsKeyDown(HID::Key::LeftShift)
								)
								DestroyDockspace(hovered_dockspace->borders.buffer, dockspace_type);
							}
						}
					}
				}
				if (succeded && (m_event == ResizeDockspaceEvent || m_event == MoveDockspaceBorderEvent)) {
					HandleFocusedWindowCleanupGeneral(mouse_position, 0);
					m_focused_window_data.ResetGeneralHandler();
				}
				return succeded;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::DetectActiveHandler(const UIHandler* handler, float2 mouse_position, unsigned int offset) const {
			Vec8f simd_position_x, simd_position_y, simd_scale_x, simd_scale_y;
			Vec8f mouse_position_x(mouse_position.x), mouse_position_y(mouse_position.y);
			size_t regular_size = (handler->position_x.size - offset) & (-simd_position_x.size());

			size_t final_index = 0xFFFFFFFF;
			for (int64_t index = offset; index < regular_size; index += simd_position_x.size()) {
				simd_position_x.load(handler->position_x.buffer + index);
				simd_position_y.load(handler->position_y + index);
				simd_scale_x.load(handler->scale_x + index);
				simd_scale_y.load(handler->scale_y + index);
				int rectangle_index = AVX2IsPointInRectangle<Vec8f>(mouse_position_x, mouse_position_y, simd_position_x, simd_position_y, simd_scale_x, simd_scale_y);
				if (rectangle_index != -1 && index + rectangle_index < handler->position_x.size) {
					final_index = rectangle_index + index;
					index -= 7 - rectangle_index;
				}
			}

			for (size_t index = regular_size; index < handler->position_x.size; index++) {
				if (IsPointInRectangle(
					mouse_position,
					float2(handler->position_x[index], handler->position_y[index]),
					float2(handler->scale_x[index], handler->scale_y[index])
				)) {
					final_index = index;
				}
			}

			return final_index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::DetectHoverables(
			size_t* counts,
			void** buffers,
			UIDockspace* dockspace,
			unsigned int border_index,
			DockspaceType type,
			float2 mouse_position,
			unsigned int offset
		) {
			float dockspace_mask = GetDockspaceMaskFromType(type);

			float2 region_position = GetDockspaceRegionPosition(dockspace, border_index, dockspace_mask);
			float2 region_scale = GetDockspaceRegionScale(dockspace, border_index, dockspace_mask);
			//CullHandler(&dockspace->borders[border_index].hoverable_handler, region_position.y + m_descriptors.window.title_y_scale, region_position.x, region_position.y + region_scale.y, region_position.x + region_scale.x);
			unsigned int hoverable_index = DetectActiveHandler(&dockspace->borders[border_index].hoverable_handler, mouse_position, offset);

			void* additional_data = nullptr;
			if (m_focused_window_data.hoverable_handler.action != nullptr && m_focused_window_data.clean_up_call_hoverable) {
				if (hoverable_index != 0xFFFFFFFF) {
					UIHandler* handler = &dockspace->borders[border_index].hoverable_handler;
					if (m_focused_window_data.hoverable_handler.action == handler->action[hoverable_index].action) {
						additional_data = handler->action[hoverable_index].data;
					}
				}
				m_focused_window_data.additional_hoverable_data = additional_data;

				HandleFocusedWindowCleanupHoverable(mouse_position, 0, additional_data);

				if (additional_data != nullptr) {
					additional_data = m_focused_window_data.hoverable_handler.data;
					if (m_focused_window_data.hoverable_handler.data_size > 0) {
						void* allocation = m_memory->Allocate(m_focused_window_data.hoverable_handler.data_size);
						memcpy(allocation, additional_data, m_focused_window_data.hoverable_handler.data_size);
						m_focused_window_data.additional_hoverable_data = allocation;
						additional_data = allocation;
					}
				}
				m_focused_window_data.clean_up_call_hoverable = false;
			}

			if (hoverable_index != 0xFFFFFFFF) {
				const UIHandler* hoverable_handler = &dockspace->borders[border_index].hoverable_handler;

				if (m_focused_window_data.hoverable_handler.action != nullptr && m_focused_window_data.hoverable_handler.data_size != 0) {
					m_memory->Deallocate(m_focused_window_data.hoverable_handler.data);
				}

				if (hoverable_handler->action[hoverable_index].data_size != 0) {
					void* allocation = m_memory->Allocate(hoverable_handler->action[hoverable_index].data_size, 8);
					memcpy(allocation, hoverable_handler->action[hoverable_index].data, hoverable_handler->action[hoverable_index].data_size);
					m_focused_window_data.ChangeHoverableHandler(hoverable_handler, hoverable_index, allocation);
					m_focused_window_data.hovered_location.border_index = border_index;
					m_focused_window_data.hovered_location.dockspace = dockspace;
					m_focused_window_data.hovered_location.type = type;
				}
				else {
					m_focused_window_data.ChangeHoverableHandler(hoverable_handler, hoverable_index, hoverable_handler->action[hoverable_index].data);;
				}

				if (hoverable_handler->action[hoverable_index].phase == ECS_UI_DRAW_NORMAL) {
					ActionData action_data = {
						this,
						dockspace,
						border_index,
						type,
						mouse_position,
						float2(hoverable_handler->position_x[hoverable_index], hoverable_handler->position_y[hoverable_index]),
						float2(hoverable_handler->scale_x[hoverable_index], hoverable_handler->scale_y[hoverable_index]),
						hoverable_handler->action[hoverable_index].data,
						nullptr,
						counts,
						buffers,
						m_mouse_tracker,
						m_keyboard_tracker,
					};
					action_data.mouse = m_mouse;
					action_data.keyboard = m_keyboard;

					m_focused_window_data.additional_hoverable_data = additional_data;
					m_focused_window_data.ExecuteHoverableHandler(&action_data);
				}
				return true;
			}
			else {
				DeallocateHoverableHandler();
				m_focused_window_data.hoverable_handler.action = nullptr;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::DetectClickables(
			size_t* counts,
			void** buffers,
			UIDockspace* dockspace,
			unsigned int border_index,
			DockspaceType type,
			float2 mouse_position,
			unsigned int thread_id,
			unsigned int offset
		) {
			float dockspace_mask = GetDockspaceMaskFromType(type);

			float2 region_position = GetDockspaceRegionPosition(dockspace, border_index, dockspace_mask);
			float2 region_scale = GetDockspaceRegionScale(dockspace, border_index, dockspace_mask);
			//CullHandler(&dockspace->borders[border_index].clickable_handler, region_position.y + m_descriptors.window.title_y_scale, region_position.x, region_position.y + region_scale.y, region_position.x + region_scale.x);
			unsigned int clickable_index = DetectActiveHandler(&dockspace->borders[border_index].clickable_handler, mouse_position, offset);
			if (clickable_index != 0xFFFFFFFF) {
				const UIHandler* clickable_handler = &dockspace->borders[border_index].clickable_handler;

				// make this new handler as the active one
				if (clickable_handler->action[clickable_index].data_size != 0) {
					void* allocation = m_memory->Allocate(clickable_handler->action[clickable_index].data_size, 8);
					memcpy(allocation, clickable_handler->action[clickable_index].data, clickable_handler->action[clickable_index].data_size);
					m_focused_window_data.ChangeClickableHandler(clickable_handler, clickable_index, allocation);
				}
				else {
					m_focused_window_data.ChangeClickableHandler(clickable_handler, clickable_index, clickable_handler->action[clickable_index].data);
				}

				if (clickable_handler->action[clickable_index].phase == ECS_UI_DRAW_NORMAL) {
					ActionData action_data;
					action_data.system = this;
					action_data.dockspace = dockspace;
					action_data.border_index = border_index;
					action_data.buffers = buffers;
					action_data.counts = counts;
					action_data.mouse_tracker = m_mouse_tracker;
					action_data.keyboard_tracker = m_keyboard_tracker;
					action_data.keyboard = m_keyboard;
					action_data.mouse = m_mouse;
					action_data.mouse_position = mouse_position;
					action_data.type = type;

					action_data.additional_data = nullptr;
					m_resources.thread_resources[thread_id].phase = clickable_handler->action[clickable_index].phase;

					if (m_focused_window_data.clickable_handler.action == DefaultClickableAction) {
						action_data.additional_data = m_focused_window_data.additional_hoverable_data;
					}
					m_focused_window_data.ExecuteClickableHandler(&action_data);
				}
				return true;
			}
			else {
				DeallocateClickableHandler();
				m_focused_window_data.clickable_handler.action = nullptr;
				return false;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::DetectGenerals(
			size_t* counts,
			void** buffers,
			UIDockspace* dockspace,
			unsigned int border_index,
			DockspaceType type,
			float2 mouse_position,
			unsigned int thread_id,
			unsigned int offset
		) {
			float dockspace_mask = GetDockspaceMaskFromType(type);

			float2 region_position = GetDockspaceRegionPosition(dockspace, border_index, dockspace_mask);
			float2 region_scale = GetDockspaceRegionScale(dockspace, border_index, dockspace_mask);			
			unsigned int general_index = DetectActiveHandler(&dockspace->borders[border_index].general_handler, mouse_position, offset);
			m_focused_window_data.general_handler.phase = ECS_UI_DRAW_NORMAL;
			
			void* additional_data = nullptr;
			if (m_focused_window_data.clean_up_call_general) {
				if (general_index != 0xFFFFFFFF) {
					UIHandler* handler = &dockspace->borders[border_index].general_handler;
					if (m_focused_window_data.general_handler.action == handler->action[general_index].action) {
						additional_data = handler->action[general_index].data;
					}
				}

				// cleaning up the last action; signaling clean up call by marking the buffers and the counts as nullptr
				HandleFocusedWindowCleanupGeneral(mouse_position, thread_id, additional_data);

				if (additional_data != nullptr) {
					additional_data = m_focused_window_data.general_handler.data;
					if (m_focused_window_data.general_handler.data_size > 0) {
						void* allocation = m_memory->Allocate(m_focused_window_data.general_handler.data_size);
						memcpy(allocation, additional_data, m_focused_window_data.general_handler.data_size);
						m_focused_window_data.additional_general_data = allocation;
						additional_data = allocation;
					}
				}
				m_focused_window_data.clean_up_call_general = false;
			}

			if (general_index != 0xFFFFFFFF) {
				const UIHandler* general_handler = &dockspace->borders[border_index].general_handler;

				if (m_focused_window_data.general_handler.action != nullptr && m_focused_window_data.general_handler.data_size != 0) {
					m_memory->Deallocate(m_focused_window_data.general_handler.data);
				}

				// change the active handler
				if (general_handler->action[general_index].data_size != 0) {
					void* allocation = m_memory->Allocate(general_handler->action[general_index].data_size, 8);
					memcpy(allocation, general_handler->action[general_index].data, general_handler->action[general_index].data_size);
					m_focused_window_data.ChangeGeneralHandler(general_handler, general_index, allocation);
				}
				else {
					m_focused_window_data.ChangeGeneralHandler(general_handler, general_index, general_handler->action[general_index].data);
				}

				if (general_handler->action[general_index].phase == ECS_UI_DRAW_NORMAL) {
					ActionData action_data = {
						this,
						dockspace,
						border_index,
						type,
						mouse_position,
						float2(general_handler->position_x[general_index], general_handler->position_y[general_index]),
						float2(general_handler->scale_x[general_index], general_handler->scale_y[general_index]),
						general_handler->action[general_index].data,
						additional_data,
						counts,
						buffers,
						m_mouse_tracker,
						m_keyboard_tracker
					};
					action_data.mouse = m_mouse;
					action_data.keyboard = m_keyboard;
					m_resources.thread_resources[thread_id].phase = general_handler->action[general_index].phase;

					m_focused_window_data.additional_general_data = additional_data;
					m_focused_window_data.ExecuteGeneralHandler(&action_data);
				}
				return true;
			}
			else {
				DeallocateGeneralHandler();
				m_focused_window_data.general_handler.action = nullptr;
				return false;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::Draw(float2 mouse_position, void** system_buffers, size_t* system_counts)
		{
			PushBackgroundDockspace();

			m_thread_tasks.Reset();
			void* allocation = m_memory->Allocate(sizeof(UIVisibleDockspaceRegion) * m_thread_tasks.GetCapacity(), alignof(UIVisibleDockspaceRegion));
			Stream<UIVisibleDockspaceRegion> regions = Stream<UIVisibleDockspaceRegion>(allocation, 0);
			GetVisibleDockspaceRegions(regions, true);

#ifdef ECS_TOOLS_UI_MULTI_THREADED
			// if there are more regions to draw than command lists, expand the stream and nullptr every entry
			if (regions.size > m_resources.window_draw_list.capacity) {
				void* region_allocation = m_memory->Allocate(sizeof(ComPtr<ID3D11CommandList>) * regions.size, alignof(ComPtr<ID3D11CommandList>));
				m_memory->Deallocate(m_resources.window_draw_list.buffer);
				m_resources.window_draw_list.buffer = (ComPtr<ID3D11CommandList>*)region_allocation;
				m_resources.window_draw_list.capacity = regions.size;
				m_resources.window_draw_list.size = regions.size;
				for (size_t index = 0; index < regions.size; index++) {
					m_resources.window_draw_list[index] = nullptr;
				}
			}
#endif

			UIDrawDockspaceRegionData* data_allocation = (UIDrawDockspaceRegionData*)m_memory->Allocate(sizeof(UIDrawDockspaceRegionData) * regions.size, alignof(UIDrawDockspaceRegionData));
			UIDockspace* mouse_dockspace = nullptr;
			DockspaceType mouse_type;
			unsigned int mouse_dockspace_region = GetDockspaceRegionFromMouse(mouse_position, &mouse_dockspace, mouse_type);

			unsigned int active_region_index = 0;
			for (size_t index = 0; index < regions.size; index++) {
				if (regions[index].dockspace == m_focused_window_data.active_location.dockspace && regions[index].border_index == m_focused_window_data.active_location.border_index) {
					active_region_index = index;
					break;
				}
			}

			auto initialize_thread_task = [=](
				UIDrawDockspaceRegionData* data, 
				UISystem* system, 
				Stream<UIVisibleDockspaceRegion> regions,
				unsigned int index
			) {
				ThreadTask task;

				data->dockspace = regions[index].dockspace;
				data->system = system;
				data->draw_index = index;
				data->border_index = regions[index].border_index;
				data->type = regions[index].type;
				data->last_index = regions.size - 1;
				data->is_fixed_default_when_border_zero = system->IsEmptyFixedDockspace(regions[index].dockspace);
				data->mouse_region = { mouse_dockspace, mouse_dockspace_region, mouse_type };
				data->mouse_position = mouse_position;
				data->system_buffers = system_buffers;
				data->system_count = system_counts;
				data->active_region_index = active_region_index;
				task.data = data;
				task.function = DrawDockspaceRegionThreadTask;

				return task;
			};
			// if there are more windows to draw, add them to the queue
			for (int64_t index = m_task_manager->GetThreadCount(); index < (int64_t)regions.size; index++) {
				ThreadTask task = initialize_thread_task(data_allocation + index, this, regions, index);
				m_thread_tasks.PushNonAtomic(task);
			}

			// initializing threads with tasks
			for (int64_t index = 0; index < m_task_manager->GetThreadCount() && index < (int64_t)regions.size; index++) {
				ThreadTask task = initialize_thread_task(data_allocation + index, this, regions, index);

#ifdef ECS_TOOLS_UI_MULTI_THREADED
				m_task_manager->AddDynamicTask(task, index, true);
#else
#ifdef ECS_TOOLS_UI_SINGLE_THREADED
				task.function(0, m_task_manager->m_world, task.data);
#endif
#endif
			}

#ifdef ECS_TOOLS_UI_SINGLE_THREADED
			// initializing threads with tasks
			size_t thread_task_count = m_thread_tasks.GetSize();
			for (int64_t index = 0; index < thread_task_count; index++) {
				ThreadTask task;
				m_thread_tasks.PopNonAtomic(task);
				task.function(0, m_task_manager->m_world, task.data);
			}
#endif

			// busy wait until drawing the window on that layer is finished
#ifdef ECS_TOOLS_UI_MULTI_THREADED
			int64_t draw_count = 0;
			while (draw_count < (int64_t)regions.size
#ifdef ECS_TOOLS_UI_ACTIVE_WINDOW
				- 1
#endif
				) {
				if (m_resources.window_draw_list[draw_count].Get() != nullptr) {
					m_graphics->GetContext()->ExecuteCommandList(m_resources.window_draw_list[draw_count].Get(), FALSE);
					//unsigned int value = m_resources.window_draw_list[draw_count]->Release();
					m_resources.window_draw_list[draw_count].Reset();
					ID3D11CommandList* yay = m_resources.window_draw_list[draw_count].Get();
					/*if (m_resources.window_draw_list[draw_count].Get() != nullptr) {
						draw_count--;
					}*/
					//ECS_ASSERT(m_resources.window_draw_list[draw_count].Get() == nullptr);
						//draw_count--;
					draw_count++;
				}
				else {
					_mm_pause();
				}
			}

#endif

			m_memory->Deallocate(allocation);
			m_memory->Deallocate(data_allocation);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::FillViewportBuffer(float* buffer, float2 viewport_position, float2 viewport_half_scale)
		{
			uint2 window_size = m_graphics->GetWindowSize();
			buffer[0] = viewport_position.x + viewport_half_scale.x;
			buffer[1] = viewport_position.y + viewport_half_scale.y;
			buffer[2] = 1.0f / viewport_half_scale.x;
			buffer[3] = 1.0f / viewport_half_scale.y;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::FillViewportBuffer(float2 viewport_position, float2 viewport_scale, float* buffer)
		{
			float2 half_scale = { viewport_scale.x * 0.5f, viewport_scale.y * 0.5f };

			FillViewportBuffer(buffer, viewport_position, half_scale);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::DoFrame() {
			m_texture_evict_count++;
			if (m_texture_evict_count == m_texture_evict_target) {
				m_resource_manager->DecrementReferenceCount(ResourceType::Texture, m_texture_evict_count);
				// Evict the outdated resources aswell
				EvictOutdatedTextures();
				m_texture_evict_count = 0;
			}

			UpdateDockspaceHierarchy();

			m_mouse_tracker = m_mouse->GetTracker();
			m_keyboard_tracker = m_keyboard->GetTracker();

#ifdef ECS_TOOLS_UI_SINGLE_THREADED
			m_graphics->DisableDepth();
#endif
			// 2 times for hoverable and clickable/general system phase
			void* buffers[ECS_TOOLS_UI_MATERIALS * 2];
			size_t counts[ECS_TOOLS_UI_MATERIALS * 2] = { 0 };

			m_resources.system_draw.Map(buffers, m_graphics->GetContext());
			m_resources.system_draw.sprite_textures[0].Reset();
			m_frame_pacing = ECS_UI_FRAME_PACING_NONE;

			float2 mouse_position = GetNormalizeMousePosition();

			m_execute_events = DetectEvents(mouse_position);

#ifdef ECS_TOOLS_UI_MULTI_THREADED
			m_task_manager->ResetDynamicQueues();
#endif

			m_resources.thread_resources[0].system_temp_allocator.Clear();

#if defined(ECS_TOOLS_UI_SINGLE_THREADED) || defined(ECS_TOOLS_UI_MULTI_THREADED)
			Draw(mouse_position, buffers, counts);
#else
			DrawFrame(mouse_position, resize_factor);
#endif

			m_focused_window_data.buffers = buffers;
			m_focused_window_data.counts = counts;

			if (m_focused_window_data.hoverable_handler.phase == ECS_UI_DRAW_SYSTEM) {
				auto phase_copy = m_resources.thread_resources[0].phase;
				HandleHoverable(mouse_position, 0, buffers, counts);
				m_resources.thread_resources[0].phase = phase_copy;
				m_frame_pacing = m_frame_pacing < ECS_UI_FRAME_PACING_LOW ? ECS_UI_FRAME_PACING_LOW : m_frame_pacing;
			}
			if (m_focused_window_data.clickable_handler.phase == ECS_UI_DRAW_SYSTEM) {
				if (m_mouse_tracker->LeftButton() == MBHELD || m_mouse_tracker->LeftButton() == MBPRESSED) {
					auto phase_copy = m_resources.thread_resources[0].phase;
					HandleFocusedWindowClickable(mouse_position, 0);
					m_resources.thread_resources[0].phase = phase_copy;
				}
				else if (m_mouse_tracker->LeftButton() == MBRELEASED) {
					auto phase_copy = m_resources.thread_resources[0].phase;
					HandleFocusedWindowClickable(mouse_position, 0);
					if (m_focused_window_data.clickable_handler.action != nullptr) {
						if (m_focused_window_data.clickable_handler.data_size != 0) {
							m_memory->Deallocate(m_focused_window_data.clickable_handler.data);
						}
						m_focused_window_data.clickable_handler.action = nullptr;
					}
					m_focused_window_data.clickable_handler.phase = ECS_UI_DRAW_NORMAL;
					m_resources.thread_resources[0].phase = phase_copy;
				}
				m_frame_pacing = m_frame_pacing < ECS_UI_FRAME_PACING_MEDIUM ? ECS_UI_FRAME_PACING_MEDIUM : m_frame_pacing;
			}
			if (m_focused_window_data.general_handler.phase == ECS_UI_DRAW_SYSTEM) {
				auto phase_copy = m_resources.thread_resources[0].phase;
				HandleFocusedWindowGeneral(mouse_position, 0);
				m_resources.thread_resources[0].phase = phase_copy;
				m_frame_pacing = m_frame_pacing < ECS_UI_FRAME_PACING_MEDIUM ? ECS_UI_FRAME_PACING_MEDIUM : m_frame_pacing;
			}

			if (m_focused_window_data.additional_general_data != nullptr && m_focused_window_data.general_handler.data_size > 0) {
				m_memory->Deallocate(m_focused_window_data.additional_general_data);
			}
			m_focused_window_data.additional_general_data = nullptr;
			if (m_focused_window_data.additional_hoverable_data != nullptr && m_focused_window_data.hoverable_handler.data_size > 0) {
				m_memory->Deallocate(m_focused_window_data.additional_hoverable_data);
			}
			m_focused_window_data.additional_hoverable_data = nullptr;

			if (m_execute_events) {
				m_event(
					this,
					m_event_data,
					buffers,
					counts,
					mouse_position.x,
					mouse_position.y,
					m_mouse->GetState(),
					m_mouse_tracker,
					m_keyboard->GetState(),
					m_keyboard_tracker
				);
			}

			m_resources.system_draw.UnmapAll(m_graphics->GetContext());
			SetViewport(
				{ -1.0f, -1.0f },
				{ 2.0f, 2.0f },
				m_graphics->GetContext()
			);
			DrawPass<ECS_UI_DRAW_SYSTEM>(
				m_resources.system_draw,
				counts,
				{ -1.0f, -1.0f },
				{ 2.0f, 2.0f },
				m_graphics->GetContext()
			);

#ifdef ECS_TOOLS_UI_SINGLE_THREADED
			m_graphics->EnableDepth();
#endif

			for (size_t index = 0; index < m_windows.size; index++) {
				DecrementWindowDynamicResource(index);
			}

			SetSystemDrawRegion({ {-1.0f, -1.0f}, {2.0f, 2.0f} });
			HandleFrameHandlers();
			UpdateFocusedWindowCleanupLocation();

			m_previous_mouse_position = mouse_position;
			m_frame_index++;

			return m_frame_pacing;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DrawCollapseTriangleHeader(
			void** buffers,
			size_t* vertex_count,
			UIDockspace* dockspace,
			unsigned int border_index,
			float mask,
			bool is_open,
			ECS_UI_DRAW_PHASE phase
		) {
			float2 position;
			float2 scale;
			GetDockspaceRegionCollapseTriangleTransform(dockspace, border_index, mask, position, scale);
			float2 expanded_scale;
			position = ExpandRectangle(position, scale, { 1.3f, 1.3f }, expanded_scale);

			if (is_open) {			
				SetSprite(
					dockspace,
					border_index,
					ECS_TOOLS_UI_TEXTURE_TRIANGLE,
					position,
					expanded_scale,
					buffers,
					vertex_count,
					m_descriptors.color_theme.collapse_triangle,
					{0.0f, 0.0f},
					{1.0f, 1.0f},
					phase
				);
			}
			else {			
				SetSprite(
					dockspace,
					border_index,
					ECS_TOOLS_UI_TEXTURE_TRIANGLE,
					position,
					expanded_scale,
					buffers,
					vertex_count,
					m_descriptors.color_theme.collapse_triangle,
					{1.0f, 0.0f},
					{0.0f, 1.0f},
					phase
				);
			}

		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		// it will call the transform variant which is slightly more expensive in order to have propragation of changes accross 
		// functions
		void UISystem::DrawDockingGizmo(float2 position, size_t* counts, void** buffers, bool draw_central_rectangle)
		{
			float2 transforms_useless[10];
			DrawDockingGizmo(position, counts, buffers, draw_central_rectangle, transforms_useless);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DrawDockingGizmo(
			float2 position,
			size_t* counts, 
			void** buffers, 
			bool draw_central_rectangle,
			float2* transforms
		)
		{
			float2 central_rectangle_scale = ECS_TOOLS_UI_DOCKING_GIZMO_CENTRAL_SCALE;
			float2 half_scale = { central_rectangle_scale.x * 0.5f, central_rectangle_scale.y * 0.5f };
			float2 scale = { central_rectangle_scale.x, central_rectangle_scale.y };

			float gizmo_line_width = ECS_TOOLS_UI_DOCKING_GIZMO_LINE_WIDTH;
			float normalized_values[] = { gizmo_line_width, gizmo_line_width };
			NormalizeHorizontalToWindowDimensions(normalized_values, 2);
			float normalized_width = normalized_values[0];
			float normalized_spacing = normalized_values[1];
			float2 central_rectangle_position = { position.x - half_scale.x, position.y - half_scale.y };

			float2 central_rectangle_spacing = ECS_TOOLS_UI_DOCKING_GIZMO_CENTRAL_SPACING;
			float2 left_rectangle_position = {
				central_rectangle_position.x - central_rectangle_scale.x - central_rectangle_spacing.x,
				central_rectangle_position.y
			};
			float2 top_rectangle_position = {
				central_rectangle_position.x,
				central_rectangle_position.y - central_rectangle_scale.y - central_rectangle_spacing.y
			};
			float2 right_rectangle_position = {
				central_rectangle_position.x + central_rectangle_scale.x + central_rectangle_spacing.x,
				central_rectangle_position.y
			};
			float2 bottom_rectangle_position = {
				central_rectangle_position.x,
				central_rectangle_position.y + central_rectangle_scale.y + central_rectangle_spacing.y
			};

			transforms[0] = left_rectangle_position;
			transforms[1] = scale;
			transforms[2] = top_rectangle_position;
			transforms[3] = scale;
			transforms[4] = right_rectangle_position;
			transforms[5] = scale;
			transforms[6] = bottom_rectangle_position;
			transforms[7] = scale;
			transforms[8] = central_rectangle_position;
			transforms[9] = scale;

			UIVertexColor* solid_color = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
			if (draw_central_rectangle)
				SetSolidColorRectangle(central_rectangle_position, scale, m_descriptors.color_theme.docking_gizmo_background, solid_color, counts[ECS_TOOLS_UI_SOLID_COLOR]);

			SetSolidColorRectangle(left_rectangle_position, scale, m_descriptors.color_theme.docking_gizmo_background, solid_color, counts[ECS_TOOLS_UI_SOLID_COLOR]);
			SetSolidColorRectangle(right_rectangle_position, scale, m_descriptors.color_theme.docking_gizmo_background, solid_color, counts[ECS_TOOLS_UI_SOLID_COLOR]);
			SetSolidColorRectangle(bottom_rectangle_position, scale, m_descriptors.color_theme.docking_gizmo_background, solid_color, counts[ECS_TOOLS_UI_SOLID_COLOR]);
			SetSolidColorRectangle(top_rectangle_position, scale, m_descriptors.color_theme.docking_gizmo_background, solid_color, counts[ECS_TOOLS_UI_SOLID_COLOR]);

			float2 border_scale = ECS_TOOLS_UI_DOCKING_GIZMO_BORDER_SCALE;
			if (draw_central_rectangle)
				CreateSolidColorRectangleBorder<false>(central_rectangle_position, scale, border_scale, m_descriptors.color_theme.docking_gizmo_border, counts, buffers);

			CreateSolidColorRectangleBorder<false>(left_rectangle_position, scale, border_scale, m_descriptors.color_theme.docking_gizmo_border, counts, buffers);
			CreateSolidColorRectangleBorder<false>(right_rectangle_position, scale, border_scale, m_descriptors.color_theme.docking_gizmo_border, counts, buffers);
			CreateSolidColorRectangleBorder<false>(bottom_rectangle_position, scale, border_scale, m_descriptors.color_theme.docking_gizmo_border, counts, buffers);
			CreateSolidColorRectangleBorder<false>(top_rectangle_position, scale, border_scale, m_descriptors.color_theme.docking_gizmo_border, counts, buffers);

			// left
			CreateDottedLine<false>(
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR],
				ECS_TOOLS_UI_DOCKING_GIZMO_LINE_COUNT,
				{ left_rectangle_position.x + half_scale.x - gizmo_line_width * 0.5f, left_rectangle_position.y },
				left_rectangle_position.y + scale.y,
				ECS_TOOLS_UI_DOCKING_GIZMO_SPACING,
				normalized_width,
				m_descriptors.color_theme.docking_gizmo_border
			);
			counts[ECS_TOOLS_UI_SOLID_COLOR] += ECS_TOOLS_UI_DOCKING_GIZMO_LINE_COUNT * 6;

			// top
			CreateDottedLine<true>(
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR],
				ECS_TOOLS_UI_DOCKING_GIZMO_LINE_COUNT,
				{ top_rectangle_position.x, top_rectangle_position.y + half_scale.y - normalized_width * 0.5f },
				top_rectangle_position.x + scale.x,
				normalized_spacing,
				gizmo_line_width,
				m_descriptors.color_theme.docking_gizmo_border
			);
			counts[ECS_TOOLS_UI_SOLID_COLOR] += ECS_TOOLS_UI_DOCKING_GIZMO_LINE_COUNT * 6;

			// right
			CreateDottedLine<false>(
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR],
				ECS_TOOLS_UI_DOCKING_GIZMO_LINE_COUNT,
				{ right_rectangle_position.x + half_scale.x - gizmo_line_width * 0.5f, right_rectangle_position.y },
				right_rectangle_position.y + scale.y,
				ECS_TOOLS_UI_DOCKING_GIZMO_SPACING,
				normalized_width,
				m_descriptors.color_theme.docking_gizmo_border
			);
			counts[ECS_TOOLS_UI_SOLID_COLOR] += ECS_TOOLS_UI_DOCKING_GIZMO_LINE_COUNT * 6;

			// bottom
			CreateDottedLine<true>(
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR],
				ECS_TOOLS_UI_DOCKING_GIZMO_LINE_COUNT,
				{ bottom_rectangle_position.x, bottom_rectangle_position.y + half_scale.y - normalized_width * 0.5f },
				bottom_rectangle_position.x + scale.x,
				normalized_spacing,
				gizmo_line_width,
				m_descriptors.color_theme.docking_gizmo_border
			);
			counts[ECS_TOOLS_UI_SOLID_COLOR] += ECS_TOOLS_UI_DOCKING_GIZMO_LINE_COUNT * 6;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DrawDockspaceRegion(const UIDrawDockspaceRegionData* data) {
			unsigned int window_index_in_border = data->dockspace->borders[data->border_index].active_window;
			unsigned int window_index = 0xFFFFFFFF;

			float dockspace_mask = GetDockspaceMaskFromType(data->type);
			float2 region_position = GetDockspaceRegionPosition(data->dockspace, data->border_index, dockspace_mask);
			float2 region_scale = GetDockspaceRegionScale(data->dockspace, data->border_index, dockspace_mask);
			if (data->is_fixed_default_when_border_zero) {
				region_position = data->dockspace->transform.position;
				region_scale = data->dockspace->transform.scale;
			}
			else {
				window_index = data->dockspace->borders[data->border_index].window_indices[window_index_in_border];
			}

			float2 region_half_scale = { region_scale.x * 0.5f, region_scale.y * 0.5f };

#ifdef ECS_TOOLS_UI_MULTI_THREADED
			m_graphics->BindRenderTargetViewFromInitialViews(m_resources.thread_resources[data->thread_id].deferred_context.Get());
			m_graphics->DisableDepth(m_resources.thread_resources[data->thread_id].deferred_context.Get());

#endif

			size_t vertex_count[ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES] = { 0 };
			void* buffers[ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES];

			data->dockspace->borders[data->border_index].draw_resources.Map(
				buffers,
#ifdef ECS_TOOLS_UI_MULTI_THREADED
				m_resources.thread_resources[data->thread_id].deferred_context.Get()
#else 
				m_graphics->GetContext()
#endif
			);

			m_resources.thread_resources[data->thread_id].phase = ECS_UI_DRAW_NORMAL;

			if (!data->is_fixed_default_when_border_zero) {
				data->dockspace->borders[data->border_index].Reset();

				DrawDockspaceRegionBackground(
					data->thread_id,
					data->dockspace,
					data->border_index,
					dockspace_mask,
					buffers,
					vertex_count
				);
			}
			else {
				DrawFixedDockspaceRegionBackground(
					data->dockspace,
					data->border_index,
					data->dockspace->transform.position,
					data->dockspace->transform.scale,
					buffers,
					vertex_count
				);
			}

			if (window_index != 0xFFFFFFFF) {
				UIDrawerDescriptor drawer_descriptor = InitializeDrawerDescriptorReferences(window_index);
				drawer_descriptor.border_index = data->border_index;
				drawer_descriptor.dockspace = data->dockspace;
				drawer_descriptor.buffers = buffers;
				drawer_descriptor.counts = vertex_count;
				drawer_descriptor.dockspace_type = data->type;
				drawer_descriptor.system = this;
				drawer_descriptor.thread_id = data->thread_id;
				drawer_descriptor.window_index = window_index;
				drawer_descriptor.system_buffers = data->system_buffers;
				drawer_descriptor.system_counts = data->system_count;
				drawer_descriptor.mouse_position = data->mouse_position;
				drawer_descriptor.export_scale = nullptr;
				drawer_descriptor.do_not_initialize_viewport_sliders = false;
				drawer_descriptor.do_not_allocate_buffers = false;
				if (m_windows[drawer_descriptor.window_index].draw != nullptr)
					m_windows[drawer_descriptor.window_index].draw(m_windows[drawer_descriptor.window_index].window_data, &drawer_descriptor, false);

				DrawDockspaceRegionHeader(
					data->thread_id,
					data->dockspace,
					data->border_index,
					dockspace_mask,
					buffers + ECS_TOOLS_UI_MATERIALS,
					vertex_count + ECS_TOOLS_UI_MATERIALS
				);
			}

			DrawDockspaceRegionBorders(
				region_position,
				region_scale,
				buffers + ECS_TOOLS_UI_MATERIALS,
				vertex_count + ECS_TOOLS_UI_MATERIALS
			);

			bool is_hoverable = false;
			bool is_clicked = false;
			bool is_general = false;
			if (data->mouse_region.dockspace == data->dockspace && data->mouse_region.border_index == data->border_index && !m_execute_events && m_focused_window_data.locked_window == 0) {
				if ((m_focused_window_data.clickable_handler.action == nullptr || m_mouse_tracker->LeftButton() != MBHELD) || (m_focused_window_data.always_hoverable)
					&& data->dockspace->borders[data->border_index].hoverable_handler.position_x.buffer != nullptr) {

					is_hoverable = DetectHoverables(
						vertex_count,
						buffers,
						data->dockspace,
						data->border_index,
						data->type,
						data->mouse_position,
						0
					);
					m_frame_pacing = (is_hoverable && m_frame_pacing < ECS_UI_FRAME_PACING_LOW) ? ECS_UI_FRAME_PACING_LOW : m_frame_pacing;
				}

				if (m_mouse_tracker->LeftButton() == MBPRESSED) {
					DockspaceType floating_type;
					UIDockspace* floating_dockspace = GetFloatingDockspaceFromDockspace(
						data->dockspace,
						dockspace_mask,
						floating_type
					);			
					SetNewFocusedDockspace(floating_dockspace, floating_type);
					SetNewFocusedDockspaceRegion(data->dockspace, data->border_index, data->type);
					is_clicked = DetectClickables(
						vertex_count,
						buffers,
						data->dockspace,
						data->border_index,
						data->type,
						data->mouse_position,
						data->thread_id,
						0
					);
					is_general = DetectGenerals(
						vertex_count,
						buffers,
						data->dockspace,
						data->border_index,
						data->type,
						data->mouse_position,
						data->thread_id,
						0
					);
					m_frame_pacing = (is_clicked || is_general && m_frame_pacing < ECS_UI_FRAME_PACING_MEDIUM) ? ECS_UI_FRAME_PACING_MEDIUM : m_frame_pacing;
				}
			}

			// The spin wait for the texture resizing and compression must be done here - because the 
			// threads use the immediate context
			unsigned int semaphore_count = m_resources.texture_semaphore.count;
			m_resources.texture_semaphore.SpinWait();

			data->dockspace->borders[data->border_index].draw_resources.UnmapNormal(
#ifdef ECS_TOOLS_UI_MULTI_THREADED
				m_resources.thread_resources[data->thread_id].deferred_context.Get()
#else 
				m_graphics->GetContext()
#endif
			);

			SetViewport(
#ifdef ECS_TOOLS_UI_MULTI_THREADED
				m_resources.thread_resources[data->thread_id].deferred_context.Get(),
#else
				m_graphics->GetContext(),
#endif
				region_position,
				region_half_scale
			);

			m_resources.thread_resources[data->thread_id].phase = ECS_UI_DRAW_NORMAL;
			DrawPass<ECS_UI_DRAW_NORMAL>(
				data->dockspace->borders[data->border_index].draw_resources,
				vertex_count,
				{ region_position.x, region_position.y },
				{ region_scale.x, region_scale.y },

#ifdef ECS_TOOLS_UI_MULTI_THREADED
				m_resources.thread_resources[data->thread_id].deferred_context.Get()
#else 
				m_graphics->GetContext()
#endif
			);

			if (is_hoverable && m_focused_window_data.hoverable_handler.phase == ECS_UI_DRAW_LATE) {
				HandleHoverable(data->mouse_position, data->thread_id, buffers + ECS_TOOLS_UI_MATERIALS, vertex_count + ECS_TOOLS_UI_MATERIALS);
			}
			if (is_clicked && m_focused_window_data.clickable_handler.phase == ECS_UI_DRAW_LATE) {
				HandleFocusedWindowClickable(data->mouse_position, data->thread_id);
			}
			if (is_general && m_focused_window_data.general_handler.phase == ECS_UI_DRAW_LATE) {
				HandleFocusedWindowGeneral(data->mouse_position, data->thread_id);
			}

			data->dockspace->borders[data->border_index].draw_resources.UnmapLate(
#ifdef ECS_TOOLS_UI_MULTI_THREADED
				m_resources.thread_resources[data->thread_id].deferred_context.Get()
#else 
				m_graphics->GetContext()
#endif
			);

			
			SetViewport(
#ifdef ECS_TOOLS_UI_MULTI_THREADED
				m_resources.thread_resources[data->thread_id].deferred_context.Get(),
#else
				m_graphics->GetContext(),
#endif
				region_position,
				region_half_scale
			);

			m_resources.thread_resources[data->thread_id].phase = ECS_UI_DRAW_LATE;
			DrawPass<ECS_UI_DRAW_LATE>(
				data->dockspace->borders[data->border_index].draw_resources,
				vertex_count,
				region_position,
				{ region_scale.x, region_scale.y },

#ifdef ECS_TOOLS_UI_MULTI_THREADED
				m_resources.thread_resources[data->thread_id].deferred_context.Get()
#else 
				m_graphics->GetContext()
#endif
			);

			if (data->mouse_region.dockspace == data->dockspace && data->mouse_region.border_index == data->border_index && window_index != 0xFFFFFFFF
				 && m_focused_window_data.locked_window == 0) {
				ActionData window_handler;
				window_handler.border_index = data->border_index;
				window_handler.buffers = data->system_buffers;
				window_handler.counts = data->system_count;
				window_handler.data = m_windows[window_index].default_handler.data;
				window_handler.dockspace = data->dockspace;
				window_handler.keyboard = m_keyboard;
				window_handler.keyboard_tracker = m_keyboard_tracker;
				window_handler.mouse = m_mouse;
				window_handler.mouse_position = data->mouse_position;
				window_handler.mouse_tracker = m_mouse_tracker;
				window_handler.position = region_position;
				window_handler.scale = region_scale;
				window_handler.system = this;
				window_handler.type = data->type;
				m_windows[window_index].default_handler.action(&window_handler);
			}

		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DrawDockspaceRegionActive(const UIDrawDockspaceRegionData* data) {
			unsigned int window_index_in_border = data->dockspace->borders[data->border_index].active_window;
			unsigned int window_index = 0xFFFFFFFF;

			float dockspace_mask = GetDockspaceMaskFromType(data->type);

			float2 region_position = GetDockspaceRegionPosition(data->dockspace, data->border_index, dockspace_mask);
			float2 region_scale = GetDockspaceRegionScale(data->dockspace, data->border_index, dockspace_mask);

			float2 region_copy = region_position;

			if (data->is_fixed_default_when_border_zero) {
				region_position = data->dockspace->transform.position;
				region_scale = data->dockspace->transform.scale;
			}
			else {
				window_index = data->dockspace->borders[data->border_index].window_indices[window_index_in_border];
			}

			float2 region_half_scale = { region_scale.x * 0.5f, region_scale.y * 0.5f };

			size_t vertex_count[ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES] = { 0 };
			void* buffers[ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES];

			data->dockspace->borders[data->border_index].draw_resources.Map(buffers, m_graphics->GetContext());
			m_resources.thread_resources[data->thread_id].phase = ECS_UI_DRAW_NORMAL;

			if (!data->is_fixed_default_when_border_zero) {
				data->dockspace->borders[data->border_index].Reset();

				DrawDockspaceRegionBackground(
					data->thread_id,
					data->dockspace,
					data->border_index,
					dockspace_mask,
					buffers,
					vertex_count
				);
			}
			else {
				DrawFixedDockspaceRegionBackground(
					data->dockspace,
					data->border_index,
					data->dockspace->transform.position,
					data->dockspace->transform.scale,
					buffers,
					vertex_count
				);
			}

			if (window_index != 0xFFFFFFFF) {
				UIDrawerDescriptor drawer_descriptor = InitializeDrawerDescriptorReferences(window_index);
				drawer_descriptor.border_index = data->border_index;
				drawer_descriptor.dockspace = data->dockspace;
				drawer_descriptor.buffers = buffers;
				drawer_descriptor.counts = vertex_count;
				drawer_descriptor.dockspace_type = data->type;
				drawer_descriptor.system = this;
				drawer_descriptor.thread_id = data->thread_id;
				drawer_descriptor.window_index = window_index;
				drawer_descriptor.system_buffers = data->system_buffers;
				drawer_descriptor.system_counts = data->system_count;
				drawer_descriptor.mouse_position = data->mouse_position;
				drawer_descriptor.export_scale = nullptr;
				drawer_descriptor.do_not_initialize_viewport_sliders = false;
				drawer_descriptor.do_not_allocate_buffers = false;
				if (m_windows[drawer_descriptor.window_index].draw != nullptr)
					m_windows[drawer_descriptor.window_index].draw(m_windows[drawer_descriptor.window_index].window_data, &drawer_descriptor, false);
				
				DrawDockspaceRegionHeader(
					data->thread_id,
					data->dockspace,
					data->border_index,
					dockspace_mask,
					buffers + ECS_TOOLS_UI_MATERIALS,
					vertex_count + ECS_TOOLS_UI_MATERIALS
				);
			}

			DrawDockspaceRegionBorders(
				region_position,
				region_scale,
				buffers + ECS_TOOLS_UI_MATERIALS,
				vertex_count + ECS_TOOLS_UI_MATERIALS
			);

			bool is_hoverable = false;
			if (!m_execute_events && data->mouse_region.dockspace == data->dockspace && data->mouse_region.border_index == data->border_index) {
				if ((m_focused_window_data.clickable_handler.action == nullptr || m_mouse_tracker->LeftButton() != MBHELD) || (m_focused_window_data.always_hoverable)
					&& data->dockspace->borders[data->border_index].hoverable_handler.position_x.buffer != nullptr) {
					is_hoverable = DetectHoverables(
						vertex_count,
						buffers,
						data->dockspace,
						data->border_index,
						data->type,
						data->mouse_position,
						0
					);
					m_frame_pacing = (is_hoverable && m_frame_pacing < ECS_UI_FRAME_PACING_LOW) ? ECS_UI_FRAME_PACING_LOW : m_frame_pacing;
				}
				if (m_mouse_tracker->LeftButton() == MBPRESSED) {
					DockspaceType floating_type;
					UIDockspace* floating_dockspace = GetFloatingDockspaceFromDockspace(
						data->dockspace,
						dockspace_mask,
						floating_type
					);
					bool is_clickable = DetectClickables(
						vertex_count,
						buffers,
						data->dockspace,
						data->border_index,
						data->type,
						data->mouse_position,
						data->thread_id,
						0
					);
					bool is_general = DetectGenerals(
						vertex_count,
						buffers,
						data->dockspace,
						data->border_index,
						data->type,
						data->mouse_position,
						data->thread_id,
						0
					);
					m_frame_pacing = (is_clickable && is_general && m_frame_pacing < ECS_UI_FRAME_PACING_MEDIUM) ? ECS_UI_FRAME_PACING_MEDIUM : m_frame_pacing;
				}
			}

			m_focused_window_data.buffers = buffers;
			m_focused_window_data.counts = vertex_count;
			if (m_focused_window_data.clickable_handler.phase == ECS_UI_DRAW_NORMAL) {
				if (m_mouse_tracker->LeftButton() == MBHELD) {
					HandleFocusedWindowClickable(data->mouse_position, data->thread_id);
				}
				else if (m_mouse_tracker->LeftButton() == MBRELEASED) {
					HandleFocusedWindowClickable(data->mouse_position, data->thread_id);
					if (m_focused_window_data.clickable_handler.action != nullptr) {
						if (m_focused_window_data.clickable_handler.data_size != 0) {
							m_memory->Deallocate(m_focused_window_data.clickable_handler.data);
						}
						m_focused_window_data.clickable_handler.action = nullptr;
					}
					m_focused_window_data.clickable_handler.phase = ECS_UI_DRAW_NORMAL;
				}
			}
			if (m_focused_window_data.general_handler.phase == ECS_UI_DRAW_NORMAL && m_mouse_tracker->LeftButton() != MBPRESSED) {
				HandleFocusedWindowGeneral(data->mouse_position, data->thread_id);
			}

			// The spin wait for the texture resizing and compression must be done here - because the 
			// threads use the immediate context
			unsigned int semaphore_count = m_resources.texture_semaphore.count;
			m_resources.texture_semaphore.SpinWait();

			data->dockspace->borders[data->border_index].draw_resources.UnmapNormal(m_graphics->GetContext());

			SetViewport(
				m_graphics->GetContext(),
				region_position,
				region_half_scale
			);

			// viewport buffer description: 
			// float2 region_center_position;
			// float2 region_half_dimensions_inverse;
			DrawPass<ECS_UI_DRAW_NORMAL>(
				data->dockspace->borders[data->border_index].draw_resources,
				vertex_count,
				{ region_position.x, region_position.y },
				{ region_scale.x, region_scale.y },
				m_graphics->GetContext()
			);

			m_focused_window_data.buffers = buffers + ECS_TOOLS_UI_MATERIALS;
			m_focused_window_data.counts = vertex_count + ECS_TOOLS_UI_MATERIALS;
			if (is_hoverable && m_focused_window_data.hoverable_handler.phase == ECS_UI_DRAW_LATE) {
				HandleHoverable(data->mouse_position, data->thread_id, m_focused_window_data.buffers, m_focused_window_data.counts);
			}
			if (m_focused_window_data.clickable_handler.phase == ECS_UI_DRAW_LATE) {
				if (m_mouse_tracker->LeftButton() == MBHELD || m_mouse_tracker->LeftButton() == MBPRESSED) {
					HandleFocusedWindowClickable(data->mouse_position, data->thread_id);
				}
				else if (m_mouse_tracker->LeftButton() == MBRELEASED) {
					HandleFocusedWindowClickable(data->mouse_position, data->thread_id);
					if (m_focused_window_data.clickable_handler.action != nullptr) {
						if (m_focused_window_data.clickable_handler.data_size != 0) {
							m_memory->Deallocate(m_focused_window_data.clickable_handler.data);
						}
						m_focused_window_data.clickable_handler.action = nullptr;
						m_focused_window_data.clickable_handler.phase = ECS_UI_DRAW_NORMAL;
					}
				}
			}
			if (m_focused_window_data.general_handler.phase == ECS_UI_DRAW_LATE) {
				HandleFocusedWindowGeneral(data->mouse_position, data->thread_id);
			}

			data->dockspace->borders[data->border_index].draw_resources.UnmapLate(m_graphics->GetContext());

			SetViewport(
				m_graphics->GetContext(),
				region_position,
				region_half_scale
			);

			DrawPass<ECS_UI_DRAW_LATE>(
				data->dockspace->borders[data->border_index].draw_resources,
				vertex_count,
				region_position,
				{ region_scale.x, region_scale.y },
				m_graphics->GetContext()
			);

			if (window_index != 0xFFFFFFFF) {
				ActionData window_handler;
				window_handler.border_index = data->border_index;
				window_handler.buffers = data->system_buffers;
				window_handler.counts = data->system_count;
				window_handler.data = m_windows[window_index].default_handler.data;
				window_handler.dockspace = data->dockspace;
				window_handler.keyboard = m_keyboard;
				window_handler.keyboard_tracker = m_keyboard_tracker;
				window_handler.mouse = m_mouse;
				window_handler.mouse_position = data->mouse_position;
				window_handler.mouse_tracker = m_mouse_tracker;
				window_handler.position = region_position;
				window_handler.scale = region_scale;
				window_handler.system = this;
				window_handler.type = data->type;
				m_windows[window_index].default_handler.action(&window_handler);
			}

		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DrawDockspaceRegionHeader(
			unsigned int thread_id,
			UIDockspace* dockspace,
			unsigned int border_index,
			float offset_mask,
			void** buffers,
			size_t* vertex_count
		) {
			UIVertexColor* solid_color = (UIVertexColor*)(buffers[ECS_TOOLS_UI_SOLID_COLOR]);
			UISpriteVertex* text_sprites = (UISpriteVertex*)(buffers[ECS_TOOLS_UI_TEXT_SPRITE]);
			UISpriteVertex* sprites = (UISpriteVertex*)(buffers[ECS_TOOLS_UI_SPRITE]);

			float2 dockspace_region_position = GetDockspaceRegionPosition(dockspace, border_index, offset_mask);
			float2 dockspace_region_scale = GetDockspaceRegionScale(dockspace, border_index, offset_mask);

			float2 collapse_triangle_hover_box_position;
			float2 collapse_triangle_hover_box_scale;
			GetDockspaceRegionCollapseTriangleTransform(dockspace, border_index, offset_mask, collapse_triangle_hover_box_position, collapse_triangle_hover_box_scale);
			dockspace_region_position.y += m_descriptors.dockspaces.border_size;

			float2 expanded_collapse_triangle_scale;
			collapse_triangle_hover_box_position = ExpandRectangle(
				collapse_triangle_hover_box_position,
				collapse_triangle_hover_box_scale,
				float2(1.0f, 1.0f),
				expanded_collapse_triangle_scale
			);
			UIDefaultHoverableData collapse_triangle_hoverable_data;
			collapse_triangle_hoverable_data.colors[0] = m_descriptors.color_theme.region_header;
			collapse_triangle_hoverable_data.percentages[0] = 1.4f;
			UISystemDefaultClickableData collapse_triangle_data;
			collapse_triangle_data.border_index = border_index;
			collapse_triangle_data.clickable_handler.action = CollapseTriangleClickableAction;
			collapse_triangle_data.clickable_handler.data = nullptr;
			collapse_triangle_data.clickable_handler.data_size = 0;
			collapse_triangle_data.dockspace = dockspace;
			collapse_triangle_data.position = collapse_triangle_hover_box_position;
			collapse_triangle_data.scale = expanded_collapse_triangle_scale;
			collapse_triangle_data.thread_id = thread_id;
			collapse_triangle_data.hoverable_handler.action = DefaultHoverableAction;
			collapse_triangle_data.hoverable_handler.data_size = sizeof(collapse_triangle_hoverable_data);
			collapse_triangle_data.hoverable_handler.data = &collapse_triangle_hoverable_data;
			collapse_triangle_data.hoverable_handler.phase = ECS_UI_DRAW_LATE;

			float2 close_x_scale = float2(
				m_descriptors.dockspaces.close_x_position_x_left - m_descriptors.dockspaces.close_x_position_x_right,
				m_descriptors.dockspaces.close_x_scale_y
			);
			float2 expanded_close_x_scale = { close_x_scale.x * 1.35f, close_x_scale.y * 1.15f };
			unsigned int x_sprite_offset = FindCharacterType('X');

			if (dockspace->borders[border_index].draw_region_header) {
				bool is_focused_dockspace = m_focused_window_data.active_location.dockspace == dockspace;
				bool is_focused_border = m_focused_window_data.active_location.border_index == border_index;
				bool is_focused_region = is_focused_dockspace && is_focused_border;
				Color title_color = m_descriptors.color_theme.inactive_title;
				if (is_focused_region) {
					title_color = m_descriptors.color_theme.title;
				}

				SetSolidColorRectangle(
					{dockspace_region_position.x, dockspace_region_position.y},
					float2(dockspace_region_scale.x, m_descriptors.misc.title_y_scale - m_descriptors.dockspaces.border_size),
					title_color,
					solid_color,
					vertex_count[ECS_TOOLS_UI_SOLID_COLOR]
				);
				AddHoverableToDockspaceRegion(
					thread_id,
					dockspace, 
					border_index, 
					dockspace_region_position, 
					{ dockspace_region_scale.x, m_descriptors.misc.title_y_scale - m_descriptors.dockspaces.border_size }, 
					{ SkipAction, nullptr, 0 }
				);
				AddClickableToDockspaceRegion(
					thread_id,
					dockspace,
					border_index,
					dockspace_region_position,
					{ dockspace_region_scale.x, m_descriptors.misc.title_y_scale - m_descriptors.dockspaces.border_size },
					{ SkipAction, nullptr, 0 }
				);
				AddGeneralActionToDockspaceRegion(
					thread_id,
					dockspace,
					border_index,
					dockspace_region_position,
					{ dockspace_region_scale.x, m_descriptors.misc.title_y_scale - m_descriptors.dockspaces.border_size },
					{ SkipAction, nullptr, 0 }
				);

				float2 sizes[128];
				CalculateDockspaceRegionHeaders(dockspace, border_index, offset_mask, sizes);

				DrawCollapseTriangleHeader(buffers, vertex_count, dockspace, border_index, offset_mask, true);
				AddDefaultClickable(collapse_triangle_data);

				for (size_t index = 0; index < dockspace->borders[border_index].window_indices.size; index++) {
					// header rectangle
					float2 header_position = float2(dockspace_region_position.x + sizes[index].x, dockspace_region_position.y);
					float2 header_scale = float2(sizes[index].y, m_descriptors.misc.title_y_scale - m_descriptors.dockspaces.border_size);
					Color region_header_color = m_descriptors.color_theme.region_header;
					if (is_focused_region && (dockspace->borders[border_index].active_window == index))
						region_header_color = LightenColorClamp(region_header_color, m_descriptors.color_theme.region_header_active_window_factor);

					SetSolidColorRectangle(
						header_position,
						header_scale,
						region_header_color,
						solid_color,
						vertex_count[ECS_TOOLS_UI_SOLID_COLOR]
					);

					UIRegionHeaderData region_header_data;
					UISystemDefaultHoverableData hoverable_data;
					region_header_data.window_index = index;
					region_header_data.hoverable_data.colors[0] = m_descriptors.color_theme.region_header;
					region_header_data.hoverable_data.percentages[0] = 1.25f;
					region_header_data.drag_data.floating_dockspace = nullptr;
					region_header_data.dockspace_to_drag = nullptr;
					hoverable_data.border_index = border_index;
					hoverable_data.dockspace = dockspace;
					hoverable_data.thread_id = thread_id;
					hoverable_data.data.colors[0] = region_header_color;
					hoverable_data.position = header_position;
					hoverable_data.scale = header_scale;
					hoverable_data.phase = ECS_UI_DRAW_LATE;
					AddDefaultHoverable(hoverable_data);
					AddClickableToDockspaceRegion(
						thread_id,
						dockspace,
						border_index,
						header_position,
						header_scale,
						&region_header_data,
						RegionHeaderAction,
						ECS_UI_DRAW_SYSTEM
					);
					
					CullRegionHeader(
						dockspace, 
						border_index, 
						index,
						offset_mask, 
						dockspace_region_position.x + sizes[index].x, 
						sizes,
						buffers,
						vertex_count
					);
				
					if (sizes[index].y >= close_x_scale.x * 1.25f) {
						// close window X; adding it to the text sprite stream
						float2 close_x_position = float2(
							dockspace_region_position.x + sizes[index].x + sizes[index].y - m_descriptors.dockspaces.close_x_position_x_left,
							AlignMiddle(dockspace_region_position.y, m_descriptors.misc.title_y_scale - m_descriptors.dockspaces.border_size, m_descriptors.dockspaces.close_x_scale_y)
						);
						float2 expanded_position = ExpandRectangle(close_x_position, close_x_scale, expanded_close_x_scale);
						SetSpriteRectangle(
							close_x_position,
							close_x_scale,
							m_descriptors.color_theme.region_header_x,
							m_font_character_uvs[2 * x_sprite_offset],
							m_font_character_uvs[2 * x_sprite_offset + 1],
							text_sprites,
							vertex_count[ECS_TOOLS_UI_TEXT_SPRITE]
						);

						UICloseXClickData click_data = { index };
						UIDefaultHoverableData x_hoverable_data;
						x_hoverable_data.colors[0] = m_descriptors.color_theme.region_header;
						x_hoverable_data.percentages[0] = 1.8f;
						UISystemDefaultClickableData clickable_data;
						clickable_data.clickable_handler.action = CloseXAction;
						clickable_data.clickable_handler.data = &click_data;
						clickable_data.clickable_handler.data_size = sizeof(click_data);
						clickable_data.clickable_handler.phase = ECS_UI_DRAW_SYSTEM;
						clickable_data.border_index = border_index;
						clickable_data.dockspace = dockspace;
						clickable_data.position = expanded_position;
						clickable_data.scale = expanded_close_x_scale;
						clickable_data.thread_id = thread_id;
						clickable_data.hoverable_handler.action = CloseXBorderHoverableAction;
						clickable_data.hoverable_handler.data = &x_hoverable_data;
						clickable_data.hoverable_handler.data_size = sizeof(x_hoverable_data);
						clickable_data.hoverable_handler.phase = ECS_UI_DRAW_LATE;
						AddDefaultClickable(clickable_data);
						AddHoverableToDockspaceRegion(thread_id, dockspace, border_index, expanded_position, expanded_close_x_scale, { CloseXBorderHoverableAction, &x_hoverable_data, sizeof(x_hoverable_data), ECS_UI_DRAW_PHASE::ECS_UI_DRAW_LATE });
					}
				}
			}
			else if (dockspace->borders[border_index].draw_elements) {
				DrawCollapseTriangleHeader(buffers, vertex_count, dockspace, border_index, offset_mask, false);
				UIDefaultHoverableData hoverable_data;
				hoverable_data.colors[0] = m_descriptors.color_theme.background;
				hoverable_data.percentages[0] = 6.0f;
				collapse_triangle_data.hoverable_handler.data = &hoverable_data;
				collapse_triangle_data.hoverable_handler.data_size = sizeof(hoverable_data);
				AddDefaultClickable(collapse_triangle_data);
			}

			if (dockspace->borders[border_index].draw_close_x) {
				// close window X; adding it to the text sprite stream
				float2 close_x_position = float2(
					dockspace_region_position.x + dockspace_region_scale.x - m_descriptors.dockspaces.close_x_total_x_padding,
					AlignMiddle(dockspace_region_position.y, m_descriptors.misc.title_y_scale - m_descriptors.dockspaces.border_size, m_descriptors.dockspaces.close_x_scale_y)
				);
				float2 expanded_position = ExpandRectangle(close_x_position, close_x_scale, expanded_close_x_scale);
				SetSpriteRectangle(
					close_x_position,
					close_x_scale,
					m_descriptors.color_theme.region_header_x,
					m_font_character_uvs[2 * x_sprite_offset],
					m_font_character_uvs[2 * x_sprite_offset + 1],
					text_sprites,
					vertex_count[ECS_TOOLS_UI_TEXT_SPRITE]
				);

				UIDefaultHoverableData close_x_hoverable_data;
				close_x_hoverable_data.colors[0] = m_descriptors.color_theme.region_header_hover_x;
				close_x_hoverable_data.percentages[0] = 1.5f;
				UISystemDefaultClickableData clickable_data;
				clickable_data.clickable_handler.action = CloseXBorderClickableAction;
				clickable_data.clickable_handler.data = nullptr;
				clickable_data.clickable_handler.data_size = 0;
				clickable_data.clickable_handler.phase = ECS_UI_DRAW_SYSTEM;
				clickable_data.border_index = border_index;
				clickable_data.dockspace = dockspace;
				clickable_data.position = expanded_position;
				clickable_data.scale = expanded_close_x_scale;
				clickable_data.thread_id = thread_id;
				clickable_data.hoverable_handler.phase = ECS_UI_DRAW_LATE;
				clickable_data.hoverable_handler.action = CloseXBorderHoverableAction;
				clickable_data.hoverable_handler.data = &close_x_hoverable_data;
				clickable_data.hoverable_handler.data_size = sizeof(close_x_hoverable_data);
				AddDefaultClickable(clickable_data);
			}
			
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DrawDockspaceRegionBackground(
			unsigned int thread_id,
			UIDockspace* dockspace, 
			unsigned int border_index, 
			float offset_mask, 
			void** buffers, 
			size_t* vertex_count
		)
		{
			UIVertexColor* solid_color = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
			float offset_mask_inverse = 1 - offset_mask;
			unsigned int window_index = GetWindowIndexFromBorder(dockspace, border_index);
			Color background_color = m_windows[window_index].descriptors->color_theme.background;
			float2 dockspace_region_position = GetDockspaceRegionPosition(dockspace, border_index, offset_mask);
			float2 dockspace_region_scale = GetDockspaceRegionScale(dockspace, border_index, offset_mask);
			SetSolidColorRectangle(
				dockspace_region_position,
				dockspace_region_scale,
				background_color,
				solid_color,
				vertex_count[ECS_TOOLS_UI_SOLID_COLOR]
			);
			UIDragDockspaceData drag_dockspace_data;
			drag_dockspace_data.floating_dockspace = nullptr;
			float normalized_margin = NormalizeHorizontalToWindowDimensions(m_descriptors.dockspaces.border_margin);
			AddClickableToDockspaceRegion(
				thread_id,
				dockspace,
				border_index,
				{
					dockspace_region_position.x + normalized_margin * 0.5f,
					dockspace_region_position.y + m_descriptors.dockspaces.border_margin * 0.5f
				},
			{
				dockspace_region_scale.x - normalized_margin,
				dockspace_region_scale.y - m_descriptors.dockspaces.border_margin
			},
				&drag_dockspace_data,
				DragDockspaceAction,
				ECS_UI_DRAW_PHASE::ECS_UI_DRAW_SYSTEM
			);

		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DrawFixedDockspaceRegionBackground(
			const UIDockspace* dockspace,
			unsigned int border_index,
			float2 position,
			float2 scale,
			void** buffers, 
			size_t* vertex_count
		)
		{
			// draw background and borders
			UIVertexColor* solid_color = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
			Color background_color = m_descriptors.color_theme.background;
			if (dockspace->borders[border_index].window_indices.size > 0) {
				unsigned int window_index = GetWindowIndexFromBorder(dockspace, border_index);
				background_color = m_windows[window_index].descriptors->color_theme.background;
			}
			SetSolidColorRectangle(
				position,
				scale,
				background_color,
				solid_color,
				vertex_count[ECS_TOOLS_UI_SOLID_COLOR]
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2  UISystem::DrawToolTipSentence(
			ActionData* action_data,
			Stream<char> characters,
			UITooltipBaseData* data
		)
		{
			float2 position = action_data->position;
			float2 scale = action_data->scale;

			void** buffers = action_data->buffers;
			size_t* counts = action_data->counts;
			float2 mouse_position = action_data->mouse_position;
			position += data->offset;
			position.x += data->offset_scale.x ? scale.x : 0.0f;
			position.y += data->offset_scale.y ? scale.y : 0.0f;

			unsigned int new_line_characters[256];
			Stream<unsigned int> temp_stream = Stream<unsigned int>(new_line_characters, 0);
			for (size_t index = 0; index < characters.size; index++) {
				if (characters[index] == '\n') {
					temp_stream.Add(index);
				}
			}

			temp_stream.Add(characters.size);
			size_t word_start_index = 0;
			size_t word_end_index = 0;

			ConfigureToolTipBase(data);

			float space_x_scale = GetSpaceXSpan(data->font_size.x);
			float text_y_span = GetTextSpriteYScale(data->font_size.y);

			position.x = function::ClampMin(position.x, -0.99f);
			position.y = function::ClampMin(position.y, -0.99f);
			float2 initial_position = position;

			position.x += m_descriptors.misc.tool_tip_padding.x;
			position.y += /*m_descriptors.misc.tool_tip_padding.y*/ + data->next_row_offset;
			float2 max_bounds = position;

			UISpriteVertex* text_vertices = (UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE];
			size_t* count = &counts[ECS_TOOLS_UI_TEXT_SPRITE];
			size_t initial_vertex_count = *count;

			float2 row_position = initial_position;
			float2 row_scale = { 0.0f, 0.0f };
			Color current_color = data->font_color;
			if (data->unavailable_rows != nullptr && data->unavailable_rows[0] == true) {
				current_color = data->unavailable_font_color;
			}
			size_t row_index = 0;

			auto handle_row_action = [](ActionData* action_data, UITooltipPerRowData data, size_t index) {
				if (data.handlers != nullptr) {
					if (data.is_uniform) {
						action_data->data = data.handlers->data;
						data.handlers->action(action_data);
					}
					else {
						action_data->data = data.handlers[index].data;
						data.handlers[index].action(action_data);
					}
				}
			};
			for (size_t index = 0; index < temp_stream.size; index++) {
				if (temp_stream[index] != 0) {
					word_end_index = temp_stream[index] - 1;
				}
				if (word_end_index >= word_start_index) {
					float2 text_span;

					Stream<UISpriteVertex> temp_vertex_stream = Stream<UISpriteVertex>(text_vertices + *count, (word_end_index - word_start_index + 1) * 6);
					ConvertCharactersToTextSprites(
						{ characters.buffer + word_start_index, temp_vertex_stream.size / 6 },
						position,
						text_vertices,
						current_color,
						*count,
						data->font_size,
						data->character_spacing
					);

					text_span = ECSEngine::Tools::GetTextSpan(temp_vertex_stream);
					position.x += text_span.x;
					*count += temp_vertex_stream.size;
				}

				while (temp_stream[index] == temp_stream[index + 1] - 1) {
					position = { initial_position.x + m_descriptors.misc.tool_tip_padding.x, position.y + text_y_span + data->next_row_offset };
					max_bounds.x = std::max(max_bounds.x, position.x);
					max_bounds.y = std::max(max_bounds.y, position.y);
					index++;
				}

				// last character will cause a next row jump and invalidate last recorded position on the x axis
				max_bounds.x = std::max(max_bounds.x, position.x);
				bool is_available = true;
				if (data->unavailable_rows != nullptr && data->unavailable_rows[row_index] == true) {
					current_color = data->unavailable_font_color;
					is_available = false;
				}
				else {
					current_color = data->font_color;
				}

				if (is_available) {
					row_scale = { position.x - row_position.x + m_descriptors.misc.tool_tip_padding.x, position.y - row_position.y + text_y_span };
					action_data->position = row_position;
					action_data->scale = row_scale;
					handle_row_action(action_data, data->basic_draw, row_index);
					if (IsPointInRectangle(mouse_position, row_position, row_scale)) {
						if (data->previous_hoverable != nullptr) {
							*data->previous_hoverable = row_index;
						}
						handle_row_action(action_data, data->hoverable_draw, row_index);
					}
					else if ((data->previous_hoverable != nullptr) && (*data->previous_hoverable = row_index)) {
						handle_row_action(action_data, data->hoverable_draw, row_index);
					}
				}

				position = { initial_position.x + m_descriptors.misc.tool_tip_padding.x, position.y + text_y_span + data->next_row_offset };
				row_position = position;
				row_index++;
				max_bounds.x = std::max(max_bounds.x, position.x);
				max_bounds.y = std::max(max_bounds.y, position.y);
				word_start_index = temp_stream[index] + 1;
			}

			max_bounds.y += m_descriptors.misc.tool_tip_padding.y /*+ 0.003f*/;
			max_bounds.x += m_descriptors.misc.tool_tip_padding.x;

			float2 translation = { 0.0f, 0.0f };
			translation.x = max_bounds.x > 0.99f ? max_bounds.x - 0.99f : 0.0f;
			translation.y = max_bounds.y > 0.99f ? max_bounds.y - 0.99f : 0.0f;

			if (translation.x != 0.0f || translation.y != 0.0f) {
				for (size_t index = initial_vertex_count; index < *count; index++) {
					text_vertices[index].position.x -= translation.x;
					text_vertices[index].position.y += translation.y;
				}
			}
			initial_position -= translation;
			max_bounds -= translation;

			SetSolidColorRectangle(initial_position, max_bounds - initial_position, data->background_color, buffers, counts, 0);
			CreateSolidColorRectangleBorder<false>(initial_position, max_bounds - initial_position, data->border_size, data->border_color, counts, buffers);
		
			return max_bounds - initial_position;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::DrawToolTipSentenceWithTextToRight(
			ActionData* action_data,
			Stream<char> aligned_to_left_text,
			Stream<char> aligned_to_right_text,
			UITooltipBaseData* data
		)
		{

#pragma region New line detection

			ECS_STACK_CAPACITY_STREAM(unsigned int, left_new_lines, 256);
			ECS_STACK_CAPACITY_STREAM(unsigned int, right_new_lines, 256);

			function::FindToken(aligned_to_left_text, '\n', left_new_lines);
			function::FindToken(aligned_to_right_text, '\n', right_new_lines);

			left_new_lines.Add(aligned_to_left_text.size);
			right_new_lines.Add(aligned_to_right_text.size);

			ECS_ASSERT(left_new_lines.size == right_new_lines.size);

#pragma endregion

#pragma region Configuration and position handling

			unsigned int current_position = 0;
			float max_left_scale = 0.0f;

			float2 position = action_data->position;
			float2 scale = action_data->scale;

			void** buffers = action_data->buffers;
			size_t* counts = action_data->counts;
			float2 mouse_position = action_data->mouse_position;
			position += data->offset;
			position.x += data->offset_scale.x ? scale.x : 0.0f;
			position.y += data->offset_scale.y ? scale.y : 0.0f;
			size_t word_start_index = 0;
			size_t word_end_index = 0;

			ConfigureToolTipBase(data);

			float space_x_scale = GetSpaceXSpan(data->font_size.x);
			float text_y_span = GetTextSpriteYScale(data->font_size.y);

			position.x = position.x < -0.99f ? -0.99f : position.x;
			position.y = position.y < -0.99f ? -0.99f : position.y;
			float2 initial_position = position;

			position.x += m_descriptors.misc.tool_tip_padding.x;
			position.y += data->next_row_offset;
			float2 max_bounds = position;

			UISpriteVertex* text_vertices = (UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE];
			size_t* count = &counts[ECS_TOOLS_UI_TEXT_SPRITE];
			size_t initial_vertex_count = *count;

			float2 row_position = initial_position;
			float2 row_scale = { 0.0f, 0.0f };
			Color current_color = data->font_color;
			if (data->unavailable_rows != nullptr && data->unavailable_rows[0] == true) {
				current_color = data->unavailable_font_color;
			}
			size_t row_index = 0;

#pragma endregion

#pragma region Left characters draw

			for (size_t index = 0; index < left_new_lines.size; index++) {
				if (left_new_lines[index] != 0) {
					word_end_index = left_new_lines[index] - 1;
				}
				if (word_end_index >= word_start_index) {
					float2 text_span;

					Stream<UISpriteVertex> temp_vertex_stream = Stream<UISpriteVertex>(text_vertices + *count, (word_end_index - word_start_index + 1) * 6);
					ConvertCharactersToTextSprites(
						{ aligned_to_left_text.buffer + word_start_index, temp_vertex_stream.size / 6 },
						position,
						text_vertices,
						current_color,
						*count,
						data->font_size,
						data->character_spacing
					);

					text_span = ECSEngine::Tools::GetTextSpan(temp_vertex_stream);
					max_left_scale = std::max(max_left_scale, text_span.x);
					position.x += text_span.x;
					*count += temp_vertex_stream.size;
				}

				if (data->unavailable_rows != nullptr && data->unavailable_rows[row_index] == true) {
					current_color = data->unavailable_font_color;
				}
				else {
					current_color = data->font_color;
				}

				position = { initial_position.x + m_descriptors.misc.tool_tip_padding.x, position.y + text_y_span + data->next_row_offset };
				row_position = position;
				row_index++;
				max_bounds.y = std::max(max_bounds.y, position.y);
				word_start_index = left_new_lines[index] + 1;
			}

			max_bounds.y += m_descriptors.misc.tool_tip_padding.y;

#pragma endregion

#pragma region Right character draw

			float max_right_scale = 0.0f;
			
			float right_text_spans[256];
			auto calculate_span_kernel = [data, this, &current_position](float& max_scale, const char* characters, size_t new_line_character, float& text_x_scale) {
				if (new_line_character > current_position) {
					float2 text_span = GetTextSpan(
						{ characters + current_position, new_line_character - current_position },
						data->font_size.x,
						data->font_size.y,
						data->character_spacing
					);
					max_scale = std::max(max_scale, text_span.x);
					text_x_scale = text_span.x;
				}
				else {
					text_x_scale = 0.0f;
				}
			};

			for (size_t index = 0; index < right_new_lines.size; index++) {
				calculate_span_kernel(max_right_scale, aligned_to_right_text.buffer, right_new_lines[index], right_text_spans[index]);
				current_position = right_new_lines[index] + 1;
			}

			float current_y = initial_position.y + m_descriptors.misc.tool_tip_padding.y;
			max_bounds.x = initial_position.x + m_descriptors.misc.tool_tip_padding.x * 2.0f + max_left_scale + ECS_TOOLS_UI_TOOL_TIP_TEXT_TO_RIGHT_PADD + max_right_scale;
			float font_y_size = GetTextSpriteYScale(data->font_size.y);

			int64_t _word_start_index = 0;
			int64_t _word_end_index = 0;
			for (size_t index = 0; index < right_new_lines.size; index++) {
				_word_end_index = right_new_lines[index] - 1;
				
				if (_word_end_index - _word_start_index > 0) {
					float2 text_position = { max_bounds.x - m_descriptors.misc.tool_tip_padding.x - right_text_spans[index], current_y };

					Color current_color = data->font_color;
					if (data->unavailable_rows != nullptr && data->unavailable_rows[row_index] == true) {
						current_color = data->unavailable_font_color;
					}
					ConvertCharactersToTextSprites(
						{ aligned_to_right_text.buffer + _word_start_index, (size_t)_word_end_index - (size_t)_word_start_index + 1 },
						text_position,
						text_vertices,
						current_color,
						*count,
						data->font_size,
						data->character_spacing
					);
				}

				current_y += font_y_size + data->next_row_offset;
				*count += (_word_end_index - _word_start_index + 1) * 6;
				_word_start_index = _word_end_index + 2;
			}

#pragma endregion

#pragma region Translation if needed

			float2 translation = { 0.0f, 0.0f };
			translation.x = max_bounds.x > 0.99f ? max_bounds.x - 0.99f : 0.0f;
			translation.y = max_bounds.y > 0.99f ? max_bounds.y - 0.99f : 0.0f;

			if (translation.x != 0.0f || translation.y != 0.0f) {
				for (size_t index = initial_vertex_count; index < *count; index++) {
					text_vertices[index].position.x -= translation.x;
					text_vertices[index].position.y += translation.y;
				}
			}
			initial_position -= translation;
			max_bounds -= translation;

#pragma endregion

			SetSolidColorRectangle(initial_position, max_bounds - initial_position, data->background_color, buffers, counts, 0);
			CreateSolidColorRectangleBorder<false>(initial_position, max_bounds - initial_position, data->border_size, data->border_color, counts, buffers);

			return max_bounds - initial_position;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::DrawToolTipSentenceSize(Stream<char> characters, UITooltipBaseData* data)
		{
			unsigned int new_line_characters[256];
			Stream<unsigned int> temp_stream = Stream<unsigned int>(new_line_characters, 0);
			for (size_t index = 0; index < characters.size; index++) {
				if (characters[index] == '\n') {
					temp_stream.Add(index);
				}
			}

			temp_stream.Add(characters.size);
			size_t word_start_index = 0;
			size_t word_end_index = 0;

			ConfigureToolTipBase(data);

			float space_x_scale = GetSpaceXSpan(data->font_size.x);
			float text_y_span = GetTextSpriteYScale(data->font_size.y);

			float2 position = { 0.0f, 0.0f };
			float2 initial_position = position;

			position.x += m_descriptors.misc.tool_tip_padding.x;
			position.y += m_descriptors.misc.tool_tip_padding.y + data->next_row_offset;
			float2 max_bounds = position;

			for (size_t index = 0; index < temp_stream.size; index++) {
				if (temp_stream[index] != 0) {
					word_end_index = temp_stream[index] - 1;
				}
				if (word_end_index >= word_start_index) {
					float2 text_span = GetTextSpan({ characters.buffer + word_start_index, word_end_index - word_start_index + 1 }, data->font_size.x, data->font_size.y, data->character_spacing);
					position.x += text_span.x;
				}

				while (temp_stream[index] == temp_stream[index + 1] - 1) {
					position = { initial_position.x + m_descriptors.misc.tool_tip_padding.x, position.y + text_y_span + data->next_row_offset };
					max_bounds.x = std::max(max_bounds.x, position.x);
					max_bounds.y = std::max(max_bounds.y, position.y);
					index++;
				}

				// last character will cause a next row jump and invalidate last recorded position on the x axis
				max_bounds.x = std::max(max_bounds.x, position.x);
				position = { initial_position.x + m_descriptors.misc.tool_tip_padding.x, position.y + text_y_span + data->next_row_offset };
				max_bounds.x = std::max(max_bounds.x, position.x);
				max_bounds.y = std::max(max_bounds.y, position.y);
				word_start_index = temp_stream[index] + 1;
			}

			max_bounds.y += m_descriptors.misc.tool_tip_padding.y;
			max_bounds.x += m_descriptors.misc.tool_tip_padding.x;

			return max_bounds;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::DrawToolTipSentenceWithTextToRightSize(
			Stream<char> aligned_to_left_text,
			Stream<char> aligned_to_right_text,
			UITooltipBaseData* data
		)
		{
			ConfigureToolTipBase(data);
			ECS_STACK_CAPACITY_STREAM(unsigned int, left_new_lines, 256);
			ECS_STACK_CAPACITY_STREAM(unsigned int, right_new_lines, 256);

			function::FindToken(aligned_to_left_text, '\n', left_new_lines);
			function::FindToken(aligned_to_right_text, '\n', right_new_lines);

			left_new_lines.Add(aligned_to_left_text.size);
			right_new_lines.Add(aligned_to_right_text.size);

			ECS_ASSERT(left_new_lines.size == right_new_lines.size);

			float max_left_scale = 0.0f;
			float max_right_scale = 0.0f;
			float sprite_y_scale = GetTextSpriteYScale(data->font_size.y);

			size_t current_position = 0;
			for (size_t index = 0; index < left_new_lines.size; index++) {
				float2 text_span = GetTextSpan(
					{ aligned_to_left_text.buffer + current_position, left_new_lines[index] - current_position },
					data->font_size.x,
					data->font_size.y,
					data->character_spacing
				);
				max_left_scale = std::max(max_left_scale, text_span.x);
				current_position = left_new_lines[index] + 1;
			}

			current_position = 0;
			for (size_t index = 0; index < right_new_lines.size; index++) {
				float2 text_span = GetTextSpan(
					{ aligned_to_right_text.buffer + current_position, right_new_lines[index] - current_position },
					data->font_size.x,
					data->font_size.y,
					data->character_spacing
				);
				max_right_scale = std::max(max_right_scale, text_span.x);
				current_position = right_new_lines[index] + 1;
			}

			return { max_left_scale + max_right_scale + 2.0f * m_descriptors.misc.tool_tip_padding.x + ECS_TOOLS_UI_TOOL_TIP_TEXT_TO_RIGHT_PADD, 2.0f * m_descriptors.misc.tool_tip_padding.y + (sprite_y_scale + data->next_row_offset) * left_new_lines.size + data->next_row_offset };
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DrawDockspaceRegionBorders(
			float2 region_position,
			float2 region_scale,
			void** buffers,
			size_t* vertex_count
		) {
			float2 border_transforms[8];
			// add small offsets in order to avoid random culling of the borders by the viewport
			CreateSolidColorRectangleBorder<true>(
				{ region_position.x, region_position.y },
				{region_scale.x - 0.0000f, region_scale.y - 0.000f},
				{ NormalizeHorizontalToWindowDimensions(m_descriptors.dockspaces.border_size), m_descriptors.dockspaces.border_size },
				m_descriptors.color_theme.borders,
				vertex_count,
				buffers,
				border_transforms
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::DrawPass(
			CapacityStream<VertexBuffer>& buffers,
			CapacityStream<UIDynamicStream<UISpriteTexture>>& sprite_textures,
			ConstantBuffer& viewport_buffer,
			const size_t* counts,
			GraphicsContext* context,
			void* extra_information,
			unsigned int material_offset
		) {
			BindVertexConstantBuffer(viewport_buffer, context);

			if (counts[ECS_TOOLS_UI_SOLID_COLOR + material_offset] > 0) {
				BindVertexBuffer(buffers[ECS_TOOLS_UI_SOLID_COLOR + material_offset], context);
				SetSolidColorRenderState(context);
				ECSEngine::Draw(counts[ECS_TOOLS_UI_SOLID_COLOR + material_offset], context);
			}

			if (counts[ECS_TOOLS_UI_LINE + material_offset] > 0) {
				BindVertexBuffer(buffers[ECS_TOOLS_UI_LINE + material_offset], context);
				SetSolidColorRenderState(context);

				Topology topology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
				BindTopology(topology, context);
				ECSEngine::Draw(counts[ECS_TOOLS_UI_LINE + material_offset], context);
			}

			if (counts[ECS_TOOLS_UI_SPRITE + material_offset] > 0) {
				BindVertexBuffer(buffers[ECS_TOOLS_UI_SPRITE + material_offset], context);
				SetSpriteRenderState(context);

				// drawing each sprite one by one
				unsigned int sprite_texture_material_offset = material_offset / ECS_TOOLS_UI_MATERIALS;
				size_t sprite_texture_count = sprite_textures[ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFER_INDEX + sprite_texture_material_offset * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS].size;
				UIDynamicStream<UISpriteTexture>* textures = &sprite_textures[ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFER_INDEX + sprite_texture_material_offset * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS];
				
				for (size_t index = 0; index < sprite_texture_count; index++) {
					BindPixelResourceView(textures->buffer[index].view, context);
					ECSEngine::Draw(6, context, index * 6);
				}
				m_graphics->DisableAlphaBlending(context);
			}

			if (counts[ECS_TOOLS_UI_SPRITE_CLUSTER + material_offset] > 0) {
				BindVertexBuffer(buffers[ECS_TOOLS_UI_SPRITE_CLUSTER + material_offset], context);
				SetSpriteRenderState(context);

				// drawing each cluster one by one
				const UIDynamicStream<unsigned int>* substream = (const UIDynamicStream<unsigned int>*)extra_information;
				size_t offset = 0;
				unsigned int sprite_texture_material_offset = material_offset / ECS_TOOLS_UI_MATERIALS;
				UIDynamicStream<UISpriteTexture>* textures = &sprite_textures[ECS_TOOLS_UI_SPRITE_CLUSTER_TEXTURE_BUFFER_INDEX + sprite_texture_material_offset * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS];
				for (size_t index = 0; index < substream->size; index++) {
					BindPixelResourceView(textures->buffer[index].view, context);
					ECSEngine::Draw(substream->buffer[index], context, offset);
					offset += substream->buffer[index];
				}
				m_graphics->DisableAlphaBlending(context);
			}

			if (counts[ECS_TOOLS_UI_TEXT_SPRITE + material_offset] > 0) {
				BindVertexBuffer(buffers[ECS_TOOLS_UI_TEXT_SPRITE + material_offset], context);
				SetTextSpriteRenderState(context);
				ECSEngine::Draw(counts[ECS_TOOLS_UI_TEXT_SPRITE + material_offset], context);
				m_graphics->DisableAlphaBlending(context);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		// this cannot handle render region
		template<ECS_UI_DRAW_PHASE phase>
		void UISystem::DrawPass(
			UIDrawResources& resources,
			const size_t* counts,
			float2 viewport_position,
			float2 viewport_scale,
			GraphicsContext* context
		) {
			void* constant_buffer = MapBuffer(resources.region_viewport_info.buffer, context);
			// viewport buffer description: 
			// float2 region_center_position;
			// float2 region_half_dimensions_inverse;
			float* viewport_buffer_ptr = (float*)constant_buffer;
			float2 viewport_half_scale = { (viewport_scale.x * 0.5f), (viewport_scale.y * 0.5f) };

			// add a small offset in order to have the borders drawn
			FillViewportBuffer(viewport_buffer_ptr, viewport_position, viewport_half_scale);

			UnmapBuffer(resources.region_viewport_info.buffer, context);

			if constexpr (phase == ECS_UI_DRAW_PHASE::ECS_UI_DRAW_SYSTEM) {
				DrawPass(resources.buffers, resources.sprite_textures, resources.region_viewport_info, counts, context, &resources.sprite_cluster_subtreams, 0);
			}
			else {
				DrawPass(resources.buffers, resources.sprite_textures, resources.region_viewport_info, counts, context, &resources.sprite_cluster_subtreams, (unsigned int)phase * ECS_TOOLS_UI_MATERIALS);
			}
		}

		template ECSENGINE_API void UISystem::DrawPass<ECS_UI_DRAW_NORMAL>(UIDrawResources&, const size_t*, float2, float2, GraphicsContext*);
		template ECSENGINE_API void UISystem::DrawPass<ECS_UI_DRAW_LATE>(UIDrawResources&, const size_t*, float2, float2, GraphicsContext*);
		template ECSENGINE_API void UISystem::DrawPass<ECS_UI_DRAW_SYSTEM>(UIDrawResources&, const size_t*, float2, float2, GraphicsContext*);

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::ExistsWindowResource(unsigned int window_index, Stream<char> name) const
		{
			ResourceIdentifier identifier(name);
			return m_windows[window_index].table.Find(identifier) != -1;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::EvictOutdatedTextures()
		{
			m_resource_manager->EvictOutdatedResources(ResourceType::Texture);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::FillActionDataSystemParameters(ActionData* action_data)
		{
			action_data->keyboard = m_keyboard;
			action_data->mouse = m_mouse;
			action_data->keyboard_tracker = m_keyboard_tracker;
			action_data->mouse_tracker = m_mouse_tracker;
			action_data->system = this;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::FillWindowDataAfterFileLoad(Stream<UIWindowSerializedMissingData> data)
		{
			for (size_t index = 0; index < m_windows.size; index++) {
				m_windows[index].draw = data[index].draw;
				m_windows[index].private_handler.action = data[index].private_action;

				m_windows[index].private_handler.data = function::CopyNonZero(GetAllocatorPolymorphic(m_memory), data[index].private_action_data, data[index].private_action_data_size);
				m_windows[index].window_data = function::CopyNonZero(GetAllocatorPolymorphic(m_memory), data[index].window_data, data[index].window_data_size);

				if (data[index].resource_count != 0) {
					m_memory->Deallocate(m_windows[index].table.GetAllocatedBuffer());
					size_t table_memory = WindowTable::MemoryOf(data[index].resource_count);
					void* allocation = m_memory->Allocate(table_memory);
					memset(allocation, 0, table_memory);
					m_windows[index].table = WindowTable(allocation, data[index].resource_count);
				}

				UIDrawerDescriptor descriptor = GetDrawerDescriptor(index);
				data[index].draw(m_windows[index].window_data, &descriptor, true);

				// handler initialization with render sliders
				Action data_initializer = (Action)m_window_handler.data;

				UIWindowDescriptor window_descriptor;
				window_descriptor.window_name = m_windows[index].name;

				ActionData initializer_data;
				initializer_data.border_index = 0;
				initializer_data.data = m_windows[index].default_handler.data;
				initializer_data.system = this;
				initializer_data.dockspace = &m_floating_horizontal_dockspaces[0];
				initializer_data.counts = &index;
				initializer_data.buffers = (void**)&window_descriptor;

				data_initializer(&initializer_data);

			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::FinalizeColorTheme()
		{
			for (size_t index = 0; index < m_windows.size; index++) {
				if (!m_windows[index].descriptors->configured[(unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COLOR_THEME]) {
					memcpy(&m_windows[index].descriptors->color_theme, &m_descriptors.color_theme, sizeof(UIColorThemeDescriptor));
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::FinalizeElementDescriptor() {
			for (size_t index = 0; index < m_windows.size; index++) {
				if (!m_windows[index].descriptors->configured[(unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_ELEMENT]) {
					memcpy(&m_windows[index].descriptors->element_descriptor, &m_descriptors.element_descriptor, sizeof(UIElementDescriptor));
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::FinalizeFont() {
			for (size_t index = 0; index < m_windows.size; index++) {
				if (!m_windows[index].descriptors->configured[(unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_FONT]) {
					memcpy(&m_windows[index].descriptors->font, &m_descriptors.font, sizeof(UIFontDescriptor));
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::FinalizeLayout() {
			for (size_t index = 0; index < m_windows.size; index++) {
				if (!m_windows[index].descriptors->configured[(unsigned int)ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX::ECS_UI_WINDOW_DRAWER_DESCRIPTOR_LAYOUT]) {
					memcpy(&m_windows[index].descriptors->layout, &m_descriptors.window_layout, sizeof(UILayoutDescriptor));
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::FindCharacterType(char character) const {
			return function::GetAlphabetIndex(character);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::FindCharacterType(char character, CharacterType& character_type) const {
			return function::GetAlphabetIndex(character, character_type);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void* UISystem::FindWindowResource(unsigned int window_index, Stream<char> name) const
		{
			return FindWindowResource(window_index, name.buffer, name.size);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void* UISystem::FindWindowResource(unsigned int window_index, const void* _identifier, unsigned int identifier_size) const {
			ResourceIdentifier identifier = ResourceIdentifier(_identifier, identifier_size);
			return m_windows[window_index].table.GetValue(identifier);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::FixFixedDockspace(UIDockspaceLayer old, UIDockspaceLayer new_layer)
		{
			for (size_t index = 0; index < m_fixed_dockspaces.size; index++) {
				if (m_fixed_dockspaces[index].type == old.type && m_fixed_dockspaces[index].index == old.index) {
					m_fixed_dockspaces[index] = new_layer;
					break;
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::FixBackgroundDockspace(UIDockspaceLayer old, UIDockspaceLayer new_layer)
		{
			for (size_t index = 0; index < m_background_dockspaces.size; index++) {
				if (m_background_dockspaces[index].type == old.type && m_background_dockspaces[index].index == old.index) {
					m_background_dockspaces[index] = new_layer;
					break;
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetDockspaceIndexFromMouseCoordinates(float2 mouse_position, DockspaceType& dockspace_type) const {

			float2 minimum_scale = float2(10.0f, 10.f);
			for (size_t index = 0; index < m_horizontal_dockspaces.size; index++) {
				if (IsPointInRectangle(
					mouse_position,
					m_horizontal_dockspaces[index].transform.position,
					m_horizontal_dockspaces[index].transform.scale
				)) {
					dockspace_type = DockspaceType::Horizontal;
					return index;
				}
			}
			for (size_t index = 0; index < m_vertical_dockspaces.size; index++) {
				if (IsPointInRectangle(
					mouse_position,
					m_vertical_dockspaces[index].transform.position,
					m_vertical_dockspaces[index].transform.scale
				)) {
					dockspace_type = DockspaceType::Vertical;
					return index;
				}
			}
			return 0xFFFFFFFF;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIDockspace* UISystem::GetDockspaceFromWindow(unsigned int window_index, unsigned int& border_index, DockspaceType& type)
		{
			auto search_lambda = [&](CapacityStream<UIDockspace>& dockspaces, DockspaceType current_type) {
				for (size_t index = 0; index < dockspaces.size; index++) {
					UIDockspace* dockspace = &dockspaces[index];
					for (size_t subindex = 0; subindex < dockspace->borders.size; subindex++) {
						if (!dockspace->borders[subindex].is_dock) {
							for (size_t window_idx = 0; window_idx < dockspace->borders[subindex].window_indices.size; window_idx++) {
								if (dockspace->borders[subindex].window_indices[window_idx] == window_index) {
									border_index = subindex;
									type = current_type;
									return dockspace;
								}
							}
						}
					}
				}
				return (UIDockspace*)nullptr;
			};

			auto pointer = search_lambda(m_floating_horizontal_dockspaces, DockspaceType::FloatingHorizontal);
			if (pointer == nullptr) {
				pointer = search_lambda(m_floating_vertical_dockspaces, DockspaceType::FloatingVertical);
				if (pointer == nullptr) {
					pointer = search_lambda(m_horizontal_dockspaces, DockspaceType::Horizontal);
					if (pointer == nullptr) {
						pointer = search_lambda(m_vertical_dockspaces, DockspaceType::Vertical);
					}
				}
			}
			return pointer;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetDockspaceIndexFromWindow(unsigned int window_index, unsigned int& border_index, DockspaceType& type)
		{
			auto search_lambda = [&](CapacityStream<UIDockspace>& dockspaces, DockspaceType current_type) {
				for (size_t index = 0; index < dockspaces.size; index++) {
					UIDockspace* dockspace = &dockspaces[index];
					for (size_t subindex = 0; subindex < dockspace->borders.size; subindex++) {
						if (!dockspace->borders[subindex].is_dock) {
							for (size_t window_idx = 0; window_idx < dockspace->borders[subindex].window_indices.size; window_idx++) {
								if (dockspace->borders[subindex].window_indices[window_idx] == window_index) {
									border_index = subindex;
									type = current_type;
									return (unsigned int)index;
								}
							}
						}
					}
				}
				return 0xFFFFFFFF;
			};

			unsigned int index = search_lambda(m_floating_horizontal_dockspaces, DockspaceType::FloatingHorizontal);
			if (index == 0xFFFFFFFF) {
				index = search_lambda(m_floating_vertical_dockspaces, DockspaceType::FloatingVertical);
				if (index == 0xFFFFFFFF) {
					index = search_lambda(m_horizontal_dockspaces, DockspaceType::Horizontal);
					if (index == 0xFFFFFFFF) {
						index = search_lambda(m_vertical_dockspaces, DockspaceType::Vertical);
					}
				}
			}
			return index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		// MSVC++ bug - that's why NOINLINE is there
		float2 ECS_NOINLINE UISystem::GetDockspaceRegionPosition(const UIDockspace* dockspace, unsigned int border_index, float offset_mask) const
		{
			float2 position = float2(
				dockspace->transform.position.x + (dockspace->borders[border_index].position * offset_mask),
				dockspace->transform.position.y + (dockspace->borders[border_index].position * (1.0f - offset_mask)) + 0.001f
			);
			return position;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		// MSVC++ bug - that's why NOINLINE is there
		float2 ECS_NOINLINE UISystem::GetDockspaceRegionScale(const UIDockspace* dockspace, unsigned int border_index, float offset_mask) const
		{
			float inverse_mask = 1.0f - offset_mask;
			float border_difference = dockspace->borders[border_index + 1].position - dockspace->borders[border_index].position;
			// add small offsets in order to help border get drawn correctly
			return float2(
				dockspace->transform.scale.x * inverse_mask + border_difference * offset_mask + m_descriptors.dockspaces.viewport_padding_x,
				dockspace->transform.scale.y * offset_mask + border_difference * inverse_mask + m_descriptors.dockspaces.viewport_padding_y
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetDockspaceBorderOffset(const UIDockspace* dockspace, unsigned int border_index, float offset_mask) const
		{
			return float2(
				dockspace->borders[border_index].position * offset_mask, 
				dockspace->borders[border_index].position * (1 - offset_mask)
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float UISystem::GetSpaceXSpan(float font_size_x)
		{
			unsigned int uv_index = FindCharacterType(' ');
			uv_index *= 2;

			float2 atlas_dimensions = m_font_character_uvs[m_descriptors.font.texture_dimensions];
			return (m_font_character_uvs[uv_index + 1].x - m_font_character_uvs[uv_index].x) * font_size_x * atlas_dimensions.x;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float UISystem::GetUnknownCharacterXSpan(float font_size_x)
		{
			unsigned int uv_index = FindCharacterType('\n');
			uv_index *= 2;

			float2 atlas_dimensions = m_font_character_uvs[m_descriptors.font.texture_dimensions];
			return (m_font_character_uvs[uv_index + 1].x - m_font_character_uvs[uv_index].x) * font_size_x * atlas_dimensions.x;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetDockspaceBorderPosition(const UIDockspace* dockspace, unsigned int border_index, float mask) const {
			float2 border_offset = GetDockspaceBorderOffset(dockspace, border_index, mask);
			return float2(
				dockspace->transform.position.x + border_offset.x,
				dockspace->transform.position.y + border_offset.y
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetDockspaceBorderScale(const UIDockspace* dockspace, unsigned int border_index, float mask) const {
			float inverse_mask = 1.0f - mask;
			return float2(
				m_descriptors.dockspaces.border_size * mask + dockspace->transform.scale.x * inverse_mask,
				m_descriptors.dockspaces.border_size * inverse_mask + dockspace->transform.scale.y * mask
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetDockspaceBorderFromMouseCoordinates(float2 mouse_position, UIDockspace** dockspace, float& mask) const {
			DockspaceType type;
			unsigned int floating_dockspace = GetFloatingDockspaceIndexFromMouseCoordinates(mouse_position, type);
			if (floating_dockspace != 0xFFFFFFFF) {
				return GetDockspaceBorderFromMouseCoordinates(mouse_position, dockspace, mask, floating_dockspace, type);
			}
			return 0xFFFFFFFF;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetDockspaceBorderFromMouseCoordinates(
			float2 mouse_position,
			UIDockspace** dockspace,
			float& mask,
			unsigned int floating_dockspace,
			DockspaceType type
		) const {
			UIDockspace* dockspaces[2] = {
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			return GetDockspaceBorderFromMouseCoordinates(
				mouse_position, 
				dockspace,
				mask, 
				&dockspaces[(unsigned int)type - 2][floating_dockspace],
				type
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetDockspaceBorderFromMouseCoordinates(
			float2 mouse_position,
			UIDockspace** dockspace,
			float& mask,
			UIDockspace* dockspace_to_search,
			DockspaceType type_to_search
		) const {
			const float masks[4] = {
				1.0f,
				0.0f,
				1.0f,
				0.0f
			};
			UIDockspace* dockspaces[4] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};

			const DockspaceType children_type[4] = {
				DockspaceType::Vertical,
				DockspaceType::Horizontal,
				DockspaceType::Vertical,
				DockspaceType::Horizontal
			};

			for (size_t index = 1; index < dockspace_to_search->borders.size - 1; index++) {
				float2 border_position = GetDockspaceBorderPosition(dockspace_to_search, index, masks[(unsigned int)type_to_search]);
				float2 border_scale = GetDockspaceBorderScale(dockspace_to_search, index, masks[(unsigned int)type_to_search]);
				if (IsPointInRectangle(
					mouse_position,
					border_position,
					border_scale
				)) {
					*dockspace = dockspace_to_search;
					mask = masks[(unsigned int)type_to_search];
					return index;
				}
			}
			for (size_t index = 0; index < dockspace_to_search->borders.size - 1; index++) {
				if (dockspace_to_search->borders[index].is_dock) {
					unsigned int value = GetDockspaceBorderFromMouseCoordinates(
						mouse_position, 
						dockspace, 
						mask, 
						&dockspaces[(unsigned int)children_type[(unsigned int)type_to_search]][dockspace_to_search->borders[index].window_indices[0]],
						children_type[(unsigned int)type_to_search]
					);
					if (value != 0xFFFFFFFF) {
						return value;
					}
				}
			}
			return 0xFFFFFFFF;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetDockspaceIndex(const UIDockspace* dockspace, DockspaceType type) const
		{
			const UIDockspace* dockspaces[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			return ((uintptr_t)dockspace - (uintptr_t)dockspaces[(unsigned int)type]) / (sizeof(UIDockspace));
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetFloatingDockspaceIndexFromMouseCoordinates(float2 mouse_position, DockspaceType& dockspace_type) const {
			const UIDockspace* dockspaces[2] = { m_floating_horizontal_dockspaces.buffer, m_floating_vertical_dockspaces.buffer };
			for (size_t index = 0; index < m_dockspace_layers.size; index++) {
				if (IsPointInRectangle(
					mouse_position,
					dockspaces[(unsigned int)m_dockspace_layers[index].type - 2][m_dockspace_layers[index].index].transform.position,
					dockspaces[(unsigned int)m_dockspace_layers[index].type - 2][m_dockspace_layers[index].index].transform.scale
				)) {
					dockspace_type = m_dockspace_layers[index].type;
					return m_dockspace_layers[index].index;
				}
			}
			return 0xFFFFFFFF;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetDockspaceIndexFromMouseCoordinatesWithChildren(
			float2 mouse_position, 
			DockspaceType& dockspace_type,
			unsigned int* parent_indices,
			DockspaceType* dockspace_types,
			unsigned int& parent_count
		) {
			// get the floating dockspace index
			unsigned int floating_dockspace_index = GetFloatingDockspaceIndexFromMouseCoordinates(mouse_position, dockspace_type);
		
			parent_count = 0;
			// if hovering over a floating dockspace
			if (floating_dockspace_index != 0xFFFFFFFF) {
				// search to the deepest child and get that dockspace
				unsigned int child_index = 0xFFFFFFFF;
				if (dockspace_type == DockspaceType::FloatingHorizontal) {
					for (size_t index = 0; child_index == 0xFFFFFFFF && index < m_floating_horizontal_dockspaces[floating_dockspace_index].borders.size - 1; index++) {
						// if it a dock search its children
						if (m_floating_horizontal_dockspaces[floating_dockspace_index].borders[index].is_dock) {
							child_index = SearchDockspaceForChildrenDockspaces(
								mouse_position, 
								m_floating_horizontal_dockspaces[floating_dockspace_index].borders[index].window_indices[0], 
								DockspaceType::Vertical, 
								dockspace_type,
								parent_indices,
								dockspace_types,
								parent_count
							);
						}
					}
					if (child_index != 0xFFFFFFFF) {
						parent_indices[parent_count] = floating_dockspace_index;
						dockspace_types[parent_count++] = DockspaceType::FloatingHorizontal;
						return child_index;
					}
					else {
						parent_indices[parent_count] = floating_dockspace_index;
						dockspace_types[parent_count++] = DockspaceType::FloatingHorizontal;
						dockspace_type = DockspaceType::FloatingHorizontal;
						return floating_dockspace_index;
					}
				}
				else if (dockspace_type == DockspaceType::FloatingVertical) {
					for (size_t index = 0; child_index == 0xFFFFFFFF && index < m_floating_vertical_dockspaces[floating_dockspace_index].borders.size - 1; index++) {
						// if it a dock search its children
						if (m_floating_vertical_dockspaces[floating_dockspace_index].borders[index].is_dock) {
							child_index = SearchDockspaceForChildrenDockspaces(
								mouse_position,
								m_floating_vertical_dockspaces[floating_dockspace_index].borders[index].window_indices[0],
								DockspaceType::Horizontal,
								dockspace_type,
								parent_indices,
								dockspace_types,
								parent_count
							);
						}
					}
					if (child_index != 0xFFFFFFFF) {
						parent_indices[parent_count] = floating_dockspace_index;
						dockspace_types[parent_count++] = DockspaceType::FloatingVertical;
						return child_index;
					}
					else {
						parent_indices[parent_count] = floating_dockspace_index;
						dockspace_types[parent_count++] = DockspaceType::FloatingVertical;
						dockspace_type = DockspaceType::FloatingVertical;
						return floating_dockspace_index;
					}
				}
			}
			return 0xFFFFFFFF;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::GetDockspacesFromParent(
			unsigned int dockspace_index,
			DockspaceType dockspace_type,
			unsigned int* subindicies,
			DockspaceType* subtypes,
			unsigned int& children_count
		) const {
			if (dockspace_type == DockspaceType::Horizontal) {
				for (size_t index = 0; index < m_horizontal_dockspaces[dockspace_index].borders.size - 1; index++) {
					if (m_horizontal_dockspaces[dockspace_index].borders[index].is_dock) {
						GetDockspacesFromParent(
							m_horizontal_dockspaces[dockspace_index].borders[index].window_indices[0],
							DockspaceType::Vertical,
							subindicies,
							subtypes,
							children_count
						);
						subindicies[children_count] = m_horizontal_dockspaces[dockspace_index].borders[index].window_indices[0];
						subtypes[children_count++] = DockspaceType::Vertical;
					}
				}
			}
			else if (dockspace_type == DockspaceType::Vertical) {
				for (size_t index = 0; index < m_vertical_dockspaces[dockspace_index].borders.size - 1; index++) {
					if (m_vertical_dockspaces[dockspace_index].borders[index].is_dock) {
						GetDockspacesFromParent(
							m_vertical_dockspaces[dockspace_index].borders[index].window_indices[0],
							DockspaceType::Horizontal,
							subindicies,
							subtypes,
							children_count
						);
						subindicies[children_count] = m_vertical_dockspaces[dockspace_index].borders[index].window_indices[0];
						subtypes[children_count++] = DockspaceType::Horizontal;
					}
				}
			}
			else if (dockspace_type == DockspaceType::FloatingHorizontal) {
				for (size_t index = 0; index < m_floating_horizontal_dockspaces[dockspace_index].borders.size - 1; index++) {
					if (m_floating_horizontal_dockspaces[dockspace_index].borders[index].is_dock) {
						GetDockspacesFromParent(
							m_floating_horizontal_dockspaces[dockspace_index].borders[index].window_indices[0],
							DockspaceType::Vertical,
							subindicies,
							subtypes,
							children_count
						);
						subindicies[children_count] = m_floating_horizontal_dockspaces[dockspace_index].borders[index].window_indices[0];
						subtypes[children_count++] = DockspaceType::Vertical;
					}
				}
			}
			else if (dockspace_type == DockspaceType::FloatingVertical) {
				for (size_t index = 0; index < m_floating_vertical_dockspaces[dockspace_index].borders.size - 1; index++) {
					if (m_floating_vertical_dockspaces[dockspace_index].borders[index].is_dock) {
						GetDockspacesFromParent(
							m_floating_vertical_dockspaces[dockspace_index].borders[index].window_indices[0],
							DockspaceType::Horizontal,
							subindicies,
							subtypes,
							children_count
						);
						subindicies[children_count] = m_floating_vertical_dockspaces[dockspace_index].borders[index].window_indices[0];
						subtypes[children_count++] = DockspaceType::Horizontal;
					}
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIDockspace* UISystem::GetFloatingDockspaceFromDockspace(UIDockspace* dockspace, float mask, DockspaceType& floating_type) const
		{
			const CapacityStream<UIDockspace>* dockspaces[4] = {
				&m_horizontal_dockspaces,
				&m_vertical_dockspaces,
				&m_floating_horizontal_dockspaces,
				&m_floating_vertical_dockspaces
			};
			bool is_vertical = mask == 1.0f;
			const CapacityStream<UIDockspace>* search1 = dockspaces[is_vertical];
			const CapacityStream<UIDockspace>* search2 = dockspaces[is_vertical + 2];
			const UIDockspace* type = dockspaces[1 - is_vertical]->buffer;

			for (size_t index = 0; index < search1->size; index++) {
				size_t border_count = search1->buffer[index].borders.size;
				UIDockspaceBorder* borders = search1->buffer[index].borders.buffer;
				for (size_t subindex = 0; subindex < border_count - 1; subindex++) {
					if (borders[subindex].is_dock) {
						if (&type[borders[subindex].window_indices[0]] == dockspace) {
							floating_type = DockspaceType(is_vertical);
							return GetFloatingDockspaceFromDockspace(&search1->buffer[index], 1.0f - is_vertical, floating_type);
						}
					}
				}
			}
			for (size_t index = 0; index < search2->size; index++) {
				size_t border_count = search2->buffer[index].borders.size;
				UIDockspaceBorder* borders = search2->buffer[index].borders.buffer;
				for (size_t subindex = 0; subindex < border_count - 1; subindex++) {
					if (borders[subindex].is_dock) {
						if (&type[borders[subindex].window_indices[0]] == dockspace) {
							floating_type = DockspaceType(is_vertical + 2);
							return &search2->buffer[index];
						}
					}
				}
			}
			return dockspace;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIDockspace* UISystem::GetFloatingDockspaceFromDockspace(
			UIDockspace* dockspace, 
			float mask,
			DockspaceType& floating_type, 
			unsigned int& border_index
		) const
		{
			const CapacityStream<UIDockspace>* dockspaces[4] = {
				&m_horizontal_dockspaces,
				&m_vertical_dockspaces,
				&m_floating_horizontal_dockspaces,
				&m_floating_vertical_dockspaces
			};
			bool is_horizontal = mask == 1.0f;
			const CapacityStream<UIDockspace>* search1 = dockspaces[is_horizontal];
			const CapacityStream<UIDockspace>* search2 = dockspaces[is_horizontal + 2];
			const UIDockspace* type = dockspaces[1 - is_horizontal]->buffer;

			for (size_t index = 0; index < search1->size; index++) {
				size_t border_count = search1->buffer[index].borders.size;
				UIDockspaceBorder* borders = search1->buffer[index].borders.buffer;
				for (size_t subindex = 0; subindex < border_count - 1; subindex++) {
					if (borders[subindex].is_dock) {
						if (&type[borders[subindex].window_indices[0]] == dockspace) {
							floating_type = DockspaceType(is_horizontal + 2);
							border_index = subindex;
							return GetFloatingDockspaceFromDockspace(&search1->buffer[index], 1.0f - is_horizontal, floating_type, border_index);
						}
					}
				}
			}
			for (size_t index = 0; index < search2->size; index++) {
				size_t border_count = search2->buffer[index].borders.size;
				UIDockspaceBorder* borders = search2->buffer[index].borders.buffer;
				for (size_t subindex = 0; subindex < border_count - 1; subindex++) {
					if (borders[subindex].is_dock) {
						if (&type[borders[subindex].window_indices[0]] == dockspace) {
							floating_type = DockspaceType(is_horizontal + 2);
							border_index = subindex;
							return &search2->buffer[index];
						}
					}
				}
			}
			return dockspace;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::GetLastRevertCommand(HandlerCommand& command, unsigned int window_index)
		{
			auto handler = GetDefaultWindowHandlerData(window_index);
			return handler->PeekRevertCommand(command);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetMouseDelta(float2 mouse_position) const {
			return float2(mouse_position.x - m_previous_mouse_position.x, mouse_position.y - m_previous_mouse_position.y);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		template<bool horizontal>
		float2 UISystem::GetTextSpan(
			Stream<char> characters,
			float font_size_x,
			float font_size_y,
			float character_spacing
		) const {
			float2 text_span = { 0.0f, 0.0f };

			float2 atlas_dimensions = m_font_character_uvs[m_descriptors.font.texture_dimensions];
			float2 sprite_scale_factor = { atlas_dimensions.x * font_size_x, atlas_dimensions.y * font_size_y };
			float new_character_spacing;

			if constexpr (!horizontal) {
				new_character_spacing = character_spacing * 0.1f;
			}

			if constexpr (horizontal) {
				text_span.y = GetTextSpriteYScale(font_size_y);
			}

			for (size_t index = 0; index < characters.size; index++) {
				unsigned int character_uv_index = FindCharacterType(characters[index]);

				size_t character_index = character_uv_index * 2;
				// they are in uv space
				float2 top_left = m_font_character_uvs[character_index];
				float2 bottom_right = m_font_character_uvs[character_index + 1];
				float2 uv = float2(bottom_right.x - top_left.x, bottom_right.y - top_left.y);

				float2 scale = float2(uv.x * sprite_scale_factor.x, uv.y * sprite_scale_factor.y);

				if constexpr (horizontal) {
					text_span.x += scale.x + character_spacing;
				}
				else {
					text_span.y += scale.y + new_character_spacing;
					text_span.x = std::max(text_span.x, scale.x);
				}
			}
			if constexpr (horizontal) {
				text_span.x -= character_spacing;
			}
			return text_span;
		}

		ECS_TEMPLATE_FUNCTION_BOOL_CONST(float2, UISystem::GetTextSpan, Stream<char>, float, float, float);

		// -----------------------------------------------------------------------------------------------------------------------------------

		float UISystem::GetTextSpriteNDC_YScale() const
		{
			unsigned int width, height;
			m_graphics->GetWindowSize(width, height);
			return m_font_character_uvs[m_descriptors.font.texture_dimensions].y * (m_font_character_uvs[3].y - m_font_character_uvs[0].y) / height;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float UISystem::GetTextSpriteYScale(float font_size_y) const
		{
			float2 atlas_dimensions = m_font_character_uvs[m_descriptors.font.texture_dimensions];
			float sprite_scale_factor = atlas_dimensions.y * font_size_y;
			return (m_font_character_uvs[3].y - m_font_character_uvs[0].y) * sprite_scale_factor;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float UISystem::GetTextSpriteSizeToScale(float scale) const
		{
			return scale / GetTextSpriteYScale(m_descriptors.font.size) * m_descriptors.font.size;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetTextSpriteSize(float size) const {
			return { size * ECS_TOOLS_UI_FONT_X_FACTOR, size };
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::GetVisibleDockspaceRegions(Stream<UIVisibleDockspaceRegion>& regions)
		{
			regions.size = 0;
			UIDockspace* dockspaces[4] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			const float offset_mask[4] = {
				1.0f,
				0.0f,
				1.0f,
				0.0f
			};

			const UIDockspace* dockspace_ptr[64];
			unsigned int dockspace_count = 0;

			unsigned int children_dockspace_indices[64];
			DockspaceType children_types[64];

			for (size_t index = 0; index < m_dockspace_layers.size; index++) {
				unsigned int children_dockspace_count = 0;
				GetDockspacesFromParent(m_dockspace_layers[index].index, m_dockspace_layers[index].type, children_dockspace_indices, children_types, children_dockspace_count);
				// children dockspaces windows
				for (size_t subindex = 0; subindex < children_dockspace_count; subindex++) {
					UIDockspace* dockspace = &dockspaces[(unsigned int)children_types[subindex]][children_dockspace_indices[subindex]];
					for (size_t border_index = 0; border_index < dockspace->borders.size - 1; border_index++) {
						if (!dockspace->borders[border_index].is_dock) {
							size_t floating_dockspace_index = 0;
							const UIElementTransform* window_transform = &m_windows[dockspace->borders[border_index].window_indices[dockspace->borders[border_index].active_window]].transform;
							for (; floating_dockspace_index < dockspace_count; floating_dockspace_index++) {
								if (
									IsRectangleInRectangle(
										*window_transform,
										dockspace_ptr[floating_dockspace_index]->transform
									)
									)
								{
									break;
								}

							}
							if (floating_dockspace_index >= dockspace_count) {
								regions[regions.size].dockspace = dockspace;
								regions[regions.size].border_index = border_index;
								regions[regions.size].type = children_types[subindex];
								regions[regions.size++].layer = index;
							}
						}
					}
				}
				// current floating dockspace windows
				UIDockspace* current_dockspace = &dockspaces[(unsigned int)m_dockspace_layers[index].type][m_dockspace_layers[index].index];
				bool is_fixed = IsFixedDockspace(current_dockspace);
				if (!is_fixed || current_dockspace->borders.size > 1) {
					for (size_t border_index = 0; border_index < current_dockspace->borders.size - 1; border_index++) {
						if (!current_dockspace->borders[border_index].is_dock) {
							size_t floating_dockspace_index = 0;
							for (; floating_dockspace_index < dockspace_count; floating_dockspace_index++) {
								UIElementTransform transform = {GetDockspaceRegionPosition(current_dockspace, border_index, offset_mask[(unsigned int)m_dockspace_layers[index].index]), GetDockspaceRegionScale(current_dockspace, border_index, offset_mask[(unsigned int)m_dockspace_layers[index].type])};
								if (
									IsRectangleInRectangle(
										transform,
										dockspace_ptr[floating_dockspace_index]->transform
									)
									)
								{
									break;
								}

							}
							if (floating_dockspace_index >= dockspace_count) {
								regions[regions.size].dockspace = current_dockspace;
								regions[regions.size].border_index = border_index;
								regions[regions.size].type = m_dockspace_layers[index].type;
								regions[regions.size++].layer = index;
							}
						}
					}
				}
				else {
					regions[regions.size].dockspace = current_dockspace;
					regions[regions.size].border_index = 0;
					regions[regions.size].type = m_dockspace_layers[index].type;
					regions[regions.size++].layer = index;
				}
				size_t floating_dockspace_index = 0;
				for (; floating_dockspace_index < dockspace_count; floating_dockspace_index++) {
					if (
						IsRectangleInRectangle(
							dockspaces[(unsigned int)m_dockspace_layers[index].type][m_dockspace_layers[index].index].transform,
							dockspace_ptr[floating_dockspace_index]->transform
						)
					) {
						break;
					}
				}
				if (floating_dockspace_index >= dockspace_count) {
					dockspace_ptr[dockspace_count++] = &dockspaces[(unsigned int)m_dockspace_layers[index].type][m_dockspace_layers[index].index];
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::GetVisibleDockspaceRegions(Stream<UIVisibleDockspaceRegion>& regions, bool from_lowest_layer_to_highest)
		{
			GetVisibleDockspaceRegions(regions);
			for (size_t index = 0; index < regions.size >> 1; index++) {
				UIVisibleDockspaceRegion copy = regions[index];
				regions[index] = regions[regions.size - index - 1];
				regions[regions.size - index - 1] = copy;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetOuterDockspaceBorderPosition(const UIDockspace* dockspace, ECS_UI_BORDER_TYPE type) const
		{
			unsigned int width, height;
			m_graphics->GetWindowSize(width, height);
			float horizontal_half_margin = m_descriptors.dockspaces.border_margin * 0.5f * height / width;
			float vertical_half_margin = m_descriptors.dockspaces.border_margin * 0.5f;
			float horizontal_border_size = m_descriptors.dockspaces.border_size * height / width;
			float vertical_border_size = m_descriptors.dockspaces.border_size;
			switch (type) {
			case ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP:
				return {
					dockspace->transform.position.x, dockspace->transform.position.y - vertical_half_margin + vertical_border_size * 0.5f
				};
			case ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT:
				return  {
					dockspace->transform.position.x - horizontal_half_margin + horizontal_border_size * 0.5f, dockspace->transform.position.y
				};
			case ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM:
				return {
					dockspace->transform.position.x, dockspace->transform.position.y + dockspace->transform.scale.y - vertical_half_margin + vertical_border_size * 0.5f
				};
			case ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT:
				return {
					dockspace->transform.position.x + dockspace->transform.scale.x - horizontal_half_margin + horizontal_border_size * 0.5f, dockspace->transform.position.y
				};
			}

			return { 0.0f, 0.0f };
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetOuterDockspaceBorderScale(const UIDockspace* dockspace, ECS_UI_BORDER_TYPE type) const {
			unsigned int width, height;
			m_graphics->GetWindowSize(width, height);
			float horizontal_border_size = m_descriptors.dockspaces.border_size * height / width;
			float vertical_border_size = m_descriptors.dockspaces.border_size;
			float horizontal_margin = m_descriptors.dockspaces.border_margin * height / width - horizontal_border_size;
			float vertical_margin = m_descriptors.dockspaces.border_margin - vertical_border_size;
			float2 sizes[2] = { 
				{dockspace->transform.scale.x, vertical_margin}, 
				{horizontal_margin, dockspace->transform.scale.y} 
			};
			bool is_horizontal = ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT == type || ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT == type;
			return sizes[is_horizontal];
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetInnerDockspaceBorderPosition(
			const UIDockspace* dockspace, 
			unsigned int border_index, 
			DockspaceType type
		) const
		{
			const float masks[4] = { 1.0f, 0.0f, 1.0f, 0.0f };
			float mask = masks[(unsigned int)type];
			return GetInnerDockspaceBorderPosition(dockspace, border_index, mask);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetInnerDockspaceBorderPosition(
			const UIDockspace* dockspace,
			unsigned int border_index,
			float mask
		) const
		{
			float inverse_mask = 1.0f - mask;
			float normalized_values[2] = { m_descriptors.dockspaces.border_size, m_descriptors.dockspaces.border_margin };
			NormalizeHorizontalToWindowDimensions(normalized_values, 2);
			float horizontal_margin = 0.5f * (normalized_values[1]);
			float vertical_margin = 0.5f * (m_descriptors.dockspaces.border_margin);
			return {
				dockspace->transform.position.x + normalized_values[0] + mask * (dockspace->borders[border_index].position - horizontal_margin),
				dockspace->transform.position.y + m_descriptors.dockspaces.border_size + inverse_mask * (dockspace->borders[border_index].position - vertical_margin)
			};
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetInnerDockspaceBorderScale(
			const UIDockspace* dockspace,
			unsigned int border_index,
			DockspaceType type
		) const {
			const float masks[4] = { 1.0f, 0.0f, 1.0f, 0.0f };
			float mask = masks[(unsigned int)type];
			return GetInnerDockspaceBorderScale(dockspace, border_index, mask);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetInnerDockspaceBorderScale(
			const UIDockspace* dockspace,
			unsigned int border_index,
			float mask
		) const {
			float inverse_mask = 1.0f - mask;
			float normalized_values[2] = { m_descriptors.dockspaces.border_size, m_descriptors.dockspaces.border_margin };
			NormalizeHorizontalToWindowDimensions(normalized_values, 2);
			float horizontal_scale = normalized_values[1] - normalized_values[0] * 2.0f;
			float vertical_scale = m_descriptors.dockspaces.border_margin - m_descriptors.dockspaces.border_size * 2.0f;
			return  {
				(dockspace->transform.scale.x - normalized_values[0] * 2.0f) * inverse_mask + mask * horizontal_scale,
				(dockspace->transform.scale.y - m_descriptors.dockspaces.border_size * 2.0f) * mask + inverse_mask * vertical_scale
			};
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::GetDockspaceRegionCollapseTriangleTransform(
			const UIDockspace* dockspace,
			unsigned int border_index,
			float offset_mask,
			float2& position, 
			float2& scale
		)
		{
			float2 region_position = GetDockspaceRegionPosition(dockspace, border_index, offset_mask);
			scale = GetSquareScale(m_descriptors.dockspaces.collapse_triangle_scale);
			position = {
				AlignMiddle(region_position.x, m_descriptors.dockspaces.region_header_padding, scale.x),
				AlignMiddle(region_position.y, m_descriptors.misc.title_y_scale, scale.y)
			};
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetDockspaceRegionFromMouse(float2 mouse_position, UIDockspace** dockspace, DockspaceType& type) const {
			unsigned int floating_dockspace_index = GetFloatingDockspaceIndexFromMouseCoordinates(mouse_position, type);
			if (floating_dockspace_index != 0xFFFFFFFF) {
				unsigned int index = GetDockspaceRegionFromDockspace(
					mouse_position,
					dockspace,
					type,
					floating_dockspace_index,
					type
				);
				return index;
			}
			return 0xFFFFFFFF;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetDockspaceRegionFromDockspace(
			float2 mouse_position,
			UIDockspace** dockspace,
			DockspaceType& type,
			unsigned int dockspace_index,
			DockspaceType type_to_search
		) const {
			UIDockspace* dockspaces[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			const float masks[] = {
				1.0f,
				0.0f,
				1.0f,
				0.0f
			};
			const DockspaceType children_types[] = {
				DockspaceType::Vertical,
				DockspaceType::Horizontal,
				DockspaceType::Vertical,
				DockspaceType::Horizontal
			};
			UIDockspace* dockspace_to_search = &dockspaces[(unsigned int)type_to_search][dockspace_index];
			float mask = masks[(unsigned int)type_to_search];
			for (size_t index = 0; index < dockspace_to_search->borders.size - 1; index++) {
				if (!dockspace_to_search->borders[index].is_dock) {
					float2 region_position = GetDockspaceRegionPosition(dockspace_to_search, index, mask);
					float2 region_scale = GetDockspaceRegionScale(dockspace_to_search, index, mask);
					if (IsPointInRectangle(mouse_position, region_position, region_scale)) {
						*dockspace = dockspace_to_search;
						type = type_to_search;
						return index;
					}
				}
				else {
					unsigned int children_index = GetDockspaceRegionFromDockspace(
						mouse_position,
						dockspace,
						type,
						dockspace_to_search->borders[index].window_indices[0],
						children_types[(unsigned int)type_to_search]
					);
					if (children_index != 0xFFFFFFFF) {
						return children_index;
					}
				}
			}
			return 0xFFFFFFFF;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIDockspace* UISystem::GetDockspaceParent(const UIDockspace* dockspace, DockspaceType type, DockspaceType& parent_type)
		{

			// checking horizontal/vertical type, floating are excluded because they have no parent
			if (type == DockspaceType::Horizontal) {

				// floating
				for (size_t index = 0; index < m_floating_vertical_dockspaces.size; index++) {
					for (size_t subindex = 0; subindex < m_floating_vertical_dockspaces[index].borders.size; subindex++) {
						if (m_floating_vertical_dockspaces[index].borders[subindex].is_dock
							&& &m_horizontal_dockspaces[m_floating_vertical_dockspaces[index].borders[subindex].window_indices[0]] == dockspace) {
							parent_type = DockspaceType::FloatingVertical;
							return &m_floating_vertical_dockspaces[index];
						}
					}
				}

				// vertical
				for (size_t index = 0; index < m_vertical_dockspaces.size; index++) {
					for (size_t subindex = 0; subindex < m_vertical_dockspaces[index].borders.size; subindex++) {
						if (m_vertical_dockspaces[index].borders[subindex].is_dock
							&& &m_horizontal_dockspaces[m_vertical_dockspaces[index].borders[subindex].window_indices[0]] == dockspace) {
							parent_type = DockspaceType::Vertical;
							return &m_vertical_dockspaces[index];
						}
					}
				}
			}

			else if (type == DockspaceType::Vertical) {

				// floating
				for (size_t index = 0; index < m_floating_horizontal_dockspaces.size; index++) {
					for (size_t subindex = 0; subindex < m_floating_horizontal_dockspaces[index].borders.size; subindex++) {
						if (m_floating_horizontal_dockspaces[index].borders[subindex].is_dock
							&& &m_vertical_dockspaces[m_floating_horizontal_dockspaces[index].borders[subindex].window_indices[0]] == dockspace) {
							parent_type = DockspaceType::FloatingHorizontal;
							return &m_floating_horizontal_dockspaces[index];
						}
					}
				}

				// horizontal
				for (size_t index = 0; index < m_horizontal_dockspaces.size; index++) {
					for (size_t subindex = 0; subindex < m_horizontal_dockspaces[index].borders.size; subindex++) {
						if (m_horizontal_dockspaces[index].borders[subindex].is_dock
							&& &m_vertical_dockspaces[m_horizontal_dockspaces[index].borders[subindex].window_indices[0]] == dockspace) {
							parent_type = DockspaceType::Horizontal;
							return &m_horizontal_dockspaces[index];
						}
					}
				}
			}

			// if a floating type was gives, return nullptr
			return nullptr;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIDockspace* UISystem::GetDockspaceParent(
			const UIDockspace* dockspace, 
			DockspaceType type, 
			DockspaceType& parent_type, 
			unsigned int& border_index
		)
		{

			// checking horizontal/vertical type, floating are excluded because they have no parent
			if (type == DockspaceType::Horizontal) {

				// floating
				for (size_t index = 0; index < m_floating_vertical_dockspaces.size; index++) {
					for (size_t subindex = 0; subindex < m_floating_vertical_dockspaces[index].borders.size; subindex++) {
						if (m_floating_vertical_dockspaces[index].borders[subindex].is_dock
							&& &m_horizontal_dockspaces[m_floating_vertical_dockspaces[index].borders[subindex].window_indices[0]] == dockspace) {
							parent_type = DockspaceType::FloatingVertical;
							border_index = subindex;
							return &m_floating_vertical_dockspaces[index];
						}
					}
				}

				// vertical
				for (size_t index = 0; index < m_vertical_dockspaces.size; index++) {
					for (size_t subindex = 0; subindex < m_vertical_dockspaces[index].borders.size; subindex++) {
						if (m_vertical_dockspaces[index].borders[subindex].is_dock
							&& &m_horizontal_dockspaces[m_vertical_dockspaces[index].borders[subindex].window_indices[0]] == dockspace) {
							parent_type = DockspaceType::Vertical;
							border_index = subindex;
							return &m_vertical_dockspaces[index];
						}
					}
				}
			}

			else if (type == DockspaceType::Vertical) {

				// floating
				for (size_t index = 0; index < m_floating_horizontal_dockspaces.size; index++) {
					for (size_t subindex = 0; subindex < m_floating_horizontal_dockspaces[index].borders.size; subindex++) {
						if (m_floating_horizontal_dockspaces[index].borders[subindex].is_dock
							&& &m_vertical_dockspaces[m_floating_horizontal_dockspaces[index].borders[subindex].window_indices[0]] == dockspace) {
							parent_type = DockspaceType::FloatingHorizontal;
							border_index = subindex;
							return &m_floating_horizontal_dockspaces[index];
						}
					}
				}

				// horizontal
				for (size_t index = 0; index < m_horizontal_dockspaces.size; index++) {
					for (size_t subindex = 0; subindex < m_horizontal_dockspaces[index].borders.size; subindex++) {
						if (m_horizontal_dockspaces[index].borders[subindex].is_dock
							&& &m_vertical_dockspaces[m_horizontal_dockspaces[index].borders[subindex].window_indices[0]] == dockspace) {
							parent_type = DockspaceType::Horizontal;
							border_index = subindex;
							return &m_horizontal_dockspaces[index];
						}
					}
				}
			}

			// if a floating type was gives, return nullptr
			return nullptr;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::GetDockspaceRegionsFromMouse(
			float2 mouse_position,
			UIDockspace** dockspaces,
			unsigned int* border_indices, 
			DockspaceType* types,
			unsigned int& count
		) const
		{
			count = 0;
			UIDockspace* docks[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			for (size_t index = 0; index < m_dockspace_layers.size; index++) {
				GetDockspaceRegionsFromMouse(
					mouse_position,
					&docks[(unsigned int)m_dockspace_layers[index].type][m_dockspace_layers[index].index], 
					m_dockspace_layers[index].type, 
					dockspaces, 
					border_indices,
					types,
					count
				);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::GetDockspaceRegionsFromMouse(
			float2 mouse_position, 
			UIDockspace* dockspace_to_search,
			DockspaceType type, 
			UIDockspace** dockspaces, 
			unsigned int* border_indices,
			DockspaceType* types,
			unsigned int& count
		) const
		{
			const float masks[] = { 1.0f, 0.0f, 1.0f, 0.0f };
			for (size_t index = 0; index < dockspace_to_search->borders.size - 1; index++) {
				if (!dockspace_to_search->borders[index].is_dock) {
					float2 region_position = GetDockspaceRegionPosition(dockspace_to_search, index, masks[(unsigned int)type]);
					float2 region_scale = GetDockspaceRegionScale(dockspace_to_search, index, masks[(unsigned int)type]);
					if (IsPointInRectangle(
						mouse_position,
						region_position,
						region_scale
					)) {
						dockspaces[count] = dockspace_to_search;
						border_indices[count] = index;
						types[count++] = type;
					}
				}
				else {
					UIDockspace* docks[] = {
						m_vertical_dockspaces.buffer,
						m_horizontal_dockspaces.buffer,
						m_vertical_dockspaces.buffer,
						m_horizontal_dockspaces.buffer
					};
					DockspaceType children_types[] = {
						DockspaceType::Vertical,
						DockspaceType::Horizontal,
						DockspaceType::Vertical,
						DockspaceType::Horizontal
					};
					GetDockspaceRegionsFromMouse(
						mouse_position, 
						&docks[(unsigned int)type][dockspace_to_search->borders[index].window_indices[0]], 
						children_types[(unsigned int)type],
						dockspaces, 
						border_indices, 
						types, 
						count
					);
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetDockspaceRegionHorizontalRenderSliderPosition(
			const UIDockspace* dockspace,
			unsigned int border_index, 
			float dockspace_mask
		) const
		{
			float2 region_position = GetDockspaceRegionPosition(dockspace, border_index, dockspace_mask);
			float2 region_scale = GetDockspaceRegionScale(dockspace, border_index, dockspace_mask);
			return GetDockspaceRegionHorizontalRenderSliderPosition(region_position, region_scale);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetDockspaceRegionHorizontalRenderSliderPosition(float2 region_position, float2 region_scale) const {
			float2 position;
			position.x = region_position.x;
			position.y = region_position.y + region_scale.y - m_descriptors.dockspaces.border_size - m_descriptors.misc.render_slider_horizontal_size;
			return position;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetDockspaceRegionHorizontalRenderSliderScale(
			const UIDockspace* dockspace, 
			unsigned int border_index,
			float dockspace_mask
		) const {
			float2 region_scale = GetDockspaceRegionScale(dockspace, border_index, dockspace_mask);
			return GetDockspaceRegionHorizontalRenderSliderScale(region_scale);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetDockspaceRegionHorizontalRenderSliderScale(float2 region_scale) const {
			float2 scale;
			scale.x = region_scale.x - NormalizeHorizontalToWindowDimensions(m_descriptors.dockspaces.border_size);
			scale.y = m_descriptors.misc.render_slider_horizontal_size;
			return scale;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetDockspaceRegionVerticalRenderSliderPosition(
			const UIDockspace* dockspace,
			unsigned int border_index,
			float dockspace_mask
		) const {
			float2 region_position = GetDockspaceRegionPosition(dockspace, border_index, dockspace_mask);
			float2 region_scale = GetDockspaceRegionScale(dockspace, border_index, dockspace_mask);
			return GetDockspaceRegionVerticalRenderSliderPosition(region_position, region_scale, dockspace, border_index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetDockspaceRegionVerticalRenderSliderPosition(
			float2 region_position,
			float2 region_scale,
			const UIDockspace* dockspace,
			unsigned int border_index
		) const {
			float2 position;
			position.x = region_position.x + region_scale.x - NormalizeHorizontalToWindowDimensions(m_descriptors.dockspaces.border_size) - m_descriptors.misc.render_slider_vertical_size;
			position.y = region_position.y + m_descriptors.misc.title_y_scale;
			if (!dockspace->borders[border_index].draw_region_header && !dockspace->borders[border_index].draw_close_x) {
				position.y -= m_descriptors.misc.title_y_scale;
			}
			return position;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetDockspaceRegionVerticalRenderSliderScale(
			const UIDockspace* dockspace,
			unsigned int border_index,
			float dockspace_mask
		) const {
			float2 region_scale = GetDockspaceRegionScale(dockspace, border_index, dockspace_mask);
			size_t window_index = dockspace->borders[border_index].window_indices[dockspace->borders[border_index].active_window];
			return GetDockspaceRegionVerticalRenderSliderScale(
				region_scale,
				dockspace,
				border_index,
				window_index
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetDockspaceRegionVerticalRenderSliderScale(float2 region_scale, const UIDockspace* dockspace, unsigned int border_index, unsigned int window_index) const {
			float2 scale;
			scale.x = m_descriptors.misc.render_slider_vertical_size;
			scale.y = region_scale.y - m_descriptors.misc.title_y_scale - m_descriptors.dockspaces.border_size;
			if (m_windows[window_index].is_horizontal_render_slider) {
				scale.y -= m_descriptors.window_layout.next_row_y_offset;
			}
			if (!dockspace->borders[border_index].draw_region_header && !dockspace->borders[border_index].draw_close_x) {
				scale.y += m_descriptors.misc.title_y_scale;
			}
			return scale;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIDockspace* UISystem::GetDockspace(unsigned int dockspace_index, DockspaceType type)
		{
			UIDockspace* dockspaces[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			return &dockspaces[(unsigned int)type][dockspace_index];
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		const UIDockspace* UISystem::GetConstDockspace(UIDockspaceLayer layer) const
		{
			const UIDockspace* dockspaces[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			return &dockspaces[(unsigned int)layer.type][layer.index];
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIDrawerDescriptor UISystem::GetDrawerDescriptor(unsigned int window_index)
		{
			UIDrawerDescriptor descriptor = InitializeDrawerDescriptorReferences(window_index);

			unsigned int border_index;
			DockspaceType type;
			UIDockspace* dockspace = GetDockspaceFromWindow(window_index, border_index, type);

			descriptor.border_index = border_index;
			descriptor.buffers = nullptr;
			descriptor.counts = nullptr;
			descriptor.dockspace = dockspace;
			descriptor.dockspace_type = type;
			descriptor.mouse_position = { 1.0f, 1.0f };
			descriptor.system = this;
			descriptor.system_buffers = nullptr;
			descriptor.system_counts = nullptr;
			descriptor.thread_id = 0;
			descriptor.window_index = window_index;
			descriptor.export_scale = nullptr;
			descriptor.do_not_initialize_viewport_sliders = false;
			descriptor.do_not_allocate_buffers = false;

			return descriptor;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void* UISystem::GetFrameHandlerData(unsigned int index) const
		{
			return m_frame_handlers[index].data;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetFrameHandlerCount() const
		{
			return m_frame_handlers.size;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::GetFixedDockspaceRegionsFromMouse(float2 mouse_position, UIDockspace** output_dockspaces, DockspaceType* types, unsigned int& count) const
		{
			UIDockspace* dockspaces[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			for (size_t index = 0; index < m_dockspace_layers.size; index++) {
				for (size_t subindex = 0; subindex < m_fixed_dockspaces.size; subindex++) {
					UIDockspace* current_dockspace = &dockspaces[(unsigned int)m_fixed_dockspaces[subindex].type][m_fixed_dockspaces[subindex].index];
					if (m_dockspace_layers[index].index == m_fixed_dockspaces[subindex].index && m_dockspace_layers[index].type == m_fixed_dockspaces[subindex].type) {
						if (current_dockspace->borders.size == 1 && IsPointInRectangle(
							mouse_position,
							current_dockspace->transform
						)) {
							output_dockspaces[count] = current_dockspace;
							types[count++] = m_fixed_dockspaces[subindex].type;
						}
					}
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		ActionData UISystem::GetFilledActionData(unsigned int window_index)
		{
			ActionData result;
			
			unsigned int border_index;
			DockspaceType type;
			UIDockspace* dockspace = GetDockspaceFromWindow(window_index, border_index, type);
			result.additional_data = nullptr;
			result.border_index = border_index;
			result.buffers = nullptr;
			result.counts = nullptr;
			result.data = nullptr;
			result.dockspace = dockspace;
			result.keyboard = m_keyboard;
			result.keyboard_tracker = m_keyboard_tracker;
			result.mouse = m_mouse;
			result.mouse_position = { -0.0f, -0.0f };
			result.mouse_tracker = m_mouse_tracker;
			result.position = { 0.0f, 0.0f };
			result.scale = { 0.0f, 0.0f };
			result.system = this;
			result.type = type;

			return result;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		size_t UISystem::GetFrameIndex() const
		{
			return m_frame_index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetWindowFromMouse(float2 mouse_position) const {
			DockspaceType type;
			unsigned int floating_dockspace_index = GetFloatingDockspaceIndexFromMouseCoordinates(mouse_position, type);
			if (floating_dockspace_index != 0xFFFFFFFF) {
				unsigned int index = GetWindowFromDockspace(
					mouse_position, 
					floating_dockspace_index, 
					type
				);
				return index;
			}
			return 0xFFFFFFFF;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetWindowDynamicElement(unsigned int window_index, Stream<char> identifier) const
		{
			return m_windows[window_index].dynamic_resources.Find(identifier);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIWindowDynamicResource* UISystem::GetWindowDynamicElement(unsigned int window_index, unsigned int index)
		{
			return m_windows[window_index].dynamic_resources.GetValuePtrFromIndex(index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::HandleFocusedWindowClickable(float2 mouse_position, unsigned int thread_id) {
			ActionData action_data;
			action_data.system = this;
			action_data.dockspace = m_focused_window_data.active_location.dockspace;
			action_data.border_index = m_focused_window_data.active_location.border_index;
			action_data.buffers = m_focused_window_data.buffers;
			action_data.counts = m_focused_window_data.counts;
			action_data.type = m_focused_window_data.active_location.type;
			action_data.mouse_position = mouse_position;
			action_data.keyboard_tracker = m_keyboard_tracker;
			action_data.mouse_tracker = m_mouse_tracker;
			action_data.mouse = m_mouse;
			action_data.keyboard = m_keyboard;
			action_data.additional_data = nullptr;

			if (m_focused_window_data.clickable_handler.action == DefaultClickableAction) {
				action_data.additional_data = m_focused_window_data.additional_hoverable_data;
			}
			
			m_resources.thread_resources[thread_id].phase = m_focused_window_data.clickable_handler.phase;
			m_execute_events = !m_focused_window_data.ExecuteClickableHandler(&action_data);
			m_frame_pacing = (!m_execute_events && m_frame_pacing < ECS_UI_FRAME_PACING_MEDIUM) ? ECS_UI_FRAME_PACING_MEDIUM : m_frame_pacing;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::HandleHoverable(float2 mouse_position, unsigned int thread_id, void** buffers, size_t* counts)
		{
			ActionData action_data;
			action_data.system = this;
			action_data.dockspace = m_focused_window_data.hovered_location.dockspace;
			action_data.border_index = m_focused_window_data.hovered_location.border_index;
			action_data.buffers = buffers;
			action_data.counts = counts;
			action_data.type = m_focused_window_data.hovered_location.type;
			action_data.mouse_position = mouse_position;
			action_data.keyboard_tracker = m_keyboard_tracker;
			action_data.mouse_tracker = m_mouse_tracker;
			action_data.keyboard = m_keyboard;
			action_data.mouse = m_mouse;
			action_data.additional_data = m_focused_window_data.additional_hoverable_data;

			m_resources.thread_resources[thread_id].phase = m_focused_window_data.hoverable_handler.phase;
			bool executed = m_focused_window_data.ExecuteHoverableHandler(&action_data);
			m_frame_pacing = (executed && m_frame_pacing < ECS_UI_FRAME_PACING_LOW) ? ECS_UI_FRAME_PACING_LOW : m_frame_pacing;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::HandleFocusedWindowGeneral(float2 mouse_position, unsigned int thread_id) {
			ActionData action_data;
			action_data.system = this;
			action_data.dockspace = m_focused_window_data.active_location.dockspace;
			action_data.border_index = m_focused_window_data.active_location.border_index;
			action_data.buffers = m_focused_window_data.buffers;
			action_data.counts = m_focused_window_data.counts;
			action_data.type = m_focused_window_data.active_location.type;
			action_data.mouse_position = mouse_position;
			action_data.keyboard_tracker = m_keyboard_tracker;
			action_data.mouse_tracker = m_mouse_tracker;
			action_data.keyboard = m_keyboard;
			action_data.mouse = m_mouse;
			action_data.additional_data = nullptr;

			m_resources.thread_resources[thread_id].phase = m_focused_window_data.general_handler.phase;
			bool executed = m_focused_window_data.ExecuteGeneralHandler(&action_data);
			m_frame_pacing = (executed && m_frame_pacing < ECS_UI_FRAME_PACING_MEDIUM) ? ECS_UI_FRAME_PACING_MEDIUM : m_frame_pacing;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::HandleFocusedWindowCleanupGeneral(
			float2 mouse_position,
			unsigned int thread_id,
			void* additional_data
		)
		{
			if (m_focused_window_data.clean_up_call_general) {
				ActionData action_data;
				action_data.additional_data = additional_data;
				action_data.border_index = m_focused_window_data.cleanup_general_location.border_index;
				action_data.dockspace = m_focused_window_data.cleanup_general_location.dockspace;
				action_data.buffers = nullptr;
				action_data.counts = nullptr;
				action_data.data = m_focused_window_data.general_handler.data;
				action_data.keyboard = m_keyboard;
				action_data.keyboard_tracker = m_keyboard_tracker;
				action_data.mouse = m_mouse;
				action_data.mouse_position = mouse_position;
				action_data.mouse_tracker = m_mouse_tracker;
				action_data.system = this;
				action_data.type = m_focused_window_data.cleanup_general_location.type;
				action_data.position = m_focused_window_data.general_transform.position;
				action_data.scale = m_focused_window_data.general_transform.scale;

				m_focused_window_data.general_handler.action(&action_data);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::HandleFocusedWindowCleanupHoverable(float2 mouse_position, unsigned int thread_id, void* additional_data)
		{
			if (m_focused_window_data.clean_up_call_hoverable) {
				ActionData action_data;
				action_data.additional_data = additional_data;
				action_data.border_index = m_focused_window_data.cleanup_hoverable_location.border_index;
				action_data.dockspace = m_focused_window_data.cleanup_hoverable_location.dockspace;
				action_data.buffers = nullptr;
				action_data.counts = nullptr;
				action_data.data = m_focused_window_data.hoverable_handler.data;
				action_data.keyboard = m_keyboard;
				action_data.keyboard_tracker = m_keyboard_tracker;
				action_data.mouse = m_mouse;
				action_data.mouse_position = mouse_position;
				action_data.mouse_tracker = m_mouse_tracker;
				action_data.system = this;
				action_data.type = m_focused_window_data.cleanup_hoverable_location.type;
				action_data.position = m_focused_window_data.hoverable_transform.position;
				action_data.scale = m_focused_window_data.hoverable_transform.scale;

				m_focused_window_data.hoverable_handler.action(&action_data);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::HandleDockingGizmoTransparentHover(
			UIDockspace* dockspace,
			unsigned int border_index,
			float2 rectangle_position, 
			float2 rectangle_scale, 
			void** system_buffers, 
			size_t* system_counts
		)
		{
			Color lightened_color = LightenColorClamp(m_descriptors.color_theme.docking_gizmo_background, 1.3f);
			SetSprite(
				dockspace,
				border_index,
				ECS_TOOLS_UI_TEXTURE_MASK,
				rectangle_position,
				rectangle_scale,
				system_buffers,
				system_counts,
				Color(
					lightened_color.red,
					lightened_color.green,
					lightened_color.blue,
					ECS_TOOLS_UI_DOCKING_GIZMO_TRANSPARENT_HOVER_ALPHA
				),
				{ 0.0f, 0.0f },
				{ 1.0f, 1.0f },
				ECS_UI_DRAW_PHASE::ECS_UI_DRAW_SYSTEM
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetWindowFromDockspace(float2 mouse_position, unsigned int dockspace_index, DockspaceType type) const {
			const UIDockspace* dockspaces[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			const float masks[] = {
				1.0f,
				0.0f,
				1.0f,
				0.0f
			};
			const DockspaceType children_types[] = {
				DockspaceType::Vertical,
				DockspaceType::Horizontal,
				DockspaceType::Vertical,
				DockspaceType::Horizontal
			};
			const UIDockspace* dockspace = &dockspaces[(unsigned int)type][dockspace_index];
			float mask = masks[(unsigned int)type];
			for (size_t index = 0; index < dockspace->borders.size - 1; index++) {
				if (!dockspace->borders[index].is_dock) {
					float2 region_position = GetDockspaceRegionPosition(dockspace, index, mask);
					float2 region_scale = GetDockspaceRegionScale(dockspace, index, mask);
					if (IsPointInRectangle(mouse_position, region_position, region_scale)) {
						return dockspace->borders[index].window_indices[dockspace->borders[index].active_window];
					}
				}
				else {
					unsigned int children_index = GetWindowFromDockspace(
						mouse_position,
						dockspace->borders[index].window_indices[0],
						children_types[(unsigned int)type]
					);
					if (children_index != 0xFFFFFFFF) {
						return children_index;
					}
				}
			}
			return 0xFFFFFFFF;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetWindowFromName(Stream<char> name) const
		{
			for (size_t index = 0; index < m_windows.size; index++) {
				Stream<char> window_name = GetWindowName(index);
				if (function::CompareStrings(name, window_name)) {
					return index;
				}
			}
			return 0xFFFFFFFF;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		WindowTable* UISystem::GetWindowTable(unsigned int window_index)
		{
			return &m_windows[window_index].table;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		Stream<char> UISystem::GetWindowName(unsigned int window_index) const
		{
			return m_windows[window_index].name;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIWindow* UISystem::GetWindowPointer(unsigned int window_index) {
			return &m_windows[window_index];
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIDefaultWindowHandler* UISystem::GetDefaultWindowHandlerData(unsigned int window_index)
		{
			return (UIDefaultWindowHandler*)m_windows[window_index].default_handler.data;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void* UISystem::GetWindowPrivateHandlerData(unsigned int window_index)
		{
			return m_windows[window_index].private_handler.data;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void* UISystem::GetWindowData(unsigned int window_index)
		{
			return m_windows[window_index].window_data;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetActiveWindowIndexInBorder(const UIDockspace* dockspace, unsigned int border_index) const
		{
			return dockspace->borders[border_index].active_window;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetActiveWindow() const
		{
			return GetWindowIndexFromBorder(m_focused_window_data.active_location.dockspace, m_focused_window_data.active_location.border_index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::GetWindowIndexFromBorder(const UIDockspace* dockspace, unsigned int border_index) const
		{
			return dockspace->borders[border_index].window_indices[GetActiveWindowIndexInBorder(dockspace, border_index)];
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetWindowPosition(unsigned int window_index)
		{
			return m_windows[window_index].transform.position;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetWindowScale(unsigned int window_index)
		{
			return m_windows[window_index].transform.scale;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetWindowRenderRegion(unsigned int window_index)
		{
			return { 
				m_windows[window_index].render_region_offset.x, 
				m_windows[window_index].render_region_offset.y
			};
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float4 UISystem::GetUVForCharacter(char character) const
		{
			unsigned int uv_index = FindCharacterType(character);
			return float4(m_font_character_uvs[uv_index * 2].x, m_font_character_uvs[uv_index * 2].y, m_font_character_uvs[uv_index * 2 + 1].x, m_font_character_uvs[uv_index * 2 + 1].y);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UISpriteTexture* UISystem::GetNextSpriteTextureToDraw(UIDockspace* dockspace, unsigned int border_index, ECS_UI_DRAW_PHASE phase, ECS_UI_SPRITE_TYPE type)
		{
			if (phase == ECS_UI_DRAW_PHASE::ECS_UI_DRAW_SYSTEM) {
				auto stream = &m_resources.system_draw.sprite_textures[(unsigned int)type];
				unsigned int before_size = stream->ReserveNewElement();
				stream->size++;
				return stream->buffer + before_size;
			}
			else {
				auto stream = &dockspace->borders[border_index].draw_resources.sprite_textures[(unsigned int)phase * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS + (unsigned int)type];
				unsigned int before_size = stream->ReserveNewElement();
				stream->size++;
				return stream->buffer + before_size;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::HandleDockingGizmoAdditionOfDockspace(
			UIDockspace* dockspace_receiver, 
			unsigned int border_index_receiver,
			DockspaceType type_receiver, 
			unsigned int addition_type,
			UIDockspace* element_to_add,
			DockspaceType element_type
		)
		{
			ECS_ASSERT(element_type == DockspaceType::FloatingHorizontal || element_type == DockspaceType::FloatingVertical);
			
			auto copy_multiple_windows = [](
				UIDockspace* dockspace_receiver,
				unsigned int border_index_receiver,
				UIDockspace* element_to_add,
				unsigned int element_border
			) {
				for (size_t index = 1; index < element_to_add->borders[element_border].window_indices.size; index++) {
					dockspace_receiver->borders[border_index_receiver].window_indices[index] = element_to_add->borders[element_border].window_indices[index];
				}
				dockspace_receiver->borders[border_index_receiver].window_indices.size = element_to_add->borders[element_border].window_indices.size;
				dockspace_receiver->borders[border_index_receiver].draw_region_header = element_to_add->borders[element_border].draw_region_header;
				dockspace_receiver->borders[border_index_receiver].draw_elements = element_to_add->borders[element_border].draw_elements;
				dockspace_receiver->borders[border_index_receiver].draw_close_x = element_to_add->borders[element_border].draw_close_x;
			};

			auto handle_fixed_dockspace_empty = [](
				UISystem* system,
				UIDockspace* dockspace_receiver,
				DockspaceType type_receiver,
				UIDockspace* element_to_add,
				DockspaceType element_type
			) {
				UIDockspace* dockspaces[] = {
					system->m_horizontal_dockspaces.buffer,
					system->m_vertical_dockspaces.buffer,
					system->m_floating_horizontal_dockspaces.buffer,
					system->m_floating_vertical_dockspaces.buffer
				};
				
				// Get the background index
				unsigned int background_layer_index = -1;
				for (size_t index = 0; index < system->m_background_dockspaces.size; index++) {
					if (system->GetDockspace(system->m_background_dockspaces[index].index, system->m_background_dockspaces[index].type) == dockspace_receiver) {
						background_layer_index = index;
						break;
					}
				}

				if (element_type == DockspaceType::FloatingHorizontal) {
					system->AppendDockspaceResize(element_to_add, dockspace_receiver->transform.scale.x, element_to_add->transform.scale.x, dockspace_receiver->transform.scale.y, 1.0f);
				}
				else {
					system->AppendDockspaceResize(element_to_add, dockspace_receiver->transform.scale.y, element_to_add->transform.scale.y, dockspace_receiver->transform.scale.x, 0.0f);
				}

				float2 position = dockspace_receiver->transform.position;
				system->DestroyDockspace(dockspace_receiver->borders.buffer, type_receiver);
				
				unsigned int new_index = system->CreateFixedDockspace(element_to_add, element_type);
				UIDockspace* dockspace = &dockspaces[(unsigned int)element_type][new_index];
				dockspace->transform.position = position;
				system->SetNewFocusedDockspace(dockspace, element_type);
				system->SearchAndSetNewFocusedDockspaceRegion(dockspace, 0, element_type);

				// transfer background 
				if (background_layer_index != -1) {
					system->m_background_dockspaces[background_layer_index].index = new_index;
					system->m_background_dockspaces[background_layer_index].type = element_type;
				}
			};

			// it will convert from floating to non-floating and add to the specified border
			auto border_add = [&](
				UISystem* system,
				UIDockspace* dockspace_receiver,
				unsigned int border_index_receiver,
				DockspaceType type_receiver,
				UIDockspace* element_to_add,
				DockspaceType element_type
			) {
				ECS_ASSERT(dockspace_receiver->borders.size < dockspace_receiver->borders.capacity);
				const DockspaceType replacement_type[] = {
					DockspaceType::Horizontal,
					DockspaceType::Vertical,
					DockspaceType::Horizontal,
					DockspaceType::Vertical
				};
				
				if (system->IsEmptyFixedDockspace(dockspace_receiver)) {
					handle_fixed_dockspace_empty(this, dockspace_receiver, type_receiver, element_to_add, element_type);
				}
				else if (element_to_add->borders.size > 2) {
					unsigned int new_dockspace_index = 0;
					new_dockspace_index = system->CreateDockspace(
						element_to_add,
						replacement_type[(unsigned int)element_type]
					);
					system->AddElementToDockspace(
						new_dockspace_index,
						true,
						border_index_receiver,
						type_receiver,
						dockspace_receiver
					);
					system->SetNewFocusedDockspace(dockspace_receiver, type_receiver);
					system->SearchAndSetNewFocusedDockspaceRegion(dockspace_receiver, border_index_receiver, type_receiver);
				}
				else {
					system->AddElementToDockspace(
						element_to_add->borders[0].window_indices[0],
						false,
						border_index_receiver,
						type_receiver,
						dockspace_receiver
					);
					copy_multiple_windows(dockspace_receiver, border_index_receiver, element_to_add, 0);
					system->SetNewFocusedDockspace(dockspace_receiver, type_receiver);
					system->SearchAndSetNewFocusedDockspaceRegion(dockspace_receiver, border_index_receiver, type_receiver);
				}
			};

			auto border_with_window_add = [&](
				UISystem* system,
				UIDockspace* dockspace_receiver,
				unsigned int border_index_receiver,
				DockspaceType type_receiver,
				UIDockspace* element_to_add,
				DockspaceType element_type
			) {
				ECS_ASSERT(dockspace_receiver->borders.size < dockspace_receiver->borders.capacity);
				const DockspaceType replacement_type[] = {
					DockspaceType::Horizontal,
					DockspaceType::Vertical,
					DockspaceType::Horizontal,
					DockspaceType::Vertical
				};

				if (system->IsEmptyFixedDockspace(dockspace_receiver)) {
					handle_fixed_dockspace_empty(this, dockspace_receiver, type_receiver, element_to_add, element_type);
				}
				else {
					unsigned int last_index = element_to_add->borders.size - 1;
					system->AddElementToDockspace(
						dockspace_receiver->borders[border_index_receiver].window_indices[0],
						false,
						last_index,
						element_type,
						element_to_add
					);
					copy_multiple_windows(element_to_add, last_index, dockspace_receiver, border_index_receiver);
					unsigned int new_dockspace_index = 0;
					new_dockspace_index = system->CreateDockspace(
						element_to_add,
						replacement_type[(unsigned int)element_type]
					);
					system->AddElementToDockspace(
						new_dockspace_index,
						true,
						border_index_receiver,
						type_receiver,
						dockspace_receiver
					);

					system->SetNewFocusedDockspace(dockspace_receiver, type_receiver);
					system->SearchAndSetNewFocusedDockspaceRegion(dockspace_receiver, border_index_receiver, type_receiver);
				}
			};

			auto per_element_border_add = [&](
				UISystem* system,
				UIDockspace* dockspace_receiver,
				unsigned int border_index_receiver,
				DockspaceType type_receiver,
				UIDockspace* element_to_add,
				DockspaceType element_type
			) {
				ECS_ASSERT(dockspace_receiver->borders.size + element_to_add->borders.size - 1 <= dockspace_receiver->borders.capacity);

				if (system->IsEmptyFixedDockspace(dockspace_receiver)) {
					handle_fixed_dockspace_empty(this, dockspace_receiver, type_receiver, element_to_add, element_type);
				}
				else {
					for (size_t index = 0; index < element_to_add->borders.size - 1; index++) {
						unsigned int receiver_index = border_index_receiver + index;
						system->AddElementToDockspace(
							element_to_add->borders[index].window_indices[0],
							element_to_add->borders[index].is_dock,
							receiver_index,
							type_receiver,
							dockspace_receiver
						);
						copy_multiple_windows(dockspace_receiver, receiver_index, element_to_add, index);
					}

					system->SetNewFocusedDockspace(dockspace_receiver, type_receiver);
					system->SearchAndSetNewFocusedDockspaceRegion(dockspace_receiver, 0, type_receiver);
				}
			};

			auto convert_floating_and_replace = [&](
				UISystem* system,
				UIDockspace* dockspace_receiver,
				unsigned int border_index_receiver,
				DockspaceType type_receiver,
				UIDockspace* element_to_add,
				DockspaceType element_type,
				unsigned int position_to_add_window
			) {
				const DockspaceType replacement_type[] = {
					DockspaceType::Horizontal,
					DockspaceType::Vertical,
					DockspaceType::Horizontal,
					DockspaceType::Vertical
				};

				if (system->IsEmptyFixedDockspace(dockspace_receiver)) {
					handle_fixed_dockspace_empty(this, dockspace_receiver, type_receiver, element_to_add, element_type);
				}
				else {
					system->AddElementToDockspace(
						dockspace_receiver->borders[border_index_receiver].window_indices[0],
						false,
						position_to_add_window,
						element_type,
						element_to_add
					);
					copy_multiple_windows(element_to_add, position_to_add_window, dockspace_receiver, border_index_receiver);
					unsigned int new_dockspace_index = 0;
					new_dockspace_index = system->CreateDockspace(
						element_to_add,
						replacement_type[(unsigned int)element_type]
					);
					system->AddElementToDockspace(
						new_dockspace_index,
						true,
						border_index_receiver,
						type_receiver,
						dockspace_receiver
					);
					system->SetNewFocusedDockspace(dockspace_receiver, type_receiver);
					system->SearchAndSetNewFocusedDockspaceRegion(dockspace_receiver, border_index_receiver, type_receiver);

					system->RemoveDockspaceBorder<false>(dockspace_receiver, border_index_receiver + 1, type_receiver);
				}
			};

			auto convert_floating_and_house_replace = [&](
				UISystem* system,
				float2 region_scale,
				UIDockspace* dockspace_receiver,
				unsigned int border_index_receiver,
				DockspaceType type_receiver,
				UIDockspace* element_to_add,
				DockspaceType element_type,
				unsigned int position_to_add_window
			) {
				const DockspaceType replacement_type[] = { 
					DockspaceType::Horizontal,
					DockspaceType::Vertical,
					DockspaceType::Horizontal, 
					DockspaceType::Vertical 
				};
				const DockspaceType new_create_type[] = {
					DockspaceType::Vertical,
					DockspaceType::Horizontal,
					DockspaceType::Vertical,
					DockspaceType::Horizontal
				};
				const DockspaceType floating_type[] = {
					DockspaceType::FloatingVertical,
					DockspaceType::FloatingHorizontal,
					DockspaceType::FloatingVertical,
					DockspaceType::FloatingHorizontal
				};
				UIDockspace* dockspaces[] = {
					system->m_horizontal_dockspaces.buffer,
					system->m_vertical_dockspaces.buffer,
					system->m_floating_horizontal_dockspaces.buffer,
					system->m_floating_vertical_dockspaces.buffer
				};

				bool is_fixed = system->IsFixedDockspace(dockspace_receiver);
				if (system->IsEmptyFixedDockspace(dockspace_receiver)) {
					handle_fixed_dockspace_empty(this, dockspace_receiver, type_receiver, element_to_add, element_type);
				}
				else {
					unsigned int new_dockspace_index = 0;
					unsigned int new_converted_index = 0;
					bool do_addition = true;
					if (element_to_add->borders.size > 2) {
						new_converted_index = system->CreateDockspace(
							element_to_add,
							replacement_type[(unsigned int)element_type]
						);
						if (dockspace_receiver->borders.size > 2) {
							new_dockspace_index = system->CreateDockspace(
								{ {0.0f, 0.0f}, region_scale },
								new_create_type[(unsigned int)element_type],
								new_converted_index,
								true
							);
						}
						else {
							if (is_fixed) {
								new_dockspace_index = system->CreateFixedDockspace(
									dockspace_receiver,
									floating_type[(unsigned int)type_receiver]
								);
							}
							else {
								new_dockspace_index = system->CreateDockspace(
									dockspace_receiver,
									floating_type[(unsigned int)type_receiver]
								);
							}
							UIDockspace* new_dockspace = &dockspaces[(unsigned int)floating_type[(unsigned int)type_receiver]][new_dockspace_index];
							
							float mask = GetDockspaceMaskFromType(floating_type[(unsigned int)type_receiver]);
							
							new_dockspace->borders[1].position = region_scale.x * mask + region_scale.y * (1.0f - mask);
							system->AddElementToDockspace(
								new_converted_index,
								true,
								1 - position_to_add_window,
								floating_type[(unsigned int)type_receiver],
								new_dockspace
							);

							system->SetNewFocusedDockspace(new_dockspace, floating_type[(unsigned int)type_receiver]);
							system->SearchAndSetNewFocusedDockspaceRegion(new_dockspace, 0, floating_type[(unsigned int)type_receiver]);

							if (system->IsBackgroundDockspace(dockspace_receiver)) {
								for (size_t index = 0; index < system->m_background_dockspaces.size; index++) {
									if (system->GetDockspace(system->m_background_dockspaces[index].index, system->m_background_dockspaces[index].type) == dockspace_receiver) {
										system->m_background_dockspaces[index].index = new_dockspace_index;
										system->m_background_dockspaces[index].type = floating_type[(unsigned int)type_receiver];
										break;
									}
								}
							}

							system->DestroyDockspace(dockspace_receiver->borders.buffer, type_receiver);
							do_addition = false;
						}
					}
					else {
						if ((type_receiver == DockspaceType::FloatingHorizontal || type_receiver == DockspaceType::FloatingVertical)
							&& !dockspace_receiver->borders[0].is_dock && !element_to_add->borders[0].is_dock) {
							if (dockspace_receiver->borders.size == 2) {
								unsigned int new_floating_dockspace_index = 0;
								if (is_fixed) {
									new_floating_dockspace_index = system->CreateFixedDockspace(
										dockspace_receiver->transform,
										floating_type[(unsigned int)element_type],
										element_to_add->borders[0].window_indices[0],
										false
									);
								}
								else {
									new_floating_dockspace_index = system->CreateDockspace(
										dockspace_receiver->transform,
										floating_type[(unsigned int)element_type],
										element_to_add->borders[0].window_indices[0],
										false
									);
								}
								UIDockspace* floating_dockspace = &dockspaces[(unsigned int)floating_type[(unsigned int)element_type]][new_floating_dockspace_index];
								copy_multiple_windows(
									floating_dockspace,
									0,
									element_to_add,
									0
								);
								floating_dockspace->borders[0].active_window = element_to_add->borders[0].active_window;
								system->AddElementToDockspace(
									dockspace_receiver->borders[0].window_indices[0],
									false,
									0,
									new_floating_dockspace_index,
									floating_type[(unsigned int)element_type]
								);
								copy_multiple_windows(
									floating_dockspace,
									1,
									dockspace_receiver,
									0
								);
								floating_dockspace->borders[1].active_window = dockspace_receiver->borders[0].active_window;

								system->SetNewFocusedDockspace(floating_dockspace, floating_type[(unsigned int)element_type]);
								system->SearchAndSetNewFocusedDockspaceRegion(floating_dockspace, 0, floating_type[(unsigned int)element_type]);

								if (system->IsBackgroundDockspace(dockspace_receiver)) {
									for (size_t index = 0; index < system->m_background_dockspaces.size; index++) {
										if (system->GetDockspace(system->m_background_dockspaces[index].index, system->m_background_dockspaces[index].type) == dockspace_receiver) {
											system->m_background_dockspaces[index].index = new_floating_dockspace_index;
											system->m_background_dockspaces[index].type = floating_type[(unsigned int)type_receiver];
											break;
										}
									}
								}

								system->DestroyDockspace(dockspace_receiver->borders.buffer, type_receiver);
								do_addition = false;
							}
							else if (dockspace_receiver->borders.size == 1) {
								system->AddElementToDockspace(
									element_to_add->borders[0].window_indices[0],
									element_to_add->borders[0].is_dock,
									0,
									type_receiver,
									dockspace_receiver
								);
								copy_multiple_windows(
									dockspace_receiver,
									0,
									element_to_add,
									0
								);

								system->SetNewFocusedDockspaceRegion(dockspace_receiver, 0, type_receiver);
								system->SetNewFocusedDockspace(dockspace_receiver, type_receiver);

								do_addition = false;
							}
							else {
								new_dockspace_index = system->CreateDockspace({ {0.0f, 0.0f}, region_scale }, new_create_type[(unsigned int)type_receiver], element_to_add->borders[0].window_indices[0], false);
							}
						}
						else {
							new_dockspace_index = system->CreateDockspace({ {0.0f, 0.0f}, region_scale }, new_create_type[(unsigned int)type_receiver], element_to_add->borders[0].window_indices[0], false);
						}
					}
					if (do_addition) {
						UIDockspace* new_dockspace = &dockspaces[(unsigned int)new_create_type[(unsigned int)element_type]][new_dockspace_index];
						copy_multiple_windows(
							new_dockspace,
							0,
							element_to_add,
							0
						);
						new_dockspace->borders[0].active_window = element_to_add->borders[0].active_window;

						system->AddElementToDockspace(
							dockspace_receiver->borders[border_index_receiver].window_indices[0],
							false,
							position_to_add_window,
							new_dockspace_index,
							new_create_type[(unsigned int)element_type]
						);
						copy_multiple_windows(new_dockspace, position_to_add_window, dockspace_receiver, border_index_receiver);

						system->AddElementToDockspace(
							new_dockspace_index,
							true,
							border_index_receiver,
							type_receiver,
							dockspace_receiver
						);

						system->SetNewFocusedDockspace(new_dockspace, new_create_type[(unsigned int)element_type]);
						system->SearchAndSetNewFocusedDockspaceRegion(new_dockspace, 0, new_create_type[(unsigned int)element_type]);

						system->RemoveDockspaceBorder<false>(dockspace_receiver, border_index_receiver + 1, type_receiver);
					}
				}
			};

			const float masks[] = { 1.0f, 0.0f, 1.0f, 0.0f };
			float2 region_scale;
			if (IsEmptyFixedDockspace(dockspace_receiver)) {
				region_scale = dockspace_receiver->transform.scale;
			}
			else {
				region_scale = GetDockspaceRegionScale(dockspace_receiver, border_index_receiver, masks[(unsigned int)type_receiver]);
			}
				
			// left
			if (addition_type == 0) {
				if (type_receiver == DockspaceType::FloatingHorizontal || type_receiver == DockspaceType::Horizontal) {
					if (element_type == DockspaceType::FloatingHorizontal) {
						per_element_border_add(this, dockspace_receiver, border_index_receiver, type_receiver, element_to_add, element_type);
					}
					else if (element_type == DockspaceType::FloatingVertical) {
						border_add(this, dockspace_receiver, border_index_receiver, type_receiver, element_to_add, element_type);
					}
				}
				else if (type_receiver == DockspaceType::FloatingVertical || type_receiver == DockspaceType::Vertical) {
					if (element_type == DockspaceType::FloatingHorizontal) {
						convert_floating_and_replace(
							this, 
							dockspace_receiver, 
							border_index_receiver, 
							type_receiver, 
							element_to_add,
							element_type, 
							element_to_add->borders.size - 1
						);
					}
					else if (element_type == DockspaceType::FloatingVertical) {
						convert_floating_and_house_replace(
							this, 
							region_scale, 
							dockspace_receiver,
							border_index_receiver, 
							type_receiver, 
							element_to_add,
							element_type, 
							1
						);
					}
				}
			}

			// top
			else if (addition_type == 1) {
				if (type_receiver == DockspaceType::FloatingHorizontal || type_receiver == DockspaceType::Horizontal) {
					if (element_type == DockspaceType::FloatingHorizontal) {
						convert_floating_and_house_replace(
							this, 
							region_scale, 
							dockspace_receiver, 
							border_index_receiver,
							type_receiver,
							element_to_add,
							element_type,
							1
						);
					}
					else if (element_type == DockspaceType::FloatingVertical) {
						convert_floating_and_replace(
							this,
							dockspace_receiver,
							border_index_receiver,
							type_receiver,
							element_to_add,
							element_type,
							element_to_add->borders.size - 1
						);
					}
				}

				else if (type_receiver == DockspaceType::FloatingVertical || type_receiver == DockspaceType::Vertical) {
					if (element_type == DockspaceType::FloatingHorizontal) {
						border_add(this, dockspace_receiver, border_index_receiver, type_receiver, element_to_add, element_type);
					}
					else if (element_type == DockspaceType::FloatingVertical) {
						per_element_border_add(this, dockspace_receiver, border_index_receiver, type_receiver, element_to_add, element_type);
					}
				}
			}

			// right
			else if (addition_type == 2) {
				if (type_receiver == DockspaceType::FloatingHorizontal || type_receiver == DockspaceType::Horizontal) {
					if (element_type == DockspaceType::FloatingHorizontal) {
						per_element_border_add(this, dockspace_receiver, border_index_receiver + 1, type_receiver, element_to_add, element_type);
					}
					else if (element_type == DockspaceType::FloatingVertical) {
						border_add(this, dockspace_receiver, border_index_receiver + 1, type_receiver, element_to_add, element_type);
					}
				}
				else if (type_receiver == DockspaceType::FloatingVertical || type_receiver == DockspaceType::Vertical) {
					if (element_type == DockspaceType::FloatingHorizontal) {
						convert_floating_and_replace(this, dockspace_receiver, border_index_receiver, type_receiver, element_to_add, element_type, 0);
					}
					else if (element_type == DockspaceType::FloatingVertical) {
						convert_floating_and_house_replace(
							this, 
							region_scale,
							dockspace_receiver,
							border_index_receiver, 
							type_receiver,
							element_to_add,
							element_type,
							0
						);
					}
				}
			}

			// bottom
			else if (addition_type == 3) {
				if (type_receiver == DockspaceType::FloatingHorizontal || type_receiver == DockspaceType::Horizontal) {
					if (element_type == DockspaceType::FloatingHorizontal) {
						convert_floating_and_house_replace(
							this,
							region_scale,
							dockspace_receiver,
							border_index_receiver,
							type_receiver,
							element_to_add,
							element_type,
							0
						);
					}
					else if (element_type == DockspaceType::FloatingVertical) {
						convert_floating_and_replace(
							this,
							dockspace_receiver,
							border_index_receiver,
							type_receiver,
							element_to_add,
							element_type,
							0
						);
					}
				}
				else if (type_receiver == DockspaceType::FloatingVertical || type_receiver == DockspaceType::Vertical) {
					if (element_type == DockspaceType::FloatingHorizontal) {
						border_add(this, dockspace_receiver, border_index_receiver + 1, type_receiver, element_to_add, element_type);
					}
					else if (element_type == DockspaceType::FloatingVertical) {
						per_element_border_add(this, dockspace_receiver, border_index_receiver + 1, type_receiver, element_to_add, element_type);
					}
				}
			}
			
			// central
			else if (addition_type == 4) {
			ECS_ASSERT(dockspace_receiver->borders[border_index_receiver].window_indices.size
				+ element_to_add->borders[0].window_indices.size < dockspace_receiver->borders[border_index_receiver].window_indices.capacity);
				AddWindowToDockspaceRegion(element_to_add->borders[0].window_indices[0], dockspace_receiver, border_index_receiver);
				unsigned int window_count = dockspace_receiver->borders[border_index_receiver].window_indices.size;
				copy_multiple_windows(dockspace_receiver, border_index_receiver, element_to_add, 0);
				dockspace_receiver->borders[border_index_receiver].window_indices.size = window_count;
			}

			DestroyDockspace(element_to_add->borders.buffer, element_type);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::HandleFrameHandlers()
		{
			if (m_frame_handlers.size > 0) {
				ActionData data;
				data.border_index = 0;
				data.buffers = nullptr;
				data.counts = nullptr;
				data.dockspace = nullptr;
				data.keyboard = m_keyboard;
				data.keyboard_tracker = m_keyboard_tracker;
				data.mouse = m_mouse;
				data.mouse_position = GetNormalizeMousePosition();
				data.mouse_tracker = m_mouse_tracker;
				data.position = { -10.0f, -10.0f };
				data.additional_data = nullptr;
				data.scale = { 0.0f, 0.0f };
				data.system = this;
				data.type = DockspaceType::FloatingHorizontal;

				// Create a temporary copy of those handlers - they might remove themselves and create bad dependencies
				unsigned int frame_handler_count = m_frame_handlers.size;
				UIActionHandler* handlers = (UIActionHandler*)ECS_STACK_ALLOC(sizeof(UIActionHandler) * frame_handler_count);
				m_frame_handlers.CopyTo(handlers);

				for (size_t index = 0; index < frame_handler_count; index++) {
					data.data = handlers[index].data;
					handlers[index].action(&data);
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::IncrementWindowDynamicResource(unsigned int window_index, Stream<char> name)
		{
			ResourceIdentifier identifier(name);
			UIWindowDynamicResource* resource = m_windows[window_index].dynamic_resources.GetValuePtr(identifier);
			resource->reference_count++;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::InitializeDefaultDescriptors()
		{
#pragma region Color Theme
			m_descriptors.color_theme.theme = ECS_TOOLS_UI_THEME_COLOR;
			m_descriptors.color_theme.background = ECS_TOOLS_UI_WINDOW_BACKGROUND_COLOR;
			m_descriptors.color_theme.borders = ECS_TOOLS_UI_WINDOW_BORDER_COLOR;
			m_descriptors.color_theme.darken_hover_factor = ECS_TOOLS_UI_DARKEN_HOVER_WHEN_PRESSED_FACTOR;
			m_descriptors.color_theme.text = ECS_TOOLS_UI_TEXT_COLOR;
			m_descriptors.color_theme.unavailable_text = ECS_TOOLS_UI_UNAVAILABLE_TEXT_COLOR;
			m_descriptors.color_theme.collapse_triangle = ECS_TOOLS_UI_WINDOW_REGION_HEADER_COLLAPSE_TRIANGLE_COLOR;
			m_descriptors.color_theme.region_header_x = ECS_TOOLS_UI_WINDOW_REGION_HEADER_CLOSE_X_COLOR;
			m_descriptors.color_theme.docking_gizmo_border = ECS_TOOLS_UI_DOCKING_GIZMO_BORDER_COLOR;
			m_descriptors.color_theme.region_header_active_window_factor = ECS_TOOLS_UI_WINDOW_REGION_HEADER_ACTIVE_WINDOW_LIGHTEN_FACTOR;
			m_descriptors.color_theme.slider_lighten_factor = ECS_TOOLS_UI_SLIDER_LIGHTEN_FACTOR;
			m_descriptors.color_theme.render_sliders_active_part = ECS_TOOLS_UI_RENDER_SLIDERS_ACTIVE_PART_COLOR;
			m_descriptors.color_theme.render_sliders_background = ECS_TOOLS_UI_RENDER_SLIDERS_BACKGROUND_COLOR;
			m_descriptors.color_theme.select_text_factor = ECS_TOOLS_UI_TEXT_INPUT_SELECT_FACTOR;
			m_descriptors.color_theme.check_box_factor = ECS_TOOLS_UI_CHECK_BOX_LIGTHEN_FACTOR;
			m_descriptors.color_theme.hierarchy_drag_node_bar = ECS_TOOLS_UI_HIERARCHY_DRAG_NODE_COLOR;
			m_descriptors.color_theme.graph_line = ECS_TOOLS_UI_GRAPH_LINE_COLOR;
			m_descriptors.color_theme.graph_hover_line = ECS_TOOLS_UI_GRAPH_HOVER_LINE_COLOR;
			m_descriptors.color_theme.graph_sample_circle = ECS_TOOLS_UI_GRAPH_SAMPLE_CIRCLE_COLOR;
			m_descriptors.color_theme.histogram_color = ECS_TOOLS_UI_HISTOGRAM_COLOR;
			m_descriptors.color_theme.histogram_hovered_color = ECS_TOOLS_UI_HISTOGRAM_HOVERED_COLOR;
			m_descriptors.color_theme.histogram_text_color = ECS_TOOLS_UI_HISTOGRAM_TEXT_COLOR;
			m_descriptors.color_theme.progress_bar = ECS_TOOLS_UI_PROGRESS_BAR_COLOR;
			m_descriptors.color_theme.darken_inactive_item = ECS_TOOLS_UI_DARKEN_INACTIVE_ITEM_FACTOR;
			m_descriptors.color_theme.alpha_inactive_item = ECS_TOOLS_UI_ALPHA_INACTIVE_ITEM_FACTOR;
			SetColorTheme(ECS_TOOLS_UI_THEME_COLOR);
#pragma endregion

#pragma region Dockspace
			m_descriptors.dockspaces.border_minimum_distance = ECS_TOOLS_UI_DOCKSPACE_BORDER_MINIMUM_DISTANCE;
			m_descriptors.dockspaces.border_default_sprite_cache_count = ECS_TOOLS_UI_DOCKSPACE_BORDER_SPRITE_CACHE_COUNT;
			m_descriptors.dockspaces.border_default_sprite_texture_count = ECS_TOOLS_UI_DOCKSPACE_BORDER_SPRITE_TEXTURE_DEFAULT_COUNT;
			m_descriptors.dockspaces.border_default_hoverable_handler_count = ECS_TOOLS_UI_DOCKSPACE_BORDER_HOVERABLE_HANDLER_COUNT;
			m_descriptors.dockspaces.border_default_clickable_handler_count = ECS_TOOLS_UI_DOCKSPACE_BORDER_CLICKABLE_HANDLER_COUNT;
			m_descriptors.dockspaces.border_default_general_handler_count = ECS_TOOLS_UI_DOCKSPACE_BORDER_GENERAL_HANDLER_COUNT;
			m_descriptors.dockspaces.close_x_position_x_left = ECS_TOOLS_UI_DOCKSPACE_CLOSE_X_POSITION_X_LEFT;
			m_descriptors.dockspaces.close_x_position_x_right = ECS_TOOLS_UI_DOCKSPACE_CLOSE_X_POSITION_X_RIGHT;
			m_descriptors.dockspaces.close_x_scale_y = ECS_TOOLS_UI_DOCKSPACE_CLOSE_X_SCALE_Y;
			m_descriptors.dockspaces.close_x_total_x_padding = ECS_TOOLS_UI_DOCKSPACE_CLOSE_X_TOTAL_X_PADDING;
			m_descriptors.dockspaces.collapse_triangle_scale = ECS_TOOLS_UI_DOCKSPACE_COLLAPSE_TRIANGLE_SCALE;
			m_descriptors.dockspaces.count = ECS_TOOLS_UI_DOCKSPACE_COUNT;
			m_descriptors.dockspaces.border_margin = ECS_TOOLS_UI_DOCKSPACE_BORDER_MARGIN;
			m_descriptors.dockspaces.border_size = ECS_TOOLS_UI_DOCKSPACE_BORDER_SIZE;
			m_descriptors.dockspaces.max_border_count = ECS_TOOLS_UI_DOCKSPACE_MAX_BORDER_COUNT;
			m_descriptors.dockspaces.max_windows_border = ECS_TOOLS_UI_DOCKSPACE_MAX_WINDOWS_PER_BORDER;
			m_descriptors.dockspaces.mininum_scale = ECS_TOOLS_UI_DOCKSPACE_MINIMUM_SCALE;
			m_descriptors.dockspaces.region_header_padding = ECS_TOOLS_UI_DOCKSPACE_REGION_HEADER_PADDING;
			m_descriptors.dockspaces.region_header_spacing = ECS_TOOLS_UI_DOCKSPACE_REGION_HEADER_SPACING;
			m_descriptors.dockspaces.region_header_added_scale = ECS_TOOLS_UI_DOCKSPACE_REGION_HEADER_ADDED_SCALE;
			m_descriptors.dockspaces.viewport_padding_x = ECS_TOOLS_UI_DOCKSPACE_VIEWPORT_PADDING_X;
			m_descriptors.dockspaces.viewport_padding_y = ECS_TOOLS_UI_DOCKSPACE_VIEWPORT_PADDING_Y;
#pragma endregion

#pragma region Font
			m_descriptors.font.character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING;
			m_descriptors.font.size = ECS_TOOLS_UI_FONT_SIZE;
			m_descriptors.font.symbol_count = ECS_TOOLS_UI_FONT_SYMBOL_COUNT;
			m_descriptors.font.texture_dimensions = ECS_TOOLS_UI_FONT_TEXTURE_DIMENSIONS;
#pragma endregion

#pragma region Misc
			m_descriptors.misc.title_y_scale = ECS_TOOLS_UI_WINDOW_TITLE_Y_SCALE;
			m_descriptors.misc.window_count = ECS_TOOLS_UI_MAX_WINDOW_COUNT;
			m_descriptors.misc.thread_temp_memory = ECS_TOOLS_UI_THREAD_TEMP_ALLOCATOR_MEMORY;
			m_descriptors.misc.system_vertex_buffers[ECS_TOOLS_UI_SOLID_COLOR] = ECS_TOOLS_UI_VERTEX_BUFFER_MISC_SOLID_COLOR;
			m_descriptors.misc.system_vertex_buffers[ECS_TOOLS_UI_TEXT_SPRITE] = ECS_TOOLS_UI_VERTEX_BUFFER_MISC_TEXT_SPRITE;
			m_descriptors.misc.system_vertex_buffers[ECS_TOOLS_UI_SPRITE] = ECS_TOOLS_UI_VERTEX_BUFFER_MISC_SPRITE;
			m_descriptors.misc.system_vertex_buffers[ECS_TOOLS_UI_SPRITE_CLUSTER] = ECS_TOOLS_UI_VERTEX_BUFFER_MISC_SPRITE_CLUSTER;
			m_descriptors.misc.system_vertex_buffers[ECS_TOOLS_UI_LINE] = ECS_TOOLS_UI_VERTEX_BUFFER_MISC_LINE;
			m_descriptors.misc.drawer_identifier_memory = ECS_TOOLS_UI_MISC_DRAWER_IDENTIFIER;
			m_descriptors.misc.drawer_temp_memory = ECS_TOOLS_UI_MISC_DRAWER_TEMP;
			m_descriptors.misc.window_table_default_count = ECS_TOOLS_UI_MISC_WINDOW_TABLE_SIZE;
			m_descriptors.misc.render_slider_horizontal_size = ECS_TOOLS_UI_MISC_RENDER_SLIDER_HORIZONTAL_SIZE;
			m_descriptors.misc.render_slider_vertical_size = ECS_TOOLS_UI_MISC_RENDER_SLIDER_VERTICAL_SIZE;
			m_descriptors.misc.window_handler_revert_command_count = ECS_TOOLS_UI_REVERT_HANDLER_COMMAND_COUNT;
			m_descriptors.misc.color_input_window_size_x = ECS_TOOLS_UI_COLOR_INPUT_WINDOW_SIZE_X;
			m_descriptors.misc.color_input_window_size_y = ECS_TOOLS_UI_COLOR_INPUT_WINDOW_SIZE_Y;
			m_descriptors.misc.tool_tip_padding = ECS_TOOLS_UI_MISC_TOOL_TIP_PADDING;
			m_descriptors.misc.tool_tip_background_color = ECS_TOOLS_UI_TOOL_TIP_BACKGROUND_COLOR;
			m_descriptors.misc.tool_tip_border_color = ECS_TOOLS_UI_TOOL_TIP_BORDER_COLOR;
			m_descriptors.misc.tool_tip_font_color = ECS_TOOLS_UI_TOOL_TIP_FONT_COLOR;
			m_descriptors.misc.tool_tip_unavailable_font_color = ECS_TOOLS_UI_TOOL_TIP_UNAVAILABLE_FONT_COLOR;
			m_descriptors.misc.menu_x_padd = ECS_TOOLS_UI_MENU_X_PADD;
			m_descriptors.misc.menu_arrow_color = ECS_TOOLS_UI_MENU_ARROW_COLOR;
			m_descriptors.misc.menu_unavailable_arrow_color = ECS_TOOLS_UI_MENU_ARROW_UNAVAILABLE_COLOR;
			m_descriptors.misc.slider_bring_back_start = ECS_TOOLS_UI_SLIDER_BRING_BACK_START;
			m_descriptors.misc.slider_bring_back_tick = ECS_TOOLS_UI_SLIDER_BRING_BACK_TICK;
			m_descriptors.misc.slider_enter_value_duration = ECS_TOOLS_UI_SLIDER_ENTER_VALUES_DURATION;
			m_descriptors.misc.text_input_caret_display_time = ECS_TOOLS_UI_TEXT_INPUT_CARET_DISPLAY_TIME;
			m_descriptors.misc.text_input_repeat_time = ECS_TOOLS_UI_TEXT_INPUT_REPEAT_COUNT;
			m_descriptors.misc.text_input_repeat_start_duration = ECS_TOOLS_UI_TEXT_INPUT_REPEAT_START_TIME;
			m_descriptors.misc.rectangle_hierarchy_drag_node_dimension = ECS_TOOLS_UI_HIERARCHY_DRAG_NODE_DIMENSION;
			m_descriptors.misc.hierarchy_drag_node_time = ECS_TOOLS_UI_HIERARCHY_DRAG_NODE_TIME;
			m_descriptors.misc.hierarchy_drag_node_hover_drop = ECS_TOOLS_UI_HIERARCHY_DRAG_NODE_HOVER_DROP;
			m_descriptors.misc.graph_hover_offset = ECS_TOOLS_UI_GRAPH_HOVER_OFFSET;
			m_descriptors.misc.histogram_hover_offset = ECS_TOOLS_UI_HISTOGRAM_HOVER_OFFSET;

#pragma endregion

#pragma region Window Layout
			m_descriptors.window_layout.element_indentation = ECS_TOOLS_UI_WINDOW_LAYOUT_ELEMENT_INDENTATION;
			m_descriptors.window_layout.next_row_y_offset = ECS_TOOLS_UI_WINDOW_LAYOUT_NEXT_ROW;
			m_descriptors.window_layout.default_element_x = ECS_TOOLS_UI_WINDOW_LAYOUT_DEFAULT_ELEMENT_X;
			m_descriptors.window_layout.default_element_y = ECS_TOOLS_UI_WINDOW_LAYOUT_DEFAULT_ELEMENT_Y;
			m_descriptors.window_layout.next_row_padding = ECS_TOOLS_UI_WINDOW_LAYOUT_NEXT_ROW_PADDING;
			m_descriptors.window_layout.node_indentation = ECS_TOOLS_UI_WINDOW_LAYOUT_NODE_INDENTATION;
#pragma endregion

#pragma region Elements
			m_descriptors.element_descriptor.slider_length.x = ECS_TOOLS_UI_SLIDER_LENGTH_X;
			m_descriptors.element_descriptor.slider_length.y = ECS_TOOLS_UI_SLIDER_LENGTH_Y;
			m_descriptors.element_descriptor.slider_shrink.x = ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_X;
			m_descriptors.element_descriptor.slider_shrink.y = ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_Y;

			m_descriptors.element_descriptor.label_padd = { ECS_TOOLS_UI_LABEL_HORIZONTAL_PADD, ECS_TOOLS_UI_LABEL_VERTICAL_PADD };

			m_descriptors.element_descriptor.color_input_padd = ECS_TOOLS_UI_COLOR_INPUT_PADD;

			m_descriptors.element_descriptor.combo_box_padding = ECS_TOOLS_UI_COMBO_BOX_PADDING;

			m_descriptors.element_descriptor.graph_padding = ECS_TOOLS_UI_GRAPH_PADDING;
			m_descriptors.element_descriptor.graph_x_axis_space = ECS_TOOLS_UI_GRAPH_X_SPACE;
			m_descriptors.element_descriptor.graph_axis_value_line_size = ECS_TOOLS_UI_GRAPH_VALUE_LINE_SIZE;
			m_descriptors.element_descriptor.graph_axis_bump = ECS_TOOLS_UI_GRAPH_BUMP;
			m_descriptors.element_descriptor.graph_reduce_font = ECS_TOOLS_UI_GRAPH_REDUCE_FONT;
			m_descriptors.element_descriptor.graph_sample_circle_size = ECS_TOOLS_UI_GRAPH_SAMPLE_CIRCLE_SIZE;

			m_descriptors.element_descriptor.histogram_padding = ECS_TOOLS_UI_HISTOGRAM_PADDING;
			m_descriptors.element_descriptor.histogram_bar_min_scale = ECS_TOOLS_UI_HISTOGRAM_BAR_MIN_SCALE;
			m_descriptors.element_descriptor.histogram_bar_spacing = ECS_TOOLS_UI_HISTOGRAM_BAR_SPACING;
			m_descriptors.element_descriptor.histogram_reduce_font = ECS_TOOLS_UI_HISTOGRAM_REDUCE_FONT;

			m_descriptors.element_descriptor.menu_button_padding = ECS_TOOLS_UI_MENU_BUTTON_PADDING;
			m_descriptors.element_descriptor.label_list_circle_size = ECS_TOOLS_UI_LABEL_LIST_CIRCLE_SIZE;
#pragma endregion

		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::InitializeMaterials()
		{
#pragma region Materials
			m_descriptors.materials.count = ECS_TOOLS_UI_MATERIALS;
			m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_SOLID_COLOR] = ECS_TOOLS_UI_VERTEX_BUFFER_SOLID_COLOR_SIZE;
			m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_TEXT_SPRITE] = ECS_TOOLS_UI_VERTEX_BUFFER_TEXT_SPRITE_SIZE;
			m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_SPRITE] = ECS_TOOLS_UI_VERTEX_BUFFER_SPRITE_SIZE;
			m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_SPRITE_CLUSTER] = ECS_TOOLS_UI_VERTEX_BUFFER_SPRITE_CLUSTER_SIZE;
			m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_LINE] = ECS_TOOLS_UI_VERTEX_BUFFER_LINE_SIZE;
			m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_SOLID_COLOR + ECS_TOOLS_UI_MATERIALS] = ECS_TOOLS_UI_LATE_DRAW_VERTEX_BUFFER_SOLID_COLOR_SIZE;
			m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_TEXT_SPRITE + ECS_TOOLS_UI_MATERIALS] = ECS_TOOLS_UI_LATE_DRAW_VERTEX_BUFFER_TEXT_SPRITE_SIZE;
			m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_SPRITE + ECS_TOOLS_UI_MATERIALS] = ECS_TOOLS_UI_LATE_DRAW_VERTEX_BUFFER_SPRITE_SIZE;
			m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_SPRITE_CLUSTER + ECS_TOOLS_UI_MATERIALS] = ECS_TOOLS_UI_LATE_DRAW_VERTEX_BUFFER_SPRITE_CLUSTER_SIZE;
			m_descriptors.materials.vertex_buffer_count[ECS_TOOLS_UI_LINE + ECS_TOOLS_UI_MATERIALS] = ECS_TOOLS_UI_LATE_DRAW_VERTEX_BUFFER_LINE_SIZE;
			for (size_t index = 0; index < ECS_TOOLS_UI_PASSES; index++) {
				m_descriptors.materials.strides[ECS_TOOLS_UI_SOLID_COLOR + index * ECS_TOOLS_UI_MATERIALS] = sizeof(UIVertexColor);
				m_descriptors.materials.strides[ECS_TOOLS_UI_TEXT_SPRITE + index * ECS_TOOLS_UI_MATERIALS] = sizeof(UISpriteVertex);
				m_descriptors.materials.strides[ECS_TOOLS_UI_SPRITE + index * ECS_TOOLS_UI_MATERIALS] = sizeof(UISpriteVertex);
				m_descriptors.materials.strides[ECS_TOOLS_UI_SPRITE_CLUSTER + index * ECS_TOOLS_UI_MATERIALS] = sizeof(UISpriteVertex);
				m_descriptors.materials.strides[ECS_TOOLS_UI_LINE + index * ECS_TOOLS_UI_MATERIALS] = sizeof(UIVertexColor);
			}
			m_descriptors.materials.sampler_count = ECS_TOOLS_UI_SAMPLER_COUNT;
#pragma endregion
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIDrawerDescriptor UISystem::InitializeDrawerDescriptorReferences(unsigned int window_index) const
		{
			return UIDrawerDescriptor(
				m_windows[window_index].descriptors->color_theme,
				m_windows[window_index].descriptors->font,
				m_windows[window_index].descriptors->layout,
				m_windows[window_index].descriptors->element_descriptor
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::InitializeWindowDraw(unsigned int index, WindowDraw initialize)
		{
			void* buffers[ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES] = { nullptr };
			size_t counts[ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES] = { 0 };

			UIDrawerDescriptor drawer_descriptor = InitializeDrawerDescriptorReferences(index);
			drawer_descriptor.buffers = buffers;
			drawer_descriptor.counts = counts;
			drawer_descriptor.system = this;
			drawer_descriptor.window_index = index;
			drawer_descriptor.thread_id = 0;
			drawer_descriptor.do_not_initialize_viewport_sliders = false;
			drawer_descriptor.do_not_allocate_buffers = false;
			drawer_descriptor.export_scale = nullptr;
			
			initialize(m_windows[index].window_data, &drawer_descriptor, true);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::InitializeWindowDraw(Stream<char> window_name, WindowDraw initialize) {
			unsigned int window_index = GetWindowFromName(window_name);
			InitializeWindowDraw(window_index, initialize);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::InitializeWindowDefaultHandler(size_t window_index, const UIWindowDescriptor& descriptor)
		{
			Action data_initializer = (Action)m_window_handler.data;

			ActionData initializer_data;
			initializer_data.border_index = 0;
			initializer_data.data = m_windows[window_index].default_handler.data;
			initializer_data.system = this;
			initializer_data.dockspace = &m_floating_horizontal_dockspaces[0];
			initializer_data.counts = &window_index;
			initializer_data.additional_data = (void**)&descriptor;

			data_initializer(&initializer_data);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::IsEmptyFixedDockspace(const UIDockspace* dockspace) const
		{
			bool is_fixed = IsFixedDockspace(dockspace);
			return is_fixed && dockspace->borders[0].window_indices.size == 0;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::IsFixedDockspace(const UIDockspace* dockspace) const
		{
			const UIDockspace* dockspaces[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			for (size_t index = 0; index < m_fixed_dockspaces.size; index++) {
				if (&dockspaces[(unsigned int)m_fixed_dockspaces[index].type][m_fixed_dockspaces[index].index] == dockspace)
					return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::IsFixedDockspace(const UIDockspaceBorder* border) const
		{
			const UIDockspace* dockspaces[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			for (size_t index = 0; index < m_fixed_dockspaces.size; index++) {
				if (dockspaces[(unsigned int)m_fixed_dockspaces[index].type][m_fixed_dockspaces[index].index].borders.buffer == border)
					return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::IsBackgroundDockspace(const UIDockspace* dockspace) const
		{
			const UIDockspace* dockspaces[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			for (size_t index = 0; index < m_background_dockspaces.size; index++) {
				if (&dockspaces[(unsigned int)m_background_dockspaces[index].type][m_background_dockspaces[index].index] == dockspace)
					return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::IsBackgroundDockspace(const UIDockspaceBorder* border) const
		{
			const UIDockspace* dockspaces[] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
			};
			for (size_t index = 0; index < m_background_dockspaces.size; index++) {
				if (dockspaces[(unsigned int)m_background_dockspaces[index].type][m_background_dockspaces[index].index].borders.buffer == border)
					return true;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::IsPopUpWindow(unsigned int window_index) const
		{
			for (size_t index = 0; index < m_pop_up_windows.size; index++) {
				const UIDockspace* dockspace = GetConstDockspace(m_dockspace_layers[m_pop_up_windows[index]]);
				if (dockspace->borders[0].window_indices[0] == window_index) {
					return index;
				}
			}
			return -1;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::IsPopUpWindow(const UIDockspace* dockspace) const {
			if (dockspace->borders.size == 2 && dockspace->borders[0].window_indices.size == 1 && !dockspace->borders[0].is_dock) {
				return IsPopUpWindow(dockspace->borders[0].window_indices[0]);
			}
			return -1;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::IsPopUpWindow(const UIDockspaceBorder* border) const
		{
			if (border[0].window_indices.size == 1 && !border[0].is_dock) {
				unsigned int window_index = border[0].window_indices[0];
				return IsPopUpWindow(window_index);
			}
			return -1;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		struct AddToUIFileData {
			void* window_names;
			Stream<char> name;
		};

		void AddToUIFileChar(AddToUIFileData* parameter) {
			Stream<const char*>* window_names = (Stream<const char*>*)parameter->window_names;
			window_names->Add(parameter->name.buffer);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void AddToUIFileStream(AddToUIFileData* parameter) {
			Stream<Stream<char>>* window_names = (Stream<Stream<char>>*)parameter->window_names;
			window_names->Add(parameter->name);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool LoadUIFileBase(UISystem* system, Stream<void> contents, AddToUIFileData* parameter, void (*handler)(AddToUIFileData*)) {
			system->Clear();

			uintptr_t ptr = (uintptr_t)contents.buffer;
			unsigned short versions[2];
			memcpy(versions, (void*)ptr, sizeof(unsigned short) * 2);
			ptr += sizeof(unsigned short) * 2;

			unsigned char metadata[32];
			memcpy(metadata, (void*)ptr, sizeof(unsigned char) * 32);
			ptr += sizeof(unsigned char) * 32;

#pragma region Dockspaces

			unsigned short dockspace_counts[4];
			memcpy(dockspace_counts, (void*)ptr, sizeof(unsigned short) * 4);
			ptr += sizeof(unsigned short) * 4;

			ECS_ASSERT(dockspace_counts[0] <= system->m_descriptors.dockspaces.count, "Horizontal dockspaces excedeed the limit");
			ECS_ASSERT(dockspace_counts[1] <= system->m_descriptors.dockspaces.count, "Vertical dockspaces excedeed the limit");
			ECS_ASSERT(dockspace_counts[2] <= system->m_descriptors.dockspaces.count, "Floating horizontal dockspaces excedeed the limit");
			ECS_ASSERT(dockspace_counts[3] <= system->m_descriptors.dockspaces.count, "Floating vertical dockspaces excedeed the limit");

			uintptr_t buffer = ptr;
			unsigned char dockspace_border_count = 0;

			auto initialize_dockspace_type = [&](CapacityStream<UIDockspace>& dockspaces, DockspaceType type) {
				dockspaces.size = 0;
				for (size_t index = 0; index < dockspace_counts[(unsigned int)type]; index++) {
					system->CreateEmptyDockspace(type);
					buffer += dockspaces[index].LoadFromFile((const void*)buffer, dockspace_border_count);
					dockspaces[index].borders[dockspace_border_count - 1] = dockspaces[index].borders[1];
					if (type == DockspaceType::FloatingHorizontal || type == DockspaceType::Horizontal) {
						dockspaces[index].borders[dockspace_border_count - 1].position = dockspaces[index].transform.scale.x;
					}
					else {
						dockspaces[index].borders[dockspace_border_count - 1].position = dockspaces[index].transform.scale.y;
					}
					dockspaces[index].borders.size = dockspace_border_count;
					for (size_t border_index = 1; border_index < dockspace_border_count - 1; border_index++) {
						system->CreateEmptyDockspaceBorder(dockspaces.buffer + index, border_index);
					}
					buffer += dockspaces[index].LoadBordersFromFile((const void*)buffer);
				}
			};

			initialize_dockspace_type(system->m_horizontal_dockspaces, DockspaceType::Horizontal);
			initialize_dockspace_type(system->m_vertical_dockspaces, DockspaceType::Vertical);
			initialize_dockspace_type(system->m_floating_horizontal_dockspaces, DockspaceType::FloatingHorizontal);
			initialize_dockspace_type(system->m_floating_vertical_dockspaces, DockspaceType::FloatingVertical);

#pragma endregion

#pragma region Windows
			const unsigned short* window_count = (const unsigned short*)buffer;
			buffer += sizeof(unsigned short);
			unsigned short window_count_value = *window_count;
			system->m_windows.size = 0;
			ECS_ASSERT(window_count_value <= system->m_windows.capacity);

			char window_name_characters[128];
			Stream<char> window_name_stream = Stream<char>(window_name_characters, 0);
			for (size_t index = 0; index < window_count_value; index++) {
				system->CreateEmptyWindow();
				buffer += system->m_windows[index].LoadFromFile((const void*)buffer, window_name_stream);
				system->SetWindowName(index, window_name_characters);
				parameter->name = Stream<char>(system->m_windows[index].name.buffer, window_name_stream.size);
				handler(parameter);
			}
#pragma endregion

#pragma region Dockspace layers
			unsigned short layer_count = *(const unsigned short*)buffer;
			buffer += sizeof(unsigned short);
			system->m_dockspace_layers.size = layer_count;

			memcpy(system->m_dockspace_layers.buffer, (const void*)buffer, sizeof(UIDockspaceLayer) * layer_count);
			buffer += sizeof(UIDockspaceLayer) * layer_count;
#pragma endregion

#pragma region Fixed dockspaces
			unsigned short fixed_dockspace_count = *(const unsigned short*)buffer;
			buffer += sizeof(unsigned short);
			system->m_fixed_dockspaces.size = fixed_dockspace_count;

			memcpy(system->m_fixed_dockspaces.buffer, (const void*)buffer, sizeof(UIDockspaceLayer) * fixed_dockspace_count);
			buffer += sizeof(UIDockspaceLayer) * fixed_dockspace_count;
#pragma endregion

#pragma region Background dockspaces
			unsigned short background_dockspace_count = *(const unsigned short*)buffer;
			buffer += sizeof(unsigned short);
			system->m_background_dockspaces.size = background_dockspace_count;

			memcpy(system->m_background_dockspaces.buffer, (const void*)buffer, sizeof(UIDockspaceLayer) * background_dockspace_count);
			buffer += sizeof(UIDockspaceLayer) * background_dockspace_count;
#pragma endregion

			system->UpdateDockspaceHierarchy();
			if (system->m_floating_horizontal_dockspaces.size > 0) {
				system->SearchAndSetNewFocusedDockspaceRegion(system->m_floating_horizontal_dockspaces.buffer, 0, DockspaceType::FloatingHorizontal);
				system->SetNewFocusedDockspace(system->m_floating_horizontal_dockspaces.buffer, DockspaceType::FloatingHorizontal);
			}
			else {
				system->SearchAndSetNewFocusedDockspaceRegion(system->m_floating_vertical_dockspaces.buffer, 0, DockspaceType::FloatingVertical);
				system->SetNewFocusedDockspace(system->m_floating_vertical_dockspaces.buffer, DockspaceType::FloatingVertical);
			}

			system->m_memory->Deallocate(contents.buffer);
			return true;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::IsWindowDrawing(unsigned int window_index)
		{
			unsigned int border_index;
			DockspaceType type;
			UIDockspace* dockspace = GetDockspaceFromWindow(window_index, border_index, type);
			if (dockspace == nullptr) {
				return false;
			}
			return dockspace->borders[border_index].window_indices[dockspace->borders[border_index].active_window] == window_index;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::LoadUIFile(Stream<wchar_t> filename, Stream<Stream<char>>& window_names)
		{
			Stream<void> contents = ReadWholeFileBinary(filename, GetAllocatorPolymorphic(m_memory));

			AddToUIFileData data;
			data.window_names = &window_names;
			return LoadUIFileBase(this, contents, &data, AddToUIFileStream);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::LoadDescriptorFile(Stream<wchar_t> filename)
		{
			ECS_FILE_HANDLE file = 0;
			ECS_FILE_STATUS_FLAGS status = OpenFile(filename, &file, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_TEXT);

			if (status == ECS_FILE_STATUS_OK) {
				ScopedFile scoped_file({ file });

				unsigned int version = 0;
				bool success = ReadFile(file, { &version, sizeof(version) });
				if (version == ECS_TOOLS_UI_DESCRIPTOR_FILE_VERSION) {
					success &= ReadFile(file, { &m_descriptors, sizeof(UISystemDescriptors) });
				}
				else {
					success = false;
				}

				return success;
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::MoveDockspaceBorder(UIDockspaceBorder* border, unsigned int border_index, float delta_x, float delta_y) {
			border[border_index].position += delta_x + delta_y;
			bool should_not_resize_children = false, should_not_revert = true;
			bool is_delta_x = delta_x != 0.0f;
			bool is_delta_y = delta_y != 0.0f;
			if (border[border_index].position < border[border_index - 1].position + m_descriptors.dockspaces.border_minimum_distance) {
				should_not_revert = false;
				should_not_resize_children = true;
			}
			else if (border[border_index].position > border[border_index + 1].position - m_descriptors.dockspaces.border_minimum_distance) {
				should_not_revert = false;
				should_not_resize_children = true;
			}
			if (border[border_index].is_dock && !should_not_resize_children) {
				if (is_delta_x) {
					should_not_revert = ResizeDockspace(border[border_index].window_indices[0], -delta_x, ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT, DockspaceType::Vertical);
				}
				else if (is_delta_y){
					should_not_revert = ResizeDockspace(border[border_index].window_indices[0], -delta_y, ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP, DockspaceType::Horizontal);
				}
			}
			if (border_index - 1 >= 0 && border[border_index - 1].is_dock && !should_not_resize_children && should_not_revert) {
				if (is_delta_x) {
					should_not_revert = ResizeDockspace(border[border_index - 1].window_indices[0], delta_x, ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT, DockspaceType::Vertical);
				}
				else if (is_delta_y){
					should_not_revert = ResizeDockspace(border[border_index - 1].window_indices[0], delta_y, ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM, DockspaceType::Horizontal);
				}
			}
			if (!should_not_revert) {
				border[border_index].position -= delta_x + delta_y;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::MoveDockspaceBorder(UIDockspaceBorder* border, unsigned int border_index, float delta_x, float delta_y, bool ignore_border_checks) {
			border[border_index].position += delta_x + delta_y;
			if (border[border_index].is_dock) {
				if (delta_x != 0.0f) {
					ResizeDockspace(border[border_index].window_indices[0], delta_x, ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT, DockspaceType::Vertical);
				}
				else if (delta_y != 0.0f) {
					ResizeDockspace(border[border_index].window_indices[0], delta_y, ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP, DockspaceType::Horizontal);
				}
			}
			if (border_index - 1 >= 0 && border[border_index - 1].is_dock) {
				if (delta_x != 0.0f) {
					ResizeDockspace(border[border_index - 1].window_indices[0], delta_x, ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT, DockspaceType::Vertical);
				}
				else if (delta_y != 0.0f) {
					ResizeDockspace(border[border_index - 1].window_indices[0], delta_y, ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP, DockspaceType::Horizontal);
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetNormalizeMousePosition() const
		{
			int2 mouse_position = m_mouse->GetPosition();

			return { static_cast<float>(mouse_position.x) / m_window_os_size.x * 2 - 1.0f,  static_cast<float>(mouse_position.y) / m_window_os_size.y * 2 - 1.0f };
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float2 UISystem::GetSquareScale(float y_scale) const
		{
			return { NormalizeHorizontalToWindowDimensions(y_scale), y_scale };
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float UISystem::NormalizeHorizontalToWindowDimensions(float value) const
		{
			unsigned int width, height;
			m_graphics->GetWindowSize(width, height);
			return value * height / width;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::NormalizeHorizontalToWindowDimensions(float* values, size_t count) const
		{
			unsigned int width, height;
			m_graphics->GetWindowSize(width, height);
			float multiply_factor = (float)height / width;
			for (size_t index = 0; index < count; index++) {
				values[index] *= multiply_factor;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::NormalizeHorizontalToWindowDimensions(Stream<float>& values) const
		{
			unsigned int width, height;
			m_graphics->GetWindowSize(width, height);
			float multiply_factor = (float)height / width;
			for (size_t index = 0; index < values.size; index++) {
				values[index] *= multiply_factor;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		float UISystem::NormalizeVerticalToWindowDimensions(float value) const
		{
			unsigned int width, height;
			m_graphics->GetWindowSize(width, height);
			return value * width / height;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		UIActionHandler UISystem::PeekFrameHandler() const
		{
			ECS_ASSERT(m_frame_handlers.size > 0, "Nothing to peek");
			return m_frame_handlers[0];
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::PopFrameHandler()
		{
			RemoveFrameHandler((unsigned int)0);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::PopUpFrameHandler(
			Stream<char> name, 
			bool is_fixed,
			bool destroy_at_first_click,
			bool is_initialized,
			bool destroy_when_released
		)
		{
			UIPopUpWindowData data;
			data.deallocate_name = true;
			data.destroy_at_first_click = destroy_at_first_click;
			data.destroy_at_release = destroy_when_released;
			data.is_fixed = is_fixed;
			data.is_initialized = is_initialized;

			data.name = function::StringCopy(GetAllocatorPolymorphic(m_memory), name);
			data.reset_when_window_is_destroyed = true;

			UIActionHandler handler;
			handler.action = PopUpWindowSystemHandler;
			handler.data = &data;
			handler.data_size = sizeof(data);
			PushFrameHandler(handler);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::PushBackgroundDockspace()
		{
			UIDockspaceLayer old_layers[128];
			memcpy(old_layers, m_dockspace_layers.buffer, sizeof(UIDockspaceLayer) * m_dockspace_layers.size);

			unsigned int current_index = 0;
			for (size_t index = 0; index < m_dockspace_layers.size; index++) {
				const UIDockspace* dockspace = GetDockspace(old_layers[index].index, old_layers[index].type);
				if (!IsBackgroundDockspace(dockspace)) {
					m_dockspace_layers[current_index++] = old_layers[index];
				}
			}
			for (size_t index = 0; index < m_background_dockspaces.size; index++) {
				m_dockspace_layers[current_index++] = m_background_dockspaces[index];
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::PushActiveDockspaceRegion(Stream<UIVisibleDockspaceRegion>& regions) const
		{
			for (int64_t index = regions.size - 1; index >= 0; index--) {
				if (regions[index].dockspace == m_focused_window_data.active_location.dockspace && regions[index].border_index == m_focused_window_data.active_location.border_index) {
					UIVisibleDockspaceRegion last_copy = regions[regions.size - 1];
					regions[regions.size - 1] = regions[index];
					regions[index] = last_copy;
					break;
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::PushFrameHandler(UIActionHandler handler)
		{
			handler.data = function::CopyNonZero(Allocator(), handler.data, handler.data_size);
			m_frame_handlers.DisplaceElements(0, m_frame_handlers.size);
			m_frame_handlers[0] = handler;
			m_frame_handlers.size++;
			m_frame_handlers.AssertCapacity();
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::PushDestroyWindowHandler(unsigned int window_index)
		{
			PushFrameHandler({ DestroyWindowSystemHandler, &window_index, sizeof(window_index) });
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::PushDestroyCallbackWindowHandler(unsigned int window_index, UIActionHandler handler)
		{
			DestroyWindowCallbackSystemHandlerData data;
			data.window_index = window_index;
			data.callback = handler;

			// + 8 for alignment
			PushFrameHandler({ DestroyWindowCallbackSystemHandler, &data, (unsigned int)sizeof(data) + handler.data_size + 8 });
			if (handler.data_size > 0) {
				DestroyWindowCallbackSystemHandlerData* system_handler_data = (DestroyWindowCallbackSystemHandlerData*)m_frame_handlers[0].data;
				void* handler_data = (void*)function::AlignPointer((uintptr_t)function::OffsetPointer(system_handler_data, sizeof(DestroyWindowCallbackSystemHandlerData)), 8);
				system_handler_data->callback.data = handler_data;
				memcpy(handler_data, handler.data, handler.data_size);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ReadFontDescriptionFile(Stream<wchar_t> filename) {
			// loading font uv descriptor;
			size_t size = 2000;
			void* uv_buffer = m_resource_manager->LoadTextFileImplementation(filename, &size);
			char* uv_character_buffer = (char*)uv_buffer;
			unsigned int uvs[1024];
			size_t numbers = function::ParseNumbersFromCharString(Stream<char>(uv_character_buffer, size), uvs);

			// first two numbers represents the texture width and height
			// these parameters will be used to normalize the coordinates
			size_t atlas_width = uvs[0];
			size_t atlas_height = uvs[1];
			m_font_character_uvs[m_descriptors.font.texture_dimensions] = float2((float)atlas_width, (float)atlas_height);


			size_t uv_v_top = uvs[2];
			size_t uv_v_bottom = uvs[3];

			size_t index = 4;

			size_t number_count = 0;
			// 2 numbers for texture dimensions and 2 for uv_v_top and uv_v_bottom
			// it contains uv information for space and unknown
			for (; index <= numbers - 4; index += 2) {
				m_font_character_uvs[number_count] = float2(
					(float)uvs[index] / atlas_width,
					(float)uv_v_top / atlas_height
				);            // top left
				m_font_character_uvs[number_count + 1] = float2(
					(float)uvs[index + 1] / atlas_width,
					(float)uv_v_bottom / atlas_height
				);           // bottom right
				number_count += 2;
			}

			// For the unknown character copy the one from the space
			m_font_character_uvs[(unsigned int)AlphabetIndex::Unknown * 2] = m_font_character_uvs[(unsigned int)AlphabetIndex::Space * 2];
			m_font_character_uvs[(unsigned int)AlphabetIndex::Unknown * 2 + 1] = m_font_character_uvs[(unsigned int)AlphabetIndex::Space * 2 + 1];
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetNewFocusedDockspaceRegion(UIDockspace* dockspace, unsigned int border_index, DockspaceType type) {
			m_focused_window_data.active_location.dockspace = dockspace;
			m_focused_window_data.active_location.border_index = border_index;
			m_focused_window_data.active_location.type = type;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SearchAndSetNewFocusedDockspaceRegion(UIDockspace* dockspace, unsigned int border_index, DockspaceType type)
		{
			UIDockspace* dockspaces[4] = {
				m_vertical_dockspaces.buffer,
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_horizontal_dockspaces.buffer
			};
			const DockspaceType types[4] = {
				DockspaceType::Vertical,
				DockspaceType::Horizontal,
				DockspaceType::Vertical,
				DockspaceType::Horizontal
			};
			if (dockspace->borders[border_index].is_dock) {
				SearchAndSetNewFocusedDockspaceRegion(&dockspaces[(unsigned int)type][dockspace->borders[border_index].window_indices[0]], 0, types[(unsigned int)type]);
			}
			else {
				SetNewFocusedDockspaceRegion(dockspace, border_index, type);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetNewFocusedDockspace(UIDockspace* dockspace, DockspaceType type) {
			if (!IsBackgroundDockspace(dockspace)) {

				const UIDockspace* dockspaces[4] = {
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_floating_horizontal_dockspaces.buffer,
				m_floating_vertical_dockspaces.buffer
				};
				for (size_t index = 0; index < m_dockspace_layers.size; index++) {
					if (&dockspaces[(unsigned int)m_dockspace_layers[index].type][m_dockspace_layers[index].index] == dockspace) {
						UIDockspaceLayer copy = m_dockspace_layers[index];
						for (int64_t subindex = index - 1; subindex >= 0; subindex--) {
							m_dockspace_layers[subindex + 1] = m_dockspace_layers[subindex];
						}
						m_dockspace_layers[0] = copy;

						for (size_t subindex = 0; subindex < m_pop_up_windows.size; subindex++) {
							if (m_pop_up_windows[subindex] == index) {
								m_pop_up_windows[subindex] = 0;
							}
							else if (m_pop_up_windows[subindex] > index) {
								m_pop_up_windows[subindex]--;
							}
						}
						break;
					}
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSolidColorRenderState()
		{
			SetSolidColorRenderState(m_graphics->GetContext());
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSolidColorRenderState(unsigned int thread_id) {
#ifdef ECS_TOOLS_UI_MULTI_THREADED
			GraphicsContext* context = m_resources.thread_resources[thread_id].deferred_context.Get();
#else
			GraphicsContext* context = m_graphics->GetContext();
#endif
			SetSolidColorRenderState(context);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSolidColorRenderState(GraphicsContext* context) {
			BindVertexShader(m_resources.vertex_shaders[ECS_TOOLS_UI_SOLID_COLOR], context);
			BindPixelShader(m_resources.pixel_shaders[ECS_TOOLS_UI_SOLID_COLOR], context);

			BindInputLayout(m_resources.input_layouts[ECS_TOOLS_UI_SOLID_COLOR], context);

			Topology topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			BindTopology(topology, context);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetTextSpriteRenderState() {
			GraphicsContext* context = m_graphics->GetContext();
			SetTextSpriteRenderState(m_graphics->GetContext());
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetTextSpriteRenderState(unsigned int thread_id) {
#ifdef ECS_TOOLS_UI_MULTI_THREADED
			GraphicsContext* context = m_resources.thread_resources[thread_id].deferred_context.Get();
#else
			GraphicsContext* context = m_graphics->GetContext();
#endif
			SetTextSpriteRenderState(m_graphics->GetContext());
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetTextSpriteRenderState(GraphicsContext* context) {
			m_graphics->EnableAlphaBlending(context);

			BindVertexShader(m_resources.vertex_shaders[ECS_TOOLS_UI_TEXT_SPRITE], context);
			BindPixelShader(m_resources.pixel_shaders[ECS_TOOLS_UI_TEXT_SPRITE], context);

			BindSamplerState(m_resources.texture_samplers[ECS_TOOLS_UI_ANISOTROPIC_SAMPLER], context);

			BindPixelResourceView(m_resources.font_texture, context);

			BindInputLayout(m_resources.input_layouts[ECS_TOOLS_UI_TEXT_SPRITE], context);

			Topology topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			BindTopology(topology, context);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSpriteRenderState() {
			GraphicsContext* context = m_graphics->GetContext();
			SetSpriteRenderState(context);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSpriteRenderState(unsigned int thread_id) {
#ifdef ECS_TOOLS_UI_MULTI_THREADED
			GraphicsContext* context = m_resources.thread_resources[thread_id].deferred_context.Get();
#else
			GraphicsContext* context = m_graphics->GetContext();
#endif
			SetSpriteRenderState(context);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSpriteRenderState(GraphicsContext* context) {
			m_graphics->EnableAlphaBlending(context);

			BindVertexShader(m_resources.vertex_shaders[ECS_TOOLS_UI_SPRITE], context);
			BindPixelShader(m_resources.pixel_shaders[ECS_TOOLS_UI_SPRITE], context);

			BindSamplerState(m_resources.texture_samplers[ECS_TOOLS_UI_ANISOTROPIC_SAMPLER], context);

			BindInputLayout(m_resources.input_layouts[ECS_TOOLS_UI_SPRITE], context);

			Topology topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			BindTopology(topology, context);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSprite(
			UIDockspace* dockspace,
			unsigned int border_index,
			UISpriteTexture texture,
			float2 position,
			float2 scale,
			void** buffers,
			size_t* counts,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv,
			ECS_UI_DRAW_PHASE phase
		) {
			SetSpriteRectangle(
				position,
				scale,
				color,
				top_left_uv,
				bottom_right_uv,
				(UISpriteVertex*)buffers[ECS_TOOLS_UI_SPRITE],
				counts[ECS_TOOLS_UI_SPRITE]
			);
			SetSpriteTextureToDraw(dockspace, border_index, texture, ECS_UI_SPRITE_NORMAL, phase);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSprite(
			UIDockspace* dockspace,
			unsigned int border_index,
			Stream<wchar_t> texture,
			float2 position,
			float2 scale,
			void** buffers,
			size_t* counts,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv,
			ECS_UI_DRAW_PHASE phase
		)
		{
			SetSpriteRectangle(
				position,
				scale,
				color,
				top_left_uv,
				bottom_right_uv,
				(UISpriteVertex*)buffers[ECS_TOOLS_UI_SPRITE],
				counts[ECS_TOOLS_UI_SPRITE]
			);
			SetSpriteTextureToDraw(dockspace, border_index, texture, ECS_UI_SPRITE_NORMAL, phase);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetVertexColorSprite(
			UIDockspace* dockspace, 
			unsigned int border_index, 
			Stream<wchar_t> texture, 
			float2 position,
			float2 scale, 
			void** buffers, 
			size_t* counts, 
			const Color* colors,
			float2 top_left_uv, 
			float2 bottom_right_uv,
			ECS_UI_DRAW_PHASE phase
		)
		{
			SetVertexColorSpriteRectangle(
				position,
				scale,
				colors,
				top_left_uv,
				bottom_right_uv,
				(UISpriteVertex*)buffers[ECS_TOOLS_UI_SPRITE],
				counts[ECS_TOOLS_UI_SPRITE]
			);
			SetSpriteTextureToDraw(dockspace, border_index, texture, ECS_UI_SPRITE_NORMAL, phase);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetVertexColorSprite(
			UIDockspace* dockspace, 
			unsigned int border_index, 
			Stream<wchar_t> texture, 
			float2 position, 
			float2 scale, 
			void** buffers, 
			size_t* counts,
			const ColorFloat* colors, 
			float2 top_left_uv, 
			float2 bottom_right_uv, 
			ECS_UI_DRAW_PHASE phase
		)
		{
			SetVertexColorSpriteRectangle(
				position,
				scale,
				colors,
				top_left_uv,
				bottom_right_uv,
				(UISpriteVertex*)buffers[ECS_TOOLS_UI_SPRITE],
				counts[ECS_TOOLS_UI_SPRITE]
			);
			SetSpriteTextureToDraw(dockspace, border_index, texture, ECS_UI_SPRITE_NORMAL, phase);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSpriteCluster(
			UIDockspace* dockspace, 
			unsigned int border_index, 
			Stream<wchar_t> texture, 
			unsigned int count,
			ECS_UI_DRAW_PHASE phase
		)
		{
			dockspace->borders[border_index].draw_resources.sprite_cluster_subtreams.Add(count * 6);
			SetSpriteTextureToDraw(
				dockspace,
				border_index,
				texture,
				ECS_UI_SPRITE_CLUSTER,
				phase
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSpriteTextureToDraw(
			UIDockspace* dockspace, 
			unsigned int border_index, 
			Stream<wchar_t> _texture,
			ECS_UI_SPRITE_TYPE type,
			ECS_UI_DRAW_PHASE phase
		)
		{
			UISpriteTexture* next_texture = GetNextSpriteTextureToDraw(dockspace, border_index, phase, type);
			CreateSpriteTexture(_texture, next_texture);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSpriteTextureToDraw(
			UIDockspace* dockspace,
			unsigned int border_index,
			UISpriteTexture texture,
			ECS_UI_SPRITE_TYPE type,
			ECS_UI_DRAW_PHASE phase
		)
		{
			UISpriteTexture* next_texture = GetNextSpriteTextureToDraw(dockspace, border_index, phase, type);
			*next_texture = texture;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetPopUpWindowPosition(unsigned int window_index, float2 new_position)
		{
			unsigned int border_index;
			DockspaceType type;
			UIDockspace* dockspace = GetDockspaceFromWindow(window_index, border_index, type);

			SetDockspacePosition(dockspace, new_position);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetPopUpWindowScale(unsigned int window_index, float2 new_scale)
		{
			unsigned int border_index;
			DockspaceType type;
			UIDockspace* dockspace = GetDockspaceFromWindow(window_index, border_index, type);

			new_scale.x = new_scale.x > 0.0f ? new_scale.x : 0.0f;
			new_scale.y = new_scale.y > 0.0f ? new_scale.y : 0.0f;
			dockspace->transform.scale = new_scale;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetDockspacePosition(UIDockspace* dockspace, float2 new_position)
		{
			dockspace->transform.position = new_position;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetDockspaceScale(UIDockspace* dockspace, float2 new_scale)
		{
			dockspace->transform.scale = new_scale;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetViewport(float2 position, float2 scale, GraphicsContext* context)
		{
			float2 half_scale = { scale.x * 0.5f, scale.y * 0.5f };
			SetViewport(context, position, half_scale);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetViewport(GraphicsContext* context, float2 position, float2 half_scale)
		{
			unsigned int window_width, window_height;
			m_graphics->GetWindowSize(window_width, window_height);

			BindViewport(
				(position.x + 1.0f) * (float)window_width * 0.5f,
				(position.y + 1.0f) * (float)window_height * 0.5f,
				(half_scale.x) * (float)window_width,
				(half_scale.y) * (float)window_height,
				0.0f,
				1.0f,
				context
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetWindowActions(unsigned int index, const UIWindowDescriptor& descriptor) {
			void* window_data = function::CopyNonZero(Allocator(), descriptor.window_data, descriptor.window_data_size);
			void* private_handler_data = function::CopyNonZero(Allocator(), descriptor.private_action_data, descriptor.private_action_data_size);
			void* destroy_data = function::CopyNonZero(Allocator(), descriptor.destroy_action_data, descriptor.destroy_action_data_size);

			m_windows[index].window_data = window_data;
			m_windows[index].draw = descriptor.draw;
			
			InitializeWindowDraw(index, descriptor.draw);

			m_windows[index].private_handler = { descriptor.private_action, private_handler_data, (unsigned int)descriptor.private_action_data_size };
			m_windows[index].destroy_handler = { descriptor.destroy_action, destroy_data, (unsigned int)descriptor.destroy_action_data_size };
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetWindowActions(Stream<char> name, const UIWindowDescriptor& descriptor) {
			unsigned int window_index = GetWindowFromName(name);
			SetWindowActions(window_index, descriptor);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetWindowDestroyAction(unsigned int index, UIActionHandler handler)
		{
			handler.data = function::CopyNonZero(Allocator(), handler.data, handler.data_size);

			m_windows[index].destroy_handler.data_size = handler.data_size;
			m_windows[index].destroy_handler.data = handler.data;
			m_windows[index].destroy_handler.action = handler.action;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetWindowDestroyAction(Stream<char> name, UIActionHandler handler) {
			unsigned int window_index = GetWindowFromName(name);
			ECS_ASSERT(window_index != -1);
			SetWindowDestroyAction(window_index, handler);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetWindowPrivateAction(unsigned int index, UIActionHandler handler)
		{
			handler.data = function::CopyNonZero(Allocator(), handler.data, handler.data_size);

			m_windows[index].private_handler.action = handler.action;
			m_windows[index].private_handler.data = handler.data;
			m_windows[index].private_handler.data_size = handler.data_size;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetWindowPrivateAction(Stream<char> name, UIActionHandler handler) {
			unsigned int window_index = GetWindowFromName(name);
			ECS_ASSERT(window_index != -1);
			SetWindowPrivateAction(window_index, handler);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetWindowName(unsigned int window_index, Stream<char> name) {
			if (m_windows[window_index].name_vertex_buffer.buffer != nullptr) {
				m_memory->Deallocate(m_windows[window_index].name_vertex_buffer.buffer);
			}
			if (name.size == 0) {
				m_windows[window_index].name_vertex_buffer.buffer = nullptr;
				m_windows[window_index].name_vertex_buffer.size = 0;

				m_windows[window_index].name.size = 0;
			}
			else {
				void* new_allocation = m_memory->Allocate((sizeof(UISpriteVertex) * 6 + sizeof(char)) * name.size, alignof(UISpriteVertex));
				uintptr_t buffer = (uintptr_t)new_allocation;
				m_windows[window_index].name_vertex_buffer.buffer = (UISpriteVertex*)new_allocation;
				m_windows[window_index].name_vertex_buffer.size = name.size * 6;
				buffer += sizeof(UISpriteVertex) * 6 * name.size;
				m_windows[window_index].name.InitializeAndCopy(buffer, name);
				float sprite_y_scale = GetTextSpriteYScale(m_descriptors.font.size);
				ConvertCharactersToTextSprites(
					name,
					float2(0.0f, AlignMiddle(
						0.0f,
						m_descriptors.misc.title_y_scale - m_descriptors.dockspaces.border_size,
						sprite_y_scale)
					),
					m_windows[window_index].name_vertex_buffer.buffer,
					Color((unsigned char)255, 255, 255, 255),
					0,
					m_descriptors.font.size,
					m_descriptors.font.character_spacing
				);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetWindowMaxZoom(unsigned int window_index, float max_zoom)
		{
			m_windows[window_index].max_zoom = max_zoom;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetWindowMinZoom(unsigned int window_index, float min_zoom) {
			m_windows[window_index].min_zoom = min_zoom;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetWindowDrawerDifferenceSpan(unsigned int window_index, float2 span)
		{
			m_windows[window_index].drawer_draw_difference = span;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetWindowOSSize(uint2 new_size)
		{
			m_window_os_size = new_size;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::TranslateDockspace(UIDockspace* dockspace, float2 translation)
		{
			dockspace->transform.position.x += translation.x;
			dockspace->transform.position.y += translation.y;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		LinearAllocator* UISystem::TemporaryAllocator(unsigned int thread_id, ECS_UI_DRAW_PHASE phase)
		{
			if (phase == ECS_UI_DRAW_SYSTEM) {
				return &m_resources.thread_resources[thread_id].system_temp_allocator;
			}
			return &m_resources.thread_resources[thread_id].temp_allocator;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RegisterFocusedWindowClickableAction(
			float2 position, 
			float2 scale, 
			Action action, 
			const void* data,
			size_t data_size,
			ECS_UI_DRAW_PHASE phase
		)
		{
			void* allocation = m_memory->Allocate(data_size, 8);
			memcpy(allocation, data, data_size);
			if (m_focused_window_data.clickable_handler.action != nullptr) {
				m_memory->Deallocate(m_focused_window_data.clickable_handler.data);
			}
			m_focused_window_data.ChangeClickableHandler(position, scale, action, allocation, data_size, phase);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RegisterPixelShader(wchar_t* filename) {
			ECS_ASSERT(m_resources.pixel_shaders.size < m_resources.pixel_shaders.capacity);
			m_resources.pixel_shaders[m_resources.pixel_shaders.size++] = m_resource_manager->LoadPixelShaderImplementation(filename);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RegisterPixelShaders() {
			RegisterPixelShader(ECS_PIXEL_SHADER_SOURCE(UIVertexColor));
			RegisterPixelShader(ECS_PIXEL_SHADER_SOURCE(UITextSpriteVertex));
			RegisterPixelShader(ECS_PIXEL_SHADER_SOURCE(UISpriteVertex));
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RegisterSamplers()
		{
			SamplerDescriptor sampler_descriptor;
			sampler_descriptor.SetAddressType(ECS_SAMPLER_ADDRESS_WRAP);
			sampler_descriptor.max_anisotropic_level = 4;
			sampler_descriptor.filter_type = ECS_SAMPLER_FILTER_ANISOTROPIC;

			m_resources.texture_samplers[ECS_TOOLS_UI_ANISOTROPIC_SAMPLER] = m_graphics->CreateSamplerState(sampler_descriptor);
			m_resources.texture_samplers.size = m_descriptors.materials.sampler_count;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RegisterWindowResource(size_t window_index, void* data, const void* _identifier, unsigned int identifier_size)
		{
			ResourceIdentifier identifier = ResourceIdentifier(_identifier, identifier_size);
			m_windows[window_index].table.Insert(data, identifier);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ReleaseWindowResource(size_t window_index, const void* _identifier, unsigned int identifier_size)
		{
			ResourceIdentifier identifier = ResourceIdentifier(_identifier, identifier_size);
			m_windows[window_index].table.Erase(identifier);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RegisterVertexShaderAndLayout(wchar_t* filename) {
			ECS_ASSERT(m_resources.vertex_shaders.size < m_resources.vertex_shaders.capacity);
			Stream<char> shader_source;
			m_resources.vertex_shaders[m_resources.vertex_shaders.size++] = m_resource_manager->LoadVertexShaderImplementation(
				filename, 
				&shader_source
			);
			m_resources.input_layouts[m_resources.input_layouts.size] = m_graphics->ReflectVertexShaderInput(m_resources.vertex_shaders[m_resources.input_layouts.size], shader_source);
			m_resource_manager->Deallocate(shader_source.buffer);
			m_resources.input_layouts.size++;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RegisterVertexShadersAndLayouts() {
			RegisterVertexShaderAndLayout(ECS_VERTEX_SHADER_SOURCE(UIVertexColor));
			RegisterVertexShaderAndLayout(ECS_VERTEX_SHADER_SOURCE(UITextSpriteVertex));
			RegisterVertexShaderAndLayout(ECS_VERTEX_SHADER_SOURCE(UISpriteVertex));
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveDockspace(unsigned int dockspace_index, DockspaceType dockspace_type) {
			switch (dockspace_type) {
			case DockspaceType::Horizontal:
				RemoveDockspace(dockspace_index, m_horizontal_dockspaces);
				break;
			case DockspaceType::Vertical:
				RemoveDockspace(dockspace_index, m_vertical_dockspaces);
				break;
			case DockspaceType::FloatingHorizontal:
				RemoveDockspace(dockspace_index, m_floating_horizontal_dockspaces);
				break;
			case DockspaceType::FloatingVertical:
				RemoveDockspace(dockspace_index, m_floating_vertical_dockspaces);
				break;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveDockspace(unsigned int dockspace_index, CapacityStream<UIDockspace>& dockspace_buffer) {
			ECS_ASSERT(dockspace_index >= 0 && dockspace_index < dockspace_buffer.size);

			for (size_t index = 0; index < dockspace_buffer[dockspace_index].borders.size; index++) {
				m_memory->Deallocate(dockspace_buffer[dockspace_index].borders[index].window_indices.buffer);
			}

			m_memory->Deallocate(dockspace_buffer[dockspace_index].borders.buffer);

			// not using memcpy because of aliasing
			for (size_t index = dockspace_index; index < dockspace_buffer.size - 1; index++) {
				dockspace_buffer[index] = dockspace_buffer[index + 1];
			}
			dockspace_buffer.size--;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveWindowMemoryResource(unsigned int window_index, const void* buffer)
		{
			size_t index = function::SearchBytes(m_windows[window_index].memory_resources.buffer, m_windows[window_index].memory_resources.size, (size_t)buffer, sizeof(buffer));
			ECS_ASSERT(index != -1);
			RemoveWindowMemoryResource(window_index, index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveWindowMemoryResource(unsigned int window_index, unsigned int buffer_index)
		{
			m_windows[window_index].memory_resources.RemoveSwapBack(buffer_index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveWindowDynamicResource(unsigned int window_index, Stream<char> name)
		{
			unsigned int index = GetWindowDynamicElement(window_index, name);
			ECS_ASSERT(index != -1);
			RemoveWindowDynamicResource(window_index, index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveWindowDynamicResource(unsigned int window_index, unsigned int index)
		{
			const UIWindowDynamicResource* resource = m_windows[window_index].dynamic_resources.GetValuePtrFromIndex(index);

			// cannot simply bump back those allocations because if some are deleted before it can affect
			// the order in which they were stored, invalidating the index reference
			for (size_t subindex = 0; subindex < resource->element_allocations.size; subindex++) {
				const void* buffer = resource->element_allocations[subindex];
				m_memory->Deallocate(buffer);
				RemoveWindowMemoryResource(window_index, buffer);
			}

			for (size_t subindex = 0; subindex < resource->added_allocations.size; subindex++) {
				const void* buffer = resource->added_allocations[subindex];
				m_memory->Deallocate(buffer);
				RemoveWindowMemoryResource(window_index, buffer);
			}

			for (size_t subindex = 0; subindex < resource->table_resources.size; subindex++) {
				ResourceIdentifier identifier = resource->table_resources[subindex];
				m_windows[window_index].table.Erase(identifier);
			}

			// The element allocations contains the starting coallesced allocation
			m_memory->Deallocate(resource->element_allocations.buffer);
			m_windows[window_index].dynamic_resources.EraseFromIndex(index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveWindowBufferFromAll(unsigned int window_index, const void* buffer, unsigned int dynamic_index)
		{
			m_memory->Deallocate(buffer);
			RemoveWindowMemoryResource(window_index, buffer);
			if (dynamic_index != -1) {
				RemoveWindowDynamicResourceAllocation(window_index, dynamic_index, buffer);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::RemoveWindowDynamicResourceAllocation(unsigned int window_index, unsigned int index, const void* buffer)
		{
			UIWindowDynamicResource* resource = GetWindowDynamicElement(window_index, index);

			size_t buffer_index = function::SearchBytes(resource->element_allocations.buffer, resource->element_allocations.size, (size_t)buffer, sizeof(buffer));
			if (buffer_index == -1) {
				// Might be in the added allocations
				buffer_index = function::SearchBytes(resource->added_allocations.buffer, resource->added_allocations.size, (size_t)buffer, sizeof(buffer));
				
				if (buffer_index != -1) {
					resource->added_allocations.RemoveSwapBack(buffer_index);
					// Relocate the buffer
					void* new_allocation = m_memory->Allocate(resource->added_allocations.MemoryOf(resource->added_allocations.size));
					resource->added_allocations.CopyTo(new_allocation);

					void* old_buffer = resource->added_allocations.buffer;
					resource->added_allocations.buffer = (void**)new_allocation;
					ReplaceWindowMemoryResource(window_index, old_buffer, new_allocation);
					return true;
				}
				else {
					return false;
				}
			}
			else {
				resource->element_allocations.RemoveSwapBack(buffer_index);
				return true;
			}

			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::RemoveWindowDynamicResourceTableResource(unsigned int window_index, unsigned int index, ResourceIdentifier identifier)
		{
			UIWindowDynamicResource* resource = GetWindowDynamicElement(window_index, index);
			for (size_t index = 0; index < resource->table_resources.size; index++) {
				if (resource->table_resources[index].Compare(identifier)) {
					resource->table_resources.RemoveSwapBack(index);
					return true;
				}
			}
			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ReplaceWindowMemoryResource(unsigned int window_index, const void* old_buffer, const void* new_buffer)
		{
			size_t index = function::SearchBytes(m_windows[window_index].memory_resources.buffer, m_windows[window_index].memory_resources.size, (size_t)old_buffer, sizeof(old_buffer));
			ECS_ASSERT(index != -1);
			m_windows[window_index].memory_resources[index] = (void*)new_buffer;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ReplaceWindowDynamicResourceAllocation(unsigned int window_index, unsigned int index, const void* old_buffer, void* new_buffer)
		{
			UIWindowDynamicResource* resource = GetWindowDynamicElement(window_index, index);
			for (size_t index = 0; index < resource->element_allocations.size; index++) {
				if (resource->element_allocations[index] == old_buffer) {
					resource->element_allocations[index] = new_buffer;
					return;
				}
			}
			size_t resource_index = function::SearchBytes(resource->element_allocations.buffer, resource->element_allocations.size, (size_t)old_buffer, sizeof(old_buffer));
			if (resource_index == -1) {
				// Might be an added allocation
				resource_index = function::SearchBytes(resource->added_allocations.buffer, resource->added_allocations.size, (size_t)old_buffer, sizeof(old_buffer));
				ECS_ASSERT(resource_index != -1);
				resource->added_allocations[resource_index] = new_buffer;
			}
			else {
				resource->element_allocations[resource_index] = new_buffer;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ReplaceWindowBufferFromAll(unsigned int window_index, const void* old_buffer, const void* new_buffer, unsigned int dynamic_index)
		{
			m_memory->Deallocate(old_buffer);
			ReplaceWindowMemoryResource(window_index, old_buffer, new_buffer);
			if (dynamic_index != -1) {
				ReplaceWindowDynamicResourceAllocation(window_index, dynamic_index, old_buffer, (void*)new_buffer);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ReplaceWindowDynamicResourceTableResource(unsigned int window_index, unsigned int index, ResourceIdentifier old_identifier, ResourceIdentifier new_identifier)
		{
			UIWindowDynamicResource* resource = GetWindowDynamicElement(window_index, index);
			for (size_t index = 0; index < resource->table_resources.size; index++) {
				if (resource->table_resources[index].Compare(old_identifier)) {
					resource->table_resources[index] = new_identifier;
					return;
				}
			}

			ECS_ASSERT(false);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveUnrestoredWindows()
		{
			for (size_t index = 0; index < m_windows.size; index++) {
				if (m_windows[index].draw == nullptr) {
					DockspaceType type;
					unsigned int border_index;
					UIDockspace* dockspace = GetDockspaceFromWindow(index, border_index, type);

					unsigned int window_count = 0;
					for (size_t subindex = 0; subindex < dockspace->borders[border_index].window_indices.size; subindex++) {
						if (dockspace->borders[border_index].window_indices[subindex] == index) {
							window_count = dockspace->borders[border_index].window_indices.size;
							RemoveWindowFromDockspaceRegion(dockspace, type, border_index, subindex);
							break;
						}
					}
					if (window_count > 1) {
						DestroyWindow(index);
					}
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveHoverableHandler(UIDockspace* dockspace, unsigned int border_index)
		{
			dockspace->borders[border_index].hoverable_handler.position_x.size--;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveClickableHandler(UIDockspace* dockspace, unsigned int border_index) {
			dockspace->borders[border_index].clickable_handler.position_x.size--;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveGeneralHandler(UIDockspace* dockspace, unsigned int border_index) {
			dockspace->borders[border_index].general_handler.position_x.size--;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveFrameHandler(Action action, void* data)
		{
			for (size_t index = 0; index < m_frame_handlers.size; index++) {
				if (m_frame_handlers[index].action == action && m_frame_handlers[index].data == data) {
					RemoveFrameHandler(index);
					return;
				}
			}
			ECS_ASSERT(false);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveFrameHandler(unsigned int index)
		{
			if (m_frame_handlers[index].data_size > 0) {
				m_memory->Deallocate(m_frame_handlers[index].data);
			}
			m_frame_handlers.RemoveSwapBack(index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RemoveSpriteTexture(UIDockspace* dockspace, unsigned int border_index, ECS_UI_DRAW_PHASE phase, ECS_UI_SPRITE_TYPE type)
		{
			if (phase == ECS_UI_DRAW_PHASE::ECS_UI_DRAW_SYSTEM) {
				m_resources.system_draw.sprite_textures[(unsigned int)type].size--;
			}
			else {
				dockspace->borders[border_index].draw_resources.sprite_textures[(unsigned int)phase * ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS + (unsigned int)type].size--;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::ResizeDockspace(
			unsigned int dockspace_index, 
			float delta_scale,
			ECS_UI_BORDER_TYPE border_type,
			DockspaceType dockspace_type
		) {
			bool should_resize_parent = true;
			if (dockspace_type == DockspaceType::Horizontal) {
				ECS_ASSERT(dockspace_index >= 0 && dockspace_index < m_horizontal_dockspaces.size);

				if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP) {
					//delta_scale = -delta_scale;
					m_horizontal_dockspaces[dockspace_index].transform.position.y -= delta_scale;
					m_horizontal_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					should_resize_parent = ResizeDockspaceInternal(
						this,
						dockspace_index,
						delta_scale,
						border_type,
						m_horizontal_dockspaces,
						DockspaceType::Horizontal,
						[&]() {
							m_horizontal_dockspaces[dockspace_index].transform.position.y += delta_scale;
							m_horizontal_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
						}
					);
					return should_resize_parent;
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT) {
					m_horizontal_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 1].position += delta_scale;

					if (m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 1].position
						- m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 2].position < m_descriptors.dockspaces.border_minimum_distance) {
						if (m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 1].position
							- m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 2].position - delta_scale >= m_descriptors.dockspaces.border_minimum_distance
							&& delta_scale < 0.0f) {
							m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 1].position -= delta_scale;
							m_horizontal_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
							return false;
						}
					}
					if (m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 2].is_dock) {
						should_resize_parent = ResizeDockspace(
							m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 2].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT,
							DockspaceType::Vertical
						);
					}
					if (!should_resize_parent) {
						m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 1].position -= delta_scale;
						m_horizontal_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
					}
					return should_resize_parent;
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM) {
					//delta_scale = -delta_scale;
					m_horizontal_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
					should_resize_parent = ResizeDockspaceInternal(
						this,
						dockspace_index,
						delta_scale,
						border_type,
						m_horizontal_dockspaces,
						DockspaceType::Horizontal,
						[&]() {
							m_horizontal_dockspaces[dockspace_index].transform.scale.y += delta_scale;
						}
					);
					return should_resize_parent;
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT) {
					//delta_scale = -delta_scale;
					m_horizontal_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_horizontal_dockspaces[dockspace_index].transform.position.x -= delta_scale;
					if (m_horizontal_dockspaces[dockspace_index].borders[1].position
						- m_horizontal_dockspaces[dockspace_index].borders[0].position + delta_scale < m_descriptors.dockspaces.border_minimum_distance) {
						if (m_horizontal_dockspaces[dockspace_index].borders[1].position
							- m_horizontal_dockspaces[dockspace_index].borders[0].position >= m_descriptors.dockspaces.border_minimum_distance
							&& delta_scale < 0.0f) {
							m_horizontal_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
							m_horizontal_dockspaces[dockspace_index].transform.position.x += delta_scale;
							return false;
						}
					}
					if (m_horizontal_dockspaces[dockspace_index].borders[0].is_dock) {
						should_resize_parent = ResizeDockspace(
							m_horizontal_dockspaces[dockspace_index].borders[0].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT,
							DockspaceType::Vertical
						);
					}
					if (should_resize_parent) {
						for (size_t index = 1; index < m_horizontal_dockspaces[dockspace_index].borders.size; index++) {
							m_horizontal_dockspaces[dockspace_index].borders[index].position += delta_scale;
						}
					}
					else {
						m_horizontal_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
						m_horizontal_dockspaces[dockspace_index].transform.position.x += delta_scale;
					}
					return should_resize_parent;
				}
			}
			else if (dockspace_type == DockspaceType::Vertical) {
				ECS_ASSERT(dockspace_index >= 0 && dockspace_index < m_vertical_dockspaces.size);

				if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP) {
					//delta_scale = -delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.position.y -= delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					if (m_vertical_dockspaces[dockspace_index].borders[1].position
						- m_vertical_dockspaces[dockspace_index].borders[0].position + delta_scale < m_descriptors.dockspaces.border_minimum_distance) {
						if (m_vertical_dockspaces[dockspace_index].borders[1].position
							- m_vertical_dockspaces[dockspace_index].borders[0].position >= m_descriptors.dockspaces.border_minimum_distance
							&& delta_scale < 0.0f) {
							m_vertical_dockspaces[dockspace_index].transform.position.y += delta_scale;
							m_vertical_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
							return false;
						}
					}
					if (m_vertical_dockspaces[dockspace_index].borders[0].is_dock) {
						should_resize_parent = ResizeDockspace(
							m_vertical_dockspaces[dockspace_index].borders[0].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP,
							DockspaceType::Horizontal
						);
					}
					if (should_resize_parent) {
						for (size_t index = 1; index < m_vertical_dockspaces[dockspace_index].borders.size; index++) {
							m_vertical_dockspaces[dockspace_index].borders[index].position += delta_scale;
						}
					}
					else {
						m_vertical_dockspaces[dockspace_index].transform.position.y += delta_scale;
						m_vertical_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
					}
					return should_resize_parent;
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT) {
					//delta_scale = -delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					should_resize_parent = ResizeDockspaceInternal(
						this,
						dockspace_index,
						delta_scale,
						border_type,
						m_vertical_dockspaces,
						DockspaceType::Vertical,
						[&]() {
							m_vertical_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
						}
					);
					return should_resize_parent;
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM) {
					//delta_scale = -delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 1].position += delta_scale;
					if (m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 1].position
			- m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 2].position < m_descriptors.dockspaces.border_minimum_distance) {
						if (m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 1].position
							- m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 2].position - delta_scale >= m_descriptors.dockspaces.border_minimum_distance
							&& delta_scale < 0.0f) {
							m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 1].position -= delta_scale;
							m_vertical_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
							return false;
						}
					}
					if (m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 2].is_dock) {
						should_resize_parent = ResizeDockspace(
							m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 2].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM,
							DockspaceType::Horizontal
						);
					}
					if (!should_resize_parent) {
						m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 1].position -= delta_scale;
						m_vertical_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
					}
					return should_resize_parent;
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT) {
					//delta_scale = -delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.position.x -= delta_scale;
					should_resize_parent = ResizeDockspaceInternal(
						this,
						dockspace_index,
						delta_scale,
						border_type,
						m_vertical_dockspaces,
						DockspaceType::Vertical,
						[&]() {
							m_vertical_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
							m_vertical_dockspaces[dockspace_index].transform.position.x += delta_scale;
						}
					);
					return should_resize_parent;
				}
			}
			else if (dockspace_type == DockspaceType::FloatingHorizontal) {
				ECS_ASSERT(dockspace_index >= 0 && dockspace_index < m_floating_horizontal_dockspaces.size);

				if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP) {
					delta_scale = -delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].transform.position.y -= delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					if (m_floating_horizontal_dockspaces[dockspace_index].transform.scale.y < m_descriptors.dockspaces.mininum_scale) {
						m_floating_horizontal_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
						m_floating_horizontal_dockspaces[dockspace_index].transform.position.y += delta_scale;
						return false;
					}
					else {
						should_resize_parent = ResizeDockspaceInternal(
							this,
							dockspace_index,
							delta_scale,
							border_type,
							m_floating_horizontal_dockspaces,
							DockspaceType::FloatingHorizontal,
							[&]() {
								m_floating_horizontal_dockspaces[dockspace_index].transform.position.y += delta_scale;
								m_floating_horizontal_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
							}
						);
						return should_resize_parent;
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT) {
					//delta_scale = -delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 1].position += delta_scale;
					if (m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 1].position
						- m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 2].position < m_descriptors.dockspaces.border_minimum_distance) {
						if (m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 1].position
							- m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 2].position - delta_scale >= m_descriptors.dockspaces.border_minimum_distance
							&& delta_scale < 0.0f){
							m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 1].position -= delta_scale;
							m_floating_horizontal_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
							return false;
						}
					}
					if (m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 2].is_dock) {
						should_resize_parent = ResizeDockspace(
							m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 2].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT,
							DockspaceType::Vertical
						);
					}
					if (!should_resize_parent) {
						m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 1].position -= delta_scale;
						m_floating_horizontal_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
					}
					return should_resize_parent;
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM) {
					m_floating_horizontal_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					if (m_floating_horizontal_dockspaces[dockspace_index].transform.scale.y < m_descriptors.dockspaces.mininum_scale) {
						m_floating_horizontal_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
						return false;
					}
					else {
						should_resize_parent = ResizeDockspaceInternal(
							this,
							dockspace_index,
							delta_scale,
							border_type,
							m_floating_horizontal_dockspaces,
							DockspaceType::FloatingHorizontal,
							[&]() {
								m_floating_horizontal_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
							}
						);
						return should_resize_parent;
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT) {
					delta_scale = -delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].transform.position.x -= delta_scale;
					if (m_floating_horizontal_dockspaces[dockspace_index].borders[1].position 
						- m_floating_horizontal_dockspaces[dockspace_index].borders[0].position + delta_scale < m_descriptors.dockspaces.border_minimum_distance) {
						if (m_floating_horizontal_dockspaces[dockspace_index].borders[1].position
							- m_floating_horizontal_dockspaces[dockspace_index].borders[0].position >= m_descriptors.dockspaces.border_minimum_distance
							&& delta_scale < 0.0f) {
							m_floating_horizontal_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
							m_floating_horizontal_dockspaces[dockspace_index].transform.position.x += delta_scale;
							return false;
						}
					}
					if (m_floating_horizontal_dockspaces[dockspace_index].borders[0].is_dock) {
						should_resize_parent = ResizeDockspace(
							m_floating_horizontal_dockspaces[dockspace_index].borders[0].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT,
							DockspaceType::Vertical
						);
					}
					if (!should_resize_parent) {
						m_floating_horizontal_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
						m_floating_horizontal_dockspaces[dockspace_index].transform.position.x += delta_scale;
					}
					else {
						for (size_t index = 1; index < m_floating_horizontal_dockspaces[dockspace_index].borders.size; index++) {
							m_floating_horizontal_dockspaces[dockspace_index].borders[index].position += delta_scale;
						}
					}
					return should_resize_parent;
				}
			}
			else if (dockspace_type == DockspaceType::FloatingVertical) {
				ECS_ASSERT(dockspace_index >= 0 && dockspace_index < m_floating_vertical_dockspaces.size);

				if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP) {
					delta_scale = -delta_scale;
					m_floating_vertical_dockspaces[dockspace_index].transform.position.y -= delta_scale;
					m_floating_vertical_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					if (m_floating_vertical_dockspaces[dockspace_index].borders[1].position
						- m_floating_vertical_dockspaces[dockspace_index].borders[0].position + delta_scale < m_descriptors.dockspaces.border_minimum_distance) {
						if (m_floating_vertical_dockspaces[dockspace_index].borders[1].position
							- m_floating_vertical_dockspaces[dockspace_index].borders[0].position >= m_descriptors.dockspaces.border_minimum_distance
							&& delta_scale < 0.0f){
							m_floating_vertical_dockspaces[dockspace_index].transform.position.y += delta_scale;
							m_floating_vertical_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
							return false;
						}
					}
					if (m_floating_vertical_dockspaces[dockspace_index].borders[0].is_dock) {
						should_resize_parent = ResizeDockspace(
							m_floating_vertical_dockspaces[dockspace_index].borders[0].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP,
							DockspaceType::Horizontal
						);
					}
					if (should_resize_parent) {
						for (size_t index = 1; index < m_floating_vertical_dockspaces[dockspace_index].borders.size; index++) {
							m_floating_vertical_dockspaces[dockspace_index].borders[index].position += delta_scale;
						}
					}
					else {
						m_floating_vertical_dockspaces[dockspace_index].transform.position.y += delta_scale;
						m_floating_vertical_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
					}
					return should_resize_parent;
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT) {
					m_floating_vertical_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					if (m_floating_vertical_dockspaces[dockspace_index].transform.scale.x < m_descriptors.dockspaces.mininum_scale) {
						m_floating_vertical_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
						return false;
					}
					else {
						should_resize_parent = ResizeDockspaceInternal(
							this,
							dockspace_index,
							delta_scale,
							border_type,
							m_floating_vertical_dockspaces,
							DockspaceType::FloatingVertical,
							[&]() {
								m_floating_vertical_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
							}
						);
						return should_resize_parent;
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM) {
					m_floating_vertical_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 1].position += delta_scale;
					if (m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 1].position
						- m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 2].position < m_descriptors.dockspaces.border_minimum_distance) {
						if (m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 1].position
							- m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 2].position - delta_scale >= m_descriptors.dockspaces.border_minimum_distance
							&& delta_scale < 0.0f) {
							m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 1].position -= delta_scale;
							m_floating_vertical_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
							return false;
						}
					}
					bool is_dock = m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 2].is_dock;
					if (is_dock) {
						should_resize_parent = ResizeDockspace(
							m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 2].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM,
							DockspaceType::Horizontal
						);
					}
					if (!should_resize_parent) {
						m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 1].position -= delta_scale;
						m_floating_vertical_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
					}
					return should_resize_parent;
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT) {
					delta_scale = -delta_scale;
					m_floating_vertical_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_floating_vertical_dockspaces[dockspace_index].transform.position.x -= delta_scale;
					if (m_floating_vertical_dockspaces[dockspace_index].transform.scale.x < m_descriptors.dockspaces.mininum_scale) {
						m_floating_vertical_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
						m_floating_vertical_dockspaces[dockspace_index].transform.position.x += delta_scale;
						return false;
					}
					else {
						should_resize_parent = ResizeDockspaceInternal(
							this,
							dockspace_index,
							delta_scale,
							border_type,
							m_floating_vertical_dockspaces,
							DockspaceType::FloatingVertical,
							[&]() {
								m_floating_vertical_dockspaces[dockspace_index].transform.scale.x -= delta_scale;
								m_floating_vertical_dockspaces[dockspace_index].transform.position.x += delta_scale;
							}
						);
						return should_resize_parent;
					}
				}
			}

			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ResizeDockspaceUnguarded(
			unsigned int dockspace_index,
			float delta_scale,
			ECS_UI_BORDER_TYPE border_type,
			DockspaceType dockspace_type
		) {
			bool should_resize_parent = true;
			if (dockspace_type == DockspaceType::Horizontal) {
				ECS_ASSERT(dockspace_index >= 0 && dockspace_index < m_horizontal_dockspaces.size);

				if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP) {
					//delta_scale = -delta_scale;
					m_horizontal_dockspaces[dockspace_index].transform.position.y -= delta_scale;
					m_horizontal_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					{
						ResizeDockspaceUnguarded(
							dockspace_index,
							delta_scale,
							border_type,
							DockspaceType::Horizontal
						);
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT) {
					m_horizontal_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 1].position += delta_scale;
					if (m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 2].is_dock) {
						ResizeDockspaceUnguarded(
							m_horizontal_dockspaces[dockspace_index].borders[m_horizontal_dockspaces[dockspace_index].borders.size - 2].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT,
							DockspaceType::Vertical
						);
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM) {
					//delta_scale = -delta_scale;
					m_horizontal_dockspaces[dockspace_index].transform.scale.y -= delta_scale;
					{
						ResizeDockspaceUnguarded(
							dockspace_index,
							delta_scale,
							border_type,
							DockspaceType::Horizontal
						);
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT) {
					//delta_scale = -delta_scale;
					m_horizontal_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_horizontal_dockspaces[dockspace_index].transform.position.x -= delta_scale;
					if (m_horizontal_dockspaces[dockspace_index].borders[0].is_dock) {
						ResizeDockspaceUnguarded(
							m_horizontal_dockspaces[dockspace_index].borders[0].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT,
							DockspaceType::Vertical
						);
					}
					for (size_t index = 1; index < m_horizontal_dockspaces[dockspace_index].borders.size; index++) {
						m_horizontal_dockspaces[dockspace_index].borders[index].position += delta_scale;
					}
				}
			}
			else if (dockspace_type == DockspaceType::Vertical) {
				ECS_ASSERT(dockspace_index >= 0 && dockspace_index < m_vertical_dockspaces.size);

				if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP) {
					//delta_scale = -delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.position.y -= delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					if (m_vertical_dockspaces[dockspace_index].borders[0].is_dock) {
						ResizeDockspaceUnguarded(
							m_vertical_dockspaces[dockspace_index].borders[0].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP,
							DockspaceType::Horizontal
						);
					}
					for (size_t index = 1; index < m_vertical_dockspaces[dockspace_index].borders.size; index++) {
						m_vertical_dockspaces[dockspace_index].borders[index].position += delta_scale;
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT) {
					//delta_scale = -delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					{
						ResizeDockspaceUnguarded(
							dockspace_index,
							delta_scale,
							border_type,
							DockspaceType::Vertical
						);
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM) {
					//delta_scale = -delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 1].position += delta_scale;
					if (m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 2].is_dock) {
						ResizeDockspaceUnguarded(
							m_vertical_dockspaces[dockspace_index].borders[m_vertical_dockspaces[dockspace_index].borders.size - 2].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM,
							DockspaceType::Horizontal
						);
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT) {
					//delta_scale = -delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_vertical_dockspaces[dockspace_index].transform.position.x -= delta_scale;
					{
						ResizeDockspaceUnguarded(
							dockspace_index,
							delta_scale,
							border_type,
							DockspaceType::Vertical
						);
					}
				}
			}
			else if (dockspace_type == DockspaceType::FloatingHorizontal) {
				ECS_ASSERT(dockspace_index >= 0 && dockspace_index < m_floating_horizontal_dockspaces.size);

				if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP) {
					delta_scale = -delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].transform.position.y -= delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					{
						ResizeDockspaceUnguarded(
							dockspace_index,
							delta_scale,
							border_type,
							DockspaceType::FloatingHorizontal
						);
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT) {
					//delta_scale = -delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 1].position += delta_scale;
					if (m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 2].is_dock) {
						ResizeDockspaceUnguarded(
							m_floating_horizontal_dockspaces[dockspace_index].borders[m_floating_horizontal_dockspaces[dockspace_index].borders.size - 2].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT,
							DockspaceType::Vertical
						);
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM) {
					m_floating_horizontal_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					{
						ResizeDockspaceUnguarded(
							dockspace_index,
							delta_scale,
							border_type,
							DockspaceType::FloatingHorizontal
						);
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT) {
					delta_scale = -delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_floating_horizontal_dockspaces[dockspace_index].transform.position.x -= delta_scale;
					if (m_floating_horizontal_dockspaces[dockspace_index].borders[0].is_dock) {
						ResizeDockspaceUnguarded(
							m_floating_horizontal_dockspaces[dockspace_index].borders[0].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT,
							DockspaceType::Vertical
						);
					}
					for (size_t index = 1; index < m_floating_horizontal_dockspaces[dockspace_index].borders.size; index++) {
						m_floating_horizontal_dockspaces[dockspace_index].borders[index].position += delta_scale;
					}
				}
			}
			else if (dockspace_type == DockspaceType::FloatingVertical) {
				ECS_ASSERT(dockspace_index >= 0 && dockspace_index < m_floating_vertical_dockspaces.size);

				if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP) {
					delta_scale = -delta_scale;
					m_floating_vertical_dockspaces[dockspace_index].transform.position.y -= delta_scale;
					m_floating_vertical_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					if (m_floating_vertical_dockspaces[dockspace_index].borders[0].is_dock) {
						ResizeDockspaceUnguarded(
							m_floating_vertical_dockspaces[dockspace_index].borders[0].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_TOP,
							DockspaceType::Horizontal
						);
					}
					for (size_t index = 1; index < m_floating_vertical_dockspaces[dockspace_index].borders.size; index++) {
						m_floating_vertical_dockspaces[dockspace_index].borders[index].position += delta_scale;
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_RIGHT) {
					m_floating_vertical_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					{
						ResizeDockspaceUnguarded(
							dockspace_index,
							delta_scale,
							border_type,
							DockspaceType::FloatingVertical
						);
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM) {
					m_floating_vertical_dockspaces[dockspace_index].transform.scale.y += delta_scale;
					m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 1].position += delta_scale;
					if (m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 2].is_dock) {
						ResizeDockspaceUnguarded (
							m_floating_vertical_dockspaces[dockspace_index].borders[m_floating_vertical_dockspaces[dockspace_index].borders.size - 2].window_indices[0],
							delta_scale,
							ECS_UI_BORDER_TYPE::ECS_UI_BORDER_BOTTOM,
							DockspaceType::Horizontal
						);
					}
				}
				else if (border_type == ECS_UI_BORDER_TYPE::ECS_UI_BORDER_LEFT) {
					delta_scale = -delta_scale;
					m_floating_vertical_dockspaces[dockspace_index].transform.scale.x += delta_scale;
					m_floating_vertical_dockspaces[dockspace_index].transform.position.x -= delta_scale;
					{
						ResizeDockspaceUnguarded(
							dockspace_index,
							delta_scale,
							border_type,
							DockspaceType::FloatingVertical
						);
					}
				}
			}
		
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RestoreWindow(Stream<char> window_name, const UIWindowDescriptor& descriptor)
		{
			unsigned int window_index = GetWindowFromName(window_name);
			if (m_windows[window_index].default_handler.data != nullptr) {
				UIDefaultWindowHandler* handler = GetDefaultWindowHandlerData(window_index);
				m_memory->Deallocate(handler->revert_commands.GetAllocatedBuffer());
			}

			SetWindowActions(window_index, descriptor);
			InitializeWindowDefaultHandler(window_index, descriptor);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::RepairDockspaceReferences(unsigned int dockspace_index, DockspaceType type, unsigned int new_index)
		{
			if (type == DockspaceType::Horizontal) {
				for (size_t index = 0; index < m_floating_vertical_dockspaces.size; index++) {
					UIDockspace* dockspace = &m_floating_vertical_dockspaces[index];
					for (size_t subindex = 0; subindex < dockspace->borders.size - 1; subindex++) {
						if (dockspace->borders[subindex].is_dock && dockspace_index == dockspace->borders[subindex].window_indices[0]) {
							dockspace->borders[subindex].window_indices[0] = new_index;
							return;
						}
					}
				}

				for (size_t index = 0; index < m_vertical_dockspaces.size; index++) {
					UIDockspace* dockspace = &m_vertical_dockspaces[index];
					for (size_t subindex = 0; subindex < dockspace->borders.size - 1; subindex++) {
						if (dockspace->borders[subindex].is_dock && dockspace_index == dockspace->borders[subindex].window_indices[0]) {
							dockspace->borders[subindex].window_indices[0] = new_index;
							return;
						}
					}
				}
				if (m_focused_window_data.active_location.dockspace == m_horizontal_dockspaces.buffer + dockspace_index) {
					m_focused_window_data.active_location.dockspace = m_horizontal_dockspaces.buffer + new_index;
				}
			}
			else if (type == DockspaceType::Vertical) {
				for (size_t index = 0; index < m_floating_horizontal_dockspaces.size; index++) {
					UIDockspace* dockspace = &m_floating_horizontal_dockspaces[index];
					for (size_t subindex = 0; subindex < dockspace->borders.size - 1; subindex++) {
						if (dockspace->borders[subindex].is_dock && dockspace_index == dockspace->borders[subindex].window_indices[0]) {
							dockspace->borders[subindex].window_indices[0] = new_index;
							return;
						}
					}
				}

				for (size_t index = 0; index < m_horizontal_dockspaces.size; index++) {
					UIDockspace* dockspace = &m_horizontal_dockspaces[index];
					for (size_t subindex = 0; subindex < dockspace->borders.size - 1; subindex++) {
						if (dockspace->borders[subindex].is_dock && dockspace_index == dockspace->borders[subindex].window_indices[0]) {
							dockspace->borders[subindex].window_indices[0] = new_index;
							return;
						}
					}
				}
				if (m_focused_window_data.active_location.dockspace == m_vertical_dockspaces.buffer + dockspace_index) {
					m_focused_window_data.active_location.dockspace = m_vertical_dockspaces.buffer + new_index;
				}
			}
			else if (type == DockspaceType::FloatingVertical || type == DockspaceType::FloatingHorizontal) {
				const UIDockspace* dockspaces[] = {
					m_horizontal_dockspaces.buffer,
					m_vertical_dockspaces.buffer,
					m_floating_horizontal_dockspaces.buffer,
					m_floating_vertical_dockspaces.buffer
				};
				for (size_t index = 0; index < m_dockspace_layers.size; index++) {
					if (m_dockspace_layers[index].type == type && m_dockspace_layers[index].index == dockspace_index) {
						m_dockspace_layers[index].index = new_index;
						break;
					}
				}
				FixFixedDockspace({ dockspace_index, type }, { new_index, type });
				FixBackgroundDockspace({ dockspace_index, type }, { new_index, type });
				
				if (m_focused_window_data.active_location.dockspace == m_floating_horizontal_dockspaces.buffer + dockspace_index) {
					m_focused_window_data.active_location.dockspace = m_floating_horizontal_dockspaces.buffer + new_index;
				}
				else if(m_focused_window_data.active_location.dockspace == m_floating_vertical_dockspaces.buffer + dockspace_index) {
					m_focused_window_data.active_location.dockspace = m_floating_vertical_dockspaces.buffer + new_index;
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		template<bool destroy_dockspace_if_last>
		void UISystem::RemoveWindowFromDockspaceRegion(UIDockspace* dockspace, DockspaceType type, unsigned int border_index, unsigned int in_border_index) {
			dockspace->borders[border_index].window_indices.RemoveSwapBack(in_border_index);
			if (dockspace->borders[border_index].active_window == in_border_index)
				dockspace->borders[border_index].active_window = 0;

			if constexpr (destroy_dockspace_if_last) {
				if (dockspace->borders[border_index].window_indices.size == 0) {
					RemoveDockspaceBorder(dockspace, border_index, type);
				}
			}
		}

		ECS_TEMPLATE_FUNCTION_BOOL(void, UISystem::RemoveWindowFromDockspaceRegion, UIDockspace*, DockspaceType, unsigned int, unsigned int);

		// -----------------------------------------------------------------------------------------------------------------------------------

		template<bool destroy_dockspace_if_last>
		void UISystem::RemoveWindowFromDockspaceRegion(unsigned int window_index)
		{
			DockspaceType type;
			unsigned int border_index;
			UIDockspace* dockspace = GetDockspaceFromWindow(window_index, border_index, type);

			if (dockspace != nullptr) {
				bool is_pop_up = IsPopUpWindow(window_index) != -1;
				if (is_pop_up) {
					DestroyDockspace(dockspace->borders.buffer, type);
				}
				else {
					for (size_t index = 0; index < dockspace->borders[border_index].window_indices.size; index++) {
						if (dockspace->borders[border_index].window_indices[index] == window_index) {
							RemoveWindowFromDockspaceRegion(dockspace, type, border_index, index);
						}
					}
				}
			}
		}

		ECS_TEMPLATE_FUNCTION_BOOL(void, UISystem::RemoveWindowFromDockspaceRegion, unsigned int);

		// -----------------------------------------------------------------------------------------------------------------------------------

		template<bool destroy_windows>
		void UISystem::RemoveDockspaceBorder(UIDockspace* dockspace, unsigned int border_index, DockspaceType type) {
			ECS_ASSERT(border_index < dockspace->borders.size - 1);

			bool is_fixed = IsFixedDockspace(dockspace);

			if constexpr (destroy_windows) {
				for (size_t index = 0; index < dockspace->borders[border_index].window_indices.size; index++) {
					DestroyWindow(dockspace->borders[border_index].window_indices[index]);
				}
				if (is_fixed) {
					dockspace->borders[border_index].window_indices.size = 0;
					dockspace->borders[border_index].draw_close_x = false;
					dockspace->borders[border_index].draw_elements = false;
					dockspace->borders[border_index].draw_region_header = false;
				}
			}

			if (!is_fixed || dockspace->borders.size > 2) {
				DeallocateDockspaceBorderResource(dockspace, border_index);

				if (border_index == 0) {
					if (dockspace->borders[border_index + 1].is_dock) {
						if (type == DockspaceType::Horizontal || type == DockspaceType::FloatingHorizontal) {
							AppendDockspaceResize(&m_vertical_dockspaces[dockspace->borders[border_index + 1].window_indices[0]], dockspace->borders[border_index + 2].position, DockspaceType::Vertical);
						}
						else if (type == DockspaceType::Vertical || type == DockspaceType::FloatingVertical) {
							AppendDockspaceResize(&m_horizontal_dockspaces[dockspace->borders[border_index + 1].window_indices[0]], dockspace->borders[border_index + 2].position, DockspaceType::Horizontal);
						}
					}
				}
				else if (border_index < dockspace->borders.size - 2) {
					if (dockspace->borders[border_index + 1].is_dock) {
						if (type == DockspaceType::Horizontal || type == DockspaceType::FloatingHorizontal) {
							AppendDockspaceResize(&m_vertical_dockspaces[dockspace->borders[border_index + 1].window_indices[0]], dockspace->borders[border_index + 2].position - dockspace->borders[border_index].position, DockspaceType::Vertical);
						}
						else if (type == DockspaceType::Vertical || type == DockspaceType::FloatingVertical) {
							AppendDockspaceResize(&m_horizontal_dockspaces[dockspace->borders[border_index + 1].window_indices[0]], dockspace->borders[border_index + 2].position - dockspace->borders[border_index].position, DockspaceType::Horizontal);
						}
					}
				}
				else {
					if (dockspace->borders[border_index - 1].is_dock) {
						if (type == DockspaceType::Horizontal || type == DockspaceType::FloatingHorizontal) {
							AppendDockspaceResize(&m_vertical_dockspaces[dockspace->borders[border_index - 1].window_indices[0]], dockspace->borders[border_index + 1].position - dockspace->borders[border_index - 1].position, DockspaceType::Vertical);
						}
						else if (type == DockspaceType::Vertical || type == DockspaceType::FloatingVertical) {
							AppendDockspaceResize(&m_horizontal_dockspaces[dockspace->borders[border_index - 1].window_indices[0]], dockspace->borders[border_index + 1].position - dockspace->borders[border_index - 1].position, DockspaceType::Horizontal);
						}
					}
				}
			}

			if (dockspace == m_focused_window_data.active_location.dockspace && border_index == m_focused_window_data.active_location.border_index) {
				if (border_index > 0 && border_index == dockspace->borders.size - 2) {
					SearchAndSetNewFocusedDockspaceRegion(dockspace, border_index - 1, type);
				}
				else {
					SearchAndSetNewFocusedDockspaceRegion(dockspace, border_index, type);
				}
			}

			bool has_been_destroyed = false;
			// one higher because substraction of size must happen after
			if ((type == DockspaceType::Horizontal || type == DockspaceType::Vertical) && dockspace->borders.size <= 3) {
				ReplaceDockspace(dockspace, border_index, type);
				dockspace->borders.size--;
				DestroyDockspace(dockspace->borders.buffer, type);
				has_been_destroyed = true;
			}
			// one higher because substraction of size must happen after
			else if ((type == DockspaceType::FloatingHorizontal || type == DockspaceType::FloatingVertical)) {
				if (dockspace->borders.size == 3 && dockspace->borders[1 - border_index].is_dock) {
					bool is_floating_vertical = type == DockspaceType::FloatingVertical;
					unsigned int dockspace_border_to_copy = 1 - border_index;
					UIDockspace* dockspaces[] = {
						m_horizontal_dockspaces.buffer,
						m_vertical_dockspaces.buffer,
						m_floating_horizontal_dockspaces.buffer,
						m_floating_vertical_dockspaces.buffer
					};
					unsigned int new_dockspace = CreateDockspace(
						&dockspaces[1 - is_floating_vertical][dockspace->borders[dockspace_border_to_copy].window_indices[0]],
						DockspaceType(2 + !is_floating_vertical)
					);
					dockspaces[2 + !is_floating_vertical][new_dockspace].transform = dockspace->transform;

					unsigned int dockspace_index = GetDockspaceIndex(dockspace, type);
					UIDockspaceLayer old_layer = { dockspace_index, type };
					UIDockspaceLayer new_layer = { new_dockspace, DockspaceType(2 + !is_floating_vertical) };
					FixBackgroundDockspace(old_layer, new_layer);
					FixFixedDockspace(old_layer, new_layer);

					dockspace->borders.size--;
					dockspaces[1 - is_floating_vertical][dockspace->borders[dockspace_border_to_copy].window_indices[0]].borders.size--;
					DestroyDockspace(dockspaces[1 - is_floating_vertical][dockspace->borders[dockspace_border_to_copy].window_indices[0]].borders.buffer, DockspaceType(1 - is_floating_vertical));
					DestroyDockspace(dockspace->borders.buffer, type);

					has_been_destroyed = true;
				}
				else if (dockspace->borders.size <= 2 && !is_fixed) {
					dockspace->borders.size--;
					DestroyDockspace(dockspace->borders.buffer, type);
					has_been_destroyed = true;
				}
			}
			if (is_fixed && dockspace->borders.size == 2) {
				dockspace->borders[border_index].hoverable_handler.Reset();
				dockspace->borders[border_index].clickable_handler.Reset();
				dockspace->borders[border_index].general_handler.Reset();
			}
			else if (!has_been_destroyed) {
				float border_offset = dockspace->borders[border_index].position;
				for (size_t index = border_index; index < dockspace->borders.size - 1; index++) {
					dockspace->borders[index] = dockspace->borders[index + 1];
				}

				if (border_index != dockspace->borders.size - 2)
					dockspace->borders[border_index].position = border_offset;

				dockspace->borders.size--;
			}
		}

		ECS_TEMPLATE_FUNCTION_BOOL(void, UISystem::RemoveDockspaceBorder, UIDockspace*, unsigned int, DockspaceType);

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::RepairWindowReferences(unsigned int window_index)
		{
			DockspaceType removed_type;
			unsigned int removed_border;
			UIDockspace* removed_dockspace = GetDockspaceFromWindow(window_index, removed_border, removed_type);

			m_windows.RemoveSwapBack(window_index);
			size_t swapped_index = m_windows.size;

			bool continue_searching = true;
			// repair references to the swapped window
			auto lambda_search = [&continue_searching, swapped_index, window_index](CapacityStream<UIDockspace>& dockspaces) {
				for (size_t index = 0; continue_searching && index < dockspaces.size; index++) {
					UIDockspace* dockspace = &dockspaces[index];
					for (size_t subindex = 0; subindex < dockspaces[index].borders.size - 1 && continue_searching; subindex++) {
						if (!dockspace->borders[subindex].is_dock) {
							for (size_t index_to_search = 0; index_to_search < dockspace->borders[subindex].window_indices.size; index_to_search++) {
								if (dockspace->borders[subindex].window_indices[index_to_search] == swapped_index) {
									dockspace->borders[subindex].window_indices[index_to_search] = window_index;
									continue_searching = false;
									break;
								}
							}
						}
					}
				}
			};
			lambda_search(m_floating_horizontal_dockspaces);
			lambda_search(m_floating_vertical_dockspaces);
			lambda_search(m_horizontal_dockspaces);
			lambda_search(m_vertical_dockspaces);
			// sometimes the last one may already be deleted if using delete non referenced
			return !continue_searching;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::ReplaceDockspace(UIDockspace* dockspace, unsigned int border_index, DockspaceType type)
		{
			DockspaceType parent_type;
			unsigned int parent_border_index;
			UIDockspace* parent_dockspace = GetDockspaceParent(dockspace, type, parent_type, parent_border_index);

			const UIDockspace* dockspaces[4] = {
				m_vertical_dockspaces.buffer,
				m_horizontal_dockspaces.buffer,
				m_vertical_dockspaces.buffer,
				m_horizontal_dockspaces.buffer
			};
			const DockspaceType types[] = {
				DockspaceType::Vertical,
				DockspaceType::Horizontal,
				DockspaceType::Vertical,
				DockspaceType::Horizontal
			};
			const float masks[] = { 1.0f, 0.0f, 1.0f, 0.0f };

			if (border_index == 0 && dockspace->borders[border_index + 1].is_dock) {
				const UIDockspace* child_dockspace = &dockspaces[(unsigned int)type][dockspace->borders[border_index + 1].window_indices[0]];

				// bumping forwards the parent borders after the current one
				ECS_ASSERT(parent_dockspace->borders.capacity >= parent_dockspace->borders.size + child_dockspace->borders.size - 2);
				for (int64_t index = parent_dockspace->borders.size - 1; index > parent_border_index; index--) {
					parent_dockspace->borders[index + 1] = parent_dockspace->borders[index];
				}
				parent_dockspace->borders.size += child_dockspace->borders.size - 2;

				// not assigning because the draw resources and border ones must be separate
				parent_dockspace->borders[parent_border_index].is_dock = child_dockspace->borders[0].is_dock;
				parent_dockspace->borders[parent_border_index].active_window = child_dockspace->borders[0].active_window;
				parent_dockspace->borders[parent_border_index].draw_region_header = child_dockspace->borders[0].draw_region_header;
				parent_dockspace->borders[parent_border_index].window_indices[0] = child_dockspace->borders[0].window_indices[0];
				parent_dockspace->borders[parent_border_index].draw_close_x = child_dockspace->borders[0].draw_close_x;
				parent_dockspace->borders[parent_border_index].window_indices.size = 1;
				for (size_t subindex = 1; subindex < child_dockspace->borders[0].window_indices.size; subindex++) {
					AddWindowToDockspaceRegion(child_dockspace->borders[0].window_indices[subindex], parent_dockspace, parent_border_index);
				}

				// adding the new dockspace borders from child
				float border_offset = parent_dockspace->borders[parent_border_index].position;
				for (size_t index = 1; index < child_dockspace->borders.size - 1; index++) {
					unsigned int current_border_index = parent_border_index + index;
					CreateDockspaceBorder(
						parent_dockspace,
						current_border_index,
						border_offset + child_dockspace->borders[index].position,
						child_dockspace->borders[index].window_indices[0],
						child_dockspace->borders[index].is_dock
					);
					parent_dockspace->borders[current_border_index].window_indices.size = 1;
					for (size_t subindex = 1; subindex < child_dockspace->borders[index].window_indices.size; subindex++) {
						AddWindowToDockspaceRegion(child_dockspace->borders[index].window_indices[subindex], parent_dockspace, current_border_index);
					}
					parent_dockspace->borders[current_border_index].active_window = child_dockspace->borders[index].active_window;
					parent_dockspace->borders[current_border_index].draw_elements = child_dockspace->borders[index].draw_elements;
					parent_dockspace->borders[current_border_index].draw_region_header = child_dockspace->borders[index].draw_region_header;
					parent_dockspace->borders[current_border_index].draw_close_x = child_dockspace->borders[index].draw_close_x;
				}

				float mask = masks[(unsigned int)parent_type];
				float inverse_mask = 1.0f - mask;
				parent_dockspace->borders[parent_dockspace->borders.size - 1].position = parent_dockspace->transform.scale.x * mask + inverse_mask * parent_dockspace->transform.scale.y;
				DestroyDockspace(child_dockspace->borders.buffer, types[(unsigned int)type]);
			}
			else if (border_index > 0 && dockspace->borders[border_index - 1].is_dock){
				const UIDockspace* child_dockspace = &dockspaces[(unsigned int)type][dockspace->borders[border_index - 1].window_indices[0]];
				
				// bumping forwards the parent borders after the current one
				ECS_ASSERT(parent_dockspace->borders.capacity >= parent_dockspace->borders.size + child_dockspace->borders.size - 2);
				for (int64_t index = parent_dockspace->borders.size - 1; index > parent_border_index; index--) {
					parent_dockspace->borders[index + 1] = parent_dockspace->borders[index];
				}
				parent_dockspace->borders.size += child_dockspace->borders.size - 2;

				// not assigning because the draw resources and border ones must be separate
				parent_dockspace->borders[parent_border_index].is_dock = child_dockspace->borders[0].is_dock;
				parent_dockspace->borders[parent_border_index].active_window = child_dockspace->borders[0].active_window;
				parent_dockspace->borders[parent_border_index].draw_region_header = child_dockspace->borders[0].draw_region_header;
				parent_dockspace->borders[parent_border_index].draw_elements = child_dockspace->borders[0].draw_elements;
				parent_dockspace->borders[parent_border_index].draw_close_x = child_dockspace->borders[0].draw_close_x;
				parent_dockspace->borders[parent_border_index].window_indices[0] = child_dockspace->borders[0].window_indices[0];
				parent_dockspace->borders[parent_border_index].window_indices.size = 1;
				for (size_t subindex = 1; subindex < child_dockspace->borders[0].window_indices.size; subindex++) {
					AddWindowToDockspaceRegion(child_dockspace->borders[0].window_indices[subindex], parent_dockspace, parent_border_index);
				}

				// adding the new dockspace borders from child
				float border_offset = parent_dockspace->borders[parent_border_index].position;
				for (size_t index = 1; index < child_dockspace->borders.size - 1; index++) {
					unsigned int current_border_index = parent_border_index + index;
					CreateDockspaceBorder(
						parent_dockspace,
						current_border_index,
						border_offset + child_dockspace->borders[index].position,
						child_dockspace->borders[index].window_indices[0],
						child_dockspace->borders[index].is_dock
					);
					parent_dockspace->borders[current_border_index].window_indices.size = 1;
					for (size_t subindex = 1; subindex < child_dockspace->borders[index].window_indices.size; subindex++) {
						AddWindowToDockspaceRegion(child_dockspace->borders[index].window_indices[subindex], parent_dockspace, current_border_index);
					}
					parent_dockspace->borders[current_border_index].active_window = child_dockspace->borders[index].active_window;
					parent_dockspace->borders[current_border_index].draw_region_header = child_dockspace->borders[index].draw_region_header;
					parent_dockspace->borders[current_border_index].draw_elements = child_dockspace->borders[index].draw_elements;
					parent_dockspace->borders[current_border_index].draw_close_x = child_dockspace->borders[index].draw_close_x;
				}
			}
			else {
				UIDockspaceBorder* border = border_index == 0 ? &dockspace->borders[1] : &dockspace->borders[0];
				
				// not assigning because the draw resources and border ones must be separate
				parent_dockspace->borders[parent_border_index].is_dock = border->is_dock;
				parent_dockspace->borders[parent_border_index].active_window = border->active_window;
				parent_dockspace->borders[parent_border_index].draw_region_header = border->draw_region_header;
				parent_dockspace->borders[parent_border_index].draw_elements = border->draw_elements;
				parent_dockspace->borders[parent_border_index].draw_close_x = border->draw_close_x;
				parent_dockspace->borders[parent_border_index].window_indices[0] = border->window_indices[0];
				parent_dockspace->borders[parent_border_index].window_indices.size = 1;
				for (size_t subindex = 1; subindex < border->window_indices.size; subindex++) {
					AddWindowToDockspaceRegion(border->window_indices[subindex], parent_dockspace, parent_border_index);
				}
			}

		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int UISystem::SearchDockspaceForChildrenDockspaces(
			float2 mouse_position,
			unsigned int dockspace_index,
			DockspaceType dockspace_type,
			DockspaceType& children_type,
			unsigned int* parent_indices,
			DockspaceType* dockspace_types,
			unsigned int& parent_count
		) {
			unsigned int child_index = 0xFFFFFFFF;
			if (dockspace_type == DockspaceType::Horizontal) {
				for (size_t index = 0; child_index == 0xFFFFFFFF && index < m_horizontal_dockspaces[dockspace_index].borders.size - 1; index++) {
					if (m_horizontal_dockspaces[dockspace_index].borders[index].is_dock) {
						child_index = SearchDockspaceForChildrenDockspaces(
							mouse_position, 
							m_horizontal_dockspaces[dockspace_index].borders[index].window_indices[0], 
							DockspaceType::Vertical, 
							children_type,
							parent_indices,
							dockspace_types,
							parent_count
						);
					}
				}
				if (IsPointInRectangle(
					mouse_position,
					m_horizontal_dockspaces[dockspace_index].transform.position,
					m_horizontal_dockspaces[dockspace_index].transform.scale)
					) {
					parent_indices[parent_count] = dockspace_index;
					dockspace_types[parent_count++] = DockspaceType::Horizontal;
					if (child_index != 0xFFFFFFFF) {
						return child_index;
					}
					else {
						children_type = DockspaceType::Horizontal;
					}
					return dockspace_index;
				}
				return 0xFFFFFFFF;
			}
			else if (dockspace_type == DockspaceType::Vertical) {
				for (size_t index = 0; child_index == 0xFFFFFFFF && index < m_vertical_dockspaces[dockspace_index].borders.size - 1; index++) {
					if (m_vertical_dockspaces[dockspace_index].borders[index].is_dock) {
						child_index = SearchDockspaceForChildrenDockspaces(
							mouse_position, 
							m_vertical_dockspaces[dockspace_index].borders[index].window_indices[0], 
							DockspaceType::Horizontal, 
							children_type,
							parent_indices,
							dockspace_types,
							parent_count
						);
					}
				}
				if (IsPointInRectangle(
					mouse_position,
					m_vertical_dockspaces[dockspace_index].transform.position,
					m_vertical_dockspaces[dockspace_index].transform.scale)
					) {
					parent_indices[parent_count] = dockspace_index;
					dockspace_types[parent_count++] = DockspaceType::Vertical;
					//children_type = DockspaceType::Vertical;
					if (child_index != 0xFFFFFFFF) {
						return child_index;
					}
					else {
						children_type = DockspaceType::Vertical;
					}
					return dockspace_index;
				}
				return 0xFFFFFFFF;
			}

			return 0xFFFFFFFF;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetActiveWindow(unsigned int window_index) {
			DockspaceType type;
			unsigned int border_index;
			UIDockspace* dockspace = GetDockspaceFromWindow(window_index, border_index, type);

			float mask = GetDockspaceMaskFromType(type);
			DockspaceType floating_type = type;
			UIDockspace* floating_dockspace = GetFloatingDockspaceFromDockspace(dockspace, mask, floating_type);
			SetNewFocusedDockspace(floating_dockspace, floating_type);
			SetNewFocusedDockspaceRegion(dockspace, border_index, type);

			for (size_t index = 0; index < dockspace->borders[border_index].window_indices.size; index++) {
				if (dockspace->borders[border_index].window_indices[index] == window_index) {
					dockspace->borders[border_index].active_window = index;
					break;
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetActiveWindow(Stream<char> name)
		{
			unsigned int window_index = GetWindowFromName(name);
			if (window_index != -1) {
				SetActiveWindow(window_index);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetCleanupGeneralHandler()
		{
			m_focused_window_data.clean_up_call_general = true;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetCleanupHoverableHandler()
		{
			m_focused_window_data.clean_up_call_hoverable = true;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetSystemDrawRegion(UIElementTransform transform)
		{
			m_system_draw_region = transform;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetTextureEvictionCount(unsigned int frame_count)
		{
			m_texture_evict_target = frame_count;
			m_texture_evict_count = 0;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::SetColorTheme(Color color)
		{
			m_descriptors.color_theme.SetNewTheme(color);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::UpdateDockspace(unsigned int index, DockspaceType type)
		{
			if (type == DockspaceType::FloatingHorizontal) {
				CapacityStream<UIDockspaceBorder>* borders = &m_floating_horizontal_dockspaces[index].borders;
				for (size_t border_index = 0; border_index < borders->size - 1; border_index++) {
					// element transform
					// Position x: parent dockspace x + border offset
					// Position y: parent dockspace y
					// Scale    x: difference between next border offset and the current one
					// Scale    y: parent dockspace x
					UIElementTransform element_transform(
						float2(
							(m_floating_horizontal_dockspaces[index].transform.position.x + borders->buffer[border_index].position),
							m_floating_horizontal_dockspaces[index].transform.position.y
						),
						float2(
							(borders->buffer[border_index + 1].position - borders->buffer[border_index].position),
							m_floating_horizontal_dockspaces[index].transform.scale.y
						)
					);
					if (borders->buffer[border_index].is_dock) {
						// since it is a floating horizontal dockspace, it cannot contain other horizontal dockspaces or floating ones
						m_vertical_dockspaces[borders->buffer[border_index].window_indices[0]].transform = element_transform;
						UpdateDockspace(borders->buffer[border_index].window_indices[0], DockspaceType::Vertical);
					}
					else {
						// set window transform 
						for (size_t window_index = 0; window_index < borders->buffer[border_index].window_indices.size; window_index++) {
							m_windows[borders->buffer[border_index].window_indices[window_index]].transform = element_transform;
						}
					}
				}
			}
			else if (type == DockspaceType::FloatingVertical) {
				CapacityStream<UIDockspaceBorder>* borders = &m_floating_vertical_dockspaces[index].borders;
				for (size_t border_index = 0; border_index < borders->size - 1; border_index++) {
					// element transform
					// Position x: parent dockspace x
					// Position y: parent dockspace y + border offset
					// Scale    x: parent dockspace x
					// Scale    y: difference between next border offset and the current one
					UIElementTransform element_transform(
						float2(
							m_floating_vertical_dockspaces[index].transform.position.x,
							(m_floating_vertical_dockspaces[index].transform.position.y + borders->buffer[border_index].position)
						),
						float2(
							m_floating_vertical_dockspaces[index].transform.scale.x,
							(borders->buffer[border_index + 1].position - borders->buffer[border_index].position)
						)
					);
					if (borders->buffer[border_index].is_dock) {
						// since it is a floating vertical dockspace, it cannot contain other vertical dockspaces or floating one
						m_horizontal_dockspaces[borders->buffer[border_index].window_indices[0]].transform = element_transform;
						UpdateDockspace(borders->buffer[border_index].window_indices[0], DockspaceType::Horizontal);
					}
					else {
						// set window transform 
						for (size_t window_index = 0; window_index < borders->buffer[border_index].window_indices.size; window_index++) {
							m_windows[borders->buffer[border_index].window_indices[window_index]].transform = element_transform;
						}
					}
				}
			}
			else if (type == DockspaceType::Horizontal) {
				CapacityStream<UIDockspaceBorder>* borders = &m_horizontal_dockspaces[index].borders;
				for (size_t border_index = 0; border_index < borders->size - 1; border_index++) {
					// element transform: parent dockspace means this current dockspace
					// Position x: parent dockspace x + border offset
					// Position y: parent dockspace y
					// Scale    x: difference between next border offset and the current one
					// Scale    y: parent dockspace y
					UIElementTransform element_transform(
						float2(
							(m_horizontal_dockspaces[index].transform.position.x + borders->buffer[border_index].position),
							m_horizontal_dockspaces[index].transform.position.y
						),
						float2(
							(borders->buffer[border_index + 1].position - borders->buffer[border_index].position),
							m_horizontal_dockspaces[index].transform.scale.y
						)
					);
					if (borders->buffer[border_index].is_dock) {
						// since it is a horizontal dockspace, it cannot contain other horizontal dockspaces
						m_vertical_dockspaces[borders->buffer[border_index].window_indices[0]].transform = element_transform;
						UpdateDockspace(borders->buffer[border_index].window_indices[0], DockspaceType::Vertical);
					}
					else {
						// set window transform 
						for (size_t window_index = 0; window_index < borders->buffer[border_index].window_indices.size; window_index++) {
							m_windows[borders->buffer[border_index].window_indices[window_index]].transform = element_transform;
						}
					}
				}
			}
			else if (type == DockspaceType::Vertical) {
				CapacityStream<UIDockspaceBorder>* borders = &m_vertical_dockspaces[index].borders;
				for (size_t border_index = 0; border_index < borders->size - 1; border_index++) {
					// element transform
					// Position x: parent dockspace x
					// Position y: parent dockspace y + border offset
					// Scale    x: parent dockspace x
					// Scale    y: difference between next border offset and the current one
					UIElementTransform element_transform(
						float2(
							m_vertical_dockspaces[index].transform.position.x,
							(m_vertical_dockspaces[index].transform.position.y + borders->buffer[border_index].position)
						),
						float2(
							m_vertical_dockspaces[index].transform.scale.x,
							(borders->buffer[border_index + 1].position - borders->buffer[border_index].position)
						)
					);
					if (borders->buffer[border_index].is_dock) {
						// since it is a floating vertical dockspace, it cannot contain other vertical dockspaces or floating one
						m_horizontal_dockspaces[borders->buffer[border_index].window_indices[0]].transform = element_transform;
						UpdateDockspace(borders->buffer[border_index].window_indices[0], DockspaceType::Horizontal);
					}
					else {
						// set window transform 
						for (size_t window_index = 0; window_index < borders->buffer[border_index].window_indices.size; window_index++) {
							m_windows[borders->buffer[border_index].window_indices[window_index]].transform = element_transform;
						}
					}
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::UpdateFocusedWindowCleanupLocation()
		{
			// Just copy the current active location for the general handler and the current hovered location for the hoverable handler
			m_focused_window_data.cleanup_general_location = m_focused_window_data.active_location;
			m_focused_window_data.cleanup_hoverable_location = m_focused_window_data.hovered_location;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void UISystem::UpdateDockspaceHierarchy() {
			for (size_t index = 0; index < m_dockspace_layers.size; index++) {
				UpdateDockspace(m_dockspace_layers[index].index, m_dockspace_layers[index].type);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::WriteDescriptorsFile(Stream<wchar_t> filename) const
		{
			ECS_FILE_HANDLE file = 0;
			ECS_FILE_STATUS_FLAGS status = OpenFile(filename, &file, ECS_FILE_ACCESS_BINARY | ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TRUNCATE_FILE);

			if (status == ECS_FILE_STATUS_OK) {
				unsigned int version = ECS_TOOLS_UI_DESCRIPTOR_FILE_VERSION;
				bool success = WriteFile(file, { &version, sizeof(version) });
				success &= WriteFile(file, { &m_descriptors, sizeof(UISystemDescriptors) });

				CloseFile(file);
				return success;
			}

			return false;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		bool UISystem::WriteUIFile(Stream<wchar_t> filename, CapacityStream<char>& error_message) const
		{
			ECS_FILE_HANDLE file = 0;
			CapacityStream<char>* open_file_error_message = error_message.buffer != nullptr ? &error_message : nullptr;
			ECS_FILE_STATUS_FLAGS status = FileCreate(
				filename,
				&file,
				ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_BINARY | ECS_FILE_ACCESS_TRUNCATE_FILE,
				ECS_FILE_CREATE_READ_WRITE,
				open_file_error_message
			);

			if (status == ECS_FILE_STATUS_OK) {
				// The file will be closed manually
				const char* FIRST_ERROR = "Writing file contents to ";
				const char* SECOND_ERROR = " failed!";

#pragma region Header file - versions, metadata

				unsigned short versions[2] = { ECS_TOOLS_UI_FILE_VERSION, ECS_TOOLS_UI_FILE_WINDOW_DESCRIPTOR_VERSION };

				unsigned char metadata[32];

				bool success = true;

				success &= WriteFile(file, { versions, sizeof(versions) });
				success &= WriteFile(file, { metadata, sizeof(metadata) });

#pragma endregion

				unsigned short dockspace_counts[] = {
					m_horizontal_dockspaces.size,
					m_vertical_dockspaces.size,
					m_floating_horizontal_dockspaces.size,
					m_floating_vertical_dockspaces.size
				};

				UIDockspaceLayer pop_up_layers[32];
				Stream<UIDockspaceLayer> pop_up_stream(pop_up_layers, 0);

				UIDockspaceLayer dockspace_layers[32];
				Stream<UIDockspaceLayer> stack_dockspace_layers(dockspace_layers, 0);
				stack_dockspace_layers.Copy(m_dockspace_layers);

				unsigned short _valid_layers[32];
				Stream<unsigned short> valid_layers(_valid_layers, m_dockspace_layers.size);

				unsigned short _pop_up_windows[32];
				Stream<unsigned short> pop_up_windows(_pop_up_windows, 0);

				unsigned short _valid_horizontals[64];
				unsigned short _valid_verticals[64];
				unsigned short _valid_floating_horizontals[64];
				unsigned short _valid_floating_verticals[64];
				Stream<unsigned short> valid_horizontals(_valid_horizontals, m_horizontal_dockspaces.size);
				Stream<unsigned short> valid_verticals(_valid_verticals, m_vertical_dockspaces.size);
				Stream<unsigned short> valid_floating_horizontals(_valid_floating_horizontals, m_floating_horizontal_dockspaces.size);
				Stream<unsigned short> valid_floating_verticals(_valid_floating_verticals, m_floating_vertical_dockspaces.size);

				Stream<unsigned short>* valids[] = { &valid_horizontals, &valid_verticals, &valid_floating_horizontals, &valid_floating_verticals };

				function::MakeSequence(valid_layers);
				function::MakeSequence(valid_horizontals);
				function::MakeSequence(valid_verticals);
				function::MakeSequence(valid_floating_horizontals);
				function::MakeSequence(valid_floating_verticals);

				for (size_t index = 0; index < m_pop_up_windows.size; index++) {
					unsigned int layer_index = m_pop_up_windows[index];
					const UIDockspace* dockspace = GetConstDockspace(m_dockspace_layers[layer_index]);
					pop_up_stream.Add(m_dockspace_layers[layer_index]);
					pop_up_windows.Add(dockspace->borders[0].window_indices[0]);
					dockspace_counts[(unsigned int)m_dockspace_layers[layer_index].type]--;

					Stream<unsigned short>* stream = valids[(unsigned int)m_dockspace_layers[layer_index].type];
					for (size_t subindex = 0; subindex < stream->size; subindex++) {
						if (stream->buffer[subindex] == m_dockspace_layers[layer_index].index) {
							stream->RemoveSwapBack(subindex);

							valid_layers.Remove(layer_index);
							for (subindex = 0; subindex < stack_dockspace_layers.size; subindex++) {
								bool decrement = stack_dockspace_layers[subindex].type == m_dockspace_layers[layer_index].type
									&& stack_dockspace_layers[subindex].index > m_dockspace_layers[layer_index].index;
								stack_dockspace_layers[subindex].index -= decrement;
							}
							break;
						}
					}
				}

				function::insertion_sort(pop_up_windows.buffer, pop_up_windows.size);
				/*std::sort(pop_up_windows.buffer, pop_up_windows.buffer + pop_up_windows.size, [](unsigned short a, unsigned short b) {
					return a < b;
				});*/

#pragma region Dockspaces

				const size_t temp_memory_size = 8192;

				success &= WriteFile(file, { dockspace_counts, sizeof(dockspace_counts) });
				char temp_memory[temp_memory_size];

				unsigned short _dockspace_window_replacement[256];
				unsigned short* _dockspace_window_replacement_pointers[16];
				Stream<unsigned short*> dockspace_window_replacement(_dockspace_window_replacement_pointers, 16);
				for (size_t index = 0; index < 16; index++) {
					dockspace_window_replacement[index] = _dockspace_window_replacement + 16 * index;
				}

				auto copy_border_window_indices = [dockspace_window_replacement](const UIDockspace* dockspace) {
					for (size_t index = 0; index < dockspace->borders.size - 1; index++) {
						dockspace->borders[index].window_indices.CopyTo(dockspace_window_replacement[index]);
					}
				};

				auto solve_border_references = [pop_up_windows](UIDockspace* dockspace) {
					for (size_t index = 0; index < dockspace->borders.size - 1; index++) {
						if (!dockspace->borders[index].is_dock) {
							for (size_t window_index = 0; window_index < dockspace->borders[index].window_indices.size; window_index++) {
								for (size_t pop_index = 0; pop_index < pop_up_windows.size; pop_index++) {
									dockspace->borders[index].window_indices[window_index] -= dockspace->borders[index].window_indices[window_index] > pop_up_windows[pop_index];
								}
							}
						}
					}
				};

				auto restore_border_window_indices = [dockspace_window_replacement](UIDockspace* dockspace) {
					for (size_t index = 0; index < dockspace->borders.size - 1; index++) {
						if (!dockspace->borders[index].is_dock) {
							memcpy(dockspace->borders[index].window_indices.buffer, dockspace_window_replacement[index], sizeof(unsigned short) * dockspace->borders[index].window_indices.size);
						}
					}
				};

				for (size_t index = 0; index < valid_horizontals.size && success; index++) {
					copy_border_window_indices(m_horizontal_dockspaces.buffer + valid_horizontals[index]);
					solve_border_references(m_horizontal_dockspaces.buffer + valid_horizontals[index]);
					size_t size = m_horizontal_dockspaces[valid_horizontals[index]].Serialize(temp_memory);
					restore_border_window_indices(m_horizontal_dockspaces.buffer + valid_horizontals[index]);
					success &= WriteFile(file, { temp_memory, size });
				}
				for (size_t index = 0; index < valid_verticals.size && success; index++) {
					copy_border_window_indices(m_vertical_dockspaces.buffer + valid_verticals[index]);
					solve_border_references(m_vertical_dockspaces.buffer + valid_verticals[index]);
					size_t size = m_vertical_dockspaces[valid_verticals[index]].Serialize(temp_memory);
					restore_border_window_indices(m_vertical_dockspaces.buffer + valid_verticals[index]);
					success &= WriteFile(file, { temp_memory, size });
				}
				for (size_t index = 0; index < valid_floating_horizontals.size && success; index++) {
					copy_border_window_indices(m_floating_horizontal_dockspaces.buffer + valid_floating_horizontals[index]);
					solve_border_references(m_floating_horizontal_dockspaces.buffer + valid_floating_horizontals[index]);
					size_t size = m_floating_horizontal_dockspaces[valid_floating_horizontals[index]].Serialize(temp_memory);
					restore_border_window_indices(m_floating_horizontal_dockspaces.buffer + valid_floating_horizontals[index]);
					success &= WriteFile(file, { temp_memory, size });
				}
				for (size_t index = 0; index < valid_floating_verticals.size && success; index++) {
					copy_border_window_indices(m_floating_vertical_dockspaces.buffer + valid_floating_verticals[index]);
					solve_border_references(m_floating_vertical_dockspaces.buffer + valid_floating_verticals[index]);
					size_t size = m_floating_vertical_dockspaces[valid_floating_verticals[index]].Serialize(temp_memory);
					restore_border_window_indices(m_floating_vertical_dockspaces.buffer + valid_floating_verticals[index]);
					success &= WriteFile(file, { temp_memory, size });
				}

#pragma endregion

#pragma region Windows

				unsigned int valid_windows[128];
				Stream<unsigned int> valid_stream(valid_windows, m_windows.size);
				function::MakeSequence(valid_stream);

				for (size_t index = 0; index < pop_up_stream.size; index++) {
					for (size_t subindex = 0; subindex < valid_stream.size; subindex++) {
						const UIDockspace* dockspace = GetConstDockspace(pop_up_stream[index]);
						if (valid_stream[subindex] == dockspace->borders[0].window_indices[0]) {
							valid_stream.Remove(subindex);
							break;
						}
					}
				}

				unsigned short window_count = valid_stream.size;
				success &= WriteFile(file, { &window_count, sizeof(window_count) });

				for (size_t index = 0; index < window_count && success; index++) {
					size_t size = m_windows[valid_stream[index]].Serialize(temp_memory);
					success &= WriteFile(file, { temp_memory, size });
				}

#pragma endregion

#pragma region Dockspace layers

				unsigned short dockspace_layers_count = valid_layers.size;
				success &= WriteFile(file, { &dockspace_layers_count, sizeof(dockspace_layers_count) });
				function::CopyStreamWithMask(file, Stream<void>(stack_dockspace_layers.buffer, sizeof(UIDockspaceLayer)), valid_layers);

#pragma endregion

#pragma region Fixed dockspaces

				Stream<unsigned short> valid_fixed(_valid_horizontals, m_fixed_dockspaces.size);
				function::MakeSequence(valid_fixed);
				for (int64_t index = 0; index < valid_fixed.size; index++) {
					for (size_t subindex = 0; subindex < pop_up_stream.size; subindex++) {
						if (pop_up_stream[subindex].index == m_fixed_dockspaces[valid_fixed[index]].index && 
							pop_up_stream[subindex].type == m_fixed_dockspaces[valid_fixed[index]].type) {
							valid_fixed.Remove(index);
							index--;
							break;
						}
					}
				}

				unsigned short fixed_dockspaces_count = (unsigned short)valid_fixed.size;
				success &= WriteFile(file, { &fixed_dockspaces_count, sizeof(fixed_dockspaces_count) });
				function::CopyStreamWithMask(file, Stream<void>(m_fixed_dockspaces.buffer, sizeof(UIDockspaceLayer)), valid_fixed);

#pragma endregion

#pragma region Background dockspaces

				Stream<unsigned short> valid_background(_valid_horizontals, m_background_dockspaces.size);
				function::MakeSequence(valid_background);
				for (int64_t index = 0; index < valid_background.size; index++) {
					for (size_t subindex = 0; subindex < pop_up_stream.size; subindex++) {
						if (pop_up_stream[subindex].index == m_background_dockspaces[valid_background[index]].index &&
							pop_up_stream[subindex].type == m_background_dockspaces[valid_background[index]].type) {
							valid_background.RemoveSwapBack(index);
							index--;
							break;
						}
					}
				}

				unsigned short background_dockspace_count = (unsigned short)valid_background.size;
				success &= WriteFile(file, { &background_dockspace_count, sizeof(background_dockspace_count) });
				function::CopyStreamWithMask(file, Stream<void>(m_background_dockspaces.buffer, sizeof(UIDockspaceLayer)), valid_background);

				CloseFile(file);
				if (!success) {
					if (error_message.buffer != nullptr) {
						Stream<char> first_error = FIRST_ERROR;
						Stream<char> second_error = SECOND_ERROR;
						error_message.AddStreamSafe(first_error);

						ECS_STACK_CAPACITY_STREAM(char, temp_characters, 512);
						function::ConvertWideCharsToASCII(filename, temp_characters);
						error_message.AddStreamSafe(temp_characters);
						error_message.AddStreamSafe(second_error);
						error_message[error_message.size] = '\0';
					}

					if (ExistsFileOrFolder(filename)) {
						RemoveFile(filename);
					}

					return false;
				}

#pragma endregion

				return true;
			}
			else {
				const char* FIRST_ERROR = "Opening file ";
				const char* SECOND_ERROR = " to write UI windows failed!";

				if (error_message.buffer != nullptr) {
					Stream<char> first_error = FIRST_ERROR;
					Stream<char> second_error = SECOND_ERROR;
					error_message.AddStreamSafe(first_error);

					ECS_STACK_CAPACITY_STREAM(char, temp_characters, 512);
					function::ConvertWideCharsToASCII(filename, temp_characters);
					error_message.AddStreamSafe(temp_characters);
					error_message.AddStreamSafe(second_error);
					error_message[error_message.size] = '\0';
				}

				return false;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		// ---------------------------------------------------- Thread Task ------------------------------------------------------------------

		void DrawDockspaceRegionThreadTask(unsigned int thread_id, World* world, void* data) {
			UIDrawDockspaceRegionData* reinterpretation = (UIDrawDockspaceRegionData*)data;
			reinterpretation->thread_id = thread_id;
			UISystem* system = (UISystem*)reinterpretation->system;

			system->m_resources.thread_resources[thread_id].temp_allocator.Clear();
			if (reinterpretation->active_region_index == reinterpretation->draw_index) {
				system->DrawDockspaceRegionActive(reinterpretation);
			}
			else {
				system->DrawDockspaceRegion(reinterpretation);
			}

#ifdef ECS_TOOLS_UI_MULTI_THREADED
			ThreadTask new_task;
			bool succeded = system->m_thread_tasks.Pop(new_task);
			if (succeded) {
				DrawDockspaceRegionThreadTask(thread_id, new_task.data);
			}
#endif
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void ProcessTexture(unsigned int thread_index, World* world, void* _data) {
			ProcessTextureData* data = (ProcessTextureData*)_data;

			Texture2D old_texture(data->texture->GetResource());

			data->system->m_resources.texture_spinlock.lock();
			Texture2D new_texture = ResizeTextureWithStaging(data->system->m_graphics, old_texture, 256, 256, { true }, ECS_RESIZE_TEXTURE_FILTER_BOX);
			data->system->m_resources.texture_spinlock.unlock();
			if (new_texture.tex != nullptr) {
				uint2 texture_dimensions = GetTextureDimensions(old_texture);

				bool success = true;
				// Only compress the texture if it is multiple of 4 size in both dimensions
				if ((texture_dimensions.x & 3) == 0 && (texture_dimensions.y & 3) == 0) {
					// Compress the texture
					CompressTextureDescriptor compress_descriptor;
					compress_descriptor.spin_lock = &data->system->m_resources.texture_spinlock;
					success = CompressTexture(data->system->m_graphics, new_texture, ECS_TEXTURE_COMPRESSION_COLOR, compress_descriptor);
				}

				if (success) {
					// Create a shader resource view
					ResourceView new_view = data->system->m_graphics->CreateTextureShaderViewResource(new_texture);

					// If it not failed				
					if (new_view.view != nullptr) {
						// Rebind the pointer in the resource manager; the old texture will be automatically deleted
						// and remove the reference count since it will bottleneck the file explorer when many textures are loaded
						// at the same time
						ResourceIdentifier identifier(data->filename);
						data->system->m_resource_manager->RebindResource(identifier, ResourceType::Texture, new_view.view);
						data->system->m_resource_manager->RemoveReferenceCountForResource(identifier, ResourceType::Texture);
						*data->texture = new_view;
					}
				}
				else {
					new_texture.Release();
				}
			}
			data->system->m_resources.texture_semaphore.Exit();
		}

#pragma region Actions

		// ----------------------------------------------------------- Actions ---------------------------------------------------------------

		// -----------------------------------------------------------------------------------------------------------------------------------

		void SkipAction(ActionData* action_data) {}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void PopUpWindowSystemHandler(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIPopUpWindowData* data = (UIPopUpWindowData*)_data;
			
			HID::MouseButtonState state = MBPRESSED;
			if (data->destroy_at_release) {
				state = MBRELEASED;
			}
			if (mouse_tracker->LeftButton() == state) {
				if (data->is_initialized) {
					auto destroy_lambda = [&]() {
						DockspaceType type;
						unsigned int border_index;
						unsigned int window_index = system->GetWindowFromName(data->name);
						if (window_index != -1) {
							UIDockspace* dockspace = system->GetDockspaceFromWindow(window_index, border_index, type);
							system->DestroyDockspace(dockspace->borders.buffer, type);
							//system->RemoveDockspaceBorder(dockspace, border_index, type, true);
						}
						system->RemoveFrameHandler(PopUpWindowSystemHandler, data);
						if (data->flag_destruction != nullptr) {
							*data->flag_destruction = true;
						}
					};

					if (data->reset_when_window_is_destroyed) {
						if (system->GetWindowFromName(data->name) == -1) {
							system->RemoveFrameHandler(PopUpWindowSystemHandler, data);
							if (data->deallocate_name) {
								system->m_memory->Deallocate(data->name.buffer);
							}
							return;
						}
					}
					if (data->destroy_at_first_click) {
						destroy_lambda();
					}
					else {
						unsigned int window_index = system->GetWindowFromName(data->name);
						if (window_index == -1) {
							system->RemoveFrameHandler(PopUpWindowSystemHandler, data);
							if (data->deallocate_name) {
								system->m_memory->Deallocate(data->name.buffer);
							}
							return;
						}
						dockspace = system->GetDockspaceFromWindow(window_index, border_index, dockspace_type);
						float2 region_position = system->GetDockspaceRegionPosition(dockspace, border_index, GetDockspaceMaskFromType(dockspace_type));
						float2 region_scale = system->GetDockspaceRegionScale(dockspace, border_index, GetDockspaceMaskFromType(dockspace_type));
						if (!IsPointInRectangle(mouse_position, region_position, region_scale) && system->m_event != ResizeDockspaceEvent) {
							destroy_lambda();
						}
					}
				}
				else {
					data->is_initialized = true;
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void CloseXAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UICloseXClickData* data = (UICloseXClickData*)_data;
			unsigned int window_index = dockspace->borders[border_index].window_indices[data->window_in_border_index];
			system->DestroyWindow(window_index);
			system->RemoveWindowFromDockspaceRegion(dockspace, dockspace_type, border_index, data->window_in_border_index);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void CloseXBorderClickableAction(ActionData* action_data) {
			DestroyCurrentActionWindow(action_data);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void CloseXBorderHoverableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			float dockspace_mask = GetDockspaceMaskFromType(dockspace_type);
			DefaultHoverableAction(action_data);
			float2 region_position = system->GetDockspaceRegionPosition(dockspace, border_index, dockspace_mask);
			region_position.y += system->m_descriptors.dockspaces.border_size;
			float2 region_scale = system->GetDockspaceRegionScale(dockspace, border_index, dockspace_mask);
			float2 close_x_scale = float2(
				system->m_descriptors.dockspaces.close_x_position_x_left - system->m_descriptors.dockspaces.close_x_position_x_right,
				system->m_descriptors.dockspaces.close_x_scale_y
			);
			
			float2 expanded_close_x_scale = { close_x_scale.x * 1.35f, close_x_scale.y * 1.15f };
			unsigned int x_sprite_offset = system->FindCharacterType('X');

			// close window X; adding it to the text sprite stream
			float2 close_x_position = float2(
				position
			);
			float2 expanded_position = ExpandRectangle(close_x_position, expanded_close_x_scale, close_x_scale);
			SetSpriteRectangle(
				expanded_position,
				close_x_scale,
				system->m_descriptors.color_theme.region_header_x,
				system->m_font_character_uvs[2 * x_sprite_offset],
				system->m_font_character_uvs[2 * x_sprite_offset + 1],
				(UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE],
				counts[ECS_TOOLS_UI_TEXT_SPRITE]
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void DestroyCurrentActionWindow(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;
			system->RemoveDockspaceBorder(dockspace, border_index, dockspace_type);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void CollapseTriangleClickableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			if (mouse_tracker->LeftButton() == MBRELEASED && IsPointInRectangle(mouse_position, position, scale))
				dockspace->borders[border_index].draw_region_header = 1 - dockspace->borders[border_index].draw_region_header;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void DefaultHoverableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			const UIDefaultHoverableData* data = (UIDefaultHoverableData*)_data;
			bool is_pressed_or_held = mouse_tracker->LeftButton() == MBPRESSED || mouse_tracker->LeftButton() == MBHELD;

			if (!data->is_single_action_parameter_draw) {
				for (size_t index = 0; index < data->count; index++) {
					float lighten_percentage = data->percentages[index];
					if (is_pressed_or_held) {
						lighten_percentage *= system->m_descriptors.color_theme.darken_hover_factor;
					}
					Color new_color = ToneColor(data->colors[index], lighten_percentage);

					SetSolidColorRectangle(
						data->positions[index],
						data->scales[index],
						new_color,
						(UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR],
						counts[ECS_TOOLS_UI_SOLID_COLOR]
					);
				}
			}
			else {
				float lighten_percentage = data->percentages[0];
				if (is_pressed_or_held) {
					lighten_percentage *= system->m_descriptors.color_theme.darken_hover_factor;
				}
				Color new_color = ToneColor(data->colors[0], lighten_percentage);

				SetSolidColorRectangle(
					position,
					scale,
					new_color,
					(UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR],
					counts[ECS_TOOLS_UI_SOLID_COLOR]
				);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void DefaultTextHoverableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			const UIDefaultTextHoverableData* data = (UIDefaultTextHoverableData*)_data;
			bool is_pressed_or_held = mouse_tracker->LeftButton() == MBPRESSED || mouse_tracker->LeftButton() == MBHELD;

			float lighten_percentage = data->percentage;
			if (is_pressed_or_held) {
				lighten_percentage *= system->m_descriptors.color_theme.darken_hover_factor;
			}
			Color new_color = ToneColor(data->color, lighten_percentage);

			SetSolidColorRectangle(
				position,
				scale,
				new_color,
				(UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR],
				counts[ECS_TOOLS_UI_SOLID_COLOR]
			);

			if (data->vertices != nullptr) {
				size_t validated_vertices = data->vertex_count;
				Stream<UISpriteVertex> vertices = Stream<UISpriteVertex>(data->vertices, data->vertex_count);
				float2 text_span = GetTextSpan(vertices, !data->vertical_text);

				size_t before_count = counts[ECS_TOOLS_UI_TEXT_SPRITE];
				counts[ECS_TOOLS_UI_TEXT_SPRITE] += data->vertex_count;
				if (data->horizontal_cull) {
					if (data->vertical_text) {
						if (position.x + data->text_offset.x + text_span.x < data->horizontal_cull_bound) {
							counts[ECS_TOOLS_UI_TEXT_SPRITE] -= data->vertex_count;
							validated_vertices = 0;
						}
					}
					else {
						validated_vertices = CullTextSprites<0>(vertices, data->horizontal_cull_bound);
						counts[ECS_TOOLS_UI_TEXT_SPRITE] -= data->vertex_count - validated_vertices;
					}
				}
				if (data->vertical_cull) {
					if (data->vertical_text) {
						if (validated_vertices > 0) {
							validated_vertices = CullTextSprites<3>(vertices, data->vertical_cull_bound);
							counts[ECS_TOOLS_UI_TEXT_SPRITE] -= data->vertex_count - validated_vertices;
						}
					}
					else {
						if (validated_vertices > 0 && position.y + data->text_offset.y + text_span.y > data->vertical_cull_bound) {
							counts[ECS_TOOLS_UI_TEXT_SPRITE] -= before_count;
							validated_vertices = 0;
						}
					}
				}
				vertices.buffer = (UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE] + before_count;
				vertices.size = validated_vertices;
				memcpy(vertices.buffer, data->vertices, sizeof(UISpriteVertex) * validated_vertices);	

				TranslateText(position.x + data->text_offset.x, position.y + data->text_offset.y, vertices, 0, 0);
			}
			else {
				size_t before_count = counts[ECS_TOOLS_UI_TEXT_SPRITE];
				system->ConvertCharactersToTextSprites(
					data->text,
					{ position.x + data->text_offset.x, position.y + data->text_offset.y }, 
					(UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE], 
					data->text_color, 
					counts[ECS_TOOLS_UI_TEXT_SPRITE],
					data->font_size, 
					data->character_spacing,
					!data->vertical_text
				);
				counts[ECS_TOOLS_UI_TEXT_SPRITE] += data->text.size * 6;
				if (data->horizontal_cull || data->vertical_cull) {
					Stream<UISpriteVertex> vertices = Stream<UISpriteVertex>((UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE] + before_count, data->text.size * 6);
					float2 text_span;
					if (data->vertical_text) {
						text_span = GetTextSpan(vertices, false);
						if (data->horizontal_cull) {
							if (position.x + data->text_offset.x + text_span.x > data->horizontal_cull_bound) {
								counts[ECS_TOOLS_UI_TEXT_SPRITE] -= data->text.size * 6;
							}
						}
						if (data->vertical_cull) {
							size_t validated_vertices = CullTextSprites<3>(vertices, data->vertical_cull_bound);
							counts[ECS_TOOLS_UI_TEXT_SPRITE] -= data->text.size * 6 - validated_vertices;
						}
					}
					else {
						text_span = GetTextSpan(vertices);
						if (data->horizontal_cull) {
							size_t validated_vertices = CullTextSprites<0>(vertices, data->horizontal_cull_bound);
							counts[ECS_TOOLS_UI_TEXT_SPRITE] -= data->text.size * 6 - validated_vertices;
						}
					}

				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void DefaultVertexColorHoverableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			const UIDefaultVertexColorHoverableData* data = (UIDefaultVertexColorHoverableData*)_data;
			bool is_pressed_or_held = mouse_tracker->LeftButton() == MBPRESSED || mouse_tracker->LeftButton() == MBHELD;

			float lighten_percentage = data->percentage;
			if (is_pressed_or_held) {
				lighten_percentage *= system->m_descriptors.color_theme.darken_hover_factor;
			}

			Color new_top_left = ToneColor(data->top_left, lighten_percentage);
			Color new_top_right = ToneColor(data->top_right, lighten_percentage);
			Color new_bottom_left = ToneColor(data->bottom_left, lighten_percentage);
			Color new_bottom_right = ToneColor(data->bottom_right, lighten_percentage);
			
			SetVertexColorRectangle(
				position,
				scale,
				new_top_left,
				new_top_right,
				new_bottom_left,
				new_bottom_right,
				(UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR],
				&counts[ECS_TOOLS_UI_SOLID_COLOR]
			);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void DefaultClickableAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDefaultClickableData* data = (UIDefaultClickableData*)_data;

			HID::MouseButtonState left_state = mouse_tracker->LeftButton();
			if (left_state == MBPRESSED || left_state == MBHELD) {
				// The data must be inferred
				void* handler_data = data->hoverable_handler.data_size == 0 ? data->hoverable_handler.data : function::OffsetPointer(data, sizeof(*data));

				action_data->data = handler_data;
				data->hoverable_handler.action(action_data);
			}
			else if (left_state == MBRELEASED) {
				if (IsPointInRectangle(
					mouse_position,
					position,
					scale
				)) {
					// The handler data must be inferred
					void* handler_data = data->click_handler.data_size == 0 ? data->click_handler.data :
						function::OffsetPointer(data, sizeof(*data) + data->hoverable_handler.data_size);

					// If the clickable handler is system phase but the hoverable is in another phase, 
					// This will be placed for the hoverable's phase. Change the focused clickable handler phase
					// to trick the system into recalling this clickable - need to override the general handler into doing that
					// because the clickable handler will be deallocated afterwards
					if (data->initial_clickable_phase != system->m_focused_window_data.clickable_handler.phase) {
						UIActionHandler recall_handler = data->click_handler;
						recall_handler.data = handler_data;
						recall_handler.data = function::CopyNonZero(system->m_memory, recall_handler.data, recall_handler.data_size);

						if (system->m_focused_window_data.general_handler.action == nullptr) {
							auto recall_general = [](ActionData* action_data) {
								UI_UNPACK_ACTION_DATA;

								UIActionHandler* handler = (UIActionHandler*)_data;
								//handler->phase = ECS_UI_DRAW_SYSTEM;

								system->m_focused_window_data.ChangeClickableHandler(position, scale, handler);

								// Deallocate the memory
								system->m_memory->Deallocate(handler);

								// Change back to the empty handler
								system->m_focused_window_data.ResetGeneralHandler();
							};
							// Need to allocate the data by ourselves
							void* _recall_handler = system->m_memory->Allocate(sizeof(UIActionHandler));
							memcpy(_recall_handler, &recall_handler, sizeof(recall_handler));

							UIActionHandler current_handler = { recall_general, _recall_handler, sizeof(recall_handler), system->m_focused_window_data.clickable_handler.phase };
							system->m_focused_window_data.ChangeGeneralHandler(position, scale, &current_handler);
						}
						else {
							struct GeneralWrapperData {
								float2 clickable_position;
								float2 clickable_scale;
								UIActionHandler general_handler;
								UIActionHandler clickable_handler;
							};

							// A wrapper for the general handler
							auto general_wrapper = [](ActionData* action_data) {
								UI_UNPACK_ACTION_DATA;

								GeneralWrapperData* data = (GeneralWrapperData*)_data;

								//data->clickable_handler.phase = ECS_UI_DRAW_SYSTEM;

								// Change the clickable
								system->m_focused_window_data.ChangeClickableHandler(data->clickable_position, data->clickable_scale, &data->clickable_handler);

								// Change the general
								system->m_focused_window_data.ChangeGeneralHandler(position, scale, &data->general_handler);

								// Call it if they are the same phase
								if (data->general_handler.phase == data->clickable_handler.phase) {
									action_data->data = data->general_handler.data;
									data->general_handler.action(action_data);
								}

								// Deallocate the data - only this one - the other ones will get deallocated
								// by their handlers
								system->m_memory->Deallocate(data);
							};

							// Allocate the clickable data as well because the data will be deallocated and if another allocation
							// is done it can overwrite that data
							GeneralWrapperData* allocation = (GeneralWrapperData*)system->m_memory->Allocate(sizeof(GeneralWrapperData));
							
							allocation->clickable_handler = recall_handler;
							allocation->general_handler = system->m_focused_window_data.general_handler;
							allocation->clickable_position = system->m_focused_window_data.clickable_transform.position;
							allocation->clickable_scale = system->m_focused_window_data.clickable_transform.scale;

							UIActionHandler current_handler = { general_wrapper, allocation, sizeof(*allocation), system->m_focused_window_data.clickable_handler.phase };
							system->m_focused_window_data.ChangeGeneralHandler(
								system->m_focused_window_data.general_transform.position,
								system->m_focused_window_data.general_transform.scale,
								&current_handler
							);
						}
					}
					else {
						action_data->data = handler_data;
						data->click_handler.action(action_data);
					}
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void DoubleClickAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDoubleClickData* data = (UIDoubleClickData*)_data;
			UIDoubleClickData* additional_data = (UIDoubleClickData*)_additional_data;
			system->SetCleanupGeneralHandler();

			if (UI_ACTION_IS_NOT_CLEAN_UP_CALL) {
				if (mouse_tracker->LeftButton() == MBPRESSED) {
					if (UI_ACTION_IS_THE_SAME_AS_PREVIOUS) {
						size_t duration = additional_data->timer.GetDurationSinceMarker_ms();
						if (duration < data->max_duration_between_clicks) {
							// The data must be inferred
							void* double_click_data = data->double_click_handler.data_size == 0 ? data->double_click_handler.data :
								function::OffsetPointer(data, sizeof(*data) + data->first_click_handler.data_size);

							action_data->data = double_click_data;
							data->double_click_handler.action(action_data);
							return;
						}
					}
					data->timer.SetMarker();

					// The data must be inferred
					void* first_click_data = data->first_click_handler.data_size == 0 ? data->first_click_handler.data : function::OffsetPointer(data, sizeof(*data));
					action_data->data = first_click_data;
					data->first_click_handler.action(action_data);
				}
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void DragDockspaceAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDragDockspaceData* data = (UIDragDockspaceData*)_data;

			unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);

			if (data->floating_dockspace != nullptr && !system->IsFixedDockspace(data->floating_dockspace) && dockspace->allow_docking) {
				DockspaceType types[32];
				UIDockspace* dockspaces[32];
				unsigned int border_indices[32];
				unsigned int regions_count;
				system->GetDockspaceRegionsFromMouse(mouse_position, dockspaces, border_indices, types, regions_count);
				unsigned int normal_region_count = regions_count;
				system->GetFixedDockspaceRegionsFromMouse(mouse_position, dockspaces, types, regions_count);
				for (size_t index = normal_region_count; index < regions_count; index++) {
					border_indices[index] = 0;
				}
				const float masks[] = { 1.0f, 0.0f, 1.0f, 0.0f };
				for (size_t index = 0; index < regions_count; index++) {
					if (dockspaces[index] != dockspace) {
						UIDockspace* hovered_dockspace = dockspaces[index];
						float2 hovered_region_position;
						float2 hovered_region_scale;
						if (index < normal_region_count) {
							hovered_region_position = system->GetDockspaceRegionPosition(dockspaces[index], border_indices[index], masks[(unsigned int)types[index]]);
							hovered_region_scale = system->GetDockspaceRegionScale(dockspaces[index], border_indices[index], masks[(unsigned int)types[index]]);
						}
						else {
							hovered_region_position = dockspaces[index]->transform.position;
							hovered_region_scale = dockspaces[index]->transform.scale;
						}

						// if it is not an empty fixed dockspace
						if (dockspaces[index]->borders[0].window_indices.size > 0) {
							unsigned int hovered_window_index = system->GetWindowIndexFromBorder(dockspaces[index], 0);
							if (!dockspaces[index]->allow_docking) {
								hovered_region_scale = { 0.0f, 0.0f };
							}
						}

						float2 region_minimum_scale = ECS_TOOLS_UI_DOCKING_GIZMO_MINIMUM_SCALE;
						if (hovered_region_scale.x > region_minimum_scale.x
							&& hovered_region_scale.y > region_minimum_scale.y) {
							float2 rectangle_transforms[10];
							bool is_single_windowed = dockspace->borders.size == 2 && !dockspace->borders[0].is_dock;
							if (is_single_windowed) {
								system->DrawDockingGizmo(
									{ hovered_region_position.x + hovered_region_scale.x * 0.5f, hovered_region_position.y + hovered_region_scale.y * 0.5f },
									counts,
									buffers,
									true,
									rectangle_transforms
								);
							}
							else {
								system->DrawDockingGizmo(
									{ hovered_region_position.x + hovered_region_scale.x * 0.5f, hovered_region_position.y + hovered_region_scale.y * 0.5f },
									counts,
									buffers,
									false,
									rectangle_transforms
								);
							}

							// left rectangle
							if (IsPointInRectangle(
								mouse_position,
								rectangle_transforms[0],
								rectangle_transforms[1]
							)) {
								if (mouse_tracker->LeftButton() == MBHELD || mouse_tracker->LeftButton() == MBPRESSED) {
									system->HandleDockingGizmoTransparentHover(
										hovered_dockspace,
										border_indices[index],
										hovered_region_position,
										{ hovered_region_scale.x * 0.5f, hovered_region_scale.y },
										buffers,
										counts
									);
								}
								else if (mouse_tracker->LeftButton() == MBRELEASED) {
									system->HandleDockingGizmoAdditionOfDockspace(
										dockspaces[index],
										border_indices[index],
										types[index],
										0,
										data->floating_dockspace,
										data->type
									);
								}
							}

							// top rectangle
							else if (IsPointInRectangle(
								mouse_position,
								rectangle_transforms[2],
								rectangle_transforms[3]
							)) {
								if (mouse_tracker->LeftButton() == MBHELD || mouse_tracker->LeftButton() == MBPRESSED) {
									system->HandleDockingGizmoTransparentHover(
										hovered_dockspace,
										border_indices[index],
										hovered_region_position,
										{ hovered_region_scale.x, hovered_region_scale.y * 0.5f },
										buffers,
										counts
									);
								}
								else if (mouse_tracker->LeftButton() == MBRELEASED) {
									system->HandleDockingGizmoAdditionOfDockspace(
										dockspaces[index],
										border_indices[index],
										types[index],
										1,
										data->floating_dockspace,
										data->type
									);
								}
							}

							// right rectangle 
							else if (IsPointInRectangle(
								mouse_position,
								rectangle_transforms[4],
								rectangle_transforms[5]
							)) {
								if (mouse_tracker->LeftButton() == MBHELD || mouse_tracker->LeftButton() == MBPRESSED) {
									system->HandleDockingGizmoTransparentHover(
										hovered_dockspace,
										border_indices[index],
										{ hovered_region_position.x + hovered_region_scale.x * 0.5f, hovered_region_position.y },
										{ hovered_region_scale.x * 0.5f, hovered_region_scale.y },
										buffers,
										counts
									);
								}
								else if (mouse_tracker->LeftButton() == MBRELEASED) {
									system->HandleDockingGizmoAdditionOfDockspace(
										dockspaces[index],
										border_indices[index],
										types[index],
										2,
										data->floating_dockspace,
										data->type
									);
								}
							}

							// bottom rectangle
							else if (IsPointInRectangle(
								mouse_position,
								rectangle_transforms[6],
								rectangle_transforms[7]
							)) {
								if (mouse_tracker->LeftButton() == MBHELD || mouse_tracker->LeftButton() == MBPRESSED) {
									system->HandleDockingGizmoTransparentHover(
										hovered_dockspace,
										border_indices[index],
										{ hovered_region_position.x, hovered_region_position.y + hovered_region_scale.y * 0.5f },
										{ hovered_region_scale.x, hovered_region_scale.y * 0.5f },
										buffers,
										counts
									);
								}
								else if (mouse_tracker->LeftButton() == MBRELEASED) {
									system->HandleDockingGizmoAdditionOfDockspace(
										dockspaces[index],
										border_indices[index],
										types[index],
										3,
										data->floating_dockspace,
										data->type
									);
								}
							}

							// central rectangle
							else if (is_single_windowed && IsPointInRectangle(
								mouse_position,
								rectangle_transforms[8],
								rectangle_transforms[9]
							)) {
								if (mouse_tracker->LeftButton() == MBHELD || mouse_tracker->LeftButton() == MBPRESSED) {
									system->HandleDockingGizmoTransparentHover(
										hovered_dockspace,
										border_indices[index],
										{ hovered_region_position.x, hovered_region_position.y + system->m_descriptors.misc.title_y_scale },
										{ hovered_region_scale.x, hovered_region_scale.y - system->m_descriptors.misc.title_y_scale },
										buffers,
										counts
									);
								}
								else if (mouse_tracker->LeftButton() == MBRELEASED) {
									system->HandleDockingGizmoAdditionOfDockspace(
										dockspaces[index],
										border_indices[index],
										types[index],
										4,
										data->floating_dockspace,
										data->type
									);
								}
							}
						}
						break;
					}
				}
			}

			bool is_null = data->floating_dockspace == nullptr;
			UIDockspace* _dockspace = data->floating_dockspace;
			if (is_null) {
				float dockspace_mask = GetDockspaceMaskFromType(dockspace_type);
				data->floating_dockspace = system->GetFloatingDockspaceFromDockspace(dockspace, dockspace_mask, data->type);
				if (data->type != DockspaceType::FloatingHorizontal && data->type != DockspaceType::FloatingVertical) {
					data->type = dockspace_type;
				}
			}
			bool is_fixed = system->IsFixedDockspace(data->floating_dockspace->borders.buffer);
			if (!is_fixed) {
				data->floating_dockspace->transform.position.x += mouse_delta.x;
				data->floating_dockspace->transform.position.y += mouse_delta.y;
				system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void RegionHeaderAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			float dockspace_mask = GetDockspaceMaskFromType(dockspace_type);
			UIRegionHeaderData* data = (UIRegionHeaderData*)_data;
			dockspace->borders[border_index].active_window = data->window_index;

			if (mouse_tracker->LeftButton() == MBPRESSED) {
				float2 sizes[32];
				system->CalculateDockspaceRegionHeaders(
					dockspace,
					border_index,
					dockspace_mask,
					data->initial_window_positions
				);

				float2 dockspace_position = system->GetDockspaceRegionPosition(dockspace, border_index, dockspace_mask);
				for (size_t index = 0; index < dockspace->borders[border_index].window_indices.size; index++) {
					data->initial_window_positions[index].x += dockspace_position.x;
				}
			}

			if (data->dockspace_to_drag == nullptr) {
				if (mouse_position.y < position.y || mouse_position.y > position.y + scale.y) {
					UIDockspace* dockspace_to_drag = dockspace;
					DockspaceType floating_type = DockspaceType::FloatingHorizontal;
					bool set_new_active_region_data = false;
					system->m_windows[data->window_index].transform.scale = system->GetDockspaceRegionScale(dockspace, border_index, dockspace_mask);
					
					if (dockspace->borders[border_index].window_indices.size > 1) {
						if (system->m_floating_horizontal_dockspaces.size < system->m_floating_horizontal_dockspaces.capacity) {
							system->CreateDockspace(
								{ {mouse_position.x - 0.03f, mouse_position.y - 0.03f}, system->m_windows[data->window_index].transform.scale },
								DockspaceType::FloatingHorizontal,
								dockspace->borders[border_index].window_indices[data->window_index],
								false
							);
							dockspace_to_drag = &system->m_floating_horizontal_dockspaces[system->m_floating_horizontal_dockspaces.size - 1];
						}
						else {
							system->CreateDockspace(
								{ {mouse_position.x - 0.03f, mouse_position.y - 0.03f}, system->m_windows[data->window_index].transform.scale },
								DockspaceType::FloatingVertical,
								dockspace->borders[border_index].window_indices[data->window_index],
								false
							);
							dockspace_to_drag = &system->m_floating_vertical_dockspaces[system->m_floating_vertical_dockspaces.size - 1];
							floating_type = DockspaceType::FloatingVertical;
						}

						system->RemoveWindowFromDockspaceRegion(dockspace, dockspace_type, border_index, data->window_index);
						set_new_active_region_data = true;
					}
					else if (dockspace_type == DockspaceType::Horizontal || dockspace_type == DockspaceType::Vertical || (dockspace->borders.size > 2)){
						if (system->m_floating_horizontal_dockspaces.size < system->m_floating_horizontal_dockspaces.capacity) {
							system->CreateDockspace(
								{ {mouse_position.x - 0.03f, mouse_position.y - 0.03f}, system->m_windows[data->window_index].transform.scale },
								DockspaceType::FloatingHorizontal,
								dockspace->borders[border_index].window_indices[data->window_index],
								false
							);
							dockspace_to_drag = &system->m_floating_horizontal_dockspaces[system->m_floating_horizontal_dockspaces.size - 1];
						}
						else {
							system->CreateDockspace(
								{ {mouse_position.x - 0.03f, mouse_position.y - 0.03f}, system->m_windows[data->window_index].transform.scale },
								DockspaceType::FloatingVertical,
								dockspace->borders[border_index].window_indices[data->window_index],
								false
							);
							dockspace_to_drag = &system->m_floating_vertical_dockspaces[system->m_floating_vertical_dockspaces.size - 1];
							floating_type = DockspaceType::FloatingVertical;
						}

						system->RemoveDockspaceBorder<false>(dockspace, border_index, dockspace_type);
						set_new_active_region_data = true;
					}
					if (set_new_active_region_data)
						system->SetNewFocusedDockspaceRegion(dockspace_to_drag, 0, floating_type);

					const float masks[4] = { 1.0f, 0.0f, 1.0f, 0.0f };
					data->dockspace_to_drag = dockspace_to_drag;
					data->type = floating_type;
					action_data->data = &data->drag_data;
					action_data->dockspace = dockspace_to_drag;
					action_data->border_index = 0;
					action_data->type = floating_type;
					DragDockspaceAction(action_data);

					if (set_new_active_region_data) {
						data->window_index = 0;
						system->RegisterFocusedWindowClickableAction(position, scale, RegionHeaderAction, data, sizeof(UIRegionHeaderData), ECS_UI_DRAW_SYSTEM);
					}
				}
				else {
					action_data->data = &data->hoverable_data;
					DefaultHoverableAction(action_data);
					float2 sizes[32];

					bool has_moved = false;
					for (size_t index = 0; index < dockspace->borders[border_index].window_indices.size; index++) {
						if (mouse_position.x >= data->initial_window_positions[index].x &&
							mouse_position.x <= data->initial_window_positions[index].x + data->initial_window_positions[index].y) {
							dockspace->borders[border_index].window_indices.Swap(index, data->window_index);
							data->window_index = index;
							dockspace->borders[border_index].active_window = data->window_index;

							float2 region_position = system->GetDockspaceRegionPosition(dockspace, border_index, dockspace_mask);
							float2 region_scale = system->GetDockspaceRegionScale(dockspace, border_index, dockspace_mask);
							system->DrawDockspaceRegionHeader(0, dockspace, border_index, dockspace_mask, buffers, counts);
							system->DrawDockspaceRegionBorders(region_position, region_scale, buffers, counts);
							system->DrawCollapseTriangleHeader(buffers, counts, dockspace, border_index, dockspace_mask, dockspace->borders[border_index].draw_region_header, ECS_UI_DRAW_PHASE::ECS_UI_DRAW_SYSTEM);
							has_moved = true;
							break;
						}
					}

					if (!has_moved) {
						system->CalculateDockspaceRegionHeaders(
							dockspace,
							border_index,
							dockspace_mask,
							sizes
						);
						system->CullRegionHeader(
							dockspace,
							border_index,
							data->window_index,
							dockspace_mask,
							position.x,
							sizes,
							buffers,
							counts
						);
						float2 close_x_scale = float2(
							system->m_descriptors.dockspaces.close_x_position_x_left - system->m_descriptors.dockspaces.close_x_position_x_right,
							system->m_descriptors.dockspaces.close_x_scale_y
						);

						unsigned int x_sprite_offset = system->FindCharacterType('X');
						SetSpriteRectangle(
							float2(
								position.x + scale.x - system->m_descriptors.dockspaces.close_x_position_x_left,
								AlignMiddle(position.y, system->m_descriptors.misc.title_y_scale - system->m_descriptors.dockspaces.border_size, system->m_descriptors.dockspaces.close_x_scale_y)
							),
							close_x_scale,
							system->m_descriptors.color_theme.region_header_x,
							system->m_font_character_uvs[2 * x_sprite_offset],
							system->m_font_character_uvs[2 * x_sprite_offset + 1],
							(UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE],
							counts[ECS_TOOLS_UI_TEXT_SPRITE]
						);
					}
				}
			}
			else {
				action_data->data = &data->drag_data;
				action_data->dockspace = data->dockspace_to_drag;
				action_data->border_index = 0;
				action_data->type = dockspace_type;
				DragDockspaceAction(action_data);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void TextTooltipHoverable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UITextTooltipHoverableData* data = (UITextTooltipHoverableData*)_data;
			system->m_focused_window_data.always_hoverable = true;
			system->DrawToolTipSentence(action_data, data->characters, &data->base);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void TooltipHoverable(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UITooltipHoverableData* data = (UITooltipHoverableData*)_data;
			system->m_focused_window_data.always_hoverable = true;
			position += data->base.offset;
			position.x += data->base.offset_scale.x ? scale.x : 0.0f;
			position.y += data->base.offset_scale.y ? scale.y : 0.0f;

			system->ConfigureToolTipBase(&data->base);

			position.x = position.x < -0.99f ? -0.99f : position.x;
			position.y = position.y < -0.99f ? -0.99f : position.y;

			size_t initial_counts[ECS_TOOLS_UI_MATERIALS];
			for (size_t index = 0; index < ECS_TOOLS_UI_MATERIALS; index++) {
				initial_counts[index] = counts[index];
			}

			UITooltipDrawData draw_data;
			draw_data.buffers = buffers;
			draw_data.counts = counts;
			draw_data.character_spacing = data->base.character_spacing;
			draw_data.font_color = data->base.font_color;
			draw_data.font_size = data->base.font_size;
			draw_data.initial_position = position;
			draw_data.position = { position.x + system->m_descriptors.misc.tool_tip_padding.x, position.y + system->m_descriptors.misc.tool_tip_padding.y };
			draw_data.max_bounds = position;
			draw_data.current_scale = { 0.0f, 0.0f };
			draw_data.indentation = system->m_descriptors.window_layout.element_indentation;
			draw_data.x_padding = system->m_descriptors.misc.tool_tip_padding.x;

			data->drawer(data->data, draw_data);

			draw_data.max_bounds.y += system->m_descriptors.misc.tool_tip_padding.y;
			draw_data.max_bounds.x += system->m_descriptors.misc.tool_tip_padding.x;

			float2 translation = { 0.0f, 0.0f };
			translation.x = draw_data.max_bounds.x > 0.99f ? draw_data.max_bounds.x - 0.99f : 0.0f;
			translation.y = draw_data.max_bounds.y > 0.99f ? draw_data.max_bounds.y - 0.99f : 0.0f;

			if (translation.x != 0.0f || translation.y != 0.0f) {
				for (size_t material = 0; material < ECS_TOOLS_UI_MATERIALS; material++) {
					if (material == 0) {
						UIVertexColor* vertices = (UIVertexColor*)buffers[material];
						for (size_t index = initial_counts[material]; index < counts[material]; index++) {
							vertices[index].position.x -= translation.x;
							vertices[index].position.y += translation.y;
						}
					}
					else if (material == 1 || material == 2) {
						UISpriteVertex* vertices = (UISpriteVertex*)buffers[material];
						for (size_t index = initial_counts[material]; index < counts[material]; index++) {
							vertices[index].position.x -= translation.x;
							vertices[index].position.y += translation.y;
						}
					}
				}
			}
			draw_data.initial_position -= translation;
			draw_data.max_bounds -= translation;

			SetSolidColorRectangle(draw_data.initial_position, draw_data.max_bounds - draw_data.initial_position, data->base.background_color, buffers, counts, 0);
			CreateSolidColorRectangleBorder<false>(draw_data.initial_position, draw_data.max_bounds - draw_data.initial_position, data->base.border_size, data->base.border_color, counts, buffers);

			if (data->data_size != 0 && data->data != nullptr) {
				system->m_memory->Deallocate(data->data);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void TextTooltipAlignTextToRight(Stream<char>& current_text, const char* new_text, size_t total_characters)
		{
			size_t next_text_length = strlen(new_text);
			ECS_ASSERT(next_text_length + current_text.size < total_characters);

			for (size_t index = current_text.size; index < total_characters - next_text_length; index++) {
				current_text[index] = ' ';
			}
			memcpy(current_text.buffer + total_characters - next_text_length, new_text, next_text_length * sizeof(char));
			current_text.size = total_characters;
			current_text[current_text.size] = '\0';
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void ReleaseLockedWindow(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			system->m_focused_window_data.locked_window--;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void ConvertASCIIToWideAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIConvertASCIIToWideData* data = (UIConvertASCIIToWideData*)_data;
			size_t size = strlen(data->ascii);
			function::ConvertASCIIToWide(data->wide, data->ascii, size);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void ConvertASCIIToWideStreamAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIConvertASCIIToWideStreamData* data = (UIConvertASCIIToWideStreamData*)_data;
			size_t size = strlen(data->ascii);
			ECS_ASSERT(data->wide->capacity >= size);

			function::ConvertASCIIToWide(data->wide->buffer, data->ascii, size);
			data->wide->size = size;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void DefaultHoverableWithToolTipAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDefaultHoverableWithTooltipData* data = (UIDefaultHoverableWithTooltipData*)_data;
			UIDefaultHoverableData hoverable_data;
			hoverable_data.colors[0] = data->color;
			hoverable_data.percentages[0] = data->percentage;

			action_data->data = &hoverable_data;
			DefaultHoverableAction(action_data);

			action_data->data = &data->tool_tip_data;
			TextTooltipHoverable(action_data);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void DestroyWindowSystemHandler(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			unsigned int* window_index = (unsigned int*)_data;
			system->RemoveWindowFromDockspaceRegion(*window_index);
			system->RemoveFrameHandler(DestroyWindowSystemHandler, _data);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void DestroyWindowCallbackSystemHandler(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			DestroyWindowCallbackSystemHandlerData* data = (DestroyWindowCallbackSystemHandlerData*)_data;
			action_data->data = data->callback.data;
			data->callback.action(action_data);

			system->RemoveWindowFromDockspaceRegion(data->window_index);
			system->PopFrameHandler();
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Events

		// ---------------------------------------------------------- Events -----------------------------------------------------------------

		// -----------------------------------------------------------------------------------------------------------------------------------

		void MoveDockspaceBorderEvent(
			UISystem* system,
			void* parameter,
			void** buffers,
			size_t* counts,
			float normalized_mouse_x,
			float normalized_mouse_y,
			const HID::MouseState* mouse,
			const HID::MouseTracker* mouse_tracker,
			const HID::KeyboardState* keyboard,
			const HID::KeyboardTracker* keyboard_tracker
		) {
			UIMoveDockspaceBorderEventData* data = (UIMoveDockspaceBorderEventData*)parameter;
			float2 mouse_delta = system->GetMouseDelta({ normalized_mouse_x, normalized_mouse_y });
			float delta_x = mouse_delta.x * data->move_x;
			float delta_y = mouse_delta.y * data->move_y;

			if (system->m_focused_window_data.locked_window == 0 || (system->m_focused_window_data.active_location.dockspace == data->dockspace &&
				system->m_focused_window_data.active_location.border_index == data->border_index)) {
				system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
				
				if (mouse_tracker->LeftButton() == MBPRESSED) {
					if (system->m_focused_window_data.general_handler.action != nullptr) {
						system->m_memory->Deallocate(system->m_focused_window_data.general_handler.data);
					}
					system->HandleFocusedWindowCleanupGeneral({ normalized_mouse_x, normalized_mouse_y }, 0);
					system->m_focused_window_data.ChangeGeneralHandler({ {0.0f, 0.0f}, {0.0f, 0.0f} }, nullptr, nullptr, 0, ECS_UI_DRAW_NORMAL);
				}

				if (mouse_tracker->LeftButton() == MBHELD || mouse_tracker->LeftButton() == MBPRESSED) {
					DockspaceType floating_type = DockspaceType::Horizontal;
					UIDockspace* floating_dockspace = system->GetFloatingDockspaceFromDockspace(data->dockspace, data->move_x, floating_type);
					if (floating_type == DockspaceType::Horizontal) {
						floating_type = data->type;
					}
					system->SetNewFocusedDockspace(floating_dockspace, floating_type);
					system->SearchAndSetNewFocusedDockspaceRegion(data->dockspace, data->border_index, data->type);
					UIVertexColor* solid_color = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];

					float2 position = system->GetInnerDockspaceBorderPosition(data->dockspace, data->border_index, data->type);
					float2 scale = system->GetInnerDockspaceBorderScale(data->dockspace, data->border_index, data->type);
					SetSolidColorRectangle(
						position,
						scale,
						DarkenColor(system->m_descriptors.color_theme.hovered_borders, system->m_descriptors.color_theme.darken_hover_factor),
						solid_color,
						counts[ECS_TOOLS_UI_SOLID_COLOR]
					);
					system->MoveDockspaceBorder(data->dockspace->borders.buffer, data->border_index, delta_x, delta_y);
				}
				else {
					system->DeallocateEventData();
					system->m_event = SkipEvent;
					UIVertexColor* solid_color = nullptr;

					solid_color = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];

					float2 position = system->GetInnerDockspaceBorderPosition(data->dockspace, data->border_index, data->type);
					float2 scale = system->GetInnerDockspaceBorderScale(data->dockspace, data->border_index, data->type);
					SetSolidColorRectangle(
						position,
						scale,
						DarkenColor(system->m_descriptors.color_theme.hovered_borders, system->m_descriptors.color_theme.darken_hover_factor),
						solid_color,
						counts[ECS_TOOLS_UI_SOLID_COLOR]
					);
				}

				if (data->type == DockspaceType::FloatingHorizontal || data->type == DockspaceType::Horizontal) {
					system->m_application->ChangeCursor(ECS_CURSOR_SIZE_EW);
				}
				else {
					system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NS);
				}
			}

		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void ResizeDockspaceEvent(
			UISystem* system,
			void* parameter,
			void** buffers,
			size_t* counts,
			float normalized_mouse_x,
			float normalized_mouse_y,
			const HID::MouseState* mouse,
			const HID::MouseTracker* mouse_tracker,
			const HID::KeyboardState* keyboard,
			const HID::KeyboardTracker* keyboard_tracker
		) {
			UIDockspace* dockspaces[4] = {
				system->m_horizontal_dockspaces.buffer,
				system->m_vertical_dockspaces.buffer,
				system->m_floating_horizontal_dockspaces.buffer,
				system->m_floating_vertical_dockspaces.buffer
			};

			UIResizeEventData* data = (UIResizeEventData*)parameter;
			UIDockspace* dockspace = &dockspaces[(unsigned int)data->dockspace_type][data->dockspace_index];

			if (system->m_focused_window_data.locked_window == 0 || (system->m_focused_window_data.active_location.dockspace == dockspace)) {
				system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
				
				if (mouse_tracker->LeftButton() == MBPRESSED) {
					// cleaning up the last action; signaling clean up call by marking the buffers and the counts as nullptr
					if (system->m_focused_window_data.general_handler.action != nullptr) {
						system->HandleFocusedWindowCleanupGeneral({ normalized_mouse_x, normalized_mouse_y }, 0, nullptr);

						system->m_focused_window_data.clean_up_call_general = false;
						if (system->m_focused_window_data.general_handler.action != nullptr && system->m_focused_window_data.general_handler.data_size != 0) {
							system->m_memory->Deallocate(system->m_focused_window_data.general_handler.data);
						}
						system->m_focused_window_data.ChangeGeneralHandler({ {0.0f, 0.0f}, {0.0f, 0.0f} }, nullptr, nullptr, 0, ECS_UI_DRAW_NORMAL);
					}

				}

				auto draw_color_and_resize = [=]() {
					float2 mouse_delta = system->GetMouseDelta({ normalized_mouse_x, normalized_mouse_y });
					float delta_x = mouse_delta.x;
					float delta_y = mouse_delta.y;

					UIVertexColor* solid_color = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
					if (data->border_hover.IsTop()) {
						float2 border_position = system->GetOuterDockspaceBorderPosition(dockspace, ECS_UI_BORDER_TOP);
						float2 border_scale = system->GetOuterDockspaceBorderScale(dockspace, ECS_UI_BORDER_TOP);
						SetSolidColorRectangle(
							border_position,
							border_scale,
							DarkenColor(system->m_descriptors.color_theme.hovered_borders, system->m_descriptors.color_theme.darken_hover_factor),
							solid_color,
							counts[ECS_TOOLS_UI_SOLID_COLOR]
						);
						system->ResizeDockspace(data->dockspace_index, delta_y, ECS_UI_BORDER_TOP, data->dockspace_type);
					}
					else if (data->border_hover.IsBottom()) {
						float2 border_position = system->GetOuterDockspaceBorderPosition(dockspace, ECS_UI_BORDER_BOTTOM);
						float2 border_scale = system->GetOuterDockspaceBorderScale(dockspace, ECS_UI_BORDER_BOTTOM);
						SetSolidColorRectangle(
							border_position,
							border_scale,
							DarkenColor(system->m_descriptors.color_theme.hovered_borders, system->m_descriptors.color_theme.darken_hover_factor),
							solid_color,
							counts[ECS_TOOLS_UI_SOLID_COLOR]
						);
						system->ResizeDockspace(data->dockspace_index, delta_y, ECS_UI_BORDER_BOTTOM, data->dockspace_type);
					}
					if (data->border_hover.IsRight()) {
						float2 border_position = system->GetOuterDockspaceBorderPosition(dockspace, ECS_UI_BORDER_RIGHT);
						float2 border_scale = system->GetOuterDockspaceBorderScale(dockspace, ECS_UI_BORDER_RIGHT);
						SetSolidColorRectangle(
							border_position,
							border_scale,
							DarkenColor(system->m_descriptors.color_theme.hovered_borders, system->m_descriptors.color_theme.darken_hover_factor),
							solid_color,
							counts[ECS_TOOLS_UI_SOLID_COLOR]
						);
						system->ResizeDockspace(data->dockspace_index, delta_x, ECS_UI_BORDER_RIGHT, data->dockspace_type);
					}
					else if (data->border_hover.IsLeft()) {
						float2 border_position = system->GetOuterDockspaceBorderPosition(dockspace, ECS_UI_BORDER_LEFT);
						float2 border_scale = system->GetOuterDockspaceBorderScale(dockspace, ECS_UI_BORDER_LEFT);
						SetSolidColorRectangle(
							border_position,
							border_scale,
							DarkenColor(system->m_descriptors.color_theme.hovered_borders, system->m_descriptors.color_theme.darken_hover_factor),
							solid_color,
							counts[ECS_TOOLS_UI_SOLID_COLOR]
						);
						system->ResizeDockspace(data->dockspace_index, delta_x, ECS_UI_BORDER_LEFT, data->dockspace_type);
					}
				};

				if (mouse_tracker->LeftButton() == MBHELD || mouse_tracker->LeftButton() == MBPRESSED) {
					DockspaceType floating_type = DockspaceType::Horizontal;
					UIDockspace* floating_dockspace = system->GetFloatingDockspaceFromDockspace(dockspace, GetDockspaceMaskFromType(data->dockspace_type), floating_type);
					if (floating_type == DockspaceType::Horizontal) {
						floating_type = data->dockspace_type;
					}
					system->SetNewFocusedDockspace(floating_dockspace, floating_type);
					system->SearchAndSetNewFocusedDockspaceRegion(dockspace, 0, data->dockspace_type);
					draw_color_and_resize();
				}
				else {
					system->DeallocateEventData();
					system->m_event = SkipEvent;
					draw_color_and_resize();
				}

				if (data->border_hover.IsBottom()) {
					if (data->border_hover.IsLeft()) {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NESW);
					}
					else if (data->border_hover.IsRight()) {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NWSE);
					}
					else {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NS);
					}
				}
				else if (data->border_hover.IsTop()) {
					if (data->border_hover.IsLeft()) {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NWSE);
					}
					else if (data->border_hover.IsRight()) {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NESW);
					}
					else {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NS);
					}
				}
				else if (data->border_hover.IsLeft() || data->border_hover.IsRight()) {
					system->m_application->ChangeCursor(ECS_CURSOR_SIZE_EW);
				}
			}

		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void HoverOuterDockspaceBorderEvent(
			UISystem* system,
			void* parameter,
			void** buffers,
			size_t* counts,
			float normalized_mouse_x,
			float normalized_mouse_y,
			const HID::MouseState* mouse,
			const HID::MouseTracker* mouse_tracker,
			const HID::KeyboardState* keyboard,
			const HID::KeyboardTracker* keyboard_tracker
		) {
			const UIDockspace* dockspaces[4] = {
				system->m_horizontal_dockspaces.buffer,
				system->m_vertical_dockspaces.buffer,
				system->m_floating_horizontal_dockspaces.buffer,
				system->m_floating_vertical_dockspaces.buffer
			};
			UIResizeEventData* data = (UIResizeEventData*)parameter;
			const UIDockspace* dockspace = &dockspaces[(unsigned int)data->dockspace_type][data->dockspace_index];

			if (system->m_focused_window_data.locked_window == 0 || (system->m_focused_window_data.active_location.dockspace == dockspace)) {
				UIVertexColor* solid_color = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
				if (data->border_hover.IsTop()) {
					float2 border_position = system->GetOuterDockspaceBorderPosition(dockspace, ECS_UI_BORDER_TOP);
					float2 border_scale = system->GetOuterDockspaceBorderScale(dockspace, ECS_UI_BORDER_TOP);
					SetSolidColorRectangle(
						border_position,
						border_scale,
						system->m_descriptors.color_theme.hovered_borders,
						solid_color,
						counts[ECS_TOOLS_UI_SOLID_COLOR]
					);
				}
				else if (data->border_hover.IsBottom()) {
					float2 border_position = system->GetOuterDockspaceBorderPosition(dockspace, ECS_UI_BORDER_BOTTOM);
					float2 border_scale = system->GetOuterDockspaceBorderScale(dockspace, ECS_UI_BORDER_BOTTOM);
					SetSolidColorRectangle(
						border_position,
						border_scale,
						system->m_descriptors.color_theme.hovered_borders,
						solid_color,
						counts[ECS_TOOLS_UI_SOLID_COLOR]
					);
				}
				if (data->border_hover.IsRight()) {
					float2 border_position = system->GetOuterDockspaceBorderPosition(dockspace, ECS_UI_BORDER_RIGHT);
					float2 border_scale = system->GetOuterDockspaceBorderScale(dockspace, ECS_UI_BORDER_RIGHT);
					SetSolidColorRectangle(
						border_position,
						border_scale,
						system->m_descriptors.color_theme.hovered_borders,
						solid_color,
						counts[ECS_TOOLS_UI_SOLID_COLOR]
					);
				}
				else if (data->border_hover.IsLeft()) {
					float2 border_position = system->GetOuterDockspaceBorderPosition(dockspace, ECS_UI_BORDER_LEFT);
					float2 border_scale = system->GetOuterDockspaceBorderScale(dockspace, ECS_UI_BORDER_LEFT);
					SetSolidColorRectangle(
						border_position,
						border_scale,
						system->m_descriptors.color_theme.hovered_borders,
						solid_color,
						counts[ECS_TOOLS_UI_SOLID_COLOR]
					);
				}

				if (data->border_hover.IsBottom()) {
					if (data->border_hover.IsLeft()) {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NESW);
					}
					else if (data->border_hover.IsRight()) {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NWSE);
					}
					else {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NS);
					}
				}
				else if (data->border_hover.IsTop()) {
					if (data->border_hover.IsLeft()) {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NWSE);
					}
					else if (data->border_hover.IsRight()) {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NESW);
					}
					else {
						system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NS);
					}
				}
				else if (data->border_hover.IsLeft() || data->border_hover.IsRight()) {
					system->m_application->ChangeCursor(ECS_CURSOR_SIZE_EW);
				}

			}
			system->DeallocateEventData();
			system->m_event = SkipEvent;

		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		void HoverInnerDockspaceBorderEvent(
			UISystem* system,
			void* parameter,
			void** buffers,
			size_t* counts,
			float normalized_mouse_x,
			float normalized_mouse_y,
			const HID::MouseState* mouse,
			const HID::MouseTracker* mouse_tracker,
			const HID::KeyboardState* keyboard,
			const HID::KeyboardTracker* keyboard_tracker
		) {
			UIMoveDockspaceBorderEventData* data = (UIMoveDockspaceBorderEventData*)parameter;
			const float masks[4] = { 1.0f, 0.0f, 1.0f, 0.0f };

			UIVertexColor* solid_color = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];

			if (system->m_focused_window_data.locked_window == 0 || (system->m_focused_window_data.active_location.dockspace == data->dockspace && 
				system->m_focused_window_data.active_location.border_index == data->border_index)) {
				float2 position = system->GetInnerDockspaceBorderPosition(data->dockspace, data->border_index, data->type);
				float2 scale = system->GetInnerDockspaceBorderScale(data->dockspace, data->border_index, data->type);
				SetSolidColorRectangle(
					position,
					scale,
					system->m_descriptors.color_theme.hovered_borders,
					solid_color,
					counts[ECS_TOOLS_UI_SOLID_COLOR]
				);

				if (data->type == DockspaceType::FloatingHorizontal || data->type == DockspaceType::Horizontal) {
					system->m_application->ChangeCursor(ECS_CURSOR_SIZE_EW);
				}
				else {
					system->m_application->ChangeCursor(ECS_CURSOR_SIZE_NS);
				}
			}

			system->DeallocateEventData();
			system->m_event = SkipEvent;
		}
		
		// -----------------------------------------------------------------------------------------------------------------------------------

		void SkipEvent(
			UISystem* system,
			void* parameter,
			void** buffers,
			size_t* counts,
			float normalized_mouse_x,
			float normalized_mouse_y,
			const HID::MouseState* mouse,
			const HID::MouseTracker* mouse_tracker,
			const HID::KeyboardState* keyboard,
			const HID::KeyboardTracker* keyboard_tracker
		) {
			ECS_CURSOR_TYPE current_cursor = system->m_application->GetCurrentCursor();
			
			if (current_cursor != ECS_CURSOR_DEFAULT) {
				system->m_application->ChangeCursor(ECS_CURSOR_DEFAULT);
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

	}

}