#include "Inspector.h"
#include "InspectorData.h"
#include "../Editor/EditorState.h"
#include "../HelperWindows.h"
#include "../OSFunctions.h"
#include "../Editor/EditorEvent.h"
#include "FileExplorer.h"
#include "../Modules/Module.h"

using namespace ECSEngine;
ECS_CONTAINERS;
ECS_TOOLS;

constexpr float2 WINDOW_SIZE = float2(0.5f, 1.2f);
#define INSPECTOR_DATA_NAME "Inspector Data"

constexpr float ICON_SIZE = 0.07f;

constexpr size_t HASH_TABLE_CAPACITY = 32;
constexpr float2 TEXT_FILE_BORDER_OFFSET = { 0.005f, 0.005f };
constexpr float TEXT_FILE_ROW_OFFSET = 0.008f;
constexpr float C_FILE_ROW_OFFSET = 0.015f;

using HashFunction = HashFunctionMultiplyString;

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	descriptor.draw = InspectorWindowDraw<false>;
	descriptor.initialize = InspectorWindowDraw<true>;

	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;
	descriptor.window_name = INSPECTOR_WINDOW_NAME;
}

// ----------------------------------------------------------------------------------------------------------------------------

void AddInspectorTableFunction(InspectorTable* table, InspectorDrawFunction function, const wchar_t* _identifier) {
	ResourceIdentifier identifier(_identifier);

	ECS_ASSERT(table->Find(identifier) == -1);
	ECS_ASSERT(!table->Insert(function, identifier));
}

// ----------------------------------------------------------------------------------------------------------------------------

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorDrawFunction& function, Stream<wchar_t> _identifier) {
	ResourceIdentifier identifier(_identifier.buffer, _identifier.size * sizeof(wchar_t));

	InspectorData* data = (InspectorData*)editor_state->inspector_data;
	return data->table.TryGetValue(identifier, function);
}

// ----------------------------------------------------------------------------------------------------------------------------

bool TryGetInspectorTableFunction(const EditorState* editor_state, InspectorDrawFunction& function, const wchar_t* _identifier) {
	return TryGetInspectorTableFunction(editor_state, function, ToStream(_identifier));
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIcon(UIDrawer<false>* drawer, const wchar_t* path, Color color = ECS_COLOR_WHITE) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = icon_size / drawer->GetStaticElementDefaultScale();
	config.AddFlag(transform);

	drawer->SpriteRectangle<UI_CONFIG_RELATIVE_TRANSFORM>(config, path, color);
}

// ----------------------------------------------------------------------------------------------------------------------------

