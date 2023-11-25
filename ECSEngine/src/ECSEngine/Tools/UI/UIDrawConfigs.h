#pragma once

namespace ECSEngine {

	namespace Tools {

		constexpr size_t UI_CONFIG_ABSOLUTE_TRANSFORM = 1 << 0;
		constexpr size_t UI_CONFIG_RELATIVE_TRANSFORM = 1 << 1;
		constexpr size_t UI_CONFIG_MAKE_SQUARE = 1 << 2;
		constexpr size_t UI_CONFIG_WINDOW_DEPENDENT_SIZE = 1 << 3;
		constexpr size_t UI_CONFIG_DO_CACHE = 1 << 4;
		constexpr size_t UI_CONFIG_TEXT_PARAMETERS = 1 << 5;
		constexpr size_t UI_CONFIG_TEXT_ALIGNMENT = 1 << 6;
		constexpr size_t UI_CONFIG_COLOR = 1 << 7;
		constexpr size_t UI_CONFIG_VERTICAL = 1 << 8;
		constexpr size_t UI_CONFIG_BORDER = (size_t)1 << 9;
		constexpr size_t UI_CONFIG_DO_NOT_ADVANCE = 1 << 10;
		constexpr size_t UI_CONFIG_DO_NOT_FIT_SPACE = 1 << 11;
		constexpr size_t UI_CONFIG_LATE_DRAW = 1 << 12;
		constexpr size_t UI_CONFIG_SYSTEM_DRAW = 1 << 13;
		constexpr size_t UI_CONFIG_DO_NOT_VALIDATE_POSITION = 1 << 14;
		constexpr size_t UI_CONFIG_ELEMENT_NAME_FIRST = 1 << 15;
		constexpr size_t UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW = 1 << 16;
		constexpr size_t UI_CONFIG_UNAVAILABLE_TEXT = (size_t)1 << 17;
		constexpr size_t UI_CONFIG_ALIGN_TO_ROW_Y = (size_t)1 << 18;
		constexpr size_t UI_CONFIG_ACTIVE_STATE = (size_t)1 << 19;
		constexpr size_t UI_CONFIG_ALIGN_ELEMENT = (size_t)1 << 20;		
		constexpr size_t UI_CONFIG_DO_NOT_UPDATE_RENDER_BOUNDS = (size_t)1 << 21;
		constexpr size_t UI_CONFIG_ELEMENT_NAME_ACTION = (size_t)1 << 22;
		constexpr size_t UI_CONFIG_ELEMENT_NAME_BACKGROUND = (size_t)1 << 23;
		constexpr size_t UI_CONFIG_NAME_PADDING = (size_t)1 << 24;
		constexpr size_t UI_CONFIG_DYNAMIC_RESOURCE = (size_t)1 << 25;
		constexpr size_t UI_CONFIG_GET_TRANSFORM = (size_t)1 << 26;
		constexpr size_t UI_CONFIG_INITIALIZER_DO_NOT_BEGIN = (size_t)1 << 27;
		constexpr size_t UI_CONFIG_TOOL_TIP = (size_t)1 << 28;
		
		// Low slots available 29

		// High slots available 60, 61, 62, 63 (60 aliases with an array config)
		
		constexpr size_t UI_CONFIG_RECTANGLE_VERTEX_COLOR = (size_t)1 << 55;
		constexpr size_t UI_CONFIG_RECTANGLE_HOVERABLE_ACTION = (size_t)1 << 56;
		constexpr size_t UI_CONFIG_RECTANGLE_CLICKABLE_ACTION = (size_t)1 << 57;
		constexpr size_t UI_CONFIG_RECTANGLE_GENERAL_ACTION = (size_t)1 << 58;

		// Internal flag used by the group slider and text input to signal to element name
		// that they should fit the group name to the scale
		constexpr size_t UI_CONFIG_NAME_FIT_TO_SCALE = (size_t)1 << 63;

		// Internal flag
		constexpr size_t UI_CONFIG_DISABLE_TRANSLATE_TEXT = (size_t)1 << 63;

		constexpr size_t UI_CONFIG_BUTTON_HOVERABLE = (size_t)1 << 40;
		
		constexpr size_t UI_CONFIG_RECTANGLES_INDIVIDUAL_ACTIONS = (size_t)1 << 35;

		constexpr size_t UI_CONFIG_SPRITE_GRADIENT = (size_t)1 << 32;

