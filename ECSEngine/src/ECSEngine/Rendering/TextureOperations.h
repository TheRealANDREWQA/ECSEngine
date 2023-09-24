#pragma once
#include "../Core.h"
#include "RenderingStructures.h"
#include "../Resources/ResourceTypes.h"

namespace ECSEngine {

	struct Graphics;

	struct DecodedTexture {
		Stream<void> data;
		unsigned int width;
		unsigned int height;
		ECS_GRAPHICS_FORMAT format;
	};

	enum ResizeTextureFlag {
		ECS_RESIZE_TEXTURE_FILTER_POINT = 1 << 0, // Nearest neighbour, weakest and fastest filter method
		ECS_RESIZE_TEXTURE_FILTER_LINEAR = 1 << 1, // Bilinear/Trilinear Interpolation
		ECS_RESIZE_TEXTURE_FILTER_CUBIC = 1 << 2, // Bicubic/Tricubic Interpolation
		ECS_RESIZE_TEXTURE_FILTER_BOX = 1 << 3, // Box filtering
		ECS_RESIZE_TEXTURE_MIP_MAPS = 1 << 4 // Generate mip maps
	};

	// Resizing is done on the CPU; a staging texture is created and then mapped
	// It relies on the immediate context to copy the contents
	// The top is resized and a texture is created either with the first mip or with the whole
	// mip map chain
	// If it fails, it will return nullptr
	ECSENGINE_API Texture2D ResizeTextureWithStaging(
		Graphics* graphics,
		Texture2D texture,
		size_t new_width,
		size_t new_height,
		size_t resize_flags = ECS_RESIZE_TEXTURE_FILTER_LINEAR,
		bool temporary = false
	);

	// Resizing is done on the CPU; the texture is mapped directly with D3D11_MAP_READ
	// It relies on the immediate context to copy the contents
	// The top is resized and a texture is created either with the first mip or with the whole
	// mip map chain
	// If it fails, it will return nullptr
	ECSENGINE_API Texture2D ResizeTextureWithMap(
		Graphics* graphics,
		Texture2D texture,
		size_t new_width,
		size_t new_height,
		size_t resize_flags = ECS_RESIZE_TEXTURE_FILTER_LINEAR,
		bool temporary = false
	);

	// Resizing is done on the CPU; it will not modify the given data and take parameters from the texture definition
	// The top is resized and a texture is created either with the first mip or with the whole
	// mip map chain
	// If it fails, it will return nullptr
	ECSENGINE_API Texture2D ResizeTexture(
		Graphics* graphics,
		void* texture_data,
		Texture2D texture,
		size_t new_width,
		size_t new_height,
		AllocatorPolymorphic allocator = { nullptr },
		size_t resize_flags = ECS_RESIZE_TEXTURE_FILTER_LINEAR,
		bool temporary = false
	);

	// Resizing is done on the CPU; an allocation will be made for the newly resized data
	// If it fails, it will return { nullptr, 0 }
	ECSENGINE_API Stream<void> ResizeTexture(
		void* texture_data,
		size_t current_width,
		size_t current_height,
		ECS_GRAPHICS_FORMAT format,
		size_t new_width,
		size_t new_height,
		AllocatorPolymorphic allocator = { nullptr },
		size_t resize_flags = ECS_RESIZE_TEXTURE_FILTER_LINEAR
	);

#define ECS_DECODE_TEXTURE_HDR_TONEMAP (1 << 0)
#define ECS_DECODE_TEXTURE_NO_SRGB (1 << 1)
#define ECS_DECODE_TEXTURE_FORCE_SRGB (1 << 2)

	// Returns the section of the texture without the header - it will do internally another allocation for the 
	// decompressed data;
	// If it fails it returns a { nullptr, 0 } 
	// FLAGS: ECS_DECODE_TEXTURE_HDR_TONEMAP, ECS_DECODE_TEXTURE_NO_SRGB, ECS_DECODE_TEXTURE_FORCE_SRGB
	ECSENGINE_API DecodedTexture DecodeTexture(Stream<void> data, TextureExtension extension, AllocatorPolymorphic allocator, size_t flags = 0);

	// Returns the section of the texture without the header - it will do internally another allocation for the 
	// decompressed data; The filename is needed in for the texture extension
	// If it fails it returns a { nullptr, 0 } 
	// FLAGS: ECS_DECODE_TEXTURE_HDR_TONEMAP, ECS_DECODE_TEXTURE_NO_SRGB, ECS_DECODE_TEXTURE_FORCE_SRGB
	ECSENGINE_API DecodedTexture DecodeTexture(Stream<void> data, Stream<wchar_t> filename, AllocatorPolymorphic allocator, size_t flags = 0);

