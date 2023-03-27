#pragma once
#include "../InspectorData.h"

struct EditorState;

void InspectorDrawMeshFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer);

void InspectorMeshFileAddFunctors(InspectorTable* table);

void ChangeInspectorToMeshFile(EditorState* editor_state, ECSEngine::Stream<wchar_t> path, unsigned int inspector_index, ECSEngine::Stream<char> initial_name = { nullptr, 0 });

void ChangeInspectorMeshFileConfiguration(EditorState* editor_state, unsigned int inspector_index, ECSEngine::Stream<char> name);