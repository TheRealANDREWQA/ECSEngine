#include "editorpch.h"
#include "../Inspector.h"
#include "../../Editor/EditorState.h"
#include "InspectorModule.h"
#include "../../Editor/EditorPalette.h"
#include "InspectorUtilities.h"
#include "../../Sandbox/Sandbox.h"
#include "../../Sandbox/SandboxFile.h"
#include "../../Sandbox/SandboxModule.h"
#include "../../Sandbox/SandboxProfiling.h"
#include "../../Sandbox/SandboxEntityOperations.h"
#include "../../Sandbox/SandboxRecording.h"
#include "../../Sandbox/SandboxReplay.h"
#include "../Common.h"
#include "../../Project/ProjectFolders.h"

#include "../CreateScene.h"
#include "../../Modules/Module.h"
#include "../../Modules/ModuleSettings.h"

#define NAME_PADDING_LENGTH 0.30f
#define AVAILABLE_RUNTIME_SETTINGS_ALLOCATOR_CAPACITY ECS_KB
#define AVAILABLE_MODULE_SETTINGS_ALLOCATOR_CAPACITY ECS_KB * 4
#define AVAILABLE_MODULE_SETTINGS_BACKUP_ALLOCATOR_CAPACITY ECS_KB * 4
#define AVAILABLE_MODULE_SETTINGS_ENTRY_CAPACITY 128

// Use a large refresh time for this window since modules can be added
// From other input sources and this window might miss them
#define RETAINED_MODE_REFRESH_MS 250

// How many ms it takes to validate that the recording path is valid
#define RECORDING_FILE_VALID_STATUS_LAZY_MS 300
// This value is used when the input file is currently selected
#define RECORDING_FILE_VALID_STATUS_SHORT_LAZY_MS 50

// How many ms it takes to validate that the replay path is valid
#define REPLAY_FILE_VALID_STATUS_LAZY_MS 300
// This value is used when the input file is currently selected
#define REPLAY_FILE_VALID_STATUS_SHORT_LAZY_MS 50

struct DrawSandboxSettingsData {
	EditorState* editor_state;
	Stream<char> ui_reflection_instance_name;
	WorldDescriptor ui_descriptor;
	LinearAllocator runtime_settings_allocator;
	size_t last_write_descriptor;
	// This is used by the retained mode
	// To determine when the sandbox changes the run state
	// Such that we can redraw then
	unsigned int sandbox_index;
	EDITOR_SANDBOX_STATE run_state;

	bool collapsing_module_state;
	bool collapsing_components_state;
	bool collapsing_runtime_state;
	bool collapsing_modifiers_state;
	bool collapsing_statistics_state;
	bool collapsing_recording_state;
	bool collapsing_replay_state;

	struct AvailableModuleSettings {
		unsigned char label;
		Stream<Stream<char>> string_labels;
	};

	ResizableLinearAllocator module_settings_allocator;
	CapacityStream<AvailableModuleSettings> available_module_settings;

	Timer retained_timer;
	// Used to lazy update the valid status for the recording files
	Timer update_recording_valid_status_timer;
	// Used to lazy update the valid status for the replay files
	Timer update_replay_valid_status_timer;

	unsigned char sandbox_to_copy;
	unsigned char sandbox_mappings[EDITOR_MAX_SANDBOX_COUNT];

	bool is_recording_input_selected;
	bool is_replay_input_selected;
};

void InspectorDrawSandboxSettingsClean(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	DrawSandboxSettingsData* data = (DrawSandboxSettingsData*)_data;
	// Destroy the UI reflection
	editor_state->ui_reflection->DestroyInstance(data->ui_reflection_instance_name.buffer);

	// Deallocate the name
	editor_state->editor_allocator->Deallocate(data->ui_reflection_instance_name.buffer);

	// Deallocate the available runtime and module settings
	editor_state->editor_allocator->Deallocate(data->runtime_settings_allocator.GetAllocatedBuffer());
	// The available module settings entries are allocated from this allocator
	data->module_settings_allocator.Free();
	data->available_module_settings.Deallocate(editor_state->editor_allocator);
}

struct DrawSandboxSelectionWindowData {
	EditorState* editor_state;
	unsigned int selection;
	unsigned int sandbox_index;

	EDITOR_MODULE_CONFIGURATION configuration;
};

