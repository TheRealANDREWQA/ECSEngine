#include "ecspch.h"
#include "SceneDelta.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Utilities/Serialization/DeltaStateSerialization.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "../ECS/EntityManager.h"
#include "../ECS/EntityManagerSerialize.h"
#include "../ECS/World.h"
#include "../Resources/AssetDatabase.h"

// The current format version
#define VERSION 0

#define CHANGE_SET_ALLOCATOR_CAPACITY ECS_MB * 50
#define CHANGE_SET_ALLOCATOR_BACKUP_CAPACITY ECS_MB * 100

namespace ECSEngine {

	using namespace Reflection;

	// -----------------------------------------------------------------------------------------------------------------------------

	// EntityManagerType should be EntityManager or const EntityManager
	template<typename EntityManagerType, typename AssetDatabaseType>
	struct WriterReaderBaseData {
		// Maintain a personal allocator for the previous state, such that we don't force the caller to do
		// This, which can be quite a hassle
		GlobalMemoryManager previous_state_allocator;
		// Use a separate allocator for the change set, that is linear in nature since it is temporary,
		// Such that we can wink it in order to deallocate the change set
		ResizableLinearAllocator change_set_allocator;
		EntityManager previous_entity_manager;
		EntityManagerType* current_entity_manager;
		AssetDatabase previous_asset_database;
		AssetDatabaseType* current_asset_database;
		SerializeEntityManagerOptions serialize_options;
	};

	struct WriterData : public WriterReaderBaseData<const EntityManager, const AssetDatabase> {
		// We need to record this reflection manager such that we can write the variable length header
		const ReflectionManager* reflection_manager;
		// These 2 floats can be nullptr, in case they are not specified
		const float* delta_time_value;
		const float* speed_up_time_value;
	};

	struct ReaderData : public WriterReaderBaseData<EntityManager, AssetDatabase> {
		// This reflection manager will store the reflection types used for serialization
		// In the file. It will be constructed upon initialization
		ReflectionManager reflection_manager;
		// In this field, the deserialized table will be recorded such that in can be forwarded to
		// The deserialize calls directly
		DeserializeFieldTable file_field_table;
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

		// Copy the current contents
		delta_state->previous_entity_manager.CopyOther(delta_state->current_entity_manager);
		// Use malloc for now for the change set allocator
		delta_state->change_set_allocator = ResizableLinearAllocator(CHANGE_SET_ALLOCATOR_CAPACITY, CHANGE_SET_ALLOCATOR_BACKUP_CAPACITY, ECS_MALLOC_ALLOCATOR);
	}

	static void WriterDeallocate(void* user_data, AllocatorPolymorphic allocator) {
		WriterData* delta_state = (WriterData*)user_data;
		delta_state->previous_state_allocator.Free();
	}

	static void ReaderInitialize(void* user_data, AllocatorPolymorphic allocator) {
		// The function is identical to the writer, and the data layout is identical as well, so we can directly forward to it
		WriterInitialize(user_data, allocator);

		// There is one more thing to do tho, and that is to initialize the reflection manager
		ReaderData* data = (ReaderData*)user_data;
		data->reflection_manager = ReflectionManager(&data->previous_state_allocator, 0, 0);
	}

	static void ReaderDeallocate(void* user_data, AllocatorPolymorphic allocator) {
		// The function is identical to the writer, and the data layout is identical as well, so we can directly forward to it
		WriterDeallocate(user_data, allocator);
	}

	static float WriterExtractFunction(void* user_data) {
		WriterWorldData* data = (WriterWorldData*)user_data;
		return data->world->elapsed_seconds;
	}

	static bool WriterHeaderWriteFunction(void* user_data, WriteInstrument* write_instrument) {
		WriterData* data = (WriterData*)user_data;

		// Ensure that the EntityManagerChangeSet exists among these types
		const ReflectionType* entity_manager_change_set_reflection_type = data->reflection_manager->TryGetType(STRING(EntityManagerChangeSet));
		ECS_ASSERT(entity_manager_change_set_reflection_type != nullptr,
			"In order to write scene deltas the EntityManagerChangeSet needs to be reflected");

		// Write the entity manager change set reflectable type, such that we can recover it for this file type
		if (!SerializeFieldTable(data->reflection_manager, entity_manager_change_set_reflection_type, write_instrument)) {
			return false;
		}

		// We need to write an entity manager header section, this will contain all the reflection data needed for
		// Deserializing all entire and delta states.
		return SerializeEntityManagerHeaderSection(data->current_entity_manager, write_instrument, &data->serialize_options);
	}

