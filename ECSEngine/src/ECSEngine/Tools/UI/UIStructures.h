#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Containers/HashTable.h"
#include "../../Containers/Stacks.h"
#include "../../Allocators/MemoryArena.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Allocators/LinearAllocator.h"
#include "../../Rendering/Graphics.h"
#include "../../Rendering/RenderingStructures.h"
#include "../../Internal/InternalStructures.h"
#include "UIMacros.h"
#include "../../Application.h"
#include "../../Utilities/Timer.h"
#include "../../Utilities/Mouse.h"
#include "../../Utilities/Keyboard.h"

namespace ECSEngine {

	namespace Tools {

		using UIToolsAllocator = ECSEngine::ResizableMemoryArena;
		using UIHash = ECSEngine::HashFunctionMultiplyString;

		template<typename T>
		using UIDynamicStream = ResizableStream<T>;

		enum class DockspaceType : unsigned char {
			Horizontal,
			Vertical,
			FloatingHorizontal,
			FloatingVertical
		};

		enum class UIDrawPhase : unsigned char {
			Normal,
			Late,
			System
		};

		enum class ECSENGINE_API ActionAdditionalData : unsigned char {
			None,
			Previous,
			Next,
			ReturnToDefault
		};
		
		enum class ECSENGINE_API UISpriteType : unsigned char {
			Normal,
			Cluster
		};

		// additional data is primarly intended for general actions which depend on previous information or next one
		struct ECSENGINE_API ActionData {
			void* system;
			void* dockspace;
			unsigned int border_index;
			float dockspace_mask;
			DockspaceType type;
			float2 mouse_position;
			float2 position;
			float2 scale;
			void* data;
			void* additional_data;
			ActionAdditionalData additional_data_type;
			size_t* counts;
			void** buffers;
			const HID::MouseTracker* mouse_tracker;
			const HID::KeyboardTracker* keyboard_tracker;
			HID::Keyboard* keyboard;
			HID::Mouse* mouse;
			unsigned int thread_id;
		};

		using Action = void (*)(ActionData* action_data);
		using WindowDraw = void (*)(void* window_data, void* drawer_descriptor, bool initializer);
		using UIDrawerElementDraw = void (*)(void* element_data, void* drawer_ptr);

		// data size 0 will be interpreted as take data as a pointer, with no data copy
		struct ECSENGINE_API UIActionHandler {
			Action action;
			void* data;
			unsigned int data_size = 0;
			UIDrawPhase phase = UIDrawPhase::Normal;
		};

#pragma region Vertex types
		struct ECSENGINE_API UISpriteVertex {
			UISpriteVertex();
			UISpriteVertex(float position_x, float position_y, float uv_u, float uv_v, Color color);
			UISpriteVertex(float2 position, float2 uvs, Color color);

			UISpriteVertex(const UISpriteVertex& other) = default;
			UISpriteVertex& operator = (const UISpriteVertex& other) = default;

			// it does not invert the y axis
			void SetTransform(float position_x, float position_y);
			// it does invert the y axis
			void SetTransform(float2 position);
			void SetColor(Color color);
			void SetUV(float2 uv);

			float2 position;
			float2 uvs;
			Color color;
		};

		struct ECSENGINE_API UIVertexColor {
			UIVertexColor();
			UIVertexColor(float position_x, float position_y, Color color);
			UIVertexColor(float2 position, Color color);

			UIVertexColor(const UIVertexColor& other) = default;
			UIVertexColor& operator = (const UIVertexColor& other) = default;

			// it does not inver the y axis
			void SetTransform(float position_x, float position_y);
			// it inverts the y axis
			void SetTransform(float2 position);
			void SetColor(Color color);

			float2 position;
			Color color;
		};

#pragma endregion

#pragma region UI primitives

		struct ECSENGINE_API UIElementTransform {
			UIElementTransform();
			UIElementTransform(float position_x, float position_y, float scale_x, float scale_y);
			UIElementTransform(float2 position, float2 scale);

			UIElementTransform(const UIElementTransform& other) = default;
			UIElementTransform(UIElementTransform&& other) = default;

			UIElementTransform& operator = (const UIElementTransform& other) = default;
			UIElementTransform& operator = (UIElementTransform&& other) = default;

			float2 position;
			float2 scale;
		};

		using UISpriteTexture = ResourceView;

#pragma endregion

#pragma region Descriptors

