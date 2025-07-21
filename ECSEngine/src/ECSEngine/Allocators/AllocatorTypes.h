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
		ECS_ALLOCATOR_MALLOC,
		// This value describes a user defined allocator that is not one of these common
		ECS_ALLOCATOR_INTERFACE,
		ECS_ALLOCATOR_TYPE_COUNT
	};

	enum ECS_ALLOCATION_TYPE : unsigned char {
		ECS_ALLOCATION_SINGLE,
		ECS_ALLOCATION_MULTI
	};

	struct AllocatorBase;

	struct AllocatorPolymorphic {
		ECS_INLINE AllocatorPolymorphic() : allocator(nullptr) {}
		ECS_INLINE AllocatorPolymorphic(std::nullptr_t nullptr_t) : allocator(nullptr) {}
		ECS_INLINE AllocatorPolymorphic(AllocatorBase* _allocator, ECS_ALLOCATION_TYPE _allocation_type = ECS_ALLOCATION_SINGLE)
			: allocator(_allocator), allocation_type(_allocation_type) {
		}

		ECS_INLINE void SetMulti() {
			allocation_type = ECS_ALLOCATION_MULTI;
		}

		ECS_INLINE AllocatorPolymorphic AsMulti() const {
			return { allocator, ECS_ALLOCATION_MULTI };
		}

		AllocatorBase* allocator;
		ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_SINGLE;
	};

	// Returns ECS_ALLOCATOR_TYPE_COUNT if the string does not match any allocator.
	ECSENGINE_API ECS_ALLOCATOR_TYPE AllocatorTypeFromString(const char* string, size_t string_size);

	// It will assert if the string capacity is not large enough
	ECSENGINE_API void WriteAllocatorTypeToString(ECS_ALLOCATOR_TYPE type, char* string, unsigned int& string_size, unsigned int string_capacity);

	struct ECSENGINE_API Copyable {
		ECS_INLINE Copyable(size_t _byte_size) : byte_size(_byte_size) {}

	protected:
		// Make this function friend in order to allow it to call deallocate
		friend ECSENGINE_API void CopyableDeallocate(Copyable* copyable, AllocatorPolymorphic allocator);

		// This instance should copy the instance provided in the other pointer. The vtable does not need to be initialized,
		// It is initialized from outside.
		virtual void CopyImpl(const void* other, AllocatorPolymorphic allocator) = 0;

		virtual void Deallocate(AllocatorPolymorphic allocator) = 0;

		// Returns the total amount of bytes needed to copy this to a buffer (except the byte size)
		virtual size_t CopySizeImpl() const = 0;

	public:
		// In order to deallocate, use CopyableDeallocate, since it handles the nullptr case as well

		ECS_INLINE void* GetChildData() const {
			return (void*)((uintptr_t)this + sizeof(Copyable));
		}

		ECS_INLINE size_t CopySize() const {
			return sizeof(Copyable) + byte_size + CopySizeImpl();
		}

		// Implemented in AllocatorPolymorphic.cpp
		// Allocates a data block and then initializes any buffers that are needed
		Copyable* Copy(AllocatorPolymorphic allocator) const;

		// Implemented in AllocatorPolymoprhic.cpp
		// It assumes that the pointer data has enough space for all the buffers!
		Copyable* CopyTo(uintptr_t& ptr) const;

		// This is the size of the basic structure, that contains the buffers
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

	// A helper macro that implements the necessary copyable functions with implementations for a blittable type
	// The type must have a single field, with the name provided as an argument
#define ECS_BLITTABLE_COPYABLE_IMPLEMENT(field_name) \
	size_t CopySizeImpl() const override { \
		/* No buffer data */ \
		return 0; \
	} \
\
	void CopyImpl(const void* other, AllocatorPolymorphic allocator) override { \
		/* Blit the blittable data contained by this type */ \
		memcpy(&field_name, &((decltype(this))other)->field_name, sizeof(field_name)); \
	} \
\
	void Deallocate(AllocatorPolymorphic allocator) override {}

	// A helper structure that satisfies the Copyable interface and can be used with the copyable APIs
	template<typename BlittableType>
	struct BlittableCopyable : public Copyable {
		ECS_INLINE BlittableCopyable() : Copyable(sizeof(BlittableType)) {}
		ECS_INLINE BlittableCopyable(const BlittableType& _data) : Copyable(sizeof(BlittableType)), data(_data) {}

		size_t CopySizeImpl() const override {
			// No buffer data
			return 0;
		}

		void CopyImpl(const void* other, AllocatorPolymorphic allocator) override {
			// Blit the blittable data contained by this type
			memcpy(&data, &((BlittableCopyable<BlittableType>*)other)->data, sizeof(data));
		}

		void Deallocate(AllocatorPolymorphic allocator) override {}

		BlittableType data;
	};

	// Only linear/stack/multipool/arena allocators can be created
	// Using this. This is intentional
	struct CreateBaseAllocatorInfo {
		ECS_INLINE CreateBaseAllocatorInfo() {}

		ECS_ALLOCATOR_TYPE allocator_type;

		union {
			// Linear allocator options
			struct {
				size_t linear_capacity;
			};
			// Stack allocator options
			struct {
				size_t stack_capacity;
			};
			// Multipool allocator options
			struct {
				size_t multipool_block_count;
				size_t multipool_capacity;
			};
			// Arena allocator options
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

#define ECS_EXPAND_ALLOCATOR_MACRO(macro) \
	macro(LinearAllocator) \
	macro(StackAllocator) \
	macro(MultipoolAllocator) \
	macro(MemoryManager) \
	macro(MemoryArena) \
	macro(ResizableLinearAllocator) \
	macro(MemoryProtectedAllocator) \
	macro(MallocAllocator)

#define ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(return_type, function_name, ...) template ECSENGINE_API return_type function_name(LinearAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(StackAllocator*, __VA_ARGS__); \
/*template ECSENGINE_API return_type function_name(PoolAllocator*, __VA_ARGS__);*/ \
template ECSENGINE_API return_type function_name(MultipoolAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MemoryManager*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MemoryArena*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(ResizableLinearAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MemoryProtectedAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MallocAllocator*, __VA_ARGS__);

#define ECS_TEMPLATE_FUNCTION_ALLOCATOR(return_type, function_name, ...) template return_type function_name(LinearAllocator*, __VA_ARGS__); \
template return_type function_name(StackAllocator*, __VA_ARGS__); \
/*template return_type function_name(PoolAllocator*, __VA_ARGS__);*/ \
template return_type function_name(MultipoolAllocator*, __VA_ARGS__); \
template return_type function_name(MemoryManager*, __VA_ARGS__); \
template return_type function_name(MemoryArena*, __VA_ARGS__); \
template return_type function_name(ResizableLinearAllocator*, __VA_ARGS__); \
template return_type function_name(MemoryProtectedAllocator*, __VA_ARGS__); \
template return_type function_name(MallocAllocator*, __VA_ARGS__);

}