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

	struct LinearAllocator;
	struct StackAllocator;
	struct MultipoolAllocator;
	struct MemoryManager;
	struct MemoryArena;
	struct ResizableLinearAllocator;
	struct MemoryProtectedAllocator;

	struct AllocatorPolymorphic {
		ECS_INLINE AllocatorPolymorphic() : allocator(nullptr) {}
		ECS_INLINE AllocatorPolymorphic(std::nullptr_t nullptr_t) : allocator(nullptr) {}
		ECS_INLINE AllocatorPolymorphic(LinearAllocator* _allocator) {
			allocator = _allocator;
			allocator_type = ECS_ALLOCATOR_LINEAR;
		}
		ECS_INLINE AllocatorPolymorphic(StackAllocator* _allocator) {
			allocator = _allocator;
			allocator_type = ECS_ALLOCATOR_STACK;
		}
		ECS_INLINE AllocatorPolymorphic(MultipoolAllocator* _allocator) {
			allocator = _allocator;
			allocator_type = ECS_ALLOCATOR_MULTIPOOL;
		}
		ECS_INLINE AllocatorPolymorphic(MemoryManager* _allocator) {
			allocator = _allocator;
			allocator_type = ECS_ALLOCATOR_MANAGER;
		}
		ECS_INLINE AllocatorPolymorphic(MemoryArena* _allocator) {
			allocator = _allocator;
			allocator_type = ECS_ALLOCATOR_ARENA;
		}
		ECS_INLINE AllocatorPolymorphic(ResizableLinearAllocator* _allocator) {
			allocator = _allocator;
			allocator_type = ECS_ALLOCATOR_RESIZABLE_LINEAR;
		}
		ECS_INLINE AllocatorPolymorphic(MemoryProtectedAllocator* _allocator) {
			allocator = _allocator;
			allocator_type = ECS_ALLOCATOR_MEMORY_PROTECTED;
		}
		ECS_INLINE AllocatorPolymorphic(void* _allocator, ECS_ALLOCATOR_TYPE _allocator_type, ECS_ALLOCATION_TYPE _allocation_type = ECS_ALLOCATION_SINGLE)
			: allocator(_allocator), allocator_type(_allocator_type), allocation_type(_allocation_type) {}

		ECS_INLINE void SetMulti() {
			allocation_type = ECS_ALLOCATION_MULTI;
		}

		ECS_INLINE AllocatorPolymorphic AsMulti() const {
			return { allocator, allocator_type, ECS_ALLOCATION_MULTI };
		}

		void* allocator;
		ECS_ALLOCATOR_TYPE allocator_type;
		ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_SINGLE;
	};

	struct ECSENGINE_API Copyable {
		ECS_INLINE Copyable(size_t _byte_size) : byte_size(_byte_size) {}

		virtual void CopyBuffers(const void* other, AllocatorPolymorphic allocator) = 0;

		virtual void DeallocateBuffers(AllocatorPolymorphic allocator) = 0;

		ECS_INLINE void* GetChildData() const {
			return (void*)((uintptr_t)this + sizeof(Copyable));
		}

		ECS_INLINE size_t CopySize() const {
			return sizeof(Copyable) + byte_size;
		}

		// Implemented in AllocatorPolymorphic.cpp
		// Allocates a data block and then initializes any buffers that are needed
		Copyable* Copy(AllocatorPolymorphic allocator) const;

		// Implemented in AllocatorPolymoprhic.cpp
		// It assumes that the pointer data has enough space for all the buffers!
		Copyable* CopyTo(uintptr_t& ptr) const;

		size_t byte_size;
	};

	// Takes care of the nullptr case
	ECS_INLINE size_t CopyableCopySize(const Copyable* copyable) {
		return copyable != nullptr ? copyable->CopySize() : 0;
	}

	// Takes care of the nullptr case
	ECS_INLINE Copyable* CopyableCopy(const Copyable* copyable, AllocatorPolymorphic allocator) {
		return copyable != nullptr ? copyable->Copy(allocator) : nullptr;
	}

	// Takes care of the nullptr case
	ECS_INLINE Copyable* CopyableCopyTo(const Copyable* copyable, uintptr_t& ptr) {
		return copyable != nullptr ? copyable->CopyTo(ptr) : nullptr;
	}

	// Takes care of the nullptr case and also deallocates the pointer, if it is allocated
	// Besides the allocated buffers
	// Implemented in AllocatorPolymorphic.cpp
	ECSENGINE_API void CopyableDeallocate(Copyable* copyable, AllocatorPolymorphic allocator);

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

	// Helper function that allocates an instance and calls the constructor for it. Helpful for polymorphic types that need the vtable
	template<typename T, typename Allocator, typename... Args>
	ECS_INLINE T* AllocateAndConstruct(Allocator* allocator, Args... arguments) {
		T* allocation = (T*)allocator->Allocate(sizeof(T), alignof(T));
		if (allocation != nullptr) {
			new (allocation) T(arguments...);
		}
		return allocation;
	}

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