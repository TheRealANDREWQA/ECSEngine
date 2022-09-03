#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Containers/HashTable.h"
#include "../../Containers/Stacks.h"
#include "../../Containers/Queues.h"
#include "UIStructures.h"
#include "UIDrawConfigs.h"
#include "../../Rendering/ColorMacros.h"
#include "UISystem.h"

namespace ECSEngine {

	namespace Tools {

		struct UIDrawConfig;

		using UIDrawerTextInputFilter = bool (*)(char, CharacterType);

		typedef void (*UIDrawerSliderInterpolate)(const void* lower_bound, const void* upper_bound, void* value, float percentage);
		typedef void (*UIDrawerSliderFromFloat)(void* value, float factor);
		typedef float (*UIDrawerSliderToFloat)(const void* value);
		typedef float (*UIDrawerSliderPercentage)(const void* lower_bound, const void* upper_bound, const void* value);
		typedef void (*UIDrawerSliderToString)(CapacityStream<char>& characters, const void* data, void* extra_data);
		typedef void (*UIDrawerSliderConvertTextInput)(CapacityStream<char>& characters, void* data);
		typedef bool (*UIDrawerSliderIsSmaller)(const void* left, const void* right);

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

		struct ECSENGINE_API UIConfigAbsoluteTransform {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_ABSOLUTE_TRANSFORM;
			}

			float2 position;
			float2 scale;
		};

		struct ECSENGINE_API UIConfigRelativeTransform {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_RELATIVE_TRANSFORM;
			}

