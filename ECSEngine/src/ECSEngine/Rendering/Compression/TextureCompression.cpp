#include "ecspch.h"
#include "TextureCompression.h"
#include "../../../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../../Utilities/Function.h"
#include "../GraphicsHelpers.h"
#include "../../Utilities/Path.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../Graphics.h"

namespace ECSEngine {

	constexpr DXGI_FORMAT COMPRESSED_FORMATS[] = {
		DXGI_FORMAT_BC1_UNORM,
		DXGI_FORMAT_BC3_UNORM,
		DXGI_FORMAT_BC4_UNORM,
		DXGI_FORMAT_BC5_UNORM,
		DXGI_FORMAT_BC6H_UF16,
		DXGI_FORMAT_BC7_UNORM
	};

	// Only BC1, BC3 and BC7 are affected
	constexpr DXGI_FORMAT COMPRESSED_FORMATS_SRGB[] = {
		DXGI_FORMAT_BC1_UNORM_SRGB,
		DXGI_FORMAT_BC3_UNORM_SRGB,
		DXGI_FORMAT_BC4_UNORM,
		DXGI_FORMAT_BC5_UNORM,
		DXGI_FORMAT_BC6H_UF16,
		DXGI_FORMAT_BC7_UNORM_SRGB
	};

	constexpr TextureCompression EXPLICIT_COMPRESSION_MAPPING[] = {
		TextureCompression::BC1,
		TextureCompression::BC1,
		TextureCompression::BC1,
		TextureCompression::BC3,
		TextureCompression::BC4,
		TextureCompression::BC5,
		TextureCompression::BC5,
		TextureCompression::BC6,
		TextureCompression::BC7
	};

