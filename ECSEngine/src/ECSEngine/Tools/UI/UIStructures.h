#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Containers/HashTable.h"
#include "../../Containers/Stacks.h"
#include "../../Allocators/MemoryArena.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Allocators/LinearAllocator.h"
#include "../../Allocators/ResizableLinearAllocator.h"
#include "../../Rendering/RenderingStructures.h"
#include "UIMacros.h"
#include "../../Application.h"
#include "../../Utilities/Timer.h"
#include "../../Input/Mouse.h"
#include "../../Input/Keyboard.h"
#include "../../Rendering/Graphics.h"
#include "../../Allocators/MemoryManager.h"

namespace ECSEngine {

	namespace Tools {

		typedef ECSEngine::ResizableMemoryArena UIToolsAllocator;

		struct UISystem;
		struct UIDockspace;

		template<typename T>
		using UIDynamicStream = ResizableStream<T>;

		enum DockspaceType : unsigned char {
			Horizontal,
			Vertical,
			FloatingHorizontal,
			FloatingVertical
		};

		enum ECS_UI_DRAW_PHASE : unsigned char {
			ECS_UI_DRAW_NORMAL,
			ECS_UI_DRAW_LATE,
			ECS_UI_DRAW_SYSTEM
		};
		
		enum ECS_UI_SPRITE_TYPE : unsigned char {
			ECS_UI_SPRITE_NORMAL,
			ECS_UI_SPRITE_CLUSTER
		};

		// additional data is primarly intended for general actions which depend on previous information or next one
		// Or for callbacks to receive extra data
		struct ActionData {
			UISystem* system;
			UIDockspace* dockspace;
			unsigned int border_index;
			unsigned int window_index;
			DockspaceType type;
			// This is used for snapshots - callbacks can set this to true
			// To informt the UISystem to discard the existing window snapshot, if there is one
			bool redraw_window;
			float2 mouse_position;
			float2 position;
			float2 scale;
			void* data;
			void* additional_data;
			size_t* counts;
			void** buffers;
			Keyboard* keyboard;
			Mouse* mouse;
		};

		struct UIDrawerDescriptor;

		struct WindowRetainedModeInfo;

		typedef void (*Action)(ActionData* action_data);
		typedef void (*WindowDraw)(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initializer);
		// This function is used to ask the window if it wants to redraw itself or not
		// This is optional - if not specified, it will be drawn according to the normal rules
		// (that is, every frame the window is drawn unless you specify to do a selective frame drawing)
		// Should return true in order to stay in retained mode, else false when to redraw
		typedef bool (*WindowRetainedMode)(void* window_data, WindowRetainedModeInfo* info);
		typedef void (*UIDrawerElementDraw)(void* element_data, void* drawer_ptr);

		// This is a function that can be provided to be called when an action is selected
		// Such that temporary buffers can be copied over to an allocator tied to the action selection lifetime
		typedef void (*UIHandlerCopyBuffers)(void* data, AllocatorPolymorphic allocator);

		// data size 0 will be interpreted as take data as a pointer, with no data copy
		struct UIActionHandler {
			Action action;
			void* data;
			unsigned int data_size = 0;
			ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
		};

#pragma region Vertex types

		struct UISpriteVertex {
			ECS_INLINE UISpriteVertex() : position(0.0f, 0.0f), uvs(0.0f, 0.0f) {}
			ECS_INLINE UISpriteVertex(float position_x, float position_y, float uv_u, float uv_v, Color _color) : position(position_x, position_y), uvs(uv_u, uv_v), color(_color) {}
			ECS_INLINE UISpriteVertex(float2 _position, float2 _uvs, Color _color) : position(_position), uvs(_uvs), color(_color) {}

			UISpriteVertex(const UISpriteVertex& other) = default;
			UISpriteVertex& operator = (const UISpriteVertex& other) = default;

			// It does invert the y axis
			ECS_INLINE void SetTransform(float2 _position) {
				position = { _position.x, -_position.y };
			}

			ECS_INLINE void SetColor(Color _color) {
				color = _color;
			}

			ECS_INLINE void SetUV(float2 _uv) {
				uvs = _uv;
			}

			float2 position;
			float2 uvs;
			Color color;
		};

		struct UIVertexColor {
			ECS_INLINE UIVertexColor() : position(0.0f, 0.0f), color((unsigned char)0, 0, 0, 255) {}
			ECS_INLINE UIVertexColor(float position_x, float position_y, Color _color) : position(position_x, position_y), color(_color) {}
			ECS_INLINE UIVertexColor(float2 _position, Color _color) : position(_position), color(_color) {}

			UIVertexColor(const UIVertexColor& other) = default;
			UIVertexColor& operator = (const UIVertexColor& other) = default;

			// It inverts the y axis
			ECS_INLINE void SetTransform(float2 _position) {
				position = { _position.x, -_position.y };
			}

			ECS_INLINE void SetColor(Color _color) {
				color = _color;
			}

			float2 position;
			Color color;
		};

#pragma endregion

#pragma region UI primitives

		struct UIElementTransform {
			ECS_INLINE UIElementTransform() : position(0.0f, 0.0f), scale(1.0f, 1.0f) {}
			ECS_INLINE UIElementTransform(float position_x, float position_y, float scale_x, float scale_y) : position(position_x, position_y), scale(scale_x, scale_y) {}
			ECS_INLINE UIElementTransform(float2 _position, float2 _scale) : position(_position), scale(_scale) {}

