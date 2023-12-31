#pragma once
#include "../InspectorData.h"

struct EditorState;

void InspectorDrawGPUSamplerFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer);

void ChangeInspectorToGPUSamplerFile(EditorState* editor_state, ECSEngine::Stream<wchar_t> path, unsigned int inspector_index);

void InspectorGPUSamplerFileAddFunctors(InspectorTable* table);

InspectorAssetTarget InspectorDrawGPUSamplerTarget(const void* inspector_data);