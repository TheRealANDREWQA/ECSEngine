#pragma once
#include "../../Core.h"
#include "UIStructures.h"

namespace ECSEngine {

	namespace Tools {

		struct ECSENGINE_API UIDrawerDescriptor {
			UIDrawerDescriptor(
				UIColorThemeDescriptor& _color_theme,
				UIFontDescriptor& _font,
				UILayoutDescriptor& _layout,
				UIElementDescriptor& _element_descriptor
			) : color_theme(_color_theme), font(_font), layout(_layout), element_descriptor(_element_descriptor) {}

			UIDrawerDescriptor& operator = (const UIDrawerDescriptor& other) = default;

			float2* export_scale;
			void* system;
			UIDockspace* dockspace;
			unsigned int window_index;
			unsigned int border_index;
			DockspaceType dockspace_type;
			bool do_not_allocate_buffers;
			bool record_handlers;
			bool record_snapshot_runnables;
			float2 mouse_position;
			Stream<CapacityStream<void>> buffers;
			Stream<CapacityStream<void>> system_buffers;
			UIColorThemeDescriptor& color_theme;
			UIFontDescriptor& font;
			UILayoutDescriptor& layout;
			UIElementDescriptor& element_descriptor;
		};

	}

}