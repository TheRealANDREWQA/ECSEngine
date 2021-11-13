#pragma once
#include "UIStructures.h"
#include "UIHelpers.h"
#include "UIMacros.h"
#include "UIDrawerDescriptor.h"
#include "../../Application.h"
#include "../../Utilities/Mouse.h"
#include "../../Utilities/Keyboard.h"
#include "../../Rendering/ColorMacros.h"
#include "../../Internal/Multithreading/ThreadTask.h"

namespace ECSEngine {

	struct Graphics;
	struct TaskManager;
	struct ResourceManager;

	struct World;

	namespace Tools {

		// Bool acts as a placeholder, only interested to see if the resource existed previously
		using UISystemAddDynamicWindowElementTable = IdentifierHashTable<bool, ResourceIdentifier, HashFunctionPowerOfTwo>;

		class ECSENGINE_API UISystem
		{
		public:
			// Can provide a global memory manager in order to have the first big allocation obtained from it
			// in order to not increase the arena memory size
			UISystem(
				Application* application,
				UIToolsAllocator* memory,
				HID::Keyboard* keyboard,
				HID::Mouse* mouse,
				Graphics* graphics,
				ResourceManager* resource,
				TaskManager* task_manager,
				const wchar_t* font_filename,
				const char* font_uv_descriptor,
				uint2 window_os_size,
				GlobalMemoryManager* initial_allocator = nullptr
			);

			void AddActionHandler(
				LinearAllocator* allocator,
				UIHandler* handler,
				float2 position,
				float2 scale,
				UIActionHandler action
			);

			template<typename T>
			void AddActionHandler(
				LinearAllocator* allocator,
				UIHandler* handler,
				float2 position,
				float2 scale,
				const T* data,
				Action action,
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				AddActionHandler(
					allocator,
					handler,
					position,
					scale,
					{
						action,
						data,
						sizeof(T),
						phase
					}
				);
			}

			void AddDefaultHoverable(const UISystemDefaultHoverableData& data);

			void AddHoverableToDockspaceRegion(
				unsigned int thread_id,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler
			);

			void AddHoverableToDockspaceRegion(
				LinearAllocator* allocator,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler
			);

			template<typename T>
			void AddHoverableToDockspaceRegion(
				unsigned int thread_id,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				T* data,
				Action action,
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				AddHoverableToDockspaceRegion(
					thread_id,
					dockspace,
					border_index,
					position,
					scale,
					{
						action,
						(void*)data,
						sizeof(T),
						phase
					}	
				);
			}

			void AddDefaultClickable(const UISystemDefaultClickableData& data);

			void AddDefaultClickable(
				const UISystemDefaultClickableData& data, 
				LinearAllocator* allocator, 
				UIHandler* hoverable,
				UIHandler* clickable
			);

			void AddDefaultHoverableClickable(UISystemDefaultHoverableClickableData& data);

			void AddDefaultHoverableClickable(
				UISystemDefaultHoverableClickableData& data,
				LinearAllocator* allocator,
				UIHandler* hoverable,
				UIHandler* clickable
			);

			void AddClickableToDockspaceRegion(
				unsigned int thread_id,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler
			);

			void AddClickableToDockspaceRegion(
				LinearAllocator* allocator,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler
			);

			template<typename T>
			void AddClickableToDockspaceRegion(
				unsigned int thread_id,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				T* data,
				Action action,
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				AddClickableToDockspaceRegion(
					thread_id,
					dockspace,
					border_index,
					position,
					scale, 
					{
						action,
						data,
						sizeof(T),
						phase
					}
				);
			}

			// This is a general action
			void AddDoubleClickActionToDockspaceRegion(
				unsigned int thread_id,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				unsigned int identifier,
				size_t duration_between_clicks,
				UIActionHandler first_click_handler,
				UIActionHandler second_click_handler
			);

			void AddGeneralActionToDockspaceRegion(
				unsigned int thread_id,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler
			);

			void AddGeneralActionToDockspaceRegion(
				LinearAllocator* allocator,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler
			);

			template<typename T>
			void AddGeneralActionToDockspaceRegion(
				unsigned int thread_id,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				T* data,
				Action action,
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				AddGeneralActionToDockspaceRegion(
					thread_id,
					dockspace,
					border_index,
					position,
					scale,
					{
						action,
						data,
						sizeof(T),
						phase
					}
				);
			}

			template<typename Function>
			void AddMultipleActionHandlers(
				unsigned int thread_id,
				UIDockspace* dockspace,
				unsigned int border_index,
				const float2* position,
				const float2* scale,
				const size_t* data_size,
				const void** data,
				const Action* action,
				const UIDrawPhase* phase,
				size_t count,
				Function&& function
			) {
				for (size_t index = 0; index < count; index++) {
					function(
						thread_id,
						dockspace,
						border_index,
						position[index],
						scale[index],
						{
							action[index],
							data[index],
							data_size[index],
							phase[index]
						}
					);
				}
			}

			template<typename FunctionData, typename Function>
			void AddMultipleDefaultActionHandlers(const FunctionData* data, size_t count, Function&& function) {
				for (size_t index = 0; index < count; index++) {
					function(data[index]);
				}
			}

			template<typename FunctionData, typename Function>
			void AddMultipleDefaultActionHandlers(FunctionData& data, size_t count, Function&& function, float2* transforms) {
				for (size_t index = 0; index < count; index++) {
					data.position = transforms[index * 2];
					data.scale = transforms[index * 2 + 1];
					function(data);
				}
			}

			template<typename FunctionData, typename Function>
			void AddMultipleDefaultActionHandlers(const Stream<FunctionData>& data, Function&& function) {
				AddMultipleDefaultActionHandlers(data.buffer, data.size, function);
			}

			template<typename FunctionData, typename Function>
			void AddMultipleDefaultActionHandlers(FunctionData& data, const Stream<float2>& transforms, Function&& function) {
				AddMultipleDefaultActionHandlers(data.buffer, data.size, function, transforms.buffer);
			}
			
			// this is triggered when the element is placed over the center of the Docking gizmo
			void AddWindowToDockspaceRegion(
				unsigned int window_index,
				unsigned int dockspace_index,
				CapacityStream<UIDockspace>& dockspace_stream,
				unsigned char border_index
			);

			void AddWindowToDockspaceRegion(
				unsigned int window_index,
				UIDockspace* dockspace,
				unsigned char border_index
			);

			void* AllocateHandlerMemory(unsigned int thread_id, size_t size, size_t alignment, const void* memory_to_copy);

			void* AllocateHandlerMemory(LinearAllocator* allocator, size_t size, size_t alignment, const void* memory_to_copy);

			// this is triggered when the element is placed over one of the four sides of the Docking gizmo, not in center of it
			void AddElementToDockspace(
				unsigned char element_index,
				bool is_dock,
				unsigned char element_position,
				unsigned int dockspace_index,
				DockspaceType dockspace_type
			);

