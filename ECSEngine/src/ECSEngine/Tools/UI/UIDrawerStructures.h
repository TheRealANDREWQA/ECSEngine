#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Containers/HashTable.h"
#include "../../Containers/Stacks.h"
#include "../../Containers/Queues.h"
#include "UIStructures.h"
#include "UIDrawConfigs.h"
#include "../../Rendering/ColorUtilities.h"
#include "UISystem.h"

namespace ECSEngine {

	namespace Tools {

		struct UIDrawConfig;
		struct UIDrawer;

		typedef bool (*UIDrawerTextInputFilter)(char, CharacterType);

		typedef void (*UIDrawerSliderInterpolate)(const void* lower_bound, const void* upper_bound, void* value, float percentage);
		typedef void (*UIDrawerSliderFromFloat)(void* value, float factor);
		typedef float (*UIDrawerSliderToFloat)(const void* value);
		typedef float (*UIDrawerSliderPercentage)(const void* lower_bound, const void* upper_bound, const void* value);
		typedef void (*UIDrawerSliderToString)(CapacityStream<char>& characters, const void* data, void* extra_data);
		typedef void (*UIDrawerSliderConvertTextInput)(CapacityStream<char>& characters, void* data);
		typedef bool (*UIDrawerSliderIsSmaller)(const void* left, const void* right);

		struct DebouncingEntry {
			// Returns the data casted to the template type
			template<typename T>
			ECS_INLINE T* Data() const {
				return (T*)data;
			}

			Timer timer;
			void* data;
			unsigned int dynamic_index;
			// Set this flag to true when the debouncing value should be updated
			// Immediately the next call, even tho the update period has not been exceeded
			bool force_update;
		};

		// Sets the force update flag to true only if the pointer is non null
		ECS_INLINE void DebouncingEntryForceUpdate(DebouncingEntry* entry) {
			if (entry != nullptr) {
				entry->force_update = true;
			}
		}

		enum ECS_UI_DRAWER_MODE : unsigned char {
			ECS_UI_DRAWER_INDENT,
			ECS_UI_DRAWER_NEXT_ROW,
			ECS_UI_DRAWER_NEXT_ROW_COUNT,
			ECS_UI_DRAWER_FIT_SPACE,
			ECS_UI_DRAWER_COLUMN_DRAW,
			ECS_UI_DRAWER_COLUMN_DRAW_FIT_SPACE,
			ECS_UI_DRAWER_NOTHING
		};

		enum ECS_UI_ALIGN : unsigned char {
			ECS_UI_ALIGN_LEFT,
			ECS_UI_ALIGN_RIGHT,
			ECS_UI_ALIGN_MIDDLE,
			ECS_UI_ALIGN_TOP,
			ECS_UI_ALIGN_BOTTOM
		};

		enum ECS_UI_WINDOW_DEPENDENT_SIZE : unsigned char {
			ECS_UI_WINDOW_DEPENDENT_HORIZONTAL,
			ECS_UI_WINDOW_DEPENDENT_VERTICAL,
			ECS_UI_WINDOW_DEPENDENT_BOTH
		};

		struct UIConfigAbsoluteTransform {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_ABSOLUTE_TRANSFORM;
			}

			float2 position;
			float2 scale;
		};

		struct UIConfigRelativeTransform {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_RELATIVE_TRANSFORM;
			}