			UIElementTransform(const UIElementTransform& other) = default;
			UIElementTransform& operator = (const UIElementTransform& other) = default;

			float2 position;
			float2 scale;
		};

		typedef ResourceView UISpriteTexture;

#pragma endregion

#pragma region Descriptors

		struct UIMaterialDescriptor {
			ECS_INLINE UIMaterialDescriptor() : count(0), sampler_count(0) {}
			ECS_INLINE UIMaterialDescriptor(
				unsigned int _material_count,
				unsigned int _sampler_count
			) : count(_material_count), sampler_count(_sampler_count) {}

			unsigned int count;
			unsigned int vertex_buffer_count[ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES];
			unsigned int strides[ECS_TOOLS_UI_MATERIALS * ECS_TOOLS_UI_PASSES];
			unsigned int sampler_count;
		};

		struct ECSENGINE_API UIColorThemeDescriptor {
			void SetNewTheme(Color new_theme);

			Color theme;
			Color background;
			Color title;
			Color inactive_title;
			Color borders;
			Color hovered_borders;
			Color text;
			Color unavailable_text;
			Color collapse_triangle;
			Color region_header_x;
			Color region_header_hover_x;
			Color region_header;
			Color docking_gizmo_background;
			Color docking_gizmo_border;
			Color render_sliders_background;
			Color render_sliders_active_part;
			Color hierarchy_drag_node_bar;
			Color graph_line;
			Color graph_hover_line;
			Color graph_sample_circle;
			Color histogram_color;
			Color histogram_hovered_color;
			Color histogram_text_color;
			Color progress_bar;
			float region_header_active_window_factor;
			float darken_hover_factor;
			float slider_lighten_factor;
			float select_text_factor;
			float check_box_factor;
			float darken_inactive_item;
			float alpha_inactive_item;
		};

		struct ECSENGINE_API UIFontDescriptor {
			// The x component must be the current OS window width divided by the monitor's width
			// The y component must be the current OS window height divided by the monitor's height
			void ChangeDimensionRatio(float2 current_ratio, float2 new_ratio);

			float2 size;
			float character_spacing;
			unsigned int texture_dimensions;
			unsigned int symbol_count;
		};

		struct ECSENGINE_API UIDockspaceDescriptor {
			// The x component must be the current OS window width divided by the monitor's width
			// The y component must be the current OS window height divided by the monitor's height
			void ChangeDimensionRatio(float2 current_ratio, float2 new_ratio);

			unsigned int count;
			unsigned int max_border_count;
			unsigned int max_windows_border;
			unsigned int border_default_sprite_texture_count;
			unsigned int border_default_sprite_cache_count;
			unsigned int border_default_hoverable_handler_count;
			unsigned int border_default_left_clickable_handler_count;
			unsigned int border_default_misc_clickable_handler_count;
			unsigned int border_default_general_handler_count;
			float border_margin;
			float border_size;
			float border_minimum_distance;
			float mininum_scale;
			float region_header_spacing;
			float region_header_padding;
			float region_header_added_scale;
			float viewport_padding_x;
			float viewport_padding_y;
			float collapse_triangle_scale;
			float close_x_position_x_left;
			float close_x_position_x_right;
			float close_x_scale_y;
			float close_x_total_x_padding;
		};

		struct ECSENGINE_API UILayoutDescriptor {
			// The x component must be the current OS window width divided by the monitor's width
			// The y component must be the current OS window height divided by the monitor's height
			void ChangeDimensionRatio(float2 current_ratio, float2 new_ratio);

			float default_element_x;
			float default_element_y;
			float element_indentation;
			float next_row_padding;
			float next_row_y_offset;
			float node_indentation;
		};

		struct ECSENGINE_API UIMiscellaneousDescriptor {
			// The x component must be the current OS window width divided by the monitor's width
			// The y component must be the current OS window height divided by the monitor's height
			void ChangeDimensionRatio(float2 current_ratio, float2 new_ratio);

			unsigned short window_count;
			unsigned short window_table_default_count;
			unsigned int system_vertex_buffers[ECS_TOOLS_UI_MATERIALS];
			unsigned int thread_temp_memory;
			unsigned int drawer_temp_memory;
			unsigned int drawer_identifier_memory;
			unsigned int window_handler_revert_command_count;
			unsigned int hierarchy_drag_node_time;
			unsigned int hierarchy_drag_node_hover_drop;
			unsigned int text_input_caret_display_time;
			unsigned int text_input_repeat_time;
			unsigned int text_input_repeat_start_duration;
			unsigned int text_input_coalesce_command;
			unsigned int slider_bring_back_start;
			unsigned int slider_enter_value_duration;
			float slider_bring_back_tick;
			float render_slider_horizontal_size;
			float render_slider_vertical_size;
			float color_input_window_size_x;
			float color_input_window_size_y;
			float rectangle_hierarchy_drag_node_dimension;
			float2 graph_hover_offset;
			float2 histogram_hover_offset;
			float2 tool_tip_padding;
			Color tool_tip_font_color;
			Color tool_tip_unavailable_font_color;
			Color tool_tip_background_color;
			Color tool_tip_border_color;
			Color menu_arrow_color;
			Color menu_unavailable_arrow_color;
			float menu_x_padd;
		};

