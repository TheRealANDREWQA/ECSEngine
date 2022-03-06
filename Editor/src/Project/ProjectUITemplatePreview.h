#pragma once
#include "editorpch.h"
#include "ECSEngineUI.h"

using namespace ECSEngine;
;
ECS_TOOLS;

struct EditorState;

void CreateProjectUITemplatePreview(EditorState* editor_state);

void CreateProjectUITemplatePreviewAction(ActionData* action_data);