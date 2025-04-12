#include "ecspch.h"
#include "EntityManagerChangeSet.h"
#include "EntityManager.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Utilities/Serialization/DeltaStateSerialization.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "World.h"

// 256
#define DECK_POWER_OF_TWO_EXPONENT 8

// The current format version
#define VERSION 0

using namespace ECSEngine::Reflection;

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------------------------

	void EntityManagerChangeSet::Initialize(AllocatorPolymorphic allocator) {
		entity_unique_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_destroys.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		entity_info_additions.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		shared_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
		global_component_changes.Initialize(allocator, 1, DECK_POWER_OF_TWO_EXPONENT);
	}

	void EntityManagerChangeSet::Deallocate() {
		// The unique and shared changes have their own buffers
		entity_unique_component_changes.ForEach([&](const EntityChanges& changes) {
			changes.changes.Deallocate(Allocator());
			return false;
		});
		shared_component_changes.ForEach([&](const SharedComponentChanges& changes) {
			changes.changes.Deallocate(Allocator());
			return false;
		});

		entity_unique_component_changes.Deallocate();
		entity_info_changes.Deallocate();
		entity_info_destroys.Deallocate();
		entity_info_additions.Deallocate();
		shared_component_changes.Deallocate();
		global_component_changes.Deallocate();
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	// The change set must have the buffers set up before calling this function. This function only calculates the changes that have taken
	// Place in the entity pool entity info structures, it does not check unique/shared/global components
	static void DetermineEntityManagerEntityInfoChangeSet(const EntityManager* previous_entity_manager, const EntityManager* new_entity_manager, EntityManagerChangeSet& change_set) {
		const EntityPool* previous_pool = previous_entity_manager->m_entity_pool;
		const EntityPool* new_pool = new_entity_manager->m_entity_pool;

		// We can utilize the existing interface of the entity pool in order to query the data
		previous_pool->ForEach([&](Entity entity, EntityInfo entity_info) {
			// Check to see if the entity info still exists in the new entity manager, if it doesn't, a removal took place
			unsigned int new_pool_generation_index = new_pool->IsEntityAt(entity.index);
			if (new_pool_generation_index == -1) {
				// Removal
				change_set.entity_info_destroys.Add({ entity });
			}
			else {
				// If the generation index is different, then it means that the previous entity was destroyed and a new one was created
				// In its place instead
				EntityInfo new_entity_info = new_pool->GetInfoNoChecks(Entity(entity.index, new_pool_generation_index));

				if (new_pool_generation_index != entity_info.generation_count) {
					change_set.entity_info_destroys.Add({ entity });
					change_set.entity_info_additions.Add({ entity, new_entity_info });
				}
				else {
					// The generation is the same, then check the fields of the entity info for changes
					if (entity_info != new_entity_info) {
						change_set.entity_info_changes.Add({ entity, new_entity_info });
					}
				}
			}
		});

		// After iterating over the previous pool, we now need to iterate over the new pool to determine new additions
		new_pool->ForEach([&](Entity entity, EntityInfo entity_info) {
			unsigned int previous_pool_generation_index = new_pool->IsEntityAt(entity.index);
			if (previous_pool_generation_index == -1) {
				// This is a new addition, register it
				change_set.entity_info_additions.Add({ entity, entity_info });
			}
		});
	}

	static void DetermineEntityManagerUniqueComponentChangeSet(
		const EntityManager* previous_entity_manager, 
		const EntityManager* new_entity_manager, 
		const ReflectionManager* reflection_manager,
		EntityManagerChangeSet& change_set
	) {
		ComponentSignature archetype_unique_signature;
		const ReflectionType* unique_signature_reflection_types[ECS_ARCHETYPE_MAX_COMPONENTS];
		
		// Iterate over the previous manager, using the for each function, and for all entities that exist in the other manager as well,
		// Perform the cross reference to see what was added/removed/updated
		previous_entity_manager->ForEachEntity(
			[&](const Archetype* archetype) {
				archetype_unique_signature = archetype->GetUniqueSignature();
				// Cache the reflection types per each component
				for (size_t index = 0; index < archetype_unique_signature.count; index++) {
					unique_signature_reflection_types[index] = reflection_manager->GetType(previous_entity_manager->GetComponentName(archetype_unique_signature[index]));
				}
			}, 
			[](const Archetype* archetype, unsigned int base_index) {}, 
			[&](const Archetype* archetype, const ArchetypeBase* archetype_base, Entity entity, void** unique_components) {
				ECS_STACK_CAPACITY_STREAM(EntityManagerChangeSet::EntityComponentChange, component_changes, ECS_ARCHETYPE_MAX_COMPONENTS);

				size_t changes_deck_index = change_set.entity_unique_component_changes.ReserveAndUpdateSize();
				EntityManagerChangeSet::EntityChanges& changes = change_set.entity_unique_component_changes[changes_deck_index];

				const EntityInfo* new_entity_info = new_entity_manager->TryGetEntityInfo(entity);
				if (new_entity_info != nullptr) {
					for (size_t index = 0; index < archetype_unique_signature.count; index++) {
						Component current_component = archetype_unique_signature[index];
						const void* new_component = new_entity_manager->TryGetComponent(*new_entity_info, current_component);
						if (new_component == nullptr) {
							// The new entity is missing this component, register the change
							component_changes.AddAssert({ ECS_CHANGE_SET_REMOVE, current_component });
						}
						else {
							// The component exists, compare the data
							bool is_data_the_same = CompareReflectionTypeInstances(reflection_manager, unique_signature_reflection_types[index], unique_components[index], new_component);
							if (!is_data_the_same) {
								component_changes.AddAssert({ ECS_CHANGE_SET_UPDATE, current_component });
							}
						}
					}

					// After iterating over the previous manager entity components, iterate over the components in the new manager and detect those
					// That do not appear in the previous but are present in the new manager, this indicates that a new component was added
					ComponentSignature new_entity_signature = new_entity_manager->GetEntitySignatureStable(entity);
					for (size_t index = 0; index < new_entity_signature.count; index++) {
						if (archetype_unique_signature.Find(new_entity_signature[index]) == UCHAR_MAX) {
							// The component was added
							component_changes.AddAssert({ ECS_CHANGE_SET_ADD, new_entity_signature[index] });
						}
					}

					if (component_changes.size > 0) {
						// Insert this entry
						change_set.entity_unique_component_changes.Add({ entity, component_changes.Copy(change_set.Allocator()) });
					}
				}
			}
		);
	}

	static void DetermineEntityManagerSharedComponentChangeSet(
		const EntityManager* previous_entity_manager,
		const EntityManager* new_entity_manager,
		const ReflectionManager* reflection_manager,
		EntityManagerChangeSet& change_set
	) {
		// TODO: Assert that the named shared instances did not modify - at the moment we do not handle them
		previous_entity_manager->ForEachSharedComponent([&](Component component) {
			const ReflectionType* reflection_type = reflection_manager->GetType(previous_entity_manager->GetSharedComponentName(component));

			ECS_ASSERT(previous_entity_manager->GetNamedSharedInstanceCount(component) == 0 && new_entity_manager->GetNamedSharedInstanceCount(component) == 0, 
				"Named shared instances are not yet handled for EntityManagerChangeSet");

			ECS_STACK_CAPACITY_STREAM(EntityManagerChangeSet::SharedComponentInstanceChange, changes, ECS_KB * 16);
			// Iterate over the previous changes, where we can detect what was removed and updated
			previous_entity_manager->ForEachSharedInstance(component, [&](SharedInstance instance) {
				if (!new_entity_manager->ExistsSharedInstanceOnly(component, instance)) {
					// This instance was destroyed
					changes.AddAssert({ instance, ECS_CHANGE_SET_REMOVE });
				}
				else {
					// See if the data has changed
					const void* previous_data = previous_entity_manager->GetSharedData(component, instance);
					const void* new_data = new_entity_manager->GetSharedData(component, instance);
					if (!CompareReflectionTypeInstances(reflection_manager, reflection_type, previous_data, new_data)) {
						changes.AddAssert({ instance, ECS_CHANGE_SET_UPDATE });
					}
				}
			});

			// Now iterate over the new instances, to see what was added
			new_entity_manager->ForEachSharedInstance(component, [&](SharedInstance instance) {
				if (!previous_entity_manager->ExistsSharedInstanceOnly(component, instance)) {
					changes.AddAssert({ instance, ECS_CHANGE_SET_ADD });
				}
			});

			if (changes.size > 0) {
				// Add a new entry
				change_set.shared_component_changes.Add({ component, changes.Copy(change_set.Allocator()) });
			}
		});
	}

	static void DetermineEntityManagerGlobalComponentChangeSet(
		const EntityManager* previous_entity_manager, 
		const EntityManager* new_entity_manager, 
		const ReflectionManager* reflection_manager,
		EntityManagerChangeSet& change_set
	) {
		// Iterate over the previous global components to find those that have been removed or updated
		previous_entity_manager->ForAllGlobalComponents([&](const void* data, Component component) {
			const void* new_data = new_entity_manager->TryGetGlobalComponent(component);
			if (new_data != nullptr) {
				// Compare the new data to see if it changed
				if (!CompareReflectionTypeInstances(reflection_manager, reflection_manager->GetType(previous_entity_manager->GetGlobalComponentName(component)), data, new_data)) {
					change_set.global_component_changes.Add({ ECS_CHANGE_SET_UPDATE, component });
				}
			}
			else {
				// The global component was removed
				change_set.global_component_changes.Add({ ECS_CHANGE_SET_REMOVE, component });
			}
		});

		// Iterate over the new global components to find those that were added
		new_entity_manager->ForAllGlobalComponents([&](const void* data, Component component) {
			if (previous_entity_manager->TryGetGlobalComponent(component) == nullptr) {
				change_set.global_component_changes.Add({ ECS_CHANGE_SET_ADD, component });
			}
		});
	}

	EntityManagerChangeSet DetermineEntityManagerChangeSet(
		const EntityManager* previous_entity_manager, 
		const EntityManager* new_entity_manager, 
		const ReflectionManager* reflection_manager, 
		AllocatorPolymorphic change_set_allocator
	) {
		EntityManagerChangeSet change_set;
		// Initialize the change set as the first action
		change_set.Initialize(change_set_allocator);

		DetermineEntityManagerEntityInfoChangeSet(previous_entity_manager, new_entity_manager, change_set);
		DetermineEntityManagerUniqueComponentChangeSet(previous_entity_manager, new_entity_manager, reflection_manager, change_set);
		DetermineEntityManagerSharedComponentChangeSet(previous_entity_manager, new_entity_manager, reflection_manager, change_set);
		DetermineEntityManagerGlobalComponentChangeSet(previous_entity_manager, new_entity_manager, reflection_manager, change_set);
	
		return change_set;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	bool SerializeEntityManagerChangeSet(
		const EntityManagerChangeSet* change_set,
		const EntityManager* new_entity_manager,
		const Reflection::ReflectionManager* reflection_manager,
		WriteInstrument* write_instrument
	) {
		// Firstly, write the change set itself, the structure that describes what changes need to be performed,
		// Then write the component data that was changed

		return false;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	// EntityManagerType should be EntityManager or const EntityManager
	template<typename EntityManagerType>
	struct WriterReaderBaseData {
		// Maintain a personal allocator for the previous state, such that we don't force the caller to do
		// This, which can be quite a hassle
		GlobalMemoryManager previous_state_allocator;
		// Use a separate allocator for the change set, that is linear in nature since it is temporary,
		// Such that we can wink it in order to deallocate the change set
		ResizableLinearAllocator change_set_allocator;
		EntityManager previous_state;
		EntityManagerType* current_state;
	};

	struct WriterData : public WriterReaderBaseData<const EntityManager> {
		// We need to record this reflection manager such that we can write the variable length header
		const ReflectionManager* reflection_manager;
	};

	struct ReaderData : public WriterReaderBaseData<EntityManager> {
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
		CreateBaseAllocatorInfo global_allocator_info = delta_state->current_state->m_memory_manager->GetInitialAllocatorInfo();
		// Ensure that the base allocator infos are of multipool type, such that we access the correct field
		ECS_ASSERT(global_allocator_info.allocator_type == ECS_ALLOCATOR_MULTIPOOL);
		ECS_ASSERT(delta_state->current_state->m_memory_manager->m_backup_info.allocator_type == ECS_ALLOCATOR_MULTIPOOL);
		size_t current_entity_manager_initial_size = global_allocator_info.multipool_capacity;
		// Use a conservative small value
		global_allocator_info.multipool_capacity += ECS_MB * 32;

		CreateGlobalMemoryManager(&delta_state->previous_state_allocator, global_allocator_info, delta_state->current_state->m_memory_manager->m_backup_info);
		EntityManagerDescriptor previous_state_descriptor;
		// We can use as memory manager the global allocator directly
		previous_state_descriptor.memory_manager = &delta_state->previous_state_allocator;
		// The deferred capacity can be 0 for this entity manager, since it won't be used to run actual deferred calls
		previous_state_descriptor.deferred_action_capacity = 0;
		CreateEntityManagerWithPool(
			&delta_state->previous_state,
			current_entity_manager_initial_size,
			global_allocator_info.multipool_block_count,
			delta_state->current_state->m_memory_manager->m_backup_info.multipool_capacity,
			delta_state->current_state->m_entity_pool->m_pool_power_of_two,
			&delta_state->previous_state_allocator
		);

		// Copy the current contents
		delta_state->previous_state.CopyOther(delta_state->current_state);
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

		// We need to write the reflection manager types, such that they are stored only once in the file
		return SerializeReflectionManager(data->reflection_manager, write_instrument);
	}

	static bool ReaderHeaderReadFunction(DeltaStateReaderHeaderReadFunctionData* functor_data) {
		// Do a quick pre-check, if the variable length header is missing, fail
		if (functor_data->write_size == 0) {
			return false;
		}

		ReaderData* data = (ReaderData*)functor_data->user_data;

		// Read the reflection manager stored in the file
		return DeserializeReflectionManager(&data->reflection_manager, functor_data->read_instrument, &data->previous_state_allocator, &data->file_field_table);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	static bool WriterDeltaFunction(DeltaStateWriterDeltaFunctionData* function_data) {
		WriterData* data = (WriterData*)function_data->user_data;

		// Determine the change set
		EntityManagerChangeSet change_set = DetermineEntityManagerChangeSet(&data->previous_state, data->current_state, data->reflection_manager, &data->change_set_allocator);

		// Write the change set description first
		// Use the reflection manager for that, it should handle this successfully
		if (Serialize(data->reflection_manager, data->reflection_manager->GetType(STRING(EntityManagerChangeSet)), &change_set, function_data->write_instrument) != ECS_SERIALIZE_OK) {
			return false;
		}

		// Now write the actual components that need to be written. Start with the global ones
		change_set.global_component_changes.ForEach([&](const EntityManagerChangeSet::GlobalComponentChange& change) {
			if (change.type == ECS_CHANGE_SET_ADD || change.type == ECS_CHANGE_SET_UPDATE) {
				const void* global_data = data->current_state->GetGlobalComponent(change.component);
				
			}

			return false;
		});

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

	// The deserialize functor is called with a (const InputSerializationHeader& header, Stream<ECS_INPUT_SERIALIZE_TYPE> accepted_input_types) parameters
	template<typename FunctorData, typename DeserializeFunctor>
	static bool InputDeltaReaderFunctionImpl(FunctorData* function_data, DeserializeFunctor&& deserialize_functor) {
		//DeltaStateReaderData* data = (DeltaStateReaderData*)function_data->user_data;

		//// If the header does not have the same size, fail
		//if (function_data->header.size != sizeof(InputSerializationHeader)) {
		//	return false;
		//}

		//// If nothing was written, we can exit
		//if (function_data->write_size == 0) {
		//	return true;
		//}

		//InputSerializationHeader* header = (InputSerializationHeader*)function_data->header.buffer;

		//ECS_STACK_CAPACITY_STREAM(ECS_INPUT_SERIALIZE_TYPE, valid_types, ECS_INPUT_SERIALIZE_COUNT);
		//for (size_t index = 0; index < ECS_INPUT_SERIALIZE_COUNT; index++) {
		//	valid_types[index] = (ECS_INPUT_SERIALIZE_TYPE)index;
		//}
		//valid_types.size = ECS_INPUT_SERIALIZE_COUNT;

		//size_t initial_offset = function_data->read_instrument->GetOffset();
		//size_t read_size = 0;
		//while (read_size < function_data->write_size && valid_types.size > 0) {
		//	ECS_INPUT_SERIALIZE_TYPE serialized_type = deserialize_functor(*header, valid_types);
		//	if (serialized_type == ECS_INPUT_SERIALIZE_COUNT) {
		//		return false;
		//	}
		//	else {
		//		// Verify that an input of that type is still available
		//		unsigned int valid_type_index = valid_types.Find(serialized_type);
		//		if (valid_type_index == -1) {
		//			// An input is repeated, which means that the data is corrupted.
		//			return false;
		//		}
		//		valid_types.RemoveSwapBack(valid_type_index);

		//		if (serialized_type == ECS_INPUT_SERIALIZE_MOUSE) {
		//			data->previous_mouse = *data->mouse;
		//		}
		//		else if (serialized_type == ECS_INPUT_SERIALIZE_KEYBOARD) {
		//			data->previous_keyboard = *data->keyboard;
		//		}
		//		else {
		//			ECS_ASSERT(false);
		//		}

		//		size_t current_offset = function_data->read_instrument->GetOffset();
		//		read_size += current_offset - initial_offset;
		//		initial_offset = current_offset;
		//	}
		//}

		return true;
	}

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

	unsigned char SerializeEntityManagerDeltaVersion() {
		return VERSION;
	}

	EntityManagerDeltaSerializationHeader GetEntityManagerDeltaSerializeHeader() {
		EntityManagerDeltaSerializationHeader header;
		ZeroOut(&header);
		header.version = VERSION;
		return header;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void SetEntityManagerDeltaWriterInitializeInfo(
		DeltaStateWriterInitializeFunctorInfo& info, 
		const EntityManager* entity_manager, 
		const ReflectionManager* reflection_manager,
		CapacityStream<void>& stack_memory,
		const EntityManagerDeltaWriterInitializeInfoOptions* options
	) {
		EntityManagerDeltaWriterInitializeInfoOptions default_options;
		if (options == nullptr) {
			options = &default_options;
		}

		info.delta_function = WriterDeltaFunction;
		info.entire_function = WriterEntireFunction;
		info.self_contained_extract = nullptr;
		info.user_data_allocator_initialize = WriterInitialize;
		info.user_data_allocator_deallocate = WriterDeallocate;
		info.header_write_function = WriterHeaderWriteFunction;

		EntityManagerDeltaSerializationHeader serialization_header = GetEntityManagerDeltaSerializeHeader();
		info.header = stack_memory.Add(&serialization_header);

		WriterData writer_data;
		writer_data.current_state = entity_manager;
		info.user_data = stack_memory.Add(&writer_data);
	}

	void SetEntityManagerDeltaWriterWorldInitializeInfo(
		DeltaStateWriterInitializeFunctorInfo& info, 
		const World* world, 
		const ReflectionManager* reflection_manager,
		CapacityStream<void>& stack_memory,
		const EntityManagerDeltaWriterInitializeInfoOptions* options
	) {
		// Call the basic function and simply override the user data and the extract function, since that's what is different
		SetEntityManagerDeltaWriterInitializeInfo(info, world->entity_manager, reflection_manager, stack_memory, options);
		info.self_contained_extract = WriterExtractFunction;

		WriterWorldData writer_data;
		writer_data.current_state = world->entity_manager;
		writer_data.world = world;
		info.user_data = stack_memory.Add(&writer_data);
	}

	void SetEntityManagerDeltaReaderInitializeInfo(
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

		ReaderData reader_data;
		ZeroOut(&reader_data);
		reader_data.current_state = entity_manager;
		info.user_data = stack_memory.Add(&reader_data);
	}

	void SetEntityManagerDeltaReaderWorldInitializeInfo(
		DeltaStateReaderInitializeFunctorInfo& info, 
		World* world, 
		const ReflectionManager* reflection_manager,
		CapacityStream<void>& stack_memory
	) {
		// Can forward to the normal initialize
		SetEntityManagerDeltaReaderInitializeInfo(info, world->entity_manager, reflection_manager, stack_memory);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

}