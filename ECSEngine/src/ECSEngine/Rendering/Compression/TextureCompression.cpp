#include "ecspch.h"
#include "TextureCompression.h"
#include "../../../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../../Utilities/Function.h"
#include "../GraphicsHelpers.h"

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

	template<typename void (*LockFunction)(SpinLock*), typename void (*UnlockFunction)(SpinLock*)>
	bool CompressTextureImplementation(Texture2D& texture, TextureCompression compression_type, SpinLock* spin_lock, size_t flags, CapacityStream<char>* error_message) {
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
		initial_image.Initialize2D(texture_descriptor.Format, texture_descriptor.Width, texture_descriptor.Height, texture_descriptor.ArraySize, texture_descriptor.MipLevels);
		DXGI_FORMAT compressed_format = COMPRESSED_FORMATS[(unsigned int)compression_type];
		DirectX::ScratchImage final_image;

		DirectX::TEX_COMPRESS_FLAGS compress_flag = DirectX::TEX_COMPRESS_DEFAULT;
		compress_flag = AddCompressionFlag(compress_flag, DirectX::TEX_COMPRESS_UNIFORM, function::HasFlag(flags, ECS_TEXTURE_COMPRESSION_DISABLE_PERCEPTUAL_WEIGHTING));

		GraphicsDevice* device = nullptr;
		texture.tex->GetDevice(&device);
		GraphicsContext* context = nullptr;
		device->GetImmediateContext(&context);

		D3D11_MAPPED_SUBRESOURCE mapping;

		// Create a staging resource that will copy the original texture and map every mip level
		// For compression
		LockFunction(spin_lock);
		Texture2D staging_texture = ToStaging(texture);
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
				compress_flag | DirectX::TEX_COMPRESS_PARALLEL,
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
		texture_descriptor.BindFlags = function::RemoveFlag(texture_descriptor.BindFlags, D3D11_BIND_RENDER_TARGET);
		texture_descriptor.MiscFlags = function::RemoveFlag(texture_descriptor.MiscFlags, D3D11_RESOURCE_MISC_GENERATE_MIPS);
		result = device->CreateTexture2D(&texture_descriptor, new_texture_data, &texture.tex);
		if (FAILED(result)) {
			SetErrorMessage(error_message, "Creating final texture after compression failed.");
			return false;
		}

		unsigned int countu = old_texture->Release();

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

	bool CompressTexture(Texture2D& texture, TextureCompression compression_type, size_t flags, CapacityStream<char>* error_message) {
		return CompressTextureImplementation<NothingSpinLock, NothingSpinLock>(texture, compression_type, nullptr, flags, error_message);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	bool CompressTexture(Texture2D& texture, TextureCompression compression_type, SpinLock* spin_lock, size_t flags, CapacityStream<char>* error_message) {
		return CompressTextureImplementation<LockSpinLock, UnlockSpinLock>(texture, compression_type, spin_lock, flags, error_message);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	bool CompressTexture(Texture2D& texture, TextureCompressionExplicit explicit_compression_type, CapacityStream<char>* error_message) {
		return CompressTexture(
			texture, 
			EXPLICIT_COMPRESSION_MAPPING[(unsigned int)explicit_compression_type], 
			EXPLICIT_COMPRESSION_FLAGS[(unsigned int)explicit_compression_type],
			error_message
		);
	}

	// --------------------------------------------------------------------------------------------------------------------------------------

	bool CompressTexture(Texture2D& texture, TextureCompressionExplicit explicit_compression_type, SpinLock* spin_lock, CapacityStream<char>* error_message) {
		return CompressTexture(
			texture,
			EXPLICIT_COMPRESSION_MAPPING[(unsigned int)explicit_compression_type], 
			spin_lock, 
			EXPLICIT_COMPRESSION_FLAGS[(unsigned int)explicit_compression_type], 
			error_message
		);
	}

}