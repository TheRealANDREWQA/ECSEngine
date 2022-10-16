#include "editorpch.h"
#include "InspectorUtilities.h"
#include "../Inspector.h"
#include "InspectorTextFile.h"

#include "InspectorMeshFile.h"
#include "InspectorTextureFile.h"

struct EditorState;

constexpr float2 TEXT_FILE_BORDER_OFFSET = { 0.005f, 0.005f };
constexpr float TEXT_FILE_ROW_OFFSET = 1.6f;
constexpr float C_FILE_ROW_OFFSET = 1.25f;

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawBlankFile(EditorState* editor_state, unsigned int inspector_index, void* data, UIDrawer* drawer) {
	const wchar_t* path = (const wchar_t*)data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, drawer->color_theme.text);
	Stream<wchar_t> stream_path = path;
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);
	InspectorOpenAndShowButton(drawer, stream_path);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawTextFileImplementation(EditorState* editor_state, unsigned int inspector_index, void* data, UIDrawer* drawer, const wchar_t* icon_texture, float row_offset_factor) {
	const wchar_t* path = (const wchar_t*)data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, icon_texture, drawer->color_theme.text, drawer->color_theme.theme);
	Stream<wchar_t> stream_path = path;
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);
	InspectorOpenAndShowButton(drawer, stream_path);

	DrawTextFileEx(drawer, path, TEXT_FILE_BORDER_OFFSET, drawer->layout.next_row_y_offset / row_offset_factor);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawTextFile(EditorState* editor_state, unsigned int inspector_index, void* data, UIDrawer* drawer) {
	InspectorDrawTextFileImplementation(editor_state, inspector_index, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_TEXT, TEXT_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawCTextFile(EditorState* editor_state, unsigned int inspector_index, void* data, UIDrawer* drawer) {
	InspectorDrawTextFileImplementation(editor_state, inspector_index, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_C, C_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawCppTextFile(EditorState* editor_state, unsigned int inspector_index, void* data, UIDrawer* drawer) {
	InspectorDrawTextFileImplementation(editor_state, inspector_index, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_CPP, C_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawHlslTextFile(EditorState* editor_state, unsigned int inspector_index, void* data, UIDrawer* drawer) {
	InspectorDrawTextFileImplementation(editor_state, inspector_index, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_SHADER, C_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToTextFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	InspectorFunctions functions;
	Stream<wchar_t> extension = function::PathExtension(path);

	ECS_TEMP_STRING(null_terminated_path, 256);
	null_terminated_path.Copy(path);
	null_terminated_path[null_terminated_path.size] = L'\0';
	if (extension.size == 0 || !TryGetInspectorTableFunction(editor_state, functions, extension)) {
		functions.draw_function = InspectorDrawBlankFile;
		functions.clean_function = InspectorCleanNothing;
	}

	ChangeInspectorDrawFunction(editor_state, inspector_index, functions, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1));
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorTextFileAddFunctors(InspectorTable* table)
{
	AddInspectorTableFunction(table, { InspectorDrawTextFile, InspectorCleanNothing }, L".txt");
	AddInspectorTableFunction(table, { InspectorDrawTextFile, InspectorCleanNothing }, L".md");
	AddInspectorTableFunction(table, { InspectorDrawCppTextFile, InspectorCleanNothing }, L".cpp");
	AddInspectorTableFunction(table, { InspectorDrawCppTextFile, InspectorCleanNothing }, L".h");
	AddInspectorTableFunction(table, { InspectorDrawCppTextFile, InspectorCleanNothing }, L".hpp");
	AddInspectorTableFunction(table, { InspectorDrawCTextFile, InspectorCleanNothing }, L".c");
	AddInspectorTableFunction(table, { InspectorDrawHlslTextFile, InspectorCleanNothing }, L".hlsl");
	AddInspectorTableFunction(table, { InspectorDrawHlslTextFile, InspectorCleanNothing }, L".hlsli");
}

// ----------------------------------------------------------------------------------------------------------------------------