		struct ECSENGINE_API UIMaterialDescriptor {
			UIMaterialDescriptor() : count(0), sampler_count(0) {}
			UIMaterialDescriptor(
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
			Color default_text;
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
			float size;
			float character_spacing;
			unsigned int texture_dimensions;
			unsigned int symbol_count;
		};

		struct ECSENGINE_API UIDockspaceDescriptor {
			unsigned int count;
			unsigned int max_border_count;
			unsigned int max_windows_border;
			unsigned int border_default_sprite_texture_count;
			unsigned int border_default_sprite_cache_count;
			unsigned int border_default_hoverable_handler_count;
			unsigned int border_default_clickable_handler_count;
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
			float default_element_x;
			float default_element_y;
			float element_indentation;
			float next_row_padding;
			float next_row_y_offset;
			float node_indentation;
		};

		struct ECSENGINE_API UIMiscellaneousDescriptor {
			unsigned short window_count;
			float title_y_scale;
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
			unsigned int text_input_coallesce_command;
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
			float label_horizontal_padd;
			float label_vertical_padd;
			float2 slider_shrink;
			float2 slider_padding;
			float2 slider_length;
			float2 text_input_padding;
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


		enum class UIWindowDrawerDescriptorIndex {
			ColorTheme,
			Layout,
			Font,
			Element,
			Count
		};

		struct ECSENGINE_API UIWindowDrawerDescriptor {
			void UpdateZoom(float2 before_zoom, float2 current_zoom);

			bool configured[(unsigned int)UIWindowDrawerDescriptorIndex::Count];
			UIColorThemeDescriptor color_theme;
			UILayoutDescriptor layout;
			UIFontDescriptor font;
			UIElementDescriptor element_descriptor;
		};

		struct ECSENGINE_API UISystemDescriptors {
			UIMaterialDescriptor materials;
			UIColorThemeDescriptor color_theme;
			UIFontDescriptor font;
			UIDockspaceDescriptor dockspaces;
			UILayoutDescriptor window_layout;
			UIMiscellaneousDescriptor misc;
			UIElementDescriptor element_descriptor;
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

		struct ECSENGINE_API UIRenderThreadResources {
			GraphicsContext* deferred_context;
			LinearAllocator temp_allocator;
			UIDrawPhase phase;
		};

		struct ECSENGINE_API UIRenderResources
		{
			Stream<UIRenderThreadResources> thread_resources;
			CapacityStream<VertexShader> vertex_shaders;
			CapacityStream<PixelShader> pixel_shaders;
			CapacityStream<InputLayout> input_layouts;
			CapacityStream<SamplerState> texture_samplers;
			ResourceView font_texture;
			UIDrawResources system_draw;
			Semaphore texture_semaphore;
			SpinLock texture_spinlock;
		};

		struct ECSENGINE_API UIHandler {

			bool Add(float _position_x, float _position_y, float _scale_x, float _scale_y, UIActionHandler handler);

			bool Add(float2 position, float2 scale, UIActionHandler handler);

			// position, scale, data and late_update are set inside the call
			void Execute(unsigned int action_index, ActionData* data) const;

			void Reset();

			void SetBuffer(void* buffer, size_t count);

			template<typename DataToExpand>
			void WriteExtraArguments(const void** data, const size_t* data_size, size_t count) {
				unsigned int index = position_x.size - 1;
				void* old_data = action[index].data;
				uintptr_t ptr = (uintptr_t)old_data;
				ptr += sizeof(DataToExpand);
				DataToExpand* reinterpretation = (DataToExpand*)old_data;
				for (size_t index = 0; index < count; index++) {
					if (data_size[index] != 0) {
						memcpy((void*)ptr, data[index], data_size[index]);
						reinterpretation->WriteExtraArgument((void*)ptr, data_size[index]);
						ptr += data_size[index];
					}
				}
			}

			static size_t MemoryOf(size_t count);

			CapacityStream<float> position_x;
			float* position_y;
			float* scale_x;
			float* scale_y;
			UIActionHandler* action;
		};

#pragma endregion

#pragma region Dockspace

		// window_indices contain the windows that are held inside that section
		// if it contains a dockspace, index 0 indicates the dockspace held in it
		struct ECSENGINE_API UIDockspaceBorder {
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
			UIHandler clickable_handler;
			UIHandler general_handler;
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
			float offset_mask;
			unsigned short border_index;
			unsigned short layer;
		};

		struct ECSENGINE_API UIDockspaceRegion {
			UIDockspace* dockspace;
			unsigned int border_index;
			DockspaceType type;
		};