			void AddElementToDockspace(
				unsigned char element_index,
				bool is_dock,
				unsigned char element_position,
				DockspaceType dockspace_type,
				UIDockspace* dockspace
			);

			// this is triggered when the element is placed over one of the four sides of the Docking gizmo, not in center of it
			// border_width is used to normalize the new windows
			void AddWindowToDockspace(
				unsigned char element_index,
				unsigned char element_position,
				unsigned int dockspace_index,
				CapacityStream<UIDockspace>& child_dockspaces,
				CapacityStream<UIDockspace>& dockspace_stream,
				float border_width,
				float mask
			);

			void AddWindowToDockspace(
				unsigned char element_index,
				unsigned char element_position,
				UIDockspace* dockspace,
				CapacityStream<UIDockspace>& child_dockspaces,
				float border_width,
				float mask
			);

			// this is triggered when the element is placed over one of the four sides of the Docking gizmo, not in center of it
			// border_width is used to normalize the new windows; last parameter must be the dockspace y scale for horizontal 
			// and x scale for vertical
			void AddDockspaceToDockspace(
				unsigned char element_index,
				DockspaceType child_type,
				unsigned char element_position,
				unsigned int dockspace_index,
				DockspaceType dockspace_type,
				float border_width,
				float adjust_dockspace_borders,
				float mask
			);

			// this is triggered when the element is placed over one of the four sides of the Docking gizmo, not in center of it
			// border_width is used to normalize the new windows; last parameter must be the dockspace y scale for horizontal 
			// and x scale for vertical
			void AddDockspaceToDockspace(
				unsigned char child_index,
				DockspaceType child_type,
				unsigned char element_position,
				UIDockspace* dockspace,
				float border_width,
				float adjust_dockspace_borders,
				float mask
			);

			void AddWindowMemoryResource(void* resource, unsigned int window_index);

			void AddWindowMemoryResourceToTable(void* resource, ResourceIdentifier identifier, unsigned int window_index);

			void AppendDockspaceResize(
				unsigned char dockspace_index,
				CapacityStream<UIDockspace>* dockspace,
				float new_scale_to_resize,
				float old_scale,
				float other_scale,
				float mask
			);

			void AppendDockspaceResize(
				UIDockspace* dockspace,
				float new_scale_to_resize,
				float old_scale,
				float other_scale,
				float mask
			);

			void AppendDockspaceResize(
				UIDockspace* dockspace,
				float other_scale,
				DockspaceType type
			);

			// It returns a list of streams that will be needed when finishing the drawer element
			void AddWindowDrawerElement(unsigned int window_index, const char* name, Stream<void*> allocations, Stream<ResourceIdentifier> table_resources);

			void BindWindowHandler(Action action, Action data_initializer, size_t data_size);

			void CalculateDockspaceRegionHeaders(
				unsigned int dockspace_index,
				unsigned int border_index,
				float offset_mask,
				const CapacityStream<UIDockspace>& dockspaces,
				float2* sizes
			) const;

			void CalculateDockspaceRegionHeaders(
				const UIDockspace* dockspace,
				unsigned int border_index,
				float offset_mask,
				float2* sizes
			) const;

			// it will automatically set the event if a border is hovered
			bool CheckDockspaceInnerBorders(
				float2 point_position,
				unsigned int dockspace_index,
				DockspaceType dockspace_type
			);

			// it will automatically set the event if a border is hovered
			// border_offset must be either 1 or 0, in order to apply the correct offset
			bool CheckDockspaceInnerBorders(
				float2 point_position,
				UIDockspace* dockspace,
				DockspaceType type
			);

			// checks the Top, Bottom, Left and Right borders of the dockspace limits
			BorderHover CheckDockspaceOuterBorders(
				float2 point_position,
				unsigned int dockspace_index,
				DockspaceType dockspace_type
			) const;

			// checks the Top, Bottom, Left and Right borders of the dockspace limits
			BorderHover CheckDockspaceOuterBorders(
				float2 point_position,
				unsigned int dockspace_index,
				const CapacityStream<UIDockspace>& dockspace_stream
			) const;

			bool CheckDockspaceResize(float2 mouse_position);

			bool CheckDockspaceResize(unsigned int dockspace_index, DockspaceType type, float2 mouse_position);

			bool CheckParentInnerBorders(
				float2 mouse_position,
				const unsigned int* dockspace_indices,
				const DockspaceType* dockspace_types,
				unsigned int parent_count
			);

			void Clear();

			void ClearFixedDockspace(UIDockspace* dockspace, DockspaceType type);

			void ClearFixedDockspace(UIDockspaceBorder* dockspace, DockspaceType type);

			void ConfigureToolTipBase(UITooltipBaseData* data) const;

			// it will convert the characters into a row of text sprites; position represents the x and y of the
			template<bool horizontal = true, bool invert_order = false>
			void ConvertCharactersToTextSprites(
				const char* characters,
				float2 position,
				UISpriteVertex* vertex_buffer,
				unsigned int character_count,
				Color color = Color((unsigned char)255, 255, 255, 255),
				unsigned int buffer_offset = 0,
				float font_size = 0.01f,
				float character_spacing = 0.001f
			) {
				ConvertCharactersToTextSprites<horizontal, invert_order>(
					characters,
					position,
					vertex_buffer,
					character_count,
					color,
					buffer_offset,
					{font_size * ECS_TOOLS_UI_FONT_X_FACTOR, font_size},
					character_spacing
				);
			}

			// it will convert the characters into a row of text sprites; position represents the x and y of the 
			template<bool horizontal = true, bool invert_order = false>
			ECSENGINE_API void ConvertCharactersToTextSprites(
				const char* characters,
				float2 position,
				UISpriteVertex* vertex_buffer,
				unsigned int character_count,
				Color color,
				unsigned int buffer_offset,
				float2 font_size,
				float character_spacing
			);

			template<bool horizontal = true, bool invert_order = false>
			void ConvertFloatToTextSprites(
				UISpriteVertex* vertices,
				size_t& count, 
				float value,
				float2 position,
				size_t precision = 2,
				Color color = ECS_TOOLS_UI_TEXT_COLOR,
				float font_size = ECS_TOOLS_UI_FONT_SIZE,
				float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING
			) {
				ConvertFloatToTextSprites<horizontal, invert_order>(
					vertices,
					count,
					value,
					position,
					font_size * ECS_TOOLS_UI_FONT_X_FACTOR,
					font_size,
					precision,
					color,
					character_spacing
				);
			}

			template<bool horizontal = true, bool invert_order = false>
			ECSENGINE_API void ConvertFloatToTextSprites(
				UISpriteVertex* vertices,
				size_t& count,
				float value,
				float2 position,
				float font_size_x,
				float font_size_y,
				size_t precision = 2,
				Color color = ECS_TOOLS_UI_TEXT_COLOR,
				float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING
			);