void DrawSandboxSelectionWindow(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
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
				background_color.color = EDITOR_GREEN_COLOR;
				config.AddFlag(background_color);

				configuration = ClearFlag(configuration, UI_CONFIG_LABEL_TRANSPARENT);
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
			configuration = ClearFlag(configuration, UI_CONFIG_LABEL_TRANSPARENT);
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

static void CreateAddSandboxWindow(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateAddSandboxWindowData* data = (CreateAddSandboxWindowData*)_data;
	UIWindowDescriptor descriptor;

	DrawSandboxSelectionWindowData window_data;
	window_data.editor_state = data->editor_state;
	window_data.selection = -1;
	window_data.sandbox_index = data->sandbox_index;
	// Set the default configuration as release
	window_data.configuration = EDITOR_MODULE_CONFIGURATION_RELEASE;

	descriptor.window_name = "Select a Module";

	descriptor.draw = DrawSandboxSelectionWindow;
	descriptor.window_data = &window_data;
	descriptor.window_data_size = sizeof(window_data);

	descriptor.destroy_action = ReleaseLockedWindow;

	descriptor.initial_size_x = 0.7f;
	descriptor.initial_size_y = 0.6f;

	system->CreateWindowAndDockspace(descriptor, UI_POP_UP_WINDOW_ALL);
}

static void InspectorDrawSandboxModuleSection(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	unsigned int inspector_index,
	UIDrawer* drawer, 
	DrawSandboxSettingsData* data, 
	Stream<unsigned int> graphics_module_indices
) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	UIDrawConfig config;

	drawer->CollapsingHeader("Modules", &data->collapsing_module_state, [&]() {
		// Display the count of modules in use
		ECS_STACK_CAPACITY_STREAM(char, in_use_stream, 256);
		in_use_stream.CopyOther("In use: ");
		ConvertIntToChars(in_use_stream, sandbox->modules_in_use.size);
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
			// Redraw the window as well
			action_data->redraw_window = true;
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
			action_data->redraw_window = true;
		};

		ECS_STACK_CAPACITY_STREAM(unsigned int, module_display_order, 64);
		module_display_order.size = sandbox->modules_in_use.size;
		MakeSequence(module_display_order);

		for (unsigned int index = 0; index < graphics_module_indices.size; index++) {
			for (unsigned int subindex = 0; subindex < module_display_order.size; subindex++) {
				if (module_display_order[subindex] == graphics_module_indices[index]) {
					module_display_order.Swap(index, subindex);
				}
			}
		}

		data->module_settings_allocator.Clear();
		data->available_module_settings.size = 0;
		ECS_ASSERT(module_display_order.size <= data->available_module_settings.capacity);

		AllocatorPolymorphic settings_allocator = &data->module_settings_allocator;
		for (unsigned int index = 0; index < module_display_order.size; index++) {
			ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, current_options, 256);
			GetModuleAvailableSettings(
				editor_state,
				sandbox->modules_in_use[module_display_order[index]].module_index,
				current_options,
				settings_allocator
			);
			// Add an initial label that says no settings
			data->available_module_settings[index].label = 0;
			data->available_module_settings[index].string_labels.Initialize(settings_allocator, current_options.size + 1);
			data->available_module_settings[index].string_labels[0].InitializeAndCopy(settings_allocator, "No settings");
			for (unsigned int subindex = 0; subindex < current_options.size; subindex++) {
				unsigned int write_index = subindex + 1;

				// We must add one since the conversion function adds a '\0'
				data->available_module_settings[index].string_labels[write_index].Initialize(settings_allocator, current_options[subindex].size + 1);
				ConvertWideCharsToASCII(
					current_options[subindex].buffer,
					data->available_module_settings[index].string_labels[write_index].buffer,
					current_options[subindex].size,
					current_options[subindex].size + 1
				);
				// Reduce the size by 1 since we don't want this null terminator
				data->available_module_settings[index].string_labels[write_index].size--;
			}

			ECS_STACK_CAPACITY_STREAM(char, ascii_module_settings, 512);
			// Update the label to match the string label
			Stream<wchar_t> current_module_settings = sandbox->modules_in_use[module_display_order[index]].settings_name;
			ConvertWideCharsToASCII(current_module_settings, ascii_module_settings);
			size_t existing_index = data->available_module_settings[index].string_labels.Find(ascii_module_settings);
			if (existing_index != -1) {
				data->available_module_settings[index].label = (unsigned char)existing_index;
			}
		}
		data->available_module_settings.size = module_display_order.size;

		struct ChangeInspectorToModulePageData {
			EditorState* editor_state;
			unsigned int inspector_index;
			unsigned int module_index;
		};

		auto change_inspector_to_module_page = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ChangeInspectorToModulePageData* data = (ChangeInspectorToModulePageData*)_data;
			unsigned int sandbox_index = GetInspectorTargetSandbox(data->editor_state, data->inspector_index);
			const EditorSandbox* sandbox = GetSandbox(data->editor_state, sandbox_index);
			unsigned int in_sandbox_index = GetSandboxModuleInStreamIndex(data->editor_state, sandbox_index, data->module_index);
			ChangeInspectorToModule(data->editor_state, data->module_index, data->inspector_index, sandbox->modules_in_use[in_sandbox_index].settings_name);
		};

		struct ChangeModuleSettingsData {
			DrawSandboxSettingsData* draw_data;
			unsigned int editor_module_index;
			unsigned int available_module_index;
		};

		auto change_module_settings = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			ChangeModuleSettingsData* data = (ChangeModuleSettingsData*)_data;
			auto available_settings = data->draw_data->available_module_settings[data->available_module_index];
			ECS_STACK_CAPACITY_STREAM(wchar_t, wide_setting_name, 512);
			// In case it is the first label, it means that we want to clear the settings
			if (available_settings.label != 0) {
				ConvertASCIIToWide(wide_setting_name, available_settings.string_labels[available_settings.label]);
			}
			ChangeSandboxModuleSettings(data->draw_data->editor_state, data->draw_data->sandbox_index, data->editor_module_index, wide_setting_name);
			// Save the editor sandbox file as well
			SaveEditorSandboxFile(data->draw_data->editor_state);
		};

		// Draw the modules in use, first the graphics modules	
		for (unsigned int index = 0; index < module_display_order.size; index++) {
			unsigned int in_stream_index = module_display_order[index];
			unsigned int module_index = sandbox->modules_in_use[in_stream_index].module_index;

			// Draw the remove X button
			RemoveModuleActionData remove_data;
			remove_data.editor_state = editor_state;
			remove_data.in_module_index = index;
			remove_data.sandbox_index = sandbox_index;
			drawer->SpriteButton(UI_CONFIG_MAKE_SQUARE, module_config, { remove_module_action, &remove_data, sizeof(remove_data) }, ECS_TOOLS_UI_TEXTURE_X);

			bool is_graphics_module = IsGraphicsModule(editor_state, module_index);
			const wchar_t* module_texture = is_graphics_module ? GRAPHICS_MODULE_TEXTURE : ECS_TOOLS_UI_TEXTURE_MODULE;

			const size_t CONFIGURATION = UI_CONFIG_MAKE_SQUARE;
			const wchar_t* status_texture = nullptr;
			Color status_color;

			const EditorModuleInfo* info = GetModuleInfo(
				editor_state,
				module_index,
				sandbox->modules_in_use[in_stream_index].module_configuration
			);
			switch (info->load_status) {
			case EDITOR_MODULE_LOAD_GOOD:
				status_texture = ECS_TOOLS_UI_TEXTURE_CHECKBOX_CHECK;
				status_color = EDITOR_GREEN_COLOR;
				break;
			case EDITOR_MODULE_LOAD_OUT_OF_DATE:
				status_texture = ECS_TOOLS_UI_TEXTURE_CLOCK;
				status_color = EDITOR_YELLOW_COLOR;
				break;
			case EDITOR_MODULE_LOAD_FAILED:
				status_texture = ECS_TOOLS_UI_TEXTURE_MINUS;
				status_color = EDITOR_RED_COLOR;
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

			Stream<wchar_t> module_library = editor_state->project_modules->buffer[module_index].library_name;
			drawer->TextLabelWide(label_configuration, module_config, module_library);

			float x_bound = drawer->GetCurrentPositionNonOffset().x;

			drawer->PushIdentifierStackRandom(index);

			UIConfigAlignElement align_element;
			align_element.horizontal = ECS_UI_ALIGN_RIGHT;
			module_config.AddFlag(align_element);

			drawer->ComboBox(
				UI_CONFIG_ALIGN_ELEMENT | UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_COMBO_BOX_CALLBACK,
				module_config,
				"configuration",
				{ MODULE_CONFIGURATIONS, EDITOR_MODULE_CONFIGURATION_COUNT },
				EDITOR_MODULE_CONFIGURATION_COUNT,
				(unsigned char*)&sandbox->modules_in_use[in_stream_index].module_configuration
			);
			module_config.flag_count--;

			drawer->PopIdentifierStack();
			drawer->NextRow();

			Stream<Stream<char>> available_module_settings = data->available_module_settings[index].string_labels;
			UIDrawerRowLayout settings_display_row_layout = drawer->GenerateRowLayout();
			settings_display_row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });
			settings_display_row_layout.AddComboBox(available_module_settings);

			config.flag_count = 0;
			size_t configuration = 0;
			settings_display_row_layout.GetTransform(config, configuration);

			ECS_STACK_CAPACITY_STREAM(char, module_settings_display, 512);
			UIConfigActiveState active_state;
			active_state.state = true;
			Stream<char> settings_name = "No settings";
			if (sandbox->modules_in_use[module_display_order[index]].reflected_settings.size > 0) {
				FormatString(module_settings_display, "{#} - {#}", module_library, sandbox->modules_in_use[module_display_order[index]].settings_name);
				settings_name = module_settings_display;
			}
			else {
				active_state.state = false;
			}
			config.AddFlag(active_state);

			ChangeInspectorToModulePageData change_to_module_data = { editor_state, inspector_index, module_index };
			drawer->Button(
				configuration | UI_CONFIG_LABEL_TRANSPARENT | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y | UI_CONFIG_ACTIVE_STATE,
				config,
				settings_name,
				{ change_inspector_to_module_page, &change_to_module_data, sizeof(change_to_module_data) }
			);

			config.flag_count = 0;
			configuration = 0;
			settings_display_row_layout.GetTransform(config, configuration);

			ChangeModuleSettingsData change_setting_data = { data, module_index, index };
			UIConfigComboBoxCallback combo_module_callback;
			combo_module_callback.handler = { change_module_settings, &change_setting_data, sizeof(change_setting_data) };
			config.AddFlag(combo_module_callback);

			static unsigned char label_index = 0;

			drawer->PushIdentifierStackRandom(index);
			drawer->ComboBox(
				configuration | UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_COMBO_BOX_CALLBACK,
				config,
				"module settings",
				available_module_settings,
				available_module_settings.size,
				&data->available_module_settings[index].label
			);
			drawer->NextRow();
			drawer->PopIdentifierStack();
		}

		drawer->PopIdentifierStack();

		// The add button
		CreateAddSandboxWindowData create_data;
		create_data.editor_state = editor_state;
		create_data.sandbox_index = sandbox_index;
		drawer->Button("Add Module", { CreateAddSandboxWindow, &create_data, sizeof(create_data), ECS_UI_DRAW_SYSTEM });
	});
}

