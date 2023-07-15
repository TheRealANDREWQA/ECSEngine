#include "ecspch.h"
#include "VisualizeTexture.h"
#include "Graphics.h"
#include "GraphicsHelpers.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetTextureVisualizeFormat(ECS_GRAPHICS_FORMAT format, bool keep_channel_width, VisualizeFormatConversion* conversion)
	{
		auto set_conversion = [&](float offset, float normalize_factor, unsigned int shader_helper) {
			if (conversion != nullptr) {
				conversion->offset = offset;
				conversion->normalize_factor = normalize_factor;
				conversion->shader_helper_index = shader_helper;
			}
		};

		auto set_conversion_integer = [&](bool negative, float normalize_factor) {
			set_conversion(negative ? 0.5f : 0.0f, normalize_factor, negative ? ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_SINT : ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_UINT);
		};

		bool is_unorm_compatible = IsGraphicsFormatFloatCompatible(format, false);
		if (is_unorm_compatible) {
			// If it is a SNORM format, we need to record the offset
			bool is_snorm = IsGraphicsFormatSNORM(format);
			set_conversion(is_snorm ? 0.5f : 0.0f, 1.0f, ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_FLOAT);

			if (!keep_channel_width) {
				// Convert the texture into a 4 channel texture
				return GetGraphicsFormatChannelCount(format, 4);
			}

			return format;
		}
		
		// Now for float textures - we can't know the normalization factor - just set it to 1.0f
		switch (format) {
		case ECS_GRAPHICS_FORMAT_R16_FLOAT:
		{
			set_conversion(0.0f, 1.0f, ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_FLOAT);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA16_UNORM;
			}
			else {
				return ECS_GRAPHICS_FORMAT_R16_UNORM;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_RG16_FLOAT:
		{
			set_conversion(0.0f, 1.0f, ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_FLOAT);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA16_UNORM;
			}
			else {
				return ECS_GRAPHICS_FORMAT_RG16_UNORM;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_RGBA16_FLOAT:
		{
			set_conversion(0.0f, 1.0f, ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_FLOAT);
			return ECS_GRAPHICS_FORMAT_RGBA16_UNORM;
		}
		break;
		case ECS_GRAPHICS_FORMAT_R32_FLOAT:
		{
			set_conversion(0.0f, 1.0f, ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_FLOAT);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA32_FLOAT;
			}
			else {
				return format;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_RG32_FLOAT:
		{
			set_conversion(0.0f, 1.0f, ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_FLOAT);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA32_FLOAT;
			}
			else {
				return format;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_RGB32_FLOAT:
		{
			set_conversion(0.0f, 1.0f, ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_FLOAT);
			// Cannot preserve 3 channel texture as UNORM
			return ECS_GRAPHICS_FORMAT_RGBA32_FLOAT;
		}
		break;
		case ECS_GRAPHICS_FORMAT_RGBA32_FLOAT:
		{
			set_conversion(0.0f, 1.0f, ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_FLOAT);
			return ECS_GRAPHICS_FORMAT_RGBA32_FLOAT;
		}
		break;
		case ECS_GRAPHICS_FORMAT_R11G11B10_FLOAT:
		{
			set_conversion(0.0f, 1.0f, ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_FLOAT);
			return ECS_GRAPHICS_FORMAT_RGBA16_UNORM;
		}
		break;
		}

		// For depth textures there is a known conversion
		// The normalize factor can be left at 1.0f
		switch (format) {
		case ECS_GRAPHICS_FORMAT_D16_UNORM:
		{
			set_conversion(0.0f, 1.0f, ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_DEPTH);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA16_UNORM;
			}
			else {
				return ECS_GRAPHICS_FORMAT_R16_UNORM;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
		case ECS_GRAPHICS_FORMAT_D32_FLOAT:
		{
			set_conversion(0.0f, 1.0f, ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_DEPTH);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA32_FLOAT;
			}
			else {
				return ECS_GRAPHICS_FORMAT_R32_FLOAT;
			}
		}
		break;
		}

		// Now UINT and SINT textures
		switch (format) {
		case ECS_GRAPHICS_FORMAT_R8_UINT:
		case ECS_GRAPHICS_FORMAT_R8_SINT:
		{
			set_conversion_integer(format == ECS_GRAPHICS_FORMAT_R8_SINT, 1.0f / UCHAR_MAX);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
			}
			else {
				return ECS_GRAPHICS_FORMAT_R8_UNORM;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_RG8_UINT:
		case ECS_GRAPHICS_FORMAT_RG8_SINT:
		{
			set_conversion_integer(format == ECS_GRAPHICS_FORMAT_RG8_SINT, 1.0f / UCHAR_MAX);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
			}
			else {
				return ECS_GRAPHICS_FORMAT_RG8_UNORM;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_RGBA8_UINT:
		case ECS_GRAPHICS_FORMAT_RGBA8_SINT:
		{
			set_conversion_integer(format == ECS_GRAPHICS_FORMAT_RGBA8_SINT, 1.0f / UCHAR_MAX);
			return ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		}
		break;
		case ECS_GRAPHICS_FORMAT_R16_UINT:
		case ECS_GRAPHICS_FORMAT_R16_SINT:
		{
			set_conversion_integer(format == ECS_GRAPHICS_FORMAT_R16_SINT, 1.0f / USHORT_MAX);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA16_UNORM;
			}
			else {
				return ECS_GRAPHICS_FORMAT_R16_UNORM;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_RG16_UINT:
		case ECS_GRAPHICS_FORMAT_RG16_SINT:
		{
			set_conversion_integer(format == ECS_GRAPHICS_FORMAT_RG16_SINT, 1.0f / USHORT_MAX);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA16_UNORM;
			}
			else {
				return ECS_GRAPHICS_FORMAT_RG16_UNORM;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_RGBA16_UINT:
		case ECS_GRAPHICS_FORMAT_RGBA16_SINT:
		{
			set_conversion_integer(format == ECS_GRAPHICS_FORMAT_RGBA16_SINT, 1.0f / USHORT_MAX);
			return ECS_GRAPHICS_FORMAT_RGBA16_UNORM;
		}
		break;
		case ECS_GRAPHICS_FORMAT_R32_UINT:
		case ECS_GRAPHICS_FORMAT_R32_SINT:
		{
			set_conversion_integer(format == ECS_GRAPHICS_FORMAT_R32_SINT, 1.0f / UINT_MAX);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA32_FLOAT;
			}
			else {
				return ECS_GRAPHICS_FORMAT_R32_FLOAT;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_RG32_UINT:
		case ECS_GRAPHICS_FORMAT_RG32_SINT:
		{
			set_conversion_integer(format == ECS_GRAPHICS_FORMAT_RG32_SINT, 1.0f / UINT_MAX);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA32_FLOAT;
			}
			else {
				return ECS_GRAPHICS_FORMAT_RG32_FLOAT;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_RGB32_UINT:
		case ECS_GRAPHICS_FORMAT_RGB32_SINT:
		{
			set_conversion_integer(format == ECS_GRAPHICS_FORMAT_RGB32_SINT, 1.0f / UINT_MAX);
			if (!keep_channel_width) {
				return ECS_GRAPHICS_FORMAT_RGBA32_FLOAT;
			}
			else {
				return ECS_GRAPHICS_FORMAT_RGB32_FLOAT;
			}
		}
		break;
		case ECS_GRAPHICS_FORMAT_RGBA32_UINT:
		case ECS_GRAPHICS_FORMAT_RGBA32_SINT:
		{
			set_conversion_integer(format == ECS_GRAPHICS_FORMAT_RGBA32_SINT, 1.0f / UINT_MAX);
			return ECS_GRAPHICS_FORMAT_RGBA32_FLOAT;
		}
		break;
		}

		// Typeless and BC textures are left as is
		return format;
	}

	// ------------------------------------------------------------------------------------------------------------

	Texture2D ConvertTextureToVisualize(Graphics* graphics, Texture2D texture, const VisualizeTextureOptions* options)
	{
		Texture2D new_texture;
		
		ECS_GRAPHICS_FORMAT texture_format = ECS_GRAPHICS_FORMAT_UNKNOWN;
		ECS_GRAPHICS_FORMAT override_format = ECS_GRAPHICS_FORMAT_UNKNOWN;
		bool keep_channel_width = false;
		VisualizeFormatConversion conversion = { 0.0f, 1.0f, (unsigned int)-1 };
		bool temporary = false;
		bool copy_texture_if_same_format = false;

		if (options != nullptr) {
			texture_format = options->override_format;
			override_format = options->override_format;
			keep_channel_width = options->keep_channel_width;
			conversion = options->format_conversion;

			temporary = options->temporary;
			copy_texture_if_same_format = options->copy_texture_if_same_format;
		}

		// Get the format of the texture
		Texture2DDescriptor texture_descriptor = GetTextureDescriptor(texture);
		if (texture_format == ECS_GRAPHICS_FORMAT_UNKNOWN) {
			texture_format = texture_descriptor.format;
		}

		VisualizeFormatConversion* format_conversion = conversion.normalize_factor != 1.0f || conversion.offset != 0.0f ? nullptr : &conversion;
		ECS_GRAPHICS_FORMAT view_format = GetTextureVisualizeFormat(texture_format, keep_channel_width, format_conversion);

		// If the view format is the same as the current one then skip
		if (view_format == texture_format) {
			// Return the existing texture - if we don't need to copy
			if (copy_texture_if_same_format) {
				texture_descriptor.bind_flag = ECS_GRAPHICS_BIND_SHADER_RESOURCE;
				texture_descriptor.usage = ECS_GRAPHICS_USAGE_DEFAULT;
				texture_descriptor.cpu_flag = ECS_GRAPHICS_CPU_ACCESS_NONE;
				new_texture = graphics->CreateTexture(&texture_descriptor);

				CopyGraphicsResource(new_texture, texture, graphics->GetContext());
				texture = new_texture;
			}
			return texture;
		}

		Texture2DDescriptor new_texture_descriptor;
		// Create a new texture with the given format
		new_texture_descriptor.mip_levels = 1;
		new_texture_descriptor.mip_data = { nullptr, 0 };
		new_texture_descriptor.format = view_format;
		new_texture_descriptor.bind_flag = ECS_GRAPHICS_BIND_UNORDERED_ACCESS | ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		new_texture_descriptor.usage = ECS_GRAPHICS_USAGE_DEFAULT;
		new_texture_descriptor.cpu_flag = ECS_GRAPHICS_CPU_ACCESS_NONE;
		new_texture_descriptor.size = texture_descriptor.size;

		new_texture = graphics->CreateTexture(&new_texture_descriptor, temporary);
		ConvertTextureToVisualize(graphics, texture, new_texture, options);

		return new_texture;
	}

	// ------------------------------------------------------------------------------------------------------------

	void ConvertTextureToVisualize(Graphics* graphics, Texture2D target_texture, Texture2D visualize_texture, const VisualizeTextureOptions* options)
	{
		ECS_GRAPHICS_FORMAT texture_format = ECS_GRAPHICS_FORMAT_UNKNOWN;
		ECS_GRAPHICS_FORMAT override_format = ECS_GRAPHICS_FORMAT_UNKNOWN;
		bool keep_channel_width = false;
		VisualizeFormatConversion conversion = { 0.0f, 1.0f, (unsigned int)-1 };
		bool keep_red = true;
		bool keep_green = true;
		bool keep_blue = true;
		bool keep_alpha = false;

		if (options != nullptr) {
			texture_format = options->override_format;
			override_format = options->override_format;
			keep_channel_width = options->keep_channel_width;
			conversion = options->format_conversion;

			keep_red = options->enable_red;
			keep_green = options->enable_green;
			keep_blue = options->enable_blue;
			keep_alpha = options->enable_alpha;
		}

		if (texture_format == ECS_GRAPHICS_FORMAT_UNKNOWN) {
			Texture2DDescriptor target_descriptor = GetTextureDescriptor(target_texture);
			texture_format = target_descriptor.format;
		}

		VisualizeFormatConversion* format_conversion = conversion.normalize_factor != 1.0f || conversion.offset != 0.0f ? nullptr : &conversion;
		// We are interested only in the conversion
		GetTextureVisualizeFormat(texture_format, keep_channel_width, format_conversion);

		// Create a UA view for the output texture and a ResourceView for the input texture
		UAView ua_view = graphics->CreateUAView(visualize_texture, 0, ECS_GRAPHICS_FORMAT_UNKNOWN, true);

		// If the override format is a depth format, we need to convert that into a normal format
		override_format = ConvertDepthToRenderFormat(override_format);
		ResourceView input_view = graphics->CreateTextureShaderView(target_texture, override_format, 0, -1, true);

		ECS_ASSERT(conversion.shader_helper_index == ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_DEPTH
			|| conversion.shader_helper_index == ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_FLOAT
			|| conversion.shader_helper_index == ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_UINT
			|| conversion.shader_helper_index == ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_SINT);

		struct ComputeCBuffer {
			float4 offset;
			float4 normalize_factor;
			unsigned int keep_red;
			unsigned int keep_green;
			unsigned int keep_blue;
			unsigned int keep_alpha;
			unsigned int keep_channel;
			unsigned int channel_count;
		};

		ComputeCBuffer compute_cbuffer;
		compute_cbuffer.offset = float4::Splat(conversion.offset);
		compute_cbuffer.normalize_factor = float4::Splat(conversion.normalize_factor);
		compute_cbuffer.keep_red = keep_red;
		compute_cbuffer.keep_blue = keep_blue;
		compute_cbuffer.keep_green = keep_green;
		compute_cbuffer.keep_alpha = keep_alpha;
		compute_cbuffer.keep_channel = keep_channel_width;
		compute_cbuffer.channel_count = GetGraphicsFormatChannelCount(texture_format);

		ConstantBuffer compute_buffer = graphics->CreateConstantBuffer(sizeof(compute_cbuffer), &compute_cbuffer, true);
		graphics->BindComputeConstantBuffer(compute_buffer);
		graphics->BindComputeShader(graphics->m_shader_helpers[conversion.shader_helper_index].compute);
		graphics->BindComputeUAView(ua_view);
		graphics->BindComputeResourceView(input_view);
		graphics->Dispatch(visualize_texture, graphics->m_shader_helpers[conversion.shader_helper_index].compute_shader_dispatch_size);

		ua_view.Release();
		input_view.Release();
		compute_buffer.Release();
	}

	// ------------------------------------------------------------------------------------------------------------

}