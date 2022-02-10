#include "ecspch.h"
#include "EntityManager.h"
#include "../Utilities/Function.h"
#include "../Utilities/FunctionInterfaces.h"

#define ENTITY_MANAGER_DEFAULT_UNIQUE_COMPONENTS (1 << 10)
#define ENTITY_MANAGER_DEFAULT_SHARED_COMPONENTS (1 << 8)
#define ENTITY_MANAGER_DEFAULT_ARCHETYPE_COUNT (1 << 7)

#define ENTITY_MANAGER_TEMPORARY_ALLOCATOR_MAX_BACKUP (1 << 3)
#define ENTITY_MANAGER_TEMPORARY_ALLOCATOR_CHUNK_SIZE 1 * ECS_MB
#define ENTITY_MANAGER_TEMPORARY_ALLOCATOR_BACKUP_TRACK_CAPACITY (1 << 8)

#define ENTITY_MANAGER_SMALL_ALLOCATOR_SIZE ECS_KB * 256
#define ENTITY_MANAGER_SMALL_ALLOCATOR_CHUNKS ECS_KB * 2
#define ENTITY_MANAGER_SMALL_ALLOCATOR_BACKUP_SIZE ECS_KB * 128

namespace ECSEngine {

	using DeferredCallback = void (*)(EntityManager*, void*, void*);

	struct DeferredCreateEntities {
		unsigned int count;
		ComponentSignature unique_components;
		SharedComponentSignature shared_components;
		const void** data;
		ComponentSignature components_with_data;
	};

	struct DeferredDeleteEntities {
		Stream<Entity> entities;
	};

	struct DeferredAddComponentEntities {
		Stream<Entity> entities;
		ComponentSignature components;
		const void** data;
	};

	struct DeferredRemoveComponentEntities {
		Stream<Entity> entities;
		ComponentSignature components;
	};

	struct DeferredAddSharedComponentEntities {
		Stream<Entity> entities;
		SharedComponentSignature components;
	};

	struct DeferredRemoveSharedComponentEntities {
		Stream<Entity> entities;
		ComponentSignature components;
	};

	struct DeferredCreateArchetype {
		ComponentSignature unique_components;
		ComponentSignature shared_components;
	};

	struct DeferredDestroyArchetype {
		ComponentSignature unique_components;
		ComponentSignature shared_components;
		unsigned short archetype_index;
	};

	struct DeferredCreateArchetypeBase {
		ComponentSignature unique_components;
		SharedComponentSignature shared_components;
		unsigned int archetype_chunk_size;
	};

	struct DeferredDestroyArchetypeBase {
		ComponentSignature unique_components;
		SharedComponentSignature shared_components;
		ushort2 indices;
	};

	struct DeferredCreateComponent {
		Component component;
		unsigned short size;
	};

	struct DeferredDestroyComponent {
		Component component;
	};

	struct DeferredCreateSharedComponent {
		Component component;
		unsigned short size;
	};

	struct DeferredDestroySharedComponent {
		Component component;
	};

	struct DeferredCreateSharedInstance {
		Component component;
		const void* data;
	};

	struct DeferredCreateNamedSharedInstance {
		Component component;
		const void* data;
		ResourceIdentifier identifier;
	};

	struct DeferredDestroySharedInstance {
		Component component;
		const void* data;
	};

	struct DeferredDestroyNamedSharedInstance {
		Component component;
		ResourceIdentifier identifier;
	};
	
	enum DeferredActionType : unsigned char {
		DEFERRED_ENTITY_ADD_COMPONENT,
		DEFERRED_ENTITY_ADD_COMPONENT_SPLATTED,
		DEFERRED_ENTITY_ADD_COMPONENT_SCATTERED,
		DEFERRED_ENTITY_ADD_COMPONENT_BY_ENTITIES,
		DEFERRED_ENTITY_ADD_COMPONENT_CONTIGUOUS,
		DEFERRED_ENTITY_ADD_COMPONENT_BY_ENTITIES_CONTIGUOUS,
		DEFERRED_ENTITY_REMOVE_COMPONENT,
		DEFERRED_CREATE_ENTITIES,
		DEFERRED_CREATE_ENTITIES_SPLATTED,
		DEFERRED_CREATE_ENTITIES_SCATTERED,
		DEFERRED_CREATE_ENTITIES_BY_ENTITIES,
		DEFERRED_CREATE_ENTITIES_BY_ENTITIES_CONTIGUOUS,
		DEFERRED_CREATE_ENTITIES_CONTIGUOUS,
		DEFERRED_DELETE_ENTITIES,
		DEFERRED_ENTITY_ADD_SHARED_COMPONENT,
		DEFERRED_ENTITY_REMOVE_SHARED_COMPONENT,
		DEFERRED_CREATE_ARCHETYPE,
		DEFERRED_DESTROY_ARCHETYPE,
		DEFERRED_CREATE_ARCHETYPE_BASE,
		DEFERRED_DESTROY_ARCHETYPE_BASE,
		DEFERRED_CREATE_COMPONENT,
		DEFERRED_DESTROY_COMPONENT,
		DEFERRED_CREATE_SHARED_COMPONENT,
		DEFERRED_DESTROY_SHARED_COMPONENT,
		DEFERRED_CREATE_SHARED_INSTANCE,
		DEFERRED_CREATE_NAMED_SHARED_INSTANCE,
		DEFERRED_DESTROY_SHARED_INSTANCE,
		DEFERRED_DESTROY_NAMED_SHARED_INSTANCE,
		DEFERRED_CALLBACK_COUNT
	};

#pragma region Helpers

	void WriteCommandStream(EntityManager* manager, DeferredActionParameters parameters, DeferredAction action) {
		if (parameters.command_stream == nullptr) {
			unsigned int index = manager->m_deferred_actions.Add(action);
			ECS_ASSERT(index < manager->m_deferred_actions.capacity);
		}
		else {
			parameters.command_stream->AddSafe(action);
		}
	}

	void GetComponentSizes(unsigned short* sizes, ComponentSignature signature, const ComponentInfo* info) {
		for (size_t index = 0; index < signature.count; index++) {
			sizes[index] = info[signature.indices[index].value].size;
		}
	}

	unsigned short GetComponentsSizePerEntity(const unsigned short* sizes, unsigned char count) {
		unsigned short total = 0;
		for (size_t index = 0; index < count; index++) {
			total += sizes[index];
		}

		return total;
	}

	void GetCombinedSignature(Component* components, ComponentSignature base, ComponentSignature addition) {
		memcpy(components, base.indices, base.count * sizeof(Component));
		memcpy(components + base.count, addition.indices, addition.count * sizeof(Component));
	}

	size_t GetDeferredCallDataAllocationSize(
		EntityManager* manager,
		unsigned int entity_count,
		unsigned short* component_sizes, 
		ComponentSignature components, 
		DeferredActionParameters parameters,
		DeferredActionType type,
		DeferredActionType copy_by_entities_type,
		DeferredActionType copy_scattered_type,
		DeferredActionType copy_splatted,
		DeferredActionType copy_contiguous,
		DeferredActionType copy_entities_contiguous
	) {
		size_t allocation_size = 0;
		if (!parameters.data_is_stable) {
			// Allocate them contiguously for each component each
			if (type == copy_by_entities_type || type == copy_scattered_type || type == copy_entities_contiguous || type == copy_contiguous) {
				GetComponentSizes(component_sizes, components, manager->m_unique_components.buffer);
				allocation_size += sizeof(const void*) * components.count;
				allocation_size += GetComponentsSizePerEntity(component_sizes, components.count) * entity_count;
				// Align the buffers to 8 byte boundaries
				allocation_size += 8 * components.count;
			}
			else if (type == copy_splatted) {
				GetComponentSizes(component_sizes, components, manager->m_unique_components.buffer);
				allocation_size += sizeof(const void*) * components.count;
				allocation_size += GetComponentsSizePerEntity(component_sizes, components.count);
				// Align the buffers to 8 byte boundaries
				allocation_size += 8 * components.count;
			}
		}

		return allocation_size;
	}

	Stream<Entity> GetEntitiesFromActionParameters(Stream<Entity> entities, DeferredActionParameters parameters, uintptr_t& buffer) {
		if (!parameters.entities_are_stable) {
			Stream<Entity> new_entities((void*)buffer, entities.size);
			new_entities.Copy(entities);
			buffer += sizeof(Entity) * entities.size;
			return new_entities;
		}
		else {
			return entities;
		}
	}

	const void** GetDeferredCallData(
		ComponentSignature components,
		const void** component_data,
		unsigned int entity_count,
		const unsigned short* component_sizes,
		DeferredActionParameters parameters,
		DeferredActionType type, 
		DeferredActionType scattered_type, 
		DeferredActionType by_entities_type,
		DeferredActionType splatted_type,
		DeferredActionType copy_contiguous,
		DeferredActionType by_entities_contiguous,
		uintptr_t& buffer
	) {
		const void** data = nullptr;

		if (!parameters.data_is_stable) {
			if (type == scattered_type || type == by_entities_type || type == splatted_type || type == by_entities_contiguous || type == copy_contiguous) {
				data = (const void**)buffer;
				buffer += sizeof(const void*) * components.count;

				uintptr_t buffer_after_pointers = buffer;
				for (size_t component_index = 0; component_index < components.count; component_index) {
					buffer = function::align_pointer(buffer, 8);
					data[component_index] = (void*)buffer;
					buffer += component_sizes[component_index] * entity_count;
				}
				buffer = buffer_after_pointers;

				if (type == by_entities_type) {
					// Pack the data into a contiguous stream for each component type
					for (size_t component_index = 0; component_index < components.count; component_index++) {
						buffer = function::align_pointer(buffer, 8);
						for (size_t entity_index = 0; entity_index < entity_count; entity_index++) {
							memcpy((void*)buffer, component_data[entity_index * components.count + component_index], component_sizes[component_index]);
							buffer += component_sizes[component_index];
						}
					}
				}
				else if (type == by_entities_contiguous) {
					unsigned short* size_until_component = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * components.count);
					size_until_component[0] = 0;
					for (size_t component_index = 1; component_index < components.count; component_index++) {
						size_until_component[component_index] = size_until_component[component_index - 1] + component_sizes[component_index];
					}

					// Pack the data into a contiguous stream for each component type
					for (size_t entity_index = 0; entity_index < entity_count; entity_index++) {
						for (size_t component_index = 0; component_index < components.count; component_index++) {
							memcpy(
								function::OffsetPointer(data[component_index], entity_index * component_sizes[component_index]),
								function::OffsetPointer(component_data[entity_index], size_until_component[component_index]),
								component_sizes[component_index]
							);
						}
					}
				}
				else if (type == copy_contiguous) {
					for (size_t component_index = 0; component_index < components.count; component_index++) {
						memcpy((void*)data[component_index], component_data[component_index], component_sizes[component_index] * entity_count);
					}
				}
				else if (type == scattered_type) {
					// Pack the data into a contiguous stream for each component type
					for (size_t component_index = 0; component_index < components.count; component_index++) {
						buffer = function::align_pointer(buffer, 8);
						for (size_t entity_index = 0; entity_index < entity_count; entity_index++) {
							memcpy((void*)buffer, component_data[component_index * entity_count + entity_index], component_sizes[component_index]);
							buffer += component_sizes[component_index];
						}
					}
				}
				// COMPONENTS_SPLATTED
				else {
					for (size_t component_index = 0; component_index < components.count; component_index++) {
						buffer = function::align_pointer(buffer, 8);
						memcpy((void*)buffer, component_data[component_index], component_sizes[component_index]);
						buffer += component_sizes[component_index];
					}
				}
			}
			else {
				// No data is provided
				data = nullptr;
			}
		}
		else {
			data = component_data;
		}
		
		return data;
	}

	void UpdateEntityInfos(
		const Entity* entities, 
		Stream<EntitySequence> chunk_positions,
		EntityPool* entity_pool,
		unsigned short main_archetype_index,
		unsigned short base_archetype_index
	) {
		unsigned int total_entity_count = 0;
		for (size_t chunk_index = 0; chunk_index < chunk_positions.size; chunk_index++) {
			for (size_t entity_index = 0; entity_index < chunk_positions[chunk_index].entity_count; entity_index++) {
				EntityInfo* entity_info = entity_pool->GetInfoPtr(entities[total_entity_count]);
				entity_info->main_archetype = main_archetype_index;
				entity_info->base_archetype = base_archetype_index;
				entity_info->chunk = chunk_positions[chunk_index].chunk_index;
				entity_info->stream_index = chunk_positions[chunk_index].stream_offset + entity_index;
			}
		}
	}