static void InspectorDrawSandboxComponentsSection(EditorState* editor_state, unsigned int sandbox_index, UIDrawer* drawer, DrawSandboxSettingsData* data) {
	drawer->CollapsingHeader("Components", &data->collapsing_components_state, [&]() {
		UIDrawConfig config;

		// At the moment, add some basic functions that operate on all entities
		// Reset is the primary candidate, maybe Add/Remove for all? Questinable use tho
		struct ComponentToAllActionData {
			EditorState* editor_state;
			unsigned int sandbox_index;
			Stream<char> component_name;
		};

		auto reset_component_to_all = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			struct FunctorData {
				EditorState* editor_state;
				unsigned int sandbox_index;
				Stream<char> component_name;
			};

			ComponentToAllActionData* data = (ComponentToAllActionData*)_data;
			FunctorData functor_data = { data->editor_state, data->sandbox_index, data->component_name };
			ArchetypeQueryDescriptor query_descriptor;
			bool is_shared = data->editor_state->editor_components.IsSharedComponent(data->component_name);
			Component component_id = data->editor_state->editor_components.GetComponentID(data->component_name);
			if (is_shared) {
				query_descriptor.shared = { &component_id, 1 };
			}
			else {
				query_descriptor.unique = { &component_id, 1 };
			}
			SandboxForEachEntity(data->editor_state, data->sandbox_index, [](ForEachEntityUntypedFunctorData* for_each_data) {
				FunctorData* data = (FunctorData*)for_each_data->base.user_data;
				ResetSandboxEntityComponent(data->editor_state, data->sandbox_index, for_each_data->base.entity, data->component_name);
				}, &functor_data, query_descriptor);
			RenderSandboxViewports(data->editor_state, data->sandbox_index);
			SetSandboxSceneDirty(data->editor_state, data->sandbox_index);
		};

		struct DrawMenuFunctor {
			// Returns true if the component should appear in the menu
			bool IsUniqueEnabled(Component component) const {
				return true;
			}

			// Returns true if the component should appear in the menu
			bool IsSharedEnabled(Component component) const {
				return true;
			}

			// Fills the data for a click handler
			void Fill(void* callback_untyped_data, Stream<char> component_name) {
				ComponentToAllActionData* callback_data = (ComponentToAllActionData*)callback_untyped_data;
				*callback_data = { editor_state, sandbox_index, component_name };
			}

			// Return true if the unique entries should be made unavailable
			bool LimitUnique() const {
				return false;
			}

			// Return true if the shared entries should be made unavailable
			bool LimitShared() const {
				return false;
			}

			EditorState* editor_state;
			unsigned int sandbox_index;
		};

		DrawMenuFunctor draw_menu_functor = { editor_state, sandbox_index };

		UIConfigWindowDependentSize dependent_size;
		config.AddFlag(dependent_size);
		DrawChooseComponentMenu(
			editor_state,
			*drawer,
			UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X,
			config,
			"Reset All",
			{ reset_component_to_all, nullptr, sizeof(ComponentToAllActionData), ECS_UI_DRAW_NORMAL },
			draw_menu_functor
		);
	});
}

