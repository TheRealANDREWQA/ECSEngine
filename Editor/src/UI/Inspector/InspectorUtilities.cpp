#include "editorpch.h"
#include "InspectorUtilities.h"
#include "ECSEngineUI.h"
#include "../Inspector.h"
#include "../../Editor/EditorState.h"
#include "../../Sandbox/SandboxAccessor.h"
#include "../FileExplorerData.h"

using namespace ECSEngine;
ECS_TOOLS;

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawNothing(EditorState* editor_state, unsigned int inspector_index, void* data, UIDrawer* drawer) {
	UIDrawConfig config;
	drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, "Nothing is selected.");
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorCleanNothing(EditorState* editor_state, unsigned int inspector_index, void* data) {}

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

	ECS_STACK_CAPACITY_STREAM(char, ascii_path, 256);
	ConvertWideCharsToASCII(path, ascii_path);
	Stream<char> folder_name = PathStem(ascii_path);
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

struct HighlightPathActionData {
	EditorState* editor_state;
	Stream<wchar_t> path;
};

static void HighlightPathAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	HighlightPathActionData* data = (HighlightPathActionData*)_data;
	ChangeFileExplorerFile(data->editor_state, data->path);
}

void InspectorDefaultInteractButtons(EditorState* editor_state, UIDrawer* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	const char* OPEN_LABEL = "Open";
	const char* HIGHLIGHT_LABEL = "Highlight";
	const char* SHOW_LABEL = "Show";

	UIDrawerRowLayout row_layout = drawer->GenerateRowLayout();
	row_layout.AddLabel(OPEN_LABEL);
	row_layout.AddLabel(HIGHLIGHT_LABEL, ECS_UI_ALIGN_MIDDLE);
	row_layout.AddLabel(SHOW_LABEL, ECS_UI_ALIGN_RIGHT);

	size_t configuration = 0;
	row_layout.GetTransform(config, configuration);
	drawer->Button(configuration, config, OPEN_LABEL, { OpenFileWithDefaultApplicationStreamAction, &path, sizeof(path) });
	configuration = 0;
	config.flag_count = 0;

	row_layout.GetTransform(config, configuration);
	// We can reference the path directly
	HighlightPathActionData highlight_data = { editor_state };
	highlight_data.path = path;
	drawer->Button(configuration, config, HIGHLIGHT_LABEL, { HighlightPathAction, &highlight_data, sizeof(highlight_data) });
	configuration = 0;
	config.flag_count = 0;

	row_layout.GetTransform(config, configuration);
	drawer->Button(configuration, config, "Show", { LaunchFileExplorerStreamAction, &path, sizeof(path) });
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
	// Look for an inspector that doesn't have a function assigned (i.e. the InspectorNothing function) and use that one
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		bool matches_sandbox = target_sandbox == -1 || DoesInspectorMatchSandbox(editor_state, index, target_sandbox);
		if (matches_sandbox && GetInspectorDrawFunction(editor_state, index) == InspectorDrawNothing) {
			return index;
		}
	}

	unsigned int round_robing_index = target_sandbox == -1 ? GetSandboxCount(editor_state) : target_sandbox;
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		unsigned int current_inspector = (editor_state->inspector_manager.round_robin_index[round_robing_index] + index) % editor_state->inspector_manager.data.size;
		bool matches_sandbox = target_sandbox == -1 || DoesInspectorMatchSandbox(editor_state, current_inspector, target_sandbox);
		if (matches_sandbox && !IsInspectorLocked(editor_state, current_inspector)) {
			editor_state->inspector_manager.round_robin_index[round_robing_index] = current_inspector + 1 == editor_state->inspector_manager.data.size ?
				0 : current_inspector + 1;
			return current_inspector;
		}
	}

	return -1;
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetInspectorUIWindowIndex(const EditorState* editor_state, unsigned int inspector_index)
{
	ECS_STACK_CAPACITY_STREAM(char, inspector_name, 128);
	GetInspectorName(inspector_index, inspector_name);
	return editor_state->ui_system->GetWindowFromName(inspector_name);
}

// ----------------------------------------------------------------------------------------------------------------------------

