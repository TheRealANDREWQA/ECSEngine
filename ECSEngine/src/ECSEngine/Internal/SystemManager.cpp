#include "ecspch.h"
#include "SystemManager.h"
#include "../Utilities/FunctionInterfaces.h"
#include "../Utilities/Crash.h"
#include "../Allocators/MemoryManager.h"

#define MEMORY_MANAGER_SIZE 10'000
#define MEMORY_MANAGER_CHUNK_COUNT 512
#define MEMORY_MANAGER_NEW_ALLOCATION_SIZE 10'000

#define SYSTEM_DATA_COUNT 128
#define TEMPORARY_TABLE_COUNT 128
#define SYSTEM_SETTINGS_COUNT 128

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------------------

	SystemManager::SystemManager(GlobalMemoryManager* global_memory)
	{
		MemoryManager* memory_manager = (MemoryManager*)global_memory->Allocate(sizeof(MemoryManager));
		*memory_manager = MemoryManager(MEMORY_MANAGER_SIZE, MEMORY_MANAGER_CHUNK_COUNT, MEMORY_MANAGER_NEW_ALLOCATION_SIZE, global_memory);

		system_data.Initialize(memory_manager, SYSTEM_DATA_COUNT);
		temporary_table.Initialize(memory_manager, TEMPORARY_TABLE_COUNT);
		system_settings.Initialize(memory_manager, SYSTEM_SETTINGS_COUNT);

		allocator = memory_manager;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::BindSystemData(Stream<char> system_name, const void* data, size_t data_size)
	{
		unsigned int index = system_data.Find(system_name);

		void* final_data = function::CopyNonZero(allocator, data, data_size);
		if (index == -1) {
			//InsertIntoDynamicTable(system_data, allocator, final_data, system_name);
		}
		else {
			void** ptr = system_data.GetValuePtrFromIndex(index);
			*ptr = final_data;
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::BindTemporaryTable(Stream<char> table_name, const void* data, size_t data_size)
	{
		unsigned int index = temporary_table.Find(table_name);

		void* final_data = function::CopyNonZero(allocator, data, data_size);
		if (index == -1) {
			InsertIntoDynamicTable(temporary_table, allocator, final_data, table_name);
		}
		else {
			void** ptr = temporary_table.GetValuePtrFromIndex(index);
			*ptr = final_data;
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::BindSystemSettings(Stream<char> system_name, Stream<void*> data, Stream<char>* names, size_t* byte_sizes)
	{
		unsigned int setting_index = system_settings.Find(system_name);
		
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(void*, mutable_data, data.size);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<char>, mutable_names, data.size);

		// Allocate the pointers - do a single coallesced allocation
		size_t total_size = 0;

		for (size_t index = 0; index < data.size; index++) {
			if (byte_sizes != nullptr) {
				total_size += byte_sizes[index];
				// Add another 8 bytes for the alignment of the data
				total_size += 8;
			}
			total_size += names[index].size;
		}

		void* allocation = allocator->Allocate(total_size);
		uintptr_t ptr = (uintptr_t)allocation;

		for (size_t index = 0; index < data.size; index++) {
			// Firstly the data
			if (byte_sizes != nullptr) {
				ptr = function::AlignPointer(ptr, 8);
				if (byte_sizes[index] > 0) {
					void* buffer = (void*)ptr;
					ptr += byte_sizes[index];
					memcpy(buffer, data[index], byte_sizes[index]);
					mutable_data[index] = buffer;
				}
				else {
					mutable_data[index] = data[index];
				}
			}
			else {
				mutable_data[index] = data[index];
			}

			// Now the name
			char* new_name = (char*)ptr;
			names[index].CopyTo(ptr);
			mutable_names[index] = { new_name, names[index].size };
		}

		data.buffer = mutable_data.buffer;
		names = mutable_names.buffer;

		ECS_ASSERT(ptr - (uintptr_t)allocation <= total_size);

		SystemSettings setting = { data, names };
		if (setting_index == -1) {
			InsertIntoDynamicTable(system_settings, allocator, setting, system_name);
		}
		else {
			SystemSettings* settings_ptr = system_settings.GetValuePtrFromIndex(setting_index);
			*settings_ptr = setting;
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::GetSystemData(Stream<char> system_name) const
	{
		void* data = nullptr;
		if (system_data.TryGetValue(system_name, data)) {
			return data;
		}
		
		ECS_CRASH_RETURN_VALUE(false, nullptr, "System Manager error: there is no system data for {#}.", system_name);
		return nullptr;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::GetSystemSettings(Stream<char> system_name, Stream<char> setting_name) const
	{
		SystemSettings settings;
		if (system_settings.TryGetValue(system_name, settings)) {
			for (size_t index = 0; index < settings.data.size; index++) {
				if (function::CompareStrings(settings.names[index], setting_name)) {
					return settings.data[index];
				}
			}

			ECS_CRASH_RETURN_VALUE(false, nullptr, "System manager error: failed to find setting {#} for system {#}.", setting_name, system_name);
		}

		ECS_CRASH_RETURN_VALUE(false, nullptr, "System Manager error: there is are no settings for system {#}.", system_name);
		return nullptr;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::GetTemporaryTable(Stream<char> table_name) const
	{
		void* data = nullptr;
		if (temporary_table.TryGetValue(table_name, data)) {
			return data;
		}

		ECS_CRASH_RETURN_VALUE(false, nullptr, "System Manager error: there is no temporary table with name {#}.", table_name);
		return nullptr;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::ResetTemporaryTable()
	{
		temporary_table.Clear();
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

}