#pragma region Add component

	struct CommitEntityAddComponentAdditionalData {
		ushort2 archetype_indices;
		CapacityStream<EntitySequence> chunk_positions;
	};

	// Can specifiy get_chunk_positions true in order to fill the additional_data interpreted as a CapacityStream<uint3>*
	// with the chunk positions of the entities
	template<bool get_chunk_positions>
	void CommitEntityAddComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddComponentEntities* data = (DeferredAddComponentEntities*)_data;

		Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];

		ComponentSignature unique_signature = manager->GetEntitySignature(data->entities[0], unique_components);
		SharedComponentSignature shared_signature = manager->GetEntitySharedSignatureStable(data->entities[0]);

		ComponentSignature initial_signature = unique_signature;
		memcpy(unique_components + unique_signature.count, data->components.indices, data->components.count * sizeof(Component));
		unique_signature.count += data->components.count;

		ushort2 archetype_indices = manager->FindOrCreateArchetypeBase(unique_signature, shared_signature);
		ArchetypeBase* base_archetype = manager->GetArchetypeBase(archetype_indices.x, archetype_indices.y);

		EntityInfo entity_info = manager->GetEntityInfo(data->entities[0]);
		ArchetypeBase* old_archetype = manager->GetArchetypeBase(entity_info.main_archetype, entity_info.base_archetype);
		if constexpr (!get_chunk_positions) {
			ECS_STACK_CAPACITY_STREAM(EntitySequence, chunk_positions, ECS_ARCHETYPE_MAX_CHUNKS);
			// If the chunk positions are not needed, it means we must update the entities
			base_archetype->AddEntities(data->entities, chunk_positions);

			// The old component data must be transfered to the new archetype
			base_archetype->CopyFromAnotherArchetype(
				chunk_positions,
				old_archetype,
				data->entities.buffer,
				manager->m_entity_pool,
				initial_signature
			);
			// Remove the entities from the old archetype
			old_archetype->RemoveEntities(data->entities, manager->m_entity_pool);

			UpdateEntityInfos(data->entities.buffer, chunk_positions, manager->m_entity_pool, archetype_indices.x, archetype_indices.y);
		}
		else {
			CommitEntityAddComponentAdditionalData* additional_data = (CommitEntityAddComponentAdditionalData*)_additional_data;
			base_archetype->AddEntities(data->entities, additional_data->chunk_positions);
			// Let the caller update the entity infos
			additional_data->archetype_indices = archetype_indices;
			base_archetype->CopyFromAnotherArchetype(
				additional_data->chunk_positions,
				old_archetype,
				data->entities.buffer,
				manager->m_entity_pool,
				initial_signature
			);
		}
	}

	template<typename CopyFunction>
	void CommitEntityAddComponentWithData(EntityManager* manager, void* _data, CopyFunction&& copy_function) {
		ECS_STACK_CAPACITY_STREAM(EntitySequence, chunk_positions, ECS_ARCHETYPE_MAX_CHUNKS);
		CommitEntityAddComponentAdditionalData additional_data = { {}, chunk_positions };
		CommitEntityAddComponent<true>(manager, _data, &additional_data);

		// The entity addition was commited by the previous call
		DeferredAddComponentEntities* data = (DeferredAddComponentEntities*)_data;
		// We need to copy the data and to update the entity infos
		ArchetypeBase* base_archetype = manager->GetArchetypeBase(additional_data.archetype_indices.x, additional_data.archetype_indices.y);

		copy_function(base_archetype, chunk_positions, data->data, data->components);

		// Remove the entities from the old archetype
		EntityInfo entity_info = manager->GetEntityInfo(data->entities[0]);
		ArchetypeBase* old_archetype = manager->GetArchetypeBase(entity_info.main_archetype, entity_info.base_archetype);
		old_archetype->RemoveEntities(data->entities, manager->m_entity_pool);

		UpdateEntityInfos(data->entities.buffer, additional_data.chunk_positions, manager->m_entity_pool, additional_data.archetype_indices.x, additional_data.archetype_indices.y);
	}

	void CommitEntityAddComponentSplatted(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components) {
			base_archetype->CopySplatComponents(chunk_positions, data, components);
		});
	}

	void CommitEntityAddComponentByEntities(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntity(chunk_positions, data, components);
		});
	}

	void CommitEntityAddComponentScattered(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponents(chunk_positions, data, components);
		});
	}

	void CommitEntityAddComponentContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponentsContiguous(chunk_positions, data, components);
		});
	}

	void CommitEntityAddComponentByEntitiesContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntityContiguous(chunk_positions, data, components);
		});
	}

	void RegisterEntityAddComponent(
		EntityManager* manager,
		Stream<Entity> entities,
		ComponentSignature components,
		const void** component_data,
		DeferredActionType type,
		DeferredActionParameters parameters
	) {
		unsigned short* component_sizes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * components.count);

		size_t allocation_size = sizeof(DeferredAddComponentEntities) + sizeof(Component) * components.count;
		allocation_size += parameters.entities_are_stable ? 0 : entities.MemoryOf(entities.size);

		allocation_size += GetDeferredCallDataAllocationSize(
			manager,
			entities.size,
			component_sizes,
			components,
			parameters,
			type,
			DEFERRED_ENTITY_ADD_COMPONENT_BY_ENTITIES,
			DEFERRED_ENTITY_ADD_COMPONENT_SCATTERED,
			DEFERRED_ENTITY_ADD_COMPONENT_SPLATTED,
			DEFERRED_ENTITY_ADD_COMPONENT_CONTIGUOUS,
			DEFERRED_ENTITY_ADD_COMPONENT_BY_ENTITIES_CONTIGUOUS
		);

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredAddComponentEntities* data = (DeferredAddComponentEntities*)allocation;
		buffer += sizeof(DeferredAddComponentEntities);

		data->components.indices = (Component*)buffer;
		data->components.count = components.count;
		memcpy(data->components.indices, components.indices, sizeof(Component) * components.count);
		buffer += sizeof(Component) * components.count;

		data->entities = GetEntitiesFromActionParameters(entities, parameters, buffer);

		data->data = GetDeferredCallData(
			components,
			component_data,
			entities.size,
			component_sizes,
			parameters,
			type,
			DEFERRED_ENTITY_ADD_COMPONENT_SCATTERED,
			DEFERRED_ENTITY_ADD_COMPONENT_BY_ENTITIES,
			DEFERRED_ENTITY_ADD_COMPONENT_SPLATTED,
			DEFERRED_ENTITY_ADD_COMPONENT_CONTIGUOUS,
			DEFERRED_ENTITY_ADD_COMPONENT_BY_ENTITIES_CONTIGUOUS,
			buffer
		);

		WriteCommandStream(manager, parameters, { allocation, type });
	}

#pragma endregion

#pragma region Remove Components

	void CommitEntityRemoveComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredRemoveComponentEntities* data = (DeferredRemoveComponentEntities*)_data;

		Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];

		ComponentSignature unique_signature = manager->GetEntitySignature(data->entities[0], unique_components);
		SharedComponentSignature shared_signature = manager->GetEntitySharedSignatureStable(data->entities[0]);

		ComponentSignature initial_signature = unique_signature;
		// Determine which is the final signature
		for (size_t index = 0; index < data->components.count; index++) {
			size_t subindex = 0;
			for (; subindex < unique_signature.count; subindex++) {
				if (unique_signature.indices[subindex] == data->components.indices[index]) {
					unique_signature.count--;
					unique_signature.indices[subindex] = unique_signature.indices[unique_signature.count];
					// Exit the loop
					subindex = -2;
				}
			}
			// If the component was not found, fail
			ECS_ASSERT(subindex != -1);
		}

		ushort2 archetype_indices = manager->FindOrCreateArchetypeBase(unique_signature, shared_signature);
		ArchetypeBase* base_archetype = manager->GetArchetypeBase(archetype_indices.x, archetype_indices.y);

		EntityInfo entity_info = manager->GetEntityInfo(data->entities[0]);
		ArchetypeBase* old_archetype = manager->GetArchetypeBase(entity_info.main_archetype, entity_info.base_archetype);

		ECS_STACK_CAPACITY_STREAM(EntitySequence, chunk_positions, ECS_ARCHETYPE_MAX_CHUNKS);
		base_archetype->AddEntities(data->entities, chunk_positions);

		// The old component data must be transfered to the new archetype
		base_archetype->CopyFromAnotherArchetype(
			chunk_positions,
			old_archetype,
			data->entities.buffer,
			manager->m_entity_pool,
			initial_signature
		);
		// Remove the entities from the old archetype
		old_archetype->RemoveEntities(data->entities, manager->m_entity_pool);

		UpdateEntityInfos(data->entities.buffer, chunk_positions, manager->m_entity_pool, archetype_indices.x, archetype_indices.y);
	}

	void RegisterEntityRemoveComponents(
		EntityManager* manager,
		Stream<Entity> entities,
		ComponentSignature components,
		DeferredActionParameters parameters
	) {
		size_t allocation_size = sizeof(DeferredRemoveComponentEntities) + sizeof(Component) * components.count;
		allocation_size += parameters.entities_are_stable ? 0 : entities.MemoryOf(entities.size);

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredRemoveComponentEntities* data = (DeferredRemoveComponentEntities*)allocation;
		buffer += sizeof(DeferredRemoveComponentEntities);

		data->components.indices = (Component*)buffer;
		data->components.count = components.count;
		memcpy(data->components.indices, components.indices, sizeof(Component) * components.count);
		buffer += sizeof(Component) * components.count;

		WriteCommandStream(manager, parameters, { allocation, DEFERRED_ENTITY_REMOVE_COMPONENT });
	}

#pragma endregion

