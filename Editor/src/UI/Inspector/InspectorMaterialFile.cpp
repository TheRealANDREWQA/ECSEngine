#include "editorpch.h"
#include "InspectorMiscFile.h"
#include "../Inspector.h"
#include "InspectorUtilities.h"
#include "../../Editor/EditorState.h"
#include "../../Editor/EditorPalette.h"

#include "../AssetSettingHelper.h"
#include "../../Assets/AssetManagement.h"
#include "../../Assets/AssetExtensions.h"
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

#define SHADER_ALLOCATOR_CAPACITY ECS_KB * 64

/*
	The module reflection contains the overrides, so use it for the override draw
*/

#define VERTEX_TAG STRING(ECS_VERTEX_SHADER_HANDLE)
#define PIXEL_TAG STRING(ECS_PIXEL_SHADER_HANDLE)
#define TEXTURE_TAG STRING(ECS_TEXTURE_HANDLE)
#define SAMPLER_TAG STRING(ECS_GPU_SAMPLER_HANDLE)

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

inline ECS_MATERIAL_SHADER GetMaterialShader(ORDER order) {
	return order == VERTEX_ORDER ? ECS_MATERIAL_SHADER_VERTEX : ECS_MATERIAL_SHADER_PIXEL;
}

struct InspectorDrawMaterialFileData {
	MaterialAsset material_asset;
	Stream<wchar_t> path;
	EditorState* editor_state;
	
	bool success[ORDER_COUNT][SUCCESS_COUNT];
	unsigned char shader_cbuffer_allocator_index[ORDER_COUNT];
	bool shader_collapsing_header_state[ORDER_COUNT];
	Timer timer[ORDER_COUNT];
	void* shader_override_data[ORDER_COUNT];
	size_t shader_stamp[ORDER_COUNT];

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
	MemoryManager shader_cbuffer_allocator[ORDER_COUNT][2];
};

// ------------------------------------------------------------------------------------------------------------

inline unsigned int GetShaderHandle(const InspectorDrawMaterialFileData* data, ORDER order) {
	return order == VERTEX_ORDER ? data->material_asset.vertex_shader_handle : data->material_asset.pixel_shader_handle;
}

// ------------------------------------------------------------------------------------------------------------

void GetUITypeNameForCBuffer(
	const EditorState* editor_state, 
	unsigned int inspector_index, 
	unsigned int shader_handle,
	const Reflection::ReflectionType* type, 
	CapacityStream<char>& name
) {
	// Concatenate the type name with the shader name and the inspector index
	name.AddStream(type->name);
	Stream<wchar_t> shader_path = editor_state->asset_database->GetAssetPath(shader_handle, ECS_ASSET_SHADER);
	function::ConvertWideCharsToASCII(shader_path, name);
	function::ConvertIntToChars(name, inspector_index);
}

// ------------------------------------------------------------------------------------------------------------

