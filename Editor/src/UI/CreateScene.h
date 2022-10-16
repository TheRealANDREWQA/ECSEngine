#pragma once
#include "ECSEngineUI.h"

struct EditorState;

enum SAVE_SCENE_POP_UP_STATUS {
	SAVE_SCENE_POP_UP_SUCCESSFUL,
	SAVE_SCENE_POP_UP_FAILED,
	SAVE_SCENE_POP_UP_ABORTED
};

struct SaveScenePopUpResult {
	unsigned int sandbox_indices[16];
	SAVE_SCENE_POP_UP_STATUS statuses[16];
	unsigned int count;

	// Set to true when pressed by cancel
	bool cancel_call;
};

// The continue_handler receives in the additional_data a SaveScenePopUpResult* that describes what the state last
// operation was and its status. There will be as many SAVE_SCENE_POP_UP_STATUS as unique scenes provided. If the don't save
// button is pressed then all SAVE_SCENE_POP_UP_STATUS will be ABORTED. Also it is called on cancel and sets the cancel_call to true
void CreateSaveScenePopUp(EditorState* editor_state, ECSEngine::Stream<unsigned int> sandbox_indices, ECSEngine::Tools::UIActionHandler continue_handler);

// It includes the wizard that selects the name. The data must be an EditorState*
void CreateEmptySceneAction(ECSEngine::Tools::ActionData* action_data);

struct ChangeSandboxSceneActionData {
	EditorState* editor_state;
	unsigned int sandbox_index;
};

// Needs to receive as data a ChangeSandboxSceneActionData*
void ChangeSandboxSceneAction(ECSEngine::Tools::ActionData* action_data);