#include "editorpch.h"
#include "InspectorModule.h"
#include "InspectorUtilities.h"
#include "../../Editor/EditorState.h"

#include "../Inspector.h"
#include "../../Project/ProjectFolders.h"
#include "../../Modules/Module.h"
#include "../../Modules/ModuleSettings.h"

using namespace ECSEngine;
ECS_TOOLS;

#define DRAW_MODULE_LINEAR_ALLOCATOR_CAPACITY ECS_KB
#define DRAW_MODULE_SETTINGS_ALLOCATOR_CAPACITY ECS_KB * 128
#define DRAW_MODULE_SETTINGS_ALLOCATOR_POOL_COUNT 1024
#define DRAW_MODULE_SETTINGS_ALLOCATOR_BACKUP_SIZE ECS_KB * 512
#define MAX_HEADER_STATES 32

struct DrawModuleData {
	Stream<wchar_t> module_name;
	CapacityStream<wchar_t> settings_name;
	Stream<EditorModuleReflectedSetting> reflected_settings;
	MemoryManager settings_allocator;
	LinearAllocator linear_allocator;
	bool* header_states;
	unsigned int window_index;
	unsigned int inspector_index;
};

#pragma region Draw Module Helpers

// ----------------------------------------------------------------------------------------------------------------------------

static void AllocateInspectorSettingsHelper(EditorState* editor_state, unsigned int module_index, unsigned int inspector_index) {
	ECS_STACK_CAPACITY_STREAM(EditorModuleReflectedSetting, settings, 64);
	DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[inspector_index].draw_data;
	AllocatorPolymorphic allocator = &draw_data->settings_allocator;

	AllocateModuleSettings(editor_state, module_index, settings, allocator);
	draw_data->reflected_settings.InitializeAndCopy(allocator, settings);
}

static void CreateInspectorSettingsHelper(EditorState* editor_state, unsigned int module_index, unsigned int inspector_index) {
	ECS_STACK_CAPACITY_STREAM(EditorModuleReflectedSetting, settings, 64);

	DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[inspector_index].draw_data;
	AllocatorPolymorphic allocator = &draw_data->settings_allocator;
	CreateModuleSettings(editor_state, module_index, settings, allocator, inspector_index);

	draw_data->reflected_settings.InitializeAndCopy(allocator, settings);
}

// ----------------------------------------------------------------------------------------------------------------------------

static void DeallocateInspectorSettingsHelper(EditorState* editor_state, unsigned int module_index, unsigned int inspector_index) {
	DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[inspector_index].draw_data;
	DestroyModuleSettings(editor_state, module_index, draw_data->reflected_settings, inspector_index);
	draw_data->settings_allocator.Clear();
}

// ----------------------------------------------------------------------------------------------------------------------------

// Returns true if it succeded. Else false
static bool LoadInspectorSettingsHelper(EditorState* editor_state, unsigned int module_index, unsigned int inspector_index) {
	DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[inspector_index].draw_data;
	if (draw_data->reflected_settings.size > 0) {
		DeallocateInspectorSettingsHelper(editor_state, module_index, inspector_index);
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetModuleSettingsFilePath(editor_state, module_index, draw_data->settings_name, absolute_path);

	CreateInspectorSettingsHelper(editor_state, module_index, inspector_index);
	bool success = LoadModuleSettings(
		editor_state,
		module_index,
		absolute_path,
		draw_data->reflected_settings,
		&draw_data->settings_allocator
	);

	return success;
}

static bool SaveInspectorSettingsHelper(EditorState* editor_state, unsigned int module_index, unsigned int inspector_index) {
	DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[inspector_index].draw_data;

	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetModuleSettingsFilePath(editor_state, module_index, draw_data->settings_name, absolute_path);

	bool success = SaveModuleSettings(
		editor_state,
		module_index,
		absolute_path,
		draw_data->reflected_settings
	);
	if (!success) {
		ECS_FORMAT_TEMP_STRING(message, "Failed writing module settings to disk for path {#}", absolute_path);
		EditorSetConsoleWarn(message);
	}
	return success;
}

struct CreateSettingsCallbackData {
	EditorState* editor_state;
	unsigned int module_index;
	unsigned int inspector_index;
};

static void CreateModuleSettingsCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateSettingsCallbackData* data = (CreateSettingsCallbackData*)_data;
	CapacityStream<char>* characters = (CapacityStream<char>*)_additional_data;

	DrawModuleData* draw_data = (DrawModuleData*)data->editor_state->inspector_manager.data[data->inspector_index].draw_data;

	if (draw_data->reflected_settings.size > 0) {
		DeallocateInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
		data->editor_state->editor_allocator->Deallocate(draw_data->settings_name.buffer);
	}
	CreateInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);

	ECS_STACK_CAPACITY_STREAM(wchar_t, converted_name, 128);
	ConvertASCIIToWide(converted_name, *characters);

	draw_data->settings_name.InitializeAndCopy(data->editor_state->EditorAllocator(), converted_name);

	// Write to a file the current values
	bool success = SaveInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
	if (!success) {
		EditorSetConsoleWarn("Could not save the new settings file.");
	}
}

