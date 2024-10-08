#pragma once
#include "editorpch.h"
#include "../InspectorData.h"

constexpr float ICON_SIZE = 0.07f;

using namespace ECSEngine;

namespace ECSEngine {
	namespace Tools {
		struct UIDrawer;
	}
}

struct EditorState;

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawNothing(EditorState* editor_state, unsigned int inspector_index, void* data, Tools::UIDrawer* drawer);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorCleanNothing(EditorState* editor_state, unsigned int inspector_index, void* data);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIconTexture(Tools::UIDrawer* drawer, ResourceView texture, Color color = ECS_COLOR_WHITE);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIcon(Tools::UIDrawer* drawer, Stream<wchar_t> path, Color color = ECS_COLOR_WHITE);

// ----------------------------------------------------------------------------------------------------------------------------

// A double sprite icon
void InspectorIconDouble(Tools::UIDrawer* drawer, Stream<wchar_t> icon0, Stream<wchar_t> icon1);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIconNameAndPath(Tools::UIDrawer* drawer, Stream<wchar_t> path);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorShowButton(Tools::UIDrawer* drawer, Stream<wchar_t> path);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDefaultInteractButtons(EditorState* editor_state, Tools::UIDrawer* drawer, Stream<wchar_t> path);

// ----------------------------------------------------------------------------------------------------------------------------

// Display the stock information and buttons for an inspector file. It does not include the preamble icon
void InspectorDefaultFileInfo(EditorState* editor_state, Tools::UIDrawer* drawer, Stream<wchar_t> path);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawFileTimes(Tools::UIDrawer* drawer, Stream<wchar_t> path);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawTextFile(EditorState* editor_state, unsigned int inspector_index, void* data, Tools::UIDrawer* drawer);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawCTextFile(EditorState* editor_state, unsigned int inspector_index, void* data, Tools::UIDrawer* drawer);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawCppTextFile(EditorState* editor_state, unsigned int inspector_index, void* data, Tools::UIDrawer* drawer);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawHlslTextFile(EditorState* editor_state, unsigned int inspector_index, void* data, Tools::UIDrawer* drawer);

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetMatchingIndexFromRobin(EditorState* editor_state, unsigned int target_sandbox = -1);

// ----------------------------------------------------------------------------------------------------------------------------

InspectorDrawFunction GetInspectorDrawFunction(const EditorState* editor_state, unsigned int inspector_index);

// ----------------------------------------------------------------------------------------------------------------------------

// Inspector index == -1 means find the first suitable inspector
// Sandbox index != -1 means find the first suitable inspector bound to sandbox index
// Returns the final inspector index
unsigned int ChangeInspectorDrawFunction(
	EditorState* editor_state,
	unsigned int inspector_index,
	InspectorFunctions functions,
	void* data,
	size_t data_size,
	unsigned int sandbox_index = -1,
	bool do_not_push_target_entry = false
);

// The functor takes a parameter the data of the inspector to be compared and returns true
// if a match was found, else false. This is used to instead highlight an inspector if it already exists.
// It returns -1 if either no inspector was found or an inspector already exists with that highlighted data
template<typename Functor>
unsigned int ChangeInspectorDrawFunctionWithSearch(
	EditorState* editor_state,
	unsigned int inspector_index,
	InspectorFunctions functions,
	void* data,
	size_t data_size,
	unsigned int sandbox_index,
	Functor&& functor
) {
	ECS_STACK_CAPACITY_STREAM(unsigned int, indices, MAX_INSPECTOR_WINDOWS);
	FindInspectorWithDrawFunction(editor_state, functions.draw_function, &indices, sandbox_index);
	for (unsigned int index = 0; index < indices.size; index++) {
		if (functor(GetInspectorDrawFunctionData(editor_state, indices[index]))) {
			return indices[index];
		}
	}

	return ChangeInspectorDrawFunction(editor_state, inspector_index, functions, data, data_size, sandbox_index);
}

// The functor takes a parameter the data of the inspector to be compared and returns true
// if a match was found, else false. This is used to instead highlight an inspector if it already exists.
// It returns the index of an already existing inspector that matches the functor in the x component
// If there is none it will be it -1 and the y component will be the index of an inspector that didn't target
// that element (it can be -1 if they are all locked). The z component is the index of the inspector found (without
// distinction for existing vs not existing inspector)
template<typename Functor>
uint3 ChangeInspectorDrawFunctionWithSearchEx(
	EditorState* editor_state,
	unsigned int inspector_index,
	InspectorFunctions functions,
	void* data,
	size_t data_size,
	unsigned int sandbox_index,
	Functor&& functor
) {
	ECS_STACK_CAPACITY_STREAM(unsigned int, indices, MAX_INSPECTOR_WINDOWS);
	FindInspectorWithDrawFunction(editor_state, functions.draw_function, &indices, sandbox_index);
	for (unsigned int index = 0; index < indices.size; index++) {
		if (functor(GetInspectorDrawFunctionData(editor_state, indices[index]))) {
			// Highlight the window to keep the same behaviour
			ECS_STACK_CAPACITY_STREAM(char, inspector_window_name, 512);
			GetInspectorName(indices[index], inspector_window_name);
			editor_state->ui_system->SetActiveWindow(inspector_window_name);
			return { indices[index], (unsigned int)-1, indices[index] };
		}
	}

	unsigned int new_inspector_index = ChangeInspectorDrawFunction(editor_state, inspector_index, functions, data, data_size, sandbox_index);
	return { (unsigned int)-1, new_inspector_index, new_inspector_index };
}

// ----------------------------------------------------------------------------------------------------------------------------

// Returns the first inspector which matches the sandbox and the draw function, or -1 if it doesn't exist
unsigned int FindInspectorWithDrawFunction(
	const EditorState* editor_state,
	InspectorDrawFunction draw_function,
	unsigned int sandbox_index = -1
);

// ----------------------------------------------------------------------------------------------------------------------------

// Returns all inspector indices which match the given draw function and the sandbox
void FindInspectorWithDrawFunction(
	const EditorState* editor_state,
	InspectorDrawFunction draw_function,
	CapacityStream<unsigned int>* inspector_indices,
	unsigned int sandbox_index = -1
);

// ----------------------------------------------------------------------------------------------------------------------------

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorFunctions& function, Stream<wchar_t> _identifier);

// ----------------------------------------------------------------------------------------------------------------------------

void AddInspectorTableFunction(InspectorTable* table, InspectorFunctions function, Stream<wchar_t> _identifier);

// ----------------------------------------------------------------------------------------------------------------------------