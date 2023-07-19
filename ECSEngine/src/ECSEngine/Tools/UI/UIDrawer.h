#pragma once
#include "UISystem.h"
#include "UIDrawConfig.h"
#include "UIDrawConfigs.h"
#include "UIDrawerActionStructures.h"
#include "UIDrawerActions.h"
#include "UIResourcePaths.h"
#include "../../Utilities/TreeIterator.h"

#define UI_HIERARCHY_NODE_FUNCTION ECSEngine::Tools::UIDrawer* drawer = (ECSEngine::Tools::UIDrawer*)drawer_ptr; \
												ECSEngine::Tools::UIDrawerHierarchy* hierarchy = (ECSEngine::Tools::UIDrawerHierarchy*)hierarchy_ptr

#define UI_LIST_NODE_FUNCTION  ECSEngine::Tools::UIDrawer* drawer = (ECSEngine::Tools::UIDrawer*)drawer_ptr; \
											ECSEngine::Tools::UIDrawerList* list = (ECSEngine::Tools::UIDrawerList*)list_ptr; \
											list->InitializeNodeYScale(drawer->GetCounts())

namespace ECSEngine {

	namespace Tools {

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Flag>
		void SetConfigParameter(size_t configuration, UIDrawConfig& config, const Flag& flag, Flag& previous_flag) {
			size_t bit_flag = flag.GetAssociatedBit();
			if (configuration & bit_flag) {
				config.SetExistingFlag(flag, previous_flag);
			}
			else {
				config.AddFlag(flag);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Flag>
		void RemoveConfigParameter(size_t configuration, UIDrawConfig& config, const Flag& previous_flag) {
			size_t bit_flag = previous_flag.GetAssociatedBit();
			if (configuration & bit_flag) {
				config.RestoreFlag(previous_flag);
			}
			else {
				config.flag_count--;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		inline size_t DynamicConfiguration(size_t configuration) {
			return function::SetFlag(configuration, UI_CONFIG_DO_CACHE) | UI_CONFIG_DYNAMIC_RESOURCE;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		struct UIDrawerArrayAddRemoveData {
			union {
				CapacityStream<void>* capacity_data;
				ResizableStream<void>* resizable_data;
			};
			bool is_resizable_data;
			// Used only for resizable_data to resize
			unsigned int element_byte_size;
			unsigned int new_size;
			UIDrawerArrayData* array_data;
		};

		ECSENGINE_API void UIDrawerArrayAddAction(ActionData* action_data);

		ECSENGINE_API void UIDrawerArrayRemoveAction(ActionData* action_data);

		ECSENGINE_API void UIDrawerArrayIntInputAction(ActionData* action_data);

		ECSENGINE_API void UIDrawerArrayRemoveAnywhereAction(ActionData* action_data);

		struct UIDrawer;

		// It supports at max 12 elements
		struct ECSENGINE_API UIDrawerRowLayout {
			// If adding an absolute element, the parameters represents its size.
			// When adding a relative element, the parameters are its relative size
			// When adding a window dependent size, the parameter x is ignored and instead it is assumed
			// that it stretches to accomodate the window. The y component is still used like in relative transform
			// The y component is used just to change the row size if it isn't enough
			// Make the y component 0.0f if you want to be the size of the row
			void AddElement(size_t transform_type, float2 parameters, ECS_UI_ALIGN alignment = ECS_UI_ALIGN_LEFT);

			void AddLabel(Stream<char> characters, ECS_UI_ALIGN alignment = ECS_UI_ALIGN_LEFT);

			// If the label size is defaulted with 0.0f, then it will use the row scale
			void AddSquareLabel(float label_size = 0.0f, ECS_UI_ALIGN alignment = ECS_UI_ALIGN_LEFT);

			// It will use the row scale
			void AddCheckBox(Stream<char> name = { nullptr, 0 }, ECS_UI_ALIGN alignment = ECS_UI_ALIGN_LEFT);

			void AddComboBox(
				Stream<Stream<char>> labels, 
				Stream<char> name = { nullptr, 0 }, 
				Stream<char> prefix = { nullptr, 0 }, 
				ECS_UI_ALIGN alignment = ECS_UI_ALIGN_LEFT
			);

			// If the thickness is left at 0.0f, then it will use the default border thickness
			void AddBorderToLastElement();

			// Combines the last elements and the resulting transform will be an absolute transform
			// This only works if the elements have the same alignment
			void CombineLastElements(unsigned int count = 2, bool keep_indents = false);

			// It will place the elements one right after the other like there is no indentation applied
			// But also takes into account window dependent elements
			void RemoveLastElementsIndentation(unsigned int count = 2);

			// Add the corresponding transform such that it gets rendered as it should be
			void GetTransform(UIDrawConfig& config, size_t& configuration);

			float2 GetScaleForElement(unsigned int index) const;

			// Reduces the x scale
			void IndentRow(float scale);

			void SetRowYScale(float scale);

			void SetIndentation(float indentation);

			void SetHorizontalAlignment(ECS_UI_ALIGN alignment);

			void SetVerticalAlignment(ECS_UI_ALIGN alignment);

			// If set to true, then the row won't change on the specified axis when scrolling
			void SetOffsetRenderRegion(bool2 should_offset);

			// Overrides the current thickness
			void SetBorderThickness(float thickness);

			void UpdateRowYScale(float scale);

			void UpdateSquareElements();

			// At the moment just 1 window dependent scale element is accepted
			void UpdateWindowDependentElements();

			// Sets the new positions of the elements according to the alignment
			void UpdateElementsFromAlignment();

			ECS_UI_ALIGN horizontal_alignment;
			ECS_UI_ALIGN vertical_alignment;

			UIDrawer* drawer;
			float2 row_scale; // This is the total scale
			float indentation;
			// This field is the actual double of the value given since the border is symmetrical
			float2 border_thickness;
			unsigned int current_index;
			unsigned int element_count;

#define MAX_ELEMENTS 8

			float2 element_sizes[MAX_ELEMENTS];
			// The indentation between elements. The indentation at index 0 is the indentation
			// before the first element, so the indentation between element 0 and 1 is at index 1
			float indentations[MAX_ELEMENTS]; 
			size_t element_transform_types[MAX_ELEMENTS];
			ECS_UI_ALIGN element_alignment[MAX_ELEMENTS];
			bool has_border[MAX_ELEMENTS];
			bool2 offset_render_region;

#undef MAX_ELEMENTS
		};

		// ------------------------------------------------------------------------------------------------------------------------------------

		struct ECSENGINE_API UIDrawer
		{
		public:	
			
			// ------------------------------------------------------------------------------------------------------------------------------------

			void CalculateRegionParameters(float2 position, float2 scale, const float2* render_offset);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DefaultDrawParameters();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ConvertTextToWindowResource(size_t configuration, const UIDrawConfig& config, Stream<char> text, UIDrawerTextElement* element, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ExpandRectangleFromFlag(const float* resize_factor, float2& position, float2& scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool ValidatePosition(size_t configuration, float2 position) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool ValidatePosition(size_t configuration, float2 position, float2 scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool ValidatePositionY(size_t configuration, float2 position, float2 scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool ValidatePositionMinBound(size_t configuration, float2 position, float2 scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIVertexColor* HandleSolidColorBuffer(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t* HandleSolidColorCount(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UISpriteVertex* HandleTextSpriteBuffer(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t* HandleTextSpriteCount(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UISpriteVertex* HandleSpriteBuffer(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t* HandleSpriteCount(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UISpriteVertex* HandleSpriteClusterBuffer(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t* HandleSpriteClusterCount(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIVertexColor* HandleLineBuffer(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t* HandleLineCount(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Line(size_t configuration, float2 positions1, float2 positions2, Color color);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleLateAndSystemDrawActionNullify(size_t configuration, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			ECS_UI_DRAW_PHASE HandlePhase(size_t configuration) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleTransformFlags(size_t configuration, const UIDrawConfig& config, float2& position, float2& scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleText(size_t configuration, const UIDrawConfig& config, Color& color, float2& font_size, float& character_spacing) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleTextStreamColorUpdate(Color color, Stream<UISpriteVertex> vertices) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleTextStreamColorUpdate(Color color, CapacityStream<UISpriteVertex> vertices) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleTextLabelAlignment(
				size_t configuration,
				const UIDrawConfig& config,
				float2 text_span,
				float2 label_size,
				float2 label_position,
				float& x_position,
				float& y_position,
				ECS_UI_ALIGN& horizontal_alignment,
				ECS_UI_ALIGN& vertical_alignment
			) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleTextLabelAlignment(
				float2 text_span,
				float2 label_scale,
				float2 label_position,
				float& x_position,
				float& y_position,
				ECS_UI_ALIGN horizontal,
				ECS_UI_ALIGN vertical
			) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleDynamicResource(size_t configuration, Stream<char> name);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleBorder(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			Color HandleColor(size_t configuration, const UIDrawConfig& config) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			Stream<char> HandleResourceIdentifier(Stream<char> input, bool permanent_buffer = false);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// This variant will check to see if the stack string contains the separation pattern
			// And not add it again
			Stream<char> HandleResourceIdentifierEx(Stream<char> input, bool permanent_buffer = false);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleRectangleActions(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextElement* HandleTextCopyFromResource(
				size_t configuration,
				Stream<char> text, 
				float2& position,
				Color font_color,
				float2 (*scale_from_text)(float2 scale, const UIDrawer& drawer)
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// if zoom differes between initialization and copying, then it will scale and will translate
			// as if it didn't change zoom; this is due to allow simple text to be copied and for bigger constructs
			// to determine their own change in translation needed to copy the element; since the buffer will reside
			// in cache because of the previous iteration, this should not have a big penalty on performance
			void HandleTextCopyFromResource(
				size_t configuration,
				UIDrawerTextElement* element,
				float2& position,
				Color font_color,
				float2(*scale_from_text)(float2 scale, const UIDrawer& drawer)
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// cull_mode 0 - horizontal
			// cull_mode 1 - vertical
			// cull_mode 2 - horizontal main, vertical secondary
			// cull_mode 3 - vertical main, horizontal secondary
			UIDrawerTextElement* HandleLabelTextCopyFromResourceWithCull(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> text,
				float2& position,
				float2 scale,
				float2& text_span,
				size_t& copy_count,
				size_t cull_mode
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// cull_mode 0 - horizontal
			// cull_mode 1 - vertical
			// cull_mode 2 - horizontal main, vertical secondary
			// cull_mode 3 - vertical main, horizontal secondary
			void HandleLabelTextCopyFromResourceWithCull(
				size_t configuration,
				const UIDrawConfig& config,
				UIDrawerTextElement* info,
				float2& position,
				float2 scale,
				float2& text_span,
				size_t& copy_count,
				size_t cull_mode
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool HandleFitSpaceRectangle(size_t configuration, float2& position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleSliderActions(
				size_t configuration,
				const UIDrawConfig& config,
				float2 position,
				float2 scale,
				Color color,
				float2 slider_position,
				float2 slider_scale,
				Color slider_color,
				UIDrawerSlider* info,
				const UIDrawerSliderFunctions& functions,
				UIDrawerTextInputFilter filter
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 HandleLabelSize(float2 text_span) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleSliderVariables(
				size_t configuration,
				const UIDrawConfig& config,
				float& length, 
				float& padding,
				float2& shrink_factor,
				Color& slider_color,
				Color background_color
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// The configuration is needed such that we know to which draw phase to write to
			void HandleAcquireDrag(size_t configuration, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SolidColorRectangle(size_t configuration, float2 position, float2 scale, Color color);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SolidColorRectangle(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SolidColorRectangle(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale, Color color);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// it will finalize the rectangle
			void TextLabel(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2& position, float2& scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// resource drawer
			void TextLabel(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2& position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Rectangle(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SpriteClusterRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				Stream<wchar_t> texture,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				Color color = ECS_COLOR_WHITE
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteClusterRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				Stream<wchar_t> texture,
				const Color* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteClusterRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				Stream<wchar_t> texture,
				const ColorFloat* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// resource drawer
			void Text(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextLabelWithCull(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextElement* TextInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> characters, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextElement* TextLabelInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> characters, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextLabelDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* element, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// single lined text input; drawer kernel
			void TextInputDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerTextInput* input, float2 position, float2 scale, UIDrawerTextInputFilter filter);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextInput* TextInputInitializer(size_t configuration, UIDrawConfig& config, Stream<char> name, CapacityStream<char>* text_to_fill, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerSlider* SliderInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				float2 position,
				float2 scale,
				unsigned int byte_size,
				void* value_to_modify,
				const void* lower_bound,
				const void* upper_bound,
				const void* default_value,
				const UIDrawerSliderFunctions& functions
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SliderDrawer(
				size_t configuration,
				const UIDrawConfig& config,
				UIDrawerSlider* slider,
				float2 position,
				float2 scale,
				void* value_to_modify,
				const void* lower_bound,
				const void* upper_bound,
				const UIDrawerSliderFunctions& functions,
				UIDrawerTextInputFilter filter
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SliderGroupDrawer(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				void** ECS_RESTRICT values_to_modify,
				const void* ECS_RESTRICT lower_bounds,
				const void* ECS_RESTRICT upper_bounds,
				const UIDrawerSliderFunctions& functions,
				UIDrawerTextInputFilter filter,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SliderGroupInitializer(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				unsigned int byte_size,
				void** ECS_RESTRICT values_to_modify,
				const void* ECS_RESTRICT lower_bounds,
				const void* ECS_RESTRICT upper_bounds,
				const void* ECS_RESTRICT default_values,
				const UIDrawerSliderFunctions& functions,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API UIDrawerSlider* IntSliderInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				float2 position,
				float2 scale,
				Integer* value_to_modify,
				Integer lower_bound,
				Integer upper_bound,
				Integer default_value = 0
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntSliderDrawer(
				size_t configuration,
				const UIDrawConfig& config,
				UIDrawerSlider* slider,
				float2 position,
				float2 scale,
				Integer* value_to_modify,
				Integer lower_bound,
				Integer upper_bound
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerSlider* FloatSliderInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				float2 position,
				float2 scale,
				float* value_to_modify,
				float lower_bound,
				float upper_bound,
				float default_value = 0.0f,
				unsigned int precision = 3
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatSliderDrawer(
				size_t configuration,
				const UIDrawConfig& config,
				UIDrawerSlider* slider,
				float2 position,
				float2 scale,
				float* value_to_modify,
				float lower_bound,
				float upper_bound,
				unsigned int precision = 3
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerSlider* DoubleSliderInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				float2 position,
				float2 scale,
				double* value_to_modify,
				double lower_bound,
				double upper_bound,
				double default_value = 0,
				unsigned int precision = 3
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleSliderDrawer(
				size_t configuration,
				const UIDrawConfig& config,
				UIDrawerSlider* slider,
				float2 position,
				float2 scale,
				double* value_to_modify,
				double lower_bound,
				double upper_bound,
				unsigned int precision = 3
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerComboBox* ComboBoxInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				Stream<Stream<char>> labels,
				unsigned int label_display_count,
				unsigned char* active_label,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextElement* CheckBoxInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void CheckBoxDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* element, bool* value_to_modify, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void CheckBoxDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, bool* value_to_modify, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ComboBoxDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerComboBox* data, unsigned char* active_label, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerColorInput* ColorInputInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				Color* color,
				Color default_color,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ColorInputDrawer(size_t configuration, UIDrawConfig& config, UIDrawerColorInput* data, float2 position, float2 scale, Color* color);

			UIDrawerCollapsingHeader* CollapsingHeaderInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void CollapsingHeaderDrawer(size_t configuration, UIDrawConfig& config, Stream<char> name, UIDrawerCollapsingHeader* data, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void CollapsingHeaderDrawer(size_t configuration, UIDrawConfig& config, Stream<char> name, bool* state, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerHierarchy* HierarchyInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename InitialValueInitializer, typename CallbackHover>
			UIDrawerTextInput* NumberInputInitializer(
				size_t configuration,
				UIDrawConfig& config, 
				Stream<char> name, 
				Action callback_action, 
				void* callback_data,
				size_t callback_data_size,
				float2 position, 
				float2 scale, 
				InitialValueInitializer&& initial_value_init,
				CallbackHover&& callback_hover
			) {
				// Begin recording allocations and table resources for dynamic resources
				if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
					BeginElement();
				}
				// kinda unnecessary memcpy's here but should not cost too much
				ECS_STACK_CAPACITY_STREAM(char, full_name, 256);
				Stream<char> identifier = HandleResourceIdentifier(name);
				full_name.Copy(identifier);
				full_name.AddStream("stream");
				full_name.AssertCapacity();
				
				CapacityStream<char>* stream = GetMainAllocatorBufferAndStoreAsResource<CapacityStream<char>>(full_name);
				stream->InitializeFromBuffer(GetMainAllocatorBuffer(sizeof(char) * 64, alignof(char)), 0, 63);
				initial_value_init(stream);

				UIConfigTextInputCallback initial_user_callback;
				// Implement callback - if it already has one, catch it and wrapp it
				if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
					UIConfigTextInputCallback* user_callback = (UIConfigTextInputCallback*)config.GetParameter(UI_CONFIG_TEXT_INPUT_CALLBACK);
					initial_user_callback = *user_callback;

					// Coallesce the allocation
					void* allocation = GetMainAllocatorBuffer(callback_data_size + user_callback->handler.data_size);
					memcpy(allocation, callback_data, callback_data_size);
					UIDrawerNumberInputCallbackData* base_data = (UIDrawerNumberInputCallbackData*)allocation;
					base_data->user_action = user_callback->handler.action;
					
					if (user_callback->handler.data_size > 0) {
						void* user_data = function::OffsetPointer(base_data, callback_data_size);
						memcpy(user_data, user_callback->handler.data, user_callback->handler.data_size);
						// Indicate that the user data is relative
						base_data->user_action_data_offset = callback_data_size;
						base_data->relative_user_data = true;
					}
					else {
						base_data->user_action_data = user_callback->handler.data;
						base_data->relative_user_data = false;
					}

					user_callback->handler.action = callback_action;
					user_callback->handler.data = allocation;
					user_callback->handler.data_size = 0;
				}
				else {
					UIConfigTextInputCallback callback;
					callback.handler = { callback_action, callback_data, static_cast<unsigned int>(callback_data_size) };
					// Nullify the user action and data callbacks
					UIDrawerNumberInputCallbackData* base_data = (UIDrawerNumberInputCallbackData*)callback_data;
					base_data->user_action = nullptr;
					base_data->user_action_data = nullptr;
					config.AddFlag(callback);
				}
				
				UIDrawerTextInput* input = TextInputInitializer(
					configuration | UI_CONFIG_TEXT_INPUT_CALLBACK | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN,
					config,
					name, 
					stream, 
					position, 
					scale
				);
				
				// Restore the previous text input callback
				if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
					UIConfigTextInputCallback* user_callback = (UIConfigTextInputCallback*)config.GetParameter(UI_CONFIG_TEXT_INPUT_CALLBACK);
					*user_callback = initial_user_callback;
				}

				// Type pun all types - they all have as the third data member the text input followed by the bool to indicate
				// whether or not return to default is allowed
				UIDrawerNumberInputCallbackData* callback_input_ptr = (UIDrawerNumberInputCallbackData*)input->callback_data;
				callback_input_ptr->input = input;
				callback_input_ptr->return_to_default = true;
				callback_input_ptr->display_range = function::HasFlag(configuration, UI_CONFIG_NUMBER_INPUT_RANGE);
				if (~configuration & UI_CONFIG_NUMBER_INPUT_DEFAULT) {
					callback_input_ptr->return_to_default = false;
				}
				if (~configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
					config.flag_count--;
				}

				full_name.size = identifier.size;
				full_name.AddStream("tool tip");
				char* tool_tip_characters = (char*)GetMainAllocatorBufferAndStoreAsResource(full_name, sizeof(char) * 64, alignof(char));
				Stream<char> tool_tip_stream = Stream<char>(tool_tip_characters, 0);
				tool_tip_stream.Add('[');
				callback_hover(tool_tip_stream);
				tool_tip_stream.Add(']');
				tool_tip_stream[tool_tip_stream.size] = '\0';

				return input;
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Lambda>
			void NumberInputDrawer(
				size_t configuration,
				const UIDrawConfig& config, 
				Stream<char> name, 
				void* number,
				Action hoverable_action,
				Action wrapper_hoverable_action,
				Action draggable_action,
				void* draggable_data,
				unsigned int draggable_data_size,
				float2 position,
				float2 scale, 
				Lambda&& lambda
			) {
				constexpr float DRAG_X_THRESHOLD = 0.0175f;
				UIDrawerTextInput* input = (UIDrawerTextInput*)GetResource(name);

				// Only update the tool tip if it is visible in the y dimension
				bool is_valid_y = ValidatePositionY(configuration, position, scale);
				const char* tool_tip_characters = nullptr;
				if (is_valid_y) {
					// Update the pointer in the callback data - if any
					if (input->HasCallback()) {
						// The layout is like this:
						// struct {
						//		UIDrawerNumberInputCallbackData data;
						//		float*/double*/Integer* number;
						// }
						void** callback_pointer = (void**)function::OffsetPointer(input->callback_data, sizeof(UIDrawerNumberInputCallbackData));
						*callback_pointer = number;
					}

					// If we have an exterior callback that needs update do it before
					if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
						const UIConfigTextInputCallback* callback = (const UIConfigTextInputCallback*)config.GetParameter(UI_CONFIG_TEXT_INPUT_CALLBACK);
						if (callback->handler.data_size > 0 && !callback->copy_on_initialization) {
							UIDrawerNumberInputCallbackData* base_data = (UIDrawerNumberInputCallbackData*)input->callback_data;
							void* user_data = base_data->GetUserData();
							memcpy(user_data, callback->handler.data, callback->handler.data_size);
						}
					}

					size_t text_count_before = *HandleTextSpriteCount(configuration);
					// The callback will still be triggered. In this way we avoid having the text input recopying the data
					TextInputDrawer(function::ClearFlag(configuration, UI_CONFIG_TEXT_INPUT_CALLBACK), config, input, position, scale, UIDrawerTextInputFilterNumbers);

					Stream<char> tool_tip_stream;
					Stream<char> identifier = HandleResourceIdentifier(name);
					ECS_STACK_CAPACITY_STREAM(char, tool_tip_name, 512);
					tool_tip_name.Copy(identifier);
					tool_tip_name.AddStream("tool tip");
					tool_tip_name.AssertCapacity();

					tool_tip_characters = (char*)FindWindowResource(tool_tip_name);
					tool_tip_stream = Stream<char>(tool_tip_characters, 0);

					lambda(input, tool_tip_stream);

					UISpriteVertex* text_buffer = HandleTextSpriteBuffer(configuration);
					size_t* text_count = HandleTextSpriteCount(configuration);

					// Only add the actions if it is visible in the y dimension
					if (text_count_before != *text_count) {
						size_t name_length = input->name.text_vertices.size;
						Stream<UISpriteVertex> stream;

						float2 text_span = { 0.0f, 0.0f };
						float2 text_position = { 0.0f, 0.0f };
						if (IsElementNameAfter(configuration, UI_CONFIG_TEXT_INPUT_NO_NAME)) {
							name_length = function::ClampMax(name_length, *text_count);
							stream = Stream<UISpriteVertex>(text_buffer + *text_count - name_length, name_length);
							text_span = GetTextSpan(stream);
							text_position = { stream[0].position.x, -stream[0].position.y };
						}
						else if (IsElementNameFirst(configuration, UI_CONFIG_TEXT_INPUT_NO_NAME)) {
							stream = Stream<UISpriteVertex>(text_buffer + text_count_before, name_length);
							text_span = GetTextSpan(stream);
							text_position = { stream[0].position.x, -stream[0].position.y };
						}
						else {
							stream = Stream<UISpriteVertex>(nullptr, 0);
						}

						// If the scale on the x axis is smaller than a threshold, increase it
						if (text_span.x < DRAG_X_THRESHOLD) {
							float adjust_position = DRAG_X_THRESHOLD - text_span.x;
							text_span.x = DRAG_X_THRESHOLD;
							text_position.x -= adjust_position * 0.5f;
						}

						if (configuration & UI_CONFIG_TEXT_INPUT_NO_NAME) {
							text_position = position;
							text_span = scale;
							hoverable_action = wrapper_hoverable_action;
						}

						// Type pun the types - all have UITextTooltipHoverableData as first field
						// and second field a pointer to the input callback data
						UIDrawerFloatInputHoverableData hoverable_data;

						hoverable_data.tool_tip.characters = tool_tip_characters;
						hoverable_data.tool_tip.base.offset.y = 0.007f;
						hoverable_data.tool_tip.base.offset_scale.y = true;
						hoverable_data.tool_tip.base.next_row_offset = 0.006f;
						uintptr_t tool_tip_reinterpretation = (uintptr_t)&hoverable_data;
						tool_tip_reinterpretation += sizeof(UITextTooltipHoverableData);
						void** temp_reinterpretation = (void**)tool_tip_reinterpretation;
						*temp_reinterpretation = input->callback_data;

						// We need to intercept the name draw, if any
						if (configuration & UI_CONFIG_ELEMENT_NAME_ACTION) {
							const UIConfigElementNameAction* name_action = (const UIConfigElementNameAction*)config.GetParameter(UI_CONFIG_ELEMENT_NAME_ACTION);

							if (name_action->hoverable_handler.action != nullptr) {
								size_t hoverable_data_storage[256];
								ActionWrapperWithCallbackData* wrapper_data = (ActionWrapperWithCallbackData*)hoverable_data_storage;
								wrapper_data->base_action_data_size = sizeof(hoverable_data);
								void* base_data = wrapper_data->GetBaseData();
								memcpy(base_data, &hoverable_data, sizeof(hoverable_data));

								unsigned int write_size = wrapper_data->WriteCallback(name_action->hoverable_handler);
								AddHoverable(configuration, text_position, text_span, { ActionWrapperWithCallback, hoverable_data_storage, write_size, ECS_UI_DRAW_SYSTEM });
							}
							else {
								AddHoverable(configuration, text_position, text_span, { hoverable_action, &hoverable_data, sizeof(hoverable_data), ECS_UI_DRAW_SYSTEM });
							}

							if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_DRAG_VALUE) {
								UIDrawerNumberInputCallbackData* base_data = (UIDrawerNumberInputCallbackData*)draggable_data;
								base_data->input = input;
								if (name_action->clickable_handler.action != nullptr) {
									size_t clickable_data_storage[256];
									ActionWrapperWithCallbackData* wrapper_data = (ActionWrapperWithCallbackData*)clickable_data_storage;
									wrapper_data->base_action_data_size = draggable_data_size;
									wrapper_data->base_action = draggable_action;
									if (draggable_data_size == 0) {
										wrapper_data->base_action_data = draggable_data;
									}
									else {
										void* base_data = wrapper_data->GetBaseData();
										memcpy(base_data, draggable_data, draggable_data_size);
									}
									unsigned int write_size = wrapper_data->WriteCallback(name_action->clickable_handler);
									AddClickable(configuration, text_position, text_span, { ActionWrapperWithCallback, wrapper_data, write_size, name_action->clickable_handler.phase });
								}
								else {
									AddClickable(configuration, text_position, text_span, { draggable_action, draggable_data, draggable_data_size });
								}
							}
						}
						else {
							AddHoverable(configuration, text_position, text_span, { hoverable_action, &hoverable_data, sizeof(hoverable_data), ECS_UI_DRAW_SYSTEM });

							if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_DRAG_VALUE) {
								UIDrawerNumberInputCallbackData* base_data = (UIDrawerNumberInputCallbackData*)draggable_data;
								base_data->input = input;
								AddClickable(configuration, text_position, text_span, { draggable_action, draggable_data, draggable_data_size });
							}
						}
					}
				}
				else {
					float2 scale_to_finalize = { scale.x + layout.element_indentation + input->name.scale.x, scale.y };
					if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
						// For window dependent size the x scale is the same - should not add the indentation and the name scale
						const UIConfigWindowDependentSize* size = (const UIConfigWindowDependentSize*)config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);

						if (size->type == ECS_UI_WINDOW_DEPENDENT_HORIZONTAL || size->type == ECS_UI_WINDOW_DEPENDENT_BOTH) {
							scale_to_finalize.x = scale.x;
						}
					}

					FinalizeRectangle(configuration, position, scale_to_finalize);
				}
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextInput* FloatInputInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				float* number,
				float default_value,
				float min,
				float max,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextInput* DoubleInputInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				double* number,
				double default_value,
				double min,
				double max,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API UIDrawerTextInput* IntInputInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				Integer* number,
				Integer default_value,
				Integer min,
				Integer max,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatInputDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, float* number, float min, float max, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleInputDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, double* number, double min, double max, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntInputDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, Integer* number, Integer min, Integer max, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatInputGroupDrawer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> group_name,
				size_t count,
				Stream<char>* names,
				float** values,
				const float* lower_bounds,
				const float* upper_bounds,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatInputGroupInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> group_name,
				size_t count,
				Stream<char>* names,
				float** values,
				const float* default_values,
				const float* lower_bounds,
				const float* upper_bounds,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleInputGroupDrawer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> group_name,
				size_t count,
				Stream<char>* names,
				double** values,
				const double* lower_bounds,
				const double* upper_bounds,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleInputGroupInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> group_name,
				size_t count,
				Stream<char>* names,
				double** values,
				const double* default_values,
				const double* lower_bounds,
				const double* upper_bounds,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntInputGroupDrawer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> group_name,
				size_t count,
				Stream<char>* names,
				Integer** values,
				const Integer* lower_bounds,
				const Integer* upper_bounds,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntInputGroupInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> group_name,
				size_t count,
				Stream<char>* names,
				Integer** values,
				const Integer* default_values,
				const Integer* lower_bounds,
				const Integer* upper_bounds,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------
			
			void HierarchyDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerHierarchy* data, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HistogramDrawer(size_t configuration, const UIDrawConfig& config, const Stream<float> samples, Stream<char> name, float2 position, float2 scale, unsigned int precision = 2);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerList* ListInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ListDrawer(size_t configuration, UIDrawConfig& config, UIDrawerList* data, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void MenuDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerMenu* data, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// If the copy_states is set
			size_t MenuCalculateStateMemory(const UIDrawerMenuState* state, bool copy_states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// If the copy_states is set
			size_t MenuWalkStatesMemory(const UIDrawerMenuState* state, bool copy_states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void MenuSetStateBuffers(
				UIDrawerMenuState* state,
				uintptr_t& buffer,
				CapacityStream<UIDrawerMenuWindow>* stream,
				unsigned int submenu_index,
				bool copy_states
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void MenuWalkSetStateBuffers(
				UIDrawerMenuState* state,
				uintptr_t& buffer,
				CapacityStream<UIDrawerMenuWindow>* stream,
				unsigned int submenu_index,
				bool copy_states
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerMenu* MenuInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, float2 scale, UIDrawerMenuState* menu_state);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void MultiGraphDrawer(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				Stream<float> samples,
				size_t sample_count,
				const Color* colors,
				float2 position,
				float2 scale,
				size_t x_axis_precision,
				size_t y_axis_precision
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void* SentenceInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> text, char separator_token = ' ');

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t SentenceWhitespaceCharactersCount(Stream<char> identifier, CapacityStream<unsigned int> stack_buffer, char separator_token = ' ');

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SentenceNonCachedInitializerKernel(Stream<char> identifier, UIDrawerSentenceNotCached* data, char separator_token = ' ');

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SentenceDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> identifier, void* resource, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextTable* TextTableInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, unsigned int rows, unsigned int columns, Stream<char>* labels);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextTableDrawer(
				size_t configuration,
				const UIDrawConfig& config,
				float2 position,
				float2 scale,
				UIDrawerTextTable* data,
				unsigned int rows,
				unsigned int columns
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextTableDrawer(
				size_t configuration,
				const UIDrawConfig& config,
				float2 position,
				float2 scale,
				unsigned int rows,
				unsigned int columns,
				Stream<char>* labels
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float GraphYAxis(
				size_t configuration,
				const UIDrawConfig& config,
				size_t y_axis_precision,
				float min_y,
				float max_y,
				float2 position,
				float2 scale,
				Color font_color,
				float2 font_size,
				float character_spacing
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// it returns the left and the right text spans in order to adjust the graph scale and position
			float2 GraphXAxis(
				size_t configuration,
				const UIDrawConfig& config,
				size_t x_axis_precision,
				float min_x,
				float max_x,
				float2 position,
				float2 scale,
				Color font_color,
				float2 font_size,
				float character_spacing,
				bool disable_horizontal_line = false
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void GraphDrawer(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				Stream<float2> samples,
				float2 position,
				float2 scale,
				size_t x_axis_precision,
				size_t y_axis_precision
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// it returns the index of the sprite with the smallest x
			template<bool left = true>
			ECSENGINE_API unsigned int LevelVerticalTextX(Stream<UISpriteVertex> vertices) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FixedScaleTextLabel(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// this will add automatically VerticalSlider and HorizontalSlider to name
			void ViewportRegionSliderInitializer(
				float2* region_offset,
				Stream<char> name,
				void** horizontal_slider,
				void** vertical_slider
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ViewportRegionSliders(
				float2 render_span,
				float2 render_zone,
				float2* region_offset,
				void* horizontal_slider,
				void* vertical_slider
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ViewportRegionSliders(
				Stream<char> horizontal_slider_name,
				Stream<char> vertical_slider_name,
				float2 render_span,
				float2 render_zone,
				float2* region_offset
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SpriteRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				Stream<wchar_t> texture,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SpriteRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				ResourceView view,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			void SpriteRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				ResourceView view,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				Color top_left,
				Color top_right,
				Color bottom_left,
				Color bottom_right
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				ColorFloat top_left,
				ColorFloat top_right,
				ColorFloat bottom_left,
				ColorFloat bottom_right
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				const Color* colors
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				const ColorFloat* colors
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				float2 position,
				float2 scale,
				Stream<wchar_t> texture,
				const Color* colors,
				const float2* uvs,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				float2 position,
				float2 scale,
				Stream<wchar_t> texture,
				const Color* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				float2 position,
				float2 scale,
				Stream<wchar_t> texture,
				const ColorFloat* colors,
				const float2* uvs,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				float2 position,
				float2 scale,
				Stream<wchar_t> texture,
				const ColorFloat* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			Stream<UIVertexColor> GetSolidColorStream(size_t configuration, size_t size);

			// ------------------------------------------------------------------------------------------------------------------------------------
			
			Stream<UISpriteVertex> GetTextStream(size_t configuration, size_t size);

			// ------------------------------------------------------------------------------------------------------------------------------------

			Stream<UISpriteVertex> GetSpriteStream(size_t configuration, size_t size);

			// ------------------------------------------------------------------------------------------------------------------------------------

			Stream<UISpriteVertex> GetSpriteClusterStream(size_t configuration, size_t size);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 TextSpan(Stream<char> characters, float2 font_size, float character_spacing) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 TextSpan(Stream<char> characters) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetLabelScale(Stream<char> characters) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetLabelScale(Stream<char> characters, float2 font_size, float character_spacing) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Uses the scale and the padding to return the size
			float2 GetLabelScale(const UIDrawerTextElement* element) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void GetTextLabelAlignment(size_t configuration, const UIDrawConfig& config, ECS_UI_ALIGN& horizontal, ECS_UI_ALIGN& vertical) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void GetElementAlignment(size_t configuration, const UIDrawConfig& config, ECS_UI_ALIGN& horizontal, ECS_UI_ALIGN& vertical) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Uses GetElementAlignment to get the element alignment and then calculates the new position based on the scale
			void GetElementAlignedPosition(size_t configuration, const UIDrawConfig& config, float2& position, float2 scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float GetXScaleUntilBorder(float position) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float GetYScaleUntilBorder(float position) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float GetNextRowXPosition() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t GetRandomIntIdentifier(size_t index) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Adds the actions that were recorded during the late binding
			void PushLateActions();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void UpdateCurrentColumnScale(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void UpdateCurrentRowScale(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void UpdateCurrentScale(float2 position, float2 value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void UpdateMaxRenderBoundsRectangle(float2 limits);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void UpdateMinRenderBoundsRectangle(float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void UpdateRenderBoundsRectangle(float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool VerifyFitSpaceNonRectangle(size_t vertex_count) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool VerifyFitSpaceRectangle(float2 position, float2 scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleDrawMode(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ECS_NOINLINE FinalizeRectangle(size_t configuration, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ElementName(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2& position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ElementName(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2& position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Takes into account the name padding
			float ElementNameSize(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2 scale);

			// Takes into account the name padding
			float ElementNameSize(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool ExistsResource(Stream<char> name);

		public:

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawer(
				UIColorThemeDescriptor& _color_theme,
				UIFontDescriptor& _font,
				UILayoutDescriptor& _layout,
				UIElementDescriptor& _element_descriptor,
				bool _initializer
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawer(
				UIDrawerDescriptor& descriptor,
				void* _window_data,
				bool _initializer
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			~UIDrawer();

			// ------------------------------------------------------------------------------------------------------------------------------------

			// the initializer must return a pointer to the data and take as a parameter the resource identifier (i.e. its name)
			template<typename Initializer>
			void AddWindowResource(Stream<char> label, Initializer&& init) {
				Stream<char> label_identifier = HandleResourceIdentifier(label, true);
				ResourceIdentifier identifier(label_identifier);
				void* data = init(label_identifier);
				AddWindowResourceToTable(data, identifier);
			}

			void AddWindowResourceToTable(void* resource, ResourceIdentifier identifier);
			
			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddHoverable(size_t configuration, float2 position, float2 scale, UIActionHandler handler);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddTextTooltipHoverable(
				size_t configuration, 
				float2 position, 
				float2 scale, 
				UITextTooltipHoverableData* data, 
				bool stable = false, 
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_SYSTEM
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddClickable(
				size_t configuration,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT
			);

			// ------------------------------------------------------------------------------------------------------------------------------------
			
			void AddGeneral(
				size_t configuration,
				float2 position,
				float2 scale,
				UIActionHandler handler
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultHoverable(
				size_t configuration,
				float2 position,
				float2 scale,
				Color color,
				float percentage = 1.25f,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultHoverable(
				size_t configuration,
				float2 main_position,
				float2 main_scale,
				const float2* positions,
				const float2* scales,
				const Color* colors,
				const float* percentages,
				unsigned int count,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultClickable(
				size_t configuration,
				float2 position,
				float2 scale,
				UIActionHandler hoverable_handler,
				UIActionHandler clickable_handler,
				ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT
			);

			// Write into a reserved spot
			void AddDefaultClickable(
				UIReservedHandler reserved_hoverable,
				UIReservedHandler reserved_clickable,
				float2 position,
				float2 scale,
				UIActionHandler hoverable_handler,
				UIActionHandler clickable_handler,
				ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDoubleClickAction(
				size_t configuration,
				float2 position,
				float2 scale,
				unsigned int identifier,
				size_t duration_between_clicks,
				UIActionHandler first_click_handler,
				UIActionHandler second_click_handler
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerMenuRightClickData PrepareRightClickActionData(Stream<char> name, UIDrawerMenuState* menu_state, UIActionHandler custom_handler = { nullptr });

			// This will always have the system phase
			void AddRightClickAction(
				size_t configuration,
				float2 position,
				float2 scale,
				Stream<char> name,
				UIDrawerMenuState* menu_state,
				UIActionHandler custom_handler = { nullptr }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultClickableHoverableWithText(
				size_t configuration,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				Color color,
				Stream<char> text,
				float2 text_offset,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_LATE,
				Color font_color = ECS_COLOR_WHITE,
				float percentage = 1.25f,
				ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Either horizontal or vertical cull should be set
			// White color by default means use the color from the color theme
			void AddDefaultClickableHoverableWithTextEx(
				size_t configuration,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				UIDefaultTextHoverableData* hoverable_data,
				Color text_color = ECS_COLOR_WHITE,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL,
				ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultClickableHoverable(
				size_t configuration,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				Color color = ECS_COLOR_WHITE,
				float percentage = 1.25f,
				ECS_UI_DRAW_PHASE hoverable_phase = ECS_UI_DRAW_NORMAL,
				ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT
			);

			// Write into a reserved spot
			void AddDefaultClickableHoverable(
				UIReservedHandler reserved_hoverable,
				UIReservedHandler reserved_clickable,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				Color color = ECS_COLOR_WHITE,
				float percentage = 1.25f,
				ECS_UI_DRAW_PHASE hoverable_phase = ECS_UI_DRAW_NORMAL,
				ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultClickableHoverable(
				size_t configuration,
				float2 main_position,
				float2 main_scale,
				const float2* positions,
				const float2* scales,
				const Color* colors,
				const float* percentages,
				unsigned int count,
				UIActionHandler handler,
				ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT
			);

			// Write into a reserved spot
			void AddDefaultClickableHoverable(
				UIReservedHandler reserved_hoverable,
				UIReservedHandler reserved_clickable,
				float2 main_position,
				float2 main_scale,
				const float2* positions,
				const float2* scales,
				const Color* colors,
				const float* percentages,
				unsigned int count,
				UIActionHandler handler,
				ECS_MOUSE_BUTTON button_type
			);
			
			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddWindowDependentSizeToConfigFromPoint(float2 initial_point, UIDrawConfig& config) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddRelativeTransformToConfigFromPoint(float2 initial_point, UIDrawConfig& config) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddAbsoluteTransformToConfigFromPoint(float2 initial_point, UIDrawConfig& config) const;

#pragma region Array

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Only draw function, no initialization; data is the per element data; additional_data can be used to access data outside the element
			typedef void (*UIDrawerArrayDrawFunction)(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData data);

			// Optional handler for when an element has been dragged to a new position
			// The new order indicates elements in the new order
			typedef void (*UIDrawerArrayDragFunction)(UIDrawer& drawer, void* elements, unsigned int element_count, unsigned int* new_order, void* additional_data);

			// Only accepts CapacityStream or ResizableStream as StreamType
			// The collapsing header will go to the next row after every element - no need to call NextRow() inside draw_function
			// When changing the values, it will simply do a memcpy between elements; the draw function must cope with that
			template<typename StreamType>
			void Array(
				size_t configuration,
				size_t element_configuration,
				const UIDrawConfig& config,
				const UIDrawConfig* element_config,
				Stream<char> name,
				StreamType* elements,
				void* additional_data,
				UIDrawerArrayDrawFunction draw_function,
				UIDrawerArrayDragFunction drag_function = nullptr
			) {
				float2 position;
				float2 scale;
				HandleTransformFlags(configuration, config, position, scale);

				ECS_TEMP_ASCII_STRING(data_name, 256);
				data_name.Copy(name);
				data_name.AddStream(" data");
				data_name = HandleResourceIdentifier(data_name);

				if (!initializer) {
					if (configuration & UI_CONFIG_DO_CACHE) {		
						UIDrawerArrayData* data = (UIDrawerArrayData*)FindWindowResource(data_name);

						ArrayDrawer(
							configuration | UI_CONFIG_DO_NOT_VALIDATE_POSITION,
							element_configuration,
							config,
							element_config,
							data,
							name,
							elements,
							additional_data,
							draw_function,
							drag_function,
							position,
							scale
						);
						// Manual increment
						if (configuration & UI_CONFIG_DYNAMIC_RESOURCE) {
							// The increment must be done on the identifier of the name - not on the identifier of the data
							system->IncrementWindowDynamicResource(window_index, HandleResourceIdentifier(name));
						}
					}
					else {
						// Must check for the name manually
						bool exists = system->ExistsWindowResource(window_index, data_name);
						if (!exists) {
							UIDrawerInitializeArrayElementData initialize_data;
							initialize_data.config = (UIDrawConfig*)&config;
							initialize_data.name = name;
							initialize_data.element_count = elements->size;
							InitializeDrawerElement(*this, &initialize_data, name, InitializeArrayElement, DynamicConfiguration(configuration));
						}
						Array(DynamicConfiguration(configuration), element_configuration, config, element_config, name, elements, additional_data, draw_function, drag_function);
					}
				}
				else {
					if (configuration & UI_CONFIG_DO_CACHE) {
						ArrayInitializer(configuration, config, name, elements->size);
					}
				}
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Only accepts CapacityStream or ResizableStream as StreamType parameters
			template<typename StreamType>
			void ArrayDrawer(
				size_t configuration,
				size_t element_configuration,
				const UIDrawConfig& config,
				const UIDrawConfig* element_config,
				UIDrawerArrayData* data, 
				Stream<char> name, 
				StreamType* elements,
				void* additional_data,
				UIDrawerArrayDrawFunction draw_function,
				UIDrawerArrayDragFunction drag_function,
				float2 position,
				float2 scale
			) {
				constexpr float SIZE_INPUT_X_SCALE = 0.06f;
				constexpr float2 ARROW_SIZE = { 0.02f, 0.02f };

				// The collapsing header will control the active state
				float collapsing_header_size = GetXScaleUntilBorder(position.x);
				UIDrawConfig header_config;

				// Copy all the other configurations except the transform ones
				for (size_t index = 0; index < config.flag_count; index++) {
					if (config.associated_bits[index] != UI_CONFIG_ABSOLUTE_TRANSFORM && config.associated_bits[index] != UI_CONFIG_RELATIVE_TRANSFORM
						&& config.associated_bits[index] != UI_CONFIG_WINDOW_DEPENDENT_SIZE && config.associated_bits[index] != UI_CONFIG_MAKE_SQUARE) {
						size_t header_config_count = header_config.flag_count;
						size_t config_float_size = config.parameter_start[index + 1] - config.parameter_start[index];
						size_t config_byte_size = config_float_size * sizeof(float);
						header_config.associated_bits[header_config_count] = config.associated_bits[index];
						header_config.parameter_start[header_config_count + 1] = header_config.parameter_start[header_config_count]
							+ config_float_size;
						memcpy(
							header_config.parameters + header_config.parameter_start[header_config_count],
							config.parameters + config.parameter_start[index],
							config_byte_size
						);
						header_config.flag_count++;
					}
				}

				// Add the transform
				UIConfigAbsoluteTransform transform;
				transform.position = { position.x + region_render_offset.x, position.y + region_render_offset.y };
				if (~configuration & UI_CONFIG_ARRAY_DISABLE_SIZE_INPUT) {
					float label_size = GetLabelScale(name).x;
					transform.scale.x = collapsing_header_size - SIZE_INPUT_X_SCALE * zoom_ptr->x - layout.element_indentation;
				}
				else {
					transform.scale.x = collapsing_header_size;
				}
				transform.scale.y = scale.y;
				header_config.AddFlag(transform);

				float2 header_position;
				float2 header_scale;
				UIConfigGetTransform get_header_transform;
				get_header_transform.position = &header_position;
				get_header_transform.scale = &header_scale;
				header_config.AddFlag(get_header_transform);

				size_t NEW_CONFIGURATION = UI_CONFIG_ABSOLUTE_TRANSFORM | function::ClearFlag(configuration, UI_CONFIG_RELATIVE_TRANSFORM,
					UI_CONFIG_WINDOW_DEPENDENT_SIZE, UI_CONFIG_MAKE_SQUARE, UI_CONFIG_DO_CACHE);

				// Draw the collapsing header - nullified configurations - relative transform, window dependent size, make square
				CollapsingHeader(NEW_CONFIGURATION | UI_CONFIG_COLLAPSING_HEADER_DO_NOT_INFER | UI_CONFIG_GET_TRANSFORM,
					header_config, 
					name, 
					&data->collapsing_header_state, 
				[&]() {
					// If it has any pre draw, make sure to draw it
					if (configuration & UI_CONFIG_ARRAY_PRE_POST_DRAW) {
						const UIConfigArrayPrePostDraw* custom_draw = (const UIConfigArrayPrePostDraw*)config.GetParameter(UI_CONFIG_ARRAY_PRE_POST_DRAW);
						if (custom_draw->pre_function != nullptr) {
							custom_draw->pre_function(this, custom_draw->pre_data);
						}
					}

					ECS_TEMP_ASCII_STRING(temp_name, 64);
					temp_name.Copy("Element ");
					size_t base_name_size = temp_name.size;

					// Set the draw mode to indent if it is different
					ECS_UI_DRAWER_MODE old_draw_mode = draw_mode;
					unsigned short old_draw_count = draw_mode_count;
					unsigned short old_draw_target = draw_mode_target;
					SetDrawMode(ECS_UI_DRAWER_INDENT);

					// Push onto the identifier stack the field name
					bool has_pushed_stack = PushIdentifierStackStringPattern();
					PushIdentifierStack(name);

					UIDrawerArrayDragData drag_data;
					drag_data.array_data = data;

					UIDrawConfig arrow_config;
					UIConfigAbsoluteTransform arrow_transform;
					arrow_transform.scale = ARROW_SIZE;

					UIDrawConfig internal_element_config;
					memcpy(&internal_element_config, element_config, sizeof(internal_element_config));

					struct SelectElementData {
						UIDrawerArrayData* array_data;
						unsigned int index;
					};

					auto select_element = [](ActionData* action_data) {
						UI_UNPACK_ACTION_DATA;

						if (IsClickableTrigger(action_data)) {
							SelectElementData* data = (SelectElementData*)_data;
							data->array_data->remove_anywhere_index = data->index;
						}
					};

					SelectElementData select_element_data;

					if (configuration & UI_CONFIG_ARRAY_REMOVE_ANYWHERE) {
						element_configuration |= UI_CONFIG_ELEMENT_NAME_ACTION;
						select_element_data.array_data = data;

						UIConfigElementNameAction name_action;
						name_action.clickable_handler = { select_element, &select_element_data, sizeof(select_element_data) };
						internal_element_config.AddFlag(name_action);
					}

					const char* draw_element_name = temp_name.buffer;
					bool has_drag = !function::HasFlag(configuration, UI_CONFIG_ARRAY_DISABLE_DRAG);
					bool multi_line_element = function::HasFlag(configuration, UI_CONFIG_ARRAY_MULTI_LINE);

					auto draw_drag_arrow = [&](unsigned int index, float row_y_scale, auto has_action) {
						arrow_transform.position.y = AlignMiddle(current_y, row_y_scale, ARROW_SIZE.y);
						arrow_config.AddFlag(arrow_transform);
						SpriteRectangle(
							UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_FIT_SPACE,
							arrow_config,
							ECS_TOOLS_UI_TEXTURE_ARROW,
							color_theme.text
						);
						arrow_config.flag_count--;

						if constexpr (has_action) {
							drag_data.index = index;
							drag_data.row_y_scale = row_y_scale;
							float2 action_position = arrow_transform.position - region_render_offset;
							float2 action_scale;
							action_position = ExpandRectangle(action_position, arrow_transform.scale, { 1.25f, 1.25f }, action_scale);
							AddClickable(element_configuration, action_position, action_scale, { ArrayDragAction, &drag_data, sizeof(drag_data) });
							AddDefaultHoverable(element_configuration, action_position, action_scale, color_theme.theme);
						}
					};

					// Has auto is a true/false flag that tells whether or not to add an action
					auto draw_row = [&](unsigned int index, auto has_action) {
						if (has_drag) {
							arrow_transform.position.x = current_x;
							// Indent the arrow position in order to align to row Y
							Indent();
							OffsetX(ARROW_SIZE.x);
						}

						PushIdentifierStackRandom(index);
						function::ConvertIntToChars(temp_name, index);

						float previous_current_y = current_y;

						if (multi_line_element) {
							TextLabel(UI_CONFIG_LABEL_TRANSPARENT, config, temp_name);
							draw_drag_arrow(index, current_row_y_scale, has_action);
							NextRow();
							CrossLine();
						}

						bool has_name_action = function::HasFlag(element_configuration, UI_CONFIG_ELEMENT_NAME_ACTION);
						if (has_name_action) {
							// Check the selected item
							if (index == data->remove_anywhere_index) {
								element_configuration |= UI_CONFIG_ELEMENT_NAME_BACKGROUND;
								UIConfigElementNameBackground background;
								background.color = color_theme.theme;
								internal_element_config.AddFlag(background);
							}
						}

						UIDrawerArrayDrawData function_data;
						function_data.additional_data = additional_data;
						function_data.current_index = index;
						// MemoryOf(1) gives us the byte size of an element
						function_data.element_data = function::OffsetPointer(elements->buffer, index * elements->MemoryOf(1));
						function_data.configuration = element_configuration;
						function_data.config = &internal_element_config;

						select_element_data.index = index;

						draw_function(*this, draw_element_name, function_data);

						if (has_name_action) {
							if (function::HasFlag(element_configuration, UI_CONFIG_ELEMENT_NAME_BACKGROUND)) {
								internal_element_config.flag_count--;
								element_configuration = function::ClearFlag(element_configuration, UI_CONFIG_ELEMENT_NAME_BACKGROUND);
							}
						}

						// Get the row y scale
						float row_y_scale = current_y - previous_current_y + current_row_y_scale;
						data->row_y_scale = row_y_scale;
						if (!multi_line_element) {
							if (has_drag) {
								draw_drag_arrow(index, row_y_scale, has_action);
							}
						}
						
						NextRow();
						temp_name.size = base_name_size;
						PopIdentifierStack();
					};

					// If there is no drag, proceed normally
					unsigned int skip_count = 0;
					unsigned int index = 0;

					// Skip all the elements that are outside the view region - first the elements that come before
					if (data->row_y_scale > 0.0f) {
						float skip_length = region_fit_space_vertical_offset - current_y + region_render_offset.y;
						if (skip_length > 0.0f) {
							skip_count = (unsigned int)(skip_length / (data->row_y_scale + layout.next_row_y_offset));
							if (skip_count > 0) {
								skip_count = function::ClampMax(skip_count, elements->size);
								float2 update_position = GetCurrentPosition();
								float2 update_scale = { 0.0f, skip_count * (data->row_y_scale + layout.next_row_y_offset) - layout.next_row_y_offset };
								UpdateRenderBoundsRectangle(update_position, update_scale);
								UpdateCurrentScale(update_position, update_scale);
								NextRow();
							}
							index = skip_count;
						}
					}

					auto draw_when_drag = [&](auto is_released) {
						// Make a buffer that will point for each element which is the new element that
						// will replace it
						unsigned int* _order_allocation = nullptr;
						Stream<unsigned int> order_allocation = { nullptr, 0 };
						unsigned int skip_count_start = skip_count + (data->drag_index < skip_count);
						
						if constexpr (is_released) {
							_order_allocation = (unsigned int*)GetTempBuffer(sizeof(unsigned int) * elements->size);
							order_allocation = { _order_allocation, 0 };

							for (unsigned int counter = 0; counter < skip_count_start; counter++) {
								counter += data->drag_index == counter;
								if (counter < skip_count_start) {
									order_allocation.Add(counter);
								}
							}
						}

						index = skip_count_start;
						position.y = current_y - region_render_offset.y;
						unsigned int drawn_elements = 0;
						while (position.y + data->row_y_scale < data->drag_current_position && drawn_elements < elements->size - 1 - skip_count) {
							index += data->drag_index == index;
							if constexpr (is_released) {
								order_allocation.Add(index);
							}
							draw_row(index, std::false_type{});
							drawn_elements++;
							index++;
							position.y = current_y - region_render_offset.y;
						}

						if constexpr (is_released) {
							// Reached the dragged element
							order_allocation.Add(data->drag_index);
						}

						// Draw a small highlight
						float x_highlight_scale = GetXScaleUntilBorder(position.x - region_render_offset.x);
						float y_hightlight_scale = data->row_y_scale * 1.25f;
						float y_highlight_position = AlignMiddle(current_y, data->row_y_scale, y_hightlight_scale);
						Color highlight_color = color_theme.theme;
						highlight_color.alpha = 100;
						// Offset the x a little to the left
						SpriteRectangle(
							0,
							{ current_x - 0.01f - region_render_offset.x, y_highlight_position - region_render_offset.y },
							{ x_highlight_scale, y_hightlight_scale },
							ECS_TOOLS_UI_TEXTURE_MASK,
							highlight_color
						);
						draw_row(data->drag_index, std::false_type{});

						unsigned int last_element = data->drag_index == elements->size - 1 ? elements->size - 1 : elements->size;
						for (; index < last_element && current_y - region_render_offset.y <= region_limit.y; index++) {
							// Skip the current drag index if it is before drawn
							index += data->drag_index == index;
							if constexpr (is_released) {
								order_allocation.Add(index);
							}
							draw_row(index, std::false_type{});
						}
						index += data->drag_index == index;

						if constexpr (is_released) {
							for (unsigned int counter = index; counter < last_element; counter++) {
								// Skip the current drag index if it is before drawn
								counter += data->drag_index == counter;
								order_allocation.Add(counter);
							}
							ECS_ASSERT(order_allocation.size == elements->size);

							// The default drag behaviour is to simply memcpy the new data
							if (drag_function == nullptr) {
								size_t temp_marker = GetTempAllocatorMarker();
								// Make a temporary allocation that will hold the new elements
								size_t element_byte_size = elements->MemoryOf(1);

								void* temp_allocation = GetTempBuffer(element_byte_size * elements->size);
								for (size_t index = 0; index < elements->size; index++) {
									memcpy(function::OffsetPointer(temp_allocation, index * element_byte_size), &elements->buffer[order_allocation[index]], element_byte_size);
									//temp_allocation[index] = elements->buffer[order_allocation[index]];
								}

								memcpy(elements->buffer, temp_allocation, element_byte_size * elements->size);
								ReturnTempAllocator(temp_marker);

							}
							else {
								drag_function(*this, elements->buffer, elements->size, order_allocation.buffer, additional_data);
							}

							data->drag_is_released = false;
							data->drag_index = -1;
						}
					};

					if (data->drag_index == -1) {
						for (; index < elements->size && current_y - region_render_offset.y - data->row_y_scale <= region_limit.y; index++) {
							draw_row(index, std::true_type{});
						}
					}
					// There is currently a drag active - no other drag actions should be added
					// And the draw should be effective 
					else {
						if (data->drag_is_released) {
							draw_when_drag(std::true_type{});
						}
						else {
							draw_when_drag(std::false_type{});
						}
					}

					if (data->row_y_scale > 0.0f) {
						// Skip all the elements that are the outside the view region - the elements that come after
						skip_count = elements->size - index;
						if (skip_count > 0) {
							float2 update_position = GetCurrentPosition();
							float2 update_scale = { 0.0f, skip_count * (data->row_y_scale + layout.next_row_y_offset) - layout.next_row_y_offset };
							UpdateRenderBoundsRectangle(update_position, update_scale);
							UpdateCurrentScale(update_position, update_scale);
							NextRow();
						}
					}

					// The collapsing header had an absolute transform added, so remove it
					header_config.flag_count -= 2;

					float x_scale_till_border = GetXScaleUntilBorder(current_x) - layout.element_indentation;
					UIConfigWindowDependentSize button_size;
					button_size.type = ECS_UI_WINDOW_DEPENDENT_HORIZONTAL;
					button_size.scale_factor = GetWindowSizeFactors(button_size.type, {x_scale_till_border * 0.5f, layout.default_element_y});
					header_config.AddFlag(button_size);

					float2 button_position;
					float2 button_scale;
					UIConfigGetTransform get_transform;
					get_transform.position = &button_position;
					get_transform.scale = &button_scale;
					header_config.AddFlag(get_transform);

					size_t COLOR_RECTANGLE_CONFIGURATION = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_GET_TRANSFORM;

					SolidColorRectangle(COLOR_RECTANGLE_CONFIGURATION, header_config, color_theme.theme);

					size_t BUTTON_CONFIGURATION = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE;

					float2 square_scale = GetSquareScale(layout.default_element_y);
					UIConfigAbsoluteTransform sprite_button_transform;
					sprite_button_transform.position = { AlignMiddle(button_position.x, button_scale.x, square_scale.x), AlignMiddle(button_position.y, button_scale.y, square_scale.y) };
					sprite_button_transform.scale = square_scale;
					sprite_button_transform.position += region_render_offset;
					header_config.AddFlag(sprite_button_transform);

					Color sprite_color = color_theme.text;
					
					UIDrawerArrayAddRemoveData add_remove_data;
					add_remove_data.array_data = data;
					add_remove_data.new_size = elements->size + 1;
					add_remove_data.element_byte_size = elements->MemoryOf(1);
					bool is_capacity_stream = sizeof(*elements) == sizeof(CapacityStream<void>);
					if (is_capacity_stream) {
						add_remove_data.is_resizable_data = false;
						add_remove_data.capacity_data = (CapacityStream<void>*)elements;
					}
					else {
						add_remove_data.is_resizable_data = true;
						add_remove_data.resizable_data = (ResizableStream<void>*)elements;
					}

					// Update the user callback data if needed
					if (configuration & UI_CONFIG_ARRAY_ADD_CALLBACK) {
						const UIConfigArrayAddCallback* callback = (const UIConfigArrayAddCallback*)config.GetParameter(UI_CONFIG_ARRAY_ADD_CALLBACK);
						if (callback->handler.data_size > 0 && !callback->copy_on_initialization) {
							memcpy(data->add_callback_data, callback->handler.data, callback->handler.data_size);
						}
					}

					if (is_capacity_stream) {
						if (elements->size < elements->capacity) {
							AddDefaultClickableHoverable(
								configuration,
								button_position,
								button_scale,
								{ UIDrawerArrayAddAction, &add_remove_data, sizeof(add_remove_data), data->add_callback_phase },
								color_theme.theme
							);
						}
						else {
							sprite_color.alpha *= color_theme.alpha_inactive_item;
						}
					}
					else {
						AddDefaultClickableHoverable(
							configuration,
							button_position,
							button_scale,
							{ UIDrawerArrayAddAction, &add_remove_data, sizeof(add_remove_data), data->add_callback_phase },
							color_theme.theme
						);
					}

					// Add and remove buttons
					SpriteRectangle(
						BUTTON_CONFIGURATION,
						header_config, 
						ECS_TOOLS_UI_TEXTURE_PLUS,
						sprite_color
					);

					// One for the absolute transform
					header_config.flag_count--;
					
					SolidColorRectangle(COLOR_RECTANGLE_CONFIGURATION, header_config, color_theme.theme);
					
					// Copy the remove callback data if needed
					if (configuration & UI_CONFIG_ARRAY_REMOVE_CALLBACK) {
						const UIConfigArrayRemoveCallback* callback = (const UIConfigArrayRemoveCallback*)config.GetParameter(UI_CONFIG_ARRAY_REMOVE_CALLBACK);
						if (callback->handler.data_size > 0 && !callback->copy_on_initialization) {
							memcpy(data->remove_callback_data, callback->handler.data, callback->handler.data_size);
						}
					}

					add_remove_data.new_size = elements->size - 1;
					if (elements->size > 0) {
						AddDefaultClickableHoverable(
							configuration,
							button_position,
							button_scale,
							{ UIDrawerArrayRemoveAction, &add_remove_data, sizeof(add_remove_data), data->remove_callback_phase },
							color_theme.theme
						);
						sprite_color.alpha = 255;
					}
					else {
						sprite_color.alpha = color_theme.text.alpha * color_theme.alpha_inactive_item;
					}

					sprite_button_transform.position = { AlignMiddle(button_position.x, button_scale.x, square_scale.x), AlignMiddle(button_position.y, button_scale.y, square_scale.y) };
					sprite_button_transform.position += region_render_offset;
					header_config.AddFlag(sprite_button_transform);

					SpriteRectangle(
						BUTTON_CONFIGURATION,
						header_config,
						ECS_TOOLS_UI_TEXTURE_MINUS,
						sprite_color
					);

					if (configuration & UI_CONFIG_ARRAY_REMOVE_ANYWHERE) {
						if (system->m_keyboard->IsPressed(ECS_KEY_DELETE) && data->remove_anywhere_index != -1) {
							ActionData action_data = GetDummyActionData();
							action_data.data = &add_remove_data;
							UIDrawerArrayRemoveAnywhereAction(&action_data);
						}
					}

					// One for the window dependent size and one for the absolute transform
					header_config.flag_count -= 2;
					NextRow();

					draw_mode = old_draw_mode;
					draw_mode_count = old_draw_count + 1;
					draw_mode_target = old_draw_target;

					// Pop the field name identifier
					if (has_pushed_stack) {
						PopIdentifierStack();
					}
					PopIdentifierStack();

					// If it has a post draw, make sure to draw it
					if (configuration & UI_CONFIG_ARRAY_PRE_POST_DRAW) {
						const UIConfigArrayPrePostDraw* custom_draw = (const UIConfigArrayPrePostDraw*)config.GetParameter(UI_CONFIG_ARRAY_PRE_POST_DRAW);
						if (custom_draw->post_function != nullptr) {
							custom_draw->post_function(this, custom_draw->post_data);
						}
					}
				}
				);

				if (~configuration & UI_CONFIG_ARRAY_DISABLE_SIZE_INPUT) {
					ECS_TEMP_ASCII_STRING(temp_input_name, 256);
					temp_input_name.Copy(name);
					temp_input_name.AddStream("Size input");
					temp_input_name.AddAssert('\0');

					// Eliminate the get transform
					header_config.flag_count--;
					// Draw the size with the capacity set
					if (header_config.associated_bits[header_config.flag_count - 1] == UI_CONFIG_ABSOLUTE_TRANSFORM) {
						header_config.flag_count--;
					}
					transform.position.x = header_position.x + header_scale.x + layout.element_indentation + region_render_offset.x;
					transform.scale.x = SIZE_INPUT_X_SCALE * zoom_ptr->x;
					header_config.AddFlag(transform);

					if (~configuration & UI_CONFIG_ARRAY_FIXED_SIZE) {
						bool is_capacity_stream = sizeof(*elements) == sizeof(CapacityStream<void>);
						unsigned int max_capacity = is_capacity_stream ? elements->capacity : ECS_MB_10;

						UIDrawerArrayAddRemoveData callback_data;
						callback_data.array_data = data;
						callback_data.new_size = elements->size;
						callback_data.is_resizable_data = !is_capacity_stream;
						if (callback_data.is_resizable_data) {
							callback_data.element_byte_size = elements->MemoryOf(1);
							callback_data.resizable_data = (ResizableStream<void>*)elements;
						}
						else {
							callback_data.capacity_data = (CapacityStream<void>*)elements;
						}

						UIConfigTextInputCallback input_callback;
						// Choose the highest draw phase
						ECS_UI_DRAW_PHASE callback_phase = (ECS_UI_DRAW_PHASE)((unsigned int)data->add_callback_phase < (unsigned int)data->remove_callback_phase ?
							(unsigned int)data->remove_callback_phase : (unsigned int)data->add_callback_phase);
						input_callback.handler = { UIDrawerArrayIntInputAction, &callback_data, sizeof(callback_data), callback_phase };
						header_config.AddFlag(input_callback);

						IntInput(
							NEW_CONFIGURATION | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_NUMBER_INPUT_NO_DRAG_VALUE | UI_CONFIG_TEXT_INPUT_CALLBACK,
							header_config,
							temp_input_name,
							&elements->size,
							(unsigned int)0,
							(unsigned int)0,
							max_capacity
						);
					}
					else {
						UIConfigActiveState active_state;
						active_state.state = false;
						header_config.AddFlag(active_state);
						IntInput(
							NEW_CONFIGURATION | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_ACTIVE_STATE,
							header_config,
							temp_input_name,
							&elements->size,
							(unsigned int)0,
							(unsigned int)0,
							elements->capacity
						);
					}
				}

				NextRow();

			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerArrayData* ArrayInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, size_t element_count);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/values: CapacityStream<float> or ResizableStream<float>
			template<typename StreamType>
			void ArrayFloat(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<float>, StreamType> || std::is_same_v<ResizableStream<float>, StreamType>,
					"Incorrect stream type for UIDrawer array check box.");
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayFloatFunction);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/values: CapacityStream<double> or ResizableStream<double>
			template<typename StreamType>
			void ArrayDouble(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<double>, StreamType> || std::is_same_v<ResizableStream<double>, StreamType>,
					"Incorrect stream type for UIDrawer array check box.");
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayDoubleFunction);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/values: CapacityStream<Integer> or ResizableStream<Integer>
			template<typename Integer, typename StreamType>
			void ArrayInteger(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<Integer>, StreamType> || std::is_same_v<ResizableStream<Integer>, StreamType>,
					"Incorrect stream type for UIDrawer array integer.");
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayIntegerFunction<Integer>);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Instantiations - 2 components

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/values: CapacityStream<float2> or ResizableStream<float2>
			template<typename StreamType>
			void ArrayFloat2(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<float2>, StreamType> || std::is_same_v<ResizableStream<float2>, StreamType>,
					"Incorrect stream type for UIDrawer array float2.");
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayFloat2Function);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/values: CapacityStream<double2> or ResizableStream<double2>
			template<typename StreamType>
			void ArrayDouble2(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<double2>, StreamType> || std::is_same_v<ResizableStream<double2>, StreamType>,
					"Incorrect stream type for UIDrawer array double2.");
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayDouble2Function);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/values: CapacityStream<Integer2> or ResizableStream<Integer2>
			template<typename BaseInteger, typename StreamType>
			void ArrayInteger2(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayInteger2Function<BaseInteger>);
			}

#define UI_DRAWER_ARRAY_INTEGER_INFER(count) if constexpr (std::is_same_v<StreamType, CapacityStream<void>>) { \
				if constexpr (std::is_same_v<BaseInteger, char>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (CapacityStream<char##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, int8_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (CapacityStream<char##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, uint8_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (CapacityStream<uchar##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, int16_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (CapacityStream<short##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, uint16_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (CapacityStream<ushort##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, int32_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (CapacityStream<int##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, uint32_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (CapacityStream<uint##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, int64_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (CapacityStream<long##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, uint64_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (CapacityStream<ulong##count>*)values, element_configuration, element_config); \
				} \
				else { \
					static_assert(false, "Incorrect base integer argument"); \
				} \
			} \
			else if constexpr (std::is_same_v<StreamType, ResizableStream<void>>) { \
				if constexpr (std::is_same_v<BaseInteger, char>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (ResizableStream<char##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, int8_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (ResizableStream<char##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, uint8_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (ResizableStream<uchar##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, int16_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (ResizableStream<short##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, uint16_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (ResizableStream<ushort##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, int32_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (ResizableStream<int##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, uint32_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (ResizableStream<uint##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, int64_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (ResizableStream<long##count>*)values, element_configuration, element_config); \
				} \
				else if constexpr (std::is_same_v<BaseInteger, uint64_t>) { \
					ArrayInteger##count<BaseInteger>(configuration, config, name, (ResizableStream<ulong##count>*)values, element_configuration, element_config); \
				} \
				else { \
					static_assert(false, "Incorrect base integer argument"); \
				} \
			} \
			else { \
				static_assert(false, "Incorrect stream type argument"); \
			}

			// StreamType/values: CapacityStream<void> or ResizableStream<void>
			template<typename BaseInteger, typename StreamType>
			void ArrayInteger2Infer(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<void>, StreamType> || std::is_same_v<ResizableStream<void>, StreamType>,
					"Incorrect stream type for UIDrawer array integer 2 infer. Void streams should be given.");
				UI_DRAWER_ARRAY_INTEGER_INFER(2);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Instantiations - 3 components

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename StreamType>
			void ArrayFloat3(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<float3>, StreamType> || std::is_same_v<ResizableStream<float3>, StreamType>,
					"Incorrect stream type for UIDrawer array float3.");
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayFloat3Function);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/values: CapacityStream<double3> or ResizableStream<double3>
			template<typename StreamType>
			void ArrayDouble3(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<double3>, StreamType> || std::is_same_v<ResizableStream<double3>, StreamType>,
					"Incorrect stream type for UIDrawer array double3.");
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayDouble3Function);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/values: CapacityStream<Integer3> or ResizableStream<Integer3>
			template<typename BaseInteger, typename StreamType>
			void ArrayInteger3(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayInteger3Function<BaseInteger>);
			}

			// StreamType/values: CapacityStream<void> or ResizableStream<void>
			template<typename BaseInteger, typename StreamType>
			void ArrayInteger3Infer(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<void>, StreamType> || std::is_same_v<ResizableStream<void>, StreamType>,
					"Incorrect stream type for UIDrawer array integer3 infer. Void streams should be given.");
				UI_DRAWER_ARRAY_INTEGER_INFER(3);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Instantiations - 4 components

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/values: CapacityStream<float4> or ResizableStream<float4>
			template<typename StreamType>
			void ArrayFloat4(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<float4>, StreamType> || std::is_same_v<ResizableStream<float4>, StreamType>,
					"Incorrect stream type for UIDrawer array float4.");
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayFloat4Function);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/values: CapacityStream<double4> or ResizableStream<double4>
			template<typename StreamType>
			void ArrayDouble4(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<double4>, StreamType> || std::is_same_v<ResizableStream<double4>, StreamType>,
					"Incorrect stream type for UIDrawer array double4.");
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayDouble4Function);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/values: CapacityStream<Integer4> or ResizableStream<Integer4>
			template<typename BaseInteger, typename StreamType>
			void ArrayInteger4(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				Array(configuration, element_configuration, config, element_config, name, values, nullptr, UIDrawerArrayInteger4Function<BaseInteger>);
			}

			// StreamType/values: CapacityStream<void> or ResizableStream<void>
			template<typename BaseInteger, typename StreamType>
			void ArrayInteger4Infer(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* values,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<void>, StreamType> || std::is_same_v<ResizableStream<void>, StreamType>,
					"Incorrect stream type for UIDrawer array integer 4 infer. Void streams should be given.");
				UI_DRAWER_ARRAY_INTEGER_INFER(4);
			}


#undef UI_DRAWER_ARRAY_INTEGER_INFER

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Color

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/colors: CapacityStream<Color> or ResizableStream<Color>
			template<typename StreamType>
			void ArrayColor(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* colors,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<Color>, StreamType> || std::is_same_v<ResizableStream<Color>, StreamType>,
					"Incorrect stream type for UIDrawer array color.");
				Array(configuration, element_configuration, config, element_config, name, colors, nullptr, UIDrawerArrayColorFunction);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Color Float

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/colors: CapacityStream<ColorFloat> or ResizableStream<ColorFloat>
			template<typename StreamType>
			void ArrayColorFloat(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* colors,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<ColorFloat>, StreamType> || std::is_same_v<ResizableStream<ColorFloat>, StreamType>,
					"Incorrect stream type for UIDrawer array color float.");
				Array(configuration, element_configuration, config, element_config, name, colors, nullptr, UIDrawerArrayColorFloatFunction);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion


#pragma region Array Check Boxes

			// ------------------------------------------------------------------------------------------------------------------------------------
			
			// StreamType/states: CapacityStream<bool> or ResizableStream<bool>
			template<typename StreamType>
			void ArrayCheckBox(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* states,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<bool>, StreamType> || std::is_same_v<ResizableStream<bool>, StreamType>,
					"Incorrect stream type for UIDrawer array check box.");
				Array(configuration, element_configuration, config, element_config, name, states, nullptr, UIDrawerArrayCheckBoxFunction);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Text Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/texts: CapacityStream<CapacityStream<char>> or ResizableStream<CapacityStream<char>>
			template<typename StreamType>
			void ArrayTextInput(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* texts,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<CapacityStream<char>>, StreamType> || std::is_same_v<ResizableStream<CapacityStream<char>>, StreamType>,
					"Incorrect stream type for UIDrawer array text input.");

				UIDrawerTextInput** inputs = (UIDrawerTextInput**)GetTempBuffer(sizeof(UIDrawerTextInput*) * texts->size);

				Array(configuration, element_configuration, config, element_config, name, texts, inputs, UIDrawerArrayTextInputFunction,
					[](UIDrawer& drawer, void* _elements, unsigned int element_count, unsigned int* new_order, void* additional_data) 
				{
					Stream<CapacityStream<char>> elements(_elements, element_count);
					UIDrawerTextInput** inputs = (UIDrawerTextInput**)additional_data;

					size_t temp_marker = drawer.GetTempAllocatorMarker();
					CapacityStream<char>* copied_elements = (CapacityStream<char>*)drawer.GetTempBuffer(sizeof(CapacityStream<char>) * elements.size);
					/* Copy the old contents to temp buffers */
					for (size_t index = 0; index < elements.size; index++) {
						copied_elements[index].InitializeFromBuffer(drawer.GetTempBuffer(sizeof(char) * elements[index].size), elements[index].size, elements[index].size);
						copied_elements[index].Copy(elements[index]);
					}
					for (size_t index = 0; index < elements.size; index++) {
						if (new_order[index] != index) {
							inputs[index]->DeleteAllCharacters(); 
							inputs[index]->InsertCharacters(copied_elements[new_order[index]].buffer, copied_elements[new_order[index]].size, 0, drawer.system);
						}
					}
					drawer.ReturnTempAllocator(temp_marker);
				});
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Combo Box

			// ------------------------------------------------------------------------------------------------------------------------------------

			// StreamType/flags: CapacityStream<unsigned char> or ResizableStream<unsigned char>
			template<typename StreamType>
			void ArrayComboBox(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* flags,
				CapacityStream<Stream<Stream<char>>> flag_labels,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<unsigned char>, StreamType> || std::is_same_v<ResizableStream<unsigned char>, StreamType>,
					"Incorrect stream type for UIDrawer array combo box.");
				Array(configuration, element_configuration, config, element_config, name, flags, &flag_labels, UIDrawerArrayComboBoxFunction);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Directory Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename StreamType>
			void ArrayDirectoryInput(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* texts,
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<CapacityStream<wchar_t>>, StreamType> || std::is_same_v<ResizableStream<CapacityStream<wchar_t>>, StreamType>,
					"Incorrect stream type for UIDrawer array directory input.");
				Array(configuration, element_configuration, config, element_config, name, texts, nullptr, UIDrawerArrayDirectoryInputFunction);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region File Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename StreamType>
			void ArrayFileInput(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				StreamType* texts,
				Stream<Stream<wchar_t>> extensions = { nullptr, 0 },
				size_t element_configuration = 0,
				const UIDrawConfig* element_config = nullptr
			) {
				static_assert(std::is_same_v<CapacityStream<CapacityStream<wchar_t>>, StreamType> || std::is_same_v<ResizableStream<CapacityStream<wchar_t>>, StreamType>,
					"Incorrect stream type for UIDrawer array file input.");
				Array(configuration, element_configuration, config, element_config, name, texts, &extensions, UIDrawerArrayFileInputFunction);
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Alpha Color Rectangle

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AlphaColorRectangle(Color color);

			void AlphaColorRectangle(size_t configuration, const UIDrawConfig& config, float2& position, float2& scale, Color color);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AlphaColorRectangle(size_t configuration, const UIDrawConfig& config, Color color);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion 

			// Used to track which allocations have been made inside an initialization block
			void BeginElement();

#pragma region Button

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Button(
				Stream<char> label,
				UIActionHandler handler
			);

			void Button(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> label,
				UIActionHandler handler
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Converts to ASCII and sends it over to the ASCII variant
			void ButtonWide(Stream<wchar_t> label, UIActionHandler handler);

			// Converts to ASCII and sends it over to the ASCII variant
			void ButtonWide(size_t configuration, const UIDrawConfig& config, Stream<wchar_t> label, UIActionHandler handler);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void StateButton(Stream<char> name, bool* state);

			void StateButton(size_t configuration, const UIDrawConfig& config, Stream<char> name, bool* state);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SpriteStateButton(
				Stream<wchar_t> texture,
				bool* state,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				float2 expand_factor = { 1.0f, 1.0f }
			);

			void SpriteStateButton(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<wchar_t> texture,
				bool* state,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				float2 expand_factor = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Stateless, label is done with UI_CONFIG_DO_NOT_CACHE; if drawing with a sprite
			// name can be made nullptr
			void MenuButton(Stream<char> name, UIWindowDescriptor& window_descriptor, size_t border_flags = 0);

			// Stateless, label is done with UI_CONFIG_DO_NOT_CACHE; if drawing with a sprite
			// name can be made nullptr
			void MenuButton(size_t configuration, const UIDrawConfig& config, Stream<char> name, UIWindowDescriptor& window_descriptor, size_t border_flags = 0);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ChangeThemeColor(Color new_theme_color);

#pragma region Checkbox

			// ------------------------------------------------------------------------------------------------------------------------------------

			void CheckBox(Stream<char> name, bool* value_to_change);

			void CheckBox(size_t configuration, const UIDrawConfig& config, Stream<char> name, bool* value_to_change);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion 

#pragma region Combo box

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ComboBox(Stream<char> name, Stream<Stream<char>> labels, unsigned int label_display_count, unsigned char* active_label);

			void ComboBox(size_t configuration, UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels, unsigned int label_display_count, unsigned char* active_label);

			// If the name is nullptr, then it will ignore it aswell
			float ComboBoxDefaultSize(Stream<Stream<char>> labels, Stream<char> name = { nullptr, 0 }, Stream<char> prefix = { nullptr, 0 }) const;

			// It will report which label is the biggest.
			unsigned int ComboBoxBiggestLabel(Stream<Stream<char>> labels) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ComboBoxDropDownDrawer(size_t configuration, UIDrawConfig& config, UIDrawerComboBox* data);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Collapsing Header

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Function>
			bool CollapsingHeader(Stream<char> name, bool* state, Function&& function) {
				UIDrawConfig config;
				return CollapsingHeader(0, config, name, state, function);
			}

			template<typename Function>
			bool CollapsingHeader(size_t configuration, UIDrawConfig& config, Stream<char> name, bool* state, Function&& function) {
				float2 position;
				float2 scale;
				HandleTransformFlags(configuration, config, position, scale);

				auto call_function = [&]() {
					OffsetNextRow(layout.node_indentation);
					current_x = GetNextRowXPosition();
					function();
					OffsetNextRow(-layout.node_indentation);
					NextRow();
				};

				if (!initializer) {
					if (state == nullptr) {
						UIDrawerCollapsingHeader* element = (UIDrawerCollapsingHeader*)GetResource(name);

						// The name is still given in order to have unique names for buttons
						CollapsingHeaderDrawer(configuration, config, name, element, position, scale);
						HandleDynamicResource(configuration, name);
						if (element->state) {
							call_function();
						}
						return element->state;
					}
					else {
						CollapsingHeaderDrawer(configuration, config, name, state, position, scale);
						HandleDynamicResource(configuration, name);

						if (*state) {
							call_function();
						}
						return *state;
					}
				}
				else {
					if (state == nullptr) {
						CollapsingHeaderInitializer(configuration, config, name, position, scale);
					}
					function();
					return true;
				}
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Color Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ColorInput(Stream<char> name, Color* color, Color default_color = ECS_COLOR_WHITE);

			void ColorInput(size_t configuration, UIDrawConfig& config, Stream<char> name, Color* color, Color default_color = ECS_COLOR_WHITE);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Color Float Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ColorFloatInput(Stream<char> name, ColorFloat* color, ColorFloat default_color = ECS_COLOR_WHITE);

			void ColorFloatInput(size_t configuration, UIDrawConfig& config, Stream<char> name, ColorFloat* color, ColorFloat default_color = ECS_COLOR_WHITE);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ColorFloatInputIntensityInputName(char* intensity_input_name);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ColorFloatInputDrawer(size_t configuration, UIDrawConfig& config, Stream<char> name, ColorFloat* color, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerColorFloatInput* ColorFloatInputInitializer(size_t configuration, UIDrawConfig& config, Stream<char> name, ColorFloat* color, ColorFloat default_color, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region CrossLine

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Setting the y size for horizontal or x size for vertical as 0.0f means get default 1 pixel width
			void CrossLine();

			void CrossLine(size_t configuration, const UIDrawConfig& config);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DisablePaddingForRenderSliders();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DisablePaddingForRenderRegion();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DisableZoom() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Directory Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DirectoryInput(Stream<char> name, CapacityStream<wchar_t>* path);

			void DirectoryInput(size_t configuration, UIDrawConfig& config, Stream<char> name, CapacityStream<wchar_t>* path);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DirectoryInputDrawer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				CapacityStream<wchar_t>* path,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextInput* DirectoryInputInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				CapacityStream<wchar_t>* path
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region File Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FileInput(Stream<char> name, CapacityStream<wchar_t>* path, Stream<Stream<wchar_t>> extensions = { nullptr, 0 });

			void FileInput(size_t configuration, UIDrawConfig& config, Stream<char> name, CapacityStream<wchar_t>* path, Stream<Stream<wchar_t>> extensions = { nullptr, 0 });

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FileInputDrawer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				CapacityStream<wchar_t>* path,
				Stream<Stream<wchar_t>> extensions,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextInput* FileInputInitializer(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				CapacityStream<wchar_t>* path,
				Stream<Stream<wchar_t>> extensions
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Filesystem hierarchy

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FilesystemHierarchy(Stream<char> identifier, Stream<Stream<char>> labels);

			// Parent index 0 means root
			UIDrawerFilesystemHierarchy* FilesystemHierarchy(size_t configuration, UIDrawConfig& config, Stream<char> identifier, Stream<Stream<char>> labels);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerFilesystemHierarchy* FilesystemHierarchyInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> identifier);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FilesystemHierarchyDrawer(
				size_t configuration,
				UIDrawConfig& config,
				UIDrawerFilesystemHierarchy* data,
				Stream<Stream<char>> labels,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Filter Menu

			// ------------------------------------------------------------------------------------------------------------------------------------

			// States needs to be stable
			void FilterMenu(Stream<char> name, Stream<Stream<char>> label_names, bool** states);

			// States needs to be stable
			void FilterMenu(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> label_names, bool** states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// States should be a stable bool array
			void FilterMenu(Stream<char> name, Stream<Stream<char>> label_names, bool* states);

			// States should be a stable bool array
			void FilterMenu(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> label_names, bool* states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FilterMenuDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, UIDrawerFilterMenuData* data);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerFilterMenuData* FilterMenuInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> label_names, bool** states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FilterMenuDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, UIDrawerFilterMenuSinglePointerData* data);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerFilterMenuSinglePointerData* FilterMenuInitializer(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<char> name,
				Stream<Stream<char>> label_names,
				bool* states
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FitTextToScale(
				size_t configuration,
				UIDrawConfig& config,
				float2& font_size,
				float& character_spacing,
				Color& color,
				float2 scale,
				float padding,
				UIConfigTextParameters& previous_parameters
			) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FinishFitTextToScale(size_t configuration, UIDrawConfig& config, const UIConfigTextParameters& previous_parameters) const;
			
			// ------------------------------------------------------------------------------------------------------------------------------------

			// Returns the new font size that matches the given y scale with an optional y padding in the xy component
			// And the new character spacing in the z component
			float3 FitTextToScale(float scale, float padding = 0.0f) const;
			
			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Gradients

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Gradient(size_t configuration, const UIDrawConfig& config, const Color* colors, size_t horizontal_count, size_t vertical_count);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Graphs

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Graph(Stream<float2> samples, Stream<char> name = { nullptr, 0 }, unsigned int x_axis_precision = 2, unsigned int y_axis_precision = 2);

			void Graph(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<float2> unfiltered_samples,
				float filter_delta,
				Stream<char> name = { nullptr, 0 },
				unsigned int x_axis_precision = 2,
				unsigned int y_axis_precision = 2
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Graph(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<float2> samples, 
				Stream<char> name = { nullptr, 0 },
				unsigned int x_axis_precision = 2,
				unsigned int y_axis_precision = 2
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void MultiGraph(
				size_t configuration, 
				const UIDrawConfig& config, 
				Stream<float> samples, 
				size_t sample_count,
				const Color* colors,
				Stream<char> name = { nullptr, 0 },
				unsigned int x_axis_precision = 2,
				unsigned int y_axis_precision = 2
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerRowLayout GenerateRowLayout();

			// ------------------------------------------------------------------------------------------------------------------------------------

			float GetDefaultBorderThickness() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetDefaultBorderThickness2() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetElementDefaultScale() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetRelativeElementSize(float2 factors) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Accounts for zoom
			float2 GetStaticElementDefaultScale() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetCurrentPosition() const;
			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetCurrentPositionNonOffset() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetCurrentPositionStatic() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetWindowSizeFactors(ECS_UI_WINDOW_DEPENDENT_SIZE type, float2 scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetWindowSizeScaleElement(ECS_UI_WINDOW_DEPENDENT_SIZE type, float2 scale_factors) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// It returns the factor for ECS_UI_WINDOW_DEPENDENT_SIZE
			// If the until border flag is specified, it will return the scale until the actual region border
			// Not the region limit
			float2 GetWindowSizeScaleUntilBorder(bool until_border = false) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// It returns the actual size until the border
			// If the until border flag is specified, it will return the scale until the actual region border
			// Not the region limit
			float GetWindowScaleUntilBorder(bool until_border = false) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetRelativeTransformFactors(float2 desired_scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetRelativeTransformFactorsZoomed(float2 desired_scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIConfigAbsoluteTransform GetWholeRegionTransform(bool consider_region_header = false) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void* GetTempBuffer(size_t size, ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL, size_t alignment = 8);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Returns memory from the memory handler of the callbacks - it is temporary (valid for this drawer only)
			void* GetHandlerBuffer(size_t size, ECS_UI_DRAW_PHASE phase);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Can be used to release some temp memory - cannot be used when handlers are used
			void ReturnTempAllocator(size_t marker, ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL);

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t GetTempAllocatorMarker(ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UISystem* GetSystem();

			// ------------------------------------------------------------------------------------------------------------------------------------

			// If defaulted, it will return the square scale of the default_element_y
			float2 GetSquareScale(float value = FLT_MAX) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// If defaulted, it will return the square scale of the default_element_y
			float2 GetSquareScaleScaled(float value = FLT_MAX) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// It will use HandleResourceIdentifier to construct the string
			void* GetResource(Stream<char> string);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// It will forward directly to the UI system; No HandleResourceIdentifier used
			void* FindWindowResource(Stream<char> string);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetRegionPosition() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetRegionScale() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetRegionRenderOffset() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// returns the render bound difference
			float2 GetRenderSpan() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// returns the viewport visible zone
			float2 GetRenderZone() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void** GetBuffers();

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerBufferState GetBufferState(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerHandlerState GetHandlerState();

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t* GetCounts();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void** GetSystemBuffers();

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t* GetSystemCounts();

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetFontSize() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIFontDescriptor* GetFontDescriptor();

			// ------------------------------------------------------------------------------------------------------------------------------------

			UILayoutDescriptor* GetLayoutDescriptor();

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIColorThemeDescriptor* GetColorThemeDescriptor();

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIElementDescriptor* GetElementDescriptor();

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetLastSolidColorRectanglePosition(size_t configuration = 0, unsigned int previous_rectangle = 0);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetLastSolidColorRectangleScale(size_t configuration = 0, unsigned int previous_rectangle = 0);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetLastSpriteRectanglePosition(size_t configuration = 0, unsigned int previous_rectangle = 0);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetLastSpriteRectangleScale(size_t configuration = 0, unsigned int previous_rectangle = 0);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDockspace* GetDockspace();

			// ------------------------------------------------------------------------------------------------------------------------------------

			float GetAlignedToCenterX(float x_scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float GetAlignedToCenterY(float y_scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetAlignedToCenter(float2 scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------
			
			float2 GetAlignedToRight(float x_scale, float target_position = FLT_MAX) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetAlignedToRightOverLimit(float x_scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetAlignedToBottom(float y_scale, float target_position = FLT_MAX) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetAlignedToBottomOverLimit(float y_scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Returns the position of a rectangle aligned to the corners of the window - it doesn't take into account the renderable
			// region - only the true window corners. Can optionally add padding to the position
			float2 GetCornerRectangle(ECS_UI_ALIGN horizontal_alignment, ECS_UI_ALIGN vertical_alignment, float2 dimensions, float2 padding = { 0.0f, 0.0f }) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			unsigned int GetBorderIndex() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			DockspaceType GetDockspaceType() const;

			// ------------------------------------------------------------------------------------------------------------------------------------
			
			ActionData GetDummyActionData();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void* GetMainAllocatorBuffer(size_t size, size_t alignment = 8);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename T>
			T* GetMainAllocatorBuffer() {
				return (T*)GetMainAllocatorBuffer(sizeof(T), alignof(T));
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			// name must not conflict with other element names
			template<typename T>
			T* GetMainAllocatorBufferAndStoreAsResource(Stream<char> name) {
				return (T*)GetMainAllocatorBufferAndStoreAsResource(name, sizeof(T), alignof(T));
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			void* GetMainAllocatorBufferAndStoreAsResource(Stream<char> name, size_t size, size_t alignment = 8);

			// ------------------------------------------------------------------------------------------------------------------------------------

			unsigned int GetWindowIndex() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Hierarchy

			// ------------------------------------------------------------------------------------------------------------------------------------



			// TODO: Deprecate this (eliminate)
			UIDrawerHierarchy* Hierarchy(Stream<char> name);

			// TODO: Deprecate this (eliminate)
			UIDrawerHierarchy* Hierarchy(size_t configuration, const UIDrawConfig& config, Stream<char> name);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Histogram

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Histogram(Stream<float> samples, Stream<char> name = { nullptr, 0 });

			void Histogram(size_t configuration, const UIDrawConfig& config, Stream<float> samples, Stream<char> name = { nullptr, 0 });

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void InitializeElementName(
				size_t configuration,
				size_t no_element_name,
				const UIDrawConfig& config,
				Stream<char> name,
				UIDrawerTextElement* element,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool HasClicked(float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool IsClicked(float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool IsMouseInRectangle(float2 position, float2 scale);
			
			// ------------------------------------------------------------------------------------------------------------------------------------

			bool IsActiveWindow() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Indent(float count = 1.0f);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Indents a percentage of the window's size
			void IndentWindowSize(float percentage);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region List

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerList* List(Stream<char> name);

			UIDrawerList* List(size_t configuration, UIDrawConfig& config, Stream<char> name);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ListFinalizeNode(UIDrawerList* list);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Label Hierarchy

			// The name is not displayed, is used only for the resource storage
			void LabelHierarchy(Stream<char> name, IteratorPolymorphic* iterator, bool keep_storage = false);

			// The name is not displayed, is used only for the resource storage
			// Functions needed for the iterator (the one from the TreeIterator):	bool Valid();
			//																		Stream<char> Next(unsigned int* level);
			//																		Stream<char> Peek(unsigned int* level, bool* has_children);
			//																		void Skip();
			void LabelHierarchy(size_t configuration, const UIDrawConfig& config, Stream<char> name, IteratorPolymorphic* iterator, bool keep_storage = false);

			void LabelHierarchyDrawer(
				size_t configuration,
				const UIDrawConfig& config,
				UIDrawerLabelHierarchyData* data,
				float2 position,
				float2 scale,
				unsigned int dynamic_resource_index,
				IteratorPolymorphic* iterator
			);

			UIDrawerLabelHierarchyData* LabelHierarchyInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, unsigned int storage_type_size = 0);

#pragma endregion

#pragma region Label List

			// ------------------------------------------------------------------------------------------------------------------------------------

			void LabelList(Stream<char> name, Stream<Stream<char>> labels);

			void LabelList(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void LabelListDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerLabelList* data, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void LabelListDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerLabelList* LabelListInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Menu

			// ------------------------------------------------------------------------------------------------------------------------------------

			// State can be stack allocated; name should be unique if drawing with a sprite
			void Menu(Stream<char> name, UIDrawerMenuState* state);

			// State can be stack allocated; name should be unique if drawing with a sprite
			void Menu(size_t configuration, const UIDrawConfig& config, Stream<char> name, UIDrawerMenuState* state);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void* MenuAllocateStateHandlers(Stream<UIActionHandler> handlers);

#pragma endregion

#pragma region Number input

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatInput(Stream<char> name, float* value, float default_value = 0.0f, float lower_bound = -FLT_MAX, float upper_bound = FLT_MAX);

			void FloatInput(size_t configuration, UIDrawConfig& config, Stream<char> name, float* value, float default_value = 0.0f, float lower_bound = -FLT_MAX, float upper_bound = FLT_MAX);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatInputGroup(
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				float** values,
				const float* default_values = nullptr,
				const float* lower_bound = nullptr,
				const float* upper_bound = nullptr
			);

			void FloatInputGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				float** values,
				const float* default_values = nullptr,
				const float* lower_bound = nullptr,
				const float* upper_bound = nullptr
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleInput(Stream<char> name, double* value, double default_value = 0, double lower_bound = -DBL_MAX, double upper_bound = DBL_MAX);

			void DoubleInput(size_t configuration, UIDrawConfig& config, Stream<char> name, double* value, double default_value = 0, double lower_bound = -DBL_MAX, double upper_bound = DBL_MAX);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleInputGroup(
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				double** values,
				const double* default_values = nullptr,
				const double* lower_bound = nullptr,
				const double* upper_bound = nullptr
			);

			void DoubleInputGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				double** values,
				const double* default_values = nullptr,
				const double* lower_bound = nullptr,
				const double* upper_bound = nullptr
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntInput(Stream<char> name, Integer* value, Integer default_value = 0, Integer min = LLONG_MIN, Integer max = ULLONG_MAX);

			template<typename Integer>
			ECSENGINE_API void IntInput(size_t configuration, UIDrawConfig& config, Stream<char> name, Integer* value, Integer default_value = 0, Integer min = LLONG_MIN, Integer max = ULLONG_MAX);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntInputGroup(
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				Integer** values,
				const Integer* default_values = nullptr,
				const Integer* lower_bound = nullptr,
				const Integer* upper_bound = nullptr
			);

			template<typename Integer>
			ECSENGINE_API void IntInputGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				Integer** values,
				const Integer* default_values = nullptr,
				const Integer* lower_bound = nullptr,
				const Integer* upper_bound = nullptr
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void NextRow(float count = 1.0f);

			// Moves the current position by a percentage of the window size
			void NextRowWindowSize(float percentage);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float NormalizeHorizontalToWindowDimensions(float value) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void OffsetNextRow(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void OffsetX(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void OffsetY(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Progress bar

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ProgressBar(Stream<char> name, float percentage, float x_scale_factor = 1);

			void ProgressBar(size_t configuration, UIDrawConfig& config, Stream<char> name, float percentage, float x_scale_factor = 1);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void PopIdentifierStack();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void PushIdentifierStack(Stream<char> identifier);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// This version eliminates the string pattern before pushing on the stack
			void PushIdentifierStackEx(Stream<char> identifier);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void PushIdentifierStack(size_t index);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void PushIdentifierStackRandom(size_t index);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Returns whether or not it pushed onto the stack
			bool PushIdentifierStackStringPattern();

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Instructs the drawer that for the upcoming elements to try and retrieve a drag payload.
			void PushDragDrop(Stream<Stream<char>> names, bool highlight_border = true, Color highlight_color = ECS_COLOR_WHITE);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Returns the drag data for the last element. If there is no data, it returns { nullptr, 0 }. Can optionally ask the
			// drawer to give you which name provided the drag data. If the remove drag drop is set to true, then the drag drop
			// is deactivated for the next elements. Else it will be kept
			Stream<void> ReceiveDragDrop(bool remove_drag_drop = true, unsigned int* matched_name = nullptr);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void RemoveAllocation(const void* allocation);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIReservedHandler ReserveHoverable(ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIReservedHandler ReserveClickable(ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT, ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIReservedHandler ReserveGeneral(ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void RestoreBufferState(size_t configuration, UIDrawerBufferState state);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void RestoreHandlerState(UIDrawerHandlerState state);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Sentence

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Sentence(Stream<char> text);

			void Sentence(size_t configuration, const UIDrawConfig& config, Stream<char> text);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Selectable List

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SelectableList(Stream<Stream<char>> labels, unsigned int* selected_label);

			void SelectableList(size_t configuration, const UIDrawConfig& config, Stream<Stream<char>> labels, unsigned int* selected_label);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SelectableList(Stream<Stream<char>> labels, CapacityStream<char>* selected_label);

			void SelectableList(size_t configuration, const UIDrawConfig& config, Stream<Stream<char>> labels, CapacityStream<char>* selected_label);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetCurrentX(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetCurrentY(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Sets the current x and y to be the region position
			void SetCurrentPositionToHeader();

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Sets the current x and y to the beginning of the normal render space
			void SetCurrentPositionToStart();

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<bool reduce_values = true>
			ECSENGINE_API void SetLayoutFromZoomXFactor(float zoom_factor);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<bool reduce_values = true>
			ECSENGINE_API void SetLayoutFromZoomYFactor(float zoom_factor);

			// ------------------------------------------------------------------------------------------------------------------------------------


#pragma region Rectangle

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Rectangle(size_t configuration, const UIDrawConfig& config);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Rectangles(size_t configuration, const UIDrawConfig& overall_config, const UIDrawConfig* configs, const float2* offsets, size_t count);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Rectangles(size_t configuration, const UIDrawConfig& overall_config, const UIDrawConfig* configs, float2 offset, size_t count);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Sliders

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Slider(
				Stream<char> name,
				unsigned int byte_size,
				void* value_to_modify,
				const void* lower_bound,
				const void* upper_bound,
				const void* default_value,
				const UIDrawerSliderFunctions& functions,
				UIDrawerTextInputFilter filter
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Slider(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name,
				unsigned int byte_size,
				void* value_to_modify,
				const void* lower_bound,
				const void* upper_bound,
				const void* default_value,
				const UIDrawerSliderFunctions& functions,
				UIDrawerTextInputFilter filter
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SliderGroup(
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				unsigned int byte_size,
				void** ECS_RESTRICT values_to_modify,
				const void* ECS_RESTRICT lower_bounds,
				const void* ECS_RESTRICT upper_bounds,
				const void* ECS_RESTRICT default_values,
				const UIDrawerSliderFunctions& functions,
				UIDrawerTextInputFilter filter
			);

			void SliderGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				unsigned int byte_size,
				void** ECS_RESTRICT values_to_modify,
				const void* ECS_RESTRICT lower_bounds,
				const void* ECS_RESTRICT upper_bounds,
				const void* ECS_RESTRICT default_values,
				const UIDrawerSliderFunctions& functions,
				UIDrawerTextInputFilter filter
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatSlider(Stream<char> name, float* value_to_modify, float lower_bound, float upper_bound, float default_value = 0.0f, unsigned int precision = 3);

			void FloatSlider(size_t configuration, UIDrawConfig& config, Stream<char> name, float* value_to_modify, float lower_bound, float upper_bound, float default_value = 0.0f, unsigned int precision = 3);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatSliderGroup(
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				float** values_to_modify,
				const float* lower_bounds,
				const float* upper_bounds,
				const float* default_values = nullptr,
				unsigned int precision = 3
			);

			void FloatSliderGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				float** values_to_modify,
				const float* lower_bounds,
				const float* upper_bounds,
				const float* default_values = nullptr,
				unsigned int precision = 3
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleSlider(Stream<char> name, double* value_to_modify, double lower_bound, double upper_bound, double default_value = 0, unsigned int precision = 3);

			void DoubleSlider(size_t configuration, UIDrawConfig& config, Stream<char> name, double* value_to_modify, double lower_bound, double upper_bound, double default_value = 0, unsigned int precision = 3);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleSliderGroup(
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				double** values_to_modify,
				const double* lower_bounds,
				const double* upper_bounds,
				const double* default_values = nullptr,
				unsigned int precision = 3
			);

			void DoubleSliderGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				double** values_to_modify,
				const double* lower_bounds,
				const double* upper_bounds,
				const double* default_values = nullptr,
				unsigned int precision = 3
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntSlider(Stream<char> name, Integer* value_to_modify, Integer lower_bound, Integer upper_bound, Integer default_value = 0);

			template<typename Integer>
			ECSENGINE_API void IntSlider(size_t configuration, UIDrawConfig& config, Stream<char> name, Integer* value_to_modify, Integer lower_bound, Integer upper_bound, Integer default_value = 0);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntSliderGroup(
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				Integer** values_to_modify,
				const Integer* lower_bounds,
				const Integer* upper_bounds,
				const Integer* default_values = nullptr
			);

			template<typename Integer>
			ECSENGINE_API void IntSliderGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				Integer** values_to_modify,
				const Integer* lower_bounds,
				const Integer* upper_bounds,
				const Integer* default_values = nullptr
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatDraggable(Stream<char> name, float* value_to_modify, float lower_bound = -FLT_MAX, float upper_bound = FLT_MAX, float default_value = 0.0f, unsigned int precision = 3);

			void FloatDraggable(
				size_t configuration,
				UIDrawConfig& config, 
				Stream<char> name,
				float* value_to_modify,
				float lower_bound = -FLT_MAX, 
				float upper_bound = FLT_MAX, 
				float default_value = 0.0f, 
				unsigned int precision = 3
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatDraggableGroup(
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				float** values_to_modify,
				const float* lower_bounds = nullptr,
				const float* upper_bounds = nullptr,
				const float* default_values = nullptr,
				unsigned int precision = 3
			);

			void FloatDraggableGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				float** values_to_modify,
				const float* lower_bounds = nullptr,
				const float* upper_bounds = nullptr,
				const float* default_values = nullptr,
				unsigned int precision = 3
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleDraggable(Stream<char> name, double* value_to_modify, double lower_bound = -DBL_MAX, double upper_bound = DBL_MAX, double default_value = 0, unsigned int precision = 3);

			void DoubleDraggable(
				size_t configuration, 
				UIDrawConfig& config, 
				Stream<char> name, 
				double* value_to_modify, 
				double lower_bound = -DBL_MAX,
				double upper_bound = DBL_MAX,
				double default_value = 0, 
				unsigned int precision = 3
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleDraggableGroup(
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				double** values_to_modify,
				const double* lower_bounds = nullptr,
				const double* upper_bounds = nullptr,
				const double* default_values = nullptr,
				unsigned int precision = 3
			);

			void DoubleDraggableGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				double** values_to_modify,
				const double* lower_bounds = nullptr,
				const double* upper_bounds = nullptr,
				const double* default_values = nullptr,
				unsigned int precision = 3
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntDraggable(Stream<char> name, Integer* value_to_modify, Integer lower_bound = 0, Integer upper_bound = 0, Integer default_value = 0);

			template<typename Integer>
			ECSENGINE_API void IntDraggable(
				size_t configuration,
				UIDrawConfig& config,
				Stream<char> name, 
				Integer* value_to_modify, 
				Integer lower_bound = 0, 
				Integer upper_bound = 0,
				Integer default_value = 0
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntDraggableGroup(
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				Integer** values_to_modify,
				const Integer* lower_bounds = nullptr,
				const Integer* upper_bounds = nullptr,
				const Integer* default_values = nullptr
			);

			template<typename Integer>
			ECSENGINE_API void IntDraggableGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				Stream<char> group_name,
				Stream<char>* names,
				Integer** values_to_modify,
				const Integer* lower_bounds = nullptr,
				const Integer* upper_bounds = nullptr,
				const Integer* default_values = nullptr
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Selection Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			// A label displays the selection and at the end there is a button that will create a new window when pressed
			// which allows to select an item. Give the name a size of 0 to skip it
			void SelectionInput(
				Stream<char> name,
				CapacityStream<char>* selection, 
				Stream<wchar_t> texture, 
				const UIWindowDescriptor* window_descriptor,
				UIActionHandler selection_callback = {}
			);

			// ------------------------------------------------------------------------------------------------------------------------------------
			
			// A label displays the selection and at the end there is a button that will create a new window when pressed
			// which allows to select an item. Give the name a size of 0 to skip it
			void SelectionInput(
				size_t configuration, 
				const UIDrawConfig& config, 
				Stream<char> name, 
				CapacityStream<char>* selection,
				Stream<wchar_t> texture, 
				const UIWindowDescriptor* window_descriptor,
				UIActionHandler selection_callback = {}
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetZoomXFactor(float zoom_x_factor);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetZoomYFactor(float zoom_y_factor);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// An offset added to the already in place padding
			// Useful for node indentation
			void SetRowHorizontalOffset(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// The default row horizontal padding
			void SetRowPadding(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetNextRowYOffset(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------
			
			// Currently, draw_mode_float is needed for column draw which denotes the spacing in between columns
			// And the target is how many elements to be drawn for each column, if such a behaviour is desired
			void SetDrawMode(ECS_UI_DRAWER_MODE mode, unsigned int target = 0, float draw_mode_float = 0.0f);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Can be used when some sort of caching is used to cull many objects at once
			void SetRenderSpan(float2 value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// places a hoverable on the whole window
			void SetWindowHoverable(const UIActionHandler* handler);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// places a clickable on the whole window
			void SetWindowClickable(const UIActionHandler* handler, ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// places a general on the whole window
			void SetWindowGeneral(const UIActionHandler* handler);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// configuration is needed for phase deduction
			void SetSpriteClusterTexture(size_t configuration, Stream<wchar_t> texture, unsigned int count);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Solid Color Rectangle

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SolidColorRectangle(Color color = ECS_TOOLS_UI_THEME_COLOR);

			void SolidColorRectangle(size_t configuration, const UIDrawConfig& config, Color color = ECS_TOOLS_UI_THEME_COLOR);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region State Table

			// ------------------------------------------------------------------------------------------------------------------------------------

			// States should be a stack pointer to bool* to the members that should be changed
			void StateTable(Stream<char> name, Stream<Stream<char>> labels, bool** states);

			// States should be a stack pointer to a bool array
			void StateTable(Stream<char> name, Stream<Stream<char>> labels, bool* states);

			// States should be a stack pointer to bool* to the members that should be changed
			void StateTable(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels, bool** states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// States should be a stack pointer to a bool array
			void StateTable(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels, bool* states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerStateTable* StateTableInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void StateTableDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerStateTable* data, bool** states, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Sprite Rectangle

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SpriteRectangle(Stream<wchar_t> texture, Color color = ECS_COLOR_WHITE, float2 top_left_uv = { 0.0f, 0.0f }, float2 bottom_right_uv = { 1.0f, 1.0f });

			void SpriteRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<wchar_t> texture,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SpriteRectangleDouble(
				Stream<wchar_t> texture0,
				Stream<wchar_t> texture1,
				Color color0 = ECS_COLOR_WHITE,
				Color color1 = ECS_COLOR_WHITE,
				float2 top_left_uv0 = { 0.0f, 0.0f },
				float2 bottom_right_uv0 = { 1.0f, 1.0f },
				float2 top_left_uv1 = { 0.0f, 0.0f },
				float2 bottom_right_uv1 = { 1.0f, 1.0f }
			);

			void SpriteRectangleDouble(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<wchar_t> texture0,
				Stream<wchar_t> texture1,
				Color color0 = ECS_COLOR_WHITE,
				Color color1 = ECS_COLOR_WHITE,
				float2 top_left_uv0 = { 0.0f, 0.0f },
				float2 bottom_right_uv0 = { 1.0f, 1.0f },
				float2 top_left_uv1 = { 0.0f, 0.0f },
				float2 bottom_right_uv1 = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Sprite Button

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SpriteButton(
				UIActionHandler clickable,
				Stream<wchar_t> texture,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			void SpriteButton(
				size_t configuration,
				const UIDrawConfig& config,
				UIActionHandler clickable,
				Stream<wchar_t> texture,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			void SpriteButton(
				UIActionHandler clickable,
				ResourceView texture,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			void SpriteButton(
				size_t configuration,
				const UIDrawConfig& config,
				UIActionHandler clickable,
				ResourceView texture,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Sprite Texture Bool

			// A clickable handler can be supplied in addition to the default state change
			// It receives the state in the additional_data parameter
			void SpriteTextureBool(
				Stream<wchar_t> texture_false,
				Stream<wchar_t> texture_true,
				bool* state,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// A clickable handler can be supplied in addition to the default state change
			// It receives the state in the additional_data parameter
			void SpriteTextureBool(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<wchar_t> texture_false,
				Stream<wchar_t> texture_true,
				bool* state,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// A clickable handler can be supplied in addition to the default state change
			// It receives the state in the additional_data parameter
			void SpriteTextureBool(
				Stream<wchar_t> texture_false,
				Stream<wchar_t> texture_true,
				size_t* flags,
				size_t flag_to_set,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// A clickable handler can be supplied in addition to the default state change
			// It receives the state in the additional_data parameter
			void SpriteTextureBool(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<wchar_t> texture_false,
				Stream<wchar_t> texture_true,
				size_t* flags,
				size_t flag_to_set,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

#pragma endregion

#pragma region Text Table

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextTable(Stream<char> name, unsigned int rows, unsigned int columns, Stream<char>* labels);

			void TextTable(size_t configuration, const UIDrawConfig& config, Stream<char> name, unsigned int rows, unsigned int columns, Stream<char>* labels);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Text Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Single lined text input
			UIDrawerTextInput* TextInput(Stream<char> name, CapacityStream<char>* text_to_fill);

			// Single lined text input
			UIDrawerTextInput* TextInput(size_t configuration, UIDrawConfig& config, Stream<char> name, CapacityStream<char>* text_to_fill, UIDrawerTextInputFilter filter = UIDrawerTextInputFilterAll);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Text Label

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextLabel(Stream<char> characters);

			void TextLabel(size_t configuration, const UIDrawConfig& config, Stream<char> characters);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextLabelWide(Stream<wchar_t> characters);

			void TextLabelWide(size_t configuration, const UIDrawConfig& config, Stream<wchar_t> characters);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Text

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Text(Stream<char> text);

			// non cached drawer
			void Text(size_t configuration, const UIDrawConfig& config, Stream<char> characters, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Text(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2& position, float2& text_span);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Text(size_t configuration, const UIDrawConfig& config, Stream<char> characters);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region TextWide - Just converts to ASCII and then forwards

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextWide(Stream<wchar_t> text);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextWide(size_t configuration, const UIDrawConfig& config, Stream<wchar_t> characters);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextToolTip(Stream<char> characters, float2 position, float2 scale, const UITooltipBaseData* base = nullptr);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIActionHandler TextToolTipHandler(
				Stream<char> characters, 
				const UITextTooltipHoverableData* data, 
				bool stable_characters = false, 
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Default values for the parameters
			UIConfigTextParameters TextParameters() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DefaultHoverableWithToolTip(
				size_t configuration, 
				Stream<char> characters, 
				float2 position, 
				float2 scale, 
				const Color* color = nullptr, 
				const float* percentage = nullptr, 
				const UITooltipBaseData* base = nullptr
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DefaultHoverableWithToolTip(size_t configuration, float2 position, float2 scale, const UIDefaultHoverableWithTooltipData* data);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Vertex Color Rectangle

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				Color top_left,
				Color top_right,
				Color bottom_left,
				Color bottom_right
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				ColorFloat top_left,
				ColorFloat top_right,
				ColorFloat bottom_left,
				ColorFloat bottom_right
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<wchar_t> texture,
				const Color* colors,
				const float2* uvs,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<wchar_t> texture,
				const Color* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<wchar_t> texture,
				const ColorFloat* colors,
				const float2* uvs,
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<wchar_t> texture,
				const ColorFloat* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		//private:
			UISystem* system;
			UIDockspace* dockspace;
			unsigned int thread_id;
			unsigned int window_index;
			unsigned int border_index;
			DockspaceType dockspace_type;
			float current_x;
			float current_y;
			float current_column_x_scale;
			float current_row_y_scale;
			ECS_UI_DRAWER_MODE draw_mode;
			bool no_padding_for_render_sliders;
			bool no_padding_render_region;
			bool deallocate_constructor_allocations;
			bool initializer;
			unsigned short draw_mode_count;
			unsigned short draw_mode_target;
			float4 draw_mode_extra_float;
			float2 region_position;
			float2 region_scale;
			float2 region_limit;
			float2 region_render_offset;
			float2 max_region_render_limit;
			float2 min_region_render_limit;
			float2 max_render_bounds;
			float2 min_render_bounds;
			float2 mouse_position;
			float region_fit_space_horizontal_offset;
			float region_fit_space_vertical_offset;
			float next_row_offset;
			mutable const float2* zoom_ptr;
			float2* zoom_ptr_modifyable;
			float2* export_scale;
			float2 zoom_inverse;
			UILayoutDescriptor& layout;
			UIFontDescriptor& font;
			UIColorThemeDescriptor& color_theme;
			UIElementDescriptor& element_descriptor;
			void** buffers;
			size_t* counts;
			void** system_buffers;
			size_t* system_counts;
			void* window_data;
			Stream<unsigned char> identifier_stack;
			Stream<char> current_identifier;
			UIDrawerAcquireDragDrop acquire_drag_drop;
			CapacityStream<void*> last_initialized_element_allocations;
			CapacityStream<ResourceIdentifier> last_initialized_element_table_resources;
			CapacityStream<unsigned int> late_hoverables;
			CapacityStream<unsigned int> late_clickables[ECS_MOUSE_BUTTON_COUNT];
			CapacityStream<unsigned int> late_generals;
		};

		typedef void (*UIDrawerInitializeFunction)(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		// --------------------------------------------------------------------------------------------------------------

		// Last initialized element allocations and table resources must be manually deallocated after finishing using it
		ECSENGINE_API UIDrawer InitializeInitializerDrawer(UIDrawer& drawer_to_copy);

		// --------------------------------------------------------------------------------------------------------------

		// The identifier stack and the current identifier are used to initialize the identifier stack of the initializing drawer
		// These are mostly used by the internal initializing functions inside the UIDrawer
		ECSENGINE_API void InitializeDrawerElement(
			UIDrawer& drawer_to_copy,
			void* additional_data,
			Stream<char> name,
			UIDrawerInitializeFunction initialize,
			size_t configuration
		);

		// --------------------------------------------------------------------------------------------------------------

#pragma region Initialize dynamic elements

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeComboBoxElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeColorInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeFilterMenuElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeFilterMenuSinglePointerElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------
		
		ECSENGINE_API void InitializeHierarchyElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeFilesystemHierarchyElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeListElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeLabelHierarchyElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------
		
		ECSENGINE_API void InitializeMenuElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeFloatInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		ECSENGINE_API void InitializeIntegerInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);
		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeDoubleInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeFloatInputGroupElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		ECSENGINE_API void InitializeIntegerInputGroupElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeDoubleInputGroupElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeSliderElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeSliderGroupElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeStateTableElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeStateTableSinglePointerElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeTextInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeColorFloatInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeArrayElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeFileInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeDirectoryInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array functions

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayFloatFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayDoubleFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		ECSENGINE_API void UIDrawerArrayIntegerFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayFloat2Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayDouble2Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		ECSENGINE_API void UIDrawerArrayInteger2Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------
		
		ECSENGINE_API void UIDrawerArrayFloat3Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayDouble3Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		ECSENGINE_API void UIDrawerArrayInteger3Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayFloat4Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayDouble4Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		ECSENGINE_API void UIDrawerArrayInteger4Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayColorFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayColorFloatFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayCheckBoxFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayTextInputFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayComboBoxFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayDirectoryInputFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayFileInputFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

#pragma endregion

		// --------------------------------------------------------------------------------------------------------------

	}

}