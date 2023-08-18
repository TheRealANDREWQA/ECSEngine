#include "ecspch.h"
#include "EntityManager.h"
#include "../Utilities/Function.h"
#include "../Utilities/FunctionInterfaces.h"
#include "../Utilities/Crash.h"
#include "ArchetypeQueryCache.h"

#define ENTITY_MANAGER_DEFAULT_UNIQUE_COMPONENTS (1 << 7)
#define ENTITY_MANAGER_DEFAULT_SHARED_COMPONENTS (1 << 7)
#define ENTITY_MANAGER_DEFAULT_ARCHETYPE_COUNT (1 << 7)

#define ENTITY_MANAGER_TEMPORARY_ALLOCATOR_MAX_BACKUP (1 << 3)
#define ENTITY_MANAGER_TEMPORARY_ALLOCATOR_CHUNK_SIZE 1 * ECS_MB
#define ENTITY_MANAGER_TEMPORARY_ALLOCATOR_BACKUP_TRACK_CAPACITY (1 << 8)

#define ENTITY_MANAGER_SMALL_ALLOCATOR_SIZE ECS_KB * 256
#define ENTITY_MANAGER_SMALL_ALLOCATOR_CHUNKS ECS_KB * 2
#define ENTITY_MANAGER_SMALL_ALLOCATOR_BACKUP_SIZE ECS_KB * 128

// A default of 4 is a good start
#define COMPONENT_ALLOCATOR_ARENA_COUNT 4
#define COMPONENT_ALLOCATOR_BLOCK_COUNT 2048

namespace ECSEngine {

	typedef void (*DeferredCallbackFunctor)(EntityManager*, void*, void*);

	// This is done per entity basis. Might be slow due to the fact that it will
	// bounce around the RAM. The buffers should be like this
	// AAAAA BBBBBB CCCCCCC (a buffer per component)
	struct DeferredWriteComponent {
		Stream<Entity> entities;
		ComponentSignature write_signature;
		const void** buffers;
	};

	// This is efficient because it will directly memcpy into a buffer
	struct DeferredWriteArchetypes {
		Stream<ushort2> archetypes;
		ComponentSignature write_signature;
		Stream<void**> buffers;
	};

	struct DeferredCreateEntities {
		unsigned int count;
		bool exclude_from_hierarchy;
		bool copy_buffers;
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

	struct DeferredChangeSharedInstanceEntities {
		Stream<ChangeSharedComponentElement> elements;
		bool destroy_base_archetype;
	};

	struct DeferredCopyEntities {
		Entity entity;
		unsigned int count;
		bool copy_children;
	};

	struct DeferredCreateArchetype {
		ComponentSignature unique_components;
		ComponentSignature shared_components;
	};

	struct DeferredDestroyArchetype {
		ComponentSignature unique_components;
		ComponentSignature shared_components;
		unsigned int archetype_index;
	};

	struct DeferredCreateArchetypeBase {
		ComponentSignature unique_components;
		SharedComponentSignature shared_components;
		unsigned int starting_size;
	};

	struct DeferredDestroyArchetypeBase {
		ComponentSignature unique_components;
		SharedComponentSignature shared_components;
		uint2 indices;
	};

	struct DeferredCreateComponent {
		Component component;
		unsigned int size;
		size_t allocator_size;
		Stream<char> name;
		ComponentBuffer component_buffers[ECS_COMPONENT_INFO_MAX_BUFFER_COUNT];
		unsigned short component_buffers_count;
	};

	struct DeferredDestroyComponent {
		Component component;
	};

	struct DeferredCreateSharedComponent {
		Component component;
		unsigned int size;
		size_t allocator_size;
		Stream<char> name;
		ComponentBuffer component_buffers[ECS_COMPONENT_INFO_MAX_BUFFER_COUNT];
		unsigned short component_buffers_count;
	};

	struct DeferredDestroySharedComponent {
		Component component;
	};

	struct DeferredCreateSharedInstance {
		Component component;
		bool copy_buffers;
		const void* data;
	};

	struct DeferredCreateNamedSharedInstance {
		Component component;
		bool copy_buffers;
		const void* data;
		Stream<char> identifier;
	};
	
	struct DeferredBindNamedSharedInstance {
		Component component;
		SharedInstance instance;
		Stream<char> identifier;
	};

	struct DeferredDestroySharedInstance {
		Component component;
		SharedInstance instance;
	};

	struct DeferredDestroyNamedSharedInstance {
		Component component;
		Stream<char> identifier;
	};

	struct DeferredDestroyUnreferencedSharedInstance {
		Component component;
		SharedInstance instance;
	};
	
	struct DeferredDestroyUnreferencedSharedInstanceAdditional {
		bool return_value;
	};

	struct DeferredDestroyUnreferencedSharedInstances {
		Component component;
	};

	struct DeferredClearEntityTag {
		Stream<Entity> entities;
		unsigned char tag;
	};

	struct DeferredSetEntityTag {
		Stream<Entity> entities;
		unsigned char tag;
	};

	struct DeferredAddEntitiesToHierarchyPairs {
		Stream<EntityPair> pairs;
	};

	struct DeferredAddEntitiesToHierarchyContiguous {
		const Entity* parents;
		const Entity* children;
		unsigned int count;
	};

	struct DeferredAddEntitiesToParentHierarchy {
		Stream<Entity> entities;
		Entity parent;
	};

	struct DeferredRemoveEntitiesFromHierarchy {
		Stream<Entity> entities;
		bool default_destroy_children;
	};

	struct DeferredTryRemoveEntitiesFromHierarchy {
		Stream<Entity> entities;
		bool default_destroy_children;
	};

	struct DeferredChangeEntityParentHierarchy {
		Stream<EntityPair> pairs;
	};

	struct DeferredChangeOrSetEntityParentHierarchy {
		Stream<EntityPair> pairs;
	};
	
	enum DeferredActionType : unsigned char {
		DEFERRED_ENTITY_WRITE_COMPONENT,
		DEFERRED_ENTITY_WRITE_ARCHETYPE,
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
		DEFERRED_COPY_ENTITIES,
		DEFERRED_DELETE_ENTITIES,
		DEFERRED_ENTITY_ADD_SHARED_COMPONENT,
		DEFERRED_ENTITY_REMOVE_SHARED_COMPONENT,
		DEFERRED_ENTITY_CHANGE_SHARED_INSTANCE,
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
		DEFERRED_BIND_NAMED_SHARED_INSTANCE,
		DEFERRED_DESTROY_SHARED_INSTANCE,
		DEFERRED_DESTROY_NAMED_SHARED_INSTANCE,
		DEFERRED_DESTROY_UNREFERENCED_SHARED_SINGLE_INSTANCE,
		DEFERRED_DESTROY_UNREFERENCED_SHARED_INSTANCES,
		DEFERRED_CLEAR_ENTITY_TAGS,
		DEFERRED_SET_ENTITY_TAGS,
		DEFERRED_ADD_ENTITIES_TO_HIERARCHY_PAIRS,
		DEFERRED_ADD_ENTITIES_TO_HIERARCHY_CONTIGUOUS,
		DEFERRED_ADD_ENTITIES_TO_PARENT_HIERARCHY,
		DEFERRED_REMOVE_ENTITIES_FROM_HIERARCHY,
		DEFERRED_TRY_REMOVE_ENTITIES_FROM_HIERARCHY,
		DEFERRED_CHANGE_ENTITY_PARENT_HIERARCHY,
		DEFERRED_CHANGE_OR_SET_ENTITY_PARENT_HIERARCHY,
		DEFERRED_CALLBACK_COUNT
	};

#pragma region Helpers

	void WriteCommandStream(EntityManager* manager, DeferredActionParameters parameters, DeferredAction action) {
		if (parameters.command_stream == nullptr) {
			unsigned int index = manager->m_deferred_actions.Add(action);
			ECS_CRASH_RETURN(index < manager->m_deferred_actions.capacity, "EntityManager: Insufficient space for the entity manager command stream.");
		}
		else {
			ECS_CRASH_RETURN(parameters.command_stream->size == parameters.command_stream->capacity, "EntityManager: Insufficient space for the user given command stream.");
			parameters.command_stream->Add(action);
		}
	}

	void GetComponentSizes(unsigned int* sizes, ComponentSignature signature, const ComponentInfo* info) {
		for (size_t index = 0; index < signature.count; index++) {
			sizes[index] = info[signature.indices[index].value].size;
		}
	}

	unsigned int GetComponentsSizePerEntity(const unsigned int* sizes, unsigned char count) {
		short total = 0;
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
		unsigned int* component_sizes, 
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

	void CreateAllocatorForComponent(EntityManager* entity_manager, ComponentInfo& info, size_t allocator_size) {
		if (allocator_size > 0) {
			// Allocate the allocator
			size_t total_allocation_size = sizeof(MemoryArena) + MemoryArena::MemoryOf(allocator_size, COMPONENT_ALLOCATOR_ARENA_COUNT, COMPONENT_ALLOCATOR_BLOCK_COUNT);
			void* allocation = entity_manager->m_memory_manager->Allocate(total_allocation_size);
			info.allocator = (MemoryArena*)allocation;
			*info.allocator = MemoryArena(function::OffsetPointer(allocation, sizeof(MemoryArena)), allocator_size, COMPONENT_ALLOCATOR_ARENA_COUNT, COMPONENT_ALLOCATOR_BLOCK_COUNT);
		}
		else {
			info.allocator = nullptr;
		}
	}

	const void** GetDeferredCallData(
		ComponentSignature components,
		const void** component_data,
		unsigned int entity_count,
		const unsigned int* component_sizes,
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
					buffer = function::AlignPointer(buffer, 8);
					data[component_index] = (void*)buffer;
					buffer += component_sizes[component_index] * entity_count;
				}
				buffer = buffer_after_pointers;

				if (type == by_entities_type) {
					// Pack the data into a contiguous stream for each component type
					for (size_t component_index = 0; component_index < components.count; component_index++) {
						buffer = function::AlignPointer(buffer, 8);
						for (size_t entity_index = 0; entity_index < entity_count; entity_index++) {
							memcpy((void*)buffer, component_data[entity_index * components.count + component_index], component_sizes[component_index]);
							buffer += component_sizes[component_index];
						}
					}
				}
				else if (type == by_entities_contiguous) {
					unsigned int* size_until_component = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * components.count);
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
						buffer = function::AlignPointer(buffer, 8);
						for (size_t entity_index = 0; entity_index < entity_count; entity_index++) {
							memcpy((void*)buffer, component_data[component_index * entity_count + entity_index], component_sizes[component_index]);
							buffer += component_sizes[component_index];
						}
					}
				}
				// COMPONENTS_SPLATTED
				else {
					for (size_t component_index = 0; component_index < components.count; component_index++) {
						buffer = function::AlignPointer(buffer, 8);
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
		Stream<Entity> entities, 
		unsigned int copy_position,
		EntityPool* entity_pool,
		unsigned int main_archetype_index,
		unsigned int base_archetype_index
	) {
		for (size_t entity_index = 0; entity_index < entities.size; entity_index++) {
			EntityInfo* entity_info = entity_pool->GetInfoPtr(entities[entity_index]);
			entity_info->main_archetype = main_archetype_index;
			entity_info->base_archetype = base_archetype_index;
			entity_info->stream_index = copy_position + entity_index;
		}
	}

#pragma region Write Component

	void CommitWriteComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredWriteComponent* data = (DeferredWriteComponent*)_data;
		ECS_STACK_CAPACITY_STREAM(unsigned int, component_sizes, ECS_ARCHETYPE_MAX_COMPONENTS);
		GetComponentSizes(component_sizes.buffer, data->write_signature, manager->m_unique_components.buffer);

		for (size_t index = 0; index < data->entities.size; index++) {
			for (unsigned char component_index = 0; component_index < data->write_signature.count; component_index++) {
				void* component = manager->GetComponent(data->entities[index], data->write_signature.indices[component_index]);
				memcpy(component, function::OffsetPointer(data->buffers[component_index], component_sizes[component_index] * index), component_sizes[component_index]);
			}
		}
	}

	void RegisterWriteComponent(
		EntityManager* manager, 
		Stream<Entity> entities, 
		ComponentSignature component_signature, 
		void** buffers,
		DeferredActionParameters parameters,
		DebugInfo debug_info
	) {
		ECS_STACK_CAPACITY_STREAM(unsigned int, component_sizes, ECS_ARCHETYPE_MAX_COMPONENTS);
		GetComponentSizes(component_sizes.buffer, component_signature, manager->m_unique_components.buffer);

		size_t allocation_size = sizeof(DeferredWriteComponent) + sizeof(Component) * component_signature.count;
		if (!parameters.entities_are_stable) {
			allocation_size += entities.MemoryOf(entities.size);
		}
		if (!parameters.data_is_stable) {
			for (unsigned char index = 0; index < component_signature.count; index++) {
				allocation_size += entities.size * component_sizes[index];
			}
			allocation_size += sizeof(void*) * component_signature.count;
		}

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t ptr = (uintptr_t)allocation;
		DeferredWriteComponent* data = (DeferredWriteComponent*)ptr;
		ptr += sizeof(*data);

		data->entities = GetEntitiesFromActionParameters(entities, parameters, ptr);
		data->write_signature = component_signature.Copy(ptr);
		
		if (!parameters.data_is_stable) {
			data->buffers = (const void**)ptr;
			ptr += sizeof(void*) * component_signature.count;

			for (unsigned char component_index = 0; component_index < component_signature.count; component_index++) {
				data->buffers[component_index] = (const void*)ptr;
				memcpy((void*)ptr, buffers[component_index], component_sizes[component_index] * entities.size);
				ptr += component_sizes[component_index] * entities.size;
			}
		}
		else {
			data->buffers = (const void**)buffers;
		}
		WriteCommandStream(manager, parameters, { DataPointer(allocation, DEFERRED_ENTITY_WRITE_COMPONENT), debug_info });
	}

#pragma endregion

#pragma region Write Archetype

	void CommitWriteArchetype(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredWriteArchetypes* data = (DeferredWriteArchetypes*)_data;
		ECS_STACK_CAPACITY_STREAM(unsigned int, component_sizes, ECS_ARCHETYPE_MAX_COMPONENTS);
		GetComponentSizes(component_sizes.buffer, data->write_signature, manager->m_unique_components.buffer);

		for (size_t index = 0; index < data->archetypes.size; index++) {
			ArchetypeBase* base_archetype = manager->GetBase(data->archetypes[index].x, data->archetypes[index].y);
			ECS_STACK_CAPACITY_STREAM(void*, archetype_buffers, ECS_ARCHETYPE_MAX_COMPONENTS);
			base_archetype->GetBuffers(archetype_buffers.buffer, data->write_signature);
			unsigned int entity_count = base_archetype->m_size;

			for (unsigned char component_index = 0; component_index < data->write_signature.count; component_index++) {
				memcpy(archetype_buffers[component_index], data->buffers[index][component_index], component_sizes[component_index] * entity_count);
			}
		}
	}

	void RegisterWriteArchetype(
		EntityManager* manager,
		Stream<ushort2> archetypes,
		ComponentSignature component_signature,
		Stream<void**> buffers,
		DeferredActionParameters parameters,
		DebugInfo debug_info
	) {
		ECS_STACK_CAPACITY_STREAM(unsigned int, component_sizes, ECS_ARCHETYPE_MAX_COMPONENTS);
		GetComponentSizes(component_sizes.buffer, component_signature, manager->m_unique_components.buffer);

		size_t allocation_size = sizeof(DeferredWriteArchetypes) + sizeof(Component) * component_signature.count + sizeof(ushort2) * archetypes.size;
		if (!parameters.data_is_stable) {
			for (unsigned int index = 0; index < archetypes.size; index++) {
				const ArchetypeBase* base = manager->GetBase(archetypes[index].x, archetypes[index].y);
				unsigned int size = base->m_size;

				for (unsigned char component_index = 0; component_index < component_signature.count; component_index++) {
					allocation_size += size * component_sizes[component_index];
				}
			}
			allocation_size += sizeof(void**) * archetypes.size + sizeof(void*) * archetypes.size * component_signature.count;
		}

		void* allocation = manager->AllocateTemporaryBuffer(allocation_size);
		uintptr_t ptr = (uintptr_t)allocation;
		DeferredWriteArchetypes* data = (DeferredWriteArchetypes*)ptr;
		ptr += sizeof(*data);

		data->write_signature = component_signature.Copy(ptr);

		if (!parameters.data_is_stable) {
			data->buffers.buffer = (void***)ptr;
			data->buffers.size = archetypes.size;
			ptr += sizeof(void**) * archetypes.size;

			for (unsigned int index = 0; index < archetypes.size; index++) {
				data->buffers[index] = (void**)ptr;
				ptr += sizeof(void*) * component_signature.count;
			}

			for (unsigned int index = 0; index < archetypes.size; index++) {
				ArchetypeBase* base = manager->GetBase(archetypes[index].x, archetypes[index].y);

				for (unsigned char component_index = 0; component_index < component_signature.count; component_index++) {
					data->buffers[index][component_index] = (void*)ptr;
					memcpy((void*)ptr, buffers[component_index], component_sizes[component_index] * base->m_size);
					ptr += component_sizes[component_index] * base->m_size;
				}
			}
		}
		else {
			data->buffers = buffers;
		}
		WriteCommandStream(manager, parameters, { DataPointer(allocation, DEFERRED_ENTITY_WRITE_ARCHETYPE), debug_info });
	}

#pragma endregion

#pragma region Add component

	struct CommitEntityAddComponentAdditionalData {
		uint2 archetype_indices;
		unsigned int copy_position;
	};

	// Can specifiy get_chunk_positions true in order to fill the additional_data interpreted as a CapacityStream<uint3>*
	// with the chunk positions of the entities
	template<bool get_copy_position>
	void CommitEntityAddComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddComponentEntities* data = (DeferredAddComponentEntities*)_data;

		Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];

		ComponentSignature unique_signature = manager->GetEntitySignature(data->entities[0], unique_components);
		SharedComponentSignature shared_signature = manager->GetEntitySharedSignatureStable(data->entities[0]);

		ComponentSignature initial_signature = unique_signature;
		memcpy(unique_components + unique_signature.count, data->components.indices, data->components.count * sizeof(Component));
		unique_signature.count += data->components.count;

		uint2 archetype_indices = manager->FindOrCreateArchetypeBase(unique_signature, shared_signature);
		ArchetypeBase* base_archetype = manager->GetBase(archetype_indices.x, archetype_indices.y);

		EntityInfo entity_info = manager->GetEntityInfo(data->entities[0]);
		ArchetypeBase* old_archetype = manager->GetBase(entity_info.main_archetype, entity_info.base_archetype);
		if constexpr (!get_copy_position) {
			// If the copy position is not needed, it means we must update the entities
			unsigned int copy_position = base_archetype->AddEntities(data->entities);

			// The old component data must be transfered to the new archetype
			base_archetype->CopyFromAnotherArchetype(
				{copy_position, (unsigned int)data->entities.size},
				old_archetype,
				data->entities.buffer,
				manager->m_entity_pool,
				initial_signature
			);
			// Remove the entities from the old archetype
			for (size_t index = 0; index < data->entities.size; index++) {
				old_archetype->RemoveEntity(data->entities[index], manager->m_entity_pool);
			}

			UpdateEntityInfos(data->entities, copy_position, manager->m_entity_pool, archetype_indices.x, archetype_indices.y);
		}
		else {
			CommitEntityAddComponentAdditionalData* additional_data = (CommitEntityAddComponentAdditionalData*)_additional_data;
			unsigned int copy_position = base_archetype->AddEntities(data->entities);
			// Let the caller update the entity infos
			additional_data->archetype_indices = archetype_indices;
			additional_data->copy_position = copy_position;
			
			base_archetype->CopyFromAnotherArchetype(
				{copy_position, (unsigned int)data->entities.size},
				old_archetype,
				data->entities.buffer,
				manager->m_entity_pool,
				initial_signature
			);
		}
	}

	template<typename CopyFunction>
	void CommitEntityAddComponentWithData(EntityManager* manager, void* _data, CopyFunction&& copy_function) {
		CommitEntityAddComponentAdditionalData additional_data = {};
		CommitEntityAddComponent<true>(manager, _data, &additional_data);

		// The entity addition was commited by the previous call
		DeferredAddComponentEntities* data = (DeferredAddComponentEntities*)_data;
		// We need to copy the data and to update the entity infos
		ArchetypeBase* base_archetype = manager->GetBase(additional_data.archetype_indices.x, additional_data.archetype_indices.y);

		copy_function(base_archetype, uint2(additional_data.copy_position, data->entities.size), data->data, data->components);

		// Remove the entities from the old archetype
		EntityInfo entity_info = manager->GetEntityInfo(data->entities[0]);
		ArchetypeBase* old_archetype = manager->GetBase(entity_info.main_archetype, entity_info.base_archetype);
		for (size_t index = 0; index < data->entities.size; index++) {
			old_archetype->RemoveEntity(data->entities[index], manager->m_entity_pool);
		}

		UpdateEntityInfos(data->entities, additional_data.copy_position, manager->m_entity_pool, additional_data.archetype_indices.x, additional_data.archetype_indices.y);
	}

	void CommitEntityAddComponentSplatted(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopySplatComponents(copy_position, data, components);
		});
	}

	void CommitEntityAddComponentByEntities(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntity(copy_position, data, components);
		});
	}

	void CommitEntityAddComponentScattered(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponents(copy_position, data, components);
		});
	}

	void CommitEntityAddComponentContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponentsContiguous(copy_position, data, components);
		});
	}

	void CommitEntityAddComponentByEntitiesContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntityContiguous(copy_position, data, components);
		});
	}

	void RegisterEntityAddComponent(
		EntityManager* manager,
		Stream<Entity> entities,
		ComponentSignature components,
		const void** component_data,
		DeferredActionType type,
		DeferredActionParameters parameters,
		DebugInfo debug_info
	) {
		unsigned int* component_sizes = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * components.count);

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

		WriteCommandStream(manager, parameters, { DataPointer(allocation, type), debug_info });
	}