// A double sprite icon
void InspectorIconDouble(UIDrawer<false>* drawer, const wchar_t* icon0, const wchar_t* icon1, Color icon_color0 = ECS_COLOR_WHITE, Color icon_color1 = ECS_COLOR_WHITE) {
	UIDrawConfig config;
	UIConfigRelativeTransform transform;
	float2 icon_size = drawer->GetSquareScale(ICON_SIZE);
	transform.scale = icon_size / drawer->GetStaticElementDefaultScale();
	config.AddFlag(transform);

	drawer->SpriteRectangleDouble<UI_CONFIG_RELATIVE_TRANSFORM>(config, icon0, icon1, icon_color0, icon_color1);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorIconNameAndPath(UIDrawer<false>* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	Stream<wchar_t> folder_name = function::PathStem(path);
	ECS_TEMP_ASCII_STRING(ascii_path, 256);
	function::ConvertWideCharsToASCII(path.buffer, ascii_path.buffer, path.size + 1, 256);
	drawer->TextLabel<UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT>(config, ascii_path.buffer + (folder_name.buffer - path.buffer));
	drawer->NextRow();
	drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, ascii_path.buffer);
	drawer->NextRow();
	drawer->CrossLine();
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorShowButton(UIDrawer<false>* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	drawer->Button<UI_CONFIG_DO_NOT_CACHE>(config, "Show", { LaunchFileExplorerStreamAction, &path, sizeof(path) });
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorOpenAndShowButton(UIDrawer<false>* drawer, Stream<wchar_t> path) {
	UIDrawConfig config;

	drawer->Button<UI_CONFIG_DO_NOT_CACHE>(config, "Open", { OpenFileWithDefaultApplicationStreamAction, &path, sizeof(path) });

	UIConfigAbsoluteTransform transform;
	transform.scale = drawer->GetLabelScale("Show");
	transform.position = drawer->GetAlignedToRight(transform.scale.x);
	config.AddFlag(transform);
	drawer->Button<UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ABSOLUTE_TRANSFORM>(config, "Show", { LaunchFileExplorerStreamAction, &path, sizeof(path) });
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawFileTimes(UIDrawer<false>* drawer, const wchar_t* path) {
	char creation_time[256];
	char write_time[256];
	char access_time[256];
	bool success = OS::GetFileTimes(path, creation_time, access_time, write_time);

	UIDrawConfig config;
	// Display file times
	if (success) {
		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "Creation time: ");
		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, creation_time);
		drawer->NextRow();

		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "Access time: ");
		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, access_time);
		drawer->NextRow();

		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "Last write time: ");
		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, write_time);
		drawer->NextRow();
	}
	// Else error message
	else {
		drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "Could not retrieve directory times.");
		drawer->NextRow();
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawNothing(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	UIDrawConfig config;
	drawer->Text<UI_CONFIG_DO_NOT_CACHE>(config, "Nothing is selected.");
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawFolderInfo(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	const wchar_t* path = (const wchar_t*)data;

	// Test to see if the folder still exists - if it does not revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state);
		return;
	}

	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FOLDER);

	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);

	struct OpenFolderData {
		EditorState* state;
		const wchar_t* path;
	};

	auto OpenFileExplorerFolder = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		OpenFolderData* data = (OpenFolderData*)_data;
		Stream<wchar_t> path = ToStream(data->path);
		ChangeFileExplorerDirectory(data->state, path);
	};

	UIDrawConfig config;

	config.flag_count = 0;
	OpenFolderData open_data;
	open_data.path = path;
	open_data.state = editor_state;
	drawer->Button<UI_CONFIG_DO_NOT_CACHE>(config, "Open", { OpenFileExplorerFolder, &open_data, sizeof(open_data) });

	UIConfigAbsoluteTransform absolute_transform;
	absolute_transform.scale = drawer->GetLabelScale("Show");
	absolute_transform.position = drawer->GetAlignedToRight(absolute_transform.scale.x);
	config.AddFlag(absolute_transform);
	drawer->Button<UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_ABSOLUTE_TRANSFORM>(config, "Show", { LaunchFileExplorerAction, (void*)path, 0 });
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawTexture(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	const wchar_t* path = (const wchar_t*)data;

	// Check to see if the file still exists - if it doesn't revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state);
		return;
	}

	InspectorIcon(drawer, path);
	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);

	InspectorOpenAndShowButton(drawer, stream_path);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawBlankFile(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	const wchar_t* path = (const wchar_t*)data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state);
		return;
	}

	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, drawer->color_theme.default_text);
	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);
	InspectorOpenAndShowButton(drawer, stream_path);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawTextFileImplementation(EditorState* editor_state, void* data, UIDrawer<false>* drawer, const wchar_t* icon_texture, float row_offset) {
	EDITOR_STATE(editor_state);
	const wchar_t* path = (const wchar_t*)data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state);
		return;
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, icon_texture, drawer->color_theme.default_text, drawer->color_theme.theme);
	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);

	float2* stabilized_render_span = DrawTextFileEx(drawer, path, TEXT_FILE_BORDER_OFFSET, TEXT_FILE_ROW_OFFSET);

	InspectorOpenAndShowButton(drawer, stream_path);
	*stabilized_render_span = drawer->GetRenderSpan();
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawTextFile(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_TEXT, TEXT_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawCTextFile(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_C, C_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawCppTextFile(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_CPP, C_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawHlslTextFile(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	InspectorDrawTextFileImplementation(editor_state, data, drawer, ECS_TOOLS_UI_TEXTURE_FILE_SHADER, C_FILE_ROW_OFFSET);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawMeshFile(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	const wchar_t* path = (const wchar_t*)data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(path)) {
		ChangeInspectorToNothing(editor_state);
		return;
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ECS_TOOLS_UI_TEXTURE_FILE_MESH, drawer->color_theme.default_text, drawer->color_theme.theme);
	Stream<wchar_t> stream_path = ToStream(path);
	InspectorIconNameAndPath(drawer, stream_path);

	InspectorDrawFileTimes(drawer, path);
	InspectorOpenAndShowButton(drawer, stream_path);
}

// ----------------------------------------------------------------------------------------------------------------------------

struct RemoveModuleConfigurationGroupData {
	EditorState* editor_state;
	unsigned int group_index;
	unsigned int library_index;
};

void InspectorDeleteModuleConfigurationGroupAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RemoveModuleConfigurationGroupData* data = (RemoveModuleConfigurationGroupData*)_data;
	RemoveModuleConfigurationGroup(data->editor_state, data->group_index);

	if (data->editor_state->module_configuration_groups.size == 0) {
		DeleteModuleConfigurationGroupFile(data->editor_state);
	}
	else {
		bool success = SaveModuleConfigurationGroupFile(data->editor_state);
		if (!success) {
			EditorSetConsoleError(data->editor_state, ToStream("An error occured during module configuration group file save after deleting a group."));
		}
	}
}

void InspectorDrawModuleConfigurationGroup(EditorState* editor_state, void* data, UIDrawer<false>* drawer) {
	EDITOR_STATE(editor_state);
	bool* header_state = (bool*)data;
	const char* module_configuration_group_name = (const char*)function::OffsetPointer(data, 1);
	UIDrawConfig config;
	const ProjectModules* modules = (const ProjectModules*)editor_state->project_modules;

	// Test if the configuration group still exists - if it doesn't revert to draw nothing
	unsigned int group_index = GetModuleConfigurationGroupIndex(editor_state, ToStream(module_configuration_group_name));
	if (group_index == -1) {
		ChangeInspectorToNothing(editor_state);
		return;
	}
	const ModuleConfigurationGroup* group = editor_state->module_configuration_groups.buffer + group_index;

	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FILE_CONFIG);
	// The group name
	drawer->TextLabel<UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT>(config, module_configuration_group_name);

	// Specify that it is a module configuration group
	drawer->Indent(-2.0f);
	drawer->TextLabel<UI_CONFIG_LABEL_ALIGN_TO_ROW_Y_SIZE | UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_LABEL_TRANSPARENT>(config, " - Module Configuration Group");
	drawer->NextRow();
	drawer->CrossLine();

	// Draw the configurations
	UIConfigCollapsingHeaderDoNotCache header_do_not_cache;
	header_do_not_cache.state = header_state;
	config.AddFlag(header_do_not_cache);
	Stream<const char*> combo_labels = Stream<const char*>(MODULE_CONFIGURATIONS, std::size(MODULE_CONFIGURATIONS));
	unsigned int combo_display_labels = 3;
	
	drawer->CollapsingHeader<UI_CONFIG_DO_NOT_CACHE>(config, "Configurations", [&]() {
		ECS_TEMP_ASCII_STRING(ascii_name, 256);
		for (size_t index = 0; index < group->libraries.size; index++) {
			ascii_name.size = 0;
			function::ConvertWideCharsToASCII(group->libraries[index], ascii_name);
			drawer->ComboBox<UI_CONFIG_DO_NOT_CACHE>(
				config, 
				ascii_name.buffer, 
				combo_labels,
				combo_display_labels,
				(unsigned char*)group->configurations + index
			);

			// Draw the remove button
			UIDrawConfig button_config;

			float window_x_scale = drawer->GetXScaleUntilBorder(drawer->current_x - drawer->region_render_offset.x);
			UIConfigRelativeTransform transform;
			transform.scale = { window_x_scale / drawer->layout.default_element_x, 1.0f };
			button_config.AddFlag(transform);

			// The remove action
			auto remove_callback = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				RemoveModuleConfigurationGroupData* data = (RemoveModuleConfigurationGroupData*)_data;
				ModuleConfigurationGroup* group = data->editor_state->module_configuration_groups.buffer + data->group_index;

				// If there are no libraries left, then ask for approval to destroy the group
				if (group->libraries.size == 1) {
					CreateConfirmWindow(system, ToStream("This action will delete the configuration group. Are you sure?"), { InspectorDeleteModuleConfigurationGroupAction, data, sizeof(*data) });
					return;
				}

				// No need to deallocate and create a new one; there is not a big memory footprint to warrant a deallocate and 
				// a new allocation
				for (size_t index = data->library_index; index <= group->libraries.size; index++) {
					group->configurations[index] = group->configurations[index + 1];
				}
				group->libraries.RemoveSwapBack(data->library_index);
			};

			RemoveModuleConfigurationGroupData remove_data;
			remove_data.editor_state = editor_state;
			remove_data.group_index = group_index;
			remove_data.library_index = index;

			drawer->Button<UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_DO_NOT_CACHE>(
				button_config,
				"Remove",
				{ remove_callback, &remove_data, sizeof(remove_data), UIDrawPhase::System }
			);
			drawer->NextRow();
		}

	});

	drawer->NextRow(2.0f);

	// Draw the add button
	UIDrawConfig button_config;

	UIConfigWindowDependentSize size;
	button_config.AddFlag(size);

	UIConfigActiveState active_state;
	// Check that there is at least a module that can be added
	active_state.state = group->libraries.size < (modules->size - !HasGraphicsModule(editor_state));
	button_config.AddFlag(active_state);

	struct ButtonData {
		EditorState* editor_state;
		unsigned int index;
	};

	// The add sign
	auto add_callback = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		ButtonData* data = (ButtonData*)_data;
		CreateModuleConfigurationGroupAddWizard(data->editor_state, data->index);
	};

	ButtonData add_data;
	add_data.editor_state = editor_state;
	add_data.index = group_index;

	drawer->Button<UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ACTIVE_STATE | UI_CONFIG_DO_NOT_CACHE>(
		button_config,
		"Add",
		{ add_callback, &add_data, sizeof(add_data), UIDrawPhase::System }
	);
	drawer->NextRow();

	// Draw the fallback configuration for the other modules
	drawer->ComboBox<UI_CONFIG_DO_NOT_CACHE>(config, "Fallback configuration", combo_labels, combo_display_labels, (unsigned char*)group->configurations + group->libraries.size);
}

