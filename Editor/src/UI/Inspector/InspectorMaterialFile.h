#pragma once
#include "../InspectorData.h"

struct EditorState;

void InspectorDrawMaterialFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer);

void ChangeInspectorToMaterialFile(EditorState* editor_state, ECSEngine::Stream<wchar_t> path, unsigned int inspector_index);

void InspectorMaterialFileAddFunctors(InspectorTable* table);