struct SetNewSettingData {
	EditorState* editor_state;
	Stream<wchar_t> name;
	CapacityStream<wchar_t>* active_settings;
	unsigned int module_index;
	unsigned int inspector_index;
};

static void SetNewSetting(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SetNewSettingData* data = (SetNewSettingData*)_data;
	if (data->active_settings->size > 0) {
		data->editor_state->editor_allocator->Deallocate(data->active_settings->buffer);
	}
	data->active_settings->InitializeAndCopy(data->editor_state->EditorAllocator(), data->name);
	LoadInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
};

static void InitializeDrawModuleData() {

}

#pragma endregion

void InspectorDrawModuleClean(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	UISystem* ui_system = editor_state->ui_system;
	DrawModuleData* data = (DrawModuleData*)_data;

	// If there is currently a present reflection, release it
	if (data->settings_name.size > 0) {
		unsigned int module_index = GetModuleIndexFromName(editor_state, data->module_name);
		if (module_index != -1) {
			DeallocateInspectorSettingsHelper(editor_state, module_index, data->inspector_index);
		}
	}
	
	editor_state->editor_allocator->Deallocate(data->linear_allocator.GetAllocatedBuffer());
	editor_state->editor_allocator->Deallocate(data->header_states);
	editor_state->editor_allocator->Deallocate(data->module_name.buffer);
	data->settings_allocator.Free();
}

