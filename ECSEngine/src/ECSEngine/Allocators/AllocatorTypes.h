#pragma once

namespace ECSEngine {

	enum class AllocatorType : unsigned char {
		LinearAllocator,
		StackAllocator,
		PoolAllocator,
		MultipoolAllocator,
		MemoryManager,
		GlobalMemoryManager,
		MemoryArena,
		ResizableMemoryArena
	};

	enum class AllocationType : unsigned char {
		SingleThreaded,
		MultiThreaded
	};

	struct AllocatorPolymorphic {
		void* allocator;
		AllocatorType allocator_type;
		AllocationType allocation_type;
	};

}