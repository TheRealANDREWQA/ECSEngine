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

void InspectorDrawNothing(EditorState* editor_state, unsigned int inspector_index, void* data, UIDrawer* drawer) {
	UIDrawConfig config;
	drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, "Nothing is selected.");
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

void InspectorWindowDraw(void* window_data, void* drawer_descriptor, bool initialize) {
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

		UIDrawConfig config;
		UIConfigRelativeTransform transform;
		transform.scale.y = drawer.GetRelativeTransformFactorsZoomed({ 0.0f, PADLOCK_SIZE }).y;
		transform.offset.y = (zoomed_icon_size - padlock_size) / 2;
		config.AddFlag(transform);

		drawer.SpriteTextureBool(
			UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_MAKE_SQUARE,
			config, 
			ECS_TOOLS_UI_TEXTURE_UNLOCKED_PADLOCK, 
			ECS_TOOLS_UI_TEXTURE_LOCKED_PADLOCK,
			&data->flags, 
			INSPECTOR_FLAG_LOCKED, 
			EDITOR_GREEN_COLOR
		);
		drawer.current_row_y_scale = zoomed_icon_size;

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

		// The final positions will be set inside the if
		UIConfigAbsoluteTransform combo_transform;
		combo_transform.position.y = drawer.GetCurrentPositionNonOffset().y;

		float square_scale = drawer.GetSquareScale(drawer.layout.default_element_y).x;
		UIConfigAbsoluteTransform cog_transform;

		if (sandbox_count > 0) {
			float combo_size = drawer.ComboBoxDefaultSize(combo_labels, { nullptr, 0 }, COMBO_PREFIX);

			combo_transform.position.x = drawer.GetAlignedToRight(combo_size).x;
			combo_transform.scale = { combo_size, drawer.GetElementDefaultScale().y };

			cog_transform.position.x = combo_transform.position.x - square_scale - drawer.layout.element_indentation;
			cog_transform.position.y = combo_transform.position.y;
			cog_transform.scale = { square_scale, drawer.layout.default_element_y };
		}

		if (data->draw_function == nullptr) {
			ChangeInspectorToNothing(editor_state, inspector_index);
		}

		data->draw_function(editor_state, inspector_index, data->draw_data, &drawer);

		// Draw now the combo for the sandbox
		// We draw later on rather then at that moment because it will interfere with what's on the first row
		if (sandbox_count > 0) {
			drawer.current_row_y_scale = zoomed_icon_size;

			// In order to detect the offset for the combo draw, look into the text stream, sprite stream and solid color stream and
			// see which is the bound
			UISpriteVertex* sprites = drawer.HandleSpriteBuffer(0);
			size_t sprite_count = *drawer.HandleSpriteCount(0);
			// The first sprite is always the padlock
			float row_y_start = sprites[0].position.y;
			float row_y_end = sprites[2].position.y;

			float sprite_bound = -1.0f;
			size_t last_index = 0;
			// The signs need to be inversed because the y starts from the bottom of the screen in vertex space
			// Normally they should be >= and <=
			while (last_index < sprite_count && sprites[last_index].position.y <= row_y_start && sprites[last_index + 2].position.y >= row_y_end) {
				last_index += 6;
			}
			if (last_index > 0) {
				last_index -= 6;
				sprite_bound = sprites[last_index + 1].position.x;
			}

			UISpriteVertex* text_sprites = drawer.HandleTextSpriteBuffer(0);
			size_t text_sprite_count = *drawer.HandleTextSpriteCount(0);
			float2 text_span = { 0.0f, 0.0f };
			float initial_text_position = -1.0f;

			if (text_sprite_count > 0) {
				last_index = 0;
				initial_text_position = text_sprites[0].position.x;

				// The signs need to be inversed because the y starts from the bottom of the screen in vertex space
				// Normally they should be >= and <=
				while (last_index < text_sprite_count && text_sprites[last_index].position.y <= row_y_start && text_sprites[last_index + 2].position.y >= row_y_end) {
					last_index += 6;
				}
				if (last_index > 0) {
					last_index -= 6;
					text_span = GetTextSpan(Stream<UISpriteVertex>(text_sprites, last_index));
				}
			}

			UIVertexColor* solid_color = drawer.HandleSolidColorBuffer(0);
			size_t solid_color_count = *drawer.HandleSolidColorCount(0);
			float solid_color_bound = -1.0f;
			if (solid_color_count > 0) {
				last_index = 0;
				// The title bar might get detected, skip them
				while (last_index < solid_color_count && (solid_color[last_index].position.y >= row_y_start || solid_color[last_index + 2].position.y <= row_y_end)) {
					last_index += 6;
				}
				size_t last_last_index = last_index;
				// The signs need to be inversed because the y starts from the bottom of the screen in vertex space
				// Normally they should be >= and <=
				while (last_index < solid_color_count && solid_color[last_index].position.y <= row_y_start && solid_color[last_index + 2].position.y >= row_y_end) {
					solid_color_bound = std::max(solid_color_bound, solid_color[last_index + 1].position.x);
					last_index += 6;
				}
			}

			float final_bound = initial_text_position + text_span.x + drawer.layout.element_indentation;
			final_bound = std::max(final_bound, solid_color_bound + drawer.layout.element_indentation);
			final_bound = std::max(final_bound, sprite_bound + drawer.layout.element_indentation);

			float cog_limit = final_bound;
			float offset = cog_limit - cog_transform.position.x;
			if (offset > 0.0f) {
				offset += drawer.region_render_offset.x;
				cog_transform.position.x += offset;
				combo_transform.position.x += offset;
			}

			// To the rightmost border draw the bound sandbox index
			config.flag_count = 0;
			config.AddFlag(combo_transform);

			UIConfigComboBoxPrefix combo_prefix;
			combo_prefix.prefix = COMBO_PREFIX;
			config.AddFlag(combo_prefix);

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

			// Draw the checkbox
			// For the moment, cast the pointer to unsigned char even tho it is an unsigned int
			// It behaves correctly since if the value stays lower than UCHAR_MAX
			drawer.ComboBox(
				UI_CONFIG_COMBO_BOX_NO_NAME | UI_CONFIG_COMBO_BOX_PREFIX | UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_ALIGN_TO_ROW_Y | UI_CONFIG_COMBO_BOX_CALLBACK,
				config,
				"Sandbox combo",
				combo_labels,
				sandbox_count,
				(unsigned char*)&editor_state->inspector_manager.data[inspector_index].target_sandbox
			);

			config.flag_count = 0;
			config.AddFlag(cog_transform);

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
				UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_ALIGN_TO_ROW_Y,
				config, 
				{ open_sandbox_settings, &open_data, sizeof(open_data) },
				ECS_TOOLS_UI_TEXTURE_COG
			);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

unsigned int CreateInspectorWindow(EditorState* editor_state, unsigned int inspector_index) {
	EDITOR_STATE(editor_state);

	UIWindowDescriptor descriptor;
	descriptor.initial_position_x = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.x);
	descriptor.initial_position_y = AlignMiddle(-1.0f, 2.0f, WINDOW_SIZE.y);
	descriptor.initial_size_x = WINDOW_SIZE.x;
	descriptor.initial_size_y = WINDOW_SIZE.y;

	size_t stack_memory[128];
	unsigned int* stack_inspector_index = (unsigned int*)stack_memory;
	*stack_inspector_index = inspector_index;
	InspectorSetDescriptor(descriptor, editor_state, stack_memory);

	return ui_system->Create_Window(descriptor);
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

	void (*change_functions[])(EditorState*, Stream<wchar_t>, unsigned int) = {
		ChangeInspectorToMeshFile,
		ChangeInspectorToTextureFile,
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

void RegisterInspectorSandbox(EditorState* editor_state) {
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