			float2 offset = { 0.0f, 0.0f };
			float2 scale = { 1.0f, 1.0f };
		};

		struct UIConfigWindowDependentSize {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_WINDOW_DEPENDENT_SIZE;
			}

			ECS_UI_WINDOW_DEPENDENT_SIZE type = ECS_UI_WINDOW_DEPENDENT_HORIZONTAL;
			float2 offset = { 0.0f, 0.0f };
			float2 scale_factor = { 1.0f, 1.0f };
		};

		struct UIConfigTextParameters {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_PARAMETERS;
			}

			Color color = ECS_TOOLS_UI_TEXT_COLOR;
			float2 size = { 0.0f, 0.0f };
			float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING;
		};

		struct UIConfigTextAlignment {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_ALIGNMENT;
			}

			ECS_UI_ALIGN horizontal = ECS_UI_ALIGN_MIDDLE;
			ECS_UI_ALIGN vertical = ECS_UI_ALIGN_MIDDLE;
		};

		struct UIConfigColor {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COLOR;
			}

			Color color = ECS_TOOLS_UI_THEME_COLOR;
		};

		struct UIConfigRectangleHoverable {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_RECTANGLE_HOVERABLE_ACTION;
			}

			UIActionHandler handler;
		};

		struct UIConfigRectangleClickable {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_RECTANGLE_CLICKABLE_ACTION;
			}

			UIActionHandler handlers[3] = { {} };
			ECS_MOUSE_BUTTON button_types[ECS_COUNTOF(handlers)] = { ECS_MOUSE_LEFT };
		};

		struct UIConfigRectangleGeneral {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_RECTANGLE_GENERAL_ACTION;
			}

			UIActionHandler handler;
		};

		struct UIConfigButtonHoverable {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_BUTTON_HOVERABLE;
			}

			UIActionHandler handler;
		};

		struct UIConfigSliderParameters {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_PARAMETERS;
			}

			Color color = ToneColor(ECS_TOOLS_UI_THEME_COLOR, ECS_TOOLS_UI_SLIDER_LIGHTEN_FACTOR);
			float2 shrink = { ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_X, ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_Y };
			float padding = ECS_TOOLS_UI_LABEL_HORIZONTAL_PADD;
			float length = ECS_TOOLS_UI_SLIDER_LENGTH_X;
			// If enabled, the mouse will wrap around the screen X axis
			bool wrap_mouse = true;
		};

		struct UIConfigBorder {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_BORDER;
			}

			Color color;
			float thickness = ECS_TOOLS_UI_DOCKSPACE_BORDER_SIZE;
			bool is_interior = false;
			ECS_UI_DRAW_PHASE draw_phase = ECS_UI_DRAW_LATE;
		};

		struct ECSENGINE_API UIDrawerTextElement {
			float2* TextPosition();
			float2* TextScale();
			UISpriteVertex* TextBuffer();
			unsigned int* TextSize();
			CapacityStream<UISpriteVertex>* TextStream();

			float GetLowestX() const;
			float GetLowestY() const;
			float2 GetLowest() const;

			float2 GetZoom() const;
			float2 GetInverseZoom() const;
			float GetZoomX() const;
			float GetZoomY() const;
			float GetInverseZoomX() const;
			float GetInverseZoomY() const;

			void SetZoomFactor(float2 zoom);

			CapacityStream<UISpriteVertex> text_vertices;
			float2 position;
			float2 scale;
			float2 zoom;
			float2 inverse_zoom;
		};

		struct UIDrawerSliderFunctions {
			UIDrawerSliderInterpolate interpolate;
			UIDrawerSliderFromFloat from_float;
			UIDrawerSliderToFloat to_float;
			UIDrawerSliderPercentage percentage;
			UIDrawerSliderToString to_string;
			UIDrawerSliderConvertTextInput convert_text_input;
			UIDrawerSliderIsSmaller is_smaller;
			// Currently used by to_string
			void* extra_data;
		};

		template<typename Type>
		void UIDrawerSliderInterpolateImplementation(const void* _lower_bound, const void* _upper_bound, void* _value, float percentage) {
			const Type* lower_bound = (const Type*)_lower_bound;
			const Type* upper_bound = (const Type*)_upper_bound;
			Type* value = (Type*)_value;

			*value = *lower_bound + (*upper_bound - *lower_bound) * percentage;
		}

		template<typename Type>
		float UIDrawerSliderPercentageImplementation(const void* _lower_bound, const void* _upper_bound, const void* _value) {
			const Type* lower_bound = (const Type*)_lower_bound;
			const Type* upper_bound = (const Type*)_upper_bound;
			const Type* value = (const Type*)_value;

			return (float)(*value - *lower_bound) / (float)(*upper_bound - *lower_bound);
		}

		template<typename Type>
		bool UIDrawerSliderIsSmallerImplementation(const void* _left, const void* _right) {
			const Type* left = (const Type*)_left;
			const Type* right = (const Type*)_right;
			return *left < *right;
		}

		template<typename Integer>
		UIDrawerSliderFunctions UIDrawerGetIntSliderFunctions() {
			UIDrawerSliderFunctions result;

			auto convert_text_input = [](CapacityStream<char>& characters, void* _value) {
				bool dummy;
				Integer character_value = ConvertCharactersToIntImpl<Integer, char, false>(characters, dummy);
				Integer* value = (Integer*)_value;
				*value = character_value;
			};

			auto to_string = [](CapacityStream<char>& characters, const void* _value, void* extra_data) {
				characters.size = 0;
				ConvertIntToChars(characters, *(const Integer*)_value);
			};

			auto from_float = [](void* value, float float_percentage) {
				Integer* integer_value = (Integer*)value;
				Integer min, max;
				IntegerRange<Integer>(min, max);
				int64_t min_64 = min;
				int64_t max_64 = max;

				int64_t percentage_64 = (int64_t)float_percentage;
				*integer_value = Clamp(percentage_64, min_64, max_64);
			};

			auto to_float = [](const void* value) {
				return (float)(*(Integer*)value);
			};

			result.convert_text_input = convert_text_input;
			result.to_string = to_string;
			result.extra_data = nullptr;
			result.interpolate = UIDrawerSliderInterpolateImplementation<Integer>;
			result.is_smaller = UIDrawerSliderIsSmallerImplementation<Integer>;
			result.percentage = UIDrawerSliderPercentageImplementation<Integer>;
			result.from_float = from_float;
			result.to_float = to_float;

			return result;
		}

		ECSENGINE_API UIDrawerSliderFunctions UIDrawerGetFloatSliderFunctions(unsigned int& precision);

		ECSENGINE_API UIDrawerSliderFunctions UIDrawerGetDoubleSliderFunctions(unsigned int& precision);

		struct UIConfigTextInputHint {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_INPUT_HINT;
			}

			const char* characters;
		};

		struct UIConfigTextInputCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_INPUT_CALLBACK;
			}

			UIActionHandler handler;
			bool copy_on_initialization = false;
			bool trigger_only_on_release = false;
		};

		struct UIConfigSliderChangedValueCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;
			}

			UIActionHandler handler;
			bool copy_on_initialization = false;
		};

		struct UIConfigRectangleVertexColor {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_RECTANGLE_VERTEX_COLOR;
			}

			Color colors[4];
		};

		struct UIConfigSpriteGradient {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_SPRITE_GRADIENT;
			}

			const wchar_t* texture;
			float2 top_left_uv;
			float2 bottom_right_uv;
		};

		struct UIDrawerCollapsingHeader {
			UIDrawerTextElement name;
			bool state;
		};

		struct UIDrawerSentenceNotCached {
			CapacityStream<unsigned int> whitespace_characters;
		};

		struct UIDrawerWhitespaceCharacter {
			unsigned short position;
			char type;
		};

		struct ECSENGINE_API UIDrawerSentenceBase {
			void SetWhitespaceCharacters(Stream<char> characters, char parse_token = ' ');

			CapacityStream<UISpriteVertex> vertices;
			CapacityStream<UIDrawerWhitespaceCharacter> whitespace_characters;
		};

		struct UIDrawerSentenceCached {
			UIDrawerSentenceBase base;
			float2 zoom;
			float2 inverse_zoom;
		};

		struct UIDrawerSentenceFitSpaceToken {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_SENTENCE_FIT_SPACE_TOKEN;
			}

			char token;
			bool keep_token = true;
		};

		struct UIDrawerTextTable {
			Stream<Stream<UISpriteVertex>> labels;
			Stream<float> column_scales;
			float row_scale;
			float2 zoom;
			float2 inverse_zoom;
		};

		struct UIConfigGraphKeepResolutionX {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_KEEP_RESOLUTION_X;
			}

			float max_y_scale = 10000.0f;
			float min_y_scale = 0.2f;
		};

		struct UIConfigGraphKeepResolutionY {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_KEEP_RESOLUTION_Y;
			}

			float max_x_scale = 10000.0f;
			float min_x_scale = 0.2f;
		};

		struct UIConfigGraphColor {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_LINE_COLOR;
			}

			Color color;
		};

		struct UIConfigHistogramColor {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_HISTOGRAM_COLOR;
			}

			Color color;
		};

		struct UIDrawerGraphTagData {
			void* drawer;
			float2 position;
			float2 graph_position;
			float2 graph_scale;
		};

		struct UIConfigGraphMinY {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_MIN_Y;
			}

			float value;
		};

		struct UIConfigGraphMaxY {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_MAX_Y;
			}

			float value;
		};

		struct UIConfigGraphClickable {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_CLICKABLE;
			}

			UIActionHandler handler;
			UIHandlerCopyBuffers copy_function = nullptr;
			ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT;
		};

		// You need to draw using the tags. They will receive UIDrawerGraphTagData as data
		struct UIConfigGraphTags {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_TAGS;
			}

			Action vertical_tags[4];
			Action horizontal_tags[4];
			float vertical_values[4];
			float horizontal_values[4];
			unsigned int vertical_tag_count = 0;
			unsigned int horizontal_tag_count = 0;
		};

		struct UIConfigGraphInfoLabels {
			enum CORNERS : unsigned char {
				TOP_LEFT,
				TOP_RIGHT,
				BOTTOM_LEFT,
				BOTTOM_RIGHT
			};

			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_INFO_LABELS;
			}

			ECS_INLINE void SetTopLeft(Stream<char> label) {
				labels[TOP_LEFT] = label;
			}

			ECS_INLINE void SetTopRight(Stream<char> label) {
				labels[TOP_RIGHT] = label;
			}

			ECS_INLINE void SetBottomLeft(Stream<char> label) {
				labels[BOTTOM_LEFT] = label;
			}

			ECS_INLINE void SetBottomRight(Stream<char> label) {
				labels[BOTTOM_RIGHT] = label;
			}

			ECS_INLINE Stream<char> GetTopLeft() const {
				return labels[TOP_LEFT];
			}

			ECS_INLINE Stream<char> GetTopRight() const {
				return labels[TOP_RIGHT];
			}

			ECS_INLINE Stream<char> GetBottomLeft() const {
				return labels[BOTTOM_LEFT];
			}

			ECS_INLINE Stream<char> GetBottomRight() const {
				return labels[BOTTOM_RIGHT];
			}

			Stream<char> labels[4] = {};
			// If set to true, then the text color will be set to this one
			bool inherit_line_color = false;
		};

		struct UIDrawerMenuWindow {
			Stream<char> name;
			float2 position;
			float2 scale;
		};

		struct UIDrawerMenuState {
			//  ------------------------------- User modifiable -------------------------
			Stream<char> left_characters;
			// The click handlers receive in the additional data the Stream<cbar>
			// With the row's characters
			UIActionHandler* click_handlers;
			unsigned short row_count;

			Stream<char> right_characters = { nullptr, 0 };
			bool* unavailables = nullptr;
			bool* row_has_submenu = nullptr;
			UIDrawerMenuState* submenues = nullptr;
			
			unsigned char separation_line_count = 0;
			unsigned char separation_lines[11];
			//  ------------------------------- User modifiable -------------------------

			// ----------------------------------- Reserved -----------------------------
			unsigned short* left_row_substreams;
			unsigned short* right_row_substreams;
			CapacityStream<UIDrawerMenuWindow>* windows;
			unsigned short submenu_index;
			// ----------------------------------- Reserved -----------------------------
		};

		ECSENGINE_API Stream<char> GetUIDrawerMenuStateRowString(const UIDrawerMenuState* state, unsigned int row_index);

		ECSENGINE_API size_t UIMenuCalculateStateMemory(const UIDrawerMenuState* state, bool copy_states);

		ECSENGINE_API size_t UIMenuWalkStatesMemory(const UIDrawerMenuState* state, bool copy_states);

		ECSENGINE_API void UIMenuSetStateBuffers(
			UIDrawerMenuState* state,
			uintptr_t& buffer,
			CapacityStream<UIDrawerMenuWindow>* stream,
			unsigned int submenu_index,
			bool copy_states
		);

		ECSENGINE_API void UIMenuWalkSetStateBuffers(
			UIDrawerMenuState* state,
			uintptr_t& buffer,
			CapacityStream<UIDrawerMenuWindow>* stream,
			unsigned int submenu_index,
			bool copy_states
		);

		struct UIDrawerMenu {
			ECS_INLINE bool IsTheSameData(const UIDrawerMenu* other) const {
				return other != nullptr && name == other->name;
			}

			UIDrawerTextElement* name;
			bool is_opened;
			CapacityStream<UIDrawerMenuWindow> windows;
			UIDrawerMenuState state;
		};

		struct UIDrawerMenuGeneralData {
			UIDrawerMenu* menu;
			UIDrawerMenuState* destroy_state = nullptr;
			float2 destroy_position;
			float2 destroy_scale;
			unsigned char menu_initializer_index = 255;
			bool initialize_from_right_click = false;
			bool is_opened_when_clicked = false;
			// This is the name of the window that created us
			// To restore the active location to it once the click is finished
			char parent_window_name[64];
			char parent_window_name_size = 0;
		};

		struct ECSENGINE_API UIDrawerMenuDrawWindowData {
			UIDrawerMenuState* GetState() const;

			unsigned char GetLastPosition() const;

			UIDrawerMenu* menu;
			// In this way we get reliable UIDrawerMenuState* even when the
			// underlying menu is changed
			unsigned char submenu_offsets[8];
			// This is the name of the window that spawned the menu
			Stream<char> parent_window_name;
		};


		struct UIDrawerSubmenuHoverable {
			ECS_INLINE bool IsTheSameData(const UIDrawerSubmenuHoverable* other) const {
				return other != nullptr && draw_data.GetState() == other->draw_data.GetState() && row_index == other->row_index;
			}

			UIDrawerMenuDrawWindowData draw_data;
			unsigned int row_index;
		};

		template<size_t sample_count>
		struct UIDrawerMultiGraphSample {
			ECS_INLINE float GetX() const {
				return x;
			}

			ECS_INLINE float GetY(unsigned int index) const {
				return y[index];
			}

			float x;
			float y[sample_count];
		};

		using UIDrawerMultiGraphSample2 = UIDrawerMultiGraphSample<2>;
		using UIDrawerMultiGraphSample3 = UIDrawerMultiGraphSample<3>;
		using UIDrawerMultiGraphSample4 = UIDrawerMultiGraphSample<4>;
		using UIDrawerMultiGraphSample5 = UIDrawerMultiGraphSample<5>;
		using UIDrawerMultiGraphSample6 = UIDrawerMultiGraphSample<6>;
		using UIDrawerMultiGraphSample7 = UIDrawerMultiGraphSample<7>;

		struct UIConfigColorInputCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COLOR_INPUT_CALLBACK;
			}
			
			UIActionHandler callback;
			UIActionHandler final_callback = { nullptr };
		};

		struct UIConfigColorFloatCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COLOR_FLOAT_CALLBACK;
			}

			UIActionHandler callback;
		};

		enum ECS_UI_DRAWER_RETURN_TO_DEFAULT_DESCRIPTOR_TYPE : unsigned char {
			ECS_UI_DRAWER_RETURN_TO_DEFAULT_DESCRIPTOR_COLOR_THEME,
			ECS_UI_DRAWER_RETURN_TO_DEFAULT_DESCRIPTOR_LAYOUT,
			ECS_UI_DRAWER_RETURN_TO_DEFAULT_DESCRIPTOR_FONT,
			ECS_UI_DRAWER_RETURN_TO_DEFAULT_DESCRIPTOR_ELEMENT,
			ECS_UI_DRAWER_RETURN_TO_DEFAULT_DESCRIPTOR_MATERIAL,
			ECS_UI_DRAWER_RETURN_TO_DEFAULT_DESCRIPTOR_MISC,
			ECS_UI_DRAWER_RETURN_TO_DEFAULT_DESCRIPTOR_DOCKSPACE
		};

		struct UIParameterWindowReturnToDefaultButtonData {
			UIWindowDrawerDescriptor* window_descriptor;
			void* system_descriptor;
			void* default_descriptor;
			bool is_system_theme;
			ECS_UI_WINDOW_DRAWER_DESCRIPTOR_INDEX descriptor_index;
			unsigned int descriptor_size;
		};

		struct UIConfigToolTip {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_TOOL_TIP;
			}

			Stream<char> characters;
			// Optimization flag to avoid some copies
			bool is_stable = false;
		};

		struct UIDrawerStateTable {
			UIDrawerTextElement name;
			Stream<UIDrawerTextElement> labels;
			unsigned int max_x_scale;
			UIActionHandler clickable_callback;
		};

		struct ECSENGINE_API UIDrawerMenuButtonData {
			// Embedds after it all the data for the actions and the name
			// Returns the total write size. The storage capacity parameter is the total capacity of the stack memory where this is located
			unsigned int Write(size_t storage_capacity) const;

			// Retrieves all the data for the actions (when relative offsets are in place)
			void Read();

			UIWindowDescriptor descriptor;
			size_t border_flags;
			bool is_opened_when_pressed;
		};

		struct UIDrawerFilterMenuData {
			Stream<char> window_name;
			Stream<char> name;
			Stream<Stream<char>> labels;
			bool** states;
			bool draw_all;
			bool* notifier;
		};

		struct UIDrawerFilterMenuSinglePointerData {
			Stream<char> window_name;
			Stream<char> name;
			Stream<Stream<char>> labels;
			bool* states;
			bool draw_all;
			bool* notifier;
		};

		struct UIDrawerBufferState {
			size_t solid_color_count;
			size_t text_sprite_count;
			size_t sprite_count;
			size_t sprite_cluster_count;
			size_t line_count;
		};

		struct UIDrawerHandlerState {
			size_t hoverable_count;
			size_t clickable_count[ECS_MOUSE_BUTTON_COUNT];
			size_t general_count;
		};

		struct UIDrawerClipState {
			UIDrawerBufferState buffer_state;
			UIDrawerHandlerState handler_state;
			// These 2 fields are the original window values for the min and max limits.
			// When activating clipping, we temporarly hijack these to make the elements
			// Aware that they should not render outside these bounds
			float2 min_region_limit;
			float2 max_region_limit;

			UIElementTransform clip_region;
			size_t configuration;
		};

		struct UIConfigStateTableNotify {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE;
			}

			bool* notifier;
		};

		typedef UIConfigStateTableNotify UIConfigFilterMenuNotify;

		struct UIConfigStateTableCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_STATE_TABLE_CALLBACK;
			}

			UIActionHandler handler;
			bool copy_on_initialization = false;
		};

		struct UIDrawerStateTableBoolClickable {
			UIDrawerStateTable* state_table;
			bool* notifier;
			bool* state;
		};

		struct UIConfigComboBoxPrefix {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COMBO_BOX_PREFIX;
			}

			Stream<char> prefix;
		};

		struct UIDrawerFilesystemHierarchyLabelData {
			bool state;
			unsigned char activation_count;
		};

		struct UIConfigFilesystemHierarchyHashTableCapacity {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_FILESYSTEM_HIERARCHY_HASH_TABLE_CAPACITY;
			}

			unsigned int capacity;
		};

		struct UIDrawerFilesystemHierarchy {
			CapacityStream<char> active_label;
			HashTableDefault<UIDrawerFilesystemHierarchyLabelData> label_states;
			Action selectable_callback;
			void* selectable_callback_data;
			Action right_click_callback;
			void* right_click_callback_data;
		};

		struct UIConfigLabelHierarchySpriteTexture {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_SPRITE_TEXTURE;
			}

			const wchar_t* opened_texture;
			const wchar_t* closed_texture;
			float2 opened_texture_top_left_uv = {0.0f, 0.0f};
			float2 closed_texture_top_left_uv = {0.0f, 0.0f};
			float2 opened_texture_bottom_right_uv = { 1.0f, 1.0f };
			float2 closed_texture_bottom_right_uv = { 1.0f, 1.0f };
			Color opened_color = ECS_COLOR_WHITE;
			Color closed_color = ECS_COLOR_WHITE;
			float2 expand_factor = { 1.0f, 1.0f };
			bool keep_triangle = true;
		};

		typedef UIConfigLabelHierarchySpriteTexture UIConfigFilesystemHierarchySpriteTexture;

		// The callback receives the label char* in the additional action data member; if copy_on_initialization is set,
		//  then the data parameter will be copied only in the initializer pass
		struct UIConfigLabelHierarchySelectableCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK;
			}

			Action callback;
			void* data;
			unsigned int data_size = 0;
			ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
			bool copy_on_initialization = false;
		};

		typedef UIConfigLabelHierarchySelectableCallback UIConfigFilesystemHierarchySelectableCallback;

		// The callback must cast to UIDrawerLabelHierarchyRightClickData the _data pointer in order to get access to the 
		// label stream; if copy_on_initialization is set, then the data parameter will be copied only in the initializer pass
		struct UIConfigLabelHierarchyRightClick {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK;
			}

			Action callback;
			void* data;
			unsigned int data_size = 0;
			ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
			bool copy_on_initialization = false;
		};

		typedef UIConfigLabelHierarchyRightClick UIConfigFilesystemHierarchyRightClick;

		struct UIConfigLabelHierarchyDragCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_DRAG_LABEL;
			}

			Action callback;
			void* data;
			unsigned int data_size = 0;
			ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
			bool copy_on_initialization = false;
			bool reject_same_label_drag = true;
			bool call_on_mouse_hold = false;
		};

		struct UIConfigLabelHierarchyRenameCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_RENAME_LABEL;
			}

			Action callback;
			void* data;
			unsigned int data_size = 0;
			ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
			bool copy_on_initialization = false;
		};

		// Needs to take as data parameter a UIDrawerLabelHierarchyDoubleClickData
		struct UIConfigLabelHierarchyDoubleClickCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_DOUBLE_CLICK_ACTION;
			}

			Action callback;
			void* data;
			unsigned int data_size = 0;
			ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
			bool copy_on_initialization = false;
		};

		struct UIConfigLabelHierarchyFilter {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_FILTER;
			}

			Stream<char> filter;
		};

		// Any handler can be omitted
		struct UIConfigLabelHierarchyBasicOperations {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_BASIC_OPERATIONS;
			}

			UIActionHandler copy_handler = { nullptr };
			UIActionHandler cut_handler = { nullptr };
			UIActionHandler delete_handler = { nullptr };
			bool trigger_when_window_is_focused_only = true;
		};

		struct UIConfigLabelHierarchyMonitorSelection;

		struct UIConfigLabelHierarchyMonitorSelectionInfo {
			UIConfigLabelHierarchyMonitorSelection* monitor_selection;
			// Use AsIs to cast the data to the appropriate format.
			// If using strings, then cast to Stream<char>, else to your own blittable type
			// You can reorder these however you like, you can leave 0 elements or you can remove elements
			// You can even override the selection but you must do this by using the
			// override labels field - this field must not be overriden (because it might need to be deallocated)
			Stream<void> selected_labels;
			Stream<void> override_labels = {};
		};

		// The callback receives as parameter in the additional_data field a structure of type
		// UIConfigLabelHierarchyMonitorSelectionInfo that it can use to alter the behaviour
		struct ECSENGINE_API UIConfigLabelHierarchyMonitorSelection {
			ECS_INLINE UIConfigLabelHierarchyMonitorSelection() {}

			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_MONITOR_SELECTION;
			}

			ECS_INLINE AllocatorPolymorphic Allocator() const {
				return is_capacity_selection ? capacity_allocator : resizable_selection->allocator;
			}

			ECS_INLINE Stream<void> Selection() const {
				return is_capacity_selection ? *capacity_selection : resizable_selection->ToStream();
			}

			ECS_INLINE Stream<Stream<char>> SelectionStrings() const {
				return Selection().AsIs<Stream<char>>();
			}

			ECS_INLINE bool ShouldUpdate() const {
				return boolean_changed_flag ? *is_changed : (*is_changed_counter) > 0;
			}

			void Deallocate(bool is_blittable);

			// This callback will be called
			UIActionHandler callback = {};
			bool is_capacity_selection;
			bool boolean_changed_flag;
			union {
				bool* is_changed;
				unsigned char* is_changed_counter;
			};
			union {
				struct {
					CapacityStream<void>* capacity_selection;
					AllocatorPolymorphic capacity_allocator;
				};
				ResizableStream<void>* resizable_selection;
			};
		};

		// One of the texture or resource view must be specified
		struct UIConfigMenuButtonSprite {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_MENU_BUTTON_SPRITE;
			}

			Stream<wchar_t> texture = { nullptr, 0 };
			ResourceView resource_view = nullptr;
			float2 top_left_uv = { 0.0f, 0.0f };
			float2 bottom_right_uv = { 1.0f, 1.0f };
			Color color = ECS_COLOR_WHITE;
		};

		typedef UIConfigMenuButtonSprite UIConfigMenuSprite;

		struct UIConfigActiveState {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_ACTIVE_STATE;
			}

			bool state;
			char reserved[7];
		};

		struct UIDrawerInitializeComboBox {
			UIDrawConfig* config;
			Stream<char> name;
			Stream<Stream<char>> labels;
			unsigned int label_display_count;
			unsigned char* active_label;
		};

		struct UIDrawerInitializeColorInput {
			UIDrawConfig* config;
			Stream<char> name;
			Color* color;
			Color default_color;
		};

		struct UIDrawerInitializeFilterMenu {
			UIDrawConfig* config;
			Stream<char> name;
			Stream<Stream<char>> label_names;
			bool** states;
		};

		struct UIDrawerInitializeFilterMenuSinglePointer {
			UIDrawConfig* config;
			Stream<char> name;
			Stream<Stream<char>> label_names;
			bool* states;
		};

		struct UIDrawerInitializeHierarchy {
			UIDrawConfig* config;
			Stream<char> name;
		};

		struct UIDrawerInitializeFilesystemHierarchy {
			UIDrawConfig* config;
			Stream<char> identifier;
			Stream<Stream<char>> labels;
		};

		struct UIDrawerInitializeList {
			UIDrawConfig* config;
			Stream<char> name;
		};

		struct UIDrawerInitializeMenu {
			UIDrawConfig* config;
			Stream<char> name;
			UIDrawerMenuState* state;
		};

		struct UIDrawerInitializeFloatInput {
			UIDrawConfig* config;
			Stream<char> name; 
			float* value; 
			float default_value = 0.0f;
			float lower_bound = -FLT_MAX;
			float upper_bound = FLT_MAX;
		};

		template<typename Integer>
		struct UIDrawerInitializeIntegerInput {
			UIDrawConfig* config; 
			Stream<char> name; 
			Integer* value;
			Integer default_value = 0;
			Integer min = LLONG_MIN;
			Integer max = ULLONG_MAX;
		};

		struct UIDrawerInitializeDoubleInput {
			UIDrawConfig* config;
			Stream<char> name; 
			double* value; 
			double default_value = 0;
			double lower_bound = -DBL_MAX; 
			double upper_bound = DBL_MAX;
		};

		struct UIDrawerInitializeFloatInputGroup {
			UIDrawConfig* config;
			size_t count;
			Stream<char> group_name;
			Stream<char>* names;
			float** values;
			const float* default_values;
			const float* lower_bound;
			const float* upper_bound;
		};

		struct UIDrawerInitializeDoubleInputGroup {
			UIDrawConfig* config;
			size_t count;
			Stream<char> group_name;
			Stream<char>* names;
			double** values;
			const double* default_values;
			const double* lower_bound;
			const double* upper_bound;
		};

		template<typename Integer>
		struct UIDrawerInitializeIntegerInputGroup {
			UIDrawConfig* config;
			size_t count;
			Stream<char> group_name;
			Stream<char>* names;
			Integer** values;
			const Integer* default_values;
			const Integer* lower_bound;
			const Integer* upper_bound;
		};

		struct UIDrawerInitializeSlider {
			UIDrawConfig* config;
			Stream<char> name;
			unsigned int byte_size;
			void* value_to_modify;
			const void* lower_bound;
			const void* upper_bound;
			const void* default_value;
			const UIDrawerSliderFunctions* functions;
			UIDrawerTextInputFilter filter;
		};

		struct UIDrawerInitializeSliderGroup {
			UIDrawConfig* config;
			size_t count;
			Stream<char> group_name;
			Stream<char>* names;
			unsigned int byte_size;
			void** values_to_modify;
			const void* lower_bounds;
			const void* upper_bounds;
			const void* default_values;
			const UIDrawerSliderFunctions* functions;
			UIDrawerTextInputFilter filter;
		};

		struct UIDrawerInitializeStateTable {
			UIDrawConfig* config;
			Stream<char> name;
			Stream<Stream<char>> labels;
			bool** states;
		};

		struct UIDrawerInitializeStateTableSinglePointer {
			UIDrawConfig* config;
			Stream<char> name;
			Stream<Stream<char>> labels;
			bool* states;
		};

		struct UIDrawerInitializeTextInput {
			UIDrawConfig* config;
			Stream<char> name;
			union {
				CapacityStream<char>* text_to_fill;
				ResizableStream<char>* resizable_text_to_fill;
			};
		};

		struct UIDrawerInitializeTextInputWide {
			UIDrawConfig* config;
			Stream<char> name;
			union {
				CapacityStream<wchar_t>* text_to_fill;
				ResizableStream<wchar_t>* resizable_text_to_fill;
			};
		};

		struct UIDrawerInitializePathInput {
			ECS_INLINE UIDrawerInitializePathInput() {}

			UIDrawConfig* config;
			Stream<char> name;
			union {
				CapacityStream<wchar_t>* capacity_characters;
				struct {
					Stream<wchar_t>* stream_characters;
					AllocatorPolymorphic allocator;
				};
			};
			Stream<Stream<wchar_t>> extensions;
		};

		struct UIDrawerInitializeLabelHierarchy {
			const UIDrawConfig* config;
			Stream<char> name;
			unsigned int storage_type_size;
		};

		struct UIDrawerTimeline;

		struct UIDrawerInitializeTimeline {
			const UIDrawConfig* config;
			Stream<char> name;
			const UIDrawerTimeline* timeline;
		};

		struct UIDrawerLabelList {
			UIDrawerTextElement name;
			Stream<UIDrawerTextElement> labels;
		};

		struct UIConfigCollapsingHeaderSelection {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COLLAPSING_HEADER_SELECTION;
			}

			bool is_selected;
		};

		union UIConfigCollapsingHeaderButtonData {
			ECS_INLINE UIConfigCollapsingHeaderButtonData() {}

			// Image based (both button and display only)
			struct {
				Stream<wchar_t> image_texture;
				Color image_color;
			};
			// Check box.
			struct {
				bool* check_box_flag;
			};
			// Menu
			struct {
				UIDrawerMenuState* menu_state;
				Stream<wchar_t> menu_texture;
				Color menu_texture_color;
				bool menu_copy_states;
			};
			// Menu general
			struct {
				UIWindowDescriptor* menu_general_descriptor;
				Stream<wchar_t> menu_general_texture;
				Color menu_general_texture_color;
				size_t border_flags;
			};
		};

		enum UIConfigCollapsingHeaderButtonType : unsigned char {
			ECS_UI_COLLAPSING_HEADER_BUTTON_IMAGE_DISPLAY, // Only displays, no action
			ECS_UI_COLLAPSING_HEADER_BUTTON_IMAGE_BUTTON,
			ECS_UI_COLLAPSING_HEADER_BUTTON_CHECK_BOX,
			ECS_UI_COLLAPSING_HEADER_BUTTON_MENU, // Default menu, the one where you can have submenues and list which labels can be choosen
			ECS_UI_COLLAPSING_HEADER_BUTTON_MENU_GENERAL // The one where you give a window descriptor to be spawned
		};

		struct UIConfigCollapsingHeaderButton {
			UIConfigCollapsingHeaderButtonData data;
			UIConfigCollapsingHeaderButtonType type;
			UIActionHandler handler = { nullptr }; // The handler can be missing for check box, menu or menu general. It is needed only for image button or check box
			ECS_UI_ALIGN alignment = ECS_UI_ALIGN_LEFT; // Can be only left or right aligned
			bool is_enabled = true;
		};

		// There can be at max 6 buttons
		struct UIConfigCollapsingHeaderButtons {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COLLAPSING_HEADER_BUTTONS;
			}

			Stream<UIConfigCollapsingHeaderButton> buttons;
		};

		struct UIConfigCollapsingHeaderCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COLLAPSING_HEADER_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct UIConfigGetTransform {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_GET_TRANSFORM;
			}

			float2* position;
			float2* scale;
		};

		// Phase will always be UIDrawPhase::System
		struct UIConfigComboBoxCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COMBO_BOX_CALLBACK;
			}

			UIActionHandler handler;
			bool copy_on_initialization = false;
		};

		// The mapping can be any arbitrary blittable type
		// The mapping values will be written into the given pointer instead of the normal indices
		// If the stable bool is true, then it will reference the given mapping instead of copying it
		struct UIConfigComboBoxMapping {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COMBO_BOX_MAPPING;
			}

			bool stable = false;
			unsigned short byte_size;
			void* mappings;
		};

		// Marks certain values as unavailable
		struct UIConfigComboBoxUnavailable {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COMBO_BOX_UNAVAILABLE;
			}

			bool stable = false;
			bool* unavailables;
		};

		struct UIConfigComboBoxDefault {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COMBO_BOX_DEFAULT;
			}

			bool is_pointer_value = false;
			bool mapping_value_stable = false;
			union {
				unsigned char char_value;
				unsigned char* char_pointer;
				void* mapping_value;
			};
		};

		struct UIDrawerInitializeArrayElementData {
			UIDrawConfig* config;
			Stream<char> name;
			size_t element_count;
		};

		struct UIDrawerArrayData {
			bool collapsing_header_state;
			bool drag_is_released;
			// Having a boolean flag here makes it easy for all callbacks to change
			// This and have a snapshot runnable redraw the window
			bool should_redraw;
			ECS_UI_DRAW_PHASE add_callback_phase;
			ECS_UI_DRAW_PHASE remove_callback_phase;
			ECS_UI_DRAW_PHASE remove_anywhere_callback_phase;
			unsigned int drag_index;
			float drag_current_position;
			float row_y_scale;
			unsigned int previous_element_count;
			Action add_callback;
			void* add_callback_data;
			Action remove_callback;
			void* remove_callback_data;
			Action remove_anywhere_callback;
			void* remove_anywhere_callback_data;
			unsigned int remove_anywhere_index;
		};

		struct UIDrawerArrayDrawData {
			void* element_data;
			void* additional_data;
			size_t configuration;
			UIDrawConfig* config;
			unsigned int current_index;
		};

		struct UIDrawerInitializeColorFloatInput {
			UIDrawConfig* config;
			Stream<char> name;
			ColorFloat* color;
			ColorFloat default_color;
		};

		// In the additional data field it will receive a pointer to
		// UIDrawerArrayAddRemoveData* and can use it to do initialization 
		// of new elements or some other tasks. In the UIDrawerArrayAddRemoveData* the new size 
		// will actually be the old size. The new size can be looked into the streams
		struct UIConfigArrayAddCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_ARRAY_ADD_CALLBACK;
			}

			UIActionHandler handler;
			bool copy_on_initialization = false;
		};

		// In the additional data field it will receive a pointer to
		// UIDrawerArrayAddRemoveData* and can use it to do initialization 
		// of new elements or some other tasks. In the UIDrawerArrayAddRemoveData* the new size 
		// will actually be the old size. The new size can be looked into the streams
		struct UIConfigArrayRemoveCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_ARRAY_REMOVE_CALLBACK;
			}

			UIActionHandler handler;
			bool copy_on_initialization = false;
		};

		// Overwrite color black means ignore it - use the theme color
		// The scale factor is used to increase or reduce the scale of the
		// background compared to that of the sprite
		struct UIConfigSpriteButtonBackground {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_SPRITE_BUTTON_BACKGROUND;
			}

			float2 scale_factor = { 1.2f, 1.2f };
			Color overwrite_color = ECS_COLOR_BLACK;
		};

		// If the colors is not specified, then it will just use ECS_COLOR_WHITE
		struct UIConfigSpriteButtonMultiTexture {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_SPRITE_BUTTON_MULTI_TEXTURE;
			}

			Stream<Stream<wchar_t>> textures = { nullptr, 0 };
			Stream<ResourceView> resource_views = { nullptr, 0 };
			Color* texture_colors = nullptr;
			Color* resource_view_colors = nullptr;
			bool draw_before_main_texture = false;
		};

		// The callback receives the flag in the _additional_data field
		struct UIConfigCheckBoxCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_CHECK_BOX_CALLBACK;
			}
		
			UIActionHandler handler;
			// When this value is set to true, it means that the boolean
			// Value is not changed when a click is triggered, only this callback
			// Will be called
			bool disable_value_to_modify = false;
		};

		struct UIConfigCheckBoxDefault {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_CHECK_BOX_DEFAULT;
			}

			bool is_pointer_variable = false;
			union {
				bool value;
				bool* pointer_variable;
			};
		};

		// With this config the path input can be restricted to only paths that start from the given roots
		struct UIConfigPathInputRoot {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_PATH_INPUT_ROOT;
			}

			Stream<Stream<wchar_t>> roots;
		};

		// Can choose between directly giving the files or giving a callback that will be called
		// when the user presses the folder button
		// The callback will receive as parameter in the additional_data a ResizableStream<Stream<wchar_t>>*
		// in order to make visible to the external function the files. The strings must be allocated
		// from that buffer such that they are visible to the outer scope
		struct UIConfigPathInputGiveFiles {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_PATH_INPUT_GIVE_FILES;
			}

			union {
				Stream<Stream<wchar_t>> files;
				UIActionHandler callback_handler;
			};
			bool is_callback = false;
		};

		struct UIDrawer;
		struct UIDrawerTextInput;

		// This is given through the additional_data field
		// Must update both the input and the path
		struct UIConfigPathInputCustomFilesystemDrawData {
			UIDrawer* drawer;
			UIDrawerTextInput* input;
			CapacityStream<wchar_t>* path;
			bool should_destroy;
		};

		// Provide an action which draws and receives the user input
		// The action receives in the additional_data field an UIConfigPathInputCustomFilesystemDrawData*
		// When finishing, it can either signal a flag or directly add a destroy window handler
		// with the index from the UIDrawer. Change the input in order to reflect the modification
		struct UIConfigPathInputCustomFilesystem {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_PATH_INPUT_CUSTOM_FILESYSTEM;
			}

			UIActionHandler callback;
			bool copy_on_initialization = false;
		};

		// Receives the previous path as CapacityStream<wchar_t>* to the additional_data parameter
		struct UIConfigPathInputCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_PATH_INPUT_CALLBACK;
			}

			UIActionHandler callback;
			bool copy_on_initialization = false;
			bool trigger_on_release = true;
		};

		struct UIConfigPathInputSpriteTexture {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_PATH_INPUT_SPRITE_TEXTURE;
			}

			const wchar_t* texture;
		};

		// If the omit_text is set to true, then it will just padd 
		struct UIConfigNamePadding {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_NAME_PADDING;
			}

			bool omit_text = false;
			ECS_UI_ALIGN alignment = ECS_UI_ALIGN_LEFT;
			float total_length = -1.0f;
			float offset_size = 0.0f;
 		};

		struct UIConfigSelectionInputOverrideHoverable {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_SELECTION_INPUT_OVERRIDE_HOVERABLE;
			}

			Stream<char> text;
			bool stable = false;
		};

		struct UIConfigSelectionInputLabelClickable {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_SELECTION_INPUT_LABEL_CLICKABLE;
			}

			bool double_click_action = false;
			size_t double_click_duration_between_clicks = 0;
			UIActionHandler first_click_handler = {};
			// This is the second click handler when using double click
			// And also the one taken into consideration for normal callback
			UIActionHandler handler;
		};

		struct UIConfigSliderMouseDraggable {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_MOUSE_DRAGGABLE;
			}

			bool interpolate_bounds = true;
		};

		struct UIConfigAlignElement {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_ALIGN_ELEMENT;
			}

			ECS_UI_ALIGN horizontal = ECS_UI_ALIGN_LEFT;
			ECS_UI_ALIGN vertical = ECS_UI_ALIGN_TOP;
		};

		// Can be used polymorphically. The basic functionality is that of a label but the hierarchy can store different types
		// such that when giving the data to all callbacks is easier for them to process which thing actually was selected, dragged etc
		// then trying to figure it out from the strings (if the strings are the same it might be impossible). There is a limitation tho:
		// This type needs to be blittable (stable if it has streams) otherwise it would add too much complexity to deep copy the data
		// All functions take the label as a pointer because the data is handled polymorphically and would be impossible to
		// take it by value. Pretty much wherever there is void you can replace it with Stream<char> to get the default behaviour
		// (Stream<void> -> Stream<Stream<char>>, const void* -> const Stream<char>*)
		struct ECSENGINE_API UIDrawerLabelHierarchyData {
			// If the action_data is nullptr, then it will skip the callback
			void AddSelection(const void* label, ActionData* action_data);

			void AddOpenedLabel(UISystem* system, const void* label);

			// If the action_data is nullptr, then it will skip the callback
			void ChangeSelection(const void* label, ActionData* action_data);

			void ChangeSelection(Stream<void> labels, ActionData* action_data);

			void ClearSelection(ActionData* action_data);

			bool CompareFirstLabel(const void* label) const;

			bool CompareLastLabel(const void* label) const;

			bool CompareLabels(const void* first, const void* second) const;

			// If the default char labels are used, it will return 16
			unsigned int CopySize() const;

			unsigned int DynamicIndex(const UISystem* system, unsigned int window_index) const;

			unsigned int FindSelectedLabel(const void* label) const;

			unsigned int FindOpenedLabel(const void* label) const;

			unsigned int FindCopiedLabel(const void* label) const;

			// For copy/cut
			void RecordSelection(ActionData* action_data);

			// If the action_data is nullptr, then it will skip the callback
			void RemoveSelection(const void* label, ActionData* action_data);

			void RemoveOpenedLabel(UISystem* system, const void* label);

			void ResetCopiedLabels(ActionData* action_data);

			void SetSelectionCut(bool is_cut = true);

			void SetFirstSelectedLabel(const void* label);

			void SetLastSelectedLabel(const void* label);

			void SetHoveredLabel(const void* label);

			void TriggerSelectable(ActionData* action_data);

			void TriggerCopy(ActionData* action_data);

			void TriggerCut(ActionData* action_data);
			
			void TriggerDelete(ActionData* action_data);

			void TriggerDoubleClick(ActionData* action_data);

			void TriggerDrag(ActionData* action_data, bool is_released);

			// It will check that the has_monitor_selection boolean before proceeding
			void UpdateMonitorSelection();

			Stream<void> opened_labels;
			CapacityStream<void> hovered_label;

			// We need to keep separate the renamed label because when renaming
			// and then clicking on another label it will apply the renaming to the newly clicked label
			// instead of the original one
			void* rename_label;
			// Filled in by the text input
			CapacityStream<char> rename_label_storage;
			// These are untyped
			CapacityStream<void> first_selected_label;
			CapacityStream<void> last_selected_label;

			Stream<void> selected_labels;

			// These are coalesced into a single allocation
			Stream<void> copied_labels;

			UIConfigLabelHierarchyMonitorSelection monitor_selection;
			bool has_monitor_selection;

			// Value of 0 means no rename, 1 means just selected, 2 the label was copied
			unsigned char is_rename_label;
			bool is_dragging;
			bool reject_same_label_drag;
			bool determine_selection;
			bool is_selection_cut;
			// This is set to true when we need to call the callback
			// When the mouse is dragging the label, not only on release
			bool drag_callback_when_held;
			// This boolean flag indicates whether or not the Ctrl actions
			// Should trigger only when the window is focused or all the time
			bool are_basic_operations_on_focus_only;

			// The byte size of the label. If default behaviour is used, this will be 0
			unsigned short label_size;

			// This is the index of the UI window from which the allocations are to be made
			unsigned int window_index;

			// Used for the dynamic identifier
			Stream<char> identifier;

			Action selectable_action;
			void* selectable_data;

			Action right_click_action;
			void* right_click_data;

			Action drag_action;
			void* drag_data;

			Action rename_action;
			void* rename_data;

			Action double_click_action;
			void* double_click_data;

			// Basic operations - the next 3 handlers
			Action copy_action;
			void* copy_data;
			unsigned int copy_data_size;

			Action cut_action;
			void* cut_data;
			unsigned int cut_data_size;

			Action delete_action;
			void* delete_data;
			unsigned int delete_data_size;
		};

		template<typename UIDrawerLabelHierarchyActionData>
		void UIDrawerLabelHierarchyGetEmbeddedLabel(const UIDrawerLabelHierarchyActionData* data, void* storage) {
			if (data->hierarchy->label_size == 0) {
				Stream<char> string_label = { OffsetPointer(data, sizeof(*data)), data->label_size };
				Stream<char>* storage_string = (Stream<char>*)storage;
				*storage_string = string_label;
			}
			else {
				memcpy(storage, OffsetPointer(data, sizeof(*data)), data->hierarchy->label_size);
			}
		}

		// Returns the total write size
		template<typename UIDrawerLabelHierarchyActionData>
		unsigned int UIDrawerLabelHierarchySetEmbeddedLabel(UIDrawerLabelHierarchyActionData* data, const void* untyped_label, size_t storage_capacity) {
			unsigned int total_size = sizeof(*data);

			if (data->hierarchy->label_size == 0) {
				Stream<char> label = *(Stream<char>*)untyped_label;
				ECS_ASSERT(sizeof(*data) + label.size <= storage_capacity);
				memcpy(OffsetPointer(data, sizeof(*data)), label.buffer, label.size);
				data->label_size = label.size;

				total_size += label.size;
			}
			else {
				ECS_ASSERT(sizeof(*data) + data->hierarchy->label_size <= storage_capacity);
				memcpy(OffsetPointer(data, sizeof(*data)), untyped_label, data->hierarchy->label_size);
				data->label_size = 0;
				total_size += data->hierarchy->label_size;
			}

			return total_size;
		}

		// The label is embedded in it
		struct ECSENGINE_API UIDrawerLabelHierarchyRightClickData {
			void GetLabel(void* storage) const;

			// Set the hierarchy before it
			unsigned int WriteLabel(const void* untyped_label, size_t storage_capacity);

			UIDrawerLabelHierarchyData* hierarchy;
			void* data;
			unsigned int label_size;
			Color hover_color;
		};

		struct UIDrawerLabelHierarchyDragData {
			// If this is set, then the callback is used
			// When it is released over another label
			// Else it means that it is dragging and you can perform
			// Some other operations here
			bool is_released;
			// This is set to true if this is the initial call of the held drag
			// When using only the released mode, this value is not used
			bool has_started;
			void* data;
			Stream<void> source_labels; // The ones being dragged
			void* destination_label; // The one where they are being dragged
		};

		struct UIDrawerLabelHierarchyDoubleClickData {
			void* data;
			void* label;
		};

		struct UIDrawerLabelHierarchySelectableData {
			void* data;
			Stream<void> labels;
		};

		struct UIDrawerLabelHierarchyRenameData {
			void* data;
			void* previous_label;
			Stream<char> new_label;
		};

		struct UIDrawerLabelHierarchyRenameFrameHandlerData {
			UIDrawerLabelHierarchyData* hierarchy;
			float2 label_position;
			float2 label_scale;
		};

		// If the destination label is empty, it means that the labels are copied as roots
		struct UIDrawerLabelHierarchyCopyData {
			void* data;
			Stream<void> source_labels;
			void* destination_label;
		};

		// If the destination label is empty, it means that the labels are cut as roots
		struct UIDrawerLabelHierarchyCutData {
			void* data;
			Stream<void> source_labels;
			void* destination_label;
		};

		struct UIDrawerLabelHierarchyDeleteData {
			void* data;
			Stream<void> source_labels;
		};

		struct UIDrawerAcquireDragDrop {
			Stream<char> region_name;
			Stream<Stream<char>> names;
			Color highlight_color;
			bool highlight_element;
			float highlight_thickness;
		};

		struct UIConfigElementNameAction {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_ELEMENT_NAME_ACTION;
			}

			UIActionHandler hoverable_handler = { nullptr };
			// Only the left button at the moment
			UIActionHandler clickable_handler = { nullptr };
			UIActionHandler general_handler = { nullptr };
		};

		// The handler receives the index to be deleted before the deletion actually takes place
		// in the additional data parameter
		struct UIConfigArrayRemoveAnywhere {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_ARRAY_REMOVE_ANYWHERE;
			}

			UIActionHandler handler;
		};

		struct UIConfigElementNameBackground {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_ELEMENT_NAME_BACKGROUND;
			}

			Color color;
		};

		struct UIDrawer;

		typedef void (*UIConfigArrayPrePostDrawFunction)(UIDrawer* drawer, void* draw_data);

		struct UIConfigArrayPrePostDraw {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_ARRAY_PRE_POST_DRAW;
			}

			UIConfigArrayPrePostDrawFunction pre_function = nullptr;
			void* pre_data;
			UIConfigArrayPrePostDrawFunction post_function = nullptr;
			void* post_data;
		};

		typedef void (*UIConfigTextInputDisplayFunction)(Stream<char> text, CapacityStream<char>& formatted_message, void* data);

		struct UIConfigTextInputMisc {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_INPUT_MISC;
			}

			bool display_tooltip = false;
			UIConfigTextInputDisplayFunction display_function = nullptr;
			void* display_data = nullptr;
		};

		struct UIConfigColorInputSliders {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_COLOR_INPUT_SLIDERS;
			}

			bool rgb = true;
			bool hsv = true;
			bool alpha = true;
			bool hex = true;
			Stream<char> target_window_name = {};
		};

		struct UIConfigSpriteStateButtonCallback {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_SPRITE_STATE_BUTTON_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct UIConfigDebouncing {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_DEBOUNCING;
			}

			// How many milliseconds should pass until the new value is updated
			float milliseconds;
		};

		// An enumeration of element identifiers that can be used to unique identify
		// A subportion of an overarching element
		enum ECS_UI_ELEMENT_IDENTIFIER_TYPE : size_t {
			ECS_UI_ELEMENT_IDENTIFIER_NAME,
			ECS_UI_ELEMENT_IDENTIFIER_NUMBER_INPUT,
			ECS_UI_ELEMENT_IDENTIFIER_SLIDER_INPUT,
			ECS_UI_ELEMENT_IDENTIFIER_CHECK_BOX,
			ECS_UI_ELEMENT_IDENTIFIER_TEXT_INPUT
		};

		struct UICustomElementIdentifier {
			ECS_UI_ELEMENT_IDENTIFIER_TYPE type;
			// When true, the custom element is called before the normal element is drawn, else
			// It will be called after it was drawn. The draw before the normal element can be more
			// Expensive if the visual elements are still enabled
			bool call_before_element;
			// When true, the normal element won't output anything visual
			bool disable_visual_elements = true;
			// When true, the normal element won't add any action handlers that it would normally add
			bool disable_action_handlers = true;
		};

		struct UICustomElementDrawFunctionData {
			UIDrawer* drawer;
			// Data from the registration
			void* user_data;

			// The bounds of the element to be overriden (cannot exceed them)
			float2 position;
			float2 scale;

			// If an element is made out of multiple subpieces, this identifier can help you
			// Perform an operation only on some subpieces
			ECS_UI_ELEMENT_IDENTIFIER_TYPE identifier;
		};

		// A custom element draw function that can be used to override the default draw method of a particular portion
		// Of an UI element. The return is relevant only for the case when the draw function is called before the
		// Override element, if it is called after it, it has no bearing
		typedef void (*UICustomElementDrawFunction)(UICustomElementDrawFunctionData* data);

		struct UIConfigCustomElementDraw {
			ECS_INLINE static size_t GetAssociatedBit() {
				return UI_CONFIG_CUSTOM_ELEMENT_DRAW;
			}

			void AddIdentifier(ECS_UI_ELEMENT_IDENTIFIER_TYPE identifier, bool call_before_element, bool disable_visual_elements, bool disable_action_handlers) {
				ECS_ASSERT(identifier_count < ECS_COUNTOF(identifiers));
				identifiers[identifier_count].type = identifier;
				identifiers[identifier_count].call_before_element = call_before_element;
				identifiers[identifier_count].disable_visual_elements = disable_visual_elements;
				identifiers[identifier_count].disable_action_handlers = disable_action_handlers;
				identifier_count++;
			}

			// Returns nullptr if it doesn't find it
			const UICustomElementIdentifier* GetIdentifier(ECS_UI_ELEMENT_IDENTIFIER_TYPE identifier_type) const {
				for (unsigned char index = 0; index < identifier_count; index++) {
					if (identifiers[index].type == identifier_type) {
						return &identifiers[index];
					}
				}
				return nullptr;
			}

			// This data is not copied anywhere, can be temporary
			void* user_data;
			UICustomElementDrawFunction function;

			// Keeps track of how many elements there are in identifiers. Use "AddIdentifier" to ensure that
			// The internal storage is overrun
			unsigned char identifier_count = 0;
			// Each identifier has separate options, to allow for the most amount of customization
			UICustomElementIdentifier identifiers[8];
		};
	}

}