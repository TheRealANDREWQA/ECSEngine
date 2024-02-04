#include "editorpch.h"
#include "Inspector.h"
#include "InspectorData.h"
#include "FileExplorer.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorPalette.h"
#include "../Editor/EditorEvent.h"
#include "../Editor/EditorInputMapping.h"
#include "../Modules/Module.h"
#include "../Modules/ModuleSettings.h"
#include "../Assets/AssetManagement.h"
#include "../Assets/PrefabFile.h"
#include "../Sandbox/Sandbox.h"

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

#define INSPECTOR_TARGET_ALLOCATOR_CAPACITY ECS_KB * 32
#define INSPECTOR_MAX_TARGET_COUNT 12
#define INSPECTOR_TARGET_INITIALIZE_ALLOCATOR_CAPACITY ECS_KB

void InitializeInspectorTable(EditorState* editor_state);

void InitializeInspectorInstance(EditorState* editor_state, unsigned int index);

// ----------------------------------------------------------------------------------------------------------------------------

static void InspectorDestroyCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_additional_data;
	unsigned int* inspector_index = (unsigned int*)_data;

	DestroyInspectorInstance(editor_state, *inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

static void InspectorPrivateAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_additional_data;
	unsigned int* inspector_index = (unsigned int*)_data;

	if (window_index == system->GetActiveWindow()) {
		if (!editor_state->Keyboard()->IsCaptureCharacters()) {
			// Check for these actions only if we are the focused window
			if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_INSPECTOR_PREVIOUS_TARGET)) {
				PopInspectorTarget(editor_state, *inspector_index);
			}
			else if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_INSPECTOR_NEXT_TARGET)) {
				RestoreInspectorTarget(editor_state, *inspector_index);
			}
		}
	}
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

	descriptor.private_action = InspectorPrivateAction;
	descriptor.private_action_data = stack_memory;
	descriptor.private_action_data_size = sizeof(inspector_index);

	CapacityStream<char> inspector_name(OffsetPointer(stack_memory, sizeof(unsigned int)), 0, 128);
	GetInspectorName(inspector_index, inspector_name);
	descriptor.window_name = inspector_name.buffer;
}

// ----------------------------------------------------------------------------------------------------------------------------

// Just calls the clean function 
void CleanInspectorData(EditorState* editor_state, unsigned int inspector_index) {
	editor_state->inspector_manager.data[inspector_index].clean_function(editor_state, inspector_index, editor_state->inspector_manager.data[inspector_index].draw_data);
}

// ----------------------------------------------------------------------------------------------------------------------------

static void InspectorDrawSceneFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
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

static void InspectorDrawPrefabFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	Stream<wchar_t> absolute_path = (const wchar_t*)_data;
	if (!ExistsFileOrFolder(absolute_path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
	}

	// TODO: What icon should we have for prefabs?
	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, drawer->color_theme.text, drawer->color_theme.theme);
	InspectorIconNameAndPath(drawer, absolute_path);
	InspectorDrawFileTimes(drawer, absolute_path);
}

// ----------------------------------------------------------------------------------------------------------------------------

static void ChangeInspectorToSceneFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	ECS_STACK_CAPACITY_STREAM(wchar_t, null_terminated_path, 512);
	null_terminated_path.CopyOther(path);
	null_terminated_path.AddAssert(L'\0');
	ChangeInspectorDrawFunctionWithSearch(editor_state, inspector_index, { InspectorDrawSceneFile, InspectorCleanNothing }, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1), -1,
		[=](void* inspector_data) {
			const wchar_t* other_data = (const wchar_t*)inspector_data;
			return other_data == path;
	});
}

// ----------------------------------------------------------------------------------------------------------------------------

static void ChangeInspectorToPrefabFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	ECS_STACK_CAPACITY_STREAM(wchar_t, null_terminated_path, 512);
	null_terminated_path.CopyOther(path);
	null_terminated_path.AddAssert(L'\0');
	ChangeInspectorDrawFunctionWithSearch(editor_state, inspector_index, { InspectorDrawPrefabFile, InspectorCleanNothing }, null_terminated_path.buffer, sizeof(wchar_t) * (path.size + 1), -1,
		[=](void* inspector_data) {
			const wchar_t* other_data = (const wchar_t*)inspector_data;
			return other_data == path;
	});
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

		unsigned int sandbox_count = GetSandboxCount(editor_state);

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<char>, combo_labels, sandbox_count + 1);
		// We need only to convert the sandbox indices. We will use a prefix to indicate that it is the sandbox index
		ECS_STACK_CAPACITY_STREAM(char, converted_labels, 128);
		for (unsigned int index = 0; index < sandbox_count; index++) {
			combo_labels[index].buffer = converted_labels.buffer + converted_labels.size;
			combo_labels[index].size = ConvertIntToChars(converted_labels, index);
			// The last character was a '\0' but it is not included in the size
			converted_labels.size++;
		}
		// The last entry is the All one
		combo_labels.size = combo_labels.capacity - 1;
		combo_labels.Add("All");

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

			if (GetInspectorMatchingSandbox(editor_state, inspector_index) == -1) {
				// Disable the settings button
				UIConfigActiveState active_state;
				active_state.state = false;
				configuration |= UI_CONFIG_ACTIVE_STATE;
				config.AddFlag(active_state);
			}

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
				// Truncate the value
				unsigned char new_sandbox = (unsigned char)GetInspectorMatchingSandbox(data->editor_state, data->inspector_index);
				// Compare as unsigned chars since the normal index is uint and might have something set in
				// the upper bits
				if (new_sandbox == (unsigned char)GetSandboxCount(data->editor_state)) {
					SetInspectorMatchingSandboxAll(data->editor_state, data->inspector_index);
				}
				else {
					SetInspectorMatchingSandbox(data->editor_state, data->inspector_index, new_sandbox);
				}
			};

			ComboCallbackData combo_callback_data = { editor_state, inspector_index };
			UIConfigComboBoxCallback combo_callback;
			combo_callback.handler = { combo_callback_action, &combo_callback_data, sizeof(combo_callback_data) };
			config.AddFlag(combo_callback);

			UIConfigComboBoxPrefix combo_prefix;
			combo_prefix.prefix = COMBO_PREFIX;
			config.AddFlag(combo_prefix);

			bool* sandbox_unavailables = (bool*)ECS_STACK_ALLOC(sizeof(bool) * (sandbox_count + 1));
			memset(sandbox_unavailables, false, sizeof(bool) * (sandbox_count + 1));

			unsigned int temporary_sandbox_count = GetSandboxTemporaryCount(editor_state);
			memset(sandbox_unavailables + sandbox_count - temporary_sandbox_count, true, sizeof(bool) * temporary_sandbox_count);
			// We also need to add the temporary sandboxes as unavailables
			UIConfigComboBoxUnavailable unavailables;
			unavailables.unavailables = sandbox_unavailables;
			unavailables.stable = false;
			config.AddFlag(unavailables);

			// Draw the checkbox
			// For the moment, cast the pointer to unsigned char even tho it is an unsigned int
			// It behaves correctly since if the value stays lower than UCHAR_MAX
			drawer.ComboBox(
				configuration | UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_COMBO_BOX_PREFIX | UI_CONFIG_COMBO_BOX_CALLBACK | UI_CONFIG_COMBO_BOX_UNAVAILABLE,
				config,
				"Sandbox combo",
				combo_labels,
				combo_labels.size,
				(unsigned char*)&editor_state->inspector_manager.data[inspector_index].matching_sandbox
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
	unsigned int inspector_index = editor_state->inspector_manager.data.ReserveRange();
	InitializeInspectorInstance(editor_state, inspector_index);
	ChangeInspectorToNothing(editor_state, inspector_index, true);
	return inspector_index;
}

// ----------------------------------------------------------------------------------------------------------------------------

bool DoesInspectorMatchSandbox(const EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index)
{
	unsigned int matching_sandbox = GetInspectorMatchingSandbox(editor_state, inspector_index);
	return matching_sandbox == sandbox_index || matching_sandbox == -1;
}

// ----------------------------------------------------------------------------------------------------------------------------

void CreateInspectorAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	unsigned int inspector_index = CreateInspectorInstance(editor_state);
	CreateInspectorDockspace(editor_state, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToNothing(EditorState* editor_state, unsigned int inspector_index, bool do_not_push_target_entry) {
	ChangeInspectorDrawFunction(editor_state, inspector_index, { InspectorDrawNothing, InspectorCleanNothing }, nullptr, 0, -1, do_not_push_target_entry);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index)
{
	InspectorFunctions functions;
	Stream<wchar_t> extension = PathExtension(path);

	if (extension.size == 0 || !TryGetInspectorTableFunction(editor_state, functions, extension)) {
		functions.draw_function = InspectorDrawBlankFile;
		functions.clean_function = InspectorCleanNothing;
	}

	InspectorDrawFunction draw_functions[] = {
		InspectorDrawMeshFile,
		InspectorDrawTextureFile,
		InspectorDrawGPUSamplerFile,
		InspectorDrawShaderFile,
		InspectorDrawMaterialFile,
		InspectorDrawMiscFile,
		InspectorDrawSceneFile,
		InspectorDrawPrefabFile
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
		ChangeInspectorToMiscFile,
		ChangeInspectorToSceneFile,
		ChangeInspectorToPrefabFile
	};
	
	static_assert(std::size(change_functions) == std::size(draw_functions));
	
	size_t index = 0;
	size_t draw_function_count = std::size(draw_functions);
	for (; index < draw_function_count; index++) {
		if (functions.draw_function == draw_functions[index]) {
			change_functions[index](editor_state, path, inspector_index);
			break;
		}
	}

	if (index == draw_function_count) {
		if (IsInspectorTextFileDraw(functions.draw_function)) {
			// These are the only types of draws left
			ChangeInspectorToTextFile(editor_state, path, inspector_index);
		}
		else {
			ChangeInspectorToBlankFile(editor_state, path, inspector_index);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetInspectorTargetSandbox(const EditorState* editor_state, unsigned int inspector_index)
{
	return editor_state->inspector_manager.data[inspector_index].target_sandbox;
}

// ----------------------------------------------------------------------------------------------------------------------------

void GetInspectorsForSandbox(const EditorState* editor_state, unsigned int sandbox_index, ECSEngine::CapacityStream<unsigned int>* inspector_indices)
{
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (DoesInspectorMatchSandbox(editor_state, index, sandbox_index)) {
			inspector_indices->AddAssert(index);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void SetInspectorMatchingSandbox(EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index)
{
	editor_state->inspector_manager.data[inspector_index].matching_sandbox = sandbox_index;
	// Reset the draw to nothing - some windows are dependent on the sandbox and changing it without anouncing
	// them can result in inconsistent results
	ChangeInspectorToNothing(editor_state, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

void SetInspectorMatchingSandboxAll(EditorState* editor_state, unsigned int inspector_index)
{
	SetInspectorMatchingSandbox(editor_state, inspector_index, -1);
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetInspectorTargetSandboxFromUIWindow(const EditorState* editor_state, unsigned int window_index) {
	Stream<char> window_name = editor_state->ui_system->GetWindowName(window_index);
	Stream<char> prefix = INSPECTOR_WINDOW_NAME;
	if (window_name.StartsWith(prefix)) {
		unsigned int inspector_index = GetInspectorIndex(window_name);
		return GetInspectorTargetSandbox(editor_state, inspector_index);
	}
	else {
		return -1;
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorEntitySelection(EditorState* editor_state, unsigned int sandbox_index) {
	Stream<Entity> selection = GetSandboxSelectedEntities(editor_state, sandbox_index);
	if (selection.size == 0) {
		ECS_STACK_CAPACITY_STREAM(unsigned int, inspector_indices, 512);
		GetInspectorsForSandbox(editor_state, sandbox_index, &inspector_indices);
		for (unsigned int index = 0; index < inspector_indices.size; index++) {
			if (!IsInspectorLocked(editor_state, inspector_indices[index]) && IsInspectorDrawEntity(editor_state, inspector_indices[index])) {
				ChangeInspectorToNothing(editor_state, inspector_indices[index]);
			}
		}
	}
	else if (selection.size == 1) {
		ChangeInspectorToEntity(editor_state, sandbox_index, selection[0]);
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void DestroyInspectorInstance(EditorState* editor_state, unsigned int inspector_index)
{
	CleanInspectorData(editor_state, inspector_index);
	// The target allocator always needs to be deallocated
	editor_state->inspector_manager.data[inspector_index].target_allocator.Free();

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

	if (window_name.StartsWith(INSPECTOR_WINDOW_NAME)) {
		unsigned int number = 0;
		while (IsNumberCharacter(window_name[current_index])) {
			number *= 10;
			number += window_name[current_index] - '0';
			current_index--;
		}

		return number;
	}
	else {
		return -1;
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void GetInspectorName(unsigned int inspector_index, CapacityStream<char>& inspector_name)
{
	inspector_name.AddStreamSafe(INSPECTOR_WINDOW_NAME);
	ConvertIntToChars(inspector_name, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InitializeInspectorManager(EditorState* editor_state)
{
	AllocatorPolymorphic allocator = editor_state->editor_allocator;
	editor_state->inspector_manager.data.Initialize(allocator, 1);
	editor_state->inspector_manager.round_robin_index.Initialize(allocator, 2);
	InitializeInspectorTable(editor_state);
}

// ----------------------------------------------------------------------------------------------------------------------------

void LockInspector(EditorState* editor_state, unsigned int inspector_index)
{
	InspectorData* data = editor_state->inspector_manager.data.buffer + inspector_index;
	data->flags = SetFlag(data->flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------

void UnlockInspector(EditorState* editor_state, unsigned int inspector_index) 
{
	InspectorData* data = editor_state->inspector_manager.data.buffer + inspector_index;
	data->flags = ClearFlag(data->flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------

void PushInspectorTarget(
	EditorState* editor_state,
	unsigned int inspector_index,
	InspectorFunctions functions,
	void* data,
	size_t data_size,
	unsigned int sandbox_index
)
{
	InspectorData* inspector_data = &editor_state->inspector_manager.data[inspector_index];

	// If we have left-over entries, we need to deallocate them
	if (inspector_data->target_valid_count > inspector_data->targets.GetSize()) {
		inspector_data->targets.ForEachRange(inspector_data->targets.GetSize(), inspector_data->target_valid_count, [inspector_data](const InspectorData::Target& entry) {
			if (entry.data_size > 0) {
				inspector_data->target_allocator.Deallocate(entry.data);
			}
			inspector_data->target_allocator.Deallocate(entry.initialize_allocator.GetAllocatedBuffer());
		});
	}

	InspectorData::Target new_entry;
	new_entry.data = CopyNonZero(&inspector_data->target_allocator, data, data_size);
	new_entry.data_size = data_size;
	new_entry.functions = functions;
	new_entry.sandbox_index = sandbox_index;
	new_entry.initialize = nullptr;
	new_entry.initialize_data = nullptr;
	new_entry.initialize_data_size = 0;
	new_entry.initialize_allocator = LinearAllocator::InitializeFrom(
		&inspector_data->target_allocator, 
		INSPECTOR_TARGET_INITIALIZE_ALLOCATOR_CAPACITY
	);

	InspectorData::Target overwritten_entry;
	if (inspector_data->targets.PushOverwrite(&new_entry, &overwritten_entry)) {
		if (overwritten_entry.data_size > 0) {
			inspector_data->target_allocator.Deallocate(overwritten_entry.data);
		}
		inspector_data->target_allocator.Deallocate(overwritten_entry.initialize_allocator.GetAllocatedBuffer());
	}
	inspector_data->target_valid_count = inspector_data->targets.GetSize();
}

// ----------------------------------------------------------------------------------------------------------------------------

void PopInspectorTarget(EditorState* editor_state, unsigned int inspector_index)
{
	InspectorData* inspector_data = &editor_state->inspector_manager.data[inspector_index];

	if (inspector_data->targets.PopIndexOnly()) {
		InspectorData::Target entry;
		if (inspector_data->targets.Peek(entry)) {
			// Change the inspector to this entry
			ChangeInspectorDrawFunction(editor_state, inspector_index, entry.functions, entry.data, entry.data_size, entry.sandbox_index, true);
			if (entry.initialize != nullptr) {
				entry.initialize(editor_state, GetInspectorDrawFunctionData(editor_state, inspector_index), entry.initialize_data, inspector_index);
			}
		}
		else {
			ChangeInspectorToNothing(editor_state, inspector_index, true);
		}
	}
	else {
		ChangeInspectorToNothing(editor_state, inspector_index, true);
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void RestoreInspectorTarget(EditorState* editor_state, unsigned int inspector_index)
{
	InspectorData* inspector_data = &editor_state->inspector_manager.data[inspector_index];

	// Check to see if we have remaining entries
	if (inspector_data->target_valid_count > inspector_data->targets.GetSize()) {
		inspector_data->targets.PushIndexOnly();
		InspectorData::Target entry;
		inspector_data->targets.Peek(entry);

		// Change the inspector to this entry
		ChangeInspectorDrawFunction(editor_state, inspector_index, entry.functions, entry.data, entry.data_size, entry.sandbox_index, true);
		if (entry.initialize != nullptr) {
			entry.initialize(editor_state, GetInspectorDrawFunctionData(editor_state, inspector_index), entry.initialize_data, inspector_index);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void* AllocateLastInspectorTargetInitialize(
	EditorState* editor_state,
	unsigned int inspector_index,
	size_t data_size
) {
	InspectorData* inspector_data = &editor_state->inspector_manager.data[inspector_index];
	InspectorData::Target* target = inspector_data->targets.PeekIntrusive();
	target->initialize_data = target->initialize_allocator.Allocate(data_size);
	target->initialize_data_size = data_size;
	return target->initialize_data;
}

// ----------------------------------------------------------------------------------------------------------------------------

AllocatorPolymorphic GetLastInspectorTargetInitializeAllocator(EditorState* editor_state, unsigned int inspector_index)
{
	InspectorData* inspector_data = &editor_state->inspector_manager.data[inspector_index];
	InspectorData::Target* target = inspector_data->targets.PeekIntrusive();
	return &target->initialize_allocator;
}

// ----------------------------------------------------------------------------------------------------------------------------

void SetLastInspectorTargetInitializeFromAllocation(
	EditorState* editor_state,
	unsigned int inspector_index,
	InspectorData::TargetInitialize initialize,
	bool apply_on_current_data
) {
	InspectorData* inspector_data = &editor_state->inspector_manager.data[inspector_index];
	InspectorData::Target* target = inspector_data->targets.PeekIntrusive();
	target->initialize = initialize;
	if (apply_on_current_data) {
		initialize(editor_state, GetInspectorDrawFunctionData(editor_state, inspector_index), target->initialize_data, inspector_index);
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void* SetLastInspectorTargetInitialize(
	EditorState* editor_state, 
	unsigned int inspector_index, 
	InspectorData::TargetInitialize initialize, 
	void* initialize_data, 
	size_t initialize_data_size,
	bool apply_on_current_data
)
{
	InspectorData* inspector_data = &editor_state->inspector_manager.data[inspector_index];
	InspectorData::Target* target = inspector_data->targets.PeekIntrusive();
	target->initialize = initialize;
	target->initialize_data = CopyNonZero(GetLastInspectorTargetInitializeAllocator(editor_state, inspector_index), initialize_data, initialize_data_size);
	target->initialize_data_size = initialize_data_size;

	if (apply_on_current_data) {
		initialize(editor_state, GetInspectorDrawFunctionData(editor_state, inspector_index), initialize_data, inspector_index);
	}
	return target->initialize_data;
}

// ----------------------------------------------------------------------------------------------------------------------------

void UpdateLastInspectorTargetData(EditorState* editor_state, unsigned int inspector_index, void* new_data)
{
	InspectorData* inspector_data = &editor_state->inspector_manager.data[inspector_index];
	InspectorData::Target* target = inspector_data->targets.PeekIntrusive();
	if (target->data_size > 0) {
		memcpy(target->data, new_data, target->data_size);
	}
	else {
		target->data = new_data;
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

struct ReloadInspectorAssetEventData {
	Stream<wchar_t> path;
	Stream<char> initial_setting;
	unsigned int inspector_index;
	ECS_ASSET_TYPE asset_type;
};

EDITOR_EVENT(ReloadInspectorAssetEvent) {
	ReloadInspectorAssetEventData* data = (ReloadInspectorAssetEventData*)_data;
	ChangeInspectorToNothing(editor_state, data->inspector_index);
	ChangeInspectorToAsset(editor_state, data->path, data->initial_setting, data->asset_type, data->inspector_index);
	data->path.Deallocate(editor_state->EditorAllocator());
	data->initial_setting.Deallocate(editor_state->EditorAllocator());
	return false;
}

void ReloadInspectorAssetFromMetadata(EditorState* editor_state, unsigned int inspector_index)
{
	InspectorDrawFunction draw_function = GetInspectorDrawFunction(editor_state, inspector_index);
	const void* inspector_data = GetInspectorDrawFunctionData(editor_state, inspector_index);
	InspectorAssetTarget asset_target;
	ECS_ASSET_TYPE asset_type = ECS_ASSET_TYPE_COUNT;

	if (draw_function == InspectorDrawMeshFile) {
		asset_target = InspectorDrawMeshTarget(inspector_data);
		asset_type = ECS_ASSET_MESH;
	}
	else if (draw_function == InspectorDrawTextureFile) {
		asset_target = InspectorDrawTextureTarget(inspector_data);
		asset_type = ECS_ASSET_TEXTURE;
	}
	else if (draw_function == InspectorDrawGPUSamplerFile) {
		asset_target = InspectorDrawGPUSamplerTarget(inspector_data);
		asset_type = ECS_ASSET_GPU_SAMPLER;
	}
	else if (draw_function == InspectorDrawShaderFile) {
		asset_target = InspectorDrawShaderTarget(inspector_data);
		asset_type = ECS_ASSET_SHADER;
	}
	else if (draw_function == InspectorDrawMaterialFile) {
		asset_target = InspectorDrawMaterialTarget(inspector_data);
		asset_type = ECS_ASSET_MATERIAL;
	}
	else if (draw_function == InspectorDrawMiscFile) {
		asset_target = InspectorDrawMiscTarget(inspector_data);
		asset_type = ECS_ASSET_MISC;
	}
	else {
		ECS_ASSERT(false, "Invalid inspector asset target type");
	}

	ReloadInspectorAssetEventData event_data;
	event_data.inspector_index = inspector_index;
	event_data.path = asset_target.path.Copy(editor_state->EditorAllocator());
	event_data.initial_setting = asset_target.initial_asset.Copy(editor_state->EditorAllocator());
	event_data.asset_type = asset_type;
	EditorAddEvent(editor_state, ReloadInspectorAssetEvent, &event_data, sizeof(event_data));
}

// ----------------------------------------------------------------------------------------------------------------------------

static bool InspectorRetainedModeWrapper(void* window_data, WindowRetainedModeInfo* info) {
	EditorState* editor_state = (EditorState*)window_data;

	Stream<char> window_name = info->system->GetWindowName(info->window_index);
	unsigned int inspector_index = GetInspectorIndex(window_name);

	void* draw_data = GetInspectorDrawFunctionData(editor_state, inspector_index);
	return editor_state->inspector_manager.data[inspector_index].retained_mode(draw_data, info);
}

void SetInspectorRetainedMode(EditorState* editor_state, unsigned int inspector_index, WindowRetainedMode retained_mode)
{
	ECS_STACK_CAPACITY_STREAM(char, inspector_name, 512);
	GetInspectorName(inspector_index, inspector_name);
	editor_state->inspector_manager.data[inspector_index].retained_mode = retained_mode;
	WindowRetainedMode inspector_retained_mode = retained_mode != nullptr ? InspectorRetainedModeWrapper : nullptr;
	editor_state->ui_system->SetWindowRetainedFunction(inspector_name, inspector_retained_mode);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InitializeInspectorTable(EditorState* editor_state) {
	void* allocation = editor_state->editor_allocator->Allocate(InspectorTable::MemoryOf(FUNCTION_TABLE_CAPACITY));
	editor_state->inspector_manager.function_table.InitializeFromBuffer(allocation, FUNCTION_TABLE_CAPACITY);
	
	InspectorTable* inspector_table = &editor_state->inspector_manager.function_table;
	AddInspectorTableFunction(inspector_table, { InspectorDrawSceneFile, InspectorCleanNothing }, EDITOR_SCENE_EXTENSION);
	AddInspectorTableFunction(inspector_table, { InspectorDrawPrefabFile, InspectorCleanNothing }, PREFAB_FILE_EXTENSION);
	InspectorTextFileAddFunctors(inspector_table);
	InspectorTextureFileAddFunctors(inspector_table);
	InspectorMeshFileAddFunctors(inspector_table);
	InspectorMiscFileAddFunctors(inspector_table);
	InspectorShaderFileAddFunctors(inspector_table);
	InspectorMaterialFileAddFunctors(inspector_table);
	InspectorGPUSamplerFileAddFunctors(inspector_table);
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
	data->matching_sandbox = 0;
	data->target_sandbox = 0;
	data->table = &editor_state->inspector_manager.function_table;
	data->target_allocator = MemoryManager(INSPECTOR_TARGET_ALLOCATOR_CAPACITY, ECS_KB, INSPECTOR_TARGET_ALLOCATOR_CAPACITY, editor_state->EditorAllocator());
	data->targets.Initialize(&data->target_allocator, INSPECTOR_MAX_TARGET_COUNT);
	data->target_valid_count = 0;
}

// ----------------------------------------------------------------------------------------------------------------------------

void FixInspectorSandboxReference(EditorState* editor_state, unsigned int old_sandbox_index, unsigned int new_sandbox_index) {
	for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
		if (editor_state->inspector_manager.data[index].matching_sandbox == old_sandbox_index) {
			editor_state->inspector_manager.data[index].matching_sandbox = new_sandbox_index;
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void RegisterInspectorSandboxChange(EditorState* editor_state) {
	unsigned int sandbox_count = GetSandboxCount(editor_state);
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
				target_inspectors_for_module += GetInspectorMatchingSandbox(editor_state, subindex) == index;
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

		// Any inspectors that are matching invalid sandboxes, retarget them to the 0th sandbox
		for (unsigned int index = 0; index < editor_state->inspector_manager.data.size; index++) {
			unsigned int matching_sandbox = GetInspectorMatchingSandbox(editor_state, index);
			if (matching_sandbox >= sandbox_count && matching_sandbox != -1) {
				ChangeInspectorMatchingSandbox(editor_state, index, 0);
				ChangeInspectorToNothing(editor_state, index);
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

bool IsInspectorLocked(const EditorState* editor_state, unsigned int inspector_index)
{
	return HasFlag(editor_state->inspector_manager.data[inspector_index].flags, INSPECTOR_FLAG_LOCKED);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorMatchingSandbox(EditorState* editor_state, unsigned int inspector_index, unsigned int sandbox_index)
{
	editor_state->inspector_manager.data[inspector_index].matching_sandbox = sandbox_index;
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

	ChangeInspectorToAsset(editor_state, metadata_file, GetAssetName(metadata, asset_type), asset_type, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE asset_type, unsigned int inspector_index)
{
	ChangeInspectorToAsset(editor_state, editor_state->asset_database->GetAssetConst(handle, asset_type), asset_type, inspector_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToAsset(EditorState* editor_state, Stream<wchar_t> path, Stream<char> initial_setting, ECS_ASSET_TYPE asset_type, unsigned int inspector_index)
{
	switch (asset_type) {
	case ECS_ASSET_MESH:
		ChangeInspectorToMeshFile(editor_state, path, inspector_index, initial_setting);
		break;
	case ECS_ASSET_TEXTURE:
		ChangeInspectorToTextureFile(editor_state, path, inspector_index, initial_setting);
		break;
	case ECS_ASSET_GPU_SAMPLER:
		ChangeInspectorToGPUSamplerFile(editor_state, path, inspector_index);
		break;
	case ECS_ASSET_SHADER:
		ChangeInspectorToShaderFile(editor_state, path, inspector_index);
		break;
	case ECS_ASSET_MATERIAL:
		ChangeInspectorToMaterialFile(editor_state, path, inspector_index);
		break;
	case ECS_ASSET_MISC:
		ChangeInspectorToMiscFile(editor_state, path, inspector_index);
		break;
	default:
		ECS_ASSERT(false, "Invalid asset type");
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

bool ExistsInspector(const EditorState* editor_state, unsigned int inspector_index)
{
	return editor_state->inspector_manager.data.size > inspector_index;
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int GetInspectorMatchingSandbox(const EditorState* editor_state, unsigned int inspector_index)
{
	return editor_state->inspector_manager.data[inspector_index].matching_sandbox;
}

// ----------------------------------------------------------------------------------------------------------------------------
