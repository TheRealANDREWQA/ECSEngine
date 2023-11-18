#include "ecspch.h"
#include "PhysicalMemory.h"
#include "../Utilities/PointerUtilities.h"
#include "../Utilities/Utilities.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Allocators/MemoryManager.h"

#include <psapi.h>

namespace ECSEngine {

	namespace OS {
		
		static size_t PAGE_SIZE = 0;

		void InitializePhysicalMemoryPageSize()
		{
			SYSTEM_INFO system_info;
			GetSystemInfo(&system_info);
			PAGE_SIZE = system_info.dwPageSize;
		}

		size_t GetPhysicalMemoryPageSize()
		{
			return PAGE_SIZE;
		}

		size_t ProcessPhysicalMemoryUsage()
		{
			PROCESS_MEMORY_COUNTERS counters;
			BOOL success = GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
			return success ? counters.WorkingSetSize : -1;
		}

		void EmptyPhysicalMemory()
		{
			EmptyWorkingSet(GetCurrentProcess());
		}

		bool EmptyPhysicalMemory(void* allocation, size_t allocation_size)
		{
			bool success = VirtualUnlock(allocation, allocation_size);
			size_t last_error = GetLastError();
			return  success || last_error == ERROR_NOT_LOCKED;
		}

		size_t GetPhysicalMemoryBytesForAllocation(void* allocation, size_t allocation_size)
		{
			size_t page_count = SlotsFor(allocation_size, PAGE_SIZE);
			// Use a large heap size just in case this function wants to check for a large allocation
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 100);
			size_t set_allocation_size = sizeof(PSAPI_WORKING_SET_EX_INFORMATION) * page_count;
			PSAPI_WORKING_SET_EX_INFORMATION* set_information = (PSAPI_WORKING_SET_EX_INFORMATION*)stack_allocator.Allocate(set_allocation_size);
			for (size_t index = 0; index < page_count; index++) {
				set_information[index].VirtualAddress = allocation;
				allocation = OffsetPointer(allocation, PAGE_SIZE);
			}

			BOOL success = QueryWorkingSetEx(GetCurrentProcess(), set_information, set_allocation_size);
			if (success) {
				size_t physical_pages = 0;
				for (size_t index = 0; index < page_count; index++) {
					if (set_information[index].VirtualAttributes.Valid) {
						physical_pages++;
					}
				}
				return physical_pages * PAGE_SIZE;
			}
			return -1;
		}

		bool GuardPages(void* allocation, size_t allocation_size)
		{
			DWORD old_protect = 0;
			return VirtualProtect(allocation, allocation_size, PAGE_READWRITE | PAGE_GUARD, &old_protect);
		}

	}

}