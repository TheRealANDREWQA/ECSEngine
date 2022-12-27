#include "editorpch.h"
#include "AssetOverrides.h"
#include "ECSEngineUI.h"
#include "../Editor/EditorState.h"
#include "../Assets/AssetManagement.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Project/ProjectFolders.h"

#include "AssetIcons.h"

using namespace ECSEngine;

#define LAZY_EVALUATION_MILLISECONDS 250
#define SELECTION_INPUT_WINDOW_NAME "Selection Input Window"

struct GlobalOverrideData {
	EditorState* editor_state;
};

void ConvertMetadataNameAndFileToSelection(Stream<char> name, Stream<wchar_t> file, CapacityStream<char>& characters) {
	ECS_FORMAT_STRING(characters, "{#} ({#})", file, name);
}

struct BaseDrawData {
	BaseDrawData() {}

	Action callback_action;
	void* callback_action_data;
	bool callback_verify;

	EditorState* editor_state;
	CapacityStream<char> filter;
	ResizableLinearAllocator resizable_allocator;
	Timer timer;
	union {
		Stream<TablePair<Stream<Stream<char>>, Stream<wchar_t>>> asset_name_with_path;
		Stream<Stream<wchar_t>> asset_paths;
	};
	unsigned int sandbox_index;
	unsigned int* handle;
};

void FilterBar(UIDrawer* drawer, BaseDrawData* base_data) {
	if (drawer->initializer) {
		const size_t capacity = 256;
		base_data->filter.buffer = (char*)drawer->GetMainAllocatorBuffer(sizeof(char) * capacity);
		base_data->filter.capacity = capacity;
		base_data->filter.size = 0;
		
		drawer->element_descriptor.label_padd.x *= 0.5f;
		drawer->element_descriptor.label_padd.y *= 0.5f;
		drawer->font.size *= 0.8f;
		drawer->font.character_spacing *= 0.8f;
	}

	drawer->SetCurrentPositionToHeader();

	UIDrawConfig config;
	UIConfigWindowDependentSize size;
	size.scale_factor.x = drawer->GetWindowSizeScaleUntilBorder(true).x;
	size.offset = drawer->region_render_offset;
	config.AddFlag(size);

	UIConfigTextInputHint hint;
	hint.characters = "Search";
	config.AddFlag(hint);

	drawer->TextInput(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_INPUT_HINT | UI_CONFIG_TEXT_INPUT_NO_NAME 
		| UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_LATE_DRAW, config, "Filter", &base_data->filter);
	drawer->OffsetY(drawer->GetWindowSizeScaleElement(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, size.scale_factor).y);
	drawer->NextRow();
}

void InitializeBaseData(BaseDrawData* base_data) {
	const size_t initial_allocation_size = ECS_KB * 64;
	const size_t backup_allocation_size = ECS_KB * 256;
	base_data->resizable_allocator = ResizableLinearAllocator(
		base_data->editor_state->editor_allocator->Allocate(initial_allocation_size), 
		initial_allocation_size, 
		backup_allocation_size,
		base_data->editor_state->EditorAllocator()
	);
	// Make sure to trigger the retrieval immediately afterwards
	base_data->timer.DelayStart(-LAZY_EVALUATION_MILLISECONDS, ECS_TIMER_DURATION_MS);
}

// Returns true if the lazy evaluation was triggered
bool LazyRetrievalOfPaths(BaseDrawData* base_data, ECS_ASSET_TYPE type) {
	if (base_data->timer.GetDuration(ECS_TIMER_DURATION_MS) > LAZY_EVALUATION_MILLISECONDS) {
		base_data->resizable_allocator.Clear();

		AllocatorPolymorphic base_allocator = GetAllocatorPolymorphic(&base_data->resizable_allocator);
		Stream<Stream<wchar_t>> paths = base_data->editor_state->asset_database->GetMetadatasForType(type, base_allocator);
		if (type == ECS_ASSET_MESH || type == ECS_ASSET_TEXTURE || type == ECS_ASSET_SHADER || type == ECS_ASSET_MISC) {
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(table_allocator, ECS_KB * 128, ECS_MB);
			AllocatorPolymorphic allocator_polymorphic = GetAllocatorPolymorphic(&table_allocator);

			HashTableDefault<Stream<Stream<char>>> hash_table;
			hash_table.Initialize(&table_allocator, function::PowerOfTwoGreater(paths.size / 2 + 1));

			for (size_t index = 0; index < paths.size; index++) {
				ECS_STACK_CAPACITY_STREAM(wchar_t, _target_file, 256);
				AssetDatabase::ExtractFileFromFile(paths[index], _target_file);
				Stream<wchar_t> target_file = function::StringCopy(allocator_polymorphic, _target_file);

				ECS_STACK_CAPACITY_STREAM(char, temp_name, 256);
				AssetDatabase::ExtractNameFromFile(paths[index], temp_name);
				Stream<char> allocated_name = function::StringCopy(allocator_polymorphic, temp_name);

				ResourceIdentifier identifier = target_file;
				unsigned int table_index = hash_table.Find(identifier);
				Stream<char>* settings_buffer = nullptr;
				size_t settings_count = 0;
				if (table_index == -1) {
					// Insert it
					Stream<char>* settings = (Stream<char>*)table_allocator.Allocate(sizeof(Stream<char>));
					InsertIntoDynamicTable(hash_table, &table_allocator, Stream<Stream<char>>(settings, 1), identifier);

					settings_buffer = settings;
					settings_count = 0;
				}
				else {
					Stream<Stream<char>>* settings = hash_table.GetValuePtrFromIndex(table_index);			
					settings_count = settings->size;
					settings->AddResize(allocator_polymorphic, 1);
					settings_buffer = settings->buffer;
				}
				settings_buffer[settings_count] = allocated_name;
			}

			base_data->resizable_allocator.Clear();
			
			base_data->asset_name_with_path.Initialize(&base_data->resizable_allocator, hash_table.GetCount());
			base_data->asset_name_with_path.size = 0;
			hash_table.ForEachConst([&](Stream<Stream<char>> streams, ResourceIdentifier identifier) {
				ResourceIdentifier copy = identifier.Copy(base_allocator);
				base_data->asset_name_with_path.Add({ StreamDeepCopy(streams, base_allocator), { copy.ptr, copy.size / sizeof(wchar_t) } });
			});
		}
		else {
			base_data->asset_paths = paths;
		}
		base_data->timer.SetNewStart();
		return true;
	}
	return false;
}

