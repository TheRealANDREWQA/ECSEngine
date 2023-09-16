#pragma once
#include "ECSEngineContainers.h"

inline ECSEngine::Stream<wchar_t> ASSET_MESH_EXTENSIONS[] = {
	L".glb",
	L".gltf"
};

inline ECSEngine::Stream<wchar_t> ASSET_TEXTURE_EXTENSIONS[] = {
	L".jpg",
	L".png",
	L".tga",
	L".bmp",
	L".tiff",
	L".hdr"
};

inline ECSEngine::Stream<wchar_t> ASSET_GPU_SAMPLER_EXTENSIONS[] = {
	L".sampler"
};

inline ECSEngine::Stream<wchar_t> ASSET_SHADER_EXTENSIONS[] = {
	L".shader",
	L".vshader",
	L".pshader",
	L".dshader",
	L".hshader",
	L".gshader",
	L".cshader"
};

namespace ECSEngine {
	enum ECS_SHADER_TYPE : unsigned char;
}

// Returns ECS_SHADER_TYPE_COUNT if it doesn't match a particular shader type
inline ECSEngine::ECS_SHADER_TYPE AssetExtensionTypeShader(ECSEngine::Stream<wchar_t> string) {
	for (size_t index = 1; index < std::size(ASSET_SHADER_EXTENSIONS); index++) {
		if (string == ASSET_SHADER_EXTENSIONS[index]) {
			return (ECSEngine::ECS_SHADER_TYPE)(index - 1);
		}
	}
	return (ECSEngine::ECS_SHADER_TYPE)(std::size(ASSET_SHADER_EXTENSIONS) - 1);
}

inline ECSEngine::Stream<wchar_t> AssetExtensionFromType(ECSEngine::ECS_SHADER_TYPE type) {
	if ((unsigned int)type == std::size(ASSET_SHADER_EXTENSIONS) - 1) {
		return ASSET_SHADER_EXTENSIONS[0];
	}
	return ASSET_SHADER_EXTENSIONS[(unsigned int)type + 1];
}

inline ECSEngine::Stream<wchar_t> ASSET_MATERIAL_EXTENSIONS[] = {
	L".mat"
};

inline ECSEngine::Stream<wchar_t> ASSET_MISC_EXTENSIONS[] = {
	L".misc"
};

inline ECSEngine::Stream<ECSEngine::Stream<wchar_t>> ASSET_EXTENSIONS[] = {
	{ ASSET_MESH_EXTENSIONS, std::size(ASSET_MESH_EXTENSIONS) },
	{ ASSET_TEXTURE_EXTENSIONS, std::size(ASSET_TEXTURE_EXTENSIONS) },
	{ ASSET_GPU_SAMPLER_EXTENSIONS, std::size(ASSET_GPU_SAMPLER_EXTENSIONS) },
	{ ASSET_SHADER_EXTENSIONS, std::size(ASSET_SHADER_EXTENSIONS) },
	{ ASSET_MATERIAL_EXTENSIONS, std::size(ASSET_MATERIAL_EXTENSIONS) },
	{ ASSET_MISC_EXTENSIONS, std::size(ASSET_MISC_EXTENSIONS) }
};

// Fills in the extensions for those assets that have thunk or forwarding file
void GetAssetExtensionsWithThunkOrForwardingFile(ECSEngine::CapacityStream<ECSEngine::Stream<wchar_t>>& extensions);