		struct ECSENGINE_API UIElementDescriptor {
			// The x component must be the current OS window width divided by the monitor's width
			// The y component must be the current OS window height divided by the monitor's height
			void ChangeDimensionRatio(float2 current_ratio, float2 new_ratio);
			
			float2 label_padd;
			float2 slider_shrink;
			float2 slider_length;
			float color_input_padd;
			float combo_box_padding;
			float2 graph_padding;
			float graph_x_axis_space;
			float2 graph_axis_value_line_size;
			float2 graph_axis_bump;
			float graph_reduce_font;
			float graph_sample_circle_size;
			float2 histogram_padding;
			float histogram_bar_min_scale;
			float histogram_bar_spacing;
			float histogram_reduce_font;
			float menu_button_padding;
			float label_list_circle_size;
		};

		enum ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX {
			ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COLOR_THEME,
			ECS_UI_WINDOW_DRAWER_DESCRIPTOR_LAYOUT,
			ECS_UI_WINDOW_DRAWER_DESCRIPTOR_FONT,
			ECS_UI_WINDOW_DRAWER_DESCRIPTOR_ELEMENT,
			ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT
		};

		struct ECSENGINE_API UIWindowDrawerDescriptor {
			void UpdateZoom(float2 before_zoom, float2 current_zoom);

			// The x component must be the current OS window width divided by the monitor's width
			// The y component must be the current OS window height divided by the monitor's height
			void ChangeDimensionRatio(float2 current_ratio, float2 new_ratio);

			bool configured[ECS_UI_WINDOW_DRAWER_DESCRIPTOR_COUNT];
			UIColorThemeDescriptor color_theme;
			UILayoutDescriptor layout;
			UIFontDescriptor font;
			UIElementDescriptor element_descriptor;
		};

		struct ECSENGINE_API UISystemDescriptors {
			// The x component must be the current OS window width divided by the monitor's width
			// The y component must be the current OS window height divided by the monitor's height
			void ChangeDimensionRatio(float2 current_ratio, float2 new_ratio);

			UIMaterialDescriptor materials;
			UIColorThemeDescriptor color_theme;
			UIFontDescriptor font;
			UIDockspaceDescriptor dockspaces;
			UILayoutDescriptor window_layout;
			UIMiscellaneousDescriptor misc;
			UIElementDescriptor element_descriptor;
			float title_y_scale;
		};

#pragma endregion

#pragma region Render Resources

		struct ECSENGINE_API UIDrawResources {
			void Map(void** buffers, GraphicsContext* context);

			void UnmapNormal(GraphicsContext* context);

			void UnmapLate(GraphicsContext* context);

			void UnmapAll(GraphicsContext* context);

			void Unmap(GraphicsContext* context, unsigned int starting_index, unsigned int end_index);

			void ReleaseSpriteTextures();
			void Release(Graphics* graphics);

			CapacityStream<VertexBuffer> buffers;
			CapacityStream<UIDynamicStream<UISpriteTexture>> sprite_textures;
			ConstantBuffer region_viewport_info;
			UIDynamicStream<unsigned int> sprite_cluster_subtreams;
		};

		struct UIRenderResources
		{
			// Used by the normal and late phase handler allocations
			// such that it can be reset in between window renders
			LinearAllocator temp_allocator;
			// System phase allocation are made from this allocator
			LinearAllocator system_temp_allocator;
			CapacityStream<VertexShader> vertex_shaders;
			CapacityStream<PixelShader> pixel_shaders;
			CapacityStream<InputLayout> input_layouts;
			CapacityStream<SamplerState> texture_samplers;
			ResourceView font_texture;
			// Used when a texture cannot be loaded
			ResourceView invalid_texture;
			UIDrawResources system_draw;
			Semaphore texture_semaphore;
			SpinLock texture_spinlock;
		};

		typedef HashTableDefault<Stream<void>> UIGlobalResources;

		struct ECSENGINE_API UIHandler {
			bool Add(float _position_x, float _position_y, float _scale_x, float _scale_y, UIActionHandler handler, UIHandlerCopyBuffers copy_function);

			bool Add(float2 position, float2 scale, UIActionHandler handler, UIHandlerCopyBuffers copy_function);

			unsigned int AddResizable(AllocatorPolymorphic allocator, float2 position, float2 scale, UIActionHandler handler, UIHandlerCopyBuffers copy_function);

			// Copies all the data that is currently stored here. If you want to use a coalesced allocation
			// For the action data, specify the pointer and it will give it to you back to know when to deallocate
			UIHandler Copy(AllocatorPolymorphic allocator, void** coalesced_action_data = nullptr) const;

			// You can optionally specify a handler data allocator for the system phase - by default
			// it will use the simple handler_data_allocator
			void CopyData(
				const UIHandler* other, 
				AllocatorPolymorphic buffers_allocator, 
				AllocatorPolymorphic handler_data_allocator,
				AllocatorPolymorphic handler_system_data_allocator = {}
			);

			void Deallocate(AllocatorPolymorphic allocator) const;

