﻿#pragma once

#define UI_UNPACK_ACTION_DATA	ECSEngine::Tools::UIDockspace* dockspace = action_data->dockspace; \
								unsigned int border_index = action_data->border_index; \
								unsigned int window_index = action_data->window_index; \
								ECSEngine::Tools::UISystem* system = action_data->system; \
								ECSEngine::Tools::DockspaceType dockspace_type = action_data->type; \
								ECSEngine::float2 mouse_position = action_data->mouse_position; \
								ECSEngine::float2 position = action_data->position; \
								ECSEngine::float2 scale = action_data->scale; \
								void* _data = action_data->data; \
								void* _additional_data = action_data->additional_data; \
								void** buffers = action_data->buffers; \
								size_t* counts = action_data->counts; \
								ECSEngine::Keyboard* keyboard = action_data->keyboard; \
								ECSEngine::Mouse* mouse = action_data->mouse; \
								ECSEngine::float2 mouse_delta = system != nullptr ? system->GetMouseDelta(mouse_position) : ECSEngine::float2(0.0f, 0.0f);

#define UI_PREPARE_DRAWER(initializer) ECSEngine::Tools::UIDrawer drawer = ECSEngine::Tools::UIDrawer( \
								*drawer_descriptor, \
								window_data, \
								initializer \
							); 

#define UI_ACTION_IS_THE_SAME_AS_PREVIOUS data->IsTheSameData(additional_data)
#define UI_ACTION_IS_NOT_CLEAN_UP_CALL (buffers != nullptr && counts != nullptr)

#define ECS_TOOLS_UI_RESIZABLE_MEMORY_ARENA

#define ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR '#'
#define ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT 2
#define ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT "##"
#define ECS_TOOLS_UI_CONSTANT_BUFFER_FLOAT_SIZE 4

#define ECS_TOOLS_UI_DESCRIPTOR_FILE_VERSION 1
#define ECS_TOOLS_UI_FILE_VERSION 1
#define ECS_TOOLS_UI_FILE_WINDOW_DESCRIPTOR_VERSION 1

#define ECS_TOOLS_UI_SYSTEM_HANDLER_FRAME_COUNT 8

#define ECS_TOOLS_UI_SINGLE_THREADED

#define ECS_TOOLS_UI_DESIGNED_WIDTH 2560.0f
#define ECS_TOOLS_UI_DESIGNED_HEIGHT 1600.0f

#define ECS_TOOLS_UI_WINDOW_PARAMETER_INITIAL_SIZE_X 0.8f
#define ECS_TOOLS_UI_WINDOW_PARAMETER_INITIAL_SIZE_Y 0.8f

#define ECS_TOOLS_UI_WINDOW_MIN_ZOOM 0.65f
#define ECS_TOOLS_UI_WINDOW_MAX_ZOOM 2.0f

#define ECS_TOOLS_UI_ONE_PIXEL_X 0.0010626f
#define ECS_TOOLS_UI_ONE_PIXEL_Y 0.00175536f

#define ECS_TOOLS_UI_THREAD_TEMP_ALLOCATOR_MEMORY 1'000'000
#define ECS_TOOLS_UI_SNAPSHOT_RUNNABLE_ALLOCATOR_CAPACITY ECS_KB * 32

#define ECS_TOOLS_UI_MISC_DRAWER_IDENTIFIER 250
#define ECS_TOOLS_UI_MISC_DRAWER_TEMP 250
#define ECS_TOOLS_UI_MISC_WINDOW_TABLE_SIZE 256
#define ECS_TOOLS_UI_MISC_TOOL_TIP_PADDING {0.007f, 0.005f}
#define ECS_TOOLS_UI_MISC_DRAWER_ELEMENT_ALLOCATIONS 256
#define ECS_TOOLS_UI_MISC_DRAWER_LATE_ACTION_CAPACITY 32

#define ECS_TOOLS_UI_MISC_RENDER_SLIDER_HORIZONTAL_SIZE 0.031f 
#define ECS_TOOLS_UI_MISC_RENDER_SLIDER_VERTICAL_SIZE 0.018f

#define ECS_TOOLS_UI_WINDOW_RENDER_REGION_SLIDER_X_SCALE 0.01f
#define ECS_TOOLS_UI_WINDOW_TITLE_Y_SCALE 0.04f
#define ECS_TOOLS_UI_MAX_WINDOW_COUNT 32