#pragma endregion

#pragma region Remove Components

	void CommitEntityRemoveComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredRemoveComponentEntities* data = (DeferredRemoveComponentEntities*)_data;

		Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];

		ComponentSignature unique_signature = manager->GetEntitySignature(data->entities[0], unique_components);
		SharedComponentSignature shared_signature = manager->GetEntitySharedSignatureStable(data->entities[0]);

		// Determine which is the final signature
		for (size_t index = 0; index < data->components.count; index++) {
			size_t subindex = 0;
			for (; subindex < unique_signature.count; subindex++) {
				if (unique_signature.indices[subindex] == data->components.indices[index]) {
					unique_signature.count--;
					unique_signature.indices[subindex] = unique_signature.indices[unique_signature.count];
					subindex--;
					break;
				}
			}
			// If the component was not found, fail
			ECS_CRASH_RETURN(
				subindex != unique_signature.count, 
				"EntityManager: Could not find component {#} when trying to remove components from entities. First entity is {#}.",
				manager->GetComponentName(data->components.indices[index]),
				data->entities[0].index
			);
		}

		uint2 archetype_indices = manager->FindOrCreateArchetypeBase(unique_signature, shared_signature);
		ArchetypeBase* base_archetype = manager->GetBase(archetype_indices.x, archetype_indices.y);

		EntityInfo entity_info = manager->GetEntityInfo(data->entities[0]);
		ArchetypeBase* old_archetype = manager->GetBase(entity_info.main_archetype, entity_info.base_archetype);

		unsigned int copy_position = base_archetype->AddEntities(data->entities);

		// The old component data must be transfered to the new archetype
		base_archetype->CopyFromAnotherArchetype(
			{copy_position, (unsigned int)data->entities.size},
			old_archetype,
			data->entities.buffer,
			manager->m_entity_pool,
			unique_signature
		);
		// Remove the entities from the old archetype
		for (size_t index = 0; index < data->entities.size; index++) {
			old_archetype->RemoveEntity(data->entities[index], manager->m_entity_pool);
		}

		UpdateEntityInfos(data->entities, copy_position, manager->m_entity_pool, archetype_indices.x, archetype_indices.y);
	}

#pragma endregion

#pragma region Create Entities

	struct CommitCreateEntitiesAdditionalData {
		uint2 archetype_indices;
		unsigned int copy_position;
		Stream<Entity> entities;
	};

	enum CreateEntitiesTemplateType {
		CREATE_ENTITIES_TEMPLATE_BASIC,
		CREATE_ENTITIES_TEMPLATE_GET_ENTITIES,
	};

	// Can specifiy get_chunk_positions true in order to fill the additional_data interpreted as a CapacityStream<uint3>*
	// with the chunk positions of the entities
	template<CreateEntitiesTemplateType type>
	void CommitCreateEntities(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateEntities* data = (DeferredCreateEntities*)_data;
		CommitCreateEntitiesAdditionalData* additional_data = (CommitCreateEntitiesAdditionalData*)_additional_data;

		uint2 archetype_indices = manager->FindOrCreateArchetypeBase(data->unique_components, data->shared_components);
		ArchetypeBase* base_archetype = manager->GetBase(archetype_indices.x, archetype_indices.y);

		const size_t MAX_ENTITY_INFOS_ON_STACK = ECS_KB * 64;

		Entity* entities = nullptr;

		if constexpr (type != CREATE_ENTITIES_TEMPLATE_GET_ENTITIES) {
			// Fill in a buffer with the corresponding entity infos
			if (data->count < MAX_ENTITY_INFOS_ON_STACK) {
				entities = (Entity*)ECS_STACK_ALLOC(sizeof(Entity) * data->count);
			}
			else {
				entities = (Entity*)manager->m_small_memory_manager.Allocate(sizeof(Entity) * data->count);
			}
		}
		else {
			entities = additional_data->entities.buffer;
		}

		// If the chunk positions are not needed, it means we must update the entities
		unsigned int copy_position = base_archetype->Reserve(data->count);

		// Create the entities from the entity pool - their infos will already be set
		manager->m_entity_pool->AllocateEx({ entities, data->count }, archetype_indices, copy_position);
		
		// Copy the entities into the archetype entity reference
		base_archetype->SetEntities({ entities, data->count }, copy_position);
		base_archetype->m_size += data->count;

		if (!data->exclude_from_hierarchy) {
			// Add the entities as roots
			manager->AddEntitiesToParentCommit({ entities, data->count }, Entity(-1));
		}

		additional_data->copy_position = copy_position;
		additional_data->archetype_indices = archetype_indices;
		if constexpr (type != CREATE_ENTITIES_TEMPLATE_GET_ENTITIES) {
			if (data->count >= MAX_ENTITY_INFOS_ON_STACK) {
				manager->m_small_memory_manager.Deallocate(entities);
			}
		}
	}

	template<typename CopyFunction>
	void CommitCreateEntitiesWithData(EntityManager* manager, void* _data, void* _additional_data, CopyFunction&& copy_function) {
		CommitCreateEntitiesAdditionalData additional_data = {};
		if (_additional_data != nullptr) {
			CommitCreateEntitiesAdditionalData* __additional_data = (CommitCreateEntitiesAdditionalData*)_additional_data;
			additional_data.entities = __additional_data->entities;
			CommitCreateEntities<CREATE_ENTITIES_TEMPLATE_GET_ENTITIES>(manager, _data, &additional_data);
		}
		else {
			CommitCreateEntities<CREATE_ENTITIES_TEMPLATE_BASIC>(manager, _data, &additional_data);
		}

		// The entity addition was commit by the previous call
		DeferredCreateEntities* data = (DeferredCreateEntities*)_data;
		// We need only to copy the data
		ArchetypeBase* base_archetype = manager->GetBase(additional_data.archetype_indices.x, additional_data.archetype_indices.y);
		copy_function(base_archetype, { additional_data.copy_position, data->count }, data->data, data->components_with_data);

		if (data->copy_buffers) {
			Archetype* archetype = manager->GetArchetype(additional_data.archetype_indices.x);
			if (archetype->m_unique_components_to_deallocate_count > 0) {
				for (unsigned char deallocate_index = 0; deallocate_index < archetype->m_unique_components_to_deallocate_count; deallocate_index++) {
					for (unsigned int index = 0; index < data->count; index++) {
						void* target_component = base_archetype->GetComponentByIndex(additional_data.copy_position + index, archetype->m_unique_components_to_deallocate[deallocate_index]);
						archetype->CopyEntityBuffers(additional_data.copy_position + index, additional_data.archetype_indices.y, deallocate_index, target_component);
					}
				}
			}
		}
	}

	void CommitCreateEntitiesDataSplatted(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopySplatComponents(copy_position, data, components);
		});
	}

	void CommitCreateEntitiesDataByEntities(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntity(copy_position, data, components);
		});
	}

	void CommitCreateEntitiesDataScattered(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponents(copy_position, data, components);
		});
	}

	void CommitCreateEntitiesDataContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponentsContiguous(copy_position, data, components);
		});
	}

	void CommitCreateEntitiesDataByEntitiesContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntityContiguous(copy_position, data, components);
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
		bool exclude_from_hierarchy,
		bool copy_buffers,
		DeferredActionParameters parameters,
		DebugInfo debug_info
	) {
		
	}

#pragma endregion

#pragma region Copy entities

	void CommitCopyEntities(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCopyEntities* data = (DeferredCopyEntities*)_data;
		Entity* entities = (Entity*)_additional_data;

		// We need an entities buffer anyway because we must add the entities to their corresponding
		if (entities == nullptr) {
			entities = (Entity*)ECS_STACK_ALLOC(sizeof(Entity) * data->count);
		}

		ComponentSignature unique;
		SharedComponentSignature shared;
		manager->GetEntityCompleteSignatureStable(data->entity, &unique, &shared);

		void* unique_data[ECS_ARCHETYPE_MAX_COMPONENTS];
		manager->GetComponent(data->entity, unique, unique_data);

		manager->CreateEntitiesCommit(data->count, unique, shared, unique, (const void**)unique_data, ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_SPLAT, true, true, entities);
		manager->AddEntitiesToParentCommit({ entities, data->count }, manager->GetEntityParent(data->entity));
		
		// Now copy its children recursively - if enabled
		if (data->copy_children) {
			Entity* temporary_entities = (Entity*)ECS_STACK_ALLOC(data->count * sizeof(Entity));
			Entity temporary_static_entities[ECS_ENTITY_HIERARCHY_STATIC_STORAGE * 2];
			size_t temporary_static_count = 0;

			struct ParentWithChildren {
				Entity* copied_parents;
				Stream<Entity> children;
			};
			ResizableQueue<ParentWithChildren> process_queue(GetAllocatorPolymorphic(&manager->m_small_memory_manager), 128);
			process_queue.Push({ entities, manager->GetHierarchyChildren(data->entity, temporary_static_entities + temporary_static_count * ECS_ENTITY_HIERARCHY_STATIC_STORAGE) });
			temporary_static_count = 1;

			ParentWithChildren current_data;
			while (process_queue.Pop(current_data)) {
				Stream<Entity> children = current_data.children;

				for (size_t index = 0; index < children.size; index++) {					
					manager->GetEntityCompleteSignatureStable(children[index], &unique, &shared);
					manager->GetComponent(children[index], unique, unique_data);
								
					Entity* current_entities = nullptr;
					Stream<Entity> current_children = manager->GetHierarchyChildren(children[index], temporary_static_entities + temporary_static_count * ECS_ENTITY_HIERARCHY_STATIC_STORAGE);
					temporary_static_count = temporary_static_count == 1 ? 0 : 1;
					if (current_children.size > 0) {
						// Allocate a buffer that can hold this allocation such we can reference the entities later on
						current_entities = (Entity*)manager->m_memory_manager->Allocate(sizeof(Entity) * data->count);
						ParentWithChildren new_data;
						new_data.children = current_children;
						new_data.copied_parents = current_entities;
						process_queue.Push(new_data);
					}
					else {
						current_entities = temporary_entities;
					}

					manager->CreateEntitiesCommit(data->count, unique, shared, unique, (const void**)unique_data, ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_SPLAT, true, true, current_entities);
					manager->AddEntityToHierarchyCommit(current_data.copied_parents, current_entities, data->count);
				}

				// Deallocate the copied_parents buffer if it belongs
				if (manager->m_memory_manager->Belongs(current_data.copied_parents)) {
					manager->m_memory_manager->Deallocate(current_data.copied_parents);
				}
			}
			process_queue.FreeBuffer();
		}
	}

#pragma endregion

#pragma region Delete Entities

	void CommitDeleteEntities(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDeleteEntities* data = (DeferredDeleteEntities*)_data;

		for (size_t index = 0; index < data->entities.size; index++) {
			EntityInfo info = manager->GetEntityInfo(data->entities[index]);
			ArchetypeBase* base_archetype = manager->GetBase(info.main_archetype, info.base_archetype);
			const Archetype* main_archetype = manager->GetArchetype(info.main_archetype);
			
			// Dealocate any buffers this entity might have
			main_archetype->DeallocateEntityBuffers(info);

			// Remove the entities from the archetype
			base_archetype->RemoveEntity(info.stream_index, manager->m_entity_pool);
			
			// Remove the entity from the hierarchy
			manager->RemoveEntityFromHierarchyCommit({ data->entities.buffer + index, 1 });
		}
		manager->m_entity_pool->Deallocate(data->entities);
	}

#pragma endregion

#pragma region Add Shared Component

	void CommitEntityAddSharedComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddSharedComponentEntities* data = (DeferredAddSharedComponentEntities*)_data;

		EntityInfo info = manager->GetEntityInfo(data->entities[0]);
		Archetype* old_archetype = manager->GetArchetype(info.main_archetype);
		ArchetypeBase* old_base_archetype = old_archetype->GetBase(info.base_archetype);

		Component _shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		SharedInstance _shared_instances[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

		ComponentSignature unique_signature = old_archetype->GetUniqueSignature();
		ComponentSignature _shared_signature = old_archetype->GetSharedSignature();
		SharedComponentSignature shared_signature(_shared_components, _shared_instances, 0);
		memcpy(shared_signature.indices, _shared_signature.indices, sizeof(Component) * _shared_signature.count);
		const SharedInstance* current_instances = old_archetype->GetBaseInstances(info.base_archetype);
		memcpy(shared_signature.instances, current_instances, sizeof(SharedInstance) * _shared_signature.count);
		shared_signature.count = _shared_signature.count;

		memcpy(shared_signature.indices + shared_signature.count, data->components.indices, sizeof(Component) * data->components.count);
		memcpy(shared_signature.instances + shared_signature.count, data->components.instances, sizeof(SharedInstance) * data->components.count);
		shared_signature.count += data->components.count;

		uint2 new_archetype_indices = manager->FindOrCreateArchetypeBase(unique_signature, shared_signature);
		ArchetypeBase* new_archetype = manager->GetBase(new_archetype_indices.x, new_archetype_indices.y);

		unsigned int copy_position = new_archetype->AddEntities(data->entities);
		// Copy the data now from the other archetype
		new_archetype->CopyFromAnotherArchetype({ copy_position, (unsigned int)data->entities.size }, old_base_archetype, data->entities.buffer, manager->m_entity_pool, unique_signature);

		// Remove the entities from the archetype
		for (size_t index = 0; index < data->entities.size; index++) {
			old_base_archetype->RemoveEntity(data->entities[index], manager->m_entity_pool);
		}
		UpdateEntityInfos(data->entities, copy_position, manager->m_entity_pool, new_archetype_indices.x, new_archetype_indices.y);
	}

#pragma endregion

#pragma region Remove Shared Component

	void CommitEntityRemoveSharedComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredRemoveSharedComponentEntities* data = (DeferredRemoveSharedComponentEntities*)_data;

		EntityInfo info = manager->GetEntityInfo(data->entities[0]);
		Archetype* old_archetype = manager->GetArchetype(info.main_archetype);
		ArchetypeBase* old_base_archetype = old_archetype->GetBase(info.base_archetype);

		Component _shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		SharedInstance _shared_instances[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

		ComponentSignature unique_signature = old_archetype->GetUniqueSignature();
		ComponentSignature _shared_signature = old_archetype->GetSharedSignature();
		SharedComponentSignature shared_signature(_shared_components, _shared_instances, 0);
		memcpy(shared_signature.indices, _shared_signature.indices, sizeof(Component) * _shared_signature.count);
		const SharedInstance* current_instances = old_archetype->GetBaseInstances(info.base_archetype);
		memcpy(shared_signature.instances, current_instances, sizeof(SharedInstance) * _shared_signature.count);
		shared_signature.count = _shared_signature.count;

		for (size_t index = 0; index < data->components.count; index++) {
			size_t subindex = 0;
			for (; subindex < shared_signature.count; subindex++) {
				if (shared_signature.indices[subindex] == data->components.indices[index]) {
					shared_signature.count--;
					shared_signature.indices[subindex] = shared_signature.indices[shared_signature.count];
					shared_signature.instances[subindex] = shared_signature.instances[shared_signature.count];
					// Decrement the subindex as well in order to not crash when exiting when this is the last element
					subindex--;
					break;
				}
			}
			// Fail if the component doesn't exist
			ECS_CRASH_RETURN(subindex < data->components.count || subindex == -1, "EntityManager: A component could not be found when trying to remove components from entities. "
				"The first entity is {#}. The component is {#}.", data->entities[0].value, manager->GetComponentName(data->components.indices[index]));
		}

		uint2 new_archetype_indices = manager->FindOrCreateArchetypeBase(unique_signature, shared_signature);
		ArchetypeBase* new_archetype = manager->GetBase(new_archetype_indices.x, new_archetype_indices.y);

		unsigned int copy_position = new_archetype->AddEntities(data->entities);
		// Copy the components from the old archetype
		new_archetype->CopyFromAnotherArchetype({ copy_position, (unsigned int)data->entities.size }, old_base_archetype, data->entities.buffer, manager->m_entity_pool, unique_signature);

		// Remove the entities from the old archetype
		for (size_t index = 0; index < data->entities.size; index++) {
			old_base_archetype->RemoveEntity(data->entities[index], manager->m_entity_pool);
		}
		UpdateEntityInfos(data->entities, copy_position, manager->m_entity_pool, new_archetype_indices.x, new_archetype_indices.y);
	}

#pragma endregion

#pragma region Change Entity Shared Instance

	void CommitEntityChangeSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredChangeSharedInstanceEntities* data = (DeferredChangeSharedInstanceEntities*)_data;

		for (size_t index = 0; index < data->elements.size; index++) {
			SharedComponentSignature shared_signature = { &data->elements[index].component, &data->elements[index].new_instance, 1 };

			ECS_CRASH_RETURN(
				manager->ExistsComponent(data->elements[index].component),
				"EntityManager: The shared component {#} does not exist when trying to change the shared instance for entity {#}.",
				manager->GetSharedComponentName(data->elements[index].component),
				data->elements[index].entity
			);

			ECS_CRASH_RETURN(
				manager->ExistsSharedInstanceOnly(data->elements[index].component, data->elements[index].new_instance),
				"EntityManager: The shared instance {#} does not exist for component {#} when trying to change the shared instance for entity {#}.",
				data->elements[index].new_instance,
				manager->GetSharedComponentName(data->elements[index].component),
				data->elements[index].entity
			);

			EntityInfo* entity_info = manager->m_entity_pool->GetInfoPtr(data->elements[index].entity);

			Archetype* archetype = manager->GetArchetype(entity_info->main_archetype);
			unsigned int previous_base_index = entity_info->base_archetype;
			unsigned int new_base_index = archetype->FindBaseIndex(shared_signature);

			if (new_base_index == -1) {
				new_base_index = manager->CreateArchetypeBaseCommit(entity_info->main_archetype, shared_signature);
			}

			ArchetypeBase* old_archetype_base = archetype->GetBase(previous_base_index);
			ArchetypeBase* new_archetype_base = archetype->GetBase(new_base_index);
			
			unsigned int allocated_position = new_archetype_base->AddEntity(data->elements[index].entity);
			new_archetype_base->CopyFromAnotherArchetype({ allocated_position, 1 }, old_archetype_base, &data->elements[index].entity, manager->m_entity_pool);
			old_archetype_base->RemoveEntity(entity_info->stream_index, manager->m_entity_pool);

			entity_info->base_archetype = new_base_index;
			entity_info->stream_index = allocated_position;

			if (data->destroy_base_archetype && old_archetype_base->EntityCount() == 0) {
				archetype->DestroyBase(previous_base_index, manager->m_entity_pool);
			}
		}
	}

#pragma endregion

#pragma region Create Archetype

	void CommitCreateArchetype(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateArchetype* data = (DeferredCreateArchetype*)_data;

		size_t old_capacity = manager->m_archetypes.capacity;
		unsigned int archetype_index = manager->m_archetypes.Add(
			Archetype(
				&manager->m_small_memory_manager,
				manager->m_memory_manager,
				manager->m_unique_components.buffer,
				data->unique_components,
				data->shared_components
			)
		);

		// If the archetypes stream resized, do that for the vector components aswell
		if (old_capacity != manager->m_archetypes.capacity) {
			// If the components are small, copy to a stack buffer, release the current buffer and then
			// request a new buffer. In this one the pressure on the allocator is reduced
			size_t new_size = sizeof(VectorComponentSignature) * 2 * manager->m_archetypes.capacity;
			if (manager->m_archetypes.capacity < ECS_KB * 2) {
				// Copy to the stack buffer
				void* stack_buffer = ECS_STACK_ALLOC(new_size);
				
				size_t copy_size = sizeof(VectorComponentSignature) * 2 * (manager->m_archetypes.size - 1);
				memcpy(stack_buffer, manager->m_archetype_vector_signatures, copy_size);
				manager->m_small_memory_manager.Deallocate(manager->m_archetype_vector_signatures);
				manager->m_archetype_vector_signatures = (VectorComponentSignature*)manager->m_small_memory_manager.Allocate(new_size);
				memcpy(manager->m_archetype_vector_signatures, stack_buffer, copy_size);
			}
			else {
				// Allocate directly for a new buffer
				void* new_buffer = manager->m_small_memory_manager.Allocate(new_size);
				memcpy(new_buffer, manager->m_archetype_vector_signatures, sizeof(VectorComponentSignature) * 2 * (manager->m_archetypes.size - 1));
				manager->m_small_memory_manager.Deallocate(manager->m_archetype_vector_signatures);
				manager->m_archetype_vector_signatures = (VectorComponentSignature*)new_buffer;
			}
		}

		// Set the vector components
		manager->GetArchetypeUniqueComponentsPtr(archetype_index)->ConvertComponents(data->unique_components);
		manager->GetArchetypeSharedComponentsPtr(archetype_index)->ConvertComponents(data->shared_components);

		// Update all the queries to reflect the change
		manager->m_query_cache->UpdateAdd(archetype_index);

		if (_additional_data != nullptr) {
			unsigned int* additional_data = (unsigned int*)_additional_data;
			*additional_data = archetype_index;
		}
	}

#pragma endregion

