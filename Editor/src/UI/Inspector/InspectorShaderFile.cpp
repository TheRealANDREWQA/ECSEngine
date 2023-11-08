#include "editorpch.h"
#include "InspectorMiscFile.h"
#include "../Inspector.h"
#include "InspectorUtilities.h"
#include "../../Editor/EditorState.h"

#include "../../Assets/AssetManagement.h"
#include "../../Assets/AssetExtensions.h"
#include "../../Assets/AssetBuiltins.h"
#include "../AssetIcons.h"
#include "../AssetSettingHelper.h"
#include "../../Modules/Module.h"
#include "../../Project/ProjectFolders.h"
#include "../../Editor/EditorPalette.h"
#include "InspectorShaderFile.h"

using namespace ECSEngine;
ECS_TOOLS;

#define LAZY_EVALUATION_MILLISECONDS 500
#define MACRO_INPUT_CAPACITY 128

struct ShaderMacroUI {
	CapacityStream<char> name_stream;
	CapacityStream<char> definition_stream;
};

struct InspectorDrawShaderFileData {
	ShaderMetadata shader_metadata;
	Stream<wchar_t> path;
	// This only a path
	CapacityStream<wchar_t> target_file;
	ResizableStream<ShaderMacroUI> shader_macros;

	// Lazily retrieve the shader code and suggest to the user the macros that it can define
	Stream<char> shader_source_code;
	Stream<Stream<char>> conditional_macros;
	Timer timer;
	EDITOR_SHADER_BUILTIN builtin;
	size_t metadata_timestamp;
};

static void DeallocateShaderStructures(EditorState* editor_state, InspectorDrawShaderFileData* data) {
	if (data->shader_source_code.size > 0) {
		editor_state->editor_allocator->Deallocate(data->shader_source_code.buffer);
		for (size_t index = 0; index < data->conditional_macros.size; index++) {
			editor_state->editor_allocator->Deallocate(data->conditional_macros[index].buffer);
		}
		if (data->conditional_macros.size > 0) {
			editor_state->editor_allocator->Deallocate(data->conditional_macros.buffer);
		}
	}
}

static void RetrieveSourceCode(EditorState* editor_state, InspectorDrawShaderFileData* data) {
	if (data->timer.GetDuration(ECS_TIMER_DURATION_MS) >= LAZY_EVALUATION_MILLISECONDS) {
		DeallocateShaderStructures(editor_state, data);
		AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
		data->shader_source_code = ReadWholeFileText(data->target_file, editor_allocator);
		if (data->shader_source_code.size > 0) {
			// Determine all the macros
			ECS_STACK_CAPACITY_STREAM(Stream<char>, conditional_macros, 512);
			bool success = editor_state->UIGraphics()->ReflectShaderMacros(data->shader_source_code, nullptr, &conditional_macros, editor_allocator);
			if (!success) {
				// Print an error
				ECS_FORMAT_TEMP_STRING(console_message, "Failed to retrieve macros for target shader {#}.", data->target_file);
				EditorSetConsoleError(console_message);
			}
			else {
				// Allocate the main buffer as well
				data->conditional_macros.InitializeAndCopy(editor_allocator, conditional_macros);
			}
		}
		data->timer.SetNewStart();
	}
}

struct AddMacroData {
	InspectorDrawShaderFileData* draw_data;
	EditorState* editor_state;
};

static void AddMacroCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	AddMacroData* data = (AddMacroData*)_data;
	UIDrawerArrayAddRemoveData* additional_data = (UIDrawerArrayAddRemoveData*)_additional_data;
	// This is the old size
	unsigned int start_index = additional_data->new_size;
	for (unsigned int index = start_index; index < data->draw_data->shader_macros.size; index++) {
		data->draw_data->shader_macros[index].definition_stream.Initialize(data->editor_state->editor_allocator, 0, MACRO_INPUT_CAPACITY);
		data->draw_data->shader_macros[index].name_stream.Initialize(data->editor_state->editor_allocator, 0, MACRO_INPUT_CAPACITY);
	}
};