	static bool ReaderHeaderReadFunction(DeltaStateReaderHeaderReadFunctionData* functor_data) {
		// Do a quick pre-check, if the variable length header is missing, fail
		if (functor_data->write_size == 0) {
			return false;
		}

		ReaderData* data = (ReaderData*)functor_data->user_data;

		// Read the reflection manager stored in the file
		bool success = DeserializeReflectionManager(&data->reflection_manager, functor_data->read_instrument, &data->previous_state_allocator, &data->file_field_table);
		if (!success) {
			return false;
		}

		// Ensure that the entity manager change set structure exists among them
		if (data->reflection_manager.TryGetType(STRING(EntityManagerChangeSet)) == nullptr) {
			return false;
		}

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	static bool WriterDeltaFunction(DeltaStateWriterDeltaFunctionData* function_data) {
		WriterData* data = (WriterData*)function_data->user_data;

		// Determine the entity manager change set and serialize it
		EntityManagerChangeSet change_set = DetermineEntityManagerChangeSet(
			&data->previous_entity_manager, 
			data->current_entity_manager, 
			data->reflection_manager, 
			&data->change_set_allocator
		);
		if (!SerializeEntityManagerChangeSet(
			&change_set, 
			data->current_entity_manager, 
			&data->serialize_options, 
			data->reflection_manager, 
			function_data->write_instrument, 
			false
		)) {
			return false;
		}

		// Determine the asset database change set and serialize it
		return true;
	}

	static bool WriterEntireFunction(DeltaStateWriterEntireFunctionData* function_data) {
		WriterData* data = (WriterData*)function_data->user_data;

		/*if (!SerializeMouseInput(data->mouse, function_data->write_instrument)) {
			return false;
		}
		data->previous_mouse = *data->mouse;

		if (!SerializeKeyboardInput(data->keyboard, function_data->write_instrument)) {
			return false;
		}
		data->previous_keyboard.CopyOther(data->keyboard);*/

		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	static bool ReaderDeltaFunction(DeltaStateReaderDeltaFunctionData* function_data) {
		ReaderData* data = (ReaderData*)function_data->user_data;
		//return InputDeltaReaderFunctionImpl(function_data, [function_data, data](const InputSerializationHeader& header, Stream<ECS_INPUT_SERIALIZE_TYPE> accepted_input_types) {
		//	return DeserializeInputDelta(&data->previous_mouse, data->mouse, &data->previous_keyboard, data->keyboard, function_data->read_instrument, header, accepted_input_types);
		//	});

		return false;
	}

	static bool ReaderEntireFunction(DeltaStateReaderEntireFunctionData* function_data) {
		// This is a mirror of the delta function, but instead of the delta function, we use the entire function
		ReaderData* data = (ReaderData*)function_data->user_data;
		//return InputDeltaReaderFunctionImpl(function_data, [function_data, data](const InputSerializationHeader& header, Stream<ECS_INPUT_SERIALIZE_TYPE> accepted_input_types) {
		//	return DeserializeInput(data->mouse, data->keyboard, function_data->read_instrument, header, accepted_input_types);
		//	});

		return false;
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
		CapacityStream<void>& stack_memory,
		const SceneDeltaWriterInitializeInfoOptions* options
	) {
		info.delta_function = WriterDeltaFunction;
		info.entire_function = WriterEntireFunction;
		info.self_contained_extract = nullptr;
		info.user_data_allocator_initialize = WriterInitialize;
		info.user_data_allocator_deallocate = WriterDeallocate;
		info.header_write_function = WriterHeaderWriteFunction;

		SceneDeltaSerializationHeader serialization_header = GetSceneDeltaHeader();
		info.header = stack_memory.Add(&serialization_header);

		WriterData writer_data;
		writer_data.current_entity_manager = entity_manager;
		writer_data.current_asset_database = asset_database;
		writer_data.delta_time_value = delta_time_value;
		writer_data.speed_up_time_value = speed_up_time_value;
		info.user_data = stack_memory.Add(&writer_data);
	}

	void SetSceneDeltaWriterWorldInitializeInfo(
		DeltaStateWriterInitializeFunctorInfo& info,
		const World* world,
		const ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		CapacityStream<void>& stack_memory,
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
			stack_memory, 
			options
		);
		info.self_contained_extract = WriterExtractFunction;

		// The current info.user_data should contain a WriterData that was filled in,
		// We only need to write the additional world field

		WriterWorldData* writer_data = (WriterWorldData*)info.user_data.buffer;
		stack_memory.Reserve(sizeof(*writer_data) - sizeof(WriterData));
		writer_data->world = world;
		info.user_data.size = sizeof(*writer_data);
	}

	void SetSceneDeltaReaderInitializeInfo(
		DeltaStateReaderInitializeFunctorInfo& info,
		EntityManager* entity_manager,
		const ReflectionManager* reflection_manager,
		CapacityStream<void>& stack_memory
	) {
		info.delta_function = ReaderDeltaFunction;
		info.entire_function = ReaderEntireFunction;
		info.user_data_allocator_initialize = ReaderInitialize;
		info.user_data_allocator_deallocate = ReaderDeallocate;
		info.header_read_function = ReaderHeaderReadFunction;

		//ReaderData reader_data;
		//ZeroOut(&reader_data);
		//reader_data.current_state = entity_manager;
		//info.user_data = stack_memory.Add(&reader_data);
	}

	void SetSceneDeltaReaderWorldInitializeInfo(
		DeltaStateReaderInitializeFunctorInfo& info,
		World* world,
		const ReflectionManager* reflection_manager,
		CapacityStream<void>& stack_memory
	) {
		// Can forward to the normal initialize
		SetSceneDeltaReaderInitializeInfo(info, world->entity_manager, reflection_manager, stack_memory);
	}

}