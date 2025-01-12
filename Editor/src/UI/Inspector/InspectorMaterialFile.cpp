#include "editorpch.h"
#include "InspectorMaterialFile.h"
#include "ECSEngineCBufferTags.h"

#include "InspectorMiscFile.h"
#include "../Inspector.h"
#include "InspectorUtilities.h"
#include "InspectorAssetUtilities.h"
#include "../../Editor/EditorState.h"
#include "../../Editor/EditorPalette.h"
#include "../../Assets/EditorSandboxAssets.h"

#include "../AssetSettingHelper.h"
#include "../../Assets/AssetManagement.h"
#include "../../Assets/AssetExtensions.h"
#include "../../Assets/AssetBuiltins.h"
#include "../AssetOverrides.h"
#include "../AssetIcons.h"
#include "../../Project/ProjectFolders.h"

using namespace ECSEngine;
ECS_TOOLS;

#define MAX_TEXTURE_SLOT_COUNT 128
#define MAX_SAMPLER_SLOT_COUNT 16
#define MAX_BUFFER_SLOT_COUNT 15
#define COMBO_MAX_DISPLAY_COUNT 16

#define SHADER_LAZY_RELOAD_MS 300

#define SHADER_ALLOCATOR_CAPACITY ECS_KB * 96
#define TEMPORARY_DATABASE_ALLOCATOR_CAPACITY ECS_KB * 8

/*
	The module reflection contains the overrides, so use it for the override draw
*/

#define VERTEX_TAG STRING(ECS_VERTEX_SHADER_HANDLE)
#define PIXEL_TAG STRING(ECS_PIXEL_SHADER_HANDLE)
#define TEXTURE_TAG STRING(ECS_TEXTURE_HANDLE)
#define SAMPLER_TAG STRING(ECS_GPU_SAMPLER_HANDLE)

#define REFLECTED_SETTINGS_NAME_PADDING 0.22f

// #define GENERAL_ARRAYS

enum SUCCESS_ORDER : unsigned char {
	SUCCESS_CBUFFER,
	SUCCESS_TEXTURE,
	SUCCESS_SAMPLER,
	SUCCESS_COUNT
};

enum ORDER : unsigned char {
	VERTEX_ORDER,
	PIXEL_ORDER,
	ORDER_COUNT
};

ECS_INLINE ECS_MATERIAL_SHADER GetMaterialShader(ORDER order) {
	return order == VERTEX_ORDER ? ECS_MATERIAL_SHADER_VERTEX : ECS_MATERIAL_SHADER_PIXEL;
}

struct InspectorDrawMaterialFileData {
	ECS_INLINE AllocatorPolymorphic MaterialAssetAlllocator() const {
		return editor_state->EditorAllocator();
	}

	MaterialAsset material_asset;
	Stream<wchar_t> path;
	EditorState* editor_state;
	Reflection::ReflectionManager reflection_manager;
	AssetDatabase temporary_database;

	bool success[ORDER_COUNT][SUCCESS_COUNT];
	unsigned char shader_cbuffer_allocator_index[ORDER_COUNT];
	bool shader_collapsing_header_state[ORDER_COUNT];
	Timer timer[ORDER_COUNT];
	void* shader_override_data[ORDER_COUNT];
	size_t shader_stamp[ORDER_COUNT];
	size_t metadata_shader_stamp[ORDER_COUNT];

	// This is kept in sync with material_asset.buffers
	Stream<Reflection::ReflectionType> cbuffers[ORDER_COUNT];
	Stream<ShaderReflectedTexture> textures[ORDER_COUNT];
	void** texture_override_data[ORDER_COUNT];
	Stream<ShaderReflectedSampler> samplers[ORDER_COUNT];
	void** sampler_override_data[ORDER_COUNT];

	// Used to keep the buffer data for the shaders.
	// There is a pair of them such that they can be used in
	// successive fashion when the shader is changed and completely
	// deallocate the previous manager
	ResizableLinearAllocator shader_cbuffer_allocator[ORDER_COUNT][2];
	MemoryManager database_allocator;

	EDITOR_MATERIAL_BUILTIN builtin;
	CapacityStream<char> construct_pbr_from_prefix_characters;
};

// ------------------------------------------------------------------------------------------------------------

ECS_INLINE unsigned int GetShaderHandle(const InspectorDrawMaterialFileData* data, ORDER order) {
	return order == VERTEX_ORDER ? data->material_asset.vertex_shader_handle : data->material_asset.pixel_shader_handle;
}

// ------------------------------------------------------------------------------------------------------------

static void GetUITypeNameForCBuffer(
	const InspectorDrawMaterialFileData* data, 
	unsigned int inspector_index, 
	unsigned int shader_handle,
	const Reflection::ReflectionType* type, 
	CapacityStream<char>& name
) {
	// Concatenate the type name with the shader name and the inspector index
	name.AddStream(type->name);
	Stream<wchar_t> shader_path = data->temporary_database.GetAssetPath(shader_handle, ECS_ASSET_SHADER);
	ConvertWideCharsToASCII(shader_path, name);
	ConvertIntToChars(name, inspector_index);
}

// ------------------------------------------------------------------------------------------------------------

// It will also increment the current index
static void DeallocateCurrentShaderAllocator(InspectorDrawMaterialFileData* draw_data, ORDER order) {
	unsigned char current_index = draw_data->shader_cbuffer_allocator_index[order];
	draw_data->shader_cbuffer_allocator[order][current_index].Clear();
	if (current_index == 0) {
		draw_data->shader_cbuffer_allocator_index[order] = 1;
	}
	else {
		draw_data->shader_cbuffer_allocator_index[order] = 0;
	}
}

// ------------------------------------------------------------------------------------------------------------

ECS_INLINE static AllocatorPolymorphic GetCurrentShaderAllocator(InspectorDrawMaterialFileData* draw_data, ORDER order) {
	return draw_data->shader_cbuffer_allocator[order] + draw_data->shader_cbuffer_allocator_index[order];
}

// ------------------------------------------------------------------------------------------------------------

static void DeallocateCBuffers(
	InspectorDrawMaterialFileData* draw_data, 
	unsigned int inspector_index, 
	ORDER order
) {
	Stream<Reflection::ReflectionType> cbuffers = draw_data->cbuffers[order];
	unsigned int shader_handle = GetShaderHandle(draw_data, order);

	if (cbuffers.size > 0) {
		for (size_t index = 0; index < cbuffers.size; index++) {
			// Destroy the UI type as well (it will destroy the instance associated with it as well)
			ECS_STACK_CAPACITY_STREAM(char, ui_type_name, 512);
			GetUITypeNameForCBuffer(draw_data, inspector_index, shader_handle, cbuffers.buffer + index, ui_type_name);
			draw_data->editor_state->ui_reflection->DestroyType(ui_type_name);
		}
		draw_data->cbuffers[order].size = 0;

		draw_data->material_asset.ResizeBufferNewValue(0, GetMaterialShader(order), draw_data->MaterialAssetAlllocator());
	}
}

// ------------------------------------------------------------------------------------------------------------

static void DeallocateTextures(InspectorDrawMaterialFileData* draw_data, ORDER order) {
	if (draw_data->textures[order].size > 0) {
		for (size_t index = 0; index < draw_data->textures[order].size; index++) {
			draw_data->editor_state->module_reflection->DeallocateFieldOverride(TEXTURE_TAG, draw_data->texture_override_data[order][index]);
		}
		draw_data->textures[order].size = 0;

		draw_data->material_asset.ResizeTexturesNewValue(0, GetMaterialShader(order), draw_data->MaterialAssetAlllocator());
	}
}