static void InspectorDrawSandboxRuntimeSettingsSection(EditorState* editor_state, unsigned int sandbox_index, UIDrawer* drawer, DrawSandboxSettingsData* data) {
	drawer->CollapsingHeader("Runtime Settings", &data->collapsing_runtime_state, [&]() {
		UIDrawConfig config;
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

		// Display all the available settings
		ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, available_settings, 128);
		data->runtime_settings_allocator.Clear();
		GetSandboxAvailableRuntimeSettings(editor_state, available_settings, &data->runtime_settings_allocator);

		// Reload the values if the lazy evaluation has finished
		if (data->last_write_descriptor != sandbox->runtime_settings_last_write) {
			memcpy(&data->ui_descriptor, GetSandboxWorldDescriptor(editor_state, sandbox_index), sizeof(data->ui_descriptor));
			data->last_write_descriptor = sandbox->runtime_settings_last_write;
		}

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

			UIDrawConfig local_config;
			UIConfigWindowDependentSize dependent_size;
			local_config.AddFlag(dependent_size);

			for (unsigned int index = 0; index < available_settings.size; index++) {
				SelectData select_data = { data, sandbox_index, available_settings[index] };

				size_t configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_LABEL_TRANSPARENT;
				if (available_settings[index] == sandbox->runtime_settings) {
					configuration = ClearFlag(configuration, UI_CONFIG_LABEL_TRANSPARENT);
				}

				drawer->ButtonWide(configuration, local_config, available_settings[index], { select_action, &select_data, sizeof(select_data) });
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

		UIConfigNamePadding name_padding;
		name_padding.total_length = NAME_PADDING_LENGTH;
		config.AddFlag(name_padding);

		UIReflectionDrawInstanceOptions options;
		options.drawer = drawer;
		options.config = &config;
		options.global_configuration = UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_NAME_PADDING;
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

		UIConfigActiveState save_active_state;
		save_active_state.state = !IsSandboxStateRuntime(sandbox->run_state);
		config.AddFlag(save_active_state);

		SaveCallbackData save_data;
		save_data.data = data;
		save_data.sandbox_index = sandbox_index;
		drawer->Button(UI_CONFIG_ACTIVE_STATE, config, "Save", { save_callback, &save_data, sizeof(save_data) });

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

static void InspectorDrawSandboxModifiersSection(EditorState* editor_state, unsigned int sandbox_index, UIDrawer* drawer, DrawSandboxSettingsData* data) {
	drawer->CollapsingHeader("Modifiers", &data->collapsing_modifiers_state, [&]() {
		UIDrawConfig config;
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

		const size_t CONFIGURATION = UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_NAME_PADDING;

		UIConfigNamePadding name_padding;
		name_padding.total_length = NAME_PADDING_LENGTH;
		config.AddFlag(name_padding);

		drawer->SetDrawMode(ECS_UI_DRAWER_NEXT_ROW);

		auto save_sandbox_file_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SaveEditorSandboxFile((const EditorState*)_data);
		};

		UIConfigCheckBoxCallback check_box_callback;
		check_box_callback.handler = { save_sandbox_file_action, editor_state, 0 };
		config.AddFlag(check_box_callback);

		const size_t CHECK_BOX_CONFIGURATION = CONFIGURATION | UI_CONFIG_CHECK_BOX_CALLBACK;
		drawer->CheckBox(CHECK_BOX_CONFIGURATION, config, "Should Play", &sandbox->should_play);
		drawer->CheckBox(CHECK_BOX_CONFIGURATION, config, "Should Pause", &sandbox->should_pause);
		drawer->CheckBox(CHECK_BOX_CONFIGURATION, config, "Should Step", &sandbox->should_step);

		config.flag_count--;

		UIConfigWindowDependentSize dependent_size;
		config.AddFlag(dependent_size);

		struct SpeedUpCallbackData {
			EditorState* editor_state;
			unsigned int sandbox_index;
		};

		auto speed_up_callback_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SpeedUpCallbackData* data = (SpeedUpCallbackData*)_data;
			EditorSandbox* sandbox = GetSandbox(data->editor_state, data->sandbox_index);
			sandbox->sandbox_world.speed_up_factor = sandbox->simulation_speed_up_factor;
			SaveEditorSandboxFile(data->editor_state);
		};

		SpeedUpCallbackData speed_up_data = { editor_state, sandbox_index };
		UIConfigTextInputCallback speed_up_callback;
		speed_up_callback.handler = { speed_up_callback_action, &speed_up_data, sizeof(speed_up_data) };
		config.AddFlag(speed_up_callback);
		drawer->FloatInput(
			CONFIGURATION | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_INPUT_CALLBACK
			| UI_CONFIG_NUMBER_INPUT_DEFAULT | UI_CONFIG_NUMBER_INPUT_RANGE,
			config,
			"Simulation Speed",
			&sandbox->simulation_speed_up_factor,
			1.0,
			0.001f,
			100.0f
		);

		config.flag_count = 0;
		drawer->SetDrawMode(ECS_UI_DRAWER_INDENT);
	});
}

static void InspectorDrawSandboxStatisticsSection(EditorState* editor_state, unsigned int sandbox_index, UIDrawer* drawer, DrawSandboxSettingsData* data) {
	drawer->CollapsingHeader("Statistics", &data->collapsing_statistics_state, [&]() {
		UIDrawConfig config;
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

		const size_t CONFIGURATION = UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_NAME_PADDING;

		drawer->SetDrawMode(ECS_UI_DRAWER_NEXT_ROW);

		UIConfigNamePadding name_padding;
		name_padding.total_length = NAME_PADDING_LENGTH;
		config.AddFlag(name_padding);

		struct EditorSandboxActionData {
			EditorState* editor_state;
			unsigned int sandbox_index;
		};

		auto invert_enable_statistics_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			EditorSandboxActionData* data = (EditorSandboxActionData*)_data;
			InvertSandboxStatisticsDisplay(data->editor_state, data->sandbox_index);
			action_data->redraw_window = true;
		};

		EditorSandboxActionData editor_sandbox_action_data = { editor_state, sandbox_index };
		UIConfigCheckBoxCallback check_box_callback;
		check_box_callback.handler = { invert_enable_statistics_action, &editor_sandbox_action_data, sizeof(editor_sandbox_action_data) };
		check_box_callback.disable_value_to_modify = true;
		config.AddFlag(check_box_callback);

		drawer->CheckBox(CONFIGURATION | UI_CONFIG_CHECK_BOX_CALLBACK, config, "Activated", &sandbox->statistics_display.is_enabled);
		drawer->OffsetNextRow(drawer->layout.node_indentation);
		// The row was already passed, we need to reset the X
		drawer->SetCurrentX(drawer->GetNextRowXPosition());
		config.flag_count--;

		config.flag_count = 0;
		name_padding.total_length -= drawer->layout.node_indentation;
		config.AddFlag(name_padding);

		auto change_cpu_statistic_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			EditorSandboxActionData* data = (EditorSandboxActionData*)_data;
			const EditorSandbox* sandbox = GetSandbox(data->editor_state, data->sandbox_index);
			// Here it is kinda weird to change the statistic type to the already one set, but the function
			// Does a bit more than just setting
			ChangeSandboxCPUStatisticsType(data->editor_state, data->sandbox_index, sandbox->cpu_statistics_type);
			action_data->redraw_window = true;
		};

		UIConfigComboBoxCallback cpu_callback;
		cpu_callback.handler = { change_cpu_statistic_action, &editor_sandbox_action_data, sizeof(editor_sandbox_action_data) };
		config.AddFlag(cpu_callback);

		const Reflection::ReflectionEnum* cpu_statistic_enum = editor_state->ModuleReflectionManager()->GetEnum(STRING(EDITOR_SANDBOX_CPU_STATISTICS_TYPE));
		Stream<Stream<char>> cpu_statistic_labels = cpu_statistic_enum->fields;
		drawer->ComboBox(
			CONFIGURATION | UI_CONFIG_COMBO_BOX_CALLBACK,
			config,
			"CPU Statistic",
			cpu_statistic_labels,
			cpu_statistic_labels.size,
			(unsigned char*)&sandbox->cpu_statistics_type
		);
		config.flag_count--;

		auto change_gpu_statistic_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			EditorSandboxActionData* data = (EditorSandboxActionData*)_data;
			const EditorSandbox* sandbox = GetSandbox(data->editor_state, data->sandbox_index);
			// Here it is kinda weird to change the statistic type to the already one set, but the function
			// Does a bit more than just setting
			ChangeSandboxGPUStatisticsType(data->editor_state, data->sandbox_index, sandbox->gpu_statistics_type);
			action_data->redraw_window = true;
		};

		UIConfigComboBoxCallback gpu_callback;
		gpu_callback.handler = { change_cpu_statistic_action, &editor_sandbox_action_data, sizeof(editor_sandbox_action_data) };
		config.AddFlag(gpu_callback);

		const Reflection::ReflectionEnum* gpu_statistic_enum = editor_state->ModuleReflectionManager()->GetEnum(STRING(EDITOR_SANDBOX_GPU_STATISTICS_TYPE));
		Stream<Stream<char>> gpu_statistic_labels = gpu_statistic_enum->fields;
		drawer->ComboBox(
			CONFIGURATION | UI_CONFIG_COMBO_BOX_CALLBACK,
			config,
			"GPU Statistic",
			gpu_statistic_labels,
			gpu_statistic_labels.size,
			(unsigned char*)&sandbox->gpu_statistics_type
		);
		config.flag_count--;

		auto should_display_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			EditorState* editor_state = (EditorState*)_data;
			SaveEditorSandboxFile(editor_state);
		};

		UIConfigStateTableCallback enabled_callback;
		enabled_callback.handler = { should_display_action, editor_state, 0 };
		config.AddFlag(enabled_callback);

		Stream<Stream<char>> enabled_statistic_labels = { EDITOR_SANDBOX_STATISTIC_DISPLAY_ENTRY_STRINGS, EDITOR_SANDBOX_STATISTIC_DISPLAY_COUNT };
		drawer->StateTable(
			CONFIGURATION | UI_CONFIG_STATE_TABLE_ALL | UI_CONFIG_STATE_TABLE_CALLBACK,
			config,
			"Enabled Statistics",
			enabled_statistic_labels,
			sandbox->statistics_display.should_display
		);
		config.flag_count--;

		drawer->OffsetNextRow(-drawer->layout.node_indentation);
		drawer->SetDrawMode(ECS_UI_DRAWER_INDENT);
	});
}

