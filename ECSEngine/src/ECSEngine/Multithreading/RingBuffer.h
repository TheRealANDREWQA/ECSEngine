#pragma once
#include "../Core.h"
#include "ConcurrentPrimitives.h"

namespace ECSEngine {

	// The producer is single threaded. The consumer only reports when it has finished using that block
	struct ECSENGINE_API RingBuffer {
		ECS_INLINE RingBuffer() {}
		ECS_INLINE RingBuffer(void* _buffer, size_t _size, size_t _capacity) : buffer(_buffer), size(_size), capacity(_capacity), last_in_use(0), profiling_mode(false) {}

		RingBuffer(const RingBuffer& other);
		RingBuffer& operator = (const RingBuffer& other);

		void* Allocate(size_t allocation_size);

		void* Allocate(size_t allocation_size, size_t alignment);

		void Finish(const void* buffer, size_t allocation_size);

		void InitializeFromBuffer(uintptr_t& ptr, size_t _capacity);

		void ExitProfilingMode();

		void SetProfilingMode(const char* name);

		void* buffer;
		size_t size;
		size_t capacity;
		std::atomic<size_t> last_in_use;
		SpinLock lock;
		bool profiling_mode;
	};

}