#pragma region Create Entities

	struct CommitCreateEntitiesAdditionalData {
		ushort2 archetype_indices;
		CapacityStream<EntitySequence> chunk_positions;
		Stream<Entity> entities;
	};

	enum CreateEntitiesTemplateType {
		CREATE_ENTITIES_TEMPLATE_NO_ADDITIONAL_DATA,
		CREATE_ENTITIES_TEMPLATE_CHUNK_POSITIONS,
		CREATE_ENTITIES_TEMPLATE_GET_ENTITIES,
		CREATE_ENTITIES_TEMPLATE_CHUNKS_AND_ENTITIES
	};

	// Can specifiy get_chunk_positions true in order to fill the additional_data interpreted as a CapacityStream<uint3>*
	// with the chunk positions of the entities
	template<CreateEntitiesTemplateType type>
	void CommitCreateEntities(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateEntities* data = (DeferredCreateEntities*)_data;
		CommitCreateEntitiesAdditionalData* additional_data = (CommitCreateEntitiesAdditionalData*)_additional_data;

		ushort2 archetype_indices = manager->FindOrCreateArchetypeBase(data->unique_components, data->shared_components);
		ArchetypeBase* base_archetype = manager->GetArchetypeBase(archetype_indices.x, archetype_indices.y);

		const size_t MAX_ENTITY_INFOS_ON_STACK = ECS_KB * 4;

		CapacityStream<EntitySequence>* chunk_positions;
		Entity* entities = nullptr;

		if constexpr (type != CREATE_ENTITIES_TEMPLATE_GET_ENTITIES && type != CREATE_ENTITIES_TEMPLATE_CHUNKS_AND_ENTITIES) {
			// Fill in a buffer with the corresponding entity infos
			if (data->count < MAX_ENTITY_INFOS_ON_STACK) {
				entities = (Entity*)ECS_STACK_ALLOC(sizeof(Entity) * data->count);
			}
			else {
				entities = (Entity*)ECS_MALLOCA(sizeof(Entity) * data->count);
			}
		}
		else {
			entities = additional_data->entities.buffer;
		}

		// Reserve the space for these entities
		if constexpr (type != CREATE_ENTITIES_TEMPLATE_CHUNK_POSITIONS && type != CREATE_ENTITIES_TEMPLATE_CHUNKS_AND_ENTITIES) {
			ECS_STACK_CAPACITY_STREAM(EntitySequence, _chunk_positions, ECS_ARCHETYPE_MAX_CHUNKS);
			chunk_positions = &_chunk_positions;
		}
		else {			
			additional_data->archetype_indices = archetype_indices;
			chunk_positions = &additional_data->chunk_positions;
		}

		// If the chunk positions are not needed, it means we must update the entities
		base_archetype->Reserve(data->count, *chunk_positions);

		// Create the entities from the entity pool - their infos will already be set
		manager->m_entity_pool->AllocateEx({ entities, data->count }, archetype_indices, *chunk_positions);
		
		// Copy the entities into the archetype entity reference
		base_archetype->SetEntities(entities, *chunk_positions);

		if constexpr (type != CREATE_ENTITIES_TEMPLATE_GET_ENTITIES && type != CREATE_ENTITIES_TEMPLATE_CHUNKS_AND_ENTITIES) {
			if (data->count >= MAX_ENTITY_INFOS_ON_STACK) {
				ECS_FREEA(entities);
			}
		}
	}

	template<typename CopyFunction>
	void CommitCreateEntitiesWithData(EntityManager* manager, void* _data, void* _additional_data, CopyFunction&& copy_function) {
		ECS_STACK_CAPACITY_STREAM(EntitySequence, chunk_positions, ECS_ARCHETYPE_MAX_CHUNKS);
		CommitCreateEntitiesAdditionalData additional_data = { {}, chunk_positions };
		if (_additional_data != nullptr) {
			CommitCreateEntitiesAdditionalData* __additional_data = (CommitCreateEntitiesAdditionalData*)_additional_data;
			additional_data.entities = __additional_data->entities;
			CommitCreateEntities<CREATE_ENTITIES_TEMPLATE_CHUNKS_AND_ENTITIES>(manager, _data, &additional_data);
		}
		else {
			CommitCreateEntities<CREATE_ENTITIES_TEMPLATE_CHUNK_POSITIONS>(manager, _data, &additional_data);
		}

		// The entity addition was commit by the previous call
		DeferredCreateEntities* data = (DeferredCreateEntities*)_data;
		// We need only to copy the data
		ArchetypeBase* base_archetype = manager->GetArchetypeBase(additional_data.archetype_indices.x, additional_data.archetype_indices.y);
		copy_function(base_archetype, chunk_positions, data->data, data->components_with_data);
	}

	void CommitCreateEntitiesDataSplatted(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components) {
			base_archetype->CopySplatComponents(chunk_positions, data, components);
		});
	}

	void CommitCreateEntitiesDataByEntities(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntity(chunk_positions, data, components);
		});
	}

	void CommitCreateEntitiesDataScattered(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponents(chunk_positions, data, components);
		});
	}

	void CommitCreateEntitiesDataContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponentsContiguous(chunk_positions, data, components);
		});
	}

	void CommitCreateEntitiesDataByEntitiesContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, Stream<EntitySequence> chunk_positions, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntityContiguous(chunk_positions, data, components);
		});
	}

	void RegisterCreateEntities(
		EntityManager* manager,
		unsigned int entity_count,
		ComponentSignature unique_components,
		SharedComponentSignature shared_components,
		ComponentSignature components_to_copy,
		const void** component_data,
		DeferredActionType type,
		DeferredActionParameters parameters
	) {
		unsigned short* component_sizes = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * unique_components.count);

		size_t allocation_size = sizeof(DeferredCreateEntities) + sizeof(Component) * (unique_components.count + components_to_copy.count)
			+ (sizeof(Component) + sizeof(SharedInstance)) * shared_components.count;

		allocation_size += GetDeferredCallDataAllocationSize(
			manager,
			entity_count,
			component_sizes,
			unique_components,
			parameters,
			type,
			DEFERRED_CREATE_ENTITIES_BY_ENTITIES,
			DEFERRED_CREATE_ENTITIES_SCATTERED,
			DEFERRED_CREATE_ENTITIES_SPLATTED,
			DEFERRED_CREATE_ENTITIES_CONTIGUOUS,
			DEFERRED_CREATE_ENTITIES_BY_ENTITIES_CONTIGUOUS
		);

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredCreateEntities* data = (DeferredCreateEntities*)allocation;
		buffer += sizeof(*data);

		data->unique_components.indices = (Component*)buffer;
		data->unique_components.count = unique_components.count;
		memcpy(data->unique_components.indices, unique_components.indices, sizeof(Component) * unique_components.count);
		buffer += sizeof(Component) * unique_components.count;

		data->components_with_data.indices = (Component*)buffer;
		data->components_with_data.count = components_to_copy.count;
		memcpy(data->components_with_data.indices, components_to_copy.indices, sizeof(Component) * components_to_copy.count);
		buffer += sizeof(Component) * components_to_copy.count;

		data->shared_components.indices = (Component*)buffer;
		data->shared_components.count = shared_components.count;
		memcpy(data->shared_components.indices, shared_components.indices, sizeof(Component) * shared_components.count);
		buffer += sizeof(Component) * shared_components.count;

		data->shared_components.instances = (SharedInstance*)buffer;
		memcpy(data->shared_components.instances, shared_components.instances, sizeof(SharedInstance) * shared_components.count);
		buffer += sizeof(SharedInstance) * shared_components.count;

		data->data = GetDeferredCallData(
			unique_components,
			component_data,
			entity_count,
			component_sizes,
			parameters,
			type,
			DEFERRED_CREATE_ENTITIES_SCATTERED,
			DEFERRED_CREATE_ENTITIES_BY_ENTITIES,
			DEFERRED_CREATE_ENTITIES_SPLATTED,
			DEFERRED_CREATE_ENTITIES_CONTIGUOUS,
			DEFERRED_CREATE_ENTITIES_BY_ENTITIES_CONTIGUOUS,
			buffer
		);

		WriteCommandStream(manager, parameters, { allocation, type });
	}

#pragma endregion

#pragma region Delete Entities

	void CommitDeleteEntities(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDeleteEntities* data = (DeferredDeleteEntities*)_data;
		
		EntityInfo info = manager->GetEntityInfo(data->entities[0]);
		ArchetypeBase* base_archetype = manager->GetArchetypeBase(info.main_archetype, info.base_archetype);
		// Remove the entities from the archetype
		base_archetype->RemoveEntities(data->entities, manager->m_entity_pool);

		manager->m_entity_pool->Deallocate(data->entities);
	}

	void RegisterDeleteEntities(EntityManager* manager, Stream<Entity> entities, DeferredActionParameters parameters) {
		size_t allocation_size = sizeof(DeferredDeleteEntities);
		allocation_size += parameters.entities_are_stable ? 0 : entities.MemoryOf(entities.size);

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredDeleteEntities* data = (DeferredDeleteEntities*)allocation;
		buffer += sizeof(*data);

		WriteCommandStream(manager, parameters, { allocation, DEFERRED_DELETE_ENTITIES });
	}

#pragma endregion

#pragma region Add Shared Component

	void CommitEntityAddSharedComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddSharedComponentEntities* data = (DeferredAddSharedComponentEntities*)_data;

		EntityInfo info = manager->GetEntityInfo(data->entities[0]);
		Archetype* old_archetype = manager->GetArchetype(info.main_archetype);
		ArchetypeBase* old_base_archetype = old_archetype->GetArchetypeBase(info.base_archetype);

		Component _shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		SharedInstance _shared_instances[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

		ComponentSignature unique_signature = old_archetype->GetUniqueSignature();
		ComponentSignature _shared_signature = old_archetype->GetSharedSignature();
		SharedComponentSignature shared_signature(_shared_components, _shared_instances, 0);
		memcpy(shared_signature.indices, _shared_signature.indices, sizeof(Component) * _shared_signature.count);
		const SharedInstance* current_instances = old_archetype->GetArchetypeBaseInstances(info.base_archetype);
		memcpy(shared_signature.instances, current_instances, sizeof(SharedInstance) * _shared_signature.count);
		shared_signature.count = _shared_signature.count;

		memcpy(shared_signature.indices + shared_signature.count, data->components.indices, sizeof(Component) * data->components.count);
		memcpy(shared_signature.instances + shared_signature.count, data->components.instances, sizeof(SharedInstance) * data->components.count);
		shared_signature.count += data->components.count;

		// Remove the entities from the archetype
		old_base_archetype->RemoveEntities(data->entities, manager->m_entity_pool);

		ushort2 new_archetype_indices = manager->FindOrCreateArchetypeBase(unique_signature, shared_signature);
		ArchetypeBase* new_archetype = manager->GetArchetypeBase(new_archetype_indices.x, new_archetype_indices.y);

		ECS_STACK_CAPACITY_STREAM(EntitySequence, chunk_positions, ECS_ARCHETYPE_MAX_CHUNKS);
		new_archetype->AddEntities(data->entities, chunk_positions);

		UpdateEntityInfos(data->entities.buffer, chunk_positions, manager->m_entity_pool, new_archetype_indices.x, new_archetype_indices.y);
	}

	void RegisterEntityAddSharedComponent(EntityManager* manager, Stream<Entity> entities, SharedComponentSignature components, DeferredActionParameters parameters) {
		size_t allocation_size = sizeof(DeferredAddSharedComponentEntities) + (sizeof(Component) + sizeof(SharedInstance)) * components.count;
		allocation_size += parameters.entities_are_stable ? 0 : entities.MemoryOf(entities.size);

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredAddSharedComponentEntities* data = (DeferredAddSharedComponentEntities*)allocation;
		buffer += sizeof(*data);

		data->components.count = components.count;
		data->components.indices = (Component*)buffer;
		memcpy(data->components.indices, components.indices, sizeof(Component) * data->components.count);
		buffer += sizeof(Component) * data->components.count;

		data->components.instances = (SharedInstance*)buffer;
		memcpy(data->components.instances, components.instances, sizeof(SharedInstance) * data->components.count);
		buffer += sizeof(SharedInstance) * data->components.count;

		buffer = function::align_pointer(buffer, alignof(Entity));
		data->entities = GetEntitiesFromActionParameters(entities, parameters, buffer);

		WriteCommandStream(manager, parameters, { allocation, DEFERRED_ENTITY_ADD_SHARED_COMPONENT });
	}

#pragma endregion

#pragma region Remove Shared Component

	void CommitEntityRemoveSharedComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredRemoveSharedComponentEntities* data = (DeferredRemoveSharedComponentEntities*)_data;

		EntityInfo info = manager->GetEntityInfo(data->entities[0]);
		Archetype* old_archetype = manager->GetArchetype(info.main_archetype);
		ArchetypeBase* old_base_archetype = old_archetype->GetArchetypeBase(info.base_archetype);

		Component _shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		SharedInstance _shared_instances[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

		ComponentSignature unique_signature = old_archetype->GetUniqueSignature();
		ComponentSignature _shared_signature = old_archetype->GetSharedSignature();
		SharedComponentSignature shared_signature(_shared_components, _shared_instances, 0);
		memcpy(shared_signature.indices, _shared_signature.indices, sizeof(Component) * _shared_signature.count);
		const SharedInstance* current_instances = old_archetype->GetArchetypeBaseInstances(info.base_archetype);
		memcpy(shared_signature.instances, current_instances, sizeof(SharedInstance) * _shared_signature.count);
		shared_signature.count = _shared_signature.count;

		for (size_t index = 0; index < data->components.count; index++) {
			size_t subindex = 0;
			for (; subindex < shared_signature.count; subindex++) {
				if (shared_signature.indices[subindex] == data->components.indices[index]) {
					shared_signature.count--;
					shared_signature.indices[subindex] = shared_signature.indices[shared_signature.count];
					shared_signature.instances[subindex] = shared_signature.instances[shared_signature.count];

					// Exit the loop
					subindex = -2;
				}
			}
			// Fail if the component doesn't exist
			ECS_ASSERT(subindex != -1);
		}

		// Remove the entities from the archetype
		old_base_archetype->RemoveEntities(data->entities, manager->m_entity_pool);

		ushort2 new_archetype_indices = manager->FindOrCreateArchetypeBase(unique_signature, shared_signature);
		ArchetypeBase* new_archetype = manager->GetArchetypeBase(new_archetype_indices.x, new_archetype_indices.y);

		ECS_STACK_CAPACITY_STREAM(EntitySequence, chunk_positions, ECS_ARCHETYPE_MAX_CHUNKS);
		new_archetype->AddEntities(data->entities, chunk_positions);

		UpdateEntityInfos(data->entities.buffer, chunk_positions, manager->m_entity_pool, new_archetype_indices.x, new_archetype_indices.y);
	}

	void RegisterEntityRemoveSharedComponent(EntityManager* manager, Stream<Entity> entities, ComponentSignature components, DeferredActionParameters parameters) {
		size_t allocation_size = sizeof(DeferredRemoveSharedComponentEntities) + sizeof(Component) * components.count;
		allocation_size += parameters.entities_are_stable ? 0 : entities.MemoryOf(entities.size);

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredRemoveSharedComponentEntities* data = (DeferredRemoveSharedComponentEntities*)allocation;
		buffer += sizeof(*data);

		data->components.count = components.count;
		data->components.indices = (Component*)buffer;
		memcpy(data->components.indices, components.indices, sizeof(Component) * data->components.count);
		buffer += sizeof(Component) * data->components.count;

		buffer = function::align_pointer(buffer, alignof(Entity));
		data->entities = GetEntitiesFromActionParameters(entities, parameters, buffer);

		WriteCommandStream(manager, parameters, { allocation, DEFERRED_ENTITY_REMOVE_SHARED_COMPONENT });
	}