#pragma region Destroy Archetype

	template<bool use_component_search>
	void CommitDestroyArchetype(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyArchetype* data = (DeferredDestroyArchetype*)_data;

		unsigned int archetype_index = data->archetype_index;
		if constexpr (use_component_search) {
			archetype_index = manager->FindArchetype({ data->unique_components, data->shared_components });
		}
		// Fail if it doesn't exist
		ECS_CRASH_RETURN(archetype_index != -1, "EntityManager: Could not find archetype when trying to destroy it.");

		Archetype* archetype = manager->GetArchetype(archetype_index);
		// If it has components with buffers, those need to be deallocated manually
		if (archetype->m_unique_components_to_deallocate_count > 0) {
			archetype->DeallocateEntityBuffers();
		}

		EntityPool* pool = manager->m_entity_pool;
		// Do a search and eliminate every entity inside the base archetypes
		for (unsigned int index = 0; index < archetype->GetBaseCount(); index++) {
			const ArchetypeBase* base = archetype->GetBase(index);
			pool->Deallocate({ base->m_entities, base->m_size });
		}

		// Now the archetype can be safely deallocated
		archetype->Deallocate();

		manager->m_archetypes.RemoveSwapBack(archetype_index);

		// Update infos for the entities from the archetype that got swaped in its place
		// unless it is the last one
		if (archetype_index != manager->m_archetypes.size) {
			// Also update the vector components
			*manager->GetArchetypeUniqueComponentsPtr(archetype_index) = manager->GetArchetypeUniqueComponents(manager->m_archetypes.size);
			*manager->GetArchetypeSharedComponentsPtr(archetype_index) = manager->GetArchetypeSharedComponents(manager->m_archetypes.size);

			for (size_t base_index = 0; base_index < archetype->GetBaseCount(); base_index++) {
				const ArchetypeBase* base = archetype->GetBase(base_index);
				for (size_t entity_index = 0; entity_index < base->m_size; entity_index++) {
					EntityInfo* entity_info = manager->m_entity_pool->GetInfoPtr(base->m_entities[entity_index]);
					entity_info->main_archetype = archetype_index;
				}
			}
		}

		// TODO: is it worth trimming? The memory saving is small and might induce fragmentation
		bool has_trimmed = manager->m_archetypes.Trim(10);
		if (has_trimmed) {
			// Resize the vector components
			size_t allocate_count = manager->m_archetypes.capacity;
			size_t new_size = sizeof(VectorComponentSignature) * 2 * allocate_count;

			if (new_size < ECS_KB * 32) {
				// Copy to a stack buffer and release the old buffer rightaway - It will clear space inside the allocator
				// And hopefully reduce the pressure on it
				void* temp_buffer = ECS_STACK_ALLOC(new_size);
				memcpy(temp_buffer, manager->m_archetype_vector_signatures, new_size);
				manager->m_small_memory_manager.Deallocate(manager->m_archetype_vector_signatures);
				manager->m_archetype_vector_signatures = (VectorComponentSignature*)manager->m_small_memory_manager.Allocate(new_size);
				memcpy(manager->m_archetype_vector_signatures, temp_buffer, new_size);
			}
			else {
				// Allocate a new buffer and release the old afterwards
				void* new_buffer = manager->m_small_memory_manager.Allocate(new_size);
				memcpy(new_buffer, manager->m_archetype_vector_signatures, new_size);
				manager->m_small_memory_manager.Deallocate(manager->m_archetype_vector_signatures);
				manager->m_archetype_vector_signatures = (VectorComponentSignature*)new_buffer;
			}
		}
	}

#pragma endregion

#pragma region Create Archetype Base

	void CommitCreateArchetypeBase(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateArchetypeBase* data = (DeferredCreateArchetypeBase*)_data;

		VectorComponentSignature vector_shared;
		vector_shared.InitializeSharedComponent(data->shared_components);
		unsigned int main_archetype_index = manager->FindArchetype({ data->unique_components, vector_shared });
		ECS_CRASH_RETURN(main_archetype_index != -1, "EntityManager: Could not find main archetype when trying to create base archetype.");

		Archetype* archetype = manager->GetArchetype(main_archetype_index);

		unsigned int base_index = archetype->CreateBaseArchetype(data->shared_components, data->starting_size);

		if (_additional_data != nullptr) {
			uint2* additional_data = (uint2*)_additional_data;
			*additional_data = { main_archetype_index, base_index };
		}
	}

#pragma endregion

#pragma region Destroy Archetype Base

	template<bool use_component_search>
	void CommitDestroyArchetypeBase(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyArchetypeBase* data = (DeferredDestroyArchetypeBase*)_data;

		Archetype* archetype = nullptr;
		unsigned int base_index = 0;

		if constexpr (use_component_search) {
			VectorComponentSignature vector_shared;
			vector_shared.InitializeSharedComponent(data->shared_components);
			VectorComponentSignature vector_instances;
			vector_instances.InitializeSharedInstances(data->shared_components);
			Archetype* archetype = manager->FindArchetypePtr({ data->unique_components, vector_shared });
			ECS_CRASH_RETURN(archetype != nullptr, "EntityManager: Could not find main archetype when trying to delete base from components.");

			unsigned int base_index = archetype->FindBaseIndex(vector_shared, vector_instances);
			ECS_CRASH_RETURN(base_index != -1, "EntityManager: Could not find base archetype index when trying to delete it from components.");
		}
		else {
			archetype = manager->GetArchetype(data->indices.x);
			base_index = data->indices.y;
		}

		const ArchetypeBase* base = archetype->GetBase(base_index);
		EntityPool* pool = manager->m_entity_pool;
		// Do a search and eliminate every entity inside the base archetypes
		pool->Deallocate({ base->m_entities, base->m_size });

		archetype->DestroyBase(base_index, manager->m_entity_pool);
	}

#pragma endregion

#pragma region Create Component

	void CommitCreateComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateComponent* data = (DeferredCreateComponent*)_data;

		ECS_STACK_CAPACITY_STREAM(char, component_name_storage, 16);
		Stream<char> component_name = data->name;
		if (data->name.size == 0) {
			function::ConvertIntToChars(component_name_storage, data->component.value);
			component_name = component_name_storage;
		}

		// Crash if the allocator is specified but the buffer offsets are not
		ECS_CRASH_RETURN(
			data->allocator_size == 0 || data->component_buffers_count > 0, 
			"EntityManager: Trying to create component {#} with an allocator but without buffer offsets.", 
			component_name
		);

		if (manager->m_unique_components.size <= data->component.value) {
			size_t old_capacity = manager->m_unique_components.size;
			void* allocation = manager->m_small_memory_manager.Allocate(sizeof(ComponentInfo) * (data->component.value + 1));
			manager->m_unique_components.CopyTo(allocation);
			manager->m_small_memory_manager.Deallocate(manager->m_unique_components.buffer);

			manager->m_unique_components.InitializeFromBuffer(allocation, data->component.value + 1);
			for (size_t index = old_capacity; index < manager->m_unique_components.size; index++) {
				manager->m_unique_components[index].size = -1;
			}

			// Update the component info reference to all archetypes and base archetypes
			for (unsigned int main_index = 0; main_index < manager->m_archetypes.size; main_index++) {
				Archetype* archetype = manager->GetArchetype(main_index);
				archetype->m_unique_infos = manager->m_unique_components.buffer;
				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					ArchetypeBase* base = archetype->GetBase(base_index);
					base->m_infos = manager->m_unique_components.buffer;
				}
			}
		}
		// If the size is different from -1, it means there is a component actually allocated to this slot
		ECS_CRASH_RETURN(!manager->ExistsComponent(data->component), "EntityManager: Creating component {#} at position {#} failed. It already exists.", 
			component_name, data->component.value);
		manager->m_unique_components[data->component.value].size = data->size;
		manager->m_unique_components[data->component.value].component_buffers_count = data->component_buffers_count;
		manager->m_unique_components[data->component.value].name = function::StringCopy(manager->SmallAllocator(), component_name);
		memcpy(manager->m_unique_components[data->component.value].component_buffers, data->component_buffers, sizeof(ComponentBuffer) * data->component_buffers_count);
		CreateAllocatorForComponent(manager, manager->m_unique_components[data->component.value], data->allocator_size);
	}

#pragma endregion

#pragma region Destroy Component

	void CommitDestroyComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyComponent* data = (DeferredDestroyComponent*)_data;

		ECS_CRASH_RETURN(data->component.value < manager->m_unique_components.size, "EntityManager: Incorrect component index {#} when trying to delete it.", data->component.value);
		// -1 Signals it's empty - a component was never associated to this slot
		ECS_CRASH_RETURN(manager->ExistsComponent(data->component), "EntityManager: Trying to destroy component {#} when it doesn't exist.", 
			data->component.value);

		// If it has an allocator, deallocate it
		if (manager->m_unique_components[data->component.value].allocator != nullptr) {
			manager->m_memory_manager->Deallocate(manager->m_unique_components[data->component.value].allocator);
			manager->m_unique_components[data->component.value].allocator = nullptr;
		}

		manager->m_small_memory_manager.Deallocate(manager->m_unique_components[data->component.value].name.buffer);
		manager->m_unique_components[data->component.value].size = -1;
	}

#pragma endregion

#pragma region Create shared component

	void CommitCreateSharedComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateSharedComponent* data = (DeferredCreateSharedComponent*)_data;

		ECS_STACK_CAPACITY_STREAM(char, component_name_storage, 64);
		Stream<char> component_name = data->name;
		if (data->name.size == 0) {
			function::ConvertIntToChars(component_name_storage, data->component.value);
			component_name = component_name_storage;
		}

		// Crash if the allocator size is specified but the buffer offsets are not
		ECS_CRASH_RETURN(data->allocator_size == 0 || data->component_buffers_count > 0,
			"EntityManager: Trying to create shared component {#} with an allocator but without buffer_offsets.", component_name);

		if (manager->m_shared_components.size <= data->component.value) {
			size_t old_capacity = manager->m_shared_components.size;
			void* allocation = manager->m_small_memory_manager.Allocate(sizeof(SharedComponentInfo) * (data->component.value + 1));
			manager->m_shared_components.CopyTo(allocation);
			manager->m_small_memory_manager.Deallocate(manager->m_shared_components.buffer);

			manager->m_shared_components.InitializeFromBuffer(allocation, data->component.value + 1);
			for (size_t index = old_capacity; index < manager->m_shared_components.size; index++) {
				manager->m_shared_components[index].info.size = -1;
			}
		}


		// If the size is different from -1, it means there is a component actually allocated to this slot
		ECS_CRASH_RETURN(manager->m_shared_components[data->component.value].info.size == -1, 
			"EntityManager: Trying to create shared component {#} when it already exists {#} at that slot.", component_name, manager->GetSharedComponentName(data->component));
		manager->m_shared_components[data->component.value].info.size = data->size;
		manager->m_shared_components[data->component.value].info.component_buffers_count = data->component_buffers_count;
		memcpy(manager->m_shared_components[data->component.value].info.component_buffers, data->component_buffers, sizeof(ComponentBuffer) * data->component_buffers_count);
		manager->m_shared_components[data->component.value].instances.Initialize(manager->SmallAllocator(), 0);
		manager->m_shared_components[data->component.value].named_instances.Initialize(manager->SmallAllocator(), 0);
		manager->m_shared_components[data->component.value].info.name = function::StringCopy(manager->SmallAllocator(), component_name);
		
		CreateAllocatorForComponent(manager, manager->m_shared_components[data->component.value].info, data->allocator_size);
		// The named instances table should start with size 0 and only create it when actually needing the tags
	}

#pragma endregion

#pragma region Destroy shared component

	void CommitDestroySharedComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroySharedComponent* data = (DeferredDestroySharedComponent*)_data;

		ECS_CRASH_RETURN(data->component.value < manager->m_shared_components.size, "EntityManager: Invalid shared component {#} when trying to destroy it.", data->component.value);
		// -1 Signals it's empty - a component was never associated to this slot
		ECS_CRASH_RETURN(manager->ExistsSharedComponent(data->component), "EntityManager: Trying to destroy shared component {#} when it doesn't exist.", 
			data->component.value);

		// If it has an allocator deallocate it
		MemoryArena** allocator = &manager->m_shared_components[data->component.value].info.allocator;
		if (*allocator != nullptr) {
			manager->m_memory_manager->Deallocate(*allocator);
			*allocator = nullptr;
		}

		manager->m_shared_components[data->component.value].instances.FreeBuffer();
		manager->m_shared_components[data->component.value].info.size = -1;
		manager->m_small_memory_manager.Deallocate(manager->m_shared_components[data->component.value].info.name.buffer);
		const void* table_buffer = manager->m_shared_components[data->component.value].named_instances.GetAllocatedBuffer();
		if (table_buffer != nullptr && manager->m_shared_components[data->component.value].named_instances.GetCapacity() > 0) {
			// Deallocate the table
			manager->m_small_memory_manager.Deallocate(table_buffer);
		}
	}

#pragma endregion

#pragma region Create shared instance

	void CommitCreateSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateSharedInstance* data = (DeferredCreateSharedInstance*)_data;

		// If no component is allocated at that slot, fail
		unsigned int component_size = manager->m_shared_components[data->component.value].info.size;
		ECS_CRASH_RETURN(component_size != -1, "EntityManager: Trying to create a shared instance of shared component {#} failed. "
			"There is no such component.", manager->GetSharedComponentName(data->component));

		// Allocate the memory
		void* allocation = manager->m_small_memory_manager.Allocate(component_size);
		memcpy(allocation, data->data, component_size);

		unsigned int instance_index = manager->m_shared_components[data->component.value].instances.Add(allocation);
		ECS_CRASH_RETURN(instance_index < ECS_SHARED_INSTANCE_MAX_VALUE, "EntityManager: Too many shared instances created for component {#}.",
			manager->GetSharedComponentName(data->component));
		
		if (data->copy_buffers) {
			MemoryArena* arena = manager->GetSharedComponentAllocator(data->component);
			if (arena != nullptr) {
				const ComponentInfo* info = &manager->m_shared_components[data->component.value].info;
				for (unsigned char buffer_index = 0; buffer_index < info->component_buffers_count; buffer_index++) {
					ComponentBufferCopy(info->component_buffers[buffer_index], arena, data->data, allocation);
				}
			}
		}

		// Assert that the maximal amount of shared instance is not reached
		if (_additional_data != nullptr) {
			SharedInstance* instance = (SharedInstance*)_additional_data;
			instance->value = (short)instance_index;
		}
	}

#pragma endregion

#pragma region Create Named Shared Instance

	void CommitCreateNamedSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateNamedSharedInstance* data = (DeferredCreateNamedSharedInstance*)_data;

		SharedInstance instance;
		CommitCreateSharedInstance(manager, data, &instance);
		// Allocate memory for the name
		Stream<char> allocated_identifier;
		allocated_identifier.InitializeAndCopy(manager->SmallAllocator(), data->identifier);

		InsertIntoDynamicTable(manager->m_shared_components[data->component.value].named_instances, manager->SmallAllocator(), instance, allocated_identifier);
	}

	void CommitBindNamedSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredBindNamedSharedInstance* data = (DeferredBindNamedSharedInstance*)_data;

		ECS_CRASH_RETURN(manager->ExistsSharedComponent(data->component),
			"Entity Manager: Trying to bind a named shared instance for component which doesn't exist", manager->GetSharedComponentName(data->component));

		ECS_CRASH_RETURN(manager->ExistsSharedInstanceOnly(data->component, data->instance), 
			"EntityManager: Trying to bind named shared instance {#} for component {#} at slot {#} which doesn't exist.", data->identifier, manager->GetSharedComponentName(data->component), data->instance.value);
		
		Stream<char> allocated_identifier;
		allocated_identifier.InitializeAndCopy(manager->SmallAllocator(), data->identifier);

		// Allocate memory for the name
		InsertIntoDynamicTable(manager->m_shared_components[data->component.value].named_instances, manager->SmallAllocator(), data->instance, allocated_identifier);
	}

#pragma endregion

#pragma region Destroy Shared Instance

	void CommitDestroySharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroySharedInstance* data = (DeferredDestroySharedInstance*)_data;

		size_t index = 0;
		ECS_CRASH_RETURN(manager->ExistsSharedComponent(data->component), "EntityManager: The component {#} doesn't exist when trying to destroy a shared instance.",
			manager->GetSharedComponentName(data->component));

		ECS_CRASH_RETURN(manager->ExistsSharedInstanceOnly(data->component, data->instance), "EntityManager: The shared instance {#} for component {#} "
			"doesn't exist when trying to destroy it.", data->instance.value, manager->GetSharedComponentName(data->component));

		Deallocate(manager->SmallAllocator(), manager->m_shared_components[data->component.value].instances[data->instance.value]);
		manager->m_shared_components[data->component.value].instances.Remove(data->instance.value);
	}

#pragma endregion

#pragma region Destroy Named Shared Instance

	void CommitDestroyNamedSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyNamedSharedInstance* data = (DeferredDestroyNamedSharedInstance*)_data;

		Stream<char> identifier = data->identifier;
		ECS_CRASH_RETURN(manager->ExistsSharedComponent(data->component), "EntityManager: Incorrect shared component {#} when trying to destroy named shared instance {#}.",
			manager->GetSharedComponentName(data->component), identifier);
		
		SharedInstance instance;
		ECS_CRASH_RETURN(manager->m_shared_components[data->component.value].named_instances.TryGetValue(data->identifier, instance), "EntityManager: There is no shared "
			"instance {#} at shared component {#}.", identifier, manager->GetSharedComponentName(data->component));

		DeferredDestroySharedInstance commit_data;
		commit_data.component = data->component;
		commit_data.instance = instance;
		CommitDestroySharedInstance(manager, &commit_data, nullptr);
	}

#pragma endregion

#pragma region Destroy Unreferenced Shared Instance (single instance)

	void CommitDestroyUnreferencedSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyUnreferencedSharedInstance* data = (DeferredDestroyUnreferencedSharedInstance*)_data;
		DeferredDestroyUnreferencedSharedInstanceAdditional* return_value = (DeferredDestroyUnreferencedSharedInstanceAdditional*)_additional_data;

		ECS_CRASH_RETURN(manager->ExistsSharedComponent(data->component), "EntityManager: Incorrect shared component {#} when trying to destroy "
			"unreferenced shared instances.", manager->GetSharedComponentName(data->component));

		// Go through the archetypes and pop the shared instances which are matched
		VectorComponentSignature vector_signature;
		vector_signature.ConvertComponents({ &data->component, 1 });

		unsigned int archetype_count = manager->m_archetypes.size;
		for (unsigned int archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
			VectorComponentSignature archetype_signature = manager->GetArchetypeSharedComponents(archetype_index);

			unsigned char component_in_archetype_index;
			archetype_signature.Find(vector_signature, &component_in_archetype_index);
			if (component_in_archetype_index != UCHAR_MAX) {
				// Has the component
				const Archetype* archetype = manager->GetArchetype(archetype_index);
				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					const SharedInstance* current_instances = archetype->GetBaseInstances(base_index);
					if (current_instances[component_in_archetype_index] == data->instance) {
						// Found it, stop
						if (return_value != nullptr) {
							return_value->return_value = false;
						}
						return;
					}
				}
			}
		}

		// It is unreferenced, delete it
		manager->UnregisterSharedInstanceCommit(data->component, data->instance);
		if (return_value != nullptr) {
			return_value->return_value = true;
		}
	}

#pragma endregion

#pragma region Destroy Unreferenced Shared Instances

	void CommitDestroyUnreferencedSharedInstances(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyUnreferencedSharedInstances* data = (DeferredDestroyUnreferencedSharedInstances*)_data;

		ECS_CRASH_RETURN(manager->ExistsSharedComponent(data->component), "EntityManager: Incorrect shared component {#} when trying to destroy "
			"unreferenced shared instances.", manager->GetSharedComponentName(data->component));

		// Gather all the shared instances and put them inside an array and pop them out when we find a match
		ECS_STACK_CAPACITY_STREAM(SharedInstance, component_instances, ECS_KB * 32);
		manager->m_shared_components[data->component].instances.stream.ForEachIndex([&](unsigned int index) {
			SharedInstance current_instance = { (short)index };
			component_instances.AddAssert(current_instance);
		});

		// Go through the archetypes and pop the shared instances which are matched
		VectorComponentSignature vector_signature;
		vector_signature.ConvertComponents({ &data->component, 1 });

		unsigned int archetype_count = manager->m_archetypes.size;
		for (unsigned int archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
			VectorComponentSignature archetype_signature = manager->GetArchetypeSharedComponents(archetype_index);

			unsigned char component_in_archetype_index;
			archetype_signature.Find(vector_signature, &component_in_archetype_index);
			if (component_in_archetype_index != UCHAR_MAX) {
				// Has the component
				const Archetype* archetype = manager->GetArchetype(archetype_index);
				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					const SharedInstance* current_instances = archetype->GetBaseInstances(base_index);
					size_t stream_index = function::SearchBytes(component_instances.buffer, component_instances.size, current_instances[component_in_archetype_index].value, sizeof(SharedInstance));
					if (stream_index != -1) {
						component_instances.RemoveSwapBack(component_instances.size);
					}
				}
			}
		}
		
		// All the remaining components are not referenced
		// Destroy these
		for (unsigned int index = 0; index < component_instances.size; index++) {
			manager->UnregisterSharedInstanceCommit(data->component, component_instances[index]);
		}
	}

#pragma endregion

