#pragma once
#include "ECSEngineUI.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;
ECS_CONTAINERS;

struct EditorState;

using CreateWindowFunction = unsigned int (*)(EditorState*);

void CreateDockspaceFromWindow(const char* window_name, EditorState* editor_state, CreateWindowFunction function);

// It sets the window data to editor state
unsigned int CreateDefaultWindow(
	const char* window_name,
	EditorState* editor_state,
	float2 window_size,
	WindowDraw draw,
	WindowDraw initialize
);

#define EDITOR_CREATE_DOCKSPACE_FROM_WINDOW_DEFINITIONS(base_name, window_name) void base_name(void* _editor_state) { \
	Editor::CreateDockspaceFromWindow(window_name, _editor_state, base_name##Window); \
} \
void base_name##Action(ActionData* action_data) { \
	base_name(action_data->data); \
}


// The callback can access this parameter through additional_data in ActionData*
// The callback can be omitted by making it nullptr
struct ChooseDirectoryOrFileNameData {
	UIActionHandler callback;
	CapacityStream<char>* ascii = nullptr;
	CapacityStream<wchar_t>* wide = nullptr;
};

template<bool initialize>
void ChooseDirectoryOrFileName(void* window_data, void* drawer_descriptor);

// Returns the window index
// The callback can access this parameter through additional_data in ActionData*
// The callback can be omitted by making it nullptr
unsigned int CreateChooseDirectoryOrFileNameDockspace(UISystem* system, ChooseDirectoryOrFileNameData data);

unsigned int CreateRenameFileWizard(Stream<wchar_t> path, UISystem* system);

// Data must be a Stream<wchar_t>*
void CreateRenameFileWizardAction(ActionData* action_data);

unsigned int CreateRenameFolderWizard(Stream<wchar_t> path, UISystem* system);

// Data must be a Stream<wchar_t>*
void CreateRenameFolderWizardAction(ActionData* action_data);