struct SelectActionData {
	EditorState* editor_state;
	unsigned int* handle;

	Stream<char> name;
	Stream<wchar_t> file;

	unsigned int sandbox_index;
	ECS_ASSET_TYPE type;
	bool destroy_selection;

	Action action;
	void* action_data;
	bool verify_action;
};

void SelectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectActionData* data = (SelectActionData*)_data;
	if (data->name.buffer == nullptr) {
		// Relative name
		data->name.buffer = (char*)function::OffsetPointer(data, sizeof(*data));
	}

	bool register_asset = true;
	if (data->verify_action) {
		AssetOverrideCallbackVerifyData verify_data;
		verify_data.editor_state = data->editor_state;
		verify_data.name = data->name;
		verify_data.file = data->file;
		verify_data.prevent_registering = false;
		action_data->data = data->action_data;
		action_data->additional_data = &verify_data;
		data->action(action_data);

		register_asset = !verify_data.prevent_registering;
	}

	if (register_asset) {
		// If now it is loaded, launch the resource manager load task as well
		RegisterSandboxAsset(data->editor_state, data->sandbox_index, data->name, data->file, data->type, data->handle, true, { data->action, data->action_data, 0 });
	}

	system->PushDestroyWindowHandler(system->GetWindowIndexFromBorder(dockspace, border_index));
	if (data->destroy_selection) {
		unsigned int selection_input = system->GetWindowFromName(SELECTION_INPUT_WINDOW_NAME);
		if (selection_input != -1) {
			system->PushDestroyWindowHandler(selection_input);
		}
	}
}

struct DeselectActionData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	ECS_ASSET_TYPE asset_type;
	unsigned int* handle;

	Action callback_action;
	void* callback_action_data;
	bool verify_action;
};

void DeselectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	DeselectActionData* data = (DeselectActionData*)_data;

	bool unregister_asset = true;
	if (data->verify_action) {
		AssetOverrideCallbackVerifyData verify_data;
		verify_data.editor_state = data->editor_state;
		verify_data.name = data->editor_state->asset_database->GetAssetName(*data->handle, data->asset_type);
		verify_data.file = data->editor_state->asset_database->GetAssetPath(*data->handle, data->asset_type);
		verify_data.prevent_registering = false;
		action_data->data = data->callback_action_data;
		action_data->additional_data = &verify_data;
		data->callback_action(action_data);

		unregister_asset = !verify_data.prevent_registering;
	}

	if (unregister_asset) {
		UnregisterSandboxAsset(data->editor_state, data->sandbox_index, *data->handle, data->asset_type, { data->callback_action, data->callback_action_data, 0 });
	}

	unsigned int selection_input = system->GetWindowFromName(SELECTION_INPUT_WINDOW_NAME);
	if (selection_input != -1) {
		system->PushDestroyWindowHandler(selection_input);
	}

	// Make the handle equal to -1, even when the unload fails
	*data->handle = -1;
}

// Returns the total written size
void CreateSelectActionData(SelectActionData* select_data, const BaseDrawData* base_data, ECS_ASSET_TYPE asset_type) {
	select_data->editor_state = base_data->editor_state;
	select_data->handle = base_data->handle;
	select_data->sandbox_index = base_data->sandbox_index;
	select_data->type = asset_type;
	select_data->action = base_data->callback_action;
	select_data->action_data = base_data->callback_action_data;
	select_data->verify_action = base_data->callback_verify;
}

