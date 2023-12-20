#pragma once
#include "../Core.h"


namespace ECSEngine {

	// SettingsAllocator designed to be used as a bump allocator for multithreaded allocations
	// Uses atomic fetch_add for additions. The only difference between it and the 
	// AtomicStream is that it can handle size_t data sizes and that it uses a single
	// atomic variable instead of the 2 of the atomic stream
	struct ECSENGINE_API AtomicLinearAllocator {
		AtomicLinearAllocator() : top(0), marker(0), capacity(0), buffer(nullptr) {}
		AtomicLinearAllocator(void* buffer, size_t capacity) : top(0), marker(0), capacity(capacity), buffer(buffer) {}

		AtomicLinearAllocator(const AtomicLinearAllocator& other) = default;
		AtomicLinearAllocator& operator = (const AtomicLinearAllocator& other);

		void Clear();

		void SetMarker();

		void ReturnToMarker();

		void* Allocate(size_t size, size_t alignment = 8);

		void Deallocate(const void* buffer);

		std::atomic<size_t> top;
		size_t marker;
		size_t capacity;
		void* buffer;
	};

}