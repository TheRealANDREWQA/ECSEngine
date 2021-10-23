#pragma once
#include "../../Core.h"
#include "../RenderingStructures.h"
#include "../../Containers/Stream.h"
#include "TextureCompressionTypes.h"

ECS_CONTAINERS;

namespace ECSEngine {

	// If the texture resides in GPU memory with no CPU access, for CPU compression i.e. BC1, BC3, BC4 
	// and BC5, it will have to be copied to a staging texture, compressed and then copied backwards
	// to the original texture; it relies on the immediate context for the staging texture and supplying
	// the data to the cpu
	bool CompressTexture(Texture2D& texture, TextureCompression compression_type, size_t flags = 0, CapacityStream<char>* error_message = nullptr);

	// It relies on the immediate context for the staging texture
	bool CompressTexture(Texture2D& texture, TextureCompressionExplicit explicit_compression_type, CapacityStream<char>* error_message = nullptr);

}