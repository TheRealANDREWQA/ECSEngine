#include "editorpch.h"
#include "InspectorUtilities.h"
#include "../Inspector.h"
#include "InspectorTextFile.h"
#include "../../Editor/EditorState.h"

#include "InspectorMeshFile.h"
#include "InspectorTextureFile.h"

struct EditorState;

constexpr float2 TEXT_FILE_BORDER_OFFSET = { 0.005f, 0.005f };
constexpr float TEXT_FILE_ROW_OFFSET = 1.4f;
constexpr float C_FILE_ROW_OFFSET = 1.2f;

// ----------------------------------------------------------------------------------------------------------------------------

static bool InspectorDrawBlankFileRetainedMode(void* window_data, WindowRetainedModeInfo* info) {
	// At the moment, always return true for this. The only information that gets lost
	// Is the file times but those are not all that relevant anyway
	return true;
}

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
	InspectorDefaultInteractButtons(editor_state, drawer, stream_path);
}

// ----------------------------------------------------------------------------------------------------------------------------

static bool InspectorDrawTextRetainedMode(void* window_data, WindowRetainedModeInfo* info) {
	TextFileDrawData* draw_data = (TextFileDrawData*)window_data;
	// This retained mode function needs only the draw data, which at the moment
	// is identical to our current window data
	return TextFileWindowRetainedMode(draw_data, info);
}

static void InspectorDrawTextFileCleanFunction(EditorState* editor_state, unsigned int inspector_index, void* window_data) {
	TextFileDrawData* draw_data = (TextFileDrawData*)window_data;
	DeallocateTextFileDrawData(draw_data, editor_state->EditorAllocator());
}

static void InspectorDrawTextFileImplementation(
	EditorState* editor_state, 
	unsigned int inspector_index, 
	void* _data, 
	UIDrawer* drawer, 
	const wchar_t* icon_texture, 
	float row_offset_factor
) {
	TextFileDrawData* data = (TextFileDrawData*)_data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(data->path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, icon_texture);
	InspectorIconNameAndPath(drawer, data->path);
	InspectorDrawFileTimes(drawer, data->path);
	InspectorDefaultInteractButtons(editor_state, drawer, data->path);

	data->next_row_y_offset = drawer->layout.next_row_y_offset * row_offset_factor;
	DrawTextFile(drawer, data);
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
	Stream<wchar_t> extension = PathExtension(path);

	if (extension.size == 0 || !TryGetInspectorTableFunction(editor_state, functions, extension)) {
		functions.draw_function = InspectorDrawBlankFile;
		functions.clean_function = InspectorCleanNothing;
		functions.retained_function = InspectorDrawBlankFileRetainedMode;
	}

	if (functions.draw_function != InspectorDrawBlankFile) {
		functions.retained_function = InspectorDrawTextRetainedMode;

		TextFileDrawData draw_data;
		draw_data.border_padding = TEXT_FILE_BORDER_OFFSET;
		draw_data.timer.SetUninitialized();
		uint3 inspector_indices = ChangeInspectorDrawFunctionWithSearchEx(
			editor_state,
			inspector_index,
			functions,
			&draw_data,
			sizeof(draw_data),
			-1,
			[=](void* inspector_data) {
				TextFileDrawData* other_data = (TextFileDrawData*)inspector_data;
				return other_data->path == path;
			}
		);

		if (inspector_indices.y != -1) {
			TextFileDrawData* inspector_data = (TextFileDrawData*)GetInspectorDrawFunctionData(editor_state, inspector_indices.y);
			struct InitializeData {
				Stream<wchar_t> path;
			};

			AllocatorPolymorphic initialize_allocator = GetLastInspectorTargetInitializeAllocator(editor_state, inspector_indices.y);
			InitializeData initialize_data;
			initialize_data.path = path.Copy(initialize_allocator);

			auto initialize = [](EditorState* editor_state, void* data, void* _initialize_data, unsigned int inspector_index) {
				TextFileDrawData* inspector_data = (TextFileDrawData*)data;
				InitializeData* initialize_data = (InitializeData*)_initialize_data;
				inspector_data->path = initialize_data->path.Copy(editor_state->EditorAllocator());
			};

			SetLastInspectorTargetInitialize(editor_state, inspector_indices.y, initialize, &initialize_data, sizeof(initialize_data));
		}
	}
	else {
		ECS_STACK_CAPACITY_STREAM(wchar_t, null_terminated_path, 512);
		null_terminated_path.CopyOther(path);
		null_terminated_path.AddAssert(L'\0');
		ChangeInspectorDrawFunctionWithSearch(editor_state, inspector_index, functions, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1), -1, 
			[=](void* inspector_data) {
				const wchar_t* other_data = (const wchar_t*)inspector_data;
				return other_data == path;
		});
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToBlankFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, null_terminated_path, 512);
	null_terminated_path.CopyOther(path);
	null_terminated_path.AddAssert(L'\0');
	ChangeInspectorDrawFunctionWithSearch(editor_state, inspector_index, { InspectorDrawBlankFile, InspectorCleanNothing }, null_terminated_path.buffer,
		sizeof(wchar_t) * (path.size + 1), -1, [=](void* inspector_data) {
			const wchar_t* other_data = (const wchar_t*)inspector_data;
			return other_data == path;
	});
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorTextFileAddFunctors(InspectorTable* table)
{
	AddInspectorTableFunction(table, { InspectorDrawTextFile, InspectorDrawTextFileCleanFunction }, L".txt");
	AddInspectorTableFunction(table, { InspectorDrawTextFile, InspectorDrawTextFileCleanFunction }, L".md");
	AddInspectorTableFunction(table, { InspectorDrawCppTextFile, InspectorDrawTextFileCleanFunction }, L".cpp");
	AddInspectorTableFunction(table, { InspectorDrawCppTextFile, InspectorDrawTextFileCleanFunction }, L".h");
	AddInspectorTableFunction(table, { InspectorDrawCppTextFile, InspectorDrawTextFileCleanFunction }, L".hpp");
	AddInspectorTableFunction(table, { InspectorDrawCTextFile, InspectorDrawTextFileCleanFunction }, L".c");
	AddInspectorTableFunction(table, { InspectorDrawHlslTextFile, InspectorDrawTextFileCleanFunction }, ECS_SHADER_EXTENSION);
	AddInspectorTableFunction(table, { InspectorDrawHlslTextFile, InspectorDrawTextFileCleanFunction }, ECS_SHADER_INCLUDE_EXTENSION);
}

// ----------------------------------------------------------------------------------------------------------------------------

bool IsInspectorTextFileDraw(const EditorState* editor_state, unsigned int inspector_index) {
	return IsInspectorTextFileDraw(GetInspectorDrawFunction(editor_state, inspector_index));
}

bool IsInspectorTextFileDraw(InspectorDrawFunction draw_function) {
	return draw_function == InspectorDrawTextFile || draw_function == InspectorDrawCppTextFile ||
		draw_function == InspectorDrawCTextFile || draw_function == InspectorDrawHlslTextFile;
}

// ----------------------------------------------------------------------------------------------------------------------------