			template<bool horizontal = true, bool invert_order = false>
			void ConvertDoubleToTextSprites(
				UISpriteVertex* vertices,
				size_t& count,
				double value,
				float2 position,
				size_t precision = 2,
				Color color = ECS_TOOLS_UI_TEXT_COLOR,
				float font_size = ECS_TOOLS_UI_FONT_SIZE,
				float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING
			) {
				ConvertDoubleToTextSprites<horizontal, invert_order>(
					vertices,
					count,
					value,
					position,
					font_size * ECS_TOOLS_UI_FONT_X_FACTOR,
					font_size,
					precision,
					color,
					character_spacing
					);
			}

			template<bool horizontal = true, bool invert_order = false>
			ECSENGINE_API void ConvertDoubleToTextSprites(
				UISpriteVertex* vertices,
				size_t& count,
				double value,
				float2 position,
				float font_size_x,
				float font_size_y,
				size_t precision = 2,
				Color color = ECS_TOOLS_UI_TEXT_COLOR,
				float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING
			);

			void CreateSpriteTexture(const wchar_t* filename, UISpriteTexture* sprite_view);

			void CreateDockspaceBorder(
				UIDockspace* dockspace, 
				unsigned int border_index,
				float position, 
				unsigned char element,
				bool is_dock,
				size_t border_flags = 0
			);

			void CreateEmptyDockspaceBorder(
				UIDockspace* dockspace,
				unsigned int border_index
			);

			unsigned int CreateDockspace(
				UIElementTransform transform,
				CapacityStream<UIDockspace>& dockspace_stream,
				float border_position,
				unsigned char element,
				bool is_dock,
				DockspaceType type,
				size_t flags = 0
			);

			unsigned int CreateEmptyDockspace(
				CapacityStream<UIDockspace>& dockspace_stream,
				DockspaceType type
			);

			// only floating dockspaces can be created as fixed ones
			unsigned int CreateFixedDockspace(
				UIElementTransform transform,
				CapacityStream<UIDockspace>& dockspace_stream,
				float border_position,
				unsigned char element,
				bool is_dock,
				DockspaceType type,
				size_t flags = 0
			);

			unsigned int CreateDockspace(
				UIElementTransform transform,
				DockspaceType dockspace_type,
				unsigned char element,
				bool is_dock,
				size_t flags = 0
			);

			unsigned int CreateEmptyDockspace(
				DockspaceType type
			);

			// only floating dockspaces can be created as fixed ones
			unsigned int CreateFixedDockspace(
				UIElementTransform transform,
				DockspaceType dockspace_type,
				unsigned char element,
				bool is_dock,
				size_t border_flags = 0
			);

			unsigned int CreateDockspace(
				UIDockspace* dockspace_to_copy,
				DockspaceType type_to_create
			);

			// only floating dockspaces can be created as fixed ones
			unsigned int CreateFixedDockspace(
				UIDockspace* dockspace_to_copy,
				DockspaceType type_to_create
			);

			// confusion with Win32 CreateWindow
			unsigned int Create_Window(const UIWindowDescriptor& descriptor, bool do_not_initialize_handler = false);

			unsigned int CreateEmptyWindow();

			unsigned int CreateWindowAndDockspace(const UIWindowDescriptor& descriptor, size_t additional_flags = 0);

			void CullRegionHeader(
				const UIDockspace* dockspace,
				unsigned int border_index,
				unsigned int window_index_in_region,
				float offset_mask,
				float add_x,
				const float2* sizes,
				void** buffers,
				size_t* counts
			);

			void CullHandler(
				UIHandler* handler,
				float top,
				float left,
				float bottom,
				float right
			) const;

			void DeallocateDockspaceBorderResource(UIDockspace* dockspace, unsigned int border_index);

			void DeallocateClickableHandler();

			void DeallocateHoverableHandler();

			void DeallocateGeneralHandler();

			void DeallocateEventData();

			void DecrementWindowDynamicResource(unsigned int window_index);

			// this is the safe way of destroying dockspaces since if in the same frame have been destroyed
			// other dockspaces of the same type they will change position and can results in not detecting
			// or false detecting.
			template<bool destroy_internal_dockspaces = false, bool deallocate_borders = false>
			void DestroyDockspace(UIDockspaceBorder* border, DockspaceType type);

			// it returns wheter or not it found a match; if using delete non referenced this can lead to
			// swapping deleted windows and advancing to the next window even tho this one should be deleted 
			// aswell
			bool DestroyWindow(unsigned int window_index);

			// Returns whether or not the window was found
			template<bool destroy_dockspace_if_fixed = true>
			bool DestroyWindowIfFound(const char* name);

			void DestroyNonReferencedWindows();

			bool DetectEvents(float2 mouse_position);

			unsigned int DetectActiveHandler(const UIHandler* handler, float2 mouse_position, unsigned int offset) const;

			bool DetectHoverables(
				size_t* counts,
				void** buffers,
				UIDockspace* dockspace, 
				unsigned int border_index, 
				float dockspace_mask,
				DockspaceType type,
				float2 mouse_position,
				unsigned int thread_id,
				unsigned int offset
			); 

			bool DetectClickables(
				size_t* counts,
				void** buffers,
				UIDockspace* dockspace,
				unsigned int border_index,
				float dockspace_mask,
				DockspaceType type,
				float2 mouse_position,
				unsigned int thread_id,
				unsigned int offset
			);

			bool DetectGenerals(
				size_t* counts,
				void** buffers,
				UIDockspace* dockspace,
				unsigned int border_index,
				float dockspace_mask,
				DockspaceType type,
				float2 mouse_position,
				unsigned int thread_id,
				unsigned int offset
			);

			unsigned int DoFrame();

			void Draw(float2 mouse_position, void** system_buffers, size_t* system_counts);

			void DrawCollapseTriangleHeader(
				void** buffers,
				size_t* counts,
				UIDockspace* dockspace,
				unsigned int border_index,
				float mask,
				bool is_open,
				UIDrawPhase phase = UIDrawPhase::Late
			);

			void DrawDockingGizmo(
				float2 region_position,
				size_t* counts,
				void** buffers,
				bool draw_central_rectangle
			);

			// left - top - right - bottom - central fill order of the transforms
			void DrawDockingGizmo(
				float2 region_position,
				size_t* counts,
				void** buffers,
				bool draw_central_rectangle,
				float2* transforms
			);

			void DrawPass(
				CapacityStream<VertexBuffer>& buffers,
				CapacityStream<UIDynamicStream<UISpriteTexture, true>>& sprite_textures,
				ConstantBuffer& viewport_buffer,
				const size_t* counts,
				GraphicsContext* context,
				void* extra_information,
				unsigned int material_offset
			);

			// this cannot handle render region
			template<UIDrawPhase phase>
			ECSENGINE_API void DrawPass(
				UIDrawResources& resources,
				const size_t* counts,
				float2 viewport_position,
				float2 viewport_scale,
				GraphicsContext* context
			);

			ID3D11CommandList* DrawDockspaceRegion(const UIDrawDockspaceRegionData* draw_data);

			void DrawDockspaceRegion(const UIDrawDockspaceRegionData* draw_data, bool active);

