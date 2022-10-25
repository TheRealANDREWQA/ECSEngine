#include "editorpch.h"
#include "../Inspector.h"
#include "../../Editor/EditorState.h"
#include "InspectorModule.h"
#include "../../Editor/EditorPalette.h"
#include "InspectorUtilities.h"

#include "../CreateScene.h"
#include "../../Modules/Module.h"

struct DrawSandboxSettingsData {
	EditorState* editor_state;
	Stream<char> ui_reflection_instance_name;
	WorldDescriptor ui_descriptor;
	LinearAllocator runtime_settings_allocator;
	size_t last_write_descriptor;

	bool collapsing_module_state;
	bool collapsing_runtime_state;
};

void InspectorDrawSandboxSettingsClean(EditorState* editor_state, void* _data) {
	DrawSandboxSettingsData* data = (DrawSandboxSettingsData*)_data;
	// Destroy the UI reflection
	editor_state->ui_reflection->DestroyInstance(data->ui_reflection_instance_name.buffer);

	// Deallocate the name
	editor_state->editor_allocator->Deallocate(data->ui_reflection_instance_name.buffer);

	// Deallocate the linear allocator
	editor_state->editor_allocator->Deallocate(data->runtime_settings_allocator.m_buffer);
}

struct DrawSandboxSelectionWindowData {
	EditorState* editor_state;
	unsigned int selection;
	unsigned int sandbox_index;

	EDITOR_MODULE_CONFIGURATION configuration;
};

void DrawSandboxSelectionWindow(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	DrawSandboxSelectionWindowData* data = (DrawSandboxSelectionWindowData*)window_data;
	EditorState* editor_state = data->editor_state;

	// Display the combo box for configuration selection
	drawer.ComboBox("Module configuration", { MODULE_CONFIGURATIONS, EDITOR_MODULE_CONFIGURATION_COUNT }, EDITOR_MODULE_CONFIGURATION_COUNT, (unsigned char*)&data->configuration);
	drawer.NextRow();

	UIDrawConfig config;

	float2 square_scale = drawer.GetSquareScale();
	float scale_until_border = drawer.GetWindowScaleUntilBorder();

	UIConfigWindowDependentSize dependent_size;
	dependent_size.scale_factor.x = drawer.GetWindowSizeFactors(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, { scale_until_border - square_scale.x - drawer.layout.element_indentation, 0.0f }).x;
	config.AddFlag(dependent_size);

	const EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
	ProjectModules* project_modules = editor_state->project_modules;
	// Display all the modules now
	for (unsigned int index = 0; index < project_modules->size; index++) {
		bool is_graphics = project_modules->buffer[index].is_graphics_module;

		const wchar_t* texture_to_display = is_graphics ? GRAPHICS_MODULE_TEXTURE : ECS_TOOLS_UI_TEXTURE_MODULE;
		drawer.SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, texture_to_display, drawer.color_theme.theme);

		size_t configuration = UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_ACTIVE_STATE | UI_CONFIG_WINDOW_DEPENDENT_SIZE;
		// Look to see if it already belongs to the sandbox
		unsigned int subindex = 0;

		config.flag_count = 1;

		UIConfigActiveState active_state;
		active_state.state = true;
		for (; subindex < sandbox->modules_in_use.size; subindex++) {
			if (sandbox->modules_in_use[subindex].module_index == index) {
				active_state.state = false;
				UIConfigColor background_color;
				background_color.color = ECS_COLOR_GREEN;
				config.AddFlag(background_color);

				configuration = function::ClearFlag(configuration, UI_CONFIG_LABEL_TRANSPARENT);
				configuration |= UI_CONFIG_COLOR;
				break;
			}
		}

		config.AddFlag(active_state);

		struct SelectData {
			DrawSandboxSelectionWindowData* data;
			unsigned int index;
		};

		auto select_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SelectData* data = (SelectData*)_data;
			data->data->selection = data->index;
		};

		if (index == data->selection) {
			configuration = function::ClearFlag(configuration, UI_CONFIG_LABEL_TRANSPARENT);
		}

		SelectData select_data = { data, index };
		drawer.ButtonWide(configuration, config, project_modules->buffer[index].library_name, { select_action, &select_data, sizeof(select_data) });
		drawer.NextRow();
	}

	auto confirm_data = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		DrawSandboxSelectionWindowData* data = (DrawSandboxSelectionWindowData*)_data;
		AddSandboxModule(data->editor_state, data->sandbox_index, data->selection, data->configuration);

		// Write the sandbox file
		SaveEditorSandboxFile(data->editor_state);
		DestroyCurrentActionWindow(action_data);
	};

	UIConfigActiveState active_state;
	active_state.state = data->selection != -1;

	// Now add and cancel buttons
	config.flag_count = 0;
	config.AddFlag(active_state);

	UIConfigAlignElement align_element;
	align_element.vertical = ECS_UI_ALIGN_BOTTOM;
	config.AddFlag(align_element);

	drawer.Button(UI_CONFIG_ACTIVE_STATE | UI_CONFIG_ALIGN_ELEMENT, config, "Confirm", { confirm_data, data, 0, ECS_UI_DRAW_SYSTEM });
	config.flag_count--;

	align_element.horizontal = ECS_UI_ALIGN_RIGHT;
	config.AddFlag(align_element);
	drawer.Button(UI_CONFIG_ALIGN_ELEMENT, config, "Cancel", { DestroyCurrentActionWindow, nullptr, 0, ECS_UI_DRAW_SYSTEM });
}