			// position, scale, data and late_update are set inside the call
			void Execute(unsigned int action_index, ActionData* data) const;

			ECS_INLINE UIActionHandler* GetLastHandler() const {
				return action + GetLastHandlerIndex();
			}

			ECS_INLINE float2 GetPositionFromIndex(unsigned int index) const {
				return { position_x[index], position_y[index] };
			}

			ECS_INLINE float2 GetScaleFromIndex(unsigned int index) const {
				return { scale_x[index], scale_y[index] };
			}

			ECS_INLINE UIActionHandler GetActionFromIndex(unsigned int index) const {
				return action[index];
			}

			ECS_INLINE UIHandlerCopyBuffers GetCopyFunctionFromIndex(unsigned int index) const {
				return copy_function[index];
			}

			ECS_INLINE unsigned int GetLastHandlerIndex() const {
				return position_x.size - 1;
			}

			void Insert(AllocatorPolymorphic allocator, float2 position, float2 scale, UIActionHandler handler, UIHandlerCopyBuffers copy_function, unsigned int insert_index);

			void Resize(AllocatorPolymorphic allocator, size_t new_count);

			unsigned int ReserveOne(AllocatorPolymorphic allocator);

			void Reset();

			// The handler data must be allocated before hand
			void WriteOnIndex(unsigned int index, float2 position, float2 scale, UIActionHandler handler, UIHandlerCopyBuffers copy_function);

			CapacityStream<float> position_x;
			float* position_y;
			float* scale_x;
			float* scale_y;
			UIActionHandler* action;
			UIHandlerCopyBuffers* copy_function;
		};

		struct ECSENGINE_API UIReservedHandler {
			void Write(float2 position, float2 scale, UIActionHandler action_handler, UIHandlerCopyBuffers copy_function);

			void* WrittenBuffer() const;

			LinearAllocator* allocator;
			UIHandler* handler;
			unsigned int index;
		};

#pragma endregion

#pragma region Dockspace

		// The position and the scale in action data are undefined - you need to provide them in the data 
		// Pointer if you need those. Returns a boolean - true if you want to redraw the window, else false
		typedef bool (*UIDockspaceBorderDrawSnapshotRunnableFunction)(void* data, ActionData* action_data);

		// The draw phase is used to give the action data the correct buffers and counts
		struct UIDockspaceBorderDrawSnapshotRunnable {
			UIDockspaceBorderDrawSnapshotRunnableFunction function;
			void* data;
			unsigned int data_size;
			ECS_UI_DRAW_PHASE draw_phase;
		};

		struct UIDockspaceBorderDrawOutputSnapshotCreateInfo {
			const void** buffers;
			const size_t* counts;
			const void** system_buffers;
			const size_t* system_counts;
			const size_t* previous_system_counts;
			Stream<Stream<UISpriteTexture>> border_sprite_textures;
			Stream<Stream<UISpriteTexture>> system_sprite_textures;
			Stream<unsigned int> border_cluster_sprite_count;
			const UIHandler* hoverable_handler;
			Stream<UIHandler> clickable_handlers; // There must be ECS_MOUSE_BUTTON_COUNT
			const UIHandler* general_handler;
			Stream<UIDockspaceBorderDrawSnapshotRunnable> runnables;

			// This can be Malloc as well
			AllocatorPolymorphic allocator;
			// This is used to allocate the runnables
			AllocatorPolymorphic runnable_allocator;
		};

		struct UIDockspaceBorderDrawOutputSnapshotRestoreInfo {
			void** buffers;
			size_t* counts;
			void** system_buffers;
			size_t* system_counts;
			Stream<UIDynamicStream<UISpriteTexture>> border_sprite_textures;
			Stream<UIDynamicStream<UISpriteTexture>> system_sprite_textures;
			ResizableStream<unsigned int>* border_cluster_sprite_count;

			UIHandler* hoverable_handler;
			Stream<UIHandler> clickable_handlers; // There must be ECS_MOUSE_BUTTON_COUNT
			UIHandler* general_handler;
			AllocatorPolymorphic handler_buffer_allocator;
			AllocatorPolymorphic handler_data_allocator;
			AllocatorPolymorphic handler_system_data_allocator;

			// This will be passed to the runnables
			ActionData runnable_data;
		};

		struct ECSENGINE_API UIDockspaceBorderDrawOutputSnapshot {
			ECS_INLINE void InitializeEmpty() {
				ZeroStructure(this);
			}

			// It will override the current contents
			void ConstructFrom(const UIDockspaceBorderDrawOutputSnapshotCreateInfo* info);

			// Can optionally choose not to deallocate the runnables data pointers
			// If that is the case, the runnable allocator is useless
			void Deallocate(AllocatorPolymorphic allocator, AllocatorPolymorphic runnable_allocator, bool deallocate_runnable_data = true);

			bool IsValid() const;

			// Returns true if the region should have the snapshot redrawn
			bool Restore(UIDockspaceBorderDrawOutputSnapshotRestoreInfo* restore_info);

			bool ContainsTexture(UISpriteTexture texture) const;

			// Returns true if the old texture was found and replaced, else false
			bool ReplaceTexture(UISpriteTexture old_texture, UISpriteTexture new_texture);