// It will also increment the current index
void DeallocateCurrentShaderAllocator(InspectorDrawMaterialFileData* draw_data, ORDER order) {
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

void DeallocateCBuffers(
	InspectorDrawMaterialFileData* draw_data, 
	unsigned int inspector_index, 
	ORDER order
) {
	Stream<Reflection::ReflectionType> cbuffers = { nullptr, 0 };
	unsigned int shader_handle = -1;
	if (order == VERTEX_ORDER) {
		cbuffers = draw_data->cbuffers[order];
		shader_handle = draw_data->material_asset.vertex_shader_handle;
	}
	else if (order == PIXEL_ORDER) {
		cbuffers = draw_data->cbuffers[order];
		shader_handle = draw_data->material_asset.pixel_shader_handle;
	}

	AllocatorPolymorphic allocator = draw_data->editor_state->EditorAllocator();
	if (cbuffers.size > 0) {
		for (size_t index = 0; index < cbuffers.size; index++) {
			ShaderConstantBufferReflectionTypeDeallocate(cbuffers.buffer + index, allocator);
			// Destroy the UI type as well (it will destroy the instance associated with it as well)
			ECS_STACK_CAPACITY_STREAM(char, ui_type_name, 512);
			GetUITypeNameForCBuffer(draw_data->editor_state, inspector_index, shader_handle, cbuffers.buffer + index, ui_type_name);
			draw_data->editor_state->ui_reflection->DestroyType(ui_type_name);
		}
		Deallocate(allocator, cbuffers.buffer);
	}

	DeallocateCurrentShaderAllocator(draw_data, order);
}

// ------------------------------------------------------------------------------------------------------------

void DeleteTextures(InspectorDrawMaterialFileData* draw_data, ORDER order) {
	AllocatorPolymorphic allocator = draw_data->editor_state->EditorAllocator();
	if (draw_data->textures[order].size > 0) {
		for (size_t index = 0; index < draw_data->textures[order].size; index++) {
			draw_data->editor_state->module_reflection->DeallocateFieldOverride(TEXTURE_TAG, draw_data->texture_override_data[order][index]);
		}
		ShaderReflectedTexturesDeallocate(draw_data->textures[order], allocator);
		Deallocate(allocator, draw_data->texture_override_data[order]);
	}
}

// ------------------------------------------------------------------------------------------------------------

void DeleteSamplers(InspectorDrawMaterialFileData* draw_data, ORDER order) {
	AllocatorPolymorphic allocator = draw_data->editor_state->EditorAllocator();
	if (draw_data->samplers[order].size > 0) {
		for (size_t index = 0; index < draw_data->samplers[order].size; index++) {
			draw_data->editor_state->module_reflection->DeallocateFieldOverride(SAMPLER_TAG, draw_data->sampler_override_data[order][index]);
		}
		ShaderReflectedSamplersDeallocate(draw_data->samplers[order], allocator);
		Deallocate(allocator, draw_data->sampler_override_data[order]);
	}
}

// ------------------------------------------------------------------------------------------------------------

// When new cbuffer definitions are found, we need to retrieve the old data into the new format
// The new_reflected_buffers is needed for the slot index. The new_cbuffers are going to be copied individually
// and a new buffer allocated for the stream. Also the UI types and instances will be created
// It will also deallocate the old cbuffers
void RegisterNewCBuffers(
	InspectorDrawMaterialFileData* draw_data,
	unsigned int inspector_index,
	ORDER order,
	Stream<Reflection::ReflectionType> new_cbuffers,
	ShaderReflectedBuffer* new_reflected_buffers
) {
	ECS_STACK_CAPACITY_STREAM(MaterialAssetBuffer, new_buffers, MAX_BUFFER_SLOT_COUNT);
	ECS_ASSERT(new_cbuffers.size <= MAX_BUFFER_SLOT_COUNT);

	Stream<Reflection::ReflectionType> old_cbuffers = draw_data->cbuffers[order];

	AllocatorPolymorphic current_allocator = GetAllocatorPolymorphic(draw_data->shader_cbuffer_allocator[order] + (draw_data->shader_cbuffer_allocator_index[order] == 1));

	ECS_MATERIAL_SHADER material_shader_type = GetMaterialShader(order);

	// Start by making a new allocation for every new buffer
	for (size_t index = 0; index < new_cbuffers.size; index++) {
		size_t type_byte_size = Reflection::GetReflectionTypeByteSize(new_cbuffers.buffer + index);
		new_buffers[index].data.buffer = Allocate(current_allocator, type_byte_size);
		new_buffers[index].data.size = type_byte_size;
		new_buffers[index].slot = new_reflected_buffers[index].register_index;
		new_buffers[index].dynamic = true;

		// Set the default values for that type
		draw_data->editor_state->ui_reflection->reflection->SetInstanceDefaultData(new_cbuffers.buffer + index, new_buffers[index].data.buffer);
	}
	new_buffers.size = new_cbuffers.size;

	for (size_t index = 0; index < old_cbuffers.size; index++) {
		// Check to see if it still exists
		size_t subindex = 0;
		for (; subindex < new_cbuffers.size; subindex++) {
			if (function::CompareStrings(new_cbuffers[subindex].name, old_cbuffers[index].name)) {
				break;
			}
		}
		
		// Deallocate the old reflection type as well
		ShaderConstantBufferReflectionTypeDeallocate(old_cbuffers.buffer + index, draw_data->editor_state->EditorAllocator());
		// Destroy the UI type - it will also destroy the instance
		ECS_STACK_CAPACITY_STREAM(char, ui_type_name, 512);
		GetUITypeNameForCBuffer(
			draw_data->editor_state,
			inspector_index,
			order == VERTEX_ORDER ? draw_data->material_asset.vertex_shader_handle : draw_data->material_asset.pixel_shader_handle,
			old_cbuffers.buffer + index,
			ui_type_name
		);
		draw_data->editor_state->ui_reflection->DestroyType(ui_type_name);

		if (subindex < new_cbuffers.size) {
			const void* buffer_data = draw_data->material_asset.buffers[material_shader_type][index].data.buffer;
			// We have a match
			const Reflection::ReflectionManager* reflection_manager = draw_data->editor_state->ui_reflection->reflection;
			if (Reflection::CompareReflectionTypes(reflection_manager, reflection_manager, old_cbuffers.buffer + index, new_cbuffers.buffer + subindex)) {
				memcpy(new_buffers[subindex].data.buffer, buffer_data, new_buffers[subindex].data.size);
			}
			else {
				// It has changed. We need to convert the old type to the new type
				Reflection::CopyReflectionTypeToNewVersion(
					reflection_manager,
					reflection_manager,
					old_cbuffers.buffer + index,
					new_cbuffers.buffer + subindex,
					buffer_data,
					new_buffers[subindex].data.buffer,
					current_allocator,
					true
				);
			}
		}
	}

	// Can deallocate the previous allocator
	DeallocateCurrentShaderAllocator(draw_data, order);

	AllocatorPolymorphic allocator = draw_data->editor_state->EditorAllocator();
	old_cbuffers.Deallocate(allocator);

	draw_data->cbuffers[order].InitializeAndCopy(allocator, new_cbuffers);
	for (size_t index = 0; index < new_cbuffers.size; index++) {
		ShaderConstantBufferReflectionTypeCopy(new_cbuffers.buffer + index, draw_data->cbuffers[order].buffer + index, allocator);
		// Create the UI type and the UI instance
		ECS_STACK_CAPACITY_STREAM(char, ui_type_name, 512);
		GetUITypeNameForCBuffer(
			draw_data->editor_state,
			inspector_index,
			order == VERTEX_ORDER ? draw_data->material_asset.vertex_shader_handle : draw_data->material_asset.pixel_shader_handle,
			draw_data->cbuffers[order].buffer + index,
			ui_type_name
		);
		UIReflectionType* ui_type = draw_data->editor_state->ui_reflection->CreateType(draw_data->cbuffers[order].buffer + index, ui_type_name);
		UIReflectionInstance* instance = draw_data->editor_state->ui_reflection->CreateInstance(ui_type_name, ui_type, ui_type_name);
		draw_data->editor_state->ui_reflection->BindInstancePtrs(instance, new_buffers[index].data.buffer, draw_data->cbuffers[order].buffer + index);
	}

	unsigned int counts[ECS_MATERIAL_SHADER_COUNT * 3];
	draw_data->material_asset.WriteCounts(true, true, true, counts);
	counts[ECS_MATERIAL_SHADER_COUNT * 2 + material_shader_type] = new_cbuffers.size;
	draw_data->material_asset.Resize(counts, allocator);
	draw_data->material_asset.buffers[material_shader_type].Copy(new_buffers);
}

// ------------------------------------------------------------------------------------------------------------

void RegisterNewTextures(
	InspectorDrawMaterialFileData* draw_data,
	ORDER order,
	Stream<ShaderReflectedTexture> new_textures
) {
	AllocatorPolymorphic allocator = draw_data->editor_state->EditorAllocator();

	Stream<ShaderReflectedTexture> old_textures = draw_data->textures[order];

	ECS_MATERIAL_SHADER material_shader_type = GetMaterialShader(order);

	ECS_STACK_CAPACITY_STREAM(bool, matched_old_override, MAX_TEXTURE_SLOT_COUNT);
	memset(matched_old_override.buffer, 0, sizeof(bool) * MAX_TEXTURE_SLOT_COUNT);
	ECS_STACK_CAPACITY_STREAM(MaterialAssetResource, textures, MAX_TEXTURE_SLOT_COUNT);
	ECS_ASSERT(new_textures.size <= MAX_TEXTURE_SLOT_COUNT);

	void** override_data = nullptr;
	if (new_textures.size > 0) {
		override_data = (void**)Allocate(allocator, sizeof(void*) * new_textures.size);
	}

	// Preinitialize the array
	for (size_t index = 0; index < new_textures.size; index++) {
		textures[index].metadata_handle = -1;
		textures[index].slot = new_textures[index].register_index;
		override_data[index] = nullptr;
	}
	textures.size = new_textures.size;

	// See which textures have remained the same
	for (size_t index = 0; index < new_textures.size; index++) {
		size_t subindex = 0;
		for (; subindex < old_textures.size; subindex++) {
			if (function::CompareStrings(old_textures[subindex].name, new_textures[index].name)) {
				break;
			}
		}
		if (subindex < old_textures.size) {
			// We have a match
			// Retrieve the metadata handle and the override data
			textures[index].metadata_handle = draw_data->material_asset.textures[material_shader_type][subindex].metadata_handle;
			override_data[index] = draw_data->texture_override_data[order][subindex];
			matched_old_override[subindex] = true;
		}
		else {
			override_data[index] = draw_data->editor_state->module_reflection->InitializeFieldOverride(TEXTURE_TAG, new_textures[index].name);
		}
	}

	if (draw_data->textures[order].size > 0) {
		// All old overrides that were not matched need to be deallocated
		for (size_t index = 0; index < draw_data->textures[order].size; index++) {
			if (!matched_old_override[index]) {
				draw_data->editor_state->module_reflection->DeallocateFieldOverride(TEXTURE_TAG, draw_data->texture_override_data[order][index]);
			}
		}
		Deallocate(allocator, draw_data->texture_override_data[order]);
		draw_data->textures[order].Deallocate(allocator);
	}
	
	draw_data->textures[order].InitializeAndCopy(allocator, new_textures);

	unsigned int counts[ECS_MATERIAL_SHADER_COUNT * 3];
	draw_data->material_asset.WriteCounts(true, true, true, counts);
	counts[material_shader_type] = new_textures.size;
	draw_data->material_asset.Resize(counts, allocator);
	draw_data->material_asset.textures[material_shader_type].Copy(textures);
	draw_data->texture_override_data[order] = override_data;
}

// ------------------------------------------------------------------------------------------------------------

void RegisterNewSamplers(
	InspectorDrawMaterialFileData* draw_data,
	ORDER order,
	Stream<ShaderReflectedSampler> new_samplers
) {
	AllocatorPolymorphic allocator = draw_data->editor_state->EditorAllocator();

	Stream<ShaderReflectedSampler> old_samplers = draw_data->samplers[order];

	ECS_MATERIAL_SHADER material_shader_type = GetMaterialShader(order);

	ECS_STACK_CAPACITY_STREAM(bool, matched_old_override, MAX_SAMPLER_SLOT_COUNT);
	memset(matched_old_override.buffer, 0, sizeof(bool) * MAX_SAMPLER_SLOT_COUNT);
	ECS_STACK_CAPACITY_STREAM(MaterialAssetResource, samplers, MAX_SAMPLER_SLOT_COUNT);
	ECS_ASSERT(new_samplers.size <= MAX_SAMPLER_SLOT_COUNT);

	void** new_override_data = nullptr;
	if (new_samplers.size > 0) {
		new_override_data = (void**)Allocate(allocator, sizeof(void*) * new_samplers.size);
	}

	// Preinitialize the array
	for (size_t index = 0; index < new_samplers.size; index++) {
		samplers[index].metadata_handle = -1;
		samplers[index].slot = new_samplers[index].register_index;
		new_override_data[index] = nullptr;
	}
	samplers.size = new_samplers.size;

	// See which textures have remained the same
	for (size_t index = 0; index < new_samplers.size; index++) {
		size_t subindex = 0;
		for (; subindex < old_samplers.size; subindex++) {
			if (function::CompareStrings(old_samplers[subindex].name, new_samplers[index].name)) {
				break;
			}
		}
		if (subindex < old_samplers.size) {
			// We have a match
			// Retrieve the metadata handle and the override data
			samplers[index].metadata_handle = draw_data->material_asset.samplers[material_shader_type][subindex].metadata_handle;
			new_override_data[index] = draw_data->sampler_override_data[order][subindex];
			matched_old_override[subindex] = true;
		}
		else {
			// Must initialize the override data
			new_override_data[index] = draw_data->editor_state->module_reflection->InitializeFieldOverride(SAMPLER_TAG, new_samplers[index].name);
		}
	}

	if (draw_data->samplers[order].size > 0) {
		// Need to deallocate the unmatched overrides
		for (size_t index = 0; index < draw_data->samplers[order].size; index++) {
			if (!matched_old_override[index]) {
				draw_data->editor_state->module_reflection->DeallocateFieldOverride(SAMPLER_TAG, draw_data->sampler_override_data[order][index]);
			}
		}
		Deallocate(allocator, draw_data->sampler_override_data[order]);
		draw_data->samplers[order].Deallocate(allocator);
	}

	draw_data->samplers[order].InitializeAndCopy(allocator, new_samplers);

	unsigned int counts[ECS_MATERIAL_SHADER_COUNT * 3];
	draw_data->material_asset.WriteCounts(true, true, true, counts);
	counts[ECS_MATERIAL_SHADER_COUNT + material_shader_type] = new_samplers.size;
	draw_data->material_asset.Resize(counts, allocator);
	draw_data->material_asset.samplers[material_shader_type].Copy(samplers);
	draw_data->sampler_override_data[order] = new_override_data;
}

// ------------------------------------------------------------------------------------------------------------

void BaseModifyCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	InspectorDrawMaterialFileData* data = (InspectorDrawMaterialFileData*)_data;

	AssetSettingsHelperChangedNoFileActionData changed_data;
	changed_data.editor_state = data->editor_state;
	changed_data.asset = &data->material_asset;
	changed_data.asset_type = ECS_ASSET_MATERIAL;

	action_data->data = &changed_data;
	AssetSettingsHelperChangedNoFileAction(action_data);
}