static void InspectorDrawSandboxRecordingSection(EditorState* editor_state, unsigned int sandbox_index, UIDrawer* drawer, DrawSandboxSettingsData* data) {
	drawer->CollapsingHeader("Recording", &data->collapsing_recording_state, [&]() {
		drawer->SetDrawMode(ECS_UI_DRAWER_NEXT_ROW);

		bool is_valid_lazy_update = data->update_recording_valid_status_timer.HasPassedAndReset(ECS_TIMER_DURATION_MS, data->is_recording_input_selected ? RECORDING_FILE_VALID_STATUS_SHORT_LAZY_MS : RECORDING_FILE_VALID_STATUS_LAZY_MS);
		data->is_recording_input_selected = false;
		
		auto draw_block = [=](const SandboxRecordingInfo& block_info, EDITOR_SANDBOX_RECORDING_TYPE recording_type) {
			UIDrawConfig config;
			EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

			size_t CONFIGURATION = UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_NAME_PADDING;

			UIConfigNamePadding name_padding;
			name_padding.total_length = NAME_PADDING_LENGTH;
			config.AddFlag(name_padding);

			ECS_FORMAT_TEMP_STRING(enabled_box_name, "{#} recording", block_info.type_string);
			drawer->CheckBox(CONFIGURATION, config, enabled_box_name, &sandbox->flags, FirstLSB64(block_info.flag));

			CONFIGURATION |= UI_CONFIG_ACTIVE_STATE;

			// Disable modifying the automatic mode or the text input after the sandbox started running
			UIConfigActiveState active_state;
			active_state.state = HasFlag(sandbox->flags, block_info.flag) && GetSandboxState(editor_state, sandbox_index) == EDITOR_SANDBOX_SCENE;
			config.AddFlag(active_state);

			ECS_FORMAT_TEMP_STRING(automatic_box_name, "{#} automatic mode", block_info.type_string);
			drawer->CheckBox(CONFIGURATION, config, automatic_box_name, block_info.is_recording_automatic);

			UIConfigWindowDependentSize dependent_size;
			config.AddFlag(dependent_size);

			if (is_valid_lazy_update) {
				UpdateSandboxValidFileBoolRecording(editor_state, sandbox_index, recording_type);
			}

			UIConfigTextParameters text_parameters = drawer->TextParameters();
			// Make the text with green when the path is valid, else make it red
			text_parameters.color = block_info.recorder->is_file_valid ? EDITOR_GREEN_COLOR : EDITOR_RED_COLOR;

			config.AddFlag(text_parameters);

			ECS_FORMAT_TEMP_STRING(file_path_name, "{#} recording file", block_info.type_string);
			Stream<wchar_t> recording_extension = block_info.extension;
			UIDrawerTextInput* text_input = drawer->TextInput(CONFIGURATION | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_PARAMETERS, config, file_path_name, &block_info.recorder->file);
			data->is_recording_input_selected |= text_input->is_currently_selected;
		};

		SandboxRecordingInfo recording_input_info = GetSandboxRecordingInfo(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_INPUT);
		draw_block(recording_input_info, EDITOR_SANDBOX_RECORDING_INPUT);
		drawer->CrossLine();

		SandboxRecordingInfo recording_state_info = GetSandboxRecordingInfo(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_STATE);
		draw_block(recording_state_info, EDITOR_SANDBOX_RECORDING_STATE);
		
		drawer->SetDrawMode(ECS_UI_DRAWER_INDENT);
	});
}