	constexpr size_t EXPLICIT_COMPRESSION_FLAGS[] = {
		0,
		0,
		ECS_TEXTURE_COMPRESSION_DISABLE_PERCEPTUAL_WEIGHTING,
		0,
		0,
		0,
		0,
		0
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
	DXGI_FORMAT ToTypelessFormat(DXGI_FORMAT format) {
		return (DXGI_FORMAT)(format - 1);
	}

	void SetErrorMessage(CapacityStream<char>* error_message, const char* error) {
		if (error_message != nullptr) {
			error_message->Copy(ToStream(error));
			error_message->AssertCapacity();
		}
	}

	bool IsCPUCodec(TextureCompression compression) {
		return compression == TextureCompression::BC1 || compression == TextureCompression::BC3 || compression == TextureCompression::BC4
			|| compression == TextureCompression::BC5;
	}

	bool IsGPUCodec(TextureCompression compression) {
		return compression == TextureCompression::BC6 || compression == TextureCompression::BC7;
	}

	DirectX::TEX_COMPRESS_FLAGS AddCompressionFlag(DirectX::TEX_COMPRESS_FLAGS initial, DirectX::TEX_COMPRESS_FLAGS flag, bool state) {
		return (DirectX::TEX_COMPRESS_FLAGS)((unsigned int)initial | ((unsigned int)flag * state));
	}

	DirectX::TEX_COMPRESS_FLAGS GetCompressionFlag(size_t flags) {
		DirectX::TEX_COMPRESS_FLAGS compress_flag = DirectX::TEX_COMPRESS_DEFAULT;
		compress_flag = AddCompressionFlag(compress_flag, DirectX::TEX_COMPRESS_UNIFORM, function::HasFlag(flags, ECS_TEXTURE_COMPRESSION_DISABLE_PERCEPTUAL_WEIGHTING));
		compress_flag = AddCompressionFlag(compress_flag, DirectX::TEX_COMPRESS_PARALLEL, !function::HasFlag(flags, ECS_TEXTURE_COMPRESS_DISABLE_MULTICORE));
		return compress_flag;
	}

	template<typename void (*LockFunction)(SpinLock*), typename void (*UnlockFunction)(SpinLock*)>
	bool CompressTextureImplementation(
		Graphics* graphics,
		Texture2D& texture, 
		TextureCompression compression_type, 
		SpinLock* spin_lock,
		AllocatorPolymorphic allocator,
		size_t flags,
		CapacityStream<char>* error_message
	) {
		// Get the texture descriptor and fill the DirectX image
		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		// Check if the compression type conforms to the range
		if ((unsigned int)compression_type > (unsigned int)TextureCompression::BC7) {
			SetErrorMessage(error_message, "Incorrect compression type - out of range.");
			return false;
		}

		// Check to see that the dimensions are multiple of 4
		if ((texture_descriptor.Width & 3) != 0 || (texture_descriptor.Height & 3) != 0) {
			SetErrorMessage(error_message, "Cannot compress textures that do not have dimensions multiple of 4.");
			return false;
		}

		DirectX::ScratchImage initial_image;
		SetInternalImageAllocator(&initial_image, allocator);

		initial_image.Initialize2D(texture_descriptor.Format, texture_descriptor.Width, texture_descriptor.Height, texture_descriptor.ArraySize, texture_descriptor.MipLevels);
		DXGI_FORMAT compressed_format;
		if (function::HasFlag(flags, ECS_TEXTURE_COMPRESS_SRGB)) {
			compressed_format = COMPRESSED_FORMATS_SRGB[(unsigned int)compression_type];
		}
		else {
			compressed_format = COMPRESSED_FORMATS[(unsigned int)compression_type];
		}
		DirectX::ScratchImage final_image;

		DirectX::TEX_COMPRESS_FLAGS compress_flag = GetCompressionFlag(flags);

		GraphicsDevice* device = graphics->m_device;
		GraphicsContext* context = graphics->m_context;

		D3D11_MAPPED_SUBRESOURCE mapping;

		// Create a staging resource that will copy the original texture and map every mip level
		// For compression
		LockFunction(spin_lock);
		Texture2D staging_texture = TextureToStaging(graphics, texture);
		if (staging_texture.tex == nullptr) {
			SetErrorMessage(error_message, "Failed compressing a texture. Could not create staging texture.");
			UnlockFunction(spin_lock);
			return false;
		}

		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			HRESULT result = context->Map(staging_texture.tex, index, D3D11_MAP_READ, 0, &mapping);
			if (FAILED(result)) {
				SetErrorMessage(error_message, "Failed compressing a texture. Mapping a mip level failed.");
				UnlockFunction(spin_lock);
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
		UnlockFunction(spin_lock);

		HRESULT result;
		// CPU codec
		if (IsCPUCodec(compression_type)) {
			result = DirectX::Compress(
				initial_image.GetImages(),
				initial_image.GetImageCount(),
				initial_image.GetMetadata(),
				compressed_format,
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
				compressed_format,
				compress_flag | DirectX::TEX_COMPRESS_BC7_QUICK,
				1.0f,
				final_image
			);
		}
		else {
			SetErrorMessage(error_message, "Invalid compression codec for texture compression.");
			return false;
		}
		if (FAILED(result)) {
			SetErrorMessage(error_message, "Texture compression failed during codec.");
			return false;
		}

		D3D11_SUBRESOURCE_DATA new_texture_data[32];
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			const DirectX::Image* image = final_image.GetImage(index, 0, 0);
			new_texture_data[index].pSysMem = image->pixels;
			new_texture_data[index].SysMemPitch = image->rowPitch;
			new_texture_data[index].SysMemSlicePitch = image->slicePitch;
		}

		ID3D11Texture2D* old_texture = texture.tex;
		texture_descriptor.Format = compressed_format;
		if (!function::HasFlag(flags, ECS_TEXTURE_COMPRESS_BIND_RENDER_TARGET)) {
			texture_descriptor.BindFlags = function::ClearFlag(texture_descriptor.BindFlags, D3D11_BIND_RENDER_TARGET);
		}
		texture_descriptor.MiscFlags = function::ClearFlag(texture_descriptor.MiscFlags, D3D11_RESOURCE_MISC_GENERATE_MIPS);
		result = device->CreateTexture2D(&texture_descriptor, new_texture_data, &texture.tex);
		if (FAILED(result)) {
			SetErrorMessage(error_message, "Creating final texture after compression failed.");
			return false;
		}

		// Release the staging texture
		staging_texture.Release();
		old_texture->Release();
		graphics->RemovePossibleResourceFromTracking(old_texture);
		graphics->AddInternalResource(texture, ECS_DEBUG_INFO);

		return true;
	}

	void NothingSpinLock(SpinLock* spin_lock) {}

	void LockSpinLock(SpinLock* spin_lock) {
		spin_lock->lock();
	}

	void UnlockSpinLock(SpinLock* spin_lock) {
		spin_lock->unlock();
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	bool CompressTexture(
		Graphics* graphics, 
		Texture2D& texture, 
		TextureCompression compression_type,
		AllocatorPolymorphic allocator,
		size_t flags,
		CapacityStream<char>* error_message
	) {
		return CompressTextureImplementation<NothingSpinLock, NothingSpinLock>(graphics, texture, compression_type, nullptr, allocator, flags, error_message);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture,
		TextureCompression compression_type,
		SpinLock* spin_lock, 
		AllocatorPolymorphic allocator,
		size_t flags,
		CapacityStream<char>* error_message
	) {
		return CompressTextureImplementation<LockSpinLock, UnlockSpinLock>(graphics, texture, compression_type, spin_lock, allocator, flags, error_message);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture, 
		TextureCompressionExplicit explicit_compression_type, 
		AllocatorPolymorphic allocator, 
		size_t flags, 
		CapacityStream<char>* error_message
	) {
		return CompressTexture(
			graphics,
			texture, 
			EXPLICIT_COMPRESSION_MAPPING[(unsigned int)explicit_compression_type], 
			allocator,
			flags | EXPLICIT_COMPRESSION_FLAGS[(unsigned int)explicit_compression_type],
			error_message
		);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture, 
		TextureCompressionExplicit explicit_compression_type, 
		SpinLock* spin_lock,
		AllocatorPolymorphic allocator,
		size_t flags,
		CapacityStream<char>* error_message
	) {
		return CompressTexture(
			graphics,
			texture,
			EXPLICIT_COMPRESSION_MAPPING[(unsigned int)explicit_compression_type], 
			spin_lock, 
			allocator,
			flags | EXPLICIT_COMPRESSION_FLAGS[(unsigned int)explicit_compression_type], 
			error_message
		);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	Texture2D CompressTexture(
		Graphics* graphics,
		Stream<Stream<void>> data,
		size_t width, 
		size_t height, 
		TextureCompression compression_type, 
		AllocatorPolymorphic allocator,
		size_t flags,
		CapacityStream<char>* error_message
	)
	{
		Texture2D texture_result = (ID3D11Texture2D*)nullptr;

		// Get the texture descriptor and fill the DirectX image
		DXGI_FORMAT texture_format = DXGI_FORMAT_R8G8B8A8_UNORM;
		if (compression_type == TextureCompression::BC3) {
			texture_format = DXGI_FORMAT_R8_UNORM;
		}
		else if (compression_type == TextureCompression::BC5) {
			texture_format = DXGI_FORMAT_R8G8_UNORM;
		}
		else if (compression_type == TextureCompression::BC6) {
			texture_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		}

		// Check if the compression type conforms to the range
		if ((unsigned int)compression_type > (unsigned int)TextureCompression::BC5) {
			SetErrorMessage(error_message, "Incorrect compression type - out of range.");
			return texture_result;
		}

		// Check to see that the dimensions are multiple of 4
		if ((width & 3) != 0 || (height & 3) != 0) {
			SetErrorMessage(error_message, "Cannot compress textures that do not have dimensions multiple of 4.");
			return texture_result;
		}

		DirectX::ScratchImage initial_image;
		SetInternalImageAllocator(&initial_image, allocator);

		initial_image.Initialize2D(texture_format, width, height, 1, data.size);
		DXGI_FORMAT compressed_format = COMPRESSED_FORMATS[(unsigned int)compression_type];
		DirectX::ScratchImage final_image;
		SetInternalImageAllocator(&final_image, allocator);

		DirectX::TEX_COMPRESS_FLAGS compress_flag = GetCompressionFlag(flags);

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
			compressed_format,
			compress_flag,
			DirectX::TEX_THRESHOLD_DEFAULT,
			final_image
		);

		if (FAILED(result)) {
			SetErrorMessage(error_message, "Texture compression failed during codec.");
			return texture_result;
		}

		D3D11_SUBRESOURCE_DATA new_texture_data[32];
		for (size_t index = 0; index < data.size; index++) {
			const DirectX::Image* image = final_image.GetImage(index, 0, 0);
			new_texture_data[index].pSysMem = image->pixels;
			new_texture_data[index].SysMemPitch = image->rowPitch;
			new_texture_data[index].SysMemSlicePitch = image->slicePitch;
		}

		GraphicsTexture2DDescriptor descriptor;
		descriptor.bind_flag = D3D11_BIND_SHADER_RESOURCE;
		descriptor.format = compressed_format;
		descriptor.usage = D3D11_USAGE_DEFAULT;
		descriptor.size = { (unsigned int)width, (unsigned int)height };
		descriptor.mip_levels = data.size;

		texture_result = graphics->CreateTexture(&descriptor);
		if (texture_result.tex == nullptr) {
			SetErrorMessage(error_message, "Creating final texture after compression failed.");
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
		TextureCompressionExplicit compression_type,
		AllocatorPolymorphic allocator,
		size_t flags,
		CapacityStream<char>* error_message
	)
	{
		return CompressTexture(
			graphics, 
			data,
			width, 
			height,
			EXPLICIT_COMPRESSION_MAPPING[(unsigned int)compression_type], 
			allocator,
			flags | EXPLICIT_COMPRESSION_FLAGS[(unsigned int)compression_type],
			error_message
		);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	Stream<Stream<void>> CompressTexture(
		Stream<Stream<void>> data, 
		size_t width,
		size_t height, 
		TextureCompression compression_type, 
		AllocatorPolymorphic allocator, 
		size_t flags, 
		CapacityStream<char>* error_message
	)
	{
		Stream<Stream<void>> compressed_data = { nullptr, 0 };
		
		// Get the texture descriptor and fill the DirectX image
		DXGI_FORMAT texture_format = DXGI_FORMAT_R8G8B8A8_UNORM;
		if (compression_type == TextureCompression::BC4) {
			texture_format = DXGI_FORMAT_R8_UNORM;
		}
		else if (compression_type == TextureCompression::BC5) {
			texture_format = DXGI_FORMAT_R8G8_UNORM;
		}
		else if (compression_type == TextureCompression::BC6) {
			texture_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		}

		// Check if the compression type conforms to the range
		if ((unsigned int)compression_type > (unsigned int)TextureCompression::BC5) {
			SetErrorMessage(error_message, "Incorrect compression type - out of range.");
			return compressed_data;
		}

		// Check to see that the dimensions are multiple of 4
		if ((width & 3) != 0 || (height & 3) != 0) {
			SetErrorMessage(error_message, "Cannot compress textures that do not have dimensions multiple of 4.");
			return compressed_data;
		}

		DirectX::ScratchImage initial_image;
		SetInternalImageAllocator(&initial_image, allocator);

		initial_image.Initialize2D(texture_format, width, height, 1, data.size);
		DXGI_FORMAT compressed_format = COMPRESSED_FORMATS[(unsigned int)compression_type];
		DirectX::ScratchImage final_image;
		SetInternalImageAllocator(&final_image, allocator);

		DirectX::TEX_COMPRESS_FLAGS compress_flag = GetCompressionFlag(flags);

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
			compressed_format,
			compress_flag,
			DirectX::TEX_THRESHOLD_DEFAULT,
			final_image
		);

		if (FAILED(result)) {
			SetErrorMessage(error_message, "Texture compression failed during codec.");
			return compressed_data;
		}

		Stream<void>* streams = (Stream<void>*)Allocate(allocator, sizeof(Stream<void>) * data.size + final_image.GetPixelsSize());
		uintptr_t ptr = (uintptr_t)streams;
		ptr += sizeof(Stream<void>) * data.size;

		for (size_t index = 0; index < data.size; index++) {
			const DirectX::Image* image = final_image.GetImage(index, 0, 0);
			// The row pitch must be divided by 4 because the function returns the byte count of 4 scanlines - not for a single one
			streams[index] = { image->pixels, image->rowPitch * height };
		}

		final_image.DetachPixels();
		compressed_data = { streams, data.size };
		return compressed_data;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	Stream<Stream<void>> CompressTexture(
		Stream<Stream<void>> data,
		size_t width,
		size_t height, 
		TextureCompressionExplicit compression_type,
		AllocatorPolymorphic allocator, 
		size_t flags, 
		CapacityStream<char>* error_message
	)
	{
		return CompressTexture(
			data,
			width,
			height,
			EXPLICIT_COMPRESSION_MAPPING[(unsigned int)compression_type],
			allocator,
			flags | EXPLICIT_COMPRESSION_FLAGS[(unsigned int)compression_type],
			error_message
		);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	TextureCompressionExplicit GetTextureCompressionType(const wchar_t* texture)
	{
		Stream<wchar_t> path = ToStream(texture);
		Path extension = function::PathExtensionBoth(path);

		// Fail
		if (extension.size == 0) {
			return (TextureCompressionExplicit)-1;
		}

		bool is_hdr = false;
		bool is_tga = false;
		is_hdr = function::CompareStrings(extension, ToStream(L".hdr"));
		is_tga = function::CompareStrings(extension, ToStream(L".tga"));

		if (is_hdr) {
			return TextureCompressionExplicit::HDRMap;
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
			return (TextureCompressionExplicit)-1;
		}

		return GetTextureCompressionType(metadata.format);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	TextureCompressionExplicit GetTextureCompressionType(DXGI_FORMAT format)
	{
		if (format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
			return TextureCompressionExplicit::ColorMap;
		}
		if (format == DXGI_FORMAT_R8_UNORM) {
			return TextureCompressionExplicit::GrayscaleMap;
		}
		if (format == DXGI_FORMAT_R8G8_UNORM) {
			return TextureCompressionExplicit::DoubleGrayscaleMap;
		}
		if (format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
			return TextureCompressionExplicit::HDRMap;
		}
		return (TextureCompressionExplicit)-1;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	DXGI_FORMAT GetCompressedRenderFormat(TextureCompression compression, bool srgb)
	{
		DXGI_FORMAT compressed_format;

		if (srgb) {
			compressed_format = COMPRESSED_FORMATS_SRGB[(unsigned int)compression];
		}
		else {
			compressed_format = COMPRESSED_FORMATS[(unsigned int)compression];
		}

		return compressed_format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	DXGI_FORMAT GetCompressedRenderFormat(TextureCompressionExplicit compression, bool srgb)
	{
		return GetCompressedRenderFormat(EXPLICIT_COMPRESSION_MAPPING[(unsigned int)compression], srgb);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

}