// ------------------------------------------------------------------------------------------------------------

void ReloadShaders(InspectorDrawMaterialFileData* data, unsigned int inspector_index) {
	// Returns true if it managed to open the file, else false
	EditorState* editor_state = data->editor_state;
	auto reload_shader = [data, editor_state, inspector_index](Stream<wchar_t> shader_path, ORDER order) {
		AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
		const ShaderReflection* shader_reflection = editor_state->UIGraphics()->GetShaderReflection();

		Stream<char> source_code = ReadWholeFileText(shader_path, editor_allocator);
		if (source_code.buffer != nullptr) {
			ECS_STACK_CAPACITY_STREAM(ShaderReflectedBuffer, shader_buffers, MAX_BUFFER_SLOT_COUNT);
			ECS_STACK_CAPACITY_STREAM(Reflection::ReflectionType, shader_cbuffer_types, MAX_BUFFER_SLOT_COUNT);
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 64, ECS_MB);
			AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

			bool success = shader_reflection->ReflectShaderBuffersSource(source_code, shader_buffers, stack_allocator, true, shader_cbuffer_types.buffer);
			shader_cbuffer_types.size = shader_buffers.size;
			data->success[order][SUCCESS_CBUFFER] = success;
			if (success) {
				RegisterNewCBuffers(data, inspector_index, order, shader_cbuffer_types, shader_buffers.buffer);
			}
			else {
				if (data->cbuffers[order].size > 0) {
					DeallocateCBuffers(data, inspector_index, order);
					data->cbuffers[order].size = 0;
					// We could do a resize here, but it's probably not worth
				}
			}

			ECS_STACK_CAPACITY_STREAM(ShaderReflectedTexture, shader_textures, MAX_TEXTURE_SLOT_COUNT);
			success = shader_reflection->ReflectShaderTexturesSource(source_code, shader_textures, editor_allocator);
			data->success[order][SUCCESS_TEXTURE] = success;
			if (success) {
				RegisterNewTextures(data, order, shader_textures);
			}
			else {
				DeleteTextures(data, order);
				// We could do a resize here, but it's probably not worth
			}

			ECS_STACK_CAPACITY_STREAM(ShaderReflectedSampler, shader_samplers, MAX_SAMPLER_SLOT_COUNT);
			success = shader_reflection->ReflectShaderSamplersSource(source_code, shader_samplers, editor_allocator);
			data->success[order][SUCCESS_SAMPLER] = success;
			if (success) {
				RegisterNewSamplers(data, order, shader_samplers);
			}
			else {
				DeleteSamplers(data, order);
				// We could do a resize here, but it's probably not worth
			}

			Deallocate(editor_allocator, source_code.buffer);
			return true;
		}
		else {
			return false;
		}
	};

	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	for (size_t index = 0; index < ORDER_COUNT; index++) {
		if (data->timer[index].GetDuration(ECS_TIMER_DURATION_MS) >= SHADER_LAZY_RELOAD_MS) {
			unsigned int current_handle = GetShaderHandle(data, (ORDER)index);

			if (current_handle != -1) {
				Stream<wchar_t> shader_file = editor_state->asset_database->GetAssetPath(current_handle, ECS_ASSET_SHADER);

				ECS_STACK_CAPACITY_STREAM(wchar_t, shader_path_storage, 512);
				Stream<wchar_t> shader_path = function::MountPathOnlyRel(shader_file, assets_folder, shader_path_storage);
				size_t new_shader_stamp = OS::GetFileLastWrite(shader_path);

				if (data->shader_stamp[index] == 0 || data->shader_stamp[index] == -1 || new_shader_stamp > data->shader_stamp[index]) {
					bool read_success = reload_shader(shader_path, (ORDER)index);
					if (!read_success) {
						data->shader_stamp[index] == -1;
					}
					else {
						data->shader_stamp[index] = new_shader_stamp;
					}
				}
			}
			data->timer[index].SetNewStart();
		}
	}
}

