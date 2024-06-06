#include "ecspch.h"
#include "SystemManager.h"
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

		temporary_allocator = LinearAllocator::InitializeFrom(global_memory, TEMPORARY_ALLOCATOR_SIZE);

		data_table.Initialize(memory_manager, DATA_COUNT);
		temporary_table.Initialize(memory_manager, TEMPORARY_TABLE_COUNT);
		system_settings.Initialize(memory_manager, SYSTEM_SETTINGS_COUNT);

		allocator = memory_manager;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	// Returns the pointer stored in the hash table
	// unsigned int FindTable(ResourceIdentifier identifier)
	// void InsertIntoTable(void* final_pointer, ResourceIdentifier identifier)
	// void* GetPointerFromIndex(unsigned int index)
	// void ChangePointerFromIndex(unsigned int index, void* data)
	template<typename FindTable, typename InsertIntoTable, typename GetPointerFromIndex, typename ChangePointerFromIndex>
	void* BindBasic(
		Stream<char> identifier, 
		const void* data, 
		size_t data_size, 
		AllocatorPolymorphic allocator,
		FindTable&& find_table,
		InsertIntoTable&& insert_table, 
		GetPointerFromIndex&& get_from_index,
		ChangePointerFromIndex&& change_pointer_from_index
	) {
		unsigned int index = find_table(identifier);

		void* final_pointer = (void*)data;
		if (index == -1) {
			// Add padding for the alignment of the final pointer
			size_t total_size_to_allocate = identifier.size + data_size + (data_size > 0 ? alignof(void*) : 0);
			void* allocation = Allocate(allocator, total_size_to_allocate);
			uintptr_t allocation_ptr = (uintptr_t)allocation;
			identifier.CopyTo(allocation_ptr);
			identifier.buffer = (char*)allocation;

			if (data_size > 0) {
				allocation_ptr = AlignPointer(allocation_ptr, alignof(void*));
				memcpy((void*)allocation_ptr, data, data_size);
				final_pointer = (void*)allocation_ptr;
			}

			insert_table(final_pointer, identifier);		
		}
		else {
			if (data_size > 0) {
				memcpy(get_from_index(index), data, data_size);
			}
			else {
				change_pointer_from_index(index, (void*)data);
			}

			final_pointer = get_from_index(index);
		}

		return final_pointer;
	}

	void* SystemManager::BindData(Stream<char> identifier, const void* data, size_t data_size)
	{
		return BindBasic(
			identifier, 
			data, 
			data_size, 
			Allocator(),
			[&](ResourceIdentifier identifier) {
				return data_table.Find(identifier);
			},
			[&](void* final_pointer, ResourceIdentifier identifier) {
				DataPointer data_pointer;
				data_pointer.SetData(data_size);
				data_pointer.SetPointer(final_pointer);
				data_table.InsertDynamic(allocator, data_pointer, identifier);
			},
			[&](unsigned int index) {
				return data_table.GetValueFromIndex(index).GetPointer();
			},
			[&](unsigned int index, void* data) {
				DataPointer* data_pointer = data_table.GetValuePtrFromIndex(index);
				data_pointer->SetPointer(data);
			}
		);
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::BindDataNoCopy(Stream<char> identifier, size_t data_size)
	{
		unsigned int table_index = data_table.Find(identifier);
		void* data = nullptr;
		if (table_index == -1) {
			// Allocate the space and insert into the table
			void* allocation = allocator->Allocate(data_size + identifier.size);
			uintptr_t ptr = (uintptr_t)allocation;
			identifier.InitializeAndCopy(ptr, identifier);
			data = (void*)ptr;
			data_table.InsertDynamic(allocator, DataPointer(data, data_size), identifier);
		}
		else {
			data = data_table.GetValueFromIndex(table_index).GetPointer();
		}
		return data;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::BindTemporaryData(Stream<char> identifier, const void* data, size_t data_size)
	{
		return BindBasic(
			identifier, 
			data, 
			data_size, 
			TemporaryAllocator(),
			[&](ResourceIdentifier identifier) {
				return temporary_table.Find(identifier);
			},
			[&](void* final_pointer, ResourceIdentifier identifier) {
				temporary_table.InsertDynamic(allocator, final_pointer, identifier);
			},
			[&](unsigned int index) {
				return temporary_table.GetValueFromIndex(index);
			},
			[&](unsigned int index, void* data) {
				void** pointer = temporary_table.GetValuePtrFromIndex(index);
				*pointer = data;
			}
		);
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::BindSystemSettings(Stream<char> system_name, Stream<SystemManagerSetting> settings)
	{
		unsigned int setting_index = system_settings.Find(system_name);
		
		// Allocate the pointers - do a single coalesced allocation
		size_t total_size = 0;

		for (size_t index = 0; index < settings.size; index++) {
			total_size += settings[index].byte_size;
			total_size += settings[index].name.size;
			// Add alignment bytes for the data
			total_size += alignof(void*);
		}
		total_size += settings.MemoryOf(settings.size);
		total_size += system_name.MemoryOf(system_name.size);

		Stream<SystemManagerSetting> new_settings;
		void* allocation = allocator->Allocate(total_size);
		uintptr_t ptr = (uintptr_t)allocation;
		new_settings.InitializeFromBuffer(ptr, settings.size);

		for (size_t index = 0; index < settings.size; index++) {
			new_settings[index].byte_size = settings[index].byte_size;

			// Firstly the data
			if (settings[index].byte_size > 0) {
				ptr = AlignPointer(ptr, alignof(void*));
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
			// Copy the system name to the final ptr location - such that we have a stable name
			system_name.InitializeAndCopy(ptr, system_name);
			ECS_ASSERT(ptr - (uintptr_t)allocation <= total_size);
			system_settings.InsertDynamic(allocator, new_settings, system_name);
		}
		else {
			Stream<SystemManagerSetting>* settings_ptr = system_settings.GetValuePtrFromIndex(setting_index);
			// Coalesced allocation starting on the buffer - safe to deallocate
			allocator->Deallocate(settings_ptr->buffer);
			*settings_ptr = new_settings;
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::Clear()
	{
		temporary_allocator.Clear();
		allocator->Clear();

		// We need to reset these since we cleared the allocator
		temporary_table.Reset();
		data_table.Reset();
		system_settings.Reset();
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::ClearFrame()
	{
		temporary_allocator.Clear();
		temporary_table.Clear();
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::ClearSystemSettings()
	{
		system_settings.ForEachConst([&](Stream<SystemManagerSetting> settings, ResourceIdentifier identifier) {
			allocator->Deallocate(settings.buffer);
		});
		system_settings.Clear();
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::FreeMemory()
	{
		// This is also allocated
		Deallocate(allocator->m_backup, temporary_allocator.GetAllocatedBuffer());
		Deallocate(allocator->m_backup, allocator);
		allocator->Free();
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::GetData(Stream<char> identifier) const
	{
		DataPointer data_pointer = nullptr;
		if (data_table.TryGetValue(identifier, data_pointer)) {
			return data_pointer.GetPointer();
		}
		
		ECS_CRASH_CONDITION_RETURN(false, nullptr, "System Manager error: there is no data associated to {#}.", identifier);
		return nullptr;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::GetSystemSettings(Stream<char> system_name, Stream<char> setting_name) const
	{
		Stream<SystemManagerSetting> settings;
		if (system_settings.TryGetValue(system_name, settings)) {
			for (size_t index = 0; index < settings.size; index++) {
				if (settings[index].name == setting_name) {
					return settings[index].data;
				}
			}

			ECS_CRASH_CONDITION_RETURN(false, nullptr, "System manager error: failed to find setting {#} for system {#}.", setting_name, system_name);
		}

		ECS_CRASH_CONDITION_RETURN(false, nullptr, "System Manager error: there are no settings for system {#}.", system_name);
		return nullptr;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::GetTemporaryData(Stream<char> identifier) const
	{
		void* data = nullptr;
		if (temporary_table.TryGetValue(identifier, data)) {
			return data;
		}

		ECS_CRASH_CONDITION_RETURN(false, nullptr, "System Manager error: there is no temporary data associated to {#}.", identifier);
		return nullptr;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::TryGetData(Stream<char> identifier) const
	{
		DataPointer data_pointer = nullptr;
		data_table.TryGetValue(identifier, data_pointer);
		return data_pointer.GetPointer();
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void* SystemManager::TryGetSystemSettings(Stream<char> system_name, Stream<char> settings_name) const
	{
		void* data = nullptr;

		Stream<SystemManagerSetting> settings = { nullptr, 0 };
		if (system_settings.TryGetValue(system_name, settings)) {
			for (size_t index = 0; index < settings.size; index++) {
				if (settings[index].name == settings_name) {
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

	void SystemManager::RemapData(void* old_pointer, void* new_pointer) {
		data_table.ForEach<true>([=](DataPointer& data_pointer, ResourceIdentifier identifier) {
			if (data_pointer.GetPointer() == old_pointer) {
				data_pointer.SetPointer(new_pointer);
				return true;
			}
			return false;
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::RemoveData(Stream<char> identifier)
	{
		unsigned int index = data_table.Find(identifier);
		if (index == -1) {
			ECS_CRASH_CONDITION(false, "System Manager error: there is no data associated to {#} when trying to remove.", identifier);
		}

		// Normally the identifier is coalesced with the data if it has a byte size to be copied
		ResourceIdentifier allocated_identifier = data_table.GetIdentifierFromIndex(index);
		allocator->Deallocate(allocated_identifier.ptr);

		DataPointer ptr = data_table.GetValueFromIndex(index);
		// It may have been allocated by the user
		unsigned short data_size = ptr.GetData();
		if (data_size == 0) {
			allocator->DeallocateIfBelongs(ptr.GetPointer());
		}
		data_table.EraseFromIndex(index);
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	void SystemManager::RemoveSystemSetting(Stream<char> system_name)
	{
		unsigned int index = system_settings.Find(system_name);
		if (index == -1) {
			ECS_CRASH_CONDITION(false, "System Manager error: there are no system settings for {#} when trying to remove.", system_name);
		}
		
		// Deallocate the buffer as well
		Stream<SystemManagerSetting> settings = system_settings.GetValueFromIndex(index);
		allocator->Deallocate(settings.buffer);
		system_settings.EraseFromIndex(index);
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

}