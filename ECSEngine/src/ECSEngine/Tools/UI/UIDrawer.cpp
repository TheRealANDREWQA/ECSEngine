#include "ecspch.h"
#include "UISystem.h"
#include "UIDrawConfigs.h"
#include "UIDrawerStructures.h"
#include "UIResourcePaths.h"
#include "../../Utilities/Path.h"
#include "UIDrawer.h"

#define ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config) float2 position; float2 scale; HandleTransformFlags(configuration, config, position, scale)

#define ECS_TOOLS_UI_DRAWER_IDENTITY_SCALE [](float2 scale, const UIDrawer& drawer) {return scale;}
#define ECS_TOOLS_UI_DRAWER_LABEL_SCALE [](float2 scale, const UIDrawer& drawer){return float2(scale.x + 2.0f * drawer.element_descriptor.label_horizontal_padd, scale.y + 2.0f * drawer.element_descriptor.label_vertical_padd);}

namespace ECSEngine {

	namespace Tools {

		const char* UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS[] = {
			"x",
			"y",
			"z",
			"w",
			"t",
			"u",
			"v"
		};

		using UIDrawerLabelHierarchyHash = HashFunctionMultiplyString;

		const size_t SLIDER_GROUP_MAX_COUNT = 8;
		const float NUMBER_INPUT_DRAG_FACTOR = 200.0f;

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t NullifyConfiguration(size_t configuration, size_t bit_flag) {
			return ~bit_flag & configuration;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool IsElementNameFirst(size_t configuration, size_t no_name_flag) {
			return ((configuration & UI_CONFIG_ELEMENT_NAME_FIRST) != 0) && ((configuration & no_name_flag) == 0);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool IsElementNameAfter(size_t configuration, size_t no_name_flag) {
			return ((configuration & UI_CONFIG_ELEMENT_NAME_FIRST) == 0) && ((configuration & no_name_flag) == 0);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t DynamicConfiguration(size_t configuration) {
			return NullifyConfiguration(configuration, UI_CONFIG_DO_NOT_CACHE) | UI_CONFIG_DYNAMIC_RESOURCE;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename BasicType>
		void GroupInitializerImplementation(
			UIDrawer* drawer,
			size_t configuration,
			UIDrawConfig& config,
			const char* group_name,
			size_t count,
			const char** names,
			BasicType** values,
			const BasicType* default_values,
			const BasicType* lower_bounds,
			const BasicType* upper_bounds,
			float2 position,
			float2 scale
		) {
			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				drawer->BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}

			size_t LABEL_CONFIGURATION = configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
			if (IsElementNameFirst(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
				drawer->TextLabel(LABEL_CONFIGURATION | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, group_name);
				drawer->Indent(-1.0f);
			}

			bool has_pushed_stack = drawer->PushIdentifierStackStringPattern();
			drawer->PushIdentifierStack(group_name);

			for (size_t index = 0; index < count; index++) {
				BasicType lower_bound, upper_bound, default_value = *values[index];
				if constexpr (std::is_same_v<BasicType, float>) {
					lower_bound = -FLT_MAX;
					upper_bound = FLT_MAX;
				}
				else if constexpr (std::is_same_v<BasicType, double>) {
					lower_bound = -DBL_MAX;
					upper_bound = DBL_MAX;
				}
				else {
					function::IntegerRange(lower_bound, upper_bound);
				}

				if (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS) {
					lower_bound = lower_bounds[0];
					upper_bound = upper_bounds[0];
					default_value = default_values[0];
				}
				else {
					if (lower_bounds != nullptr) {
						lower_bound = lower_bounds[index];
					}
					if (upper_bounds != nullptr) {
						upper_bound = upper_bounds[index];
					}
					if (default_values != nullptr) {
						default_value = default_values[index];
					}
				}

				size_t initializer_configuration = configuration | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
				initializer_configuration |= function::Select<size_t>(~configuration & UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES, UI_CONFIG_ELEMENT_NAME_FIRST, UI_CONFIG_TEXT_INPUT_NO_NAME);
#define PARAMETERS initializer_configuration, \
					config, \
					names[index], \
					values[index], \
					default_value, \
					lower_bound, \
					upper_bound, \
					position, \
					scale
				
				if constexpr (std::is_same_v<BasicType, float>) {
					drawer->FloatInputInitializer(PARAMETERS);
				}
				else if constexpr (std::is_same_v<BasicType, double>) {
					drawer->DoubleInputInitializer(PARAMETERS);
				}
				else {
					drawer->IntInputInitializer<BasicType>(PARAMETERS);
				}

#undef PARAMETERS
			}

			if (has_pushed_stack) {
				drawer->PopIdentifierStack();
			}
			drawer->PopIdentifierStack();

			if (IsElementNameAfter(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
				drawer->TextLabel(LABEL_CONFIGURATION, config, group_name);
			}
		}

		template<typename BasicType>
		void GroupDrawerImplementation(
			UIDrawer* drawer,
			size_t configuration,
			UIDrawConfig& config,
			const char* group_name,
			size_t count,
			const char** names,
			BasicType** values,
			const BasicType* lower_bounds,
			const BasicType* upper_bounds,
			float2 position,
			float2 scale
		) {
			size_t LABEL_CONFIGURATION = configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
			if (IsElementNameFirst(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
				drawer->TextLabel(LABEL_CONFIGURATION | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, group_name);
				drawer->Indent(-1.0f);
			}

			bool has_pushed_stack = drawer->PushIdentifierStackStringPattern();
			drawer->PushIdentifierStack(group_name);

			for (size_t index = 0; index < count; index++) {
				BasicType lower_bound, upper_bound, default_value = *values[index];
				if constexpr (std::is_same_v<BasicType, float>) {
					lower_bound = -FLT_MAX;
					upper_bound = FLT_MAX;
				}
				else if constexpr (std::is_same_v<BasicType, double>) {
					lower_bound = -DBL_MAX;
					upper_bound = DBL_MAX;
				}
				else {
					function::IntegerRange(lower_bound, upper_bound);
				}

				if (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS) {
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

				size_t drawer_configuration = configuration | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
					| function::Select<size_t>(~configuration & UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES, UI_CONFIG_ELEMENT_NAME_FIRST, UI_CONFIG_TEXT_INPUT_NO_NAME);

				if constexpr (std::is_same_v<BasicType, float>) {
					drawer->FloatInputDrawer(
						drawer_configuration,
						config,
						names[index],
						values[index],
						lower_bound,
						upper_bound,
						position,
						scale
					);
				}
				else if constexpr (std::is_same_v<BasicType, double>) {
					drawer->DoubleInputDrawer(
						drawer_configuration,
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
					drawer->IntInputDrawer<BasicType>(
						drawer_configuration,
						config,
						names[index],
						values[index],
						lower_bound,
						upper_bound,
						position,
						scale
					);
				}
				position = drawer->GetCurrentPosition();
			}

			if (has_pushed_stack) {
				drawer->PopIdentifierStack();
			}
			drawer->PopIdentifierStack();

			if (IsElementNameAfter(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
				drawer->TextLabel(LABEL_CONFIGURATION, config, group_name);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::CalculateRegionParameters(float2 position, float2 scale, const float2* render_offset) {
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
			if (!initializer) {
				bool substract = dockspace->borders[border_index].draw_elements == false;
				region_fit_space_vertical_offset -= system->m_descriptors.misc.title_y_scale * substract;
			}

			current_x = GetNextRowXPosition();
			current_y = region_fit_space_vertical_offset;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DefaultDrawParameters() {
			draw_mode = UIDrawerMode::Indent;
			draw_mode_count = 0;
			draw_mode_target = 0;
			current_column_x_scale = 0.0f;
			current_row_y_scale = 0.0f;
			next_row_offset = 0.0f;

			zoom_inverse = { 1.0f / zoom_ptr->x, 1.0f / zoom_ptr->y };

			no_padding_for_render_sliders = false;
			no_padding_render_region = false;
			deallocate_constructor_allocations = true;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ConvertTextToWindowResource(size_t configuration, const UIDrawConfig& config, const char* text, UIDrawerTextElement* element, float2 position) {
			size_t text_count = ParseStringIdentifier(text, strlen(text));
			Color color;
			float character_spacing;
			float2 font_size;
			HandleText(configuration, config, color, font_size, character_spacing);

			TextAlignment horizontal_alignment = TextAlignment::Middle, vertical_alignment = TextAlignment::Middle;
			if (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
				GetTextLabelAlignment(configuration, config, horizontal_alignment, vertical_alignment);
			}

			element->position = position;
			element->SetZoomFactor(*zoom_ptr);

			if (text_count > 0) {
				void* text_allocation = GetMainAllocatorBuffer(sizeof(UISpriteVertex) * text_count * 6, alignof(UISpriteVertex));

				CapacityStream<UISpriteVertex>* text_stream = element->TextStream();
				text_stream->buffer = (UISpriteVertex*)text_allocation;
				text_stream->size = text_count * 6;

				float2 text_span;
				if (configuration & UI_CONFIG_VERTICAL) {
					if (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
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
					if (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
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

				element->scale = text_span;
			}
			else {
				element->text_vertices.size = 0;
				element->scale = { 0.0f, 0.0f };
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ExpandRectangleFromFlag(const float* resize_factor, float2& position, float2& scale) const {
			float2 temp_scale = scale;
			position = ExpandRectangle(position, temp_scale, { *resize_factor, *(resize_factor + 1) }, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::ValidatePosition(size_t configuration, float2 position) {
			if (~configuration & UI_CONFIG_DO_NOT_VALIDATE_POSITION) {
				return (position.x < max_region_render_limit.x) && (position.y < max_region_render_limit.y);
			}
			else {
				return true;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::ValidatePosition(size_t configuration, float2 position, float2 scale) {
			return ValidatePosition(configuration, position) && ValidatePositionMinBound(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::ValidatePositionY(size_t configuration, float2 position, float2 scale) {
			if (~configuration & UI_CONFIG_DO_NOT_VALIDATE_POSITION) {
				return (position.y < max_region_render_limit.y) && (position.y + scale.y >= min_region_render_limit.y);
			}
			else {
				return true;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::ValidatePositionMinBound(size_t configuration, float2 position, float2 scale) {
			if (~configuration & UI_CONFIG_DO_NOT_VALIDATE_POSITION) {
				return (position.x + scale.x >= min_region_render_limit.x) && (position.y + scale.y >= min_region_render_limit.y);
			}
			else {
				return true;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIVertexColor* UIDrawer::HandleSolidColorBuffer(size_t configuration) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return (UIVertexColor*)buffers[ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SOLID_COLOR];
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return (UIVertexColor*)system_buffers[ECS_TOOLS_UI_SOLID_COLOR];
			}
			else {
				return (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t* UIDrawer::HandleSolidColorCount(size_t configuration) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return counts + ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SOLID_COLOR;
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return system_counts + ECS_TOOLS_UI_SOLID_COLOR;
			}
			else {
				return counts + ECS_TOOLS_UI_SOLID_COLOR;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UISpriteVertex* UIDrawer::HandleTextSpriteBuffer(size_t configuration) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return (UISpriteVertex*)buffers[ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_TEXT_SPRITE];
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return (UISpriteVertex*)system_buffers[ECS_TOOLS_UI_TEXT_SPRITE];
			}
			else {
				return (UISpriteVertex*)buffers[ECS_TOOLS_UI_TEXT_SPRITE];
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t* UIDrawer::HandleTextSpriteCount(size_t configuration) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return counts + ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_TEXT_SPRITE;
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return system_counts + ECS_TOOLS_UI_TEXT_SPRITE;
			}
			else {
				return counts + ECS_TOOLS_UI_TEXT_SPRITE;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UISpriteVertex* UIDrawer::HandleSpriteBuffer(size_t configuration) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return (UISpriteVertex*)buffers[ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SPRITE];
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return (UISpriteVertex*)system_buffers[ECS_TOOLS_UI_SPRITE];
			}
			else {
				return (UISpriteVertex*)buffers[ECS_TOOLS_UI_SPRITE];
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t* UIDrawer::HandleSpriteCount(size_t configuration) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return counts + ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SPRITE;
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return system_counts + ECS_TOOLS_UI_SPRITE;
			}
			else {
				return counts + ECS_TOOLS_UI_SPRITE;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UISpriteVertex* UIDrawer::HandleSpriteClusterBuffer(size_t configuration) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return (UISpriteVertex*)buffers[ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SPRITE_CLUSTER];
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return (UISpriteVertex*)system_buffers[ECS_TOOLS_UI_SPRITE_CLUSTER];
			}
			else {
				return (UISpriteVertex*)buffers[ECS_TOOLS_UI_SPRITE_CLUSTER];
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t* UIDrawer::HandleSpriteClusterCount(size_t configuration) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return counts + ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_SPRITE_CLUSTER;
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return system_counts + ECS_TOOLS_UI_SPRITE_CLUSTER;
			}
			else {
				return counts + ECS_TOOLS_UI_SPRITE_CLUSTER;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIVertexColor* UIDrawer::HandleLineBuffer(size_t configuration) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return (UIVertexColor*)buffers[ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_LINE];
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return (UIVertexColor*)system_buffers[ECS_TOOLS_UI_LINE];
			}
			else {
				return (UIVertexColor*)buffers[ECS_TOOLS_UI_LINE];
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t* UIDrawer::HandleLineCount(size_t configuration) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return counts + ECS_TOOLS_UI_MATERIALS + ECS_TOOLS_UI_LINE;
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return system_counts + ECS_TOOLS_UI_LINE;
			}
			else {
				return counts + ECS_TOOLS_UI_LINE;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Line(size_t configuration, float2 positions1, float2 positions2, Color color) {
			auto buffer = HandleLineBuffer(configuration);
			auto count = HandleLineCount(configuration);
			SetLine(positions1, positions2, color, *count, buffer);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleLateAndSystemDrawActionNullify(size_t configuration, float2 position, float2 scale) {
			if ((configuration & UI_CONFIG_LATE_DRAW) || (configuration & UI_CONFIG_SYSTEM_DRAW)) {
				unsigned char dummy = 0;
				if (~configuration & UI_CONFIG_DO_NOT_NULLIFY_HOVERABLE_ACTION) {
					AddHoverable(position, scale, { SkipAction, &dummy, 1 });
				}
				if (~configuration & UI_CONFIG_DO_NOT_NULLIFY_CLICKABLE_ACTION) {
					AddClickable(position, scale, { SkipAction, &dummy, 1 });
				}
				if (~configuration & UI_CONFIG_DO_NOT_NULLIFY_GENERAL_ACTION) {
					AddGeneral(position, scale, { SkipAction, &dummy, 1, });
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawPhase UIDrawer::HandlePhase(size_t configuration) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return UIDrawPhase::Late;
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return UIDrawPhase::System;
			}
			else {
				return UIDrawPhase::Normal;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleTransformFlags(size_t configuration, const UIDrawConfig& config, float2& position, float2& scale) {
			if (configuration & UI_CONFIG_ABSOLUTE_TRANSFORM) {
				const float* transform = (const float*)config.GetParameter(UI_CONFIG_ABSOLUTE_TRANSFORM);
				position = { *transform, *(transform + 1) };
				scale = { *(transform + 2), *(transform + 3) };
			}
			else if (configuration & UI_CONFIG_RELATIVE_TRANSFORM) {
				const float* transform = (const float*)config.GetParameter(UI_CONFIG_RELATIVE_TRANSFORM);
				position.x = current_x + *transform;
				position.y = current_y + *(transform + 1);
				scale = {
					layout.default_element_x * *(transform + 2),
					layout.default_element_y * *(transform + 3)
				};
			}
			else if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				const float* transform = (const float*)config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
				WindowSizeTransformType type = *(WindowSizeTransformType*)transform;

				position.x = current_x + (*(transform + 1) * layout.default_element_x);
				position.y = current_y + (*(transform + 2) * layout.default_element_y);

				scale = GetWindowSizeScaleElement(type, { *(transform + 3), *(transform + 4) });
			}
			else {
				position = { current_x, current_y };
				scale = { layout.default_element_x, layout.default_element_y };
			}

			if (configuration & UI_CONFIG_MAKE_SQUARE) {
				scale = GetSquareScale(scale.y);
			}

			position.x -= region_render_offset.x;
			position.y -= region_render_offset.y;

		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleText(size_t configuration, const UIDrawConfig& config, Color& color, float2& font_size, float& character_spacing) const {
			if (configuration & UI_CONFIG_TEXT_PARAMETERS) {
				const float* params = (const float*)config.GetParameter(UI_CONFIG_TEXT_PARAMETERS);
				// first the color, then the size and finally the spacing
				color = *(Color*)params;
				font_size = *(float2*)(params + 1);
				character_spacing = *(params + 3);
				font_size.x *= zoom_ptr->x;
			}
			else {
				font_size.y = font.size * zoom_inverse.x;
				font_size.x = font.size * ECS_TOOLS_UI_FONT_X_FACTOR;
				if (~configuration & UI_CONFIG_UNAVAILABLE_TEXT) {
					color = color_theme.default_text;
				}
				else {
					color = color_theme.unavailable_text;
				}
				character_spacing = font.character_spacing;
			}
			font_size.y *= zoom_ptr->y;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleTextStreamColorUpdate(Color color, Stream<UISpriteVertex> vertices) {
			if (color != vertices[0].color) {
				for (size_t index = 0; index < vertices.size; index++) {
					vertices[index].color = color;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleTextStreamColorUpdate(Color color, CapacityStream<UISpriteVertex> vertices) {
			if (color != vertices[0].color) {
				for (size_t index = 0; index < vertices.size; index++) {
					vertices[index].color = color;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleTextLabelAlignment(
			size_t configuration,
			const UIDrawConfig& config,
			float2 text_span,
			float2 label_size,
			float2 label_position,
			float& x_position,
			float& y_position,
			TextAlignment& horizontal_alignment,
			TextAlignment& vertical_alignment
		) {
			if (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
				const float* params = (const float*)config.GetParameter(UI_CONFIG_TEXT_ALIGNMENT);
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleTextLabelAlignment(
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleDynamicResource(size_t configuration, const char* name) {
			if (configuration & UI_CONFIG_DYNAMIC_RESOURCE) {
				const char* identifier = HandleResourceIdentifier(name);
				system->IncrementWindowDynamicResource(window_index, identifier);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleBorder(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale) {
			if (configuration & UI_CONFIG_BORDER) {
				const UIConfigBorder* parameters = (const UIConfigBorder*)config.GetParameter(UI_CONFIG_BORDER);

				float2 border_size = GetSquareScale(parameters->thickness);
				border_size.x *= zoom_ptr->x;
				border_size.y *= zoom_ptr->y;

				// Late phase or system phase always in order to have the border drawn upon the hovered effect
				void** phase_buffers = buffers + ECS_TOOLS_UI_MATERIALS;
				size_t* phase_counts = counts + ECS_TOOLS_UI_MATERIALS;
				if (configuration & UI_CONFIG_SYSTEM_DRAW) {
					phase_buffers = system_buffers;
					phase_counts = system_counts;
				}
				else if (configuration & UI_CONFIG_BORDER_DRAW_NORMAL) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		Color UIDrawer::HandleColor(size_t configuration, const UIDrawConfig& config) const {
			if (configuration & UI_CONFIG_COLOR) {
				return *(Color*)config.GetParameter(UI_CONFIG_COLOR);
			}
			else {
				return color_theme.theme;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		const char* UIDrawer::HandleResourceIdentifier(const char* input, bool permanent_buffer) {
			constexpr size_t max_character_count = 1024;

			if (!permanent_buffer) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		const char* UIDrawer::HandleResourceIdentifier(const char* input, size_t& size, bool permanent_buffer) {
			constexpr size_t max_character_count = 1024;

			size = strnlen_s(input, max_character_count);
			if (!permanent_buffer) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleRectangleActions(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale) {
			if (configuration & UI_CONFIG_HOVERABLE_ACTION) {
				const UIActionHandler* handler = (const UIActionHandler*)config.GetParameter(UI_CONFIG_HOVERABLE_ACTION);
				AddHoverable(position, scale, *handler);
			}

			if (configuration & UI_CONFIG_CLICKABLE_ACTION) {
				const UIActionHandler* handler = (const UIActionHandler*)config.GetParameter(UI_CONFIG_CLICKABLE_ACTION);
				AddClickable(position, scale, *handler);
			}

			if (configuration & UI_CONFIG_GENERAL_ACTION) {
				const UIActionHandler* handler = (const UIActionHandler*)config.GetParameter(UI_CONFIG_GENERAL_ACTION);
				AddGeneral(position, scale, *handler);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextElement* UIDrawer::HandleTextCopyFromResource(size_t configuration, const char* text, float2& position, Color font_color, float2 (*scale_from_text)(float2 scale, const UIDrawer& drawer)) {
			void* text_info = GetResource(text);
			UIDrawerTextElement* info = (UIDrawerTextElement*)text_info;
			HandleTextCopyFromResource(configuration, info, position, font_color, scale_from_text);

			return info;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// if zoom differes between initialization and copying, then it will scale and will translate
		// as if it didn't change zoom; this is due to allow simple text to be copied and for bigger constructs
		// to determine their own change in translation needed to copy the element; since the buffer will reside
		// in cache because of the previous iteration, this should not have a big penalty on performance
		void UIDrawer::HandleTextCopyFromResource(
			size_t configuration,
			UIDrawerTextElement* element,
			float2& position,
			Color font_color,
			float2(*scale_from_text)(float2 scale, const UIDrawer& drawer)
		) {
			if (~configuration & UI_CONFIG_DO_NOT_FIT_SPACE) {
				HandleFitSpaceRectangle(configuration, position, *element->TextScale());
			}

			size_t* text_count = HandleTextSpriteCount(configuration);
			UISpriteVertex* current_buffer = HandleTextSpriteBuffer(configuration) + *text_count;

			HandleTextStreamColorUpdate(font_color, *element->TextStream());

			if (~configuration & UI_CONFIG_DISABLE_TRANSLATE_TEXT) {
				if (position.x != element->TextPosition()->x || position.y != element->TextPosition()->y) {
					TranslateText(position.x, position.y, element->TextPosition()->x, -element->TextPosition()->y, *element->TextStream());
					*element->TextPosition() = position;
				}
			}

			if (element->GetZoomX() == zoom_ptr->x && element->GetZoomY() == zoom_ptr->y) {
				if (ValidatePosition(configuration, *element->TextPosition(), *element->TextScale())) {
					memcpy(current_buffer, element->TextBuffer(), sizeof(UISpriteVertex) * *element->TextSize());
					*text_count += *element->TextSize();
				}
			}
			else {
				ScaleText(element, current_buffer - *text_count, text_count, zoom_ptr, font.character_spacing);
				memcpy(element->TextBuffer(), current_buffer, sizeof(UISpriteVertex) * *element->TextSize());
				element->TextScale()->x *= element->GetInverseZoomX() * zoom_ptr->x;
				element->TextScale()->y *= element->GetInverseZoomY() * zoom_ptr->y;
				element->SetZoomFactor(*zoom_ptr);
			}

			if (~configuration & UI_CONFIG_DO_NOT_FIT_SPACE) {
				float2 new_scale = scale_from_text({
					element->TextScale()->x * zoom_ptr->x * element->GetInverseZoomX(),
					element->TextScale()->y * zoom_ptr->y * element->GetInverseZoomY()
					}, *this);
				HandleFitSpaceRectangle(configuration, position, new_scale);
			}

			if (configuration & UI_CONFIG_TEXT_ALIGN_TO_ROW_Y) {
				float y_position = AlignMiddle(position.y, current_row_y_scale, element->TextScale()->y);
				TranslateTextY(y_position, 0, *element->TextStream());
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// cull_mode 0 - horizontal
		// cull_mode 1 - vertical
		// cull_mode 2 - horizontal main, vertical secondary
		// cull_mode 3 - vertical main, horizontal secondary
		UIDrawerTextElement* UIDrawer::HandleLabelTextCopyFromResourceWithCull(
			size_t configuration,
			const UIDrawConfig& config,
			const char* text,
			float2& position,
			float2 scale,
			float2& text_span,
			size_t& copy_count,
			size_t cull_mode
		) {
			void* text_info = GetResource(text);
			UIDrawerTextElement* info = (UIDrawerTextElement*)text_info;
			HandleLabelTextCopyFromResourceWithCull(configuration, config, info, position, scale, text_span, copy_count, cull_mode);

			return info;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// cull_mode 0 - horizontal
		// cull_mode 1 - vertical
		// cull_mode 2 - horizontal main, vertical secondary
		// cull_mode 3 - vertical main, horizontal secondary
		void UIDrawer::HandleLabelTextCopyFromResourceWithCull(
			size_t configuration,
			const UIDrawConfig& config,
			UIDrawerTextElement* info,
			float2& position,
			float2 scale,
			float2& text_span,
			size_t& copy_count,
			size_t cull_mode
		) {
			HandleFitSpaceRectangle(configuration, position, scale);

			bool memcpy_all = true;
			if (cull_mode == 0) {
				if (info->TextScale()->x > scale.x - 2 * element_descriptor.label_horizontal_padd) {
					memcpy_all = false;
				}
			}
			else if (cull_mode == 1) {
				if (info->TextScale()->y > scale.y - 2 * element_descriptor.label_vertical_padd) {
					memcpy_all = false;
				}
			}
			else if (cull_mode == 2 || cull_mode == 3) {
				if ((info->TextScale()->x > scale.x - 2 * element_descriptor.label_horizontal_padd) || (info->TextScale()->y > scale.y - 2 * element_descriptor.label_vertical_padd)) {
					memcpy_all = false;
				}
			}

			size_t* text_count = HandleTextSpriteCount(configuration);
			auto text_buffer = HandleTextSpriteBuffer(configuration);
			UISpriteVertex* current_buffer = text_buffer + *text_count;
			copy_count = *info->TextSize();

			float2 font_size;
			float character_spacing;
			Color font_color;
			HandleText(configuration, config, font_color, font_size, character_spacing);
			HandleTextStreamColorUpdate(font_color, *info->TextStream());

			Stream<UISpriteVertex> current_stream = Stream<UISpriteVertex>(current_buffer, copy_count);
			CapacityStream<UISpriteVertex> text_stream = *info->TextStream();
			TextAlignment horizontal_alignment, vertical_alignment;
			GetTextLabelAlignment(configuration, config, horizontal_alignment, vertical_alignment);

			auto memcpy_fnc = [&](unsigned int first_index, unsigned int second_index, bool vertical) {
				float x_position, y_position;
				TextAlignment dummy1, dummy2;
				HandleTextLabelAlignment(
					configuration,
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
			if (cull_mode == 0) {
				if (horizontal_alignment == TextAlignment::Right) {
					if (!memcpy_all) {
						CullTextSprites<1, 1>(text_stream, current_stream, text_stream[1].position.x - scale.x + 2 * element_descriptor.label_horizontal_padd);
						text_span = GetTextSpan<true, true>(current_stream);

						float x_position, y_position;
						HandleTextLabelAlignment(
							configuration,
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
						HandleTextLabelAlignment(
							configuration,
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
			else if (cull_mode == 1) {
				if (vertical_alignment == TextAlignment::Bottom) {
					if (!memcpy_all) {
						CullTextSprites<3, 1>(text_stream, current_stream, text_stream[text_stream.size - 3].position.y - scale.y + 2 * element_descriptor.label_vertical_padd);
						text_span = GetTextSpan<false, true>(current_stream);

						float x_position, y_position;
						HandleTextLabelAlignment(
							configuration,
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
						HandleTextLabelAlignment(
							configuration,
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
			else if (cull_mode == 2) {
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
							HandleTextLabelAlignment(
								configuration,
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
							HandleTextLabelAlignment(
								configuration,
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
			else if (cull_mode == 3) {
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
							HandleTextLabelAlignment(
								configuration,
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
							HandleTextLabelAlignment(
								configuration,
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::HandleFitSpaceRectangle(size_t configuration, float2& position, float2 scale) {
			if (~configuration & UI_CONFIG_DO_NOT_FIT_SPACE) {
				if (draw_mode == UIDrawerMode::FitSpace) {
					bool is_outside = VerifyFitSpaceRectangle(position, scale);
					if (is_outside) {
						NextRow();
						position = { current_x, current_y };
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleSliderActions(
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
		) {
			UIDrawPhase phase = HandlePhase(configuration);
			if (configuration & UI_CONFIG_SLIDER_DEFAULT_VALUE) {
				AddHoverable(position, scale, { SliderReturnToDefault, info, 0, phase });
			}
			if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
				UIDrawerSliderEnterValuesData enter_data;
				enter_data.slider = info;
				enter_data.convert_input = functions.convert_text_input;
				enter_data.filter_function = filter;
				AddGeneral(position, scale, { SliderEnterValues, &enter_data, sizeof(enter_data), phase });
			}
			if (~configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
				UIDrawerSliderBringToMouse bring_info;
				bring_info.slider = info;
				if (configuration & UI_CONFIG_VERTICAL) {
					bring_info.slider_length = slider_scale.y;
				}
				else {
					bring_info.slider_length = slider_scale.x;
				}

				if (info->text_input_counter == 0) {
					SolidColorRectangle(configuration, slider_position, slider_scale, slider_color);
					AddClickable(position, scale, { SliderBringToMouse, &bring_info, sizeof(bring_info), phase });
					AddClickable(slider_position, slider_scale, { SliderAction, info, 0, phase });
					AddDefaultHoverable(slider_position, slider_scale, slider_color, 1.25f, phase);
				}
			}
			else {
				if (info->text_input_counter == 0) {
					AddClickable(position, scale, { SliderMouseDraggable, &info, 8, phase });
					if (configuration & UI_CONFIG_SLIDER_DEFAULT_VALUE) {
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
					if (configuration & UI_CONFIG_SLIDER_DEFAULT_VALUE) {
						AddHoverable(position, scale, { SliderReturnToDefault, info, 0, phase });
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::HandleLabelSize(float2 text_span) const {
			float2 scale;
			scale = text_span;
			scale.x += 2 * element_descriptor.label_horizontal_padd;
			scale.y += 2 * element_descriptor.label_vertical_padd;
			return scale;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleSliderVariables(
			size_t configuration,
			const UIDrawConfig& config,
			float& length, float& padding,
			float2& shrink_factor,
			Color& slider_color,
			Color background_color
		) {
			if (configuration & UI_CONFIG_SLIDER_LENGTH) {
				length = *(const float*)config.GetParameter(UI_CONFIG_SLIDER_LENGTH);
			}
			else {
				if (configuration & UI_CONFIG_VERTICAL) {
					length = element_descriptor.slider_length.y;
				}
				else {
					length = element_descriptor.slider_length.x;
				}
			}

			if (configuration & UI_CONFIG_SLIDER_PADDING) {
				padding = *(const float*)config.GetParameter(UI_CONFIG_SLIDER_PADDING);
			}
			else {
				if (configuration & UI_CONFIG_VERTICAL) {
					padding = element_descriptor.slider_padding.y;
				}
				else {
					padding = element_descriptor.slider_padding.x;
				}
			}

			if (configuration & UI_CONFIG_SLIDER_SHRINK) {
				shrink_factor = *(const float2*)config.GetParameter(UI_CONFIG_SLIDER_SHRINK);
			}
			else {
				shrink_factor = element_descriptor.slider_shrink;
			}

			if (configuration & UI_CONFIG_SLIDER_COLOR) {
				slider_color = *(const Color*)config.GetParameter(UI_CONFIG_SLIDER_COLOR);
			}
			else {
				slider_color = ToneColor(background_color, color_theme.slider_lighten_factor);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SolidColorRectangle(size_t configuration, float2 position, float2 scale, Color color) {
			if (!initializer) {
				SetSolidColorRectangle(
					position,
					scale,
					color,
					HandleSolidColorBuffer(configuration),
					*HandleSolidColorCount(configuration)
				);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SolidColorRectangle(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale) {
			Color color = HandleColor(configuration, config);
			SolidColorRectangle(configuration, position, scale, color);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SolidColorRectangle(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale, Color color) {
			if (!initializer) {
				SolidColorRectangle(configuration, position, scale, color);

				HandleBorder(configuration, config, position, scale);
				HandleLateAndSystemDrawActionNullify(configuration, position, scale);
				HandleRectangleActions(configuration, config, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// it will finalize the rectangle
		void UIDrawer::TextLabel(size_t configuration, const UIDrawConfig& config, const char* text, float2& position, float2& scale) {
			Stream<UISpriteVertex> current_text = GetTextStream(configuration, ParseStringIdentifier(text, strlen(text)) * 6);
			float2 text_span;

			float2 temp_position = { position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd };
			Text(configuration | UI_CONFIG_DO_NOT_FIT_SPACE, config, text, temp_position, text_span);

			float2 label_scale = HandleLabelSize(text_span);

			if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) {
				scale.x = label_scale.x;
			}
			if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) {
				scale.y = label_scale.y;
			}

			if (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
				position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
			}

			float2 position_copy = position;
			bool is_moved = HandleFitSpaceRectangle(configuration, position, scale);

			TextAlignment horizontal_alignment = TextAlignment::Middle, vertical_alignment = TextAlignment::Top;
			float x_text_position, y_text_position;
			HandleTextLabelAlignment(
				configuration,
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

			if (configuration & UI_CONFIG_VERTICAL) {
				AlignVerticalText(current_text);
			}

			HandleBorder(configuration, config, position, scale);

			if (~configuration & UI_CONFIG_LABEL_TRANSPARENT) {
				Color label_color = HandleColor(configuration, config);
				SolidColorRectangle(
					configuration,
					position,
					scale,
					label_color
				);
			}

			HandleLateAndSystemDrawActionNullify(configuration, position, scale);

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// resource drawer
		void UIDrawer::TextLabel(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2& position, float2 scale) {
			if (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
				position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
			}
			float2 text_span = text->scale;

			bool is_moved = HandleFitSpaceRectangle(configuration, position, text_span);

			HandleBorder(configuration, config, position, scale);
			if (~configuration & UI_CONFIG_LABEL_TRANSPARENT) {
				Color label_color = HandleColor(configuration, config);
				SolidColorRectangle(
					configuration,
					position,
					scale,
					label_color
				);
			}

			HandleLateAndSystemDrawActionNullify(configuration, position, scale);

			float x_position, y_position;
			TextAlignment horizontal = TextAlignment::Left, vertical = TextAlignment::Top;
			HandleTextLabelAlignment(
				configuration,
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
			HandleText(configuration, config, font_color, font_size, character_spacing);

			HandleTextCopyFromResource(configuration | UI_CONFIG_DO_NOT_FIT_SPACE, text, position, font_color, ECS_TOOLS_UI_DRAWER_IDENTITY_SCALE);

			FinalizeRectangle(configuration, position, text_span);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Rectangle(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale) {
			HandleFitSpaceRectangle(configuration, position, scale);

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				if (configuration & UI_CONFIG_COLOR) {
					const Color* color = (const Color*)config.GetParameter(UI_CONFIG_COLOR);
					SolidColorRectangle(configuration, position, scale, *color);
				}
				else if (configuration & UI_CONFIG_RECTANGLE_VERTEX_COLOR) {
					const Color* colors = (const Color*)config.GetParameter(UI_CONFIG_RECTANGLE_VERTEX_COLOR);
					VertexColorRectangle(configuration, position, scale, *colors, *(colors + 1), *(colors + 2), *(colors + 3));
				}

				if (configuration & UI_CONFIG_TOOL_TIP) {
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

				HandleRectangleActions(configuration, config, position, scale);
				HandleBorder(configuration, config, position, scale);
			}
			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteClusterRectangle(
			size_t configuration,
			float2 position,
			float2 scale,
			LPCWSTR texture,
			float2 top_left_uv,
			float2 bottom_right_uv,
			Color color
		) {
			auto buffer = HandleSpriteClusterBuffer(configuration);
			auto count = HandleSpriteClusterCount(configuration);

			SetSpriteRectangle(position, scale, color, top_left_uv, bottom_right_uv, buffer, *count);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteClusterRectangle(
			size_t configuration,
			float2 position,
			float2 scale,
			LPCWSTR texture,
			const Color* colors,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			auto buffer = HandleSpriteClusterBuffer(configuration);
			auto count = HandleSpriteClusterCount(configuration);

			SetVertexColorSpriteRectangle(position, scale, colors, top_left_uv, bottom_right_uv, buffer, *count);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteClusterRectangle(
			size_t configuration,
			float2 position,
			float2 scale,
			LPCWSTR texture,
			const ColorFloat* colors,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			auto buffer = HandleSpriteClusterBuffer(configuration);
			auto count = HandleSpriteClusterCount(configuration);

			SetVertexColorSpriteRectangle(position, scale, colors, top_left_uv, bottom_right_uv, buffer, *count);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// resource drawer
		void UIDrawer::Text(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2 position) {
			float2 zoomed_scale = { text->scale.x * zoom_ptr->x * text->inverse_zoom.x, text->scale.y * zoom_ptr->y * text->inverse_zoom.y };
			float2 font_size;
			float character_spacing;
			Color font_color;
			HandleText(configuration, config, font_color, font_size, character_spacing);

			if (ValidatePosition(configuration, position, zoomed_scale)) {
				HandleTextCopyFromResource(configuration, text, position, font_color, ECS_TOOLS_UI_DRAWER_IDENTITY_SCALE);
			}

			FinalizeRectangle(configuration, position, zoomed_scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::TextLabelWithCull(size_t configuration, const UIDrawConfig& config, const char* text, float2 position, float2 scale) {
			if (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
				position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
			}

			Stream<UISpriteVertex> current_text = GetTextStream(configuration, ParseStringIdentifier(text, strlen(text)) * 6);
			TextAlignment horizontal_alignment, vertical_alignment;
			GetTextLabelAlignment(configuration, config, horizontal_alignment, vertical_alignment);

			const float* dependent_size = (const float*)config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
			const WindowSizeTransformType* type = (WindowSizeTransformType*)dependent_size;

			size_t text_count = ParseStringIdentifier(text, strlen(text));
			Color color;
			float2 font_size;
			float character_spacing;
			HandleText(configuration, config, color, font_size, character_spacing);
			float2 text_span;

			auto text_sprites = HandleTextSpriteBuffer(configuration);
			auto text_sprite_count = HandleTextSpriteCount(configuration);
			if (configuration & UI_CONFIG_VERTICAL) {
				if (vertical_alignment == TextAlignment::Bottom) {
					system->ConvertCharactersToTextSprites<false, true>(
						text,
						{ position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd },
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
						{ position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd },
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
			if (~configuration & UI_CONFIG_VERTICAL) {
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
			HandleTextLabelAlignment(
				configuration,
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

			if (configuration & UI_CONFIG_VERTICAL) {
				AlignVerticalText(current_text);
			}

			*text_sprite_count += vertex_count;
			if (~configuration & UI_CONFIG_LABEL_TRANSPARENT) {
				Color label_color = HandleColor(configuration, config);
				SolidColorRectangle(
					configuration,
					position,
					scale,
					label_color
				);
			}

			HandleBorder(configuration, config, position, scale);

			HandleLateAndSystemDrawActionNullify(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextElement* UIDrawer::TextInitializer(size_t configuration, const UIDrawConfig& config, const char* characters, float2 position) {
			UIDrawerTextElement* element;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(characters, [&](const char* label_identifier) {
				element = GetMainAllocatorBuffer<UIDrawerTextElement>();

				ConvertTextToWindowResource(configuration, config, label_identifier, element, position);

				if (~configuration & UI_CONFIG_DO_NOT_ADVANCE) {
					FinalizeRectangle(configuration, position, element->scale);
				}
				return element;
				});

			return element;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextElement* UIDrawer::TextLabelInitializer(size_t configuration, const UIDrawConfig& config, const char* characters, float2 position, float2 scale) {
			UIDrawerTextElement* element;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(characters, [&](const char* identifier) {
				TextAlignment horizontal_alignment, vertical_alignment;
				element = GetMainAllocatorBuffer<UIDrawerTextElement>();

				ConvertTextToWindowResource(configuration, config, identifier, element, { position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd });
				float x_position, y_position;
				if (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) {
						scale.x = element->scale.x + 2 * element_descriptor.label_horizontal_padd;
					}
					if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) {
						scale.y = element->scale.y + 2 * element_descriptor.label_vertical_padd;
					}

					if (((configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) != 0) && ((configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) != 0)) {
						scale.x = function::Select(scale.x < element->scale.x, element->scale.x, scale.x);
						scale.y = function::Select(scale.y < element->scale.y, element->scale.y, scale.y);
						/*scale.x += 2 * element_descriptor.label_horizontal_padd;
						scale.y += 2 * element_descriptor.label_vertical_padd;*/
					}
					HandleTextLabelAlignment(configuration, config, element->scale, scale, position, x_position, y_position, horizontal_alignment, vertical_alignment);
					TranslateText(x_position, y_position, element->text_vertices);
					element->position.x -= x_position - position.x;
					element->position.y -= y_position - position.y;
				}

				FinalizeRectangle(configuration, position, scale);
				return element;
				});

			return element;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::TextLabelDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* element, float2 position, float2 scale) {
			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				if (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
					position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
				}

				TextAlignment horizontal_alignment, vertical_alignment;
				GetTextLabelAlignment(configuration, config, horizontal_alignment, vertical_alignment);

				if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					const float* dependent_size = (const float*)config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
					const WindowSizeTransformType* type = (WindowSizeTransformType*)dependent_size;

					size_t copy_count;
					float2 text_span;
					Stream<UISpriteVertex> vertices = GetTextStream(configuration, 0);

					if (*type == WindowSizeTransformType::Horizontal) {
						HandleLabelTextCopyFromResourceWithCull(
							configuration,
							config,
							element,
							position,
							scale,
							text_span,
							copy_count,
							0
						);
					}
					else if (*type == WindowSizeTransformType::Vertical) {
						HandleLabelTextCopyFromResourceWithCull(
							configuration,
							config,
							element,
							position,
							scale,
							text_span,
							copy_count,
							1
						);
					}
					else if (*type == WindowSizeTransformType::Both) {
						HandleLabelTextCopyFromResourceWithCull(
							configuration,
							config,
							element,
							position,
							scale,
							text_span,
							copy_count,
							(configuration & UI_CONFIG_VERTICAL) != 0 ? 3 : 2
						);
					}
				}
				else {
					float2 font_size;
					float character_spacing;
					Color font_color;

					float2 label_scale = HandleLabelSize(element->scale);
					if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) {
						scale.x = label_scale.x;
					}
					if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) {
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
						if (configuration & UI_CONFIG_VERTICAL) {
							if (vertical_alignment == TextAlignment::Bottom) {
								TranslateText(x_position, y_position, element->text_vertices, element->text_vertices.size - 1, element->text_vertices.size - 3);
							}
							else {
								TranslateText(x_position, y_position, element->text_vertices, 0, 0);
							}
						}
						else {
							if (horizontal_alignment == TextAlignment::Right) {
								TranslateText(x_position, y_position, element->text_vertices, element->text_vertices.size - 1, element->text_vertices.size - 3);
							}
							else {
								TranslateText(x_position, y_position, element->text_vertices, 0, 0);
							}
						}

						element->position.x = x_position;
						element->position.y = y_position;
					}

					HandleText(configuration, config, font_color, font_size, character_spacing);
					HandleTextCopyFromResource(
						configuration | UI_CONFIG_DISABLE_TRANSLATE_TEXT,
						element,
						position,
						font_color,
						ECS_TOOLS_UI_DRAWER_LABEL_SCALE
					);
				}

				if (~configuration & UI_CONFIG_LABEL_TRANSPARENT) {
					Color label_color = HandleColor(configuration, config);
					SolidColorRectangle(
						configuration,
						position,
						scale,
						label_color
					);
					HandleBorder(configuration, config, position, scale);
				}
				HandleLateAndSystemDrawActionNullify(configuration, position, scale);
			}

			float2 font_size;
			float character_spacing;
			Color font_color;
			HandleText(configuration, config, font_color, font_size, character_spacing);
			HandleTextStreamColorUpdate(font_color, element->text_vertices);

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// single lined text input; drawer kernel
		void UIDrawer::TextInputDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerTextInput* input, float2 position, float2 scale, UIDrawerTextInputFilter filter) {
			if (IsElementNameFirst(configuration, UI_CONFIG_TEXT_INPUT_NO_NAME)) {
				ElementName(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, &input->name, position, scale);
				position.x = current_x - region_render_offset.x;
			}

			if (configuration & UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER) {
				if (!input->is_currently_selected) {
					int64_t integer = function::ConvertCharactersToInt<int64_t>(*input->text);
					input->DeleteAllCharacters();

					char temp_characters[256];
					CapacityStream<char> temp_stream(temp_characters, 0, 256);
					function::ConvertIntToCharsFormatted(temp_stream, integer);

					input->InsertCharacters(temp_characters, temp_stream.size, 0, system);
				}
			}

			HandleFitSpaceRectangle(configuration, position, scale);

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				bool is_active = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				auto text_stream = GetTextStream(configuration, input->vertices.size);
				UISpriteVertex* text_buffer = HandleTextSpriteBuffer(configuration);
				size_t* text_count = HandleTextSpriteCount(configuration);

				float2 font_size;
				float character_spacing;
				Color font_color;
				HandleText(configuration, config, font_color, font_size, character_spacing);
				if (!is_active) {
					font_color = color_theme.unavailable_text;
				}
				HandleTextStreamColorUpdate(font_color, input->vertices);
				input->text_color = font_color;

				bool is_x_zoom_different = input->current_zoom.x != zoom_ptr->x;
				bool is_y_zoom_different = input->current_zoom.y != zoom_ptr->y;
				if (input->vertices.size > 0) {
					if (input->position.x != position.x || input->position.y != position.y || is_x_zoom_different || is_y_zoom_different) {
						TranslateText(position.x + element_descriptor.text_input_padding.x, position.y + element_descriptor.text_input_padding.y, input->vertices, 0, 0);
						if (configuration & UI_CONFIG_TEXT_INPUT_HINT) {
							TranslateText(position.x + element_descriptor.text_input_padding.x, position.y + element_descriptor.text_input_padding.y, input->hint_vertices, 0, 0);
						}
					}
					ScaleText(input->vertices, input->current_zoom, input->inverse_zoom, text_buffer, text_count, zoom_ptr, font.character_spacing);
					unsigned int visible_sprites = input->GetVisibleSpriteCount(position.x + scale.x - input->padding.x);

					if (input->current_zoom.x != zoom_ptr->x || input->current_zoom.y != zoom_ptr->y) {
						memcpy(input->vertices.buffer, text_stream.buffer, input->vertices.size * sizeof(UISpriteVertex));

						if (configuration & UI_CONFIG_TEXT_INPUT_HINT) {
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
					if (configuration & UI_CONFIG_TEXT_INPUT_HINT) {
						if (input->position.x != position.x || input->position.y != position.y || is_x_zoom_different || is_y_zoom_different) {
							TranslateText(position.x + element_descriptor.text_input_padding.x, position.y + element_descriptor.text_input_padding.y, input->hint_vertices, 0, 0);
						}
					}
					if (is_x_zoom_different || is_y_zoom_different) {
						if (configuration & UI_CONFIG_TEXT_INPUT_HINT) {
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
				if (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					scale.y = input->solid_color_y_scale;
				}
				Color color = HandleColor(configuration, config);
				SolidColorRectangle(configuration, position, scale, color);

				if (is_active) {
					AddHoverable(position, scale, { TextInputHoverable, input, 0 });
					AddClickable(position, scale, { TextInputClickable, input, 0 });
					UIDrawerTextInputActionData input_data = { input, filter };
					AddGeneral(position, scale, { TextInputAction, &input_data, sizeof(input_data) });
				}


				if (input->current_selection != input->current_sprite_position) {
					Color select_color = ToneColor(color, color_theme.select_text_factor);
					float2 first_position;
					float2 last_position;

					if (input->current_selection < input->current_sprite_position) {
						first_position = input->GetPositionFromSprite(input->current_selection);
						last_position = input->GetPositionFromSprite<false>(input->current_sprite_position - 1);
					}
					else {
						first_position = input->GetPositionFromSprite(input->current_sprite_position);
						last_position = input->GetPositionFromSprite<false>(input->current_selection - 1);
					}

					if (last_position.x > input->bound) {
						last_position.x = input->bound;
					}
					if (first_position.x < position.x + input->padding.x) {
						first_position.x = position.x + input->padding.x;
					}

					SolidColorRectangle(configuration, first_position, { last_position.x - first_position.x, last_position.y - first_position.y }, select_color);
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

				HandleBorder(configuration, config, position, scale);
			}

			FinalizeRectangle(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, position, scale);

			if (IsElementNameAfter(configuration, UI_CONFIG_TEXT_INPUT_NO_NAME)) {
				ElementName(configuration, config, &input->name, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextInput* UIDrawer::TextInputInitializer(size_t configuration, UIDrawConfig& config, const char* name, CapacityStream<char>* text_to_fill, float2 position, float2 scale) {
			UIDrawerTextInput* element;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {

				float character_spacing;
				float2 font_size;
				Color font_color;
				UIConfigTextParameters previous_parameters;

				FitTextToScale(configuration, config, font_size, character_spacing, font_color, scale, element_descriptor.text_input_padding.y, previous_parameters);

				element = GetMainAllocatorBuffer<UIDrawerTextInput>();

				ConvertTextToWindowResource(
					configuration | UI_CONFIG_TEXT_PARAMETERS,
					config,
					identifier,
					&element->name,
					{ element_descriptor.text_input_padding.x, element_descriptor.text_input_padding.y }
				);

				FinishFitTextToScale(configuration, config, previous_parameters);

				element->caret_start = std::chrono::high_resolution_clock::now();
				element->key_repeat_start = std::chrono::high_resolution_clock::now();
				element->repeat_key_count = 0;
				element->is_caret_display = false;
				element->character_spacing = character_spacing;
				element->current_selection = 0;
				element->current_sprite_position = 0;
				element->solid_color_y_scale = system->GetTextSpriteYScale(font_size.y) + 2.0f * element_descriptor.text_input_padding.y;
				element->position = { 0.0f, 0.0f };
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

				if (configuration & UI_CONFIG_TEXT_INPUT_HINT) {
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

				if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
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

				FinalizeRectangle(configuration, position, scale);
				return element;
				});

			return element;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerSlider* UIDrawer::SliderInitializer(
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
		) {
			UIDrawerSlider* slider;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				Color font_color;
				float character_spacing;
				float2 font_size;
				UIConfigTextParameters previous_parameters;

				float slider_padding, slider_length;
				float2 slider_shrink;
				Color slider_color;
				// color here is useless
				HandleSliderVariables(configuration, config, slider_length, slider_padding, slider_shrink, slider_color, ECS_COLOR_WHITE);

				HandleText(configuration, config, font_color, font_size, character_spacing);

				float sprite_y_scale = system->GetTextSpriteYScale(font_size.y);

				slider = GetMainAllocatorBuffer<UIDrawerSlider>();

				float alignment;
				if (~configuration & UI_CONFIG_SLIDER_NO_NAME) {
					if (~configuration & UI_CONFIG_VERTICAL) {
						alignment = AlignMiddle(position.y, scale.y, sprite_y_scale);
						ConvertTextToWindowResource(configuration, config, identifier, &slider->label, { position.x + scale.x + slider_padding + slider_length, alignment });
					}
					else {
						ConvertTextToWindowResource(configuration, config, identifier, &slider->label, { position.x, position.y + scale.y + slider_padding + slider_length });
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

				if (configuration & UI_CONFIG_VERTICAL) {
					slider->is_vertical = true;
				}
				else {
					slider->is_vertical = false;
				}

				void* character_allocation = GetMainAllocatorBuffer(sizeof(char) * 32, 1);
				slider->characters.buffer = (char*)character_allocation;
				slider->characters.size = 0;
				slider->characters.capacity = 32;

				bool is_smaller = functions.is_smaller(value_to_modify, lower_bound);
				bool is_greater = functions.is_smaller(upper_bound, value_to_modify);
				if (is_smaller || is_greater) {
					slider->slider_position = 0.5f;
					functions.interpolate(lower_bound, upper_bound, value_to_modify, slider->slider_position);
				}
				else {
					slider->slider_position = functions.percentage(lower_bound, upper_bound, value_to_modify);
				}

				slider->text_input_counter = 0;
				slider->value_to_change = value_to_modify;

				if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
					const UIConfigSliderChangedValueCallback* callback = (const UIConfigSliderChangedValueCallback*)config.GetParameter(UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);
					slider->changed_value_callback = callback->handler;
				}
				else {
					slider->changed_value_callback = { nullptr, nullptr, 0 };
				}

				if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
					size_t name_size = strlen(identifier);

					UIConfigTextInputCallback previous_callback;
					UIConfigTextInputCallback input_callback;
					input_callback.handler = slider->changed_value_callback;
					if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
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

					if (~configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
						slider->text_input = TextInputInitializer(
							configuration | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN,
							config,
							stack_memory,
							&slider->characters,
							position,
							scale
						);
					}
					else {
						slider->text_input = TextInputInitializer(configuration | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN
							| UI_CONFIG_TEXT_INPUT_CALLBACK, config, stack_memory, &slider->characters, position, scale);
					}

					RemoveConfigParameter(configuration, UI_CONFIG_TEXT_INPUT_CALLBACK, config, previous_callback);
				}

				slider->value_byte_size = byte_size;
				void* allocation = GetMainAllocatorBuffer(slider->value_byte_size);
				if (~configuration & UI_CONFIG_SLIDER_DEFAULT_VALUE) {
					memcpy(allocation, value_to_modify, slider->value_byte_size);
				}
				else {
					memcpy(allocation, default_value, slider->value_byte_size);
				}
				slider->default_value = allocation;

				if (configuration & UI_CONFIG_VERTICAL) {
					slider->total_length_main_axis = scale.y + slider->TextScale()->y + slider_length + slider_padding;
					FinalizeRectangle(configuration, position, { scale.x, slider->total_length_main_axis });
				}
				else {
					slider->total_length_main_axis = scale.x + slider->TextScale()->x + slider_length + slider_padding;
					FinalizeRectangle(configuration, position, { slider->total_length_main_axis, scale.y });
				}


				return slider;
				}
			);
			return slider;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SliderDrawer(
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
		) {
			bool is_null_window_dependent_size = false;

			float slider_padding, slider_length;
			float2 slider_shrink;
			Color slider_color;

			Color color = HandleColor(configuration, config);
			HandleSliderVariables(configuration, config, slider_length, slider_padding, slider_shrink, slider_color, color);

			if (IsElementNameFirst(configuration, UI_CONFIG_SLIDER_NO_NAME)) {
				ElementName(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, &slider->label, position, scale);
				HandleTransformFlags(configuration, config, position, scale);
			}

			if (ValidatePosition(configuration, position)) {
				if (slider->interpolate_value) {
					functions.interpolate(lower_bound, upper_bound, value_to_modify, slider->slider_position);
				}
				else if (slider->character_value) {
					functions.convert_text_input(slider->characters, value_to_modify);
					bool is_greater = functions.is_smaller(upper_bound, value_to_modify);
					bool is_smaller = functions.is_smaller(value_to_modify, lower_bound);
					if (is_greater) {
						memcpy(value_to_modify, upper_bound, slider->value_byte_size);
					}
					else if (is_smaller) {
						memcpy(value_to_modify, lower_bound, slider->value_byte_size);
					}
				}
				else {
					bool is_greater = functions.is_smaller(upper_bound, value_to_modify);
					bool is_smaller = functions.is_smaller(value_to_modify, lower_bound);
					if (is_greater) {
						memcpy(value_to_modify, upper_bound, slider->value_byte_size);
					}
					else if (is_smaller) {
						memcpy(value_to_modify, lower_bound, slider->value_byte_size);
					}
					slider->slider_position = functions.percentage(lower_bound, upper_bound, value_to_modify);
				}

				WindowSizeTransformType* type = nullptr;
				float2 copy_position = position;

				if (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					if (configuration & UI_CONFIG_VERTICAL) {
						HandleFitSpaceRectangle(configuration, position, { scale.x, scale.y + 2 * slider_padding + slider_length });

						if (configuration & UI_CONFIG_GET_TRANSFORM) {
							UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
							*get_transform->position = position;
							*get_transform->scale = { scale.x, scale.y + 2 * slider_padding + slider_length };
						}
					}
					else {
						HandleFitSpaceRectangle(configuration, position, { scale.x + 2 * slider_padding + slider_length, scale.y });

						if (configuration & UI_CONFIG_GET_TRANSFORM) {
							UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
							*get_transform->position = position;
							*get_transform->scale = { scale.x + 2 * slider_padding + slider_length, scale.y };
						}
					}
				}
				else {
					if (configuration & UI_CONFIG_VERTICAL) {
						scale.y -= slider_padding + slider_length;
					}
					else {
						scale.x -= slider_padding + slider_length;
					}
					const float* window_transform = (const float*)config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
					type = (WindowSizeTransformType*)window_transform;
				}

				float2 old_scale = scale;

				if (configuration & UI_CONFIG_VERTICAL) {
					scale.y += slider_length;
				}
				else {
					scale.x += slider_length;
				}
				if (~configuration & UI_CONFIG_SLIDER_NO_TEXT) {
					auto text_label_lambda = [&]() {
						functions.to_string(slider->characters, value_to_modify, functions.extra_data);
						slider->characters[slider->characters.size] = '\0';
						FixedScaleTextLabel(
							configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_DO_NOT_NULLIFY_HOVERABLE_ACTION | UI_CONFIG_DO_NOT_NULLIFY_GENERAL_ACTION
							| UI_CONFIG_DO_NOT_NULLIFY_CLICKABLE_ACTION,
							config,
							slider->characters.buffer,
							position,
							scale
						);

						AddHoverable(position, scale, { SliderCopyPaste, slider, 0 });
					};
					if (~configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						text_label_lambda();
					}
					else {
						if (slider->text_input_counter == 0) {
							text_label_lambda();
						}
						else {
							if (slider->text_input_counter == 1) {
								functions.to_string(slider->characters, value_to_modify, functions.extra_data);
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
							TextInputDrawer(configuration | UI_CONFIG_TEXT_INPUT_NO_NAME, config, slider->text_input, position, scale, filter);
							Indent(-1.0f);
						}
					}
				}
				else {
					SolidColorRectangle(configuration, position, scale, color);
				}

				float2 slider_position = { 0.0f, 0.0f };
				float2 slider_scale = { -10.0f, -10.0f };
				if (~configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
					if (configuration & UI_CONFIG_VERTICAL) {
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

				HandleLateAndSystemDrawActionNullify(configuration, position, scale);

				if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					if (*type == WindowSizeTransformType::Both) {
						if (configuration & UI_CONFIG_VERTICAL) {
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
					if (configuration & UI_CONFIG_VERTICAL) {
						if (~configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
							if (slider_scale.y < scale.y - slider_padding) {
								HandleSliderActions(configuration, position, scale, color, slider_position, slider_scale, slider_color, slider, functions, filter);
							}
						}
					}
					else {
						if (slider_scale.x < scale.x - slider_padding) {
							HandleSliderActions(configuration, position, scale, color, slider_position, slider_scale, slider_color, slider, functions, filter);
						}
					}
				}
				else {
					HandleSliderActions(configuration, position, scale, color, slider_position, slider_scale, slider_color, slider, functions, filter);
				}

				if (configuration & UI_CONFIG_VERTICAL) {
					if (~configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
						slider->current_scale = { scale.x, scale.y - slider_length - slider_padding };
					}
					else {
						slider->current_scale = scale;
					}
					slider->current_position = { position.x, position.y + slider_padding };
				}
				else {
					if (~configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
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
					action_data.mouse = system->m_mouse;
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

			if (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				FinalizeRectangle(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, position, scale);
			}
			else {
				if (!is_null_window_dependent_size) {
					FinalizeRectangle(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, position, scale);
				}
			}
			if (IsElementNameAfter(configuration, UI_CONFIG_SLIDER_NO_NAME)) {
				ElementName(configuration, config, &slider->label, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SliderGroupDrawer(
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
		) {

			size_t LABEL_CONFIGURATION = configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
			if (IsElementNameFirst(configuration, UI_CONFIG_SLIDER_GROUP_NO_NAME)) {
				TextLabel(LABEL_CONFIGURATION | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, group_name);
				Indent(-1.0f);
			}

			bool has_pushed_stack = PushIdentifierStackStringPattern();
			PushIdentifierStack(group_name);

			for (size_t index = 0; index < count; index++) {
				UIDrawerSlider* slider = (UIDrawerSlider*)GetResource(names[index]);

				HandleTransformFlags(configuration, config, position, scale);
				const void* lower_bound;
				const void* upper_bound;
				if (configuration & UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS) {
					lower_bound = lower_bounds;
					upper_bound = upper_bounds;
				}
				else {
					lower_bound = function::OffsetPointer(lower_bounds, index * slider->value_byte_size);
					upper_bound = function::OffsetPointer(upper_bounds, index * slider->value_byte_size);
				}

				if (~configuration & UI_CONFIG_SLIDER_GROUP_NO_SUBNAMES) {
					SliderDrawer(
						configuration | UI_CONFIG_ELEMENT_NAME_FIRST,
						config,
						slider,
						position,
						scale,
						values_to_modify[index],
						lower_bound,
						upper_bound,
						functions,
						filter
					);
				}
				else {
					SliderDrawer(
						configuration | UI_CONFIG_SLIDER_NO_NAME,
						config,
						slider,
						position,
						scale,
						values_to_modify[index],
						lower_bound,
						upper_bound,
						functions,
						filter
					);
				}
			}

			if (has_pushed_stack) {
				PopIdentifierStack();
			}
			PopIdentifierStack();

			if (IsElementNameAfter(configuration, UI_CONFIG_SLIDER_GROUP_NO_NAME)) {
				TextLabel(LABEL_CONFIGURATION, config, group_name);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SliderGroupInitializer(
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
		) {
			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}

			size_t LABEL_CONFIGURATION = configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
				| UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			if (IsElementNameFirst(configuration, UI_CONFIG_SLIDER_GROUP_NO_NAME)) {
				TextLabel(LABEL_CONFIGURATION, config, group_name);
			}

			bool has_pushed_stack = PushIdentifierStackStringPattern();
			PushIdentifierStack(group_name);

			for (size_t index = 0; index < count; index++) {
				const void* lower_bound;
				const void* upper_bound;
				const void* default_value;

				size_t byte_offset = index * byte_size;
				if (configuration & UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS) {
					lower_bound = lower_bounds;
					upper_bound = upper_bounds;
					default_value = default_values;
				}
				else {
					lower_bound = function::OffsetPointer(lower_bounds, byte_offset);
					upper_bound = function::OffsetPointer(upper_bounds, byte_offset);
					if (default_values != nullptr) {
						default_value = function::OffsetPointer(default_values, byte_offset);
					}
				}

				if (~configuration & UI_CONFIG_SLIDER_GROUP_NO_SUBNAMES) {
					SliderInitializer(
						configuration | UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN,
						config,
						names[index],
						position,
						scale,
						byte_size,
						values_to_modify[index],
						lower_bound,
						upper_bound,
						default_value,
						functions
					);
				}
				else {
					SliderInitializer(
						configuration | UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN,
						config,
						names[index],
						position,
						scale,
						byte_size,
						values_to_modify[index],
						lower_bound,
						upper_bound,
						default_value,
						functions
					);
				}
			}

			if (has_pushed_stack) {
				PopIdentifierStack();
			}
			PopIdentifierStack();

			if (IsElementNameAfter(configuration, UI_CONFIG_SLIDER_GROUP_NO_NAME)) {
				TextLabel(LABEL_CONFIGURATION, config, group_name);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		UIDrawerSlider* UIDrawer::IntSliderInitializer(
			size_t configuration,
			UIDrawConfig& config,
			const char* name,
			float2 position,
			float2 scale,
			Integer* value_to_modify,
			Integer lower_bound,
			Integer upper_bound,
			Integer default_value
		) {
			UIDrawerSliderFunctions functions = UIDrawerGetIntSliderFunctions<Integer>();
			return SliderInitializer(configuration, config, name, position, scale, sizeof(Integer), value_to_modify, &lower_bound, &upper_bound, &default_value, functions);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(UIDrawerSlider*, UIDrawer::IntSliderInitializer<integer>, size_t, UIDrawConfig&, const char*, float2, float2, integer*, integer, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntSliderDrawer(
			size_t configuration,
			const UIDrawConfig& config,
			UIDrawerSlider* slider,
			float2 position,
			float2 scale,
			Integer* value_to_modify,
			Integer lower_bound,
			Integer upper_bound
		) {
			UIDrawerSliderFunctions functions = UIDrawerGetIntSliderFunctions<Integer>();
			SliderDrawer(configuration, config, slider, position, scale, value_to_modify, &lower_bound, &upper_bound, functions, UIDrawerTextInputFilterDigits);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntSliderDrawer<integer>, size_t, const UIDrawConfig&, UIDrawerSlider*, float2, float2, integer*, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerSlider* UIDrawer::FloatSliderInitializer(
			size_t configuration,
			UIDrawConfig& config,
			const char* name,
			float2 position,
			float2 scale,
			float* value_to_modify,
			float lower_bound,
			float upper_bound,
			float default_value,
			unsigned int precision
		) {
			UIDrawerSliderFunctions functions = UIDrawerGetFloatSliderFunctions(precision);
			return SliderInitializer(configuration, config, name, position, scale, sizeof(float), value_to_modify, &lower_bound, &upper_bound, &default_value, functions);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatSliderDrawer(
			size_t configuration,
			const UIDrawConfig& config,
			UIDrawerSlider* slider,
			float2 position,
			float2 scale,
			float* value_to_modify,
			float lower_bound,
			float upper_bound,
			unsigned int precision
		) {
			UIDrawerSliderFunctions functions = UIDrawerGetFloatSliderFunctions(precision);
			SliderDrawer(configuration, config, slider, position, scale, value_to_modify, &lower_bound, &upper_bound, functions, UIDrawerTextInputFilterNumbers);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerSlider* UIDrawer::DoubleSliderInitializer(
			size_t configuration,
			UIDrawConfig& config,
			const char* name,
			float2 position,
			float2 scale,
			double* value_to_modify,
			double lower_bound,
			double upper_bound,
			double default_value,
			unsigned int precision
		) {
			UIDrawerSliderFunctions functions = UIDrawerGetDoubleSliderFunctions(precision);
			return SliderInitializer(configuration, config, name, position, scale, sizeof(double), value_to_modify, &lower_bound, &upper_bound, &default_value, functions);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleSliderDrawer(
			size_t configuration,
			const UIDrawConfig& config,
			UIDrawerSlider* slider,
			float2 position,
			float2 scale,
			double* value_to_modify,
			double lower_bound,
			double upper_bound,
			unsigned int precision
		) {
			UIDrawerSliderFunctions functions = UIDrawerGetDoubleSliderFunctions(precision);
			SliderDrawer(configuration, config, slider, position, scale, value_to_modify, &lower_bound, &upper_bound, functions, UIDrawerTextInputFilterNumbers);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerComboBox* UIDrawer::ComboBoxInitializer(
			size_t configuration,
			UIDrawConfig& config,
			const char* name,
			Stream<const char*> labels,
			unsigned int label_display_count,
			unsigned char* active_label
		) {
			UIDrawerComboBox* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				float2 position = { 0.0f, 0.0f };

				data = GetMainAllocatorBuffer<UIDrawerComboBox>();

				data->is_opened = false;
				data->active_label = active_label;
				data->label_display_count = label_display_count;

				InitializeElementName(configuration, UI_CONFIG_COMBO_BOX_NO_NAME, config, identifier, &data->name, position);

				void* allocation = GetMainAllocatorBuffer(sizeof(UIDrawerTextElement) * labels.size);

				data->labels.buffer = (UIDrawerTextElement*)allocation;
				data->labels.size = labels.size;

				if (configuration & UI_CONFIG_COMBO_BOX_PREFIX) {
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
					data->prefix_x_scale = 0.0f;
				}

				float current_max_x = 0.0f;
				for (size_t index = 0; index < labels.size; index++) {
					ConvertTextToWindowResource(configuration, config, labels[index], &data->labels[index], { position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd });

					position.y += data->labels[index].scale.y + 2 * element_descriptor.label_vertical_padd;
					if (data->labels[index].scale.x + 2 * element_descriptor.label_horizontal_padd > current_max_x) {
						data->biggest_label_x_index = index;
						current_max_x = data->labels[index].scale.x + 2 * element_descriptor.label_horizontal_padd;
					}
				}

				if (configuration & UI_CONFIG_COMBO_BOX_CALLBACK) {
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

				return data;
				});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextElement* UIDrawer::CheckBoxInitializer(size_t configuration, const UIDrawConfig& config, const char* name) {
			UIDrawerTextElement* element = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				element = GetMainAllocatorBuffer<UIDrawerTextElement>();

				InitializeElementName(configuration, UI_CONFIG_CHECK_BOX_NO_NAME, config, identifier, element, { 0.0f, 0.0f });

				return element;
				});

			return element;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::CheckBoxDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* element, bool* value_to_modify, float2 position, float2 scale) {
			if (IsElementNameFirst(configuration, UI_CONFIG_CHECK_BOX_NO_NAME)) {
				ElementName(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, element, position, scale);
				HandleTransformFlags(configuration, config, position, scale);
			}

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				bool state = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					state = active_state->state;
				}

				Color color = HandleColor(configuration, config);
				if (!state) {
					SolidColorRectangle(configuration, position, scale, DarkenColor(color, color_theme.darken_inactive_item));
				}
				else {
					SolidColorRectangle(configuration, position, scale, color);
					if (*value_to_modify) {
						Color check_color = ToneColor(color, color_theme.check_box_factor);
						SpriteRectangle(configuration, position, scale, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, check_color);
					}
					AddDefaultClickableHoverable(position, scale, { BoolClickable, value_to_modify, 0 }, color);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
				}

			}

			FinalizeRectangle(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, position, scale);

			if (IsElementNameAfter(configuration, UI_CONFIG_CHECK_BOX_NO_NAME)) {
				ElementName(configuration, config, element, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::CheckBoxDrawer(size_t configuration, const UIDrawConfig& config, const char* name, bool* value_to_modify, float2 position, float2 scale) {
			if (IsElementNameFirst(configuration, UI_CONFIG_CHECK_BOX_NO_NAME)) {
				ElementName(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, name, position, scale);
				HandleTransformFlags(configuration, config, position, scale);
			}

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				bool state = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					state = active_state->state;
				}

				Color color = HandleColor(configuration, config);
				if (!state) {
					SolidColorRectangle(configuration, position, scale, DarkenColor(color, color_theme.darken_inactive_item));
				}
				else {
					if (*value_to_modify) {
						Color check_color = ToneColor(color, color_theme.check_box_factor);
						SpriteRectangle(configuration, position, scale, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, check_color, { 0.0f, 0.0f }, { 1.0f, 1.0f });
					}

					SolidColorRectangle(configuration, position, scale, color);
					AddDefaultClickableHoverable(position, scale, { BoolClickable, value_to_modify, 0 }, color);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
				}

			}

			FinalizeRectangle(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, position, scale);

			if (IsElementNameAfter(configuration, UI_CONFIG_CHECK_BOX_NO_NAME)) {
				ElementName(configuration, config, name, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ComboBoxDrawer(size_t configuration, UIDrawConfig& config, UIDrawerComboBox* data, unsigned char* active_label, float2 position, float2 scale) {
			bool is_active = true;
			if (configuration & UI_CONFIG_ACTIVE_STATE) {
				const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				is_active = active_state->state;
			}

			if (IsElementNameFirst(configuration, UI_CONFIG_COMBO_BOX_NO_NAME)) {
				ElementName(configuration, config, &data->name, position, scale);
				HandleTransformFlags(configuration, config, position, scale);
			}

			UIConfigTextAlignment previous_alignment;
			UIConfigTextAlignment alignment;
			alignment.horizontal = TextAlignment::Left;
			alignment.vertical = TextAlignment::Middle;

			SetConfigParameter(configuration, UI_CONFIG_TEXT_ALIGNMENT, config, alignment, previous_alignment);

			float2 border_position = position;
			float2 border_scale = scale;

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			float prefix_scale = 0.0f;
			if (ValidatePosition(configuration, position, scale)) {
				if (configuration & UI_CONFIG_COMBO_BOX_PREFIX) {
					Stream<UISpriteVertex> vertices = GetTextStream(configuration, strlen(data->prefix) * 6);
					size_t new_configuration = function::ClearFlag(function::ClearFlag(configuration, UI_CONFIG_BORDER), UI_CONFIG_GET_TRANSFORM)
						| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_CACHE;

					new_configuration |= function::Select<size_t>(is_active, 0, UI_CONFIG_UNAVAILABLE_TEXT);

					TextLabel(new_configuration, config, data->prefix, position, scale);
					float2 vertex_span = GetTextSpan(vertices);
					prefix_scale = vertex_span.x;
					position.x += prefix_scale;
					scale.x -= prefix_scale;
					TextLabelDrawer(new_configuration | UI_CONFIG_LABEL_TRANSPARENT, config, &data->labels[*data->active_label], position, scale);

					// Restore the position and scale for the collapsing header
					position.x -= prefix_scale;
					scale.x += prefix_scale;
				}
				else {
					size_t new_configuration = function::ClearFlag(configuration, UI_CONFIG_GET_TRANSFORM) | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X
						| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_TEXT_ALIGNMENT;

					new_configuration |= function::Select<size_t>(is_active, 0, UI_CONFIG_UNAVAILABLE_TEXT);
					TextLabelDrawer(new_configuration, config, &data->labels[*data->active_label], position, scale);
				}

				float2 positions[2];
				float2 scales[2];
				Color colors[2];
				float percentages[2];
				positions[0] = position;
				scales[0] = scale;

				Color color = HandleColor(configuration, config);
				colors[0] = color;

				color = ToneColor(color, 1.35f);
				colors[1] = color;
				percentages[0] = 1.25f;
				percentages[1] = 1.25f;

				size_t no_get_transform_configuration = function::ClearFlag(configuration, UI_CONFIG_GET_TRANSFORM);

				float2 triangle_scale = GetSquareScale(scale.y);
				float2 triangle_position = { position.x + scale.x - triangle_scale.x, position.y };
				SolidColorRectangle(no_get_transform_configuration, triangle_position, triangle_scale, color);

				positions[1] = triangle_position;
				scales[1] = triangle_scale;

				if (is_active) {
					SpriteRectangle(no_get_transform_configuration, triangle_position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE);

					UIDrawerComboBoxClickable clickable_data;
					clickable_data.config = config;
					clickable_data.box = data;
					clickable_data.configuration = configuration;
					data->label_y_scale = scale.y;

					AddDefaultHoverable(position, scale, positions, scales, colors, percentages, 2);
					AddClickable(position, scale, { ComboBoxClickable, &clickable_data, sizeof(clickable_data), UIDrawPhase::System });

					if (data->is_opened) {
						if (data->window_index != -1) {
							if (!data->has_been_destroyed) {
								float2 window_position = { position.x, position.y + scale.y + element_descriptor.combo_box_padding };

								// only y axis needs to be handled since to move on the x axis
								// the left mouse button must be pressed
								float2 window_scale = { scale.x, scale.y * data->label_display_count };
								//system->TrimPopUpWindow(data->window_index, window_position, window_scale, window_index, region_position, region_scale);
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
					SpriteRectangle(no_get_transform_configuration, triangle_position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, color_theme.unavailable_text);
				}

				if (configuration & UI_CONFIG_COMBO_BOX_PREFIX) {
					HandleBorder(configuration, config, border_position, border_scale);
				}

				RemoveConfigParameter(configuration, UI_CONFIG_TEXT_ALIGNMENT, config, previous_alignment);
			}

			FinalizeRectangle(configuration, position, scale);

			if (IsElementNameAfter(configuration, UI_CONFIG_COMBO_BOX_NO_NAME)) {
				ElementName(configuration, config, &data->name, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerColorInput* UIDrawer::ColorInputInitializer(
			size_t configuration,
			UIDrawConfig& config,
			const char* name,
			Color* color,
			Color default_color,
			float2 position,
			float2 scale
		) {
			UIDrawerColorInput* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {

				data = GetMainAllocatorBuffer<UIDrawerColorInput>();

				InitializeElementName(configuration, UI_CONFIG_COLOR_INPUT_NO_NAME, config, identifier, &data->name, position);
				data->hsv = RGBToHSV(*color);
				data->rgb = color;
				if (~configuration & UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE) {
					data->default_color = *color;
				}
				else {
					data->default_color = default_color;
				}

				unsigned int name_size = strlen(name);

				size_t slider_configuration = configuration | UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
				slider_configuration |= function::Select<size_t>(configuration & UI_CONFIG_SLIDER_ENTER_VALUES, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK, 0);

				if (configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) {
					UIConfigSliderChangedValueCallback previous_callback;
					if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						UIConfigSliderChangedValueCallback callback;

						UIDrawerColorInputSliderCallback callback_data;
						callback_data.input = data;
						callback_data.is_rgb = true;
						callback_data.is_alpha = false;
						callback_data.is_hsv = false;

						callback.handler.action = ColorInputSliderCallback;
						callback.handler.data = &callback_data;
						callback.handler.data_size = sizeof(callback_data);

						if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
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

					data->r_slider = IntSliderInitializer(
						slider_configuration,
						config,
						stack_memory,
						position,
						scale,
						&color->red,
						(unsigned char)0,
						(unsigned char)255
					);
					stack_memory[name_size] = '\0';
					strcat(stack_memory, "Green");
					ECS_ASSERT(name_size + 5 < 128);

					HandleTransformFlags(configuration, config, position, scale);
					data->g_slider = IntSliderInitializer(
						slider_configuration,
						config,
						stack_memory,
						position,
						scale,
						&color->green,
						(unsigned char)0,
						(unsigned char)255
					);

					stack_memory[name_size] = '\0';
					strcat(stack_memory, "Blue");

					HandleTransformFlags(configuration, config, position, scale);
					data->b_slider = IntSliderInitializer(
						slider_configuration,
						config,
						stack_memory,
						position,
						scale,
						&color->blue,
						(unsigned char)0,
						(unsigned char)255
					);

					if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
							config.RestoreFlag(previous_callback, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);
						}
						else {
							config.flag_count--;
						}
					}
				}

				if (configuration & UI_CONFIG_COLOR_INPUT_HSV_SLIDERS) {
					UIConfigSliderChangedValueCallback previous_callback;
					if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						UIConfigSliderChangedValueCallback callback;

						UIDrawerColorInputSliderCallback callback_data;
						callback_data.input = data;
						callback_data.is_rgb = false;
						callback_data.is_alpha = false;
						callback_data.is_hsv = true;

						callback.handler.action = ColorInputSliderCallback;
						callback.handler.data = &callback_data;
						callback.handler.data_size = sizeof(callback_data);

						if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
							config.SetExistingFlag(callback, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK, previous_callback);
						}
						else {
							config.AddFlag(callback);
						}
					}

					HandleTransformFlags(configuration, config, position, scale);
					char stack_memory[128];
					memcpy(stack_memory, name, name_size);
					stack_memory[name_size] = '\0';
					strcat(stack_memory, "Hue");

					data->h_slider = IntSliderInitializer(
						slider_configuration,
						config,
						stack_memory,
						position,
						scale,
						&data->hsv.hue,
						(unsigned char)0,
						(unsigned char)255
					);
					stack_memory[name_size] = '\0';
					strcat(stack_memory, "Saturation");
					ECS_ASSERT(name_size + std::size("Saturation") < 128);

					HandleTransformFlags(configuration, config, position, scale);
					data->s_slider = IntSliderInitializer(
						slider_configuration,
						config,
						stack_memory,
						position,
						scale,
						&data->hsv.saturation,
						(unsigned char)0,
						(unsigned char)255
					);

					stack_memory[name_size] = '\0';
					strcat(stack_memory, "Value");

					HandleTransformFlags(configuration, config, position, scale);
					data->v_slider = IntSliderInitializer(
						slider_configuration,
						config,
						stack_memory,
						position,
						scale,
						&data->hsv.value,
						(unsigned char)0,
						(unsigned char)255
					);

					if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
							config.RestoreFlag(previous_callback, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);
						}
						else {
							config.flag_count--;
						}
					}
				}

				if (configuration & UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER) {
					UIConfigSliderChangedValueCallback previous_callback;
					if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						UIConfigSliderChangedValueCallback callback;

						UIDrawerColorInputSliderCallback callback_data;
						callback_data.input = data;
						callback_data.is_rgb = false;
						callback_data.is_alpha = true;
						callback_data.is_hsv = false;

						callback.handler.action = ColorInputSliderCallback;
						callback.handler.data = &callback_data;
						callback.handler.data_size = sizeof(callback_data);

						if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
							config.SetExistingFlag(callback, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK, previous_callback);
						}
						else {
							config.AddFlag(callback);
						}
					}

					HandleTransformFlags(configuration, config, position, scale);
					char stack_memory[128];
					memcpy(stack_memory, name, name_size);
					stack_memory[name_size] = '\0';
					strcat(stack_memory, "Alpha");
					stack_memory[name_size + 5] = '\0';
					data->a_slider = IntSliderInitializer(
						slider_configuration,
						config,
						stack_memory,
						position,
						scale,
						&color->alpha,
						(unsigned char)0,
						(unsigned char)255
					);

					if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
							config.RestoreFlag(previous_callback, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);
						}
						else {
							config.flag_count--;
						}
					}
				}

				if (configuration & UI_CONFIG_COLOR_INPUT_HEX_INPUT) {
					data->hex_characters.buffer = (char*)GetMainAllocatorBuffer(sizeof(char) * 7);
					data->hex_characters.size = 0;
					data->hex_characters.capacity = 6;

					HandleTransformFlags(configuration, config, position, scale);
					char stack_memory[256];
					memcpy(stack_memory, name, name_size);
					stack_memory[name_size] = '\0';
					strcat(stack_memory, "HexInput");
					stack_memory[name_size + std::size("HexInput")] = '\0';
					ECS_ASSERT(name_size + std::size("HexInput") < 256);

					UIConfigTextInputCallback callback;
					callback.handler.action = ColorInputHexCallback;
					callback.handler.data = data;
					callback.handler.data_size = 0;

					config.AddFlag(callback);
					data->hex_input = TextInputInitializer(
						configuration | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN | UI_CONFIG_TEXT_INPUT_CALLBACK,
						config,
						stack_memory,
						&data->hex_characters,
						position,
						scale
					);
					config.flag_count--;
				}
				else {
					data->hex_characters.buffer = nullptr;
				}
				data->is_hex_input = false;

				if (configuration & UI_CONFIG_COLOR_INPUT_CALLBACK) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ColorInputDrawer(size_t configuration, UIDrawConfig& config, UIDrawerColorInput* data, float2 position, float2 scale, Color* color) {
			Color initial_frame_color = *color;

			UIDrawConfig label_config;

			Color text_color;
			float2 font_size;
			float character_spacing;
			HandleText(configuration, config, text_color, font_size, character_spacing);

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
			if (configuration & UI_CONFIG_ACTIVE_STATE) {
				const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				is_active = active_state->state;
			}

			size_t LABEL_CONFIGURATION = UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_TRANSPARENT \
				| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
			LABEL_CONFIGURATION |= function::Select<size_t>(is_active, 0, UI_CONFIG_UNAVAILABLE_TEXT);

			size_t SLIDER_CONFIGURATION = configuration | UI_CONFIG_SLIDER_NO_NAME;
			SLIDER_CONFIGURATION |= function::Select<size_t>(~configuration & UI_CONFIG_SLIDER_ENTER_VALUES, 0, UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);

			auto callback_lambda = [&](bool is_rgb, bool is_hsv, bool is_alpha) {
				if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
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
				if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
					config.flag_count--;
				}
			};

			auto rgb_lambda = [&]() {
				Indent(-1.0f);
				HandleTransformFlags(configuration, config, position, scale);
				TextLabel(LABEL_CONFIGURATION, label_config, "R:", position, scale);

				callback_lambda(true, false, false);

				Indent(-1.0f);
				HandleTransformFlags(LABEL_CONFIGURATION, config, position, scale);
				starting_point = position;
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->r_slider, position, scale, &color->red, (unsigned char)0, (unsigned char)255);

				Indent(-1.0f);
				HandleTransformFlags(configuration, config, position, scale);
				TextLabel(LABEL_CONFIGURATION, label_config, "G:", position, scale);

				Indent(-1.0f);
				HandleTransformFlags(configuration, config, position, scale);
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->g_slider, position, scale, &color->green, (unsigned char)0, (unsigned char)255);

				Indent(-1.0f);
				HandleTransformFlags(configuration, config, position, scale);
				TextLabel(LABEL_CONFIGURATION, label_config, "B:", position, scale);

				Indent(-1.0f);
				HandleTransformFlags(configuration, config, position, scale);
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->b_slider, position, scale, &color->blue, (unsigned char)0, (unsigned char)255);
				end_point = { current_x - region_render_offset.x - layout.element_indentation, position.y };

				remove_callback_lambda();
			};

			auto hsv_lambda = [&]() {
				Indent(-1.0f);
				HandleTransformFlags(configuration, config, position, scale);
				TextLabel(LABEL_CONFIGURATION, label_config, "H:", position, scale);

				callback_lambda(false, true, false);

				Indent(-1.0f);
				HandleTransformFlags(configuration, config, position, scale);
				starting_point = position;
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->h_slider, position, scale, &data->hsv.hue, (unsigned char)0, (unsigned char)255);

				Indent(-1.0f);
				HandleTransformFlags(configuration, config, position, scale);
				TextLabel(LABEL_CONFIGURATION, label_config, "S:", position, scale);

				Indent(-1.0f);
				HandleTransformFlags(configuration, config, position, scale);
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->s_slider, position, scale, &data->hsv.saturation, (unsigned char)0, (unsigned char)255);

				Indent(-1.0f);
				HandleTransformFlags(configuration, config, position, scale);
				TextLabel(LABEL_CONFIGURATION, label_config, "V:", position, scale);

				Indent(-1.0f);
				HandleTransformFlags(configuration, config, position, scale);
				alpha_endpoint = { position.x + scale.x, position.y };
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->v_slider, position, scale, &data->hsv.value, (unsigned char)0, (unsigned char)255);

				if (configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) {
					if (data->r_slider->interpolate_value || data->g_slider->interpolate_value || data->b_slider->interpolate_value || data->is_hex_input) {
						data->hsv = RGBToHSV(*color);
						data->is_hex_input = false;
					}
					else if (data->h_slider->interpolate_value || data->s_slider->interpolate_value || data->v_slider->interpolate_value) {
						*color = HSVToRGB(data->hsv);
					}
				}
				else {
					*color = HSVToRGB(data->hsv);
				}

				remove_callback_lambda();
			};

			auto alpha_lambda = [&]() {
				callback_lambda(false, false, true);

				if (((configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) == 0) && ((configuration & UI_CONFIG_COLOR_INPUT_HSV_SLIDERS) == 0)) {
					Indent(-1.0f);
					HandleTransformFlags(configuration, config, position, scale);
					TextLabel(LABEL_CONFIGURATION, label_config, "A:", position, scale);
					Indent(-1.0f);

					HandleTransformFlags(configuration, config, position, scale);
					IntSliderDrawer(SLIDER_CONFIGURATION, config, data->a_slider, position, scale, &color->alpha, (unsigned char)0, (unsigned char)255);
				}
				else {
					NextRow();
					SetCurrentX(overall_start_position.x + region_render_offset.x);
					Indent(-1.0f);
					HandleTransformFlags(configuration, config, position, scale);
					TextLabel(LABEL_CONFIGURATION, label_config, "A:", position, scale);
					Indent(-1.0f);

					HandleTransformFlags(configuration, config, position, scale);
					scale.x = alpha_endpoint.x - starting_point.x - layout.element_indentation - system->NormalizeHorizontalToWindowDimensions(scale.y);
					IntSliderDrawer(SLIDER_CONFIGURATION, config, data->a_slider, position, scale, &color->alpha, (unsigned char)0, (unsigned char)255);
				}


				if (data->a_slider->interpolate_value) {
					data->hsv.alpha = color->alpha;
				}

				remove_callback_lambda();
			};

			auto hex_lambda = [&]() {
				if (((configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) != 0) || (configuration & UI_CONFIG_COLOR_INPUT_HSV_SLIDERS != 0) || (configuration & UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER != 0)) {
					NextRow();
					SetCurrentX(overall_start_position.x + region_render_offset.x);

					HandleTransformFlags(configuration, config, position, scale);
				}

				UIConfigTextInputCallback callback;
				callback.handler.action = ColorInputHexCallback;
				callback.handler.data = data;
				callback.handler.data_size = 0;

				config.AddFlag(callback);

				if (!data->hex_input->is_currently_selected) {
					char hex_characters[6];
					Stream<char> temp(hex_characters, 0);
					RGBToHex(temp, *data->rgb);
					data->hex_input->DeleteAllCharacters();
					data->hex_input->InsertCharacters(hex_characters, 6, 0, GetSystem());
				}

				TextInputDrawer(
					configuration | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_TEXT_INPUT_CALLBACK,
					config,
					data->hex_input,
					position,
					{ end_point.x - overall_start_position.x, scale.y },
					UIDrawerTextInputHexFilter
				);
			};

			if (configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) {
				rgb_lambda();
			}

			if (configuration & UI_CONFIG_COLOR_INPUT_HSV_SLIDERS) {
				if (configuration & UI_CONFIG_COLOR_INPUT_RGB_SLIDERS) {
					NextRow();
					SetCurrentX(overall_start_position.x + region_render_offset.x);
				}
				hsv_lambda();
			}

			if (configuration & UI_CONFIG_COLOR_INPUT_ALPHA_SLIDER) {
				alpha_lambda();
			}

			if (~configuration & UI_CONFIG_COLOR_INPUT_DO_NOT_CHOOSE_COLOR) {
				HandleTransformFlags(configuration, config, position, scale);
			}

			AlphaColorRectangle(configuration | UI_CONFIG_MAKE_SQUARE | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, *color);

			if (is_active) {
				if (~configuration & UI_CONFIG_COLOR_INPUT_DO_NOT_CHOOSE_COLOR) {
					AddDefaultClickable(position, GetSquareScale(scale.y), { SkipAction, nullptr, 0 }, { ColorInputCreateWindow, data, 0, UIDrawPhase::System });
				}
				if (configuration & UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE) {
					AddHoverable(position, GetSquareScale(scale.y), { ColorInputDefaultColor, data, 0 });
				}
			}

			if (IsElementNameAfter(configuration, UI_CONFIG_COLOR_INPUT_NO_NAME)) {
				HandleTransformFlags(configuration, config, position, scale);
				ElementName(configuration, config, &data->name, position, scale);
			}

			if (configuration & UI_CONFIG_COLOR_INPUT_HEX_INPUT) {
				hex_lambda();
			}

		}

		UIDrawerCollapsingHeader* UIDrawer::CollapsingHeaderInitializer(size_t configuration, const UIDrawConfig& config, const char* name, float2 position, float2 scale) {
			UIDrawerCollapsingHeader* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				data = GetMainAllocatorBuffer<UIDrawerCollapsingHeader>();

				if (IsElementNameAfter(configuration, UI_CONFIG_COLLAPSING_HEADER_NO_NAME) || IsElementNameFirst(configuration, UI_CONFIG_COLLAPSING_HEADER_NO_NAME)) {
					ConvertTextToWindowResource(configuration, config, identifier, &data->name, position);
				}

				data->state = false;
				return data;
				});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::CollapsingHeaderDrawer(size_t configuration, UIDrawConfig& config, UIDrawerCollapsingHeader* data, float2 position, float2 scale) {
			if (~configuration & UI_CONFIG_COLLAPSING_HEADER_DO_NOT_INFER) {
				scale.x = GetXScaleUntilBorder(position.x);
			}

			float total_x_scale = 0.0f;
			float initial_position_x = position.x;
			if (ValidatePosition(configuration, position, scale)) {
				bool active_state = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* _active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					active_state = _active_state->state;
				}
				bool state = data->state;
				state &= active_state;

				bool is_selected = true;
				if (configuration & UI_CONFIG_COLLAPSING_HEADER_SELECTION) {
					const UIConfigCollapsingHeaderSelection* selection = (const UIConfigCollapsingHeaderSelection*)config.GetParameter(UI_CONFIG_COLLAPSING_HEADER_SELECTION);
					is_selected = selection->is_selected;
				}

				Color current_color = HandleColor(configuration, config);
				UIConfigColor previous_color = { current_color };
				float2 triangle_scale = GetSquareScale(scale.y);
				float2 initial_position = position;

				if (is_selected) {
					UIConfigColor darkened_color = { DarkenColor(current_color, color_theme.darken_inactive_item) };
					if (active_state) {
						darkened_color = previous_color;
					}
					SetConfigParameter(configuration, UI_CONFIG_COLOR, config, darkened_color, previous_color);

					SolidColorRectangle(configuration | UI_CONFIG_COLOR, config, position, triangle_scale);
				}

				if (state) {
					SpriteRectangle(configuration, position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, color_theme.default_text);
				}
				else {
					SpriteRectangle(configuration, position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, color_theme.default_text, { 1.0f, 0.0f }, { 0.0f, 1.0f });
				}

				position.x += triangle_scale.x;
				total_x_scale += triangle_scale.x;

				if (~configuration & UI_CONFIG_COLLAPSING_HEADER_NO_NAME) {
					UIConfigTextAlignment previous_alignment;
					UIConfigTextAlignment alignment;
					alignment.horizontal = TextAlignment::Left;
					alignment.vertical = TextAlignment::Middle;

					SetConfigParameter(configuration, UI_CONFIG_TEXT_ALIGNMENT, config, alignment, previous_alignment);

					float label_scale = scale.x - triangle_scale.x;
					if (label_scale < data->name.scale.x + 2 * element_descriptor.label_horizontal_padd) {
						label_scale = data->name.scale.x + 2 * element_descriptor.label_horizontal_padd;
					}

					size_t LABEL_CONFIGURATION = configuration | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X
						| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_ADVANCE;
					LABEL_CONFIGURATION |= function::Select<size_t>(is_selected, UI_CONFIG_COLOR, UI_CONFIG_LABEL_TRANSPARENT);

					TextLabelDrawer(
						LABEL_CONFIGURATION,
						config,
						&data->name,
						position,
						{ label_scale, scale.y }
					);

					total_x_scale += label_scale;

					UpdateCurrentRowScale(scale.y);
					UpdateRenderBoundsRectangle(position, { 0.0f, scale.y });

					RemoveConfigParameter(configuration, UI_CONFIG_TEXT_ALIGNMENT, config, previous_alignment);
				}

				if (is_selected) {
					RemoveConfigParameter(configuration, UI_CONFIG_COLOR, config, previous_color);
				}

				if (active_state) {
					Color color = HandleColor(configuration, config);

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

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = { initial_position_x, position.y };
				*get_transform->scale = { total_x_scale, scale.y };
			}

			NextRow();
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::CollapsingHeaderDrawer(size_t configuration, UIDrawConfig& config, const char* name, bool* state, float2 position, float2 scale) {
			if (~configuration & UI_CONFIG_COLLAPSING_HEADER_DO_NOT_INFER) {
				scale.x = GetXScaleUntilBorder(position.x);
			}

			float total_x_scale = 0.0f;
			float initial_position_x = position.x;
			if (ValidatePosition(configuration, position, scale)) {
				bool active_state = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* _active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					active_state = _active_state->state;
				}

				bool stack_state = *state;
				float2 initial_position = position;
				stack_state &= active_state;

				Color current_color = HandleColor(configuration, config);
				UIConfigColor previous_color = { current_color };
				float2 triangle_scale = GetSquareScale(scale.y);

				bool is_selected = true;
				if (configuration & UI_CONFIG_COLLAPSING_HEADER_SELECTION) {
					const UIConfigCollapsingHeaderSelection* selection = (const UIConfigCollapsingHeaderSelection*)config.GetParameter(UI_CONFIG_COLLAPSING_HEADER_SELECTION);
					is_selected = selection->is_selected;
				}

				if (is_selected) {
					UIConfigColor darkened_color = { DarkenColor(current_color, color_theme.darken_inactive_item) };
					if (active_state) {
						darkened_color = previous_color;
					}
					SetConfigParameter(configuration, UI_CONFIG_COLOR, config, darkened_color, previous_color);

					SolidColorRectangle(configuration | UI_CONFIG_COLOR, config, position, triangle_scale);
				}

				if (stack_state) {
					SpriteRectangle(configuration, position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE);
				}
				else {
					SpriteRectangle(configuration, position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, ECS_COLOR_WHITE, { 1.0f, 0.0f }, { 0.0f, 1.0f });
				}

				position.x += triangle_scale.x;
				total_x_scale += triangle_scale.x;

				if (~configuration & UI_CONFIG_COLLAPSING_HEADER_NO_NAME) {
					UIConfigTextAlignment previous_alignment;
					UIConfigTextAlignment alignment;
					alignment.horizontal = TextAlignment::Left;
					alignment.vertical = TextAlignment::Middle;

					SetConfigParameter(configuration, UI_CONFIG_TEXT_ALIGNMENT, config, alignment, previous_alignment);

					float label_scale = scale.x - triangle_scale.x;
					float2 name_scale = GetLabelScale(name);
					label_scale = function::Select(label_scale < name_scale.x, name_scale.x, label_scale);

					size_t LABEL_CONFIGURATION = configuration | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X
						| UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_CACHE;
					LABEL_CONFIGURATION |= function::Select<size_t>(is_selected, UI_CONFIG_COLOR, UI_CONFIG_LABEL_TRANSPARENT);

					float2 new_scale = { label_scale, scale.y };
					TextLabel(
						LABEL_CONFIGURATION,
						config,
						name,
						position,
						new_scale
					);

					total_x_scale += new_scale.x;

					UpdateCurrentRowScale(scale.y);
					UpdateRenderBoundsRectangle(position, { 0.0f, scale.y });

					RemoveConfigParameter(configuration, UI_CONFIG_TEXT_ALIGNMENT, config, previous_alignment);
				}

				if (is_selected) {
					RemoveConfigParameter(configuration, UI_CONFIG_COLOR, config, previous_color);
				}

				if (active_state) {
					Color color = HandleColor(configuration, config);

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

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = { initial_position_x, position.y };
				*get_transform->scale = { total_x_scale, scale.y };
			}

			NextRow();
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerHierarchy* UIDrawer::HierarchyInitializer(size_t configuration, const UIDrawConfig& config, const char* name) {
			UIDrawerHierarchy* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				data = (UIDrawerHierarchy*)GetMainAllocatorBuffer<UIDrawerHierarchy>();

				data->nodes.allocator = GetAllocatorPolymorphic(system->m_memory);
				data->nodes.capacity = 0;
				data->nodes.size = 0;
				data->nodes.buffer = nullptr;
				data->pending_initializations.allocator = GetAllocatorPolymorphic(system->m_memory);
				data->pending_initializations.buffer = nullptr;
				data->pending_initializations.size = 0;
				data->pending_initializations.capacity = 0;
				data->system = system;
				data->data = nullptr;
				data->data_size = 0;
				data->window_index = window_index;

				if (((configuration & UI_CONFIG_HIERARCHY_SELECTABLE) != 0) || ((configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) != 0)) {
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

				if (((configuration & UI_CONFIG_HIERARCHY_CHILD) == 0) && (((configuration & UI_CONFIG_HIERARCHY_SELECTABLE) != 0) || ((configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) != 0))) {
					data->multiple_hierarchy_data = GetMainAllocatorBuffer<UIDrawerHierarchiesData>();
					data->multiple_hierarchy_data->hierarchy_transforms = UIDynamicStream<UIElementTransform>(GetAllocatorPolymorphic(system->m_memory), 0);
					data->multiple_hierarchy_data->elements = UIDynamicStream<UIDrawerHierarchiesElement>(GetAllocatorPolymorphic(system->m_memory), 0);
					data->multiple_hierarchy_data->active_hierarchy = data;
				}
				else if (configuration & UI_CONFIG_HIERARCHY_CHILD) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextInput* UIDrawer::FloatInputInitializer(
			size_t configuration,
			UIDrawConfig& config,
			const char* name,
			float* number,
			float default_value,
			float min,
			float max,
			float2 position,
			float2 scale
		) {
			UIDrawerFloatInputCallbackData callback_data;
			if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
				callback_data.max = max;
				callback_data.min = min;
			}
			else {
				callback_data.max = FLT_MAX;
				callback_data.min = -FLT_MAX;
			}
			callback_data.number = number;
			if (~configuration & UI_CONFIG_NUMBER_INPUT_DEFAULT) {
				callback_data.default_value = *number;
			}
			else {
				callback_data.default_value = default_value;
			}

			return NumberInputInitializer(
				configuration,
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextInput* UIDrawer::DoubleInputInitializer(
			size_t configuration,
			UIDrawConfig& config,
			const char* name,
			double* number,
			double default_value,
			double min,
			double max,
			float2 position,
			float2 scale
		) {
			UIDrawerDoubleInputCallbackData callback_data;
			if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
				callback_data.max = max;
				callback_data.min = min;
			}
			else {
				callback_data.max = DBL_MAX;
				callback_data.min = -DBL_MAX;
			}
			callback_data.number = number;
			if (~configuration & UI_CONFIG_NUMBER_INPUT_DEFAULT) {
				callback_data.default_value = *number;
			}
			else {
				callback_data.default_value = default_value;
			}

			return NumberInputInitializer(
				configuration,
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		UIDrawerTextInput* UIDrawer::IntInputInitializer(
			size_t configuration,
			UIDrawConfig& config,
			const char* name,
			Integer* number,
			Integer default_value,
			Integer min,
			Integer max,
			float2 position,
			float2 scale
		) {
			UIDrawerIntegerInputCallbackData<Integer> callback_data;
			if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
				callback_data.max = max;
				callback_data.min = min;
			}
			else {
				function::IntegerRange(callback_data.min, callback_data.max);
			}
			callback_data.number = number;
			if (~configuration & UI_CONFIG_NUMBER_INPUT_DEFAULT) {
				callback_data.default_value = *number;
			}
			else {
				callback_data.default_value = default_value;
			}

			return NumberInputInitializer(
				configuration,
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

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(UIDrawerTextInput*, UIDrawer::IntInputInitializer, size_t, UIDrawConfig&, const char*, integer*, integer, integer, integer, float2, float2);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatInputDrawer(size_t configuration, const UIDrawConfig& config, const char* name, float* number, float min, float max, float2 position, float2 scale) {
			const float EPSILON = 0.0005f;

			UIDrawerFloatInputDragData drag_data;
			drag_data.number = number;
			if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
				drag_data.min = min;
				drag_data.max = max;
			}
			else {
				drag_data.min = -FLT_MAX;
				drag_data.max = FLT_MAX;
			}

			NumberInputDrawer(configuration, config, name, FloatInputHoverable, FloatInputNoNameHoverable, FloatInputDragValue,
				&drag_data, sizeof(drag_data), position, scale, [=](UIDrawerTextInput* input, Stream<char> tool_tip_characters) {
					char temp_chars[256];
					Stream<char> temp_stream = Stream<char>(temp_chars, 0);
					UIDrawerFloatInputCallbackData* data = (UIDrawerFloatInputCallbackData*)input->callback_data;

					if (!function::IsFloatingPointNumber(*input->text) && !input->is_currently_selected) {
						input->DeleteAllCharacters();
						function::ConvertFloatToChars(temp_stream, function::Clamp(0.0f, data->min, data->max));
						input->InsertCharacters(temp_chars, temp_stream.size, 0, system);
						temp_stream.size = 0;
					}

					unsigned int digit_count = 0;
					const char* dot = strchr(input->text->buffer, '.');
					if (dot != nullptr) {
						digit_count = input->text->buffer + input->text->size - dot - 1;
					}

					// If the value changed, update the input stream
					float current_value = function::ConvertCharactersToFloat(*input->text);
					if ((abs(current_value - *number) > EPSILON || digit_count != 3) && !input->is_currently_selected) {
						input->DeleteAllCharacters();
						function::ConvertFloatToChars(temp_stream, *number, 3);
						input->InsertCharacters(temp_chars, temp_stream.size, 0, system);
					}

					if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleInputDrawer(size_t configuration, const UIDrawConfig& config, const char* name, double* number, double min, double max, float2 position, float2 scale) {
			const double EPSILON = 0.0005;

			UIDrawerDoubleInputDragData drag_data;
			drag_data.number = number;
			if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
				drag_data.min = min;
				drag_data.max = max;
			}
			else {
				drag_data.min = -DBL_MAX;
				drag_data.max = DBL_MAX;
			}

			NumberInputDrawer(configuration, config, name, DoubleInputHoverable, DoubleInputNoNameHoverable, DoubleInputDragValue,
				&drag_data, sizeof(drag_data), position, scale, [=](UIDrawerTextInput* input, Stream<char> tool_tip_characters) {
					char temp_chars[256];
					Stream<char> temp_stream = Stream<char>(temp_chars, 0);
					UIDrawerDoubleInputCallbackData* data = (UIDrawerDoubleInputCallbackData*)input->callback_data;

					if (!function::IsFloatingPointNumber(*input->text) && !input->is_currently_selected) {
						input->DeleteAllCharacters();
						function::ConvertDoubleToChars(temp_stream, function::Clamp(0.0, data->min, data->max), 3);
						input->InsertCharacters(temp_chars, temp_stream.size, 0, system);
						temp_stream.size = 0;
					}

					unsigned int digit_count = 0;
					const char* dot = strchr(input->text->buffer, '.');
					if (dot != nullptr) {
						digit_count = input->text->buffer + input->text->size - dot - 1;
					}

					// If the value changed, update the input stream
					double current_value = function::ConvertCharactersToDouble(*input->text);
					if ((abs(current_value - *number) > EPSILON || digit_count != 3) && !input->is_currently_selected) {
						input->DeleteAllCharacters();
						function::ConvertDoubleToChars(temp_stream, *number, 3);
						input->InsertCharacters(temp_chars, temp_stream.size, 0, system);
					}

					if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntInputDrawer(size_t configuration, const UIDrawConfig& config, const char* name, Integer* number, Integer min, Integer max, float2 position, float2 scale) {
			UIDrawerIntInputDragData<Integer> drag_data;
			drag_data.data.number = number;
			if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
				drag_data.data.min = min;
				drag_data.data.max = max;
			}
			else {
				function::IntegerRange(drag_data.data.min, drag_data.data.max);
			}

			NumberInputDrawer(configuration, config, name, IntInputHoverable<Integer>, IntInputNoNameHoverable<Integer>, IntegerInputDragValue<Integer>,
				&drag_data, sizeof(drag_data), position, scale, [=](UIDrawerTextInput* input, Stream<char> tool_tip_characters) {
					char temp_chars[256];
					Stream<char> temp_stream = Stream<char>(temp_chars, 0);
					UIDrawerIntegerInputCallbackData<Integer>* data = (UIDrawerIntegerInputCallbackData<Integer>*)input->callback_data;

					if (!function::IsIntegerNumber(*input->text) && !input->is_currently_selected) {
						input->DeleteAllCharacters();
						function::ConvertIntToCharsFormatted(temp_stream, function::Clamp((Integer)0, data->min, data->max));
						input->InsertCharacters(temp_chars, temp_stream.size, 0, system);
						temp_stream.size = 0;
					}

					// If the value changed, update the input stream
					Integer current_value = function::ConvertCharactersToInt<Integer>(*input->text);
					if (current_value != *number && !input->is_currently_selected) {
						input->DeleteAllCharacters();
						function::ConvertIntToCharsFormatted(temp_stream, static_cast<int64_t>(*number));
						input->InsertCharacters(temp_chars, temp_stream.size, 0, system);
					}

					if (~configuration & UI_CONFIG_NUMBER_INPUT_NO_RANGE) {
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

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInputDrawer, size_t, const UIDrawConfig&, const char*, integer*, integer, integer, float2, float2);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatInputGroupDrawer(
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
		) {
			GroupDrawerImplementation<float>(this, configuration, config, group_name, count, names, values, lower_bounds, upper_bounds, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatInputGroupInitializer(
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
		) {
			GroupInitializerImplementation<float>(this, configuration, config, group_name, count, names, values, default_values, lower_bounds, upper_bounds, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleInputGroupDrawer(
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
		) {
			size_t LABEL_CONFIGURATION = configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
			if (IsElementNameFirst(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
				TextLabel(LABEL_CONFIGURATION | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, group_name);
				Indent(-1.0f);
			}

			bool has_pushed_stack = PushIdentifierStackStringPattern();
			PushIdentifierStack(group_name);

			for (size_t index = 0; index < count; index++) {
				HandleTransformFlags(configuration, config, position, scale);
				double lower_bound = -DBL_MAX, upper_bound = DBL_MAX;
				if (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS) {
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

				size_t drawer_configuration = configuration | UI_CONFIG_DO_NOT_ADVANCE |
					function::Select<size_t>(~configuration & UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES, UI_CONFIG_ELEMENT_NAME_FIRST, UI_CONFIG_TEXT_INPUT_NO_NAME);
				DoubleInputDrawer(
					drawer_configuration,
					config,
					names[index],
					values[index],
					lower_bound,
					upper_bound,
					position,
					scale
				);
			}

			if (has_pushed_stack) {
				PopIdentifierStack();
			}
			PopIdentifierStack();

			if (IsElementNameAfter(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
				TextLabel(LABEL_CONFIGURATION, config, group_name);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleInputGroupInitializer(
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
		) {
			GroupInitializerImplementation<double>(this, configuration, config, group_name, count, names, values, default_values, lower_bounds, upper_bounds, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntInputGroupDrawer(
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
		) {
			size_t LABEL_CONFIGURATION = configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
			if (IsElementNameFirst(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
				TextLabel(LABEL_CONFIGURATION | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, group_name);
				Indent(-1.0f);
			}

			bool has_pushed_stack = PushIdentifierStackStringPattern();
			PushIdentifierStack(group_name);

			for (size_t index = 0; index < count; index++) {
				HandleTransformFlags(configuration, config, position, scale);
				Integer lower_bound, upper_bound, default_value = *values[index];
				function::IntegerRange(lower_bound, upper_bound);
				if (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS) {
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

				size_t drawer_configuration = configuration | UI_CONFIG_DO_NOT_ADVANCE |
					function::Select<size_t>(~configuration & UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES, UI_CONFIG_ELEMENT_NAME_FIRST, UI_CONFIG_TEXT_INPUT_NO_NAME);
				IntInputDrawer<Integer>(
					drawer_configuration,
					config,
					names[index],
					values[index],
					lower_bound,
					upper_bound,
					position,
					scale
				);
			}

			if (has_pushed_stack) {
				PopIdentifierStack();
			}
			PopIdentifierStack();

			if (IsElementNameAfter(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME)) {
				TextLabel(LABEL_CONFIGURATION, config, group_name);
			}
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInputGroupDrawer, size_t, UIDrawConfig&, const char*, size_t, const char**, integer**, const integer*, const integer*, float2, float2);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntInputGroupInitializer(
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
		) {
			GroupInitializerImplementation<Integer>(this, configuration, config, group_name, count, names, values, default_values, lower_bounds, upper_bounds, position, scale);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInputGroupInitializer, size_t, UIDrawConfig&, const char*, size_t, const char**, integer**, const integer*, const integer*, const integer*, float2, float2);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HierarchyDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerHierarchy* data, float2 scale) {
			if (~configuration & UI_CONFIG_HIERARCHY_DO_NOT_REDUCE_Y) {
				scale.y *= 0.75f;
			}

			if (~configuration & UI_CONFIG_HIERARCHY_CHILD) {
				data->multiple_hierarchy_data->hierarchy_transforms.Reset();
				data->multiple_hierarchy_data->elements.Reset();
			}

			if (data->pending_initializations.size > 0) {
				// creating a temporary initializer
				UIDrawer drawer_initializer = UIDrawer(color_theme, font, layout, element_descriptor, true);
				memcpy(&drawer_initializer, this, sizeof(UIDrawer));
				drawer_initializer.deallocate_constructor_allocations = false;

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
			if (configuration & UI_CONFIG_HIERARCHY_NODE_DO_NOT_INFER_SCALE_X) {
				scale.x = initial_scale;
			}

			float2 sprite_scale = GetSquareScale(scale.y);

			UIConfigTextAlignment alignment;
			alignment.horizontal = TextAlignment::Left;
			alignment.vertical = TextAlignment::Middle;

			UIDrawConfig label_config;
			label_config.AddFlag(alignment);

			Color hover_color = HandleColor(configuration, config);
			LPCWSTR texture = nullptr;
			float2 top_left_closed_uv;
			float2 bottom_right_closed_uv;
			float2 top_left_opened_uv;
			float2 bottom_right_opened_uv;
			float2 sprite_scale_factor;
			Color texture_color;
			UIConfigHierarchyNoAction* extra_info = nullptr;

			bool extra_draw_index = false;
			if (configuration & UI_CONFIG_HIERARCHY_SPRITE_TEXTURE) {
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

			if (configuration & UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME) {
				extra_info = (UIConfigHierarchyNoAction*)config.GetParameter(UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME);
			}

			float max_label_scale = scale.x;

			auto extra_draw_triangle_opened = [](UIDrawer& drawer, float2& position, float2 sprite_scale, size_t configuration) {
				drawer.SpriteRectangle(configuration, position, sprite_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, drawer.color_theme.default_text);
				position.x += sprite_scale.x;
			};

			auto extra_draw_triangle_closed = [](UIDrawer& drawer, float2& position, float2 sprite_scale, size_t configuration) {
				drawer.SpriteRectangle(configuration, position, sprite_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, drawer.color_theme.default_text, { 1.0f, 0.0f }, { 0.0f, 1.0f });
				position.x += sprite_scale.x;
			};

			auto extra_draw_nothing = [](UIDrawer& drawer, float2& position, float2 sprite_scale, size_t configuration) {};

			using ExtraDrawFunction = void (*)(UIDrawer&, float2&, float2, size_t);

			ExtraDrawFunction opened_table[] = { extra_draw_nothing, extra_draw_triangle_opened };
			ExtraDrawFunction closed_table[] = { extra_draw_nothing, extra_draw_triangle_closed };

			for (size_t index = 0; index < data->nodes.size; index++) {
				float label_scale = scale.x;

				// general implementation
				if (~configuration & UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME) {
					unsigned int hierarchy_children_start = 0;
					if ((configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) != 0) {
						data->multiple_hierarchy_data->elements.Add({ data, (unsigned int)index, 0 });
						data->multiple_hierarchy_data->hierarchy_transforms.Add({ position, {label_scale, scale.y} });
						hierarchy_children_start = data->multiple_hierarchy_data->hierarchy_transforms.size;
					}

					if (data->nodes[index].state) {
						if (ValidatePosition(0, position, { label_scale, scale.y })) {
							opened_table[extra_draw_index](*this, position, sprite_scale, configuration);
							SpriteRectangle(configuration, position, sprite_scale, texture, texture_color, top_left_opened_uv, bottom_right_opened_uv);
							TextLabelDrawer
							(
								UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_TEXT_ALIGNMENT
								| UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_VALIDATE_POSITION,
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

						if (configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) {
							data->multiple_hierarchy_data->elements[hierarchy_children_start - 1].children_count = data->multiple_hierarchy_data->elements.size - hierarchy_children_start;
						}
					}
					else {
						if (ValidatePosition(0, position, { label_scale, scale.y })) {
							closed_table[extra_draw_index](*this, position, sprite_scale, configuration);
							SpriteRectangle(configuration, position, sprite_scale, texture, texture_color, top_left_closed_uv, bottom_right_closed_uv);
							TextLabelDrawer
							(
								UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_TEXT_ALIGNMENT
								| UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_VALIDATE_POSITION | UI_CONFIG_DO_NOT_ADVANCE,
								label_config,
								&data->nodes[index].name_element,
								{ position.x + sprite_scale.x, position.y },
								{ 0.0f, scale.y }
							);
						}
						FinalizeRectangle(0, position, { sprite_scale.x + data->nodes[index].name_element.scale.x, scale.y });
						NextRow(0.5f);
					}

					float element_scale = data->nodes[index].name_element.scale.x + 2 * element_descriptor.label_horizontal_padd + sprite_scale.x;
					label_scale = function::Select(label_scale < element_scale, element_scale, label_scale);

					UIDrawerBoolClickableWithPinData click_data;
					click_data.pointer = &data->nodes[index].state;
					click_data.is_vertical = true;

					float2 hoverable_scale = { label_scale, scale.y };
					if (configuration & UI_CONFIG_HIERARCHY_SELECTABLE) {
						if (data->selectable.selected_index == index && data->multiple_hierarchy_data->active_hierarchy == data) {
							SolidColorRectangle(configuration, config, position, hoverable_scale);
						}
						UIDrawerHierarchySelectableData selectable_data;
						selectable_data.bool_data = click_data;
						selectable_data.hierarchy = data;
						selectable_data.node_index = index;
						AddDefaultClickableHoverable(position, hoverable_scale, { HierarchySelectableClick, &selectable_data, sizeof(selectable_data) }, hover_color);
					}
					else if (configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) {
						if (data->selectable.selected_index == index && data->multiple_hierarchy_data->active_hierarchy == data) {
							SolidColorRectangle(configuration, config, position, hoverable_scale);
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
					max_label_scale = function::Select(max_label_scale < label_scale, label_scale, max_label_scale);
					position = { GetNextRowXPosition() - region_render_offset.x, current_y - region_render_offset.y };
				}
				// list implementation is here
				else {
					float2 sprite_position = position;
					FinalizeRectangle(0, position, sprite_scale);
					data->nodes[index].function(this, window_data, extra_info->extra_information);
					NextRow();
					position = { current_x - region_render_offset.x, current_y - region_render_offset.y };

					sprite_position.y = AlignMiddle(sprite_position.y, extra_info->row_y_scale, sprite_scale.y);
					SpriteRectangle(configuration, sprite_position, sprite_scale, texture, texture_color, top_left_closed_uv, bottom_right_closed_uv);
				}
			}

			// adding a sentinel in order to have nodes placed at the back
			if ((configuration & UI_CONFIG_HIERARCHY_DRAG_NODE) != 0) {
				data->multiple_hierarchy_data->elements.Add({ data, data->nodes.size });
				data->multiple_hierarchy_data->hierarchy_transforms.Add({ position, {0.0f, scale.y} });
			}

		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HistogramDrawer(size_t configuration, const UIDrawConfig& config, const Stream<float> samples, const char* name, float2 position, float2 scale, unsigned int precision) {
			const size_t STACK_CHARACTER_COUNT = 128;

			float histogram_min_scale = samples.size * element_descriptor.histogram_bar_min_scale + (samples.size - 1) * element_descriptor.histogram_bar_spacing + 2.0f * element_descriptor.histogram_padding.x;
			function::ClampMin(scale.x, histogram_min_scale);

			float2 initial_scale = scale;
			if (IsElementNameFirst(configuration, UI_CONFIG_HISTOGRAM_NO_NAME)) {
				if (name != nullptr) {
					TextLabel(configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y, config, name, position, scale);
					scale = initial_scale;
				}
			}

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				SolidColorRectangle(configuration, config, position, scale);
				float2 histogram_scale = { scale.x - 2.0f * element_descriptor.histogram_padding.x, scale.y - 2.0f * element_descriptor.histogram_padding.y };
				float2 histogram_position = { position.x + element_descriptor.histogram_padding.x, position.y + element_descriptor.histogram_padding.y };

				Color histogram_color;
				if (configuration & UI_CONFIG_HISTOGRAM_COLOR) {
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


				if (configuration & UI_CONFIG_TEXT_PARAMETERS) {
					text_parameters = *(const UIConfigTextParameters*)config.GetParameter(UI_CONFIG_TEXT_PARAMETERS);
				}
				else if (~configuration & UI_CONFIG_HISTOGRAM_VARIANT_ZOOM_FONT) {
					text_parameters.size.x *= zoom_inverse.x * zoom_inverse.x;
					text_parameters.size.y *= zoom_inverse.y * zoom_inverse.y;
					text_parameters.character_spacing *= zoom_inverse.x;
				}

				if (configuration & UI_CONFIG_HISTOGRAM_REDUCE_FONT) {
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

				char stack_characters[STACK_CHARACTER_COUNT];
				Stream<char> stack_stream = Stream<char>(stack_characters, 0);

				int64_t starting_index = (region_position.x - histogram_position.x) / bar_scale;
				starting_index = function::Select(starting_index < 0, (int64_t)0, starting_index);
				int64_t end_index = (region_position.x + region_scale.x - histogram_position.x) / bar_scale + 1;
				end_index = function::Select(end_index > samples.size, (int64_t)samples.size, end_index);
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

					if (IsPointInRectangle(mouse_position, { sample_position.x, histogram_position.y }, { sample_scale.x, histogram_scale.y })) {
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
						TextLabel(
							UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_DO_NOT_FIT_SPACE
							| UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y,
							label_config,
							stack_characters,
							text_position,
							sample_scale
						);
					}
					SolidColorRectangle(configuration, sample_position, sample_scale, histogram_color);

					UIDrawerHistogramHoverableData hoverable_data;
					hoverable_data.sample_index = index;
					hoverable_data.sample_value = samples[index];
					AddHoverable({ sample_position.x, histogram_position.y }, { sample_scale.x, histogram_scale.y }, { HistogramHoverable, &hoverable_data, sizeof(hoverable_data), UIDrawPhase::System });
					histogram_position.x += sample_scale.x + element_descriptor.histogram_bar_spacing;
				}
			}

			FinalizeRectangle(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, position, scale);

			if (IsElementNameAfter(configuration, UI_CONFIG_HISTOGRAM_NO_NAME)) {
				if (name != nullptr) {
					position.x += scale.x + layout.element_indentation;
					TextLabel(configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y, config, name, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerList* UIDrawer::ListInitializer(size_t configuration, const UIDrawConfig& config, const char* name) {
			UIDrawerList* list = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				list = GetMainAllocatorBuffer<UIDrawerList>();

				list->hierarchy.nodes.allocator = GetAllocatorPolymorphic(system->m_memory);
				list->hierarchy.nodes.capacity = 0;
				list->hierarchy.nodes.size = 0;
				list->hierarchy.nodes.buffer = nullptr;
				list->hierarchy.pending_initializations.allocator = GetAllocatorPolymorphic(system->m_memory);
				list->hierarchy.pending_initializations.buffer = nullptr;
				list->hierarchy.pending_initializations.size = 0;
				list->hierarchy.pending_initializations.capacity = 0;
				list->hierarchy.system = system;
				list->hierarchy.data = nullptr;
				list->hierarchy.data_size = 0;
				list->hierarchy.window_index = window_index;

				if (~configuration & UI_CONFIG_LIST_NO_NAME) {
					InitializeElementName(configuration, UI_CONFIG_LIST_NO_NAME, config, identifier, &list->name, { 0.0f, 0.0f });
				}

				return list;
				});
			return list;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ListDrawer(size_t configuration, UIDrawConfig& config, UIDrawerList* data, float2 position, float2 scale) {
			if (IsElementNameFirst(configuration, UI_CONFIG_LIST_NO_NAME) || IsElementNameAfter(configuration, UI_CONFIG_LIST_NO_NAME)) {
				current_x += layout.element_indentation;
				ElementName(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, &data->name, position, scale);
				NextRow();
			}

			UIConfigHierarchyNoAction info;
			info.extra_information = data;

			config.AddFlag(info);
			data->hierarchy_extra = (UIConfigHierarchyNoAction*)config.GetParameter(UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME);

			HierarchyDrawer(configuration | UI_CONFIG_HIERARCHY_NO_ACTION_NO_NAME | UI_CONFIG_HIERARCHY_CHILD, config, &data->hierarchy, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::MenuDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerMenu* data, float2 position, float2 scale) {
			HandleFitSpaceRectangle(configuration, position, scale);

			bool is_active = true;
			if (configuration & UI_CONFIG_ACTIVE_STATE) {
				const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				is_active = active_state->state;
			}

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				if (~configuration & UI_CONFIG_MENU_SPRITE) {
					size_t label_configuration = configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_ADVANCE;
					label_configuration |= function::Select<size_t>(is_active, 0, UI_CONFIG_UNAVAILABLE_TEXT);
					TextLabelDrawer(
						label_configuration,
						config,
						data->name,
						position,
						scale
					);
				}
				else {
					const UIConfigMenuSprite* sprite_definition = (const UIConfigMenuSprite*)config.GetParameter(UI_CONFIG_MENU_SPRITE);
					Color sprite_color = sprite_definition->color;
					if (!is_active) {
						sprite_color.alpha *= color_theme.alpha_inactive_item;
					}
					SpriteRectangle(
						configuration,
						position,
						scale,
						sprite_definition->texture,
						sprite_definition->color,
						sprite_definition->top_left_uv,
						sprite_definition->bottom_right_uv
					);

					HandleBorder(configuration, config, position, scale);
				}

				if (is_active) {
					UIDrawerMenuGeneralData general_data;
					general_data.menu = data;
					general_data.menu_initializer_index = 255;

					AddDefaultHoverable(position, scale, HandleColor(configuration, config));
					AddGeneral(position, scale, { MenuGeneral, &general_data, sizeof(general_data), UIDrawPhase::System });
					AddClickable(position, scale, { SkipAction, nullptr, 0 });
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t UIDrawer::MenuCalculateStateMemory(const UIDrawerMenuState* state, bool copy_states) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t UIDrawer::MenuWalkStatesMemory(const UIDrawerMenuState* state, bool copy_states) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::MenuSetStateBuffers(
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::MenuWalkSetStateBuffers(
			UIDrawerMenuState* state,
			uintptr_t& buffer,
			CapacityStream<UIDrawerMenuWindow>* stream,
			bool copy_states
		) {
			UIDrawer::MenuSetStateBuffers(state, buffer, stream, copy_states);
			if (state->row_has_submenu != nullptr) {
				for (size_t index = 0; index < state->row_count; index++) {
					if (state->row_has_submenu[index]) {
						MenuSetStateBuffers(&state->submenues[index], buffer, stream, copy_states);
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerMenu* UIDrawer::MenuInitializer(size_t configuration, const UIDrawConfig& config, const char* name, float2 scale, UIDrawerMenuState* menu_state) {
			UIDrawerMenu* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				data = GetMainAllocatorBuffer<UIDrawerMenu>();

				if (~configuration & UI_CONFIG_MENU_SPRITE) {
					char temp_characters[512];
					strcpy(temp_characters, identifier);
					strcat(temp_characters, "##Separate");

					// separate the identifier for the text label
					data->name = TextLabelInitializer(configuration | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN, config, temp_characters, { 0.0f, 0.0f }, scale);
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::MultiGraphDrawer(
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
		) {
			constexpr size_t label_configuration = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;

			if (IsElementNameFirst(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
				float2 initial_scale = scale;
				if (name != nullptr) {
					TextLabel(label_configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, name, position, scale);
					scale = initial_scale;
				}
			}

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			auto get_sample = [=](size_t index, size_t sample_index) {
				return samples[index * sample_count + sample_index];
			};

			if (ValidatePosition(configuration, position, scale)) {
				float2 graph_scale = { scale.x - 2.0f * element_descriptor.graph_padding.x, scale.y - 2.0f * element_descriptor.graph_padding.y };
				float2 graph_position = { position.x + element_descriptor.graph_padding.x, position.y + element_descriptor.graph_padding.y };

				float2 difference;
				difference.x = get_sample(samples.size - 1, 0) - get_sample(0, 0);
				difference.y = 0.0f;

				float min_y = 1000000.0f;
				float max_y = -1000000.0f;

				if ((configuration & UI_CONFIG_GRAPH_MIN_Y) == 0 || (configuration & UI_CONFIG_GRAPH_MAX_Y) == 0) {
					for (size_t index = 0; index < samples.size; index++) {
						for (size_t sample_index = 0; sample_index < sample_count - 1; sample_index++) {
							float sample_value = get_sample(index, sample_index);
							min_y = function::Select(min_y > sample_value, sample_value, min_y);
							max_y = function::Select(max_y < sample_value, sample_value, max_y);
						}
					}
				}

				if (configuration & UI_CONFIG_GRAPH_MIN_Y) {
					const UIConfigGraphMinY* min = (const UIConfigGraphMinY*)config.GetParameter(UI_CONFIG_GRAPH_MIN_Y);
					min_y = min->value;
				}
				if (configuration & UI_CONFIG_GRAPH_MAX_Y) {
					const UIConfigGraphMaxY* max = (const UIConfigGraphMaxY*)config.GetParameter(UI_CONFIG_GRAPH_MAX_Y);
					max_y = max->value;
				}


				difference.y = max_y - min_y;

				Color font_color;
				float character_spacing;
				float2 font_size;

				HandleText(configuration, config, font_color, font_size, character_spacing);
				font_color.alpha = ECS_TOOLS_UI_GRAPH_TEXT_ALPHA;

				if (configuration & UI_CONFIG_GRAPH_REDUCE_FONT) {
					font_size.x *= element_descriptor.graph_reduce_font;
					font_size.y *= element_descriptor.graph_reduce_font;
				}

				float2 axis_bump = { 0.0f, 0.0f };
				float y_sprite_size = system->GetTextSpriteYScale(font_size.y);
				if (configuration & UI_CONFIG_GRAPH_X_AXIS) {
					axis_bump.y = y_sprite_size;
				}

				if (configuration & UI_CONFIG_GRAPH_Y_AXIS) {
					float axis_span = GraphYAxis(
						configuration,
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
					function::ConvertFloatToChars(temp_stream, get_sample(0, 0), x_axis_precision);

					float2 first_sample_span = system->GetTextSpan(temp_stream.buffer, temp_stream.size, font_size.x, font_size.y, character_spacing);
					axis_bump.x -= first_sample_span.x * 0.5f;
					graph_position.y += y_sprite_size * 0.5f;
					graph_scale.y -= y_sprite_size * 0.5f;
					if (~configuration & UI_CONFIG_GRAPH_X_AXIS) {
						graph_scale.y -= y_sprite_size * 0.5f;
					}
				}

				if (configuration & UI_CONFIG_GRAPH_X_AXIS) {
					float2 spans = GraphXAxis(
						configuration,
						config,
						x_axis_precision,
						get_sample(0, 0),
						get_sample(samples.size - 1, 0),
						{ position.x + element_descriptor.graph_padding.x + axis_bump.x, position.y },
						{ scale.x - element_descriptor.graph_padding.x - axis_bump.x, scale.y },
						font_color,
						font_size,
						character_spacing,
						axis_bump.x != 0.0f
					);
					graph_scale.y -= y_sprite_size + element_descriptor.graph_axis_bump.y;
					if (~configuration & UI_CONFIG_GRAPH_Y_AXIS) {
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
				float min_x = get_sample(0, 0);
				float x_space_factor = 1.0f / difference.x * graph_scale.x;
				while ((get_sample(index, 0) - min_x) * x_space_factor + graph_position.x < region_position.x && index < samples.size) {
					index++;
				}
				starting_index = function::Select(index <= 0, (int64_t)0, index - 1);
				while ((get_sample(index, 0) - min_x) * x_space_factor + graph_position.x < region_limit.x && index < samples.size) {
					index++;
				}
				end_index = function::Select(index >= samples.size - 2, (int64_t)samples.size, index + 3);

				SolidColorRectangle(configuration, config, position, scale);

				auto convert_to_graph_space_x = [](float position, float2 graph_position,
					float2 graph_scale, float2 min, float2 inverse_sample_span) {
						return graph_position.x + graph_scale.x * (position - min.x) * inverse_sample_span.x;
				};

				auto convert_to_graph_space_y = [](float position, float2 graph_position,
					float2 graph_scale, float2 min, float2 inverse_sample_span) {
						return graph_position.y + graph_scale.y * (1.0f - (position - min.y) * inverse_sample_span.y);
				};

				float max_x = get_sample(samples.size - 1, 0);
				float2 inverse_sample_span = { 1.0f / (max_x - min_x), 1.0f / (max_y - min_y) };
				float2 min_values = { min_x, min_y };
				float2 previous_point_position = {
					convert_to_graph_space_x(get_sample(starting_index, 0), graph_position, graph_scale, min_values, inverse_sample_span),
					0.0f
				};

				Color line_color = colors[0];

				Stream<UISpriteVertex> drop_color_stream;
				if (end_index > starting_index + 1) {
					int64_t line_count = (end_index - starting_index - 1) * (sample_count - 1);
					SetSpriteClusterTexture(configuration, ECS_TOOLS_UI_TEXTURE_MASK, line_count);
					drop_color_stream = GetSpriteClusterStream(configuration, line_count * 6);
					auto sprite_count = HandleSpriteClusterCount(configuration);
					*sprite_count += line_count * 6;
				}

				for (int64_t index = starting_index; index < end_index - 1; index++) {
					float2 next_point = {
						convert_to_graph_space_x(get_sample(index + 1, 0), graph_position, graph_scale, min_values, inverse_sample_span),
						0.0f
					};

					for (size_t sample_index = 0; sample_index < sample_count - 1; sample_index++) {
						previous_point_position.y = convert_to_graph_space_y(get_sample(index, sample_index), graph_position, graph_scale, min_values, inverse_sample_span);
						next_point.y = convert_to_graph_space_y(get_sample(index + 1, sample_index), graph_position, graph_scale, min_values, inverse_sample_span);
						line_color = colors[sample_index];

						Line(configuration, previous_point_position, next_point, line_color);

						line_color.alpha = ECS_TOOLS_UI_GRAPH_DROP_COLOR_ALPHA;
						size_t base_index = (index - starting_index) * 6 * (sample_count - 1) + sample_index * 6;
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

				if (configuration & UI_CONFIG_GRAPH_TAGS) {
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

			FinalizeRectangle(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, position, scale);

			if (IsElementNameAfter(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
				if (name != nullptr) {
					position.x += scale.x + layout.element_indentation;
					TextLabel(label_configuration, config, name, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void* UIDrawer::SentenceInitializer(size_t configuration, const UIDrawConfig& config, const char* text) {
			if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
				UIDrawerSentenceCached* data = nullptr;

				// Begin recording allocations and table resources for dynamic resources
				if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
					BeginElement();
					configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
				}
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
					HandleText(configuration, config, text_color, font_size, character_spacing);

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

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t UIDrawer::SentenceWhitespaceCharactersCount(const char* identifier, CapacityStream<unsigned int> stack_buffer) {
			return function::FindWhitespaceCharactersCount(identifier, &stack_buffer);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SentenceNonCachedInitializerKernel(const char* identifier, UIDrawerSentenceNotCached* data) {
			//size_t identifier_length = ParseStringIdentifier(identifier, strlen(identifier));
			size_t identifier_length = strlen(identifier);
			size_t temp_marker = GetTempAllocatorMarker();
			char* temp_chars = (char*)GetTempBuffer(identifier_length + 1);
			memcpy(temp_chars, identifier, identifier_length);
			temp_chars[identifier_length + 1] = '\0';
			size_t space_count = function::ParseWordsFromSentence(temp_chars);

			function::FindWhitespaceCharacters(temp_chars, data->whitespace_characters);
			data->whitespace_characters.Add(identifier_length);
			ReturnTempAllocator(temp_marker);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SentenceDrawer(size_t configuration, const UIDrawConfig& config, const char* identifier, void* resource, float2 position) {
			UIConfigSentenceHoverableHandlers* hoverables = nullptr;
			UIConfigSentenceClickableHandlers* clickables = nullptr;
			UIConfigSentenceGeneralHandlers* generals = nullptr;

			if (configuration & UI_CONFIG_SENTENCE_HOVERABLE_HANDLERS) {
				hoverables = (UIConfigSentenceHoverableHandlers*)config.GetParameter(UI_CONFIG_SENTENCE_HOVERABLE_HANDLERS);
			}

			if (configuration & UI_CONFIG_SENTENCE_CLICKABLE_HANDLERS) {
				clickables = (UIConfigSentenceClickableHandlers*)config.GetParameter(UI_CONFIG_SENTENCE_CLICKABLE_HANDLERS);
			}

			if (configuration & UI_CONFIG_SENTENCE_GENERAL_HANDLERS) {
				generals = (UIConfigSentenceGeneralHandlers*)config.GetParameter(UI_CONFIG_SENTENCE_GENERAL_HANDLERS);
			}

			auto handlers = [&](float2 handler_position, float2 handler_scale, unsigned int line_count) {
				if (configuration & UI_CONFIG_SENTENCE_HOVERABLE_HANDLERS) {
					if (hoverables->callback[line_count] != nullptr) {
						AddHoverable(handler_position, handler_scale, { hoverables->callback[line_count], hoverables->data[line_count], hoverables->data_size[line_count], hoverables->phase[line_count] });
					}
				}
				if (configuration & UI_CONFIG_SENTENCE_CLICKABLE_HANDLERS) {
					if (clickables->callback[line_count] != nullptr) {
						AddClickable(handler_position, handler_scale, { clickables->callback[line_count], clickables->data[line_count], clickables->data_size[line_count], clickables->phase[line_count] });
					}
				}
				if (configuration & UI_CONFIG_SENTENCE_GENERAL_HANDLERS) {
					if (generals->callback[line_count] != nullptr) {
						AddGeneral(handler_position, handler_scale, { generals->callback[line_count], generals->data[line_count], generals->data_size[line_count], generals->phase[line_count] });
					}
				}
			};

			unsigned int line_count = 0;

			if (configuration & UI_CONFIG_DO_NOT_CACHE) {
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
					whitespace_characters.buffer = data.whitespace_characters.buffer;
					whitespace_characters.capacity = whitespace_characters.size;
				}
				else {
					whitespace_characters.Add(ParseStringIdentifier(identifier, strlen(identifier)));
				}

				float y_row_scale = current_row_y_scale;

				UIDrawerMode previous_draw_mode = draw_mode;
				unsigned int previous_draw_count = draw_mode_count;
				unsigned int previous_draw_target = draw_mode_target;
				if (configuration & UI_CONFIG_SENTENCE_FIT_SPACE) {
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

				HandleText(configuration, config, font_color, font_size, character_spacing);

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
						if (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
							current_vertices = GetTextStream(configuration, (word_end_index - word_start_index + 1) * 6);
						}
						Text(configuration, config, temp_chars + word_start_index, position, text_span);

						if (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
							current_row_y_scale = y_row_scale;
							float y_position = AlignMiddle<float>(position.y, y_row_scale, text_span.y);
							TranslateTextY(y_position, 0, current_vertices);
						}

						FinalizeRectangle(0, position, text_span);
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
							if (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
								current_row_y_scale = y_row_scale;
							}
						}
						index++;
					}
					if (identifier[whitespace_characters[index]] == ' ') {
						position.x += character_spacing + space_x_scale;
					}
					else if (identifier[whitespace_characters[index]] == '\n') {
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

				if (configuration & UI_CONFIG_SENTENCE_FIT_SPACE) {
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
				if (configuration & UI_CONFIG_SENTENCE_FIT_SPACE) {
					SetDrawMode(UIDrawerMode::FitSpace);
				}

				float row_y_scale = current_row_y_scale;

				UIDrawerSentenceCached* data = (UIDrawerSentenceCached*)resource;

				if (ValidatePosition(configuration, position)) {
					float character_spacing;
					Color font_color;
					float2 font_size;
					HandleText(configuration, config, font_color, font_size, character_spacing);

					if (data->base.vertices[0].color != font_color) {
						for (size_t index = 0; index < data->base.vertices.size; index++) {
							data->base.vertices[index].color = font_color;
						}
					}
					auto text_sprite_buffer = HandleTextSpriteBuffer(configuration);
					auto text_sprite_counts = HandleTextSpriteCount(configuration);

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
								auto text_sprite_stream = GetTextStream(configuration, 0);
								ScaleText(temp_stream, data->zoom, data->inverse_zoom, text_sprite_buffer, text_sprite_counts, zoom_ptr, character_spacing);
								memcpy(temp_stream.buffer, text_sprite_stream.buffer, sizeof(UISpriteVertex) * temp_stream.size);
							}
							text_span = GetTextSpan(temp_stream);

							HandleFitSpaceRectangle(configuration, position, text_span);
							if (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
								position.y = AlignMiddle(position.y, row_y_scale, text_span.y);
							}

							if (position.x != temp_stream[0].position.x || position.y != -temp_stream[0].position.y) {
								TranslateText(position.x, position.y, temp_stream, 0, 0);
							}
							memcpy(text_sprite_buffer + *text_sprite_counts, temp_stream.buffer, sizeof(UISpriteVertex) * temp_stream.size);
							*text_sprite_counts += temp_stream.size;

							FinalizeRectangle(0, position, text_span);
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
								if (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
									current_row_y_scale = row_y_scale;
								}
							}
							index++;
						}
						if (data->base.whitespace_characters[index].type == ' ') {
							position.x += character_spacing + space_x_scale;
						}
						else if (data->base.whitespace_characters[index].type == '\n') {
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

					if (configuration & UI_CONFIG_SENTENCE_FIT_SPACE) {
						draw_mode = previous_draw_mode;
						draw_mode_count = previous_draw_count;
						draw_mode_target = previous_draw_target;
					}
				}
			}

			if (configuration & UI_CONFIG_SENTENCE_HOVERABLE_HANDLERS) {
				ECS_ASSERT(line_count == hoverables->count);
			}
			if (configuration & UI_CONFIG_SENTENCE_CLICKABLE_HANDLERS) {
				ECS_ASSERT(line_count == clickables->count);
			}
			if (configuration & UI_CONFIG_SENTENCE_GENERAL_HANDLERS) {
				ECS_ASSERT(line_count == generals->count);
			}

#pragma endregion

		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextTable* UIDrawer::TextTableInitializer(size_t configuration, const UIDrawConfig& config, const char* name, unsigned int rows, unsigned int columns, const char** labels) {
			UIDrawerTextTable* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				data = GetMainAllocatorBuffer<UIDrawerTextTable>();

				data->zoom = *zoom_ptr;
				data->inverse_zoom = zoom_inverse;

				unsigned int cell_count = rows * columns;
				size_t temp_marker = GetTempAllocatorMarker();
				unsigned int* label_sizes = (unsigned int*)GetTempBuffer(sizeof(unsigned int) * cell_count, alignof(unsigned int));
				size_t total_memory_size = 0;
				for (size_t index = 0; index < cell_count; index++) {
					label_sizes[index] = strlen(labels[index]);
					total_memory_size += label_sizes[index];
				}

				size_t total_char_count = total_memory_size;
				total_memory_size *= 6 * sizeof(UISpriteVertex);
				total_memory_size += sizeof(Stream<UISpriteVertex>) * cell_count;

				if (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
					total_memory_size += sizeof(float) * columns;
				}

				void* allocation = GetMainAllocatorBuffer(total_memory_size);
				uintptr_t buffer = (uintptr_t)allocation;

				data->labels.buffer = (Stream<UISpriteVertex>*)buffer;
				data->labels.size = cell_count;
				buffer += sizeof(Stream<UISpriteVertex>) * cell_count;

				if (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
					data->column_scales.buffer = (float*)buffer;
					buffer += sizeof(float) * columns;
				}

				Color font_color;
				float character_spacing;
				float2 font_size;

				HandleText(configuration, config, font_color, font_size, character_spacing);

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
					if (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
						float2 text_span = GetTextSpan(data->labels[index]);
						unsigned int column_index = index % columns;
						if (data->column_scales[column_index] < text_span.x + ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR * element_descriptor.label_horizontal_padd) {
							data->column_scales[column_index] = text_span.x + ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR * element_descriptor.label_horizontal_padd;
						}
						data->row_scale = text_span.y + element_descriptor.label_vertical_padd * ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR;
					}
				}

				ReturnTempAllocator(temp_marker);
				return data;
				});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::TextTableDrawer(
			size_t configuration,
			const UIDrawConfig& config,
			float2 position,
			float2 scale,
			UIDrawerTextTable* data,
			unsigned int rows,
			unsigned int columns
		) {
			if (~configuration & UI_CONFIG_TEXT_TABLE_DO_NOT_INFER) {
				scale = GetSquareScale(scale.y * ECS_TOOLS_UI_TEXT_TABLE_INFER_FACTOR);
			}

			if (zoom_ptr->x != data->zoom.x || zoom_ptr->y != data->zoom.y) {
				auto text_sprite_stream = GetTextStream(configuration, 0);

				Color font_color;
				float character_spacing;
				float2 font_size;

				HandleText(configuration, config, font_color, font_size, character_spacing);
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

			if (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
				scale.y = data->row_scale;
			}

			float2 initial_position = position;
			auto text_sprite_buffer = HandleTextSpriteBuffer(configuration);
			auto text_sprite_count = HandleTextSpriteCount(configuration);

			Color border_color;
			float2 border_size;

			if (configuration & UI_CONFIG_BORDER) {
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
				if (((configuration & UI_CONFIG_TEXT_TABLE_NO_BORDER) != 0) && ((configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) == 0)) {
					CreateSolidColorRectangleBorder<false>(position, { scale.x * columns, scale.y }, border_size, border_color, counts, buffers);
				}

				for (size_t column = 0; column < columns; column++) {
					if (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
						scale.x = data->column_scales[column];
					}

					if (ValidatePosition(0, position, scale)) {
						unsigned int cell_index = row * columns + column;
						float2 text_span = GetTextSpan(data->labels[cell_index]);
						float x_position, y_position;
						HandleTextLabelAlignment(text_span, scale, position, x_position, y_position, TextAlignment::Middle, TextAlignment::Middle);

						TranslateText(x_position, y_position, data->labels[cell_index], 0, 0);
						memcpy(text_sprite_buffer + *text_sprite_count, data->labels[cell_index].buffer, sizeof(UISpriteVertex) * data->labels[cell_index].size);
						*text_sprite_count += data->labels[cell_index].size;

						if (configuration & UI_CONFIG_COLOR) {
							SolidColorRectangle(configuration, config, position, scale);
						}
					}
					FinalizeRectangle(0, position, scale);
					position.x += scale.x;
				}
				position.x = initial_position.x;
				position.y += scale.y;
			}

			position = initial_position;
			if (~configuration & UI_CONFIG_TEXT_TABLE_NO_BORDER) {
				for (size_t column = 0; column < columns; column++) {
					if (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
						scale.x = data->column_scales[column];
					}
					CreateSolidColorRectangleBorder<false>(position, { scale.x, scale.y * rows }, border_size, border_color, counts, buffers);
					position.x += scale.x;
				}
			}

			if (((configuration & UI_CONFIG_TEXT_TABLE_NO_BORDER) == 0) && ((configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE))) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::TextTableDrawer(
			size_t configuration,
			const UIDrawConfig& config,
			float2 position,
			float2 scale,
			unsigned int rows,
			unsigned int columns,
			const char**& labels
		) {
			if (~configuration & UI_CONFIG_TEXT_TABLE_DO_NOT_INFER) {
				scale = GetSquareScale(scale.y * ECS_TOOLS_UI_TEXT_TABLE_INFER_FACTOR);
			}

			unsigned int cell_count = rows * columns;

			size_t temp_marker = GetTempAllocatorMarker();
			Stream<UISpriteVertex>* cells = (Stream<UISpriteVertex>*)GetTempBuffer(sizeof(Stream<UISpriteVertex>) * cell_count);

			unsigned int* label_sizes = (unsigned int*)GetTempBuffer(sizeof(unsigned int) * cell_count);

			Color font_color;
			float character_spacing;
			float2 font_size;

			HandleText(configuration, config, font_color, font_size, character_spacing);

			Color border_color;
			float2 border_size;
			if (configuration & UI_CONFIG_BORDER) {
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

			if (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
				float* cell_scales = (float*)GetTempBuffer(sizeof(float) * cell_count);

				for (size_t index = 0; index < columns; index++) {
					column_biggest_scale[index] = 0.0f;
				}

				float y_text_span;
				auto text_sprite_count = HandleTextSpriteCount(configuration);
				size_t before_count = *text_sprite_count;
				for (size_t row = 0; row < rows; row++) {
					for (size_t column = 0; column < columns; column++) {
						unsigned int index = row * columns + column;
						label_sizes[index] = strlen(labels[index]);
						cells[index] = GetTextStream(configuration, label_sizes[index] * 6);
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
						column_biggest_scale[column] = function::Select(current_x_scale > column_biggest_scale[column], current_x_scale, column_biggest_scale[column]);
						cell_scales[index] = text_span.x;
						y_text_span = text_span.y;
						scale.y = text_span.y + ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR * element_descriptor.label_vertical_padd;
						*text_sprite_count += cells[index].size;
					}
				}

				*text_sprite_count = before_count;
				unsigned int validated_cell_index = 0;

				auto text_sprite_buffer = HandleTextSpriteBuffer(configuration);
				for (size_t row = 0; row < rows; row++) {
					for (size_t column = 0; column < columns; column++) {
						scale.x = column_biggest_scale[column];
						if (ValidatePosition(0, position, scale)) {
							unsigned int cell_index = row * columns + column;
							float x_position, y_position;
							HandleTextLabelAlignment({ cell_scales[cell_index], y_text_span }, scale, position, x_position, y_position, TextAlignment::Middle, TextAlignment::Middle);

							TranslateText(x_position, y_position, cells[cell_index], 0, 0);
							if (validated_cell_index != cell_index) {
								memcpy(text_sprite_buffer + *text_sprite_count, cells[cell_index].buffer, sizeof(UISpriteVertex) * cells[cell_index].size);
							}
							validated_cell_index++;
							*text_sprite_count += cells[cell_index].size;

							if (configuration & UI_CONFIG_COLOR) {
								SolidColorRectangle(configuration, config, position, scale);
							}
						}
						FinalizeRectangle(0, position, scale);
						position.x += scale.x;
					}

					position.x = initial_position.x;
					position.y += scale.y;
				}

				if ((configuration & UI_CONFIG_TEXT_TABLE_NO_BORDER) == 0) {
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
			if (~configuration & UI_CONFIG_TEXT_TABLE_NO_BORDER) {
				for (size_t column = 0; column < columns; column++) {
					if (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
						scale.x = column_biggest_scale[column];
					}
					CreateSolidColorRectangleBorder<false>(position, { scale.x, scale.y * rows }, border_size, border_color, counts, buffers);
					position.x += scale.x;
				}
			}

			ReturnTempAllocator(temp_marker);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GraphYAxis(
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
		) {
			float y_sprite_scale = system->GetTextSpriteYScale(font_size.y);

			// upper corner value
			char temp_memory[128];
			Stream<char> temp_float_stream = Stream<char>(temp_memory, 0);
			function::ConvertFloatToChars(temp_float_stream, max_y, y_axis_precision);

			size_t* text_count = HandleTextSpriteCount(configuration);
			auto text_buffer = HandleTextSpriteBuffer(configuration);

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

			auto top_vertices_stream = GetTextStream(configuration, temp_float_stream.size * 6);
			float2 text_span = GetTextSpan(top_vertices_stream);
			max_x_scale = function::Select(max_x_scale < text_span.x, text_span.x, max_x_scale);
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

			auto bottom_vertices_stream = GetTextStream(configuration, temp_float_stream.size * 6);
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
			SpriteRectangle(
				configuration,
				{ top_text_position.x + max_x_scale + element_descriptor.graph_axis_bump.x * 0.5f, AlignMiddle(top_text_position.y, y_sprite_scale, element_descriptor.graph_axis_value_line_size.y) },
				{ position.x + scale.x - top_text_position.x - max_x_scale - element_descriptor.graph_padding.x, element_descriptor.graph_axis_value_line_size.y },
				ECS_TOOLS_UI_TEXTURE_MASK,
				font_color
			);

			// bottom text strip
			SpriteRectangle(
				configuration,
				{ bottom_text_position.x + max_x_scale + 0.001f, AlignMiddle(bottom_text_position.y, y_sprite_scale, element_descriptor.graph_axis_value_line_size.y) },
				{ position.x + scale.x - bottom_text_position.x - max_x_scale - element_descriptor.graph_padding.x, element_descriptor.graph_axis_value_line_size.y },
				ECS_TOOLS_UI_TEXTURE_MASK,
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

				auto temp_vertices = GetTextStream(configuration, temp_float_stream.size * 6);
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

				SpriteRectangle(
					configuration,
					{ sprite_position.x + max_x_scale, AlignMiddle(sprite_position.y, y_sprite_scale, element_descriptor.graph_axis_value_line_size.y) },
					{ position.x + scale.x - sprite_position.x - max_x_scale - element_descriptor.graph_padding.x, element_descriptor.graph_axis_value_line_size.y },
					ECS_TOOLS_UI_TEXTURE_MASK,
					font_color
				);

				sprite_position.y += y_sprite_scale + alignment_offset;
			}

			SpriteRectangle(
				configuration,
				{ top_text_position.x + max_x_scale + element_descriptor.graph_axis_bump.x, top_text_position.y + y_sprite_scale * 0.5f },
				{
					element_descriptor.graph_axis_value_line_size.x,
					bottom_text_position.y - top_text_position.y
				},
				ECS_TOOLS_UI_TEXTURE_MASK,
				font_color,
				{ 1.0f, 0.0f },
				{ 0.0f, 1.0f }
			);

			return max_x_scale + element_descriptor.graph_axis_value_line_size.x + 0.001f;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// it returns the left and the right text spans in order to adjust the graph scale and position
		float2 UIDrawer::GraphXAxis(
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
			bool disable_horizontal_line
		) {
			float y_sprite_scale = system->GetTextSpriteYScale(font_size.y);

			// upper corner value
			char temp_memory[128];
			Stream<char> temp_float_stream = Stream<char>(temp_memory, 0);
			function::ConvertFloatToChars(temp_float_stream, min_x, x_axis_precision);

			size_t* text_count = HandleTextSpriteCount(configuration);
			auto text_buffer = HandleTextSpriteBuffer(configuration);

			float text_y = position.y + scale.y - y_sprite_scale - element_descriptor.graph_padding.y;

			system->ConvertCharactersToTextSprites(
				temp_float_stream.buffer,
				{ position.x, text_y },
				text_buffer,
				temp_float_stream.size,
				font_color,
				*text_count,
				font_size,
				character_spacing
			);

			auto left_vertices_stream = GetTextStream(configuration, temp_float_stream.size * 6);
			float2 left_span = GetTextSpan(left_vertices_stream);
			*text_count += 6 * temp_float_stream.size;

			float2 vertical_line_scale = { element_descriptor.graph_axis_value_line_size.x, scale.y - 2 * element_descriptor.graph_padding.y - y_sprite_scale - element_descriptor.graph_axis_bump.y };
			if (!disable_horizontal_line) {
				SpriteRectangle(
					configuration,
					{ AlignMiddle(position.x, left_span.x, vertical_line_scale.x), position.y + element_descriptor.graph_padding.y },
					vertical_line_scale,
					ECS_TOOLS_UI_TEXTURE_MASK,
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

			SpriteRectangle(
				configuration,
				{ AlignMiddle(right_text_position.x, right_span.x, vertical_line_scale.x), position.y + element_descriptor.graph_padding.y },
				vertical_line_scale,
				ECS_TOOLS_UI_TEXTURE_MASK,
				font_color
			);

			float remaining_x_scale = scale.x - element_descriptor.graph_padding.x - (left_span.x + right_span.x);
			int64_t max_sprite_count = static_cast<int64_t>((remaining_x_scale) / (left_span.x + element_descriptor.graph_x_axis_space));
			int64_t min_sprite_count = static_cast<int64_t>((remaining_x_scale) / (right_span.x + element_descriptor.graph_x_axis_space));

			int64_t min_copy = min_sprite_count;
			min_sprite_count = function::Select(min_sprite_count > max_sprite_count, max_sprite_count, min_sprite_count);
			max_sprite_count = function::Select(max_sprite_count < min_copy, min_copy, max_sprite_count);
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

					auto current_vertices = GetTextStream(configuration, temp_float_stream.size * 6);
					float2 text_span = GetTextSpan(current_vertices);
					*text_count += 6 * temp_float_stream.size;

					float line_x = AlignMiddle(sprite_position.x, text_span.x, element_descriptor.graph_axis_value_line_size.x);
					SpriteRectangle(
						configuration,
						{ line_x, position.y + element_descriptor.graph_padding.y },
						vertical_line_scale,
						ECS_TOOLS_UI_TEXTURE_MASK,
						font_color
					);

					sprite_position.x += text_span.x + x_space;
				}
			}

			if (!disable_horizontal_line) {
				SpriteRectangle(
					configuration,
					{ position.x + left_span.x * 0.5f, text_y - element_descriptor.graph_axis_bump.y },
					{
						remaining_x_scale + (left_span.x + right_span.x) * 0.5f,
						element_descriptor.graph_axis_value_line_size.y
					},
					ECS_TOOLS_UI_TEXTURE_MASK,
					font_color,
					{ 1.0f, 0.0f },
					{ 0.0f, 1.0f }
				);
			}

			return { left_span.x, right_span.x };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::GraphDrawer(
			size_t configuration,
			const UIDrawConfig& config,
			const char* name,
			Stream<float2> samples,
			float2 position,
			float2 scale,
			size_t x_axis_precision,
			size_t y_axis_precision
		) {
			if (configuration & UI_CONFIG_GRAPH_KEEP_RESOLUTION_X) {
				const UIConfigGraphKeepResolutionX* data = (const UIConfigGraphKeepResolutionX*)config.GetParameter(UI_CONFIG_GRAPH_KEEP_RESOLUTION_X);

				float2 difference;
				difference.x = samples[samples.size - 1].x - samples[0].x;
				difference.y = 0.0f;

				float min_y = 1000000.0f;
				float max_y = -1000000.0f;
				if (configuration & UI_CONFIG_GRAPH_MIN_Y) {
					const UIConfigGraphMinY* min = (const UIConfigGraphMinY*)config.GetParameter(UI_CONFIG_GRAPH_MIN_Y);
					min_y = min->value;
				}
				if (configuration & UI_CONFIG_GRAPH_MAX_Y) {
					const UIConfigGraphMaxY* max = (const UIConfigGraphMaxY*)config.GetParameter(UI_CONFIG_GRAPH_MAX_Y);
					max_y = max->value;
				}
				for (size_t index = 0; index < samples.size; index++) {
					if (~configuration & UI_CONFIG_GRAPH_MIN_Y) {
						min_y = function::Select(min_y > samples[index].y, samples[index].y, min_y);
					}
					if (~configuration & UI_CONFIG_GRAPH_MAX_Y) {
						max_y = function::Select(max_y < samples[index].y, samples[index].y, max_y);
					}
				}

				difference.y = max_y - min_y;
				float ratio = difference.y / difference.x;
				scale.y = scale.x * ratio;
				scale.y = system->NormalizeVerticalToWindowDimensions(scale.y);
				scale.y = function::Select(scale.y > data->max_y_scale, data->max_y_scale, scale.y);
				scale.y = function::Select(scale.y < data->min_y_scale, data->min_y_scale, scale.y);
			}
			else if (configuration & UI_CONFIG_GRAPH_KEEP_RESOLUTION_Y) {
				const UIConfigGraphKeepResolutionY* data = (const UIConfigGraphKeepResolutionY*)config.GetParameter(UI_CONFIG_GRAPH_KEEP_RESOLUTION_Y);

				float2 difference;
				difference.x = samples[samples.size - 1].x - samples[0].x;
				difference.y = 0.0f;

				float min_y = 1000000.0f;
				float max_y = -1000000.0f;

				if ((configuration & UI_CONFIG_GRAPH_MIN_Y) == 0 || (configuration & UI_CONFIG_GRAPH_MAX_Y) == 0) {
					for (size_t index = 0; index < samples.size; index++) {
						if (~configuration & UI_CONFIG_GRAPH_MIN_Y) {
							min_y = function::Select(min_y > samples[index].y, samples[index].y, min_y);
						}
						if (~configuration & UI_CONFIG_GRAPH_MAX_Y) {
							max_y = function::Select(max_y < samples[index].y, samples[index].y, max_y);
						}
					}
				}

				if (configuration & UI_CONFIG_GRAPH_MIN_Y) {
					const UIConfigGraphMinY* min = (const UIConfigGraphMinY*)config.GetParameter(UI_CONFIG_GRAPH_MIN_Y);
					min_y = min->value;
				}
				if (configuration & UI_CONFIG_GRAPH_MAX_Y) {
					const UIConfigGraphMaxY* max = (const UIConfigGraphMaxY*)config.GetParameter(UI_CONFIG_GRAPH_MAX_Y);
					max_y = max->value;
				}

				difference.y = max_y - min_y;
				float ratio = difference.x / difference.y;
				scale.x = scale.y * ratio;
				scale.x = system->NormalizeHorizontalToWindowDimensions(scale.x);
				scale.x = function::Select(scale.x > data->max_x_scale, data->max_x_scale, scale.x);
				scale.x = function::Select(scale.x < data->min_x_scale, data->min_x_scale, scale.x);
			}

			size_t label_configuration = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;

			if (IsElementNameFirst(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
				if (name != nullptr) {
					TextLabel(label_configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, name, position, scale);
				}
			}

			HandleFitSpaceRectangle(configuration, position, scale);

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				float2 initial_scale = scale;
				float2 graph_scale = { scale.x - 2.0f * element_descriptor.graph_padding.x, scale.y - 2.0f * element_descriptor.graph_padding.y };
				float2 graph_position = { position.x + element_descriptor.graph_padding.x, position.y + element_descriptor.graph_padding.y };

				if (IsElementNameFirst(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
					if (name != nullptr) {
						TextLabel(label_configuration, config, name, position, scale);
						scale = initial_scale;
					}
				}

				float2 difference;
				difference.x = samples[samples.size - 1].x - samples[0].x;
				difference.y = 0.0f;

				float min_y = 1000000.0f;
				float max_y = -1000000.0f;
				if (configuration & UI_CONFIG_GRAPH_MIN_Y) {
					const UIConfigGraphMinY* min = (const UIConfigGraphMinY*)config.GetParameter(UI_CONFIG_GRAPH_MIN_Y);
					min_y = min->value;
				}
				if (configuration & UI_CONFIG_GRAPH_MAX_Y) {
					const UIConfigGraphMaxY* max = (const UIConfigGraphMaxY*)config.GetParameter(UI_CONFIG_GRAPH_MAX_Y);
					max_y = max->value;
				}
				for (size_t index = 0; index < samples.size; index++) {
					if (~configuration & UI_CONFIG_GRAPH_MIN_Y) {
						min_y = function::Select(min_y > samples[index].y, samples[index].y, min_y);
					}
					if (~configuration & UI_CONFIG_GRAPH_MAX_Y) {
						max_y = function::Select(max_y < samples[index].y, samples[index].y, max_y);
					}
				}


				difference.y = max_y - min_y;

				Color font_color;
				float character_spacing;
				float2 font_size;

				HandleText(configuration, config, font_color, font_size, character_spacing);
				font_color.alpha = ECS_TOOLS_UI_GRAPH_TEXT_ALPHA;

				if (configuration & UI_CONFIG_GRAPH_REDUCE_FONT) {
					font_size.x *= element_descriptor.graph_reduce_font;
					font_size.y *= element_descriptor.graph_reduce_font;
				}

				float2 axis_bump = { 0.0f, 0.0f };
				float y_sprite_size = system->GetTextSpriteYScale(font_size.y);
				if (configuration & UI_CONFIG_GRAPH_X_AXIS) {
					axis_bump.y = y_sprite_size;
				}

				if (configuration & UI_CONFIG_GRAPH_Y_AXIS) {
					float axis_span = GraphYAxis(
						configuration,
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
					if (~configuration & UI_CONFIG_GRAPH_X_AXIS) {
						graph_scale.y -= y_sprite_size * 0.5f;
					}
				}

				if (configuration & UI_CONFIG_GRAPH_X_AXIS) {
					float2 spans = GraphXAxis(
						configuration,
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
					if (~configuration & UI_CONFIG_GRAPH_Y_AXIS) {
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
				starting_index = function::Select(index <= 2, (int64_t)0, index - 2);
				while ((samples[index].x - min_x) * x_space_factor + graph_position.x < region_limit.x && index < samples.size) {
					index++;
				}
				end_index = function::Select(index >= samples.size - 2, (int64_t)samples.size, index + 3);

				SolidColorRectangle(configuration, config, position, scale);

				auto convert_absolute_position_to_graph_space = [](float2 position, float2 graph_position, float2 graph_scale,
					float2 min, float2 inverse_sample_span) {
						return float2(graph_position.x + graph_scale.x * (position.x - min.x) * inverse_sample_span.x, graph_position.y + graph_scale.y * (1.0f - (position.y - min.y) * inverse_sample_span.y));
				};

				float2 inverse_sample_span = { 1.0f / (samples[samples.size - 1].x - samples[0].x), 1.0f / (max_y - min_y) };
				float2 min_values = { samples[0].x, min_y };
				float2 previous_point_position = convert_absolute_position_to_graph_space(samples[starting_index], graph_position, graph_scale, min_values, inverse_sample_span);

				Color line_color;
				if (configuration & UI_CONFIG_GRAPH_LINE_COLOR) {
					const UIConfigGraphColor* color_desc = (const UIConfigGraphColor*)config.GetParameter(UI_CONFIG_GRAPH_LINE_COLOR);
					line_color = color_desc->color;
				}
				else {
					line_color = color_theme.graph_line;
				}
				Color initial_color = line_color;

				Stream<UISpriteVertex> drop_color_stream;
				if (configuration & UI_CONFIG_GRAPH_DROP_COLOR) {
					if (end_index > starting_index + 1) {
						int64_t line_count = end_index - starting_index - 1;
						SetSpriteClusterTexture(configuration, ECS_TOOLS_UI_TEXTURE_MASK, line_count);
						drop_color_stream = GetSpriteClusterStream(configuration, line_count * 6);
						auto sprite_count = HandleSpriteClusterCount(configuration);
						*sprite_count += line_count * 6;
					}
				}
				Stream<UISpriteVertex> sample_circle_stream;
				if (configuration & UI_CONFIG_GRAPH_SAMPLE_CIRCLES) {
					if (end_index > starting_index) {
						int64_t circle_count = end_index - starting_index;
						SetSpriteClusterTexture(configuration, ECS_TOOLS_UI_TEXTURE_FILLED_CIRCLE, circle_count);
						sample_circle_stream = GetSpriteClusterStream(configuration, circle_count * 6);
						auto sprite_count = HandleSpriteClusterCount(configuration);
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
					Line(configuration, previous_point_position, next_point, line_color);

					UIDrawerGraphHoverableData hoverable_data;
					hoverable_data.first_sample_values = samples[index];
					hoverable_data.second_sample_values = samples[index + 1];
					hoverable_data.sample_index = index;

					AddHoverable(hoverable_position, hoverable_scale, { GraphHoverable, &hoverable_data, sizeof(hoverable_data), UIDrawPhase::System });
					if (configuration & UI_CONFIG_GRAPH_DROP_COLOR) {
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
					if (configuration & UI_CONFIG_GRAPH_SAMPLE_CIRCLES) {
						float2 sprite_scale = GetSquareScale(element_descriptor.graph_sample_circle_size);
						size_t temp_index = (index - starting_index) * 6;
						SetSpriteRectangle({ previous_point_position.x - sprite_scale.x * 0.5f, previous_point_position.y - sprite_scale.y * 0.5f }, sprite_scale, color_theme.graph_sample_circle, { 0.0f, 0.0f }, { 1.0f, 1.0f }, sample_circle_stream.buffer, temp_index);
					}

					previous_point_position = next_point;
				}

				if (configuration & UI_CONFIG_GRAPH_SAMPLE_CIRCLES) {
					if (end_index > starting_index + 1) {
						float2 sprite_scale = GetSquareScale(element_descriptor.graph_sample_circle_size);
						size_t temp_index = (end_index - 1 - starting_index) * 6;
						SetSpriteRectangle({ previous_point_position.x - sprite_scale.x * 0.5f, previous_point_position.y - sprite_scale.y * 0.5f }, sprite_scale, color_theme.graph_sample_circle, { 0.0f, 0.0f }, { 1.0f, 1.0f }, sample_circle_stream.buffer, temp_index);
					}
				}

				if (configuration & UI_CONFIG_GRAPH_TAGS) {
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

			FinalizeRectangle(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, position, scale);

			if (IsElementNameAfter(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
				if (name != nullptr) {
					position.x += scale.x + layout.element_indentation;
					TextLabel(label_configuration, config, name, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// it returns the index of the sprite with the smallest x
		template<bool left>
		unsigned int UIDrawer::LevelVerticalTextX(Stream<UISpriteVertex> vertices) const {
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

		ECS_TEMPLATE_FUNCTION_BOOL_CONST(unsigned int, UIDrawer::LevelVerticalTextX, Stream<UISpriteVertex>);

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::TextLabelWithSize(size_t configuration, const UIDrawConfig& config, const char* text, float2 position) {
			float2 scale;
			size_t text_count = strlen(text);

			Stream<UISpriteVertex> current_text = GetTextStream(configuration, text_count * 6);
			float2 text_position = { position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd };
			Text(configuration, config, text, text_position, scale);
			scale.x += 2 * element_descriptor.label_horizontal_padd;
			scale.y += 2 * element_descriptor.label_vertical_padd;

			if (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
				position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
			}

			TextAlignment horizontal_alignment = TextAlignment::Middle, vertical_alignment = TextAlignment::Top;
			float x_text_position, y_text_position;
			float2 text_span;
			if (configuration & UI_CONFIG_VERTICAL) {
				text_span = GetTextSpan<true>(current_text);
			}
			else {
				text_span = GetTextSpan(current_text);
			}

			HandleTextLabelAlignment(
				configuration,
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
				for (size_t index = 0; index < current_text.size; index++) {
					current_text[index].position.x += x_translation;
				}
			}
			if (vertical_alignment != TextAlignment::Top) {
				float y_translation = y_text_position - position.y - element_descriptor.label_vertical_padd;
				for (size_t index = 0; index < current_text.size; index++) {
					current_text[index].position.y += y_translation;
				}
			}
			return scale;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FixedScaleTextLabel(size_t configuration, const UIDrawConfig& config, const char* text, float2 position, float2 scale) {
			if (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
				position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
			}
			Stream<UISpriteVertex> current_text = GetTextStream(configuration, ParseStringIdentifier(text, strlen(text)) * 6);
			float2 text_span;

			float2 temp_position = { position.x + element_descriptor.label_horizontal_padd, position.y + element_descriptor.label_vertical_padd };
			Text(configuration | UI_CONFIG_DO_NOT_FIT_SPACE, config, text, temp_position, text_span);

			TextAlignment horizontal_alignment = TextAlignment::Middle, vertical_alignment = TextAlignment::Top;
			GetTextLabelAlignment(configuration, config, horizontal_alignment, vertical_alignment);
			bool translate = true;

			auto text_sprite_count = HandleTextSpriteCount(configuration);
			if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				const WindowSizeTransformType* type = (const WindowSizeTransformType*)config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
				if (*type == WindowSizeTransformType::Both) {
					if (configuration & UI_CONFIG_VERTICAL) {
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
					if (configuration & UI_CONFIG_VERTICAL) {
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
				HandleTextLabelAlignment(
					configuration,
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
			if (configuration & UI_CONFIG_VERTICAL) {
				AlignVerticalText(current_text);
			}

			if (~configuration & UI_CONFIG_LABEL_TRANSPARENT) {
				Color label_color = HandleColor(configuration, config);
				SolidColorRectangle(
					configuration,
					position,
					scale,
					label_color
				);
			}

			HandleLateAndSystemDrawActionNullify(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// this will add automatically VerticalSlider and HorizontalSlider to name
		void UIDrawer::ViewportRegionSliderInitializer(
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
			*vertical_slider = FloatSliderInitializer(
				UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_SLIDER_NO_TEXT | UI_CONFIG_VERTICAL | UI_CONFIG_DO_NOT_ADVANCE,
				null_config,
				stack_memory,
				{ 0.0f, 0.0f },
				{ 0.0f, 0.0f },
				&region_offset->y,
				0.0f,
				10.0f
			);

			stack_memory[name_size] = '\0';
			strcat(stack_memory, "HorizontalSlider");
			stack_memory[name_size + 16] = '\0';
			// bounds, position and scale here don't matter
			*horizontal_slider = FloatSliderInitializer(
				UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_SLIDER_NO_TEXT | UI_CONFIG_DO_NOT_ADVANCE,
				null_config,
				stack_memory,
				{ 0.0f, 0.0f },
				{ 0.0f, 0.0f },
				&region_offset->x,
				0.0f,
				10.0f
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ViewportRegionSliders(
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

				size_t slider_configuration = UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_SLIDER_NO_TEXT | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_COLOR
					| UI_CONFIG_SLIDER_COLOR | UI_CONFIG_LATE_DRAW | UI_CONFIG_SLIDER_SHRINK | UI_CONFIG_SLIDER_LENGTH | UI_CONFIG_SLIDER_PADDING
					| UI_CONFIG_DO_NOT_VALIDATE_POSITION | UI_CONFIG_DO_NOT_FIT_SPACE;

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

				FloatSliderDrawer(
					slider_configuration,
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
				system->m_windows[window_index].render_region_offset.x = 0.0f;
				system->m_windows[window_index].is_horizontal_render_slider = false;
			}

			UIConfigAbsoluteTransform vertical_transform;
			if ((!is_horizontal_extended && is_vertical_extended && !is_horizontal) || (is_vertical && is_horizontal)) {
				float difference = render_span.y - render_zone.y;
				if (!is_horizontal_extended && is_vertical_extended && !is_horizontal) {
					difference -= system->m_descriptors.misc.render_slider_horizontal_size;
				}

				constexpr size_t slider_configuration = UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_SLIDER_NO_TEXT | UI_CONFIG_VERTICAL | UI_CONFIG_COLOR
					| UI_CONFIG_SLIDER_COLOR | UI_CONFIG_LATE_DRAW | UI_CONFIG_SLIDER_SHRINK | UI_CONFIG_SLIDER_LENGTH | UI_CONFIG_SLIDER_PADDING
					| UI_CONFIG_DO_NOT_VALIDATE_POSITION | UI_CONFIG_DO_NOT_FIT_SPACE;

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

				FloatSliderDrawer(
					slider_configuration,
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
				system->m_windows[window_index].render_region_offset.y = 0.0f;
				system->m_windows[window_index].is_vertical_render_slider = false;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ViewportRegionSliders(
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteRectangle(
			size_t configuration,
			float2 position,
			float2 scale,
			const wchar_t* texture,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			if (!initializer) {
				if (configuration & UI_CONFIG_LATE_DRAW) {
					system->SetSprite(
						dockspace,
						border_index,
						texture,
						position,
						scale,
						buffers + ECS_TOOLS_UI_MATERIALS,
						counts + ECS_TOOLS_UI_MATERIALS,
						color,
						top_left_uv,
						bottom_right_uv,
						UIDrawPhase::Late
					);
				}
				else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
					system->SetSprite(
						dockspace,
						border_index,
						texture,
						position,
						scale,
						system_buffers,
						system_counts,
						color,
						top_left_uv,
						bottom_right_uv,
						UIDrawPhase::System
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
						color,
						top_left_uv,
						bottom_right_uv,
						UIDrawPhase::Normal
					);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteRectangle(size_t configuration, const UIDrawConfig& config, ResourceView view, Color color, float2 top_left_uv, float2 bottom_right_uv)
		{
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			SpriteRectangle(configuration, position, scale, view, color, top_left_uv, bottom_right_uv);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteRectangle(size_t configuration, float2 position, float2 scale, ResourceView view, Color color, float2 top_left_uv, float2 bottom_right_uv)
		{
			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (ValidatePosition(configuration, position, scale)) {
					if (configuration & UI_CONFIG_LATE_DRAW) {
						system->SetSprite(
							dockspace,
							border_index,
							view,
							position,
							scale,
							buffers + ECS_TOOLS_UI_MATERIALS,
							counts + ECS_TOOLS_UI_MATERIALS,
							color,
							top_left_uv,
							bottom_right_uv,
							UIDrawPhase::Late
						);
					}
					else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
						system->SetSprite(
							dockspace,
							border_index,
							view,
							position,
							scale,
							system_buffers,
							system_counts,
							color,
							top_left_uv,
							bottom_right_uv,
							UIDrawPhase::System
						);
					}
					else {
						system->SetSprite(
							dockspace,
							border_index,
							view,
							position,
							scale,
							buffers,
							counts,
							color,
							top_left_uv,
							bottom_right_uv,
							UIDrawPhase::Normal
						);
					}
				}

				FinalizeRectangle(configuration, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorRectangle(
			size_t configuration,
			float2 position,
			float2 scale,
			Color top_left,
			Color top_right,
			Color bottom_left,
			Color bottom_right
		) {
			if (!initializer) {
				auto buffer = HandleSolidColorBuffer(configuration);
				auto count = HandleSolidColorCount(configuration);

				SetVertexColorRectangle(position, scale, top_left, top_right, bottom_left, bottom_right, buffer, count);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorRectangle(
			size_t configuration,
			float2 position,
			float2 scale,
			ColorFloat top_left,
			ColorFloat top_right,
			ColorFloat bottom_left,
			ColorFloat bottom_right
		) {
			if (!initializer) {
				auto buffer = HandleSolidColorBuffer(configuration);
				auto count = HandleSolidColorCount(configuration);

				SetVertexColorRectangle(position, scale, Color(top_left), Color(top_right), Color(bottom_left), Color(bottom_right), buffer, count);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorRectangle(
			size_t configuration,
			float2 position,
			float2 scale,
			const Color* colors
		) {
			if (!initializer) {
				auto buffer = HandleSolidColorBuffer(configuration);
				auto count = HandleSolidColorCount(configuration);

				SetVertexColorRectangle(position, scale, colors, buffer, count);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorRectangle(
			size_t configuration,
			float2 position,
			float2 scale,
			const ColorFloat* colors
		) {
			if (!initializer) {
				auto buffer = HandleSolidColorBuffer(configuration);
				auto count = HandleSolidColorCount(configuration);

				SetVertexColorRectangle(position, scale, Color(colors[0]), Color(colors[1]), Color(colors[2]), Color(colors[3]), buffer, count);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			LPCWSTR texture,
			const Color* colors,
			const float2* uvs,
			UIDrawPhase phase
		) {
			system->SetVertexColorSprite(dockspace, border_index, texture, position, scale, buffers, counts, colors, uvs[0], uvs[1], phase);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const wchar_t* texture,
			const Color* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			UIDrawPhase phase
		) {
			system->SetVertexColorSprite(dockspace, border_index, texture, position, scale, buffers, counts, colors, top_left_uv, bottom_right_uv, phase);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const wchar_t* texture,
			const ColorFloat* colors,
			const float2* uvs,
			UIDrawPhase phase
		) {
			system->SetVertexColorSprite(dockspace, border_index, texture, position, scale, buffers, counts, colors, uvs[0], uvs[1], phase);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			LPCWSTR texture,
			const ColorFloat* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			UIDrawPhase phase
		) {
			system->SetVertexColorSprite(dockspace, border_index, texture, position, scale, buffers, counts, colors, top_left_uv, bottom_right_uv, phase);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		Stream<UIVertexColor> UIDrawer::GetSolidColorStream(size_t configuration, size_t size) {
			return Stream<UIVertexColor>(HandleSolidColorBuffer(configuration) + *HandleSolidColorCount(configuration), size);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		Stream<UISpriteVertex> UIDrawer::GetTextStream(size_t configuration, size_t size) {
			return Stream<UISpriteVertex>(HandleTextSpriteBuffer(configuration) + *HandleTextSpriteCount(configuration), size);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		Stream<UISpriteVertex> UIDrawer::GetSpriteStream(size_t configuration, size_t size) {
			return Stream<UISpriteVertex>(HandleSpriteBuffer(configuration) + *HandleSpriteCount(configuration), size);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		Stream<UISpriteVertex> UIDrawer::GetSpriteClusterStream(size_t configuration, size_t size) {
			return Stream<UISpriteVertex>(HandleSpriteClusterBuffer(configuration) + *HandleSpriteClusterCount(configuration), size);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::TextSpan(Stream<char> characters, float2 font_size, float character_spacing) {
			return system->GetTextSpan(characters.buffer, characters.size, font_size.x, font_size.y, character_spacing);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::TextSpan(const char* characters, float2 font_size, float character_spacing) {
			return TextSpan(Stream<char>(characters, strlen(characters)), font_size, character_spacing);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::TextSpan(Stream<char> characters) {
			return TextSpan(characters, GetFontSize(), font.character_spacing);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::TextSpan(const char* characters) {
			return TextSpan(characters, GetFontSize(), font.character_spacing);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetLabelScale(Stream<char> characters) {
			return HandleLabelSize(TextSpan(characters));
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetLabelScale(const char* characters) {
			return HandleLabelSize(TextSpan(characters));
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetLabelScale(Stream<char> characters, float2 font_size, float character_spacing) {
			return HandleLabelSize(TextSpan(characters, font_size, character_spacing));
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetLabelScale(const char* characters, float2 font_size, float character_spacing) {
			return HandleLabelSize(TextSpan(characters, font_size, character_spacing));
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::GetTextLabelAlignment(size_t configuration, const UIDrawConfig& config, TextAlignment& horizontal, TextAlignment& vertical) {
			if (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
				const float* params = (const float*)config.GetParameter(UI_CONFIG_TEXT_ALIGNMENT);
				const TextAlignment* alignments = (TextAlignment*)params;
				horizontal = *alignments;
				vertical = *(alignments + 1);
			}
			else {
				horizontal = TextAlignment::Middle;
				vertical = TextAlignment::Middle;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GetXScaleUntilBorder(float position) const {
			return region_limit.x - position;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GetNextRowXPosition() const {
			return region_position.x + layout.next_row_padding + next_row_offset /*- region_render_offset.x*/;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t UIDrawer::GetRandomIntIdentifier(size_t index) {
			return (index * index + (index & 15)) << (index & 7) >> 2 << 5;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::UpdateCurrentColumnScale(float value) {
			current_column_x_scale = function::Select(value > current_column_x_scale, value, current_column_x_scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::UpdateCurrentRowScale(float value) {
			current_row_y_scale = function::Select(value > current_row_y_scale, value, current_row_y_scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::UpdateCurrentScale(float2 position, float2 value) {
			// currently only ColumnDraw and ColumnDrawFitSpace require z component
			draw_mode_extra_float.z = value.y;
			if (draw_mode != UIDrawerMode::ColumnDraw && draw_mode != UIDrawerMode::ColumnDrawFitSpace) {
				UpdateCurrentRowScale(position.y - current_y + value.y + region_render_offset.y);
			}
			UpdateCurrentColumnScale(position.x - current_x + value.x + region_render_offset.x);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::UpdateMaxRenderBoundsRectangle(float2 limits) {
			max_render_bounds.x = function::Select(max_render_bounds.x < limits.x, limits.x, max_render_bounds.x);
			max_render_bounds.y = function::Select(max_render_bounds.y < limits.y, limits.y, max_render_bounds.y);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::UpdateMinRenderBoundsRectangle(float2 position) {
			min_render_bounds.x = function::Select(min_render_bounds.x > position.x, position.x, min_render_bounds.x);
			min_render_bounds.y = function::Select(min_render_bounds.y > position.y, position.y, min_render_bounds.y);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::UpdateRenderBoundsRectangle(float2 position, float2 scale) {
			float2 limits = { position.x + scale.x, position.y + scale.y };
			UpdateMaxRenderBoundsRectangle(limits);
			UpdateMinRenderBoundsRectangle(position);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::VerifyFitSpaceNonRectangle(size_t vertex_count) const {
			UIVertexColor* vertices = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
			size_t last_index = counts[ECS_TOOLS_UI_SOLID_COLOR] + vertex_count;
			for (size_t index = counts[ECS_TOOLS_UI_SOLID_COLOR]; index < last_index; index++) {
				if (vertices[index].position.x > region_limit.x - region_render_offset.x) {
					return false;
				}
			}
			return true;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::VerifyFitSpaceRectangle(float2 position, float2 scale) const {
			return position.x + scale.x > region_limit.x - region_render_offset.x;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleDrawMode(size_t configuration) {
			switch (draw_mode) {
			case UIDrawerMode::Indent:
			case UIDrawerMode::FitSpace:
				Indent();
				break;
			case UIDrawerMode::NextRow:
				if (~configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW) {
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
					if (~configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW) {
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
					current_row_y_scale = function::Select(draw_mode_count == 0, -draw_mode_extra_float.x, current_row_y_scale);
					current_y += draw_mode_extra_float.z + draw_mode_extra_float.x;
					draw_mode_count++;
					current_row_y_scale += draw_mode_extra_float.z + draw_mode_extra_float.x;
				}
				break;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FinalizeRectangle(size_t configuration, float2 position, float2 scale) {
			if (~configuration & UI_CONFIG_DO_NOT_ADVANCE) {
				UpdateRenderBoundsRectangle(position, scale);
				UpdateCurrentScale(position, scale);
				HandleDrawMode(configuration);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ElementName(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2 position, float2 scale) {
			size_t label_configuration = UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_DO_NOT_FIT_SPACE;
			UIDrawConfig label_config;
			UIConfigTextAlignment alignment;

			alignment.horizontal = TextAlignment::Left;
			alignment.vertical = TextAlignment::Middle;
			label_config.AddFlag(alignment);

			if (configuration & UI_CONFIG_TEXT_PARAMETERS) {
				const UIConfigTextParameters* parameters = (const UIConfigTextParameters*)config.GetParameter(UI_CONFIG_TEXT_PARAMETERS);
				label_config.AddFlag(*parameters);
			}

			if (~configuration & UI_CONFIG_ELEMENT_NAME_FIRST) {
				position = {current_x, current_y};
				position.x -= region_render_offset.x;
				position.y -= region_render_offset.y;
				// next row is here because it will always be called with UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
				if (draw_mode == UIDrawerMode::Indent || draw_mode == UIDrawerMode::FitSpace || draw_mode == UIDrawerMode::NextRow) {
					position.x -= layout.element_indentation;
				}
			}

			bool is_active = true;
			if (configuration & UI_CONFIG_ACTIVE_STATE) {
				const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				is_active = active_state->state;
			}

			label_configuration |= function::Select<size_t>(configuration & UI_CONFIG_TEXT_PARAMETERS, UI_CONFIG_TEXT_PARAMETERS, 0);
			label_configuration |= function::Select<size_t>(is_active, 0, UI_CONFIG_UNAVAILABLE_TEXT);
			label_configuration |= function::Select<size_t>(configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, 0);

			TextLabel(label_configuration, label_config, text, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ElementName(size_t configuration, const UIDrawConfig& config, const char* text, float2 position, float2 scale) {
			size_t label_configuration = UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_CACHE;
			UIDrawConfig label_config;
			UIConfigTextAlignment alignment;

			alignment.horizontal = TextAlignment::Left;
			alignment.vertical = TextAlignment::Middle;
			label_config.AddFlag(alignment);

			if (configuration & UI_CONFIG_TEXT_PARAMETERS) {
				const UIConfigTextParameters* parameters = (const UIConfigTextParameters*)config.GetParameter(UI_CONFIG_TEXT_PARAMETERS);
				label_config.AddFlag(*parameters);
			}

			if (~configuration & UI_CONFIG_ELEMENT_NAME_FIRST) {
				position = {current_x, current_y};
				position.x -= region_render_offset.x;
				position.y -= region_render_offset.y;
				// next row is here because it will always be called with UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
				if (draw_mode == UIDrawerMode::Indent || draw_mode == UIDrawerMode::FitSpace || draw_mode == UIDrawerMode::NextRow) {
					position.x -= layout.element_indentation;
				}
			}

			bool is_active = true;
			if (configuration & UI_CONFIG_ACTIVE_STATE) {
				const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				is_active = active_state->state;
			}

			label_configuration |= function::Select<size_t>(configuration & UI_CONFIG_TEXT_PARAMETERS, UI_CONFIG_TEXT_PARAMETERS, 0);
			label_configuration |= function::Select<size_t>(is_active, 0, UI_CONFIG_UNAVAILABLE_TEXT);
			label_configuration |= function::Select<size_t>(configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, 0);

			TextLabel(label_configuration, label_config, text, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::ExistsResource(const char* name) {
			const char* string_identifier = HandleResourceIdentifier(name);
			return system->ExistsWindowResource(window_index, string_identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawer::UIDrawer(
			UIColorThemeDescriptor& _color_theme,
			UIFontDescriptor& _font,
			UILayoutDescriptor& _layout,
			UIElementDescriptor& _element_descriptor,
			bool _initializer
		) : color_theme(_color_theme), font(_font), layout(_layout), element_descriptor(_element_descriptor),
			export_scale(nullptr), initializer(_initializer) {};

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawer::UIDrawer(
			UIDrawerDescriptor& descriptor,
			void* _window_data,
			bool _initializer
		) : dockspace(descriptor.dockspace), thread_id(descriptor.thread_id), window_index(descriptor.window_index),
			border_index(descriptor.border_index), dockspace_type(descriptor.dockspace_type), buffers(descriptor.buffers),
			counts(descriptor.counts), system_buffers(descriptor.system_buffers), system_counts(descriptor.system_counts),
			window_data(_window_data), mouse_position(descriptor.mouse_position), color_theme(descriptor.color_theme),
			font(descriptor.font), layout(descriptor.layout), element_descriptor(descriptor.element_descriptor),
			export_scale(descriptor.export_scale), initializer(_initializer)
		{
			system = (UISystem*)descriptor.system;

			zoom_ptr = &system->m_windows[window_index].zoom;
			zoom_ptr_modifyable = &system->m_windows[window_index].zoom;

			DefaultDrawParameters();

			//SetLayoutFromZoomXFactor<true>(zoom_ptr->x);
			//SetLayoutFromZoomYFactor<true>(zoom_ptr->y);

			if (!initializer) {
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

			if (!descriptor.do_not_allocate_buffers) {
				identifier_stack.buffer = (unsigned char*)system->m_memory->Allocate(system->m_descriptors.misc.drawer_temp_memory, 1);
				identifier_stack.size = 0;
				current_identifier.buffer = (char*)system->m_memory->Allocate(system->m_descriptors.misc.drawer_identifier_memory, 1);
				current_identifier.size = 0;


				if (initializer) {
					// For convinience, use separate allocations
					last_initialized_element_allocations.Initialize(system->m_memory, 0, ECS_TOOLS_UI_MISC_DRAWER_ELEMENT_ALLOCATIONS);
					last_initialized_element_table_resources.Initialize(system->m_memory, 0, ECS_TOOLS_UI_MISC_DRAWER_ELEMENT_ALLOCATIONS);
				}
			}
			else {
				deallocate_constructor_allocations = false;
			}

			if (initializer) {
				// creating temporaries to hold the values, even tho they are unneeded
				if (!descriptor.do_not_initialize_viewport_sliders) {
					void* temp_horizontal_slider;
					void* temp_vertical_slider;
					ViewportRegionSliderInitializer(
						&system->m_windows[window_index].render_region_offset,
						ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT,
						&temp_horizontal_slider,
						&temp_vertical_slider
					);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawer::~UIDrawer() {
			if (!initializer) {
				float2 render_span = GetRenderSpan();
				float2 render_zone = GetRenderZone();

				float2 difference = render_span - render_zone;
				difference.x = function::ClampMin(difference.x, 0.0f);
				difference.y = function::ClampMin(difference.y, 0.0f);
				system->SetWindowDrawerDifferenceSpan(window_index, difference);

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
					&system->m_windows[window_index].render_region_offset,
					horizontal_slider_position,
					horizontal_slider_scale,
					vertical_slider_position,
					vertical_slider_scale
				);
			}

			if (deallocate_constructor_allocations) {
				system->m_memory->Deallocate(current_identifier.buffer);
				system->m_memory->Deallocate(identifier_stack.buffer);

				if (initializer) {
					system->m_memory->Deallocate(last_initialized_element_allocations.buffer);
					system->m_memory->Deallocate(last_initialized_element_table_resources.buffer);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddWindowResourceToTable(void* resource, ResourceIdentifier identifier) {
			system->AddWindowMemoryResourceToTable(resource, identifier, window_index);
			if (initializer) {
				if (last_initialized_element_table_resources.size < last_initialized_element_table_resources.capacity) {
					void* temp_identifier = GetTempBuffer(identifier.size);
					memcpy(temp_identifier, identifier.ptr, identifier.size);
					last_initialized_element_table_resources.Add({ temp_identifier, identifier.size });
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddHoverable(float2 position, float2 scale, UIActionHandler handler) {
			if (!initializer) {
				system->AddHoverableToDockspaceRegion(
					thread_id,
					dockspace,
					border_index,
					position,
					scale,
					handler
				);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddTextTooltipHoverable(float2 position, float2 scale, UITextTooltipHoverableData* data, UIDrawPhase phase) {
			system->AddHoverableToDockspaceRegion(thread_id, dockspace, border_index, position, scale, { TextTooltipHoverable, data, sizeof(*data), phase });
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddClickable(
			float2 position,
			float2 scale,
			UIActionHandler handler
		) {
			if (!initializer) {
				system->AddClickableToDockspaceRegion(
					thread_id,
					dockspace,
					border_index,
					position,
					scale,
					handler
				);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddGeneral(
			float2 position,
			float2 scale,
			UIActionHandler handler
		) {
			if (!initializer) {
				system->AddGeneralActionToDockspaceRegion(
					thread_id,
					dockspace,
					border_index,
					position,
					scale,
					handler
				);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultHoverable(
			float2 position,
			float2 scale,
			Color color,
			float percentage,
			UIDrawPhase phase
		) {
			if (!initializer) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultHoverable(
			float2 main_position,
			float2 main_scale,
			const float2* positions,
			const float2* scales,
			const Color* colors,
			const float* percentages,
			unsigned int count,
			UIDrawPhase phase
		) {
			if (!initializer) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultClickable(
			float2 position,
			float2 scale,
			UIActionHandler hoverable_handler,
			UIActionHandler clickable_handler
		) {
			if (!initializer) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDoubleClickAction(
			float2 position,
			float2 scale,
			unsigned int identifier,
			size_t duration_between_clicks,
			UIActionHandler first_click_handler,
			UIActionHandler second_click_handler
		) {
			if (!initializer) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultClickableHoverableWithText(
			float2 position,
			float2 scale,
			UIActionHandler handler,
			Color color,
			const char* text,
			float2 text_offset,
			UIDrawPhase phase,
			Color font_color,
			float percentage
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		// Either horizontal or vertical cull should be set
		void UIDrawer::AddDefaultClickableHoverableWithTextEx(
			float2 position,
			float2 scale,
			UIActionHandler handler,
			Color color,
			const char* text,
			float2 text_offset,
			bool horizontal_cull,
			float horizontal_cull_bound,
			bool vertical_cull,
			float vertical_cull_bound,
			bool vertical_text,
			UIDrawPhase phase,
			Color font_color,
			float percentage
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultClickableHoverable(
			float2 position,
			float2 scale,
			UIActionHandler handler,
			Color color,
			float percentage
		) {
			if (!initializer) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultClickableHoverable(
			float2 main_position,
			float2 main_scale,
			const float2* positions,
			const float2* scales,
			const Color* colors,
			const float* percentages,
			unsigned int count,
			UIActionHandler handler
		) {
			if (!initializer) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddWindowDependentSizeToConfigFromPoint(float2 initial_point, UIDrawConfig& config) const {
			float x_difference = current_x - region_render_offset.x - initial_point.x;
			float y_difference = current_y - region_render_offset.y - initial_point.y;

			UIConfigWindowDependentSize size;
			size.scale_factor = { x_difference / (region_limit.x - region_fit_space_horizontal_offset), y_difference / (region_limit.y - region_fit_space_vertical_offset) };
			config.AddFlag(size);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddRelativeTransformToConfigFromPoint(float2 initial_point, UIDrawConfig& config) const {
			float x_difference = current_x - region_render_offset.x - initial_point.x;
			float y_difference = current_y - region_render_offset.y - initial_point.y;

			UIConfigRelativeTransform transform;
			transform.scale.x = x_difference / layout.default_element_x;
			transform.scale.y = y_difference / layout.default_element_y;
			config.AddFlag(transform);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddAbsoluteTransformToConfigFromPoint(float2 initial_point, UIDrawConfig& config) const {
			float x_difference = current_x - region_render_offset.x - initial_point.x;
			float y_difference = current_y - region_render_offset.y - initial_point.y;

			UIConfigAbsoluteTransform transform;
			transform.position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
			transform.scale.x = x_difference;
			transform.scale.y = y_difference;
			config.AddFlag(transform);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerArrayData* UIDrawer::ArrayInitializer(size_t configuration, const UIDrawConfig& config, const char* name, size_t element_count) {
			UIDrawerArrayData* data = nullptr;

			ECS_TEMP_ASCII_STRING(data_name, 256);
			data_name.Copy(ToStream(name));
			data_name.AddStream(ToStream(" data"));
			data_name.AddSafe('\0');

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(data_name.buffer, [&](const char* identifier) {
				data = GetMainAllocatorBuffer<UIDrawerArrayData>();

				data->collapsing_header_state = false;
				data->drag_index = -1;
				data->drag_current_position = 0.0f;
				data->row_y_scale = 0.0f;
				data->previous_element_count = 0;
				data->add_callback = nullptr;
				data->add_callback_data = nullptr;
				data->remove_callback = nullptr;
				data->remove_callback_data = nullptr;
				data->add_callback_phase = UIDrawPhase::Normal;
				data->remove_callback_phase = UIDrawPhase::Normal;

				if (configuration & UI_CONFIG_ARRAY_ADD_CALLBACK) {
					const UIConfigArrayAddCallback* config_callback = (const UIConfigArrayAddCallback*)config.GetParameter(UI_CONFIG_ARRAY_ADD_CALLBACK);
					if (config_callback->handler.data_size > 0) {
						void* allocation = GetMainAllocatorBuffer(config_callback->handler.data_size);
						memcpy(allocation, config_callback->handler.data, config_callback->handler.data_size);
						data->add_callback_data = allocation;
					}
					else {
						data->add_callback_data = config_callback->handler.data;
					}
					data->add_callback = config_callback->handler.action;
					data->add_callback_phase = config_callback->handler.phase;
				}

				if (configuration & UI_CONFIG_ARRAY_REMOVE_CALLBACK) {
					const UIConfigArrayRemoveCallback* config_callback = (const UIConfigArrayRemoveCallback*)config.GetParameter(UI_CONFIG_ARRAY_REMOVE_CALLBACK);
					if (config_callback->handler.data_size > 0) {
						void* allocation = GetMainAllocatorBuffer(config_callback->handler.data_size);
						memcpy(allocation, config_callback->handler.data, config_callback->handler.data_size);
						data->remove_callback_data = allocation;
					}
					else {
						data->remove_callback_data = config_callback->handler.data;
					}
					data->remove_callback = config_callback->handler.action;
					data->remove_callback_phase = config_callback->handler.phase;
				}

				return data;
				});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Array Instantiations - 1 component

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayFloat(const char* name, CapacityStream<float>* values) {
			UIDrawConfig config;
			ArrayFloat(0, config, name, values);
		}

		void UIDrawer::ArrayFloat(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<float>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayFloatFunction);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayDouble(const char* name, CapacityStream<double>* values) {
			UIDrawConfig config;
			ArrayDouble(0, config, name, values);
		}

		void UIDrawer::ArrayDouble(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<double>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayDoubleFunction);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::ArrayInteger(const char* name, CapacityStream<Integer>* values) {
			UIDrawConfig config;
			ArrayInteger(0, config, name, values);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::ArrayInteger, const char*, CapacityStream<integer>*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::ArrayInteger(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<Integer>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayIntegerFunction<Integer>);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::ArrayInteger, size_t, const UIDrawConfig&, const char*, CapacityStream<integer>*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Instantiations - 2 components

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayFloat2(const char* name, CapacityStream<float2>* values) {
			UIDrawConfig config;
			ArrayFloat2(0, config, name, values);
		}

		void UIDrawer::ArrayFloat2(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<float2>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayFloat2Function);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayDouble2(const char* name, CapacityStream<double2>* values) {
			UIDrawConfig config;
			ArrayDouble2(0, config, name, values);
		}

		void UIDrawer::ArrayDouble2(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<double2>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayDouble2Function);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger, typename Integer2>
		void UIDrawer::ArrayInteger2(const char* name, CapacityStream<Integer2>* values) {
			UIDrawConfig config;
			ArrayInteger2<BaseInteger>(0, config, name, values);
		}

		template ECSENGINE_API void UIDrawer::ArrayInteger2<char>(const char* name, CapacityStream<char2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<int8_t>(const char* name, CapacityStream<char2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<uint8_t>(const char* name, CapacityStream<uchar2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<int16_t>(const char* name, CapacityStream<short2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<uint16_t>(const char* name, CapacityStream<ushort2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<int32_t>(const char* name, CapacityStream<int2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<uint32_t>(const char* name, CapacityStream<uint2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<int64_t>(const char* name, CapacityStream<long2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<uint64_t>(const char* name, CapacityStream<ulong2>* values);

		template<typename BaseInteger, typename Integer2>
		void UIDrawer::ArrayInteger2(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<Integer2>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayInteger2Function<BaseInteger>);
		}

		template ECSENGINE_API void UIDrawer::ArrayInteger2<char>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<char2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<int8_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<char2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<uint8_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<uchar2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<int16_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<short2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<uint16_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<ushort2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<int32_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<int2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<uint32_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<uint2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<int64_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<long2>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger2<uint64_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<ulong2>* values);

		// ------------------------------------------------------------------------------------------------------------------------------------

		// it will infer the extended type
		template<typename BaseInteger>
		void UIDrawer::ArrayInteger2Infer(const char* name, CapacityStream<void>* values) {
			UIDrawConfig config;
			ArrayInteger2Infer<BaseInteger>(0, config, name, values);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::ArrayInteger2Infer<integer>, const char*, CapacityStream<void>*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// it will infer the extended type
		template<typename BaseInteger>
		void UIDrawer::ArrayInteger2Infer(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<void>* values) {
			if constexpr (std::is_same_v<BaseInteger, char> || std::is_same_v<BaseInteger, int8_t>) {
				ArrayInteger2<BaseInteger>(configuration, config, name, (CapacityStream<char2>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint8_t>) {
				ArrayInteger2<BaseInteger>(configuration, config, name, (CapacityStream<uchar2>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, int16_t>) {
				ArrayInteger2<BaseInteger>(configuration, config, name, (CapacityStream<short2>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint16_t>) {
				ArrayInteger2<BaseInteger>(configuration, config, name, (CapacityStream<ushort2>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, int32_t>) {
				ArrayInteger2<BaseInteger>(configuration, config, name, (CapacityStream<int2>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint32_t>) {
				ArrayInteger2<BaseInteger>(configuration, config, name, (CapacityStream<uint2>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, int64_t>) {
				ArrayInteger2<BaseInteger>(configuration, config, name, (CapacityStream<long2>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint64_t>) {
				ArrayInteger2<BaseInteger>(configuration, config, name, (CapacityStream<ulong2>*)values);
			}
			else {
				ECS_ASSERT(false);
			}
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::ArrayInteger2Infer<integer>, size_t, const UIDrawConfig&, const char*, CapacityStream<void>*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Instantiations - 3 components

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayFloat3(const char* name, CapacityStream<float3>* values) {
			UIDrawConfig config;
			ArrayFloat3(0, config, name, values);
		}

		void UIDrawer::ArrayFloat3(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<float3>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayFloat3Function);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayDouble3(const char* name, CapacityStream<double3>* values) {
			UIDrawConfig config;
			ArrayDouble3(0, config, name, values);
		}

		void UIDrawer::ArrayDouble3(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<double3>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayDouble3Function);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger, typename Integer3>
		void UIDrawer::ArrayInteger3(const char* name, CapacityStream<Integer3>* values) {
			UIDrawConfig config;
			ArrayInteger3<BaseInteger>(0, config, name, values);
		}

		template ECSENGINE_API void UIDrawer::ArrayInteger3<char>(const char* name, CapacityStream<char3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<int8_t>(const char* name, CapacityStream<char3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<uint8_t>(const char* name, CapacityStream<uchar3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<int16_t>(const char* name, CapacityStream<short3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<uint16_t>(const char* name, CapacityStream<ushort3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<int32_t>(const char* name, CapacityStream<int3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<uint32_t>(const char* name, CapacityStream<uint3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<int64_t>(const char* name, CapacityStream<long3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<uint64_t>(const char* name, CapacityStream<ulong3>* values);

		template<typename BaseInteger, typename Integer3>
		void UIDrawer::ArrayInteger3(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<Integer3>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayInteger3Function<BaseInteger>);
		}

		template ECSENGINE_API void UIDrawer::ArrayInteger3<char>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<char3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<int8_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<char3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<uint8_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<uchar3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<int16_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<short3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<uint16_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<ushort3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<int32_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<int3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<uint32_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<uint3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<int64_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<long3>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger3<uint64_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<ulong3>* values);

		// ------------------------------------------------------------------------------------------------------------------------------------

		// it will infer the extended type
		template<typename BaseInteger>
		void UIDrawer::ArrayInteger3Infer(const char* name, CapacityStream<void>* values) {
			UIDrawConfig config;
			ArrayInteger3Infer<BaseInteger>(0, config, name, values);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::ArrayInteger3Infer<integer>, const char*, CapacityStream<void>*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// it will infer the extended type
		template<typename BaseInteger>
		void UIDrawer::ArrayInteger3Infer(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<void>* values) {
			if constexpr (std::is_same_v<BaseInteger, char> || std::is_same_v<BaseInteger, int8_t>) {
				ArrayInteger3<BaseInteger>(configuration, config, name, (CapacityStream<char3>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint8_t>) {
				ArrayInteger3<BaseInteger>(configuration, config, name, (CapacityStream<uchar3>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, int16_t>) {
				ArrayInteger3<BaseInteger>(configuration, config, name, (CapacityStream<short3>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint16_t>) {
				ArrayInteger3<BaseInteger>(configuration, config, name, (CapacityStream<ushort3>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, int32_t>) {
				ArrayInteger3<BaseInteger>(configuration, config, name, (CapacityStream<int3>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint32_t>) {
				ArrayInteger3<BaseInteger>(configuration, config, name, (CapacityStream<uint3>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, int64_t>) {
				ArrayInteger3<BaseInteger>(configuration, config, name, (CapacityStream<long3>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint64_t>) {
				ArrayInteger3<BaseInteger>(configuration, config, name, (CapacityStream<ulong3>*)values);
			}
			else {
				ECS_ASSERT(false);
			}
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::ArrayInteger3Infer<integer>, size_t, const UIDrawConfig&, const char*, CapacityStream<void>*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Instantiations - 4 components

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayFloat4(const char* name, CapacityStream<float4>* values) {
			UIDrawConfig config;
			ArrayFloat4(0, config, name, values);
		}

		void UIDrawer::ArrayFloat4(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<float4>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayFloat4Function);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayDouble4(const char* name, CapacityStream<double4>* values) {
			UIDrawConfig config;
			ArrayDouble4(0, config, name, values);
		}

		void UIDrawer::ArrayDouble4(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<double4>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayDouble4Function);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger, typename Integer4>
		void UIDrawer::ArrayInteger4(const char* name, CapacityStream<Integer4>* values) {
			UIDrawConfig config;
			ArrayInteger4<BaseInteger>(0, config, name, values);
		}

		template ECSENGINE_API void UIDrawer::ArrayInteger4<char>(const char* name, CapacityStream<char4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<int8_t>(const char* name, CapacityStream<char4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<uint8_t>(const char* name, CapacityStream<uchar4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<int16_t>(const char* name, CapacityStream<short4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<uint16_t>(const char* name, CapacityStream<ushort4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<int32_t>(const char* name, CapacityStream<int4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<uint32_t>(const char* name, CapacityStream<uint4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<int64_t>(const char* name, CapacityStream<long4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<uint64_t>(const char* name, CapacityStream<ulong4>* values);

		template<typename BaseInteger, typename Integer4>
		void UIDrawer::ArrayInteger4(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<Integer4>* values) {
			Array(configuration, config, name, values, nullptr, UIDrawerArrayInteger4Function<BaseInteger>);
		}

		template ECSENGINE_API void UIDrawer::ArrayInteger4<char>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<char4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<int8_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<char4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<uint8_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<uchar4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<int16_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<short4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<uint16_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<ushort4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<int32_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<int4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<uint32_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<uint4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<int64_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<long4>* values);
		template ECSENGINE_API void UIDrawer::ArrayInteger4<uint64_t>(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<ulong4>* values);

		// ------------------------------------------------------------------------------------------------------------------------------------

		// it will infer the extended type
		template<typename BaseInteger>
		void UIDrawer::ArrayInteger4Infer(const char* name, CapacityStream<void>* values) {
			UIDrawConfig config;
			ArrayInteger4Infer<BaseInteger>(0, config, name, values);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::ArrayInteger4Infer<integer>, const char*, CapacityStream<void>*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// it will infer the extended type
		template<typename BaseInteger>
		void UIDrawer::ArrayInteger4Infer(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<void>* values) {
			if constexpr (std::is_same_v<BaseInteger, char> || std::is_same_v<BaseInteger, int8_t>) {
				ArrayInteger4<BaseInteger>(configuration, config, name, (CapacityStream<char4>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint8_t>) {
				ArrayInteger4<BaseInteger>(configuration, config, name, (CapacityStream<uchar4>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, int16_t>) {
				ArrayInteger4<BaseInteger>(configuration, config, name, (CapacityStream<short4>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint16_t>) {
				ArrayInteger4<BaseInteger>(configuration, config, name, (CapacityStream<ushort4>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, int32_t>) {
				ArrayInteger4<BaseInteger>(configuration, config, name, (CapacityStream<int4>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint32_t>) {
				ArrayInteger4<BaseInteger>(configuration, config, name, (CapacityStream<uint4>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, int64_t>) {
				ArrayInteger4<BaseInteger>(configuration, config, name, (CapacityStream<long4>*)values);
			}
			else if constexpr (std::is_same_v<BaseInteger, uint64_t>) {
				ArrayInteger4<BaseInteger>(configuration, config, name, (CapacityStream<ulong4>*)values);
			}
			else {
				ECS_ASSERT(false);
			}
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::ArrayInteger4Infer<integer>, size_t, const UIDrawConfig&, const char*, CapacityStream<void>*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Color

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayColor(const char* name, CapacityStream<Color>* colors) {
			UIDrawConfig config;
			ArrayColor(0, config, name, colors);
		}

		void UIDrawer::ArrayColor(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<Color>* colors) {
			Array(configuration, config, name, colors, nullptr, UIDrawerArrayColorFunction);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Color Float

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayColorFloat(const char* name, CapacityStream<ColorFloat>* colors) {
			UIDrawConfig config;
			ArrayColorFloat(0, config, name, colors);
		}

		void UIDrawer::ArrayColorFloat(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<ColorFloat>* colors) {
			Array(configuration, config, name, colors, nullptr, UIDrawerArrayColorFloatFunction);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion


#pragma region Array Check Boxes

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayCheckBox(const char* name, CapacityStream<bool>* states) {
			UIDrawConfig config;
			ArrayCheckBox(0, config, name, states);
		}

		void UIDrawer::ArrayCheckBox(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<bool>* states) {
			Array(configuration, config, name, states, nullptr, UIDrawerArrayCheckBoxFunction);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Array Text Input

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ArrayTextInput(const char* name, CapacityStream<CapacityStream<char>>* texts) {
			UIDrawConfig config;
			ArrayTextInput(0, config, name, texts);
		}

		void UIDrawer::ArrayTextInput(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<CapacityStream<char>>* texts) {
			UIDrawerTextInput** inputs = (UIDrawerTextInput**)GetTempBuffer(sizeof(UIDrawerTextInput*) * texts->size);

			Array(configuration, config, name, texts, inputs, UIDrawerArrayTextInputFunction,
				[](UIDrawer& drawer, void* _elements, unsigned int element_count, unsigned int* new_order, void* additional_data) {
					Stream<CapacityStream<char>> elements(_elements, element_count);
					UIDrawerTextInput** inputs = (UIDrawerTextInput**)additional_data;

					size_t temp_marker = drawer.GetTempAllocatorMarker();
					CapacityStream<char>* copied_elements = (CapacityStream<char>*)drawer.GetTempBuffer(sizeof(CapacityStream<char>) * elements.size);
					// Copy the old contents to temp buffers
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

		void UIDrawer::ArrayComboBox(const char* name, CapacityStream<unsigned char>* flags, CapacityStream<Stream<const char*>> flag_labels) {
			UIDrawConfig config;
			ArrayComboBox(0, config, name, flags, flag_labels);
		}

		void UIDrawer::ArrayComboBox(size_t configuration, const UIDrawConfig& config, const char* name, CapacityStream<unsigned char>* flags, CapacityStream<Stream<const char*>> flag_labels) {
			Array(configuration, config, name, flags, &flag_labels, UIDrawerArrayComboBoxFunction);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma endregion

#pragma region Alpha Color Rectangle

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AlphaColorRectangle(Color color) {
			UIDrawConfig config;
			AlphaColorRectangle(0, config, color);
		}

		void UIDrawer::AlphaColorRectangle(size_t configuration, const UIDrawConfig& config, float2& position, float2& scale, Color color) {
			HandleFitSpaceRectangle(configuration, position, scale);

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				bool state = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					state = active_state->state;
				}

				Color empty_texture_color = ECS_COLOR_WHITE;
				empty_texture_color.alpha = 255 - color.alpha;
				if (!state) {
					color.alpha *= color_theme.alpha_inactive_item;
					empty_texture_color.alpha *= color_theme.alpha_inactive_item;
				}
				SpriteRectangle(configuration, position, scale, ECS_TOOLS_UI_TEXTURE_MASK, color);
				SpriteRectangle(configuration, position, scale, ECS_TOOLS_UI_TEXTURE_EMPTY_TEXTURE_TILES_BIG, empty_texture_color);

				HandleLateAndSystemDrawActionNullify(configuration, position, scale);
				HandleRectangleActions(configuration, config, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AlphaColorRectangle(size_t configuration, const UIDrawConfig& config, Color color) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				AlphaColorRectangle(configuration, config, position, scale, color);
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion 

		// Used to track which allocations have been made inside an initialization block
		void UIDrawer::BeginElement() {
			last_initialized_element_allocations.size = 0;
			last_initialized_element_table_resources.size = 0;
		}

#pragma region Button

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Button(
			const char* label,
			UIActionHandler handler
		) {
			UIDrawConfig config;
			Button(0, config, label, handler);
		}

		void UIDrawer::Button(
			size_t configuration,
			const UIDrawConfig& config,
			const char* text,
			UIActionHandler handler
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			bool is_active = true;

			if (!initializer) {
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = state->state;
				}

				configuration |= function::Select<size_t>(is_active, 0, UI_CONFIG_UNAVAILABLE_TEXT);

				if (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					TextLabel(configuration, config, text, position, scale);
				}
				else {
					TextLabelWithCull(configuration, config, text, position, scale);
					if (configuration & UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE) {
						position.y = AlignMiddle(position.y, current_row_y_scale, scale.y);
					}
					FinalizeRectangle(configuration, position, scale);
				}

				if (is_active) {
					if (~configuration & UI_CONFIG_BUTTON_HOVERABLE) {
						Color label_color = HandleColor(configuration, config);

						//AddDefaultHoverable(position, scale, label_color, 1.25f, handler.phase);
						if (handler.phase == UIDrawPhase::Normal) {
							UIDefaultHoverableData hoverable_data;
							hoverable_data.colors[0] = label_color;
							hoverable_data.percentages[0] = 1.25f;

							UIDrawPhase phase = UIDrawPhase::Normal;
							if (configuration & UI_CONFIG_LATE_DRAW) {
								phase = UIDrawPhase::Late;
							}
							else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
								phase = UIDrawPhase::System;
							}
							AddDefaultClickable(position, scale, { DefaultHoverableAction, &hoverable_data, sizeof(hoverable_data), phase }, handler);
						}
						else {
							UIDefaultTextHoverableData hoverable_data;
							hoverable_data.color = label_color;
							hoverable_data.text = text;

							UISpriteVertex* vertices = HandleTextSpriteBuffer(configuration);
							size_t* count = HandleTextSpriteCount(configuration);

							size_t text_vertex_count = strlen(text) * 6;
							hoverable_data.text_offset = { vertices[*count - text_vertex_count].position.x - position.x, -vertices[*count - text_vertex_count].position.y - position.y };
							Color text_color;
							float2 text_size;
							float text_character_spacing;
							HandleText(configuration, config, text_color, text_size, text_character_spacing);

							hoverable_data.character_spacing = text_character_spacing;
							hoverable_data.font_size = text_size;
							hoverable_data.text_color = text_color;

							UIDrawPhase hoverable_phase = UIDrawPhase::Normal;
							if (configuration & UI_CONFIG_LATE_DRAW) {
								hoverable_phase = UIDrawPhase::Late;
							}
							else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
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

				if (configuration & UI_CONFIG_TOOL_TIP) {
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
				TextLabel(configuration, config, text);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::StateButton(const char* name, bool* state) {
			UIDrawConfig config;
			StateButton(0, config, name, state);
		}

		void UIDrawer::StateButton(size_t configuration, const UIDrawConfig& config, const char* name, bool* state) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				bool is_active = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				if (is_active) {
					TextLabel(configuration | UI_CONFIG_DO_NOT_CACHE, config, name, position, scale);
					Color color = HandleColor(configuration, config);
					if (*state) {
						color = LightenColorClamp(color, 1.4f);
						SolidColorRectangle(configuration, position, scale, color);
					}

					AddDefaultClickableHoverable(position, scale, { BoolClickable, state, 0 }, color);
				}
				else {
					TextLabel(configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_UNAVAILABLE_TEXT, config, name, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteStateButton(
			const wchar_t* texture,
			bool* state,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv,
			float2 expand_factor
		) {
			UIDrawConfig config;
			SpriteStateButton(0, config, texture, state, color, top_left_uv, bottom_right_uv, expand_factor);
		}

		void UIDrawer::SpriteStateButton(
			size_t configuration,
			const UIDrawConfig& config,
			const wchar_t* texture,
			bool* state,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv,
			float2 expand_factor
		)
		{
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				bool is_active = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				if (ValidatePosition(configuration, position, scale)) {
					Color color = HandleColor(configuration, config);

					if (*state) {
						color = LightenColorClamp(color, 1.4f);
					}
					SolidColorRectangle(configuration, position, scale, color);
					float2 sprite_scale;
					float2 sprite_position = ExpandRectangle(position, scale, expand_factor, sprite_scale);
					if (is_active) {
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);
						AddDefaultClickableHoverable(position, scale, { BoolClickable, state, 0 }, color);
					}
					else {
						color.alpha *= color_theme.alpha_inactive_item;
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// Stateless, label is done with UI_CONFIG_DO_NOT_CACHE; if drawing with a sprite
		// name can be made nullptr
		void UIDrawer::MenuButton(const char* name, UIWindowDescriptor& window_descriptor, size_t border_flags) {
			UIDrawConfig config;
			MenuButton(0, config, name, window_descriptor, border_flags);
		}

		// Stateless, label is done with UI_CONFIG_DO_NOT_CACHE; if drawing with a sprite
		// name can be made nullptr
		void UIDrawer::MenuButton(size_t configuration, const UIDrawConfig& config, const char* name, UIWindowDescriptor& window_descriptor, size_t border_flags) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				bool is_active = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				}

				if (~configuration & UI_CONFIG_MENU_BUTTON_SPRITE) {
					configuration |= function::Select<size_t>(is_active, 0, UI_CONFIG_UNAVAILABLE_TEXT);
					TextLabel(configuration | UI_CONFIG_DO_NOT_CACHE, config, name, position, scale);
				}
				else {
					const UIConfigMenuButtonSprite* sprite_definition = (const UIConfigMenuButtonSprite*)config.GetParameter(UI_CONFIG_MENU_BUTTON_SPRITE);
					Color sprite_color = sprite_definition->color;
					sprite_color.alpha = function::Select<unsigned char>(is_active, sprite_color.alpha, sprite_color.alpha * color_theme.alpha_inactive_item);
					SpriteRectangle(
						configuration,
						position,
						scale,
						sprite_definition->texture,
						sprite_color,
						sprite_definition->top_left_uv,
						sprite_definition->bottom_right_uv
					);
					FinalizeRectangle(configuration, position, scale);
				}

				if (is_active) {
					UIDrawerMenuButtonData data;
					data.descriptor = window_descriptor;
					data.descriptor.initial_position_x = position.x;
					data.descriptor.initial_position_y = position.y + scale.y + element_descriptor.menu_button_padding;
					data.border_flags = border_flags;
					data.is_opened_when_pressed = false;
					Color color = HandleColor(configuration, config);

					AddClickable(position, scale, { MenuButtonAction, &data, sizeof(data), UIDrawPhase::System });
					UIDrawPhase hovered_phase = UIDrawPhase::Normal;
					if (configuration & UI_CONFIG_LATE_DRAW) {
						hovered_phase = UIDrawPhase::Late;
					}
					else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ChangeThemeColor(Color new_theme_color) {
			color_theme.SetNewTheme(new_theme_color);
		}

#pragma region Checkbox

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::CheckBox(const char* name, bool* value_to_change) {
			UIDrawConfig config;
			CheckBox(0, config, name, value_to_change);
		}

		void UIDrawer::CheckBox(size_t configuration, const UIDrawConfig& config, const char* name, bool* value_to_change) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				scale = GetSquareScale(scale.y);

				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerTextElement* element = (UIDrawerTextElement*)GetResource(name);

					CheckBoxDrawer(configuration, config, element, value_to_change, position, scale);
				}
				else {
					CheckBoxDrawer(configuration, config, name, value_to_change, position, scale);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					CheckBoxInitializer(configuration, config, name);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion 

#pragma region Combo box

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ComboBox(const char* name, Stream<const char*> labels, unsigned int label_display_count, unsigned char* active_label) {
			UIDrawConfig config;
			ComboBox(0, config, name, labels, label_display_count, active_label);
		}

		void UIDrawer::ComboBox(size_t configuration, UIDrawConfig& config, const char* name, Stream<const char*> labels, unsigned int label_display_count, unsigned char* active_label) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerComboBox* data = (UIDrawerComboBox*)GetResource(name);

					data->active_label = active_label;
					float min_value = data->labels[data->biggest_label_x_index].scale.x * data->labels[data->biggest_label_x_index].GetInverseZoomX() * zoom_ptr->x
						+ layout.element_indentation * 3.0f + element_descriptor.label_horizontal_padd * 2.0f;
					min_value += data->prefix_x_scale * zoom_ptr->x;
					scale.x = function::Select(min_value > scale.x, min_value, scale.x);

					ComboBoxDrawer(configuration | UI_CONFIG_DO_NOT_VALIDATE_POSITION, config, data, active_label, position, scale);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeComboBox initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_4(initialize_data, name, labels, label_display_count, active_label);
						InitializeDrawerElement(*this, &initialize_data, name, InitializeComboBoxElement, DynamicConfiguration(configuration));
					}
					ComboBox(DynamicConfiguration(configuration), config, name, labels, label_display_count, active_label);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					ComboBoxInitializer(configuration, config, name, labels, label_display_count, active_label);
				}
			}

		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ComboBoxDropDownDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerComboBox* data) {
			float2 position = region_position;
			float2 scale = { region_scale.x, data->label_y_scale };

			size_t text_label_configuration = NullifyConfiguration(configuration, UI_CONFIG_GET_TRANSFORM) | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
				| UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_LABEL_TRANSPARENT;

			Color color = HandleColor(configuration, config);
			color = DarkenColor(color, 0.8f);
			Color normal_color = color;
			Color active_color = ToneColor(color, 1.3f);

			for (size_t index = 0; index < data->labels.size; index++) {
				if (ValidatePosition(0, position, scale)) {
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
					SolidColorRectangle(text_label_configuration, rectangle_position, rectangle_scale, color);
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
					HandleTextLabelAlignment(
						UI_CONFIG_TEXT_ALIGNMENT,
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
					if (index == data->labels.size - 1) {
						rectangle_scale.y -= system->m_descriptors.dockspaces.border_size;
					}
					AddDefaultClickable(rectangle_position, rectangle_scale, hoverable_handler, { ComboBoxLabelClickable, &click_data, sizeof(click_data), UIDrawPhase::System });
					TextLabelDrawer(text_label_configuration, config, &data->labels[index], position, scale);
				}
				position.y += scale.y;
			}

			if (position.y < region_position.y + region_scale.y) {
				SolidColorRectangle(text_label_configuration, { position.x + region_render_offset.x, position.y }, { 2.0f, region_position.y + region_scale.y - position.y }, color);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Collapsing Header

		// ------------------------------------------------------------------------------------------------------------------------------------

		/*template<typename Function>
		bool CollapsingHeader(const char* name, Function&& function) {
			UIDrawConfig config;
			return CollapsingHeader(0, config, name, function);
		}

		template<typename Function>
		bool CollapsingHeader(size_t configuration, UIDrawConfig& config, const char* name, Function&& function) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

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
		}*/

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Color Input

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ColorInput(const char* name, Color* color, Color default_color) {
			UIDrawConfig config;
			ColorInput(0, config, name, color, default_color);
		}

		void UIDrawer::ColorInput(size_t configuration, UIDrawConfig& config, const char* name, Color* color, Color default_color) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerColorInput* data = (UIDrawerColorInput*)GetResource(name);

					ColorInputDrawer(configuration, config, data, position, scale, color);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeColorInput initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_3(initialize_data, color, name, default_color);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							name,
							InitializeColorInputElement,
							DynamicConfiguration(configuration)
						);
					}
					ColorInput(DynamicConfiguration(configuration), config, name, color, default_color);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					ColorInputInitializer(configuration, config, name, color, default_color, position, scale);
				}
			}

		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Color Float Input

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ColorFloatInput(const char* name, ColorFloat* color, ColorFloat default_color) {
			UIDrawConfig config;
			ColorFloatInput(0, config, name, color, default_color);
		}

		void UIDrawer::ColorFloatInput(size_t configuration, UIDrawConfig& config, const char* name, ColorFloat* color, ColorFloat default_color) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					ColorFloatInputDrawer(configuration, config, name, color, position, scale);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeColorFloatInput initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_3(initialize_data, color, name, default_color);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							name,
							InitializeColorFloatInputElement,
							DynamicConfiguration(configuration)
						);
					}
					ColorFloatInput(DynamicConfiguration(configuration), config, name, color, default_color);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					ColorFloatInputInitializer(configuration, config, name, color, default_color, position, scale);
				}
			}

		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ColorFloatInputIntensityInputName(char* intensity_input_name) {
			size_t base_intensity_name = strlen("Intensity");
			memcpy(intensity_input_name, "Intensity", base_intensity_name * sizeof(char));
			intensity_input_name[base_intensity_name] = '\0';
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ColorFloatInputDrawer(size_t configuration, UIDrawConfig& config, const char* name, ColorFloat* color, float2 position, float2 scale) {
			const char* identifier = HandleResourceIdentifier(name);

			// The resource must be taken from the table with manual parsing
			char resource_name[512];
			size_t name_size = strlen(identifier);
			memcpy(resource_name, identifier, sizeof(char) * name_size);
			size_t resource_name_size = strlen(" resource");
			memcpy(resource_name + name_size, " resource", sizeof(char) * resource_name_size);
			name_size += resource_name_size;
			resource_name[name_size] = '\0';
			UIDrawerColorFloatInput* data = (UIDrawerColorFloatInput*)system->FindWindowResource(window_index, resource_name, name_size);

			data->color_float = color;

			size_t COLOR_INPUT_DRAWER_CONFIGURATION = configuration | UI_CONFIG_COLOR_INPUT_CALLBACK | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW | UI_CONFIG_MAKE_SQUARE;

			// Draw the color input
			COLOR_INPUT_DRAWER_CONFIGURATION |= function::Select<size_t>(configuration & UI_CONFIG_COLOR_FLOAT_DEFAULT_VALUE, UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE, 0);
			ColorInputDrawer(
				COLOR_INPUT_DRAWER_CONFIGURATION,
				config,
				data->color_input,
				position,
				scale,
				&data->base_color
			);

			// Draw the intensity
			char intensity_input_name[256];
			ColorFloatInputIntensityInputName(intensity_input_name);

			size_t FLOAT_INPUT_CONFIGURATION = configuration | UI_CONFIG_TEXT_INPUT_CALLBACK;
			FLOAT_INPUT_CONFIGURATION |= function::Select<size_t>(configuration & UI_CONFIG_COLOR_FLOAT_DEFAULT_VALUE, UI_CONFIG_NUMBER_INPUT_DEFAULT, 0);
			FloatInputDrawer(FLOAT_INPUT_CONFIGURATION, config, intensity_input_name, &data->intensity, 0.0f, 10000.0f, { current_x - region_render_offset.x, position.y }, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerColorFloatInput* UIDrawer::ColorFloatInputInitializer(size_t configuration, UIDrawConfig& config, const char* name, ColorFloat* color, ColorFloat default_color, float2 position, float2 scale) {
			UIDrawerColorFloatInput* input = nullptr;

			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}

			const char* identifier = HandleResourceIdentifier(name);

			// Create a temporary name for resource, in order to avoid poluting the color input's
			// name - if there is any
			char color_input_name[256];
			color_input_name[0] = '\0';
			strcpy(color_input_name, identifier);
			strcat(color_input_name, " resource");
			ECS_ASSERT(strlen(color_input_name) < 256);
			input = GetMainAllocatorBufferAndStoreAsResource<UIDrawerColorFloatInput>(color_input_name);

			input->color_float = color;
			input->base_color = HDRColorToSDR(*color, &input->intensity);

			float default_sdr_intensity;
			Color default_sdr_color = HDRColorToSDR(default_color, &default_sdr_intensity);

			// The intensity will be controlled by number input - the reference must be made through the name
			char intensity_input_name[256];
			ColorFloatInputIntensityInputName(intensity_input_name);
			// Add the callback
			UIConfigTextInputCallback callback;
			callback.handler.action = ColorFloatInputIntensityCallback;
			callback.handler.data = input;
			callback.handler.data_size = 0;
			callback.handler.phase = UIDrawPhase::Normal;
			config.AddFlag(callback);

			size_t FLOAT_INPUT_CONFIGURATION = configuration | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN | UI_CONFIG_TEXT_INPUT_CALLBACK;
			FLOAT_INPUT_CONFIGURATION |= function::Select<size_t>(configuration & UI_CONFIG_COLOR_FLOAT_DEFAULT_VALUE, UI_CONFIG_NUMBER_INPUT_DEFAULT, 0);
			// Add the number input default
			FloatInputInitializer(
				FLOAT_INPUT_CONFIGURATION,
				config,
				intensity_input_name,
				&input->intensity,
				default_sdr_intensity,
				0.0f,
				10000.0f,
				position,
				scale
			);
			config.flag_count--;

			// The callback must be intercepted
			if (configuration & UI_CONFIG_COLOR_INPUT_CALLBACK) {
				// Make a coallesced allocation for the callback data
				UIConfigColorInputCallback* callback = (UIConfigColorInputCallback*)config.GetParameter(UI_CONFIG_COLOR_INPUT_CALLBACK);
				UIDrawerColorFloatInputCallbackData* callback_data = (UIDrawerColorFloatInputCallbackData*)GetMainAllocatorBuffer(
					sizeof(UIDrawerColorFloatInputCallbackData) + callback->callback.data_size
				);
				callback_data->input = input;
				if (callback->callback.data_size > 0) {
					void* callback_data_user = function::OffsetPointer(callback_data, sizeof(UIDrawerColorFloatInputCallbackData));
					memcpy(callback_data_user, callback->callback.data, callback->callback.data_size);
					callback_data->callback_data = callback_data_user;
				}
				else {
					callback_data->callback_data = callback->callback.data;
				}
				callback->callback.action = ColorFloatInputCallback;
				callback->callback.data = callback_data;
				callback->callback.data_size = 0;
			}
			else {
				UIDrawerColorFloatInputCallbackData callback_data;
				callback_data.callback = nullptr;
				callback_data.callback_data = nullptr;
				callback_data.input = input;
				UIConfigColorInputCallback callback = { ColorFloatInputCallback, &callback_data, sizeof(callback_data) };

				// Cringe MSVC bug - it does not set the callback data correctly
				memset(&callback_data.callback, 0, 16);
				memcpy(&callback_data.input, &input, sizeof(input));
				config.AddFlag(callback);
			}

			size_t COLOR_INPUT_CONFIGURATION = configuration | UI_CONFIG_COLOR_INPUT_CALLBACK | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			COLOR_INPUT_CONFIGURATION |= function::Select<size_t>(configuration & UI_CONFIG_COLOR_FLOAT_DEFAULT_VALUE, UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE, 0);
			// Add the color input default flag
			input->color_input = ColorInputInitializer(
				COLOR_INPUT_CONFIGURATION,
				config,
				name,
				&input->base_color,
				default_sdr_color,
				position,
				scale
			);
			return input;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region CrossLine

		// ------------------------------------------------------------------------------------------------------------------------------------

		// Setting the y size for horizontal or x size for vertical as 0.0f means get default 1 pixel width
		void UIDrawer::CrossLine() {
			UIDrawConfig config;
			CrossLine(0, config);
		}

		void UIDrawer::CrossLine(size_t configuration, const UIDrawConfig& config) {
			if (!initializer) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if (~configuration & UI_CONFIG_CROSS_LINE_DO_NOT_INFER) {
					if (configuration & UI_CONFIG_VERTICAL) {
						scale.y = region_position.y + region_scale.y - position.y - layout.next_row_y_offset;
						scale.x = system->NormalizeHorizontalToWindowDimensions(system->m_descriptors.dockspaces.border_size * 1.0f);
					}
					else {
						scale.x = region_position.x + region_scale.x - position.x - (region_fit_space_horizontal_offset - region_position.x);
						scale.y = system->m_descriptors.dockspaces.border_size * 1.0f;
					}
				}
				else {
					if (configuration & UI_CONFIG_VERTICAL) {
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

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					if (~configuration & UI_CONFIG_COLOR) {
						SolidColorRectangle(configuration, position, scale, color_theme.borders);
					}
					else {
						Color color = HandleColor(configuration, config);
						SolidColorRectangle(configuration, position, scale, color);
					}
				}

				if (~configuration & UI_CONFIG_DO_NOT_ADVANCE) {
					if (configuration & UI_CONFIG_VERTICAL) {
						current_x += scale.x + layout.element_indentation;
					}
					else {
						current_y += scale.y + layout.next_row_y_offset;
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DisablePaddingForRenderSliders() {
			region_limit.x += system->m_descriptors.misc.render_slider_vertical_size;
			region_limit.y += system->m_descriptors.misc.render_slider_horizontal_size;
			no_padding_for_render_sliders = true;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DisablePaddingForRenderRegion() {
			no_padding_render_region = true;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DisableZoom() const {
			if (!initializer) {
				UIDefaultWindowHandler* data = system->GetDefaultWindowHandlerData(window_index);
				data->allow_zoom = false;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Filter Menu

		// ------------------------------------------------------------------------------------------------------------------------------------

		// States should be a stack pointer with bool* to the members that need to be changed
		void UIDrawer::FilterMenu(const char* name, Stream<const char*> label_names, bool** states) {
			UIDrawConfig config;
			FilterMenu(0, config, name, label_names, states);
		}

		// States should be a stack pointer with bool* to the members that need to be changed
		void UIDrawer::FilterMenu(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> label_names, bool** states) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerFilterMenuData* data = (UIDrawerFilterMenuData*)GetResource(name);

					FilterMenuDrawer(configuration, config, name, data);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeFilterMenu initialize_data;
						initialize_data.config = (UIDrawConfig*)&config;
						ECS_FORWARD_STRUCT_MEMBERS_3(initialize_data, name, label_names, states);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							name,
							InitializeFilterMenuElement,
							DynamicConfiguration(configuration)
						);
					}
					FilterMenu(DynamicConfiguration(configuration), config, name, label_names, states);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					FilterMenuInitializer(configuration, config, name, label_names, states);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// States should be a stack pointer to a stable bool array
		void UIDrawer::FilterMenu(const char* name, Stream<const char*> label_names, bool* states) {
			UIDrawConfig config;
			FilterMenu(0, config, name, label_names, states);
		}

		// States should be a stack pointer to a stable bool array
		void UIDrawer::FilterMenu(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> label_names, bool* states) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerFilterMenuSinglePointerData* data = (UIDrawerFilterMenuSinglePointerData*)GetResource(name);

					FilterMenuDrawer(configuration, config, name, data);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeFilterMenuSinglePointer initialize_data;
						initialize_data.config = (UIDrawConfig*)&config;
						ECS_FORWARD_STRUCT_MEMBERS_3(initialize_data, label_names, name, states);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							name,
							InitializeFilterMenuSinglePointerElement,
							DynamicConfiguration(configuration)
						);
					}
					FilterMenu(DynamicConfiguration(configuration), config, name, label_names, states);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					FilterMenuInitializer(configuration, config, name, label_names, states);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FilterMenuDrawer(size_t configuration, const UIDrawConfig& config, const char* name, UIDrawerFilterMenuData* data) {
			UIWindowDescriptor window_descriptor;
			window_descriptor.initial_size_x = 10000.0f;
			window_descriptor.initial_size_y = 10000.0f;
			window_descriptor.draw = FilterMenuDraw;

			window_descriptor.window_name = data->window_name;

			window_descriptor.window_data = data;
			window_descriptor.window_data_size = 0;

			MenuButton(configuration, config, name, window_descriptor);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerFilterMenuData* UIDrawer::FilterMenuInitializer(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> label_names, bool** states) {
			UIDrawerFilterMenuData* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				size_t total_memory = sizeof(UIDrawerFilterMenuData);
				total_memory += sizeof(const char*) * label_names.size;
				total_memory += sizeof(bool*) * label_names.size;
				size_t identifier_size = strlen(identifier) + 1;
				size_t window_name_size = identifier_size + strlen("Filter Window");
				total_memory += sizeof(char) * window_name_size;
				if (configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
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

				if (~configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
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

				data->draw_all = (configuration & UI_CONFIG_FILTER_MENU_ALL) == 0;

				if (configuration & UI_CONFIG_FILTER_MENU_NOTIFY_ON_CHANGE) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FilterMenuDrawer(size_t configuration, const UIDrawConfig& config, const char* name, UIDrawerFilterMenuSinglePointerData* data) {
			UIWindowDescriptor window_descriptor;
			window_descriptor.initial_size_x = 10000.0f;
			window_descriptor.initial_size_y = 10000.0f;
			window_descriptor.draw = FilterMenuSinglePointerDraw;

			window_descriptor.window_name = data->window_name;

			window_descriptor.window_data = data;
			window_descriptor.window_data_size = 0;

			MenuButton(configuration, config, name, window_descriptor);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerFilterMenuSinglePointerData* UIDrawer::FilterMenuInitializer(
			size_t configuration,
			const UIDrawConfig& config,
			const char* name,
			Stream<const char*> label_names,
			bool* states
		) {
			UIDrawerFilterMenuSinglePointerData* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				size_t total_memory = sizeof(UIDrawerFilterMenuSinglePointerData);
				total_memory += sizeof(const char*) * label_names.size;
				size_t identifier_size = strlen(identifier) + 1;
				size_t window_name_size = identifier_size + strlen("Filter Window");
				total_memory += sizeof(char) * window_name_size;
				if (configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
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

				if (~configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
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

				data->draw_all = (configuration & UI_CONFIG_FILTER_MENU_ALL) == 0;

				if (configuration & UI_CONFIG_FILTER_MENU_NOTIFY_ON_CHANGE) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FitTextToScale(
			size_t configuration,
			UIDrawConfig& config,
			float2& font_size,
			float& character_spacing,
			Color& color,
			float2 scale,
			float padding,
			UIConfigTextParameters& previous_parameters
		) {
			HandleText(configuration, config, color, font_size, character_spacing);

			float old_scale = font_size.y;
			font_size.y = system->GetTextSpriteSizeToScale(scale.y - padding * 2);
			font_size.x = font_size.y * ECS_TOOLS_UI_FONT_X_FACTOR;
			float factor = font_size.y / old_scale;
			character_spacing *= factor;

			UIConfigTextParameters parameters;
			parameters.character_spacing = character_spacing;
			parameters.color = color;
			parameters.size = font_size;
			SetConfigParameter(configuration, UI_CONFIG_TEXT_PARAMETERS, config, parameters, previous_parameters);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FinishFitTextToScale(size_t configuration, UIDrawConfig& config, const UIConfigTextParameters& previous_parameters) {
			RemoveConfigParameter(configuration, UI_CONFIG_TEXT_PARAMETERS, config, previous_parameters);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Gradients

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Gradient(size_t configuration, const UIDrawConfig& config, const Color* colors, size_t horizontal_count, size_t vertical_count) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}
				if (ValidatePosition(configuration, position, scale)) {
					float x_scale = scale.x / horizontal_count;
					float y_scale = scale.y / vertical_count;

					ColorFloat top_left(colors[0]), top_right(colors[1]), bottom_left(colors[2]), bottom_right(colors[3]);

					const wchar_t* texture;
					float2 top_left_uv;
					float2 uv_delta;
					float2 current_top_left_uv;
					float2 current_bottom_right_uv;

					if (configuration & UI_CONFIG_SPRITE_GRADIENT) {
						const UIConfigSpriteGradient* sprite_info = (const UIConfigSpriteGradient*)config.GetParameter(UI_CONFIG_SPRITE_GRADIENT);
						texture = sprite_info->texture;
						top_left_uv = sprite_info->top_left_uv;
						uv_delta = { sprite_info->bottom_right_uv.x - top_left_uv.x, sprite_info->bottom_right_uv.y - top_left_uv.y };
						uv_delta.x /= horizontal_count;
						uv_delta.y /= vertical_count;

						SetSpriteClusterTexture(configuration, texture, horizontal_count * vertical_count);
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

						if (configuration & UI_CONFIG_SPRITE_GRADIENT) {
							current_top_left_uv = { top_left_uv.x, top_left_uv.y + uv_delta.y * row };
							current_bottom_right_uv = { top_left_uv.x + uv_delta.x, current_top_left_uv.y + uv_delta.y };
							VertexColorSpriteClusterRectangle(configuration | UI_CONFIG_DO_NOT_ADVANCE, rectangle_position, rectangle_scale, texture, current_colors, current_top_left_uv, current_bottom_right_uv);
							current_top_left_uv.x += uv_delta.x;
							current_bottom_right_uv.x += uv_delta.x;
						}
						else {
							VertexColorRectangle(configuration | UI_CONFIG_DO_NOT_ADVANCE, rectangle_position, rectangle_scale, current_colors);
						}

						rectangle_position.x += x_scale;
						for (size_t column = 1; column < horizontal_count; column++) {
							current_colors[0] = current_colors[1];
							current_colors[2] = current_colors[3];
							current_colors[1] = function::PlanarLerp(top_left, top_right, bottom_left, bottom_right, (column + 1) * inverse_horizontal_count_float, row * inverse_vertical_count_float);
							current_colors[3] = function::PlanarLerp(top_left, top_right, bottom_left, bottom_right, (column + 1) * inverse_horizontal_count_float, (row + 1) * inverse_vertical_count_float);

							if (configuration & UI_CONFIG_SPRITE_GRADIENT) {
								VertexColorSpriteClusterRectangle(
									configuration | UI_CONFIG_DO_NOT_ADVANCE,
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
								VertexColorRectangle(
									configuration | UI_CONFIG_DO_NOT_ADVANCE,
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

					HandleBorder(configuration, config, position, scale);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
					HandleRectangleActions(configuration, config, position, scale);
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Graphs

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Graph(Stream<float2> samples, const char* name, unsigned int x_axis_precision, unsigned int y_axis_precision) {
			UIDrawConfig config;
			Graph(0, config, samples, name, x_axis_precision, y_axis_precision);
		}

		void UIDrawer::Graph(
			size_t configuration,
			const UIDrawConfig& config,
			Stream<float2> unfiltered_samples,
			float filter_delta,
			const char* name,
			unsigned int x_axis_precision,
			unsigned int y_axis_precision
		) {
			if (!initializer) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position)) {
					float2* samples = (float2*)GetTempBuffer(sizeof(float2) * 128);
					Stream<float2> filtered_samples = Stream<float2>(samples, 0);
					float first_value = 0.0f;
					float second_value = 0.0f;
					if (configuration & UI_CONFIG_GRAPH_FILTER_SAMPLES_X) {
						first_value = unfiltered_samples[0].x;
					}
					else if (configuration & UI_CONFIG_GRAPH_FILTER_SAMPLES_Y) {
						first_value = unfiltered_samples[0].y;
					}
					filtered_samples.Add(unfiltered_samples[0]);

					for (size_t index = 1; index < unfiltered_samples.size; index++) {
						if (configuration & UI_CONFIG_GRAPH_FILTER_SAMPLES_X) {
							second_value = unfiltered_samples[index].x;
						}
						else if (configuration & UI_CONFIG_GRAPH_FILTER_SAMPLES_Y) {
							second_value = unfiltered_samples[index].y;
						}
						if (fabs(second_value - first_value) > filter_delta) {
							filtered_samples.Add(unfiltered_samples[index]);
							if (configuration & UI_CONFIG_GRAPH_FILTER_SAMPLES_X) {
								first_value = unfiltered_samples[index].x;
							}
							else {
								first_value = unfiltered_samples[index].y;
							}
						}
					}
					GraphDrawer(configuration, config, name, filtered_samples, position, scale, x_axis_precision, y_axis_precision);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Graph(size_t configuration, const UIDrawConfig& config, Stream<float2> samples, const char* name, unsigned int x_axis_precision, unsigned int y_axis_precision) {
			if (!initializer) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				GraphDrawer(configuration, config, name, samples, position, scale, x_axis_precision, y_axis_precision);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::MultiGraph(
			size_t configuration,
			const UIDrawConfig& config, 
			Stream<float> samples, 
			size_t sample_count, 
			const Color* colors, 
			const char* name, 
			unsigned int x_axis_precision,
			unsigned int y_axis_precision
		) {
			if (!initializer) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				MultiGraphDrawer(configuration, config, name, samples, sample_count, colors, position, scale, x_axis_precision, y_axis_precision);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		const char* UIDrawer::GetElementName(unsigned int index) const {
			return system->GetDrawElementName(window_index, index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GetDefaultBorderThickness() const {
			return system->m_descriptors.dockspaces.border_size;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetElementDefaultScale() const {
			return { layout.default_element_x, layout.default_element_y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// Accounts for zoom
		float2 UIDrawer::GetStaticElementDefaultScale() const {
			return { layout.default_element_x / zoom_ptr->x, layout.default_element_y / zoom_ptr->y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetCurrentPosition() const {
			return { current_x - region_render_offset.x, current_y - region_render_offset.y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetCurrentPositionNonOffset() const {
			return { current_x, current_y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetCurrentPositionStatic() const {
			return { current_x + region_render_offset.x, current_y + region_render_offset.y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetWindowSizeFactors(WindowSizeTransformType type, float2 scale) const {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetWindowSizeScaleElement(WindowSizeTransformType type, float2 scale_factors) const {
			float2 scale;
			switch (type) {
			case WindowSizeTransformType::Horizontal:
				scale = {
					scale_factors.x * (region_limit.x - region_fit_space_horizontal_offset),
					layout.default_element_y * scale_factors.y
				};
				scale.x = function::Select(scale.x == 0.0f, region_limit.x - current_x, scale.x);
				break;
			case WindowSizeTransformType::Vertical:
				scale = {
					scale_factors.x * layout.default_element_x,
					scale_factors.y * (region_limit.y - region_fit_space_vertical_offset)
				};
				scale.y = function::Select(scale.y == 0.0f, region_limit.y - current_y, scale.y);
				break;
			case WindowSizeTransformType::Both:
				scale = { scale_factors.x * (region_limit.x - region_fit_space_horizontal_offset), scale_factors.y * (region_limit.y - region_fit_space_vertical_offset) };
				scale.x = function::Select(scale.x == 0.0f, region_limit.x - current_x, scale.x);
				scale.y = function::Select(scale.y == 0.0f, region_limit.y - current_y, scale.y);
				break;
			}
			return scale;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetWindowSizeScaleUntilBorder() const {
			return { (region_limit.x - current_x) / (region_limit.x - region_fit_space_horizontal_offset), 1.0f };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void* UIDrawer::GetTempBuffer(size_t size, size_t alignment) {
			return system->m_resources.thread_resources[thread_id].temp_allocator.Allocate(size, alignment);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// Can be used to release some temp memory - cannot be used when handlers are used
		void UIDrawer::ReturnTempAllocator(size_t marker) {
			system->m_resources.thread_resources[thread_id].temp_allocator.ReturnToMarker(marker);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t UIDrawer::GetTempAllocatorMarker() {
			return system->m_resources.thread_resources[thread_id].temp_allocator.m_top;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UISystem* UIDrawer::GetSystem() {
			return system;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetSquareScale(float value) const {
			return { system->NormalizeHorizontalToWindowDimensions(value), value };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// It will use HandleResourceIdentifier to construct the string
		void* UIDrawer::GetResource(const char* string) {
			const char* string_identifier = HandleResourceIdentifier(string);
			return system->FindWindowResource(window_index, string_identifier, strlen(string_identifier));
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// It will forward directly to the UI system; No HandleResourceIdentifier used
		void* UIDrawer::FindWindowResource(const char* string) {
			return system->FindWindowResource(window_index, string, strlen(string));
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetRegionPosition() const {
			return region_position;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetRegionScale() const {
			return region_scale;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetRegionRenderOffset() const
		{
			return region_render_offset;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// returns the render bound difference
		float2 UIDrawer::GetRenderSpan() const {
			return { max_render_bounds.x - min_render_bounds.x, max_render_bounds.y - min_render_bounds.y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// returns the viewport visible zone
		float2 UIDrawer::GetRenderZone() const {
			float horizontal_region_difference;
			horizontal_region_difference = region_scale.x - 2 * layout.next_row_padding - system->m_descriptors.misc.render_slider_vertical_size + 0.001f;
			horizontal_region_difference += function::Select(no_padding_for_render_sliders, system->m_descriptors.misc.render_slider_vertical_size, 0.0f);
			horizontal_region_difference += function::Select(no_padding_render_region, 2 * layout.next_row_padding, 0.0f);

			float vertical_region_difference;
			vertical_region_difference = region_scale.y - 2 * layout.next_row_y_offset - system->m_descriptors.misc.render_slider_horizontal_size + 0.001f;
			vertical_region_difference += function::Select(no_padding_for_render_sliders, system->m_descriptors.misc.render_slider_horizontal_size, 0.0f);
			vertical_region_difference += function::Select(no_padding_render_region, 2 * layout.next_row_y_offset, 0.0f);

			if (dockspace->borders[border_index].draw_elements) {
				vertical_region_difference -= system->m_descriptors.misc.title_y_scale;
			}

			return { horizontal_region_difference, vertical_region_difference };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void** UIDrawer::GetBuffers() {
			return buffers;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerBufferState UIDrawer::GetBufferState(size_t configuration) {
			UIDrawerBufferState state;

			if (!initializer) {
				state.solid_color_count = *HandleSolidColorCount(configuration);
				state.sprite_count = *HandleSpriteCount(configuration);
				state.text_sprite_count = *HandleTextSpriteCount(configuration);
				state.sprite_cluster_count = *HandleSpriteClusterCount(configuration);
			}
			else {
				state.solid_color_count = 0;
				state.sprite_count = 0;
				state.text_sprite_count = 0;
				state.text_sprite_count = 0;
				state.sprite_cluster_count = 0;
			}

			return state;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerHandlerState UIDrawer::GetHandlerState() {
			UIDrawerHandlerState state;

			if (!initializer) {
				state.hoverable_count = dockspace->borders[border_index].hoverable_handler.position_x.size;
				state.clickable_count = dockspace->borders[border_index].clickable_handler.position_x.size;
				state.general_count = dockspace->borders[border_index].general_handler.position_x.size;
			}
			else {
				state.hoverable_count = 0;
				state.clickable_count = 0;
				state.general_count = 0;
			}

			return state;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t* UIDrawer::GetCounts() {
			return counts;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void** UIDrawer::GetSystemBuffers() {
			return system_buffers;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t* UIDrawer::GetSystemCounts() {
			return system_counts;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetFontSize() const {
			return { ECS_TOOLS_UI_FONT_X_FACTOR * font.size, font.size };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIFontDescriptor* UIDrawer::GetFontDescriptor() {
			return &font;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UILayoutDescriptor* UIDrawer::GetLayoutDescriptor() {
			return &layout;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIColorThemeDescriptor* UIDrawer::GetColorThemeDescriptor() {
			return &color_theme;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIElementDescriptor* UIDrawer::GetElementDescriptor() {
			return &element_descriptor;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetLastSolidColorRectanglePosition(size_t configuration, unsigned int previous_rectangle) {
			UIVertexColor* buffer = HandleSolidColorBuffer(configuration);
			size_t* count = HandleSolidColorCount(configuration);

			if (*count > 6 * previous_rectangle) {
				size_t new_index = *count - previous_rectangle * 6 - 6;
				return { buffer[new_index].position.x, -buffer[new_index].position.y };
			}
			return float2(0.0f, 0.0f);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetLastSolidColorRectangleScale(size_t configuration, unsigned int previous_rectangle) {
			UIVertexColor* buffer = HandleSolidColorBuffer(configuration);
			size_t* count = HandleSolidColorCount(configuration);

			if (*count > 6 * previous_rectangle) {
				size_t new_index = *count - previous_rectangle * 6;
				return { buffer[new_index - 5].position.x - buffer[new_index - 6].position.x, buffer[new_index - 3].position.y - buffer[new_index - 1].position.y };
			}
			return float2(0.0f, 0.0f);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetLastSpriteRectanglePosition(size_t configuration, unsigned int previous_rectangle) {
			UISpriteVertex* buffer = HandleSpriteBuffer(configuration);
			size_t* count = HandleSpriteCount(configuration);

			if (*count > 6 * previous_rectangle) {
				size_t new_index = *count - previous_rectangle * 6 - 6;
				return { buffer[new_index].position.x, -buffer[new_index].position.y };
			}
			return float2(0.0f, 0.0f);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetLastSpriteRectangleScale(size_t configuration, unsigned int previous_rectangle) {
			UISpriteVertex* buffer = HandleSpriteBuffer(configuration);
			size_t* count = HandleSpriteCount(configuration);

			if (*count > 6 * previous_rectangle) {
				size_t new_index = *count - previous_rectangle * 6;
				return { buffer[new_index - 5].position.x - buffer[new_index - 6].position.x, buffer[new_index - 3].position.y - buffer[new_index - 1].position.y };
			}
			return float2(0.0f, 0.0f);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDockspace* UIDrawer::GetDockspace() {
			return dockspace;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GetAlignedToCenterX(float x_scale) const {
			float position = function::Select(export_scale != nullptr, current_x, region_position.x);
			float _region_scale = function::Select(export_scale != nullptr, x_scale, region_scale.x);
			return AlignMiddle(position, _region_scale, x_scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GetAlignedToCenterY(float y_scale) const {
			float position = function::Select(export_scale != nullptr, current_y, region_position.y);
			float _region_scale = function::Select(export_scale != nullptr, y_scale, region_scale.y);
			return AlignMiddle(position, _region_scale, y_scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetAlignedToCenter(float2 scale) const {
			return { GetAlignedToCenterX(scale.x), GetAlignedToCenterY(scale.y) };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetAlignedToRight(float x_scale, float target_position) const {
			const float EPSILON = 0.0005f;

			target_position = function::Select(target_position == -5.0f, region_limit.x, target_position);
			target_position = function::Select(export_scale != nullptr, current_x + x_scale, target_position);

			// Move the position by a small offset so as to not have floating point calculation errors that would result
			// In an increased render span even tho it supposed to not contribute to it
			target_position -= EPSILON;
			return { function::ClampMin(target_position - x_scale, current_x), current_y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetAlignedToRightOverLimit(float x_scale) const {
			float vertical_slider_offset = system->m_windows[window_index].is_vertical_render_slider * system->m_descriptors.misc.render_slider_vertical_size;
			return { function::ClampMin(region_position.x + region_scale.x - x_scale - vertical_slider_offset, current_x), current_y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetAlignedToBottom(float y_scale, float target_position) const {
			const float EPSILON = 0.0003f;

			target_position = function::Select(target_position == -5.0f, region_limit.y, target_position);
			target_position = function::Select(export_scale != nullptr, current_y + y_scale, target_position);
			
			// Move the position by a small offset so as to not have floating point calculation errors that would result
			// In an increased render span even tho it supposed to not contribute to it
			target_position -= EPSILON;
			return { current_x, function::ClampMin(target_position - y_scale, current_y) };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIDrawer::GetBorderIndex() const {
			return border_index;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		DockspaceType UIDrawer::GetDockspaceType() const {
			return dockspace_type;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		ActionData UIDrawer::GetDummyActionData() {
			return system->GetFilledActionData(window_index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void* UIDrawer::GetMainAllocatorBuffer(size_t size, size_t alignment) {
			void* allocation = system->m_memory->Allocate(size, alignment);
			system->AddWindowMemoryResource(allocation, window_index);
			ECS_ASSERT(allocation != nullptr);

			if (initializer) {
				ECS_ASSERT(last_initialized_element_allocations.size < last_initialized_element_allocations.capacity);
				if (last_initialized_element_allocations.size < last_initialized_element_allocations.capacity) {
					last_initialized_element_allocations.Add(allocation);
				}
			}

			return allocation;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void* UIDrawer::GetMainAllocatorBufferAndStoreAsResource(const char* name, size_t size, size_t alignment) {
			size_t name_size = strlen(name);
			void* resource = GetMainAllocatorBuffer(size + name_size * sizeof(char), alignment);
			void* name_ptr = function::OffsetPointer(resource, size * sizeof(char));
			memcpy(name_ptr, name, sizeof(char) * name_size);
			ResourceIdentifier identifier(name_ptr, name_size);
			AddWindowResourceToTable(resource, identifier);
			return resource;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIDrawer::GetWindowIndex() const {
			return window_index;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Hierarchy

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerHierarchy* UIDrawer::Hierarchy(const char* name) {
			UIDrawConfig config;
			return Hierarchy(0, config, name);
		}

		UIDrawerHierarchy* UIDrawer::Hierarchy(size_t configuration, const UIDrawConfig& config, const char* name) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerHierarchy* data = (UIDrawerHierarchy*)GetResource(name);

					HierarchyDrawer(configuration, config, data, scale);
					HandleDynamicResource(configuration, name);
					return data;
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeHierarchy initialize_data;
						initialize_data.config = (UIDrawConfig*)&config;
						ECS_FORWARD_STRUCT_MEMBERS_1(initialize_data, name);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							name,
							InitializeHierarchyElement,
							DynamicConfiguration(configuration)
						);
					}
					Hierarchy(DynamicConfiguration(configuration), config, name);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					return HierarchyInitializer(configuration, config, name);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Histogram

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Histogram(Stream<float> samples, const char* name) {
			UIDrawConfig config;
			Histogram(0, config, samples, name);
		}

		void UIDrawer::Histogram(size_t configuration, const UIDrawConfig& config, Stream<float> samples, const char* name) {
			if (!initializer) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				HistogramDrawer(configuration, config, samples, name, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::InitializeElementName(
			size_t configuration,
			size_t no_element_name,
			const UIDrawConfig& config,
			const char* name,
			UIDrawerTextElement* element,
			float2 position
		) {
			if (~configuration & no_element_name) {
				ConvertTextToWindowResource(configuration, config, name, element, position);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::HasClicked(float2 position, float2 scale) {
			return IsPointInRectangle(mouse_position, position, scale) && system->m_mouse_tracker->LeftButton() == MBPRESSED;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::IsMouseInRectangle(float2 position, float2 scale) {
			return IsPointInRectangle(mouse_position, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Indent() {
			min_render_bounds.x = function::Select(min_render_bounds.x > current_x - region_render_offset.x, current_x - region_render_offset.x, min_render_bounds.x);
			current_x += layout.element_indentation + current_column_x_scale;
			current_column_x_scale = 0.0f;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Indent(float count) {
			min_render_bounds.x = function::Select(min_render_bounds.x > current_x - region_render_offset.x, current_x - region_render_offset.x, min_render_bounds.x);
			current_x += count * layout.element_indentation + current_column_x_scale;
			current_column_x_scale = 0.0f;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Label hierarchy

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::LabelHierarchy(const char* identifier, Stream<const char*> labels) {
			UIDrawConfig config;
			LabelHierarchy(0, config, identifier, labels);
		}

		// Parent index 0 means root
		UIDrawerLabelHierarchy* UIDrawer::LabelHierarchy(size_t configuration, UIDrawConfig& config, const char* identifier, Stream<const char*> labels) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerLabelHierarchy* data = (UIDrawerLabelHierarchy*)GetResource(identifier);

					LabelHierarchyDrawer(configuration, config, data, labels, position, scale);
					HandleDynamicResource(configuration, identifier);
					return data;
				}
				else {
					bool exists = ExistsResource(identifier);
					if (!exists) {
						UIDrawerInitializeLabelHierarchy initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_2(initialize_data, identifier, labels);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							identifier,
							InitializeLabelHierarchyElement,
							DynamicConfiguration(configuration)
						);
					}
					return LabelHierarchy(DynamicConfiguration(configuration), config, identifier, labels);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					return LabelHierarchyInitializer(configuration, config, identifier);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerLabelHierarchy* UIDrawer::LabelHierarchyInitializer(size_t configuration, const UIDrawConfig& config, const char* _identifier) {
			UIDrawerLabelHierarchy* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(_identifier, [&](const char* identifier) {
				data = GetMainAllocatorBuffer<UIDrawerLabelHierarchy>();

				size_t max_characters = 256;

				unsigned int hash_table_capacity = 256;
				if (configuration & UI_CONFIG_LABEL_HIERARCHY_HASH_TABLE_CAPACITY) {
					const UIConfigLabelHierarchyHashTableCapacity* capacity = (const UIConfigLabelHierarchyHashTableCapacity*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_HASH_TABLE_CAPACITY);
					hash_table_capacity = capacity->capacity;
				}

				size_t total_size = data->active_label.MemoryOf(max_characters) + data->selected_label_temporary.MemoryOf(max_characters) +
					data->right_click_label_temporary.MemoryOf(max_characters) + data->label_states.MemoryOf(hash_table_capacity);
				void* allocation = GetMainAllocatorBuffer(total_size);
				uintptr_t allocation_ptr = (uintptr_t)allocation;
				data->label_states.InitializeFromBuffer(allocation_ptr, hash_table_capacity);
				data->active_label.InitializeFromBuffer(allocation_ptr, max_characters);
				data->selected_label_temporary.InitializeFromBuffer(allocation_ptr, max_characters);
				data->right_click_label_temporary.InitializeFromBuffer(allocation_ptr, max_characters);

				if (configuration & UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK) {
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

				if (configuration & UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK) {
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

				return data;
				});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::LabelHierarchyDrawer(
			size_t configuration,
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
			if (configuration & UI_CONFIG_LABEL_HIERARCHY_SPRITE_TEXTURE) {
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
			if (configuration & UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK) {
				const UIConfigLabelHierarchySelectableCallback* selectable = (const UIConfigLabelHierarchySelectableCallback*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK);
				if (!selectable->copy_on_initialization && selectable->callback_data_size > 0) {
					memcpy(data->selectable_callback_data, selectable->callback_data, selectable->callback_data_size);
				}
				selectable_callback_phase = selectable->phase;
			}

			UIDrawPhase right_click_phase = UIDrawPhase::Normal;
			// check right click update of data
			if (configuration & UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK) {
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
			HandleText(configuration, config, text_color, font_size, character_spacing);

			Color label_color = HandleColor(configuration, config);
			UIConfigTextAlignment text_alignment;
			text_alignment.horizontal = TextAlignment::Left;
			text_alignment.vertical = TextAlignment::Middle;
			config.AddFlag(text_alignment);

			HashTable<unsigned int, ResourceIdentifier, HashFunctionPowerOfTwo, HashFunctionMultiplyString> parent_hash_table;
			size_t table_count = function::PowerOfTwoGreater(labels.size).x * 2;
			parent_hash_table.InitializeFromBuffer(GetTempBuffer(parent_hash_table.MemoryOf(table_count)), table_count);

			size_t label_configuration = UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE |
				UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_ADVANCE;
			for (size_t index = 0; index < labels.size; index++) {
				unsigned int current_parent_index = 0;
				Stream<char> label_stream(labels[index], strlen(labels[index]));
				size_t parent_path_size = function::PathParentSize(label_stream);

				if (parent_path_size != 0) {
					ResourceIdentifier identifier(labels[index], parent_path_size);
					parent_hash_table.TryGetValue(identifier, current_parent_index);
				}

				// check to see if it is inside the hash table; if it is, then 
				// increase the activation count else introduce it
				ResourceIdentifier identifier(labels[index], label_stream.size);
				ECS_ASSERT(!parent_hash_table.Insert(index + 1, identifier));
				
				unsigned int table_index = data->label_states.Find(identifier);

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
					ECS_ASSERT(!data->label_states.Insert(current_data, identifier));
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
				if (stack.GetSize() == 0) {
					stack.Push({ current_parent_index, 0.0f });
				}
				else {
					stack.Peek(last_element);
					if (last_element.parent_index < current_parent_index) {
						stack.Push({ current_parent_index, last_element.next_row_gain + layout.element_indentation * 2.0f });
						current_gain = last_element.next_row_gain + layout.element_indentation * 2.0f;
					}
					else if (last_element.parent_index > current_parent_index) {
						while (stack.GetSize() > 0 && last_element.parent_index > current_parent_index) {
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
							parent_hash_table.TryGetValue(next_identifier, next_parent_index);
						}
						has_children = index < next_parent_index;
					}

					current_position = { current_x + current_gain - region_render_offset.x, current_y - region_render_offset.y };
					float2 initial_label_position = current_position;
					scale.x = horizontal_bound - current_position.x - horizontal_texture_offset;

					if (ValidatePosition(configuration, current_position, { horizontal_bound - current_position.x, scale.y })) {
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
						TextLabel(
							configuration | label_configuration | UI_CONFIG_LABEL_TRANSPARENT,
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
								SpriteRectangle(configuration, current_position, square_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, text_color);
							}
							else {
								SpriteRectangle(configuration, current_position, square_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, text_color, { 1.0f, 0.0f }, { 0.0f, 1.0f });
							}
							AddClickable(current_position, square_scale, { LabelHierarchyChangeState, &change_state_data, sizeof(change_state_data), selectable_callback_phase });
						}
						current_position.x += square_scale.x;
						if (configuration & UI_CONFIG_LABEL_HIERARCHY_SPRITE_TEXTURE) {
							if (label_state) {
								SpriteRectangle(configuration, current_position, square_scale, opened_texture, opened_color, opened_top_left_uv, opened_bottom_right_uv);
							}
							else {
								SpriteRectangle(configuration, current_position, square_scale, closed_texture, closed_color, closed_top_left_uv, closed_bottom_right_uv);
							}
							if (!keep_triangle && has_children) {
								AddClickable(current_position, square_scale, { LabelHierarchyChangeState, &change_state_data, sizeof(change_state_data), selectable_callback_phase });
							}
						}

						if (configuration & UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK) {
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
							SolidColorRectangle(configuration, initial_label_position, { horizontal_scale, scale.y }, current_color);
						}

					}

					FinalizeRectangle(configuration, initial_label_position, { horizontal_bound - initial_label_position.x, scale.y });
					NextRow();
				}

			}

			/*size_t milliseconds = stack_timer.GetDuration_ms();
			ECS_STACK_CAPACITY_STREAM(char, milliseconds_duration, 64);
			function::ConvertIntToChars(milliseconds_duration, milliseconds);
			milliseconds_duration[milliseconds_duration.size] = '\n';
			milliseconds_duration[milliseconds_duration.size + 1] = '\0';
			OutputDebugStringA(milliseconds_duration.buffer);*/

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

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region List

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerList* UIDrawer::List(const char* name) {
			UIDrawConfig config;
			return List(0, config, name);
		}

		UIDrawerList* UIDrawer::List(size_t configuration, UIDrawConfig& config, const char* name) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerList* list = (UIDrawerList*)GetResource(name);

					ListDrawer(configuration, config, list, position, scale);
					HandleDynamicResource(configuration, name);
					return list;
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeList initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_1(initialize_data, name);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							name,
							InitializeListElement,
							DynamicConfiguration(configuration)
						);
					}
					return List(DynamicConfiguration(configuration), config, name);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					return ListInitializer(configuration, config, name);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ListFinalizeNode(UIDrawerList* list) {
			list->FinalizeNodeYscale((const void**)buffers, counts);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Label List

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::LabelList(const char* name, Stream<const char*> labels) {
			UIDrawConfig config;
			LabelList(0, config, name, labels);
		}

		void UIDrawer::LabelList(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerLabelList* data = (UIDrawerLabelList*)GetResource(name);

					LabelListDrawer(configuration, config, data, position, scale);
				}
				else {
					LabelListDrawer(configuration, config, name, labels, position, scale);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					LabelListInitializer(configuration, config, name, labels);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::LabelListDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerLabelList* data, float2 position, float2 scale) {
			if (~configuration & UI_CONFIG_LABEL_LIST_NO_NAME) {
				ElementName(configuration, config, &data->name, position, scale);
				NextRow();
			}

			Color font_color;
			float2 font_size;
			float character_spacing;
			HandleText(configuration, config, font_color, font_size, character_spacing);

			float font_scale = system->GetTextSpriteYScale(font_size.y);
			float label_scale = font_scale + element_descriptor.label_vertical_padd * 2.0f;
			float2 square_scale = GetSquareScale(element_descriptor.label_list_circle_size);
			for (size_t index = 0; index < data->labels.size; index++) {
				position = GetCurrentPosition();
				float2 circle_position = { position.x, AlignMiddle(position.y, label_scale, square_scale.y) };
				SpriteRectangle(configuration, circle_position, square_scale, ECS_TOOLS_UI_TEXTURE_FILLED_CIRCLE, font_color);
				position.x += square_scale.x + layout.element_indentation;
				TextLabelDrawer(
					configuration | UI_CONFIG_LABEL_TRANSPARENT,
					config,
					data->labels.buffer + index,
					position,
					scale
				);
				NextRow();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::LabelListDrawer(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels, float2 position, float2 scale) {
			if (~configuration & UI_CONFIG_LABEL_LIST_NO_NAME) {
				ElementName(configuration, config, name, position, scale);
				NextRow();
			}

			Color font_color;
			float2 font_size;
			float character_spacing;
			HandleText(configuration, config, font_color, font_size, character_spacing);

			float font_scale = system->GetTextSpriteYScale(font_size.y);
			float label_scale = font_scale + element_descriptor.label_vertical_padd * 2.0f;
			float2 square_scale = GetSquareScale(element_descriptor.label_list_circle_size);
			for (size_t index = 0; index < labels.size; index++) {
				position = GetCurrentPosition();
				float2 circle_position = { position.x, AlignMiddle(position.y, label_scale, square_scale.y) };
				SpriteRectangle(configuration, circle_position, square_scale, ECS_TOOLS_UI_TEXTURE_FILLED_CIRCLE, font_color);
				UpdateCurrentColumnScale(square_scale.x);
				Indent();
				TextLabel(
					configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT,
					config,
					labels.buffer[index]
				);
				NextRow();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerLabelList* UIDrawer::LabelListInitializer(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels) {
			UIDrawerLabelList* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				data = GetMainAllocatorBuffer<UIDrawerLabelList>();
				data->labels.buffer = (UIDrawerTextElement*)GetMainAllocatorBuffer(sizeof(UIDrawerTextElement) * labels.size);

				InitializeElementName(configuration, UI_CONFIG_LABEL_LIST_NO_NAME, config, name, &data->name, { 0.0f, 0.0f });
				for (size_t index = 0; index < labels.size; index++) {
					ConvertTextToWindowResource(configuration, config, labels[index], data->labels.buffer + index, { 0.0f, 0.0f });
				}

				return data;
				});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Menu

		// ------------------------------------------------------------------------------------------------------------------------------------

		// State can be stack allocated; name should be unique if drawing with a sprite
		void UIDrawer::Menu(const char* name, UIDrawerMenuState* state) {
			UIDrawConfig config;
			Menu(0, config, name, state);
		}

		// State can be stack allocated; name should be unique if drawing with a sprite
		void UIDrawer::Menu(size_t configuration, const UIDrawConfig& config, const char* name, UIDrawerMenuState* state) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerMenu* data = (UIDrawerMenu*)GetResource(name);

					MenuDrawer(configuration, config, data, position, scale);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeMenu initialize_data;
						initialize_data.config = (UIDrawConfig*)&config;
						ECS_FORWARD_STRUCT_MEMBERS_2(initialize_data, name, state);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							name,
							InitializeMenuElement,
							DynamicConfiguration(configuration)
						);
					}
					Menu(DynamicConfiguration(configuration), config, name, state);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					MenuInitializer(configuration, config, name, scale, state);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void* UIDrawer::MenuAllocateStateHandlers(Stream<UIActionHandler> handlers) {
			void* allocation = GetMainAllocatorBuffer(sizeof(UIActionHandler) * handlers.size);
			memcpy(allocation, handlers.buffer, sizeof(UIActionHandler) * handlers.size);
			return allocation;
		}

#pragma endregion

#pragma region Number input

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatInput(const char* name, float* value, float default_value, float lower_bound, float upper_bound) {
			UIDrawConfig config;
			FloatInput(0, config, name, value, default_value, lower_bound, upper_bound);
		}

		void UIDrawer::FloatInput(size_t configuration, UIDrawConfig& config, const char* name, float* value, float default_value, float lower_bound, float upper_bound) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					FloatInputDrawer(configuration, config, name, value, lower_bound, upper_bound, position, scale);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeFloatInput initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_5(initialize_data, lower_bound, name, upper_bound, value, default_value);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							name,
							InitializeFloatInputElement,
							DynamicConfiguration(configuration)
						);
					}
					FloatInput(DynamicConfiguration(configuration), config, name, value, default_value, lower_bound, upper_bound);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					FloatInputInitializer(configuration, config, name, value, default_value, lower_bound, upper_bound, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatInputGroup(
			size_t count,
			const char* group_name,
			const char** names,
			float** values,
			const float* default_values,
			const float* lower_bound,
			const float* upper_bound
		) {
			UIDrawConfig config;
			FloatInputGroup(0, config, count, group_name, names, values, default_values, lower_bound, upper_bound);
		}

		void UIDrawer::FloatInputGroup(
			size_t configuration,
			UIDrawConfig& config,
			size_t count,
			const char* group_name,
			const char** names,
			float** values,
			const float* default_values,
			const float* lower_bound,
			const float* upper_bound
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					FloatInputGroupDrawer(configuration, config, group_name, count, names, values, lower_bound, upper_bound, position, scale);
					HandleDynamicResource(configuration, group_name);
				}
				else {
					bool exists = ExistsResource(group_name);
					if (!exists) {
						UIDrawerInitializeFloatInputGroup initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_7(initialize_data, count, group_name, lower_bound, names, upper_bound, values, default_values);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							group_name,
							InitializeFloatInputGroupElement,
							DynamicConfiguration(configuration)
						);
					}
					FloatInputGroup(DynamicConfiguration(configuration), config, count, group_name, names, values, default_values, lower_bound, upper_bound);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					FloatInputGroupInitializer(configuration, config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleInput(const char* name, double* value, double default_value, double lower_bound, double upper_bound) {
			UIDrawConfig config;
			DoubleInput(0, config, name, value, default_value, lower_bound, upper_bound);
		}

		void UIDrawer::DoubleInput(size_t configuration, UIDrawConfig& config, const char* name, double* value, double default_value, double lower_bound, double upper_bound) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					DoubleInputDrawer(configuration, config, name, value, lower_bound, upper_bound, position, scale);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeDoubleInput initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_5(initialize_data, lower_bound, name, upper_bound, value, default_value);
						InitializeDrawerElement(*this, &initialize_data, name, InitializeDoubleInputElement, DynamicConfiguration(configuration));
					}
					DoubleInput(DynamicConfiguration(configuration), config, name, value, default_value, lower_bound, upper_bound);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					DoubleInputInitializer(configuration, config, name, value, default_value, lower_bound, upper_bound, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleInputGroup(
			size_t count,
			const char* group_name,
			const char** names,
			double** values,
			const double* default_values,
			const double* lower_bound,
			const double* upper_bound
		) {
			UIDrawConfig config;
			DoubleInputGroup(0, config, count, group_name, names, values, default_values, lower_bound, upper_bound);
		}

		void UIDrawer::DoubleInputGroup(
			size_t configuration,
			UIDrawConfig& config,
			size_t count,
			const char* group_name,
			const char** names,
			double** values,
			const double* default_values,
			const double* lower_bound,
			const double* upper_bound
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					DoubleInputGroupDrawer(configuration, config, group_name, count, names, values, lower_bound, upper_bound, position, scale);
					HandleDynamicResource(configuration, group_name);
				}
				else {
					bool exists = ExistsResource(group_name);
					if (!exists) {
						UIDrawerInitializeDoubleInputGroup initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_7(initialize_data, group_name, count, names, values, lower_bound, upper_bound, default_values);
						InitializeDrawerElement(*this, &initialize_data, group_name, InitializeDoubleInputGroupElement, DynamicConfiguration(configuration));
					}
					DoubleInputGroup(DynamicConfiguration(configuration), config, count, group_name, names, values, default_values, lower_bound, upper_bound);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					DoubleInputGroupInitializer(configuration, config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntInput(const char* name, Integer* value, Integer default_value, Integer min, Integer max) {
			UIDrawConfig config;
			IntInput(0, config, name, value, min, max);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInput, const char*, integer*, integer, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::IntInput(size_t configuration, UIDrawConfig& config, const char* name, Integer* value, Integer default_value, Integer min, Integer max) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					IntInputDrawer<Integer>(configuration, config, name, value, min, max, position, scale);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeIntegerInput<Integer> initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_5(initialize_data, name, value, min, max, default_value);
						InitializeDrawerElement(*this, &initialize_data, name, InitializeIntegerInputElement<Integer>, DynamicConfiguration(configuration));
					}
					IntInput<Integer>(DynamicConfiguration(configuration), config, name, value, default_value, min, max);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					IntInputInitializer<Integer>(configuration, config, name, value, default_value, min, max, position, scale);
				}
			}
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInput, size_t, UIDrawConfig&, const char*, integer*, integer, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntInputGroup(
			size_t count,
			const char* group_name,
			const char** names,
			Integer** values,
			const Integer* default_values,
			const Integer* lower_bound,
			const Integer* upper_bound
		) {
			UIDrawConfig config;
			IntInputGroup(0, config, count, group_name, names, values, default_values, lower_bound, upper_bound);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInputGroup, size_t, const char*, const char**, integer**, const integer*, const integer*, const integer*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::IntInputGroup(
			size_t configuration,
			UIDrawConfig& config,
			size_t count,
			const char* group_name,
			const char** names,
			Integer** values,
			const Integer* default_values,
			const Integer* lower_bound,
			const Integer* upper_bound
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					IntInputGroupDrawer<Integer>(configuration, config, group_name, count, names, values, lower_bound, upper_bound, position, scale);
					HandleDynamicResource(configuration, group_name);
				}
				else {
					bool exists = ExistsResource(group_name);
					if (!exists) {
						UIDrawerInitializeIntegerInputGroup<Integer> initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_7(initialize_data, count, group_name, names, values, lower_bound, upper_bound, default_values);
						InitializeDrawerElement(*this, &initialize_data, group_name, InitializeIntegerInputGroupElement<Integer>, DynamicConfiguration(configuration));
					}
					IntInputGroup<Integer>(DynamicConfiguration(configuration), config, count, group_name, names, values, default_values, lower_bound, upper_bound);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					IntInputGroupInitializer<Integer>(configuration, config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
				}
			}
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInputGroup, size_t, UIDrawConfig&, size_t, const char*, const char**, integer**, const integer*, const integer*, const integer*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::NextRow() {
			min_render_bounds.y = function::Select(min_render_bounds.y > current_y - region_render_offset.y, current_y - region_render_offset.y, min_render_bounds.y);
			current_y += layout.next_row_y_offset + current_row_y_scale;
			current_x = GetNextRowXPosition();
			current_row_y_scale = 0.0f;
			current_column_x_scale = 0.0f;
			draw_mode_extra_float.y = current_y;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::NextRow(float count) {
			min_render_bounds.y = function::Select(min_render_bounds.y > current_y - region_render_offset.y, current_y - region_render_offset.y, min_render_bounds.y);
			current_y += count * layout.next_row_y_offset + current_row_y_scale;
			current_x = GetNextRowXPosition();
			current_row_y_scale = 0.0f;
			current_column_x_scale = 0.0f;
			draw_mode_extra_float.x = current_y;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::NormalizeHorizontalToWindowDimensions(float value) const {
			return system->NormalizeHorizontalToWindowDimensions(value);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::OffsetNextRow(float value) {
			next_row_offset += value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::OffsetX(float value) {
			current_x += value;
			current_column_x_scale -= value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::OffsetY(float value) {
			current_y += value;
			current_row_y_scale -= value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Progress bar

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ProgressBar(const char* name, float percentage, float x_scale_factor) {
			UIDrawConfig config;
			ProgressBar(0, config, name, percentage, x_scale_factor);
		}

		void UIDrawer::ProgressBar(size_t configuration, UIDrawConfig& config, const char* name, float percentage, float x_scale_factor) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			scale.x *= x_scale_factor;

			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					Color bar_color;
					if (configuration & UI_CONFIG_COLOR) {
						const UIConfigColor* color_ptr = (const UIConfigColor*)config.GetParameter(UI_CONFIG_COLOR);
						bar_color = color_ptr->color;
					}
					else {
						bar_color = color_theme.progress_bar;
					}

					UIConfigBorder border;
					border.color = color_theme.theme;
					border.is_interior = false;
					border.thickness = system->m_descriptors.dockspaces.border_size * 2;
					if (~configuration & UI_CONFIG_BORDER) {
						config.AddFlag(border);
					}

					HandleBorder(configuration | UI_CONFIG_BORDER, config, position, scale);
					SolidColorRectangle(configuration, position, { scale.x * percentage, scale.y }, bar_color);

					if (~configuration & UI_CONFIG_BORDER) {
						config.flag_count--;
					}

					float percent_value = percentage * 100.0f;
					char temp_values[16];
					Stream<char> temp_stream = Stream<char>(temp_values, 0);
					function::ConvertFloatToChars(temp_stream, percent_value, 0);
					temp_stream.Add('%');
					temp_stream[temp_stream.size] = '\0';
					auto text_stream = GetTextStream(configuration, temp_stream.size * 6);
					float2 text_position = { position.x + scale.x * percentage, position.y };

					Color text_color;
					float character_spacing;
					float2 font_size;
					HandleText(configuration, config, text_color, font_size, character_spacing);
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
					auto text_count = HandleTextSpriteCount(configuration);
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

					HandleRectangleActions(configuration, config, position, scale);
				}

				UIConfigTextAlignment alignment;
				alignment.horizontal = TextAlignment::Left;
				alignment.vertical = TextAlignment::Middle;
				UIConfigTextAlignment previous_alignment;
				if (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
					config.SetExistingFlag(alignment, UI_CONFIG_TEXT_ALIGNMENT, previous_alignment);
				}
				else {
					config.AddFlag(alignment);
				}

				FinalizeRectangle(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, position, scale);
				position.x += scale.x + layout.element_indentation * 0.5f;
				TextLabel(configuration | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_TRANSPARENT, config, name, position, scale);

				if (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
					config.SetExistingFlag(previous_alignment, UI_CONFIG_TEXT_ALIGNMENT, alignment);
				}
				else {
					config.flag_count--;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::PopIdentifierStack() {
			identifier_stack.size--;
			current_identifier.size -= identifier_stack[identifier_stack.size];
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::PushIdentifierStack(const char* identifier) {
			size_t count = strlen(identifier);

			ECS_ASSERT(count + current_identifier.size < system->m_descriptors.misc.drawer_identifier_memory);
			ECS_ASSERT(identifier_stack.size < system->m_descriptors.misc.drawer_temp_memory);

			memcpy(current_identifier.buffer + current_identifier.size, identifier, count);
			identifier_stack.Add(count);
			current_identifier.size += count;
			current_identifier[current_identifier.size] = '\0';
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::PushIdentifierStack(size_t index) {
			char temp_chars[64];
			Stream<char> stream_char = Stream<char>(temp_chars, 0);
			function::ConvertIntToChars(stream_char, (int64_t)index);
			PushIdentifierStack(temp_chars);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::PushIdentifierStackRandom(size_t index) {
			index = GetRandomIntIdentifier(index);
			PushIdentifierStack(index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// Returns whether or not it pushed onto the stack
		bool UIDrawer::PushIdentifierStackStringPattern() {
			if (current_identifier.size < 2 || (current_identifier[0] != ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR && current_identifier[1] != ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR)) {
				PushIdentifierStack(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
				return true;
			}
			return false;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::RemoveAllocation(const void* allocation) {
			system->m_memory->Deallocate(allocation);
			system->RemoveWindowMemoryResource(window_index, allocation);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::RestoreBufferState(size_t configuration, UIDrawerBufferState state) {
			if (!initializer) {
				size_t* solid_color_count = HandleSolidColorCount(configuration);
				size_t* text_sprite_count = HandleTextSpriteCount(configuration);
				size_t* sprite_count = HandleSpriteCount(configuration);
				size_t* sprite_cluster_count = HandleSpriteClusterCount(configuration);

				size_t sprite_difference = *sprite_count - state.sprite_count;
				size_t sprite_cluster_difference = *sprite_cluster_count - state.sprite_cluster_count;

				sprite_difference /= 6;
				sprite_cluster_difference /= 6;

				UIDrawPhase phase = HandlePhase(configuration);
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::RestoreHandlerState(UIDrawerHandlerState state) {
			if (!initializer) {
				dockspace->borders[border_index].hoverable_handler.position_x.size = state.hoverable_count;
				dockspace->borders[border_index].clickable_handler.position_x.size = state.clickable_count;
				dockspace->borders[border_index].general_handler.position_x.size = state.general_count;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Sentence

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Sentence(const char* text) {
			UIDrawConfig config;
			Sentence(0, config, text);
		}

		void UIDrawer::Sentence(size_t configuration, const UIDrawConfig& config, const char* text) {
			if (!initializer) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				void* resource = nullptr;

				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					resource = GetResource(text);
				}
				SentenceDrawer(configuration, config, HandleResourceIdentifier(text), resource, position);
			}
			else {
				SentenceInitializer(configuration, config, text);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetCurrentX(float value) {
			current_x = value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetCurrentY(float value) {
			current_y = value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<bool reduce_values>
		void UIDrawer::SetLayoutFromZoomXFactor(float zoom_factor) {
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

		ECS_TEMPLATE_FUNCTION_BOOL(void, UIDrawer::SetLayoutFromZoomXFactor, float);

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<bool reduce_values>
		void UIDrawer::SetLayoutFromZoomYFactor(float zoom_factor) {
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

		ECS_TEMPLATE_FUNCTION_BOOL(void, UIDrawer::SetLayoutFromZoomYFactor, float);

		// ------------------------------------------------------------------------------------------------------------------------------------


#pragma region Rectangle

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Rectangle(size_t configuration, const UIDrawConfig& config) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			Rectangle(configuration, config, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Rectangles(size_t configuration, const UIDrawConfig& overall_config, const UIDrawConfig* configs, const float2* offsets, size_t count) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, overall_config);

			float2 initial_position = position;
			for (size_t index = 0; index < count; index++) {
				if (configuration & UI_CONFIG_RECTANGLES_INDIVIDUAL_ACTIONS) {
					Rectangle(configuration | UI_CONFIG_DO_NOT_ADVANCE, configs[index], position, scale);
				}
				else {
					Rectangle((configuration | UI_CONFIG_DO_NOT_ADVANCE) & ~(UI_CONFIG_HOVERABLE_ACTION | UI_CONFIG_CLICKABLE_ACTION | UI_CONFIG_GENERAL_ACTION), configs[index], position, scale);
				}
				position.x += offsets[index].x;
				position.y += offsets[index].y;
			}

			float2 total_scale = { position.x - initial_position.x, position.y - initial_position.y };
			if (~configuration & UI_CONFIG_RECTANGLES_INDIVIDUAL_ACTIONS) {
				if (configuration & UI_CONFIG_HOVERABLE_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_HOVERABLE_ACTION);
					AddHoverable(initial_position, total_scale, *handler);
				}
				if (configuration & UI_CONFIG_CLICKABLE_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_HOVERABLE_ACTION);
					AddClickable(initial_position, total_scale, *handler);
				}
				if (configuration & UI_CONFIG_GENERAL_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_HOVERABLE_ACTION);
					AddGeneral(initial_position, total_scale, *handler);
				}
			}

			FinalizeRectangle(configuration, initial_position, total_scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Rectangles(size_t configuration, const UIDrawConfig& overall_config, const UIDrawConfig* configs, float2 offset, size_t count) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, overall_config);

			float2 initial_position = position;
			for (size_t index = 0; index < count; index++) {
				if (configuration & UI_CONFIG_RECTANGLES_INDIVIDUAL_ACTIONS) {
					Rectangle(configuration | UI_CONFIG_DO_NOT_ADVANCE, configs[index], position, scale);
				}
				else {
					Rectangle((configuration | UI_CONFIG_DO_NOT_ADVANCE) & ~(UI_CONFIG_HOVERABLE_ACTION | UI_CONFIG_CLICKABLE_ACTION | UI_CONFIG_GENERAL_ACTION), configs[index], position, scale);
				}
				position.x += offset.x;
				position.y += offset.y;
			}

			float2 total_scale = { position.x - initial_position.x, position.y - initial_position.y };
			if (~configuration & UI_CONFIG_RECTANGLES_INDIVIDUAL_ACTIONS) {
				if (configuration & UI_CONFIG_HOVERABLE_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_HOVERABLE_ACTION);
					AddHoverable(initial_position, total_scale, *handler);
				}
				if (configuration & UI_CONFIG_CLICKABLE_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_CLICKABLE_ACTION);
					AddClickable(initial_position, total_scale, *handler);
				}
				if (configuration & UI_CONFIG_GENERAL_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_GENERAL_ACTION);
					AddGeneral(initial_position, total_scale, *handler);
				}
			}

			FinalizeRectangle(configuration, initial_position, total_scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Sliders

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Slider(
			const char* name,
			unsigned int byte_size,
			void* value_to_modify,
			const void* lower_bound,
			const void* upper_bound,
			const void* default_value,
			const UIDrawerSliderFunctions& functions,
			UIDrawerTextInputFilter filter
		) {
			UIDrawConfig config;
			Slider(0, config, name, byte_size, value_to_modify, lower_bound, upper_bound, default_value, functions, filter);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Slider(
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
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					void* _info = GetResource(name);
					UIDrawerSlider* info = (UIDrawerSlider*)_info;

					SliderDrawer(configuration, config, info, position, scale, value_to_modify, lower_bound, upper_bound, functions, filter);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeSlider initialize_data;
						initialize_data.config = &config;
						initialize_data.functions = &functions;
						ECS_FORWARD_STRUCT_MEMBERS_7(initialize_data, name, byte_size, value_to_modify, lower_bound, upper_bound, default_value, filter);
						InitializeDrawerElement(*this, &initialize_data, name, InitializeSliderElement, DynamicConfiguration(configuration));
					}
					Slider(DynamicConfiguration(configuration), config, name, byte_size, value_to_modify, lower_bound, upper_bound, default_value, functions, filter);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					SliderInitializer(configuration, config, name, position, scale, byte_size, value_to_modify, lower_bound, upper_bound, default_value, functions);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SliderGroup(
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
		) {
			UIDrawConfig config;
			SliderGroup(0, config, count, group_name, names, byte_size, values_to_modify, lower_bounds, upper_bounds, default_values, functions, filter);
		}

		void UIDrawer::SliderGroup(
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
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					SliderGroupDrawer(configuration, config, count, group_name, names, values_to_modify, lower_bounds, upper_bounds, functions, filter, position, scale);
					HandleDynamicResource(configuration, group_name);
				}
				else {
					bool exists = ExistsResource(group_name);
					if (!exists) {
						UIDrawerInitializeSliderGroup initialize_data;
						initialize_data.config = &config;
						initialize_data.functions = &functions;
						ECS_FORWARD_STRUCT_MEMBERS_9(initialize_data, count, group_name, names, byte_size, values_to_modify, lower_bounds, upper_bounds, default_values, filter);
						InitializeDrawerElement(*this, &initialize_data, group_name, InitializeSliderGroupElement, DynamicConfiguration(configuration));
					}
					SliderGroup(DynamicConfiguration(configuration), config, count, group_name, names, byte_size, values_to_modify, lower_bounds, upper_bounds, default_values, functions, filter);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					SliderGroupInitializer(configuration, config, count, group_name, names, byte_size, values_to_modify, lower_bounds, upper_bounds, default_values, functions, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatSlider(const char* name, float* value_to_modify, float lower_bound, float upper_bound, float default_value, unsigned int precision) {
			UIDrawConfig config;
			FloatSlider(0, config, name, value_to_modify, lower_bound, upper_bound, default_value, precision);
		}

		void UIDrawer::FloatSlider(size_t configuration, UIDrawConfig& config, const char* name, float* value_to_modify, float lower_bound, float upper_bound, float default_value, unsigned int precision) {
			UIDrawerSliderFunctions functions = UIDrawerGetFloatSliderFunctions(precision);
			Slider(configuration, config, name, sizeof(float), value_to_modify, &lower_bound, &upper_bound, &default_value, functions, UIDrawerTextInputFilterNumbers);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatSliderGroup(
			size_t count,
			const char* group_name,
			const char** names,
			float** values_to_modify,
			const float* lower_bounds,
			const float* upper_bounds,
			const float* default_values,
			unsigned int precision
		) {
			UIDrawConfig config;;
			FloatSliderGroup(
				0,
				config,
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

		void UIDrawer::FloatSliderGroup(
			size_t configuration,
			UIDrawConfig& config,
			size_t count,
			const char* group_name,
			const char** names,
			float** values_to_modify,
			const float* lower_bounds,
			const float* upper_bounds,
			const float* default_values,
			unsigned int precision
		) {
			UIDrawerSliderFunctions functions = UIDrawerGetFloatSliderFunctions(precision);

			SliderGroup(
				configuration,
				config,
				count,
				group_name,
				names,
				sizeof(float),
				(void**)values_to_modify,
				lower_bounds,
				upper_bounds,
				default_values,
				functions,
				UIDrawerTextInputFilterNumbers
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleSlider(const char* name, double* value_to_modify, double lower_bound, double upper_bound, double default_value, unsigned int precision) {
			UIDrawConfig config;
			DoubleSlider(0, config, name, value_to_modify, lower_bound, upper_bound, default_value, precision);
		}

		void UIDrawer::DoubleSlider(size_t configuration, UIDrawConfig& config, const char* name, double* value_to_modify, double lower_bound, double upper_bound, double default_value, unsigned int precision) {
			UIDrawerSliderFunctions functions = UIDrawerGetDoubleSliderFunctions(precision);
			Slider(configuration, config, name, sizeof(double), value_to_modify, &lower_bound, &upper_bound, &default_value, functions, UIDrawerTextInputFilterNumbers);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleSliderGroup(
			size_t count,
			const char* group_name,
			const char** names,
			double** values_to_modify,
			const double* lower_bounds,
			const double* upper_bounds,
			const double* default_values,
			unsigned int precision
		) {
			UIDrawConfig config;
			DoubleSliderGroup(
				0,
				config,
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

		void UIDrawer::DoubleSliderGroup(
			size_t configuration,
			UIDrawConfig& config,
			size_t count,
			const char* group_name,
			const char** names,
			double** values_to_modify,
			const double* lower_bounds,
			const double* upper_bounds,
			const double* default_values,
			unsigned int precision
		) {
			UIDrawerSliderFunctions functions = UIDrawerGetDoubleSliderFunctions(precision);

			SliderGroup(
				configuration,
				config,
				count,
				group_name,
				names,
				sizeof(double),
				(void**)values_to_modify,
				lower_bounds,
				upper_bounds,
				default_values,
				functions,
				UIDrawerTextInputFilterNumbers
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntSlider(const char* name, Integer* value_to_modify, Integer lower_bound, Integer upper_bound, Integer default_value) {
			UIDrawConfig config;
			IntSlider(0, config, name, value_to_modify, lower_bound, upper_bound, default_value);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntSlider, const char*, integer*, integer, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::IntSlider(size_t configuration, UIDrawConfig& config, const char* name, Integer* value_to_modify, Integer lower_bound, Integer upper_bound, Integer default_value) {
			UIDrawerSliderFunctions functions = UIDrawerGetIntSliderFunctions<Integer>();
			Slider(configuration, config, name, sizeof(Integer), value_to_modify, &lower_bound, &upper_bound, &default_value, functions, UIDrawerTextInputFilterNumbers);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntSlider, size_t, UIDrawConfig&, const char*, integer*, integer, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntSliderGroup(
			size_t count,
			const char* group_name,
			const char** names,
			Integer** values_to_modify,
			const Integer* lower_bounds,
			const Integer* upper_bounds,
			const Integer* default_values
		) {
			UIDrawConfig config;
			IntSliderGroup(
				0,
				config,
				count,
				group_name,
				names,
				values_to_modify,
				lower_bounds,
				upper_bounds,
				default_values
			);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntSliderGroup, size_t, const char*, const char**, integer**, const integer*, const integer*, const integer*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::IntSliderGroup(
			size_t configuration,
			UIDrawConfig& config,
			size_t count,
			const char* group_name,
			const char** names,
			Integer** values_to_modify,
			const Integer* lower_bounds,
			const Integer* upper_bounds,
			const Integer* default_values
		) {
			UIDrawerSliderFunctions functions = UIDrawerGetIntSliderFunctions<Integer>();
			SliderGroup(
				configuration,
				config,
				count,
				group_name,
				names,
				sizeof(Integer),
				(void**)values_to_modify,
				lower_bounds,
				upper_bounds,
				default_values,
				functions,
				UIDrawerTextInputFilterInteger
			);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntSliderGroup, size_t, UIDrawConfig&, size_t, const char*, const char**, integer**, const integer*, const integer*, const integer*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetZoomXFactor(float zoom_x_factor) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetZoomYFactor(float zoom_y_factor) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetRowHorizontalOffset(float value) {
			if (current_x == GetNextRowXPosition()) {
				current_x += value - next_row_offset;
			}
			next_row_offset = value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetRowPadding(float value) {
			region_fit_space_horizontal_offset += value - layout.next_row_padding;
			if (fabsf(current_x - GetNextRowXPosition()) < 0.0001f) {
				current_x += value - layout.next_row_padding;
			}
			region_limit.x -= value - layout.next_row_padding;
			layout.next_row_padding = value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetNextRowYOffset(float value) {
			region_fit_space_vertical_offset += value - layout.next_row_y_offset;
			if (fabsf(current_x - GetNextRowXPosition()) < 0.0001f) {
				current_y += value - layout.next_row_y_offset;
			}
			region_limit.y -= value - layout.next_row_y_offset;
			layout.next_row_y_offset = value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// Currently, draw_mode_floats is needed for column draw, only the y component needs to be filled
		// with the desired y spacing
		void UIDrawer::SetDrawMode(UIDrawerMode mode, unsigned int target, float draw_mode_float) {
			draw_mode = mode;
			draw_mode_count = 0;
			draw_mode_target = target;
			draw_mode_extra_float.x = draw_mode_float;
			if (mode == UIDrawerMode::ColumnDraw || mode == UIDrawerMode::ColumnDrawFitSpace) {
				draw_mode_extra_float.y = current_y;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// places a hoverable on the whole window
		void UIDrawer::SetWindowHoverable(const UIActionHandler* handler) {
			AddHoverable(region_position, region_scale, *handler);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// places a clickable on the whole window
		void UIDrawer::SetWindowClickable(const UIActionHandler* handler) {
			AddClickable(region_position, region_scale, *handler);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// places a general on the whole window
		void UIDrawer::SetWindowGeneral(const UIActionHandler* handler) {
			AddGeneral(region_position, region_scale, *handler);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// configuration is needed for phase deduction
		void UIDrawer::SetSpriteClusterTexture(size_t configuration, const wchar_t* texture, unsigned int count) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				system->SetSpriteCluster(dockspace, border_index, texture, count, UIDrawPhase::Late);
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				system->SetSpriteCluster(dockspace, border_index, texture, count, UIDrawPhase::System);
			}
			else {
				system->SetSpriteCluster(dockspace, border_index, texture, count, UIDrawPhase::Normal);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Solid Color Rectangle

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SolidColorRectangle(Color color) {
			UIDrawConfig config;
			SolidColorRectangle(0, config, color);
		}

		void UIDrawer::SolidColorRectangle(size_t configuration, const UIDrawConfig& config, Color color) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position)) {
					SolidColorRectangle(configuration, position, scale, color);

					HandleBorder(configuration, config, position, scale);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
					HandleRectangleActions(configuration, config, position, scale);
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region State Table

		// ------------------------------------------------------------------------------------------------------------------------------------

		// States should be a stack pointer to bool* to the members that should be changed
		void UIDrawer::StateTable(const char* name, Stream<const char*> labels, bool** states) {
			UIDrawConfig config;
			StateTable(0, config, name, labels, states);
		}

		// States should be a stack pointer to a bool array
		void UIDrawer::StateTable(const char* name, Stream<const char*> labels, bool* states) {
			UIDrawConfig config;
			StateTable(0, config, name, labels, states);
		}

		// States should be a stack pointer to bool* to the members that should be changed
		void UIDrawer::StateTable(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels, bool** states) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerStateTable* data = (UIDrawerStateTable*)GetResource(name);

					StateTableDrawer(configuration, config, data, states, position, scale);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeStateTable initialize_data;
						initialize_data.config = (UIDrawConfig*)&config;
						ECS_FORWARD_STRUCT_MEMBERS_3(initialize_data, name, labels, states);
						InitializeDrawerElement(*this, &initialize_data, name, InitializeStateTableElement, DynamicConfiguration(configuration));
					}
					StateTable(DynamicConfiguration(configuration), config, name, labels, states);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					StateTableInitializer(configuration, config, name, labels, position);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// States should be a stack pointer to a bool array
		void UIDrawer::StateTable(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels, bool* states) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerStateTable* data = (UIDrawerStateTable*)GetResource(name);

					StateTableDrawer(configuration | UI_CONFIG_STATE_TABLE_SINGLE_POINTER, config, data, (bool**)states, position, scale);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeStateTableSinglePointer initialize_data;
						initialize_data.config = (UIDrawConfig*)&config;
						ECS_FORWARD_STRUCT_MEMBERS_3(initialize_data, name, labels, states);
						InitializeDrawerElement(*this, &initialize_data, name, InitializeStateTableSinglePointerElement, DynamicConfiguration(configuration));
					}
					StateTable(DynamicConfiguration(configuration), config, name, labels, states);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					StateTableInitializer(configuration, config, name, labels, position);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerStateTable* UIDrawer::StateTableInitializer(size_t configuration, const UIDrawConfig& config, const char* name, Stream<const char*> labels, float2 position) {
			UIDrawerStateTable* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](const char* identifier) {
				data = GetMainAllocatorBuffer<UIDrawerStateTable>();
				data->labels.Initialize(system->m_memory, labels.size);

				InitializeElementName(configuration, UI_CONFIG_STATE_TABLE_NO_NAME, config, name, &data->name, position);

				data->max_x_scale = 0;
				float max_x_scale = 0.0f;
				for (size_t index = 0; index < labels.size; index++) {
					ConvertTextToWindowResource(configuration, config, labels[index], data->labels.buffer + index, position);
					if (max_x_scale < data->labels[index].scale.x) {
						max_x_scale = data->labels[index].scale.x;
						data->max_x_scale = index;
					}
				}

				return data;
				});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::StateTableDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerStateTable* data, bool** states, float2 position, float2 scale) {
			if (IsElementNameFirst(configuration, UI_CONFIG_STATE_TABLE_NO_NAME) || IsElementNameAfter(configuration, UI_CONFIG_STATE_TABLE_NO_NAME)) {
				ElementName(configuration, config, &data->name, position, scale);
				NextRow();
				position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
			}

			float2 square_scale = GetSquareScale(scale.y);
			scale = { data->labels[data->max_x_scale].scale.x + square_scale.x + element_descriptor.label_horizontal_padd * 2.0f, scale.y * data->labels.size + (data->labels.size - 1) * layout.next_row_y_offset };

			if (configuration & UI_CONFIG_STATE_TABLE_ALL) {
				scale.y += square_scale.y + layout.next_row_y_offset;
			}

			HandleFitSpaceRectangle(configuration, position, scale);

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				float2 current_position = position;
				Color background_color = HandleColor(configuration, config);
				Color checkbox_color = LightenColorClamp(background_color, 1.3f);

				UIDrawerStateTableBoolClickable clickable_data;
				if (configuration & UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE) {
					UIConfigStateTableNotify* notify_data = (UIConfigStateTableNotify*)config.GetParameter(UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE);
					clickable_data.notifier = notify_data->notifier;
				}
				else {
					clickable_data.notifier = nullptr;
				}
				for (size_t index = 0; index < data->labels.size; index++) {
					current_row_y_scale = square_scale.y;

					bool* destination_ptr;
					if (configuration & UI_CONFIG_STATE_TABLE_SINGLE_POINTER) {
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

						SolidColorRectangle(UI_CONFIG_LATE_DRAW, current_position, square_scale, current_color);
						SpriteRectangle(UI_CONFIG_LATE_DRAW, current_position, square_scale, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, current_checkbox_color);
					}

					AddDefaultClickableHoverable(current_position, { scale.x, square_scale.y }, { StateTableBoolClickable, &clickable_data, sizeof(clickable_data) }, background_color);

					current_position.x += square_scale.x + element_descriptor.label_horizontal_padd;
					Text(configuration | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_TEXT_ALIGN_TO_ROW_Y, config, data->labels.buffer + index, current_position);
					NextRow();

					current_position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
				}

				if (configuration & UI_CONFIG_STATE_TABLE_ALL) {
					current_row_y_scale = square_scale.y;

					bool all_true = true;
					if (configuration & UI_CONFIG_STATE_TABLE_SINGLE_POINTER) {
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

						SolidColorRectangle(UI_CONFIG_LATE_DRAW, current_position, square_scale, current_color);
						SpriteRectangle(UI_CONFIG_LATE_DRAW, current_position, square_scale, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, current_checkbox_color);
					}

					UIDrawerStateTableAllButtonData button_data;
					button_data.all_true = all_true;
					button_data.single_pointer = false;
					if (configuration & UI_CONFIG_STATE_TABLE_SINGLE_POINTER) {
						button_data.single_pointer = true;
					}
					button_data.states = Stream<bool*>(states, data->labels.size);
					if (configuration & UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE) {
						UIConfigStateTableNotify* notify = (UIConfigStateTableNotify*)config.GetParameter(UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE);
						button_data.notifier = notify->notifier;
					}
					else {
						button_data.notifier = nullptr;
					}

					AddDefaultClickableHoverable(current_position, { scale.x, square_scale.y }, { StateTableAllButtonAction, &button_data, sizeof(button_data) }, background_color);

					current_position.x += square_scale.x + element_descriptor.label_horizontal_padd;
					Text(configuration | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_TEXT_ALIGN_TO_ROW_Y, config, "All", current_position);
					NextRow();

					current_position = { current_x - region_render_offset.x, current_y - region_render_offset.y };
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Sprite Rectangle

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteRectangle(const wchar_t* texture, Color color, float2 top_left_uv, float2 bottom_right_uv) {
			UIDrawConfig config;
			SpriteRectangle(0, config, texture, color, top_left_uv, bottom_right_uv);
		}

		void UIDrawer::SpriteRectangle(
			size_t configuration,
			const UIDrawConfig& config,
			const wchar_t* texture,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);

					HandleBorder(configuration, config, position, scale);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
					HandleRectangleActions(configuration, config, position, scale);
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteRectangleDouble(
			const wchar_t* texture0,
			const wchar_t* texture1,
			Color color0,
			Color color1,
			float2 top_left_uv0,
			float2 bottom_right_uv0,
			float2 top_left_uv1,
			float2 bottom_right_uv1
		) {
			UIDrawConfig config;
			SpriteRectangleDouble(0, config, texture0, texture1, color0, color1, top_left_uv0, bottom_right_uv0, top_left_uv1, bottom_right_uv1);
		}

		void UIDrawer::SpriteRectangleDouble(
			size_t configuration,
			const UIDrawConfig& config,
			const wchar_t* texture0,
			const wchar_t* texture1,
			Color color0,
			Color color1,
			float2 top_left_uv0,
			float2 bottom_right_uv0,
			float2 top_left_uv1,
			float2 bottom_right_uv1
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					SpriteRectangle(configuration, position, scale, texture0, color0, top_left_uv0, bottom_right_uv0);
					SpriteRectangle(configuration, position, scale, texture1, color1, top_left_uv1, bottom_right_uv1);

					HandleBorder(configuration, config, position, scale);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
					HandleRectangleActions(configuration, config, position, scale);
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Sprite Button

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteButton(
			UIActionHandler clickable,
			const wchar_t* texture,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			UIDrawConfig config;
			SpriteButton(0, config, clickable, texture, color, top_left_uv, bottom_right_uv);
		}

		void UIDrawer::SpriteButton(
			size_t configuration,
			const UIDrawConfig& config,
			UIActionHandler clickable,
			const wchar_t* texture,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				bool is_active = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				HandleFitSpaceRectangle(configuration, position, scale);

				float2 background_position;
				float2 background_scale;
				Color background_color;
				if (configuration & UI_CONFIG_SPRITE_BUTTON_BACKGROUND) {
					const UIConfigSpriteButtonBackground* background = (const UIConfigSpriteButtonBackground*)config.GetParameter(UI_CONFIG_SPRITE_BUTTON_BACKGROUND);
					background_color = HandleColor(configuration, config);
					if (background->overwrite_color != ECS_COLOR_BLACK) {
						background_color = background->overwrite_color;
					}

					background_position = { position.x + (scale.x - background->scale.x) * 0.5f, position.y + (scale.y - background->scale.y) * 0.5f };
					background_scale = background->scale;

					if (configuration & UI_CONFIG_SPRITE_BUTTON_CENTER_SPRITE_TO_BACKGROUND) {
						background_position = position;
						position = AlignMiddle(background_position, background_scale, scale);
					}
				}

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					if (is_active) {
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);

						HandleBorder(configuration, config, position, scale);
						HandleLateAndSystemDrawActionNullify(configuration, position, scale);

						AddDefaultClickableHoverable(position, scale, clickable);
					}
					else {
						color.alpha *= color_theme.alpha_inactive_item;
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);
						HandleBorder(configuration, config, position, scale);
					}

					if (configuration & UI_CONFIG_SPRITE_BUTTON_BACKGROUND) {
						SolidColorRectangle(configuration, background_position, background_scale, background_color);
					}
				}

				FinalizeRectangle(configuration, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Sprite Texture Bool

		void UIDrawer::SpriteTextureBool(
			const wchar_t* texture_false,
			const wchar_t* texture_true,
			bool* state,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			UIDrawConfig config;
			SpriteTextureBool(0, config, texture_false, texture_true, state, color, top_left_uv, bottom_right_uv);
		}

		void UIDrawer::SpriteTextureBool(
			size_t configuration,
			const UIDrawConfig& config,
			const wchar_t* texture_false,
			const wchar_t* texture_true,
			bool* state,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				bool is_active = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				HandleFitSpaceRectangle(configuration, position, scale);

				float2 background_position;
				float2 background_scale;
				Color background_color;
				if (configuration & UI_CONFIG_SPRITE_BUTTON_BACKGROUND) {
					const UIConfigSpriteButtonBackground* background = (const UIConfigSpriteButtonBackground*)config.GetParameter(UI_CONFIG_SPRITE_BUTTON_BACKGROUND);
					background_color = HandleColor(configuration, config);
					if (background->overwrite_color != ECS_COLOR_BLACK) {
						background_color = background->overwrite_color;
					}

					background_position = { position.x + (scale.x - background->scale.x) * 0.5f, position.y + (scale.y - background->scale.y) * 0.5f };
					background_scale = background->scale;

					if (configuration & UI_CONFIG_SPRITE_BUTTON_CENTER_SPRITE_TO_BACKGROUND) {
						background_position = position;
						position = AlignMiddle(background_position, background_scale, scale);
					}
				}

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					const wchar_t* texture = *state ? texture_true : texture_false;
					if (is_active) {
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);

						HandleBorder(configuration, config, position, scale);
						HandleLateAndSystemDrawActionNullify(configuration, position, scale);

						AddDefaultClickableHoverable(position, scale, { BoolClickable, state, 0 });
					}
					else {
						color.alpha *= color_theme.alpha_inactive_item;
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);
						HandleBorder(configuration, config, position, scale);
					}

					if (configuration & UI_CONFIG_SPRITE_BUTTON_BACKGROUND) {
						SolidColorRectangle(configuration, background_position, background_scale, background_color);
					}
				}

				FinalizeRectangle(configuration, position, scale);
			}
		}

		void UIDrawer::SpriteTextureBool(
			const wchar_t* texture_false,
			const wchar_t* texture_true,
			size_t* flags,
			size_t flag_to_set,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			UIDrawConfig config;
			SpriteTextureBool(0, config, texture_false, texture_true, flags, flag_to_set, color, top_left_uv, bottom_right_uv);
		}

		void UIDrawer::SpriteTextureBool(
			size_t configuration,
			const UIDrawConfig& config,
			const wchar_t* texture_false,
			const wchar_t* texture_true,
			size_t* flags,
			size_t flag_to_set,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (ValidatePosition(configuration, position, scale)) {
					bool is_active = true;
					if (configuration & UI_CONFIG_ACTIVE_STATE) {
						const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
						is_active = active_state->state;
					}

					HandleFitSpaceRectangle(configuration, position, scale);

					float2 background_position;
					float2 background_scale;
					Color background_color;
					if (configuration & UI_CONFIG_SPRITE_BUTTON_BACKGROUND) {
						const UIConfigSpriteButtonBackground* background = (const UIConfigSpriteButtonBackground*)config.GetParameter(UI_CONFIG_SPRITE_BUTTON_BACKGROUND);
						background_color = HandleColor(configuration, config);
						if (background->overwrite_color != ECS_COLOR_BLACK) {
							background_color = background->overwrite_color;
						}

						background_position = { position.x + (scale.x - background->scale.x) * 0.5f, position.y + (scale.y - background->scale.y) * 0.5f };
						background_scale = background->scale;

						if (configuration & UI_CONFIG_SPRITE_BUTTON_CENTER_SPRITE_TO_BACKGROUND) {
							background_position = position;
							position = AlignMiddle(background_position, background_scale, scale);
						}
					}

					if (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}


					const wchar_t* texture = function::HasFlag(*flags, flag_to_set) ? texture_true : texture_false;
					if (is_active) {
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);

						HandleBorder(configuration, config, position, scale);
						HandleLateAndSystemDrawActionNullify(configuration, position, scale);

						UIChangeStateData change_state;
						change_state.state = flags;
						change_state.flag = flag_to_set;
						AddDefaultClickableHoverable(position, scale, { ChangeStateAction, &change_state, sizeof(change_state) });
					}
					else {
						color.alpha *= color_theme.alpha_inactive_item;
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);
						HandleBorder(configuration, config, position, scale);
					}

					if (configuration & UI_CONFIG_SPRITE_BUTTON_BACKGROUND) {
						SolidColorRectangle(configuration, background_position, background_scale, background_color);
					}
				}

				FinalizeRectangle(configuration, position, scale);
			}
		}

#pragma endregion

#pragma region Text Table

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::TextTable(const char* name, unsigned int rows, unsigned int columns, const char** labels) {
			UIDrawConfig config;
			TextTable(0, config, name, rows, columns, labels);
		}

		void UIDrawer::TextTable(size_t configuration, const UIDrawConfig& config, const char* name, unsigned int rows, unsigned int columns, const char** labels) {
			if (!initializer) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if (configuration & UI_CONFIG_DO_NOT_CACHE) {
					TextTableDrawer(configuration, config, position, scale, rows, columns, labels);
				}
				else {
					UIDrawerTextTable* data = (UIDrawerTextTable*)GetResource(name);
					TextTableDrawer(configuration, config, position, scale, data, rows, columns);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					TextTableInitializer(configuration, config, name, rows, columns, labels);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Text Input

		// ------------------------------------------------------------------------------------------------------------------------------------

		// single lined text input
		UIDrawerTextInput* UIDrawer::TextInput(const char* name, CapacityStream<char>* text_to_fill) {
			UIDrawConfig config;
			return TextInput(0, config, name, text_to_fill);
		}

		// single lined text input
		UIDrawerTextInput* UIDrawer::TextInput(size_t configuration, UIDrawConfig& config, const char* name, CapacityStream<char>* text_to_fill, UIDrawerTextInputFilter filter) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerTextInput* input = (UIDrawerTextInput*)GetResource(name);

					TextInputDrawer(configuration, config, input, position, scale, filter);
					HandleDynamicResource(configuration, name);
					return input;
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeTextInput initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_2(initialize_data, name, text_to_fill);
						InitializeDrawerElement(*this, &initialize_data, name, InitializeTextInputElement, DynamicConfiguration(configuration));
					}
					return TextInput(DynamicConfiguration(configuration), config, name, text_to_fill, filter);
				}
			}
			else {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					return TextInputInitializer(configuration, config, name, text_to_fill, position, scale);
				}
				else {
					return nullptr;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Text Label

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::TextLabel(const char* characters) {
			UIDrawConfig config;
			TextLabel(0, config, characters);
		}

		void UIDrawer::TextLabel(size_t configuration, const UIDrawConfig& config, const char* characters) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_NOT_CACHE) {
					const char* identifier = HandleResourceIdentifier(characters);
					if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
						TextLabelWithCull(configuration, config, identifier, position, scale);

						if (configuration & UI_CONFIG_GET_TRANSFORM) {
							UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
							*get_transform->position = position;
							*get_transform->scale = scale;
						}

						FinalizeRectangle(configuration, position, scale);
					}
					else {
						TextLabel(configuration, config, identifier, position, scale);
					}
				}
				else {
					UIDrawerTextElement* element = (UIDrawerTextElement*)GetResource(characters);
					HandleFitSpaceRectangle(configuration, position, scale);
					TextLabelDrawer(configuration, config, element, position, scale);
				}
			}
			else if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
				TextLabelInitializer(configuration, config, characters, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Text

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Text(const char* text) {
			UIDrawConfig config;
			Text(0, config, text);
		}

		// non cached drawer
		void UIDrawer::Text(size_t configuration, const UIDrawConfig& config, const char* characters, float2 position) {
			float2 text_span;

			size_t text_count = ParseStringIdentifier(characters, strlen(characters));
			Color color;
			float2 font_size;
			float character_spacing;
			HandleText(configuration, config, color, font_size, character_spacing);
			auto text_sprites = HandleTextSpriteBuffer(configuration);
			auto text_sprite_count = HandleTextSpriteCount(configuration);

			Stream<UISpriteVertex> vertices = Stream<UISpriteVertex>(text_sprites + *text_sprite_count, text_count * 6);
			TextAlignment horizontal_alignment, vertical_alignment;
			GetTextLabelAlignment(configuration, config, horizontal_alignment, vertical_alignment);

			if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				if (configuration & UI_CONFIG_VERTICAL) {
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
						Stream<UISpriteVertex> current_text = GetTextStream(configuration, text_count * 6);
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
						Stream<UISpriteVertex> current_text = GetTextStream(configuration, text_count * 6);
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
						text_span = GetTextSpan<true, true>(GetTextStream(configuration, text_count * 6));
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
						text_span = GetTextSpan<true>(GetTextStream(configuration, text_count * 6));
					}
				}
			}
			else {
				if (configuration & UI_CONFIG_VERTICAL) {
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
					Stream<UISpriteVertex> current_text = GetTextStream(configuration, text_count * 6);
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
					text_span = GetTextSpan<true>(GetTextStream(configuration, text_count * 6));
				}
			}
			size_t before_count = *text_sprite_count;
			*text_sprite_count += text_count * 6;

			if (~configuration & UI_CONFIG_DO_NOT_FIT_SPACE) {
				float2 copy = position;
				bool is_moved = HandleFitSpaceRectangle(configuration, position, text_span);
				if (is_moved) {
					float x_translation = position.x - copy.x;
					float y_translation = position.y - copy.y;
					for (size_t index = before_count; index < *text_sprite_count; index++) {
						text_sprites[index].position.x += x_translation;
						text_sprites[index].position.y -= y_translation;
					}
				}
			}

			if (configuration & UI_CONFIG_TEXT_ALIGN_TO_ROW_Y) {
				float y_position = AlignMiddle(position.y, current_row_y_scale, text_span.y);
				if (horizontal_alignment == TextAlignment::Right || vertical_alignment == TextAlignment::Bottom) {
					TranslateTextY(y_position, vertices.size - 6, vertices);
				}
				else {
					TranslateTextY(y_position, 0, vertices);
				}
			}

			FinalizeRectangle(configuration, position, text_span);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Text(size_t configuration, const UIDrawConfig& config, const char* text, float2& position, float2& text_span) {
			Color color;
			float character_spacing;
			float2 font_size;

			HandleText(configuration, config, color, font_size, character_spacing);
			if (configuration & UI_CONFIG_DO_NOT_CACHE) {
				if (!initializer) {
					float text_y_span = system->GetTextSpriteYScale(font_size.y);
					// Preemptively check to see if it actually needs to be drawn on the y axis by setting the horizontal 
					// draw to maximum
					if (ValidatePositionY(configuration, position, { 100.0f, text_y_span })) {
						size_t text_count = ParseStringIdentifier(text, strlen(text));

						auto text_stream = GetTextStream(configuration, text_count * 6);
						auto text_sprites = HandleTextSpriteBuffer(configuration);
						auto text_sprite_count = HandleTextSpriteCount(configuration);
						if (configuration & UI_CONFIG_VERTICAL) {
							system->ConvertCharactersToTextSprites<false>(
								text,
								position,
								text_sprites,
								text_count,
								color,
								*text_sprite_count,
								font_size,
								character_spacing
								);
							text_span = GetTextSpan<false>(text_stream);
						}
						else {
							system->ConvertCharactersToTextSprites(
								text,
								position,
								text_sprites,
								text_count,
								color,
								*text_sprite_count,
								font_size,
								character_spacing
							);
							text_span = GetTextSpan(text_stream);
						}
						*text_sprite_count += 6 * text_count;

						bool is_moved = HandleFitSpaceRectangle(configuration, position, text_span);
						if (is_moved) {
							TranslateText(position.x, position.y, text_stream);
						}

						if (configuration & UI_CONFIG_TEXT_ALIGN_TO_ROW_Y) {
							float y_position = AlignMiddle(position.y, current_row_y_scale, text_span.y);
							TranslateTextY(y_position, 0, text_stream);
						}

						if (!ValidatePosition(configuration, { text_stream[0].position.x, -text_stream[0].position.y }, text_span)) {
							*text_sprite_count -= 6 * text_count;
						}
					}
					// Still have to fill in the text span
					else {
						text_span = system->GetTextSpan(text, ParseStringIdentifier(text, strlen(text)), font_size.x, font_size.y, character_spacing);
						HandleFitSpaceRectangle(configuration, position, text_span);
					}
				}
			}
			else {
				UIDrawerTextElement* info = HandleTextCopyFromResource(configuration, text, position, color, ECS_TOOLS_UI_DRAWER_IDENTITY_SCALE);
				text_span = *info->TextScale();

				text_span.x *= zoom_ptr->x * info->GetInverseZoomX();
				text_span.y *= zoom_ptr->y * info->GetInverseZoomY();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Text(size_t configuration, const UIDrawConfig& config, const char* characters) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
					UIDrawerTextElement* resource = (UIDrawerTextElement*)GetResource(characters);
					Text(configuration, config, resource, position);
				}
				else {
					Text(configuration, config, characters, position);
				}
			}
			else if (~configuration & UI_CONFIG_DO_NOT_CACHE) {
				TextInitializer(configuration, config, characters, position);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::TextToolTip(const char* characters, float2 position, float2 scale, const UITooltipBaseData* base) {
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DefaultHoverableWithToolTip(const char* characters, float2 position, float2 scale, const Color* color, const float* percentage, const UITooltipBaseData* base) {
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

			AddHoverable(position, scale, { DefaultHoverableWithToolTipAction, &tool_tip_data, sizeof(tool_tip_data), UIDrawPhase::System });
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DefaultHoverableWithToolTip(float2 position, float2 scale, const UIDefaultHoverableWithTooltipData* data) {
			AddHoverable(position, scale, { DefaultHoverableWithToolTipAction, (void*)data, sizeof(*data), UIDrawPhase::System });
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Vertex Color Rectangle

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorRectangle(
			size_t configuration,
			const UIDrawConfig& config,
			Color top_left,
			Color top_right,
			Color bottom_left,
			Color bottom_right
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					auto buffer = HandleSolidColorBuffer(configuration);
					auto count = HandleSolidColorCount(configuration);

					SetVertexColorRectangle(position, scale, top_left, top_right, bottom_left, bottom_right, buffer, count);

					HandleBorder(configuration, config, position, scale);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
					HandleRectangleActions(configuration, config, position, scale);
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorRectangle(
			size_t configuration,
			const UIDrawConfig& config,
			ColorFloat top_left,
			ColorFloat top_right,
			ColorFloat bottom_left,
			ColorFloat bottom_right
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					auto buffer = HandleSolidColorBuffer(configuration);
					auto count = HandleSolidColorCount(configuration);

					SetVertexColorRectangle(position, scale, Color(top_left), Color(top_right), Color(bottom_left), Color(bottom_right), buffer, count);
					HandleBorder(configuration, config, position, scale);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
					HandleRectangleActions(configuration, config, position, scale);
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteRectangle(
			size_t configuration,
			const UIDrawConfig& config,
			const wchar_t* texture,
			const Color* colors,
			const float2* uvs,
			UIDrawPhase phase
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					VertexColorSpriteRectangle(position, scale, texture, colors, uvs, phase);

					HandleBorder(configuration, config, position, scale);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
					HandleRectangleActions(configuration, config, position, scale);
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteRectangle(
			size_t configuration,
			const UIDrawConfig& config,
			const wchar_t* texture,
			const Color* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			UIDrawPhase phase
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					VertexColorSpriteRectangle(position, scale, texture, colors, top_left_uv, bottom_right_uv, phase);

					HandleBorder(configuration, config, position, scale);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
					HandleRectangleActions(configuration, config, position, scale);
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteRectangle(
			size_t configuration,
			const UIDrawConfig& config,
			const wchar_t* texture,
			const ColorFloat* colors,
			const float2* uvs,
			UIDrawPhase phase
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					VertexColorSpriteRectangle(position, scale, texture, colors, uvs, phase);

					HandleBorder(configuration, config, position, scale);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
					HandleRectangleActions(configuration, config, position, scale);
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteRectangle(
			size_t configuration,
			const UIDrawConfig& config,
			const wchar_t* texture,
			const ColorFloat* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			UIDrawPhase phase
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					VertexColorSpriteRectangle(position, scale, texture, colors, top_left_uv, bottom_right_uv, phase);

					HandleBorder(configuration, config, position, scale);

					HandleLateAndSystemDrawActionNullify(configuration, position, scale);
					HandleRectangleActions(configuration, config, position, scale);
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// --------------------------------------------------------------------------------------------------------------

		// Last initialized element allocations and table resources must be manually deallocated after finishing using it
		UIDrawer InitializeInitializerDrawer(UIDrawer& drawer_to_copy) {
			UIDrawerDescriptor descriptor = drawer_to_copy.system->GetDrawerDescriptor(drawer_to_copy.window_index);
			descriptor.do_not_initialize_viewport_sliders = true;
			descriptor.do_not_allocate_buffers = true;

			UIDrawer drawer = UIDrawer(
				descriptor,
				drawer_to_copy.window_data,
				true
			);

			drawer.identifier_stack = drawer_to_copy.identifier_stack;
			drawer.current_identifier = drawer_to_copy.current_identifier;
			drawer.last_initialized_element_allocations.Initialize(drawer.system->m_memory, 0, ECS_TOOLS_UI_MISC_DRAWER_ELEMENT_ALLOCATIONS);
			drawer.last_initialized_element_table_resources.Initialize(drawer.system->m_memory, 0, ECS_TOOLS_UI_MISC_DRAWER_ELEMENT_ALLOCATIONS);

			return drawer;
		}

		// --------------------------------------------------------------------------------------------------------------

		// The identifier stack and the current identifier are used to initialize the identifier stack of the initializing drawer
		// These are mostly used by the internal initializing functions inside the UIDrawer
		void InitializeDrawerElement(
			UIDrawer& drawer_to_copy,
			void* additional_data,
			const char* name,
			UIDrawerInitializeFunction initialize,
			size_t configuration
		) {
			UIDrawer drawer = InitializeInitializerDrawer(drawer_to_copy);

			const char* identifier = drawer.HandleResourceIdentifier(name);
			initialize(drawer.window_data, additional_data, &drawer, configuration);
			drawer.system->AddWindowDrawerElement(drawer.window_index, identifier, drawer.last_initialized_element_allocations, drawer.last_initialized_element_table_resources);

			// The last element initializations buffers must be manually deallocated
			drawer.system->m_memory->Deallocate(drawer.last_initialized_element_allocations.buffer);
			drawer.system->m_memory->Deallocate(drawer.last_initialized_element_table_resources.buffer);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeComboBoxElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeComboBox* data = (UIDrawerInitializeComboBox*)additional_data;
			drawer_ptr->ComboBox(configuration, *data->config, data->name, data->labels, data->label_display_count, data->active_label);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeColorInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeColorInput* data = (UIDrawerInitializeColorInput*)additional_data;
			drawer_ptr->ColorInput(configuration, *data->config, data->name, data->color, data->default_color);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeFilterMenuElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeFilterMenu* data = (UIDrawerInitializeFilterMenu*)additional_data;
			drawer_ptr->FilterMenu(configuration, *data->config, data->name, data->label_names, data->states);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeFilterMenuSinglePointerElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeFilterMenuSinglePointer* data = (UIDrawerInitializeFilterMenuSinglePointer*)additional_data;
			drawer_ptr->FilterMenu(configuration, *data->config, data->name, data->label_names, data->states);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeHierarchyElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeHierarchy* data = (UIDrawerInitializeHierarchy*)additional_data;
			drawer_ptr->Hierarchy(configuration, *data->config, data->name);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeLabelHierarchyElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeLabelHierarchy* data = (UIDrawerInitializeLabelHierarchy*)additional_data;
			drawer_ptr->LabelHierarchy(configuration, *data->config, data->identifier, data->labels);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeListElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeList* data = (UIDrawerInitializeList*)additional_data;
			drawer_ptr->List(configuration, *data->config, data->name);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeMenuElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeMenu* data = (UIDrawerInitializeMenu*)additional_data;
			drawer_ptr->Menu(configuration, *data->config, data->name, data->state);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeFloatInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeFloatInput* data = (UIDrawerInitializeFloatInput*)additional_data;
			drawer_ptr->FloatInput(configuration, *data->config, data->name, data->value, data->default_value, data->lower_bound, data->upper_bound);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void InitializeIntegerInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeIntegerInput<Integer>* data = (UIDrawerInitializeIntegerInput<Integer>*)additional_data;
			drawer_ptr->IntInput(configuration, *data->config, data->name, data->value, data->default_value, data->min, data->max);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, InitializeIntegerInputElement<integer>, void*, void*, UIDrawer*, size_t);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// --------------------------------------------------------------------------------------------------------------

		void InitializeDoubleInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeDoubleInput* data = (UIDrawerInitializeDoubleInput*)additional_data;
			drawer_ptr->DoubleInput(configuration, *data->config, data->name, data->value, data->default_value, data->lower_bound, data->upper_bound);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeFloatInputGroupElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeFloatInputGroup* data = (UIDrawerInitializeFloatInputGroup*)additional_data;
			drawer_ptr->FloatInputGroup(configuration, *data->config, data->count, data->group_name, data->names, data->values, data->default_values, data->lower_bound, data->upper_bound);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void InitializeIntegerInputGroupElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeIntegerInputGroup<Integer>* data = (UIDrawerInitializeIntegerInputGroup<Integer>*)additional_data;
			drawer_ptr->IntInputGroup(configuration, *data->config, data->count, data->group_name, data->names, data->values, data->default_values, data->lower_bound, data->upper_bound);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, InitializeIntegerInputGroupElement<integer>, void*, void*, UIDrawer*, size_t);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// --------------------------------------------------------------------------------------------------------------

		void InitializeDoubleInputGroupElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeDoubleInputGroup* data = (UIDrawerInitializeDoubleInputGroup*)additional_data;
			drawer_ptr->DoubleInputGroup(configuration, *data->config, data->count, data->group_name, data->names, data->values, data->default_values, data->lower_bound, data->upper_bound);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeSliderElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeSlider* data = (UIDrawerInitializeSlider*)additional_data;
			drawer_ptr->Slider(
				configuration,
				*data->config,
				data->name,
				data->byte_size,
				data->value_to_modify,
				data->lower_bound,
				data->upper_bound,
				data->default_value,
				*data->functions,
				data->filter
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeSliderGroupElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeSliderGroup* data = (UIDrawerInitializeSliderGroup*)additional_data;
			drawer_ptr->SliderGroup(
				configuration,
				*data->config,
				data->count,
				data->group_name,
				data->names,
				data->byte_size,
				data->values_to_modify,
				data->lower_bounds,
				data->upper_bounds,
				data->default_values,
				*data->functions,
				data->filter
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeStateTableElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeStateTable* data = (UIDrawerInitializeStateTable*)additional_data;
			drawer_ptr->StateTable(configuration, *data->config, data->name, data->labels, data->states);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeStateTableSinglePointerElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeStateTableSinglePointer* data = (UIDrawerInitializeStateTableSinglePointer*)additional_data;
			drawer_ptr->StateTable(configuration, *data->config, data->name, data->labels, data->states);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeTextInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeTextInput* data = (UIDrawerInitializeTextInput*)additional_data;
			drawer_ptr->TextInput(configuration, *data->config, data->name, data->text_to_fill);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeColorFloatInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeColorFloatInput* data = (UIDrawerInitializeColorFloatInput*)additional_data;
			drawer_ptr->ColorFloatInput(configuration, *data->config, data->name, data->color, data->default_color);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeArrayElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeArrayElementData* data = (UIDrawerInitializeArrayElementData*)additional_data;
			drawer_ptr->ArrayInitializer(configuration, *data->config, data->name, data->element_count);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayFloatFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.FloatInput(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_NO_RANGE, temp_config, element_name, (float*)draw_data.element_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayDoubleFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.DoubleInput(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_NO_RANGE, temp_config, element_name, (double*)draw_data.element_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawerArrayIntegerFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.IntInput<Integer>(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_NO_RANGE, temp_config, element_name, (Integer*)draw_data.element_data);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawerArrayIntegerFunction<integer>, UIDrawer&, const char*, UIDrawerArrayDrawData);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayFloat2Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			float* values[2];
			values[0] = (float*)draw_data.element_data;
			values[1] = values[0] + 1;

			drawer.FloatInputGroup(
				UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_NO_RANGE,
				temp_config,
				2,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayDouble2Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			double* values[2];
			values[0] = (double*)draw_data.element_data;
			values[1] = values[0] + 1;

			drawer.DoubleInputGroup(
				UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_NO_RANGE,
				temp_config,
				2,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		void UIDrawerArrayInteger2Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			BaseInteger* values[2];
			values[0] = (BaseInteger*)draw_data.element_data;
			values[1] = values[0] + 1;

			drawer.IntInputGroup(
				UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_NO_RANGE,
				temp_config,
				2,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawerArrayInteger2Function<integer>, UIDrawer&, const char*, UIDrawerArrayDrawData);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayFloat3Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			float* values[3];
			values[0] = (float*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;

			drawer.FloatInputGroup(
				UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_NO_RANGE,
				temp_config,
				3,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayDouble3Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			double* values[3];
			values[0] = (double*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;

			drawer.DoubleInputGroup(
				UI_CONFIG_NUMBER_INPUT_NO_RANGE,
				temp_config,
				3,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		void UIDrawerArrayInteger3Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			BaseInteger* values[3];
			values[0] = (BaseInteger*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;

			drawer.IntInputGroup(
				UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_NO_RANGE,
				temp_config,
				3,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawerArrayInteger3Function<integer>, UIDrawer&, const char*, UIDrawerArrayDrawData);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayFloat4Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			float* values[4];
			values[0] = (float*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;
			values[3] = values[0] + 3;

			drawer.FloatInputGroup(
				UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_NO_RANGE,
				temp_config,
				4,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayDouble4Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;

			double* values[4];
			values[0] = (double*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;
			values[3] = values[0] + 3;

			drawer.DoubleInputGroup(
				UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_NO_RANGE,
				temp_config,
				4,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		void UIDrawerArrayInteger4Function(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;

			BaseInteger* values[4];
			values[0] = (BaseInteger*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;
			values[3] = values[0] + 3;

			drawer.IntInputGroup(
				UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_NUMBER_INPUT_NO_RANGE,
				temp_config,
				4,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawerArrayInteger4Function<integer>, UIDrawer&, const char*, UIDrawerArrayDrawData);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayColorFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.ColorInput(UI_CONFIG_DO_NOT_CACHE, temp_config, element_name, (Color*)draw_data.element_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayColorFloatFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.ColorFloatInput(UI_CONFIG_DO_NOT_CACHE, temp_config, element_name, (ColorFloat*)draw_data.element_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayCheckBoxFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.CheckBox(UI_CONFIG_DO_NOT_CACHE, temp_config, element_name, (bool*)draw_data.element_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayTextInputFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			UIDrawerTextInput** inputs = (UIDrawerTextInput**)draw_data.additional_data;

			inputs[draw_data.current_index] = drawer.TextInput(UI_CONFIG_DO_NOT_CACHE, temp_config, element_name, (CapacityStream<char>*)draw_data.element_data);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayComboBoxFunction(UIDrawer& drawer, const char* element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			CapacityStream<Stream<const char*>>* flag_labels = (CapacityStream<Stream<const char*>>*)draw_data.additional_data;

			drawer.ComboBox(
				UI_CONFIG_DO_NOT_CACHE,
				temp_config,
				element_name,
				flag_labels->buffer[draw_data.current_index],
				flag_labels->buffer[draw_data.current_index].size,
				(unsigned char*)draw_data.element_data
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		// --------------------------------------------------------------------------------------------------------------

	}

}