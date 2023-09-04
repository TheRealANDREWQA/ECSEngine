#pragma once
#include "UIDrawerStructures.h"
#include "UIDrawerActionStructures.h"
#include "../../Utilities/FunctionInterfaces.h"

namespace ECSEngine {

	namespace Tools {

		struct UISystem;

		// --------------------------------------------------------------------------------------------------------------
		
		ECSENGINE_API void WindowHandlerInitializer(ActionData* action_data);
		
		// --------------------------------------------------------------------------------------------------------------

		// The owning resource is used to identify commands which are no longer valid
		// For example, commands from a text input which no longer exists. If the pointer
		// is left at nullptr, then it will consider that the command is always valid.
		// Else (it is specified), it will search for the pointer in the memory resources
		// For that window
		ECSENGINE_API void AddWindowHandleCommand(
			UISystem* system,
			unsigned int window_index,
			Action action,
			void* data,
			size_t data_size,
			void* owning_resource
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

		ECSENGINE_API void ChangeAtomicStateAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void MenuButtonAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------
		
		ECSENGINE_API void StateTableAllButtonAction(ActionData* action_data);
		
		// --------------------------------------------------------------------------------------------------------------
		 
		ECSENGINE_API void StateTableBoolClickable(ActionData* action_data);
		 
		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void FilesystemHierarchySelectable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void FilesystemHierarchyChangeState(ActionData* action_data);

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

		ECSENGINE_API void TextInputAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void SliderEnterValues(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ColorInputSVRectangleClickableAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ColorInputHRectangleClickableAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ColorInputARectangleClickableAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ColorInputWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initializer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ColorInputCreateWindow(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ComboBoxWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initializer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void ComboBoxClickable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void MenuDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initializer);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void MenuSubmenuHoverable(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void MenuGeneral(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		// Takes a UIDrawerMenuRightClickData* as data parameter 
		ECSENGINE_API void RightClickMenu(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void FilterMenuDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void FilterMenuSinglePointerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void PathInputFilesystemDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void FileInputFolderAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void DirectoryInputFolderAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		// This corresponds to the UI_CONFIG_PATH_INPUT_GIVE_FILES flag
		ECSENGINE_API void PathInputFolderWithInputsAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void LabelHierarchyDeselect(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void LabelHierarchyChangeState(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void LabelHierarchyRenameLabelFrameHandler(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void LabelHierarchyRenameLabel(ActionData* action_data);
		
		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void LabelHierarchyClickAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

		ECSENGINE_API void LabelHierarchyRightClickAction(ActionData* action_data);

		// --------------------------------------------------------------------------------------------------------------

	}

}