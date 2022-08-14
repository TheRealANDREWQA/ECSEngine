#pragma once
#include "../../Core.h"

namespace ECSEngine {

	// The producer is single threaded. The consumer only reports when it has finished using that block
	struct ECSENGINE_API RingBuffer {
		RingBuffer() {}
		RingBuffer(void* _buffer, size_t _size, size_t _capacity) : buffer(_buffer), size(_size), capacity(_capacity), last_in_use(0) {}

		RingBuffer(const RingBuffer& other);
		RingBuffer& operator = (const RingBuffer& other);

		void* Allocate(size_t allocation_size);

		void Finish(const void* buffer, size_t allocation_size);

		void InitializeFromBuffer(uintptr_t& ptr, size_t _capacity);

		void* buffer;
		size_t size;
		size_t capacity;
		std::atomic<size_t> last_in_use;
	};

}