		struct ECSENGINE_API UIDrawDockspaceRegionData {
			void* system;
			UIDockspace* dockspace;
			unsigned int border_index;
			float offset_mask;
			DockspaceType type;
			bool is_fixed_default_when_border_zero;
			unsigned int draw_index;
			unsigned int last_index;
			unsigned int thread_id;
			unsigned int active_region_index;
			void** system_buffers;
			size_t* system_count;
			UIDockspaceRegion mouse_region;
			float2 mouse_position;
			Semaphore* texture_semaphore;
		};

		enum class ECSENGINE_API BorderType {
			Top,
			Right,
			Bottom,
			Left
		};

		// bottom mask: 0x01
		// top    mask: 0x02
		// left   mask: 0x04
		// right  mask: 0x08
		struct ECSENGINE_API BorderHover {

			bool IsTop();
			bool IsLeft();
			bool IsRight();
			bool IsBottom();

			void SetTop();
			void SetBottom();
			void SetLeft();
			void SetRight();

			unsigned char value;
		};

		struct ECSENGINE_API UIDockspaceLayer {
			unsigned int index;
			DockspaceType type;
		};

#pragma endregion


#pragma region Window

		using WindowTable = HashTable<void*, ResourceIdentifier, HashFunctionPowerOfTwo, UIHash>;

		struct ECSENGINE_API UIDrawerElementDrawData {
			UIDrawerElementDraw draw;
			void* data;
			UIDynamicStream<void*> element_allocations;
			unsigned int previous_allocation_count;
		};

		struct ECSENGINE_API UIWindowDynamicResource {
			Stream<void*> element_allocations;
			Stream<ResourceIdentifier> table_resources;
			unsigned int reference_count;
		};

		struct ECSENGINE_API UIWindow {
			// it returns the number of bytes written
			size_t Serialize(void* buffer) const;
			size_t SerializeSize() const;
			// the characters will be copied onto the name stack buffer and then the system can safely assign the name
			size_t LoadFromFile(const void* buffer, Stream<char>& name_stack);

			float2 zoom;
			float2 render_region_offset;
			float2 drawer_draw_difference;
			float max_zoom;
			float min_zoom;
			UIElementTransform transform;
			WindowTable table;
			UIWindowDrawerDescriptor* descriptors;
			char* name;
			void* window_data;
			size_t window_data_size;
			Stream<UISpriteVertex> name_vertex_buffer;
			UIDynamicStream<const char*> draw_element_names;
			UIDynamicStream<void*> memory_resources;
			HashTable<UIWindowDynamicResource, ResourceIdentifier, HashFunctionPowerOfTwo, UIHash> dynamic_resources;
			WindowDraw draw;
			UIActionHandler private_handler;
			UIActionHandler default_handler;
			UIActionHandler destroy_handler;
			bool is_horizontal_render_slider;
			bool is_vertical_render_slider;
			unsigned char pin_horizontal_slider_count;
			unsigned char pin_vertical_slider_count;
		};

		struct ECSENGINE_API UIWindowSerializedMissingData {
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
			unsigned short resource_count = 0;
			float initial_position_x;
			float initial_position_y;
			float initial_size_x;
			float initial_size_y;
			const char* window_name;
			void* window_data = nullptr;
			size_t window_data_size = 0;
			WindowDraw draw = nullptr;
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

			void ChangeHoverableHandler(float2 position, float2 scale, const UIActionHandler* handler);
			void ChangeHoverableHandler(UIElementTransform transform, Action action, void* data, size_t data_size, UIDrawPhase phase);
			void ChangeHoverableHandler(float2 position, float2 scale, Action action, void* data, size_t data_size, UIDrawPhase phase);
			void ChangeHoverableHandler(const UIHandler* handler, unsigned int index, void* data);
			void ChangeClickableHandler(float2 position, float2 scale, const UIActionHandler* handler);
			void ChangeClickableHandler(UIElementTransform transform, Action action, void* data, size_t data_size, UIDrawPhase phase);
			void ChangeClickableHandler(float2 position, float2 scale, Action action, void* data, size_t data_size, UIDrawPhase phase);
			void ChangeClickableHandler(const UIHandler* handler, unsigned int index, void* data);
			void ChangeGeneralHandler(float2 position, float2 scale, const UIActionHandler* handler);
			void ChangeGeneralHandler(UIElementTransform transform, Action action, void* data, size_t data_size, UIDrawPhase phase);
			void ChangeGeneralHandler(float2 position, float2 scale, Action action, void* data, size_t data_size, UIDrawPhase phase);
			void ChangeGeneralHandler(const UIHandler* handler, unsigned int index, void* data);