struct RemoveAnywhereMacroData {
	InspectorDrawShaderFileData* draw_data;
	EditorState* editor_state;
};

static void RemoveAnywhereCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RemoveAnywhereMacroData* data = (RemoveAnywhereMacroData*)_data;
	unsigned int remove_index = *(unsigned int*)_additional_data;
	Stream<char> definition = data->draw_data->shader_macros[remove_index].definition_stream;
	Stream<char> name = data->draw_data->shader_macros[remove_index].name_stream;

	unsigned int metadata_index = data->draw_data->shader_metadata.FindMacro(name);
	ECS_ASSERT(metadata_index != -1);
	data->draw_data->shader_metadata.RemoveMacro(metadata_index, data->editor_state->asset_database->Allocator());

	data->editor_state->editor_allocator->Deallocate(definition.buffer);
	data->editor_state->editor_allocator->Deallocate(name.buffer);

	// Write the asset
	AssetSettingsHelperChangedWithFileActionData changed_data;
	changed_data.asset = &data->draw_data->shader_metadata;
	changed_data.asset_type = ECS_ASSET_SHADER;
	changed_data.editor_state = data->editor_state;
	changed_data.previous_name = data->draw_data->shader_metadata.name;
	changed_data.previous_target_file = data->draw_data->shader_metadata.file;
	changed_data.target_database = data->editor_state->asset_database;
	action_data->data = &changed_data;
	AssetSettingsHelperChangedWithFileAction(action_data);
};

struct MacroInputCallbackData {
	EditorState* editor_state;
	InspectorDrawShaderFileData* data;
	unsigned int macro_index;
	bool is_name;
};

static void MacroInputCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	MacroInputCallbackData* data = (MacroInputCallbackData*)_data;
	UIDrawerTextInput* input = (UIDrawerTextInput*)_additional_data;

	AllocatorPolymorphic allocator = data->editor_state->asset_database->Allocator();
	if (data->is_name) {
		// Use the old input name to detect the index of the macro
		unsigned int macro_index = data->data->shader_metadata.FindMacro(input->previous_text);
		// When the name is inserted the first time it might not be in the metadata
		if (macro_index != -1) {
			data->data->shader_metadata.RemoveMacro(macro_index, allocator);
		}
		data->data->shader_metadata.AddMacro(data->data->shader_macros[data->macro_index].name_stream, data->data->shader_macros[data->macro_index].definition_stream, allocator);
	}
	else {
		unsigned int macro_index = data->data->shader_metadata.FindMacro(data->data->shader_macros[data->macro_index].name_stream);
		if (macro_index != -1) {
			data->data->shader_metadata.UpdateMacro(macro_index, data->data->shader_macros[data->macro_index].definition_stream, allocator);
		}
		else {
			data->data->shader_metadata.AddMacro(data->data->shader_macros[data->macro_index].name_stream, data->data->shader_macros[data->macro_index].definition_stream, allocator);
		}
	}

	// Now try to update the asset
	AssetSettingsHelperChangedWithFileActionData changed_data;
	changed_data.asset_type = ECS_ASSET_SHADER;
	changed_data.editor_state = data->editor_state;
	changed_data.asset = &data->data->shader_metadata;
	changed_data.previous_name = data->data->shader_metadata.name;
	changed_data.previous_target_file = data->data->shader_metadata.file;
	changed_data.target_database = data->editor_state->asset_database;
	// The thunk file is not needed
	action_data->data = &changed_data;
	AssetSettingsHelperChangedWithFileAction(action_data);
}

struct BuiltinCallbackData {
	EditorState* editor_state;
	InspectorDrawShaderFileData* data;
};

static void BuiltinCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	BuiltinCallbackData* data = (BuiltinCallbackData*)_data;

	SetAssetBuiltinActionData set_data;
	set_data.asset = &data->data->shader_metadata;
	set_data.asset_type = ECS_ASSET_SHADER;
	set_data.builtin_index = data->data->builtin;
	set_data.current_path = data->data->path;
	set_data.editor_state = data->editor_state;

	action_data->data = &set_data;
	SetAssetBuiltinAction(action_data);
};

