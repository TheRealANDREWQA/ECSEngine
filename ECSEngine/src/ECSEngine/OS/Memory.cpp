#include "ecspch.h"
#include "Memory.h"
#include "../Utilities/Assert.h"

namespace ECSEngine {

	namespace OS {

		void* VirtualAllocation(size_t size)
		{
			// MEM_COMMIT | MEM_RESERVE means to reserve virtual space and indicate that we want these pages
			// to be mapped to physical memory (the actual physical memory will be reserved when the process access it)
			void* allocation = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			ECS_ASSERT(allocation != nullptr, "Failed to allocated virtual memory");
			return allocation;
		}

		void VirtualDeallocation(void* block)
		{
			ECS_ASSERT(VirtualFree(block, 0, MEM_RELEASE));
		}

		bool DisableVirtualWriteProtection(void* allocation)
		{
			MEMORY_BASIC_INFORMATION memory_info;
			size_t memory_info_byte_size = VirtualQuery(allocation, &memory_info, sizeof(memory_info));
			if (memory_info_byte_size == 0) {
				// The function failed
				return false;
			}

			DWORD previous_access;
			return VirtualProtect(allocation, memory_info.RegionSize, PAGE_READWRITE, &previous_access);
		}

		bool EnableVirtualWriteProtection(void* allocation)
		{
			MEMORY_BASIC_INFORMATION memory_info;
			size_t memory_info_byte_size = VirtualQuery(allocation, &memory_info, sizeof(memory_info));
			if (memory_info_byte_size == 0) {
				// The function failed
				return false;
			}

			DWORD previous_access;
			return VirtualProtect(allocation, memory_info.RegionSize, PAGE_READONLY, &previous_access);
		}

	}

}