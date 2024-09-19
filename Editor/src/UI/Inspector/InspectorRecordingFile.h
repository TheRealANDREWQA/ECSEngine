#pragma once
#include "../InspectorData.h"

void InspectorDrawRecordingFile(EditorState* editor_state, unsigned int inspector_index, void* data, ECSEngine::Tools::UIDrawer* drawer);

void InspectorRecordingFileAddFunctors(InspectorTable* table);

void ChangeInspectorToRecordingFile(EditorState* editor_state, ECSEngine::Stream<wchar_t> path, unsigned int inspector_index);