static void InspectorDrawSandboxReplaySection(EditorState* editor_state, unsigned int sandbox_index, UIDrawer* drawer, DrawSandboxSettingsData* data) {
	drawer->CollapsingHeader("Replay", &data->collapsing_replay_state, [&]() {
		drawer->SetDrawMode(ECS_UI_DRAWER_NEXT_ROW);

		bool is_valid_lazy_update = data->update_replay_valid_status_timer.HasPassedAndReset(ECS_TIMER_DURATION_MS, data->is_replay_input_selected ? REPLAY_FILE_VALID_STATUS_SHORT_LAZY_MS : REPLAY_FILE_VALID_STATUS_LAZY_MS);
		data->is_replay_input_selected = false;

		auto draw_block = [=](const SandboxReplayInfo& block_info, EDITOR_SANDBOX_RECORDING_TYPE recording_type) {
			UIDrawConfig config;
			EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

			size_t CONFIGURATION = UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_NAME_PADDING | UI_CONFIG_ACTIVE_STATE;

			UIConfigNamePadding name_padding;
			name_padding.total_length = NAME_PADDING_LENGTH;
			config.AddFlag(name_padding);

			// If the sandbox is running, prevent the options from being changed, as this can result in unpredictable behavior
			// Or situations the current implementation is not capable of dealing with.
			UIConfigActiveState active_state;
			active_state.state = GetSandboxState(editor_state, sandbox_index) == EDITOR_SANDBOX_SCENE;
			config.AddFlag(active_state);

			ECS_FORMAT_TEMP_STRING(enabled_box_name, "{#} replay", block_info.type_string);
			drawer->CheckBox(CONFIGURATION, config, enabled_box_name, &sandbox->flags, FirstLSB64(block_info.flag));

			// We need to update the active state for the next elements, to make them editable only
			// When the replay is active
			config.flag_count--;
			active_state.state &= HasFlag(sandbox->flags, block_info.flag);
			config.AddFlag(active_state);

			UIConfigWindowDependentSize dependent_size;
			config.AddFlag(dependent_size);

			if (is_valid_lazy_update) {
				UpdateSandboxValidFileBoolReplay(editor_state, sandbox_index, recording_type);
			}

			ECS_FORMAT_TEMP_STRING(enabled_elapsed_seconds_name, "{#} replay drive delta time", block_info.type_string);
			drawer->CheckBox(CONFIGURATION, config, enabled_elapsed_seconds_name, &block_info.replay->is_driving_delta_time);

			UIConfigTextParameters text_parameters = drawer->TextParameters();
			// Make the text with green when the path is valid, else make it red
			text_parameters.color = block_info.replay->is_file_valid ? EDITOR_GREEN_COLOR : EDITOR_RED_COLOR;

			config.AddFlag(text_parameters);

			ECS_FORMAT_TEMP_STRING(file_path_name, "{#} replay file", block_info.type_string);
			Stream<wchar_t> recording_extension = block_info.extension;
			UIDrawerTextInput* input = drawer->TextInput(CONFIGURATION | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_PARAMETERS, config, file_path_name, &block_info.replay->file);
			data->is_replay_input_selected |= input->is_currently_selected;
		};

		SandboxReplayInfo replay_input_info = GetSandboxReplayInfo(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_INPUT);
		draw_block(replay_input_info, EDITOR_SANDBOX_RECORDING_INPUT);
		drawer->CrossLine();

		SandboxReplayInfo replay_state_info = GetSandboxReplayInfo(editor_state, sandbox_index, EDITOR_SANDBOX_RECORDING_STATE);
		draw_block(replay_state_info, EDITOR_SANDBOX_RECORDING_STATE);

		drawer->SetDrawMode(ECS_UI_DRAWER_INDENT);
	});
}