// ------------------------------------------------------------------------------------------------------------

static void DeallocateSamplers(InspectorDrawMaterialFileData* draw_data, ORDER order) {
	if (draw_data->samplers[order].size > 0) {
		for (size_t index = 0; index < draw_data->samplers[order].size; index++) {
			draw_data->editor_state->module_reflection->DeallocateFieldOverride(SAMPLER_TAG, draw_data->sampler_override_data[order][index]);
		}
		draw_data->samplers[order].size = 0;

		draw_data->material_asset.ResizeSamplersNewValue(0, GetMaterialShader(order), draw_data->MaterialAssetAlllocator());
	}
}

// ------------------------------------------------------------------------------------------------------------

static void BaseModifyCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	InspectorDrawMaterialFileData* data = (InspectorDrawMaterialFileData*)_data;

	AssetSettingsHelperChangedNoFileActionData changed_data;
	changed_data.editor_state = data->editor_state;
	changed_data.asset = &data->material_asset;
	changed_data.asset_type = ECS_ASSET_MATERIAL;
	changed_data.target_database = &data->temporary_database;

	action_data->data = &changed_data;
	AssetSettingsHelperChangedNoFileAction(action_data);
}

// ------------------------------------------------------------------------------------------------------------

// When new cbuffer definitions are found, we need to retrieve the old data into the new format
// The new_reflected_buffers is needed for the slot index. The new_cbuffers are going to be copied individually
// and a new buffer allocated for the stream. Also the UI types and instances will be created
// It will also deallocate the old cbuffers
static void RegisterNewCBuffers(
	InspectorDrawMaterialFileData* draw_data,
	unsigned int inspector_index,
	ORDER order,
	Stream<Reflection::ReflectionType> new_cbuffers,
	ShaderReflectedBuffer* new_reflected_buffers,
	Stream<ShaderReflectionBufferMatrixField>* matrix_types
) {
	EditorState* editor_state = draw_data->editor_state;

	ECS_STACK_CAPACITY_STREAM(MaterialAssetBuffer, new_buffers, MAX_BUFFER_SLOT_COUNT);
	ECS_ASSERT(new_cbuffers.size <= MAX_BUFFER_SLOT_COUNT);

	Stream<Reflection::ReflectionType> old_cbuffers = draw_data->cbuffers[order];

	AllocatorPolymorphic current_allocator = GetCurrentShaderAllocator(draw_data, order);

	ECS_MATERIAL_SHADER material_shader_type = GetMaterialShader(order);

	// Start by making a new allocation for every new buffer
	for (size_t index = 0; index < new_cbuffers.size; index++) {
		size_t type_byte_size = Reflection::GetReflectionTypeByteSize(new_cbuffers.buffer + index);
		new_buffers[index].data.buffer = Allocate(current_allocator, type_byte_size);
		new_buffers[index].data.size = type_byte_size;
		new_buffers[index].slot = new_reflected_buffers[index].register_index;
		new_buffers[index].dynamic = true;
		new_buffers[index].name = StringCopy(current_allocator, new_cbuffers[index].name);

		// Set the default values for that type
		draw_data->material_asset.reflection_manager->SetInstanceDefaultData(new_cbuffers.buffer + index, new_buffers[index].data.buffer);
	}
	new_buffers.size = new_cbuffers.size;

	unsigned int shader_handle = GetShaderHandle(draw_data, order);
	for (size_t index = 0; index < old_cbuffers.size; index++) {
		// Check to see if it still exists
		size_t subindex = 0;
		for (; subindex < new_cbuffers.size; subindex++) {
			if (new_cbuffers[subindex].name == old_cbuffers[index].name) {
				break;
			}
		}
		
		// Destroy the UI type - it will also destroy the instance
		ECS_STACK_CAPACITY_STREAM(char, ui_type_name, 512);
		GetUITypeNameForCBuffer(
			draw_data,
			inspector_index,
			shader_handle,
			old_cbuffers.buffer + index,
			ui_type_name
		);
		editor_state->ui_reflection->DestroyType(ui_type_name);

		if (subindex < new_cbuffers.size) {
			const void* buffer_data = draw_data->material_asset.buffers[material_shader_type][index].data.buffer;
			// We have a match
			const Reflection::ReflectionManager* reflection_manager = editor_state->ui_reflection->reflection;
			if (Reflection::CompareReflectionTypes(reflection_manager, reflection_manager, old_cbuffers.buffer + index, new_cbuffers.buffer + subindex)) {
				memcpy(new_buffers[subindex].data.buffer, buffer_data, new_buffers[subindex].data.size);
			}
			else {
				// It has changed. We need to convert the old type to the new type
				Reflection::CopyReflectionDataOptions copy_options;
				copy_options.allocator = current_allocator;
				copy_options.always_allocate_for_buffers = true;
				Reflection::CopyReflectionTypeToNewVersion(
					reflection_manager,
					reflection_manager,
					old_cbuffers.buffer + index,
					new_cbuffers.buffer + subindex,
					buffer_data,
					new_buffers[subindex].data.buffer,
					&copy_options
				);
			}
		}
	}

	unsigned int counts[ECS_MATERIAL_SHADER_COUNT * 3];
	draw_data->material_asset.WriteCounts(true, true, true, counts);
	counts[ECS_MATERIAL_SHADER_COUNT * 2 + material_shader_type] = new_cbuffers.size;
	draw_data->material_asset.Resize(counts, draw_data->MaterialAssetAlllocator());
	draw_data->material_asset.buffers[material_shader_type].CopyOther(new_buffers);

	draw_data->cbuffers[order].InitializeAndCopy(current_allocator, new_cbuffers);
	for (size_t index = 0; index < new_cbuffers.size; index++) {
		ShaderConstantBufferReflectionTypeCopy(new_cbuffers.buffer + index, draw_data->cbuffers[order].buffer + index, current_allocator);
		// Create the UI type and the UI instance
		ECS_STACK_CAPACITY_STREAM(char, ui_type_name, 512);
		GetUITypeNameForCBuffer(
			draw_data,
			inspector_index,
			order == VERTEX_ORDER ? draw_data->material_asset.vertex_shader_handle : draw_data->material_asset.pixel_shader_handle,
			draw_data->cbuffers[order].buffer + index,
			ui_type_name
		);

		ECS_STACK_CAPACITY_STREAM(unsigned int, ignore_type_fields, 512);
		GetConstantBufferInjectTagFieldsFromType(new_cbuffers.buffer + index, &ignore_type_fields, false);

		UIReflectionDrawerCreateTypeOptions create_options;
		create_options.identifier_name = ui_type_name;
		create_options.ignore_fields = ignore_type_fields;
		UIReflectionType* ui_type = editor_state->ui_reflection->CreateType(draw_data->cbuffers[order].buffer + index, &create_options);
		// Add matrix types if any - if they are not skipped by the inject fields
		for (size_t subindex = 0; subindex < matrix_types[index].size; subindex++) {
			bool is_injected = SearchBytes(ignore_type_fields.ToStream(), matrix_types[index][subindex].position.x) != -1;
			if (!is_injected) {
				UIReflectionTypeFieldGrouping grouping;
				grouping.field_index = matrix_types[index][subindex].position.x;
				grouping.range = matrix_types[index][subindex].position.y;
				grouping.name = matrix_types[index][subindex].name;
				grouping.per_element_name = "row";
				editor_state->ui_reflection->AddTypeGrouping(ui_type, grouping);
			}
		}

		UIReflectionInstance* instance = editor_state->ui_reflection->CreateInstance(ui_type_name, ui_type, ui_type_name);
		editor_state->ui_reflection->BindInstancePtrs(instance, new_buffers[index].data.buffer, draw_data->cbuffers[order].buffer + index);
		draw_data->material_asset.buffers[material_shader_type][index].reflection_type = draw_data->cbuffers[order].buffer + index;
	}
}

