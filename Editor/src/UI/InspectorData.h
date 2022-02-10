#pragma once
#include "ECSEngineUI.h"

struct EditorState;

typedef void (*InspectorDrawFunction)(EditorState* editor_state, void* data, ECSEngine::Tools::UIDrawer* drawer);
typedef void (*InspectorCleanDrawFunction)(EditorState* editor_state, void* data);
struct InspectorFunctions {
	InspectorDrawFunction draw_function;
	InspectorCleanDrawFunction clean_function;
};
typedef ECSEngine::containers::HashTable<InspectorFunctions, ECSEngine::ResourceIdentifier, ECSEngine::containers::HashFunctionPowerOfTwo, ECSEngine::HashFunctionMultiplyString> InspectorTable;

struct InspectorData {
	InspectorDrawFunction draw_function;
	// This function will be called when the inspector changes to another type
	// in order to release resources that were created/loaded in the draw function
	InspectorCleanDrawFunction clean_function;
	void* draw_data;
	size_t data_size;
	InspectorTable table;
	size_t flags;
};