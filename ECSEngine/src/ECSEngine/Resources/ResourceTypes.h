#pragma once
#include "../Core.h"

namespace ECSEngine {

	enum class ResourceType : unsigned char {
		Texture,
		TextFile,
		Mesh,
		CoalescedMesh,
		PBRMaterial,
		PBRMesh,
		Shader,
		Misc,
		TimeStamp,
		TypeCount
	};

	constexpr const char* ECS_RESOURCE_TYPE_NAMES[] = {
		"Texture",
		"TextFile",
		"Mesh",
		"CoalescedMesh",
		"PBRMaterial",
		"PBRMesh",
		"Shader",
		"Misc",
		"TimeStamp"
	};
	static_assert(ECS_COUNTOF(ECS_RESOURCE_TYPE_NAMES) == (unsigned int)ResourceType::TypeCount);

	ECS_INLINE const char* ResourceTypeString(ResourceType type) {
		return ECS_RESOURCE_TYPE_NAMES[(unsigned int)type];
	}

	enum TextureExtension {
		ECS_TEXTURE_EXTENSION_JPG,
		ECS_TEXTURE_EXTENSION_PNG,
		ECS_TEXTURE_EXTENSION_TIFF,
		ECS_TEXTURE_EXTENSION_BMP,
		ECS_TEXTURE_EXTENSION_TGA,
		ECS_TEXTURE_EXTENSION_HDR,
		ECS_TEXTURE_EXTENSION_COUNT
	};

	constexpr const wchar_t* ECS_TEXTURE_EXTENSIONS[] = {
		L".jpg",
		L".png",
		L".tiff",
		L".bmp",
		L".tga",
		L".hdr"
	};

	static_assert(ECS_COUNTOF(ECS_TEXTURE_EXTENSIONS) == ECS_TEXTURE_EXTENSION_COUNT);

	constexpr const char* ECS_TEXTURE_EXTENSIONS_ASCII[] = {
		".jpg",
		".png",
		".tiff",
		".bmp",
		".tga",
		".hdr"
	};

	static_assert(ECS_COUNTOF(ECS_TEXTURE_EXTENSIONS_ASCII) == ECS_TEXTURE_EXTENSION_COUNT);

}