// ----------------------------------------------------------------------------------------------------------------------------

template<bool initialize>
void InspectorWindowDraw(void* window_data, void* drawer_descriptor) {
	UI_PREPARE_DRAWER(initialize);

	EditorState* editor_state = (EditorState*)window_data;
	InspectorData* data = (InspectorData*)editor_state->inspector_data;

	if constexpr (initialize) {
		ChangeInspectorToFile(editor_state, ToStream(L"C:\\Users\\Andrei\\Test\\Assets\\Textures\\ErrorIcon.png"));
	}

	if constexpr (!initialize) {
		if (data->draw_function != nullptr) {
			data->draw_function(editor_state, data->draw_data, &drawer);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateInspectorWindow(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);
	descriptor.initial_size_x = WINDOW_SIZE.x;
	descriptor.initial_size_y = WINDOW_SIZE.y;

	size_t stack_memory[128];
	InspectorSetDescriptor(descriptor, editor_state, stack_memory);

	return ui_system->Create_Window(descriptor);
}

// ----------------------------------------------------------------------------------------------------------------------------

void CreateInspector(EditorState* editor_state) {
	CreateDockspaceFromWindow(INSPECTOR_WINDOW_NAME, editor_state, CreateInspectorWindow);
}

// ----------------------------------------------------------------------------------------------------------------------------

void CreateInspectorAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateInspector((EditorState*)_data);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorDrawFunction(EditorState* editor_state, InspectorDrawFunction draw_function, void* data, size_t data_size)
{
	EDITOR_STATE(editor_state);

	InspectorData* inspector_data = (InspectorData*)editor_state->inspector_data;
	if (inspector_data->data_size > 0) {
		editor_allocator->Deallocate(inspector_data->draw_data);
	}
	inspector_data->draw_data = function::AllocateMemory(editor_allocator, data, data_size);
	inspector_data->draw_function = draw_function;
	inspector_data->data_size = data_size;
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToNothing(EditorState* editor_state) {
	ChangeInspectorDrawFunction(editor_state, InspectorDrawNothing, nullptr, 0);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToFolder(EditorState* editor_state, Stream<wchar_t> path) {
	ECS_TEMP_STRING(null_terminated_path, 256);
	null_terminated_path.Copy(path);
	null_terminated_path[null_terminated_path.size] = L'\0';

	ChangeInspectorDrawFunction(editor_state, InspectorDrawFolderInfo, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1));
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToFile(EditorState* editor_state, Stream<wchar_t> path) {
	InspectorDrawFunction draw_function;
	Stream<wchar_t> extension = function::PathExtension(path);

	ECS_TEMP_STRING(null_terminated_path, 256);
	null_terminated_path.Copy(path);
	null_terminated_path[null_terminated_path.size] = L'\0';
	if (extension.size == 0 || !TryGetInspectorTableFunction(editor_state, draw_function, extension)) {
		draw_function = InspectorDrawBlankFile;
	}
	ChangeInspectorDrawFunction(editor_state, draw_function, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1));
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToModule(EditorState* editor_state, unsigned int index) {

}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToGraphicsModule(EditorState* editor_state) {

}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToModuleConfigurationGroup(EditorState* editor_state, unsigned int index) {
	ECS_ASSERT(index < editor_state->module_configuration_groups.size);

	// First char must be a bool for the header non cached state
	ECS_TEMP_ASCII_STRING(null_terminated_path, 256);
	null_terminated_path[0] = '\0';
	editor_state->module_configuration_groups[index].name.CopyTo(null_terminated_path.buffer + 1);
	// Null terminate the name
	null_terminated_path[editor_state->module_configuration_groups[index].name.size + 1] = '\0';
	ChangeInspectorDrawFunction(
		editor_state, 
		InspectorDrawModuleConfigurationGroup,
		null_terminated_path.buffer, 
		editor_state->module_configuration_groups[index].name.size + 2
	);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InitializeInspector(EditorState* editor_state)
{
	InspectorData* data = (InspectorData*)editor_state->inspector_data;
	data->draw_function = nullptr;
	data->data_size = 0;
	data->draw_data = nullptr;

	void* allocation = editor_state->editor_allocator->Allocate(InspectorTable::MemoryOf(HASH_TABLE_CAPACITY));
	data->table.InitializeFromBuffer(allocation, HASH_TABLE_CAPACITY);

	AddInspectorTableFunction(&data->table, InspectorDrawTexture, L".png");
	AddInspectorTableFunction(&data->table, InspectorDrawTexture, L".jpg");
	AddInspectorTableFunction(&data->table, InspectorDrawTexture, L".bmp");
	AddInspectorTableFunction(&data->table, InspectorDrawTexture, L".tiff");
	AddInspectorTableFunction(&data->table, InspectorDrawTexture, L".tga");
	AddInspectorTableFunction(&data->table, InspectorDrawTexture, L".hdr");
	AddInspectorTableFunction(&data->table, InspectorDrawTextFile, L".txt");
	AddInspectorTableFunction(&data->table, InspectorDrawTextFile, L".md");
	AddInspectorTableFunction(&data->table, InspectorDrawCppTextFile, L".cpp");
	AddInspectorTableFunction(&data->table, InspectorDrawCppTextFile, L".h");
	AddInspectorTableFunction(&data->table, InspectorDrawCppTextFile, L".hpp");
	AddInspectorTableFunction(&data->table, InspectorDrawCTextFile, L".c");
	AddInspectorTableFunction(&data->table, InspectorDrawHlslTextFile, L".hlsl");
	AddInspectorTableFunction(&data->table, InspectorDrawHlslTextFile, L".hlsli");
	AddInspectorTableFunction(&data->table, InspectorDrawMeshFile, L".gltf");
	AddInspectorTableFunction(&data->table, InspectorDrawMeshFile, L".glb");
}
