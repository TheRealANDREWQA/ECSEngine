#pragma once
#include "UIDrawerStructures.h"
#include "UIDrawerActionStructures.h"
#include "../../Utilities/FunctionTemplates.h"

namespace ECSEngine {

	namespace Tools {

		struct UISystem;

		// --------------------------------------------------------------------------------------------------------------
		
		ECSENGINE_API void WindowHandlerInitializer(ActionData* action_data);
		
		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void AddWindowHandleCommand(
			UISystem* system,
			unsigned int window_index,
			Action action,
			void* data,
			size_t data_size,
			HandlerCommandType command_type
		);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SliderAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SliderBringToMouse(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SliderReturnToDefault(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SliderReturnToDefaultMouseDraggable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SliderMouseDraggable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void TextInputRevertAddText(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void TextInputRevertRemoveText(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void TextInputRevertReplaceText(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void TextInputClickable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void TextInputHoverable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ColorInputSliderCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ColorInputHexCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ColorInputDefaultColor(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ColorInputPreviousRectangleClickableAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ComboBoxLabelClickable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void BoolClickable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void BoolClickableWithPin(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void HierarchySelectableClick(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void HierarchyNodeDrag(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void GraphHoverable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void HistogramHoverable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void DoubleInputDragValue(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void FloatInputDragValue(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SliderCopyPaste(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		ECSENGINE_API void IntegerInputDragValue(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		// if a different submenu is being hovered, delay the destruction so all windows can be rendered during that frame
		ECSENGINE_API void MenuCleanupSystemHandler(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void RightClickMenuReleaseResource(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void FloatInputCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void DoubleInputCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		ECSENGINE_API void IntegerInputCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------
		 
		ECSENGINE_API void FloatInputHoverable(ActionData* action_data);

		ECSENGINE_API void FloatInputNoNameHoverable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void DoubleInputHoverable(ActionData* action_data);

		ECSENGINE_API void DoubleInputNoNameHoverable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		template<typename Integer>
		ECSENGINE_API void IntInputHoverable(ActionData* action_data);

		template<typename Integer>
		ECSENGINE_API void IntInputNoNameHoverable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParameterColorInputThemeCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParameterColorInputCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParameterLayoutCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void WindowParameterElementDescriptorCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParameterColorInputThemeCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParameterColorThemeCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParameterLayoutCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SystemParameterElementDescriptorCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ChangeStateAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void MenuButtonAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------
		
		ECSENGINE_API void StateTableAllButtonAction(ActionData* action_data);
		
		// --------------------------------------------------------------------------------------------------------------
		 
		ECSENGINE_API void StateTableBoolClickable(ActionData* action_data);
		 
		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void LabelHierarchySelectable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void LabelHierarchyChangeState(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SetWindowVerticalSliderPosition(UISystem* system, unsigned int window_index, float value);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void PinWindowVerticalSliderPosition(UISystem* system, unsigned int window_index);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void PinWindowHorizontalSliderPosition(UISystem* system, unsigned int window_index);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ArrayDragAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ColorFloatInputCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ColorFloatInputIntensityCallback(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

	}

}