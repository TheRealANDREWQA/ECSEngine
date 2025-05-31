#include "ecspch.h"
#include "SceneDelta.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Utilities/Serialization/DeltaStateSerialization.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "../ECS/EntityManager.h"
#include "../ECS/EntityManagerSerialize.h"
#include "../ECS/World.h"
#include "../Resources/AssetDatabase.h"
#include "../Utilities/Serialization/SerializeIntVariableLength.h"
#include "../Utilities/ReaderWriterInterface.h"
#include "../Tools/Modules/ModuleDefinition.h"

// The current format version
#define VERSION 0

#define CHANGE_SET_ALLOCATOR_CAPACITY ECS_MB * 50
#define CHANGE_SET_ALLOCATOR_BACKUP_CAPACITY ECS_MB * 100
#define ASSET_DATABASE_SNAPSHOT_ALLOCATOR_CAPACITY ECS_MB
#define ENTIRE_SCENE_BUFFERING_CAPACITY ECS_MB * 64
#define DESERIALIZE_REFLECTION_ALLOCATOR_CAPACITY ECS_MB * 4
#define DESERIALIZE_ENTIRE_SCENE_ALLOCATOR_CAPACITY ECS_MB * 50
#define DESERIALIZE_ENTIRE_SCENE_ALLOCATOR_BACKUP_CAPACITY ECS_MB * 100
#define INITIALIZE_OPTIONS_ALLOCATOR_CAPACITY ECS_MB * 2

namespace ECSEngine {

	using namespace Reflection;

	// -----------------------------------------------------------------------------------------------------------------------------

	// EntityManagerType should be EntityManager or const EntityManager
	template<typename EntityManagerType>
	struct WriterReaderBaseData {
		// Use a separate allocator for the change set, that is linear in nature since it is temporary,
		// Such that we can wink it in order to deallocate the change set
		ResizableLinearAllocator change_set_allocator;
		EntityManagerType* current_entity_manager;

		// We need to record this reflection manager such that we can access reflection structures
		const ReflectionManager* reflection_manager;
	};

	struct WriterData : public WriterReaderBaseData<const EntityManager> {
		// Returns the reference to new active allocator 
		ECS_INLINE ResizableLinearAllocator& AdvanceAssetSnapshotAllocatorIndex() {
			previous_asset_database_snapshot_allocator_active_index = previous_asset_database_snapshot_allocator_active_index == 0 ? 1 : 0;
			return previous_asset_database_snapshot_allocators[previous_asset_database_snapshot_allocator_active_index];
		}

		// Maintain a personal allocator for the previous state, such that we don't force the caller to do
		// This, which can be quite a hassle. From this allocator the buffering for the entire scene serialization
		// Will also be made
		GlobalMemoryManager previous_state_allocator;
		EntityManager previous_entity_manager;

		const AssetDatabase* current_asset_database;

		SerializeEntityManagerOptions serialize_options;
		// These 2 floats can be nullptr, in case they are not specified
		const float* delta_time_value;
		const float* speed_up_time_value;
		// Record the snapshot of the previous asset database state
		// In order to determine the delta change
		AssetDatabaseFullSnapshot previous_asset_database_snapshot;
		// There are 2 allocators, such that we can have one allocator for the previous database
		// Snapshot and another one from which the current asset database snapshot and delta change
		// Are calculated. This allows for maximum efficiency, since we can directly allocate an
		// Maintain the current snapshot, which will become the previous snapshot by the end of the frame
		ResizableLinearAllocator previous_asset_database_snapshot_allocators[2];
		// This indicates from which allocator the previous asset database snapshot is allocated from
		unsigned int previous_asset_database_snapshot_allocator_active_index = 0;

		// Store the buffering for the entire scene call here - we can initialize it during the init call
		CapacityStream<void> entire_scene_entity_manager_buffering;

		SerializeEntityManagerHeaderSectionOutput entity_manager_header_section_output;

		ResizableLinearAllocator initialize_options_allocator;
		// These are the options it has been initialized with. The memory of the streams inside it
		// Is all allocated from initialize_options_allocator
		SceneDeltaWriterInitializeInfoOptions initialize_options;

		// Don't cache the reflection types of the AssetDatabaseFullSnapshot and AssetDatabaseDeltaChange,
		// Even tho they are needed for each call, because if the reflection manager is updated during the
		// Replay (which can happen during the editor), the references might become invalidated and it will
		// Result in hard to track bugs. For this reason, always look them up in that specific call
	};

	struct ReaderData : public WriterReaderBaseData<EntityManager> {
		// This is an allocator that is used exclusively for the reflection manager and the field tables
		ResizableLinearAllocator deserialized_reflection_manager_allocator;

		// This reflection manager will store the reflection types used for serialization
		// In the file. It will not contain components, but rather the structures that are deserialized
		// Outside the main scene runtime.
		ReflectionManager deserialized_reflection_manager;

		// In this field, the EntityManagerChangeSet deserialized table will be recorded such that in can be forwarded to
		// The deserialize calls directly
		DeserializeFieldTable entity_manager_change_set_field_table;
		// In this field, the AssetDatabaseFullSnapshot deserialized table will be recorded such that in can be forwarded to
		// The deserialize calls directly
		DeserializeFieldTable asset_database_full_snapshot_field_table;
		// In this field, the AssetDatabaseDeltaChange deserialized table will be recorded such that in can be forwarded to
		// The deserialize calls directly
		DeserializeFieldTable asset_database_delta_change_field_table;

		// This is the deserialized header section of the file
		DeserializeEntityManagerHeaderSectionData entity_manager_header_section;

		// These are the cached options
		DeserializeEntityManagerOptions deserialize_entity_manager_options;

		// This is an allocator that is used only for the entire scene deserialization
		ResizableLinearAllocator deserialize_entire_scene_allocator;

		ResizableLinearAllocator initialize_options_allocator;
		SceneDeltaReaderInitializeInfoOptions initialize_options;

		struct SerializedFunctor {
			unsigned int version;
			// This index indexes into initialize_options.**_functor
			unsigned int mapped_chunk;
		};