#pragma endregion

#pragma region Create Archetype

	void CommitCreateArchetype(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateArchetype* data = (DeferredCreateArchetype*)_data;

		unsigned short archetype_index = manager->m_archetypes.Add(
			Archetype(
				&manager->m_small_memory_manager,
				manager->m_memory_manager,
				manager->m_unique_components.buffer,
				data->unique_components,
				data->shared_components
			)
		);
		ECS_ASSERT(archetype_index < ECS_MAX_MAIN_ARCHETYPES);

		if (_additional_data != nullptr) {
			unsigned short* additional_data = (unsigned short*)_additional_data;
			*additional_data = archetype_index;
		}
	}

	void RegisterCreateArchetype(EntityManager* manager, ComponentSignature unique_components, ComponentSignature shared_components, EntityManagerCommandStream* command_stream) {
		size_t allocation_size = sizeof(DeferredCreateArchetype) + sizeof(Component) * unique_components.count + sizeof(Component) * shared_components.count;

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredCreateArchetype* data = (DeferredCreateArchetype*)allocation;
		buffer += sizeof(*data);

		data->unique_components.count = unique_components.count;
		data->unique_components.indices = (Component*)buffer;
		memcpy(data->unique_components.indices, unique_components.indices, sizeof(Component) * data->unique_components.count);
		buffer += sizeof(Component) * data->unique_components.count;

		data->shared_components.count = shared_components.count;
		data->shared_components.indices = (Component*)buffer;
		memcpy(data->shared_components.indices, shared_components.indices, sizeof(Component) * data->shared_components.count);
		buffer += sizeof(Component) * data->shared_components.count;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_CREATE_ARCHETYPE });
	}

#pragma endregion

#pragma region Destroy Archetype

	template<bool use_component_search>
	void CommitDestroyArchetype(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyArchetype* data = (DeferredDestroyArchetype*)_data;

		unsigned short archetype_index = data->archetype_index;
		if constexpr (use_component_search) {
			archetype_index = manager->FindArchetype({ data->unique_components, data->shared_components });
		}
		// Fail if it doesn't exist
		ECS_ASSERT(archetype_index != -1);

		Archetype* archetype = manager->GetArchetype(archetype_index);
		EntityPool* pool = manager->m_entity_pool;
		// Do a search and eliminate every entity inside the base archetypes
		for (unsigned short index = 0; index < archetype->GetArchetypeBaseCount(); index++) {
			ArchetypeBase* base = archetype->GetArchetypeBase(index);
			for (size_t chunk_index = 0; chunk_index < base->m_chunks.size; chunk_index++) {
				for (size_t entity_index = 0; entity_index < base->m_chunks[chunk_index].size; entity_index++) {
					pool->Deallocate(base->m_chunks[chunk_index].entities[entity_index]);
				}
			}
		}

		// Now the archetype can be safely deallocated
		archetype->Deallocate();

		manager->m_archetypes.RemoveSwapBack(archetype_index);

		// Update infos for the entities from the archetype that got swaped in its place
		// unless it is the last one
		if (archetype_index != manager->m_archetypes.size) {
			for (size_t base_index = 0; base_index < archetype->GetArchetypeBaseCount(); base_index++) {
				const ArchetypeBase* base = archetype->GetArchetypeBase(base_index);
				for (size_t chunk_index = 0; chunk_index < base->m_chunks.size; chunk_index++) {
					for (size_t entity_index = 0; entity_index < base->m_chunks[chunk_index].size; entity_index++) {
						EntityInfo* entity_info = manager->m_entity_pool->GetInfoPtr(base->m_chunks[chunk_index].entities[entity_index]);
						entity_info->main_archetype = archetype_index;
					}
				}
			}
		}

		// TODO: is it worth trimming? The memory saving is small and might induce fragmentation
		manager->m_archetypes.TrimThreshold(10);
	}

	void RegisterDestroyArchetype(EntityManager* manager, ComponentSignature unique_components, ComponentSignature shared_components, EntityManagerCommandStream* command_stream) {
		size_t allocation_size = sizeof(DeferredDestroyArchetype) + sizeof(Component) * unique_components.count + sizeof(Component) * shared_components.count;

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredDestroyArchetype* data = (DeferredDestroyArchetype*)allocation;
		buffer += sizeof(*data);

		data->unique_components.count = unique_components.count;
		data->unique_components.indices = (Component*)buffer;
		memcpy(data->unique_components.indices, unique_components.indices, sizeof(Component) * data->unique_components.count);
		buffer += sizeof(Component) * data->unique_components.count;

		data->shared_components.count = shared_components.count;
		data->shared_components.indices = (Component*)buffer;
		memcpy(data->shared_components.indices, shared_components.indices, sizeof(Component) * data->shared_components.count);
		buffer += sizeof(Component) * data->shared_components.count;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_DESTROY_ARCHETYPE });
	}

#pragma endregion

#pragma region Create Archetype Base

	void CommitCreateArchetypeBase(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateArchetypeBase* data = (DeferredCreateArchetypeBase*)_data;

		VectorComponentSignature vector_shared;
		vector_shared.InitializeSharedComponent(data->shared_components);
		unsigned short main_archetype_index = manager->FindArchetype({ data->unique_components, vector_shared });
		ECS_ASSERT(main_archetype_index != -1);

		Archetype* archetype = manager->GetArchetype(main_archetype_index);

		unsigned short base_index = archetype->CreateBaseArchetype(data->shared_components, data->archetype_chunk_size);
		ECS_ASSERT(base_index < ECS_ARCHETYPE_MAX_BASE_ARCHETYPES);

		if (_additional_data != nullptr) {
			ushort2* additional_data = (ushort2*)_additional_data;
			*additional_data = { main_archetype_index, base_index };
		}
	}

	void RegisterCreateArchetypeBase(
		EntityManager* manager, 
		ComponentSignature unique_components,
		SharedComponentSignature shared_components, 
		unsigned int archetype_chunk_size,
		EntityManagerCommandStream* command_stream
	) {
		size_t allocation_size = sizeof(DeferredCreateArchetypeBase) + sizeof(Component) * unique_components.count + 
			(sizeof(Component) + sizeof(SharedInstance)) * shared_components.count;

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredCreateArchetypeBase* data = (DeferredCreateArchetypeBase*)allocation;
		buffer += sizeof(*data);

		data->archetype_chunk_size = archetype_chunk_size;
		data->unique_components.count = unique_components.count;
		data->unique_components.indices = (Component*)buffer;
		memcpy(data->unique_components.indices, unique_components.indices, sizeof(Component) * data->unique_components.count);
		buffer += sizeof(Component) * data->unique_components.count;

		data->shared_components.count = shared_components.count;
		data->shared_components.indices = (Component*)buffer;
		memcpy(data->shared_components.indices, shared_components.indices, sizeof(Component) * data->shared_components.count);
		buffer += sizeof(Component) * data->shared_components.count;

		data->shared_components.instances = (SharedInstance*)buffer;
		memcpy(data->shared_components.instances, shared_components.instances, sizeof(SharedInstance) * data->shared_components.count);
		buffer += sizeof(SharedInstance) * data->shared_components.count;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_CREATE_ARCHETYPE_BASE });
	}

#pragma endregion

#pragma region Destroy Archetype Base

	template<bool use_component_search>
	void CommitDestroyArchetypeBase(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyArchetypeBase* data = (DeferredDestroyArchetypeBase*)_data;

		Archetype* archetype = nullptr;
		unsigned short base_index = 0;

		if constexpr (use_component_search) {
			VectorComponentSignature vector_shared;
			vector_shared.InitializeSharedComponent(data->shared_components);
			VectorComponentSignature vector_instances;
			vector_instances.InitializeSharedInstances(data->shared_components);
			Archetype* archetype = manager->FindArchetypePtr({ data->unique_components, vector_shared });
			ECS_ASSERT(archetype != nullptr);

			unsigned short base_index = archetype->FindArchetypeBaseIndex(vector_shared, vector_instances);
			ECS_ASSERT(base_index != -1);
		}
		else {
			archetype = manager->GetArchetype(data->indices.x);
			base_index = data->indices.y;
		}

		ArchetypeBase* base = archetype->GetArchetypeBase(base_index);
		EntityPool* pool = manager->m_entity_pool;
		// Do a search and eliminate every entity inside the base archetypes
		for (size_t chunk_index = 0; chunk_index < base->m_chunks.size; chunk_index++) {
			for (size_t entity_index = 0; entity_index < base->m_chunks[chunk_index].size; entity_index++) {
				pool->Deallocate(base->m_chunks[chunk_index].entities[entity_index]);
			}
		}

		archetype->DestroyBase(base_index, manager->m_entity_pool);
	}

	void RegisterDestroyArchetypeBase(
		EntityManager* manager,
		ComponentSignature unique_components,
		SharedComponentSignature shared_components,
		EntityManagerCommandStream* command_stream
	) {
		size_t allocation_size = sizeof(DeferredDestroyArchetypeBase) + sizeof(Component) * unique_components.count +
			(sizeof(Component) + sizeof(SharedInstance)) * shared_components.count;

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredDestroyArchetypeBase* data = (DeferredDestroyArchetypeBase*)allocation;
		buffer += sizeof(*data);

		data->unique_components.count = unique_components.count;
		data->unique_components.indices = (Component*)buffer;
		memcpy(data->unique_components.indices, unique_components.indices, sizeof(Component) * data->unique_components.count);
		buffer += sizeof(Component) * data->unique_components.count;

		data->shared_components.count = shared_components.count;
		data->shared_components.indices = (Component*)buffer;
		memcpy(data->shared_components.indices, shared_components.indices, sizeof(Component) * data->shared_components.count);
		buffer += sizeof(Component) * data->shared_components.count;

		data->shared_components.instances = (SharedInstance*)buffer;
		memcpy(data->shared_components.instances, shared_components.instances, sizeof(SharedInstance) * data->shared_components.count);
		buffer += sizeof(SharedInstance) * data->shared_components.count;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_DESTROY_ARCHETYPE_BASE });
	}

#pragma endregion

#pragma region Create Component

	void CommitCreateComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateComponent* data = (DeferredCreateComponent*)_data;

		if (manager->m_unique_components.size <= data->component.value) {
			size_t old_capacity = manager->m_unique_components.size;
			void* allocation = manager->m_small_memory_manager.Allocate(sizeof(SharedComponentInfo) * data->component.value);
			manager->m_unique_components.CopyTo(allocation);
			manager->m_small_memory_manager.Deallocate(manager->m_unique_components.buffer);

			manager->m_unique_components.InitializeFromBuffer(allocation, data->component.value);
			for (size_t index = old_capacity; index < manager->m_unique_components.size; index++) {
				manager->m_unique_components[index].size = -1;
			}
		}
		// If the size is different from -1, it means there is no component actually allocated to this slot
		ECS_ASSERT(manager->m_unique_components[data->component.value].size == -1);
		manager->m_unique_components[data->component.value].size = data->size;
	}

	void RegisterCreateComponent(
		EntityManager* manager,
		Component component,
		unsigned short size,
		EntityManagerCommandStream* command_stream
	) {
		size_t allocation_size = sizeof(DeferredCreateComponent);

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		DeferredCreateComponent* data = (DeferredCreateComponent*)allocation;
		data->component = component;
		data->size = size;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_CREATE_COMPONENT });
	}

#pragma endregion

