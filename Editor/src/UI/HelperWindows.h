#pragma once
#include "ECSEngineUI.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

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
	void (*set_descriptor)(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory)
);

unsigned int CreateDefaultWindowWithIndex(
	const char* window_name,
	EditorState* editor_state,
	float2 window_size,
	unsigned int index,
	void (*set_descriptor)(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory)
);

// Changes the index of an UI window (eg Inspector 1 -> Inspector 0)
// Returns the window index. (-1 if it doesn't exist)
unsigned int UpdateUIWindowAggregateName(EditorState* editor_state, const char* base_name, Stream<char> previous_name, Stream<char> new_name);

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

struct RenameWizardAdditionalData {
	bool prevent_default_renaming;
	// This is the file being renamed
	Stream<wchar_t> path;
	// These 2 names are the same, but given in ascii or wide form
	Stream<char> new_name;
	Stream<wchar_t> wide_new_name;
};

// The user callback receives in the additional data parameter a RenameWizardAdditionalData*
// You can have the rename omitted and handled by you. The new name is given here
unsigned int CreateRenameFileWizard(Stream<wchar_t> path, UISystem* system, UIActionHandler user_callback = {});

// The user callback receives in the additional data parameter a RenameWizardAdditionalData*
// You can have the rename omitted and handled by you. The new name is given here
unsigned int CreateRenameFolderWizard(Stream<wchar_t> path, UISystem* system, UIActionHandler user_callback = {});

// Returns -1 if no window which targets a sandbox is selected
// Else the sandbox index that is being targeted
unsigned int GetActiveWindowSandbox(const EditorState* editor_state);