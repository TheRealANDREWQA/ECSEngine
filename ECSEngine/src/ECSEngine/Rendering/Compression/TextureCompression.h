#pragma once
#include "../../Core.h"
#include "../RenderingStructures.h"
#include "../../Containers/Stream.h"
#include "TextureCompressionTypes.h"
#include "../../Internal/Multithreading/ConcurrentPrimitives.h"

ECS_CONTAINERS;

namespace ECSEngine {

	// If the texture resides in GPU memory with no CPU access, for CPU compression i.e. BC1, BC3, BC4 
	// and BC5, it will have to be copied to a staging texture, compressed and then copied backwards
	// to the original texture; it relies on the immediate context for the staging texture and supplying
	// the data to the cpu
	ECSENGINE_API bool CompressTexture(Texture2D& texture, TextureCompression compression_type, size_t flags = 0, CapacityStream<char>* error_message = nullptr);

	// If the texture resides in GPU memory with no CPU access, for CPU compression i.e. BC1, BC3, BC4 
	// and BC5, it will have to be copied to a staging texture, compressed and then copied backwards
	// to the original texture; it relies on the immediate context for the staging texture and supplying
	// the data to the cpu; a lock is provided to lock the staging mapping
	ECSENGINE_API bool CompressTexture(Texture2D& texture, TextureCompression compression_type, SpinLock* spin_lock, size_t flags = 0, CapacityStream<char>* error_message = nullptr);

	// It relies on the immediate context for the staging texture
	ECSENGINE_API bool CompressTexture(Texture2D& texture, TextureCompressionExplicit explicit_compression_type, CapacityStream<char>* error_message = nullptr);

	// It relies on the immediate context for the staging texture and a lock is provided to lock the staging mapping
	ECSENGINE_API bool CompressTexture(Texture2D& texture, TextureCompressionExplicit explicit_compression_type, SpinLock* spin_lock, CapacityStream<char>* error_message = nullptr);

}