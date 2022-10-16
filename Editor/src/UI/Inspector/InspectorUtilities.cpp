#include "editorpch.h"
#include "InspectorUtilities.h"
#include "ECSEngineUI.h"
#include "../Inspector.h"
#include "../../Editor/EditorState.h"

using namespace ECSEngine;
ECS_TOOLS;

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorCleanNothing(EditorState* editor_state, void* data) {}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIconTexture(UIDrawer* drawer, ResourceView texture, Color color) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = drawer->GetRelativeTransformFactorsZoomed(icon_size);
	config.AddFlag(transform);

	drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM, config, texture, color);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIcon(UIDrawer* drawer, Stream<wchar_t> path, Color color) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = drawer->GetRelativeTransformFactorsZoomed(icon_size);
	config.AddFlag(transform);

	drawer->SpriteRectangle(UI_CONFIG_RELATIVE_TRANSFORM, config, path, color);
}

// ----------------------------------------------------------------------------------------------------------------------------

// A double sprite icon
void InspectorIconDouble(UIDrawer* drawer, Stream<wchar_t> icon0, Stream<wchar_t> icon1, Color icon_color0, Color icon_color1) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = drawer->GetRelativeTransformFactorsZoomed(icon_size);
	config.AddFlag(transform);

	drawer->SpriteRectangleDouble(UI_CONFIG_RELATIVE_TRANSFORM, config, icon0, icon1, icon_color0, icon_color1);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIconNameAndPath(UIDrawer* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	ECS_TEMP_ASCII_STRING(ascii_path, 256);
	function::ConvertWideCharsToASCII(path, ascii_path);
	Stream<char> folder_name = function::PathStem(ascii_path);
	drawer->TextLabel(UI_CONFIG_ALIGN_TO_ROW_Y | UI_CONFIG_LABEL_TRANSPARENT, config, folder_name);
	drawer->NextRow();

	UIDrawerSentenceFitSpaceToken fit_space_token;
	fit_space_token.token = '\\';
	config.AddFlag(fit_space_token);
	drawer->Sentence(UI_CONFIG_SENTENCE_FIT_SPACE | UI_CONFIG_SENTENCE_FIT_SPACE_TOKEN, config, ascii_path);
	drawer->NextRow(2.0f);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorShowButton(UIDrawer* drawer, Stream<wchar_t> path) {
	drawer->Button("Show", { LaunchFileExplorerStreamAction, &path, sizeof(path) });
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorOpenAndShowButton(UIDrawer* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	drawer->Button("Open", { OpenFileWithDefaultApplicationStreamAction, &path, sizeof(path) });

	UIConfigAbsoluteTransform transform;
	transform.scale = drawer->GetLabelScale("Show");
	transform.position = drawer->GetAlignedToRight(transform.scale.x);
	config.AddFlag(transform);
	drawer->Button(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE, config, "Show", { LaunchFileExplorerStreamAction, &path, sizeof(path) });
	drawer->NextRow();
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawFileTimesInternal(UIDrawer* drawer, const char* creation_time, const char* write_time, const char* access_time, bool success) {
	UIDrawConfig config;
	// Display file times
	if (success) {
		drawer->Text("Creation time: ");
		drawer->Text(creation_time);
		drawer->NextRow();

		drawer->Text("Access time: ");
		drawer->Text(access_time);
		drawer->NextRow();

		drawer->Text("Last write time: ");
		drawer->Text(write_time);
		drawer->NextRow();
	}
	// Else error message
	else {
		drawer->Text("Could not retrieve directory times.");
		drawer->NextRow();
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawFileTimes(UIDrawer* drawer, Stream<wchar_t> path) {
	char creation_time[256];
	char write_time[256];
	char access_time[256];
	bool success = OS::GetFileTimes(path, creation_time, access_time, write_time);

	InspectorDrawFileTimesInternal(drawer, creation_time, write_time, access_time, success);
}


// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetMatchingIndexFromRobin(EditorState* editor_state, unsigned int target_sandbox) {
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		unsigned int current_inspector = (editor_state->inspector_manager.round_robin_index[target_sandbox] + index) % editor_state->inspector_manager.data.size;
		if (editor_state->inspector_manager.data[current_inspector].target_sandbox == target_sandbox && !IsInspectorLocked(editor_state, current_inspector)) {
			editor_state->inspector_manager.round_robin_index[target_sandbox] = current_inspector + 1 == editor_state->inspector_manager.data.size ?
				0 : current_inspector + 1;
			return current_inspector;
		}
	}

	return -1;
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetMatchingIndexFromRobin(EditorState* editor_state) {
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		unsigned int current_inspector = (editor_state->inspector_manager.round_robin_index[editor_state->sandboxes.size] + index) % editor_state->inspector_manager.data.size;
		if (!IsInspectorLocked(editor_state, current_inspector)) {
			editor_state->inspector_manager.round_robin_index[editor_state->sandboxes.size] = current_inspector + 1 == editor_state->inspector_manager.data.size ?
				0 : current_inspector + 1;
			return current_inspector;
		}
	}

	return -1;
}

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
	unsigned int sandbox_index
)
{
	EDITOR_STATE(editor_state);

	if (sandbox_index != -1) {
		inspector_index = GetMatchingIndexFromRobin(editor_state, sandbox_index);
	}
	else if (inspector_index == -1) {
		inspector_index = GetMatchingIndexFromRobin(editor_state);
	}

	if (inspector_index != -1) {
		InspectorData* inspector_data = editor_state->inspector_manager.data.buffer + inspector_index;
		inspector_data->clean_function(editor_state, inspector_data->draw_data);

		if (inspector_data->data_size > 0) {
			editor_allocator->Deallocate(inspector_data->draw_data);
		}
		inspector_data->draw_data = function::CopyNonZero(editor_allocator, data, data_size);
		inspector_data->draw_function = functions.draw_function;
		inspector_data->clean_function = functions.clean_function;
		inspector_data->data_size = data_size;

		// Also make this window the focused one
		ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
		GetInspectorName(inspector_index, window_name);
		unsigned int inspector_window_index = GetInspectorIndex(window_name);

		DockspaceType type = DockspaceType::Horizontal;
		unsigned int border_index = 0;
		UIDockspace* dockspace = editor_state->ui_system->GetDockspaceFromWindow(inspector_window_index, border_index, type);
		editor_state->ui_system->SetActiveWindowForDockspaceBorder(dockspace, border_index, inspector_window_index);
	}

	return inspector_index;
}

// ----------------------------------------------------------------------------------------------------------------------------

void* GetInspectorDrawFunctionData(EditorState* editor_state, unsigned int inspector_index) {
	return editor_state->inspector_manager.data[inspector_index].draw_data;
}

// ----------------------------------------------------------------------------------------------------------------------------

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorFunctions& function, Stream<wchar_t> _identifier) {
	ResourceIdentifier identifier(_identifier.buffer, _identifier.size * sizeof(wchar_t));

	return editor_state->inspector_manager.function_table.TryGetValue(identifier, function);
}

// ----------------------------------------------------------------------------------------------------------------------------

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorFunctions& function, const wchar_t* _identifier) {
	return TryGetInspectorTableFunction(editor_state, function, _identifier);
}

// ----------------------------------------------------------------------------------------------------------------------------

void AddInspectorTableFunction(InspectorTable* table, InspectorFunctions function, const wchar_t* _identifier) {
	ResourceIdentifier identifier(_identifier);

	ECS_ASSERT(table->Find(identifier) == -1);
	ECS_ASSERT(!table->Insert(function, identifier));
}

// ----------------------------------------------------------------------------------------------------------------------------