// The stack allocation must be at least ECS_KB * 4 large
UIDrawerMenuState GetMenuForSelection(const BaseDrawData* base_data, ECS_ASSET_TYPE type, unsigned int index, void* stack_allocation) {
	UIDrawerMenuState menu_state;

	unsigned int count = base_data->asset_name_with_path[index].value.size;
	
	menu_state.click_handlers = (UIActionHandler*)stack_allocation;
	stack_allocation = function::OffsetPointer(stack_allocation, sizeof(UIActionHandler) * count);

	for (unsigned int subindex = 0; subindex < count; subindex++) {
		SelectActionData* select_data = (SelectActionData*)stack_allocation;
		CreateSelectActionData(select_data, base_data, type);
		stack_allocation = function::OffsetPointer(stack_allocation, sizeof(*select_data));
		select_data->file = base_data->asset_name_with_path[index].identifier;
		select_data->name = base_data->asset_name_with_path[index].value[subindex];
		select_data->destroy_selection = true;
		menu_state.click_handlers[subindex] = { SelectAction, select_data, sizeof(*select_data) };
	}

	uintptr_t ptr = (uintptr_t)stack_allocation;
	char* left_characters = (char*)stack_allocation;
	Stream<char> new_line = "\n";

	size_t left_characters_size = 0;
	for (unsigned int subindex = 0; subindex < count; subindex++) {
		base_data->asset_name_with_path[index].value[subindex].CopyTo(ptr);
		new_line.CopyTo(ptr);
		
		left_characters_size += 1 + base_data->asset_name_with_path[index].value[subindex].size;
	}
	left_characters_size--;

	menu_state.row_count = count;
	menu_state.left_characters = { left_characters, left_characters_size };
	menu_state.submenu_index = 0;

	return menu_state;
}

struct AcquireCallbackData {
	EditorState* editor_state;
	ECS_ASSET_TYPE asset_type;
	unsigned int handle;
	CapacityStream<char>* selection;
};

void AcquireCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	AcquireCallbackData* data = (AcquireCallbackData*)_data;
	bool success = CreateAsset(data->editor_state, data->handle, data->asset_type);

	if (success) {
		Stream<char> asset_name = data->editor_state->asset_database->GetAssetName(data->handle, data->asset_type);
		Stream<wchar_t> asset_file = data->editor_state->asset_database->GetAssetPath(data->handle, data->asset_type);

		Stream<char> final_selection = asset_name;
		ECS_STACK_CAPACITY_STREAM(char, converted_name, 256);

		if (asset_file.size > 0) {
			ConvertMetadataNameAndFileToSelection(asset_name, asset_file, converted_name);
			final_selection = converted_name;

		}
		data->selection->Copy(final_selection);
	}
	else {
		EditorSetConsoleError("Failed to acquire the drag.");
	}
}

// Stack allocation needs to be at least ECS_KB * 4 large
struct DrawBaseReturn {
	CapacityStream<wchar_t> absolute_path;
	CapacityStream<char> converted_filename;
	UIDrawConfig label_config;
	UIDrawConfig config;
	void* stack_allocation;
	SelectActionData* select_data;
};

// Returns true if the lazy evaluation was triggered, else false
bool DrawBaseSelectionInput(UIDrawer& drawer, BaseDrawData* base_data, ECS_ASSET_TYPE type, DrawBaseReturn* return_value) {
	FilterBar(&drawer, base_data);
	if (drawer.initializer) {
		InitializeBaseData(base_data);
	}

	bool trigger_lazy_evaluation = LazyRetrievalOfPaths(base_data, type);

	drawer.SetDrawMode(ECS_UI_DRAWER_COLUMN_DRAW_FIT_SPACE, 2, 0.01f * drawer.zoom_ptr->y);

	const float RELATIVE_FACTOR = 3.5f;

	UIConfigRelativeTransform transform;
	transform.scale.y = RELATIVE_FACTOR;
	return_value->config.AddFlag(transform);

	SelectActionData* select_data = (SelectActionData*)return_value->stack_allocation;
	CreateSelectActionData(select_data, base_data, type);
	select_data->destroy_selection = false;
	return_value->select_data = select_data;
	return_value->stack_allocation = function::OffsetPointer(return_value->stack_allocation, sizeof(*select_data));

	return_value->converted_filename.InitializeFromBuffer(return_value->stack_allocation, 0, 256);
	return_value->stack_allocation = function::OffsetPointer(return_value->stack_allocation, sizeof(char) * 256);

	return_value->absolute_path.InitializeFromBuffer(return_value->stack_allocation, 0, 512);
	GetProjectAssetsFolder(base_data->editor_state, return_value->absolute_path);
	return_value->absolute_path.Add(ECS_OS_PATH_SEPARATOR);

	return_value->stack_allocation = function::OffsetPointer(return_value->stack_allocation, sizeof(wchar_t) * 512);

	UIConfigWindowDependentSize dependent_size;
	dependent_size.scale_factor.x = drawer.GetWindowSizeFactors(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, drawer.GetSquareScale() * RELATIVE_FACTOR).x;
	return_value->label_config.AddFlag(dependent_size);

	return trigger_lazy_evaluation;
}