struct DrawShaderMacroData {
	EditorState* editor_state;
	InspectorDrawShaderFileData* data;
};

static void DrawShaderMacro(UIDrawer& drawer, Stream<char> element_name, UIDrawerArrayDrawData data) {
	UIDrawConfig temp_config;

	ShaderMacroUI* macro_ui = (ShaderMacroUI*)data.element_data;
	DrawShaderMacroData* draw_data = (DrawShaderMacroData*)data.additional_data;

	size_t configuration = ClearFlag(data.configuration, UI_CONFIG_DO_CACHE);
	UIDrawConfig* config = data.config == nullptr ? &temp_config : data.config;

	MacroInputCallbackData input_callback_data;
	input_callback_data.editor_state = draw_data->editor_state;
	input_callback_data.data = draw_data->data;
	input_callback_data.is_name = true;
	input_callback_data.macro_index = data.current_index;

	UIConfigTextInputCallback callback;
	callback.handler = { MacroInputCallback, &input_callback_data, sizeof(input_callback_data) };
	callback.trigger_only_on_release = true;
	config->AddFlag(callback);

	drawer.TextInput(configuration | UI_CONFIG_TEXT_INPUT_CALLBACK, *config, "Name", &macro_ui->name_stream);
	drawer.NextRow();

	input_callback_data.is_name = false;
	drawer.TextInput(configuration | UI_CONFIG_TEXT_INPUT_CALLBACK, *config, "Definition", &macro_ui->definition_stream);
	drawer.NextRow();

	config->flag_count--;
}

static void InspectorCleanShader(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	InspectorDrawShaderFileData* data = (InspectorDrawShaderFileData*)_data;
	AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();

	data->target_file.Deallocate(editor_allocator);
	for (unsigned int index = 0; index < data->shader_macros.size; index++) {
		editor_state->editor_allocator->Deallocate(data->shader_macros[index].definition_stream.buffer);
		editor_state->editor_allocator->Deallocate(data->shader_macros[index].name_stream.buffer);
	}
	data->shader_macros.FreeBuffer();
	data->shader_metadata.DeallocateMemory(editor_allocator);
	DeallocateShaderStructures(editor_state, data);
}

void InspectorDrawShaderFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	InspectorDrawShaderFileData* data = (InspectorDrawShaderFileData*)_data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(data->path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	if (data->shader_metadata.name.size == 0) {
		data->builtin = EDITOR_SHADER_BUILTIN_COUNT;

		// Determine the name
		ECS_STACK_CAPACITY_STREAM(char, name, 512);
		GetAssetNameFromThunkOrForwardingFile(editor_state, data->path, name);
		data->shader_metadata.name = StringCopy(editor_state->EditorAllocator(), name);

		data->target_file.Initialize(editor_state->editor_allocator, 0, 512);
		data->shader_macros.Initialize(editor_state->EditorAllocator(), 0);

		// Read the file, if there is one
		bool success = editor_state->asset_database->ReadShaderFile(name, &data->shader_metadata);
		if (!success) {
			// Set the default
			data->shader_metadata.Default(data->shader_metadata.name, {});
		}
		else {
			// Copy all the macros
			data->target_file.CopyOther(data->shader_metadata.file);

			data->shader_macros.ResizeNoCopy(data->shader_metadata.macros.size);
			for (size_t index = 0; index < data->shader_metadata.macros.size; index++) {
				data->shader_macros[index].name_stream.Initialize(editor_state->editor_allocator, 0, MACRO_INPUT_CAPACITY);
				data->shader_macros[index].definition_stream.Initialize(editor_state->editor_allocator, 0, MACRO_INPUT_CAPACITY);

				data->shader_macros[index].name_stream.CopyOther(data->shader_metadata.macros[index].name);
				data->shader_macros[index].definition_stream.CopyOther(data->shader_metadata.macros[index].definition);
			}
			data->shader_macros.size = data->shader_metadata.macros.size;
		}		

		// Determine the shader type
		ECS_SHADER_TYPE shader_type = AssetExtensionTypeShader(PathExtension(data->path));
		if (shader_type != ECS_SHADER_TYPE_COUNT) {
			if (shader_type != data->shader_metadata.shader_type) {
				data->shader_metadata.shader_type = shader_type;
			}
		}

		data->timer.DelayStart(-LAZY_EVALUATION_MILLISECONDS, ECS_TIMER_DURATION_MS);
	}

	// Lazy retrieve the shader data
	RetrieveSourceCode(editor_state, data);

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ASSET_SHADER_ICON, drawer->color_theme.text, drawer->color_theme.theme);

	InspectorIconNameAndPath(drawer, data->path);
	InspectorDrawFileTimes(drawer, data->path);
	InspectorOpenAndShowButton(drawer, data->path);
	drawer->CrossLine();

	UIDrawConfig config;

	// Display a warning if the target is invalid
	bool target_file_is_valid = true;
	if (!ExistsFileOrFolder(data->target_file)) {
		target_file_is_valid = false;
	}
	else {
		// Verify the extension
		target_file_is_valid = PathExtension(data->target_file) == ECS_SHADER_EXTENSION;
	}

	if (!target_file_is_valid) {
		Stream<char> message = "The target file is not valid";

		UIDrawerRowLayout row_layout = drawer->GenerateRowLayout();
		row_layout.SetHorizontalAlignment(ECS_UI_ALIGN_MIDDLE);

		row_layout.AddSquareLabel();
		row_layout.AddLabel(message);

		size_t configuration = 0;
		row_layout.GetTransform(config, configuration);

		drawer->SpriteRectangle(configuration, config, ECS_TOOLS_UI_TEXTURE_WARN_ICON, EDITOR_YELLOW_COLOR);
		
		configuration = 0;
		config.flag_count = 0;
		row_layout.GetTransform(config, configuration);

		drawer->Text(configuration | UI_CONFIG_ALIGN_TO_ROW_Y, config, message);
		drawer->NextRow();
	}

	// Draw the settings
	UIConfigNamePadding name_padding;
	name_padding.total_length = ASSET_NAME_PADDING;
	config.AddFlag(name_padding);

	UIConfigWindowDependentSize dependent_size;
	config.AddFlag(dependent_size);

	const size_t BASE_CONFIGURATION = UI_CONFIG_NAME_PADDING | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ELEMENT_NAME_FIRST;

	BuiltinCallbackData builtin_callback_data = { editor_state, data };
	UIConfigComboBoxCallback builtin_combo_callback;
	builtin_combo_callback.handler = { BuiltinCallback, &builtin_callback_data, sizeof(builtin_callback_data) };
	config.AddFlag(builtin_combo_callback);

	bool builtin_unavailable_values[std::size(EDITOR_SHADER_BUILTIN_NAME)];
	// Determine which shaders are allowed for this type
	for (size_t index = 0; index < std::size(EDITOR_SHADER_BUILTIN_NAME); index++) {
		ECS_SHADER_TYPE shader_type = EDITOR_SHADER_BUILTIN_TYPE[index];
		if (shader_type == data->shader_metadata.shader_type || shader_type == ECS_SHADER_TYPE_COUNT) {
			builtin_unavailable_values[index] = false;
		}
		else {
			builtin_unavailable_values[index] = true;
		}
	}

	UIConfigComboBoxUnavailable builtin_unavailables;
	builtin_unavailables.unavailables = builtin_unavailable_values;
	config.AddFlag(builtin_unavailables);

	// Determine if the builtin has changed - if it did, update the flag
	data->builtin = FindShaderBuiltinIndex(&data->shader_metadata);

	// Draw the combo box indicating builtins
	drawer->ComboBox(
		BASE_CONFIGURATION | UI_CONFIG_COMBO_BOX_CALLBACK | UI_CONFIG_COMBO_BOX_UNAVAILABLE,
		config,
		"Builtin", 
		{ EDITOR_SHADER_BUILTIN_NAME, std::size(EDITOR_SHADER_BUILTIN_NAME) }, 
		std::size(EDITOR_SHADER_BUILTIN_NAME), 
		(unsigned char*)&data->builtin
	);
	drawer->NextRow();

	// Combo callback and unavailables
	config.flag_count -= 2;

	struct FileInputTargetCallbackData {
		InspectorDrawShaderFileData* draw_data;
		EditorState* editor_state;
	};

	auto file_input_target_callback = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		FileInputTargetCallbackData* data = (FileInputTargetCallbackData*)_data;
		CapacityStream<wchar_t>* previous_path = (CapacityStream<wchar_t>*)_additional_data;

		if (*previous_path != data->draw_data->target_file) {
			data->draw_data->shader_metadata.file = data->draw_data->target_file;

			AssetSettingsHelperChangedWithFileActionData changed_data;
			changed_data.editor_state = data->editor_state;
			changed_data.asset_type = ECS_ASSET_SHADER;
			changed_data.asset = &data->draw_data->shader_metadata;
			changed_data.thunk_file_path = data->draw_data->path;
			changed_data.previous_name = data->draw_data->shader_metadata.name;
			changed_data.previous_target_file = *previous_path;
			changed_data.target_database = data->editor_state->asset_database;

			action_data->data = &changed_data;
			AssetSettingsHelperChangedWithFileAction(action_data);

			// Check to see if the new target path exists
			if (data->draw_data->target_file.size > 0 && !ExistsFileOrFolder(data->draw_data->target_file)) {
				// Print a message
				ECS_FORMAT_TEMP_STRING(console_message, "Shader {#} has invalid target {#}.", data->draw_data->shader_metadata.name, data->draw_data->target_file);
				EditorSetConsoleError(console_message);
			}
		}
	};

	FileInputTargetCallbackData file_input_callback_data;
	file_input_callback_data.editor_state = editor_state;
	file_input_callback_data.draw_data = data;

	UIConfigPathInputCallback path_callback;
	path_callback.callback = { file_input_target_callback, &file_input_callback_data, sizeof(file_input_callback_data) };
	config.AddFlag(path_callback);

	unsigned int module_count = editor_state->project_modules->size;
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<wchar_t>, module_roots, module_count + 1);
	ECS_STACK_CAPACITY_STREAM(wchar_t, module_roots_storage, ECS_KB * 8);

	for (unsigned int index = 0; index < module_count; index++) {
		unsigned int offset = module_roots_storage.size;
		bool success = GetModuleReflectSolutionPath(editor_state, index, module_roots_storage);
		if (success) {
			module_roots[index] = { module_roots_storage.buffer + offset, module_roots_storage.size - offset };
		}
		else {
			module_roots[index] = { nullptr, 0 };
		}
	}
	// The last "module" root is actually the assets folder
	unsigned int module_offset = module_roots_storage.size;
	GetProjectAssetsFolder(editor_state, module_roots_storage);
	module_roots[module_count] = { module_roots_storage.buffer + module_offset, module_roots_storage.size - module_offset };

	UIConfigPathInputRoot roots;
	roots.roots = { module_roots.buffer, module_count + 1 };
	config.AddFlag(roots);

	Stream<wchar_t> shader_file_extensions[] = {
		ECS_SHADER_EXTENSION
	};

	UIConfigTextInputMisc path_input_misc = { true };
	config.AddFlag(path_input_misc);

	drawer->FileInput(
		BASE_CONFIGURATION | UI_CONFIG_PATH_INPUT_CALLBACK | UI_CONFIG_PATH_INPUT_ROOT | UI_CONFIG_TEXT_INPUT_MISC, 
		config, 
		"Target File",
		&data->target_file, 
		{ shader_file_extensions, std::size(shader_file_extensions) }
	);
	drawer->NextRow();
	config.flag_count -= 2;

	const Reflection::ReflectionManager* reflection_manager = editor_state->EditorReflectionManager();
	const Reflection::ReflectionEnum* shader_type_enum_ = reflection_manager->GetEnum(STRING(ECS_SHADER_TYPE));
	const Reflection::ReflectionEnum* compile_flag_enum_ = reflection_manager->GetEnum(STRING(ECS_SHADER_COMPILE_FLAGS));

	AssetSettingsHelperChangedWithFileActionData combo_changed_data;
	combo_changed_data.asset_type = ECS_ASSET_SHADER;
	combo_changed_data.editor_state = editor_state;
	combo_changed_data.asset = &data->shader_metadata;
	combo_changed_data.thunk_file_path = data->path;
	combo_changed_data.previous_name = data->shader_metadata.name;
	combo_changed_data.previous_target_file = data->shader_metadata.file;
	combo_changed_data.target_database = editor_state->asset_database;
	UIActionHandler modified_callback = { AssetSettingsHelperChangedWithFileAction, &combo_changed_data, sizeof(combo_changed_data) };

	UIConfigComboBoxCallback combo_callback;
	combo_callback.handler = modified_callback;
	config.AddFlag(combo_callback);

	UIConfigActiveState active_state;
	active_state.state = false;
	config.AddFlag(active_state);

	drawer->ComboBox(
		BASE_CONFIGURATION | UI_CONFIG_COMBO_BOX_CALLBACK | UI_CONFIG_ACTIVE_STATE,
		config,
		"Shader Type",
		shader_type_enum_->fields,
		shader_type_enum_->fields.size,
		(unsigned char*)&data->shader_metadata.shader_type
	);
	drawer->NextRow();
	config.flag_count--;

	drawer->ComboBox(
		BASE_CONFIGURATION | UI_CONFIG_COMBO_BOX_CALLBACK,
		config,
		"Compile Level",
		compile_flag_enum_->fields,
		compile_flag_enum_->fields.size,
		(unsigned char*)&data->shader_metadata.compile_flag
	);
	drawer->NextRow();

	config.flag_count = 0;

	AddMacroData add_data = { data, editor_state };

	UIConfigArrayAddCallback config_add_callback;
	config_add_callback.handler = { AddMacroCallback, &add_data, sizeof(add_data) };
	config.AddFlag(config_add_callback);

	RemoveAnywhereMacroData remove_anywhere_data;
	remove_anywhere_data.editor_state = editor_state;
	remove_anywhere_data.draw_data = data;

	UIConfigArrayRemoveAnywhere remove_anywhere_config;
	remove_anywhere_config.handler = { RemoveAnywhereCallback, &remove_anywhere_data, sizeof(remove_anywhere_data) };
	config.AddFlag(remove_anywhere_config);

	struct RemoveData {
		InspectorDrawShaderFileData* draw_data;
		EditorState* editor_state;
	};

	auto remove_callback = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		RemoveData* data = (RemoveData*)_data;
		UIDrawerArrayAddRemoveData* additional_data = (UIDrawerArrayAddRemoveData*)_additional_data;
		unsigned int remove_index = additional_data->new_size;
		for (unsigned int index = remove_index; index < data->draw_data->shader_macros.size; index++) {
			data->editor_state->editor_allocator->Deallocate(data->draw_data->shader_macros[index].definition_stream.buffer);
			data->editor_state->editor_allocator->Deallocate(data->draw_data->shader_macros[index].name_stream.buffer);
		}
	};

	RemoveData remove_data = { data, editor_state };
	UIConfigArrayRemoveCallback config_remove;
	config_remove.handler = { remove_callback, &remove_data, sizeof(remove_data) };
	config.AddFlag(config_remove);

	struct PreMacroArrayDrawData {
		EditorState* editor_state;
		InspectorDrawShaderFileData* data;
	};

	auto pre_macro_array_draw = [](UIDrawer* drawer, void* _data) {
		PreMacroArrayDrawData* data = (PreMacroArrayDrawData*)_data;
		
		struct AddConditionalMacroData {
			EditorState* editor_state;
			InspectorDrawShaderFileData* data;
			unsigned int conditional_index;
		};

		auto add_conditional_macro = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			AddConditionalMacroData* data = (AddConditionalMacroData*)_data;
			// Resize the input
			UIDrawerArrayAddRemoveData add_remove_data;
			add_remove_data.array_data = nullptr;
			add_remove_data.resizable_data = (ResizableStream<void>*)&data->data->shader_macros;
			unsigned int write_index = data->data->shader_macros.size;
			// The starting index
			add_remove_data.new_size = write_index;
			data->data->shader_macros.Resize(data->data->shader_macros.size + 1);
			data->data->shader_macros.size = data->data->shader_macros.capacity;

			AddMacroData add_data = { data->data, data->editor_state };
			action_data->data = &add_data;
			action_data->additional_data = &add_remove_data;
			AddMacroCallback(action_data);

			// Now copy the macro
			data->data->shader_macros[write_index].name_stream.CopyOther(data->data->conditional_macros[data->conditional_index]);
			data->data->shader_macros[write_index].name_stream[data->data->shader_macros[write_index].name_stream.size] = '\0';
			data->data->shader_metadata.AddMacro(data->data->shader_macros[write_index].name_stream.buffer, "", data->editor_state->asset_database->Allocator());

			// Write to the asset and file
			AssetSettingsHelperChangedWithFileActionData changed_data;
			changed_data.asset = &data->data->shader_metadata;
			changed_data.asset_type = ECS_ASSET_SHADER;
			changed_data.editor_state = data->editor_state;
			changed_data.previous_name = data->data->shader_metadata.name;
			changed_data.previous_target_file = data->data->shader_metadata.file;
			changed_data.target_database = data->editor_state->asset_database;
			action_data->data = &changed_data;
			AssetSettingsHelperChangedWithFileAction(action_data);
		};

		struct RemoveConditionalMacroData {
			EditorState* editor_state;
			InspectorDrawShaderFileData* data;
			unsigned int conditional_index;
		};

		auto remove_conditional_macro = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			RemoveConditionalMacroData* data = (RemoveConditionalMacroData*)_data;
			unsigned int remove_index = FindString<ShaderMacroUI>(data->data->conditional_macros[data->conditional_index], data->data->shader_macros, [](ShaderMacroUI macro_ui) {
				return macro_ui.name_stream;
			});
			ECS_ASSERT(remove_index != -1);

			RemoveAnywhereMacroData remove_data = { data->data, data->editor_state };
			action_data->data = &remove_data;
			action_data->additional_data = &remove_index;
			RemoveAnywhereCallback(action_data);

			// Resize the buffer
			data->data->shader_macros.RemoveSwapBack(remove_index);
			data->data->shader_macros.Resize(data->data->shader_macros.size);
		};

		// Only if there are conditional macros do draw
		if (data->data->conditional_macros.size > 0) {
			drawer->CrossLine();
			drawer->Text("Conditional macros:");
			UIDrawConfig config;

			drawer->OffsetNextRow(drawer->layout.node_indentation);

			for (size_t index = 0; index < data->data->conditional_macros.size; index++) {
				drawer->NextRow();

				UIDrawerRowLayout row_layout = drawer->GenerateRowLayout();
				row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });
				row_layout.AddSquareLabel();
				row_layout.AddSquareLabel();

				size_t configuration = 0;
				row_layout.GetTransform(config, configuration);

				drawer->TextLabel(configuration, config, data->data->conditional_macros[index]);

				configuration = 0;
				config.flag_count = 0;
				row_layout.GetTransform(config, configuration);

				unsigned int subindex = FindString<ShaderMacroUI>(data->data->conditional_macros[index], data->data->shader_macros, [](ShaderMacroUI ui_macro) {
					return ui_macro.name_stream;
				});

				UIConfigActiveState active_state;
				active_state.state = subindex == -1;
				config.AddFlag(active_state);
				
				AddConditionalMacroData add_conditional_data;
				add_conditional_data.data = data->data;
				add_conditional_data.editor_state = data->editor_state;
				add_conditional_data.conditional_index = index;

				drawer->SpriteButton(
					configuration | UI_CONFIG_ACTIVE_STATE, 
					config, 
					{ add_conditional_macro, &add_conditional_data, sizeof(add_conditional_data) }, 
					ECS_TOOLS_UI_TEXTURE_PLUS, 
					drawer->color_theme.text
				);
			
				configuration = 0;
				config.flag_count = 0;
				row_layout.GetTransform(config, configuration);

				active_state.state = !active_state.state;
				config.AddFlag(active_state);
				
				RemoveConditionalMacroData remove_conditional_data;
				remove_conditional_data.data = data->data;
				remove_conditional_data.editor_state = data->editor_state;
				remove_conditional_data.conditional_index = index;

				drawer->SpriteButton(
					configuration | UI_CONFIG_ACTIVE_STATE, 
					config,
					{ remove_conditional_macro, &remove_conditional_data, sizeof(remove_conditional_data) }, 
					ECS_TOOLS_UI_TEXTURE_MINUS,
					drawer->color_theme.text
				);
			}

			drawer->OffsetNextRow(-drawer->layout.node_indentation);
			drawer->NextRow();
			drawer->CrossLine();
		}
	};

	PreMacroArrayDrawData pre_macro_draw_data;
	pre_macro_draw_data.editor_state = editor_state;
	pre_macro_draw_data.data = data;

	UIConfigArrayPrePostDraw pre_post_draw;
	pre_post_draw.pre_function = pre_macro_array_draw;
	pre_post_draw.pre_data = &pre_macro_draw_data;
	config.AddFlag(pre_post_draw);

	DrawShaderMacroData draw_shader_macro_data = { editor_state, data };

	UIDrawConfig array_macro_config;
	array_macro_config.AddFlag(dependent_size);
	array_macro_config.AddFlag(name_padding);

	drawer->Array(UI_CONFIG_ARRAY_ADD_CALLBACK | UI_CONFIG_ARRAY_REMOVE_ANYWHERE | UI_CONFIG_ARRAY_REMOVE_CALLBACK
		| UI_CONFIG_ARRAY_DISABLE_DRAG | UI_CONFIG_ARRAY_PRE_POST_DRAW, BASE_CONFIGURATION, config, &array_macro_config, "Macros", &data->shader_macros, &draw_shader_macro_data, DrawShaderMacro);

	drawer->CrossLine();
}

void ChangeInspectorToShaderFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	InspectorDrawShaderFileData data;
	data.path = path;
	memset(&data.shader_metadata, 0, sizeof(data.shader_metadata));

	// Allocate the data and embedd the path in it
	// Later on. It is fine to read from the stack more bytes
	uint3 inspector_indices = ChangeInspectorDrawFunctionWithSearchEx(
		editor_state,
		inspector_index,
		{ InspectorDrawShaderFile, InspectorCleanShader },
		&data,
		sizeof(data) + sizeof(wchar_t) * path.size,
		-1,
		[=](void* inspector_data) {
			InspectorDrawShaderFileData* other_data = (InspectorDrawShaderFileData*)inspector_data;
			return other_data->path == path;
		}
	);

	if (inspector_indices.x == -1 && inspector_indices.y != -1) {
		// Get the data and set the path
		InspectorDrawShaderFileData* draw_data = (InspectorDrawShaderFileData*)GetInspectorDrawFunctionData(editor_state, inspector_indices.y);
		draw_data->path = { OffsetPointer(draw_data, sizeof(*draw_data)), path.size };
		draw_data->path.CopyOther(path);
	}
}

void InspectorShaderFileAddFunctors(InspectorTable* table) {
	for (size_t index = 0; index < std::size(ASSET_SHADER_EXTENSIONS); index++) {
		AddInspectorTableFunction(table, { InspectorDrawShaderFile, InspectorCleanShader }, ASSET_SHADER_EXTENSIONS[index]);
	}
}

InspectorAssetTarget InspectorDrawShaderTarget(const void* inspector_data)
{
	InspectorDrawShaderFileData* data = (InspectorDrawShaderFileData*)inspector_data;
	return { data->path, {} };
}
