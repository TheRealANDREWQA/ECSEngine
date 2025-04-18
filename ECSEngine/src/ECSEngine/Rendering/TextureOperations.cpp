#include "ecspch.h"
#include "TextureOperations.h"
#include "Graphics.h"
#include "GraphicsHelpers.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../Utilities/Path.h"
#include "../Utilities/Crash.h"

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int GetTextureDimensions(Texture1D texture, unsigned int mip_level)
	{
		unsigned int result;

		Texture1DDescriptor descriptor = GetTextureDescriptor(texture);
		ECS_ASSERT(mip_level < descriptor.mip_levels);
		result = ClampMin<unsigned int>(descriptor.width << mip_level, 1);

		return result;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint2 GetTextureDimensions(Texture2D texture, unsigned int mip_level) {
		uint2 result;

		Texture2DDescriptor descriptor = GetTextureDescriptor(texture);
		ECS_ASSERT(mip_level < descriptor.mip_levels);
		result.x = ClampMin<unsigned int>(descriptor.size.x << mip_level, 1);
		result.y = ClampMin<unsigned int>(descriptor.size.y << mip_level, 1);

		return result;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint3 GetTextureDimensions(Texture3D texture, unsigned int mip_level)
	{
		uint3 result;

		Texture3DDescriptor descriptor = GetTextureDescriptor(texture);
		ECS_ASSERT(mip_level < descriptor.mip_levels);
		result.x = ClampMin<unsigned int>(descriptor.size.x << mip_level, 1);
		result.y = ClampMin<unsigned int>(descriptor.size.y << mip_level, 1);
		result.z = ClampMin<unsigned int>(descriptor.size.z << mip_level, 1);

		return result;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	GraphicsViewport GetGraphicsViewportForTexture(Texture2D texture, float min_depth, float max_depth)
	{
		GraphicsViewport viewport;

		D3D11_TEXTURE2D_DESC descriptor;
		texture.Interface()->GetDesc(&descriptor);

		viewport.width = descriptor.Width;
		viewport.height = descriptor.Height;
		viewport.min_depth = min_depth;
		viewport.max_depth = max_depth;

		return viewport;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ResizeTextureWithStaging(Graphics* graphics, Texture2D texture, size_t new_width, size_t new_height, size_t resize_flag, bool temporary)
	{
		Texture2D staging_texture = TextureToStaging(graphics, texture);
		MappedTexture first_mip = MapTexture(staging_texture, graphics->GetContext(), ECS_GRAPHICS_MAP_READ);
		Texture2D new_texture = ResizeTexture(graphics, first_mip.data, texture, new_width, new_height, ECS_MALLOC_ALLOCATOR, resize_flag, temporary);
		UnmapTexture(staging_texture, graphics->GetContext());
		staging_texture.Release();
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ResizeTextureWithMap(Graphics* graphics, Texture2D texture, size_t new_width, size_t new_height, size_t resize_flag, bool temporary)
	{
		Texture2D result;

		MappedTexture first_mip = MapTexture(texture, graphics->GetContext(), ECS_GRAPHICS_MAP_READ);
		result = ResizeTexture(graphics, first_mip.data, texture, new_width, new_height, ECS_MALLOC_ALLOCATOR, resize_flag, temporary);
		UnmapTexture(texture, graphics->GetContext());
		return result;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ResizeTexture(
		Graphics* graphics,
		void* data,
		Texture2D texture,
		size_t new_width,
		size_t new_height,
		AllocatorPolymorphic allocator,
		size_t resize_flag,
		bool temporary
	) {
		Texture2D new_texture = (ID3D11Texture2D*)nullptr;

		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		DirectX::Image dx_image;
		dx_image.pixels = (uint8_t*)data;
		dx_image.format = texture_descriptor.Format;
		dx_image.width = texture_descriptor.Width;
		dx_image.height = texture_descriptor.Height;
		HRESULT result = DirectX::ComputePitch(texture_descriptor.Format, texture_descriptor.Width, texture_descriptor.Height, dx_image.rowPitch, dx_image.slicePitch);
		if (FAILED(result)) {
			return Texture2D((ID3D11Texture2D*)nullptr);
		}

		DirectX::TEX_FILTER_FLAGS filter_flag = DirectX::TEX_FILTER_LINEAR;
		size_t filter_flags = ClearFlag(resize_flag, ECS_RESIZE_TEXTURE_MIP_MAPS);
		switch (filter_flags) {
		case ECS_RESIZE_TEXTURE_FILTER_BOX:
			filter_flag = DirectX::TEX_FILTER_BOX;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_CUBIC:
			filter_flag = DirectX::TEX_FILTER_CUBIC;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_LINEAR:
			filter_flag = DirectX::TEX_FILTER_LINEAR;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_POINT:
			filter_flag = DirectX::TEX_FILTER_POINT;
			break;
		}

		DirectX::ScratchImage new_image;
		if (allocator.allocator != nullptr) {
			SetInternalImageAllocator(&new_image, allocator);
		}
		result = DirectX::Resize(dx_image, new_width, new_height, filter_flag, new_image);
		if (FAILED(result)) {
			return new_texture;
		}

		const DirectX::Image* resized_image = new_image.GetImage(0, 0, 0);
		D3D11_SUBRESOURCE_DATA subresource_data;
		subresource_data.pSysMem = resized_image->pixels;
		subresource_data.SysMemPitch = resized_image->rowPitch;
		subresource_data.SysMemSlicePitch = resized_image->slicePitch;

		ID3D11Texture2D* _new_texture;
		texture_descriptor.MipLevels = 1;
		texture_descriptor.Width = new_width;
		texture_descriptor.Height = new_height;

		result = graphics->GetDevice()->CreateTexture2D(&texture_descriptor, &subresource_data, &_new_texture);
		if (FAILED(result)) {
			return new_texture;
		}
		if (HasFlag(resize_flag, ECS_RESIZE_TEXTURE_MIP_MAPS)) {
			ID3D11Texture2D* first_mip = _new_texture;
			texture_descriptor.MipLevels = 0;
			texture_descriptor.BindFlags |= D3D11_BIND_RENDER_TARGET;
			texture_descriptor.Usage = D3D11_USAGE_DEFAULT;
			texture_descriptor.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

			result = graphics->GetDevice()->CreateTexture2D(&texture_descriptor, nullptr, &_new_texture);
			if (FAILED(result)) {
				return new_texture;
			}

			CopyTextureSubresource(_new_texture, { 0, 0 }, 0, first_mip, { 0, 0 }, { (unsigned int)resized_image->width, (unsigned int)resized_image->height }, 0, graphics->GetContext());
			first_mip->Release();

			ID3D11ShaderResourceView* resource_view;
			result = graphics->GetDevice()->CreateShaderResourceView(_new_texture, nullptr, &resource_view);
			if (FAILED(result)) {
				_new_texture->Release();
				return new_texture;
			}
			graphics->GenerateMips(resource_view);
			resource_view->Release();
		}

		new_texture = _new_texture;
		if (!temporary) {
			graphics->AddInternalResource(new_texture, ECS_DEBUG_INFO);
		}
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Stream<void> ResizeTexture(void* texture_data, size_t current_width, size_t current_height, ECS_GRAPHICS_FORMAT format, size_t new_width, size_t new_height, AllocatorPolymorphic allocator, size_t resize_flags)
	{
		Stream<void> data = { nullptr, 0 };

		DirectX::Image dx_image;
		dx_image.pixels = (uint8_t*)texture_data;
		dx_image.format = GetGraphicsNativeFormat(format);
		dx_image.width = current_width;
		dx_image.height = current_height;
		HRESULT result = DirectX::ComputePitch(dx_image.format, dx_image.width, dx_image.height, dx_image.rowPitch, dx_image.slicePitch);
		if (FAILED(result)) {
			return data;
		}

		DirectX::TEX_FILTER_FLAGS filter_flag = DirectX::TEX_FILTER_LINEAR;
		switch (resize_flags) {
		case ECS_RESIZE_TEXTURE_FILTER_BOX:
			filter_flag = DirectX::TEX_FILTER_BOX;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_CUBIC:
			filter_flag = DirectX::TEX_FILTER_CUBIC;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_POINT:
			filter_flag = DirectX::TEX_FILTER_POINT;
			break;
		}

		DirectX::ScratchImage new_image;
		if (allocator.allocator != nullptr) {
			SetInternalImageAllocator(&new_image, allocator);
		}
		result = DirectX::Resize(dx_image, new_width, new_height, filter_flag, new_image);
		if (FAILED(result)) {
			return data;
		}

		data = { new_image.GetPixels(), new_image.GetPixelsSize() };
		new_image.DetachPixels();
		return data;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	DecodedTexture DecodeTexture(Stream<void> data, TextureExtension extension, AllocatorPolymorphic allocator, size_t flags)
	{
		DecodedTexture new_data = { { nullptr, 0 }, 0, 0, ECS_GRAPHICS_FORMAT_UNKNOWN };

		DirectX::ScratchImage image;
		SetInternalImageAllocator(&image, allocator);

		HRESULT result;
		DirectX::TexMetadata metadata;
		if (extension == ECS_TEXTURE_EXTENSION_HDR) {
			bool apply_tonemapping = HasFlag(flags, ECS_DECODE_TEXTURE_HDR_TONEMAP);
			result = DirectX::LoadFromHDRMemory(data.buffer, data.size, &metadata, image, apply_tonemapping);
		}
		else if (extension == ECS_TEXTURE_EXTENSION_TGA) {
			result = DirectX::LoadFromTGAMemory(data.buffer, data.size, &metadata, image);
		}
		else {
			DirectX::WIC_FLAGS wic_flags = DirectX::WIC_FLAGS_FORCE_RGB;
			result = DirectX::LoadFromWICMemory(data.buffer, data.size, wic_flags, &metadata, image);
		}

		if (FAILED(result)) {
			return new_data;
		}

		new_data.data = { image.GetPixels(), image.GetPixelsSize() };
		new_data.format = GetGraphicsFormatFromNative(metadata.format);
		new_data.height = metadata.height;
		new_data.width = metadata.width;
		image.DetachPixels();

		if (HasFlag(flags, ECS_DECODE_TEXTURE_NO_SRGB)) {
			new_data.format = GetGraphicsFormatNoSRGB(new_data.format);
		}

		if (HasFlag(flags, ECS_DECODE_TEXTURE_FORCE_SRGB)) {
			new_data.format = GetGraphicsFormatWithSRGB(new_data.format);
		}

		return new_data;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	DecodedTexture DecodeTexture(Stream<void> data, Stream<wchar_t> filename, AllocatorPolymorphic allocator, size_t flags)
	{
		Path extension = PathExtensionBoth(filename);

		if (extension == L".hdr") {
			return DecodeTexture(data, ECS_TEXTURE_EXTENSION_HDR, allocator, flags);
		}
		if (extension == L".tga") {
			return DecodeTexture(data, ECS_TEXTURE_EXTENSION_TGA, allocator, flags);
		}

		// The remaining textures don't matter exactly the type - they will all get mapped to the WIC path
		return DecodeTexture(data, ECS_TEXTURE_EXTENSION_JPG, allocator, flags);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	//ECS_OPTIMIZE_START;

	template<int index>
	ECS_INLINE Vec32uc ECS_VECTORCALL ConvertSingleChannelTextureToGrayscaleSplat(Vec32uc samples) {
		return permute32<index, index, index, V_DC, index + 1, index + 1, index + 1, V_DC, index + 2, index + 2, index + 2, V_DC, index + 3, index + 3, index + 3, V_DC,
			index + 4, index + 4, index + 4, V_DC, index + 5, index + 5, index + 5, V_DC, index + 6, index + 6, index + 6, V_DC, index + 7, index + 7, index + 7, V_DC>(samples);
	}

	ECS_INLINE Vec32uc ECS_VECTORCALL ConvertSingleChannelTextureToGrayscaleBlend(Vec32uc values, Vec32uc alpha) {
		return blend32<0, 1, 2, 3 + 32, 4, 5, 6, 7 + 32, 8, 9, 10, 11 + 32, 12, 13, 14, 15 + 32, 
			16, 17, 18, 19 + 32, 20, 21, 22, 23 + 32, 24, 25, 26, 27 + 32, 28, 29, 30, 31 + 32>(values, alpha);
	}

	template<typename SizeFunctor>
	Stream<Stream<void>> GetConvertStream(
		Stream<Stream<void>> mip_data, 
		AllocatorPolymorphic allocator, 
		bool in_place, 
		CapacityStream<Stream<void>>& new_data, 
		SizeFunctor&& size_functor
	) {
		// First determine the total amount of memory needed to transform the mip maps
		size_t total_data_size = 0;
		for (size_t index = 0; index < mip_data.size; index++) {
			total_data_size += size_functor(index, mip_data[index].size);
		}

		// Add the streams necessary to the allocation
		Stream<void>* streams;
		uintptr_t buffer;
		if (!in_place) {
			total_data_size += sizeof(Stream<void>) * mip_data.size;
			streams = (Stream<void>*)Allocate(allocator, total_data_size);
			buffer = (uintptr_t)streams;
			buffer += sizeof(Stream<void>) * mip_data.size;
		}
		else {
			ECS_ASSERT(mip_data.size <= new_data.capacity);

			streams = new_data.buffer;
			void* allocation = Allocate(allocator, total_data_size);
			buffer = (uintptr_t)allocation;
		}

		// Initialize the streams
		for (size_t index = 0; index < mip_data.size; index++) {
			streams[index].InitializeFromBuffer(buffer, size_functor(index, mip_data[index].size));
		}

		return { streams, mip_data.size };
	}

	Stream<Stream<void>> ConvertSingleChannelTextureToGrayscaleImpl(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		AllocatorPolymorphic allocator,
		bool in_place
	) {
		ECS_STACK_CAPACITY_STREAM(Stream<void>, output_streams, 512);
		Stream<Stream<void>> streams = GetConvertStream(mip_data, allocator, in_place, output_streams, [](size_t index, size_t mip_size) {
			return mip_size * 4;
		});

		Vec32uc samples;
		Vec32uc alpha(255);
		// Now copy the data - use SIMD instructions
		for (size_t index = 0; index < mip_data.size; index++) {
			size_t width_times_height = width * height;
			size_t steps = width_times_height / samples.size();
			size_t remainder = width_times_height % samples.size();
			if (remainder == 0) {
				for (size_t step = 0; step < steps; step++) {
					// Load the data into the register
					samples.load(OffsetPointer(mip_data[index].buffer, samples.size() * step));

					// Splat the values 8 at a time - but set the alpha to 255
					Vec32uc splat0 = ConvertSingleChannelTextureToGrayscaleSplat<0>(samples);
					Vec32uc splat1 = ConvertSingleChannelTextureToGrayscaleSplat<8>(samples);
					Vec32uc splat2 = ConvertSingleChannelTextureToGrayscaleSplat<16>(samples);
					Vec32uc splat3 = ConvertSingleChannelTextureToGrayscaleSplat<24>(samples);

					// Blend with the alpha
					Vec32uc values0 = ConvertSingleChannelTextureToGrayscaleBlend(splat0, alpha);
					Vec32uc values1 = ConvertSingleChannelTextureToGrayscaleBlend(splat1, alpha);
					Vec32uc values2 = ConvertSingleChannelTextureToGrayscaleBlend(splat2, alpha);
					Vec32uc values3 = ConvertSingleChannelTextureToGrayscaleBlend(splat3, alpha);

					void* base_pointer = OffsetPointer(streams[index].buffer, samples.size() * step * 4);
					// Write the values
					values0.store(base_pointer);
					values1.store(OffsetPointer(base_pointer, samples.size()));
					values2.store(OffsetPointer(base_pointer, samples.size() * 2));
					values3.store(OffsetPointer(base_pointer, samples.size() * 3));
				}
			}
			else {
				// Scalar version
				unsigned char* source_data = (unsigned char*)mip_data[index].buffer;
				unsigned char* destination_data = (unsigned char*)streams[index].buffer;
				for (size_t subindex = 0; subindex < mip_data[index].size; subindex++) {
					size_t dest_offset = subindex * 4;
					destination_data[dest_offset] = source_data[subindex];
					destination_data[dest_offset + 1] = source_data[subindex];
					destination_data[dest_offset + 2] = source_data[subindex];
					// Alpha
					destination_data[dest_offset + 3] = 255;
				}
			}
		}

		if (in_place) {
			mip_data.CopyOther(streams);
		}
		return streams;
	}

	Stream<Stream<void>> ConvertSingleChannelTextureToGrayscale(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		AllocatorPolymorphic allocator
	)
	{
		return ConvertSingleChannelTextureToGrayscaleImpl(mip_data, width, height, allocator, false);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ConvertSingleChannelTextureToGrayscaleInPlace(Stream<Stream<void>> mip_data, size_t width, size_t height, AllocatorPolymorphic allocator)
	{
		ConvertSingleChannelTextureToGrayscaleImpl(mip_data, width, height, allocator, true);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Stream<Stream<void>> ConvertTextureToSingleChannelImpl(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		size_t channel_count,
		size_t channel_to_copy,
		AllocatorPolymorphic allocator,
		bool in_place
	) {
		ECS_STACK_CAPACITY_STREAM(Stream<void>, output_streams, 512);
		Stream<Stream<void>> streams = GetConvertStream(mip_data, allocator, in_place, output_streams, [channel_count](size_t index, size_t mip_size) {
			return mip_size / channel_count;
		});

		// At the moment use just a scalar version. If needed add a SIMD version
		for (size_t index = 0; index < mip_data.size; index++) {
			unsigned char* values = (unsigned char*)streams[index].buffer;
			unsigned char* copy_values = (unsigned char*)mip_data[index].buffer;
			for (size_t pixel_index = 0; pixel_index < streams[index].size; pixel_index++) {
				values[pixel_index] = copy_values[pixel_index * channel_count + channel_to_copy];
			}
		}

		if (in_place) {
			mip_data.CopyOther(streams);
		}
		return streams;
	}

	Stream<Stream<void>> ConvertTextureToSingleChannel(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		size_t channel_count,
		size_t channel_to_copy,
		AllocatorPolymorphic allocator
	)
	{
		return ConvertTextureToSingleChannelImpl(mip_data, width, height, channel_count, channel_to_copy, allocator, false);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ConvertTextureToSingleChannelInPlace(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		size_t channel_count,
		size_t channel_to_copy,
		AllocatorPolymorphic allocator
	)
	{
		ConvertTextureToSingleChannelImpl(mip_data, width, height, channel_count, channel_to_copy, allocator, true);
	}
	
	// ----------------------------------------------------------------------------------------------------------------------

	Stream<Stream<void>> ConvertRGBTextureToRGBAImpl(Stream<Stream<void>> mip_data, size_t width, size_t height, AllocatorPolymorphic allocator, bool in_place) {
		ECS_STACK_CAPACITY_STREAM(Stream<void>, output_streams, 512);
		Stream<Stream<void>> streams = GetConvertStream(mip_data, allocator, in_place, output_streams, [](size_t index, size_t mip_size) {
			return mip_size / 3 * 4;
		});

		// Use a SIMD version with load, permute and store
		Vec32uc alpha_value = 255;
		for (size_t index = 0; index < mip_data.size; index++) {
			// Load 8 values at a time
			const unsigned char* input_values = (const unsigned char*)mip_data[index].buffer;
			unsigned char* output_values = (unsigned char*)streams[index].buffer;
			size_t output_value_index = 0;

			const size_t step_size = 24;
			size_t simd_count = GetSimdCount(mip_data[index].size, step_size);
			ECS_ASSERT((mip_data[index].size - simd_count) % 3 == 0)

				for (size_t simd_index = 0; simd_index < simd_count; simd_index += step_size) {
					Vec32uc current_values;
					current_values.load(input_values + simd_index);

					current_values = permute32<0, 1, 2, V_DC, 3, 4, 5, V_DC, 6, 7, 8, V_DC, 9, 10, 11, V_DC, 12, 13, 14, V_DC,
						15, 16, 17, V_DC, 18, 19, 20, V_DC, 21, 22, 23, V_DC>(current_values);
					current_values = blend32<0, 1, 2, 3 + 32, 4, 5, 6, 7 + 32, 8, 9, 10, 11 + 32, 12, 13, 14, 15 + 32,
						16, 17, 18, 19 + 32, 20, 21, 22, 23 + 32, 24, 25, 26, 27 + 32, 28, 29, 30, 31 + 32>(current_values, alpha_value);
					current_values.store(output_values + output_value_index);
					output_value_index += 32;
				}

			// The remainder is handled normally
			for (size_t scalar_index = simd_count; scalar_index < mip_data[index].size; scalar_index += 3) {
				output_values[output_value_index] = input_values[scalar_index];
				output_values[output_value_index + 1] = input_values[scalar_index + 1];
				output_values[output_value_index + 2] = input_values[scalar_index + 2];
				output_values[output_value_index + 3] = 255;
				output_value_index += 4;
			}
		}

		if (in_place) {
			mip_data.CopyOther(streams);
		}
		return streams;
	}

	Stream<Stream<void>> ConvertRGBTextureToRGBA(Stream<Stream<void>> mip_data, size_t width, size_t height, AllocatorPolymorphic allocator)
	{
		return ConvertRGBTextureToRGBAImpl(mip_data, width, height, allocator, false);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ConvertRGBTexturetoRGBAInPlace(Stream<Stream<void>> mip_data, size_t width, size_t height, AllocatorPolymorphic allocator)
	{
		ConvertRGBTextureToRGBAImpl(mip_data, width, height, allocator, true);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	TextureCube ConvertTexturesToCube(
		Graphics* graphics,
		Texture2D x_positive,
		Texture2D x_negative,
		Texture2D y_positive,
		Texture2D y_negative,
		Texture2D z_positive,
		Texture2D z_negative,
		bool temporary
	)
	{
		D3D11_TEXTURE2D_DESC texture_descriptor;
		x_negative.tex->GetDesc(&texture_descriptor);

		texture_descriptor.ArraySize = 6;
		texture_descriptor.Usage = D3D11_USAGE_DEFAULT;
		texture_descriptor.CPUAccessFlags = 0;
		texture_descriptor.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS;
		ID3D11Texture2D* texture = nullptr;
		HRESULT result = graphics->GetDevice()->CreateTexture2D(&texture_descriptor, nullptr, &texture);
		TextureCube cube_texture;
		cube_texture.tex = nullptr;

		ECS_CRASH_CONDITION_RETURN(result, cube_texture, "Converting textures to cube textures failed.");
		cube_texture = { texture };

		GraphicsContext* context = graphics->GetContext();
		// For every mip, copy the mip into the corresponding array resource
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			CopyGraphicsResource(cube_texture, x_negative, ECS_TEXTURE_CUBE_X_NEG, context, index);
			CopyGraphicsResource(cube_texture, x_positive, ECS_TEXTURE_CUBE_X_POS, context, index);
			CopyGraphicsResource(cube_texture, y_negative, ECS_TEXTURE_CUBE_Y_NEG, context, index);
			CopyGraphicsResource(cube_texture, y_positive, ECS_TEXTURE_CUBE_Y_POS, context, index);
			CopyGraphicsResource(cube_texture, z_negative, ECS_TEXTURE_CUBE_Z_NEG, context, index);
			CopyGraphicsResource(cube_texture, z_positive, ECS_TEXTURE_CUBE_Z_POS, context, index);
		}

		if (!temporary) {
			graphics->AddInternalResource(cube_texture);
		}
		return cube_texture;
	}

	TextureCube ConvertTexturesToCube(Graphics* graphics, const Texture2D* textures, bool temporary)
	{
		return ConvertTexturesToCube(graphics, textures[0], textures[1], textures[2], textures[3], textures[4], textures[5], temporary);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	TextureCube ConvertTextureToCube(Graphics* graphics, ResourceView texture_view, ECS_GRAPHICS_FORMAT cube_format, uint2 face_size, bool temporary)
	{
		TextureCube cube;

		// Create the 6 faces as render targets
		Texture2DDescriptor texture_descriptor;
		texture_descriptor.format = cube_format;
		texture_descriptor.bind_flag = GetGraphicsBindFromNative((D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));
		texture_descriptor.size = face_size;
		texture_descriptor.mip_levels = 0;

		Texture2D cube_textures[6];
		RenderTargetView render_views[6];
		for (size_t index = 0; index < 6; index++) {
			cube_textures[index] = graphics->CreateTexture(&texture_descriptor, true);
			render_views[index] = graphics->CreateRenderTargetView(cube_textures[index], 0, ECS_GRAPHICS_FORMAT_UNKNOWN, true);
		}

		// Generate a unit cube vertex buffer - a cube is needed instead of a rectangle because it will be rotated
		// by the look at matrix
		VertexBuffer vertex_buffer;
		IndexBuffer index_buffer;
		CreateCubeVertexBuffer(graphics, 0.5f, vertex_buffer, index_buffer, true);

		// Bind a nullptr depth stencil view - remove depth
		RenderTargetView current_render_view = graphics->GetBoundRenderTarget();
		DepthStencilView current_depth_view = graphics->GetBoundDepthStencil();
		GraphicsViewport current_viewport = graphics->GetBoundViewport();

		graphics->BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_CREATE_TEXTURE_CUBE);
		graphics->BindVertexBuffer(vertex_buffer);
		graphics->BindIndexBuffer(index_buffer);
		graphics->BindTopology(Topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
		graphics->BindPixelResourceView(texture_view);

		Matrix projection_matrix = ProjectionMatrixTextureCube();

		ConstantBuffer vertex_constants = graphics->CreateConstantBuffer(sizeof(Matrix), true);
		graphics->BindVertexConstantBuffer(vertex_constants);
		GraphicsViewport cube_viewport = { 0.0f, 0.0f, (float)face_size.x, (float)face_size.y, 0.0f, 1.0f };
		graphics->BindViewport(cube_viewport);
		graphics->DisableDepth();
		graphics->DisableCulling();

		for (size_t index = 0; index < 6; index++) {
			Matrix current_matrix = MatrixGPU(ViewMatrixTextureCube((TextureCubeFace)index) * projection_matrix);
			UpdateBufferResource(vertex_constants.buffer, &current_matrix, sizeof(Matrix), graphics->GetContext());
			graphics->BindRenderTargetView(render_views[index], nullptr);

			graphics->DrawIndexed(index_buffer.count);
		}
		graphics->EnableDepth();
		graphics->EnableCulling();

		graphics->BindRenderTargetView(current_render_view, current_depth_view);
		graphics->BindViewport(current_viewport);

		cube = ConvertTexturesToCube(graphics, cube_textures, temporary);
		ResourceView cube_view = graphics->CreateTextureShaderViewResource(cube, true);
		graphics->GenerateMips(cube_view);
		cube_view.Release();

		vertex_buffer.Release();
		index_buffer.Release();
		vertex_constants.Release();
		for (size_t index = 0; index < 6; index++) {
			render_views[index].Release();
			cube_textures[index].Release();
		}

		return cube;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint2 GetTextureDimensions(Stream<wchar_t> filename)
	{
		NULL_TERMINATE_WIDE(filename);

		uint2 dimensions = { 0,0 };

		Path extension = PathExtensionBoth(filename);

		if (extension.size == 0) {
			return dimensions;
		}

		bool is_tga = false;
		bool is_hdr = false;

		is_tga = extension == L".tga";
		is_hdr = extension == L".hdr";

		DirectX::TexMetadata metadata;
		HRESULT result;

		if (is_tga) {
			result = DirectX::GetMetadataFromTGAFile(filename.buffer, metadata);
		}
		else if (is_hdr) {
			result = DirectX::GetMetadataFromHDRFile(filename.buffer, metadata);
		}
		else {
			result = DirectX::GetMetadataFromWICFile(filename.buffer, DirectX::WIC_FLAGS_NONE, metadata);
		}

		if (FAILED(result)) {
			return dimensions;
		}

		dimensions = { (unsigned int)metadata.width, (unsigned int)metadata.height };
		return dimensions;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint2 GetTextureDimensions(Stream<void> data, TextureExtension extension)
	{
		using function = uint2(*)(Stream<void> data);
		function functions[] = {
			GetTextureDimensionsJPG,
			GetTextureDimensionsPNG,
			GetTextureDimensionsTIFF,
			GetTextureDimensionsBMP,
			GetTextureDimensionsTGA,
			GetTextureDimensionsHDR
		};

		return functions[extension](data);
	}

	template<typename Handler>
	static uint2 GetTextureDimensionsExtension(Stream<void> data, Handler&& handler) {
		uint2 dimensions = { 0,0 };

		DirectX::TexMetadata metadata;
		HRESULT result;

		result = handler(metadata);

		if (FAILED(result)) {
			return dimensions;
		}

		dimensions = { (unsigned int)metadata.width, (unsigned int)metadata.height };
		return dimensions;
	}

	uint2 GetTextureDimensionsPNG(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
			});
	}

	uint2 GetTextureDimensionsJPG(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
			});
	}

	uint2 GetTextureDimensionsBMP(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
			});
	}

	uint2 GetTextureDimensionsTIFF(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
			});
	}

	uint2 GetTextureDimensionsTGA(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromTGAMemory(data.buffer, data.size, metadata);
			});
	}

	uint2 GetTextureDimensionsHDR(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromHDRMemory(data.buffer, data.size, metadata);
			});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture1DDescriptor GetTextureDescriptor(Texture1D texture)
	{
		Texture1DDescriptor descriptor;

		D3D11_TEXTURE1D_DESC desc;
		texture.tex->GetDesc(&desc);

		descriptor.array_size = desc.ArraySize;
		descriptor.bind_flag = GetGraphicsBindFromNative(desc.BindFlags);
		descriptor.cpu_flag = GetGraphicsCPUAccessFromNative(desc.CPUAccessFlags);
		descriptor.format = GetGraphicsFormatFromNative(desc.Format);
		descriptor.mip_data = { nullptr, 0 };
		descriptor.mip_levels = desc.MipLevels;
		descriptor.misc_flag = GetGraphicsMiscFlagsFromNative(desc.MiscFlags);
		descriptor.usage = GetGraphicsUsageFromNative(desc.Usage);
		descriptor.width = desc.Width;

		return descriptor;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2DDescriptor GetTextureDescriptor(Texture2D texture)
	{
		Texture2DDescriptor descriptor;

		D3D11_TEXTURE2D_DESC desc;
		texture.tex->GetDesc(&desc);

		descriptor.array_size = desc.ArraySize;
		descriptor.bind_flag = GetGraphicsBindFromNative(desc.BindFlags);
		descriptor.cpu_flag = GetGraphicsCPUAccessFromNative(desc.CPUAccessFlags);
		descriptor.format = GetGraphicsFormatFromNative(desc.Format);
		descriptor.mip_data = { nullptr, 0 };
		descriptor.mip_levels = desc.MipLevels;
		descriptor.misc_flag = GetGraphicsMiscFlagsFromNative(desc.MiscFlags);
		descriptor.usage = GetGraphicsUsageFromNative(desc.Usage);
		descriptor.size = { desc.Width, desc.Height };
		descriptor.sampler_quality = desc.SampleDesc.Quality;
		descriptor.sample_count = desc.SampleDesc.Count;

		return descriptor;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture3DDescriptor GetTextureDescriptor(Texture3D texture)
	{
		Texture3DDescriptor descriptor;

		D3D11_TEXTURE3D_DESC desc;
		texture.tex->GetDesc(&desc);

		descriptor.bind_flag = GetGraphicsBindFromNative(desc.BindFlags);
		descriptor.cpu_flag = GetGraphicsCPUAccessFromNative(desc.CPUAccessFlags);
		descriptor.format = GetGraphicsFormatFromNative(desc.Format);
		descriptor.mip_data = { nullptr, 0 };
		descriptor.mip_levels = desc.MipLevels;
		descriptor.misc_flag = GetGraphicsMiscFlagsFromNative(desc.MiscFlags);
		descriptor.usage = GetGraphicsUsageFromNative(desc.Usage);
		descriptor.size = { desc.Width, desc.Height, desc.Depth };

		return descriptor;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	TextureCubeDescriptor GetTextureDescriptor(TextureCube texture)
	{
		TextureCubeDescriptor descriptor;

		D3D11_TEXTURE2D_DESC desc;
		texture.tex->GetDesc(&desc);

		descriptor.bind_flag = GetGraphicsBindFromNative(desc.BindFlags);
		descriptor.cpu_flag = GetGraphicsCPUAccessFromNative(desc.CPUAccessFlags);
		descriptor.format = GetGraphicsFormatFromNative(desc.Format);
		descriptor.mip_data = { nullptr, 0 };
		descriptor.mip_levels = desc.MipLevels;
		descriptor.misc_flag = GetGraphicsMiscFlagsFromNative(desc.MiscFlags);
		descriptor.usage = GetGraphicsUsageFromNative(desc.Usage);
		descriptor.size = { desc.Width, desc.Height };

		return descriptor;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int ClampToTextureBounds(Texture1D texture, unsigned int position, unsigned int mip_level)
	{
		unsigned int texture_bound = GetTextureDimensions(texture, mip_level);
		return ClampMax(position, texture_bound);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint2 ClampToTextureBounds(Texture2D texture, uint2 position, unsigned int mip_level)
	{
		uint2 texture_bounds = GetTextureDimensions(texture, mip_level);
		return BasicTypeClampMax(position, texture_bounds);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint3 ClampToTextureBounds(Texture3D texture, uint3 position, unsigned int mip_level)
	{
		uint3 texture_bounds = GetTextureDimensions(texture, mip_level);
		return BasicTypeClampMax(position, texture_bounds);
	}

	// ----------------------------------------------------------------------------------------------------------------------

}