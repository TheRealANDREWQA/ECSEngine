#include "editorpch.h"
#include "AssetBuiltins.h"
#include "AssetExtensions.h"
#include "AssetManagement.h"
#include "../Project/ProjectFolders.h"
#include "../Editor/EditorState.h"

#define ECS_VERTEX_PBR_SOURCE ECS_VERTEX_SHADER_SOURCE(PBR)
#define ECS_PIXEL_PBR_SOURCE ECS_PIXEL_SHADER_SOURCE(PBR)

Stream<char> EDITOR_SHADER_BUILTIN_NAME[EDITOR_SHADER_BUILTIN_COUNT + 1] = {
	"Vertex PBR",
	"Pixel PBR",
	"Custom"
};

ECS_SHADER_TYPE EDITOR_SHADER_BUILTIN_TYPE[EDITOR_SHADER_BUILTIN_COUNT + 1] = {
	ECS_SHADER_VERTEX,
	ECS_SHADER_PIXEL,
	ECS_SHADER_TYPE_COUNT
};

Stream<char> EDITOR_MATERIAL_BUILTIN_NAME[EDITOR_MATERIAL_BUILTIN_COUNT + 1] = {
	"PBR",
	"Custom"
};

EDITOR_SHADER_BUILTIN FindShaderBuiltinIndex(const ShaderMetadata* shader)
{
	if (shader->file == ECS_VERTEX_PBR_SOURCE) {
		return EDITOR_SHADER_BUILTIN_VERTEX_PBR;
	}

	if (shader->file == ECS_PIXEL_PBR_SOURCE) {
		return EDITOR_SHADER_BUILTIN_PIXEL_PBR;
	}

	return EDITOR_SHADER_BUILTIN_COUNT;
}

void FillShaderBuiltin(EDITOR_SHADER_BUILTIN builtin_index, const ShaderMetadata* current_metadata, ShaderMetadata* builtin_metadata, AllocatorPolymorphic temporary_allocator)
{
	ECS_ASSERT(builtin_index < EDITOR_SHADER_BUILTIN_COUNT);

	builtin_metadata->name = current_metadata->name;
	builtin_metadata->compile_flag = ECS_SHADER_COMPILE_NONE;

	ShaderMacro* macros = (ShaderMacro*)Allocate(temporary_allocator, sizeof(ShaderMacro) * 3);
	switch (builtin_index) {
	case EDITOR_SHADER_BUILTIN_VERTEX_PBR:
	{
		builtin_metadata->file = ECS_VERTEX_PBR_SOURCE;
		builtin_metadata->macros = {};
		builtin_metadata->shader_type = ECS_SHADER_VERTEX;
	}
	break;
	case EDITOR_SHADER_BUILTIN_PIXEL_PBR:
	{
		builtin_metadata->file = ECS_PIXEL_PBR_SOURCE;
		macros[0] = { "COLOR_TEXTURE", "" };
		macros[1] = { "NORMAL_TEXTURE", "" };
		macros[2] = { "ROUGHNESS_TEXTURE", "" };
		builtin_metadata->macros = { macros, 3 };
		builtin_metadata->shader_type = ECS_SHADER_PIXEL;
	}
	break;
	}
}

void SetShaderBuiltin(
	const EditorState* editor_state, 
	EDITOR_SHADER_BUILTIN builtin_index, 
	const ShaderMetadata* shader_metadata,
	ShaderMetadata* builtin_metadata,
	AllocatorPolymorphic temporary_allocator
)
{
	FillShaderBuiltin(builtin_index, shader_metadata, builtin_metadata, temporary_allocator);

	bool success = editor_state->asset_database->WriteShaderFile(builtin_metadata);
	if (!success) {
		EditorSetConsoleError("Failed to change the shader to builtin");
	}
}

EDITOR_MATERIAL_BUILTIN FindMaterialBuiltinIndex(const AssetDatabase* database, const MaterialAsset* material)
{
	if (material->vertex_shader_handle != -1 && material->pixel_shader_handle != -1) {
		const ShaderMetadata* vertex_metadata = database->GetShaderConst(material->vertex_shader_handle);
		const ShaderMetadata* pixel_metadata = database->GetShaderConst(material->pixel_shader_handle);
		if (FindShaderBuiltinIndex(vertex_metadata) == EDITOR_SHADER_BUILTIN_VERTEX_PBR && FindShaderBuiltinIndex(pixel_metadata) == EDITOR_SHADER_BUILTIN_PIXEL_PBR) {
			return EDITOR_MATERIAL_BUILTIN_PBR;
		}
	}

	return EDITOR_MATERIAL_BUILTIN_COUNT;
}

