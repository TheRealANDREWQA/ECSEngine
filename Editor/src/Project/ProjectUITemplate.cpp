#include "editorpch.h"
#include "ProjectUITemplate.h"
#include "../Editor/EditorState.h"
#include "../Sandbox/SandboxAccessor.h"
#include "ProjectOperations.h"

#include "../UI/DirectoryExplorer.h"
#include "../UI/FileExplorer.h"
#include "../UI/DirectoryExplorer.h"
#include "../UI/ToolbarUI.h"
#include "../UI/MiscellaneousBar.h"
#include "../UI/ModuleExplorer.h"
#include "../UI/Inspector.h"
#include "../UI/NotificationBar.h"
#include "../UI/Backups.h"
#include "../UI/SandboxExplorer.h"
#include "../UI/Game.h"
#include "../UI/Scene.h"
#include "../UI/EntitiesUI.h"
#include "../UI/AssetExplorer.h"
#include "../UI/VisualizeTexture.h"

using namespace ECSEngine;
ECS_TOOLS;

// -------------------------------------------------------------------------------------------------------

void CreateProjectDefaultUI(EditorState* editor_state) {
	UISystem* ui_system = editor_state->ui_system;
	ui_system->Clear();

	CreateToolbarUI(editor_state);
	CreateMiscellaneousBar(editor_state);
	CreateNotificationBar(editor_state);
	UIDockspace* main_dockspace = CreateProjectBackgroundDockspace(ui_system);
	unsigned int viewport_window_index = CreateSandboxExplorerWindow(editor_state);
	ui_system->AddWindowToDockspaceRegion(viewport_window_index, main_dockspace, 0);
	CreateDirectoryExplorer(editor_state);
	CreateFileExplorer(editor_state);
}

// -------------------------------------------------------------------------------------------------------

UIDockspace* CreateProjectBackgroundDockspace(UISystem* system)
{
	float2 dockspace_position = { -1.0f, -1.0f + TOOLBAR_SIZE_Y + MISCELLANEOUS_BAR_SIZE_Y };
	float2 dockspace_scale = { 2.0f - system->GetPixelSizeX(), 1.0f - dockspace_position.y - NOTIFICATION_BAR_WINDOW_SIZE };
	unsigned int dockspace_index = system->CreateFixedDockspace({ dockspace_position, dockspace_scale }, DockspaceType::FloatingVertical, 0, false, UI_DOCKSPACE_BACKGROUND);
	UIDockspace* dockspace = system->GetDockspace(dockspace_index, DockspaceType::FloatingVertical);

	system->RemoveWindowFromDockspaceRegion<false>(dockspace, DockspaceType::FloatingVertical, 0, 0);
	return dockspace;
}

// -------------------------------------------------------------------------------------------------------

bool OpenProjectUI(ProjectOperationData data)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, template_path, 256);
	GetProjectCurrentUI(template_path, data.file_data);
	return LoadProjectUITemplate(data.editor_state, { template_path }, data.error_message);
}

// -------------------------------------------------------------------------------------------------------

void OpenProjectUIAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;
	OpenProjectUI(*(ProjectOperationData*)_data);
}

// -------------------------------------------------------------------------------------------------------

bool SaveProjectUI(ProjectOperationData data)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, template_path, 256);
	GetProjectCurrentUI(template_path, data.file_data);
	return SaveProjectUITemplate(data.editor_state->ui_system, { template_path }, data.error_message);
}

// -------------------------------------------------------------------------------------------------------

void SaveProjectUIAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;
	SaveProjectUI(*(ProjectOperationData*)_data);
}
// -------------------------------------------------------------------------------------------------------

bool SaveCurrentProjectUI(EditorState* editor_state) {
	return SaveCurrentProjectConverted(editor_state, SaveProjectUI);
}

// -------------------------------------------------------------------------------------------------------

void SaveCurrentProjectUIAction(ActionData* action_data) {
	SaveCurrentProjectUI((EditorState*)action_data->data);
}

// -------------------------------------------------------------------------------------------------------

typedef void (*SetDescriptorFunction)(UIWindowDescriptor&, EditorState*, CapacityStream<void>*);

unsigned int VerifyIndexedWindowIndexOnly(
	Stream<char> file_window_name,
	const char* base_name,
	unsigned int max_search
) {
	ECS_STACK_CAPACITY_STREAM(char, window_name, 256);
	Stream<char> base_name_stream = base_name;

	if (memcmp(file_window_name.buffer, base_name, base_name_stream.size) == 0) {
		// At the moment check for at max 
		for (unsigned int index = 0; index < max_search; index++) {
			window_name.size = 0;
			window_name.CopyOther(base_name_stream);
			ConvertIntToChars(window_name, index);

			if (window_name == file_window_name) {
				return index;
			}
		}
	}

	return -1;
}

