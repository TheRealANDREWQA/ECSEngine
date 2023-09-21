#include "ecspch.h"
#include "TextureCompression.h"
#include "../../../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../../Utilities/Function.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "../GraphicsHelpers.h"
#include "../../Utilities/Path.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../Graphics.h"

namespace ECSEngine {

	ECS_GRAPHICS_FORMAT COMPRESSED_FORMATS[] = {
		ECS_GRAPHICS_FORMAT_UNKNOWN,
		ECS_GRAPHICS_FORMAT_BC1,
		ECS_GRAPHICS_FORMAT_BC3,
		ECS_GRAPHICS_FORMAT_BC4,
		ECS_GRAPHICS_FORMAT_BC5,
		ECS_GRAPHICS_FORMAT_BC6,
		ECS_GRAPHICS_FORMAT_BC7
	};

	// Only BC1, BC3 and BC7 are affected
	ECS_GRAPHICS_FORMAT COMPRESSED_FORMATS_SRGB[] = {
		ECS_GRAPHICS_FORMAT_UNKNOWN,
		ECS_GRAPHICS_FORMAT_BC1_SRGB,
		ECS_GRAPHICS_FORMAT_BC3_SRGB,
		ECS_GRAPHICS_FORMAT_BC4,
		ECS_GRAPHICS_FORMAT_BC5,
		ECS_GRAPHICS_FORMAT_BC6,
		ECS_GRAPHICS_FORMAT_BC7_SRGB
	};

	ECS_TEXTURE_COMPRESSION EXPLICIT_COMPRESSION_MAPPING[] = {
		ECS_TEXTURE_COMPRESSION_NONE,
		ECS_TEXTURE_COMPRESSION_BC1,
		ECS_TEXTURE_COMPRESSION_BC1,
		ECS_TEXTURE_COMPRESSION_BC1,
		ECS_TEXTURE_COMPRESSION_BC3,
		ECS_TEXTURE_COMPRESSION_BC4,
		ECS_TEXTURE_COMPRESSION_BC5,
		ECS_TEXTURE_COMPRESSION_BC5,
		ECS_TEXTURE_COMPRESSION_BC6,
		ECS_TEXTURE_COMPRESSION_BC7
	};

	ECS_TEXTURE_COMPRESS_FLAGS EXPLICIT_COMPRESSION_FLAGS[] = {
		ECS_TEXTURE_COMPRESS_NONE,
		ECS_TEXTURE_COMPRESS_NONE,
		ECS_TEXTURE_COMPRESS_NONE,
		ECS_TEXTURE_COMPRESS_DISABLE_PERCEPTUAL_WEIGHTING,
		ECS_TEXTURE_COMPRESS_NONE,
		ECS_TEXTURE_COMPRESS_NONE,
		ECS_TEXTURE_COMPRESS_NONE,
		ECS_TEXTURE_COMPRESS_NONE,
		ECS_TEXTURE_COMPRESS_NONE
	};

	DXGI_FORMAT ToSignedFormat(DXGI_FORMAT format) {
		switch (format) {
		case DXGI_FORMAT_BC4_UNORM:
			return DXGI_FORMAT_BC4_SNORM;
		case DXGI_FORMAT_BC5_UNORM:
			return DXGI_FORMAT_BC5_SNORM;
		case DXGI_FORMAT_BC6H_UF16:
			return DXGI_FORMAT_BC6H_SF16;
		default:
			return format;
		}
	}

	// the format must come from the UNORM formats
	ECS_INLINE DXGI_FORMAT ToTypelessFormat(DXGI_FORMAT format) {
		return (DXGI_FORMAT)(format - 1);
	}

	static void SetErrorMessage(CapacityStream<char>* error_message, Stream<char> error) {
		if (error_message != nullptr) {
			error_message->Copy(error);
			error_message->AssertCapacity();
		}
	}

