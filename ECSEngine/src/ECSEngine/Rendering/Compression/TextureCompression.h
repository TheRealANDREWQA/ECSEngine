#pragma once
#include "../../Core.h"
#include "../RenderingStructures.h"
#include "../../Containers/Stream.h"
#include "TextureCompressionTypes.h"
#include "../../Allocators/AllocatorTypes.h"

namespace ECSEngine {

	struct Graphics;
	struct SpinLock;

	struct CompressTextureDescriptor {
		AllocatorPolymorphic allocator = { nullptr };
		ECS_TEXTURE_COMPRESS_FLAGS flags = ECS_TEXTURE_COMPRESS_NONE;
		CapacityStream<char>* error_message = nullptr;
		SpinLock* spin_lock = nullptr;
	};

	// If the texture resides in GPU memory with no CPU access, for CPU compression i.e. BC1, BC3, BC4 
	// and BC5, it will have to be copied to a staging texture, compressed and then copied backwards
	// to the original texture; it relies on the immediate context for the staging texture and supplying the data to the cpu
	// The allocator is used to allocate memory for the temporary compressed data on the cpu side
	// The graphics is needed only to keep track of the newly created resource and to remove the old one if it exists
	ECSENGINE_API bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture, 
		ECS_TEXTURE_COMPRESSION compression_type, 
		CompressTextureDescriptor descriptor = {}
	);

	// It relies on the immediate context for the staging texture
	// The graphics is needed only to keep track of the newly created resource and to remove the old one if it exists
	ECSENGINE_API bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture, 
		ECS_TEXTURE_COMPRESSION_EX explicit_compression_type,
		CompressTextureDescriptor descriptor = {}
	);

	// It doesn't relly on the immediate context - if no allocator is specified, it will use malloc to generate the temporary data
	// Data is a stream of data for each of the textures mip levels
	// GPU codecs should not use this compression function
	// It returns a null texture if it fails
	// The elements of the data stream must be a Stream<const void> which has as its size the row pitch of the mip level
	// When mapping the texture for copying the data use the D3D11_MAPPED_RESOURCE RowPitch member to set these streams
	// The graphics is needed to create the texture object and optionally keep track of it
	ECSENGINE_API Texture2D CompressTexture(
		Graphics* graphics,
		Stream<Stream<void>> data, 
		size_t width, 
		size_t height,
		ECS_TEXTURE_COMPRESSION compression_type,
		bool temporary_texture = false,
		CompressTextureDescriptor descriptor = {}
	);

	// It doesn't relly on the immediate context - if no allocator is specified, it will use malloc to generate the temporary data
	// Data is a stream of data for each of the textures mip levels
	// GPU codecs should not use this compression function
	// It returns a null texture if it fails
	// The graphics is needed to create the texture object and optionally keep track of it
	ECSENGINE_API Texture2D CompressTexture(
		Graphics* graphics,
		Stream<Stream<void>> data,
		size_t width,
		size_t height,
		ECS_TEXTURE_COMPRESSION_EX compression_type,
		bool temporary_texture = false,
		CompressTextureDescriptor descriptor = {}
	);

	// It doesn't relly on the immediate context - if no allocator is specified, it will use malloc to generate the temporary data
	// Data is a stream of data for each of the textures mip levels
	// GPU codecs should not use this compression function
	// It returns a null texture if it fails
	// In order to deallocate the received data, just deallocate the buffer of the first stream
	// e.g. new_data[0].buffer
	ECSENGINE_API bool CompressTexture(
		Stream<Stream<void>> data,
		Stream<void>* new_data,
		size_t width,
		size_t height,
		ECS_TEXTURE_COMPRESSION compression_type,
		CompressTextureDescriptor descriptor = {}
	);

	// It doesn't relly on the immediate context - if no allocator is specified, it will use malloc to generate the temporary data
	// Data is a stream of data for each of the textures mip levels
	// GPU codecs should not use this compression function
	// It returns a null texture if it fails
	// In order to deallocate the received data, just deallocate the buffer of the first stream
	// e.g. new_data[0].buffer
	ECSENGINE_API bool CompressTexture(
		Stream<Stream<void>> data,
		Stream<void>* new_data,
		size_t width,
		size_t height,
		ECS_TEXTURE_COMPRESSION_EX compression_type,
		CompressTextureDescriptor descriptor = {}
	);

}