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
#include "../Utilities/ReaderWriterInterface.h"
#include "../ECS/ComponentAssetHandling.h"

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
		bool normal_database = load_data->database != nullptr;
		AssetDatabase* database = normal_database ? load_data->database : load_data->database_reference->database;

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB * 128);
		AllocatorPolymorphic stack_allocator = &_stack_allocator;

		return load_data->read_target.Read(ECS_KB * 32, stack_allocator, load_data->detailed_error_string, [&](ReadInstrument* read_instrument) {
			// If the data source is empty, then we consider the scene as empty as well
			if (read_instrument->TotalSize() == 0) {
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

			SceneFileHeader file_header;
			if (!read_instrument->Read(&file_header)) {
				ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "Could not read scene file header");
				return false;
			}

			if (file_header.version != FILE_VERSION) {
				ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "Invalid scene file header");
				return false;
			}

			if (file_header.chunk_offsets[ASSET_DATABASE_CHUNK] != read_instrument->GetOffset()) {
				ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "The scene file is corrupted (asset database chunk)");
				return false;
			}

			bool randomize_assets = load_data->randomize_assets;
			AssetDatabaseAddSnapshot asset_database_snapshot = database->GetAddSnapshot(&_stack_allocator);

			bool success = true;
			// Try to load the asset database first
			if (load_data->database != nullptr) {
				success = DeserializeAssetDatabase(load_data->database, read_instrument) == ECS_DESERIALIZE_OK;
			}
			else {
				AssetDatabaseReferenceFromStandaloneOptions options = { load_data->handle_remapping, load_data->asset_remapping };
				success = load_data->database_reference->DeserializeStandalone(load_data->reflection_manager, read_instrument, &options);
			}

			if (!success) {
				ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "Failed to deserialize the asset database of the scene");
				return false;
			}

			// TODO: We need to adapt the randomize pointers - since the pointers now matter
			// Check to see if we need to make the added assets unique
			AssetDatabaseAssetRemap asset_remapping_value;
			if (randomize_assets) {
				AssetDatabaseAssetRemap* asset_remapping = nullptr;
				if (load_data->randomize_assets_update_entity_values) {
					asset_remapping_value.Initialize(stack_allocator);
					asset_remapping = &asset_remapping_value;
				}
				database->RandomizePointers(asset_database_snapshot, asset_remapping);
			}

			if (file_header.chunk_offsets[ENTITY_MANAGER_CHUNK] != read_instrument->GetOffset()) {
				// Restore the snapshot
				database->RestoreAddSnapshot(asset_database_snapshot);
				ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "The scene file is corrupted (entity manager chunk)");
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
				read_instrument,
				&deserialize_options
			);
			if (deserialize_entity_manager_status != ECS_DESERIALIZE_ENTITY_MANAGER_OK) {
				// Reset the database
				database->RestoreAddSnapshot(asset_database_snapshot);
				ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, ". Failed to deserialize the entity manager");
				return false;
			}

			// After loading the entity manager, perform the asset remapping, if requested
			if (load_data->randomize_assets_update_entity_values) {
				UpdateAssetRemappings(load_data->reflection_manager, load_data->entity_manager, asset_remapping_value);
			}

			if (file_header.chunk_offsets[TIME_CHUNK] != read_instrument->GetOffset()) {
				// Restore the snapshot
				database->RestoreAddSnapshot(asset_database_snapshot);
				ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "The scene file is corrupted (time chunk)");
				return false;
			}

			float2 world_time_values;
			if (!read_instrument->Read(&world_time_values)) {
				database->RestoreAddSnapshot(asset_database_snapshot);
				ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "Could not read world time values (delta time and speed up factor)");
				return false;
			}

			if (load_data->delta_time != nullptr) {
				*load_data->delta_time = world_time_values.x;
			}
			if (load_data->speed_up_factor != nullptr) {
				*load_data->speed_up_factor = world_time_values.y;
			}

			// The modules chunk is following
			if (file_header.chunk_offsets[MODULES_CHUNK] != read_instrument->GetOffset()) {
				// Restore the snapshot
				database->RestoreAddSnapshot(asset_database_snapshot);
				ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "The scene file is corrupted (modules chunk)");
				return false;
			}

			// Before retrieving the modules themselves, get the source code branch and commit hash.
			Stream<char> source_code_branch_name;
			if (!read_instrument->ReadOrReferenceDataWithSizeVariableLength(source_code_branch_name, stack_allocator)) {
				database->RestoreAddSnapshot(asset_database_snapshot);
				ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "Could not read the source code branch name");
				return false;
			}

			Stream<char> source_code_commit_hash;
			if (!read_instrument->ReadOrReferenceDataWithSizeVariableLength(source_code_commit_hash, stack_allocator)) {
				database->RestoreAddSnapshot(asset_database_snapshot);
				ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "Could not read the source code commit hash");
				return false;
			}

			const Reflection::ReflectionType* scene_modules_type = load_data->reflection_manager->TryGetType(STRING(ModulesSourceCode));
			ECS_ASSERT(scene_modules_type != nullptr, "Loading scene requires the ReflectionManager to have the type ModulesSourceCode reflected!");
			if (load_data->scene_modules.IsInitialized()) {
				ECS_ASSERT(load_data->scene_modules_allocator.allocator != nullptr, "Loading a scene with module retrieval but no allocator is specified.");

				DeserializeOptions options;
				options.field_allocator = load_data->scene_modules_allocator;
				options.default_initialize_missing_fields = true;

				ModulesSourceCode scene_modules;
				ECS_DESERIALIZE_CODE deserialize_status = Deserialize(load_data->reflection_manager, scene_modules_type, &scene_modules, read_instrument, &options);
				if (deserialize_status != ECS_DESERIALIZE_OK) {
					if (load_data->detailed_error_string) {
						load_data->detailed_error_string->AddStreamAssert("The scene file is corrupted - could not deserialize the modules (module chunk)");
					}
					return false;
				}

				load_data->scene_modules.AddStream(scene_modules.values);
				// Write the source code info. Don't forget to allocate from the scene modules allocator
				load_data->source_code_branch_name = source_code_branch_name.Copy(load_data->scene_modules_allocator);
				load_data->source_code_commit_hash = source_code_commit_hash.Copy(load_data->scene_modules_allocator);
			}
			else {
				IgnoreDeserialize(read_instrument);
			}

			// If there are any chunk functors specified, we should call them
			for (size_t index = 0; index < ECS_SCENE_EXTRA_CHUNKS; index++) {
				size_t current_instrument_offset = read_instrument->GetOffset();
				if (file_header.chunk_offsets[CHUNK_COUNT + index] != current_instrument_offset) {
					// Restore the snapshot
					database->RestoreAddSnapshot(asset_database_snapshot);
					ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "The scene file is corrupted (extra chunk {#} offset invalid)", index);
					return false;
				}

				size_t chunk_size = 0;
				if (index < ECS_SCENE_EXTRA_CHUNKS - 1) {
					chunk_size = file_header.chunk_offsets[CHUNK_COUNT + index + 1] - file_header.chunk_offsets[CHUNK_COUNT + index];
				}
				else {
					size_t current_difference = current_instrument_offset;
					chunk_size = read_instrument->TotalSize() - current_difference;
				}

				unsigned int version = 0;
				chunk_size -= sizeof(version);

				if (load_data->chunk_functors[index].function != nullptr) {
					// Before each chunk, there is a version written, such that we can version each individual chunk
					// Without versioning the main scene file
					if (!read_instrument->ReadAlways(&version)) {
						// Restore the snapshot
						database->RestoreAddSnapshot(asset_database_snapshot);
						ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "Could not read scene chunk version (extra chunk {#} functor returned)", index);
						return false;
					}

					// Create a subinstrument, such that the chunk can be thought of as an individual unit
					ReadInstrument::SubinstrumentData subinstrument_data;
					auto subinstrument_deallocator = read_instrument->PushSubinstrument(&subinstrument_data, chunk_size);

					LoadSceneChunkFunctionData functor_data;
					functor_data.entity_manager = load_data->entity_manager;
					functor_data.reflection_manager = load_data->reflection_manager;
					functor_data.file_version = version;
					functor_data.user_data = load_data->chunk_functors[index].user_data.buffer;
					functor_data.read_instrument = read_instrument;

					if (!load_data->chunk_functors[index].function(&functor_data)) {
						// Restore the snapshot
						database->RestoreAddSnapshot(asset_database_snapshot);
						ECS_FORMAT_ERROR_MESSAGE(load_data->detailed_error_string, "The scene file is corrupted (extra chunk {#} functor returned)", index);
						return false;
					}
				}
				else {
					read_instrument->Ignore(chunk_size + sizeof(version));
				}
			}

			ECS_ASSERT(read_instrument->GetOffset() == read_instrument->TotalSize());
			return true;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	bool SaveScene(SaveSceneData* save_data)
	{
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 256, ECS_MB * 8);
		AllocatorPolymorphic stack_allocator = &_stack_allocator;

		// A buffering of 64KB should generally be enough
		return save_data->write_target.Write(ECS_KB * 64, stack_allocator, nullptr, [&](WriteInstrument* write_instrument) {
			SceneFileHeader header;
			memset(&header, 0, sizeof(header));
			header.version = FILE_VERSION;
			header.header_size = sizeof(header);
			// Write the unitialized header first
			if (!write_instrument->Write(&header)) {
				return false;
			}
			header.chunk_offsets[ASSET_DATABASE_CHUNK] = sizeof(header);

			if (SerializeAssetDatabase(save_data->asset_database, write_instrument) != ECS_SERIALIZE_OK) {
				return false;
			}
			header.chunk_offsets[ENTITY_MANAGER_CHUNK] = write_instrument->GetOffset();

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
			if (!SerializeEntityManager(save_data->entity_manager, write_instrument, &serialize_options)) {
				return false;
			}
			header.chunk_offsets[TIME_CHUNK] = write_instrument->GetOffset();

			// Write the final float value
			float2 world_time_values = { save_data->delta_time, save_data->speed_up_factor };
			if (!write_instrument->Write(&world_time_values)) {
				return false;
			}
			header.chunk_offsets[MODULES_CHUNK] = write_instrument->GetOffset();

			// Retrieve the module serialize size firstly such that we can make a stack allocation for it and write it to the file then
			// Use the reflection manager to write the scene modules
			const Reflection::ReflectionType* scene_modules_type = save_data->reflection_manager->TryGetType(STRING(ModulesSourceCode));
			ECS_ASSERT(scene_modules_type, "Saving scene requires the ModulesSourceCode type to be reflected!");

			// Write the source code branch and commit hash first
			if (!write_instrument->WriteWithSizeVariableLength(save_data->source_code_branch_name)) {
				return false;
			}
			if (!write_instrument->WriteWithSizeVariableLength(save_data->source_code_commit_hash)) {
				return false;
			}
			if (Serialize(save_data->reflection_manager, scene_modules_type, &save_data->modules, write_instrument) != ECS_SERIALIZE_OK) {
				// An error has occured
				return false;
			}

			// Now write all the chunk functors
			for (size_t index = 0; index < ECS_SCENE_EXTRA_CHUNKS; index++) {
				size_t chunk_start_offset = write_instrument->GetOffset();
				ECS_ASSERT(chunk_start_offset != -1);
				header.chunk_offsets[CHUNK_COUNT + index] = chunk_start_offset;
				if (save_data->chunk_functors[index].function != nullptr) {
					// Write a prefix version
					if (!write_instrument->Write(&save_data->chunk_functors[index].version)) {
						return false;
					}

					SaveSceneChunkFunctionData functor_data;
					functor_data.entity_manager = save_data->entity_manager;
					functor_data.reflection_manager = save_data->reflection_manager;
					functor_data.user_data = save_data->chunk_functors[index].user_data.buffer;
					functor_data.write_instrument = write_instrument;
					if (!save_data->chunk_functors[index].function(&functor_data)) {
						// We should fail as well if the function told us so
						return false;
					}
				}
			}

			// Seek to the beginning, such that we write the proper header
			if (!write_instrument->Seek(ECS_INSTRUMENT_SEEK_START, 0) || !write_instrument->Write(&header)) {
				return false;
			}

			return true;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

}