		constexpr size_t UI_CONFIG_LABEL_TRANSPARENT = (size_t)1 << 30;
		constexpr size_t UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X = (size_t)1 << 31;
		constexpr size_t UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y = (size_t)1 << 32;

		constexpr size_t UI_CONFIG_TEXT_INPUT_NO_NAME = (size_t)1 << 33;
		constexpr size_t UI_CONFIG_TEXT_INPUT_HINT = (size_t)1 << 34;
		constexpr size_t UI_CONFIG_TEXT_INPUT_CALLBACK = (size_t)1 << 35;
		constexpr size_t UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER = (size_t)1 << 36;
		// Display the entire input with a tooltip when the size exceeds that of the input 
		constexpr size_t UI_CONFIG_TEXT_INPUT_MISC = (size_t)1 << 37;

		constexpr size_t UI_CONFIG_PATH_INPUT_ROOT = (size_t)1 << 38;
		constexpr size_t UI_CONFIG_PATH_INPUT_GIVE_FILES = (size_t)1 << 39;
		constexpr size_t UI_CONFIG_PATH_INPUT_CUSTOM_FILESYSTEM = (size_t)1 << 40;
		constexpr size_t UI_CONFIG_PATH_INPUT_CALLBACK = (size_t)1 << 41;
		constexpr size_t UI_CONFIG_PATH_INPUT_SPRITE_TEXTURE = (size_t)1 << 42;

		constexpr size_t UI_CONFIG_SLIDER_NO_TEXT = (size_t)1 << 38;
		constexpr size_t UI_CONFIG_SLIDER_NO_NAME = (size_t)1 << 39;
		constexpr size_t UI_CONFIG_SLIDER_COLOR = (size_t)1 << 40;
		constexpr size_t UI_CONFIG_SLIDER_SHRINK = (size_t)1 << 41;
		constexpr size_t UI_CONFIG_SLIDER_LENGTH = (size_t)1 << 42;
		constexpr size_t UI_CONFIG_SLIDER_PADDING = (size_t)1 << 43;
		constexpr size_t UI_CONFIG_SLIDER_MOUSE_DRAGGABLE = (size_t)1 << 44;
		constexpr size_t UI_CONFIG_SLIDER_ENTER_VALUES = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_SLIDER_DEFAULT_VALUE = (size_t)1 << 47;

		constexpr size_t UI_CONFIG_SLIDER_GROUP_NO_NAME = (size_t)1 << 50;
		constexpr size_t UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS = (size_t)1 << 51;
		constexpr size_t UI_CONFIG_SLIDER_GROUP_UNIFORM_DEFAULT = (size_t)1 << 52;
		constexpr size_t UI_CONFIG_SLIDER_GROUP_NO_SUBNAMES = (size_t)1 << 53;

		constexpr size_t UI_CONFIG_COLOR_INPUT_SLIDERS = (size_t)1 << 48;
		constexpr size_t UI_CONFIG_COLOR_INPUT_NO_NAME = (size_t)1 << 49;
		constexpr size_t UI_CONFIG_COLOR_INPUT_DO_NOT_CHOOSE_COLOR = (size_t)1 << 50;
		constexpr size_t UI_CONFIG_COLOR_INPUT_CALLBACK = (size_t)1 << 51;
		constexpr size_t UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE = (size_t)1 << 52;

		constexpr size_t UI_CONFIG_COMBO_BOX_NO_NAME = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_COMBO_BOX_DISABLE_X_SCALING = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_COMBO_BOX_PREFIX = (size_t)1 << 47;
		constexpr size_t UI_CONFIG_COMBO_BOX_CALLBACK = (size_t)1 << 48;
		constexpr size_t UI_CONFIG_COMBO_BOX_NAME_WITH_SCALE = (size_t)1 << 49;
		constexpr size_t UI_CONFIG_COMBO_BOX_MAPPING = (size_t)1 << 50;
		constexpr size_t UI_CONFIG_COMBO_BOX_UNAVAILABLE = (size_t)1 << 51;
		constexpr size_t UI_CONFIG_COMBO_BOX_DEFAULT = (size_t)1 << 52;

		constexpr size_t UI_CONFIG_CHECK_BOX_NO_NAME = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_CHECK_BOX_CALLBACK = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_CHECK_BOX_DEFAULT = (size_t)1 << 47;

		constexpr size_t UI_CONFIG_CROSS_LINE_DO_NOT_INFER = (size_t)1 << 45;

