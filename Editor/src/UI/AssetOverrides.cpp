#include "editorpch.h"
#include "AssetOverrides.h"
#include "ECSEngineUI.h"
#include "../Editor/EditorState.h"
#include "../Assets/AssetManagement.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../Project/ProjectFolders.h"
#include "../UI/Inspector.h"

#include "AssetIcons.h"

using namespace ECSEngine;

#define LAZY_EVALUATION_MILLISECONDS 250
#define SELECTION_INPUT_WINDOW_NAME "Selection Input Window"

struct GlobalOverrideData {
	EditorState* editor_state;
};

ECS_INLINE void ConvertMetadataNameAndFileToSelection(Stream<char> name, Stream<wchar_t> file, CapacityStream<char>& characters) {
	FormatString(characters, "{#} ({#})", file, name);
}

struct BaseDrawData {
	ECS_INLINE BaseDrawData() {}

	Action callback_action;
	void* callback_action_data;

	Action registration_callback_action;
	void* registration_callback_action_data;

	bool callback_verify;
	// Used only for deselection
	bool callback_before_handle_update;
	// Used only for selection
	bool callback_disable_selection_unregistering;
	bool callback_is_single_threaded;

	EditorState* editor_state;
	AssetDatabase* target_database;
	CapacityStream<char> filter;
	ResizableLinearAllocator resizable_allocator;
	Timer timer;
	union {
		Stream<TablePair<Stream<Stream<char>>, Stream<wchar_t>>> asset_name_with_path;
		Stream<Stream<wchar_t>> asset_paths;
	};
	unsigned int sandbox_index;

	void* asset_target_field;
	// Needed to know how to set the field data
	const Reflection::ReflectionField* reflection_field;
};

