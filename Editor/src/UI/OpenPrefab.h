#pragma once
#include "ECSEngineUI.h"

ECS_TOOLS;

struct EditorState;

struct OpenPrefabActionData {
	EditorState* editor_state;
	unsigned int prefab_id;
	unsigned int launching_sandbox;
	unsigned int inspector_index;
};

void OpenPrefabAction(ActionData* action_data);