			void DrawDockspaceRegionHeader(
				unsigned int thread_id,
				UIDockspace* dockspace,
				unsigned int border_index,
				float offset_mask,
				void** buffers,
				size_t* vertex_count
			);

			void DrawDockspaceRegionBackground(
				unsigned int thread_id,
				UIDockspace* dockspace,
				unsigned int border_index,
				float offset_mask,
				void** buffers,
				size_t* vertex_count
			);

			void DrawDockspaceRegionBorders(
				float2 region_position,
				float2 region_scale,
				void** buffers,
				size_t* vertex_count
			);

			void DrawFixedDockspaceRegionBackground(
				const UIDockspace* dockspaces,
				unsigned int border_index,
				float2 position,
				float2 scale,
				void** buffers,
				size_t* vertex_count
			);

			// returns the size of the window
			float2 DrawToolTipSentence(
				ActionData* action_data,
				const char* characters,
				UITooltipBaseData* data
			);

			// returns the size of the window; aligned to left and right text characters pointers
			// should have different rows separated by \n
			float2 DrawToolTipSentenceWithTextToRight(
				ActionData* ECS_RESTRICT action_data,
				const char* ECS_RESTRICT aligned_to_left_text,
				const char* ECS_RESTRICT aligned_to_right_text,
				UITooltipBaseData* ECS_RESTRICT data
			);

			float2 DrawToolTipSentenceSize(
				const char* characters,
				UITooltipBaseData* data
			);
			
			//aligned to left and right text characters pointers should have different rows separated by \n
			float2 DrawToolTipSentenceWithTextToRightSize(
				const char* ECS_RESTRICT aligned_to_left_text,
				const char* ECS_RESTRICT aligned_to_right_text,
				UITooltipBaseData* ECS_RESTRICT data
			);

			bool ExistsWindowResource(unsigned int window_index, const char* name) const;

			bool ExistsWindowResource(unsigned int window_index, Stream<void> identifier) const;

			void FillActionDataSystemParameters(ActionData* action_data);

			void FillWindowDataAfterFileLoad(Stream<UIWindowSerializedMissingData> data);

			void FillViewportBuffer(float* buffer, float2 viewport_position, float2 viewport_half_scale);

			void FillViewportBuffer(float2 viewport_position, float2 viewport_scale, float* buffer);

			void FinalizeColorTheme();

			void FinalizeLayout();

			void FinalizeFont();

			void FinalizeElementDescriptor();

			unsigned int FindCharacterUVFromAtlas(char character) const;

			unsigned int FindCharacterUVFromAtlas(char character, CharacterType& character_type) const;

			void* FindWindowResource(unsigned int window_index, const void* identifier, unsigned int identifier_size) const;

			void FixFixedDockspace(UIDockspaceLayer old, UIDockspaceLayer new_layer);

			void FixBackgroundDockspace(UIDockspaceLayer old, UIDockspaceLayer new_layer);

			template<bool horizontal = true>
			ECSENGINE_API float2 GetTextSpan(
				const char* characters,
				unsigned int character_count,
				float font_size_x,
				float font_size_y,
				float character_spacing
			) const;

			float GetSpaceXSpan(float font_size_x);
			
			float GetUnknownCharacterXSpan(float font_size_x);

			float2 GetDockspaceBorderPosition(const UIDockspace* dockspace, unsigned int border_index, float mask) const;

			float2 GetDockspaceBorderScale(const UIDockspace* dockspace, unsigned int border_index, float mask) const;

			// searches only inner borders, not the first one and the last one which are outer
			unsigned int GetDockspaceBorderFromMouseCoordinates(
				float2 mouse_position,
				UIDockspace** dockspace, 
				float& mask
			) const;

			// searches only inner borders, not the first one and the last one which are outer
			unsigned int GetDockspaceBorderFromMouseCoordinates(
				float2 mouse_position, 
				UIDockspace** dockspace, 
				float& mask, 
				unsigned int floating_dockspace, 
				DockspaceType type
			) const;

			// searches only inner borders, not the first one and the last one which are outer
			unsigned int GetDockspaceBorderFromMouseCoordinates(
				float2 mouse_position,
				UIDockspace** dockspace,
				float& mask,
				UIDockspace* dockspace_to_search,
				DockspaceType type_to_search
			) const;

			float GetDockspaceMaskFromType(DockspaceType type) const;

			unsigned int GetDockspaceIndex(const UIDockspace* dockspace, DockspaceType type) const;

			// only horizontal and vertical are searched, not floating
			unsigned int GetDockspaceIndexFromMouseCoordinates(float2 mouse_position, DockspaceType& dockspace_type) const;

			UIDockspace* GetDockspaceFromWindow(unsigned int window_index, unsigned int& border_index, DockspaceType& type);

			unsigned int GetDockspaceIndexFromWindow(unsigned int window_index, unsigned int& border_index, DockspaceType& type);

			float2 GetDockspaceRegionPosition(const UIDockspace* dockspace, unsigned int border_index, float offset_mask) const;

			float2 GetDockspaceRegionScale(const UIDockspace* dockspace, unsigned int border_index, float offset_mask) const;

			float2 GetDockspaceBorderOffset(const UIDockspace* dockspace, unsigned int border_index, float offset_mask) const;

			// only floating horizontal and floating vertical are searched, not children dockspaces
			unsigned int GetFloatingDockspaceIndexFromMouseCoordinates(float2 mouse_position, DockspaceType& dockspace_type) const;

			unsigned int GetDockspaceIndexFromMouseCoordinatesWithChildren(
				float2 mouse_position,
				DockspaceType& dockspace_type,
				unsigned int* parent_indicies,
				DockspaceType* dockspace_types,
				unsigned int& parent_count
			);

			void GetDockspacesFromParent(
				unsigned int dockspace_index,
				DockspaceType dockspace_type,
				unsigned int* subindicies,
				DockspaceType* subtypes,
				unsigned int& children_count
			) const;

			// if the given dockspace is already a floating one, the type will not be set
			UIDockspace* GetFloatingDockspaceFromDockspace(UIDockspace* dockspace, float mask, DockspaceType& type) const;

			// if the given dockspace is already a floating one, the type will not be set
			UIDockspace* GetFloatingDockspaceFromDockspace(
				UIDockspace* dockspace, 
				float mask, 
				DockspaceType& type,
				unsigned int& border_index
			) const;

			bool GetLastRevertCommand(HandlerCommand& command, unsigned int window_index);

			void* GetLastSystemHandlerData();

			float2 GetMouseDelta(float2 mouse_position) const;

			float GetTextSpriteNDC_YScale() const;

			float GetTextSpriteYScale(float font_size_y) const;

			float GetTextSpriteSizeToScale(float scale) const;

			float2 GetTextSpriteSize(float size) const;

			void GetVisibleDockspaceRegions(Stream<UIVisibleDockspaceRegion>& windows);

			void GetVisibleDockspaceRegions(Stream<UIVisibleDockspaceRegion>& windows, bool from_lowest_layer_to_highest);