#define ECS_TOOLS_UI_DOCKSPACE_COUNT 15
#define ECS_TOOLS_UI_DOCKSPACE_MAX_BORDER_COUNT 8
#define ECS_TOOLS_UI_DOCKSPACE_MAX_WINDOWS_PER_BORDER 8
#define ECS_TOOLS_UI_DOCKSPACE_BORDER_MARGIN 0.0185f
#define ECS_TOOLS_UI_DOCKSPACE_BORDER_SIZE 0.0026474f
#define ECS_TOOLS_UI_DOCKSPACE_BORDER_MINIMUM_DISTANCE 0.08f
#define ECS_TOOLS_UI_DOCKSPACE_BORDER_SPRITE_TEXTURE_DEFAULT_COUNT 128
#define ECS_TOOLS_UI_DOCKSPACE_BORDER_SPRITE_CACHE_COUNT 16
#define ECS_TOOLS_UI_DOCKSPACE_BORDER_HOVERABLE_HANDLER_COUNT 128
#define ECS_TOOLS_UI_DOCKSPACE_BORDER_LEFT_CLICKABLE_HANDLER_COUNT 128
#define ECS_TOOLS_UI_DOCKSPACE_BORDER_MISC_CLICKABLE_HANDLER_COUNT 32
#define ECS_TOOLS_UI_DOCKSPACE_BORDER_GENERAL_HANDLER_COUNT 128
#define ECS_TOOLS_UI_DOCKSPACE_MINIMUM_SCALE 0.08f
#define ECS_TOOLS_UI_DOCKSPACE_VIEWPORT_PADDING_X 0.0011692f
#define ECS_TOOLS_UI_DOCKSPACE_VIEWPORT_PADDING_Y 0.001455f
#define ECS_TOOLS_UI_DOCKSPACE_REGION_HEADER_SPACING 0.007f
#define ECS_TOOLS_UI_DOCKSPACE_REGION_HEADER_PADDING 0.025f
#define ECS_TOOLS_UI_DOCKSPACE_REGION_HEADER_ADDED_SCALE 0.02f
#define ECS_TOOLS_UI_DOCKSPACE_COLLAPSE_TRIANGLE_SCALE 0.03f
#define ECS_TOOLS_UI_DOCKSPACE_CLOSE_X_POSITION_X_LEFT 0.015f
#define ECS_TOOLS_UI_DOCKSPACE_CLOSE_X_POSITION_X_RIGHT 0.0035f
#define ECS_TOOLS_UI_DOCKSPACE_CLOSE_X_SCALE_Y 0.03f
#define ECS_TOOLS_UI_DOCKSPACE_CLOSE_X_TOTAL_X_PADDING 0.02f

#define ECS_TOOLS_UI_FONT_SIZE 0.00075f
#define ECS_TOOLS_UI_FONT_CHARACTER_SPACING 0.0009f
#define ECS_TOOLS_UI_FONT_SYMBOL_COUNT 105
#define ECS_TOOLS_UI_FONT_TEXTURE_DIMENSIONS 201
#define ECS_TOOLS_UI_FONT_X_FACTOR 0.52f
#define ECS_TOOLS_UI_FONT 0

#define ECS_TOOLS_UI_ATLAS_VERTEX_BUFFER 1000

#define ECS_TOOLS_UI_DOCKING_GIZMO_CENTRAL_SCALE {NormalizeHorizontalToWindowDimensions(0.075f), 0.075f}
#define ECS_TOOLS_UI_DOCKING_GIZMO_BORDER_SCALE {ECS_TOOLS_UI_ONE_PIXEL_X, ECS_TOOLS_UI_ONE_PIXEL_Y }
#define ECS_TOOLS_UI_DOCKING_GIZMO_MINIMUM_SCALE {system->NormalizeHorizontalToWindowDimensions(0.35f), 0.35f}
#define ECS_TOOLS_UI_DOCKING_GIZMO_CENTRAL_SPACING {NormalizeHorizontalToWindowDimensions(0.015f), 0.015f}
#define ECS_TOOLS_UI_DOCKING_GIZMO_LINE_WIDTH ECS_TOOLS_UI_ONE_PIXEL_Y
#define ECS_TOOLS_UI_DOCKING_GIZMO_LINE_COUNT 3
#define ECS_TOOLS_UI_DOCKING_GIZMO_SPACING 0.005f
#define ECS_TOOLS_UI_DOCKING_GIZMO_TRANSPARENT_HOVER_ALPHA 125