// ------------------------------------------------------------------------------------------------------------

void InspectorCleanMaterial(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	InspectorDrawMaterialFileData* data = (InspectorDrawMaterialFileData*)_data;

	AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
	data->material_asset.DeallocateMemory(editor_allocator);
	editor_state->module_reflection->DeallocateFieldOverride(VERTEX_TAG, data->shader_override_data[VERTEX_ORDER]);
	editor_state->module_reflection->DeallocateFieldOverride(PIXEL_TAG, data->shader_override_data[PIXEL_ORDER]);

	for (size_t index = 0; index < ORDER_COUNT; index++) {
		ORDER order = (ORDER)index;
		DeallocateCBuffers(data, inspector_index, order);
		DeleteTextures(data, order);
		DeleteSamplers(data, order);

		// Deallocate the allocator
		for (size_t subindex = 0; subindex < 2; subindex++) {
			data->shader_cbuffer_allocator[index][subindex].Free();
		}
	}
}

// ------------------------------------------------------------------------------------------------------------

void InspectorDrawMaterialFile(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	InspectorDrawMaterialFileData* data = (InspectorDrawMaterialFileData*)_data;

	// Check to see if the file still exists - else revert to draw nothing
	if (!ExistsFileOrFolder(data->path)) {
		ChangeInspectorToNothing(editor_state, inspector_index);
		return;
	}

	ReloadShaders(data, inspector_index);

	InspectorIconDouble(drawer, ECS_TOOLS_UI_TEXTURE_FILE_BLANK, ASSET_MATERIAL_ICON, drawer->color_theme.text, drawer->color_theme.theme);

	InspectorIconNameAndPath(drawer, data->path);
	InspectorDrawFileTimes(drawer, data->path);
	InspectorOpenAndShowButton(drawer, data->path);
	drawer->CrossLine();

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

	if (!IsMaterialFromMetadataLoadedEx(&data->material_asset, editor_state->RuntimeResourceManager(), editor_state->asset_database, &is_loaded_desc)) {
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
				name_padding.total_length = 0.25f;
				config.AddFlag(name_padding);
				UIConfigWindowDependentSize window_dependent;
				config.AddFlag(window_dependent);

				if (data->cbuffers[order].size > 0) {
					ECS_STACK_CAPACITY_STREAM(char, ui_instance_name, 512);

					unsigned int shader_handle = GetShaderHandle(data, order);
					for (size_t index = 0; index < data->cbuffers[order].size; index++) {
						ECS_STACK_CAPACITY_STREAM(char, ui_instance_name, 512);
						GetUITypeNameForCBuffer(editor_state, inspector_index, shader_handle, data->cbuffers[order].buffer + index, ui_instance_name);

						UIReflectionDrawInstanceOptions options;
						options.drawer = drawer;
						options.config = &config;
						options.global_configuration = GENERAL_CONFIGURATION;

						editor_state->ui_reflection->DrawInstance(ui_instance_name, &options);
					}
				}
				else if (!data->success[order][SUCCESS_CBUFFER]) {
					drawer->Text("Failed to reflect the constant buffers for this shader.");
					drawer->NextRow();
				}

				if (data->textures[order].size > 0) {
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
				else if (!data->success[order][SUCCESS_TEXTURE]) {
					drawer->Text("Failed to reflect the textures for this shader.");
					drawer->NextRow();
				}

				if (data->samplers[order].size > 0) {
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
				else if (!data->success[order][SUCCESS_SAMPLER]) {
					drawer->Text("Failed to reflect the samplers for this shader.");
					drawer->NextRow();
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
}

// ------------------------------------------------------------------------------------------------------------

void ChangeInspectorToMaterialFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int inspector_index) {
	InspectorDrawMaterialFileData data;
	data.path = path;
	data.editor_state = editor_state;
	memset(&data.material_asset, 0, sizeof(data.material_asset));

	// Allocate the data and embedd the path in it
	// Later on. It is fine to read from the stack more bytes
	inspector_index = ChangeInspectorDrawFunctionWithSearch(
		editor_state,
		inspector_index,
		{ InspectorDrawMaterialFile, InspectorCleanMaterial },
		&data,
		sizeof(data) + sizeof(wchar_t) * (path.size + 1),
		-1,
		[=](void* inspector_data) {
			InspectorDrawMaterialFileData* other_data = (InspectorDrawMaterialFileData*)inspector_data;
			return function::CompareStrings(other_data->path, path);
		}
	);

	if (inspector_index != -1) {
		// Get the data and set the path
		AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();
		
		InspectorDrawMaterialFileData* draw_data = (InspectorDrawMaterialFileData*)GetInspectorDrawFunctionData(editor_state, inspector_index);
		memset(draw_data->success, 0, sizeof(draw_data->success));
		draw_data->path = { function::OffsetPointer(draw_data, sizeof(*draw_data)), path.size };
		draw_data->path.Copy(path);
		draw_data->shader_override_data[VERTEX_ORDER] = editor_state->module_reflection->InitializeFieldOverride(VERTEX_TAG, "Vertex Shader");
		draw_data->shader_override_data[PIXEL_ORDER] = editor_state->module_reflection->InitializeFieldOverride(PIXEL_TAG, "Pixel Shader");

		for (size_t index = 0; index < ORDER_COUNT; index++) {
			draw_data->cbuffers[index].size = 0;
			draw_data->textures[index].size = 0;
			draw_data->samplers[index].size = 0;
			draw_data->shader_stamp[index] = 0;

			// Create the shader allocators
			draw_data->shader_cbuffer_allocator_index[index] = 0;
			for (size_t subindex = 0; subindex < 2; subindex++) {
				draw_data->shader_cbuffer_allocator[index][subindex] = MemoryManager(SHADER_ALLOCATOR_CAPACITY, 1024, SHADER_ALLOCATOR_CAPACITY, editor_state->GlobalMemoryManager());
			}
			draw_data->timer[index].DelayStart(-SHADER_LAZY_RELOAD_MS, ECS_TIMER_DURATION_MS);
		}

		AssetOverrideSetAllData set_all_data;
		set_all_data.set_index.sandbox_index = GetInspectorTargetSandbox(editor_state, inspector_index);
		set_all_data.callback = { BaseModifyCallback, draw_data, 0 };
		editor_state->module_reflection->BindInstanceFieldOverride(draw_data->shader_override_data[VERTEX_ORDER], VERTEX_TAG, AssetOverrideSetAll, &set_all_data);
		editor_state->module_reflection->BindInstanceFieldOverride(draw_data->shader_override_data[PIXEL_ORDER], PIXEL_TAG, AssetOverrideSetAll, &set_all_data);

		// Try to read the metadata file
		ECS_STACK_CAPACITY_STREAM(char, asset_name, 512);
		GetAssetNameFromThunkOrForwardingFile(editor_state, draw_data->path, asset_name);
		draw_data->material_asset.name = asset_name;

		// Retrieve the data from the file, if any
		bool success = editor_state->asset_database->ReadMaterialFile(asset_name, &draw_data->material_asset);
		if (!success) {
			// Set the default for the metadata
			draw_data->material_asset.Default(asset_name, { nullptr, 0 });
		}

		draw_data->material_asset.name = function::StringCopy(editor_state->EditorAllocator(), asset_name);
	}
}

// ------------------------------------------------------------------------------------------------------------

void InspectorMaterialFileAddFunctors(InspectorTable* table) {
	for (size_t index = 0; index < std::size(ASSET_MATERIAL_EXTENSIONS); index++) {
		AddInspectorTableFunction(table, { InspectorDrawMaterialFile, InspectorCleanMaterial }, ASSET_MATERIAL_EXTENSIONS[index]);
	}
}

// ------------------------------------------------------------------------------------------------------------