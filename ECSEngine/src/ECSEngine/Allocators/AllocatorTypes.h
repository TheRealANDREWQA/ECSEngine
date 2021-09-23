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

}