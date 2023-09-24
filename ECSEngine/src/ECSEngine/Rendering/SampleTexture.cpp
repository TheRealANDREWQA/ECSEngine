#include "ecspch.h"
#include "SampleTexture.h"
#include "Graphics.h"
#include "TextureOperations.h"

namespace ECSEngine {

	double4 SampleTexture(
		Graphics* graphics, 
		Texture2D texture, 
		uint2 texel_position, 
		ECS_GRAPHICS_FORMAT override_format, 
		unsigned int mip_level
	)
	{
		double4 result;
		SampleTexture(
			graphics,
			texture,
			texel_position,
			texel_position + uint2(1, 1),
			&result,
			override_format,
			mip_level
		);
		return result;
	}

	void SampleTexture(
		Graphics* graphics, 
		Texture2D texture, 
		uint2 top_left, 
		uint2 bottom_right, 
		double4* values, 
		ECS_GRAPHICS_FORMAT override_format, 
		unsigned int mip_level
	)
	{
		uint2 selection_size = bottom_right - top_left;
		selection_size.x++;
		selection_size.y++;

		Texture2DDescriptor descriptor = GetTextureDescriptor(texture);
		ECS_ASSERT(descriptor.mip_levels > mip_level);

		uint2 texture_size = uint2(descriptor.size.x >> mip_level, descriptor.size.y >> mip_level);
		texture_size.x = function::ClampMin(texture_size.x, (unsigned int)1);
		texture_size.y = function::ClampMin(texture_size.y, (unsigned int)1);

		ECS_ASSERT(top_left.x < texture_size.x && top_left.y < texture_size.y);
		ECS_ASSERT(bottom_right.x <= texture_size.x && bottom_right.y <= texture_size.y);

		// Create a staging texture with a single pixel, copy into it from the main texture and then
		// read the value back to the CPU
		descriptor.usage = ECS_GRAPHICS_USAGE_STAGING;
		descriptor.size = selection_size;
		descriptor.mip_levels = 1;
		descriptor.bind_flag = ECS_GRAPHICS_BIND_NONE;
		descriptor.misc_flag = ECS_GRAPHICS_MISC_NONE;
		descriptor.cpu_flag = ECS_GRAPHICS_CPU_ACCESS_READ;
		Texture2D temp_staging_texture = graphics->CreateTexture(&descriptor, true);

		CopyTextureSubresource(temp_staging_texture, { 0, 0 }, 0, texture, top_left, selection_size, mip_level, graphics->GetContext());
		void* pixels = graphics->MapTexture(temp_staging_texture, ECS_GRAPHICS_MAP_READ).data;

		ExtractPixelFromGraphicsFormat(
			override_format != ECS_GRAPHICS_FORMAT_UNKNOWN ? override_format : descriptor.format,
			(size_t)selection_size.x * (size_t)selection_size.y,
			pixels,
			values
		);

		graphics->UnmapTexture(temp_staging_texture);
		temp_staging_texture.Release();
	}

}