void DrawDeselectButton(UIDrawer& drawer, DrawBaseReturn* base_return, ECS_ASSET_TYPE type) {
	// Do this only if the handle is different from -1
	if (*base_return->select_data->handle != -1) {
		DeselectActionData deselect_data;
		deselect_data.editor_state = base_return->select_data->editor_state;
		deselect_data.sandbox_index = base_return->select_data->sandbox_index;
		deselect_data.handle = base_return->select_data->handle;
		deselect_data.asset_type = type;
		deselect_data.callback_action = base_return->select_data->action;
		deselect_data.callback_action_data = base_return->select_data->action_data;
		deselect_data.verify_action = base_return->select_data->verify_action;

		// Draw a none button
		const size_t BASE_CONFIGURATION = UI_CONFIG_MAKE_SQUARE | UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
		drawer.Button(BASE_CONFIGURATION, base_return->config, "Deselect", { DeselectAction, &deselect_data, sizeof(deselect_data) });
	}
}

// The GetTexture must take in as parameters a Stream<wchar_t>* and a ResourceView* and set the one it wants to use
// alongside a CapacityStream<wchar_t>& having the assets_folder and the index. It can reject the current asset
// by not setting the resource view or the filename
template<typename LazyEvaluation, typename GetTexture>
void DrawFileAndName(UIDrawer& drawer, BaseDrawData* base_data, ECS_ASSET_TYPE type, LazyEvaluation&& lazy_evaluation, GetTexture&& get_texture) {
	size_t _stack_allocation[ECS_KB];

	DrawBaseReturn base_return;
	base_return.stack_allocation = _stack_allocation;
	bool trigger_lazy_evaluation = DrawBaseSelectionInput(drawer, base_data, type, &base_return);
	if (trigger_lazy_evaluation) {
		lazy_evaluation();
	}

	unsigned int base_size = base_return.absolute_path.size;

	for (size_t index = 0; index < base_data->asset_name_with_path.size; index++) {
		base_return.absolute_path.size = base_size;

		base_return.converted_filename.size = 0;
		Stream<wchar_t> filename = function::PathFilenameBoth(base_data->asset_name_with_path[index].identifier);
		function::ConvertWideCharsToASCII(filename, base_return.converted_filename);

		if (base_data->filter.size > 0) {
			if (function::FindFirstToken(base_return.converted_filename, base_data->filter).size == 0) {
				continue;
			}
		}
		
		Stream<wchar_t> texture = { nullptr, 0 };
		ResourceView resource_view = nullptr;

		get_texture(base_return.absolute_path, index, &texture, &resource_view);

		const size_t BASE_CONFIGURATION = UI_CONFIG_MAKE_SQUARE | UI_CONFIG_RELATIVE_TRANSFORM;

		bool rejected = texture.size == 0 && resource_view.Interface() == nullptr;
		
		if (!rejected) {
			if (base_data->asset_name_with_path[index].value.size == 1) {
				base_return.select_data->file = base_data->asset_name_with_path[index].identifier;
				base_return.select_data->name = base_data->asset_name_with_path[index].value[0];

				if (resource_view.view != nullptr) {
					drawer.SpriteButton(BASE_CONFIGURATION, base_return.config, { SelectAction, base_return.select_data, sizeof(*base_return.select_data) }, resource_view);
				}
				else {
					// Draw a file texture instead
					drawer.SpriteButton(
						BASE_CONFIGURATION,
						base_return.config,
						{ SelectAction, base_return.select_data, sizeof(*base_return.select_data) },
						texture,
						ECS_COLOR_WHITE
					);
				}
			}
			else {
				UIConfigMenuSprite menu_sprite;
				if (texture.size > 0) {
					menu_sprite.texture = texture;
					menu_sprite.color = drawer.color_theme.text;
				}
				else {
					menu_sprite.resource_view = resource_view;
				}

				size_t stack_allocation[ECS_KB];
				UIDrawerMenuState menu_state = GetMenuForSelection(base_data, ECS_ASSET_MESH, index, stack_allocation);

				float2 menu_position;
				float2 menu_scale;
				UIConfigGetTransform get_transform;
				get_transform.position = &menu_position;
				get_transform.scale = &menu_scale;
				base_return.config.AddFlag(get_transform);

				base_return.config.AddFlag(menu_sprite);
				drawer.PushIdentifierStackRandom(index);
				drawer.Menu(BASE_CONFIGURATION | UI_CONFIG_MENU_SPRITE | UI_CONFIG_GET_TRANSFORM, base_return.config, base_return.converted_filename, &menu_state);
				drawer.PopIdentifierStack();

				const float CORNER_ICON_FACTOR = 0.2f;
				float2 corner_icon_size = menu_scale * CORNER_ICON_FACTOR;
				float2 corner_icon_position = { menu_position.x + menu_scale.x - corner_icon_size.x, menu_position.y };

				// Draw a small plus in the right top corner
				drawer.SpriteRectangle(0, corner_icon_position, corner_icon_size, ECS_TOOLS_UI_TEXTURE_MASK, drawer.color_theme.theme);
				drawer.SpriteRectangle(0, corner_icon_position, corner_icon_size, ECS_TOOLS_UI_TEXTURE_PLUS, drawer.color_theme.text);

				// Both the menu sprite and the get transform
				base_return.config.flag_count -= 2;
			}

			// Draw the name
			drawer.TextLabel(UI_CONFIG_WINDOW_DEPENDENT_SIZE, base_return.label_config, base_return.converted_filename);
		}
	}

	DrawDeselectButton(drawer, &base_return, type);
}

