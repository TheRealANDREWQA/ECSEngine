#pragma once
#include "ECSEngineUI.h"

struct EditorState;

using InspectorDrawFunction = void (*)(EditorState* editor_state, void* data, ECSEngine::Tools::UIDrawer* drawer);
using InspectorTable = ECSEngine::containers::HashTable<InspectorDrawFunction, ECSEngine::ResourceIdentifier, ECSEngine::containers::HashFunctionPowerOfTwo, ECSEngine::HashFunctionMultiplyString>;

struct InspectorData {
	InspectorDrawFunction draw_function;
	void* draw_data;
	size_t data_size;
	InspectorTable table;
	size_t flags;
};