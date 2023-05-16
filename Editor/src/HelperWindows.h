#pragma once
#include "ECSEngineUI.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;
;

struct EditorState;

typedef unsigned int (*CreateWindowFunction)(EditorState*);

typedef unsigned int (*CreateWindowFunctionWithIndex)(EditorState* editor_state, unsigned int);

void CreateDockspaceFromWindow(const char* window_name, EditorState* editor_state, CreateWindowFunction function);

void CreateDockspaceFromWindowWithIndex(const char* window_name, EditorState* editor_state, unsigned int index, CreateWindowFunctionWithIndex function);

// It sets the window data to editor state
unsigned int CreateDefaultWindow(
	const char* window_name,
	EditorState* editor_state,
	float2 window_size,
	void (*set_descriptor)(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
);

unsigned int CreateDefaultWindowWithIndex(
	const char* window_name,
	EditorState* editor_state,
	float2 window_size,
	unsigned int index,
	void (*set_descriptor)(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
);

// Changes the index of an UI window (eg Inspector 1 -> Inspector 0)
// Returns the window index. (-1 if it doesn't exist)
unsigned int UpdateUIWindowIndex(EditorState* editor_state, const char* base_name, unsigned int old_index, unsigned int new_index);

// Returns the number from the name
unsigned int GetWindowNameIndex(Stream<char> name);

// Returns the number from the name
unsigned int GetWindowNameIndex(const UIDrawer& drawer);

// The callback can access this parameter through additional_data in ActionData*
// The callback can be omitted by making it nullptr
struct ChooseDirectoryOrFileNameData {
	UIActionHandler callback;
	CapacityStream<char>* ascii = nullptr;
	CapacityStream<wchar_t>* wide = nullptr;
};

void ChooseDirectoryOrFileName(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

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