			float2 offset = { 0.0f, 0.0f };
			float2 scale = { 1.0f, 1.0f };
		};

		struct ECSENGINE_API UIConfigWindowDependentSize {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_WINDOW_DEPENDENT_SIZE;
			}

			ECS_UI_WINDOW_DEPENDENT_SIZE type = ECS_UI_WINDOW_DEPENDENT_HORIZONTAL;
			float2 offset = { 0.0f, 0.0f };
			float2 scale_factor = { 1.0f, 1.0f };
		};

		struct ECSENGINE_API UIConfigTextParameters {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_PARAMETERS;
			}

			Color color = ECS_TOOLS_UI_TEXT_COLOR;
			float2 size = { ECS_TOOLS_UI_FONT_SIZE * ECS_TOOLS_UI_FONT_X_FACTOR, ECS_TOOLS_UI_FONT_SIZE };
			float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING;
		};

		struct ECSENGINE_API UIConfigTextAlignment {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_ALIGNMENT;
			}

			ECS_UI_ALIGN horizontal = ECS_UI_ALIGN_MIDDLE;
			ECS_UI_ALIGN vertical = ECS_UI_ALIGN_MIDDLE;
		};

		struct ECSENGINE_API UIConfigColor {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_COLOR;
			}

			Color color = ECS_TOOLS_UI_THEME_COLOR;
		};

		struct ECSENGINE_API UIConfigDoNotAdvance {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_DO_NOT_ADVANCE;
			}

			bool update_render_bounds = true;
			bool update_current_scale = true;
			bool handle_draw_mode = true;
		};

		struct ECSENGINE_API UIConfigButtonHoverable {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_BUTTON_HOVERABLE;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigSliderColor {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_COLOR;
			}

			Color color = ToneColor(ECS_TOOLS_UI_THEME_COLOR, ECS_TOOLS_UI_SLIDER_LIGHTEN_FACTOR);
		};

		struct ECSENGINE_API UIConfigSliderShrink {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_SHRINK;
			}

			float2 value = { ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_X, ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_Y };
		};

		struct ECSENGINE_API UIConfigSliderPadding {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_PADDING;
			}

			float value = ECS_TOOLS_UI_LABEL_HORIZONTAL_PADD;
		};

		struct ECSENGINE_API UIConfigSliderLength {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_LENGTH;
			}

			float value = ECS_TOOLS_UI_SLIDER_LENGTH_X;
		};

		struct ECSENGINE_API UIConfigBorder {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_BORDER;
			}

			bool is_interior = false;
			Color color;
			float thickness = ECS_TOOLS_UI_DOCKSPACE_BORDER_SIZE;
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
				Integer character_value = function::ConvertCharactersToIntImpl<Integer, char>(characters);
				Integer* value = (Integer*)_value;
				*value = character_value;
			};

			auto to_string = [](CapacityStream<char>& characters, const void* _value, void* extra_data) {
				characters.size = 0;
				function::ConvertIntToChars(characters, *(const Integer*)_value);
			};

			auto from_float = [](void* value, float float_percentage) {
				Integer* integer_value = (Integer*)value;
				Integer min, max;
				function::IntegerRange<Integer>(min, max);
				int64_t min_64 = min;
				int64_t max_64 = max;

				int64_t percentage_64 = (int64_t)float_percentage;
				*integer_value = function::Clamp(percentage_64, min_64, max_64);
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

		struct ECSENGINE_API UIConfigTextInputHint {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_INPUT_HINT;
			}

			const char* characters;
		};

		struct ECSENGINE_API UIConfigTextInputCallback {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_INPUT_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigSliderChangedValueCallback {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigHoverableAction {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_RECTANGLE_HOVERABLE_ACTION;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigClickableAction {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_RECTANGLE_CLICKABLE_ACTION;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigGeneralAction {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_RECTANGLE_GENERAL_ACTION;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigRectangleVertexColor {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_RECTANGLE_VERTEX_COLOR;
			}

			Color colors[4];
		};

		struct ECSENGINE_API UIConfigSpriteGradient {
			static inline size_t GetAssociatedBit() {
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

		// can be drawer or initializer
		typedef void (*UINodeDraw)(void* drawer_ptr, void* window_data, void* other_ptr);

		struct UIDrawerHierarchyNode {
			UIDrawerTextElement name_element;
			const char* name;
			unsigned int name_length;
			bool state;
			// if it is the first time it is drawn, make sure to pin the y axis
			bool has_been_pinned;
			UIDynamicStream<void*> internal_allocations;
			UINodeDraw function;
		};

		struct UIDrawerHierarchyPendingNode {
			UINodeDraw function;
			unsigned int bound_node;
		};

		struct UIDrawerHierarchiesElement {
			void* hierarchy;
			unsigned int node_index;
			unsigned int children_count;
		};

		// used to coordinate multiple hierarchies - one parent and the rest child
		struct UIDrawerHierarchiesData {
			UIDynamicStream<UIElementTransform> hierarchy_transforms;
			UIDynamicStream<UIDrawerHierarchiesElement> elements;
			void* active_hierarchy;
		};

		struct UIDrawerHierarchySelectable {
			unsigned int selected_index;
			unsigned int offset;
			unsigned int* pointer;
			Action callback = nullptr;
			void* callback_data = nullptr;
		};

		// multiple hierarchy data is used to implement embedded hierarchies and syncronize between them
		struct ECSENGINE_API UIDrawerHierarchy {
			// it returns an identifier that can be used to speed up node removal otherwise it can be ignored
			// it should not modify anything
			// if no name is displayed, the pointer returned will be nullptr and should not be used
			template<bool no_name_display = false>
			void* AddNode(
				Stream<char> name,
				UINodeDraw draw,
				UINodeDraw initialize,
				bool initial_state = false,
				Color text_color = ECS_COLOR_WHITE
			) {
				size_t current_index = nodes.size;
				nodes.ReserveNewElements(1);

				size_t name_length = ParseStringIdentifier(name);
				if constexpr (no_name_display) {
					name_length = 0;
				}
				void* allocation = system->m_memory->Allocate(sizeof(UISpriteVertex) * name_length * 6 + name.size + 1);
				uintptr_t buffer = (uintptr_t)allocation;

				if constexpr (!no_name_display) {
					system->ConvertCharactersToTextSprites(
						name,
						float2(0.0f, 0.0f),
						(UISpriteVertex*)allocation,
						text_color,
						0,
						system->m_descriptors.font.size,
						system->m_descriptors.font.character_spacing
					);
					buffer += sizeof(UISpriteVertex) * name_length * 6;

					nodes[current_index].name_element.text_vertices.buffer = (UISpriteVertex*)allocation;
					nodes[current_index].name_element.text_vertices.size = name_length * 6;
					nodes[current_index].name_element.text_vertices.capacity = name_length * 6;
					nodes[current_index].name_element.inverse_zoom = { 1.0f, 1.0f };
					nodes[current_index].name_element.zoom = { 1.0f, 1.0f };
					nodes[current_index].name_element.position = { 0.0f, 0.0f };
					nodes[current_index].name_element.scale = GetTextSpan(nodes[current_index].name_element.text_vertices);
				}
				else {
					nodes[current_index].name_element.text_vertices.buffer = nullptr;
					nodes[current_index].name_element.text_vertices.size = 0;
				}

				nodes[current_index].name = (const char*)buffer;
				name.CopyTo(buffer);

				nodes[current_index].internal_allocations.buffer = nullptr;
				nodes[current_index].internal_allocations.size = 0;
				nodes[current_index].internal_allocations.capacity = 0;
				nodes[current_index].internal_allocations.allocator = GetAllocatorPolymorphic(system->m_memory);

				nodes[current_index].function = draw;
				nodes[current_index].name_length = name.size;
				nodes[current_index].state = initial_state;

				nodes.size++;

				UIDrawerHierarchyPendingNode pending_node = { initialize, static_cast<unsigned int>(current_index) };
				pending_initializations.Add(pending_node);

				return nodes[current_index].name_element.TextBuffer();
			}

			void AddNode(const UIDrawerHierarchyNode& node, unsigned int position);

			void Clear();
			void DeallocateNode(unsigned int index);
			// Multiple hierarchies data won't be deallocated since it can't be known
			// for sure if this is the last owner; window destruction will corectly release it
			void DeallocateThis();
			
			template<bool is_name = true>
			void RemoveNode(const void* identifier) {
				if constexpr (is_name) {
					RemoveNode(FindNode((const char*)identifier));
				}
				else {
					// the identifier is the pointer to the name pointer 
					// the compare becames a simple pointer check
					for (size_t index = 0; index < nodes.size; index++) {
						if (nodes[index].name_element.text_vertices.buffer == identifier) {
							RemoveNode(index);
							return;
						}
					}
				}

				// if no node matches the identifier/name
				ECS_ASSERT(false);
			}

			unsigned int FindNode(const char* label) const;

			void* GetData();
			UIDrawerHierarchyNode GetNode(unsigned int index) const;
			void SetSelectedNode(unsigned int index);
			void SetData(void* data, unsigned int data_size = 0);
			void SetChildData(UIDrawConfig& config);

			// if sure about what is happening, this can also be used
			// but it has no referential integrity: removing other nodes can invalidate 
			// the index if taken at the insertion time
			void RemoveNode(unsigned int index);

			// used by drag node
			void RemoveNodeWithoutDeallocation(unsigned int index);

			UISystem* system;
			UIDynamicStream<UIDrawerHierarchyNode> nodes;
			UIDynamicStream<UIDrawerHierarchyPendingNode> pending_initializations;
			void* data;
			UIDrawerHierarchiesData* multiple_hierarchy_data;
			unsigned int data_size;
			unsigned int window_index;
			UIDrawerHierarchySelectable selectable;
		};

		// if using this for list, override closed uvs
		struct ECSENGINE_API UIConfigHierarchySpriteTexture {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_HIERARCHY_SPRITE_TEXTURE;
			}

			const wchar_t* texture;
			float2 top_left_closed_uv = { 0.0f, 0.0f };
			float2 bottom_right_closed_uv = { 1.0f, 1.0f };
			float2 top_left_opened_uv = {0.0f, 0.0f};
			float2 bottom_right_opened_uv = {1.0f, 1.0f};
			float2 scale_factor = {1.0f, 1.0f};
			Color color = ECS_COLOR_WHITE;
			bool keep_triangle = false;
		};

		struct ECSENGINE_API UIConfigHierarchyNoAction {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME;
			}

			void* extra_information;
			float row_y_scale;
		};

		struct ECSENGINE_API UIConfigHierarchyChild {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_HIERARCHY_CHILD;
			}
			
			void* parent;
		};

		// The callback receives the hierarchy into additional data parameter
		// Pointer or callback can miss
		struct ECSENGINE_API UIConfigHierarchySelectable {
			static inline size_t GetAssociatedBit() {
				return UI_CONFIG_HIERARCHY_SELECTABLE;
			}

			unsigned int* pointer = nullptr;
			Action callback_action = nullptr;
			void* callback_data = nullptr;
			unsigned int callback_data_size = 0;
			unsigned int offset = 0;
		};

		struct ECSENGINE_API UIDrawerList {
			void* AddNode(
				const char* name,
				UINodeDraw draw,
				UINodeDraw initialize
			);

			template<bool is_name = true>
			void RemoveNode(const void* identifier) {
				hierarchy.RemoveNode<is_name>(identifier);
			}

			void SetData(void* data, unsigned int data_size = 0);

			void InitializeNodeYScale(size_t* counts);

			void FinalizeNodeYscale(const void** buffers, size_t* counts);

			UIDrawerHierarchy hierarchy;
			UIDrawerTextElement name;
			size_t counts[ECS_TOOLS_UI_MATERIALS];
			UIConfigHierarchyNoAction* hierarchy_extra;
		};

		struct UIDrawerSentenceNotCached {
			Stream<unsigned int> whitespace_characters;
		};

		struct UIDrawerWhitespaceCharacter {
			unsigned short position;
			char type;
		};

		struct ECSENGINE_API UIDrawerSentenceBase {
			void SetWhitespaceCharacters(Stream<char> characters, char parse_token = ' ');

			Stream<UISpriteVertex> vertices;
			Stream<UIDrawerWhitespaceCharacter> whitespace_characters;
		};

		struct UIDrawerSentenceCached {
			UIDrawerSentenceBase base;
			float2 zoom;
			float2 inverse_zoom;
		};

		struct UIDrawerSentenceFitSpaceToken {
			static inline size_t GetAssociatedBit() {
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

		struct ECSENGINE_API UIConfigGraphKeepResolutionX {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_KEEP_RESOLUTION_X;
			}

			float max_y_scale = 10000.0f;
			float min_y_scale = 0.2f;
		};

		struct ECSENGINE_API UIConfigGraphKeepResolutionY {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_KEEP_RESOLUTION_Y;
			}

			float max_x_scale = 10000.0f;
			float min_x_scale = 0.2f;
		};

		struct ECSENGINE_API UIConfigGraphColor {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_DROP_COLOR;
			}

			Color color;
		};

		struct ECSENGINE_API UIConfigHistogramColor {
			inline static size_t GetAssociatedBit() {
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

		struct ECSENGINE_API UIConfigGraphMinY {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_MIN_Y;
			}

			float value;
		};

		struct ECSENGINE_API UIConfigGraphMaxY {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_MAX_Y;
			}

			float value;
		};

		struct ECSENGINE_API UIConfigGraphTags {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_TAGS;
			}

			Action vertical_tags[4];
			Action horizontal_tags[4];
			float vertical_values[4];
			float horizontal_values[4];
			unsigned int vertical_tag_count = 0;
			unsigned int horizontal_tag_count = 0;
		};

		struct UIDrawerMenuWindow {
			Stream<char> name;
			float2 position;
			float2 scale;
		};

		struct UIDrawerMenuState {
			//  ------------------------------- User modifiable -------------------------
			Stream<char> left_characters;
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

		struct ECSENGINE_API UIDrawerMenu {
			inline bool IsTheSameData(const UIDrawerMenu* other) const {
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
			unsigned char menu_initializer_index = 255;
			float2 destroy_position;
			float2 destroy_scale;
			bool initialize_from_right_click = false;
			bool is_opened_when_clicked = false;
		};

		struct UIDrawerMenuDrawWindowData {
			inline UIDrawerMenuState* GetState() const {
				if (submenu_offsets[0] == (unsigned char)-1) {
					return &menu->state;
				}

				UIDrawerMenuState* state = &menu->state.submenues[submenu_offsets[0]];
				for (unsigned char index = 1; index < 8; index++) {
					if (submenu_offsets[index] == (unsigned char)-1) {
						return state;
					}
					state = &state->submenues[submenu_offsets[index]];
				}

				return state;
			}

			inline unsigned char GetLastPosition() const {
				unsigned char index = 0;
				for (; index < 8; index++) {
					if (submenu_offsets[index] == (unsigned char)-1) {
						return index;
					}
				}

				ECS_ASSERT(false, "Too many submenues");
				return 8;
			}

			UIDrawerMenu* menu;
			// In this way we get reliable UIDrawerMenuState* even when the
			// underlying menu is changed
			unsigned char submenu_offsets[8];
		};


		struct ECSENGINE_API UIDrawerSubmenuHoverable {
			inline bool IsTheSameData(const UIDrawerSubmenuHoverable* other) const {
				return other != nullptr && draw_data.GetState() == other->draw_data.GetState() && row_index == other->row_index;
			}

			UIDrawerMenuDrawWindowData draw_data;
			unsigned int row_index;
		};

		template<size_t sample_count>
		struct UIDrawerMultiGraphSample {
			float GetX() const {
				return x;
			}
			float GetY(unsigned int index) const {
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

		struct ECSENGINE_API UIConfigColorInputCallback {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_COLOR_INPUT_CALLBACK;
			}

			UIActionHandler callback;
		};

		struct ECSENGINE_API UIConfigColorFloatCallback {
			inline static size_t GetAssociatedBit() {
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

		struct ECSENGINE_API UIConfigToolTip {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_RECTANGLE_TOOL_TIP;
			}

			Stream<char> characters;
		};

		struct UIDrawerStateTable {
			UIDrawerTextElement name;
			Stream<UIDrawerTextElement> labels;
			unsigned int max_x_scale;
		};

		struct UIDrawerMenuButtonData {
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
		};

		struct UIDrawerHandlerState {
			size_t hoverable_count;
			size_t clickable_count;
			size_t general_count;
		};

		struct ECSENGINE_API UIConfigStateTableNotify {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE;
			}

			bool* notifier;
		};

		using UIConfigFilterMenuNotify = UIConfigStateTableNotify;

		struct UIDrawerStateTableBoolClickable {
			bool* notifier;
			bool* state;
		};

		struct ECSENGINE_API UIConfigComboBoxPrefix {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_COMBO_BOX_PREFIX;
			}

			Stream<char> prefix;
		};

		struct UIDrawerFilesystemHierarchyLabelData {
			bool state;
			unsigned char activation_count;
		};

		struct ECSENGINE_API UIConfigFilesystemHierarchyHashTableCapacity {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_FILESYSTEM_HIERARCHY_HASH_TABLE_CAPACITY;
			}

			unsigned int capacity;
		};

		struct UIDrawerFilesystemHierarchy {
			Stream<char> active_label;
			Stream<char> selected_label_temporary;
			Stream<char> right_click_label_temporary;
			HashTableDefault<UIDrawerFilesystemHierarchyLabelData> label_states;
			Action selectable_callback;
			void* selectable_callback_data;
			Action right_click_callback;
			void* right_click_callback_data;
		};

		struct ECSENGINE_API UIConfigLabelHierarchySpriteTexture {
			inline static size_t GetAssociatedBit() {
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
		struct ECSENGINE_API UIConfigLabelHierarchySelectableCallback {
			inline static size_t GetAssociatedBit() {
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
		// label stream; if copy_on_initialization is set, then the data parameter will be copied only in the initializer
		// pass
		struct ECSENGINE_API UIConfigLabelHierarchyRightClick {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK;
			}

			Action callback;
			void* data;
			unsigned int data_size = 0;
			ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
			bool copy_on_initialization = false;
		};

		typedef UIConfigLabelHierarchyRightClick UIConfigFilesystemHierarchyRightClick;

		struct ECSENGINE_API UIConfigLabelHierarchyDragCallback {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_DRAG_LABEL;
			}

			Action callback;
			void* data;
			unsigned int data_size = 0;
			ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
			bool copy_on_initialization = false;
			bool reject_same_label_drag = true;
		};

		struct ECSENGINE_API UIConfigLabelHierarchyRenameCallback {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_RENAME_LABEL;
			}

			Action callback;
			void* data;
			unsigned int data_size = 0;
			ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
			bool copy_on_initialization = false;
		};

		// Needs to take as data parameter a UIDrawerLabelHierarchyDoubleClickData
		struct ECSENGINE_API UIConfigLabelHierarchyDoubleClickCallback {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_DOUBLE_CLICK_ACTION;
			}

			Action callback;
			void* data;
			unsigned int data_size = 0;
			ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
			bool copy_on_initialization = false;
		};

		struct UIConfigLabelHierarchyFilter {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_FILTER;
			}

			Stream<char> filter;
		};

		// Any handler can be omitted
		struct UIConfigLabelHierarchyBasicOperations {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_BASIC_OPERATIONS;
			}

			UIActionHandler copy_handler = { nullptr };
			UIActionHandler cut_handler = { nullptr };
			UIActionHandler delete_handler = { nullptr };
		};

		struct ECSENGINE_API UIConfigMenuButtonSprite {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_MENU_BUTTON_SPRITE;
			}

			const wchar_t* texture;
			float2 top_left_uv = { 0.0f, 0.0f };
			float2 bottom_right_uv = { 1.0f, 1.0f };
			Color color = ECS_COLOR_WHITE;
		};

		using UIConfigMenuSprite = UIConfigMenuButtonSprite;

		struct ECSENGINE_API UIConfigActiveState {
			inline static size_t GetAssociatedBit() {
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
			float** ECS_RESTRICT values;
			const float* ECS_RESTRICT default_values;
			const float* ECS_RESTRICT lower_bound;
			const float* ECS_RESTRICT upper_bound;
		};

		struct UIDrawerInitializeDoubleInputGroup {
			UIDrawConfig* config;
			size_t count;
			Stream<char> group_name;
			Stream<char>* names;
			double** ECS_RESTRICT values;
			const double* ECS_RESTRICT default_values;
			const double* ECS_RESTRICT lower_bound;
			const double* ECS_RESTRICT upper_bound;
		};

		template<typename Integer>
		struct UIDrawerInitializeIntegerInputGroup {
			UIDrawConfig* config;
			size_t count;
			Stream<char> group_name;
			Stream<char>* names;
			Integer** ECS_RESTRICT values;
			const Integer* ECS_RESTRICT default_values;
			const Integer* ECS_RESTRICT lower_bound;
			const Integer* ECS_RESTRICT upper_bound;
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

		struct UIDrawerInitializePathInput {
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
		};

		struct UIDrawerLabelList {
			UIDrawerTextElement name;
			Stream<UIDrawerTextElement> labels;
		};

		struct ECSENGINE_API UIConfigCollapsingHeaderSelection {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_COLLAPSING_HEADER_SELECTION;
			}

			bool is_selected;
		};

		struct ECSENGINE_API UIConfigGetTransform {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GET_TRANSFORM;
			}

			float2* position;
			float2* scale;
		};

		// Phase will always be UIDrawPhase::System
		struct ECSENGINE_API UIConfigComboBoxCallback {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_COMBO_BOX_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct UIDrawerInitializeArrayElementData {
			UIDrawConfig* config;
			Stream<char> name;
			size_t element_count;
		};

		struct UIDrawerArrayData {
			bool collapsing_header_state;
			bool drag_is_released;
			ECS_UI_DRAW_PHASE add_callback_phase;
			ECS_UI_DRAW_PHASE remove_callback_phase;
			unsigned int drag_index;
			float drag_current_position;
			float row_y_scale;
			unsigned int previous_element_count;
			Action add_callback;
			void* add_callback_data;
			Action remove_callback;
			void* remove_callback_data;
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

		struct UIConfigArrayProvideNameActionData {
			char** name;
			unsigned int index;
		};

		// The names must be unique for the window
		// All fields are optional - at least one must be filled
		// The action receives the data it wants in _data and a UIConfigArrayProvideNameActionData to
		// retrieve the index from. It will default to Element + index
		// for elements that are outside the boundary of the capacity
		struct ECSENGINE_API UIConfigArrayProvideNames {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_ARRAY_PROVIDE_NAMES;
			}

			Stream<const char*> char_names = { nullptr, 0 };
			Stream<Stream<char>> stream_names = { nullptr, 0 };
			Action select_name_action = nullptr;
			void* select_name_action_data = nullptr;
			unsigned int select_name_action_capacity = 0;
		};

		// In the additional data field it will receive a pointer to
		// UIDrawerArrayAddRemoveData* and can use it to do initialization 
		// of new elements or some other tasks. In the UIDrawerArrayAddRemoveData* the new size 
		// will actually be the old size. The new size can be looked into the streams
		struct ECSENGINE_API UIConfigArrayAddCallback {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_ARRAY_ADD_CALLBACK;
			}

			UIActionHandler handler;
		};

		// In the additional data field it will receive a pointer to
		// UIDrawerArrayAddRemoveData* and can use it to do initialization 
		// of new elements or some other tasks. In the UIDrawerArrayAddRemoveData* the new size 
		// will actually be the old size. The new size can be looked into the streams
		struct ECSENGINE_API UIConfigArrayRemoveCallback {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_ARRAY_REMOVE_CALLBACK;
			}

			UIActionHandler handler;
		};

		// The background is centered at the center of the sprite
		// Overwrite color black means ignore it - use the theme color
		struct ECSENGINE_API UIConfigSpriteButtonBackground {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_SPRITE_BUTTON_BACKGROUND;
			}

			float2 scale;
			Color overwrite_color = ECS_COLOR_BLACK;
		};

		struct ECSENGINE_API UIConfigCheckBoxCallback {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_CHECK_BOX_CALLBACK;
			}
		
			UIActionHandler handler;
			bool disable_value_to_modify = false;
		};

		// With this config the path input can be restricted to only paths that start from the given roots
		struct ECSENGINE_API UIConfigPathInputRoot {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_PATH_INPUT_ROOT;
			}

			Stream<Stream<wchar_t>> roots;
		};

		// Can choose between directly giving the files or giving a callback that will be called
		// when the user presses the folder button
		// The callback will receive as parameter in the additional_data a ResizableStream<Stream<wchar_t>>*
		// in order to make visible to the external function the files. The strings must be allocated
		// from that buffer such that they are visible to the outer scope
		struct ECSENGINE_API UIConfigPathInputGiveFiles {
			inline static size_t GetAssociatedBit() {
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
		struct ECSENGINE_API UIConfigPathInputCustomFilesystem {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_PATH_INPUT_CUSTOM_FILESYSTEM;
			}

			UIActionHandler callback;
		};

		// Receives the path as CapacityStream<wchar_t>* to the additional_data parameter
		struct ECSENGINE_API UIConfigPathInputCallback {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_PATH_INPUT_CALLBACK;
			}

			UIActionHandler callback;
		};

		struct ECSENGINE_API UIConfigPathInputSpriteTexture {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_PATH_INPUT_SPRITE_TEXTURE;
			}

			const wchar_t* texture;
		};

		struct ECSENGINE_API UIConfigNamePadding {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_NAME_PADDING;
			}

			ECS_UI_ALIGN alignment = ECS_UI_ALIGN_LEFT;
			float total_length = -1.0f;
			float offset_size = 0.0f;
 		};

		struct ECSENGINE_API UIConfigSliderMouseDraggable {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_MOUSE_DRAGGABLE;
			}

			bool interpolate_bounds = true;
		};

		struct ECSENGINE_API UIConfigAlignElement {
			inline static size_t GetAssociatedBit() {
				return UI_CONFIG_ALIGN_ELEMENT;
			}

			ECS_UI_ALIGN horizontal;
			ECS_UI_ALIGN vertical;
		};

		// Keeps a list with all the opened labels and any user callbacks
		struct ECSENGINE_API UIDrawerLabelHierarchyData {
			// If the action_data is nullptr, then it will skip the callback
			void AddSelection(Stream<char> label, ActionData* action_data);

			// If the action_data is nullptr, then it will skip the callback
			void ChangeSelection(Stream<char> label, ActionData* action_data);

			// If the action_data is nullptr, then it will skip the callback
			void RemoveSelection(Stream<char> label, ActionData* action_data);

			void ClearSelection(ActionData* action_data);

			// For copy/cut
			void RecordSelection(ActionData* action_data);

			Stream<Stream<char>> opened_labels;
			CapacityStream<char> hovered_label;
			// Filled in by the text input
			CapacityStream<char> rename_label;

			CapacityStream<char> first_selected;
			CapacityStream<char> last_selected;
			Stream<Stream<char>> selected_labels;

			// These are coallesced into a single allocation
			Stream<Stream<char>> copied_labels;

			bool is_rename_label;
			bool is_dragging;
			bool reject_same_label_drag;
			bool determine_selection;
			bool is_selection_cut;

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

			Action cut_action;
			void* cut_data;

			Action delete_action;
			void* delete_data;
		};

		// The label is embedded in it
		struct UIDrawerLabelHierarchyRightClickData {
			inline Stream<char> GetLabel() const {
				return { function::OffsetPointer(this, sizeof(*this)), label_size };
			}

			inline unsigned int WriteLabel(Stream<char> label) {
				memcpy(function::OffsetPointer(this, sizeof(*this)), label.buffer, label.size);
				label_size = label.size;
				return sizeof(*this) + label_size;
			}

			UIDrawerLabelHierarchyData* hierarchy;
			void* data;
			unsigned int label_size;
		};

		struct UIDrawerLabelHierarchyDragData {
			void* data;
			Stream<Stream<char>> source_label; // The ones being dragged
			Stream<char> destination_label; // The one where it is being dragged
		};

		struct UIDrawerLabelHierarchyDoubleClickData {
			void* data;
			Stream<char> label;
		};

		struct UIDrawerLabelHierarchySelectableData {
			void* data;
			Stream<Stream<char>> labels;
		};

		struct UIDrawerLabelHierarchyRenameData {
			void* data;
			Stream<char> previous_label;
			Stream<char> new_label;
		};

		// If the destination label is empty, it means that the labels are copied as roots
		struct UIDrawerLabelHierarchyCopyData {
			void* data;
			Stream<Stream<char>> source_labels;
			Stream<char> destination_label;
		};

		// If the destination label is empty, it means that the labels are cut as roots
		struct UIDrawerLabelHierarchyCutData {
			void* data;
			Stream<Stream<char>> source_labels;
			Stream<char> destination_label;
		};

		struct UIDrawerLabelHierarchyDeleteData {
			void* data;
			Stream<Stream<char>> source_labels;
		};

	}

}