void InspectorDrawModule(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	DrawModuleData* data = (DrawModuleData*)_data;
	data->inspector_index = inspector_index;
	unsigned int module_index = GetModuleIndexFromName(editor_state, data->module_name);
	if (module_index == -1) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	data->window_index = drawer->window_index;
	LinearAllocator* linear_allocator = nullptr;
	bool* collapsing_headers_state = nullptr;

	linear_allocator = &data->linear_allocator;
	linear_allocator->Clear();

	collapsing_headers_state = data->header_states;

	UIDrawConfig config;

	ECS_STACK_CAPACITY_STREAM(wchar_t, setting_absolute_path, 512);

	bool is_graphics_module = IsGraphicsModule(editor_state, module_index);
	const wchar_t* display_icon = is_graphics_module ? GRAPHICS_MODULE_TEXTURE : ECS_TOOLS_UI_TEXTURE_MODULE;
	InspectorIcon(drawer, display_icon, drawer->color_theme.theme);

	ECS_STACK_CAPACITY_STREAM(char, ascii_name, 256);
	ConvertWideCharsToASCII(data->module_name, ascii_name);
	unsigned int base_ascii_name_size = ascii_name.size;
	if (editor_state->project_modules->buffer[module_index].is_graphics_module) {
		ascii_name.AddStream(" (Graphics Type)");
	}
	drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, ascii_name);
	drawer->NextRow();

	// Get all the components and the shared components
	ECS_STACK_CAPACITY_STREAM(unsigned int, reflected_components, 32);
	ECS_STACK_CAPACITY_STREAM(unsigned int, reflected_shared_components, 32);

	unsigned int editor_components_index = editor_state->editor_components.FindModule({ ascii_name.buffer, base_ascii_name_size });
	if (editor_components_index != -1) {
		editor_state->editor_components.GetModuleComponentIndices(editor_components_index, &reflected_components);
		editor_state->editor_components.GetModuleSharedComponentIndices(editor_components_index, &reflected_shared_components);
	}

	if (reflected_components.size == 0) {
		drawer->Text("The module does not have any components.");
		drawer->NextRow();
	}
	else {
		// Draw the components alongside their byte size and their ID
		ECS_STACK_CAPACITY_STREAM(char, label_list_characters, 512);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<char>, labels, reflected_components.size);

		Stream<char> evaluation_name = "ID";

		for (unsigned int index = 0; index < reflected_components.size; index++) {
			const Reflection::ReflectionType* type = editor_state->editor_components.GetType(editor_components_index, reflected_components[index]);
			size_t byte_size = Reflection::GetReflectionTypeByteSize(type);

			CapacityStream<char> current_label = label_list_characters;
			// The component might be lacking an ID
			double id_evaluation = type->GetEvaluation(evaluation_name);
			if (id_evaluation == DBL_MAX) {
				FormatString(label_list_characters, "{#} (ID is missing, {#} byte size)", type->name, byte_size);
			}
			else {
				FormatString(label_list_characters, "{#} ({#} ID, {#} byte size)", type->name, (unsigned short)id_evaluation, byte_size);
			}
			labels[index] = { current_label.buffer + current_label.size, label_list_characters.size - current_label.size };
		}

		labels.size = reflected_components.size;
		drawer->LabelList("Components:", labels);
	}

	drawer->CrossLine();

	if (reflected_shared_components.size == 0) {
		drawer->Text("The module does not have any shared components.");
		drawer->NextRow();
	}
	else {
		// Draw the components alongside their byte size and their ID
		ECS_STACK_CAPACITY_STREAM(char, label_list_characters, 512);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<char>, labels, reflected_shared_components.size);

		Stream<char> evaluation_name = "ID";

		for (unsigned int index = 0; index < reflected_shared_components.size; index++) {
			const Reflection::ReflectionType* type = editor_state->editor_components.GetType(editor_components_index, reflected_shared_components[index]);;
			size_t byte_size = Reflection::GetReflectionTypeByteSize(type);

			CapacityStream<char> current_label = label_list_characters;
			// The component might be lacking an ID
			double id_evaluation = type->GetEvaluation(evaluation_name);
			if (id_evaluation == DBL_MAX) {
				FormatString(label_list_characters, "{#} (ID is missing, {#} byte size)", type->name, byte_size);
			}
			else {
				FormatString(label_list_characters, "{#} ({#} ID, {#} byte size)", type->name, (unsigned short)id_evaluation, byte_size);
			}
			labels[index] = { current_label.buffer + current_label.size, label_list_characters.size - current_label.size };
		}

		labels.size = reflected_shared_components.size;
		drawer->LabelList("Shared Components:", labels);
	}

	drawer->CrossLine();

	// Display the reflected types for this module
	ECS_STACK_CAPACITY_STREAM(unsigned int, reflected_type_indices, 32);
	GetModuleSettingsUITypesIndices(editor_state, module_index, reflected_type_indices);

	if (reflected_type_indices.size > 0) {
		// Coalesce the string
		ECS_STACK_CAPACITY_STREAM(Stream<char>, type_names, 32);
		for (size_t index = 0; index < reflected_type_indices.size; index++) {
			const UIReflectionType* type = editor_state->ui_reflection->GetType(reflected_type_indices[index]);
			type_names[index] = type->name;
		}
		type_names.size = reflected_type_indices.size;
		drawer->LabelList(UI_CONFIG_LABEL_TRANSPARENT, config, "Module settings types", type_names);
		drawer->NextRow();

		ECS_ASSERT(reflected_type_indices.size < MAX_HEADER_STATES);
	}
	// No settings for this module
	else {
		drawer->Text("The module does not have settings types.");
		drawer->NextRow();
	}
	drawer->CrossLine();

	// Allow the user to create a different profile
	auto create_new_setting = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		CreateSettingsCallbackData* data = (CreateSettingsCallbackData*)_data;

		TextInputWizardData wizard_data;
		wizard_data.input_name = "Setting name";
		wizard_data.window_name = "Choose Setting Name";
		wizard_data.callback = CreateModuleSettingsCallback;
		wizard_data.callback_data = data;
		wizard_data.callback_data_size = sizeof(*data);

		CreateTextInputWizard(&wizard_data, system);
	};

	UIConfigActiveState create_settings_active_state;
	create_settings_active_state.state = reflected_type_indices.size != 0;

	UIConfigWindowDependentSize dependent_size;
	dependent_size.scale_factor.x = 1.0f;
	config.AddFlag(dependent_size);
	config.AddFlag(create_settings_active_state);

	drawer->Text("Project Settings ");

	// Display the current setting name - if it has one
	if (data->settings_name.size > 0) {
		GetProjectConfigurationModuleFolder(editor_state, setting_absolute_path);
		setting_absolute_path.Add(ECS_OS_PATH_SEPARATOR);
		setting_absolute_path.AddStreamSafe(data->settings_name);

		// Display the configuration's name
		ECS_STACK_CAPACITY_STREAM(char, ascii_setting_name, 256);
		ConvertWideCharsToASCII(data->settings_name, ascii_setting_name);

		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, ascii_setting_name);
	}
	else {
		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, "No module settings path is selected.");
	}
	drawer->NextRow();

	drawer->CrossLine();

	CreateSettingsCallbackData create_data = { editor_state, module_index, inspector_index };
	drawer->Button(
		UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ACTIVE_STATE,
		config,
		"Create new setting",
		{ create_new_setting, &create_data, sizeof(create_data) }
	);
	drawer->NextRow();

	// Enumerate all the other possible settings
	ECS_STACK_CAPACITY_STREAM(wchar_t, settings_folder, 512);
	GetModuleSettingsFolderPath(editor_state, module_index, settings_folder);

	Stream<wchar_t> settings_extensions[] = { MODULE_SETTINGS_EXTENSION };

	struct FunctorData {
		UIDrawer* drawer;
		EditorState* editor_state;
		LinearAllocator* temp_allocator;
		unsigned int module_index;
		unsigned int inspector_index;
		CapacityStream<wchar_t>* active_settings;
	};

	FunctorData functor_data = { drawer, editor_state, linear_allocator, module_index, data->inspector_index, &data->settings_name };

	if (create_settings_active_state.state) {
		drawer->Text("Available settings ");
		drawer->NextRow();

		ForEachFileInDirectoryWithExtension(settings_folder, { settings_extensions, std::size(settings_extensions) }, &functor_data,
			[](Stream<wchar_t> path, void* _data) {
				FunctorData* data = (FunctorData*)_data;

				Stream<wchar_t> stem = PathStem(path);

				bool is_active = data->active_settings != nullptr ? stem == *data->active_settings : false;
				UIDrawConfig config;

				wchar_t* name_allocation = (wchar_t*)data->temp_allocator->Allocate((stem.size + 1) * sizeof(wchar_t));
				stem.CopyTo(name_allocation);
				name_allocation[stem.size] = L'\0';

				SetNewSettingData callback_data = {
					data->editor_state,
					{name_allocation, stem.size},
					data->active_settings,
					data->module_index,
					data->inspector_index
				};

				float2 square_scale = data->drawer->GetSquareScale();

				UIConfigWindowDependentSize dependent_size;
				float total_scale = data->drawer->GetWindowScaleUntilBorder();
				float label_scale = total_scale - square_scale.x - data->drawer->layout.element_indentation;
				size_t label_configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_TRANSPARENT;
				if (is_active) {
					label_configuration = ClearFlag(label_configuration, UI_CONFIG_LABEL_TRANSPARENT);
				}
				dependent_size.scale_factor.x = data->drawer->GetWindowSizeFactors(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, { label_scale, 0.0f }).x;
				config.AddFlag(dependent_size);

				data->drawer->ButtonWide(label_configuration, config, stem, { SetNewSetting, &callback_data, sizeof(callback_data) });

				struct DeleteActionData {
					const wchar_t* filename;
					EditorState* editor_state;
					unsigned int module_index;
					unsigned int inspector_index;
					CapacityStream<wchar_t>* active_settings;
				};
				DeleteActionData action_data = { (wchar_t*)name_allocation, data->editor_state, data->module_index, data->inspector_index, data->active_settings };

				auto delete_action = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					DeleteActionData* data = (DeleteActionData*)_data;

					ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
					GetModuleSettingsFilePath(data->editor_state, data->module_index, data->filename, absolute_path);
					bool success = RemoveFile(absolute_path);
					if (!success) {
						ECS_FORMAT_TEMP_STRING(error_message, "Could not delete module settings file {#}.", absolute_path);
						EditorSetConsoleError(error_message);
					}
					else {
						// Check to see if that is the current setting
						if (data->active_settings->size > 0 && data->filename == *data->active_settings) {
							DeallocateInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
							data->editor_state->editor_allocator->Deallocate(data->active_settings->buffer);
							*data->active_settings = { nullptr, 0, 0 };
						}
					}
				};

				data->drawer->SpriteButton(UI_CONFIG_LATE_DRAW | UI_CONFIG_MAKE_SQUARE, config, { delete_action, &action_data, sizeof(action_data) }, ECS_TOOLS_UI_TEXTURE_X, data->drawer->color_theme.text);
				data->drawer->NextRow();

				return true;
			});

		drawer->CrossLine();
	}

	// Present the data now - if a path is selected

	if (data->settings_name.size > 0) {
		drawer->Text("Settings Variables");
		drawer->NextRow();

		drawer->CrossLine();

		struct SaveFileAfterChangeData {
			EditorState* editor_state;
			unsigned int module_index;
			unsigned int inspector_index;
		};

		auto save_file_after_change = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SaveFileAfterChangeData* data = (SaveFileAfterChangeData*)_data;
			bool success = SaveInspectorSettingsHelper(data->editor_state, data->module_index, data->inspector_index);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(console_string, "An error occured when saving the module setting file for inspector {#} after value change. "
					"The modification has not been commited.", data->inspector_index);
				EditorSetConsoleError(console_string);
			}
		};

		auto array_add_element_change = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			UIDrawerArrayAddRemoveData* data = (UIDrawerArrayAddRemoveData*)_additional_data;
			// memset the elements to 0
			unsigned int new_elements = data->capacity_data->size - data->new_size;
			memset(OffsetPointer(data->capacity_data->buffer, data->element_byte_size * data->new_size), 0, data->element_byte_size * new_elements);

			SaveFileAfterChangeData* save_data = (SaveFileAfterChangeData*)_data;
			bool success = SaveInspectorSettingsHelper(save_data->editor_state, save_data->module_index, save_data->inspector_index);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(console_string, "An error occured when saving the module setting file for inspector {#} after value change. "
					"The modification has not been commited.", save_data->inspector_index);
				EditorSetConsoleError(console_string);
			}
		};

		ECS_STACK_CAPACITY_STREAM(unsigned int, instance_indices, 64);
		GetModuleSettingsUIInstancesIndices(editor_state, module_index, data->inspector_index, instance_indices);

		SaveFileAfterChangeData save_file_data = { editor_state, module_index, data->inspector_index };

		if (instance_indices.size > 0) {
			UIReflectionDrawConfig ui_reflection_configs[10];
			memset(ui_reflection_configs, 0, sizeof(ui_reflection_configs));
			UIActionHandler save_handler = { save_file_after_change, &save_file_data, sizeof(save_file_data) };
			void* ui_reflection_configs_stack_memory = ECS_STACK_ALLOC(ECS_KB);

			size_t used_count = UIReflectionDrawConfigSplatCallback(
				{ ui_reflection_configs, std::size(ui_reflection_configs) },
				save_handler, 
				ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ALL,
				ui_reflection_configs_stack_memory
			);

			config.flag_count = 0;
			dependent_size.scale_factor.x = 1.0f;
			config.AddFlag(dependent_size);
			for (size_t index = 0; index < instance_indices.size; index++) {
				// Display them as collapsing headers
				UIReflectionInstance* instance = editor_state->ui_reflection->GetInstance(instance_indices[index]);

				Stream<char> separator = FindFirstCharacter(instance->name, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR);
				Stream<char> name = { instance->name.buffer, instance->name.size - separator.size };

				drawer->CollapsingHeader(name, collapsing_headers_state + index, [&]() {
					Stream<char> current_instance_name = instance->name;
					instance->name = name;
					UIReflectionDrawInstanceOptions options;
					options.drawer = drawer;
					options.config = &config;
					options.global_configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE;
					options.additional_configs = { ui_reflection_configs, used_count };
					editor_state->ui_reflection->DrawInstance(
						instance,
						&options
					);
					// Restore the name
					instance->name = current_instance_name;
					});
			}
		}

		auto set_default_values = [](ActionData* action_data) {
			SaveFileAfterChangeData* data = (SaveFileAfterChangeData*)action_data->data;

			DrawModuleData* draw_data = (DrawModuleData*)data->editor_state->inspector_manager.data[data->inspector_index].draw_data;
			if (draw_data->reflected_settings.size > 0) {
				SetModuleDefaultSettings(data->editor_state, data->module_index, draw_data->reflected_settings);
				ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
				GetModuleSettingsFilePath(data->editor_state, data->module_index, draw_data->settings_name, absolute_path);

				bool success = SaveModuleSettings(data->editor_state, data->module_index, absolute_path, draw_data->reflected_settings);
				if (!success) {
					EditorSetConsoleError("An error has occured when trying to save default values for module settings file. The modification has not been commited.");
				}
			}
		};

		drawer->Button(UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Default Values", { set_default_values, &save_file_data, sizeof(save_file_data) });
		config.flag_count = 0;
		drawer->NextRow();

		drawer->CrossLine();
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void UpdateInspectorUIModuleSettings(EditorState* editor_state, unsigned int module_index)
{
	for (size_t index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (editor_state->inspector_manager.data[index].draw_function == InspectorDrawModule) {
			DrawModuleData* draw_data = (DrawModuleData*)editor_state->inspector_manager.data[index].draw_data;
			unsigned int current_module_index = GetModuleIndexFromName(editor_state, draw_data->module_name);
			if (current_module_index == module_index) {
				// Deallocate the current settings
				if (draw_data->settings_name.size > 0) {
					DeallocateInspectorSettingsHelper(editor_state, module_index, index);
					LoadInspectorSettingsHelper(editor_state, module_index, index);
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToModule(EditorState* editor_state, unsigned int index, unsigned int inspector_index, Stream<wchar_t> initial_settings) {
	ECS_ASSERT(index < editor_state->project_modules->size);

	DrawModuleData draw_data;
	draw_data.module_name = {};
	draw_data.window_index = 0;
	draw_data.settings_name = { nullptr, 0 };
	draw_data.header_states = nullptr;
	draw_data.reflected_settings = { nullptr, 0 };

	if (inspector_index == -1) {
		inspector_index = GetMatchingIndexFromRobin(editor_state);
	}

	if (inspector_index != -1) {
		ChangeInspectorDrawFunction(
			editor_state,
			inspector_index,
			{ InspectorDrawModule, InspectorDrawModuleClean },
			&draw_data,
			sizeof(draw_data)
		);

		struct InitializeData {
			Stream<wchar_t> module_name;
			Stream<wchar_t> initial_settings;
		};

		AllocatorPolymorphic initialize_allocator = GetLastInspectorTargetInitializeAllocator(editor_state, inspector_index);
		InitializeData initialize_data;
		initialize_data.module_name = editor_state->project_modules->buffer[index].library_name.Copy(initialize_allocator);
		initialize_data.initial_settings = initial_settings.Copy(initialize_allocator);

		auto initialize = [](EditorState* editor_state, void* data, void* _initialize_data, unsigned int inspector_index) {
			DrawModuleData* draw_data = (DrawModuleData*)data;
			InitializeData* initialize_data = (InitializeData*)_initialize_data;

			Stream<wchar_t> module_name = StringCopy(editor_state->EditorAllocator(), initialize_data->module_name);
			draw_data->module_name = module_name;
			draw_data->inspector_index = inspector_index;
			draw_data->linear_allocator = LinearAllocator::InitializeFrom(editor_state->editor_allocator, DRAW_MODULE_LINEAR_ALLOCATOR_CAPACITY);
			draw_data->header_states = (bool*)editor_state->editor_allocator->Allocate(sizeof(bool) * MAX_HEADER_STATES);
			memset(draw_data->header_states, false, sizeof(bool)* MAX_HEADER_STATES);
			draw_data->settings_allocator = MemoryManager(
				DRAW_MODULE_SETTINGS_ALLOCATOR_CAPACITY,
				DRAW_MODULE_SETTINGS_ALLOCATOR_POOL_COUNT,
				DRAW_MODULE_SETTINGS_ALLOCATOR_BACKUP_SIZE,
				editor_state->GlobalMemoryManager()
			);

			if (initialize_data->initial_settings.size > 0) {
				unsigned int module_index = GetModuleIndexFromName(editor_state, initialize_data->module_name);
				if (module_index != -1) {
					SetNewSettingData set_new_data;
					set_new_data.active_settings = &draw_data->settings_name;
					set_new_data.editor_state = editor_state;
					set_new_data.inspector_index = inspector_index;
					set_new_data.module_index = module_index;
					set_new_data.name = initialize_data->initial_settings;

					ActionData action_data;
					action_data.data = &set_new_data;
					action_data.system = nullptr;
					SetNewSetting(&action_data);
					memset(draw_data->header_states, true, sizeof(bool) * MAX_HEADER_STATES);
				}
			}
		};

		SetLastInspectorTargetInitialize(editor_state, inspector_index, initialize, &initialize_data, sizeof(initialize_data));
	}
}

// ----------------------------------------------------------------------------------------------------------------------------