#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	struct WriteJPGTextureOptions {
		float compression_quality = 0.8f;
		bool srgb = false;
	};

	// Returns true if it succeeded, else false
	// By default, it assumes that the data is in RGBA format, even tho there is no alpha written 
	// (that's what the writer expects). You can force no alpha, but this will make a temporary
	// conversion where the data is converted to 4 channels
	ECSENGINE_API bool WriteJPGTexture(
		Stream<wchar_t> path, 
		const void* texture_data, 
		uint2 dimensions,
		size_t channel_count = 4,
		WriteJPGTextureOptions options = {}
	);

}