// ------------------------------------------------------------------------------------------------------------

static void RegisterNewTextures(
	InspectorDrawMaterialFileData* draw_data,
	ORDER order,
	Stream<ShaderReflectedTexture> new_textures
) {
	AllocatorPolymorphic current_allocator = GetCurrentShaderAllocator(draw_data, order);

	ECS_MATERIAL_SHADER material_shader_type = GetMaterialShader(order);
	Stream<MaterialAssetResource> old_textures = draw_data->material_asset.textures[material_shader_type];

	ECS_STACK_CAPACITY_STREAM(MaterialAssetResource, textures, MAX_TEXTURE_SLOT_COUNT);
	ECS_ASSERT(new_textures.size <= MAX_TEXTURE_SLOT_COUNT);

	void** override_data = nullptr;
	if (new_textures.size > 0) {
		override_data = (void**)Allocate(current_allocator, sizeof(void*) * new_textures.size);
	}

	// Preinitialize the array
	for (size_t index = 0; index < new_textures.size; index++) {
		textures[index].metadata_handle = -1;
		textures[index].slot = new_textures[index].register_index;
		textures[index].name = StringCopy(current_allocator, new_textures[index].name);
		override_data[index] = nullptr;
	}
	textures.size = new_textures.size;

	// Deallocate the previous field overrides
	for (size_t index = 0; index < draw_data->textures[order].size; index++) {
		draw_data->editor_state->module_reflection->DeallocateFieldOverride(TEXTURE_TAG, draw_data->texture_override_data[order][index]);
	}

	// See which textures have remained the same
	for (size_t index = 0; index < new_textures.size; index++) {
		size_t subindex = 0;
		for (; subindex < old_textures.size; subindex++) {
			if (old_textures[subindex].name == new_textures[index].name) {
				break;
			}
		}
		if (subindex < old_textures.size) {
			// We have a match
			// Retrieve the metadata handle
			textures[index].metadata_handle = draw_data->material_asset.textures[material_shader_type][subindex].metadata_handle;
		}
		override_data[index] = draw_data->editor_state->module_reflection->InitializeFieldOverride(TEXTURE_TAG, new_textures[index].name);
		AssetOverrideSetAllData set_all_data;
		set_all_data.callback.handler = { BaseModifyCallback, draw_data, 0 };
		set_all_data.callback.callback_before_handle_update = false;
		set_all_data.set_index.sandbox_index = -1;
		set_all_data.new_database.database = &draw_data->temporary_database;
		draw_data->editor_state->module_reflection->BindInstanceFieldOverride(override_data[index], TEXTURE_TAG, AssetOverrideSetAll, &set_all_data);
	}
	
	draw_data->textures[order].InitializeAndCopy(current_allocator, new_textures);

	unsigned int counts[ECS_MATERIAL_SHADER_COUNT * 3];
	draw_data->material_asset.WriteCounts(true, true, true, counts);
	counts[material_shader_type] = new_textures.size;
	draw_data->material_asset.Resize(counts, draw_data->MaterialAssetAlllocator());
	draw_data->material_asset.textures[material_shader_type].CopyOther(textures);
	draw_data->texture_override_data[order] = override_data;

	// Update the name to reflect the one stored in the material_asset.textures
	for (size_t index = 0; index < new_textures.size; index++) {
		draw_data->textures[order][index].name = draw_data->material_asset.textures[material_shader_type][index].name;
	}
}

// ------------------------------------------------------------------------------------------------------------

static void RegisterNewSamplers(
	InspectorDrawMaterialFileData* draw_data,
	ORDER order,
	Stream<ShaderReflectedSampler> new_samplers
) {
	AllocatorPolymorphic current_allocator = GetCurrentShaderAllocator(draw_data, order);

	ECS_MATERIAL_SHADER material_shader_type = GetMaterialShader(order);
	Stream<MaterialAssetResource> old_samplers = draw_data->material_asset.samplers[material_shader_type];

	ECS_STACK_CAPACITY_STREAM(MaterialAssetResource, samplers, MAX_SAMPLER_SLOT_COUNT);
	ECS_ASSERT(new_samplers.size <= MAX_SAMPLER_SLOT_COUNT);

	void** new_override_data = nullptr;
	if (new_samplers.size > 0) {
		new_override_data = (void**)Allocate(current_allocator, sizeof(void*) * new_samplers.size);
	}

	// Preinitialize the array
	for (size_t index = 0; index < new_samplers.size; index++) {
		samplers[index].metadata_handle = -1;
		samplers[index].slot = new_samplers[index].register_index;
		samplers[index].name = StringCopy(current_allocator, new_samplers[index].name);
		new_override_data[index] = nullptr;
	}
	samplers.size = new_samplers.size;

	// Deallocate the previous field overrides
	for (size_t index = 0; index < draw_data->samplers[order].size; index++) {
		draw_data->editor_state->module_reflection->DeallocateFieldOverride(SAMPLER_TAG, draw_data->sampler_override_data[order][index]);
	}

	// See which textures have remained the same
	for (size_t index = 0; index < new_samplers.size; index++) {
		size_t subindex = 0;
		for (; subindex < old_samplers.size; subindex++) {
			if (old_samplers[subindex].name == new_samplers[index].name) {
				break;
			}
		}
		if (subindex < old_samplers.size) {
			// We have a match
			// Retrieve the metadata handle and the override data
			samplers[index].metadata_handle = draw_data->material_asset.samplers[material_shader_type][subindex].metadata_handle;
		}
		// Must initialize the override data
		new_override_data[index] = draw_data->editor_state->module_reflection->InitializeFieldOverride(SAMPLER_TAG, new_samplers[index].name);
		AssetOverrideSetAllData set_all_data;
		set_all_data.callback.handler = { BaseModifyCallback, draw_data, 0 };
		set_all_data.callback.callback_before_handle_update = false;
		set_all_data.set_index.sandbox_index = -1;
		set_all_data.new_database.database = &draw_data->temporary_database;
		draw_data->editor_state->module_reflection->BindInstanceFieldOverride(new_override_data[index], SAMPLER_TAG, AssetOverrideSetAll, &set_all_data);
	}

	draw_data->samplers[order].InitializeAndCopy(current_allocator, new_samplers);

	unsigned int counts[ECS_MATERIAL_SHADER_COUNT * 3];
	draw_data->material_asset.WriteCounts(true, true, true, counts);
	counts[ECS_MATERIAL_SHADER_COUNT + material_shader_type] = new_samplers.size;
	draw_data->material_asset.Resize(counts, draw_data->MaterialAssetAlllocator());
	draw_data->material_asset.samplers[material_shader_type].CopyOther(samplers);
	draw_data->sampler_override_data[order] = new_override_data;

	// Update the name to reflect the one stored in the material_asset.samplers
	for (size_t index = 0; index < new_samplers.size; index++) {
		draw_data->samplers[order][index].name = draw_data->material_asset.samplers[material_shader_type][index].name;
	}
}

// ------------------------------------------------------------------------------------------------------------

static void InitializeReflectionManager(InspectorDrawMaterialFileData* data) {
	data->reflection_manager = Reflection::ReflectionManager(data->editor_state->EditorAllocator(), 64, 0);
}