			// returns the expanded position
			float2 GetOuterDockspaceBorderPosition(const UIDockspace* dockspace, BorderType type) const;

			// returns the expanded scale
			float2 GetOuterDockspaceBorderScale(const UIDockspace* dockspace, BorderType type) const;

			float2 GetInnerDockspaceBorderPosition(
				const UIDockspace* dockspace,
				unsigned int border_index,
				DockspaceType type
			) const;

			float2 GetInnerDockspaceBorderPosition(
				const UIDockspace* dockspace,
				unsigned int border_index,
				float mask
			) const;

			float2 GetInnerDockspaceBorderScale(
				const UIDockspace* dockspace,
				unsigned int border_index,
				DockspaceType type
			) const;

			float2 GetInnerDockspaceBorderScale(
				const UIDockspace* dockspace,
				unsigned int border_index,
				float mask
			) const;

			void GetDockspaceRegionCollapseTriangleTransform(
				const UIDockspace* dockspace, 
				unsigned int border_index, 
				float offset_mask,
				float2& position,
				float2& scale
			);

			unsigned int GetDockspaceRegionFromMouse(float2 mouse_position, UIDockspace** dockspace, DockspaceType& type) const;

			unsigned int GetDockspaceRegionFromDockspace(
				float2 mouse_position, 
				UIDockspace** dockspace, 
				DockspaceType& type, 
				unsigned int dockspace_index, 
				DockspaceType type_to_search
			) const;

			UIDockspace* GetDockspaceParent(const UIDockspace* dockspace, DockspaceType type, DockspaceType& parent_type);

			UIDockspace* GetDockspaceParent(const UIDockspace* dockspace, DockspaceType type, DockspaceType& parent_type, unsigned int& border_index);

			// they will be placed in order of layers, from the foreground to the background
			void GetDockspaceRegionsFromMouse(
				float2 mouse_position,
				UIDockspace** dockspaces, 
				unsigned int* border_indices,
				DockspaceType* types,
				unsigned int& count
			) const;

			// recursive solver
			void GetDockspaceRegionsFromMouse(
				float2 mouse_position,
				UIDockspace* dockspace_to_search,
				DockspaceType type,
				UIDockspace** dockspaces,
				unsigned int* border_indices,
				DockspaceType* types,
				unsigned int& count
			) const;

			float2 GetDockspaceRegionHorizontalRenderSliderPosition(
				const UIDockspace* dockspace, 
				unsigned int border_index, 
				float dockspace_mask
			) const;

			float2 GetDockspaceRegionHorizontalRenderSliderPosition(float2 region_position, float2 region_scale) const;

			float2 GetDockspaceRegionHorizontalRenderSliderScale(
				const UIDockspace* dockspace, 
				unsigned int border_index,
				float dockspace_mask
			) const;

			float2 GetDockspaceRegionHorizontalRenderSliderScale(float2 region_scale) const;

			float2 GetDockspaceRegionVerticalRenderSliderPosition(
				const UIDockspace* dockspace, 
				unsigned int border_index, 
				float dockspace_mask
			) const;

			float2 GetDockspaceRegionVerticalRenderSliderPosition(
				float2 region_position,
				float2 region_scale, 
				bool draw_region_header
			) const;

			float2 GetDockspaceRegionVerticalRenderSliderScale(
				const UIDockspace* dockspace,
				unsigned int border_index,
				float dockspace_mask
			) const;

			float2 GetDockspaceRegionVerticalRenderSliderScale(
				float2 region_scale, 
				bool draw_region_header,
				bool horizontal_slider
			) const;

			UIDockspace* GetDockspace(unsigned int dockspace_index, DockspaceType type);

			const UIDockspace* GetConstDockspace(UIDockspaceLayer layer) const;

			UIDrawerDescriptor GetDrawerDescriptor(unsigned int window_index);

			void GetFixedDockspaceRegionsFromMouse(
				float2 mouse_position,
				UIDockspace** dockspaces,
				DockspaceType* types,
				unsigned int& count
			) const;

			const char* GetDrawElementName(unsigned int window_index, unsigned int index) const;

			ActionData GetFilledActionData(unsigned int window_index);

			size_t GetFrameIndex() const;

			float2 GetNormalizeMousePosition() const;

			float2 GetSquareScale(float y_scale) const;

			unsigned int GetWindowFromMouse(float2 mouse_position) const;

			unsigned int GetWindowFromDockspace(float2 mouse_position, unsigned int dockspace_index, DockspaceType type) const;

			unsigned int GetWindowFromName(const char* name) const;

			unsigned int GetWindowFromName(Stream<char> name) const;

			WindowTable* GetWindowTable(unsigned int window_index);

			const char* GetWindowName(unsigned int window_index) const;

			UIWindow* GetWindowPointer(unsigned int window_index);

			UIDefaultWindowHandler* GetDefaultWindowHandlerData(unsigned int window_index);

			void* GetWindowPrivateHandlerData(unsigned int window_index);

			void* GetWindowData(unsigned int window_index);

			unsigned int GetActiveWindowIndexInBorder(const UIDockspace* dockspace, unsigned int border_index) const;

			unsigned int GetWindowIndexFromBorder(const UIDockspace* dockspace, unsigned int border_index) const;

			float2 GetWindowPosition(unsigned int window_index);

			float2 GetWindowScale(unsigned int window_index);

			float2 GetWindowRenderRegion(unsigned int window_index);
			
			float4 GetUVForCharacter(char character) const;

			// Advances the next sprite texture
			UISpriteTexture* GetNextSpriteTextureToDraw(UIDockspace* dockspace, unsigned int border_index, UIDrawPhase phase, UISpriteType type);

			void HandleFocusedWindowClickable(float2 mouse_position, unsigned int thread_id);

			void HandleHoverable(float2 mouse_position, unsigned int thread_id, void** buffers, size_t* counts);

			void HandleFocusedWindowGeneral(float2 mouse_position, unsigned int thread_id);

			void HandleFocusedWindowCleanupGeneral(
				float2 mouse_position, 
				unsigned int thread_id,
				void* additional_data = nullptr,
				ActionAdditionalData additional_data_type = ActionAdditionalData::None
			);

			void HandleFocusedWindowCleanupHoverable(
				float2 mouse_position,
				unsigned int thread_id,
				void* additional_data = nullptr,
				ActionAdditionalData additional_data_type = ActionAdditionalData::None
			);

			void HandleDockingGizmoTransparentHover(
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 rectangle_position, 
				float2 rectangle_scale, 
				void** system_buffers, 
				size_t* counts
			);

			// addition type legend is the same as the DrawDockingGizmo transform:
			// 0 - left
			// 1 - top 
			// 2 - right
			// 3 - bottom
			// 4 - central
			void HandleDockingGizmoAdditionOfDockspace(
				UIDockspace* dockspace_receiver, 
				unsigned int border_index_receiver, 
				DockspaceType type_receiver, 
				unsigned int addition_type,
				UIDockspace* element_to_add, 
				DockspaceType element_type
			);

			void HandleSystemHandler();

