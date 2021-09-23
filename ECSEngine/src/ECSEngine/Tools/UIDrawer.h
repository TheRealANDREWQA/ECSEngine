#pragma once
#include "UISystem.h"
#include "UIDrawConfigs.h"
#include "UIDrawerStructures.h"
#include "UIDrawerActionsTemplates.h"
#include "UIResourcePaths.h"
#include "../Utilities/Path.h"

#define ECS_TOOLS_UI_DRAWER_FILL_ARGUMENTS thread_id, dockspace, border_index
#define ECS_TOOLS_UI_DRAWER_CURRENT_POSITION {current_x, current_y}
#define ECS_TOOLS_UI_DRAWER_DEFAULT_SIZE {layout.default_element_x, layout.default_element_y}
#define ECS_TOOLS_UI_DRAWER_DEFAULT_ARGUMENTS ECS_TOOLS_UI_DRAWER_FILL_ARGUMENTS, ECS_TOOLS_UI_DRAWER_CURRENT_POSITION, ECS_TOOLS_UI_DRAWER_DEFAULT_SIZE

#define ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config) float2 position; float2 scale; HandleTransformFlags<configuration>(config, position, scale)
#define ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION constexpr size_t configuration = 0; UIDrawConfig null_config

#define ECS_TOOLS_UI_DRAWER_IDENTITY_SCALE [](float2 scale) {return scale;}
#define ECS_TOOLS_UI_DRAWER_LABEL_SCALE [&](float2 scale){return float2(scale.x + 2.0f * element_descriptor.label_horizontal_padd, scale.y + 2.0f * element_descriptor.label_vertical_padd);}

#define UI_HIERARCHY_NODE_FUNCTION(initializer) ECSEngine::Tools::UIDrawer<initializer>* drawer = (ECSEngine::Tools::UIDrawer<initializer>*)drawer_ptr; \
												ECSEngine::Tools::UIDrawerHierarchy* hierarchy = (ECSEngine::Tools::UIDrawerHierarchy*)hierarchy_ptr

#define UI_LIST_NODE_FUNCTION(initializer)  ECSEngine::Tools::UIDrawer<initializer>* drawer = (ECSEngine::Tools::UIDrawer<initializer>*)drawer_ptr; \
											ECSEngine::Tools::UIDrawerList* list = (ECSEngine::Tools::UIDrawerList*)list_ptr; \
											list->InitializeNodeYScale(drawer->GetCounts())

namespace ECSEngine {

	namespace Tools {

		constexpr size_t slider_group_max_count = 8;
		constexpr float number_input_drag_factor = 200.0f;

		static constexpr size_t NullifyConfiguration(size_t configuration, size_t bit_flag) {
			return ~bit_flag & configuration;
		}

		static constexpr bool IsElementNameFirst(size_t configuration, size_t no_name_flag) {
			return ((configuration & UI_CONFIG_ELEMENT_NAME_FIRST) != 0) && ((configuration & no_name_flag) == 0);
		}

		static constexpr bool IsElementNameAfter(size_t configuration, size_t no_name_flag) {
			return ((configuration & UI_CONFIG_ELEMENT_NAME_FIRST) == 0) && ((configuration & no_name_flag) == 0);
		}

		static constexpr size_t DynamicConfiguration(size_t configuration) {
			return NullifyConfiguration(configuration, UI_CONFIG_DO_NOT_CACHE) | UI_CONFIG_DYNAMIC_RESOURCE;
		}

		template<size_t configuration, size_t bit_flag, typename Flag>
		void SetConfigParameter(UIDrawConfig& config, const Flag& flag, Flag& previous_flag) {
			if constexpr (configuration & bit_flag) {
				config.SetExistingFlag(flag, bit_flag, previous_flag);
			}
			else {
				config.AddFlag(flag);
			}
		}

		template<size_t configuration, size_t bit_flag, typename Flag>
		void RemoveConfigParameter(UIDrawConfig& config, const Flag& previous_flag) {
			if constexpr (configuration & bit_flag) {
				config.RestoreFlag(previous_flag, bit_flag);
			}
			else {
				config.flag_count--;
			}
		}

		template<bool initializer>
		class UIDrawer
		{
		public:	
			void CalculateRegionParameters(float2 position, float2 scale, const float2* render_offset) {
				region_position = position;
				region_scale = scale;

				region_limit = {
					region_position.x + region_scale.x - layout.next_row_padding - system->m_descriptors.misc.render_slider_vertical_size,
					region_position.y + region_scale.y - layout.next_row_y_offset - system->m_descriptors.misc.render_slider_horizontal_size
				};
				region_render_offset = *render_offset;
				max_region_render_limit.x = region_position.x + region_scale.x;
				max_region_render_limit.y = region_position.y + scale.y;

				min_region_render_limit.x = region_position.x;
				min_region_render_limit.y = region_position.y;

				max_render_bounds.x = -1000.0f;
				max_render_bounds.y = -1000.0f;
				min_render_bounds.x = 1000.0f;
				min_render_bounds.y = 1000.0f;

				region_fit_space_horizontal_offset = region_position.x + layout.next_row_padding;
				region_fit_space_vertical_offset = region_position.y + system->m_descriptors.misc.title_y_scale + layout.next_row_y_offset;
				if constexpr (!initializer) {
					bool substract = dockspace->borders[border_index].draw_elements == false;
					region_fit_space_vertical_offset -= system->m_descriptors.misc.title_y_scale * substract;
				}

				current_x = GetNextRowXPosition();
				current_y = region_fit_space_vertical_offset;
			}

			void DefaultDrawParameters() {
				draw_mode = UIDrawerMode::Indent;
				draw_mode_count = 0;
				draw_mode_target = 0;
				current_column_x_scale = 0.0f;
				current_row_y_scale = 0.0f;
				next_row_offset = 0.0f;
				stabilized_render_span = { 0.0f, 0.0f };

				zoom_inverse = { 1.0f / zoom_ptr->x, 1.0f / zoom_ptr->y };

				no_padding_for_render_sliders = false;
				no_padding_render_region = false;
				deallocate_identifier_buffers = true;
			}

			template<size_t configuration>
			void ConvertTextToWindowResource(const UIDrawConfig& config, const char* text, UIDrawerTextElement* element, float2 position) {
				size_t text_count = ParseStringIdentifier(text, strlen(text));
				Color color;
				float character_spacing;
				float2 font_size;
				HandleText<configuration>(config, color, font_size, character_spacing);

				TextAlignment horizontal_alignment = TextAlignment::Middle, vertical_alignment = TextAlignment::Middle;
				if constexpr (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
					GetTextLabelAlignment<configuration>(config, horizontal_alignment, vertical_alignment);
				}

				element->position = position;
				element->SetZoomFactor(*zoom_ptr);

				if (text_count > 0) {
					void* text_allocation = GetMainAllocatorBuffer(sizeof(UISpriteVertex) * text_count * 6, alignof(UISpriteVertex));

					CapacityStream<UISpriteVertex>* text_stream = element->TextStream();
					text_stream->buffer = (UISpriteVertex*)text_allocation;
					text_stream->size = text_count * 6;

					float2 text_span;
					if constexpr (configuration & UI_CONFIG_VERTICAL) {
						if constexpr (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
							if (vertical_alignment == TextAlignment::Bottom) {
								system->ConvertCharactersToTextSprites<false, true>(
									text,
									position,
									text_stream->buffer,
									text_count,
									color,
									0,
									font_size,
									character_spacing
									);
								text_span = GetTextSpan<false, true>(*text_stream);
							}
							else {
								system->ConvertCharactersToTextSprites<false, false>(
									text,
									position,
									text_stream->buffer,
									text_count,
									color,
									0,
									font_size,
									character_spacing
									);
								text_span = GetTextSpan<false, false>(*text_stream);
							}
						}
						else {
							system->ConvertCharactersToTextSprites<false, false>(
								text,
								position,
								text_stream->buffer,
								text_count,
								color,
								0,
								font_size,
								character_spacing
								);
							text_span = GetTextSpan<false, false>(*text_stream);
						}
						AlignVerticalText(*text_stream);
					}
					else {
						if constexpr (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
							if (horizontal_alignment == TextAlignment::Right) {
								system->ConvertCharactersToTextSprites<true, true>(
									text,
									position,
									text_stream->buffer,
									text_count,
									color,
									0,
									font_size,
									character_spacing
									);
								text_span = GetTextSpan<true, true>(*text_stream);
							}
							else {
								system->ConvertCharactersToTextSprites<true, false>(
									text,
									position,
									text_stream->buffer,
									text_count,
									color,
									0,
									font_size,
									character_spacing
									);
								text_span = GetTextSpan<true, false>(*text_stream);
							}
						}
						else {
							system->ConvertCharactersToTextSprites<true, false>(
								text,
								position,
								text_stream->buffer,
								text_count,
								color,
								0,
								font_size,
								character_spacing
								);
							text_span = GetTextSpan<true, false>(*text_stream);
						}
					}

					/*if (text_span.x == 0.0f) {
						__debugbreak();
						text_span = GetTextSpan<true, false>(*text_stream);
					}*/
					element->scale = text_span;
				}
				else {
					element->text_vertices.size = 0;
					element->scale = { 0.0f, 0.0f };
				}
			}

			void ExpandRectangleFromFlag(const float* resize_factor, float2& position, float2& scale) const {
				float2 temp_scale = scale;
				position = ExpandRectangle(position, temp_scale, { *resize_factor, *(resize_factor + 1) }, scale);
			}

			template<size_t configuration>
			bool ValidatePosition(float2 position) {
				if constexpr (~configuration & UI_CONFIG_DO_NOT_VALIDATE_POSITION) {
					return (position.x < max_region_render_limit.x) && (position.y < max_region_render_limit.y);
				}
				else {
					return true;
				}
			}

			template<size_t configuration>
			bool ValidatePosition(float2 position, float2 scale) {
				return ValidatePosition<configuration>(position) && ValidatePositionMinBound<configuration>(position, scale);
			}

			template<size_t configuration>
			bool ValidatePositionY(float2 position, float2 scale) {
				if constexpr (~configuration & UI_CONFIG_DO_NOT_VALIDATE_POSITION) {
					return (position.y < max_region_render_limit.y) && (position.y + scale.y >= min_region_render_limit.y);
				}
				else {
					return true;
				}
			}

			template<size_t configuration>
			bool ValidatePositionMinBound(float2 position, float2 scale) {
				if constexpr (~configuration & UI_CONFIG_DO_NOT_VALIDATE_POSITION) {
					return (position.x + scale.x >= min_region_render_limit.x) && (position.y + scale.y >= min_region_render_limit.y);
				}
				else {
					return true;
				}
			}

			template<size_t configuration>
			UIVertexColor* HandleSolidColorBuffer() {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					return (UIVertexColor*)buffers[ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SOLID_COLOR];
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					return (UIVertexColor*)system_buffers[ECS_TOOLS_UI_SOLID_COLOR];
				}
				else {
					return (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
				}
			}

			template<size_t configuration>
			size_t* HandleSolidColorCount() {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					return counts + ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SOLID_COLOR;
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					return system_counts + ECS_TOOLS_UI_SOLID_COLOR;
				}
				else {
					return counts + ECS_TOOLS_UI_SOLID_COLOR;
				}
			}

			template<size_t configuration>
			UISpriteVertex* HandleTextSpriteBuffer() {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					return (UISpriteVertex*)buffers[ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_TEXT_SPRITE];
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					return (UISpriteVertex*)system_buffers[ECS_TOOLS_UI_TEXT_SPRITE];
				}
				else {
					return (UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE];
				}
			}

			template<size_t configuration>
			size_t* HandleTextSpriteCount() {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					return counts + ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_TEXT_SPRITE;
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					return system_counts + ECS_TOOLS_UI_TEXT_SPRITE;
				}
				else {
					return counts + ECS_TOOLS_UI_TEXT_SPRITE;
				}
			}

			template<size_t configuration>
			UISpriteVertex* HandleSpriteBuffer() {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					return (UISpriteVertex*)buffers[ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SPRITE];
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					return (UISpriteVertex*)system_buffers[ECS_TOOLS_UI_SPRITE];
				}
				else {
					return (UISpriteVertex*)buffers[ECS_TOOLS_UI_SPRITE];
				}
			}

			template<size_t configuration>
			size_t* HandleSpriteCount() {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					return counts + ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SPRITE;
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					return system_counts + ECS_TOOLS_UI_SPRITE;
				}
				else {
					return counts + ECS_TOOLS_UI_SPRITE;
				}
			}

			template<size_t configuration>
			UISpriteVertex* HandleSpriteClusterBuffer() {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					return (UISpriteVertex*)buffers[ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SPRITE_CLUSTER];
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					return (UISpriteVertex*)system_buffers[ECS_TOOLS_UI_SPRITE_CLUSTER];
				}
				else {
					return (UISpriteVertex*)buffers[ECS_TOOLS_UI_SPRITE_CLUSTER];
				}
			}

			template<size_t configuration>
			size_t* HandleSpriteClusterCount() {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					return counts + ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SPRITE_CLUSTER;
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					return system_counts + ECS_TOOLS_UI_SPRITE_CLUSTER;
				}
				else {
					return counts + ECS_TOOLS_UI_SPRITE_CLUSTER;
				}
			}

			template<size_t configuration>
			UIVertexColor* HandleLineBuffer() {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					return (UIVertexColor*)buffers[ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_LINE];
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					return (UIVertexColor*)system_buffers[ECS_TOOLS_UI_LINE];
				}
				else {
					return (UIVertexColor*)buffers[ECS_TOOLS_UI_LINE];
				}
			}

			template<size_t configuration>
			size_t* HandleLineCount() {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					return counts + ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_LINE;
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					return system_counts + ECS_TOOLS_UI_LINE;
				}
				else {
					return counts + ECS_TOOLS_UI_LINE;
				}
			}

			template<size_t configuration>
			void Line(float2 positions1, float2 positions2, Color color) {
				auto buffer = HandleLineBuffer<configuration>();
				auto count = HandleLineCount<configuration>();
				SetLine(positions1, positions2, color, *count, buffer);
			}

			template<size_t configuration>
			void HandleLateAndSystemDrawActionNullify(float2 position, float2 scale) {
				if constexpr ((configuration & UI_CONFIG_LATE_DRAW) || (configuration & UI_CONFIG_SYSTEM_DRAW)) {
					unsigned char dummy = 0;
					if constexpr (~configuration & UI_CONFIG_DO_NOT_NULLIFY_HOVERABLE_ACTION) {
						AddHoverable(position, scale, { SkipAction, &dummy, 1  });
					}
					if constexpr (~configuration & UI_CONFIG_DO_NOT_NULLIFY_CLICKABLE_ACTION) {
						AddClickable(position, scale, { SkipAction, &dummy, 1 });
					}
					if constexpr (~configuration & UI_CONFIG_DO_NOT_NULLIFY_GENERAL_ACTION) {
						AddGeneral(position, scale, { SkipAction, &dummy, 1, });
					}
				}
			}

			template<size_t configuration>
			UIDrawPhase HandlePhase() {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					return UIDrawPhase::Late;
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					return UIDrawPhase::System;
				}
				else {
					return UIDrawPhase::Normal;
				}
			}

			template<size_t configuration>
			void HandleTransformFlags(const UIDrawConfig& config, float2& position, float2& scale) {
				if constexpr (configuration & UI_CONFIG_ABSOLUTE_TRANSFORM) {
					const float* transform = config.GetParameter(UI_CONFIG_ABSOLUTE_TRANSFORM);
					position = { *transform, *(transform + 1) };
					scale = { *(transform + 2), *(transform + 3) };
				}
				else if constexpr (configuration & UI_CONFIG_RELATIVE_TRANSFORM) {
					const float* transform = config.GetParameter(UI_CONFIG_RELATIVE_TRANSFORM);
					position.x = current_x + *transform;
					position.y = current_y + *(transform + 1);
					scale = {
						layout.default_element_x * *(transform + 2),
						layout.default_element_y * *(transform + 3)
					};
				}
				else if constexpr (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					const float* transform = config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
					WindowSizeTransformType type = *(WindowSizeTransformType*)transform;

					position.x = current_x + (*(transform + 1) * layout.default_element_x);
					position.y = current_y + (*(transform + 2) * layout.default_element_y);
					
					scale = GetWindowSizeScaleElement(type, { *(transform + 3), *(transform + 4) });
				}
				else {
					position = { current_x, current_y };
					scale = { layout.default_element_x, layout.default_element_y };
				}

				if constexpr (configuration & UI_CONFIG_MAKE_SQUARE) {
					scale = GetSquareScale(scale.y);
				}

				position.x -= region_render_offset.x;
				position.y -= region_render_offset.y;

			}

			template<size_t configuration>
			void HandleText(const UIDrawConfig& config, Color& color, float2& font_size, float& character_spacing) const {
				if constexpr (configuration & UI_CONFIG_TEXT_PARAMETERS) {
					const float* params = config.GetParameter(UI_CONFIG_TEXT_PARAMETERS);
					// first the color, then the size and finally the spacing
					color = *(Color*)params;
					font_size = *(float2*)(params + 1);
					character_spacing = *(params + 3);
					font_size.x *= zoom_ptr->x;
				}
				else {
					font_size.y = font.size * zoom_inverse.x;
					font_size.x = font.size * ECS_TOOLS_UI_FONT_X_FACTOR;
					if constexpr (~configuration & UI_CONFIG_UNAVAILABLE_TEXT) {
						color = color_theme.default_text;
					}
					else {
						color = color_theme.unavailable_text;
					}
					character_spacing = font.character_spacing;
				}
				font_size.y *= zoom_ptr->y;
			}

			void HandleTextStreamColorUpdate(Color color, Stream<UISpriteVertex> vertices) {
				if (color != vertices[0].color) {
					for (size_t index = 0; index < vertices.size; index++) {
						vertices[index].color = color;
					}
				}
			}

			void HandleTextStreamColorUpdate(Color color, CapacityStream<UISpriteVertex> vertices) {
				if (color != vertices[0].color) {
					for (size_t index = 0; index < vertices.size; index++) {
						vertices[index].color = color;
					}
				}
			}

			template<size_t configuration>
			void HandleTextLabelAlignment(
				const UIDrawConfig& config,
				float2 text_span,
				float2 label_size,
				float2 label_position,
				float& x_position,
				float& y_position,
				TextAlignment& horizontal_alignment,
				TextAlignment& vertical_alignment
			) {
				if constexpr (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
					const float* params = config.GetParameter(UI_CONFIG_TEXT_ALIGNMENT);
					const TextAlignment* alignments = (TextAlignment*)params;
					horizontal_alignment = *alignments;
					vertical_alignment = *(alignments + 1);

					switch (horizontal_alignment) {
					case TextAlignment::Left:
						x_position = label_position.x + element_descriptor.label_horizontal_padd;
						break;
					case TextAlignment::Right:
						x_position = label_position.x + label_size.x - element_descriptor.label_horizontal_padd - text_span.x;
						break;
					case TextAlignment::Middle:
						x_position = AlignMiddle(label_position.x, label_size.x, text_span.x);
						break;
					}

					switch (vertical_alignment) {
					case TextAlignment::Top:
						y_position = label_position.y + element_descriptor.label_vertical_padd;
						break;
					case TextAlignment::Bottom:
						y_position = label_position.y + label_size.y - element_descriptor.label_vertical_padd - text_span.y;
						break;
					case TextAlignment::Middle:
						y_position = AlignMiddle(label_position.y, label_size.y, text_span.y);
						break;
					}
				}
				else {
					x_position = AlignMiddle(label_position.x, label_size.x, text_span.x);
					horizontal_alignment = TextAlignment::Middle;
					y_position = AlignMiddle(label_position.y, label_size.y, text_span.y);
					vertical_alignment = TextAlignment::Middle;
				}
			}

			void HandleTextLabelAlignment(
				float2 text_span,
				float2 label_scale,
				float2 label_position, 
				float& x_position,
				float& y_position,
				TextAlignment horizontal, 
				TextAlignment vertical
			) {
				switch (horizontal) {
				case TextAlignment::Left:
					x_position = label_position.x + element_descriptor.label_horizontal_padd;
					break;
				case TextAlignment::Right:
					x_position = label_position.x + label_scale.x - element_descriptor.label_horizontal_padd - text_span.x;
					break;
				case TextAlignment::Middle:
					x_position = AlignMiddle(label_position.x, label_scale.x, text_span.x);
					break;
				}

				switch (vertical) {
				case TextAlignment::Top:
					y_position = label_position.y + element_descriptor.label_vertical_padd;
					break;
				case TextAlignment::Bottom:
					y_position = label_position.y + label_scale.y - element_descriptor.label_vertical_padd - text_span.y;
					break;
				case TextAlignment::Middle:
					y_position = AlignMiddle(label_position.y, label_scale.y, text_span.y);
					break;
				}
			}

			template<size_t configuration>
			void HandleDynamicResource(const char* name) {
				if constexpr (configuration & UI_CONFIG_DYNAMIC_RESOURCE) {
					const char* identifier = HandleResourceIdentifier(name);
					system->IncrementWindowDynamicResource(window_index, identifier);
				}
			}

			template<size_t configuration>
			void HandleBorder(const UIDrawConfig& config, float2 position, float2 scale) {
				if constexpr (configuration & UI_CONFIG_BORDER) {
					const UIConfigBorder* parameters = (const UIConfigBorder*)config.GetParameter(UI_CONFIG_BORDER);

					float2 border_size = GetSquareScale(parameters->thickness);
					border_size.x *= zoom_ptr->x;
					border_size.y *= zoom_ptr->y;

					// Late phase or system phase always in order to have the border drawn upon the hovered effect
					void** phase_buffers = buffers + ECS_TOOLS_UI_MATERIALS;
					size_t* phase_counts = counts + ECS_TOOLS_UI_MATERIALS;
					if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
						phase_buffers = system_buffers;
						phase_counts = system_counts;
					}
					else if constexpr (configuration & UI_CONFIG_BORDER_DRAW_NORMAL) {
						phase_buffers = buffers;
						phase_counts = counts;
					}

					if (parameters->is_interior) {
						CreateSolidColorRectangleBorder(position, scale, border_size, parameters->color, phase_counts, phase_buffers);
					}
					else {
						CreateSolidColorRectangleBorder<false>(position, scale, border_size, parameters->color, phase_counts, phase_buffers);
					}
				}
			}

			template<size_t configuration>
			Color HandleColor(const UIDrawConfig& config) const {
				if constexpr (configuration & UI_CONFIG_COLOR) {
					return *(Color*)config.GetParameter(UI_CONFIG_COLOR);
				}
				else {
					return color_theme.theme;
				}
			}

			template<bool permanent_buffer = false>
			const char* HandleResourceIdentifier(const char* input) {
				constexpr size_t max_character_count = 1024;

				if constexpr (!permanent_buffer) {
					if (identifier_stack.size > 0) {
						size_t input_count = strnlen_s(input, max_character_count);
						char* temp_memory = (char*)GetTempBuffer(input_count + current_identifier.size + 1, 1);
						memcpy(temp_memory, input, input_count);
						memcpy(temp_memory + input_count, current_identifier.buffer, current_identifier.size);
						temp_memory[input_count + current_identifier.size] = '\0';
						return temp_memory;
					}
					else {
						return input;
					}
				}
				else {
					size_t input_count = strnlen_s(input, max_character_count);
					char* memory = (char*)GetMainAllocatorBuffer(input_count + current_identifier.size + 1, 1);
					memcpy(memory, input, input_count);
					if (identifier_stack.size > 0) {
						memcpy(memory + input_count, current_identifier.buffer, current_identifier.size);
					}
					memory[input_count + current_identifier.size] = '\0';
					return memory;
				}
			}

			template<bool permanent_buffer = false>
			const char* HandleResourceIdentifier(const char* input, size_t& size) {
				constexpr size_t max_character_count = 1024;

				size = strnlen_s(input, max_character_count);
				if constexpr (!permanent_buffer) {
					if (identifier_stack.size > 0) {
						char* temp_memory = (char*)GetTempBuffer(size + current_identifier.size + 1, 1);
						memcpy(temp_memory, input, size);
						memcpy(temp_memory, current_identifier.buffer, current_identifier.size);
						temp_memory[size + current_identifier.size] = '\0';
						return temp_memory;
					}
					else {
						return input;
					}
				}
				else {
					char* memory = (char*)GetMainAllocatorBuffer(size + current_identifier.size + 1, 1);
					memcpy(memory, input, size);
					if (identifier_stack.size > 0) {
						memcpy(memory + size, current_identifier.buffer, current_identifier.size);
					}
					memory[size + current_identifier.size] = '\0';
					return memory;
				}
			}

			template<size_t configuration>
			void HandleRectangleActions(const UIDrawConfig& config, float2 position, float2 scale) {
				if constexpr (configuration & UI_CONFIG_HOVERABLE_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)config.GetParameter(UI_CONFIG_HOVERABLE_ACTION);
					AddHoverable(position, scale, *handler);
				}

				if constexpr (configuration & UI_CONFIG_CLICKABLE_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)config.GetParameter(UI_CONFIG_CLICKABLE_ACTION);
					AddClickable(position, scale, *handler);
				}

				if constexpr (configuration & UI_CONFIG_GENERAL_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)config.GetParameter(UI_CONFIG_GENERAL_ACTION);
					AddGeneral(position, scale, *handler);
				}
			}

			// if zoom differes between initialization and copying, then it will scale and will translate
			// as if it didn't change zoom; this is due to allow simple text to be copied and for bigger constructs
			// to determine their own change in translation needed to copy the element; since the buffer will reside
			// in cache because of the previous iteration, this should not have a big penalty on performance
			template<size_t configuration, typename TextElement = UIDrawerTextElement, typename ScaleFromText>
			TextElement* HandleTextCopyFromResource(const char* text, float2& position, Color font_color, ScaleFromText&& scale_from_text) {
				void* text_info = GetResource(text);
				TextElement* info = (TextElement*)text_info;
				HandleTextCopyFromResource<configuration, TextElement>(info, position, font_color, scale_from_text);

				return info;
			}

			// if zoom differes between initialization and copying, then it will scale and will translate
			// as if it didn't change zoom; this is due to allow simple text to be copied and for bigger constructs
			// to determine their own change in translation needed to copy the element; since the buffer will reside
			// in cache because of the previous iteration, this should not have a big penalty on performance
			template<size_t configuration, typename TextElement = UIDrawerTextElement, typename ScaleFromText>
			void HandleTextCopyFromResource(TextElement* element, float2& position, Color font_color, ScaleFromText&& scale_from_text) {
				if constexpr (~configuration & UI_CONFIG_DO_NOT_FIT_SPACE) {
					HandleFitSpaceRectangle<configuration>(position, *element->TextScale());
				}
				
				size_t* text_count = HandleTextSpriteCount<configuration>();
				UISpriteVertex* current_buffer = HandleTextSpriteBuffer<configuration>() + *text_count;

				HandleTextStreamColorUpdate(font_color, *element->TextStream());

				if constexpr (~configuration & UI_CONFIG_DISABLE_TRANSLATE_TEXT) {
					if (position.x != element->TextPosition()->x || position.y != element->TextPosition()->y) {
						TranslateText(position.x, position.y, element->TextPosition()->x, -element->TextPosition()->y, *element->TextStream());
						*element->TextPosition() = position;
					}
				}

				if (element->GetZoomX() == zoom_ptr->x && element->GetZoomY() == zoom_ptr->y) {
					if (ValidatePosition<configuration>(*element->TextPosition(), *element->TextScale())) {
						memcpy(current_buffer, element->TextBuffer(), sizeof(UISpriteVertex) * *element->TextSize());
						*text_count += *element->TextSize();
					}
				}
				else {
					ScaleText(element, current_buffer - *text_count, text_count, zoom_ptr, font.character_spacing);
					memcpy(element->TextBuffer(), current_buffer, sizeof(UISpriteVertex) * *element->TextSize());
					element->TextScale()->x *= element->GetInverseZoomX() *  zoom_ptr->x;
					element->TextScale()->y *= element->GetInverseZoomY() *  zoom_ptr->y;
					element->SetZoomFactor(*zoom_ptr);
				}

				if constexpr (~configuration & UI_CONFIG_DO_NOT_FIT_SPACE) {
					float2 new_scale = scale_from_text({
						element->TextScale()->x * zoom_ptr->x * element->GetInverseZoomX(),
						element->TextScale()->y * zoom_ptr->y * element->GetInverseZoomY()
						});
					HandleFitSpaceRectangle<configuration>(position, new_scale);
				}

				if constexpr (configuration & UI_CONFIG_TEXT_ALIGN_TO_ROW_Y) {
					float y_position = AlignMiddle(position.y, current_row_y_scale, element->TextScale()->y);
					TranslateTextY(y_position, 0, *element->TextStream());
				}
			}

			// cull_mode 0 - horizontal
			// cull_mode 1 - vertical
			// cull_mode 2 - horizontal main, vertical secondary
			// cull_mode 3 - vertical main, horizontal secondary
			template<size_t configuration, size_t cull_mode, typename TextElement = UIDrawerTextElement>
			TextElement* HandleLabelTextCopyFromResourceWithCull(
				const UIDrawConfig& config, 
				const char* text, 
				float2& position, 
				float2 scale, 
				float2& text_span,
				size_t& copy_count
			) {
				void* text_info = GetResource(text);
				TextElement* info = (TextElement*)text_info;
				HandleLabelTextCopyFromResourceWithCull<configuration, cull_mode, TextElement>(config, info, position, scale, text_span, copy_count);
						
				return info;
			}

			// cull_mode 0 - horizontal
			// cull_mode 1 - vertical
			// cull_mode 2 - horizontal main, vertical secondary
			// cull_mode 3 - vertical main, horizontal secondary
			template<size_t configuration, size_t cull_mode, typename TextElement = UIDrawerTextElement>
			void HandleLabelTextCopyFromResourceWithCull(
				const UIDrawConfig& config,
				TextElement* info,
				float2& position,
				float2 scale,
				float2& text_span,
				size_t& copy_count
			) {
				HandleFitSpaceRectangle<configuration>(position, scale);

				bool memcpy_all = true;
				if constexpr (cull_mode == 0) {
					if (info->TextScale()->x > scale.x - 2 * element_descriptor.label_horizontal_padd) {
						memcpy_all = false;
					}
				}
				else if constexpr (cull_mode == 1) {
					if (info->TextScale()->y > scale.y - 2 * element_descriptor.label_vertical_padd) {
						memcpy_all = false;
					}
				}
				else if constexpr (cull_mode == 2 || cull_mode == 3) {
					if ((info->TextScale()->x > scale.x - 2 * element_descriptor.label_horizontal_padd) || (info->TextScale()->y > scale.y - 2 * element_descriptor.label_vertical_padd)) {
						memcpy_all = false;
					}
				}

				size_t* text_count = HandleTextSpriteCount<configuration>();
				auto text_buffer = HandleTextSpriteBuffer<configuration>();
				UISpriteVertex* current_buffer = text_buffer + *text_count;
				copy_count = *info->TextSize();

				float2 font_size;
				float character_spacing;
				Color font_color;
				HandleText<configuration>(config, font_color, font_size, character_spacing);
				HandleTextStreamColorUpdate(font_color, *info->TextStream());

				Stream<UISpriteVertex> current_stream = Stream<UISpriteVertex>(current_buffer, copy_count);
				CapacityStream<UISpriteVertex> text_stream = *info->TextStream();
				TextAlignment horizontal_alignment, vertical_alignment;
				GetTextLabelAlignment<configuration>(config, horizontal_alignment, vertical_alignment);

				auto memcpy_fnc = [&](unsigned int first_index, unsigned int second_index, bool vertical) {
					float x_position, y_position;
					TextAlignment dummy1, dummy2;
					HandleTextLabelAlignment<configuration>(
						config,
						*info->TextScale(),
						scale,
						position,
						x_position,
						y_position,
						dummy1,
						dummy2
						);

					float x_translation;
					float y_translation;
					if (!vertical) {
						x_translation = x_position - text_stream[first_index].position.x;
						y_translation = -y_position - text_stream[second_index].position.y;
					}
					else {
						float lowest_x = info->GetLowestX();
						x_translation = x_position - lowest_x;
						y_translation = -y_position - text_stream[second_index].position.y;
					}
					for (size_t index = 0; index < text_stream.size; index++) {
						text_stream[index].position.x += x_translation;
						text_stream[index].position.y += y_translation;
					}


					memcpy(current_stream.buffer, text_stream.buffer, sizeof(UISpriteVertex) * text_stream.size);
					text_span = *info->TextScale();
				};

				text_span = *info->TextScale();
				if constexpr (cull_mode == 0) {
					if (horizontal_alignment == TextAlignment::Right) {
						if (!memcpy_all) {
							CullTextSprites<1, 1>(text_stream, current_stream, text_stream[1].position.x - scale.x + 2 * element_descriptor.label_horizontal_padd);
							text_span = GetTextSpan<true, true>(current_stream);

							float x_position, y_position;
							HandleTextLabelAlignment<configuration>(
								config,
								text_span,
								scale,
								position,
								x_position,
								y_position,
								horizontal_alignment,
								vertical_alignment
								);

							TranslateText(x_position, y_position, current_stream.size - 2, current_stream.size - 3, current_stream);
						}
						else {
							memcpy_fnc(text_stream.size - 2, text_stream.size - 3, false);
						}
					}
					else {
						if (!memcpy_all) {
							CullTextSprites<0, 1>(text_stream, current_stream, text_stream[0].position.x + scale.x - 2 * element_descriptor.label_horizontal_padd);
							text_span = GetTextSpan<true, false>(current_stream);

							float x_position, y_position;
							HandleTextLabelAlignment<configuration>(
								config,
								text_span,
								scale,
								position,
								x_position,
								y_position,
								horizontal_alignment,
								vertical_alignment
								);

							TranslateText(x_position, y_position, current_stream, text_stream[0].position.x, text_stream[0].position.y);
						}
						else {
							memcpy_fnc(0, 0, false);
						}
					}
				}
				else if constexpr (cull_mode == 1) {
					if (vertical_alignment == TextAlignment::Bottom) {
						if (!memcpy_all) {
							CullTextSprites<3, 1>(text_stream, current_stream, text_stream[text_stream.size - 3].position.y - scale.y + 2 * element_descriptor.label_vertical_padd);
							text_span = GetTextSpan<false, true>(current_stream);

							float x_position, y_position;
							HandleTextLabelAlignment<configuration>(
								config,
								text_span,
								scale,
								position,
								x_position,
								y_position,
								horizontal_alignment,
								vertical_alignment
								);

							float lowest_x = GetMinXRectangle(current_stream.buffer, current_stream.size);
							TranslateText(x_position, y_position, current_stream, lowest_x, current_stream[current_stream.size - 3].position.y);
						}
						else {
							memcpy_fnc(text_stream.size - 1, text_stream.size - 3, true);
						}
					}
					else {
						if (!memcpy_all) {
							CullTextSprites<2, 1>(text_stream, current_stream, text_stream[0].position.y - scale.y + 2 * element_descriptor.label_vertical_padd);
							text_span = GetTextSpan<false, false>(current_stream);

							float x_position, y_position;
							HandleTextLabelAlignment<configuration>(
								config,
								text_span,
								scale,
								position,
								x_position,
								y_position,
								horizontal_alignment,
								vertical_alignment
								);

							float lowest_x = GetMinXRectangle(current_stream.buffer, current_stream.size);
							TranslateText(x_position, y_position, current_stream, lowest_x, text_stream[0].position.y);
						}
						else {
							memcpy_fnc(0, 0, true);
						}
					}
				}
				else if constexpr (cull_mode == 2) {
					if (info->TextScale()->y > scale.y - 2 * element_descriptor.label_vertical_padd) {
						current_stream.size = 0;
						text_span = { 0.0f, 0.0f };
					}
					else {
						if (horizontal_alignment == TextAlignment::Right) {
							if (!memcpy_all) {
								CullTextSprites<1, 1>(text_stream, current_stream, text_stream[1].position.x - scale.x + 2 * element_descriptor.label_horizontal_padd);
								text_span = GetTextSpan<true, true>(current_stream);

								float x_position, y_position;
								HandleTextLabelAlignment<configuration>(
									config,
									text_span,
									scale,
									position,
									x_position,
									y_position,
									horizontal_alignment,
									vertical_alignment
									);

								TranslateText(x_position, y_position, current_stream.size - 2, current_stream.size - 3, current_stream);
							}
							else {
								memcpy_fnc(text_stream.size - 2, text_stream.size - 3, false);
							}
						}
						else {
							if (!memcpy_all) {
								CullTextSprites<0, 1>(text_stream, current_stream, text_stream[0].position.x + scale.x - 2 * element_descriptor.label_horizontal_padd);
								text_span = GetTextSpan<true, false>(current_stream);

								float x_position, y_position;
								HandleTextLabelAlignment<configuration>(
									config,
									text_span,
									scale,
									position,
									x_position,
									y_position,
									horizontal_alignment,
									vertical_alignment
									);

								TranslateText(x_position, y_position, current_stream, text_stream[0].position.x, text_stream[0].position.y);
							}
							else {
								memcpy_fnc(0, 0, false);
							}
						}
					}
				}
				else if constexpr (cull_mode == 3) {
					if (info->TextScale()->x > scale.x - 2 * element_descriptor.label_horizontal_padd) {
						current_stream.size = 0;
						text_span = { 0.0f, 0.0f };
					}
					else {
						if (vertical_alignment == TextAlignment::Bottom) {
							if (!memcpy_all) {
								CullTextSprites<3, 1>(text_stream, current_stream, text_stream[text_stream.size - 3].position.y - scale.y + 2 * element_descriptor.label_vertical_padd);
								text_span = GetTextSpan<false, true>(current_stream);

								float x_position, y_position;
								HandleTextLabelAlignment<configuration>(
									config,
									text_span,
									scale,
									position,
									x_position,
									y_position,
									horizontal_alignment,
									vertical_alignment
									);

								float lowest_x = GetMinXRectangle(current_stream.buffer, current_stream.size);
								TranslateText(x_position, y_position, current_stream, lowest_x, current_stream[current_stream.size - 3].position.y);
							}
							else {
								memcpy_fnc(text_stream.size - 1, text_stream.size - 3, true);
							}
						}
						else {
							if (!memcpy_all) {
								if (vertical_alignment == TextAlignment::Middle) {
									CullTextSprites<2, 1>(text_stream, current_stream, text_stream[0].position.y - scale.y + element_descriptor.label_vertical_padd);
								}
								else {
									CullTextSprites<2, 1>(text_stream, current_stream, text_stream[0].position.y - scale.y + 2 * element_descriptor.label_vertical_padd);
								}
								text_span = GetTextSpan<false, false>(current_stream);

								float x_position, y_position;
								HandleTextLabelAlignment<configuration>(
									config,
									text_span,
									scale,
									position,
									x_position,
									y_position,
									horizontal_alignment,
									vertical_alignment
									);

								float lowest_x = GetMinXRectangle(current_stream.buffer, current_stream.size);
								TranslateText(x_position, y_position, current_stream, lowest_x, text_stream[0].position.y);
							}
							else {
								memcpy_fnc(0, 0, true);
							}
						}
					}
				}
				copy_count = current_stream.size;

				*text_count += copy_count;
			}

			template<size_t configuration>
			bool HandleFitSpaceRectangle(float2& position, float2 scale) {
				if constexpr (~configuration & UI_CONFIG_DO_NOT_FIT_SPACE) {
					if (draw_mode == UIDrawerMode::FitSpace) {
						bool is_outside = VerifyFitSpaceRectangle(position, scale);
						if (is_outside) {
							NextRow();
							position = ECS_TOOLS_UI_DRAWER_CURRENT_POSITION;
							position.x -= region_render_offset.x;
							position.y -= region_render_offset.y;
							return true;
						}
						return false;
					}
					else if (draw_mode == UIDrawerMode::ColumnDrawFitSpace || draw_mode == UIDrawerMode::ColumnDraw) {
						bool is_outside = VerifyFitSpaceRectangle(position, scale);
						if (is_outside) {
							NextRow();
							position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
							//draw_mode_extra_float.y = position.y;
							return true;
						}
						return false;
					}
				}
				return false;
			}

			template<size_t configuration, typename SliderType, typename Filter>
			void HandleSliderActions(
				float2 position, 
				float2 scale,
				Color color, 
				float2 slider_position, 
				float2 slider_scale, 
				Color slider_color,
				UIDrawerSlider* info
			) {
				UIDrawPhase phase = HandlePhase<configuration>();
				if constexpr (configuration & UI_CONFIG_SLIDER_DEFAULT_VALUE) {
					AddHoverable(position, scale, { SliderReturnToDefault, info, 0, phase });
				}
				if constexpr (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
					AddGeneral(position, scale, { SliderEnterValues<SliderType, Filter>, info, 0, phase });
				}
				if constexpr (~configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
					UIDrawerSliderBringToMouse bring_info;
					bring_info.slider = info;
					if constexpr (configuration & UI_CONFIG_VERTICAL) {
						bring_info.slider_length = slider_scale.y;
					}
					else {
						bring_info.slider_length = slider_scale.x;
					}

					if (info->text_input_counter == 0) {
						SolidColorRectangle<configuration>(slider_position, slider_scale, slider_color);
						AddClickable(position, scale, { SliderBringToMouse, &bring_info, sizeof(bring_info), phase });
						AddClickable(slider_position, slider_scale, { SliderAction, info, 0, phase });
						AddDefaultHoverable(slider_position, slider_scale, slider_color, 1.25f, phase);
					}
				}
				else {
					if (info->text_input_counter == 0) {
						AddClickable(position, scale, { SliderMouseDraggable, &info, 8, phase });
						if constexpr (configuration & UI_CONFIG_SLIDER_DEFAULT_VALUE) {
							UIDrawerSliderReturnToDefaultMouseDraggable data;
							data.slider = info;
							data.hoverable_data.colors[0] = color;
							data.hoverable_data.percentages[0] = 1.25f;
							AddHoverable(position, scale, { SliderReturnToDefaultMouseDraggable, &data, sizeof(data), phase });
						}
						else {
							AddDefaultHoverable(position, scale, color, 1.25f, phase);
						}
					}
					else {
						if constexpr (configuration & UI_CONFIG_SLIDER_DEFAULT_VALUE) {
							AddHoverable(position, scale, { SliderReturnToDefault, info, 0, phase });
						}
					}
				}
			}

			float2 HandleLabelSize(float2 text_span) const {
				float2 scale;
				scale = text_span;
				scale.x += 2 * element_descriptor.label_horizontal_padd;
				scale.y += 2 * element_descriptor.label_vertical_padd;
				/*if constexpr (configuration & UI_CONFIG_RELATIVE_TRANSFORM) {
					const float* transform = config.GetParameter(UI_CONFIG_RELATIVE_TRANSFORM);
					scale.x += *(transform + 2) * layout.default_element_x;
					scale.y += *(transform + 3) * layout.default_element_y;
				}*/
				return scale;
			}

			template<size_t configuration>
			void HandleSliderVariables(
				const UIDrawConfig& config,
				float& length, float& padding,
				float2& shrink_factor, 
				Color& slider_color,
				Color background_color
			) {
				if constexpr (configuration & UI_CONFIG_SLIDER_LENGTH) {
					length = *config.GetParameter(UI_CONFIG_SLIDER_LENGTH);
				}
				else {
					if constexpr (configuration & UI_CONFIG_VERTICAL) {
						length = element_descriptor.slider_length.y;
					}
					else {
						length = element_descriptor.slider_length.x;
					}
				}

				if constexpr (configuration & UI_CONFIG_SLIDER_PADDING) {
					padding = *config.GetParameter(UI_CONFIG_SLIDER_PADDING);
				}
				else {
					if constexpr (configuration & UI_CONFIG_VERTICAL) {
						padding = element_descriptor.slider_padding.y;
					}
					else {
						padding = element_descriptor.slider_padding.x;
					}
				}
				
				if constexpr (configuration & UI_CONFIG_SLIDER_SHRINK) {
					shrink_factor = *(const float2*)config.GetParameter(UI_CONFIG_SLIDER_SHRINK);
				}
				else {
					shrink_factor = element_descriptor.slider_shrink;
				}

				if constexpr (configuration & UI_CONFIG_SLIDER_COLOR) {
					slider_color = *(const Color*)config.GetParameter(UI_CONFIG_SLIDER_COLOR);
				}
				else {
					slider_color = ToneColor(background_color, color_theme.slider_lighten_factor);
				}
			}

			template<size_t configuration>
			void SolidColorRectangle(float2 position, float2 scale, Color color) {
				if constexpr (!initializer) {
					SetSolidColorRectangle(
						position,
						scale,
						color,
						HandleSolidColorBuffer<configuration>(),
						*HandleSolidColorCount<configuration>()
					);
				}
			}

			template<size_t configuration>
			void SolidColorRectangle(const UIDrawConfig& config, float2 position, float2 scale) {
				Color color = HandleColor<configuration>(config);
				SolidColorRectangle<configuration>(position, scale, color);
			}

			template<size_t configuration>
			void SolidColorRectangle(const UIDrawConfig& config, float2 position, float2 scale, Color color) {
				if constexpr (!initializer) {
					SolidColorRectangle(position, scale, color);

					HandleBorder<configuration>(config, position, scale);
					HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
					HandleRectangleActions<configuration>(config, position, scale);
				}
			}

			// it will finalize the rectangle
			template<size_t configuration>
			void TextLabel(const UIDrawConfig& config, const char* text, float2& position, float2& scale) {
				Stream<UISpriteVertex> current_text = GetTextStream<configuration>(ParseStringIdentifier(text, strlen(text)) * 6);
				float2 text_span;

				float2 temp_position = { position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd };
				Text<configuration | UI_CONFIG_DO_NOT_FIT_SPACE>(config, text, temp_position, text_span);
				
				float2 label_scale = HandleLabelSize(text_span);

				if constexpr (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) {
					scale.x = label_scale.x;
				}
				if constexpr (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) {
					scale.y = label_scale.y;
				}

				if constexpr (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
					position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
				}

				float2 position_copy = position;
				bool is_moved = HandleFitSpaceRectangle<configuration>(position, scale);

				TextAlignment horizontal_alignment = TextAlignment::Middle, vertical_alignment = TextAlignment::Top;
				float x_text_position, y_text_position;
				HandleTextLabelAlignment<configuration>(
					config,
					text_span,
					scale,
					position,
					x_text_position,
					y_text_position,
					horizontal_alignment,
					vertical_alignment
				);

				if (is_moved) {
					TranslateText(x_text_position, y_text_position, current_text);
				}
				else if (horizontal_alignment != TextAlignment::Left || vertical_alignment != TextAlignment::Top) {
					float x_translation = x_text_position - current_text[0].position.x;
					float y_translation = y_text_position + current_text[0].position.y;
					for (size_t index = 0; index < current_text.size; index++) {
						current_text[index].position.x += x_translation;
						current_text[index].position.y -= y_translation;
					}
				}

				if constexpr (configuration & UI_CONFIG_VERTICAL) {
					AlignVerticalText(current_text);
				}

				HandleBorder<configuration>(config, position, scale);

				if constexpr (~configuration & UI_CONFIG_LABEL_TRANSPARENT) {
					Color label_color = HandleColor<configuration>(config);
					SolidColorRectangle<configuration>(
						position,
						scale,
						label_color
					);
				}

				HandleLateAndSystemDrawActionNullify<configuration>(position, scale);

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				FinalizeRectangle<configuration>(position, scale);
			}

			// resource drawer
			template<size_t configuration>
			void TextLabel(const UIDrawConfig& config, UIDrawerTextElement* text, float2& position, float2 scale) {
				if constexpr (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
					position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
				}
				float2 text_span = text->scale;

				bool is_moved = HandleFitSpaceRectangle<configuration>(position, text_span);

				HandleBorder<configuration>(config, position, scale);
				if constexpr (~configuration & UI_CONFIG_LABEL_TRANSPARENT) {
					Color label_color = HandleColor<configuration>(config);
					SolidColorRectangle<configuration>(
						position,
						scale,
						label_color
					);
				}

				HandleLateAndSystemDrawActionNullify<configuration>(position, scale);

				float x_position, y_position;
				TextAlignment horizontal = TextAlignment::Left, vertical = TextAlignment::Top;
				HandleTextLabelAlignment<configuration>(
					config,
					text_span,
					scale,
					position,
					x_position,
					y_position,
					horizontal,
					vertical
				);
				
				position = { x_position, y_position };

				float2 font_size;
				float character_spacing;
				Color font_color;
				HandleText<configuration>(config, font_color, font_size, character_spacing);

				HandleTextCopyFromResource<configuration | UI_CONFIG_DO_NOT_FIT_SPACE>(text, position, font_color, ECS_TOOLS_UI_DRAWER_IDENTITY_SCALE);
					
				FinalizeRectangle<configuration>(position, text_span);
			}

			template<size_t configuration, typename UIDrawConfig>
			void Rectangle(const UIDrawConfig& config, float2 position, float2 scale) {
				HandleFitSpaceRectangle<configuration>(position, scale);

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}
				
				if (ValidatePosition<configuration>(position, scale)) {
					if constexpr (configuration & UI_CONFIG_COLOR) {
						const Color* color = (const Color*)config.GetParameter(UI_CONFIG_COLOR);
						SolidColorRectangle<configuration>(position, scale, *color);
					}
					else if constexpr (configuration & UI_CONFIG_RECTANGLE_VERTEX_COLOR) {
						const Color* colors = (const Color*)config.GetParameter(UI_CONFIG_RECTANGLE_VERTEX_COLOR);
						VertexColorRectangle<configuration>(position, scale, *colors, *(colors + 1), *(colors + 2), *(colors + 3));
					}

					if constexpr (configuration & UI_CONFIG_TOOL_TIP) {
						if (IsPointInRectangle(mouse_position, position, scale)) {
							const UIConfigToolTip* tool_tip = (const UIConfigToolTip*)config.GetParameter(UI_CONFIG_TOOL_TIP);

							UITextTooltipHoverableData hover_data;
							hover_data.characters = tool_tip->characters;
							hover_data.base.offset.y = 0.005f;
							hover_data.base.offset_scale.y = true;

							ActionData action_data = system->GetFilledActionData(window_index);
							action_data.data = &hover_data;
							TextTooltipHoverable(&action_data);
						}
					}

					HandleRectangleActions<configuration>(config, position, scale);
					HandleBorder<configuration>(config, position, scale);
				}
				FinalizeRectangle<configuration>(position, scale);
			}

			template<size_t configuration>
			void SpriteClusterRectangle(
				float2 position,
				float2 scale,
				LPCWSTR texture,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				Color color = ECS_COLOR_WHITE
			) {
				auto buffer = HandleSpriteClusterBuffer<configuration>();
				auto count = HandleSpriteClusterCount<configuration>();

				SetSpriteRectangle(position, scale, color, top_left_uv, bottom_right_uv, buffer, *count);
			}

			template<size_t configuration>
			void VertexColorSpriteClusterRectangle(
				float2 position,
				float2 scale,
				LPCWSTR texture,
				const Color* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			) {
				auto buffer = HandleSpriteClusterBuffer<configuration>();
				auto count = HandleSpriteClusterCount<configuration>();

				SetVertexColorSpriteRectangle(position, scale, colors, top_left_uv, bottom_right_uv, buffer, *count);
			}

			template<size_t configuration>
			void VertexColorSpriteClusterRectangle(
				float2 position,
				float2 scale,
				LPCWSTR texture,
				const ColorFloat* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			) {
				auto buffer = HandleSpriteClusterBuffer<configuration>();
				auto count = HandleSpriteClusterCount<configuration>();

				SetVertexColorSpriteRectangle(position, scale, colors, top_left_uv, bottom_right_uv, buffer, *count);
			}

			// resource drawer
			template<size_t configuration>
			void Text(const UIDrawConfig& config, UIDrawerTextElement* text, float2 position) {
				float2 zoomed_scale = { text->scale.x * zoom_ptr->x * text->inverse_zoom.x, text->scale.y * zoom_ptr->y * text->inverse_zoom.y };
				float2 font_size;
				float character_spacing;
				Color font_color;
				HandleText<configuration>(config, font_color, font_size, character_spacing);
					
				if (ValidatePosition<configuration>(position, zoomed_scale)) {
					HandleTextCopyFromResource<configuration>(text, position, font_color, ECS_TOOLS_UI_DRAWER_IDENTITY_SCALE);
				}

				FinalizeRectangle<configuration>(position, zoomed_scale);
			}

			template<size_t configuration>
			void TextLabelWithCull(const UIDrawConfig& config, const char* text, float2 position, float2 scale) {
				if constexpr (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
					position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
				}
				
				Stream<UISpriteVertex> current_text = GetTextStream<configuration>(ParseStringIdentifier(text, strlen(text)) * 6);
				TextAlignment horizontal_alignment, vertical_alignment;
				GetTextLabelAlignment<configuration>(config, horizontal_alignment, vertical_alignment);

				const float* dependent_size = config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
				const WindowSizeTransformType* type = (WindowSizeTransformType*)dependent_size;

				size_t text_count = ParseStringIdentifier(text, strlen(text));
				Color color;
				float2 font_size;
				float character_spacing;
				HandleText<configuration>(config, color, font_size, character_spacing);
				float2 text_span;

				auto text_sprites = HandleTextSpriteBuffer<configuration>();
				auto text_sprite_count = HandleTextSpriteCount<configuration>();
				if constexpr (configuration & UI_CONFIG_VERTICAL) {
					if (vertical_alignment == TextAlignment::Bottom) {
						system->ConvertCharactersToTextSprites<false, true>(
							text,
							{ position.x + layout.label_horizontal_padd, position.y + layout.label_vertical_padd },
							text_sprites,
							text_count,
							color,
							*text_sprite_count,
							font_size,
							character_spacing
						);
						text_span = GetTextSpan<false, true>(current_text);
					}
					else {
						system->ConvertCharactersToTextSprites<false, false>(
							text,
							{ position.x + layout.label_horizontal_padd, position.y + layout.label_vertical_padd },
							text_sprites,
							text_count,
							color,
							*text_sprite_count,
							font_size,
							character_spacing
						);
						text_span = GetTextSpan<false, false>(current_text);
					}
				}
				else {
					if (horizontal_alignment == TextAlignment::Right) {
						system->ConvertCharactersToTextSprites<true, true>(
							text,
							{ position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd },
							text_sprites,
							text_count,
							color,
							*text_sprite_count,
							font_size,
							character_spacing
						);
						text_span = GetTextSpan<true, true>(current_text);
					}
					else {
						system->ConvertCharactersToTextSprites<true, false>(
							text,
							{ position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd },
							text_sprites,
							text_count,
							color,
							*text_sprite_count,
							font_size,
							character_spacing
						);
						text_span = GetTextSpan<true, false>(current_text);
					}
				}

				size_t vertex_count = 0;
				if constexpr (~configuration & UI_CONFIG_VERTICAL) {
					if (*type == WindowSizeTransformType::Both && text_span.y > scale.y - 2 * element_descriptor.label_vertical_padd) {
						vertex_count = 0;
					}
					else {
						if (text_span.x > scale.x - 2 * element_descriptor.label_horizontal_padd) {
							if (horizontal_alignment == TextAlignment::Left || horizontal_alignment == TextAlignment::Middle) {
								vertex_count = CullTextSprites<0>(current_text, position.x + scale.x - element_descriptor.label_horizontal_padd);
								current_text.size = vertex_count;
								text_span = GetTextSpan<true, false>(current_text);
							}
							else {
								vertex_count = CullTextSprites<1>(current_text, position.x + element_descriptor.label_horizontal_padd);
								current_text.size = vertex_count;
								text_span = GetTextSpan<true, true>(current_text);
							}
						}
						else {
							vertex_count = current_text.size;
						}
					}
				}
				else {
					if (*type == WindowSizeTransformType::Both && text_span.x > scale.x - 2 * element_descriptor.label_horizontal_padd) {
						vertex_count = 0;
					}
					else {
						if (text_span.y > scale.y - 2 * element_descriptor.label_vertical_padd) {
							if (vertical_alignment == TextAlignment::Top || vertical_alignment == TextAlignment::Middle) {
								vertex_count = CullTextSprites<2>(current_text, -position.y - scale.y + element_descriptor.label_vertical_padd);
								current_text.size = vertex_count;
								text_span = GetTextSpan<false, false>(current_text);
							}
							else {
								vertex_count = CullTextSprites<3>(current_text, -position.y - scale.y + element_descriptor.label_vertical_padd);
								current_text.size = vertex_count;
								text_span = GetTextSpan<false, true>(current_text);
							}
						}
						else {
							vertex_count = current_text.size;
						}
					}
				}

				float x_text_position, y_text_position;
				HandleTextLabelAlignment<configuration>(
					config,
					text_span,
					scale,
					position,
					x_text_position,
					y_text_position,
					horizontal_alignment,
					vertical_alignment
				);

				if (horizontal_alignment != TextAlignment::Left) {
					float x_translation = x_text_position - position.x - element_descriptor.label_horizontal_padd;
					for (size_t index = 0; index < vertex_count; index++) {
						current_text[index].position.x += x_translation;
					}
				}

				if (vertical_alignment == TextAlignment::Bottom) {
					float y_translation = y_text_position + (current_text[current_text.size - 3]).position.y;					
					for (size_t index = 0; index < vertex_count; index++) {
						current_text[index].position.y -= y_translation;
					}
				}
				else if (vertical_alignment == TextAlignment::Middle) {
					float y_translation = y_text_position - position.y - element_descriptor.label_vertical_padd;
					for (size_t index = 0; index < vertex_count; index++) {
						current_text[index].position.y -= y_translation;
					}
				}

				if constexpr (configuration & UI_CONFIG_VERTICAL) {
					AlignVerticalText(current_text);
				}

				*text_sprite_count += vertex_count;
				if constexpr (~configuration & UI_CONFIG_LABEL_TRANSPARENT) {
					Color label_color = HandleColor<configuration>(config);
					SolidColorRectangle<configuration>(
						position,
						scale,
						label_color
					);
				}

				HandleBorder<configuration>(config, position, scale);

				HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
			}

			template<size_t configuration>
			UIDrawerTextElement* TextInitializer(const UIDrawConfig& config, const char* characters, float2 position) {
				UIDrawerTextElement* element;

				AddWindowResource(characters, [&](const char* label_identifier) {
					element = GetMainAllocatorBuffer<UIDrawerTextElement>();

					ConvertTextToWindowResource<configuration>(config, label_identifier, element, position);

					if constexpr (~configuration & UI_CONFIG_DO_NOT_ADVANCE) {
						FinalizeRectangle<configuration>(position, element->scale);
					}
					return element;
				});

				return element;
			}

			template<size_t configuration>
			UIDrawerTextElement* TextLabelInitializer(const UIDrawConfig& config, const char* characters, float2 position, float2 scale) {
				UIDrawerTextElement* element;
				
				AddWindowResource(characters, [&](const char* identifier) {
					TextAlignment horizontal_alignment, vertical_alignment;
					element = GetMainAllocatorBuffer<UIDrawerTextElement>();

					ConvertTextToWindowResource<configuration>(config, identifier, element, {position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd});
					float x_position, y_position;
					if constexpr (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
						if constexpr (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) {
							scale.x = element->scale.x + 2 * element_descriptor.label_horizontal_padd;
						}
						if constexpr (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) {
							scale.y = element->scale.y + 2 * element_descriptor.label_vertical_padd;
						}
						
						if constexpr (((configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) != 0) && ((configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) != 0)) {
							scale.x = function::PredicateValue(scale.x < element->scale.x, element->scale.x, scale.x);
							scale.y = function::PredicateValue(scale.y < element->scale.y, element->scale.y, scale.y);
							/*scale.x += 2 * element_descriptor.label_horizontal_padd;
							scale.y += 2 * element_descriptor.label_vertical_padd;*/
						}
						HandleTextLabelAlignment<configuration>(config, element->scale, scale, position, x_position, y_position, horizontal_alignment, vertical_alignment);
						TranslateText(x_position, y_position, element->text_vertices);
						element->position.x -= x_position - position.x;
						element->position.y -= y_position - position.y;
					}

					FinalizeRectangle<configuration>(position, scale);
					return element;
				});

				return element;
			}

			template<size_t configuration>
			void TextLabelDrawer(const UIDrawConfig& config, UIDrawerTextElement* element, float2 position, float2 scale) {
				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {	
					if constexpr (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
						position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
					}

					TextAlignment horizontal_alignment, vertical_alignment;
					GetTextLabelAlignment<configuration>(config, horizontal_alignment, vertical_alignment);

					if constexpr (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
						const float* dependent_size = config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
						const WindowSizeTransformType* type = (WindowSizeTransformType*)dependent_size;

						size_t copy_count;
						float2 text_span;
						Stream<UISpriteVertex> vertices = GetTextStream<configuration>(0);

						if (*type == WindowSizeTransformType::Horizontal) {
							HandleLabelTextCopyFromResourceWithCull<configuration, 0>(
								config,
								element,
								position,
								scale,
								text_span,
								copy_count
								);
						}
						else if (*type == WindowSizeTransformType::Vertical) {
							HandleLabelTextCopyFromResourceWithCull<configuration, 1>(
								config,
								element,
								position,
								scale,
								text_span,
								copy_count
								);
						}
						else if (*type == WindowSizeTransformType::Both) {
							if constexpr (configuration & UI_CONFIG_VERTICAL) {
								HandleLabelTextCopyFromResourceWithCull<configuration, 3>(
									config,
									element,
									position,
									scale,
									text_span,
									copy_count
									);
							}
							else {
								HandleLabelTextCopyFromResourceWithCull<configuration, 2>(
									config,
									element,
									position,
									scale,
									text_span,
									copy_count
									);
							}
						}
					}
					else {
						Stream<UISpriteVertex> vertices = GetTextStream<configuration>(element->text_vertices.size);
						float2 font_size;
						float character_spacing;
						Color font_color;

						HandleText<configuration>(config, font_color, font_size, character_spacing);
						HandleTextCopyFromResource<configuration | UI_CONFIG_DISABLE_TRANSLATE_TEXT>(
							element, 
							position, 
							font_color, 
							ECS_TOOLS_UI_DRAWER_LABEL_SCALE
						);

						float2 label_scale = HandleLabelSize(element->scale);
						if constexpr (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) {
							scale.x = label_scale.x;
						}
						if constexpr (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) {
							scale.y = label_scale.y;
						}

						float x_position, y_position;
						HandleTextLabelAlignment(
							element->scale,
							scale,
							position,
							x_position,
							y_position,
							horizontal_alignment,
							vertical_alignment
						);

						if (x_position != element->position.x || y_position != element->position.y) {
							if constexpr (configuration & UI_CONFIG_VERTICAL) {
								if (vertical_alignment == TextAlignment::Bottom) {
									TranslateText(x_position, y_position, element->text_vertices, element->text_vertices.size - 1, element->text_vertices.size - 3);
									TranslateText(x_position, y_position, vertices, element->text_vertices.size - 1, element->text_vertices.size - 3);
								}
								else {
									TranslateText(x_position, y_position, element->text_vertices, 0, 0);
									TranslateText(x_position, y_position, vertices, 0, 0);
								}
							}
							else {
								if (horizontal_alignment == TextAlignment::Right) {
									TranslateText(x_position, y_position, element->text_vertices, element->text_vertices.size - 1, element->text_vertices.size - 3);
									TranslateText(x_position, y_position, vertices, element->text_vertices.size - 1, element->text_vertices.size - 3);
								}
								else {
									TranslateText(x_position, y_position, element->text_vertices, 0, 0);
									TranslateText(x_position, y_position, vertices, 0, 0);
								}
							}

							element->position.x = x_position;
							element->position.y = y_position;
						}
					}

					if constexpr (~configuration & UI_CONFIG_LABEL_TRANSPARENT) {
						Color label_color = HandleColor<configuration>(config);
						SolidColorRectangle<configuration>(
							position,
							scale,
							label_color
						);
						HandleBorder<configuration>(config, position, scale);
					}
					HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
				}

				float2 font_size;
				float character_spacing;
				Color font_color;
				HandleText<configuration>(config, font_color, font_size, character_spacing);
				HandleTextStreamColorUpdate(font_color, element->text_vertices);

				FinalizeRectangle<configuration>(position, scale);
			}

			/*template<size_t configuration>
			void TableDrawer(
				UIDrawConfig& config,
				size_t row_count,
				size_t column_count,
				Action column_action,
				const char** column_headings,
				const float* column_x_span,
				void** cell_data,
				const char* name,
				TextAlignment name_alignment,
				float2 position, 
				float2 scale
			) {
				float2 starting_position = position;
				if (name != nullptr) {
					UIConfigTextAlignment alignment;
					alignment.horizontal = name_alignment;
					alignment.vertical = TextAlignment::Middle;

					UIConfigTextAlignment previous_alignment;
					if constexpr (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
						config.SetExistingFlag(alignmnet, UI_CONFIG_TEXT_ALIGNMENT, previous_alignment);
					}
					TextLabel<configuration | UI_CONFIG_TEXT_ALIGNMENT>(config, name, position, scale);
				}
			}*/

			// single lined text input; drawer kernel
			template<size_t configuration, typename Filter = TextFilterAll>
			void TextInputDrawer(const UIDrawConfig& config, UIDrawerTextInput* input, float2 position, float2 scale) {
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_TEXT_INPUT_NO_NAME)) {
					ElementName<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(config, &input->name, position, scale);
					HandleTransformFlags<configuration>(config, position, scale);
				}

				if constexpr (configuration & UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER) {
					if (!input->is_currently_selected) {
						int64_t integer = function::ConvertCharactersToInt<int64_t>(*input->text);
						input->DeleteAllCharacters();

						char temp_characters[256];
						CapacityStream<char> temp_stream(temp_characters, 0, 256);
						function::ConvertIntToCharsFormatted(temp_stream, integer);

						input->InsertCharacters(temp_characters, temp_stream.size, 0, system);
					}
				}

				HandleFitSpaceRectangle<configuration>(position, scale);

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {
					auto text_stream = GetTextStream<configuration>(input->vertices.size);
					UISpriteVertex* text_buffer = HandleTextSpriteBuffer<configuration>();
					size_t* text_count = HandleTextSpriteCount<configuration>();

					float2 font_size;
					float character_spacing;
					Color font_color;
					HandleText<configuration>(config, font_color, font_size, character_spacing);
					HandleTextStreamColorUpdate(font_color, input->vertices);
					input->text_color = font_color;

					bool is_x_zoom_different = input->current_zoom.x != zoom_ptr->x;
					bool is_y_zoom_different = input->current_zoom.y != zoom_ptr->y;
					if (input->vertices.size > 0) {
						if (input->position.x != position.x || input->position.y != position.y || is_x_zoom_different || is_y_zoom_different) {
							TranslateText(position.x + element_descriptor.text_input_padding.x, position.y + element_descriptor.text_input_padding.y, input->vertices, 0, 0);
							if constexpr (configuration & UI_CONFIG_TEXT_INPUT_HINT) {
								TranslateText(position.x + element_descriptor.text_input_padding.x, position.y + element_descriptor.text_input_padding.y, input->hint_vertices, 0, 0);
							}
						}
						ScaleText(input->vertices, input->current_zoom, input->inverse_zoom, text_buffer, text_count, zoom_ptr, font.character_spacing);
						unsigned int visible_sprites = input->GetVisibleSpriteCount(position.x + scale.x - input->padding.x);

						if (input->current_zoom.x != zoom_ptr->x || input->current_zoom.y != zoom_ptr->y) {
							memcpy(input->vertices.buffer, text_stream.buffer, input->vertices.size * sizeof(UISpriteVertex));

							if constexpr (configuration & UI_CONFIG_TEXT_INPUT_HINT) {
								size_t text_copy = *text_count;
								ScaleText(input->hint_vertices, input->current_zoom, input->inverse_zoom, text_buffer, text_count, zoom_ptr, font.character_spacing);
								memcpy(input->hint_vertices.buffer, text_buffer + text_copy, input->hint_vertices.size * sizeof(UISpriteVertex));
								*text_count -= input->hint_vertices.size;
							}
							input->SetNewZoom(*zoom_ptr);
							if (visible_sprites < input->text->size) {
								size_t visible_offset = input->sprite_render_offset * 6;
								for (size_t index = *text_count; index < *text_count + visible_sprites * 6; index++) {
									text_buffer[index] = text_buffer[index + visible_offset];
								}
							}
						}
						else {
							if (visible_sprites == input->text->size) {
								memcpy(text_buffer + *text_count, input->vertices.buffer, input->vertices.size * sizeof(UISpriteVertex));
							}
							else {
								memcpy(text_buffer + *text_count, input->vertices.buffer + input->sprite_render_offset * 6, visible_sprites * 6 * sizeof(UISpriteVertex));
								text_stream.size = visible_sprites * 6;
								TranslateText(position.x + input->padding.x, position.y + input->padding.y, text_stream, 0, 0);
							}
							*text_count += visible_sprites * 6;
						}
					}
					else {
						if constexpr (configuration & UI_CONFIG_TEXT_INPUT_HINT) {
							if (input->position.x != position.x || input->position.y != position.y || is_x_zoom_different || is_y_zoom_different) {
								TranslateText(position.x + element_descriptor.text_input_padding.x, position.y + element_descriptor.text_input_padding.y, input->hint_vertices, 0, 0);
							}
						}
						if (is_x_zoom_different || is_y_zoom_different) {
							if constexpr (configuration & UI_CONFIG_TEXT_INPUT_HINT) {
								size_t text_copy = *text_count;
								ScaleText(input->hint_vertices, input->current_zoom, input->inverse_zoom, text_buffer, text_count, zoom_ptr, font.character_spacing);
								memcpy(input->hint_vertices.buffer, text_buffer + text_copy, input->hint_vertices.size * sizeof(UISpriteVertex));
								if (input->is_currently_selected) {
									*text_count -= input->hint_vertices.size;
								}
							}
							input->SetNewZoom(*zoom_ptr);
						}
						else {
							if (!input->is_currently_selected) {
								size_t valid_vertices = CullTextSprites(input->hint_vertices, position.x + scale.x - element_descriptor.text_input_padding.x);
								memcpy(text_buffer + *text_count, input->hint_vertices.buffer, valid_vertices * sizeof(UISpriteVertex));
								*text_count += valid_vertices;
							}
						}
					}

					if (input->is_caret_display) {
						float2 caret_position = input->GetCaretPosition();
						system->ConvertCharactersToTextSprites(
							"|",
							caret_position,
							text_buffer,
							1,
							input->text_color,
							*text_count,
							input->font_size,
							input->character_spacing
						);
						*text_count += 6;
					}

					input->position = position;
					input->bound = position.x + scale.x - input->padding.x;
					if constexpr (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
						scale.y = input->solid_color_y_scale;
					}
					Color color = HandleColor<configuration>(config);
					SolidColorRectangle<configuration>(position, scale, color);
					AddHoverable(position, scale, { TextInputHoverable, input, 0 });
					AddClickable(position, scale, { TextInputClickable, input, 0 });
					AddGeneral(position, scale, { TextInputAction<Filter>, input, 0 });

					if (input->current_selection != input->current_sprite_position) {
						Color select_color = ToneColor(color, color_theme.select_text_factor);
						if (input->current_selection < input->current_sprite_position) {
							float2 first_position = input->GetPositionFromSprite(input->current_selection);
							float2 last_position = input->GetPositionFromSprite<false>(input->current_sprite_position - 1);
							if (last_position.x > input->bound) {
								last_position.x = input->bound;
							}
							if (first_position.x < position.x + input->padding.x) {
								first_position.x = position.x + input->padding.x;
							}

							SolidColorRectangle<configuration>(first_position, { last_position.x - first_position.x, last_position.y - first_position.y }, select_color);
							if (input->is_caret_display) {
								*text_count -= 6;
								system->ConvertCharactersToTextSprites(
									"|",
									{ last_position.x, first_position.y },
									text_buffer,
									1,
									input->text_color,
									*text_count,
									input->font_size,
									input->character_spacing
								);
								*text_count += 6;
							}
						}
						else {
							float2 first_position = input->GetPositionFromSprite(input->current_sprite_position);
							float2 last_position = input->GetPositionFromSprite<false>(input->current_selection - 1);
							if (last_position.x > input->bound) {
								last_position.x = input->bound;
							}
							if (first_position.x < position.x + input->padding.x) {
								first_position.x = position.x + input->padding.x;
							}

							SolidColorRectangle<configuration>(first_position, { last_position.x - first_position.x, last_position.y - first_position.y }, select_color);
							if (input->is_caret_display) {
								*text_count -= 6;
								system->ConvertCharactersToTextSprites(
									"|",
									{ first_position.x, first_position.y },
									text_buffer,
									1,
									input->text_color,
									*text_count,
									input->font_size,
									input->character_spacing
								);
								*text_count += 6;
							}
						}
					}

					HandleBorder<configuration>(config, position, scale);
				}

				FinalizeRectangle<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(position, scale);

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_TEXT_INPUT_NO_NAME)) {
					ElementName<configuration>(config, &input->name, position, scale);
				}
			}

			template<size_t configuration>
			UIDrawerTextInput* TextInputInitializer(UIDrawConfig& config, const char* name, CapacityStream<char>* text_to_fill, float2 position, float2 scale) {
				UIDrawerTextInput* element;
				AddWindowResource(name, [&](const char* identifier) {

					float character_spacing;
					float2 font_size;
					Color font_color;
					UIConfigTextParameters previous_parameters;

					FitTextToScale<configuration>(config, font_size, character_spacing, font_color, scale, element_descriptor.text_input_padding.y, previous_parameters);

					element = GetMainAllocatorBuffer<UIDrawerTextInput>();		

					ConvertTextToWindowResource<configuration | UI_CONFIG_TEXT_PARAMETERS>(
						config, 
						identifier, 
						&element->name,
						{ position.x + scale.x + element_descriptor.label_horizontal_padd, position.y + scale.y + element_descriptor.label_vertical_padd }
					);

					FinishFitTextToScale<configuration>(config, previous_parameters);

					element->caret_start = std::chrono::high_resolution_clock::now();
					element->key_repeat_start = std::chrono::high_resolution_clock::now();
					element->repeat_key_count = 0;
					element->is_caret_display = false;
					element->character_spacing = character_spacing;
					element->current_selection = 0;
					element->current_sprite_position = 0;
					element->solid_color_y_scale = system->GetTextSpriteYScale(font_size.y) + 2.0f * element_descriptor.text_input_padding.y;
					element->position = position;
					element->padding = element_descriptor.text_input_padding;
					element->text_color = font_color;
					element->inverse_zoom = zoom_inverse;
					element->current_zoom = *zoom_ptr;
					element->font_size = font_size;
					element->sprite_render_offset = 0;
					element->suppress_arrow_movement = false;
					element->word_click_count = 0;
					element->bound = position.x + scale.x - element_descriptor.text_input_padding.x;
					element->text = text_to_fill;
					element->filter_characters_start = 0;
					element->filter_character_count = 0;
					element->is_currently_selected = false;
					element->vertices = CapacityStream<UISpriteVertex>(GetMainAllocatorBuffer(sizeof(UISpriteVertex) * text_to_fill->capacity * 6), 0, text_to_fill->capacity * 6);
					
					if (text_to_fill->size > 0) {
						unsigned int character_count = text_to_fill->size;
						text_to_fill->size = 0;
						element->InsertCharacters(text_to_fill->buffer, character_count, 0, system);
						element->sprite_render_offset = 0;
					}

					if constexpr (configuration & UI_CONFIG_TEXT_INPUT_HINT) {
						const char* hint = *(const char**)config.GetParameter(UI_CONFIG_TEXT_INPUT_HINT);
						size_t hint_size = strlen(hint) * 6;
						element->hint_vertices.buffer = (UISpriteVertex*)GetMainAllocatorBuffer(sizeof(UISpriteVertex) * hint_size);
						element->hint_vertices.size = hint_size;
						element->hint_vertices.capacity = hint_size;

						Color hint_color = DarkenColor(font_color, 0.75f);
						hint_color.alpha = 150;
						system->ConvertCharactersToTextSprites(
							hint,
							{ position.x + element_descriptor.text_input_padding.x, position.y + element_descriptor.text_input_padding.y },
							element->hint_vertices.buffer,
							hint_size / 6,
							hint_color,
							0,
							font_size,
							character_spacing
						);
					}
					else {
						element->hint_vertices.buffer = nullptr;
						element->hint_vertices.size = 0;
						element->hint_vertices.capacity = 0;
					}

					if constexpr (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
						const UIActionHandler* handler = (const UIActionHandler*)config.GetParameter(UI_CONFIG_TEXT_INPUT_CALLBACK);
						if (handler->data_size > 0) {
							element->callback_data = GetMainAllocatorBuffer(handler->data_size);
							memcpy(element->callback_data, handler->data, handler->data_size);
						}
						else {
							element->callback_data = handler->data;
						}
						element->callback = handler->action;
					}
					else {
						element->callback = nullptr;
						element->callback_data = nullptr;
					}

					FinalizeRectangle<configuration>(position, scale);
					return element;
				});

				return element;
			}

			template<size_t configuration, typename Value>
			UIDrawerSlider* SliderInitializer(
				UIDrawConfig& config, 
				const char* name, 
				float2 position, 
				float2 scale, 
				Value value_to_modify, 
				Value lower_bound, 
				Value upper_bound,
				Value default_value
			) {
				UIDrawerSlider* slider;
				AddWindowResource(
					name, [&](const char* identifier) {
						Color font_color;
						float character_spacing;
						float2 font_size;
						UIConfigTextParameters previous_parameters;

						float slider_padding, slider_length;
						float2 slider_shrink;
						Color slider_color;
						// color here is useless
						HandleSliderVariables<configuration>(config, slider_length, slider_padding, slider_shrink, slider_color, ECS_COLOR_WHITE);

						HandleText<configuration>(config, font_color, font_size, character_spacing);

						float sprite_y_scale = system->GetTextSpriteYScale(font_size.y);

						slider = GetMainAllocatorBuffer<UIDrawerSlider>();

						float alignment;
						if constexpr (~configuration & UI_CONFIG_SLIDER_NO_NAME) {
							if constexpr (~configuration & UI_CONFIG_VERTICAL) {
								alignment = AlignMiddle(position.y, scale.y, sprite_y_scale);
								ConvertTextToWindowResource<configuration>(config, identifier, &slider->label, { position.x + scale.x + slider_padding + slider_length, alignment });
							}
							else {
								ConvertTextToWindowResource<configuration>(config, identifier, &slider->label, { position.x, position.y + scale.y + slider_padding + slider_length });
								alignment = AlignMiddle(position.x, scale.x, slider->TextScale()->x);
								if (*slider->TextSize() > 0) {
									TranslateText(alignment, position.y + scale.y + slider_padding + slider_length, *slider->TextStream());
								}
							}
						}

						slider->initial_scale = scale;
						slider->current_scale = scale;
						slider->current_position = position;
						slider->changed_value = false;
						slider->character_value = false;
						slider->interpolate_value = false;

						if constexpr (configuration & UI_CONFIG_VERTICAL) {
							slider->is_vertical = true;
						}
						else {
							slider->is_vertical = false;
						}

						void* character_allocation = GetMainAllocatorBuffer(sizeof(char) * 32, 1);
						slider->characters.buffer = (char*)character_allocation;
						slider->characters.size = 0;
						slider->characters.capacity = 32;

						if (*value_to_modify.Pointer() < lower_bound.Value() && *value_to_modify.Pointer() > upper_bound.Value()) {
							slider->slider_position = 0.5f;
							*value_to_modify.Pointer() = Value::Interpolate(lower_bound, upper_bound, 0.5f);
						}
						else {
							slider->slider_position = Value::Percentage(lower_bound, upper_bound, value_to_modify);
						}

						slider->text_input_counter = 0;
						slider->value_to_change = value_to_modify.Pointer();

						if constexpr (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
							const UIConfigSliderChangedValueCallback* callback = (const UIConfigSliderChangedValueCallback*)config.GetParameter(UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);
							slider->changed_value_callback = callback->handler;
						}
						else {
							slider->changed_value_callback = { nullptr, nullptr, 0 };
						}

						if constexpr (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
							size_t name_size = strlen(identifier);

							UIActionHandler previous_callback;
							UIConfigTextInputCallback input_callback;
							input_callback.handler = slider->changed_value_callback;
							if constexpr (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
								config.SetExistingFlag(input_callback, UI_CONFIG_TEXT_INPUT_CALLBACK, previous_callback);
							}
							else {
								config.AddFlag(input_callback);
							}

							// TextInput - 9 chars
							char stack_memory[256];
							memcpy(stack_memory, identifier, name_size);
							stack_memory[name_size] = '\0';
							strcat(stack_memory, "TextInput");
							stack_memory[name_size + 9] = '\0';

							if constexpr (~configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
								slider->text_input = TextInputInitializer<configuration | UI_CONFIG_TEXT_INPUT_NO_NAME>(config, stack_memory, &slider->characters, position, scale);
							}
							else {
								slider->text_input = TextInputInitializer<configuration | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_TEXT_INPUT_CALLBACK>(config, stack_memory, &slider->characters, position, scale);
							}

							RemoveConfigParameter<configuration, UI_CONFIG_TEXT_INPUT_CALLBACK>(config, previous_callback);
						}

						slider->value_byte_size = Value::ByteSize();
						if constexpr (configuration & UI_CONFIG_SLIDER_DEFAULT_VALUE) {
							void* allocation = GetMainAllocatorBuffer(slider->value_byte_size);
							auto def_value = default_value.Value();
							memcpy(allocation, &def_value, slider->value_byte_size);
							slider->default_value = allocation;
						}

						if constexpr (configuration & UI_CONFIG_VERTICAL) {
							slider->total_length_main_axis = scale.y + slider->TextScale()->y + slider_length + slider_padding;
							FinalizeRectangle<configuration>(position, { scale.x, slider->total_length_main_axis });
						}
						else {
							slider->total_length_main_axis = scale.x + slider->TextScale()->x + slider_length + slider_padding;
							FinalizeRectangle<configuration>(position, { slider->total_length_main_axis, scale.y });
						}


						return slider;
					}
				);
				return slider;
			}

			template<size_t configuration, typename EnterValuesFilter = TextFilterAll, typename Value>
			void SliderDrawer(
				const UIDrawConfig& config, 
				UIDrawerSlider* slider,
				float2 position, 
				float2 scale,
				Value value_to_modify, 
				Value lower_bound,
				Value upper_bound,
				Value default_value
			) {
				bool is_null_window_dependent_size = false;

				float slider_padding, slider_length;
				float2 slider_shrink;
				Color slider_color;

				Color color = HandleColor<configuration>(config);
				HandleSliderVariables<configuration>(config, slider_length, slider_padding, slider_shrink, slider_color, color);

				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_SLIDER_NO_NAME)) {
					ElementName<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(config, &slider->label, position, scale);
					HandleTransformFlags<configuration>(config, position, scale);
				}

				if (ValidatePosition<configuration>(position)) {
					if (slider->interpolate_value) {
						*value_to_modify.Pointer() = Value::Interpolate(lower_bound, upper_bound, slider->slider_position);
					}
					else if (slider->character_value) {
						Value::ConvertTextInput(slider->characters, value_to_modify.Pointer());
						if (*value_to_modify.ConstPointer() > upper_bound.Value()) {
							*value_to_modify.Pointer() = upper_bound.Value();
						}
						else if (*value_to_modify.ConstPointer() < lower_bound.Value()) {
							*value_to_modify.Pointer() = lower_bound.Value();
						}
					}
					else {
						if (*value_to_modify.ConstPointer() > upper_bound.Value()) {
							*value_to_modify.Pointer() = upper_bound.Value();
						}
						else if (*value_to_modify.ConstPointer() < lower_bound.Value()) {
							*value_to_modify.Pointer() = lower_bound.Value();
						}
						slider->slider_position = Value::Percentage(lower_bound, upper_bound, value_to_modify);
					}

					WindowSizeTransformType* type = nullptr;
					float2 copy_position = position;

					if constexpr (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
						if constexpr (configuration & UI_CONFIG_VERTICAL) {
							HandleFitSpaceRectangle<configuration>(position, { scale.x, scale.y + 2 * slider_padding + slider_length });

							if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
								UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
								*get_transform->position = position;
								*get_transform->scale = {scale.x, scale.y + 2 * slider_padding + slider_length};
							}
						}
						else {
							HandleFitSpaceRectangle<configuration>(position, { scale.x + 2 * slider_padding + slider_length, scale.y });

							if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
								UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
								*get_transform->position = position;
								*get_transform->scale = {scale.x + 2 * slider_padding + slider_length, scale.y};
							}
						}
					}
					else {
						if constexpr (configuration & UI_CONFIG_VERTICAL) {
							scale.y -= slider_padding + slider_length;
						}
						else {
							scale.x -= slider_padding + slider_length;
						}
						const float* window_transform = config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
						type = (WindowSizeTransformType*)window_transform;
					}

					float2 old_scale = scale;

					if constexpr (configuration & UI_CONFIG_VERTICAL) {
						scale.y += slider_length;
					}
					else {
						scale.x += slider_length;
					}
					if constexpr (~configuration & UI_CONFIG_SLIDER_NO_TEXT) {
						auto text_label_lambda = [&]() {
							value_to_modify.ToString(slider->characters);
							slider->characters[slider->characters.size] = '\0';
							FixedScaleTextLabel<configuration
								| UI_CONFIG_DO_NOT_CACHE
								| UI_CONFIG_DO_NOT_NULLIFY_HOVERABLE_ACTION
								| UI_CONFIG_DO_NOT_NULLIFY_GENERAL_ACTION
								| UI_CONFIG_DO_NOT_NULLIFY_CLICKABLE_ACTION
							>(
								config,
								slider->characters.buffer,
								position,
								scale
							);

							AddHoverable(position, scale, { SliderCopyPaste, slider, 0 });
						};
						if constexpr (~configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
							text_label_lambda();
						}
						else {
							if (slider->text_input_counter == 0) {
								text_label_lambda();
							}
							else {
								if (slider->text_input_counter == 1) {
									value_to_modify.ToString(slider->characters);
									system->ConvertCharactersToTextSprites(
										slider->characters.buffer,
										{ position.x + element_descriptor.text_input_padding.x, position.y + element_descriptor.text_input_padding.y },
										slider->text_input->vertices.buffer,
										slider->characters.size,
										slider->text_input->text_color,
										0,
										slider->text_input->font_size,
										slider->text_input->character_spacing
									);
									slider->text_input->vertices.size = slider->characters.size * 6;
									slider->text_input->current_selection = 0;
									slider->text_input->current_sprite_position = slider->characters.size;
								}
								slider->text_input_counter++;
								TextInputDrawer<configuration | UI_CONFIG_TEXT_INPUT_NO_NAME, EnterValuesFilter>(config, slider->text_input, position, scale);
								Indent(-1.0f);
							}
						}
					}
					else {
						SolidColorRectangle<configuration>(position, scale, color);
					}

					float2 slider_position = { 0.0f, 0.0f };
					float2 slider_scale = { -10.0f, -10.0f };
					if constexpr (~configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
						if constexpr (configuration & UI_CONFIG_VERTICAL) {
							slider_scale.x = scale.x - 2 * slider_shrink.x;
							slider_scale.y = slider_length;
							slider_position.x = AlignMiddle(position.x, scale.x, slider_scale.x);
							slider_position.y = position.y + slider_padding + (old_scale.y - slider_padding * 2) * slider->slider_position;
						}
						else {
							slider_scale.x = slider_length;
							slider_scale.y = scale.y - 2 * slider_shrink.y;
							slider_position.x = position.x + slider_padding + (old_scale.x - slider_padding * 2) * slider->slider_position;
							slider_position.y = AlignMiddle(position.y, scale.y, slider_scale.y);
						}
					}

					HandleLateAndSystemDrawActionNullify<configuration>(position, scale);

					if constexpr (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
						if (*type == WindowSizeTransformType::Both) {
							if constexpr (configuration & UI_CONFIG_VERTICAL) {
								if (scale.y <= 0.0f) {
									is_null_window_dependent_size = true;
								}
							}
							else {
								if (scale.x <= 0.0f) {
									is_null_window_dependent_size = true;
								}
							}
						}
						if constexpr (configuration & UI_CONFIG_VERTICAL) {
							if constexpr (~configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
								if (slider_scale.y < scale.y - slider_padding) {
									HandleSliderActions<configuration, Value, EnterValuesFilter>(position, scale, color, slider_position, slider_scale, slider_color, slider);
								}
							}
						}
						else {
							if (slider_scale.x < scale.x - slider_padding) {
								HandleSliderActions<configuration, Value, EnterValuesFilter>(position, scale, color, slider_position, slider_scale, slider_color, slider);
							}
						}
					}
					else {
						HandleSliderActions<configuration, Value, EnterValuesFilter>(position, scale, color, slider_position, slider_scale, slider_color, slider);
					}

					if constexpr (configuration & UI_CONFIG_VERTICAL) {
						if constexpr (~configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
							slider->current_scale = { scale.x, scale.y - slider_length - slider_padding };
						}
						else {
							slider->current_scale = scale;
						}
						slider->current_position = { position.x, position.y + slider_padding };
					}
					else {
						if constexpr (~configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
							slider->current_scale = { scale.x - slider_length - slider_padding, scale.y };
						}
						else {
							slider->current_scale = scale;
						}
						slider->current_position = { position.x + slider_padding, position.y };
					}
				}

				if (slider->changed_value) {
					if (slider->changed_value_callback.action != nullptr) {
						ActionData action_data;
						action_data.border_index = border_index;
						action_data.buffers = buffers;
						action_data.counts = counts;
						action_data.dockspace = dockspace;
						action_data.dockspace_mask = system->GetDockspaceMaskFromType(dockspace_type);
						action_data.keyboard = system->m_keyboard;
						action_data.keyboard_tracker = system->m_keyboard_tracker;
						action_data.mouse = system->m_mouse->GetState();
						action_data.mouse_position = mouse_position;
						action_data.position = position;
						action_data.scale = scale;
						action_data.system = system;
						action_data.thread_id = thread_id;
						action_data.type = dockspace_type;
						action_data.data = slider->changed_value_callback.data;
						slider->changed_value_callback.action(&action_data);
					}
					slider->changed_value = false;
				}

				if constexpr (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					FinalizeRectangle<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(position, scale);
				}
				else {
					if (!is_null_window_dependent_size) {
						FinalizeRectangle<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(position, scale);
					}
				}
				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_SLIDER_NO_NAME)) {
					//HandleTransformFlags<configuration>(config, position, scale);
					ElementName<configuration>(config, &slider->label, position, scale);
				}
			}

			template<size_t configuration, typename EnterValuesFilter = TextFilterAll, typename Value>
			void SliderGroupDrawer(
				UIDrawConfig& config,
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				Value* ECS_RESTRICT values_to_modify,
				const Value* ECS_RESTRICT lower_bounds,
				const Value* ECS_RESTRICT upper_bounds,
				const Value* ECS_RESTRICT default_values,
				float2 position,
				float2 scale
			) {

#define LABEL_CONFIGURATION configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
#define LABEL_CONFIGURATION_LAST configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_SLIDER_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION>(config, group_name);
					Indent(-1.0f);
				}

				char temp_name[128];
				size_t group_name_size = strlen(group_name);
				memcpy(temp_name, group_name, group_name_size);

				for (size_t index = 0; index < count; index++) {
					UIDrawerSlider* slider;
					if constexpr (configuration & UI_CONFIG_SLIDER_GROUP_NO_SUBNAMES) {
						temp_name[group_name_size] = '\0';
						size_t random_index = (index * index + (index & 15)) << (index & 3);
						function::ConvertIntToChars(Stream<char>(temp_name, group_name_size), random_index);
						slider = (UIDrawerSlider*)GetResource(temp_name);
					}
					else {
						slider = (UIDrawerSlider*)GetResource(names[index]);
					}
					
					HandleTransformFlags<configuration>(config, position, scale);
					Value lower_bound, upper_bound, default_value;
					if constexpr (configuration & UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS) {
						lower_bound = lower_bounds[0];
						upper_bound = upper_bounds[0];
					}
					else {
						lower_bound = lower_bounds[index];
						upper_bound = upper_bounds[index];
					}
					
					if constexpr (configuration & UI_CONFIG_SLIDER_GROUP_UNIFORM_DEFAULT) {
						default_value = default_values[0];
					}
					else {
						default_value = default_values[index];
					}

					if constexpr (~configuration & UI_CONFIG_SLIDER_GROUP_NO_SUBNAMES) {
						SliderDrawer<configuration | UI_CONFIG_ELEMENT_NAME_FIRST, EnterValuesFilter, Value>(
							config,
							slider,
							position,
							scale,
							values_to_modify[index],
							lower_bound,
							upper_bound,
							default_value
							);
					}
					else {
						SliderDrawer<configuration | UI_CONFIG_SLIDER_NO_NAME, EnterValuesFilter, Value>(
							config,
							slider,
							position,
							scale,
							values_to_modify[index],
							lower_bound,
							upper_bound,
							default_value
						);
					}
				}

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_SLIDER_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION_LAST>(config, group_name);
				}
#undef LABEL_CONFIGURATION
#undef LABEL_CONFIGURATION_LAST
			}

			template<size_t configuration, typename Value>
			void SliderGroupInitializer(
				UIDrawConfig& config,
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				Value* ECS_RESTRICT values_to_modify,
				const Value* ECS_RESTRICT lower_bounds,
				const Value* ECS_RESTRICT upper_bounds,
				const Value* ECS_RESTRICT default_values,
				float2 position,
				float2 scale
			) {
#define LABEL_CONFIGURATION configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_SLIDER_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION>(config, group_name);
				}

				char temp_name[128];
				size_t group_name_size = strlen(group_name);
				memcpy(temp_name, group_name, group_name_size);

				for (size_t index = 0; index < count; index++) {
					Value lower_bound, upper_bound, default_value;
					if constexpr (configuration & UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS) {
						lower_bound = lower_bounds[0];
						upper_bound = upper_bounds[0];
					}
					else {
						lower_bound = lower_bounds[index];
						upper_bound = upper_bounds[index];
					}

					if constexpr (configuration & UI_CONFIG_SLIDER_GROUP_UNIFORM_DEFAULT) {
						default_value = default_values[0];
					}
					else {
						default_value = default_values[index];
					}

					if constexpr (~configuration & UI_CONFIG_SLIDER_GROUP_NO_SUBNAMES) {
						SliderInitializer<configuration | UI_CONFIG_ELEMENT_NAME_FIRST, Value>(
							config,
							names[index],
							position,
							scale,
							values_to_modify[index],
							lower_bound,
							upper_bound,
							default_value
							);
					}
					else {
						temp_name[group_name_size] = '\0';
						size_t random_index = (index * index + (index & 15)) << (index & 3);
						function::ConvertIntToChars(Stream<char>(temp_name, group_name_size), random_index);
						SliderInitializer<configuration | UI_CONFIG_SLIDER_NO_NAME, Value>(
							config,
							temp_name,
							position,
							scale,
							values_to_modify[index],
							lower_bound,
							upper_bound,
							default_value
							);
					}
				}

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_SLIDER_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION>(config, group_name);
				}
#undef LABEL_CONFIGURATION
			}

			template<size_t configuration, typename Integer>
			UIDrawerSlider* IntSliderInitializer(
				UIDrawConfig& config,
				const char* name,
				float2 position,
				float2 scale,
				Integer* value_to_modify,
				Integer lower_bound,
				Integer upper_bound,
				Integer default_value = Integer(0)
			) {
				SliderInteger<Integer> _value_to_modify, _lower_bound, _upper_bound, _default_value;
				_value_to_modify.ConstructPointer(value_to_modify);
				_lower_bound.ConstructValue(lower_bound);
				_upper_bound.ConstructValue(upper_bound);
				_default_value.ConstructValue(default_value);
				return SliderInitializer<configuration>(config, name, position, scale, _value_to_modify, _lower_bound, _upper_bound, _default_value);
			}

			template<size_t configuration, typename Integer>
			void IntSliderDrawer(
				const UIDrawConfig& config,
				UIDrawerSlider* slider, 
				float2 position,
				float2 scale,
				Integer* value_to_modify, 
				Integer lower_bound, 
				Integer upper_bound,
				Integer default_value = Integer(0)
			) {
				SliderInteger<Integer> _value_to_modify, _lower_bound, _upper_bound, _default_value;
				_value_to_modify.ConstructPointer(value_to_modify);
				_lower_bound.ConstructValue(lower_bound);
				_upper_bound.ConstructValue(upper_bound);
				_default_value.ConstructValue(default_value);
				SliderDrawer<configuration>(config, slider, position, scale, _value_to_modify, _lower_bound, _upper_bound, _default_value);
			}

			template<size_t configuration>
			UIDrawerSlider* FloatSliderInitializer(
				UIDrawConfig& config,
				const char* name,
				float2 position,
				float2 scale,
				float* value_to_modify,
				float lower_bound,
				float upper_bound,
				unsigned int precision = 2,
				float default_value = 0.0f
			) {
				SliderFloat _value_to_modify, _lower_bound, _upper_bound, _default_value;
				_value_to_modify.ConstructPointer(value_to_modify);
				_lower_bound.ConstructValue(lower_bound);
				_upper_bound.ConstructValue(upper_bound);
				_default_value.ConstructValue(default_value);
				_value_to_modify.ConstructExtraData(&precision);
				_lower_bound.ConstructExtraData(&precision);
				_upper_bound.ConstructExtraData(&precision);
				_default_value.ConstructExtraData(&precision);
				return SliderInitializer<configuration>(config, name, position, scale, _value_to_modify, _lower_bound, _upper_bound, _default_value);
			}

			template<size_t configuration>
			void FloatSliderDrawer(
				const UIDrawConfig& config,
				UIDrawerSlider* slider,
				float2 position,
				float2 scale,
				float* value_to_modify,
				float lower_bound,
				float upper_bound,
				unsigned int precision = 2,
				float default_value = 0.0f
			) {
				SliderFloat _value_to_modify, _lower_bound, _upper_bound, _default_value;
				_value_to_modify.ConstructPointer(value_to_modify);
				_lower_bound.ConstructValue(lower_bound);
				_upper_bound.ConstructValue(upper_bound);
				_default_value.ConstructValue(default_value);
				_value_to_modify.ConstructExtraData(&precision);
				_lower_bound.ConstructExtraData(&precision);
				_upper_bound.ConstructExtraData(&precision);
				_default_value.ConstructExtraData(&precision);
				SliderDrawer<configuration>(config, slider, position, scale, _value_to_modify, _lower_bound, _upper_bound, _default_value);
			}

			template<size_t configuration>
			UIDrawerSlider* DoubleSliderInitializer(
				UIDrawConfig& config,
				const char* name,
				float2 position,
				float2 scale,
				double* value_to_modify,
				double lower_bound,
				double upper_bound,
				size_t precision = 2,
				double default_value = 0
			) {
				SliderDouble _value_to_modify, _lower_bound, _upper_bound, _default_value;
				_value_to_modify.ConstructPointer(value_to_modify);
				_lower_bound.ConstructValue(lower_bound);
				_upper_bound.ConstructValue(upper_bound);
				_default_value.ConstructValue(default_value);
				_value_to_modify.ConstructExtraData(&precision);
				_lower_bound.ConstructExtraData(&precision);
				_upper_bound.ConstructExtraData(&precision);
				_default_value.ConstructExtraData(&precision);
				return SliderInitializer<configuration>(config, name, position, scale, _value_to_modify, _lower_bound, _upper_bound, _default_value);
			}

			template<size_t configuration>
			void DoubleSliderDrawer(
				const UIDrawConfig& config,
				UIDrawerSlider* slider,
				float2 position,
				float2 scale,
				double* value_to_modify,
				double lower_bound,
				double upper_bound,
				size_t precision = 2,
				double default_value = 0
			) {
				SliderDouble _value_to_modify, _lower_bound, _upper_bound, _default_value;
				_value_to_modify.ConstructPointer(value_to_modify);
				_lower_bound.ConstructValue(lower_bound);
				_upper_bound.ConstructValue(upper_bound);
				_default_value.ConstructValue(default_value);
				_value_to_modify.ConstructExtraData(&precision);
				_lower_bound.ConstructExtraData(&precision);
				_upper_bound.ConstructExtraData(&precision);
				_default_value.ConstructExtraData(&precision);
				SliderDrawer<configuration>(config, slider, position, scale, _value_to_modify, _lower_bound, _upper_bound, _default_value);
			}

			template<size_t configuration>
			UIDrawerComboBox* ComboBoxInitializer(UIDrawConfig& config, const char* name, Stream<const char*> labels, unsigned int label_display_count, unsigned char* active_label, float2 position, float2 scale) {
				UIDrawerComboBox* data = nullptr;

				AddWindowResource(name, [&](const char* identifier) {
					data = GetMainAllocatorBuffer<UIDrawerComboBox>();

					data->is_opened = false;
					data->active_label = active_label;
					data->label_display_count = label_display_count;

					InitializeElementName<configuration, UI_CONFIG_COMBO_BOX_NO_NAME>(config, identifier, &data->name, position);

					void* allocation = GetMainAllocatorBuffer(sizeof(UIDrawerTextElement) * labels.size);

					data->labels.buffer = (UIDrawerTextElement*)allocation;
					data->labels.size = labels.size;
					
					if constexpr (configuration & UI_CONFIG_COMBO_BOX_PREFIX) {
						const UIConfigComboBoxPrefix* prefix = (const UIConfigComboBoxPrefix*)config.GetParameter(UI_CONFIG_COMBO_BOX_PREFIX);
						size_t prefix_size = strlen(prefix->prefix) + 1;
						void* allocation = GetMainAllocatorBuffer(sizeof(char) * prefix_size, alignof(char));
						memcpy(allocation, prefix->prefix, sizeof(char) * prefix_size);
						data->prefix = (const char*)allocation;

						float2 text_span = TextSpan(data->prefix);
						data->prefix_x_scale = text_span.x / zoom_ptr->x;
					}
					else {
						data->prefix = nullptr;
					}

					float current_max_x = 0.0f;
					for (size_t index = 0; index < labels.size; index++) {
						ConvertTextToWindowResource<configuration>(config, labels[index], &data->labels[index], { position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd });

						position.y += data->labels[index].scale.y + 2 * element_descriptor.label_vertical_padd;
						if (data->labels[index].scale.x + 2 * element_descriptor.label_horizontal_padd > current_max_x) {
							data->biggest_label_x_index = index;
							current_max_x = data->labels[index].scale.x + 2 * element_descriptor.label_horizontal_padd;
						}
					}

					if constexpr (configuration & UI_CONFIG_COMBO_BOX_CALLBACK) {
						const UIConfigComboBoxCallback* callback = (const UIConfigComboBoxCallback*)config.GetParameter(UI_CONFIG_COMBO_BOX_CALLBACK);
						data->callback = callback->handler.action;
						if (callback->handler.data_size > 0) {
							data->callback_data = GetMainAllocatorBuffer(callback->handler.data_size);
							memcpy(data->callback_data, callback->handler.data, callback->handler.data_size);
						}
						else {
							data->callback_data = callback->handler.data;
						}
					}
					else {
						data->callback = nullptr;
						data->callback_data = nullptr;
					}

					FinalizeRectangle<configuration>(position, scale);

					return data;
				});

				return data;
			}

			template<size_t configuration>
			UIDrawerTextElement* CheckBoxInitializer(const UIDrawConfig& config, const char* name, float2 position) {
				UIDrawerTextElement* element = nullptr;

				AddWindowResource(name, [&](const char* identifier) {
					element = GetMainAllocatorBuffer<UIDrawerTextElement>();

					InitializeElementName<configuration, UI_CONFIG_CHECK_BOX_NO_NAME>(config, identifier, element, position);

					return element;
				});

				return element;
			}

			template<size_t configuration>
			void CheckBoxDrawer(const UIDrawConfig& config, UIDrawerTextElement* element, bool* value_to_modify, float2 position, float2 scale) {
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_CHECK_BOX_NO_NAME)) {
					ElementName<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(config, element, position, scale);
					HandleTransformFlags<configuration>(config, position, scale);
				}

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {
					bool state = true;
					if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
						const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
						state = active_state->state;
					}

					Color color = HandleColor<configuration>(config);
					if (!state) {
						SolidColorRectangle<configuration>(position, scale, DarkenColor(color, color_theme.darken_inactive_item));
					}
					else {
						SolidColorRectangle<configuration>(position, scale, color);
						if (*value_to_modify) {
							Color check_color = ToneColor(color, color_theme.check_box_factor);
							SpriteRectangle<configuration>(position, scale, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, check_color);
						}
						AddDefaultClickableHoverable(position, scale, { BoolClickable, value_to_modify, 0 }, color);

						HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
					}

					FinalizeRectangle<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(position, scale);

					if constexpr (IsElementNameAfter(configuration, UI_CONFIG_CHECK_BOX_NO_NAME)) {
						ElementName<configuration>(config, element, position, scale);
					}
				}
			}

			template<size_t configuration>
			void CheckBoxDrawer(const UIDrawConfig& config, const char* name, bool* value_to_modify, float2 position, float2 scale) {
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_CHECK_BOX_NO_NAME)) {
					ElementName<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(config, name, position, scale);
					HandleTransformFlags<configuration>(config, position, scale);
				}

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {
					bool state = true;
					if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
						const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
						state = active_state->state;
					}

					Color color = HandleColor<configuration>(config);
					if (!state) {
						SolidColorRectangle<configuration>(position, scale, DarkenColor(color, color_theme.darken_inactive_item));
					}
					else {
						if (*value_to_modify) {
							Color check_color = ToneColor(color, color_theme.check_box_factor);
							SpriteRectangle<configuration>(position, scale, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, check_color, { 0.0f, 0.0f }, { 1.0f, 1.0f });
						}

						AddDefaultClickableHoverable(position, scale, { BoolClickable, value_to_modify, 0 }, color);

						HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
					}

					FinalizeRectangle<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(position, scale);

					if constexpr (IsElementNameAfter(configuration, UI_CONFIG_CHECK_BOX_NO_NAME)) {
						ElementName<configuration>(config, name, position, scale);
					}
				}
			}

			template<size_t configuration>
			void ComboBoxDrawer(UIDrawConfig& config, UIDrawerComboBox* data, unsigned char* active_label, float2 position, float2 scale) {
				bool is_active = true;
				if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_COMBO_BOX_NO_NAME)) {
					ElementName<configuration>(config, &data->name, position, scale);
					HandleTransformFlags<configuration>(config, position, scale);
				}

				UIConfigTextAlignment previous_alignment;
				UIConfigTextAlignment alignment;
				alignment.horizontal = TextAlignment::Left;
				alignment.vertical = TextAlignment::Middle;

				SetConfigParameter<configuration, UI_CONFIG_TEXT_ALIGNMENT>(config, alignment, previous_alignment);

				float2 border_position = position;
				float2 border_scale = scale;

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				float prefix_scale = 0.0f;
				if (ValidatePosition<configuration>(position, scale)) {
					if constexpr (configuration & UI_CONFIG_COMBO_BOX_PREFIX) {
						Stream<UISpriteVertex> vertices = GetTextStream<configuration>(strlen(data->prefix) * 6);
						constexpr size_t new_configuration = function::RemoveFlag(function::RemoveFlag(configuration, UI_CONFIG_BORDER), UI_CONFIG_GET_TRANSFORM)
							| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_CACHE;

						if (is_active) {
							TextLabel<new_configuration>(config, data->prefix, position, scale);
							float2 vertex_span = GetTextSpan(vertices);
							prefix_scale = vertex_span.x;
							position.x += prefix_scale;
							TextLabelDrawer<new_configuration | UI_CONFIG_LABEL_TRANSPARENT>(config, &data->labels[*data->active_label], position, scale);
						}
						else {
							TextLabel<new_configuration | UI_CONFIG_UNAVAILABLE_TEXT>(config, data->prefix, position, scale);
							float2 vertex_span = GetTextSpan(vertices);
							prefix_scale = vertex_span.x;
							position.x += prefix_scale;
							TextLabelDrawer<new_configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_UNAVAILABLE_TEXT>(config, &data->labels[*data->active_label], position, scale);
						}
					}
					else {
						constexpr size_t new_configuration = function::RemoveFlag(configuration, UI_CONFIG_GET_TRANSFORM) | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X
							| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_TEXT_ALIGNMENT;
						if (is_active) {
							TextLabelDrawer<new_configuration>(config, &data->labels[*data->active_label], position, scale);
						}
						else {
							TextLabelDrawer<new_configuration | UI_CONFIG_UNAVAILABLE_TEXT>(config, &data->labels[*data->active_label], position, scale);
						}
					}
					if constexpr (configuration & UI_CONFIG_COMBO_BOX_PREFIX) {
						position.x -= prefix_scale;
					}

					float2 positions[2];
					float2 scales[2];
					Color colors[2];
					float percentages[2];
					positions[0] = position;
					scales[0] = scale;

					Color color = HandleColor<configuration>(config);
					colors[0] = color;

					color = ToneColor(color, 1.35f);
					colors[1] = color;
					percentages[0] = 1.25f;
					percentages[1] = 1.25f;

					constexpr size_t no_get_transform_configuration = function::RemoveFlag(configuration, UI_CONFIG_GET_TRANSFORM);

					float2 triangle_scale = GetSquareScale(scale.y);
					float2 triangle_position = { position.x + scale.x - triangle_scale.x, position.y };
					SolidColorRectangle<no_get_transform_configuration>(triangle_position, triangle_scale, color);

					positions[1] = triangle_position;
					scales[1] = triangle_scale;

					if (is_active) {
						SpriteRectangle<no_get_transform_configuration>(triangle_position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE);

						UIDrawerComboBoxClickable clickable_data;
						clickable_data.config = config;
						clickable_data.box = data;
						data->label_y_scale = scale.y;
						data->starting_position.x = position.x;
						data->starting_position.y = position.y + scale.y + element_descriptor.combo_box_padding;

						AddDefaultHoverable(position, scale, positions, scales, colors, percentages, 2);
						AddClickable(position, scale, { ComboBoxClickable<configuration>, &clickable_data, sizeof(clickable_data), UIDrawPhase::System });

						if (data->is_opened) {
							if (data->window_index != -1) {
								if (!data->has_been_destroyed) {
									float2 window_position = { position.x, position.y + scale.y + element_descriptor.combo_box_padding };

									// only y axis needs to be handled since to move on the x axis
									// the left mouse button must be pressed
									float2 window_scale = { scale.x, scale.y * data->label_display_count };
									system->TrimPopUpWindow(data->window_index, window_position, window_scale, window_index, region_position, region_scale);
								}
								else {
									data->has_been_destroyed = false;
									data->window_index = -1;
									data->is_opened = false;
								}
							}
						}
						else {
							data->window_index = -1;
						}
					}
					else {
						SpriteRectangle<no_get_transform_configuration>(triangle_position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, color_theme.unavailable_text);
					}

					if constexpr (configuration & UI_CONFIG_COMBO_BOX_PREFIX) {
						HandleBorder<configuration>(config, border_position, border_scale);
					}

					RemoveConfigParameter<configuration, UI_CONFIG_TEXT_ALIGNMENT>(config, previous_alignment);
				}

				FinalizeRectangle<configuration>(position, scale);

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_COMBO_BOX_NO_NAME)) {
					//HandleTransformFlags<configuration>(config, position, scale);
					ElementName<configuration>(config, &data->name, position, scale);
				}
			}

			template<size_t configuration>
			UIDrawerColorInput* ColorInputInitializer(
				UIDrawConfig& config, 
				const char* name, 
				Color* color,
				Color default_color,
				float2 position,
				float2 scale
			) {
				UIDrawerColorInput* data = nullptr;
				
				AddWindowResource(name, [&](const char* identifier) {

					data = GetMainAllocatorBuffer<UIDrawerColorInput>();

					InitializeElementName<configuration, UI_CONFIG_COLOR_INPUT_NO_NAME>(config, identifier, &data->name, position);
					data->hsv = RGBToHSV(*color);
					data->rgb = color;
					data->default_color = default_color;

					unsigned int name_size = strlen(name);
					constexpr auto slider_lambda = [&]() {
						if constexpr (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
							return configuration | UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK;
						}
						else {
							return configuration | UI_CONFIG_SLIDER_NO_NAME;
						}
					};

					constexpr size_t slider_configuration = slider_lambda();

					if constexpr (configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) {
						UIConfigSliderChangedValueCallback previous_callback;
						if  constexpr (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
							UIConfigSliderChangedValueCallback callback;

							UIDrawerColorInputSliderCallback callback_data;
							callback_data.input = data;
							callback_data.is_rgb = true;
							callback_data.is_alpha = false;
							callback_data.is_hsv = false;

							callback.handler.action = ColorInputSliderCallback;
							callback.handler.data = &callback_data;
							callback.handler.data_size = sizeof(callback_data);

							if constexpr (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
								config.SetExistingFlag(callback, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK, previous_callback);
							}
							else {
								config.AddFlag(callback);
							}
						}

						char stack_memory[128];
						memcpy(stack_memory, name, name_size);
						stack_memory[name_size] = '\0';
						strcat(stack_memory, "Red");
						stack_memory[name_size + 3] = '\0';
						data->r_slider = IntSliderInitializer<slider_configuration>(config, stack_memory, position, scale, &color->red, (unsigned char)0, (unsigned char)255);
						stack_memory[name_size] = '\0';
						strcat(stack_memory, "Green");

						HandleTransformFlags<configuration>(config, position, scale);
						data->g_slider = IntSliderInitializer<slider_configuration>(config, stack_memory, position, scale, &color->green, (unsigned char)0, (unsigned char)255);

						stack_memory[name_size] = '\0';
						strcat(stack_memory, "Blue");

						HandleTransformFlags<configuration>(config, position, scale);
						data->b_slider = IntSliderInitializer<slider_configuration>(config, stack_memory, position, scale, &color->blue, (unsigned char)0, (unsigned char)255);			
						
						if constexpr (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
							if constexpr (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
								config.RestoreFlag(previous_callback, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);
							}
							else {
								config.flag_count--;
							}
						}
					}

					if constexpr (configuration & UI_CONFIG_COLOR_INPUT_HSV_SLIDERS) {
						UIConfigSliderChangedValueCallback previous_callback;
						if constexpr (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
							UIConfigSliderChangedValueCallback callback;

							UIDrawerColorInputSliderCallback callback_data;
							callback_data.input = data;
							callback_data.is_rgb = false;
							callback_data.is_alpha = false;
							callback_data.is_hsv = true;

							callback.handler.action = ColorInputSliderCallback;
							callback.handler.data = &callback_data;
							callback.handler.data_size = sizeof(callback_data);

							if constexpr (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
								config.SetExistingFlag(callback, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK, previous_callback);
							}
							else {
								config.AddFlag(callback);
							}
						}

						HandleTransformFlags<configuration>(config, position, scale);
						char stack_memory[128];
						memcpy(stack_memory, name, name_size);
						stack_memory[name_size] = '\0';
						strcat(stack_memory, "Hue");

						data->h_slider = IntSliderInitializer<slider_configuration>(config, stack_memory, position, scale, &data->hsv.hue, (unsigned char)0, (unsigned char)255);
						stack_memory[name_size] = '\0';
						strcat(stack_memory, "Saturation");

						HandleTransformFlags<configuration>(config, position, scale);
						data->s_slider = IntSliderInitializer<slider_configuration>(config, stack_memory, position, scale, &data->hsv.saturation, (unsigned char)0, (unsigned char)255);

						stack_memory[name_size] = '\0';
						strcat(stack_memory, "Value");

						HandleTransformFlags<configuration>(config, position, scale);
						data->v_slider = IntSliderInitializer<slider_configuration>(config, stack_memory, position, scale, &data->hsv.value, (unsigned char)0, (unsigned char)255);
						
						if constexpr (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
							if constexpr (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
								config.RestoreFlag(previous_callback, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);
							}
							else {
								config.flag_count--;
							}
						}
					}

					if constexpr (configuration & UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER) {
						UIConfigSliderChangedValueCallback previous_callback;
						if constexpr (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
							UIConfigSliderChangedValueCallback callback;

							UIDrawerColorInputSliderCallback callback_data;
							callback_data.input = data;
							callback_data.is_rgb = false;
							callback_data.is_alpha = true;
							callback_data.is_hsv = false;

							callback.handler.action = ColorInputSliderCallback;
							callback.handler.data = &callback_data;
							callback.handler.data_size = sizeof(callback_data);

							if constexpr (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
								config.SetExistingFlag(callback, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK, previous_callback);
							}
							else {
								config.AddFlag(callback);
							}
						}

						HandleTransformFlags<configuration>(config, position, scale);
						char stack_memory[128];
						memcpy(stack_memory, name, name_size);
						stack_memory[name_size] = '\0';
						strcat(stack_memory, "Alpha");
						stack_memory[name_size + 5] = '\0';
						data->a_slider = IntSliderInitializer<slider_configuration>(config, stack_memory, position, scale, &color->alpha, (unsigned char)0, (unsigned char)255);
						
						if constexpr (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
							if constexpr (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
								config.RestoreFlag(previous_callback, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);
							}
							else {
								config.flag_count--;
							}
						}
					}

					if constexpr (configuration & UI_CONFIG_COLOR_INPUT_HEX_INPUT) {
						data->hex_characters.buffer = (char*)GetMainAllocatorBuffer(sizeof(char) * 7);
						data->hex_characters.size = 0;
						data->hex_characters.capacity = 6;

						HandleTransformFlags<configuration>(config, position, scale);
						char stack_memory[256];
						memcpy(stack_memory, name, name_size);
						stack_memory[name_size] = '\0';
						strcat(stack_memory, "HexInput");
						stack_memory[name_size + 8] = '\0';

						UIConfigTextInputCallback callback;
						callback.handler.action = ColorInputHexCallback;
						callback.handler.data = data;
						callback.handler.data_size = 0;

						config.AddFlag(callback);
						data->hex_input = TextInputInitializer<configuration | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_TEXT_INPUT_CALLBACK>(config, stack_memory, &data->hex_characters, position, scale);
						config.flag_count--;
					}
					else {
						data->hex_characters.buffer = nullptr;
					}
					data->is_hex_input = false;

					if constexpr (configuration & UI_CONFIG_COLOR_INPUT_CALLBACK) {
						const UIConfigColorInputCallback* callback = (const UIConfigColorInputCallback*)config.GetParameter(UI_CONFIG_COLOR_INPUT_CALLBACK);
						
						if (callback->callback.data_size > 0) {
							void* allocation = GetMainAllocatorBuffer(callback->callback.data_size);
							memcpy(allocation, callback->callback.data, callback->callback.data_size);
							data->callback.data = allocation;
						}
						else {
							data->callback.data = callback->callback.data;
						}
						
						data->callback.action = callback->callback.action;
						data->callback.data_size = callback->callback.data_size;
					}
					else {
						data->callback.action = nullptr;
					}

					return data;
				});

				return data;
			}

			template<size_t configuration>
			void ColorInputDrawer(UIDrawConfig& config, UIDrawerColorInput* data, float2 position, float2 scale, Color* color, Color default_color) {
				if constexpr (configuration & UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE) {
					data->default_color = default_color;
				}
				
				Color initial_frame_color = *color;

#define LABEL_CONFIGURATION UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_TRANSPARENT \
				| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
#define LABEL_CONFIGURATION_UNAVAILABLE LABEL_CONFIGURATION | UI_CONFIG_UNAVAILABLE_TEXT
				
				UIDrawConfig label_config;

				Color text_color;
				float2 font_size;
				float character_spacing;
				HandleText<configuration>(config, text_color, font_size, character_spacing);

				UIConfigTextParameters text_params;
				text_params.color = text_color;
				text_params.character_spacing = character_spacing;
				text_params.size = { font_size.x, font_size.y / zoom_ptr->y };

				UIConfigTextAlignment text_alignment;
				text_alignment.horizontal = TextAlignment::Left;
				text_alignment.vertical = TextAlignment::Middle;

				label_config.AddFlags(text_params, text_alignment);
				//data->hsv.alpha = color->alpha;

				float2 end_point;
				float2 starting_point;
				float2 overall_start_position = position;
				float2 alpha_endpoint;

				bool is_active = true;
				if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

#define SLIDER(config, slider, position, scale, pointer) if constexpr (~configuration & UI_CONFIG_SLIDER_ENTER_VALUES) { \
					IntSliderDrawer<configuration | UI_CONFIG_SLIDER_NO_NAME>(config, slider, position, scale, pointer, (unsigned char)0, (unsigned char)255); } \
				else { \
					IntSliderDrawer<configuration | UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK>(config, slider, position, scale, pointer, (unsigned char)0, (unsigned char)255); \
				} 

				auto callback_lambda = [&](bool is_rgb, bool is_hsv, bool is_alpha) {
					if constexpr (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						UIConfigSliderChangedValueCallback callback;

						UIDrawerColorInputSliderCallback callback_data;
						callback_data.input = data;
						callback_data.is_rgb = is_rgb;
						callback_data.is_hsv = is_hsv;
						callback_data.is_alpha = is_alpha;

						callback.handler.action = ColorInputSliderCallback;
						callback.handler.data = &callback_data;
						callback.handler.data_size = sizeof(callback_data);

						config.AddFlag(callback);
					}
				};

				auto remove_callback_lambda = [&]() {
					if constexpr (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						config.flag_count--;
					}
				};

				auto rgb_lambda = [&]() {
					if (is_active) {
						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION>(label_config, "R:", position, scale);

						callback_lambda(true, false, false);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						starting_point = position;
						SLIDER(config, data->r_slider, position, scale, &color->red)

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION>(label_config, "G:", position, scale);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						SLIDER(config, data->g_slider, position, scale, &color->green);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION>(label_config, "B:", position, scale);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						SLIDER(config, data->b_slider, position, scale, &color->blue);
					}
					else {
						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION_UNAVAILABLE>(label_config, "R:", position, scale);

						callback_lambda(true, false, false);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						starting_point = position;
						SLIDER(config, data->r_slider, position, scale, &color->red)

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION_UNAVAILABLE>(label_config, "G:", position, scale);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						SLIDER(config, data->g_slider, position, scale, &color->green);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION_UNAVAILABLE>(label_config, "B:", position, scale);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						SLIDER(config, data->b_slider, position, scale, &color->blue);
					}
					end_point = { current_x - region_render_offset.x - layout.element_indentation, position.y };

					remove_callback_lambda();
				};

				auto hsv_lambda = [&]() {
					if (is_active) {
						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION>(label_config, "H:", position, scale);

						callback_lambda(false, true, false);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						starting_point = position;
						SLIDER(config, data->h_slider, position, scale, &data->hsv.hue);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION>(label_config, "S:", position, scale);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						SLIDER(config, data->s_slider, position, scale, &data->hsv.saturation);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION>(label_config, "V:", position, scale);
					}
					else {
						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION_UNAVAILABLE>(label_config, "H:", position, scale);

						callback_lambda(false, true, false);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						starting_point = position;
						SLIDER(config, data->h_slider, position, scale, &data->hsv.hue);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION_UNAVAILABLE>(label_config, "S:", position, scale);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						SLIDER(config, data->s_slider, position, scale, &data->hsv.saturation);

						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						TextLabel<LABEL_CONFIGURATION_UNAVAILABLE>(label_config, "V:", position, scale);
					}

					Indent(-1.0f);
					HandleTransformFlags<configuration>(config, position, scale);
					alpha_endpoint = { position.x + scale.x, position.y };
					SLIDER(config, data->v_slider, position, scale, &data->hsv.value);
					
					//end_point = { current_x - region_render_offset.x - layout.element_indentation, position.y };
					if constexpr (configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) {
						if (data->r_slider->interpolate_value || data->g_slider->interpolate_value || data->b_slider->interpolate_value || data->is_hex_input) {
							data->hsv = RGBToHSV(*color);
							//*color = HSVToRGB(data->hsv);
							data->is_hex_input = false;
						}
						else if (data->h_slider->interpolate_value || data->s_slider->interpolate_value || data->v_slider->interpolate_value) {
							*color = HSVToRGB(data->hsv);
						}
						//else {
						//	*color = HSVToRGB(data->hsv);
						//	//data->hsv = RGBToHSV(*color);
						//}
					}
					else {
						*color = HSVToRGB(data->hsv);
					}

					remove_callback_lambda();
				};

				auto alpha_lambda = [&]() {
					callback_lambda(false, false, true);

					if constexpr (((configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) == 0) && ((configuration & UI_CONFIG_COLOR_INPUT_HSV_SLIDERS) == 0)) {
						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						if (is_active) {
							TextLabel<LABEL_CONFIGURATION>(label_config, "A:", position, scale);
						}
						else {
							TextLabel<LABEL_CONFIGURATION_UNAVAILABLE>(label_config, "A:", position, scale);
						}
						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						SLIDER(config, data->a_slider, position, scale, &color->alpha);
					}
					else {
						NextRow();
						SetCurrentX(overall_start_position.x + region_render_offset.x);
						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						if (is_active) {
							TextLabel<LABEL_CONFIGURATION>(label_config, "A:", position, scale);
						}
						else {
							TextLabel<LABEL_CONFIGURATION_UNAVAILABLE>(label_config, "A:", position, scale);
						}
						Indent(-1.0f);
						HandleTransformFlags<configuration>(config, position, scale);
						scale.x = alpha_endpoint.x - starting_point.x - layout.element_indentation - system->NormalizeHorizontalToWindowDimensions(scale.y);
						SLIDER(config, data->a_slider, position, scale, &color->alpha);
					}
					

					if (data->a_slider->interpolate_value) {
						data->hsv.alpha = color->alpha;
					}

					remove_callback_lambda();
				};

				auto hex_lambda = [&]() {
					if constexpr (((configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) != 0) || (configuration & UI_CONFIG_COLOR_INPUT_HSV_SLIDERS != 0) || (configuration & UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER != 0)) {
						NextRow();
						SetCurrentX(overall_start_position.x + region_render_offset.x);

						HandleTransformFlags<configuration>(config, position, scale);
					}

					UIConfigTextInputCallback callback;
					callback.handler.action = ColorInputHexCallback;
					callback.handler.data = data;
					callback.handler.data_size = 0;

					config.AddFlag(callback);

					TextInputDrawer<configuration | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_TEXT_INPUT_CALLBACK, TextFilterHex>(
						config,
						data->hex_input,
						position,
						{ end_point.x - overall_start_position.x, scale.y}
					);

					if (!data->hex_input->is_currently_selected) {
						char hex_characters[6];
						Stream<char> temp(hex_characters, 0);
						RGBToHex(temp, *data->rgb);
						data->hex_input->DeleteAllCharacters();
						data->hex_input->InsertCharacters(hex_characters, 6, 0, GetSystem());
					}
				};

				if constexpr (configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) {
					rgb_lambda();
				}

				if constexpr (configuration & UI_CONFIG_COLOR_INPUT_HSV_SLIDERS) {
					if constexpr (configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) {
						NextRow();
						SetCurrentX(overall_start_position.x + region_render_offset.x);
					}
					hsv_lambda();
				}

				if constexpr (configuration & UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER) {
					alpha_lambda();
				}

				if constexpr (~configuration & UI_CONFIG_COLOR_INPUT_DO_NOT_CHOOSE_COLOR) {
					HandleTransformFlags<configuration>(config, position, scale);
				}

				AlphaColorRectangle<configuration | UI_CONFIG_MAKE_SQUARE | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(config, *color);

				if (is_active) {
					if constexpr (~configuration & UI_CONFIG_COLOR_INPUT_DO_NOT_CHOOSE_COLOR) {
						AddDefaultClickable(position, GetSquareScale(scale.y), { SkipAction, nullptr, 0 }, { ColorInputCreateWindow, data, 0, UIDrawPhase::System });
					}
					if constexpr (configuration & UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE) {
						AddHoverable(position, GetSquareScale(scale.y), { ColorInputDefaultColor, data, 0 });
					}
				}

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_COLOR_INPUT_NO_NAME)) {
					HandleTransformFlags<configuration>(config, position, scale);
					ElementName<configuration>(config, &data->name, position, scale);
				}

				if constexpr (configuration & UI_CONFIG_COLOR_INPUT_HEX_INPUT) {
					hex_lambda();
				}

#undef LABEL_CONFIGURATION
#undef SLIDER_CONFIGURATION
			}

			template<size_t configuration>
			UIDrawerCollapsingHeader* CollapsingHeaderInitializer(const UIDrawConfig& config, const char* name, float2 position, float2 scale) {
				UIDrawerCollapsingHeader* data = nullptr;

				AddWindowResource(name, [&](const char* identifier) {
					data = GetMainAllocatorBuffer<UIDrawerCollapsingHeader>();

					if constexpr (IsElementNameAfter(configuration, UI_CONFIG_COLLAPSING_HEADER_NO_NAME) || IsElementNameFirst(configuration, UI_CONFIG_COLLAPSING_HEADER_NO_NAME)) {
						ConvertTextToWindowResource<configuration>(config, identifier, &data->name, position);
					}

					data->state = false;
					return data;
				});

				return data;
			}

			template<size_t configuration>
			void CollapsingHeaderDrawer(UIDrawConfig& config, UIDrawerCollapsingHeader* data, float2 position, float2 scale) {
				if constexpr (~configuration & UI_CONFIG_COLLAPSING_HEADER_DO_NOT_INFER) {
					scale.x = GetXScaleUntilBorder(position.x);
				}

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {
					bool active_state = true;
					if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
						const UIConfigActiveState* _active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
						active_state = _active_state->state;
					}
					bool state = data->state;
					state &= active_state;

					bool is_selected = true;
					if constexpr (configuration & UI_CONFIG_COLLAPSING_HEADER_SELECTION) {
						const UIConfigCollapsingHeaderSelection* selection = (const UIConfigCollapsingHeaderSelection*)config.GetParameter(UI_CONFIG_COLLAPSING_HEADER_SELECTION);
						is_selected = selection->is_selected;
					}

					Color current_color = HandleColor<configuration>(config);
					UIConfigColor previous_color = { current_color };
					float2 triangle_scale = GetSquareScale(scale.y);
					float2 initial_position = position;
					
					if (is_selected) {
						UIConfigColor darkened_color = { DarkenColor(current_color, color_theme.darken_inactive_item) };
						if (active_state) {
							darkened_color = previous_color;
						}
						SetConfigParameter<configuration, UI_CONFIG_COLOR>(config, darkened_color, previous_color);

						SolidColorRectangle<configuration | UI_CONFIG_COLOR>(config, position, triangle_scale);
					}

					if (state) {
						SpriteRectangle<configuration>(position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE);
					}
					else {
						SpriteRectangle<configuration>(position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, ECS_COLOR_WHITE, { 1.0f, 0.0f }, { 0.0f, 1.0f });
					}

					position.x += triangle_scale.x;

					if constexpr (~configuration & UI_CONFIG_COLLAPSING_HEADER_NO_NAME) {
						UIConfigTextAlignment previous_alignment;
						UIConfigTextAlignment alignment;
						alignment.horizontal = TextAlignment::Left;
						alignment.vertical = TextAlignment::Middle;

						SetConfigParameter<configuration, UI_CONFIG_TEXT_ALIGNMENT>(config, alignment, previous_alignment);

						float label_scale = scale.x - triangle_scale.x;
						if (label_scale < data->name.scale.x + 2 * element_descriptor.label_horizontal_padd) {
							label_scale = data->name.scale.x + 2 * element_descriptor.label_horizontal_padd;
						}

#define LABEL_CONFIGURATION configuration | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X \
						| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_ADVANCE

						if (is_selected) {
							TextLabelDrawer<LABEL_CONFIGURATION | UI_CONFIG_COLOR>(
								config,
								&data->name,
								position,
								{ label_scale, scale.y }
							);
						}
						else {
							TextLabelDrawer<LABEL_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT>(
								config,
								&data->name,
								position,
								{ label_scale, scale.y }
							);
						}

#undef LABEL_CONFIGURATION
						UpdateCurrentRowScale(scale.y);
						UpdateRenderBoundsRectangle(position, { 0.0f, scale.y });

						RemoveConfigParameter<configuration, UI_CONFIG_TEXT_ALIGNMENT>(config, previous_alignment);
					}

					if (is_selected) {
						RemoveConfigParameter<configuration, UI_CONFIG_COLOR>(config, previous_color);
					}

					if (active_state) {
						Color color = HandleColor<configuration>(config);

						UIDrawerBoolClickableWithPinData click_data;
						click_data.pointer = &data->state;
						click_data.is_vertical = true;

						AddDefaultClickableHoverable(initial_position, scale, { BoolClickableWithPin, &click_data, sizeof(click_data) }, color);
					}
				}
				else {
					current_row_y_scale = scale.y;
					UpdateRenderBoundsRectangle(position, { 0.0f, scale.y });
				}
				NextRow();
			}

			template<size_t configuration>
			void CollapsingHeaderDrawer(UIDrawConfig& config, const char* name, bool* state, float2 position, float2 scale) {
				if constexpr (~configuration & UI_CONFIG_COLLAPSING_HEADER_DO_NOT_INFER) {
					scale.x = GetXScaleUntilBorder(position.x);
				}

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {
					bool active_state = true;
					if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
						const UIConfigActiveState* _active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
						active_state = _active_state->state;
					}

					bool stack_state = *state;
					float2 initial_position = position;
					stack_state &= active_state;

					Color current_color = HandleColor<configuration>(config);
					UIConfigColor previous_color = { current_color };
					float2 triangle_scale = GetSquareScale(scale.y);
						
					bool is_selected = true;
					if constexpr (configuration & UI_CONFIG_COLLAPSING_HEADER_SELECTION) {
						const UIConfigCollapsingHeaderSelection* selection = (const UIConfigCollapsingHeaderSelection*)config.GetParameter(UI_CONFIG_COLLAPSING_HEADER_SELECTION);
						is_selected = selection->is_selected;
					}

					if (is_selected) {
						UIConfigColor darkened_color = { DarkenColor(current_color, color_theme.darken_inactive_item) };
						if (active_state) {
							darkened_color = previous_color;
						}
						SetConfigParameter<configuration, UI_CONFIG_COLOR>(config, darkened_color, previous_color);

						SolidColorRectangle<configuration | UI_CONFIG_COLOR>(config, position, triangle_scale);
					}

					if (stack_state) {
						SpriteRectangle<configuration>(position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE);
					}
					else {
						SpriteRectangle<configuration>(position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, ECS_COLOR_WHITE, { 1.0f, 0.0f }, { 0.0f, 1.0f });
					}

					position.x += triangle_scale.x;

					if constexpr (~configuration & UI_CONFIG_COLLAPSING_HEADER_NO_NAME) {
						UIConfigTextAlignment previous_alignment;
						UIConfigTextAlignment alignment;
						alignment.horizontal = TextAlignment::Left;
						alignment.vertical = TextAlignment::Middle;

						SetConfigParameter<configuration, UI_CONFIG_TEXT_ALIGNMENT>(config, alignment, previous_alignment);

						float label_scale = scale.x - triangle_scale.x;
						float2 name_scale = GetLabelScale(name);
						label_scale = function::PredicateValue(label_scale < name_scale.x, name_scale.x, label_scale);

#define LABEL_CONFIGURATION configuration | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X \
						| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_CACHE

						float2 new_scale = { label_scale, scale.y };
						if (is_selected) {
							TextLabel<LABEL_CONFIGURATION | UI_CONFIG_COLOR>(
								config,
								name,
								position,
								new_scale
							);
						}
						else {
							TextLabel<LABEL_CONFIGURATION | UI_CONFIG_LABEL_TRANSPARENT>(
								config,
								name,
								position,
								new_scale
							);
						}

#undef LABEL_CONFIGURATION
						UpdateCurrentRowScale(scale.y);
						UpdateRenderBoundsRectangle(position, { 0.0f, scale.y });

						RemoveConfigParameter<configuration, UI_CONFIG_TEXT_ALIGNMENT>(config, previous_alignment);
					}

					if (is_selected) {
						RemoveConfigParameter<configuration, UI_CONFIG_COLOR>(config, previous_color);
					}

					if (active_state) {
						Color color = HandleColor<configuration>(config);

						UIDrawerBoolClickableWithPinData click_data;
						click_data.pointer = state;
						click_data.is_vertical = true;

						AddDefaultClickableHoverable(initial_position, scale, { BoolClickableWithPin, &click_data, sizeof(click_data) }, color);
					}
				}
				else {
					current_row_y_scale = scale.y;
					UpdateRenderBoundsRectangle(position, { 0.0f, scale.y });
				}
				NextRow();
			}

			template<size_t configuration>
			UIDrawerHierarchy* HierarchyInitializer(const UIDrawConfig& config, const char* name) {
				UIDrawerHierarchy* data = nullptr;

				AddWindowResource(name, [&](const char* identifier) {
					data = (UIDrawerHierarchy*)GetMainAllocatorBuffer<UIDrawerHierarchy>();

					data->nodes.allocator = system->m_memory;
					data->nodes.capacity = 0;
					data->nodes.size = 0;
					data->nodes.buffer = nullptr;
					data->pending_initializations.allocator = system->m_memory;
					data->pending_initializations.buffer = nullptr;
					data->pending_initializations.size = 0;
					data->pending_initializations.capacity = 0;
					data->system = system;
					data->data = nullptr;
					data->data_size = 0;
					data->window_index = window_index;

					if constexpr (((configuration & UI_CONFIG_HIERARCHY_SELECTABLE) != 0) || ((configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) != 0)) {
						const UIConfigHierarchySelectable* selectable = (const UIConfigHierarchySelectable*)config.GetParameter(UI_CONFIG_HIERARCHY_SELECTABLE);
						data->selectable.selected_index = 0;
						data->selectable.offset = selectable->offset;
						data->selectable.pointer = selectable->pointer;
						data->selectable.callback = selectable->callback_action;
						data->selectable.callback_data = selectable->callback_data;
						if (selectable->callback_data_size > 0) {
							void* allocation = GetMainAllocatorBuffer(selectable->callback_data_size);
							memcpy(allocation, selectable->callback_data, selectable->callback_data_size);
							data->selectable.callback_data = allocation;
						}
					}

					if constexpr (((configuration & UI_CONFIG_HIERARCHY_CHILD) == 0) && (((configuration & UI_CONFIG_HIERARCHY_SELECTABLE) != 0) || ((configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) != 0))) {
						data->multiple_hierarchy_data = GetMainAllocatorBuffer<UIDrawerHierarchiesData>();
						data->multiple_hierarchy_data->hierarchy_transforms = UIDynamicStream<UIElementTransform>(system->m_memory, 0);
						data->multiple_hierarchy_data->elements = UIDynamicStream<UIDrawerHierarchiesElement>(system->m_memory, 0);
						data->multiple_hierarchy_data->active_hierarchy = data;
					}
					else if constexpr (configuration & UI_CONFIG_HIERARCHY_CHILD) {
						const UIConfigHierarchyChild* child_data = (const UIConfigHierarchyChild*)config.GetParameter(UI_CONFIG_HIERARCHY_CHILD);
						UIDrawerHierarchy* parent_hierarchy = (UIDrawerHierarchy*)child_data->parent;
						data->multiple_hierarchy_data = parent_hierarchy->multiple_hierarchy_data;
					}
					else {
						data->multiple_hierarchy_data = nullptr;
					}


					return data;
				});

				return data;
			}

			template<size_t configuration, typename InitialValueInitializer, typename CallbackHover>
			UIDrawerTextInput* NumberInputInitializer(
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
				// kinda unnecessary memcpy's here but should not cost too much
				char full_name[256];
				const char* identifier = HandleResourceIdentifier(name);
				size_t identifier_size = strlen(identifier);
				memcpy(full_name, identifier, identifier_size);
				full_name[identifier_size] = '\0';
				strcat(full_name, "stream");
				CapacityStream<char>* stream = GetMainAllocatorBufferAndStoreAsResource<CapacityStream<char>>(full_name);
				stream->InitializeFromBuffer(GetMainAllocatorBuffer(sizeof(char) * 64, alignof(char)), 0, 63);
				initial_value_init(stream);

				// implement callback
				UIConfigTextInputCallback callback;
				callback.handler = { callback_action, callback_data, static_cast<unsigned int>(callback_data_size) };
				config.AddFlag(callback);
				UIDrawerTextInput* input = TextInputInitializer<configuration | UI_CONFIG_TEXT_INPUT_CALLBACK>(config, name, stream, position, scale);
				UIDrawerFloatInputCallbackData* callback_input_ptr = (UIDrawerFloatInputCallbackData*)input->callback_data;
				callback_input_ptr->input = input;
				config.flag_count--;

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

			template<size_t configuration, typename Lambda>
			void NumberInputDrawer(
				const UIDrawConfig& config, 
				const char* name, 
				Action hoverable_action,
				void* hoverable_data,
				unsigned int hoverable_data_size,
				Action draggable_action,
				void* draggable_data,
				unsigned int draggable_data_size,
				float2 position,
				float2 scale, 
				Lambda&& lambda
			) {
				UIDrawerTextInput* input = (UIDrawerTextInput*)GetResource(name);

				Stream<char> tool_tip_stream;
				const char* identifier = HandleResourceIdentifier(name);
				size_t identifier_size = strlen(identifier);
				char tool_tip_name[128];
				memcpy(tool_tip_name, identifier, identifier_size);
				tool_tip_name[identifier_size] = '\0';
				strcat(tool_tip_name, "tool tip");
				char* tool_tip_characters = (char*)GetResource(tool_tip_name);
				tool_tip_stream = Stream<char>(tool_tip_characters, 0);

				lambda(input, tool_tip_stream);
				size_t text_count_before = *HandleTextSpriteCount<configuration>();
				TextInputDrawer<configuration, TextFilterNumbers>(config, input, position, scale);

				UISpriteVertex* text_buffer = HandleTextSpriteBuffer<configuration>();
				size_t* text_count = HandleTextSpriteCount<configuration>();

				if (text_count_before != *text_count) {
					size_t name_length = input->name.text_vertices.size;
					Stream<UISpriteVertex> stream;

					if constexpr (IsElementNameAfter(configuration, UI_CONFIG_TEXT_INPUT_NO_NAME)) {
						stream = Stream<UISpriteVertex>(text_buffer + *text_count - name_length, name_length);
					}
					else if constexpr (IsElementNameFirst(configuration, UI_CONFIG_TEXT_INPUT_NO_NAME)) {
						stream = Stream<UISpriteVertex>(text_buffer + text_count_before, name_length);
					}
					else {
						stream = Stream<UISpriteVertex>(nullptr, 0);
					}

					float2 text_span = GetTextSpan(stream);
					float2 text_position = { stream[0].position.x, -stream[0].position.y };
					if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
						tool_tip_name[identifier_size] = '\0';
						strcat(tool_tip_name, "tool tip");
						char* tool_tip_data_characters = (char*)GetResource(tool_tip_name);

						UITextTooltipHoverableData* tool_tip_data_ptr = (UITextTooltipHoverableData*)hoverable_data;
						tool_tip_data_ptr->characters = tool_tip_data_characters;
						tool_tip_data_ptr->base.offset.y = 0.007f;
						tool_tip_data_ptr->base.offset_scale.y = true;
						tool_tip_data_ptr->base.next_row_offset = 0.006f;
						uintptr_t tool_tip_reinterpretation = (uintptr_t)tool_tip_data_ptr;
						tool_tip_reinterpretation += sizeof(UITextTooltipHoverableData);
						UIDrawerFloatInputCallbackData* temp_reinterpretation = (UIDrawerFloatInputCallbackData*)tool_tip_reinterpretation;
						temp_reinterpretation->input = input;

						AddHoverable(text_position, text_span, { hoverable_action, hoverable_data, hoverable_data_size, UIDrawPhase::System });
					}

					if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_NO_DRAG_VALUE) {
						AddClickable(text_position, text_span, { draggable_action, draggable_data, draggable_data_size });
					}
				}
			}

			template<size_t configuration>
			UIDrawerTextInput* FloatInputInitializer(UIDrawConfig& config, const char* name, float* number, float min, float max, float2 position, float2 scale) {
				UIDrawerFloatInputCallbackData callback_data;
				if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
					callback_data.max = max;
					callback_data.min = min;
				}
				else {
					callback_data.max = FLT_MAX;
					callback_data.min = FLT_MIN;
				}
				callback_data.number = number;			

				return NumberInputInitializer<configuration>(
					config,
					name,
					FloatInputCallback,
					&callback_data,
					sizeof(callback_data),
					position,
					scale,
					[number](CapacityStream<char>* stream) {
						function::ConvertFloatToChars(*stream, *number, 3);
					},
					[min, max](Stream<char>& tool_tip_stream) {
						function::ConvertFloatToChars(tool_tip_stream, min, 3);
						tool_tip_stream.Add(',');
						tool_tip_stream.Add(' ');
						function::ConvertFloatToChars(tool_tip_stream, max, 3);
					}
				);
			}

			template<size_t configuration>
			UIDrawerTextInput* DoubleInputInitializer(UIDrawConfig& config, const char* name, double* number, double min, double max, float2 position, float2 scale) {
				UIDrawerDoubleInputCallbackData callback_data;
				if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
					callback_data.max = max;
					callback_data.min = min;
				}
				else {
					callback_data.max = DBL_MAX;
					callback_data.min = DBL_MIN;
				}
				callback_data.number = number;

				return NumberInputInitializer<configuration>(
					config,
					name,
					DoubleInputCallback,
					&callback_data,
					sizeof(callback_data),
					position,
					scale,
					[number](CapacityStream<char>* stream) {
						function::ConvertDoubleToChars(*stream, *number, 3);
					},
					[min, max](Stream<char>& tool_tip_stream) {
						function::ConvertDoubleToChars(tool_tip_stream, min, 3);
						tool_tip_stream.Add(',');
						tool_tip_stream.Add(' ');
						function::ConvertDoubleToChars(tool_tip_stream, max, 3);
					}
					);
			}

			template<size_t configuration, typename Integer>
			UIDrawerTextInput* IntInputInitializer(UIDrawConfig& config, const char* name, Integer* number, Integer min, Integer max, float2 position, float2 scale) {
				UIDrawerIntegerInputCallbackData< Integer> callback_data;
				if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
					callback_data.max = max;
					callback_data.min = min;
				}
				else {
					if constexpr (std::is_same_v<Integer, int8_t>) {
						callback_data.max = CHAR_MAX;
						callback_data.min = CHAR_MIN;
					}
					else if constexpr (std::is_same_v<Integer, uint8_t>) {
						callback_data.max = UCHAR_MAX;
						callback_data.min = 0;
					}
					else if constexpr (std::is_same_v<Integer, int16_t>) {
						callback_data.max = SHORT_MAX;
						callback_data.min = SHORT_MIN;
					}
					else if constexpr (std::is_same_v<Integer, uint16_t>) {
						callback_data.max = USHORT_MAX;
						callback_data.min = 0;
					}
					else if constexpr (std::is_same_v<Integer, int32_t>) {
						callback_data.max = INT_MAX;
						callback_data.min = INT_MIN;
					}
					else if constexpr (std::is_same_v<Integer, uint32_t>) {
						callback_data.max = UINT_MAX;
						callback_data.min = 0;
					}
					else if constexpr (std::is_same_v<Integer, int64_t>) {
						callback_data.max = LLONG_MAX;
						callback_data.min = LLONG_MIN;
					}
					else if constexpr (std::is_same_v<Integer, uint64_t>) {
						callback_data.max = LLONG_MAX;
						callback_data.min = 0;
					}
				}
				callback_data.number = number;

				return NumberInputInitializer<configuration>(
					config,
					name,
					IntegerInputCallback<Integer>,
					&callback_data,
					sizeof(callback_data),
					position,
					scale,
					[number](CapacityStream<char>* stream) {
						function::ConvertIntToCharsFormatted(*stream, static_cast<int64_t>(*number));
					},
					[min, max](Stream<char>& tool_tip_stream) {
						function::ConvertIntToCharsFormatted(tool_tip_stream, static_cast<int64_t>(min));
						tool_tip_stream.Add(',');
						tool_tip_stream.Add(' ');
						function::ConvertIntToCharsFormatted(tool_tip_stream, static_cast<int64_t>(max));
					}
				);
			}


			template<size_t configuration>
			void FloatInputDrawer(const UIDrawConfig& config, const char* name, float* number, float default_value, float min, float max, float2 position, float2 scale) {
				UIDrawerFloatInputHoverableData hover_data;
				hover_data.data.default_value = default_value;
				if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_DEFAULT) {
					hover_data.data.default_value = default_value;
				}
				else {
					hover_data.data.default_value = *number;
				}
				UIDrawerFloatInputDragData drag_data;
				drag_data.data.number = number;
				if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
					drag_data.data.min = min;
					drag_data.data.max = max;
				}
				else {
					drag_data.data.min = FLT_MIN;
					drag_data.data.max = FLT_MAX;
				}
				drag_data.difference_factor = number_input_drag_factor;

				NumberInputDrawer<configuration>(config, name, FloatInputHoverable, &hover_data, sizeof(hover_data),
					FloatInputDragValue, &drag_data, sizeof(drag_data), position, scale, [=](UIDrawerTextInput* input, Stream<char> tool_tip_characters) {
					char temp_chars[256];
					float current_value = function::ConvertCharactersToFloat(*input->text);
					if (current_value != *number && !input->is_currently_selected) {
						input->DeleteAllCharacters();
						Stream<char> temp_stream = Stream<char>(temp_chars, 0);
						function::ConvertFloatToChars(temp_stream, *number, 3);
						input->InsertCharacters(temp_chars, temp_stream.size, 0, system);
					}

					if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
						UIDrawerFloatInputCallbackData* data = (UIDrawerFloatInputCallbackData*)input->callback_data;
						bool is_different = data->max != max || data->min != min;
						data->max = max;
						data->min = min;

						if (is_different) {
							tool_tip_characters.size = 0;
							tool_tip_characters.Add('[');
							function::ConvertFloatToChars(tool_tip_characters, min, 3);
							tool_tip_characters.Add(',');
							tool_tip_characters.Add(' ');
							function::ConvertFloatToChars(tool_tip_characters, max, 3);
							tool_tip_characters.Add(']');
							tool_tip_characters[tool_tip_characters.size] = '\0';
						}
						
					}
				});
			}

			template<size_t configuration>
			void DoubleInputDrawer(const UIDrawConfig& config, const char* name, double* number, double default_value, double min, double max, float2 position, float2 scale) {
				UIDrawerDoubleInputHoverableData hover_data;
				hover_data.data.default_value = default_value;
				if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_DEFAULT) {
					hover_data.data.default_value = default_value;
				}
				else {
					hover_data.data.default_value = *number;
				}
				UIDrawerDoubleInputDragData drag_data;
				drag_data.data.number = number;
				if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
					drag_data.data.min = min;
					drag_data.data.max = max;
				}
				else {
					drag_data.data.min = DBL_MIN;
					drag_data.data.max = DBL_MAX;
				}
				drag_data.difference_factor = number_input_drag_factor;

				NumberInputDrawer<configuration>(config, name, DoubleInputHoverable, &hover_data, sizeof(hover_data), 
					DoubleInputDragValue, &drag_data, sizeof(drag_data), position,
					scale, [=](UIDrawerTextInput* input, Stream<char> tool_tip_characters) {
					char temp_chars[256];
					double current_value = function::ConvertCharactersToDouble(*input->text);
					if (current_value != *number && !input->is_currently_selected) {
						input->DeleteAllCharacters();
						Stream<char> temp_stream = Stream<char>(temp_chars, 0);
						function::ConvertDoubleToChars(temp_stream, *number, 3);
						input->InsertCharacters(temp_chars, temp_stream.size, 0, system);
					}
					
					if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
						UIDrawerDoubleInputCallbackData* data = (UIDrawerDoubleInputCallbackData*)input->callback_data;
						bool is_different = data->max != max || data->min != min;
						data->max = max;
						data->min = min;

						if (is_different) {
							tool_tip_characters.size = 0;
							tool_tip_characters.Add('[');
							function::ConvertDoubleToChars(tool_tip_characters, min, 3);
							tool_tip_characters.Add(',');
							tool_tip_characters.Add(' ');
							function::ConvertDoubleToChars(tool_tip_characters, max, 3);
							tool_tip_characters.Add(']');
							tool_tip_characters[tool_tip_characters.size] = '\0';
						}
					}
					});
			}

			template<size_t configuration, typename Integer>
			void IntInputDrawer(const UIDrawConfig& config, const char* name, Integer* number, Integer default_value, Integer min, Integer max, float2 position, float2 scale) {
				UIDrawerIntInputHoverableData<Integer> hover_data;
				hover_data.data.number = number;
				if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_DEFAULT) {
					hover_data.data.default_value = default_value;
				}
				else {
					hover_data.data.default_value = *number;
				}

				UIDrawerIntInputDragData<Integer> drag_data;
				drag_data.data.number = number;
				if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
					drag_data.data.min = min;
					drag_data.data.max = max;
				}
				else {
					function::IntegerRange(drag_data.data.min, drag_data.data.max);
				}
				drag_data.difference_factor = number_input_drag_factor;

				NumberInputDrawer<configuration>(config, name, IntInputHoverable<Integer>, &hover_data, sizeof(hover_data),
					IntegerInputDragValue<Integer>, &drag_data, sizeof(drag_data),
					position, scale, [=](UIDrawerTextInput* input, Stream<char> tool_tip_characters) {
					char temp_chars[256];
					Integer current_value = function::ConvertCharactersToInt<Integer>(*input->text);
					if (current_value != *number && !input->is_currently_selected) {
						input->DeleteAllCharacters();
						Stream<char> temp_stream = Stream<char>(temp_chars, 0);
						function::ConvertIntToCharsFormatted(temp_stream, static_cast<int64_t>(*number));
						input->InsertCharacters(temp_chars, temp_stream.size, 0, system);
					}

					if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
						UIDrawerIntegerInputCallbackData<Integer>* data = (UIDrawerIntegerInputCallbackData<Integer>*)input->callback_data;
						bool is_different = data->max != max || data->min != min;
						data->max = max;
						data->min = min;

						if (is_different) {
							tool_tip_characters.size = 0;
							tool_tip_characters.Add('[');
							function::ConvertIntToCharsFormatted(tool_tip_characters, static_cast<int64_t>(min));
							tool_tip_characters.Add(',');
							tool_tip_characters.Add(' ');
							function::ConvertIntToCharsFormatted(tool_tip_characters, static_cast<int64_t>(max));
							tool_tip_characters.Add(']');
							tool_tip_characters[tool_tip_characters.size] = '\0';
						}
					}
				});
			}

			template<size_t configuration>
			void FloatInputGroupDrawer(
				UIDrawConfig& config,
				const char* ECS_RESTRICT group_name,
				size_t count,
				const char** ECS_RESTRICT names,
				float** ECS_RESTRICT values,
				const float* ECS_RESTRICT default_values,
				const float* ECS_RESTRICT lower_bounds,
				const float* ECS_RESTRICT upper_bounds,
				float2 position,
				float2 scale
			) {
#define LABEL_CONFIGURATION configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
#define LABEL_CONFIGURATION_LAST configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION>(config, group_name);
					Indent(-1.0f);
				}

				size_t group_name_size = strlen(group_name);
				char temp_name[128];
				memcpy(temp_name, group_name, group_name_size);

				for (size_t index = 0; index < count; index++) {
					HandleTransformFlags<configuration>(config, position, scale);
					float lower_bound, upper_bound, default_value;
					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS) {
						lower_bound = lower_bounds[0];
						upper_bound = upper_bounds[0];
					}
					else {
						if (lower_bounds != nullptr) {
							lower_bound = lower_bounds[index];
						}
						if (upper_bounds != nullptr) {
							upper_bound = upper_bounds[index];
						}
					}

					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_DEFAULT) {
						default_value = default_values[0];
					}
					else {
						if (default_values != nullptr) {
							default_value = default_values[index];
						}
					}

					if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES) {
						FloatInputDrawer<configuration | UI_CONFIG_ELEMENT_NAME_FIRST>(
							config,
							names[index],
							values[index],
							default_value,
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
					else {
						temp_name[group_name_size] = '\0';
						size_t random_index = (index * index + (index & 15)) << (index & 3);
						function::ConvertIntToChars(Stream<char>(temp_name, group_name_size), static_cast<int64_t>(random_index));
						FloatInputDrawer<configuration | UI_CONFIG_TEXT_INPUT_NO_NAME>(
							config,
							temp_name,
							values[index],
							default_value,
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
				}

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION_LAST>(config, group_name);
				}
#undef LABEL_CONFIGURATION
#undef LABEL_CONFIGURATION_LAST
			}

			template<size_t configuration>
			void FloatInputGroupInitializer(
				UIDrawConfig& config,
				const char* ECS_RESTRICT group_name,
				size_t count,
				const char** ECS_RESTRICT names,
				float** ECS_RESTRICT values,
				const float* ECS_RESTRICT default_values,
				const float* ECS_RESTRICT lower_bounds,
				const float* ECS_RESTRICT upper_bounds,
				float2 position,
				float2 scale
			) {
#define LABEL_CONFIGURATION configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
#define LABEL_CONFIGURATION_LAST configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION>(config, group_name);
					Indent(-1.0f);
				}

				size_t group_name_size = strlen(group_name);
				char temp_name[128];
				memcpy(temp_name, group_name, group_name_size);

				for (size_t index = 0; index < count; index++) {
					HandleTransformFlags<configuration>(config, position, scale);
					float lower_bound, upper_bound, default_value;
					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS) {
						lower_bound = lower_bounds[0];
						upper_bound = upper_bounds[0];
					}
					else {
						if (lower_bounds != nullptr) {
							lower_bound = lower_bounds[index];
						}
						if (upper_bounds != nullptr) {
							upper_bound = upper_bounds[index];
						}
					}

					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_DEFAULT) {
						default_value = default_values[0];
					}
					else {
						if (default_values != nullptr) {
							default_value = default_values[index];
						}
					}

					if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES) {
						FloatInputInitializer<configuration | UI_CONFIG_ELEMENT_NAME_FIRST>(
							config,
							names[index],
							values[index],
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
					else {
						temp_name[group_name_size] = '\0';
						size_t random_index = (index * index + (index & 15)) << (index & 3);
						function::ConvertIntToChars(Stream<char>(temp_name, group_name_size), static_cast<int64_t>(random_index));
						FloatInputInitializer<configuration | UI_CONFIG_TEXT_INPUT_NO_NAME>(
							config,
							temp_name,
							values[index],
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
				}

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION_LAST>(config, group_name);
				}
#undef LABEL_CONFIGURATION
#undef LABEL_CONFIGURATION_LAST
			}

			template<size_t configuration>
			void DoubleInputGroupDrawer(
				UIDrawConfig& config,
				const char* ECS_RESTRICT group_name,
				size_t count,
				const char** ECS_RESTRICT names,
				double** ECS_RESTRICT values,
				const double* ECS_RESTRICT default_values,
				const double* ECS_RESTRICT lower_bounds,
				const double* ECS_RESTRICT upper_bounds,
				float2 position,
				float2 scale
			) {
#define LABEL_CONFIGURATION configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
#define LABEL_CONFIGURATION_LAST configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION>(config, group_name);
					Indent(-1.0f);
				}

				size_t group_name_size = strlen(group_name);
				char temp_name[128];
				memcpy(temp_name, group_name, group_name_size);

				for (size_t index = 0; index < count; index++) {
					HandleTransformFlags<configuration>(config, position, scale);
					double lower_bound, upper_bound, default_value;
					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS) {
						lower_bound = lower_bounds[0];
						upper_bound = upper_bounds[0];
					}
					else {
						if (lower_bounds != nullptr) {
							lower_bound = lower_bounds[index];
						}
						if (upper_bounds != nullptr) {
							upper_bound = upper_bounds[index];
						}
					}

					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_DEFAULT) {
						default_value = default_values[0];
					}
					else {
						if (default_values != nullptr) {
							default_value = default_values[index];
						}
					}

					if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES) {
						DoubleInputDrawer<configuration | UI_CONFIG_ELEMENT_NAME_FIRST>(
							config,
							names[index],
							values[index],
							default_value,
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
					else {
						temp_name[group_name_size] = '\0';
						size_t random_index = GetRandomIntIdentifier(index);
						function::ConvertIntToChars(Stream<char>(temp_name, group_name_size), static_cast<int64_t>(random_index));
						DoubleInputDrawer<configuration | UI_CONFIG_TEXT_INPUT_NO_NAME>(
							config,
							temp_name,
							values[index],
							default_value,
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
				}

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION_LAST>(config, group_name);
				}
#undef LABEL_CONFIGURATION
#undef LABEL_CONFIGURATION_LAST
			}

			template<size_t configuration>
			void DoubleInputGroupInitializer(
				UIDrawConfig& config,
				const char* ECS_RESTRICT group_name,
				size_t count,
				const char** ECS_RESTRICT names,
				double** ECS_RESTRICT values,
				const double* ECS_RESTRICT default_values,
				const double* ECS_RESTRICT lower_bounds,
				const double* ECS_RESTRICT upper_bounds,
				float2 position,
				float2 scale
			) {
#define LABEL_CONFIGURATION configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
#define LABEL_CONFIGURATION_LAST configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION>(config, group_name);
					Indent(-1.0f);
				}

				size_t group_name_size = strlen(group_name);
				char temp_name[128];
				memcpy(temp_name, group_name, group_name_size);

				for (size_t index = 0; index < count; index++) {
					HandleTransformFlags<configuration>(config, position, scale);
					double lower_bound, upper_bound, default_value;
					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS) {
						lower_bound = lower_bounds[0];
						upper_bound = upper_bounds[0];
					}
					else {
						if (lower_bounds != nullptr) {
							lower_bound = lower_bounds[index];
						}
						if (upper_bounds != nullptr) {
							upper_bound = upper_bounds[index];
						}
					}

					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_DEFAULT) {
						default_value = default_values[0];
					}
					else {
						if (default_values != nullptr) {
							default_value = default_values[index];
						}
					}

					if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES) {
						DoubleInputInitializer<configuration | UI_CONFIG_ELEMENT_NAME_FIRST>(
							config,
							names[index],
							values[index],
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
					else {
						temp_name[group_name_size] = '\0';
						size_t random_index = (index * index + (index & 15)) << (index & 3);
						function::ConvertIntToChars(Stream<char>(temp_name, group_name_size), static_cast<int64_t>(random_index));
						DoubleInputInitializer<configuration | UI_CONFIG_TEXT_INPUT_NO_NAME>(
							config,
							temp_name,
							values[index],
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
				}

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION_LAST>(config, group_name);
				}
#undef LABEL_CONFIGURATION
#undef LABEL_CONFIGURATION_LAST
			}

			template<size_t configuration, typename Integer>
			void IntInputGroupDrawer(
				UIDrawConfig& config,
				const char* ECS_RESTRICT group_name,
				size_t count,
				const char** ECS_RESTRICT names,
				Integer** ECS_RESTRICT values,
				const Integer* ECS_RESTRICT default_values,
				const Integer* ECS_RESTRICT lower_bounds,
				const Integer* ECS_RESTRICT upper_bounds,
				float2 position,
				float2 scale
			) {
#define LABEL_CONFIGURATION configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
#define LABEL_CONFIGURATION_LAST configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION>(config, group_name);
					Indent(-1.0f);
				}

				size_t group_name_size = strlen(group_name);
				char temp_name[128];
				memcpy(temp_name, group_name, group_name_size);

				for (size_t index = 0; index < count; index++) {
					HandleTransformFlags<configuration>(config, position, scale);
					Integer lower_bound, upper_bound, default_value;
					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS) {
						lower_bound = lower_bounds[0];
						upper_bound = upper_bounds[0];
					}
					else {
						if (lower_bounds != nullptr) {
							lower_bound = lower_bounds[index];
						}
						if (upper_bounds != nullptr) {
							upper_bound = upper_bounds[index];
						}
					}

					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_DEFAULT) {
						default_value = default_values[0];
					}
					else {
						if (default_values != nullptr) {
							default_value = default_values[index];
						}
					}

					if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES) {
						IntInputDrawer<configuration | UI_CONFIG_ELEMENT_NAME_FIRST, Integer>(
							config,
							names[index],
							values[index],
							default_value,
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
					else {
						temp_name[group_name_size] = '\0';
						size_t random_index = (index * index + (index & 15)) << (index & 3);
						function::ConvertIntToChars(Stream<char>(temp_name, group_name_size), static_cast<int64_t>(random_index));
						IntInputDrawer<configuration | UI_CONFIG_TEXT_INPUT_NO_NAME, Integer>(
							config,
							temp_name,
							values[index],
							default_value,
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
				}

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION_LAST>(config, group_name);
				}
#undef LABEL_CONFIGURATION
#undef LABEL_CONFIGURATION_LAST
			}

			template<size_t configuration, typename Integer>
			void IntInputGroupInitializer(
				UIDrawConfig& config,
				const char* ECS_RESTRICT group_name,
				size_t count,
				const char** ECS_RESTRICT names,
				Integer** ECS_RESTRICT values,
				const Integer* ECS_RESTRICT default_values,
				const Integer* ECS_RESTRICT lower_bounds,
				const Integer* ECS_RESTRICT upper_bounds,
				float2 position,
				float2 scale
			) {
#define LABEL_CONFIGURATION configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
#define LABEL_CONFIGURATION_LAST configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION>(config, group_name);
					Indent(-1.0f);
				}

				size_t group_name_size = strlen(group_name);
				char temp_name[128];
				memcpy(temp_name, group_name, group_name_size);

				for (size_t index = 0; index < count; index++) {
					HandleTransformFlags<configuration>(config, position, scale);
					Integer lower_bound, upper_bound, default_value;
					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS) {
						lower_bound = lower_bounds[0];
						upper_bound = upper_bounds[0];
					}
					else {
						if (lower_bounds != nullptr) {
							lower_bound = lower_bounds[index];
						}
						if (upper_bounds != nullptr) {
							upper_bound = upper_bounds[index];
						}
					}

					if constexpr (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_DEFAULT) {
						default_value = default_values[0];
					}
					else {
						if (default_values != nullptr) {
							default_value = default_values[index];
						}
					}

					if constexpr (~configuration & UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES) {
						IntInputInitializer<configuration | UI_CONFIG_ELEMENT_NAME_FIRST, Integer>(
							config,
							names[index],
							values[index],
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
					else {
						temp_name[group_name_size] = '\0';
						size_t random_index = (index * index + (index & 15)) << (index & 3);
						function::ConvertIntToChars(Stream<char>(temp_name, group_name_size), static_cast<int64_t>(random_index));
						IntInputInitializer<configuration | UI_CONFIG_TEXT_INPUT_NO_NAME, Integer>(
							config,
							temp_name,
							values[index],
							lower_bound,
							upper_bound,
							position,
							scale
							);
					}
				}

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
					TextLabel<LABEL_CONFIGURATION_LAST>(config, group_name);
				}
#undef LABEL_CONFIGURATION
#undef LABEL_CONFIGURATION_LAST
			}
			
			template<size_t configuration>
			void HierarchyDrawer(const UIDrawConfig& config, UIDrawerHierarchy* data, float2 scale) {
				if constexpr (~configuration & UI_CONFIG_HIERARCHY_DO_NOT_REDUCE_Y) {
					scale.y *= 0.75f;
				}

				if constexpr (~configuration & UI_CONFIG_HIERARCHY_CHILD) {
					data->multiple_hierarchy_data->hierarchy_transforms.Reset();
					data->multiple_hierarchy_data->elements.Reset();
				}

				if (data->pending_initializations.size > 0) {
					// creating a temporary initializer
					UIDrawer<true> drawer_initializer = UIDrawer<true>(color_theme, font, layout, element_descriptor);
					memcpy(&drawer_initializer, this, sizeof(UIDrawer<true>));
					drawer_initializer.deallocate_identifier_buffers = false;

					for (size_t index = 0; index < data->pending_initializations.size; index++) {
						size_t current_window_allocation_count = system->m_windows[window_index].memory_resources.size;
						data->pending_initializations[index].function(&drawer_initializer, window_data, data);
						for (size_t subindex = current_window_allocation_count; subindex < system->m_windows[window_index].memory_resources.size; subindex++) {
							data->nodes[data->pending_initializations[index].bound_node].internal_allocations.Add(system->m_windows[window_index].memory_resources[subindex]);
						}
					}
				}
				data->pending_initializations.Reset();

				float initial_scale = scale.x;
				float2 position = { GetNextRowXPosition() - region_render_offset.x, current_y - region_render_offset.y };
				float2 initial_position = position;

				scale.x = GetXScaleUntilBorder(position.x);
				if constexpr (configuration & UI_CONFIG_HIERARCHY_NODE_DO_NOT_INFER_SCALE_X) {
					scale.x = initial_scale;
				}

				float2 sprite_scale = GetSquareScale(scale.y);

				UIConfigTextAlignment alignment;
				alignment.horizontal = TextAlignment::Left;
				alignment.vertical = TextAlignment::Middle;

				UIDrawConfig label_config;
				label_config.AddFlag(alignment);

				Color hover_color = HandleColor<configuration>(config);
				LPCWSTR texture = nullptr;
				float2 top_left_closed_uv;
				float2 bottom_right_closed_uv;
				float2 top_left_opened_uv;
				float2 bottom_right_opened_uv;
				float2 sprite_scale_factor;
				Color texture_color;
				UIConfigHierarchyNoAction* extra_info = nullptr;

				bool extra_draw_index = false;
				if constexpr (configuration & UI_CONFIG_HIERARCHY_SPRITE_TEXTURE) {
					const UIConfigHierarchySpriteTexture* sprite_texture = (const UIConfigHierarchySpriteTexture*)config.GetParameter(UI_CONFIG_HIERARCHY_SPRITE_TEXTURE);
					texture = sprite_texture->texture;
					top_left_closed_uv = sprite_texture->top_left_closed_uv;
					bottom_right_closed_uv = sprite_texture->bottom_right_closed_uv;
					top_left_opened_uv = sprite_texture->top_left_opened_uv;
					bottom_right_opened_uv = sprite_texture->bottom_right_opened_uv;
					sprite_scale_factor = sprite_texture->scale_factor;
					texture_color = sprite_texture->color;
					extra_draw_index = sprite_texture->keep_triangle;
				}
				else {
					texture = ECS_TOOLS_UI_TEXTURE_TRIANGLE;
					top_left_closed_uv = { 1.0f, 0.0f };
					bottom_right_closed_uv = { 0.0f, 1.0f };
					top_left_opened_uv = { 0.0f, 0.0f };
					bottom_right_opened_uv = { 1.0f, 1.0f };
					sprite_scale_factor = { 1.0f, 1.0f };
					texture_color = ECS_COLOR_WHITE;
				}

				sprite_scale *= sprite_scale_factor;

				if constexpr (configuration & UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME) {
					extra_info = (UIConfigHierarchyNoAction*)config.GetParameter(UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME);
				}

				float max_label_scale = scale.x;

				auto extra_draw_triangle_opened = [](UIDrawer<false>& drawer, float2& position, float2 sprite_scale) {
					drawer.SpriteRectangle<configuration>(position, sprite_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, { 0.0f, 0.0f }, { 1.0f, 1.0f });
					position.x += sprite_scale.x;
				};

				auto extra_draw_triangle_closed = [](UIDrawer<false>& drawer, float2& position, float2 sprite_scale) {
					drawer.SpriteRectangle<configuration>(position, sprite_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, { 1.0f, 0.0f }, { 0.0f, 1.0f });
					position.x += sprite_scale.x;
				};

				auto extra_draw_nothing = [](UIDrawer<false>& drawer, float2& position, float2 sprite_scale) {};

				using ExtraDrawFunction = void (*)(UIDrawer<false>&, float2&, float2);

				ExtraDrawFunction opened_table[] = { extra_draw_nothing, extra_draw_triangle_opened };
				ExtraDrawFunction closed_table[] = { extra_draw_nothing, extra_draw_triangle_closed };

				for (size_t index = 0; index < data->nodes.size; index++) {
					float label_scale = scale.x;

					// general implementation
					if constexpr (~configuration & UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME) {
						unsigned int hierarchy_children_start = 0;
						if constexpr ((configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) != 0) {
							data->multiple_hierarchy_data->elements.Add({ data, (unsigned int)index, 0 });
							data->multiple_hierarchy_data->hierarchy_transforms.Add({ position, {label_scale, scale.y} });
							hierarchy_children_start = data->multiple_hierarchy_data->hierarchy_transforms.size;
						}

						if (data->nodes[index].state) {
							if (ValidatePosition<0>(position, { label_scale, scale.y })) {
								opened_table[extra_draw_index](*this, position, sprite_scale);
								SpriteRectangle<configuration>(position, sprite_scale, texture, top_left_opened_uv, bottom_right_opened_uv, texture_color);
								TextLabelDrawer<UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_TEXT_ALIGNMENT
									| UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_VALIDATE_POSITION>
									(
										label_config,
										&data->nodes[index].name_element,
										{ position.x + sprite_scale.x, position.y },
										{ 0.0f, scale.y }
								);
							}
							UpdateCurrentRowScale(scale.y);
							UpdateRenderBoundsRectangle(position, { 0.0f, scale.y });

							OffsetNextRow(layout.node_indentation);
							NextRow(0.5f);
							data->nodes[index].function(this, window_data, data);
							NextRow(0.5f);
							OffsetNextRow(-layout.node_indentation);

							if constexpr (configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) {
								data->multiple_hierarchy_data->elements[hierarchy_children_start - 1].children_count = data->multiple_hierarchy_data->elements.size - hierarchy_children_start;
							}
						}
						else {
							if (ValidatePosition<0>(position, {label_scale, scale.y})) {
								closed_table[extra_draw_index](*this, position, sprite_scale);
								SpriteRectangle<configuration>(position, sprite_scale, texture, top_left_closed_uv, bottom_right_closed_uv, texture_color);
								TextLabelDrawer<UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
									| UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_VALIDATE_POSITION | UI_CONFIG_DO_NOT_ADVANCE>
									(
										label_config,
										&data->nodes[index].name_element,
										{ position.x + sprite_scale.x, position.y },
										{ 0.0f, scale.y }
								);
							}
							FinalizeRectangle<0>(position, { sprite_scale.x + data->nodes[index].name_element.scale.x, scale.y });
							NextRow(0.5f);
						}

						float element_scale = data->nodes[index].name_element.scale.x + 2 * element_descriptor.label_horizontal_padd + sprite_scale.x;
						label_scale = function::PredicateValue(label_scale < element_scale, element_scale, label_scale);

						UIDrawerBoolClickableWithPinData click_data;
						click_data.pointer = &data->nodes[index].state;
						click_data.is_vertical = true;

						float2 hoverable_scale = { label_scale, scale.y };
						if constexpr (configuration & UI_CONFIG_HIERARCHY_SELECTABLE) {
							if (data->selectable.selected_index == index && data->multiple_hierarchy_data->active_hierarchy == data) {
								SolidColorRectangle<configuration>(config, position, hoverable_scale);
							}
							UIDrawerHierarchySelectableData selectable_data;
							selectable_data.bool_data = click_data;
							selectable_data.hierarchy = data;
							selectable_data.node_index = index;
							AddDefaultClickableHoverable(position, hoverable_scale, { HierarchySelectableClick, &selectable_data, sizeof(selectable_data) }, hover_color);
						}
						else if constexpr (configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) {
							if (data->selectable.selected_index == index && data->multiple_hierarchy_data->active_hierarchy == data) {
								SolidColorRectangle<configuration>(config, position, hoverable_scale);
							}

							UIDrawerHierarchyDragNode drag_data;
							drag_data.hierarchies_data = data->multiple_hierarchy_data;
							drag_data.selectable_data.hierarchy = data;
							drag_data.selectable_data.node_index = index;
							drag_data.selectable_data.bool_data = click_data;
							drag_data.timer.SetMarker();

							AddClickable(position, hoverable_scale, { HierarchyNodeDrag, &drag_data, sizeof(drag_data) });
							AddDefaultHoverable(position, hoverable_scale, hover_color);
						}
						else {
							AddDefaultClickableHoverable(position, hoverable_scale, { BoolClickableWithPin, &click_data, sizeof(click_data) }, hover_color);
						}
						max_label_scale = function::PredicateValue(max_label_scale < label_scale, label_scale, max_label_scale);
						position = { GetNextRowXPosition() - region_render_offset.x, current_y - region_render_offset.y };
					}
					// list implementation is here
					else {
						float2 sprite_position = position;
						FinalizeRectangle<0>(position, sprite_scale);
						data->nodes[index].function(this, window_data, extra_info->extra_information);
						NextRow();
						position = { current_x - region_render_offset.x, current_y - region_render_offset.y };

						sprite_position.y = AlignMiddle(sprite_position.y, extra_info->row_y_scale, sprite_scale.y);
						SpriteRectangle<configuration>(sprite_position, sprite_scale, texture, top_left_closed_uv, bottom_right_closed_uv, texture_color);
					}
				}

				// adding a sentinel in order to have nodes placed at the back
				if constexpr ((configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) != 0) {
					data->multiple_hierarchy_data->elements.Add({ data, data->nodes.size });
					data->multiple_hierarchy_data->hierarchy_transforms.Add({ position, {0.0f, scale.y} });
				}

			}

			template<size_t configuration, typename InputStream>
			void HistogramDrawer(const UIDrawConfig& config, const InputStream& samples, const char* name, float2 position, float2 scale, size_t precision = 2) {
				constexpr size_t stack_character_count = 128;
				
				float histogram_min_scale = samples.size * element_descriptor.histogram_bar_min_scale + (samples.size - 1) * element_descriptor.histogram_bar_spacing + 2.0f * element_descriptor.histogram_padding.x;
				function::ClampMin(scale.x, histogram_min_scale);
				
				float2 initial_scale = scale;
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_HISTOGRAM_NO_NAME)) {
					if (name != nullptr) {
						TextLabel<configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
							| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y>(config, name, position, scale);
						scale = initial_scale;
					}
				}

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {
					SolidColorRectangle<configuration>(config, position, scale);
					float2 histogram_scale = { scale.x - 2.0f * element_descriptor.histogram_padding.x, scale.y - 2.0f * element_descriptor.histogram_padding.y };
					float2 histogram_position = { position.x + element_descriptor.histogram_padding.x, position.y + element_descriptor.histogram_padding.y };

					Color histogram_color;
					if constexpr (configuration & UI_CONFIG_HISTOGRAM_COLOR) {
						const UIConfigHistogramColor* color_desc = (const UIConfigHistogramColor*)config.GetParameter(UI_CONFIG_HISTOGRAM_COLOR);
						histogram_color = color_desc->color;
					}
					else {
						histogram_color = color_theme.histogram_color;
					}
					Color initial_color = histogram_color;

					UIDrawConfig label_config;
					UIConfigTextParameters text_parameters;
					text_parameters.color = color_theme.histogram_text_color;
					text_parameters.character_spacing = font.character_spacing;
					text_parameters.size = { font.size * ECS_TOOLS_UI_FONT_X_FACTOR * zoom_ptr->x * zoom_inverse.y, font.size };


					if constexpr (configuration & UI_CONFIG_TEXT_PARAMETERS) {
						text_parameters = *(const UIConfigTextParameters*)config.GetParameter(UI_CONFIG_TEXT_PARAMETERS);
					}
					else if constexpr (~configuration & UI_CONFIG_HISTOGRAM_VARIANT_ZOOM_FONT) {
						text_parameters.size.x *= zoom_inverse.x * zoom_inverse.x;
						text_parameters.size.y *= zoom_inverse.y * zoom_inverse.y;
						text_parameters.character_spacing *= zoom_inverse.x;
					}

					if constexpr (configuration & UI_CONFIG_HISTOGRAM_REDUCE_FONT) {
						text_parameters.size.x *= element_descriptor.histogram_reduce_font;
						text_parameters.size.y *= element_descriptor.histogram_reduce_font;
					}
					label_config.AddFlag(text_parameters);

					float min_value = 100000.0f;
					float max_value = -100000.0f;
					function::GetExtremesFromStream(samples, min_value, max_value, [](float value) {
						return value;
					});
					float difference = max_value - min_value * (min_value < 0.0f);
					float difference_inverse = 1.0f / difference;

					float zero_y_position = histogram_position.y + histogram_scale.y;
					bool negative_only = min_value < 0.0f && max_value <= 0.0f;
					bool positive_only = min_value >= 0.0f;
					bool mixed = !negative_only && !positive_only;
					if (mixed) {
						zero_y_position = histogram_position.y + histogram_scale.y * max_value * difference_inverse;
					}
					else if (negative_only) {
						zero_y_position = histogram_position.y;
					}

					float bar_scale = (histogram_scale.x - (samples.size - 1) * element_descriptor.histogram_bar_spacing) / samples.size;
					float y_sprite_scale = system->GetTextSpriteYScale(text_parameters.size.y);

					char stack_characters[stack_character_count];
					Stream<char> stack_stream = Stream<char>(stack_characters, 0);

					int64_t starting_index = (region_position.x - histogram_position.x) / bar_scale;
					starting_index = function::PredicateValue(starting_index < 0, (int64_t)0, starting_index);
					int64_t end_index = (region_position.x + region_scale.x - histogram_position.x) / bar_scale + 1;
					end_index = function::PredicateValue(end_index > samples.size, (int64_t)samples.size, end_index);
					histogram_position.x += starting_index * (bar_scale + element_descriptor.histogram_bar_spacing);
					for (int64_t index = starting_index; index < end_index; index++) {
						stack_stream.size = 0;
						function::ConvertFloatToChars(stack_stream, samples[index], precision);
						float2 sample_scale = { bar_scale, samples[index] * difference_inverse * histogram_scale.y };
						float2 sample_position = { histogram_position.x, zero_y_position - sample_scale.y };
						if (sample_scale.y < 0.0f) {
							sample_position.y = zero_y_position;
							sample_scale.y = -sample_scale.y;
						}

						float text_offset = 0.0f;

						if (IsPointInRectangle(mouse_position, { sample_position.x, histogram_position.y }, {sample_scale.x, histogram_scale.y})) {
							histogram_color = color_theme.histogram_hovered_color;
						}
						else {
							histogram_color = initial_color;
						}

						if (sample_scale.y < y_sprite_scale) {
							float offset = (y_sprite_scale - sample_scale.y) * 0.5f;
							if (positive_only) {
								offset = -offset;
							}
							else if (mixed) {
								offset = 0.0f;
							}
							text_offset += offset;
						}

						float2 text_position = { sample_position.x, sample_position.y + text_offset };
						float2 text_scale = system->GetTextSpan(stack_characters, stack_stream.size, text_parameters.size.x, text_parameters.size.y, text_parameters.character_spacing);
						if (text_scale.x < sample_scale.x) {
							TextLabel<UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_CACHE
								| UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X
								| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y>(
									label_config,
									stack_characters,
									text_position,
									sample_scale
								);
						}
						SolidColorRectangle<configuration>(sample_position, sample_scale, histogram_color);

						UIDrawerHistogramHoverableData hoverable_data;
						hoverable_data.sample_index = index;
						hoverable_data.sample_value = samples[index];
						AddHoverable({ sample_position.x, histogram_position.y }, { sample_scale.x, histogram_scale.y }, { HistogramHoverable, &hoverable_data, sizeof(hoverable_data), UIDrawPhase::System });
						histogram_position.x += sample_scale.x + element_descriptor.histogram_bar_spacing;
					}
				}
				
				FinalizeRectangle<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(position, scale);

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_HISTOGRAM_NO_NAME)) {
					if (name != nullptr) {
						position.x += scale.x + layout.element_indentation;
						TextLabel<configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y>(config, name, position, scale);
					}
				}
			}

			template<size_t configuration>
			UIDrawerList* ListInitializer(const UIDrawConfig& config, const char* name) {
				UIDrawerList* list = nullptr;

				AddWindowResource(name, [&](const char* identifier) {
					list = GetMainAllocatorBuffer<UIDrawerList>();

					list->hierarchy.nodes.allocator = system->m_memory;
					list->hierarchy.nodes.capacity = 0;
					list->hierarchy.nodes.size = 0;
					list->hierarchy.nodes.buffer = nullptr;
					list->hierarchy.pending_initializations.allocator = system->m_memory;
					list->hierarchy.pending_initializations.buffer = nullptr;
					list->hierarchy.pending_initializations.size = 0;
					list->hierarchy.pending_initializations.capacity = 0;
					list->hierarchy.system = system;
					list->hierarchy.data = nullptr;
					list->hierarchy.data_size = 0;
					list->hierarchy.window_index = window_index;

					if constexpr (~configuration & UI_CONFIG_LIST_NO_NAME) {
						InitializeElementName<configuration, UI_CONFIG_LIST_NO_NAME>(config, identifier, &list->name, { 0.0f, 0.0f });
					}

					return list;
					});
				return list;
			}

			template<size_t configuration>
			void ListDrawer(UIDrawConfig& config, UIDrawerList* data, float2 position, float2 scale) {
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_LIST_NO_NAME) || IsElementNameAfter(configuration, UI_CONFIG_LIST_NO_NAME)) {
					current_x += layout.element_indentation;
					ElementName<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(config, &data->name, position, scale);
					NextRow();
				}

				UIConfigHierarchyNoAction info;
				info.extra_information = data;

				config.AddFlag(info);
				data->hierarchy_extra = (UIConfigHierarchyNoAction*)config.GetParameter(UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME);

				HierarchyDrawer<configuration | UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME | UI_CONFIG_HIERARCHY_CHILD>(config, &data->hierarchy, scale);
			}

			template<size_t configuration>
			void MenuDrawer(const UIDrawConfig& config, UIDrawerMenu* data, float2 position, float2 scale) {
				HandleFitSpaceRectangle<configuration>(position, scale);

				bool is_active = true;
				if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {
					if constexpr (~configuration & UI_CONFIG_MENU_SPRITE) {
						if (is_active) {
							TextLabelDrawer<configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y>(
								config,
								data->name,
								position,
								scale
								);
						}
						else {
							TextLabelDrawer<configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
								| UI_CONFIG_UNAVAILABLE_TEXT>(
									config,
									data->name,
									position,
									scale
									);
						}
					}
					else {
						const UIConfigMenuSprite* sprite_definition = (const UIConfigMenuSprite*)config.GetParameter(UI_CONFIG_MENU_SPRITE);
						Color sprite_color = sprite_definition->color;
						if (!is_active) {
							sprite_color.alpha *= color_theme.alpha_inactive_item;
						}
						SpriteRectangle<configuration>(
							position,
							scale,
							sprite_definition->texture,
							sprite_definition->color,
							sprite_definition->top_left_uv,
							sprite_definition->bottom_right_uv
						);

						HandleBorder<configuration>(config, position, scale);
						FinalizeRectangle<configuration>(position, scale);
					}

					if (is_active) {
						UIDrawerMenuGeneralData general_data;
						general_data.menu = data;
						general_data.menu_initializer_index = 255;

						AddDefaultHoverable(position, scale, HandleColor<configuration>(config));
						AddGeneral(position, scale, { MenuGeneral, &general_data, sizeof(general_data), UIDrawPhase::System });
						AddClickable(position, scale, { SkipAction, nullptr, 0 });
					}
				}
			}

			size_t MenuCalculateStateMemory(const UIDrawerMenuState* state, bool copy_states) {
				size_t total_memory = 0;
				if (copy_states) {
					if (state->unavailables != nullptr) {
						total_memory += state->row_count * sizeof(bool);
					}
					if (state->row_has_submenu != nullptr) {
						total_memory += state->row_count * (sizeof(bool) + sizeof(UIDrawerMenuState));
					}

					total_memory += sizeof(UIActionHandler) * state->row_count;
				}
				size_t left_character_count = strlen(state->left_characters);
				total_memory += left_character_count + 1;

				if (state->right_characters != nullptr) {
					size_t right_character_count = strlen(state->right_characters);
					total_memory += right_character_count + 1;
					// for the right substream
					if (state->row_count > 1) {
						total_memory += sizeof(unsigned short) * state->row_count;
					}
				}

				// for the left substream
				if (state->row_count > 1) {
					total_memory += sizeof(unsigned short) * state->row_count;
				}
				// for alignment
				total_memory += 8;
				return total_memory;
			}

			size_t MenuWalkStatesMemory(const UIDrawerMenuState* state, bool copy_states) {
				size_t total_memory = MenuCalculateStateMemory(state, copy_states);
				if (state->row_has_submenu != nullptr) {
					for (size_t index = 0; index < state->row_count; index++) {
						if (state->row_has_submenu[index]) {
							total_memory += MenuCalculateStateMemory(&state->submenues[index], copy_states);
							MenuWalkStatesMemory(&state->submenues[index], copy_states);
						}
					}
				}
				return total_memory;
			}

			void MenuSetStateBuffers(
				UIDrawerMenuState* state,
				uintptr_t& buffer, 
				CapacityStream<UIDrawerMenuWindow>* stream,
				bool copy_states
			) {
				size_t left_character_count = strlen(state->left_characters);
				memcpy((void*)buffer, state->left_characters, left_character_count + 1);
				state->left_characters = (char*)buffer;
				state->left_characters[left_character_count] = '\0';
				buffer += sizeof(char) * (left_character_count + 1);

				size_t right_character_count = 0;
				if (state->right_characters != nullptr) {
					right_character_count = strlen(state->right_characters);
					memcpy((void*)buffer, state->right_characters, right_character_count + 1);
					state->right_characters = (char*)buffer;
					state->right_characters[right_character_count] = '\0';
					buffer += sizeof(char) * (right_character_count + 1);
				}

				buffer = function::align_pointer(buffer, alignof(unsigned short));
				state->left_row_substreams = (unsigned short*)buffer;
				buffer += sizeof(unsigned short) * state->row_count;

				if (state->right_characters != nullptr) {
					state->right_row_substreams = (unsigned short*)buffer;
					buffer += sizeof(unsigned short) * state->row_count;
				}

				size_t new_line_count = 0;
				for (size_t index = 0; index < left_character_count; index++) {
					if (state->left_characters[index] == '\n') {
						state->left_row_substreams[new_line_count++] = index;
					}
				}
				state->left_row_substreams[new_line_count] = left_character_count;

				if (state->right_characters != nullptr) {
					new_line_count = 0;
					for (size_t index = 0; index < right_character_count; index++) {
						if (state->right_characters[index] == '\n') {
							state->right_row_substreams[new_line_count++] = index;
						}
					}
					state->right_row_substreams[new_line_count] = right_character_count;
				}

				if (copy_states) {
					if (state->row_has_submenu != nullptr) {
						memcpy((void*)buffer, state->row_has_submenu, sizeof(bool) * state->row_count);
						state->row_has_submenu = (bool*)buffer;
						buffer += sizeof(bool) * state->row_count;

						buffer = function::align_pointer(buffer, alignof(UIDrawerMenuState));
						memcpy((void*)buffer, state->submenues, sizeof(UIDrawerMenuState) * state->row_count);
						state->submenues = (UIDrawerMenuState*)buffer;
						buffer += sizeof(UIDrawerMenuState) * state->row_count;
					}

					if (state->unavailables != nullptr) {
						memcpy((void*)buffer, state->unavailables, sizeof(bool) * state->row_count);
						state->unavailables = (bool*)buffer;
						buffer += sizeof(bool) * state->row_count;
					}

					buffer = function::align_pointer(buffer, alignof(UIActionHandler));
					memcpy((void*)buffer, state->click_handlers, sizeof(UIActionHandler) * state->row_count);
					state->click_handlers = (UIActionHandler*)buffer;
					buffer += sizeof(UIActionHandler) * state->row_count;
				}

				state->windows = stream;
			}

			void MenuWalkSetStateBuffers(
				UIDrawerMenuState* state, 
				uintptr_t& buffer,
				CapacityStream<UIDrawerMenuWindow>* stream,
				bool copy_states
			) {
				MenuSetStateBuffers(state, buffer, stream, copy_states);
				if (state->row_has_submenu != nullptr) {
					for (size_t index = 0; index < state->row_count; index++) {
						if (state->row_has_submenu[index]) {
							MenuSetStateBuffers(&state->submenues[index], buffer, stream, copy_states);
						}
					}
				}
			}

			template<size_t configuration>
			UIDrawerMenu* MenuInitializer(const UIDrawConfig& config, const char* name, float2 scale, UIDrawerMenuState* menu_state) {
				UIDrawerMenu* data = nullptr;

				AddWindowResource(name, [&](const char* identifier) {
					data = GetMainAllocatorBuffer<UIDrawerMenu>();

					if constexpr (~configuration & UI_CONFIG_MENU_SPRITE) {
						char temp_characters[512];
						strcpy(temp_characters, identifier);
						strcat(temp_characters, "##Separate");

						// separate the identifier for the text label
						data->name = TextLabelInitializer<configuration>(config, temp_characters, { 0.0f, 0.0f }, scale);
					}
					data->is_opened = false;

					size_t total_memory = 0;

					total_memory = MenuWalkStatesMemory(menu_state, function::HasFlag(configuration, UI_CONFIG_MENU_COPY_STATES));
					// the capacity stream with window names, positions and scales
					total_memory += sizeof(UIDrawerMenuWindow) * ECS_TOOLS_UI_MENU_SUBMENUES_MAX_COUNT;

					void* allocation = GetMainAllocatorBuffer(total_memory);
					uintptr_t buffer = (uintptr_t)allocation;
					data->windows = CapacityStream<UIDrawerMenuWindow>((void*)buffer, 0, ECS_TOOLS_UI_MENU_SUBMENUES_MAX_COUNT);
					buffer += sizeof(UIDrawerMenuWindow) * ECS_TOOLS_UI_MENU_SUBMENUES_MAX_COUNT;

					data->state = *menu_state;
					data->state.submenu_index = 0;
					MenuWalkSetStateBuffers(&data->state, buffer, &data->windows, function::HasFlag(configuration, UI_CONFIG_MENU_COPY_STATES));

					return data;
				});
				
				return data;
			}

			template<size_t configuration, typename InputStream>
			void MultiGraphDrawer(
				const UIDrawConfig& config, 
				const char* name, 
				InputStream samples, 
				size_t y_sample_count,
				const Color* colors,
				float2 position, 
				float2 scale, 
				size_t x_axis_precision, 
				size_t y_axis_precision
			) {
				constexpr size_t label_configuration = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT
					| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;

				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
					float2 initial_scale = scale;
					if (name != nullptr) {
						TextLabel<label_configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(config, name, position, scale);
						scale = initial_scale;
					}
				}

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {
					float2 graph_scale = { scale.x - 2.0f * element_descriptor.graph_padding.x, scale.y - 2.0f * element_descriptor.graph_padding.y };
					float2 graph_position = { position.x + element_descriptor.graph_padding.x, position.y + element_descriptor.graph_padding.y };

					float2 difference;
					difference.x = samples[samples.size - 1].GetX() - samples[0].GetX();
					difference.y = 0.0f;

					float min_y = 1000000.0f;
					float max_y = -1000000.0f;
					if constexpr (configuration & UI_CONFIG_GRAPH_MIN_Y) {
						const UIConfigGraphMinY* min = (const UIConfigGraphMinY*)config.GetParameter(UI_CONFIG_GRAPH_MIN_Y);
						min_y = min->value;
					}
					if constexpr (configuration & UI_CONFIG_GRAPH_MAX_Y) {
						const UIConfigGraphMaxY* max = (const UIConfigGraphMaxY*)config.GetParameter(UI_CONFIG_GRAPH_MAX_Y);
						max_y = max->value;
					}
					for (size_t index = 0; index < samples.size; index++) {
						for (size_t sample_index = 0; sample_index < y_sample_count; sample_index++) {
							float sample_value = samples[index].GetY(sample_index);
							if constexpr (~configuration & UI_CONFIG_GRAPH_MIN_Y) {
								min_y = function::PredicateValue(min_y > sample_value, sample_value, min_y);
							}
							if constexpr (~configuration & UI_CONFIG_GRAPH_MAX_Y) {
								max_y = function::PredicateValue(max_y < sample_value, sample_value, max_y);
							}
						}
					}


					difference.y = max_y - min_y;

					Color font_color;
					float character_spacing;
					float2 font_size;

					HandleText<configuration>(config, font_color, font_size, character_spacing);
					font_color.alpha = ECS_TOOLS_UI_GRAPH_TEXT_ALPHA;

					if constexpr (configuration & UI_CONFIG_GRAPH_REDUCE_FONT) {
						font_size.x *= element_descriptor.graph_reduce_font;
						font_size.y *= element_descriptor.graph_reduce_font;
					}

					float2 axis_bump = { 0.0f, 0.0f };
					float y_sprite_size = system->GetTextSpriteYScale(font_size.y);
					if constexpr (configuration & UI_CONFIG_GRAPH_X_AXIS) {
						axis_bump.y = y_sprite_size;
					}

					if constexpr (configuration & UI_CONFIG_GRAPH_Y_AXIS) {
						float axis_span = GraphYAxis<configuration>(
							config,
							y_axis_precision,
							min_y,
							max_y,
							position,
							{ scale.x, scale.y - axis_bump.y },
							font_color,
							font_size,
							character_spacing
							);
						axis_bump.x += axis_span;
						graph_scale.x -= axis_span;
						graph_position.x += axis_span;

						char stack_memory[64];
						Stream<char> temp_stream = Stream<char>(stack_memory, 0);
						function::ConvertFloatToChars(temp_stream, samples[0].GetX(), x_axis_precision, 1);

						float2 first_sample_span = system->GetTextSpan(temp_stream.buffer, temp_stream.size, font_size.x, font_size.y, character_spacing);
						axis_bump.x -= first_sample_span.x * 0.5f;
						graph_position.y += y_sprite_size * 0.5f;
						graph_scale.y -= y_sprite_size * 0.5f;
						if constexpr (~configuration & UI_CONFIG_GRAPH_X_AXIS) {
							graph_scale.y -= y_sprite_size * 0.5f;
						}
					}

					if constexpr (configuration & UI_CONFIG_GRAPH_X_AXIS) {
						float2 spans = GraphXAxis<configuration>(
							config,
							x_axis_precision,
							samples[0].GetX(),
							samples[samples.size - 1].GetX(),
							{ position.x + element_descriptor.graph_padding.x + axis_bump.x, position.y },
							{ scale.x - element_descriptor.graph_padding.x - axis_bump.x, scale.y },
							font_color,
							font_size,
							character_spacing,
							axis_bump.x != 0.0f
							);
						graph_scale.y -= y_sprite_size + element_descriptor.graph_axis_bump.y;
						if constexpr (~configuration & UI_CONFIG_GRAPH_Y_AXIS) {
							graph_position.x += spans.x * 0.5f;
							graph_scale.x -= (spans.x + spans.y) * 0.5f;
						}
						else {
							graph_scale.x -= spans.y * 0.5f;
							graph_scale.y -= y_sprite_size * 0.5f - element_descriptor.graph_axis_value_line_size.y * 2.0f;
						}
					}

					int64_t starting_index = samples.size;
					int64_t end_index = 0;
					int64_t index = 0;
					float min_x = samples[0].GetX();
					float x_space_factor = 1.0f / difference.x * graph_scale.x;
					while ((samples[index].GetX() - min_x) * x_space_factor + graph_position.x < region_position.x && index < samples.size) {
						index++;
					}
					starting_index = function::PredicateValue(index <= 0, (int64_t)0, index - 1);
					while ((samples[index].GetX() - min_x) * x_space_factor + graph_position.x < region_limit.x && index < samples.size) {
						index++;
					}
					end_index = function::PredicateValue(index >= samples.size - 2, (int64_t)samples.size, index + 3);

					SolidColorRectangle<configuration>(config, position, scale);

					auto convert_to_graph_space_x = [](float position, float2 graph_position,
						float2 graph_scale, float2 min, float2 inverse_sample_span) {
							return graph_position.x + graph_scale.x * (position - min.x) * inverse_sample_span.x;
					};

					auto convert_to_graph_space_y = [](float position, float2 graph_position,
						float2 graph_scale, float2 min, float2 inverse_sample_span) {
							return graph_position.y + graph_scale.y * (1.0f - (position - min.y) * inverse_sample_span.y);
					};

					float max_x = samples[samples.size - 1].GetX();
					float2 inverse_sample_span = { 1.0f / (max_x - min_x), 1.0f / (max_y - min_y) };
					float2 min_values = { min_x, min_y };
					float2 previous_point_position = { 
						convert_to_graph_space_x(samples[starting_index].GetX(), graph_position, graph_scale, min_values, inverse_sample_span), 
						0.0f
					};

					Color line_color = colors[0];

					Stream<UISpriteVertex> drop_color_stream;
					if (end_index > starting_index + 1) {
						int64_t line_count = (end_index - starting_index - 1) * y_sample_count;
						SetSpriteClusterTexture<configuration>(ECS_TOOLS_UI_TEXTURE_MASK, line_count);
						drop_color_stream = GetSpriteClusterStream<configuration>(line_count * 6);
						auto sprite_count = HandleSpriteClusterCount<configuration>();
						*sprite_count += line_count * 6;
					}

					for (int64_t index = starting_index; index < end_index - 1; index++) {
						float2 next_point = { 
							convert_to_graph_space_x(samples[index + 1].GetX(), graph_position, graph_scale, min_values, inverse_sample_span), 
							0.0f 
						};

						for (size_t sample_index = 0; sample_index < y_sample_count; sample_index++) {
							previous_point_position.y = convert_to_graph_space_y(samples[index].GetY(sample_index), graph_position, graph_scale, min_values, inverse_sample_span);
							next_point.y = convert_to_graph_space_y(samples[index + 1].GetY(sample_index), graph_position, graph_scale, min_values, inverse_sample_span);
							line_color = colors[sample_index];
							
							Line<configuration>(previous_point_position, next_point, line_color);

							line_color.alpha = ECS_TOOLS_UI_GRAPH_DROP_COLOR_ALPHA;
							size_t base_index = (index - starting_index) * 6 * y_sample_count + sample_index * 6;
							drop_color_stream[base_index].SetTransform(previous_point_position);
							drop_color_stream[base_index + 1].SetTransform(next_point);
							drop_color_stream[base_index + 2].SetTransform({ next_point.x, graph_position.y + graph_scale.y });
							drop_color_stream[base_index + 3].SetTransform(previous_point_position);
							drop_color_stream[base_index + 4].SetTransform({ next_point.x, graph_position.y + graph_scale.y });
							drop_color_stream[base_index + 5].SetTransform({ previous_point_position.x, graph_position.y + graph_scale.y });
							SetUVForRectangle({ 0.0f, 0.0f }, { 1.0f, 1.0f }, base_index, drop_color_stream.buffer);
							SetColorForRectangle(line_color, base_index, drop_color_stream.buffer);
						}

						previous_point_position.x = next_point.x;
					}

					if constexpr (configuration & UI_CONFIG_GRAPH_TAGS) {
						const UIConfigGraphTags* tags = (const UIConfigGraphTags*)config.GetParameter(UI_CONFIG_GRAPH_TAGS);

						ActionData action_data;
						UIDrawerGraphTagData tag_data;
						tag_data.drawer = this;
						tag_data.graph_position = graph_position;
						tag_data.graph_position += region_render_offset;
						tag_data.graph_scale = graph_scale;
						action_data.data = &tag_data;
						for (size_t index = 0; index < tags->vertical_tag_count; index++) {
							float2 position = { 
								convert_to_graph_space_x(min_x, graph_position, graph_scale, min_values, inverse_sample_span), 
								convert_to_graph_space_y(tags->vertical_values[index], graph_position, graph_scale, min_values, inverse_sample_span) 
							};
							tag_data.position = position;
							tag_data.position += region_render_offset;
							tags->vertical_tags[index](&action_data);
						}
						for (size_t index = 0; index < tags->horizontal_tag_count; index++) {
							float2 position = {
								convert_to_graph_space_x(tags->horizontal_values[index], graph_position, graph_scale, min_values, inverse_sample_span),
								convert_to_graph_space_y(min_y, graph_position, graph_scale, min_values, inverse_sample_span)
							};
							tag_data.position = position;
							tag_data.position += region_render_offset;
							tags->horizontal_tags[index](&action_data);
						}
					}

				}

				FinalizeRectangle<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(position, scale);

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
					if (name != nullptr) {
						position.x += scale.x + layout.element_indentation;
						TextLabel<label_configuration>(config, name, position, scale);
					}
				}
			}

			template<size_t configuration>
			void* SentenceInitializer(const UIDrawConfig& config, const char* text) {
				if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerSentenceCached* data = nullptr;

					AddWindowResource(text, [&](const char* identifier) {
						data = GetMainAllocatorBuffer<UIDrawerSentenceCached>();
						
						// getting the whitespace characters count to preallocate the buffers accordingly
						size_t sentence_length = ParseStringIdentifier(identifier, strlen(identifier));
						size_t whitespace_characters = function::ParseWordsFromSentence(identifier);

						data->base.whitespace_characters.buffer = (UIDrawerWhitespaceCharacter*)GetMainAllocatorBuffer(sizeof(UISpriteVertex) * sentence_length * 6
							+ sizeof(UIDrawerWhitespaceCharacter) * (whitespace_characters + 1));
						data->base.whitespace_characters.size = 0;

						// setting up the buffers
						uintptr_t buffer = (uintptr_t)data->base.whitespace_characters.buffer;

						buffer += sizeof(UIDrawerWhitespaceCharacter) * (whitespace_characters + 1);

						data->base.vertices.buffer = (UISpriteVertex*)buffer;
						data->base.vertices.size = 0;

						buffer += sizeof(UISpriteVertex) * sentence_length * 6;

						data->base.SetWhitespaceCharacters(identifier, sentence_length);

						Color text_color;
						float character_spacing;
						float2 font_size;
						HandleText<configuration>(config, text_color, font_size, character_spacing);

						// converting the characters in a continouos fashion
						system->ConvertCharactersToTextSprites(
							identifier,
							{ 0.0f, 0.0f },
							data->base.vertices.buffer,
							sentence_length,
							text_color,
							0,
							font_size,
							character_spacing
						);
						data->base.vertices.size = sentence_length * 6;

						// setting the zoom
						data->zoom = *zoom_ptr;
						data->inverse_zoom = { 1.0f / zoom_ptr->x, 1.0f / zoom_ptr->y };

						return data;
					});

					return data;
				}
				return nullptr;
			}

			size_t SentenceWhitespaceCharactersCount(const char* identifier, CapacityStream<unsigned int> stack_buffer) {
				return function::FindWhitespaceCharactersCount(identifier, &stack_buffer);
			}

			void SentenceNonCachedInitializerKernel(const char* identifier, UIDrawerSentenceNotCached* data) {
				size_t identifier_length = ParseStringIdentifier(identifier, strlen(identifier));
				char* temp_chars = (char*)GetTempBuffer(identifier_length + 1);
				memcpy(temp_chars, identifier, identifier_length);
				temp_chars[identifier_length + 1] = '\0';
				size_t space_count = function::ParseWordsFromSentence(temp_chars);

				function::FindWhitespaceCharacters(temp_chars, data->whitespace_characters);
				data->whitespace_characters.Add(identifier_length);
			}

			template<size_t configuration>
			void SentenceDrawer(const UIDrawConfig& config, const char* identifier, void* resource, float2 position) {
				UIConfigSentenceHoverableHandlers* hoverables = nullptr;
				UIConfigSentenceClickableHandlers* clickables = nullptr;
				UIConfigSentenceGeneralHandlers* generals = nullptr;

				if constexpr (configuration & UI_CONFIG_SENTENCE_HOVERABLE_HANDLERS) {
					hoverables = (UIConfigSentenceHoverableHandlers*)config.GetParameter(UI_CONFIG_SENTENCE_HOVERABLE_HANDLERS);
				}

				if constexpr (configuration & UI_CONFIG_SENTENCE_CLICKABLE_HANDLERS) {
					clickables = (UIConfigSentenceClickableHandlers*)config.GetParameter(UI_CONFIG_SENTENCE_CLICKABLE_HANDLERS);
				}

				if constexpr (configuration & UI_CONFIG_SENTENCE_GENERAL_HANDLERS) {
					generals = (UIConfigSentenceGeneralHandlers*)config.GetParameter(UI_CONFIG_SENTENCE_GENERAL_HANDLERS);
				}

				auto handlers = [&](float2 handler_position, float2 handler_scale, unsigned int line_count) {
					if constexpr (configuration & UI_CONFIG_SENTENCE_HOVERABLE_HANDLERS) {
						if (hoverables->callback[line_count] != nullptr) {
							AddHoverable(handler_position, handler_scale, { hoverables->callback[line_count], hoverables->data[line_count], hoverables->data_size[line_count], hoverables->phase[line_count] });
						}
					}
					if constexpr (configuration & UI_CONFIG_SENTENCE_CLICKABLE_HANDLERS) {
						if (clickables->callback[line_count] != nullptr) {
							AddClickable(handler_position, handler_scale, { clickables->callback[line_count], clickables->data[line_count], clickables->data_size[line_count], clickables->phase[line_count] });
						}
					}
					if constexpr (configuration & UI_CONFIG_SENTENCE_GENERAL_HANDLERS) {
						if (generals->callback[line_count] != nullptr) {
							AddGeneral(handler_position, handler_scale, { generals->callback[line_count], generals->data[line_count], generals->data_size[line_count], generals->phase[line_count] });
						}
					}
				};

				unsigned int line_count = 0;

				if constexpr (configuration & UI_CONFIG_DO_NOT_CACHE) {
#pragma region Non cached
					
					constexpr size_t WHITESPACE_ALLOCATION_SIZE = ECS_KB * 50;
					unsigned int _whitespace_characters[4096];
					CapacityStream<unsigned int> whitespace_characters(_whitespace_characters, 0, 4096);
					whitespace_characters.size = SentenceWhitespaceCharactersCount(identifier, whitespace_characters);
					if (whitespace_characters.size > whitespace_characters.capacity) {
						unsigned int* new_whitespace_characters = (unsigned int*)GetTempBuffer(sizeof(unsigned int) * whitespace_characters.size);
						UIDrawerSentenceNotCached data;
						data.whitespace_characters = Stream<unsigned int>(new_whitespace_characters, 0);
						SentenceNonCachedInitializerKernel(identifier, &data);
						whitespace_characters.buffer = new_whitespace_characters;
					}
					else {
						whitespace_characters.Add(ParseStringIdentifier(identifier, strlen(identifier)));
					}

					float y_row_scale = current_row_y_scale;

					UIDrawerMode previous_draw_mode = draw_mode;
					unsigned int previous_draw_count = draw_mode_count;
					unsigned int previous_draw_target = draw_mode_target;
					if constexpr (configuration & UI_CONFIG_SENTENCE_FIT_SPACE) {
						SetDrawMode(UIDrawerMode::FitSpace);
					}

					size_t text_length = ParseStringIdentifier(identifier, strlen(identifier));
					size_t word_start_index = 0;
					size_t word_end_index = 0;

					char* temp_chars = (char*)GetTempBuffer(text_length + 1);
					memcpy(temp_chars, identifier, text_length);

					Color font_color;
					float2 font_size;
					float character_spacing;

					HandleText<configuration>(config, font_color, font_size, character_spacing);

					float starting_row_position = position.x;
					float space_x_scale = system->GetSpaceXSpan(font_size.x);
					for (size_t index = 0; index < whitespace_characters.size; index++) {
						if (whitespace_characters[index] != 0) {
							word_end_index = whitespace_characters[index] - 1;
						}
						if (word_end_index >= word_start_index) {
							temp_chars[word_end_index + 1] = '\0';
							float2 text_span;

							Stream<UISpriteVertex> current_vertices;
							if constexpr (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
								current_vertices = GetTextStream<configuration>((word_end_index - word_start_index + 1) * 6);
							}
							Text<configuration>(config, temp_chars + word_start_index, position, text_span);

							if constexpr (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
								current_row_y_scale = y_row_scale;
								float y_position = AlignMiddle<float>(position.y, y_row_scale, text_span.y);
								TranslateTextY(y_position, 0, current_vertices);
							}

							FinalizeRectangle<0>(position, text_span);
							position.x += text_span.x;
						}

						while (whitespace_characters[index] == whitespace_characters[index + 1] - 1 && index < whitespace_characters.size - 1) {
							if (identifier[whitespace_characters[index]] == ' ') {
								position.x += character_spacing + space_x_scale;
							}
							else {
								float2 handler_position = { starting_row_position, position.y };
								float2 handler_scale = { position.x - starting_row_position, y_row_scale };
								handlers(handler_position, handler_scale, line_count);
								line_count++;

								NextRow();
								position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
								starting_row_position = position.x;
								if constexpr (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
									current_row_y_scale = y_row_scale;
								}
							}
							index++;
						}
						if (identifier[whitespace_characters[index]] == ' ') {
							position.x += character_spacing + space_x_scale;
						}
						else if (identifier[whitespace_characters[index]] == '\n'){
							float2 handler_position = { starting_row_position, position.y };
							float2 handler_scale = { position.x - starting_row_position, y_row_scale };
							handlers(handler_position, handler_scale, line_count);
							line_count++;

							NextRow();
							position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
							starting_row_position = position.x;
						}
						word_start_index = whitespace_characters[index] + 1;
					}

					if constexpr (configuration & UI_CONFIG_SENTENCE_FIT_SPACE) {
						draw_mode = previous_draw_mode;
						draw_mode_count = previous_draw_count;
						draw_mode_target = previous_draw_target;
					}

#pragma endregion

				}
				else {

#pragma region Cached

					UIDrawerMode previous_draw_mode = draw_mode;
					unsigned int previous_draw_count = draw_mode_count;
					unsigned int previous_draw_target = draw_mode_target;
					if constexpr (configuration & UI_CONFIG_SENTENCE_FIT_SPACE) {
						SetDrawMode(UIDrawerMode::FitSpace);
					}

					float row_y_scale = current_row_y_scale;

					UIDrawerSentenceCached* data = (UIDrawerSentenceCached*)resource;

					if (ValidatePosition<configuration>(position)) {
						float character_spacing;
						Color font_color;
						float2 font_size;
						HandleText<configuration>(config, font_color, font_size, character_spacing);

						if (data->base.vertices[0].color != font_color) {
							for (size_t index = 0; index < data->base.vertices.size; index++) {
								data->base.vertices[index].color = font_color;
							}
						}
						auto text_sprite_buffer = HandleTextSpriteBuffer<configuration>();
						auto text_sprite_counts = HandleTextSpriteCount<configuration>();

						float space_x_scale = system->GetSpaceXSpan(font_size.x);
						size_t word_end_index = 0;
						size_t word_start_index = 0;

						Stream<UISpriteVertex> temp_stream = Stream<UISpriteVertex>(nullptr, 0);
						const bool is_zoom_different = data->zoom.x != zoom_ptr->x || data->zoom.y != zoom_ptr->y;
						float starting_row_position = position.x;

						for (size_t index = 0; index < data->base.whitespace_characters.size; index++) {
							if (data->base.whitespace_characters[index].position != 0) {
								word_end_index = data->base.whitespace_characters[index].position - 1;
							}
							if (word_end_index >= word_start_index) {
								float2 text_span;
								temp_stream.buffer = data->base.vertices.buffer + word_start_index * 6;
								temp_stream.size = (word_end_index - word_start_index + 1) * 6;

								if (is_zoom_different) {
									auto text_sprite_stream = GetTextStream<configuration>(0);
									ScaleText(temp_stream, data->zoom, data->inverse_zoom, text_sprite_buffer, text_sprite_counts, zoom_ptr, character_spacing);
									memcpy(temp_stream.buffer, text_sprite_stream.buffer, sizeof(UISpriteVertex) * temp_stream.size);
								}
								text_span = GetTextSpan(temp_stream);

								HandleFitSpaceRectangle<configuration>(position, text_span);
								if constexpr (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
									position.y = AlignMiddle(position.y, row_y_scale, text_span.y);
								}

								if (position.x != temp_stream[0].position.x || position.y != -temp_stream[0].position.y) {
									TranslateText(position.x, position.y, temp_stream, 0, 0);
								}
								memcpy(text_sprite_buffer + *text_sprite_counts, temp_stream.buffer, sizeof(UISpriteVertex) * temp_stream.size);
								*text_sprite_counts += temp_stream.size;

								FinalizeRectangle<0>(position, text_span);
								position.x += text_span.x;
							}

							while (data->base.whitespace_characters[index].position == data->base.whitespace_characters[index + 1].position - 1) {
								if (data->base.whitespace_characters[index].type == ' ') {
									position.x += character_spacing + space_x_scale;
								}
								else {
									float2 handler_position = { starting_row_position, position.y };
									float2 handler_scale = { position.x - starting_row_position, row_y_scale };
									handlers(handler_position, handler_scale, line_count);
									line_count++;

									NextRow();
									position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
									starting_row_position = position.x;
									if constexpr (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
										current_row_y_scale = row_y_scale;
									}
								}
								index++;
							}
							if (data->base.whitespace_characters[index].type == ' ') {
								position.x += character_spacing + space_x_scale;
							}
							else if (data->base.whitespace_characters[index].type == '\n'){
								float2 handler_position = { starting_row_position, position.y };
								float2 handler_scale = { position.x - starting_row_position, row_y_scale };
								handlers(handler_position, handler_scale, line_count);
								line_count++;

								NextRow();
								position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
								starting_row_position = position.x;
							}
							word_start_index = data->base.whitespace_characters[index].position + 1;
						}

						if (is_zoom_different) {
							data->zoom = *zoom_ptr;
							data->inverse_zoom = zoom_inverse;
						}

						if constexpr (configuration & UI_CONFIG_SENTENCE_FIT_SPACE) {
							draw_mode = previous_draw_mode;
							draw_mode_count = previous_draw_count;
							draw_mode_target = previous_draw_target;
						}
					}
				}

				if constexpr (configuration & UI_CONFIG_SENTENCE_HOVERABLE_HANDLERS) {
					ECS_ASSERT(line_count == hoverables->count);
				}
				if constexpr (configuration & UI_CONFIG_SENTENCE_CLICKABLE_HANDLERS) {
					ECS_ASSERT(line_count == clickables->count);
				}
				if constexpr (configuration & UI_CONFIG_SENTENCE_GENERAL_HANDLERS) {
					ECS_ASSERT(line_count == generals->count);
				}

#pragma endregion

			}

			template<size_t configuration>
			UIDrawerTextTable* TextTableInitializer(const UIDrawConfig& config, const char* name, unsigned int rows, unsigned int columns, const char** labels) {
				UIDrawerTextTable* data = nullptr;

				AddWindowResource(name, [&](const char* identifier) {
					data = GetMainAllocatorBuffer<UIDrawerTextTable>();

					data->zoom = *zoom_ptr;
					data->inverse_zoom = zoom_inverse;

					unsigned int cell_count = rows * columns;
					unsigned int* label_sizes = (unsigned int*)GetTempBuffer(sizeof(unsigned int) * cell_count, alignof(unsigned int));
					size_t total_memory_size = 0;
					for (size_t index = 0; index < cell_count; index++) {
						label_sizes[index] = strlen(labels[index]);
						total_memory_size += label_sizes[index];
					}

					size_t total_char_count = total_memory_size;
					total_memory_size *= 6 * sizeof(UISpriteVertex);
					total_memory_size += sizeof(Stream<UISpriteVertex>) * cell_count;

					if constexpr (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
						total_memory_size += sizeof(float) * columns;
					}
						
					void* allocation = GetMainAllocatorBuffer(total_memory_size);
					uintptr_t buffer = (uintptr_t)allocation;

					data->labels.buffer = (Stream<UISpriteVertex>*)buffer;
					data->labels.size = cell_count;
					buffer += sizeof(Stream<UISpriteVertex>) * cell_count;

					if constexpr (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
						data->column_scales.buffer = (float*)buffer;
						buffer += sizeof(float) * columns;
					}

					Color font_color;
					float character_spacing;
					float2 font_size;

					HandleText<configuration>(config, font_color, font_size, character_spacing);

					for (size_t index = 0; index < cell_count; index++) {
						data->labels[index].buffer = (UISpriteVertex*)buffer;
						system->ConvertCharactersToTextSprites(
							labels[index], 
							{ 0.0f, 0.0f }, 
							data->labels[index].buffer,
							label_sizes[index],
							font_color, 
							0, 
							font_size,
							character_spacing
						);

						data->labels[index].size = label_sizes[index] * 6;
						buffer += sizeof(UISpriteVertex) * data->labels[index].size;
						if constexpr (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
							float2 text_span = GetTextSpan(data->labels[index]);
							unsigned int column_index = index % columns;
							if (data->column_scales[column_index] < text_span.x + ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR * element_descriptor.label_horizontal_padd) {
								data->column_scales[column_index] = text_span.x + ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR * element_descriptor.label_horizontal_padd;
							}
							data->row_scale = text_span.y + element_descriptor.label_vertical_padd * ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR;
						}
					}

					return data;
				});
				
				return data;
			}

			template<size_t configuration>
			void TextTableDrawer(
				const UIDrawConfig& config, 
				float2 position, 
				float2 scale,
				UIDrawerTextTable* data,
				unsigned int rows,
				unsigned int columns
			) {
				if constexpr (~configuration & UI_CONFIG_TEXT_TABLE_DO_NOT_INFER) {
					scale = GetSquareScale(scale.y * ECS_TOOLS_UI_TEXT_TABLE_INFER_FACTOR);
				}

				if (zoom_ptr->x != data->zoom.x || zoom_ptr->y != data->zoom.y) {
					auto text_sprite_stream = GetTextStream<configuration>(0);

					Color font_color;
					float character_spacing;
					float2 font_size;

					HandleText<configuration>(config, font_color, font_size, character_spacing);
					for (size_t index = 0; index < data->labels.size; index++) {
						HandleTextStreamColorUpdate(font_color, data->labels[index]);
					}

					unsigned int cell_count = rows * columns;
					for (size_t index = 0; index < data->labels.size; index++) {
						ScaleText(data->labels[index], data->zoom, data->inverse_zoom, text_sprite_stream.buffer, &text_sprite_stream.size, zoom_ptr, character_spacing);
						memcpy(data->labels[index].buffer, text_sprite_stream.buffer, sizeof(UISpriteVertex) * data->labels[index].size);
					}
					float zoom_factor = zoom_ptr->x * data->inverse_zoom.x;
					for (size_t index = 0; index < columns; index++) {
						data->column_scales[index] *= zoom_factor;
					}
					data->row_scale = zoom_ptr->y * data->inverse_zoom.y;

					data->zoom = *zoom_ptr;
					data->inverse_zoom = zoom_inverse;
				}

				if constexpr (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
					scale.y = data->row_scale;
				}

				float2 initial_position = position;
				auto text_sprite_buffer = HandleTextSpriteBuffer<configuration>();
				auto text_sprite_count = HandleTextSpriteCount<configuration>();

				Color border_color;
				float2 border_size;

				if constexpr (configuration & UI_CONFIG_BORDER) {
					const UIConfigBorder* parameters = (const UIConfigBorder*)config.GetParameter(UI_CONFIG_BORDER);

					float2 border_size = GetSquareScale(parameters->thickness);
					border_size.x *= zoom_ptr->x;
					border_size.y *= zoom_ptr->y;
				}
				else {
					border_color = color_theme.borders;
					border_size = GetSquareScale(system->m_descriptors.dockspaces.border_size);
				}
				for (size_t row = 0; row < rows; row++) {
					if constexpr (((configuration & UI_CONFIG_TEXT_TABLE_NO_BORDER) != 0) && ((configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) == 0)) {
						CreateSolidColorRectangleBorder<false>(position, {scale.x * columns, scale.y}, border_size, border_color, counts, buffers);
					}

					for (size_t column = 0; column < columns; column++) {
						if constexpr (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
							scale.x = data->column_scales[column];
						}

						if (ValidatePosition<0>(position, scale)) {
							unsigned int cell_index = row * columns + column;
							float2 text_span = GetTextSpan(data->labels[cell_index]);
							float x_position, y_position;
							HandleTextLabelAlignment(text_span, scale, position, x_position, y_position, TextAlignment::Middle, TextAlignment::Middle);

							TranslateText(x_position, y_position, data->labels[cell_index], 0, 0);
							memcpy(text_sprite_buffer + *text_sprite_count, data->labels[cell_index].buffer, sizeof(UISpriteVertex) * data->labels[cell_index].size);
							*text_sprite_count += data->labels[cell_index].size;

							if constexpr (configuration & UI_CONFIG_COLOR) {
								SolidColorRectangle<configuration>(config, position, scale);
							}
						}
						FinalizeRectangle<0>(position, scale);
						position.x += scale.x;
					}
					position.x = initial_position.x;
					position.y += scale.y;
				}

				position = initial_position;
				if constexpr (~configuration & UI_CONFIG_TEXT_TABLE_NO_BORDER) {
					for (size_t column = 0; column < columns; column++) {
						if constexpr (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
							scale.x = data->column_scales[column];
						}
						CreateSolidColorRectangleBorder<false>(position, {scale.x, scale.y * rows}, border_size, border_color, counts, buffers);
						position.x += scale.x;
					}
				}

				if constexpr (((configuration & UI_CONFIG_TEXT_TABLE_NO_BORDER) == 0) && ((configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE))) {
					position = initial_position;
					float total_column_length = 0.0f;
					for (size_t column = 0; column < columns; column++) {
						total_column_length += data->column_scales[column];
					}
					for (size_t row = 0; row < rows; row++) {
						CreateSolidColorRectangleBorder<false>(position, { total_column_length, scale.y }, border_size, border_color, counts, buffers);
						position.y += scale.y;
					}
				}
			}

			template<size_t configuration>
			void TextTableDrawer(
				const UIDrawConfig& config, 
				float2 position, 
				float2 scale, 
				unsigned int rows, 
				unsigned int columns,
				const char**& labels
			) {
				if constexpr (~configuration & UI_CONFIG_TEXT_TABLE_DO_NOT_INFER) {
					scale = GetSquareScale(scale.y * ECS_TOOLS_UI_TEXT_TABLE_INFER_FACTOR);
				}

				unsigned int cell_count = rows * columns;

				Stream<UISpriteVertex>* cells = (Stream<UISpriteVertex>*)GetTempBuffer(sizeof(Stream<UISpriteVertex>) * cell_count);

				unsigned int* label_sizes = (unsigned int*)GetTempBuffer(sizeof(unsigned int) * cell_count);

				Color font_color;
				float character_spacing;
				float2 font_size;

				HandleText<configuration>(config, font_color, font_size, character_spacing);

				Color border_color;
				float2 border_size;
				if constexpr (configuration & UI_CONFIG_BORDER) {
					const UIConfigBorder* parameters = (const UIConfigBorder*)config.GetParameter(UI_CONFIG_BORDER);

					float2 border_size = GetSquareScale(parameters->thickness);
					border_size.x *= zoom_ptr->x;
					border_size.y *= zoom_ptr->y;
				}
				else {
					border_color = color_theme.borders;
					border_size = GetSquareScale(system->m_descriptors.dockspaces.border_size);
				}
				float2 initial_position = position;
				float* column_biggest_scale = (float*)GetTempBuffer(sizeof(float) * columns);

				if constexpr (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
					float* cell_scales = (float*)GetTempBuffer(sizeof(float) * cell_count);

					for (size_t index = 0; index < columns; index++) {
						column_biggest_scale[index] = 0.0f;
					}

					float y_text_span;
					auto text_sprite_count = HandleTextSpriteCount<configuration>();
					size_t before_count = *text_sprite_count;
					for (size_t row = 0; row < rows; row++) {
						for (size_t column = 0; column < columns; column++) {
							unsigned int index = row * columns + column;
							label_sizes[index] = strlen(labels[index]);
							cells[index] = GetTextStream<configuration>(label_sizes[index] * 6);
							system->ConvertCharactersToTextSprites(
								labels[index],
								{ 0.0f, 0.0f },
								cells[index].buffer,
								label_sizes[index],
								font_color,
								0,
								font_size,
								character_spacing
							);
							float2 text_span = GetTextSpan(cells[index]);
							float current_x_scale = text_span.x + ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR * element_descriptor.label_horizontal_padd;
							column_biggest_scale[column] = function::PredicateValue(current_x_scale > column_biggest_scale[column], current_x_scale, column_biggest_scale[column]);
							cell_scales[index] = text_span.x;
							y_text_span = text_span.y;
							scale.y = text_span.y + ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR * element_descriptor.label_vertical_padd;
							*text_sprite_count += cells[index].size;
						}
					}

					*text_sprite_count = before_count;
					unsigned int validated_cell_index = 0;

					auto text_sprite_buffer = HandleTextSpriteBuffer<configuration>();
					for (size_t row = 0; row < rows; row++) {
						for (size_t column = 0; column < columns; column++) {
							scale.x = column_biggest_scale[column];
							if (ValidatePosition<0>(position, scale)) {
								unsigned int cell_index = row * columns + column;
								float x_position, y_position;
								HandleTextLabelAlignment({cell_scales[cell_index], y_text_span}, scale, position, x_position, y_position, TextAlignment::Middle, TextAlignment::Middle);

								TranslateText(x_position, y_position, cells[cell_index], 0, 0);
								if (validated_cell_index != cell_index) {
									memcpy(text_sprite_buffer + *text_sprite_count, cells[cell_index].buffer, sizeof(UISpriteVertex) * cells[cell_index].size);
								}
								validated_cell_index++;
								*text_sprite_count += cells[cell_index].size;

								if constexpr (configuration & UI_CONFIG_COLOR) {
									SolidColorRectangle<configuration>(config, position, scale);
								}
							}
							FinalizeRectangle<0>(position, scale);
							position.x += scale.x;
						}

						position.x = initial_position.x;
						position.y += scale.y;
					}

					if constexpr ((configuration & UI_CONFIG_TEXT_TABLE_NO_BORDER) == 0) {
						position = initial_position;
						float total_column_length = 0.0f;
						for (size_t column = 0; column < columns; column++) {
							total_column_length += column_biggest_scale[column];
						}
						for (size_t row = 0; row < rows; row++) {
							CreateSolidColorRectangleBorder<false>(position, { total_column_length, scale.y }, border_size, border_color, counts, buffers);
							position.y += scale.y;
						}
					}
				}

				position = initial_position;
				if constexpr (~configuration & UI_CONFIG_TEXT_TABLE_NO_BORDER) {
					for (size_t column = 0; column < columns; column++) {
						if constexpr (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
							scale.x = column_biggest_scale[column];
						}
						CreateSolidColorRectangleBorder<false>(position, { scale.x, scale.y * rows }, border_size, border_color, counts, buffers);
						position.x += scale.x;
					}
				}
			}

			template<size_t configuration>
			float GraphYAxis(
				const UIDrawConfig& config,
				size_t y_axis_precision,
				float min_y,
				float max_y, 
				float2 position, 
				float2 scale,
				Color font_color, 
				float2 font_size,
				float character_spacing
			) {
				float y_sprite_scale = system->GetTextSpriteYScale(font_size.y);

				// upper corner value
				char temp_memory[128];
				Stream<char> temp_float_stream = Stream<char>(temp_memory, 0);
				function::ConvertFloatToChars(temp_float_stream, max_y, y_axis_precision);

				size_t* text_count = HandleTextSpriteCount<configuration>();
				auto text_buffer = HandleTextSpriteBuffer<configuration>();

				float2 top_text_position = { position.x + element_descriptor.graph_padding.x, position.y + element_descriptor.graph_padding.y };
				float max_x_scale = 0.0f;
				system->ConvertCharactersToTextSprites(
					temp_float_stream.buffer,
					top_text_position,
					text_buffer,
					temp_float_stream.size,
					font_color,
					*text_count,
					font_size,
					character_spacing
				);

				auto top_vertices_stream = GetTextStream<configuration>(temp_float_stream.size * 6);
				float2 text_span = GetTextSpan(top_vertices_stream);
				max_x_scale = function::PredicateValue(max_x_scale < text_span.x, text_span.x, max_x_scale);
				*text_count += 6 * temp_float_stream.size;

				float2 bottom_text_position = { top_text_position.x, position.y + scale.y - y_sprite_scale - element_descriptor.graph_padding.y };
				temp_float_stream.size = 0;
				function::ConvertFloatToChars(temp_float_stream, min_y, y_axis_precision);
				system->ConvertCharactersToTextSprites(
					temp_float_stream.buffer,
					bottom_text_position,
					text_buffer,
					temp_float_stream.size,
					font_color,
					*text_count,
					font_size,
					character_spacing
				);

				auto bottom_vertices_stream = GetTextStream<configuration>(temp_float_stream.size * 6);
				text_span = GetTextSpan(bottom_vertices_stream);
				if (max_x_scale < text_span.x) {
					float top_scale = max_x_scale;
					max_x_scale = text_span.x;
					TranslateText(top_text_position.x + max_x_scale - top_scale, top_text_position.y, top_vertices_stream, 0, 0);
				}
				else {
					TranslateText(bottom_text_position.x + max_x_scale - text_span.x, bottom_text_position.y, bottom_vertices_stream, 0, 0);
				}
				*text_count += temp_float_stream.size * 6;

				// top text strip
				SpriteRectangle<configuration>(
					{ top_text_position.x + max_x_scale + element_descriptor.graph_axis_bump.x * 0.5f, AlignMiddle(top_text_position.y, y_sprite_scale, element_descriptor.graph_axis_value_line_size.y) },
					{ position.x + scale.x - top_text_position.x - max_x_scale - element_descriptor.graph_padding.x, element_descriptor.graph_axis_value_line_size.y },
					ECS_TOOLS_UI_TEXTURE_MASK,
					{ 0.0f, 0.0f },
					{ 1.0f, 1.0f },
					font_color
				);

				// bottom text strip
				SpriteRectangle<configuration>(
					{ bottom_text_position.x + max_x_scale + 0.001f, AlignMiddle(bottom_text_position.y, y_sprite_scale, element_descriptor.graph_axis_value_line_size.y) },
					{ position.x + scale.x - bottom_text_position.x - max_x_scale - element_descriptor.graph_padding.x, element_descriptor.graph_axis_value_line_size.y },
					ECS_TOOLS_UI_TEXTURE_MASK,
					{ 0.0f, 0.0f },
					{ 1.0f, 1.0f },
					font_color
				);

				float remaining_y_scale = bottom_text_position.y - y_sprite_scale - top_text_position.y;
				int64_t sprite_count = static_cast<int64_t>((remaining_y_scale) / (y_sprite_scale * 2.0f));
				float total_sprite_y_scale = sprite_count * y_sprite_scale;
				float alignment_offset = (remaining_y_scale - total_sprite_y_scale) / (sprite_count + 1);

				float2 sprite_position = { top_text_position.x, top_text_position.y + y_sprite_scale + alignment_offset };
				float step = 1.0f / (sprite_count + 1);
				for (int64_t index = 0; index < sprite_count; index++) {
					float value = function::Lerp(min_y, max_y, 1.0f - (index + 1) * step);
					temp_float_stream.size = 0;
					function::ConvertFloatToChars(temp_float_stream, value, y_axis_precision);

					auto temp_vertices = GetTextStream<configuration>(temp_float_stream.size * 6);
					system->ConvertCharactersToTextSprites(
						temp_float_stream.buffer,
						sprite_position,
						text_buffer,
						temp_float_stream.size,
						font_color,
						*text_count,
						font_size,
						character_spacing
					);
					float2 current_span = GetTextSpan(temp_vertices);
					TranslateText(sprite_position.x + max_x_scale - current_span.x, sprite_position.y, temp_vertices, 0, 0);

					*text_count += 6 * temp_float_stream.size;

					SpriteRectangle<configuration>(
						{ sprite_position.x + max_x_scale, AlignMiddle(sprite_position.y, y_sprite_scale, element_descriptor.graph_axis_value_line_size.y) },
						{ position.x + scale.x - sprite_position.x - max_x_scale - element_descriptor.graph_padding.x, element_descriptor.graph_axis_value_line_size.y },
						ECS_TOOLS_UI_TEXTURE_MASK, 
						{ 0.0f, 0.0f }, 
						{ 1.0f, 1.0f }, 
						font_color
					);

					sprite_position.y += y_sprite_scale + alignment_offset;
				}

				SpriteRectangle<configuration>(
					{ top_text_position.x + max_x_scale + element_descriptor.graph_axis_bump.x, top_text_position.y + y_sprite_scale * 0.5f },
					{ 
						element_descriptor.graph_axis_value_line_size.x,
						bottom_text_position.y - top_text_position.y
					},
					ECS_TOOLS_UI_TEXTURE_MASK,
					{ 1.0f, 0.0f },
					{ 0.0f, 1.0f },
					font_color
				);

				return max_x_scale + element_descriptor.graph_axis_value_line_size.x + 0.001f;
			}

			// it returns the left and the right text spans in order to adjust the graph scale and position
			template<size_t configuration>
			float2 GraphXAxis(
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
			) {
				float y_sprite_scale = system->GetTextSpriteYScale(font_size.y);

				// upper corner value
				char temp_memory[128];
				Stream<char> temp_float_stream = Stream<char>(temp_memory, 0);
				function::ConvertFloatToChars(temp_float_stream, min_x, x_axis_precision);

				size_t* text_count = HandleTextSpriteCount<configuration>();
				auto text_buffer = HandleTextSpriteBuffer<configuration>();

				float text_y = position.y + scale.y - y_sprite_scale - element_descriptor.graph_padding.y;

				system->ConvertCharactersToTextSprites(
					temp_float_stream.buffer,
					{position.x, text_y},
					text_buffer,
					temp_float_stream.size,
					font_color,
					*text_count,
					font_size,
					character_spacing
				);

				auto left_vertices_stream = GetTextStream<configuration>(temp_float_stream.size * 6);
				float2 left_span = GetTextSpan(left_vertices_stream);
				*text_count += 6 * temp_float_stream.size;

				float2 vertical_line_scale = { element_descriptor.graph_axis_value_line_size.x, scale.y - 2 * element_descriptor.graph_padding.y - y_sprite_scale - element_descriptor.graph_axis_bump.y };
				if (!disable_horizontal_line) {
					SpriteRectangle<configuration>(
						{ AlignMiddle(position.x, left_span.x, vertical_line_scale.x), position.y + element_descriptor.graph_padding.y },
						vertical_line_scale,
						ECS_TOOLS_UI_TEXTURE_MASK,
						{ 0.0f, 0.0f },
						{ 1.0f, 1.0f },
						font_color
						);
				}

				temp_float_stream.size = 0;
				function::ConvertFloatToChars(temp_float_stream, max_x, x_axis_precision);

				float2 right_span = system->GetTextSpan(temp_float_stream.buffer, temp_float_stream.size, font_size.x, font_size.y, character_spacing);
				float2 right_text_position = { position.x + scale.x - element_descriptor.graph_padding.x - right_span.x, text_y };
				system->ConvertCharactersToTextSprites(
					temp_float_stream.buffer,
					right_text_position,
					text_buffer,
					temp_float_stream.size,
					font_color,
					*text_count,
					font_size,
					character_spacing
				);

				*text_count += temp_float_stream.size * 6;

				SpriteRectangle<configuration>(
					{ AlignMiddle(right_text_position.x, right_span.x, vertical_line_scale.x), position.y + element_descriptor.graph_padding.y },
					vertical_line_scale,
					ECS_TOOLS_UI_TEXTURE_MASK,
					{ 0.0f, 0.0f },
					{ 1.0f, 1.0f },
					font_color
				);

				float remaining_x_scale = scale.x - element_descriptor.graph_padding.x - (left_span.x + right_span.x);
				int64_t max_sprite_count = static_cast<int64_t>((remaining_x_scale) / (left_span.x + element_descriptor.graph_x_axis_space));
				int64_t min_sprite_count = static_cast<int64_t>((remaining_x_scale) / (right_span.x + element_descriptor.graph_x_axis_space));
				
				int64_t min_copy = min_sprite_count;
				min_sprite_count = function::PredicateValue(min_sprite_count > max_sprite_count, max_sprite_count, min_sprite_count);
				max_sprite_count = function::PredicateValue(max_sprite_count < min_copy, min_copy, max_sprite_count);
				int64_t index = min_sprite_count;
				float total_sprite_length = 0.0f;

				for (; index <= max_sprite_count && index > 0; index++) {
					total_sprite_length = 0.0f;

					float step = 1.0f / (index + 1);
					for (size_t subindex = 0; subindex < index; subindex++) {
						float value = function::Lerp(min_x, max_x, 1.0f - (subindex + 1) * step);
						temp_float_stream.size = 0;
						function::ConvertFloatToChars(temp_float_stream, value, x_axis_precision);
						float2 current_span = system->GetTextSpan(temp_float_stream.buffer, temp_float_stream.size, font_size.x, font_size.y, character_spacing);
						total_sprite_length += current_span.x;
					}
					if (total_sprite_length < remaining_x_scale - (index + 1) * element_descriptor.graph_x_axis_space) {
						break;
					}
				}

				if (index > 0) {
					index -= index > max_sprite_count;
					float x_space = (remaining_x_scale - total_sprite_length) / (index + 1);

					float2 sprite_position = { position.x + left_span.x + x_space, text_y };
					float step = 1.0f / (index + 1);
					for (int64_t subindex = 0; subindex < index; subindex++) {
						float value = function::Lerp(min_x, max_x, (subindex + 1) * step);
						temp_float_stream.size = 0;
						function::ConvertFloatToChars(temp_float_stream, value, x_axis_precision);
						system->ConvertCharactersToTextSprites(
							temp_float_stream.buffer,
							sprite_position,
							text_buffer,
							temp_float_stream.size,
							font_color,
							*text_count,
							font_size,
							character_spacing
						);

						auto current_vertices = GetTextStream<configuration>(temp_float_stream.size * 6);
						float2 text_span = GetTextSpan(current_vertices);
						*text_count += 6 * temp_float_stream.size;

						float line_x = AlignMiddle(sprite_position.x, text_span.x, element_descriptor.graph_axis_value_line_size.x);
						SpriteRectangle<configuration>(
							{ line_x, position.y + element_descriptor.graph_padding.y },
							vertical_line_scale,
							ECS_TOOLS_UI_TEXTURE_MASK,
							{ 0.0f, 0.0f },
							{ 1.0f, 1.0f },
							font_color
						);

						sprite_position.x += text_span.x + x_space;
					}
				}			

				if (!disable_horizontal_line) {
					SpriteRectangle<configuration>(
						{ position.x + left_span.x * 0.5f, text_y - element_descriptor.graph_axis_bump.y },
					{
						remaining_x_scale + (left_span.x + right_span.x) * 0.5f,
						element_descriptor.graph_axis_value_line_size.y
					},
						ECS_TOOLS_UI_TEXTURE_MASK,
						{ 1.0f, 0.0f },
						{ 0.0f, 1.0f },
						font_color
					);
				}

				return { left_span.x, right_span.x };
			}

			template<size_t configuration, typename InputStream>
			void GraphDrawer(
				const UIDrawConfig& config,
				const char* name, 
				const InputStream& samples, 
				float2 position,
				float2 scale,
				size_t x_axis_precision,
				size_t y_axis_precision
			) {
				if constexpr (configuration & UI_CONFIG_GRAPH_KEEP_RESOLUTION_X) {
					const UIConfigGraphKeepResolutionX* data = (const UIConfigGraphKeepResolutionX*)config.GetParameter(UI_CONFIG_GRAPH_KEEP_RESOLUTION_X);

					float2 difference;
					difference.x = samples[samples.size - 1].x - samples[0].x;
					difference.y = 0.0f;

					float min_y = 1000000.0f;
					float max_y = -1000000.0f;
					if constexpr (configuration & UI_CONFIG_GRAPH_MIN_Y) {
						const UIConfigGraphMinY* min = (const UIConfigGraphMinY*)config.GetParameter(UI_CONFIG_GRAPH_MIN_Y);
						min_y = min->value;
					}
					if constexpr (configuration & UI_CONFIG_GRAPH_MAX_Y) {
						const UIConfigGraphMaxY* max = (const UIConfigGraphMaxY*)config.GetParameter(UI_CONFIG_GRAPH_MAX_Y);
						max_y = max->value;
					}
					for (size_t index = 0; index < samples.size; index++) {
						if constexpr (~configuration & UI_CONFIG_GRAPH_MIN_Y) {
							min_y = function::PredicateValue(min_y > samples[index].y, samples[index].y, min_y);
						}
						if constexpr (~configuration & UI_CONFIG_GRAPH_MAX_Y) {
							max_y = function::PredicateValue(max_y < samples[index].y, samples[index].y, max_y);
						}
					}

					difference.y = max_y - min_y;
					float ratio = difference.y / difference.x;
					scale.y = scale.x * ratio;
					scale.y = system->NormalizeVerticalToWindowDimensions(scale.y);
					scale.y = function::PredicateValue(scale.y > data->max_y_scale, data->max_y_scale, scale.y);
					scale.y = function::PredicateValue(scale.y < data->min_y_scale, data->min_y_scale, scale.y);
				}
				else if constexpr (configuration & UI_CONFIG_GRAPH_KEEP_RESOLUTION_Y) {
					const UIConfigGraphKeepResolutionY* data = (const UIConfigGraphKeepResolutionY*)config.GetParameter(UI_CONFIG_GRAPH_KEEP_RESOLUTION_Y);

					float2 difference;
					difference.x = samples[samples.size - 1].x - samples[0].x;
					difference.y = 0.0f;

					float min_y = 1000000.0f;
					float max_y = -1000000.0f;
					if constexpr (configuration & UI_CONFIG_GRAPH_MIN_Y) {
						const UIConfigGraphMinY* min = (const UIConfigGraphMinY*)config.GetParameter(UI_CONFIG_GRAPH_MIN_Y);
						min_y = min->value;
					}
					if constexpr (configuration & UI_CONFIG_GRAPH_MAX_Y) {
						const UIConfigGraphMaxY* max = (const UIConfigGraphMaxY*)config.GetParameter(UI_CONFIG_GRAPH_MAX_Y);
						max_y = max->value;
					}
					for (size_t index = 0; index < samples.size; index++) {
						if constexpr (~configuration & UI_CONFIG_GRAPH_MIN_Y) {
							min_y = function::PredicateValue(min_y > samples[index].y, samples[index].y, min_y);
						}
						if constexpr (~configuration & UI_CONFIG_GRAPH_MAX_Y) {
							max_y = function::PredicateValue(max_y < samples[index].y, samples[index].y, max_y);
						}
					}

					difference.y = max_y - min_y;
					float ratio = difference.x / difference.y;
					scale.x = scale.y * ratio;
					scale.x = system->NormalizeHorizontalToWindowDimensions(scale.x);
					scale.x = function::PredicateValue(scale.x > data->max_x_scale, data->max_x_scale, scale.x);
					scale.x = function::PredicateValue(scale.x < data->min_x_scale, data->min_x_scale, scale.x);
				}

				constexpr size_t label_configuration = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT
					| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
				
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
					if (name != nullptr) {
						TextLabel<label_configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(config, name, position, scale);
					}
				}

				HandleFitSpaceRectangle<configuration>(position, scale);

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {				
					float2 initial_scale = scale;
					float2 graph_scale = { scale.x - 2.0f * element_descriptor.graph_padding.x, scale.y - 2.0f * element_descriptor.graph_padding.y };
					float2 graph_position = { position.x + element_descriptor.graph_padding.x, position.y + element_descriptor.graph_padding.y };

					if constexpr (IsElementNameFirst(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
						if (name != nullptr) {
							TextLabel<label_configuration>(config, name, position, scale);
							scale = initial_scale;
						}
					}

					float2 difference;
					difference.x = samples[samples.size - 1].x - samples[0].x;
					difference.y = 0.0f;

					float min_y = 1000000.0f;
					float max_y = -1000000.0f;
					if constexpr (configuration & UI_CONFIG_GRAPH_MIN_Y) {
						const UIConfigGraphMinY* min = (const UIConfigGraphMinY*)config.GetParameter(UI_CONFIG_GRAPH_MIN_Y);
						min_y = min->value;
					}
					if constexpr (configuration & UI_CONFIG_GRAPH_MAX_Y) {
						const UIConfigGraphMaxY* max = (const UIConfigGraphMaxY*)config.GetParameter(UI_CONFIG_GRAPH_MAX_Y);
						max_y = max->value;
					}
					for (size_t index = 0; index < samples.size; index++) {
						if constexpr (~configuration & UI_CONFIG_GRAPH_MIN_Y) {
							min_y = function::PredicateValue(min_y > samples[index].y, samples[index].y, min_y);
						}
						if constexpr (~configuration & UI_CONFIG_GRAPH_MAX_Y) {
							max_y = function::PredicateValue(max_y < samples[index].y, samples[index].y, max_y);
						}
					}


					difference.y = max_y - min_y;

					Color font_color;
					float character_spacing;
					float2 font_size;

					HandleText<configuration>(config, font_color, font_size, character_spacing);
					font_color.alpha = ECS_TOOLS_UI_GRAPH_TEXT_ALPHA;

					if constexpr (configuration & UI_CONFIG_GRAPH_REDUCE_FONT) {
						font_size.x *= element_descriptor.graph_reduce_font;
						font_size.y *= element_descriptor.graph_reduce_font;
					}

					float2 axis_bump = { 0.0f, 0.0f };
					float y_sprite_size = system->GetTextSpriteYScale(font_size.y);
					if constexpr (configuration & UI_CONFIG_GRAPH_X_AXIS) {
						axis_bump.y = y_sprite_size;
					}

					if constexpr (configuration & UI_CONFIG_GRAPH_Y_AXIS) {
						float axis_span = GraphYAxis<configuration>(
							config,
							y_axis_precision,
							min_y,
							max_y,
							position,
							{ scale.x, scale.y - axis_bump.y },
							font_color,
							font_size,
							character_spacing
							);
						axis_bump.x += axis_span;
						graph_scale.x -= axis_span;
						graph_position.x += axis_span;

						char stack_memory[64];
						Stream<char> temp_stream = Stream<char>(stack_memory, 0);
						function::ConvertFloatToChars(temp_stream, samples[0].x, x_axis_precision);

						float2 first_sample_span = system->GetTextSpan(temp_stream.buffer, temp_stream.size, font_size.x, font_size.y, character_spacing);
						axis_bump.x -= first_sample_span.x * 0.5f;
						graph_position.y += y_sprite_size * 0.5f;
						graph_scale.y -= y_sprite_size * 0.5f;
						if constexpr (~configuration & UI_CONFIG_GRAPH_X_AXIS) {
							graph_scale.y -= y_sprite_size * 0.5f;
						}
					}

					if constexpr (configuration & UI_CONFIG_GRAPH_X_AXIS) {
						float2 spans = GraphXAxis<configuration>(
							config,
							x_axis_precision,
							samples[0].x,
							samples[samples.size - 1].x,
							{ position.x + element_descriptor.graph_padding.x + axis_bump.x, position.y },
							{ scale.x - element_descriptor.graph_padding.x - axis_bump.x, scale.y },
							font_color,
							font_size,
							character_spacing,
							axis_bump.x != 0.0f
							);
						graph_scale.y -= y_sprite_size + element_descriptor.graph_axis_bump.y;
						if constexpr (~configuration & UI_CONFIG_GRAPH_Y_AXIS) {
							graph_position.x += spans.x * 0.5f;
							graph_scale.x -= (spans.x + spans.y) * 0.5f;
						}
						else {
							graph_scale.x -= spans.y * 0.5f;
							graph_scale.y -= y_sprite_size * 0.5f - element_descriptor.graph_axis_value_line_size.y * 2.0f;
						}
					}

					int64_t starting_index = samples.size;
					int64_t end_index = 0;
					int64_t index = 0;
					float min_x = samples[0].x;
					float x_space_factor = 1.0f / difference.x * graph_scale.x;
					while ((samples[index].x - min_x) * x_space_factor + graph_position.x < region_position.x && index < samples.size) {
						index++;
					}
					starting_index = function::PredicateValue(index <= 2, (int64_t)0, index - 2);
					while ((samples[index].x - min_x) * x_space_factor + graph_position.x < region_limit.x && index < samples.size) {
						index++;
					}
					end_index = function::PredicateValue(index >= samples.size - 2, (int64_t)samples.size, index + 3);

					SolidColorRectangle<configuration>(config, position, scale);

					auto convert_absolute_position_to_graph_space = [](float2 position, float2 graph_position, float2 graph_scale,
						float2 min, float2 inverse_sample_span) {
							return float2(graph_position.x + graph_scale.x * (position.x - min.x) * inverse_sample_span.x, graph_position.y + graph_scale.y * (1.0f - (position.y - min.y) * inverse_sample_span.y));
					};

					float2 inverse_sample_span = { 1.0f / (samples[samples.size - 1].x - samples[0].x), 1.0f / (max_y - min_y) };
					float2 min_values = { samples[0].x, min_y };
					float2 previous_point_position = convert_absolute_position_to_graph_space(samples[starting_index], graph_position, graph_scale, min_values, inverse_sample_span);

					Color line_color;
					if constexpr (configuration & UI_CONFIG_GRAPH_LINE_COLOR) {
						const UIConfigGraphColor* color_desc = (const UIConfigGraphColor*)config.GetParameter(UI_CONFIG_GRAPH_LINE_COLOR);
						line_color = color_desc->color;
					}
					else {
						line_color = color_theme.graph_line;
					}
					Color initial_color = line_color;

					Stream<UISpriteVertex> drop_color_stream;
					if constexpr (configuration & UI_CONFIG_GRAPH_DROP_COLOR) {
						if (end_index > starting_index + 1) {
							int64_t line_count = end_index - starting_index - 1;
							SetSpriteClusterTexture<configuration>(ECS_TOOLS_UI_TEXTURE_MASK, line_count);
							drop_color_stream = GetSpriteClusterStream<configuration>(line_count * 6);
							auto sprite_count = HandleSpriteClusterCount<configuration>();
							*sprite_count += line_count * 6;
						}
					}
					Stream<UISpriteVertex> sample_circle_stream;
					if constexpr (configuration & UI_CONFIG_GRAPH_SAMPLE_CIRCLES) {
						if (end_index > starting_index) {
							int64_t circle_count = end_index - starting_index;
							SetSpriteClusterTexture<configuration>(ECS_TOOLS_UI_TEXTURE_FILLED_CIRCLE, circle_count);
							sample_circle_stream = GetSpriteClusterStream<configuration>(circle_count * 6);
							auto sprite_count = HandleSpriteClusterCount<configuration>();
							*sprite_count += circle_count * 6;
						}
					}

					for (int64_t index = starting_index; index < end_index - 1; index++) {
						float2 next_point = convert_absolute_position_to_graph_space(samples[index + 1], graph_position, graph_scale, min_values, inverse_sample_span);
						
						float2 hoverable_position = { previous_point_position.x, graph_position.y };
						float2 hoverable_scale = { next_point.x - previous_point_position.x, graph_scale.y };
						if (IsPointInRectangle(mouse_position, hoverable_position, hoverable_scale)) {
							line_color = color_theme.graph_hover_line;
						}
						else {
							line_color = initial_color;
						}
						Line<configuration>(previous_point_position, next_point, line_color);

						if (next_point.x > 10.0f) {
							__debugbreak();
						}
						UIDrawerGraphHoverableData hoverable_data;
						hoverable_data.first_sample_values = samples[index];
						hoverable_data.second_sample_values = samples[index + 1];
						hoverable_data.sample_index = index;

						AddHoverable(hoverable_position, hoverable_scale, { GraphHoverable, &hoverable_data, sizeof(hoverable_data), UIDrawPhase::System });
						if constexpr (configuration & UI_CONFIG_GRAPH_DROP_COLOR) {
							line_color.alpha = ECS_TOOLS_UI_GRAPH_DROP_COLOR_ALPHA;
							size_t base_index = (index - starting_index) * 6;
							drop_color_stream[base_index].SetTransform(previous_point_position);
							drop_color_stream[base_index + 1].SetTransform(next_point);
							drop_color_stream[base_index + 2].SetTransform({ next_point.x, graph_position.y + graph_scale.y });
							drop_color_stream[base_index + 3].SetTransform(previous_point_position);
							drop_color_stream[base_index + 4].SetTransform({ next_point.x, graph_position.y + graph_scale.y });
							drop_color_stream[base_index + 5].SetTransform({ previous_point_position.x, graph_position.y + graph_scale.y });
							SetUVForRectangle({ 0.0f, 0.0f }, { 1.0f, 1.0f }, base_index, drop_color_stream.buffer);
							SetColorForRectangle(line_color, base_index, drop_color_stream.buffer);
						}
						if constexpr (configuration & UI_CONFIG_GRAPH_SAMPLE_CIRCLES) {
							float2 sprite_scale = GetSquareScale(element_descriptor.graph_sample_circle_size);
							size_t temp_index = (index - starting_index) * 6;
							SetSpriteRectangle({ previous_point_position.x - sprite_scale.x * 0.5f, previous_point_position.y - sprite_scale.y * 0.5f }, sprite_scale, color_theme.graph_sample_circle, { 0.0f, 0.0f }, { 1.0f, 1.0f }, sample_circle_stream.buffer, temp_index);
						}
						
						previous_point_position = next_point;
					}

					if constexpr (configuration & UI_CONFIG_GRAPH_SAMPLE_CIRCLES) {
						if (end_index > starting_index + 1) {
							float2 sprite_scale = GetSquareScale(element_descriptor.graph_sample_circle_size);
							size_t temp_index = (end_index - 1 - starting_index) * 6;
							SetSpriteRectangle({ previous_point_position.x - sprite_scale.x * 0.5f, previous_point_position.y - sprite_scale.y * 0.5f }, sprite_scale, color_theme.graph_sample_circle, { 0.0f, 0.0f }, { 1.0f, 1.0f }, sample_circle_stream.buffer, temp_index);
						}
					}

					if constexpr (configuration & UI_CONFIG_GRAPH_TAGS) {
						const UIConfigGraphTags* tags = (const UIConfigGraphTags*)config.GetParameter(UI_CONFIG_GRAPH_TAGS);

						ActionData action_data;
						UIDrawerGraphTagData tag_data;
						tag_data.drawer = this;
						tag_data.graph_position = graph_position;
						tag_data.graph_position += region_render_offset;
						tag_data.graph_scale = graph_scale;
						action_data.data = &tag_data;
						action_data.system = system;
						for (size_t index = 0; index < tags->vertical_tag_count; index++) {
							float2 position = convert_absolute_position_to_graph_space({ tags->vertical_values[index], min_y }, graph_position, graph_scale, min_values, inverse_sample_span);
							tag_data.position = position;
							tag_data.position += region_render_offset;
							tags->vertical_tags[index](&action_data);
						}
						for (size_t index = 0; index < tags->horizontal_tag_count; index++) {
							float2 position = convert_absolute_position_to_graph_space({ min_x, tags->horizontal_values[index] }, graph_position, graph_scale, min_values, inverse_sample_span);
							tag_data.position = position;
							tag_data.position += region_render_offset;
							tags->horizontal_tags[index](&action_data);
						}
					}

				}

				FinalizeRectangle<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(position, scale);

				if constexpr (IsElementNameAfter(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
					if (name != nullptr) {
						position.x += scale.x + layout.element_indentation;
						TextLabel<label_configuration>(config, name, position, scale);
					}
				}
			}

			// it returns the index of the sprite with the smallest x
			template<bool left = true>
			unsigned int LevelVerticalTextX(Stream<UISpriteVertex>& vertices) const {
				if constexpr (left) {
					float min_x = vertices[0].position.x;
					unsigned int smallest_index = 0;

					for (size_t index = 6; index < vertices.size; index += 6) {
						if (min_x > vertices[index].position.x) {
							min_x = vertices[index].position.x;
							smallest_index = index;
						}
					}

					for (size_t index = 0; index < vertices.size; index += 6) {
						if (min_x != vertices[index].position.x) {
							float translation = vertices[index].position.x - min_x;
							vertices[index].position.x -= translation;
							vertices[index + 1].position.x -= translation;
							vertices[index + 2].position.x -= translation;
							vertices[index + 3].position.x -= translation;
							vertices[index + 4].position.x -= translation;
							vertices[index + 5].position.x -= translation;
						}
					}
					return smallest_index;
				}
				else {
					float max_x = vertices[1].position.x;
					unsigned int biggest_index = 1;

					for (size_t index = 7; index < vertices.size; index += 6) {
						if (max_x < vertices[index].position.x) {
							max_x = vertices[index].position.x;
							biggest_index = index;
						}
					}

					for (size_t index = 1; index < vertices.size; index += 6) {
						if (max_x != vertices[index].position.x) {
							float translation = max_x - vertices[index].position.x;
							vertices[index - 1].position.x += translation;
							vertices[index].position.x += translation;
							vertices[index + 1].position.x += translation;
							vertices[index + 2].position.x += translation;
							vertices[index + 3].position.x += translation;
							vertices[index + 4].position.x += translation;
						}
					}
					return biggest_index;
				}
			}

			template<size_t configuration>
			float2 TextLabelWithSize(const UIDrawConfig& config, const char* text, float2 position) {
				float2 scale;
				size_t text_count = strlen(text);
				
				Stream<UISpriteVertex> current_text = GetTextStream<configuration>(text_count * 6);
				Text<configuration>(config, text, { position.x + font.label_horizontal_padd, position.y + font.label_vertical_padd }, scale);
				scale.x += 2 * font.label_horizontal_padd;
				scale.y += 2 * font.label_vertical_padd;
				
				if constexpr (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
					position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
				}

				TextAlignment horizontal_alignment = TextAlignment::Middle, vertical_alignment = TextAlignment::Top;
				float x_text_position, y_text_position;
				HandleTextLabelAlignment<configuration>(
					config,
					GetTextSpan<~configuration & UI_CONFIG_VERTICAL>(current_text),
					scale,
					position,
					x_text_position,
					y_text_position,
					horizontal_alignment,
					vertical_alignment
				);

				if (horizontal_alignment != TextAlignment::Left) {
					float x_translation = x_text_position - position.x - font.label_horizontal_padd;
					for (size_t index = 0; index < current_text.size; index++) {
						current_text[index].position.x += x_translation;
					}
				}
				if (vertical_alignment != TextAlignment::Top) {
					float y_translation = y_text_position - position.y - font.label_vertical_padd;
					for (size_t index = 0; index < current_text.size; index++) {
						current_text[index].position.y += y_translation;
					}
				}
				return scale;
			}

			template<size_t configuration>
			void FixedScaleTextLabel(const UIDrawConfig& config, const char* text, float2 position, float2 scale) {
				if constexpr (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
					position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
				}
				Stream<UISpriteVertex> current_text = GetTextStream<configuration>(ParseStringIdentifier(text, strlen(text)) * 6);
				float2 text_span;

				float2 temp_position = { position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd };
				Text<configuration | UI_CONFIG_DO_NOT_FIT_SPACE>(config, text, temp_position, text_span);
				
				TextAlignment horizontal_alignment = TextAlignment::Middle, vertical_alignment = TextAlignment::Top;
				GetTextLabelAlignment<configuration>(config, horizontal_alignment, vertical_alignment);
				bool translate = true;

				auto text_sprite_count = HandleTextSpriteCount<configuration>();
				if constexpr (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					const WindowSizeTransformType* type = (const WindowSizeTransformType*)config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
					if (*type == WindowSizeTransformType::Both) {
						if constexpr (configuration & UI_CONFIG_VERTICAL) {
							if (text_span.x > scale.x - 2 * element_descriptor.label_horizontal_padd) {
								translate = false;
								*text_sprite_count -= current_text.size;
							}
							else {
								if (text_span.y > scale.y - 2 * element_descriptor.label_vertical_padd) {
									size_t valid_sprites;
									if (vertical_alignment == TextAlignment::Bottom) {
										valid_sprites = CullTextSprites<3>(current_text, -position.y - scale.y + element_descriptor.label_vertical_padd);
										*text_sprite_count -= current_text.size - valid_sprites;
										current_text.size = valid_sprites;
										text_span = GetTextSpan<false, true>(current_text);
									}
									else {
										valid_sprites = CullTextSprites<2>(current_text, -position.y - scale.y + element_descriptor.label_vertical_padd);
										*text_sprite_count -= current_text.size - valid_sprites;
										current_text.size = valid_sprites;
										text_span = GetTextSpan<false>(current_text);
									}
								}
							}
						}
						else {
							if (text_span.y > scale.y - 2 * element_descriptor.label_vertical_padd) {
								translate = false;
								*text_sprite_count -= current_text.size;
							}
							else {
								if (text_span.x > scale.x - 2 * element_descriptor.label_horizontal_padd) {
									size_t valid_sprites;
									if (horizontal_alignment == TextAlignment::Right) {
										valid_sprites = CullTextSprites<1>(current_text, position.x + element_descriptor.label_horizontal_padd);
										*text_sprite_count -= current_text.size - valid_sprites;
										current_text.size = valid_sprites;
										text_span = GetTextSpan<true, true>(current_text);
									}
									else {
										valid_sprites = CullTextSprites<0>(current_text, position.x + scale.x - element_descriptor.label_horizontal_padd);
										*text_sprite_count -= current_text.size - valid_sprites;
										current_text.size = valid_sprites;
										text_span = GetTextSpan<true>(current_text);
									}
									
								}
							}
						}
					}
					else {
						if constexpr (configuration & UI_CONFIG_VERTICAL) {
							if (text_span.y > scale.y - 2 * element_descriptor.label_vertical_padd) {
								size_t valid_sprites;
								if (vertical_alignment == TextAlignment::Bottom) {
									valid_sprites = CullTextSprites<3>(current_text, -position.y - scale.y + element_descriptor.label_vertical_padd);
									*text_sprite_count -= current_text.size - valid_sprites;
									current_text.size = valid_sprites;
									text_span = GetTextSpan<false, true>(current_text);
								}
								else {
									valid_sprites = CullTextSprites<2>(current_text, -position.y - scale.y + element_descriptor.label_vertical_padd);
									*text_sprite_count -= current_text.size - valid_sprites;
									current_text.size = valid_sprites;
									text_span = GetTextSpan<false>(current_text);
								}
							}
						}
						else {
							if (text_span.x > scale.x - 2 * element_descriptor.label_horizontal_padd) {
								size_t valid_sprites;
								if (horizontal_alignment == TextAlignment::Right) {
									valid_sprites = CullTextSprites<1>(current_text, position.x + element_descriptor.label_horizontal_padd);
									*text_sprite_count -= current_text.size - valid_sprites;
									current_text.size = valid_sprites;
									text_span = GetTextSpan<true, true>(current_text);
								}
								else {
									valid_sprites = CullTextSprites<0>(current_text, position.x + scale.x - element_descriptor.label_horizontal_padd);
									*text_sprite_count -= current_text.size - valid_sprites;
									current_text.size = valid_sprites;
									text_span = GetTextSpan<true>(current_text);
								}

							}
						}
					}
				}
				if (translate) {
					float x_text_position, y_text_position;
					HandleTextLabelAlignment<configuration>(
						config,
						text_span,
						scale,
						position,
						x_text_position,
						y_text_position,
						horizontal_alignment,
						vertical_alignment
					);

					if (horizontal_alignment != TextAlignment::Left || vertical_alignment != TextAlignment::Top) {
						float x_translation = x_text_position - position.x - element_descriptor.label_horizontal_padd;
						float y_translation = y_text_position - position.y - element_descriptor.label_vertical_padd;
						for (size_t index = 0; index < current_text.size; index++) {
							current_text[index].position.x += x_translation;
							current_text[index].position.y -= y_translation;
						}
					}
				}
				if constexpr (configuration & UI_CONFIG_VERTICAL) {
					AlignVerticalText(current_text);
				}

				if constexpr (~configuration & UI_CONFIG_LABEL_TRANSPARENT) {
					Color label_color = HandleColor<configuration>(config);
					SolidColorRectangle<configuration>(
						position,
						scale,
						label_color
					);
				}

				HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
			}

			// this will add automatically VerticalSlider and HorizontalSlider to name
			void ViewportRegionSliderInitializer(
				float2* region_offset, 
				const char* name, 
				void** horizontal_slider, 
				void** vertical_slider
			) {
				char stack_memory[128];

				UIDrawConfig null_config;
				size_t name_size = strlen(name);
				memcpy(stack_memory, name, name_size);
				stack_memory[name_size] = '\0';
				strcat(stack_memory, "VerticalSlider");
				stack_memory[name_size + 14] = '\0';

				// bounds, position and scale here don't matter
				*vertical_slider = FloatSliderInitializer<UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_SLIDER_NO_TEXT | UI_CONFIG_VERTICAL | UI_CONFIG_DO_NOT_ADVANCE>(null_config, stack_memory, { 0.0f, 0.0f }, { 0.0f, 0.0f }, &region_offset->y, 0.0f, 10.0f);

				stack_memory[name_size] = '\0';
				strcat(stack_memory, "HorizontalSlider");
				stack_memory[name_size + 16] = '\0';
				// bounds, position and scale here don't matter
				*horizontal_slider = FloatSliderInitializer<UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_SLIDER_NO_TEXT | UI_CONFIG_DO_NOT_ADVANCE>(null_config, stack_memory, { 0.0f, 0.0f }, { 0.0f, 0.0f }, &region_offset->x, 0.0f, 10.0f);
			}

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
			) {
				bool is_horizontal_extended = render_span.x - system->m_descriptors.misc.render_slider_vertical_size > render_zone.x;
				bool is_vertical_extended = render_span.y - system->m_descriptors.misc.render_slider_horizontal_size > render_zone.y;
				bool is_horizontal = render_span.x > render_zone.x;
				bool is_vertical = render_span.y > render_zone.y;

				UIConfigAbsoluteTransform horizontal_transform;
				if ((!is_vertical_extended && is_horizontal_extended && !is_vertical) || (is_horizontal && is_vertical)) {
					float difference = render_span.x - render_zone.x;
					if (!is_vertical_extended && is_horizontal_extended && !is_vertical) {
						difference -= system->m_descriptors.misc.render_slider_vertical_size;
					}

					constexpr size_t slider_configuration =
						UI_CONFIG_SLIDER_NO_NAME
						| UI_CONFIG_SLIDER_NO_TEXT
						| UI_CONFIG_DO_NOT_ADVANCE
						| UI_CONFIG_COLOR
						| UI_CONFIG_SLIDER_COLOR
						| UI_CONFIG_LATE_DRAW
						| UI_CONFIG_SLIDER_SHRINK
						| UI_CONFIG_SLIDER_LENGTH
						| UI_CONFIG_SLIDER_PADDING
						| UI_CONFIG_DO_NOT_VALIDATE_POSITION
						| UI_CONFIG_DO_NOT_FIT_SPACE;

					UIDrawConfig config;
					UIConfigColor color;
					UIConfigSliderColor slider_color;
					UIConfigSliderShrink slider_shrink;
					UIConfigSliderLength slider_length;
					UIConfigSliderPadding slider_padding;
					color.color = color_theme.render_sliders_background;
					slider_color.color = color_theme.render_sliders_active_part;
					slider_shrink.value.x = element_descriptor.slider_shrink.x * zoom_inverse.x;
					slider_shrink.value.y = element_descriptor.slider_shrink.y * zoom_inverse.y;
					slider_length.value = element_descriptor.slider_length.x * zoom_inverse.x;
					slider_padding.value = element_descriptor.slider_padding.x * zoom_inverse.x;

					horizontal_transform.position = horizontal_slider_position;

					horizontal_transform.scale = horizontal_slider_scale;

					config.AddFlags(horizontal_transform, color, slider_color, slider_shrink, slider_length);
					config.AddFlag(slider_padding);

					if (system->m_windows[window_index].pin_horizontal_slider_count > 0) {
						UIDrawerSlider* slider = (UIDrawerSlider*)horizontal_slider;
						slider->slider_position = region_offset->x / difference;
						system->m_windows[window_index].pin_horizontal_slider_count--;
					}

					FloatSliderDrawer<slider_configuration>(
						config,
						(UIDrawerSlider*)horizontal_slider,
						horizontal_transform.position,
						horizontal_transform.scale,
						&region_offset->x,
						0.0f,
						difference
					);
					system->m_windows[window_index].is_horizontal_render_slider = true;
				}
				else {
					UIDrawerSlider* slider = (UIDrawerSlider*)horizontal_slider;
					slider->slider_position = 0.0f;
					system->m_windows[window_index].render_region.x = 0.0f;
					system->m_windows[window_index].is_horizontal_render_slider = false;
				}

				UIConfigAbsoluteTransform vertical_transform;
				if ((!is_horizontal_extended && is_vertical_extended && !is_horizontal) || (is_vertical && is_horizontal)) {
					float difference = render_span.y - render_zone.y;
					if (!is_horizontal_extended && is_vertical_extended && !is_horizontal) {
						difference -= system->m_descriptors.misc.render_slider_horizontal_size;
					}

					constexpr size_t slider_configuration =
						UI_CONFIG_SLIDER_NO_NAME
						| UI_CONFIG_SLIDER_NO_TEXT
						| UI_CONFIG_VERTICAL
						| UI_CONFIG_COLOR
						| UI_CONFIG_SLIDER_COLOR
						| UI_CONFIG_LATE_DRAW
						| UI_CONFIG_SLIDER_SHRINK
						| UI_CONFIG_SLIDER_LENGTH
						| UI_CONFIG_SLIDER_PADDING
						| UI_CONFIG_DO_NOT_VALIDATE_POSITION
						| UI_CONFIG_DO_NOT_FIT_SPACE;

					UIDrawConfig config;
					UIConfigColor color;
					UIConfigSliderColor slider_color;
					UIConfigSliderShrink slider_shrink;
					UIConfigSliderLength slider_length;
					UIConfigSliderPadding slider_padding;
					color.color = color_theme.render_sliders_background;
					slider_color.color = color_theme.render_sliders_active_part;
					slider_shrink.value.x = element_descriptor.slider_shrink.x * zoom_inverse.x;
					slider_shrink.value.y = element_descriptor.slider_shrink.y * zoom_inverse.y;
					slider_length.value = element_descriptor.slider_length.y * zoom_inverse.y;
					slider_padding.value = element_descriptor.slider_padding.y * zoom_inverse.y;

					vertical_transform.position = vertical_slider_position;
					vertical_transform.scale = vertical_slider_scale;

					config.AddFlags(vertical_transform, color, slider_color, slider_shrink, slider_length);
					config.AddFlag(slider_padding);

					if (system->m_windows[window_index].pin_vertical_slider_count > 0) {
						UIDrawerSlider* slider = (UIDrawerSlider*)vertical_slider;
						slider->slider_position = region_offset->y / difference;
						system->m_windows[window_index].pin_vertical_slider_count--;
					}

					FloatSliderDrawer<slider_configuration>(
						config,
						(UIDrawerSlider*)vertical_slider,
						vertical_transform.position,
						vertical_transform.scale,
						&region_offset->y,
						0.0f,
						difference
						);
					system->m_windows[window_index].is_vertical_render_slider = true;
				}
				else {
					UIDrawerSlider* slider = (UIDrawerSlider*)vertical_slider;
					slider->slider_position = 0.0f;
					system->m_windows[window_index].render_region.y = 0.0f;
					system->m_windows[window_index].is_vertical_render_slider = false;
				}
			}

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
			) {
				void* horizontal_slider = GetResource(horizontal_slider_name);
				void* vertical_slider = GetResource(vertical_slider_name);

				ViewportRegionSliders(
					render_span,
					render_zone,
					region_offset,
					horizontal_slider_position,
					horizontal_slider_scale,
					vertical_slider_position,
					vertical_slider_scale,
					horizontal_slider,
					vertical_slider
				);
			}

			template<size_t configuration>
			void SpriteRectangle(
				float2 position,
				float2 scale,
				LPCWSTR texture,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			) {
				if constexpr (!initializer) {
					if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
						system->SetSprite(
							dockspace,
							border_index,
							texture,
							position,
							scale,
							buffers + ECS_TOOLS_UI_MATERIALS,
							counts + ECS_TOOLS_UI_MATERIALS,
							top_left_uv,
							bottom_right_uv,
							UIDrawPhase::Late,
							color
						);
					}
					else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
						system->SetSprite(
							dockspace,
							border_index,
							texture,
							position,
							scale,
							system_buffers,
							system_counts,
							top_left_uv,
							bottom_right_uv,
							UIDrawPhase::System,
							color
						);
					}
					else {
						system->SetSprite(
							dockspace,
							border_index,
							texture,
							position,
							scale,
							buffers,
							counts,
							top_left_uv,
							bottom_right_uv,
							UIDrawPhase::Normal,
							color
						);
					}
				}
			}

			template<size_t configuration>
			void VertexColorRectangle(
				float2 position,
				float2 scale,
				Color top_left,
				Color top_right,
				Color bottom_left,
				Color bottom_right
			) {
				if constexpr (!initializer) {
					auto buffer = HandleSolidColorBuffer<configuration>();
					auto count = HandleSolidColorCount<configuration>();

					SetVertexColorRectangle(position, scale, top_left, top_right, bottom_left, bottom_right, buffer, count);
				}
			}

			template<size_t configuration>
			void VertexColorRectangle(
				float2 position,
				float2 scale,
				ColorFloat top_left,
				ColorFloat top_right,
				ColorFloat bottom_left,
				ColorFloat bottom_right
			) {
				if constexpr (!initializer) {
					auto buffer = HandleSolidColorBuffer<configuration>();
					auto count = HandleSolidColorCount<configuration>();

					SetVertexColorRectangle(position, scale, Color(top_left), Color(top_right), Color(bottom_left), Color(bottom_right), buffer, count);
				}
			}

			template<size_t configuration>
			void VertexColorRectangle(
				float2 position,
				float2 scale,
				const Color* colors
			) {
				if constexpr (!initializer) {
					auto buffer = HandleSolidColorBuffer<configuration>();
					auto count = HandleSolidColorCount<configuration>();

					SetVertexColorRectangle(position, scale, colors, buffer, count);
				}
			}

			template<size_t configuration>
			void VertexColorRectangle(
				float2 position,
				float2 scale,
				const ColorFloat* colors
			) {
				if constexpr (!initializer) {
					auto buffer = HandleSolidColorBuffer<configuration>();
					auto count = HandleSolidColorCount<configuration>();

					SetVertexColorRectangle(position, scale, Color(colors[0]), Color(colors[1]), Color(colors[2]), Color(colors[3]), buffer, count);
				}
			}

			template<size_t configuration>
			void VertexColorSpriteRectangle(
				float2 position,
				float2 scale,
				LPCWSTR texture,
				const Color* colors,
				const float2* uvs,
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				auto buffer = HandleSpriteBuffer<configuration>();
				auto count = HandleSpriteCount<configuration>();

				system->SetVertexColorSprite(dockspace, border_index, texture, position, scale, buffers, counts, colors, uvs, phase);
			}

			template<size_t configuration>
			void VertexColorSpriteRectangle(
				float2 position,
				float2 scale,
				LPCWSTR texture,
				const Color* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				auto buffer = HandleSpriteBuffer<configuration>();
				auto count = HandleSpriteCount<configuration>();

				system->SetVertexColorSprite(dockspace, border_index, texture, position, scale, buffers, counts, colors, top_left_uv, bottom_right_uv, phase);
			}

			template<size_t configuration>
			void VertexColorSpriteRectangle(
				float2 position,
				float2 scale,
				LPCWSTR texture,
				const ColorFloat* colors,
				const float2* uvs,
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				auto buffer = HandleSpriteBuffer<configuration>();
				auto count = HandleSpriteCount<configuration>();

				system->SetVertexColorSprite(dockspace, border_index, texture, position, scale, buffers, counts, colors, uvs, phase);
			}

			template<size_t configuration>
			void VertexColorSpriteRectangle(
				float2 position,
				float2 scale,
				LPCWSTR texture,
				const ColorFloat* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				auto buffer = HandleSpriteBuffer<configuration>();
				auto count = HandleSpriteCount<configuration>();

				system->SetVertexColorSprite(dockspace, border_index, texture, position, scale, buffers, counts, colors, top_left_uv, bottom_right_uv, phase);
			}

			template<size_t configuration>
			Stream<UIVertexColor> GetSolidColorStream(size_t size) {
				return Stream<UIVertexColor>(HandleSolidColorBuffer<configuration>() + *HandleSolidColorCount<configuration>(), size);
			}

			template<size_t configuration>
			Stream<UISpriteVertex> GetTextStream(size_t size) {
				return Stream<UISpriteVertex>(HandleTextSpriteBuffer<configuration>() + *HandleTextSpriteCount<configuration>(), size);
			}

			template<size_t configuration>
			Stream<UISpriteVertex> GetSpriteStream(size_t size) {
				return Stream<UISpriteVertex>(HandleSpriteBuffer<configuration>() + *HandleSpriteCount<configuration>(), size);
			}

			template<size_t configuration>
			Stream<UISpriteVertex> GetSpriteClusterStream(size_t size) {
				return Stream<UISpriteVertex>(HandleSpriteClusterBuffer<configuration>() + *HandleSpriteClusterCount<configuration>(), size);
			}

			float2 TextSpan(Stream<char> characters, float2 font_size, float character_spacing) {
				return system->GetTextSpan(characters.buffer, characters.size, font_size.x, font_size.y, character_spacing);
			}

			float2 TextSpan(const char* characters, float2 font_size, float character_spacing) {
				return TextSpan(Stream<char>(characters, strlen(characters)), font_size, character_spacing);
			}

			float2 TextSpan(Stream<char> characters) {
				return TextSpan(characters, GetFontSize(), font.character_spacing);
			}

			float2 TextSpan(const char* characters) {
				return TextSpan(characters, GetFontSize(), font.character_spacing);
			}

			float2 GetLabelScale(Stream<char> characters) {
				return HandleLabelSize(TextSpan(characters));
			}

			float2 GetLabelScale(const char* characters) {
				return HandleLabelSize(TextSpan(characters));
			}

			float2 GetLabelScale(Stream<char> characters, float2 font_size, float character_spacing) {
				return HandleLabelSize(TextSpan(characters, font_size, character_spacing));
			}

			float2 GetLabelScale(const char* characters, float2 font_size, float character_spacing) {
				return HandleLabelSize(TextSpan(characters, font_size, character_spacing));
			}

			template<size_t configuration>
			void GetTextLabelAlignment(const UIDrawConfig& config, TextAlignment& horizontal, TextAlignment& vertical) {
				if constexpr (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
					const float* params = config.GetParameter(UI_CONFIG_TEXT_ALIGNMENT);
					const TextAlignment* alignments = (TextAlignment*)params;
					horizontal = *alignments;
					vertical = *(alignments + 1);
				}
				else {
					horizontal = TextAlignment::Middle;
					vertical = TextAlignment::Middle;
				}
			}

			float GetXScaleUntilBorder(float position) const {
				return region_scale.x - layout.next_row_padding - position + region_position.x /*- region_render_offset.x*/;
			}

			float GetNextRowXPosition() const {
				return region_position.x + layout.next_row_padding + next_row_offset /*- region_render_offset.x*/;
			}

			size_t GetRandomIntIdentifier(size_t index) {
				return (index * index + (index & 15)) << (index & 3);
			}

			void UpdateCurrentColumnScale(float value) {
				current_column_x_scale = function::PredicateValue(value > current_column_x_scale, value, current_column_x_scale);
			}

			void UpdateCurrentRowScale(float value) {
				current_row_y_scale = function::PredicateValue(value > current_row_y_scale, value, current_row_y_scale);
			}

			void UpdateCurrentScale(float2 position, float2 value) {
				// currently only ColumnDraw and ColumnDrawFitSpace require z component
				draw_mode_extra_float.z = value.y;
				if (draw_mode != UIDrawerMode::ColumnDraw && draw_mode != UIDrawerMode::ColumnDrawFitSpace){
					UpdateCurrentRowScale(position.y - current_y + value.y + region_render_offset.y);
				}
				UpdateCurrentColumnScale(position.x - current_x + value.x + region_render_offset.x);
			}

			void UpdateMaxRenderBoundsRectangle(float2 limits) {
				max_render_bounds.x = function::PredicateValue(max_render_bounds.x < limits.x, limits.x, max_render_bounds.x);
				max_render_bounds.y = function::PredicateValue(max_render_bounds.y < limits.y, limits.y, max_render_bounds.y);
			}

			void UpdateMinRenderBoundsRectangle(float2 position) {
				min_render_bounds.x = function::PredicateValue(min_render_bounds.x > position.x, position.x, min_render_bounds.x);
				min_render_bounds.y = function::PredicateValue(min_render_bounds.y > position.y, position.y, min_render_bounds.y);
			}

			void UpdateRenderBoundsRectangle(float2 position, float2 scale) {
				float2 limits = { position.x + scale.x, position.y + scale.y };
				UpdateMaxRenderBoundsRectangle(limits);
				UpdateMinRenderBoundsRectangle(position);
			}

			bool VerifyFitSpaceNonRectangle(size_t vertex_count) const {
				UIVertexColor* vertices = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
				size_t last_index = counts[ECS_TOOLS_UI_SOLID_COLOR] + vertex_count;
				for (size_t index = counts[ECS_TOOLS_UI_SOLID_COLOR]; index < last_index; index++) {
					if (vertices[index].position.x > region_limit.x - region_render_offset.x) {
						return false;
					}
				}
				return true;
			}

			bool VerifyFitSpaceRectangle(float2 position, float2 scale) const {
				return position.x + scale.x > region_limit.x - region_render_offset.x;
			}

			template<size_t configuration>
			void HandleDrawMode() {
				switch (draw_mode) {
				case UIDrawerMode::Indent:
				case UIDrawerMode::FitSpace:
					Indent();
					break;
				case UIDrawerMode::NextRow:
					if constexpr (~configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW) {
						NextRow();
					}
					else {
						Indent();
					}
					break;
				case UIDrawerMode::NextRowCount:
					if (draw_mode_count == draw_mode_target) {
						NextRow();
						draw_mode_count = 0;
					}
					else {
						if constexpr (~configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW) {
							draw_mode_count++;
						}
					}
					break;
				case UIDrawerMode::Nothing:
					current_x += current_column_x_scale;
					current_column_x_scale = 0.0f;
					break;
				case UIDrawerMode::ColumnDraw:
				case UIDrawerMode::ColumnDrawFitSpace:
					if (draw_mode_target - 1 == draw_mode_count) {
						draw_mode_count = 0;
						current_x += current_column_x_scale + layout.element_indentation;
						current_y = draw_mode_extra_float.y;
						current_row_y_scale += draw_mode_extra_float.z + draw_mode_extra_float.x;
					}
					else {
						current_row_y_scale = function::PredicateValue(draw_mode_count == 0, -draw_mode_extra_float.x, current_row_y_scale);
						current_y += draw_mode_extra_float.z + draw_mode_extra_float.x;
						draw_mode_count++;
						current_row_y_scale += draw_mode_extra_float.z + draw_mode_extra_float.x;
					}
					break;
				}
			}

			template<size_t configuration>
			void ECS_NOINLINE FinalizeRectangle(float2 position, float2 scale) {
				if constexpr (~configuration & UI_CONFIG_DO_NOT_ADVANCE) {
					UpdateRenderBoundsRectangle(position, scale);
					UpdateCurrentScale(position, scale);
					HandleDrawMode<configuration>();
				}
			}

			template<size_t configuration>
			void /*__declspec(noinline)*/ ElementName(const UIDrawConfig& config, UIDrawerTextElement* text, float2 position, float2 scale) {
				constexpr size_t label_configuration = UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_DO_NOT_FIT_SPACE;
				UIDrawConfig label_config;
				UIConfigTextAlignment alignment;

				alignment.horizontal = TextAlignment::Left;
				alignment.vertical = TextAlignment::Middle;
				label_config.AddFlag(alignment);

				if constexpr (configuration & UI_CONFIG_TEXT_PARAMETERS) {
					const UIConfigTextParameters* parameters = (const UIConfigTextParameters*)config.GetParameter(UI_CONFIG_TEXT_PARAMETERS);
					label_config.AddFlag(*parameters);
				}

				if constexpr (~configuration & UI_CONFIG_ELEMENT_NAME_FIRST) {
					position = ECS_TOOLS_UI_DRAWER_CURRENT_POSITION;
					position.x -= region_render_offset.x;
					position.y -= region_render_offset.y;
					// next row is here because it will always be called with UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
					if (draw_mode == UIDrawerMode::Indent || draw_mode == UIDrawerMode::FitSpace || draw_mode == UIDrawerMode::NextRow) {
						position.x -= layout.element_indentation;
					}
				}

				bool is_active = true;
				if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				if constexpr (~configuration & UI_CONFIG_TEXT_PARAMETERS) {
					if constexpr (configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW) {
						if (is_active) {
							TextLabel<label_configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(label_config, text, position, scale);
						}
						else {
							TextLabel<label_configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW | UI_CONFIG_UNAVAILABLE_TEXT>(label_config, text, position, scale);
						}
					}
					else {
						if (is_active) {
							TextLabel<label_configuration>(label_config, text, position, scale);
						}
						else {
							TextLabel<label_configuration | UI_CONFIG_UNAVAILABLE_TEXT>(label_config, text, position, scale);
						}
					}
				}
				else {
					if constexpr (configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW) {
						if (is_active) {
							TextLabel<label_configuration | UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(label_config, text, position, scale);
						}
						else {
							TextLabel<label_configuration | UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW | UI_CONFIG_UNAVAILABLE_TEXT>(label_config, text, position, scale);
						}
					}
					else {
						if (is_active) {
							TextLabel<label_configuration | UI_CONFIG_TEXT_PARAMETERS>(label_config, text, position, scale);
						}
						else {
							TextLabel<label_configuration | UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_UNAVAILABLE_TEXT>(label_config, text, position, scale);
						}
					}
				}
			}

			template<size_t configuration>
			void ElementName(const UIDrawConfig& config, const char* text, float2 position, float2 scale) {
				constexpr size_t label_configuration = UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_CACHE;
				UIDrawConfig label_config;
				UIConfigTextAlignment alignment;

				alignment.horizontal = TextAlignment::Left;
				alignment.vertical = TextAlignment::Middle;
				label_config.AddFlag(alignment);

				if constexpr (configuration & UI_CONFIG_TEXT_PARAMETERS) {
					const UIConfigTextParameters* parameters = (const UIConfigTextParameters*)config.GetParameter(UI_CONFIG_TEXT_PARAMETERS);
					label_config.AddFlag(*parameters);
				}

				if constexpr (~configuration & UI_CONFIG_ELEMENT_NAME_FIRST) {
					position = ECS_TOOLS_UI_DRAWER_CURRENT_POSITION;
					position.x -= region_render_offset.x;
					position.y -= region_render_offset.y;
					// next row is here because it will always be called with UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
					if (draw_mode == UIDrawerMode::Indent || draw_mode == UIDrawerMode::FitSpace || draw_mode == UIDrawerMode::NextRow) {
						position.x -= layout.element_indentation;
					}
				}

				bool is_active = true;
				if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				if constexpr (~configuration & UI_CONFIG_TEXT_PARAMETERS) {
					if constexpr (configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW) {
						if (is_active) {
							TextLabel<label_configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(label_config, text, position, scale);
						}
						else {
							TextLabel<label_configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW | UI_CONFIG_UNAVAILABLE_TEXT>(label_config, text, position, scale);
						}
					}
					else {
						if (is_active) {
							TextLabel<label_configuration>(label_config, text, position, scale);
						}
						else {
							TextLabel<label_configuration | UI_CONFIG_UNAVAILABLE_TEXT>(label_config, text, position, scale);
						}
					}
				}
				else {
					if constexpr (configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW) {
						if (is_active) {
							TextLabel<label_configuration | UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(label_config, text, position, scale);
						}
						else {
							TextLabel<label_configuration | UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW | UI_CONFIG_UNAVAILABLE_TEXT>(label_config, text, position, scale);
						}
					}
					else {
						if (is_active) {
							TextLabel<label_configuration | UI_CONFIG_TEXT_PARAMETERS>(label_config, text, position, scale);
						}
						else {
							TextLabel<label_configuration | UI_CONFIG_TEXT_PARAMTERS | UI_CONFIG_UNAVAILABLE_TEXT>(label_config, text, position, scale);
						}
					}
				}
			}

			bool ExistsResource(const char* name) {
				const char* string_identifier = HandleResourceIdentifier(name);
				return system->ExistsWindowResource(window_index, string_identifier);
			}

		public:
			UIDrawer(
				UIColorThemeDescriptor& _color_theme, 
				UIFontDescriptor& _font, 
				UILayoutDescriptor& _layout, 
				UIElementDescriptor& _element_descriptor
			) : color_theme(_color_theme), font(_font), layout(_layout), element_descriptor(_element_descriptor),
			export_scale(nullptr) {};

			UIDrawer(
				UIDrawerDescriptor& descriptor,
				void* _window_data
			) : dockspace(descriptor.dockspace), thread_id(descriptor.thread_id), window_index(descriptor.window_index), 
				border_index(descriptor.border_index), dockspace_type(descriptor.dockspace_type), buffers(descriptor.buffers), 
				counts(descriptor.counts), system_buffers(descriptor.system_buffers), system_counts(descriptor.system_counts),
				window_data(_window_data), mouse_position(descriptor.mouse_position), color_theme(descriptor.color_theme),
				font(descriptor.font), layout(descriptor.layout), element_descriptor(descriptor.element_descriptor),
				export_scale(descriptor.export_scale)
			{
				system = (UISystem*)descriptor.system;

				zoom_ptr = &system->m_windows[window_index].zoom;
				zoom_ptr_modifyable = &system->m_windows[window_index].zoom;

				DefaultDrawParameters();

				//SetLayoutFromZoomXFactor<true>(zoom_ptr->x);
				//SetLayoutFromZoomYFactor<true>(zoom_ptr->y);

				if constexpr (!initializer) {
					float mask = system->GetDockspaceMaskFromType(dockspace_type);
					region_position = system->GetDockspaceRegionPosition(dockspace, border_index, mask);
					region_scale = system->GetDockspaceRegionScale(dockspace, border_index, mask);
				}
				else {
					region_position = system->GetWindowPosition(window_index);
					region_scale = system->GetWindowScale(window_index);
				}

				float2 render_region = system->GetWindowRenderRegion(window_index);
				CalculateRegionParameters(region_position, region_scale, &render_region);

				identifier_stack.buffer = (unsigned char*)system->m_memory->Allocate_ts(system->m_descriptors.misc.drawer_temp_memory, 1);
				identifier_stack.size = 0;
				current_identifier.buffer = (char*)system->m_memory->Allocate_ts(system->m_descriptors.misc.drawer_identifier_memory, 1);
				current_identifier.size = 0;

				if constexpr (initializer) {
					temp_memory_from_main_allocator = Stream<void*>(system->m_memory->Allocate_ts(system->m_descriptors.misc.drawer_identifier_memory), 0);
					
					// creating temporaries to hold the values, even tho they are unneeded
					if (!descriptor.do_not_initialize_viewport_sliders) {
						void* temp_horizontal_slider;
						void* temp_vertical_slider;
						ViewportRegionSliderInitializer(&system->m_windows[window_index].render_region, "##", &temp_horizontal_slider, &temp_vertical_slider);
					}
				}
			}


			~UIDrawer() {
				if constexpr (!initializer) {
					float2 render_span = GetRenderSpan();
					float2 render_zone = GetRenderZone();

					constexpr float2 STABILIZE_EPSILON = { 0.5f, 0.5f };

					render_span.x = function::PredicateValue(fabsf(stabilized_render_span.x - render_span.x) < STABILIZE_EPSILON.x && stabilized_render_span.x > 0.0f, stabilized_render_span.x, render_span.x);
					render_span.y = function::PredicateValue(fabsf(stabilized_render_span.y - render_span.y) < STABILIZE_EPSILON.y && stabilized_render_span.y > 0.0f, stabilized_render_span.y, render_span.y);

					if (export_scale != nullptr) {
						float y_offset = dockspace->borders[border_index].draw_region_header * system->m_descriptors.misc.title_y_scale;
						*export_scale = { render_span.x + 2.0f * layout.next_row_padding, render_span.y + 2.0f * layout.next_row_y_offset + y_offset };
					}

					float2 horizontal_slider_position = system->GetDockspaceRegionHorizontalRenderSliderPosition(
						region_position,
						region_scale
					);
					float2 horizontal_slider_scale = system->GetDockspaceRegionHorizontalRenderSliderScale(
						region_scale
					);

					float2 vertical_slider_position = system->GetDockspaceRegionVerticalRenderSliderPosition(
						region_position,
						region_scale,
						dockspace->borders[border_index].draw_region_header
					);
					float2 vertical_slider_scale = system->GetDockspaceRegionVerticalRenderSliderScale(
						region_scale,
						dockspace->borders[border_index].draw_region_header,
						system->m_windows[window_index].is_horizontal_render_slider
					);


					if (!dockspace->borders[border_index].draw_region_header && dockspace->borders[border_index].draw_close_x) {
						vertical_slider_position.y += system->m_descriptors.misc.title_y_scale;
						vertical_slider_scale.y -= system->m_descriptors.misc.title_y_scale;
					}

					ViewportRegionSliders(
						"##HorizontalSlider",
						"##VerticalSlider",
						render_span,
						render_zone,
						&system->m_windows[window_index].render_region,
						horizontal_slider_position,
						horizontal_slider_scale,
						vertical_slider_position,
						vertical_slider_scale
					);
				}

				if (deallocate_identifier_buffers) {
					system->m_memory->Deallocate_ts(current_identifier.buffer);
					system->m_memory->Deallocate_ts(identifier_stack.buffer);
					if constexpr (initializer) {
						for (size_t index = 0; index < temp_memory_from_main_allocator.size; index++) {
							system->m_memory->Deallocate_ts(temp_memory_from_main_allocator[index]);
						}
						system->m_memory->Deallocate_ts(temp_memory_from_main_allocator.buffer);
					}
				}
			}

			// the initializer must return a pointer to the data and take as a parameter the resource identifier (i.e. its name)
			template<typename Initializer>
			void AddWindowResource(const char* label, Initializer&& init) {
				const char* label_identifier = HandleResourceIdentifier<true>(label);
				ResourceIdentifier identifier(label_identifier, strlen(label_identifier));
				void* data = init(label_identifier);
				system->AddWindowMemoryResourceToTable(data, identifier, window_index);
			}
			
			void AddHoverable(float2 position, float2 scale, UIActionHandler handler) {
				if constexpr (!initializer) {
					system->AddHoverableToDockspaceRegion(
						ECS_TOOLS_UI_DRAWER_FILL_ARGUMENTS,
						position,
						scale,
						handler
					);
				}
			}

			void AddTextTooltipHoverable(float2 position, float2 scale, UITextTooltipHoverableData* data, UIDrawPhase phase = UIDrawPhase::System) {
				system->AddHoverableToDockspaceRegion(thread_id, dockspace, border_index, position, scale, { TextTooltipHoverable, data, sizeof(*data), phase });
			}

			void AddClickable(
				float2 position,
				float2 scale,
				UIActionHandler handler
			) {
				if constexpr (!initializer) {
					system->AddClickableToDockspaceRegion(
						ECS_TOOLS_UI_DRAWER_FILL_ARGUMENTS,
						position, 
						scale,
						handler
					);
				}
			}
			
			void AddGeneral(
				float2 position,
				float2 scale,
				UIActionHandler handler
			) {
				if constexpr (!initializer) {
					system->AddGeneralActionToDockspaceRegion(
						ECS_TOOLS_UI_DRAWER_FILL_ARGUMENTS,
						position,
						scale,
						handler
					);
				}
			}

			void AddDefaultHoverable(
				float2 position, 
				float2 scale, 
				Color color, 
				float percentage = 1.25f, 
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				if constexpr (!initializer) {
					UISystemDefaultHoverableData data;
					data.border_index = border_index;
					data.data.colors[0] = color;
					data.dockspace = dockspace;
					data.data.percentages[0] = percentage;
					data.position = position;
					data.scale = scale;
					data.thread_id = thread_id;
					data.phase = phase;
					system->AddDefaultHoverable(data);
				}
			}

			void AddDefaultHoverable(
				float2 main_position,
				float2 main_scale,
				const float2* positions,
				const float2* scales,
				const Color* colors,
				const float* percentages,
				unsigned int count,
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				if constexpr (!initializer) {
					ECS_ASSERT(count <= ECS_TOOLS_UI_DEFAULT_HOVERABLE_DATA_COUNT);

					UISystemDefaultHoverableData data;
					for (size_t index = 0; index < count; index++) {
						data.data.colors[index] = colors[index];
						data.data.percentages[index] = percentages[index];
						data.data.positions[index] = positions[index];
						data.data.scales[index] = scales[index];
					}
					data.data.count = count;
					data.data.is_single_action_parameter_draw = false;
					data.border_index = border_index;
					data.dockspace = dockspace;
					data.thread_id = thread_id;
					data.phase = phase;
					data.position = main_position;
					data.scale = main_scale;
					system->AddDefaultHoverable(data);
				}
			}

			void AddDefaultClickable(
				float2 position,
				float2 scale,
				UIActionHandler hoverable_handler,
				UIActionHandler clickable_handler
			) {
				if constexpr (!initializer) {
					UISystemDefaultClickableData data;
					data.border_index = border_index;
					data.clickable_handler = clickable_handler;
					data.hoverable_handler = hoverable_handler;
					data.dockspace = dockspace;
					data.position = position;
					data.scale = scale;
					data.thread_id = thread_id;
					system->AddDefaultClickable(data);
				}
			}

			void AddDoubleClickAction(
				float2 position,
				float2 scale, 
				unsigned int identifier,
				size_t duration_between_clicks, 
				UIActionHandler first_click_handler,
				UIActionHandler second_click_handler
			) {
				if constexpr (!initializer) {
					system->AddDoubleClickActionToDockspaceRegion(
						thread_id,
						dockspace,
						border_index, 
						position,
						scale, 
						identifier,
						duration_between_clicks,
						first_click_handler, 
						second_click_handler
					);
				}
			}

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
			) {
				UIDefaultTextHoverableData hoverable_data;
				hoverable_data.color = color;
				hoverable_data.percentage = percentage;
				hoverable_data.character_spacing = font.character_spacing;
				hoverable_data.font_size = GetFontSize();
				hoverable_data.text = text;
				if (font_color == ECS_COLOR_WHITE) {
					hoverable_data.text_color = color_theme.default_text;
				}
				else {
					hoverable_data.text_color = font_color;
				}
				hoverable_data.text_offset = text_offset;
				
				AddDefaultClickable(position, scale, { DefaultTextHoverableAction, &hoverable_data, sizeof(hoverable_data), phase }, handler);
			}

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
			) {
				UIDefaultTextHoverableData hoverable_data;
				hoverable_data.color = color;
				hoverable_data.percentage = percentage;
				hoverable_data.character_spacing = font.character_spacing;
				hoverable_data.font_size = GetFontSize();
				hoverable_data.text = text;
				if (font_color == ECS_COLOR_WHITE) {
					hoverable_data.text_color = color_theme.default_text;
				}
				else {
					hoverable_data.text_color = font_color;
				}
				hoverable_data.text_offset = text_offset;
				hoverable_data.horizontal_cull = horizontal_cull;
				hoverable_data.vertical_cull = vertical_cull;
				hoverable_data.horizontal_cull_bound = horizontal_cull_bound;
				hoverable_data.vertical_cull_bound = vertical_cull_bound;
				hoverable_data.vertical_text = vertical_text;

				AddDefaultClickable(position, scale, { DefaultTextHoverableAction, &hoverable_data, sizeof(hoverable_data), phase }, handler);
			}

			template<size_t configuration>
			void AddDefaultLabelClickableHoverableLateOrSystem(
				const UIDrawConfig& config,
				float2 position,
				float2 scale,
				UIActionHandler handler,
				Color color,
				const char* text,
				float percentage = 1.25f
			) {
				TextAlignment horizontal_alignment, vertical_alignment;
				GetTextLabelAlignment<configuration>(config, horizontal_alignment, vertical_alignment);


			}

			void AddDefaultClickableHoverable(
				float2 position,
				float2 scale,
				UIActionHandler handler,
				Color color = ECS_COLOR_WHITE,
				float percentage = 1.25f
			) {
				if constexpr (!initializer) {
					UISystemDefaultHoverableClickableData data;
					data.border_index = border_index;
					data.clickable_handler = handler;
					if (color == ECS_COLOR_WHITE) {
						data.hoverable_data.colors[0] = color_theme.theme;
					}
					else {
						data.hoverable_data.colors[0] = color;

					}
					data.hoverable_phase = handler.phase;
					data.dockspace = dockspace;
					data.hoverable_data.percentages[0] = percentage;
					data.position = position;
					data.scale = scale;
					data.thread_id = thread_id;
					system->AddDefaultHoverableClickable(data);
				}
			}

			void AddDefaultClickableHoverable(
				float2 main_position,
				float2 main_scale,
				const float2* positions,
				const float2* scales,
				const Color* colors,
				const float* percentages,
				unsigned int count,
				UIActionHandler handler
			) {
				if constexpr (!initializer) {
					ECS_ASSERT(count <= ECS_TOOLS_UI_DEFAULT_HOVERABLE_DATA_COUNT);

					UISystemDefaultHoverableClickableData data;
					for (size_t index = 0; index < count; index++) {
						data.hoverable_data.positions[index] = positions[index];
						data.hoverable_data.scales[index] = scales[index];
						data.hoverable_data.colors[index] = colors[index];
						data.hoverable_data.percentages[index] = percentages[index];
					}
					data.hoverable_phase = handler.phase;
					data.hoverable_data.count = count;
					data.hoverable_data.is_single_action_parameter_draw = false;
					data.border_index = border_index;
					data.clickable_handler = handler;
					data.dockspace = dockspace;
					data.position = main_position;
					data.scale = main_scale;
					data.thread_id = thread_id;
					system->AddDefaultHoverableClickable(data);
				}
			}
			
			template<typename T>
			void AddDefaultClickableHoverable(
				float2 position,
				float2 scale,
				Action click_action,
				T* click_data,
				Color color = ECS_COLOR_WHITE,
				float percentage = 1.25f,
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				AddDefaultClickableHoverable(
					position,
					scale,
					{ click_action, click_data, sizeof(T), phase },
					color,
					percentage
				);
			}
			
			void AddElementDraw(size_t index, UIDrawerElementDraw draw_function, void* data, size_t data_size = 0) {
				system->AddWindowDrawerElement(window_index, index, draw_function, data, data_size);
			}

			void AddWindowDependentSizeToConfigFromPoint(float2 initial_point, UIDrawConfig& config) const {
				float x_difference = current_x - region_render_offset.x - initial_point.x;
				float y_difference = current_y - region_render_offset.y - initial_point.y;

				UIConfigWindowDependentSize size;
				size.scale_factor = { x_difference / (region_limit.x - region_fit_space_horizontal_offset), y_difference / (region_limit.y - region_fit_space_vertical_offset) };
				config.AddFlag(size);
			}

			void AddRelativeTransformToConfigFromPoint(float2 initial_point, UIDrawConfig& config) const {
				float x_difference = current_x - region_render_offset.x - initial_point.x;
				float y_difference = current_y - region_render_offset.y - initial_point.y;

				UIConfigRelativeTransform transform;
				transform.scale.x = x_difference / layout.default_element_x;
				transform.scale.y = y_difference / layout.default_element_y;
				config.AddFlag(transform);
			}

			void AddAbsoluteTransformToConfigFromPoint(float2 initial_point, UIDrawConfig& config) const {
				float x_difference = current_x - region_render_offset.x - initial_point.x;
				float y_difference = current_y - region_render_offset.y - initial_point.y;

				UIConfigAbsoluteTransform transform;
				transform.position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
				transform.scale.x = x_difference;
				transform.scale.y = y_difference;
				config.AddFlag(transform);
			}

#pragma region Alpha Color Rectangle

			void AlphaColorRectangle(Color color) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				AlphaColorRectangle<configuration>(null_config, color);
			}

			template<size_t configuration>
			void AlphaColorRectangle(const UIDrawConfig& config, float2& position, float2& scale, Color color) {
				HandleFitSpaceRectangle<configuration>(position, scale);

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {
					bool state = true;
					if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
						const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
						state = active_state->state;
					}

					Color empty_texture_color = ECS_COLOR_WHITE;
					empty_texture_color.alpha = 255 - color.alpha;
					if (!state) {
						color.alpha *= color_theme.alpha_inactive_item;
						empty_texture_color.alpha *= color_theme.alpha_inactive_item;
					}
					SpriteRectangle<configuration>(position, scale, ECS_TOOLS_UI_TEXTURE_MASK, color);
					SpriteRectangle<configuration>(position, scale, ECS_TOOLS_UI_TEXTURE_EMPTY_TEXTURE_TILES_BIG, empty_texture_color);

					HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
					HandleRectangleActions<configuration>(config, position, scale);
				}
			}

			template<size_t configuration>
			void AlphaColorRectangle(const UIDrawConfig& config, Color color) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
				
				if constexpr (!initializer) {
					AlphaColorRectangle<configuration>(config, position, scale, color);
				}

				FinalizeRectangle<configuration>(position, scale);
			}

#pragma endregion 

#pragma region Button

			void Button(
				const char* label, 
				UIActionHandler handler
			) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				Button<configuration>(null_config, label, handler);
			}

			template<size_t configuration>
			void Button(
				const UIDrawConfig& config,
				const char* text,
				UIActionHandler handler
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				bool is_active = true;

				if constexpr (!initializer) {
					if constexpr (configuration & UI_CONFIG_ACTIVE_STATE){
						const UIConfigActiveState* state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
						is_active = state->state;
					}

					if (is_active) {
						TextLabel<configuration>(config, text, position, scale);

						if constexpr (~configuration & UI_CONFIG_BUTTON_HOVERABLE) {
							Color label_color = HandleColor<configuration>(config);

							//AddDefaultHoverable(position, scale, label_color, 1.25f, handler.phase);
							if (handler.phase == UIDrawPhase::Normal) {
								UIDefaultHoverableData hoverable_data;
								hoverable_data.colors[0] = label_color;
								hoverable_data.percentages[0] = 1.25f;

								UIDrawPhase phase = UIDrawPhase::Normal;
								if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
									phase = UIDrawPhase::Late;
								}
								else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
									phase = UIDrawPhase::System;
								}
								AddDefaultClickable(position, scale, { DefaultHoverableAction, &hoverable_data, sizeof(hoverable_data), phase }, handler);
							}
							else {
								UIDefaultTextHoverableData hoverable_data;
								hoverable_data.color = label_color;
								hoverable_data.text = text;

								UISpriteVertex* vertices = HandleTextSpriteBuffer<configuration>();
								size_t* count = HandleTextSpriteCount<configuration>();

								size_t text_vertex_count = strlen(text) * 6;
								hoverable_data.text_offset = { vertices[*count - text_vertex_count].position.x - position.x, -vertices[*count - text_vertex_count].position.y - position.y };
								Color text_color;
								float2 text_size;
								float text_character_spacing;
								HandleText<configuration>(config, text_color, text_size, text_character_spacing);

								hoverable_data.character_spacing = text_character_spacing;
								hoverable_data.font_size = text_size;
								hoverable_data.text_color = text_color;

								UIDrawPhase hoverable_phase = UIDrawPhase::Normal;
								if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
									hoverable_phase = UIDrawPhase::Late;
								}
								else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
									hoverable_phase = UIDrawPhase::System;
								}
								AddDefaultClickable(position, scale, { DefaultTextHoverableAction, &hoverable_data, sizeof(hoverable_data), hoverable_phase }, handler);
							}
						}
						else {
							const UIConfigButtonHoverable* hoverable = (const UIConfigButtonHoverable*)config.GetParameter(UI_CONFIG_BUTTON_HOVERABLE);
							AddDefaultClickable(position, scale, hoverable->handler, handler);
							AddHoverable(position, scale, hoverable->handler);
						}
					}
					else {
						TextLabel<configuration | UI_CONFIG_UNAVAILABLE_TEXT>(config, text, position, scale);
					}

					if constexpr (configuration & UI_CONFIG_TOOL_TIP) {
						if (IsPointInRectangle(mouse_position, position, scale)) {
							const UIConfigToolTip* tool_tip = (const UIConfigToolTip*)config.GetParameter(UI_CONFIG_TOOL_TIP);

							UITextTooltipHoverableData hover_data;
							hover_data.characters = tool_tip->characters;
							hover_data.base.offset.y = 0.005f;
							hover_data.base.offset_scale.y = true;

							ActionData action_data = system->GetFilledActionData(window_index);
							action_data.data = &hover_data;
							TextTooltipHoverable(&action_data);
						}
					}
				}
				else {
					TextLabel<configuration>(config, text);
				}
			}

			void StateButton(const char* name, bool* state) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				StateButton<configuration>(null_config, name, state);
			}

			template<size_t configuration>
			void StateButton(const UIDrawConfig& config, const char* name, bool* state) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					bool is_active = true;
					if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
						const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
						is_active = active_state->state;
					}

					if (is_active) {
						TextLabel<configuration | UI_CONFIG_DO_NOT_CACHE>(config, name, position, scale);
						Color color = HandleColor<configuration>(config);
						if (*state) {
							color = LightenColorClamp(color, 1.4f);
							SolidColorRectangle<configuration>(position, scale, color);
						}

						AddDefaultClickableHoverable(position, scale, { BoolClickable, state, 0 }, color);
					}
					else {
						TextLabel<configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_UNAVAILABLE_TEXT>(config, name, position, scale);
					}
				}
			}

			void SpriteStateButton(
				LPCWSTR texture,
				bool* state,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f }, 
				float2 bottom_right_uv = { 1.0f, 1.0f }, 
				float2 expand_factor = {1.0f, 1.0f}
			) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				SpriteStateButton<configuration>(null_config, texture, state, color, top_left_uv, bottom_right_uv, expand_factor);
			}

			template<size_t configuration>
			void SpriteStateButton(
				const UIDrawConfig& config, 
				LPCWSTR texture,
				bool* state,
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				float2 expand_factor = { 1.0f, 1.0f })
			{
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					bool is_active = true;
					if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
						const UIConfigActiveState* active_state = (const UIConfigaActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
						is_active = active_state->state;
					}

					if (ValidatePosition<configuration>(position, scale)) {
						Color color = HandleColor<configuration>(config);

						if (*state) {
							color = LightenColorClamp(color, 1.4f);
						}
						SolidColorRectangle<configuration>(position, scale, color);
						float2 sprite_scale;
						float2 sprite_position = ExpandRectangle(position, scale, expand_factor, sprite_scale);
						if (is_active) {
							SpriteRectangle<configuration>(position, scale, texture, color, top_left_uv, bottom_right_uv);
							AddDefaultClickableHoverable(position, scale, { BoolClickable, state, 0 }, color);
						}
						else {
							color.alpha *= color_theme.alpha_inactive_item;
							SpriteRectangle<configuration>(position, scale, texture, color, top_left_uv, bottom_right_uv);
						}
					}
				}
			}

			// Stateless, label is done with UI_CONFIG_DO_NOT_CACHE; if drawing with a sprite
			// name can be made nullptr
			void MenuButton(const char* name, UIWindowDescriptor& window_descriptor, size_t border_flags = 0) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				MenuButton<configuration>(null_config, name, window_descriptor, border_flags);
			}

			// Stateless, label is done with UI_CONFIG_DO_NOT_CACHE; if drawing with a sprite
			// name can be made nullptr
			template<size_t configuration>
			void MenuButton(const UIDrawConfig& config, const char* name, UIWindowDescriptor& window_descriptor, size_t border_flags = 0) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					bool is_active = true;
					if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
						const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					}

					if constexpr (~configuration & UI_CONFIG_MENU_BUTTON_SPRITE) {
						if (is_active) {
							TextLabel<configuration | UI_CONFIG_DO_NOT_CACHE>(config, name, position, scale);
						}
						else {
							TextLabel<configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_UNAVAILABLE_TEXT>(config, name, position, scale);
						}
					}
					else {
						const UIConfigMenuButtonSprite* sprite_definition = (const UIConfigMenuButtonSprite*)config.GetParameter(UI_CONFIG_MENU_BUTTON_SPRITE);
						Color sprite_color = sprite_definition->color;
						sprite_color.alpha = function::PredicateValue(is_active, sprite_color.alpha, sprite_color.alpha * color_theme.alpha_inactive_item);
						SpriteRectangle<configuration>(
							position, 
							scale, 
							sprite_definition->texture, 
							sprite_definition->top_left_uv, 
							sprite_definition->bottom_right_uv, 
							sprite_color
						);
						FinalizeRectangle<configuration>(position, scale);
					}

					if (is_active) {
						UIDrawerMenuButtonData data;
						data.descriptor = window_descriptor;
						data.descriptor.initial_position_x = position.x;
						data.descriptor.initial_position_y = position.y + scale.y + element_descriptor.menu_button_padding;
						data.border_flags = border_flags;
						data.is_opened_when_pressed = false;
						Color color = HandleColor<configuration>(config);

						AddClickable(position, scale, { MenuButtonAction, &data, sizeof(data), UIDrawPhase::System });
						UIDrawPhase hovered_phase = UIDrawPhase::Normal;
						if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
							hovered_phase = UIDrawPhase::Late;
						}
						else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
							hovered_phase = UIDrawPhase::System;
						}

						AddDefaultHoverable(position, scale, color, 1.25f, hovered_phase);

						unsigned int pop_up_window_index = system->GetWindowFromName(window_descriptor.window_name);
						if (pop_up_window_index != -1) {
							system->SetPopUpWindowPosition(pop_up_window_index, { position.x, position.y + scale.y + element_descriptor.menu_button_padding });
						}
					}
				}
			}

#pragma endregion

			void ChangeThemeColor(Color new_theme_color) {
				color_theme.SetNewTheme(new_theme_color);
			}

#pragma region Checkbox

			void CheckBox(const char* name, bool* value_to_change) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				CheckBox<configuration>(null_config, name, value_to_change);
			}

			template<size_t configuration>
			void CheckBox(const UIDrawConfig& config, const char* name, bool* value_to_change) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					scale = GetSquareScale(scale.y);

					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerTextElement* element = (UIDrawerTextElement*)GetResource(name);

						CheckBoxDrawer<configuration>(config, element, value_to_change, position, scale);
					}
					else {
						CheckBoxDrawer<configuration>(config, name, value_to_change, position, scale);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						CheckBoxInitializer<configuration>(config, name, position);
					}
				}
			}

#pragma endregion 

#pragma region Combo box

			void ComboBox(const char* name, Stream<const char*> labels, unsigned int label_display_count, unsigned char* active_label) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				ComboBox<configuration>(null_config, name, labels, label_display_count, active_label);
			}

			template<size_t configuration>
			void ComboBox(UIDrawConfig& config, const char* name, Stream<const char*> labels, unsigned int label_display_count, unsigned char* active_label) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
				
				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerComboBox* data = (UIDrawerComboBox*)GetResource(name);

						data->active_label = active_label;
						float min_value = data->labels[data->biggest_label_x_index].scale.x * data->labels[data->biggest_label_x_index].GetInverseZoomX() * zoom_ptr->x 
							+ system->NormalizeHorizontalToWindowDimensions(scale.y) + 2 * element_descriptor.label_horizontal_padd;
						min_value += data->prefix_x_scale * zoom_ptr->x;
						scale.x = function::PredicateValue(min_value > scale.x, min_value, scale.x);

						ComboBoxDrawer<configuration | UI_CONFIG_DO_NOT_VALIDATE_POSITION>(config, data, active_label, position, scale);
						HandleDynamicResource<configuration>(name);
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeComboBox initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_4(initialize_data, name, labels, label_display_count, active_label);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeComboBoxElement<DynamicConfiguration(configuration)>);
						}
						ComboBox<DynamicConfiguration(configuration)>(config, name, labels, label_display_count, active_label);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						ComboBoxInitializer<configuration>(config, name, labels, label_display_count, active_label, position, scale);
					}
				}

			}

			template<size_t configuration>
			void ComboBoxDropDownDrawer(const UIDrawConfig& config, UIDrawerComboBox* data) {
				float2 position = { data->starting_position.x - region_render_offset.x, data->starting_position.y - region_render_offset.y };
				float2 scale = { region_scale.x, data->label_y_scale };

				constexpr size_t text_label_configuration = NullifyConfiguration(configuration, UI_CONFIG_GET_TRANSFORM) | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y 
					| UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_LABEL_TRANSPARENT;
				
				Color color = HandleColor<configuration>(config);
				color = DarkenColor(color, 0.8f);
				Color normal_color = color;
				Color active_color = ToneColor(color, 1.3f);

				for (size_t index = 0; index < data->labels.size; index++) {
					if (ValidatePosition<0>(position, scale)) {
						if (index == *data->active_label) {
							color = active_color;
						}
						else {
							color = normal_color;
						}

						float2 rectangle_position = position;
						float2 rectangle_scale = {
							//data->labels[data->biggest_label_x_index].scale.x + 2 * element_descriptor.label_horizontal_padd + system->NormalizeHorizontalToWindowDimensions(scale.y), 
							scale.x + system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.dockspaces.border_size),
							scale.y 
						};
						SolidColorRectangle<text_label_configuration>(rectangle_position, rectangle_scale, color);
						UIDrawerComboBoxLabelClickable click_data;
						click_data.index = index;
						click_data.box = data;

						UIDefaultTextHoverableData hoverable_data;
						hoverable_data.color = color;
						hoverable_data.vertices = data->labels[index].text_vertices.buffer;
						hoverable_data.vertex_count = data->labels[index].text_vertices.size;
						hoverable_data.percentage = 1.6f;
						hoverable_data.horizontal_cull = true;
						hoverable_data.horizontal_cull_bound = rectangle_position.x + rectangle_scale.x;

						UIDrawConfig align_config;
						UIConfigTextAlignment alignment;
						alignment.horizontal = TextAlignment::Left;
						alignment.vertical = TextAlignment::Middle;
						float x_position, y_position;

						align_config.AddFlag(alignment);
						HandleTextLabelAlignment<UI_CONFIG_TEXT_ALIGNMENT>(
							align_config,
							data->labels[index].scale,
							rectangle_scale,
							rectangle_position,
							x_position,
							y_position,
							alignment.horizontal,
							alignment.vertical
						);

						hoverable_data.text_offset = { x_position - rectangle_position.x - system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.dockspaces.border_size), y_position - rectangle_position.y };

						UIActionHandler hoverable_handler;
						hoverable_handler.action = DefaultTextHoverableAction;
						hoverable_handler.data = &hoverable_data;
						hoverable_handler.data_size = sizeof(hoverable_data);
						rectangle_position.x += system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.dockspaces.border_size);
						rectangle_scale.x -= 2 * system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.dockspaces.border_size);
						if (index == 0) {
							//rectangle_position.y += system->m_descriptors.dockspaces.border_size;
							////rectangle_scale.y -= system->m_descriptors.dockspaces.border_size;
							//hoverable_data.text_offset.y -= system->m_descriptors.dockspaces.border_size;
						}
						else if (index == data->labels.size - 1) {
							rectangle_scale.y -= system->m_descriptors.dockspaces.border_size;
						}
						AddDefaultClickable(rectangle_position, rectangle_scale, hoverable_handler, { ComboBoxLabelClickable, &click_data, sizeof(click_data), UIDrawPhase::System });
						TextLabelDrawer<text_label_configuration>(config, &data->labels[index], position, scale);
					}
					position.y += scale.y;
				}

				if (position.y < region_position.y + region_scale.y) {
					SolidColorRectangle<text_label_configuration>({ position.x + region_render_offset.x, position.y }, {2.0f, region_position.y + region_scale.y - position.y}, color);
				}
			}

#pragma endregion

#pragma region Collapsing Header

			template<typename Function>
			bool CollapsingHeader(const char* name, Function&& function) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				return CollapsingHeader<configuration>(null_config, name, function);
			}

			template<size_t configuration, typename Function>
			bool CollapsingHeader(UIDrawConfig& config, const char* name, Function&& function) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				auto call_function = [&]() {
					OffsetNextRow(layout.node_indentation);
					current_x = GetNextRowXPosition();
					function();
					OffsetNextRow(-layout.node_indentation);
					NextRow();
				};

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerCollapsingHeader* element = (UIDrawerCollapsingHeader*)GetResource(name);

						CollapsingHeaderDrawer<configuration>(config, element, position, scale);
						if (element->state) {
							call_function();
						}
						return element->state;
					}
					else {
						UIConfigCollapsingHeaderDoNotCache* data = (UIConfigCollapsingHeaderDoNotCache*)config.GetParameter(UI_CONFIG_DO_NOT_CACHE);
						CollapsingHeaderDrawer<configuration>(config, name, data->state, position, scale);

						if (*data->state) {
							call_function();
						}
						return *data->state;
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						CollapsingHeaderInitializer<configuration>(config, name, position, scale);
					}
					function();
					return true;
				}
			}

#pragma endregion

#pragma region Color Input

			void ColorInput(const char* name, Color* color, Color default_color = ECS_COLOR_WHITE) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				ColorInput<configuration>(null_config, name, color, default_color);
			}

			template<size_t configuration>
			void ColorInput(UIDrawConfig& config, const char* name, Color* color, Color default_color = ECS_COLOR_WHITE) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerColorInput* data = (UIDrawerColorInput*)GetResource(name);

						ColorInputDrawer<configuration>(config, data, position, scale, color, default_color);
						HandleDynamicResource<configuration>(name);
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeColorInput initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_3(initialize_data, color, name, default_color);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeColorInputElement<DynamicConfiguration(configuration)>);
						}
						ColorInput<DynamicConfiguration(configuration)>(config, name, color, default_color);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						ColorInputInitializer<configuration>(config, name, color, default_color, position, scale);
					}
				}

			}

#pragma endregion

#pragma region CrossLine

			// Setting the y size for horizontal or x size for vertical as 0.0f means get default 1 pixel width
			void CrossLine() {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				CrossLine<configuration>(null_config);
			}

			template<size_t configuration>
			void CrossLine(const UIDrawConfig& config) {
				if constexpr (!initializer) {
					ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

					if constexpr (~configuration & UI_CONFIG_CROSS_LINE_DO_NOT_INFER) {
						if constexpr (configuration & UI_CONFIG_VERTICAL) {
							scale.y = region_position.y + region_scale.y - position.y - layout.next_row_y_offset;
							scale.x = system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.dockspaces.border_size * 1.0f);
						}
						else {
							scale.x = region_position.x + region_scale.x - position.x - (region_fit_space_horizontal_offset - region_position.x);
							scale.y = system->m_descriptors.dockspaces.border_size * 1.0f;
						}
					}
					else {
						if constexpr (configuration & UI_CONFIG_VERTICAL) {
							if (scale.x == 0.0f) {
								scale.x = system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.dockspaces.border_size * 1.0f);
							}
						}
						else {
							if (scale.y == 0.0f) {
								scale.y = system->m_descriptors.dockspaces.border_size;
							}
						}
					}

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					if (ValidatePosition<configuration>(position, scale)) {
						if constexpr (~configuration & UI_CONFIG_COLOR) {
							//SpriteRectangle<configuration>(position, scale, ECS_TOOLS_UI_TEXTURE_LINE, { 0.0f, 0.0f }, { 1.0f, 1.0f }, color_theme.borders);
							SolidColorRectangle<configuration>(position, scale, color_theme.borders);
						}
						else {
							Color color = HandleColor<configuration>(config);
							SolidColorRectangle<configuration>(position, scale, color);
							//SpriteRectangle<configuration>(position, scale, ECS_TOOLS_UI_TEXTURE_LINE, { 0.0f, 0.0f }, { 1.0f, 1.0f }, color);
						}
					}
					
					if constexpr (~configuration & UI_CONFIG_DO_NOT_ADVANCE) {
						if constexpr (configuration & UI_CONFIG_VERTICAL) {
							current_x += scale.x + layout.element_indentation;
						}
						else {
							current_y += scale.y + layout.next_row_y_offset;
						}
					}
				}
			}

#pragma endregion

			void DisablePaddingForRenderSliders() {
				region_limit.x += system->m_descriptors.misc.render_slider_vertical_size;
				region_limit.y += system->m_descriptors.misc.render_slider_horizontal_size;
				no_padding_for_render_sliders = true;
			}

			void DisablePaddingForRenderRegion() {
				no_padding_render_region = true;
			}

			void DisableZoom() const {
				if constexpr (!initializer) {
					UIDefaultWindowHandler* data = system->GetDefaultWindowHandlerData(window_index);
					data->allow_zoom = false;
				}
			}

			void DrawElement(size_t index) {
				system->m_windows[window_index].element_draw[index].draw(system->m_windows[window_index].element_draw[index].data, this);
			}

#pragma region Filter Menu

			// States should be a stack pointer with bool* to the members that need to be changed
			void FilterMenu(const char* name, Stream<const char*> label_names, bool** states) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				FilterMenu<configuration>(null_config, name, label_names, states);
			}

			// States should be a stack pointer with bool* to the members that need to be changed
			template<size_t configuration>
			void FilterMenu(const UIDrawConfig& config, const char* name, Stream<const char*> label_names, bool** states) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerFilterMenuData* data = (UIDrawerFilterMenuData*)GetResource(name);

						FilterMenuDrawer<configuration>(config, name, data);
						HandleDynamicResource<configuration>(name);
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeFilterMenu initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_3(initialize_data, name, label_names, states);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeFilterMenuElement<DynamicConfiguration(configuration)>);
						}
						FilterMenu<DynamicConfiguration(configuration)>(config, name, label_names, states);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						FilterMenuInitializer<configuration>(config, name, label_names, states);
					}
				}
			}

			// States should be a stack pointer to a stable bool array
			void FilterMenu(const char* name, Stream<const char*> label_names, bool* states) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				FilterMenu<configuration>(null_config, name, label_names, states);
			}

			// States should be a stack pointer to a stable bool array
			template<size_t configuration>
			void FilterMenu(const UIDrawConfig& config, const char* name, Stream<const char*> label_names, bool* states) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerFilterMenuSinglePointerData* data = (UIDrawerFilterMenuSinglePointerData*)GetResource(name);

						FilterMenuDrawer<configuration>(config, name, data);
						HandleDynamicResource<configuration>(name);
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeFilterMenuSinglePointer initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_3(initialize_data, label_names, name, states);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeFilterMenuSinglePointerElement<DynamicConfiguration(configuration)>);
						}
						FilterMenu<DynamicConfiguration(configuration)>(config, name, label_names, states);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						FilterMenuInitializer<configuration>(config, name, label_names, states);
					}
				}
			}

			template<size_t configuration>
			void FilterMenuDrawer(const UIDrawConfig& config, const char* name, UIDrawerFilterMenuData* data) {
				UIWindowDescriptor window_descriptor;
				window_descriptor.initial_size_x = 10000.0f;
				window_descriptor.initial_size_y = 10000.0f;
				window_descriptor.draw = FilterMenuDraw<false>;
				window_descriptor.initialize = FilterMenuDraw<true>;

				window_descriptor.window_name = data->window_name;

				window_descriptor.window_data = data;
				window_descriptor.window_data_size = 0;

				MenuButton<configuration>(config, name, window_descriptor);
			}

			template<size_t configuration>
			UIDrawerFilterMenuData* FilterMenuInitializer(const UIDrawConfig& config, const char* name, Stream<const char*> label_names, bool** states) {
				UIDrawerFilterMenuData* data = nullptr;

				AddWindowResource(name, [&](const char* identifier) {
					size_t total_memory = sizeof(UIDrawerFilterMenuData);
					total_memory += sizeof(const char*) * label_names.size;
					total_memory += sizeof(bool*) * label_names.size;
					size_t identifier_size = strlen(identifier) + 1;
					size_t window_name_size = identifier_size + strlen("Filter Window");
					total_memory += sizeof(char) * window_name_size;
					if constexpr (configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
						for (size_t index = 0; index < label_names.size; index++) {
							total_memory += strlen(label_names[index]) + 1;
						}
					}

					data = (UIDrawerFilterMenuData*)GetMainAllocatorBuffer(total_memory);
					data->name = identifier;
					uintptr_t ptr = (uintptr_t)data;
					ptr += sizeof(UIDrawerFilterMenuData);

					data->labels.buffer = (const char**)ptr;
					data->labels.size = label_names.size;
					ptr += sizeof(const char*) * label_names.size;

					data->states = (bool**)ptr;
					ptr += sizeof(bool*) * label_names.size;
					memcpy(data->states, states, sizeof(bool*) * label_names.size);

					char* window_name = (char*)ptr;
					memcpy(window_name, identifier, identifier_size);
					strcat(window_name, "Filter Window");
					data->window_name = window_name;
					ptr += window_name_size;

					if constexpr (~configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
						memcpy(data->labels.buffer, label_names.buffer, label_names.size * sizeof(const char*));
					}
					else {
						for (size_t index = 0; index < label_names.size; index++) {
							char* char_ptr = (char*)ptr;
							size_t name_length = strlen(label_names[index]) + 1;
							memcpy(char_ptr, label_names[index], name_length * sizeof(char));
							data->labels[index] = char_ptr;
							ptr += sizeof(char) * name_length;
						}
					}

					if constexpr (configuration & UI_CONFIG_FILTER_MENU_ALL) {
						data->draw_all = true;
					}
					else {
						data->draw_all = false;
					}

					if constexpr (configuration & UI_CONFIG_FILTER_MENU_NOTIFY_ON_CHANGE) {
						UIConfigFilterMenuNotify* notifier = (UIConfigFilterMenuNotify*)config.GetParameter(UI_CONFIG_FILTER_MENU_NOTIFY_ON_CHANGE);
						data->notifier = notifier->notifier;
					}
					else {
						data->notifier = nullptr;
					}

					return data;
				});
				
				return data;
			}

			template<size_t configuration>
			void FilterMenuDrawer(const UIDrawConfig& config, const char* name, UIDrawerFilterMenuSinglePointerData* data) {
				UIWindowDescriptor window_descriptor;
				window_descriptor.initial_size_x = 10000.0f;
				window_descriptor.initial_size_y = 10000.0f;
				window_descriptor.draw = FilterMenuSinglePointerDraw<false>;
				window_descriptor.initialize = FilterMenuSinglePointerDraw<true>;

				window_descriptor.window_name = data->window_name;

				window_descriptor.window_data = data;
				window_descriptor.window_data_size = 0;

				MenuButton<configuration>(config, name, window_descriptor);
			}

			template<size_t configuration>
			UIDrawerFilterMenuSinglePointerData* FilterMenuInitializer(const UIDrawConfig& config, const char* name, Stream<const char*> label_names, bool* states) {
				UIDrawerFilterMenuSinglePointerData* data = nullptr;
				AddWindowResource(name, [&](const char* identifier) {
					size_t total_memory = sizeof(UIDrawerFilterMenuSinglePointerData);
					total_memory += sizeof(const char*) * label_names.size;
					size_t identifier_size = strlen(identifier) + 1;
					size_t window_name_size = identifier_size + strlen("Filter Window");
					total_memory += sizeof(char) * window_name_size;
					if constexpr (configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
						for (size_t index = 0; index < label_names.size; index++) {
							total_memory += strlen(label_names[index]) + 1;
						}
					}

					data = (UIDrawerFilterMenuSinglePointerData*)GetMainAllocatorBuffer(total_memory);
					data->name = identifier;
					uintptr_t ptr = (uintptr_t)data;
					ptr += sizeof(UIDrawerFilterMenuSinglePointerData);

					data->labels.buffer = (const char**)ptr;
					data->labels.size = label_names.size;
					ptr += sizeof(const char*) * label_names.size;

					data->states = states;
					char* window_name = (char*)ptr;
					memcpy(window_name, identifier, identifier_size);
					strcat(window_name, "Filter Window");
					data->window_name = window_name;
					ptr += window_name_size;

					if constexpr (~configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
						memcpy(data->labels.buffer, label_names.buffer, label_names.size * sizeof(const char*));
					}
					else {
						for (size_t index = 0; index < label_names.size; index++) {
							char* char_ptr = (char*)ptr;
							size_t name_length = strlen(label_names[index]) + 1;
							memcpy(char_ptr, label_names[index], name_length * sizeof(char));
							data->labels[index] = char_ptr;
							ptr += sizeof(char) * name_length;
						}
					}

					if constexpr (configuration & UI_CONFIG_FILTER_MENU_ALL) {
						data->draw_all = true;
					}
					else {
						data->draw_all = false;
					}

					if constexpr (configuration & UI_CONFIG_FILTER_MENU_NOTIFY_ON_CHANGE) {
						UIConfigFilterMenuNotify* notifier = (UIConfigFilterMenuNotify*)config.GetParameter(UI_CONFIG_FILTER_MENU_NOTIFY_ON_CHANGE);
						data->notifier = notifier->notifier;
					}
					else {
						data->notifier = nullptr;
					}

					return data;
				});

				return data;
			}

#pragma endregion

			template<size_t configuration>
			void FitTextToScale(
				UIDrawConfig& config,
				float2& font_size, 
				float& character_spacing,
				Color& color, 
				float2 scale, 
				float padding, 
				UIConfigTextParameters& previous_parameters
			) {
				HandleText<configuration>(config, color, font_size, character_spacing);

				float old_scale = font_size.y;
				font_size.y = system->GetTextSpriteSizeToScale(scale.y - padding * 2);
				font_size.x = font_size.y * ECS_TOOLS_UI_FONT_X_FACTOR;
				float factor = font_size.y / old_scale;
				character_spacing *= factor;

				UIConfigTextParameters parameters;
				parameters.character_spacing = character_spacing;
				parameters.color = color;
				parameters.size = font_size;
				SetConfigParameter<configuration, UI_CONFIG_TEXT_PARAMETERS>(config, parameters, previous_parameters);
			}

			template<size_t configuration>
			void FinishFitTextToScale(UIDrawConfig& config, const UIConfigTextParameters& previous_parameters) {
				RemoveConfigParameter<configuration, UI_CONFIG_TEXT_PARAMETERS>(config, previous_parameters);
			}

			void FinalizeElementDraw(size_t index) {
				system->FinalizeWindowElementDraw(window_index, index);
			}

#pragma region Gradients

			template<size_t configuration>
			void Gradient(const UIDrawConfig& config, const Color* colors, size_t horizontal_count, size_t vertical_count) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					HandleFitSpaceRectangle<configuration>(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}
					if (ValidatePosition<configuration>(position, scale)) {
						float x_scale = scale.x / horizontal_count;
						float y_scale = scale.y / vertical_count;

						ColorFloat top_left(colors[0]), top_right(colors[1]), bottom_left(colors[2]), bottom_right(colors[3]);

						LPCWSTR texture;
						float2 top_left_uv;
						float2 uv_delta;
						float2 current_top_left_uv;
						float2 current_bottom_right_uv;

						if constexpr (configuration & UI_CONFIG_SPRITE_GRADIENT) {
							const UIConfigSpriteGradient* sprite_info = (const UIConfigSpriteGradient*)config.GetParameter(UI_CONFIG_SPRITE_GRADIENT);
							texture = sprite_info->texture;
							top_left_uv = sprite_info->top_left_uv;
							uv_delta = { sprite_info->bottom_right_uv.x - top_left_uv.x, sprite_info->bottom_right_uv.y - top_left_uv.y };
							uv_delta.x /= horizontal_count;
							uv_delta.y /= vertical_count;

							SetSpriteClusterTexture<configuration>(texture, horizontal_count * vertical_count);
						}

						float2 rectangle_position = position;
						float2 rectangle_scale = { x_scale, y_scale };

						float inverse_horizontal_count_float = 1.0f / static_cast<float>(horizontal_count);
						float inverse_vertical_count_float = 1.0f / static_cast<float>(vertical_count);

						for (size_t row = 0; row < vertical_count; row++) {
							ColorFloat current_colors[4];
							current_colors[0] = function::PlanarLerp(top_left, top_right, bottom_left, bottom_right, 0.0f, row * inverse_vertical_count_float);
							current_colors[1] = function::PlanarLerp(top_left, top_right, bottom_left, bottom_right, inverse_horizontal_count_float, row * inverse_vertical_count_float);
							current_colors[2] = function::PlanarLerp(top_left, top_right, bottom_left, bottom_right, 0.0f, (row + 1) * inverse_vertical_count_float);
							current_colors[3] = function::PlanarLerp(top_left, top_right, bottom_left, bottom_right, inverse_horizontal_count_float, (row + 1) * inverse_vertical_count_float);

							if constexpr (configuration & UI_CONFIG_SPRITE_GRADIENT) {
								current_top_left_uv = { top_left_uv.x, top_left_uv.y + uv_delta.y * row };
								current_bottom_right_uv = { top_left_uv.x + uv_delta.x, current_top_left_uv.y + uv_delta.y };
								VertexColorSpriteClusterRectangle<configuration | UI_CONFIG_DO_NOT_ADVANCE>(rectangle_position, rectangle_scale, texture, current_colors, current_top_left_uv, current_bottom_right_uv);
								current_top_left_uv.x += uv_delta.x;
								current_bottom_right_uv.x += uv_delta.x;
							}
							else {
								VertexColorRectangle<configuration | UI_CONFIG_DO_NOT_ADVANCE>(rectangle_position, rectangle_scale, current_colors);
							}

							rectangle_position.x += x_scale;
							for (size_t column = 1; column < horizontal_count; column++) {
								current_colors[0] = current_colors[1];
								current_colors[2] = current_colors[3];
								current_colors[1] = function::PlanarLerp(top_left, top_right, bottom_left, bottom_right, (column + 1) * inverse_horizontal_count_float, row * inverse_vertical_count_float);
								current_colors[3] = function::PlanarLerp(top_left, top_right, bottom_left, bottom_right, (column + 1) * inverse_horizontal_count_float, (row + 1) * inverse_vertical_count_float);

								if constexpr (configuration & UI_CONFIG_SPRITE_GRADIENT) {
									VertexColorSpriteClusterRectangle<configuration | UI_CONFIG_DO_NOT_ADVANCE>(
										rectangle_position,
										rectangle_scale,
										texture,
										current_colors,
										current_top_left_uv,
										current_bottom_right_uv
										);
									current_top_left_uv.x += uv_delta.x;
									current_bottom_right_uv.x += uv_delta.x;
								}
								else {
									VertexColorRectangle<configuration | UI_CONFIG_DO_NOT_ADVANCE>(
										rectangle_position,
										rectangle_scale,
										current_colors
										);
								}
								rectangle_position.x += x_scale;
							}
							rectangle_position.x = position.x;
							rectangle_position.y += y_scale;
						}

						HandleBorder<configuration>(config, position, scale);

						HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
						HandleRectangleActions<configuration>(config, position, scale);
					}
				}

				FinalizeRectangle<configuration>(position, scale);
			}

#pragma endregion

#pragma region Graphs

			template<typename Stream>
			void Graph(Stream samples, const char* name = nullptr, size_t x_axis_precision = 2, size_t y_axis_precision = 2) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				Graph<configuration>(null_config, samples, name, x_axis_precision, y_axis_precision);
			}

			template<size_t configuration, typename Stream>
			void Graph(
				const UIDrawConfig& config, 
				Stream unfiltered_samples, 
				float filter_delta, 
				const char* name = nullptr,
				size_t x_axis_precision = 2,
				size_t y_axis_precision = 2
			) {
				if constexpr (!initializer) {
					ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

					HandleFitSpaceRectangle<configuration>(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					if (ValidatePosition<configuration>(position)) {
						float2* samples = GetTempBuffer(sizeof(float2) * 128);
						Stream filtered_samples = Stream(samples, 0);
						float first_value = 0.0f;
						float second_value = 0.0f;
						if constexpr (configuration & UI_CONFIG_GRAPH_FILTER_SAMPLES_X) {
							first_value = unfiltered_samples[0].x;
						}
						else if constexpr (configuration & UI_CONFIG_GRAPH_FILTER_SAMPLES_Y) {
							first_value = unfiltered_samples[0].y;
						}
						filtered_samples.Add(unfiltered_samples[0]);

						for (size_t index = 1; index < unfiltered_samples.size; index++) {
							if constexpr (configuration & UI_CONFIG_GRAPH_FILTER_SAMPLES_X) {
								second_value = unfiltered_samples[index].x;
							}
							else if constexpr (configuration & UI_CONFIG_GRAPH_FILTER_SAMPLES_Y) {
								second_value = unfiltered_samples[index].y;
							}
							if (fabs(second_value - first_value) > filter_delta) {
								filtered_samples.Add(unfiltered_samples[index]);
								if constexpr (configuration & UI_CONFIG_GRAPH_FILTER_SAMPLES_X) {
									first_value = unfiltered_samples[index].x;
								}
								else {
									first_value = unfiltered_samples[index].y;
								}
							}
						}
						GraphDrawer<configuration>(config, name, filtered_samples, position, scale, x_axis_precision, y_axis_precision);
					}
				}
			}

			template<size_t configuration, typename Stream>
			void Graph(const UIDrawConfig& config, Stream samples, const char* name = nullptr, size_t x_axis_precision = 2, size_t y_axis_precision = 2) {
				if constexpr (!initializer) {
					ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

					GraphDrawer<configuration>(config, name, samples, position, scale, x_axis_precision, y_axis_precision);
				}
			}

			template<size_t configuration, typename Stream>
			void MultiGraph(const UIDrawConfig& config, Stream samples, size_t y_sample_count, const Color* colors = multi_graph_colors, const char* name = nullptr, size_t x_axis_precision = 2, size_t y_axis_precision = 2) {
				if constexpr (!initializer) {
					ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

					MultiGraphDrawer<configuration>(config, name, samples, y_sample_count, colors, position, scale, x_axis_precision, y_axis_precision);
				}
			}

#pragma endregion
			
			const char* GetElementName(unsigned int index) const {
				return system->GetDrawElementName(window_index, index);
			}

			float GetDefaultBorderThickness() const {
				return system->m_descriptors.dockspaces.border_size;
			}

			float2 GetElementDefaultScale() const {
				return { layout.default_element_x, layout.default_element_y };
			}

			// Accounts for zoom
			float2 GetStaticElementDefaultScale() const {
				return { layout.default_element_x / zoom_ptr->x, layout.default_element_y / zoom_ptr->y };
			}

			float2 GetCurrentPosition() const {
				return { current_x - region_render_offset.x, current_y - region_render_offset.y };
			}

			float2 GetCurrentPositionNonOffset() const {
				return { current_x, current_y };
			}

			float2 GetCurrentPositionStatic() const {
				return { current_x + region_render_offset.x, current_y + region_render_offset.y };
			}

			float2 GetWindowSizeFactors(WindowSizeTransformType type, float2 scale) const {
				float2 factors;
				switch (type) {
				case WindowSizeTransformType::Horizontal:
					factors = {
						scale.x / (region_limit.x - region_fit_space_horizontal_offset),
						scale.y / layout.default_element_y
					};
					break;
				case WindowSizeTransformType::Vertical:
					factors = {
						scale.x / layout.default_element_x,
						scale.y / (region_limit.y - region_fit_space_vertical_offset)
					};
					break;
				case WindowSizeTransformType::Both:
					factors = {
						scale.x / (region_limit.x - region_fit_space_horizontal_offset),
						scale.y / (region_limit.y - region_fit_space_vertical_offset)
					};
					break;
				}

				return factors;
			}

			float2 GetWindowSizeScaleElement(WindowSizeTransformType type, float2 scale_factors) const {
				float2 scale;
				switch (type) {
				case WindowSizeTransformType::Horizontal:
					scale = {
						scale_factors.x * (region_limit.x - region_fit_space_horizontal_offset),
						layout.default_element_y * scale_factors.y
					};
					scale.x = function::PredicateValue(scale.x == 0.0f, region_limit.x - current_x, scale.x);
					//scale.x = function::PredicateValue(scale.x < 0.0f, 0.0f, scale.x);
					break;
				case WindowSizeTransformType::Vertical:
					scale = {
						scale_factors.x * layout.default_element_x,
						scale_factors.y * (region_limit.y - region_fit_space_vertical_offset)
					};
					scale.y = function::PredicateValue(scale.y == 0.0f, region_limit.y - current_y, scale.y);
					//scale.y = function::PredicateValue(scale.y < 0.0f, 0.0f, scale.y);
					break;
				case WindowSizeTransformType::Both:
					scale = { scale_factors.x * (region_limit.x - region_fit_space_horizontal_offset), scale_factors.y * (region_limit.y - region_fit_space_vertical_offset) };
					scale.x = function::PredicateValue(scale.x == 0.0f, region_limit.x - current_x, scale.x);
					scale.y = function::PredicateValue(scale.y == 0.0f, region_limit.y - current_y, scale.y);
					//scale.x = function::PredicateValue(scale.x < 0.0f, 0.0f, scale.x);
					//scale.y = function::PredicateValue(scale.y < 0.0f, 0.0f, scale.y);
					break;
				}
				return scale;
			}

			float2 GetWindowSizeScaleUntilBorder() const {
				return { (region_limit.x - current_x) / (region_limit.x - region_fit_space_horizontal_offset), 1.0f };
			}

			void* GetTempBuffer(size_t size, size_t alignment = 8) {
				if constexpr (!initializer) {
					return system->m_resources.thread_resources[thread_id].temp_allocator.Allocate(size, alignment);
				}
				else {
					void* allocation = system->m_memory->Allocate_ts(size, alignment);
					temp_memory_from_main_allocator.Add(allocation);
					return allocation;
				}
			}

			UISystem* GetSystem() {
				return system;
			}

			float2 GetSquareScale(float value) const {
				return { system->NormalizeHorizontalToWindowDimensions(value), value };
			}

			void* GetResource(const char* string) {
				const char* string_identifier = HandleResourceIdentifier(string);
				return system->FindWindowResource(window_index, string_identifier, strlen(string_identifier));
			}

			float2 GetRegionPosition() const {
				return region_position;
			}

			float2 GetRegionScale() const {
				return region_scale;
			}

			// returns the render bound difference
			float2 GetRenderSpan() const {
				return { max_render_bounds.x - min_render_bounds.x, max_render_bounds.y - min_render_bounds.y };
			}

			// returns the viewport visible zone
			float2 GetRenderZone() const {
				float horizontal_region_difference;
				horizontal_region_difference = region_scale.x - 2 * layout.next_row_padding - system->m_descriptors.misc.render_slider_vertical_size + 0.001f;
				horizontal_region_difference += function::PredicateValue(no_padding_for_render_sliders, system->m_descriptors.misc.render_slider_vertical_size, 0.0f);
				horizontal_region_difference += function::PredicateValue(no_padding_render_region, 2 * layout.next_row_padding, 0.0f);
				
				float vertical_region_difference;
				vertical_region_difference = region_scale.y - 2 * layout.next_row_y_offset - system->m_descriptors.misc.render_slider_horizontal_size + 0.001f;
				vertical_region_difference += function::PredicateValue(no_padding_for_render_sliders, system->m_descriptors.misc.render_slider_horizontal_size, 0.0f);
				vertical_region_difference += function::PredicateValue(no_padding_render_region, 2 * layout.next_row_y_offset, 0.0f);

				if (dockspace->borders[border_index].draw_elements) {
					vertical_region_difference -= system->m_descriptors.misc.title_y_scale;
				}

				return { horizontal_region_difference, vertical_region_difference };
			}

			void** GetBuffers() {
				return buffers;
			}

			template<size_t configuration>
			UIDrawerBufferState GetBufferState() {
				UIDrawerBufferState state;
				if constexpr (!initializer) {
					state.solid_color_count = *HandleSolidColorCount<configuration>();
					state.sprite_count = *HandleSpriteCount<configuration>();
					state.text_sprite_count = *HandleTextSpriteCount<configuration>();
					state.sprite_cluster_count = *HandleSpriteClusterCount<configuration>();
				}
				return state;
			}

			UIDrawerHandlerState GetHandlerState() {
				UIDrawerHandlerState state;

				if constexpr (!initializer) {
					state.hoverable_count = dockspace->borders[border_index].hoverable_handler.position_x.size;
					state.clickable_count = dockspace->borders[border_index].clickable_handler.position_x.size;
					state.general_count = dockspace->borders[border_index].general_handler.position_x.size;
				}

				return state;
			}

			size_t* GetCounts() {
				return counts;
			}

			void** GetSystemBuffers() {
				return system_buffers;
			}

			size_t* GetSystemCounts() {
				return system_counts;
			}

			float2 GetFontSize() const {
				return { ECS_TOOLS_UI_FONT_X_FACTOR * font.size, font.size };
			}

			UIFontDescriptor* GetFontDescriptor() {
				return &font;
			}

			UILayoutDescriptor* GetLayoutDescriptor() {
				return &layout;
			}

			UIColorThemeDescriptor* GetColorThemeDescriptor() {
				return &color_theme;
			}

			UIElementDescriptor* GetElementDescriptor() {
				return &element_descriptor;
			}

			template<size_t configuration = 0>
			float2 GetLastSolidColorRectanglePosition(unsigned int previous_rectangle = 0) {
				UIVertexColor* buffer = HandleSolidColorBuffer<configuration>();
				size_t* count = HandleSolidColorCount<configuration>();
				
				if (*count > 6 * previous_rectangle) {
					size_t new_index = *count - previous_rectangle * 6 - 6;
					return { buffer[new_index].position.x, -buffer[new_index].position.y };
				}
				return float2(0.0f, 0.0f);
			}

			template<size_t configuration = 0>
			float2 GetLastSolidColorRectangleScale(unsigned int previous_rectangle = 0) {
				UIVertexColor* buffer = HandleSolidColorBuffer<configuration>();
				size_t* count = HandleSolidColorCount<configuration>();

				if (*count > 6 * previous_rectangle) {
					size_t new_index = *count - previous_rectangle * 6;
					return { buffer[new_index - 5].position.x - buffer[new_index - 6].position.x, buffer[new_index - 3].position.y - buffer[new_index - 1].position.y };
				}
				return float2(0.0f, 0.0f);
			}

			template<size_t configuration = 0>
			float2 GetLastSpriteRectanglePosition(unsigned int previous_rectangle = 0) {
				UISpriteVertex* buffer = HandleSpriteBuffer<configuration>();
				size_t* count = HandleSpriteCount<configuration>();

				if (*count > 6 * previous_rectangle) {
					size_t new_index = *count - previous_rectangle * 6 - 6;
					return { buffer[new_index].position.x, -buffer[new_index].position.y };
				}
				return float2(0.0f, 0.0f);
			}

			template<size_t configuration = 0>
			float2 GetLastSpriteRectangleScale(unsigned int previous_rectangle = 0) {
				UISpriteVertex* buffer = HandleSpriteBuffer<configuration>();
				size_t* count = HandleSpriteCount<configuration>();

				if (*count > 6 * previous_rectangle) {
					size_t new_index = *count - previous_rectangle * 6;
					return { buffer[new_index - 5].position.x - buffer[new_index - 6].position.x, buffer[new_index - 3].position.y - buffer[new_index - 1].position.y };
				}
				return float2(0.0f, 0.0f);
			}

			UIDockspace* GetDockspace() {
				return dockspace;
			}

			float GetAlignedToCenterX(float x_scale) const {
				float position = function::PredicateValue(export_scale != nullptr, current_x, region_position.x);
				float _region_scale = function::PredicateValue(export_scale != nullptr, x_scale, region_scale.x);
				return AlignMiddle(position, _region_scale, x_scale);
			}

			float GetAlignedToCenterY(float y_scale) const {
				float position = function::PredicateValue(export_scale != nullptr, current_y, region_position.y);
				float _region_scale = function::PredicateValue(export_scale != nullptr, y_scale, region_scale.y);
				return AlignMiddle(position, _region_scale, y_scale);
			}

			float2 GetAlignedToCenter(float2 scale) const {
				return { GetAlignedToCenterX(scale.x), GetAlignedToCenterY(scale.y) };
			}
			
			float2 GetAlignedToRight(float x_scale, float target_position = -5.0f) const {
				target_position = function::PredicateValue(target_position == -5.0f, region_limit.x, target_position);
				target_position = function::PredicateValue(export_scale != nullptr, current_x + x_scale, target_position);
				return { function::ClampMin(target_position - x_scale, current_x), current_y /*+ region_render_offset.y*/ };
			}

			float2 GetAlignedToRightOverLimit(float x_scale) const {
				float vertical_slider_offset = system->m_windows[window_index].is_vertical_render_slider * system->m_descriptors.misc.render_slider_vertical_size;
				return { function::ClampMin(region_position.x + region_scale.x - x_scale - vertical_slider_offset, current_x), current_y };
			}

			float2 GetAlignedToBottom(float y_scale, float target_position = -5.0f) const {
				target_position = function::PredicateValue(target_position == -5.0f, region_limit.y, target_position);
				target_position = function::PredicateValue(export_scale != nullptr, current_y + y_scale, target_position);
				return { current_x, function::ClampMin(target_position - y_scale, current_y) };
			}

			unsigned int GetBorderIndex() const {
				return border_index;
			}

			DockspaceType GetDockspaceType() const {
				return dockspace_type;
			}
			
			ActionData GetDummyActionData() {
				return system->GetFilledActionData(window_index);
			}

			void* GetMainAllocatorBuffer(size_t size, size_t alignment = 8) {
				void* allocation = system->m_memory->Allocate(size, alignment);
				system->AddWindowMemoryResource(allocation, window_index);
				ECS_ASSERT(allocation != nullptr);
				return allocation;
			}

			template<typename T>
			T* GetMainAllocatorBuffer() {
				T* allocation = (T*)system->m_memory->Allocate(sizeof(T), alignof(T));
				system->AddWindowMemoryResource(allocation, window_index);
				ECS_ASSERT(allocation != nullptr);
				return allocation;
			}

			// name must not conflict with other element names
			template<typename T>
			T* GetMainAllocatorBufferAndStoreAsResource(const char* name) {
				T* resource = GetMainAllocatorBuffer<T>();
				AddWindowResource(name, [resource](const char* identifier) {
					return resource;
				});
				return resource;
			}

			void* GetMainAllocatorBufferAndStoreAsResource(const char* name, size_t size, size_t alignment = 8) {
				void* resource = GetMainAllocatorBuffer(size, alignment);
				AddWindowResource(name, [resource](const char* identifier) {
					return resource;
				});
				return resource;
			}

			unsigned int GetWindowIndex() const {
				return window_index;
			}

#pragma region Hierarchy

			UIDrawerHierarchy* Hierarchy(const char* name) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				return Hierarchy<configuration>(null_config, name);
			}

			template<size_t configuration>
			UIDrawerHierarchy* Hierarchy(const UIDrawConfig& config, const char* name) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerHierarchy* data = (UIDrawerHierarchy*)GetResource(name);

						HierarchyDrawer<configuration>(config, data, scale);
						HandleDynamicResource<configuration>(name);
						return data;
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeHierarchy initialize_data;
							initialize_data.config = (UIDrawConfig*)&config;
							ECS_FORWARD_STRUCT_MEMBERS_1(initialize_data, name);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeHierarchyElement<DynamicConfiguration(configuration)>);
						}
						Hierarchy<DynamicConfiguration(configuration)>(config, name);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						return HierarchyInitializer<configuration>(config, name);
					}
				}
			}

#pragma endregion

#pragma region Histogram

			template<typename Stream>
			void Histogram(Stream samples, const char* name = nullptr) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				Histogram<configuration>(null_config, samples, name);
			}

			template<size_t configuration, typename Stream>
			void Histogram(const UIDrawConfig& config, Stream samples, const char* name = nullptr) {
				if constexpr (!initializer) {
					ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

					HistogramDrawer<configuration>(config, samples, name, position, scale);
				}
			}

#pragma endregion

			template<size_t configuration, size_t no_element_name>
			void InitializeElementName(
				const UIDrawConfig& config, 
				const char* name, 
				UIDrawerTextElement* element,
				float2 position
			) {
				if constexpr (~configuration & no_element_name) {
					ConvertTextToWindowResource<configuration>(config, name, element, position);
				}
			}

			bool HasClicked(float2 position, float2 scale) {
				return IsPointInRectangle(mouse_position, position, scale) && system->m_mouse_tracker->LeftButton() == MBPRESSED;
			}

			bool IsMouseInRectangle(float2 position, float2 scale) {
				return IsPointInRectangle(mouse_position, position, scale);
			}

			void Indent() {
				min_render_bounds.x = function::PredicateValue(min_render_bounds.x > current_x - region_render_offset.x, current_x - region_render_offset.x, min_render_bounds.x);
				current_x += layout.element_indentation + current_column_x_scale;
				current_column_x_scale = 0.0f;
			}

			void Indent(float count) {
				min_render_bounds.x = function::PredicateValue(min_render_bounds.x > current_x - region_render_offset.x, current_x - region_render_offset.x, min_render_bounds.x);
				current_x += count * layout.element_indentation + current_column_x_scale;
				current_column_x_scale = 0.0f;
				//max_render_bounds.x = function::PredicateValue(max_render_bounds.x < current_x - region_render_offset.x, current_x - region_render_offset.x, max_render_bounds.x);
			}

#pragma region Label hierarchy

			void LabelHierarchy(const char* identifier, Stream<const char*> labels) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				LabelHierarchy<configuration>(null_config, identifier, labels);
			}

			// Parent index 0 means root
			template<size_t configuration>
			UIDrawerLabelHierarchy* LabelHierarchy(UIDrawConfig& config, const char* identifier, Stream<const char*> labels) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerLabelHierarchy* data = (UIDrawerLabelHierarchy*)GetResource(identifier);

						LabelHierarchyDrawer<configuration>(config, data, labels, position, scale);
						HandleDynamicResource<configuration>(identifier);
						return data;
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeLabelHierarchy initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_2(initialize_data, identifier, labels);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeLabelHierarchyElement<DynamicConfiguration(configuration)>);
						}
						return LabelHierarchy<DynamicConfiguration(configuration)>(config, identifier, labels);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						return LabelHierarchyInitializer<configuration>(config, identifier);
					}
				}
			}

			template<size_t configuration>
			UIDrawerLabelHierarchy* LabelHierarchyInitializer(const UIDrawConfig& config, const char* _identifier) {
				UIDrawerLabelHierarchy* data = nullptr;

				AddWindowResource(_identifier, [&](const char* identifier) {
					data = GetMainAllocatorBuffer<UIDrawerLabelHierarchy>();
					
					constexpr size_t max_characters = 256;
					data->active_label.InitializeFromBuffer(GetMainAllocatorBuffer(max_characters * sizeof(char), alignof(char)), max_characters);
					data->selected_label_temporary.InitializeFromBuffer(GetMainAllocatorBuffer(max_characters * sizeof(char), alignof(char)), max_characters);
					data->right_click_label_temporary.InitializeFromBuffer(GetMainAllocatorBuffer(max_characters * sizeof(char), alignof(char)), max_characters);

					if constexpr (configuration & UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK) {
						const UIConfigLabelHierarchySelectableCallback* callback = (const UIConfigLabelHierarchySelectableCallback*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK);
						
						void* callback_data = callback->callback_data;
						if (callback->callback_data_size > 0) {
							void* allocation = GetMainAllocatorBuffer(callback->callback_data_size);
							memcpy(allocation, callback->callback_data, callback->callback_data_size);
							callback_data = allocation;
						}
						data->selectable_callback = callback->callback;
						data->selectable_callback_data = callback_data;
					}
					else {
						data->selectable_callback = nullptr;
						data->selectable_callback_data = nullptr;
					}

					if constexpr (configuration & UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK) {
						const UIConfigLabelHierarchyRightClick* callback = (const UIConfigLabelHierarchyRightClick*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK);

						void* callback_data = callback->data;
						if (callback->data_size > 0) {
							void* allocation = GetMainAllocatorBuffer(callback->data_size);
							memcpy(allocation, callback->data, callback->data_size);
							callback_data = allocation;
						}
						data->right_click_callback = callback->callback;
						data->right_click_callback_data = callback_data;
					}
					else {
						data->right_click_callback = nullptr;
						data->right_click_callback_data = nullptr;
					}

					unsigned int hash_table_capacity = 256;
					if constexpr (configuration & UI_CONFIG_LABEL_HIERARCHY_HASH_TABLE_CAPACITY) {
						const UIConfigLabelHierarchyHashTableCapacity* capacity = (const UIConfigLabelHierarchyHashTableCapacity*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_HASH_TABLE_CAPACITY);
						hash_table_capacity = capacity->capacity;
					}
					data->label_states.InitializeFromBuffer(GetMainAllocatorBuffer(data->label_states.MemoryOf(hash_table_capacity)), hash_table_capacity);

					return data;
				});

				return data;
			}

			template<size_t configuration>
			void LabelHierarchyDrawer(
				UIDrawConfig& config, 
				UIDrawerLabelHierarchy* data, 
				Stream<const char*> labels, 
				float2 position, 
				float2 scale
			) {
				struct StackElement {
					unsigned int parent_index;
					float next_row_gain;
				};

				// keep a stack of current parent indices
				StackElement* stack_buffer = (StackElement*)GetTempBuffer(sizeof(StackElement) * labels.size, alignof(StackElement));
				Stack<StackElement> stack(stack_buffer, labels.size);
				bool* label_states = (bool*)GetTempBuffer(sizeof(bool) * labels.size, alignof(bool));

				float2 current_position = position;
				float2 square_scale = GetSquareScale(scale.y);

				// aliases for sprite texture info
				float2 opened_top_left_uv, closed_top_left_uv, opened_bottom_right_uv, closed_bottom_right_uv;
				Color opened_color, closed_color;
				const wchar_t* opened_texture;
				const wchar_t* closed_texture;
				float2 expand_factor;
				float horizontal_bound = position.x + scale.x;
				if (~configuration & UI_CONFIG_LABEL_HIERARCHY_DO_NOT_INFER_X) {
					horizontal_bound = region_limit.x;
				}

				float horizontal_texture_offset = square_scale.x;
				bool keep_triangle = true;
				
				// copy the information into aliases
				if constexpr (configuration & UI_CONFIG_LABEL_HIERARCHY_SPRITE_TEXTURE) {
					const UIConfigLabelHierarchySpriteTexture* texture_info = (const UIConfigLabelHierarchySpriteTexture*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_SPRITE_TEXTURE);
					opened_top_left_uv = texture_info->opened_texture_top_left_uv;
					opened_bottom_right_uv = texture_info->opened_texture_bottom_right_uv;
					closed_top_left_uv = texture_info->closed_texture_top_left_uv;
					closed_bottom_right_uv = texture_info->closed_texture_bottom_right_uv;

					opened_color = texture_info->opened_color;
					closed_color = texture_info->closed_color;

					opened_texture = texture_info->opened_texture;
					closed_texture = texture_info->closed_texture;

					expand_factor = texture_info->expand_factor;
					horizontal_texture_offset *= (1.0f + texture_info->keep_triangle);
					keep_triangle = texture_info->keep_triangle;
				}

				UIDrawPhase selectable_callback_phase = UIDrawPhase::Normal;
				// check selectable update of data
				if constexpr (configuration & UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK) {
					const UIConfigLabelHierarchySelectableCallback* selectable = (const UIConfigLabelHierarchySelectableCallback*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK);
					if (!selectable->copy_on_initialization && selectable->callback_data_size > 0) {
						memcpy(data->selectable_callback_data, selectable->callback_data, selectable->callback_data_size);
					}
					selectable_callback_phase = selectable->phase;
				}

				UIDrawPhase right_click_phase = UIDrawPhase::Normal;
				// check right click update of data
				if constexpr (configuration & UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK) {
					const UIConfigLabelHierarchyRightClick* right_click = (const UIConfigLabelHierarchyRightClick*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK);
					if (!right_click->copy_on_initialization && right_click->data_size > 0) {
						memcpy(data->right_click_callback_data, right_click->data, right_click->data_size);
					}
					right_click_phase = right_click->phase;
				}

				// font size and character spacing are dummies, text color is the one that's needed for
				// drop down triangle color
				float2 font_size;
				float character_spacing;
				Color text_color;
				HandleText<configuration>(config, text_color, font_size, character_spacing);

				Color label_color = HandleColor<configuration>(config);
				UIConfigTextAlignment text_alignment;
				text_alignment.horizontal = TextAlignment::Left;
				text_alignment.vertical = TextAlignment::Middle;
				config.AddFlag(text_alignment);

				IdentifierHashTable<unsigned int, ResourceIdentifier, HashFunctionPowerOfTwo> parent_hash_table;
				size_t table_count = function::PowerOfTwoGreater(labels.size) * 2;
				parent_hash_table.InitializeFromBuffer(GetTempBuffer(parent_hash_table.MemoryOf(table_count)), table_count);

				constexpr size_t label_configuration = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE |
					UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_ADVANCE;
				for (size_t index = 0; index < labels.size; index++) {
					unsigned int current_parent_index = 0;
					Stream<char> label_stream(labels[index], strlen(labels[index]));
					size_t parent_path_size = function::PathParentSize(label_stream);

					if (parent_path_size != 0) {
						ResourceIdentifier identifier(labels[index], parent_path_size);
						unsigned int hash = HashFunctionAdditiveString::Hash(identifier);
						parent_hash_table.TryGetValue(hash, identifier, current_parent_index);
					}

					// check to see if it is inside the hash table; if it is, then 
					// increase the activation count else introduce it
					ResourceIdentifier identifier(labels[index], label_stream.size);
					unsigned int hash = HashFunctionAdditiveString::Hash(identifier);

					ECS_ASSERT(!parent_hash_table.Insert(hash, index + 1, identifier));

					unsigned int table_index = data->label_states.Find(hash, identifier);

					// get the label state for triangle drop down
					bool label_state = false;
					// get a char pointer to a stable reference label character stream; the newly created
					// stream for insertion or the already existing identifier for existing label
					Stream<char> table_char_stream;

					const ResourceIdentifier* table_identifiers = data->label_states.GetIdentifiers();

					if (table_index == -1) {
						void* allocation = GetMainAllocatorBuffer((label_stream.size + 1) * sizeof(char), alignof(char));
						memcpy(allocation, labels[index], (label_stream.size + 1) * sizeof(char));
						identifier.ptr = (const wchar_t*)allocation;
						table_char_stream.buffer = (char*)allocation;
						table_char_stream.size = label_stream.size;

						UIDrawerLabelHierarchyLabelData current_data;
						current_data.activation_count = 5;
						current_data.state = false;
						label_states[index] = false;
						ECS_ASSERT(!data->label_states.Insert(hash, current_data, identifier));
					}
					else {
						UIDrawerLabelHierarchyLabelData* current_data = data->label_states.GetValuePtrFromIndex(table_index);
						current_data->activation_count++;
						label_state = current_data->state;
						table_char_stream.buffer = (char*)table_identifiers[table_index].ptr;
						table_char_stream.size = table_identifiers[table_index].size;
						if (current_parent_index == 0) {
							label_states[index] = current_data->state;
						}
						else {
							label_states[index] = current_data->state && label_states[current_parent_index - 1];
						}
					}

					StackElement last_element;
					float current_gain = 0.0f;
					if (stack.GetElementCount() == 0) {
						stack.Push({ current_parent_index, 0.0f });
					}
					else {
						stack.Peek(last_element);
						if (last_element.parent_index < current_parent_index) {
							stack.Push({ current_parent_index, last_element.next_row_gain + layout.element_indentation * 2.0f });
							current_gain = last_element.next_row_gain + layout.element_indentation * 2.0f;
						}
						else if (last_element.parent_index > current_parent_index){
							while (stack.GetElementCount() > 0 && last_element.parent_index > current_parent_index) {
								stack.Pop(last_element);
							}
							stack.Push(last_element);
							current_gain = last_element.next_row_gain;
						}
						else {
							current_gain = last_element.next_row_gain;
						}
					} 

					if (current_parent_index == 0 || label_states[current_parent_index - 1]) {
						bool has_children = false;

						if (index < labels.size - 1) {
							unsigned int next_parent_index = 0;
							Stream<char> next_label_stream(labels[index + 1], strlen(labels[index + 1]));
							size_t next_parent_path_size = function::PathParentSize(next_label_stream);

							if (next_parent_path_size != 0) {
								ResourceIdentifier next_identifier(next_label_stream.buffer, next_parent_path_size);
								unsigned int next_hash = HashFunctionAdditiveString::Hash(next_identifier);
								parent_hash_table.TryGetValue(next_hash, next_identifier, next_parent_index);
							}
							has_children = index < next_parent_index;
						}

						current_position = { current_x + current_gain - region_render_offset.x, current_y - region_render_offset.y };
						float2 initial_label_position = current_position;
						scale.x = horizontal_bound - current_position.x - horizontal_texture_offset;

						if (ValidatePosition<configuration>(current_position, { horizontal_bound - current_position.x, scale.y })) {						
							Color current_color = label_color;

							float2 label_position = { current_position.x + horizontal_texture_offset, current_position.y };
							bool is_active = false;
							float active_label_scale = 0.0f;
							if (label_stream.size == data->active_label.size) {
								if (memcmp(label_stream.buffer, data->active_label.buffer, label_stream.size) == 0) {
									current_color = ToneColor(label_color, 1.25f);
									is_active = true;
									active_label_scale = GetLabelScale(labels[index] + parent_path_size + (parent_path_size != 0)).x;
								}
							}
							TextLabel<configuration | label_configuration | UI_CONFIG_LABEL_TRANSPARENT>(
								config, 
								labels[index] + parent_path_size + (parent_path_size != 0), 
								label_position, 
								scale
							);

							float2 action_scale = { horizontal_bound - initial_label_position.x, scale.y };
							if (IsMouseInRectangle(initial_label_position, action_scale) && system->m_mouse_tracker->LeftButton() == MBRELEASED) {
								data->selected_label_temporary.Copy(label_stream);
							}

							AddClickable(initial_label_position, action_scale, { LabelHierarchySelectable, data, 0, selectable_callback_phase });
							AddDefaultHoverable(initial_label_position, action_scale, current_color);

							UIDrawerLabelHierarchyChangeStateData change_state_data;
							change_state_data.hierarchy = data;
							change_state_data.label = table_char_stream;
							if (keep_triangle && has_children) {
								if (label_state) {
									SpriteRectangle<configuration>(current_position, square_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, text_color);
								}
								else {
									SpriteRectangle<configuration>(current_position, square_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, text_color, { 1.0f, 0.0f }, { 0.0f, 1.0f });
								}
								AddClickable(current_position, square_scale, { LabelHierarchyChangeState, &change_state_data, sizeof(change_state_data), selectable_callback_phase });
							}
							current_position.x += square_scale.x;
							if constexpr (configuration & UI_CONFIG_LABEL_HIERARCHY_SPRITE_TEXTURE) {
								if (label_state) {
									SpriteRectangle<configuration>(current_position, square_scale, opened_texture, opened_color, opened_top_left_uv, opened_bottom_right_uv);
								}
								else {
									SpriteRectangle<configuration>(current_position, square_scale, closed_texture, closed_color, closed_top_left_uv, closed_bottom_right_uv);
								}
								if (!keep_triangle && has_children) {
									AddClickable(current_position, square_scale, { LabelHierarchyChangeState, &change_state_data, sizeof(change_state_data), selectable_callback_phase });
								}
							}

							if constexpr (configuration & UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK) {
								bool trigger = IsMouseInRectangle(initial_label_position, action_scale) && system->m_mouse_tracker->RightButton() == MBRELEASED;
								if (trigger) {
									data->right_click_label_temporary.Copy(label_stream);
									data->selected_label_temporary.Copy(label_stream);
								}

								UIDrawerLabelHierarchyRightClickData right_click;
								right_click.data = data->right_click_callback_data;
								right_click.label = data->right_click_label_temporary;
								AddHoverable(initial_label_position, action_scale, { data->right_click_callback, &right_click, sizeof(right_click), right_click_phase });
								
								if (trigger) {
									data->active_label.Copy(data->selected_label_temporary);
									data->active_label[data->active_label.size] = '\0';

									if (data->selectable_callback != nullptr) {
										ActionData action_data = GetDummyActionData();
										action_data.buffers = buffers;
										action_data.counts = counts;
										action_data.mouse_position = mouse_position;
										action_data.position = initial_label_position;
										action_data.scale = action_scale;
										action_data.additional_data = data->active_label.buffer;
										action_data.data = data->selectable_callback_data;
										data->selectable_callback(&action_data);
									}
								}
							}

							if (is_active) {
								float horizontal_scale = horizontal_bound - initial_label_position.x;
								horizontal_scale = function::ClampMin(horizontal_scale, active_label_scale + square_scale.x * 2.0f);
								SolidColorRectangle<configuration>(initial_label_position, { horizontal_scale, scale.y }, current_color);
							}

						}

						FinalizeRectangle<configuration>(initial_label_position, { horizontal_bound - initial_label_position.x, scale.y });
						NextRow();
					}
				}

				const ResourceIdentifier* label_state_identifiers = data->label_states.GetIdentifiers();
				Stream<UIDrawerLabelHierarchyLabelData> table_values = data->label_states.GetValueStream();
				for (int64_t index = 0; index < table_values.size; index++) {
					if (data->label_states.IsItemAt(index)) {
						table_values[index].activation_count--;
						if (table_values[index].activation_count == 0) {
							system->RemoveWindowMemoryResource(window_index, label_state_identifiers[index].ptr);
							system->m_memory->Deallocate(label_state_identifiers[index].ptr);
							data->label_states.EraseFromIndex(index);
							index--;
						}
					}
				}

				config.flag_count--;
				OffsetNextRow(-next_row_offset);
			}

#pragma endregion

#pragma region List

			UIDrawerList* List(const char* name) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				return List<configuration>(null_config, name);
			}

			template<size_t configuration>
			UIDrawerList* List(UIDrawConfig& config, const char* name) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerList* list = (UIDrawerList*)GetResource(name);

						ListDrawer<configuration>(config, list, position, scale);
						HandleDynamicResource<configuration>(name);
						return list;
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeList initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_1(initialize_data, name);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeListElement<DynamicConfiguration(configuration)>);
						}
						return List<DynamicConfiguration(configuration)>(config, name);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						return ListInitializer<configuration>(config, name);
					}
				}
			}

			void ListFinalizeNode(UIDrawerList* list) {
				list->FinalizeNodeYscale((const void**)buffers, counts);
			}

#pragma endregion

#pragma region Label List

			void LabelList(const char* name, Stream<const char*> labels) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				LabelList<configuration>(null_config, name, labels);
			}

			template<size_t configuration>
			void LabelList(const UIDrawConfig& config, const char* name, Stream<const char*> labels) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerLabelList* data = (UIDrawerLabelList*)GetResource(name);

						LabelListDrawer<configuration>(config, data, position, scale);
					}
					else {
						LabelListDrawer<configuration>(config, name, labels, position, scale);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						LabelListInitializer<configuration>(config, name, labels);
					}
				}
			}

			template<size_t configuration>
			void LabelListDrawer(const UIDrawConfig& config, UIDrawerLabelList* data, float2 position, float2 scale) {
				if constexpr (~configuration & UI_CONFIG_LABEL_LIST_NO_NAME) {
					ElementName<configuration>(config, &data->name, position, scale);
					NextRow();
				}

				Color font_color;
				float2 font_size;
				float character_spacing;
				HandleText<configuration>(config, font_color, font_size, character_spacing);

				float font_scale = system->GetTextSpriteYScale(font_size.y);
				float label_scale = font_scale + element_descriptor.label_vertical_padd * 2.0f;
				float2 square_scale = GetSquareScale(element_descriptor.label_list_circle_size);
				for (size_t index = 0; index < data->labels.size; index++) {
					position = GetCurrentPosition();
					float2 circle_position = { position.x, AlignMiddle(position.y, label_scale, square_scale.y) };
					SpriteRectangle<configuration>(circle_position, square_scale, ECS_TOOLS_UI_TEXTURE_FILLED_CIRCLE, font_color);
					position.x += square_scale.x + layout.element_indentation;
					TextLabelDrawer<configuration | UI_CONFIG_LABEL_TRANSPARENT>(
						config,
						data->labels.buffer + index, 
						position, 
						scale
					);
					NextRow();
				}
			}

			template<size_t configuration>
			void LabelListDrawer(const UIDrawConfig& config, const char* name, Stream<const char*> labels, float2 position, float2 scale) {
				if constexpr (~configuration & UI_CONFIG_LABEL_LIST_NO_NAME) {
					ElementName<configuration>(config, name, position, scale);
					NextRow();
				}

				Color font_color;
				float2 font_size;
				float character_spacing;
				HandleText<configuration>(config, font_color, font_size, character_spacing);

				float font_scale = system->GetTextSpriteYScale(font_size.y);
				float label_scale = font_scale + element_descriptor.label_vertical_padd * 2.0f;
				float2 square_scale = GetSquareScale(element_descriptor.label_list_circle_size);
				for (size_t index = 0; index < labels.size; index++) {
					position = GetCurrentPosition();
					float2 circle_position = { position.x, AlignMiddle(position.y, label_scale, square_scale.y) };
					SpriteRectangle<configuration>(circle_position, square_scale, ECS_TOOLS_UI_TEXTURE_FILLED_CIRCLE, font_color);
					UpdateCurrentColumnScale(square_scale.x);
					Indent();
					TextLabel<configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT>(
						config,
						labels.buffer[index]
					);
					NextRow();
				}
			}

			template<size_t configuration>
			UIDrawerLabelList* LabelListInitializer(const UIDrawConfig& config, const char* name, Stream<const char*> labels) {
				UIDrawerLabelList* data = nullptr;

				AddWindowResource(name, [&](const char* identifier) {
					data = GetMainAllocatorBuffer<UIDrawerLabelList>();
					data->labels.buffer = (UIDrawerTextElement*)GetMainAllocatorBuffer(sizeof(UIDrawerTextElement) * labels.size);

					InitializeElementName<configuration, UI_CONFIG_LABEL_LIST_NO_NAME>(config, name, &data->name, { 0.0f, 0.0f });
					for (size_t index = 0; index < labels.size; index++) {
						ConvertTextToWindowResource<configuration>(config, labels[index], data->labels.buffer + index, { 0.0f, 0.0f });
					}

					return data;
				});

				return data;
			}

#pragma endregion

#pragma region Menu

			// State can be stack allocated; name should be unique if drawing with a sprite
			void Menu(const char* name, UIDrawerMenuState* state) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				Menu<configuration>(null_config, name, state);
			}

			// State can be stack allocated; name should be unique if drawing with a sprite
			template<size_t configuration>
			void Menu(const UIDrawConfig& config, const char* name, UIDrawerMenuState* state) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerMenu* data = (UIDrawerMenu*)GetResource(name);

						MenuDrawer<configuration>(config, data, position, scale);
						HandleDynamicResource<configuration>(name);
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeMenu initialize_data;
							initialize_data.config = (UIDrawConfig*)&config;
							ECS_FORWARD_STRUCT_MEMBERS_2(initialize_data, name, state);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeMenuElement<DynamicConfiguration(configuration)>);
						}
						Menu<DynamicConfiguration(configuration)>(config, name, state);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						MenuInitializer<configuration>(config, name, scale, state);
					}
				}
			}

			void* MenuAllocateStateHandlers(Stream<UIActionHandler> handlers) {
				void* allocation = GetMainAllocatorBuffer(sizeof(UIActionHandler) * handlers.size);
				memcpy(allocation, handlers.buffer, sizeof(UIActionHandler) * handlers.size);
				return allocation;
			}

#pragma endregion

#pragma region Number input

			void FloatInput(const char* name, float* value, float default_value = 0.0f, float lower_bound = FLT_MIN, float upper_bound = FLT_MAX) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				FloatInput<configuration>(null_config, name, value, default_value, lower_bound, upper_bound);
			}

			template<size_t configuration>
			void FloatInput(UIDrawConfig& config, const char* name, float* value, float default_value = 0.0f, float lower_bound = FLT_MIN, float upper_bound = FLT_MAX) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						FloatInputDrawer<configuration>(config, name, value, default_value, lower_bound, upper_bound, position, scale);
						HandleDynamicResource<configuration>(name);
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeFloatInput initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_5(initialize_data, default_value, lower_bound, name, upper_bound, value);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeFloatInputElement<DynamicConfiguration(configuration)>);
						}
						FloatInput<DynamicConfiguration(configuration)>(config, name, value, default_value, lower_bound, upper_bound);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						FloatInputInitializer<configuration>(config, name, value, lower_bound, upper_bound, position, scale);
					}
				}
			}

			void FloatInputGroup(
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				float** ECS_RESTRICT values,
				const float* ECS_RESTRICT default_values,
				const float* ECS_RESTRICT lower_bound,
				const float* ECS_RESTRICT upper_bound
			) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				FloatInputGroup<configuration>(null_config, count, group_name, names, values, default_values, lower_bound, upper_bound);
			}

			template<size_t configuration>
			void FloatInputGroup(
				UIDrawConfig& config,
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				float** ECS_RESTRICT values,
				const float* ECS_RESTRICT default_values = nullptr,
				const float* ECS_RESTRICT lower_bound = nullptr,
				const float* ECS_RESTRICT upper_bound = nullptr
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						FloatInputGroupDrawer<configuration>(config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
						HandleDynamicResource<configuration>(group_name);
					}
					else {
						bool exists = ExistsResource(group_name);
						if (!exists) {
							UIDrawerInitializeFloatInputGroup initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_7(initialize_data, count, default_values, group_name, lower_bound, names, upper_bound, values);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, group_name, InitializeFloatInputGroupElement<DynamicConfiguration(configuration)>);
						}
						FloatInputGroup<DynamicConfiguration(configuration)>(config, group_name, count, names, values, default_values, lower_bound, upper_bound);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						FloatInputGroupInitializer<configuration>(config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
					}
				}
			}

			void DoubleInput(const char* name, double* value, double default_value = 0, double lower_bound = DBL_MIN, double upper_bound = DBL_MAX) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				DoubleInput<configuration>(null_config, name, value, default_value, lower_bound, upper_bound);
			}

			template<size_t configuration>
			void DoubleInput(UIDrawConfig& config, const char* name, double* value, double default_value = 0, double lower_bound = DBL_MIN, double upper_bound = DBL_MAX) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						DoubleInputDrawer<configuration>(config, name, value, default_value, lower_bound, upper_bound, position, scale);
						HandleDynamicResource<configuration>(name);
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeDoubleInput initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_5(initialize_data, default_value, lower_bound, name, upper_bound, value);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeDoubleInputElement<DynamicConfiguration(configuration)>);
						}
						DoubleInput<DynamicConfiguration(configuration)>(config, name, value, default_value, lower_bound, upper_bound);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						DoubleInputInitializer<configuration>(config, name, value, lower_bound, upper_bound, position, scale);
					}
				}
			}

			void DoubleInputGroup(
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				double** ECS_RESTRICT values,
				const double* ECS_RESTRICT default_values = nullptr,
				const double* ECS_RESTRICT lower_bound = nullptr,
				const double* ECS_RESTRICT upper_bound = nullptr
			) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				DoubleInputGroup<configuration>(null_config, count, group_name, names, values, default_values, lower_bound, upper_bound);
			}

			template<size_t configuration>
			void DoubleInputGroup(
				UIDrawConfig& config,
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				double** ECS_RESTRICT values,
				const double* ECS_RESTRICT default_values = nullptr,
				const double* ECS_RESTRICT lower_bound = nullptr,
				const double* ECS_RESTRICT upper_bound = nullptr
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						DoubleInputGroupDrawer<configuration>(config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
						HandleDynamicResource<configuration>(group_name);
					}
					else {
						bool exists = ExistsResource(group_name);
						if (!exists) {
							UIDrawerInitializeDoubleInputGroup initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_7(initialize_data, group_name, count, names, values, default_values, lower_bound, upper_bound);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, group_name, InitializeDoubleInputGroupElement<DynamicConfiguration(configuration)>);
						}
						DoubleInputGroup<DynamicConfiguration(configuration)>(config, count, group_name, names, values, default_values, lower_bound, upper_bound);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						DoubleInputGroupInitializer<configuration>(config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
					}
				}
			}

			template<typename Integer>
			void IntInput(const char* name, Integer* value, Integer default_value = 0, Integer min = LLONG_MIN, Integer max = ULLONG_MAX) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				IntInput<configuration>(null_config, name, value, default_value, min, max);
			}

			template<size_t configuration, typename Integer>
			void IntInput(UIDrawConfig& config, const char* name, Integer* value, Integer default_value = 0, Integer min = LLONG_MIN, Integer max = ULLONG_MAX) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				/*if constexpr (std::is_unsigned_v<Integer>) {
					min = 0;
				}*/

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						IntInputDrawer<configuration, Integer>(config, name, value, default_value, min, max, position, scale);
						HandleDynamicResource<configuration>(name);
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeIntegerInput<Integer> initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_5(initialize_data, name, value, default_value, min, max);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeIntegerInputElement<DynamicConfiguration(configuration), Integer>);
						}
						IntInput<DynamicConfiguration(configuration)>(config, name, value, default_value, min, max);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						IntInputInitializer<configuration, Integer>(config, name, value, min, max, position, scale);
					}
				}
			}

			template<typename Integer>
			void IntInputGroup(
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				Integer** ECS_RESTRICT values,
				const Integer* ECS_RESTRICT default_values,
				const Integer* ECS_RESTRICT lower_bound,
				const Integer* ECS_RESTRICT upper_bound
			) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				IntInputGroup<configuration>(null_config, count, group_name, names, values, default_values, lower_bound, upper_bound);
			}

			template<size_t configuration, typename Integer>
			void IntInputGroup(
				UIDrawConfig& config,
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				Integer** ECS_RESTRICT values,
				const Integer* ECS_RESTRICT default_values,
				const Integer* ECS_RESTRICT lower_bound,
				const Integer* ECS_RESTRICT upper_bound
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						IntInputGroupDrawer<configuration, Integer>(config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
						HandleDynamicResource<configuration>(group_name);
					}
					else {
						bool exists = ExistsResource(group_name);
						if (!exists) {
							UIDrawerInitializeIntegerInputGroup<Integer> initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_7(initialize_data, count, group_name, names, values, default_values, lower_bound, upper_bound);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, group_name, InitializeIntegerInputGroupElement<DynamicConfiguration(configuration), Integer>);
						}
						IntInput<DynamicConfiguration(configuration)>(config, count, group_name, names, values, default_values, lower_bound, upper_bound);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						IntInputGroupInitializer<configuration, Integer>(config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
					}
				}
			}

#pragma endregion

			void NextRow() {
				min_render_bounds.y = function::PredicateValue(min_render_bounds.y > current_y - region_render_offset.y, current_y - region_render_offset.y, min_render_bounds.y);
				current_y += layout.next_row_y_offset + current_row_y_scale;
				current_x = GetNextRowXPosition();
				current_row_y_scale = 0.0f;
				current_column_x_scale = 0.0f;
				draw_mode_extra_float.y = current_y/* - region_render_offset.y*/;
			}

			void NextRow(float count) {
				min_render_bounds.y = function::PredicateValue(min_render_bounds.y > current_y - region_render_offset.y, current_y - region_render_offset.y, min_render_bounds.y);
				current_y += count * layout.next_row_y_offset + current_row_y_scale;
				current_x = GetNextRowXPosition();
				current_row_y_scale = 0.0f;
				current_column_x_scale = 0.0f;
				draw_mode_extra_float.x = current_y;
			}

			float NormalizeHorizontalToWindowDimensions(float value) const {
				return system->NormalizeHorizontalToWindowDimensions(value);
			}

			void OffsetNextRow(float value) {
				next_row_offset += value;
			}

			void OffsetX(float value) {
				current_x += value;
				current_column_x_scale -= value;
			}

			void OffsetY(float value) {
				current_y += value;
				current_row_y_scale -= value;
			}


#pragma region Progress bar

			void ProgressBar(const char* name, float percentage, float x_scale_factor = 1) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				ProgressBar<configuration>(null_config, name, percentage, x_scale_factor);
			}

			template<size_t configuration>
			void ProgressBar(UIDrawConfig& config, const char* name, float percentage, float x_scale_factor = 1) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
				scale.x *= x_scale_factor;

				if constexpr (!initializer) {
					HandleFitSpaceRectangle<configuration>(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					if (ValidatePosition<configuration>(position, scale)) {
						Color bar_color;
						if constexpr (configuration & UI_CONFIG_COLOR) {
							const UIConfigColor* color_ptr = (const UIConfigColor*)config.GetParameter(UI_CONFIG_COLOR);
							bar_color = *color_ptr;
						}
						else {
							bar_color = color_theme.progress_bar;
						}

						UIConfigBorder border;
						border.color = color_theme.theme;
						border.is_interior = false;
						border.thickness = system->m_descriptors.dockspaces.border_size * 2;
						if constexpr (~configuration & UI_CONFIG_BORDER) {
							config.AddFlag(border);
						}

						HandleBorder<configuration | UI_CONFIG_BORDER>(config, position, scale);
						SolidColorRectangle<configuration>(position, { scale.x * percentage, scale.y }, bar_color);

						if constexpr (~configuration & UI_CONFIG_BORDER) {
							config.flag_count--;
						}

						float percent_value = percentage * 100.0f;
						char temp_values[16];
						Stream<char> temp_stream = Stream<char>(temp_values, 0);
						function::ConvertFloatToChars(temp_stream, percent_value, 0);
						temp_stream.Add('%');
						temp_stream[temp_stream.size] = '\0';
						auto text_stream = GetTextStream<configuration>(temp_stream.size * 6);
						float2 text_position = { position.x + scale.x * percentage, position.y };

						Color text_color;
						float character_spacing;
						float2 font_size;
						HandleText<configuration>(config, text_color, font_size, character_spacing);
						system->ConvertCharactersToTextSprites(
							temp_values,
							text_position,
							text_stream.buffer,
							temp_stream.size,
							text_color,
							0,
							font_size,
							character_spacing
						);
						auto text_count = HandleTextSpriteCount<configuration>();
						*text_count += text_stream.size;
						float2 text_span = GetTextSpan(text_stream);

						float x_position, y_position;
						HandleTextLabelAlignment(
							text_span,
							{ scale.x * (1.0f - percentage), scale.y },
							text_position,
							x_position,
							y_position,
							TextAlignment::Left,
							TextAlignment::Middle
						);
						if (x_position + text_span.x > position.x + scale.x - element_descriptor.label_horizontal_padd) {
							x_position = position.x + scale.x - element_descriptor.label_horizontal_padd - text_span.x;
						}
						TranslateText(x_position, y_position, text_stream, 0, 0);

						HandleRectangleActions<configuration>(config, position, scale);
					}

					UIConfigTextAlignment alignment;
					alignment.horizontal = TextAlignment::Left;
					alignment.vertical = TextAlignment::Middle;
					UIConfigTextAlignment previous_alignment;
					if constexpr (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
						config.SetExistingFlag(alignment, UI_CONFIG_TEXT_ALIGNMENT, previous_alignment);
					}
					else {
						config.AddFlag(alignment);
					}

					FinalizeRectangle<configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW>(position, scale);
					position.x += scale.x + layout.element_indentation * 0.5f;
					TextLabel<configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_TRANSPARENT> (config, name, position, scale);

					if constexpr (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
						config.SetExistingFlag(previous_alignment, UI_CONFIG_TEXT_ALIGNMENT, alignment);
					}
					else {
						config.flag_count--;
					}
				}
			}

#pragma endregion

			void PopIdentifierStack() {
				identifier_stack.size--;
				current_identifier.size -= identifier_stack[identifier_stack.size];
			}

			void PushIdentifierStack(const char* identifier) {
				size_t count = strlen(identifier);

				ECS_ASSERT(count + current_identifier.size < system->m_descriptors.misc.drawer_identifier_memory);
				ECS_ASSERT(identifier_stack.size < system->m_descriptors.misc.drawer_temp_memory);

				memcpy(current_identifier.buffer + current_identifier.size, identifier, count);
				identifier_stack.Add(count);
				current_identifier.size += count;
				current_identifier[current_identifier.size] = '\0';
			}

			void PushIdentifierStack(size_t index) {
				char temp_chars[64];
				Stream<char> stream_char = Stream<char>(temp_chars, 0);
				function::ConvertIntToCharsFormatted(stream_char, (int64_t)index);
				PushIdentifierStack(temp_chars);
			}

			void PushIdentifierStackRandom(size_t index) {
				index = GetRandomIntIdentifier(index);
				PushIdentifierStack(index);
			}

			void RemoveAllocation(const void* allocation) {
				system->m_memory->Deallocate(allocation);
				system->RemoveWindowMemoryResource(window_index, allocation);
			}

			template<size_t configuration>
			void RestoreBufferState(UIDrawerBufferState state) {
				if constexpr (!initializer) {
					size_t* solid_color_count = HandleSolidColorCount<configuration>();
					size_t* text_sprite_count = HandleTextSpriteCount<configuration>();
					size_t* sprite_count = HandleSpriteCount<configuration>();
					size_t* sprite_cluster_count = HandleSpriteClusterCount<configuration>();

					size_t sprite_difference = *sprite_count - state.sprite_count;
					size_t sprite_cluster_difference = *sprite_cluster_count - state.sprite_cluster_count;

					sprite_difference /= 6;
					sprite_cluster_difference /= 6;

					UIDrawPhase phase = HandlePhase<configuration>();
					for (size_t index = 0; index < sprite_difference; index++) {
						system->RemoveSpriteTexture(dockspace, border_index, phase);
					}
					size_t sprite_cluster_current_count = 0;
					int64_t current_cluster_index = (int64_t)dockspace->borders[border_index].draw_resources.sprite_cluster_subtreams.size - 1;
					while (sprite_cluster_current_count < sprite_cluster_difference && current_cluster_index >= 0) {
						sprite_cluster_current_count += dockspace->borders[border_index].draw_resources.sprite_cluster_subtreams[current_cluster_index];
						current_cluster_index--;
					}

					for (size_t index = current_cluster_index; index < dockspace->borders[border_index].draw_resources.sprite_cluster_subtreams.size; index++) {
						system->RemoveSpriteTexture(dockspace, border_index, phase, UISpriteType::Cluster);
					}
					dockspace->borders[border_index].draw_resources.sprite_cluster_subtreams.size = current_cluster_index;

					*solid_color_count = state.solid_color_count;
					*text_sprite_count = state.text_sprite_count;
					*sprite_count = state.sprite_count;
					*sprite_cluster_count = state.sprite_cluster_count;
				}
			}

			void RestoreHandlerState(UIDrawerHandlerState state) {
				if constexpr (!initializer) {
					dockspace->borders[border_index].hoverable_handler.position_x.size = state.hoverable_count;
					dockspace->borders[border_index].clickable_handler.position_x.size = state.clickable_count;
					dockspace->borders[border_index].general_handler.position_x.size = state.general_count;
				}
			}

#pragma region Sentence

			void Sentence(const char* text) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				Sentence<configuration>(null_config, text);
			}

			template<size_t configuration>
			void Sentence(const UIDrawConfig& config, const char* text) {
				if constexpr (!initializer) {
					ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

					void* resource = nullptr;

					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						resource = GetResource(text);
					}
					SentenceDrawer<configuration>(config, HandleResourceIdentifier(text), resource, position);
				}
				else {
					SentenceInitializer<configuration>(config, text);
				}
			}

#pragma endregion

			void SetCurrentX(float value) {
				current_x = value;
			}

			void SetCurrentY(float value) {
				current_y = value;
			}

			template<bool reduce_values = true>
			void SetLayoutFromZoomXFactor(float zoom_factor) {
				if constexpr (reduce_values) {
					float multiply_factor = zoom_factor * zoom_inverse.x;
					layout.default_element_x *= multiply_factor;
					layout.element_indentation *= multiply_factor;
					layout.next_row_padding *= multiply_factor;

					element_descriptor.label_horizontal_padd *= multiply_factor;
					element_descriptor.slider_length.x *= multiply_factor;
					element_descriptor.slider_padding.x *= multiply_factor;
					element_descriptor.slider_shrink.x *= multiply_factor;
					element_descriptor.text_input_padding.x *= multiply_factor;

					font.size *= multiply_factor;
					font.character_spacing *= multiply_factor;
				}
				else {
					layout.default_element_x *= zoom_factor;
					layout.element_indentation *= zoom_factor;
					layout.next_row_padding *= zoom_factor;

					element_descriptor.label_horizontal_padd *= zoom_factor;
					element_descriptor.slider_length.x *= zoom_factor;
					element_descriptor.slider_padding.x *= zoom_factor;
					element_descriptor.slider_shrink.x *= zoom_factor;
					element_descriptor.text_input_padding.x *= zoom_factor;

					font.size *= zoom_factor;
					font.character_spacing *= zoom_factor;
				}
			}

			template<bool reduce_values = true>
			void SetLayoutFromZoomYFactor(float zoom_factor) {
				if constexpr (reduce_values) {
					float multiply_factor = zoom_factor * zoom_inverse.y;
					layout.default_element_y *= multiply_factor;
					layout.next_row_y_offset *= multiply_factor;

					element_descriptor.label_vertical_padd *= multiply_factor;
					element_descriptor.slider_length.y *= multiply_factor;
					element_descriptor.slider_padding.y *= multiply_factor;
					element_descriptor.slider_shrink.y *= multiply_factor;
					element_descriptor.text_input_padding.y *= multiply_factor;
				}
				else {
					layout.default_element_y *= zoom_factor;
					layout.next_row_y_offset *= zoom_factor;

					element_descriptor.label_vertical_padd *= zoom_factor;
					element_descriptor.slider_length.y *= zoom_factor;
					element_descriptor.slider_padding.y *= zoom_factor;
					element_descriptor.slider_shrink.y *= zoom_factor;
					element_descriptor.text_input_padding.y *= zoom_factor;
				}
			}


#pragma region Rectangle

			template<size_t configuration, typename UIDrawConfig>
			void Rectangle(const UIDrawConfig& config) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				Rectangle<configuration>(config, position, scale);
			}

			template<size_t configuration>
			void Rectangles(const UIDrawConfig& overall_config, const UIDrawSmallConfig* configs, const float2* offsets, size_t count) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, overall_config);

				float2 initial_position = position;
				for (size_t index = 0; index < count; index++) {
					if constexpr (configuration & UI_CONFIG_RECTANGLES_INDIVIDUAL_ACTIONS) {
						Rectangle<configuration | UI_CONFIG_DO_NOT_ADVANCE>(configs[index], position, scale);
					}
					else {
						Rectangle<(configuration | UI_CONFIG_DO_NOT_ADVANCE) & ~(UI_CONFIG_HOVERABLE_ACTION | UI_CONFIG_CLICKABLE_ACTION | UI_CONFIG_GENERAL_ACTION)>(configs[index], position, scale);
					}
					position.x += offsets[index].x;
					position.y += offsets[index].y;
				}

				float2 total_scale = { position.x - initial_position.x, position.y - initial_position.y };
				if constexpr (~configuration & UI_CONFIG_RECTANGLES_INDIVIDUAL_ACTIONS) {
					if constexpr (configuration & UI_CONFIG_HOVERABLE_ACTION) {
						const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_HOVERABLE_ACTION);
						AddHoverable(initial_position, total_scale, *handler);
					}
					if constexpr (configuration & UI_CONFIG_CLICKABLE_ACTION) {
						const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_HOVERABLE_ACTION);
						AddClickable(initial_position, total_scale, *handler);
					}
					if constexpr (configuration & UI_CONFIG_GENERAL_ACTION) {
						const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_HOVERABLE_ACTION);
						AddGeneral(initial_position, total_scale, *handler);
					}
				}

				FinalizeRectangle<configuration>(initial_position, total_scale);
			}

			template<size_t configuration>
			void Rectangles(const UIDrawConfig& overall_config, const UIDrawSmallConfig* configs, float2 offset, size_t count) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, overall_config);

				float2 initial_position = position;
				for (size_t index = 0; index < count; index++) {
					if constexpr (configuration & UI_CONFIG_RECTANGLES_INDIVIDUAL_ACTIONS) {
						Rectangle<configuration | UI_CONFIG_DO_NOT_ADVANCE>(configs[index], position, scale);
					}
					else {
						Rectangle<(configuration | UI_CONFIG_DO_NOT_ADVANCE) & ~(UI_CONFIG_HOVERABLE_ACTION | UI_CONFIG_CLICKABLE_ACTION | UI_CONFIG_GENERAL_ACTION)>(configs[index], position, scale);
					}
					position.x += offset.x;
					position.y += offset.y;
				}

				float2 total_scale = { position.x - initial_position.x, position.y - initial_position.y };
				if constexpr (~configuration & UI_CONFIG_RECTANGLES_INDIVIDUAL_ACTIONS) {
					if constexpr (configuration & UI_CONFIG_HOVERABLE_ACTION) {
						const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_HOVERABLE_ACTION);
						AddHoverable(initial_position, total_scale, *handler);
					}
					if constexpr (configuration & UI_CONFIG_CLICKABLE_ACTION) {
						const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_CLICKABLE_ACTION);
						AddClickable(initial_position, total_scale, *handler);
					}
					if constexpr (configuration & UI_CONFIG_GENERAL_ACTION) {
						const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_GENERAL_ACTION);
						AddGeneral(initial_position, total_scale, *handler);
					}
				}

				FinalizeRectangle<configuration>(initial_position, total_scale);
			}

#pragma endregion

#pragma region Sliders

			// Value must implement Interpolate, Percentage and ToString in order to be used
			template<typename EnterValuesFilter = TextFilterAll, typename Value>
			void Slider(const char* label, Value value_to_modify, Value lower_bound, Value upper_bound, Value default_value = Value()) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				Slider<configuration, EnterValuesFilter, Value>(null_config, label, value_to_modify, lower_bound, upper_bound, default_value);
			}

			// Value must implement Interpolate, Percentage and ToString in order to be used;
			// If values can be entered, then Validate must be implemented
			template<size_t configuration, typename EnterValuesFilter = TextFilterAll, typename Value>
			void Slider(UIDrawConfig& config, const char* name, Value value_to_modify, Value lower_bound, Value upper_bound, Value default_value = Value()) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						void* _info = GetResource(name);
						UIDrawerSlider* info = (UIDrawerSlider*)_info;

						SliderDrawer<configuration, EnterValuesFilter, Value>(config, info, position, scale, value_to_modify, lower_bound, upper_bound, default_value);
						HandleDynamicResource<configuration>(name);
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeSlider<Value> initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_5(initialize_data, name, value_to_modify, lower_bound, upper_bound, default_value);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeSliderElement<DynamicConfiguration(configuration), Value>);
						}
						Slider<DynamicConfiguration(configuration), EnterValuesFilter, Value>(config, info, value_to_modify, lower_bound, upper_bound, default_value);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						SliderInitializer<configuration>(config, name, position, scale, value_to_modify, lower_bound, upper_bound, default_value);
					}
				}
			}

			template<typename EnterValuesFilter = TextFilterAll, typename Value>
			void SliderGroup(
				size_t count, 
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				Value* ECS_RESTRICT values_to_modify,
				const Value* ECS_RESTRICT lower_bounds,
				const Value* ECS_RESTRICT upper_bounds,
				const Value* ECS_RESTRICT default_values
			) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				SliderGroup<configuration, EnterValuesFilter, Value>(null_config, count, group_name, names, values_to_modify, lower_bounds, upper_bounds, default_values);
			}

			template<size_t configuration, typename EnterValuesFilter = TextFilterAll, typename Value>
			void SliderGroup(
				UIDrawConfig& config,
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				Value* ECS_RESTRICT values_to_modify,
				const Value* ECS_RESTRICT lower_bounds,
				const Value* ECS_RESTRICT upper_bounds,
				const Value* ECS_RESTRICT default_values
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						SliderGroupDrawer<configuration, EnterValuesFilter>(config, count, group_name, names, values_to_modify, lower_bounds, upper_bounds, default_values, position, scale);
						HandleDynamicResource<configuration>(group_name);
					}
					else {
						bool exists = ExistsResource(group_name);
						if (!exists) {
							UIDrawerInitializeSliderGroup<Value> initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_7(initialize_data, count, group_name, names, values_to_modify, lower_bounds, upper_bounds, default_values);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, group_name, InitializeSliderGroupElement<DynamicConfiguration(configuration), Value>);
						}
						SliderGroup<DynamicConfiguration(configuration), EnterValueFilter>(config, count, group_name, names, values_to_modify, lower_bounds, upper_bounds, default_values);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						SliderGroupInitializer<configuration, Value>(config, count, group_name, names, values_to_modify, lower_bounds, upper_bounds, default_values, position, scale);
					}
				}
			}

			template<typename EnterValuesFilter = TextFilterAll, typename Value>
			void SliderGroup2(
				const char* ECS_RESTRICT group_name,
				const char* ECS_RESTRICT name1,
				Value value_to_modify1,
				Value lower_bounds1,
				Value upper_bounds1,
				Value default_value1,
				const char* ECS_RESTRICT name2,
				Value value_to_modify2,
				Value lower_bounds2,
				Value upper_bounds2,
				Value default_value2
			) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				SliderGroup2<configuration, EnterValuesFilter, Value>(null_config, group_name, name1, value_to_modify1, lower_bounds1, upper_bounds1, default_value1, name2, value_to_modify2, lower_bounds2, upper_bounds2, default_value1);
			}

			template<size_t configuration, typename EnterValuesFilter = TextFilterAll, typename Value>
			void SliderGroup2(
				UIDrawConfig& config,
				const char* ECS_RESTRICT group_name,
				const char* ECS_RESTRICT name1,
				Value value_to_modify1,
				Value lower_bounds1,
				Value upper_bounds1,
				Value default_value1,
				const char* ECS_RESTRICT name2,
				Value value_to_modify2,
				Value lower_bounds2,
				Value upper_bounds2,
				Value default_value2
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				const char* names[2];
				Value values_to_modify[2];
				Value lower_bounds[2];
				Value upper_bounds[2];
				Value default_values[2];

				names[0] = name1;
				names[1] = name2;
				values_to_modify[0] = value_to_modify1;
				values_to_modify[1] = value_to_modify2;
				lower_bounds[0] = lower_bounds1;
				lower_bounds[1] = lower_bounds2;
				upper_bounds[0] = upper_bounds1;
				upper_bounds[1] = upper_bounds2;
				default_values[0] = default_value1;
				default_values[1] = default_value2;

				SliderGroup<configuration, EnterValuesFilter, Value>(config, 2, group_name, names, values_to_modify, lower_bounds, upper_bounds, default_values);
			}

			void FloatSlider(const char* _label, float* value_to_modify, float lower_bound, float upper_bound, float default_value = 0.0f, unsigned int precision = 2) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				FloatSlider<configuration>(null_config, _label, value_to_modify, lower_bound, upper_bound, default_value, precision);
			}

			template<size_t configuration>
			void FloatSlider(UIDrawConfig& config, const char* _label, float* value_to_modify, float lower_bound, float upper_bound, float default_value = 0.0f, unsigned int precision = 2) {
				SliderFloat _value_to_modify, _lower_bound, _upper_bound, _default_value;
				_value_to_modify.ConstructPointer(value_to_modify);
				_lower_bound.ConstructValue(lower_bound);
				_upper_bound.ConstructValue(upper_bound);
				_default_value.ConstructValue(default_value);
				_value_to_modify.ConstructExtraData(&precision);
				_lower_bound.ConstructExtraData(&precision);
				_upper_bound.ConstructExtraData(&precision);
				_default_value.ConstructExtraData(&precision);
				Slider<configuration, TextFilterNumbers>(config, _label, _value_to_modify, _lower_bound, _upper_bound, _default_value);
			}

			void FloatSliderGroup(
				size_t count, 
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				float** ECS_RESTRICT values_to_modify,
				const float* ECS_RESTRICT lower_bounds,
				const float* ECS_RESTRICT upper_bounds,
				const float* ECS_RESTRICT default_values,
				size_t precision = 2
			) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				FloatSliderGroup<configuration>(
					null_config, 
					count,
					group_name,
					names, 
					values_to_modify, 
					lower_bounds,
					upper_bounds, 
					default_values, 
					precision
				);
			}

			template<size_t configuration>
			void FloatSliderGroup(
				UIDrawConfig& config,
				size_t count, 
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				float** ECS_RESTRICT values_to_modify,
				const float* ECS_RESTRICT lower_bounds,
				const float* ECS_RESTRICT upper_bounds,
				const float* ECS_RESTRICT default_values,
				size_t precision = 2
			) {
				SliderFloat _values_to_modify[slider_group_max_count];
				SliderFloat _lower_bounds[slider_group_max_count];
				SliderFloat _upper_bounds[slider_group_max_count];
				SliderFloat _default_values[slider_group_max_count];

				for (size_t index = 0; index < count; index++) {
					_values_to_modify[index].ConstructPointer(values_to_modify[index]);
					_lower_bounds[index].ConstructValue(lower_bounds[index]);
					_upper_bounds[index].ConstructValue(upper_bounds[index]);
					_default_values[index].ConstructValue(default_values[index]);

					_values_to_modify[index].ConstructExtraData(&precision);
					_lower_bounds[index].ConstructExtraData(&precision);
					_upper_bounds[index].ConstructExtraData(&precision);
					_default_values[index].ConstructExtraData(&precision);
				}

				SliderGroup<configuration, TextFilterNumbers>(
					config,
					count, 
					group_name, 
					names, 
					_values_to_modify, 
					_lower_bounds, 
					_upper_bounds, 
					_default_values
				);
			}

			void DoubleSlider(const char* _label, double* value_to_modify, double lower_bound, double upper_bound, double default_value = 0, size_t precision = 2) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				DoubleSlider<configuration>(null_config, _label, value_to_modify, lower_bound, upper_bound, default_value, precision);
			}

			template<size_t configuration>
			void DoubleSlider(UIDrawConfig& config, const char* _label, double* value_to_modify, double lower_bound, double upper_bound, double default_value = 0, size_t precision = 2) {
				SliderDouble _value_to_modify, _lower_bound, _upper_bound, _default_value;
				_value_to_modify.ConstructPointer(value_to_modify);
				_lower_bound.ConstructValue(lower_bound);
				_upper_bound.ConstructValue(upper_bound);
				_default_value.ConstructValue(default_value);
				_value_to_modify.ConstructExtraData(&precision);
				_lower_bound.ConstructExtraData(&precision);
				_upper_bound.ConstructExtraData(&precision);
				_default_value.ConstructExtraData(&precision);
				Slider<configuration, TextFilterNumbers>(config, _label, _value_to_modify, _lower_bound, _upper_bound, _default_value);
			}

			void DoubleSliderGroup(
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				double** ECS_RESTRICT values_to_modify,
				const double* ECS_RESTRICT lower_bounds,
				const double* ECS_RESTRICT upper_bounds,
				const double* ECS_RESTRICT default_values,
				size_t precision = 2
			) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				DoubleSliderGroup<configuration>(
					null_config,
					count,
					group_name,
					names,
					values_to_modify,
					lower_bounds,
					upper_bounds,
					default_values,
					precision
					);
			}

			template<size_t configuration>
			void DoubleSliderGroup(
				UIDrawConfig& config,
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				double** ECS_RESTRICT values_to_modify,
				const double* ECS_RESTRICT lower_bounds,
				const double* ECS_RESTRICT upper_bounds,
				const double* ECS_RESTRICT default_values,
				size_t precision = 2
			) {
				SliderDouble _values_to_modify[slider_group_max_count];
				SliderDouble _lower_bounds[slider_group_max_count];
				SliderDouble _upper_bounds[slider_group_max_count];
				SliderDouble _default_values[slider_group_max_count];

				for (size_t index = 0; index < count; index++) {
					_values_to_modify[index].ConstructPointer(values_to_modify[index]);
					_lower_bounds[index].ConstructValue(lower_bounds[index]);
					_upper_bounds[index].ConstructValue(upper_bounds[index]);
					_default_values[index].ConstructValue(default_values[index]);

					_values_to_modify[index].ConstructExtraData(&precision);
					_lower_bounds[index].ConstructExtraData(&precision);
					_upper_bounds[index].ConstructExtraData(&precision);
					_default_values[index].ConstructExtraData(&precision);
				}

				SliderGroup<configuration, TextFilterNumbers>(
					config,
					count, 
					group_name,
					names,
					_values_to_modify, 
					_lower_bounds, 
					_upper_bounds, 
					_default_values
				);
			}

			template<typename Integer>
			void IntSlider(const char* _label, Integer* value_to_modify, Integer lower_bound, Integer upper_bound, Integer default_value = Integer(0)) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				IntSlider<configuration>(null_config, _label, value_to_modify, lower_bound, upper_bound, default_value);
			}

			template<size_t configuration, typename Integer>
			void IntSlider(UIDrawConfig& config, const char* _label, Integer* value_to_modify, Integer lower_bound, Integer upper_bound, Integer default_value = Integer(0)) {
				SliderInteger<Integer> _value_to_modify, _lower_bound, _upper_bound, _default_value;
				_value_to_modify.ConstructPointer(value_to_modify);
				_lower_bound.ConstructValue(lower_bound);
				_upper_bound.ConstructValue(upper_bound);
				_default_value.ConstructValue(default_value);
				Slider<configuration, TextFilterNumbers>(config, _label, _value_to_modify, _lower_bound, _upper_bound, _default_value);
			}

			template<typename Integer>
			void IntSliderGroup(
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				Integer** ECS_RESTRICT values_to_modify,
				const Integer* ECS_RESTRICT lower_bounds,
				const Integer* ECS_RESTRICT upper_bounds,
				const Integer* ECS_RESTRICT default_values
			) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				IntSliderGroup<configuration>(
					null_config,
					count,
					group_name,
					names,
					values_to_modify,
					lower_bounds,
					upper_bounds,
					default_values
				);
			}

			template<size_t configuration, typename Integer>
			void IntSliderGroup(
				UIDrawConfig& config,
				size_t count,
				const char* ECS_RESTRICT group_name,
				const char** ECS_RESTRICT names,
				Integer** ECS_RESTRICT values_to_modify,
				const Integer* ECS_RESTRICT lower_bounds,
				const Integer* ECS_RESTRICT upper_bounds,
				const Integer* ECS_RESTRICT default_values
			) {
				SliderInteger<Integer> _values_to_modify[slider_group_max_count];
				SliderInteger<Integer> _lower_bounds[slider_group_max_count];
				SliderInteger<Integer> _upper_bounds[slider_group_max_count];
				SliderInteger<Integer> _default_values[slider_group_max_count];

				for (size_t index = 0; index < count; index++) {
					_values_to_modify[index].ConstructPointer(values_to_modify[index]);
					_lower_bounds[index].ConstructValue(lower_bounds[index]);
					_upper_bounds[index].ConstructValue(upper_bounds[index]);
					_default_values[index].ConstructValue(default_values[index]);
				}

				SliderGroup<configuration, TextFilterNumbers>(
					config,
					count, 
					group_name, 
					names,
					_values_to_modify,
					_lower_bounds,
					_upper_bounds,
					_default_values
				);
			}

#pragma endregion

			void SetZoomXFactor(float zoom_x_factor) {
				if (current_x == GetNextRowXPosition()) {
					SetLayoutFromZoomXFactor(zoom_x_factor);
					zoom_ptr_modifyable->x = zoom_x_factor;
					current_x = GetNextRowXPosition();
					region_fit_space_horizontal_offset = region_position.x + layout.next_row_padding;
				}
				else {
					SetLayoutFromZoomXFactor(zoom_x_factor);
					zoom_ptr_modifyable->x = zoom_x_factor;
				}
				zoom_inverse.x = 1 / zoom_x_factor;
			}

			void SetZoomYFactor(float zoom_y_factor) {
				if (current_y == region_fit_space_vertical_offset) {
					SetLayoutFromZoomYFactor(zoom_y_factor);
					zoom_ptr_modifyable->y = zoom_y_factor;
					region_fit_space_vertical_offset = region_position.y + system->m_descriptors.misc.title_y_scale + layout.next_row_y_offset;
					if (!dockspace->borders[border_index].draw_region_header) {
						region_fit_space_vertical_offset -= system->m_descriptors.misc.title_y_scale;
					}
					current_y = region_fit_space_vertical_offset;
				}
				else {
					SetLayoutFromZoomYFactor(zoom_y_factor);
					zoom_ptr_modifyable->y = zoom_y_factor;
				}
				zoom_inverse.y = 1 / zoom_y_factor;
			}

			void SetRowHorizontalOffset(float value) {
				if (current_x == GetNextRowXPosition()) {
					current_x += value - next_row_offset;
				}
				next_row_offset = value;
			}

			void SetRowPadding(float value) {
				region_fit_space_horizontal_offset += value - layout.next_row_padding;
				if (fabsf(current_x - GetNextRowXPosition()) < 0.0001f) {
					current_x += value - layout.next_row_padding;
				}
				region_limit.x -= value - layout.next_row_padding;
				layout.next_row_padding = value;
			}

			void SetNextRowYOffset(float value) {
				region_fit_space_vertical_offset += value - layout.next_row_y_offset;
				if (fabsf(current_x - GetNextRowXPosition()) < 0.0001f) {
					current_y += value - layout.next_row_y_offset;
				}
				region_limit.y -= value - layout.next_row_y_offset;
				layout.next_row_y_offset = value;
			}

			// Currently, draw_mode_floats is needed for column draw, only the y component needs to be filled
			// with the desired y spacing
			void SetDrawMode(UIDrawerMode mode, unsigned int target = 0, float draw_mode_float = 0.0f) {
				draw_mode = mode;
				draw_mode_count = 0;
				draw_mode_target = target;
				draw_mode_extra_float.x = draw_mode_float;
				if (mode == UIDrawerMode::ColumnDraw || mode == UIDrawerMode::ColumnDrawFitSpace) {
					draw_mode_extra_float.y = current_y;
				}
			}

			// places a hoverable on the whole window
			void SetWindowHoverable(const UIActionHandler* handler) {
				AddHoverable(region_position, region_scale, *handler);
			}

			// places a clickable on the whole window
			void SetWindowClickable(const UIActionHandler* handler) {
				AddClickable(region_position, region_scale, *handler);
			}

			// places a general on the whole window
			void SetWindowGeneral(const UIActionHandler* handler) {
				AddGeneral(region_position, region_scale, *handler);
			}

			void StabilizeRenderSpan(float2 span) {
				stabilized_render_span = span;
			}

			// configuration is needed for phase deduction
			template<size_t configuration>
			void SetSpriteClusterTexture(LPCWSTR texture, unsigned int count) {
				if constexpr (configuration & UI_CONFIG_LATE_DRAW) {
					system->SetSpriteCluster(dockspace, border_index, texture, count, UIDrawPhase::Late);
				}
				else if constexpr (configuration & UI_CONFIG_SYSTEM_DRAW) {
					system->SetSpriteCluster(dockspace, border_index, texture, count, UIDrawPhase::System);
				}
				else {
					system->SetSpriteCluster(dockspace, border_index, texture, count, UIDrawPhase::Normal);
				}
			}

#pragma region Solid Color Rectangle

			void SolidColorRectangle(Color color = ECS_TOOLS_UI_THEME_COLOR) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				SolidColorRectangle<configuration>(null_config, color);
			}

			template<size_t configuration>
			void SolidColorRectangle(const UIDrawConfig& config, Color color = ECS_TOOLS_UI_THEME_COLOR) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					HandleFitSpaceRectangle<configuration>(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					if (ValidatePosition<configuration>(position)) {
						SolidColorRectangle<configuration>(position, scale, color);

						HandleBorder<configuration>(config, position, scale);

						HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
						HandleRectangleActions<configuration>(config, position, scale);
					}
				}

				FinalizeRectangle<configuration>(position, scale);
			}

#pragma endregion

#pragma region State Table

			// States should be a stack pointer to bool* to the members that should be changed
			void StateTable(const char* name, Stream<const char*> labels, bool** states) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				StateTable<configuration>(null_config, name, labels, states);
			}

			// States should be a stack pointer to a bool array
			void StateTable(const char* name, Stream<const char*> labels, bool* states) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				StateTable<configuration>(null_config, name, labels, states);
			}

			// States should be a stack pointer to bool* to the members that should be changed
			template<size_t configuration>
			void StateTable(const UIDrawConfig& config, const char* name, Stream<const char*> labels, bool** states) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerStateTable* data = (UIDrawerStateTable*)GetResource(name);

						StateTableDrawer<configuration>(config, data, states, position, scale);
						HandleDynamicResource<configuration>(name);
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeStateTable initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_3(initialize_data, name, labels, states);
							InitializeDrawerElement(system, window_index, window_data, additional_data, name, InitializeStateTableElement<DynamicConfiguration(configuration)>);
						}
						StateTable<DynamicConfiguration(configuration)>(config, name, labels, states);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						StateTableInitializer<configuration>(config, name, labels, position);
					}
				}
			}

			// States should be a stack pointer to a bool array
			template<size_t configuration>
			void StateTable(const UIDrawConfig& config, const char* name, Stream<const char*> labels, bool* states) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerStateTable* data = (UIDrawerStateTable*)GetResource(name);

						StateTableDrawer<configuration | UI_CONFIG_STATE_TABLE_SINGLE_POINTER>(config, data, (bool**)states, position, scale);
						HandleDynamicResource<configuration>(name);
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeStateTableSinglePointer initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_3(initialize_data, name, labels, states);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeStateTableSinglePointerElement<DynamicConfiguration(configuration)>);
						}
						StateTable<DynamicConfiguration(configuration)>(config, name, labels, states);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						StateTableInitializer<configuration>(config, name, labels, position);
					}
				}
			}

			template<size_t configuration>
			UIDrawerStateTable* StateTableInitializer(const UIDrawConfig& config, const char* name, Stream<const char*> labels, float2 position) {
				UIDrawerStateTable* data = nullptr;
				
				AddWindowResource(name, [&](const char* identifier) {
					data = GetMainAllocatorBuffer<UIDrawerStateTable>();
					data->labels.Initialize(system->m_memory, labels.size);

					InitializeElementName<configuration, UI_CONFIG_STATE_TABLE_NO_NAME>(config, name, &data->name, position);

					data->max_x_scale = 0;
					float max_x_scale = 0.0f;
					for (size_t index = 0; index < labels.size; index++) {
						ConvertTextToWindowResource<configuration>(config, labels[index], data->labels.buffer + index, position);
						if (max_x_scale < data->labels[index].scale.x) {
							max_x_scale = data->labels[index].scale.x;
							data->max_x_scale = index;
						}
					}

					return data;
				});

				return data;
			}

			template<size_t configuration>
			void StateTableDrawer(const UIDrawConfig& config, UIDrawerStateTable* data, bool** states, float2 position, float2 scale) {
				if constexpr (IsElementNameFirst(configuration, UI_CONFIG_STATE_TABLE_NO_NAME) || IsElementNameAfter(configuration, UI_CONFIG_STATE_TABLE_NO_NAME)) {
					ElementName<configuration>(config, &data->name, position, scale);
					NextRow();
					position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
				}

				float2 square_scale = GetSquareScale(scale.y);
				scale = { data->labels[data->max_x_scale].scale.x + square_scale.x + element_descriptor.label_horizontal_padd * 2.0f, scale.y * data->labels.size + (data->labels.size - 1) * layout.next_row_y_offset };
				
				if constexpr (configuration & UI_CONFIG_STATE_TABLE_ALL) {
					scale.y += square_scale.y + layout.next_row_y_offset;
				}
				
				HandleFitSpaceRectangle<configuration>(position, scale);

				if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition<configuration>(position, scale)) {
					float2 current_position = position;
					Color background_color = HandleColor<configuration>(config);
					Color checkbox_color = LightenColorClamp(background_color, 1.3f);

					UIDrawerStateTableBoolClickable clickable_data;
					if constexpr (configuration & UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE) {
						UIConfigStateTableNotify* notify_data = (UIConfigStateTableNotify*)config.GetParameter(UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE);
						clickable_data.notifier = notify_data->notifier;
					}
					else {
						clickable_data.notifier = nullptr;
					}
					for (size_t index = 0; index < data->labels.size; index++) {
						current_row_y_scale = square_scale.y;

						bool* destination_ptr;
						if constexpr (configuration & UI_CONFIG_STATE_TABLE_SINGLE_POINTER) {
							bool* conversion = (bool*)states;
							destination_ptr = conversion + index;
						}
						else {
							destination_ptr = states[index];
						}
						clickable_data.state = destination_ptr;

						if (*destination_ptr) {
							Color current_color = background_color;
							Color current_checkbox_color = checkbox_color;
							if (IsMouseInRectangle(current_position, { scale.x, square_scale.y })) {
								current_color = LightenColorClamp(current_color, 1.5f);
								current_checkbox_color = LightenColorClamp(current_checkbox_color, 1.5f);
							}

							SolidColorRectangle<UI_CONFIG_LATE_DRAW>(current_position, square_scale, current_color);
							SpriteRectangle<UI_CONFIG_LATE_DRAW>(current_position, square_scale, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, current_checkbox_color);
						}

						AddDefaultClickableHoverable(current_position, { scale.x, square_scale.y }, { StateTableBoolClickable, &clickable_data, sizeof(clickable_data) }, background_color);

						current_position.x += square_scale.x + element_descriptor.label_horizontal_padd;
						Text<configuration | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_TEXT_ALIGN_TO_ROW_Y>(config, data->labels.buffer + index, current_position);
						NextRow();

						current_position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
					}

					if constexpr (configuration & UI_CONFIG_STATE_TABLE_ALL) {
						current_row_y_scale = square_scale.y;

						bool all_true = true;
						if constexpr (configuration & UI_CONFIG_STATE_TABLE_SINGLE_POINTER) {
							bool* conversion = (bool*)states;
							for (size_t index = 0; index < data->labels.size; index++) {
								all_true &= conversion[index];
							}
						}
						else {
							for (size_t index = 0; index < data->labels.size; index++) {
								all_true &= *states[index];
							}
						}

						if (all_true) {
							Color current_color = background_color;
							Color current_checkbox_color = checkbox_color;
							if (IsMouseInRectangle(current_position, { scale.x, square_scale.y })) {
								current_color = LightenColorClamp(current_color, 1.5f);
								current_checkbox_color = LightenColorClamp(current_checkbox_color, 1.5f);
							}

							SolidColorRectangle<UI_CONFIG_LATE_DRAW>(current_position, square_scale, current_color);
							SpriteRectangle<UI_CONFIG_LATE_DRAW>(current_position, square_scale, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, current_checkbox_color);
						}

						UIDrawerStateTableAllButtonData button_data;
						button_data.all_true = all_true;
						button_data.single_pointer = false;
						if constexpr (configuration & UI_CONFIG_STATE_TABLE_SINGLE_POINTER) {
							button_data.single_pointer = true;
						}
						button_data.states = Stream<bool*>(states, data->labels.size);
						if constexpr (configuration & UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE) {
							UIConfigStateTableNotify* notify = (UIConfigStateTableNotify*)config.GetParameter(UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE);
							button_data.notifier = notify->notifier;
						}
						else {
							button_data.notifier = nullptr;
						}

						AddDefaultClickableHoverable(current_position, { scale.x, square_scale.y }, { StateTableAllButtonAction, &button_data, sizeof(button_data) }, background_color);

						current_position.x += square_scale.x + element_descriptor.label_horizontal_padd;
						Text<configuration | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_TEXT_ALIGN_TO_ROW_Y>(config, "All", current_position);
						NextRow();

						current_position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
					}
				}

				FinalizeRectangle<configuration>(position, scale);
			}

#pragma endregion

#pragma region Sprite Rectangle

			void SpriteRectangle(LPCWSTR texture, Color color = ECS_COLOR_WHITE, float2 top_left_uv = {0.0f, 0.0f}, float2 bottom_right_uv = { 1.0f, 1.0f }) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				SpriteRectangle<configuration>(null_config, texture, color, top_left_uv, bottom_right_uv);
			}

			template<size_t configuration>
			void SpriteRectangle(
				const UIDrawConfig& config, 
				LPCWSTR texture, 
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f }, 
				float2 bottom_right_uv = {1.0f, 1.0f}
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					HandleFitSpaceRectangle<configuration>(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}
					
					if (ValidatePosition<configuration>(position, scale)) {
						SpriteRectangle<configuration>(position, scale, texture, color, top_left_uv, bottom_right_uv);
					
						HandleBorder<configuration>(config, position, scale);

						HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
						HandleRectangleActions<configuration>(config, position, scale);
					}
				}

				FinalizeRectangle<configuration>(position, scale);
			}

#pragma endregion

			void StoreElementName(const char* name) {
				system->StoreElementDrawName(window_index, name);
			}

#pragma region Sprite Button

			void SpriteButton(
				UIActionHandler clickable,
				LPCWSTR texture, 
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f }, 
				float2 bottom_right_uv = { 1.0f, 1.0f }
			) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				SpriteButton<configuration>(null_config, clickable, texture, top_left_uv, bottom_right_uv, color);
			}

			template<size_t configuration>
			void SpriteButton(
				const UIDrawConfig& config, 
				UIActionHandler clickable, 
				LPCWSTR texture, 
				Color color = ECS_COLOR_WHITE,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f }
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					bool is_active = true;
					if constexpr (configuration & UI_CONFIG_ACTIVE_STATE) {
						const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
						is_active = active_state->state;
					}

					HandleFitSpaceRectangle<configuration>(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					if (ValidatePosition<configuration>(position, scale)) {
						if (is_active) {
							SpriteRectangle<configuration>(position, scale, texture, color, top_left_uv, bottom_right_uv);

							HandleBorder<configuration>(config, position, scale);
							HandleLateAndSystemDrawActionNullify<configuration>(position, scale);

							AddDefaultClickableHoverable(position, scale, clickable);
						}
						else {
							color.alpha *= color_theme.alpha_inactive_item;
							SpriteRectangle<configuration>(position, scale, texture, color, top_left_uv, bottom_right_uv);
							HandleBorder<configuration>(config, position, scale);
						}
					}

					FinalizeRectangle<configuration>(position, scale);
				}
			}

#pragma endregion

#pragma region Text Table

			void TextTable(const char* name, unsigned int rows, unsigned int columns, const char**  labels) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				TextTable<configuration>(null_config, name, rows, columns, labels);
			}

			template<size_t configuration>
			void TextTable(const UIDrawConfig& config, const char* name, unsigned int rows, unsigned int columns, const char** labels) {
				if constexpr (!initializer) {
					ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

					if constexpr (configuration & UI_CONFIG_DO_NOT_CACHE) {
						TextTableDrawer<configuration>(config, position, scale, rows, columns, labels);
					}
					else {
						UIDrawerTextTable* data = (UIDrawerTextTable*)GetResource(name);
						TextTableDrawer<configuration>(config, position, scale, data, rows, columns);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						TextTableInitializer<configuration>(config, name, rows, columns, labels);
					}
				}
			}

#pragma endregion

#pragma region Text Input

			// single lined text input
			UIDrawerTextInput* TextInput(const char* name, CapacityStream<char>* text_to_fill) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				return TextInput<configuration>(null_config, name, text_to_fill);
			}

			// single lined text input
			template<size_t configuration, typename Filter = TextFilterAll>
			UIDrawerTextInput* TextInput(UIDrawConfig& config, const char* name, CapacityStream<char>* text_to_fill) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerTextInput* input = (UIDrawerTextInput*)GetResource(name);

						TextInputDrawer<configuration, Filter>(config, input, position, scale);
						HandleDynamicResource<configuration>(name);
						return input;
					}
					else {
						bool exists = ExistsResource(name);
						if (!exists) {
							UIDrawerInitializeTextInput initialize_data;
							initialize_data.config = &config;
							ECS_FORWARD_STRUCT_MEMBERS_2(initialize_data, name, text_to_fill);
							InitializeDrawerElement(system, window_index, window_data, &initialize_data, name, InitializeTextInputElement<DynamicConfiguration(configuration)>);
						}
						return TextInput<DynamicConfiguration(configuration), Filter>(config, name, text_to_fill);
					}
				}
				else {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						return TextInputInitializer<configuration>(config, name, text_to_fill, position, scale);
					}
					else {
						return nullptr;
					}
				}
			}

#pragma endregion

#pragma region Text Label

			void TextLabel(const char* characters) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				TextLabel<configuration>(null_config, characters);
			}

			template<size_t configuration>
			void TextLabel(const UIDrawConfig& config, const char* characters) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (configuration & UI_CONFIG_DO_NOT_CACHE) {
						const char* identifier = HandleResourceIdentifier(characters);
						if constexpr (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
							TextLabelWithCull<configuration>(config, identifier, position, scale);

							if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
								UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
								*get_transform->position = position;
								*get_transform->scale = scale;
							}

							FinalizeRectangle<configuration>(position, scale);
						}
						else {
							TextLabel<configuration>(config, identifier, position, scale);
						}
					}
					else {
						UIDrawerTextElement* element = (UIDrawerTextElement*)GetResource(characters);
						HandleFitSpaceRectangle<configuration>(position, scale);
						TextLabelDrawer<configuration>(config, element, position, scale);
					}
				}
				else if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE){
					TextLabelInitializer<configuration>(config, characters, position, scale);
				}
			}

#pragma endregion

#pragma region Text

			void Text(const char* text) {
				ECS_TOOLS_UI_DRAWER_DEFAULT_CONFIGURATION;
				Text<configuration>(null_config, text);
			}

			// non cached drawer
			template<size_t configuration>
			void Text(const UIDrawConfig& config, const char* characters, float2 position) {
				float2 text_span;

				size_t text_count = ParseStringIdentifier(characters, strlen(characters));
				Color color;
				float2 font_size;
				float character_spacing;
				HandleText<configuration>(config, color, font_size, character_spacing);
				auto text_sprites = HandleTextSpriteBuffer<configuration>();
				auto text_sprite_count = HandleTextSpriteCount<configuration>();

				Stream<UISpriteVertex> vertices = Stream<UISpriteVertex>(text_sprites + *text_sprite_count, text_count * 6);
				TextAlignment horizontal_alignment, vertical_alignment;
				GetTextLabelAlignment<configuration>(config, horizontal_alignment, vertical_alignment);

				if constexpr (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					if constexpr (configuration & UI_CONFIG_VERTICAL) {
						if (vertical_alignment == TextAlignment::Bottom) {
							system->ConvertCharactersToTextSprites<false, true>(
								characters,
								position,
								text_sprites,
								text_count,
								color,
								*text_sprite_count,
								font_size,
								character_spacing
								);
							Stream<UISpriteVertex> current_text = GetTextStream<configuration>(text_count * 6);
							text_span = GetTextSpan<false, true>(current_text);
							AlignVerticalText(current_text);
						}
						else {
							system->ConvertCharactersToTextSprites<false>(
								characters,
								position,
								text_sprites,
								text_count,
								color,
								*text_sprite_count,
								font_size,
								character_spacing
								);
							Stream<UISpriteVertex> current_text = GetTextStream<configuration>(text_count * 6);
							text_span = GetTextSpan<false>(current_text);
							AlignVerticalText(current_text);
						}
					}
					else {
						if (horizontal_alignment == TextAlignment::Right) {
							system->ConvertCharactersToTextSprites<true, true>(
								characters,
								position,
								text_sprites,
								text_count,
								color,
								*text_sprite_count,
								font_size,
								character_spacing
								);
							text_span = GetTextSpan<true, true>(GetTextStream<configuration>(text_count * 6));
						}
						else {
							system->ConvertCharactersToTextSprites<true>(
								characters,
								position,
								text_sprites,
								text_count,
								color,
								*text_sprite_count,
								font_size,
								character_spacing
								);
							text_span = GetTextSpan<true>(GetTextStream<configuration>(text_count * 6));
						}
					}
				}
				else {
					if constexpr (configuration & UI_CONFIG_VERTICAL) {
						system->ConvertCharactersToTextSprites<false>(
							characters,
							position,
							text_sprites,
							text_count,
							color,
							*text_sprite_count,
							font_size,
							character_spacing
							);
						Stream<UISpriteVertex> current_text = GetTextStream<configuration>(text_count * 6);
						text_span = GetTextSpan<false>(current_text);
						AlignVerticalText(current_text);
					}
					else {
						system->ConvertCharactersToTextSprites<true>(
							characters,
							position,
							text_sprites,
							text_count,
							color,
							*text_sprite_count,
							font_size,
							character_spacing
							);
						text_span = GetTextSpan<true>(GetTextStream<configuration>(text_count * 6));
					}
				}
				size_t before_count = *text_sprite_count;
				*text_sprite_count += text_count * 6;

				if constexpr (~configuration & UI_CONFIG_DO_NOT_FIT_SPACE) {
					float2 copy = position;
					bool is_moved = HandleFitSpaceRectangle<configuration>(position, text_span);
					if (is_moved) {
						float x_translation = position.x - copy.x;
						float y_translation = position.y - copy.y;
						for (size_t index = before_count; index < *text_sprite_count; index++) {
							text_sprites[index].position.x += x_translation;
							text_sprites[index].position.y -= y_translation;
						}
					}
				}

				if constexpr (configuration & UI_CONFIG_TEXT_ALIGN_TO_ROW_Y) {
					float y_position = AlignMiddle(position.y, current_row_y_scale, text_span.y);
					if (horizontal_alignment == TextAlignment::Right || vertical_alignment == TextAlignment::Bottom) {
						TranslateTextY(y_position, vertices.size - 6, vertices);
					}
					else {
						TranslateTextY(y_position, 0, vertices);
					}
				}

				if constexpr (~configuration & UI_CONFIG_DO_NOT_ADVANCE) {
					UpdateRenderBoundsRectangle(position, text_span);
					UpdateCurrentScale(position, text_span);
				}
				if constexpr (~configuration & UI_CONFIG_DO_NOT_ADVANCE) {
					HandleDrawMode<configuration>();
				}
			}

			template<size_t configuration>
			void Text(const UIDrawConfig& config, const char* text, float2& position, float2& text_span) {
				Color color;
				float character_spacing;
				float2 font_size;

				HandleText<configuration>(config, color, font_size, character_spacing);
				if constexpr (configuration & UI_CONFIG_DO_NOT_CACHE) {
					if constexpr (!initializer) {
						float text_y_span = system->GetTextSpriteYScale(font_size.y);
						// Preemptively check to see if it actually needs to be drawn on the y axis by setting the horizontal 
						// draw to maximum
						if (ValidatePositionY<configuration>(position, { 100.0f, text_y_span })) {
							size_t text_count = ParseStringIdentifier(text, strlen(text));

							auto text_stream = GetTextStream<configuration>(text_count * 6);
							auto text_sprites = HandleTextSpriteBuffer<configuration>();
							auto text_sprite_count = HandleTextSpriteCount<configuration>();
							system->ConvertCharactersToTextSprites<((~configuration)& UI_CONFIG_VERTICAL) != 0>(
								text,
								position,
								text_sprites,
								text_count,
								color,
								*text_sprite_count,
								font_size,
								character_spacing
								);
							text_span = GetTextSpan<((~configuration)& UI_CONFIG_VERTICAL) != 0>(text_stream);
							*text_sprite_count += 6 * text_count;

							bool is_moved = HandleFitSpaceRectangle<configuration>(position, text_span);
							if (is_moved) {
								TranslateText(position.x, position.y, text_stream);
							}

							if constexpr (configuration & UI_CONFIG_TEXT_ALIGN_TO_ROW_Y) {
								float y_position = AlignMiddle(position.y, current_row_y_scale, text_span.y);
								TranslateTextY(y_position, 0, text_stream);
							}

							if (!ValidatePosition<configuration>({ text_stream[0].position.x, -text_stream[0].position.y }, text_span)) {
								*text_sprite_count -= 6 * text_count;
							}
						}
						// Still have to fill in the text span
						else {
							text_span = system->GetTextSpan(text, ParseStringIdentifier(text, strlen(text)), font_size.x, font_size.y, character_spacing);
							HandleFitSpaceRectangle<configuration>(position, text_span);
						}
					}
				}
				else {
					UIDrawerTextElement* info = HandleTextCopyFromResource<configuration>(text, position, color, ECS_TOOLS_UI_DRAWER_IDENTITY_SCALE);
					text_span = *info->TextScale();

					text_span.x *= zoom_ptr->x * info->GetInverseZoomX();
					text_span.y *= zoom_ptr->y * info->GetInverseZoomY();
				}
			}

			template<size_t configuration>
			void Text(const UIDrawConfig& config, const char* characters) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
						UIDrawerTextElement* resource = (UIDrawerTextElement*)GetResource(characters);
						Text<configuration>(config, resource, position);
					}
					else {
						Text<configuration>(config, characters, position);
					}
				}

				else if constexpr (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					TextInitializer<configuration>(config, characters, position);
				}
			}

#pragma endregion

			void TextToolTip(const char* characters, float2 position, float2 scale, const UITooltipBaseData* base = nullptr) {
				UITextTooltipHoverableData tool_tip_data;
				if (base != nullptr) {
					tool_tip_data.base = *base;
				}
				tool_tip_data.characters = characters;

				if (IsPointInRectangle(mouse_position, position, scale)) {
					ActionData default_action_data = system->GetFilledActionData(window_index);
					default_action_data.buffers = system_buffers;
					default_action_data.counts = system_counts;
					default_action_data.data = &tool_tip_data;
					default_action_data.position = position;
					default_action_data.scale = scale;

					TextTooltipHoverable(&default_action_data);
				}
			}

			void DefaultHoverableWithToolTip(const char* characters, float2 position, float2 scale, const Color* color = nullptr, const float* percentage = nullptr, const UITooltipBaseData* base = nullptr) {
				UIDefaultHoverableWithTooltipData tool_tip_data;
				if (color != nullptr) {
					tool_tip_data.color = *color;
				}
				else {
					tool_tip_data.color = color_theme.theme;
				}
				
				if (percentage != nullptr) {
					tool_tip_data.percentage = *percentage;
				}
				else {
					tool_tip_data.percentage = 1.25f;
				}

				if (base != nullptr) {
					tool_tip_data.tool_tip_data.base = *base;
				}
				tool_tip_data.tool_tip_data.characters = characters;

				AddHoverable(position, scale, { DefaultHoverableWithToolTip, &tool_tip_data, sizeof(tool_tip_data), UIDrawPhase::System });
			}

			void DefaultHoverableWithToolTip(float2 position, float2 scale, const UIDefaultHoverableWithTooltipData* data) {
				AddHoverable(position, scale, { DefaultHoverableWithToolTip, data, sizeof(*data), UIDrawPhase::System });
			}

#pragma region Vertex Color Rectangle

			template<size_t configuration>
			void VertexColorRectangle(
				const UIDrawConfig& config,
				Color top_left,
				Color top_right, 
				Color bottom_left, 
				Color bottom_right
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					HandleFitSpaceRectangle(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					if (ValidatePosition<configuration>(position, scale)) {
						auto buffer = HandleSolidColorBuffer<configuration>();
						auto count = HandleSolidColorCount<configuration>();

						SetVertexColorRectangle(position, scale, top_left, top_right, bottom_left, bottom_right, buffer, count);

						HandleBorder<configuration>(config, position, scale);

						HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
						HandleRectangleActions<configuration>(config, position, scale);
					}
				}

				FinalizeRectangle<configuration>(position, scale);
			}

			template<size_t configuration>
			void VertexColorRectangle(
				const UIDrawConfig& config,
				ColorFloat top_left,
				ColorFloat top_right,
				ColorFloat bottom_left,
				ColorFloat bottom_right
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					HandleFitSpaceRectangle(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					if (ValidatePosition<configuration>(position, scale)) {
						auto buffer = HandleSolidColorBuffer<configuration>();
						auto count = HandleSolidColorCount<configuration>();

						SetVertexColorRectangle(position, scale, Color(top_left), Color(top_right), Color(bottom_left), Color(bottom_right), buffer, count);
						HandleBorder<configuration>(config, position, scale);

						HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
						HandleRectangleActions<configuration>(config, position, scale);
					}
				}

				FinalizeRectangle<configuration>(position, scale);
			}

			template<size_t configuration>
			void VertexColorSpriteRectangle(
				const UIDrawConfig& config,
				LPCWSTR texture,
				const Color* colors,
				const float2* uvs,
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					HandleFitSpaceRectangle(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					if (ValidatePosition<configuration>(position, scale)) {
						VertexColorSpriteRectangle<configuration>(position, scale, texture, colors, uvs, phase);

						HandleBorder<configuration>(config, position, scale);

						HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
						HandleRectangleActions<configuration>(config, position, scale);
					}
				}

				FinalizeRectangle<configuration>(position, scale);
			}

			template<size_t configuration>
			void VertexColorSpriteRectangle(
				const UIDrawConfig& config,
				LPCWSTR texture,
				const Color* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					HandleFitSpaceRectangle(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					if (ValidatePosition<configuration>(position, scale)) {
						VertexColorSpriteRectangle<configuration>(position, scale, texture, colors, top_left_uv, bottom_right_uv, phase);

						HandleBorder<configuration>(config, position, scale);

						HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
						HandleRectangleActions<configuration>(config, position, scale);
					}
				}

				FinalizeRectangle<configuration>(position, scale);
			}

			template<size_t configuration>
			void VertexColorSpriteRectangle(
				const UIDrawConfig& config,
				LPCWSTR texture,
				const ColorFloat* colors,
				const float2* uvs,
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					HandleFitSpaceRectangle(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					if (ValidatePosition<configuration>(position, scale)) {
						VertexColorSpriteRectangle<configuration>(position, scale, texture, colors, uvs, phase);

						HandleBorder<configuration>(position, scale);

						HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
						HandleRectangleActions<configuration>(config, position, scale);
					}
				}

				FinalizeRectangle<configuration>(position, scale);
			}

			template<size_t configuration>
			void VertexColorSpriteRectangle(
				const UIDrawConfig& config,
				LPCWSTR texture,
				const ColorFloat* colors,
				float2 top_left_uv = { 0.0f, 0.0f },
				float2 bottom_right_uv = { 1.0f, 1.0f },
				UIDrawPhase phase = UIDrawPhase::Normal
			) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if constexpr (!initializer) {
					HandleFitSpaceRectangle(position, scale);

					if constexpr (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					if (ValidatePosition<configuration>(position, scale)) {
						VertexColorSpriteRectangle<configuration>(position, scale, texture, colors, top_left_uv, bottom_right_uv, phase);

						HandleBorder<configuration>(config, position, scale);

						HandleLateAndSystemDrawActionNullify<configuration>(position, scale);
						HandleRectangleActions<configuration>(config, position, scale);
					}
				}

				FinalizeRectangle<configuration>(position, scale);
			}

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
			bool deallocate_identifier_buffers;
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
			float2 stabilized_render_span;
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
			// this is in order to not add another allocator for the system; 
			// it will keep track of all the allocations made and free them all at the end
			Stream<void*> temp_memory_from_main_allocator;
		};

		using UIDrawerInitializeFunction = void (*)(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr);

		// --------------------------------------------------------------------------------------------------------------

		static void InitializeDrawerElement(UISystem* system, unsigned int window_index, void* window_data, void* additional_data, const char* name, UIDrawerInitializeFunction initialize) {
			UIDrawerDescriptor descriptor = system->GetDrawerDescriptor(window_index);
			descriptor.do_not_initialize_viewport_sliders = true;
			UIDrawer<true> drawer = UIDrawer<true>(
				descriptor,
				window_data
			);

			system->AddWindowDrawerElement(window_index, name);
			initialize(window_data, additional_data, &drawer);
			system->FinalizeWindowElement(window_index);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeComboBoxElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeComboBox* data = (UIDrawerInitializeComboBox*)additional_data;
			drawer_ptr->ComboBox<configuration>(*data->config, data->name, data->labels, data->label_display_count, data->active_label);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeColorInputElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeColorInput* data = (UIDrawerInitializeColorInput*)additional_data;
			drawer_ptr->ColorInput<configuration>(*data->config, data->name, data->color, data->default_color);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeFilterMenuElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeFilterMenu* data = (UIDrawerInitializeFilterMenu*)additional_data;
			drawer_ptr->FilterMenu<configuration>(*data->config, data->name, data->label_names, data->states);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeFilterMenuSinglePointerElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeFilterMenuSinglePointer* data = (UIDrawerInitializeFilterMenuSinglePointer*)additional_data;
			drawer_ptr->FilterMenu<configuration>(*data->config, data->name, data->labels, data->states);
		}

		// --------------------------------------------------------------------------------------------------------------
		
		template<size_t configuration>
		void InitializeHierarchyElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeHierarchy* data = (UIDrawerInitializeHierarchy*)additional_data;
			drawer_ptr->Hierarchy<configuration>(*data->config, data->name);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeLabelHierarchyElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeLabelHierarchy* data = (UIDrawerInitializeLabelHierarchy*)additional_data;
			drawer_ptr->LabelHierarchy<configuration>(*data->config, data->identifier, data->labels);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeListElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeList* data = (UIDrawerInitializeList*)additional_data;
			drawer_ptr->List<configuration>(*data->config, data->name);
		}

		// --------------------------------------------------------------------------------------------------------------
		
		template<size_t configuration>
		void InitializeMenuElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeMenu* data = (UIDrawerInitializeMenu*)additional_data;
			drawer_ptr->Menu<configuration>(*data->config, data->name, data->state);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeFloatInputElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeFloatInput* data = (UIDrawerInitializeFloatInput*)additional_data;
			drawer_ptr->FloatInput<configuration>(*data->config, data->name, data->value, data->default_value, data->lower_bound, data->upper_bound);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration, typename Integer>
		void InitializeIntegerInputElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeIntegerInput<Integer>* data = (UIDrawerInitializeIntegerInput<Integer>*)additional_data;
			drawer_ptr->IntInput<configuration>(*data->config, data->name, data->value, data->default_value, data->min, data->max);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeDoubleInputElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeDoubleInput* data = (UIDrawerInitializeDoubleInput*)additional_data;
			drawer_ptr->DoubleInput<configuration>(*data->config, data->name, data->value, data->default_value, data->lower_bound, data->upper_bound);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeFloatInputGroupElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeFloatInputGroup* data = (UIDrawerInitializeFloatInputGroup*)additional_data;
			drawer_ptr->FloatInputGroup<configuration>(*data->config, data->count, data->group_name, data->names, data->values, data->default_values, data->lower_bound, data->upper_bound);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration, typename Integer>
		void InitializeIntegerInputGroupElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeIntegerInputGroup<Integer>* data = (UIDrawerInitializeIntegerInputGroup<Integer>*)additional_data;
			drawer_ptr->IntInputGroup<configuration>(*data->config, data->count, data->group_name, data->names, data->values, data->default_values, data->lower_bound, data->upper_bound);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeDoubleInputGroupElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeDoubleInputGroup* data = (UIDrawerInitializeDoubleInputGroup*)additional_data;
			drawer_ptr->DoubleInputGroup<configuration>(*data->config, data->count, data->group_name, data->names, data->values, data->default_values, data->lower_bound, data->upper_bound);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration, typename Value>
		void InitializeSliderElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeSlider<Value>* data = (UIDrawerInitializeSlider<Value>*)additional_data;
			drawer_ptr->Slider<configuration>(*data->config, data->name, data->value_to_modify, data->lower_bound, data->upper_bound, data->default_value);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration, typename Value>
		void InitializeSliderGroupElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeSliderGroup<Value>* data = (UIDrawerInitializeSliderGroup<Value>*)additional_data;
			drawer_ptr->SliderGroup<configuration>(*data->config, data->count, data->group_name, data->names, data->values_to_modify, data->lower_bounds, data->upper_bounds, data->default_values);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeStateTableElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeStateTable* data = (UIDrawerInitializeStateTable*)additional_data;
			drawer_ptr->StateTable<configuration>(*data->config, data->name, data->labels, data->states);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeStateTableSinglePointerElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeStateTableSinglePointer* data = (UIDrawerInitializeStateTableSinglePointer*)additional_data;
			drawer_ptr->StateTable<configuration>(*data->config, data->name, data->labels, data->states);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<size_t configuration>
		void InitializeTextInputElement(void* window_data, void* additional_data, UIDrawer<true>* drawer_ptr) {
			UIDrawerInitializeTextInput* data = (UIDrawerInitializeTextInput*)additional_data;
			drawer_ptr->TextInput<configuration>(*data->config, data->name, data->text_to_fill);
		}

		// --------------------------------------------------------------------------------------------------------------

	}

}