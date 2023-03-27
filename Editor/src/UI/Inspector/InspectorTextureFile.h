#pragma once
#include "../InspectorData.h"

struct EditorState;

void InspectorDrawTextureFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer);

void InspectorTextureFileAddFunctors(InspectorTable* table);

void ChangeInspectorToTextureFile(EditorState* editor_state, ECSEngine::Stream<wchar_t> path, unsigned int inspector_index, ECSEngine::Stream<char> name = { nullptr, 0 });

void ChangeInspectorTextureFileConfiguration(EditorState* editor_state, unsigned int inspector_index, ECSEngine::Stream<char> name);