#pragma region Clear Tags

	void CommitClearEntityTags(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredClearEntityTag* data = (DeferredClearEntityTag*)_data;

		for (size_t index = 0; index < data->entities.size; index++) {
			manager->m_entity_pool->ClearTag(data->entities[index], data->tag);
		}
	}

	void RegisterClearEntityTags(EntityManager* manager, Stream<Entity> entities, unsigned char tag, DeferredActionParameters parameters, DebugInfo debug_info) {
		DeferredClearEntityTag* data = nullptr;
		
		if (parameters.entities_are_stable) {
			data = (DeferredClearEntityTag*)manager->AllocateTemporaryBuffer(sizeof(DeferredClearEntityTag));
			data->entities = entities;
		}
		else {
			data = (DeferredClearEntityTag*)manager->AllocateTemporaryBuffer(sizeof(Entity) * entities.size + sizeof(DeferredClearEntityTag));
			void* entity_buffer = function::OffsetPointer(data, sizeof(DeferredClearEntityTag));
			entities.CopyTo(entity_buffer);
			data->entities = { entity_buffer, entities.size };
		}

		data->tag = tag;
		WriteCommandStream(manager, parameters, { DataPointer(data, DEFERRED_CLEAR_ENTITY_TAGS), debug_info });
	}

#pragma endregion

#pragma region Set Tags

	void CommitSetEntityTags(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredSetEntityTag* data = (DeferredSetEntityTag*)_data;

		for (size_t index = 0; index < data->entities.size; index++) {
			manager->m_entity_pool->SetTag(data->entities[index], data->tag);
		}
	}

#pragma endregion

#pragma region Add Entities To Hierarchy

	void CommitAddEntitiesToHierarchyPairs(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddEntitiesToHierarchyPairs* data = (DeferredAddEntitiesToHierarchyPairs*)_data;
		
		for (size_t index = 0; index < data->pairs.size; index++) {
			manager->m_hierarchy.AddEntry(data->pairs[index].parent, data->pairs[index].child);
		}
	}

	void CommitAddEntitiesToHierarchyContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddEntitiesToHierarchyContiguous* data = (DeferredAddEntitiesToHierarchyContiguous*)_data;

		for (unsigned int index = 0; index < data->count; index++) {
			manager->m_hierarchy.AddEntry(data->parents[index], data->children[index]);
		}
	}

#pragma endregion

#pragma region Add Entities To Parent Hierarchy

	void CommitAddEntitiesToParentHierarchy(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddEntitiesToParentHierarchy* data = (DeferredAddEntitiesToParentHierarchy*)_data;

		for (size_t index = 0; index < data->entities.size; index++) {
			manager->m_hierarchy.AddEntry(data->parent, data->entities[index]);
		}
	}

#pragma endregion

#pragma region Remove Entities From Hierarchy

	void CommitRemoveEntitiesFromHierarchy(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredRemoveEntitiesFromHierarchy* data = (DeferredRemoveEntitiesFromHierarchy*)_data;

		bool destroy_children = !data->default_destroy_children;

		if (!destroy_children) {
			for (size_t index = 0; index < data->entities.size; index++) {
				manager->m_hierarchy.RemoveEntry(data->entities[index]);
			}
		}
		else {
			ECS_STACK_CAPACITY_STREAM(Entity, temp_children, ECS_KB * 2);

			// There is no need for batch deletion - the archetype base deletes
			for (size_t index = 0; index < data->entities.size; index++) {
				// Get the children first
				temp_children.size = 0;
				manager->m_hierarchy.GetAllChildrenFromEntity(data->entities[index], temp_children);

				if (temp_children.size > 0) {
					DeferredDeleteEntities delete_data;
					delete_data.entities = temp_children;
					CommitDeleteEntities(manager, &delete_data, nullptr);
				}
				manager->m_hierarchy.RemoveEntry(data->entities[index]);
			}
		}
	}

#pragma endregion

#pragma region Try Remove Entities From Hierarchy

	void CommitTryRemoveEntitiesFromHierarchy(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredTryRemoveEntitiesFromHierarchy* data = (DeferredTryRemoveEntitiesFromHierarchy*)_data;

		bool destroy_children = !data->default_destroy_children;
		bool all_exist = true;

		if (!destroy_children) {
			for (size_t index = 0; index < data->entities.size; index++) {
				bool exists = manager->m_hierarchy.Exists(data->entities[index]);
				all_exist &= exists;
				if (exists) {
					manager->m_hierarchy.RemoveEntry(data->entities[index]);
				}
			}
		}
		else {
			ECS_STACK_CAPACITY_STREAM(Entity, temp_children, ECS_KB * 2);

			// There is no need for batch deletion - the archetype base deletes
			for (size_t index = 0; index < data->entities.size; index++) {
				bool exists = manager->m_hierarchy.Exists(data->entities[index]);
				all_exist &= exists;
				if (exists) {
					// Get the children first
					temp_children.size = 0;
					manager->m_hierarchy.GetAllChildrenFromEntity(data->entities[index], temp_children);

					if (temp_children.size > 0) {
						DeferredDeleteEntities delete_data;
						delete_data.entities = temp_children;
						CommitDeleteEntities(manager, &delete_data, nullptr);
					}
					manager->m_hierarchy.RemoveEntry(data->entities[index]);
				}
			}
		}

		bool* state = (bool*)_additional_data;
		if (state != nullptr) {
			*state = all_exist;
		}
	}

#pragma endregion

#pragma region Change Entity Parent From Hierarchy

	void CommitChangeEntityParentHierarchy(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredChangeEntityParentHierarchy* data = (DeferredChangeEntityParentHierarchy*)_data;

		for (size_t index = 0; index < data->pairs.size; index++) {
			manager->m_hierarchy.ChangeParent(data->pairs[index].parent, data->pairs[index].child);
		}
	}

#pragma endregion

#pragma region Change Or Set Entity Parent From Hierarchy

	void CommitChangeOrSetEntityParentHierarchy(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredChangeOrSetEntityParentHierarchy* data = (DeferredChangeOrSetEntityParentHierarchy*)_data;

		for (size_t index = 0; index < data->pairs.size; index++) {
			manager->m_hierarchy.ChangeOrSetParent(data->pairs[index].parent, data->pairs[index].child);
		}
	}

