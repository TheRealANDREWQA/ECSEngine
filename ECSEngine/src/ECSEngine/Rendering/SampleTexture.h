#pragma once
#include "../Core.h"
#include "RenderingStructures.h"

namespace ECSEngine {

	struct Graphics;

	// Returns the value stored at the given texel position from a texture on the GPU
	// It asserts that the pixel is in the bounds of the texture
	// It needs the immediate context of the graphics
	// You can specify an override format (useful for typeless textures)
	ECSENGINE_API double4 SampleTexture(
		Graphics* graphics, 
		Texture2D texture, 
		uint2 texel_position,
		ECS_GRAPHICS_FORMAT override_format = ECS_GRAPHICS_FORMAT_UNKNOWN,
		unsigned int mip_level = 0
	);

	// This is the same as the other variant, but retrieves the values
	// for a rectangular selection. The bottom right is not included, so
	// top_left == bottom_right doesn't actually copy anything
	// It asserts that the selection is in the bounds of the texture
	ECSENGINE_API void SampleTexture(
		Graphics* graphics,
		Texture2D texture,
		uint2 top_left,
		uint2 bottom_right,
		double4* values,
		ECS_GRAPHICS_FORMAT override_format = ECS_GRAPHICS_FORMAT_UNKNOWN,
		unsigned int mip_level = 0
	);

}