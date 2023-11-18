#pragma once
#include "ECSEngineUI.h"

struct EditorState;
ECS_TOOLS;

#define MAX_INSPECTOR_WINDOWS 8

typedef void (*InspectorDrawFunction)(EditorState* editor_state, unsigned int inspector_index, void* data, ECSEngine::Tools::UIDrawer* drawer);
typedef void (*InspectorCleanDrawFunction)(EditorState* editor_state, unsigned int inspector_index, void* data);
struct InspectorFunctions {
	InspectorDrawFunction draw_function;
	InspectorCleanDrawFunction clean_function;
	WindowRetainedMode retained_function = nullptr;
};
typedef ECSEngine::HashTableDefault<InspectorFunctions> InspectorTable;

struct InspectorData {
	InspectorDrawFunction draw_function;
	// This function will be called when the inspector changes to another type
	// in order to release resources that were created/loaded in the draw function
	InspectorCleanDrawFunction clean_function;
	// This function will be called in case the current inspector draw wants to be in
	// Retained mode
	WindowRetainedMode retained_mode;
	void* draw_data;
	unsigned int data_size;
	// This is the sandbox that describes which sandbox is accepted
	// If it is -1, it means that all sandboxes are accepted
	unsigned int matching_sandbox;
	// This is the sandbox that is currently bound
	// To the display function
	unsigned int target_sandbox;
	size_t flags;
	InspectorTable* table;
};

// A thin wrapper over an array of inspector data and a round robin indices in order to
// select just one inspector at a time
struct InspectorManager {
	InspectorTable function_table;
	ECSEngine::ResizableStream<InspectorData> data;
	// The index at sandbox size represents the index for an inspector action for valid
	// for all sandboxes
	ECSEngine::Stream<unsigned int> round_robin_index;
};

struct InspectorAssetTarget {
	ECSEngine::Stream<wchar_t> path;
	ECSEngine::Stream<char> initial_asset;
};

// Returns the index of the inspector instance. Does not create the UI window, only registers it
unsigned int CreateInspectorInstance(EditorState* editor_state);

bool DoesInspectorMatchSandbox(const EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index);

// This function does not destroy the UI window, just the backend for the inspector manager data
void DestroyInspectorInstance(EditorState* editor_state, unsigned int inspector_index);

void InitializeInspectorManager(EditorState* editor_state);

// Determines the index of the inspector from its name
unsigned int GetInspectorIndex(ECSEngine::Stream<char> window_name);

// Returns the inspector name according to an index
void GetInspectorName(unsigned int inspector_index, ECSEngine::CapacityStream<char>& inspector_name);

InspectorDrawFunction GetInspectorDrawFunction(const EditorState* editor_state, unsigned int inspector_index);

void GetInspectorsForSandbox(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::CapacityStream<unsigned int>* inspector_indices);

// It will resize the round robin index - can be used both when removing or adding
// And reroute any inspector pointing to an invalid sandbox (when removing)
// to the 0 sandbox
void RegisterInspectorSandboxChange(EditorState* editor_state);

// All inspectors that point to old_sandbox_index will be rerouted to new_sandbox_index
// It will also change the name of the sandbox window if there is one with that index
void FixInspectorSandboxReference(EditorState* editor_state, unsigned int old_sandbox_index, unsigned int new_sandbox_index);

// Recreates the UI instances for the inspectors that target the settings of the given module
void UpdateInspectorUIModuleSettings(EditorState* editor_state, unsigned int module_index);

// Sandbox index -1 means accept information from any sandbox
void SetInspectorMatchingSandbox(EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index);

void SetInspectorMatchingSandboxAll(EditorState* editor_state, unsigned int inspector_index);

// Returns the target sandbox index if the UI window is an inspector window, else -1
unsigned int GetInspectorTargetSandboxFromUIWindow(const EditorState* editor_state, unsigned int window_index);

// It will change inspectors such that the selection will be shown (if possible)
void ChangeInspectorEntitySelection(EditorState* editor_state, unsigned int sandbox_index);