		// The size of this array indicates how many entire states functors were serialized
		// These contain the functors that were serialized in the serialization order
		Stream<SerializedFunctor> entire_state_serialized_functors;
		// The size of this array indicates how many delta states functors were serialized
		// These contain the functors that were serialized in the serialization order
		Stream<SerializedFunctor> delta_state_serialized_functors;
	};

	// This allows the writer to work in a self-contained manner
	struct WriterWorldData : WriterData {
		// Needed for the elapsed seconds
		const World* world;
	};

	static void WriterInitialize(void* user_data, AllocatorPolymorphic allocator) {
		WriterData* delta_state = (WriterData*)user_data;

		// Create the global allocator with the info taken from the actual memory manager that is bound, such that we can handle
		// Everything that the passed manager can. We need to augment the initial info such that it can handle the additional initial
		// Memory that the entity manager needs.
		CreateBaseAllocatorInfo global_allocator_info = delta_state->current_entity_manager->m_memory_manager->GetInitialAllocatorInfo();
		// Ensure that the base allocator infos are of multipool type, such that we access the correct field
		ECS_ASSERT(global_allocator_info.allocator_type == ECS_ALLOCATOR_MULTIPOOL);
		ECS_ASSERT(delta_state->current_entity_manager->m_memory_manager->m_backup_info.allocator_type == ECS_ALLOCATOR_MULTIPOOL);
		size_t current_entity_manager_initial_size = global_allocator_info.multipool_capacity;
		// Use a conservative small value
		global_allocator_info.multipool_capacity += ECS_MB * 32;
		// Also, add the buffering capacity
		global_allocator_info.multipool_capacity += ENTIRE_SCENE_BUFFERING_CAPACITY;

		CreateGlobalMemoryManager(&delta_state->previous_state_allocator, global_allocator_info, delta_state->current_entity_manager->m_memory_manager->m_backup_info);
		EntityManagerDescriptor previous_state_descriptor;
		// We can use as memory manager the global allocator directly
		previous_state_descriptor.memory_manager = &delta_state->previous_state_allocator;
		// The deferred capacity can be 0 for this entity manager, since it won't be used to run actual deferred calls
		previous_state_descriptor.deferred_action_capacity = 0;
		CreateEntityManagerWithPool(
			&delta_state->previous_entity_manager,
			current_entity_manager_initial_size,
			global_allocator_info.multipool_block_count,
			delta_state->current_entity_manager->m_memory_manager->m_backup_info.multipool_capacity,
			delta_state->current_entity_manager->m_entity_pool->m_pool_power_of_two,
			&delta_state->previous_state_allocator
		);

		// Use malloc for now for the change set allocator
		delta_state->change_set_allocator = ResizableLinearAllocator(CHANGE_SET_ALLOCATOR_CAPACITY, CHANGE_SET_ALLOCATOR_BACKUP_CAPACITY, ECS_MALLOC_ALLOCATOR);

		// Copy the current contents
		delta_state->previous_entity_manager.CopyOther(delta_state->current_entity_manager);

		for (size_t index = 0; index < ECS_COUNTOF(delta_state->previous_asset_database_snapshot_allocators); index++) {
			delta_state->previous_asset_database_snapshot_allocators[index] = ResizableLinearAllocator(ASSET_DATABASE_SNAPSHOT_ALLOCATOR_CAPACITY, ASSET_DATABASE_SNAPSHOT_ALLOCATOR_CAPACITY, ECS_MALLOC_ALLOCATOR);
		}
		delta_state->previous_asset_database_snapshot_allocator_active_index = 0;
		// Retrieve the initial database snapshot
		delta_state->previous_asset_database_snapshot = delta_state->current_asset_database->GetFullSnapshot(delta_state->previous_asset_database_snapshot_allocators + 0);
		delta_state->entire_scene_entity_manager_buffering.Initialize(&delta_state->previous_state_allocator, ENTIRE_SCENE_BUFFERING_CAPACITY);

		// Allocate the initialize options from the stated allocator
		delta_state->initialize_options_allocator = ResizableLinearAllocator(INITIALIZE_OPTIONS_ALLOCATOR_CAPACITY, INITIALIZE_OPTIONS_ALLOCATOR_CAPACITY, ECS_MALLOC_ALLOCATOR);
		AllocatorPolymorphic initialize_options_allocator = &delta_state->initialize_options_allocator;
		delta_state->initialize_options.source_code_branch_name = delta_state->initialize_options.source_code_branch_name.Copy(initialize_options_allocator);
		delta_state->initialize_options.source_code_commit_hash = delta_state->initialize_options.source_code_commit_hash.Copy(initialize_options_allocator);
		// No need to use the coalesced variant, since this is a linear allocator, we don't care about many small allocations
		delta_state->initialize_options.modules = StreamDeepCopy(delta_state->initialize_options.modules, initialize_options_allocator);

		// Copy the overrides, even tho the memory could technically be stable from outside, we shouldn't use that as a default behavior.
		// Allocating them once more here won't hurt that much, and it keeps things decoupled. We are using coalesced deep copy because
		// That's the only function they implement
		delta_state->initialize_options.unique_overrides = StreamCoalescedDeepCopy(delta_state->initialize_options.unique_overrides, initialize_options_allocator);
		delta_state->initialize_options.shared_overrides = StreamCoalescedDeepCopy(delta_state->initialize_options.shared_overrides, initialize_options_allocator);
		delta_state->initialize_options.global_overrides = StreamCoalescedDeepCopy(delta_state->initialize_options.global_overrides, initialize_options_allocator);

		// Create the serialize tables and cache them for the entire duration of the writer
		SerializeEntityManagerComponentTable* component_table = Allocate<SerializeEntityManagerComponentTable>(initialize_options_allocator);
		CreateSerializeEntityManagerComponentTableAddOverrides(*component_table, delta_state->reflection_manager, initialize_options_allocator, delta_state->initialize_options.unique_overrides);
		delta_state->serialize_options.component_table = component_table;

		SerializeEntityManagerSharedComponentTable* shared_component_table = Allocate<SerializeEntityManagerSharedComponentTable>(initialize_options_allocator);
		CreateSerializeEntityManagerSharedComponentTableAddOverrides(*shared_component_table, delta_state->reflection_manager, initialize_options_allocator, delta_state->initialize_options.shared_overrides);
		delta_state->serialize_options.shared_component_table = shared_component_table;

		SerializeEntityManagerGlobalComponentTable* global_component_table = Allocate<SerializeEntityManagerGlobalComponentTable>(initialize_options_allocator);
		CreateSerializeEntityManagerGlobalComponentTableAddOverrides(*global_component_table, delta_state->reflection_manager, initialize_options_allocator, delta_state->initialize_options.global_overrides);
		delta_state->serialize_options.global_component_table = global_component_table;

		// Copy the general functors
		for (size_t index = 0; index < delta_state->initialize_options.save_header_functors.size; index++) {
			delta_state->initialize_options.save_header_functors[index].user_data = CopyNonZero(initialize_options_allocator, delta_state->initialize_options.save_header_functors[index].user_data);
		}
		for (size_t index = 0; index < delta_state->initialize_options.save_entire_functors.size; index++) {
			delta_state->initialize_options.save_entire_functors[index].user_data = CopyNonZero(initialize_options_allocator, delta_state->initialize_options.save_entire_functors[index].user_data);
		}
		for (size_t index = 0; index < delta_state->initialize_options.save_delta_functors.size; index++) {
			delta_state->initialize_options.save_delta_functors[index].user_data = CopyNonZero(initialize_options_allocator, delta_state->initialize_options.save_delta_functors[index].user_data);
		}
	}