			// This is the draw state
			struct {
				void* buffers[ECS_TOOLS_UI_MATERIALS * (ECS_TOOLS_UI_PASSES + 1)];
				// Normal draw, late draw, system draw
				size_t counts[ECS_TOOLS_UI_MATERIALS * (ECS_TOOLS_UI_PASSES + 1)];

				Stream<UISpriteTexture> sprites[ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS * (ECS_TOOLS_UI_PASSES + 1)];
				Stream<unsigned int> cluster_sprite_counts[ECS_TOOLS_UI_PASSES + 1];
			};

			// We also need the handler state
			struct {
				UIHandler hoverables;
				UIHandler clickables[ECS_MOUSE_BUTTON_COUNT];
				UIHandler generals;
				// This is where all the action data that needs to be copied will be written
				void* hoverables_action_allocated_data;
				void* clickables_action_allocated_data[ECS_MOUSE_BUTTON_COUNT];
				void* generals_action_allocated_data;
			};

			// The snapshot runnables
			struct {
				Stream<UIDockspaceBorderDrawSnapshotRunnable> runnables;
			};
		};

		// window_indices contain the windows that are held inside that section
		// if it contains a dockspace, index 0 indicates the dockspace held in it
		struct ECSENGINE_API UIDockspaceBorder {
			void DeallocateSnapshot(AllocatorPolymorphic snapshot_allocator, bool free_runnable_allocator);
			
			void Reset();
			
			size_t Serialize(void* buffer) const;
			size_t SerializeSize() const;
			size_t LoadFromFile(const void* buffer);

			float position;
			bool is_dock;
			bool draw_region_header;
			bool draw_close_x;
			bool draw_elements;
			unsigned char active_window;
		private:
			unsigned char padding_bytes[7];
		public:
			CapacityStream<unsigned short> window_indices;
			UIDrawResources draw_resources;
			UIHandler hoverable_handler;
			UIHandler clickable_handler[ECS_MOUSE_BUTTON_COUNT];
			UIHandler general_handler;

			// This is the index inside the window_indices
			unsigned char snapshot_active_window;
			// This is the index inside the array of windows
			// It is used to trigger a snapshot recalculation
			// When the window is changed even when the trigger
			// Period is not reached
			unsigned int snapshot_window_index;
			UIDockspaceBorderDrawOutputSnapshot snapshot;
			ResizableStream<UIDockspaceBorderDrawSnapshotRunnable> snapshot_runnables;
			// From this allocator the allocations for the data for the snapshot_runnables are being made
			// This makes easier allocation of data for routines in the UIDrawer
			ResizableLinearAllocator snapshot_runnable_data_allocator;
			// This is stored in case the window is resized/moved in order to retrigger a redraw
			// Earlier than the lazy evaluation timer
			float2 snapshot_position;
			float2 snapshot_scale;
			// These are used to determine if the window has hidden during the create
			// And the current frame. If it was hidden once, then we need to recompute the snapshot
			// Once the window is visible
			size_t snapshot_frame_create_index;
			size_t snapshot_frame_display_count;
		};

		struct ECSENGINE_API UIDockspace {
			size_t Serialize(void* buffer) const;
			size_t SerializeSize() const;
			// returns the count of bytes read and takes as parameter an index representing the count of borders that is has
			size_t LoadFromFile(const void* buffer, unsigned char& border_count);
			size_t LoadBordersFromFile(const void* buffer);

			UIElementTransform transform;
			CapacityStream<UIDockspaceBorder> borders;
			bool allow_docking;
		};

		struct ECSENGINE_API UIVisibleDockspaceRegion {
			float2 GetPosition();
			float2 GetScale();

			UIDockspace* dockspace;
			DockspaceType type;
			unsigned short border_index;
			unsigned short layer;
		};

		struct UIDockspaceRegion {
			UIDockspace* dockspace;
			unsigned int border_index;
			DockspaceType type;
		};

		struct UIDrawDockspaceRegionData {
			void* system;
			UIDockspace* dockspace;
			unsigned int border_index;
			DockspaceType type;
			bool is_fixed_default_when_border_zero;
			bool snapshot_mode;
			unsigned int snapshot_mode_lazy_update;
			unsigned int draw_index;
			unsigned int last_index;
			unsigned int active_region_index;
			void** system_buffers;
			size_t* system_count;
			UIDockspaceRegion mouse_region;
			float2 mouse_position;
			Semaphore* texture_semaphore;
		};

		enum ECS_UI_BORDER_TYPE {
			ECS_UI_BORDER_BOTTOM = 1 << 0,
			ECS_UI_BORDER_TOP = 1 << 1,
			ECS_UI_BORDER_LEFT = 1 << 2,
			ECS_UI_BORDER_RIGHT = 1 << 3
		};
		
		struct BorderHover {
			ECS_INLINE bool IsTop() {
				return (value & ECS_UI_BORDER_TOP) != 0;
			}

			ECS_INLINE bool IsLeft() {
				return (value & ECS_UI_BORDER_LEFT) != 0;
			}

			ECS_INLINE bool IsRight() {
				return (value & ECS_UI_BORDER_RIGHT) != 0;
			}

			ECS_INLINE bool IsBottom() const {
				return (value & ECS_UI_BORDER_BOTTOM) != 0;
			}