static void RecreateReflectionManagerForShaders(InspectorDrawMaterialFileData* data) {
	data->reflection_manager.ClearFromAllocator(true, false);
	InitializeReflectionManager(data);

	for (size_t index = 0; index < data->cbuffers[VERTEX_ORDER].size; index++) {
		data->reflection_manager.type_definitions.Insert(data->cbuffers[VERTEX_ORDER][index], data->cbuffers[VERTEX_ORDER][index].name);
	}

	// For the rest of the shaders test before inserting to see that the types match
	for (size_t order = PIXEL_ORDER; order < ORDER_COUNT; order++) {
		for (size_t index = 0; index < data->cbuffers[order].size; index++) {
			const Reflection::ReflectionType* reflection_type = &data->cbuffers[order][index];
			const Reflection::ReflectionType* other_type = data->reflection_manager.type_definitions.TryGetValuePtr(reflection_type->name);
			if (other_type != nullptr) {
				if (!Reflection::CompareReflectionTypes(
					&data->reflection_manager,
					&data->reflection_manager,
					reflection_type,
					other_type
				)) {
					// At the moment assert
					ECS_ASSERT(false);
				}
			}
			else {
				data->reflection_manager.type_definitions.Insert(*reflection_type, reflection_type->name);
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------

struct ConstructFromNameActionData {
	InspectorDrawMaterialFileData* data;
	bool text_input_enter;
};

static void ConstructFromNameAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ConstructFromNameActionData* construct_data = (ConstructFromNameActionData*)_data;
	InspectorDrawMaterialFileData* data = construct_data->data;
	if (!construct_data->text_input_enter || keyboard->IsPressed(ECS_KEY_ENTER)) {
		if (data->construct_pbr_from_prefix_characters.size > 0) {
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
			ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
			GetProjectAssetsFolder(data->editor_state, assets_folder);

			CreatePBRMaterialFromNameOptions create_options;
			create_options.search_directory_is_mount_point = true;
			PBRMaterial pbr_material = CreatePBRMaterialFromName(
				data->material_asset.name,
				data->construct_pbr_from_prefix_characters,
				assets_folder,
				&stack_allocator,
				create_options
			);

			// Check to see if it has textures
			if (pbr_material.HasTextures()) {
				bool success = CreateMaterialAssetFromPBRMaterial(&data->temporary_database, &data->material_asset, &pbr_material, &stack_allocator);
				if (!success) {
					EditorSetConsoleError("Failed to construct the material from texture group");
				}

				Stream<char> window_name = system->GetWindowName(window_index);
				unsigned int inspector_index = GetInspectorIndex(window_name);
				// In any case, we need to now change the inspector to nothing and then back to this
				// material to reload the material from the metadata and have the values be correctly re-read
				ReloadInspectorAssetFromMetadata(data->editor_state, inspector_index);
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------

static void ReloadShaders(InspectorDrawMaterialFileData* data, unsigned int inspector_index, bool initial_reload) {
	// Returns true if it managed to open the file, else false
	EditorState* editor_state = data->editor_state;

	//MemoryProtectedAllocator protected_allocator(0);

	// We need this in case it is the initial reload and everything could be reflected
	// To match the resources from the metadata file with the current representation of the shaders
	//MaterialAsset* initial_material_asset = (MaterialAsset*)protected_allocator.Allocate(sizeof(MaterialAsset));
	//*initial_material_asset = data->material_asset;
	//MaterialAsset second_copy = *initial_material_asset;
	//void* initial_ptr = initial_material_asset->buffers[1][1].data.buffer;
	//void* initial_buffers_ptr = initial_material_asset->buffers[1].buffer;

	//MEMORY_BASIC_INFORMATION basic_info;
	//size_t returned_size = VirtualQuery(initial_material_asset->buffers[0].buffer, &basic_info, sizeof(basic_info));
	//if (initial_reload) {
	//	ECS_ASSERT(basic_info.Protect == 2);
	//}

	// We need to store this outside the reload shader lambda since inside it data->material_asset will change
	// When reallocation occur for the buffers resulting in incorrect values for the pixel shader (it happens rarely,
	// when some values don't match, resulting in a very hard to debug problem)
	MaterialAsset initial_material_asset = data->material_asset;

	auto reload_shader = [&](Stream<wchar_t> shader_path, ORDER order) {
		// We need to deallocate the dynamic resources in case a field changes types and the dynamic resources
		// Match causing an unexpected result
		ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
		GetInspectorName(inspector_index, window_name);
		unsigned int inspector_window_index = editor_state->ui_system->GetWindowFromName(window_name);
		editor_state->ui_system->DeallocateWindowDynamicResources(inspector_window_index);

		AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
		const ShaderReflection* shader_reflection = editor_state->UIGraphics()->GetShaderReflection();

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 64, ECS_MB);
		AllocatorPolymorphic stack_allocator = &_stack_allocator;
		Stream<char> source_code = ReadWholeFileText(shader_path, stack_allocator);
		if (source_code.buffer != nullptr) {
			// Deallocate the current allocator - it will update the index
			DeallocateCurrentShaderAllocator(data, order);

			unsigned int shader_handle = GetShaderHandle(data, order);
			// Get the shader metadata such that we can get the macros in order to remove the conditional blocks
			const ShaderMetadata* metadata = data->temporary_database.GetShaderConst(shader_handle);
			ECS_STACK_CAPACITY_STREAM(Stream<char>, shader_macros, 128);
			ECS_ASSERT(metadata->macros.size <= shader_macros.capacity);
			for (size_t index = 0; index < metadata->macros.size; index++) {
				shader_macros[index] = metadata->macros[index].name;
			}
			shader_macros.size = metadata->macros.size;
			source_code = PreprocessCFile(source_code, shader_macros);

			ECS_STACK_CAPACITY_STREAM(ShaderReflectedBuffer, shader_buffers, MAX_BUFFER_SLOT_COUNT);
			ECS_STACK_CAPACITY_STREAM(Reflection::ReflectionType, shader_cbuffer_types, MAX_BUFFER_SLOT_COUNT);
			ECS_STACK_CAPACITY_STREAM(ShaderReflectionBufferMatrixField, matrix_types_storage, 32);
			ECS_STACK_CAPACITY_STREAM(Stream<ShaderReflectionBufferMatrixField>, matrix_types, 8);
			ECS_STACK_CAPACITY_STREAM(char, matrix_types_name_storage, ECS_KB);

			ShaderReflectionBuffersOptions options;
			options.constant_buffer_reflection = &shader_cbuffer_types;
			options.only_constant_buffers = true;
			options.matrix_types_storage = matrix_types_storage;
			options.matrix_types = matrix_types.buffer;
			options.matrix_type_name_storage = matrix_types_name_storage;
			
			/*auto test = [&]() {
				if (initial_reload) {
					if (initial_material_asset->buffers[1][0].name.buffer < (void*)10000) {
						__debugbreak();
					}
					if (initial_material_asset->buffers[1][1].name.buffer < (void*)10000) {
						__debugbreak();
					}
					if (initial_material_asset->buffers[1][2].name.buffer < (void*)10000) {
						__debugbreak();
					}
					if (initial_material_asset->buffers[1][0].slot > 5) {
						__debugbreak();
					}
					if (initial_material_asset->buffers[1][1].slot > 5) {
						__debugbreak();
					}
					if (initial_material_asset->buffers[1][2].slot > 5) {
						__debugbreak();
					}
					if (initial_buffers_ptr != initial_material_asset->buffers[1].buffer) {
						__debugbreak();
					}
					if (initial_ptr != initial_material_asset->buffers[1][1].data.buffer) {
						void* ssinitial_ptr = initial_material_asset->reflection_manager;
						MEMORY_BASIC_INFORMATION basic_info;
						size_t returned_size = VirtualQuery(initial_ptr, &basic_info, sizeof(basic_info));

						returned_size = VirtualQuery(initial_material_asset->buffers[1].buffer, &basic_info, sizeof(basic_info));

						initial_material_asset->buffers[1][1].data.buffer = initial_ptr;
						__debugbreak();
					}
					if (order != VERTEX_ORDER) {
						if (initial_material_asset->buffers[1][1].data.buffer == data->material_asset.buffers[1][1].data.buffer) {
							__debugbreak();
						}
					}
					if (memcmp(&second_copy, initial_material_asset, sizeof(second_copy)) != 0) {
						__debugbreak();
					}
				}
			};*/

			bool all_reflect_success = true;
			bool success = shader_reflection->ReflectShaderBuffersSource(source_code, shader_buffers, stack_allocator, options);
			shader_cbuffer_types.size = shader_buffers.size;
			if (success) {
				// Check to see if any of the structures could not be parsed - if that is the case,
				// Then change the inspector to nothing and back to this material in order to have
				// The previous values be recovered when the parsing succeeds
				if (!data->success[order][SUCCESS_CBUFFER]) {
					ReloadInspectorAssetFromMetadata(editor_state, inspector_index);
					return true;
				}

				RegisterNewCBuffers(data, inspector_index, order, shader_cbuffer_types, shader_buffers.buffer, matrix_types.buffer);
			}
			else {
				if (data->cbuffers[order].size > 0) {
					DeallocateCBuffers(data, inspector_index, order);
				}
				all_reflect_success = false;
			}
			// Set this last since we need the previous value to check for the missing case
			data->success[order][SUCCESS_CBUFFER] = success;
			_stack_allocator.Clear();
			//test();

			ECS_STACK_CAPACITY_STREAM(ShaderReflectedTexture, shader_textures, MAX_TEXTURE_SLOT_COUNT);
			success = shader_reflection->ReflectShaderTexturesSource(source_code, shader_textures, stack_allocator);
			if (success) {
				if (!data->success[order][SUCCESS_TEXTURE]) {
					ReloadInspectorAssetFromMetadata(editor_state, inspector_index);
					return true;
				}

				RegisterNewTextures(data, order, shader_textures);
			}
			else {
				DeallocateTextures(data, order);
				all_reflect_success = false;
			}
			// Set this last since we need the previous value to check for the missing case
			data->success[order][SUCCESS_TEXTURE] = success;
			_stack_allocator.Clear();
			//test();

			ECS_STACK_CAPACITY_STREAM(ShaderReflectedSampler, shader_samplers, MAX_SAMPLER_SLOT_COUNT);
			success = shader_reflection->ReflectShaderSamplersSource(source_code, shader_samplers, stack_allocator);
			if (success) {
				if (!data->success[order][SUCCESS_SAMPLER]) {
					ReloadInspectorAssetFromMetadata(editor_state, inspector_index);
					return true;
				}

				RegisterNewSamplers(data, order, shader_samplers);
			}
			else {
				DeallocateSamplers(data, order);
				all_reflect_success = false;
			}
			// Set this last since we need the previous value to check for the missing case
			data->success[order][SUCCESS_SAMPLER] = success;
			//test();

			RecreateReflectionManagerForShaders(data);
			//test();

			if (all_reflect_success) {
				if (initial_reload) {
					// Try to match the serialized data with the current buffers
					data->material_asset.CopyMatchingNames(&initial_material_asset, GetMaterialShader(order));
				}

				// Update the material asset metadata file such that any new additions/removals will be picked up by the reload system
				// Do this only if the material could be reflected correctly and if the existing metadata file is different than this
				// Current version
				MaterialAsset file_asset;
				file_asset.Default(GetAssetName(&data->material_asset, ECS_ASSET_MATERIAL), GetAssetFile(&data->material_asset, ECS_ASSET_MATERIAL));
				_stack_allocator.Clear();
				bool read_success = data->temporary_database.ReadMaterialFile(data->material_asset.name, &file_asset, stack_allocator);
				bool write_new_metadata_file = true;
				if (read_success) {
					write_new_metadata_file = file_asset.Compare(&data->material_asset).is_different;
				}

				if (write_new_metadata_file) {
					bool write_file_success = data->temporary_database.WriteMaterialFile(&data->material_asset);
					if (!write_file_success) {
						ECS_FORMAT_TEMP_STRING(message, "Failed to write material metadata file for {#} after reflecting shaders", GetAssetName(&data->material_asset, ECS_ASSET_MATERIAL));
						EditorSetConsoleWarn(message);
					}
				}
			}


			return true;
		}
		else {
			return false;
		}
	};

	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	for (size_t index = 0; index < ORDER_COUNT; index++) {
		ORDER order = (ORDER)index;
		if (data->timer[index].GetDuration(ECS_TIMER_DURATION_MS) >= SHADER_LAZY_RELOAD_MS) {
			unsigned int current_handle = GetShaderHandle(data, (ORDER)index);

			if (current_handle != -1) {
				Stream<wchar_t> shader_file = data->temporary_database.GetAssetPath(current_handle, ECS_ASSET_SHADER);

				ECS_STACK_CAPACITY_STREAM(wchar_t, shader_path_storage, 512);
				Stream<wchar_t> shader_path = MountPathOnlyRel(shader_file, assets_folder, shader_path_storage);
				size_t new_shader_stamp = OS::GetFileLastWrite(shader_path);

				if (data->shader_stamp[index] == 0 || data->shader_stamp[index] == -1 || new_shader_stamp > data->shader_stamp[index]) {
					bool read_success = reload_shader(shader_path, order);
					if (!read_success) {
						data->shader_stamp[index] = -1;
					}
					else {
						data->shader_stamp[index] = new_shader_stamp;
					}
				}
				else {
					// Verify the metadata file now
					ECS_STACK_CAPACITY_STREAM(wchar_t, metadata_file, 512);
					Stream<char> asset_name = data->temporary_database.GetAssetName(current_handle, ECS_ASSET_SHADER);
					Stream<wchar_t> asset_file = data->temporary_database.GetAssetPath(current_handle, ECS_ASSET_SHADER);
					data->temporary_database.FileLocationAsset(asset_name, asset_file, metadata_file, ECS_ASSET_SHADER);
					size_t new_metadata_time_stamp = OS::GetFileLastWrite(metadata_file);
					if (data->metadata_shader_stamp[index] == 0 || data->metadata_shader_stamp[index] == -1 || new_metadata_time_stamp > data->metadata_shader_stamp[index]) {
						// Reload the shader
						bool success = data->temporary_database.UpdateAssetFromFile(GetShaderHandle(data, order), ECS_ASSET_SHADER);
						if (success) {
							bool read_success = reload_shader(shader_path, order);
							if (!read_success) {
								data->metadata_shader_stamp[index] = -1;
							}
							else {
								data->metadata_shader_stamp[index] = new_metadata_time_stamp;
							}
						}
						else {
							// If it failed, at the moment, don't do anything
							data->metadata_shader_stamp[index] = -1;
						}
					}
				}
			}
			data->timer[index].SetNewStart();
		}
	}

	//protected_allocator.Free();

}

// ------------------------------------------------------------------------------------------------------------

struct ShaderModifyCallbackData {
	InspectorDrawMaterialFileData* data;
	unsigned int inspector_index;
	ORDER order;
};

static void ShaderModifyCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ShaderModifyCallbackData* data = (ShaderModifyCallbackData*)_data;
	AssetOverrideCallbackAdditionalInfo* additional_data = (AssetOverrideCallbackAdditionalInfo*)_additional_data;

	// Delay the timer for that shader
	data->data->timer[data->order].DelayStart(-SHADER_LAZY_RELOAD_MS, ECS_TIMER_DURATION_MS);

	// If it was removed
	if (!additional_data->is_selection) {
		memset(data->data->success[data->order], 0, sizeof(data->data->success[data->order]));

		DeallocateCBuffers(data->data, data->inspector_index, data->order);
		DeallocateSamplers(data->data, data->order);
		DeallocateTextures(data->data, data->order);

		// Also update the shader time stamp to 0 such that it can be reloaded again
		data->data->shader_stamp[data->order] = 0;

		// The handle will get updated afterwards to -1
		// Update it manually here to -1
		if (data->order == VERTEX_ORDER) {
			data->data->material_asset.vertex_shader_handle = -1;
		}
		else {
			data->data->material_asset.pixel_shader_handle = -1;
		}

		unsigned int resize_counts[ECS_MATERIAL_SHADER_COUNT * 3];
		data->data->material_asset.WriteCounts(true, true, true, resize_counts);
		for (size_t index = 0; index < 3; index++) {
			resize_counts[data->order + index * ECS_MATERIAL_SHADER_COUNT] = 0;
		}

		data->data->material_asset.Resize(resize_counts, data->data->MaterialAssetAlllocator());
	}

	ReloadShaders(data->data, data->inspector_index, false);
	action_data->data = data->data;
	BaseModifyCallback(action_data);
}

// ------------------------------------------------------------------------------------------------------------

struct BuiltinCallbackData {
	InspectorDrawMaterialFileData* data;
};

static void BuiltinCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	BuiltinCallbackData* data = (BuiltinCallbackData*)_data;

	SetAssetBuiltinActionData set_data;
	set_data.asset = &data->data->material_asset;
	set_data.asset_type = ECS_ASSET_MATERIAL;
	set_data.builtin_index = data->data->builtin;
	set_data.current_path = data->data->path;
	set_data.editor_state = data->data->editor_state;

	action_data->data = &set_data;
	SetAssetBuiltinAction(action_data);
}

// ------------------------------------------------------------------------------------------------------------

static void InspectorCleanMaterial(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	InspectorDrawMaterialFileData* data = (InspectorDrawMaterialFileData*)_data;

	AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
	editor_state->module_reflection->DeallocateFieldOverride(VERTEX_TAG, data->shader_override_data[VERTEX_ORDER]);
	editor_state->module_reflection->DeallocateFieldOverride(PIXEL_TAG, data->shader_override_data[PIXEL_ORDER]);

	// Deallocate the temporary database allocator
	data->database_allocator.Clear();

	for (size_t index = 0; index < ORDER_COUNT; index++) {
		ORDER order = (ORDER)index;
		DeallocateCBuffers(data, inspector_index, order);
		DeallocateTextures(data, order);
		DeallocateSamplers(data, order);

		// Deallocate the allocator
		for (size_t subindex = 0; subindex < 2; subindex++) {
			data->shader_cbuffer_allocator[index][subindex].Free();
		}
	}

	Deallocate(editor_allocator, data->material_asset.name.buffer);
	data->construct_pbr_from_prefix_characters.Deallocate(editor_allocator);
}

// ------------------------------------------------------------------------------------------------------------

void InspectorDrawMaterialFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	InspectorDrawMaterialFileData* data = (InspectorDrawMaterialFileData*)_data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(data->path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	// Rebind the shader modify callback - in case the inspector index is changed
	ShaderModifyCallbackData shader_modify;
	shader_modify.data = data;
	shader_modify.inspector_index = inspector_index;
	shader_modify.order = VERTEX_ORDER;
	AssetOverrideBindCallbackData bind_data;
	bind_data.handler = { ShaderModifyCallback, &shader_modify, sizeof(shader_modify) };
	editor_state->module_reflection->BindInstanceFieldOverride(data->shader_override_data[VERTEX_ORDER], VERTEX_TAG, AssetOverrideBindCallback, &bind_data);

	shader_modify.order = PIXEL_ORDER;
	editor_state->module_reflection->BindInstanceFieldOverride(data->shader_override_data[PIXEL_ORDER], PIXEL_TAG, AssetOverrideBindCallback, &bind_data);

	ReloadShaders(data, inspector_index, false);

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ASSET_MATERIAL_ICON);
	InspectorDefaultFileInfo(editor_state, drawer, data->path);

	// Draw the settings
	UIDrawConfig config;

	// Determine if one of the dependencies is not loaded
	ECS_STACK_CAPACITY_STREAM(char, error_message, ECS_KB * 8);
	ECS_STACK_CAPACITY_STREAM(Stream<char>, failed_strings, 64);
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	IsMaterialFromMetadataLoadedExDesc is_loaded_desc;
	is_loaded_desc.mount_point = assets_folder;
	is_loaded_desc.error_string = &error_message;
	is_loaded_desc.segmented_error_string = &failed_strings;

	ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 128);
	GetAssetDependencies(&data->material_asset, ECS_ASSET_MATERIAL, &dependencies);

	bool is_material_loaded = true;

	// Check to see if this material is being used in the editor
	unsigned int main_database_handle = FindAsset(editor_state, data->material_asset.name, {}, ECS_ASSET_MATERIAL);
	if (main_database_handle != -1) {
		// Copy the texture and sampler pointers from the main database into the temporary database
		for (unsigned int index = 0; index < dependencies.size; index++) {
			void* current_asset = data->temporary_database.GetAsset(dependencies[index].handle, dependencies[index].type);
			Stream<char> asset_name = GetAssetName(current_asset, dependencies[index].type);
			Stream<wchar_t> asset_file = GetAssetFile(current_asset, dependencies[index].type);
			unsigned int handle = data->editor_state->asset_database->FindAsset(asset_name, asset_file, dependencies[index].type);
			if (handle != -1) {
				SetAssetToMetadata(
					current_asset, 
					dependencies[index].type, 
					GetAssetFromMetadata(data->editor_state->asset_database->GetAssetConst(handle, dependencies[index].type), dependencies[index].type)
				);
			}
			else {
				if (IsAssetFromMetadataValid(current_asset, dependencies[index].type)) {
					data->temporary_database.RandomizePointer(current_asset, dependencies[index].type);
				}
				//dependencies[index].handle = -1;
			}
		}
		RemapAssetDependencies(&data->material_asset, ECS_ASSET_MATERIAL, dependencies);

		const MaterialAsset* main_material = editor_state->asset_database->GetMaterialConst(main_database_handle);
		// Set the pointer for the material
		SetAssetToMetadata(&data->material_asset, ECS_ASSET_MATERIAL, GetAssetFromMetadata(main_material, ECS_ASSET_MATERIAL));

		is_material_loaded = IsMaterialFromMetadataLoadedEx(main_material, editor_state->RuntimeResourceManager(), editor_state->asset_database, &is_loaded_desc);
	}
	else {
		// Set a pointer value to the material such that it will "appear" as loaded. Since it is only in edit mode and
		// not actually used yet, it shouldn't generate a warning. The same for all internal dependencies
		SetValidAssetToMetadata(&data->material_asset, ECS_ASSET_MATERIAL);

		for (unsigned int index = 0; index < dependencies.size; index++) {
			void* current_dependency = data->temporary_database.GetAsset(dependencies[index].handle, dependencies[index].type);
			SetValidAssetToMetadata(current_dependency, dependencies[index].type);
		}

		is_material_loaded = IsMaterialFromMetadataLoadedEx(&data->material_asset, &data->temporary_database, &is_loaded_desc);
	}
	
	if (!is_material_loaded) {
		drawer->SpriteRectangle(UI_CONFIG_MAKE_SQUARE, config, ECS_TOOLS_UI_TEXTURE_WARN_ICON, EDITOR_YELLOW_COLOR);
		drawer->LabelList(failed_strings[0], { failed_strings.buffer + 1, failed_strings.size - 1 });
		drawer->CrossLine();
	}

	// Draw the settings
	UIConfigNamePadding name_padding;
	name_padding.total_length = ASSET_NAME_PADDING;
	config.AddFlag(name_padding);

	UIConfigWindowDependentSize dependent_size;
	config.AddFlag(dependent_size);

	const size_t BASE_CONFIGURATION = UI_CONFIG_NAME_PADDING | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ELEMENT_NAME_FIRST;

	BuiltinCallbackData builtin_callback_data = { data };
	UIConfigComboBoxCallback builtin_combo_callback;
	builtin_combo_callback.handler = { BuiltinCallback, &builtin_callback_data, sizeof(builtin_callback_data) };
	config.AddFlag(builtin_combo_callback);

	// Determine if the builtin has changed - if it did, update the flag
	data->builtin = FindMaterialBuiltinIndex(&data->temporary_database, &data->material_asset);

	// Draw the combo box indicating builtins
	drawer->ComboBox(
		BASE_CONFIGURATION | UI_CONFIG_COMBO_BOX_CALLBACK,
		config,
		"Builtin",
		{ EDITOR_MATERIAL_BUILTIN_NAME, std::size(EDITOR_MATERIAL_BUILTIN_NAME) },
		std::size(EDITOR_MATERIAL_BUILTIN_NAME),
		(unsigned char*)&data->builtin
	);
	config.flag_count--;
	drawer->NextRow();

	if (data->builtin == EDITOR_MATERIAL_BUILTIN_PBR) {
		ConstructFromNameActionData construct_data = { data, true };

		// Draw the Construct from name input and button
		UIConfigTextInputCallback input_callback;
		input_callback.handler = { ConstructFromNameAction, &construct_data, sizeof(construct_data) };
		input_callback.trigger_only_on_release = true;
		config.AddFlag(input_callback);

		drawer->TextInput(BASE_CONFIGURATION | UI_CONFIG_TEXT_INPUT_CALLBACK, config, "Create from prefix", &data->construct_pbr_from_prefix_characters);
		drawer->NextRow();

		config.flag_count--;
	}

	drawer->CrossLine();

	editor_state->module_reflection->DrawFieldOverride(
		STRING(ECS_VERTEX_SHADER_HANDLE),
		data->shader_override_data[VERTEX_ORDER],
		&data->material_asset.vertex_shader_handle,
		drawer,
		BASE_CONFIGURATION,
		&config,
		"Vertex Shader"
	);
	drawer->NextRow();

	editor_state->module_reflection->DrawFieldOverride(
		STRING(ECS_PIXEL_SHADER_HANDLE),
		data->shader_override_data[PIXEL_ORDER],
		&data->material_asset.pixel_shader_handle,
		drawer,
		BASE_CONFIGURATION,
		&config,
		"Pixel Shader"
	);
	drawer->NextRow();

	drawer->CrossLine();

	// Returns true if it has drawn an element
	auto draw_shader = [=](ORDER order, Stream<char> shader_string) {
		size_t shader_elements = 0;
		shader_elements += data->cbuffers[order].size;
		shader_elements += data->textures[order].size;
		shader_elements += data->samplers[order].size;

		bool has_failed = false;
		for (size_t index = 0; index < SUCCESS_COUNT; index++) {
			has_failed |= data->success[order][index];
		}

		if (shader_elements > 0 || has_failed) {
			ECS_MATERIAL_SHADER material_shader = GetMaterialShader(order);

			ECS_FORMAT_TEMP_STRING(collapsing_header_string, "{#} Inputs", shader_string);
			drawer->CollapsingHeader(collapsing_header_string, data->shader_collapsing_header_state + order, [=]() {
				size_t GENERAL_CONFIGURATION = UI_CONFIG_NAME_PADDING | UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_ELEMENT_NAME_FIRST;

				UIDrawConfig config;
				UIConfigNamePadding name_padding;
				name_padding.total_length = REFLECTED_SETTINGS_NAME_PADDING;
				config.AddFlag(name_padding);
				UIConfigWindowDependentSize window_dependent;
				config.AddFlag(window_dependent);
				
				ECS_STACK_VOID_STREAM(stack_allocation, ECS_KB * 4);
				UIReflectionDrawConfig reflection_config[15];

				for (size_t index = 0; index < std::size(reflection_config); index++) {
					reflection_config[index].index_count = 0;
				}

				size_t slots_used = UIReflectionDrawConfigSplatCallback(
					{ reflection_config, std::size(reflection_config) }, 
					{ BaseModifyCallback, data, 0 }, 
					ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ALL, 
					stack_allocation.buffer,
					false
				);

				if (!data->success[order][SUCCESS_CBUFFER]) {
					drawer->Text("Failed to reflect the constant buffers for this shader.");
					drawer->NextRow();
				}
				else if (data->cbuffers[order].size > 0) {
					unsigned int shader_handle = GetShaderHandle(data, order);
					for (size_t index = 0; index < data->cbuffers[order].size; index++) {
						ECS_STACK_CAPACITY_STREAM(char, ui_instance_name, 512);
						GetUITypeNameForCBuffer(data, inspector_index, shader_handle, data->cbuffers[order].buffer + index, ui_instance_name);

						UIReflectionDrawInstanceOptions options;
						options.drawer = drawer;
						options.config = &config;
						options.global_configuration = GENERAL_CONFIGURATION;
						options.additional_configs = { reflection_config, slots_used };

						editor_state->ui_reflection->DrawInstance(ui_instance_name, &options);
					}
				}

				if (!data->success[order][SUCCESS_TEXTURE]) {
					drawer->Text("Failed to reflect the textures for this shader.");
					drawer->NextRow();
				}
				else if (data->textures[order].size > 0) {
					for (size_t index = 0; index < data->textures[order].size; index++) {
						editor_state->module_reflection->DrawFieldOverride(
							TEXTURE_TAG,
							data->texture_override_data[order][index],
							&data->material_asset.textures[material_shader][index].metadata_handle,
							drawer,
							GENERAL_CONFIGURATION,
							&config,
							data->textures[order][index].name
						);
						drawer->NextRow();
					}
				}

				if (!data->success[order][SUCCESS_SAMPLER]) {
					drawer->Text("Failed to reflect the samplers for this shader.");
					drawer->NextRow();
				}
				else if (data->samplers[order].size > 0) {
					for (size_t index = 0; index < data->samplers[order].size; index++) {
						editor_state->module_reflection->DrawFieldOverride(
							SAMPLER_TAG,
							data->sampler_override_data[order][index],
							&data->material_asset.samplers[material_shader][index].metadata_handle,
							drawer,
							GENERAL_CONFIGURATION,
							&config,
							data->samplers[order][index].name
						);
						drawer->NextRow();
					}
				}
			});		
			return true;
		}
		return has_failed;
	};

	bool has_drawn = draw_shader(VERTEX_ORDER, "Vertex");
	has_drawn |= draw_shader(PIXEL_ORDER, "Pixel");

	if (!has_drawn) {
		// Display a text informing the user that there is no input needed
		drawer->Text("The current material does not need any input.");
		drawer->NextRow();
	}

	InspectorCopyCurrentAssetSettingData copy_data;
	copy_data.editor_state = editor_state;
	copy_data.asset_type = ECS_ASSET_MATERIAL;
	copy_data.inspector_index = inspector_index;
	copy_data.metadata = &data->material_asset;
	copy_data.path = data->path;
	copy_data.database = &data->temporary_database;
	InspectorDrawCopyCurrentAssetSetting(drawer, &copy_data);
}

// ------------------------------------------------------------------------------------------------------------

void ChangeInspectorToMaterialFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	InspectorDrawMaterialFileData data;
	data.path = path;
	data.editor_state = editor_state;
	memset(&data.material_asset, 0, sizeof(data.material_asset));

	// Allocate the data and embedd the path in it
	// Later on. It is fine to read from the stack more bytes
	uint3 found_inspector_index = ChangeInspectorDrawFunctionWithSearchEx(
		editor_state,
		inspector_index,
		{ InspectorDrawMaterialFile, InspectorCleanMaterial },
		&data,
		sizeof(data) + sizeof(wchar_t) * (path.size + 1),
		-1,
		[=](void* inspector_data) {
			InspectorDrawMaterialFileData* other_data = (InspectorDrawMaterialFileData*)inspector_data;
			return other_data->path == path;
		}
	);

	if (found_inspector_index.x == -1 && found_inspector_index.y != -1) {
		inspector_index = found_inspector_index.y;
		// Get the data and set the path
		InspectorDrawMaterialFileData* draw_data = (InspectorDrawMaterialFileData*)GetInspectorDrawFunctionData(editor_state, inspector_index);
		memset(draw_data->success, 0, sizeof(draw_data->success));
		draw_data->path = { OffsetPointer(draw_data, sizeof(*draw_data)), path.size };
		draw_data->path.CopyOther(path);
		UpdateLastInspectorTargetData(editor_state, inspector_index, draw_data);
		
		SetLastInspectorTargetInitialize(editor_state, inspector_index, [](EditorState* editor_state, void* data, unsigned int inspector_index) {
			InspectorDrawMaterialFileData* draw_data = (InspectorDrawMaterialFileData*)data;
			AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();

			const size_t CONSTRUCT_PBR_INPUT_CAPACITY = 256;
			draw_data->construct_pbr_from_prefix_characters.Initialize(editor_allocator, 0, CONSTRUCT_PBR_INPUT_CAPACITY);

			draw_data->shader_override_data[VERTEX_ORDER] = editor_state->module_reflection->InitializeFieldOverride(VERTEX_TAG, "Vertex Shader");
			draw_data->shader_override_data[PIXEL_ORDER] = editor_state->module_reflection->InitializeFieldOverride(PIXEL_TAG, "Pixel Shader");
			// Initialize the reflection manager
			InitializeReflectionManager(draw_data);
			draw_data->material_asset.reflection_manager = &draw_data->reflection_manager;
			draw_data->database_allocator = MemoryManager(
				TEMPORARY_DATABASE_ALLOCATOR_CAPACITY,
				ECS_KB,
				TEMPORARY_DATABASE_ALLOCATOR_CAPACITY,
				editor_state->GlobalMemoryManager()
			);
			draw_data->temporary_database = AssetDatabase(&draw_data->database_allocator, editor_state->asset_database->reflection_manager);
			draw_data->temporary_database.SetFileLocation(editor_state->asset_database->metadata_file_location);

			for (size_t index = 0; index < ORDER_COUNT; index++) {
				draw_data->cbuffers[index].size = 0;
				draw_data->textures[index].size = 0;
				draw_data->samplers[index].size = 0;
				draw_data->shader_stamp[index] = 0;
				draw_data->metadata_shader_stamp[index] = 0;
				// Set this to true since most of the time you want to see these
				draw_data->shader_collapsing_header_state[index] = true;
				// Set these to true such that when the first reload occurs it won't trigger
				// A reload event because the previous success is false
				for (size_t subindex = 0; subindex < SUCCESS_COUNT; subindex++) {
					draw_data->success[index][subindex] = true;
				}

				// Create the shader allocators
				draw_data->shader_cbuffer_allocator_index[index] = 0;
				for (size_t subindex = 0; subindex < 2; subindex++) {
					draw_data->shader_cbuffer_allocator[index][subindex] = ResizableLinearAllocator(
						SHADER_ALLOCATOR_CAPACITY,
						SHADER_ALLOCATOR_CAPACITY,
						editor_state->EditorAllocator()
					);
				}
				draw_data->timer[index].DelayStart(-SHADER_LAZY_RELOAD_MS, ECS_TIMER_DURATION_MS);
			}

			ShaderModifyCallbackData shader_modify_data;
			shader_modify_data.data = draw_data;
			shader_modify_data.inspector_index = inspector_index;
			shader_modify_data.order = VERTEX_ORDER;
			AssetOverrideSetAllData set_all_data;
			set_all_data.set_index.sandbox_index = -1;
			set_all_data.callback = { ShaderModifyCallback, &shader_modify_data, sizeof(shader_modify_data) };
			set_all_data.new_database.database = &draw_data->temporary_database;
			editor_state->module_reflection->BindInstanceFieldOverride(draw_data->shader_override_data[VERTEX_ORDER], VERTEX_TAG, AssetOverrideSetAll, &set_all_data);

			shader_modify_data.order = PIXEL_ORDER;
			editor_state->module_reflection->BindInstanceFieldOverride(draw_data->shader_override_data[PIXEL_ORDER], PIXEL_TAG, AssetOverrideSetAll, &set_all_data);

			// Try to read the metadata file
			ECS_STACK_CAPACITY_STREAM(char, asset_name, 512);
			GetAssetNameFromThunkOrForwardingFile(editor_state, draw_data->path, asset_name);
			draw_data->material_asset.name = asset_name;

			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(temporary_allocator, ECS_KB * 64, ECS_MB);
			bool success = draw_data->temporary_database.ReadMaterialFile(asset_name, &draw_data->material_asset, &temporary_allocator);
			draw_data->material_asset.reflection_manager = &draw_data->reflection_manager;
			if (!success) {
				// Set the default for the metadata
				draw_data->material_asset.Default(asset_name, { nullptr, 0 });
				// The default will make this nullptr
				draw_data->material_asset.reflection_manager = &draw_data->reflection_manager;
			}
			else {
				// The buffers and the material are allocated from the asset database
				// Reload the shaders and then determine what data to copy
				// The reload will correctly copy the matching names for the resources it manages to read
				ReloadShaders(draw_data, inspector_index, true);
			}
			draw_data->material_asset.name = StringCopy(editor_state->EditorAllocator(), asset_name);
		});
	}
}

// ------------------------------------------------------------------------------------------------------------

void InspectorMaterialFileAddFunctors(InspectorTable* table) {
	for (size_t index = 0; index < std::size(ASSET_MATERIAL_EXTENSIONS); index++) {
		AddInspectorTableFunction(table, { InspectorDrawMaterialFile, InspectorCleanMaterial }, ASSET_MATERIAL_EXTENSIONS[index]);
	}
}

// ------------------------------------------------------------------------------------------------------------

InspectorAssetTarget InspectorDrawMaterialTarget(const void* inspector_data)
{
	InspectorDrawMaterialFileData* data = (InspectorDrawMaterialFileData*)inspector_data;
	return { data->path, {} };
}

// ------------------------------------------------------------------------------------------------------------