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

void InspectorCleanNothing(EditorState* editor_state, void* data);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIconTexture(Tools::UIDrawer* drawer, ResourceView texture, Color color = ECS_COLOR_WHITE);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIcon(Tools::UIDrawer* drawer, Stream<wchar_t> path, Color color = ECS_COLOR_WHITE);

// ----------------------------------------------------------------------------------------------------------------------------

// A double sprite icon
void InspectorIconDouble(Tools::UIDrawer* drawer, Stream<wchar_t> icon0, Stream<wchar_t> icon1, Color icon_color0 = ECS_COLOR_WHITE, Color icon_color1 = ECS_COLOR_WHITE);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIconNameAndPath(Tools::UIDrawer* drawer, Stream<wchar_t> path);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorShowButton(Tools::UIDrawer* drawer, Stream<wchar_t> path);

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorOpenAndShowButton(Tools::UIDrawer* drawer, Stream<wchar_t> path);

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

unsigned int GetMatchingIndexFromRobin(EditorState* editor_state);

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetMatchingIndexFromRobin(EditorState* editor_state, unsigned int target_sandbox);

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
	unsigned int sandbox_index = -1
);

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

void* GetInspectorDrawFunctionData(EditorState* editor_state, unsigned int inspector_index);

// ----------------------------------------------------------------------------------------------------------------------------

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorFunctions& function, Stream<wchar_t> _identifier);

// ----------------------------------------------------------------------------------------------------------------------------

void AddInspectorTableFunction(InspectorTable* table, InspectorFunctions function, const wchar_t* _identifier);

// ----------------------------------------------------------------------------------------------------------------------------