static void CreateIndexedWindow(
	unsigned int index,
	Stream<char> window_name,
	UIWindowDescriptor& descriptor,
	EditorState* editor_state,
	CapacityStream<void>* stack_memory,
	SetDescriptorFunction set_descriptor
) {
	unsigned int* window_index = stack_memory->Reserve<unsigned int>();
	*window_index = index;
	set_descriptor(descriptor, editor_state, stack_memory);
	editor_state->ui_system->RestoreWindow(window_name, descriptor);
}

// Returns the index that matched, or -1 if no index matched
static unsigned int VerifyIndexedWindow(
	UIWindowDescriptor& descriptor,
	EditorState* editor_state,
	Stream<char> file_window_name, 
	const char* base_name, 
	unsigned int max_search,
	CapacityStream<void>* stack_memory,
	SetDescriptorFunction set_descriptor
) {
	ECS_STACK_CAPACITY_STREAM(char, window_name, 256);
	Stream<char> base_name_stream = base_name;

	if (memcmp(file_window_name.buffer, base_name, base_name_stream.size) == 0) {
		// At the moment check for at max 
		for (unsigned int index = 0; index < max_search; index++) {
			window_name.size = 0;
			window_name.CopyOther(base_name_stream);
			ConvertIntToChars(window_name, index);

			if (window_name == file_window_name) {
				CreateIndexedWindow(index, window_name, descriptor, editor_state, stack_memory, set_descriptor);
				return index;
			}
		}
	}

	return -1;
}

bool LoadProjectUITemplate(EditorState* editor_state, ProjectUITemplate _template, CapacityStream<char>& error_message)
{
	UISystem* ui_system = editor_state->ui_system;

	ECS_STACK_CAPACITY_STREAM(Stream<char>, file_window_names, 64);
	bool success = ui_system->LoadUIFile(_template.ui_file, file_window_names);
	if (!success) {
		if (error_message.buffer != nullptr) {
			FormatString(error_message, "Error when loading Project UI template: {#} does not exist or it is corrupted!", _template.ui_file);
		}
		return false;
	}
	else {
		// The inspector is handled separately - because multiple instances can be instanced
		Stream<char> _window_names[] = {
			TOOLBAR_WINDOW_NAME,
			MISCELLANEOUS_BAR_WINDOW_NAME,
			NOTIFICATION_BAR_WINDOW_NAME,
			CONSOLE_WINDOW_NAME,
			DIRECTORY_EXPLORER_WINDOW_NAME,
			FILE_EXPLORER_WINDOW_NAME,
			MODULE_EXPLORER_WINDOW_NAME,
			SANDBOX_EXPLORER_WINDOW_NAME,
			ASSET_EXPLORER_WINDOW_NAME,
			BACKUPS_WINDOW_NAME,
			ENTITIES_UI_WINDOW_NAME,
			GAME_WINDOW_NAME,
			SCENE_WINDOW_NAME
		};

		Stream<Stream<char>> window_names(_window_names, std::size(_window_names));
		// The inspector is handled separately - because multiple instances can be instanced
		SetDescriptorFunction set_functions[] = {
			ToolbarSetDescriptor,
			MiscellaneousBarSetDescriptor,
			NotificationBarSetDescriptor,
			ConsoleSetDescriptor,
			DirectoryExplorerSetDescriptor,
			FileExplorerSetDescriptor,
			ModuleExplorerSetDescriptor,
			SandboxExplorerSetDescriptor,
			AssetExplorerSetDecriptor,
			BackupsWindowSetDescriptor,
			EntitiesUISetDescriptor,
			GameSetDecriptor,
			SceneUISetDecriptor
		};

		for (size_t index = 0; index < file_window_names.size; index++) {
			UIWindowDescriptor descriptor;
			unsigned int in_index = FindString(file_window_names[index], window_names);

			ECS_STACK_VOID_STREAM(stack_memory, ECS_KB * 4);
			if (in_index != (unsigned int)-1) {
				set_functions[in_index](descriptor, editor_state, &stack_memory);
				ui_system->RestoreWindow(window_names[in_index], descriptor);

				set_functions[in_index] = set_functions[window_names.size - 1];
				window_names.RemoveSwapBack(in_index);
			}
			else {
				// Check for the indexed windows
				auto get_indexed_window = [&](const char* window_name, unsigned int max_count, SetDescriptorFunction function) {
					return VerifyIndexedWindow(descriptor, editor_state, file_window_names[index], window_name, max_count, &stack_memory, function);
				};

				// Start with the inspector
				unsigned int matched_index = get_indexed_window(INSPECTOR_WINDOW_NAME, MAX_INSPECTOR_WINDOWS, InspectorSetDescriptor);
				if (matched_index != -1) {
					while (editor_state->inspector_manager.data.size <= matched_index) {
						CreateInspectorInstance(editor_state);
					}
					continue;
				}

				// We need to prevent creating UI scenes and game windows if their respective sandbox doesn't exist
				unsigned int sandbox_count = GetSandboxCount(editor_state, true);

				// Now the game/scene
				matched_index = get_indexed_window(GAME_WINDOW_NAME, min<unsigned int>(sandbox_count, MAX_GAME_WINDOWS), GameSetDecriptor);
				if (matched_index != -1) {
					continue;
				}

				matched_index = get_indexed_window(SCENE_WINDOW_NAME, min<unsigned int>(sandbox_count, MAX_SCENE_WINDOWS), SceneUISetDecriptor);
				if (matched_index != -1) {
					continue;
				}

				// If this fails as well, then check for entitites UI
				matched_index = get_indexed_window(ENTITIES_UI_WINDOW_NAME, sandbox_count, EntitiesUISetDescriptor);
				if (matched_index != -1) {
					continue;
				}

				// The test for visualize texture needs to be done separately
				if (file_window_names[index].StartsWith(VISUALIZE_TEXTURE_WINDOW_NAME)) {
					unsigned int visualize_index = ConvertCharactersToInt(file_window_names[index]);
					CreateIndexedWindow(visualize_index, file_window_names[index], descriptor, editor_state, &stack_memory, VisualizeTextureUISetDecriptor);
					unsigned int ui_index = editor_state->ui_system->GetWindowFromName(file_window_names[index]);
					TransitionVisualizeTextureWindowSelection(editor_state->ui_system, ui_index);
					continue;
				}
			}
		}
		ui_system->RemoveUnrestoredWindows();
	}

	// Destroy invalid windows - like Scene and Game with invalid sandbox index
	DestroyInvalidGameUIWindows(editor_state);
	DestroyInvalidSceneUIWindows(editor_state);
	return true;
}