			bool ExecuteHoverableHandler(ActionData* action_data);
			bool ExecuteClickableHandler(ActionData* action_data);
			bool ExecuteGeneralHandler(ActionData* action_data);

			void ResetHoverableHandler();
			void ResetClickableHandler();
			void ResetGeneralHandler();

			UIDockspace* dockspace;
			UIDockspace* hovered_dockspace;
			unsigned short hovered_border_index;
			unsigned short border_index;
			bool always_hoverable;
			bool clean_up_call_general;
			bool clean_up_call_hoverable;
			unsigned char locked_window;
			DockspaceType type;
			DockspaceType hovered_type;
			float mask;
			float hovered_mask;
			void** buffers;
			size_t* counts;
			void* additional_general_data;
			void* additional_hoverable_data;
			ActionAdditionalData additional_general_data_type;
			ActionAdditionalData additional_hoverable_data_type;
			UIActionHandler hoverable_handler;
			UIElementTransform hoverable_transform;
			UIActionHandler clickable_handler;
			UIElementTransform clickable_transform;
			UIActionHandler general_handler;
			UIElementTransform general_transform;
		};

		struct ECSENGINE_API UIMoveDockspaceBorderEventData {
			UIDockspace* dockspace;
			unsigned short border_index;
			DockspaceType type;
			bool move_x;
			bool move_y;
		};

		struct ECSENGINE_API UIResizeEventData {
			unsigned int dockspace_index;
			BorderHover border_hover;
			DockspaceType dockspace_type;
		};

#pragma endregion

#pragma region Action Data Blocks

		struct ECSENGINE_API UIPopUpWindowData {
			bool is_initialized = false;
			bool is_fixed = false;
			bool destroy_at_first_click = false;
			bool destroy_at_release = false;
			bool reset_when_window_is_destroyed = false;
			bool deallocate_name = false;
			const char* name;
			bool* flag_destruction = nullptr;
		};

		struct ECSENGINE_API UIDefaultHoverableData {
			Color colors[ECS_TOOLS_UI_DEFAULT_HOVERABLE_DATA_COUNT];
			float percentages[ECS_TOOLS_UI_DEFAULT_HOVERABLE_DATA_COUNT] = { 1.25f };
			float2 positions[ECS_TOOLS_UI_DEFAULT_HOVERABLE_DATA_COUNT];
			float2 scales[ECS_TOOLS_UI_DEFAULT_HOVERABLE_DATA_COUNT];
			unsigned int count;
			bool is_single_action_parameter_draw = true;
		};

		struct ECSENGINE_API UIDefaultTextHoverableData {
			Color color;
			float percentage = 1.25f;
			UISpriteVertex* vertices = nullptr;
			unsigned int vertex_count = 0;
			float2 text_offset;
			Color text_color = ECS_TOOLS_UI_TEXT_COLOR;
			float2 font_size = { ECS_TOOLS_UI_FONT_SIZE * ECS_TOOLS_UI_FONT_X_FACTOR, ECS_TOOLS_UI_FONT_SIZE };
			float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING;
			const char* text = nullptr;
			bool vertical_text = false;
			bool horizontal_cull = false;
			bool vertical_cull = false;
			float horizontal_cull_bound;
			float vertical_cull_bound;
		};

		struct ECSENGINE_API UIDefaultVertexColorHoverableData {
			Color top_left;
			Color top_right;
			Color bottom_left;
			Color bottom_right;
			float percentage = 1.25f;
		};

		struct ECSENGINE_API UIDefaultClickableData {

			void WriteExtraArgument(void* data, size_t data_size);

			UIActionHandler hoverable_handler;
			UIActionHandler click_handler;
		};

		// duration is interpreted as milliseconds
		struct ECSENGINE_API UIDoubleClickData {
			void WriteExtraArgument(void* data, size_t data_size);
			bool IsTheSameData(const UIDoubleClickData* other) const;
			
			UIActionHandler double_click_handler;
			UIActionHandler first_click_handler;
			size_t max_duration_between_clicks;
			Timer timer;
			unsigned int identifier;
		};

		struct ECSENGINE_API UICloseXClickData {
			unsigned int window_in_border_index;
		};

		struct ECSENGINE_API UIRenderRegionActionData {
			float2 position;
			float2 scale;
		};