			ECS_INLINE void SetTop() {
				value |= ECS_UI_BORDER_TOP;
			}

			ECS_INLINE void SetBottom() {
				value |= ECS_UI_BORDER_BOTTOM;
			}

			ECS_INLINE void SetLeft() {
				value |= ECS_UI_BORDER_LEFT;
			}

			ECS_INLINE void SetRight() {
				value |= ECS_UI_BORDER_RIGHT;
			}

			unsigned char value;
		};

		struct UIDockspaceLayer {
			ECS_INLINE bool operator == (UIDockspaceLayer other) const {
				return index == other.index && type == other.type;
			}
			
			ECS_INLINE bool operator != (UIDockspaceLayer other) const {
				return !(*this == other);
			}

			unsigned int index;
			DockspaceType type;
		};

#pragma endregion


#pragma region Window

		typedef HashTableDefault<void*> WindowTable;

		struct UIWindowDynamicResource {
			Stream<void*> element_allocations;
			Stream<ResourceIdentifier> table_resources;
			// The resource can add allocations without changing the coalesced allocation
			Stream<void*> added_allocations;
			unsigned int reference_count;
		};

		struct WindowRetainedModeInfo {
			UISystem* system;
			UIDockspace* dockspace;
			unsigned int border_index;
			unsigned int window_index;
		};

		struct ECSENGINE_API UIWindow {
			// it returns the number of bytes written
			size_t Serialize(void* buffer) const;
			size_t SerializeSize() const;
			// the characters will be copied onto the name stack buffer and then the system can safely assign the name
			size_t LoadFromFile(const void* buffer, Stream<char>& name_stack);

			ECS_INLINE Stream<char> DrawableName() const {
				return { name.buffer, name_drawable_count };
			}

			float2 zoom;
			float2 render_region_offset;
			float2 drawer_draw_difference;
			float max_zoom;
			float min_zoom;
			UIElementTransform transform;
			WindowTable table;
			UIWindowDrawerDescriptor* descriptors;
			Stream<char> name;
			void* window_data;
			size_t window_data_size;
			UIDynamicStream<void*> memory_resources;
			HashTableDefault<UIWindowDynamicResource> dynamic_resources;
			WindowDraw draw;
			WindowRetainedMode retained_mode;
			UIActionHandler private_handler;
			UIActionHandler default_handler;
			UIActionHandler destroy_handler;
			bool is_horizontal_render_slider;
			bool is_vertical_render_slider;
			unsigned char pin_horizontal_slider_count;
			unsigned char pin_vertical_slider_count;
			unsigned int name_drawable_count;
		};

		struct UIWindowSerializedMissingData {
			WindowDraw draw;
			Action private_action = nullptr;
			void* private_action_data = nullptr;
			size_t private_action_data_size = 0;
			void* window_data = nullptr;
			size_t window_data_size = 0;
			size_t resource_count = 0;
		};

		// window data size 0 means it does not need to allocate memory, just take the pointer
		// The destroy callback receives the window data in the additional data parameter
		struct ECSENGINE_API UIWindowDescriptor {
			void Center(float2 size);

			unsigned short resource_count = 0;
			float initial_position_x;
			float initial_position_y;
			float initial_size_x;
			float initial_size_y;
			Stream<char> window_name;
			void* window_data = nullptr;
			size_t window_data_size = 0;
			WindowDraw draw = nullptr;
			WindowRetainedMode retained_mode = nullptr;
			Action private_action = nullptr;
			void* private_action_data = nullptr;
			size_t private_action_data_size = 0;
			Action destroy_action = nullptr;
			void* destroy_action_data = nullptr;
			size_t destroy_action_data_size = 0;
		};

#pragma endregion

#pragma region Event data

		struct ECSENGINE_API UIFocusedWindowData {
			// Returns the final data pointer
			void* ChangeHoverableHandler(float2 position, float2 scale, const UIActionHandler* handler, UIHandlerCopyBuffers copy_function);
			// Returns the final data pointer
			void* ChangeHoverableHandler(const UIHandler* handler, unsigned int index);
			
			// Returns the final data pointer
			void* ChangeClickable(float2 position, float2 scale, const UIActionHandler* handler, UIHandlerCopyBuffers copy_function, ECS_MOUSE_BUTTON button_type);
			// Returns the final data pointer
			void* ChangeClickable(const UIHandler* handler, unsigned int index, ECS_MOUSE_BUTTON button_type);

			// Returns the final data pointer
			void* ChangeGeneralHandler(float2 position, float2 scale, const UIActionHandler* handler, UIHandlerCopyBuffers copy_function);
			// Returns the final data pointer
			void* ChangeGeneralHandler(const UIHandler* handler, unsigned int index);

			bool ExecuteHoverableHandler(ActionData* action_data);
			bool ExecuteClickableHandler(ActionData* action_data, ECS_MOUSE_BUTTON button_type);
			bool ExecuteGeneralHandler(ActionData* action_data);

			void ResetHoverableHandler();
			void ResetClickableHandler(ECS_MOUSE_BUTTON button_type);
			void ResetGeneralHandler();

			struct Location {
				UIDockspace* dockspace;
				unsigned int border_index;
				DockspaceType type;
			};