static void InspectorDrawSandboxCopySection(EditorState* editor_state, unsigned int sandbox_index, UIDrawer* drawer, DrawSandboxSettingsData* data) {
	unsigned int sandbox_count = GetSandboxCount(editor_state, true);
	if (sandbox_count > 1) {
		drawer->NextRow();
		UIDrawConfig config;

		UIConfigComboBoxPrefix prefix;
		prefix.prefix = "Sandbox: ";
		config.AddFlag(prefix);

		ECS_STACK_CAPACITY_STREAM(char, label_buffer_storage, ECS_KB * 4);
		ECS_STACK_CAPACITY_STREAM(Stream<char>, labels, EDITOR_MAX_SANDBOX_COUNT);
		ECS_ASSERT_FORMAT(sandbox_count <= EDITOR_MAX_SANDBOX_COUNT - 1, "Insufficient space for InspectorSandboxSettings. Max supported sandboxes are {#}. "
			"Stick below that limit", EDITOR_MAX_SANDBOX_COUNT);
		const size_t CHARACTERS_PER_LABEL = 3;

		unsigned int written_count = 0;
		for (unsigned int index = 0; index < sandbox_count; index++) {
			if (data->sandbox_index != index) {
				labels[written_count].InitializeFromBuffer(label_buffer_storage.buffer + label_buffer_storage.size, CHARACTERS_PER_LABEL);
				labels[written_count].size = 0;
				ConvertIntToChars(labels[written_count], index);
				label_buffer_storage.size += CHARACTERS_PER_LABEL;
				data->sandbox_mappings[written_count++] = index;
			}
		}
		labels.size = written_count;

		drawer->ComboBox(UI_CONFIG_COMBO_BOX_PREFIX, config, "Sandbox to Copy", labels, labels.size, &data->sandbox_to_copy);

		auto copy_sandbox_action = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			DrawSandboxSettingsData* data = (DrawSandboxSettingsData*)_data;
			CopySandbox(data->editor_state, data->sandbox_index, data->sandbox_mappings[data->sandbox_to_copy]);
		};

		config.flag_count = 0;
		drawer->AddWindowDependentSizeUntilBorder(config);
		drawer->Button(UI_CONFIG_WINDOW_DEPENDENT_SIZE, config, "Copy", { copy_sandbox_action, data, 0 });
		drawer->NextRow();
	}
}