AssetDatabase* FillMaterialBuiltin(
	const EditorState* editor_state, 
	EDITOR_MATERIAL_BUILTIN builtin_index, 
	const MaterialAsset* current_material, 
	MaterialAsset* output_material, 
	AllocatorPolymorphic temporary_allocator
)
{
	ECS_ASSERT(builtin_index < EDITOR_MATERIAL_BUILTIN_COUNT);

	AssetDatabase* database = (AssetDatabase*)Allocate(temporary_allocator, sizeof(AssetDatabase));
	*database = { temporary_allocator, editor_state->ui_reflection->reflection };
	database->SetFileLocation(editor_state->asset_database->metadata_file_location);

	output_material->name = current_material->name;
	for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
		output_material->buffers[index] = {};
		output_material->textures[index] = {};
		output_material->samplers[index] = {};
	}
	output_material->vertex_shader_handle = -1;
	output_material->pixel_shader_handle = -1;

	ShaderMacro macros[3];
	switch (builtin_index) {
	case EDITOR_MATERIAL_BUILTIN_PBR:
	{
		// Add the vertex and pixel pbr shaders to the temporary database
		ShaderMetadata vertex_shader;
		ShaderMetadata pixel_shader;

		// Create new shader files in the corresponding path
		ECS_STACK_CAPACITY_STREAM(char, vertex_name, 512);
		vertex_name.CopyOther(current_material->name);
		ConvertWideCharsToASCII(AssetExtensionFromShaderType(ECS_SHADER_VERTEX), vertex_name);

		ECS_STACK_CAPACITY_STREAM(wchar_t, wide_vertex_name, 512);
		ConvertASCIIToWide(wide_vertex_name, vertex_name);
		bool success = CreateShaderFile(editor_state, wide_vertex_name, ECS_SHADER_TYPE_COUNT);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to create the vertex shader file {#} for the PBR material preset", vertex_name);
			EditorSetConsoleError(error_message);
		}

		ECS_STACK_CAPACITY_STREAM(char, pixel_name, 512);
		pixel_name.CopyOther(current_material->name);
		ConvertWideCharsToASCII(AssetExtensionFromShaderType(ECS_SHADER_PIXEL), pixel_name);

		ECS_STACK_CAPACITY_STREAM(wchar_t, wide_pixel_name, 512);
		ConvertASCIIToWide(wide_pixel_name, pixel_name);
		success = CreateShaderFile(editor_state, wide_pixel_name, ECS_SHADER_TYPE_COUNT);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to create the pixel shader file {#} for the PBR material preset", pixel_name);
			EditorSetConsoleError(error_message);
		}

		// These shaders will reference these temporary names, but when they will be added internally,
		// they will use the allocator to make a copy
		ShaderMetadata temp_metadata;
		temp_metadata.name = vertex_name;
		FillShaderBuiltin(EDITOR_SHADER_BUILTIN_VERTEX_PBR, &temp_metadata, &vertex_shader, temporary_allocator);

		temp_metadata.name = pixel_name;
		FillShaderBuiltin(EDITOR_SHADER_BUILTIN_PIXEL_PBR, &temp_metadata, &pixel_shader, temporary_allocator);

		output_material->vertex_shader_handle = database->AddAssetInternal(&vertex_shader, ECS_ASSET_SHADER);
		output_material->pixel_shader_handle = database->AddAssetInternal(&pixel_shader, ECS_ASSET_SHADER);

		success = database->WriteShaderFile(&vertex_shader);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to write metadata file for vertex shader {#} for the PBR material preset", vertex_name);
			EditorSetConsoleError(error_message);
		}
		success = database->WriteShaderFile(&pixel_shader);
		if (!success) {
			ECS_FORMAT_TEMP_STRING(error_message, "Failed to write metadata file for pixel shader {#} for the PBR material preset", pixel_name);
		}
	}
	break;
	}

	return database;
}

void SetMaterialBuiltin(
	const EditorState* editor_state,
	EDITOR_MATERIAL_BUILTIN builtin_index,
	const MaterialAsset* material_asset,
	MaterialAsset* builtin_asset,
	AllocatorPolymorphic temporary_allocator
)
{
	AssetDatabase* temporary_database = FillMaterialBuiltin(editor_state, builtin_index, material_asset, builtin_asset, temporary_allocator);

	bool success = temporary_database->WriteMaterialFile(builtin_asset);
	if (!success) {
		EditorSetConsoleError("Failed to change the material to builtin");
	}
}

void SetAssetBuiltin(
	const EditorState* editor_state, 
	unsigned char builtin_index, 
	const void* asset, 
	ECS_ASSET_TYPE type, 
	void* builtin_asset_storage, 
	AllocatorPolymorphic temporary_allocator
)
{
	ECS_ASSERT(type == ECS_ASSET_SHADER || type == ECS_ASSET_MATERIAL);
	
	switch (type) {
	case ECS_ASSET_SHADER:
	{
		SetShaderBuiltin(editor_state, (EDITOR_SHADER_BUILTIN)builtin_index, (const ShaderMetadata*)asset, (ShaderMetadata*)builtin_asset_storage, temporary_allocator);
	}
	break;
	case ECS_ASSET_MATERIAL:
	{
		SetMaterialBuiltin(editor_state, (EDITOR_MATERIAL_BUILTIN)builtin_index, (const MaterialAsset*)asset, (MaterialAsset*)builtin_asset_storage, temporary_allocator);
	}
	break;
	}
}