#define ECS_TOOLS_UI_DRAG_PADDING 0.05f

#define ECS_TOOLS_UI_MATERIALS 5
#define ECS_TOOLS_UI_VERTEX_BUFFER_SOLID_COLOR_SIZE 10000
#define ECS_TOOLS_UI_VERTEX_BUFFER_TEXT_SPRITE_SIZE 35000
#define ECS_TOOLS_UI_VERTEX_BUFFER_SPRITE_SIZE 1000
#define ECS_TOOLS_UI_VERTEX_BUFFER_SPRITE_CLUSTER_SIZE 2500
#define ECS_TOOLS_UI_VERTEX_BUFFER_LINE_SIZE 25000
#define ECS_TOOLS_UI_LATE_DRAW_VERTEX_BUFFER_SOLID_COLOR_SIZE 2000
#define ECS_TOOLS_UI_LATE_DRAW_VERTEX_BUFFER_TEXT_SPRITE_SIZE 5000
#define ECS_TOOLS_UI_LATE_DRAW_VERTEX_BUFFER_SPRITE_SIZE 500
#define ECS_TOOLS_UI_LATE_DRAW_VERTEX_BUFFER_SPRITE_CLUSTER_SIZE 1000
#define ECS_TOOLS_UI_LATE_DRAW_VERTEX_BUFFER_LINE_SIZE 25000
#define ECS_TOOLS_UI_VERTEX_BUFFER_MISC_SOLID_COLOR 10000
#define ECS_TOOLS_UI_VERTEX_BUFFER_MISC_TEXT_SPRITE 20000
#define ECS_TOOLS_UI_VERTEX_BUFFER_MISC_SPRITE 1000
#define ECS_TOOLS_UI_VERTEX_BUFFER_MISC_SPRITE_CLUSTER 5000
#define ECS_TOOLS_UI_VERTEX_BUFFER_MISC_LINE 25000

#define ECS_TOOLS_UI_SOLID_COLOR 0
#define ECS_TOOLS_UI_TEXT_SPRITE 1
#define ECS_TOOLS_UI_SPRITE 2
#define ECS_TOOLS_UI_SPRITE_CLUSTER 3
#define ECS_TOOLS_UI_LINE 4

#define ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFER_INDEX 0
#define ECS_TOOLS_UI_SPRITE_CLUSTER_TEXTURE_BUFFER_INDEX 1

#define ECS_TOOLS_UI_CLUSTER_SPRITE_SUBSTREAM_INITIAL_COUNT 16

#define ECS_TOOLS_UI_PASSES 2
#define ECS_TOOLS_UI_SPRITE_TEXTURE_BUFFERS_PER_PASS 2

#define ECS_TOOLS_UI_SAMPLER_COUNT 8
#define ECS_TOOLS_UI_ANISOTROPIC_SAMPLER 0

#define ECS_TOOLS_UI_WINDOW_LAYOUT_ELEMENT_INDENTATION 0.01f
#define ECS_TOOLS_UI_WINDOW_LAYOUT_NEXT_ROW 0.018f
#define ECS_TOOLS_UI_WINDOW_LAYOUT_NEXT_ROW_PADDING 0.015f
#define ECS_TOOLS_UI_WINDOW_LAYOUT_DEFAULT_ELEMENT_X 0.09f
#define ECS_TOOLS_UI_WINDOW_LAYOUT_DEFAULT_ELEMENT_Y 0.037f
#define ECS_TOOLS_UI_WINDOW_LAYOUT_NODE_INDENTATION 0.015f

#define ECS_TOOLS_UI_REVERT_HANDLER_COMMAND_COUNT 256

#define ECS_TOOLS_UI_TEXT_INPUT_CARET_DISPLAY_TIME 500
#define ECS_TOOLS_UI_TEXT_INPUT_REPEAT_COUNT 60
#define ECS_TOOLS_UI_TEXT_INPUT_REPEAT_START_TIME 400
#define ECS_TOOLS_UI_TEXT_INPUT_DEFAULT_COUNT 32
#define ECS_TOOLS_UI_TEXT_INPUT_COPY_CLIPBOARD_ALLOCATION 1024