void InspectorDrawSandboxSettings(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();

	DrawSandboxSettingsData* data = (DrawSandboxSettingsData*)_data;
	unsigned int sandbox_index = GetInspectorTargetSandbox(editor_state, inspector_index);
	data->sandbox_index = sandbox_index;

	// If the targetted sandbox is outside the sandbox count, (i.e. it has been deleted)
	// Then change the inspector
	if (sandbox_index >= GetSandboxCount(editor_state, true)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	auto get_name = [](unsigned int index, CapacityStream<char>& name) {
		name.CopyOther("Sandbox ");
		ConvertIntToChars(name, index);
	};

	// Initialize the ui_reflection_instance if we haven't done so
	if (data->ui_reflection_instance_name.size == 0) {
		ECS_STACK_CAPACITY_STREAM(char, ui_reflection_name, 128);
		UIReflectionDrawer* ui_drawer = editor_state->ui_reflection;

		unsigned int current_index = 0;
		get_name(current_index, ui_reflection_name);
		// Try the UI reflection instance indices until we get a free slot
		UIReflectionInstance* instance = ui_drawer->GetInstance(ui_reflection_name.buffer);
		while (instance != nullptr) {
			// Try again with the current_index
			ui_reflection_name.size = 0;
			get_name(current_index, ui_reflection_name);

			instance = ui_drawer->GetInstance(ui_reflection_name.buffer);
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

		data->runtime_settings_allocator = LinearAllocator::InitializeFrom(editor_allocator, AVAILABLE_RUNTIME_SETTINGS_ALLOCATOR_CAPACITY);
		data->module_settings_allocator = ResizableLinearAllocator(
			AVAILABLE_MODULE_SETTINGS_ALLOCATOR_CAPACITY, 
			AVAILABLE_MODULE_SETTINGS_BACKUP_ALLOCATOR_CAPACITY, 
			editor_allocator
		);
		data->available_module_settings.Initialize(editor_allocator, 0, AVAILABLE_MODULE_SETTINGS_ENTRY_CAPACITY);
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ECS_TOOLS_UI_TEXTURE_FILE_CONFIG);

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

	config.flag_count--;
	active_state.state = !IsSandboxStateRuntime(sandbox->run_state);
	config.AddFlag(active_state);
	ChangeSandboxSceneActionData change_scene_data = { editor_state, sandbox_index };
	drawer->SpriteButton(
		UI_CONFIG_MAKE_SQUARE | UI_CONFIG_ACTIVE_STATE,
		config,
		{ ChangeSandboxSceneAction, &change_scene_data, sizeof(change_scene_data), ECS_UI_DRAW_SYSTEM },
		ECS_TOOLS_UI_TEXTURE_FOLDER
	);
	drawer->NextRow();
	config.flag_count = 0;

	InspectorDrawSandboxModuleSection(editor_state, sandbox_index, inspector_index, drawer, data, graphics_module_indices);
	InspectorDrawSandboxComponentsSection(editor_state, sandbox_index, drawer, data);
	InspectorDrawSandboxRuntimeSettingsSection(editor_state, sandbox_index, drawer, data);
	InspectorDrawSandboxModifiersSection(editor_state, sandbox_index, drawer, data);
	InspectorDrawSandboxStatisticsSection(editor_state, sandbox_index, drawer, data);
	InspectorDrawSandboxRecordingSection(editor_state, sandbox_index, drawer, data);
	InspectorDrawSandboxReplaySection(editor_state, sandbox_index, drawer, data);
	InspectorDrawSandboxCopySection(editor_state, sandbox_index, drawer, data);
}

// ----------------------------------------------------------------------------------------------------------------------------

static bool InspectorSandboxSettingsRetainedMode(void* window_data, WindowRetainedModeInfo* info) {
	DrawSandboxSettingsData* draw_data = (DrawSandboxSettingsData*)window_data;
	EDITOR_SANDBOX_STATE current_state = GetSandboxState(draw_data->editor_state, draw_data->sandbox_index);
	if (current_state != draw_data->run_state) {
		draw_data->run_state = current_state;
		return false;
	}

	if (draw_data->retained_timer.GetDuration(ECS_TIMER_DURATION_MS) >= RETAINED_MODE_REFRESH_MS) {
		draw_data->retained_timer.SetNewStart();
		return false;
	}
	return true;
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToSandboxSettings(EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index)
{
	DrawSandboxSettingsData data;
	memset(&data, 0, sizeof(data));
	data.editor_state = editor_state;
	data.retained_timer.SetUninitialized();
	data.update_recording_valid_status_timer.SetUninitialized();
	unsigned int matched_inspector_index = ChangeInspectorDrawFunction(
		editor_state,
		inspector_index,
		{ InspectorDrawSandboxSettings, InspectorDrawSandboxSettingsClean, InspectorSandboxSettingsRetainedMode },
		&data,
		sizeof(data),
		sandbox_index
	);

	if (matched_inspector_index == -1 && sandbox_index != -1) {
		// Create a new inspector instance and set it to the sandbox with the draw on the settings
		matched_inspector_index = CreateInspectorInstance(editor_state);
		CreateInspectorDockspace(editor_state, matched_inspector_index);
		SetInspectorMatchingSandbox(editor_state, matched_inspector_index, sandbox_index);
		ChangeInspectorToSandboxSettings(editor_state, matched_inspector_index);
	}

	// Set the new active window to be this inspector
	ECS_STACK_CAPACITY_STREAM(char, inspector_window_name, 128);
	GetInspectorName(matched_inspector_index, inspector_window_name);
	editor_state->ui_system->SetActiveWindow(inspector_window_name);
}