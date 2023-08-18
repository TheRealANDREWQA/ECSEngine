#pragma once

namespace ECSEngine {

	enum ECS_ALLOCATOR_TYPE : unsigned char {
		ECS_ALLOCATOR_LINEAR,
		ECS_ALLOCATOR_STACK,
		ECS_ALLOCATOR_MULTIPOOL,
		ECS_ALLOCATOR_MANAGER,
		ECS_ALLOCATOR_GLOBAL_MANAGER,
		ECS_ALLOCATOR_ARENA,
		ECS_ALLOCATOR_RESIZABLE_ARENA,
		ECS_ALLOCATOR_RESIZABLE_LINEAR
	};

	enum ECS_ALLOCATION_TYPE : unsigned char {
		ECS_ALLOCATION_SINGLE,
		ECS_ALLOCATION_MULTI
	};

	struct AllocatorPolymorphic {
		void* allocator;
		ECS_ALLOCATOR_TYPE allocator_type;
		ECS_ALLOCATION_TYPE allocation_type;
	};

#define ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(return_type, function_name, ...) template ECSENGINE_API return_type function_name(LinearAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(StackAllocator*, __VA_ARGS__); \
/*template ECSENGINE_API return_type function_name(PoolAllocator*, __VA_ARGS__);*/ \
template ECSENGINE_API return_type function_name(MultipoolAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MemoryManager*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(GlobalMemoryManager*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MemoryArena*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(ResizableMemoryArena*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(ResizableLinearAllocator*, __VA_ARGS__);

#define ECS_TEMPLATE_FUNCTION_ALLOCATOR(return_type, function_name, ...) template return_type function_name(LinearAllocator*, __VA_ARGS__); \
template return_type function_name(StackAllocator*, __VA_ARGS__); \
/*template return_type function_name(PoolAllocator*, __VA_ARGS__);*/ \
template return_type function_name(MultipoolAllocator*, __VA_ARGS__); \
template return_type function_name(MemoryManager*, __VA_ARGS__); \
template return_type function_name(GlobalMemoryManager*, __VA_ARGS__); \
template return_type function_name(MemoryArena*, __VA_ARGS__); \
template return_type function_name(ResizableMemoryArena*, __VA_ARGS__); \
template return_type function_name(ResizableLinearAllocator*, __VA_ARGS__);

}