	static void WriterDeallocate(void* user_data, AllocatorPolymorphic allocator) {
		WriterData* delta_state = (WriterData*)user_data;
		delta_state->previous_state_allocator.Free();
		delta_state->change_set_allocator.Free();
		for (size_t index = 0; index < ECS_COUNTOF(delta_state->previous_asset_database_snapshot_allocators); index++) {
			delta_state->previous_asset_database_snapshot_allocators[index].Free();
		}
		delta_state->initialize_options_allocator.Free();
	}

	static void ReaderInitialize(void* user_data, AllocatorPolymorphic allocator) {
		ReaderData* data = (ReaderData*)user_data;

		// Use malloc for now for the change set allocator
		data->change_set_allocator = ResizableLinearAllocator(CHANGE_SET_ALLOCATOR_CAPACITY, CHANGE_SET_ALLOCATOR_BACKUP_CAPACITY, ECS_MALLOC_ALLOCATOR);
		data->deserialized_reflection_manager_allocator = ResizableLinearAllocator(DESERIALIZE_REFLECTION_ALLOCATOR_CAPACITY, DESERIALIZE_REFLECTION_ALLOCATOR_CAPACITY, ECS_MALLOC_ALLOCATOR);
		data->deserialize_entire_scene_allocator = ResizableLinearAllocator(DESERIALIZE_ENTIRE_SCENE_ALLOCATOR_CAPACITY, DESERIALIZE_ENTIRE_SCENE_ALLOCATOR_BACKUP_CAPACITY, ECS_MALLOC_ALLOCATOR);
		data->initialize_options_allocator = ResizableLinearAllocator(INITIALIZE_OPTIONS_ALLOCATOR_CAPACITY, INITIALIZE_OPTIONS_ALLOCATOR_CAPACITY, ECS_MALLOC_ALLOCATOR);
		
		// Copy the initialize options now
		AllocatorPolymorphic initialize_options_allocator = &data->initialize_options_allocator;
		data->initialize_options.asset_load_callback_data = CopyNonZero(initialize_options_allocator, data->initialize_options.asset_load_callback_data);

		// Copy the overrides, even tho the memory could technically be stable from outside, we shouldn't use that as a default behavior.
		// Allocating them once more here won't hurt that much, and it keeps things decoupled. We are using coalesced deep copy because
		// That's the only function they implement
		data->initialize_options.unique_overrides = StreamCoalescedDeepCopy(data->initialize_options.unique_overrides, initialize_options_allocator);
		data->initialize_options.shared_overrides = StreamCoalescedDeepCopy(data->initialize_options.shared_overrides, initialize_options_allocator);
		data->initialize_options.global_overrides = StreamCoalescedDeepCopy(data->initialize_options.global_overrides, initialize_options_allocator);
		// Do the same for module component functions
		data->initialize_options.module_component_functions = StreamDeepCopy(data->initialize_options.module_component_functions, initialize_options_allocator);

		// Create the deserialize options tables
		DeserializeEntityManagerComponentTable* component_table = Allocate<DeserializeEntityManagerComponentTable>(initialize_options_allocator);
		CreateDeserializeEntityManagerComponentTableAddOverrides(
			*component_table,
			data->reflection_manager,
			initialize_options_allocator,
			data->initialize_options.module_component_functions,
			data->initialize_options.unique_overrides
		);
		data->deserialize_entity_manager_options.component_table = component_table;

		DeserializeEntityManagerSharedComponentTable* shared_component_table = Allocate<DeserializeEntityManagerSharedComponentTable>(initialize_options_allocator);
		CreateDeserializeEntityManagerSharedComponentTableAddOverrides(
			*shared_component_table,
			data->reflection_manager,
			initialize_options_allocator,
			data->initialize_options.module_component_functions,
			data->initialize_options.shared_overrides
		);
		data->deserialize_entity_manager_options.shared_component_table = shared_component_table;

		DeserializeEntityManagerGlobalComponentTable* global_component_table = Allocate<DeserializeEntityManagerGlobalComponentTable>(initialize_options_allocator);
		CreateDeserializeEntityManagerGlobalComponentTableAddOverrides(
			*global_component_table,
			data->reflection_manager,
			initialize_options_allocator,
			data->initialize_options.global_overrides
		);
		data->deserialize_entity_manager_options.global_component_table = global_component_table;
		// TODO: Determine if we want to remove the components that are missing or not. Probably, as a default, use a yes for the moment.
		data->deserialize_entity_manager_options.remove_missing_components = true;

		// Allocate the chunk functors
		for (size_t index = 0; index < data->initialize_options.read_header_functors.size; index++) {
			data->initialize_options.read_header_functors[index].user_data = CopyNonZero(initialize_options_allocator, data->initialize_options.read_header_functors[index].user_data);
		}
		for (size_t index = 0; index < data->initialize_options.read_entire_functors.size; index++) {
			data->initialize_options.read_entire_functors[index].user_data = CopyNonZero(initialize_options_allocator, data->initialize_options.read_entire_functors[index].user_data);
		}
		for (size_t index = 0; index < data->initialize_options.read_delta_functors.size; index++) {
			data->initialize_options.read_delta_functors[index].user_data = CopyNonZero(initialize_options_allocator, data->initialize_options.read_delta_functors[index].user_data);
		}
	}

