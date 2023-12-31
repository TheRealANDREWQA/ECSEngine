#pragma once
#include "../InspectorData.h"

struct EditorState;

void InspectorDrawMiscFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer);

void ChangeInspectorToMiscFile(EditorState* editor_state, ECSEngine::Stream<wchar_t> path, unsigned int inspector_index);

void InspectorMiscFileAddFunctors(InspectorTable* table);

InspectorAssetTarget InspectorDrawMiscTarget(const void* inspector_data);