#define ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_X 0.0032f
#define ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_Y 0.0055f
#define ECS_TOOLS_UI_SLIDER_LENGTH_X 0.024f
#define ECS_TOOLS_UI_SLIDER_LENGTH_Y 0.042f
#define ECS_TOOLS_UI_SLIDER_BRING_BACK_START 200
#define ECS_TOOLS_UI_SLIDER_BRING_BACK_TICK 0.015f
#define ECS_TOOLS_UI_SLIDER_ENTER_VALUES_DURATION 300

#define ECS_TOOLS_UI_LABEL_HORIZONTAL_PADD 0.007f
#define ECS_TOOLS_UI_LABEL_VERTICAL_PADD 0.007f

#define ECS_TOOLS_UI_COLOR_INPUT_WINDOW_SIZE_Y 0.8f
#define ECS_TOOLS_UI_COLOR_INPUT_WINDOW_SIZE_X NormalizeHorizontalToWindowDimensions(ECS_TOOLS_UI_COLOR_INPUT_WINDOW_SIZE_Y)

#define ECS_TOOLS_UI_COLOR_INPUT_PADD ECS_TOOLS_UI_WINDOW_LAYOUT_ELEMENT_INDENTATION * 0.5f

#define ECS_TOOLS_UI_COMBO_BOX_TRIANGLE_SCALE_FACTOR 1.0f
#define ECS_TOOLS_UI_COMBO_BOX_PADDING 0.01f

#define ECS_TOOLS_UI_HIERARCHY_DRAG_NODE_DIMENSION 0.01f
#define ECS_TOOLS_UI_HIERARCHY_DRAG_NODE_TIME 500
#define ECS_TOOLS_UI_HIERARCHY_DRAG_NODE_HOVER_DROP 1500

#define ECS_TOOLS_UI_TEXT_TABLE_INFER_FACTOR 4.0f
#define ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR 5.0f

#define ECS_TOOLS_UI_GRAPH_PADDING {NormalizeHorizontalToWindowDimensions(0.014f), 0.014f}
#define ECS_TOOLS_UI_GRAPH_LINE_THICKNESS 0.04f
#define ECS_TOOLS_UI_GRAPH_REDUCE_FONT 0.85f
#define ECS_TOOLS_UI_GRAPH_VALUE_LINE_SIZE {NormalizeHorizontalToWindowDimensions(0.0028f), 0.0028f}
#define ECS_TOOLS_UI_GRAPH_X_SPACE 0.025f
#define ECS_TOOLS_UI_GRAPH_BUMP {NormalizeHorizontalToWindowDimensions(0.0056f), 0.0056f}
#define ECS_TOOLS_UI_GRAPH_HOVER_OFFSET {0.03f, 0.0f}
#define ECS_TOOLS_UI_GRAPH_SAMPLE_CIRCLE_SIZE 0.015f

#define ECS_TOOLS_UI_HISTOGRAM_PADDING {NormalizeHorizontalToWindowDimensions(0.01f), 0.01f}
#define ECS_TOOLS_UI_HISTOGRAM_BAR_MIN_SCALE NormalizeHorizontalToWindowDimensions(0.0028f)
#define ECS_TOOLS_UI_HISTOGRAM_BAR_SPACING NormalizeHorizontalToWindowDimensions(0.0028f)
#define ECS_TOOLS_UI_HISTOGRAM_REDUCE_FONT 0.85f
#define ECS_TOOLS_UI_HISTOGRAM_HOVER_OFFSET {0.03f, 0.0f}

#define ECS_TOOLS_UI_MENU_PADDING 0.01f
#define ECS_TOOLS_UI_MENU_SUBMENUES_MAX_COUNT 8
#define ECS_TOOLS_UI_MENU_X_PADD 0.005f

#define ECS_TOOLS_UI_LABEL_LIST_CIRCLE_SIZE 0.015f

#define ECS_TOOLS_UI_DARKEN_HOVER_WHEN_PRESSED_FACTOR 0.85f
#define ECS_TOOLS_UI_SLIDER_LIGHTEN_FACTOR 1.4f

