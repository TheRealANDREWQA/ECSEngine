#pragma once
#include "EditorParameters.h"
#include "editorpch.h"
#include "ECSEngineUI.h"

using namespace ECSEngine;
ECS_TOOLS;

constexpr const wchar_t* EDITOR_FILE = L"Resources/.ecseditor";

struct EditorState;

// Can provide an optional error message to be filled in case an error is encountered. It can be filled in even when
// It doesn't return false, in case the error is skippable
bool LoadEditorFile(EditorState* editor_state, CapacityStream<char>* error_message = nullptr);

bool SaveEditorFile(EditorState* editor_state);

void SaveEditorFileThreadTask(unsigned int thread_index, World* world, void* data);

void LoadEditorFileAction(ActionData* action_data);

void SaveEditorFileAction(ActionData* action_data);