static void FilterBar(UIDrawer* drawer, BaseDrawData* base_data) {
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

static void InitializeBaseData(BaseDrawData* base_data) {
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
static bool LazyRetrievalOfPaths(BaseDrawData* base_data, ECS_ASSET_TYPE type) {
	if (base_data->timer.GetDuration(ECS_TIMER_DURATION_MS) > LAZY_EVALUATION_MILLISECONDS) {
		base_data->resizable_allocator.Clear();

		AllocatorPolymorphic base_allocator = &base_data->resizable_allocator;
		Stream<Stream<wchar_t>> paths = base_data->editor_state->asset_database->GetMetadatasForType(type, base_allocator);
		if (type == ECS_ASSET_MESH || type == ECS_ASSET_TEXTURE || type == ECS_ASSET_SHADER || type == ECS_ASSET_MISC) {
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(table_allocator, ECS_KB * 128, ECS_MB);
			AllocatorPolymorphic allocator_polymorphic = &table_allocator;

			HashTableDefault<Stream<Stream<char>>> hash_table;
			hash_table.Initialize(&table_allocator, PowerOfTwoGreater(paths.size / 2 + 1));

			for (size_t index = 0; index < paths.size; index++) {
				ECS_STACK_CAPACITY_STREAM(char, temp_name, 256);
				AssetDatabase::ExtractNameFromFile(paths[index], temp_name);
				Stream<char> allocated_name = StringCopy(allocator_polymorphic, temp_name);

				ECS_STACK_CAPACITY_STREAM(wchar_t, _target_file, 256);
				AssetDatabase::ExtractFileFromFile(paths[index], _target_file);
				if (_target_file.size == 0 && IsThunkOrForwardingFile(type) && AssetHasFile(type)) {
					size_t metadata_storage[AssetMetadataMaxSizetSize()];
					ECS_STACK_LINEAR_ALLOCATOR(temporary_allocator, ECS_KB * 8);
					bool success = base_data->editor_state->asset_database->ReadAssetFile(allocated_name, {}, metadata_storage, type, &temporary_allocator);
					if (success) {
						_target_file.CopyOther(GetAssetFile(metadata_storage, type));
					}
				}
				Stream<wchar_t> target_file = StringCopy(allocator_polymorphic, _target_file);

				ResourceIdentifier identifier = target_file;
				unsigned int table_index = hash_table.Find(identifier);
				Stream<char>* settings_buffer = nullptr;
				size_t settings_count = 0;
				if (table_index == -1) {
					// Insert it
					Stream<char>* settings = (Stream<char>*)table_allocator.Allocate(sizeof(Stream<char>));
					hash_table.InsertDynamic(&table_allocator, Stream<Stream<char>>(settings, 1), identifier);

					settings_buffer = settings;
					settings_count = 0;
				}
				else {
					Stream<Stream<char>>* settings = hash_table.GetValuePtrFromIndex(table_index);			
					settings_count = settings->size;
					settings->Expand(allocator_polymorphic, 1, true);
					settings_buffer = settings->buffer;
				}
				settings_buffer[settings_count] = allocated_name;
			}

			base_data->resizable_allocator.Clear();
			
			base_data->asset_name_with_path.Initialize(&base_data->resizable_allocator, hash_table.GetCount());
			base_data->asset_name_with_path.size = 0;
			hash_table.ForEachConst([&](Stream<Stream<char>> streams, ResourceIdentifier identifier) {
				ResourceIdentifier copy = identifier.Copy(base_allocator);
				base_data->asset_name_with_path.Add({ StreamCoalescedDeepCopy(streams, base_allocator), { copy.ptr, copy.size / sizeof(wchar_t) } });
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
	void* asset_target_data;
	const Reflection::ReflectionField* reflection_field;
	AssetDatabase* target_database;

	Stream<char> name;
	Stream<wchar_t> file;

	unsigned int sandbox_index;
	ECS_ASSET_TYPE type;
	bool destroy_selection;

	// This will be called when the asset has finished loading
	Action action;
	void* action_data;

	// This will be called when the selection is changed
	Action asset_registration_action;
	void* asset_registration_action_data;

	// You can set this boolean to indicate that the sandbox will be unregistered manually (or possibly skipped)
	// By default this is set to false, as to not create leaks
	bool do_not_unregister_asset;
	bool verify_action;

	// Used only for deselection
	bool callback_before_handle_update;
	bool callback_is_single_threaded;
};

void SelectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SelectActionData* data = (SelectActionData*)_data;
	if (data->name.buffer == nullptr) {
		// Relative name
		data->name.buffer = (char*)OffsetPointer(data, sizeof(*data));
	}

	RegisterAssetTarget register_target(data->reflection_field, data->asset_target_data, data->type);

	// Only register the asset if the asset is to be loaded into the main database
	if (data->target_database == data->editor_state->asset_database) {
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
			// We need to wrap the callback into a wrapper such that we respect the
			// AssetOverrideCallbackAdditionalInfo* additional_data field

			struct WrapperData {
				Action action;
				// We just reference this
				void* action_data;
			};

			auto callback_wrapper = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				const RegisterAssetEventCallbackInfo* register_info = (const RegisterAssetEventCallbackInfo*)_additional_data;
				AssetOverrideCallbackAdditionalInfo override_info;
				override_info.handle = register_info->handle;
				override_info.success = true;
				override_info.type = register_info->type;
				override_info.is_selection = true;
				override_info.previous_handle = register_info->previous_handle;
				action_data->additional_data = &override_info;
				
				const WrapperData* data = (const WrapperData*)_data;
				action_data->data = data->action_data;
				data->action(action_data);
			};

			WrapperData wrapper_data;
			UIActionHandler callback_handler = {};
			if (data->action != nullptr) {
				callback_handler.action = callback_wrapper;
				callback_handler.data = &wrapper_data;
				callback_handler.data_size = sizeof(wrapper_data);

				wrapper_data.action = data->action;
				wrapper_data.action_data = data->action_data;
			}

			if (data->asset_registration_action != nullptr) {
				AssetOverrideCallbackRegistrationAdditionalInfo additional_info;
				additional_info.asset_field = data->asset_target_data;
				additional_info.reflection_field = data->reflection_field;
				additional_info.type = data->type;
				action_data->data = data->asset_registration_action_data;
				action_data->additional_data = &additional_info;
				data->asset_registration_action(action_data);
			}

			RegisterSandboxAsset(data->editor_state, data->sandbox_index, data->name, data->file, data->type, register_target, !data->do_not_unregister_asset, callback_handler, data->callback_is_single_threaded);
		}
	}
	else {
		unsigned int previous_handle = register_target.GetHandle(data->target_database);
		unsigned int new_handle = data->target_database->AddAsset(data->name, data->file, data->type);
		register_target.SetHandle(data->target_database, new_handle);
		if (data->action != nullptr) {
			AssetOverrideCallbackAdditionalInfo info;
			info.handle = new_handle;
			info.type = data->type;
			info.success = new_handle != -1;
			info.is_selection = true;
			info.previous_asset = nullptr;
			info.previous_handle = previous_handle;

			ActionData dummy_data;
			dummy_data.data = data->action_data;
			dummy_data.additional_data = &info;
			dummy_data.system = nullptr;
			data->action(&dummy_data);
		}
	}

	system->PushDestroyWindowHandler(window_index);
	if (data->destroy_selection) {
		unsigned int selection_input = system->GetWindowFromName(SELECTION_INPUT_WINDOW_NAME);
		if (selection_input != -1) {
			system->PushDestroyWindowHandler(selection_input);
		}
	}
}

struct DeselectActionData {
	EditorState* editor_state;
	RegisterAssetTarget asset_target;
	AssetDatabase* target_database;

	unsigned int sandbox_index;
	ECS_ASSET_TYPE asset_type;

	Action callback_action;
	void* callback_action_data;
	bool verify_action;
	// Used only for deselection
	bool callback_before_handle_update;
};

void DeselectAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	DeselectActionData* data = (DeselectActionData*)_data;

	unsigned int handle_value = data->asset_target.GetHandle(data->target_database);
	alignas(alignof(size_t)) char previous_asset[AssetMetadataMaxByteSize()];
	const void* previous_asset_database = data->target_database->GetAssetConst(handle_value, data->asset_type);
	memcpy(previous_asset, previous_asset_database, AssetMetadataByteSize(data->asset_type));

	auto trigger_callback = [&]() {
		AssetOverrideCallbackAdditionalInfo info;
		info.handle = handle_value;
		info.type = data->asset_type;
		info.success = true;
		info.is_selection = false;
		info.previous_asset = previous_asset;

		ActionData dummy_data;
		dummy_data.system = nullptr;
		dummy_data.data = data->callback_action_data;
		dummy_data.additional_data = &info;
		data->callback_action(&dummy_data);
	};

	// Only if the target database matches the editor database
	if (data->target_database == data->editor_state->asset_database) {
		bool unregister_asset = true;
		if (data->verify_action) {
			AssetOverrideCallbackVerifyData verify_data;
			verify_data.editor_state = data->editor_state;
			verify_data.name = data->editor_state->asset_database->GetAssetName(handle_value, data->asset_type);
			verify_data.file = data->editor_state->asset_database->GetAssetPath(handle_value, data->asset_type);
			verify_data.prevent_registering = false;
			action_data->data = data->callback_action_data;
			action_data->additional_data = &verify_data;
			data->callback_action(action_data);

			unregister_asset = !verify_data.prevent_registering;
		}

		if (unregister_asset) {
			// If the index is -1, then it is a global asset
			// We need to wrap the callback into a wrapper such that we respect the AssetOverrideCallbackAdditionalInfo*
			// additional_data field convention

			struct WrapperData {
				Action action;
				void* action_data;
			};

			auto callback_wrapper = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				const WrapperData* data = (const WrapperData*)_data;
				const UnregisterAssetEventCallbackInfo* unregister_info = (const UnregisterAssetEventCallbackInfo*)_additional_data;

				AssetOverrideCallbackAdditionalInfo info;
				info.handle = unregister_info->handle;
				info.type = unregister_info->type;
				info.success = true;
				info.is_selection = false;
				// We cannot provide this value
				info.previous_asset = nullptr;
				info.previous_handle = -1;
				action_data->additional_data = &info;

				action_data->data = data->action_data;
				data->action(action_data);
			};

			WrapperData wrapper_data;
			UIActionHandler callback_handler = {};
			if (data->callback_action != nullptr) {
				callback_handler.action = callback_wrapper;
				callback_handler.data = &wrapper_data;
				callback_handler.data_size = sizeof(wrapper_data);

				wrapper_data.action = data->callback_action;
				wrapper_data.action_data = data->callback_action_data;
			}
			if (data->sandbox_index != -1) {
				UnregisterSandboxAsset(data->editor_state, data->sandbox_index, handle_value, data->asset_type, callback_handler);
			}
			else {
				UnregisterGlobalAsset(data->editor_state, handle_value, data->asset_type, callback_handler);
			}
		}
	}
	else {
		// The handle can be modified inside - use the stored handle value
		if (data->callback_action != nullptr && data->callback_before_handle_update) {
			trigger_callback();
		}
		data->target_database->RemoveAsset(handle_value, data->asset_type);
	}

	unsigned int selection_input = system->GetWindowFromName(SELECTION_INPUT_WINDOW_NAME);
	if (selection_input != -1) {
		system->PushDestroyWindowHandler(selection_input);
	}
	
	data->asset_target.SetHandle(data->target_database, -1);

	if (data->callback_action != nullptr && !data->callback_before_handle_update && !data->verify_action) {
		trigger_callback();
	}
}

static void CreateSelectActionData(SelectActionData* select_data, const BaseDrawData* base_data, ECS_ASSET_TYPE asset_type) {
	select_data->editor_state = base_data->editor_state;
	select_data->reflection_field = base_data->reflection_field;
	select_data->asset_target_data = base_data->asset_target_field;
	select_data->sandbox_index = base_data->sandbox_index;
	select_data->type = asset_type;
	select_data->action = base_data->callback_action;
	select_data->action_data = base_data->callback_action_data;
	select_data->verify_action = base_data->callback_verify;
	select_data->target_database = base_data->target_database;
	select_data->callback_before_handle_update = base_data->callback_before_handle_update;
	select_data->do_not_unregister_asset = base_data->callback_disable_selection_unregistering;
	select_data->callback_is_single_threaded = base_data->callback_is_single_threaded;
	select_data->asset_registration_action = base_data->registration_callback_action;
	select_data->asset_registration_action_data = base_data->registration_callback_action_data;
}

// The stack allocation must be at least ECS_KB * 4 large
static UIDrawerMenuState GetMenuForSelection(const BaseDrawData* base_data, ECS_ASSET_TYPE type, unsigned int index, void* stack_allocation) {
	UIDrawerMenuState menu_state;

	unsigned int count = base_data->asset_name_with_path[index].value.size;
	
	menu_state.click_handlers = (UIActionHandler*)stack_allocation;
	stack_allocation = OffsetPointer(stack_allocation, sizeof(UIActionHandler) * count);

	for (unsigned int subindex = 0; subindex < count; subindex++) {
		SelectActionData* select_data = (SelectActionData*)stack_allocation;
		CreateSelectActionData(select_data, base_data, type);
		stack_allocation = OffsetPointer(stack_allocation, sizeof(*select_data));
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
static bool DrawBaseSelectionInput(UIDrawer& drawer, BaseDrawData* base_data, ECS_ASSET_TYPE type, DrawBaseReturn* return_value) {
	FilterBar(&drawer, base_data);
	if (drawer.initializer) {
		InitializeBaseData(base_data);
	}

	bool trigger_lazy_evaluation = LazyRetrievalOfPaths(base_data, type);

	const float RELATIVE_FACTOR = 3.5f;

	UIConfigRelativeTransform transform;
	transform.scale.y = RELATIVE_FACTOR;
	return_value->config.AddFlag(transform);

	SelectActionData* select_data = (SelectActionData*)return_value->stack_allocation;
	CreateSelectActionData(select_data, base_data, type);
	select_data->destroy_selection = false;
	return_value->select_data = select_data;
	return_value->stack_allocation = OffsetPointer(return_value->stack_allocation, sizeof(*select_data));

	return_value->converted_filename.InitializeFromBuffer(return_value->stack_allocation, 0, 256);
	return_value->stack_allocation = OffsetPointer(return_value->stack_allocation, sizeof(char) * 256);

	return_value->absolute_path.InitializeFromBuffer(return_value->stack_allocation, 0, 512);
	GetProjectAssetsFolder(base_data->editor_state, return_value->absolute_path);
	return_value->absolute_path.Add(ECS_OS_PATH_SEPARATOR);

	return_value->stack_allocation = OffsetPointer(return_value->stack_allocation, sizeof(wchar_t) * 512);

	UIConfigWindowDependentSize dependent_size;
	dependent_size.scale_factor.x = drawer.GetWindowSizeFactors(ECS_UI_WINDOW_DEPENDENT_HORIZONTAL, drawer.GetSquareScale() * RELATIVE_FACTOR).x;
	return_value->label_config.AddFlag(dependent_size);

	return trigger_lazy_evaluation;
}

static void DrawDeselectButton(UIDrawer& drawer, DrawBaseReturn* base_return, ECS_ASSET_TYPE type) {
	// Do this only if the handle is different from -1
	RegisterAssetTarget register_target(base_return->select_data->reflection_field, base_return->select_data->asset_target_data, type);

	if (register_target.GetHandle(base_return->select_data->target_database) != -1) {
		DeselectActionData deselect_data;
		deselect_data.editor_state = base_return->select_data->editor_state;
		deselect_data.sandbox_index = base_return->select_data->sandbox_index;
		deselect_data.asset_target = register_target;
		deselect_data.asset_type = type;
		deselect_data.callback_action = base_return->select_data->action;
		deselect_data.callback_action_data = base_return->select_data->action_data;
		deselect_data.verify_action = base_return->select_data->verify_action;
		deselect_data.target_database = base_return->select_data->target_database;
		deselect_data.callback_before_handle_update = base_return->select_data->callback_before_handle_update;

		// Draw a none button
		const size_t BASE_CONFIGURATION = UI_CONFIG_MAKE_SQUARE | UI_CONFIG_RELATIVE_TRANSFORM | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_Y;
		drawer.Button(BASE_CONFIGURATION, base_return->config, "Deselect", { DeselectAction, &deselect_data, sizeof(deselect_data) });
	}
}

ECS_INLINE void SetColumnDrawFitSpace(UIDrawer& drawer) {
	drawer.SetDrawMode(ECS_UI_DRAWER_COLUMN_DRAW_FIT_SPACE, 2, 0.01f * drawer.zoom_ptr->y);
}

// The GetTexture must take in as parameters a Stream<wchar_t>* and a ResourceView* and set the one it wants to use
// alongside a CapacityStream<wchar_t>& having the assets_folder and the index. It can reject the current asset
// by not setting the resource view or the filename
template<typename LazyEvaluation, typename GetTexture>
static void DrawFileAndName(UIDrawer& drawer, BaseDrawData* base_data, ECS_ASSET_TYPE type, LazyEvaluation&& lazy_evaluation, GetTexture&& get_texture) {
	size_t _stack_allocation[ECS_KB];

	DrawBaseReturn base_return;
	base_return.stack_allocation = _stack_allocation;
	bool trigger_lazy_evaluation = DrawBaseSelectionInput(drawer, base_data, type, &base_return);
	if (trigger_lazy_evaluation) {
		lazy_evaluation();
	}

	DrawDeselectButton(drawer, &base_return, type);
	SetColumnDrawFitSpace(drawer);

	unsigned int base_size = base_return.absolute_path.size;

	for (size_t index = 0; index < base_data->asset_name_with_path.size; index++) {
		base_return.absolute_path.size = base_size;

		base_return.converted_filename.size = 0;
		ConvertWideCharsToASCII(base_data->asset_name_with_path[index].identifier, base_return.converted_filename);
		Stream<char> filename = PathFilenameBoth(base_return.converted_filename);

		if (base_data->filter.size > 0) {
			if (FindFirstToken(filename, base_data->filter).size == 0) {
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
				UIDrawerMenuState menu_state = GetMenuForSelection(base_data, type, index, stack_allocation);

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
			UIConfigToolTip tool_tip;
			tool_tip.characters = base_return.converted_filename;
			base_return.label_config.AddFlag(tool_tip);
			drawer.TextLabel(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TOOL_TIP, base_return.label_config, filename);
			base_return.label_config.flag_count--;
		}
	}
}

// The GetTexture must take in as parameter a Stream<wchar_t>*
// alongside a CapacityStream<wchar_t>& having the assets_folder and the index
template<typename GetTexture>
static void DrawOnlyName(UIDrawer& drawer, BaseDrawData* base_data, ECS_ASSET_TYPE type, GetTexture&& get_texture) {
	size_t _stack_allocation[ECS_KB];

	DrawBaseReturn base_return;
	base_return.stack_allocation = _stack_allocation;
	DrawBaseSelectionInput(drawer, base_data, type, &base_return);

	DrawDeselectButton(drawer, &base_return, type);
	SetColumnDrawFitSpace(drawer);

	unsigned int base_size = base_return.absolute_path.size;

	for (size_t index = 0; index < base_data->asset_paths.size; index++) {
		base_return.absolute_path.size = base_size;
		base_return.converted_filename.size = 0;
		
		AssetDatabase::ExtractNameFromFile(base_data->asset_paths[index], base_return.converted_filename);
		Stream<char> filename = PathFilenameBoth(base_return.converted_filename);

		if (base_data->filter.size > 0) {
			if (FindFirstToken(base_return.converted_filename, base_data->filter).size == 0) {
				continue;
			}
		}

		Stream<wchar_t> texture = { nullptr, 0 };
		get_texture(base_return.absolute_path, index, &texture);

		base_return.select_data->file = { nullptr, 0 };
		// The name will be deduced from the relative part
		base_return.select_data->name = { nullptr, base_return.converted_filename.size };

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
			{ SelectAction, base_return.select_data, sizeof(*base_return.select_data) + (unsigned int)base_return.converted_filename.size },
			ECS_TOOLS_UI_TEXTURE_FILE_BLANK,
			drawer.color_theme.text
		);
		base_return.config.flag_count--;

		// Draw the name
		UIConfigToolTip tool_tip;
		tool_tip.characters = base_return.converted_filename;
		base_return.label_config.AddFlag(tool_tip);
		drawer.TextLabel(UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TOOL_TIP, base_return.label_config, filename);
		base_return.label_config.flag_count--;
	}
}

#pragma region Mesh Draw

struct MeshDrawData {
	BaseDrawData base_data;
};

void MeshDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	MeshDrawData* data = (MeshDrawData*)window_data;

	DrawFileAndName(drawer, &data->base_data, ECS_ASSET_MESH, []() {}, 
		[data](CapacityStream<wchar_t>& absolute_path, unsigned int index, Stream<wchar_t>* texture, ResourceView* resource_view) {
			absolute_path.AddStreamSafe(data->base_data.asset_name_with_path[index].identifier);
			// Replace relative slashes with absolute ones
			ReplaceCharacter(absolute_path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
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

void TextureDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	TextureDrawData* data = (TextureDrawData*)window_data;

	DrawFileAndName(drawer, &data->base_data, ECS_ASSET_TEXTURE, []() {},
		[data](CapacityStream<wchar_t>& absolute_path, unsigned int index, Stream<wchar_t>* texture, ResourceView* resource_view) {
			absolute_path.AddStreamSafe(data->base_data.asset_name_with_path[index].identifier);
			// Replace relative slashes with absolute ones
			ReplaceCharacter(absolute_path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
			*texture = absolute_path;
	});
}

#pragma endregion

#pragma region GPU Sampler Draw

struct GPUSamplerDrawData {
	BaseDrawData base_data;
};

void GPUSamplerDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
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

void ShaderDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	ShaderDrawData* data = (ShaderDrawData*)window_data;

	DrawFileAndName(drawer, &data->base_data, ECS_ASSET_SHADER, 
		[&]() {
			// Resize the shader metadata
			size_t path_count = data->base_data.asset_name_with_path.size;
			data->shader_metadata = (ShaderMetadata*)data->base_data.resizable_allocator.Allocate(sizeof(ShaderMetadata) * path_count);
			for (size_t index = 0; index < path_count; index++) {
				Stream<char> current_name = data->base_data.asset_name_with_path[index].value[0];
				bool success = data->base_data.editor_state->asset_database->ReadShaderFile(current_name, data->shader_metadata + index);
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

void MaterialDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
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

void MiscDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
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
	AssetDatabase* database;
	CapacityStream<char> selection;
	unsigned int sandbox_index;
	bool callback_is_ptr;
	bool registration_callback_is_ptr;
	Action callback;
	Action registration_callback;
	bool callback_disable_selection_unregistering;
	bool callback_verify;
	// Used for deselection
	bool callback_before_handle_update;
	bool callback_is_single_threaded;
	// Cache the reflection field here, such that we can pass it to the target asset pointer functions
	const Reflection::ReflectionField* reflection_field;

	union {
		// Embed the data directly into it
		size_t callback_data[8];
		void* callback_data_ptr;
	};
	union {
		// Embed the data directly into it
		size_t registration_callback_data[8];
		void* registration_callback_data_ptr;
	};
};

static void OverrideAssetHandle(
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

	base_window_data->editor_state = base_data->editor_state;
	base_window_data->reflection_field = base_data->reflection_field;
	base_window_data->asset_target_field = field_data;
	base_window_data->sandbox_index = base_data->sandbox_index;
	base_window_data->callback_action = base_data->callback;
	base_window_data->callback_action_data = base_data->callback_is_ptr ? base_data->callback_data_ptr : base_data->callback_data;
	base_window_data->callback_verify = base_data->callback_verify;
	base_window_data->callback_before_handle_update = base_data->callback_before_handle_update;
	base_window_data->callback_disable_selection_unregistering = base_data->callback_disable_selection_unregistering;
	base_window_data->registration_callback_action = base_data->registration_callback;
	base_window_data->registration_callback_action_data = base_data->registration_callback_is_ptr ? base_data->registration_callback_data_ptr 
		: base_data->registration_callback_data;
	base_window_data->callback_is_single_threaded = base_data->callback_is_single_threaded;

	base_window_data->target_database = base_data->database;

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

	RegisterAssetTarget asset_target(base_window_data->reflection_field, field_data, type);
	unsigned int handle_value = asset_target.GetHandle(base_window_data->target_database);
	if (handle_value != -1) {
		Stream<char> name = base_data->database->GetAssetName(handle_value, type);
		Stream<wchar_t> file = base_data->database->GetAssetPath(handle_value, type);

		if (file.size > 0) {
			Stream<wchar_t> filename = PathFilenameBoth(file);
			if (filename.size != file.size) {
				configuration |= UI_CONFIG_SELECTION_INPUT_OVERRIDE_HOVERABLE;
				FormatString(hoverable_string, "{#} ({#})", file, name);
				override_hoverable.text = hoverable_string;
				override_hoverable.stable = false;
				config->AddFlag(override_hoverable);
			}

			// Name and file. Put the file first and the name in paranthesis
			FormatString(composite_string, "{#} ({#})", filename, name);
		}
		else {
			composite_string = name;
		}
	}
	base_data->selection.CopyOther(composite_string);

	struct DoubleClickActionData {
		EditorState* editor_state;
		unsigned int handle;
		ECS_ASSET_TYPE asset_type;
		const AssetDatabase* target_database;
	};

	auto double_click_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		DoubleClickActionData* data = (DoubleClickActionData*)_data;
		Stream<char> window_name = system->GetWindowName(window_index);
		unsigned int inspector_index = GetInspectorIndex(window_name);
		if (inspector_index != -1) {
			if (data->target_database == data->editor_state->asset_database) {
				ChangeInspectorToAsset(data->editor_state, data->handle, data->asset_type, inspector_index);
			}
			else {
				// Get the metadata
				const void* metadata = data->target_database->GetAssetConst(data->handle, data->asset_type);
				ChangeInspectorToAsset(data->editor_state, metadata, data->asset_type, inspector_index);
			}
		}
	};

	DoubleClickActionData double_click_data;
	double_click_data.asset_type = type;
	double_click_data.handle = handle_value;
	double_click_data.editor_state = base_data->editor_state;
	double_click_data.target_database = base_data->database;

	UIConfigSelectionInputLabelClickable selection_clickable;
	selection_clickable.double_click_action = true;
	selection_clickable.double_click_duration_between_clicks = 200;
	selection_clickable.handler = { double_click_action, &double_click_data, sizeof(double_click_data), ECS_UI_DRAW_SYSTEM };

	config->AddFlag(selection_clickable);

	// Draw the field as a selection input
	drawer->SelectionInput(
		configuration | UI_CONFIG_SELECTION_INPUT_LABEL_CLICKABLE, 
		*config, 
		field_name, 
		&base_data->selection, 
		ECS_TOOLS_UI_TEXTURE_FOLDER, 
		&window_descriptor
	);
	if (configuration & UI_CONFIG_SELECTION_INPUT_OVERRIDE_HOVERABLE) {
		config->flag_count--;
	}
	// For the selection_clickable
	config->flag_count--;
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

	data->selection.buffer = (char*)Allocate(allocator, sizeof(char) * capacity, alignof(char));
	data->selection.size = 0;
	data->selection.capacity = capacity;
	data->database = global_data->editor_state->asset_database;

	data->editor_state = global_data->editor_state;
	data->callback_verify = false;
	data->callback_before_handle_update = true;
}

#pragma endregion

#pragma region Deallocators

ECS_UI_REFLECTION_INSTANCE_DEALLOCATE_OVERRIDE(AssetDeallocate) {
	// Every asset has a OverrideBaseData as the first field
	OverrideBaseData* data = (OverrideBaseData*)_data;
	if (data->selection.capacity > 0) {
		data->selection.Deallocate(allocator);
		data->selection = { nullptr, 0, 0 };
	}
}

#pragma endregion

#pragma region Modifiers

void AssetOverrideSetSandboxIndex(const UIReflectionInstanceModifyOverrideData* function_data, void* user_data) {
	OverrideBaseData* data = (OverrideBaseData*)function_data->data;
	AssetOverrideSetSandboxIndexData* modify_data = (AssetOverrideSetSandboxIndexData*)user_data;
	data->sandbox_index = modify_data->sandbox_index;
}

void AssetOverrideBindCallback(const UIReflectionInstanceModifyOverrideData* function_data, void* user_data) {
	OverrideBaseData* data = (OverrideBaseData*)function_data->data;
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

	if (modify_data->registration_handler.action != nullptr) {
		data->registration_callback = modify_data->registration_handler.action;
		ECS_ASSERT(modify_data->registration_handler.data_size <= sizeof(data->registration_callback_data));
		if (modify_data->registration_handler.data_size == 0) {
			data->registration_callback_data_ptr = modify_data->registration_handler.data;
			data->registration_callback_is_ptr = true;
		}
		else {
			memcpy(data->registration_callback_data, modify_data->registration_handler.data, modify_data->registration_handler.data_size);
			data->registration_callback_is_ptr = false;
		}
	}

	data->callback_verify = modify_data->verify_handler;
	data->callback_before_handle_update = modify_data->callback_before_handle_update;
	data->callback_disable_selection_unregistering = modify_data->disable_selection_registering;
	data->callback_is_single_threaded = modify_data->callback_is_single_threaded;
}

void AssetOverrideBindNewDatabase(const UIReflectionInstanceModifyOverrideData* function_data, void* user_data) {
	OverrideBaseData* data = (OverrideBaseData*)function_data->data;
	AssetOverrideBindNewDatabaseData* modify_data = (AssetOverrideBindNewDatabaseData*)user_data;

	data->database = modify_data->database;
}

void AssetOverrideSetAll(const UIReflectionInstanceModifyOverrideData* function_data, void* user_data) {
	OverrideBaseData* data = (OverrideBaseData*)function_data->data;
	AssetOverrideSetAllData* modify_data = (AssetOverrideSetAllData*)user_data;

	AssetOverrideSetSandboxIndex(function_data, &modify_data->set_index);
	AssetOverrideBindCallback(function_data, &modify_data->callback);
	if (modify_data->new_database.database != nullptr) {
		AssetOverrideBindNewDatabase(function_data, &modify_data->new_database);
	}

	const UIReflectionType* ui_type = function_data->drawer->GetType(function_data->instance->type_name);
	// Use the name of the ui_type, since it might be mangled
	const Reflection::ReflectionType* reflection_type = function_data->drawer->reflection->GetType(ui_type->name);
	data->reflection_field = reflection_type->fields.buffer + ui_type->fields[function_data->field].reflection_type_index;
}

#pragma endregion

void SetUIOverrideBase(EditorState* editor_state, UIReflectionFieldOverride* override, AllocatorPolymorphic allocator) {
	override->initialize_function = AssetInitialize;
	override->deallocate_function = AssetDeallocate;
	GlobalOverrideData* override_data = (GlobalOverrideData*)Allocate(allocator, sizeof(GlobalOverrideData));
	override_data->editor_state = editor_state;
	override->global_data = override_data;
	override->global_data_size = sizeof(GlobalOverrideData);
	override->tag = {};
}

void GetEntityComponentUIOverrides(EditorState* editor_state, UIReflectionFieldOverride* overrides, AllocatorPolymorphic allocator) {
	size_t count = AssetUIOverrideCount();
	for (size_t index = 0; index < count; index++) {
		SetUIOverrideBase(editor_state, overrides + index, allocator);
	}

	unsigned int current_index = 0;
	overrides[current_index].draw_function = OverrideMeshHandle;
	overrides[current_index].name = STRING(CoalescedMesh);
	overrides[current_index].match_function = [](const Reflection::ReflectionField& field, void* global_data) -> bool {
		return FindAssetTargetField(field.definition).type == ECS_ASSET_MESH;
	};
	overrides[current_index].draw_data_size = sizeof(OverrideMeshData);
	current_index++;

	overrides[current_index].draw_function = OverrideTextureHandle;
	overrides[current_index].name = STRING(ResourceView);
	overrides[current_index].match_function = [](const Reflection::ReflectionField& field, void* global_data) -> bool {
		return FindAssetTargetField(field.definition).type == ECS_ASSET_TEXTURE;
	};
	overrides[current_index].draw_data_size = sizeof(OverrideTextureData);
	current_index++;

	overrides[current_index].draw_function = OverrideGPUSamplerHandle;
	overrides[current_index].name = STRING(SamplerState);
	overrides[current_index].match_function = [](const Reflection::ReflectionField& field, void* global_data) -> bool {
		return FindAssetTargetField(field.definition).type == ECS_ASSET_GPU_SAMPLER;
	};
	overrides[current_index].draw_data_size = sizeof(OverrideGPUSamplerData);
	current_index++;

	overrides[current_index].draw_function = OverrideShaderHandle<ECS_SHADER_VERTEX>;
	overrides[current_index].name = STRING(VertexShader);
	overrides[current_index].match_function = [](const Reflection::ReflectionField& field, void* global_data) -> bool {
		AssetTypeEx asset_type = FindAssetTargetField(field.definition);
		return asset_type.type == ECS_ASSET_SHADER && asset_type.shader_type == ECS_SHADER_VERTEX;
	};
	overrides[current_index].draw_data_size = sizeof(OverrideShaderData);
	current_index++;

	overrides[current_index].draw_function = OverrideShaderHandle<ECS_SHADER_PIXEL>;
	overrides[current_index].name = STRING(PixelShader);
	overrides[current_index].match_function = [](const Reflection::ReflectionField& field, void* global_data) -> bool {
		AssetTypeEx asset_type = FindAssetTargetField(field.definition);
		return asset_type.type == ECS_ASSET_SHADER && asset_type.shader_type == ECS_SHADER_PIXEL;
	};
	overrides[current_index].draw_data_size = sizeof(OverrideShaderData);
	current_index++;

	overrides[current_index].draw_function = OverrideShaderHandle<ECS_SHADER_COMPUTE>;
	overrides[current_index].name = STRING(ComputeShader);
	overrides[current_index].match_function = [](const Reflection::ReflectionField& field, void* global_data) -> bool {
		AssetTypeEx asset_type = FindAssetTargetField(field.definition);
		return asset_type.type == ECS_ASSET_SHADER && asset_type.shader_type == ECS_SHADER_COMPUTE;
	};
	overrides[current_index].draw_data_size = sizeof(OverrideShaderData);
	current_index++;

	overrides[current_index].draw_function = OverrideMaterialHandle;
	overrides[current_index].name = STRING(Material);
	overrides[current_index].match_function = [](const Reflection::ReflectionField& field, void* global_data) -> bool {
		return FindAssetTargetField(field.definition).type == ECS_ASSET_MATERIAL;
	};
	overrides[current_index].draw_data_size = sizeof(OverrideMaterialData);
	current_index++;

	overrides[current_index].draw_function = OverrideMiscHandle;
	overrides[current_index].name = STRING(MiscAssetData);
	overrides[current_index].match_function = [](const Reflection::ReflectionField& field, void* global_data) -> bool {
		return FindAssetTargetField(field.definition).type == ECS_ASSET_MISC;
	};
	overrides[current_index].draw_data_size = sizeof(OverrideMiscData);
	current_index++;
}

void AssetOverrideBindInstanceOverrides(
	UIReflectionDrawer* drawer, 
	UIReflectionInstance* instance, 
	unsigned int sandbox_index, 
	UIActionHandler modify_action_handler,
	const AssetOverrideBindInstanceOverridesOptions& options
)
{
	AssetOverrideSetAllData set_data;
	set_data.set_index.sandbox_index = sandbox_index;
	set_data.callback.handler = modify_action_handler;
	set_data.callback.disable_selection_registering = options.disable_selection_unregistering;
	set_data.callback.registration_handler = options.registration_modify_action_handler;
	set_data.callback.callback_is_single_threaded = options.modify_handler_is_single_threaded;

	size_t count = AssetUIOverrideCount();
	for (size_t index = 0; index < count; index++) {
		drawer->BindInstanceFieldOverride(instance, ECS_ASSET_TARGET_FIELD_NAMES[index].name, AssetOverrideSetAll, &set_data);
	}
}