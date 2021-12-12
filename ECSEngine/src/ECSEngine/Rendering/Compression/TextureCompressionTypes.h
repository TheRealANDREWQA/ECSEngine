#pragma once

namespace ECSEngine {

	enum class TextureCompression : unsigned char {
		BC1,
		BC3,
		BC4,
		BC5,
		BC6,
		BC7
	};

	enum class TextureCompressionExplicit : unsigned char {
		ColorMap,
		CutoutMap,
		NormalMapLowQuality,
		ColorMapWithAlpha,
		GrayscaleMap,
		TangentSpaceNormalMap,
		DoubleGrayscaleMap,
		HDRMap,
		ColorMapHighQuality
	};

	constexpr size_t ECS_TEXTURE_COMPRESSION_DISABLE_PERCEPTUAL_WEIGHTING = 1 << 0;

}