#define ECS_TOOLS_UI_DEFAULT_HOVERABLE_DATA_COUNT 4
#define ECS_TOOLS_UI_DEFAULT_HANDLER_SCROLL_FACTOR 0.00035f
#define ECS_TOOLS_UI_DEFAULT_HANDLER_SCROLL_THRESHOLD 1.0f
#define ECS_TOOLS_UI_DEFAULT_HANDLER_SCROLL_STEP 0.0007f
#define ECS_TOOLS_UI_DEFAULT_HANDLER_ZOOM_FACTOR 0.00025f

#define ECS_TOOLS_UI_MENU_BUTTON_PADDING 0.005f

#define ECS_TOOLS_UI_FIT_TO_CONTENT_PADDING_X 0.01f
#define ECS_TOOLS_UI_FIT_TO_CONTENT_PADDING_Y 0.01f

#define ECS_TOOLS_UI_TOOL_TIP_TEXT_TO_RIGHT_PADD 0.03f

#define ECS_TOOLS_UI_THEME_COLOR Color(0, 50, 120)
#define ECS_TOOLS_UI_TEXT_COLOR Color(200, 200, 200)
#define ECS_TOOLS_UI_WINDOW_BACKGROUND_COLOR Color(25, 25, 25)
#define ECS_TOOLS_UI_WINDOW_BORDER_COLOR Color(90, 90, 90)
#define ECS_TOOLS_UI_WINDOW_REGION_HEADER_COLLAPSE_TRIANGLE_COLOR ECS_TOOLS_UI_TEXT_COLOR
#define ECS_TOOLS_UI_WINDOW_REGION_HEADER_CLOSE_X_COLOR ECS_TOOLS_UI_TEXT_COLOR
#define ECS_TOOLS_UI_DOCKING_GIZMO_BORDER_COLOR ECS_TOOLS_UI_WINDOW_BORDER_COLOR
#define ECS_TOOLS_UI_UNAVAILABLE_TEXT_COLOR Color(120, 120, 120)
#define ECS_TOOLS_UI_RENDER_SLIDERS_BACKGROUND_COLOR Color(40, 40, 40)
#define ECS_TOOLS_UI_RENDER_SLIDERS_ACTIVE_PART_COLOR Color(75, 75, 75)
#define ECS_TOOLS_UI_HIERARCHY_DRAG_NODE_COLOR Color(20, 110, 10)
#define ECS_TOOLS_UI_GRAPH_LINE_COLOR ECS_TOOLS_UI_TEXT_COLOR
#define ECS_TOOLS_UI_GRAPH_HOVER_LINE_COLOR Color(228, 84, 13)
#define ECS_TOOLS_UI_GRAPH_SAMPLE_CIRCLE_COLOR Color(130, 20, 0)
#define ECS_TOOLS_UI_HISTOGRAM_COLOR DarkenColor(Color(230, 179, 0), 0.9f)
#define ECS_TOOLS_UI_HISTOGRAM_HOVERED_COLOR Color(228, 84, 13)
#define ECS_TOOLS_UI_HISTOGRAM_TEXT_COLOR Color(140, 20, 20)
#define ECS_TOOLS_UI_PROGRESS_BAR_COLOR Color(20, 200, 30)

#define ECS_TOOLS_UI_TOOL_TIP_BACKGROUND_COLOR Color(20, 20, 20)
#define ECS_TOOLS_UI_TOOL_TIP_BORDER_COLOR Color(29, 52, 97)
#define ECS_TOOLS_UI_TOOL_TIP_FONT_COLOR Color(130, 156, 188)
#define ECS_TOOLS_UI_TOOL_TIP_UNAVAILABLE_FONT_COLOR DarkenColor(ECS_TOOLS_UI_TOOL_TIP_FONT_COLOR, 0.5f)

#define ECS_TOOLS_UI_MENU_ARROW_COLOR ECS_TOOLS_UI_TEXT_COLOR
#define ECS_TOOLS_UI_MENU_ARROW_UNAVAILABLE_COLOR ECS_TOOLS_UI_TOOL_TIP_UNAVAILABLE_FONT_COLOR
#define ECS_TOOLS_UI_MENU_HOVERED_ARROW_COLOR Color(150, 20, 150)

