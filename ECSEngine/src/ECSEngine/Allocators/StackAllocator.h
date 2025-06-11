#pragma once
#include "../Core.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/DebugInfo.h"
#include "AllocatorBase.h"

namespace ECSEngine {

	struct ECSENGINE_API StackAllocator final : public AllocatorBase
	{
		ECS_INLINE StackAllocator(void* buffer, size_t capacity) : AllocatorBase(ECS_ALLOCATOR_STACK), m_buffer((unsigned char*)buffer), m_capacity(capacity), 
			m_top(0), m_marker(0), m_last_top(0) {}

		// Override the operator such that the vtable is always copied
		ECS_INLINE StackAllocator& operator = (const StackAllocator& other) {
			memcpy(this, &other, sizeof(*this));
			return *this;
		}

		// The Ts functions can use the default

		virtual void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override;

		virtual bool DeallocateNoAssert(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO) override;

		// Deallocates the last entry
		void Deallocate(DebugInfo debug_info = ECS_DEBUG_INFO);

		ECS_INLINE virtual void* Reallocate(const void* buffer, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override {
			// Can't deallocate previous buffers, try to allocate a new one
			return Allocate(new_size, alignment, debug_info);
		}
		
		virtual void Clear(DebugInfo debug_info = ECS_DEBUG_INFO) override;

		ECS_INLINE virtual void Free(bool assert_that_is_standalone = false, DebugInfo debug_info = ECS_DEBUG_INFO) override {
			ECS_ASSERT(!assert_that_is_standalone, "StackAllocator is not standalone!");
		}

		ECS_INLINE virtual void FreeFrom(AllocatorBase* backup_allocator, bool multithreaded_deallocation, DebugInfo debug_info = ECS_DEBUG_INFO) override {
			if (multithreaded_deallocation) {
				backup_allocator->DeallocateTs(GetAllocatedBuffer(), debug_info);
			}
			else {
				backup_allocator->Deallocate(GetAllocatedBuffer(), debug_info);
			}
		}

		virtual bool Belongs(const void* pointer) const override;

		ECS_INLINE virtual void* GetAllocatedBuffer() const override {
			return m_buffer;
		}

		void SetMarker();

		void SetMarkerTs();

		void ReturnToMarker(DebugInfo debug_info = ECS_DEBUG_INFO);
		
		void ReturnToMarkerTs(DebugInfo debug_info = ECS_DEBUG_INFO);

		ECS_INLINE size_t GetTop() const {
			return m_top;
		}

		ECS_INLINE size_t GetCapacity() const {
			return m_capacity;
		}

		ECS_INLINE virtual bool IsEmpty() const override {
			return m_top == 0;
		}

		ECS_INLINE virtual size_t GetCurrentUsage() const override {
			return m_top;
		}

		// Region start and region size are parallel arrays. Returns the count of regions
		// Pointer capacity must represent the count of valid entries for the given pointers
		virtual size_t GetRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const override;

		unsigned char* m_buffer;
		size_t m_capacity;
		size_t m_top;
		size_t m_marker;
		size_t m_last_top;
	};
}

