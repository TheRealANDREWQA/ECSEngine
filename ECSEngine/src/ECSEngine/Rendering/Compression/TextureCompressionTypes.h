// ECS_REFLECT
#pragma once
#include "../../Containers/Stream.h"
#include "../../Utilities/Reflection/ReflectionMacros.h"

namespace ECSEngine {

	enum ECS_REFLECT ECS_TEXTURE_COMPRESSION : unsigned char {
		ECS_TEXTURE_COMPRESSION_NONE,
		ECS_TEXTURE_COMPRESSION_BC1,
		ECS_TEXTURE_COMPRESSION_BC3,
		ECS_TEXTURE_COMPRESSION_BC4,
		ECS_TEXTURE_COMPRESSION_BC5,
		ECS_TEXTURE_COMPRESSION_BC6,
		ECS_TEXTURE_COMPRESSION_BC7
	};

	enum ECS_REFLECT ECS_TEXTURE_COMPRESSION_EX : unsigned char {
		ECS_TEXTURE_COMPRESSION_EX_NONE,
		ECS_TEXTURE_COMPRESSION_EX_COLOR,
		ECS_TEXTURE_COMPRESSION_EX_CUTOUT,
		ECS_TEXTURE_COMPRESSION_EX_NORMAL_LOW_QUALITY,
		ECS_TEXTURE_COMPRESSION_EX_COLOR_WITH_ALPHA,
		ECS_TEXTURE_COMPRESSION_EX_GRAYSCALE,
		ECS_TEXTURE_COMPRESSION_EX_TANGENT_SPACE_NORMAL,
		ECS_TEXTURE_COMPRESSION_EX_DOUBLE_GRAYSCALE,
		ECS_TEXTURE_COMPRESSION_EX_HDR,
		ECS_TEXTURE_COMPRESSION_EX_COLOR_HIGH_QUALITY
	};

	enum ECS_TEXTURE_COMPRESS_FLAGS : unsigned char {
		ECS_TEXTURE_COMPRESS_NONE = 0,
		ECS_TEXTURE_COMPRESS_DISABLE_PERCEPTUAL_WEIGHTING = 1 << 0,
		// Works for BC1, BC3 and BC7
		ECS_TEXTURE_COMPRESS_SRGB = 1 << 1,
		// Adds to the bind flags the render target flag
		ECS_TEXTURE_COMPRESS_BIND_RENDER_TARGET = 1 << 2,
		ECS_TEXTURE_COMPRESS_DISABLE_MULTICORE = 1 << 3
	};

	ECS_ENUM_BITWISE_OPERATIONS(ECS_TEXTURE_COMPRESS_FLAGS);

	// Returns the appropriate compression type for the given texture - based on the extension which will get translated into a DXGI_FORMAT
	// For color maps it will give back BC1 and for normal maps the low quality one (i.e. still BC1)
	// It cannot make distinction between 3 color and 4 color channel types - it won't give back ColorMapWithAlpha or CutoutMap
	// If it fails it will return an invalid ECS_TEXTURE_COMPRESSION_EX_NONE texture compression type
	ECSENGINE_API ECS_TEXTURE_COMPRESSION_EX GetTextureCompressionType(Stream<wchar_t> texture);

	// Returns the appropriate compression type for the given texture - based on the format
	// For color maps and normal maps it will give back BC1
	// It cannot make distinction between 3 color and 4 color channel types - it won't give back ColorMapWithAlpha or CutoutMap
	// If it fails it will return an invalid ECS_TEXTURE_COMPRESSION_EX_NONE texture compression type
	ECSENGINE_API ECS_TEXTURE_COMPRESSION_EX GetTextureCompressionType(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API ECS_GRAPHICS_FORMAT GetCompressedRenderFormat(ECS_TEXTURE_COMPRESSION compression, bool srgb = false);

	ECSENGINE_API ECS_GRAPHICS_FORMAT GetCompressedRenderFormat(ECS_TEXTURE_COMPRESSION_EX compression, bool srgb = false);

	ECS_INLINE bool IsTextureCompressionTypeValid(ECS_TEXTURE_COMPRESSION_EX compression_type) {
		return (unsigned int)compression_type <= (unsigned int)ECS_TEXTURE_COMPRESSION_EX_COLOR_HIGH_QUALITY;
	}

	ECS_INLINE bool IsTextureCompressionTypeValid(ECS_TEXTURE_COMPRESSION compression_type) {
		return (unsigned int)compression_type <= (unsigned int)ECS_TEXTURE_COMPRESSION_BC7;
	}

	ECS_INLINE bool IsCPUCodec(ECS_TEXTURE_COMPRESSION compression) {
		return compression == ECS_TEXTURE_COMPRESSION_BC1 || compression == ECS_TEXTURE_COMPRESSION_BC3 || compression == ECS_TEXTURE_COMPRESSION_BC4
			|| compression == ECS_TEXTURE_COMPRESSION_BC5;
	}

	ECS_INLINE bool IsGPUCodec(ECS_TEXTURE_COMPRESSION compression) {
		return compression == ECS_TEXTURE_COMPRESSION_BC6 || compression == ECS_TEXTURE_COMPRESSION_BC7;
	}

}