			Location active_location;
			Location hovered_location;

			Location cleanup_hoverable_location;
			Location cleanup_general_location;

			//bool always_hoverable;
			bool clean_up_call_hoverable;
			bool clean_up_call_general;
			unsigned char locked_window;

			void** buffers;
			size_t* counts;

			void* additional_hoverable_data;
			void* additional_general_data;

			UIActionHandler hoverable_handler;
			UIElementTransform hoverable_transform;
			ResizableLinearAllocator hoverable_handler_allocator;

			UIActionHandler general_handler;
			UIElementTransform general_transform;
			ResizableLinearAllocator general_handler_allocator;

			UIActionHandler clickable_handler[ECS_MOUSE_BUTTON_COUNT];
			UIElementTransform mouse_click_transform[ECS_MOUSE_BUTTON_COUNT];
			ResizableLinearAllocator clickable_handler_allocator[ECS_MOUSE_BUTTON_COUNT];
		};

		struct UIMoveDockspaceBorderEventData {
			UIDockspace* dockspace;
			unsigned short border_index;
			DockspaceType type;
			bool move_x;
			bool move_y;
		};

		struct UIResizeEventData {
			unsigned int dockspace_index;
			BorderHover border_hover;
			DockspaceType dockspace_type;
		};

#pragma endregion

#pragma region Action Data Blocks

		struct UIPopUpWindowData {
			bool is_initialized = false;
			bool is_fixed = false;
			bool destroy_at_first_click = false;
			bool destroy_at_release = false;
			bool reset_when_window_is_destroyed = false;
			bool deallocate_name = false;
			Stream<char> name;
			bool* flag_destruction = nullptr;
		};

		struct UIDefaultHoverableData {
			Color colors[ECS_TOOLS_UI_DEFAULT_HOVERABLE_DATA_COUNT];
			float percentages[ECS_TOOLS_UI_DEFAULT_HOVERABLE_DATA_COUNT] = { 1.25f };
			float2 positions[ECS_TOOLS_UI_DEFAULT_HOVERABLE_DATA_COUNT];
			float2 scales[ECS_TOOLS_UI_DEFAULT_HOVERABLE_DATA_COUNT];
			unsigned int count;
			bool is_single_action_parameter_draw = true;
		};

		struct UIDefaultTextHoverableData {
			Color color;
			Color text_color = ECS_TOOLS_UI_TEXT_COLOR;
			float percentage = 1.25f;
			float2 text_offset = { 0.0f, 0.0f };
			float2 font_size = { ECS_TOOLS_UI_FONT_SIZE * ECS_TOOLS_UI_FONT_X_FACTOR, ECS_TOOLS_UI_FONT_SIZE };
			float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING;
			Stream<char> text = { nullptr, 0 };
			bool vertical_text = false;
			bool horizontal_cull = false;
			bool vertical_cull = false;
			float horizontal_cull_bound = 0.0f;
			float vertical_cull_bound = 0.0f;
		};

		struct UIDefaultVertexColorHoverableData {
			Color top_left;
			Color top_right;
			Color bottom_left;
			Color bottom_right;
			float percentage = 1.25f;
		};

		struct UIDefaultClickableData {
			UIActionHandler hoverable_handler;
			UIActionHandler click_handler;
			ECS_UI_DRAW_PHASE initial_clickable_phase;
			ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT;
		};

		// duration is interpreted as milliseconds
		struct ECSENGINE_API UIDoubleClickData {
			bool IsTheSameData(const UIDoubleClickData* other) const;
			
			UIActionHandler double_click_handler;
			UIActionHandler first_click_handler;
			size_t max_duration_between_clicks;
			Timer timer;
			bool is_identifier_int;
			char identifier_char_count;
			unsigned int identifier;
			char identifier_char[64];
		};

		struct UICloseXClickData {
			unsigned int window_in_border_index;
		};

		struct UIRenderRegionActionData {
			float2 position;
			float2 scale;
		};

		struct UITooltipPerRowData {
			UIActionHandler* handlers = nullptr;
			bool is_uniform = false;
		};

		struct UITooltipBaseData {
			bool default_background = true;
			bool default_border = true;
			bool default_font = true;
			bool center_horizontal_x = false;
			bool2 offset_scale = {false, false}; // With this you can offset with the scale of the hoverable region
			Color background_color = Color(0, 0, 0); // Uses default
			Color border_color = Color(0, 0, 0); // Uses default
			float2 border_size = { 0.0f, 0.0f }; // Uses default
			Color font_color = Color(0, 0, 0); // Uses default
			Color unavailable_font_color = Color(0, 0, 0); // Uses default
			float2 font_size = { 0.0f, 0.0f }; // Uses default
			float character_spacing = 0.0f; // Uses default
			float next_row_offset = 0.0f;
			float2 offset = { 0.0f, 0.0f };
			bool* unavailable_rows = nullptr;
			unsigned int* previous_hoverable = nullptr;
		};

		// If the characters pointer is nullptr, it means that the data is relative
		struct UITextTooltipHoverableData {
			unsigned int Write(Stream<char> copy_characters) {
				memcpy(OffsetPointer(this, sizeof(*this)), copy_characters.buffer, copy_characters.size * sizeof(char));
				characters = { nullptr, copy_characters.size };
				return sizeof(*this) + copy_characters.size * sizeof(char);
			}

