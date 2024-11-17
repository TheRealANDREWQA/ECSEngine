#include "ecspch.h"
#include "Scene.h"
#include "../ECS/EntityManager.h"
#include "../ECS/EntityManagerSerialize.h"
#include "AssetDatabase.h"
#include "AssetDatabaseReference.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "../Utilities/Serialization/SerializationHelpers.h"
#include "../Utilities/Path.h"

#define FILE_VERSION 0

namespace ECSEngine {

	enum CHUNK_TYPE : unsigned char {
		ASSET_DATABASE_CHUNK,
		ENTITY_MANAGER_CHUNK,
		TIME_CHUNK,
		MODULES_CHUNK,
		CHUNK_COUNT
	};

	struct SceneFileHeader {
		unsigned int header_size;
		unsigned char version;
		unsigned char reserved[3];
		size_t chunk_offsets[CHUNK_COUNT + ECS_SCENE_EXTRA_CHUNKS];
	};

	// ------------------------------------------------------------------------------------------------------------

	bool CreateEmptyScene(Stream<wchar_t> file)
	{
		return CreateEmptyFile(file);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool LoadScene(LoadSceneData* load_data)
	{
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB * 8);
		AllocatorPolymorphic stack_allocator = &_stack_allocator;

		bool normal_database = load_data->database != nullptr;
		AssetDatabase* database = normal_database ? load_data->database : load_data->database_reference->database;

		auto set_empty_scene = [&]() {
			// Just reset the entity manager and database reference
			load_data->entity_manager->Reset();
			if (normal_database) {
				load_data->database->DeallocateStreams();
			}
			else {
				load_data->database_reference->Reset();
			}
		};

		struct DeallocateMalloc {
			void operator() () {
				if (file_handle != -1) {
					CloseFile(file_handle);
				}
				if (allocation != nullptr) {
					Free(allocation);
				}
			}

			ECS_FILE_HANDLE file_handle;
			void* allocation;
		};
		StackScope<DeallocateMalloc> deallocate_malloc({ -1, nullptr });

		Stream<void> scene_data = {};
		if (load_data->is_file_data) {
			ECS_FILE_HANDLE file_handle = -1;
			ECS_FILE_STATUS_FLAGS status = OpenFile(load_data->file, &file_handle, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL | ECS_FILE_ACCESS_BINARY);
			if (status != ECS_FILE_STATUS_OK) {
				if (load_data->detailed_error_string) {
					load_data->detailed_error_string->AddStreamAssert("Failed to open the scene file");
				}
				return false;
			}

			// If the file is empty, then the entity manager and the database reference is empty as well
			size_t file_byte_size = GetFileByteSize(file_handle);
			if (file_byte_size == 0) {
				CloseFile(file_handle);
				set_empty_scene();
				return true;
			}

			void* file_allocation = Malloc(file_byte_size);
			deallocate_malloc.deallocator.file_handle = file_handle;
			deallocate_malloc.deallocator.allocation = file_allocation;

			bool success = ReadFileExact(file_handle, { file_allocation, file_byte_size });
			if (!success) {
				if (load_data->detailed_error_string) {
					load_data->detailed_error_string->AddStreamAssert("Failed to read the scene file");
				}
				return false;
			}

			scene_data = { file_allocation, file_byte_size };
		}
		else {
			scene_data = load_data->in_memory_data;
			if (scene_data.size == 0) {
				set_empty_scene();
				return true;
			}
		}

		uintptr_t data_start_ptr = (uintptr_t)scene_data.buffer;
		SceneFileHeader* file_header = (SceneFileHeader*)scene_data.buffer;
		if (file_header->version != FILE_VERSION) {
			if (load_data->detailed_error_string) {
				load_data->detailed_error_string->AddStreamAssert("Invalid scene file header");
			}
			return false;
		}

		uintptr_t ptr = data_start_ptr + file_header->header_size;
		if (file_header->chunk_offsets[ASSET_DATABASE_CHUNK] != ptr - data_start_ptr) {
			if (load_data->detailed_error_string) {
				load_data->detailed_error_string->AddStreamAssert("The scene file is corrupted (asset database chunk)");
			}
			return false;
		}

		bool randomize_assets = load_data->randomize_assets;
		AssetDatabaseSnapshot asset_database_snapshot = database->GetSnapshot(&_stack_allocator);

		bool success = true;
		// Try to load the asset database first
		if (load_data->database != nullptr) {
			success = DeserializeAssetDatabase(load_data->database, ptr) == ECS_DESERIALIZE_OK;
		}
		else {
			AssetDatabaseReferenceFromStandaloneOptions options = { load_data->handle_remapping, load_data->pointer_remapping };
			success = load_data->database_reference->DeserializeStandalone(load_data->reflection_manager, ptr, options);
		}

		if (!success) {
			if (load_data->detailed_error_string) {
				load_data->detailed_error_string->AddStreamAssert("Failed to deserialize the asset database of the scene");
			}
			return false;
		}

		// Check to see if we need to make the added assets unique
		if (randomize_assets) {
			database->RandomizePointers(asset_database_snapshot);
		}

		if (file_header->chunk_offsets[ENTITY_MANAGER_CHUNK] != ptr - data_start_ptr) {
			// Restore the snapshot
			database->RestoreSnapshot(asset_database_snapshot);
			if (load_data->detailed_error_string) {
				load_data->detailed_error_string->AddStreamAssert("The scene file is corrupted (entity manager chunk)");
			}
			return false;
		}


		// Create the deserialize tables firstly
		DeserializeEntityManagerComponentTable component_table;
		CreateDeserializeEntityManagerComponentTableAddOverrides(
			component_table, 
			load_data->reflection_manager, 
			stack_allocator, 
			load_data->module_component_functions, 
			load_data->unique_overrides
		);

		DeserializeEntityManagerSharedComponentTable shared_component_table;
		CreateDeserializeEntityManagerSharedComponentTableAddOverrides(
			shared_component_table, 
			load_data->reflection_manager, 
			stack_allocator, 
			load_data->module_component_functions, 
			load_data->shared_overrides
		);

		DeserializeEntityManagerGlobalComponentTable global_component_table;
		CreateDeserializeEntityManagerGlobalComponentTableAddOverrides(
			global_component_table, 
			load_data->reflection_manager, 
			stack_allocator, 
			load_data->global_overrides
		);

		DeserializeEntityManagerOptions deserialize_options;
		deserialize_options.component_table = &component_table;
		deserialize_options.shared_component_table = &shared_component_table;
		deserialize_options.global_component_table = &global_component_table;
		deserialize_options.detailed_error_string = load_data->detailed_error_string;
		deserialize_options.remove_missing_components = load_data->allow_missing_components;
		ECS_DESERIALIZE_ENTITY_MANAGER_STATUS deserialize_entity_manager_status = DeserializeEntityManager(
			load_data->entity_manager,
			ptr,
			&deserialize_options
		);	
		if (deserialize_entity_manager_status != ECS_DESERIALIZE_ENTITY_MANAGER_OK) {
			// Reset the database
			database->RestoreSnapshot(asset_database_snapshot);
			return false;
		}

		if (file_header->chunk_offsets[TIME_CHUNK] != ptr - data_start_ptr) {
			// Restore the snapshot
			database->RestoreSnapshot(asset_database_snapshot);
			if (load_data->detailed_error_string) {
				load_data->detailed_error_string->AddStreamAssert("The scene file is corrupted (time chunk)");
			}
			return false;
		}

		float2 world_time_values;
		Read<true>(&ptr, &world_time_values, sizeof(world_time_values));
			
		if (load_data->delta_time != nullptr) {
			*load_data->delta_time = world_time_values.x;
		}
		if (load_data->speed_up_factor != nullptr) {
			*load_data->speed_up_factor = world_time_values.y;
		}

		// The modules chunk is following
		if (file_header->chunk_offsets[MODULES_CHUNK] != ptr - data_start_ptr) {
			// Restore the snapshot
			database->RestoreSnapshot(asset_database_snapshot);
			if (load_data->detailed_error_string) {
				load_data->detailed_error_string->AddStreamAssert("The scene file is corrupted (modules chunk)");
			}
			return false;
		}

		// Before retrieving the modules themselves, get the source code branch and commit hash.
		void* source_code_branch_name = nullptr;
		unsigned short source_code_branch_name_size = 0;
		ReferenceDataWithSizeShort<true>(&ptr, &source_code_branch_name, source_code_branch_name_size);

		void* source_code_commit_hash = nullptr;
		unsigned short source_code_commit_hash_size = 0;
		ReferenceDataWithSizeShort<true>(&ptr, &source_code_commit_hash, source_code_commit_hash_size);

		Reflection::ReflectionType scene_modules_type;
		ECS_ASSERT(load_data->reflection_manager->TryGetType(STRING(SceneModule), scene_modules_type), "Loading scene requires the ReflectionManager to have the type SceneModules reflected!");
		if (load_data->scene_modules.IsInitialized()) {
			ECS_ASSERT(load_data->scene_modules_allocator.allocator != nullptr, "Loading a scene with module retrieval but no allocator is specified.");

			DeserializeOptions options;
			options.backup_allocator = load_data->scene_modules_allocator;
			options.field_allocator = load_data->scene_modules_allocator;
			options.default_initialize_missing_fields = true;

			SceneModules scene_modules;
			ECS_DESERIALIZE_CODE deserialize_status = Deserialize(load_data->reflection_manager, &scene_modules_type, &scene_modules, ptr, &options);
			if (deserialize_status != ECS_DESERIALIZE_OK) {
				if (load_data->detailed_error_string) {
					load_data->detailed_error_string->AddStreamAssert("The scene file is corrupted - could not deserialize the modules (module chunk)");
				}
				return false;
			}

			load_data->scene_modules.AddStream(scene_modules.values);
			// Write the source code info. Don't forget to allocate from the scene modules allocator
			load_data->source_code_branch_name = { source_code_branch_name, source_code_branch_name_size };
			load_data->source_code_branch_name = load_data->source_code_branch_name.Copy(load_data->scene_modules_allocator);
			load_data->source_code_commit_hash = { source_code_commit_hash, source_code_commit_hash_size };
			load_data->source_code_commit_hash = load_data->source_code_commit_hash.Copy(load_data->scene_modules_allocator);
		}
		else {
			IgnoreDeserialize(ptr);
		}

		// If there are any chunk functors specified, we should call them
		for (size_t index = 0; index < ECS_SCENE_EXTRA_CHUNKS; index++) {
			if (file_header->chunk_offsets[CHUNK_COUNT + index] != ptr - data_start_ptr) {
				// Restore the snapshot
				database->RestoreSnapshot(asset_database_snapshot);
				if (load_data->detailed_error_string) {
					ECS_FORMAT_TEMP_STRING(message, "The scene file is corrupted (extra chunk {#} offset invalid)", index);
					load_data->detailed_error_string->AddStreamAssert(message);
				}
				return false;
			}

			size_t chunk_size = 0;
			if (index < ECS_SCENE_EXTRA_CHUNKS - 1) {
				chunk_size = file_header->chunk_offsets[CHUNK_COUNT + index + 1] - file_header->chunk_offsets[CHUNK_COUNT + index];
			}
			else {
				size_t current_difference = ptr - data_start_ptr;
				chunk_size = scene_data.size - current_difference;
			}

			if (load_data->chunk_functors[index].function != nullptr) {
				LoadSceneChunkFunctionData functor_data;
				functor_data.entity_manager = load_data->entity_manager;
				functor_data.reflection_manager = load_data->reflection_manager;
				functor_data.file_version = file_header->version;
				functor_data.user_data = load_data->chunk_functors[index].user_data;
				functor_data.chunk_index = index;
				functor_data.chunk_data = { (void*)ptr, chunk_size };
				
				if (!load_data->chunk_functors[index].function(&functor_data)) {
					// Restore the snapshot
					database->RestoreSnapshot(asset_database_snapshot);
					if (load_data->detailed_error_string) {
						ECS_FORMAT_TEMP_STRING(message, "The scene file is corrupted (extra chunk {#} functor returned)", index);
						load_data->detailed_error_string->AddStreamAssert(message);
					}
					return false;
				}
			}

			ptr += chunk_size;
		}
		
		ECS_ASSERT((ptr - data_start_ptr) == scene_data.size);	
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool SaveScene(SaveSceneData* save_data)
	{
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 256, ECS_MB * 8);
		AllocatorPolymorphic stack_allocator = &_stack_allocator;

		// Rename the file to a temporary name such that if we fail to serialize we don't lose the previous data
		// Do this only if the file exists at that location
		ECS_STACK_CAPACITY_STREAM(wchar_t, renamed_file, 512);
		if (ExistsFileOrFolder(save_data->file)) {
			renamed_file.CopyOther(save_data->file);
			renamed_file.AddStream(L".temp");
			if (!RenameFileAbsolute(save_data->file, renamed_file)) {
				// If we fail, then return now.
				return false;
			}
		}

		struct StackDeallocator {
			void operator()() {
				renamed_file.CopyOther(original_file);
				renamed_file.AddStream(L".temp");

				// Here we can skip checking if the renaming file exists or not,
				// the calls will simply fail and we don't care about that
				if (file_handle != -1) {
					CloseFile(file_handle);

					if (!destroy_temporary) {
						RemoveFile(original_file);

						// Try to rename the previous file to this name
						RenameFile(renamed_file, original_file);
					}
					else {
						RemoveFile(renamed_file);
					}
				}
				else {
					RenameFileAbsolute(renamed_file, original_file);
				}
			}

			Stream<wchar_t> original_file;
			Stream<wchar_t> renamed_file;
			ECS_FILE_HANDLE file_handle;
			bool destroy_temporary;
		};

		StackScope<StackDeallocator> stack_deallocator({ save_data->file, renamed_file, -1, false });

		// Start with writing the entity manager first
		// Open the file first
		ECS_FILE_HANDLE file_handle = -1;
		ECS_FILE_STATUS_FLAGS status = FileCreate(save_data->file, &file_handle, ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE);
		if (status != ECS_FILE_STATUS_OK) {
			return false;
		}
		stack_deallocator.deallocator.file_handle = file_handle;

		size_t file_write_size = 0;
		SceneFileHeader header;
		memset(&header, 0, sizeof(header));
		header.version = FILE_VERSION;
		header.header_size = sizeof(header);
		// Write the unitialized header first
		bool success = WriteFile(file_handle, &header);
		if (!success) {
			return false;
		}
		file_write_size += sizeof(header);

		header.chunk_offsets[ASSET_DATABASE_CHUNK] = file_write_size;
		AssetDatabase final_database;
		size_t database_serialize_size = SerializeAssetDatabaseSize(save_data->asset_database);

		void* asset_database_allocation = Malloc(database_serialize_size);
		uintptr_t ptr = (uintptr_t)asset_database_allocation;

		success = SerializeAssetDatabase(save_data->asset_database, ptr) == ECS_SERIALIZE_OK;
		if (!success) {
			Free(asset_database_allocation);
			return false;
		}

		success = WriteFile(file_handle, { asset_database_allocation, database_serialize_size });
		Free(asset_database_allocation);
		if (!success) {
			return false;
		}
		file_write_size += database_serialize_size;

		header.chunk_offsets[ENTITY_MANAGER_CHUNK] = file_write_size;
		// Create the deserialize tables firstly
		SerializeEntityManagerComponentTable component_table;
		CreateSerializeEntityManagerComponentTableAddOverrides(component_table, save_data->reflection_manager, stack_allocator, save_data->unique_overrides);

		SerializeEntityManagerSharedComponentTable shared_component_table;
		CreateSerializeEntityManagerSharedComponentTableAddOverrides(shared_component_table, save_data->reflection_manager, stack_allocator, save_data->shared_overrides);

		SerializeEntityManagerGlobalComponentTable global_component_table;
		CreateSerializeEntityManagerGlobalComponentTableAddOverrides(global_component_table, save_data->reflection_manager, stack_allocator, save_data->global_overrides);

		SerializeEntityManagerOptions serialize_options;
		serialize_options.component_table = &component_table;
		serialize_options.shared_component_table = &shared_component_table;
		serialize_options.global_component_table = &global_component_table;
		success = SerializeEntityManager(save_data->entity_manager, file_handle, &serialize_options);
		if (!success) {
			return false;
		}
		file_write_size = GetFileCursor(file_handle);
		header.chunk_offsets[TIME_CHUNK] = file_write_size;
		
		// Write the final float value
		float2 world_time_values = { save_data->delta_time, save_data->speed_up_factor };
		success = WriteFile(file_handle, &world_time_values);
		if (!success) {
			return false;
		}

		_stack_allocator.Clear();
		file_write_size += sizeof(world_time_values);
		header.chunk_offsets[MODULES_CHUNK] = file_write_size;

		// Retrieve the module serialize size firstly such that we can make a stack allocation for it and write it to the file then
		// Use the reflection manager to write the scene modules
		Reflection::ReflectionType scene_modules_type;
		ECS_ASSERT(save_data->reflection_manager->TryGetType(STRING(SceneModules), scene_modules_type), "Saving scene requires the SceneModules type to be reflected!");
		size_t serialize_modules_size = SerializeSize(save_data->reflection_manager, &scene_modules_type, &save_data->modules);
		if (serialize_modules_size == 0) {
			// An error has occured
			return false;
		}

		size_t modules_allocation_total_size = serialize_modules_size + save_data->source_code_branch_name.CopySize() + save_data->source_code_commit_hash.CopySize()
			+ sizeof(unsigned short) * 2;
		void* modules_allocation = _stack_allocator.Allocate(modules_allocation_total_size);
		uintptr_t modules_allocation_ptr = (uintptr_t)modules_allocation;

		// Write the source code branch and commit hash first
		WriteWithSizeShort<true>(&modules_allocation_ptr, save_data->source_code_branch_name);
		WriteWithSizeShort<true>(&modules_allocation_ptr, save_data->source_code_commit_hash);

		ECS_SERIALIZE_CODE serialize_modules_status = Serialize(save_data->reflection_manager, &scene_modules_type, &save_data->modules, modules_allocation_ptr);
		if (serialize_modules_status != ECS_SERIALIZE_OK) {
			// An error has occured
			return false;
		}

		success = WriteFile(file_handle, { modules_allocation, modules_allocation_total_size });
		if (!success) {
			return false;
		}

		// Now write all the chunk functors
		// Have a global memory manager to make allocations from it, if necessary
		// But instantiate it lazily such that we don't create it for nothing
		GlobalMemoryManager extra_chunk_allocator;
		bool is_extra_chunk_allocator_initialized = false;

		auto deallocate_extra_chunk_allocator = StackScope([&]() {
			if (is_extra_chunk_allocator_initialized) {
				extra_chunk_allocator.Free();
			}
		});

		for (size_t index = 0; index < ECS_SCENE_EXTRA_CHUNKS; index++) {
			size_t chunk_start_offset = GetFileCursor(file_handle);
			ECS_ASSERT(chunk_start_offset != -1);
			header.chunk_offsets[CHUNK_COUNT + index] = chunk_start_offset;
			if (save_data->chunk_functors[index].function != nullptr) {
				SaveSceneChunkFunctionData functor_data;
				functor_data.chunk_index = index;
				functor_data.entity_manager = save_data->entity_manager;
				functor_data.reflection_manager = save_data->reflection_manager;
				functor_data.user_data = save_data->chunk_functors[index].user_data;
				if (save_data->chunk_functors[index].file_handle_write) {
					functor_data.write_handle = file_handle;
					if (!save_data->chunk_functors[index].function(&functor_data)) {
						// We should fail as well if the function told us so
						return false;
					}
				}
				else {
					if (!is_extra_chunk_allocator_initialized) {
						is_extra_chunk_allocator_initialized = true;
						extra_chunk_allocator = CreateGlobalMemoryManager(ECS_GB, ECS_KB, ECS_GB * 10);
					}
					functor_data.write_data = {};
					if (!save_data->chunk_functors[index].function(&functor_data)) {
						// For some reason the function failed (here we were requesting the write size only), we should fail as well
						return false;
					}

					void* allocation = extra_chunk_allocator.Allocate(functor_data.write_data.size);
					functor_data.write_data.buffer = allocation;
					if (!save_data->chunk_functors[index].function(&functor_data)) {
						// We should fail as well if the function told us so
						return false;
					}
					
					// We should commit directly to the file handle
					if (!WriteFile(file_handle, functor_data.write_data)) {
						return false;
					}

					// We should also free the allocation now
					extra_chunk_allocator.Deallocate(allocation);
				}
			}
		}

		success = SetFileCursorBool(file_handle, 0, ECS_FILE_SEEK_BEG);
		if (!success) {
			return false;
		}

		// Write the header at the end
		success = WriteFile(file_handle, &header);
		if (!success) {
			return false;
		}

		stack_deallocator.deallocator.destroy_temporary = true;
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

}