	ECS_INLINE DirectX::TEX_COMPRESS_FLAGS AddCompressionFlag(DirectX::TEX_COMPRESS_FLAGS initial, DirectX::TEX_COMPRESS_FLAGS flag, bool state) {
		return (DirectX::TEX_COMPRESS_FLAGS)((unsigned int)initial | ((unsigned int)flag * state));
	}

	static DirectX::TEX_COMPRESS_FLAGS GetCompressionFlag(size_t flags) {
		DirectX::TEX_COMPRESS_FLAGS compress_flag = DirectX::TEX_COMPRESS_DEFAULT;
		compress_flag = AddCompressionFlag(compress_flag, DirectX::TEX_COMPRESS_UNIFORM, function::HasFlag(flags, ECS_TEXTURE_COMPRESS_DISABLE_PERCEPTUAL_WEIGHTING));
		compress_flag = AddCompressionFlag(compress_flag, DirectX::TEX_COMPRESS_PARALLEL, !function::HasFlag(flags, ECS_TEXTURE_COMPRESS_DISABLE_MULTICORE));
		return compress_flag;
	}

	bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture, 
		ECS_TEXTURE_COMPRESSION compression_type,
		CompressTextureDescriptor descriptor
	) {
		// Get the texture descriptor and fill the DirectX image
		Texture2DDescriptor texture_descriptor = GetTextureDescriptor(texture);

		// Check if the compression type conforms to the range
		if ((unsigned int)compression_type > (unsigned int)ECS_TEXTURE_COMPRESSION_BC7) {
			SetErrorMessage(descriptor.error_message, "Incorrect compression type - out of range.");
			return false;
		}

		// Check to see that the dimensions are multiple of 4
		if ((texture_descriptor.size.x & 3) != 0 || (texture_descriptor.size.y & 3) != 0) {
			SetErrorMessage(descriptor.error_message, "Cannot compress textures that do not have dimensions multiple of 4.");
			return false;
		}

		DirectX::ScratchImage initial_image;
		SetInternalImageAllocator(&initial_image, descriptor.allocator);

		initial_image.Initialize2D(
			GetGraphicsNativeFormat(texture_descriptor.format), 
			texture_descriptor.size.x, 
			texture_descriptor.size.y, 
			texture_descriptor.array_size, 
			texture_descriptor.mip_levels
		);
		ECS_GRAPHICS_FORMAT compressed_format;
		if (function::HasFlag(descriptor.flags, ECS_TEXTURE_COMPRESS_SRGB)) {
			compressed_format = COMPRESSED_FORMATS_SRGB[(unsigned int)compression_type];
		}
		else {
			compressed_format = COMPRESSED_FORMATS[(unsigned int)compression_type];
		}
		DirectX::ScratchImage final_image;

		DirectX::TEX_COMPRESS_FLAGS compress_flag = GetCompressionFlag(descriptor.flags);

		GraphicsDevice* device = graphics->m_device;
		GraphicsContext* context = graphics->m_context;

		D3D11_MAPPED_SUBRESOURCE mapping;

		// Create a staging resource that will copy the original texture and map every mip level
		// For compression
		if (descriptor.spin_lock != nullptr) {
			descriptor.spin_lock->lock();
		}

		Texture2D staging_texture = TextureToStaging(graphics, texture);
		if (staging_texture.tex == nullptr) {
			SetErrorMessage(descriptor.error_message, "Failed compressing a texture. Could not create staging texture.");
			if (descriptor.spin_lock != nullptr) {
				descriptor.spin_lock->unlock();
			}
			return false;
		}

		for (size_t index = 0; index < texture_descriptor.mip_levels; index++) {
			HRESULT result = context->Map(staging_texture.tex, index, D3D11_MAP_READ, 0, &mapping);
			if (FAILED(result)) {
				SetErrorMessage(descriptor.error_message, "Failed compressing a texture. Mapping a mip level failed.");
				if (descriptor.spin_lock != nullptr) {
					descriptor.spin_lock->unlock();
				}
				return false;
			}
			const DirectX::Image* image = initial_image.GetImage(index, 0, 0);
			if (image->rowPitch == mapping.RowPitch) {
				memcpy(image->pixels, mapping.pData, image->rowPitch * image->height);
			}
			else {
				size_t offset = 0;
				size_t mapping_offset = 0;
				for (size_t subindex = 0; subindex < image->height; subindex++) {
					memcpy(function::OffsetPointer(image->pixels, offset), function::OffsetPointer(mapping.pData, mapping_offset), image->rowPitch);
					offset += image->rowPitch;
					mapping_offset += mapping.RowPitch;
				}
			}
			context->Unmap(staging_texture.tex, index);
		}

		HRESULT result;
		// CPU codec
		if (IsCPUCodec(compression_type)) {
			if (descriptor.spin_lock != nullptr) {
				descriptor.spin_lock->unlock();
			}

			result = DirectX::Compress(
				initial_image.GetImages(),
				initial_image.GetImageCount(),
				initial_image.GetMetadata(),
				GetGraphicsNativeFormat(compressed_format),
				compress_flag,
				DirectX::TEX_THRESHOLD_DEFAULT,
				final_image
			);
		}
		// GPU Codec
		else if (IsGPUCodec(compression_type)) {
			result = DirectX::Compress(
				device,
				initial_image.GetImages(),
				initial_image.GetImageCount(),
				initial_image.GetMetadata(),
				GetGraphicsNativeFormat(compressed_format),
				compress_flag | DirectX::TEX_COMPRESS_BC7_QUICK,
				1.0f,
				final_image
			);
			if (descriptor.spin_lock != nullptr) {
				descriptor.spin_lock->unlock();
			}
		}
		else {
			SetErrorMessage(descriptor.error_message, "Invalid compression codec for texture compression.");
			return false;
		}

		if (FAILED(result)) {
			SetErrorMessage(descriptor.error_message, "Texture compression failed during codec.");
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(Stream<void>, mip_data, 32);
		for (size_t index = 0; index < texture_descriptor.mip_levels; index++) {
			const DirectX::Image* image = final_image.GetImage(index, 0, 0);
			mip_data[index].buffer = image->pixels;
			mip_data[index].size = image->slicePitch;
		}

		ID3D11Texture2D* old_texture = texture.tex;
		texture_descriptor.format = compressed_format;
		texture_descriptor.mip_data = { mip_data.buffer, texture_descriptor.mip_levels };
		if (!function::HasFlag(descriptor.flags, ECS_TEXTURE_COMPRESS_BIND_RENDER_TARGET)) {
			texture_descriptor.bind_flag = (ECS_GRAPHICS_BIND_TYPE)function::ClearFlag(texture_descriptor.bind_flag, ECS_GRAPHICS_BIND_RENDER_TARGET);
		}
		texture_descriptor.misc_flag = (ECS_GRAPHICS_MISC_FLAGS)function::ClearFlag(texture_descriptor.misc_flag, D3D11_RESOURCE_MISC_GENERATE_MIPS);
		if (FAILED(result)) {
			SetErrorMessage(descriptor.error_message, "Creating final texture after compression failed.");
			return false;
		}
		texture = graphics->CreateTexture(&texture_descriptor);

		// Release the staging texture
		staging_texture.Release();
		old_texture->Release();
		graphics->RemovePossibleResourceFromTracking(old_texture);
		graphics->AddInternalResource(texture, ECS_DEBUG_INFO);

		return true;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture, 
		ECS_TEXTURE_COMPRESSION_EX explicit_compression_type, 
		CompressTextureDescriptor descriptor
	) {
		descriptor.flags |= EXPLICIT_COMPRESSION_FLAGS[(unsigned int)explicit_compression_type];
		
		return CompressTexture(
			graphics,
			texture, 
			EXPLICIT_COMPRESSION_MAPPING[(unsigned int)explicit_compression_type], 
			descriptor
		);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	Texture2D CompressTexture(
		Graphics* graphics,
		Stream<Stream<void>> data,
		size_t width, 
		size_t height, 
		ECS_TEXTURE_COMPRESSION compression_type,
		bool temporary_texture,
		CompressTextureDescriptor descriptor
	)
	{
		Texture2D texture_result = (ID3D11Texture2D*)nullptr;

		ECS_GRAPHICS_FORMAT compressed_format = function::HasFlag(descriptor.flags, ECS_TEXTURE_COMPRESS_SRGB) ? COMPRESSED_FORMATS_SRGB[compression_type] 
			: COMPRESSED_FORMATS[compression_type];

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<void>, new_data, data.size);
		if (IsCPUCodec(compression_type)) {
			bool success = CompressTexture(data, new_data.buffer, width, height, compression_type, descriptor);
			if (!success) {
				return texture_result;
			}

			Texture2DDescriptor texture_descriptor;
			texture_descriptor.bind_flag = ECS_GRAPHICS_BIND_SHADER_RESOURCE;
			texture_descriptor.format = compressed_format;
			texture_descriptor.usage = ECS_GRAPHICS_USAGE_DEFAULT;
			texture_descriptor.size = { (unsigned int)width, (unsigned int)height };
			texture_descriptor.mip_levels = data.size;
			texture_descriptor.mip_data = { new_data.buffer, data.size };

			texture_result = graphics->CreateTexture(&texture_descriptor, temporary_texture);

			// Release the memory for the compressed texture
			DeallocateEx(descriptor.allocator, new_data[0].buffer);
		}
		else {
			// Need to call the GPU version.
			DirectX::Image initial_images[32];
			void* mip_data[32];
			for (size_t index = 0; index < data.size; index++) {
				mip_data[index] = data[index].buffer;
			}

			DirectX::TEX_COMPRESS_FLAGS compress_flag = GetCompressionFlag(descriptor.flags);

			ECS_GRAPHICS_FORMAT non_compressed_format = compression_type == ECS_TEXTURE_COMPRESSION_BC7 ? ECS_GRAPHICS_FORMAT_RGBA8_UNORM : ECS_GRAPHICS_FORMAT_RGBA32_FLOAT;
			if (function::HasFlag(descriptor.flags, ECS_TEXTURE_COMPRESS_SRGB) && compression_type == ECS_TEXTURE_COMPRESSION_BC7) {
				non_compressed_format = ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB;
			}
			DirectX::ScratchImage initial_image;
			initial_image.Initialize2DNoMemory((const void**)mip_data, data.size, initial_images, GetGraphicsNativeFormat(non_compressed_format), width, height, 1, data.size);

			// Validate that the buffer has enough data to be read - for example if mistankenly choosing HDR compression for a texture
			// That does not sufficient byte size might cause a crash
			if (initial_image.GetImage(0, 0, 0)->slicePitch > data[0].size) {
				Stream<char> compression_string = compression_type == ECS_TEXTURE_COMPRESSION_BC7 ? "BC7" : "BC6";
				ECS_FORMAT_TEMP_STRING(message, "Texture compression {#} not adequate for the given texture. There is not enough data to be read", compression_string);
				SetErrorMessage(descriptor.error_message, message);
				return texture_result;
			}

			// Lock the gpu lock, if any
			if (descriptor.spin_lock != nullptr) {
				descriptor.spin_lock->lock();
			}

			ID3D11Texture2D* final_texture = nullptr;
			HRESULT result = DirectX::CompressGPU(
				graphics->GetDevice(),
				initial_images, 
				data.size, 
				initial_image.GetMetadata(),
				GetGraphicsNativeFormat(compressed_format),
				compress_flag | DirectX::TEX_COMPRESS_BC7_QUICK,
				1.0f, 
				&final_texture
			);

			if (descriptor.spin_lock != nullptr) {
				descriptor.spin_lock->unlock();
			}

			if (!temporary_texture) {
				// Add the resource to the graphics internal tracked resources
				graphics->AddInternalResource(Texture2D(final_texture));
			}

			if (FAILED(result)) {
				SetErrorMessage(descriptor.error_message, "Texture compression failed during codec.");
				return texture_result;
			}

			texture_result = final_texture;
		}

		if (texture_result.tex == nullptr) {
			SetErrorMessage(descriptor.error_message, "Creating final texture after compression failed.");
			return texture_result;
		}

		return texture_result;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	Texture2D CompressTexture(
		Graphics* graphics,
		Stream<Stream<void>> data, 
		size_t width, 
		size_t height, 
		ECS_TEXTURE_COMPRESSION_EX compression_type,
		bool temporary_texture,
		CompressTextureDescriptor descriptor
	)
	{
		descriptor.flags |= EXPLICIT_COMPRESSION_FLAGS[(unsigned int)compression_type];

		return CompressTexture(
			graphics, 
			data,
			width, 
			height,
			EXPLICIT_COMPRESSION_MAPPING[(unsigned int)compression_type],
			temporary_texture,
			descriptor
		);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	bool CompressTexture(
		Stream<Stream<void>> data, 
		Stream<void>* new_data,
		size_t width,
		size_t height, 
		ECS_TEXTURE_COMPRESSION compression_type,
		CompressTextureDescriptor descriptor
	)
	{
		// Get the texture descriptor and fill the DirectX image
		ECS_GRAPHICS_FORMAT texture_format = ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		if (compression_type == ECS_TEXTURE_COMPRESSION_BC3) {
			texture_format = ECS_GRAPHICS_FORMAT_R8_UNORM;
		}
		else if (compression_type == ECS_TEXTURE_COMPRESSION_BC5) {
			texture_format = ECS_GRAPHICS_FORMAT_RG8_UNORM;
		}

		// Check if the compression type conforms to the range
		if ((unsigned int)compression_type > (unsigned int)ECS_TEXTURE_COMPRESSION_BC5) {
			SetErrorMessage(descriptor.error_message, "Incorrect compression type - out of range.");
			return false;
		}

		// Check to see that the dimensions are multiple of 4
		if ((width & 3) != 0 || (height & 3) != 0) {
			SetErrorMessage(descriptor.error_message, "Cannot compress textures that do not have dimensions multiple of 4.");
			return false;
		}

		DirectX::ScratchImage initial_image;
		SetInternalImageAllocator(&initial_image, descriptor.allocator);

		initial_image.Initialize2D(GetGraphicsNativeFormat(texture_format), width, height, 1, data.size);
		ECS_GRAPHICS_FORMAT compressed_format = COMPRESSED_FORMATS[(unsigned int)compression_type];
		DirectX::ScratchImage final_image;
		SetInternalImageAllocator(&final_image, descriptor.allocator);

		DirectX::TEX_COMPRESS_FLAGS compress_flag = GetCompressionFlag(descriptor.flags);

		for (size_t index = 0; index < data.size; index++) {
			const DirectX::Image* image = initial_image.GetImage(index, 0, 0);
			size_t row_pitch = data[index].size / image->height;
			if (image->rowPitch == row_pitch) {
				memcpy(image->pixels, data[index].buffer, image->rowPitch * image->height);
			}
			else {
				size_t offset = 0;
				size_t mapping_offset = 0;
				for (size_t subindex = 0; subindex < image->height; subindex++) {
					memcpy(function::OffsetPointer(image->pixels, offset), function::OffsetPointer(data[index].buffer, mapping_offset), image->rowPitch);
					offset += image->rowPitch;
					mapping_offset += row_pitch;
				}
			}
		}

		HRESULT result = DirectX::Compress(
			initial_image.GetImages(),
			initial_image.GetImageCount(),
			initial_image.GetMetadata(),
			GetGraphicsNativeFormat(compressed_format),
			compress_flag,
			DirectX::TEX_THRESHOLD_DEFAULT,
			final_image
		);

		if (FAILED(result)) {
			SetErrorMessage(descriptor.error_message, "Texture compression failed during codec.");
			return false;
		}

		for (size_t index = 0; index < data.size; index++) {
			const DirectX::Image* image = final_image.GetImage(index, 0, 0);
			// The row pitch must be divided by 4 because the function returns the byte count of 4 scanlines - not for a single one
			// Or use the slice pitch
			new_data[index] = { image->pixels, image->slicePitch };
		}

		final_image.DetachPixels();
		return true;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	bool CompressTexture(
		Stream<Stream<void>> data,
		Stream<void>* new_data,
		size_t width,
		size_t height, 
		ECS_TEXTURE_COMPRESSION_EX compression_type,
		CompressTextureDescriptor descriptor
	)
	{
		descriptor.flags |= EXPLICIT_COMPRESSION_FLAGS[(unsigned int)compression_type];

		return CompressTexture(
			data,
			new_data,
			width,
			height,
			EXPLICIT_COMPRESSION_MAPPING[(unsigned int)compression_type],
			descriptor
		);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	ECS_TEXTURE_COMPRESSION TextureCompressionFromEx(ECS_TEXTURE_COMPRESSION_EX ex_type)
	{
		return EXPLICIT_COMPRESSION_MAPPING[ex_type];
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	ECS_TEXTURE_COMPRESSION_EX GetTextureCompressionType(const wchar_t* texture)
	{
		Stream<wchar_t> path = texture;
		Path extension = function::PathExtensionBoth(path);

		// Fail
		if (extension.size == 0) {
			return ECS_TEXTURE_COMPRESSION_EX_NONE;
		}

		bool is_hdr = false;
		bool is_tga = false;
		is_hdr = function::CompareStrings(extension, L".hdr");
		is_tga = function::CompareStrings(extension, L".tga");

		if (is_hdr) {
			return ECS_TEXTURE_COMPRESSION_EX_HDR;
		}

		DirectX::TexMetadata metadata;
		HRESULT result;
		if (is_tga) {
			result = DirectX::GetMetadataFromTGAFile(texture, metadata);
		}
		else {
			result = DirectX::GetMetadataFromWICFile(texture, DirectX::WIC_FLAGS_NONE, metadata);
		}

		if (FAILED(result)) {
			return ECS_TEXTURE_COMPRESSION_EX_NONE;
		}

		return GetTextureCompressionType(GetGraphicsFormatFromNative(metadata.format));
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	ECS_TEXTURE_COMPRESSION_EX GetTextureCompressionType(ECS_GRAPHICS_FORMAT format)
	{
		if (format == ECS_GRAPHICS_FORMAT_RGBA8_UNORM || format == ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB) {
			return ECS_TEXTURE_COMPRESSION_EX_COLOR;
		}
		if (format == ECS_GRAPHICS_FORMAT_R8_UNORM) {
			return ECS_TEXTURE_COMPRESSION_EX_GRAYSCALE;
		}
		if (format == ECS_GRAPHICS_FORMAT_RG8_UNORM) {
			return ECS_TEXTURE_COMPRESSION_EX_DOUBLE_GRAYSCALE;
		}
		if (format == ECS_GRAPHICS_FORMAT_RGBA16_FLOAT || format == ECS_GRAPHICS_FORMAT_RGBA32_FLOAT) {
			return ECS_TEXTURE_COMPRESSION_EX_HDR;
		}
		return ECS_TEXTURE_COMPRESSION_EX_NONE;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetCompressedRenderFormat(ECS_TEXTURE_COMPRESSION compression, bool srgb)
	{
		ECS_GRAPHICS_FORMAT compressed_format;

		if (srgb) {
			compressed_format = COMPRESSED_FORMATS_SRGB[(unsigned int)compression];
		}
		else {
			compressed_format = COMPRESSED_FORMATS[(unsigned int)compression];
		}

		return compressed_format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetCompressedRenderFormat(ECS_TEXTURE_COMPRESSION_EX compression, bool srgb)
	{
		return GetCompressedRenderFormat(EXPLICIT_COMPRESSION_MAPPING[(unsigned int)compression], srgb);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

}