	static void ReaderDeallocate(void* user_data, AllocatorPolymorphic allocator) {
		ReaderData* data = (ReaderData*)user_data;
		data->change_set_allocator.Free();
		data->deserialized_reflection_manager_allocator.Free();
		data->deserialize_entire_scene_allocator.Free();
		data->initialize_options_allocator.Free();
	}

	static float WriterExtractFunction(void* user_data) {
		WriterWorldData* data = (WriterWorldData*)user_data;
		return data->world->elapsed_seconds;
	}

	static bool WriterHeaderWriteFunction(void* user_data, WriteInstrument* write_instrument) {
		WriterData* data = (WriterData*)user_data;

		// Ensure that the EntityManagerChangeSet, AssetDatabaseDeltaChange and AssetDatabaseFullSnapshot exist among these types
		const ReflectionType* entity_manager_change_set_reflection_type = data->reflection_manager->TryGetType(STRING(EntityManagerChangeSet));
		ECS_ASSERT(entity_manager_change_set_reflection_type != nullptr, "In order to write scene deltas, the EntityManagerChangeSet type needs to be reflected");
		
		const ReflectionType* asset_database_delta_change_type = data->reflection_manager->TryGetType(STRING(AssetDatabaseDeltaChange));
		ECS_ASSERT(asset_database_delta_change_type != nullptr, "In order to write scene deltas, the AssetDatabaseDeltaChange type needs to be reflected");

		const ReflectionType* asset_database_full_snapshot_type = data->reflection_manager->TryGetType(STRING(AssetDatabaseFullSnapshot));
		ECS_ASSERT(asset_database_full_snapshot_type != nullptr, "In order to write scene deltas, the AssetDatabaseFullSnapshot type needs to be reflected");

		// Write the serialization field tables, such that we can recover it for this file type
		if (!SerializeFieldTable(data->reflection_manager, entity_manager_change_set_reflection_type, write_instrument)) {
			return false;
		}

		if (!SerializeFieldTable(data->reflection_manager, asset_database_delta_change_type, write_instrument)) {
			return false;
		}

		if (!SerializeFieldTable(data->reflection_manager, asset_database_full_snapshot_type, write_instrument)) {
			return false;
		}

		// We don't have to write the entire reflection manager to the file because the scene components reflection
		// Data will be written in the next call, this way we save that data only once, which is the most efficiently

		// We need to write an entity manager header section, this will contain all the reflection data needed for
		// Deserializing all entire and delta states.
		// Retrieve the unique components and the shared components. As allocator, use the previous state allocator,
		// Since it should have plenty of available space
		data->entity_manager_header_section_output.unique_components.has_value = true;
		data->entity_manager_header_section_output.shared_components.has_value = true;
		data->entity_manager_header_section_output.temporary_allocator = &data->previous_state_allocator;
		if (!SerializeEntityManagerHeaderSection(data->current_entity_manager, write_instrument, &data->serialize_options, nullptr, &data->entity_manager_header_section_output)) {
			return false;
		}

		// Write the header functors, if there are any. Write the count of entire state functors and
		// Delta state functors as well, such that we know exactly for each step how many there are,
		// Without having to check every time
		if (!SerializeIntVariableLengthBoolMultiple(write_instrument, data->initialize_options.save_header_functors.size, data->initialize_options.save_entire_functors.size, data->initialize_options.save_delta_functors.size)) {
			return false;
		}

		// Serialize the versions and the name of each functor, must be done only once in the beginning.
		// For the header functors, do that while in the loop where the data is actually serialized
		for (size_t index = 0; index < data->initialize_options.save_entire_functors.size; index++) {
			if (!SerializeIntVariableLengthBool(write_instrument, data->initialize_options.save_entire_functors[index].version)) {
				return false;
			}

			if (write_instrument->WriteWithSizeVariableLength(data->initialize_options.save_entire_functors[index].name)) {
				return false;
			}
		}

		for (size_t index = 0; index < data->initialize_options.save_delta_functors.size; index++) {
			if (!SerializeIntVariableLengthBool(write_instrument, data->initialize_options.save_delta_functors[index].version)) {
				return false;
			}

			if (write_instrument->WriteWithSizeVariableLength(data->initialize_options.save_delta_functors[index].name)) {
				return false;
			}
		}

		SaveSceneChunkFunctionData save_chunk_data;
		// We can use either the previous entity manager or this current one, it shouldn't matter
		save_chunk_data.entity_manager = data->current_entity_manager;
		save_chunk_data.reflection_manager = data->reflection_manager;
		save_chunk_data.write_instrument = write_instrument;
		size_t chunk_initial_offset = write_instrument->GetOffset();

		for (size_t index = 0; index < data->initialize_options.save_header_functors.size; index++) {
			if (!SerializeIntVariableLengthBool(write_instrument, data->initialize_options.save_header_functors[index].version)) {
				return false;
			}

			if (write_instrument->WriteWithSizeVariableLength(data->initialize_options.save_header_functors[index].name)) {
				return false;
			}

			// Prepend the size of the chunk before it, such that we know to jump it in the deserialize
			// In case the data is unwanted
			if (!write_instrument->AppendUninitialized(sizeof(size_t))) {
				return false;
			}

			save_chunk_data.user_data = data->initialize_options.save_header_functors[index].user_data.buffer;
			if (!data->initialize_options.save_header_functors[index].function(&save_chunk_data)) {
				return false;
			}

			size_t chunk_final_offset = write_instrument->GetOffset();
			size_t chunk_write_size = chunk_final_offset - chunk_initial_offset - sizeof(chunk_initial_offset);
			if (!write_instrument->WriteUninitializedData(chunk_initial_offset, &chunk_write_size, sizeof(chunk_write_size))) {
				return false;
			}

			chunk_initial_offset = chunk_final_offset;
		}

		return true;
	}

