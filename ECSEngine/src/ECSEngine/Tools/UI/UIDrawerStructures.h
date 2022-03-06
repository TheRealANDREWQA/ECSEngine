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

		using UIDrawerSliderInterpolate = void (*)(const void* lower_bound, const void* upper_bound, void* value, float percentage);
		using UIDrawerSliderPercentage = float (*)(const void* lower_bound, const void* upper_bound, const void* value);
		using UIDrawerSliderToString = void (*)(CapacityStream<char>& characters, const void* data, void* extra_data);
		using UIDrawerSliderConvertTextInput = void (*)(CapacityStream<char>& characters, void* data);
		using UIDrawerSliderIsSmaller = bool (*)(const void* left, const void* right);

		enum class UIDrawerMode : unsigned char {
			Indent,
			NextRow,
			NextRowCount,
			FitSpace,
			ColumnDraw,
			ColumnDrawFitSpace,
			Nothing
		};

		enum class TextAlignment : unsigned char {
			Left,
			Right,
			Middle,
			Top,
			Bottom
		};

		enum class WindowSizeTransformType : unsigned char {
			Horizontal,
			Vertical,
			Both
		};

		struct ECSENGINE_API UIConfigAbsoluteTransform {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_ABSOLUTE_TRANSFORM;
			}

			float2 position;
			float2 scale;
		};

		struct ECSENGINE_API UIConfigRelativeTransform {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_RELATIVE_TRANSFORM;
			}

			float2 offset = { 0.0f, 0.0f };
			float2 scale = { 1.0f, 1.0f };
		};

		struct ECSENGINE_API UIConfigWindowDependentSize {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_WINDOW_DEPENDENT_SIZE;
			}

			WindowSizeTransformType type = WindowSizeTransformType::Horizontal;
			float2 offset = { 0.0f, 0.0f };
			float2 scale_factor = { 1.0f, 1.0f };
		};

		struct ECSENGINE_API UIConfigTextParameters {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_PARAMETERS;
			}

			Color color = ECS_TOOLS_UI_TEXT_COLOR;
			float2 size = { ECS_TOOLS_UI_FONT_SIZE * ECS_TOOLS_UI_FONT_X_FACTOR, ECS_TOOLS_UI_FONT_SIZE };
			float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING;
		};

		struct ECSENGINE_API UIConfigTextAlignment {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_ALIGNMENT;
			}

			TextAlignment horizontal = TextAlignment::Middle;
			TextAlignment vertical = TextAlignment::Middle;
		};

		struct ECSENGINE_API UIConfigColor {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_COLOR;
			}

			Color color = ECS_TOOLS_UI_THEME_COLOR;
		};

		struct ECSENGINE_API UIConfigButtonHoverable {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_BUTTON_HOVERABLE;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigSliderColor {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_COLOR;
			}

			Color color = ToneColor(ECS_TOOLS_UI_THEME_COLOR, ECS_TOOLS_UI_SLIDER_LIGHTEN_FACTOR);
		};

		struct ECSENGINE_API UIConfigSliderShrink {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_SHRINK;
			}

			float2 value = { ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_X, ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_Y };
		};

		struct ECSENGINE_API UIConfigSliderPadding {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_PADDING;
			}

			float value = ECS_TOOLS_UI_SLIDER_PADDING_X;
		};

		struct ECSENGINE_API UIConfigSliderLength {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_LENGTH;
			}

			float value = ECS_TOOLS_UI_SLIDER_LENGTH_X;
		};

		struct ECSENGINE_API UIConfigBorder {
			static inline constexpr size_t GetAssociatedBit() {
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
				Integer character_value = function::ConvertCharactersToInt<Integer>(characters);
				Integer* value = (Integer*)_value;
				*value = character_value;
			};

			auto to_string = [](CapacityStream<char>& characters, const void* _value, void* extra_data) {
				characters.size = 0;
				function::ConvertIntToChars(characters, *(const Integer*)_value);
			};

			result.convert_text_input = convert_text_input;
			result.to_string = to_string;
			result.extra_data = nullptr;
			result.interpolate = UIDrawerSliderInterpolateImplementation<Integer>;
			result.is_smaller = UIDrawerSliderIsSmallerImplementation<Integer>;
			result.percentage = UIDrawerSliderPercentageImplementation<Integer>;

			return result;
		}

		inline UIDrawerSliderFunctions UIDrawerGetFloatSliderFunctions(unsigned int& precision) {
			UIDrawerSliderFunctions result;

			auto convert_text_input = [](CapacityStream<char>& characters, void* _value) {
				float character_value = function::ConvertCharactersToFloat(characters);
				float* value = (float*)_value;
				*value = character_value;
			};

			auto to_string = [](CapacityStream<char>& characters, const void* _value, void* extra_data) {
				characters.size = 0;
				function::ConvertFloatToChars(characters, *(const float*)_value, *(unsigned int*)extra_data);
			};

			result.convert_text_input = convert_text_input;
			result.to_string = to_string;
			result.extra_data = &precision;
			result.interpolate = UIDrawerSliderInterpolateImplementation<float>;
			result.is_smaller = UIDrawerSliderIsSmallerImplementation<float>;
			result.percentage = UIDrawerSliderPercentageImplementation<float>;

			return result;
		}

		inline UIDrawerSliderFunctions UIDrawerGetDoubleSliderFunctions(unsigned int& precision) {
			UIDrawerSliderFunctions result;

			auto convert_text_input = [](CapacityStream<char>& characters, void* _value) {
				double character_value = function::ConvertCharactersToDouble(characters);
				double* value = (double*)_value;
				*value = character_value;
			};

			auto to_string = [](CapacityStream<char>& characters, const void* _value, void* extra_data) {
				characters.size = 0;
				function::ConvertDoubleToChars(characters, *(const double*)_value, *(unsigned int*)extra_data);
			};

			result.convert_text_input = convert_text_input;
			result.to_string = to_string;
			result.extra_data = &precision;
			result.interpolate = UIDrawerSliderInterpolateImplementation<double>;
			result.is_smaller = UIDrawerSliderIsSmallerImplementation<double>;
			result.percentage = UIDrawerSliderPercentageImplementation<double>;

			return result;
		}

		struct ECSENGINE_API UIConfigTextInputHint {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_INPUT_HINT;
			}

			const char* characters;
		};

		struct ECSENGINE_API UIConfigTextInputCallback {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_INPUT_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigSliderChangedValueCallback {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigHoverableAction {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_HOVERABLE_ACTION;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigClickableAction {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_CLICKABLE_ACTION;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigGeneralAction {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_GENERAL_ACTION;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigRectangleVertexColor {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_RECTANGLE_VERTEX_COLOR;
			}

			Color colors[4];
		};

		struct ECSENGINE_API UIConfigSpriteGradient {
			static inline constexpr size_t GetAssociatedBit() {
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
		using UINodeDraw = void (*)(void* drawer_ptr, void* window_data, void* other_ptr);

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
				const char* name,
				UINodeDraw draw,
				UINodeDraw initialize,
				bool initial_state = false,
				Color text_color = ECS_COLOR_WHITE
			) {
				size_t current_index = nodes.size;
				nodes.ReserveNewElements(1);

				size_t full_name_length = strlen(name);
				size_t name_length = ParseStringIdentifier(name, full_name_length);
				if constexpr (no_name_display) {
					name_length = 0;
				}
				void* allocation = system->m_memory->Allocate(sizeof(UISpriteVertex) * name_length * 6 + full_name_length + 1);
				uintptr_t buffer = (uintptr_t)allocation;

				if constexpr (!no_name_display) {
					system->ConvertCharactersToTextSprites(
						name,
						float2(0.0f, 0.0f),
						(UISpriteVertex*)allocation,
						name_length,
						text_color,
						0,
						{ system->m_descriptors.font.size * ECS_TOOLS_UI_FONT_X_FACTOR, system->m_descriptors.font.size },
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
				memcpy((void*)buffer, name, sizeof(char) * full_name_length);

				// making sure it is null terminated
				char* temp_char = (char*)buffer;
				temp_char[full_name_length] = '\0';

				nodes[current_index].internal_allocations.buffer = nullptr;
				nodes[current_index].internal_allocations.size = 0;
				nodes[current_index].internal_allocations.capacity = 0;
				nodes[current_index].internal_allocations.allocator = GetAllocatorPolymorphic(system->m_memory);

				nodes[current_index].function = draw;
				nodes[current_index].name_length = full_name_length;
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
			static inline constexpr size_t GetAssociatedBit() {
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
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME;
			}

			void* extra_information;
			float row_y_scale;
		};

		struct ECSENGINE_API UIConfigHierarchyChild {
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_HIERARCHY_CHILD;
			}
			
			void* parent;
		};

		// The callback receives the hierarchy into additional data parameter
		// Pointer or callback can miss
		struct ECSENGINE_API UIConfigHierarchySelectable {
			static inline constexpr size_t GetAssociatedBit() {
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
			void SetWhitespaceCharacters(const char* characters, size_t character_count, char parse_token = ' ');

			Stream<UISpriteVertex> vertices;
			Stream<UIDrawerWhitespaceCharacter> whitespace_characters;
		};

		struct UIDrawerSentenceCached {
			UIDrawerSentenceBase base;
			float2 zoom;
			float2 inverse_zoom;
		};

		struct UIDrawerSentenceFitSpaceToken {
			static inline constexpr size_t GetAssociatedBit() {
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
			constexpr inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_KEEP_RESOLUTION_X;
			}

			float max_y_scale = 10000.0f;
			float min_y_scale = 0.2f;
		};

		struct ECSENGINE_API UIConfigGraphKeepResolutionY {
			constexpr inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_KEEP_RESOLUTION_Y;
			}

			float max_x_scale = 10000.0f;
			float min_x_scale = 0.2f;
		};

		struct ECSENGINE_API UIConfigGraphColor {
			constexpr inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_DROP_COLOR;
			}

			Color color;
		};

		struct ECSENGINE_API UIConfigHistogramColor {
			constexpr inline static size_t GetAssociatedBit() {
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
			constexpr inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_MIN_Y;
			}

			float value;
		};

		struct ECSENGINE_API UIConfigGraphMaxY {
			constexpr inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_MAX_Y;
			}

			float value;
		};

		struct ECSENGINE_API UIConfigGraphTags {
			constexpr inline static size_t GetAssociatedBit() {
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
			const char* name;
			float2 position;
			float2 scale;
		};

		struct UIDrawerMenuState {
			unsigned short* left_row_substreams;
			unsigned short* right_row_substreams;
			char* left_characters;
			char* right_characters = nullptr;
			UIActionHandler* click_handlers;
			bool* unavailables = nullptr;
			bool* row_has_submenu = nullptr;
			CapacityStream<UIDrawerMenuWindow>* windows;
			UIDrawerMenuState* submenues = nullptr;
			unsigned short row_count;
			unsigned short submenu_index;
			unsigned char separation_line_count = 0;
			unsigned char separation_lines[11];
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
			UIDrawerMenu* menu;
			UIDrawerMenuState* state;
		};


		struct ECSENGINE_API UIDrawerSubmenuHoverable {
			inline bool IsTheSameData(const UIDrawerSubmenuHoverable* other) const {
				return other != nullptr && state == other->state && row_index == other->row_index;
			}

			UIDrawerMenu* menu;
			UIDrawerMenuState* state;
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
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_COLOR_INPUT_CALLBACK;
			}

			UIActionHandler callback;
		};

		enum class UIDrawerReturnToDefaultDescriptorType : unsigned char {
			ColorTheme,
			Layout,
			Font,
			ElementDescriptor,
			Material,
			Miscellaneous,
			Dockspace
		};

		struct UIParameterWindowReturnToDefaultButtonData {
			UIWindowDrawerDescriptor* window_descriptor;
			void* system_descriptor;
			void* default_descriptor;
			bool is_system_theme;
			UIWindowDrawerDescriptorIndex descriptor_index;
			unsigned int descriptor_size;
		};

		struct ECSENGINE_API UIConfigToolTip {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_TOOL_TIP;
			}

			const char* characters;
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
			const char* window_name;
			const char* name;
			Stream<const char*> labels;
			bool** states;
			bool draw_all;
			bool* notifier;
		};

		struct UIDrawerFilterMenuSinglePointerData {
			const char* window_name;
			const char* name;
			Stream<const char*> labels;
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
			inline static constexpr size_t GetAssociatedBit() {
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
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_COMBO_BOX_PREFIX;
			}

			const char* prefix;
		};

		struct UIDrawerLabelHierarchyLabelData {
			bool state;
			unsigned char activation_count;
		};

		struct UIDrawerLabelHierarchy {
			Stream<char> active_label;
			Stream<char> selected_label_temporary;
			Stream<char> right_click_label_temporary;
			HashTable<UIDrawerLabelHierarchyLabelData, ResourceIdentifier, HashFunctionPowerOfTwo, UIHash> label_states;
			Action selectable_callback;
			void* selectable_callback_data;
			Action right_click_callback;
			void* right_click_callback_data;
		};

		struct ECSENGINE_API UIConfigLabelHierarchySpriteTexture {
			inline static constexpr size_t GetAssociatedBit() {
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

		// The callback receives the label char* in the additional action data member; if copy_on_initialization is set,
		//  then the data parameter will be copied only in the initializer pass
		struct ECSENGINE_API UIConfigLabelHierarchySelectableCallback {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK;
			}

			Action callback;
			void* callback_data;
			unsigned int callback_data_size = 0;
			UIDrawPhase phase = UIDrawPhase::Normal;
			bool copy_on_initialization = false;
		};

		struct ECSENGINE_API UIConfigLabelHierarchyHashTableCapacity {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_HASH_TABLE_CAPACITY;
			}

			unsigned int capacity;
		};

		// The callback must cast to UIDrawerLabelHierarchyRightClickData the _data pointer in order to get access to the 
		// label stream; if copy_on_initialization is set, then the data parameter will be copied only in the initializer
		// pass
		struct ECSENGINE_API UIConfigLabelHierarchyRightClick {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK;
			}

			Action callback;
			void* data;
			unsigned int data_size = 0;
			UIDrawPhase phase = UIDrawPhase::Normal;
			bool copy_on_initialization = false;
		};

		// Must be specified for each row of the sentence, nullptr action means skip
		// Count must be filled in order to be double checked with the drawer count
		// In order to prevent out of bounds accesses
		struct ECSENGINE_API UIConfigSentenceHoverableHandlers {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SENTENCE_HOVERABLE_HANDLERS;
			}

			Action* callback;
			void** data;
			unsigned int* data_size;
			UIDrawPhase* phase;
			unsigned int count;
		};

		// Must be specified for each row of the sentence, nullptr action means skip
		// Count must be filled in order to be double checked with the drawer count
		// In order to prevent out of bounds accesses
		struct ECSENGINE_API UIConfigSentenceClickableHandlers {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SENTENCE_CLICKABLE_HANDLERS;
			}

			Action* callback;
			void** data;
			unsigned int* data_size;
			UIDrawPhase* phase;
			unsigned int count;
		};

		// Must be specified for each row of the sentence, nullptr action means skip
		// Count must be filled in order to be double checked with the drawer count
		// In order to prevent out of bounds accesses
		struct ECSENGINE_API UIConfigSentenceGeneralHandlers {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SENTENCE_GENERAL_HANDLERS;
			}

			Action* callback;
			void** data;
			unsigned int* data_size;
			UIDrawPhase* phase;
			unsigned int count;
		};

		struct ECSENGINE_API UIConfigMenuButtonSprite {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_MENU_BUTTON_SPRITE;
			}

			const wchar_t* texture;
			float2 top_left_uv = { 0.0f, 0.0f };
			float2 bottom_right_uv = { 1.0f, 1.0f };
			Color color = ECS_COLOR_WHITE;
		};

		using UIConfigMenuSprite = UIConfigMenuButtonSprite;

		struct ECSENGINE_API UIConfigActiveState {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_ACTIVE_STATE;
			}

			bool state;
			char reserved[7];
		};

		struct ECSENGINE_API UIConfigCollapsingHeaderDoNotCache {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_DO_NOT_CACHE;
			}

			bool* state;
		};

		struct UIDrawerInitializeComboBox {
			UIDrawConfig* config;
			const char* name;
			Stream<const char*> labels;
			unsigned int label_display_count;
			unsigned char* active_label;
		};

		struct UIDrawerInitializeColorInput {
			UIDrawConfig* config;
			const char* name;
			Color* color;
			Color default_color;
		};

		struct UIDrawerInitializeFilterMenu {
			UIDrawConfig* config;
			const char* name;
			Stream<const char*> label_names;
			bool** states;
		};

		struct UIDrawerInitializeFilterMenuSinglePointer {
			UIDrawConfig* config;
			const char* name;
			Stream<const char*> label_names;
			bool* states;
		};

		struct UIDrawerInitializeHierarchy {
			UIDrawConfig* config;
			const char* name;
		};

		struct UIDrawerInitializeLabelHierarchy {
			UIDrawConfig* config;
			const char* identifier;
			Stream<const char*> labels;
		};

		struct UIDrawerInitializeList {
			UIDrawConfig* config;
			const char* name;
		};

		struct UIDrawerInitializeMenu {
			UIDrawConfig* config;
			const char* name;
			UIDrawerMenuState* state;
		};

		struct UIDrawerInitializeFloatInput {
			UIDrawConfig* config;
			const char* name; 
			float* value; 
			float default_value = 0.0f;
			float lower_bound = -FLT_MAX;
			float upper_bound = FLT_MAX;
		};

		template<typename Integer>
		struct UIDrawerInitializeIntegerInput {
			UIDrawConfig* config; 
			const char* name; 
			Integer* value;
			Integer default_value = 0;
			Integer min = LLONG_MIN;
			Integer max = ULLONG_MAX;
		};

		struct UIDrawerInitializeDoubleInput {
			UIDrawConfig* config;
			const char* name; 
			double* value; 
			double default_value = 0;
			double lower_bound = -DBL_MAX; 
			double upper_bound = DBL_MAX;
		};

		struct UIDrawerInitializeFloatInputGroup {
			UIDrawConfig* config;
			size_t count;
			const char* ECS_RESTRICT group_name;
			const char** ECS_RESTRICT names;
			float** ECS_RESTRICT values;
			const float* ECS_RESTRICT default_values;
			const float* ECS_RESTRICT lower_bound;
			const float* ECS_RESTRICT upper_bound;
		};

		struct UIDrawerInitializeDoubleInputGroup {
			UIDrawConfig* config;
			size_t count;
			const char* ECS_RESTRICT group_name;
			const char** ECS_RESTRICT names;
			double** ECS_RESTRICT values;
			const double* ECS_RESTRICT default_values;
			const double* ECS_RESTRICT lower_bound;
			const double* ECS_RESTRICT upper_bound;
		};

		template<typename Integer>
		struct UIDrawerInitializeIntegerInputGroup {
			UIDrawConfig* config;
			size_t count;
			const char* ECS_RESTRICT group_name;
			const char** ECS_RESTRICT names;
			Integer** ECS_RESTRICT values;
			const Integer* ECS_RESTRICT default_values;
			const Integer* ECS_RESTRICT lower_bound;
			const Integer* ECS_RESTRICT upper_bound;
		};

		struct UIDrawerInitializeSlider {
			UIDrawConfig* config;
			const char* name;
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
			const char* group_name;
			const char** names;
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
			const char* name;
			Stream<const char*> labels;
			bool** states;
		};

		struct UIDrawerInitializeStateTableSinglePointer {
			UIDrawConfig* config;
			const char* name;
			Stream<const char*> labels;
			bool* states;
		};

		struct UIDrawerInitializeTextInput {
			UIDrawConfig* config;
			const char* name;
			CapacityStream<char>* text_to_fill;
		};

		struct UIDrawerLabelList {
			UIDrawerTextElement name;
			Stream<UIDrawerTextElement> labels;
		};

		struct ECSENGINE_API UIConfigCollapsingHeaderSelection {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_COLLAPSING_HEADER_SELECTION;
			}

			bool is_selected;
		};

		struct ECSENGINE_API UIConfigGetTransform {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_GET_TRANSFORM;
			}

			float2* position;
			float2* scale;
		};

		// Phase will always be UIDrawPhase::System
		struct ECSENGINE_API UIConfigComboBoxCallback {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_COMBO_BOX_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct UIDrawerInitializeArrayElementData {
			UIDrawConfig* config;
			const char* name;
			size_t element_count;
		};

		struct UIDrawerArrayData {
			bool collapsing_header_state;
			bool drag_is_released;
			UIDrawPhase add_callback_phase;
			UIDrawPhase remove_callback_phase;
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
			const char* name;
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
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_ARRAY_PROVIDE_NAMES;
			}

			Stream<const char*> char_names = { nullptr, 0 };
			Stream<Stream<char>> stream_names = { nullptr, 0 };
			Action select_name_action = nullptr;
			void* select_name_action_data = nullptr;
			unsigned int select_name_action_capacity = 0;
		};

		struct ECSENGINE_API UIConfigArrayAddCallback {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_ARRAY_ADD_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigArrayRemoveCallback {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_ARRAY_REMOVE_CALLBACK;
			}

			UIActionHandler handler;
		};

		// The background is centered at the center of the sprite
		// Overwrite color black means ignore it - use the theme color
		struct ECSENGINE_API UIConfigSpriteButtonBackground {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SPRITE_BUTTON_BACKGROUND;
			}

			float2 scale;
			Color overwrite_color = ECS_COLOR_BLACK;
		};

		struct ECSENGINE_API UIConfigCheckBoxCallback {
			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_CHECK_BOX_CALLBACK;
			}
		
			UIActionHandler handler;
			bool disable_value_to_modify = true;
		};

	}

}