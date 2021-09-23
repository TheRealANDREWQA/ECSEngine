#pragma once
#include "../../Core.h"

namespace ECSEngine {

	// Allocator designed to be used as a bump allocator for multithreaded allocations
	// Main usage is for Task Manager
	struct ECSENGINE_API TaskAllocator {
		TaskAllocator() : top(0), marker(0), capacity(0), buffer(nullptr) {}
		TaskAllocator(void* buffer, size_t capacity) : top(0), marker(0), capacity(capacity), buffer(buffer) {}

		TaskAllocator(const TaskAllocator& other) = default;
		TaskAllocator& operator = (const TaskAllocator& other);

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