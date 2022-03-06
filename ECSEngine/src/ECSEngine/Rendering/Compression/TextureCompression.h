#pragma once
#include "../../Core.h"
#include "../RenderingStructures.h"
#include "../../Containers/Stream.h"
#include "TextureCompressionTypes.h"
#include "../../Internal/Multithreading/ConcurrentPrimitives.h"
#include "../../Allocators/AllocatorTypes.h"

namespace ECSEngine {

	struct Graphics;

	// Works for BC1, BC3 and BC7
	inline size_t ECS_TEXTURE_COMPRESS_SRGB = 1 << 1;
	// Adds to the bind flags the render target flag
	inline size_t ECS_TEXTURE_COMPRESS_BIND_RENDER_TARGET = 1 << 2;
	inline size_t ECS_TEXTURE_COMPRESS_DISABLE_MULTICORE = 1 << 3;

	// If the texture resides in GPU memory with no CPU access, for CPU compression i.e. BC1, BC3, BC4 
	// and BC5, it will have to be copied to a staging texture, compressed and then copied backwards
	// to the original texture; it relies on the immediate context for the staging texture and supplying the data to the cpu
	// The allocator is used to allocate memory for the temporary compressed data on the cpu side
	// The graphics is needed only to keep track of the newly created resource and to remove the old one if it exists
	ECSENGINE_API bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture, 
		TextureCompression compression_type, 
		AllocatorPolymorphic allocator = { nullptr }, 
		size_t flags = 0, 
		CapacityStream<char>*error_message = nullptr
	);

	// If the texture resides in GPU memory with no CPU access, for CPU compression i.e. BC1, BC3, BC4 
	// and BC5, it will have to be copied to a staging texture, compressed and then copied backwards
	// to the original texture; it relies on the immediate context for the staging texture and supplying
	// the data to the cpu; a lock is provided to lock the staging mapping
	// The allocator is used to allocate memory for the temporary compressed data on the cpu side
	// The graphics is needed only to keep track of the newly created resource and to remove the old one if it exists
	ECSENGINE_API bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture,
		TextureCompression compression_type,
		SpinLock* spin_lock, 
		AllocatorPolymorphic allocator = { nullptr }, 
		size_t flags = 0, 
		CapacityStream<char>*error_message = nullptr
	);

	// It relies on the immediate context for the staging texture
	// The graphics is needed only to keep track of the newly created resource and to remove the old one if it exists
	ECSENGINE_API bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture, 
		TextureCompressionExplicit explicit_compression_type,
		AllocatorPolymorphic allocator = {nullptr},
		size_t flags = 0,
		CapacityStream<char>* error_message = nullptr
	);

	// It relies on the immediate context for the staging texture and a lock is provided to lock the staging mapping
	// The graphics is needed only to keep track of the newly created resource and to remove the old one if it exists
	ECSENGINE_API bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture, 
		TextureCompressionExplicit explicit_compression_type, 
		SpinLock* spin_lock, 
		AllocatorPolymorphic allocator = {nullptr},
		size_t flags = 0, 
		CapacityStream<char>* error_message = nullptr
	);

	// It doesn't relly on the immediate context - if no allocator is specified, it will use malloc to generate the temporary data
	// Data is a stream of data for each of the textures mip levels
	// GPU codecs should not use this compression function
	// It returns a null texture if it fails
	// The elements of the data stream must be a Stream<const void> which has as its size the row pitch of the mip level
	// When mapping the texture for copying the data use the D3D11_MAPPED_RESOURCE RowPitch member to set these streams
	// The graphics is needed only to keep track of the newly created resource and to remove the old one if it exists
	ECSENGINE_API Texture2D CompressTexture(
		Graphics* graphics,
		Stream<Stream<void>> data, 
		size_t width, 
		size_t height,
		TextureCompression compression_type,
		AllocatorPolymorphic allocator = {nullptr},
		size_t flags = 0, 
		CapacityStream<char>* error_message = nullptr
	);

	// It doesn't relly on the immediate context - if no allocator is specified, it will use malloc to generate the temporary data
	// Data is a stream of data for each of the textures mip levels
	// GPU codecs should not use this compression function
	// It returns a null texture if it fails
	// The graphics is needed only to keep track of the newly created resource and to remove the old one if it exists
	ECSENGINE_API Texture2D CompressTexture(
		Graphics* graphics,
		Stream<Stream<void>> data,
		size_t width, 
		size_t height, 
		TextureCompressionExplicit compression_type, 
		AllocatorPolymorphic allocator = {nullptr},
		size_t flags = 0,
		CapacityStream<char>* error_message = nullptr
	);

	// It doesn't relly on the immediate context - if no allocator is specified, it will use malloc to generate the temporary data
	// Data is a stream of data for each of the textures mip levels
	// GPU codecs should not use this compression function
	// It returns a null texture if it fails
	// In order to deallocate the received data, just deallocate the buffer of Stream<void>*
	ECSENGINE_API Stream<Stream<void>> CompressTexture(
		Stream<Stream<void>> data,
		size_t width,
		size_t height,
		TextureCompression compression_type,
		AllocatorPolymorphic allocator = {nullptr},
		size_t flags = 0,
		CapacityStream<char>* error_message = nullptr
	);

	// It doesn't relly on the immediate context - if no allocator is specified, it will use malloc to generate the temporary data
	// Data is a stream of data for each of the textures mip levels
	// GPU codecs should not use this compression function
	// It returns a null texture if it fails
	// In order to deallocate the received data, just deallocate the buffer of Stream<void>*
	ECSENGINE_API Stream<Stream<void>> CompressTexture(
		Stream<Stream<void>> data,
		size_t width,
		size_t height,
		TextureCompressionExplicit compression_type,
		AllocatorPolymorphic allocator = {nullptr},
		size_t flags = 0,
		CapacityStream<char>* error_message = nullptr
	);

	// Returns the appropriate compression type for the given texture - based on the extension which will get translated into a DXGI_FORMAT
	// For color maps it will give back BC1 and for normal maps the low quality one (i.e. still BC1)
	// It cannot make distinction between 3 color and 4 color channel types - it won't give back ColorMapWithAlpha or CutoutMap
	// If it fails it will return an invalid -1 texture compression type
	ECSENGINE_API TextureCompressionExplicit GetTextureCompressionType(const wchar_t* texture);

	// Returns the appropriate compression type for the given texture - based on the format
	// For color maps and normal maps it will give back BC1
	// It cannot make distinction between 3 color and 4 color channel types - it won't give back ColorMapWithAlpha or CutoutMap
	// If it fails it will return an invalid -1 texture compression type
	ECSENGINE_API TextureCompressionExplicit GetTextureCompressionType(DXGI_FORMAT format);

	ECSENGINE_API DXGI_FORMAT GetCompressedRenderFormat(TextureCompression compression, bool srgb = false);

	ECSENGINE_API DXGI_FORMAT GetCompressedRenderFormat(TextureCompressionExplicit compression, bool srgb = false);

	inline bool IsTextureCompressionTypeValid(TextureCompressionExplicit compression_type) {
		return (unsigned int)compression_type <= (unsigned int)TextureCompressionExplicit::ColorMapHighQuality;
	}

	inline bool IsTextureCompressionTypeValid(TextureCompression compression_type) {
		return (unsigned int)compression_type <= (unsigned int)TextureCompression::BC7;
	}

}