// The GetTexture must take in as parameter a Stream<wchar_t>*
// alongside a CapacityStream<wchar_t>& having the assets_folder and the index
template<typename GetTexture>
void DrawOnlyName(UIDrawer& drawer, BaseDrawData* base_data, ECS_ASSET_TYPE type, GetTexture&& get_texture) {
	size_t _stack_allocation[ECS_KB];

	DrawBaseReturn base_return;
	base_return.stack_allocation = _stack_allocation;
	DrawBaseSelectionInput(drawer, base_data, type, &base_return);

	unsigned int base_size = base_return.absolute_path.size;

	for (size_t index = 0; index < base_data->asset_paths.size; index++) {
		base_return.absolute_path.size = base_size;
		base_return.converted_filename.size = 0;
		
		Stream<wchar_t> filename = function::PathFilenameBoth(base_data->asset_paths[index]);
		function::ConvertWideCharsToASCII(filename, base_return.converted_filename);

		if (base_data->filter.size > 0) {
			if (function::FindFirstToken(base_return.converted_filename, base_data->filter).size == 0) {
				continue;
			}
		}

		Stream<wchar_t> texture = { nullptr, 0 };
		get_texture(base_return.absolute_path, index, &texture);

		Stream<char> stem = function::PathStem(base_return.converted_filename);

		base_return.select_data->file = { nullptr, 0 };
		// The name will be deduced from the relative part
		base_return.select_data->name = { nullptr, stem.size };

		const size_t BASE_CONFIGURATION = UI_CONFIG_MAKE_SQUARE | UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_SPRITE_BUTTON_MULTI_TEXTURE;

		Color multi_texture_color = drawer.color_theme.theme;

		UIConfigSpriteButtonMultiTexture multi_texture;
		multi_texture.texture_colors = &multi_texture_color;
		multi_texture.textures = { &texture, 1 };
		base_return.config.AddFlag(multi_texture);

		// Draw a file texture instead
		drawer.SpriteButton(
			BASE_CONFIGURATION,
			base_return.config,
			{ SelectAction, base_return.select_data, sizeof(*base_return.select_data) + (unsigned int)stem.size },
			ECS_TOOLS_UI_TEXTURE_FILE_BLANK,
			drawer.color_theme.text
		);
		base_return.config.flag_count--;

		// Draw the name
		drawer.TextLabel(UI_CONFIG_WINDOW_DEPENDENT_SIZE, base_return.label_config, base_return.converted_filename);
	}

	DrawDeselectButton(drawer, &base_return, type);
}

#pragma region Mesh Draw

struct MeshDrawData {
	BaseDrawData base_data;
};

void MeshDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	MeshDrawData* data = (MeshDrawData*)window_data;

	DrawFileAndName(drawer, &data->base_data, ECS_ASSET_MESH, []() {}, 
		[data](CapacityStream<wchar_t>& absolute_path, unsigned int index, Stream<wchar_t>* texture, ResourceView* resource_view) {
			absolute_path.AddStreamSafe(data->base_data.asset_name_with_path[index].identifier);
			// Replace relative slashes with absolute ones
			function::ReplaceCharacter(absolute_path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
			ResourceView thumbnail = FileExplorerGetMeshThumbnail(data->base_data.editor_state, absolute_path);
			if (thumbnail.view == nullptr) {
				*texture = ECS_TOOLS_UI_TEXTURE_FILE_BLANK;
			}
			else {
				*resource_view = thumbnail;
			}
	});
}

#pragma endregion

#pragma region Texture Draw

struct TextureDrawData {
	BaseDrawData base_data;
};

void TextureDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	TextureDrawData* data = (TextureDrawData*)window_data;

	DrawFileAndName(drawer, &data->base_data, ECS_ASSET_TEXTURE, []() {},
		[data](CapacityStream<wchar_t>& absolute_path, unsigned int index, Stream<wchar_t>* texture, ResourceView* resource_view) {
			absolute_path.AddStreamSafe(data->base_data.asset_name_with_path[index].identifier);
			// Replace relative slashes with absolute ones
			function::ReplaceCharacter(absolute_path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
			*texture = absolute_path;
	});
}

#pragma endregion

#pragma region GPU Sampler Draw

struct GPUSamplerDrawData {
	BaseDrawData base_data;
};

void GPUSamplerDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	GPUSamplerDrawData* data = (GPUSamplerDrawData*)window_data;

	DrawOnlyName(drawer, &data->base_data, ECS_ASSET_GPU_SAMPLER, [data](CapacityStream<wchar_t>& absolute_path, unsigned int index, Stream<wchar_t>* texture) {
		*texture = ASSET_GPU_SAMPLER_ICON;
	});
}

#pragma endregion

#pragma region Shader Draw