		constexpr size_t UI_CONFIG_COLLAPSING_HEADER_NO_NAME = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_COLLAPSING_HEADER_DO_NOT_INFER = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_COLLAPSING_HEADER_SELECTION = (size_t)1 << 47;
		constexpr size_t UI_CONFIG_COLLAPSING_HEADER_BUTTONS = (size_t)1 << 48;
		constexpr size_t UI_CONFIG_COLLAPSING_HEADER_CALLBACK = (size_t)1 << 49;

		constexpr size_t UI_CONFIG_MENU_SPRITE = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_MENU_COPY_STATES = (size_t)1 << 46;

		constexpr size_t UI_CONFIG_MENU_BUTTON_SPRITE = (size_t)1 << 45;

		constexpr size_t UI_CONFIG_SENTENCE_FIT_SPACE = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_SENTENCE_DO_NOT_CACHE_VARIABLE = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE = (size_t)1 << 47;
		constexpr size_t UI_CONFIG_SENTENCE_HOVERABLE_HANDLERS = (size_t)1 << 48;
		constexpr size_t UI_CONFIG_SENTENCE_CLICKABLE_HANDLERS = (size_t)1 << 49;
		constexpr size_t UI_CONFIG_SENTENCE_GENERAL_HANDLERS = (size_t)1 << 50;
		// Can make the sentence fit space even tho the draw mode is different.
		// And can change the token after which the fit space is done for.
		constexpr size_t UI_CONFIG_SENTENCE_FIT_SPACE_TOKEN = (size_t)1 << 51;

		constexpr size_t UI_CONFIG_TEXT_TABLE_DO_NOT_INFER = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_TEXT_TABLE_NO_BORDER = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE = (size_t)1 << 47;

		constexpr size_t UI_CONFIG_GRAPH_NO_NAME = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_GRAPH_X_AXIS = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_GRAPH_Y_AXIS = (size_t)1 << 47;
		constexpr size_t UI_CONFIG_GRAPH_LINE_COLOR = (size_t)1 << 48;
		constexpr size_t UI_CONFIG_GRAPH_REDUCE_FONT = (size_t)1 << 49;
		constexpr size_t UI_CONFIG_GRAPH_KEEP_RESOLUTION_X = (size_t)1 << 50;
		constexpr size_t UI_CONFIG_GRAPH_KEEP_RESOLUTION_Y = (size_t)1 << 51;
		constexpr size_t UI_CONFIG_GRAPH_DROP_COLOR = (size_t)1 << 52;
		constexpr size_t UI_CONFIG_GRAPH_SAMPLE_CIRCLES = (size_t)1 << 53;
		constexpr size_t UI_CONFIG_GRAPH_FILTER_SAMPLES_X = (size_t)1 << 54;
		constexpr size_t UI_CONFIG_GRAPH_FILTER_SAMPLES_Y = (size_t)1 << 55;
		constexpr size_t UI_CONFIG_GRAPH_TAGS = (size_t)1 << 56;
		constexpr size_t UI_CONFIG_GRAPH_MIN_Y = (size_t)1 << 57;
		constexpr size_t UI_CONFIG_GRAPH_MAX_Y = (size_t)1 << 58;
		constexpr size_t UI_CONFIG_GRAPH_NO_BACKGROUND = (size_t)1 << 59;
		constexpr size_t UI_CONFIG_GRAPH_CLICKABLE = (size_t)1 << 60;
		constexpr size_t UI_CONFIG_GRAPH_INFO_LABELS = (size_t)1 << 61;

		constexpr size_t UI_CONFIG_HISTOGRAM_COLOR = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_HISTOGRAM_REDUCE_FONT = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_HISTOGRAM_VARIANT_ZOOM_FONT = (size_t)1 << 47;

		constexpr size_t UI_CONFIG_NUMBER_INPUT_RANGE = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_NUMBER_INPUT_DEFAULT = (size_t)1 << 47;
		constexpr size_t UI_CONFIG_NUMBER_INPUT_NO_DRAG_VALUE = (size_t)1 << 48;

		constexpr size_t UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME = (size_t)1 << 49;
		constexpr size_t UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS = (size_t)1 << 50;
		constexpr size_t UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_DEFAULT = (size_t)1 << 51;
		constexpr size_t UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES = (size_t)1 << 52;

		constexpr size_t UI_CONFIG_STATE_TABLE_NO_NAME = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_STATE_TABLE_SINGLE_POINTER = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_STATE_TABLE_ALL = (size_t)1 << 47;
		constexpr size_t UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE = (size_t)1 << 48;
		constexpr size_t UI_CONFIG_STATE_TABLE_CALLBACK = (size_t)1 << 49;