	static bool ReaderHeaderReadFunction(DeltaStateReaderHeaderReadFunctionData* functor_data) {
		// Do a quick pre-check, if the variable length header is missing, fail
		if (functor_data->write_size == 0) {
			return false;
		}

		ReaderData* data = (ReaderData*)functor_data->user_data;

		data->deserialized_reflection_manager_allocator.Clear();
		// Read the field tables for the necessary types
		data->entity_manager_change_set_field_table = DeserializeFieldTableFromData(functor_data->read_instrument, &data->deserialized_reflection_manager_allocator);
		if (data->entity_manager_change_set_field_table.IsFailed()) {
			return false;
		}

		data->asset_database_delta_change_field_table = DeserializeFieldTableFromData(functor_data->read_instrument, &data->deserialized_reflection_manager_allocator);
		if (data->asset_database_delta_change_field_table.IsFailed()) {
			return false;
		}

		data->asset_database_full_snapshot_field_table = DeserializeFieldTableFromData(functor_data->read_instrument, &data->deserialized_reflection_manager_allocator);
		if (data->asset_database_full_snapshot_field_table.IsFailed()) {
			return false;
		}

		// Ensure that the EntityManagerChangeSet, AssetDatabaseDeltaChange and AssetDatabaseFullSnapshot exist among these types
		const ReflectionType* entity_manager_change_set_reflection_type = data->reflection_manager->TryGetType(STRING(EntityManagerChangeSet));
		ECS_ASSERT(entity_manager_change_set_reflection_type != nullptr, "In order to read scene deltas, the EntityManagerChangeSet type needs to be reflected");

		const ReflectionType* asset_database_delta_change_type = data->reflection_manager->TryGetType(STRING(AssetDatabaseDeltaChange));
		ECS_ASSERT(asset_database_delta_change_type != nullptr, "In order to read scene deltas, the AssetDatabaseDeltaChange type needs to be reflected");

		const ReflectionType* asset_database_full_snapshot_type = data->reflection_manager->TryGetType(STRING(AssetDatabaseFullSnapshot));
		ECS_ASSERT(asset_database_full_snapshot_type != nullptr, "In order to read scene deltas, the AssetDatabaseFullSnapshot type needs to be reflected");

		// Construct a deserialized reflection manager for these 3 types, such that we can speed up the creation of it during the deserialization
		data->deserialized_reflection_manager = ReflectionManager(&data->deserialized_reflection_manager_allocator, HashTablePowerOfTwoCapacityForElements(3), 0);
		// Check for the insertion of elements, in case they might have shared types
		data->entity_manager_change_set_field_table.ToNormalReflection(&data->deserialized_reflection_manager, &data->deserialized_reflection_manager_allocator, false, true);
		data->asset_database_delta_change_field_table.ToNormalReflection(&data->deserialized_reflection_manager, &data->deserialized_reflection_manager_allocator, false, true);
		data->asset_database_full_snapshot_field_table.ToNormalReflection(&data->deserialized_reflection_manager, &data->deserialized_reflection_manager_allocator, false, true);

		// Deserialize the entity manager header section
		Optional<DeserializeEntityManagerHeaderSectionData> deserialized_entity_manager_header_section = DeserializeEntityManagerHeaderSection(
			functor_data->read_instrument,
			&data->deserialized_reflection_manager_allocator,
			&data->deserialize_entity_manager_options
		);
		if (!deserialized_entity_manager_header_section.has_value) {
			return false;
		}
		data->entity_manager_header_section = deserialized_entity_manager_header_section.value;
		
		// Now, read the general functor information
		size_t header_chunk_functor_count = 0;
		size_t entire_chunk_functor_count = 0;
		size_t delta_chunk_functor_count = 0;

		if (!DeserializeIntVariableLengthBoolMultipleEnsureRange(functor_data->read_instrument, header_chunk_functor_count, entire_chunk_functor_count, delta_chunk_functor_count)) {
			return false;
		}
		
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(temporary_allocator, ECS_KB * 4, ECS_MB);

		// Read the entire chunk and delta functors. Use a helper function, since the idea is the same for both
		auto read_functor_info = [&](size_t count, Stream<ReaderData::SerializedFunctor>& serialized_functors, Stream<LoadSceneNamedChunkFunctor> read_functors) -> bool {
			serialized_functors.Initialize(&data->initialize_options_allocator, count);
			
			for (size_t index = 0; index < count; index++) {
				unsigned int version = 0;
				if (!DeserializeIntVariableLengthBool(functor_data->read_instrument, version)) {
					return false;
				}

				temporary_allocator.Clear();
				Stream<char> name;
				if (!functor_data->read_instrument->ReadOrReferenceDataWithSizeVariableLength(name, &temporary_allocator)) {
					return false;
				}

				serialized_functors[index].version = version;
				// Find the matching functor
				size_t functor_index = read_functors.Find(name, [](const LoadSceneNamedChunkFunctor& functor) {
					return functor.name;
				});
				serialized_functors[index].mapped_chunk = functor_index == -1 ? (unsigned int)-1 : (unsigned int)functor_index;
			}

			return true;
		};

		if (!read_functor_info(entire_chunk_functor_count, data->entire_state_serialized_functors, data->initialize_options.read_entire_functors)) {
			return false;
		}
		if (!read_functor_info(delta_chunk_functor_count, data->delta_state_serialized_functors, data->initialize_options.read_delta_functors)) {
			return false;
		}

		for (size_t index = 0; index < header_chunk_functor_count; index++) {
			unsigned int version = 0;
			if (!DeserializeIntVariableLengthBool(functor_data->read_instrument, version)) {
				return false;
			}

			temporary_allocator.Clear();
			Stream<char> name;
			if (!functor_data->read_instrument->ReadOrReferenceDataWithSizeVariableLength(name, &temporary_allocator)) {
				return false;
			}

			size_t chunk_write_size = 0;
			if (!functor_data->read_instrument->Read(&chunk_write_size)) {
				return false;
			}

			// Find the associated functor
			size_t functor_index = data->initialize_options.read_header_functors.Find(name, [](const LoadSceneNamedChunkFunctor& functor) {
				return functor.name;
			});
			
			if (functor_index == -1) {
				// We can skip the chunk
				if (!functor_data->read_instrument->Ignore(chunk_write_size)) {
					return false;
				}
			}
			else {
				// Read the chunk. Create a subinstrument, such that it doesn't read overbounds
				ReadInstrument::SubinstrumentData subinstrument_data;
				auto pop_subinstrument = functor_data->read_instrument->PushSubinstrument(&subinstrument_data, chunk_write_size);

				LoadSceneChunkFunctionData load_chunk_data;
				load_chunk_data.entity_manager = data->current_entity_manager;
				load_chunk_data.file_version = version;
				load_chunk_data.read_instrument = functor_data->read_instrument;
				load_chunk_data.reflection_manager = data->reflection_manager;
				load_chunk_data.user_data = data->initialize_options.read_header_functors[functor_index].user_data.buffer;
				if (!data->initialize_options.read_header_functors[functor_index].function(&load_chunk_data)) {
					return false;
				}
			}
		}

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	static void WriteUpdateEntityManagerState(WriterData* data) {
		// TODO: Determine if it is worth only applying the diff instead of blindly copying the entire data.
		// It most likely is worth, but it requires a new apply function, that is not trivial and requires quite
		// A substantial amount of work. Wait to see the real world performance cost of doing this teardown and re-construction

		// Don't forget to deallocate the allocator, since all previous data can be winked
		data->previous_entity_manager.ClearEntitiesAndAllocator();
		data->previous_entity_manager.CopyOther(data->current_entity_manager);
	}

	static bool WriterDeltaFunction(DeltaStateWriterDeltaFunctionData* function_data) {
		WriterData* data = (WriterData*)function_data->user_data;

		// Determine the asset database change set and serialize it, before the entity change set
		// This snapshot should be allocated from the next allocator, such that it remains valid until the next iteration
		ResizableLinearAllocator& asset_snapshot_allocator = data->AdvanceAssetSnapshotAllocatorIndex();
		asset_snapshot_allocator.Clear();
		AssetDatabaseFullSnapshot current_asset_database_snapshot = data->current_asset_database->GetFullSnapshot(&asset_snapshot_allocator);
		// We can reference the identifiers from the snapshot, since they are stable until the next iteration
		AssetDatabaseDeltaChange asset_database_delta_change = DetermineAssetDatabaseDeltaChange(data->previous_asset_database_snapshot, current_asset_database_snapshot, &asset_snapshot_allocator, true);
		
		// Serialize the asset database delta change now (without the field table)
		SerializeOptions asset_database_serialize_options;
		asset_database_serialize_options.write_type_table = false;
		asset_database_serialize_options.verify_dependent_types = false;
		if (Serialize(data->reflection_manager, data->reflection_manager->GetType(STRING(AssetDatabaseDeltaChange)), &asset_database_delta_change, function_data->write_instrument, &asset_database_serialize_options) != ECS_SERIALIZE_OK) {
			return false;
		}

		// Set the current asset snapshot to be the previous, we can directly assign it, its allocations are already stable
		data->previous_asset_database_snapshot = current_asset_database_snapshot;

		// Don't forget to clear the change set allocator before using it
		data->change_set_allocator.Clear();
		// Determine the entity manager change set and serialize it
		EntityManagerChangeSet change_set = DetermineEntityManagerChangeSet(
			&data->previous_entity_manager, 
			data->current_entity_manager, 
			data->reflection_manager, 
			&data->change_set_allocator
		);

		SerializeOptions entity_change_set_serialize_options;
		entity_change_set_serialize_options.verify_dependent_types = false;
		entity_change_set_serialize_options.write_type_table = false;

		SerializeEntityManagerChangeSetOptions change_set_options;
		change_set_options.write_entity_manager_header_section = false;
		change_set_options.serialize_options = &entity_change_set_serialize_options;
		if (!SerializeEntityManagerChangeSet(
			&change_set, 
			data->current_entity_manager, 
			&data->serialize_options, 
			data->reflection_manager, 
			function_data->write_instrument, 
			&change_set_options
		)) {
			return false;
		}

		// Write the optional chunks now
		SceneDeltaWriterChunkFunctionData save_chunk_data;
		save_chunk_data.current_entity_manager = data->current_entity_manager;
		save_chunk_data.previous_entity_manager = &data->previous_entity_manager;
		save_chunk_data.reflection_manager = data->reflection_manager;
		save_chunk_data.write_instrument = function_data->write_instrument;
		save_chunk_data.asset_database = data->current_asset_database;
		save_chunk_data.asset_database_snapshot = &data->previous_asset_database_snapshot;
		for (size_t index = 0; index < data->initialize_options.save_delta_functors.size; index++) {
			if (!function_data->write_instrument->WriteChunkWithSizeHeader([&](WriteInstrument* write_instrument) -> bool {
				save_chunk_data.user_data = data->initialize_options.save_delta_functors[index].user_data.buffer;
				return data->initialize_options.save_delta_functors[index].function(&save_chunk_data);
			})) {
				return false;
			}
		}

		// Update the entity manager state
		WriteUpdateEntityManagerState(data);

		return true;
	}

	static bool WriterEntireFunction(DeltaStateWriterEntireFunctionData* function_data) {
		WriterData* data = (WriterData*)function_data->user_data;

		// Instead of serializing the asset database delta change, we need to serialize the full snapshot
		ResizableLinearAllocator& asset_snapshot_allocator = data->AdvanceAssetSnapshotAllocatorIndex();
		asset_snapshot_allocator.Clear();
		AssetDatabaseFullSnapshot current_asset_database_snapshot = data->current_asset_database->GetFullSnapshot(&asset_snapshot_allocator);

		// Serialize the asset database snapshot now (without the field table)
		SerializeOptions serialize_options;
		serialize_options.write_type_table = false;
		if (Serialize(data->reflection_manager, data->reflection_manager->GetType(STRING(AssetDatabaseFullSnapshot)), &current_asset_database_snapshot, function_data->write_instrument, &serialize_options) != ECS_SERIALIZE_OK) {
			return false;
		}

		// Update the asset database snapshot
		data->previous_asset_database_snapshot = current_asset_database_snapshot;

		// Now serialize the entire scene, without the header, to reduce the memory footprint
		// Reset the buffering
		data->entire_scene_entity_manager_buffering.size = 0;
		if (!SerializeEntityManager(data->current_entity_manager, function_data->write_instrument, data->entire_scene_entity_manager_buffering, &data->serialize_options, &data->entity_manager_header_section_output)) {
			return false;
		}

		// Update the entity manager state
		WriteUpdateEntityManagerState(data);

		// Write the optional chunks now
		SaveSceneChunkFunctionData save_chunk_data;
		save_chunk_data.entity_manager = data->current_entity_manager;
		save_chunk_data.reflection_manager = data->reflection_manager;
		save_chunk_data.write_instrument = function_data->write_instrument;
		for (size_t index = 0; index < data->initialize_options.save_entire_functors.size; index++) {
			if (!function_data->write_instrument->WriteChunkWithSizeHeader([&](WriteInstrument* write_instrument) -> bool {
				save_chunk_data.user_data = data->initialize_options.save_entire_functors[index].user_data.buffer;
				return data->initialize_options.save_entire_functors[index].function(&save_chunk_data);
			})) {
				return false;
			}
		}

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	static bool ReaderDeltaFunction(DeltaStateReaderDeltaFunctionData* function_data) {
		ReaderData* data = (ReaderData*)function_data->user_data;

		// TODO: Determine how to handle this? 
		// Resources should be preloaded, and somehow the runtime should know the appropriate load function and unload
		// Function such that it can work properly in the editor context. For the moment, until a proper streaming system
		// Is decided, disable it
		
		if constexpr (false) {
			data->deserialized_reflection_manager_allocator.SetMarker();

			// Read the asset database delta change
			DeserializeOptions deserialize_options;
			deserialize_options.read_type_table = false;
			deserialize_options.field_table = &data->asset_database_delta_change_field_table;
			deserialize_options.deserialized_field_manager = &data->deserialized_reflection_manager;
			deserialize_options.verify_dependent_types = false;
			// Use this as an allocator, since we can set a marker and return to it later on
			deserialize_options.field_allocator = &data->deserialized_reflection_manager_allocator;

			AssetDatabaseDeltaChange asset_database_delta_change;
			if (Deserialize(data->reflection_manager, data->reflection_manager->GetType(STRING(AssetDatabaseDeltaChange)), &asset_database_delta_change, function_data->read_instrument, &deserialize_options) != ECS_DESERIALIZE_OK) {
				return false;
			}

			data->deserialized_reflection_manager_allocator.ReturnToMarker();
		}
		else {
			if (!IgnoreDeserialize(function_data->read_instrument, data->asset_database_delta_change_field_table, nullptr, &data->deserialized_reflection_manager)) {
				return false;
			}
		}

		// Now, read the entity manager change set
		data->change_set_allocator.Clear();

		DeserializeOptions entity_manager_change_set_options;
		entity_manager_change_set_options.read_type_table = false;
		entity_manager_change_set_options.deserialized_field_manager = &data->deserialized_reflection_manager;
		entity_manager_change_set_options.field_table = &data->entity_manager_change_set_field_table;
		entity_manager_change_set_options.field_allocator = &data->change_set_allocator;
		entity_manager_change_set_options.verify_dependent_types = false;
		
		DeserializeEntityManagerChangeSetOptions deserialize_entity_manager_change_set_options;
		deserialize_entity_manager_change_set_options.deserialize_change_set_options = &entity_manager_change_set_options;
		deserialize_entity_manager_change_set_options.deserialize_manager_header_section = &data->entity_manager_header_section;
		if (!DeserializeEntityManagerChangeSet(
			data->current_entity_manager,
			&data->deserialize_entity_manager_options,
			data->reflection_manager,
			function_data->read_instrument,
			&deserialize_entity_manager_change_set_options
		)) {
			return false;
		}

		// Read the optional chunks now
		LoadSceneChunkFunctionData read_chunk_data;
		read_chunk_data.entity_manager = data->current_entity_manager;
		read_chunk_data.reflection_manager = data->reflection_manager;
		read_chunk_data.read_instrument = function_data->read_instrument;
		for (size_t index = 0; index < data->delta_state_serialized_functors.size; index++) {
			if (!function_data->read_instrument->ReadOrIgnoreChunkWithSizeHeader(data->delta_state_serialized_functors[index].mapped_chunk == -1, [&](ReadInstrument* read_instrument, size_t chunk_size) -> bool {
				read_chunk_data.file_version = data->delta_state_serialized_functors[index].version;
				const LoadSceneNamedChunkFunctor& load_chunk_functor = data->initialize_options.read_delta_functors[data->delta_state_serialized_functors[index].mapped_chunk];
				read_chunk_data.user_data = load_chunk_functor.user_data.buffer;
				return load_chunk_functor.function(&read_chunk_data);
			})) {
				return false;
			}
		}

		return true;
	}

	static bool ReaderEntireFunction(DeltaStateReaderEntireFunctionData* function_data) {
		// This is a mirror of the delta function, but instead of the delta function, we use the entire function
		ReaderData* data = (ReaderData*)function_data->user_data;

		// TODO: Decide how to tackle this? We should probably have a callback that the user provides in order to
		// Execute the read, but it shouldn't be done now, ideally, it should be streamed ahead of time
		if constexpr (false) {
			data->deserialized_reflection_manager_allocator.SetMarker();

			// Read the asset database delta change
			DeserializeOptions deserialize_options;
			deserialize_options.read_type_table = false;
			deserialize_options.field_table = &data->asset_database_full_snapshot_field_table;
			deserialize_options.deserialized_field_manager = &data->deserialized_reflection_manager;
			deserialize_options.verify_dependent_types = false;
			// Use this as an allocator, since we can set a marker and return to it later on
			deserialize_options.field_allocator = &data->deserialized_reflection_manager_allocator;

			AssetDatabaseFullSnapshot asset_database_snapshot;
			if (Deserialize(data->reflection_manager, data->reflection_manager->GetType(STRING(AssetDatabaseFullSnapshot)), &asset_database_snapshot, function_data->read_instrument, &deserialize_options) != ECS_DESERIALIZE_OK) {
				return false;
			}

			// Call the callback
			SceneDeltaReaderAssetLoadCallbackData callback_data;
			callback_data.full_snapshot = &asset_database_snapshot;
			callback_data.is_delta_change = false;
			callback_data.user_data = data->initialize_options.asset_load_callback_data;
			data->initialize_options.asset_load_callback(&callback_data);

			data->deserialized_reflection_manager_allocator.ReturnToMarker();
		}
		else {
			if (!IgnoreDeserialize(function_data->read_instrument, data->asset_database_full_snapshot_field_table, nullptr, &data->deserialized_reflection_manager)) {
				return false;
			}
		}

		// We can read the scene now
		data->deserialize_entire_scene_allocator.Clear();
		if (DeserializeEntityManager(
			data->current_entity_manager, 
			function_data->read_instrument, 
			data->entity_manager_header_section, 
			&data->deserialize_entity_manager_options, 
			&data->deserialize_entire_scene_allocator
		) != ECS_DESERIALIZE_ENTITY_MANAGER_OK) {
			return false;
		}

		// Read the optional chunks now
		LoadSceneChunkFunctionData read_chunk_data;
		read_chunk_data.entity_manager = data->current_entity_manager;
		read_chunk_data.reflection_manager = data->reflection_manager;
		read_chunk_data.read_instrument = function_data->read_instrument;
		for (size_t index = 0; index < data->entire_state_serialized_functors.size; index++) {
			if (!function_data->read_instrument->ReadOrIgnoreChunkWithSizeHeader(data->entire_state_serialized_functors[index].mapped_chunk == -1, [&](ReadInstrument* read_instrument, size_t chunk_size) -> bool {
				read_chunk_data.file_version = data->entire_state_serialized_functors[index].version;
				const LoadSceneNamedChunkFunctor& load_chunk_functor = data->initialize_options.read_entire_functors[data->entire_state_serialized_functors[index].mapped_chunk];
				read_chunk_data.user_data = load_chunk_functor.user_data.buffer;
				return load_chunk_functor.function(&read_chunk_data);
			})) {
				return false;
			}
		}

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	unsigned char SceneDeltaVersion() {
		return VERSION;
	}

	static SceneDeltaSerializationHeader GetSceneDeltaHeader() {
		SceneDeltaSerializationHeader header;
		ZeroOut(&header);
		header.version = VERSION;
		return header;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void SetSceneDeltaWriterInitializeInfo(
		DeltaStateWriterInitializeFunctorInfo& info,
		const EntityManager* entity_manager,
		const ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		const float* delta_time_value,
		const float* speed_up_time_value,
		AllocatorPolymorphic temporary_allocator,
		const SceneDeltaWriterInitializeInfoOptions* options
	) {
		info.delta_function = WriterDeltaFunction;
		info.entire_function = WriterEntireFunction;
		info.self_contained_extract = nullptr;
		info.user_data_allocator_initialize = WriterInitialize;
		info.user_data_allocator_deallocate = WriterDeallocate;
		info.header_write_function = WriterHeaderWriteFunction;

		SceneDeltaSerializationHeader* serialization_header = Allocate<SceneDeltaSerializationHeader>(temporary_allocator);
		*serialization_header = GetSceneDeltaHeader();
		info.header = serialization_header;

		WriterData* writer_data = Allocate<WriterData>(temporary_allocator);
		writer_data->current_entity_manager = entity_manager;
		writer_data->current_asset_database = asset_database;
		writer_data->delta_time_value = delta_time_value;
		writer_data->speed_up_time_value = speed_up_time_value;

		// At the moment, just assign these values, they will be properly allocated in the initialize
		writer_data->initialize_options = *options;	
		info.user_data = writer_data;
	}

	void SetSceneDeltaWriterWorldInitializeInfo(
		DeltaStateWriterInitializeFunctorInfo& info,
		const World* world,
		const ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		AllocatorPolymorphic temporary_allocator,
		const SceneDeltaWriterInitializeInfoOptions* options
	) {
		// Call the basic function and simply override the user data and the extract function, since that's what is different
		SetSceneDeltaWriterInitializeInfo(
			info, 
			world->entity_manager, 
			reflection_manager, 
			asset_database, 
			&world->delta_time, 
			&world->speed_up_factor, 
			temporary_allocator,
			options
		);
		info.self_contained_extract = WriterExtractFunction;

		// The current info.user_data should contain a WriterData that was filled in,
		// We only need to write the additional world field, but we need to make another allocation

		WriterData* writer_data = (WriterData*)info.user_data.buffer;
		WriterWorldData* writer_world_data = Allocate<WriterWorldData>(temporary_allocator);
		memcpy(writer_world_data, writer_data, sizeof(*writer_data));
		writer_world_data->world = world;
		info.user_data = writer_world_data;
	}

	void SetSceneDeltaReaderInitializeInfo(
		DeltaStateReaderInitializeFunctorInfo& info,
		EntityManager* entity_manager,
		const ReflectionManager* reflection_manager,
		AllocatorPolymorphic temporary_allocator,
		const SceneDeltaReaderInitializeInfoOptions* options
	) {
		info.delta_function = ReaderDeltaFunction;
		info.entire_function = ReaderEntireFunction;
		info.user_data_allocator_initialize = ReaderInitialize;
		info.user_data_allocator_deallocate = ReaderDeallocate;
		info.header_read_function = ReaderHeaderReadFunction;
			
		ReaderData reader_data;
		ZeroOut(&reader_data);

		reader_data.current_entity_manager = entity_manager;
		reader_data.reflection_manager = reflection_manager;
		// Don't allocate anything for the options, they will be properly allocated in the initialize
		reader_data.initialize_options = *options;
	}

	void SetSceneDeltaReaderWorldInitializeInfo(
		DeltaStateReaderInitializeFunctorInfo& info,
		World* world,
		const ReflectionManager* reflection_manager,
		AllocatorPolymorphic temporary_allocator,
		const SceneDeltaReaderInitializeInfoOptions* options
	) {
		// Can forward to the normal initialize
		SetSceneDeltaReaderInitializeInfo(info, world->entity_manager, reflection_manager, temporary_allocator, options);
	}

}