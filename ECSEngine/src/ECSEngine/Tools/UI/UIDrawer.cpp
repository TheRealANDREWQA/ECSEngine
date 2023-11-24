#include "ecspch.h"
#include "UIDrawer.h"
#include "UISystem.h"
#include "UIDrawConfigs.h"
#include "UIDrawerStructures.h"
#include "UIResourcePaths.h"
#include "../../Utilities/Path.h"
#include "../../Utilities/StreamUtilities.h"

#define ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config) float2 position; float2 scale; HandleTransformFlags(configuration, config, position, scale)

#define ECS_TOOLS_UI_DRAWER_IDENTITY_SCALE [](float2 scale, const UIDrawer& drawer) {return scale;}
#define ECS_TOOLS_UI_DRAWER_LABEL_SCALE [](float2 scale, const UIDrawer& drawer){return float2(scale.x + 2.0f * drawer.element_descriptor.label_padd.x, scale.y + 2.0f * drawer.element_descriptor.label_padd.y);}

namespace ECSEngine {

	namespace Tools {

		Stream<char> UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS[] = {
			"x",
			"y",
			"z",
			"w",
			"t",
			"u",
			"v"
		};

		const size_t SLIDER_GROUP_MAX_COUNT = 8;
		const float NUMBER_INPUT_DRAG_FACTOR = 200.0f;

		// ------------------------------------------------------------------------------------------------------------------------------------

		ECS_INLINE bool IsElementNameFirst(size_t configuration, size_t no_name_flag) {
			return ((configuration & UI_CONFIG_ELEMENT_NAME_FIRST) != 0) && ((configuration & no_name_flag) == 0);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		ECS_INLINE bool IsElementNameAfter(size_t configuration, size_t no_name_flag) {
			return ((configuration & UI_CONFIG_ELEMENT_NAME_FIRST) == 0) && ((configuration & no_name_flag) == 0);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		ECS_INLINE void AlignToRowY(UIDrawer* drawer, size_t configuration, float2& position, float2 scale) {
			if (configuration & UI_CONFIG_ALIGN_TO_ROW_Y) {
				position.y = AlignMiddle(position.y, drawer->current_row_y_scale, scale.y);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		ECS_INLINE bool IsLateOrSystemConfiguration(size_t configuration) {
			return HasFlag(configuration, UI_CONFIG_LATE_DRAW) || HasFlag(configuration, UI_CONFIG_SYSTEM_DRAW);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// Add to the late_hoverable, late_clickable or late_generals the last action in order to be added later on
		static void AddLateOrSystemAction(UIDrawer* drawer, size_t configuration, bool hoverable, bool general, ECS_MOUSE_BUTTON click_button) {
			if (IsLateOrSystemConfiguration(configuration)) {
				if (hoverable) {
					drawer->late_hoverables.Add(drawer->system->GetLastHoverableIndex(drawer->dockspace, drawer->border_index));
				}
				if (click_button != ECS_MOUSE_BUTTON_COUNT) {
					drawer->late_clickables[click_button].Add(drawer->system->GetLastClickableIndex(drawer->dockspace, drawer->border_index, click_button));
				}
				if (general) {
					drawer->late_generals.Add(drawer->system->GetLastGeneralIndex(drawer->dockspace, drawer->border_index));
				}
			}
		}
		
		ECS_INLINE static AllocatorPolymorphic TemporaryAllocator(const UIDrawer* drawer, ECS_UI_DRAW_PHASE phase) {
			return drawer->record_snapshot_runnables ? drawer->SnapshotRunnableAllocator() : 
				GetAllocatorPolymorphic(drawer->system->TemporaryAllocator(phase));
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename BasicType>
		static void InputGroupInitializerImplementation(
			UIDrawer* drawer,
			size_t configuration,
			UIDrawConfig& config,
			Stream<char> group_name,
			size_t count,
			Stream<char>* names,
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

			if (configuration & UI_CONFIG_DYNAMIC_RESOURCE) {
				// The identifier must be stable
				Stream<char> identifier = drawer->HandleResourceIdentifier(group_name, true);
				// Save a resource to the window for dynamic type resources such that they recognize that they are allocated
				drawer->AddWindowResourceToTable(nullptr, identifier);
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
					IntegerRange(lower_bound, upper_bound);
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
				initializer_configuration |= (~configuration & UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES) ? UI_CONFIG_ELEMENT_NAME_FIRST : UI_CONFIG_TEXT_INPUT_NO_NAME;
				initializer_configuration = ClearFlag(initializer_configuration, UI_CONFIG_NAME_PADDING);

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
		}

		template<typename BasicType>
		static void InputGroupDrawerImplementation(
			UIDrawer* drawer,
			size_t configuration,
			UIDrawConfig& config,
			Stream<char> group_name,
			size_t count,
			Stream<char>* names,
			BasicType** values,
			const BasicType* lower_bounds,
			const BasicType* upper_bounds,
			float2 position,
			float2 scale
		) {
			float2 initial_position = position;

			bool is_name_first = IsElementNameFirst(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME);
			bool is_name_after = IsElementNameAfter(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_NAME);

			if (is_name_first) {
				drawer->ElementName(configuration, config, group_name, position, scale);
			}

			bool has_pushed_stack = drawer->PushIdentifierStackStringPattern();
			drawer->PushIdentifierStack(group_name);

			if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				// Account for the name
				if (is_name_first) {
					scale.x -= position.x - initial_position.x;
				}
				else if (is_name_after) {
					scale.x -= drawer->ElementNameSize(configuration, config, group_name, scale);
				}
				scale.x -= drawer->layout.element_indentation * 0.25f * (count - 1);
				scale.x /= count;
			}

			// Reduce the indentation and the padding for the names of the components
			float indentation = drawer->layout.element_indentation;
			float label_padding = drawer->element_descriptor.label_padd.x;

			drawer->layout.element_indentation *= 0.25f;
			drawer->element_descriptor.label_padd.x *= 0.5f;

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
					IntegerRange(lower_bound, upper_bound);
				}

				size_t drawer_configuration = configuration | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
					| (HasFlag(configuration, UI_CONFIG_NUMBER_INPUT_GROUP_NO_SUBNAMES) ? UI_CONFIG_TEXT_INPUT_NO_NAME : UI_CONFIG_ELEMENT_NAME_FIRST);
				drawer_configuration = ClearFlag(drawer_configuration, UI_CONFIG_NAME_PADDING);

				if (configuration & UI_CONFIG_NUMBER_INPUT_GROUP_UNIFORM_BOUNDS) {
					lower_bound = lower_bounds[0];
					upper_bound = upper_bounds[0];
					drawer_configuration |= UI_CONFIG_NUMBER_INPUT_RANGE;
				}
				else {
					if (lower_bounds != nullptr) {
						lower_bound = lower_bounds[index];
						drawer_configuration |= UI_CONFIG_NUMBER_INPUT_RANGE;
					}
					if (upper_bounds != nullptr) {
						upper_bound = upper_bounds[index];
						drawer_configuration |= UI_CONFIG_NUMBER_INPUT_RANGE;
					}

					if (lower_bounds == nullptr || upper_bounds == nullptr) {
						drawer_configuration = ClearFlag(drawer_configuration, UI_CONFIG_NUMBER_INPUT_RANGE);
					}
				}
				
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

			// Restore the previous values
			drawer->layout.element_indentation = indentation;
			drawer->element_descriptor.label_padd.x = label_padding;

			if (is_name_after) {
				// Indent the name by a full indentation, the slider before it only indented at 25%
				drawer->Indent(0.75f);
				drawer->ElementName(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, group_name, position, scale);
			}

			drawer->HandleDrawMode(configuration);
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
			draw_mode = ECS_UI_DRAWER_INDENT;
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

		void UIDrawer::ConvertTextToWindowResource(size_t configuration, const UIDrawConfig& config, Stream<char> text, UIDrawerTextElement* element, float2 position, float2 scale) {
			size_t text_count = ParseStringIdentifier(text);
			Color color;
			float character_spacing;
			float2 font_size;
			HandleText(configuration, config, color, font_size, character_spacing);

			/*if (scale.y != 0.0f) {
				float old_scale = font_size.y;
				font_size.y = system->GetTextSpriteSizeToScale(scale.y - element_descriptor.label_padd.y * 2);
				font_size.x = font_size.y * ECS_TOOLS_UI_FONT_X_FACTOR;
				float factor = font_size.y / old_scale;
				character_spacing *= factor;
			}*/

			ECS_UI_ALIGN horizontal_alignment = ECS_UI_ALIGN_MIDDLE, vertical_alignment = ECS_UI_ALIGN_MIDDLE;
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
					bool invert_order = ((configuration & UI_CONFIG_TEXT_ALIGNMENT) != 0) && vertical_alignment == ECS_UI_ALIGN_BOTTOM;

					system->ConvertCharactersToTextSprites(
						{ text.buffer, text_count },
						position,
						text_stream->buffer,
						color,
						0,
						font_size,
						character_spacing,
						false,
						invert_order
					);
					text_span = GetTextSpan(*text_stream, false, invert_order);
					AlignVerticalText(*text_stream);
				}
				else {
					bool invert_order = ((configuration & UI_CONFIG_TEXT_ALIGNMENT) != 0) && horizontal_alignment == ECS_UI_ALIGN_RIGHT;
					system->ConvertCharactersToTextSprites(
						{ text.buffer, text_count },
						position,
						text_stream->buffer,
						color,
						0,
						font_size,
						character_spacing,
						true,
						invert_order
					);
					text_span = GetTextSpan(*text_stream, true, invert_order);
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

		bool UIDrawer::ValidatePosition(size_t configuration, float2 position) const {
			if (~configuration & UI_CONFIG_DO_NOT_VALIDATE_POSITION) {
				return (position.x < max_region_render_limit.x) && (position.y < max_region_render_limit.y);
			}
			else {
				return true;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::ValidatePosition(size_t configuration, float2 position, float2 scale) const {
			return ValidatePosition(configuration, position) && ValidatePositionMinBound(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::ValidatePositionY(size_t configuration, float2 position, float2 scale) const {
			if (~configuration & UI_CONFIG_DO_NOT_VALIDATE_POSITION) {
				return (position.y < max_region_render_limit.y) && (position.y + scale.y >= min_region_render_limit.y);
			}
			else {
				return true;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::ValidatePositionMinBound(size_t configuration, float2 position, float2 scale) const {
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
			if (record_actions && ((configuration & UI_CONFIG_LATE_DRAW) || (configuration & UI_CONFIG_SYSTEM_DRAW))) {
				AddHoverable(0, position, scale, { SkipAction, nullptr, 0 });
				AddClickable(0, position, scale, { SkipAction, nullptr, 0 });
				AddGeneral(0, position, scale, { SkipAction, nullptr, 0 });
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		ECS_UI_DRAW_PHASE UIDrawer::HandlePhase(size_t configuration) const {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				return ECS_UI_DRAW_LATE;
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				return ECS_UI_DRAW_SYSTEM;
			}
			else {
				return ECS_UI_DRAW_NORMAL;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleTransformFlags(size_t configuration, const UIDrawConfig& config, float2& position, float2& scale) const {
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
				ECS_UI_WINDOW_DEPENDENT_SIZE type = *(ECS_UI_WINDOW_DEPENDENT_SIZE*)transform;

				position.x = current_x + *(transform + 1);
				position.y = current_y + *(transform + 2);

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
					color = color_theme.text;
				}
				else {
					color = color_theme.unavailable_text;
				}
				character_spacing = font.character_spacing;
			}
			font_size.y *= zoom_ptr->y;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleTextStreamColorUpdate(Color color, Stream<UISpriteVertex> vertices) const {
			if (color != vertices[0].color) {
				for (size_t index = 0; index < vertices.size; index++) {
					vertices[index].color = color;
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleTextStreamColorUpdate(Color color, CapacityStream<UISpriteVertex> vertices) const {
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
			ECS_UI_ALIGN& horizontal_alignment,
			ECS_UI_ALIGN& vertical_alignment
		) const {
			if (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
				const float* params = (const float*)config.GetParameter(UI_CONFIG_TEXT_ALIGNMENT);
				const ECS_UI_ALIGN* alignments = (ECS_UI_ALIGN*)params;
				horizontal_alignment = *alignments;
				vertical_alignment = *(alignments + 1);

				switch (horizontal_alignment) {
				case ECS_UI_ALIGN_LEFT:
					x_position = label_position.x + element_descriptor.label_padd.x;
					break;
				case ECS_UI_ALIGN_RIGHT:
					x_position = label_position.x + label_size.x - element_descriptor.label_padd.x - text_span.x;
					break;
				case ECS_UI_ALIGN_MIDDLE:
					x_position = AlignMiddle(label_position.x, label_size.x, text_span.x);
					break;
				}

				switch (vertical_alignment) {
				case ECS_UI_ALIGN_TOP:
					y_position = label_position.y + element_descriptor.label_padd.y;
					break;
				case ECS_UI_ALIGN_BOTTOM:
					y_position = label_position.y + label_size.y - element_descriptor.label_padd.y - text_span.y;
					break;
				case ECS_UI_ALIGN_MIDDLE:
					y_position = AlignMiddle(label_position.y, label_size.y, text_span.y);
					break;
				}
			}
			else {
				x_position = AlignMiddle(label_position.x, label_size.x, text_span.x);
				horizontal_alignment = ECS_UI_ALIGN_MIDDLE;
				y_position = AlignMiddle(label_position.y, label_size.y, text_span.y);
				vertical_alignment = ECS_UI_ALIGN_MIDDLE;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleTextLabelAlignment(
			float2 text_span,
			float2 label_scale,
			float2 label_position,
			float& x_position,
			float& y_position,
			ECS_UI_ALIGN horizontal,
			ECS_UI_ALIGN vertical
		) const {
			switch (horizontal) {
			case ECS_UI_ALIGN_LEFT:
				x_position = label_position.x + element_descriptor.label_padd.x;
				break;
			case ECS_UI_ALIGN_RIGHT:
				x_position = label_position.x + label_scale.x - element_descriptor.label_padd.y - text_span.x;
				break;
			case ECS_UI_ALIGN_MIDDLE:
				x_position = AlignMiddle(label_position.x, label_scale.x, text_span.x);
				break;
			}

			switch (vertical) {
			case ECS_UI_ALIGN_TOP:
				y_position = label_position.y + element_descriptor.label_padd.y;
				break;
			case ECS_UI_ALIGN_BOTTOM:
				y_position = label_position.y + label_scale.y - element_descriptor.label_padd.y - text_span.y;
				break;
			case ECS_UI_ALIGN_MIDDLE:
				y_position = AlignMiddle(label_position.y, label_scale.y, text_span.y);
				break;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleDynamicResource(size_t configuration, Stream<char> name) {
			if (configuration & UI_CONFIG_DYNAMIC_RESOURCE) {
				Stream<char> identifier = HandleResourceIdentifier(name);
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
				if (parameters->draw_phase == ECS_UI_DRAW_SYSTEM) {
					phase_buffers = system_buffers;
					phase_counts = system_counts;
				}
				else if (parameters->draw_phase == ECS_UI_DRAW_NORMAL) {
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

		// The functor returns the string to copy
		template<typename Functor>
		Stream<char> HandleResourceIdentifierImpl(UIDrawer* drawer, Stream<char> input, bool permanent_buffer, Functor&& functor) {
			if (!permanent_buffer) {
				if (drawer->identifier_stack.size > 0) {
					Stream<char> string_to_copy = functor();

					char* temp_memory = (char*)drawer->GetTempBuffer((input.size + string_to_copy.size) * sizeof(char), ECS_UI_DRAW_NORMAL, alignof(char));
					input.CopyTo(temp_memory);
					memcpy(temp_memory + input.size, string_to_copy.buffer, string_to_copy.size);
					return { temp_memory, input.size + string_to_copy.size };
				}
				else {
					return input;
				}
			}
			else {
				Stream<char> string_to_copy = functor();

				char* memory = (char*)drawer->GetMainAllocatorBuffer((input.size + string_to_copy.size) * sizeof(char), alignof(char));
				input.CopyTo(memory);
				if (drawer->identifier_stack.size > 0) {
					memcpy(memory + input.size, string_to_copy.buffer, string_to_copy.size * sizeof(char));
				}
				return { memory, input.size + string_to_copy.size };
			}
		}
		
		Stream<char> UIDrawer::HandleResourceIdentifier(Stream<char> input, bool permanent_buffer) {
			return HandleResourceIdentifierImpl(this, input, permanent_buffer, [=]() {
				return current_identifier;
			});
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		Stream<char> UIDrawer::HandleResourceIdentifierEx(Stream<char> input, bool permanent_buffer)
		{
			return HandleResourceIdentifierImpl(this, input, permanent_buffer, [=]() {
				Stream<char> string_to_copy = current_identifier;
				Stream<char> string_pattern = FindFirstToken(current_identifier, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
				if (string_pattern.buffer != nullptr) {
					if (FindFirstToken(input, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT).buffer != nullptr) {
						string_to_copy = { string_pattern.buffer + ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT, string_pattern.size - ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT };
					}
				}
				return string_to_copy;
			});
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleRectangleActions(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale) {
			if (configuration & UI_CONFIG_RECTANGLE_HOVERABLE_ACTION) {
				const UIActionHandler* handler = (const UIActionHandler*)config.GetParameter(UI_CONFIG_RECTANGLE_HOVERABLE_ACTION);
				AddHoverable(configuration, position, scale, *handler);
			}

			if (configuration & UI_CONFIG_RECTANGLE_CLICKABLE_ACTION) {
				const UIActionHandler* handler = (const UIActionHandler*)config.GetParameter(UI_CONFIG_RECTANGLE_CLICKABLE_ACTION);
				AddClickable(configuration, position, scale, *handler);
			}

			if (configuration & UI_CONFIG_RECTANGLE_GENERAL_ACTION) {
				const UIActionHandler* handler = (const UIActionHandler*)config.GetParameter(UI_CONFIG_RECTANGLE_GENERAL_ACTION);
				AddGeneral(configuration, position, scale, *handler);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextElement* UIDrawer::HandleTextCopyFromResource(size_t configuration, Stream<char> text, float2& position, Color font_color, float2 (*scale_from_text)(float2 scale, const UIDrawer& drawer)) {
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

			if (configuration & UI_CONFIG_ALIGN_TO_ROW_Y) {
				float y_position = AlignMiddle(position.y, current_row_y_scale, element->TextScale()->y);
				TranslateTextY(y_position, 0, *element->TextStream());
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
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// cull_mode 0 - horizontal
		// cull_mode 1 - vertical
		// cull_mode 2 - horizontal main, vertical secondary
		// cull_mode 3 - vertical main, horizontal secondary
		UIDrawerTextElement* UIDrawer::HandleLabelTextCopyFromResourceWithCull(
			size_t configuration,
			const UIDrawConfig& config,
			Stream<char> text,
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
				if (info->TextScale()->x > scale.x - 2 * element_descriptor.label_padd.x) {
					memcpy_all = false;
				}
			}
			else if (cull_mode == 1) {
				if (info->TextScale()->y > scale.y - 2 * element_descriptor.label_padd.y) {
					memcpy_all = false;
				}
			}
			else if (cull_mode == 2 || cull_mode == 3) {
				if ((info->TextScale()->x > scale.x - 2 * element_descriptor.label_padd.x) || (info->TextScale()->y > scale.y - 2 * element_descriptor.label_padd.y)) {
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
			ECS_UI_ALIGN horizontal_alignment, vertical_alignment;
			GetTextLabelAlignment(configuration, config, horizontal_alignment, vertical_alignment);

			auto memcpy_fnc = [&](unsigned int first_index, unsigned int second_index, bool vertical) {
				float x_position, y_position;
				ECS_UI_ALIGN dummy1, dummy2;
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
				if (horizontal_alignment == ECS_UI_ALIGN_RIGHT) {
					if (!memcpy_all) {
						CullTextSprites<1, 1>(text_stream, current_stream, text_stream[1].position.x - scale.x + 2 * element_descriptor.label_padd.x);
						text_span = GetTextSpan(current_stream, true, true);

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
						CullTextSprites<0, 1>(text_stream, current_stream, text_stream[0].position.x + scale.x - 2 * element_descriptor.label_padd.x);
						text_span = GetTextSpan(current_stream, true, false);

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
				if (vertical_alignment == ECS_UI_ALIGN_BOTTOM) {
					if (!memcpy_all) {
						CullTextSprites<3, 1>(text_stream, current_stream, text_stream[text_stream.size - 3].position.y - scale.y + 2 * element_descriptor.label_padd.y);
						text_span = GetTextSpan(current_stream, false, true);

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
						CullTextSprites<2, 1>(text_stream, current_stream, text_stream[0].position.y - scale.y + 2 * element_descriptor.label_padd.y);
						text_span = GetTextSpan(current_stream, false, false);

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
				if (info->TextScale()->y > scale.y - 2 * element_descriptor.label_padd.y) {
					current_stream.size = 0;
					text_span = { 0.0f, 0.0f };
				}
				else {
					if (horizontal_alignment == ECS_UI_ALIGN_RIGHT) {
						if (!memcpy_all) {
							CullTextSprites<1, 1>(text_stream, current_stream, text_stream[1].position.x - scale.x + 2 * element_descriptor.label_padd.x);
							text_span = GetTextSpan(current_stream, true, true);

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
							CullTextSprites<0, 1>(text_stream, current_stream, text_stream[0].position.x + scale.x - 2 * element_descriptor.label_padd.x);
							text_span = GetTextSpan(current_stream, true, false);

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
				if (info->TextScale()->x > scale.x - 2 * element_descriptor.label_padd.x) {
					current_stream.size = 0;
					text_span = { 0.0f, 0.0f };
				}
				else {
					if (vertical_alignment == ECS_UI_ALIGN_BOTTOM) {
						if (!memcpy_all) {
							CullTextSprites<3, 1>(text_stream, current_stream, text_stream[text_stream.size - 3].position.y - scale.y + 2 * element_descriptor.label_padd.y);
							text_span = GetTextSpan(current_stream, false, true);

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
							if (vertical_alignment == ECS_UI_ALIGN_MIDDLE) {
								CullTextSprites<2, 1>(text_stream, current_stream, text_stream[0].position.y - scale.y + element_descriptor.label_padd.y);
							}
							else {
								CullTextSprites<2, 1>(text_stream, current_stream, text_stream[0].position.y - scale.y + 2 * element_descriptor.label_padd.y);
							}
							text_span = GetTextSpan(current_stream, false, true);

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
				if (draw_mode == ECS_UI_DRAWER_FIT_SPACE) {
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
				else if (draw_mode == ECS_UI_DRAWER_COLUMN_DRAW_FIT_SPACE || draw_mode == ECS_UI_DRAWER_COLUMN_DRAW) {
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
		) {
			ECS_UI_DRAW_PHASE phase = HandlePhase(configuration);
			if (configuration & UI_CONFIG_SLIDER_DEFAULT_VALUE) {
				AddHoverable(configuration, position, scale, { SliderReturnToDefault, info, 0, phase });
			}
			if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
				UIDrawerSliderEnterValuesData enter_data;
				enter_data.slider = info;
				enter_data.convert_input = functions.convert_text_input;
				enter_data.filter_function = filter;
				AddGeneral(configuration, position, scale, { SliderEnterValues, &enter_data, sizeof(enter_data), phase });
			}
			if (~configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
				UIDrawerSliderBringToMouse bring_info;
				bring_info.slider = info;
				bring_info.slider_length = HasFlag(configuration, UI_CONFIG_VERTICAL) ? slider_scale.y : slider_scale.x;

				if (info->text_input_counter == 0) {
					SolidColorRectangle(configuration, slider_position, slider_scale, slider_color);
					AddClickable(configuration, position, scale, { SliderBringToMouse, &bring_info, sizeof(bring_info), phase });
					AddClickable(configuration, slider_position, slider_scale, { SliderAction, info, 0, phase });
					AddDefaultHoverable(configuration, slider_position, slider_scale, slider_color, 1.25f, phase);
				}
			}
			else {
				if (info->text_input_counter == 0) {
					SliderMouseDraggableData action_data;
					action_data.slider = info;
					action_data.interpolate_bounds = false;

					if (configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
						for (size_t index = 0; index < config.flag_count; index++) {
							if (config.associated_bits[index] == UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
								const UIConfigSliderMouseDraggable* data = (const UIConfigSliderMouseDraggable*)config.GetParameter(UI_CONFIG_SLIDER_MOUSE_DRAGGABLE);
								action_data.interpolate_bounds = data->interpolate_bounds;
								break;
							}
						}
					}

					AddClickable(configuration, position, scale, { SliderMouseDraggable, &action_data, sizeof(action_data), phase });
					if (configuration & UI_CONFIG_SLIDER_DEFAULT_VALUE) {
						UIDrawerSliderReturnToDefaultMouseDraggable data;
						data.slider = info;
						data.hoverable_data.colors[0] = color;
						data.hoverable_data.percentages[0] = 1.25f;
						AddHoverable(configuration, position, scale, { SliderReturnToDefaultMouseDraggable, &data, sizeof(data), phase });
					}
					else {
						struct CopyValueData {
							UIDefaultHoverableData hoverable_data;
							UIDrawerSlider* slider;
						};

						// Default hoverable with a copy paste added on top of it
						auto copy_value = [](ActionData* action_data) {
							UI_UNPACK_ACTION_DATA;

							CopyValueData* data = (CopyValueData*)_data;
							action_data->data = data->slider;
							SliderCopyPaste(action_data);

							action_data->data = &data->hoverable_data;
							DefaultHoverableAction(action_data);
						};

						CopyValueData copy_data;
						copy_data.slider = info;
						copy_data.hoverable_data.colors[0] = color;
						AddHoverable(configuration, position, scale, { copy_value, &copy_data, sizeof(copy_data), phase });
					}
				}
				else {
					if (configuration & UI_CONFIG_SLIDER_DEFAULT_VALUE) {
						AddHoverable(configuration, position, scale, { SliderReturnToDefault, info, 0, phase });
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::HandleLabelSize(float2 text_span) const {
			float2 scale;
			scale = text_span;
			scale += float2(2.0f, 2.0f) * element_descriptor.label_padd;
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
				length = HasFlag(configuration, UI_CONFIG_VERTICAL) ? element_descriptor.slider_length.y : element_descriptor.slider_length.x;
			}

			if (configuration & UI_CONFIG_SLIDER_PADDING) {
				padding = *(const float*)config.GetParameter(UI_CONFIG_SLIDER_PADDING);
			}
			else {
				padding = HasFlag(configuration, UI_CONFIG_VERTICAL) ? element_descriptor.label_padd.y : element_descriptor.label_padd.x;
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

		void UIDrawer::HandleAcquireDrag(size_t configuration, float2 position, float2 scale)
		{
			if (acquire_drag_drop.names.size > 0) {
				bool should_highlight = false;
				acquire_drag_drop.payload = system->AcquireDragDrop(position, scale, acquire_drag_drop.names, &should_highlight, &acquire_drag_drop.matched_name);
				if (should_highlight && acquire_drag_drop.highlight_element) {
					float2 border_scale = { ECS_TOOLS_UI_ONE_PIXEL_X, ECS_TOOLS_UI_ONE_PIXEL_Y };
					void** border_buffers = buffers;
					if (configuration & UI_CONFIG_LATE_DRAW) {
						border_buffers = buffers + ECS_TOOLS_UI_MATERIALS;
					}
					else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
						border_buffers = system_buffers;
					}
					CreateSolidColorRectangleBorder<false>(position, scale, border_scale, acquire_drag_drop.highlight_color, HandleSolidColorCount(configuration), border_buffers);
				}
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
		void UIDrawer::TextLabel(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2& position, float2& scale) {
			Stream<UISpriteVertex> current_text = GetTextStream(configuration, ParseStringIdentifier(text) * 6);
			float2 text_span;

			float2 temp_position = position + element_descriptor.label_padd;
			Text(configuration | UI_CONFIG_DO_NOT_FIT_SPACE, config, text, temp_position, text_span);

			if (!initializer) {
				float2 label_scale = HandleLabelSize(text_span);

				if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) {
					scale.x = label_scale.x;
				}
				if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) {
					scale.y = label_scale.y;
				}

				GetElementAlignedPosition(configuration, config, position, scale);

				AlignToRowY(this, configuration, position, scale);

				float2 position_copy = position;
				bool is_moved = HandleFitSpaceRectangle(configuration, position, scale);

				ECS_UI_ALIGN horizontal_alignment = ECS_UI_ALIGN_MIDDLE, vertical_alignment = ECS_UI_ALIGN_TOP;
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
				else if (horizontal_alignment != ECS_UI_ALIGN_LEFT || vertical_alignment != ECS_UI_ALIGN_TOP) {
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
				HandleTextToolTip(configuration, config, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// resource drawer
		void UIDrawer::TextLabel(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2& position, float2 scale) {			
			if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) {
				scale.x = text->scale.x + 2.0f * element_descriptor.label_padd.x;
			}

			if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) {
				scale.y = text->scale.y + 2.0f * element_descriptor.label_padd.y;
			}

			AlignToRowY(this, configuration, position, scale);

			bool is_moved = HandleFitSpaceRectangle(configuration, position, scale);

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
			ECS_UI_ALIGN horizontal = ECS_UI_ALIGN_LEFT, vertical = ECS_UI_ALIGN_TOP;
			HandleTextLabelAlignment(
				configuration,
				config,
				text->scale,
				scale,
				position,
				x_position,
				y_position,
				horizontal,
				vertical
			);

			float2 font_size;
			float character_spacing;
			Color font_color;
			HandleText(configuration, config, font_color, font_size, character_spacing);

			HandleTextCopyFromResource(configuration | UI_CONFIG_DO_NOT_FIT_SPACE, text, float2(x_position, y_position), font_color, ECS_TOOLS_UI_DRAWER_IDENTITY_SCALE);

			FinalizeRectangle(configuration, position, scale);
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

				HandleRectangleActions(configuration, config, position, scale);
				HandleBorder(configuration, config, position, scale);

				HandleTextToolTip(configuration, config, position, scale);
			}
			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteClusterRectangle(
			size_t configuration,
			float2 position,
			float2 scale,
			Stream<wchar_t> texture,
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
			Stream<wchar_t> texture,
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
			Stream<wchar_t> texture,
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

		void UIDrawer::TextLabelWithCull(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2 position, float2 scale) {
			AlignToRowY(this, configuration, position, scale);

			size_t text_count = ParseStringIdentifier(text);
			Stream<UISpriteVertex> current_text = GetTextStream(configuration, text_count * 6);
			ECS_UI_ALIGN horizontal_alignment, vertical_alignment;
			GetTextLabelAlignment(configuration, config, horizontal_alignment, vertical_alignment);

			const float* dependent_size = (const float*)config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
			const ECS_UI_WINDOW_DEPENDENT_SIZE* type = (ECS_UI_WINDOW_DEPENDENT_SIZE*)dependent_size;

			Color color;
			float2 font_size;
			float character_spacing;
			HandleText(configuration, config, color, font_size, character_spacing);
			float2 text_span;

			auto text_sprites = HandleTextSpriteBuffer(configuration);
			auto text_sprite_count = HandleTextSpriteCount(configuration);

			bool horizontal = (configuration & UI_CONFIG_VERTICAL) == 0;
			bool invert_order = (vertical_alignment == ECS_UI_ALIGN_BOTTOM) || (horizontal_alignment == ECS_UI_ALIGN_RIGHT);
			system->ConvertCharactersToTextSprites(
				{ text.buffer, text_count },
				position + element_descriptor.label_padd,
				text_sprites,
				color,
				*text_sprite_count,
				font_size,
				character_spacing,
				horizontal,
				invert_order
			);
			text_span = GetTextSpan(current_text, horizontal, invert_order);

			size_t vertex_count = 0;
			if (~configuration & UI_CONFIG_VERTICAL) {
				if (*type == ECS_UI_WINDOW_DEPENDENT_BOTH && text_span.y > scale.y - 2 * element_descriptor.label_padd.y) {
					vertex_count = 0;
				}
				else {
					if (text_span.x > scale.x - 2 * element_descriptor.label_padd.x) {
						if (horizontal_alignment == ECS_UI_ALIGN_LEFT || horizontal_alignment == ECS_UI_ALIGN_MIDDLE) {
							vertex_count = CullTextSprites<0>(current_text, position.x + scale.x - element_descriptor.label_padd.x);
							current_text.size = vertex_count;
							text_span = GetTextSpan(current_text, true, false);
						}
						else {
							vertex_count = CullTextSprites<1>(current_text, position.x + element_descriptor.label_padd.x);
							current_text.size = vertex_count;
							text_span = GetTextSpan(current_text, true, true);
						}
					}
					else {
						vertex_count = current_text.size;
					}
				}
			}
			else {
				if (*type == ECS_UI_WINDOW_DEPENDENT_BOTH && text_span.x > scale.x - 2 * element_descriptor.label_padd.x) {
					vertex_count = 0;
				}
				else {
					if (text_span.y > scale.y - 2 * element_descriptor.label_padd.y) {
						if (vertical_alignment == ECS_UI_ALIGN_TOP || vertical_alignment == ECS_UI_ALIGN_MIDDLE) {
							vertex_count = CullTextSprites<2>(current_text, -position.y - scale.y + element_descriptor.label_padd.y);
							current_text.size = vertex_count;
							text_span = GetTextSpan(current_text, false, false);
						}
						else {
							vertex_count = CullTextSprites<3>(current_text, -position.y - scale.y + element_descriptor.label_padd.y);
							current_text.size = vertex_count;
							text_span = GetTextSpan(current_text, false, true);
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

			if (horizontal_alignment != ECS_UI_ALIGN_LEFT) {
				float x_translation = x_text_position - position.x - element_descriptor.label_padd.x;
				for (size_t index = 0; index < vertex_count; index++) {
					current_text[index].position.x += x_translation;
				}
			}

			if (vertical_alignment == ECS_UI_ALIGN_BOTTOM) {
				float y_translation = y_text_position + (current_text[current_text.size - 3]).position.y;
				for (size_t index = 0; index < vertex_count; index++) {
					current_text[index].position.y -= y_translation;
				}
			}
			else if (vertical_alignment == ECS_UI_ALIGN_MIDDLE) {
				float y_translation = y_text_position - position.y - element_descriptor.label_padd.y;
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

			FinalizeRectangle(configuration, position, scale);
			HandleTextToolTip(configuration, config, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextElement* UIDrawer::TextInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> characters, float2 position) {
			UIDrawerTextElement* element;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(characters, [&](Stream<char> label_identifier) {
				element = GetMainAllocatorBuffer<UIDrawerTextElement>();

				ConvertTextToWindowResource(configuration, config, label_identifier, element, position, {0.0f, 0.0f});

				if (~configuration & UI_CONFIG_DO_NOT_ADVANCE) {
					FinalizeRectangle(configuration, position, element->scale);
				}
				return element;
				});

			return element;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextElement* UIDrawer::TextLabelInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> characters, float2 position, float2 scale) {
			UIDrawerTextElement* element;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(characters, [&](Stream<char> identifier) {
				ECS_UI_ALIGN horizontal_alignment, vertical_alignment;
				element = GetMainAllocatorBuffer<UIDrawerTextElement>();

				ConvertTextToWindowResource(configuration, config, identifier, element, position + element_descriptor.label_padd, {0.0f, 0.0f});
				float x_position, y_position;
				if (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) {
						scale.x = element->scale.x + 2 * element_descriptor.label_padd.x;
					}
					if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) {
						scale.y = element->scale.y + 2 * element_descriptor.label_padd.y;
					}

					if (((configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) != 0) && ((configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) != 0)) {
						scale.x = ClampMin(scale.x, element->scale.x);
						scale.y = ClampMin(scale.y, element->scale.y);
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
			AlignToRowY(this, configuration, position, scale);

			ECS_UI_ALIGN horizontal_alignment, vertical_alignment;
			GetTextLabelAlignment(configuration, config, horizontal_alignment, vertical_alignment);

			float2 label_scale = HandleLabelSize(element->scale);
			if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) {
				scale.x = label_scale.x;
			}
			if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y) {
				scale.y = label_scale.y;
			}

			GetElementAlignedPosition(configuration, config, position, scale);
			
			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				if ((configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE)) {
					const float* dependent_size = (const float*)config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
					const ECS_UI_WINDOW_DEPENDENT_SIZE* type = (ECS_UI_WINDOW_DEPENDENT_SIZE*)dependent_size;

					size_t copy_count;
					float2 text_span;
					Stream<UISpriteVertex> vertices = GetTextStream(configuration, 0);

					if (*type == ECS_UI_WINDOW_DEPENDENT_HORIZONTAL) {
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
					else if (*type == ECS_UI_WINDOW_DEPENDENT_VERTICAL) {
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
					else if (*type == ECS_UI_WINDOW_DEPENDENT_BOTH) {
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
							if (vertical_alignment == ECS_UI_ALIGN_BOTTOM) {
								TranslateText(x_position, y_position, element->text_vertices, element->text_vertices.size - 1, element->text_vertices.size - 3);
							}
							else {
								TranslateText(x_position, y_position, element->text_vertices, 0, 0);
							}
						}
						else {
							if (horizontal_alignment == ECS_UI_ALIGN_RIGHT) {
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
			if (configuration & UI_CONFIG_ALIGN_ELEMENT) {
				if (~configuration & UI_CONFIG_TEXT_INPUT_NO_NAME) {
					float name_size = ElementNameSize(configuration, config, &input->name, scale);
					float total_x_scale = name_size + scale.x + layout.element_indentation;
					GetElementAlignedPosition(configuration, config, position, { total_x_scale, scale.y });
				}
				else {
					GetElementAlignedPosition(configuration, config, position, scale);
				}
			}

			AlignToRowY(this, configuration, position, scale);
			
			HandleFitSpaceRectangle(configuration, position, scale);
			
			float2 initial_position = position;
			float2 initial_scale = scale;

			input->padding = element_descriptor.label_padd;

			bool is_element_name_first = IsElementNameFirst(configuration, UI_CONFIG_TEXT_INPUT_NO_NAME);
			if (is_element_name_first) {
				ElementName(configuration, config, &input->name, position, scale);
			}

			bool dependent_size = configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE;
			if (dependent_size) {
				bool is_element_name_after = IsElementNameAfter(configuration, UI_CONFIG_TEXT_INPUT_NO_NAME);
				if (is_element_name_first) {
					scale.x -= position.x - initial_position.x;
				}
				else if (is_element_name_after) {
					float name_scale = ElementNameSize(configuration, config, &input->name, scale);
					scale.x -= name_scale + layout.element_indentation;
				}
			}

			if (configuration & UI_CONFIG_TEXT_INPUT_FORMAT_NUMBER) {
				if (!input->is_currently_selected) {
					SnapshotRunnable(input, 0, ECS_UI_DRAW_NORMAL, [](void* _data, ActionData* action_data) {
						UIDrawerTextInput* input = (UIDrawerTextInput*)_data;
						int64_t integer = ConvertCharactersToInt(*input->text);

						char temp_characters[256];
						CapacityStream<char> temp_stream(temp_characters, 0, 256);
						ConvertIntToCharsFormatted(temp_stream, integer);
						if (temp_stream != *input->text) {
							input->DeleteAllCharacters();
							input->InsertCharacters(temp_characters, temp_stream.size, 0, action_data->system);
						}

						return true;
					});
				}
			}

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = initial_position;
				*get_transform->scale = initial_scale;
			}

			// Trigger the callback if necessary
			if (input->HasCallback()) {
				// Copy the callback data again if needed
				if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
					const UIConfigTextInputCallback* callback = (const UIConfigTextInputCallback*)config.GetParameter(UI_CONFIG_TEXT_INPUT_CALLBACK);
					if (callback->handler.data_size > 0 && !callback->copy_on_initialization) {
						memcpy(input->callback_data, callback->handler.data, callback->handler.data_size);
					}
				}

				SnapshotRunnable(input, 0, ECS_UI_DRAW_NORMAL, [](void* _data, ActionData* action_data) {
					UIDrawerTextInput* input = (UIDrawerTextInput*)_data;

					if ((input->trigger_callback == UIDrawerTextInput::TRIGGER_CALLBACK_MODIFY && !input->trigger_callback_on_release) ||
						input->trigger_callback == UIDrawerTextInput::TRIGGER_CALLBACK_EXIT) 
					{
						input->Callback(action_data);
						input->trigger_callback = UIDrawerTextInput::TRIGGER_CALLBACK_NONE;
					}
					return false;
				});
			}

			// Determine if the text was changed from outside the UI - do this after the callback to give it
			// a chance to use the previous data
			SnapshotRunnable(input, 0, ECS_UI_DRAW_NORMAL, [](void* _data, ActionData* action_data) {
				UIDrawerTextInput* input = (UIDrawerTextInput*)_data;

				if (!input->is_currently_selected) {
					if (*input->text != input->previous_text) {
						// It has changed from the outside
						input->sprite_render_offset = 0;
						input->current_sprite_position = 0;
						input->current_selection = 0;
						input->previous_text.CopyOther(*input->text);
						return true;
					}
					return false;
				}

				// When we are selected, always redraw otherwise we have to update many actions to
				// Support redrawing, but this should suffice
				return true;
			});

			if (ValidatePosition(configuration, position, scale)) {
				if (configuration & UI_CONFIG_TEXT_INPUT_MISC) {
					const UIConfigTextInputMisc* misc = (const UIConfigTextInputMisc*)config.GetParameter(UI_CONFIG_TEXT_INPUT_MISC);
					input->display_tooltip = misc->display_tooltip;
				}
				else {
					input->display_tooltip = false;
				}

				bool is_active = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				float2 font_size;
				float character_spacing;
				Color font_color;
				HandleText(configuration, config, font_color, font_size, character_spacing);
				if (font_size != input->font_size) {
					input->SetNewZoom(*zoom_ptr);
				}

				if (!is_active) {
					font_color = color_theme.unavailable_text;
				}
				input->text_color = font_color;

				Stream<char> characters_to_draw = { nullptr, 0 };

				size_t label_configuration = configuration | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_WINDOW_DEPENDENT_SIZE;
				label_configuration = ClearFlag(label_configuration, UI_CONFIG_ALIGN_TO_ROW_Y, UI_CONFIG_DO_CACHE);

				UIDrawConfig label_config;
				memcpy(&label_config, &config, sizeof(label_config));

				if (input->text->size > 0) {
					characters_to_draw = { input->text->buffer + input->sprite_render_offset, input->text->size - input->sprite_render_offset };
				}
				else {
					characters_to_draw = input->hint_text;
					
					Color hint_color = DarkenColor(font_color, 0.75f);
					hint_color.alpha = 150;
					// Change the text color
					UIConfigTextParameters text_parameters;
					text_parameters.color = hint_color;
					text_parameters.size = font_size * zoom_inverse;
					text_parameters.character_spacing = character_spacing * zoom_inverse.x;

					UIConfigTextParameters previous_parameters;
					SetConfigParameter(configuration, label_config, text_parameters, previous_parameters);
					label_configuration |= UI_CONFIG_TEXT_PARAMETERS;
				}

				UIConfigTextAlignment text_alignment;
				text_alignment.horizontal = ECS_UI_ALIGN_LEFT;
				text_alignment.vertical = ECS_UI_ALIGN_MIDDLE;
				UIConfigTextAlignment previous_text_alignment;
				SetConfigParameter(configuration, label_config, text_alignment, previous_text_alignment);

				size_t* text_count = HandleTextSpriteCount(configuration);

				// This is needed such that it will cull the sprites
				UIConfigWindowDependentSize dependent_size;
				UIConfigWindowDependentSize previous_dependent_size;
				SetConfigParameter(configuration, label_config, dependent_size, previous_dependent_size);
				FixedScaleTextLabel(label_configuration, label_config, characters_to_draw, position, scale);

				if (input->is_caret_display) {
					float y_sprite_size = system->GetTextSpriteYScale(font_size.y);

					float2 caret_position = input->GetCaretPosition(system);
					caret_position.y = AlignMiddle(position.y, scale.y, y_sprite_size);

					UISpriteVertex* text_buffer = HandleTextSpriteBuffer(configuration);
					system->ConvertCharactersToTextSprites(
						{ "|", 1 },
						caret_position,
						text_buffer,
						input->text_color,
						*text_count,
						input->font_size,
						input->character_spacing
					);
					*text_count += 6;
				}

				input->position = position;
				input->bound = position.x + scale.x - input->padding.x;
				Color color = HandleColor(configuration, config);
				SolidColorRectangle(configuration, position, scale, color);

				if (is_active) {
					// Use system phase for the hoverable because we might have to display a tooltip
					AddHoverable(configuration, position, scale, { TextInputHoverable, input, 0, ECS_UI_DRAW_SYSTEM });
					AddClickable(configuration, position, scale, { TextInputClickable, input, 0 });
					UIDrawerTextInputActionData input_data = { input, filter };
					AddGeneral(configuration, position, scale, { TextInputAction, &input_data, sizeof(input_data) });
				}

				if (input->current_selection != input->current_sprite_position) {
					Color hsv_color = RGBToHSV(color);
					hsv_color.hue -= 60;
					//Color select_color = ToneColor(color, color_theme.select_text_factor);
					Color select_color = HSVToRGB(hsv_color);
					float2 first_position;
					float2 last_position;

					if (input->current_selection < input->current_sprite_position) {
						first_position = input->GetPositionFromSprite(system, input->current_selection);
						last_position = input->GetPositionFromSprite(system, input->current_sprite_position - 1, true, false);
					}
					else {
						first_position = input->GetPositionFromSprite(system, input->current_sprite_position);
						last_position = input->GetPositionFromSprite(system, input->current_selection - 1, true, false);
					}

					if (last_position.x > input->bound) {
						last_position.x = input->bound;
					}
					if (first_position.x < position.x + input->padding.x) {
						first_position.x = position.x + input->padding.x;
					}

					SolidColorRectangle(configuration, first_position, { last_position.x - first_position.x, last_position.y - first_position.y }, select_color);
					if (input->is_caret_display) {
						float y_sprite_size = system->GetTextSpriteYScale(font_size.y);

						float2 caret_position = input->GetCaretPosition(system);
						caret_position.y = AlignMiddle(position.y, scale.y, y_sprite_size);
						UISpriteVertex* text_buffer = HandleTextSpriteBuffer(configuration);

						*text_count -= 6;
						system->ConvertCharactersToTextSprites(
							{ "|", 1 },
							caret_position,
							text_buffer,
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

			bool is_name_after = IsElementNameAfter(configuration, UI_CONFIG_TEXT_INPUT_NO_NAME);
			FinalizeRectangle(configuration | (is_name_after ? UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW : 0), position, scale);

			if (is_name_after) {
				ElementName(configuration, config, &input->name, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextInput* UIDrawer::TextInputInitializer(size_t configuration, UIDrawConfig& config, Stream<char> name, CapacityStream<char>* text_to_fill, float2 position, float2 scale) {
			UIDrawerTextInput* element;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {

				float character_spacing;
				float2 font_size;
				Color font_color;
				HandleText(configuration, config, font_color, font_size, character_spacing);

				element = GetMainAllocatorBuffer<UIDrawerTextInput>();

				ConvertTextToWindowResource(
					configuration,
					config,
					identifier,
					&element->name,
					{ 0.0f, 0.0f },
					scale
				);

				element->caret_start.SetNewStart();
				element->key_repeat_start.SetNewStart();
				element->repeat_key_count = 0;
				element->is_caret_display = false;
				element->character_spacing = character_spacing;
				element->current_selection = 0;
				element->current_sprite_position = 0;
				element->solid_color_y_scale = scale.y;
				element->position = { 0.0f, 0.0f };
				element->padding = element_descriptor.label_padd;
				element->text_color = font_color;
				element->inverse_zoom = zoom_inverse;
				element->current_zoom = *zoom_ptr;
				element->font_size = font_size;
				element->sprite_render_offset = 0;
				element->suppress_arrow_movement = false;
				element->word_click_count = 0;
				element->bound = position.x + scale.x - element_descriptor.label_padd.x;
				element->text = text_to_fill;
				element->filter_characters_start = 0;
				element->filter_character_count = 0;
				element->is_currently_selected = false;
				element->previous_text.buffer = (char*)GetMainAllocatorBuffer(sizeof(char) * text_to_fill->capacity);
				element->previous_text.size = 0;
				element->previous_text.capacity = text_to_fill->capacity;
				element->display_tooltip = false;

				if (text_to_fill->size > 0) {
					unsigned int character_count = text_to_fill->size;
					ECS_STACK_CAPACITY_STREAM_DYNAMIC(char, temp_characters, character_count);
					temp_characters.CopyOther(*text_to_fill);

					text_to_fill->size = 0;
					element->InsertCharacters(temp_characters.buffer, character_count, 0, system);
					element->sprite_render_offset = 0;
					element->text->CopyTo(element->previous_text.buffer);
					element->previous_text.size = text_to_fill->size;
				}

				if (configuration & UI_CONFIG_TEXT_INPUT_HINT) {
					const char* hint = *(const char**)config.GetParameter(UI_CONFIG_TEXT_INPUT_HINT);
					size_t hint_size = strlen(hint);
					element->hint_text.buffer = (char*)GetMainAllocatorBuffer(sizeof(char) * hint_size);
					element->hint_text.size = hint_size;
					memcpy(element->hint_text.buffer, hint, sizeof(char) * hint_size);
				}
				else {
					element->hint_text = { nullptr, 0 };
				}

				if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
					const UIConfigTextInputCallback* callback = (const UIConfigTextInputCallback*)config.GetParameter(UI_CONFIG_TEXT_INPUT_CALLBACK);
					if (callback->handler.data_size > 0) {
						element->callback_data = GetMainAllocatorBuffer(callback->handler.data_size);
						memcpy(element->callback_data, callback->handler.data, callback->handler.data_size);
					}
					else {
						element->callback_data = callback->handler.data;
					}
					element->callback = callback->handler.action;
					element->trigger_callback_on_release = callback->trigger_only_on_release;
				}
				else {
					element->callback = nullptr;
					element->callback_data = nullptr;
					element->trigger_callback_on_release = false;
				}
				element->trigger_callback = UIDrawerTextInput::TRIGGER_CALLBACK_NONE;

				FinalizeRectangle(configuration, position, scale);
				return element;
				});

			return element;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawerSliderIsDraggable(size_t configuration, const UIDrawConfig& config) {
			bool mouse_draggable_fixed_offset = (configuration & UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) != 0;
			if (mouse_draggable_fixed_offset) {
				for (size_t index = 0; index < config.flag_count; index++) {
					if (config.associated_bits[index] == UI_CONFIG_SLIDER_MOUSE_DRAGGABLE) {
						const UIConfigSliderMouseDraggable* draggable = (const UIConfigSliderMouseDraggable*)config.GetParameter(UI_CONFIG_SLIDER_MOUSE_DRAGGABLE);
						mouse_draggable_fixed_offset = !draggable->interpolate_bounds;
						break;
					}
				}
			}

			return mouse_draggable_fixed_offset;
		}

		UIDrawerSlider* UIDrawer::SliderInitializer(
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
		) {
			UIDrawerSlider* slider;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {
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
						ConvertTextToWindowResource(configuration, config, identifier, &slider->label, { position.x + scale.x + slider_padding + slider_length, alignment }, scale);
					}
					else {
						ConvertTextToWindowResource(configuration, config, identifier, &slider->label, { position.x, position.y + scale.y + slider_padding + slider_length }, scale);
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

				bool mouse_draggable = UIDrawerSliderIsDraggable(configuration, config);
				if (!mouse_draggable) {
					bool is_smaller = functions.is_smaller(value_to_modify, lower_bound);
					bool is_greater = functions.is_smaller(upper_bound, value_to_modify);
					if (is_smaller || is_greater) {
						slider->slider_position = 0.5f;
						functions.interpolate(lower_bound, upper_bound, value_to_modify, slider->slider_position);
					}
					else {
						slider->slider_position = functions.percentage(lower_bound, upper_bound, value_to_modify);
					}
				}
				else {
					slider->slider_position = functions.to_float(default_value);
				}

				slider->text_input_counter = 0;
				slider->value_to_change = value_to_modify;

				if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
					UIConfigSliderChangedValueCallback* callback = (UIConfigSliderChangedValueCallback*)config.GetParameter(UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);
					if (callback->handler.data_size > 0) {
						void* allocation = GetMainAllocatorBuffer(callback->handler.data_size);
						memcpy(allocation, callback->handler.data, callback->handler.data_size);
						callback->handler.data = allocation;
					}
					
					slider->changed_value_callback = callback->handler;
				}
				else {
					slider->changed_value_callback = { nullptr, nullptr, 0 };
				}

				if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
					UIConfigTextInputCallback previous_callback;
					UIConfigTextInputCallback input_callback;
					input_callback.handler = slider->changed_value_callback;
					if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
						config.SetExistingFlag(input_callback, previous_callback);
					}
					else {
						config.AddFlag(input_callback);
					}

					ECS_STACK_CAPACITY_STREAM(char, stack_memory, 256);
					stack_memory.CopyOther(identifier);
					stack_memory.AddStream("TextInput");

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

					RemoveConfigParameter(configuration, config, previous_callback);
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

			float2 initial_position = position;
			float2 initial_scale = scale;

			bool is_name_first = IsElementNameFirst(configuration, UI_CONFIG_SLIDER_NO_NAME);
			if (is_name_first) {
				ElementName(configuration, config, &slider->label, position, scale);
			}

			bool dependent_size = configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE;
			if (dependent_size) {
				bool is_name_after = IsElementNameAfter(configuration, UI_CONFIG_SLIDER_NO_NAME);
				if (is_name_first) {
					scale.x -= position.x - initial_position.x;
				}
				else if (is_name_after) {
					scale.x -= slider->label.scale.x + layout.element_indentation;
				}
			}

			bool mouse_draggable_fixed_offset = UIDrawerSliderIsDraggable(configuration, config);

			if (ValidatePosition(configuration, position)) {
				if (slider->interpolate_value) {
					if (!mouse_draggable_fixed_offset) {
						slider->slider_position = Clamp(slider->slider_position, 0.0f, 1.0f);
						functions.interpolate(lower_bound, upper_bound, value_to_modify, slider->slider_position);
					}
					else {
						functions.from_float(value_to_modify, slider->slider_position);
					}
				}
				else if (slider->character_value) {
					size_t storage[128];
					memcpy(storage, value_to_modify, slider->value_byte_size);

					functions.convert_text_input(slider->characters, value_to_modify);
					if (!mouse_draggable_fixed_offset) {
						bool is_greater = functions.is_smaller(upper_bound, value_to_modify);
						bool is_smaller = functions.is_smaller(value_to_modify, lower_bound);
						if (is_greater) {
							memcpy(value_to_modify, upper_bound, slider->value_byte_size);
						}
						else if (is_smaller) {
							memcpy(value_to_modify, lower_bound, slider->value_byte_size);
						}
					}
					slider->slider_position = functions.percentage(lower_bound, upper_bound, value_to_modify);
					slider->character_value = false;
					slider->changed_value = memcmp(storage, value_to_modify, slider->value_byte_size) != 0;
				}
				else {
					if (!mouse_draggable_fixed_offset) {
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
					else {
						slider->slider_position = functions.to_float(value_to_modify);
					}
				}

				ECS_UI_WINDOW_DEPENDENT_SIZE* type = nullptr;
				float2 copy_position = position;

				if (configuration & UI_CONFIG_VERTICAL) {
					scale.y = ClampMin(scale.y, slider_length + 2.0f * slider_padding + 0.005f);
				}
				else {
					scale.x = ClampMin(scale.x, slider_length + 2.0f * slider_padding + 0.005f);
				}
				if (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					if (configuration & UI_CONFIG_VERTICAL) {
						HandleFitSpaceRectangle(configuration, position, { scale.x, scale.y + 2 * slider_padding });

						if (configuration & UI_CONFIG_GET_TRANSFORM) {
							UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
							*get_transform->position = position;
							*get_transform->scale = { scale.x, scale.y + 2 * slider_padding };
						}
					}
					else {
						HandleFitSpaceRectangle(configuration, position, { scale.x + 2 * slider_padding, scale.y });

						if (configuration & UI_CONFIG_GET_TRANSFORM) {
							UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
							*get_transform->position = position;
							*get_transform->scale = { scale.x + 2 * slider_padding, scale.y };
						}
					}
				}
				else {
					const float* window_transform = (const float*)config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
					type = (ECS_UI_WINDOW_DEPENDENT_SIZE*)window_transform;

					if (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}

					/*if (configuration & UI_CONFIG_VERTICAL) {
						scale.y -= slider_length + 2 * slider_padding;
					}
					else {
						scale.x -= slider_length + 2 * slider_padding;
					}*/
				}

				if (~configuration & UI_CONFIG_SLIDER_NO_TEXT) {
					auto text_label_lambda = [&]() {
						functions.to_string(slider->characters, value_to_modify, functions.extra_data);
						slider->characters[slider->characters.size] = '\0';
						FixedScaleTextLabel(
							ClearFlag(configuration, UI_CONFIG_DO_CACHE),
							config,
							slider->characters.buffer,
							position,
							scale
						);

						AddHoverable(configuration, position, scale, { SliderCopyPaste, slider, 0 });
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
								slider->text_input->current_selection = 0;
								slider->text_input->current_sprite_position = slider->characters.size;
							}
							slider->text_input_counter++;
							TextInputDrawer(configuration | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_DO_NOT_ADVANCE, config, slider->text_input, position, scale, filter);
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
						slider_position.y = position.y + slider_padding + (scale.y - slider_padding * 2 - slider_length) * slider->slider_position;
					}
					else {
						slider_scale.x = slider_length;
						slider_scale.y = scale.y - 2 * slider_shrink.y;
						slider_position.x = position.x + slider_padding + (scale.x - slider_padding * 2 - slider_length) * slider->slider_position;
						slider_position.y = AlignMiddle(position.y, scale.y, slider_scale.y);
					}
				}

				HandleLateAndSystemDrawActionNullify(configuration, position, scale);

				if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					if (*type == ECS_UI_WINDOW_DEPENDENT_BOTH) {
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
								HandleSliderActions(configuration, config, position, scale, color, slider_position, slider_scale, slider_color, slider, functions, filter);
							}
						}
					}
					else {
						if (slider_scale.x < scale.x - slider_padding) {
							HandleSliderActions(configuration, config, position, scale, color, slider_position, slider_scale, slider_color, slider, functions, filter);
						}
					}
				}
				else {
					HandleSliderActions(configuration, config, position, scale, color, slider_position, slider_scale, slider_color, slider, functions, filter);
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
					if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
						const UIConfigSliderChangedValueCallback* callback = (const UIConfigSliderChangedValueCallback*)config.GetParameter(UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK);
						if (callback->handler.data_size > 0 && !callback->copy_on_initialization) {
							memcpy(slider->changed_value_callback.data, callback->handler.data, callback->handler.data_size);
						}
					}
				}
			}

			// Add a snapshot runnable that will redraw the window if the changed value is modified
			// And call the callback
			SnapshotRunnable(slider, 0, ECS_UI_DRAW_NORMAL, [](void* _data, ActionData* action_data) {
				UIDrawerSlider* slider = (UIDrawerSlider*)_data;

				bool was_changed = slider->changed_value;
				if (slider->changed_value) {
					if (slider->changed_value_callback.action != nullptr) {
						// Update the data if needed
						action_data->data = slider->changed_value_callback.data;
						slider->changed_value_callback.action(action_data);
					}
					slider->changed_value = false;
				}

				// We need to redraw if the slider needs to transition to a text input as well
				return was_changed || slider->text_input_counter == 1;
			});

			bool is_name_after = IsElementNameAfter(configuration, UI_CONFIG_SLIDER_NO_NAME);
			size_t finalize_configuration = is_name_after ? configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW : configuration;
			if (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				FinalizeRectangle(finalize_configuration, position, scale);
			}
			else {
				if (!is_null_window_dependent_size) {
					FinalizeRectangle(finalize_configuration, position, scale);
				}
			}
			if (is_name_after) {
				ElementName(configuration, config, &slider->label, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SliderGroupDrawer(
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
		) {
			float2 initial_position = position;

			bool is_name_first = IsElementNameFirst(configuration, UI_CONFIG_SLIDER_GROUP_NO_NAME);
			bool is_name_after = IsElementNameAfter(configuration, UI_CONFIG_SLIDER_GROUP_NO_NAME);
			if (is_name_first) {
				ElementName(configuration, config, group_name, position, scale);
			}

			if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				// Account for the name - if before the group
				if (is_name_first) {
					scale.x -= position.x - initial_position.x;
				}
				else if (is_name_after) {
					scale.x -= ElementNameSize(configuration, config, group_name, scale);
				}
				scale.x -= layout.element_indentation * 0.25f * (count - 1);
				scale.x /= count;
			}

			bool has_pushed_stack = PushIdentifierStackStringPattern();
			PushIdentifierStack(group_name);

			float indentation = layout.element_indentation;
			float label_padding = element_descriptor.label_padd.x;

			layout.element_indentation *= 0.25f;
			element_descriptor.label_padd.x *= 0.5f;

			size_t slider_configuration = ClearFlag(configuration, UI_CONFIG_NAME_PADDING) | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW;
			for (size_t index = 0; index < count; index++) {
				UIDrawerSlider* slider = (UIDrawerSlider*)GetResource(names[index]);
				
				const void* lower_bound;
				const void* upper_bound;
				if (configuration & UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS) {
					lower_bound = lower_bounds;
					upper_bound = upper_bounds;
				}
				else {
					lower_bound = OffsetPointer(lower_bounds, index * slider->value_byte_size);
					upper_bound = OffsetPointer(upper_bounds, index * slider->value_byte_size);
				}

				if (~configuration & UI_CONFIG_SLIDER_GROUP_NO_SUBNAMES) {
					SliderDrawer(
						slider_configuration | UI_CONFIG_ELEMENT_NAME_FIRST,
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
						slider_configuration | UI_CONFIG_SLIDER_NO_NAME,
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

				position.x = GetCurrentPosition().x;
			}
			layout.element_indentation = indentation;
			element_descriptor.label_padd.x = label_padding;

			// To restore the previous indent
			Indent(0.75f);

			if (has_pushed_stack) {
				PopIdentifierStack();
			}
			PopIdentifierStack();

			if (is_name_after) {
				// Indent the name by a full indentation, the last slider only indented at 25%
				Indent(0.75f);
				ElementName(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, config, group_name, position, scale);
			}

			HandleDrawMode(configuration);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SliderGroupInitializer(
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
		) {
			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}

			if (configuration & UI_CONFIG_DYNAMIC_RESOURCE) {
				// It needs to be stable
				Stream<char> identifier = HandleResourceIdentifier(group_name, true);
				// Add a nullptr resource if it is a dynamic element
				AddWindowResourceToTable(nullptr, identifier);
			}

			bool has_pushed_stack = PushIdentifierStackStringPattern();
			PushIdentifierStack(group_name);

			size_t slider_configuration = ClearFlag(configuration, UI_CONFIG_NAME_PADDING);
			for (size_t index = 0; index < count; index++) {
				const void* lower_bound = nullptr;
				const void* upper_bound = nullptr;
				const void* default_value = values_to_modify[index];

				size_t byte_offset = index * byte_size;
				if (configuration & UI_CONFIG_SLIDER_GROUP_UNIFORM_BOUNDS) {
					lower_bound = lower_bounds;
					upper_bound = upper_bounds;
					default_value = default_values;
				}
				else {
					lower_bound = OffsetPointer(lower_bounds, byte_offset);
					upper_bound = OffsetPointer(upper_bounds, byte_offset);
					if (default_values != nullptr) {
						default_value = OffsetPointer(default_values, byte_offset);
					}
				}

				if (~configuration & UI_CONFIG_SLIDER_GROUP_NO_SUBNAMES) {
					SliderInitializer(
						slider_configuration | UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN,
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
						slider_configuration | UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN,
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
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		UIDrawerSlider* UIDrawer::IntSliderInitializer(
			size_t configuration,
			UIDrawConfig& config,
			Stream<char> name,
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

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(UIDrawerSlider*, UIDrawer::IntSliderInitializer<integer>, size_t, UIDrawConfig&, Stream<char>, float2, float2, integer*, integer, integer, integer);

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
			Stream<char> name,
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
			Stream<char> name,
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
			Stream<char> name,
			Stream<Stream<char>> labels,
			unsigned int label_display_count,
			unsigned char* active_label,
			float2 scale
		) {
			UIDrawerComboBox* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {
				float2 position = { 0.0f, 0.0f };

				data = GetMainAllocatorBuffer<UIDrawerComboBox>();

				data->active_label = active_label;
				data->label_display_count = label_display_count;

				InitializeElementName(configuration, UI_CONFIG_COMBO_BOX_NO_NAME, config, identifier, &data->name, position, scale);

				size_t allocation_size = StreamCoalescedDeepCopySize(labels);
				void* allocation = GetMainAllocatorBuffer(allocation_size);
				uintptr_t ptr = (uintptr_t)allocation;
				data->labels = StreamCoalescedDeepCopy(labels, ptr);

				if (configuration & UI_CONFIG_COMBO_BOX_PREFIX) {
					const UIConfigComboBoxPrefix* prefix = (const UIConfigComboBoxPrefix*)config.GetParameter(UI_CONFIG_COMBO_BOX_PREFIX);
					void* allocation = GetMainAllocatorBuffer(sizeof(char) * prefix->prefix.size, alignof(char));
					prefix->prefix.CopyTo(allocation);
					data->prefix = { allocation, prefix->prefix.size };

					float2 text_span = TextSpan(data->prefix);
					data->prefix_x_scale = text_span.x / zoom_ptr->x;
				}
				else {
					data->prefix = { nullptr, 0 };
					data->prefix_x_scale = 0.0f;
				}

				data->biggest_label_x_index = ComboBoxBiggestLabel(labels);

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

				if (configuration & UI_CONFIG_COMBO_BOX_MAPPING) {
					const UIConfigComboBoxMapping* mappings = (const UIConfigComboBoxMapping*)config.GetParameter(UI_CONFIG_COMBO_BOX_MAPPING);
					if (mappings->stable) {
						data->mappings = mappings->mappings;
					}
					else {
						size_t copy_size = mappings->byte_size * labels.size;
						data->mappings = GetMainAllocatorBuffer(copy_size);
						memcpy(data->mappings, mappings->mappings, copy_size);
					}

					data->mapping_byte_size = mappings->byte_size;
					data->mapping_capacity = labels.size;
				}
				else {
					data->mappings = nullptr;
					data->mapping_byte_size = 0;
					data->mapping_capacity = 0;
				}

				if (configuration & UI_CONFIG_COMBO_BOX_UNAVAILABLE) {
					const UIConfigComboBoxUnavailable* unavailables = (const UIConfigComboBoxUnavailable*)config.GetParameter(UI_CONFIG_COMBO_BOX_UNAVAILABLE);
					if (unavailables->stable) {
						data->unavailables = unavailables->unavailables;
					}
					else {
						size_t copy_size = sizeof(bool) * labels.size;
						data->unavailables = (bool*)GetMainAllocatorBuffer(copy_size);
						memcpy(data->unavailables, unavailables->unavailables, copy_size);
					}
					data->unavailables_capacity = labels.size;
				}
				else {
					data->unavailables = nullptr;
					data->unavailables_capacity = 0;
				}

				return data;
			});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextElement* UIDrawer::CheckBoxInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, float2 scale) {
			UIDrawerTextElement* element = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {
				element = GetMainAllocatorBuffer<UIDrawerTextElement>();

				InitializeElementName(configuration, UI_CONFIG_CHECK_BOX_NO_NAME, config, identifier, element, { 0.0f, 0.0f }, scale);

				return element;
			});

			return element;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename NameType>
		void CheckBoxDrawerImplementation(UIDrawer* drawer, size_t configuration, const UIDrawConfig& config, NameType name, bool* value_to_modify, float2 position, float2 scale) {
			struct DefaultActionData {
				bool* value_to_modify;
				UIConfigCheckBoxDefault default_value;
				UIActionHandler callback;
			};

			auto return_default_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				DefaultActionData* data = (DefaultActionData*)_data;
				if (mouse->IsPressed(ECS_MOUSE_RIGHT)) {
					bool new_value = false;
					if (data->default_value.is_pointer_variable) {
						new_value = *data->default_value.pointer_variable;
					}
					else {
						new_value = data->default_value.value;
					}

					if (new_value != *data->value_to_modify) {
						action_data->redraw_window = true;
						*data->value_to_modify = new_value;
						if (data->callback.action != nullptr) {
							action_data->data = data->callback.data;
							action_data->additional_data = data->value_to_modify;
							data->callback.action(action_data);
						}
					}
				}
			};

			// Returns a pointer to the config to be used for the name and the configuration
			auto check_box_name = [&]() {
				UIDrawConfig name_config;
				DefaultActionData default_data;

				size_t name_configuration = configuration;
				const UIDrawConfig* config_to_use = &config;
				if (configuration & UI_CONFIG_CHECK_BOX_DEFAULT) {
					memcpy(&name_config, &config, sizeof(name_config));

					const UIConfigCheckBoxDefault* default_value = (const UIConfigCheckBoxDefault*)config.GetParameter(UI_CONFIG_CHECK_BOX_DEFAULT);

					default_data.value_to_modify = value_to_modify;
					default_data.default_value = *default_value;
					default_data.callback.action = nullptr;
					default_data.callback.phase = ECS_UI_DRAW_NORMAL;

					if (configuration & UI_CONFIG_CHECK_BOX_CALLBACK) {
						const UIConfigCheckBoxCallback* callback = (const UIConfigCheckBoxCallback*)config.GetParameter(UI_CONFIG_CHECK_BOX_CALLBACK);
						default_data.callback = callback->handler;
						if (callback->handler.data_size > 0) {
							default_data.callback.data = drawer->GetHandlerBuffer(callback->handler.data_size, callback->handler.phase);
							memcpy(default_data.callback.data, callback->handler.data, callback->handler.data_size);
						}
					}

					UIConfigElementNameAction name_action;
					name_action.hoverable_handler = { return_default_action, &default_data, sizeof(default_data), default_data.callback.phase };				
					name_config.AddFlag(name_action);

					config_to_use = &name_config;
				}
				drawer->ElementName(name_configuration, *config_to_use, name, position, scale);
			};

			if (IsElementNameFirst(configuration, UI_CONFIG_CHECK_BOX_NO_NAME)) {
				check_box_name();
			}

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (drawer->ValidatePosition(configuration, position, scale)) {
				bool state = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					state = active_state->state;
				}

				Color color = drawer->HandleColor(configuration, config);
				if (!state) {
					drawer->SolidColorRectangle(configuration, position, scale, DarkenColor(color, drawer->color_theme.darken_inactive_item));
				}
				else {
					drawer->SolidColorRectangle(configuration, position, scale, color);
					if (*value_to_modify) {
						Color check_color = ToneColor(color, drawer->color_theme.check_box_factor);
						drawer->SpriteRectangle(configuration, position, scale, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, check_color);
					}

					ECS_UI_DRAW_PHASE phase = drawer->HandlePhase(configuration);

					drawer->HandleLateAndSystemDrawActionNullify(configuration, position, scale);

					if (configuration & UI_CONFIG_CHECK_BOX_CALLBACK) {
						const UIConfigCheckBoxCallback* callback = (const UIConfigCheckBoxCallback*)config.GetParameter(UI_CONFIG_CHECK_BOX_CALLBACK);
						UIActionHandler callback_handler = callback->handler;

						if (phase == ECS_UI_DRAW_LATE && callback_handler.phase == ECS_UI_DRAW_NORMAL) {
							callback_handler.phase = phase;
						}
						if (phase == ECS_UI_DRAW_SYSTEM && callback_handler.phase != ECS_UI_DRAW_SYSTEM) {
							callback_handler.phase = ECS_UI_DRAW_SYSTEM;
						}

						if (!callback->disable_value_to_modify) {
							struct WrapperData {
								bool* value_to_modify;
								Action callback;
								void* callback_data;
							};

							auto wrapper = [](ActionData* action_data) {
								UI_UNPACK_ACTION_DATA;

								WrapperData* data = (WrapperData*)_data;
								bool old_value = *data->value_to_modify;
								*data->value_to_modify = !old_value;
								action_data->redraw_window = true;

								if (data->callback_data != nullptr) {
									action_data->data = data->callback_data;
								}
								else {
									action_data->data = OffsetPointer(data, sizeof(*data));
								}
								action_data->additional_data = data->value_to_modify;
								data->callback(action_data);
							};

							size_t _wrapper_data[512];
							WrapperData* wrapper_data = (WrapperData*)_wrapper_data;
							*wrapper_data = { value_to_modify, callback->handler.action, callback->handler.data };

							unsigned int wrapper_size = sizeof(*wrapper_data);
							// The data needs to be copied, embedd it after the wrapper
							if (callback->handler.data_size > 0) {
								void* callback_data = OffsetPointer(wrapper_data, sizeof(WrapperData));
								memcpy(callback_data, callback->handler.data, callback->handler.data_size);
								// Signal that the data is relative to the wrapper
								wrapper_data->callback_data = nullptr;

								wrapper_size += callback->handler.data_size;
							}
							
							drawer->AddDefaultClickableHoverable(configuration, position, scale, { wrapper, wrapper_data, wrapper_size, callback_handler.phase });
						}
						else {
							drawer->AddDefaultClickableHoverable(configuration, position, scale, callback_handler);
						}
					}
					else {
						drawer->AddDefaultClickableHoverable(configuration, position, scale, { BoolClickable, value_to_modify, 0, phase }, nullptr, color);
					}
				}

			}

			bool is_name_after = IsElementNameAfter(configuration, UI_CONFIG_CHECK_BOX_NO_NAME);
			size_t finalize_configuration = configuration;
			finalize_configuration |= is_name_after ? UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW : 0;
			drawer->FinalizeRectangle(finalize_configuration, position, scale);

			if (is_name_after) {
				check_box_name();
			}
		}

		void UIDrawer::CheckBoxDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* element, bool* value_to_modify, float2 position, float2 scale) {
			CheckBoxDrawerImplementation(this, configuration, config, element, value_to_modify, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::CheckBoxDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, bool* value_to_modify, float2 position, float2 scale) {
			CheckBoxDrawerImplementation(this, configuration, config, name, value_to_modify, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ComboBoxDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerComboBox* data, unsigned char* active_label, float2 position, float2 scale) {
			// Determine the minimum size
			data->active_label = active_label;
			float biggest_label_scale = data->labels.size > 0 ? TextSpan(data->labels[data->biggest_label_x_index]).x : 0.0f;
			float min_value = biggest_label_scale + GetSquareScale(layout.default_element_y).x + element_descriptor.label_padd.x * 2.0f;
			min_value += data->prefix_x_scale * zoom_ptr->x;
			
			bool is_active = true;
			if (configuration & UI_CONFIG_ACTIVE_STATE) {
				const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				is_active = active_state->state;
			}

			// Copy the callback if needed
			if (configuration & UI_CONFIG_COMBO_BOX_CALLBACK) {
				const UIConfigComboBoxCallback* callback = (const UIConfigComboBoxCallback*)config.GetParameter(UI_CONFIG_COMBO_BOX_CALLBACK);
				if (callback->handler.data_size > 0 && !callback->copy_on_initialization) {
					memcpy(data->callback_data, callback->handler.data, callback->handler.data_size);
				}
				data->callback = callback->handler.action;
			}

			// Copy the mappings if needed
			if (configuration & UI_CONFIG_COMBO_BOX_MAPPING) {
				const UIConfigComboBoxMapping* mappings = (const UIConfigComboBoxMapping*)config.GetParameter(UI_CONFIG_COMBO_BOX_MAPPING);
				if (!mappings->stable) {
					size_t copy_size = data->labels.size * mappings->byte_size;
					if (data->labels.size != data->mapping_capacity) {
						if (data->mapping_capacity > 0) {
							// Allocate a new buffer
							RemoveAllocation(data->mappings);
						}
						data->mappings = GetMainAllocatorBuffer(copy_size);
					}
					memcpy(data->mappings, mappings->mappings, copy_size);
					data->mapping_byte_size = mappings->byte_size;
				}
			}
			else {
				if (data->mapping_capacity > 0) {
					RemoveAllocation(data->mappings);
					data->mappings = nullptr;
					data->mapping_byte_size = 0;
					data->mapping_capacity = 0;
				}
			}

			// Copy the unavailables if needed
			if (configuration & UI_CONFIG_COMBO_BOX_UNAVAILABLE) {
				const UIConfigComboBoxUnavailable* unavailables = (const UIConfigComboBoxUnavailable*)config.GetParameter(UI_CONFIG_COMBO_BOX_UNAVAILABLE);
				if (!unavailables->stable) {
					size_t copy_size = data->labels.size * sizeof(bool);
					if (data->labels.size != data->unavailables_capacity) {
						if (data->unavailables != nullptr && data->unavailables_capacity > 0) {
							RemoveAllocation(data->unavailables);
						}
						data->unavailables = (bool*)GetMainAllocatorBuffer(copy_size);
					}
					memcpy(data->unavailables, unavailables->unavailables, copy_size);
				}
			}
			else {
				if (data->unavailables_capacity > 0) {
					RemoveAllocation(data->unavailables);
					data->unavailables = nullptr;
					data->unavailables_capacity = 0;
				}
			}

			float2 element_scale = scale;
			if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				// Reduce the scale
				if (~configuration & UI_CONFIG_COMBO_BOX_NO_NAME) {
					scale.x -= ElementNameSize(configuration, config, &data->name, scale);
				}
			}
			else if (~configuration & UI_CONFIG_COMBO_BOX_NO_NAME) {
				if (~configuration & UI_CONFIG_COMBO_BOX_NAME_WITH_SCALE) {
					element_scale.x += layout.element_indentation + data->name.scale.x;
				}
			}

			float old_x_scale = scale.x;
			scale.x = std::max(scale.x, min_value);
			if (configuration & UI_CONFIG_COMBO_BOX_NAME_WITH_SCALE) {
				float difference = scale.x - min_value;
				if (difference > 0.0f) {
					difference = std::min(difference, layout.element_indentation + data->name.scale.x);
					scale.x -= difference;
				}
			}
			if (scale.x != old_x_scale) {
				element_scale.x += scale.x - old_x_scale;
			}

			GetElementAlignedPosition(configuration, config, position, element_scale);

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = element_scale;
			}

			AlignToRowY(this, configuration, position, scale);

			struct ReturnToDefaultData {
				UIDrawerComboBox* combo_box;
				UIConfigComboBoxDefault default_value;
			};

			auto return_to_default_data = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				ReturnToDefaultData* data = (ReturnToDefaultData*)_data;
				if (mouse->IsPressed(ECS_MOUSE_RIGHT)) {
					if (data->combo_box->mapping_byte_size > 0) {
						// Find the index of the mapping
						size_t mapping_index = SearchBytesEx(
							data->combo_box->mappings, 
							data->combo_box->mapping_capacity, 
							data->default_value.mapping_value, 
							data->combo_box->mapping_byte_size
						);
						ECS_ASSERT(mapping_index != -1);

						if ((unsigned char)mapping_index != *data->combo_box->active_label) {
							// The value needs to be changed
							*data->combo_box->active_label = mapping_index;
							action_data->redraw_window = true;

							if (data->combo_box->callback != nullptr) {
								action_data->data = data->combo_box->callback_data;
								data->combo_box->callback(action_data);
							}
						}
					}
					else {
						unsigned char value = 0;
						if (data->default_value.is_pointer_value) {
							value = *data->default_value.char_pointer;
						}
						else {
							value = data->default_value.char_value;
						}

						if (value != *data->combo_box->active_label) {
							*data->combo_box->active_label = value;
							action_data->redraw_window = true;

							if (data->combo_box->callback != nullptr) {
								action_data->data = data->combo_box->callback_data;
								data->combo_box->callback(action_data);
							}
						}
					}
				}
			};

			auto name = [&](float2& name_position) {
				size_t name_configuration = configuration;
				const UIDrawConfig* config_to_use = &config;

				UIDrawConfig name_config;
				ReturnToDefaultData return_data;

				if (configuration & UI_CONFIG_COMBO_BOX_DEFAULT) {
					const UIConfigComboBoxDefault* default_value = (const UIConfigComboBoxDefault*)config.GetParameter(UI_CONFIG_COMBO_BOX_DEFAULT);

					ECS_UI_DRAW_PHASE draw_phase = ECS_UI_DRAW_NORMAL;
					if (configuration & UI_CONFIG_COMBO_BOX_CALLBACK) {
						const UIConfigComboBoxCallback* combo_callback = (const UIConfigComboBoxCallback*)config.GetParameter(UI_CONFIG_COMBO_BOX_CALLBACK);
						draw_phase = combo_callback->handler.phase;
					}

					return_data.default_value = *default_value;
					return_data.combo_box = data;

					if (data->mapping_byte_size > 0 && !default_value->mapping_value_stable) {
						return_data.default_value.mapping_value = GetHandlerBuffer(data->mapping_byte_size, draw_phase);
						memcpy(return_data.default_value.mapping_value, default_value->mapping_value, data->mapping_byte_size);
					}

					UIConfigElementNameAction name_action;
					name_action.hoverable_handler = { 
						return_to_default_data, 
						&return_data, 
						(unsigned int)sizeof(return_data), 
						draw_phase
					};
					name_config.AddFlag(name_action);
					
					name_configuration |= UI_CONFIG_ELEMENT_NAME_ACTION;
					config_to_use = &name_config;
				}

				ElementName(name_configuration, *config_to_use, &data->name, name_position, scale);
			};

			if (IsElementNameFirst(configuration, UI_CONFIG_COMBO_BOX_NO_NAME)) {
				name(position);
			}

			float2 border_position = position;
			float2 border_scale = scale;

			float prefix_scale = 0.0f;
			if (ValidatePosition(configuration, position, scale)) {
				size_t new_configuration = ClearFlag(
					configuration,
					UI_CONFIG_BORDER, 
					UI_CONFIG_GET_TRANSFORM,
					UI_CONFIG_ALIGN_TO_ROW_Y, 
					UI_CONFIG_DO_CACHE
				) | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_TEXT_ALIGNMENT;
				new_configuration = ClearFlag(new_configuration, UI_CONFIG_ALIGN_ELEMENT);

				new_configuration |= is_active ? 0 : UI_CONFIG_UNAVAILABLE_TEXT;

				UIConfigTextAlignment previous_alignment;
				UIConfigTextAlignment alignment;
				alignment.horizontal = ECS_UI_ALIGN_LEFT;
				alignment.vertical = ECS_UI_ALIGN_MIDDLE;

				UIDrawConfig internal_config;
				memcpy(&internal_config, &config, sizeof(config));
				SetConfigParameter(configuration, internal_config, alignment, previous_alignment);

				// Use the position.x += scale.x instead of relying on finalizing because
				// if the user doesn't want finalizing then it will incorrectly appear one
				// over the other
				if (configuration & UI_CONFIG_COMBO_BOX_PREFIX) {
					TextLabel(new_configuration, internal_config, data->prefix, position, scale);
					position.x += scale.x - element_descriptor.label_padd.x * 2.0f;
				}
				if (data->labels.size > 0) {
					unsigned char label_to_be_drawn = 0;
					if (configuration & UI_CONFIG_COMBO_BOX_MAPPING) {
						for (unsigned short index = 0; index < data->mapping_capacity; index++) {
							if (memcmp(OffsetPointer(data->mappings, index * data->mapping_byte_size), data->active_label, data->mapping_byte_size) == 0) {
								// Found the label
								label_to_be_drawn = index;
								break;
							}
						}
						// If not found will draw the first one
					}
					else {
						label_to_be_drawn = *data->active_label >= data->labels.size ? data->labels.size - 1 : *data->active_label;
					}
					TextLabel(new_configuration, internal_config, data->labels[label_to_be_drawn], position, scale);
				}
				else {
					is_active = false;
				}

				float2 positions[2];
				float2 scales[2];
				Color colors[2];
				float percentages[2];
				positions[0] = border_position;
				scales[0] = border_scale;

				Color color = HandleColor(configuration, config);
				colors[0] = color;

				size_t no_get_transform_configuration = ClearFlag(configuration, UI_CONFIG_GET_TRANSFORM);

				// Draw the overall solid color here, not the lightened one
				SolidColorRectangle(no_get_transform_configuration, border_position, border_scale, color);

				color = ToneColor(color, 1.35f);
				colors[1] = color;
				percentages[0] = 1.25f;
				percentages[1] = 1.25f;

				float2 triangle_scale = GetSquareScale(scale.y);
				float2 triangle_position = { border_position.x + border_scale.x - triangle_scale.x, border_position.y };
				SolidColorRectangle(no_get_transform_configuration, triangle_position, triangle_scale, color);

				positions[1] = triangle_position;
				scales[1] = triangle_scale;

				if (is_active) {
					SpriteRectangle(no_get_transform_configuration, triangle_position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE);

					UIDrawerComboBoxClickable clickable_data;
					clickable_data.config = internal_config;
					clickable_data.box = data;
					clickable_data.configuration = configuration;
					data->label_y_scale = scale.y;

					AddDefaultHoverable(configuration, border_position, border_scale, positions, scales, colors, percentages, 2, HandlePhase(configuration));
					AddClickable(configuration, border_position, border_scale, { ComboBoxClickable, &clickable_data, sizeof(clickable_data), ECS_UI_DRAW_SYSTEM });
				}
				else {
					SpriteRectangle(no_get_transform_configuration, triangle_position, triangle_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, color_theme.unavailable_text);
				}
				
				HandleBorder(configuration, config, border_position, border_scale);

				RemoveConfigParameter(configuration, internal_config, previous_alignment);
			}

			FinalizeRectangle(configuration, border_position, border_scale);

			if (IsElementNameAfter(configuration, UI_CONFIG_COMBO_BOX_NO_NAME)) {
				position = { border_position.x + border_scale.x, border_position.y };
				name(position);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerColorInput* UIDrawer::ColorInputInitializer(
			size_t configuration,
			UIDrawConfig& config,
			Stream<char> name,
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
			AddWindowResource(name, [&](Stream<char> identifier) {

				data = GetMainAllocatorBuffer<UIDrawerColorInput>();

				InitializeElementName(configuration, UI_CONFIG_COLOR_INPUT_NO_NAME, config, identifier, &data->name, position, scale);
				data->hsv = RGBToHSV(*color);
				data->rgb = color;
				if (~configuration & UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE) {
					data->default_color = *color;
				}
				else {
					data->default_color = default_color;
				}
				size_t slider_configuration = configuration | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
				slider_configuration |= configuration & UI_CONFIG_SLIDER_ENTER_VALUES ? UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK : 0;

				bool rgb = false;
				bool hsv = false;
				bool alpha = false;
				bool hex = false;

				Stream<char> target_window_name = system->GetWindowName(window_index);

				if (configuration & UI_CONFIG_COLOR_INPUT_SLIDERS) {
					const UIConfigColorInputSliders* sliders = (const UIConfigColorInputSliders*)config.GetParameter(UI_CONFIG_COLOR_INPUT_SLIDERS);
					rgb = sliders->rgb;
					hsv = sliders->hsv;
					alpha = sliders->alpha;
					hex = sliders->hex;
					target_window_name = sliders->target_window_name;
				}

				void* target_window_name_allocation = GetMainAllocatorBuffer(target_window_name.MemoryOf(target_window_name.size));
				uintptr_t target_window_name_allocation_ptr = (uintptr_t)target_window_name_allocation;
				target_window_name.InitializeAndCopy(target_window_name_allocation_ptr, target_window_name);
				data->target_window_name = target_window_name;

				if (rgb) {
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
							config.SetExistingFlag(callback, previous_callback);
						}
						else {
							config.AddFlag(callback);
						}
					}

					bool pop_string_pattern = PushIdentifierStackStringPattern();
					PushIdentifierStack(name);

					data->r_slider = IntSliderInitializer(
						slider_configuration,
						config,
						"R:",
						position,
						scale,
						&color->red,
						(unsigned char)0,
						(unsigned char)255
					);

					HandleTransformFlags(configuration, config, position, scale);
					data->g_slider = IntSliderInitializer(
						slider_configuration,
						config,
						"G:",
						position,
						scale,
						&color->green,
						(unsigned char)0,
						(unsigned char)255
					);

					HandleTransformFlags(configuration, config, position, scale);
					data->b_slider = IntSliderInitializer(
						slider_configuration,
						config,
						"B:",
						position,
						scale,
						&color->blue,
						(unsigned char)0,
						(unsigned char)255
					);

					PopIdentifierStack();
					if (pop_string_pattern) {
						PopIdentifierStack();
					}

					if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
							config.RestoreFlag(previous_callback);
						}
						else {
							config.flag_count--;
						}
					}
				}

				if (hsv) {
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
							config.SetExistingFlag(callback, previous_callback);
						}
						else {
							config.AddFlag(callback);
						}
					}

					HandleTransformFlags(configuration, config, position, scale);

					bool pop_string_pattern = PushIdentifierStackStringPattern();
					PushIdentifierStack(name);

					data->h_slider = IntSliderInitializer(
						slider_configuration,
						config,
						"H:",
						position,
						scale,
						&data->hsv.hue,
						(unsigned char)0,
						(unsigned char)255
					);

					HandleTransformFlags(configuration, config, position, scale);
					data->s_slider = IntSliderInitializer(
						slider_configuration,
						config,
						"S:",
						position,
						scale,
						&data->hsv.saturation,
						(unsigned char)0,
						(unsigned char)255
					);

					HandleTransformFlags(configuration, config, position, scale);
					data->v_slider = IntSliderInitializer(
						slider_configuration,
						config,
						"V:",
						position,
						scale,
						&data->hsv.value,
						(unsigned char)0,
						(unsigned char)255
					);

					PopIdentifierStack();
					if (pop_string_pattern) {
						PopIdentifierStack();
					}

					if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
							config.RestoreFlag(previous_callback);
						}
						else {
							config.flag_count--;
						}
					}
				}

				if (alpha) {
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
							config.SetExistingFlag(callback, previous_callback);
						}
						else {
							config.AddFlag(callback);
						}
					}

					HandleTransformFlags(configuration, config, position, scale);

					bool pop_string_pattern = PushIdentifierStackStringPattern();
					PushIdentifierStack(name);

					data->a_slider = IntSliderInitializer(
						slider_configuration,
						config,
						"A:",
						position,
						scale,
						&color->alpha,
						(unsigned char)0,
						(unsigned char)255
					);

					PopIdentifierStack();
					if (pop_string_pattern) {
						PopIdentifierStack();
					}

					if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
						if (configuration & UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK) {
							config.RestoreFlag(previous_callback);
						}
						else {
							config.flag_count--;
						}
					}
				}

				if (hex) {
					data->hex_characters.buffer = (char*)GetMainAllocatorBuffer(sizeof(char) * 7);
					data->hex_characters.size = 0;
					data->hex_characters.capacity = 6;

					HandleTransformFlags(configuration, config, position, scale);

					bool pop_string_pattern = PushIdentifierStackStringPattern();
					PushIdentifierStack(name);

					UIConfigTextInputCallback callback;
					callback.handler.action = ColorInputHexCallback;
					callback.handler.data = data;
					callback.handler.data_size = 0;

					config.AddFlag(callback);
					data->hex_input = TextInputInitializer(
						configuration | UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN | UI_CONFIG_TEXT_INPUT_CALLBACK,
						config,
						"Hex",
						&data->hex_characters,
						position,
						scale
					);
					config.flag_count--;

					PopIdentifierStack();
					if (pop_string_pattern) {
						PopIdentifierStack();
					}
				}
				else {
					data->hex_characters.buffer = nullptr;
				}

				if (configuration & UI_CONFIG_COLOR_INPUT_CALLBACK) {
					const UIConfigColorInputCallback* callback = (const UIConfigColorInputCallback*)config.GetParameter(UI_CONFIG_COLOR_INPUT_CALLBACK);

					data->callback = callback->callback;
					data->final_callback = callback->final_callback;

					if (callback->callback.data_size > 0) {
						void* allocation = GetMainAllocatorBuffer(callback->callback.data_size);
						memcpy(allocation, callback->callback.data, callback->callback.data_size);
						data->callback.data = allocation;
					}
					else {
						data->callback.data = callback->callback.data;
					}

					if (callback->final_callback.action != nullptr && callback->final_callback.data_size > 0) {
						void* allocation = GetMainAllocatorBuffer(callback->final_callback.data_size);
						memcpy(allocation, callback->final_callback.data, callback->final_callback.data_size);
						data->final_callback.data = allocation;
					}
				}
				else {
					data->callback.action = nullptr;
					data->final_callback.action = nullptr;
				}

				return data;
				});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ColorInputDrawer(size_t configuration, UIDrawConfig& config, UIDrawerColorInput* data, float2 position, float2 scale, Color* color) {
			data->rgb = color;
			
			Color initial_frame_color = *color;

			UIDrawConfig label_config;

			if (IsElementNameFirst(configuration, UI_CONFIG_COLOR_INPUT_NO_NAME)) {
				ElementName(configuration, config, &data->name, position, scale);
			}

			Color text_color;
			float2 font_size;
			float character_spacing;
			HandleText(configuration, config, text_color, font_size, character_spacing);

			UIConfigTextParameters text_params;
			text_params.color = text_color;
			text_params.character_spacing = character_spacing;
			text_params.size = { font_size.x, font_size.y / zoom_ptr->y };

			UIConfigTextAlignment text_alignment;
			text_alignment.horizontal = ECS_UI_ALIGN_LEFT;
			text_alignment.vertical = ECS_UI_ALIGN_MIDDLE;

			label_config.AddFlags(text_params, text_alignment);

			float2 end_point;
			float2 starting_point;
			float2 overall_start_position = position;
			float2 alpha_endpoint;

			bool is_active = true;
			if (configuration & UI_CONFIG_ACTIVE_STATE) {
				const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				is_active = active_state->state;
			}

			size_t LABEL_CONFIGURATION = UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
			LABEL_CONFIGURATION |= is_active ? 0 : UI_CONFIG_UNAVAILABLE_TEXT;

			size_t SLIDER_CONFIGURATION = configuration | UI_CONFIG_ELEMENT_NAME_FIRST;
			SLIDER_CONFIGURATION |= configuration & UI_CONFIG_SLIDER_ENTER_VALUES ? UI_CONFIG_SLIDER_CHANGED_VALUE_CALLBACK : 0;

			bool rgb = false;
			bool hsv = false;
			bool alpha = false;
			bool hex = false;

			if (configuration & UI_CONFIG_COLOR_INPUT_SLIDERS) {
				const UIConfigColorInputSliders* sliders = (const UIConfigColorInputSliders*)config.GetParameter(UI_CONFIG_COLOR_INPUT_SLIDERS);
				rgb = sliders->rgb;
				hsv = sliders->hsv;
				alpha = sliders->alpha;
				hex = sliders->hex;
			}

			auto callback_lambda = [&](bool is_rgb, bool is_hsv, bool is_alpha) {
				if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
					UIConfigSliderChangedValueCallback callback;

					/*UIDrawerColorInputSliderCallback callback_data;
					callback_data.input = data;
					callback_data.is_rgb = is_rgb;
					callback_data.is_hsv = is_hsv;
					callback_data.is_alpha = is_alpha;

					callback.handler.action = ColorInputSliderCallback;
					callback.handler.data = &callback_data;
					callback.handler.data_size = sizeof(callback_data);*/
					callback.copy_on_initialization = true;

					config.AddFlag(callback);
				}
			};

			auto remove_callback_lambda = [&]() {
				if (configuration & UI_CONFIG_SLIDER_ENTER_VALUES) {
					config.flag_count--;
				}
			};

			auto rgb_lambda = [&]() {
				callback_lambda(true, false, false);

				starting_point = position;
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->r_slider, position, scale, &color->red, (unsigned char)0, (unsigned char)255);
				position.x = GetCurrentPosition().x;

				Indent(-1.0f);
				position.x -= layout.element_indentation;
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->g_slider, position, scale, &color->green, (unsigned char)0, (unsigned char)255);
				position.x = GetCurrentPosition().x;

				Indent(-1.0f);
				position.x -= layout.element_indentation;
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->b_slider, position, scale, &color->blue, (unsigned char)0, (unsigned char)255);
				position.x = current_x - region_render_offset.x - layout.element_indentation;
				end_point = { current_x - region_render_offset.x - layout.element_indentation, position.y };

				remove_callback_lambda();
			};

			auto hsv_lambda = [&]() {
				callback_lambda(false, true, false);

				if (rgb) {
					NextRow();
					SetCurrentX(overall_start_position.x + region_render_offset.x);
					position = GetCurrentPosition();
				}

				starting_point = position;
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->h_slider, position, scale, &data->hsv.hue, (unsigned char)0, (unsigned char)255);
				position.x = GetCurrentPosition().x;

				Indent(-1.0f);
				position.x -= layout.element_indentation;
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->s_slider, position, scale, &data->hsv.saturation, (unsigned char)0, (unsigned char)255);
				position.x = GetCurrentPosition().x;

				Indent(-1.0f);
				position.x -= layout.element_indentation;
				alpha_endpoint = { position.x + scale.x, position.y };
				IntSliderDrawer(SLIDER_CONFIGURATION, config, data->v_slider, position, scale, &data->hsv.value, (unsigned char)0, (unsigned char)255);
				position.x = GetCurrentPosition().x;

				if (rgb) {
					if (data->r_slider->interpolate_value || data->g_slider->interpolate_value || data->b_slider->interpolate_value) {
						data->hsv = RGBToHSV(*color);
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

				if (!rgb && !hsv) {
					HandleTransformFlags(configuration, config, position, scale);
					IntSliderDrawer(SLIDER_CONFIGURATION, config, data->a_slider, position, scale, &color->alpha, (unsigned char)0, (unsigned char)255);
					position.x = GetCurrentPosition().x;
				}
				else {
					NextRow();
					SetCurrentX(overall_start_position.x + region_render_offset.x);

					HandleTransformFlags(configuration, config, position, scale);
					scale.x = alpha_endpoint.x - starting_point.x - layout.element_indentation - system->NormalizeHorizontalToWindowDimensions(scale.y);
					IntSliderDrawer(SLIDER_CONFIGURATION, config, data->a_slider, position, scale, &color->alpha, (unsigned char)0, (unsigned char)255);
					scale.x = GetCurrentPosition().x;
				}

				if (data->a_slider->interpolate_value) {
					data->hsv.alpha = color->alpha;
				}

				remove_callback_lambda();
			};

			auto hex_lambda = [&]() {
				if (rgb || hsv || alpha) {
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
					ECS_STACK_CAPACITY_STREAM(char, hex_characters, 10);
					RGBToHex(hex_characters, *data->rgb);
					data->hex_input->DeleteAllCharacters();
					data->hex_input->InsertCharacters(hex_characters.buffer, 6, 0, GetSystem());
				}

				TextInputDrawer(
					configuration | UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_TEXT_INPUT_CALLBACK,
					config,
					data->hex_input,
					position,
					{ end_point.x - overall_start_position.x, scale.y },
					UIDrawerTextInputHexFilter
				);
				NextRow();
			};

			if (rgb) {
				rgb_lambda();
			}

			if (hsv) {
				hsv_lambda();
			}

			if (alpha) {
				alpha_lambda();
			}

			if (~configuration & UI_CONFIG_COLOR_INPUT_DO_NOT_CHOOSE_COLOR) {
				position = GetCurrentPosition();
			}

			size_t rectangle_configuration = configuration | UI_CONFIG_MAKE_SQUARE;
			bool is_name_after = IsElementNameAfter(configuration, UI_CONFIG_COLOR_INPUT_NO_NAME);
			rectangle_configuration |= is_name_after ? UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW : 0;
			AlphaColorRectangle(rectangle_configuration, config, *color);

			if (is_active) {
				if (~configuration & UI_CONFIG_COLOR_INPUT_DO_NOT_CHOOSE_COLOR) {
					AddDefaultClickable(configuration, position, GetSquareScale(scale.y), { SkipAction, nullptr, 0 }, { ColorInputCreateWindow, data, 0, ECS_UI_DRAW_SYSTEM });
				}
				if (configuration & UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE) {
					AddHoverable(configuration, position, GetSquareScale(scale.y), { ColorInputDefaultColor, data, 0 });
				}
			}

			if (is_name_after) {
				position = GetCurrentPosition();
				ElementName(configuration, config, &data->name, position, scale);
			}

			if (hex) {
				hex_lambda();
			}

		}

		UIDrawerCollapsingHeader* UIDrawer::CollapsingHeaderInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, float2 position, float2 scale) {
			UIDrawerCollapsingHeader* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {
				data = GetMainAllocatorBuffer<UIDrawerCollapsingHeader>();

				if (IsElementNameAfter(configuration, UI_CONFIG_COLLAPSING_HEADER_NO_NAME) || IsElementNameFirst(configuration, UI_CONFIG_COLLAPSING_HEADER_NO_NAME)) {
					ConvertTextToWindowResource(configuration, config, identifier, &data->name, position, scale);
				}

				data->state = false;
				return data;
				});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Data>
		void CollapsingHeaderDrawerImpl(UIDrawer* drawer, size_t configuration, UIDrawConfig& config, Stream<char> name, Data* data, float2 position, float2 scale) {
			if (~configuration & UI_CONFIG_COLLAPSING_HEADER_DO_NOT_INFER) {
				scale.x = drawer->GetXScaleUntilBorder(position.x);
			}

			float total_x_scale = 0.0f;
			float initial_position_x = position.x;

			if (drawer->ValidatePosition(configuration, position, scale)) {
				bool active_state = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* _active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					active_state = _active_state->state;
				}
				bool state = false;
				if constexpr (std::is_same_v<Data, UIDrawerCollapsingHeader>) {
					state = data->state;
				}
				else {
					state = *data;
				}
				state &= active_state;

				bool is_selected = true;
				if (configuration & UI_CONFIG_COLLAPSING_HEADER_SELECTION) {
					const UIConfigCollapsingHeaderSelection* selection = (const UIConfigCollapsingHeaderSelection*)config.GetParameter(UI_CONFIG_COLLAPSING_HEADER_SELECTION);
					is_selected = selection->is_selected;
				}

				float2 square_scale = drawer->GetSquareScale(scale.y);
				Color current_color = drawer->HandleColor(configuration, config);

				// Use a small indentation in between the buttons
				const float BUTTON_INDENTATION = 0.002f;

				UIDrawConfig button_config;
				size_t user_button_configuration = UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_VALIDATE_POSITION | UI_CONFIG_DO_NOT_FIT_SPACE
					| UI_CONFIG_COLOR | UI_CONFIG_LATE_DRAW;
				// Change the current color to a different hue such that the element can be easier seen
				UIConfigColor hued_color;
				hued_color.color = ChangeHue(current_color, -0.65f);
				button_config.AddFlag(hued_color);

				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					button_config.AddFlag(*(const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE));
					user_button_configuration |= UI_CONFIG_ACTIVE_STATE;
				}
				size_t button_config_base_size = button_config.flag_count;

				auto draw_user_button = [&](float2& position, float2 square_scale, const UIConfigCollapsingHeaderButton* button) {
					const float REDUCE_FACTOR = 0.7f;
					float2 user_button_scale = square_scale * float2(REDUCE_FACTOR, REDUCE_FACTOR);
					float2 offseted_position = position + drawer->region_render_offset;

					button_config.flag_count = button_config_base_size;

					switch (button->type) {
					case ECS_UI_COLLAPSING_HEADER_BUTTON_CHECK_BOX:
					{
						size_t configuration = user_button_configuration | UI_CONFIG_CHECK_BOX_NO_NAME;
						if (button->handler.action != nullptr) {
							UIConfigCheckBoxCallback check_box_callback;
							check_box_callback.handler = button->handler;
							configuration |= UI_CONFIG_CHECK_BOX_CALLBACK;
							button_config.AddFlag(check_box_callback);
						}

						float2 check_box_position = AlignMiddle(offseted_position, square_scale, user_button_scale);

						// The name doesn't need to be specified
						drawer->CheckBoxDrawer(configuration, button_config, "", button->data.check_box_flag, check_box_position, user_button_scale);
					}
					break;
					case ECS_UI_COLLAPSING_HEADER_BUTTON_IMAGE_BUTTON:
					{
						size_t configuration = user_button_configuration | UI_CONFIG_ABSOLUTE_TRANSFORM;
						UIConfigAbsoluteTransform absolute_transform;
						absolute_transform.scale = user_button_scale;
						absolute_transform.position = AlignMiddle(offseted_position, square_scale, user_button_scale);
						button_config.AddFlag(absolute_transform);

						drawer->SpriteButton(configuration, button_config, button->handler, button->data.image_texture, button->data.image_color);
					}
					break;
					case ECS_UI_COLLAPSING_HEADER_BUTTON_IMAGE_DISPLAY:
					{
						size_t configuration = user_button_configuration;
						float2 rectangle_position = AlignMiddle(offseted_position, square_scale, user_button_scale);
						drawer->SpriteRectangle(configuration, rectangle_position, user_button_scale, button->data.image_texture, button->data.image_color);
					}
					break;
					case ECS_UI_COLLAPSING_HEADER_BUTTON_MENU:
					{
						size_t configuration = user_button_configuration | UI_CONFIG_MENU_SPRITE | UI_CONFIG_ABSOLUTE_TRANSFORM;
						UIConfigMenuSprite sprite;
						sprite.color = button->data.menu_texture_color;
						sprite.texture = button->data.menu_texture;
						button_config.AddFlag(sprite);

						UIConfigAbsoluteTransform transform;
						transform.position = AlignMiddle(offseted_position, square_scale, user_button_scale);
						transform.scale = user_button_scale;
						button_config.AddFlag(transform);

						if (button->data.menu_copy_states) {
							configuration |= UI_CONFIG_MENU_COPY_STATES;
						}

						// We need to construct a unique name
						bool pushed_string = drawer->PushIdentifierStackStringPattern();
						drawer->PushIdentifierStackEx(name);
						drawer->Menu(configuration, button_config, "__Menu", button->data.menu_state);
						drawer->PopIdentifierStack();
						if (pushed_string) {
							drawer->PopIdentifierStack();
						}
					}
					break;
					case ECS_UI_COLLAPSING_HEADER_BUTTON_MENU_GENERAL:
					{
						size_t configuration = user_button_configuration | UI_CONFIG_MENU_BUTTON_SPRITE;
						UIConfigMenuButtonSprite sprite;
						sprite.color = button->data.menu_general_texture_color;
						sprite.texture = button->data.menu_general_texture;
						config.AddFlag(sprite);

						UIConfigAbsoluteTransform transform;
						transform.position = AlignMiddle(offseted_position, square_scale, user_button_scale);
						transform.scale = user_button_scale;
						config.AddFlag(transform);

						// We don't need to construct a name
						drawer->MenuButton(configuration, button_config, { nullptr, 0 }, *button->data.menu_general_descriptor, button->data.border_flags);
					}
					break;
					default:
						ECS_ASSERT(false, "Invalid collapsing header button type");
					}

					position.x += square_scale.x + BUTTON_INDENTATION;
				};

				const UIConfigCollapsingHeaderButtons* header_buttons = nullptr;
				if (configuration & UI_CONFIG_COLLAPSING_HEADER_BUTTONS) {
					header_buttons = (const UIConfigCollapsingHeaderButtons*)config.GetParameter(UI_CONFIG_COLLAPSING_HEADER_BUTTONS);
				}

				UIConfigColor previous_color = { current_color };
				float2 initial_position = position;

				UIReservedHandler reserved_hoverable;
				UIReservedHandler reserved_clickable;
				// Reserve the default clickable for the whole label. Because we want the buttons to appear over it
				if (active_state) {
					ECS_UI_DRAW_PHASE phase = drawer->HandlePhase(configuration);
					reserved_hoverable = drawer->ReserveHoverable(phase);
					reserved_clickable = drawer->ReserveClickable(ECS_MOUSE_LEFT, phase);
				}

				if (is_selected) {
					UIConfigColor darkened_color = { DarkenColor(current_color, drawer->color_theme.darken_inactive_item) };
					if (active_state) {
						darkened_color = previous_color;
					}
					SetConfigParameter(configuration, config, darkened_color, previous_color);

					// Draw this rectangle for the whole label in case there are buttons which have gaps in between
					drawer->SolidColorRectangle(configuration | UI_CONFIG_COLOR, config, position, scale);
				}

				if (state) {
					drawer->SpriteRectangle(configuration, position, square_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, drawer->color_theme.text);
				}
				else {
					drawer->SpriteRectangle(configuration, position, square_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, drawer->color_theme.text, { 1.0f, 0.0f }, { 0.0f, 1.0f });
				}

				position.x += square_scale.x;
				total_x_scale += square_scale.x;
				scale.x -= square_scale.x;

				if (header_buttons != nullptr) {
					// Draw all the left buttons
					position.x += BUTTON_INDENTATION;
					scale.x -= BUTTON_INDENTATION;
					for (size_t index = 0; index < header_buttons->buttons.size; index++) {
						if (header_buttons->buttons[index].alignment == ECS_UI_ALIGN_LEFT) {
							draw_user_button(position, square_scale, header_buttons->buttons.buffer + index);
							total_x_scale += square_scale.x + BUTTON_INDENTATION;
						}
						scale.x -= square_scale.x + BUTTON_INDENTATION;
					}
				}

				if (~configuration & UI_CONFIG_COLLAPSING_HEADER_NO_NAME) {
					UIConfigTextAlignment previous_alignment;
					UIConfigTextAlignment alignment;
					alignment.horizontal = ECS_UI_ALIGN_LEFT;
					alignment.vertical = ECS_UI_ALIGN_MIDDLE;

					SetConfigParameter(configuration, config, alignment, previous_alignment);

					float label_scale = scale.x;
					float name_scale = 0.0f;
					if constexpr (std::is_same_v<Data, UIDrawerCollapsingHeader>) {
						name_scale = data->name.scale.x + 2 * drawer->element_descriptor.label_padd.x;
					}
					else {
						name_scale = drawer->GetLabelScale({ name.buffer, ParseStringIdentifier(name) }).x;
					}
					label_scale = ClampMin(label_scale, name_scale);

					size_t LABEL_CONFIGURATION = ClearFlag(configuration, UI_CONFIG_DO_CACHE) | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE 
						| UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_ADVANCE;
					LABEL_CONFIGURATION |= is_selected ? UI_CONFIG_COLOR : UI_CONFIG_LABEL_TRANSPARENT;

					float2 new_scale = { label_scale, scale.y };
					if constexpr (std::is_same_v<Data, UIDrawerCollapsingHeader>) {
						drawer->TextLabelDrawer(
							LABEL_CONFIGURATION,
							config,
							&data->name,
							position,
							new_scale
						);
					}
					else {
						drawer->TextLabel(
							LABEL_CONFIGURATION,
							config,
							name,
							position,
							new_scale
						);
					}

					total_x_scale += label_scale;
					position.x += label_scale;

					drawer->UpdateCurrentRowScale(scale.y);
					drawer->UpdateRenderBoundsRectangle(position, { 0.0f, scale.y });

					RemoveConfigParameter(configuration, config, previous_alignment);
				}

				if (is_selected) {
					RemoveConfigParameter(configuration, config, previous_color);
				}

				// Draw the remaining right buttons
				if (header_buttons != nullptr) {
					position.x += BUTTON_INDENTATION;
					total_x_scale += BUTTON_INDENTATION;
					for (size_t index = 0; index < header_buttons->buttons.size; index++) {
						if (header_buttons->buttons[index].alignment == ECS_UI_ALIGN_RIGHT) {
							draw_user_button(position, square_scale, header_buttons->buttons.buffer + index);
							total_x_scale += square_scale.x + BUTTON_INDENTATION;
						}
					}
				}

				if (active_state) {
					size_t click_data_storage[256];
					UIDrawerBoolClickableWithPinCallbackData* click_data = (UIDrawerBoolClickableWithPinCallbackData*)click_data_storage;
					if constexpr (std::is_same_v<Data, UIDrawerCollapsingHeader>) {
						click_data->base.pointer = &data->state;
					}
					else {
						click_data->base.pointer = data;
					}
					click_data->base.is_vertical = true;

					UIActionHandler clickable_handler = { BoolClickableWithPin, &click_data->base, sizeof(click_data->base) };
					if (configuration & UI_CONFIG_COLLAPSING_HEADER_CALLBACK) {
						const UIConfigCollapsingHeaderCallback* callback = (const UIConfigCollapsingHeaderCallback*)config.GetParameter(UI_CONFIG_COLLAPSING_HEADER_CALLBACK);
						click_data->handler = callback->handler;
						if (click_data->handler.data_size > 0) {
							memcpy(OffsetPointer(click_data, sizeof(*click_data)), click_data->handler.data, click_data->handler.data_size);
						}
						clickable_handler = { BoolClickableWithPinCallback, click_data, sizeof(*click_data) + click_data->handler.data_size, click_data->handler.phase };
					}

					drawer->AddDefaultClickableHoverable(
						reserved_hoverable, 
						reserved_clickable, 
						initial_position,
						{ total_x_scale, scale.y }, 
						clickable_handler,
						nullptr,
						current_color
					);
				}
			}
			else {
				drawer->UpdateCurrentRowScale(scale.y);
				drawer->UpdateRenderBoundsRectangle(position, { 0.0f, scale.y });
			}

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = { initial_position_x, position.y };
				*get_transform->scale = { total_x_scale, scale.y };
			}

			drawer->NextRow();
		}

		void UIDrawer::CollapsingHeaderDrawer(size_t configuration, UIDrawConfig& config, Stream<char> name, UIDrawerCollapsingHeader* data, float2 position, float2 scale) {
			CollapsingHeaderDrawerImpl(this, configuration, config, name, data, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::CollapsingHeaderDrawer(size_t configuration, UIDrawConfig& config, Stream<char> name, bool* state, float2 position, float2 scale) {
			CollapsingHeaderDrawerImpl(this, configuration, config, name, state, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextInput* UIDrawer::FloatInputInitializer(
			size_t configuration,
			UIDrawConfig& config,
			Stream<char> name,
			float* number,
			float default_value,
			float min,
			float max,
			float2 position,
			float2 scale
		) {
			UIDrawerFloatInputCallbackData callback_data;
			if (configuration & UI_CONFIG_NUMBER_INPUT_RANGE) {
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
					ConvertFloatToChars(*stream, *number, 3);
				},
				[min, max](Stream<char>& tool_tip_stream) {
					ConvertFloatToChars(tool_tip_stream, min, 3);
					tool_tip_stream.Add(',');
					tool_tip_stream.Add(' ');
					ConvertFloatToChars(tool_tip_stream, max, 3);
				}
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextInput* UIDrawer::DoubleInputInitializer(
			size_t configuration,
			UIDrawConfig& config,
			Stream<char> name,
			double* number,
			double default_value,
			double min,
			double max,
			float2 position,
			float2 scale
		) {
			UIDrawerDoubleInputCallbackData callback_data;
			if (configuration & UI_CONFIG_NUMBER_INPUT_RANGE) {
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
					ConvertDoubleToChars(*stream, *number, 3);
				},
				[min, max](Stream<char>& tool_tip_stream) {
					ConvertDoubleToChars(tool_tip_stream, min, 3);
					tool_tip_stream.Add(',');
					tool_tip_stream.Add(' ');
					ConvertDoubleToChars(tool_tip_stream, max, 3);
				}
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		UIDrawerTextInput* UIDrawer::IntInputInitializer(
			size_t configuration,
			UIDrawConfig& config,
			Stream<char> name,
			Integer* number,
			Integer default_value,
			Integer min,
			Integer max,
			float2 position,
			float2 scale
		) {
			UIDrawerIntegerInputCallbackData<Integer> callback_data;
			if (configuration & UI_CONFIG_NUMBER_INPUT_RANGE) {
				callback_data.max = max;
				callback_data.min = min;
			}
			else {
				IntegerRange(callback_data.min, callback_data.max);
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
					ConvertIntToChars(*stream, static_cast<int64_t>(*number));
				},
				[min, max](Stream<char>& tool_tip_stream) {
					ConvertIntToChars(tool_tip_stream, static_cast<int64_t>(min));
					tool_tip_stream.Add(',');
					tool_tip_stream.Add(' ');
					ConvertIntToChars(tool_tip_stream, static_cast<int64_t>(max));
				}
			);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(UIDrawerTextInput*, UIDrawer::IntInputInitializer, size_t, UIDrawConfig&, Stream<char>, integer*, integer, integer, integer, float2, float2);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatInputDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, float* number, float min, float max, float2 position, float2 scale) {
			UIDrawerFloatInputDragData drag_data;
			drag_data.callback_data.number = number;
			drag_data.callback_on_release = false;
			if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
				const UIConfigTextInputCallback* callback = (const UIConfigTextInputCallback*)config.GetParameter(UI_CONFIG_TEXT_INPUT_CALLBACK);
				drag_data.callback_on_release = callback->trigger_only_on_release;
			}
			if (configuration & UI_CONFIG_NUMBER_INPUT_RANGE) {
				drag_data.callback_data.min = min;
				drag_data.callback_data.max = max;
			}
			else {
				drag_data.callback_data.min = -FLT_MAX;
				drag_data.callback_data.max = FLT_MAX;
			}

			NumberInputDrawer(configuration, config, name, number, FloatInputHoverable, FloatInputNoNameHoverable, FloatInputDragValue,
				&drag_data, sizeof(drag_data), position, scale, [&](UIDrawerTextInput* input, Stream<char> tool_tip_characters) {
					struct RunnableData {
						UIDrawerTextInput* input;
						bool callback_on_release;
					};

					RunnableData runnable_data = { input, drag_data.callback_on_release };
					SnapshotRunnable(&runnable_data, sizeof(runnable_data), ECS_UI_DRAW_NORMAL, [](void* _data, ActionData* action_data) {
						RunnableData* runnable_data = (RunnableData*)_data;
						UIDrawerTextInput* input = runnable_data->input;
						UIDrawerFloatInputCallbackData* data = (UIDrawerFloatInputCallbackData*)input->callback_data;

						ECS_STACK_CAPACITY_STREAM(char, temp_stream, 256);
						float EPSILON = 1.0f;

						bool return_value = false;
						if (!IsFloatingPointNumber(*input->text) && !input->is_currently_selected) {
							input->DeleteAllCharacters();
							ConvertFloatToChars(temp_stream, Clamp(0.0f, data->min, data->max));
							input->InsertCharacters(temp_stream.buffer, temp_stream.size, 0, action_data->system);
							temp_stream.size = 0;

							return_value = true;
						}

						unsigned int digit_count = 0;
						Stream<char> dot = FindFirstCharacter(*input->text, '.');
						if (dot.size > 0) {
							digit_count = dot.size - 1;
						}

						for (unsigned int digit_index = 0; digit_index < digit_count; digit_index++) {
							EPSILON *= 0.1f;
						}

						// If the value changed, update the input stream
						float current_value = ConvertCharactersToFloat(*input->text);
						if (!input->is_currently_selected) {
							float difference = abs(current_value - *data->number);

							auto has_changed_action = [&]() {
								input->DeleteAllCharacters();
								ConvertFloatToChars(temp_stream, *data->number, 3);
								input->InsertCharacters(temp_stream.buffer, temp_stream.size, 0, action_data->system);
								data->number_data.external_value_change = true;

								if (runnable_data->callback_on_release && !action_data->mouse->IsReleased(ECS_MOUSE_LEFT)) {
									input->trigger_callback = UIDrawerTextInput::TRIGGER_CALLBACK_NONE;
								}
							};

							if (isnan(difference)) {
								bool current_nan = isnan(current_value);
								bool number_nan = isnan(*data->number);
								if ((!current_nan && number_nan) || (current_nan && !number_nan)) {
									has_changed_action();
									return_value = true;
								}
							}
							else if (difference > EPSILON || digit_count != 3) {
								has_changed_action();
								return_value = true;
							}
						}
						else {
							// If the input is selected, disable the external value change flag
							data->number_data.external_value_change = false;
						}

						return return_value;
					});
					
					
					if (configuration & UI_CONFIG_NUMBER_INPUT_RANGE) {
						UIDrawerFloatInputCallbackData* data = (UIDrawerFloatInputCallbackData*)input->callback_data;
						bool is_different = data->max != max || data->min != min;
						data->max = max;
						data->min = min;

						if (is_different) {
							tool_tip_characters.size = 0;
							tool_tip_characters.Add('[');
							ConvertFloatToChars(tool_tip_characters, min, 3);
							tool_tip_characters.Add(',');
							tool_tip_characters.Add(' ');
							ConvertFloatToChars(tool_tip_characters, max, 3);
							tool_tip_characters.Add(']');
							tool_tip_characters[tool_tip_characters.size] = '\0';
						}

					}
			});
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleInputDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, double* number, double min, double max, float2 position, float2 scale) {
			UIDrawerDoubleInputDragData drag_data;
			drag_data.callback_on_release = false;
			if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
				const UIConfigTextInputCallback* callback = (const UIConfigTextInputCallback*)config.GetParameter(UI_CONFIG_TEXT_INPUT_CALLBACK);
				drag_data.callback_on_release = callback->trigger_only_on_release;
			}
			drag_data.callback_data.number = number;
			if (configuration & UI_CONFIG_NUMBER_INPUT_RANGE) {
				drag_data.callback_data.min = min;
				drag_data.callback_data.max = max;
			}
			else {
				drag_data.callback_data.min = -DBL_MAX;
				drag_data.callback_data.max = DBL_MAX;
			}

			NumberInputDrawer(configuration, config, name, number, DoubleInputHoverable, DoubleInputNoNameHoverable, DoubleInputDragValue,
				&drag_data, sizeof(drag_data), position, scale, [&](UIDrawerTextInput* input, Stream<char> tool_tip_characters) {
					struct RunnableData {
						UIDrawerTextInput* input;
						bool callback_on_release;
					};

					RunnableData runnable_data = { input, drag_data.callback_on_release };
					SnapshotRunnable(&runnable_data, sizeof(runnable_data), ECS_UI_DRAW_NORMAL, [](void* _data, ActionData* action_data) {
						RunnableData* runnable_data = (RunnableData*)_data;
						UIDrawerTextInput* input = runnable_data->input;

						double EPSILON = 1.0;

						ECS_STACK_CAPACITY_STREAM(char, temp_stream, 256);
						UIDrawerDoubleInputCallbackData* data = (UIDrawerDoubleInputCallbackData*)input->callback_data;

						bool return_value = false;
						if (!IsFloatingPointNumber(*input->text) && !input->is_currently_selected) {
							input->DeleteAllCharacters();
							ConvertDoubleToChars(temp_stream, Clamp(0.0, data->min, data->max), 3);
							input->InsertCharacters(temp_stream.buffer, temp_stream.size, 0, action_data->system);
							temp_stream.size = 0;

							return_value = true;
						}

						unsigned int digit_count = 0;
						Stream<char> dot = FindFirstCharacter(*input->text, '.');
						if (dot.size > 0) {
							digit_count = dot.size - 1;
						}

						if (digit_count > 0) {
							EPSILON = pow(0.1, digit_count);
						}

						// If the value changed, update the input stream
						double current_value = ConvertCharactersToDouble(*input->text);
						if (!input->is_currently_selected) {
							double difference = abs(current_value - *data->number);

							auto has_changed_action = [&]() {
								input->DeleteAllCharacters();
								ConvertDoubleToChars(temp_stream, *data->number, 3);
								input->InsertCharacters(temp_stream.buffer, temp_stream.size, 0, action_data->system);
								data->number_data.external_value_change = true;

								if (runnable_data->callback_on_release && !action_data->mouse->IsReleased(ECS_MOUSE_LEFT)) {
									input->trigger_callback = UIDrawerTextInput::TRIGGER_CALLBACK_NONE;
								}
							};

							if (isnan(difference)) {
								bool current_nan = isnan(current_value);
								bool number_nan = isnan(*data->number);
								if ((!current_nan && number_nan) || (current_nan && !number_nan)) {
									has_changed_action();
									return_value = true;
								}
							}
							else if (difference > EPSILON || digit_count != 3) {
								has_changed_action();
								return_value = true;
							}
						}
						else {
							// If the input is selected, disable the external value change flag
							data->number_data.external_value_change = false;
						}

						return return_value;
					});

					if (configuration & UI_CONFIG_NUMBER_INPUT_RANGE) {
						UIDrawerDoubleInputCallbackData* data = (UIDrawerDoubleInputCallbackData*)input->callback_data;
						bool is_different = data->max != max || data->min != min;
						data->max = max;
						data->min = min;

						if (is_different) {
							tool_tip_characters.size = 0;
							tool_tip_characters.Add('[');
							ConvertDoubleToChars(tool_tip_characters, min, 3);
							tool_tip_characters.Add(',');
							tool_tip_characters.Add(' ');
							ConvertDoubleToChars(tool_tip_characters, max, 3);
							tool_tip_characters.Add(']');
							tool_tip_characters[tool_tip_characters.size] = '\0';
						}
					}
				});
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntInputDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, Integer* number, Integer min, Integer max, float2 position, float2 scale) {
			UIDrawerIntInputDragData<Integer> drag_data;
			drag_data.data.number = number;
			drag_data.callback_on_release = false;
			if (configuration & UI_CONFIG_TEXT_INPUT_CALLBACK) {
				const UIConfigTextInputCallback* callback = (const UIConfigTextInputCallback*)config.GetParameter(UI_CONFIG_TEXT_INPUT_CALLBACK);
				drag_data.callback_on_release = callback->trigger_only_on_release;
			}
			if (configuration & UI_CONFIG_NUMBER_INPUT_RANGE) {
				drag_data.data.min = min;
				drag_data.data.max = max;
			}
			else {
				IntegerRange(drag_data.data.min, drag_data.data.max);
			}

			NumberInputDrawer(configuration, config, name, number, IntInputHoverable<Integer>, IntInputNoNameHoverable<Integer>, IntegerInputDragValue<Integer>,
				&drag_data, sizeof(drag_data), position, scale, [&](UIDrawerTextInput* input, Stream<char> tool_tip_characters) {
					struct RunnableData {
						UIDrawerTextInput* input;
						bool callback_on_release;
					};

					RunnableData runnable_data = { input, drag_data.callback_on_release };
					SnapshotRunnable(&runnable_data, sizeof(runnable_data), ECS_UI_DRAW_NORMAL, [](void* _data, ActionData* action_data) {
						RunnableData* runnable_data = (RunnableData*)_data;
						UIDrawerTextInput* input = runnable_data->input;

						ECS_STACK_CAPACITY_STREAM(char, temp_stream, 256);
						UIDrawerIntegerInputCallbackData<Integer>* data = (UIDrawerIntegerInputCallbackData<Integer>*)input->callback_data;

						bool return_value = false;
						if (!IsIntegerNumber(*input->text) && !input->is_currently_selected) {
							input->DeleteAllCharacters();
							ConvertIntToChars(temp_stream, Clamp((Integer)0, data->min, data->max));
							input->InsertCharacters(temp_stream.buffer, temp_stream.size, 0, action_data->system);
							temp_stream.size = 0;

							if (runnable_data->callback_on_release && !action_data->mouse->IsReleased(ECS_MOUSE_LEFT)) {
								input->trigger_callback = UIDrawerTextInput::TRIGGER_CALLBACK_NONE;
							}
							return_value = true;
						}

						// If the value changed, update the input stream
						bool dummy;
						Integer current_value = ConvertCharactersToIntImpl<Integer, char, false>(*input->text, dummy);
						if (!input->is_currently_selected) {
							if (current_value != *data->number) {
								input->DeleteAllCharacters();
								ConvertIntToChars(temp_stream, (int64_t)*data->number);
								input->InsertCharacters(temp_stream.buffer, temp_stream.size, 0, action_data->system);

								if (runnable_data->callback_on_release && !action_data->mouse->IsReleased(ECS_MOUSE_LEFT)) {
									input->trigger_callback = UIDrawerTextInput::TRIGGER_CALLBACK_EXIT;
								}
								return_value = true;
							}
						}

						return return_value;
					});

					if (configuration & UI_CONFIG_NUMBER_INPUT_RANGE) {
						UIDrawerIntegerInputCallbackData<Integer>* data = (UIDrawerIntegerInputCallbackData<Integer>*)input->callback_data;
						bool is_different = data->max != max || data->min != min;
						data->max = max;
						data->min = min;

						if (is_different) {
							tool_tip_characters.size = 0;
							tool_tip_characters.Add('[');
							ConvertIntToChars(tool_tip_characters, static_cast<int64_t>(min));
							tool_tip_characters.Add(',');
							tool_tip_characters.Add(' ');
							ConvertIntToChars(tool_tip_characters, static_cast<int64_t>(max));
							tool_tip_characters.Add(']');
							tool_tip_characters[tool_tip_characters.size] = '\0';
						}
					}
				});
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInputDrawer, size_t, const UIDrawConfig&, Stream<char>, integer*, integer, integer, float2, float2);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatInputGroupDrawer(
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
		) {
			InputGroupDrawerImplementation<float>(this, configuration, config, group_name, count, names, values, lower_bounds, upper_bounds, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatInputGroupInitializer(
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
		) {
			InputGroupInitializerImplementation<float>(this, configuration, config, group_name, count, names, values, default_values, lower_bounds, upper_bounds, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleInputGroupDrawer(
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
		) {
			InputGroupDrawerImplementation<double>(this, configuration, config, group_name, count, names, values, lower_bounds, upper_bounds, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleInputGroupInitializer(
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
		) {
			InputGroupInitializerImplementation<double>(this, configuration, config, group_name, count, names, values, default_values, lower_bounds, upper_bounds, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntInputGroupDrawer(
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
		) {
			InputGroupDrawerImplementation<Integer>(this, configuration, config, group_name, count, names, values, lower_bounds, upper_bounds, position, scale);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInputGroupDrawer, size_t, UIDrawConfig&, Stream<char>, size_t, Stream<char>*, integer**, const integer*, const integer*, float2, float2);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntInputGroupInitializer(
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
		) {
			InputGroupInitializerImplementation<Integer>(this, configuration, config, group_name, count, names, values, default_values, lower_bounds, upper_bounds, position, scale);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInputGroupInitializer, size_t, UIDrawConfig&, Stream<char>, size_t, Stream<char>*, integer**, const integer*, const integer*, const integer*, float2, float2);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HistogramDrawer(size_t configuration, const UIDrawConfig& config, const Stream<float> samples, Stream<char> name, float2 position, float2 scale, unsigned int precision) {
			const size_t STACK_CHARACTER_COUNT = 128;

			float histogram_min_scale = samples.size * element_descriptor.histogram_bar_min_scale + (samples.size - 1) * element_descriptor.histogram_bar_spacing + 2.0f * element_descriptor.histogram_padding.x;
			ClampMin(scale.x, histogram_min_scale);

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
				GetExtremesFromStream(samples, min_value, max_value, [](float value) {
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
				starting_index = starting_index < 0 ? (int64_t)0 : starting_index;
				int64_t end_index = (region_position.x + region_scale.x - histogram_position.x) / bar_scale + 1;
				end_index = end_index > samples.size ? (int64_t)samples.size : end_index;
				histogram_position.x += starting_index * (bar_scale + element_descriptor.histogram_bar_spacing);
				
				for (int64_t index = starting_index; index < end_index; index++) {
					stack_stream.size = 0;
					ConvertFloatToChars(stack_stream, samples[index], precision);
					float2 sample_scale = { bar_scale, samples[index] * difference_inverse * histogram_scale.y };
					float2 sample_position = { histogram_position.x, zero_y_position - sample_scale.y };
					if (sample_scale.y < 0.0f) {
						sample_position.y = zero_y_position;
						sample_scale.y = -sample_scale.y;
					}

					float text_offset = 0.0f;

					float2 current_bar_position = { sample_position.x, histogram_scale.y };
					float2 current_bar_scale = { sample_scale.x, histogram_scale.y };

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
					float2 text_scale = TextSpan({ stack_characters, stack_stream.size }, text_parameters.size, text_parameters.character_spacing);
					if (text_scale.x < sample_scale.x) {
						TextLabel(
							UI_CONFIG_TEXT_PARAMETERS | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_FIT_SPACE
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
					hoverable_data.bar_color = color_theme.histogram_hovered_color;
					AddHoverable(
						configuration, 
						current_bar_position, current_bar_scale, 
						{ HistogramHoverable, &hoverable_data, sizeof(hoverable_data), ECS_UI_DRAW_SYSTEM }
					);
					histogram_position.x += sample_scale.x + element_descriptor.histogram_bar_spacing;
				}
			}

			FinalizeRectangle(configuration, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::MenuDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerMenu* data, float2 position, float2 scale) {
			bool is_active = true;
			if (configuration & UI_CONFIG_ACTIVE_STATE) {
				const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				is_active = active_state->state;
			}

			size_t label_configuration = configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_VALIDATE_POSITION;
			label_configuration = ClearFlag(label_configuration, UI_CONFIG_ALIGN_ELEMENT);
			label_configuration |= is_active ? 0 : UI_CONFIG_UNAVAILABLE_TEXT;
			
			if (~configuration & UI_CONFIG_MENU_SPRITE) {
				float2 label_scale = GetLabelScale(data->name);
				if (~configuration & UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X) {
					scale.x = label_scale.x;
				}
				else {
					label_configuration |= UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X;
				}
			}

			GetElementAlignedPosition(configuration, config, position, scale);			
			HandleFitSpaceRectangle(configuration, position, scale);

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				if (~configuration & UI_CONFIG_MENU_SPRITE) {
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

					if (sprite_definition->texture.size > 0) {
						SpriteRectangle(
							configuration,
							position,
							scale,
							sprite_definition->texture,
							sprite_definition->color,
							sprite_definition->top_left_uv,
							sprite_definition->bottom_right_uv
						);
					}
					else {
						ECS_ASSERT(sprite_definition->resource_view.Interface() != nullptr);
						SpriteRectangle(
							configuration,
							position,
							scale,
							sprite_definition->resource_view,
							sprite_definition->color,
							sprite_definition->top_left_uv,
							sprite_definition->bottom_right_uv
						);
					}

					HandleBorder(configuration, config, position, scale);
				}

				if (is_active) {
					UIDrawerMenuGeneralData general_data;
					general_data.menu = data;
					general_data.menu_initializer_index = 255;

					AddDefaultHoverable(configuration, position, scale, HandleColor(configuration, config));
					AddGeneral(configuration, position, scale, { MenuGeneral, &general_data, sizeof(general_data), ECS_UI_DRAW_SYSTEM });
					AddClickable(configuration, position, scale, { SkipAction, nullptr, 0 });
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
				// Copy their data as well
				for (unsigned int index = 0; index < state->row_count; index++) {
					// Only if there is no submenu on that handler
					if (state->row_has_submenu == nullptr || !state->row_has_submenu[index]) {
						total_memory += state->click_handlers[index].data_size;
					}
				}
			}
			size_t left_character_count = state->left_characters.size;
			total_memory += left_character_count;

			if (state->right_characters.size > 0) {
				size_t right_character_count = state->right_characters.size;
				total_memory += right_character_count;
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
						if (state->unavailables == nullptr || !state->unavailables[index]) {
							total_memory += MenuWalkStatesMemory(&state->submenues[index], copy_states);					
						}
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
			unsigned int submenu_index,
			bool copy_states
		) {
			char* left_buffer = (char*)buffer;
			state->left_characters.CopyTo(buffer);
			state->left_characters.buffer = left_buffer;

			size_t right_character_count = state->right_characters.size;
			if (state->right_characters.size > 0) {
				char* new_buffer = (char*)buffer;
				state->right_characters.CopyTo(buffer);
				state->right_characters.buffer = new_buffer;
			}

			buffer = AlignPointer(buffer, alignof(unsigned short));
			state->left_row_substreams = (unsigned short*)buffer;
			buffer += sizeof(unsigned short) * state->row_count;

			if (state->right_characters.size > 0) {
				state->right_row_substreams = (unsigned short*)buffer;
				buffer += sizeof(unsigned short) * state->row_count;
			}

			size_t new_line_count = 0;
			for (size_t index = 0; index < state->left_characters.size; index++) {
				if (state->left_characters[index] == '\n') {
					state->left_row_substreams[new_line_count++] = index;
				}
			}
			state->left_row_substreams[new_line_count] = state->left_characters.size;

			if (state->right_characters.size > 0) {
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

					buffer = AlignPointer(buffer, alignof(UIDrawerMenuState));
					memcpy((void*)buffer, state->submenues, sizeof(UIDrawerMenuState) * state->row_count);
					state->submenues = (UIDrawerMenuState*)buffer;
					buffer += sizeof(UIDrawerMenuState) * state->row_count;
				}

				if (state->unavailables != nullptr) {
					memcpy((void*)buffer, state->unavailables, sizeof(bool) * state->row_count);
					state->unavailables = (bool*)buffer;
					buffer += sizeof(bool) * state->row_count;
				}

				buffer = AlignPointer(buffer, alignof(UIActionHandler));
				memcpy((void*)buffer, state->click_handlers, sizeof(UIActionHandler) * state->row_count);
				state->click_handlers = (UIActionHandler*)buffer;
				buffer += sizeof(UIActionHandler) * state->row_count;

				// Copy the handler data as well
				for (unsigned int index = 0; index < state->row_count; index++) {
					if (state->click_handlers[index].data_size > 0) {
						bool can_copy = state->row_has_submenu == nullptr || !state->row_has_submenu[index];
						if (can_copy) {
							void* current_buffer = (void*)buffer;
							memcpy(current_buffer, state->click_handlers[index].data, state->click_handlers[index].data_size);
							state->click_handlers[index].data = current_buffer;
							buffer += state->click_handlers[index].data_size;
						}
					}
				}
			}

			state->windows = stream;
			state->submenu_index = submenu_index;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::MenuWalkSetStateBuffers(
			UIDrawerMenuState* state,
			uintptr_t& buffer,
			CapacityStream<UIDrawerMenuWindow>* stream,
			unsigned int submenu_index,
			bool copy_states
		) {
			UIDrawer::MenuSetStateBuffers(state, buffer, stream, submenu_index, copy_states);
			if (state->row_has_submenu != nullptr) {
				for (size_t index = 0; index < state->row_count; index++) {
					if (state->row_has_submenu[index]) {
						if (state->unavailables == nullptr || !state->unavailables[index]) {
							MenuWalkSetStateBuffers(&state->submenues[index], buffer, stream, submenu_index + 1, copy_states);
						}
					}
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// Does not allocate the name or the windows buffer
		void InitializeMenuState(UIDrawer* drawer, UIDrawerMenu* menu_to_fill_in, const UIDrawerMenuState* menu_state_to_copy, bool copy_states) {
			// Allocate the menu windows separately because for the NON_CACHED menu 
			// it will continuously deallocate and initialize
			size_t total_memory = drawer->MenuWalkStatesMemory(menu_state_to_copy, copy_states);
			void* allocation = drawer->GetMainAllocatorBuffer(total_memory);
			uintptr_t buffer = (uintptr_t)allocation;

			menu_to_fill_in->state = *menu_state_to_copy;
			menu_to_fill_in->state.submenu_index = 0;
			drawer->MenuWalkSetStateBuffers(&menu_to_fill_in->state, buffer, &menu_to_fill_in->windows, 0, copy_states);
		}

		// The name is not deallocated - the state only gets deallocated
		void DeallocateMenuState(UIDrawer* drawer, UIDrawerMenu* menu) {
			// The allocation is coalesced - only left_characters needs to be deallocated
			drawer->RemoveAllocation(menu->state.left_characters.buffer);
		}

		// For the non-cached drawer, should call this
		void ReinitializeMenuState(UIDrawer* drawer, Stream<char> name, UIDrawerMenu* menu_to_fill_in, const UIDrawerMenuState* menu_state_to_copy) {
			const void* previous_buffer = menu_to_fill_in->state.left_characters.buffer;
			DeallocateMenuState(drawer, menu_to_fill_in);
			InitializeMenuState(drawer, menu_to_fill_in, menu_state_to_copy, true);

			// Need to update the dynamic allocation
			unsigned int dynamic_index = drawer->system->GetWindowDynamicElement(drawer->window_index, drawer->HandleResourceIdentifier(name));
			drawer->system->ReplaceWindowDynamicResourceAllocation(drawer->window_index, dynamic_index, previous_buffer, menu_to_fill_in->state.left_characters.buffer);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerMenu* UIDrawer::MenuInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, float2 scale, UIDrawerMenuState* menu_state) {
			UIDrawerMenu* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {
				data = GetMainAllocatorBuffer<UIDrawerMenu>();

				if (~configuration & UI_CONFIG_MENU_SPRITE) {
					ECS_STACK_CAPACITY_STREAM(char, temp_characters, 512);
					temp_characters.CopyOther(identifier);
					temp_characters.AddStream("##Separate");

					// separate the identifier for the text label
					data->name = TextLabelInitializer(configuration | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN, config, temp_characters, { 0.0f, 0.0f }, scale);
				}
				data->is_opened = false;

				// The windows buffer needs to be allocated here
				size_t window_allocation_size = data->windows.MemoryOf(ECS_TOOLS_UI_MENU_SUBMENUES_MAX_COUNT);
				void* window_allocation = GetMainAllocatorBuffer(window_allocation_size);
				data->windows = CapacityStream<UIDrawerMenuWindow>(window_allocation, 0, ECS_TOOLS_UI_MENU_SUBMENUES_MAX_COUNT);

				InitializeMenuState(this, data, menu_state, HasFlag(configuration, UI_CONFIG_MENU_COPY_STATES));

				return data;
			});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::MultiGraphDrawer(
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
		) {
			constexpr size_t label_configuration = UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;

			if (IsElementNameFirst(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
				if (name.size > 0) {
					ElementName(configuration, config, name, position, scale);
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
							min_y = min_y > sample_value ? sample_value : min_y;
							max_y = max_y < sample_value ? sample_value : max_y;
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
					ConvertFloatToChars(temp_stream, get_sample(0, 0), x_axis_precision);

					float2 first_sample_span = TextSpan(temp_stream, font_size, character_spacing);
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
				starting_index = index <= 0 ? (int64_t)0 : index - 1;
				while ((get_sample(index, 0) - min_x) * x_space_factor + graph_position.x < region_limit.x && index < samples.size) {
					index++;
				}
				end_index = index >= samples.size - 2 ? (int64_t)samples.size : index + 3;

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

			bool is_name_after = IsElementNameAfter(configuration, UI_CONFIG_GRAPH_NO_NAME);
			size_t finalize_configuration = is_name_after ? configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW : configuration;
			FinalizeRectangle(finalize_configuration, position, scale);

			if (is_name_after) {
				if (name.size > 0) {
					ElementName(configuration, config, name, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		char SentenceToken(size_t configuration, const UIDrawConfig& config) {
			if (configuration & UI_CONFIG_SENTENCE_FIT_SPACE_TOKEN) {
				const UIDrawerSentenceFitSpaceToken* data = (const UIDrawerSentenceFitSpaceToken*)config.GetParameter(UI_CONFIG_SENTENCE_FIT_SPACE_TOKEN);
				return data->token;
			}
			return ' ';
		}

		bool SentenceKeepToken(size_t configuration, const UIDrawConfig& config) {
			if (configuration & UI_CONFIG_SENTENCE_FIT_SPACE_TOKEN) {
				const UIDrawerSentenceFitSpaceToken* data = (const UIDrawerSentenceFitSpaceToken*)config.GetParameter(UI_CONFIG_SENTENCE_FIT_SPACE_TOKEN);
				return data->keep_token;
			}
			return false;
		}

		void* UIDrawer::SentenceInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> text, char separator_token) {
			UIDrawerSentenceCached* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(text, [&](Stream<char> text) {
				data = GetMainAllocatorBuffer<UIDrawerSentenceCached>();

				// getting the whitespace characters count to preallocate the buffers accordingly
				size_t sentence_length = ParseStringIdentifier(text);
					
				char parse_token = SentenceToken(configuration, config);
				size_t whitespace_characters = ParseWordsFromSentence(text, parse_token);

				data->base.whitespace_characters.buffer = (UIDrawerWhitespaceCharacter*)GetMainAllocatorBuffer(sizeof(UISpriteVertex) * sentence_length * 6
					+ sizeof(UIDrawerWhitespaceCharacter) * (whitespace_characters + 1));
				data->base.whitespace_characters.size = 0;

				// setting up the buffers
				uintptr_t buffer = (uintptr_t)data->base.whitespace_characters.buffer;

				buffer += sizeof(UIDrawerWhitespaceCharacter) * (whitespace_characters + 1);

				data->base.vertices.buffer = (UISpriteVertex*)buffer;
				data->base.vertices.size = 0;

				buffer += sizeof(UISpriteVertex) * sentence_length * 6;

				data->base.SetWhitespaceCharacters({ text.buffer, sentence_length }, parse_token);

				Color text_color;
				float character_spacing;
				float2 font_size;
				HandleText(configuration, config, text_color, font_size, character_spacing);

				// converting the characters in a continouos fashion
				system->ConvertCharactersToTextSprites(
					{ text.buffer, sentence_length },
					{ 0.0f, 0.0f },
					data->base.vertices.buffer,
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

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t UIDrawer::SentenceWhitespaceCharactersCount(Stream<char> identifier, CapacityStream<unsigned int> stack_buffer, char separator_token) {
			return FindWhitespaceCharactersCount(identifier, separator_token, &stack_buffer);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SentenceNonCachedInitializerKernel(Stream<char> identifier, UIDrawerSentenceNotCached* data, char separator_token) {
			size_t space_count = ParseWordsFromSentence(identifier, separator_token);

			FindWhitespaceCharacters(data->whitespace_characters, identifier, separator_token);
			data->whitespace_characters.Add(identifier.size);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SentenceDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> text, void* resource, float2 position) {
			char separator_token = SentenceToken(configuration, config);

			// the token might be needed to be kept always
			bool keep_token = true; //SentenceKeepToken(configuration, config);

			// If keeping the token, make the next character after the end of string with '\0' such that it will be rendered as a space and 
			// will not disturb the final string that much

			if (configuration & UI_CONFIG_DO_CACHE) {
#pragma region Cached

				ECS_UI_DRAWER_MODE previous_draw_mode = draw_mode;
				unsigned int previous_draw_count = draw_mode_count;
				unsigned int previous_draw_target = draw_mode_target;
				if (configuration & UI_CONFIG_SENTENCE_FIT_SPACE) {
					SetDrawMode(ECS_UI_DRAWER_FIT_SPACE);
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
							temp_stream.size = (word_end_index - word_start_index + 1 + keep_token) * 6;

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
							if (data->base.whitespace_characters[index].type != separator_token || keep_token) {
								NextRow();
							}
							index++;
						}
						if (data->base.whitespace_characters[index].type == '\n') {
							NextRow();
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

#pragma endregion

			}
			else {
#pragma region Non cached

				constexpr size_t WHITESPACE_ALLOCATION_SIZE = ECS_KB * 50;
				unsigned int _whitespace_characters[4096];
				CapacityStream<unsigned int> whitespace_characters(_whitespace_characters, 0, 4096);
				whitespace_characters.size = SentenceWhitespaceCharactersCount(text, whitespace_characters, separator_token);

				if (whitespace_characters.size > whitespace_characters.capacity) {
					unsigned int* new_whitespace_characters = (unsigned int*)GetTempBuffer(sizeof(unsigned int) * whitespace_characters.size);
					UIDrawerSentenceNotCached data;
					data.whitespace_characters = Stream<unsigned int>(new_whitespace_characters, 0);
					SentenceNonCachedInitializerKernel(text, &data, separator_token);
					whitespace_characters.buffer = data.whitespace_characters.buffer;
					whitespace_characters.capacity = whitespace_characters.size;
				}
				else {
					whitespace_characters.Add(ParseStringIdentifier(text));
				}

				float y_row_scale = current_row_y_scale;

				ECS_UI_DRAWER_MODE previous_draw_mode = draw_mode;
				unsigned int previous_draw_count = draw_mode_count;
				unsigned int previous_draw_target = draw_mode_target;
				if (configuration & UI_CONFIG_SENTENCE_FIT_SPACE) {
					SetDrawMode(ECS_UI_DRAWER_FIT_SPACE);
				}

				size_t text_length = ParseStringIdentifier(text);
				size_t word_start_index = 0;
				size_t word_end_index = 0;

				Color font_color;
				float2 font_size;
				float character_spacing;

				HandleText(configuration, config, font_color, font_size, character_spacing);

				// Check to see if the sentence contains \n. If it doesn't, can take the text span of the entire sentence and precull it 
				// if is out of bounds
				bool single_sentence = true;
				for (size_t index = 0; index < whitespace_characters.size && single_sentence; index++) {
					single_sentence = text[whitespace_characters[index]] != '\n';
				}

				auto draw_word_by_word = [&]() {
					float starting_row_position = position.x;
					float token_x_scale = TextSpan({ &separator_token, 1 }, font_size, character_spacing).x * !keep_token;

					size_t text_configuration = configuration;
					text_configuration |= HasFlag(configuration, UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) ? UI_CONFIG_ALIGN_TO_ROW_Y : 0;
					for (size_t index = 0; index < whitespace_characters.size; index++) {
						if (whitespace_characters[index] != 0) {
							word_end_index = whitespace_characters[index] - 1;
						}
						if (word_end_index >= word_start_index) {
							unsigned int last_character_index = word_end_index + 1 + keep_token;
							last_character_index -= last_character_index >= text.size;

							Text(text_configuration, config, { text.buffer + word_start_index, last_character_index - word_start_index });
							Indent(-1.0f);
						}

						while (whitespace_characters[index] == whitespace_characters[index + 1] - 1 && index < whitespace_characters.size - 1) {
							if (text[whitespace_characters[index]] != separator_token) {
								NextRow();
							}
							index++;
						}
						if (text[whitespace_characters[index]] == '\n') {
							NextRow();
						}
						word_start_index = whitespace_characters[index] + 1;
					}
				};

				if (single_sentence) {
					float2 text_span = TextSpan(Stream<char>(text.buffer, text_length), font_size, character_spacing);
					if (ValidatePosition(0, position, text_span)) {
						if (draw_mode == ECS_UI_DRAWER_FIT_SPACE) {
							if (position.x + text_span.x < region_limit.x) {
								if (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
									position.y = AlignMiddle(position.y, current_row_y_scale, text_span.y);
								}
								Text(configuration, config, text, position);
							}
							else {
								draw_word_by_word();
							}
						}
						else {
							if (configuration & UI_CONFIG_SENTENCE_ALIGN_TO_ROW_Y_SCALE) {
								position.y = AlignMiddle(position.y, current_row_y_scale, text_span.y);
							}
							Text(configuration, config, text, position);
						}
					}
					else {
						FinalizeRectangle(0, position, text_span);
					}
				}
				else {
					draw_word_by_word();
				}

				if (configuration & UI_CONFIG_SENTENCE_FIT_SPACE) {
					draw_mode = previous_draw_mode;
					draw_mode_count = previous_draw_count;
					draw_mode_target = previous_draw_target;
				}
#pragma endregion

			}

		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextTable* UIDrawer::TextTableInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, unsigned int rows, unsigned int columns, Stream<char>* labels) {
			UIDrawerTextTable* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {
				data = GetMainAllocatorBuffer<UIDrawerTextTable>();

				data->zoom = *zoom_ptr;
				data->inverse_zoom = zoom_inverse;

				unsigned int cell_count = rows * columns;
				size_t total_memory_size = 0;
				for (size_t index = 0; index < cell_count; index++) {
					total_memory_size += labels[index].size;
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
						font_color,
						0,
						font_size,
						character_spacing
					);

					data->labels[index].size = labels[index].size * 6;
					buffer += sizeof(UISpriteVertex) * data->labels[index].size;
					if (configuration & UI_CONFIG_TEXT_TABLE_DYNAMIC_SCALE) {
						float2 text_span = GetTextSpan(data->labels[index]);
						unsigned int column_index = index % columns;
						data->column_scales[column_index] = ClampMin(data->column_scales[column_index], text_span.x + ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR * element_descriptor.label_padd.x);
						data->row_scale = text_span.y + element_descriptor.label_padd.y * ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR;
					}
				}

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
						HandleTextLabelAlignment(text_span, scale, position, x_position, y_position, ECS_UI_ALIGN::ECS_UI_ALIGN_MIDDLE, ECS_UI_ALIGN::ECS_UI_ALIGN_MIDDLE);

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
			Stream<char>* labels
		) {
			if (~configuration & UI_CONFIG_TEXT_TABLE_DO_NOT_INFER) {
				scale = GetSquareScale(scale.y * ECS_TOOLS_UI_TEXT_TABLE_INFER_FACTOR);
			}

			unsigned int cell_count = rows * columns;

			size_t temp_marker = GetTempAllocatorMarker();
			Stream<UISpriteVertex>* cells = (Stream<UISpriteVertex>*)GetTempBuffer(sizeof(Stream<UISpriteVertex>) * cell_count);

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
						cells[index] = GetTextStream(configuration, labels[index].size);
						system->ConvertCharactersToTextSprites(
							labels[index],
							{ 0.0f, 0.0f },
							cells[index].buffer,
							font_color,
							0,
							font_size,
							character_spacing
						);
						float2 text_span = GetTextSpan(cells[index]);
						float current_x_scale = text_span.x + ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR * element_descriptor.label_padd.x;
						column_biggest_scale[column] = ClampMin(column_biggest_scale[column], current_x_scale);
						cell_scales[index] = text_span.x;
						y_text_span = text_span.y;
						scale.y = text_span.y + ECS_TOOLS_UI_TEXT_TABLE_ENLARGE_CELL_FACTOR * element_descriptor.label_padd.y;
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
							HandleTextLabelAlignment({ cell_scales[cell_index], y_text_span }, scale, position, x_position, y_position, ECS_UI_ALIGN::ECS_UI_ALIGN_MIDDLE, ECS_UI_ALIGN::ECS_UI_ALIGN_MIDDLE);

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
			ConvertFloatToChars(temp_float_stream, max_y, y_axis_precision);

			size_t* text_count = HandleTextSpriteCount(configuration);
			auto text_buffer = HandleTextSpriteBuffer(configuration);

			float2 top_text_position = { position.x + element_descriptor.graph_padding.x, position.y + element_descriptor.graph_padding.y };
			float max_x_scale = 0.0f;
			system->ConvertCharactersToTextSprites(
				temp_float_stream,
				top_text_position,
				text_buffer,
				font_color,
				*text_count,
				font_size,
				character_spacing
			);

			auto top_vertices_stream = GetTextStream(configuration, temp_float_stream.size * 6);
			float2 text_span = GetTextSpan(top_vertices_stream);
			max_x_scale = ClampMin(max_x_scale, text_span.x);
			*text_count += 6 * temp_float_stream.size;

			float2 bottom_text_position = { top_text_position.x, position.y + scale.y - y_sprite_scale - element_descriptor.graph_padding.y };
			temp_float_stream.size = 0;
			ConvertFloatToChars(temp_float_stream, min_y, y_axis_precision);
			system->ConvertCharactersToTextSprites(
				temp_float_stream,
				bottom_text_position,
				text_buffer,
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
				float value = Lerp(min_y, max_y, 1.0f - (index + 1) * step);
				temp_float_stream.size = 0;
				ConvertFloatToChars(temp_float_stream, value, y_axis_precision);

				auto temp_vertices = GetTextStream(configuration, temp_float_stream.size * 6);
				system->ConvertCharactersToTextSprites(
					temp_float_stream,
					sprite_position,
					text_buffer,
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
			ConvertFloatToChars(temp_float_stream, min_x, x_axis_precision);

			size_t* text_count = HandleTextSpriteCount(configuration);
			auto text_buffer = HandleTextSpriteBuffer(configuration);

			float text_y = position.y + scale.y - y_sprite_scale - element_descriptor.graph_padding.y;

			system->ConvertCharactersToTextSprites(
				temp_float_stream,
				{ position.x, text_y },
				text_buffer,
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
			ConvertFloatToChars(temp_float_stream, max_x, x_axis_precision);

			float2 right_span = TextSpan(temp_float_stream, font_size, character_spacing);
			float2 right_text_position = { position.x + scale.x - element_descriptor.graph_padding.x - right_span.x, text_y };
			system->ConvertCharactersToTextSprites(
				temp_float_stream,
				right_text_position,
				text_buffer,
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
			min_sprite_count = std::min(min_sprite_count, max_sprite_count);
			max_sprite_count = std::max(max_sprite_count, min_copy);
			int64_t index = min_sprite_count;
			float total_sprite_length = 0.0f;

			for (; index <= max_sprite_count && index > 0; index++) {
				total_sprite_length = 0.0f;

				float step = 1.0f / (index + 1);
				for (size_t subindex = 0; subindex < index; subindex++) {
					float value = Lerp(min_x, max_x, 1.0f - (subindex + 1) * step);
					temp_float_stream.size = 0;
					ConvertFloatToChars(temp_float_stream, value, x_axis_precision);
					float2 current_span = TextSpan(temp_float_stream, font_size, character_spacing);
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
					float value = Lerp(min_x, max_x, (subindex + 1) * step);
					temp_float_stream.size = 0;
					ConvertFloatToChars(temp_float_stream, value, x_axis_precision);
					system->ConvertCharactersToTextSprites(
						temp_float_stream,
						sprite_position,
						text_buffer,
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
			Stream<char> name,
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
						min_y = std::min(min_y, samples[index].y);
					}
					if (~configuration & UI_CONFIG_GRAPH_MAX_Y) {
						max_y = std::max(max_y, samples[index].y);
					}
				}

				difference.y = max_y - min_y;
				float ratio = difference.y / difference.x;
				scale.y = scale.x * ratio;
				scale.y = system->NormalizeVerticalToWindowDimensions(scale.y);
				scale.y = Clamp(scale.y, data->min_y_scale, data->max_y_scale);
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
							min_y = std::min(min_y, samples[index].y);
						}
						if (~configuration & UI_CONFIG_GRAPH_MAX_Y) {
							max_y = std::max(max_y, samples[index].y);
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
				scale.x = Clamp(scale.x, data->min_x_scale, data->max_x_scale);
			}

			size_t label_configuration = UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;

			if (IsElementNameFirst(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
				if (name.size > 0) {
					ElementName(configuration, config, name, position, scale);
				}
			}

			float2 graph_no_info_position = position;
			float2 graph_no_info_scale = scale;
			UIConfigGraphInfoLabels info_labels;
			bool is_info_labels = false;
			if (configuration & UI_CONFIG_GRAPH_INFO_LABELS) {
				info_labels = *(const UIConfigGraphInfoLabels*)config.GetParameter(UI_CONFIG_GRAPH_INFO_LABELS);
				// Determine by how much to increase the scale
				if (info_labels.GetTopLeft().size > 0 || info_labels.GetTopRight().size > 0) {
					graph_no_info_position.y += layout.default_element_y;
					scale.y += layout.default_element_y;
				}
				if (info_labels.GetBottomLeft().size > 0 || info_labels.GetBottomRight().size > 0) {
					scale.y += layout.default_element_y;
				}
				is_info_labels = true;
			}

			HandleFitSpaceRectangle(configuration, position, scale);

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			if (ValidatePosition(configuration, position, scale)) {
				// We'll need these accross the info labels
				Color font_color;
				float character_spacing;
				float2 font_size;

				HandleText(configuration, config, font_color, font_size, character_spacing);
				font_color.alpha = ECS_TOOLS_UI_GRAPH_TEXT_ALPHA;

				if (configuration & UI_CONFIG_GRAPH_REDUCE_FONT) {
					font_size.x *= element_descriptor.graph_reduce_font;
					font_size.y *= element_descriptor.graph_reduce_font;
				}

				Color line_color;
				if (configuration & UI_CONFIG_GRAPH_LINE_COLOR) {
					const UIConfigGraphColor* color_desc = (const UIConfigGraphColor*)config.GetParameter(UI_CONFIG_GRAPH_LINE_COLOR);
					line_color = color_desc->color;
				}
				else {
					line_color = color_theme.graph_line;
				}

				float2 initial_scale = graph_no_info_scale;
				float2 graph_scale = { graph_no_info_scale.x - 2.0f * element_descriptor.graph_padding.x, graph_no_info_scale.y - 2.0f * element_descriptor.graph_padding.y };
				float2 graph_position = { graph_no_info_position.x + element_descriptor.graph_padding.x, graph_no_info_position.y + element_descriptor.graph_padding.y };

				if (IsElementNameFirst(configuration, UI_CONFIG_GRAPH_NO_NAME)) {
					if (name.size > 0) {
						TextLabel(label_configuration, config, name, position, scale);
						scale = initial_scale;
					}
				}

				UIDrawConfig info_labels_config;
				size_t INFO_LABEL_CONFIGURATION = ClearFlag(configuration, UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_WINDOW_DEPENDENT_SIZE)
					| UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_VALIDATE_POSITION;
				memcpy(&info_labels_config, &config, sizeof(config));
				if (HasFlag(configuration, UI_CONFIG_ABSOLUTE_TRANSFORM)) {
					info_labels_config.RemoveFlag(UI_CONFIG_ABSOLUTE_TRANSFORM);
				}
				if (info_labels.inherit_line_color) {
					UIConfigTextParameters text_parameters;
					if (HasFlag(configuration, UI_CONFIG_TEXT_PARAMETERS)) {
						text_parameters = *(const UIConfigTextParameters*)config.GetParameter(UI_CONFIG_TEXT_PARAMETERS);
					}
					text_parameters.color = line_color;
					UIConfigTextParameters previous_text_parameters;
					SetConfigParameter(configuration, info_labels_config, text_parameters, previous_text_parameters);
					INFO_LABEL_CONFIGURATION = SetFlag(INFO_LABEL_CONFIGURATION, UI_CONFIG_TEXT_PARAMETERS);
				}

				// Draw the top info labels, if any
				if (is_info_labels) {
					float2 row_position = position + element_descriptor.graph_padding;
					Stream<char> top_left_info = info_labels.GetTopLeft();
					UIConfigAbsoluteTransform absolute_transform;
					// We don't need the scale if we are using Text
					absolute_transform.scale = { 0.0f, 0.0f };
					if (top_left_info.size > 0) {
						absolute_transform.position = row_position + region_render_offset;
						info_labels_config.AddFlag(absolute_transform);
						Text(INFO_LABEL_CONFIGURATION, info_labels_config, top_left_info);
						info_labels_config.flag_count--;
					}

					Stream<char> top_right_info = info_labels.GetTopRight();
					if (top_right_info.size > 0) {
						float2 text_span = system->GetTextSpan(top_right_info, font_size.x, font_size.y, character_spacing);
						absolute_transform.position = { row_position.x + graph_scale.x - text_span.x, row_position.y };
						absolute_transform.position += region_render_offset;
						info_labels_config.AddFlag(absolute_transform);
						Text(INFO_LABEL_CONFIGURATION, info_labels_config, top_right_info);
						info_labels_config.flag_count--;
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
				if ((~configuration & UI_CONFIG_GRAPH_MIN_Y) != 0 || (~configuration & UI_CONFIG_GRAPH_MAX_Y) != 0) {
					for (size_t index = 0; index < samples.size; index++) {
						if (~configuration & UI_CONFIG_GRAPH_MIN_Y) {
							min_y = std::min(min_y, samples[index].y);
						}
						if (~configuration & UI_CONFIG_GRAPH_MAX_Y) {
							max_y = std::max(max_y, samples[index].y);
						}
					}
				}


				difference.y = max_y - min_y;
				float epsilon = 0.001f;
				if (FloatCompare(difference.y, 0.0f, epsilon)) {
					// If the min_y and max_y are really large, we will need to increase these by a large factor
					// In order to cross the floating point error margin
					// Bump these such that they line will be smack in the middle
					if (min_y > 10000000.0f) {
						epsilon = 10.0f;
					}
					else if (min_y > 1000000.0f) {
						epsilon = 1.0f;
					}
					else if (min_y > 1000000.0f) {
						epsilon = 0.1f;
					}
					else if (min_y > 100000.0f) {
						epsilon = 0.01f;
					}
					max_y += epsilon * 0.5f;
					min_y -= epsilon * 0.5f;
					difference.y = epsilon;
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
					ConvertFloatToChars(temp_stream, samples[0].x, x_axis_precision);

					float2 first_sample_span = TextSpan(temp_stream, font_size, character_spacing);
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
				starting_index = index <= 2 ? (int64_t)0 : index - 2;
				while ((samples[index].x - min_x) * x_space_factor + graph_position.x < region_limit.x && index < samples.size) {
					index++;
				}
				end_index = index >= samples.size - 2 ? (int64_t)samples.size : index + 3;

				if (~configuration & UI_CONFIG_GRAPH_NO_BACKGROUND) {
					SolidColorRectangle(configuration, config, position, scale);
				}
				if (configuration & UI_CONFIG_GRAPH_CLICKABLE) {
					const UIConfigGraphClickable* clickable = (const UIConfigGraphClickable*)config.GetParameter(UI_CONFIG_GRAPH_CLICKABLE);
					AddClickable(
						configuration,
						position,
						scale,
						clickable->handler,
						clickable->button_type,
						clickable->copy_function
					);
				}

				auto convert_absolute_position_to_graph_space = [](float2 position, float2 graph_position, float2 graph_scale,
					float2 min, float2 inverse_sample_span) {
						return float2(graph_position.x + graph_scale.x * (position.x - min.x) * inverse_sample_span.x, graph_position.y + graph_scale.y * (1.0f - (position.y - min.y) * inverse_sample_span.y));
				};

				float2 inverse_sample_span = { 1.0f / (samples[samples.size - 1].x - samples[0].x), 1.0f / difference.y };
				float2 min_values = { samples[0].x, min_y };
				float2 previous_point_position = convert_absolute_position_to_graph_space(samples[starting_index], graph_position, graph_scale, min_values, inverse_sample_span);

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
					Line(configuration, previous_point_position, next_point, line_color);

					UIDrawerGraphHoverableData hoverable_data;
					hoverable_data.first_sample_values = samples[index];
					hoverable_data.second_sample_values = samples[index + 1];
					hoverable_data.sample_index = index;
					hoverable_data.line_color = color_theme.graph_hover_line;
					hoverable_data.line_start = previous_point_position;
					hoverable_data.line_end = next_point;

					AddHoverable(configuration, hoverable_position, hoverable_scale, { GraphHoverable, &hoverable_data, sizeof(hoverable_data), ECS_UI_DRAW_SYSTEM });
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
						SetSpriteRectangle(
							{ previous_point_position.x - sprite_scale.x * 0.5f, previous_point_position.y - sprite_scale.y * 0.5f }, 
							sprite_scale, 
							color_theme.graph_sample_circle, 
							{ 0.0f, 0.0f }, 
							{ 1.0f, 1.0f }, 
							sample_circle_stream.buffer, 
							temp_index
						);
					}

					previous_point_position = next_point;
				}

				if (configuration & UI_CONFIG_GRAPH_SAMPLE_CIRCLES) {
					if (end_index > starting_index + 1) {
						float2 sprite_scale = GetSquareScale(element_descriptor.graph_sample_circle_size);
						size_t temp_index = (end_index - 1 - starting_index) * 6;
						SetSpriteRectangle(
							{ previous_point_position.x - sprite_scale.x * 0.5f, previous_point_position.y - sprite_scale.y * 0.5f }, 
							sprite_scale, 
							color_theme.graph_sample_circle, 
							{ 0.0f, 0.0f }, 
							{ 1.0f, 1.0f }, 
							sample_circle_stream.buffer, 
							temp_index
						);
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

				// Draw the bottom info labels, if any
				if (is_info_labels) {
					float2 row_position = position + element_descriptor.graph_padding;
					row_position.y += position.y + scale.y - layout.default_element_y - element_descriptor.graph_padding.y;

					Stream<char> bottom_left_info = info_labels.GetBottomLeft();
					UIConfigAbsoluteTransform absolute_transform;
					// We don't need the scale if we are using Text
					absolute_transform.scale = { 0.0f, 0.0f };
					if (bottom_left_info.size > 0) {
						absolute_transform.position = row_position + region_render_offset;
						info_labels_config.AddFlag(absolute_transform);
						Text(INFO_LABEL_CONFIGURATION, info_labels_config, bottom_left_info);
						info_labels_config.flag_count--;
					}

					Stream<char> bottom_right_info = info_labels.GetBottomRight();
					if (bottom_right_info.size > 0) {
						float2 text_span = system->GetTextSpan(bottom_right_info, font_size.x, font_size.y, character_spacing);
						absolute_transform.position = { row_position.x + graph_scale.x - text_span.x, row_position.y };
						absolute_transform.position += region_render_offset;
						info_labels_config.AddFlag(absolute_transform);
						Text(INFO_LABEL_CONFIGURATION, info_labels_config, bottom_right_info);
						info_labels_config.flag_count--;
					}
				}
			}

			bool is_name_after = IsElementNameAfter(configuration, UI_CONFIG_GRAPH_NO_NAME);
			size_t finalize_configuration = is_name_after ? configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW : configuration;
			FinalizeRectangle(finalize_configuration, position, scale);

			if (is_name_after) {
				if (name.size > 0) {
					ElementName(configuration, config, name, position, scale);
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

		void UIDrawer::FixedScaleTextLabel(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2 position, float2 scale) {
			AlignToRowY(this, configuration, position, scale);

			Stream<UISpriteVertex> current_text = GetTextStream(configuration, ParseStringIdentifier(text) * 6);
			float2 text_span;

			float2 temp_position = position + element_descriptor.label_padd;
			Text(configuration | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_VALIDATE_POSITION, config, text, temp_position, text_span);

			ECS_UI_ALIGN horizontal_alignment = ECS_UI_ALIGN_MIDDLE, vertical_alignment = ECS_UI_ALIGN_TOP;
			GetTextLabelAlignment(configuration, config, horizontal_alignment, vertical_alignment);
			bool translate = true;

			auto text_sprite_count = HandleTextSpriteCount(configuration);
			if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				const ECS_UI_WINDOW_DEPENDENT_SIZE* type = (const ECS_UI_WINDOW_DEPENDENT_SIZE*)config.GetParameter(UI_CONFIG_WINDOW_DEPENDENT_SIZE);
				if (*type == ECS_UI_WINDOW_DEPENDENT_BOTH) {
					if (configuration & UI_CONFIG_VERTICAL) {
						if (text_span.x > scale.x - 2 * element_descriptor.label_padd.x) {
							translate = false;
							*text_sprite_count -= current_text.size;
						}
						else {
							if (text_span.y > scale.y - 2 * element_descriptor.label_padd.y) {
								size_t valid_sprites;
								if (vertical_alignment == ECS_UI_ALIGN_BOTTOM) {
									valid_sprites = CullTextSprites<3>(current_text, -position.y - scale.y + element_descriptor.label_padd.x);
									*text_sprite_count -= current_text.size - valid_sprites;
									current_text.size = valid_sprites;
									text_span = GetTextSpan(current_text, false, true);
								}
								else {
									valid_sprites = CullTextSprites<2>(current_text, -position.y - scale.y + element_descriptor.label_padd.y);
									*text_sprite_count -= current_text.size - valid_sprites;
									current_text.size = valid_sprites;
									text_span = GetTextSpan(current_text, false);
								}
							}
						}
					}
					else {
						if (text_span.y > scale.y - 2 * element_descriptor.label_padd.y) {
							translate = false;
							*text_sprite_count -= current_text.size;
						}
						else {
							if (text_span.x > scale.x - 2 * element_descriptor.label_padd.x) {
								size_t valid_sprites;
								if (horizontal_alignment == ECS_UI_ALIGN_RIGHT) {
									valid_sprites = CullTextSprites<1>(current_text, position.x + element_descriptor.label_padd.x);
									*text_sprite_count -= current_text.size - valid_sprites;
									current_text.size = valid_sprites;
									text_span = GetTextSpan(current_text, true, true);
								}
								else {
									valid_sprites = CullTextSprites<0>(current_text, position.x + scale.x - element_descriptor.label_padd.x);
									*text_sprite_count -= current_text.size - valid_sprites;
									current_text.size = valid_sprites;
									text_span = GetTextSpan(current_text, true, false);
								}

							}
						}
					}
				}
				else {
					if (configuration & UI_CONFIG_VERTICAL) {
						if (text_span.y > scale.y - 2 * element_descriptor.label_padd.y) {
							size_t valid_sprites;
							if (vertical_alignment == ECS_UI_ALIGN_BOTTOM) {
								valid_sprites = CullTextSprites<3>(current_text, -position.y - scale.y + element_descriptor.label_padd.y);
								*text_sprite_count -= current_text.size - valid_sprites;
								current_text.size = valid_sprites;
								text_span = GetTextSpan(current_text, false, true);
							}
							else {
								valid_sprites = CullTextSprites<2>(current_text, -position.y - scale.y + element_descriptor.label_padd.y);
								*text_sprite_count -= current_text.size - valid_sprites;
								current_text.size = valid_sprites;
								text_span = GetTextSpan(current_text, false, false);
							}
						}
					}
					else {
						if (text_span.x > scale.x - 2 * element_descriptor.label_padd.x) {
							size_t valid_sprites;
							if (horizontal_alignment == ECS_UI_ALIGN_RIGHT) {
								valid_sprites = CullTextSprites<1>(current_text, position.x + element_descriptor.label_padd.x);
								*text_sprite_count -= current_text.size - valid_sprites;
								current_text.size = valid_sprites;
								text_span = GetTextSpan(current_text, true, true);
							}
							else {
								valid_sprites = CullTextSprites<0>(current_text, position.x + scale.x - element_descriptor.label_padd.x);
								*text_sprite_count -= current_text.size - valid_sprites;
								current_text.size = valid_sprites;
								text_span = GetTextSpan(current_text, true, false);
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

				if (horizontal_alignment != ECS_UI_ALIGN_LEFT || vertical_alignment != ECS_UI_ALIGN_TOP) {
					float x_translation = x_text_position - position.x - element_descriptor.label_padd.x;
					float y_translation = y_text_position - position.y - element_descriptor.label_padd.y;
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
			Stream<char> name,
			void** horizontal_slider,
			void** vertical_slider
		) {
			UIDrawConfig null_config;
			ECS_STACK_CAPACITY_STREAM(char, stack_name, 128);
			stack_name.CopyOther(name);
			stack_name.AddStream("VerticalSlider");

			// bounds, position and scale here don't matter
			*vertical_slider = FloatSliderInitializer(
				UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_SLIDER_NO_TEXT | UI_CONFIG_VERTICAL | UI_CONFIG_DO_NOT_ADVANCE,
				null_config,
				stack_name,
				{ 0.0f, 0.0f },
				{ 0.0f, 0.0f },
				&region_offset->y,
				0.0f,
				10.0f
			);

			stack_name.size = name.size;
			stack_name.AddStream("HorizontalSlider");
			// bounds, position and scale here don't matter
			*horizontal_slider = FloatSliderInitializer(
				UI_CONFIG_SLIDER_NO_NAME | UI_CONFIG_SLIDER_NO_TEXT | UI_CONFIG_DO_NOT_ADVANCE,
				null_config,
				stack_name,
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
			void* horizontal_slider,
			void* vertical_slider
		) {
			float2 horizontal_slider_position = system->GetDockspaceRegionHorizontalRenderSliderPosition(region_position, region_scale);
			float2 horizontal_slider_scale = system->GetDockspaceRegionHorizontalRenderSliderScale(region_scale);
			float2 vertical_slider_position = system->GetDockspaceRegionVerticalRenderSliderPosition(region_position, region_scale, dockspace, border_index);
			float2 vertical_slider_scale = system->GetDockspaceRegionVerticalRenderSliderScale(region_scale, dockspace, border_index, window_index);

			bool is_horizontal_extended = render_span.x - system->m_descriptors.misc.render_slider_vertical_size > render_zone.x;
			bool is_vertical_extended = render_span.y - system->m_descriptors.misc.render_slider_horizontal_size > render_zone.y;
			bool is_horizontal = render_span.x > render_zone.x;
			bool is_vertical = render_span.y > render_zone.y;

			UIConfigAbsoluteTransform horizontal_transform;
			if ((!is_vertical_extended && is_horizontal_extended && !is_vertical)|| (is_horizontal && is_vertical)) {
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
				slider_padding.value = element_descriptor.label_padd.x * zoom_inverse.x;

				horizontal_transform.position = horizontal_slider_position;

				horizontal_transform.scale = horizontal_slider_scale;

				config.AddFlags(horizontal_transform, color, slider_color, slider_shrink, slider_length);
				config.AddFlag(slider_padding);

				if (system->m_windows[window_index].pin_horizontal_slider_count > 0) {
					UIDrawerSlider* slider = (UIDrawerSlider*)horizontal_slider;
					slider->slider_position = region_offset->x / difference;
					system->m_windows[window_index].pin_horizontal_slider_count--;
				}

				float previous_value = region_offset->x;
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
				if (FloatCompare(previous_value, region_offset->x, 0.0001f)) {
					// Avoid micro bounces
					region_offset->x = previous_value;
				}
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
				slider_padding.value = element_descriptor.label_padd.y * zoom_inverse.y;

				vertical_transform.position = vertical_slider_position;
				vertical_transform.scale = vertical_slider_scale;

				config.AddFlags(vertical_transform, color, slider_color, slider_shrink, slider_length);
				config.AddFlag(slider_padding);

				if (system->m_windows[window_index].pin_vertical_slider_count > 0) {
					UIDrawerSlider* slider = (UIDrawerSlider*)vertical_slider;
					slider->slider_position = region_offset->y / difference;
					system->m_windows[window_index].pin_vertical_slider_count--;
				}

				float previous_value = region_offset->y;
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
				if (FloatCompare(previous_value, region_offset->y, 0.0001f)) {
					// Avoid micro bounces
					region_offset->y = previous_value;
				}
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
			Stream<char> horizontal_slider_name,
			Stream<char> vertical_slider_name,
			float2 render_span,
			float2 render_zone,
			float2* region_offset
		) {
			void* horizontal_slider = GetResource(horizontal_slider_name);
			void* vertical_slider = GetResource(vertical_slider_name);

			ViewportRegionSliders(
				render_span,
				render_zone,
				region_offset,
				horizontal_slider,
				vertical_slider
			);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteRectangle(
			size_t configuration,
			float2 position,
			float2 scale,
			Stream<wchar_t> texture,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			if (!initializer) {
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
				void** current_buffers = buffers;
				size_t* current_counts = counts;
				if (configuration & UI_CONFIG_LATE_DRAW) {
					phase = ECS_UI_DRAW_LATE;
					current_buffers = buffers + ECS_TOOLS_UI_MATERIALS;
					current_counts = counts + ECS_TOOLS_UI_MATERIALS;
				}
				else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
					phase = ECS_UI_DRAW_SYSTEM;
					current_buffers = system_buffers;
					current_counts = system_counts;
				}

				system->SetSprite(
					dockspace,
					border_index,
					texture,
					position,
					scale,
					current_buffers,
					current_counts,
					color,
					top_left_uv,
					bottom_right_uv,
					phase
				);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteRectangle(size_t configuration, const UIDrawConfig& config, ResourceView view, Color color, float2 top_left_uv, float2 bottom_right_uv)
		{
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			if (!initializer) {
				HandleFitSpaceRectangle(configuration, position, scale);
				SpriteRectangle(configuration, position, scale, view, color, top_left_uv, bottom_right_uv);
				FinalizeRectangle(configuration, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteRectangle(size_t configuration, float2 position, float2 scale, ResourceView view, Color color, float2 top_left_uv, float2 bottom_right_uv)
		{
			if (!initializer) {
				ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
				void** current_buffers = buffers;
				size_t* current_counts = counts;

				if (configuration & UI_CONFIG_LATE_DRAW) {
					phase = ECS_UI_DRAW_LATE;
					current_buffers = buffers + ECS_TOOLS_UI_MATERIALS;
					current_counts = counts + ECS_TOOLS_UI_MATERIALS;
				}
				else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
					phase = ECS_UI_DRAW_SYSTEM;
					current_buffers = system_buffers;
					current_counts = system_counts;
				}

				system->SetSprite(
					dockspace,
					border_index,
					view,
					position,
					scale,
					current_buffers,
					current_counts,
					color,
					top_left_uv,
					bottom_right_uv,
					phase
				);
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
			Stream<wchar_t> texture,
			const Color* colors,
			const float2* uvs,
			ECS_UI_DRAW_PHASE phase
		) {
			system->SetVertexColorSprite(dockspace, border_index, texture, position, scale, buffers, counts, colors, uvs[0], uvs[1], phase);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			Stream<wchar_t> texture,
			const Color* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			ECS_UI_DRAW_PHASE phase
		) {
			system->SetVertexColorSprite(dockspace, border_index, texture, position, scale, buffers, counts, colors, top_left_uv, bottom_right_uv, phase);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			Stream<wchar_t> texture,
			const ColorFloat* colors,
			const float2* uvs,
			ECS_UI_DRAW_PHASE phase
		) {
			system->SetVertexColorSprite(dockspace, border_index, texture, position, scale, buffers, counts, colors, uvs[0], uvs[1], phase);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::VertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			Stream<wchar_t> texture,
			const ColorFloat* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			ECS_UI_DRAW_PHASE phase
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

		float2 UIDrawer::TextSpan(Stream<char> characters, float2 font_size, float character_spacing) const {
			return system->GetTextSpan(characters, font_size.x, font_size.y, character_spacing);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::TextSpan(Stream<char> characters) const {
			return TextSpan(characters, GetFontSize(), font.character_spacing);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetLabelScale(Stream<char> characters) const {
			return HandleLabelSize(TextSpan(characters));
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetLabelScale(Stream<char> characters, float2 font_size, float character_spacing) const {
			return HandleLabelSize(TextSpan(characters, font_size, character_spacing));
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetLabelScale(const UIDrawerTextElement* element) const
		{
			return { element->scale.x + element_descriptor.label_padd.x * 2.0f, element->scale.y + element_descriptor.label_padd.y * 2.0f };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::GetTextLabelAlignment(size_t configuration, const UIDrawConfig& config, ECS_UI_ALIGN& horizontal, ECS_UI_ALIGN& vertical) const {
			if (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
				const float* params = (const float*)config.GetParameter(UI_CONFIG_TEXT_ALIGNMENT);
				const ECS_UI_ALIGN* alignments = (ECS_UI_ALIGN*)params;
				horizontal = *alignments;
				vertical = *(alignments + 1);
			}
			else {
				horizontal = ECS_UI_ALIGN_MIDDLE;
				vertical = ECS_UI_ALIGN_MIDDLE;
			}
		}
		
		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::GetElementAlignment(size_t configuration, const UIDrawConfig& config, ECS_UI_ALIGN& horizontal, ECS_UI_ALIGN& vertical) const
		{
			if (configuration & UI_CONFIG_ALIGN_ELEMENT) {
				const UIConfigAlignElement* align = (const UIConfigAlignElement*)config.GetParameter(UI_CONFIG_ALIGN_ELEMENT);
				horizontal = align->horizontal;
				vertical = align->vertical;
			}
			else {
				// The same as don't do anything
				vertical = ECS_UI_ALIGN_TOP;
				horizontal = ECS_UI_ALIGN_LEFT;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::GetElementAlignedPosition(size_t configuration, const UIDrawConfig& config, float2& position, float2 scale) const
		{
			ECS_UI_ALIGN horizontal, vertical;
			GetElementAlignment(configuration, config, horizontal, vertical);

			switch (horizontal) {
			case ECS_UI_ALIGN_LEFT:
				// Same position - don't modify anything
				break;
			case ECS_UI_ALIGN_MIDDLE:
			{
				float x_scale_until_border = GetXScaleUntilBorder(position.x);
				position.x = AlignMiddle(position.x, x_scale_until_border, scale.x);
			}
				break;
			case ECS_UI_ALIGN_RIGHT:
			{
				float x_scale_until_border = GetXScaleUntilBorder(position.x);
				if (x_scale_until_border >= scale.x) {
					position.x += x_scale_until_border - scale.x;
				}
			}
				break;
			}

			switch (vertical) {
			case ECS_UI_ALIGN_TOP:
				// Same position - don't modify anything
				break;
			case ECS_UI_ALIGN_MIDDLE: 
			{
				float y_scale_until_border = GetYScaleUntilBorder(position.y);
				position.y = AlignMiddle(position.y, y_scale_until_border, scale.y);
			}
				break;
			case ECS_UI_ALIGN_BOTTOM:
			{
				float y_scale_until_border = GetYScaleUntilBorder(position.y);
				if (y_scale_until_border >= scale.y) {
					position.y += y_scale_until_border - scale.y;
				}
			}
				break;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GetXScaleUntilBorder(float position) const {
			return region_limit.x - position;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GetYScaleUntilBorder(float position) const
		{
			return region_limit.y - position;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GetNextRowXPosition() const {
			return region_position.x + layout.next_row_padding + next_row_offset /*- region_render_offset.x*/;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t UIDrawer::GetRandomIntIdentifier(size_t index) const {
			return (index * index + (index & 15)) << (index & 7) >> 2;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::PushLateActions()
		{
			for (unsigned int index = 0; index < late_hoverables.size; index++) {
				system->ReaddHoverableToDockspaceRegion(dockspace, border_index, late_hoverables[index]);
			}

			ForEachMouseButton([&](ECS_MOUSE_BUTTON button_type) {
				for (unsigned int index = 0; index < late_clickables[button_type].size; index++) {
					system->ReaddClickableToDockspaceRegion(dockspace, border_index, late_clickables[button_type][index], button_type);
				}
			});

			for (unsigned int index = 0; index < late_generals.size; index++) {
				system->ReaddGeneralToDockspaceRegion(dockspace, border_index, late_generals[index]);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::UpdateCurrentColumnScale(float value) {
			current_column_x_scale = std::max(current_column_x_scale, value);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::UpdateCurrentRowScale(float value) {
			current_row_y_scale = std::max(current_row_y_scale, value);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::UpdateCurrentScale(float2 position, float2 value) {
			// currently only ColumnDraw and ColumnDrawFitSpace require z component
			draw_mode_extra_float.z = value.y;
			if (draw_mode != ECS_UI_DRAWER_COLUMN_DRAW && draw_mode != ECS_UI_DRAWER_COLUMN_DRAW_FIT_SPACE) {
				float scale = position.y - current_y + value.y;
				scale += region_render_offset.y;
				UpdateCurrentRowScale(scale);
			}
			float scale = position.x - current_x + value.x;
			scale += region_render_offset.x;
			UpdateCurrentColumnScale(scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::UpdateMaxRenderBoundsRectangle(float2 limits) {
			max_render_bounds.x = std::max(max_render_bounds.x, limits.x);
			max_render_bounds.y = std::max(max_render_bounds.y, limits.y);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::UpdateMinRenderBoundsRectangle(float2 position) {
			min_render_bounds.x = std::min(min_render_bounds.x, position.x);
			min_render_bounds.y = std::min(min_render_bounds.y, position.y);
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
			case ECS_UI_DRAWER_INDENT:
			case ECS_UI_DRAWER_FIT_SPACE:
				Indent();
				break;
			case ECS_UI_DRAWER_NEXT_ROW:
				if (~configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW) {
					NextRow();
				}
				else {
					Indent();
				}
				break;
			case ECS_UI_DRAWER_NEXT_ROW_COUNT:
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
			case ECS_UI_DRAWER_NOTHING:
				current_x += current_column_x_scale;
				current_column_x_scale = 0.0f;
				break;
			case ECS_UI_DRAWER_COLUMN_DRAW:
			case ECS_UI_DRAWER_COLUMN_DRAW_FIT_SPACE:
				if (draw_mode_target - 1 == draw_mode_count) {
					draw_mode_count = 0;
					current_x += current_column_x_scale + layout.element_indentation;
					current_y = draw_mode_extra_float.y;
					current_row_y_scale += draw_mode_extra_float.z + draw_mode_extra_float.x;
				}
				else {
					current_row_y_scale = draw_mode_count == 0 ? -draw_mode_extra_float.x : current_row_y_scale;
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
				if (~configuration & UI_CONFIG_DO_NOT_UPDATE_RENDER_BOUNDS) {
					UpdateRenderBoundsRectangle(position, scale);
				}
				UpdateCurrentScale(position, scale);
				HandleDrawMode(configuration);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename TextType>
		void UIDrawerElementName(UIDrawer* drawer, size_t configuration, const UIDrawConfig& config, TextType text, float2& position, float2 scale) {
			size_t label_configuration = UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
			UIDrawConfig label_config;
			UIConfigTextAlignment alignment;

			alignment.horizontal = ECS_UI_ALIGN_LEFT;
			alignment.vertical = ECS_UI_ALIGN_MIDDLE;

			if (configuration & UI_CONFIG_TEXT_PARAMETERS) {
				const UIConfigTextParameters* parameters = (const UIConfigTextParameters*)config.GetParameter(UI_CONFIG_TEXT_PARAMETERS);

				UIConfigTextParameters new_parameters = *parameters;
				if (configuration & UI_CONFIG_NAME_FIT_TO_SCALE) {
					float3 font_size_spacing = drawer->FitTextToScale(scale.y, drawer->element_descriptor.label_padd.y);
					new_parameters.size = { font_size_spacing.x, font_size_spacing.y };
					new_parameters.character_spacing = font_size_spacing.z;
				}
				label_config.AddFlag(new_parameters);
				label_configuration |= UI_CONFIG_TEXT_PARAMETERS;
			}
			else if (configuration & UI_CONFIG_NAME_FIT_TO_SCALE) {
				UIConfigTextParameters text_parameters;
				text_parameters.color = drawer->color_theme.text;

				float3 font_size_spacing = drawer->FitTextToScale(scale.y, drawer->element_descriptor.label_padd.y);
				text_parameters.character_spacing = font_size_spacing.z;
				text_parameters.size = { font_size_spacing.x, font_size_spacing.y };
				label_config.AddFlag(text_parameters);
				label_configuration |= UI_CONFIG_TEXT_PARAMETERS;
			}

			if (~configuration & UI_CONFIG_ELEMENT_NAME_FIRST) {
				position = drawer->GetCurrentPosition();
				// next row is here because it will always be called with UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW
				if (drawer->draw_mode == ECS_UI_DRAWER_INDENT || drawer->draw_mode == ECS_UI_DRAWER_FIT_SPACE 
					|| drawer->draw_mode == ECS_UI_DRAWER_NEXT_ROW) {
					if (position.x != drawer->GetNextRowXPosition()) {
						position.x -= drawer->layout.element_indentation;
					}
				}
			}
			else {
				label_configuration |= UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW;
			}

			bool omit_text = false;
			bool has_name_padding = HasFlag(configuration, UI_CONFIG_NAME_PADDING);
			float name_padding_total_length = 0.0f;
			if (has_name_padding) {
				const UIConfigNamePadding* name_padding = (const UIConfigNamePadding*)config.GetParameter(UI_CONFIG_NAME_PADDING);
				alignment.horizontal = name_padding->alignment;
				scale.x = name_padding->total_length < 0.0f ? scale.x : name_padding->total_length;
				scale.x += name_padding->offset_size;
				label_configuration |= UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X;
				omit_text = name_padding->omit_text;

				name_padding_total_length = scale.x;

				if constexpr (std::is_same_v<TextType, UIDrawerTextElement*>) {
					if (text->TextScale()->x >= scale.x - 2.0f * drawer->element_descriptor.label_padd.x) {
						// Make it a window dependent size such that it gets culled
						label_configuration |= UI_CONFIG_WINDOW_DEPENDENT_SIZE;
						UIConfigWindowDependentSize dependent_size;
						dependent_size.scale_factor.x = drawer->GetWindowSizeFactors(dependent_size.type, scale).x;
						label_config.AddFlag(dependent_size);
					}
				}
				else {
					float2 text_scale = drawer->GetLabelScale(text);
					if (text_scale.x >= scale.x - 2.0f * drawer->element_descriptor.label_padd.x) {
						// Make it a window dependent size such that it gets culled
						label_configuration |= UI_CONFIG_WINDOW_DEPENDENT_SIZE;
						UIConfigWindowDependentSize dependent_size;
						dependent_size.scale_factor.x = drawer->GetWindowSizeFactors(dependent_size.type, scale).x;
						label_config.AddFlag(dependent_size);
					}
				}
			}

			label_config.AddFlag(alignment);

			bool is_active = true;
			if (configuration & UI_CONFIG_ACTIVE_STATE) {
				const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				is_active = active_state->state;
			}

			label_configuration |= is_active ? 0 : UI_CONFIG_UNAVAILABLE_TEXT;
			label_configuration |= configuration & UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW ? UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW : 0;

			float2 draw_position = position;
			float2 draw_scale = scale;
			if (label_configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				if constexpr (std::is_same_v<TextType, UIDrawerTextElement*>) {
					if (!omit_text) {
						drawer->TextLabelDrawer(label_configuration, label_config, text, position, scale);
					}
					if (!has_name_padding) {
						draw_scale.x = text->scale.x + drawer->layout.element_indentation;
					}
					else {
						draw_scale.x = name_padding_total_length;
					}
					position.x += draw_scale.x;
				}
				else {
					if (!drawer->initializer) {
						if (!omit_text) {
							drawer->TextLabelWithCull(ClearFlag(label_configuration, UI_CONFIG_DO_CACHE), label_config, text, position, scale);
						}
						position.x += scale.x;
						draw_scale.x = scale.x;
					}
				}
			}
			else {
				if constexpr (std::is_same_v<TextType, UIDrawerTextElement*>) {
					if (!omit_text) {
						drawer->TextLabel(label_configuration, label_config, text, position, scale);
					}
					if (configuration & UI_CONFIG_ELEMENT_NAME_FIRST) {
						if (~configuration & UI_CONFIG_NAME_PADDING) {
							draw_scale.x = text->scale.x + drawer->element_descriptor.label_padd.x * 2.0f;
							position.x += draw_scale.x;
						}
						else {
							position.x += scale.x;
							draw_scale.x = scale.x;
						}
					}
				}
				else {
					if (!omit_text) {
						Stream<UISpriteVertex> text_vertices = drawer->GetTextStream(label_configuration, text.size * 6);
						if (!omit_text) {
							drawer->TextLabel(label_configuration, label_config, text, position, scale);
						}
						if (configuration & UI_CONFIG_ELEMENT_NAME_FIRST) {
							if (~configuration & UI_CONFIG_NAME_PADDING) {
								if (!omit_text) {
									draw_scale.x = GetTextSpan(text_vertices).x + drawer->element_descriptor.label_padd.x * 2.0f;
								}
								else {
									draw_scale.x = drawer->GetLabelScale(text).x;
								}
								position.x += draw_scale.x;
							}
							else {
								position.x += scale.x;
								draw_scale.x = scale.x;
							}
						}
					}
				}
			}

			draw_scale.x -= drawer->layout.element_indentation;

			// If it has an action placed on the name, then perform it
			if (configuration & UI_CONFIG_ELEMENT_NAME_ACTION) {
				const UIConfigElementNameAction* name_action = (const UIConfigElementNameAction*)config.GetParameter(UI_CONFIG_ELEMENT_NAME_ACTION);
				if (name_action->hoverable_handler.action != nullptr) {
					drawer->AddHoverable(configuration, draw_position, draw_scale, name_action->hoverable_handler);
				}
				if (name_action->clickable_handler.action != nullptr) {
					drawer->AddClickable(configuration, draw_position, draw_scale, name_action->clickable_handler);
				}
				if (name_action->general_handler.action != nullptr) {
					drawer->AddGeneral(configuration, draw_position, draw_scale, name_action->general_handler);
				}
			}

			if (configuration & UI_CONFIG_ELEMENT_NAME_BACKGROUND) {
				const UIConfigElementNameBackground* background = (const UIConfigElementNameBackground*)config.GetParameter(UI_CONFIG_ELEMENT_NAME_BACKGROUND);
				drawer->SolidColorRectangle(configuration, draw_position, draw_scale, background->color);
			}

			if (has_name_padding) {
				drawer->Indent(-1.0f);
			}
		}

		void UIDrawer::ElementName(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2& position, float2 scale) {
			UIDrawerElementName(this, configuration, config, text, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ElementName(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2& position, float2 scale) {
			UIDrawerElementName(this, configuration, config, text, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::ElementNameSize(size_t configuration, const UIDrawConfig& config, UIDrawerTextElement* text, float2 scale)
		{
			if (configuration & UI_CONFIG_NAME_PADDING) {
				const UIConfigNamePadding* name_padding = (const UIConfigNamePadding*)config.GetParameter(UI_CONFIG_NAME_PADDING);
				float current_scale = name_padding->total_length < 0.0f ? scale.x : name_padding->total_length;
				current_scale += name_padding->offset_size;
				return current_scale;
			}
			return text->scale.x + element_descriptor.label_padd.x * 2.0f;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::ElementNameSize(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2 scale)
		{
			if (configuration & UI_CONFIG_NAME_PADDING) {
				const UIConfigNamePadding* name_padding = (const UIConfigNamePadding*)config.GetParameter(UI_CONFIG_NAME_PADDING);
				float current_scale = name_padding->total_length < 0.0f ? scale.x : name_padding->total_length;
				current_scale += name_padding->offset_size;
				return current_scale;
			}
			return GetLabelScale(text).x;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::ExistsResource(Stream<char> name) {
			Stream<char> string_identifier = HandleResourceIdentifier(name);
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
		) : dockspace(descriptor.dockspace), window_index(descriptor.window_index),
			border_index(descriptor.border_index), dockspace_type(descriptor.dockspace_type), buffers(descriptor.buffers),
			counts(descriptor.counts), system_buffers(descriptor.system_buffers), system_counts(descriptor.system_counts),
			window_data(_window_data), mouse_position(descriptor.mouse_position), color_theme(descriptor.color_theme),
			font(descriptor.font), layout(descriptor.layout), element_descriptor(descriptor.element_descriptor),
			export_scale(descriptor.export_scale), initializer(_initializer), record_actions(descriptor.record_handlers),
			record_snapshot_runnables(descriptor.record_snapshot_runnables)
		{
			if (record_snapshot_runnables) {
				// In order to have an accurate snapshot, we need to record actions as well
				record_actions = true;
			}

			system = (UISystem*)descriptor.system;

			zoom_ptr = &system->m_windows[window_index].zoom;
			zoom_ptr_modifyable = &system->m_windows[window_index].zoom;

			DefaultDrawParameters();

			//SetLayoutFromZoomXFactor<true>(zoom_ptr->x);
			//SetLayoutFromZoomYFactor<true>(zoom_ptr->y);

			if (!initializer) {
				float mask = GetDockspaceMaskFromType(dockspace_type);
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

				AllocatorPolymorphic system_allocator = system->Allocator();
				late_hoverables.Initialize(system_allocator, ECS_TOOLS_UI_MISC_DRAWER_LATE_ACTION_CAPACITY);
				ForEachMouseButton([&](ECS_MOUSE_BUTTON button_type) {
					late_clickables[button_type].Initialize(system_allocator, ECS_TOOLS_UI_MISC_DRAWER_LATE_ACTION_CAPACITY);
				});
				late_generals.Initialize(system_allocator, ECS_TOOLS_UI_MISC_DRAWER_LATE_ACTION_CAPACITY);

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

			cached_filled_action_data = system->GetFilledActionData(window_index);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawer::~UIDrawer() {
			PushLateActions();

			if (!initializer) {
				float2 render_span = GetRenderSpan();
				float2 render_zone = GetRenderZone();

				float2 difference = render_span - render_zone;
				difference.x = ClampMin(difference.x, 0.0f);
				difference.y = ClampMin(difference.y, 0.0f);
				system->SetWindowDrawerDifferenceSpan(window_index, difference);

				if (export_scale != nullptr) {
					float y_offset = dockspace->borders[border_index].draw_region_header * system->m_descriptors.misc.title_y_scale;
					*export_scale = { render_span.x + 2.0f * layout.next_row_padding, render_span.y + 2.0f * layout.next_row_y_offset + y_offset };
				}

				ViewportRegionSliders(
					"##HorizontalSlider",
					"##VerticalSlider",
					render_span,
					render_zone,
					&system->m_windows[window_index].render_region_offset
				);
			}

			if (deallocate_constructor_allocations) {
				system->m_memory->Deallocate(current_identifier.buffer);
				system->m_memory->Deallocate(identifier_stack.buffer);

				late_hoverables.FreeBuffer();
				ForEachMouseButton([&](ECS_MOUSE_BUTTON button_type) {
					late_clickables[button_type].FreeBuffer();
				});
				late_generals.FreeBuffer();

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

		void UIDrawer::AddHoverable(size_t configuration, float2 position, float2 scale, UIActionHandler handler, UIHandlerCopyBuffers copy_function) {
			if (!initializer && record_actions) {
				system->AddHoverableToDockspaceRegion(
					dockspace,
					border_index,
					position,
					scale,
					handler,
					copy_function
				);
				AddLateOrSystemAction(this, configuration, true, false, ECS_MOUSE_BUTTON_COUNT);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddTextTooltipHoverable(size_t configuration, float2 position, float2 scale, UITextTooltipHoverableData* data, bool stable, ECS_UI_DRAW_PHASE phase) {
			if (!initializer && record_actions) {
				size_t total_storage[256];
				unsigned int write_size = sizeof(*data);
				if (!stable) {
					UITextTooltipHoverableData* stack_data = (UITextTooltipHoverableData*)total_storage;
					memcpy(stack_data, data, sizeof(*data));
					write_size = stack_data->Write(data->characters);
					data = stack_data;
				}
				system->AddHoverableToDockspaceRegion(dockspace, border_index, position, scale, { TextTooltipHoverable, data, write_size, phase });
				AddLateOrSystemAction(this, configuration, true, false, ECS_MOUSE_BUTTON_COUNT);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddClickable(
			size_t configuration,
			float2 position,
			float2 scale,
			UIActionHandler handler,
			ECS_MOUSE_BUTTON button_type,
			UIHandlerCopyBuffers copy_function
		) {
			if (!initializer && record_actions) {
				system->AddClickableToDockspaceRegion(
					dockspace,
					border_index,
					position,
					scale,
					handler,
					button_type,
					copy_function
				);
				AddLateOrSystemAction(this, configuration, false, false, button_type);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddGeneral(
			size_t configuration,
			float2 position,
			float2 scale,
			UIActionHandler handler,
			UIHandlerCopyBuffers copy_function
		) {
			if (!initializer && record_actions) {
				system->AddGeneralActionToDockspaceRegion(
					dockspace,
					border_index,
					position,
					scale,
					handler,
					copy_function
				);
				AddLateOrSystemAction(this, configuration, false, true, ECS_MOUSE_BUTTON_COUNT);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultHoverable(
			size_t configuration,
			float2 position,
			float2 scale,
			Color color,
			float percentage,
			ECS_UI_DRAW_PHASE phase
		) {
			if (!initializer && record_actions) {
				UISystemDefaultHoverableData data;
				data.border_index = border_index;
				data.data.colors[0] = color;
				data.dockspace = dockspace;
				data.data.percentages[0] = percentage;
				data.position = position;
				data.scale = scale;
				data.phase = phase;
				system->AddDefaultHoverable(data);

				AddLateOrSystemAction(this, configuration, true, false, ECS_MOUSE_BUTTON_COUNT);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultHoverable(
			size_t configuration,
			float2 main_position,
			float2 main_scale,
			const float2* positions,
			const float2* scales,
			const Color* colors,
			const float* percentages,
			unsigned int count,
			ECS_UI_DRAW_PHASE phase
		) {
			if (!initializer && record_actions) {
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
				data.phase = phase;
				data.position = main_position;
				data.scale = main_scale;
				system->AddDefaultHoverable(data);

				AddLateOrSystemAction(this, configuration, true, false, ECS_MOUSE_BUTTON_COUNT);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultClickable(
			size_t configuration,
			float2 position,
			float2 scale,
			UIActionHandler hoverable_handler,
			UIActionHandler clickable_handler,
			UIHandlerCopyBuffers hoverable_copy_function,
			UIHandlerCopyBuffers clickable_copy_function,
			ECS_MOUSE_BUTTON button_type
		) {
			if (!initializer && record_actions) {
				UISystemDefaultClickableData data;
				data.border_index = border_index;
				data.clickable_handler = clickable_handler;
				data.hoverable_handler = hoverable_handler;
				data.dockspace = dockspace;
				data.position = position;
				data.scale = scale;
				data.button_type = button_type;
				data.hoverable_copy_function = hoverable_copy_function;
				data.clickable_copy_function = clickable_copy_function;
				system->AddDefaultClickable(data);

				AddLateOrSystemAction(this, configuration, true, false, button_type);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultClickable(
			UIReservedHandler reserved_hoverable,
			UIReservedHandler reserved_clickable,
			float2 position, 
			float2 scale, 
			UIActionHandler hoverable_handler,
			UIActionHandler clickable_handler,
			UIHandlerCopyBuffers hoverable_copy_function,
			UIHandlerCopyBuffers clickable_copy_function,
			ECS_MOUSE_BUTTON button_type
		)
		{
			if (!initializer && record_actions) {
				UISystemDefaultClickableData data;
				data.border_index = border_index;
				data.clickable_handler = clickable_handler;
				data.hoverable_handler = hoverable_handler;
				data.dockspace = dockspace;
				data.position = position;
				data.scale = scale;
				data.button_type = button_type;
				data.hoverable_copy_function = hoverable_copy_function;
				data.clickable_copy_function = clickable_copy_function;
				system->WriteReservedDefaultClickable(reserved_hoverable, reserved_clickable, data);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDoubleClickAction(
			size_t configuration,
			float2 position,
			float2 scale,
			size_t duration_between_clicks,
			UIActionHandler first_click_handler,
			UIActionHandler second_click_handler,
			unsigned int identifier
		) {
			if (!initializer && record_actions) {
				UISystemDoubleClickData register_data;
				register_data.border_index = border_index;
				register_data.dockspace = dockspace;
				register_data.position = position;
				register_data.scale = scale;
				register_data.duration_between_clicks = duration_between_clicks;
				register_data.first_click_handler = first_click_handler;
				register_data.second_click_handler = second_click_handler;
				
				register_data.is_identifier_int = true;
				register_data.identifier = identifier;
				system->AddDoubleClickActionToDockspaceRegion(register_data);

				AddLateOrSystemAction(this, configuration, false, true, ECS_MOUSE_BUTTON_COUNT);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDoubleClickAction(
			size_t configuration,
			float2 position,
			float2 scale,
			size_t duration_between_clicks,
			UIActionHandler first_click_handler,
			UIActionHandler second_click_handler,
			Stream<char> identifier
		) {
			if (!initializer && record_actions) {
				UISystemDoubleClickData register_data;
				register_data.border_index = border_index;
				register_data.dockspace = dockspace;
				register_data.position = position;
				register_data.scale = scale;
				register_data.duration_between_clicks = duration_between_clicks;
				register_data.first_click_handler = first_click_handler;
				register_data.second_click_handler = second_click_handler;

				register_data.is_identifier_int = false;
				ECS_ASSERT(identifier.size <= sizeof(register_data.identifier_char));
				identifier.CopyTo(register_data.identifier_char);
				register_data.identifier_char_count = identifier.size;
				system->AddDoubleClickActionToDockspaceRegion(register_data);

				AddLateOrSystemAction(this, configuration, false, true, ECS_MOUSE_BUTTON_COUNT);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerMenuRightClickData UIDrawer::PrepareRightClickMenuActionData(
			Stream<char> name, 
			UIDrawerMenuState* menu_state, 
			UIActionHandler custom_handler,
			AllocatorPolymorphic override_allocator
		)
		{
			UIDrawerMenuRightClickData right_click;

			menu_state->submenu_index = 0;
			size_t menu_data_size = MenuWalkStatesMemory(menu_state, true);
			size_t total_size = menu_data_size + name.size + sizeof(UIDrawerMenuWindow) * ECS_TOOLS_UI_MENU_SUBMENUES_MAX_COUNT + sizeof(CapacityStream<UIDrawerMenuWindow>);
			ECS_ASSERT(custom_handler.data_size <= sizeof(right_click.action_data), "The handler data is too large");

			void* allocated_buffer = nullptr;
			if (override_allocator.allocator == nullptr) {
				allocated_buffer = GetTempBuffer(total_size, ECS_UI_DRAW_SYSTEM);
			}
			else {
				allocated_buffer = Allocate(override_allocator, total_size);
			}

			uintptr_t ptr = (uintptr_t)allocated_buffer;
			name.InitializeAndCopy(ptr, name);

			UIDrawerMenuWindow* menu_windows = (UIDrawerMenuWindow*)ptr;
			ptr += sizeof(UIDrawerMenuWindow) * ECS_TOOLS_UI_MENU_SUBMENUES_MAX_COUNT;
			CapacityStream<UIDrawerMenuWindow>* menu_windows_stream = (CapacityStream<UIDrawerMenuWindow>*)ptr;
			ptr += sizeof(CapacityStream<UIDrawerMenuWindow>);
			menu_windows_stream->InitializeFromBuffer(menu_windows, 0, ECS_TOOLS_UI_MENU_SUBMENUES_MAX_COUNT);

			MenuSetStateBuffers(menu_state, ptr, menu_windows_stream, 0, true);

			right_click.name = name;
			right_click.action = custom_handler.action;
			if (custom_handler.data_size > 0) {
				memcpy(right_click.action_data, custom_handler.data, custom_handler.data_size);
				right_click.is_action_data_ptr = false;
			}
			else {
				right_click.is_action_data_ptr = true;
				right_click.action_data_ptr = custom_handler.data;
			}

			right_click.state = *menu_state;
			right_click.window_index = window_index;

			return right_click;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIActionHandler UIDrawer::PrepareRightClickMenuHandler(
			Stream<char> name, 
			UIDrawerMenuState* menu_state, 
			UIActionHandler custom_handler,
			AllocatorPolymorphic override_allocator
		)
		{
			UIDrawerMenuRightClickData* right_click = nullptr;
			if (override_allocator.allocator == nullptr) {
				right_click = (UIDrawerMenuRightClickData*)GetTempBuffer(sizeof(UIDrawerMenuRightClickData));
			}
			else {
				right_click = (UIDrawerMenuRightClickData*)Allocate(override_allocator, sizeof(UIDrawerMenuRightClickData));
			}

			*right_click = PrepareRightClickMenuActionData(name, menu_state, custom_handler, override_allocator);
			return { RightClickMenu, right_click, sizeof(*right_click), ECS_UI_DRAW_SYSTEM };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddRightClickMenuAction(
			size_t configuration, 
			float2 position, 
			float2 scale, 
			Stream<char> name, 
			UIDrawerMenuState* menu_state, 
			UIActionHandler custom_handler
		)
		{
			if (record_actions) {
				AddClickable(configuration, position, scale, PrepareRightClickMenuHandler(name, menu_state, custom_handler), ECS_MOUSE_RIGHT);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultClickableHoverable(
			size_t configuration,
			float2 position,
			float2 scale,
			UIActionHandler handler,
			UIHandlerCopyBuffers copy_function,
			Color color,
			float percentage,
			ECS_UI_DRAW_PHASE hoverable_phase,
			ECS_MOUSE_BUTTON button_type
		) {
			if (!initializer && record_actions) {
				UISystemDefaultHoverableClickableData data;
				data.border_index = border_index;
				data.clickable_handler = handler;
				if (color == ECS_COLOR_WHITE) {
					data.hoverable_data.colors[0] = color_theme.theme;
				}
				else {
					data.hoverable_data.colors[0] = color;

				}
				data.hoverable_phase = hoverable_phase;
				data.dockspace = dockspace;
				data.hoverable_data.percentages[0] = percentage;
				data.position = position;
				data.scale = scale;
				data.button_type = button_type;
				data.clickable_copy_function = copy_function;
				system->AddDefaultHoverableClickable(data);

				AddLateOrSystemAction(this, configuration, true, false, button_type);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultClickableHoverable(
			UIReservedHandler reserved_hoverable, 
			UIReservedHandler reserved_clickable,
			float2 position, 
			float2 scale, 
			UIActionHandler handler,
			UIHandlerCopyBuffers copy_function,
			Color color, 
			float percentage,
			ECS_UI_DRAW_PHASE hoverable_phase,
			ECS_MOUSE_BUTTON button_type
		)
		{
			if (!initializer && record_actions) {
				UISystemDefaultHoverableClickableData data;
				data.border_index = border_index;
				data.clickable_handler = handler;
				if (color == ECS_COLOR_WHITE) {
					data.hoverable_data.colors[0] = color_theme.theme;
				}
				else {
					data.hoverable_data.colors[0] = color;

				}
				data.hoverable_phase = hoverable_phase;
				data.dockspace = dockspace;
				data.hoverable_data.percentages[0] = percentage;
				data.position = position;
				data.scale = scale;
				data.button_type = button_type;
				data.clickable_copy_function = copy_function;
				system->WriteReservedDefaultHoverableClickable(reserved_hoverable, reserved_clickable, data);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultClickableHoverable(
			size_t configuration,
			float2 main_position,
			float2 main_scale,
			const float2* positions,
			const float2* scales,
			const Color* colors,
			const float* percentages,
			unsigned int count,
			UIActionHandler handler,
			UIHandlerCopyBuffers copy_function,
			ECS_MOUSE_BUTTON button_type
		) {
			if (!initializer && record_actions) {
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
				data.button_type = button_type;
				data.clickable_copy_function = copy_function;
				system->AddDefaultHoverableClickable(data);

				AddLateOrSystemAction(this, configuration, true, false, button_type);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::AddDefaultClickableHoverable(
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
			UIHandlerCopyBuffers copy_function,
			ECS_MOUSE_BUTTON button_type
		)
		{
			if (!initializer && record_actions) {
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
				data.button_type = button_type;
				data.clickable_copy_function = copy_function;
				system->WriteReservedDefaultHoverableClickable(reserved_hoverable, reserved_clickable, data);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SnapshotRunnable(UIDockspaceBorderDrawSnapshotRunnable runnable)
		{
			ActionData action_data = GetDummyActionData();
			if (runnable.draw_phase == ECS_UI_DRAW_SYSTEM) {
				action_data.buffers = system_buffers;
				action_data.counts = system_counts;
			}
			else {
				action_data.buffers = buffers;
				action_data.counts = counts;
			}
			runnable.function(runnable.data, &action_data);
			AddSnapshotRunnable(runnable);
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
			transform.scale = GetRelativeTransformFactors({ x_difference, y_difference });
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

		UIDrawerArrayData* UIDrawer::ArrayInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, size_t element_count) {
			UIDrawerArrayData* data = nullptr;

			ECS_STACK_CAPACITY_STREAM(char, data_name, 256);
			data_name.CopyOther(name);
			data_name.AddStream(" data");
			data_name.AddAssert('\0');

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(data_name.buffer, [&](Stream<char> identifier) {
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
				data->add_callback_phase = ECS_UI_DRAW_NORMAL;
				data->remove_callback_phase = ECS_UI_DRAW_NORMAL;
				data->remove_anywhere_index = -1;

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

				if (configuration & UI_CONFIG_ARRAY_REMOVE_ANYWHERE) {
					const UIConfigArrayRemoveAnywhere* callback = (const UIConfigArrayRemoveAnywhere*)config.GetParameter(UI_CONFIG_ARRAY_REMOVE_ANYWHERE);
					if (callback->handler.data_size > 0) {
						void* allocation = GetMainAllocatorBuffer(callback->handler.data_size);
						memcpy(allocation, callback->handler.data, callback->handler.data_size);
						data->remove_anywhere_callback_data = allocation;
					}
					else {
						data->remove_anywhere_callback_data = callback->handler.data;
					}

					data->remove_anywhere_callback = callback->handler.action;
					data->remove_anywhere_callback_phase = callback->handler.phase;
				}

				return data;
				});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

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
			Stream<char> label,
			UIActionHandler handler
		) {
			UIDrawConfig config;
			Button(0, config, label, handler);
		}

		void UIDrawer::Button(
			size_t configuration,
			const UIDrawConfig& config,
			Stream<char> text,
			UIActionHandler handler
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			bool is_active = true;

			if (!initializer) {
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = state->state;
				}

				configuration |= is_active ? 0 : UI_CONFIG_UNAVAILABLE_TEXT;
				size_t label_configuration = ClearFlag(configuration, UI_CONFIG_TOOL_TIP);

				size_t previous_text_sprite_count = *HandleTextSpriteCount(configuration);
				if (~configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					TextLabel(label_configuration, config, text, position, scale);
				}
				else {
					TextLabelWithCull(label_configuration, config, text, position, scale);
				}

				if (is_active) {
					if (~configuration & UI_CONFIG_BUTTON_HOVERABLE) {
						Color label_color = HandleColor(configuration, config);

						if (handler.phase == ECS_UI_DRAW_NORMAL) {
							UIDefaultHoverableData hoverable_data;
							hoverable_data.colors[0] = label_color;
							hoverable_data.percentages[0] = 1.25f;

							ECS_UI_DRAW_PHASE phase = ECS_UI_DRAW_NORMAL;
							if (configuration & UI_CONFIG_LATE_DRAW) {
								phase = ECS_UI_DRAW_LATE;
							}
							else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
								phase = ECS_UI_DRAW_SYSTEM;
							}
							AddDefaultClickable(configuration, position, scale, { DefaultHoverableAction, &hoverable_data, sizeof(hoverable_data), phase }, handler);
						}
						else {
							UISpriteVertex* sprite_vertex = HandleTextSpriteBuffer(configuration);
							size_t* count = HandleTextSpriteCount(configuration);

							if (*count > previous_text_sprite_count) {
								if (record_actions) {
									size_t text_vertex_count = *count - previous_text_sprite_count;

									UIDefaultTextHoverableData hoverable_data;
									hoverable_data.color = label_color;

									float2 text_position = sprite_vertex[*count - text_vertex_count].position;
									text_position.y = -text_position.y;
									float2 offset = text_position - position;
									hoverable_data.text_offset = offset;

									Color text_color;
									float2 text_size;
									float text_character_spacing;
									HandleText(configuration, config, text_color, text_size, text_character_spacing);

									hoverable_data.character_spacing = text_character_spacing;
									hoverable_data.font_size = text_size;
									hoverable_data.text_color = text_color;

									ECS_UI_DRAW_PHASE hoverable_phase = ECS_UI_DRAW_NORMAL;
									if (configuration & UI_CONFIG_LATE_DRAW) {
										hoverable_phase = ECS_UI_DRAW_LATE;
									}
									else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
										hoverable_phase = ECS_UI_DRAW_SYSTEM;
									}

									AddDefaultClickable(configuration, position, scale, { DefaultTextHoverableAction, &hoverable_data, sizeof(hoverable_data), hoverable_phase }, handler);

									UIDefaultTextHoverableData* handler_hoverable_data = (UIDefaultTextHoverableData*)system->GetLastHoverableData(dockspace, border_index);
									Stream<char> identifier = HandleResourceIdentifier(text);
									if (record_snapshot_runnables) {
										handler_hoverable_data->text.buffer = (char*)Allocate(SnapshotRunnableAllocator(), identifier.size, alignof(char));
									}
									else {
										handler_hoverable_data->text.buffer = (char*)GetHandlerBuffer(identifier.size, hoverable_phase);
									}
									handler_hoverable_data->text.CopyOther(identifier);
								}
							}
						}
					}
					else {
						const UIConfigButtonHoverable* hoverable = (const UIConfigButtonHoverable*)config.GetParameter(UI_CONFIG_BUTTON_HOVERABLE);
						AddDefaultClickable(configuration, position, scale, hoverable->handler, handler);
						AddHoverable(configuration, position, scale, hoverable->handler);
					}
				}

				// Handle the tool tip at the end such that we can compose with the existing hoverable
				HandleTextToolTip(configuration, config, position, scale);
			}
			else {
				TextLabel(configuration, config, text);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ButtonWide(Stream<wchar_t> label, UIActionHandler handler) {
			ECS_STACK_CAPACITY_STREAM(char, ascii_label, 512);
			ECS_ASSERT(label.size < 512);

			ConvertWideCharsToASCII(label, ascii_label);
			Button(ascii_label, handler);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ButtonWide(size_t configuration, const UIDrawConfig& config, Stream<wchar_t> label, UIActionHandler handler) {
			ECS_STACK_CAPACITY_STREAM(char, ascii_label, 512);
			ECS_ASSERT(label.size < 512);

			ConvertWideCharsToASCII(label, ascii_label);
			Button(configuration, config, ascii_label, handler);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::StateButton(Stream<char> name, bool* state) {
			UIDrawConfig config;
			StateButton(0, config, name, state);
		}

		void UIDrawer::StateButton(size_t configuration, const UIDrawConfig& config, Stream<char> name, bool* state) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				bool is_active = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				if (is_active) {
					TextLabel(ClearFlag(configuration, UI_CONFIG_DO_CACHE), config, name, position, scale);
					Color color = HandleColor(configuration, config);
					if (*state) {
						color = LightenColorClamp(color, 1.4f);
						SolidColorRectangle(configuration, position, scale, color);
					}

					AddDefaultClickableHoverable(configuration, position, scale, { BoolClickable, state, 0 }, nullptr, color);
				}
				else {
					TextLabel(ClearFlag(configuration, UI_CONFIG_DO_CACHE) | UI_CONFIG_UNAVAILABLE_TEXT, config, name, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteStateButton(
			Stream<wchar_t> texture,
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
			Stream<wchar_t> texture,
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
					Color background_color = HandleColor(configuration, config);
					if (*state) {
						background_color = LightenColorClamp(background_color, 1.4f);
					}
					if (~configuration & UI_CONFIG_SPRITE_STATE_BUTTON_NO_BACKGROUND_WHEN_DESELECTED) {
						SolidColorRectangle(configuration, position, scale, background_color);
					}
					else {
						if (*state) {
							SolidColorRectangle(configuration, position, scale, background_color);
						}
					}
					float2 sprite_scale;
					float2 sprite_position = ExpandRectangle(position, scale, expand_factor, sprite_scale);
					if (is_active) {
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);
						AddDefaultClickableHoverable(configuration, position, scale, { BoolClickable, state, 0 }, nullptr, background_color);
					}
					else {
						color.alpha *= color_theme.alpha_inactive_item;
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);
					}

					HandleBorder(configuration, config, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// Stateless, label is done with UI_CONFIG_DO_NOT_CACHE; if drawing with a sprite
		// name can be made nullptr
		void UIDrawer::MenuButton(Stream<char> name, UIWindowDescriptor& window_descriptor, size_t border_flags) {
			UIDrawConfig config;
			MenuButton(0, config, name, window_descriptor, border_flags);
		}

		// Stateless, label is done without UI_CONFIG_DO_CACHE; if drawing with a sprite
		// name can be made nullptr
		void UIDrawer::MenuButton(size_t configuration, const UIDrawConfig& config, Stream<char> name, UIWindowDescriptor& window_descriptor, size_t border_flags) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				bool is_active = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				}

				if (~configuration & UI_CONFIG_MENU_BUTTON_SPRITE) {
					configuration |= is_active ? 0 : UI_CONFIG_UNAVAILABLE_TEXT;
					TextLabel(ClearFlag(configuration, UI_CONFIG_DO_CACHE), config, name, position, scale);
				}
				else {
					const UIConfigMenuButtonSprite* sprite_definition = (const UIConfigMenuButtonSprite*)config.GetParameter(UI_CONFIG_MENU_BUTTON_SPRITE);
					Color sprite_color = sprite_definition->color;
					sprite_color.alpha = is_active ? sprite_color.alpha : sprite_color.alpha * color_theme.alpha_inactive_item;

					if (sprite_definition->texture.size > 0) {
						SpriteRectangle(
							configuration,
							position,
							scale,
							sprite_definition->texture,
							sprite_color,
							sprite_definition->top_left_uv,
							sprite_definition->bottom_right_uv
						);
					}
					else {
						ECS_ASSERT(sprite_definition->resource_view.Interface() == nullptr);
						SpriteRectangle(
							configuration,
							position,
							scale,
							sprite_definition->resource_view,
							sprite_color,
							sprite_definition->top_left_uv,
							sprite_definition->bottom_right_uv
						);
					}

					FinalizeRectangle(configuration, position, scale);
				}

				if (is_active) {
					// If the descriptor has any data with size > 0, we need to copy it as well
					size_t _button_data_storage[512];
					UIDrawerMenuButtonData* data = (UIDrawerMenuButtonData*)_button_data_storage;

					data->descriptor = window_descriptor;
					data->descriptor.initial_position_x = position.x;
					data->descriptor.initial_position_y = position.y + scale.y + element_descriptor.menu_button_padding;
					data->border_flags = border_flags;
					data->is_opened_when_pressed = false;

					unsigned int write_size = data->Write();

					Color color = HandleColor(configuration, config);

					AddClickable(configuration, position, scale, { MenuButtonAction, data, write_size, ECS_UI_DRAW_SYSTEM });
					ECS_UI_DRAW_PHASE hovered_phase = ECS_UI_DRAW_NORMAL;
					if (configuration & UI_CONFIG_LATE_DRAW) {
						hovered_phase = ECS_UI_DRAW_LATE;
					}
					else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
						hovered_phase = ECS_UI_DRAW_SYSTEM;
					}

					AddDefaultHoverable(configuration, position, scale, color, 1.25f, hovered_phase);

					unsigned int pop_up_window_index = system->GetWindowFromName(window_descriptor.window_name);
					if (pop_up_window_index != -1) {
						//system->SetPopUpWindowPosition(pop_up_window_index, { position.x, position.y + scale.y + element_descriptor.menu_button_padding });
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

		void UIDrawer::CheckBox(Stream<char> name, bool* value_to_change) {
			UIDrawConfig config;
			CheckBox(0, config, name, value_to_change);
		}

		void UIDrawer::CheckBox(size_t configuration, const UIDrawConfig& config, Stream<char> name, bool* value_to_change) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				scale = GetSquareScale(scale.y);

				if (configuration & UI_CONFIG_DO_CACHE) {
					UIDrawerTextElement* element = (UIDrawerTextElement*)GetResource(name);

					CheckBoxDrawer(configuration, config, element, value_to_change, position, scale);
				}
				else {
					CheckBoxDrawer(configuration, config, name, value_to_change, position, scale);
				}
			}
			else {
				if (configuration & UI_CONFIG_DO_CACHE) {
					CheckBoxInitializer(configuration, config, name, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion 

#pragma region Combo box

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ComboBox(Stream<char> name, Stream<Stream<char>> labels, unsigned int label_display_count, unsigned char* active_label) {
			UIDrawConfig config;
			ComboBox(0, config, name, labels, label_display_count, active_label);
		}

		void UIDrawer::ComboBox(size_t configuration, UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels, unsigned int label_display_count, unsigned char* active_label) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			
			label_display_count = label_display_count > labels.size ? labels.size : label_display_count;
			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
					UIDrawerComboBox* data = (UIDrawerComboBox*)GetResource(name);

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

					UIDrawerComboBox* data = (UIDrawerComboBox*)GetResource(name);
					// Check to see if the labels have changed
					bool has_changed = false;
					if (data->labels.size != labels.size) {
						has_changed = true;
					}
					else {
						for (size_t index = 0; index < labels.size; index++) {
							if (labels[index] != data->labels[index]) {
								has_changed = true;
								break;
							}
						}
					}

					if (has_changed) {
						// Need to update manually the labels before calling into it
						size_t allocation_size = StreamCoalescedDeepCopySize(labels);
						void* allocation = GetMainAllocatorBuffer(allocation_size);
						// Notify the dynamic element that the allocation has changed
						unsigned int dynamic_index = system->GetWindowDynamicElement(window_index, HandleResourceIdentifier(name));
						ECS_ASSERT(dynamic_index != -1);
						system->ReplaceWindowDynamicResourceAllocation(window_index, dynamic_index, data->labels.buffer, allocation);
						RemoveAllocation(data->labels.buffer);

						uintptr_t ptr = (uintptr_t)allocation;
						data->labels = StreamCoalescedDeepCopy(labels, ptr);

						float current_max_x = 0.0f;
						for (size_t index = 0; index < labels.size; index++) {
							float current_label_scale = TextSpan(labels[index]).x;
							if (current_label_scale + 2 * element_descriptor.label_padd.x > current_max_x) {
								data->biggest_label_x_index = index;
								current_max_x = current_label_scale + 2 * element_descriptor.label_padd.x;
							}
						}
						data->label_display_count = label_display_count;
					}

					ComboBox(DynamicConfiguration(configuration), config, name, labels, label_display_count, active_label);
				}
			}
			else {
				if (configuration & UI_CONFIG_DO_CACHE) {
					ComboBoxInitializer(configuration, config, name, labels, label_display_count, active_label, scale);
				}
			}

		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::ComboBoxDefaultSize(Stream<Stream<char>> labels, Stream<char> name, Stream<char> prefix) const
		{
			float maximum_label_scale = labels.size > 0 ? -1000.0f : 0.0f;
			for (size_t index = 0; index < labels.size; index++) {
				maximum_label_scale = std::max(maximum_label_scale, TextSpan(labels[index]).x);
			}

			float size = maximum_label_scale + element_descriptor.label_padd.x * 2.0f;
			if (prefix.size > 0) {
				size += TextSpan(prefix).x;
			}

			if (name.size > 0) {
				size += TextSpan(name).x + layout.element_indentation;
			}
			
			size += GetSquareScale(layout.default_element_y).x;
			
			return size;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIDrawer::ComboBoxBiggestLabel(Stream<Stream<char>> labels) const
		{
			float maximum_label_scale = -1000.0f;
			unsigned int biggest_index = 0;

			for (size_t index = 0; index < labels.size; index++) {
				float current_span = TextSpan(labels[index]).x;
				if (current_span > maximum_label_scale) {
					maximum_label_scale = current_span;
					biggest_index = index;
				}
			}

			return biggest_index;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ComboBoxDropDownDrawer(size_t configuration, UIDrawConfig& config, UIDrawerComboBox* data, Stream<char> target_window_name) {
			float2 position = region_position - region_render_offset;
			float2 scale = { region_scale.x, data->label_y_scale };

			size_t text_label_configuration = UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_TEXT_ALIGNMENT
				| UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_DO_NOT_ADVANCE;

			text_label_configuration |= configuration & UI_CONFIG_COLOR ? UI_CONFIG_COLOR : 0;
			text_label_configuration |= configuration & UI_CONFIG_TEXT_PARAMETERS ? UI_CONFIG_TEXT_PARAMETERS : 0;

			UIConfigRelativeTransform relative_transform;
			relative_transform.scale = GetRelativeTransformFactors(scale);
			config.AddFlag(relative_transform);

			Color color = HandleColor(configuration, config);

			for (size_t index = 0; index < data->labels.size; index++) {
				if (ValidatePosition(0, position, scale)) {
					UIDrawerComboBoxLabelClickable click_data;
					click_data.index = index;
					click_data.box = data;
					// This is stable, doesn't need to be allocated
					click_data.target_window_name = target_window_name;

					if (data->unavailables != nullptr) {
						UIConfigActiveState active_state;
						active_state.state = !data->unavailables[index];
						config.AddFlag(active_state);

						text_label_configuration |= UI_CONFIG_ACTIVE_STATE;
					}

					Button(text_label_configuration, config, data->labels[index], { ComboBoxLabelClickable, &click_data, sizeof(click_data), ECS_UI_DRAW_SYSTEM });

					if (data->unavailables != nullptr) {
						config.flag_count--;
						text_label_configuration ^= UI_CONFIG_ACTIVE_STATE;
					}
				}
				FinalizeRectangle(0, position, { 0.0f, scale.y });
				NextRow();
				position.y += scale.y;
			}

			//if (position.y < region_position.y + region_scale.y) {
				//SolidColorRectangle(text_label_configuration, { position.x + region_render_offset.x, position.y }, { 2.0f, region_position.y + region_scale.y - position.y }, color);
			//}

			config.flag_count--;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Color Input

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ColorInput(Stream<char> name, Color* color, Color default_color) {
			UIDrawConfig config;
			ColorInput(0, config, name, color, default_color);
		}

		void UIDrawer::ColorInput(size_t configuration, UIDrawConfig& config, Stream<char> name, Color* color, Color default_color) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
					ColorInputInitializer(configuration, config, name, color, default_color, position, scale);
				}
			}

		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Color Float Input

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ColorFloatInput(Stream<char> name, ColorFloat* color, ColorFloat default_color) {
			UIDrawConfig config;
			ColorFloatInput(0, config, name, color, default_color);
		}

		void UIDrawer::ColorFloatInput(size_t configuration, UIDrawConfig& config, Stream<char> name, ColorFloat* color, ColorFloat default_color) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
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

		void UIDrawer::ColorFloatInputDrawer(size_t configuration, UIDrawConfig& config, Stream<char> name, ColorFloat* color, float2 position, float2 scale) {
			float min_x_scale = GetSquareScale(scale.y).x + layout.element_indentation + layout.default_element_x * 2;
			scale.x = ClampMin(scale.x, min_x_scale);

			Stream<char> identifier = HandleResourceIdentifier(name);

			// The resource must be taken from the table with manual parsing
			ECS_STACK_CAPACITY_STREAM(char, resource_name, 512);
			resource_name.CopyOther(identifier);
			resource_name.AddStream("resource");
			UIDrawerColorFloatInput* data = (UIDrawerColorFloatInput*)system->FindWindowResource(window_index, resource_name);

			data->color_float = color;

			size_t COLOR_INPUT_DRAWER_CONFIGURATION = configuration | UI_CONFIG_COLOR_INPUT_CALLBACK | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW |
				UI_CONFIG_MAKE_SQUARE;

			float before_current_x = current_x;

			// Draw the color input
			COLOR_INPUT_DRAWER_CONFIGURATION |= configuration & UI_CONFIG_COLOR_FLOAT_DEFAULT_VALUE ? UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE : 0;
			ColorInputDrawer(
				COLOR_INPUT_DRAWER_CONFIGURATION,
				config,
				data->color_input,
				position,
				scale,
				&data->base_color
			);

			float horizontal_x = current_x - before_current_x;
			scale.x -= horizontal_x;

			// Draw the intensity
			char intensity_input_name[256];
			ColorFloatInputIntensityInputName(intensity_input_name);

			size_t FLOAT_INPUT_CONFIGURATION = configuration | UI_CONFIG_TEXT_INPUT_CALLBACK;
			FLOAT_INPUT_CONFIGURATION |= configuration & UI_CONFIG_COLOR_FLOAT_DEFAULT_VALUE ? UI_CONFIG_NUMBER_INPUT_DEFAULT : 0;
			FLOAT_INPUT_CONFIGURATION = ClearFlag(FLOAT_INPUT_CONFIGURATION, UI_CONFIG_NAME_PADDING);

			bool pushed_string_pattern = PushIdentifierStackStringPattern();
			PushIdentifierStack(name);

			// Add the callback
			UIConfigTextInputCallback callback;
			callback.handler.action = ColorFloatInputIntensityCallback;
			callback.handler.data = data;
			callback.handler.data_size = 0;
			callback.handler.phase = ECS_UI_DRAW_NORMAL;
			config.AddFlag(callback);
			FloatInputDrawer(
				FLOAT_INPUT_CONFIGURATION, 
				config, 
				intensity_input_name, 
				&data->intensity, 
				0.0f, 
				10000.0f, 
				{ current_x - region_render_offset.x, position.y }, 
				scale
			);

			PopIdentifierStack();
			if (pushed_string_pattern) {
				PopIdentifierStack();
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerColorFloatInput* UIDrawer::ColorFloatInputInitializer(size_t configuration, UIDrawConfig& config, Stream<char> name, ColorFloat* color, ColorFloat default_color, float2 position, float2 scale) {
			UIDrawerColorFloatInput* input = nullptr;

			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}

			Stream<char> identifier = HandleResourceIdentifier(name);

			// Create a temporary name for resource, in order to avoid poluting the color input's
			// name - if there is any
			ECS_STACK_CAPACITY_STREAM(char, color_input_name, 256);
			color_input_name.CopyOther(identifier);
			color_input_name.AddStream("resource");
			color_input_name.AssertCapacity();
			input = GetMainAllocatorBufferAndStoreAsResource<UIDrawerColorFloatInput>(color_input_name);

			input->color_float = color;
			input->base_color = HDRColorToSDR(*color, &input->intensity);

			float default_sdr_intensity;
			Color default_sdr_color = HDRColorToSDR(default_color, &default_sdr_intensity);

			bool has_callback = HasFlag(configuration, UI_CONFIG_COLOR_FLOAT_CALLBACK);
			bool has_color_callback = HasFlag(configuration, UI_CONFIG_COLOR_INPUT_CALLBACK);
			configuration = ClearFlag(configuration, UI_CONFIG_COLOR_FLOAT_CALLBACK);

			// The intensity will be controlled by number input - the reference must be made through the name
			char intensity_input_name[256];
			ColorFloatInputIntensityInputName(intensity_input_name);
			// Add the callback
			UIConfigTextInputCallback callback;
			callback.handler.action = ColorFloatInputIntensityCallback;
			callback.handler.data = input;
			callback.handler.data_size = 0;
			callback.handler.phase = ECS_UI_DRAW_NORMAL;
			config.AddFlag(callback);

			size_t FLOAT_INPUT_CONFIGURATION = configuration | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN | UI_CONFIG_TEXT_INPUT_CALLBACK;
			FLOAT_INPUT_CONFIGURATION |= configuration & UI_CONFIG_COLOR_FLOAT_DEFAULT_VALUE ? UI_CONFIG_NUMBER_INPUT_DEFAULT : 0;

			bool pushed_string_pattern = PushIdentifierStackStringPattern();
			PushIdentifierStack(name);

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

			PopIdentifierStack();
			if (pushed_string_pattern) {
				PopIdentifierStack();
			}

			UIConfigColorInputCallback initial_color_callback;
			
			// The callback must be intercepted
			if (has_color_callback) {
				// Make a coalesced allocation for the callback data
				UIActionHandler current_callback = {};

				if (has_callback) {
					const UIConfigColorFloatCallback* float_callback = (const UIConfigColorFloatCallback*)config.GetParameter(UI_CONFIG_COLOR_FLOAT_CALLBACK);
					current_callback = float_callback->callback;
				}
				UIConfigColorInputCallback* color_callback = (UIConfigColorInputCallback*)config.GetParameter(UI_CONFIG_COLOR_INPUT_CALLBACK);
				initial_color_callback = *color_callback;

				UIDrawerColorFloatInputCallbackData* callback_data = (UIDrawerColorFloatInputCallbackData*)GetMainAllocatorBuffer(
					sizeof(UIDrawerColorFloatInputCallbackData) + current_callback.data_size
				);
				callback_data->input = input;
				if (current_callback.data_size > 0) {
					void* callback_data_user = OffsetPointer(callback_data, sizeof(UIDrawerColorFloatInputCallbackData));
					memcpy(callback_data_user, current_callback.data, current_callback.data_size);
					callback_data->callback_data = callback_data_user;
				}
				else {
					callback_data->callback_data = current_callback.data;
				}

				color_callback->callback.action = ColorFloatInputCallback;
				color_callback->callback.data = callback_data;
				color_callback->callback.data_size = 0;
			}
			else {
				Action float_color_callback = nullptr;
				void* float_color_callback_data = nullptr;

				UIDrawerColorFloatInputCallbackData stack_callback_data;
				UIDrawerColorFloatInputCallbackData* callback_data = &stack_callback_data;

				size_t size_to_allocate = sizeof(stack_callback_data);

				if (has_callback) {
					const UIConfigColorFloatCallback* float_callback = (const UIConfigColorFloatCallback*)config.GetParameter(UI_CONFIG_COLOR_FLOAT_CALLBACK);
					float_color_callback = float_callback->callback.action;
					float_color_callback_data = float_callback->callback.data;
					
					if (float_callback->callback.data_size > 0) {
						size_to_allocate = 0;
						callback_data = (UIDrawerColorFloatInputCallbackData*)GetMainAllocatorBuffer(
							sizeof(UIDrawerColorFloatInputCallbackData) + float_callback->callback.data_size
						);
						float_color_callback_data = OffsetPointer(callback_data, sizeof(*callback_data));
						memcpy(float_color_callback_data, float_callback->callback.data, float_callback->callback.data_size);
					}
				}

				callback_data->callback = float_color_callback;
				callback_data->callback_data = float_color_callback_data;
				callback_data->input = input;
				UIConfigColorInputCallback callback = { ColorFloatInputCallback, callback_data, size_to_allocate };

				// Cringe MSVC bug - it does not set the callback data correctly
				//memset(&callback_data.callback, 0, 16);
				//memcpy(&callback_data.input, &input, sizeof(input));
				config.AddFlag(callback);
			}

			size_t COLOR_INPUT_CONFIGURATION = configuration | UI_CONFIG_COLOR_INPUT_CALLBACK | UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			COLOR_INPUT_CONFIGURATION |= configuration & UI_CONFIG_COLOR_FLOAT_DEFAULT_VALUE ? UI_CONFIG_COLOR_INPUT_DEFAULT_VALUE : 0;
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

			// Restore the previous color callback
			if (has_color_callback) {
				UIConfigColorInputCallback* color_callback = (UIConfigColorInputCallback*)config.GetParameter(UI_CONFIG_COLOR_INPUT_CALLBACK);
				*color_callback = initial_color_callback;
			}

			if (~configuration & UI_CONFIG_COLOR_INPUT_CALLBACK) {
				config.flag_count--;
			}

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

#pragma region Directory Input

		// ------------------------------------------------------------------------------------------------------------------------------------

		struct UIDrawerPathTextCallbackData {
			void CallCallback(ActionData* action_data) {
				if (user_callback != nullptr) {
					if (user_callback_data == nullptr) {
						action_data->data = OffsetPointer(this, sizeof(UIDrawerPathTextCallbackData));
					}
					else {
						action_data->data = user_callback_data;
					}

					// Also convert the previous text such that the callback has access to it
					ECS_STACK_CAPACITY_STREAM(wchar_t, previous_path, 512);
					ConvertASCIIToWide(previous_path, input->previous_text);
					action_data->additional_data = &previous_path;
					user_callback(action_data);
				}
			}

			UIDrawerTextInput* input;
			CapacityStream<wchar_t>* target;
			Action user_callback;
			void* user_callback_data;
			ECS_UI_DRAW_PHASE user_draw_phase;
			Stream<Stream<wchar_t>> extensions;
		};

		void UIDrawerPathTextCallbackAction(ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerPathTextCallbackData* data = (UIDrawerPathTextCallbackData*)_data;
			data->target->size = 0;
			ConvertASCIIToWide(*data->target, *data->input->text);
			action_data->redraw_window = true;

			data->CallCallback(action_data);
		}

		UIDrawerTextInput* UIDrawerPathInputInitializer(
			UIDrawer* drawer,
			size_t configuration,
			UIDrawConfig& config,
			Stream<char> name,
			CapacityStream<wchar_t>* characters,
			Stream<Stream<wchar_t>> extensions
		) {
			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				drawer->BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}

			size_t callback_size = sizeof(UIDrawerPathTextCallbackData);
			bool has_callback = HasFlag(configuration, UI_CONFIG_PATH_INPUT_CALLBACK);
			if (has_callback) {
				const UIConfigPathInputCallback* callback = (const UIConfigPathInputCallback*)config.GetParameter(UI_CONFIG_PATH_INPUT_CALLBACK);
				callback_size += callback->callback.data_size;
			}

			size_t data_size = sizeof(char) * (characters->capacity + 1) + sizeof(CapacityStream<char>) + callback_size;
			void* allocation = drawer->GetMainAllocatorBuffer(data_size);
			CapacityStream<char>* ascii_stream = (CapacityStream<char>*)allocation;
			ascii_stream->InitializeFromBuffer(OffsetPointer(allocation, sizeof(CapacityStream<char>)), 0, characters->capacity);
			if (characters->size > 0) {
				ConvertWideCharsToASCII(*characters, *ascii_stream);
			}

			UIDrawerPathTextCallbackData* callback_data = (UIDrawerPathTextCallbackData*)OffsetPointer(ascii_stream->buffer, (characters->capacity + 1) * sizeof(char));
			callback_data->target = characters;
			bool callback_copy_on_initialization = false;
			bool callback_trigger_on_release = true;
			if (has_callback) {
				const UIConfigPathInputCallback* callback = (const UIConfigPathInputCallback*)config.GetParameter(UI_CONFIG_PATH_INPUT_CALLBACK);
				if (callback->callback.data_size > 0) {
					void* user_callback_data = OffsetPointer(callback_data, sizeof(*callback_data));
					memcpy(user_callback_data, callback->callback.data, callback->callback.data_size);
					// Signal that the data is relative
					callback_data->user_callback_data = nullptr;
				}
				else {
					callback_data->user_callback_data = callback->callback.data;
				}
				callback_data->user_callback = callback->callback.action;
				callback_data->user_draw_phase = callback->callback.phase;
				callback_copy_on_initialization = callback->copy_on_initialization;
				callback_trigger_on_release = callback->trigger_on_release;
			}
			else {
				callback_data->user_callback = nullptr;
				callback_data->user_callback_data = nullptr;
				callback_data->user_draw_phase = ECS_UI_DRAW_NORMAL;
			}

			size_t input_configuration = configuration | UI_CONFIG_TEXT_INPUT_CALLBACK;
			UIConfigTextInputCallback input_callback;
			input_callback.handler = { UIDrawerPathTextCallbackAction, callback_data, 0, callback_data->user_draw_phase };
			input_callback.copy_on_initialization = callback_copy_on_initialization;
			input_callback.trigger_only_on_release = callback_trigger_on_release;
			config.AddFlag(input_callback);
			
			UIDrawerTextInput* input = drawer->TextInputInitializer(input_configuration, config, name, ascii_stream, { 0.0f, 0.0f }, drawer->GetElementDefaultScale());
			callback_data->input = input;
			config.flag_count--;

			if (extensions.size > 0) {
				size_t copy_size = StreamCoalescedDeepCopySize(extensions);
				void* allocation = drawer->GetMainAllocatorBuffer(copy_size);
				uintptr_t ptr = (uintptr_t)allocation;
				callback_data->extensions = StreamCoalescedDeepCopy(extensions, ptr);
			}
			else {
				callback_data->extensions = { nullptr, 0 };
			}

			return input;
		}

		void UIDrawerPathInputDrawer(
			UIDrawer* drawer,
			size_t configuration,
			UIDrawConfig& config,
			Stream<char> name,
			CapacityStream<wchar_t>* path,
			Action click_action,
			float2 position,
			float2 scale,
			Stream<Stream<wchar_t>> extensions
		) {
			float2 folder_icon_size = drawer->GetSquareScale(scale.y);

			scale.x = ClampMin(scale.x, folder_icon_size.x + drawer->layout.element_indentation + 0.005f);
			drawer->HandleFitSpaceRectangle(configuration, position, scale);

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = scale;
			}

			bool is_active = true;
			if (configuration & UI_CONFIG_ACTIVE_STATE) {
				UIConfigActiveState* state = (UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				is_active = state->state;
			}

			UIDrawerTextInput* text_input = (UIDrawerTextInput*)drawer->GetResource(name);

			UIDrawerPathTextCallbackData* callback_data = (UIDrawerPathTextCallbackData*)text_input->callback_data;

			bool trigger_on_release = true;
			if (configuration & UI_CONFIG_PATH_INPUT_CALLBACK) {
				const UIConfigPathInputCallback* callback = (const UIConfigPathInputCallback*)config.GetParameter(UI_CONFIG_PATH_INPUT_CALLBACK);
				if (callback->callback.data_size > 0 && !callback->copy_on_initialization) {
					// The actual user callback is stored after the base callback
					memcpy(OffsetPointer(callback_data, sizeof(*callback_data)), callback->callback.data, callback->callback.data_size);
				}
				trigger_on_release = callback->trigger_on_release;
			}

			UIConfigTextInputCallback input_callback;
			input_callback.handler = { UIDrawerPathTextCallbackAction, callback_data, 0, callback_data->user_draw_phase };

			size_t input_configuration = configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW | UI_CONFIG_TEXT_INPUT_CALLBACK;
			config.AddFlag(input_callback);

			float2 input_scale = { scale.x - folder_icon_size.x - drawer->layout.element_indentation, scale.y };
			drawer->TextInputDrawer(input_configuration, config, text_input, position, input_scale, UIDrawerTextInputFilterAll);

			struct RunnableData {
				UIDrawerTextInput* text_input;
				CapacityStream<wchar_t>* path;
				bool trigger_on_release;
			};

			RunnableData runnable_data = { text_input, path, trigger_on_release };
			drawer->SnapshotRunnable(&runnable_data, sizeof(runnable_data), ECS_UI_DRAW_NORMAL, [](void* _data, ActionData* action_data) {
				RunnableData* data = (RunnableData*)_data;

				// If the path has changed, change the text input as well
				// This must be done after the text input because the callback will be called
				// from inside the text input
				ECS_STACK_CAPACITY_STREAM(char, path_converted, 512);
				ConvertWideCharsToASCII(*data->path, path_converted);
				if (*data->text_input->text != path_converted) {
					if (!data->trigger_on_release || !data->text_input->is_currently_selected) {
						data->text_input->DeleteAllCharacters();
						if (path_converted.size > 0) {
							data->text_input->InsertCharacters(path_converted.buffer, path_converted.size, 0, action_data->system);
						}

						UIDrawerPathTextCallbackData* callback_data = (UIDrawerPathTextCallbackData*)data->text_input->callback;
						// Call the user callback, if any
						if (callback_data->user_callback != nullptr) {
							action_data->data = callback_data;
							callback_data->CallCallback(action_data);
						}

						// Disable the callback
						data->text_input->trigger_callback = UIDrawerTextInput::TRIGGER_CALLBACK_NONE;
						return true;
					}
				}
				return false;
			});		

			config.flag_count--;

			position.x = drawer->GetCurrentPosition().x;
			UIDrawConfig sprite_config;
			UIConfigAbsoluteTransform transform;
			transform.scale = drawer->GetSquareScale(scale.y);
			transform.position = position + drawer->region_render_offset;
			sprite_config.AddFlag(transform);
			
			const wchar_t* sprite_texture = ECS_TOOLS_UI_TEXTURE_FOLDER;
			if (configuration & UI_CONFIG_PATH_INPUT_SPRITE_TEXTURE) {
				const UIConfigPathInputSpriteTexture* texture = (const UIConfigPathInputSpriteTexture*)config.GetParameter(UI_CONFIG_PATH_INPUT_SPRITE_TEXTURE);
				sprite_texture = texture->texture;
			}

			void* temporary_memory = ECS_STACK_ALLOC(ECS_KB * 16);

			UIActionHandler sprite_handler;
			if (configuration & UI_CONFIG_PATH_INPUT_GIVE_FILES) {
				const UIConfigPathInputGiveFiles* give_files = (const UIConfigPathInputGiveFiles*)config.GetParameter(UI_CONFIG_PATH_INPUT_GIVE_FILES);

				UIDrawerPathInputWithInputsActionData* with_input_handler_data = (UIDrawerPathInputWithInputsActionData*)temporary_memory;
				sprite_handler.action = PathInputFolderWithInputsAction;
				with_input_handler_data->is_callback = give_files->is_callback;
				with_input_handler_data->input = text_input;
				with_input_handler_data->path = path;

				// This is the greater sized element of the union
				with_input_handler_data->callback_handler = give_files->callback_handler;

				size_t copy_size = sizeof(*with_input_handler_data);
				if (give_files->is_callback && give_files->callback_handler.data_size > 0) {
					memcpy(OffsetPointer(temporary_memory, copy_size), give_files->callback_handler.data, give_files->callback_handler.data_size);
					copy_size += give_files->callback_handler.data_size;
				}

				sprite_handler.data = with_input_handler_data;
				sprite_handler.data_size = copy_size;
				sprite_handler.phase = ECS_UI_DRAW_SYSTEM;
			}
			else {
				UIDrawerPathInputFolderActionData* handler_data = (UIDrawerPathInputFolderActionData*)temporary_memory;
				handler_data->input = text_input;
				handler_data->extensions = callback_data->extensions;
				handler_data->roots = { nullptr, 0 };
				handler_data->path = path;

				sprite_handler.action = click_action;
				sprite_handler.data = handler_data;
				sprite_handler.phase = ECS_UI_DRAW_SYSTEM;
				size_t copy_size = sizeof(*handler_data);

				if (configuration & UI_CONFIG_PATH_INPUT_CUSTOM_FILESYSTEM) {
					const UIConfigPathInputCustomFilesystem* filesystem = (const UIConfigPathInputCustomFilesystem*)config.GetParameter(UI_CONFIG_PATH_INPUT_CUSTOM_FILESYSTEM);
					handler_data->custom_handler = filesystem->callback;
					if (filesystem->callback.data_size > 0 && !filesystem->copy_on_initialization) {
						handler_data->custom_handler.data = OffsetPointer(temporary_memory, copy_size);
						memcpy(handler_data->custom_handler.data, filesystem->callback.data, filesystem->callback.data_size);
						copy_size += filesystem->callback.data_size;
					}
				}
				else {
					handler_data->custom_handler = { nullptr };
				}
				sprite_handler.data_size = copy_size;
			}

			size_t SPRITE_CONFIGURATION = UI_CONFIG_ABSOLUTE_TRANSFORM;

			if (is_active) {
				drawer->SpriteButton(SPRITE_CONFIGURATION, sprite_config, sprite_handler, sprite_texture);
			}
			else {
				// Reduce the alpha by a bit
				drawer->SpriteRectangle(SPRITE_CONFIGURATION, sprite_config, sprite_texture, Color((unsigned char)255, 255, 255, 120));
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DirectoryInput(Stream<char> name, CapacityStream<wchar_t>* path)
		{
			UIDrawConfig config;
			DirectoryInput(0, config, name, path);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DirectoryInput(size_t configuration, UIDrawConfig& config, Stream<char> name, CapacityStream<wchar_t>* path)
		{
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
					DirectoryInputDrawer(configuration, config, name, path, position, scale);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializePathInput initialize_data;
						initialize_data.config = &config;
						initialize_data.name = name;
						initialize_data.capacity_characters = path;
						InitializeDrawerElement(*this, &initialize_data, name, InitializeDirectoryInputElement, DynamicConfiguration(configuration));
					}
					DirectoryInput(DynamicConfiguration(configuration), config, name, path);
				}
			}
			else {
				if (configuration & UI_CONFIG_DO_CACHE) {
					DirectoryInputInitializer(configuration, config, name, path);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DirectoryInputDrawer(size_t configuration, UIDrawConfig& config, Stream<char> name, CapacityStream<wchar_t>* path, float2 position, float2 scale)
		{
			UIDrawerPathInputDrawer(this, configuration, config, name, path, DirectoryInputFolderAction, position, scale, { nullptr, 0 });
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextInput* UIDrawer::DirectoryInputInitializer(size_t configuration, UIDrawConfig& config, Stream<char> name, CapacityStream<wchar_t>* path)
		{
			return UIDrawerPathInputInitializer(this, configuration, config, name, path, { nullptr, 0 });
		}

#pragma region Double Draggable
		
		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleDraggable(Stream<char> name, double* value, double min, double max, double default_value, unsigned int precision)
		{
			UIDrawConfig config;
			DoubleDraggable(0, config, name, value, min, max, default_value, precision);
		}

		void UIDrawer::DoubleDraggable(size_t configuration, UIDrawConfig& config, Stream<char> name, double* value_to_modify, double lower_bound, double upper_bound, double default_value, unsigned int precision)
		{
			DoubleSlider(configuration | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE, config, name, value_to_modify, lower_bound, upper_bound, default_value, precision);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleDraggableGroup(size_t count, Stream<char> group_name, Stream<char>* names, double** values_to_modify, const double* lower_bounds, const double* upper_bounds, const double* default_values, unsigned int precision)
		{
			UIDrawConfig config;
			DoubleDraggableGroup(0, config, count, group_name, names, values_to_modify, lower_bounds, upper_bounds, default_values, precision);
		}

		void UIDrawer::DoubleDraggableGroup(size_t configuration, UIDrawConfig& config, size_t count, Stream<char> group_name, Stream<char>* names, double** values_to_modify, const double* lower_bounds, const double* upper_bounds, const double* default_values, unsigned int precision)
		{
			DoubleSliderGroup(configuration | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE, config, count, group_name, names, values_to_modify, lower_bounds, upper_bounds, default_values, precision);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma endregion


#pragma region File Input

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FileInput(Stream<char> name, CapacityStream<wchar_t>* path, Stream<Stream<wchar_t>> extensions)
		{
			UIDrawConfig config;
			FileInput(0, config, name, path, extensions);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FileInput(size_t configuration, UIDrawConfig& config, Stream<char> name, CapacityStream<wchar_t>* path, Stream<Stream<wchar_t>> extensions)
		{
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
					FileInputDrawer(configuration, config, name, path, extensions, position, scale);
					HandleDynamicResource(configuration, name);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializePathInput initialize_data;
						initialize_data.config = &config;
						initialize_data.name = name;
						initialize_data.capacity_characters = path;
						initialize_data.extensions = extensions;
						InitializeDrawerElement(*this, &initialize_data, name, InitializeFileInputElement, DynamicConfiguration(configuration));
					}
					FileInput(DynamicConfiguration(configuration), config, name, path, extensions);
				}
			}
			else {
				if (configuration & UI_CONFIG_DO_CACHE) {
					FileInputInitializer(configuration, config, name, path, extensions);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FileInputDrawer(
			size_t configuration,
			UIDrawConfig& config, 
			Stream<char> name, 
			CapacityStream<wchar_t>* path, 
			Stream<Stream<wchar_t>> extensions,
			float2 position,
			float2 scale
		)
		{
			UIDrawerPathInputDrawer(this, configuration, config, name, path, FileInputFolderAction, position, scale, extensions);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerTextInput* UIDrawer::FileInputInitializer(size_t configuration, UIDrawConfig& config, Stream<char> name, CapacityStream<wchar_t>* path, Stream<Stream<wchar_t>> extensions)
		{
			return UIDrawerPathInputInitializer(this, configuration, config, name, path, extensions);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Filter Menu

		// ------------------------------------------------------------------------------------------------------------------------------------

		Stream<Stream<char>> AllocateFilterMenuCopyLabels(UIDrawer* drawer, Stream<Stream<char>> labels) {
			size_t copy_size = StreamCoalescedDeepCopySize(labels);
			void* copy_allocation = drawer->GetMainAllocatorBuffer(copy_size);
			uintptr_t copy_ptr = (uintptr_t)copy_allocation;
			return StreamCoalescedDeepCopy(labels, copy_ptr);
		}

		Stream<Stream<char>> ReallocateFilterMenuCopyLabels(UIDrawer* drawer, Stream<Stream<char>> old_labels, Stream<Stream<char>> new_labels) {
			drawer->RemoveAllocation(old_labels.buffer);
			return AllocateFilterMenuCopyLabels(drawer, new_labels);
		}

		bool ShouldReallocateFilterMenuCopyLabels(Stream<Stream<char>> old_labels, Stream<Stream<char>> new_labels) {
			if (old_labels.size != new_labels.size) {
				return true;
			}
			for (size_t index = 0; index < old_labels.size; index++) {
				if (old_labels[index] != new_labels[index]) {
					return true;
				}
			}

			return false;
		}

		// States should be a stack pointer with bool* to the members that need to be changed
		void UIDrawer::FilterMenu(Stream<char> name, Stream<Stream<char>> label_names, bool** states) {
			UIDrawConfig config;
			FilterMenu(0, config, name, label_names, states);
		}

		// States should be a stack pointer with bool* to the members that need to be changed
		void UIDrawer::FilterMenu(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> label_names, bool** states) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
					UIDrawerFilterMenuData* data = (UIDrawerFilterMenuData*)GetResource(name);

					data->states = states;
					if (~configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
						data->labels = label_names;
					}
					else {
						if (ShouldReallocateFilterMenuCopyLabels(data->labels, label_names)) {
							data->labels = ReallocateFilterMenuCopyLabels(this, data->labels, label_names);
						}
					}

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
				if (configuration & UI_CONFIG_DO_CACHE) {
					FilterMenuInitializer(configuration, config, name, label_names, states);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// States should be a stack pointer to a stable bool array
		void UIDrawer::FilterMenu(Stream<char> name, Stream<Stream<char>> label_names, bool* states) {
			UIDrawConfig config;
			FilterMenu(0, config, name, label_names, states);
		}

		// States should be a stack pointer to a stable bool array
		void UIDrawer::FilterMenu(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> label_names, bool* states) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
					UIDrawerFilterMenuSinglePointerData* data = (UIDrawerFilterMenuSinglePointerData*)GetResource(name);

					data->states = states;
					// Update the labels if they have changed in dimension
					if (~configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
						data->labels = label_names;
					}
					else {
						if (ShouldReallocateFilterMenuCopyLabels(data->labels, label_names)) {
							data->labels = ReallocateFilterMenuCopyLabels(this, data->labels, label_names);
						}
					}

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
				if (configuration & UI_CONFIG_DO_CACHE) {
					FilterMenuInitializer(configuration, config, name, label_names, states);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FilterMenuDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, UIDrawerFilterMenuData* data) {
			UIWindowDescriptor window_descriptor;
			window_descriptor.initial_size_x = 10000.0f;
			window_descriptor.initial_size_y = 10000.0f;
			window_descriptor.draw = FilterMenuDraw;

			window_descriptor.window_name = data->window_name;

			window_descriptor.window_data = data;
			window_descriptor.window_data_size = 0;

			MenuButton(configuration, config, name, window_descriptor, UI_DOCKSPACE_FIXED | UI_DOCKSPACE_BORDER_NOTHING | UI_POP_UP_WINDOW_FIT_TO_CONTENT);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerFilterMenuData* UIDrawer::FilterMenuInitializer(
			size_t configuration,
			const UIDrawConfig& config,
			Stream<char> name, 
			Stream<Stream<char>> label_names,
			bool** states
		) {
			UIDrawerFilterMenuData* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {
				size_t total_memory = sizeof(UIDrawerFilterMenuData);
				total_memory += sizeof(bool*) * label_names.size;
				size_t identifier_size = identifier.size;
				size_t window_name_size = identifier_size + strlen("Filter Window");
				total_memory += sizeof(char) * window_name_size;

				data = (UIDrawerFilterMenuData*)GetMainAllocatorBuffer(total_memory);
				data->name = identifier;
				uintptr_t ptr = (uintptr_t)data;
				ptr += sizeof(UIDrawerFilterMenuData);

				char* window_name = (char*)ptr;
				memcpy(window_name, identifier.buffer, identifier_size);
				strcat(window_name, "Filter Window");
				data->window_name = window_name;
				ptr += window_name_size;

				if (~configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
					data->labels = label_names;
				}
				else {
					data->labels = AllocateFilterMenuCopyLabels(this, label_names);
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

		void UIDrawer::FilterMenuDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, UIDrawerFilterMenuSinglePointerData* data) {
			UIWindowDescriptor window_descriptor;
			window_descriptor.initial_size_x = 10000.0f;
			window_descriptor.initial_size_y = 10000.0f;
			window_descriptor.draw = FilterMenuSinglePointerDraw;

			window_descriptor.window_name = data->window_name;

			window_descriptor.window_data = data;
			window_descriptor.window_data_size = 0;

			MenuButton(configuration, config, name, window_descriptor, UI_DOCKSPACE_FIXED | UI_DOCKSPACE_BORDER_NOTHING | UI_POP_UP_WINDOW_FIT_TO_CONTENT);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerFilterMenuSinglePointerData* UIDrawer::FilterMenuInitializer(
			size_t configuration,
			const UIDrawConfig& config,
			Stream<char> name,
			Stream<Stream<char>> label_names,
			bool* states
		) {
			UIDrawerFilterMenuSinglePointerData* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {
				Stream<char> filter_string = "Filter Window";

				size_t total_memory = sizeof(UIDrawerFilterMenuSinglePointerData);
				size_t identifier_size = identifier.size;
				size_t window_name_size = identifier_size + filter_string.size;
				total_memory += sizeof(char) * window_name_size;

				data = (UIDrawerFilterMenuSinglePointerData*)GetMainAllocatorBuffer(total_memory);
				data->name = identifier;
				uintptr_t ptr = (uintptr_t)data;
				ptr += sizeof(UIDrawerFilterMenuSinglePointerData);

				data->states = states;
				data->window_name = { (char*)ptr, window_name_size };
				identifier.CopyTo(ptr);
				filter_string.CopyTo(ptr);

				if (~configuration & UI_CONFIG_FILTER_MENU_COPY_LABEL_NAMES) {
					data->labels = label_names;
				}
				else {
					data->labels = AllocateFilterMenuCopyLabels(this, label_names);
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
		) const {
			HandleText(configuration, config, color, font_size, character_spacing);

			float old_scale = font_size.y;
			font_size.y = system->GetTextSpriteSizeToScale(scale.y - padding * 2);
			font_size = system->GetTextSpriteSize(font_size.y);
			float factor = font_size.y / old_scale;
			character_spacing *= factor;

			UIConfigTextParameters parameters;
			parameters.character_spacing = character_spacing;
			parameters.color = color;
			parameters.size = font_size;
			SetConfigParameter(configuration, config, parameters, previous_parameters);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FinishFitTextToScale(size_t configuration, UIDrawConfig& config, const UIConfigTextParameters& previous_parameters) const {
			RemoveConfigParameter(configuration, config, previous_parameters);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float3 UIDrawer::FitTextToScale(float scale, float padding) const
		{
			float3 font_size_spacing;

			font_size_spacing.y = system->GetTextSpriteSizeToScale(scale - padding * 2.0f);
			font_size_spacing.x = system->GetTextSpriteSize(font_size_spacing.y).x;
			font_size_spacing.z = font.character_spacing * font_size_spacing.y / font.size;

			return font_size_spacing;
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
						current_colors[0] = PlanarLerp(top_left, top_right, bottom_left, bottom_right, 0.0f, row * inverse_vertical_count_float);
						current_colors[1] = PlanarLerp(top_left, top_right, bottom_left, bottom_right, inverse_horizontal_count_float, row * inverse_vertical_count_float);
						current_colors[2] = PlanarLerp(top_left, top_right, bottom_left, bottom_right, 0.0f, (row + 1) * inverse_vertical_count_float);
						current_colors[3] = PlanarLerp(top_left, top_right, bottom_left, bottom_right, inverse_horizontal_count_float, (row + 1) * inverse_vertical_count_float);

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
							current_colors[1] = PlanarLerp(top_left, top_right, bottom_left, bottom_right, (column + 1) * inverse_horizontal_count_float, row * inverse_vertical_count_float);
							current_colors[3] = PlanarLerp(top_left, top_right, bottom_left, bottom_right, (column + 1) * inverse_horizontal_count_float, (row + 1) * inverse_vertical_count_float);

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

		void UIDrawer::Graph(Stream<float2> samples, Stream<char> name, unsigned int x_axis_precision, unsigned int y_axis_precision) {
			UIDrawConfig config;
			Graph(0, config, samples, name, x_axis_precision, y_axis_precision);
		}

		void UIDrawer::Graph(
			size_t configuration,
			const UIDrawConfig& config,
			Stream<float2> unfiltered_samples,
			float filter_delta,
			Stream<char> name,
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

		void UIDrawer::Graph(size_t configuration, const UIDrawConfig& config, Stream<float2> samples, Stream<char> name, unsigned int x_axis_precision, unsigned int y_axis_precision) {
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
			Stream<char> name, 
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

		UIDrawerRowLayout UIDrawer::GenerateRowLayout()
		{
			UIDrawerRowLayout row_layout;
			
			row_layout.drawer = this;
			row_layout.current_index = 0;
			row_layout.element_count = 0;
			row_layout.indentation = layout.element_indentation;
			row_layout.row_scale = { GetXScaleUntilBorder(current_x), layout.default_element_y };
			row_layout.horizontal_alignment = ECS_UI_ALIGN_LEFT;
			row_layout.vertical_alignment = ECS_UI_ALIGN_TOP;
			row_layout.offset_render_region = { false, false };
			row_layout.SetBorderThickness(GetDefaultBorderThickness());
			row_layout.font_scaling = 1.0f;
			memset(row_layout.indentations, 0, sizeof(row_layout.indentations));
			
			return row_layout;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GetDefaultBorderThickness() const {
			return system->m_descriptors.dockspaces.border_size;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetDefaultBorderThickness2() const
		{
			return GetSquareScale(GetDefaultBorderThickness());
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetElementDefaultScale() const {
			return { layout.default_element_x, layout.default_element_y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetRelativeElementSize(float2 factors) const
		{
			return GetElementDefaultScale() * factors;
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

		float2 UIDrawer::GetWindowSizeFactors(ECS_UI_WINDOW_DEPENDENT_SIZE type, float2 scale) const {
			float2 factors;
			switch (type) {
			case ECS_UI_WINDOW_DEPENDENT_HORIZONTAL:
				factors = {
					scale.x / (region_limit.x - region_fit_space_horizontal_offset),
					scale.y / layout.default_element_y
				};
				break;
			case ECS_UI_WINDOW_DEPENDENT_VERTICAL:
				factors = {
					scale.x / layout.default_element_x,
					scale.y / (region_limit.y - region_fit_space_vertical_offset)
				};
				break;
			case ECS_UI_WINDOW_DEPENDENT_BOTH:
				factors = {
					scale.x / (region_limit.x - region_fit_space_horizontal_offset),
					scale.y / (region_limit.y - region_fit_space_vertical_offset)
				};
				break;
			}

			return factors;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetWindowSizeScaleElement(ECS_UI_WINDOW_DEPENDENT_SIZE type, float2 scale_factors) const {
			float2 scale;
			switch (type) {
			case ECS_UI_WINDOW_DEPENDENT_HORIZONTAL:
				scale = {
					scale_factors.x * (region_limit.x - region_fit_space_horizontal_offset),
					layout.default_element_y * scale_factors.y
				};
				scale.x = scale.x == 0.0f ? region_limit.x - current_x : scale.x;
				break;
			case ECS_UI_WINDOW_DEPENDENT_VERTICAL:
				scale = {
					scale_factors.x * layout.default_element_x,
					scale_factors.y * (region_limit.y - region_fit_space_vertical_offset)
				};
				scale.y = scale.y == 0.0f ? region_limit.y - current_y : scale.y;
				break;
			case ECS_UI_WINDOW_DEPENDENT_BOTH:
				scale = { scale_factors.x * (region_limit.x - region_fit_space_horizontal_offset), scale_factors.y * (region_limit.y - region_fit_space_vertical_offset) };
				scale.x = scale.x == 0.0f ? region_limit.x - current_x : scale.x;
				scale.y = scale.y == 0.0f ? region_limit.y - current_y : scale.y;
				break;
			}
			return scale;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetWindowSizeScaleUntilBorder(bool until_border) const {
			return { GetWindowScaleUntilBorder(until_border) / (region_limit.x - region_fit_space_horizontal_offset), 1.0f };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GetWindowScaleUntilBorder(bool until_border) const
		{
			return until_border ? region_position.x + region_scale.x - current_x : region_limit.x - current_x;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetRelativeTransformFactors(float2 desired_scale) const
		{
			return desired_scale / float2(layout.default_element_x, layout.default_element_y);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetRelativeTransformFactorsZoomed(float2 desired_scale) const
		{
			return (desired_scale * *zoom_ptr) / float2(layout.default_element_x, layout.default_element_y);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIConfigAbsoluteTransform UIDrawer::GetWholeRegionTransform(bool consider_region_header) const
		{
			float2 position = GetRegionPosition();
			float2 scale = GetRegionScale();
			float2 region_offset = GetRegionRenderOffset();

			if (consider_region_header) {
				UIDockspaceBorder* border = &dockspace->borders[border_index];
				if (border->draw_region_header) {
					position.y += system->m_descriptors.misc.title_y_scale;
					scale.y -= system->m_descriptors.misc.title_y_scale;
				}
			}

			return { position + region_offset, scale };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void* UIDrawer::GetTempBuffer(size_t size, ECS_UI_DRAW_PHASE phase, size_t alignment) {
			return system->TemporaryAllocator(phase)->Allocate(size, alignment);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void* UIDrawer::GetHandlerBuffer(size_t size, ECS_UI_DRAW_PHASE phase)
		{
			return system->AllocateFromHandlerAllocator(phase, size);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// Can be used to release some temp memory - cannot be used when handlers are used
		void UIDrawer::ReturnTempAllocator(size_t marker, ECS_UI_DRAW_PHASE phase) {
			system->TemporaryAllocator(phase)->ReturnToMarker(marker);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		size_t UIDrawer::GetTempAllocatorMarker(ECS_UI_DRAW_PHASE phase) {
			return system->TemporaryAllocator(phase)->m_top;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UISystem* UIDrawer::GetSystem() {
			return system;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetSquareScale(float value) const {
			if (value == FLT_MAX) {
				value = layout.default_element_y;
			}
			return { system->NormalizeHorizontalToWindowDimensions(value), value };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetSquareScaleScaled(float value) const {
			if (value == FLT_MAX) {
				value = layout.default_element_y;
			}
			return { system->NormalizeHorizontalToWindowDimensions(value) * zoom_ptr->x, value * zoom_ptr->y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// It will use HandleResourceIdentifier to construct the string
		void* UIDrawer::GetResource(Stream<char> string) {
			Stream<char> string_identifier = HandleResourceIdentifier(string);
			return system->FindWindowResource(window_index, string_identifier);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// It will forward directly to the UI system; No HandleResourceIdentifier used
		void* UIDrawer::FindWindowResource(Stream<char> string) {
			return system->FindWindowResource(window_index, string);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// returns the viewport visible zone
		float2 UIDrawer::GetRenderZone() const {
			float horizontal_region_difference;
			horizontal_region_difference = region_scale.x - 2 * layout.next_row_padding - system->m_descriptors.misc.render_slider_vertical_size + 0.001f;
			horizontal_region_difference += no_padding_for_render_sliders ? system->m_descriptors.misc.render_slider_vertical_size : 0.0f;
			horizontal_region_difference += no_padding_render_region ? 2 * layout.next_row_padding : 0.0f;

			float vertical_region_difference;
			vertical_region_difference = region_scale.y - 2 * layout.next_row_y_offset - system->m_descriptors.misc.render_slider_horizontal_size + 0.001f;
			vertical_region_difference += no_padding_for_render_sliders ? system->m_descriptors.misc.render_slider_horizontal_size : 0.0f;
			vertical_region_difference += no_padding_render_region ? 2 * layout.next_row_y_offset : 0.0f;

			if (dockspace->borders[border_index].draw_elements) {
				vertical_region_difference -= system->m_descriptors.misc.title_y_scale;
			}

			return { horizontal_region_difference, vertical_region_difference };
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
				ForEachMouseButton([&](ECS_MOUSE_BUTTON button_type) {
					state.clickable_count[button_type] = dockspace->borders[border_index].clickable_handler[button_type].position_x.size;
				});
				state.general_count = dockspace->borders[border_index].general_handler.position_x.size;
			}
			else {
				memset(&state, 0, sizeof(state));
			}

			return state;
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
			float position = export_scale != nullptr ? current_x : region_position.x;
			float _region_scale = export_scale != nullptr ? x_scale : region_scale.x;
			return AlignMiddle(position, _region_scale, x_scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float UIDrawer::GetAlignedToCenterY(float y_scale) const {
			float position = export_scale != nullptr ? current_y : region_position.y;
			float _region_scale = export_scale != nullptr ? y_scale : region_scale.y;
			return AlignMiddle(position, _region_scale, y_scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetAlignedToCenter(float2 scale) const {
			return { GetAlignedToCenterX(scale.x), GetAlignedToCenterY(scale.y) };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetAlignedToRight(float x_scale, float target_position) const {
			const float EPSILON = 0.0005f;

			target_position = target_position == FLT_MAX ? region_limit.x : target_position;
			target_position = export_scale != nullptr ? current_x + x_scale : target_position;

			// Move the position by a small offset so as to not have floating point calculation errors that would result
			// In an increased render span even tho it supposed to not contribute to it
			target_position -= EPSILON;
			return { ClampMin(target_position - x_scale, current_x), current_y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetAlignedToRightOverLimit(float x_scale) const {
			float vertical_slider_offset = system->m_windows[window_index].is_vertical_render_slider * system->m_descriptors.misc.render_slider_vertical_size;
			return { ClampMin(region_position.x + region_scale.x - x_scale - vertical_slider_offset, current_x), current_y };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetAlignedToBottom(float y_scale, float target_position) const {
			const float EPSILON = 0.0003f;

			target_position = target_position == FLT_MAX ? region_limit.y : target_position;
			target_position = export_scale != nullptr ? current_y + y_scale : target_position;
			
			// Move the position by a small offset so as to not have floating point calculation errors that would result
			// In an increased render span even tho it supposed to not contribute to it
			target_position -= EPSILON;
			return { current_x, ClampMin(target_position - y_scale, current_y) };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetAlignedToBottomOverLimit(float y_scale) const
		{
			float horizontal_slider_offset = system->m_windows[window_index].is_horizontal_render_slider * system->m_descriptors.misc.render_slider_horizontal_size;
			return { current_x, ClampMin(region_position.y + region_scale.y - y_scale - horizontal_slider_offset, current_y) };
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		float2 UIDrawer::GetCornerRectangle(ECS_UI_ALIGN horizontal_alignment, ECS_UI_ALIGN vertical_alignment, float2 dimensions, float2 padding) const 
		{
			float2 position = { 0.0f, 0.0f };
			if (horizontal_alignment == ECS_UI_ALIGN_LEFT) {
				position.x = region_position.x + padding.x;
			}
			else {
				position.x = region_position.x + region_scale.x - padding.x - dimensions.x;
			}

			if (vertical_alignment == ECS_UI_ALIGN_TOP) {
				position.y = region_position.y + padding.y;
			}
			else {
				position.y = region_position.y + region_scale.y - padding.y - dimensions.y;
			}

			return position;
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
			return cached_filled_action_data;
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

		void* UIDrawer::GetMainAllocatorBufferAndStoreAsResource(Stream<char> name, size_t size, size_t alignment) {
			void* resource = GetMainAllocatorBuffer(size + name.size * sizeof(char), alignment);
			void* name_ptr = OffsetPointer(resource, size);
			name.CopyTo(name_ptr);
			ResourceIdentifier identifier(name_ptr, name.size);
			AddWindowResourceToTable(resource, identifier);
			return resource;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		unsigned int UIDrawer::GetWindowIndex() const {
			return window_index;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Histogram

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Histogram(Stream<float> samples, Stream<char> name) {
			UIDrawConfig config;
			Histogram(0, config, samples, name);
		}

		void UIDrawer::Histogram(size_t configuration, const UIDrawConfig& config, Stream<float> samples, Stream<char> name) {
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
			Stream<char> name,
			UIDrawerTextElement* element,
			float2 position,
			float2 scale
		) {
			if (~configuration & no_element_name) {
				ConvertTextToWindowResource(configuration, config, name, element, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		bool UIDrawer::IsActiveWindow() const
		{
			return window_index == system->GetActiveWindow();
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Indent(float count) {
			min_render_bounds.x = ClampMax(min_render_bounds.x, current_x - region_render_offset.x);
			current_x += count * layout.element_indentation + current_column_x_scale;
			current_column_x_scale = 0.0f;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::IndentWindowSize(float percentage)
		{
			min_render_bounds.x = ClampMax(min_render_bounds.x, current_x - region_render_offset.x);
			current_x += (region_limit.x - region_fit_space_horizontal_offset) * percentage + current_column_x_scale;
			current_column_x_scale = 0.0f;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Label hierarchy

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FilesystemHierarchy(Stream<char> identifier, Stream<Stream<char>> labels) {
			UIDrawConfig config;
			FilesystemHierarchy(0, config, identifier, labels);
		}

		// Parent index 0 means root
		UIDrawerFilesystemHierarchy* UIDrawer::FilesystemHierarchy(size_t configuration, UIDrawConfig& config, Stream<char> identifier, Stream<Stream<char>> labels) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
					UIDrawerFilesystemHierarchy* data = (UIDrawerFilesystemHierarchy*)GetResource(identifier);

					FilesystemHierarchyDrawer(configuration, config, data, labels, position, scale);
					HandleDynamicResource(configuration, identifier);
					return data;
				}
				else {
					bool exists = ExistsResource(identifier);
					if (!exists) {
						UIDrawerInitializeFilesystemHierarchy initialize_data;
						initialize_data.config = &config;
						ECS_FORWARD_STRUCT_MEMBERS_2(initialize_data, identifier, labels);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							identifier,
							InitializeFilesystemHierarchyElement,
							DynamicConfiguration(configuration)
						);
					}
					return FilesystemHierarchy(DynamicConfiguration(configuration), config, identifier, labels);
				}
			}
			else {
				if (configuration & UI_CONFIG_DO_CACHE) {
					return FilesystemHierarchyInitializer(configuration, config, identifier);
				}
				return nullptr;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerFilesystemHierarchy* UIDrawer::FilesystemHierarchyInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> text) {
			UIDrawerFilesystemHierarchy* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(text, [&](Stream<char> identifier) {
				data = GetMainAllocatorBuffer<UIDrawerFilesystemHierarchy>();

				size_t max_characters = 256;

				unsigned int hash_table_capacity = 256;
				if (configuration & UI_CONFIG_FILESYSTEM_HIERARCHY_HASH_TABLE_CAPACITY) {
					const UIConfigFilesystemHierarchyHashTableCapacity* capacity = (const UIConfigFilesystemHierarchyHashTableCapacity*)config.GetParameter(UI_CONFIG_FILESYSTEM_HIERARCHY_HASH_TABLE_CAPACITY);
					hash_table_capacity = capacity->capacity;
				}

				size_t total_size = data->active_label.MemoryOf(max_characters) + data->label_states.MemoryOf(hash_table_capacity);
				void* allocation = GetMainAllocatorBuffer(total_size);
				uintptr_t allocation_ptr = (uintptr_t)allocation;
				data->label_states.InitializeFromBuffer(allocation_ptr, hash_table_capacity);
				data->active_label.InitializeFromBuffer(allocation_ptr, 0, max_characters);

				if (configuration & UI_CONFIG_FILESYSTEM_HIERARCHY_SELECTABLE_CALLBACK) {
					const UIConfigFilesystemHierarchySelectableCallback* callback = (const UIConfigFilesystemHierarchySelectableCallback*)config.GetParameter(UI_CONFIG_FILESYSTEM_HIERARCHY_SELECTABLE_CALLBACK);

					void* callback_data = callback->data;
					if (callback->data_size > 0) {
						void* allocation = GetMainAllocatorBuffer(callback->data_size);
						memcpy(allocation, callback->data, callback->data_size);
						callback_data = allocation;
					}
					data->selectable_callback = callback->callback;
					data->selectable_callback_data = callback_data;
				}
				else {
					data->selectable_callback = nullptr;
					data->selectable_callback_data = nullptr;
				}

				if (configuration & UI_CONFIG_FILESYSTEM_HIERARCHY_RIGHT_CLICK) {
					const UIConfigFilesystemHierarchyRightClick* callback = (const UIConfigFilesystemHierarchyRightClick*)config.GetParameter(UI_CONFIG_FILESYSTEM_HIERARCHY_RIGHT_CLICK);

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

		void UIDrawer::FilesystemHierarchyDrawer(
			size_t configuration,
			UIDrawConfig& config,
			UIDrawerFilesystemHierarchy* data,
			Stream<Stream<char>> labels,
			float2 position,
			float2 scale
		) {
			struct StackElement {
				unsigned int parent_index;
				float next_row_gain;
			};

			// keep a stack of current parent indices
			StackElement* stack_buffer = (StackElement*)GetTempBuffer(sizeof(StackElement) * labels.size, ECS_UI_DRAW_NORMAL, alignof(StackElement));
			Stack<StackElement> stack(stack_buffer, labels.size);
			bool* label_states = (bool*)GetTempBuffer(sizeof(bool) * labels.size, ECS_UI_DRAW_NORMAL, alignof(bool));

			float2 current_position = position;
			float2 square_scale = GetSquareScale(scale.y);

			// aliases for sprite texture info
			float2 opened_top_left_uv, closed_top_left_uv, opened_bottom_right_uv, closed_bottom_right_uv;
			Color opened_color, closed_color;
			const wchar_t* opened_texture;
			const wchar_t* closed_texture;
			float2 expand_factor;
			float horizontal_bound = position.x + scale.x;
			if (~configuration & UI_CONFIG_FILESYSTEM_HIERARCHY_DO_NOT_INFER_X) {
				horizontal_bound = region_limit.x;
			}

			float horizontal_texture_offset = square_scale.x;
			bool keep_triangle = true;

			// copy the information into aliases
			if (configuration & UI_CONFIG_FILESYSTEM_HIERARCHY_SPRITE_TEXTURE) {
				const UIConfigFilesystemHierarchySpriteTexture* texture_info = (const UIConfigFilesystemHierarchySpriteTexture*)config.GetParameter(UI_CONFIG_FILESYSTEM_HIERARCHY_SPRITE_TEXTURE);
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

			ECS_UI_DRAW_PHASE selectable_callback_phase = ECS_UI_DRAW_NORMAL;
			// check selectable update of data
			if (configuration & UI_CONFIG_FILESYSTEM_HIERARCHY_SELECTABLE_CALLBACK) {
				const UIConfigFilesystemHierarchySelectableCallback* selectable = (const UIConfigFilesystemHierarchySelectableCallback*)config.GetParameter(UI_CONFIG_FILESYSTEM_HIERARCHY_SELECTABLE_CALLBACK);
				if (!selectable->copy_on_initialization && selectable->data_size > 0) {
					memcpy(data->selectable_callback_data, selectable->data, selectable->data_size);
				}
				selectable_callback_phase = selectable->phase;
			}

			ECS_UI_DRAW_PHASE right_click_phase = ECS_UI_DRAW_NORMAL;
			// check right click update of data
			if (configuration & UI_CONFIG_FILESYSTEM_HIERARCHY_RIGHT_CLICK) {
				const UIConfigFilesystemHierarchyRightClick* right_click = (const UIConfigFilesystemHierarchyRightClick*)config.GetParameter(UI_CONFIG_FILESYSTEM_HIERARCHY_RIGHT_CLICK);
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
			text_alignment.horizontal = ECS_UI_ALIGN_LEFT;
			text_alignment.vertical = ECS_UI_ALIGN_MIDDLE;
			config.AddFlag(text_alignment);

			HashTableDefault<unsigned int> parent_hash_table;
			size_t table_count = PowerOfTwoGreater(labels.size) * 2;
			parent_hash_table.InitializeFromBuffer(GetTempBuffer(parent_hash_table.MemoryOf(table_count)), table_count);

			size_t label_configuration = UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE |
				UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_DO_NOT_ADVANCE;
			for (size_t index = 0; index < labels.size; index++) {
				unsigned int current_parent_index = 0;
				Stream<char> label_stream = labels[index];
				size_t parent_path_size = PathParentSize(label_stream);

				if (parent_path_size != 0) {
					ResourceIdentifier identifier(label_stream.buffer, parent_path_size);
					parent_hash_table.TryGetValue(identifier, current_parent_index);
				}

				// check to see if it is inside the hash table; if it is, then 
				// increase the activation count else introduce it
				ResourceIdentifier identifier(label_stream);
				ECS_ASSERT(!parent_hash_table.Insert(index + 1, identifier));
				
				unsigned int table_index = data->label_states.Find(identifier);

				// get the label state for triangle drop down
				bool label_state = false;
				// get a char pointer to a stable reference label character stream; the newly created
				// stream for insertion or the already existing identifier for existing label
				Stream<char> table_char_stream;

				const auto* table_pairs = data->label_states.GetPairs();

				if (table_index == -1) {
					void* allocation = GetMainAllocatorBuffer((label_stream.size + 1) * sizeof(char), alignof(char));
					memcpy(allocation, label_stream.buffer, (label_stream.size + 1) * sizeof(char));
					identifier.ptr = (const wchar_t*)allocation;
					table_char_stream.buffer = (char*)allocation;
					table_char_stream.size = label_stream.size;

					UIDrawerFilesystemHierarchyLabelData current_data;
					current_data.activation_count = 5;
					current_data.state = false;
					label_states[index] = false;
					ECS_ASSERT(!data->label_states.Insert(current_data, identifier));
				}
				else {
					UIDrawerFilesystemHierarchyLabelData* current_data = data->label_states.GetValuePtrFromIndex(table_index);
					current_data->activation_count++;
					label_state = current_data->state;

					ResourceIdentifier identifier = data->label_states.GetIdentifierFromIndex(table_index);
					table_char_stream.buffer = (char*)identifier.ptr;
					table_char_stream.size = identifier.size;
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
						Stream<char> next_label_stream = labels[index + 1];
						size_t next_parent_path_size = PathParentSize(next_label_stream);

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
						Stream<char> current_label;
						current_label.buffer = label_stream.buffer + parent_path_size + (parent_path_size != 0);
						current_label.size = PointerDifference(label_stream.buffer + label_stream.size, current_label.buffer);

						if (label_stream.size == data->active_label.size) {
							if (memcmp(label_stream.buffer, data->active_label.buffer, label_stream.size) == 0) {
								current_color = ToneColor(label_color, 1.25f);
								is_active = true;
								active_label_scale = GetLabelScale(current_label).x;
							}
						}
						TextLabel(
							ClearFlag(configuration, UI_CONFIG_DO_CACHE) | label_configuration | UI_CONFIG_LABEL_TRANSPARENT,
							config,
							current_label,
							label_position,
							scale
						);

						float2 action_scale = { horizontal_bound - initial_label_position.x, scale.y };

						auto copy_clickable_function = [](void* _data, AllocatorPolymorphic allocator) {
							UIDrawerFilesystemHierarchySelectableData* data = (UIDrawerFilesystemHierarchySelectableData*)_data;
							data->label = data->label.Copy(allocator);
						};

						UIDrawerFilesystemHierarchySelectableData clickable_data = { data };
						clickable_data.label = label_stream.Copy(TemporaryAllocator(this, selectable_callback_phase));
						AddClickable(
							configuration, 
							initial_label_position, 
							action_scale, 
							{ 
								FilesystemHierarchySelectable, 
								&clickable_data, 
								sizeof(clickable_data), 
								selectable_callback_phase 
							}, 
							ECS_MOUSE_LEFT, 
							copy_clickable_function
						);
						//AddDefaultHoverable(configuration, initial_label_position, action_scale, current_color);

						UIDrawerFilesystemHierarchyChangeStateData change_state_data;
						change_state_data.hierarchy = data;
						change_state_data.label = clickable_data.label;
						if (keep_triangle && has_children) {
							if (label_state) {
								SpriteRectangle(configuration, current_position, square_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, text_color);
							}
							else {
								SpriteRectangle(configuration, current_position, square_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, text_color, { 1.0f, 0.0f }, { 0.0f, 1.0f });
							}
							AddClickable(configuration, current_position, square_scale, { FilesystemHierarchyChangeState, &change_state_data, sizeof(change_state_data), selectable_callback_phase });
						}
						current_position.x += square_scale.x;
						if (configuration & UI_CONFIG_FILESYSTEM_HIERARCHY_SPRITE_TEXTURE) {
							if (label_state) {
								SpriteRectangle(configuration, current_position, square_scale, opened_texture, opened_color, opened_top_left_uv, opened_bottom_right_uv);
							}
							else {
								SpriteRectangle(configuration, current_position, square_scale, closed_texture, closed_color, closed_top_left_uv, closed_bottom_right_uv);
							}
							if (!keep_triangle && has_children) {
								AddClickable(configuration, current_position, square_scale, { FilesystemHierarchyChangeState, &change_state_data, sizeof(change_state_data), selectable_callback_phase });
							}
						}

						if (configuration & UI_CONFIG_FILESYSTEM_HIERARCHY_RIGHT_CLICK) {
							if (record_actions) {
								struct RightClickWrapperData {
									UIDrawerFilesystemHierarchy* data;
									Stream<char> label;
								};

								auto right_click_wrapper = [](ActionData* action_data) {
									UI_UNPACK_ACTION_DATA;

									RightClickWrapperData* data = (RightClickWrapperData*)_data;

									if (IsPointInRectangle(mouse_position, position, scale) && mouse->IsReleased(ECS_MOUSE_RIGHT)) {
										data->data->active_label.CopyOther(data->label);
										ECS_ASSERT(data->data->active_label.size < data->data->active_label.capacity);
										data->data->active_label[data->data->active_label.size] = '\0';
										action_data->redraw_window = true;

										// Call the right click callback first, then the selectable one
										UIDrawerFilesystemHierarchyUserRightClickData right_click_data;
										right_click_data.data = data->data->right_click_callback_data;
										right_click_data.label = data->label;
										action_data->data = &right_click_data;
										data->data->right_click_callback(action_data);

										if (data->data->selectable_callback != nullptr) {
											action_data->additional_data = data->data->active_label.buffer;
											action_data->data = data->data->selectable_callback_data;
											data->data->selectable_callback(action_data);
										}
									}
								};

								auto right_click_copy_function = [](void* _data, AllocatorPolymorphic allocator) {
									RightClickWrapperData* data = (RightClickWrapperData*)_data;
									data->label = data->label.Copy(allocator);
								};

								RightClickWrapperData right_click;
								right_click.data = data;
								right_click.label = label_stream.Copy(TemporaryAllocator(this, right_click_phase));
								AddClickable(
									configuration,
									initial_label_position,
									action_scale,
									{ right_click_wrapper, &right_click, sizeof(right_click), right_click_phase },
									ECS_MOUSE_RIGHT,
									right_click_copy_function
								);
							}
						}

						if (is_active) {
							float horizontal_scale = horizontal_bound - initial_label_position.x;
							horizontal_scale = ClampMin(horizontal_scale, active_label_scale + square_scale.x * 2.0f);
							SolidColorRectangle(configuration, initial_label_position, { horizontal_scale, scale.y }, current_color);
						}

					}

					FinalizeRectangle(configuration, initial_label_position, { horizontal_bound - initial_label_position.x, scale.y });
					NextRow();
				}

			}

			data->label_states.ForEachIndex([&](unsigned int index) {
				auto* value_ptr = data->label_states.GetValuePtrFromIndex(index);

				value_ptr->activation_count--;
				if (value_ptr->activation_count == 0) {
					ResourceIdentifier identifier = data->label_states.GetIdentifierFromIndex(index);

					system->RemoveWindowMemoryResource(window_index, identifier.ptr);
					Deallocate(system->Allocator(), identifier.ptr);
					data->label_states.EraseFromIndex(index);
					return true;
				}

				return false;
			});

			config.flag_count--;
			OffsetNextRow(-next_row_offset);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Label Hierarchy

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::LabelHierarchy(Stream<char> name, IteratorPolymorphic* iterator, bool keep_storage)
		{
			UIDrawConfig config;
			LabelHierarchy(0, config, name, iterator, keep_storage);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::LabelHierarchy(size_t configuration, const UIDrawConfig& config, Stream<char> name, IteratorPolymorphic* iterator, bool keep_storage)
		{
			float2 position;
			float2 scale;
			HandleTransformFlags(configuration, config, position, scale);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
					UIDrawerLabelHierarchyData* data = (UIDrawerLabelHierarchyData*)GetResource(name);

					unsigned int dynamic_resource_index = -1;
					if (configuration & UI_CONFIG_DYNAMIC_RESOURCE) {
						Stream<char> identifier = HandleResourceIdentifier(name);
						dynamic_resource_index = system->GetWindowDynamicElement(window_index, identifier);
						ECS_ASSERT(dynamic_resource_index != -1);
						system->IncrementWindowDynamicResource(window_index, identifier);
					}
					LabelHierarchyDrawer(configuration, config, data, position, scale, dynamic_resource_index, iterator);
				}
				else {
					bool exists = ExistsResource(name);
					if (!exists) {
						UIDrawerInitializeLabelHierarchy initialize_data;
						initialize_data.config = &config;

						unsigned int storage_type_size = keep_storage ? iterator->storage_type_size : 0;
						ECS_FORWARD_STRUCT_MEMBERS_2(initialize_data, name, storage_type_size);
						InitializeDrawerElement(
							*this,
							&initialize_data,
							name,
							InitializeLabelHierarchyElement,
							DynamicConfiguration(configuration)
						);
					}
					LabelHierarchy(DynamicConfiguration(configuration), config, name, iterator, keep_storage);
				}
			}
			else {
				if (configuration & UI_CONFIG_DO_CACHE) {
					unsigned int label_size = keep_storage ? iterator->storage_type_size : 0;
					LabelHierarchyInitializer(configuration, config, name, label_size);
				}
			}
		}

		void UIDrawer::LabelHierarchyDrawer(
			size_t configuration, 
			const UIDrawConfig& config, 
			UIDrawerLabelHierarchyData* data,
			float2 position, 
			float2 scale,
			unsigned int dynamic_resource_index, 
			IteratorPolymorphic* iterator
		)
		{
			UIActionHandler whole_window_deselect = { LabelHierarchyDeselect, data, 0 };
			SetWindowClickable(&whole_window_deselect);

			float2 square_scale = GetSquareScale(scale.y);

			// aliases for sprite texture info
			float2 opened_top_left_uv, closed_top_left_uv, opened_bottom_right_uv, closed_bottom_right_uv;
			Color opened_color, closed_color;
			const wchar_t* opened_texture;
			const wchar_t* closed_texture;
			float2 expand_factor;
			float horizontal_bound = region_limit.x;

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

			auto get_callback_info = [&](const auto* callback, size_t config_flag, void* ptr_to_write) {
				if (configuration & config_flag) {
					callback = (decltype(callback))config.GetParameter(config_flag);
					if (!callback->copy_on_initialization && callback->data_size > 0) {
						memcpy(ptr_to_write, callback->data, callback->data_size);
					}
					return callback->phase;
				}
				return ECS_UI_DRAW_NORMAL;
			};

			const UIConfigLabelHierarchySelectableCallback* selectable = nullptr;
			ECS_UI_DRAW_PHASE selectable_callback_phase = get_callback_info(selectable, UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK, data->selectable_data);

			const UIConfigLabelHierarchyRightClick* right_click = nullptr;
			ECS_UI_DRAW_PHASE right_click_phase = get_callback_info(right_click, UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK, data->right_click_data);

			const UIConfigLabelHierarchyDragCallback* drag_callback = nullptr;
			ECS_UI_DRAW_PHASE drag_phase = get_callback_info(drag_callback, UI_CONFIG_LABEL_HIERARCHY_DRAG_LABEL, data->drag_data);

			const UIConfigLabelHierarchyRenameCallback* rename_callback = nullptr;
			ECS_UI_DRAW_PHASE rename_phase = get_callback_info(rename_callback, UI_CONFIG_LABEL_HIERARCHY_RENAME_LABEL, data->rename_data);

			const UIConfigLabelHierarchyDoubleClickCallback* double_click_callback = nullptr;
			ECS_UI_DRAW_PHASE double_click_phase = get_callback_info(double_click_callback, UI_CONFIG_LABEL_HIERARCHY_DOUBLE_CLICK_ACTION, data->double_click_data);

			Stream<char> filter = { nullptr, 0 };
			if (configuration & UI_CONFIG_LABEL_HIERARCHY_FILTER) {
				const UIConfigLabelHierarchyFilter* filter_config = (const UIConfigLabelHierarchyFilter*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_FILTER);
				filter = filter_config->filter;
			}

			// Used by the determine selection
			ActionData action_data = GetDummyActionData();

			UIConfigLabelHierarchyMonitorSelection* monitor_selection = nullptr;
			data->has_monitor_selection = false;
			if (configuration & UI_CONFIG_LABEL_HIERARCHY_MONITOR_SELECTION) {
				monitor_selection = (UIConfigLabelHierarchyMonitorSelection*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_MONITOR_SELECTION);
				data->monitor_selection = *monitor_selection;
				data->has_monitor_selection = true;

				SnapshotRunnable(data, 0, ECS_UI_DRAW_NORMAL, [](void* _data, ActionData* action_data) {
					UIDrawerLabelHierarchyData* data = (UIDrawerLabelHierarchyData*)_data;
					if (data->monitor_selection.ShouldUpdate()) {
						data->ChangeSelection(data->monitor_selection.Selection(), action_data);
						return true;
					}
					return false;
				});
			}

			// The aggregate phase - the "latest" phase of them all. Can't satisfy different phases
			ECS_UI_DRAW_PHASE click_action_phase = std::max(selectable_callback_phase, drag_phase);
			click_action_phase = std::max(click_action_phase, double_click_phase);

			// font size and character spacing are dummies, text color is the one that's needed for
			// drop down triangle color
			float2 font_size;
			float character_spacing;
			Color text_color;
			HandleText(configuration, config, text_color, font_size, character_spacing);

			UIDrawConfig internal_config;
			memcpy(&internal_config, &config, sizeof(internal_config));

			Color label_color = HandleColor(configuration, config);
			Color drag_highlight_color = label_color;
			drag_highlight_color.alpha = 150;

			UIConfigTextAlignment text_alignment;
			text_alignment.horizontal = ECS_UI_ALIGN_LEFT;
			text_alignment.vertical = ECS_UI_ALIGN_MIDDLE;
			internal_config.AddFlag(text_alignment);

			size_t label_configuration = UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y
				| UI_CONFIG_LABEL_TRANSPARENT;

			unsigned int selection_first = -1;
			unsigned int selection_last = -1;

			// While the determine count is 1, add the labels to the selection
			unsigned char determine_count = 0;
			if (data->determine_selection) {
				ECS_STACK_CAPACITY_STREAM(char, temp_first, 512);
				temp_first.CopyOther(data->first_selected_label.buffer, data->first_selected_label.size);
				Stream<char> last_label = { nullptr, 0 };
				const void* untyped_label = data->last_selected_label.buffer;
				if (data->label_size == 0) {
					untyped_label = &last_label;
					last_label = data->last_selected_label.AsIs<char>();
				}

				data->ChangeSelection(untyped_label, &action_data);
				data->first_selected_label.CopyOther(temp_first.buffer, temp_first.size);
			}

			if (data->is_dragging) {
				data->hovered_label.size = 0;
			}

			void* untyped_storage = ECS_STACK_ALLOC(data->label_size);
			while (iterator->Valid()) {
				// Peek the value
				unsigned int depth = 0;
				bool has_children = false;
				Stream<char> current_label;
				iterator->Peek(&current_label, &depth, &has_children);

				const void* untyped_label = &current_label;
				if (data->label_size != 0) {
					untyped_label = untyped_storage;
					iterator->PeekStorage(untyped_storage);
				}

				// TODO: At the moment, use the names directly. If a case where multiple nodes can have the same
				// label happens, then make a flag for that case and write it (when that does happen)

				/*size_t opened_index = 0;
				for (; opened_index < data->opened_labels.size; opened_index++) {
					Stream<char> last_name = PathFilename(data->opened_labels[index]);
					if (last_name == current_label) {
						break;
					}
				}*/

				unsigned int opened_index = data->FindOpenedLabel(untyped_label);

				if (opened_index == -1) {
					// Not opened, skip the node
					iterator->Has();
				}
				else {
					iterator->Next(nullptr);
				}

				// Verify if it passes the filter
				if (filter.size == 0 || FindFirstToken(current_label, filter).buffer != nullptr) {
					float current_gain = depth * layout.node_indentation;
					float2 current_position = { position.x + current_gain, position.y };
					float2 current_scale = { horizontal_bound - current_position.x, scale.y };

					if (data->determine_selection) {
						if (selection_first == -1 && data->CompareFirstLabel(untyped_label)) {
							selection_first = 0;
							determine_count++;
						}
						else if (selection_last == -1 && data->CompareLastLabel(untyped_label)) {
							selection_last = 0;
							determine_count++;
						}
					}

					if (data->determine_selection && determine_count == 1) {
						data->AddSelection(untyped_label, &action_data);
					}

					if (ValidatePosition(configuration, current_position, current_scale)) {
						Color current_color = label_color;

						float2 label_position = { current_position.x + horizontal_texture_offset, current_position.y };
						float2 label_scale = { current_scale.x - horizontal_texture_offset, current_scale.y };
						bool is_active = false;
						float active_label_scale = 0.0f;

						is_active = data->FindSelectedLabel(untyped_label) != -1;
						if (is_active) {
							current_color = ToneColor(label_color, 1.25f);
							is_active = true;
							active_label_scale = GetLabelScale(current_label).x;
						}

						bool is_cut = data->FindCopiedLabel(untyped_label) != -1 && data->is_selection_cut;
						if (is_cut) {
							current_color.alpha = 150;
							current_color = BlendColors(color_theme.background, current_color);
						}

						bool active_renamed = data->is_rename_label > 0 && is_active;
						if (active_renamed) {
							UIConfigTextInputCallback text_callback;
							text_callback.handler = { LabelHierarchyRenameLabel, data, 0, rename_phase };
							internal_config.AddFlag(text_callback);

							UIConfigAbsoluteTransform transform;
							transform.position = label_position;
							transform.scale = label_scale;
							internal_config.AddFlag(transform);

							unsigned char rename_was_1 = data->is_rename_label == 1;
							if (rename_was_1) {
								data->is_rename_label = 2;
								data->rename_label_storage.CopyOther(current_label);

								// Copy the current label into the rename label
								if (data->label_size == 0) {
									Stream<char>* char_label = (Stream<char>*)data->rename_label;
									char_label->CopyOther(current_label);
								}
								else {
									memcpy(data->rename_label, untyped_label, data->label_size);
								}
							}

							size_t input_configuration = ClearFlag(configuration, UI_CONFIG_DO_CACHE) | UI_CONFIG_ABSOLUTE_TRANSFORM |
								UI_CONFIG_TEXT_INPUT_CALLBACK | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_TEXT_ALIGNMENT;
							UIConfigBorder border;
							UIDrawerTextInput* text_input = TextInput(
								input_configuration,
								internal_config,
								"LabelHierarchyInput",
								&data->rename_label_storage
							);
							if (rename_was_1) {
								text_input->is_currently_selected = true;
								text_input->current_sprite_position = data->rename_label_storage.size;

								// Change the general action to be the text input
								UIDrawerTextInputActionData input_action_data;
								input_action_data.input = text_input;
								input_action_data.filter = UIDrawerTextInputFilterAll;
								system->ChangeFocusedWindowGeneral({ TextInputAction, &input_action_data, sizeof(input_action_data) });
							}

							// Update the frame handler - if the callback was not triggered.
							// Because the callback is always triggered inside the text input, we need to recheck for the flag here
							if (data->is_rename_label > 0) {
								unsigned int frame_handler_index = system->FindFrameHandler(LabelHierarchyRenameLabelFrameHandler);
								ECS_ASSERT(frame_handler_index != -1);
								UIDrawerLabelHierarchyRenameFrameHandlerData* frame_handler_data = (UIDrawerLabelHierarchyRenameFrameHandlerData*)system->GetFrameHandlerData(frame_handler_index);
								frame_handler_data->label_position = label_position;
								frame_handler_data->label_scale = label_scale;
							}

							// 2 additions were made here
							internal_config.flag_count -= 2;
						}
						else {
							size_t current_label_configuration = ClearFlag(configuration, UI_CONFIG_DO_CACHE) | label_configuration | UI_CONFIG_LABEL_TRANSPARENT;
							current_label_configuration |= is_cut ? UI_CONFIG_UNAVAILABLE_TEXT : 0;
							TextLabel(
								current_label_configuration,
								internal_config,
								current_label,
								label_position,
								label_scale
							);
						}

						bool is_dragging = data->is_dragging;
						if (is_dragging) {
							// Record the hovered label
							struct RunnableData {
								ECS_INLINE RunnableData() {}

								float2 position;
								float2 scale;
								union {
									Stream<char> char_label;
									size_t untyped_label[32];
								};
								UIDrawerLabelHierarchyData* data;
								Color drag_highlight_color;
							};

							ECS_ASSERT(data->label_size <= sizeof(RunnableData::untyped_label));

							RunnableData runnable_data;
							runnable_data.data = data;
							runnable_data.position = current_position;
							runnable_data.scale = current_scale;
							runnable_data.drag_highlight_color = drag_highlight_color;
							if (data->label_size != 0) {
								memcpy(runnable_data.untyped_label, untyped_label, data->label_size);
							}
							else {
								if (record_snapshot_runnables) {
									runnable_data.char_label = current_label.Copy(SnapshotRunnableAllocator());
								}
								else {
									runnable_data.char_label = current_label;
								}
							}

							SnapshotRunnable(&runnable_data, sizeof(runnable_data), ECS_UI_DRAW_NORMAL, [](void* _data, ActionData* action_data) {
								RunnableData* runnable_data = (RunnableData*)_data;
								UIDrawerLabelHierarchyData* data = runnable_data->data;

								float2 position = runnable_data->position;
								float2 scale = runnable_data->scale;
								if (IsPointInRectangle(action_data->mouse_position, position, scale)) {
									// We could have referenced the untyped label directly since it is type punned in
									// The union with the untyped label but this is more explicit
									const void* untyped_label = data->label_size > 0 ? runnable_data->untyped_label : (const void*)&runnable_data->char_label;
									data->SetHoveredLabel(untyped_label);

									// Display a hovered highlight
									action_data->system->SetSprite(
										action_data->dockspace,
										action_data->border_index,
										ECS_TOOLS_UI_TEXTURE_MASK,
										position,
										scale,
										action_data->buffers,
										action_data->counts,
										runnable_data->drag_highlight_color
									);
									return true;
								}
								return false;
							});
						}

						if (!active_renamed) {
							// Embedd the string into the data
							size_t _click_data[64];
							LabelHierarchyClickActionData* click_data = (LabelHierarchyClickActionData*)_click_data;
							click_data->hierarchy = data;
							unsigned int click_data_size = click_data->WriteLabel(untyped_label);

							AddGeneral(configuration, current_position, current_scale, { LabelHierarchyClickAction, click_data, click_data_size, click_action_phase });
							AddDefaultHoverable(configuration, current_position, current_scale, current_color);
							// Add a clickable such that the window will not appear like it can be moved
							AddClickable(configuration, current_position, current_scale, { SkipAction, nullptr, 0 });
						}

						// Embedd the string into the data
						size_t _change_state_data[64];
						LabelHierarchyChangeStateData* change_state_data = (LabelHierarchyChangeStateData*)_change_state_data;
						change_state_data->hierarchy = data;
						unsigned int change_data_size = change_state_data->WriteLabel(untyped_label);

						if (keep_triangle && has_children) {
							if (opened_index != -1) {
								SpriteRectangle(configuration, current_position, square_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, text_color);
							}
							else {
								SpriteRectangle(configuration, current_position, square_scale, ECS_TOOLS_UI_TEXTURE_TRIANGLE, text_color, { 1.0f, 0.0f }, { 0.0f, 1.0f });
							}
							AddClickable(configuration, current_position, square_scale, { LabelHierarchyChangeState, change_state_data, change_data_size, ECS_UI_DRAW_NORMAL });
						}

						float2 user_sprite_texture_position = current_position;
						user_sprite_texture_position.x += square_scale.x;
						if (configuration & UI_CONFIG_LABEL_HIERARCHY_SPRITE_TEXTURE) {
							if (opened_index != -1) {
								SpriteRectangle(configuration, user_sprite_texture_position, square_scale, opened_texture, opened_color, opened_top_left_uv, opened_bottom_right_uv);
							}
							else {
								SpriteRectangle(configuration, user_sprite_texture_position, square_scale, closed_texture, closed_color, closed_top_left_uv, closed_bottom_right_uv);
							}
							if (!keep_triangle && has_children) {
								AddClickable(configuration, user_sprite_texture_position, square_scale, { LabelHierarchyChangeState, change_state_data, change_data_size, ECS_UI_DRAW_NORMAL });
							}
						}

						if (!active_renamed) {
							if (configuration & UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK) {
								// Embedd the string into the data
								size_t _right_click_data[64];
								UIDrawerLabelHierarchyRightClickData* right_click_data = (UIDrawerLabelHierarchyRightClickData*)_right_click_data;
								right_click_data->data = data->right_click_data;
								right_click_data->hierarchy = data;
								right_click_data->hover_color = current_color;
								unsigned int write_size = right_click_data->WriteLabel(untyped_label);

								AddClickable(
									configuration, 
									current_position, 
									current_scale, 
									{ LabelHierarchyRightClickAction, right_click_data, write_size, right_click_phase },
									ECS_MOUSE_RIGHT
								);
							}

							if (is_active) {
								current_scale.x = ClampMin(current_scale.x, active_label_scale + square_scale.x * 2.0f);
								SolidColorRectangle(configuration, current_position, current_scale, current_color);
							}
						}
					}
					else {
						if (~configuration & UI_CONFIG_DO_NOT_UPDATE_RENDER_BOUNDS) {
							UpdateRenderBoundsRectangle(position, scale);
						}
						UpdateCurrentScale(position, scale);
					}

					//FinalizeRectangle(configuration, current_position, current_scale);
					NextRow(0.0f);
					position.y += current_scale.y;
				}

				// Deallocate the node
				if (configuration & UI_CONFIG_LABEL_HIERARCHY_DEALLOCATE_LABEL) {
					iterator->Deallocate(current_label.buffer);
				}
			}

			if (data->determine_selection) {
				// Can happen that the first label is ommited when doing for example selecting 12 to 1
				// Also the first label gets added 2 times, so remove it
				Stream<char> first_selected = data->first_selected_label.AsIs<char>();
				const void* untyped_first_label = &first_selected;
				if (data->label_size > 0) {
					untyped_first_label = data->first_selected_label.buffer;
				}

				unsigned int first_idx = data->FindSelectedLabel(untyped_first_label);
				if (first_idx == -1) {
					data->selected_labels.RemoveSwapBack(0, data->CopySize());
					data->AddSelection(untyped_first_label, &action_data);
				}

				data->determine_selection = false;
				if (monitor_selection != nullptr) {
					data->UpdateMonitorSelection(monitor_selection);
				}
				// Call the selection callback
				data->TriggerSelectable(&action_data);
			}

			if (configuration & UI_CONFIG_LABEL_HIERARCHY_BASIC_OPERATIONS) {
				SnapshotRunnable(data, 0, ECS_UI_DRAW_NORMAL, [](void* _data, ActionData* action_data) {
					UIDrawerLabelHierarchyData* data = (UIDrawerLabelHierarchyData*)_data;

					UISystem* system = action_data->system;
					Keyboard* keyboard = action_data->keyboard;
					// Check if the window is focused
					if (system->GetActiveWindow() == system->GetWindowIndexFromBorder(action_data->dockspace, action_data->border_index)) {
						// Check for Ctrl+C, Ctrl+X, Ctrl+V and delete
						if (!keyboard->IsCaptureCharacters()) {
							if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
								if (keyboard->IsPressed(ECS_KEY_C)) {
									data->SetSelectionCut(false);
									data->RecordSelection(action_data);
									return true;
								}
								else if (keyboard->IsPressed(ECS_KEY_X)) {
									data->SetSelectionCut(true);
									data->RecordSelection(action_data);
									return true;
								}
								else if (keyboard->IsPressed(ECS_KEY_V)) {
									// Call the appropriate callback
									// Only if there is no selection or just a single selection
									if (data->copied_labels.size > 0 /*&& (data->selected_labels.size == 0 || data->selected_labels.size == 1)*/) {
										if (data->is_selection_cut && data->cut_action != nullptr) {
											data->TriggerCut(action_data);
											return true;
										}
										else if (data->copy_action != nullptr) {
											data->TriggerCopy(action_data);
											return true;
										}
									}
								}
								else if (keyboard->IsPressed(ECS_KEY_D)) {
									data->SetSelectionCut(false);
									data->RecordSelection(action_data);

									data->TriggerCopy(action_data);
									return true;
								}
							}
							else if (keyboard->IsPressed(ECS_KEY_DELETE)) {
								if (data->selected_labels.size > 0) {
									data->TriggerDelete(action_data);
									return true;
								}
							}
						}
					}
					
					return false;
				});		
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerLabelHierarchyData* UIDrawer::LabelHierarchyInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, unsigned int storage_type_size)
		{
			UIDrawerLabelHierarchyData* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {
				data = GetMainAllocatorBuffer<UIDrawerLabelHierarchyData>();
				
				// Everything will be nullptr or 0
				memset(data, 0, sizeof(*data));

				auto set_callback = [this](Action& action, void*& data, const auto* callback) {
					data = callback->data;
					if (callback->data_size > 0) {
						data = GetMainAllocatorBuffer(callback->data_size);
						memcpy(data, callback->data, callback->data_size);
					}
					action = callback->callback;
				};

				if (configuration & UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK) {
					const UIConfigLabelHierarchySelectableCallback* callback = (const UIConfigLabelHierarchySelectableCallback*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_SELECTABLE_CALLBACK);
					set_callback(data->selectable_action, data->selectable_data, callback);
				}

				if (configuration & UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK) {
					const UIConfigLabelHierarchyRightClick* callback = (const UIConfigLabelHierarchyRightClick*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_RIGHT_CLICK);
					set_callback(data->right_click_action, data->right_click_data, callback);
				}
				
				if (configuration & UI_CONFIG_LABEL_HIERARCHY_DRAG_LABEL) {
					const UIConfigLabelHierarchyDragCallback* callback = (const UIConfigLabelHierarchyDragCallback*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_DRAG_LABEL);
					set_callback(data->drag_action, data->drag_data, callback);
					data->reject_same_label_drag = callback->reject_same_label_drag;
				}

				if (configuration & UI_CONFIG_LABEL_HIERARCHY_RENAME_LABEL) {
					const UIConfigLabelHierarchyRenameCallback* callback = (const UIConfigLabelHierarchyRenameCallback*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_RENAME_LABEL);
					set_callback(data->rename_action, data->rename_data, callback);
					data->rename_label_storage.buffer = (char*)GetMainAllocatorBuffer(sizeof(char) * 256, alignof(char));
					data->rename_label_storage.size = 0;
					data->rename_label_storage.capacity = 256;

					// The Stream<char> or the storage given by the user
					size_t renamed_label_capacity = storage_type_size == 0 ? sizeof(Stream<char>) + sizeof(char) * 256 : storage_type_size;
					void* allocation = GetMainAllocatorBuffer(renamed_label_capacity);
					if (storage_type_size == 0) {
						Stream<char>* char_label = (Stream<char>*)allocation;
						data->rename_label = char_label;
						char_label->InitializeFromBuffer(OffsetPointer(allocation, sizeof(Stream<char>)), 0);
					}
					else { 
						data->rename_label = allocation;
					}
				}
				else {
					data->rename_label = nullptr;
					data->rename_label_storage = { nullptr, 0, 0 };
				}

				if (configuration & UI_CONFIG_LABEL_HIERARCHY_DOUBLE_CLICK_ACTION) {
					const UIConfigLabelHierarchyDoubleClickCallback* callback = (const UIConfigLabelHierarchyDoubleClickCallback*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_DOUBLE_CLICK_ACTION);
					set_callback(data->double_click_action, data->double_click_data, callback);
				}

				if (configuration & UI_CONFIG_LABEL_HIERARCHY_BASIC_OPERATIONS) {
					const UIConfigLabelHierarchyBasicOperations* operations = (const UIConfigLabelHierarchyBasicOperations*)config.GetParameter(UI_CONFIG_LABEL_HIERARCHY_BASIC_OPERATIONS);
					
					if (operations->copy_handler.action != nullptr) {
						data->copy_action = operations->copy_handler.action;
						data->copy_data = operations->copy_handler.data;
						if (operations->copy_handler.data_size > 0) {
							data->copy_data = GetMainAllocatorBuffer(operations->copy_handler.data_size);
							memcpy(data->copy_data, operations->copy_handler.data, operations->copy_handler.data_size);
						}
					}

					if (operations->cut_handler.action != nullptr) {
						data->cut_action = operations->cut_handler.action;
						data->cut_data = operations->cut_handler.data;
						if (operations->cut_handler.data_size > 0) {
							data->cut_data = GetMainAllocatorBuffer(operations->cut_handler.data_size);
							memcpy(data->cut_data, operations->cut_handler.data, operations->cut_handler.data_size);
						}
					}

					if (operations->delete_handler.action != nullptr) {
						data->delete_action = operations->delete_handler.action;
						data->delete_data = operations->delete_handler.data;
						if (operations->delete_handler.data_size > 0) {
							data->delete_data = GetMainAllocatorBuffer(operations->delete_handler.data_size);
							memcpy(data->delete_data, operations->delete_handler.data, operations->delete_handler.data_size);
						}
					}
				}

				// For dynamic resources we need to record the identifier to update the allocations made
				if (configuration & UI_CONFIG_DYNAMIC_RESOURCE) {
					void* allocation = GetMainAllocatorBuffer(identifier.size * sizeof(char));
					uintptr_t ptr = (uintptr_t)allocation;
					data->identifier.InitializeAndCopy(ptr, identifier);
				}
				else {
					data->identifier = { nullptr, 0 };
				}

				const size_t MAX_CAPACITY = storage_type_size == 0 ? 128 : storage_type_size;
				void* allocation = GetMainAllocatorBuffer(sizeof(char) * MAX_CAPACITY * 4);
				uintptr_t ptr = (uintptr_t)allocation;
				data->first_selected_label.InitializeFromBuffer(ptr, 0, MAX_CAPACITY);
				data->last_selected_label.InitializeFromBuffer(ptr, 0, MAX_CAPACITY);
				data->hovered_label.InitializeFromBuffer(ptr, 0, MAX_CAPACITY);

				data->label_size = storage_type_size;
				data->window_index = window_index;

				return data;
			});

			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Label List

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::LabelList(Stream<char> name, Stream<Stream<char>> labels) {
			UIDrawConfig config;
			LabelList(0, config, name, labels);
		}

		void UIDrawer::LabelList(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
					UIDrawerLabelList* data = (UIDrawerLabelList*)GetResource(name);

					LabelListDrawer(configuration, config, data, position, scale);
				}
				else {
					LabelListDrawer(configuration, config, name, labels, position, scale);
				}
			}
			else {
				if (configuration & UI_CONFIG_DO_CACHE) {
					LabelListInitializer(configuration, config, name, labels);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		struct LabelListDrawerWithData {
			void DrawName(UIDrawer* drawer, size_t configuration, const UIDrawConfig& config, float2 position, float2 scale) {
				drawer->ElementName(configuration, config, &data->name, position, scale);
			}

			void DrawElement(
				UIDrawer* drawer, 
				size_t configuration,
				const UIDrawConfig& config,
				float2& position, 
				float2 scale, 
				float2 square_scale,
				unsigned int index
			) {
				position.x += square_scale.x + drawer->layout.element_indentation;
				drawer->TextLabelDrawer(
					configuration | UI_CONFIG_LABEL_TRANSPARENT,
					config,
					data->labels.buffer + index,
					position,
					scale
				);
			}

			unsigned int LabelCount() const {
				return data->labels.size;
			}

			UIDrawerLabelList* data;
		};

		struct LabelListDrawerWithParameters {
			void DrawName(UIDrawer* drawer, size_t configuration, const UIDrawConfig& config, float2 position, float2 scale) {
				drawer->ElementName(configuration, config, name, position, scale);
			}

			void DrawElement(
				UIDrawer* drawer, 
				size_t configuration,
				const UIDrawConfig& config,
				float2 position,
				float2 scale,
				float2 square_scale,
				unsigned int index
			) {
				drawer->UpdateCurrentColumnScale(square_scale.x);
				drawer->Indent();
				drawer->TextLabel(
					ClearFlag(configuration, UI_CONFIG_DO_CACHE) | UI_CONFIG_LABEL_TRANSPARENT,
					config,
					labels.buffer[index]
				);
			}

			unsigned int LabelCount() const {
				return labels.size;
			}

			Stream<char> name;
			Stream<Stream<char>> labels;
		};

		template<typename Parameters>
		void UIDrawerLabelListDrawer(UIDrawer* drawer, size_t configuration, const UIDrawConfig& config, Parameters& data, float2 position, float2 scale) {
			if (~configuration & UI_CONFIG_LABEL_LIST_NO_NAME) {
				data.DrawName(drawer, configuration, config, position, scale);
				drawer->NextRow();
			}

			Color font_color;
			float2 font_size;
			float character_spacing;
			drawer->HandleText(configuration, config, font_color, font_size, character_spacing);

			float font_scale = drawer->system->GetTextSpriteYScale(font_size.y);
			float label_scale = font_scale + drawer->element_descriptor.label_padd.y * 2.0f;
			float2 square_scale = drawer->GetSquareScaleScaled(drawer->element_descriptor.label_list_circle_size);

			drawer->OffsetNextRow(drawer->layout.node_indentation);
			drawer->OffsetX(drawer->layout.node_indentation);

			for (size_t index = 0; index < data.LabelCount(); index++) {
				position = drawer->GetCurrentPosition();
				float2 circle_position = { position.x, AlignMiddle(position.y, label_scale, square_scale.y) };
				drawer->SpriteRectangle(configuration, circle_position, square_scale, ECS_TOOLS_UI_TEXTURE_FILLED_CIRCLE, font_color);
				
				data.DrawElement(drawer, configuration, config, position, scale, square_scale, index);
				drawer->NextRow();
			}

			drawer->OffsetNextRow(-drawer->layout.node_indentation);
			drawer->OffsetX(-drawer->layout.node_indentation);
		}
		
		void UIDrawer::LabelListDrawer(size_t configuration, const UIDrawConfig& config, UIDrawerLabelList* data, float2 position, float2 scale) {
			LabelListDrawerWithData parameters;
			parameters.data = data;
			
			UIDrawerLabelListDrawer(this, configuration, config, parameters, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::LabelListDrawer(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels, float2 position, float2 scale) {
			LabelListDrawerWithParameters parameters;
			parameters.labels = labels;
			parameters.name = name;

			UIDrawerLabelListDrawer(this, configuration, config, parameters, position, scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerLabelList* UIDrawer::LabelListInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels) {
			UIDrawerLabelList* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {
				data = GetMainAllocatorBuffer<UIDrawerLabelList>();
				data->labels.buffer = (UIDrawerTextElement*)GetMainAllocatorBuffer(sizeof(UIDrawerTextElement) * labels.size);

				InitializeElementName(configuration, UI_CONFIG_LABEL_LIST_NO_NAME, config, identifier, &data->name, { 0.0f, 0.0f }, { 0.0f, 0.0f });
				for (size_t index = 0; index < labels.size; index++) {
					ConvertTextToWindowResource(configuration, config, labels[index], data->labels.buffer + index, { 0.0f, 0.0f }, { 0.0f, 0.0f });
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
		void UIDrawer::Menu(Stream<char> name, UIDrawerMenuState* state) {
			UIDrawConfig config;
			Menu(0, config, name, state);
		}

		// State can be stack allocated; name should be unique if drawing with a sprite
		void UIDrawer::Menu(size_t configuration, const UIDrawConfig& config, Stream<char> name, UIDrawerMenuState* state) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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

					// Update the data - something might have changed in the meantime
					UIDrawerMenu* data = (UIDrawerMenu*)GetResource(name);
					ReinitializeMenuState(this, name, data, state);

					Menu(DynamicConfiguration(configuration), config, name, state);
				}
			}
			else {
				if (configuration & UI_CONFIG_DO_CACHE) {
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

		void UIDrawer::FloatInput(Stream<char> name, float* value, float default_value, float lower_bound, float upper_bound) {
			UIDrawConfig config;
			FloatInput(0, config, name, value, default_value, lower_bound, upper_bound);
		}

		void UIDrawer::FloatInput(size_t configuration, UIDrawConfig& config, Stream<char> name, float* value, float default_value, float lower_bound, float upper_bound) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
					FloatInputInitializer(configuration, config, name, value, default_value, lower_bound, upper_bound, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatInputGroup(
			size_t count,
			Stream<char> group_name,
			Stream<char>* names,
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
			Stream<char> group_name,
			Stream<char>* names,
			float** values,
			const float* default_values,
			const float* lower_bound,
			const float* upper_bound
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
					FloatInputGroupInitializer(configuration, config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleInput(Stream<char> name, double* value, double default_value, double lower_bound, double upper_bound) {
			UIDrawConfig config;
			DoubleInput(0, config, name, value, default_value, lower_bound, upper_bound);
		}

		void UIDrawer::DoubleInput(size_t configuration, UIDrawConfig& config, Stream<char> name, double* value, double default_value, double lower_bound, double upper_bound) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
					DoubleInputInitializer(configuration, config, name, value, default_value, lower_bound, upper_bound, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleInputGroup(
			size_t count,
			Stream<char> group_name,
			Stream<char>* names,
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
			Stream<char> group_name,
			Stream<char>* names,
			double** values,
			const double* default_values,
			const double* lower_bound,
			const double* upper_bound
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
					DoubleInputGroupInitializer(configuration, config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Integer Draggable

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntDraggable(Stream<char> name, Integer* value, Integer min, Integer max, Integer default_value)
		{
			UIDrawConfig config;
			IntDraggable(0, config, name, value, min, max, default_value);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntDraggable, Stream<char>, integer*, integer, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::IntDraggable(size_t configuration, UIDrawConfig& config, Stream<char> name, Integer* value, Integer min, Integer max, Integer default_value)
		{
			IntSlider<Integer>(configuration | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE, config, name, value, min, max, default_value);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntDraggable, size_t, UIDrawConfig&, Stream<char>, integer*, integer, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::IntDraggableGroup(size_t count, Stream<char> group_name, Stream<char>* names, Integer** value, const Integer* min, const Integer* max, const Integer* default_value)
		{
			UIDrawConfig config;
			IntDraggableGroup(0, config, count, group_name, names, value, min, max, default_value);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntDraggableGroup, size_t, Stream<char>, Stream<char>*, integer**, const integer*, const integer*, const integer*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::IntDraggableGroup(size_t configuration, UIDrawConfig& config, size_t count, Stream<char> group_name, Stream<char>* names, Integer** value, const Integer* min, const Integer* max, const Integer* default_value)
		{
			IntSliderGroup<Integer>(configuration | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE, config, count, group_name, names, value, min, max, default_value);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntDraggableGroup, size_t, UIDrawConfig&, size_t, Stream<char>, Stream<char>*, integer**, const integer*, const integer*, const integer*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		template<typename Integer>
		void UIDrawer::IntInput(Stream<char> name, Integer* value, Integer default_value, Integer min, Integer max) {
			UIDrawConfig config;
			IntInput(0, config, name, value, min, max);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInput, Stream<char>, integer*, integer, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::IntInput(size_t configuration, UIDrawConfig& config, Stream<char> name, Integer* value, Integer default_value, Integer min, Integer max) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
					IntInputInitializer<Integer>(configuration, config, name, value, default_value, min, max, position, scale);
				}
			}
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInput, size_t, UIDrawConfig&, Stream<char>, integer*, integer, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntInputGroup(
			size_t count,
			Stream<char> group_name,
			Stream<char>* names,
			Integer** values,
			const Integer* default_values,
			const Integer* lower_bound,
			const Integer* upper_bound
		) {
			UIDrawConfig config;
			IntInputGroup(0, config, count, group_name, names, values, default_values, lower_bound, upper_bound);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInputGroup, size_t, Stream<char>, Stream<char>*, integer**, const integer*, const integer*, const integer*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::IntInputGroup(
			size_t configuration,
			UIDrawConfig& config,
			size_t count,
			Stream<char> group_name,
			Stream<char>* names,
			Integer** values,
			const Integer* default_values,
			const Integer* lower_bound,
			const Integer* upper_bound
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
					IntInputGroupInitializer<Integer>(configuration, config, group_name, count, names, values, default_values, lower_bound, upper_bound, position, scale);
				}
			}
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntInputGroup, size_t, UIDrawConfig&, size_t, Stream<char>, Stream<char>*, integer**, const integer*, const integer*, const integer*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::NextRow(float count) {
			min_render_bounds.y = std::min(min_render_bounds.y, current_y - region_render_offset.y);
			current_y += count * layout.next_row_y_offset + current_row_y_scale;
			current_x = GetNextRowXPosition();
			current_row_y_scale = 0.0f;
			current_column_x_scale = 0.0f;

			// This records the last
			draw_mode_extra_float.y = current_y;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::NextRowWindowSize(float percentage)
		{
			min_render_bounds.y = std::min(min_render_bounds.y, current_y - region_render_offset.y);
			current_y += (region_limit.y - region_fit_space_vertical_offset) * percentage + current_row_y_scale;
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
			region_fit_space_horizontal_offset += value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::OffsetX(float value) {
			current_x += value;
			//current_column_x_scale -= value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::OffsetY(float value) {
			current_y += value;
			//current_row_y_scale -= value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Progress bar

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::ProgressBar(Stream<char> name, float percentage, float x_scale_factor) {
			UIDrawConfig config;
			ProgressBar(0, config, name, percentage, x_scale_factor);
		}

		void UIDrawer::ProgressBar(size_t configuration, UIDrawConfig& config, Stream<char> name, float percentage, float x_scale_factor) {
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
					ConvertFloatToChars(temp_stream, percent_value, 0);
					temp_stream.Add('%');
					temp_stream[temp_stream.size] = '\0';
					auto text_stream = GetTextStream(configuration, temp_stream.size * 6);
					float2 text_position = { position.x + scale.x * percentage, position.y };

					Color text_color;
					float character_spacing;
					float2 font_size;
					HandleText(configuration, config, text_color, font_size, character_spacing);
					system->ConvertCharactersToTextSprites(
						temp_stream,
						text_position,
						text_stream.buffer,
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
						ECS_UI_ALIGN_LEFT,
						ECS_UI_ALIGN_MIDDLE
					);
					x_position = ClampMax(x_position, position.x + scale.x - element_descriptor.label_padd.x - text_span.x);
					TranslateText(x_position, y_position, text_stream, 0, 0);

					HandleRectangleActions(configuration, config, position, scale);
				}

				UIConfigTextAlignment alignment;
				alignment.horizontal = ECS_UI_ALIGN_LEFT;
				alignment.vertical = ECS_UI_ALIGN_MIDDLE;
				UIConfigTextAlignment previous_alignment;
				if (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
					config.SetExistingFlag(alignment, previous_alignment);
				}
				else {
					config.AddFlag(alignment);
				}

				FinalizeRectangle(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, position, scale);
				position.x += scale.x + layout.element_indentation * 0.5f;
				TextLabel(ClearFlag(configuration, UI_CONFIG_DO_CACHE) | UI_CONFIG_TEXT_ALIGNMENT | UI_CONFIG_LABEL_TRANSPARENT, config, name, position, scale);

				if (configuration & UI_CONFIG_TEXT_ALIGNMENT) {
					config.SetExistingFlag(previous_alignment, alignment);
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

		void UIDrawer::PushIdentifierStack(Stream<char> identifier) {
			ECS_ASSERT(identifier.size + current_identifier.size < system->m_descriptors.misc.drawer_identifier_memory);
			ECS_ASSERT(identifier_stack.size < system->m_descriptors.misc.drawer_temp_memory);

			identifier.CopyTo(current_identifier.buffer + current_identifier.size);
			identifier_stack.Add(identifier.size);
			current_identifier.size += identifier.size;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::PushIdentifierStackEx(Stream<char> identifier)
		{
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(char, identifier_final, identifier.size);
			Stream<char> pattern = FindFirstToken(identifier, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
			if (pattern.size > 0) {
				identifier_final.CopyOther(Stream<char>(identifier.buffer, pattern.buffer - identifier.buffer));
				pattern.buffer += ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT;
				pattern.size -= ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT;
				identifier_final.AddStream(pattern);
			}
			else {
				identifier_final = identifier;
			}

			PushIdentifierStack(identifier_final);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::PushIdentifierStack(size_t index) {
			char temp_chars[64];
			Stream<char> stream_char = Stream<char>(temp_chars, 0);
			ConvertIntToChars(stream_char, (int64_t)index);
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

		void UIDrawer::PushDragDrop(Stream<Stream<char>> names, bool highlight_border, Color highlight_color)
		{
			acquire_drag_drop.names = names;
			acquire_drag_drop.highlight_color = highlight_color;
			acquire_drag_drop.highlight_element = highlight_border;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		Stream<void> UIDrawer::ReceiveDragDrop(bool remove_drag_drop, unsigned int* matched_name)
		{
			Stream<void> data = acquire_drag_drop.payload;
			if (matched_name != nullptr) {
				*matched_name = acquire_drag_drop.matched_name;
			}
			if (remove_drag_drop) {
				acquire_drag_drop.names.size = 0;
			}
			return data;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::RemoveAllocation(const void* allocation) {
			system->m_memory->Deallocate(allocation);
			system->RemoveWindowMemoryResource(window_index, allocation);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIReservedHandler UIDrawer::ReserveHoverable(ECS_UI_DRAW_PHASE phase)
		{
			return system->ReserveHoverable(dockspace, border_index, phase);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIReservedHandler UIDrawer::ReserveClickable(ECS_MOUSE_BUTTON button_type, ECS_UI_DRAW_PHASE phase)
		{
			return system->ReserveClickable(dockspace, border_index, phase, button_type);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIReservedHandler UIDrawer::ReserveGeneral(ECS_UI_DRAW_PHASE phase)
		{
			return system->ReserveGeneral(dockspace, border_index, phase);
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

				ECS_UI_DRAW_PHASE phase = HandlePhase(configuration);
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
					system->RemoveSpriteTexture(dockspace, border_index, phase, ECS_UI_SPRITE_CLUSTER);
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
				ForEachMouseButton([&](ECS_MOUSE_BUTTON button_type) {
					dockspace->borders[border_index].clickable_handler[button_type].position_x.size = state.clickable_count[button_type];
				});
				dockspace->borders[border_index].general_handler.position_x.size = state.general_count;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Sentence

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Sentence(Stream<char> text) {
			UIDrawConfig config;
			Sentence(0, config, text);
		}

		void UIDrawer::Sentence(size_t configuration, const UIDrawConfig& config, Stream<char> text) {
			if (!initializer) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				void* resource = nullptr;

				if (configuration & UI_CONFIG_DO_CACHE) {
					resource = GetResource(text);
				}
				SentenceDrawer(configuration, config, HandleResourceIdentifier(text), resource, position);
			}
			else {
				if (configuration & UI_CONFIG_DO_CACHE) {
					SentenceInitializer(configuration, config, text);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Selectable List

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SelectableList(Stream<Stream<char>> labels, unsigned int* selected_label)
		{
			UIDrawConfig config;
			SelectableList(0, config, labels, selected_label);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SelectableList(size_t configuration, const UIDrawConfig& config, Stream<Stream<char>> labels, unsigned int* selected_label)
		{
			// This doesn't need an initializer
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			struct SelectData {
				unsigned int* selected_label;
				unsigned int index;
			};

			auto select_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				SelectData* data = (SelectData*)_data;
				*data->selected_label = data->index;
			};

			for (size_t index = 0; index < labels.size; index++) {
				SelectData select_data;
				select_data.selected_label = selected_label;
				select_data.index = index;
				Button(configuration, config, labels[index], { select_action, &select_data, sizeof(select_data) });
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SelectableList(Stream<Stream<char>> labels, CapacityStream<char>* selected_label)
		{
			UIDrawConfig config;
			SelectableList(0, config, labels, selected_label);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SelectableList(size_t configuration, const UIDrawConfig& config, Stream<Stream<char>> labels, CapacityStream<char>* selected_label)
		{
			// This doesn't need an initializer
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			struct SelectData {
				unsigned int* Size() {
					return &label_size;
				}

				CapacityStream<char>* selected_label;
				unsigned int label_size;
			};

			auto select_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				SelectData* data = (SelectData*)_data;
				Stream<char> label = GetCoalescedStreamFromType(data).As<char>();
				data->selected_label->CopyOther(label);
			};

			for (size_t index = 0; index < labels.size; index++) {
				size_t storage[512];
				unsigned int write_size;
				SelectData* select_data = CreateCoalescedStreamIntoType<SelectData>(storage, labels[index], &write_size);
				select_data->selected_label = selected_label;

				Button(configuration, config, labels[index], { select_action, select_data, write_size });
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

		void UIDrawer::SetCurrentPositionToHeader()
		{
			SetCurrentX(region_position.x);
			SetCurrentY(region_fit_space_vertical_offset - layout.next_row_y_offset);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetCurrentPositionToStart()
		{
			SetCurrentX(region_fit_space_horizontal_offset);
			SetCurrentY(region_fit_space_vertical_offset);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<bool reduce_values>
		void UIDrawer::SetLayoutFromZoomXFactor(float zoom_factor) {
			if constexpr (reduce_values) {
				zoom_factor *= zoom_inverse.x;
			}

			layout.default_element_x *= zoom_factor;
			layout.element_indentation *= zoom_factor;
			layout.next_row_padding *= zoom_factor;

			element_descriptor.label_padd.x *= zoom_factor;
			element_descriptor.slider_length.x *= zoom_factor;
			element_descriptor.slider_shrink.x *= zoom_factor;

			font.size *= zoom_factor;
			font.character_spacing *= zoom_factor;
		}

		ECS_TEMPLATE_FUNCTION_BOOL(void, UIDrawer::SetLayoutFromZoomXFactor, float);

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<bool reduce_values>
		void UIDrawer::SetLayoutFromZoomYFactor(float zoom_factor) {
			if constexpr (reduce_values) {
				zoom_factor *= zoom_inverse.y;
			}

			layout.default_element_y *= zoom_factor;
			layout.next_row_y_offset *= zoom_factor;

			element_descriptor.label_padd.y *= zoom_factor;
			element_descriptor.slider_length.y *= zoom_factor;
			element_descriptor.slider_shrink.y *= zoom_factor;
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
					Rectangle((configuration | UI_CONFIG_DO_NOT_ADVANCE) & ~(UI_CONFIG_RECTANGLE_HOVERABLE_ACTION | UI_CONFIG_RECTANGLE_CLICKABLE_ACTION | 
						UI_CONFIG_RECTANGLE_GENERAL_ACTION), configs[index], position, scale);
				}
				position.x += offsets[index].x;
				position.y += offsets[index].y;
			}

			float2 total_scale = { position.x - initial_position.x, position.y - initial_position.y };
			if (~configuration & UI_CONFIG_RECTANGLES_INDIVIDUAL_ACTIONS) {
				if (configuration & UI_CONFIG_RECTANGLE_HOVERABLE_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_RECTANGLE_HOVERABLE_ACTION);
					AddHoverable(configuration, initial_position, total_scale, *handler);
				}
				if (configuration & UI_CONFIG_RECTANGLE_CLICKABLE_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_RECTANGLE_HOVERABLE_ACTION);
					AddClickable(configuration, initial_position, total_scale, *handler);
				}
				if (configuration & UI_CONFIG_RECTANGLE_GENERAL_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_RECTANGLE_HOVERABLE_ACTION);
					AddGeneral(configuration, initial_position, total_scale, *handler);
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
					Rectangle((configuration | UI_CONFIG_DO_NOT_ADVANCE) & ~(UI_CONFIG_RECTANGLE_HOVERABLE_ACTION | UI_CONFIG_RECTANGLE_CLICKABLE_ACTION
						| UI_CONFIG_RECTANGLE_GENERAL_ACTION), configs[index], position, scale);
				}
				position.x += offset.x;
				position.y += offset.y;
			}

			float2 total_scale = { position.x - initial_position.x, position.y - initial_position.y };
			if (~configuration & UI_CONFIG_RECTANGLES_INDIVIDUAL_ACTIONS) {
				if (configuration & UI_CONFIG_RECTANGLE_HOVERABLE_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_RECTANGLE_HOVERABLE_ACTION);
					AddHoverable(configuration, initial_position, total_scale, *handler);
				}
				if (configuration & UI_CONFIG_RECTANGLE_CLICKABLE_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_RECTANGLE_CLICKABLE_ACTION);
					AddClickable(configuration, initial_position, total_scale, *handler);
				}
				if (configuration & UI_CONFIG_RECTANGLE_GENERAL_ACTION) {
					const UIActionHandler* handler = (const UIActionHandler*)overall_config.GetParameter(UI_CONFIG_RECTANGLE_GENERAL_ACTION);
					AddGeneral(configuration, initial_position, total_scale, *handler);
				}
			}

			FinalizeRectangle(configuration, initial_position, total_scale);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Sliders

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Slider(
			Stream<char> name,
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
			Stream<char> name,
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
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
					SliderInitializer(configuration, config, name, position, scale, byte_size, value_to_modify, lower_bound, upper_bound, default_value, functions);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SliderGroup(
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
		) {
			UIDrawConfig config;
			SliderGroup(0, config, count, group_name, names, byte_size, values_to_modify, lower_bounds, upper_bounds, default_values, functions, filter);
		}

		void UIDrawer::SliderGroup(
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
		) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
					SliderGroupInitializer(configuration, config, count, group_name, names, byte_size, values_to_modify, lower_bounds, upper_bounds, default_values, functions, position, scale);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma region Float Slider

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatSlider(Stream<char> name, float* value_to_modify, float lower_bound, float upper_bound, float default_value, unsigned int precision) {
			UIDrawConfig config;
			FloatSlider(0, config, name, value_to_modify, lower_bound, upper_bound, default_value, precision);
		}

		void UIDrawer::FloatSlider(size_t configuration, UIDrawConfig& config, Stream<char> name, float* value_to_modify, float lower_bound, float upper_bound, float default_value, unsigned int precision) {
			UIDrawerSliderFunctions functions = UIDrawerGetFloatSliderFunctions(precision);
			Slider(configuration, config, name, sizeof(float), value_to_modify, &lower_bound, &upper_bound, &default_value, functions, UIDrawerTextInputFilterNumbers);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatSliderGroup(
			size_t count,
			Stream<char> group_name,
			Stream<char>* names,
			float** values_to_modify,
			const float* lower_bounds,
			const float* upper_bounds,
			const float* default_values,
			unsigned int precision
		) {
			UIDrawConfig config;
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
			Stream<char> group_name,
			Stream<char>* names,
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

#pragma endregion

#pragma region Float Draggable

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatDraggable(Stream<char> name, float* value_to_modify, float lower_bound, float upper_bound, float default_value, unsigned int precision)
		{
			UIDrawConfig config;
			FloatDraggable(0, config, name, value_to_modify, lower_bound, upper_bound, default_value, precision);
		}

		void UIDrawer::FloatDraggable(size_t configuration, UIDrawConfig& config, Stream<char> name, float* value_to_modify, float lower_bound, float upper_bound, float default_value, unsigned int precision)
		{
			FloatSlider(configuration | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE, config, name, value_to_modify, lower_bound, upper_bound, default_value, precision);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::FloatDraggableGroup(size_t count, Stream<char> group_name, Stream<char>* names, float** values_to_modify, const float* lower_bounds, const float* upper_bounds, const float* default_values, unsigned int precision)
		{
			UIDrawConfig config;
			FloatDraggableGroup(0, config, count, group_name, names, values_to_modify, lower_bounds, upper_bounds, default_values, precision);
		}

		void UIDrawer::FloatDraggableGroup(size_t configuration, UIDrawConfig& config, size_t count, Stream<char> group_name, Stream<char>* names, float** values_to_modify, const float* lower_bounds, const float* upper_bounds, const float* default_values, unsigned int precision)
		{
			FloatSliderGroup(configuration | UI_CONFIG_SLIDER_MOUSE_DRAGGABLE, config, count, group_name, names, values_to_modify, lower_bounds, upper_bounds, default_values, precision);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Double Slider

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleSlider(Stream<char> name, double* value_to_modify, double lower_bound, double upper_bound, double default_value, unsigned int precision) {
			UIDrawConfig config;
			DoubleSlider(0, config, name, value_to_modify, lower_bound, upper_bound, default_value, precision);
		}

		void UIDrawer::DoubleSlider(size_t configuration, UIDrawConfig& config, Stream<char> name, double* value_to_modify, double lower_bound, double upper_bound, double default_value, unsigned int precision) {
			UIDrawerSliderFunctions functions = UIDrawerGetDoubleSliderFunctions(precision);
			Slider(configuration, config, name, sizeof(double), value_to_modify, &lower_bound, &upper_bound, &default_value, functions, UIDrawerTextInputFilterNumbers);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DoubleSliderGroup(
			size_t count,
			Stream<char> group_name,
			Stream<char>* names,
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
			Stream<char> group_name,
			Stream<char>* names,
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

#pragma endregion

#pragma region Int Slider

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntSlider(Stream<char> name, Integer* value_to_modify, Integer lower_bound, Integer upper_bound, Integer default_value) {
			UIDrawConfig config;
			IntSlider(0, config, name, value_to_modify, lower_bound, upper_bound, default_value);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntSlider, Stream<char>, integer*, integer, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::IntSlider(size_t configuration, UIDrawConfig& config, Stream<char> name, Integer* value_to_modify, Integer lower_bound, Integer upper_bound, Integer default_value) {
			UIDrawerSliderFunctions functions = UIDrawerGetIntSliderFunctions<Integer>();
			Slider(configuration, config, name, sizeof(Integer), value_to_modify, &lower_bound, &upper_bound, &default_value, functions, UIDrawerTextInputFilterNumbers);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntSlider, size_t, UIDrawConfig&, Stream<char>, integer*, integer, integer, integer);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawer::IntSliderGroup(
			size_t count,
			Stream<char> group_name,
			Stream<char>* names,
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

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntSliderGroup, size_t, Stream<char>, Stream<char>*, integer**, const integer*, const integer*, const integer*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		template<typename Integer>
		void UIDrawer::IntSliderGroup(
			size_t configuration,
			UIDrawConfig& config,
			size_t count,
			Stream<char> group_name,
			Stream<char>* names,
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

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawer::IntSliderGroup, size_t, UIDrawConfig&, size_t, Stream<char>, Stream<char>*, integer**, const integer*, const integer*, const integer*);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma endregion

#pragma region Selection Input

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SelectionInput(Stream<char> name, CapacityStream<char>* selection, Stream<wchar_t> texture, const UIWindowDescriptor* window_descriptor, UIActionHandler selection_callback)
		{
			UIDrawConfig config;
			SelectionInput(0, config, name, selection, texture, window_descriptor, selection_callback);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SelectionInput(
			size_t configuration,
			const UIDrawConfig& config,
			Stream<char> name,
			CapacityStream<char>* selection,
			Stream<wchar_t> texture,
			const UIWindowDescriptor* window_descriptor,
			UIActionHandler selection_callback
		)
		{
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			// There is no need for an initializer
			configuration = ClearFlag(configuration, UI_CONFIG_DO_CACHE);

			bool window_dependent = HasFlag(configuration, UI_CONFIG_WINDOW_DEPENDENT_SIZE);

			bool has_name = name.size > 0;
			float name_size = has_name ? ElementNameSize(configuration, config, name, scale) : 0.0f;

			// Draw the name, if any
			float2 icon_size = GetSquareScale(scale.y);
			scale.x = ClampMin(scale.x, icon_size.x + layout.element_indentation + 0.005f);

			float2 total_scale = window_dependent ? scale : float2(name_size + scale.x, scale.y);

			HandleFitSpaceRectangle(configuration, position, total_scale);

			HandleAcquireDrag(configuration, position, total_scale);

			if (configuration & UI_CONFIG_GET_TRANSFORM) {
				UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
				*get_transform->position = position;
				*get_transform->scale = total_scale;
			}

			bool is_active = true;
			if (configuration & UI_CONFIG_ACTIVE_STATE) {
				UIConfigActiveState* state = (UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
				is_active = state->state;
			}

			bool name_first = HasFlag(configuration, UI_CONFIG_ELEMENT_NAME_FIRST);
			if (name.size > 0) {
				if (name_first) {
					ElementName(configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y, config, name, position, scale);
				}
				total_scale.x -= name_size;
			}

			float _label_scale = total_scale.x - icon_size.x - layout.element_indentation;
			_label_scale = ClampMin(_label_scale, 0.0f);

			float2 label_scale = { _label_scale, scale.y };
			// The fixed scale label doesn't finalize the rectangle
			FixedScaleTextLabel(configuration, config, *selection, position, label_scale);

			// Check the clickable handler
			if (HasFlag(configuration, UI_CONFIG_SELECTION_INPUT_LABEL_CLICKABLE)) {
				const UIConfigSelectionInputLabelClickable* clickable = (const UIConfigSelectionInputLabelClickable*)config.GetParameter(UI_CONFIG_SELECTION_INPUT_LABEL_CLICKABLE);
				if (clickable->double_click_action) {
					AddDoubleClickAction(
						configuration, 
						position, 
						label_scale, 
						clickable->double_click_duration_between_clicks, 
						clickable->first_click_handler, 
						clickable->handler, 
						name
					);
				}
				else {
					AddDefaultClickable(configuration, position, label_scale, {}, clickable->handler);
				}
			}

			FinalizeRectangle(configuration | UI_CONFIG_INDENT_INSTEAD_OF_NEXT_ROW, position, label_scale);

			Stream<char> tooltip_text = *selection;
			bool tooltip_text_stable = true;
			if (configuration & UI_CONFIG_SELECTION_INPUT_OVERRIDE_HOVERABLE) {
				const UIConfigSelectionInputOverrideHoverable* override = (const UIConfigSelectionInputOverrideHoverable*)config.GetParameter(UI_CONFIG_SELECTION_INPUT_OVERRIDE_HOVERABLE);
				tooltip_text_stable = override->stable;
				tooltip_text = override->text;
			}

			const float TOOLTIP_OFFSET = 0.01f;

			UITextTooltipHoverableData tooltip_data;
			tooltip_data.characters = tooltip_text;
			tooltip_data.base.offset_scale = { false, true };
			tooltip_data.base.center_horizontal_x = true;
			tooltip_data.base.offset.y = TOOLTIP_OFFSET;

			AddTextTooltipHoverable(configuration, position, label_scale, &tooltip_data, tooltip_text_stable);

			size_t SPRITE_CONFIGURATION = UI_CONFIG_MAKE_SQUARE | UI_CONFIG_MENU_BUTTON_SPRITE;

			UIDrawConfig menu_config;
			UIConfigMenuButtonSprite menu_button_sprite;
			menu_button_sprite.texture = texture;
			if (!is_active) {
				// Reduce the alpha by a bit
				menu_button_sprite.color.alpha = 120;
			}

			menu_config.AddFlag(menu_button_sprite);

			UIWindowDescriptor final_descriptor;
			memcpy(&final_descriptor, window_descriptor, sizeof(final_descriptor));

			// Acts as a wrapper. It automatically checks to see when the window is destroyed
			// if the selection has changed to trigger the calllback
			struct DestroyWindowCallbackData {
				ECS_INLINE void* GetSelectionData() const {
					return OffsetPointer(this, sizeof(*this));
				}

				ECS_INLINE void* GetDestroyData() const {
					return OffsetPointer(GetSelectionData(), handler_data.data_size);
				}

				ECS_INLINE Stream<char> GetTargetWindow() const {
					return { OffsetPointer(GetDestroyData(), destroy_action.data_size), target_window_name_size };
				}

				CapacityStream<char>* selection;
				UIActionHandler handler_data;
				UIActionHandler destroy_action;
				unsigned int target_window_name_size;
			};

			auto destroy_window_callback = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				DestroyWindowCallbackData* data = (DestroyWindowCallbackData*)_data;
				if (data->handler_data.action != nullptr) {
					void* handler_data = data->GetSelectionData();
					action_data->data = handler_data;
					data->handler_data.action(action_data);
				}

				if (data->destroy_action.action != nullptr) {
					action_data->data = data->destroy_action.data;
					data->destroy_action.action(action_data);
				}

				// Trigger a redraw for the target window
				Stream<char> window_name = data->GetTargetWindow();
				system->DeallocateWindowSnapshot(window_name);
			};
			
			Stream<char> current_window_name = system->GetWindowName(window_index);

			size_t _callback_data_storage[256];
			DestroyWindowCallbackData* callback_data = (DestroyWindowCallbackData*)_callback_data_storage;
			callback_data->handler_data = selection_callback;
			callback_data->selection = selection;
			callback_data->target_window_name_size = current_window_name.size;

			unsigned int write_size = sizeof(*callback_data);

			// Now copy the callback data first and then the initial selection
			if (selection_callback.data_size > 0) {
				void* selection_callback_data = OffsetPointer(callback_data, write_size);
				memcpy(selection_callback_data, selection_callback.data, selection_callback.data_size);
				write_size += selection_callback.data_size;
			}

			if (final_descriptor.destroy_action_data_size > 0) {
				void* destroy_buffer = OffsetPointer(callback_data, write_size);
				memcpy(destroy_buffer, final_descriptor.destroy_action_data, final_descriptor.destroy_action_data_size);
				write_size += final_descriptor.destroy_action_data_size;
			}

			// Write the window name
			void* target_window_name_characters = OffsetPointer(callback_data, write_size);
			current_window_name.CopyTo(target_window_name_characters);
			write_size += current_window_name.MemoryOf(current_window_name.size);

			if (final_descriptor.window_name.size == 0) {
				final_descriptor.window_name = "Selection Input Window";
			}

			callback_data->destroy_action = { final_descriptor.destroy_action, final_descriptor.destroy_action_data, (unsigned int)final_descriptor.destroy_action_data_size };
			final_descriptor.destroy_action = destroy_window_callback;
			final_descriptor.destroy_action_data = callback_data;
			final_descriptor.destroy_action_data_size = write_size;

			size_t border_flags = UI_DOCKSPACE_NO_DOCKING | UI_DOCKSPACE_POP_UP_WINDOW | UI_POP_UP_WINDOW_FIT_TO_CONTENT_CENTER;
			MenuButton(SPRITE_CONFIGURATION, menu_config, { nullptr, 0 }, final_descriptor, border_flags);

			if (name.size > 0 && !name_first) {
				ElementName(configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y, config, name, position, scale);
			}
		}

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

		void UIDrawer::SetDrawMode(ECS_UI_DRAWER_MODE mode, unsigned int target, float draw_mode_float) {
			draw_mode = mode;
			draw_mode_count = 0;
			draw_mode_target = target;
			draw_mode_extra_float.x = draw_mode_float;
			if (mode == ECS_UI_DRAWER_COLUMN_DRAW || mode == ECS_UI_DRAWER_COLUMN_DRAW_FIT_SPACE) {
				draw_mode_extra_float.y = current_y;
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetRenderSpan(float2 value)
		{
			min_render_bounds = { 0.0f, 0.0f };
			max_render_bounds = value;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		static void SetWholeWindowHandler(const UIDrawer* drawer, UIHandler* handler, const UIActionHandler* action_handler, UIHandlerCopyBuffers copy_function) {
			// Check for other whole window actions and override them
			// While leaving all other actions after this one
			unsigned int handler_count = handler->GetLastHandlerIndex() + 1;
			int index = (int)handler_count - 1;
			for (; index >= 0; index--) {
				float2 position = handler->GetPositionFromIndex(index);
				float2 scale = handler->GetScaleFromIndex(index);

				if (position == drawer->GetRegionPosition() && scale == drawer->GetRegionScale()) {
					break;
				}
			}
			index++;
			drawer->system->AddHandlerToDockspaceRegionAtIndex(
				handler, 
				drawer->GetRegionPosition(), 
				drawer->GetRegionScale(),
				*action_handler, 
				copy_function, 
				index
			);
		}

		void UIDrawer::SetWindowHoverable(const UIActionHandler* handler, UIHandlerCopyBuffers copy_function) {
			if (!initializer) {
				UIHandler* border_handler = system->GetDockspaceBorderHandler(dockspace, border_index, true, ECS_MOUSE_BUTTON_COUNT, false);
				SetWholeWindowHandler(this, border_handler, handler, copy_function);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetWindowClickable(const UIActionHandler* handler, ECS_MOUSE_BUTTON button_type, UIHandlerCopyBuffers copy_function) {
			if (!initializer) {
				UIHandler* border_handler = system->GetDockspaceBorderHandler(dockspace, border_index, false, button_type, false);
				SetWholeWindowHandler(this, border_handler, handler, copy_function);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetWindowGeneral(const UIActionHandler* handler, UIHandlerCopyBuffers copy_function) {
			if (!initializer) {
				UIHandler* border_handler = system->GetDockspaceBorderHandler(dockspace, border_index, false, ECS_MOUSE_BUTTON_COUNT, true);
				SetWholeWindowHandler(this, border_handler, handler, copy_function);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SetSpriteClusterTexture(size_t configuration, Stream<wchar_t> texture, unsigned int count) {
			if (configuration & UI_CONFIG_LATE_DRAW) {
				system->SetSpriteCluster(dockspace, border_index, texture, count, ECS_UI_DRAW_LATE);
			}
			else if (configuration & UI_CONFIG_SYSTEM_DRAW) {
				system->SetSpriteCluster(dockspace, border_index, texture, count, ECS_UI_DRAW_SYSTEM);
			}
			else {
				system->SetSpriteCluster(dockspace, border_index, texture, count, ECS_UI_DRAW_NORMAL);
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
		void UIDrawer::StateTable(Stream<char> name, Stream<Stream<char>> labels, bool** states) {
			UIDrawConfig config;
			StateTable(0, config, name, labels, states);
		}

		// States should be a stack pointer to a bool array
		void UIDrawer::StateTable(Stream<char> name, Stream<Stream<char>> labels, bool* states) {
			UIDrawConfig config;
			StateTable(0, config, name, labels, states);
		}

		// States should be a stack pointer to bool* to the members that should be changed
		void UIDrawer::StateTable(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels, bool** states) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
					StateTableInitializer(configuration, config, name, labels, position);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		// States should be a stack pointer to a bool array
		void UIDrawer::StateTable(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels, bool* states) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);
			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
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
				if (configuration & UI_CONFIG_DO_CACHE) {
					StateTableInitializer(configuration, config, name, labels, position);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIDrawerStateTable* UIDrawer::StateTableInitializer(size_t configuration, const UIDrawConfig& config, Stream<char> name, Stream<Stream<char>> labels, float2 position) {
			UIDrawerStateTable* data = nullptr;

			// Begin recording allocations and table resources for dynamic resources
			if (~configuration & UI_CONFIG_INITIALIZER_DO_NOT_BEGIN) {
				BeginElement();
				configuration |= UI_CONFIG_INITIALIZER_DO_NOT_BEGIN;
			}
			AddWindowResource(name, [&](Stream<char> identifier) {
				data = GetMainAllocatorBuffer<UIDrawerStateTable>();
				data->labels.Initialize(system->m_memory, labels.size);

				InitializeElementName(configuration, UI_CONFIG_STATE_TABLE_NO_NAME, config, identifier, &data->name, position, { 0.0f, 0.0f });

				data->max_x_scale = 0;
				float max_x_scale = 0.0f;
				for (size_t index = 0; index < labels.size; index++) {
					ConvertTextToWindowResource(configuration, config, labels[index], data->labels.buffer + index, position, { 0.0f, 0.0f });
					if (max_x_scale < data->labels[index].scale.x) {
						max_x_scale = data->labels[index].scale.x;
						data->max_x_scale = index;
					}
				}

				if (configuration & UI_CONFIG_STATE_TABLE_CALLBACK) {
					const UIConfigStateTableCallback* callback = (const UIConfigStateTableCallback*)config.GetParameter(UI_CONFIG_STATE_TABLE_CALLBACK);
					data->clickable_callback = callback->handler;
					if (callback->handler.data_size > 0) {
						data->clickable_callback.data = GetMainAllocatorBuffer(callback->handler.data_size);
						memcpy(data->clickable_callback.data, callback->handler.data, callback->handler.data_size);
					}
				}
				else {
					data->clickable_callback = {};
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
				position = GetCurrentPosition();
			}

			float2 square_scale = GetSquareScale(scale.y);
			scale = { data->labels[data->max_x_scale].scale.x + square_scale.x + element_descriptor.label_padd.x * 2.0f, scale.y * data->labels.size + (data->labels.size - 1) * layout.next_row_y_offset };

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

				if (configuration & UI_CONFIG_STATE_TABLE_CALLBACK) {
					const UIConfigStateTableCallback* callback = (const UIConfigStateTableCallback*)config.GetParameter(UI_CONFIG_STATE_TABLE_CALLBACK);
					if (!callback->copy_on_initialization) {
						if (callback->handler.data_size > 0) {
							memcpy(data->clickable_callback.data, callback->handler.data, callback->handler.data_size);
						}
						else {
							data->clickable_callback.data = callback->handler.data;
						}
						data->clickable_callback.action = callback->handler.action;
					}
				}

				UIDrawerStateTableBoolClickable clickable_data;
				clickable_data.state_table = data;
				if (configuration & UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE) {
					UIConfigStateTableNotify* notify_data = (UIConfigStateTableNotify*)config.GetParameter(UI_CONFIG_STATE_TABLE_NOTIFY_ON_CHANGE);
					clickable_data.notifier = notify_data->notifier;
				}
				else {
					clickable_data.notifier = nullptr;
				}

				struct HoverableData {
					Color theme_color;
					Color checkbox_color;
					Color checkbox_background_color;
					float2 check_box_position;
					float2 check_box_scale;
				};

				auto hoverable_action = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					HoverableData* data = (HoverableData*)_data;

					// Draw first the entire background
					UIDefaultHoverableData entire_hoverable;
					entire_hoverable.colors[0] = data->theme_color;
					entire_hoverable.percentages[0] = 1.0f;
					action_data->data = &entire_hoverable;
					DefaultHoverableAction(action_data);

					UIDefaultHoverableData check_box_hoverable;
					check_box_hoverable.colors[0] = data->checkbox_background_color;
					check_box_hoverable.positions[0] = data->check_box_position;
					check_box_hoverable.scales[0] = data->check_box_scale;
					check_box_hoverable.count = 1;
					check_box_hoverable.is_single_action_parameter_draw = false;
					action_data->data = &check_box_hoverable;
					DefaultHoverableAction(action_data);

					SetSpriteRectangle(
						data->check_box_position,
						data->check_box_scale,
						data->checkbox_color,
						{ 0.0f, 0.0f },
						{ 1.0f, 1.0f },
						buffers,
						counts,
						ECS_TOOLS_UI_SPRITE
					);
				};

				auto fill_hoverable_data = [&]() {
					HoverableData hoverable_data;
					hoverable_data.theme_color = color_theme.theme;
					hoverable_data.checkbox_color = checkbox_color;
					hoverable_data.checkbox_background_color = background_color;
					hoverable_data.check_box_position = current_position;
					hoverable_data.check_box_scale = square_scale;
					return hoverable_data;
				};

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
						SolidColorRectangle(UI_CONFIG_LATE_DRAW, current_position, square_scale, background_color);
						SpriteRectangle(UI_CONFIG_LATE_DRAW, current_position, square_scale, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, checkbox_color);
					}

					HoverableData hoverable_data = fill_hoverable_data();
					AddDefaultClickable(
						configuration, 
						current_position, 
						{ scale.x, square_scale.y },
						{ hoverable_action, &hoverable_data, sizeof(hoverable_data), ECS_UI_DRAW_LATE },
						{ StateTableBoolClickable, &clickable_data, sizeof(clickable_data) }
					);

					current_position.x += square_scale.x + element_descriptor.label_padd.x;
					Text(configuration | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_ALIGN_TO_ROW_Y
						| UI_CONFIG_LATE_DRAW, config, data->labels.buffer + index, current_position);
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
						SolidColorRectangle(UI_CONFIG_LATE_DRAW, current_position, square_scale, background_color);
						SpriteRectangle(UI_CONFIG_LATE_DRAW, current_position, square_scale, ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK, checkbox_color);
					}

					UIDrawerStateTableAllButtonData button_data;
					button_data.all_true = all_true;
					button_data.single_pointer = false;
					button_data.state_table = data;
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

					HoverableData hoverable_data = fill_hoverable_data();
					AddDefaultClickable(
						configuration, 
						current_position, 
						{ scale.x, square_scale.y },
						{ hoverable_action, &hoverable_data, sizeof(hoverable_data), ECS_UI_DRAW_LATE },
						{ StateTableAllButtonAction, &button_data, sizeof(button_data) }
					);

					current_position.x += square_scale.x + element_descriptor.label_padd.x;
					Text(configuration | UI_CONFIG_DO_NOT_FIT_SPACE | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_ALIGN_TO_ROW_Y
						| UI_CONFIG_LATE_DRAW, config, "All", current_position);
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

		void UIDrawer::SpriteRectangle(Stream<wchar_t> texture, Color color, float2 top_left_uv, float2 bottom_right_uv) {
			UIDrawConfig config;
			SpriteRectangle(0, config, texture, color, top_left_uv, bottom_right_uv);
		}

		void UIDrawer::SpriteRectangle(
			size_t configuration,
			const UIDrawConfig& config,
			Stream<wchar_t> texture,
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
			Stream<wchar_t> texture0,
			Stream<wchar_t> texture1,
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
			Stream<wchar_t> texture0,
			Stream<wchar_t> texture1,
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

		template<typename SpriteDraw>
		void SpriteButtonDraw(
			UIDrawer* drawer,
			size_t configuration,
			const UIDrawConfig& config,
			UIActionHandler clickable,
			Color color,
			SpriteDraw&& draw
		) {
			float2 position; 
			float2 scale; 
			drawer->HandleTransformFlags(configuration, config, position, scale);
			
			if (!drawer->initializer) {
				bool is_active = true;
				if (configuration & UI_CONFIG_ACTIVE_STATE) {
					const UIConfigActiveState* active_state = (const UIConfigActiveState*)config.GetParameter(UI_CONFIG_ACTIVE_STATE);
					is_active = active_state->state;
				}

				drawer->HandleFitSpaceRectangle(configuration, position, scale);

				float2 background_position;
				float2 background_scale;
				Color background_color;
				if (configuration & UI_CONFIG_SPRITE_BUTTON_BACKGROUND) {
					const UIConfigSpriteButtonBackground* background = (const UIConfigSpriteButtonBackground*)config.GetParameter(UI_CONFIG_SPRITE_BUTTON_BACKGROUND);
					background_color = drawer->HandleColor(configuration, config);
					if (background->overwrite_color != ECS_COLOR_BLACK) {
						background_color = background->overwrite_color;
					}

					background_scale = scale * background->scale_factor;
					background_position = { position.x + (scale.x - background_scale.x) * 0.5f, position.y + (scale.y - background_scale.y) * 0.5f };

					if (configuration & UI_CONFIG_SPRITE_BUTTON_CENTER_SPRITE_TO_BACKGROUND) {
						background_position = position;
						position = AlignMiddle(background_position, background_scale, scale);
					}

					drawer->SpriteRectangle(configuration, background_position, background_scale, ECS_TOOLS_UI_TEXTURE_MASK, background_color);
				}

				AlignToRowY(drawer, configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (drawer->ValidatePosition(configuration, position, scale)) {
					const UIConfigSpriteButtonMultiTexture* multi_texture = nullptr;

					float color_alpha_factor = is_active ? 1.0f : drawer->color_theme.alpha_inactive_item;
					color.alpha *= color_alpha_factor;

					auto draw_multi_texture = [&]() {
						if (multi_texture->textures.size > 0) {
							for (size_t index = 0; index < multi_texture->textures.size; index++) {
								Color current_color = ECS_COLOR_WHITE;
								if (multi_texture->texture_colors != nullptr) {
									current_color = multi_texture->texture_colors[index];
								}
								current_color.alpha *= color_alpha_factor;

								drawer->SpriteRectangle(
									configuration,
									position,
									scale,
									multi_texture->textures[index],
									current_color
								);
							}
						}

						if (multi_texture->resource_views.size > 0) {
							for (size_t index = 0; index < multi_texture->resource_views.size; index++) {
								Color current_color = ECS_COLOR_WHITE;
								if (multi_texture->resource_view_colors != nullptr) {
									current_color = multi_texture->resource_view_colors[index];
								}
								current_color.alpha *= color_alpha_factor;

								drawer->SpriteRectangle(
									configuration,
									position,
									scale,
									multi_texture->resource_views[index],
									current_color
								);
							}
						}
					};

					if (configuration & UI_CONFIG_SPRITE_BUTTON_MULTI_TEXTURE) {
						multi_texture = (const UIConfigSpriteButtonMultiTexture*)config.GetParameter(UI_CONFIG_SPRITE_BUTTON_MULTI_TEXTURE);
						if (multi_texture->draw_before_main_texture) {
							draw_multi_texture();
						}
					}

					if (is_active) {
						draw(position, scale, color);

						drawer->HandleBorder(configuration, config, position, scale);
						drawer->HandleLateAndSystemDrawActionNullify(configuration, position, scale);

						drawer->AddDefaultClickableHoverable(configuration, position, scale, clickable);
					}
					else {
						draw(position, scale, color);
						drawer->HandleBorder(configuration, config, position, scale);
					}

					if (multi_texture != nullptr && !multi_texture->draw_before_main_texture) {
						draw_multi_texture();
					}
				}

				drawer->FinalizeRectangle(configuration, position, scale);
			}
		}

		void UIDrawer::SpriteButton(
			UIActionHandler clickable,
			Stream<wchar_t> texture,
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
			Stream<wchar_t> texture,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			SpriteButtonDraw(this, configuration, config, clickable, color, [&](float2 position, float2 scale, Color color) {
				SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);
				});
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::SpriteButton(
			UIActionHandler clickable,
			ResourceView texture,
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
			ResourceView texture,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv
		) {
			SpriteButtonDraw(this, configuration, config, clickable, color, [&](float2 position, float2 scale, Color color) {
				SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);
			});
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Sprite Texture Bool

		void UIDrawer::SpriteTextureBool(
			Stream<wchar_t> texture_false,
			Stream<wchar_t> texture_true,
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
			Stream<wchar_t> texture_false,
			Stream<wchar_t> texture_true,
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

					background_scale = scale * background->scale_factor;
					background_position = { position.x + (scale.x - background_scale.x) * 0.5f, position.y + (scale.y - background_scale.y) * 0.5f };

					if (configuration & UI_CONFIG_SPRITE_BUTTON_CENTER_SPRITE_TO_BACKGROUND) {
						background_position = position;
						position = AlignMiddle(background_position, background_scale, scale);
					}
				}

				AlignToRowY(this, configuration, position, scale);

				if (configuration & UI_CONFIG_GET_TRANSFORM) {
					UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
					*get_transform->position = position;
					*get_transform->scale = scale;
				}

				if (ValidatePosition(configuration, position, scale)) {
					Stream<wchar_t> texture = *state ? texture_true : texture_false;
					if (is_active) {
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);

						HandleBorder(configuration, config, position, scale);
						HandleLateAndSystemDrawActionNullify(configuration, position, scale);

						AddDefaultClickableHoverable(configuration, position, scale, { BoolClickable, state, 0 });
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
			Stream<wchar_t> texture_false,
			Stream<wchar_t> texture_true,
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
			Stream<wchar_t> texture_false,
			Stream<wchar_t> texture_true,
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

						background_scale = background->scale_factor * scale;
						background_position = { position.x + (scale.x - background_scale.x) * 0.5f, position.y + (scale.y - background_scale.y) * 0.5f };

						if (configuration & UI_CONFIG_SPRITE_BUTTON_CENTER_SPRITE_TO_BACKGROUND) {
							background_position = position;
							position = AlignMiddle(background_position, background_scale, scale);
						}
					}

					AlignToRowY(this, configuration, position, scale);

					if (configuration & UI_CONFIG_GET_TRANSFORM) {
						UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
						*get_transform->position = position;
						*get_transform->scale = scale;
					}


					Stream<wchar_t> texture = HasFlag(*flags, flag_to_set) ? texture_true : texture_false;
					if (is_active) {
						SpriteRectangle(configuration, position, scale, texture, color, top_left_uv, bottom_right_uv);

						HandleBorder(configuration, config, position, scale);
						HandleLateAndSystemDrawActionNullify(configuration, position, scale);

						UIChangeStateData change_state;
						change_state.state = flags;
						change_state.flag = flag_to_set;
						AddDefaultClickableHoverable(configuration, position, scale, { ChangeStateAction, &change_state, sizeof(change_state) });
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

		void UIDrawer::TextTable(Stream<char> name, unsigned int rows, unsigned int columns, Stream<char>* labels) {
			UIDrawConfig config;
			TextTable(0, config, name, rows, columns, labels);
		}

		void UIDrawer::TextTable(size_t configuration, const UIDrawConfig& config, Stream<char> name, unsigned int rows, unsigned int columns, Stream<char>* labels) {
			if (!initializer) {
				ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

				if (configuration & UI_CONFIG_DO_CACHE) {
					UIDrawerTextTable* data = (UIDrawerTextTable*)GetResource(name);
					TextTableDrawer(configuration, config, position, scale, data, rows, columns);
				}
				else {
					TextTableDrawer(configuration, config, position, scale, rows, columns, labels);
				}
			}
			else {
				if (configuration & UI_CONFIG_DO_CACHE) {
					TextTableInitializer(configuration, config, name, rows, columns, labels);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Text Input

		// ------------------------------------------------------------------------------------------------------------------------------------

		// single lined text input
		UIDrawerTextInput* UIDrawer::TextInput(Stream<char> name, CapacityStream<char>* text_to_fill) {
			UIDrawConfig config;
			return TextInput(0, config, name, text_to_fill);
		}

		// single lined text input
		UIDrawerTextInput* UIDrawer::TextInput(size_t configuration, UIDrawConfig& config, Stream<char> name, CapacityStream<char>* text_to_fill, UIDrawerTextInputFilter filter) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
					UIDrawerTextInput* input = (UIDrawerTextInput*)GetResource(name);
					input->text = text_to_fill;

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
				if (configuration & UI_CONFIG_DO_CACHE) {
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

		void UIDrawer::TextLabel(Stream<char> characters) {
			UIDrawConfig config;
			TextLabel(0, config, characters);
		}

		void UIDrawer::TextLabel(size_t configuration, const UIDrawConfig& config, Stream<char> characters) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
					UIDrawerTextElement* element = (UIDrawerTextElement*)GetResource(characters);
					HandleFitSpaceRectangle(configuration, position, scale);
					TextLabelDrawer(configuration, config, element, position, scale);
				}
				else {
					Stream<char> identifier = HandleResourceIdentifier(characters);
					if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
						GetElementAlignedPosition(configuration, config, position, scale);

						TextLabelWithCull(configuration, config, identifier, position, scale);

						if (configuration & UI_CONFIG_GET_TRANSFORM) {
							UIConfigGetTransform* get_transform = (UIConfigGetTransform*)config.GetParameter(UI_CONFIG_GET_TRANSFORM);
							*get_transform->position = position;
							*get_transform->scale = scale;
						}
					}
					else {
						TextLabel(configuration, config, identifier, position, scale);
					}
				}
			}
			else if (configuration & UI_CONFIG_DO_CACHE) {
				TextLabelInitializer(configuration, config, characters, position, scale);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::TextLabelWide(Stream<wchar_t> characters)
		{
			UIDrawConfig config;
			TextLabelWide(0, config, characters);
		}

		void UIDrawer::TextLabelWide(size_t configuration, const UIDrawConfig& config, Stream<wchar_t> characters)
		{
			ECS_ASSERT(characters.size < ECS_KB * 8);
			ECS_STACK_CAPACITY_STREAM(char, ascii, ECS_KB * 8);
			ConvertWideCharsToASCII(characters, ascii);
			TextLabel(configuration, config, ascii);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Text

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Text(Stream<char> text) {
			UIDrawConfig config;
			Text(0, config, text);
		}

		// non cached drawer
		void UIDrawer::Text(size_t configuration, const UIDrawConfig& config, Stream<char> characters, float2 position) {
			float2 text_span;

			characters.size = ParseStringIdentifier(characters);
			Color color;
			float2 font_size;
			float character_spacing;
			HandleText(configuration, config, color, font_size, character_spacing);
			auto text_sprites = HandleTextSpriteBuffer(configuration);
			auto text_sprite_count = HandleTextSpriteCount(configuration);

			Stream<UISpriteVertex> vertices = Stream<UISpriteVertex>(text_sprites + *text_sprite_count, characters.size * 6);
			ECS_UI_ALIGN horizontal_alignment, vertical_alignment;
			GetTextLabelAlignment(configuration, config, horizontal_alignment, vertical_alignment);

			float text_y_scale = system->GetTextSpriteYScale(font_size.y);
			if (configuration & UI_CONFIG_ALIGN_TO_ROW_Y) {
				float row_scale = current_row_y_scale < text_y_scale ? text_y_scale : current_row_y_scale;
				position.y = AlignMiddle(position.y, row_scale, text_y_scale);
			}

			ECS_UI_ALIGN text_horizontal_alignment, text_vertical_alignment;
			GetElementAlignment(configuration, config, text_horizontal_alignment, text_vertical_alignment);
			if (text_horizontal_alignment != ECS_UI_ALIGN_LEFT || text_vertical_alignment != ECS_UI_ALIGN_TOP) {
				// Get the text span
				text_span = TextSpan(characters, font_size, character_spacing);
				GetElementAlignedPosition(configuration, config, position, text_span);
			}

			size_t before_count = *text_sprite_count;
			bool did_draw = true;
			if (ValidatePositionY(configuration, position, { 0.0f, text_y_scale })) {
				if (configuration & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					bool invert_order = (vertical_alignment == ECS_UI_ALIGN_BOTTOM) || (horizontal_alignment == ECS_UI_ALIGN_RIGHT);
					if (configuration & UI_CONFIG_VERTICAL) {
						system->ConvertCharactersToTextSprites(
							characters,
							position,
							text_sprites,
							color,
							*text_sprite_count,
							font_size,
							character_spacing,
							false,
							invert_order
						);
						Stream<UISpriteVertex> current_text = GetTextStream(configuration, characters.size * 6);
						text_span = GetTextSpan(current_text, false, invert_order);
						AlignVerticalText(current_text);
					}
					else {
						system->ConvertCharactersToTextSprites(
							characters,
							position,
							text_sprites,
							color,
							*text_sprite_count,
							font_size,
							character_spacing,
							true,
							invert_order
						);
						text_span = GetTextSpan(GetTextStream(configuration, characters.size * 6), true, invert_order);
					}
				}
				else {
					if (configuration & UI_CONFIG_VERTICAL) {
						system->ConvertCharactersToTextSprites(
							characters,
							position,
							text_sprites,
							color,
							*text_sprite_count,
							font_size,
							character_spacing,
							false
						);
						Stream<UISpriteVertex> current_text = GetTextStream(configuration, characters.size * 6);
						text_span = GetTextSpan(current_text, false);
						AlignVerticalText(current_text);
					}
					else {
						system->ConvertCharactersToTextSprites(
							characters,
							position,
							text_sprites,
							color,
							*text_sprite_count,
							font_size,
							character_spacing
						);
						text_span = GetTextSpan(GetTextStream(configuration, characters.size * 6));
					}
				}

				*text_sprite_count += characters.size * 6;
			}
			else {
				text_span = TextSpan(characters, font_size, character_spacing);
				did_draw = false;
			}

			if (~configuration & UI_CONFIG_DO_NOT_FIT_SPACE) {
				float2 copy = position;
				bool is_moved = HandleFitSpaceRectangle(configuration, position, text_span);
				if (is_moved && did_draw) {
					float x_translation = position.x - copy.x;
					float y_translation = position.y - copy.y;
					for (size_t index = before_count; index < *text_sprite_count; index++) {
						text_sprites[index].position.x += x_translation;
						text_sprites[index].position.y -= y_translation;
					}
				}
			}

			FinalizeRectangle(configuration, position, text_span);
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Text(size_t configuration, const UIDrawConfig& config, Stream<char> text, float2& position, float2& text_span) {
			Color color;
			float character_spacing;
			float2 font_size;

			HandleText(configuration, config, color, font_size, character_spacing);
			if (configuration & UI_CONFIG_DO_CACHE) {
				UIDrawerTextElement* info = HandleTextCopyFromResource(configuration, text, position, color, ECS_TOOLS_UI_DRAWER_IDENTITY_SCALE);
				text_span = *info->TextScale();

				text_span.x *= zoom_ptr->x * info->GetInverseZoomX();
				text_span.y *= zoom_ptr->y * info->GetInverseZoomY();			
			}
			else {
				if (!initializer) {
					float text_y_span = system->GetTextSpriteYScale(font_size.y);
					// Preemptively check to see if it actually needs to be drawn on the y axis by setting the horizontal 
					// draw to maximum
					if (ValidatePositionY(configuration, position, { 100.0f, text_y_span })) {
						size_t text_count = ParseStringIdentifier(text);

						auto text_stream = GetTextStream(configuration, text_count * 6);
						auto text_sprites = HandleTextSpriteBuffer(configuration);
						auto text_sprite_count = HandleTextSpriteCount(configuration);

						bool horizontal = (configuration & UI_CONFIG_VERTICAL) == 0;
						system->ConvertCharactersToTextSprites(
							{ text.buffer, text_count },
							position,
							text_sprites,
							color,
							*text_sprite_count,
							font_size,
							character_spacing,
							horizontal
						);
						text_span = GetTextSpan(text_stream, horizontal);
						*text_sprite_count += 6 * text_count;

						bool is_moved = HandleFitSpaceRectangle(configuration, position, text_span);
						if (is_moved) {
							TranslateText(position.x, position.y, text_stream);
						}

						if (configuration & UI_CONFIG_ALIGN_TO_ROW_Y) {
							float y_position = AlignMiddle(position.y, current_row_y_scale, text_span.y);
							TranslateTextY(y_position, 0, text_stream);
						}

						if (!ValidatePosition(configuration, { text_stream[0].position.x, -text_stream[0].position.y }, text_span)) {
							*text_sprite_count -= 6 * text_count;
						}
					}
					// Still have to fill in the text span
					else {
						text_span = TextSpan({ text.buffer, ParseStringIdentifier(text) }, font_size, character_spacing);
						HandleFitSpaceRectangle(configuration, position, text_span);
						FinalizeRectangle(configuration, position, text_span);
					}
				}
				// Still have to fill in the text span
				else {
					text_span = TextSpan({ text.buffer, ParseStringIdentifier(text) }, font_size, character_spacing);
					HandleFitSpaceRectangle(configuration, position, text_span);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::Text(size_t configuration, const UIDrawConfig& config, Stream<char> characters) {
			ECS_TOOLS_UI_DRAWER_HANDLE_TRANSFORM(configuration, config);

			if (!initializer) {
				if (configuration & UI_CONFIG_DO_CACHE) {
					UIDrawerTextElement* resource = (UIDrawerTextElement*)GetResource(characters);
					Text(configuration, config, resource, position);
				}
				else {
					Text(configuration, config, characters, position);
				}
			}
			else if (configuration & UI_CONFIG_DO_CACHE) {
				TextInitializer(configuration, config, characters, position);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region TextWide

		void UIDrawer::TextWide(Stream<wchar_t> characters) {
			UIDrawConfig config;
			TextWide(0, config, characters);
		}

		void UIDrawer::TextWide(size_t configuration, const UIDrawConfig& config, Stream<wchar_t> characters) {
			const size_t MAX_CHARACTERS = ECS_KB * 8;
			ECS_ASSERT(characters.size < MAX_CHARACTERS);

			ECS_STACK_CAPACITY_STREAM(char, ascii, MAX_CHARACTERS);
			ConvertWideCharsToASCII(characters, ascii);
			Text(configuration, config, ascii);
		}

#pragma endregion

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::TextToolTip(Stream<char> characters, float2 position, float2 scale, bool stable_characters, const UITooltipBaseData* base) {
			if (record_actions) {
				size_t tool_tip_data_storage[512];
				UITextTooltipHoverableData* tool_tip_data = (UITextTooltipHoverableData*)tool_tip_data_storage;
				unsigned int write_size = sizeof(*tool_tip_data);
				if (base != nullptr) {
					tool_tip_data->base = *base;
				}
				else {
					tool_tip_data->base = UITooltipBaseData();
					tool_tip_data->base.offset.y = 0.01f;
					tool_tip_data->base.offset_scale.y = true;
					tool_tip_data->base.center_horizontal_x = true;
				}

				if (stable_characters) {
					tool_tip_data->characters = characters;
				}
				else {
					write_size = tool_tip_data->Write(characters);
				}
				ECS_ASSERT(write_size <= sizeof(tool_tip_data_storage));

				system->ComposeHoverable(dockspace, border_index, position, scale, { TextTooltipHoverable, tool_tip_data, write_size, ECS_UI_DRAW_SYSTEM }, true);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIActionHandler UIDrawer::TextToolTipHandler(
			Stream<char> characters, 
			const UITextTooltipHoverableData* data, 
			bool stable_characters, 
			ECS_UI_DRAW_PHASE phase
		)
		{
			if (record_actions) {
				size_t total_storage[256];
				unsigned int write_size = sizeof(*data);
				if (!stable_characters) {
					UITextTooltipHoverableData* stack_data = (UITextTooltipHoverableData*)total_storage;
					memcpy(stack_data, data, sizeof(*data));
					write_size = stack_data->Write(data->characters);
					data = stack_data;
				}
				return { TextTooltipHoverable, (void*)data, write_size, phase };
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::HandleTextToolTip(size_t configuration, const UIDrawConfig& config, float2 position, float2 scale)
		{
			if (configuration & UI_CONFIG_TOOL_TIP) {
				const UIConfigToolTip* tool_tip = (const UIConfigToolTip*)config.GetParameter(UI_CONFIG_TOOL_TIP);
				TextToolTip(tool_tip->characters, position, scale, tool_tip->is_stable);
			}
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		UIConfigTextParameters UIDrawer::TextParameters() const
		{
			UIConfigTextParameters parameters;
			parameters.character_spacing = font.character_spacing;
			parameters.size = GetFontSize();
			parameters.color = color_theme.text;
			return parameters;
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DefaultHoverableWithToolTip(
			size_t configuration, 
			Stream<char> characters, 
			float2 position, 
			float2 scale, 
			const Color* color, 
			const float* percentage, 
			const UITooltipBaseData* base
		) {
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

			AddHoverable(configuration, position, scale, { DefaultHoverableWithToolTipAction, &tool_tip_data, sizeof(tool_tip_data), ECS_UI_DRAW_SYSTEM });
		}

		// ------------------------------------------------------------------------------------------------------------------------------------

		void UIDrawer::DefaultHoverableWithToolTip(size_t configuration, float2 position, float2 scale, const UIDefaultHoverableWithTooltipData* data) {
			AddHoverable(configuration, position, scale, { DefaultHoverableWithToolTipAction, (void*)data, sizeof(*data), ECS_UI_DRAW_SYSTEM });
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
			Stream<wchar_t> texture,
			const Color* colors,
			const float2* uvs,
			ECS_UI_DRAW_PHASE phase
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
			Stream<wchar_t> texture,
			const Color* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			ECS_UI_DRAW_PHASE phase
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
			Stream<wchar_t> texture,
			const ColorFloat* colors,
			const float2* uvs,
			ECS_UI_DRAW_PHASE phase
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
			Stream<wchar_t> texture,
			const ColorFloat* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			ECS_UI_DRAW_PHASE phase
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

		void UIDrawerArrayAddAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerArrayAddRemoveData* data = (UIDrawerArrayAddRemoveData*)_data;
			unsigned int old_size = data->capacity_data->size;
			if (data->is_resizable_data) {
				data->resizable_data->Resize(data->new_size, data->element_byte_size);
				data->resizable_data->size = data->new_size;
			}
			else {
				data->capacity_data->size = data->new_size;
			}
			data->array_data->should_redraw = true;

			if (data->array_data->add_callback != nullptr) {
				data->new_size = old_size;
				action_data->data = data->array_data->add_callback_data;
				action_data->additional_data = data;
				data->array_data->add_callback(action_data);
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayRemoveAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerArrayAddRemoveData* data = (UIDrawerArrayAddRemoveData*)_data;
			
			if (data->array_data->remove_callback != nullptr) {
				action_data->data = data->array_data->remove_callback_data;
				action_data->additional_data = data;
				data->array_data->remove_callback(action_data);
			}

			if (data->is_resizable_data) {
				data->resizable_data->Resize(data->new_size, data->element_byte_size);
				data->resizable_data->size = data->new_size;
			}
			else {
				data->capacity_data->size = data->new_size;
			}
			data->array_data->should_redraw = true;
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayIntInputAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerArrayAddRemoveData* data = (UIDrawerArrayAddRemoveData*)_data;
			if (data->is_resizable_data) {
				// Determine if the count is greater - call the add function
				// new size is the actual current size - and the changed value is
				// reflected inside the size field
				if (data->resizable_data->size > data->new_size) {
					// Swap them
					unsigned int temp = data->resizable_data->size;
					data->resizable_data->size = data->new_size;
					data->new_size = temp;

					// Call the add function
					UIDrawerArrayAddAction(action_data);
				}
				else if (data->resizable_data->size < data->new_size) {
					// Swap them
					unsigned int temp = data->resizable_data->size;
					data->resizable_data->size = data->new_size;
					data->new_size = temp;

					// Call the remove function
					UIDrawerArrayRemoveAction(action_data);
				}
				// It can happen that the add/remove buttons modify the value and this fires later on
				// after the callbacks were called. So check that the size is smaller or greater
			}
			// For the capacity stream nothing needs to be done 
			// Only the change in the size is sufficient
			// But the callbacks should still be called
			else {
				// Verify that the new value is in bounds
				data->new_size = ClampMax(data->capacity_data->size, data->capacity_data->capacity);

				// Elements were removed
				if (data->new_size > data->capacity_data->size) {
					// Check to see if the remove callback is set
					if (data->array_data->remove_callback != nullptr) {
						action_data->data = data->array_data->remove_callback_data;
						action_data->additional_data = data;
						data->array_data->remove_callback(action_data);
					}
				}
				// Elements were added
				else if (data->new_size < data->capacity_data->size) {
					// Check to see if the add callback is set
					if (data->array_data->add_callback != nullptr) {
						action_data->data = data->array_data->add_callback_data;
						action_data->additional_data = data;
						data->array_data->add_callback(action_data);
					}
				}

				// It can happen that the add/remove buttons modify the value and this fires later on
				// after the callbacks were called. So check that the size is smaller or greater
				data->capacity_data->size = data->new_size;
				data->array_data->should_redraw = true;
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayRemoveAnywhereAction(ActionData* action_data)
		{
			UI_UNPACK_ACTION_DATA;

			UIDrawerArrayAddRemoveData* data = (UIDrawerArrayAddRemoveData*)_data;
			// Call firstly the user callback
			action_data->data = data->array_data->remove_anywhere_callback_data;
			action_data->additional_data = &data->array_data->remove_anywhere_index;
			data->array_data->remove_anywhere_callback(action_data);

			// Remove swap back the data
			data->capacity_data->Remove(data->array_data->remove_anywhere_index, data->element_byte_size);
			
			// Disable the remove callback
			Action remove_callback = data->array_data->remove_callback;
			data->array_data->remove_callback = nullptr;

			// Now call the remove action
			action_data->data = data;
			UIDrawerArrayRemoveAction(action_data);

			data->array_data->remove_callback = remove_callback;
			data->array_data->remove_anywhere_index = -1;
		}

		// --------------------------------------------------------------------------------------------------------------

		// Last initialized element allocations and table resources must be manually deallocated after finishing using it
		UIDrawer InitializeInitializerDrawer(UIDrawer& drawer_to_copy) {
			UIDrawerDescriptor descriptor = drawer_to_copy.system->GetDrawerDescriptor(drawer_to_copy.window_index);
			descriptor.do_not_initialize_viewport_sliders = true;
			descriptor.do_not_allocate_buffers = true;
			descriptor.record_handlers = false;
			descriptor.record_snapshot_runnables = false;

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
			Stream<char> name,
			UIDrawerInitializeFunction initialize,
			size_t configuration
		) {
			UIDrawer drawer = InitializeInitializerDrawer(drawer_to_copy);

			Stream<char> identifier = drawer.HandleResourceIdentifier(name);
			initialize(drawer.window_data, additional_data, &drawer, configuration);
			drawer.system->AddWindowDynamicElement(drawer.window_index, identifier, drawer.last_initialized_element_allocations, drawer.last_initialized_element_table_resources);

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

		void InitializeFilesystemHierarchyElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeFilesystemHierarchy* data = (UIDrawerInitializeFilesystemHierarchy*)additional_data;
			drawer_ptr->FilesystemHierarchy(configuration, *data->config, data->identifier, data->labels);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeLabelHierarchyElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration) {
			UIDrawerInitializeLabelHierarchy* data = (UIDrawerInitializeLabelHierarchy*)additional_data;
			drawer_ptr->LabelHierarchyInitializer(configuration, *data->config, data->name, data->storage_type_size);
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

		void InitializeFileInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration)
		{
			UIDrawerInitializePathInput* data = (UIDrawerInitializePathInput*)additional_data;
			drawer_ptr->FileInput(configuration, *data->config, data->name, data->capacity_characters, data->extensions);
		}

		// --------------------------------------------------------------------------------------------------------------

		void InitializeDirectoryInputElement(void* window_data, void* additional_data, UIDrawer* drawer_ptr, size_t configuration)
		{
			UIDrawerInitializePathInput* data = (UIDrawerInitializePathInput*)additional_data;
			drawer_ptr->DirectoryInput(configuration, *data->config, data->name, data->capacity_characters);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayFloatFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.FloatInput(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE), 
				draw_data.config == nullptr ? temp_config : *draw_data.config, 
				element_name, 
				(float*)draw_data.element_data
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayDoubleFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.DoubleInput(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE), 
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				element_name, 
				(double*)draw_data.element_data
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		void UIDrawerArrayIntegerFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.IntInput<Integer>(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE), 
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				element_name,
				(Integer*)draw_data.element_data
			);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawerArrayIntegerFunction<integer>, UIDrawer&, Stream<char>, UIDrawerArrayDrawData);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayFloat2Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			float* values[2];
			values[0] = (float*)draw_data.element_data;
			values[1] = values[0] + 1;

			drawer.FloatInputGroup(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				2,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayDouble2Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			double* values[2];
			values[0] = (double*)draw_data.element_data;
			values[1] = values[0] + 1;

			drawer.DoubleInputGroup(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				2,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		void UIDrawerArrayInteger2Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			BaseInteger* values[2];
			values[0] = (BaseInteger*)draw_data.element_data;
			values[1] = values[0] + 1;

			drawer.IntInputGroup(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				2,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawerArrayInteger2Function<integer>, UIDrawer&, Stream<char>, UIDrawerArrayDrawData);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayFloat3Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			float* values[3];
			values[0] = (float*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;

			drawer.FloatInputGroup(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				3,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayDouble3Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			double* values[3];
			values[0] = (double*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;

			drawer.DoubleInputGroup(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				3,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		void UIDrawerArrayInteger3Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			BaseInteger* values[3];
			values[0] = (BaseInteger*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;

			drawer.IntInputGroup(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				3,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawerArrayInteger3Function<integer>, UIDrawer&, Stream<char>, UIDrawerArrayDrawData);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayFloat4Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			float* values[4];
			values[0] = (float*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;
			values[3] = values[0] + 3;

			drawer.FloatInputGroup(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				4,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayDouble4Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			double* values[4];
			values[0] = (double*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;
			values[3] = values[0] + 3;

			drawer.DoubleInputGroup(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				4,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		template<typename BaseInteger>
		void UIDrawerArrayInteger4Function(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			BaseInteger* values[4];
			values[0] = (BaseInteger*)draw_data.element_data;
			values[1] = values[0] + 1;
			values[2] = values[0] + 2;
			values[3] = values[0] + 3;

			drawer.IntInputGroup(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				4,
				element_name,
				UI_DRAWER_ARRAY_SUBTYPE_COMPONENTS,
				values
			);
		}

#define EXPORT(integer) ECS_TEMPLATE_FUNCTION(void, UIDrawerArrayInteger4Function<integer>, UIDrawer&, Stream<char>, UIDrawerArrayDrawData);

		ECS_TEMPLATE_FUNCTION_INTEGER(EXPORT);

#undef EXPORT

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayColorFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.ColorInput(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				element_name, 
				(Color*)draw_data.element_data
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayColorFloatFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.ColorFloatInput(
				draw_data.configuration, 
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				element_name, 
				(ColorFloat*)draw_data.element_data
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayCheckBoxFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			drawer.CheckBox(
				draw_data.configuration, 
				draw_data.config == nullptr ? temp_config : *draw_data.config, 
				element_name, 
				(bool*)draw_data.element_data
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayTextInputFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			UIDrawerTextInput** inputs = (UIDrawerTextInput**)draw_data.additional_data;

			inputs[draw_data.current_index] = drawer.TextInput(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE), 
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				element_name,
				(CapacityStream<char>*)draw_data.element_data
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayComboBoxFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data) {
			UIDrawConfig temp_config;
			CapacityStream<Stream<Stream<char>>>* flag_labels = (CapacityStream<Stream<Stream<char>>>*)draw_data.additional_data;

			drawer.ComboBox(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				element_name,
				flag_labels->buffer[draw_data.current_index],
				flag_labels->buffer[draw_data.current_index].size,
				(unsigned char*)draw_data.element_data
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayDirectoryInputFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data)
		{
			UIDrawConfig temp_config;
			drawer.DirectoryInput(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				element_name,
				(CapacityStream<wchar_t>*)draw_data.element_data
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerArrayFileInputFunction(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData draw_data)
		{
			UIDrawConfig temp_config;

			drawer.FileInput(
				ClearFlag(draw_data.configuration, UI_CONFIG_DO_CACHE),
				draw_data.config == nullptr ? temp_config : *draw_data.config,
				element_name,
				(CapacityStream<wchar_t>*)draw_data.element_data,
				*(Stream<Stream<wchar_t>>*)draw_data.additional_data
			);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::AddElement(size_t transform_type, float2 parameters, ECS_UI_ALIGN alignment)
		{
			if (current_index > 0) {
				indentations[current_index] = indentation;
			}

			if (transform_type & UI_CONFIG_ABSOLUTE_TRANSFORM) {
				if (parameters.y == 0.0f) {
					parameters.y = row_scale.y;
				}
				element_sizes[current_index] = parameters;
				element_transform_types[current_index] = transform_type;
				UpdateRowYScale(parameters.y);
			}
			else if (transform_type & UI_CONFIG_RELATIVE_TRANSFORM) {
				if (parameters.y == 0.0f) {
					parameters.y = row_scale.y / drawer->layout.default_element_y;
				}
				element_sizes[current_index] = parameters;
				element_transform_types[current_index] = transform_type;
				float expanded_scale = parameters.y * drawer->layout.default_element_y;
				UpdateRowYScale(expanded_scale);
			}
			else if (transform_type & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				element_transform_types[current_index] = transform_type;
				if (parameters.y == 0.0f) {
					parameters.y = row_scale.y / drawer->layout.default_element_y;
				}
				element_sizes[current_index].y = parameters.y * drawer->layout.default_element_y;
			}
			else if (transform_type & UI_CONFIG_MAKE_SQUARE) {
				element_transform_types[current_index] = transform_type;
				parameters.y = parameters.y == 0.0f ? row_scale.y : parameters.y;
				UpdateRowYScale(parameters.y);
				element_sizes[current_index] = drawer->GetSquareScale(parameters.y < row_scale.y ? parameters.y : row_scale.y);
			}
			else {
				ECS_ASSERT(false);
			}
			element_alignment[current_index] = alignment;
			has_border[current_index] = false;
			current_index++;
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::AddLabel(Stream<char> characters, ECS_UI_ALIGN alignment)
		{
			float2 scale = drawer->GetLabelScale(characters);
			scale *= float2::Splat(font_scaling);
			AddElement(UI_CONFIG_ABSOLUTE_TRANSFORM, scale, alignment);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::AddSquareLabel(float label_size, ECS_UI_ALIGN alignment)
		{
			AddElement(UI_CONFIG_MAKE_SQUARE, { 0.0f, label_size }, alignment);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::AddCheckBox(Stream<char> name, ECS_UI_ALIGN alignment)
		{
			AddSquareLabel(alignment);
			if (name.size > 0) {
				AddLabel(name, alignment);
				CombineLastElements();
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::AddComboBox(Stream<Stream<char>> labels, Stream<char> name, Stream<char> prefix, ECS_UI_ALIGN alignment)
		{
			float combo_scale = drawer->ComboBoxDefaultSize(labels, name, prefix);
			AddElement(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_COMBO_BOX_NAME_WITH_SCALE, { combo_scale, row_scale.y }, alignment);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::AddBorderToLastElement()
		{
			has_border[current_index - 1] = true;
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::CombineLastElements(unsigned int count, bool keep_indents)
		{
			ECS_ASSERT(count <= current_index);

			float total_scale = 0.0f;
			float max_y_scale = 0.0f;
			for (unsigned int index = 0; index < count; index++) {
				float2 current_scale = GetScaleForElement(current_index - index - 1);
				total_scale += current_scale.x;
				max_y_scale = std::max(max_y_scale, current_scale.y);
				if (index != 0 && keep_indents) {
					total_scale += indentations[current_index - index];
				}
			}

			unsigned int first_element = current_index - count;
			element_sizes[first_element].x = total_scale;
			element_sizes[first_element].y = max_y_scale;
			element_transform_types[first_element] = ClearFlag(
				element_transform_types[first_element],
				UI_CONFIG_RELATIVE_TRANSFORM, 
				UI_CONFIG_WINDOW_DEPENDENT_SIZE, 
				UI_CONFIG_MAKE_SQUARE
			);
			element_transform_types[first_element] |= UI_CONFIG_ABSOLUTE_TRANSFORM;
			current_index -= count - 1;
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::RemoveLastElementsIndentation(unsigned int count)
		{
			ECS_ASSERT(count <= current_index);

			for (unsigned int index = 0; index < count - 1; index++) {
				indentations[current_index - 1 - index] = 0.0f;
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::GetTransform(UIDrawConfig& config, size_t& configuration)
		{
			if (element_count == 0) {
				element_count = current_index;
				current_index = 0;

				UpdateWindowDependentElements();
				UpdateElementsFromAlignment();

				if (vertical_alignment == ECS_UI_ALIGN_MIDDLE) {
					drawer->SetCurrentY(AlignMiddle(drawer->current_y, drawer->region_limit.y - drawer->current_y, row_scale.y));
				}
				else if (vertical_alignment == ECS_UI_ALIGN_BOTTOM) {
					drawer->SetCurrentY(drawer->GetAlignedToBottom(row_scale.y).y);
				}
			}

			drawer->OffsetX(indentations[current_index]);
			if (current_index > 0) {
				drawer->Indent(-1.0f);
				if (offset_render_region.x) {
					drawer->OffsetX(-drawer->region_render_offset.x);
				}
			}

			ECS_ASSERT(current_index < element_count);
			if (HasFlag(element_transform_types[current_index], UI_CONFIG_ABSOLUTE_TRANSFORM) || HasFlag(element_transform_types[current_index], UI_CONFIG_MAKE_SQUARE)) {
				UIConfigAbsoluteTransform transform;
				transform.position = drawer->GetCurrentPositionNonOffset();
				transform.scale = element_sizes[current_index];

				if (offset_render_region.x) {
					transform.position.x += drawer->region_render_offset.x;
				}
				if (offset_render_region.y) {
					transform.position.y += drawer->region_render_offset.y;
				}

				config.AddFlag(transform);
				configuration |= element_transform_types[current_index];
			}
			else if (element_transform_types[current_index] & UI_CONFIG_RELATIVE_TRANSFORM) {
				UIConfigRelativeTransform transform;
				transform.offset = { 0.0f, 0.0f };

				if (offset_render_region.x) {
					transform.offset.x = drawer->region_render_offset.x;
				}
				if (offset_render_region.y) {
					transform.offset.y = drawer->region_render_offset.y;
				}

				transform.scale = element_sizes[current_index];
				config.AddFlag(transform);
				configuration |= element_transform_types[current_index];
			}
			else if (element_transform_types[current_index] & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				UIConfigWindowDependentSize transform;
				transform.type = ECS_UI_WINDOW_DEPENDENT_HORIZONTAL;
				transform.scale_factor = element_sizes[current_index];

				if (offset_render_region.x) {
					transform.offset.x = drawer->region_render_offset.x;
				}
				if (offset_render_region.y) {
					transform.offset.y = drawer->region_render_offset.y;
				}

				config.AddFlag(transform);
				configuration |= element_transform_types[current_index];
			}
			else {
				ECS_ASSERT(false);
			}
			current_index++;
		}

		// --------------------------------------------------------------------------------------------------------------

		float2 UIDrawerRowLayout::GetScaleForElement(unsigned int index) const
		{
			float2 border_size = has_border[index] ? border_thickness : float2(0.0f, 0.0f);
			if (HasFlag(element_transform_types[index], UI_CONFIG_ABSOLUTE_TRANSFORM) || HasFlag(element_transform_types[index], UI_CONFIG_MAKE_SQUARE)) {
				return element_sizes[index] + border_size;
			}
			else if (element_transform_types[index] & UI_CONFIG_RELATIVE_TRANSFORM) {
				return drawer->GetRelativeElementSize(element_sizes[index]) + border_size;
			}
			else if (element_transform_types[index] & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
				return drawer->GetWindowSizeScaleElement(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, element_sizes[index]) + border_size;
			}
			else {
				ECS_ASSERT(false);
			}

			return { 0.0f, 0.0f };
		}
		
		// --------------------------------------------------------------------------------------------------------------

		float2 UIDrawerRowLayout::GetRowScale() const
		{
			float2 scale = float2::Splat(0.0f);
			for (unsigned int index = 0; index < element_count; index++) {
				float2 current_scale = GetScaleForElement(index);
				scale.x += current_scale.x;
				scale.y = std::max(scale.y, current_scale.y);
				if (index < element_count - 1) {
					scale.x += indentations[index + 1];
				}
			}
			return scale;
		}

		// --------------------------------------------------------------------------------------------------------------

		float2 UIDrawerRowLayout::GetRowStart() const
		{
			float2 base_position = float2(drawer->GetNextRowXPosition() + indentations[0], drawer->current_y);
			if (offset_render_region.x) {
				base_position.x += drawer->region_render_offset.x;
			}
			if (offset_render_region.y) {
				base_position.y += drawer->region_render_offset.y;
			}
			return base_position;
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::IndentRow(float scale)
		{
			row_scale.x -= scale;
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::SetRowYScale(float scale)
		{
			row_scale.y = scale;
			UpdateSquareElements();
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::SetIndentation(float _indentation)
		{
			indentation = _indentation;
			for (unsigned int index = 1; index < current_index; index++) {
				if (indentations[index] > 0.0f) {
					indentations[index] = _indentation;
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::SetHorizontalAlignment(ECS_UI_ALIGN _alignment) {
			horizontal_alignment = _alignment;
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::SetVerticalAlignment(ECS_UI_ALIGN alignment)
		{
			vertical_alignment = alignment;
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::SetOffsetRenderRegion(bool2 should_offset)
		{
			offset_render_region = should_offset;
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::SetBorderThickness(float thickness)
		{
			border_thickness = drawer->GetSquareScale(thickness) * float2(2.0f, 2.0f);
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::UpdateRowYScale(float scale)
		{
			if (scale > row_scale.y) {
				row_scale.y = scale;
				UpdateSquareElements();
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::UpdateSquareElements()
		{
			for (unsigned int index = 0; index < current_index; index++) {
				if (element_transform_types[index] & UI_CONFIG_MAKE_SQUARE) {
					element_sizes[index] = drawer->GetSquareScale(row_scale.y);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::UpdateWindowDependentElements()
		{
			float horizontal_scale_before = 0.0f;
			float horizontal_scale_after = 0.0f;

			for (unsigned int index = 0; index < element_count; index++) {
				if (element_transform_types[index] & UI_CONFIG_WINDOW_DEPENDENT_SIZE) {
					// Calculate the after size
					for (unsigned int subindex = index + 1; subindex < element_count; subindex++) {
						if (HasFlag(element_transform_types[subindex], UI_CONFIG_ABSOLUTE_TRANSFORM) || HasFlag(element_transform_types[subindex], UI_CONFIG_MAKE_SQUARE)) {
							horizontal_scale_after += element_sizes[subindex].x;
						}
						else {
							horizontal_scale_after += element_sizes[subindex].x * drawer->layout.default_element_x;
						}

						//if (subindex < element_count - 1) {
							horizontal_scale_after += indentations[subindex];
						//}
					}
					float remaining_scale = row_scale.x - horizontal_scale_before - horizontal_scale_after;

					element_sizes[index] = drawer->GetWindowSizeFactors(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, { remaining_scale, element_sizes[index].y });
					break;
				}
				else {
					if (HasFlag(element_transform_types[index], UI_CONFIG_ABSOLUTE_TRANSFORM) || HasFlag(element_transform_types[index], UI_CONFIG_MAKE_SQUARE)) {
						horizontal_scale_before += element_sizes[index].x;
					}
					else {
						horizontal_scale_before += element_sizes[index].x * drawer->layout.default_element_x;
					}

					horizontal_scale_before += indentations[index];
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------------

		void UIDrawerRowLayout::UpdateElementsFromAlignment()
		{
#pragma region Whole Row Alignment
			// This is the general pass that aligns all the elements according to the overall
			if (horizontal_alignment == ECS_UI_ALIGN_LEFT) {
				indentations[0] = 0.0f;
			}
			else {
				// Calculate the total element size
				float total_size = 0.0f;
				for (unsigned int index = 0; index < element_count; index++) {
					if (HasFlag(element_transform_types[index], UI_CONFIG_ABSOLUTE_TRANSFORM) || HasFlag(element_transform_types[index], UI_CONFIG_MAKE_SQUARE)) {
						total_size += element_sizes[index].x;
					}
					else if (HasFlag(element_transform_types[index], UI_CONFIG_RELATIVE_TRANSFORM)) {
						total_size += drawer->GetRelativeElementSize(element_sizes[index]).x;
					}
					else {
						total_size += drawer->GetWindowSizeScaleElement(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, element_sizes[index]).x;
					}
					if (index < element_count - 1) {
						total_size += indentations[index + 1];
					}
				}

				if (horizontal_alignment == ECS_UI_ALIGN_MIDDLE) {
					indentations[0] = AlignMiddle(0.0f, row_scale.x, total_size);
				}
				else if (horizontal_alignment == ECS_UI_ALIGN_RIGHT) {
					indentations[0] = row_scale.x - total_size;
				}
			}
#pragma endregion

#pragma region Element Transform Alignments
			
			// At the moment we ignore indentations after elements

			// Determine the size of the middle, left and right elements
			float left_elements_size = 0.0f;
			float middle_elements_size = 0.0f;
			float right_elements_size = 0.0f;

			unsigned int last_left_element = -1;
			unsigned int first_middle_element = -1;
			unsigned int first_right_element = -1;
			for (unsigned int index = 0; index < element_count; index++) {
				float element_scale = GetScaleForElement(index).x;
				if (index < element_count - 1) {
					// Add the indentation as well if it is not the last element
					element_scale += indentations[index + 1];
				}
				if (element_alignment[index] == ECS_UI_ALIGN_MIDDLE) {
					middle_elements_size += element_scale;
					if (first_middle_element == -1) {
						first_middle_element = index;
					}
				}
				else if (element_alignment[index] == ECS_UI_ALIGN_LEFT) {
					left_elements_size += element_scale;
					last_left_element = index;
				}
				else if (element_alignment[index] == ECS_UI_ALIGN_RIGHT) {
					right_elements_size += element_scale;
					if (first_right_element == -1) {
						first_right_element = index;
					}
				}
			}
			if (last_left_element != -1) {
				left_elements_size -= indentations[last_left_element + 1];
			}

			// Determine if they fit in the available space
			float total_size = left_elements_size + middle_elements_size + right_elements_size;
			if (total_size < row_scale.x) {
				// They fit
				// Determine how much space is left for the middle alignment
				float middle_alignment_space = row_scale.x - total_size;
				// We need to offset the middle elements by the half of that distance
				middle_alignment_space *= 0.5f;

				// The right elements need to be offsetted by row_scale.x - right_elements_size
				float right_alignment = row_scale.x - right_elements_size;

				// We just need to add the offsets to the indentations
				if (first_middle_element != -1) {
					indentations[first_middle_element] = middle_alignment_space;
				}
				
				if (first_right_element != -1) {
					if (middle_elements_size > 0.0f) {
						indentations[first_right_element] = right_alignment - middle_alignment_space;
					}
					else {
						indentations[first_right_element] = right_alignment - left_elements_size;
					}
				}
			}
			else {
				// Nothing needs to be done since they don't fit anyway
			}

#pragma endregion

		}

		// --------------------------------------------------------------------------------------------------------------

	}

}