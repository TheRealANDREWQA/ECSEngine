#include "ecspch.h"
#include "SystemManager.h"
#include "../Utilities/FunctionInterfaces.h"
#include "../Utilities/Crash.h"
#include "../Allocators/MemoryManager.h"

#define MEMORY_MANAGER_SIZE 10'000
#define MEMORY_MANAGER_CHUNK_COUNT 512
#define MEMORY_MANAGER_NEW_ALLOCATION_SIZE 10'000

#define TEMPORARY_ALLOCATOR_SIZE 5'000

#define DATA_COUNT 32
#define TEMPORARY_TABLE_COUNT 32
#define SYSTEM_SETTINGS_COUNT 32

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------------------

	SystemManager::SystemManager(GlobalMemoryManager* global_memory)
	{
		MemoryManager* memory_manager = (MemoryManager*)global_memory->Allocate(sizeof(MemoryManager));
		*memory_manager = MemoryManager(MEMORY_MANAGER_SIZE, MEMORY_MANAGER_CHUNK_COUNT, MEMORY_MANAGER_NEW_ALLOCATION_SIZE, global_memory);

		temporary_allocator = LinearAllocator(GetAllocatorPolymorphic(global_memory), TEMPORARY_ALLOCATOR_SIZE);

		data_table.Initialize(memory_manager, DATA_COUNT);
		temporary_table.Initialize(memory_manager, TEMPORARY_TABLE_COUNT);
		system_settings.Initialize(memory_manager, SYSTEM_SETTINGS_COUNT);

		allocator = memory_manager;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	// Returns the pointer stored in the hash table
	void* BindBasic(Stream<char> identifier, const void* data, size_t data_size, HashTableDefault<void*>* table, AllocatorPolymorphic allocator) {
		unsigned int index = table->Find(identifier);

		void* final_pointer = (void*)data;
		if (index == -1) {
			// Add padding for the alignment of the final pointer
			size_t total_size_to_allocate = identifier.size + data_size + data_size > 0 ? alignof(void*) : 0;
			void* allocation = Allocate(allocator, total_size_to_allocate);
			uintptr_t allocation_ptr = (uintptr_t)allocation;
			identifier.CopyTo(allocation_ptr);
			identifier.buffer = (char*)allocation;

			if (data_size > 0) {
				allocation_ptr = function::AlignPointer(allocation_ptr, alignof(void*));
				memcpy((void*)allocation_ptr, data, data_size);
				final_pointer = (void*)allocation_ptr;
			}

			InsertIntoDynamicTable(*table, allocator, final_pointer, identifier);
		}
		else {
			void** ptr = table->GetValuePtrFromIndex(index);
			if (data_size > 0) {
				memcpy(*ptr, data, data_size);
			}
			else {
				*ptr = (void*)data;
			}

			final_pointer = *ptr;
		}

		return final_pointer;
	}

	void* SystemManager::BindData(Stream<char> identifier, const void* data, size_t data_size)
	{
		return BindBasic(identifier, data, data_size, &data_table, GetAllocatorPolymorphic(allocator));
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::BindTemporaryData(Stream<char> identifier, const void* data, size_t data_size)
	{
		return BindBasic(identifier, data, data_size, &temporary_table, GetAllocatorPolymorphic(&temporary_allocator));
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::BindSystemSettings(Stream<char> system_name, Stream<SystemManagerSetting> settings)
	{
		unsigned int setting_index = system_settings.Find(system_name);
		
		// Allocate the pointers - do a single coallesced allocation
		size_t total_size = 0;

		for (size_t index = 0; index < settings.size; index++) {
			total_size += settings[index].byte_size;
			total_size += settings[index].name.size;
			// Add alignment bytes for the data
			total_size += alignof(void*);
		}
		total_size += settings.MemoryOf(settings.size);

		Stream<SystemManagerSetting> new_settings;
		void* allocation = allocator->Allocate(total_size);
		uintptr_t ptr = (uintptr_t)allocation;
		new_settings.InitializeFromBuffer(ptr, settings.size);

		for (size_t index = 0; index < settings.size; index++) {
			new_settings[index].byte_size = settings[index].byte_size;

			// Firstly the data
			if (settings[index].byte_size > 0) {
				ptr = function::AlignPointer(ptr, alignof(void*));
				void* buffer = (void*)ptr;
				ptr += settings[index].byte_size;
				memcpy(buffer, settings[index].data, settings[index].byte_size);
				new_settings[index].data = buffer;
			}
			else {
				new_settings[index].data = settings[index].data;
			}

			// Now the name
			new_settings[index].name.InitializeAndCopy(ptr, settings[index].name);
		}

		ECS_ASSERT(ptr - (uintptr_t)allocation <= total_size);

		if (setting_index == -1) {
			InsertIntoDynamicTable(system_settings, allocator, new_settings, system_name);
		}
		else {
			Stream<SystemManagerSetting>* settings_ptr = system_settings.GetValuePtrFromIndex(setting_index);
			// Coallesced allocation starting on the buffer - safe to deallocate
			allocator->Deallocate(settings_ptr->buffer);
			*settings_ptr = new_settings;
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::Clear()
	{
		temporary_allocator.Clear();
		allocator->Clear();

		temporary_table.Clear();
		data_table.Clear();
		system_settings.Clear();
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::ClearFrame()
	{
		temporary_allocator.Clear();
		temporary_table.Clear();
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::FreeMemory()
	{
		// This is also allocated
		allocator->m_backup->Deallocate(allocator);
		allocator->m_backup->Deallocate(temporary_allocator.GetAllocatedBuffer());
		allocator->Free();
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::GetData(Stream<char> identifier) const
	{
		void* data = nullptr;
		if (data_table.TryGetValue(identifier, data)) {
			return data;
		}
		
		ECS_CRASH_RETURN_VALUE(false, nullptr, "System Manager error: there is no data associated to {#}.", identifier);
		return nullptr;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::GetSystemSettings(Stream<char> system_name, Stream<char> setting_name) const
	{
		Stream<SystemManagerSetting> settings;
		if (system_settings.TryGetValue(system_name, settings)) {
			for (size_t index = 0; index < settings.size; index++) {
				if (function::CompareStrings(settings[index].name, setting_name)) {
					return settings[index].data;
				}
			}

			ECS_CRASH_RETURN_VALUE(false, nullptr, "System manager error: failed to find setting {#} for system {#}.", setting_name, system_name);
		}

		ECS_CRASH_RETURN_VALUE(false, nullptr, "System Manager error: there are no settings for system {#}.", system_name);
		return nullptr;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::GetTemporaryData(Stream<char> identifier) const
	{
		void* data = nullptr;
		if (temporary_table.TryGetValue(identifier, data)) {
			return data;
		}

		ECS_CRASH_RETURN_VALUE(false, nullptr, "System Manager error: there is no temporary data associated to {#}.", identifier);
		return nullptr;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::TryGetData(Stream<char> identifier) const
	{
		void* data = nullptr;
		data_table.TryGetValue(identifier, data);
		return data;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::TryGetSystemSettings(Stream<char> system_name, Stream<char> settings_name) const
	{
		void* data = nullptr;

		Stream<SystemManagerSetting> settings = { nullptr, 0 };
		if (system_settings.TryGetValue(system_name, settings)) {
			for (size_t index = 0; index < settings.size; index++) {
				if (function::CompareStrings(settings[index].name, settings_name)) {
					data = settings[index].data;
					break;
				}
			}
		}

		return data;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::TryGetTemporaryData(Stream<char> identifier) const
	{
		void* data = nullptr;
		temporary_table.TryGetValue(identifier, data);
		return data;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::RemoveData(Stream<char> identifier)
	{
		unsigned int index = data_table.Find(identifier);
		if (index == -1) {
			ECS_CRASH_RETURN(false, "System Manager error: there is no data associated to {#} when trying to remove.", identifier);
		}

		// Deallocate the identifier as well
		ResourceIdentifier allocated_identifier = data_table.GetIdentifierFromIndex(index);
		allocator->Deallocate(allocated_identifier.ptr);
		data_table.EraseFromIndex(index);
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::RemoveSystemSetting(Stream<char> system_name)
	{
		unsigned int index = system_settings.Find(system_name);
		if (index == -1) {
			ECS_CRASH_RETURN(false, "System Manager error: there are no system settings for {#} when trying to remove.", system_name);
		}
		
		// Deallocate the buffer as well
		Stream<SystemManagerSetting> settings = system_settings.GetValueFromIndex(index);
		allocator->Deallocate(settings.buffer);
		system_settings.EraseFromIndex(index);
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

}