#pragma region Destroy Component

	void CommitDestroyComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyComponent* data = (DeferredDestroyComponent*)_data;

		ECS_ASSERT(data->component.value < manager->m_unique_components.size);
		// -1 Signals it's empty - a component was never associated to this slot
		ECS_ASSERT(manager->m_shared_components[data->component.value].info.size != -1);

		manager->m_unique_components[data->component.value].size = -1;
	}

	void RegisterDestroyComponent(
		EntityManager* manager,
		Component component,
		unsigned short size,
		EntityManagerCommandStream* command_stream
	) {
		size_t allocation_size = sizeof(DeferredDestroySharedComponent);

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		DeferredDestroySharedComponent* data = (DeferredDestroySharedComponent*)allocation;
		data->component = component;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_DESTROY_COMPONENT });
	}

#pragma endregion

#pragma region Create shared component

	void CommitCreateSharedComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateSharedComponent* data = (DeferredCreateSharedComponent*)_data;

		if (manager->m_shared_components.size <= data->component.value) {
			size_t old_capacity = manager->m_shared_components.size;
			void* allocation = manager->m_small_memory_manager.Allocate(sizeof(SharedComponentInfo) * data->component.value);
			manager->m_shared_components.CopyTo(allocation);
			manager->m_small_memory_manager.Deallocate(manager->m_shared_components.buffer);

			manager->m_shared_components.InitializeFromBuffer(allocation, data->component.value);
			for (size_t index = old_capacity; index < manager->m_shared_components.size; index++) {
				manager->m_shared_components[index].info.size = -1;
				manager->m_shared_components[index].instances.size = 0;
				manager->m_shared_components[index].named_instances.InitializeFromBuffer(nullptr, 0);
			}
		}
		// If the size is different from -1, it means there is no component actually allocated to this slot
		ECS_ASSERT(manager->m_shared_components[data->component.value].info.size == -1);
		manager->m_shared_components[data->component.value].info.size = data->size;
		manager->m_shared_components[data->component.value].instances.size = 0;
		// The named instances table should start with size 0 and only create it when actually needing the tags
	}

	void RegisterCreateSharedComponent(
		EntityManager* manager,
		Component component,
		unsigned short size,
		EntityManagerCommandStream* command_stream
	) {
		size_t allocation_size = sizeof(DeferredCreateSharedComponent);

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		DeferredCreateSharedComponent* data = (DeferredCreateSharedComponent*)allocation;
		data->component = component;
		data->size = size;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_CREATE_SHARED_COMPONENT });
	}

#pragma endregion

#pragma region Destroy shared component

	void CommitDestroySharedComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroySharedComponent* data = (DeferredDestroySharedComponent*)_data;

		ECS_ASSERT(data->component.value < manager->m_shared_components.size);
		// -1 Signals it's empty - a component was never associated to this slot
		if (manager->m_shared_components[data->component.value].info.size == -1) {
			ECS_ASSERT(false);
		}

		manager->m_small_memory_manager.Deallocate(manager->m_shared_components[data->component.value].instances.buffer);
		manager->m_shared_components[data->component.value].info.size = -1;
		const void* table_buffer = manager->m_shared_components[data->component.value].named_instances.GetAllocatedBuffer();
		if (table_buffer != nullptr) {
			// Deallocate the table
			manager->m_small_memory_manager.Deallocate(table_buffer);
		}
	}

	void RegisterDestroySharedComponent(
		EntityManager* manager,
		Component component,
		unsigned short size,
		EntityManagerCommandStream* command_stream
	) {
		size_t allocation_size = sizeof(DeferredDestroySharedComponent);

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		DeferredDestroySharedComponent* data = (DeferredDestroySharedComponent*)allocation;
		data->component = component;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_DESTROY_SHARED_COMPONENT });
	}

#pragma endregion

#pragma region Create shared instance

	void CommitCreateSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateSharedInstance* data = (DeferredCreateSharedInstance*)_data;

		// If no component is allocated at that slot, fail
		unsigned short component_size = manager->m_shared_components[data->component.value].info.size;
		ECS_ASSERT(component_size != -1);

		// Allocate the memory
		void* allocation = manager->m_small_memory_manager.Allocate(component_size);
		memcpy(allocation, data->data, component_size);

		unsigned short instance_index = manager->m_shared_components[data->component.value].instances.Add(allocation);
		// Assert that the maximal amount of shared instance is not reached
		if (_additional_data != nullptr) {
			SharedInstance* instance = (SharedInstance*)_additional_data;
			instance->value = instance_index;
		}
	}

	void RegisterCreateSharedInstance(
		EntityManager* manager,
		Component component,
		const void* instance_data,
		EntityManagerCommandStream* command_stream
	) {
		size_t allocation_size = sizeof(DeferredCreateSharedInstance) + manager->m_shared_components[component.value].info.size;

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		DeferredCreateSharedInstance* data = (DeferredCreateSharedInstance*)allocation;
		data->component = component;
		data->data = function::OffsetPointer(allocation, sizeof(DeferredCreateSharedInstance));
		memcpy((void*)data->data, instance_data, manager->m_shared_components[component.value].info.size);

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_CREATE_SHARED_INSTANCE });
	}

#pragma endregion

#pragma region Create Named Shared Instance

	void CommitCreateNamedSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateNamedSharedInstance* data = (DeferredCreateNamedSharedInstance*)_data;

		SharedInstance instance;
		CommitCreateSharedInstance(manager, data, &instance);
		// Allocate memory for the name
		Stream<void> copied_identifier(function::Copy(&manager->m_small_memory_manager, data->identifier.ptr, data->identifier.size), data->identifier.size);
		InsertToDynamicTable(manager->m_shared_components[data->component.value].named_instances, &manager->m_small_memory_manager, instance, copied_identifier);
	}

	void RegisterCreateNamedSharedInstance(
		EntityManager* manager,
		Component component,
		const void* instance_data,
		ResourceIdentifier identifier,
		EntityManagerCommandStream* command_stream
	) {
		ECS_ASSERT(component.value < manager->m_shared_components.size);
		unsigned short component_size = manager->m_shared_components[component.value].info.size;
		size_t allocation_size = sizeof(DeferredCreateNamedSharedInstance) + component_size + identifier.size;
		// If the component size is -1 it means there is no actual component allocated to this slot
		ECS_ASSERT(component_size != -1);

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		DeferredCreateNamedSharedInstance* data = (DeferredCreateNamedSharedInstance*)allocation;
		data->component = component;
		data->data = function::OffsetPointer(allocation, sizeof(*data));
		data->identifier = { function::OffsetPointer(data->data, component_size), identifier.size };
		memcpy((void*)data->data, instance_data, component_size);
		memcpy((void*)data->identifier.ptr, identifier.ptr, identifier.size);

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_CREATE_NAMED_SHARED_INSTANCE });
	}

#pragma endregion

#pragma region Destroy Shared Instance

	struct CommitDestroySharedInstanceAdditionalData {
		SharedInstance instance;
		Component component;
	};

	void CommitDestroySharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroySharedInstance* data = (DeferredDestroySharedInstance*)_data;

		auto delete_instance = [manager](Component component, unsigned short index) {
			manager->m_small_memory_manager.Deallocate(manager->m_shared_components[component.value].instances[index]);
			manager->m_shared_components[component.value].instances.RemoveSwapBack(index);

			// If the index is different from the last index, then update the new shared instance reference inside the archetypes
			// And update the named instance reference if any
			unsigned short value_to_look_after = manager->m_shared_components[component.value].instances.size;
			if (index != value_to_look_after) {
				Component shared_component = component;
				VectorComponentSignature shared_signature(ComponentSignature(&shared_component, 1));
				for (size_t archetype_index = 0; archetype_index < manager->m_archetypes.size; archetype_index++) {
					VectorComponentSignature current_shared_signature = manager->m_archetypes[archetype_index].GetSharedSignature();
					if (current_shared_signature.HasComponents(shared_signature)) {
						// Check now the instance
						unsigned short in_archetype_component_index = manager->m_archetypes[archetype_index].FindSharedComponentIndex(shared_component);
						for (size_t base_index = 0; base_index < manager->m_archetypes[archetype_index].GetArchetypeBaseCount(); base_index++) {
							SharedInstance* archetype_instances = manager->m_archetypes[archetype_index].GetArchetypeBaseInstances(base_index);
							if (archetype_instances[in_archetype_component_index].value == value_to_look_after) {
								archetype_instances[in_archetype_component_index].value = index;
							}
						}
					}
				}

				// Update the named instance reference for the one that got swapped if such a reference exists
				// If the capacity is 0, skip
				if (manager->m_shared_components[component.value].named_instances.GetCapacity() != 0) {
					unsigned int extended_capacity = manager->m_shared_components[component.value].named_instances.GetExtendedCapacity();
					unsigned int table_index = 0;
					for (; table_index < extended_capacity; table_index++) {
						if (manager->m_shared_components[component.value].named_instances.IsItemAt(table_index)) {
							SharedInstance* named_instance_value = manager->m_shared_components[component.value].named_instances.GetValuePtrFromIndex(table_index);
							if (named_instance_value->value == value_to_look_after) {
								named_instance_value->value = index;
								
								// Exit the loop
								table_index = extended_capacity;
							}
						}
					}
				}
			}
		};

		size_t index = 0;
		// Additional data can be used by the named variant to tell us directly which index to delete
		if (_additional_data != nullptr) {
			CommitDestroySharedInstanceAdditionalData* instance_to_delete = (CommitDestroySharedInstanceAdditionalData*)_additional_data;
			delete_instance(instance_to_delete->component, instance_to_delete->instance.value);

			index = -1;
		}
		else {
			// If no component is allocated at that slot, fail
			unsigned short component_size = manager->m_shared_components[data->component.value].info.size;
			ECS_ASSERT(component_size != -1);

			// Do a linear search and do a bitwise test
			for (; index < manager->m_shared_components[data->component.value].instances.size; index++) {
				if (memcmp(manager->m_shared_components[data->component.value].instances[index], data->data, component_size) == 0) {
					delete_instance(data->component, index);

					// Exit the loop
					index = -2;
				}
			}
		}

		// Fail if the instance was not found
		ECS_ASSERT(index == -1);
	}

	void RegisterDestroySharedInstance(
		EntityManager* manager,
		Component component,
		SharedInstance instance,
		EntityManagerCommandStream* command_stream
	) {
		ECS_ASSERT(component.value < manager->m_shared_components.size);
		// If the component doesn't exist, fail
		ECS_ASSERT(manager->m_shared_components[component.value].info.size != -1);

		size_t allocation_size = sizeof(DeferredDestroySharedInstance) + manager->m_shared_components[component.value].info.size;

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		DeferredDestroySharedInstance* data = (DeferredDestroySharedInstance*)allocation;
		data->component = component;
		data->data = function::OffsetPointer(allocation, sizeof(*data));
		memcpy((void*)data->data, manager->m_shared_components[component.value].instances[instance.value], manager->m_shared_components[component.value].info.size);

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_DESTROY_SHARED_INSTANCE });
	}

#pragma endregion

#pragma region Destroy Named Shared Instance

	void CommitDestroyNamedSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyNamedSharedInstance* data = (DeferredDestroyNamedSharedInstance*)_data;

		ECS_ASSERT(data->component.value < manager->m_shared_components.size);
		
		SharedInstance instance;
		ECS_ASSERT(manager->m_shared_components[data->component.value].named_instances.TryGetValue(data->identifier, instance));

		CommitDestroySharedInstanceAdditionalData additional_data;
		additional_data.component = data->component;
		additional_data.instance = instance;
		CommitDestroySharedInstance(manager, nullptr, &additional_data);
	}

	void RegisterDestroyNamedSharedInstance(
		EntityManager* manager,
		Component component,
		ResourceIdentifier identifier,
		EntityManagerCommandStream* command_stream
	) {
		ECS_ASSERT(component.value < manager->m_shared_components.size);
		// If the component doesn't exist, fail
		ECS_ASSERT(manager->m_shared_components[component.value].info.size != -1);

		size_t allocation_size = sizeof(DeferredDestroyNamedSharedInstance) + identifier.size;

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		DeferredDestroyNamedSharedInstance* data = (DeferredDestroyNamedSharedInstance*)allocation;
		data->component = component;
		data->identifier.ptr = function::OffsetPointer(allocation, sizeof(*data));
		data->identifier.size = identifier.size;
		memcpy((void*)data->identifier.ptr, identifier.ptr, identifier.size);

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(manager, parameters, { allocation, DEFERRED_DESTROY_NAMED_SHARED_INSTANCE });
	}

