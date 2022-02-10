#pragma once
#include "UISystem.h"
#include "UIDrawConfig.h"
#include "UIDrawConfigs.h"
#include "UIDrawerActionStructures.h"
#include "UIDrawerActions.h"

#define UI_HIERARCHY_NODE_FUNCTION ECSEngine::Tools::UIDrawer* drawer = (ECSEngine::Tools::UIDrawer*)drawer_ptr; \
												ECSEngine::Tools::UIDrawerHierarchy* hierarchy = (ECSEngine::Tools::UIDrawerHierarchy*)hierarchy_ptr

#define UI_LIST_NODE_FUNCTION  ECSEngine::Tools::UIDrawer* drawer = (ECSEngine::Tools::UIDrawer*)drawer_ptr; \
											ECSEngine::Tools::UIDrawerList* list = (ECSEngine::Tools::UIDrawerList*)list_ptr; \
											list->InitializeNodeYScale(drawer->GetCounts())

namespace ECSEngine {

	namespace Tools {

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Flag>
		void SetConfigParameter(size_t configuration, size_t bit_flag, UIDrawConfig& config, const Flag& flag, Flag& previous_flag) {
			if (configuration & bit_flag) {
				config.SetExistingFlag(flag, bit_flag, previous_flag);
			}
			else {
				config.AddFlag(flag);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Flag>
		void RemoveConfigParameter(size_t configuration, size_t bit_flag, UIDrawConfig& config, const Flag& previous_flag) {
			if (configuration & bit_flag) {
				config.RestoreFlag(previous_flag, bit_flag);
			}
			else {
				config.flag_count--;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		struct ECSENGINE_API UIDrawer
		{
		public:	
			
			// ------------------------------------------------------------------------------------------------------------------------------------

			void CalculateRegionParameters(float2 position, float2 scale, const float2* render_offset);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DefaultDrawParameters();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ConvertTextToWindowResource(size_t configuration, const UIDrawConfig& config, const char* text, UIDrawerTextElement* element, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ExpandRectangleFromFlag(const float* resize_factor, float2& position, float2& scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool ValidatePosition(size_t configuration, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool ValidatePosition(size_t configuration, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool ValidatePositionY(size_t configuration, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool ValidatePositionMinBound(size_t configuration, float2 position, float2 scale);

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

			UIDrawPhase HandlePhase(size_t configuration);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleTransformFlags(size_t configuration, const UIDrawConfig& config, float2& position, float2& scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleText(size_t configuration, const UIDrawConfig& config, Color& color, float2& font_size, float& character_spacing) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleTextStreamColorUpdate(Color color, Stream<UISpriteVertex> vertices);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleTextStreamColorUpdate(Color color, CapacityStream<UISpriteVertex> vertices);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleTextLabelAlignment(
				size_t configuration,
				const UIDrawConfig& config,
				float2 text_span,
				float2 label_size,
				float2 label_position,
				float& x_position,
				float& y_position,
				TextAlignment& horizontal_alignment,
				TextAlignment& vertical_alignment
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleTextLabelAlignment(
				float2 text_span,
				float2 label_scale,
				float2 label_position,
				float& x_position,
				float& y_position,
				TextAlignment horizontal,
				TextAlignment vertical
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleDynamicResource(size_t configuration, const char* name);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleBorder(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			Color HandleColor(size_t configuration, const UIDrawConfig& config) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			const char* HandleResourceIdentifier(const char* input, bool permanent_buffer = false);

			// ------------------------------------------------------------------------------------------------------------------------------------

			const char* HandleResourceIdentifier(const char* input, size_t& size, bool permanent_buffer = false);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void HandleRectangleActions(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextElement* HandleTextCopyFromResource(
				size_t configuration,
				const char* text, 
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
				const char* text,
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
				float& length, float& padding,
				float2& shrink_factor,
				Color& slider_color,
				Color background_color
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SolidColorRectangle(size_t configuration, float2 position, float2 scale, Color color);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SolidColorRectangle(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SolidColorRectangle(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale, Color color);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// it will finalize the rectangle
			void TextLabel(size_t configuration, const UIDrawConfig& config, const char* text, float2& position, float2& scale);

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
				LPCWSTR texture,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				Color color = ECS_COLOR_WHITE
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteClusterRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				const wchar_t* texture,
				const Color* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteClusterRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				const wchar_t* texture,
				const ColorFloat* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// resource drawer
			void Text(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextLabelWithCull(size_t configuration, const UIDrawConfig& config, const char* text, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextElement* TextInitializer(size_t configuration, const UIDrawConfig& config, const char* characters, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextElement* TextLabelInitializer(size_t configuration, const UIDrawConfig& config, const char* characters, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextLabelDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* element, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// single lined text input; drawer kernel
			void TextInputDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerTextInput* input, float2 position, float2 scale, UIDrawerTextInputFilter filter);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextInput* TextInputInitializer(size_t configuration, UIDrawConfig& config, const char* name, CapacityStream<char>* text_to_fill, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerSlider* SliderInitializer(
				size_t configuration,
				UIDrawConfig& config,
				const char* name,
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
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
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
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
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
				const char* name,
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
				const char* name,
				float2 position,
				float2 scale,
				float* value_to_modify,
				float lower_bound,
				float upper_bound,
				float default_value = 0.0f,
				unsigned int precision = 2
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
				const char* name,
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
				const char* name,
				Stream<const char*> labels,
				unsigned int label_display_count,
				unsigned char* active_label
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextElement* CheckBoxInitializer(size_t configuration, const UIDrawConfig& config, const char* name);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void CheckBoxDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* element, bool* value_to_modify, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void CheckBoxDrawer(size_t configuration, const UIDrawConfig& config, const char* name, bool* value_to_modify, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ComboBoxDrawer(size_t configuration, UIDrawConfig& config, UIDrawerComboBox* data, unsigned char* active_label, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerColorInput* ColorInputInitializer(
				size_t configuration,
				UIDrawConfig& config,
				const char* name,
				Color* color,
				Color default_color,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ColorInputDrawer(size_t configuration, UIDrawConfig& config, UIDrawerColorInput* data, float2 position, float2 scale, Color* color);

			UIDrawerCollapsingHeader* CollapsingHeaderInitializer(size_t configuration, const UIDrawConfig& config, const char* name, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void CollapsingHeaderDrawer(size_t configuration, UIDrawConfig& config, UIDrawerCollapsingHeader* data, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void CollapsingHeaderDrawer(size_t configuration, UIDrawConfig& config, const char* name, bool* state, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerHierarchy* HierarchyInitializer(size_t configuration, const UIDrawConfig& config, const char* name);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename InitialValueInitializer, typename CallbackHover>
			UIDrawerTextInput* NumberInputInitializer(
				size_t configuration,
				UIDrawConfig& config, 
				const char* name, 
				Action callback_action, 
				void* callback_data,
				size_t callback_data_size,
				float2 position, 
				float2 scale, 
				InitialValueInitializer&& initial_value_init,
				CallbackHover&& callback_hover
			) {
				if (~configuration & UI_CONFIG_NUMBER_INPUT_DO_NOT_REDUCE_SCALE) {
					scale.x *= 0.75f;
				}

				// Begin recording allocations and table resources for dynamic resources
				if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
					BeginElement();
				}
				// kinda unnecessary memcpy's here but should not cost too much
				char full_name[256];
				const char* identifier = HandleResourceIdentifier(name);
				size_t identifier_size = strlen(identifier);
				memcpy(full_name, identifier, identifier_size);
				full_name[identifier_size] = '\0';
				strcat(full_name, "stream");

				ECS_ASSERT(identifier_size + std::size("stream") < 256);
				CapacityStream<char>* stream = GetMainAllocatorBufferAndStoreAsResource<CapacityStream<char>>(full_name);
				stream->InitializeFromBuffer(GetMainAllocatorBuffer(sizeof(char) * 64, alignof(char)), 0, 63);
				initial_value_init(stream);

				// implement callback - if it already has one, catch it and wrapp it
				if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
					UIConfigTextInputCallback* user_callback = (UIConfigTextInputCallback*)config.GetParameter(UI_CONFIG_TEXT_INPUT_CALLBACK);
					// Coallesce the allocation
					void* allocation = GetMainAllocatorBuffer(callback_data_size + user_callback->handler.data_size);
					memcpy(allocation, callback_data, callback_data_size);
					UIDrawerNumberInputCallbackData* base_data = (UIDrawerNumberInputCallbackData*)allocation;
					base_data->user_action = user_callback->handler.action;
					
					if (user_callback->handler.data_size > 0) {
						void* user_data = function::OffsetPointer(base_data, sizeof(*base_data));
						memcpy(user_data, user_callback->handler.data, user_callback->handler.data_size);
						base_data->user_action_data = user_data;
					}
					else {
						base_data->user_action_data = user_callback->handler.data;
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
				// Type pun all types - they all have as the third data member the text input followed by the bool to indicate
				// whether or not return to default is allowed
				UIDrawerNumberInputCallbackData* callback_input_ptr = (UIDrawerNumberInputCallbackData*)input->callback_data;
				callback_input_ptr->input = input;
				callback_input_ptr->return_to_default = true;
				callback_input_ptr->display_range = true;
				if (~configuration & UI_CONFIG_NUMBER_INPUT_DEFAULT) {
					callback_input_ptr->return_to_default = false;
				}
				if (configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
					callback_input_ptr->display_range = false;
				}
				if (~configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
					config.flag_count--;
				}

				full_name[identifier_size] = '\0';
				strcat(full_name, "tool tip");
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
				const char* name, 
				Action hoverable_action,
				Action wrapper_hoverable_action,
				Action draggable_action,
				void* draggable_data,
				unsigned int draggable_data_size,
				float2 position,
				float2 scale, 
				Lambda&& lambda
			) {
				if (~configuration & UI_CONFIG_NUMBER_INPUT_DO_NOT_REDUCE_SCALE) {
					scale.x *= 0.75f;
				}

				constexpr float DRAG_X_THRESHOLD = 0.0175f;
				UIDrawerTextInput* input = (UIDrawerTextInput*)GetResource(name);

				// Only update the tool tip if it is visible in the y dimension
				bool is_valid_y = ValidatePositionY(configuration, position, scale);
				const char* tool_tip_characters = nullptr;
				if (is_valid_y) {
					Stream<char> tool_tip_stream;
					const char* identifier = HandleResourceIdentifier(name);
					size_t identifier_size = strlen(identifier);
					char tool_tip_name[128];
					memcpy(tool_tip_name, identifier, identifier_size);
					tool_tip_name[identifier_size] = '\0';
					strcat(tool_tip_name, "tool tip");

					ECS_ASSERT(identifier_size + _countof("tool tip") < 128);
					tool_tip_characters = (char*)FindWindowResource(tool_tip_name);
					tool_tip_stream = Stream<char>(tool_tip_characters, 0);

					lambda(input, tool_tip_stream);

					size_t text_count_before = *HandleTextSpriteCount(configuration);
					TextInputDrawer(configuration, config, input, position, scale, UIDrawerTextInputFilterNumbers);

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

						AddHoverable(text_position, text_span, { hoverable_action, &hoverable_data, sizeof(hoverable_data), UIDrawPhase::System });

						if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_DRAG_VALUE) {
							UIDrawerNumberInputCallbackData* base_data = (UIDrawerNumberInputCallbackData*)draggable_data;
							base_data->input = input;
							AddClickable(text_position, text_span, { draggable_action, draggable_data, draggable_data_size });
						}
					}
				}
				else {
					FinalizeRectangle(configuration, position, scale);
				}
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextInput* FloatInputInitializer(
				size_t configuration,
				UIDrawConfig& config,
				const char* name,
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
				const char* name,
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
				const char* name,
				Integer* number,
				Integer default_value,
				Integer min,
				Integer max,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatInputDrawer(size_t configuration, const UIDrawConfig& config, const char* name, float* number, float min, float max, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleInputDrawer(size_t configuration, const UIDrawConfig& config, const char* name, double* number, double min, double max, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntInputDrawer(size_t configuration, const UIDrawConfig& config, const char* name, Integer* number, Integer min, Integer max, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatInputGroupDrawer(
				size_t configuration,
				UIDrawConfig& config,
				const char* group_name,
				size_t count,
				const char** names,
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
				const char* group_name,
				size_t count,
				const char** names,
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
				const char* group_name,
				size_t count,
				const char** names,
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
				const char* group_name,
				size_t count,
				const char** names,
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
				const char* group_name,
				size_t count,
				const char** names,
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
				const char* group_name,
				size_t count,
				const char** names,
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

			void HistogramDrawer(size_t configuration, const UIDrawConfig& config, const Stream<float> samples, const char* name, float2 position, float2 scale, unsigned int precision = 2);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerList* ListInitializer(size_t configuration, const UIDrawConfig& config, const char* name);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ListDrawer(size_t configuration, UIDrawConfig& config, UIDrawerList* data, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void MenuDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerMenu* data, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t MenuCalculateStateMemory(const UIDrawerMenuState* state, bool copy_states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t MenuWalkStatesMemory(const UIDrawerMenuState* state, bool copy_states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void MenuSetStateBuffers(
				UIDrawerMenuState* state,
				uintptr_t& buffer,
				CapacityStream<UIDrawerMenuWindow>* stream,
				bool copy_states
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void MenuWalkSetStateBuffers(
				UIDrawerMenuState* state,
				uintptr_t& buffer,
				CapacityStream<UIDrawerMenuWindow>* stream,
				bool copy_states
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerMenu* MenuInitializer(size_t configuration, const UIDrawConfig& config, const char* name, float2 scale, UIDrawerMenuState* menu_state);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void MultiGraphDrawer(
				size_t configuration,
				const UIDrawConfig& config,
				const char* name,
				Stream<float> samples,
				size_t sample_count,
				const Color* colors,
				float2 position,
				float2 scale,
				size_t x_axis_precision,
				size_t y_axis_precision
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void* SentenceInitializer(size_t configuration, const UIDrawConfig& config, const char* text);

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t SentenceWhitespaceCharactersCount(const char* identifier, CapacityStream<unsigned int> stack_buffer);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SentenceNonCachedInitializerKernel(const char* identifier, UIDrawerSentenceNotCached* data);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SentenceDrawer(size_t configuration, const UIDrawConfig& config, const char* identifier, void* resource, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerTextTable* TextTableInitializer(size_t configuration, const UIDrawConfig& config, const char* name, unsigned int rows, unsigned int columns, const char** labels);

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
				const char**& labels
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
				const char* name,
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

			float2 TextLabelWithSize(size_t configuration, const UIDrawConfig& config, const char* text, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FixedScaleTextLabel(size_t configuration, const UIDrawConfig& config, const char* text, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// this will add automatically VerticalSlider and HorizontalSlider to name
			void ViewportRegionSliderInitializer(
				float2* region_offset,
				const char* name,
				void** horizontal_slider,
				void** vertical_slider
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ViewportRegionSliders(
				float2 render_span,
				float2 render_zone,
				float2* region_offset,
				float2 horizontal_slider_position,
				float2 horizontal_slider_scale,
				float2 vertical_slider_position,
				float2 vertical_slider_scale,
				void* horizontal_slider,
				void* vertical_slider
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ViewportRegionSliders(
				const char* horizontal_slider_name,
				const char* vertical_slider_name,
				float2 render_span,
				float2 render_zone,
				float2* region_offset,
				float2 horizontal_slider_position,
				float2 horizontal_slider_scale,
				float2 vertical_slider_position,
				float2 vertical_slider_scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SpriteRectangle(
				size_t configuration,
				float2 position,
				float2 scale,
				const wchar_t* texture,
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
				const wchar_t* texture,
				const Color* colors,
				const float2* uvs,
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				float2 position,
				float2 scale,
				const wchar_t* texture,
				const Color* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				float2 position,
				float2 scale,
				const wchar_t* texture,
				const ColorFloat* colors,
				const float2* uvs,
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				float2 position,
				float2 scale,
				const wchar_t* texture,
				const ColorFloat* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				UIDrawPhase phase = UIDrawPhase::Normal
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

			float2 TextSpan(Stream<char> characters, float2 font_size, float character_spacing);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 TextSpan(const char* characters, float2 font_size, float character_spacing);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 TextSpan(Stream<char> characters);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 TextSpan(const char* characters);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetLabelScale(Stream<char> characters);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetLabelScale(const char* characters);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetLabelScale(Stream<char> characters, float2 font_size, float character_spacing);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetLabelScale(const char* characters, float2 font_size, float character_spacing);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void GetTextLabelAlignment(size_t configuration, const UIDrawConfig& config, TextAlignment& horizontal, TextAlignment& vertical);

			// ------------------------------------------------------------------------------------------------------------------------------------

			float GetXScaleUntilBorder(float position) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float GetNextRowXPosition() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t GetRandomIntIdentifier(size_t index);

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

			void ElementName(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ElementName(size_t configuration, const UIDrawConfig& config, const char* text, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool ExistsResource(const char* name);

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
			void AddWindowResource(const char* label, Initializer&& init) {
				const char* label_identifier = HandleResourceIdentifier(label, true);
				ResourceIdentifier identifier(label_identifier, strlen(label_identifier));
				void* data = init(label_identifier);
				AddWindowResourceToTable(data, identifier);
			}

			void AddWindowResourceToTable(void* resource, ResourceIdentifier identifier);
			
			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddHoverable(float2 position, float2 scale, UIActionHandler handler);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddTextTooltipHoverable(float2 position, float2 scale, UITextTooltipHoverableData* data, UIDrawPhase phase = UIDrawPhase::System);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddClickable(
				float2 position,
				float2 scale,
				UIActionHandler handler
			);

			// ------------------------------------------------------------------------------------------------------------------------------------
			
			void AddGeneral(
				float2 position,
				float2 scale,
				UIActionHandler handler
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultHoverable(
				float2 position,
				float2 scale,
				Color color,
				float percentage = 1.25f,
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultHoverable(
				float2 main_position,
				float2 main_scale,
				const float2* positions,
				const float2* scales,
				const Color* colors,
				const float* percentages,
				unsigned int count,
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultClickable(
				float2 position,
				float2 scale,
				UIActionHandler hoverable_handler,
				UIActionHandler clickable_handler
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDoubleClickAction(
				float2 position,
				float2 scale,
				unsigned int identifier,
				size_t duration_between_clicks,
				UIActionHandler first_click_handler,
				UIActionHandler second_click_handler
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultClickableHoverableWithText(
				float2 position,
				float2 scale,
				UIActionHandler handler,
				Color color,
				const char* text,
				float2 text_offset,
				UIDrawPhase phase = UIDrawPhase::Late,
				Color font_color = ECS_COLOR_WHITE,
				float percentage = 1.25f
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Either horizontal or vertical cull should be set
			void AddDefaultClickableHoverableWithTextEx(
				float2 position,
				float2 scale,
				UIActionHandler handler,
				Color color,
				const char* text,
				float2 text_offset,
				bool horizontal_cull = false,
				float horizontal_cull_bound = 0.0f,
				bool vertical_cull = false,
				float vertical_cull_bound = 0.0f,
				bool vertical_text = false,
				UIDrawPhase phase = UIDrawPhase::Late,
				Color font_color = ECS_COLOR_WHITE,
				float percentage = 1.25f
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultClickableHoverable(
				float2 position,
				float2 scale,
				UIActionHandler handler,
				Color color = ECS_COLOR_WHITE,
				float percentage = 1.25f
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void AddDefaultClickableHoverable(
				float2 main_position,
				float2 main_scale,
				const float2* positions,
				const float2* scales,
				const Color* colors,
				const float* percentages,
				unsigned int count,
				UIActionHandler handler
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
			using UIDrawerArrayDrawFunction = void (*)(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData data);

			// Optional handler for when an element has been dragged to a new position
			// The new order points indicates elements in the new order
			using UIDrawerArrayDragFunction = void (*)(UIDrawer& drawer, void* elements, unsigned int element_count, unsigned int* new_order, void* additional_data);

			// The collapsing header will go to the next row after every element - no need to call NextRow() inside draw_function
			// When changing the values, it will simply do a memcpy between elements; the draw function must cope with that
			template<typename Element>
			void Array(
				const char* name, 
				CapacityStream<Element>* elements,
				void* additional_data, 
				UIDrawerArrayDrawFunction draw_function,
				UIDrawerArrayDragFunction drag_function = nullptr
			) {
				UIDrawConfig config;
				Array(0, config, name, elements, additional_data, draw_function, drag_function);
			}

			// The collapsing header will go to the next row after every element - no need to call NextRow() inside draw_function
			// When changing the values, it will simply do a memcpy between elements; the draw function must cope with that
			template<typename Element>
			void Array(
				size_t configuration,
				const UIDrawConfig& config, 
				const char* name, 
				CapacityStream<Element>* elements,
				void* additional_data,
				UIDrawerArrayDrawFunction draw_function,
				UIDrawerArrayDragFunction drag_function = nullptr
			) {
				float2 position; 
				float2 scale; 
				HandleTransformFlags(configuration, config, position, scale);

				name = HandleResourceIdentifier(name);
				ECS_TEMP_ASCII_STRING(data_name, 256);
				data_name.Copy(ToStream(name));
				data_name.AddStream(ToStream(" data"));
				data_name.AddSafe('\0');

				if (!initializer) {				
					if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerArrayData* data = (UIDrawerArrayData*)GetResource(data_name.buffer);

						ArrayDrawer(
							configuration | UI_CONFIG_DO_NOT_VALIDATE_POSITION,
							config, 
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
							system->IncrementWindowDynamicResource(window_index, name);
						}
					}
					else {
						// Must check for the name manually
						bool exists = system->ExistsWindowResource(window_index, data_name.buffer);
						if (!exists) {
							UIDrawerInitializeArrayElementData initialize_data;
							initialize_data.config = (UIDrawConfig*)&config;
							initialize_data.name = name;
							initialize_data.element_count = elements->size;
							InitializeDrawerElement(*this, &initialize_data, name, InitializeArrayElement, DynamicConfiguration(configuration));
						}
						Array<Element>(DynamicConfiguration(configuration), config, name, elements, additional_data, draw_function, drag_function);
					}
				}
				else {
					if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						ArrayInitializer(configuration, config, name, elements->size);
					}
				}
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Element>
			void ArrayDrawer(
				size_t configuration,
				const UIDrawConfig& config,
				UIDrawerArrayData* data, 
				const char* name, 
				CapacityStream<Element>* elements,
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

				UIConfigCollapsingHeaderDoNotCache do_not_cache;
				do_not_cache.state = &data->collapsing_header_state;
				header_config.AddFlag(do_not_cache);

				// Copy all the other configurations except the transform ones
				for (size_t index = 0; index < config.flag_count; index++) {
					if (config.associated_bits[index] != UI_CONFIG_ABSOLUTE_TRANSFORM && config.associated_bits[index] != UI_CONFIG_RELATIVE_TRANSFORM
						&& config.associated_bits[index] != UI_CONFIG_WINDOW_DEPENDENT_SIZE && config.associated_bits[index] != UI_CONFIG_MAKE_SQUARE) {
						size_t header_config_count = header_config.flag_count;
						size_t config_byte_size = config.parameter_start[index + 1] - config.parameter_start[index];
						header_config.associated_bits[header_config_count] = config.associated_bits[index];
						header_config.parameter_start[header_config_count + 1] = header_config.parameter_start[header_config_count]
							+ config_byte_size;
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

				size_t NEW_CONFIGURATION = UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_CACHE |
					NullifyConfiguration(NullifyConfiguration(NullifyConfiguration(configuration, UI_CONFIG_RELATIVE_TRANSFORM), UI_CONFIG_WINDOW_DEPENDENT_SIZE),
						UI_CONFIG_MAKE_SQUARE);

				// Draw the collapsing header - nullified configurations - relative transform, window dependent size, make square
				CollapsingHeader(NEW_CONFIGURATION | UI_CONFIG_COLLAPSING_HEADER_DO_NOT_INFER | UI_CONFIG_GET_TRANSFORM, header_config, name, [&]() {
					ECS_TEMP_ASCII_STRING(temp_name, 64);
					temp_name.Copy(ToStream("Element "));
					size_t base_name_size = temp_name.size;

					// Set the draw mode to indent if it is different
					UIDrawerMode old_draw_mode = draw_mode;
					unsigned short old_draw_count = draw_mode_count;
					unsigned short old_draw_target = draw_mode_target;
					SetDrawMode(UIDrawerMode::Indent);

					// Push onto the identifier stack the field name
					bool has_pushed_stack = PushIdentifierStackStringPattern();
					PushIdentifierStack(name);

					UIDrawerArrayDragData drag_data;
					drag_data.array_data = data;

					UIDrawConfig arrow_config;
					UIConfigAbsoluteTransform arrow_transform;
					arrow_transform.scale = ARROW_SIZE;

					const char* draw_element_name = temp_name.buffer;

					auto select_name = [&](unsigned int index, CapacityStream<char> temporary_provided_name_string) {
						if (configuration & UI_CONFIG_ARRAY_PROVIDE_NAMES) {
							const UIConfigArrayProvideNames* provided_names = (const UIConfigArrayProvideNames*)config.GetParameter(UI_CONFIG_ARRAY_PROVIDE_NAMES);
							// Check each option
							if (provided_names->char_names.buffer != nullptr) {
								if (provided_names->char_names.size >= index) {
									draw_element_name = provided_names->char_names[index];
								}
								else {
									draw_element_name = temp_name.buffer;
								}
							}
							else if (provided_names->stream_names.buffer != nullptr) {
								if (provided_names->stream_names.size >= index) {
									temporary_provided_name_string.Copy(provided_names->stream_names[index]);
									temporary_provided_name_string.AddSafe('\0');
								}
								else {
									draw_element_name = temp_name.buffer;
								}
							}
							else if (provided_names->select_name_action != nullptr) {
								if (provided_names->select_name_action_capacity >= index) {
									UIConfigArrayProvideNameActionData config_action_data = { (char**)&draw_element_name, index };
									ActionData action_data = GetDummyActionData();
									action_data.data = provided_names->select_name_action_data;
									action_data.additional_data = &config_action_data;
									draw_element_name = temporary_provided_name_string.buffer;
									provided_names->select_name_action(&action_data);
								}
								else {
									draw_element_name = temp_name.buffer;
								}
							}
						}
						else {
							draw_element_name = temp_name.buffer;
						}
					};

					auto draw_row = [&](unsigned int index) {
						arrow_transform.position.x = current_x;
						// Indent the arrow position in order to align to row Y
						Indent();
						OffsetX(ARROW_SIZE.x);

						ECS_TEMP_ASCII_STRING(provided_name_string, 256);

						PushIdentifierStackRandom(index);
						function::ConvertIntToChars(temp_name, index);
						
						select_name(index, provided_name_string);

						UIDrawerArrayDrawData function_data;
						function_data.additional_data = additional_data;
						function_data.current_index = index;
						function_data.element_data = elements->buffer + index;

						draw_function(*this, draw_element_name, function_data);

						// Get the row y scale
						float row_y_scale = current_row_y_scale;
						data->row_y_scale = row_y_scale;

						arrow_transform.position.y = AlignMiddle(current_y, row_y_scale, ARROW_SIZE.y);
						arrow_config.AddFlag(arrow_transform);
						SpriteRectangle(
							UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_FIT_SPACE,
							arrow_config,
							ECS_TOOLS_UI_TEXTURE_ARROW,
							color_theme.default_text
						);
						arrow_config.flag_count--;

						drag_data.index = index;
						drag_data.row_y_scale = row_y_scale;
						float2 action_position = arrow_transform.position - region_render_offset;
						float2 action_scale;
						action_position = ExpandRectangle(action_position, arrow_transform.scale, { 1.25f, 1.25f }, action_scale);
						AddClickable(action_position, action_scale, { ArrayDragAction, &drag_data, sizeof(drag_data) });
						AddDefaultHoverable(action_position, action_scale, color_theme.theme);

						NextRow();
						temp_name.size = base_name_size;
						PopIdentifierStack();
					};

					auto draw_row_no_action = [&](unsigned int index) {
						arrow_transform.position.x = current_x;
						// Indent the arrow position in order to align to row Y
						Indent();
						OffsetX(ARROW_SIZE.x);

						ECS_TEMP_ASCII_STRING(provided_name_string, 256);

						select_name(index, provided_name_string);

						PushIdentifierStackRandom(index);
						function::ConvertIntToChars(temp_name, index);

						UIDrawerArrayDrawData function_data;
						function_data.additional_data = additional_data;
						function_data.current_index = index;
						function_data.element_data = elements->buffer + index;

						draw_function(*this, draw_element_name, function_data);

						// Get the row y scale
						float row_y_scale = current_row_y_scale;
						data->row_y_scale = row_y_scale;

						arrow_transform.position.y = AlignMiddle(current_y, row_y_scale, ARROW_SIZE.y);
						arrow_config.AddFlag(arrow_transform);
						SpriteRectangle(
							UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_FIT_SPACE,
							arrow_config,
							ECS_TOOLS_UI_TEXTURE_ARROW,
							color_theme.default_text
						);
						arrow_config.flag_count--;

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
								float2 update_position = GetCurrentPosition();
								float2 update_scale = { 0.0f, skip_count * (data->row_y_scale + layout.next_row_y_offset) - layout.next_row_y_offset };
								UpdateRenderBoundsRectangle(update_position, update_scale);
								UpdateCurrentScale(update_position, update_scale);
								NextRow();
							}
							index = skip_count;
						}
					}

					if (data->drag_index == -1) {
						for (; index < elements->size && current_y - region_render_offset.y - data->row_y_scale <= region_limit.y; index++) {
							draw_row(index);
						}
					}
					// There is currently a drag active - no other drag actions should be added
					// And the draw should be effective 
					else {
						if (data->drag_is_released) {
							// Make a buffer that will point for each element which is the new element that
							// will replace it
							unsigned int* _order_allocation = nullptr;
							_order_allocation = (unsigned int*)GetTempBuffer(sizeof(unsigned int) * elements->size);
							Stream<unsigned int> order_allocation(_order_allocation, 0);
							unsigned int skip_count_start = skip_count + (data->drag_index < skip_count);
							for (unsigned int counter = 0; counter < skip_count_start; counter++) {
								counter += data->drag_index == counter;
								if (counter < skip_count_start) {
									order_allocation.Add(counter);
								}
							}

							index = skip_count_start;
							position.y = current_y - region_render_offset.y;
							unsigned int drawn_elements = 0;
							while (position.y + data->row_y_scale < data->drag_current_position && drawn_elements < elements->size - 1 - skip_count) {
								index += data->drag_index == index;
								order_allocation.Add(index);
								draw_row_no_action(index);
								drawn_elements++;
								index++;
								position.y = current_y - region_render_offset.y;
							}

							// Reached the dragged element
							order_allocation.Add(data->drag_index);

							// Draw a small highlight
							float x_highlight_scale = GetXScaleUntilBorder(current_x - region_render_offset.x);
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
						
							draw_row_no_action(data->drag_index);
							unsigned int last_element = function::Select(data->drag_index == elements->size - 1, elements->size - 1, elements->size);
							for (; index < last_element && current_y - region_render_offset.y <= region_limit.y; index++) {
								// Skip the current drag index if it is before drawn
								index += data->drag_index == index;
								order_allocation.Add(index);
								draw_row_no_action(index);
							}

							for (unsigned int counter = index; counter < last_element; counter++) {
								// Skip the current drag index if it is before drawn
								counter += data->drag_index == counter;
								order_allocation.Add(counter);
							}
							index += data->drag_index == index;
							ECS_ASSERT(order_allocation.size == elements->size);

							// The default drag behaviour is to simply memcpy the new data
							if (drag_function == nullptr) {
								size_t temp_marker = GetTempAllocatorMarker();
								// Make a temporary allocation that will hold the new elements
								Element* temp_allocation = (Element*)GetTempBuffer(sizeof(Element) * elements->size);
								for (size_t index = 0; index < elements->size; index++) {
									temp_allocation[index] = elements->buffer[order_allocation[index]];
								}							

								memcpy(elements->buffer, temp_allocation, sizeof(Element) * elements->size);
								ReturnTempAllocator(temp_marker);
								
							}
							else {
								drag_function(*this, elements->buffer, elements->size, order_allocation.buffer, additional_data);
							}
							
							data->drag_is_released = false;
							data->drag_index = -1;
						}
						else {
							// The version without the oder_allocation - no adds or memory pressure onto the temp allocator
							unsigned int skip_count_start = skip_count + (data->drag_index < skip_count);

							index = skip_count_start;
							position.y = current_y - region_render_offset.y;
							unsigned int drawn_elements = 0;
							while (position.y + data->row_y_scale < data->drag_current_position && drawn_elements < elements->size - 1 - skip_count) {
								index += data->drag_index == index;
								draw_row_no_action(index);
								drawn_elements++;
								index++;
								position.y = current_y - region_render_offset.y;
							}
							// Reached the dragged element
							
							// Draw a small highlight
							float x_highlight_scale = GetXScaleUntilBorder(current_x - region_render_offset.x);
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

							draw_row_no_action(data->drag_index);
							unsigned int last_element = function::Select(data->drag_index == elements->size - 1, elements->size - 1, elements->size);
							for (; index < last_element && current_y - region_render_offset.y <= region_limit.y; index++) {
								// Skip the current drag index if it is before drawn
								index += data->drag_index == index;
								draw_row_no_action(index);
							}
							index += data->drag_index == index;
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

					struct AddRemoveData {
						CapacityStream<void>* data;
						UIDrawerArrayData* array_data;
					};

					auto add_action = [](ActionData* action_data) {
						UI_UNPACK_ACTION_DATA;

						AddRemoveData* data = (AddRemoveData*)_data;
						data->data->size += 1;
						if (data->array_data->add_callback != nullptr) {
							action_data->data = data->array_data->add_callback_data;
							data->array_data->add_callback(action_data);
						}
					};

					auto remove_action = [](ActionData* action_data) {
						UI_UNPACK_ACTION_DATA;

						AddRemoveData* data = (AddRemoveData*)_data;
						data->data->size -= 1;
						if (data->array_data->remove_callback != nullptr) {
							action_data->data = data->array_data->remove_callback_data;
							data->array_data->remove_callback(action_data);
						}
					};

					// The collapsing header had an absolute transform added, so remove it
					header_config.flag_count -= 2;

					float x_scale_till_border = GetXScaleUntilBorder(current_x) - layout.element_indentation;
					UIConfigWindowDependentSize button_size;
					button_size.type = WindowSizeTransformType::Horizontal;
					button_size.scale_factor = GetWindowSizeFactors(button_size.type, {x_scale_till_border * 0.5f, layout.default_element_y});
					header_config.AddFlag(button_size);

					float2 button_position;
					float2 button_scale;
					UIConfigGetTransform get_transform;
					get_transform.position = &button_position;
					get_transform.scale = &button_scale;
					header_config.AddFlag(get_transform);

					size_t COLOR_RECTANGLE_CONFIGURATION = NullifyConfiguration(NullifyConfiguration(configuration, UI_CONFIG_RELATIVE_TRANSFORM), UI_CONFIG_ABSOLUTE_TRANSFORM)
						| UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_GET_TRANSFORM;

					SolidColorRectangle(COLOR_RECTANGLE_CONFIGURATION, header_config, color_theme.theme);

					size_t BUTTON_CONFIGURATION = NullifyConfiguration(NullifyConfiguration(configuration, UI_CONFIG_RELATIVE_TRANSFORM), UI_CONFIG_WINDOW_DEPENDENT_SIZE) |
						UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE;

					float2 square_scale = GetSquareScale(layout.default_element_y);
					UIConfigAbsoluteTransform sprite_button_transform;
					sprite_button_transform.position = { AlignMiddle(button_position.x, button_scale.x, square_scale.x), AlignMiddle(button_position.y, button_scale.y, square_scale.y) };
					sprite_button_transform.scale = square_scale;
					sprite_button_transform.position += region_render_offset;
					header_config.AddFlag(sprite_button_transform);

					Color sprite_color = color_theme.default_text;
					
					AddRemoveData add_remove_data;
					add_remove_data.data = (CapacityStream<void>*)elements;
					add_remove_data.array_data = data;

					if (elements->size < elements->capacity) {
						AddDefaultClickableHoverable(
							button_position, 
							button_scale, 
							{ add_action, &add_remove_data, sizeof(add_remove_data), data->add_callback_phase },
							color_theme.theme
						);
					}
					else {
						sprite_color.alpha *= color_theme.alpha_inactive_item;
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
					
					if (elements->size > 0) {
						AddDefaultClickableHoverable(
							button_position,
							button_scale,
							{ remove_action, &add_remove_data, sizeof(add_remove_data), data->remove_callback_phase },
							color_theme.theme
						);
						sprite_color.alpha = 255;
					}
					else {
						sprite_color.alpha = color_theme.default_text.alpha * color_theme.alpha_inactive_item;

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
				});

				if (~configuration & UI_CONFIG_ARRAY_DISABLE_SIZE_INPUT) {
					ECS_TEMP_ASCII_STRING(temp_input_name, 256);
					temp_input_name.Copy(ToStream(name));
					temp_input_name.AddStream(ToStream("Size input"));
					temp_input_name.AddSafe('\0');

					// Eliminate the get transform
					header_config.flag_count--;
					// Draw the size with the capacity set
					if (header_config.associated_bits[header_config.flag_count - 1] == UI_CONFIG_ABSOLUTE_TRANSFORM) {
						header_config.flag_count--;
					}
					transform.position.x = header_position.x + header_scale.x + layout.element_indentation;
					transform.scale.x = SIZE_INPUT_X_SCALE * zoom_ptr->x;
					header_config.AddFlag(transform);

					if (~configuration & UI_CONFIG_ARRAY_FIXED_SIZE) {
						IntInput(
							NEW_CONFIGURATION | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_NUMBER_INPUT_NO_DRAG_VALUE,
							header_config,
							temp_input_name.buffer,
							&elements->size,
							(unsigned int)0,
							(unsigned int)0,
							elements->capacity
						);
					}
					else {
						UIConfigActiveState active_state;
						active_state.state = false;
						header_config.AddFlag(active_state);
						IntInput(
							NEW_CONFIGURATION | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_ACTIVE_STATE,
							header_config,
							temp_input_name.buffer,
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

			UIDrawerArrayData* ArrayInitializer(size_t configuration, const UIDrawConfig& config, const char* name, size_t element_count);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Array Instantiations - 1 component

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayFloat(const char* name, CapacityStream<float>* values);

			void ArrayFloat(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<float>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayDouble(const char* name, CapacityStream<double>* values);

			void ArrayDouble(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<double>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void ArrayInteger(const char* name, CapacityStream<Integer>* values);

			template<typename Integer>
			ECSENGINE_API void ArrayInteger(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<Integer>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Instantiations - 2 components

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayFloat2(const char* name, CapacityStream<float2>* values);

			void ArrayFloat2(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<float2>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayDouble2(const char* name, CapacityStream<double2>* values);

			void ArrayDouble2(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<double2>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename BaseInteger, typename Integer2>
			ECSENGINE_API void ArrayInteger2(const char* name, CapacityStream<Integer2>* values);

			template<typename BaseInteger, typename Integer2>
			ECSENGINE_API void ArrayInteger2(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<Integer2>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// it will infer the extended type
			template<typename BaseInteger>
			ECSENGINE_API void ArrayInteger2Infer(const char* name, CapacityStream<void>* values);

			// it will infer the extended type
			template<typename BaseInteger>
			ECSENGINE_API void ArrayInteger2Infer(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<void>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Instantiations - 3 components

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayFloat3(const char* name, CapacityStream<float3>* values);

			void ArrayFloat3(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<float3>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayDouble3(const char* name, CapacityStream<double3>* values);

			void ArrayDouble3(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<double3>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename BaseInteger, typename Integer3>
			ECSENGINE_API void ArrayInteger3(const char* name, CapacityStream<Integer3>* values);

			template<typename BaseInteger, typename Integer3>
			ECSENGINE_API void ArrayInteger3(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<Integer3>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// it will infer the extended type
			template<typename BaseInteger>
			ECSENGINE_API void ArrayInteger3Infer(const char* name, CapacityStream<void>* values);

			// it will infer the extended type
			template<typename BaseInteger>
			ECSENGINE_API void ArrayInteger3Infer(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<void>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Instantiations - 4 components

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayFloat4(const char* name, CapacityStream<float4>* values);

			void ArrayFloat4(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<float4>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayDouble4(const char* name, CapacityStream<double4>* values);

			void ArrayDouble4(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<double4>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename BaseInteger, typename Integer4>
			ECSENGINE_API void ArrayInteger4(const char* name, CapacityStream<Integer4>* values);

			template<typename BaseInteger, typename Integer4>
			ECSENGINE_API void ArrayInteger4(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<Integer4>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// it will infer the extended type
			template<typename BaseInteger>
			ECSENGINE_API void ArrayInteger4Infer(const char* name, CapacityStream<void>* values);

			// it will infer the extended type
			template<typename BaseInteger>
			ECSENGINE_API void ArrayInteger4Infer(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<void>* values);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Color

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayColor(const char* name, CapacityStream<Color>* colors);

			void ArrayColor(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<Color>* colors);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Color Float

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayColorFloat(const char* name, CapacityStream<ColorFloat>* colors);

			void ArrayColorFloat(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<ColorFloat>* colors);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion


#pragma region Array Check Boxes

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayCheckBox(const char* name, CapacityStream<bool>* states);
			
			void ArrayCheckBox(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<bool>* states);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Text Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayTextInput(const char* name, CapacityStream<CapacityStream<char>>* texts);

			void ArrayTextInput(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<CapacityStream<char>>* texts);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Combo Box

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ArrayComboBox(const char* name, CapacityStream<unsigned char>* flags, CapacityStream<Stream<const char*>> flag_labels);

			void ArrayComboBox(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<unsigned char>* flags, CapacityStream<Stream<const char*>> flag_labels);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

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
				const char* label,
				UIActionHandler handler
			);

			void Button(
				size_t configuration,
				const UIDrawConfig& config,
				const char* text,
				UIActionHandler handler
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void StateButton(const char* name, bool* state);

			void StateButton(size_t configuration, const UIDrawConfig& config, const char* name, bool* state);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SpriteStateButton(
				const wchar_t* texture,
				bool* state,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				float2 expand_factor = { 1.0f, 1.0f }
			);

			void SpriteStateButton(
				size_t configuration,
				const UIDrawConfig& config,
				const wchar_t* texture,
				bool* state,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				float2 expand_factor = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Stateless, label is done with UI_CONFIG_DO_NOT_CACHE; if drawing with a sprite
			// name can be made nullptr
			void MenuButton(const char* name, UIWindowDescriptor& window_descriptor, size_t border_flags = 0);

			// Stateless, label is done with UI_CONFIG_DO_NOT_CACHE; if drawing with a sprite
			// name can be made nullptr
			void MenuButton(size_t configuration, const UIDrawConfig& config, const char* name, UIWindowDescriptor& window_descriptor, size_t border_flags = 0);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ChangeThemeColor(Color new_theme_color);

#pragma region Checkbox

			// ------------------------------------------------------------------------------------------------------------------------------------

			void CheckBox(const char* name, bool* value_to_change);

			void CheckBox(size_t configuration, const UIDrawConfig& config, const char* name, bool* value_to_change);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion 

#pragma region Combo box

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ComboBox(const char* name, Stream<const char*> labels, unsigned int label_display_count, unsigned char* active_label);

			void ComboBox(size_t configuration, UIDrawConfig& config, const char* name, Stream<const char*> labels, unsigned int label_display_count, unsigned char* active_label);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ComboBoxDropDownDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerComboBox* data);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Collapsing Header

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Function>
			bool CollapsingHeader(const char* name, Function&& function) {
				UIDrawConfig config;
				return CollapsingHeader(0, config, name, function);
			}

			template<typename Function>
			bool CollapsingHeader(size_t configuration, UIDrawConfig& config, const char* name, Function&& function) {
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
					if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerCollapsingHeader* element = (UIDrawerCollapsingHeader*)GetResource(name);

						CollapsingHeaderDrawer(configuration, config, element, position, scale);
						HandleDynamicResource(configuration, name);
						if (element->state) {
							call_function();
						}
						return element->state;
					}
					else {
						UIConfigCollapsingHeaderDoNotCache* data = (UIConfigCollapsingHeaderDoNotCache*)config.GetParameter(UI_CONFIG_DO_NOT_CACHE);
						CollapsingHeaderDrawer(configuration, config, name, data->state, position, scale);
						HandleDynamicResource(configuration, name);

						if (*data->state) {
							call_function();
						}
						return *data->state;
					}
				}
				else {
					if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
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

			void ColorInput(const char* name, Color* color, Color default_color = ECS_COLOR_WHITE);

			void ColorInput(size_t configuration, UIDrawConfig& config, const char* name, Color* color, Color default_color = ECS_COLOR_WHITE);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Color Float Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ColorFloatInput(const char* name, ColorFloat* color, ColorFloat default_color = ECS_COLOR_WHITE);

			void ColorFloatInput(size_t configuration, UIDrawConfig& config, const char* name, ColorFloat* color, ColorFloat default_color = ECS_COLOR_WHITE);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ColorFloatInputIntensityInputName(char* intensity_input_name);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ColorFloatInputDrawer(size_t configuration, UIDrawConfig& config, const char* name, ColorFloat* color, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerColorFloatInput* ColorFloatInputInitializer(size_t configuration, UIDrawConfig& config, const char* name, ColorFloat* color, ColorFloat default_color, float2 position, float2 scale);

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

#pragma region Filter Menu

			// ------------------------------------------------------------------------------------------------------------------------------------

			// States should be a stack pointer with bool* to the members that need to be changed
			void FilterMenu(const char* name, Stream<const char*> label_names, bool** states);

			// States should be a stack pointer with bool* to the members that need to be changed
			void FilterMenu(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> label_names, bool** states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// States should be a stack pointer to a stable bool array
			void FilterMenu(const char* name, Stream<const char*> label_names, bool* states);

			// States should be a stack pointer to a stable bool array
			void FilterMenu(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> label_names, bool* states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FilterMenuDrawer(size_t configuration, const UIDrawConfig& config, const char* name, UIDrawerFilterMenuData* data);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerFilterMenuData* FilterMenuInitializer(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> label_names, bool** states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FilterMenuDrawer(size_t configuration, const UIDrawConfig& config, const char* name, UIDrawerFilterMenuSinglePointerData* data);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerFilterMenuSinglePointerData* FilterMenuInitializer(
				size_t configuration,
				const UIDrawConfig& config,
				const char* name,
				Stream<const char*> label_names,
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
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FinishFitTextToScale(size_t configuration, UIDrawConfig& config, const UIConfigTextParameters& previous_parameters);
			
			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Gradients

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Gradient(size_t configuration, const UIDrawConfig& config, const Color* colors, size_t horizontal_count, size_t vertical_count);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Graphs

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Graph(Stream<float2> samples, const char* name = nullptr, unsigned int x_axis_precision = 2, unsigned int y_axis_precision = 2);

			void Graph(
				size_t configuration,
				const UIDrawConfig& config,
				Stream<float2> unfiltered_samples,
				float filter_delta,
				const char* name = nullptr,
				unsigned int x_axis_precision = 2,
				unsigned int y_axis_precision = 2
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Graph(size_t configuration, const UIDrawConfig& config, Stream<float2> samples, const char* name = nullptr, unsigned int x_axis_precision = 2, unsigned int y_axis_precision = 2);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void MultiGraph(size_t configuration, const UIDrawConfig& config, Stream<float> samples, size_t sample_count, const Color* colors = MULTI_GRAPH_COLORS, const char* name = nullptr, unsigned int x_axis_precision = 2, unsigned int y_axis_precision = 2);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------
			
			const char* GetElementName(unsigned int index) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float GetDefaultBorderThickness() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetElementDefaultScale() const;

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

			float2 GetWindowSizeFactors(WindowSizeTransformType type, float2 scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetWindowSizeScaleElement(WindowSizeTransformType type, float2 scale_factors) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetWindowSizeScaleUntilBorder() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			void* GetTempBuffer(size_t size, size_t alignment = 8);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Can be used to release some temp memory - cannot be used when handlers are used
			void ReturnTempAllocator(size_t marker);

			// ------------------------------------------------------------------------------------------------------------------------------------

			size_t GetTempAllocatorMarker();

			// ------------------------------------------------------------------------------------------------------------------------------------

			UISystem* GetSystem();

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetSquareScale(float value) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			// It will use HandleResourceIdentifier to construct the string
			void* GetResource(const char* string);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// It will forward directly to the UI system; No HandleResourceIdentifier used
			void* FindWindowResource(const char* string);

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
			
			float2 GetAlignedToRight(float x_scale, float target_position = -5.0f) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetAlignedToRightOverLimit(float x_scale) const;

			// ------------------------------------------------------------------------------------------------------------------------------------

			float2 GetAlignedToBottom(float y_scale, float target_position = -5.0f) const;

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
			T* GetMainAllocatorBufferAndStoreAsResource(const char* name) {
				return (T*)GetMainAllocatorBufferAndStoreAsResource(name, sizeof(T), alignof(T));
			}

			// ------------------------------------------------------------------------------------------------------------------------------------

			void* GetMainAllocatorBufferAndStoreAsResource(const char* name, size_t size, size_t alignment = 8);

			// ------------------------------------------------------------------------------------------------------------------------------------

			unsigned int GetWindowIndex() const;

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Hierarchy

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerHierarchy* Hierarchy(const char* name);

			UIDrawerHierarchy* Hierarchy(size_t configuration, const UIDrawConfig& config, const char* name);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Histogram

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Histogram(Stream<float> samples, const char* name = nullptr);

			void Histogram(size_t configuration, const UIDrawConfig& config, Stream<float> samples, const char* name = nullptr);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void InitializeElementName(
				size_t configuration,
				size_t no_element_name,
				const UIDrawConfig& config,
				const char* name,
				UIDrawerTextElement* element,
				float2 position
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool HasClicked(float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			bool IsMouseInRectangle(float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Indent();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Indent(float count);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Label hierarchy

			// ------------------------------------------------------------------------------------------------------------------------------------

			void LabelHierarchy(const char* identifier, Stream<const char*> labels);

			// Parent index 0 means root
			UIDrawerLabelHierarchy* LabelHierarchy(size_t configuration, UIDrawConfig& config, const char* identifier, Stream<const char*> labels);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerLabelHierarchy* LabelHierarchyInitializer(size_t configuration, const UIDrawConfig& config, const char* _identifier);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void LabelHierarchyDrawer(
				size_t configuration,
				UIDrawConfig& config,
				UIDrawerLabelHierarchy* data,
				Stream<const char*> labels,
				float2 position,
				float2 scale
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region List

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerList* List(const char* name);

			UIDrawerList* List(size_t configuration, UIDrawConfig& config, const char* name);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void ListFinalizeNode(UIDrawerList* list);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Label List

			// ------------------------------------------------------------------------------------------------------------------------------------

			void LabelList(const char* name, Stream<const char*> labels);

			void LabelList(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void LabelListDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerLabelList* data, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void LabelListDrawer(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerLabelList* LabelListInitializer(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Menu

			// ------------------------------------------------------------------------------------------------------------------------------------

			// State can be stack allocated; name should be unique if drawing with a sprite
			void Menu(const char* name, UIDrawerMenuState* state);

			// State can be stack allocated; name should be unique if drawing with a sprite
			void Menu(size_t configuration, const UIDrawConfig& config, const char* name, UIDrawerMenuState* state);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void* MenuAllocateStateHandlers(Stream<UIActionHandler> handlers);

#pragma endregion

#pragma region Number input

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatInput(const char* name, float* value, float default_value = 0.0f, float lower_bound = -FLT_MAX, float upper_bound = FLT_MAX);

			void FloatInput(size_t configuration, UIDrawConfig& config, const char* name, float* value, float default_value = 0.0f, float lower_bound = -FLT_MAX, float upper_bound = FLT_MAX);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatInputGroup(
				size_t count,
				const char* group_name,
				const char** names,
				float** values,
				const float* default_values = nullptr,
				const float* lower_bound = nullptr,
				const float* upper_bound = nullptr
			);

			void FloatInputGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				const char* group_name,
				const char** names,
				float** values,
				const float* default_values = nullptr,
				const float* lower_bound = nullptr,
				const float* upper_bound = nullptr
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleInput(const char* name, double* value, double default_value = 0, double lower_bound = -DBL_MAX, double upper_bound = DBL_MAX);

			void DoubleInput(size_t configuration, UIDrawConfig& config, const char* name, double* value, double default_value = 0, double lower_bound = -DBL_MAX, double upper_bound = DBL_MAX);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleInputGroup(
				size_t count,
				const char* group_name,
				const char** names,
				double** values,
				const double* default_values = nullptr,
				const double* lower_bound = nullptr,
				const double* upper_bound = nullptr
			);

			void DoubleInputGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				const char* group_name,
				const char** names,
				double** values,
				const double* default_values = nullptr,
				const double* lower_bound = nullptr,
				const double* upper_bound = nullptr
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntInput(const char* name, Integer* value, Integer default_value = 0, Integer min = LLONG_MIN, Integer max = ULLONG_MAX);

			template<typename Integer>
			ECSENGINE_API void IntInput(size_t configuration, UIDrawConfig& config, const char* name, Integer* value, Integer default_value = 0, Integer min = LLONG_MIN, Integer max = ULLONG_MAX);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntInputGroup(
				size_t count,
				const char* group_name,
				const char** names,
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
				const char* group_name,
				const char** names,
				Integer** values,
				const Integer* default_values = nullptr,
				const Integer* lower_bound = nullptr,
				const Integer* upper_bound = nullptr
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void NextRow();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void NextRow(float count);

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

			void ProgressBar(const char* name, float percentage, float x_scale_factor = 1);

			void ProgressBar(size_t configuration, UIDrawConfig& config, const char* name, float percentage, float x_scale_factor = 1);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void PopIdentifierStack();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void PushIdentifierStack(const char* identifier);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void PushIdentifierStack(size_t index);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void PushIdentifierStackRandom(size_t index);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Returns whether or not it pushed onto the stack
			bool PushIdentifierStackStringPattern();

			// ------------------------------------------------------------------------------------------------------------------------------------

			void RemoveAllocation(const void* allocation);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void RestoreBufferState(size_t configuration, UIDrawerBufferState state);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void RestoreHandlerState(UIDrawerHandlerState state);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Sentence

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Sentence(const char* text);

			void Sentence(size_t configuration, const UIDrawConfig& config, const char* text);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetCurrentX(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetCurrentY(float value);

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
				const char* name,
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
				const char* name,
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
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
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
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				unsigned int byte_size,
				void** ECS_RESTRICT values_to_modify,
				const void* ECS_RESTRICT lower_bounds,
				const void* ECS_RESTRICT upper_bounds,
				const void* ECS_RESTRICT default_values,
				const UIDrawerSliderFunctions& functions,
				UIDrawerTextInputFilter filter
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatSlider(const char* name, float* value_to_modify, float lower_bound, float upper_bound, float default_value = 0.0f, unsigned int precision = 2);

			void FloatSlider(size_t configuration, UIDrawConfig& config, const char* name, float* value_to_modify, float lower_bound, float upper_bound, float default_value = 0.0f, unsigned int precision = 2);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void FloatSliderGroup(
				size_t count,
				const char* group_name,
				const char** names,
				float** values_to_modify,
				const float* lower_bounds,
				const float* upper_bounds,
				const float* default_values = nullptr,
				unsigned int precision = 2
			);

			void FloatSliderGroup(
				size_t configuration,
				UIDrawConfig& config,
				size_t count,
				const char* group_name,
				const char** names,
				float** values_to_modify,
				const float* lower_bounds,
				const float* upper_bounds,
				const float* default_values = nullptr,
				unsigned int precision = 2
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleSlider(const char* name, double* value_to_modify, double lower_bound, double upper_bound, double default_value = 0, unsigned int precision = 3);

			void DoubleSlider(size_t configuration, UIDrawConfig& config, const char* name, double* value_to_modify, double lower_bound, double upper_bound, double default_value = 0, unsigned int precision = 3);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DoubleSliderGroup(
				size_t count,
				const char* group_name,
				const char** names,
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
				const char* group_name,
				const char** names,
				double** values_to_modify,
				const double* lower_bounds,
				const double* upper_bounds,
				const double* default_values = nullptr,
				unsigned int precision = 3
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntSlider(const char* name, Integer* value_to_modify, Integer lower_bound, Integer upper_bound, Integer default_value = 0);

			template<typename Integer>
			ECSENGINE_API void IntSlider(size_t configuration, UIDrawConfig& config, const char* name, Integer* value_to_modify, Integer lower_bound, Integer upper_bound, Integer default_value = 0);

			// ------------------------------------------------------------------------------------------------------------------------------------

			template<typename Integer>
			ECSENGINE_API void IntSliderGroup(
				size_t count,
				const char* group_name,
				const char** names,
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
				const char* group_name,
				const char** names,
				Integer** values_to_modify,
				const Integer* lower_bounds,
				const Integer* upper_bounds,
				const Integer* default_values = nullptr
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetZoomXFactor(float zoom_x_factor);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetZoomYFactor(float zoom_y_factor);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetRowHorizontalOffset(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetRowPadding(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SetNextRowYOffset(float value);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// Currently, draw_mode_floats is needed for column draw, only the y component needs to be filled
			// with the desired y spacing
			void SetDrawMode(UIDrawerMode mode, unsigned int target = 0, float draw_mode_float = 0.0f);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// places a hoverable on the whole window
			void SetWindowHoverable(const UIActionHandler* handler);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// places a clickable on the whole window
			void SetWindowClickable(const UIActionHandler* handler);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// places a general on the whole window
			void SetWindowGeneral(const UIActionHandler* handler);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// configuration is needed for phase deduction
			void SetSpriteClusterTexture(size_t configuration, const wchar_t* texture, unsigned int count);

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
			void StateTable(const char* name, Stream<const char*> labels, bool** states);

			// States should be a stack pointer to a bool array
			void StateTable(const char* name, Stream<const char*> labels, bool* states);

			// States should be a stack pointer to bool* to the members that should be changed
			void StateTable(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels, bool** states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			// States should be a stack pointer to a bool array
			void StateTable(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels, bool* states);

			// ------------------------------------------------------------------------------------------------------------------------------------

			UIDrawerStateTable* StateTableInitializer(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void StateTableDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerStateTable* data, bool** states, float2 position, float2 scale);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Sprite Rectangle

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SpriteRectangle(const wchar_t* texture, Color color = ECS_COLOR_WHITE, float2 top_left_uv = { 0.0f, 0.0f }, float2 bottom_right_uv = { 1.0f, 1.0f });

			void SpriteRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				const wchar_t* texture,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void SpriteRectangleDouble(
				const wchar_t* texture0,
				const wchar_t* texture1,
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
				const wchar_t* texture0,
				const wchar_t* texture1,
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
				const wchar_t* texture,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			void SpriteButton(
				size_t configuration,
				const UIDrawConfig& config,
				UIActionHandler clickable,
				const wchar_t* texture,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Sprite Texture Bool

			void SpriteTextureBool(
				const wchar_t* texture_false,
				const wchar_t* texture_true,
				bool* state,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			void SpriteTextureBool(
				size_t configuration,
				const UIDrawConfig& config,
				const wchar_t* texture_false,
				const wchar_t* texture_true,
				bool* state,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			void SpriteTextureBool(
				const wchar_t* texture_false,
				const wchar_t* texture_true,
				size_t* flags,
				size_t flag_to_set,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

			void SpriteTextureBool(
				size_t configuration,
				const UIDrawConfig& config,
				const wchar_t* texture_false,
				const wchar_t* texture_true,
				size_t* flags,
				size_t flag_to_set,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			);

#pragma endregion

#pragma region Text Table

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextTable(const char* name, unsigned int rows, unsigned int columns, const char** labels);

			void TextTable(size_t configuration, const UIDrawConfig& config, const char* name, unsigned int rows, unsigned int columns, const char** labels);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Text Input

			// ------------------------------------------------------------------------------------------------------------------------------------

			// single lined text input
			UIDrawerTextInput* TextInput(const char* name, CapacityStream<char>* text_to_fill);

			// single lined text input
			UIDrawerTextInput* TextInput(size_t configuration, UIDrawConfig& config, const char* name, CapacityStream<char>* text_to_fill, UIDrawerTextInputFilter filter = UIDrawerTextInputFilterAll);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Text Label

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextLabel(const char* characters);

			void TextLabel(size_t configuration, const UIDrawConfig& config, const char* characters);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Text

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Text(const char* text);

			// non cached drawer
			void Text(size_t configuration, const UIDrawConfig& config, const char* characters, float2 position);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Text(size_t configuration, const UIDrawConfig& config, const char* text, float2& position, float2& text_span);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void Text(size_t configuration, const UIDrawConfig& config, const char* characters);

			// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

			// ------------------------------------------------------------------------------------------------------------------------------------

			void TextToolTip(const char* characters, float2 position, float2 scale, const UITooltipBaseData* base = nullptr);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DefaultHoverableWithToolTip(const char* characters, float2 position, float2 scale, const Color* color = nullptr, const float* percentage = nullptr, const UITooltipBaseData* base = nullptr);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void DefaultHoverableWithToolTip(float2 position, float2 scale, const UIDefaultHoverableWithTooltipData* data);

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
				const wchar_t* texture,
				const Color* colors,
				const float2* uvs,
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				const wchar_t* texture,
				const Color* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				const wchar_t* texture,
				const ColorFloat* colors,
				const float2* uvs,
				UIDrawPhase phase = UIDrawPhase::Normal
			);

			// ------------------------------------------------------------------------------------------------------------------------------------

			void VertexColorSpriteRectangle(
				size_t configuration,
				const UIDrawConfig& config,
				const wchar_t* texture,
				const ColorFloat* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				UIDrawPhase phase = UIDrawPhase::Normal
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
			UIDrawerMode draw_mode;
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
			CapacityStream<void*> last_initialized_element_allocations;
			CapacityStream<ResourceIdentifier> last_initialized_element_table_resources;
		};

		using UIDrawerInitializeFunction = void (*)(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

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
			const char* name,
			UIDrawerInitializeFunction initialize,
			size_t configuration
		);

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

		ECSENGINE_API void InitializeLabelHierarchyElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void InitializeListElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration);

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

		ECSENGINE_API void UIDrawerArrayFloatFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayDoubleFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		ECSENGINE_API void UIDrawerArrayIntegerFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayFloat2Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayDouble2Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		ECSENGINE_API void UIDrawerArrayInteger2Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------
		
		ECSENGINE_API void UIDrawerArrayFloat3Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayDouble3Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		ECSENGINE_API void UIDrawerArrayInteger3Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayFloat4Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayDouble4Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		ECSENGINE_API void UIDrawerArrayInteger4Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayColorFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayColorFloatFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayCheckBoxFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayTextInputFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void UIDrawerArrayComboBoxFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data);

		// --------------------------------------------------------------------------------------------------------------

		// --------------------------------------------------------------------------------------------------------------

	}

}