#define ECS_TOOLS_UI_VIEWPORT_BACKGROUND_ALPHA 180
#define ECS_TOOLS_UI_WINDOW_TITLE_COLOR_FACTOR 0.9f
#define ECS_TOOLS_UI_WINDOW_TITLE_INACTIVE_COLOR_FACTOR 0.65f
#define ECS_TOOLS_UI_WINDOW_REGION_HEADER_COLOR_FACTOR 1.32f
#define ECS_TOOLS_UI_WINDOW_REGION_HEADER_COLOR_CLOSE_X_HOVER_COLOR_FACTOR 1.5f
#define ECS_TOOLS_UI_WINDOW_REGION_HEADER_ACTIVE_WINDOW_LIGHTEN_FACTOR 1.25f
#define ECS_TOOLS_UI_DOCKING_GIZMO_COLOR_FACTOR ECS_TOOLS_UI_WINDOW_TITLE_COLOR_FACTOR
#define ECS_TOOLS_UI_WINDOW_BORDER_HOVERED_COLOR_FACTOR 1.1f
#define ECS_TOOLS_UI_TEXT_INPUT_SELECT_FACTOR 1.6f
#define ECS_TOOLS_UI_COLOR_INPUT_BORDER 0.8f
#define ECS_TOOLS_UI_COMBO_BOX_TRIANGLE_BOX_FACTOR 1.3f
#define ECS_TOOLS_UI_CHECK_BOX_LIGTHEN_FACTOR 1.8f
#define ECS_TOOLS_UI_DARKEN_INACTIVE_ITEM_FACTOR 0.8f
#define ECS_TOOLS_UI_ALPHA_INACTIVE_ITEM_FACTOR 0.3f
#define ECS_TOOLS_UI_GRAPH_TEXT_ALPHA 100
#define ECS_TOOLS_UI_GRAPH_DROP_COLOR_ALPHA 150

constexpr size_t UI_TRIM_POP_UP_WINDOW_NO_HORIZONTAL_LEFT = 1 << 0;
constexpr size_t UI_TRIM_POP_UP_WINDOW_NO_HORIZONTAL_RIGHT = 1 << 1;
constexpr size_t UI_TRIM_POP_UP_WINDOW_NO_VERTICAL_TOP = 1 << 2;
constexpr size_t UI_TRIM_POP_UP_WINDOW_NO_VERTICAL_BOTTOM = 1 << 3;

constexpr size_t UI_DOCKSPACE_BORDER_FLAG_COLLAPSED_REGION_HEADER = 1 << 0;
constexpr size_t UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X = 1 << 1;
constexpr size_t UI_DOCKSPACE_BORDER_FLAG_NO_TITLE = 1 << 2;
constexpr size_t UI_DOCKSPACE_BORDER_NOTHING = 1 << 3;
constexpr size_t UI_POP_UP_WINDOW_FIT_TO_CONTENT = 1 << 4;
constexpr size_t UI_POP_UP_WINDOW_FIT_TO_CONTENT_ADD_RENDER_SLIDER_SIZE = 1 << 5;
constexpr size_t UI_POP_UP_WINDOW_FIT_TO_CONTENT_CENTER = 1 << 6;

constexpr size_t UI_DOCKSPACE_FIXED = 1 << 10;
constexpr size_t UI_DOCKSPACE_NO_DOCKING = 1 << 11;
constexpr size_t UI_DOCKSPACE_LOCK_WINDOW = 1 << 12;
constexpr size_t UI_DOCKSPACE_POP_UP_WINDOW = 1 << 13;
constexpr size_t UI_DOCKSPACE_BACKGROUND = 1 << 14;
constexpr size_t UI_DOCKSPACE_FIT_TO_VIEW = 1 << 15;

constexpr size_t UI_POP_UP_WINDOW_ALL = UI_POP_UP_WINDOW_FIT_TO_CONTENT | UI_POP_UP_WINDOW_FIT_TO_CONTENT_ADD_RENDER_SLIDER_SIZE | UI_POP_UP_WINDOW_FIT_TO_CONTENT_CENTER
	| UI_DOCKSPACE_POP_UP_WINDOW | UI_DOCKSPACE_LOCK_WINDOW | UI_DOCKSPACE_NO_DOCKING;