struct ShaderDrawData {
	BaseDrawData base_data;
	// This shader metadata is in lockstep with the asset_name_with_path
	// These are cached such that we don't have to read the file each time
	// we do the display
	ShaderMetadata* shader_metadata;
	ECS_SHADER_TYPE shader_type;
};

void ShaderDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	ShaderDrawData* data = (ShaderDrawData*)window_data;

	DrawFileAndName(drawer, &data->base_data, ECS_ASSET_SHADER, 
		[&]() {
			// Resize the shader metadata
			size_t path_count = data->base_data.asset_name_with_path.size;
			data->shader_metadata = (ShaderMetadata*)data->base_data.resizable_allocator.Allocate(sizeof(ShaderMetadata) * path_count);
			for (size_t index = 0; index < path_count; index++) {
				Stream<char> current_name = data->base_data.asset_name_with_path[index].value[0];
				Stream<wchar_t> current_file = data->base_data.asset_name_with_path[index].identifier;
				bool success = data->base_data.editor_state->asset_database->ReadShaderFile(current_name, current_file, data->shader_metadata + index);
				if (!success) {
					// Signal that the file was not found
					data->shader_metadata[index].shader_type = ECS_SHADER_TYPE_COUNT;
				}
			}
		},
		[data](CapacityStream<wchar_t>& absolute_path, unsigned int index, Stream<wchar_t>* texture, ResourceView* resource_view) {
			// Verify that the shader type matches the current type of the index
			if (data->shader_metadata[index].shader_type != ECS_SHADER_TYPE_COUNT && (data->shader_metadata[index].shader_type == data->shader_type 
				|| data->shader_type == ECS_SHADER_TYPE_COUNT)) {
				*texture = ASSET_SHADER_ICON;
			}
	});
}

#pragma endregion

#pragma region Material Draw

struct MaterialDrawData {
	BaseDrawData base_data;
};

void MaterialDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	MaterialDrawData* data = (MaterialDrawData*)window_data;

	DrawOnlyName(drawer, &data->base_data, ECS_ASSET_MATERIAL, [data](CapacityStream<wchar_t>& absolute_path, unsigned int index, Stream<wchar_t>* texture) {
		*texture = ASSET_MATERIAL_ICON;
	});
}

#pragma endregion

#pragma region Misc Draw

struct MiscDrawData {
	BaseDrawData base_data;
};

void MiscDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	MiscDrawData* data = (MiscDrawData*)window_data;

	DrawFileAndName(drawer, &data->base_data, ECS_ASSET_MISC, []() {},
		[data](CapacityStream<wchar_t>& absolute_path, unsigned int index, Stream<wchar_t>* texture, ResourceView* resource_view) {
			*texture = ASSET_MISC_ICON;
		}
	);
}

#pragma endregion

#pragma region Drawers

struct OverrideBaseData {
	EditorState* editor_state;
	CapacityStream<char> selection;
	unsigned int sandbox_index;
	bool callback_is_ptr;
	Action callback;
	bool callback_verify;

	union {
		// Embedd the data directly into it
		size_t callback_data[8];
		void* callback_data_ptr;
	};
};

void OverrideAssetHandle(
	UIDrawer* drawer, 
	UIDrawConfig* config, 
	size_t configuration,
	void* field_data, 
	Stream<char> field_name, 
	OverrideBaseData* base_data,
	WindowDraw window_draw,
	void* window_data,
	size_t window_data_size,
	BaseDrawData* base_window_data,
	ECS_ASSET_TYPE type
) {
	AssetDatabase* database = base_data->editor_state->asset_database;

	unsigned int* handle = (unsigned int*)field_data;

	base_window_data->editor_state = base_data->editor_state;
	base_window_data->handle = handle;
	base_window_data->sandbox_index = base_data->sandbox_index;
	base_window_data->callback_action = base_data->callback;
	base_window_data->callback_action_data = base_data->callback_is_ptr ? base_data->callback_data_ptr : base_data->callback_data;
	base_window_data->callback_verify = base_data->callback_verify;

	UIWindowDescriptor window_descriptor;
	window_descriptor.draw = window_draw;
	window_descriptor.window_data = window_data;
	window_descriptor.window_data_size = window_data_size;
	window_descriptor.window_name.size = 0;
	window_descriptor.initial_size_x = 0.4f;
	window_descriptor.initial_size_y = 0.7f;

	UIConfigSelectionInputOverrideHoverable override_hoverable;

	// Update the selection if the handle has changed
	ECS_STACK_CAPACITY_STREAM(char, composite_string, 512);
	ECS_STACK_CAPACITY_STREAM(char, hoverable_string, 512);

	unsigned int handle_value = *handle;
	if (handle_value != -1) {
		Stream<char> name = base_data->editor_state->asset_database->GetAssetName(handle_value, type);
		Stream<wchar_t> file = base_data->editor_state->asset_database->GetAssetPath(handle_value, type);

		if (file.size > 0) {
			Stream<wchar_t> filename = function::PathFilenameBoth(file);
			if (filename.size != file.size) {
				configuration |= UI_CONFIG_SELECTION_INPUT_OVERRIDE_HOVERABLE;
				ECS_FORMAT_STRING(hoverable_string, "{#} ({#})", file, name);
				override_hoverable.text = hoverable_string;
				override_hoverable.stable = false;
				config->AddFlag(override_hoverable);
			}

			// Name and file. Put the file first and the name in paranthesis
			ECS_FORMAT_STRING(composite_string, "{#} ({#})", filename, name);
		}
		else {
			composite_string = name;
		}
	}
	base_data->selection.Copy(composite_string);

	// Draw the field as a selection input
	drawer->SelectionInput(configuration, *config, field_name, &base_data->selection, ECS_TOOLS_UI_TEXTURE_FOLDER, &window_descriptor);
	if (configuration & UI_CONFIG_SELECTION_INPUT_OVERRIDE_HOVERABLE) {
		config->flag_count--;
	}
}