		struct ECSENGINE_API UITooltipPerRowData {
			UIActionHandler* handlers = nullptr;
			bool is_uniform = false;
		};

		struct ECSENGINE_API UITooltipBaseData {
			bool default_background = true;
			bool default_border = true;
			bool default_font = true;
			bool2 offset_scale = {false, false};
			Color background_color;
			Color border_color;
			float2 border_size;
			Color font_color;
			Color unavailable_font_color;
			float2 font_size;
			float character_spacing;
			float next_row_offset = 0.0f;
			float2 offset = { 0.0f, 0.0f };
			bool* unavailable_rows = nullptr;
			UITooltipPerRowData basic_draw;
			UITooltipPerRowData hoverable_draw;
			unsigned int* previous_hoverable = nullptr;
		};

		struct ECSENGINE_API UITextTooltipHoverableData {
			const char* characters;
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

		using TooltipDrawFunction = void (*)(void* data, UITooltipDrawData& draw_data);

		struct ECSENGINE_API UITooltipHoverableData {
			UITooltipBaseData base;
			TooltipDrawFunction drawer;
			void* data;
			unsigned int data_size;
		};

		struct ECSENGINE_API UIConvertASCIIToWideData {
			const char* ascii;
			wchar_t* wide;
		};

		struct ECSENGINE_API UIConvertASCIIToWideStreamData {
			const char* ascii;
			CapacityStream<wchar_t>* wide;
		};

		struct ECSENGINE_API UIDefaultHoverableWithTooltipData {
			Color color;
			float percentage = { 1.25f };
			UITextTooltipHoverableData tool_tip_data;
		};

#pragma endregion

#pragma region Function Structs

		struct ECSENGINE_API UISystemDefaultHoverableData {
			unsigned int thread_id;
			UIDockspace* dockspace;
			unsigned int border_index;
			float2 position;
			float2 scale;
			UIDefaultHoverableData data;
			UIDrawPhase phase = UIDrawPhase::Normal;
		};

		struct ECSENGINE_API UISystemDefaultClickableData {
			unsigned int thread_id;
			UIDockspace* dockspace;
			unsigned int border_index;
			float2 position;
			float2 scale;
			UIActionHandler hoverable_handler;
			UIActionHandler clickable_handler;
		};

		struct ECSENGINE_API UISystemDefaultHoverableClickableData {
			unsigned int thread_id;
			UIDockspace* dockspace;
			unsigned int border_index;
			float2 position;
			float2 scale;
			UIDefaultHoverableData hoverable_data;
			UIActionHandler clickable_handler;
			UIDrawPhase hoverable_phase = UIDrawPhase::Normal;
		};

#pragma endregion

		enum class ECSENGINE_API HandlerCommandType {
			TextAdd,
			TextRemove,
			TextReplace
		};

		struct ECSENGINE_API HandlerCommand {
			std::chrono::high_resolution_clock::time_point time;
			UIActionHandler handler;
			HandlerCommandType type;
		};

		struct ECSENGINE_API UIDefaultWindowHandler {

			// if it wraps around, it returns a copy to the deleted item, the action will be replaced with the pointer 
			// to the data
			HandlerCommand PushRevertCommand(const HandlerCommand& command);
			bool PopRevertCommand(HandlerCommand& command);
			bool PeekRevertCommand(HandlerCommand& command);
			HandlerCommand* GetLastCommand();
			void ChangeLastCommand(const HandlerCommand& command);
			void ChangeCursor(CursorType cursor);

			void* vertical_slider;
			int scroll;
			float scroll_factor;
			bool is_parameter_window_opened;
			bool is_this_parameter_window;
			bool allow_zoom;
			bool2 draggable_slide;
			CursorType commit_cursor;
			HandlerCommand last_command;
			unsigned int max_size;
			size_t last_frame;
			Stack<HandlerCommand> revert_commands;
		};

		struct ECSENGINE_API UIDragDockspaceData {
			UIDockspace* floating_dockspace;
			DockspaceType type;
		};

		struct ECSENGINE_API UIRegionHeaderData {
			float2 initial_window_positions[16];
			unsigned int window_index;
			UIDefaultHoverableData hoverable_data;
			UIDragDockspaceData drag_data;
			UIDockspace* dockspace_to_drag;
			DockspaceType type;
		};

		struct ECSENGINE_API DestroyWindowCallbackSystemHandlerData {
			UIActionHandler callback;
			unsigned int window_index;
		};

	}
}
