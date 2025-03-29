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
		ECS_INLINE void GPULock() const {
			if (gpu_lock != nullptr) {
				gpu_lock->Lock();
			}
		}

		ECS_INLINE void GPUUnlock() const {
			if (gpu_lock != nullptr) {
				gpu_lock->Unlock();
			}
		}

		AllocatorPolymorphic allocator = ECS_MALLOC_ALLOCATOR;
		ECS_TEXTURE_COMPRESS_FLAGS flags = ECS_TEXTURE_COMPRESS_NONE;
		CapacityStream<char>* error_message = nullptr;
		// If set, it will acquire lock it to synchronize the access to the immediate context
		SpinLock* gpu_lock = nullptr;
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
		const CompressTextureDescriptor& descriptor = {},
		DebugInfo debug_info = ECS_DEBUG_INFO
	);

	// It relies on the immediate context for the staging texture
	// The graphics is needed only to keep track of the newly created resource and to remove the old one if it exists
	ECSENGINE_API bool CompressTexture(
		Graphics* graphics,
		Texture2D& texture, 
		ECS_TEXTURE_COMPRESSION_EX explicit_compression_type,
		const CompressTextureDescriptor& descriptor = {},
		DebugInfo debug_info = ECS_DEBUG_INFO
	);

	// Data is a stream of data for each of the textures mip levels
	// It returns a null texture if it fails. When using a GPU codec, it will rely on the immediate context
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
		const CompressTextureDescriptor& descriptor = {},
		DebugInfo debug_info = ECS_DEBUG_INFO
	);

	// Data is a stream of data for each of the textures mip levels
	// It returns a null texture if it fails. When using a GPU codec, it will rely on the immediate context
	// The graphics is needed to create the texture object and optionally keep track of it
	ECSENGINE_API Texture2D CompressTexture(
		Graphics* graphics,
		Stream<Stream<void>> data,
		size_t width,
		size_t height,
		ECS_TEXTURE_COMPRESSION_EX compression_type,
		bool temporary_texture = false,
		const CompressTextureDescriptor& descriptor = {},
		DebugInfo debug_info = ECS_DEBUG_INFO
	);

	// It doesn't relly on the immediate context - if no allocator is specified, it will use Malloc to generate the temporary data
	// Data is a stream of data for each of the textures mip levels
	// It returns a null texture if it fails
	// In order to deallocate the received data, just deallocate the buffer of the first stream
	// e.g. new_data[0].buffer
	ECSENGINE_API bool CompressTexture(
		Stream<Stream<void>> data,
		Stream<void>* new_data,
		size_t width,
		size_t height,
		ECS_TEXTURE_COMPRESSION compression_type,
		const CompressTextureDescriptor& descriptor = {}
	);

	// It doesn't relly on the immediate context - if no allocator is specified, it will use Malloc to generate the temporary data
	// Data is a stream of data for each of the textures mip levels
	// It returns a null texture if it fails
	// In order to deallocate the received data, just deallocate the buffer of the first stream
	// e.g. new_data[0].buffer
	ECSENGINE_API bool CompressTexture(
		Stream<Stream<void>> data,
		Stream<void>* new_data,
		size_t width,
		size_t height,
		ECS_TEXTURE_COMPRESSION_EX compression_type,
		const CompressTextureDescriptor& descriptor = {}
	);

	// Converts the extended types to their underlying compression type
	ECSENGINE_API ECS_TEXTURE_COMPRESSION TextureCompressionFromEx(ECS_TEXTURE_COMPRESSION_EX ex_type);

}