// -------------------------------------------------------------------------------------------------------

void LoadProjectUITemplateSystemHandler(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
	LoadProjectUITemplateData* data = (LoadProjectUITemplateData*)_data;
	bool success = LoadProjectUITemplate(data->editor_state, data->ui_template, error_message);

	if (!success) {
		CreateErrorMessageWindow(system, error_message);
	}
}

// -------------------------------------------------------------------------------------------------------

void LoadProjectUITemplateAction(ActionData* action_data)
{
	UI_UNPACK_ACTION_DATA;

	ECS_STACK_VOID_STREAM(stack_memory, ECS_KB * 2);
	LoadProjectUITemplateData* data = (LoadProjectUITemplateData*)_data;
	ECS_ASSERT(data->ui_template.ui_file.size < 500);
	system->AddFrameHandler({ LoadProjectUITemplateSystemHandler, stack_memory.buffer, (unsigned int)(sizeof(LoadProjectUITemplateData) + sizeof(wchar_t) * data->ui_template.ui_file.size) });
	LoadProjectUITemplateData* handler_data = (LoadProjectUITemplateData*)system->GetFrameHandlerData(system->GetFrameHandlerCount() - 1);
	handler_data->editor_state = data->editor_state;
	handler_data->ui_template.ui_file.InitializeFromBuffer(OffsetPointer(handler_data, sizeof(LoadProjectUITemplateData)), data->ui_template.ui_file.size, data->ui_template.ui_file.size);
	memcpy(handler_data->ui_template.ui_file.buffer, data->ui_template.ui_file.buffer, sizeof(wchar_t) * data->ui_template.ui_file.size);
}

// -------------------------------------------------------------------------------------------------------

bool SaveProjectUITemplate(UISystem* system, ProjectUITemplate _template, CapacityStream<char>& error_message) {
	if (_template.ui_file.size < _template.ui_file.capacity) {
		_template.ui_file[_template.ui_file.size] = L'\0';
	}
	else {
		ECS_STACK_CAPACITY_STREAM(wchar_t, temp_string, 256);
		temp_string.CopyOther(_template.ui_file);
		temp_string.AddAssert(L'\0');
		_template.ui_file.buffer = temp_string.buffer;
	}

	return system->WriteUIFile(_template.ui_file.buffer, error_message);
}

// -------------------------------------------------------------------------------------------------------

void SaveProjectUITemplateAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SaveProjectUITemplateData* data = (SaveProjectUITemplateData*)_data;
	ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
	bool success = SaveProjectUITemplate(system, data->ui_template, error_message);

	if (!success) {
		CreateErrorMessageWindow(system, error_message);
	}
}

// -------------------------------------------------------------------------------------------------------