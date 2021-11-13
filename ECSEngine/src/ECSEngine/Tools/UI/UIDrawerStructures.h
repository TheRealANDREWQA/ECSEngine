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

	ECS_CONTAINERS;

	namespace Tools {

		enum class ECSENGINE_API UIDrawerMode : unsigned char {
			Indent,
			NextRow,
			NextRowCount,
			FitSpace,
			ColumnDraw,
			ColumnDrawFitSpace,
			Nothing
		};

		enum class ECSENGINE_API TextAlignment : unsigned char {
			Left,
			Right,
			Middle,
			Top,
			Bottom
		};

		enum class ECSENGINE_API WindowSizeTransformType : unsigned char {
			Horizontal,
			Vertical,
			Both
		};

		struct ECSENGINE_API UIDrawConfig {
			UIDrawConfig();

			template<typename ConfigFlag>
			void AddFlag(const ConfigFlag& flag) {
				size_t associated_bit = flag.GetAssociatedBit();
				for (size_t index = 0; index < flag_count; index++) {
					ECS_ASSERT(associated_bits[index] != associated_bit, "Repeated flag addition");
				}

				parameter_start[flag_count + 1] = parameter_start[flag_count] + flag.GetParameterCount();
				memcpy(parameters + parameter_start[flag_count], flag.GetParameters(), sizeof(float) * flag.GetParameterCount());
				associated_bits[flag_count] = associated_bit;
				flag_count++;
			}

			template<typename ConfigFlag1, typename ConfigFlag2>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2) {
				AddFlag(flag1);
				AddFlag(flag2);
			}

			template<typename ConfigFlag1, typename ConfigFlag2, typename ConfigFlag3>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2, const ConfigFlag3& flag3) {
				AddFlag(flag1);
				AddFlag(flag2);
				AddFlag(flag3);
			}

			template<typename ConfigFlag1, typename ConfigFlag2, typename ConfigFlag3, typename ConfigFlag4>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2, const ConfigFlag3& flag3, const ConfigFlag4& flag4) {
				AddFlag(flag1);
				AddFlag(flag2);
				AddFlag(flag3);
				AddFlag(flag4);
			}

			template<typename ConfigFlag1, typename ConfigFlag2, typename ConfigFlag3, typename ConfigFlag4, typename ConfigFlag5>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2, const ConfigFlag3& flag3, const ConfigFlag4& flag4, const ConfigFlag5& flag5) {
				AddFlag(flag1);
				AddFlag(flag2);
				AddFlag(flag3);
				AddFlag(flag4);
				AddFlag(flag5);
			}

			// it returns the replaced config flag
			template<typename ConfigFlag>
			void SetExistingFlag(const ConfigFlag& flag, size_t bit_flag, ConfigFlag& previous_flag) {
				for (size_t index = 0; index < flag_count; index++) {
					if (associated_bits[index] == bit_flag) {
						ConfigFlag* ptr = (ConfigFlag*)(parameters + parameter_start[index]);
						previous_flag = *ptr;
						*ptr = flag;
					}
				}
			}

			template<typename ConfigFlag>
			void RestoreFlag(const ConfigFlag& previous_flag, size_t bit_flag) {
				for (size_t index = 0; index < flag_count; index++) {
					if (associated_bits[index] == bit_flag) {
						ConfigFlag* flag = (ConfigFlag*)(parameters + parameter_start[index]);
						*flag = previous_flag;
						return;
					}
				}
			}

			const void* GetParameter(size_t bit_flag) const;

			size_t flag_count;
			unsigned int parameter_start[16];
			float parameters[64];
			size_t associated_bits[16];
		};

		struct ECSENGINE_API UIDrawSmallConfig {
			UIDrawSmallConfig();

			template<typename ConfigFlag>
			void AddFlag(const ConfigFlag& flag) {
				parameter_start[flag_count + 1] = parameter_start[flag_count] + flag.GetParameterCount();
				memcpy(parameters + parameter_start[flag_count], flag.GetParameters(), sizeof(float) * flag.GetParameterCount());
				associated_bits[flag_count] = flag.GetAssociatedBit();
				flag_count++;
			}
			template<typename ConfigFlag1, typename ConfigFlag2>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2) {
				AddFlag(flag1);
				AddFlag(flag2);
			}
			template<typename ConfigFlag1, typename ConfigFlag2, typename ConfigFlag3>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2, const ConfigFlag3& flag3) {
				AddFlag(flag1);
				AddFlag(flag2);
				AddFlag(flag3);
			}
			template<typename ConfigFlag1, typename ConfigFlag2, typename ConfigFlag3, typename ConfigFlag4>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2, const ConfigFlag3& flag3, const ConfigFlag4& flag4) {
				AddFlag(flag1);
				AddFlag(flag2);
				AddFlag(flag3);
				AddFlag(flag4);
			}
			template<typename ConfigFlag1, typename ConfigFlag2, typename ConfigFlag3, typename ConfigFlag4, typename ConfigFlag5>
			void AddFlags(const ConfigFlag1& flag1, const ConfigFlag2& flag2, const ConfigFlag3& flag3, const ConfigFlag4& flag4, const ConfigFlag5& flag5) {
				AddFlag(flag1);
				AddFlag(flag2);
				AddFlag(flag3);
				AddFlag(flag4);
				AddFlag(flag5);
			}

			const float* GetParameter(size_t bit_flag) const;

			size_t flag_count;
			unsigned int parameter_start[4];
			float parameters[4];
			size_t associated_bits[4];
		};


		struct ECSENGINE_API UIConfigAbsoluteTransform {

			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return 4;
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_ABSOLUTE_TRANSFORM;
			}

			float2 position;
			float2 scale;
		};

		struct ECSENGINE_API UIConfigRelativeTransform {

			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return 4;
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_RELATIVE_TRANSFORM;
			}

			float2 offset = { 0.0f, 0.0f };
			float2 scale = { 1.0f, 1.0f };
		};

		struct ECSENGINE_API UIConfigWindowDependentSize {

			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigWindowDependentSize) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_WINDOW_DEPENDENT_SIZE;
			}

			WindowSizeTransformType type = WindowSizeTransformType::Horizontal;
			float2 offset = { 0.0f, 0.0f };
			float2 scale_factor = { 1.0f, 1.0f };
		};

		struct ECSENGINE_API UIConfigTextParameters {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigTextParameters) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_PARAMETERS;
			}

			Color color = ECS_TOOLS_UI_TEXT_COLOR;
			float2 size = { ECS_TOOLS_UI_FONT_SIZE * ECS_TOOLS_UI_FONT_X_FACTOR, ECS_TOOLS_UI_FONT_SIZE };
			float character_spacing = ECS_TOOLS_UI_FONT_CHARACTER_SPACING;
		};

		struct ECSENGINE_API UIConfigTextAlignment {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return 1;
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_ALIGNMENT;
			}

			TextAlignment horizontal = TextAlignment::Middle;
			TextAlignment vertical = TextAlignment::Middle;
		private:
			unsigned char padding[2];
		};

		struct ECSENGINE_API UIConfigColor {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return 1;
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_COLOR;
			}

			Color color = ECS_TOOLS_UI_THEME_COLOR;
		};

		struct ECSENGINE_API UIConfigButtonHoverable {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigButtonHoverable) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_BUTTON_HOVERABLE;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigSliderColor {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return 1;
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_COLOR;
			}

			Color color = ToneColor(ECS_TOOLS_UI_THEME_COLOR, ECS_TOOLS_UI_SLIDER_LIGHTEN_FACTOR);
		};

		struct ECSENGINE_API UIConfigSliderShrink {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return 2;
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_SHRINK;
			}

			float2 value = { ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_X, ECS_TOOLS_UI_SLIDER_SHRINK_FACTOR_Y };
		};

		struct ECSENGINE_API UIConfigSliderPadding {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return 1;
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_PADDING;
			}

			float value = ECS_TOOLS_UI_SLIDER_PADDING_X;
		};

		struct ECSENGINE_API UIConfigSliderLength {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return 1;
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_LENGTH;
			}

			float value = ECS_TOOLS_UI_SLIDER_LENGTH_X;
		};

		struct ECSENGINE_API UIConfigBorder {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigBorder) / sizeof(float);
			}
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

		// Value must implement ToString in order to be used
		template<typename Type, typename SubType>
		struct UIDrawerSliderBaseType {

			SubType* Pointer() {
				return value.Pointer();
			}

			const SubType* ConstPointer() const {
				return value.ConstPointer();
			}

			SubType Value() const {
				return value.Value();
			}

			static SubType Interpolate(const UIDrawerSliderBaseType<Type, SubType>& lower_bound, const UIDrawerSliderBaseType<Type, SubType>& upper_bound, float percentage) {
				return lower_bound.Value() + (upper_bound.Value() - lower_bound.Value()) * percentage;
			}
		
			static float Percentage(const UIDrawerSliderBaseType<Type, SubType>& lower_bound, const UIDrawerSliderBaseType<Type, SubType>& upper_bound, const UIDrawerSliderBaseType<Type, SubType>& value) {
				return (float)(*value.ConstPointer() - lower_bound.Value()) / (float)(upper_bound.Value() - lower_bound.Value());
			}

			void ConstructValue(SubType _value) {
				value.ConstructValue(_value);
			}

			void ConstructPointer(SubType* pointer) {
				value.ConstructPointer(pointer);
			}

			void ConstructExtraData(const void* extra_data) {
				value.ConstructExtraData(extra_data);
			}

			static size_t ByteSize() {
				return Type::ByteSize();
			}

			// used to convert when changing value
			template<typename Stream>
			void ToString(Stream& characters) const {
				characters.size = 0;
				value.ToString(characters);
			}

			template<typename Stream>
			static bool ValidateTextInput(Stream& characters) {
				return Type::ValidateTextInput(characters);
			}

			template<typename Stream>
			static void ConvertTextInput(Stream& characters, void* value_to_change) {
				SubType* reinterpretation = (SubType*)value_to_change;
				*reinterpretation = Type::ConvertTextInput(characters);
			}

			// used to convert for enter values
			template<typename Stream>
			static void ConvertToCharacters(Stream& characters, const void* value_to_convert) {
				const SubType* reinterpretation = (const SubType*)value_to_convert;
				Type::ConvertToCharacters(characters, reinterpretation);
			}

			Type value;
		};

		struct SliderFloatBase {
			template<typename Stream>
			void ToString(Stream& characters) const {
				function::ConvertFloatToChars(characters, *pointer, precision);
			}

			float* Pointer() {
				return pointer;
			}

			const float* ConstPointer() const {
				return pointer;
			}

			float Value() const {
				return value;
			}

			void ConstructValue(float _value) {
				value = _value;
			}

			void ConstructPointer(float* _pointer) {
				pointer = _pointer;
			}

			void ConstructExtraData(const void* extra_data) {
				precision = *(const unsigned int*)extra_data;
			}

			static size_t ByteSize() {
				return 4;
			}

			template<typename Stream>
			static bool ValidateTextInput(Stream& stream) {
				return stream.size > 0;
			}

			template<typename Stream>
			static float ConvertTextInput(Stream& characters) {
				return function::ConvertCharactersToFloat(characters);
			}

			template<typename Stream>
			static void ConvertToCharacters(Stream& characters, float* ptr) {
				function::ConvertFloatToChars(*ptr, characters);
			}

			unsigned int precision;
			float value;
			float* pointer;
		};

		struct SliderDoubleBase {
			template<typename Stream>
			void ToString(Stream& characters) const {
				function::ConvertDoubleToChars(characters, *pointer, precision);
			}

			double* Pointer() {
				return pointer;
			}

			const double* ConstPointer() const {
				return pointer;
			}

			double Value() const {
				return value;
			}

			void ConstructValue(double _value) {
				value = _value;
			}

			void ConstructPointer(double* _pointer) {
				pointer = _pointer;
			}

			void ConstructExtraData(const void* extra_data) {
				precision = *(const size_t*)extra_data;
			}

			static size_t ByteSize() {
				return 8;
			}

			template<typename Stream>
			static bool ValidateTextInput(Stream& characters) {
				return characters.size > 0;
			}

			template<typename Stream>
			static double ConvertTextInput(Stream& characters) {
				return function::ConvertCharactersToDouble(characters);
			}

			template<typename Stream>
			static void ConvertToCharacters(Stream& characters, double* ptr) {
				function::ConvertDoubleToChars(*ptr, characters);
			}

			size_t precision;
			double value;
			double* pointer;
		};

		template<typename Integer>
		struct SliderIntegerBase {

			template<typename Stream>
			void ToString(Stream& characters) const {
				function::ConvertIntToCharsFormatted(characters, *pointer);
			}

			Integer* Pointer() {
				return pointer;
			}

			const Integer* ConstPointer() const {
				return pointer;
			}

			Integer Value() const {
				return value;
			}

			void ConstructValue(Integer _value) {
				value = _value;
			}

			void ConstructPointer(Integer* _pointer) {
				pointer = _pointer;
			}

			void ConstructExtraData(const void* extra_data) {}

			static size_t ByteSize() {
				return sizeof(Integer);
			}

			template<typename Stream>
			static bool ValidateTextInput(Stream& characters) {
				return characters.size > 0;
			}

			template<typename Stream>
			static Integer ConvertTextInput(Stream& characters) {
				return function::ConvertCharactersToInt<Integer>(characters);
			}

			Integer value;
			Integer* pointer;
		};

		using SliderFloat = UIDrawerSliderBaseType<SliderFloatBase, float>;

		using SliderDouble = UIDrawerSliderBaseType<SliderDoubleBase, double>;

		template<typename Integer>
		using SliderInteger = UIDrawerSliderBaseType<SliderIntegerBase<Integer>, Integer>;

		struct ECSENGINE_API UIConfigTextInputHint {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigTextInputHint) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_INPUT_HINT;
			}

			const char* characters;
		};

		struct ECSENGINE_API UIConfigTextInputCallback {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigTextInputCallback);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_TEXT_INPUT_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigSliderChangedValueCallback {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigSliderChangedValueCallback) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigHoverableAction {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigHoverableAction) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_HOVERABLE_ACTION;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigClickableAction {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigClickableAction) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_CLICKABLE_ACTION;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigGeneralAction {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigGeneralAction) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_GENERAL_ACTION;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIConfigRectangleVertexColor {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigRectangleVertexColor) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_RECTANGLE_VERTEX_COLOR;
			}

			Color colors[4];
		};

		struct ECSENGINE_API UIConfigSpriteGradient {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigSpriteGradient) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_SPRITE_GRADIENT;
			}

			LPCWSTR texture;
			float2 top_left_uv;
			float2 bottom_right_uv;
		};

		struct ECSENGINE_API UIDrawerCollapsingHeader {
			UIDrawerTextElement name;
			bool state;
		};

		// can be drawer or initializer
		using UINodeDraw = void (*)(void* drawer_ptr, void* window_data, void* other_ptr);

		struct ECSENGINE_API UIDrawerHierarchyNode {
			UIDrawerTextElement name_element;
			const char* name;
			unsigned int name_length;
			bool state;
			// if it is the first time it is drawn, make sure to pin the y axis
			bool has_been_pinned;
			UIDynamicStream<void*> internal_allocations;
			UINodeDraw function;
		};

		struct ECSENGINE_API UIDrawerHierarchyPendingNode {
			UINodeDraw function;
			unsigned int bound_node;
		};

		struct ECSENGINE_API UIDrawerHierarchiesElement {
			void* hierarchy;
			unsigned int node_index;
			unsigned int children_count;
		};

		// used to coordinate multiple hierarchies - one parent and the rest child
		struct ECSENGINE_API UIDrawerHierarchiesData {
			UIDynamicStream<UIElementTransform> hierarchy_transforms;
			UIDynamicStream<UIDrawerHierarchiesElement> elements;
			void* active_hierarchy;
		};

		struct ECSENGINE_API UIDrawerHierarchySelectable {
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
				nodes[current_index].internal_allocations.allocator = system->m_memory;

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
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigHierarchySpriteTexture) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_HIERARCHY_SPRITE_TEXTURE;
			}

			LPCWSTR texture;
			float2 top_left_closed_uv = { 0.0f, 0.0f };
			float2 bottom_right_closed_uv = { 1.0f, 1.0f };
			float2 top_left_opened_uv = {0.0f, 0.0f};
			float2 bottom_right_opened_uv = {1.0f, 1.0f};
			float2 scale_factor = {1.0f, 1.0f};
			Color color = ECS_COLOR_WHITE;
			bool keep_triangle = false;
		};

		struct ECSENGINE_API UIConfigHierarchyNoAction {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigHierarchyNoAction) / sizeof(float);
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME;
			}

			void* extra_information;
			float row_y_scale;
		};

		struct ECSENGINE_API UIConfigHierarchyChild {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return 2;
			}
			static inline constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_HIERARCHY_CHILD;
			}
			
			void* parent;
		};

		// The callback receives the hierarchy into additional data parameter
		// Pointer or callback can miss
		struct ECSENGINE_API UIConfigHierarchySelectable {
			inline const void* GetParameters() const {
				return this;
			}
			static inline constexpr size_t GetParameterCount() {
				return sizeof(UIConfigHierarchySelectable) / sizeof(float);
			}
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

		struct ECSENGINE_API UIDrawerSentenceNotCached {
			Stream<unsigned int> whitespace_characters;
		};

		struct ECSENGINE_API UIDrawerWhitespaceCharacter {
			unsigned short position;
			char type;
		};

		struct ECSENGINE_API UIDrawerSentenceBase {
			void SetWhitespaceCharacters(const char* characters, size_t character_count);

			Stream<UISpriteVertex> vertices;
			Stream<UIDrawerWhitespaceCharacter> whitespace_characters;
		};

		struct ECSENGINE_API UIDrawerSentenceCached {
			UIDrawerSentenceBase base;
			float2 zoom;
			float2 inverse_zoom;
		};

		struct ECSENGINE_API UIDrawerTextTable {
			Stream<Stream<UISpriteVertex>> labels;
			Stream<float> column_scales;
			float row_scale;
			float2 zoom;
			float2 inverse_zoom;
		};

		struct ECSENGINE_API UIConfigGraphKeepResolutionX {
			inline const void* GetParameters() const {
				return this;
			}
			constexpr inline static size_t GetParameterCount() {
				return 2;
			}
			constexpr inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_KEEP_RESOLUTION_X;
			}

			float max_y_scale = 10000.0f;
			float min_y_scale = 0.2f;
		};

		struct ECSENGINE_API UIConfigGraphKeepResolutionY {
			inline const void* GetParameters() const {
				return this;
			}
			constexpr inline static size_t GetParameterCount() {
				return 2;
			}
			constexpr inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_KEEP_RESOLUTION_Y;
			}

			float max_x_scale = 10000.0f;
			float min_x_scale = 0.2f;
		};

		struct ECSENGINE_API UIConfigGraphColor {
			inline const void* GetParameters() const {
				return this;
			}
			constexpr inline static size_t GetParameterCount() {
				return 1;
			}
			constexpr inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_DROP_COLOR;
			}

			Color color;
		};

		struct ECSENGINE_API UIConfigHistogramColor {
			inline const void* GetParameters() const {
				return this;
			}
			constexpr inline static size_t GetParameterCount() {
				return 1;
			}
			constexpr inline static size_t GetAssociatedBit() {
				return UI_CONFIG_HISTOGRAM_COLOR;
			}

			Color color;
		};

		struct ECSENGINE_API UIDrawerGraphTagData {
			void* drawer;
			float2 position;
			float2 graph_position;
			float2 graph_scale;
		};

		struct ECSENGINE_API UIConfigGraphMinY {
			inline const void* GetParameters() const {
				return this;
			}
			constexpr inline static size_t GetParameterCount() {
				return 1;
			}
			constexpr inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_MIN_Y;
			}

			float value;
		};

		struct ECSENGINE_API UIConfigGraphMaxY {
			inline const void* GetParameters() const {
				return this;
			}
			constexpr inline static size_t GetParameterCount() {
				return 1;
			}
			constexpr inline static size_t GetAssociatedBit() {
				return UI_CONFIG_GRAPH_MAX_Y;
			}

			float value;
		};

		struct ECSENGINE_API UIConfigGraphTags {
			inline const void* GetParameters() const {
				return this;
			}
			constexpr inline static size_t GetParameterCount() {
				return sizeof(UIConfigGraphTags) / sizeof(float);
			}
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

		struct ECSENGINE_API UIDrawerMenuWindow {
			const char* name;
			float2 position;
			float2 scale;
		};

		struct ECSENGINE_API UIDrawerMenuState {
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
				if (other == nullptr)
					return false;
				else {
					return name == other->name;
				}
			}

			UIDrawerTextElement* name;
			bool is_opened;
			CapacityStream<UIDrawerMenuWindow> windows;
			UIDrawerMenuState state;
		};

		struct ECSENGINE_API UIDrawerMenuGeneralData {
			UIDrawerMenu* menu;
			UIDrawerMenuState* destroy_state = nullptr;
			unsigned char menu_initializer_index = 255;
			float2 destroy_position;
			float2 destroy_scale;
			bool initialize_from_right_click = false;
			bool is_opened_when_clicked = false;
		};

		struct ECSENGINE_API UIDrawerMenuDrawWindowData {
			UIDrawerMenu* menu;
			UIDrawerMenuState* state;
		};


		struct ECSENGINE_API UIDrawerSubmenuHoverable {
			inline bool IsTheSameData(const UIDrawerSubmenuHoverable* other) const {
				if (other == nullptr) {
					return false;
				}
				else {
					return state == other->state && row_index == other->row_index;
				}
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
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigColorInputCallback) / sizeof(float);
			}
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

		struct ECSENGINE_API UIParameterWindowReturnToDefaultButtonData {
			UIWindowDrawerDescriptor* window_descriptor;
			void* system_descriptor;
			void* default_descriptor;
			bool is_system_theme;
			UIWindowDrawerDescriptorIndex descriptor_index;
			unsigned int descriptor_size;
		};

		struct ECSENGINE_API UIConfigToolTip {
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigToolTip) / sizeof(float);
			}

			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_TOOL_TIP;
			}

			const char* characters;
		};

		struct ECSENGINE_API UIDrawerStateTable {
			UIDrawerTextElement name;
			Stream<UIDrawerTextElement> labels;
			unsigned int max_x_scale;
		};

		struct ECSENGINE_API UIDrawerMenuButtonData {
			UIWindowDescriptor descriptor;
			size_t border_flags;
			bool is_opened_when_pressed;
		};

		struct ECSENGINE_API UIDrawerFilterMenuData {
			const char* window_name;
			const char* name;
			Stream<const char*> labels;
			bool** states;
			bool draw_all;
			bool* notifier;
		};

		struct ECSENGINE_API UIDrawerFilterMenuSinglePointerData {
			const char* window_name;
			const char* name;
			Stream<const char*> labels;
			bool* states;
			bool draw_all;
			bool* notifier;
		};

		struct ECSENGINE_API UIDrawerBufferState {
			size_t solid_color_count;
			size_t text_sprite_count;
			size_t sprite_count;
			size_t sprite_cluster_count;
		};

		struct ECSENGINE_API UIDrawerHandlerState {
			size_t hoverable_count;
			size_t clickable_count;
			size_t general_count;
		};

		struct ECSENGINE_API UIConfigStateTableNotify {
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigStateTableNotify) / sizeof(float);
			}

			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE;
			}

			bool* notifier;
		};

		using UIConfigFilterMenuNotify = UIConfigStateTableNotify;

		struct ECSENGINE_API UIDrawerStateTableBoolClickable {
			bool* notifier;
			bool* state;
		};

		struct ECSENGINE_API UIConfigComboBoxPrefix {
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigComboBoxPrefix) / sizeof(float);
			}

			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_COMBO_BOX_PREFIX;
			}

			const char* prefix;
		};

		struct ECSENGINE_API UIDrawerLabelHierarchyLabelData {
			bool state;
			unsigned char activation_count;
		};

		struct ECSENGINE_API UIDrawerLabelHierarchy {
			Stream<char> active_label;
			Stream<char> selected_label_temporary;
			Stream<char> right_click_label_temporary;
			IdentifierHashTable<UIDrawerLabelHierarchyLabelData, ResourceIdentifier, HashFunctionPowerOfTwo> label_states;
			Action selectable_callback;
			void* selectable_callback_data;
			Action right_click_callback;
			void* right_click_callback_data;
		};

		struct ECSENGINE_API UIConfigLabelHierarchySpriteTexture {
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigLabelHierarchySpriteTexture) / sizeof(float);
			}

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
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigLabelHierarchySelectableCallback) / sizeof(float);
			}

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
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigLabelHierarchyHashTableCapacity) / sizeof(float);
			}

			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_LABEL_HIERARCHY_HASH_TABLE_CAPACITY;
			}

			unsigned int capacity;
		};

		// The callback must cast to UIDrawerLabelHierarchyRightClickData the _data pointer in order to get access to the 
		// label stream; if copy_on_initialization is set, then the data parameter will be copied only in the initializer
		// pass
		struct ECSENGINE_API UIConfigLabelHierarchyRightClick {
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigLabelHierarchyRightClick) / sizeof(float);
			}

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
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigSentenceHoverableHandlers) / sizeof(float);
			}

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
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigSentenceClickableHandlers) / sizeof(float);
			}

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
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigSentenceGeneralHandlers) / sizeof(float);
			}

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
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigMenuButtonSprite) / sizeof(float);
			}

			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_MENU_BUTTON_SPRITE;
			}

			LPCWSTR texture;
			float2 top_left_uv = { 0.0f, 0.0f };
			float2 bottom_right_uv = { 1.0f, 1.0f };
			Color color = ECS_COLOR_WHITE;
		};

		using UIConfigMenuSprite = UIConfigMenuButtonSprite;

		struct ECSENGINE_API UIConfigActiveState {
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigActiveState) / sizeof(float);
			}

			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_ACTIVE_STATE;
			}

			bool state;
			char reserved[7];
		};

		struct ECSENGINE_API UIConfigCollapsingHeaderDoNotCache {
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigCollapsingHeaderDoNotCache) / sizeof(float);
			}

			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_DO_NOT_CACHE;
			}

			bool* state;
		};

		struct ECSENGINE_API UIDrawerInitializeComboBox {
			UIDrawConfig* config;
			const char* name;
			Stream<const char*> labels;
			unsigned int label_display_count;
			unsigned char* active_label;
		};

		struct ECSENGINE_API UIDrawerInitializeColorInput {
			UIDrawConfig* config;
			const char* name;
			Color* color;
		};

		struct ECSENGINE_API UIDrawerInitializeFilterMenu {
			UIDrawConfig* config;
			const char* name;
			Stream<const char*> label_names;
			bool** states;
		};

		struct ECSENGINE_API UIDrawerInitializeFilterMenuSinglePointer {
			UIDrawConfig* config;
			const char* name;
			Stream<const char*> label_names;
			bool* states;
		};

		struct ECSENGINE_API UIDrawerInitializeHierarchy {
			UIDrawConfig* config;
			const char* name;
		};

		struct ECSENGINE_API UIDrawerInitializeLabelHierarchy {
			UIDrawConfig* config;
			const char* identifier;
			Stream<const char*> labels;
		};

		struct ECSENGINE_API UIDrawerInitializeList {
			UIDrawConfig* config;
			const char* name;
		};

		struct ECSENGINE_API UIDrawerInitializeMenu {
			UIDrawConfig* config;
			const char* name;
			UIDrawerMenuState* state;
		};

		struct ECSENGINE_API UIDrawerInitializeFloatInput {
			UIDrawConfig* config;
			const char* name; 
			float* value; 
			float lower_bound = -FLT_MAX;
			float upper_bound = FLT_MAX;
		};

		template<typename Integer>
		struct UIDrawerInitializeIntegerInput {
			UIDrawConfig* config; 
			const char* name; 
			Integer* value;
			Integer min = LLONG_MIN;
			Integer max = ULLONG_MAX;
		};

		struct ECSENGINE_API UIDrawerInitializeDoubleInput {
			UIDrawConfig* config;
			const char* name; 
			double* value; 
			double lower_bound = -DBL_MAX; 
			double upper_bound = DBL_MAX;
		};

		struct ECSENGINE_API UIDrawerInitializeFloatInputGroup {
			UIDrawConfig* config;
			size_t count;
			const char* ECS_RESTRICT group_name;
			const char** ECS_RESTRICT names;
			float** ECS_RESTRICT values;
			const float* ECS_RESTRICT lower_bound;
			const float* ECS_RESTRICT upper_bound;
		};

		struct ECSENGINE_API UIDrawerInitializeDoubleInputGroup {
			UIDrawConfig* config;
			size_t count;
			const char* ECS_RESTRICT group_name;
			const char** ECS_RESTRICT names;
			double** ECS_RESTRICT values;
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
			const Integer* ECS_RESTRICT lower_bound;
			const Integer* ECS_RESTRICT upper_bound;
		};

		template<typename Value>
		struct UIDrawerInitializeSlider {
			UIDrawConfig* config;
			const char* name;
			Value value_to_modify;
			Value lower_bound;
			Value upper_bound;
		};

		template<typename Value>
		struct UIDrawerInitializeSliderGroup {
			UIDrawConfig* config;
			size_t count;
			const char* group_name;
			const char** names;
			Value* values_to_modify;
			const Value* lower_bounds;
			const Value* upper_bounds;
		};

		struct ECSENGINE_API UIDrawerInitializeStateTable {
			UIDrawConfig* config;
			const char* name;
			Stream<const char*> labels;
			bool** states;
		};

		struct ECSENGINE_API UIDrawerInitializeStateTableSinglePointer {
			UIDrawConfig* config;
			const char* name;
			Stream<const char*> labels;
			bool* states;
		};

		struct ECSENGINE_API UIDrawerInitializeTextInput {
			UIDrawConfig* config;
			const char* name;
			CapacityStream<char>* text_to_fill;
		};

		struct ECSENGINE_API UIDrawerLabelList {
			UIDrawerTextElement name;
			Stream<UIDrawerTextElement> labels;
		};

		struct ECSENGINE_API UIConfigCollapsingHeaderSelection {
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return 1;
			}

			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_COLLAPSING_HEADER_SELECTION;
			}

			bool is_selected;
		};

		struct ECSENGINE_API UIConfigGetTransform {
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigGetTransform);
			}

			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_GET_TRANSFORM;
			}

			float2* position;
			float2* scale;
		};

		// Phase will always be UIDrawPhase::System
		struct ECSENGINE_API UIConfigComboBoxCallback {
			inline const void* GetParameters() const {
				return this;
			}

			inline static constexpr size_t GetParameterCount() {
				return sizeof(UIConfigComboBoxCallback);
			}

			inline static constexpr size_t GetAssociatedBit() {
				return UI_CONFIG_COMBO_BOX_CALLBACK;
			}

			UIActionHandler handler;
		};

		struct ECSENGINE_API UIDrawerInitializeArrayElementData {
			UIDrawConfig* config;
			const char* name;
			size_t element_count;
		};

		struct ECSENGINE_API UIDrawerArrayData {
			bool collapsing_header_state;
			bool drag_is_released;
			unsigned int drag_index;
			float drag_current_position;
			float row_y_scale;
			unsigned int previous_element_count;
		};

		struct ECSENGINE_API UIDrawerArrayDrawData {
			void* element_data;
			void* additional_data;
			unsigned int current_index;
		};

		struct ECSENGINE_API UIDrawerInitializeColorFloatInput {
			UIDrawConfig* config;
			const char* name;
			ColorFloat* color;
		};

	}

}