	// Convert a texture from ECS_GRAPHICS_FORMAT_R8_UNORM to ECS_GRAPHICS_FORMAT_RGBA8_UNORM texture
	// Allocator nullptr means use malloc. In order to deallocate the data, just deallocate
	// the buffer of the stream of streams.
	ECSENGINE_API Stream<Stream<void>> ConvertSingleChannelTextureToGrayscale(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// Convert a texture from ECS_GRAPHICS_FORMAT_R8_UNORM to ECS_GRAPHICS_FORMAT_RGBA8_UNORM texture
	// Allocator nullptr means use malloc. In order to deallocate the data, just deallocate
	// the first buffer (it uses a coalesced allocation). It will overwrite the mip_data stream
	ECSENGINE_API void ConvertSingleChannelTextureToGrayscaleInPlace(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// Convert a texture from 2, 3 or 4 8 bit channels into a single 8 bit channel texture
	// If the channel to copy is specified, only the selected channel is used to copy into the new
	// texture. Otherwise it will assume the red channel.
	// Allocator nullptr means use malloc. In order to deallocate the data, just deallocate
	// the buffer of the return.
	ECSENGINE_API Stream<Stream<void>> ConvertTextureToSingleChannel(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		size_t channel_count = 4,
		size_t channel_to_copy = 0,
		AllocatorPolymorphic allocator = { nullptr }
	);
	
	// Convert a texture from 2, 3 or 4 8 bit channels into a single 8 bit channel texture
	// If the channel to copy is specified, only the selected channel is used to copy into the new
	// texture. Otherwise it will assume the red channel.
	// Allocator nullptr means use malloc. In order to deallocate the data, just deallocate
	// the first buffer (it uses a coalesced allocation). It will overwrite the mip_data stream
	ECSENGINE_API void ConvertTextureToSingleChannelInPlace(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		size_t channel_count = 4,
		size_t channel_to_copy = 0,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// Converts a texture with 3 8 bit channels into 4 8 bit channels with the alpha set to 255
	// Allocator nullptr means use malloc. In order to deallocate the data, just deallocate
	// the buffer of the return
	ECSENGINE_API Stream<Stream<void>> ConvertRGBTextureToRGBA(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// Converts a texture with 3 8 bit channels into 4 8 bit channels with the alpha set to 255
	// Allocator nullptr means use malloc. In order to deallocate the data, just deallocate
	// the first buffer (it uses a coalesced allocation). It will overwrite the mip_data stream
	ECSENGINE_API void ConvertRGBTexturetoRGBAInPlace(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		AllocatorPolymorphic allocator = { nullptr }
	);

	// It will use the immediate context if none specified. If a deffered context is specified, the copy calls
	// will be placed on the command list.
	// It will create a texture cube and then CopySubresourceRegion into it
	// It will have default usage and all mips will be copied
	// All textures must have the same size
	ECSENGINE_API TextureCube ConvertTexturesToCube(
		Graphics* graphics,
		Texture2D x_positive,
		Texture2D x_negative,
		Texture2D y_positive,
		Texture2D y_negative,
		Texture2D z_positive,
		Texture2D z_negative,
		bool temporary = false
	);

	// Expects all the textures to be in the correct order
	// All the constraints that apply to the 6 texture parameter version apply to this one aswell
	ECSENGINE_API TextureCube ConvertTexturesToCube(Graphics* graphics, const Texture2D* textures, bool temporary = false);

	// Equirectangular to cube map
	// Involves setting up multiple renders - can be used only in single thread circumstances
	// It will overwrite most of the pipeline; rebind resources if needed again
	ECSENGINE_API TextureCube ConvertTextureToCube(
		Graphics* graphics,
		ResourceView texture_view,
		ECS_GRAPHICS_FORMAT cube_format,
		uint2 face_size,
		bool temporary = false
	);

	// Returns the dimensions of a texture from a file
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensions(Stream<wchar_t> filename);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensions(Stream<void> data, TextureExtension texture_extension);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsPNG(Stream<void> data);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsJPG(Stream<void> data);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsBMP(Stream<void> data);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsTIFF(Stream<void> data);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsTGA(Stream<void> data);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsHDR(Stream<void> data);

	// It will assert that the mip level is in bounds
	ECSENGINE_API unsigned int GetTextureDimensions(Texture1D texture, unsigned int mip_level = 0);

	// It will assert that the mip level is in bounds
	ECSENGINE_API uint2 GetTextureDimensions(Texture2D texture, unsigned int mip_level = 0);

	// It will assert that the mip level is in bounds
	ECSENGINE_API uint3 GetTextureDimensions(Texture3D texture, unsigned int mip_level = 0);

	ECSENGINE_API Texture1DDescriptor GetTextureDescriptor(Texture1D texture);

	ECSENGINE_API Texture2DDescriptor GetTextureDescriptor(Texture2D texture);

	ECSENGINE_API Texture3DDescriptor GetTextureDescriptor(Texture3D texture);

	ECSENGINE_API TextureCubeDescriptor GetTextureDescriptor(TextureCube texture);

	// If position > bound, then the position is set to bound
	ECSENGINE_API unsigned int ClampToTextureBounds(Texture1D texture, unsigned int position, unsigned int mip_level = 0);

	// If position > bound, then the position is set to bound
	ECSENGINE_API uint2 ClampToTextureBounds(Texture2D texture, uint2 position, unsigned int mip_level = 0);

	// If position > bound, then the position is set to bound
	ECSENGINE_API uint3 ClampToTextureBounds(Texture3D texture, uint3 position, unsigned int mip_level = 0);

}