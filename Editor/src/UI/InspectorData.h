#pragma once
#include "ECSEngineUI.h"

struct EditorState;

using InspectorDrawFunction = void (*)(EditorState* editor_state, void* data, ECSEngine::Tools::UIDrawer<false>* drawer);
using InspectorTable = IdentifierHashTable<InspectorDrawFunction, ResourceIdentifier, HashFunctionPowerOfTwo>;

struct InspectorData {
	InspectorDrawFunction draw_function;
	void* draw_data;
	size_t data_size;
	InspectorTable table;
};

void InitializeInspector(EditorState* editor_state);