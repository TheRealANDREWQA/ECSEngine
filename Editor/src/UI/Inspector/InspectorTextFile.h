#pragma once
#include "../InspectorData.h"

void InspectorDrawBlankFile(EditorState* editor_state, unsigned int inspector_index, void* data, ECSEngine::Tools::UIDrawer* drawer);

void InspectorTextFileAddFunctors(InspectorTable* table);

void ChangeInspectorToTextFile(EditorState* editor_state, ECSEngine::Stream<wchar_t> path, unsigned int inspector_index);

bool IsInspectorTextFileDraw(const EditorState* editor_state, unsigned int inspector_index);

bool IsInspectorTextFileDraw(InspectorDrawFunction draw_function);