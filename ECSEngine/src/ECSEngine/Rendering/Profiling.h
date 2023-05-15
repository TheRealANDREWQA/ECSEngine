#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	// Can hold a maximum amount of timestamps
	// It uses double buffering to keep performance reasonable
	struct GPUProfiler {
		GPUProfiler(void* buffer, size_t capacity);
		GPUProfiler(AllocatorPolymorphic allocator, size_t capacity = 256);


		CapacityStream<size_t> timestamps[2];
	};

}