#pragma endregion

	// The jump table for the deferred calls
	DeferredCallbackFunctor DEFERRED_CALLBACKS[] = {
		CommitWriteComponent,
		CommitWriteArchetype,
		CommitEntityAddComponent<false>,
		CommitEntityAddComponentSplatted,
		CommitEntityAddComponentScattered,
		CommitEntityAddComponentContiguous,
		CommitEntityAddComponentByEntities,
		CommitEntityAddComponentByEntitiesContiguous,
		CommitEntityRemoveComponent,
		CommitCreateEntities<CREATE_ENTITIES_TEMPLATE_BASIC>,
		CommitCreateEntitiesDataSplatted,
		CommitCreateEntitiesDataScattered,
		CommitCreateEntitiesDataContiguous,
		CommitCreateEntitiesDataByEntities,
		CommitCreateEntitiesDataByEntitiesContiguous,
		CommitCopyEntities,
		CommitDeleteEntities,
		CommitEntityAddSharedComponent,
		CommitEntityRemoveSharedComponent,
		CommitEntityChangeSharedInstance,
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
		CommitBindNamedSharedInstance,
		CommitDestroySharedInstance,
		CommitDestroyNamedSharedInstance,
		CommitDestroyUnreferencedSharedInstance,
		CommitDestroyUnreferencedSharedInstances,
		CommitClearEntityTags,
		CommitSetEntityTags,
		CommitAddEntitiesToHierarchyPairs,
		CommitAddEntitiesToHierarchyContiguous,
		CommitAddEntitiesToParentHierarchy,
		CommitRemoveEntitiesFromHierarchy,
		CommitTryRemoveEntitiesFromHierarchy,
		CommitChangeEntityParentHierarchy,
		CommitChangeOrSetEntityParentHierarchy
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

		// Use the small memory manager in order to acquire the buffer for the vector component signatures
		m_archetype_vector_signatures = (VectorComponentSignature*)m_small_memory_manager.Allocate(sizeof(VectorComponentSignature) * 2 * ENTITY_MANAGER_DEFAULT_ARCHETYPE_COUNT);

		// Calculate the first batch of unique and shared components space
		m_unique_components.Initialize(&m_small_memory_manager, ENTITY_MANAGER_DEFAULT_UNIQUE_COMPONENTS);
		m_shared_components.Initialize(&m_small_memory_manager, ENTITY_MANAGER_DEFAULT_SHARED_COMPONENTS);

		for (size_t index = 0; index < m_unique_components.size; index++) {
			// Set the size to -1 to indicate that it is not allocated
			m_unique_components[index].size = -1;
			m_unique_components[index].allocator = nullptr;
		}

		for (size_t index = 0; index < m_shared_components.size; index++) {
			// Set the size to -1 to indicate that it is not allocated
			m_shared_components[index].info.size = -1;
			m_shared_components[index].info.allocator = nullptr;
		}

		// Allocate the atomic streams - the deferred actions, clear tags and set tags
		m_deferred_actions.Initialize(m_memory_manager, descriptor.deferred_action_capacity);

		m_hierarchy_allocator = (MemoryManager*)m_memory_manager->Allocate(sizeof(MemoryManager));
		*m_hierarchy_allocator = DefaultEntityHierarchyAllocator(m_memory_manager->m_backup);
		m_hierarchy = EntityHierarchy(m_hierarchy_allocator, 0, 0, 0);

		// Allocate the query cache now - use a separate allocation
		// Get a default allocator for it for the moment
		MemoryManager* query_cache_allocator = (MemoryManager*)m_memory_manager->m_backup->Allocate(sizeof(MemoryManager));
		*query_cache_allocator = ArchetypeQueryCache::DefaultAllocator(descriptor.memory_manager->m_backup);
		m_query_cache = (ArchetypeQueryCache*)m_memory_manager->Allocate(sizeof(ArchetypeQueryCache));
		*m_query_cache = ArchetypeQueryCache(this, GetAllocatorPolymorphic(query_cache_allocator));
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::AllocateTemporaryBuffer(size_t size, size_t alignment)
	{
		return m_temporary_allocator.Allocate_ts(size, alignment);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentCommit(Entity entity, Component component)
	{
		AddComponentCommit({ &entity, 1 }, component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponent(Entity entity, Component component, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		AddComponent({ &entity, 1 }, component, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentCommit(Entity entity, Component component, const void* data)
	{
		AddComponentCommit({ &entity, 1 }, component, data);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponent(Entity entity, Component component, const void* data, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		AddComponent({ &entity, 1 }, component, data, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentCommit(Entity entity, ComponentSignature components)
	{
		AddComponentCommit({ &entity, 1 }, components);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponent(Entity entity, ComponentSignature components, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		AddComponent({ &entity, 1 }, components, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentCommit(Entity entity, ComponentSignature components, const void** data)
	{
		AddComponentCommit({ &entity, 1 }, components, data, ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponent(Entity entity, ComponentSignature components, const void** data, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		AddComponent({ &entity, 1 }, components, data, ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY, parameters, debug_info);
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

	void EntityManager::AddComponent(Stream<Entity> entities, Component component, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		RegisterEntityAddComponent(this, entities, { &component, 1 }, nullptr, DEFERRED_ENTITY_ADD_COMPONENT, parameters, debug_info);
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

	void EntityManager::AddComponent(Stream<Entity> entities, Component component, const void* data, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		RegisterEntityAddComponent(this, entities, { &component, 1 }, &data, DEFERRED_ENTITY_ADD_COMPONENT_SPLATTED, parameters, debug_info);
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

	void EntityManager::AddComponent(Stream<Entity> entities, ComponentSignature components, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		RegisterEntityAddComponent(this, entities, components, nullptr, DEFERRED_ENTITY_ADD_COMPONENT, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponentCommit(Stream<Entity> entities, ComponentSignature components, const void** data, EntityManagerCopyEntityDataType copy_type)
	{
		DeferredAddComponentEntities commit_data;
		commit_data.components = components;
		commit_data.entities = entities;
		commit_data.data = data;

		DeferredCallbackFunctor callback = CommitEntityAddComponentSplatted;
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
		{
			ECS_STACK_CAPACITY_STREAM(char, component_signature_string, 1024);
			ECS_CRASH_RETURN(false, "EntityManager: Copy entity data type is incorrect when trying to add component/s { {#} } to entities. First entity is {#}.",
				GetComponentSignatureString(components, component_signature_string), entities[0].value);
		}
		}

		callback(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponent(
		Stream<Entity> entities, 
		ComponentSignature components, 
		const void** data,
		EntityManagerCopyEntityDataType copy_type,
		DeferredActionParameters parameters,
		DebugInfo debug_info
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
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_SPLAT:
			action_type = DEFERRED_ENTITY_ADD_COMPONENT_SPLATTED;
			break;
		default:
		{
			ECS_STACK_CAPACITY_STREAM(char, component_string_storage, 1024);
			ECS_CRASH_RETURN(false, "EntityManager: Incorrect copy type when trying to add components {#} to entities. First entity is {#}.",
				GetComponentSignatureString(components, component_string_storage), entities[0].value);
		}
		}
		RegisterEntityAddComponent(this, entities, components, data, action_type, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponentCommit(Entity entity, Component shared_component, SharedInstance instance)
	{
		AddSharedComponentCommit({ &entity, 1 }, shared_component, instance);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponent(Entity entity, Component shared_component, SharedInstance instance, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		AddSharedComponent({ &entity, 1 }, shared_component, instance, parameters, debug_info);
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

	void EntityManager::AddSharedComponent(Stream<Entity> entities, Component shared_component, SharedInstance instance, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		AddSharedComponent(entities, { &shared_component, &instance, 1 }, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponentCommit(Entity entity, SharedComponentSignature components) {
		AddSharedComponentCommit({ &entity, 1 }, components);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponent(Entity entity, SharedComponentSignature components, DeferredActionParameters parameters, DebugInfo debug_info) {
		AddSharedComponent({ &entity, 1 }, components, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddEntityToHierarchyCommit(Stream<EntityPair> pairs)
	{
		DeferredAddEntitiesToHierarchyPairs data;
		data.pairs = pairs;
		CommitAddEntitiesToHierarchyPairs(this, &data, nullptr);
	}

	void EntityManager::AddEntityToHierarchy(
		Stream<EntityPair> pairs,
		DeferredActionParameters parameters, 
		DebugInfo debug_info
	)
	{
		DeferredAddEntitiesToHierarchyPairs* data = nullptr;
		if (parameters.entities_are_stable) {
			data = (DeferredAddEntitiesToHierarchyPairs*)AllocateTemporaryBuffer(sizeof(DeferredAddEntitiesToHierarchyPairs));
			data->pairs = pairs;
		}
		else {
			data = (DeferredAddEntitiesToHierarchyPairs*)AllocateTemporaryBuffer(sizeof(DeferredAddEntitiesToHierarchyPairs) + sizeof(EntityPair) * pairs.size);
			data->pairs.buffer = (EntityPair*)function::OffsetPointer(data, sizeof(DeferredAddEntitiesToHierarchyPairs));
			pairs.CopyTo(data->pairs.buffer);
			data->pairs.size = pairs.size;
		}

		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_ADD_ENTITIES_TO_HIERARCHY_PAIRS), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddEntityToHierarchyCommit(const Entity* parents, const Entity* children, unsigned int count)
	{
		DeferredAddEntitiesToHierarchyContiguous commit_data;
		commit_data.children = children;
		commit_data.parents = parents;
		commit_data.count = count;
		CommitAddEntitiesToHierarchyContiguous(this, &commit_data, nullptr);
	}

	void EntityManager::AddEntityToHierarchy(const Entity* parents, const Entity* children, unsigned int count, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		DeferredAddEntitiesToHierarchyContiguous* data = nullptr;
		if (parameters.entities_are_stable) {
			data = (DeferredAddEntitiesToHierarchyContiguous*)AllocateTemporaryBuffer(sizeof(DeferredAddEntitiesToHierarchyContiguous));
			data->parents = parents;
			data->children = children;
		}
		else {
			data = (DeferredAddEntitiesToHierarchyContiguous*)AllocateTemporaryBuffer(sizeof(DeferredAddEntitiesToHierarchyContiguous) + sizeof(Entity) * count * 2);
			data->parents = (Entity*)function::OffsetPointer(data, sizeof(DeferredAddEntitiesToHierarchyContiguous));
			memcpy((Entity*)data->parents, parents, sizeof(Entity) * count);
			data->children = (Entity*)function::OffsetPointer(data->parents, sizeof(Entity) * count);
			memcpy((Entity*)data->children, children, sizeof(Entity) * count);
		}

		data->count = count;
		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_ADD_ENTITIES_TO_HIERARCHY_CONTIGUOUS), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddEntitiesToParentCommit(Stream<Entity> entities, Entity parent)
	{
		DeferredAddEntitiesToParentHierarchy commit_data;
		commit_data.entities = entities;
		commit_data.parent = parent;
		CommitAddEntitiesToParentHierarchy(this, &commit_data, nullptr);
	}

	void EntityManager::AddEntitiesToParent(Stream<Entity> entities, Entity parent, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		DeferredAddEntitiesToParentHierarchy* data = nullptr;
		if (parameters.entities_are_stable) {
			data = (DeferredAddEntitiesToParentHierarchy*)AllocateTemporaryBuffer(sizeof(DeferredAddEntitiesToParentHierarchy));
			data->entities = entities;
		}
		else {
			data = (DeferredAddEntitiesToParentHierarchy*)AllocateTemporaryBuffer(sizeof(DeferredAddEntitiesToParentHierarchy) + sizeof(EntityPair) * entities.size);
			data->entities.buffer = (Entity*)function::OffsetPointer(data, sizeof(DeferredAddEntitiesToParentHierarchy));
			data->entities.Copy(entities);
		}

		data->parent = parent;
		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_ADD_ENTITIES_TO_PARENT_HIERARCHY), debug_info });
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

	void EntityManager::AddSharedComponent(Stream<Entity> entities, SharedComponentSignature components, DeferredActionParameters parameters, DebugInfo debug_info) {
		size_t allocation_size = sizeof(DeferredAddSharedComponentEntities) + (sizeof(Component) + sizeof(SharedInstance)) * components.count;
		allocation_size += parameters.entities_are_stable ? 0 : entities.MemoryOf(entities.size);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
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

		buffer = function::AlignPointer(buffer, alignof(Entity));
		data->entities = GetEntitiesFromActionParameters(entities, parameters, buffer);

		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_ENTITY_ADD_SHARED_COMPONENT), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ChangeComponentIndex(Component old_component, Component new_component)
	{
		ECS_ASSERT(ExistsComponent(old_component) && !ExistsComponent(new_component));

		unsigned short byte_size = m_unique_components[old_component.value].size;

		MemoryArena* arena = GetComponentAllocator(old_component);
		// Always use size 0 allocator size because we will transfer the arena to the new slot
		// In this way we avoid creating a new allocator and transfer all the existing data to it
		RegisterComponentCommit(
			new_component, 
			byte_size, 
			0,
			m_unique_components[old_component.value].name, 
			{ m_unique_components[old_component.value].component_buffers, m_unique_components[old_component.value].component_buffers_count }
		);

		if (arena != nullptr) {
			m_unique_components[old_component.value].allocator = nullptr;
			m_unique_components[new_component.value].allocator = arena;
		}

		// Now destroy the old component
		UnregisterComponentCommit(old_component);

		unsigned int archetype_count = GetArchetypeCount();
		for (unsigned int index = 0; index < archetype_count; index++) {
			unsigned char component_index = FindArchetypeUniqueComponent(index, old_component);
			if (component_index != UCHAR_MAX) {
				m_archetypes[index].m_unique_components[component_index] = new_component;
				*GetArchetypeUniqueComponentsPtr(index) = VectorComponentSignature(m_archetypes[index].m_unique_components);
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ChangeSharedComponentIndex(Component old_component, Component new_component)
	{
		ECS_ASSERT(ExistsSharedComponent(old_component) && !ExistsSharedComponent(new_component));

		MemoryArena* arena = GetSharedComponentAllocator(old_component);

		unsigned short byte_size = m_shared_components[old_component.value].info.size;

		// Record the instances and the named instances in order to be transfered
		auto instances = m_shared_components[old_component.value].instances;
		auto named_instances = m_shared_components[old_component.value].named_instances;
		m_shared_components[old_component.value].instances.stream.capacity = 0;
		m_shared_components[old_component.value].named_instances.m_capacity = 0;

		// Always use size 0 allocator size because we will transfer the arena to the new slot
		// In this way we avoid creating a new allocator and transfer all the existing data to it
		RegisterSharedComponentCommit(
			new_component, 
			byte_size, 
			0, 
			m_shared_components[old_component.value].info.name, 
			{ m_shared_components[old_component.value].info.component_buffers, m_shared_components[old_component.value].info.component_buffers_count }
		);

		m_shared_components[new_component.value].instances = instances;
		m_shared_components[new_component.value].named_instances = named_instances;

		if (arena != nullptr) {
			m_shared_components[old_component.value].info.allocator = nullptr;
			m_shared_components[new_component.value].info.allocator = arena;
		}

		// Now destroy the old component
		UnregisterSharedComponentCommit(old_component);

		// Update the archetype references
		unsigned int archetype_count = GetArchetypeCount();
		for (unsigned int index = 0; index < archetype_count; index++) {
			unsigned char component_index = FindArchetypeSharedComponent(index, old_component);
			if (component_index != UCHAR_MAX) {
				m_archetypes[index].m_shared_components[component_index] = new_component;
				*GetArchetypeSharedComponentsPtr(index) = VectorComponentSignature(m_archetypes[index].m_shared_components);
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ChangeEntitySharedInstance(
		Stream<ChangeSharedComponentElement> elements, 
		DeferredActionParameters parameters, 
		bool destroy_base_archetype, 
		DebugInfo debug_info
	)
	{
		size_t allocation_size = sizeof(DeferredChangeSharedInstanceEntities);
		allocation_size += parameters.data_is_stable ? 0 : elements.MemoryOf(elements.size);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredChangeSharedInstanceEntities* data = (DeferredChangeSharedInstanceEntities*)allocation;
		buffer += sizeof(*data);
		data->elements.InitializeAndCopy(buffer, elements);
		data->destroy_base_archetype = destroy_base_archetype;

		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_ENTITY_CHANGE_SHARED_INSTANCE), debug_info });
	}

	void EntityManager::ChangeEntitySharedInstanceCommit(Stream<ChangeSharedComponentElement> elements, bool destroy_base_archetype)
	{
		DeferredChangeSharedInstanceEntities commit_data;
		commit_data.destroy_base_archetype = destroy_base_archetype;
		commit_data.elements = elements;
		CommitEntityChangeSharedInstance(this, &commit_data, nullptr);
	}

	SharedInstance EntityManager::ChangeEntitySharedInstanceCommit(Entity entity, Component component, SharedInstance new_instance, bool destroy_base_archetype)
	{
		EntityInfo* info = m_entity_pool->GetInfoPtr(entity);
		Archetype* archetype = GetArchetype(info->main_archetype);
		unsigned char shared_index = archetype->FindSharedComponentIndex(component);
		ECS_CRASH_RETURN_VALUE(
			shared_index != UCHAR_MAX, 
			{},
			"EntityManager: Entity {#} doesn't have shared component {#} when trying to change shared instance to {#}.", 
			entity.value, 
			GetSharedComponentName(component), 
			new_instance.value
		);

		SharedInstance shared_instances[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		SharedComponentSignature shared_signature = archetype->GetSharedSignature(info->base_archetype);
		// Check to see that the instance is indeed different
		ECS_CRASH_RETURN_VALUE(
			shared_signature.instances[shared_index] != new_instance,
			{},
			"EntityManager: Trying to replace shared instance {#} with the same instance for entity {#}, component {#}.",
			new_instance.value,
			entity.value,
			GetSharedComponentName(component)
		);

		memcpy(shared_instances, shared_signature.instances, sizeof(SharedInstance) * shared_signature.count);
		SharedInstance old_instance = shared_instances[shared_index];

		shared_instances[shared_index] = new_instance;

		SharedComponentSignature new_signature = { shared_signature.indices, shared_instances, shared_signature.count };
		// Check to see if the base archetype already exists - else create a new one
		unsigned int base_index = archetype->FindBaseIndex(new_signature);
		if (base_index == -1) {
			base_index = CreateArchetypeBaseCommit(info->main_archetype, new_signature);
		}

		unsigned int old_base_index = info->base_archetype;

		// The base must be taken now because the create commit can reallocate the array
		ArchetypeBase* initial_base = archetype->GetBase(old_base_index);

		ComponentSignature unique_signature = archetype->GetUniqueSignature();
		const void* entity_components[ECS_ARCHETYPE_MAX_COMPONENTS];
		for (unsigned char index = 0; index < unique_signature.count; index++) {
			entity_components[index] = initial_base->GetComponentByIndex(*info, index);
		}

		// Insert the entity into the given base archetype
		ArchetypeBase* final_base = archetype->GetBase(base_index);
		unsigned int write_position = final_base->AddEntity(entity, unique_signature, entity_components);

		// Remove it from the previous archetype
		initial_base->RemoveEntity(info->stream_index, m_entity_pool);

		// Change the entity info - the base archetype and the stream index
		info->base_archetype = base_index;
		info->stream_index = write_position;

		if (destroy_base_archetype) {
			// Destroy the base if it has 0 entities in it
			if (initial_base->EntityCount() == 0) {
				DestroyArchetypeBaseCommit(info->main_archetype, old_base_index);
			}
		}

		return old_instance;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ChangeEntityParentCommit(Stream<EntityPair> pairs)
	{
		DeferredChangeEntityParentHierarchy data;
		data.pairs = pairs;
		CommitChangeEntityParentHierarchy(this, &data, nullptr);
	}

	void EntityManager::ChangeEntityParent(Stream<EntityPair> pairs, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		DeferredChangeEntityParentHierarchy* data = nullptr;
		if (parameters.entities_are_stable) {
			data = (DeferredChangeEntityParentHierarchy*)AllocateTemporaryBuffer(sizeof(DeferredChangeEntityParentHierarchy));
			data->pairs = pairs;
		}
		else {
			data = (DeferredChangeEntityParentHierarchy*)AllocateTemporaryBuffer(sizeof(DeferredChangeEntityParentHierarchy) + sizeof(EntityPair) * pairs.size);
			data->pairs.buffer = (EntityPair*)function::OffsetPointer(data, sizeof(DeferredChangeEntityParentHierarchy));
			pairs.CopyTo(data->pairs.buffer);
			data->pairs.size = pairs.size;
		}

		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_CHANGE_ENTITY_PARENT_HIERARCHY), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ChangeOrSetEntityParentCommit(Stream<EntityPair> pairs)
	{
		DeferredChangeOrSetEntityParentHierarchy data;
		data.pairs = pairs;
		CommitChangeOrSetEntityParentHierarchy(this, &data, nullptr);
	}

	void EntityManager::ChangeOrSetEntityParent(Stream<EntityPair> pairs, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		DeferredChangeOrSetEntityParentHierarchy* data = nullptr;
		if (parameters.entities_are_stable) {
			data = (DeferredChangeOrSetEntityParentHierarchy*)AllocateTemporaryBuffer(sizeof(DeferredChangeOrSetEntityParentHierarchy));
			data->pairs = pairs;
		}
		else {
			data = (DeferredChangeOrSetEntityParentHierarchy*)AllocateTemporaryBuffer(sizeof(DeferredChangeOrSetEntityParentHierarchy) + sizeof(EntityPair) * pairs.size);
			data->pairs.buffer = (EntityPair*)function::OffsetPointer(data, sizeof(DeferredChangeOrSetEntityParentHierarchy));
			pairs.CopyTo(data->pairs.buffer);
			data->pairs.size = pairs.size;
		}

		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_CHANGE_OR_SET_ENTITY_PARENT_HIERARCHY), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ClearEntityTagCommit(Stream<Entity> entities, unsigned char tag)
	{
		DeferredClearEntityTag data;
		data.entities = entities;
		data.tag = tag;
		CommitClearEntityTags(this, &data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ClearEntityTag(Stream<Entity> entities, unsigned char tag, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		RegisterClearEntityTags(this, entities, tag, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ClearFrame()
	{
		m_temporary_allocator.Clear();
		m_pending_command_streams.Reset();
		m_deferred_actions.Reset();
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ClearCache()
	{
		m_query_cache->Reset();
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CopyOther(const EntityManager* entity_manager)
	{
		// Copy the entities first using the entity pool
		m_entity_pool->CopyEntities(entity_manager->m_entity_pool);

		// Copy the unique components
		if (m_unique_components.buffer != nullptr) {
			// Deallocate it
			m_small_memory_manager.Deallocate(m_unique_components.buffer);
		}
		m_unique_components.Initialize(&m_small_memory_manager, entity_manager->m_unique_components.size);
		entity_manager->m_unique_components.CopyTo(m_unique_components.buffer);

		// Allocate the allocators for the unique components
		for (size_t index = 0; index < m_unique_components.size; index++) {
			if (ExistsComponent({ (short)index })) {
				if (entity_manager->m_unique_components[index].allocator != nullptr) {
					size_t capacity = entity_manager->m_unique_components[index].allocator->m_size_per_allocator * COMPONENT_ALLOCATOR_ARENA_COUNT;
					CreateAllocatorForComponent(this, m_unique_components[index], capacity);
				}
				else {
					m_unique_components[index].allocator = nullptr;
				}
			}
		}

		// Copy the shared components
		if (m_shared_components.buffer != nullptr) {
			// Deallocate it
			m_small_memory_manager.Deallocate(m_shared_components.buffer);
		}
		m_shared_components.Initialize(&m_small_memory_manager, entity_manager->m_shared_components.size);

		// Cannot just memcpy the values because there are allocations involved
		// Must manually walk through the components and make the allocations as necessary
		for (size_t index = 0; index < entity_manager->m_shared_components.size; index++) {
			m_shared_components[index].info = entity_manager->m_shared_components[index].info;
			// If the info is different from -1, it means that there is a component actually allocated there
			unsigned int component_size = m_shared_components[index].info.size;
			if (component_size != -1) {
				// Allocate separetely the instances buffer

				unsigned int other_capacity = entity_manager->m_shared_components[index].instances.stream.capacity;
				m_shared_components[index].instances.Initialize(GetAllocatorPolymorphic(&m_small_memory_manager), other_capacity);

				if (entity_manager->m_shared_components[index].instances.stream.size > 0) {
					m_shared_components[index].instances.stream.Copy(entity_manager->m_shared_components[index].instances.stream);
					
					// For every value allocate the data
					for (size_t subindex = 0; subindex < m_shared_components[index].instances.stream.size; subindex++) {
						void* new_data = function::Copy(GetAllocatorPolymorphic(&m_small_memory_manager), entity_manager->m_shared_components[index].instances[subindex], component_size);
						m_shared_components[index].instances[subindex] = new_data;
					}
				}

				// If there are any named instances, allocate them separately
				if (entity_manager->m_shared_components[index].named_instances.GetCount() > 0) {
					size_t blit_size = entity_manager->m_shared_components[index].named_instances.MemoryOf(entity_manager->m_shared_components[index].named_instances.GetCapacity());
					void* allocation = m_small_memory_manager.Allocate(blit_size);

					m_shared_components[index].named_instances.SetBuffers(allocation, entity_manager->m_shared_components[index].named_instances.GetCapacity());

					// Blit the entire table
					memcpy(allocation, entity_manager->m_shared_components[index].named_instances.GetAllocatedBuffer(), blit_size);

					// Iterate through the named instances and allocate the identifiers and copy the value
					entity_manager->m_shared_components[index].named_instances.ForEachIndexConst([&](unsigned int subindex) {
						ResourceIdentifier current_identifier = entity_manager->m_shared_components[index].named_instances.GetIdentifierFromIndex(subindex);
						Stream<void> identifier = function::Copy(
							GetAllocatorPolymorphic(&m_small_memory_manager),
							{ current_identifier.ptr, current_identifier.size }
						);
						*m_shared_components[index].named_instances.GetIdentifierPtrFromIndex(subindex) = identifier;
					});
				}

				// Allocate the allocator
				if (entity_manager->m_shared_components[index].info.allocator != nullptr) {
					size_t capacity = entity_manager->m_shared_components[index].info.allocator->m_size_per_allocator * COMPONENT_ALLOCATOR_ARENA_COUNT;
					CreateAllocatorForComponent(this, m_shared_components[index].info, capacity);
				}
			}
		}

		// Resize the archetype vector signatures
		if (m_archetype_vector_signatures != nullptr) {
			m_small_memory_manager.Deallocate(m_archetype_vector_signatures);
		}
		m_archetype_vector_signatures = (VectorComponentSignature*)m_small_memory_manager.Allocate(sizeof(VectorComponentSignature) * entity_manager->m_archetypes.capacity * 2);
		// Blit the data
		memcpy(m_archetype_vector_signatures, entity_manager->m_archetype_vector_signatures, sizeof(VectorComponentSignature) * entity_manager->m_archetypes.size * 2);

		// Copy the actual archetypes now
		m_archetypes.FreeBuffer();
		m_archetypes.Initialize(GetAllocatorPolymorphic(&m_small_memory_manager), entity_manager->m_archetypes.capacity);

		// For every archetype copy the other one
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			m_archetypes.Add(
				Archetype(
					&m_small_memory_manager,
					m_memory_manager,
					m_unique_components.buffer,
					entity_manager->m_archetypes[index].m_unique_components,
					entity_manager->m_archetypes[index].m_shared_components
				)
			);

			m_archetypes[index].CopyOther(entity_manager->m_archetypes.buffer + index);
		}

		m_hierarchy_allocator = (MemoryManager*)m_memory_manager->Allocate(sizeof(MemoryManager));
		*m_hierarchy_allocator = DefaultEntityHierarchyAllocator(m_memory_manager->m_backup);
		m_hierarchy = EntityHierarchy(
			m_hierarchy_allocator,
			entity_manager->m_hierarchy.roots.capacity,
			entity_manager->m_hierarchy.children_table.GetCapacity(),
			entity_manager->m_hierarchy.parent_table.GetCapacity()
		);
		m_hierarchy.CopyOther(&entity_manager->m_hierarchy);

		m_query_cache->CopyOther(entity_manager->m_query_cache);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CopyQueryCache(ArchetypeQueryCache* query_cache, AllocatorPolymorphic allocator)
	{
		*query_cache = ArchetypeQueryCache(this, allocator);
		query_cache->CopyOther(m_query_cache);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CopyEntity(Entity entity, unsigned int count, bool copy_children, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		size_t allocation_size = sizeof(DeferredCopyEntities);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		DeferredCopyEntities* data = (DeferredCopyEntities*)allocation;
		data->entity = entity;
		data->count = count;

		WriteCommandStream(this, { command_stream }, { DataPointer(allocation, DEFERRED_COPY_ENTITIES), debug_info });
	}

	void EntityManager::CopyEntityCommit(Entity entity, unsigned int count, bool copy_children, Entity* entities)
	{
		DeferredCopyEntities commit_data;
		commit_data.count = count;
		commit_data.entity = entity;
		commit_data.copy_children = copy_children;
		CommitCopyEntities(this, &commit_data, entities);
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::CreateArchetypeCommit(ComponentSignature unique, ComponentSignature shared)
	{
		unsigned int index = 0;

		DeferredCreateArchetype commit_data;
		commit_data.shared_components = shared;
		commit_data.unique_components = unique;
		CommitCreateArchetype(this, &commit_data, &index);

		return index;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateArchetype(ComponentSignature unique, ComponentSignature shared, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		size_t allocation_size = sizeof(DeferredCreateArchetype) + sizeof(Component) * unique.count + sizeof(Component) * shared.count;

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredCreateArchetype* data = (DeferredCreateArchetype*)allocation;
		buffer += sizeof(*data);

		data->unique_components.count = unique.count;
		data->unique_components.indices = (Component*)buffer;
		memcpy(data->unique_components.indices, unique.indices, sizeof(Component) * data->unique_components.count);
		buffer += sizeof(Component) * data->unique_components.count;

		data->shared_components.count = shared.count;
		data->shared_components.indices = (Component*)buffer;
		memcpy(data->shared_components.indices, shared.indices, sizeof(Component) * data->shared_components.count);
		buffer += sizeof(Component) * data->shared_components.count;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_CREATE_ARCHETYPE), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	uint2 EntityManager::CreateArchetypeBaseCommit(
		ComponentSignature unique_signature,
		SharedComponentSignature shared_signature,
		unsigned int starting_size
	)
	{
		DeferredCreateArchetypeBase commit_data;
		commit_data.unique_components = unique_signature;
		commit_data.starting_size = starting_size;
		commit_data.shared_components = shared_signature;

		uint2 indices;
		CommitCreateArchetypeBase(this, &commit_data, &indices);
		return indices;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateArchetypeBase(
		ComponentSignature unique_signature,
		SharedComponentSignature shared_signature,
		EntityManagerCommandStream* command_stream,
		unsigned int starting_size,
		DebugInfo debug_info
	)
	{
		size_t allocation_size = sizeof(DeferredCreateArchetypeBase) + sizeof(Component) * shared_signature.count +
			(sizeof(Component) + sizeof(SharedInstance)) * shared_signature.count;

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredCreateArchetypeBase* data = (DeferredCreateArchetypeBase*)allocation;
		buffer += sizeof(*data);

		data->starting_size = starting_size;
		data->unique_components.count = unique_signature.count;
		data->unique_components.indices = (Component*)buffer;
		memcpy(data->unique_components.indices, unique_signature.indices, sizeof(Component) * data->unique_components.count);
		buffer += sizeof(Component) * data->unique_components.count;

		data->shared_components.count = shared_signature.count;
		data->shared_components.indices = (Component*)buffer;
		memcpy(data->shared_components.indices, shared_signature.indices, sizeof(Component) * shared_signature.count);
		buffer += sizeof(Component) * data->shared_components.count;

		data->shared_components.instances = (SharedInstance*)buffer;
		memcpy(data->shared_components.instances, shared_signature.instances, sizeof(SharedInstance) * data->shared_components.count);
		buffer += sizeof(SharedInstance) * data->shared_components.count;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_CREATE_ARCHETYPE_BASE), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	// It will commit the archetype addition
	unsigned int EntityManager::CreateArchetypeBaseCommit(
		unsigned int archetype_index,
		SharedComponentSignature shared_signature,
		unsigned int starting_size
	) {
		ComponentSignature unique = m_archetypes[archetype_index].GetUniqueSignature();
		return CreateArchetypeBaseCommit(unique, shared_signature, starting_size).y;
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Deferred Call
	void EntityManager::CreateArchetypeBase(
		unsigned int archetype_index,
		SharedComponentSignature shared_signature,
		EntityManagerCommandStream* command_stream,
		unsigned int starting_size,
		DebugInfo debug_info
	) {
		return CreateArchetypeBase(m_archetypes[archetype_index].GetUniqueSignature(), shared_signature, command_stream, starting_size, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	Entity EntityManager::CreateEntityCommit(ComponentSignature unique_components, SharedComponentSignature shared_components, bool exclude_from_hierarchy)
	{
		Entity entity;
		CreateEntitiesCommit(1, unique_components, shared_components, exclude_from_hierarchy, &entity);
		return entity;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateEntity(
		ComponentSignature unique_components,
		SharedComponentSignature shared_components, 
		bool exclude_from_hierarchy, 
		EntityManagerCommandStream* command_stream,
		DebugInfo debug_info
	)
	{
		CreateEntities(1, unique_components, shared_components, exclude_from_hierarchy, command_stream, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateEntitiesCommit(unsigned int count, ComponentSignature unique_components, SharedComponentSignature shared_components, bool exclude_from_hierarchy, Entity* entities)
	{
		DeferredCreateEntities commit_data;
		commit_data.count = count;
		commit_data.data = nullptr;
		commit_data.exclude_from_hierarchy = exclude_from_hierarchy;

		commit_data.shared_components = shared_components;
		commit_data.unique_components = unique_components;

		if (entities != nullptr) {
			CommitCreateEntitiesAdditionalData additional_data;
			additional_data.entities = { entities, count };
			CommitCreateEntities<CREATE_ENTITIES_TEMPLATE_GET_ENTITIES>(this, &commit_data, &additional_data);
		}
		else {
			CommitCreateEntities<CREATE_ENTITIES_TEMPLATE_BASIC>(this, &commit_data, nullptr);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateEntities(
		unsigned int count,
		ComponentSignature unique_components, 
		SharedComponentSignature shared_components,
		bool exclude_from_hierarchy,
		EntityManagerCommandStream* command_stream,
		DebugInfo debug_info
	)
	{
		CreateEntities(
			count, 
			unique_components, 
			shared_components, 
			{ nullptr, 0 }, 
			nullptr, 
			ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_SPLAT, 
			false, 
			exclude_from_hierarchy, 
			command_stream, 
			debug_info
		);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CreateEntitiesCommit(
		unsigned int count,
		ComponentSignature unique_components,
		SharedComponentSignature shared_components,
		ComponentSignature components_with_data,
		const void** data,
		EntityManagerCopyEntityDataType copy_type,
		bool copy_buffers,
		bool exclude_from_hierarchy,
		Entity* entities
	) {
		DeferredCreateEntities commit_data;
		commit_data.count = count;
		commit_data.data = data;
		commit_data.shared_components = shared_components;
		commit_data.unique_components = unique_components;
		commit_data.components_with_data = components_with_data;
		commit_data.exclude_from_hierarchy = exclude_from_hierarchy;
		commit_data.copy_buffers = copy_buffers;

		DeferredCallbackFunctor callback = CommitCreateEntitiesDataSplatted;
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
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_SPLAT:
			callback = CommitCreateEntitiesDataSplatted;
			break;
		default:
		{
			ECS_STACK_CAPACITY_STREAM(char, component_signature_storage, 1024);
			ECS_STACK_CAPACITY_STREAM(char, shared_component_signature_storage, 1024);

			ECS_CRASH_RETURN(false, "EntityManager: Incorrect copy type when trying to create entities from source data. "
				"Components: { {#} }. Shared Components{{#}}. Number of entities into command{#}.",
				GetComponentSignatureString(unique_components, component_signature_storage),
				GetSharedComponentSignatureString(shared_components.ComponentSignature(), shared_component_signature_storage),
				count
			);
		}		
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
		const void** component_data,
		EntityManagerCopyEntityDataType copy_type,
		bool copy_buffers,
		bool exclude_from_hierarchy,
		EntityManagerCommandStream* command_stream,
		DebugInfo debug_info
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
		case ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_SPLAT:
			action_type = DEFERRED_CREATE_ENTITIES_SPLATTED;
			break;
		default:
		{
			ECS_STACK_CAPACITY_STREAM(char, component_string_storage, 1024);
			ECS_STACK_CAPACITY_STREAM(char, shared_component_string_storage, 1024);

			ECS_CRASH_RETURN(false, "EntityManager: Incorrect copy type when trying to create entities from source data. "
				"Components: { {#} }. Shared Components : { {#} }. Entity count in command : {#}.",
				GetComponentSignatureString(unique_components, component_string_storage),
				GetSharedComponentSignatureString(shared_components.ComponentSignature(), shared_component_string_storage),
				count
			);
		}
		}

		unsigned int* component_sizes = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * unique_components.count);

		size_t allocation_size = sizeof(DeferredCreateEntities) + sizeof(Component) * (unique_components.count + components_with_data.count)
			+ (sizeof(Component) + sizeof(SharedInstance)) * shared_components.count;

		allocation_size += GetDeferredCallDataAllocationSize(
			this,
			count,
			component_sizes,
			unique_components,
			{ command_stream },
			action_type,
			DEFERRED_CREATE_ENTITIES_BY_ENTITIES,
			DEFERRED_CREATE_ENTITIES_SCATTERED,
			DEFERRED_CREATE_ENTITIES_SPLATTED,
			DEFERRED_CREATE_ENTITIES_CONTIGUOUS,
			DEFERRED_CREATE_ENTITIES_BY_ENTITIES_CONTIGUOUS
		);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredCreateEntities* data = (DeferredCreateEntities*)allocation;
		buffer += sizeof(*data);

		data->exclude_from_hierarchy = exclude_from_hierarchy;

		data->unique_components.indices = (Component*)buffer;
		data->unique_components.count = unique_components.count;
		memcpy(data->unique_components.indices, unique_components.indices, sizeof(Component) * unique_components.count);
		buffer += sizeof(Component) * unique_components.count;

		data->components_with_data.indices = (Component*)buffer;
		data->components_with_data.count = components_with_data.count;
		memcpy(data->components_with_data.indices, components_with_data.indices, sizeof(Component) * components_with_data.count);
		buffer += sizeof(Component) * components_with_data.count;

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
			count,
			component_sizes,
			{ command_stream },
			action_type,
			DEFERRED_CREATE_ENTITIES_SCATTERED,
			DEFERRED_CREATE_ENTITIES_BY_ENTITIES,
			DEFERRED_CREATE_ENTITIES_SPLATTED,
			DEFERRED_CREATE_ENTITIES_CONTIGUOUS,
			DEFERRED_CREATE_ENTITIES_BY_ENTITIES_CONTIGUOUS,
			buffer
		);
		data->copy_buffers = copy_buffers;

		WriteCommandStream(this, { command_stream }, { DataPointer(allocation, action_type), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::ComponentSize(Component component) const
	{
		return m_unique_components[component.value].size;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::SharedComponentSize(Component component) const
	{
		return m_shared_components[component.value].info.size;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DeleteEntityCommit(Entity entity)
	{
		DeleteEntitiesCommit({ &entity, 1 });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DeleteEntity(Entity entity, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		DeleteEntities({ &entity, 1 }, { command_stream }, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DeleteEntitiesCommit(Stream<Entity> entities)
	{
		DeferredDeleteEntities commit_data;
		commit_data.entities = entities;
		CommitDeleteEntities(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DeleteEntities(Stream<Entity> entities, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		size_t allocation_size = sizeof(DeferredDeleteEntities);
		allocation_size += parameters.entities_are_stable ? 0 : entities.MemoryOf(entities.size);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredDeleteEntities* data = (DeferredDeleteEntities*)allocation;
		buffer += sizeof(*data);
		data->entities = GetEntitiesFromActionParameters(entities, parameters, buffer);

		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_DELETE_ENTITIES), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypeCommit(unsigned int archetype_index)
	{
		DeferredDestroyArchetype commit_data;
		commit_data.archetype_index = archetype_index;
		CommitDestroyArchetype<false>(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetype(unsigned int archetype_index, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		const Archetype* archetype = GetArchetype(archetype_index);
		DestroyArchetype(archetype->GetUniqueSignature(), archetype->GetSharedSignature(), command_stream, debug_info);
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

	void EntityManager::DestroyArchetype(
		ComponentSignature unique_components, 
		ComponentSignature shared_components,
		EntityManagerCommandStream* command_stream, 
		DebugInfo debug_info
	)
	{
		size_t allocation_size = sizeof(DeferredDestroyArchetype) + sizeof(Component) * unique_components.count + sizeof(Component) * shared_components.count;

		void* allocation = AllocateTemporaryBuffer(allocation_size);
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
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_DESTROY_ARCHETYPE), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypesCommit(Stream<unsigned int> indices)
	{
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, remapping, 512);
		remapping.Copy(indices);

		for (size_t index = 0; index < remapping.size; index++) {
			DestroyArchetypeCommit(remapping[index]);
			unsigned int swapped_archetype = m_archetypes.size;
			size_t remapping_index = function::SearchBytes(remapping.buffer + index, indices.size, swapped_archetype, sizeof(swapped_archetype));
			if (remapping_index != -1) {
				remapping[remapping_index] = remapping[index];
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypeBaseCommit(unsigned int archetype_index, unsigned int archetype_subindex)
	{
		DeferredDestroyArchetypeBase commit_data;
		commit_data.indices.x = archetype_index;
		commit_data.indices.y = archetype_subindex;
		CommitDestroyArchetypeBase<false>(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypeBase(
		unsigned int archetype_index, 
		unsigned int base_index,
		EntityManagerCommandStream* command_stream,
		DebugInfo debug_info
	)
	{
		const Archetype* archetype = GetArchetype(archetype_index);
		DestroyArchetypeBase(archetype->GetUniqueSignature(), archetype->GetSharedSignature(base_index), command_stream, debug_info);
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

	void EntityManager::DestroyArchetypeBase(
		ComponentSignature unique_components, 
		SharedComponentSignature shared_components,
		EntityManagerCommandStream* command_stream,
		DebugInfo debug_info
	)
	{
		size_t allocation_size = sizeof(DeferredDestroyArchetypeBase) + sizeof(Component) * unique_components.count +
			(sizeof(Component) + sizeof(SharedInstance)) * shared_components.count;

		void* allocation = AllocateTemporaryBuffer(allocation_size);
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
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_DESTROY_ARCHETYPE_BASE), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypesEmptyCommit()
	{
		for (unsigned int index = 0; index < GetArchetypeCount(); index++) {
			Archetype* archetype = GetArchetype(index);
			unsigned int base_count = archetype->GetBaseCount();
			bool has_entities = false;
			for (unsigned int base_index = 0; base_index < base_count; base_index++) {
				ArchetypeBase* base = archetype->GetBase(base_index);
				if (base->EntityCount() > 0) {
					has_entities = true;
					break;
				}
			}

			if (!has_entities) {
				DestroyArchetypeCommit(index);
				index--;
			}
		}
	}

	void EntityManager::DestroyArchetypesBaseEmptyCommit(bool destroy_main_archetypes)
	{
		unsigned int count = GetArchetypeCount();
		for (unsigned int index = 0; index < count; index++) {
			Archetype* archetype = GetArchetype(index);
			unsigned int base_count = archetype->GetBaseCount();
			for (unsigned int base_index = 0; base_index < base_count; base_index++) {
				ArchetypeBase* base = archetype->GetBase(base_index);
				if (base->EntityCount() == 0) {
					DestroyArchetypeBaseCommit(index, base_index);
					base_index--;
					base_count--;
				}
			}

			if (destroy_main_archetypes) {
				unsigned int base_count = archetype->GetBaseCount();
				if (base_count == 0) {
					DestroyArchetypeCommit(index);
					index--;
					count--;
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::DestroyArchetypeEmptyCommit(unsigned int main_index) {
		const Archetype* archetype = GetArchetype(main_index);
		if (archetype->GetEntityCount() == 0) {
			DestroyArchetypeCommit(main_index);
		}
	}

	void EntityManager::DestroyArchetypeBaseEmptyCommit(unsigned int main_index, unsigned int base_index, bool main_propagation) {
		const ArchetypeBase* base_archetype = GetBase(main_index, base_index);
		if (base_archetype->EntityCount() == 0) {
			DestroyArchetypeBaseCommit(main_index, base_index);
			if (main_propagation) {
				DestroyArchetypeEmptyCommit(main_index);
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::EndFrame()
	{
		Flush();
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned char EntityManager::FindArchetypeUniqueComponent(unsigned int archetype_index, Component component) const
	{
		ECS_CRASH_RETURN_VALUE(archetype_index < m_archetypes.size, UCHAR_MAX, "EntityManager: The archetype {#} is invalid when trying to find unique component {#}.",
			archetype_index, GetComponentName(component));
		return m_archetypes[archetype_index].FindUniqueComponentIndex(component);
	}

	void EntityManager::FindArchetypeUniqueComponent(unsigned int archetype_index, ComponentSignature components, unsigned char* indices) const
	{
		ECS_STACK_CAPACITY_STREAM(char, components_string, 1024);

		ECS_CRASH_RETURN(archetype_index < m_archetypes.size, "EntityManager: The archetype {#} is invalid when trying to find unique components { {#} }.", 
			archetype_index, GetComponentSignatureString(components, components_string));

		for (size_t index = 0; index < components.count; index++) {
			indices[index] = m_archetypes[archetype_index].FindUniqueComponentIndex(components[index]);
		}
	}

	void ECS_VECTORCALL EntityManager::FindArchetypeUniqueComponentVector(unsigned int archetype_index, VectorComponentSignature components, unsigned char* indices) const
	{
		ECS_STACK_CAPACITY_STREAM(char, component_string_storage, 1024);

		ECS_CRASH_RETURN(archetype_index < m_archetypes.size, "EntityManager: The archetype {#} is invalid when trying to find unique components { {#} }.", 
			archetype_index, GetComponentSignatureString(components, component_string_storage));
		// Use the fast SIMD compare
		GetArchetypeUniqueComponents(archetype_index).Find(components, indices);
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned char EntityManager::FindArchetypeSharedComponent(unsigned int archetype_index, Component component) const {
		ECS_CRASH_RETURN_VALUE(archetype_index < m_archetypes.size, UCHAR_MAX, "EntityManager: The archetype {#} is invalid when trying to find shared component {#}.",
			archetype_index, GetSharedComponentName(component));
		return m_archetypes[archetype_index].FindSharedComponentIndex(component);
	}

	void EntityManager::FindArchetypeSharedComponent(unsigned int archetype_index, ComponentSignature components, unsigned char* indices) const {
		ECS_STACK_CAPACITY_STREAM(char, component_string_storage, 1024);

		ECS_CRASH_RETURN(archetype_index < m_archetypes.size, "EntityManager: The archetype {#} is invalid when trying to find shared components { {#} }.",
			archetype_index, GetSharedComponentSignatureString(components, component_string_storage));
		for (size_t index = 0; index < components.count; index++) {
			indices[index] = m_archetypes[archetype_index].FindSharedComponentIndex(components[index]);
		}
	}

	void ECS_VECTORCALL EntityManager::FindArchetypeSharedComponentVector(unsigned int archetype_index, VectorComponentSignature components, unsigned char* indices) const {
		ECS_STACK_CAPACITY_STREAM(char, component_string_storage, 1024);
		
		ECS_CRASH_RETURN(archetype_index < m_archetypes.size, "EntityManager: The archetype {#} is invalid when trying to find shared components { {#} }.", 
			archetype_index, GetSharedComponentSignatureString(components, component_string_storage));
		// Use the fast SIMD compare
		GetArchetypeSharedComponents(archetype_index).Find(components, indices);
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::FindArchetype(ArchetypeQuery query) const
	{
		for (unsigned int index = 0; index < m_archetypes.size; index++) {
			if (query.unique == GetArchetypeUniqueComponents(index)) {
				if (query.shared == GetArchetypeSharedComponents(index)) {
					return index;
				}
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	Archetype* EntityManager::FindArchetypePtr(ArchetypeQuery query)
	{
		unsigned int archetype_index = FindArchetype(query);
		if (archetype_index != -1) {
			return GetArchetype(archetype_index);
		}
		return nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------

	uint2 EntityManager::FindBase(ArchetypeQuery query, VectorComponentSignature shared_instances) const
	{
		unsigned int main_archetype_index = FindArchetype(query);

		if (main_archetype_index != -1) {
			for (size_t base_index = 0; base_index < m_archetypes[main_archetype_index].GetBaseCount(); base_index++) {
				VectorComponentSignature archetype_shared_instances;
				archetype_shared_instances.ConvertInstances(
					m_archetypes[main_archetype_index].m_base_archetypes[base_index].shared_instances,
					m_archetypes[main_archetype_index].m_shared_components.count
				);
				if (SharedComponentSignatureHasInstances(
					m_archetypes[main_archetype_index].GetSharedSignature(),
					archetype_shared_instances,
					query.shared,
					shared_instances
				)) {
					return { main_archetype_index, (unsigned int)base_index };
				}
			}
			return { main_archetype_index, (unsigned int)-1 };
		}
		return { (unsigned int)-1, (unsigned int)-1 };
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* EntityManager::FindArchetypeBasePtr(ArchetypeQuery query, VectorComponentSignature shared_instances)
	{
		uint2 indices = FindBase(query, shared_instances);
		if (indices.y == -1) {
			return nullptr;
		}
		return GetBase(indices.x, indices.y);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::Flush() {
		unsigned int action_count = m_deferred_actions.size.load(ECS_RELAXED);
		for (unsigned int index = 0; index < action_count; index++) {
			// Set the crash handler debug info
			SetCrashHandlerCaller(m_deferred_actions[index].debug_info.file, m_deferred_actions[index].debug_info.function, m_deferred_actions[index].debug_info.line);
			DEFERRED_CALLBACKS[m_deferred_actions[index].data_and_type.GetData()](this, m_deferred_actions[index].data_and_type.GetPointer(), nullptr);
		}
		m_deferred_actions.Reset();
		ResetCrashHandlerCaller();

		// Now go through the pending command streams aswell
		unsigned int pending_command_stream_count = m_pending_command_streams.size.load(ECS_RELAXED);
		for (unsigned int index = 0; index < pending_command_stream_count; index++) {
			Flush(m_pending_command_streams[index]);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::Flush(EntityManagerCommandStream command_stream) {
		unsigned int action_count = command_stream.size;
		for (size_t index = 0; index < action_count; index++) {
			SetCrashHandlerCaller(command_stream[index].debug_info.file, command_stream[index].debug_info.function, command_stream[index].debug_info.line);
			DEFERRED_CALLBACKS[command_stream[index].data_and_type.GetData()](this, command_stream[index].data_and_type.GetPointer(), nullptr);
		}
		ResetCrashHandlerCaller();
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::ExistsEntity(Entity entity) const {
		return m_entity_pool->IsValid(entity);
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::ExistsComponent(Component component) const
	{
		return component.value < m_unique_components.size && m_unique_components[component.value].size != -1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::ExistsSharedComponent(Component component) const
	{
		return component.value < m_shared_components.size && m_shared_components[component.value].info.size != -1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::ExistsSharedInstance(Component component, SharedInstance instance) const
	{
		return ExistsSharedComponent(component) && ExistsSharedInstanceOnly(component, instance);
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool2 EntityManager::ExistsSharedInstanceEx(Component component, SharedInstance instance) const
	{
		if (!ExistsSharedComponent(component)) {
			return { false, false };
		}

		return { true, ExistsSharedInstanceOnly(component, instance) };
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::ExistsSharedInstanceOnly(Component component, SharedInstance instance) const
	{
		return m_shared_components[component.value].instances.stream.ExistsItem(instance.value);
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::GetArchetypeCount() const
	{
		return m_archetypes.size;
	}

	// --------------------------------------------------------------------------------------------------------------------

	Archetype* EntityManager::GetArchetype(unsigned int index) {
		ECS_CRASH_RETURN_VALUE(index < m_archetypes.size, nullptr, "EntityManager: Invalid archetype index {#} when trying to retrive archetype pointer.", index);
		return &m_archetypes[index];
	}

	// --------------------------------------------------------------------------------------------------------------------

	const Archetype* EntityManager::GetArchetype(unsigned int index) const {
		ECS_CRASH_RETURN_VALUE(index < m_archetypes.size, nullptr, "EntityManager: Invalid archetype index {#} when trying to retrive archetype pointer.", index);
		return &m_archetypes[index];
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeBase* EntityManager::GetBase(unsigned int main_index, unsigned int base_index) {
		Archetype* archetype = GetArchetype(main_index);
		return archetype->GetBase(base_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const ArchetypeBase* EntityManager::GetBase(unsigned int main_index, unsigned int base_index) const {
		const Archetype* archetype = GetArchetype(main_index);
		return archetype->GetBase(base_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// It requires a syncronization barrier!! If the archetype does not exist, then it will commit the creation of a new one
	unsigned int EntityManager::FindOrCreateArchetype(ComponentSignature unique_signature, ComponentSignature shared_signature) {
		unsigned int archetype_index = FindArchetype({unique_signature, shared_signature});
		if (archetype_index != -1) {
			return archetype_index;
		}
		// A new archetype must be created
		return CreateArchetypeCommit(unique_signature, shared_signature);
	}

	// --------------------------------------------------------------------------------------------------------------------

	uint2 EntityManager::FindOrCreateArchetypeBase(
		ComponentSignature unique_signature,
		SharedComponentSignature shared_signature,
		unsigned int starting_size
	) {
		unsigned int main_archetype_index = FindOrCreateArchetype(unique_signature, { shared_signature.indices, shared_signature.count });
		Archetype* main_archetype = GetArchetype(main_archetype_index);
		unsigned int base = main_archetype->FindBaseIndex(shared_signature);
		if (base == -1) {
			// It doesn't exist, a new one must be created
			base = main_archetype->CreateBaseArchetype(shared_signature, starting_size);
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
		unsigned int starting_size
	) {
		uint2 indices = FindOrCreateArchetypeBase(unique_signature, shared_signature, starting_size);
		return GetBase(indices.x, indices.y);
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename Query>
	void ECS_VECTORCALL GetArchetypeImplementation(const EntityManager* manager, Query query, CapacityStream<unsigned int>& archetypes) {
		for (size_t index = 0; index < manager->m_archetypes.size; index++) {
			if (query.VerifiesUnique(manager->m_archetypes[index].GetUniqueSignature())) {
				if (query.VerifiesShared(manager->m_archetypes[index].GetSharedSignature())) {
					ECS_CRASH_RETURN(archetypes.size < archetypes.capacity, "EntityManager: Not enough space for capacity stream when getting archetype indices.");
					archetypes.Add(index);
				}
			}
		}
	}

	template<typename Query>
	void ECS_VECTORCALL GetArchetypePtrsImplementation(const EntityManager* manager, Query query, CapacityStream<Archetype*>& archetypes) {
		for (size_t index = 0; index < manager->m_archetypes.size; index++) {
			if (query.VerifiesUnique(manager->m_archetypes[index].GetUniqueSignature())) {
				if (query.VerifiesShared(manager->m_archetypes[index].GetSharedSignature())) {
					ECS_CRASH_RETURN(archetypes.size < archetypes.capacity, "EntityManager: Not enough space for capacity stream when getting archetype pointers.");
					archetypes.Add(manager->m_archetypes.buffer + index);
				}
			}
		}
	}

	void EntityManager::GetArchetypes(ArchetypeQuery query, CapacityStream<unsigned int>& archetypes) const
	{
		GetArchetypeImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetArchetypesPtrs(ArchetypeQuery query, CapacityStream<Archetype*>& archetypes) const
	{
		GetArchetypePtrsImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetArchetypes(ArchetypeQueryExclude query, CapacityStream<unsigned int>& archetypes) const
	{
		GetArchetypeImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetArchetypesPtrs(ArchetypeQueryExclude query, CapacityStream<Archetype*>& archetypes) const
	{
		GetArchetypePtrsImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetComponent(Entity entity, Component component)
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetBase(info.main_archetype, info.base_archetype)->GetComponent(info, component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const void* EntityManager::GetComponent(Entity entity, Component component) const
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetBase(info.main_archetype, info.base_archetype)->GetComponent(info, component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetComponentWithIndex(Entity entity, unsigned char component_index)
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetBase(info.main_archetype, info.base_archetype)->GetComponentByIndex(info, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const void* EntityManager::GetComponentWithIndex(Entity entity, unsigned char component_index) const
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetBase(info.main_archetype, info.base_archetype)->GetComponentByIndex(info, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetComponent(Entity entity, ComponentSignature components, void** data) const
	{
		EntityInfo info = GetEntityInfo(entity);
		const ArchetypeBase* base = GetBase(info.main_archetype, info.base_archetype);
		for (size_t index = 0; index < components.count; index++) {
			data[index] = (void*)base->GetComponent(info, components.indices[index]);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetComponentWithIndex(Entity entity, ComponentSignature components, void** data) const
	{
		EntityInfo info = GetEntityInfo(entity);
		const ArchetypeBase* base = GetBase(info.main_archetype, info.base_archetype);
		for (size_t index = 0; index < components.count; index++) {
			data[index] = (void*)base->GetComponentByIndex(info, components.indices[index].value);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetSharedComponent(Entity entity, Component component) {
		return (void*)((const EntityManager*)this)->GetSharedComponent(entity, component);
	}

	const void* EntityManager::GetSharedComponent(Entity entity, Component component) const {
		SharedInstance instance = GetSharedComponentInstance(component, entity);
		return GetSharedData(component, instance);
	}

	// --------------------------------------------------------------------------------------------------------------------

	MemoryArena* EntityManager::GetComponentAllocator(Component component)
	{
		ECS_CRASH_RETURN_VALUE(ExistsComponent(component), nullptr, "The component {#} doesn't exist when retrieving its allocator.", GetComponentName(component));
		return m_unique_components[component.value].allocator;
	}

	// --------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic EntityManager::GetComponentAllocatorPolymorphic(Component component)
	{
		return GetAllocatorPolymorphic(GetComponentAllocator(component));
	}

	// --------------------------------------------------------------------------------------------------------------------

	MemoryArena* EntityManager::GetSharedComponentAllocator(Component component)
	{
		ECS_CRASH_RETURN_VALUE(ExistsSharedComponent(component), nullptr, "The shared component {#} doesn't exist when retrieving its allocator.", GetSharedComponentName(component));
		return m_shared_components[component.value].info.allocator;
	}

	// --------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic EntityManager::GetSharedComponentAllocatorPolymorphic(Component component)
	{
		return GetAllocatorPolymorphic(GetSharedComponentAllocator(component));
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::GetEntityCount() const
	{
		return m_entity_pool->GetCount();
	}

	// --------------------------------------------------------------------------------------------------------------------

	Entity EntityManager::GetEntityFromIndex(unsigned int stream_index) const
	{
		Entity entity;
		entity.index = stream_index;
		EntityInfo info = m_entity_pool->GetInfoNoChecks(entity);
		return { stream_index, info.generation_count };
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

	SharedComponentSignature EntityManager::GetEntitySharedSignature(Entity entity, Component* shared, SharedInstance* instances) const {
		SharedComponentSignature signature = GetEntitySharedSignatureStable(entity);
		memcpy(shared, signature.indices, sizeof(Component) * signature.count);
		memcpy(instances, signature.instances, sizeof(SharedInstance) * signature.count);
		return { shared, instances, signature.count };
	}

	// --------------------------------------------------------------------------------------------------------------------
	
	SharedComponentSignature EntityManager::GetEntitySharedSignatureStable(Entity entity) const {
		EntityInfo info = GetEntityInfo(entity);
		ComponentSignature shared_signature = m_archetypes[info.main_archetype].GetSharedSignature();
		const SharedInstance* instances = m_archetypes[info.main_archetype].GetBaseInstances(info.base_archetype);
		return { shared_signature.indices, (SharedInstance*)instances, shared_signature.count };
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetEntityCompleteSignature(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared) const {
		ComponentSignature stable_unique = GetEntitySignatureStable(entity);
		SharedComponentSignature stable_shared = GetEntitySharedSignatureStable(entity);

		memcpy(unique->indices, stable_unique.indices, sizeof(Component) * stable_unique.count);
		unique->count = stable_unique.count;

		memcpy(shared->indices, stable_shared.indices, sizeof(Component) * stable_shared.count);
		memcpy(shared->instances, stable_shared.instances, sizeof(SharedInstance) * stable_shared.count);
		shared->count = stable_shared.count;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetEntityCompleteSignatureStable(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared) const {
		*unique = GetEntitySignatureStable(entity);
		*shared = GetEntitySharedSignatureStable(entity);
	}

	// --------------------------------------------------------------------------------------------------------------------

	Stream<char> EntityManager::GetComponentName(Component component) const
	{
		return component.value >= 0 && component.value < m_unique_components.size ? m_unique_components[component.value].name : "Invalid component";
	}

	Stream<char> EntityManager::GetSharedComponentName(Component component) const
	{
		return component.value >= 0 && component.value < m_shared_components.size ? m_shared_components[component.value].info.name : "Invalid component";
	}

	// --------------------------------------------------------------------------------------------------------------------

	Stream<char> EntityManager::GetComponentSignatureString(ComponentSignature signature, CapacityStream<char>& storage, Stream<char> separator) const
	{
		unsigned int initial_size = storage.size;
		for (unsigned char index = 0; index < signature.count; index++) {
			storage.AddStreamSafe(GetComponentName(signature[index]));
			if (index < signature.count - 1) {
				storage.AddStreamSafe(separator);
			}
		}

		return { storage.buffer + initial_size, storage.size - initial_size };
	}

	// --------------------------------------------------------------------------------------------------------------------

	Stream<char> EntityManager::GetSharedComponentSignatureString(ComponentSignature signature, CapacityStream<char>& storage, Stream<char> separator) const
	{
		unsigned int initial_size = storage.size;
		
		for (unsigned char index = 0; index < signature.count; index++) {
			storage.AddStreamSafe(GetSharedComponentName(signature[index]));
			if (index < signature.count - 1) {
				storage.AddStreamSafe(separator);
			}
		}

		return { storage.buffer + initial_size, storage.size - initial_size };
	}

	// --------------------------------------------------------------------------------------------------------------------

	Stream<char> EntityManager::GetComponentSignatureString(VectorComponentSignature signature, CapacityStream<char>& storage, Stream<char> separator) const
	{
		ECS_STACK_CAPACITY_STREAM(Component, components, ECS_ARCHETYPE_MAX_COMPONENTS);
		ComponentSignature normal_signature = signature.ToNormalSignature(components.buffer);
		return GetComponentSignatureString(normal_signature, storage, separator);
	}

	// --------------------------------------------------------------------------------------------------------------------

	Stream<char> EntityManager::GetSharedComponentSignatureString(VectorComponentSignature signature, CapacityStream<char>& storage, Stream<char> separator) const
	{
		ECS_STACK_CAPACITY_STREAM(Component, components, ECS_ARCHETYPE_MAX_COMPONENTS);
		ComponentSignature normal_signature = signature.ToNormalSignature(components.buffer);
		return GetSharedComponentSignatureString(normal_signature, storage, separator);
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename Query>
	void ECS_VECTORCALL GetEntitiesImplementation(const EntityManager* manager, Query query, CapacityStream<Entity>* entities) {
		for (size_t index = 0; index < manager->m_archetypes.size; index++) {
			if (query.VerifiesUnique(manager->m_archetypes[index].GetUniqueSignature())) {
				if (query.VerifiesShared(manager->m_archetypes[index].GetSharedSignature())) {
					// Matches the query
					for (size_t base_index = 0; base_index < manager->m_archetypes[index].GetBaseCount(); base_index++) {
						const ArchetypeBase* base = manager->GetBase(index, base_index);
						ECS_CRASH_RETURN(entities->size + base->m_size <= entities->capacity, "EntityManager: Not enough space for capacity stream when retrieving entities.");
						base->GetEntitiesCopy(entities->buffer + entities->size);
						entities->size += base->m_size;
					}
				}
			}
		}
	}

	template<typename Query>
	void ECS_VECTORCALL GetEntitiesImplementation(const EntityManager* manager, Query query, CapacityStream<Entity*>* entities) {
		for (size_t index = 0; index < manager->m_archetypes.size; index++) {
			if (query.VerifiesUnique(manager->m_archetypes[index].GetUniqueSignature())) {
				if (query.VerifiesShared(manager->m_archetypes[index].GetSharedSignature())) {
					// Matches the query
					for (size_t base_index = 0; base_index < manager->m_archetypes[index].GetBaseCount(); base_index++) {
						const ArchetypeBase* base = manager->GetBase(index, base_index);
						ECS_CRASH_RETURN(entities->size < entities->capacity, "EntityManager: Not enough space for capacity stream when retrieving entities' pointers.");
						entities->Add(base->m_entities);
					}
				}
			}
		}
	}

	void EntityManager::GetEntities(ArchetypeQuery query, CapacityStream<Entity>* entities) const
	{
		GetEntitiesImplementation(this, query, entities);
	}

	void EntityManager::GetEntities(ArchetypeQuery query, CapacityStream<Entity*>* entities) const
	{
		GetEntitiesImplementation(this, query, entities);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetEntitiesExclude(ArchetypeQueryExclude query, CapacityStream<Entity>* entities) const
	{
		GetEntitiesImplementation(this, query, entities);
	}

	void EntityManager::GetEntitiesExclude(ArchetypeQueryExclude query, CapacityStream<Entity*>* entities) const
	{
		GetEntitiesImplementation(this, query, entities);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetSharedData(Component component, SharedInstance instance)
	{
		return (void*)((const EntityManager*)this)->GetSharedData(component, instance);
	}

	const void* EntityManager::GetSharedData(Component component, SharedInstance instance) const
	{
		ECS_CRASH_RETURN_VALUE(ExistsSharedComponent(component), nullptr, "EntityManager: Shared component {#} is invalid when trying to retrieve instance {#} data.",
			GetSharedComponentName(component), instance.value);
		ECS_CRASH_RETURN_VALUE(m_shared_components[component.value].instances.stream.ExistsItem(instance.value), nullptr, "EntityManager: Shared instance "
			"{#} for component {#} is invalid.", instance.value, GetSharedComponentName(component));
		return m_shared_components[component.value].instances[instance.value];
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetNamedSharedData(Component component, Stream<char> identifier)
	{
		return (void*)((const EntityManager*)this)->GetNamedSharedData(component, identifier);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const void* EntityManager::GetNamedSharedData(Component component, Stream<char> identifier) const
	{
		SharedInstance instance;
		if (m_shared_components[component.value].named_instances.TryGetValue(identifier, instance)) {
			return GetSharedData(component, instance);
		}
		return nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::GetSharedComponentInstance(Component component, const void* data) const
	{
		ECS_CRASH_RETURN_VALUE(component.value < m_shared_components.size, { -1 }, "EntityManager: Shared component {#} is out of bounds when trying to"
			" get shared instance data.", component.value);
		unsigned int component_size = m_shared_components[component.value].info.size;
		
		// If the component size is -1, it means no actual component lives at that index
		ECS_CRASH_RETURN_VALUE(component_size != -1, { -1 },  "EntityManager: There is no shared component allocated at {#}. Cannot retrieve shared instance data.",
			component.value);

		short instance_index = -1;
		m_shared_components[component.value].instances.stream.ForEachIndex<true>([&](unsigned int index) {
			const void* current_data = m_shared_components[component.value].instances[index];
			if (memcmp(current_data, data, component_size) == 0) {
				instance_index = (short)index;
				return true;
			}
			return false;
		});

		return { instance_index };
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::GetSharedComponentInstance(Component component, Entity entity) const
	{
		ECS_CRASH_RETURN_VALUE(HasSharedComponent(entity, component), { -1 }, "EntityManager: The entity {#} doesn't have the shared component {#}.",
			entity, GetSharedComponentName(component));

		EntityInfo info = GetEntityInfo(entity);
		const Archetype* archetype = GetArchetype(info.main_archetype);
		return archetype->GetBaseInstance(component, info.base_archetype);
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::GetNamedSharedComponentInstance(Component component, Stream<char> identifier) const {
		ECS_CRASH_RETURN_VALUE(component.value < m_shared_components.size, { -1 }, "EntityManager: Shared component {#} is out of bounds when trying to retrieve named "
			"shared instance.", component.value);
		unsigned int component_size = m_shared_components[component.value].info.size;

		// If the component size is -1, it means no actual component lives at that index
		ECS_CRASH_RETURN_VALUE(component_size != -1, { -1 }, "EntityManager: There is no shared component allocated at {#} when trying to retrieve named shared instance.", 
			component.value);
		SharedInstance instance;
		ECS_CRASH_RETURN_VALUE(m_shared_components[component.value].named_instances.TryGetValue(identifier, instance), { -1 }, "EntityManager: There is no named shared"
			" instance {#} of shared component type {#}.", identifier, GetSharedComponentName(component));

		return instance;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetSharedComponentInstanceAll(Component component, CapacityStream<SharedInstance>& shared_instances) const
	{
		m_shared_components[component.value].instances.stream.ForEachIndex([&](unsigned int index) {
			SharedInstance instance = { (short)index };
			shared_instances.AddAssert(instance);
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	VectorComponentSignature EntityManager::GetArchetypeUniqueComponents(unsigned int archetype_index) const
	{
		return GetEntityManagerUniqueVectorSignature(m_archetype_vector_signatures, archetype_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	VectorComponentSignature EntityManager::GetArchetypeSharedComponents(unsigned int archetype_index) const
	{
		return GetEntityManagerSharedVectorSignature(m_archetype_vector_signatures, archetype_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	VectorComponentSignature* EntityManager::GetArchetypeUniqueComponentsPtr(unsigned int archetype_index)
	{
		return GetEntityManagerUniqueVectorSignaturePtr(m_archetype_vector_signatures, archetype_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	VectorComponentSignature* EntityManager::GetArchetypeSharedComponentsPtr(unsigned int archetype_index)
	{
		return GetEntityManagerSharedVectorSignaturePtr(m_archetype_vector_signatures, archetype_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	Entity EntityManager::GetEntityParent(Entity child) const
	{
		return m_hierarchy.GetParent(child);
	}

	// --------------------------------------------------------------------------------------------------------------------

	Stream<Entity> EntityManager::GetHierarchyChildren(Entity parent, Entity* static_storage) const
	{
		return m_hierarchy.GetChildren(parent, static_storage);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetHierarchyChildrenCopy(Entity parent, CapacityStream<Entity>& children) const
	{
		Stream<Entity> parent_children = GetHierarchyChildren(parent);
		ECS_CRASH_RETURN(
			children.capacity >= parent_children.size, 
			"EntityManager: Not enough space to copy the children for entity {#} into the capacity stream. Current capacity {#}, size needed {#}.",
			parent.value,
			children.capacity,
			parent_children.size
		);

		if (parent_children.size > 0) {
			children.AddStream(parent_children);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	Stream<unsigned int> EntityManager::GetQueryResults(unsigned int handle) const
	{
		return m_query_cache->GetResults(handle);
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeQuery ECS_VECTORCALL EntityManager::GetQueryComponents(unsigned int handle) const
	{
		return m_query_cache->GetComponents(handle);
	}

	// --------------------------------------------------------------------------------------------------------------------

	ArchetypeQueryResult EntityManager::GetQueryResultsAndComponents(unsigned int handle) const
	{
		return m_query_cache->GetResultsAndComponents(handle);
	}

	// --------------------------------------------------------------------------------------------------------------------

	Component EntityManager::GetMaxComponent() const {
		for (unsigned int index = m_unique_components.size; index > 0; index--) {
			Component current_component = { (short)(index - 1) };
			if (ExistsComponent(current_component)) {
				return current_component;
			}
		}
		return { -1 };
	}

	// --------------------------------------------------------------------------------------------------------------------

	Component EntityManager::GetMaxSharedComponent() const {
		for (unsigned int index = m_shared_components.size; index > 0; index--) {
			Component current_component = { (short)(index - 1) };
			if (ExistsSharedComponent(current_component)) {
				return current_component;
			}
		}

		return { -1 };
	}


	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::HasEntityTag(Entity entity, unsigned char tag) const
	{
		return m_entity_pool->HasTag(entity, tag);
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::HasComponent(Entity entity, Component component) const
	{
		EntityInfo info = m_entity_pool->GetInfo(entity);
		return m_archetypes[info.main_archetype].FindUniqueComponentIndex(component) != UCHAR_MAX;
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::HasComponents(Entity entity, VectorComponentSignature components) const
	{
		EntityInfo info = m_entity_pool->GetInfo(entity);
		return GetArchetypeUniqueComponents(info.main_archetype).HasComponents(components);
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::HasSharedComponent(Entity entity, Component component) const
	{
		EntityInfo info = GetEntityInfo(entity);
		return m_archetypes[info.main_archetype].FindSharedComponentIndex(component) != UCHAR_MAX;
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::HasSharedComponents(Entity entity, VectorComponentSignature components) const
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetArchetypeSharedComponents(info.main_archetype).HasComponents(components);
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::HasSharedInstance(Entity entity, Component component, SharedInstance shared_instance) const
	{
		EntityInfo info = GetEntityInfo(entity);
		unsigned char component_index = m_archetypes[info.main_archetype].FindSharedComponentIndex(component);
		if (component_index == UCHAR_MAX) {
			return false;
		}

		return m_archetypes[info.main_archetype].GetBaseInstances(info.base_archetype)[component_index] == shared_instance;
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::HasSharedInstances(Entity entity, VectorComponentSignature components, const SharedInstance* instances, unsigned char component_count) const
	{
		EntityInfo info = GetEntityInfo(entity);
		VectorComponentSignature archetype_components = GetArchetypeSharedComponents(info.main_archetype);

		unsigned char component_indices[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		archetype_components.Find(components, component_indices);
		const SharedInstance* archetype_instances = m_archetypes[info.main_archetype].GetBaseInstances(info.base_archetype);
		for (unsigned char index = 0; index < component_count; index++) {
			if (component_indices[index] == UCHAR_MAX) {
				return false;
			}

			if (archetype_instances[component_indices[index]] != instances[index]) {
				return false;
			}
		}

		return true;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RemoveEntityFromHierarchyCommit(Stream<Entity> entities, bool default_child_destroy)
	{
		DeferredRemoveEntitiesFromHierarchy data;
		data.default_destroy_children = default_child_destroy;
		data.entities = entities;
		CommitRemoveEntitiesFromHierarchy(this, &data, nullptr);
	}

	void EntityManager::RemoveEntityFromHierarchy(
		Stream<Entity> entities, 
		DeferredActionParameters parameters, 
		bool default_child_destroy,
		DebugInfo debug_info
	)
	{
		DeferredRemoveEntitiesFromHierarchy* data = nullptr;
		if (parameters.entities_are_stable) {
			data = (DeferredRemoveEntitiesFromHierarchy*)AllocateTemporaryBuffer(sizeof(DeferredRemoveEntitiesFromHierarchy));
			data->entities = entities;
		}
		else {
			data = (DeferredRemoveEntitiesFromHierarchy*)AllocateTemporaryBuffer(sizeof(DeferredRemoveEntitiesFromHierarchy) + sizeof(Entity) * entities.size);
			data->entities.buffer = (Entity*)function::OffsetPointer(data, sizeof(DeferredRemoveEntitiesFromHierarchy));
			entities.CopyTo(data->entities.buffer);
			data->entities.size = entities.size;
		}

		data->default_destroy_children = default_child_destroy;
		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_REMOVE_ENTITIES_FROM_HIERARCHY), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RemoveComponentCommit(Entity entity, ComponentSignature components)
	{
		RemoveComponentCommit({ &entity, 1 }, components);
	}

	void EntityManager::RemoveComponentCommit(Stream<Entity> entities, ComponentSignature components)
	{
		DeferredRemoveComponentEntities commit_data;
		commit_data.components = components;
		commit_data.entities = entities;
		CommitEntityRemoveComponent(this, &commit_data, nullptr);
	}

	void EntityManager::RemoveComponent(Entity entity, ComponentSignature components, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		RemoveComponent({ &entity, 1 }, components, { command_stream }, debug_info);
	}

	void EntityManager::RemoveComponent(Stream<Entity> entities, ComponentSignature components, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		size_t allocation_size = sizeof(DeferredRemoveComponentEntities) + sizeof(Component) * components.count;
		allocation_size += parameters.entities_are_stable ? 0 : entities.MemoryOf(entities.size);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredRemoveComponentEntities* data = (DeferredRemoveComponentEntities*)allocation;
		buffer += sizeof(DeferredRemoveComponentEntities);

		data->components = components.Copy(buffer);
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_ENTITY_REMOVE_COMPONENT), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RemoveSharedComponentCommit(Entity entity, ComponentSignature components)
	{
		RemoveSharedComponentCommit({ &entity, 1 }, components);
	}

	void EntityManager::RemoveSharedComponentCommit(Stream<Entity> entities, ComponentSignature components)
	{
		DeferredRemoveSharedComponentEntities commit_data;
		commit_data.components = components;
		commit_data.entities = entities;
		CommitEntityRemoveSharedComponent(this, &commit_data, nullptr);
	}

	void EntityManager::RemoveSharedComponent(Entity entity, ComponentSignature components, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		RemoveSharedComponent({ &entity, 1 }, components, { command_stream }, debug_info);
	}

	void EntityManager::RemoveSharedComponent(Stream<Entity> entities, ComponentSignature components, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		size_t allocation_size = sizeof(DeferredRemoveSharedComponentEntities) + sizeof(Component) * components.count;
		allocation_size += parameters.entities_are_stable ? 0 : entities.MemoryOf(entities.size);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t buffer = (uintptr_t)allocation;
		DeferredRemoveSharedComponentEntities* data = (DeferredRemoveSharedComponentEntities*)allocation;
		buffer += sizeof(*data);

		data->components.count = components.count;
		data->components.indices = (Component*)buffer;
		memcpy(data->components.indices, components.indices, sizeof(Component) * data->components.count);
		buffer += sizeof(Component) * data->components.count;

		buffer = function::AlignPointer(buffer, alignof(Entity));
		data->entities = GetEntitiesFromActionParameters(entities, parameters, buffer);

		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_ENTITY_REMOVE_SHARED_COMPONENT), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterComponentCommit(
		Component component, 
		unsigned int size, 
		size_t allocator_size, 
		Stream<char> name,
		Stream<ComponentBuffer> component_buffers
	) {
		DeferredCreateComponent commit_data;
		commit_data.component = component;
		commit_data.size = size;
		commit_data.allocator_size = allocator_size;
		commit_data.name = name;
		commit_data.component_buffers_count = component_buffers.size;
		component_buffers.CopyTo(commit_data.component_buffers);

		CommitCreateComponent(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterComponent(
		Component component, 
		unsigned int size, 
		size_t allocator_size,
		Stream<char> name,
		Stream<ComponentBuffer> buffer_offsets, 
		EntityManagerCommandStream* command_stream, 
		DebugInfo debug_info
	) {
		size_t allocation_size = sizeof(DeferredCreateComponent) + name.size;

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t allocation_ptr = (uintptr_t)allocation;

		DeferredCreateComponent* data = (DeferredCreateComponent*)allocation;
		data->component = component;
		data->size = size;
		data->allocator_size = allocator_size;
		if (buffer_offsets.size > 0) {
			data->component_buffers_count = buffer_offsets.size;
			buffer_offsets.CopyTo(data->component_buffers);
		}
		else {
			data->component_buffers_count = 0;
		}
		allocation_ptr += sizeof(*data);
		data->name.InitializeAndCopy(allocation_ptr, name);

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_CREATE_COMPONENT), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterSharedComponentCommit(
		Component component, 
		unsigned int size, 
		size_t allocator_size,
		Stream<char> name,
		Stream<ComponentBuffer> component_buffers
	) {
		DeferredCreateSharedComponent commit_data;
		commit_data.component = component;
		commit_data.size = size;
		commit_data.allocator_size = allocator_size;
		commit_data.name = name;
		commit_data.component_buffers_count = component_buffers.size;
		component_buffers.CopyTo(commit_data.component_buffers);

		CommitCreateSharedComponent(this, &commit_data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterSharedComponent(
		Component component, 
		unsigned int size, 
		size_t allocator_size,
		Stream<char> name,
		Stream<ComponentBuffer> buffer_offsets,
		EntityManagerCommandStream* command_stream,
		DebugInfo debug_info
	) {
		size_t allocation_size = sizeof(DeferredCreateSharedComponent) + name.size;

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t allocation_ptr = (uintptr_t)allocation_ptr;
		DeferredCreateSharedComponent* data = (DeferredCreateSharedComponent*)allocation;
		data->component = component;
		data->size = size;
		data->allocator_size = allocator_size;
		data->component_buffers_count = buffer_offsets.size;
		buffer_offsets.CopyTo(data->component_buffers);

		allocation_ptr += sizeof(*data);
		data->name.InitializeAndCopy(allocation_ptr, name);

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_CREATE_SHARED_COMPONENT), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::RegisterSharedInstanceCommit(Component component, const void* data, bool copy_buffers) {
		DeferredCreateSharedInstance commit_data;
		commit_data.component = component;
		commit_data.data = data;
		commit_data.copy_buffers = copy_buffers;

		SharedInstance instance;
		CommitCreateSharedInstance(this, &commit_data, &instance);
		
		return instance;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterSharedInstance(
		Component component, 
		const void* instance_data, 
		bool copy_buffers, 
		EntityManagerCommandStream* command_stream, 
		DebugInfo debug_info
	) {
		size_t allocation_size = sizeof(DeferredCreateSharedInstance) + m_shared_components[component.value].info.size;

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		DeferredCreateSharedInstance* data = (DeferredCreateSharedInstance*)allocation;
		data->component = component;
		data->data = function::OffsetPointer(allocation, sizeof(DeferredCreateSharedInstance));
		memcpy((void*)data->data, instance_data, m_shared_components[component.value].info.size);

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_CREATE_SHARED_INSTANCE), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::RegisterNamedSharedInstanceCommit(Component component, Stream<char> identifier, const void* data, bool copy_buffers) {
		DeferredCreateNamedSharedInstance commit_data;
		commit_data.component = component;
		commit_data.identifier = identifier;
		commit_data.data = data;
		commit_data.copy_buffers = copy_buffers;

		SharedInstance instance;
		CommitCreateNamedSharedInstance(this, &commit_data, &instance);

		return instance;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterNamedSharedInstance(
		Component component, 
		Stream<char> identifier, 
		const void* instance_data,
		bool copy_buffers,
		EntityManagerCommandStream* command_stream,
		DebugInfo debug_info
	) {
		unsigned int component_size = SharedComponentSize(component);
		size_t allocation_size = sizeof(DeferredCreateNamedSharedInstance) + component_size + identifier.size;

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t allocation_ptr = (uintptr_t)allocation;

		DeferredCreateNamedSharedInstance* data = (DeferredCreateNamedSharedInstance*)allocation;
		allocation_ptr += sizeof(*data);

		data->component = component;
		data->data = (void*)allocation_ptr;
		memcpy((void*)data->data, instance_data, component_size);
		allocation_ptr += component_size;

		data->identifier.InitializeAndCopy(allocation_ptr, identifier);

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_CREATE_NAMED_SHARED_INSTANCE), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterNamedSharedInstanceCommit(Component component, Stream<char> identifier, SharedInstance instance)
	{
		DeferredBindNamedSharedInstance commit_data;
		commit_data.component = component;
		commit_data.instance = instance;
		commit_data.identifier = identifier;

		CommitBindNamedSharedInstance(this, &commit_data, nullptr);
	}

	void EntityManager::RegisterNamedSharedInstance(Component component, Stream<char> identifier, SharedInstance instance, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		size_t allocation_size = sizeof(DeferredBindNamedSharedInstance) + identifier.size;

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t allocation_ptr = (uintptr_t)allocation;
		DeferredBindNamedSharedInstance* data = (DeferredBindNamedSharedInstance*)allocation;
		allocation_ptr += sizeof(*data);

		data->component = component;
		data->instance = instance;
		data->identifier.InitializeAndCopy(allocation_ptr, identifier);

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_CREATE_NAMED_SHARED_INSTANCE), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<typename Query>
	unsigned int RegisterQueryImplementation(EntityManager* entity_manager, Query query) {
		// The AddQuery already retrieves all matching initial archetypes
		return entity_manager->m_query_cache->AddQuery(query);
	}

	unsigned int EntityManager::RegisterQuery(ArchetypeQuery query)
	{
		return RegisterQueryImplementation(this, query);
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::RegisterQuery(ArchetypeQueryExclude query)
	{
		return RegisterQueryImplementation(this, query);
	}

	void EntityManager::RestoreQueryCache(const ArchetypeQueryCache* query_cache)
	{
		ClearCache();
		m_query_cache->CopyOther(query_cache);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterPendingCommandStream(EntityManagerCommandStream command_stream)
	{
		m_pending_command_streams.Add(command_stream);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ResizeSharedComponent(Component component, unsigned int new_size)
	{
		ECS_CRASH_RETURN(ExistsSharedComponent(component), "EntityManager: There is no shared component {#} when trying to resize it.", component.value);

		m_shared_components[component.value].info.size = new_size;
		m_shared_components[component.value].instances.stream.ForEach([&](void*& data) {
			m_small_memory_manager.Deallocate(data);
			void* new_allocation = m_small_memory_manager.Allocate(new_size);
			data = new_allocation;
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<bool shared = false>
	MemoryArena* ResizeComponentAllocator(EntityManager* entity_manager, Component component, size_t new_allocation_size) {
		bool exists;
		if constexpr (shared) {
			exists = entity_manager->ExistsSharedComponent(component);
		}
		else {
			exists = entity_manager->ExistsComponent(component);
		}

		const char* crash_string = shared ? "EntityManager: There is no shared component {#} when trying to resize its allocator." :
			"EntityManager: There is no component {#} when trying to resize its allocator.";
		ECS_CRASH_RETURN_VALUE(exists, nullptr, crash_string, component.value);

		MemoryArena* previous_arena;
		if constexpr (shared) {
			previous_arena = entity_manager->GetSharedComponentAllocator(component);
		}
		else {
			previous_arena = entity_manager->GetComponentAllocator(component);
		}

		if (previous_arena != nullptr) {
			entity_manager->m_memory_manager->Deallocate(previous_arena);
		}

		MemoryArena* arena = nullptr;

		ComponentInfo* info = nullptr;
		if constexpr (shared) {
			info = &entity_manager->m_shared_components[component.value].info;
		}
		else {
			info = &entity_manager->m_unique_components[component.value];
		}
		CreateAllocatorForComponent(entity_manager, *info, new_allocation_size);
		return info->allocator;
	}

	MemoryArena* EntityManager::ResizeComponentAllocator(Component component, size_t new_allocation_size)
	{
		return ECSEngine::ResizeComponentAllocator<false>(this, component, new_allocation_size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	MemoryArena* EntityManager::ResizeSharedComponentAllocator(Component component, size_t new_allocation_size)
	{
		return ECSEngine::ResizeComponentAllocator<true>(this, component, new_allocation_size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::Reset() {
		// Free the allocator of the query cache
		FreeAllocatorFrom(m_query_cache->allocator, GetAllocatorPolymorphic(m_memory_manager->m_backup));

		m_hierarchy_allocator->Free();
		m_memory_manager->Clear();
		m_entity_pool->Reset();

		EntityManagerDescriptor descriptor;
		descriptor.memory_manager = m_memory_manager;
		descriptor.entity_pool = m_entity_pool;
		descriptor.deferred_action_capacity = m_deferred_actions.capacity;
		*this = EntityManager(descriptor);

		// Change the allocator of the hierarchy allocator, is going to point to the temporary memory. The roots allocator needs to be changed too
		// The same goes for the query cache with the entity manager reference
		m_hierarchy.allocator = m_hierarchy_allocator;
		m_hierarchy.roots.allocator = GetAllocatorPolymorphic(m_hierarchy_allocator);

		// The allocator for the query cache is stable because it is allocated from the memory manager
		m_query_cache->entity_manager = this;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::SetSharedComponentData(Component component, SharedInstance instance, const void* data)
	{
		ECS_CRASH_RETURN(ExistsSharedComponent(component), "EntityManager: The component {#} doesn't exist when trying to set shared component data.", component.value);

		ECS_CRASH_RETURN(ExistsSharedInstanceOnly(component, instance), "EntityManager: The instance {#} doesn't exist when trying to set "
			"shared component data for component {#}.", GetSharedComponentName(component), instance.value);

		unsigned int component_size = SharedComponentSize(component);
		memcpy(m_shared_components[component.value].instances[instance.value], data, component_size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::SetNamedSharedComponentData(Component component, Stream<char> identifier, const void* data) {
		ECS_CRASH_RETURN(component.value < m_shared_components.size, "EntityManager: Shared component {#} is out of bounds when trying to set named shared instance {#}.",
			component.value, identifier);

		ECS_CRASH_RETURN(ExistsSharedComponent(component), "EntityManager: Shared component {#} does not exist when trying to set named shared instance {#}.", 
			component.value, identifier);

		SharedInstance instance;
		ECS_CRASH_RETURN(
			m_shared_components[component.value].named_instances.TryGetValue(identifier, instance), 
			"EntityManager: Shared component {#} does not have named shared instance {#}.", 
			GetSharedComponentName(component), 
			identifier
		);

		unsigned int component_size = SharedComponentSize(component);
		memcpy(m_shared_components[component.value].instances[instance.value], data, component_size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::SetEntityTagCommit(Stream<Entity> entities, unsigned char tag)
	{
		DeferredSetEntityTag data;
		data.entities = entities;
		data.tag = tag;
		CommitSetEntityTags(this, &data, nullptr);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::SetEntityTag(Stream<Entity> entities, unsigned char tag, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		DeferredSetEntityTag* data = nullptr;

		if (parameters.entities_are_stable) {
			data = (DeferredSetEntityTag*)AllocateTemporaryBuffer(sizeof(DeferredSetEntityTag));
			data->entities = entities;
		}
		else {
			data = (DeferredSetEntityTag*)AllocateTemporaryBuffer(sizeof(DeferredSetEntityTag) + sizeof(Entity) * entities.size);
			void* entity_buffer = function::OffsetPointer(data, sizeof(DeferredSetEntityTag));
			entities.CopyTo(entity_buffer);
			data->entities = { entity_buffer, entities.size };
		}

		data->tag = tag;
		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_SET_ENTITY_TAGS), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	// If the entity doesn't have the component, it will return nullptr
	void* EntityManager::TryGetComponent(Entity entity, Component component) {
		return (void*)((const EntityManager*)this)->TryGetComponent(entity, component);
	}

	// If the entity doesn't have the component, it will return nullptr
	const void* EntityManager::TryGetComponent(Entity entity, Component component) const {
		EntityInfo info = GetEntityInfo(entity);
		const ArchetypeBase* base = GetBase(info.main_archetype, info.base_archetype);
		unsigned char component_index = base->FindComponentIndex(component);
		if (component_index == UCHAR_MAX) {
			return nullptr;
		}
		return base->GetComponentByIndex(info.stream_index, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::TryGetSharedComponent(Entity entity, Component component) {
		return (void*)((const EntityManager*)this)->TryGetSharedComponent(entity, component);
	}

	const void* EntityManager::TryGetSharedComponent(Entity entity, Component component) const {
		EntityInfo info = GetEntityInfo(entity);
		const Archetype* archetype = GetArchetype(info.main_archetype);
		SharedInstance instance = archetype->GetBaseInstance(component, info.base_archetype);
		if (instance.value == -1) {
			return nullptr;
		}
		return GetSharedData(component, instance);
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::TryRemoveEntityFromHierarchyCommit(Entity entity, bool default_child_destroy)
	{
		return TryRemoveEntityFromHierarchyCommit({ &entity, 1 }, default_child_destroy);
	}

	bool EntityManager::TryRemoveEntityFromHierarchyCommit(Stream<Entity> entities, bool default_child_destroy)
	{
		DeferredTryRemoveEntitiesFromHierarchy commit_data;
		commit_data.default_destroy_children = default_child_destroy;
		commit_data.entities = entities;

		bool all_exist = true;
		CommitTryRemoveEntitiesFromHierarchy(this, &commit_data, &all_exist);
		return all_exist;
	}

	void EntityManager::TryRemoveEntityFromHierarchy(Stream<Entity> entities, bool default_child_destroy, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		DeferredTryRemoveEntitiesFromHierarchy* data = nullptr;
		if (parameters.entities_are_stable) {
			data = (DeferredTryRemoveEntitiesFromHierarchy*)AllocateTemporaryBuffer(sizeof(DeferredTryRemoveEntitiesFromHierarchy));
			data->entities = entities;
		}
		else {
			data = (DeferredTryRemoveEntitiesFromHierarchy*)AllocateTemporaryBuffer(sizeof(DeferredTryRemoveEntitiesFromHierarchy) + sizeof(Entity) * entities.size);
			data->entities.buffer = (Entity*)function::OffsetPointer(data, sizeof(DeferredTryRemoveEntitiesFromHierarchy));
			entities.CopyTo(data->entities.buffer);
			data->entities.size = entities.size;
		}

		data->default_destroy_children = default_child_destroy;
		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_TRY_REMOVE_ENTITIES_FROM_HIERARCHY), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::UnregisterComponentCommit(Component component)
	{
		DeferredDestroyComponent commit_data;
		commit_data.component = component;
		CommitDestroyComponent(this, &commit_data, nullptr);
	}

	void EntityManager::UnregisterComponent(Component component, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		size_t allocation_size = sizeof(DeferredDestroySharedComponent);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		DeferredDestroySharedComponent* data = (DeferredDestroySharedComponent*)allocation;
		data->component = component;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_DESTROY_COMPONENT), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::UnregisterSharedComponentCommit(Component component)
	{
		DeferredDestroySharedComponent commit_data;
		commit_data.component = component;
		CommitDestroySharedComponent(this, &commit_data, nullptr);
	}

	void EntityManager::UnregisterSharedComponent(Component component, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		size_t allocation_size = sizeof(DeferredDestroySharedComponent);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		DeferredDestroySharedComponent* data = (DeferredDestroySharedComponent*)allocation;
		data->component = component;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_DESTROY_SHARED_COMPONENT), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::UnregisterSharedInstanceCommit(Component component, SharedInstance instance)
	{
		DeferredDestroySharedInstance commit_data;
		commit_data.component = component;
		commit_data.instance = instance;

		CommitDestroySharedInstance(this, &commit_data, nullptr);
	}

	void EntityManager::UnregisterSharedInstance(Component component, SharedInstance instance, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		// If the component doesn't exist, fail
		ECS_CRASH_RETURN(ExistsSharedComponent(component), "EntityManager: Incorrect shared component {#} when trying to destroy shared instance {#}. ",
			component.value, instance.value);

		size_t allocation_size = sizeof(DeferredDestroySharedInstance);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		DeferredDestroySharedInstance* data = (DeferredDestroySharedInstance*)allocation;
		data->component = component;
		data->instance = instance;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_DESTROY_SHARED_INSTANCE), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::UnregisterNamedSharedInstanceCommit(Component component, Stream<char> name)
	{
		DeferredDestroyNamedSharedInstance commit_data;
		commit_data.component = component;
		commit_data.identifier = name;

		CommitDestroyNamedSharedInstance(this, &commit_data, nullptr);
	}

	void EntityManager::UnregisterNamedSharedInstance(Component component, Stream<char> name, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		// If the component doesn't exist, fail
		ECS_CRASH_RETURN(ExistsSharedComponent(component), "EntityManager: Incorrect shared component {#} when trying to register call to destroy shared instance {#}.",
			component.value, name);

		size_t allocation_size = sizeof(DeferredDestroyNamedSharedInstance) + name.size;

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t allocation_ptr = (uintptr_t)allocation;
		DeferredDestroyNamedSharedInstance* data = (DeferredDestroyNamedSharedInstance*)allocation;
		data->component = component;
		allocation_ptr += sizeof(*data);

		data->identifier.InitializeAndCopy(allocation_ptr, name);

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_DESTROY_NAMED_SHARED_INSTANCE), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::UnregisterUnreferencedSharedInstanceCommit(Component component, SharedInstance instance)
	{
		DeferredDestroyUnreferencedSharedInstance commit_data;
		commit_data.component = component;
		commit_data.instance = instance;

		DeferredDestroyUnreferencedSharedInstanceAdditional additional_data;
		CommitDestroyUnreferencedSharedInstance(this, &commit_data, &additional_data);
		return additional_data.return_value;
	}

	void EntityManager::UnregisterUnreferencedSharedInstance(Component component, SharedInstance instance, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		size_t allocation_size = sizeof(DeferredDestroyUnreferencedSharedInstance);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		DeferredDestroyUnreferencedSharedInstance* data = (DeferredDestroyUnreferencedSharedInstance*)allocation;
		data->component = component;
		data->instance = instance;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_DESTROY_UNREFERENCED_SHARED_SINGLE_INSTANCE), debug_info });
	}

	bool EntityManager::IsUnreferencedSharedInstance(Component component, SharedInstance instance) const {
		ECS_CRASH_RETURN_VALUE(ExistsSharedComponent(component), false, "EntityManager: Shared component {#} is invalid when trying to determine "
			"unreferenced shared instance {#}.", GetSharedComponentName(component), instance.value);

		unsigned int max_instance_count = m_shared_components[component].instances.stream.capacity;
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(bool, instance_bitmask, max_instance_count);
		UnreferencedSharedInstanceBitmask(component, instance_bitmask);

		return !instance_bitmask[instance.value];
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::UnregisterUnreferencedSharedInstancesCommit(Component component)
	{
		DeferredDestroyUnreferencedSharedInstances commit_data;
		commit_data.component = component;
		CommitDestroyUnreferencedSharedInstances(this, &commit_data, nullptr);
	}

	void EntityManager::UnregisterUnreferencedSharedInstances(Component component, EntityManagerCommandStream* command_stream, DebugInfo debug_info)
	{
		size_t allocation_size = sizeof(DeferredDestroyUnreferencedSharedInstances);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		DeferredDestroyUnreferencedSharedInstances* data = (DeferredDestroyUnreferencedSharedInstances*)allocation;
		data->component = component;

		DeferredActionParameters parameters = { command_stream };
		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_DESTROY_UNREFERENCED_SHARED_INSTANCES), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::UnreferencedSharedInstanceBitmask(Component component, CapacityStream<bool> bitmask) const {
		memset(bitmask.buffer, 0, bitmask.MemoryOf(bitmask.capacity));

		unsigned int archetype_count = GetArchetypeCount();
		VectorComponentSignature vector_component;
		vector_component.ConvertComponents({ &component, 1 });
		for (unsigned int index = 0; index < archetype_count; index++) {
			VectorComponentSignature archetype_shared = GetArchetypeSharedComponents(index);
			if (archetype_shared.HasComponents(vector_component)) {
				const Archetype* archetype = GetArchetype(index);
				unsigned char component_index = archetype->FindSharedComponentIndex(component);
				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					const SharedInstance* instances = archetype->GetBaseInstances(base_index);
					bitmask[instances[component_index].value] = true;
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetUnreferencedSharedInstances(Component component, CapacityStream<SharedInstance>* instances) const
	{
		ECS_CRASH_RETURN(ExistsSharedComponent(component), "EntityManager: Shared component {#} is invalid when trying to determine "
			"unreferenced shared instances.", GetSharedComponentName(component));

		ForEachUnreferencedSharedInstance(component, [&](SharedInstance instance, const void* data) {
			instances->AddAssert(instance);
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	VectorComponentSignature ECS_VECTORCALL GetEntityManagerUniqueVectorSignature(const VectorComponentSignature* signatures, unsigned int index)
	{
		return signatures[index << 1];
	}

	// --------------------------------------------------------------------------------------------------------------------

	VectorComponentSignature ECS_VECTORCALL GetEntityManagerSharedVectorSignature(const VectorComponentSignature* signatures, unsigned int index)
	{
		return signatures[(index << 1) + 1];
	}

	// --------------------------------------------------------------------------------------------------------------------

	VectorComponentSignature* GetEntityManagerUniqueVectorSignaturePtr(VectorComponentSignature* signatures, unsigned int index)
	{
		return signatures + (index << 1);
	}

	// --------------------------------------------------------------------------------------------------------------------

	VectorComponentSignature* GetEntityManagerSharedVectorSignaturePtr(VectorComponentSignature* signatures, unsigned int index)
	{
		return signatures + (index << 1) + 1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	EntityManager CreateEntityManagerWithPool(
		size_t allocator_size,
		size_t allocator_pool_count,
		size_t allocator_new_size,
		unsigned int entity_pool_power_of_two,
		GlobalMemoryManager* global_memory_manager
	)
	{
		size_t size_to_allocate = sizeof(MemoryManager) + sizeof(EntityPool);
		void* allocation = global_memory_manager->Allocate(size_to_allocate);

		// Share the allocator between the memory pool and the entity manager
		// The entity pool will make few allocations
		MemoryManager* entity_manager_allocator = (MemoryManager*)allocation;
		*entity_manager_allocator = MemoryManager(allocator_size, allocator_pool_count, allocator_new_size, global_memory_manager);

		allocation = function::OffsetPointer(allocation, sizeof(MemoryManager));
		EntityPool* entity_pool = (EntityPool*)allocation;
		*entity_pool = EntityPool(entity_manager_allocator, entity_pool_power_of_two);

		EntityManagerDescriptor descriptor;
		descriptor.entity_pool = entity_pool;
		descriptor.memory_manager = entity_manager_allocator;

		return EntityManager(descriptor);
	}

	// --------------------------------------------------------------------------------------------------------------------

}