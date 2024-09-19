#pragma once
#include "UIStructures.h"
#include "UIHelpers.h"
#include "UIMacros.h"
#include "UIDrawerDescriptor.h"
#include "../../Application.h"
#include "../../Input/Mouse.h"
#include "../../Input/Keyboard.h"
#include "../../Rendering/ColorUtilities.h"
#include "../../Multithreading/ThreadTask.h"

namespace ECSEngine {

	struct Graphics;
	struct TaskManager;
	struct ResourceManager;

	struct World;

	enum ECS_UI_FRAME_PACING {
		ECS_UI_FRAME_PACING_NONE = 0,
		ECS_UI_FRAME_PACING_LOW = 1,
		ECS_UI_FRAME_PACING_MEDIUM = 2,
		ECS_UI_FRAME_PACING_HIGH = 3,
		ECS_UI_FRAME_PACING_INSTANT = 4
	};

	namespace Tools {

		// Bool acts as a placeholder, only interested to see if the resource existed previously
		typedef HashTableDefault<bool> UISystemAddDynamicWindowElementTable;

		ECSENGINE_API UIToolsAllocator DefaultUISystemAllocator(GlobalMemoryManager* global_manager);

		// If the windows_to_draw are given, then it will use the draw function only for those,
		// All the other ones being used with a snapshot. When that mode is engaged, you can
		// Give the lazy update value to tell the ui system when to update the snapshot
		// The default is 25 ms, which is 40 fps
		struct UISystemDoFrameOptions {
			Stream<Stream<char>> windows_to_draw = {};
			unsigned int lazy_update_milliseconds = 25;
		};

		class ECSENGINE_API UISystem
		{
		public:
			// Can provide a global memory manager in order to have the first big allocation obtained from it
			// in order to not increase the arena memory size
			UISystem(
				Application* application,
				UIToolsAllocator* memory,
				Keyboard* keyboard,
				Mouse* mouse,
				Graphics* graphics,
				ResourceManager* resource,
				TaskManager* task_manager,
				uint2 window_os_size,
				uint2 monitor_size,
				GlobalMemoryManager* initial_allocator = nullptr
			);

			UISystem(const UISystem& other) = default;
			UISystem& operator= (const UISystem& other) = default;

			// If the cursor is at the border of the window it will be relocated to the other side
			void ActiveWrapCursorPosition();

			void* AllocateFromHandlerAllocator(ECS_UI_DRAW_PHASE phase, size_t size);