			Stream<char> GetCharacters() const {
				if (characters.buffer == nullptr) {
					return { OffsetPointer(this, sizeof(*this)), characters.size };
				}
				return characters;
			}

			ECS_INLINE unsigned int WriteSize() const {
				return characters.buffer != nullptr ? sizeof(*this) : sizeof(*this) + characters.size;
			}

			Stream<char> characters;
			UITooltipBaseData base;
		};

		struct ECSENGINE_API UITooltipDrawData {
			void NextRow();
			void Indent();
			void FinalizeRectangle(float2 position, float2 scale);

			void** buffers;
			size_t* counts;
			float2 initial_position;
			float2 position;
			float2 current_scale;
			float2 max_bounds;
			float character_spacing;
			float2 font_size;
			Color font_color;
			float x_padding;
			float indentation;
		};

		typedef void (*TooltipDrawFunction)(void* data, UITooltipDrawData& draw_data);

		struct UITooltipHoverableData {
			UITooltipBaseData base;
			TooltipDrawFunction drawer;
			void* data;
			unsigned int data_size;
		};

		struct UIConvertASCIIToWideData {
			const char* ascii;
			wchar_t* wide;
		};

		struct UIConvertASCIIToWideStreamData {
			const char* ascii;
			CapacityStream<wchar_t>* wide;
		};

		struct UIDefaultHoverableWithTooltipData {
			Color color;
			float percentage = { 1.25f };
			UITextTooltipHoverableData tool_tip_data;
		};

#pragma endregion

#pragma region Function Structs

		struct UISystemDefaultHoverableData {
			unsigned int border_index;
			UIDockspace* dockspace;
			float2 position;
			float2 scale;
			UIDefaultHoverableData data;
			ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
		};

		struct UISystemDefaultClickableData {
			unsigned int border_index;
			UIDockspace* dockspace;
			float2 position;
			float2 scale;
			UIActionHandler hoverable_handler;
			UIActionHandler clickable_handler;
			UIHandlerCopyBuffers hoverable_copy_function = nullptr;
			UIHandlerCopyBuffers clickable_copy_function = nullptr;
			bool disable_system_phase_retarget = false;
			ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT;
		};

		struct UISystemDefaultHoverableClickableData {
			unsigned int border_index;
			UIDockspace* dockspace;
			float2 position;
			float2 scale;
			UIDefaultHoverableData hoverable_data;
			UIActionHandler clickable_handler;
			UIHandlerCopyBuffers clickable_copy_function = nullptr;
			ECS_UI_DRAW_PHASE hoverable_phase = ECS_UI_DRAW_NORMAL;
			bool disable_system_phase_retarget = false;
			ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT;
		};

		struct UISystemDoubleClickData {
			unsigned int border_index;
			UIDockspace* dockspace;
			float2 position;
			float2 scale;
			size_t duration_between_clicks;
			UIActionHandler first_click_handler;
			UIActionHandler second_click_handler;

			// The identifier is used to differentiate between different double click actions
			bool is_identifier_int;
			unsigned char identifier_char_count;
			unsigned int identifier;
			char identifier_char[64];
		};

#pragma endregion

		struct HandlerCommand {
			UIActionHandler handler;
			void* owning_pointer;
		};

		struct ECSENGINE_API UIDefaultWindowHandler {

			// if it wraps around, it returns a copy to the deleted item, the action will be replaced with the pointer 
			// to the data
			HandlerCommand PushRevertCommand(const HandlerCommand& command);

			bool PopRevertCommand(HandlerCommand& command);

			bool PeekRevertCommand(HandlerCommand& command);

			HandlerCommand* GetLastCommand();

			void ChangeLastCommand(const HandlerCommand& command);

			void ChangeCursor(ECS_CURSOR_TYPE cursor);

			void* vertical_slider;
			int scroll;
			float scroll_factor;
			bool is_parameter_window_opened;
			bool is_this_parameter_window;
			bool allow_zoom;
			bool2 draggable_slide;
			ECS_CURSOR_TYPE commit_cursor;
			HandlerCommand last_command;
			unsigned int max_size;
			size_t last_frame;
			// This field is primarly intended to detect when changes to
			// The render offsets are made in order to trigger a redraw for retained
			// Windows since the slider modification lags behind 1 frame with the actual screen
			// Drawn image
			float2 last_window_render_offset;
			Stack<HandlerCommand> revert_commands;
		};

		struct UIDragDockspaceData {
			UIDockspace* floating_dockspace;
			DockspaceType type;
		};

		struct UIRegionHeaderData {
			float2 initial_window_positions[16];
			unsigned int window_index;
			UIDefaultHoverableData hoverable_data;
			UIDragDockspaceData drag_data;
			UIDockspace* dockspace_to_drag;
			DockspaceType type;
		};

		struct DestroyWindowCallbackSystemHandlerData {
			UIActionHandler callback;
			unsigned int window_index;
		};

		struct UISystemDragExitRegion {
			Stream<char> name;
			float2 position;
			float2 scale;
		};

		struct UISystemDragHandler {
			UIActionHandler handler;
			bool trigger_on_hover;
			bool call_on_region_exit;
		};

	}
}
