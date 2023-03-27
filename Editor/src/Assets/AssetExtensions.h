#pragma once

inline const wchar_t* ASSET_MESH_EXTENSIONS[] = {
	L".glb",
	L".gltf"
};

inline const wchar_t* ASSET_TEXTURE_EXTENSIONS[] = {
	L".jpg",
	L".png",
	L".tga",
	L".bmp",
	L".tiff",
	L".hdr"
};

inline const wchar_t* ASSET_GPU_SAMPLER_EXTENSIONS[] = {
	L".sampler"
};

inline const wchar_t* ASSET_SHADER_EXTENSIONS[] = {
	L".shader"
};

inline const wchar_t* ASSET_MATERIAL_EXTENSIONS[] = {
	L".mat"
};

inline const wchar_t* ASSET_MISC_EXTENSIONS[] = {
	L".misc"
};

inline Stream<const wchar_t*> ASSET_EXTENSIONS[] = {
	{ ASSET_MESH_EXTENSIONS, std::size(ASSET_MESH_EXTENSIONS) },
	{ ASSET_TEXTURE_EXTENSIONS, std::size(ASSET_TEXTURE_EXTENSIONS) },
	{ ASSET_GPU_SAMPLER_EXTENSIONS, std::size(ASSET_GPU_SAMPLER_EXTENSIONS) },
	{ ASSET_SHADER_EXTENSIONS, std::size(ASSET_SHADER_EXTENSIONS) },
	{ ASSET_MATERIAL_EXTENSIONS, std::size(ASSET_MATERIAL_EXTENSIONS) },
	{ ASSET_MISC_EXTENSIONS, std::size(ASSET_MISC_EXTENSIONS) }
};