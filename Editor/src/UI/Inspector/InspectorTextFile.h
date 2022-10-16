#pragma once
#include "../InspectorData.h"

void InspectorDrawBlankFile(EditorState* editor_state, unsigned int inspector_index, void* data, ECSEngine::Tools::UIDrawer* drawer);

void InspectorTextFileAddFunctors(InspectorTable* table);