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

	typedef void (*TargetInitialize)(EditorState* editor_state, void* data, void* initialize_data, unsigned int inspector_index);

	// This target is used to go back and forth between previous targets
	// Of the inspector
	struct Target {
		InspectorFunctions functions;
		void* data;
		size_t data_size;
		unsigned int sandbox_index;

		LinearAllocator initialize_allocator;
		TargetInitialize initialize;
		void* initialize_data;
		size_t initialize_data_size;
	};
	MemoryManager target_allocator;
	ECSEngine::Stack<Target> targets;
	unsigned int target_valid_count;
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

// Returns -1 if it doesn't exist
unsigned int GetInspectorUIWindowIndex(const EditorState* editor_state, unsigned int inspector_index);

void GetInspectorsForMatchingSandbox(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::CapacityStream<unsigned int>* inspector_indices);

InspectorDrawFunction GetInspectorDrawFunction(const EditorState* editor_state, unsigned int inspector_index);

void* GetInspectorDrawFunctionData(EditorState* editor_state, unsigned int inspector_index);

const void* GetInspectorDrawFunctionData(const EditorState* editor_state, unsigned int inspector_index);

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

// Adds a new entry in the "stack" of the inspector such that "undo"/"redo" can be implemented
void PushInspectorTarget(
	EditorState* editor_state,
	unsigned int inspector_index,
	InspectorFunctions functions,
	void* data,
	size_t data_size,
	unsigned int sandbox_index
);

// This only moves the current index and goes back to the previous target (if there is one)
void PopInspectorTarget(EditorState* editor_state, unsigned int inspector_index);

// If there is a slot for this, it will only bump back the index
void RestoreInspectorTarget(EditorState* editor_state, unsigned int inspector_index);

void* AllocateLastInspectorTargetInitialize(
	EditorState* editor_state,
	unsigned int inspector_index,
	size_t data_size
);

AllocatorPolymorphic GetLastInspectorTargetInitializeAllocator(EditorState* editor_state, unsigned int inspector_index);

void SetLastInspectorTargetInitializeFromAllocation(
	EditorState* editor_state,
	unsigned int inspector_index,
	InspectorData::TargetInitialize initialize,
	bool apply_on_current_data = true
);

// Returns the allocated data, if there is any
void* SetLastInspectorTargetInitialize(
	EditorState* editor_state,
	unsigned int inspector_index,
	InspectorData::TargetInitialize initialize,
	void* initialize_data,
	size_t initialize_data_size,
	bool apply_on_current_data = true
);

template<typename Functor>
void SetLastInspectorTargetInitialize(
	EditorState* editor_state,
	unsigned int inspector_index,
	Functor&& functor,
	bool apply_on_current_data = true
) {
	auto wrapper = [](EditorState* editor_state, void* data, void* initialize_data, unsigned int inspector_index) {
		Functor* functor = (Functor*)initialize_data;
		(*functor)(editor_state, data, inspector_index);
	};
	SetLastInspectorTargetInitialize(editor_state, inspector_index, wrapper, &functor, sizeof(functor), apply_on_current_data);
}

// This performs a memcpy or changes the assigned data
void UpdateLastInspectorTargetData(
	EditorState* editor_state,
	unsigned int inspector_index,
	void* new_data
);