			unsigned int HashString(const char* string) const;

			unsigned int HashString(LPCWSTR string) const;

			void IncrementWindowDynamicResource(unsigned int window_index, const char* name);

			void InitializeDefaultDescriptors();

			void InitializeMaterials();

			UIDrawerDescriptor InitializeDrawerDescriptorReferences(unsigned int window_index) const;

			void InitializeWindowDraw(unsigned int index, WindowDraw initialize);

			void InitializeWindowDraw(const char* window_name, WindowDraw initialize);

			void InitializeWindowDraw(Stream<char> window_name, WindowDraw initialize);

			void InitializeWindowDefaultHandler(size_t window_index, const UIWindowDescriptor& descriptor);

			bool IsEmptyFixedDockspace(const UIDockspace* dockspace) const;

			bool IsFixedDockspace(const UIDockspace* dockspace) const;

			bool IsFixedDockspace(const UIDockspaceBorder* border) const;

			bool IsBackgroundDockspace(const UIDockspace* dockspace) const;

			bool IsBackgroundDockspace(const UIDockspaceBorder* border) const;

			// Returns the index in the pop_up_windows stream
			unsigned int IsPopUpWindow(unsigned int window_index) const;

			// Returns the index in the pop_up_windows stream
			unsigned int IsPopUpWindow(const UIDockspace* dockspace) const;

			// Returns the index in the pop_up_windows stream
			unsigned int IsPopUpWindow(const UIDockspaceBorder* border) const;
			
			bool LoadUIFile(const wchar_t* filename, Stream<const char*>& window_names);

			bool LoadUIFile(Stream<wchar_t> filename, Stream<Stream<char>>& window_names);

			// returns whether or not a valid file was found
			bool LoadDescriptorFile(const wchar_t* filename);

			void MoveDockspaceBorder(UIDockspaceBorder* border, unsigned int border_index, float delta_x, float delta_y);

			void MoveDockspaceBorder(UIDockspaceBorder* border, unsigned int border_index, float delta_x, float delta_y, bool ignore_checks);

			float NormalizeHorizontalToWindowDimensions(float value) const;

			void NormalizeHorizontalToWindowDimensions(float* values, size_t count) const;

			void NormalizeHorizontalToWindowDimensions(Stream<float>& values) const;

			float NormalizeVerticalToWindowDimensions(float value) const;

			UIActionHandler PeekSystemHandler() const;

			void PopSystemHandler();

			void PopUpSystemHandler(const char* name, bool is_fixed, bool destroy_at_first_click = false, bool is_initialized = true, bool destroy_when_released = false);

			// Move the background dockspaces behind all other dockspaces
			void PushBackgroundDockspace();

			void PushActiveDockspaceRegion(Stream<UIVisibleDockspaceRegion>& regions) const;

			void PushSystemHandler(UIActionHandler handler);

			void ReadFontDescriptionFile(const char* filename);

			void RegisterFocusedWindowClickableAction(
				float2 position, 
				float2 scale, 
				Action action, 
				const void* data, 
				size_t data_size,
				UIDrawPhase phase
			);

			void RegisterInputLayouts();

			void RegisterVertexShader(wchar_t* filename);

			void RegisterVertexShaders();

			void RegisterPixelShader(wchar_t* filename);

			void RegisterPixelShaders();

			void RegisterSamplers();

			void RegisterWindowResource(size_t window_index, void* data, const void* identifier, unsigned int identifier_size);

			void ReleaseWindowResource(size_t window_index, const void* identifier, unsigned int identifier_size);

			void RemoveDockspace(unsigned int dockspace_index, DockspaceType dockspace_type);

			void RemoveDockspace(unsigned int dockspace_index, CapacityStream<UIDockspace>& dockspace_buffer);

			void RemoveWindowMemoryResource(unsigned int window_index, const void* buffer);

			void RemoveWindowMemoryResource(unsigned int window_index, unsigned int buffer_index);

			void RemoveWindowDynamicResource(unsigned int window_index, const char* buffer);

			void RemoveWindowDynamicResource(unsigned int window_index, unsigned int index);

			void RemoveUnrestoredWindows();

			// removes the last hoverable handler, but it does not free the memory from the temp allocator
			void RemoveHoverableHandler(UIDockspace* dockspace, unsigned int border_index);

			// removes the last clickable handler, but it does not free the memory from the temp allocator
			void RemoveClickableHandler(UIDockspace* dockspace, unsigned int border_index);

			// removes the last general handler, but it does not free the memory from the temp allocator
			void RemoveGeneralHandler(UIDockspace* dockspace, unsigned int border_index);
			
			// removes the last sprite texture written
			void RemoveSpriteTexture(UIDockspace* dockspace, unsigned int border_index, UIDrawPhase phase, UISpriteType type = UISpriteType::Normal);

			// returns whether or not the parent can continue resizing
			bool ResizeDockspace(
				unsigned int dockspace_index,
				float delta_scale,
				BorderType border,
				DockspaceType dockspace_type
			);

			void ResizeDockspaceUnguarded(
				unsigned int dockspace_index,
				float delta_scale,
				BorderType border,
				DockspaceType dockspace_type
			);

			// This function is used to restore window actions and data after UI file load
			void RestoreWindow(const char* window_name, const UIWindowDescriptor& descriptor);

			// This function is used to restore window actions and data after UI file load
			void RestoreWindow(Stream<char> window_name, const UIWindowDescriptor& descriptor);

			template<bool destroy_dockspace_if_last = true>
			ECSENGINE_API void RemoveWindowFromDockspaceRegion(UIDockspace* dockspace, DockspaceType type, unsigned int border_index, unsigned int window_index);

			template<bool destroy_windows = true>
			ECSENGINE_API void RemoveDockspaceBorder(UIDockspace* dockspace, unsigned int border_index, DockspaceType type);

			template<bool destroy_windows = true>
			ECSENGINE_API void RemoveFixedDockspaceBorder(UIDockspace* dockspace, unsigned int border_index, DockspaceType type);

			void RepairDockspaceReferences(unsigned int dockspace_index, DockspaceType type, unsigned int new_index);

			// returns whether or not a match was found
			bool RepairWindowReferences(unsigned int window_index);

			void ReplaceDockspace(UIDockspace* dockspace, unsigned int border_index, DockspaceType type);

			// returns the index of the child that is being hovered at that level
			unsigned int SearchDockspaceForChildrenDockspaces(
				float2 mouse_position,
				unsigned int dockspace_index,
				DockspaceType dockspace_type,
				DockspaceType& children_type,
				unsigned int* parent_indices,
				DockspaceType* border_mask_index,
				unsigned int& parent_count
			);

			void SetActiveWindow(unsigned int index);

			void SetActiveWindow(const char* name);

			void SetActiveWindow(Stream<char> name);

			void SetCleanupGeneralHandler();

			void SetCleanupHoverableHandler();

			void SetSystemDrawRegion(UIElementTransform transform);

			void SetTextureEvictionCount(unsigned int frame_count);

			void SetColorTheme(Color color);