struct CreateAddSandboxWindowData {
	EditorState* editor_state;
	unsigned int sandbox_index;
};

void CreateAddSandboxWindow(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateAddSandboxWindowData* data = (CreateAddSandboxWindowData*)_data;
	UIWindowDescriptor descriptor;

	DrawSandboxSelectionWindowData window_data;
	window_data.editor_state = data->editor_state;
	window_data.selection = -1;
	window_data.sandbox_index = data->sandbox_index;
	window_data.configuration = EDITOR_MODULE_CONFIGURATION_DEBUG;

	descriptor.window_name = "Select a Module";

	descriptor.draw = DrawSandboxSelectionWindow;
	descriptor.window_data = &window_data;
	descriptor.window_data_size = sizeof(window_data);

	descriptor.destroy_action = ReleaseLockedWindow;

	descriptor.initial_size_x = 0.7f;
	descriptor.initial_size_y = 0.6f;

	system->CreateWindowAndDockspace(descriptor, UI_POP_UP_WINDOW_ALL);
}

void InspectorDrawSandboxSettings(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	EDITOR_STATE(editor_state);

	DrawSandboxSettingsData* data = (DrawSandboxSettingsData*)_data;
	unsigned int sandbox_index = GetInspectorTargetSandbox(editor_state, inspector_index);

	auto get_name = [](unsigned int index, CapacityStream<char>& name) {
		name.Copy("Sandbox ");
		function::ConvertIntToChars(name, index);
	};


	// Initialize the ui_reflection_instance if we haven't done so
	if (data->ui_reflection_instance_name.size == 0) {
		ECS_STACK_CAPACITY_STREAM(char, ui_reflection_name, 128);
		UIReflectionDrawer* ui_drawer = editor_state->ui_reflection;

		unsigned int current_index = 0;
		get_name(current_index, ui_reflection_name);
		// Try the UI reflection instance indices until we get a free slot
		UIReflectionInstance* instance = ui_drawer->GetInstancePtr(ui_reflection_name.buffer);
		while (instance != nullptr) {
			// Try again with the current_index
			ui_reflection_name.size = 0;
			get_name(current_index, ui_reflection_name);

			instance = ui_drawer->GetInstancePtr(ui_reflection_name.buffer);
			current_index++;
		}

		// Increase the index by 1 such that we include the '\0'
		ui_reflection_name.size++;
		void* allocation = editor_state->editor_allocator->Allocate(sizeof(char) * ui_reflection_name.size);
		ui_reflection_name.CopyTo(allocation);

		data->ui_reflection_instance_name = { allocation, ui_reflection_name.size - 1 };
		instance = ui_drawer->CreateInstance(data->ui_reflection_instance_name.buffer, STRING(WorldDescriptor));

		WorldDescriptor* sandbox_descriptor = GetSandboxWorldDescriptor(editor_state, sandbox_index);
		memcpy(&data->ui_descriptor, sandbox_descriptor, sizeof(data->ui_descriptor));

		// Bind the pointers
		ui_drawer->BindInstancePtrs(instance, &data->ui_descriptor);

		const size_t ALLOCATOR_SIZE = ECS_KB;
		data->runtime_settings_allocator = LinearAllocator(editor_allocator->Allocate(ALLOCATOR_SIZE), ALLOCATOR_SIZE);

		data->last_write_descriptor = 0;
		data->collapsing_runtime_state = false;
		data->collapsing_module_state = false;
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ECS_TOOLS_UI_TEXTURE_FILE_CONFIG, drawer->color_theme.text, drawer->color_theme.theme);

	UIDrawConfig config;

	ECS_STACK_CAPACITY_STREAM(char, sandbox_window_name, 128);
	get_name(sandbox_index, sandbox_window_name);
	sandbox_window_name.AddStream(" Settings");
	sandbox_window_name[sandbox_window_name.size] = '\0';
	drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, sandbox_window_name.buffer);
	drawer->NextRow();

	// Display a warning if there is no graphics module or there are multiple
	ECS_STACK_CAPACITY_STREAM(unsigned int, graphics_module_indices, 128);
	GetSandboxGraphicsModules(editor_state, sandbox_index, graphics_module_indices);
	if (graphics_module_indices.size == 0) {
		drawer->SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, ECS_TOOLS_UI_TEXTURE_WARN_ICON, EDITOR_YELLOW_COLOR);
		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, "Warning: There is no graphics module assigned.");
		drawer->NextRow();
	}
	else if (graphics_module_indices.size > 1) {
		drawer->SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, ECS_TOOLS_UI_TEXTURE_WARN_ICON, EDITOR_YELLOW_COLOR);
		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, "Warning: There are multiple graphics modules assigned.");
		drawer->NextRow();
	}

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	// Display a warning if no scene path is assigned
	if (sandbox->scene_path.size == 0) {
		drawer->SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, ECS_TOOLS_UI_TEXTURE_WARN_ICON, EDITOR_YELLOW_COLOR);
		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, "Warning: There is no scene assigned. The sandbox cannot run.");
		drawer->NextRow();
	}

	config.flag_count = 0;
	UIConfigActiveState active_state;
	active_state.state = sandbox->scene_path.size > 0;
	config.AddFlag(active_state);

	float2 square_scale = drawer->GetSquareScale();
	drawer->UpdateCurrentRowScale(square_scale.y);

	drawer->Text(UI_CONFIG_ACTIVE_STATE | UI_CONFIG_ALIGN_TO_ROW_Y, config, "Scene: ");
	if (sandbox->scene_path.size == 0) {
		drawer->Text(UI_CONFIG_ACTIVE_STATE | UI_CONFIG_ALIGN_TO_ROW_Y, config, "No scene is assigned.");
	}
	else {
		drawer->TextWide(UI_CONFIG_ACTIVE_STATE | UI_CONFIG_ALIGN_TO_ROW_Y, config, sandbox->scene_path);
	}

	ChangeSandboxSceneActionData change_scene_data = { editor_state, sandbox_index };
	drawer->SpriteButton(
		UI_CONFIG_MAKE_SQUARE,
		config,
		{ ChangeSandboxSceneAction, &change_scene_data, sizeof(change_scene_data), ECS_UI_DRAW_SYSTEM },
		ECS_TOOLS_UI_TEXTURE_FOLDER
	);
	drawer->NextRow();

	drawer->CollapsingHeader("Modules", &data->collapsing_module_state, [&]() {
		// Display the count of modules in use

		ECS_STACK_CAPACITY_STREAM(char, in_use_stream, 256);
		in_use_stream.Copy("In use: ");
		function::ConvertIntToChars(in_use_stream, sandbox->modules_in_use.size);
		drawer->Text(in_use_stream);
		drawer->NextRow();

		UIDrawConfig module_config;
		UIConfigComboBoxPrefix combo_prefix;
		combo_prefix.prefix = "Configuration";
		module_config.AddFlag(combo_prefix);

		struct ComboBoxCallbackData {
			EditorState* editor_state;
		};

		auto combo_callback_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ComboBoxCallbackData* data = (ComboBoxCallbackData*)_data;
			SaveEditorSandboxFile(data->editor_state);
		};

		ComboBoxCallbackData callback_data = { editor_state };
		UIConfigComboBoxCallback combo_callback;
		combo_callback.handler = { combo_callback_action, &callback_data, sizeof(callback_data) };
		module_config.AddFlag(combo_callback);

		// Push the pattern for unique combo names
		drawer->PushIdentifierStackStringPattern();

		struct RemoveModuleActionData {
			EditorState* editor_state;
			unsigned int sandbox_index;
			unsigned int in_module_index;
		};

		auto remove_module_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			RemoveModuleActionData* data = (RemoveModuleActionData*)_data;
			RemoveSandboxModuleInStream(data->editor_state, data->sandbox_index, data->in_module_index);

			// Save the sandbox file as well
			SaveEditorSandboxFile(data->editor_state);
		};

		struct ActivateDeactivateData {
			EditorState* editor_state;
			unsigned int sandbox_index;
			unsigned int in_stream_index;
		};

		auto activate_deactivate_button = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ActivateDeactivateData* data = (ActivateDeactivateData*)_data;
			if (IsSandboxModuleDeactivatedInStream(data->editor_state, data->sandbox_index, data->in_stream_index)) {
				ActivateSandboxModuleInStream(data->editor_state, data->sandbox_index, data->in_stream_index);
			}
			else {
				DeactivateSandboxModuleInStream(data->editor_state, data->sandbox_index, data->in_stream_index);
			}

			SaveEditorSandboxFile(data->editor_state);
		};

		ECS_STACK_CAPACITY_STREAM(unsigned int, module_display_order, 64);
		module_display_order.size = sandbox->modules_in_use.size;
		function::MakeSequence(module_display_order);

		for (unsigned int index = 0; index < graphics_module_indices.size; index++) {
			for (unsigned int subindex = 0; subindex < module_display_order.size; subindex++) {
				if (module_display_order[subindex] == graphics_module_indices[index]) {
					module_display_order.Swap(index, subindex);
				}
			}
		}

		// Draw the modules in use, first the graphics modules	
		for (unsigned int index = 0; index < module_display_order.size; index++) {
			unsigned int in_stream_index = module_display_order[index];

			// Draw the remove X button
			RemoveModuleActionData remove_data;
			remove_data.editor_state = editor_state;
			remove_data.in_module_index = index;
			remove_data.sandbox_index = sandbox_index;
			drawer->SpriteButton(UI_CONFIG_MAKE_SQUARE, module_config, { remove_module_action, &remove_data, sizeof(remove_data) }, ECS_TOOLS_UI_TEXTURE_X);

			bool is_graphics_module = IsGraphicsModule(editor_state, sandbox->modules_in_use[in_stream_index].module_index);
			const wchar_t* module_texture = is_graphics_module ? GRAPHICS_MODULE_TEXTURE : ECS_TOOLS_UI_TEXTURE_MODULE;

			const size_t CONFIGURATION = UI_CONFIG_MAKE_SQUARE;
			const wchar_t* status_texture = nullptr;
			Color status_color;

			const EditorModuleInfo* info = GetModuleInfo(editor_state, sandbox->modules_in_use[in_stream_index].module_index, sandbox->modules_in_use[in_stream_index].module_configuration);
			switch (info->load_status) {
			case EDITOR_MODULE_LOAD_GOOD:
				status_texture = ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK;
				status_color = ECS_COLOR_GREEN;
				break;
			case EDITOR_MODULE_LOAD_OUT_OF_DATE:
				status_texture = ECS_TOOLS_UI_TEXTURE_CLOCK;
				status_color = ECS_COLOR_YELLOW;
				break;
			case EDITOR_MODULE_LOAD_FAILED:
				status_texture = ECS_TOOLS_UI_TEXTURE_MINUS;
				status_color = ECS_COLOR_RED;
				break;
			default:
				ECS_ASSERT(false, "Invalid load status for module");
			}

			size_t label_configuration = UI_CONFIG_LABEL_TRANSPARENT;

			if (IsSandboxModuleDeactivatedInStream(editor_state, sandbox_index, index)) {
				status_color.alpha = 100;
				label_configuration |= UI_CONFIG_UNAVAILABLE_TEXT;
			}

			ActivateDeactivateData activate_deactivate_data = { editor_state, sandbox_index, index };
			drawer->SpriteButton(CONFIGURATION, module_config, { activate_deactivate_button, &activate_deactivate_data, sizeof(activate_deactivate_data) }, module_texture, status_color);
			drawer->SpriteRectangle(CONFIGURATION, module_config, status_texture, status_color);

			Stream<wchar_t> module_library = editor_state->project_modules->buffer[sandbox->modules_in_use[in_stream_index].module_index].library_name;
			drawer->TextLabelWide(label_configuration, module_config, module_library);

			float x_bound = drawer->GetCurrentPositionNonOffset().x;

			drawer->PushIdentifierStackRandom(index);

			UIConfigAlignElement align_element;
			align_element.horizontal = ECS_UI_ALIGN_RIGHT;
			module_config.AddFlag(align_element);

			drawer->ComboBox(
				UI_CONFIG_ALIGN_ELEMENT | UI_CONFIG_COMBO_BOX_NO_NAME,
				module_config,
				"configuration",
				{ MODULE_CONFIGURATIONS, EDITOR_MODULE_CONFIGURATION_COUNT },
				EDITOR_MODULE_CONFIGURATION_COUNT,
				(unsigned char*)&sandbox->modules_in_use[in_stream_index].module_configuration
			);
			module_config.flag_count--;

			drawer->PopIdentifierStack();

			drawer->NextRow();
		}

		drawer->PopIdentifierStack();

		// The add button
		CreateAddSandboxWindowData create_data;
		create_data.editor_state = editor_state;
		create_data.sandbox_index = sandbox_index;
		drawer->Button("Add Module", { CreateAddSandboxWindow, &create_data, sizeof(create_data), ECS_UI_DRAW_SYSTEM });
		});

	drawer->CollapsingHeader("Runtime Settings", &data->collapsing_runtime_state, [&]() {
		// Display all the available settings
		ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, available_settings, 128);
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

		// Reload the values if the lazy evaluation has finished
		if (data->last_write_descriptor != sandbox->runtime_settings_last_write) {
			memcpy(&data->ui_descriptor, GetSandboxWorldDescriptor(editor_state, sandbox_index), sizeof(data->ui_descriptor));
			data->last_write_descriptor = sandbox->runtime_settings_last_write;
		}

		// Reset the linear allocator
		data->runtime_settings_allocator.Clear();
		GetSandboxAvailableRuntimeSettings(editor_state, available_settings, GetAllocatorPolymorphic(&data->runtime_settings_allocator));
		if (available_settings.size == 0) {
			drawer->Text("There are no available settings.");
			drawer->NextRow();
		}
		else {
			drawer->Text("Available runtime settings:");
			drawer->NextRow();

			struct SelectData {
				DrawSandboxSettingsData* draw_data;
				unsigned int sandbox_index;
				Stream<wchar_t> path;
			};

			auto select_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				SelectData* data = (SelectData*)_data;
				if (ChangeSandboxRuntimeSettings(data->draw_data->editor_state, data->sandbox_index, data->path)) {
					const WorldDescriptor* sandbox_descriptor = GetSandboxWorldDescriptor(data->draw_data->editor_state, data->sandbox_index);
					memcpy(&data->draw_data->ui_descriptor, sandbox_descriptor, sizeof(*sandbox_descriptor));

					// Save the sandbox file as well
					SaveEditorSandboxFile(data->draw_data->editor_state);
				}
			};

			UIDrawConfig config;
			UIConfigWindowDependentSize dependent_size;
			config.AddFlag(dependent_size);

			const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
			for (unsigned int index = 0; index < available_settings.size; index++) {
				SelectData select_data = { data, sandbox_index, available_settings[index] };

				size_t configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_TRANSPARENT;
				if (function::CompareStrings(available_settings[index], sandbox->runtime_settings)) {
					configuration = function::ClearFlag(configuration, UI_CONFIG_LABEL_TRANSPARENT);
				}

				drawer->ButtonWide(configuration, config, available_settings[index], { select_action, &select_data, sizeof(select_data) });
				drawer->NextRow();
			}
		}

		drawer->CrossLine();

		drawer->Text("Current Settings: ");
		if (sandbox->runtime_settings.size > 0) {
			drawer->TextWide(sandbox->runtime_settings);
		}
		else {
			drawer->Text("No settings selected (default values)");
		}

		drawer->NextRow();

		UIConfigWindowDependentSize dependent_size;
		config.AddFlag(dependent_size);

		UIReflectionDrawInstanceOptions options;
		options.drawer = drawer;
		options.config = &config;
		options.global_configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE;
		editor_state->ui_reflection->DrawInstance(data->ui_reflection_instance_name.buffer, &options);

		struct SaveCallbackData {
			DrawSandboxSettingsData* data;
			unsigned int sandbox_index;
		};

		auto save_callback = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SaveCallbackData* data = (SaveCallbackData*)_data;
			if (ValidateWorldDescriptor(&data->data->ui_descriptor)) {
				SetSandboxRuntimeSettings(data->data->editor_state, data->sandbox_index, data->data->ui_descriptor);
				// If it errors it will report to the user.
				SaveSandboxRuntimeSettings(data->data->editor_state, data->sandbox_index);
			}
			else {
				EditorSetConsoleError("The current runtime setting values are not valid. Increase the global allocator size.");
				// Copy the old values
				memcpy(&data->data->ui_descriptor, GetSandboxWorldDescriptor(data->data->editor_state, data->sandbox_index), sizeof(data->data->ui_descriptor));
			}
		};

		SaveCallbackData save_data;
		save_data.data = data;
		save_data.sandbox_index = sandbox_index;
		drawer->Button("Save", { save_callback, &save_data, sizeof(save_data) });

		UIConfigAbsoluteTransform default_transform;
		default_transform.scale = drawer->GetLabelScale("Default");
		default_transform.position = drawer->GetAlignedToRight(default_transform.scale.x);

		auto default_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			DrawSandboxSettingsData* data = (DrawSandboxSettingsData*)_data;
			data->ui_descriptor = GetDefaultWorldDescriptor();
		};

		config.flag_count = 0;
		config.AddFlag(default_transform);
		drawer->Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, "Default", { default_action, data, 0 });

		drawer->NextRow();
		drawer->CrossLine();
	});
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToSandboxSettings(EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index)
{
	DrawSandboxSettingsData data;
	data.editor_state = editor_state;
	data.ui_reflection_instance_name = { nullptr, 0 };
	unsigned int matched_inspector_index = ChangeInspectorDrawFunction(
		editor_state,
		inspector_index,
		{ InspectorDrawSandboxSettings, InspectorDrawSandboxSettingsClean },
		&data,
		sizeof(data),
		sandbox_index
	);

	if (matched_inspector_index == -1 && sandbox_index != -1) {
		// Create a new inspector instance and set it to the sandbox with the draw on the settings
		matched_inspector_index = CreateInspectorInstance(editor_state);
		CreateInspectorDockspace(editor_state, matched_inspector_index);
		SetInspectorTargetSandbox(editor_state, matched_inspector_index, sandbox_index);
		ChangeInspectorToSandboxSettings(editor_state, matched_inspector_index);
	}

	// Set the new active window to be this inspector
	ECS_STACK_CAPACITY_STREAM(char, inspector_window_name, 128);
	GetInspectorName(matched_inspector_index, inspector_window_name);
	editor_state->ui_system->SetActiveWindow(inspector_window_name);
}