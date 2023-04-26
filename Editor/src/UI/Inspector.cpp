#include "editorpch.h"
#include "Inspector.h"
#include "InspectorData.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorPalette.h"
#include "../HelperWindows.h"
#include "../Editor/EditorEvent.h"
#include "FileExplorer.h"
#include "../Modules/Module.h"
#include "ECSEngineRendering.h"
#include "../Modules/ModuleSettings.h"
#include "../Assets/AssetManagement.h"

#include "Inspector/InspectorUtilities.h"
#include "Inspector/InspectorMeshFile.h"
#include "Inspector/InspectorTextFile.h"
#include "Inspector/InspectorTextureFile.h"
#include "Inspector/InspectorGPUSamplerFile.h"
#include "Inspector/InspectorMaterialFile.h"
#include "Inspector/InspectorMiscFile.h"
#include "Inspector/InspectorShaderFile.h"

constexpr float2 WINDOW_SIZE = float2(0.5f, 1.2f);
constexpr size_t FUNCTION_TABLE_CAPACITY = 32;
constexpr size_t INSPECTOR_FLAG_LOCKED = 1 << 0;

void InitializeInspectorTable(EditorState* editor_state);

void InitializeInspectorInstance(EditorState* editor_state, unsigned int index);

void InspectorDestroyCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_additional_data;
	unsigned int* inspector_index = (unsigned int*)_data;

	DestroyInspectorInstance(editor_state, *inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {	
	unsigned int inspector_index = *(unsigned int*)stack_memory;

	descriptor.draw = InspectorWindowDraw;
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;

	descriptor.destroy_action = InspectorDestroyCallback;
	descriptor.destroy_action_data = stack_memory;
	descriptor.destroy_action_data_size = sizeof(inspector_index);

	CapacityStream<char> inspector_name(function::OffsetPointer(stack_memory, sizeof(unsigned int)), 0, 128);
	GetInspectorName(inspector_index, inspector_name);
	descriptor.window_name = inspector_name.buffer;
}

// ----------------------------------------------------------------------------------------------------------------------------

void GetInspectorsForModule(const EditorState* editor_state, unsigned int target_sandbox, CapacityStream<unsigned int>& inspector_indices) {
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (editor_state->inspector_manager.data[index].target_sandbox == target_sandbox) {
			inspector_indices.AddSafe(index);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

// Just calls the clean function 
void CleanInspectorData(EditorState* editor_state, unsigned int inspector_index) {
	editor_state->inspector_manager.data[inspector_index].clean_function(editor_state, inspector_index, editor_state->inspector_manager.data[inspector_index].draw_data);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawSceneFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	const wchar_t* _absolute_path = (const wchar_t*)_data;
	Stream<wchar_t> absolute_path = _absolute_path;
	if (!ExistsFileOrFolder(absolute_path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
	}

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ECS_TOOLS_UI_TEXTURE_FILE_SCENE, drawer->color_theme.text, drawer->color_theme.theme);
	InspectorIconNameAndPath(drawer, absolute_path);
	InspectorDrawFileTimes(drawer, absolute_path);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	const float PADLOCK_SIZE = 0.04f;
	const float REDUCE_FONT_SIZE = 1.0f;

	UI_PREPARE_DRAWER(initialize);
	//drawer.DisablePaddingForRenderRegion();
	//drawer.DisablePaddingForRenderSliders();

	EditorState* editor_state = (EditorState*)window_data;

	// Determine the index from the name
	Stream<char> window_name = drawer.system->GetWindowName(drawer.window_index);
	unsigned int inspector_index = GetInspectorIndex(window_name);

	InspectorData* data = editor_state->inspector_manager.data.buffer + inspector_index;

	if (initialize) {
		drawer.font.size *= REDUCE_FONT_SIZE;
		drawer.font.character_spacing *= REDUCE_FONT_SIZE;
	}
	else {
		// The padlock
		float zoomed_icon_size = ICON_SIZE * drawer.zoom_ptr->y;
		float padlock_size = PADLOCK_SIZE * drawer.zoom_ptr->y;

		const char* COMBO_PREFIX = "Sandbox: ";

		unsigned int sandbox_count = editor_state->sandboxes.size;

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<char>, combo_labels, sandbox_count);
		// We need only to convert the sandbox indices. We will use a prefix to indicate that it is the sandbox index
		ECS_STACK_CAPACITY_STREAM(char, converted_labels, 128);
		for (unsigned int index = 0; index < sandbox_count; index++) {
			combo_labels[index].buffer = converted_labels.buffer + converted_labels.size;
			combo_labels[index].size = function::ConvertIntToChars(converted_labels, index);
			// The last character was a '\0' but it is not included in the size
			converted_labels.size++;
		}
		combo_labels.size = sandbox_count;

		UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
		row_layout.SetHorizontalAlignment(ECS_UI_ALIGN_MIDDLE);
		row_layout.AddSquareLabel();
		if (sandbox_count > 0) {
			row_layout.AddSquareLabel();
			row_layout.AddComboBox(combo_labels, { nullptr, 0 }, COMBO_PREFIX);
		}

		size_t configuration = 0;
		UIDrawConfig config;
		row_layout.GetTransform(config, configuration);

		drawer.SpriteTextureBool(
			configuration,
			config, 
			ECS_TOOLS_UI_TEXTURE_UNLOCKED_PADLOCK, 
			ECS_TOOLS_UI_TEXTURE_LOCKED_PADLOCK,
			&data->flags, 
			INSPECTOR_FLAG_LOCKED, 
			EDITOR_GREEN_COLOR
		);

		if (sandbox_count > 0) {
			config.flag_count = 0;
			configuration = 0;
			row_layout.GetTransform(config, configuration);

			struct OpenSandboxSettings {
				EditorState* editor_state;
				unsigned int inspector_index;
			};

			auto open_sandbox_settings = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				OpenSandboxSettings* data = (OpenSandboxSettings*)_data;
				ChangeInspectorToSandboxSettings(data->editor_state, data->inspector_index);
			};

			OpenSandboxSettings open_data;
			open_data = { editor_state, inspector_index };
			drawer.SpriteButton(
				configuration,
				config,
				{ open_sandbox_settings, &open_data, sizeof(open_data) },
				ECS_TOOLS_UI_TEXTURE_COG
			);

			// To the rightmost border draw the bound sandbox index
			config.flag_count = 0;
			configuration = 0;
			row_layout.GetTransform(config, configuration);

			// We need to notify with a callback that the target sandbox has changed such that the inspector is reset
			struct ComboCallbackData {
				EditorState* editor_state;
				unsigned int inspector_index;
			};

			auto combo_callback_action = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				ComboCallbackData* data = (ComboCallbackData*)_data;
				unsigned char new_sandbox = GetInspectorTargetSandbox(data->editor_state, data->inspector_index);
				SetInspectorTargetSandbox(data->editor_state, data->inspector_index, new_sandbox);
			};

			ComboCallbackData combo_callback_data = { editor_state, inspector_index };
			UIConfigComboBoxCallback combo_callback;
			combo_callback.handler = { combo_callback_action, &combo_callback_data, sizeof(combo_callback_data) };
			config.AddFlag(combo_callback);

			UIConfigComboBoxPrefix combo_prefix;
			combo_prefix.prefix = COMBO_PREFIX;
			config.AddFlag(combo_prefix);

			// Draw the checkbox
			// For the moment, cast the pointer to unsigned char even tho it is an unsigned int
			// It behaves correctly since if the value stays lower than UCHAR_MAX
			drawer.ComboBox(
				configuration | UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_COMBO_BOX_PREFIX | UI_CONFIG_COMBO_BOX_CALLBACK,
				config,
				"Sandbox combo",
				combo_labels,
				sandbox_count,
				(unsigned char*)&editor_state->inspector_manager.data[inspector_index].target_sandbox
			);
		}
		drawer.NextRow();

		if (data->draw_function == nullptr) {
			ChangeInspectorToNothing(editor_state, inspector_index);
		}

		data->draw_function(editor_state, inspector_index, data->draw_data, &drawer);
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateInspectorWindow(EditorState* editor_state, unsigned int inspector_index) {
	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);
	descriptor.initial_size_x = WINDOW_SIZE.x;
	descriptor.initial_size_y = WINDOW_SIZE.y;

	size_t stack_memory[128];
	unsigned int* stack_inspector_index = (unsigned int*)stack_memory;
	*stack_inspector_index = inspector_index;
	InspectorSetDescriptor(descriptor, editor_state, stack_memory);

	return editor_state->ui_system->Create_Window(descriptor);
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateInspectorDockspace(EditorState* editor_state, unsigned int inspector_index) {
	ECS_ASSERT(inspector_index < MAX_INSPECTOR_WINDOWS);

	unsigned int window_index = CreateInspectorWindow(editor_state, inspector_index);

	float2 window_position = editor_state->ui_system->GetWindowPosition(window_index);
	float2 window_scale = editor_state->ui_system->GetWindowScale(window_index);
	editor_state->ui_system->CreateDockspace({ window_position, window_scale }, DockspaceType::FloatingHorizontal, window_index, false);
	return window_index;
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateInspectorInstance(EditorState* editor_state) {
	unsigned int inspector_index = editor_state->inspector_manager.data.ReserveNewElement();
	InitializeInspectorInstance(editor_state, inspector_index);
	editor_state->inspector_manager.data.size++;
	ChangeInspectorToNothing(editor_state, inspector_index);
	return inspector_index;
}

// ----------------------------------------------------------------------------------------------------------------------------

void CreateInspectorAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	unsigned int inspector_index = CreateInspectorInstance(editor_state);
	CreateInspectorDockspace(editor_state, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToNothing(EditorState* editor_state, unsigned int inspector_index) {
	ChangeInspectorDrawFunction(editor_state, inspector_index, { InspectorDrawNothing, InspectorCleanNothing }, nullptr, 0);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index)
{
	InspectorFunctions functions;
	Stream<wchar_t> extension = function::PathExtension(path);

	ECS_TEMP_STRING(null_terminated_path, 256);
	null_terminated_path.Copy(path);
	null_terminated_path[null_terminated_path.size] = L'\0';
	if (extension.size == 0 || !TryGetInspectorTableFunction(editor_state, functions, extension)) {
		functions.draw_function = InspectorDrawBlankFile;
		functions.clean_function = InspectorCleanNothing;
	}

	InspectorDrawFunction asset_draws[] = {
		InspectorDrawMeshFile,
		InspectorDrawTextureFile,
		InspectorDrawGPUSamplerFile,
		InspectorDrawShaderFile,
		InspectorDrawMaterialFile,
		InspectorDrawMiscFile
	};

	auto mesh_wrapper = [](EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
		ChangeInspectorToMeshFile(editor_state, path, inspector_index);
	};

	auto texture_wrapper = [](EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
		ChangeInspectorToTextureFile(editor_state, path, inspector_index);
	};

	void (*change_functions[])(EditorState*, Stream<wchar_t>, unsigned int) = {
		mesh_wrapper,
		texture_wrapper,
		ChangeInspectorToGPUSamplerFile,
		ChangeInspectorToShaderFile,
		ChangeInspectorToMaterialFile,
		ChangeInspectorToMiscFile
	};
	
	size_t index = 0;
	size_t asset_draw_count = std::size(asset_draws);
	for (; index < asset_draw_count; index++) {
		if (functions.draw_function == asset_draws[index]) {
			change_functions[index](editor_state, path, inspector_index);
			break;
		}
	}
	
	// If it didn't match an asset
	if (index == asset_draw_count) {
		ChangeInspectorDrawFunction(editor_state, inspector_index, functions, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1));
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetInspectorTargetSandbox(const EditorState* editor_state, unsigned int inspector_index)
{
	return editor_state->inspector_manager.data[inspector_index].target_sandbox;
}

// ----------------------------------------------------------------------------------------------------------------------------

void SetInspectorTargetSandbox(EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index)
{
	editor_state->inspector_manager.data[inspector_index].target_sandbox = sandbox_index;
	// Reset the draw to nothing - some windows are dependent on the sandbox and changing it without anouncing
	// them can result in inconsistent results

	ChangeInspectorToNothing(editor_state, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

void DestroyInspectorInstance(EditorState* editor_state, unsigned int inspector_index)
{
	CleanInspectorData(editor_state, inspector_index);
	// Deallocate the data if any is allocated
	if (editor_state->inspector_manager.data[inspector_index].data_size > 0) {
		editor_state->editor_allocator->Deallocate(editor_state->inspector_manager.data[inspector_index].draw_data);
		editor_state->inspector_manager.data[inspector_index].data_size = 0;
	}

	// If the inspector is not the last one and the size is greater than 1, swap it with the last one
	if (inspector_index != editor_state->inspector_manager.data.size - 1 && editor_state->inspector_manager.data.size > 0) {
		editor_state->inspector_manager.data.RemoveSwapBack(inspector_index);
		// Update the window name for the swapped inspector
		unsigned int swapped_inspector = editor_state->inspector_manager.data.size;
		editor_state->ui_system->ChangeWindowNameFromIndex(INSPECTOR_WINDOW_NAME, swapped_inspector, inspector_index);
	}
	else if (editor_state->inspector_manager.data.size > 0) {
		editor_state->inspector_manager.data.size--;
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetInspectorIndex(Stream<char> window_name) {
	size_t current_index = window_name.size - 1;

	unsigned int number = 0;
	while (function::IsNumberCharacter(window_name[current_index])) {
		number *= 10;
		number += window_name[current_index] - '0';
		current_index--;
	}

	return number;
}

// ----------------------------------------------------------------------------------------------------------------------------

void GetInspectorName(unsigned int inspector_index, CapacityStream<char>& inspector_name)
{
	inspector_name.AddStreamSafe(INSPECTOR_WINDOW_NAME);
	function::ConvertIntToChars(inspector_name, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InitializeInspectorManager(EditorState* editor_state)
{
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(editor_state->editor_allocator);
	editor_state->inspector_manager.data.Initialize(allocator, 1);
	editor_state->inspector_manager.round_robin_index.Initialize(allocator, 2);
	InitializeInspectorTable(editor_state);
}

// ----------------------------------------------------------------------------------------------------------------------------

void LockInspector(EditorState* editor_state, unsigned int inspector_index)
{
	InspectorData* data = editor_state->inspector_manager.data.buffer + inspector_index;
	data->flags = function::SetFlag(data->flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------

void UnlockInspector(EditorState* editor_state, unsigned int inspector_index) 
{
	InspectorData* data = editor_state->inspector_manager.data.buffer + inspector_index;
	data->flags = function::ClearFlag(data->flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InitializeInspectorTable(EditorState* editor_state) {
	void* allocation = editor_state->editor_allocator->Allocate(InspectorTable::MemoryOf(FUNCTION_TABLE_CAPACITY));
	editor_state->inspector_manager.function_table.InitializeFromBuffer(allocation, FUNCTION_TABLE_CAPACITY);
	
	
	AddInspectorTableFunction(&editor_state->inspector_manager.function_table, { InspectorDrawSceneFile, InspectorCleanNothing }, L".scene");
	InspectorTextFileAddFunctors(&editor_state->inspector_manager.function_table);
	InspectorTextureFileAddFunctors(&editor_state->inspector_manager.function_table);
	InspectorMeshFileAddFunctors(&editor_state->inspector_manager.function_table);
	InspectorMiscFileAddFunctors(&editor_state->inspector_manager.function_table);
	InspectorShaderFileAddFunctors(&editor_state->inspector_manager.function_table);
	InspectorMaterialFileAddFunctors(&editor_state->inspector_manager.function_table);
	InspectorGPUSamplerFileAddFunctors(&editor_state->inspector_manager.function_table);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InitializeInspectorInstance(EditorState* editor_state, unsigned int index)
{
	InspectorData* data = editor_state->inspector_manager.data.buffer + index;
	data->draw_function = nullptr;
	data->clean_function = InspectorCleanNothing;
	data->data_size = 0;
	data->draw_data = nullptr;
	data->flags = 0;
	data->target_sandbox = 0;
	data->table = &editor_state->inspector_manager.function_table;
}

// ----------------------------------------------------------------------------------------------------------------------------

void FixInspectorSandboxReference(EditorState* editor_state, unsigned int old_sandbox_index, unsigned int new_sandbox_index) {
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (editor_state->inspector_manager.data[index].target_sandbox == old_sandbox_index) {
			editor_state->inspector_manager.data[index].target_sandbox = new_sandbox_index;
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void RegisterInspectorSandboxChange(EditorState* editor_state) {
	unsigned int sandbox_count = editor_state->sandboxes.size;
	if (sandbox_count > editor_state->inspector_manager.round_robin_index.size) {
		// An addition was done - just copy to a new buffer
		Stream<unsigned int> old_stream = editor_state->inspector_manager.round_robin_index;
		editor_state->inspector_manager.round_robin_index.Initialize(editor_state->editor_allocator, sandbox_count + 1);
		old_stream.CopyTo(editor_state->inspector_manager.round_robin_index.buffer);

		// Move the count, for actions independent of sandbox, positioned at old_stream.size to sandbox_count + 1
		editor_state->inspector_manager.round_robin_index[sandbox_count] = old_stream[old_stream.size];
		editor_state->editor_allocator->Deallocate(old_stream.buffer);
	}
	else if (sandbox_count < editor_state->inspector_manager.round_robin_index.size) {
		// A removal was done - allocate a new buffer and reroute inspectors on the sandboxes removed
		Stream<unsigned int> old_stream = editor_state->inspector_manager.round_robin_index;
		editor_state->inspector_manager.round_robin_index.Initialize(editor_state->editor_allocator, sandbox_count + 1);
		// The count for actions independent of sandbox must be moved separately
		memcpy(editor_state->inspector_manager.round_robin_index.buffer, old_stream.buffer, sizeof(unsigned int) * sandbox_count);

		editor_state->inspector_manager.round_robin_index[sandbox_count] = old_stream[old_stream.size];
		editor_state->editor_allocator->Deallocate(old_stream.buffer);
		
		// Fix any invalid round robin index
		for (unsigned int index = 0; index < sandbox_count; index++) {
			unsigned int target_inspectors_for_module = 0;
			for (unsigned int subindex = 0; subindex < editor_state->inspector_manager.data.size; subindex++) {
				target_inspectors_for_module += editor_state->inspector_manager.data[subindex].target_sandbox == index;
			}

			if (target_inspectors_for_module > 0) {
				editor_state->inspector_manager.round_robin_index[index] = editor_state->inspector_manager.round_robin_index[index] % target_inspectors_for_module;
			}
			else {
				editor_state->inspector_manager.round_robin_index[index] = 0;
			}
		}

		if (sandbox_count > 0) {
			editor_state->inspector_manager.round_robin_index[sandbox_count] = editor_state->inspector_manager.round_robin_index[sandbox_count] % sandbox_count;
		}
		else {
			editor_state->inspector_manager.round_robin_index[sandbox_count] = 0;
		}

		// Any inspectors that are targetting invalid sandboxes, retarget them to the 0th sandbox
		for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
			if (GetInspectorTargetSandbox(editor_state, index) >= sandbox_count) {
				ChangeInspectorTargetSandbox(editor_state, index, 0);
				ChangeInspectorToNothing(editor_state, index);
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

bool IsInspectorLocked(const EditorState* editor_state, unsigned int inspector_index)
{
	return function::HasFlag(editor_state->inspector_manager.data[inspector_index].flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorTargetSandbox(EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index)
{
	editor_state->inspector_manager.data[inspector_index].target_sandbox = sandbox_index;
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToAsset(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE asset_type, unsigned int inspector_index) {
	ECS_STACK_CAPACITY_STREAM(wchar_t, metadata_file, 512);
	bool success = GetAssetFileFromAssetMetadata(editor_state, metadata, asset_type, metadata_file);
	
	// Early exit if it failed
	if (!success) {
		return;
	}

	switch (asset_type) {
	case ECS_ASSET_MESH:
		ChangeInspectorToMeshFile(editor_state, metadata_file, inspector_index, GetAssetName(metadata, asset_type));
		break;
	case ECS_ASSET_TEXTURE:
		ChangeInspectorToTextureFile(editor_state, metadata_file, inspector_index, GetAssetName(metadata, asset_type));
		break;
	case ECS_ASSET_GPU_SAMPLER:
		ChangeInspectorToGPUSamplerFile(editor_state, metadata_file, inspector_index);
		break;
	case ECS_ASSET_SHADER:
		ChangeInspectorToShaderFile(editor_state, metadata_file, inspector_index);
		break;
	case ECS_ASSET_MATERIAL:
		ChangeInspectorToMaterialFile(editor_state, metadata_file, inspector_index);
		break;
	case ECS_ASSET_MISC:
		ChangeInspectorToMiscFile(editor_state, metadata_file, inspector_index);
		break;
	default:
		ECS_ASSERT(false, "Invalid asset type");
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE asset_type, unsigned int inspector_index)
{
	ChangeInspectorToAsset(editor_state, editor_state->asset_database->GetAssetConst(handle, asset_type), asset_type, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------