void GetInspectorsForMatchingSandbox(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* inspector_indices)
{
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (DoesInspectorMatchSandbox(editor_state, index, sandbox_index)) {
			inspector_indices->AddAssert(index);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

InspectorDrawFunction GetInspectorDrawFunction(const EditorState* editor_state, unsigned int inspector_index)
{
	return editor_state->inspector_manager.data[inspector_index].draw_function;
}

// ----------------------------------------------------------------------------------------------------------------------------

void* GetInspectorDrawFunctionData(EditorState* editor_state, unsigned int inspector_index) {
	return (void*)GetInspectorDrawFunctionData((const EditorState*)editor_state, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

const void* GetInspectorDrawFunctionData(const EditorState* editor_state, unsigned int inspector_index) {
	return editor_state->inspector_manager.data[inspector_index].draw_data;
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
	unsigned int sandbox_index,
	bool do_not_push_target_entry
)
{
	AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
	if (inspector_index == -1) {
		inspector_index = GetMatchingIndexFromRobin(editor_state, sandbox_index);
	}

	if (inspector_index != -1) {
		InspectorData* inspector_data = editor_state->inspector_manager.data.buffer + inspector_index;
		inspector_data->clean_function(editor_state, inspector_index, inspector_data->draw_data);

		// Also make this window the focused one
		ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
		GetInspectorName(inspector_index, window_name);
		unsigned int inspector_window_index = editor_state->ui_system->GetWindowFromName(window_name);

		// When this is firstly created, the window index will be -1 since there is no window at this point
		if (inspector_window_index != -1) {
			editor_state->ui_system->DeallocateWindowDynamicResources(inspector_window_index);
			// Change the retained function only when the window is already created
			SetInspectorRetainedMode(editor_state, inspector_index, functions.retained_function);
		}

		if (inspector_data->data_size > 0) {
			Deallocate(editor_allocator, inspector_data->draw_data);
		}
		inspector_data->draw_data = CopyNonZero(editor_allocator, data, data_size);
		inspector_data->draw_function = functions.draw_function;
		inspector_data->clean_function = functions.clean_function;
		inspector_data->data_size = data_size;
		// Also change the target sandbox index
		// In case it is -1, use the matching sandbox value
		inspector_data->target_sandbox = sandbox_index == -1 ? GetInspectorMatchingSandbox(editor_state, inspector_index) : sandbox_index;

		// Same as the comment above, the window is firstly created
		if (inspector_window_index != -1) {
			editor_state->ui_system->SetActiveWindowInBorder(inspector_window_index);
		}

		if (!do_not_push_target_entry) {
			PushInspectorTarget(editor_state, inspector_index, functions, data, data_size, sandbox_index);
		}
	}

	return inspector_index;
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int FindInspectorWithDrawFunction(
	const EditorState* editor_state, 
	InspectorDrawFunction draw_function, 
	unsigned int sandbox_index
)
{
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (editor_state->inspector_manager.data[index].draw_function == draw_function) {
			if (sandbox_index != -1) {
				if (DoesInspectorMatchSandbox(editor_state, index, sandbox_index)) {
					return index;
				}
			}
			else {
				return index;
			}
		}
	}
	return -1;
}

// ----------------------------------------------------------------------------------------------------------------------------

void FindInspectorWithDrawFunction(
	const EditorState* editor_state, 
	InspectorDrawFunction draw_function, 
	CapacityStream<unsigned int>* inspector_indices, 
	unsigned int sandbox_index
)
{
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (editor_state->inspector_manager.data[index].draw_function == draw_function) {
			if (sandbox_index != -1) {
				if (DoesInspectorMatchSandbox(editor_state, index, sandbox_index)) {
					inspector_indices->AddAssert(index);
				}
			}
			else {
				inspector_indices->AddAssert(index);
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorFunctions& function, Stream<wchar_t> _identifier) {
	ResourceIdentifier identifier(_identifier.buffer, _identifier.size * sizeof(wchar_t));

	return editor_state->inspector_manager.function_table.TryGetValue(identifier, function);
}

// ----------------------------------------------------------------------------------------------------------------------------

void AddInspectorTableFunction(InspectorTable* table, InspectorFunctions function, Stream<wchar_t> _identifier) {
	ResourceIdentifier identifier(_identifier);

	ECS_ASSERT(table->Find(identifier) == -1);
	ECS_ASSERT(!table->Insert(function, identifier));
}

// ----------------------------------------------------------------------------------------------------------------------------
