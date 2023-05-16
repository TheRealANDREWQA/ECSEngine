#include "ecspch.h"
#include "Scene.h"
#include "../ECS/EntityManager.h"
#include "../ECS/EntityManagerSerialize.h"
#include "AssetDatabase.h"
#include "AssetDatabaseReference.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "../Utilities/Path.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	bool CreateEmptyScene(Stream<wchar_t> file)
	{
		return CreateEmptyFile(file);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool LoadScene(LoadSceneData* load_data)
	{
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB * 8);
		AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

		ECS_FILE_HANDLE file_handle = -1;
		ECS_FILE_STATUS_FLAGS status = OpenFile(load_data->file, &file_handle, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_BINARY);
		if (status != ECS_FILE_STATUS_OK) {
			return false;
		}

		bool normal_database = load_data->database != nullptr;
		AssetDatabase* database = normal_database ? load_data->database : load_data->database_reference->database;

		// If the file is empty, then the entity manager and the database reference is empty as well
		size_t file_byte_size = GetFileByteSize(file_handle);
		if (file_byte_size == 0) {
			CloseFile(file_handle);

			// Just reset the entity manager and database reference
			load_data->entity_manager->Reset();
			if (normal_database) {
				load_data->database->DeallocateStreams();
			}
			else {
				load_data->database_reference->Reset();
			}
			return true;
		}

		void* file_allocation = malloc(file_byte_size);

		struct DeallocateMalloc {
			void operator() () {
				CloseFile(file_handle);
				free(allocation);
				allocator->ClearBackup();
			}

			ECS_FILE_HANDLE file_handle;
			void* allocation;
			ResizableLinearAllocator* allocator;
		};

		StackScope<DeallocateMalloc> deallocate_malloc({ file_handle, file_allocation, &_stack_allocator });

		bool success = ReadFile(file_handle, { file_allocation, file_byte_size });
		if (!success) {
			return false;
		}

		uintptr_t ptr = (uintptr_t)file_allocation;

		bool randomize_assets = load_data->randomize_assets;
		AssetDatabaseSnapshot asset_database_snapshot = database->GetSnapshot();

		// Try to load the asset database first
		if (load_data->database != nullptr) {
			success = DeserializeAssetDatabase(load_data->database, ptr) == ECS_DESERIALIZE_OK;
		}
		else {
			AssetDatabaseReferenceFromStandaloneOptions options = { load_data->handle_remapping, load_data->pointer_remapping };
			success = load_data->database_reference->DeserializeStandalone(load_data->reflection_manager, ptr, options);
		}

		if (!success) {
			return false;
		}

		// Check to see if we need to make the added assets unique
		if (randomize_assets) {
			database->RandomizePointers(asset_database_snapshot);
		}

		// Create the deserialize tables firstly
		DeserializeEntityManagerComponentTable component_table;
		CreateDeserializeEntityManagerComponentTable(component_table, load_data->reflection_manager, stack_allocator, load_data->unique_overrides);
		if (load_data->unique_overrides.size > 0) {
			AddDeserializeEntityManagerComponentTableOverrides(component_table, load_data->reflection_manager, load_data->unique_overrides);
		}

		DeserializeEntityManagerSharedComponentTable shared_component_table;
		CreateDeserializeEntityManagerSharedComponentTable(shared_component_table, load_data->reflection_manager, stack_allocator, load_data->shared_overrides);
		if (load_data->shared_overrides.size > 0) {
			AddDeserializeEntityManagerSharedComponentTableOverrides(shared_component_table, load_data->reflection_manager, load_data->shared_overrides);
		}

		ECS_DESERIALIZE_ENTITY_MANAGER_STATUS deserialize_status = DeserializeEntityManager(
			load_data->entity_manager,
			ptr,
			&component_table,
			&shared_component_table
		);
		
		if (deserialize_status != ECS_DESERIALIZE_ENTITY_MANAGER_OK) {
			// Reset the database
			database->RestoreSnapshot(asset_database_snapshot);
			return false;
		}
		
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool SaveScene(SaveSceneData* save_data)
	{
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 256, ECS_MB * 8);
		AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

		// Rename the file to a temporary name such that if we fail to serialize we don't lose the previous data
		ECS_STACK_CAPACITY_STREAM(wchar_t, renamed_file, 512);
		renamed_file.Copy(function::PathFilename(save_data->file));
		renamed_file.AddStream(L".temp");
		if (!RenameFolderOrFile(save_data->file, renamed_file)) {
			// If we fail, then return now.
			return false;
		}

		struct StackDeallocator {
			void operator()() {
				linear_allocator->ClearBackup();

				renamed_file.Copy(original_file);
				renamed_file.AddStream(L".temp");

				Stream<wchar_t> original_file_filename = function::PathFilename(original_file);

				if (file_handle != -1) {
					CloseFile(file_handle);

					if (!destroy_temporary) {
						RemoveFile(original_file);

						// Try to rename the previous file to this name
						RenameFolderOrFile(renamed_file, original_file_filename);
					}
					else {
						RemoveFile(renamed_file);
					}
				}
				else {
					RenameFolderOrFile(renamed_file, original_file_filename);
				}
			}

			Stream<wchar_t> original_file;
			Stream<wchar_t> renamed_file;
			ECS_FILE_HANDLE file_handle;
			bool destroy_temporary;
			ResizableLinearAllocator* linear_allocator;
		};

		StackScope<StackDeallocator> stack_deallocator({ save_data->file, renamed_file, -1, false, &_stack_allocator });

		// Start with writing the entity manager first
		// Open the file first
		ECS_FILE_HANDLE file_handle = -1;
		ECS_FILE_STATUS_FLAGS status = FileCreate(save_data->file, &file_handle, ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE);
		if (status != ECS_FILE_STATUS_OK) {
			return false;
		}
		stack_deallocator.deallocator.file_handle = file_handle;

		AssetDatabase final_database;

		size_t database_serialize_size = SerializeAssetDatabaseSize(save_data->asset_database);

		void* asset_database_allocation = malloc(database_serialize_size);
		uintptr_t ptr = (uintptr_t)asset_database_allocation;

		bool success = SerializeAssetDatabase(save_data->asset_database, ptr) == ECS_SERIALIZE_OK;

		if (!success) {
			free(asset_database_allocation);
			return false;
		}

		success = WriteFile(file_handle, { asset_database_allocation, database_serialize_size });
		free(asset_database_allocation);
		if (!success) {
			return false;
		}

		// Create the deserialize tables firstly
		SerializeEntityManagerComponentTable component_table;
		CreateSerializeEntityManagerComponentTable(component_table, save_data->reflection_manager, stack_allocator, save_data->unique_overrides);
		if (save_data->unique_overrides.size > 0) {
			AddSerializeEntityManagerComponentTableOverrides(component_table, save_data->reflection_manager, save_data->unique_overrides);
		}

		SerializeEntityManagerSharedComponentTable shared_component_table;
		CreateSerializeEntityManagerSharedComponentTable(shared_component_table, save_data->reflection_manager, stack_allocator, save_data->shared_overrides);
		if (save_data->shared_overrides.size > 0) {
			AddSerializeEntityManagerSharedComponentTableOverrides(shared_component_table, save_data->reflection_manager, save_data->shared_overrides);
		}

		success = SerializeEntityManager(save_data->entity_manager, file_handle, &component_table, &shared_component_table);
		if (!success) {
			return false;
		}

		stack_deallocator.deallocator.destroy_temporary = true;
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

}