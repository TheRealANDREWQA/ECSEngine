#pragma once
#include "../InspectorData.h"

struct EditorState;

void InspectorDrawShaderFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer);

void ChangeInspectorToShaderFile(EditorState* editor_state, ECSEngine::Stream<wchar_t> path, unsigned int inspector_index);

void InspectorShaderFileAddFunctors(InspectorTable* table);