		constexpr size_t UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_FILTER_MENU_ALL = (size_t)1 << 47;
		constexpr size_t UI_CONFIG_FILTER_MENU_NOTIFY_ON_CHANGE = (size_t)1 << 48;

		constexpr size_t UI_CONFIG_LABEL_HIERARCHY_RENAME_LABEL = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_LABEL_HIERARCHY_DRAG_LABEL = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK = (size_t)1 << 47;
		constexpr size_t UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK = (size_t)1 << 48;
		constexpr size_t UI_CONFIG_LABEL_HIERARCHY_SPRITE_TEXTURE = (size_t)1 << 49;
		constexpr size_t UI_CONFIG_LABEL_HIERARCHY_DOUBLE_CLICK_ACTION = (size_t)1 << 50;
		constexpr size_t UI_CONFIG_LABEL_HIERARCHY_FILTER = (size_t)1 << 51;
		constexpr size_t UI_CONFIG_LABEL_HIERARCHY_DEALLOCATE_LABEL = (size_t)1 << 52;
		constexpr size_t UI_CONFIG_LABEL_HIERARCHY_BASIC_OPERATIONS = (size_t)1 << 53;
		// The selectable will fill in the selection when something is changed from the UI
		// and update the UI when the selection is done somewhere else
		constexpr size_t UI_CONFIG_LABEL_HIERARCHY_MONITOR_SELECTION = (size_t)1 << 54;

		constexpr size_t UI_CONFIG_FILESYSTEM_HIERARCHY_DO_NOT_INFER_X = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_FILESYSTEM_HIERARCHY_HASH_TABLE_CAPACITY = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_FILESYSTEM_HIERARCHY_RIGHT_CLICK = UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK;
		constexpr size_t UI_CONFIG_FILESYSTEM_HIERARCHY_SELECTABLE_CALLBACK = UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK;
		constexpr size_t UI_CONFIG_FILESYSTEM_HIERARCHY_SPRITE_TEXTURE = UI_CONFIG_LABEL_HIERARCHY_SPRITE_TEXTURE;

		constexpr size_t UI_CONFIG_SPRITE_BUTTON_BACKGROUND = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_SPRITE_BUTTON_CENTER_SPRITE_TO_BACKGROUND = (size_t)1 << 46;
		constexpr size_t UI_CONFIG_SPRITE_BUTTON_MULTI_TEXTURE = (size_t)1 << 47;

		constexpr size_t UI_CONFIG_LABEL_LIST_NO_NAME = (size_t)1 << 45;

		constexpr size_t UI_CONFIG_SELECTION_INPUT_OVERRIDE_HOVERABLE = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_SELECTION_INPUT_LABEL_CLICKABLE = (size_t)1 << 46;

		constexpr size_t UI_CONFIG_ARRAY_ADD_CALLBACK = (size_t)1 << 53;
		constexpr size_t UI_CONFIG_ARRAY_REMOVE_CALLBACK = (size_t)1 << 54;
		constexpr size_t UI_CONFIG_ARRAY_FIXED_SIZE = (size_t)1 << 55;
		constexpr size_t UI_CONFIG_ARRAY_DISABLE_SIZE_INPUT = (size_t)1 << 56;
		constexpr size_t UI_CONFIG_ARRAY_REMOVE_ANYWHERE = (size_t)1 << 57;
		constexpr size_t UI_CONFIG_ARRAY_DISABLE_DRAG = (size_t)1 << 58;
		constexpr size_t UI_CONFIG_ARRAY_PRE_POST_DRAW = (size_t)1 << 59;
		// Tells the array that the element has multi line values
		// and that it should add a header (not a collapsing one) before the values
		constexpr size_t UI_CONFIG_ARRAY_MULTI_LINE = (size_t)1 << 60;

		constexpr size_t UI_CONFIG_COLOR_FLOAT_DEFAULT_VALUE = (size_t)1 << 61;
		constexpr size_t UI_CONFIG_COLOR_FLOAT_CALLBACK = (size_t)1 << 62;

		constexpr size_t UI_CONFIG_SPRITE_STATE_BUTTON_NO_BACKGROUND_WHEN_DESELECTED = (size_t)1 << 45;
		constexpr size_t UI_CONFIG_SPRITE_STATE_BUTTON_CALLBACK = (size_t)1 << 46;
	}

}