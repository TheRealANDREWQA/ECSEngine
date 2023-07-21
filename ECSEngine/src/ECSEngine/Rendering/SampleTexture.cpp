#include "ecspch.h"
#include "SampleTexture.h"
#include "Graphics.h"
#include "GraphicsHelpers.h"

namespace ECSEngine {

	double4 SampleTexture(
		Graphics* graphics, 
		Texture2D texture, 
		uint2 texel_position, 
		ECS_GRAPHICS_FORMAT override_format, 
		unsigned int mip_level, 
		bool allow_out_of_bounds_reads
	)
	{
		Texture2DDescriptor descriptor = GetTextureDescriptor(texture);
		ECS_ASSERT(descriptor.mip_levels > mip_level);

		uint2 texture_size = uint2(descriptor.size.x >> mip_level, descriptor.size.y >> mip_level);
		texture_size.x = function::ClampMin(texture_size.x, (unsigned int)1);
		texture_size.y = function::ClampMin(texture_size.y, (unsigned int)1);

		if (allow_out_of_bounds_reads) {
			if (texel_position.x >= texture_size.x || texel_position.y >= texture_size.y) {
				return double4(DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX);
			}
		}
		else {
			ECS_ASSERT(texel_position.x < texture_size.x && texel_position.y < texture_size.y);
		}

		// Create a staging texture with a single pixel, copy into it from the main texture and then
		// read the value back to the CPU
		descriptor.usage = ECS_GRAPHICS_USAGE_STAGING;
		descriptor.size = { 1, 1 };
		descriptor.mip_levels = 1;
		descriptor.bind_flag = ECS_GRAPHICS_BIND_NONE;
		descriptor.misc_flag = ECS_GRAPHICS_MISC_NONE;
		descriptor.cpu_flag = ECS_GRAPHICS_CPU_ACCESS_READ;
		Texture2D temp_staging_texture = graphics->CreateTexture(&descriptor, true);

		CopyTextureSubresource(temp_staging_texture, { 0, 0 }, 0, texture, { texel_position.x, texel_position.y }, { 1, 1 }, mip_level, graphics->GetContext());
		void* pixel = graphics->MapTexture(temp_staging_texture, ECS_GRAPHICS_MAP_READ).data;

		// Now convert the pixel into a double4
		double4 result;
		ExtractPixelFromGraphicsFormat(override_format != ECS_GRAPHICS_FORMAT_UNKNOWN ? override_format : descriptor.format, 1, pixel, &result);

		graphics->UnmapTexture(temp_staging_texture);

		temp_staging_texture.Release();

		return result;
	}

}