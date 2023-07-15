#pragma once
#include "../Core.h"
#include "RenderingStructures.h"

namespace ECSEngine {

	struct Graphics;

	struct VisualizeFormatConversion {
		float offset;
		float normalize_factor;

		// This can be filled in with GetTextureVisualizeFormat
		unsigned int shader_helper_index;
	};

	struct VisualizeTextureOptions {
		bool enable_red = true;
		bool enable_green = true;
		bool enable_blue = true;
		bool enable_alpha = false;

		// This is used to control if channel splatting is performed
		bool keep_channel_width = false;
		// This can be used for typeless texture to give their actual format type
		ECS_GRAPHICS_FORMAT override_format = ECS_GRAPHICS_FORMAT_UNKNOWN;
		bool temporary = false;
		bool copy_texture_if_same_format = false;

		// Can override thse values such that they are used instead
		// The formula for calculating the pixel value is
		// out = input * normalize_factor + offset
		// Can be used for FLOAT formats for example (for the rest let the system decide automatically)
		VisualizeFormatConversion format_conversion = { 0.0f, 1.0f, (unsigned int)-1 };
	};

	// Returns a texture suitable to be viewed as a normalized float for normal color texture (existing floating point
	// textures are not converted since we cannot know the conversion rules - but you can provide one)
	// or R32_FLOAT/R16_FLOAT for depth texture. Can optionally give a pointer to a float
	// to give back a normalization factor that can be used to normalize the value into a UNORM
	// The normalize factor is actually 1.0f / range, so it can be used to multiply the value directly
	ECSENGINE_API ECS_GRAPHICS_FORMAT GetTextureVisualizeFormat(ECS_GRAPHICS_FORMAT format, bool keep_channel_width, VisualizeFormatConversion* conversion = nullptr);

	// Convert a texture into a suitable format to be viewed as normalized float values (RGBA8_UNORM)
	// For depth textures, it will result in a R32_FLOAT texture type or R16_UNORM for the 16 bit variant
	// Can optionally choose which channels to be enabled
	ECSENGINE_API Texture2D ConvertTextureToVisualize(
		Graphics* graphics, 
		Texture2D texture,
		const VisualizeTextureOptions* options = nullptr
	);

	// This just executes the conversion shader
	ECSENGINE_API void ConvertTextureToVisualize(
		Graphics* graphics,
		Texture2D target_texture,
		Texture2D visualize_texture,
		const VisualizeTextureOptions* options = nullptr
	);


}