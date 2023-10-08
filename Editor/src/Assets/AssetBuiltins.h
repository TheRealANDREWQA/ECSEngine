#pragma once
#include "ECSEngineAssets.h"

using namespace ECSEngine;

enum EDITOR_SHADER_BUILTIN : unsigned char {
	EDITOR_SHADER_BUILTIN_VERTEX_PBR,
	EDITOR_SHADER_BUILTIN_PIXEL_PBR,
	EDITOR_SHADER_BUILTIN_COUNT
};

enum EDITOR_MATERIAL_BUILTIN : unsigned char {
	EDITOR_MATERIAL_BUILTIN_PBR,
	EDITOR_MATERIAL_BUILTIN_COUNT
};

// There are +1 entries - the COUNT entry is set to "Custom" - this is primarly intended to be used for a combo box
extern Stream<char> EDITOR_SHADER_BUILTIN_NAME[EDITOR_SHADER_BUILTIN_COUNT + 1];
// This is parallel to EDITOR_SHADER_BUILTIN_NAME
extern ECS_SHADER_TYPE EDITOR_SHADER_BUILTIN_TYPE[EDITOR_SHADER_BUILTIN_COUNT + 1];
// There are +1 entries - the COUNT entry is set to "Custom" - this is primarly intended to be used for a combo box
extern Stream<char> EDITOR_MATERIAL_BUILTIN_NAME[EDITOR_MATERIAL_BUILTIN_COUNT + 1];

struct EditorState;

// Returns COUNT if it doesn't find it
EDITOR_SHADER_BUILTIN FindShaderBuiltinIndex(const ShaderMetadata* shader);

void FillShaderBuiltin(
	EDITOR_SHADER_BUILTIN builtin_index,
	const ShaderMetadata* current_metadata,
	ShaderMetadata* builtin_metadata,
	AllocatorPolymorphic temporary_allocator
);

// This will modify the metadata file and give you back the shader metadata to make any changes you see fit
void SetShaderBuiltin(
	EditorState* editor_state,
	EDITOR_SHADER_BUILTIN builtin_index,
	const ShaderMetadata* shader_metadata,
	ShaderMetadata* builtin_metadata,
	AllocatorPolymorphic temporary_allocator
);

// Returns COUNT if it doesn't find it
EDITOR_MATERIAL_BUILTIN FindMaterialBuiltinIndex(const AssetDatabase* database, const MaterialAsset* material);

// Returns a temporary database that has the references of the this material
// It is allocated from the temporary allocator
AssetDatabase* FillMaterialBuiltin(
	const EditorState* editor_state,
	EDITOR_MATERIAL_BUILTIN builtin_index,
	const MaterialAsset* current_material,
	MaterialAsset* output_material,
	AllocatorPolymorphic temporary_allocator
);

// This will modify the metadata file and the automatic detection of modifications will take care
// That the asset will then be correctly changed
void SetMaterialBuiltin(
	const EditorState* editor_state,
	EDITOR_MATERIAL_BUILTIN builtin_index,
	const MaterialAsset* material_asset,
	MaterialAsset* builtin_asset,
	AllocatorPolymorphic temporary_allocator
);