#pragma endregion

	// The jump table for the deferred calls
	DeferredCallback DEFERRED_CALLBACKS[] = {
		CommitEntityAddComponent<false>,
		CommitEntityAddComponentSplatted,
		CommitEntityAddComponentScattered,
		CommitEntityAddComponentContiguous,
		CommitEntityAddComponentByEntities,
		CommitEntityAddComponentByEntitiesContiguous,
		CommitEntityRemoveComponent,
		CommitCreateEntities<CREATE_ENTITIES_TEMPLATE_NO_ADDITIONAL_DATA>,
		CommitCreateEntitiesDataSplatted,
		CommitCreateEntitiesDataScattered,
		CommitCreateEntitiesDataContiguous,
		CommitCreateEntitiesDataByEntities,
		CommitCreateEntitiesDataByEntitiesContiguous,
		CommitDeleteEntities,
		CommitEntityAddSharedComponent,
		CommitEntityRemoveSharedComponent,
		CommitCreateArchetype,
		CommitDestroyArchetype<true>,
		CommitCreateArchetypeBase,
		CommitDestroyArchetypeBase<true>,
		CommitCreateComponent,
		CommitDestroyComponent,
		CommitCreateSharedComponent,
		CommitDestroySharedComponent,
		CommitCreateSharedInstance,
		CommitCreateNamedSharedInstance,
		CommitDestroySharedInstance,
		CommitDestroyNamedSharedInstance
	};

	static_assert(std::size(DEFERRED_CALLBACKS) == DEFERRED_CALLBACK_COUNT);