			void AddActionHandler(
				LinearAllocator* allocator,
				UIHandler* handler,
				float2 position,
				float2 scale,
				UIActionHandler action,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			template<typename T>
			void AddActionHandler(
				LinearAllocator* allocator,
				UIHandler* handler,
				float2 position,
				float2 scale,
				const T* data,
				Action action,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL,
				UIHandlerCopyBuffers copy_function = nullptr
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

			// Pushes the action handler as is
			void AddActionHandlerForced(
				UIHandler* handler,
				float2 position,
				float2 scale,
				UIActionHandler action_handler,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			// If highlight element is given, it will set it to true if it should highlight, else to false
			// The drag action will be called in case there is a match
			void AcquireDragDrop(
				float2 position,
				float2 scale,
				Stream<char> region_name,
				unsigned int window_index,
				Stream<Stream<char>> matching_names,
				bool* highlight_element = nullptr
			);

			void AddDragExitHandler(float2 position, float2 scale, Stream<char> name);

			void AddDefaultHoverable(const UISystemDefaultHoverableData& data);

			void AddFrameHandler(UIActionHandler handler);

			// If the resource size is 0, it will just reference the pointer
			// Returns the pointer stored internally (useful if size != 0)
			void* AddGlobalResource(Stream<void> resource, Stream<char> name);

			void AddHoverableToDockspaceRegion(
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void AddHoverableToDockspaceRegion(
				LinearAllocator* allocator,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			// Pushes the item without allocating anything
			void AddHoverableToDockspaceRegionForced(
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void AddDefaultClickable(const UISystemDefaultClickableData& data);

			void AddDefaultHoverableClickable(const UISystemDefaultHoverableClickableData& data);

			void AddClickableToDockspaceRegion(
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				ECS_MOUSE_BUTTON button_type,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void AddClickableToDockspaceRegion(
				LinearAllocator* allocator,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				ECS_MOUSE_BUTTON button_type,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void AddClickableToDockspaceRegionForced(
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				ECS_MOUSE_BUTTON button_type,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			// This is a general action
			// The identifier is used to differentiate between multiple double click actions
			void AddDoubleClickActionToDockspaceRegion(const UISystemDoubleClickData& data);

			void AddGeneralActionToDockspaceRegion(
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void AddGeneralActionToDockspaceRegion(
				LinearAllocator* allocator,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void AddGeneralActionToDockspaceRegionForced(
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void AddHandlerToDockspaceRegionAtIndex(
				UIHandler* handler,
				float2 position,
				float2 scale,
				UIActionHandler action_handler,
				UIHandlerCopyBuffers copy_function,
				unsigned int add_index
			);

			// this is triggered when the element is placed over the center of the Docking gizmo
			void AddWindowToDockspaceRegion(
				unsigned int window_index,
				unsigned int dockspace_index,
				CapacityStream<UIDockspace>& dockspace_stream,
				unsigned int border_index
			);

			void AddWindowToDockspaceRegion(
				unsigned int window_index,
				UIDockspace* dockspace,
				unsigned int border_index
			);

			void AddWindowSnapshotRunnable(
				UIDockspace* dockspace,
				unsigned int border_index,
				UIDockspaceBorderDrawSnapshotRunnable runnable
			);

			float AlignMiddleTextY(float position_y, float scale_y, float font_size_y, float padding = 0.0f);

			void* AllocateHandlerMemory(size_t size, size_t alignment, const void* memory_to_copy);

			void* AllocateHandlerMemory(LinearAllocator* allocator, size_t size, size_t alignment, const void* memory_to_copy);

			AllocatorPolymorphic AllocatorSnapshotRunnables(UIDockspace* dockspace, unsigned int border_index);

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

			ECS_INLINE AllocatorPolymorphic Allocator() const {
				return m_memory;
			}

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

			void AddWindowDynamicElement(unsigned int window_index, Stream<char> name, Stream<void*> allocations, Stream<ResourceIdentifier> table_resources);

			void AddWindowDynamicElementAllocation(unsigned int window_index, unsigned int index, void* allocation);

			void BindWindowHandler(Action action, Action data_initializer, size_t data_size);

			void CalculateDockspaceRegionHeaders(
				unsigned int dockspace_index,
				unsigned int border_index,
				float offset_mask,
				const CapacityStream<UIDockspace>& dockspaces,
				CapacityStream<float2>& sizes
			) const;

			void CalculateDockspaceRegionHeaders(
				const UIDockspace* dockspace,
				unsigned int border_index,
				float offset_mask,
				CapacityStream<float2>& sizes
			) const;

			void ChangeBorderFlags(UIDockspace* dockspace, unsigned int border_index, size_t flags);

			void ChangeBorderFlags(unsigned int window_index, size_t flags);

			// Updates the relevant structures to the aspect ratio change
			void ChangeAspectRatio(float current_ratio, float new_ratio);

			// Updates the dimension ratio of relevant structures
			void ChangeDimensionRatio(float2 current_ratio, float2 new_ratio);

			void ChangeFocusedWindowHoverable(UIActionHandler handler, float2 mouse_position = { -2.0f, -2.0f });

			void ChangeFocusedWindowGeneral(UIActionHandler handler, float2 mouse_position = { -2.0f, -2.0f });

			// Changes the name with the current index appended to the base name into the new index
			// E.g. Inspector 0 -> Inspector 1
			void ChangeWindowNameFromIndex(Stream<char> base_name, unsigned int current_index, unsigned int new_index);

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

			// It will convert the characters into a row of text sprites; position represents the x and y of the 
			void ConvertCharactersToTextSprites(
				Stream<char> characters,
				float2 position,
				UISpriteVertex* vertex_buffer,
				Color color,
				unsigned int buffer_offset,
				float2 font_size,
				float character_spacing,
				bool horizontal = true,
				bool invert_order = false
			);

			void ConvertFloatToTextSprites(
				UISpriteVertex* vertices,
				size_t& count,
				float value,
				float2 position,
				float2 font_size,
				size_t precision = 2,
				Color color = ECS_TOOLS_UI_TEXT_COLOR,
				float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING,
				bool horizontal = true,
				bool invert_order = false
			);

			void ConvertDoubleToTextSprites(
				UISpriteVertex* vertices,
				size_t& count,
				double value,
				float2 position,
				float2 font_size,
				size_t precision = 2,
				Color color = ECS_TOOLS_UI_TEXT_COLOR,
				float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING,
				bool horizontal = true,
				bool invert_order = false
			);

			void CreateSpriteTexture(Stream<wchar_t> filename, UISpriteTexture* sprite_view);

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

			void ComposeHoverable(
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				bool call_previous_before,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void ComposeHoverable(
				LinearAllocator* allocator,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				bool call_previous_before,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void ComposeClickable(
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				bool call_previous_before,
				ECS_MOUSE_BUTTON button_type,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void ComposeClickable(
				LinearAllocator* allocator,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				bool call_previous_before,
				ECS_MOUSE_BUTTON button_type,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void ComposeGeneral(
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				bool call_previous_before,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void ComposeGeneral(
				LinearAllocator* allocator,
				UIDockspace* dockspace,
				unsigned int border_index,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				bool call_previous_before,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void CullRegionHeader(
				const UIDockspace* dockspace,
				unsigned int border_index,
				unsigned int window_index_in_region,
				float offset_mask,
				Stream<float2> sizes,
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

			void DeactiveWrapCursorPosition();

			void DeallocateDockspaceBorderSnapshot(UIDockspace* dockspace, unsigned int border_index, bool free_allocator);

			void DeallocateDockspaceBorderResources(UIDockspace* dockspace, unsigned int border_index);

			void DeallocateEventData();

			// Removes all dynamic window resources - irrespective of their reference count
			void DeallocateWindowDynamicResources(unsigned int window_index);

			// Deallocates the border's snapshot that this window belongs to
			void DeallocateWindowSnapshot(unsigned int window_index);

			// Will deallocate the snapshot only if the window with that name exists, else it does nothing
			void DeallocateWindowSnapshot(Stream<char> window_name);

			// Deallocates the snapshots for all windows that have one
			void DeallocateAllWindowSnapshots();

			void DecrementWindowDynamicResource(unsigned int window_index);

			// this is the safe way of destroying dockspaces since if in the same frame have been destroyed
			// other dockspaces of the same type they will change position and can results in not detecting
			// or false detecting.
			template<bool destroy_internal_dockspaces = false, bool deallocate_borders = false>
			void DestroyDockspace(UIDockspaceBorder* border, DockspaceType type);

			// it returns wheter or not it found a match; if using delete non referenced this can lead to
			// swapping deleted windows and advancing to the next window even tho this one should be deleted 
			// as well
			bool DestroyWindow(unsigned int window_index);

			// This version removes it from its dockspace and then destroys it
			void DestroyWindowEx(unsigned int window_index);

			// Returns whether or not the window was found
			template<bool destroy_dockspace_if_fixed = true>
			bool DestroyWindowIfFound(Stream<char> name);

			void DestroyNonReferencedWindows();

			bool DetectEvents(float2 mouse_position);

			unsigned int DetectActiveHandler(const UIHandler* handler, float2 mouse_position, unsigned int offset) const;

			bool DetectHoverables(
				size_t* counts,
				void** buffers,
				UIDockspace* dockspace,
				unsigned int border_index,
				DockspaceType type,
				float2 mouse_position,
				unsigned int offset
			);

			bool DetectClickables(
				size_t* counts,
				void** buffers,
				UIDockspace* dockspace,
				unsigned int border_index,
				DockspaceType type,
				float2 mouse_position,
				unsigned int offset,
				ECS_MOUSE_BUTTON button_type
			);

			bool DetectGenerals(
				size_t* counts,
				void** buffers,
				UIDockspace* dockspace,
				unsigned int border_index,
				DockspaceType type,
				float2 mouse_position,
				unsigned int offset
			);

			ECS_UI_FRAME_PACING DoFrame(UISystemDoFrameOptions options = {});

			void Draw(float2 mouse_position, void** system_buffers, size_t* system_counts, UISystemDoFrameOptions options);

			void DrawCollapseTriangleHeader(
				void** buffers,
				size_t* counts,
				UIDockspace* dockspace,
				unsigned int border_index,
				float mask,
				bool is_open,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_LATE
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
				CapacityStream<UIDynamicStream<UISpriteTexture>>& sprite_textures,
				ConstantBuffer& viewport_buffer,
				const size_t* counts,
				GraphicsContext* context,
				void* extra_information,
				unsigned int material_offset
			);

			// this cannot handle render region
			template<ECS_UI_DRAW_PHASE phase>
			ECSENGINE_API void DrawPass(
				UIDrawResources& resources,
				const size_t* counts,
				float2 viewport_position,
				float2 viewport_scale,
				GraphicsContext* context
			);

			void DrawDockspaceRegion(const UIDrawDockspaceRegionData* draw_data, bool active_region);

			void DrawDockspaceRegionHeader(
				UIDockspace* dockspace,
				unsigned int border_index,
				float offset_mask,
				void** buffers,
				size_t* vertex_count
			);

			void DrawDockspaceRegionBackground(
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
				Stream<char> characters,
				UITooltipBaseData* data
			);

			// returns the size of the window; aligned to left and right text characters pointers
			// should have different rows separated by \n
			float2 DrawToolTipSentenceWithTextToRight(
				ActionData* action_data,
				Stream<char> aligned_to_left_text,
				Stream<char> aligned_to_right_text,
				UITooltipBaseData* data
			);

			// If the row_count is left at 0, then it will parse the entire
			// String. Else it will stop at the row given by that parameter
			float2 DrawToolTipSentenceSize(
				Stream<char> characters,
				UITooltipBaseData* data,
				unsigned int row_count = 0
			);

			// Aligned to left and right text characters pointers should have different rows separated by \n
			// If the row_count is left at 0, then it will parse the entire
			// String. Else it will stop at the row given by that parameter
			float2 DrawToolTipSentenceWithTextToRightSize(
				Stream<char> aligned_to_left_text,
				Stream<char> aligned_to_right_text,
				UITooltipBaseData* data,
				unsigned int row_count = 0
			);

			bool ExistsWindowResource(unsigned int window_index, Stream<char> name) const;

			bool ExistsWindowMemoryResource(unsigned int window_index, const void* pointer) const;

			void EvictOutdatedTextures();

			// Removes the given drag drop and deallocates everything used
			void EndDragDrop(Stream<char> name);

			bool ExistsDragDropExitHandler(Stream<char> name);

			void FillActionDataSystemParameters(ActionData* action_data);

			void FillWindowDataAfterFileLoad(Stream<UIWindowSerializedMissingData> data);

			void FillViewportBuffer(float* buffer, float2 viewport_position, float2 viewport_half_scale);

			void FillViewportBuffer(float2 viewport_position, float2 viewport_scale, float* buffer);

			void FinalizeColorTheme();

			void FinalizeLayout();

			void FinalizeFont();

			void FinalizeElementDescriptor();

			// Returns the index in the pop_up_windows stream
			unsigned int FindPopUpWindow(unsigned int window_index) const;

			// Returns the index in the pop_up_windows stream
			unsigned int FindPopUpWindow(const UIDockspace* dockspace) const;

			// Returns the index in the pop_up_windows stream
			unsigned int FindPopUpWindow(const UIDockspaceBorder* border) const;

			unsigned int FindCharacterType(char character) const;

			unsigned int FindCharacterType(char character, CharacterType& character_type) const;

			void* FindWindowResource(unsigned int window_index, Stream<char> name) const;

			void* FindWindowResource(unsigned int window_index, const void* identifier, unsigned int identifier_size) const;

			// Returns -1 if it doesn't find it. If the data is nullptr, then it will return the first frame handler
			// that has that function
			unsigned int FindFrameHandler(Action action, void* data = nullptr) const;

			void FixFixedDockspace(UIDockspaceLayer old, UIDockspaceLayer new_layer);

			void FixBackgroundDockspace(UIDockspaceLayer old, UIDockspaceLayer new_layer);

			ECS_INLINE float GetPixelSizeX() const {
				return m_pixel_size.x;
			}

			ECS_INLINE float GetPixelSizeY() const {
				return m_pixel_size.y;
			}

			ECS_INLINE float2 GetPixelSize() const {
				return m_pixel_size;
			}

			// For wide characters, it supports only the wide characters that are ASCII compatible
			template<typename CharacterType = char>
			ECSENGINE_API float2 GetTextSpan(
				Stream<CharacterType> characters,
				float2 font_size,
				float character_spacing,
				bool horizontal = true
			) const;

			// For wide characters, it supports only the wide characters that are ASCII compatible
			template<typename CharacterType = char>
			ECSENGINE_API float2 GetTextSpanLimited(
				Stream<CharacterType> characters,
				float2 font_size,
				float character_spacing,
				float2 scale_limit,
				size_t* character_count,
				bool invert_order,
				bool horizontal = true
			) const;

			// For wide characters, it supports only the wide characters that are ASCII compatible
			// It fills in the positions of the text sprites and their scale when positioned at the given translation
			template<typename CharacterType = char>
			ECSENGINE_API void GetTextCharacterPositions(
				Stream<CharacterType> characters,
				float2 font_size, 
				float character_spacing, 
				float2* positions,
				float2* scales, 
				float2 translation = { 0.0f, 0.0f },
				bool horizontal = true
			) const;

			float GetSpaceXSpan(float font_size_x);
			
			float GetUnknownCharacterXSpan(float font_size_x);

			float2 GetDockspaceBorderPosition(const UIDockspace* dockspace, unsigned int border_index, float mask) const;

			float2 GetDockspaceBorderScale(const UIDockspace* dockspace, unsigned int border_index, float mask) const;
			
			UIHandler* GetDockspaceBorderHandler(
				UIDockspace* dockspace, 
				unsigned int border_index, 
				bool hoverable, 
				ECS_MOUSE_BUTTON clickable_button, 
				bool general
			) const;

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

			unsigned int GetDockspaceIndex(const UIDockspace* dockspace, DockspaceType type) const;

			// only horizontal and vertical are searched, not floating
			unsigned int GetDockspaceIndexFromMouseCoordinates(float2 mouse_position, DockspaceType& dockspace_type) const;

			UIDockspace* GetDockspaceFromWindow(unsigned int window_index, unsigned int& border_index, DockspaceType& type);

			const UIDockspace* GetDockspaceFromWindow(unsigned int window_index, unsigned int& border_index, DockspaceType& type) const;

			unsigned int GetDockspaceIndexFromWindow(unsigned int window_index, unsigned int& border_index, DockspaceType& type) const;

			float2 GetDockspaceRegionPosition(const UIDockspace* dockspace, unsigned int border_index, float offset_mask) const;

			float2 GetDockspaceRegionScale(const UIDockspace* dockspace, unsigned int border_index, float offset_mask) const;

			float2 GetDockspaceBorderOffset(const UIDockspace* dockspace, unsigned int border_index, float offset_mask) const;

			// only floating horizontal and floating vertical are searched, not children dockspaces
			unsigned int GetFloatingDockspaceIndexFromMouseCoordinates(float2 mouse_position, DockspaceType& dockspace_type) const;

			unsigned int GetDockspaceIndexFromMouseCoordinatesWithChildren(
				float2 mouse_position,
				DockspaceType& dockspace_type,
				CapacityStream<unsigned int>* parent_indicies,
				CapacityStream<DockspaceType>* dockspace_types,
				unsigned int& parent_count
			);

			void GetDockspacesFromParent(
				unsigned int dockspace_index,
				DockspaceType dockspace_type,
				CapacityStream<unsigned int>* subindicies,
				CapacityStream<DockspaceType>* subtypes,
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

			// Returns nullptr if it doesn't exist
			Stream<void> GetGlobalResource(Stream<char> name) const;

			bool GetLastRevertCommand(HandlerCommand& command, unsigned int window_index);

			float2 GetMouseDelta(float2 mouse_position) const;

			int2 GetTexelMouseDelta() const;

			float GetTextSpriteNDC_YScale() const;

			float GetTextSpriteYScale(float font_size_y) const;

			float GetTextSpriteYSizeToScale(float scale) const;

			void GetVisibleDockspaceRegions(CapacityStream<UIVisibleDockspaceRegion>& windows) const;

			void GetVisibleDockspaceRegions(CapacityStream<UIVisibleDockspaceRegion>& windows, bool from_lowest_layer_to_highest) const;

			// returns the expanded position
			float2 GetOuterDockspaceBorderPosition(const UIDockspace* dockspace, ECS_UI_BORDER_TYPE type) const;

			// returns the expanded scale
			float2 GetOuterDockspaceBorderScale(const UIDockspace* dockspace, ECS_UI_BORDER_TYPE type) const;

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

			// Returns the border index of the region, -1 if it doesn't exist
			unsigned int GetDockspaceRegionFromMouse(float2 mouse_position, UIDockspace** dockspace, DockspaceType& type) const;

			// Returns the border index of the region, -1 if it doesn't exist
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
				const UIDockspace* dockspace,
				unsigned int border_index
			) const;

			float2 GetDockspaceRegionVerticalRenderSliderScale(
				const UIDockspace* dockspace,
				unsigned int border_index,
				float dockspace_mask
			) const;

			float2 GetDockspaceRegionVerticalRenderSliderScale(
				float2 region_scale, 
				const UIDockspace* dockspace,
				unsigned int border_index,
				unsigned int window_index
			) const;

			UIDockspace* GetDockspace(unsigned int dockspace_index, DockspaceType type);

			const UIDockspace* GetDockspace(unsigned int dockspace_index, DockspaceType type) const;

			const UIDockspace* GetConstDockspace(UIDockspaceLayer layer) const;

			UIDrawerDescriptor GetDrawerDescriptor(unsigned int window_index);

			// Returns the data for the drag drop handler. It asserts that it exists
			void* GetDragDropData(Stream<char> name) const;

			void* GetFrameHandlerData(unsigned int index) const;

			unsigned int GetFrameHandlerCount() const;

			unsigned int GetLastClickableIndex(const UIDockspace* dockspace, unsigned int border_index, ECS_MOUSE_BUTTON button_type) const;

			unsigned int GetLastHoverableIndex(const UIDockspace* dockspace, unsigned int border_index) const;

			unsigned int GetLastGeneralIndex(const UIDockspace* dockspace, unsigned int border_index) const;

			void* GetLastClickableData(const UIDockspace* dockspace, unsigned int border_index, ECS_MOUSE_BUTTON button_type) const;

			void* GetLastHoverableData(const UIDockspace* dockspace, unsigned int border_index) const;

			void* GetLastGeneralData(const UIDockspace* dockspace, unsigned int border_index) const;

			void GetFixedDockspaceRegionsFromMouse(
				float2 mouse_position,
				UIDockspace** dockspaces,
				DockspaceType* types,
				unsigned int& count
			) const;

			ActionData GetFilledActionData(unsigned int window_index);

			size_t GetFrameIndex() const;

			float2 GetNormalizeMousePosition() const;

			float2 GetSquareScale(float y_scale) const;
			
			ECS_INLINE float GetTitleYSize() const {
				return m_descriptors.title_y_scale;
			}

			unsigned int GetWindowFromMouse(float2 mouse_position) const;

			unsigned int GetWindowFromDockspace(float2 mouse_position, unsigned int dockspace_index, DockspaceType type) const;

			unsigned int GetWindowFromName(Stream<char> name) const;

			WindowTable* GetWindowTable(unsigned int window_index);

			Stream<char> GetWindowName(unsigned int window_index) const;

			UIWindow* GetWindowPointer(unsigned int window_index);

			UIDefaultWindowHandler* GetDefaultWindowHandlerData(unsigned int window_index) const;

			void* GetWindowPrivateHandlerData(unsigned int window_index) const;

			void* GetWindowData(unsigned int window_index) const;

			unsigned int GetActiveWindowIndexInBorder(const UIDockspace* dockspace, unsigned int border_index) const;

			// Returns the index of the active window
			unsigned int GetActiveWindow() const;

			unsigned int GetWindowIndexFromBorder(const UIDockspace* dockspace, unsigned int border_index) const;

			float2 GetWindowPosition(unsigned int window_index) const;

			float2 GetWindowScale(unsigned int window_index) const;

			float2 GetWindowRenderRegion(unsigned int window_index) const;
			
			float4 GetUVForCharacter(char character) const;

			uint2 GetWindowTexelSize(unsigned int window_index) const;

			// Returns the texel offset from the beginning of the given window
			// If the position falls outside the bounds, it will return { -1, -1 }
			uint2 GetWindowTexelPosition(unsigned int window_index, float2 position) const;

			// Returns the texel offset from the beginning of the given window
			// If the position falls outside the bounds, it will return the offset as it is 
			int2 GetWindowTexelPositionEx(unsigned int window_index, float2 position) const;

			// Returns the texel offset from the beginning of the given window
			// If the position falls outside the bounds, it will be clamped to a border/corner
			uint2 GetWindowTexelPositionClamped(unsigned int window_index, float2 position) const;

			// Returns the pixel offset from the beginning of the currently hovered window
			uint2 GetMousePositionHoveredWindowTexelPosition() const;

			// Advances the next sprite texture
			UISpriteTexture* GetNextSpriteTextureToDraw(UIDockspace* dockspace, unsigned int border_index, ECS_UI_DRAW_PHASE phase, ECS_UI_SPRITE_TYPE type);

			// Returns the index of the dynamic element, -1 if it doesn't find it
			unsigned int GetWindowDynamicElement(unsigned int window_index, Stream<char> identifier) const;

			UIWindowDynamicResource* GetWindowDynamicElement(unsigned int window_index, unsigned int index);

			// Returns the duration in microseconds since the last frame
			float GetFrameDeltaTime() const;

			float2 GetPreviousMousePosition() const;

			// It will call all the drag handlers that want to be notified when they exit a region
			void HandleDragExitRegions();

			void HandleHoverable(float2 mouse_position, void** buffers, size_t* counts);

			void HandleFocusedWindowClickable(float2 mouse_position, ECS_MOUSE_BUTTON button_type);

			void HandleFocusedWindowGeneral(float2 mouse_position);

			void HandleFocusedWindowCleanupGeneral(
				float2 mouse_position, 
				void* additional_data = nullptr
			);

			void HandleFocusedWindowCleanupHoverable(
				float2 mouse_position,
				void* additional_data = nullptr
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

			void HandleFrameHandlers();

			void IncrementWindowDynamicResource(unsigned int window_index, Stream<char> name);

			void InitializeDefaultDescriptors();

			void InitializeMaterials();

			UIDrawerDescriptor InitializeDrawerDescriptorReferences(unsigned int window_index) const;

			void InitializeWindowDraw(unsigned int index, WindowDraw initialize);

			void InitializeWindowDraw(Stream<char> window_name, WindowDraw initialize);

			void InitializeWindowDefaultHandler(size_t window_index, const UIWindowDescriptor& descriptor);

			bool IsEmptyFixedDockspace(const UIDockspace* dockspace) const;

			bool IsFixedDockspace(const UIDockspace* dockspace) const;

			bool IsFixedDockspace(const UIDockspaceBorder* border) const;

			bool IsBackgroundDockspace(const UIDockspace* dockspace) const;

			bool IsBackgroundDockspace(const UIDockspaceBorder* border) const;

			// Returns whether or not the window is currently drawing
			bool IsWindowDrawing(unsigned int window_index) const;

			// Returns whether or not the viewer can see this window or not
			bool IsWindowVisible(unsigned int window_index) const;
			
			bool LoadUIFile(Stream<wchar_t> filename, CapacityStream<Stream<char>>& window_names);

			// returns whether or not a valid file was found
			bool LoadDescriptorFile(Stream<wchar_t> filename);

			void MoveDockspaceBorder(UIDockspaceBorder* border, unsigned int border_index, float delta_x, float delta_y);

			void MoveDockspaceBorder(UIDockspaceBorder* border, unsigned int border_index, float delta_x, float delta_y, bool ignore_checks);

			float NormalizeHorizontalToWindowDimensions(float value) const;

			void NormalizeHorizontalToWindowDimensions(float* values, size_t count) const;

			float NormalizeVerticalToWindowDimensions(float value) const;

			UIActionHandler PeekFrameHandler() const;

			void PopFrameHandler();

			void PopUpFrameHandler(Stream<char> name, bool is_fixed, bool destroy_at_first_click = false, bool is_initialized = true, bool destroy_when_released = false);

			// Move the background dockspaces behind all other dockspaces
			void PushBackgroundDockspace();

			void PushActiveDockspaceRegion(Stream<UIVisibleDockspaceRegion>& regions) const;

			void PushFrameHandler(UIActionHandler handler);

			void PushDestroyWindowHandler(unsigned int window_index);

			void PushDestroyCallbackWindowHandler(unsigned int window_index, UIActionHandler handler);

			void ReaddHoverableToDockspaceRegion(UIDockspace* dockspace, unsigned int border_index, unsigned int handler_index);

			void ReaddClickableToDockspaceRegion(UIDockspace* dockspace, unsigned int border_index, unsigned int handler_index, ECS_MOUSE_BUTTON button_type);

			void ReaddGeneralToDockspaceRegion(UIDockspace* dockspace, unsigned int border_index, unsigned int handler_index);

			void ReadFontDescriptionFile(Stream<wchar_t> filename);

			void RegisterFocusedWindowClickableAction(
				float2 position, 
				float2 scale, 
				Action action, 
				const void* data, 
				size_t data_size,
				ECS_UI_DRAW_PHASE phase,
				ECS_MOUSE_BUTTON button_type,
				UIHandlerCopyBuffers copy_function = nullptr
			);

			void RegisterVertexShaderAndLayout(wchar_t* filename);

			void RegisterVertexShadersAndLayouts();

			void RegisterPixelShader(wchar_t* filename);

			void RegisterPixelShaders();

			void RegisterSamplers();

			void RegisterWindowResource(size_t window_index, void* data, const void* identifier, unsigned int identifier_size);

			void ReleaseWindowResource(size_t window_index, const void* identifier, unsigned int identifier_size);

			void RemoveDockspace(unsigned int dockspace_index, DockspaceType dockspace_type);

			void RemoveDockspace(unsigned int dockspace_index, CapacityStream<UIDockspace>& dockspace_buffer);

			void RemoveWindowMemoryResource(unsigned int window_index, const void* buffer);

			void RemoveWindowMemoryResource(unsigned int window_index, unsigned int buffer_index);

			void RemoveWindowDynamicResource(unsigned int window_index, Stream<char> buffer);

			void RemoveWindowDynamicResource(unsigned int window_index, unsigned int index);

			// If the dynamic index is -1 it does nothing. If it is different from -1, it will remove it from the dynamic resource
			// It also deallocates the buffer
			void RemoveWindowBufferFromAll(unsigned int window_index, const void* buffer, unsigned int dynamic_index = -1);

			// Returns true if it found it, else false
			bool RemoveWindowDynamicResourceAllocation(unsigned int window_index, unsigned int index, const void* buffer);

			// Return true if it found it, else false
			bool RemoveWindowDynamicResourceTableResource(unsigned int window_index, unsigned int index, ResourceIdentifier identifier);

			void ReplaceWindowMemoryResource(unsigned int window_index, const void* old_buffer, const void* new_buffer);

			void ReplaceWindowDynamicResourceAllocation(unsigned int window_index, unsigned int index, const void* old_buffer, void* new_buffer);

			// If the dynamic index is -1 it does nothing. If it is different from -1, it will remove it from the dynamic resource
			// It also deallocates the old buffer
			void ReplaceWindowBufferFromAll(unsigned int window_index, const void* old_buffer, const void* new_buffer, unsigned int dynamic_index = -1);

			void ReplaceWindowDynamicResourceTableResource(unsigned int window_index, unsigned int index, ResourceIdentifier old_identifier, ResourceIdentifier new_identifier);

			void RemoveUnrestoredWindows();

			// removes the last hoverable handler, but it does not free the memory from the temp allocator
			void RemoveHoverableHandler(UIDockspace* dockspace, unsigned int border_index);

			// removes the last clickable handler, but it does not free the memory from the temp allocator
			void RemoveClickableHandler(UIDockspace* dockspace, unsigned int border_index, ECS_MOUSE_BUTTON button_type);

			// removes the last general handler, but it does not free the memory from the temp allocator
			void RemoveGeneralHandler(UIDockspace* dockspace, unsigned int border_index);

			void RemoveFrameHandler(Action action, void* handler_data);

			void RemoveFrameHandler(unsigned int index);

			void RemoveGlobalResource(Stream<char> name);

			// Returns true if there is such an exit handler, else false
			bool RemoveDragExitHandler(Stream<char> name);
			
			// removes the last sprite texture written
			void RemoveSpriteTexture(UIDockspace* dockspace, unsigned int border_index, ECS_UI_DRAW_PHASE phase, ECS_UI_SPRITE_TYPE type = ECS_UI_SPRITE_NORMAL);

			UIReservedHandler ReserveHoverable(UIDockspace* dockspace, unsigned int border_index, ECS_UI_DRAW_PHASE phase);

			UIReservedHandler ReserveClickable(
				UIDockspace* dockspace, 
				unsigned int border_index, 
				ECS_UI_DRAW_PHASE phase, 
				ECS_MOUSE_BUTTON button_type
			);

			UIReservedHandler ReserveGeneral(UIDockspace* dockspace, unsigned int border_index, ECS_UI_DRAW_PHASE);

			// returns whether or not the parent can continue resizing
			bool ResizeDockspace(
				unsigned int dockspace_index,
				float delta_scale,
				ECS_UI_BORDER_TYPE border,
				DockspaceType dockspace_type
			);

			void ResizeDockspaceUnguarded(
				unsigned int dockspace_index,
				float delta_scale,
				ECS_UI_BORDER_TYPE border,
				DockspaceType dockspace_type
			);

			// This function is used to restore window actions and data after UI file load
			void RestoreWindow(Stream<char> window_name, const UIWindowDescriptor& descriptor);

			template<bool destroy_dockspace_if_last = true>
			ECSENGINE_API void RemoveWindowFromDockspaceRegion(UIDockspace* dockspace, DockspaceType type, unsigned int border_index, unsigned int in_border_index);

			// It will infer what dockspace, border_index and dockspace type it has and the remove it from the border
			// It does nothing if the window dockspace and border index could not be found
			template<bool destroy_dockspace_if_last = true>
			ECSENGINE_API void RemoveWindowFromDockspaceRegion(unsigned int window_index);

			// The last boolean tells the system if to keep the fixed dockspace even if it doesn't currently hold any windows
			// Useful for pop up windows
			template<bool destroy_windows = true>
			ECSENGINE_API void RemoveDockspaceBorder(UIDockspace* dockspace, unsigned int border_index, DockspaceType type);

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
				CapacityStream<unsigned int>* parent_indices,
				CapacityStream<DockspaceType>* border_mask_index,
				unsigned int& parent_count
			);

			void SetActiveWindow(unsigned int index);

			void SetActiveWindow(Stream<char> name);

			void SetActiveWindowInBorder(unsigned int index);

			void SetActiveWindowInBorder(Stream<char> name);

			void SetCleanupGeneralHandler();

			void SetCleanupHoverableHandler();

			void SetTextureEvictionCount(unsigned int frame_count);

			void SetColorTheme(Color color);

			void SetNewFocusedDockspace(UIDockspace* dockspace, DockspaceType type);

			void SetNewFocusedDockspaceRegion(UIDockspace* dockspace, unsigned int border_index, DockspaceType type);

			void SearchAndSetNewFocusedDockspaceRegion(UIDockspace* dockspace, unsigned int border_index, DockspaceType type);

			// immediate context
			void SetSolidColorRenderState();

			void SetSolidColorRenderState(GraphicsContext* context);

			// immediate context
			void SetTextSpriteRenderState();

			void SetTextSpriteRenderState(GraphicsContext* context);

			// immediate context
			void SetSpriteRenderState();

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
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// capable of handling rotated sprites
			void SetSprite(
				UIDockspace* dockspace,
				unsigned int border_index,
				Stream<wchar_t> texture,
				float2 position,
				float2 scale,
				void** buffers,
				size_t* counts,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = {0.0f, 0.0f},
				float2 bottom_right_uv = {1.0f, 1.0f},
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// capable of handling rotated sprites
			void SetVertexColorSprite(
				UIDockspace* dockspace,
				unsigned int border_index,
				Stream<wchar_t> texture,
				float2 position,
				float2 scale,
				void** buffers,
				size_t* counts,
				const Color* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// capable of handling rotated sprites
			void SetVertexColorSprite(
				UIDockspace* dockspace,
				unsigned int border_index,
				Stream<wchar_t> texture,
				float2 position,
				float2 scale,
				void** buffers,
				size_t* counts,
				const ColorFloat* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// it multiplies by 6 the count
			void SetSpriteCluster(
				UIDockspace* dockspace,
				unsigned int border_index,
				Stream<wchar_t> texture,
				unsigned int count,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			void SetSpriteTextureToDraw(
				UIDockspace* dockspace,
				unsigned int border_index,
				Stream<wchar_t> texture,
				ECS_UI_SPRITE_TYPE type = ECS_UI_SPRITE_NORMAL,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// This method is used for sprites that are being loaded outside of the UISystem
			void SetSpriteTextureToDraw(
				UIDockspace* dockspace,
				unsigned int border_index,
				UISpriteTexture texture,
				ECS_UI_SPRITE_TYPE type = ECS_UI_SPRITE_NORMAL,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			void SetPopUpWindowPosition(unsigned int window_index, float2 new_position);

			void SetPopUpWindowScale(unsigned int window_index, float2 new_scale);

			void SetDockspacePosition(UIDockspace* dockspace, float2 new_position);

			// primarly intended for use with pop up dockspaces, otherwise use resize dockspace
			void SetDockspaceScale(UIDockspace* dockspace, float2 new_scale);

			void SetViewport(float2 position, float2 scale, GraphicsContext* context);

			void SetViewport(GraphicsContext* context, float2 position, float2 half_scale);

			void SetWindowActions(unsigned int index, const UIWindowDescriptor& descriptor);

			void SetWindowActions(Stream<char> name, const UIWindowDescriptor& descriptor);

			void SetWindowDestroyAction(unsigned int index, UIActionHandler handler);

			void SetWindowDestroyAction(Stream<char> name, UIActionHandler handler);

			void SetWindowPrivateAction(unsigned int index, UIActionHandler handler);

			void SetWindowPrivateAction(Stream<char> name, UIActionHandler handler);

			void SetWindowRetainedFunction(unsigned int index, WindowRetainedMode retained_mode);

			void SetWindowRetainedFunction(Stream<char> name, WindowRetainedMode retained_mode);

			void SetWindowName(unsigned int window_index, Stream<char> name);

			void SetWindowMaxZoom(unsigned int window_index, float max_zoom);

			void SetWindowMinZoom(unsigned int window_index, float min_zoom);

			void SetWindowDrawerDifferenceSpan(unsigned int window_index, float2 span);

			// The monitor size must be supplied as well
			void SetWindowOSSize(uint2 new_size, uint2 monitor_size);

			// The position is relative to the window position
			void SetCursorPosition(uint2 position);

			// Used for inter window communication
			// The interested elements need to use AcquireDragDrop() to be notified if they received something
			// The handler will be called if a region is interested in this drag
			// The action receives in the additional_data parameter a Stream<char>* with the name of the region
			// If the trigger on hover is set to true, then the callback will be called while hovering as well
			// To determine whether or not it is released, check the mouse click status
			// You can also have the callback be triggered when the drag exists a region by setting the last parameter
			// To true. In that case, the region_name in tha additional data will be empty
			// Returns the allocated data (in case the data size is different from 0)
			void* StartDragDrop(UIActionHandler handler, Stream<char> name, bool trigger_on_hover = false, bool trigger_on_region_exit = false);

			void TranslateDockspace(UIDockspace* dockspace, float2 translation);

			LinearAllocator* TemporaryAllocator(ECS_UI_DRAW_PHASE phase);

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
					ResizeDockspace(dockspace_index, scale.x - dockspace->transform.scale.x, ECS_UI_BORDER_RIGHT, type);
				}
				else {
					dockspace->transform.scale.x = scale.x;
					ResizeDockspace(dockspace_index, scale.y - dockspace->transform.scale.y, ECS_UI_BORDER_BOTTOM, type);
				}

				if constexpr (~flags & UI_TRIM_POP_UP_WINDOW_NO_HORIZONTAL_LEFT) {
					if (position.x < parent_position.x) {
						float difference = parent_position.x - position.x;
						dockspace->transform.position.x = parent_position.x;
						difference = difference > dockspace->transform.scale.x ? dockspace->transform.scale.x : difference;
						if (type == DockspaceType::FloatingHorizontal) {
							ResizeDockspace(dockspace_index, -difference, ECS_UI_BORDER_RIGHT, type);
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
						difference = difference > dockspace->transform.scale.x ? dockspace->transform.scale.x : difference;
						if (type == DockspaceType::FloatingHorizontal) {
							ResizeDockspace(dockspace_index, -difference, ECS_UI_BORDER_RIGHT, type);
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
						difference = difference > dockspace->transform.scale.y ? dockspace->transform.scale.y : difference;
						if (type == DockspaceType::FloatingHorizontal) {
							dockspace->transform.scale.y -= difference;
						}
						else {
							ResizeDockspace(dockspace_index, -difference, ECS_UI_BORDER_BOTTOM, type);
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
						difference = difference > dockspace->transform.scale.y ? dockspace->transform.scale.y : difference;
						if (type == DockspaceType::FloatingHorizontal) {
							dockspace->transform.scale.y -= difference;
						}
						else {
							ResizeDockspace(dockspace_index, -difference, ECS_UI_BORDER_BOTTOM, type);
						}
					}
				}
			}

			void UpdateDockspaceHierarchy();

			void UpdateDockspace(unsigned int dockspace_index, DockspaceType dockspace_type);

			void UpdateFocusedWindowCleanupLocation();

			bool WriteDescriptorsFile(Stream<wchar_t> filename) const;

			void WriteReservedDefaultClickable(
				UIReservedHandler hoverable_reservation, 
				UIReservedHandler clickable_reservation, 
				const UISystemDefaultClickableData& data
			);

			void WriteReservedDefaultHoverableClickable(
				UIReservedHandler hoverable_reservation, 
				UIReservedHandler clickable_reservation, 
				const UISystemDefaultHoverableClickableData& data
			);

			bool WriteUIFile(Stream<wchar_t> filename, CapacityStream<char>& error_message) const;

			//private:
			Application* m_application;
			UIToolsAllocator* m_memory;
			Keyboard* m_keyboard;
			Mouse* m_mouse;
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
			UIRenderResources m_resources;
			// The intended purpose of the global resources is to ease inter-window communication
			// For example draggables across windows
			UIGlobalResources m_global_resources;
			// This is an array with all the drag regions that want to be notified
			// When they exit a region.
			ResizableStream<UISystemDragExitRegion> m_drag_exit_regions;
			// This is used to set the default handler for windows
			UIActionHandler m_window_handler;
			size_t m_frame_index;
			// Useful for calculating the normalized mouse value when scaling is being applied
			// at the OS level and try to remedy it
			uint2 m_window_os_size;
			// Used to know when the aspect ratio is changed and how to modify the descriptor dimensions
			uint2 m_monitor_size;
			// This is a factor that is used to know on which dimension the aspect ratio factor has been
			// Applied. It strives to maintain a dimension at 1.0f and let the other change (as it should be the case)
			float2 m_aspect_ratio_factor;
			// Describes how large a pixel is. Basically, 2.0f / m_window_os_size;
			float2 m_pixel_size;
			Timer m_snapshot_mode_timer;
			// Record the duration here such that it is consistent for the entire frame
			unsigned int m_snapshot_mode_elapsed_time;
			// This is used to make the allocations for the snapshots
			// Using Malloc can produce really noticeable slowdowns
			GlobalMemoryManager m_snapshot_allocator;
			// This is used to keep track of the frame delta time
			Timer m_frame_timer;
			float m_frame_delta_time;
			void* m_event_data;
			ECS_UI_FRAME_PACING m_frame_pacing;
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
				const Mouse*,
				const Keyboard*
			);
			UIFocusedWindowData m_focused_window_data;
			// These handlers will be called every frame - unlike the handler stack on which only the top most handler is called
			CapacityStream<UIActionHandler> m_frame_handlers;
		};

		void DrawDockspaceRegionThreadTask(unsigned int thread_index, World* world, void* data);

#pragma region Actions

		// ----------------------------------------------------- Actions ------------------------------------------------------

		ECSENGINE_API void PopUpWindowSystemHandler(ActionData* action_data);

		ECSENGINE_API void SkipAction(ActionData* action_data);

		ECSENGINE_API void CloseXAction(ActionData* action_data);

		ECSENGINE_API void CloseXBorderClickableAction(ActionData* action_data);

		ECSENGINE_API void CloseXBorderHoverableAction(ActionData* action_data);

		ECSENGINE_API void DestroyCurrentActionWindow(ActionData* action_data);

		ECSENGINE_API void CollapseTriangleClickableAction(ActionData* action_data);

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

		ECSENGINE_API void TextTooltipAlignTextToRight(Stream<char>& current_text, Stream<char> new_text, size_t total_characters);

		ECSENGINE_API void ReleaseLockedWindow(ActionData* action_data);

		ECSENGINE_API void ConvertASCIIToWideAction(ActionData* action_data);

		ECSENGINE_API void ConvertASCIIToWideStreamAction(ActionData* action_data);

		ECSENGINE_API void DefaultHoverableWithToolTipAction(ActionData* action_data);

		// It will destroy the window and then pop itself from the system handler stack
		ECSENGINE_API void DestroyWindowSystemHandler(ActionData* action_data);

		// It will call the callback and then destroy the window and then pop itself from the system handler stack
		ECSENGINE_API void DestroyWindowCallbackSystemHandler(ActionData* action_data);

		ECSENGINE_API void DragDockspaceAction(ActionData* action_data);

		ECSENGINE_API void RegionHeaderAction(ActionData* action_data);

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
			const Mouse* mouse,
			const Keyboard* keyboard
		);

		void ResizeDockspaceEvent(
			UISystem* system,
			void* parameter,
			void** buffers,
			size_t* counts,
			float normalized_mouse_x,
			float normalized_mouse_y,
			const Mouse* mouse,
			const Keyboard* keyboard
		);

		void HoverOuterDockspaceBorderEvent(
			UISystem* system,
			void* parameter,
			void** buffers,
			size_t* counts,
			float normalized_mouse_x,
			float normalized_mouse_y,
			const Mouse* mouse,
			const Keyboard* keyboard
		);

		void HoverInnerDockspaceBorderEvent(
			UISystem* system,
			void* parameter,
			void** buffers,
			size_t* counts,
			float normalized_mouse_x,
			float normalized_mouse_y,
			const Mouse* mouse,
			const Keyboard* keyboard
		);

		ECSENGINE_API void SkipEvent(
			UISystem* system,
			void* parameter,
			void** buffers,
			size_t* counts,
			float normalized_mouse_x,
			float normalized_mouse_y,
			const Mouse* mouse,
			const Keyboard* keyboard
		);

#pragma endregion
	
	}

}