			void SetNewFocusedDockspace(UIDockspace* dockspace, DockspaceType type);

			void SetNewFocusedDockspaceRegion(UIDockspace* dockspace, unsigned int border_index, DockspaceType type);

			void SearchAndSetNewFocusedDockspaceRegion(UIDockspace* dockspace, unsigned int border_index, DockspaceType type);

			// immediate context
			void SetSolidColorRenderState();

			void SetSolidColorRenderState(unsigned int thread_id);

			void SetSolidColorRenderState(GraphicsContext* context);

			// immediate context
			void SetTextSpriteRenderState();

			void SetTextSpriteRenderState(unsigned int thread_id);

			void SetTextSpriteRenderState(GraphicsContext* context);

			// immediate context
			void SetSpriteRenderState();

			void SetSpriteRenderState(unsigned int thread_id);

			void SetSpriteRenderState(GraphicsContext* context);

			// capable of handling rotated rectangle
			void SetSprite(
				UIDockspace* dockspace,
				unsigned int border_index,
				UISpriteTexture texture,
				float2 position,
				float2 scale,
				void** buffers,
				size_t* counts,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = float2(0.0f, 0.0f),
				float2 bottom_right_uv = float2(1.0f, 1.0f),
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// capable of handling rotated sprites
			void SetSprite(
				UIDockspace* dockspace,
				unsigned int border_index,
				const wchar_t* texture,
				float2 position,
				float2 scale,
				void** buffers,
				size_t* counts,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = {0.0f, 0.0f},
				float2 bottom_right_uv = {1.0f, 1.0f},
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// capable of handling rotated sprites
			void SetVertexColorSprite(
				UIDockspace* dockspace,
				unsigned int border_index,
				const wchar_t* texture,
				float2 position,
				float2 scale,
				void** buffers,
				size_t* counts,
				const Color* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// capable of handling rotated sprites
			void SetVertexColorSprite(
				UIDockspace* dockspace,
				unsigned int border_index,
				const wchar_t* texture,
				float2 position,
				float2 scale,
				void** buffers,
				size_t* counts,
				const ColorFloat* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// it multiplies by 6 the count
			void SetSpriteCluster(
				UIDockspace* dockspace,
				unsigned int border_index,
				const wchar_t* texture,
				unsigned int count,
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			void SetSpriteTextureToDraw(
				UIDockspace* dockspace,
				unsigned int border_index,
				const wchar_t* texture,
				UISpriteType type = UISpriteType::Normal,
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// This method is used for sprites that are being loaded outside of the UISystem
			void SetSpriteTextureToDraw(
				UIDockspace* dockspace,
				unsigned int border_index,
				UISpriteTexture texture,
				UISpriteType type = UISpriteType::Normal,
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			void SetPopUpWindowPosition(unsigned int window_index, float2 new_position);

			void SetPopUpWindowScale(unsigned int window_index, float2 new_scale);

			void SetDockspacePosition(UIDockspace* dockspace, float2 new_position);

			// primarly intended for use with pop up dockspaces, otherwise use resize dockspace
			void SetDockspaceScale(UIDockspace* dockspace, float2 new_scale);

			void SetViewport(float2 position, float2 scale, GraphicsContext* context);

			void SetViewport(GraphicsContext* context, float2 position, float2 half_scale);

			void SetWindowActions(unsigned int index, const UIWindowDescriptor& descriptor);

			void SetWindowActions(const char* name, const UIWindowDescriptor& descriptor);

			void SetWindowActions(Stream<char> name, const UIWindowDescriptor& descriptor);

			void SetWindowDestroyAction(unsigned int index, UIActionHandler handler);

			void SetWindowDestroyAction(const char* name, UIActionHandler handler);

			void SetWindowDestroyAction(Stream<char> name, UIActionHandler handler);

			void SetWindowPrivateAction(unsigned int index, UIActionHandler handler);

			void SetWindowPrivateAction(const char* name, UIActionHandler handler);

			void SetWindowPrivateAction(Stream<char> name, UIActionHandler handler);

			void SetWindowName(unsigned int window_index, const char* name);

			void SetWindowName(unsigned int window_index, const char* name, size_t name_size);

			void SetWindowMaxZoom(unsigned int window_index, float max_zoom);

			void SetWindowMinZoom(unsigned int window_index, float min_zoom);

			void SetWindowOSSize(uint2 new_size);

			unsigned int StoreElementDrawName(unsigned int window_index, const char* name);

			void TranslateDockspace(UIDockspace* dockspace, float2 translation);

			template<size_t flags = 0>
			void TrimPopUpWindow(
				unsigned int window_index,
				float2 position,
				float2 scale,
				unsigned int parent_window_index
			) {
				unsigned int border_index;
				DockspaceType type;

				const UIDockspace* parent_dockspace = GetDockspaceFromWindow(parent_window_index, border_index, type);
				float mask = GetDockspaceMaskFromType(type);

				float2 parent_position = GetDockspaceRegionPosition(parent_dockspace, border_index, mask);
				float2 parent_scale = GetDockspaceRegionScale(parent_dockspace, border_index, mask);
				TrimPopUpWindow<flags>(window_index, position, scale, parent_window_index, parent_position, parent_scale);
			}

			template<size_t flags = 0>
			void TrimPopUpWindow(
				unsigned int window_index,
				float2 position,
				float2 scale,
				unsigned int parent_window_index,
				float2 parent_position,
				float2 parent_scale
			) {
				unsigned int border_index, parent_border_index;
				DockspaceType type, parent_type;
				unsigned int dockspace_index = GetDockspaceIndexFromWindow(window_index, border_index, type);
				UIDockspace* dockspaces[] = {
					m_horizontal_dockspaces.buffer,
					m_vertical_dockspaces.buffer,
					m_floating_horizontal_dockspaces.buffer,
					m_floating_vertical_dockspaces.buffer
				};
				UIDockspace* dockspace = &dockspaces[(unsigned int)type][dockspace_index];
				UIDockspace* parent_dockspace = GetDockspaceFromWindow(parent_window_index, parent_border_index, parent_type);

				dockspace->transform.position = position;
				if (type == DockspaceType::FloatingHorizontal) {
					dockspace->transform.scale.y = scale.y;
					ResizeDockspace(dockspace_index, scale.x - dockspace->transform.scale.x, BorderType::Right, type);
				}
				else {
					dockspace->transform.scale.x = scale.x;
					ResizeDockspace(dockspace_index, scale.y - dockspace->transform.scale.y, BorderType::Bottom, type);
				}

				if constexpr (~flags & UI_TRIM_POP_UP_WINDOW_NO_HORIZONTAL_LEFT) {
					if (position.x < parent_position.x) {
						float difference = parent_position.x - position.x;
						dockspace->transform.position.x = parent_position.x;
						difference = function::Select(difference > dockspace->transform.scale.x, dockspace->transform.scale.x, difference);
						if (type == DockspaceType::FloatingHorizontal) {
							ResizeDockspace(dockspace_index, -difference, BorderType::Right, type);
						}
						else {
							dockspace->transform.scale.x -= difference;
						}
					}
				}

				if constexpr (~flags & UI_TRIM_POP_UP_WINDOW_NO_HORIZONTAL_RIGHT) {
					float right_border = parent_position.x + parent_scale.x;
					if (m_windows[parent_window_index].is_vertical_render_slider) {
						right_border -= m_descriptors.misc.render_slider_vertical_size;
					}

					if (position.x + scale.x > right_border) {
						float difference = position.x + scale.x - right_border;
						difference = function::Select(difference > dockspace->transform.scale.x, dockspace->transform.scale.x, difference);
						if (type == DockspaceType::FloatingHorizontal) {
							ResizeDockspace(dockspace_index, -difference, BorderType::Right, type);
						}
						else {
							dockspace->transform.scale.x -= difference;
						}
					}
				}

				if constexpr (~flags & UI_TRIM_POP_UP_WINDOW_NO_VERTICAL_TOP) {
					float top_border = parent_position.y;
					if (parent_dockspace->borders[parent_border_index].draw_region_header) {
						top_border += m_descriptors.misc.title_y_scale;
					}

					if (position.y < top_border) {
						float difference = top_border - position.y;
						dockspace->transform.position.y = top_border;
						difference = function::Select(difference > dockspace->transform.scale.y, dockspace->transform.scale.y, difference);
						if (type == DockspaceType::FloatingHorizontal) {
							dockspace->transform.scale.y -= difference;
						}
						else {
							ResizeDockspace(dockspace_index, -difference, BorderType::Bottom, type);
						}
					}
				}

				if constexpr (~flags & UI_TRIM_POP_UP_WINDOW_NO_VERTICAL_BOTTOM) {
					float bottom_border = parent_position.y + parent_scale.y;
					if (m_windows[parent_window_index].is_horizontal_render_slider) {
						bottom_border -= m_descriptors.misc.render_slider_horizontal_size;
					}

					if (position.y + scale.y > bottom_border) {
						float difference = position.y + scale.y - bottom_border;
						difference = function::Select(difference > dockspace->transform.scale.y, dockspace->transform.scale.y, difference);
						if (type == DockspaceType::FloatingHorizontal) {
							dockspace->transform.scale.y -= difference;
						}
						else {
							ResizeDockspace(dockspace_index, -difference, BorderType::Bottom, type);
						}
					}
				}
			}

			void UpdateDockspaceHierarchy();

			void UpdateDockspace(unsigned int dockspace_index, DockspaceType dockspace_type);

			bool WriteDescriptorsFile(const wchar_t* filename) const;

			bool WriteUIFile(const wchar_t* filename, CapacityStream<char>& error_message) const;

			//private:
			Application* m_application;
			UIToolsAllocator* m_memory;
			HID::Keyboard* m_keyboard;
			HID::Mouse* m_mouse;
			HID::MouseTracker* m_mouse_tracker;
			HID::KeyboardTracker* m_keyboard_tracker;
			Graphics* m_graphics;
			ResourceManager* m_resource_manager;
			TaskManager* m_task_manager;
			float2* m_font_character_uvs;
			UISystemDescriptors m_descriptors;
			UISystemDescriptors m_startup_descriptors;
			CapacityStream<UIDockspace> m_horizontal_dockspaces;
			CapacityStream<UIDockspace> m_vertical_dockspaces;
			CapacityStream<UIDockspace> m_floating_horizontal_dockspaces;
			CapacityStream<UIDockspace> m_floating_vertical_dockspaces;
			CapacityStream<UIWindow> m_windows;
			CapacityStream<UIDockspaceLayer> m_dockspace_layers;
			CapacityStream<UIDockspaceLayer> m_fixed_dockspaces;
			CapacityStream<UIDockspaceLayer> m_background_dockspaces;
			// these will index into m_dockspace layers
			CapacityStream<unsigned int> m_pop_up_windows;
			ThreadSafeQueue<ThreadTask> m_thread_tasks;
			UIRenderResources m_resources;
			float2 m_previous_mouse_position;
			// this is used to set the default handler for windows
			UIActionHandler m_window_handler;
			UIElementTransform m_system_draw_region;
			size_t m_frame_index;
			// Useful for calculating the normalized mouse value when scaling is being applied
			// at the OS level and try to remedy it
			uint2 m_window_os_size;
			void* m_event_data;
			unsigned short m_frame_pacing;
			unsigned short m_texture_evict_count;
			unsigned short m_texture_evict_target;
			bool m_execute_events;
			void (*m_event)(
				UISystem*,
				void*,
				void**,
				size_t*,
				float,
				float,
				const HID::MouseState*,
				const HID::MouseTracker*,
				const HID::KeyboardState*,
				const HID::KeyboardTracker*
			);
			UIFocusedWindowData m_focused_window_data;
			Stack<UIActionHandler> m_handler_stack;
		};

		void DrawDockspaceRegionThreadTask(unsigned int thread_index, World* world, void* data);

#pragma region Actions

		// ----------------------------------------------------- Actions ------------------------------------------------------

		ECSENGINE_API void PopUpWindowSystemHandler(ActionData* action_data);

		ECSENGINE_API void SkipAction(ActionData* action_data);

		void CloseXAction(ActionData* action_data);

		ECSENGINE_API void CloseXBorderClickableAction(ActionData* action_data);

		void CloseXBorderHoverableAction(ActionData* action_data);

		void CollapseTriangleClickableAction(ActionData* action_data);

		ECSENGINE_API void DefaultHoverableAction(ActionData* action_data);

		// this is primarly intended for late or system draw in which the hovered 
		// text will be replaced by the solid color;
		// if the vertex buffer is not null, it will memcpy those vertices
		// else will convert the characters at the specified offset
		ECSENGINE_API void DefaultTextHoverableAction(ActionData* action_data);

		ECSENGINE_API void DefaultClickableAction(ActionData* action_data);

		ECSENGINE_API void DoubleClickAction(ActionData* action_data);

		ECSENGINE_API void TextTooltipHoverable(ActionData* action_data);

		ECSENGINE_API void TooltipHoverable(ActionData* action_data);

		ECSENGINE_API void TextTooltipAlignTextToRight(Stream<char>& current_text, const char* new_text, size_t total_characters);

		ECSENGINE_API void ReleaseLockedWindow(ActionData* action_data);

		ECSENGINE_API void ConvertASCIIToWideAction(ActionData* action_data);

		ECSENGINE_API void ConvertASCIIToWideStreamAction(ActionData* action_data);

		ECSENGINE_API void DefaultHoverableWithToolTip(ActionData* action_data);

		void DragDockspaceAction(ActionData* action_data);

		void RegionHeaderAction(ActionData* action_data);

#pragma endregion

#pragma region Events

		// ---------------------------------------------------- Events ---------------------------------------------------------

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
		);

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
		);

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
		);

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
		);

		ECSENGINE_API void SkipEvent(
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
		);

#pragma endregion
	
	}

}