struct OverrideMeshData {
	OverrideBaseData base_data;
};

ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideMeshHandle) {
	OverrideMeshData* data = (OverrideMeshData*)extra_data;

	MeshDrawData draw_data;
	OverrideAssetHandle(drawer, config, configuration, field_data, field_name, &data->base_data, MeshDraw, &draw_data, sizeof(draw_data), &draw_data.base_data, ECS_ASSET_MESH);
}

struct OverrideTextureData {
	OverrideBaseData base_data;
};

ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideTextureHandle) {
	OverrideTextureData* data = (OverrideTextureData*)extra_data;

	TextureDrawData draw_data;
	OverrideAssetHandle(drawer, config, configuration, field_data, field_name, &data->base_data, TextureDraw, &draw_data, sizeof(draw_data), &draw_data.base_data, ECS_ASSET_TEXTURE);
}

struct OverrideGPUSamplerData {
	OverrideBaseData base_data;
};

ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideGPUSamplerHandle) {
	OverrideGPUSamplerData* data = (OverrideGPUSamplerData*)extra_data;

	GPUSamplerDrawData draw_data;
	OverrideAssetHandle(drawer, config, configuration, field_data, field_name, &data->base_data, GPUSamplerDraw, &draw_data, sizeof(draw_data), &draw_data.base_data, ECS_ASSET_GPU_SAMPLER);
}

struct OverrideShaderData {
	OverrideBaseData base_data;
};

template<ECS_SHADER_TYPE shader_type>
ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideShaderHandle) {
	OverrideShaderData* data = (OverrideShaderData*)extra_data;

	ShaderDrawData draw_data;
	draw_data.shader_type = shader_type;
	OverrideAssetHandle(drawer, config, configuration, field_data, field_name, &data->base_data, ShaderDraw, &draw_data, sizeof(draw_data), &draw_data.base_data, ECS_ASSET_SHADER);
}

struct OverrideMaterialData {
	OverrideBaseData base_data;
};

ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideMaterialHandle) {
	OverrideMaterialData* data = (OverrideMaterialData*)extra_data;

	MaterialDrawData draw_data;
	OverrideAssetHandle(drawer, config, configuration, field_data, field_name, &data->base_data, MaterialDraw, &draw_data, sizeof(draw_data), &draw_data.base_data, ECS_ASSET_MATERIAL);
}

struct OverrideMiscData {
	OverrideBaseData base_data;
};

ECS_UI_REFLECTION_INSTANCE_DRAW_CUSTOM(OverrideMiscHandle) {
	OverrideMiscData* data = (OverrideMiscData*)extra_data;

	MiscDrawData draw_data;
	OverrideAssetHandle(drawer, config, configuration, field_data, field_name, &data->base_data, MiscDraw, &draw_data, sizeof(draw_data), &draw_data.base_data, ECS_ASSET_MISC);
}

#pragma endregion

#pragma region Initializers 

ECS_UI_REFLECTION_INSTANCE_INITIALIZE_OVERRIDE(AssetInitialize) {
	// Set the editor state and selection buffer
	OverrideBaseData* data = (OverrideBaseData*)_data;
	GlobalOverrideData* global_data = (GlobalOverrideData*)_global_data;

	const size_t capacity = 256;

	data->selection.buffer = (char*)drawer->allocator->Allocate(sizeof(char) * capacity, alignof(char));
	data->selection.size = 0;
	data->selection.capacity = capacity;

	data->editor_state = global_data->editor_state;
	data->callback_verify = false;
}

#pragma endregion

#pragma region Deallocators

ECS_UI_REFLECTION_INSTANCE_DEALLOCATE_OVERRIDE(AssetDeallocate) {
	// Every asset has a OverrideBaseData as the first field
	OverrideBaseData* data = (OverrideBaseData*)_data;
	if (data->selection.capacity > 0) {
		Deallocate(allocator, data->selection.buffer);
	}
}

#pragma endregion

