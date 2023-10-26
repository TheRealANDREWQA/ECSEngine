#pragma once
#include "../Core.h"

namespace ECSEngine {

	enum ECS_ALLOCATOR_TYPE : unsigned char {
		ECS_ALLOCATOR_LINEAR,
		ECS_ALLOCATOR_STACK,
		ECS_ALLOCATOR_MULTIPOOL,
		ECS_ALLOCATOR_MANAGER,
		ECS_ALLOCATOR_ARENA,
		ECS_ALLOCATOR_RESIZABLE_LINEAR,
		ECS_ALLOCATOR_MEMORY_PROTECTED,
		ECS_ALLOCATOR_TYPE_COUNT
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

	// Only linear/stack/multipool/arena allocators can be created
	// Using this. This is intentional
	struct CreateBaseAllocatorInfo {
		ECS_INLINE CreateBaseAllocatorInfo() {}

		ECS_ALLOCATOR_TYPE allocator_type;

		union {
			struct {
				size_t linear_capacity;
			};
			struct {
				size_t stack_capacity;
			};
			struct {
				size_t multipool_block_count;
				size_t multipool_capacity;
			};
			struct {
				ECS_ALLOCATOR_TYPE arena_nested_type;
				size_t arena_allocator_count;
				size_t arena_capacity;
				// This field is used only if the nested type is multipool
				size_t arena_multipool_block_count;
			};
		};
	};

#define ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(return_type, function_name, ...) template ECSENGINE_API return_type function_name(LinearAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(StackAllocator*, __VA_ARGS__); \
/*template ECSENGINE_API return_type function_name(PoolAllocator*, __VA_ARGS__);*/ \
template ECSENGINE_API return_type function_name(MultipoolAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MemoryManager*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MemoryArena*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(ResizableLinearAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MemoryProtectedAllocator*, __VA_ARGS__);

#define ECS_TEMPLATE_FUNCTION_ALLOCATOR(return_type, function_name, ...) template return_type function_name(LinearAllocator*, __VA_ARGS__); \
template return_type function_name(StackAllocator*, __VA_ARGS__); \
/*template return_type function_name(PoolAllocator*, __VA_ARGS__);*/ \
template return_type function_name(MultipoolAllocator*, __VA_ARGS__); \
template return_type function_name(MemoryManager*, __VA_ARGS__); \
template return_type function_name(MemoryArena*, __VA_ARGS__); \
template return_type function_name(ResizableLinearAllocator*, __VA_ARGS__); \
template return_type function_name(MemoryProtectedAllocator*, __VA_ARGS__);

}