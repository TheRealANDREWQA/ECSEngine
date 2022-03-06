#include "editorpch.h"
#include "Settings.h"
#include "../Editor/EditorState.h"
#include "../HelperWindows.h"
#include "../Project/ProjectFolders.h"
#include "../Project/ProjectSettings.h"

using namespace ECSEngine;
ECS_TOOLS;

#define WINDOW_SIZE float2(0.4f, 1.0f)

#define UI_REFLECTION_INSTANCE_NAME "Descriptor"

// -----------------------------------------------------------------------------------------------------------------------------

void SettingsDestroyAction(ActionData* action_data) {
	EditorState* editor_state = (EditorState*)action_data->data;
	EDITOR_STATE(editor_state);

	ui_reflection->DestroyInstance(UI_REFLECTION_INSTANCE_NAME);
	ui_reflection->DestroyType(STRING(WorldDescriptor));
}

// -----------------------------------------------------------------------------------------------------------------------------

void SettingsWindowSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	descriptor.draw = SettingsDraw;

	descriptor.destroy_action = SettingsDestroyAction;
	descriptor.destroy_action_data = editor_state;
	descriptor.destroy_action_data_size = 0;
	
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;
	descriptor.window_name = SETTINGS_WINDOW_NAME;
}

// -----------------------------------------------------------------------------------------------------------------------------

void CreateSettingsCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	CapacityStream<char>* characters = (CapacityStream<char>*)_additional_data;

	ECS_STACK_CAPACITY_STREAM(wchar_t, wide_name, 512);
	function::ConvertASCIIToWide(wide_name, *characters);
	wide_name.AddStreamSafe(ToStream(L".config"));
	wide_name[wide_name.size] = L'\0';

	// Save the file with the default values
	WorldDescriptor default_descriptor = GetDefaultSettings(editor_state);
	bool success = SaveWorldParametersFile(editor_state, wide_name, &default_descriptor);
	if (success) {
		editor_state->project_descriptor = default_descriptor;
		SetSettingsPath(editor_state, wide_name);
	}

	// If it failed, they should already be notified about the error. 
}

#define LINEAR_ALLOCATOR_NAME "Allocator"
#define LINEAR_ALLOCATOR_CAPACITY ECS_KB

void SettingsDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	EditorState* editor_state = (EditorState*)window_data;
	EDITOR_STATE(editor_state);

	LinearAllocator* linear_allocator = nullptr;
	if (initialize) {
		linear_allocator = (LinearAllocator*)drawer.GetMainAllocatorBufferAndStoreAsResource(LINEAR_ALLOCATOR_NAME, sizeof(LinearAllocator));
		*linear_allocator = LinearAllocator(drawer.GetMainAllocatorBuffer(LINEAR_ALLOCATOR_CAPACITY), LINEAR_ALLOCATOR_CAPACITY);
		
		Reflection::ReflectionType reflection_type = editor_state->ReflectionManager()->GetType(STRING(WorldDescriptor));
		UIReflectionType* ui_type = ui_reflection->CreateType(reflection_type);

		WorldDescriptor lower_bound_descriptor;
		lower_bound_descriptor.entity_manager_memory_new_allocation_size = ECS_KB_10;
		lower_bound_descriptor.entity_manager_memory_pool_count = 32;
		lower_bound_descriptor.entity_manager_memory_size = ECS_MB_10;
		lower_bound_descriptor.global_memory_new_allocation_size = ECS_KB_10 * 32;
		lower_bound_descriptor.global_memory_size = ECS_MB_10 * 10;
		lower_bound_descriptor.global_memory_pool_count = 32;
		lower_bound_descriptor.thread_count = 1;
		lower_bound_descriptor.entity_pool_power_of_two = 5;
		ui_reflection->BindTypeLowerBounds(ui_type, &lower_bound_descriptor);

		WorldDescriptor upper_bound_descriptor;
		upper_bound_descriptor.entity_manager_memory_new_allocation_size = ECS_GB_10 * 10;
		upper_bound_descriptor.entity_manager_memory_pool_count = ECS_KB_10 * 128;
		upper_bound_descriptor.entity_manager_memory_size = ECS_GB_10 * 25;
		upper_bound_descriptor.global_memory_size = ECS_GB_10 * 50;
		upper_bound_descriptor.global_memory_pool_count = ECS_KB_10 * 128;
		upper_bound_descriptor.global_memory_new_allocation_size = ECS_GB_10 * 25;
		upper_bound_descriptor.thread_count = 256;
		upper_bound_descriptor.entity_pool_power_of_two = 1 << 20;
		ui_reflection->BindTypeUpperBounds(ui_type, &upper_bound_descriptor);

		WorldDescriptor default_descriptor = GetDefaultSettings(editor_state);
		ui_reflection->BindTypeDefaultData(ui_type, &default_descriptor);

		UIReflectionInstance* instance = ui_reflection->CreateInstance(UI_REFLECTION_INSTANCE_NAME, ui_type);
		ui_reflection->BindInstancePtrs(instance, &editor_state->project_descriptor, reflection_type);
	}
	else {
		linear_allocator = (LinearAllocator*)drawer.GetResource(LINEAR_ALLOCATOR_NAME);
		linear_allocator->Clear();
	}

	UIDrawConfig config;
	// If the path is valid, display the configuration's name
	if (ExistsProjectSettings(editor_state)) {
		ECS_STACK_CAPACITY_STREAM(char, ascii_setting_name, 256);
		function::ConvertWideCharsToASCII(editor_state->project_file->project_settings, ascii_setting_name);

		drawer.Text(UI_CONFIG_DO_NOT_CACHE, config, "Current setting: ");

		// Null terminate the string
		ascii_setting_name.AddSafe('\0');
		drawer.Text(UI_CONFIG_DO_NOT_CACHE, config, ascii_setting_name.buffer);
	}
	else {
		// The path is invalid. Display a warning
		drawer.Text(UI_CONFIG_DO_NOT_CACHE, config, "The project settings file is missing.");
	}
	drawer.NextRow();

	// Allow the user to create a different profile
	auto create_new_setting = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		EditorState* editor_state = (EditorState*)_data;
		TextInputWizardData wizard_data;
		wizard_data.input_name = "Setting name";
		wizard_data.window_name = "Choose Setting Name";
		wizard_data.callback = CreateSettingsCallback;
		wizard_data.callback_data = editor_state;
		wizard_data.callback_data_size = 0;

		CreateTextInputWizard(&wizard_data, system);
	};

	UIConfigWindowDependentSize dependent_size;
	dependent_size.scale_factor.x = 1.0f;
	config.AddFlag(dependent_size);
	drawer.Button(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Create new setting", { create_new_setting, editor_state, 0 });
	drawer.NextRow();
	drawer.CrossLine();

	// Enumerate all the other possible settings
	ECS_STACK_CAPACITY_STREAM(wchar_t, configuration_folder, 512);
	GetProjectConfigurationFolder(editor_state, configuration_folder);

	const wchar_t* configuration_extensions[] = { L".config" };

	struct FunctorData {
		UIDrawer* drawer;
		EditorState* editor_state;
		LinearAllocator* temp_allocator;
	};

	FunctorData functor_data = { &drawer, editor_state, linear_allocator };

	ForEachFileInDirectoryWithExtension(configuration_folder, { configuration_extensions, std::size(configuration_extensions) }, &functor_data, 
		[](const wchar_t* path, void* _data) {
			FunctorData* data = (FunctorData*)_data;

			Stream<wchar_t> stream_path = ToStream(path);
			Stream<wchar_t> filename = function::PathFilename(stream_path);
			bool is_active = function::CompareStrings(filename, data->editor_state->project_file->project_settings);
			UIDrawConfig config;
			UIConfigCheckBoxCallback callback;
			
			struct SetNewSettingData {
				EditorState* editor_state;
				Stream<wchar_t> name;
			};

			auto set_new_setting = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				SetNewSettingData* data = (SetNewSettingData*)_data;

				// Try to load the data first
				WorldDescriptor world_descriptor = LoadWorldParametersFile(data->editor_state, data->name, true);
				// Check for failure - thread count is zero
				if (world_descriptor.thread_count == 0) {
					// Just return. Can't change the setting if the load fails
					return;
				}

				SetSettingsPath(data->editor_state, data->name);
				// Update the world descriptor aswell
				data->editor_state->project_descriptor = world_descriptor;
			};
			
			void* name_allocation = data->temp_allocator->Allocate((filename.size + 1) * sizeof(wchar_t));
			filename.CopyTo(name_allocation);
			wchar_t* name_char = (wchar_t*)name_allocation;
			name_char[filename.size] = L'\0';

			Stream<wchar_t> stem = function::PathStem(stream_path);
			ECS_STACK_CAPACITY_STREAM(char, ascii_name, 64);
			function::ConvertWideCharsToASCII(stem, ascii_name);

			SetNewSettingData callback_data = { data->editor_state, {name_allocation, filename.size} };
			callback.handler.action = set_new_setting;
			callback.handler.data = &callback_data;
			callback.handler.data_size = sizeof(callback_data);
			callback.disable_value_to_modify = true;
			config.AddFlag(callback);

			data->drawer->CheckBox(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_CHECK_BOX_CALLBACK, config, ascii_name.buffer, &is_active);

			if (!function::CompareStrings(filename, ToStream(L"Settings.config"))) {
				UIConfigAbsoluteTransform right_transform;
				right_transform.scale = data->drawer->GetSquareScale(data->drawer->layout.default_element_y * 0.75f);
				right_transform.position = data->drawer->GetAlignedToRight(right_transform.scale.x);
				config.AddFlag(right_transform);
				data->drawer->SpriteRectangle(UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_LATE_DRAW, config, ECS_TOOLS_UI_TEXTURE_X, data->drawer->color_theme.default_text);

				struct DeleteActionData {
					const wchar_t* filename;
					EditorState* editor_state;
				};
				auto delete_action = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					DeleteActionData* data = (DeleteActionData*)_data;
					ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
					GetProjectConfigurationFolder(data->editor_state, absolute_path);
					absolute_path.Add(ECS_OS_PATH_SEPARATOR);
					absolute_path.AddStreamSafe(ToStream(data->filename));
					absolute_path[absolute_path.size] = L'\0';
					bool success = RemoveFile(absolute_path);
					if (!success) {
						ECS_FORMAT_TEMP_STRING(error_message, "Could not delete project settings file {#}.", absolute_path);
						EditorSetConsoleError(error_message);
					}
				};
				DeleteActionData action_data = { name_char, data->editor_state };
				data->drawer->AddDefaultClickableHoverable(right_transform.position, right_transform.scale, { delete_action, &action_data, sizeof(action_data) });
			}

			data->drawer->NextRow();

		return true;
	});

	drawer.CrossLine();

	// Present the data now
	drawer.Text(UI_CONFIG_DO_NOT_CACHE, config, "Project Settings: ");
	drawer.NextRow();
	
	auto save_file_after_change = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		EditorState* editor_state = (EditorState*)_data;
		bool success = SaveWorldParametersFile(editor_state, editor_state->project_file->project_settings, &editor_state->project_descriptor);
		if (!success) {
			EditorSetConsoleError(ToStream("An error occured when saving the setting file after value change. The modification has not been commited."));
		}
	};

	UIConfigTextInputCallback write_file_callback;
	write_file_callback.handler = { save_file_after_change, editor_state, 0 };
	config.AddFlag(write_file_callback);

	UIReflectionDrawConfig ui_reflection_configs[2];
	ui_reflection_configs[0].configurations = UI_CONFIG_WINDOW_DEPENDENT_SIZE;
	ui_reflection_configs[0].index[0] = UIReflectionIndex::Count;

	ui_reflection_configs[1].configurations = UI_CONFIG_NUMBER_INPUT_DO_NOT_REDUCE_SCALE | UI_CONFIG_TEXT_INPUT_CALLBACK;
	ui_reflection_configs[1].index[0] = UIReflectionIndex::IntegerInput;
	ui_reflection->DrawInstance(UI_REFLECTION_INSTANCE_NAME, drawer, config, { ui_reflection_configs, std::size(ui_reflection_configs) });
	config.flag_count--;

	auto set_default_values = [](ActionData* action_data) {
		EditorState* editor_state = (EditorState*)action_data->data;
		editor_state->project_descriptor = GetDefaultSettings(editor_state);
		bool success = SaveWorldParametersFile(editor_state, editor_state->project_file->project_settings, &editor_state->project_descriptor);
		if (!success) {
			EditorSetConsoleError(ToStream("An error has occured when trying to save default values for settings file. The modification has not been commited."));
		}
	};

	drawer.Button(UI_CONFIG_DO_NOT_CACHE | UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Default Values", { set_default_values, editor_state, 0 });
	config.flag_count = 0;
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateSettingsWindowOnly(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	UIWindowDescriptor descriptor;

	SettingsWindowSetDescriptor(descriptor, editor_state, nullptr);
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);

	descriptor.initial_size_x = WINDOW_SIZE.x;
	descriptor.initial_size_y = WINDOW_SIZE.y;

	return ui_system->Create_Window(descriptor);
}

// -----------------------------------------------------------------------------------------------------------------------------

void CreateSettingsWindow(EditorState* editor_state) {
	CreateDockspaceFromWindow(SETTINGS_WINDOW_NAME, editor_state, CreateSettingsWindowOnly);
}

// -----------------------------------------------------------------------------------------------------------------------------

void CreateSettingsWindowAction(ActionData* action_data)
{
	CreateSettingsWindow((EditorState*)action_data->data);
}

// -----------------------------------------------------------------------------------------------------------------------------