#include "editorpch.h"
#include "AssetExtensions.h"

using namespace ECSEngine;

void GetAssetExtensionsWithThunkOrForwardingFile(CapacityStream<Stream<wchar_t>>& extensions)
{
	extensions.AddStreamAssert({ ASSET_GPU_SAMPLER_EXTENSIONS, std::size(ASSET_GPU_SAMPLER_EXTENSIONS) });
	extensions.AddStreamAssert({ ASSET_SHADER_EXTENSIONS, std::size(ASSET_SHADER_EXTENSIONS) });
	extensions.AddStreamAssert({ ASSET_MATERIAL_EXTENSIONS, std::size(ASSET_MATERIAL_EXTENSIONS) });
	extensions.AddStreamAssert({ ASSET_MISC_EXTENSIONS, std::size(ASSET_MISC_EXTENSIONS) });
}