#pragma region Modifiers

ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(AssetOverrideSetSandboxIndex) {
	OverrideBaseData* data = (OverrideBaseData*)_data;
	AssetOverrideSetSandboxIndexData* modify_data = (AssetOverrideSetSandboxIndexData*)user_data;
	data->sandbox_index = modify_data->sandbox_index;
}


ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(AssetOverrideBindCallback) {
	OverrideBaseData* data = (OverrideBaseData*)_data;
	AssetOverrideBindCallbackData* modify_data = (AssetOverrideBindCallbackData*)user_data;
	data->callback = modify_data->handler.action;
	ECS_ASSERT(modify_data->handler.data_size <= sizeof(data->callback_data));
	if (modify_data->handler.data_size == 0) {
		data->callback_data_ptr = modify_data->handler.data;
		data->callback_is_ptr = true;
	}
	else {
		memcpy(data->callback_data, modify_data->handler.data, modify_data->handler.data_size);
		data->callback_is_ptr = false;
	}
	data->callback_verify = modify_data->verify_handler;
}

ECS_UI_REFLECTION_INSTANCE_MODIFY_OVERRIDE(AssetOverrideSetAll) {
	AssetOverrideSetAllData* modify_data = (AssetOverrideSetAllData*)user_data;
	AssetOverrideSetSandboxIndex(allocator, _data, _global_data, &modify_data->set_index);
	AssetOverrideBindCallback(allocator, _data, _global_data, &modify_data->callback);
}

#pragma endregion

void SetUIOverrideBase(EditorState* editor_state, UIReflectionFieldOverride* override, AllocatorPolymorphic allocator) {
	override->initialize_function = AssetInitialize;
	override->deallocate_function = AssetDeallocate;
	GlobalOverrideData* override_data = (GlobalOverrideData*)Allocate(allocator, sizeof(GlobalOverrideData));
	override_data->editor_state = editor_state;
	override->global_data = override_data;
	override->global_data_size = sizeof(GlobalOverrideData);
}

void GetEntityComponentUIOverrides(EditorState* editor_state, UIReflectionFieldOverride* overrides, AllocatorPolymorphic allocator) {
	size_t count = AssetUIOverrideCount();
	for (size_t index = 0; index < count; index++) {
		SetUIOverrideBase(editor_state, overrides + index, allocator);
	}

	unsigned int current_index = 0;
	overrides[current_index].draw_function = OverrideMeshHandle;
	overrides[current_index].tag = STRING(ECS_MESH_HANDLE);
	overrides[current_index].draw_data_size = sizeof(OverrideMeshData);
	current_index++;

	overrides[current_index].draw_function = OverrideTextureHandle;
	overrides[current_index].tag = STRING(ECS_TEXTURE_HANDLE);
	overrides[current_index].draw_data_size = sizeof(OverrideTextureData);
	current_index++;

	overrides[current_index].draw_function = OverrideGPUSamplerHandle;
	overrides[current_index].tag = STRING(ECS_GPU_SAMPLER_HANDLE);
	overrides[current_index].draw_data_size = sizeof(OverrideGPUSamplerData);
	current_index++;

	overrides[current_index].draw_function = OverrideShaderHandle<ECS_SHADER_TYPE_COUNT>;
	overrides[current_index].tag = STRING(ECS_SHADER_HANDLE);
	overrides[current_index].draw_data_size = sizeof(OverrideShaderData);
	current_index++;

	overrides[current_index].draw_function = OverrideShaderHandle<ECS_SHADER_VERTEX>;
	overrides[current_index].tag = STRING(ECS_VERTEX_SHADER_HANDLE);
	overrides[current_index].draw_data_size = sizeof(OverrideShaderData);
	current_index++;

	overrides[current_index].draw_function = OverrideShaderHandle<ECS_SHADER_PIXEL>;
	overrides[current_index].tag = STRING(ECS_PIXEL_SHADER_HANDLE);
	overrides[current_index].draw_data_size = sizeof(OverrideShaderData);
	current_index++;

	overrides[current_index].draw_function = OverrideShaderHandle<ECS_SHADER_COMPUTE>;
	overrides[current_index].tag = STRING(ECS_COMPUTE_SHADER_HANDLE);
	overrides[current_index].draw_data_size = sizeof(OverrideShaderData);
	current_index++;

	overrides[current_index].draw_function = OverrideMaterialHandle;
	overrides[current_index].tag = STRING(ECS_MATERIAL_HANDLE);
	overrides[current_index].draw_data_size = sizeof(OverrideMaterialData);
	current_index++;

	overrides[current_index].draw_function = OverrideMiscHandle;
	overrides[current_index].tag = STRING(ECS_MISC_HANDLE);
	overrides[current_index].draw_data_size = sizeof(OverrideMiscData);
	current_index++;
}

void AssetOverrideBindInstanceOverrides(
	UIReflectionDrawer* drawer, 
	UIReflectionInstance* instance, 
	unsigned int sandbox_index, 
	UIActionHandler modify_action_handler
)
{
	AssetOverrideSetAllData set_data;
	set_data.set_index.sandbox_index = sandbox_index;
	set_data.callback.handler = modify_action_handler;

	size_t count = AssetUIOverrideCount();
	for (size_t index = 0; index < count; index++) {
		drawer->BindInstanceFieldOverride(instance, ECS_ASSET_METADATA_MACROS[index].name, AssetOverrideSetAll, &set_data);
	}
}