#pragma once
#include "../Core.h"
#include "RenderingStructures.h"

namespace ECSEngine {

	struct Graphics;

	// Returns the value stored at the given texel position from a texture on the GPU
	// If out of bounds reads are allowed, it will return DBL_MAX in all channels
	// If not allowed, it will assert false
	// It needs the immediate context of the graphics
	// You can specify an override format (useful for typeless textures)
	ECSENGINE_API double4 SampleTexture(
		Graphics* graphics, 
		Texture2D texture, 
		uint2 texel_position,
		ECS_GRAPHICS_FORMAT override_format = ECS_GRAPHICS_FORMAT_UNKNOWN,
		unsigned int mip_level = 0, 
		bool allow_out_of_bounds_reads = true
	);

}