#pragma endregion

	// --------------------------------------------------------------------------------------------------------------------

	EntityManager::EntityManager(const EntityManagerDescriptor& descriptor) : m_entity_pool(descriptor.entity_pool), m_memory_manager(descriptor.memory_manager),
		m_archetypes(GetAllocatorPolymorphic(descriptor.memory_manager), ENTITY_MANAGER_DEFAULT_ARCHETYPE_COUNT)
	{
		// Create a small memory manager in order to not fragment the memory of the main allocator
		m_small_memory_manager = MemoryManager(
			ENTITY_MANAGER_SMALL_ALLOCATOR_SIZE, 
			ENTITY_MANAGER_SMALL_ALLOCATOR_CHUNKS,
			ENTITY_MANAGER_SMALL_ALLOCATOR_BACKUP_SIZE, 
			descriptor.memory_manager->m_backup
		);

		// Calculate the first batch of unique and shared components space
		m_unique_components.Initialize(&m_small_memory_manager, ENTITY_MANAGER_DEFAULT_UNIQUE_COMPONENTS);
		m_shared_components.Initialize(&m_small_memory_manager, ENTITY_MANAGER_DEFAULT_SHARED_COMPONENTS);

		// Set them to zero
		memset(m_unique_components.buffer, 0, m_unique_components.MemoryOf(m_unique_components.size));
		memset(m_shared_components.buffer, 0, m_shared_components.MemoryOf(m_shared_components.size));

		// Allocate the atomic stream
		m_deferred_actions.Initialize(m_memory_manager, descriptor.deferred_action_capacity);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentCommit(Entity entity, Component component)
	{
		AddComponentCommit({ &entity, 1 }, component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponent(Entity entity, Component component, DeferredActionParameters parameters)
	{
		AddComponent({ &entity, 1 }, component, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentCommit(Entity entity, Component component, const void* data)
	{
		AddComponentCommit({ &entity, 1 }, component, data);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponent(Entity entity, Component component, const void* data, DeferredActionParameters parameters)
	{
		AddComponent({ &entity, 1 }, component, data, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentCommit(Entity entity, ComponentSignature components)
	{
		AddComponentCommit({ &entity, 1 }, components);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponent(Entity entity, ComponentSignature components, DeferredActionParameters parameters)
	{
		AddComponent({ &entity, 1 }, components, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentCommit(Entity entity, ComponentSignature components, const void** data)
	{
		AddComponentCommit({ &entity, 1 }, components, data, ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponent(Entity entity, ComponentSignature components, const void** data, DeferredActionParameters parameters)
	{
		AddComponent({ &entity, 1 }, components, data, ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponentCommit(Stream<Entity> entities, Component component)
	{
		DeferredAddComponentEntities commit_data;
		commit_data.components = { &component, (unsigned char)1 };
		commit_data.data = nullptr;
		commit_data.entities = entities;
		CommitEntityAddComponent<false>(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponent(Stream<Entity> entities, Component component, DeferredActionParameters parameters)
	{
		RegisterEntityAddComponent(this, entities, { &component, 1 }, nullptr, DEFERRED_ENTITY_ADD_COMPONENT, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponentCommit(Stream<Entity> entities, Component component, const void* data)
	{
		DeferredAddComponentEntities commit_data;
		commit_data.components = { &component, (unsigned char)1 };
		commit_data.data = &data;
		commit_data.entities = entities;
		CommitEntityAddComponentSplatted(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponent(Stream<Entity> entities, Component component, const void* data, DeferredActionParameters parameters)
	{
		RegisterEntityAddComponent(this, entities, { &component, 1 }, &data, DEFERRED_ENTITY_ADD_COMPONENT_SPLATTED, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponentCommit(Stream<Entity> entities, ComponentSignature components)
	{
		DeferredAddComponentEntities commit_data;
		commit_data.components = components;
		commit_data.data = nullptr;
		commit_data.entities = entities;
		CommitEntityAddComponent<false>(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponent(Stream<Entity> entities, ComponentSignature components, DeferredActionParameters parameters)
	{
		RegisterEntityAddComponent(this, entities, components, nullptr, DEFERRED_ENTITY_ADD_COMPONENT, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponentCommit(Stream<Entity> entities, ComponentSignature components, const void** data, EntityManagerCopyEntityDataType copy_type)
	{
		DeferredAddComponentEntities commit_data;
		commit_data.components = components;
		commit_data.entities = entities;
		commit_data.data = data;

		DeferredCallback callback = CommitEntityAddComponentSplatted;
		switch (copy_type) {
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_COMPONENTS_CONTIGUOUS:
			callback = CommitEntityAddComponentContiguous;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_COMPONENTS_SCATTERED:
			callback = CommitEntityAddComponentScattered;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY:
			callback = CommitEntityAddComponentByEntities;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY_CONTIGUOUS:
			callback = CommitEntityAddComponentByEntitiesContiguous;
			break;
		default:
			ECS_ASSERT(false);
		}

		callback(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponent(
		Stream<Entity> entities, 
		ComponentSignature components, 
		const void** data,
		EntityManagerCopyEntityDataType copy_type,
		DeferredActionParameters parameters
	)
	{
		DeferredActionType action_type = DEFERRED_ENTITY_ADD_COMPONENT_SPLATTED;

		switch (copy_type) {
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_COMPONENTS_CONTIGUOUS:
			action_type = DEFERRED_ENTITY_ADD_COMPONENT_CONTIGUOUS;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_COMPONENTS_SCATTERED:
			action_type = DEFERRED_ENTITY_ADD_COMPONENT_SCATTERED;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY:
			action_type = DEFERRED_ENTITY_ADD_COMPONENT_BY_ENTITIES;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY_CONTIGUOUS:
			action_type = DEFERRED_ENTITY_ADD_COMPONENT_BY_ENTITIES_CONTIGUOUS;
			break;
		default:
			ECS_ASSERT(false);
		}
		RegisterEntityAddComponent(this, entities, components, data, action_type, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponentCommit(Entity entity, Component shared_component, SharedInstance instance)
	{
		AddSharedComponentCommit({ &entity, 1 }, shared_component, instance);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponent(Entity entity, Component shared_component, SharedInstance instance, DeferredActionParameters parameters)
	{
		AddSharedComponent({ &entity, 1 }, shared_component, instance, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponentCommit(Stream<Entity> entities, Component shared_component, SharedInstance instance)
	{
		DeferredAddSharedComponentEntities commit_data;
		commit_data.components.indices = &shared_component;
		commit_data.components.instances = &instance;
		commit_data.components.count = 1;
		commit_data.entities = entities;
		CommitEntityAddSharedComponent(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponent(Stream<Entity> entities, Component shared_component, SharedInstance instance, DeferredActionParameters parameters)
	{
		RegisterEntityAddSharedComponent(this, entities, { &shared_component, &instance, 1 }, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponentCommit(Entity entity, SharedComponentSignature components) {
		AddSharedComponentCommit({ &entity, 1 }, components);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponent(Entity entity, SharedComponentSignature components, DeferredActionParameters parameters) {
		AddSharedComponent({ &entity, 1 }, components, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponentCommit(Stream<Entity> entities, SharedComponentSignature components) {
		DeferredAddSharedComponentEntities commit_data;
		commit_data.components = components;
		commit_data.components.count = 1;
		commit_data.entities = entities;
		CommitEntityAddSharedComponent(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponent(Stream<Entity> entities, SharedComponentSignature components, DeferredActionParameters parameters) {
		RegisterEntityAddSharedComponent(this, entities, components, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::AllocateTemporaryBuffer(size_t size, size_t alignment)
	{
		return m_temporary_allocator.Allocate(size, alignment);
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned short EntityManager::CreateArchetypeCommit(ComponentSignature unique, ComponentSignature shared)
	{
		unsigned short index = 0;

		DeferredCreateArchetype commit_data;
		commit_data.shared_components = shared;
		commit_data.unique_components = unique;
		CommitCreateArchetype(this, &commit_data, &index);

		return index;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateArchetype(ComponentSignature unique, ComponentSignature shared, EntityManagerCommandStream* command_stream)
	{
		RegisterCreateArchetype(this, unique, shared, command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	ushort2 EntityManager::CreateArchetypeBaseCommit(
		ComponentSignature unique_signature,
		SharedComponentSignature shared_signature,
		unsigned int archetype_chunk_size
	)
	{
		DeferredCreateArchetypeBase commit_data;
		commit_data.unique_components = unique_signature;
		commit_data.archetype_chunk_size = archetype_chunk_size;
		commit_data.shared_components = shared_signature;

		ushort2 indices;
		CommitCreateArchetypeBase(this, &commit_data, &indices);
		return indices;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateArchetypeBase(
		ComponentSignature unique_signature,
		SharedComponentSignature shared_signature,
		EntityManagerCommandStream* command_stream,
		unsigned int archetype_chunk_size
	)
	{
		RegisterCreateArchetypeBase(this, unique_signature, shared_signature, archetype_chunk_size, command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// It will commit the archetype addition
	unsigned short EntityManager::CreateArchetypeBaseCommit(
		unsigned short archetype_index,
		SharedComponentSignature shared_signature,
		unsigned int archetype_chunk_size
	) {
		ComponentSignature unique = m_archetypes[archetype_index].GetUniqueSignature();
		return CreateArchetypeBaseCommit(unique, shared_signature, archetype_chunk_size).y;
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Deferred Call
	void EntityManager::CreateArchetypeBase(
		unsigned short archetype_index,
		SharedComponentSignature shared_signature,
		EntityManagerCommandStream* command_stream,
		unsigned int archetype_chunk_size
	) {
		return CreateArchetypeBase(m_archetypes[archetype_index].GetUniqueSignature(), shared_signature, command_stream, archetype_chunk_size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	Entity EntityManager::CreateEntityCommit(ComponentSignature unique_components, SharedComponentSignature shared_components)
	{
		Entity entity;
		CreateEntitiesCommit(1, unique_components, shared_components, &entity);
		return entity;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateEntity(ComponentSignature unique_components, SharedComponentSignature shared_components, EntityManagerCommandStream* command_stream)
	{
		CreateEntities(1, unique_components, shared_components, command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateEntitiesCommit(unsigned int count, ComponentSignature unique_components, SharedComponentSignature shared_components, Entity* entities)
	{
		DeferredCreateEntities commit_data;
		commit_data.count = count;
		commit_data.data = nullptr;

		commit_data.shared_components = shared_components;
		commit_data.unique_components = unique_components;

		if (entities != nullptr) {
			CommitCreateEntitiesAdditionalData additional_data;
			additional_data.entities = { entities, count };
			CommitCreateEntities<CREATE_ENTITIES_TEMPLATE_GET_ENTITIES>(this, &commit_data, &additional_data);
		}
		else {
			CommitCreateEntities<CREATE_ENTITIES_TEMPLATE_NO_ADDITIONAL_DATA>(this, &commit_data, nullptr);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateEntities(
		unsigned int count,
		ComponentSignature unique_components, 
		SharedComponentSignature shared_components,
		EntityManagerCommandStream* command_stream
	)
	{
		RegisterCreateEntities(this, count, unique_components, shared_components, {nullptr, 0}, nullptr, DEFERRED_CREATE_ENTITIES, { command_stream });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateEntitiesCommit(
		unsigned int count,
		ComponentSignature unique_components,
		SharedComponentSignature shared_components,
		ComponentSignature components_with_data,
		const void** data,
		EntityManagerCopyEntityDataType copy_type,
		Entity* entities
	) {
		DeferredCreateEntities commit_data;
		commit_data.count = count;
		commit_data.data = data;
		commit_data.shared_components = shared_components;
		commit_data.unique_components = unique_components;
		commit_data.components_with_data = components_with_data;

		DeferredCallback callback = CommitCreateEntitiesDataSplatted;
		switch (copy_type) {
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_COMPONENTS_CONTIGUOUS:
			callback = CommitCreateEntitiesDataContiguous;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_COMPONENTS_SCATTERED:
			callback = CommitCreateEntitiesDataScattered;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY:
			callback = CommitCreateEntitiesDataByEntities;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY_CONTIGUOUS:
			callback = CommitCreateEntitiesDataByEntitiesContiguous;
			break;
		default:
			ECS_ASSERT(false);
		}

		void* _additional_data = nullptr;
		CommitCreateEntitiesAdditionalData additional_data;
		if (entities != nullptr) {
			additional_data.entities = { entities, count };
			_additional_data = &additional_data;
		}

		callback(this, &commit_data, _additional_data);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateEntities(
		unsigned int count,
		ComponentSignature unique_components,
		SharedComponentSignature shared_components,
		ComponentSignature components_with_data,
		const void** data,
		EntityManagerCopyEntityDataType copy_type,
		EntityManagerCommandStream* command_stream
	) {
		DeferredActionType action_type = DEFERRED_CREATE_ENTITIES_SPLATTED;
		switch (copy_type) {
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_COMPONENTS_CONTIGUOUS:
			action_type = DEFERRED_CREATE_ENTITIES_CONTIGUOUS;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_COMPONENTS_SCATTERED:
			action_type = DEFERRED_CREATE_ENTITIES_SCATTERED;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY:
			action_type = DEFERRED_CREATE_ENTITIES_BY_ENTITIES;
			break;
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY_CONTIGUOUS:
			action_type = DEFERRED_CREATE_ENTITIES_BY_ENTITIES_CONTIGUOUS;
			break;
		default:
			ECS_ASSERT(false);
		}
		
		RegisterCreateEntities(this, count, unique_components, shared_components, components_with_data, data, action_type, {command_stream});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DeleteEntityCommit(Entity entity)
	{
		DeleteEntitiesCommit({ &entity, 1 });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DeleteEntity(Entity entity, EntityManagerCommandStream* command_stream)
	{
		DeleteEntities({ &entity, 1 }, { command_stream });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DeleteEntitiesCommit(Stream<Entity> entities)
	{
		DeferredDeleteEntities commit_data;
		commit_data.entities = entities;
		CommitDeleteEntities(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DeleteEntities(Stream<Entity> entities, DeferredActionParameters parameters)
	{
		RegisterDeleteEntities(this, entities, parameters);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypeCommit(unsigned short archetype_index)
	{
		DeferredDestroyArchetype commit_data;
		commit_data.archetype_index = archetype_index;
		CommitDestroyArchetype<false>(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetype(unsigned short archetype_index, EntityManagerCommandStream* command_stream)
	{
		const Archetype* archetype = GetArchetype(archetype_index);
		RegisterDestroyArchetype(this, archetype->GetUniqueSignature(), archetype->GetSharedSignature(), command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypeCommit(ComponentSignature unique_components, ComponentSignature shared_components)
	{
		DeferredDestroyArchetype commit_data;
		commit_data.unique_components = unique_components;
		commit_data.shared_components = shared_components;
		CommitDestroyArchetype<true>(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetype(ComponentSignature unique_components, ComponentSignature shared_components, EntityManagerCommandStream* command_stream)
	{
		RegisterDestroyArchetype(this, unique_components, shared_components, command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypeBaseCommit(unsigned short archetype_index, unsigned short archetype_subindex)
	{
		DeferredDestroyArchetypeBase commit_data;
		commit_data.indices.x = archetype_index;
		commit_data.indices.y = archetype_subindex;
		CommitDestroyArchetypeBase<false>(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypeBase(unsigned short archetype_index, unsigned short archetype_subindex, EntityManagerCommandStream* command_stream)
	{
		const Archetype* archetype = GetArchetype(archetype_index);
		RegisterDestroyArchetypeBase(this, archetype->GetUniqueSignature(), archetype->GetSharedSignature(archetype_subindex), command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypeBaseCommit(ComponentSignature unique_components, SharedComponentSignature shared_components)
	{
		DeferredDestroyArchetypeBase commit_data;
		commit_data.unique_components = unique_components;
		commit_data.shared_components = shared_components;
		CommitDestroyArchetypeBase<true>(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypeBase(ComponentSignature unique_components, SharedComponentSignature shared_components, EntityManagerCommandStream* command_stream)
	{
		RegisterDestroyArchetypeBase(this, unique_components, shared_components, command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename Query>
	unsigned short ECS_VECTORCALL FindArchetypeImplementation(const EntityManager* manager, Query query) {
		for (size_t index = 0; index < manager->m_archetypes.size; index++) {
			VectorComponentSignature archetype_unique = manager->m_archetypes[index].GetUniqueSignature();
			if (query.VerifiesUnique(archetype_unique)) {
				VectorComponentSignature archetype_shared = manager->m_archetypes[index].GetSharedSignature();
				if (query.VerifiesShared(archetype_shared)) {
					// Matches both signatures - return it
					return index;
				}
			}
		}
		return -1;
	}

	unsigned short EntityManager::FindArchetype(ArchetypeQuery query) const
	{
		return FindArchetypeImplementation(this, query);
	}

	// --------------------------------------------------------------------------------------------------------------------

	Archetype* EntityManager::FindArchetypePtr(ArchetypeQuery query)
	{
		unsigned short archetype_index = FindArchetype(query);
		if (archetype_index != -1) {
			return GetArchetype(archetype_index);
		}
		return nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned short EntityManager::FindArchetypeExclude(ArchetypeQueryExclude query) const {
		return FindArchetypeImplementation(this, query);
	}

	// --------------------------------------------------------------------------------------------------------------------

	Archetype* EntityManager::FindArchetypeExcludePtr(ArchetypeQueryExclude query) {
		unsigned short archetype_index = FindArchetypeExclude(query);
		if (archetype_index != -1) {
			return GetArchetype(archetype_index);
		}
		return nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename Query>
	ushort2 ECS_VECTORCALL FindArchetypeBaseImplementation(const EntityManager* manager, Query query, VectorComponentSignature shared_instances) {
		unsigned short main_archetype_index = 0;
		if constexpr (std::is_same_v<Query, ArchetypeQuery>) {
			main_archetype_index = manager->FindArchetype(query);
		}
		else {
			main_archetype_index = manager->FindArchetypeExclude(query);
		}

		if (main_archetype_index != -1) {
			for (size_t base_index = 0; base_index < manager->m_archetypes[main_archetype_index].GetArchetypeBaseCount(); base_index++) {
				VectorComponentSignature archetype_shared_instances;
				archetype_shared_instances.ConvertInstances(
					manager->m_archetypes[main_archetype_index].m_base_archetypes[base_index].shared_instances,
					manager->m_archetypes[main_archetype_index].m_shared_components.count
				);
				if (SharedComponentSignatureHasInstances(
					manager->m_archetypes[main_archetype_index].GetSharedSignature(),
					archetype_shared_instances,
					query.shared,
					shared_instances
				)) {
					return { main_archetype_index, (unsigned short)base_index };
				}
			}
			return { main_archetype_index, (unsigned short)-1 };
		}
		return { (unsigned short)-1, (unsigned short)-1 };
	}

	ushort2 EntityManager::FindArchetypeBase(ArchetypeQuery query, VectorComponentSignature shared_instances) const
	{
		return FindArchetypeBaseImplementation(this, query, shared_instances);
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* EntityManager::FindArchetypeBasePtr(ArchetypeQuery query, VectorComponentSignature shared_instances)
	{
		ushort2 indices = FindArchetypeBase(query, shared_instances);
		if (indices.y == -1) {
			return nullptr;
		}
		return GetArchetypeBase(indices.x, indices.y);
	}

	// --------------------------------------------------------------------------------------------------------------------

	ushort2 EntityManager::FindArchetypeBaseExclude(
		ArchetypeQueryExclude query,
		VectorComponentSignature shared_instances
	) const {
		return FindArchetypeBaseImplementation(this, query, shared_instances);
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* EntityManager::FindArchetypeBaseExcludePtr(
		ArchetypeQueryExclude query,
		VectorComponentSignature shared_instances
	) {
		ushort2 indices = FindArchetypeBaseExclude(query, shared_instances);
		if (indices.y == -1) {
			return nullptr;
		}
		return GetArchetypeBase(indices.x, indices.y);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::Flush() {
		unsigned int action_count = m_deferred_actions.size.load(ECS_RELAXED);
		for (size_t index = 0; index < action_count; index++) {
			DEFERRED_CALLBACKS[m_deferred_actions[index].type](this, m_deferred_actions[index].data, nullptr);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::Flush(EntityManagerCommandStream command_stream) {
		unsigned int action_count = command_stream.size;
		for (size_t index = 0; index < action_count; index++) {
			DEFERRED_CALLBACKS[command_stream[index].type](this, command_stream[index].data, nullptr);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	Archetype* EntityManager::GetArchetype(unsigned short index) {
		ECS_ASSERT(index < m_archetypes.size);
		return &m_archetypes[index];
	}

	// --------------------------------------------------------------------------------------------------------------------

	const Archetype* EntityManager::GetArchetype(unsigned short index) const {
		ECS_ASSERT(index < m_archetypes.size);
		return &m_archetypes[index];
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* EntityManager::GetArchetypeBase(unsigned short main_index, unsigned short base_index) {
		Archetype* archetype = GetArchetype(main_index);
		return archetype->GetArchetypeBase(base_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const ArchetypeBase* EntityManager::GetArchetypeBase(unsigned short main_index, unsigned short base_index) const {
		const Archetype* archetype = GetArchetype(main_index);
		return archetype->GetArchetypeBase(base_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// It requires a syncronization barrier!! If the archetype does not exist, then it will commit the creation of a new one
	unsigned short EntityManager::FindOrCreateArchetype(ComponentSignature unique_signature, ComponentSignature shared_signature) {
		unsigned short archetype_index = FindArchetype({unique_signature, shared_signature});
		if (archetype_index != -1) {
			return archetype_index;
		}
		// A new must be created
		return CreateArchetypeCommit(unique_signature, shared_signature);
	}

	// --------------------------------------------------------------------------------------------------------------------

	ushort2 EntityManager::FindOrCreateArchetypeBase(
		ComponentSignature unique_signature,
		SharedComponentSignature shared_signature,
		unsigned int chunk_size
	) {
		unsigned short main_archetype_index = FindOrCreateArchetype(unique_signature, { shared_signature.indices, shared_signature.count });
		Archetype* main_archetype = GetArchetype(main_archetype_index);
		unsigned short base = main_archetype->FindArchetypeBaseIndex(shared_signature);
		if (base == -1) {
			// It doesn't exist, a new one must be created
			base = main_archetype->CreateBaseArchetype(shared_signature, chunk_size);
		}

		return {main_archetype_index, base};
	}

	// --------------------------------------------------------------------------------------------------------------------

	Archetype* EntityManager::GetOrCreateArchetype(ComponentSignature unique_signature, ComponentSignature shared_signature) {
		return GetArchetype(FindOrCreateArchetype(unique_signature, shared_signature));
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* EntityManager::GetOrCreateArchetypeBase(
		ComponentSignature unique_signature,
		SharedComponentSignature shared_signature,
		unsigned int chunk_size
	) {
		ushort2 indices = FindOrCreateArchetypeBase(unique_signature, shared_signature, chunk_size);
		return GetArchetypeBase(indices.x, indices.y);
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename Query>
	void ECS_VECTORCALL GetArchetypeImplementation(const EntityManager* manager, Query query, CapacityStream<unsigned short>& archetypes) {
		for (size_t index = 0; index < manager->m_archetypes.size; index++) {
			if (query.VerifiesUnique(manager->m_archetypes[index].GetUniqueSignature())) {
				if (query.VerifiesShared(manager->m_archetypes[index].GetSharedSignature())) {
					archetypes.AddSafe(index);
				}
			}
		}
	}

	template<typename Query>
	void ECS_VECTORCALL GetArchetypePtrsImplementation(const EntityManager* manager, Query query, CapacityStream<Archetype*>& archetypes) {
		for (size_t index = 0; index < manager->m_archetypes.size; index++) {
			if (query.VerifiesUnique(manager->m_archetypes[index].GetUniqueSignature())) {
				if (query.VerifiesShared(manager->m_archetypes[index].GetSharedSignature())) {
					archetypes.AddSafe(manager->m_archetypes.buffer + index);
				}
			}
		}
	}

	void EntityManager::GetArchetypes(ArchetypeQuery query, CapacityStream<unsigned short>& archetypes) const
	{
		GetArchetypeImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetArchetypesPtrs(ArchetypeQuery query, CapacityStream<Archetype*>& archetypes) const
	{
		GetArchetypePtrsImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetArchetypesExclude(ArchetypeQueryExclude query, CapacityStream<unsigned short>& archetypes) const
	{
		GetArchetypeImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetArchetypesExcludePtrs(ArchetypeQueryExclude query, CapacityStream<Archetype*>& archetypes) const
	{
		GetArchetypePtrsImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetComponent(Entity entity, Component component) const
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetArchetypeBase(info.main_archetype, info.base_archetype)->GetComponent(info, component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetComponentWithIndex(Entity entity, unsigned char component_index) const
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetArchetypeBase(info.main_archetype, info.base_archetype)->GetComponentByIndex(info, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetComponent(Entity entity, ComponentSignature components, void** data) const
	{
		EntityInfo info = GetEntityInfo(entity);
		const ArchetypeBase* base = GetArchetypeBase(info.main_archetype, info.base_archetype);
		for (size_t index = 0; index < components.count; index++) {
			data[index] = base->GetComponent(info, components.indices[index]);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetComponentWithIndex(Entity entity, ComponentSignature components, void** data) const
	{
		EntityInfo info = GetEntityInfo(entity);
		const ArchetypeBase* base = GetArchetypeBase(info.main_archetype, info.base_archetype);
		for (size_t index = 0; index < components.count; index++) {
			data[index] = base->GetComponentByIndex(info, components.indices[index].value);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	EntityInfo EntityManager::GetEntityInfo(Entity entity) const
	{
		return m_entity_pool->GetInfo(entity);
	}

	// --------------------------------------------------------------------------------------------------------------------

	ComponentSignature EntityManager::GetEntitySignature(Entity entity, Component* components) const {
		ComponentSignature signature = GetEntitySignatureStable(entity);
		memcpy(components, signature.indices, sizeof(Component) * signature.count);
		return { components, signature.count };
	}

	// --------------------------------------------------------------------------------------------------------------------

	// It will point to the archetype's signature - Do not modify!!
	ComponentSignature EntityManager::GetEntitySignatureStable(Entity entity) const {
		EntityInfo info = GetEntityInfo(entity);
		return m_archetypes[info.main_archetype].GetUniqueSignature();
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedComponentSignature EntityManager::GetEntitySharedSignature(Entity entity, Component* shared, SharedInstance* instances) {
		SharedComponentSignature signature = GetEntitySharedSignatureStable(entity);
		memcpy(shared, signature.indices, sizeof(Component) * signature.count);
		memcpy(instances, signature.instances, sizeof(SharedInstance) * signature.count);
		return { shared, instances, signature.count };
	}

	// --------------------------------------------------------------------------------------------------------------------
	
	SharedComponentSignature EntityManager::GetEntitySharedSignatureStable(Entity entity) {
		EntityInfo info = GetEntityInfo(entity);
		ComponentSignature shared_signature = m_archetypes[info.main_archetype].GetSharedSignature();
		SharedInstance* instances = m_archetypes[info.main_archetype].GetArchetypeBaseInstances(info.base_archetype);
		return { shared_signature.indices, instances, shared_signature.count };
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetEntityCompleteSignature(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared) {
		ComponentSignature stable_unique = GetEntitySignatureStable(entity);
		SharedComponentSignature stable_shared = GetEntitySharedSignatureStable(entity);

		memcpy(unique->indices, stable_unique.indices, sizeof(Component) * stable_unique.count);
		unique->count = stable_unique.count;

		memcpy(shared->indices, stable_shared.indices, sizeof(Component) * stable_shared.count);
		memcpy(shared->instances, stable_shared.instances, sizeof(SharedInstance) * stable_shared.count);
		shared->count = stable_shared.count;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetEntityCompleteSignatureStable(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared) {
		*unique = GetEntitySignatureStable(entity);
		*shared = GetEntitySharedSignatureStable(entity);
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename Query, typename EntitiesStorage>
	void ECS_VECTORCALL GetEntitiesImplementation(const EntityManager* manager, Query query, EntitiesStorage entities) {
		for (size_t index = 0; index < manager->m_archetypes.size; index++) {
			if (query.VerifiesUnique(manager->m_archetypes[index].GetUniqueSignature())) {
				if (query.VerifiesShared(manager->m_archetypes[index].GetSharedSignature())) {
					// Matches the query
					for (size_t base_index = 0; base_index < manager->m_archetypes[index].GetArchetypeBaseCount(); base_index++) {
						const ArchetypeBase* base = manager->GetArchetypeBase(index, base_index);
						base->GetEntities(entities);
					}
				}
			}
		}
	}

	void EntityManager::GetEntities(ArchetypeQuery query, CapacityStream<Entity>& entities) const
	{
		GetEntitiesImplementation(this, query, entities);
	}

	void EntityManager::GetEntities(ArchetypeQuery query, Stream<Entity>* entities) const
	{
		GetEntitiesImplementation(this, query, entities);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetEntitiesExclude(ArchetypeQueryExclude query, CapacityStream<Entity>& entities) const
	{
		GetEntitiesImplementation(this, query, entities);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetEntitiesExclude(ArchetypeQueryExclude query, Stream<Entity>* entities) const
	{
		GetEntitiesImplementation(this, query, entities);
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::GetSharedComponentInstance(Component component, const void* data) const
	{
		ECS_ASSERT(component.value < m_shared_components.size);
		unsigned short component_size = m_shared_components[component.value].info.size;
		
		// If the component size is -1, it means no actual component lives at that index
		ECS_ASSERT(component_size != -1);
		for (size_t index = 0; index < m_shared_components[component.value].instances.size; index++) {
			if (memcmp(m_shared_components[component.value].instances[index], data, component_size) == 0) {
				return { (unsigned short)index };
			}
		}

		return { (unsigned short)-1 };
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::GetNamedSharedComponentInstance(Component component, ResourceIdentifier identifier) const {
		ECS_ASSERT(component.value < m_shared_components.size);
		unsigned short component_size = m_shared_components[component.value].info.size;

		// If the component size is -1, it means no actual component lives at that index
		ECS_ASSERT(component_size != -1);
		SharedInstance instance;
		ECS_ASSERT(m_shared_components[component.value].named_instances.TryGetValue(identifier, instance));

		return instance;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterComponentCommit(Component component, unsigned short size) {
		DeferredCreateComponent commit_data;
		commit_data.component = component;
		commit_data.size = size;
		CommitCreateComponent(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterComponent(Component component, unsigned short size, EntityManagerCommandStream* command_stream) {
		RegisterCreateComponent(this, component, size, command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterSharedComponentCommit(Component component, unsigned short size) {
		DeferredCreateSharedComponent commit_data;
		commit_data.component = component;
		commit_data.size = size;
		CommitCreateSharedComponent(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterSharedComponent(Component component, unsigned short size, EntityManagerCommandStream* command_stream) {
		RegisterCreateSharedComponent(this, component, size, command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::RegisterSharedInstanceCommit(Component component, const void* data) {
		DeferredCreateSharedInstance commit_data;
		commit_data.component = component;
		commit_data.data = data;

		SharedInstance instance;
		CommitCreateSharedInstance(this, &commit_data, &instance);
		
		return instance;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterSharedInstance(Component component, const void* data, EntityManagerCommandStream* command_stream) {
		RegisterCreateSharedInstance(this, component, data, command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::RegisterNamedSharedInstanceCommit(Component component, ResourceIdentifier identifier, const void* data) {
		DeferredCreateNamedSharedInstance commit_data;
		commit_data.component = component;
		commit_data.identifier = identifier;
		commit_data.data = data;

		SharedInstance instance;
		CommitCreateNamedSharedInstance(this, &commit_data, &instance);

		return instance;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterNamedSharedInstance(Component component, ResourceIdentifier identifier, const void* data, EntityManagerCommandStream* command_stream) {
		RegisterCreateNamedSharedInstance(this, component, data, identifier, command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::SetSharedComponentData(Component component, SharedInstance instance, const void* data)
	{
		ECS_ASSERT(component.value < m_shared_components.size);
		ECS_ASSERT(instance.value < m_shared_components[component.value].instances.size);

		unsigned short component_size = m_shared_components[component.value].info.size;
		memcpy(m_shared_components[component.value].instances[instance.value], data, component_size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::SetNamedSharedComponentData(Component component, ResourceIdentifier identifier, const void* data) {
		ECS_ASSERT(component.value < m_shared_components.size);
		SharedInstance instance;
		ECS_ASSERT(m_shared_components[component.value].named_instances.TryGetValue(identifier, instance));

		unsigned short component_size = m_shared_components[component.value].info.size;
		memcpy(m_shared_components[component.value].instances[instance.value], data, component_size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------

	EntityManager::TemporaryAllocator::TemporaryAllocator() {}

	// --------------------------------------------------------------------------------------------------------------------

	EntityManager::TemporaryAllocator::TemporaryAllocator(GlobalMemoryManager* _backup) : backup(_backup) {
		lock.unlock();
		buffers.Initialize(backup, 1, ENTITY_MANAGER_TEMPORARY_ALLOCATOR_MAX_BACKUP);
		backup_allocations.Initialize(backup, 0, ENTITY_MANAGER_TEMPORARY_ALLOCATOR_BACKUP_TRACK_CAPACITY);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::TemporaryAllocator::Allocate(size_t size, size_t alignment)
	{
		// If the allocation size is greater than the chunk size, allocate directly from the global memory manager
		if (size > ENTITY_MANAGER_TEMPORARY_ALLOCATOR_CHUNK_SIZE) {
			void* allocation = backup->Allocate_ts(size, alignment);
			backup_allocations.AddSafe(allocation);
			return allocation;
		}
		else {
			lock.wait_locked();

			Stream<char> allocation = { nullptr,0 };
			while (true) {
				allocation = buffers[buffers.size - 1].Request(size + alignment);
				if (allocation.buffer + allocation.size >= buffers[buffers.size - 1].buffer + buffers[buffers.size - 1].capacity) {
					// A new allocation must be performed
					if (lock.try_lock()) {
						// If we acquired the lock perform the new buffer allocation
						void* new_allocation = backup->Allocate_ts(ENTITY_MANAGER_TEMPORARY_ALLOCATOR_CHUNK_SIZE);
						//buffers.AddSafe(AtomicStream<char>(new_allocation, 0, ENTITY_MANAGER_TEMPORARY_ALLOCATOR_CHUNK_SIZE));
						allocation = buffers[buffers.size - 1].Request(size);

						lock.unlock();
						// Can break out of the loop after this
						break;
					}
					else {
						// Spin wait until the resizing is performed
						lock.wait_locked();
					}
				}
				else {
					// Break the loop
					break;
				}
			}
			return (void*)function::align_pointer((uintptr_t)allocation.buffer, alignment);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::TemporaryAllocator::Clear()
	{
		for (size_t index = 1; index < buffers.size; index++) {
			backup->Deallocate(buffers[index].buffer);
		}
		for (size_t index = 0; index < backup_allocations.size; index++) {
			backup->Deallocate(backup_allocations[index]);
		}
		backup_allocations.Reset();
	}

	// --------------------------------------------------------------------------------------------------------------------

}