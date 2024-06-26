#pragma once
#include "EditorParameters.h"
#include "editorpch.h"
#include "ECSEngineUI.h"

using namespace ECSEngine;
ECS_TOOLS;

constexpr const wchar_t* EDITOR_FILE = L"Resources/.ecseditor";

struct EditorState;

bool LoadEditorFile(EditorState* editor_state);

bool SaveEditorFile(EditorState* editor_state);

void SaveEditorFileThreadTask(unsigned int thread_index, World* world, void* data);

void LoadEditorFileAction(ActionData* action_data);

void SaveEditorFileAction(ActionData* action_data);