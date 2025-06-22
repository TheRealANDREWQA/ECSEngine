#include "ecspch.h"
#include "EntityManager.h"
#include "../Utilities/Crash.h"
#include "ArchetypeQueryCache.h"
#include "Components.h"
#include "../Utilities/StreamUtilities.h"

#define ENTITY_MANAGER_DEFAULT_UNIQUE_COMPONENTS (1 << 7)
#define ENTITY_MANAGER_DEFAULT_SHARED_COMPONENTS (1 << 7)
#define ENTITY_MANAGER_DEFAULT_ARCHETYPE_COUNT (1 << 7)

#define ENTITY_MANAGER_TEMPORARY_ALLOCATOR_INITIAL_CAPACITY ECS_MB * 5
#define ENTITY_MANAGER_TEMPORARY_ALLOCATOR_BACKUP_CAPACITY ECS_MB * 50

#define ENTITY_MANAGER_SMALL_ALLOCATOR_SIZE ECS_KB * 256
#define ENTITY_MANAGER_SMALL_ALLOCATOR_CHUNKS ECS_KB * 2
#define ENTITY_MANAGER_SMALL_ALLOCATOR_BACKUP_SIZE ECS_KB * 128

// This is used for Copyables
#define ENTITY_MANAGER_COMPONENT_ALLOCATOR_SIZE ECS_KB * 256
#define ENTITY_MANAGER_COMPONENT_ALLOCATOR_CHUNKS ECS_KB * 4
#define ENTITY_MANAGER_COMPONENT_ALLOCATOR_BACKUP_SIZE ECS_KB * 512

// A default of 4 is a good start
#define COMPONENT_ALLOCATOR_ARENA_COUNT 4
#define COMPONENT_ALLOCATOR_BLOCK_COUNT (ECS_KB * 4)
#define COMPONENT_ALLOCATOR_MIN_ALLOCATOR_SIZE (ECS_KB * 100 * COMPONENT_ALLOCATOR_ARENA_COUNT)

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
		bool possibly_the_same_instance;
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

	struct DeferredDestroyComponent {
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
		DEFERRED_CALLBACK_COUNT
	};

#pragma region Helpers

	ECS_INLINE static Stream<char> EntityName(const EntityManager* entity_manager, Entity entity) {
		return GetEntityNameExtendedTempStorage(entity_manager, entity);
	}

	ECS_INLINE static Stream<char> EntityName(const EntityManager* entity_manager, EntityInfo info) {
		return EntityName(entity_manager, entity_manager->GetEntityFromInfo(info));
	}

	static void WriteCommandStream(EntityManager* manager, DeferredActionParameters parameters, DeferredAction action) {
		if (parameters.command_stream == nullptr) {
			unsigned int index = manager->m_deferred_actions.Add(action);
			ECS_CRASH_CONDITION(index < manager->m_deferred_actions.capacity, "EntityManager: Insufficient space for the entity manager command stream.");
		}
		else {
			ECS_CRASH_CONDITION(parameters.command_stream->size == parameters.command_stream->capacity, "EntityManager: Insufficient space for the user given command stream.");
			parameters.command_stream->Add(action);
		}
	}

	ECS_INLINE static void GetComponentSizes(unsigned int* sizes, ComponentSignature signature, const ComponentInfo* info) {
		for (size_t index = 0; index < signature.count; index++) {
			sizes[index] = info[signature.indices[index].value].size;
		}
	}

	static unsigned int GetComponentsSizePerEntity(const unsigned int* sizes, unsigned char count) {
		short total = 0;
		for (size_t index = 0; index < count; index++) {
			total += sizes[index];
		}

		return total;
	}

	static void GetCombinedSignature(Component* components, ComponentSignature base, ComponentSignature addition) {
		memcpy(components, base.indices, base.count * sizeof(Component));
		memcpy(components + base.count, addition.indices, addition.count * sizeof(Component));
	}

	static size_t GetDeferredCallDataAllocationSize(
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

	static Stream<Entity> GetEntitiesFromActionParameters(Stream<Entity> entities, DeferredActionParameters parameters, uintptr_t& buffer) {
		if (!parameters.entities_are_stable) {
			Stream<Entity> new_entities((void*)buffer, entities.size);
			new_entities.CopyOther(entities);
			buffer += sizeof(Entity) * entities.size;
			return new_entities;
		}
		else {
			return entities;
		}
	}

	static void CreateAllocatorForComponent(EntityManager* entity_manager, ComponentInfo& info, size_t allocator_size) {
		if (allocator_size > 0) {
			// Allocate the allocator
			CreateBaseAllocatorInfo create_info;
			create_info.allocator_type = ECS_ALLOCATOR_ARENA;
			create_info.arena_allocator_count = COMPONENT_ALLOCATOR_ARENA_COUNT;
			create_info.arena_multipool_block_count = COMPONENT_ALLOCATOR_BLOCK_COUNT;
			create_info.arena_nested_type = ECS_ALLOCATOR_MULTIPOOL;
			// Here, the user specifies the allocator size for the entire allocator. Using
			// Allocator_size directly would result in overshoot and unexpected crashes
			// Clamp the allocator size such that the creation won't fail because
			// The allocator size is too small
			create_info.arena_capacity = allocator_size / COMPONENT_ALLOCATOR_ARENA_COUNT;
			// Clamp the arena capacity to at least a value large enough, otherwise it might not have enough space
			// And it would all be the overhead of the block range
			create_info.arena_capacity = max(create_info.arena_capacity, COMPONENT_ALLOCATOR_BLOCK_COUNT * 96);
			size_t total_allocation_size = sizeof(MemoryArena) + MemoryArena::MemoryOf(COMPONENT_ALLOCATOR_ARENA_COUNT, create_info);
			void* allocation = entity_manager->m_memory_manager->Allocate(total_allocation_size);
			info.allocator = (MemoryArena*)allocation;
			new (info.allocator) MemoryArena(
				OffsetPointer(allocation, sizeof(MemoryArena)),
				COMPONENT_ALLOCATOR_ARENA_COUNT,
				create_info
			);
		}
		else {
			info.allocator = nullptr;
		}
	}

	static void DeallocateAllocatorForComponent(EntityManager* entity_manager, MemoryArena* allocator) {
		if (allocator != nullptr) {
			entity_manager->m_memory_manager->Deallocate(allocator);
		}
	}

	static const void** GetDeferredCallData(
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
					buffer = AlignPointer(buffer, 8);
					data[component_index] = (void*)buffer;
					buffer += component_sizes[component_index] * entity_count;
				}
				buffer = buffer_after_pointers;

				if (type == by_entities_type) {
					// Pack the data into a contiguous stream for each component type
					for (size_t component_index = 0; component_index < components.count; component_index++) {
						buffer = AlignPointer(buffer, 8);
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
								OffsetPointer(data[component_index], entity_index * component_sizes[component_index]),
								OffsetPointer(component_data[entity_index], size_until_component[component_index]),
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
						buffer = AlignPointer(buffer, 8);
						for (size_t entity_index = 0; entity_index < entity_count; entity_index++) {
							memcpy((void*)buffer, component_data[component_index * entity_count + entity_index], component_sizes[component_index]);
							buffer += component_sizes[component_index];
						}
					}
				}
				// COMPONENTS_SPLATTED
				else {
					for (size_t component_index = 0; component_index < components.count; component_index++) {
						buffer = AlignPointer(buffer, 8);
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

	static void UpdateEntityInfos(
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

	static void DeallocateGlobalComponent(EntityManager* entity_manager, unsigned int index) {
		// If it has an allocator deallocate it. We must use the main allocator, not the one from component info
		entity_manager->m_global_components_info[index].TryCallDeallocateFunction(entity_manager->m_global_components_data[index], entity_manager->MainAllocator());
		entity_manager->m_global_components_info[index].allocator = nullptr;

		CopyableDeallocate(entity_manager->m_global_components_info[index].data, entity_manager->ComponentAllocator());

		entity_manager->m_small_memory_manager.Deallocate(entity_manager->m_global_components_data[index]);
		// We also need to deallocate the name
		Stream<char> component_name = entity_manager->m_global_components_info[index].name;
		entity_manager->m_small_memory_manager.Deallocate(component_name.buffer);
	}

	static void ResizeGlobalComponents(EntityManager* entity_manager, unsigned int new_capacity, bool copy_old_data) {
		size_t allocation_size = new_capacity * (sizeof(Component) + sizeof(void*) + sizeof(ComponentInfo));
		void* new_allocation = entity_manager->m_small_memory_manager.Allocate(allocation_size);

		if (!copy_old_data) {
			for (unsigned int index = 0; index < entity_manager->m_global_component_count; index++) {
				DeallocateGlobalComponent(entity_manager, index);
			}
			entity_manager->m_global_component_count = 0;
		}

		void** old_data = entity_manager->m_global_components_data;
		Component* old_components = entity_manager->m_global_components;
		ComponentInfo* old_infos = entity_manager->m_global_components_info;

		entity_manager->m_global_components_data = (void**)new_allocation;
		entity_manager->m_global_components = (Component*)OffsetPointer(entity_manager->m_global_components_data, sizeof(void*) * new_capacity);
		entity_manager->m_global_components_info = (ComponentInfo*)OffsetPointer(entity_manager->m_global_components, sizeof(Component) * new_capacity);

		if (copy_old_data) {
			unsigned int copy_count = entity_manager->m_global_component_count < new_capacity ? entity_manager->m_global_component_count : new_capacity;

			memcpy(entity_manager->m_global_components_data, old_data, sizeof(void*) * copy_count);
			memcpy(entity_manager->m_global_components, old_components, sizeof(Component) * copy_count);
			memcpy(entity_manager->m_global_components_info, old_infos, sizeof(ComponentInfo) * copy_count);
		}
		
		if (entity_manager->m_global_component_count > 0) {
			entity_manager->m_small_memory_manager.Deallocate(old_data);
		}

		entity_manager->m_global_component_capacity = new_capacity;
	}

#pragma region Write Component

	static void CommitWriteComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredWriteComponent* data = (DeferredWriteComponent*)_data;
		ECS_STACK_CAPACITY_STREAM(unsigned int, component_sizes, ECS_ARCHETYPE_MAX_COMPONENTS);
		GetComponentSizes(component_sizes.buffer, data->write_signature, manager->m_unique_components.buffer);

		for (size_t index = 0; index < data->entities.size; index++) {
			for (unsigned char component_index = 0; component_index < data->write_signature.count; component_index++) {
				void* component = manager->GetComponent(data->entities[index], data->write_signature.indices[component_index]);
				memcpy(component, OffsetPointer(data->buffers[component_index], component_sizes[component_index] * index), component_sizes[component_index]);
			}
		}
	}

	static void RegisterWriteComponent(
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

	static void CommitWriteArchetype(EntityManager* manager, void* _data, void* _additional_data) {
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

	static void RegisterWriteArchetype(
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
	static void CommitEntityAddComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddComponentEntities* data = (DeferredAddComponentEntities*)_data;

		Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];

		ComponentSignature unique_signature = manager->GetEntitySignature(data->entities[0], unique_components);
		SharedComponentSignature shared_signature = manager->GetEntitySharedSignatureStable(data->entities[0]);

		// Make sure that the same component is not added twice
		for (unsigned char new_index = 0; new_index < data->components.count; new_index++) {
			ECS_STACK_CAPACITY_STREAM(char, signature_string, 512);
			ECS_CRASH_CONDITION_RETURN_VOID(
				unique_signature.Find(data->components[new_index]) == UCHAR_MAX,
				"EntityManager: Trying to add the same component {#} twice to entities. First entity is {#}, total entity count {#}. Archetype components: {#}",
				manager->GetComponentName(data->components[new_index]),
				EntityName(manager, data->entities[0]),
				data->entities.size,
				manager->GetComponentSignatureString(unique_signature, signature_string)
			);
		}

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
	static void CommitEntityAddComponentWithData(EntityManager* manager, void* _data, CopyFunction&& copy_function) {
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

	ECS_INLINE static void CommitEntityAddComponentSplatted(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopySplatComponents(copy_position, data, components);
		});
	}

	ECS_INLINE static void CommitEntityAddComponentByEntities(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntity(copy_position, data, components);
		});
	}

	ECS_INLINE static void CommitEntityAddComponentScattered(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponents(copy_position, data, components);
		});
	}

	ECS_INLINE static void CommitEntityAddComponentContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponentsContiguous(copy_position, data, components);
		});
	}

	ECS_INLINE static void CommitEntityAddComponentByEntitiesContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitEntityAddComponentWithData(manager, _data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntityContiguous(copy_position, data, components);
		});
	}

	static void RegisterEntityAddComponent(
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

	static void CommitEntityRemoveComponent(EntityManager* manager, void* _data, void* _additional_data) {
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
			ECS_CRASH_CONDITION_RETURN_VOID(
				subindex != unique_signature.count, 
				"EntityManager: Could not find component {#} when trying to remove it from entities. First entity is {#}, the total entity count is {#}.",
				manager->GetComponentName(data->components.indices[index]),
				EntityName(manager, data->entities[0]),
				data->entities.size
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
	static void CommitCreateEntities(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateEntities* data = (DeferredCreateEntities*)_data;
		CommitCreateEntitiesAdditionalData* additional_data = (CommitCreateEntitiesAdditionalData*)_additional_data;

		uint2 archetype_indices = manager->FindOrCreateArchetypeBase(data->unique_components, data->shared_components);
		ArchetypeBase* base_archetype = manager->GetBase(archetype_indices.x, archetype_indices.y);

		const size_t MAX_ENTITY_INFOS_ON_STACK = ECS_KB * 32;

		Entity* entities = nullptr;

		if constexpr (type != CREATE_ENTITIES_TEMPLATE_GET_ENTITIES) {
			// Fill in a buffer with the corresponding entity infos
			if (data->count < MAX_ENTITY_INFOS_ON_STACK) {
				entities = (Entity*)ECS_STACK_ALLOC(sizeof(Entity) * data->count);
			}
			else {
				entities = (Entity*)manager->m_memory_manager->Allocate(sizeof(Entity) * data->count);
			}
		}
		else {
			entities = additional_data->entities.buffer;
		}

		// If the chunk positions are not needed, it means we must update the entities
		unsigned int copy_position = base_archetype->Reserve(data->count);

		// Create the entities from the entity pool - their infos will already be set
		manager->m_entity_pool->Allocate({ entities, data->count }, archetype_indices, copy_position);
		
		// Copy the entities into the archetype entity reference
		base_archetype->SetEntities({ entities, data->count }, copy_position);
		base_archetype->m_size += data->count;

		if (!data->exclude_from_hierarchy) {
			// Add the entities as roots
			manager->AddEntitiesToParentCommit({ entities, data->count }, Entity::Invalid());
		}

		additional_data->copy_position = copy_position;
		additional_data->archetype_indices = archetype_indices;
		if constexpr (type != CREATE_ENTITIES_TEMPLATE_GET_ENTITIES) {
			if (data->count >= MAX_ENTITY_INFOS_ON_STACK) {
				manager->m_memory_manager->Deallocate(entities);
			}
		}
	}

	template<typename CopyFunction>
	static void CommitCreateEntitiesWithData(EntityManager* manager, void* _data, void* _additional_data, CopyFunction&& copy_function) {
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
			if (archetype->m_user_defined_components.count > 0) {
				for (unsigned char deallocate_index = 0; deallocate_index < archetype->m_user_defined_components.count; deallocate_index++) {
					for (unsigned int index = 0; index < data->count; index++) {
						void* target_component = base_archetype->GetComponentByIndex(additional_data.copy_position + index, archetype->m_user_defined_components[deallocate_index]);
						archetype->CallEntityCopy(additional_data.copy_position + index, additional_data.archetype_indices.y, deallocate_index, target_component, false);
					}
				}
			}
		}
	}

	ECS_INLINE static void CommitCreateEntitiesDataSplatted(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopySplatComponents(copy_position, data, components);
		});
	}

	ECS_INLINE static void CommitCreateEntitiesDataByEntities(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntity(copy_position, data, components);
		});
	}

	ECS_INLINE static void CommitCreateEntitiesDataScattered(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponents(copy_position, data, components);
		});
	}

	ECS_INLINE static void CommitCreateEntitiesDataContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByComponentsContiguous(copy_position, data, components);
		});
	}

	ECS_INLINE static void CommitCreateEntitiesDataByEntitiesContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		CommitCreateEntitiesWithData(manager, _data, _additional_data, [](ArchetypeBase* base_archetype, uint2 copy_position, const void** data, ComponentSignature components) {
			base_archetype->CopyByEntityContiguous(copy_position, data, components);
		});
	}

#pragma endregion

#pragma region Copy entities

	static void CommitCopyEntities(EntityManager* manager, void* _data, void* _additional_data) {
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
			struct ParentWithChildren {
				Entity* copied_parents;
				EntityHierarchy::ChildIterator child_iterator;
			};
			ResizableQueue<ParentWithChildren> process_queue(&manager->m_small_memory_manager, 128);
			process_queue.Push({ entities, manager->GetChildIterator(data->entity) });

			ParentWithChildren current_data;
			while (process_queue.Pop(current_data)) {
				current_data.child_iterator.ForEach([&](Entity child_entity) {
					manager->GetEntityCompleteSignatureStable(child_entity, &unique, &shared);
					manager->GetComponent(child_entity, unique, unique_data);

					// Allocate a buffer that can hold this allocation such we can reference the entities later on
					Entity* current_copy_parents = (Entity*)Allocate(manager->MainAllocator(), sizeof(Entity) * data->count);
					EntityHierarchy::ChildIterator current_children = manager->GetChildIterator(child_entity);
					size_t current_child_count = current_children.GetRemainingCount();
					if (current_child_count > 0) {
						ParentWithChildren new_data;
						new_data.child_iterator = current_children;
						new_data.copied_parents = current_copy_parents;
						process_queue.Push(new_data);
					}

					// Exclude these entities from the creat
					manager->CreateEntitiesCommit(data->count, unique, shared, unique, (const void**)unique_data, ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_SPLAT, true, true, current_copy_parents);
					manager->AddEntityToHierarchyCommit(current_data.copied_parents, current_copy_parents, data->count);

					if (current_child_count == 0) {
						// Deallocate the allocation now
						Deallocate(manager->MainAllocator(), current_copy_parents);
					}
				});

				// Deallocate the copied_parents buffer if it belongs to
				if (manager->MainAllocator().allocator->Belongs(current_data.copied_parents)) {
					Deallocate(manager->MainAllocator(), current_data.copied_parents);
				}
			}
			process_queue.FreeBuffer();
		}
	}

#pragma endregion

#pragma region Delete Entities

	static void CommitDeleteEntities(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDeleteEntities* data = (DeferredDeleteEntities*)_data;

		for (size_t index = 0; index < data->entities.size; index++) {
			EntityInfo info = manager->GetEntityInfo(data->entities[index]);
			ArchetypeBase* base_archetype = manager->GetBase(info.main_archetype, info.base_archetype);
			Archetype* main_archetype = manager->GetArchetype(info.main_archetype);
			
			// Deallocate any buffers this entity might have
			main_archetype->CallEntityDeallocate(info);

			// Remove the entities from the archetype
			base_archetype->RemoveEntity(info.stream_index, manager->m_entity_pool);
			
			// Remove the entity from the hierarchy
			manager->RemoveEntityFromHierarchyCommit({ data->entities.buffer + index, 1 });
		}
		manager->m_entity_pool->Deallocate(data->entities);
	}

#pragma endregion

#pragma region Add Shared Component

	static void CommitEntityAddSharedComponent(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddSharedComponentEntities* data = (DeferredAddSharedComponentEntities*)_data;

		EntityInfo info = manager->GetEntityInfo(data->entities[0]);
		Archetype* old_archetype = manager->GetArchetype(info.main_archetype);
		ArchetypeBase* old_base_archetype = old_archetype->GetBase(info.base_archetype);

		Component _shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		SharedInstance _shared_instances[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

		ComponentSignature unique_signature = old_archetype->GetUniqueSignature();
		ComponentSignature _shared_signature = old_archetype->GetSharedSignature();

		// Make sure that we do not try to add the same component twice
		for (unsigned char new_index = 0; new_index < data->components.count; new_index++) {
			ECS_STACK_CAPACITY_STREAM(char, signature_string, 512);
			ECS_CRASH_CONDITION_RETURN_VOID(
				_shared_signature.Find(data->components.indices[new_index]) == UCHAR_MAX,
				"EntityManager: Trying to add shared component {#} twice for entities. First entity is {#}, total entity count {#}. The archetype components: {#}",
				manager->GetSharedComponentName(data->components.indices[new_index]),
				EntityName(manager, data->entities[0]),
				data->entities.size,
				manager->GetSharedComponentSignatureString(_shared_signature, signature_string)
			);
		}

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

	static void CommitEntityRemoveSharedComponent(EntityManager* manager, void* _data, void* _additional_data) {
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
			ECS_CRASH_CONDITION(subindex < data->components.count || subindex == -1, "EntityManager: Component {#} could not be found when trying to remove components from entities. "
				"The first entity is {#}, total entity count {#}.", manager->GetComponentName(data->components.indices[index]), EntityName(manager, data->entities[0]), data->entities.size);
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

	static void CommitEntityChangeSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredChangeSharedInstanceEntities* data = (DeferredChangeSharedInstanceEntities*)_data;

		for (size_t index = 0; index < data->elements.size; index++) {
			SharedComponentSignature shared_signature = { &data->elements[index].component, &data->elements[index].new_instance, 1 };

			ECS_CRASH_CONDITION(
				manager->ExistsComponent(data->elements[index].component),
				"EntityManager: The shared component {#} does not exist when trying to change the shared instance for entity {#}.",
				manager->GetSharedComponentName(data->elements[index].component),
				EntityName(manager, data->elements[index].entity)
			);

			SharedInstance entity_instance = manager->GetSharedComponentInstance(data->elements[index].component, data->elements[index].entity);
			if (data->possibly_the_same_instance) {
				if (data->elements[index].new_instance == entity_instance) {
					// Skip this element
					continue;
				}
			}
			else {
				ECS_CRASH_CONDITION(
					manager->ExistsSharedInstanceOnly(data->elements[index].component, data->elements[index].new_instance),
					"EntityManager: The shared instance {#} does not exist for component {#} when trying to change the shared instance for entity {#}.",
					data->elements[index].new_instance,
					manager->GetSharedComponentName(data->elements[index].component),
					EntityName(manager,  data->elements[index].entity)
				);
				ECS_CRASH_CONDITION(
					data->elements[index].new_instance != entity_instance,
					"EntityManager: Trying to change the shared instance for component {#} for entity {#} to the same instance",
					manager->GetSharedComponentName(data->elements[index].component),
					EntityName(manager, data->elements[index].entity.value)
				);
			}

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

	static void CommitCreateArchetype(EntityManager* manager, void* _data, void* _additional_data) {
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
	static void CommitDestroyArchetype(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyArchetype* data = (DeferredDestroyArchetype*)_data;

		unsigned int archetype_index = data->archetype_index;
		if constexpr (use_component_search) {
			archetype_index = manager->FindArchetype({ data->unique_components, data->shared_components });
		}
		// Fail if it doesn't exist
		ECS_CRASH_CONDITION(archetype_index != -1, "EntityManager: Could not find archetype when trying to destroy it.");

		Archetype* archetype = manager->GetArchetype(archetype_index);
		// If it has components with buffers, those need to be deallocated manually
		if (archetype->m_user_defined_components.count > 0) {
			archetype->CallEntityDeallocate();
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

			// Update the entity archetype references
			for (size_t base_index = 0; base_index < archetype->GetBaseCount(); base_index++) {
				const ArchetypeBase* base = archetype->GetBase(base_index);
				for (size_t entity_index = 0; entity_index < base->m_size; entity_index++) {
					// Use the variant without checks, since the data here should be valid
					// (only in an insane circumstance where this memory gets corrupted that
					// would be the case, but that is not the usual occurence)
					EntityInfo* entity_info = manager->m_entity_pool->GetInfoPtrNoChecks(base->m_entities[entity_index]);
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

		// We also need to update the query cache
		manager->m_query_cache->UpdateRemove(archetype_index);
	}

#pragma endregion

#pragma region Create Archetype Base

	static void CommitCreateArchetypeBase(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateArchetypeBase* data = (DeferredCreateArchetypeBase*)_data;

		VectorComponentSignature vector_shared;
		vector_shared.InitializeSharedComponent(data->shared_components);
		unsigned int main_archetype_index = manager->FindArchetype({ data->unique_components, vector_shared });
		ECS_CRASH_CONDITION(main_archetype_index != -1, "EntityManager: Could not find main archetype when trying to create base archetype.");

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
	static void CommitDestroyArchetypeBase(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyArchetypeBase* data = (DeferredDestroyArchetypeBase*)_data;

		Archetype* archetype = nullptr;
		unsigned int base_index = 0;

		if constexpr (use_component_search) {
			VectorComponentSignature vector_shared;
			vector_shared.InitializeSharedComponent(data->shared_components);
			VectorComponentSignature vector_instances;
			vector_instances.InitializeSharedInstances(data->shared_components);
			archetype = manager->FindArchetypePtr({ data->unique_components, vector_shared });
			ECS_CRASH_CONDITION(archetype != nullptr, "EntityManager: Could not find main archetype when trying to delete base from components.");

			base_index = archetype->FindBaseIndex(vector_shared, vector_instances);
			ECS_CRASH_CONDITION(base_index != -1, "EntityManager: Could not find base archetype index when trying to delete it from components.");
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

#pragma region Create shared instance

	static void CommitCreateSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateSharedInstance* data = (DeferredCreateSharedInstance*)_data;

		// If no component is allocated at that slot, fail
		unsigned int component_size = manager->m_shared_components[data->component.value].info.size;
		ECS_CRASH_CONDITION(component_size != -1, "EntityManager: Trying to create a shared instance of shared component {#} failed. "
			"There is no such component.", manager->GetSharedComponentName(data->component));

		// Allocate the memory
		void* allocation = manager->m_small_memory_manager.Allocate(component_size);
		if (data->data != nullptr) {
			memcpy(allocation, data->data, component_size);
		}

		unsigned int instance_index = manager->m_shared_components[data->component.value].instances.Add(allocation);
		ECS_CRASH_CONDITION(instance_index < ECS_SHARED_INSTANCE_MAX_VALUE, "EntityManager: Too many shared instances created for component {#}.",
			manager->GetSharedComponentName(data->component));
		
		if (data->data != nullptr && data->copy_buffers) {
			// Call the copy function
			manager->m_shared_components[data->component.value].info.TryCallCopyFunction(allocation, data->data, false);
		}

		// Assert that the maximal amount of shared instance is not reached
		if (_additional_data != nullptr) {
			SharedInstance* instance = (SharedInstance*)_additional_data;
			instance->value = (short)instance_index;
		}
	}

#pragma endregion

#pragma region Create Named Shared Instance

	static void CommitCreateNamedSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredCreateNamedSharedInstance* data = (DeferredCreateNamedSharedInstance*)_data;

		DeferredCreateSharedInstance create_shared_instance = { data->component, data->copy_buffers, data->data };
		SharedInstance instance;
		CommitCreateSharedInstance(manager, &create_shared_instance, &instance);
		// Allocate memory for the name
		Stream<char> allocated_identifier;
		allocated_identifier.InitializeAndCopy(manager->SmallAllocator(), data->identifier);

		manager->m_shared_components[data->component.value].named_instances.InsertDynamic(manager->SmallAllocator(), instance, allocated_identifier);
	}

	static void CommitBindNamedSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredBindNamedSharedInstance* data = (DeferredBindNamedSharedInstance*)_data;

		ECS_CRASH_CONDITION(manager->ExistsSharedComponent(data->component),
			"Entity Manager: Trying to bind a named shared instance for component which doesn't exist", manager->GetSharedComponentName(data->component));

		ECS_CRASH_CONDITION(manager->ExistsSharedInstanceOnly(data->component, data->instance), 
			"EntityManager: Trying to bind named shared instance {#} for component {#} at slot {#} which doesn't exist.", data->identifier,
			manager->GetSharedComponentName(data->component), data->instance.value);
		
		Stream<char> allocated_identifier;
		allocated_identifier.InitializeAndCopy(manager->SmallAllocator(), data->identifier);

		// Allocate memory for the name
		manager->m_shared_components[data->component.value].named_instances.InsertDynamic(manager->SmallAllocator(), data->instance, allocated_identifier);
	}

#pragma endregion

#pragma region Destroy Shared Instance

	static void CommitDestroySharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroySharedInstance* data = (DeferredDestroySharedInstance*)_data;

		ECS_CRASH_CONDITION(manager->ExistsSharedComponent(data->component), "EntityManager: The component {#} doesn't exist when trying to destroy a shared instance.",
			manager->GetSharedComponentName(data->component));

		ECS_CRASH_CONDITION(manager->ExistsSharedInstanceOnly(data->component, data->instance), "EntityManager: The shared instance {#} for component {#} "
			"doesn't exist when trying to destroy it.", data->instance.value, manager->GetSharedComponentName(data->component));

		void* instance_data = manager->m_shared_components[data->component.value].instances[data->instance.value];

		// We also need to deallocate the buffers, if any
		manager->m_shared_components[data->component.value].info.TryCallDeallocateFunction(instance_data);
		Deallocate(manager->SmallAllocator(), instance_data);
		manager->m_shared_components[data->component.value].instances.Remove(data->instance.value);
	}

#pragma endregion

#pragma region Destroy Named Shared Instance

	static void CommitDestroyNamedSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyNamedSharedInstance* data = (DeferredDestroyNamedSharedInstance*)_data;

		Stream<char> identifier = data->identifier;
		ECS_CRASH_CONDITION(manager->ExistsSharedComponent(data->component), "EntityManager: Incorrect shared component {#} when trying to destroy named shared instance {#}.",
			manager->GetSharedComponentName(data->component), identifier);
		
		SharedInstance instance;
		ECS_CRASH_CONDITION(manager->m_shared_components[data->component.value].named_instances.TryGetValue(data->identifier, instance), "EntityManager: There is no shared "
			"instance {#} at shared component {#}.", identifier, manager->GetSharedComponentName(data->component));

		DeferredDestroySharedInstance commit_data;
		commit_data.component = data->component;
		commit_data.instance = instance;
		CommitDestroySharedInstance(manager, &commit_data, nullptr);
	}

#pragma endregion

#pragma region Destroy Unreferenced Shared Instance (single instance)

	static void CommitDestroyUnreferencedSharedInstance(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyUnreferencedSharedInstance* data = (DeferredDestroyUnreferencedSharedInstance*)_data;
		DeferredDestroyUnreferencedSharedInstanceAdditional* return_value = (DeferredDestroyUnreferencedSharedInstanceAdditional*)_additional_data;

		ECS_CRASH_CONDITION(manager->ExistsSharedComponent(data->component), "EntityManager: Incorrect shared component {#} when trying to destroy "
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

	static void CommitDestroyUnreferencedSharedInstances(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredDestroyUnreferencedSharedInstances* data = (DeferredDestroyUnreferencedSharedInstances*)_data;

		ECS_CRASH_CONDITION(manager->ExistsSharedComponent(data->component), "EntityManager: Incorrect shared component {#} when trying to destroy "
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
					size_t stream_index = SearchBytes(
						component_instances.buffer, 
						component_instances.size, 
						current_instances[component_in_archetype_index].value
					);
					if (stream_index != -1) {
						component_instances.RemoveSwapBack(stream_index);
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

	static void CommitClearEntityTags(EntityManager* manager, void* _data, void* _additional_data) {
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
			void* entity_buffer = OffsetPointer(data, sizeof(DeferredClearEntityTag));
			entities.CopyTo(entity_buffer);
			data->entities = { entity_buffer, entities.size };
		}

		data->tag = tag;
		WriteCommandStream(manager, parameters, { DataPointer(data, DEFERRED_CLEAR_ENTITY_TAGS), debug_info });
	}

#pragma endregion

#pragma region Set Tags

	static void CommitSetEntityTags(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredSetEntityTag* data = (DeferredSetEntityTag*)_data;

		for (size_t index = 0; index < data->entities.size; index++) {
			manager->m_entity_pool->SetTag(data->entities[index], data->tag);
		}
	}

#pragma endregion

#pragma region Add Entities To Hierarchy

	static void CommitAddEntitiesToHierarchyPairs(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddEntitiesToHierarchyPairs* data = (DeferredAddEntitiesToHierarchyPairs*)_data;
		
		for (size_t index = 0; index < data->pairs.size; index++) {
			manager->m_hierarchy.AddEntry(data->pairs[index].parent, data->pairs[index].child);
		}
	}

	static void CommitAddEntitiesToHierarchyContiguous(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddEntitiesToHierarchyContiguous* data = (DeferredAddEntitiesToHierarchyContiguous*)_data;

		for (unsigned int index = 0; index < data->count; index++) {
			manager->m_hierarchy.AddEntry(data->parents[index], data->children[index]);
		}
	}

#pragma endregion

#pragma region Add Entities To Parent Hierarchy

	static void CommitAddEntitiesToParentHierarchy(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredAddEntitiesToParentHierarchy* data = (DeferredAddEntitiesToParentHierarchy*)_data;

		for (size_t index = 0; index < data->entities.size; index++) {
			manager->m_hierarchy.AddEntry(data->parent, data->entities[index]);
		}
	}

#pragma endregion

#pragma region Remove Entities From Hierarchy

	static void CommitRemoveEntitiesFromHierarchy(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredRemoveEntitiesFromHierarchy* data = (DeferredRemoveEntitiesFromHierarchy*)_data;

		bool destroy_children = !data->default_destroy_children;

		if (!destroy_children) {
			for (size_t index = 0; index < data->entities.size; index++) {
				manager->m_hierarchy.RemoveEntry(data->entities[index]);
			}
		}
		else {
			// There is no need for batch deletion - the archetype base deletes
			for (size_t index = 0; index < data->entities.size; index++) {
				// Get the children first and delete them
				EntityHierarchy::NestedChildIterator nested_child_iterator = manager->m_hierarchy.GetNestedChildIterator(data->entities[index]);
				nested_child_iterator.ForEach([&](Entity entity) {
					// Using a batch delete isn't all that faster - we can just delete entity by entity
					DeferredDeleteEntities delete_data;
					delete_data.entities = { &entity, 1 };
					CommitDeleteEntities(manager, &delete_data, nullptr);
				});

				manager->m_hierarchy.RemoveEntry(data->entities[index]);
			}
		}
	}

#pragma endregion

#pragma region Try Remove Entities From Hierarchy

	static void CommitTryRemoveEntitiesFromHierarchy(EntityManager* manager, void* _data, void* _additional_data) {
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
			// There is no need for batch deletion - the archetype base deletes
			for (size_t index = 0; index < data->entities.size; index++) {
				bool exists = manager->m_hierarchy.Exists(data->entities[index]);
				all_exist &= exists;
				if (exists) {
					// Get the children first and delete them
					EntityHierarchy::NestedChildIterator nested_child_iterator = manager->m_hierarchy.GetNestedChildIterator(data->entities[index]);
					nested_child_iterator.ForEach([&](Entity entity) {
						// Using a batch delete isn't all that faster - we can just delete entity by entity
						DeferredDeleteEntities delete_data;
						delete_data.entities = { &entity, 1 };
						CommitDeleteEntities(manager, &delete_data, nullptr);
					});

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

	static void CommitChangeEntityParentHierarchy(EntityManager* manager, void* _data, void* _additional_data) {
		DeferredChangeEntityParentHierarchy* data = (DeferredChangeEntityParentHierarchy*)_data;

		for (size_t index = 0; index < data->pairs.size; index++) {
			manager->m_hierarchy.ChangeParent(data->pairs[index].parent, data->pairs[index].child);
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
		CommitChangeEntityParentHierarchy
	};

	static_assert(ECS_COUNTOF(DEFERRED_CALLBACKS) == DEFERRED_CALLBACK_COUNT);

#pragma endregion

	// --------------------------------------------------------------------------------------------------------------------

	EntityManager::EntityManager(const EntityManagerDescriptor& descriptor) : m_entity_pool(descriptor.entity_pool), m_memory_manager(descriptor.memory_manager),
		m_archetypes(descriptor.memory_manager, ENTITY_MANAGER_DEFAULT_ARCHETYPE_COUNT), m_auto_generate_component_functions_functor(nullptr)
	{
		// Create a small memory manager in order to not fragment the memory of the main allocator
		m_small_memory_manager = MemoryManager(
			ENTITY_MANAGER_SMALL_ALLOCATOR_SIZE, 
			ENTITY_MANAGER_SMALL_ALLOCATOR_CHUNKS,
			ENTITY_MANAGER_SMALL_ALLOCATOR_BACKUP_SIZE, 
			descriptor.memory_manager
		);

		// The component allocator is used only for Copyable allocations
		m_component_memory_manager = MemoryManager(
			ENTITY_MANAGER_COMPONENT_ALLOCATOR_SIZE,
			ENTITY_MANAGER_COMPONENT_ALLOCATOR_CHUNKS,
			ENTITY_MANAGER_COMPONENT_ALLOCATOR_BACKUP_SIZE,
			descriptor.memory_manager
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

		m_global_component_count = 0;
		m_global_component_capacity = 0;

		// Allocate the atomic streams - the deferred actions, clear tags and set tags
		m_deferred_actions.Initialize(m_memory_manager, descriptor.deferred_action_capacity);

		m_temporary_allocator = ResizableLinearAllocator(
			ENTITY_MANAGER_TEMPORARY_ALLOCATOR_INITIAL_CAPACITY, 
			ENTITY_MANAGER_TEMPORARY_ALLOCATOR_BACKUP_CAPACITY, 
			m_memory_manager
		);

		m_hierarchy_allocator = (MemoryManager*)m_memory_manager->Allocate(sizeof(MemoryManager));
		DefaultEntityHierarchyAllocator(m_hierarchy_allocator, m_memory_manager);
		m_hierarchy = EntityHierarchy(m_hierarchy_allocator);

		// Allocate the query cache now - use a separate allocation
		// Get a default allocator for it for the moment
		MemoryManager* query_cache_allocator = (MemoryManager*)Allocate(m_memory_manager, sizeof(MemoryManager));
		ArchetypeQueryCache::DefaultAllocator(query_cache_allocator, m_memory_manager);
		m_query_cache = (ArchetypeQueryCache*)m_memory_manager->Allocate(sizeof(ArchetypeQueryCache));
		*m_query_cache = ArchetypeQueryCache(this, query_cache_allocator);
	}

	// --------------------------------------------------------------------------------------------------------------------

	EntityManager& EntityManager::operator = (const EntityManager& other) {
		memcpy(this, &other, sizeof(other));
		m_query_cache->entity_manager = this;
		return *this;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::AllocateTemporaryBuffer(size_t size, size_t alignment)
	{
		return m_temporary_allocator.AllocateTs(size, alignment);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ApplySortingEntityFunctor(
		Stream<Entity> entities, 
		void (*functor)(EntityManager* entity_manager, Stream<Entity>entities, void* user_data), 
		void* user_data,
		bool same_array_shuffle
	)
	{
		if (!same_array_shuffle) {
			Stream<Entity> temporary_entities = { AllocateTemporaryBuffer(sizeof(Entity) * entities.size), entities.size };
			temporary_entities.CopyOther(entities);
			entities = temporary_entities;
		}
		for (size_t index = 0; index < entities.size; index++) {
			size_t iteration_start_index = index;
			size_t iteration_entity_count = 1;
			EntityInfo iteration_info = GetEntityInfo(entities[index]);
			index++;
			for (; index < entities.size; index++) {
				EntityInfo current_info = GetEntityInfo(entities[index]);
				if (iteration_info.main_archetype == current_info.main_archetype && iteration_info.base_archetype == current_info.base_archetype) {
					// We need to swap the entry
					entities.Swap(iteration_start_index + iteration_entity_count, index);
					iteration_entity_count++;
				}
			}

			functor(this, { entities.buffer + iteration_start_index, iteration_entity_count }, user_data);
			index = iteration_start_index + iteration_entity_count - 1;
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentCommit(Entity entity, Component component)
	{
		AddComponentCommit({ &entity, 1 }, component, true);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponent(Entity entity, Component component, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		AddComponent({ &entity, 1 }, component, true, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentCommit(Entity entity, Component component, const void* data)
	{
		AddComponentsCommit(entity, { &component, 1 }, (const void**)&data);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponent(Entity entity, Component component, const void* data, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		AddComponent({ &entity, 1 }, component, data, true, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentsCommit(Entity entity, ComponentSignature components)
	{
		AddComponentsCommit({ &entity, 1 }, components, true);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponents(Entity entity, ComponentSignature components, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		AddComponents({ &entity, 1 }, components, true, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponentsCommit(Entity entity, ComponentSignature components, const void** data)
	{
		AddComponentsCommit({ &entity, 1 }, components, data, ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY, true);
	}

	// --------------------------------------------------------------------------------------------------------------------

	// Forward towards the multiple entities with size 1
	void EntityManager::AddComponents(Entity entity, ComponentSignature components, const void** data, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		AddComponents({ &entity, 1 }, components, data, ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY, true, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponentCommit(Stream<Entity> entities, Component component, bool entities_belong_to_the_same_base_archetype)
	{
		AddComponentsCommit(entities, { &component, 1 }, entities_belong_to_the_same_base_archetype);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponent(
		Stream<Entity> entities, 
		Component component, 
		bool entities_belong_to_the_same_base_archetype, 
		DeferredActionParameters parameters, 
		DebugInfo debug_info
	)
	{
		auto functor = [&](EntityManager* entity_manager, Stream<Entity> entities) {
			RegisterEntityAddComponent(entity_manager, entities, { &component, 1 }, nullptr, DEFERRED_ENTITY_ADD_COMPONENT, parameters, debug_info);
		};
		if (!entities_belong_to_the_same_base_archetype) {
			ApplySortingEntityFunctor(entities, false, functor);
		}
		else {
			functor(this, entities);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponentCommit(Stream<Entity> entities, Component component, const void* data, bool entities_belong_to_the_same_base_archetype)
	{
		AddComponentsCommit(entities, { &component, 1 }, &data, ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_SPLAT, entities_belong_to_the_same_base_archetype);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponent(
		Stream<Entity> entities, 
		Component component, 
		const void* data, 
		bool entities_belong_to_the_same_base_archetype, 
		DeferredActionParameters parameters, 
		DebugInfo debug_info
	)
	{
		auto functor = [&](EntityManager* entity_manager, Stream<Entity> entities) {
			RegisterEntityAddComponent(this, entities, { &component, 1 }, &data, DEFERRED_ENTITY_ADD_COMPONENT_SPLATTED, parameters, debug_info);
		};
		if (!entities_belong_to_the_same_base_archetype) {
			ApplySortingEntityFunctor(entities, false, functor);
		}
		else {
			functor(this, entities);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponentsCommit(Stream<Entity> entities, ComponentSignature components, bool entities_belong_to_the_same_base_archetype)
	{
		auto functor = [&](EntityManager* entity_manager, Stream<Entity> entities) {
			DeferredAddComponentEntities commit_data;
			commit_data.components = components;
			commit_data.data = nullptr;
			commit_data.entities = entities;
			CommitEntityAddComponent<false>(this, &commit_data, nullptr);
		};
		if (!entities_belong_to_the_same_base_archetype) {
			ApplySortingEntityFunctor(entities, false, functor);
		}
		else {
			functor(this, entities);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponents(
		Stream<Entity> entities, 
		ComponentSignature components, 
		bool entities_belong_to_the_same_base_archetype, 
		DeferredActionParameters parameters, 
		DebugInfo debug_info
	)
	{
		auto functor = [&](EntityManager* entity_manager, Stream<Entity> entities) {
			RegisterEntityAddComponent(this, entities, components, nullptr, DEFERRED_ENTITY_ADD_COMPONENT, parameters, debug_info);
		};
		if (!entities_belong_to_the_same_base_archetype) {
			ApplySortingEntityFunctor(entities, false, functor);
		}
		else {
			functor(this, entities);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponentsCommit(
		Stream<Entity> entities, 
		ComponentSignature components, 
		const void** data, 
		EntityManagerCopyEntityDataType copy_type,
		bool entities_belong_to_the_same_base_archetype
	)
	{
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
			ECS_CRASH_CONDITION(false, "EntityManager: Copy entity data type is incorrect when trying to add component/s { {#} } to entities. First entity is {#}, total count is {#}.",
				GetComponentSignatureString(components, component_signature_string), EntityName(this, entities[0]), entities.size);
		}
		}

		auto functor = [&](EntityManager* entity_manager, Stream<Entity> entities) {
			DeferredAddComponentEntities commit_data;
			commit_data.components = components;
			commit_data.entities = entities;
			commit_data.data = data;
			callback(entity_manager, &commit_data, nullptr);
		};
		if (!entities_belong_to_the_same_base_archetype) {
			ApplySortingEntityFunctor(entities, false, functor);
		}
		else {
			functor(this, entities);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddComponents(
		Stream<Entity> entities, 
		ComponentSignature components, 
		const void** data,
		EntityManagerCopyEntityDataType copy_type,
		bool entities_belong_to_the_same_base_archetype,
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
			ECS_CRASH_CONDITION(false, "EntityManager: Incorrect copy type when trying to add components {#} to entities. First entity is {#}, total count is {#}.",
				GetComponentSignatureString(components, component_string_storage), EntityName(this, entities[0]), entities.size);
		}
		}

		auto functor = [&](EntityManager* entity_manager, Stream<Entity> entities) {
			RegisterEntityAddComponent(entity_manager, entities, components, data, action_type, parameters, debug_info);
		};
		if (!entities_belong_to_the_same_base_archetype) {
			ApplySortingEntityFunctor(entities, false, functor);
		}
		else {
			functor(this, entities);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponentCommit(Entity entity, Component shared_component, SharedInstance instance)
	{
		AddSharedComponentCommit({ &entity, 1 }, shared_component, instance, true);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponent(Entity entity, Component shared_component, SharedInstance instance, DeferredActionParameters parameters, DebugInfo debug_info)
	{
		AddSharedComponent({ &entity, 1 }, shared_component, instance, true, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponentCommit(Stream<Entity> entities, Component shared_component, SharedInstance instance, bool entities_belong_to_the_same_base_archetype)
	{
		AddSharedComponentsCommit(entities, { &shared_component, &instance, 1 }, entities_belong_to_the_same_base_archetype);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponent(
		Stream<Entity> entities, 
		Component shared_component, 
		SharedInstance instance, 
		bool entities_belong_to_the_same_base_archetype,
		DeferredActionParameters parameters, 
		DebugInfo debug_info
	)
	{
		AddSharedComponents(entities, { &shared_component, &instance, 1 }, entities_belong_to_the_same_base_archetype, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponentsCommit(Entity entity, SharedComponentSignature components) {
		AddSharedComponentsCommit({ &entity, 1 }, components, true);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponents(Entity entity, SharedComponentSignature components, DeferredActionParameters parameters, DebugInfo debug_info) {
		AddSharedComponents({ &entity, 1 }, components, true, parameters, debug_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponentsCommit(Stream<Entity> entities, SharedComponentSignature components, bool entities_belong_to_the_same_base_archetype) {
		auto functor = [&](EntityManager* entity_manager, Stream<Entity> entities) {
			DeferredAddSharedComponentEntities commit_data;
			commit_data.components = components;
			commit_data.components.count = 1;
			commit_data.entities = entities;
			CommitEntityAddSharedComponent(entity_manager, &commit_data, nullptr);
		};
		if (!entities_belong_to_the_same_base_archetype) {
			ApplySortingEntityFunctor(entities, false, functor);
		}
		else {
			functor(this, entities);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddSharedComponents(
		Stream<Entity> entities, 
		SharedComponentSignature components, 
		bool entities_belong_to_the_same_base_archetype,
		DeferredActionParameters parameters, 
		DebugInfo debug_info
	) {
		auto functor = [&](EntityManager* entity_manager, Stream<Entity> entities) {
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

			buffer = AlignPointer(buffer, alignof(Entity));
			data->entities = GetEntitiesFromActionParameters(entities, parameters, buffer);

			WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_ENTITY_ADD_SHARED_COMPONENT), debug_info });
		};
		if (!entities_belong_to_the_same_base_archetype) {
			ApplySortingEntityFunctor(entities, false, functor);
		}
		else {
			functor(this, entities);
		}
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
			data->pairs.buffer = (EntityPair*)OffsetPointer(data, sizeof(DeferredAddEntitiesToHierarchyPairs));
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
			data->parents = (Entity*)OffsetPointer(data, sizeof(DeferredAddEntitiesToHierarchyContiguous));
			memcpy((Entity*)data->parents, parents, sizeof(Entity) * count);
			data->children = (Entity*)OffsetPointer(data->parents, sizeof(Entity) * count);
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
			data->entities.buffer = (Entity*)OffsetPointer(data, sizeof(DeferredAddEntitiesToParentHierarchy));
			data->entities.CopyOther(entities);
		}

		data->parent = parent;
		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_ADD_ENTITIES_TO_PARENT_HIERARCHY), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::AddEntityManager(const EntityManager* other, CapacityStream<Entity>* created_entities)
	{
		if (created_entities != nullptr) {
			ECS_ASSERT(created_entities->size + other->GetEntityCount() <= created_entities->capacity);
		}

		// Register all the unique/shared/global components firstly
		other->ForEachComponent([&](Component component) {
			if (!ExistsComponent(component)) {
				const ComponentInfo* component_info = &other->m_unique_components[component];
				size_t allocator_size = component_info->allocator != nullptr ? component_info->allocator->InitialArenaCapacity() : 0;
				ComponentFunctions component_functions;
				component_functions.allocator_size = allocator_size;
				component_functions.copy_function = component_info->copy_function;
				component_functions.deallocate_function = component_info->deallocate_function;
				component_functions.data = component_info->data;
				RegisterComponentCommit(
					component,
					component_info->size,
					component_info->name,
					&component_functions
				);
			}
		});

		// For the shared instances, we need a remapping in order to have the correct
		// values be maintained
		struct RemappedInstance {
			ECS_INLINE bool operator == (RemappedInstance other) const {
				return component == other.component && other_instance == other.other_instance;
			}

			Component component;
			SharedInstance other_instance;
			SharedInstance current_instance;
		};
		ECS_STACK_CAPACITY_STREAM(RemappedInstance, remapped_instances, ECS_KB * 4);
		other->ForEachSharedComponent([&](Component component) {
			if (!ExistsSharedComponent(component)) {
				const ComponentInfo* component_info = &other->m_shared_components[component].info;
				size_t allocator_size = component_info->allocator != nullptr ? component_info->allocator->InitialArenaCapacity() : 0;
				ComponentFunctions component_functions;
				component_functions.allocator_size = allocator_size;
				component_functions.copy_function = component_info->copy_function;
				component_functions.deallocate_function = component_info->deallocate_function;
				component_functions.data = component_info->data;
				RegisterSharedComponentCommit(
					component,
					component_info->size,
					component_info->name,
					&component_functions,
					other->m_shared_components[component].compare_entry
				);
			}

			// Add all instances
			other->ForEachSharedInstance(component, [&](SharedInstance instance) {
				const void* data = other->GetSharedData(component, instance);
				SharedInstance current_instance = RegisterSharedInstanceCommit(component, data);
				remapped_instances.AddAssert({ component, instance, current_instance });
			});
		});

		other->ForAllGlobalComponents([&](void* data, Component component) {
			if (!ExistsGlobalComponent(component)) {
				const ComponentInfo* component_info = other->m_global_components_info + component;
				ComponentFunctions component_functions = component_info->GetComponentFunctions();

				void* global_component = RegisterGlobalComponentCommit(
					component,
					component_info->size,
					data,
					component_info->name,
					component_functions.copy_function != nullptr ? &component_functions : nullptr
				);

				// Call the copy function if it is specified
				component_info->TryCallCopyFunction(global_component, data, false);
			}
		});

		// Now create the necessary archetypes and copy the unique data
		// We also need to hold a remapping of the other entities to the newly created
		// Entities such that we can recreate the entity hierarchy later on. Use a hash table for
		// This instead of 2 parallel arrays, since we will need quite a bit of lookups to perform,
		// And the hash table is good enough for iteration. We need to this in a separate pass because
		// We need to create all entities from the other manager in order to assign properly the parents
		unsigned int total_entity_count = other->GetEntityCount();

		// The value is the created new entity, the key is the entity from the other manager.
		HashTable<Entity, Entity, HashFunctionPowerOfTwo> created_entity_remapping;
		created_entity_remapping.Initialize(m_memory_manager, HashTablePowerOfTwoCapacityForElements(total_entity_count));

		other->ForEachArchetype([&](const Archetype* other_archetype) {
			ComponentSignature unique_signature = other_archetype->GetUniqueSignature();
			ComponentSignature shared_signature = other_archetype->GetSharedSignature();
			unsigned int main_index = FindOrCreateArchetype(unique_signature, shared_signature);
			Archetype* current_archetype = GetArchetype(main_index);

			unsigned int base_count = other_archetype->GetBaseCount();
			ECS_STACK_CAPACITY_STREAM(SharedInstance, base_remapped_instances, ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);
			for (unsigned int index = 0; index < base_count; index++) {
				const ArchetypeBase* other_archetype_base = other_archetype->GetBase(index);

				base_remapped_instances.size = 0;
				SharedComponentSignature base_shared_signature = other_archetype->GetSharedSignature(index);
				for (unsigned char component_index = 0; component_index < base_shared_signature.count; component_index++) {
					RemappedInstance remapped_instance = { base_shared_signature.indices[component_index], base_shared_signature.instances[component_index] };
					unsigned int existing_index = remapped_instances.Find(remapped_instance);
					base_remapped_instances[component_index] = remapped_instances[existing_index].current_instance;
				}
				base_shared_signature.instances = base_remapped_instances.buffer;

				unsigned int copy_count = other_archetype_base->EntityCount();
				unsigned int base_index = current_archetype->FindBaseIndex(base_shared_signature);
				if (base_index == -1) {
					base_index = CreateArchetypeBaseCommit(main_index, base_shared_signature, copy_count);
				}
				ArchetypeBase* base_archetype = current_archetype->GetBase(base_index);

				// We need to copy all the entities from the other archetype base now
				unsigned int copy_position = base_archetype->Reserve(copy_count);
				Entity* allocated_entities = (Entity*)m_memory_manager->Allocate(sizeof(Entity) * copy_count);
				m_entity_pool->Allocate({ allocated_entities, copy_count }, { main_index, base_index }, copy_position);
				// Here we need to specify the entities of the other alongside the other entity pool
				base_archetype->CopyFromAnotherArchetype({ copy_position, copy_count }, other_archetype_base, other_archetype_base->m_entities, other->m_entity_pool);

				for (unsigned char copy_index = 0; copy_index < current_archetype->m_user_defined_components.count; copy_index++) {
					unsigned char other_component_index = other_archetype->FindCopyDeallocateComponentIndex(
						current_archetype->m_unique_components[current_archetype->m_user_defined_components[copy_index]]
					);
					ECS_ASSERT(other_component_index != UCHAR_MAX);
					for (unsigned int other_entity_index = 0; other_entity_index < other_archetype_base->EntityCount(); other_entity_index++) {
						// For this, do not deallocate the previous since it is copied from another entity manager
						current_archetype->CallEntityCopy(
							other_entity_index, 
							base_index, 
							copy_index,
							other_archetype_base->GetComponentByIndex(other_entity_index, other_component_index),
							false
						);
					}
				}

				// Insert the entities into the hash table
				for (unsigned int entity_index = 0; entity_index < copy_count; entity_index++) {
					created_entity_remapping.Insert(allocated_entities[entity_index], other_archetype_base->m_entities[entity_index]);
				}

				base_archetype->SetEntities({ allocated_entities, copy_count }, copy_position);
				base_archetype->m_size += copy_count;
				m_memory_manager->Deallocate(allocated_entities);
			}
		});

		// At last, configure the entity hierarchy
		created_entity_remapping.ForEachConst([&](Entity created_entity, Entity other_entity) {
			if (other->ExistsEntityInHierarchy(other_entity)) {
				Entity other_parent = other->GetEntityParent(other_entity);
				if (other_parent != Entity::Invalid()) {
					// Lookup the remapped parent created entity
					m_hierarchy.AddEntry(created_entity_remapping.GetValue(other_parent), created_entity);
				}
				else {
					// Insert the entry as a root
					m_hierarchy.AddEntry(Entity::Invalid(), created_entity);
				}
			}
		});

		if (created_entities != nullptr) {
			auto created_entity_remapping_iterator = created_entity_remapping.MutableValueIterator();
			// Add the entities that were created using the value iterator
			created_entities->AddIterator(&created_entity_remapping_iterator);
		}

		created_entity_remapping.Deallocate(m_memory_manager);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CallEntityComponentDeallocateCommit(Entity entity, Component component)
	{
		EntityInfo entity_info = GetEntityInfo(entity);
		Archetype* archetype = GetArchetype(entity_info.main_archetype);
		unsigned char deallocate_index = archetype->FindCopyDeallocateComponentIndex(component);
		ECS_CRASH_CONDITION(deallocate_index != UCHAR_MAX, "EntityManager: Trying to deallocate buffers for component {#} for entity {#} but the "
			"component is missing.", GetComponentName(component), EntityName(this, entity));
		archetype->CallEntityDeallocate(deallocate_index, entity_info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::TryCallEntityComponentDeallocateCommit(Entity entity, Component component)
	{
		EntityInfo entity_info = GetEntityInfo(entity);
		Archetype* archetype = GetArchetype(entity_info.main_archetype);
		unsigned char deallocate_index = archetype->FindCopyDeallocateComponentIndex(component);
		if (deallocate_index != UCHAR_MAX) {
			archetype->CallEntityDeallocate(deallocate_index, entity_info);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::CallComponentSharedInstanceDeallocateCommit(Component component, SharedInstance instance)
	{
		void* instance_data = GetSharedData(component, instance);
		const ComponentInfo* component_info = &m_shared_components[component.value].info;
		component_info->CallDeallocateFunction(instance_data);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::TryCallComponentSharedInstanceDeallocateCommit(Component component, SharedInstance instance) {
		void* instance_data = GetSharedData(component, instance);
		const ComponentInfo* component_info = &m_shared_components[component.value].info;
		component_info->TryCallDeallocateFunction(instance_data);
	}

	// --------------------------------------------------------------------------------------------------------------------

	static void SwapComponentInfoUserDefinedInfo(EntityManager* entity_manager, ComponentInfo* old_info, ComponentInfo* new_info) {
		if (old_info->allocator != nullptr) {
			// Copy the user related functions fields
			new_info->allocator = old_info->allocator;
			new_info->copy_function = old_info->copy_function;
			new_info->deallocate_function = old_info->deallocate_function;
			new_info->data = old_info->data;
			old_info->allocator = nullptr;
			old_info->copy_function = nullptr;
			old_info->deallocate_function = nullptr;
			old_info->data = nullptr;
		}
	}

	void EntityManager::ChangeComponentIndex(Component old_component, Component new_component)
	{
		ECS_ASSERT(ExistsComponent(old_component) && !ExistsComponent(new_component));

		unsigned short byte_size = m_unique_components[old_component.value].size;

		// Always use size 0 allocator size because we will transfer the arena to the new slot
		// In this way we avoid creating a new allocator and transfer all the existing data to it
		RegisterComponentCommit(
			new_component, 
			byte_size, 
			m_unique_components[old_component.value].name
		);

		SwapComponentInfoUserDefinedInfo(this, &m_unique_components[old_component.value], &m_unique_components[new_component.value]);

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

		unsigned short byte_size = m_shared_components[old_component.value].info.size;

		// Record the instances and the named instances in order to be transfered
		auto instances = m_shared_components[old_component.value].instances;
		auto named_instances = m_shared_components[old_component.value].named_instances;
		auto compare_entry = m_shared_components[old_component.value].compare_entry;
		m_shared_components[old_component.value].instances.stream.capacity = 0;
		m_shared_components[old_component.value].named_instances.m_capacity = 0;
		m_shared_components[old_component.value].compare_entry = {};

		// Always use size 0 allocator size because we will transfer the arena to the new slot
		// In this way we avoid creating a new allocator and transfer all the existing data to it
		RegisterSharedComponentCommit(
			new_component, 
			byte_size, 
			m_shared_components[old_component.value].info.name
		);

		m_shared_components[new_component.value].instances = instances;
		m_shared_components[new_component.value].named_instances = named_instances;
		m_shared_components[new_component.value].compare_entry = compare_entry;

		SwapComponentInfoUserDefinedInfo(this, &m_shared_components[old_component.value].info, &m_shared_components[new_component.value].info);

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

	void EntityManager::ChangeGlobalComponentIndex(Component old_component, Component new_component)
	{
		ECS_ASSERT(ExistsGlobalComponent(old_component) && !ExistsGlobalComponent(new_component));

		// This is a special case, since the component does not index directly into the array
		size_t index = SearchBytes(Stream<Component>{ m_global_components, m_global_component_count }, old_component);
		m_global_components[index] = new_component;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ChangeEntitySharedInstance(
		Stream<ChangeSharedComponentElement> elements,
		bool possibly_the_same_instance,
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
		data->possibly_the_same_instance = possibly_the_same_instance;

		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_ENTITY_CHANGE_SHARED_INSTANCE), debug_info });
	}

	void EntityManager::ChangeEntitySharedInstanceCommit(Stream<ChangeSharedComponentElement> elements, bool possible_the_same_instance, bool destroy_base_archetype)
	{
		DeferredChangeSharedInstanceEntities commit_data;
		commit_data.destroy_base_archetype = destroy_base_archetype;
		commit_data.elements = elements;
		commit_data.possibly_the_same_instance = possible_the_same_instance;
		CommitEntityChangeSharedInstance(this, &commit_data, nullptr);
	}

	SharedInstance EntityManager::ChangeEntitySharedInstanceCommit(
		Entity entity, 
		Component component, 
		SharedInstance new_instance, 
		bool possibly_the_same_instance, 
		bool destroy_base_archetype
	)
	{
		EntityInfo* info = m_entity_pool->GetInfoPtr(entity);
		Archetype* archetype = GetArchetype(info->main_archetype);
		unsigned char shared_index = archetype->FindSharedComponentIndex(component);
		ECS_CRASH_CONDITION_RETURN_DEDUCE(
			shared_index != UCHAR_MAX,
			"EntityManager: Entity {#} doesn't have shared component {#} when trying to change shared instance to {#}.", 
			EntityName(this, entity),
			GetSharedComponentName(component), 
			new_instance.value
		);

		SharedInstance shared_instances[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		SharedComponentSignature shared_signature = archetype->GetSharedSignature(info->base_archetype);
		// Check to see that the instance is indeed different
		if (!possibly_the_same_instance) {
			ECS_CRASH_CONDITION_RETURN_DEDUCE(
				shared_signature.instances[shared_index] != new_instance,
				"EntityManager: Trying to replace shared instance {#} with the same instance for entity {#}, component {#}.",
				new_instance.value,
				EntityName(this, entity),
				GetSharedComponentName(component)
			);
		}
		else {
			if (shared_signature.instances[shared_index] == new_instance) {
				return new_instance;
			}
		}

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
			data->pairs.buffer = (EntityPair*)OffsetPointer(data, sizeof(DeferredChangeEntityParentHierarchy));
			pairs.CopyTo(data->pairs.buffer);
			data->pairs.size = pairs.size;
		}

		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_CHANGE_ENTITY_PARENT_HIERARCHY), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::ChangeComponentFunctionsCommit(Component component, const ComponentFunctions* functions)
	{
		ECS_CRASH_CONDITION(ExistsComponent(component), "EntityManager: The unique component {#} is not registered when trying to change functions", component.value);

		CopyableDeallocate(m_unique_components[component.value].data, ComponentAllocator());

		MemoryArena* previous_allocator = m_unique_components[component.value].allocator;
		if (functions->copy_function != nullptr && functions->deallocate_function != nullptr && functions->allocator_size != 0) {
			if (functions->allocator_size != 0 && previous_allocator != nullptr) {
				if (functions->allocator_size != previous_allocator->InitialArenaCapacity()) {
					// We need to record the previous allocator, and move the data to the new location
					CreateAllocatorForComponent(this, m_unique_components[component.value], functions->allocator_size);
					// Here, we are setting the component functions to the new values.
					// The reason is, that in the Editor case, when a module is reloaded,
					// Its previous functions are not available anymore and using them
					// Causes crashes. So, we must use the new function when calling the copy
					m_unique_components[component.value].SetComponentFunctions(functions, ComponentAllocator());

					size_t component_byte_size = m_unique_components[component.value].size;
					// For each component, we must copy the data to the new location
					ForEachEntityComponent(component, [&](Entity entity, void* data) {
						// We must use temporary storage since the copy function should not
						// Deal with aliased pointers
						alignas(alignof(void*)) char temporary_storage[ECS_COMPONENT_MAX_BYTE_SIZE];
						// The copy function will call all fields
						m_unique_components[component.value].CallCopyFunction(temporary_storage, data, false);
						memcpy(data, temporary_storage, component_byte_size);
					});

					// We can now deallocate the previous allocator
					DeallocateAllocatorForComponent(this, previous_allocator);
				}
			}
			else if (functions->allocator_size != 0) {
				CreateAllocatorForComponent(this, m_unique_components[component.value], functions->allocator_size);
			}
			else if (previous_allocator != nullptr) {
				// We can straight up deallocate it
				DeallocateAllocatorForComponent(this, previous_allocator);
				m_unique_components[component.value].allocator = nullptr;
			}
			m_unique_components[component.value].SetComponentFunctions(functions, ComponentAllocator());
		}
		else {
			DeallocateAllocatorForComponent(this, previous_allocator);
			if (m_unique_components[component.value].deallocate_function != nullptr) {
				// This is the case where previously there was a deallocate function, and now it is no longer
				// What we need to do is to through all archetypes that registered this as deallocate component
				// And remove the entry
				ForEachArchetype([component](Archetype* archetype) {
					archetype->RemoveCopyDeallocateComponent(component);
				});
			}
			m_unique_components[component.value].ResetComponentFunctions();
		}
	}

	void EntityManager::ChangeSharedComponentFunctionsCommit(
		Component component, 
		const ComponentFunctions* functions, 
		SharedComponentCompareEntry compare_entry
	)
	{
		ECS_CRASH_CONDITION(ExistsSharedComponent(component), "EntityManager: The shared component {#} is not registered when trying to change functions", component.value);

		CopyableDeallocate(m_shared_components[component.value].info.data, ComponentAllocator());
		if (!m_shared_components[component.value].compare_entry.use_copy_deallocate_data) {
			CopyableDeallocate(m_shared_components[component.value].compare_entry.data, ComponentAllocator());
		}

		MemoryArena* previous_allocator = m_shared_components[component.value].info.allocator;
		if (functions->copy_function != nullptr && functions->deallocate_function != nullptr && functions->allocator_size != 0) {
			if (functions->allocator_size != 0 && previous_allocator != nullptr) {
				if (functions->allocator_size != previous_allocator->InitialArenaCapacity()) {
					// We need to record the previous allocator, and move the data to the new location				
					CreateAllocatorForComponent(this, m_shared_components[component.value].info, functions->allocator_size);
					// Here, we are setting the component functions to the new values.
					// The reason is, that in the Editor case, when a module is reloaded,
					// Its previous functions are not available anymore and using them
					// Causes crashes. So, we must use the new function when calling the copy
					m_shared_components[component.value].info.SetComponentFunctions(functions, ComponentAllocator());

					size_t component_byte_size = m_shared_components[component.value].info.size;
					// For each component, we must copy the data to the new location
					ForEachSharedInstance(component, [&](SharedInstance instance) {
						void* data = GetSharedData(component, instance);
						// We must use temporary storage since the copy function should not
						// Deal with aliased pointers
						alignas(alignof(void*)) char temporary_storage[ECS_COMPONENT_MAX_BYTE_SIZE];
						// The copy function will copy all fields
						m_shared_components[component.value].info.CallCopyFunction(temporary_storage, data, false);
						memcpy(data, temporary_storage, component_byte_size);
						});

					// We can now deallocate the previous allocator
					DeallocateAllocatorForComponent(this, previous_allocator);
				}
			}
			else if (functions->allocator_size != 0) {
				CreateAllocatorForComponent(this, m_shared_components[component.value].info, functions->allocator_size);
			}
			else if (previous_allocator != nullptr) {
				// We can straight up deallocate it
				DeallocateAllocatorForComponent(this, previous_allocator);
				m_shared_components[component.value].info.allocator = nullptr;
			}
			m_shared_components[component.value].info.SetComponentFunctions(functions, ComponentAllocator());
		}
		else {
			DeallocateAllocatorForComponent(this, previous_allocator);
			m_shared_components[component.value].info.allocator = nullptr;
			m_shared_components[component.value].info.ResetComponentFunctions();
		}
		m_shared_components[component.value].compare_entry = compare_entry;
		if (compare_entry.use_copy_deallocate_data) {
			m_shared_components[component.value].compare_entry.data = m_shared_components[component.value].info.data;
		}
		else {
			m_shared_components[component.value].compare_entry.data = CopyableCopy(compare_entry.data, ComponentAllocator());
		}
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

	void EntityManager::ClearAll(bool maintain_components)
	{
		// Only the main memory manager must be cleared, since all the other structures are allocated from it.
		
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 128);
		// Record the auto generate functor as well, and restore it
		EntityManagerAutoGenerateComponentFunctionsFunctor auto_generate_functor = m_auto_generate_component_functions_functor;
		Stream<void> auto_generate_functor_data = CopyNonZero(&stack_allocator, m_auto_generate_component_functions_data);
		
		struct RecreateComponent {
			Component value;
			unsigned int size;
			Stream<char> name;
			ComponentFunctions component_functions;
		};

		struct RecreateSharedComponent : RecreateComponent {
			SharedComponentCompareEntry compare_entry;
		};

		Stream<RecreateComponent> unique_components;
		Stream<RecreateSharedComponent> shared_components;
		// If the components are to be maintained, we need to record them now
		if (maintain_components) {
			size_t unique_component_count = 0;
			ForEachComponent([&](Component component) -> void {
				unique_component_count++;
			});

			size_t shared_component_count = 0;
			ForEachSharedComponent([&](Component component) -> void {
				shared_component_count++;
			});

			unique_components.Initialize(&stack_allocator, unique_component_count);
			shared_components.Initialize(&stack_allocator, shared_component_count);
			unique_components.size = 0;
			shared_components.size = 0;

			ForEachComponent([&](Component component) -> void {
				RecreateComponent recreate_component;
				recreate_component.value = component;
				recreate_component.size = ComponentSize(component);
				// The name must be copied
				recreate_component.name = GetComponentName(component).Copy(&stack_allocator);

				// If it has component functions, record them
				ComponentFunctions component_functions = m_unique_components[component].GetComponentFunctions();
				if (component_functions.copy_function != nullptr || component_functions.deallocate_function != nullptr) {
					// We must copy the copyable into the temporary allocator
					component_functions.data = CopyableCopy(component_functions.data, &stack_allocator);
					recreate_component.component_functions = component_functions;
				}

				unique_components.Add(&recreate_component);
			});
			ForEachSharedComponent([&](Component component) -> void {
				RecreateSharedComponent recreate_component;
				recreate_component.value = component;
				recreate_component.size = SharedComponentSize(component);
				// The name must be copied
				recreate_component.name = GetSharedComponentName(component).Copy(&stack_allocator);

				// If it has component functions, record them
				ComponentFunctions component_functions = m_shared_components[component].info.GetComponentFunctions();
				if (component_functions.copy_function != nullptr || component_functions.deallocate_function != nullptr) {
					// We must copy the copyable into the temporary allocator
					component_functions.data = CopyableCopy(component_functions.data, &stack_allocator);
					recreate_component.component_functions = component_functions;
				}

				SharedComponentCompareEntry compare_entry = m_shared_components[component].compare_entry;
				if (compare_entry.function != nullptr) {
					compare_entry.data = CopyableCopy(compare_entry.data, &stack_allocator);
				}

				shared_components.Add(&recreate_component);
			});
		}

		m_memory_manager->Clear();
		// Reset the entity pool as well
		m_entity_pool->Reset();

		EntityManagerDescriptor descriptor;
		descriptor.memory_manager = m_memory_manager;
		descriptor.entity_pool = m_entity_pool;
		descriptor.deferred_action_capacity = m_deferred_actions.capacity;
		*this = EntityManager(descriptor);

		// Restore the auto generate functor
		SetAutoGenerateComponentFunctionsFunctor(auto_generate_functor, auto_generate_functor_data.buffer, auto_generate_functor_data.size);

		// Restore the components. The auto generate function
		if (maintain_components) {
			for (size_t index = 0; index < unique_components.size; index++) {
				const ComponentFunctions* component_functions = nullptr;
				// If the copy function or the deallocate function is specified, consider this entry as valid
				if (unique_components[index].component_functions.copy_function != nullptr) {
					component_functions = &unique_components[index].component_functions;
				}
				RegisterComponentCommit(unique_components[index].value, unique_components[index].size, unique_components[index].name, component_functions);
			}

			for (size_t index = 0; index < shared_components.size; index++) {
				const ComponentFunctions* component_functions = nullptr;
				// If the copy function or the deallocate function is specified, consider this entry as valid
				if (shared_components[index].component_functions.copy_function != nullptr) {
					component_functions = &shared_components[index].component_functions;
				}
				
				SharedComponentCompareEntry compare_entry;
				if (shared_components[index].compare_entry.function != nullptr) {
					compare_entry = shared_components[index].compare_entry;
				}
				RegisterSharedComponentCommit(shared_components[index].value, shared_components[index].size, shared_components[index].name, component_functions, compare_entry);
			}
		}
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
		// TODO: Enforce that everything is cleared out? Otherwise some deallocations
		// Might be lost

		// Copy the entities first using the entity pool
		m_entity_pool->CopyEntities(entity_manager->m_entity_pool);

		// Copy the auto generate component functions, before registering any components
		m_auto_generate_component_functions_functor = entity_manager->m_auto_generate_component_functions_functor;
		m_auto_generate_component_functions_data = CopyNonZero(&m_small_memory_manager, entity_manager->m_auto_generate_component_functions_data);

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
				// We also need to copy the data, if there is such data
				m_unique_components[index].data = CopyableCopy(entity_manager->m_unique_components[index].data, ComponentAllocator());
				// Copy the name using the small allocator
				m_unique_components[index].name = entity_manager->m_unique_components[index].name.Copy(SmallAllocator());
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
				// Allocate the allocator
				if (entity_manager->m_shared_components[index].info.allocator != nullptr) {
					size_t capacity = entity_manager->m_shared_components[index].info.allocator->m_size_per_allocator * COMPONENT_ALLOCATOR_ARENA_COUNT;
					CreateAllocatorForComponent(this, m_shared_components[index].info, capacity);
				}

				// Allocate separetely the instances buffer
				// The name also needs to be allocated
				m_shared_components[index].info.name = entity_manager->m_shared_components[index].info.name.Copy(SmallAllocator());

				// We also need to copy the data, if there is such data
				m_shared_components[index].info.data = CopyableCopy(entity_manager->m_shared_components[index].info.data, ComponentAllocator()); 

				// The same for the compare data
				m_shared_components[index].compare_entry = entity_manager->m_shared_components[index].compare_entry;
				if (!m_shared_components[index].compare_entry.use_copy_deallocate_data) {
					m_shared_components[index].compare_entry.data = CopyableCopy(entity_manager->m_shared_components[index].compare_entry.data, ComponentAllocator());
				}
				else {
					m_shared_components[index].compare_entry.data = m_shared_components[index].info.data;
				}

				unsigned int other_capacity = entity_manager->m_shared_components[index].instances.stream.capacity;
				m_shared_components[index].instances.Initialize(SmallAllocator(), other_capacity);

				if (entity_manager->m_shared_components[index].instances.stream.size > 0) {
					m_shared_components[index].instances.stream.Copy(entity_manager->m_shared_components[index].instances.stream);
					
					// For every value allocate the data
					// And copy the buffers, if a copy function is provided
					bool has_copy_function = m_shared_components[index].info.copy_function != nullptr;
					m_shared_components[index].instances.stream.ForEachIndex([&](unsigned int subindex) {
						const void* other_data = entity_manager->m_shared_components[index].instances[subindex];
						void* new_data = Copy(SmallAllocator(), other_data, component_size);
						m_shared_components[index].instances[subindex] = new_data;
						if (has_copy_function) {
							m_shared_components[index].info.CallCopyFunction(new_data, other_data, false);
						}
					});
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
						Stream<void> identifier = Copy(
							&m_small_memory_manager,
							{ current_identifier.ptr, current_identifier.size }
						);
						*m_shared_components[index].named_instances.GetIdentifierPtrFromIndex(subindex) = identifier;
					});
				}
				else {
					m_shared_components[index].named_instances.Reset();
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

		// Copy the global components
		ResizeGlobalComponents(this, entity_manager->m_global_component_capacity, false);
		// We need to register the components one by one because we need to copy the buffers
		for (unsigned int index = 0; index < entity_manager->m_global_component_count; index++) {
			const ComponentInfo& component_info = entity_manager->m_global_components_info[index];
			ComponentFunctions component_functions = component_info.GetComponentFunctions();
			void* new_global_component = RegisterGlobalComponentCommit(
				entity_manager->m_global_components[index],
				component_info.size,
				entity_manager->m_global_components_data[index],
				component_info.name,
				component_functions.copy_function != nullptr ? &component_functions : nullptr
			);

			// If it has a copy function, call it. We must use the MainAllocator() as the allocator to use.
			component_info.TryCallCopyFunction(new_global_component, entity_manager->m_global_components_data[index], false, MainAllocator());
		}

		// Copy the actual archetypes now
		m_archetypes.FreeBuffer();
		m_archetypes.Initialize(&m_small_memory_manager, entity_manager->m_archetypes.capacity);

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

		// If the hierarchy allocator doesn't exist, initialize it now
		if (m_hierarchy_allocator == nullptr) {
			m_hierarchy_allocator = (MemoryManager*)m_memory_manager->Allocate(sizeof(MemoryManager));
			DefaultEntityHierarchyAllocator(m_hierarchy_allocator, m_memory_manager->m_backup);
		}
		else {
			// Just clear it
			m_hierarchy_allocator->Clear();
		}
		m_hierarchy = EntityHierarchy(
			m_hierarchy_allocator,
			entity_manager->m_hierarchy.roots.GetCapacity(),
			entity_manager->m_hierarchy.node_table.GetCapacity()
		);
		m_hierarchy.CopyOther(&entity_manager->m_hierarchy);

		// If the query cache allocator doesn't exist, initialize it now
		if (m_query_cache->allocator.allocator == nullptr) {
			MemoryManager* query_cache_allocator = (MemoryManager*)Allocate(m_memory_manager->m_backup, sizeof(MemoryManager));
			ArchetypeQueryCache::DefaultAllocator(query_cache_allocator, m_memory_manager->m_backup);
			m_query_cache->allocator = query_cache_allocator;
		}
		else {
			// Just clear the allocator
			m_query_cache->allocator.allocator->Clear();
		}
		m_query_cache->entity_manager = this;
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

	uint2 EntityManager::CreateSpecificEntitiesCommit(
		Stream<Entity> entities,
		ComponentSignature unique_components,
		SharedComponentSignature shared_components,
		bool exclude_from_hierarchy
	) {
		uint2 archetype_indices = FindOrCreateArchetypeBase(unique_components, shared_components);
		ArchetypeBase* base_archetype = GetBase(archetype_indices.x, archetype_indices.y);

		// If the chunk positions are not needed, it means we must update the entities
		unsigned int copy_position = base_archetype->Reserve(entities.size);
		m_entity_pool->AllocateSpecific(entities, archetype_indices, copy_position);

		// Copy the entities into the archetype entity reference
		base_archetype->SetEntities(entities, copy_position);
		base_archetype->m_size += entities.size;

		if (!exclude_from_hierarchy) {
			// Add the entities as roots
			AddEntitiesToParentCommit(entities, Entity::Invalid());
		}

		return archetype_indices;
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

			ECS_CRASH_CONDITION(false, "EntityManager: Incorrect copy type when trying to create entities from source data. "
				"Components: { {#} }. Shared Components{ {#} }. Number of entities into command {#}.",
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

			ECS_CRASH_CONDITION(false, "EntityManager: Incorrect copy type when trying to create entities from source data. "
				"Components: { {#} }. Shared Components: { {#} }. Entity count in command: {#}.",
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

	EntityManager EntityManager::CreateSubset(Stream<Entity> entities, MemoryManager* memory_manager) const
	{
		// Create an entity pool for the subset manager, and then insert each entity one by one
		EntityPool* entity_pool = (EntityPool*)memory_manager->Allocate(sizeof(EntityPool));
		*entity_pool = EntityPool(memory_manager, PowerOfTwoGreaterEx(entities.size).y);

		EntityManagerDescriptor descriptor;
		descriptor.memory_manager = memory_manager;
		descriptor.entity_pool = entity_pool;
		EntityManager subset_manager(descriptor);
		
		struct ReferencedSharedData {
			ECS_INLINE bool operator == (ReferencedSharedData other) const {
				return component == other.component && instance == other.instance;
			}

			Component component;
			SharedInstance instance;
			SharedInstance remapped_instance;
		};

		// Do a prepass and determine all the unique/shared components that are necessary
		
		// For the shared data, we must determine all the referenced shared instances
		// And write them only. We cannot use the ExistsSharedInstanceOnly since the
		// Value might be different from the one stored in the subset manager
		ECS_STACK_CAPACITY_STREAM(ReferencedSharedData, referenced_shared_data, ECS_KB * 2);
		for (size_t index = 0; index < entities.size; index++) {
			ComponentSignature unique_signature;
			SharedComponentSignature shared_signature;
			GetEntityCompleteSignatureStable(entities[index], &unique_signature, &shared_signature);

			for (unsigned char unique_index = 0; unique_index < unique_signature.count; unique_index++) {
				if (!subset_manager.ExistsComponent(unique_signature[unique_index])) {
					const ComponentInfo* component_info = &m_unique_components[unique_signature[unique_index].value];
					ComponentFunctions component_functions = component_info->GetComponentFunctions();
					subset_manager.RegisterComponentCommit(
						unique_signature[unique_index],
						component_info->size,
						component_info->name,
						&component_functions
					);
				}
			}

			for (unsigned char shared_index = 0; shared_index < shared_signature.count; shared_index++) {
				if (!subset_manager.ExistsSharedComponent(shared_signature.indices[shared_index])) {
					const ComponentInfo* component_info = &m_shared_components[shared_signature.indices[shared_index].value].info;
					ComponentFunctions component_functions = component_info->GetComponentFunctions();
					subset_manager.RegisterSharedComponentCommit(
						shared_signature.indices[shared_index],
						component_info->size,
						component_info->name,
						&component_functions,
						m_shared_components[shared_signature.indices[shared_index].value].compare_entry
					);
				}

				ReferencedSharedData reference_data = { shared_signature.indices[shared_index], shared_signature.instances[shared_index] };
				unsigned int existing_index = referenced_shared_data.Find(reference_data);
				if (existing_index == -1) {
					const void* shared_data = GetSharedData(reference_data.component, reference_data.instance);
					reference_data.remapped_instance = subset_manager.RegisterSharedInstanceCommit(reference_data.component, shared_data);
					referenced_shared_data.AddAssert(reference_data);
				}
			}
		}

		// We also need to have the correct entity hierarchy
		// For this we need an entity remapping
		Entity* entity_remapping = (Entity*)ECS_MALLOCA(sizeof(Entity) * entities.size);
		ECS_STACK_CAPACITY_STREAM(SharedInstance, remapped_instance, ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);
		for (size_t index = 0; index < entities.size; index++) {
			// For each entity, we must create it and copy the current data
			// For both the unique and shared
			ComponentSignature unique_signature;
			SharedComponentSignature shared_signature;
			GetEntityCompleteSignatureStable(entities[index], &unique_signature, &shared_signature);
			// We need to remap the current shared instance to the subset one
			for (unsigned char shared_instance = 0; shared_instance < shared_signature.count; shared_instance++) {
				unsigned int existing_index = referenced_shared_data.Find({ shared_signature.indices[shared_instance], shared_signature.instances[shared_instance] });
				ECS_ASSERT(existing_index != -1);
				remapped_instance[shared_instance] = referenced_shared_data[existing_index].remapped_instance;
			}
			shared_signature.instances = remapped_instance.buffer;
			
			Entity subset_entity = subset_manager.CreateEntityCommit(unique_signature, shared_signature);
			ECS_STACK_CAPACITY_STREAM(void*, unique_components, ECS_ARCHETYPE_MAX_COMPONENTS);
			GetComponent(entities[index], unique_signature, unique_components.buffer);
			subset_manager.SetEntityComponentsCommit(subset_entity, unique_signature, (const void**)unique_components.buffer, true, false);

			entity_remapping[index] = subset_entity;
		}

		for (size_t index = 0; index < entities.size; index++) {
			// We can use only the parent information to construct the hierarchy
			Entity current_parent = GetEntityParent(entities[index]);
			if (current_parent.IsValid()) {
				size_t remapping_index = SearchBytes(entities, current_parent);
				if (remapping_index != -1) {
					EntityPair pair;
					pair.parent = entity_remapping[remapping_index];
					pair.child = entity_remapping[index];
					subset_manager.ChangeEntityParentCommit({ &pair, 1 });
				}
			}
			else {
				EntityPair pair;
				pair.parent = Entity::Invalid();
				pair.child = entity_remapping[index];
				subset_manager.ChangeEntityParentCommit({ &pair, 1 });
			}
		}

		ECS_FREEA(entity_remapping);
		return subset_manager;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::ComponentSize(Component component) const
	{
		ECS_CRASH_CONDITION_RETURN(component.value < m_unique_components.size, -1, "EntityManager: Trying to retrieve invalid component byte size for {#}", component.value);
		return m_unique_components[component.value].size;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::SharedComponentSize(Component component) const
	{
		ECS_CRASH_CONDITION_RETURN(component.value < m_shared_components.size, -1, "EntityManager: Trying to retrieve invalid shared component byte size for {#}", component.value);
		return m_shared_components[component.value].info.size;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::GlobalComponentSize(Component component) const
	{
		size_t index = SearchBytes(m_global_components, m_global_component_count, component.value);
		ECS_CRASH_CONDITION_RETURN(index != -1, -1, "EntityManager: Trying to retrieve invalid global component byte size for {#}", component.value);
		return m_global_components_info[component.value].size;
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

	template<typename IntegerType>
	static void DestroyArchetypesCommitImpl(EntityManager* entity_manager, IteratorInterface<const IntegerType>* indices) {
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 16, ECS_MB * 8);
		RemoveArrayElements(indices, entity_manager->m_archetypes.size, &stack_allocator, [&](unsigned int index) {
			entity_manager->DestroyArchetypeCommit(index);
		});
	}

	void EntityManager::DestroyArchetypesCommit(IteratorInterface<const unsigned int>* indices)
	{
		DestroyArchetypesCommitImpl(this, indices);
	}

	void EntityManager::DestroyArchetypesCommit(IteratorInterface<const unsigned short>* indices)
	{
		DestroyArchetypesCommitImpl(this, indices);
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
		ECS_CRASH_CONDITION_RETURN(archetype_index < m_archetypes.size, UCHAR_MAX, "EntityManager: The archetype {#} is invalid when trying to find unique component {#}.",
			archetype_index, GetComponentName(component));
		return m_archetypes[archetype_index].FindUniqueComponentIndex(component);
	}

	void EntityManager::FindArchetypeUniqueComponent(unsigned int archetype_index, ComponentSignature components, unsigned char* indices) const
	{
		ECS_STACK_CAPACITY_STREAM(char, components_string, 1024);

		ECS_CRASH_CONDITION(archetype_index < m_archetypes.size, "EntityManager: The archetype {#} is invalid when trying to find unique components { {#} }.", 
			archetype_index, GetComponentSignatureString(components, components_string));

		for (size_t index = 0; index < components.count; index++) {
			indices[index] = m_archetypes[archetype_index].FindUniqueComponentIndex(components[index]);
		}
	}

	void ECS_VECTORCALL EntityManager::FindArchetypeUniqueComponentVector(unsigned int archetype_index, VectorComponentSignature components, unsigned char* indices) const
	{
		ECS_STACK_CAPACITY_STREAM(char, component_string_storage, 1024);

		ECS_CRASH_CONDITION(archetype_index < m_archetypes.size, "EntityManager: The archetype {#} is invalid when trying to find unique components { {#} }.", 
			archetype_index, GetComponentSignatureString(components, component_string_storage));
		// Use the fast SIMD compare
		GetArchetypeUniqueComponents(archetype_index).Find(components, indices);
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned char EntityManager::FindArchetypeSharedComponent(unsigned int archetype_index, Component component) const {
		ECS_CRASH_CONDITION_RETURN(archetype_index < m_archetypes.size, UCHAR_MAX, "EntityManager: The archetype {#} is invalid when trying to find shared component {#}.",
			archetype_index, GetSharedComponentName(component));
		return m_archetypes[archetype_index].FindSharedComponentIndex(component);
	}

	void EntityManager::FindArchetypeSharedComponent(unsigned int archetype_index, ComponentSignature components, unsigned char* indices) const {
		ECS_STACK_CAPACITY_STREAM(char, component_string_storage, 1024);

		ECS_CRASH_CONDITION(archetype_index < m_archetypes.size, "EntityManager: The archetype {#} is invalid when trying to find shared components { {#} }.",
			archetype_index, GetSharedComponentSignatureString(components, component_string_storage));
		for (size_t index = 0; index < components.count; index++) {
			indices[index] = m_archetypes[archetype_index].FindSharedComponentIndex(components[index]);
		}
	}

	void ECS_VECTORCALL EntityManager::FindArchetypeSharedComponentVector(unsigned int archetype_index, VectorComponentSignature components, unsigned char* indices) const {
		ECS_STACK_CAPACITY_STREAM(char, component_string_storage, 1024);
		
		ECS_CRASH_CONDITION(archetype_index < m_archetypes.size, "EntityManager: The archetype {#} is invalid when trying to find shared components { {#} }.", 
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

	Archetype* EntityManager::GetArchetype(unsigned int index) {
		ECS_CRASH_CONDITION_RETURN(index < m_archetypes.size, nullptr, "EntityManager: Invalid archetype index {#} when trying to retrive archetype pointer.", index);
		return &m_archetypes[index];
	}

	// --------------------------------------------------------------------------------------------------------------------

	const Archetype* EntityManager::GetArchetype(unsigned int index) const {
		ECS_CRASH_CONDITION_RETURN(index < m_archetypes.size, nullptr, "EntityManager: Invalid archetype index {#} when trying to retrive archetype pointer.", index);
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

	SharedInstance EntityManager::FindOrCreateSharedInstanceCommit(Component component, const void* data)
	{
		SharedInstance instance = GetSharedComponentInstance(component, data);
		if (!instance.IsValid()) {
			instance = RegisterSharedInstanceCommit(component, data);
		}
		return instance;
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
	void ECS_VECTORCALL GetArchetypeImplementation(const EntityManager* manager, const Query& query, CapacityStream<unsigned int>& archetypes) {
		for (size_t index = 0; index < manager->m_archetypes.size; index++) {
			if (query.VerifiesUnique(manager->m_archetypes[index].GetUniqueSignature())) {
				if (query.VerifiesShared(manager->m_archetypes[index].GetSharedSignature())) {
					ECS_CRASH_CONDITION(archetypes.size < archetypes.capacity, "EntityManager: Not enough space for capacity stream when getting archetype indices.");
					archetypes.Add(index);
				}
			}
		}
	}

	/*template<typename Query>
	void ECS_VECTORCALL GetArchetypePtrsImplementation(const EntityManager* manager, Query query, CapacityStream<Archetype*>& archetypes) {
		for (size_t index = 0; index < manager->m_archetypes.size; index++) {
			if (query.VerifiesUnique(manager->m_archetypes[index].GetUniqueSignature())) {
				if (query.VerifiesShared(manager->m_archetypes[index].GetSharedSignature())) {
					ECS_CRASH_CONDITION(archetypes.size < archetypes.capacity, "EntityManager: Not enough space for capacity stream when getting archetype pointers.");
					archetypes.Add(manager->m_archetypes.buffer + index);
				}
			}
		}
	}*/

	void EntityManager::GetArchetypes(const ArchetypeQuery& query, CapacityStream<unsigned int>& archetypes) const
	{
		GetArchetypeImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetArchetypes(const ArchetypeQueryExclude& query, CapacityStream<unsigned int>& archetypes) const
	{
		GetArchetypeImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetArchetypes(const ArchetypeQueryOptional& query, CapacityStream<unsigned int>& archetypes) const
	{
		GetArchetypeImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetArchetypes(const ArchetypeQueryExcludeOptional& query, CapacityStream<unsigned int>& archetypes) const
	{
		GetArchetypeImplementation(this, query, archetypes);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetArchetypes(const ArchetypeQueryDescriptor& query_descriptor, CapacityStream<unsigned int>& archetypes) const
	{
		ECS_ARCHETYPE_QUERY_DESCRIPTOR_TYPE query_type = query_descriptor.GetType();
		switch (query_type) {
		case ECS_ARCHETYPE_QUERY_SIMPLE:
		{
			GetArchetypes(query_descriptor.AsSimple(), archetypes);
		}
		break;
		case ECS_ARCHETYPE_QUERY_EXCLUDE:
		{
			GetArchetypes(query_descriptor.AsExclude(), archetypes);
		}
		break;
		case ECS_ARCHETYPE_QUERY_OPTIONAL:
		{
			GetArchetypes(query_descriptor.AsOptional(), archetypes);
		}
		break;
		case ECS_ARCHETYPE_QUERY_EXCLUDE_OPTIONAL:
		{
			GetArchetypes(query_descriptor.AsExcludeOptional(), archetypes);
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid query type");
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetComponent(Entity entity, Component component)
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetComponent(info, component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const void* EntityManager::GetComponent(Entity entity, Component component) const
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetComponent(info, component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetComponent(EntityInfo info, Component component)
	{
		return GetBase(info.main_archetype, info.base_archetype)->GetComponent(info, component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const void* EntityManager::GetComponent(EntityInfo info, Component component) const
	{
		return GetBase(info.main_archetype, info.base_archetype)->GetComponent(info, component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetComponentWithIndex(Entity entity, unsigned char component_index)
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetComponentWithIndex(info, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const void* EntityManager::GetComponentWithIndex(Entity entity, unsigned char component_index) const
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetComponentWithIndex(info, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetComponentWithIndex(EntityInfo info, unsigned char component_index)
	{
		return GetBase(info.main_archetype, info.base_archetype)->GetComponentByIndex(info, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const void* EntityManager::GetComponentWithIndex(EntityInfo info, unsigned char component_index) const
	{
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

	void* EntityManager::GetSharedComponent(EntityInfo info, Component component) {
		return (void*)((const EntityManager*)this)->GetSharedComponent(info, component);
	}

	const void* EntityManager::GetSharedComponent(EntityInfo info, Component component) const {
		SharedInstance instance = GetSharedComponentInstance(component, info);
		return GetSharedData(component, instance);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::GetGlobalComponent(Component component)
	{
		return (void*)((const EntityManager*)this)->GetGlobalComponent(component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	const void* EntityManager::GetGlobalComponent(Component component) const
	{
		size_t index = SearchBytes(m_global_components, m_global_component_count, component.value);
		ECS_CRASH_CONDITION_RETURN(index != -1, nullptr, "Global component {#} was not registered!", GetGlobalComponentName(component));
		return m_global_components_data[index];
	}

	// --------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic EntityManager::GetComponentAllocator(Component component) const
	{
		ECS_CRASH_CONDITION_RETURN(ExistsComponent(component), nullptr, "The component {#} doesn't exist when retrieving its allocator.", component);
		return m_unique_components[component.value].allocator;
	}

	// --------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic EntityManager::GetSharedComponentAllocator(Component component) const
	{
		ECS_CRASH_CONDITION_RETURN(ExistsSharedComponent(component), nullptr, "The shared component {#} doesn't exist when retrieving its allocator.", component);
		return m_shared_components[component.value].info.allocator;
	}

	// --------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic EntityManager::GetGlobalComponentAllocator(Component component) const {
		size_t component_index = SearchBytes(Stream<Component>(m_global_components, m_global_component_count), component);
		ECS_CRASH_CONDITION_RETURN(component_index != -1, nullptr, "The global component {#} doesn't exist when retrieving its allocator.", component);
		if (m_global_components_info[component_index].type_allocator_pointer_offset == -1) {
			// No allocator can be deduced
			return nullptr;
		}

		const void* allocator_pointer = OffsetPointer(m_global_components_data[component_index], m_global_components_info[component_index].type_allocator_pointer_offset);
		return ConvertPointerToAllocatorPolymorphicEx(allocator_pointer, m_global_components_info[component_index].type_allocator_type);
	}

	// --------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic EntityManager::GetComponentAllocatorFromType(Component component, ECS_COMPONENT_TYPE type) const {
		switch (type) {
		case ECS_COMPONENT_UNIQUE:
			return GetComponentAllocator(component);
		case ECS_COMPONENT_SHARED:
			return GetSharedComponentAllocator(component);
		case ECS_COMPONENT_GLOBAL:
			return GetGlobalComponentAllocator(component);
		}

		ECS_CRASH_CONDITION_RETURN(false, nullptr, "EntityManager: invalid component type when trying to retrieve component memory allocator.");

		// Shouldn't reach here
		return nullptr;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::GetEntityCountForComponent(Component component) const {
		ComponentSignature unique_signature = { &component, 1 };
		ComponentSignature shared_signature = {};
		unsigned int total_count = 0;
		ForEachArchetype({ unique_signature, shared_signature }, [&](const Archetype* archetype) {
			total_count += archetype->GetEntityCount();
		});
		return total_count;
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::GetEntityCountForSharedComponent(Component component) const {
		ComponentSignature unique_signature = {};
		ComponentSignature shared_signature = { &component, 1 };
		unsigned int total_count = 0;
		ForEachArchetype({ unique_signature, shared_signature }, [&](const Archetype* archetype) {
			total_count += archetype->GetEntityCount();
			});
		return total_count;
	}

	// --------------------------------------------------------------------------------------------------------------------

	Entity EntityManager::GetEntityFromIndex(unsigned int stream_index) const
	{
		Entity entity;
		entity.index = stream_index;
		EntityInfo info = m_entity_pool->GetInfoNoChecks(entity);
		return { stream_index, (unsigned int)info.generation_count };
	}

	// --------------------------------------------------------------------------------------------------------------------

	ComponentSignature EntityManager::GetEntitySignature(Entity entity, Component* components) const {
		EntityInfo info = GetEntityInfo(entity);
		return GetEntitySignature(info, components);
	}

	ComponentSignature EntityManager::GetEntitySignature(EntityInfo info, Component* components) const {
		ComponentSignature signature = GetEntitySignatureStable(info);
		memcpy(components, signature.indices, sizeof(Component) * signature.count);
		return { components, signature.count };
	}

	// --------------------------------------------------------------------------------------------------------------------

	ComponentSignature EntityManager::GetEntitySignatureStable(Entity entity) const {
		EntityInfo info = GetEntityInfo(entity);
		return GetEntitySignatureStable(info);
	}

	ComponentSignature EntityManager::GetEntitySignatureStable(EntityInfo info) const {
		return m_archetypes[info.main_archetype].GetUniqueSignature();
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedComponentSignature EntityManager::GetEntitySharedSignature(Entity entity, Component* shared, SharedInstance* instances) const {
		EntityInfo info = GetEntityInfo(entity);
		return GetEntitySharedSignature(info, shared, instances);
	}

	SharedComponentSignature EntityManager::GetEntitySharedSignature(EntityInfo info, Component* shared, SharedInstance* instances) const {
		SharedComponentSignature signature = GetEntitySharedSignatureStable(info);
		memcpy(shared, signature.indices, sizeof(Component) * signature.count);
		memcpy(instances, signature.instances, sizeof(SharedInstance) * signature.count);
		return { shared, instances, signature.count };
	}

	// --------------------------------------------------------------------------------------------------------------------
	
	SharedComponentSignature EntityManager::GetEntitySharedSignatureStable(Entity entity) const {
		EntityInfo info = GetEntityInfo(entity);
		return GetEntitySharedSignatureStable(info);
	}

	SharedComponentSignature EntityManager::GetEntitySharedSignatureStable(EntityInfo info) const {
		ComponentSignature shared_signature = m_archetypes[info.main_archetype].GetSharedSignature();
		const SharedInstance* instances = m_archetypes[info.main_archetype].GetBaseInstances(info.base_archetype);
		return { shared_signature.indices, (SharedInstance*)instances, shared_signature.count };
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetEntityCompleteSignature(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared) const {
		EntityInfo info = GetEntityInfo(entity);
		GetEntityCompleteSignature(info, unique, shared);
	}

	void EntityManager::GetEntityCompleteSignature(EntityInfo info, ComponentSignature* unique, SharedComponentSignature* shared) const {
		ComponentSignature stable_unique = GetEntitySignatureStable(info);
		SharedComponentSignature stable_shared = GetEntitySharedSignatureStable(info);

		memcpy(unique->indices, stable_unique.indices, sizeof(Component) * stable_unique.count);
		unique->count = stable_unique.count;

		memcpy(shared->indices, stable_shared.indices, sizeof(Component) * stable_shared.count);
		memcpy(shared->instances, stable_shared.instances, sizeof(SharedInstance) * stable_shared.count);
		shared->count = stable_shared.count;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetEntityCompleteSignatureStable(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared) const {
		EntityInfo info = GetEntityInfo(entity);
		GetEntityCompleteSignatureStable(info, unique, shared);
	}

	void EntityManager::GetEntityCompleteSignatureStable(EntityInfo info, ComponentSignature* unique, SharedComponentSignature* shared) const {
		*unique = GetEntitySignatureStable(info);
		*shared = GetEntitySharedSignatureStable(info);
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

	Stream<char> EntityManager::GetGlobalComponentName(Component component) const
	{
		size_t index = SearchBytes(m_global_components, m_global_component_count, component.value);
		return index == -1 ? "Invalid component" : m_global_components_info[index].name;
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
						ECS_CRASH_CONDITION(entities->size + base->m_size <= entities->capacity, "EntityManager: Not enough space for capacity stream when retrieving entities.");
						base->GetEntitiesCopy(entities->buffer + entities->size);
						entities->size += base->m_size;
					}
				}
			}
		}
	}

	template<typename Query>
	void ECS_VECTORCALL GetEntitiesImplementation(const EntityManager* manager, const Query& query, CapacityStream<Entity*>* entities) {
		for (size_t index = 0; index < manager->m_archetypes.size; index++) {
			if (query.VerifiesUnique(manager->m_archetypes[index].GetUniqueSignature())) {
				if (query.VerifiesShared(manager->m_archetypes[index].GetSharedSignature())) {
					// Matches the query
					for (size_t base_index = 0; base_index < manager->m_archetypes[index].GetBaseCount(); base_index++) {
						const ArchetypeBase* base = manager->GetBase(index, base_index);
						ECS_CRASH_CONDITION(entities->size < entities->capacity, "EntityManager: Not enough space for capacity stream when retrieving entities' pointers.");
						entities->Add(base->m_entities);
					}
				}
			}
		}
	}

	void EntityManager::GetEntities(const ArchetypeQuery& query, CapacityStream<Entity>* entities) const
	{
		GetEntitiesImplementation(this, query, entities);
	}

	void EntityManager::GetEntities(const ArchetypeQuery& query, CapacityStream<Entity*>* entities) const
	{
		GetEntitiesImplementation(this, query, entities);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetEntitiesExclude(const ArchetypeQueryExclude& query, CapacityStream<Entity>* entities) const
	{
		GetEntitiesImplementation(this, query, entities);
	}

	void EntityManager::GetEntitiesExclude(const ArchetypeQueryExclude& query, CapacityStream<Entity*>* entities) const
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
		ECS_CRASH_CONDITION_RETURN(ExistsSharedComponent(component), nullptr, "EntityManager: Shared component {#} is invalid when trying to retrieve instance {#} data.",
			GetSharedComponentName(component), instance.value);
		ECS_CRASH_CONDITION_RETURN(ExistsSharedInstanceOnly(component, instance), nullptr, "EntityManager: Shared instance "
			"{#} for component {#} is invalid.", instance.value, GetSharedComponentName(component));
		return m_shared_components[component.value].instances[instance.value];
	}

	// --------------------------------------------------------------------------------------------------------------------

	unsigned int EntityManager::GetNamedSharedInstanceCount(Component component) const {
		ECS_CRASH_CONDITION_RETURN(ExistsSharedComponent(component), -1, "EntityManager: Shared component {#} is invalid when trying to retrieve named shared instance count.", GetSharedComponentName(component));
		return m_shared_components[component.value].named_instances.GetCount();
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
		ECS_CRASH_CONDITION_RETURN(component.value < m_shared_components.size, { -1 }, "EntityManager: Shared component {#} is out of bounds when trying to"
			" get shared instance data.", component.value);
		unsigned int component_size = m_shared_components[component.value].info.size;
		
		// If the component size is -1, it means no actual component lives at that index
		ECS_CRASH_CONDITION_RETURN(component_size != -1, { -1 },  "EntityManager: There is no shared component allocated at {#}. Cannot retrieve shared instance data.",
			component.value);

		short instance_index = -1;
		// Check to see if we have a compare function. If we do, use that
		if (m_shared_components[component.value].compare_entry.function != nullptr) {
			SharedComponentCompareFunctionData compare_data;
			compare_data.function_data = m_shared_components[component.value].compare_entry.data;
			compare_data.first = data;
			m_shared_components[component.value].instances.stream.ForEachIndex<true>([&](unsigned int index) {
				const void* current_data = m_shared_components[component.value].instances[index];
				compare_data.second = current_data;
				if (m_shared_components[component.value].compare_entry.function(&compare_data)) {
					instance_index = (short)index;
					return true;
				}
				return false;
			});
		}
		else {
			m_shared_components[component.value].instances.stream.ForEachIndex<true>([&](unsigned int index) {
				const void* current_data = m_shared_components[component.value].instances[index];
				if (memcmp(current_data, data, component_size) == 0) {
					instance_index = (short)index;
					return true;
				}
				return false;
			});
		}

		return { instance_index };
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::GetSharedComponentInstance(Component component, Entity entity) const
	{
		EntityInfo info = GetEntityInfo(entity);
		return GetSharedComponentInstance(component, info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::GetSharedComponentInstance(Component component, EntityInfo info) const {
		const Archetype* archetype = GetArchetype(info.main_archetype);
		SharedInstance instance = archetype->GetBaseInstance(component, info.base_archetype);
		ECS_CRASH_CONDITION_RETURN(instance.IsValid(), SharedInstance::Invalid(), "EntityManager: The entity {#} doesn't have the shared component {#}.", EntityName(this, info), GetSharedComponentName(component));
		return instance;
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::GetNamedSharedComponentInstance(Component component, Stream<char> identifier) const {
		ECS_CRASH_CONDITION_RETURN(component.value < m_shared_components.size, { -1 }, "EntityManager: Shared component {#} is out of bounds when trying to retrieve named "
			"shared instance.", component.value);
		unsigned int component_size = m_shared_components[component.value].info.size;

		// If the component size is -1, it means no actual component lives at that index
		ECS_CRASH_CONDITION_RETURN(component_size != -1, { -1 }, "EntityManager: There is no shared component allocated at {#} when trying to retrieve named shared instance.", 
			component.value);
		SharedInstance instance;
		ECS_CRASH_CONDITION_RETURN(m_shared_components[component.value].named_instances.TryGetValue(identifier, instance), { -1 }, "EntityManager: There is no named shared"
			" instance {#} of shared component type {#}.", identifier, GetSharedComponentName(component));

		return instance;
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::GetOrCreateSharedComponentInstanceCommit(Component component, const void* data, bool* created_instance)
	{
		SharedInstance existing_instance = GetSharedComponentInstance(component, data);
		if (!existing_instance.IsValid()) {
			if (created_instance != nullptr) {
				*created_instance = true;
			}
			return RegisterSharedInstanceCommit(component, data);
		}
		if (created_instance != nullptr) {
			*created_instance = false;
		}
		return existing_instance;
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

	unsigned int EntityManager::GetSharedComponentInstanceCount(Component component) const
	{
		ECS_CRASH_CONDITION(ExistsSharedComponent(component), "EntityManager: Trying to retrieve instance count for shared component {#}, but it doesn't exist.", component.value);
		return m_shared_components[component.value].instances.stream.size;
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

	void EntityManager::GetEntitiesForSharedInstance(Component component, SharedInstance instance, AdditionStream<Entity> entities) const
	{
		ForEachEntityWithSharedInstance(component, instance, [&entities](Entity entity) {
			entities.Add(entity);
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetChildren(Entity parent, AdditionStream<Entity> static_storage) const
	{
		m_hierarchy.GetChildren(parent, static_storage);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GetChildrenNested(Entity parent, AdditionStream<Entity> children) const
	{
		m_hierarchy.GetAllChildren(parent, children);
	}

	// --------------------------------------------------------------------------------------------------------------------

	EntityHierarchy::ChildIterator EntityManager::GetChildIterator(Entity parent) const {
		return m_hierarchy.GetChildIterator(parent);
	}

	// --------------------------------------------------------------------------------------------------------------------

	EntityHierarchy::NestedChildIterator EntityManager::GetNestedChildIterator(Entity parent) const {
		return m_hierarchy.GetNestedChildIterator(parent);
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

	unsigned int EntityManager::GetNumberOfEntitiesForSharedInstance(Component component, SharedInstance instance) const
	{
		unsigned int total_count = 0;

		SharedComponentSignature signature;
		signature.indices = &component;
		signature.instances = &instance;
		signature.count = 1;

		ArchetypeQuery query;
		query.shared.InitializeSharedComponent(signature);
		// This does not prune by instance - only the component
		ForEachArchetype(query, [&](const Archetype* archetype) {
			unsigned int base_archetype_count = archetype->GetBaseCount();
			unsigned char component_index = archetype->FindSharedComponentIndex(component);
			ECS_ASSERT(component_index != UCHAR_MAX);
			for (unsigned int index = 0; index < base_archetype_count; index++) {
				SharedInstance current_instance = archetype->GetBaseInstanceUnsafe(component_index, index);
				if (current_instance == instance) {
					total_count += archetype->GetBase(index)->EntityCount();
				}
			}
		});

		return total_count;
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<bool allow_missing_component>
	void GroupEntitiesBySharedInstanceImplementation(EntityManager* entity_manager, Stream<Entity> entities, Component shared_component, AdditionStream<EntitySharedGroup> groups) {
		const size_t MAX_STACK_CAPACITY = ECS_KB * 64;

		// Do a prepass where the shared instances of the entities are determined, which will also validate
		// That the entities exist and that they have the given component
		Stream<SharedInstance> instances;
		instances.buffer = (SharedInstance*)ECS_MALLOCA_ALLOCATOR(sizeof(SharedInstance) * entities.size, MAX_STACK_CAPACITY, entity_manager->TemporaryAllocator());
		instances.size = entities.size;
		for (size_t index = 0; index < entities.size; index++) {
			if constexpr (allow_missing_component) {
				instances[index] = entity_manager->TryGetComponentSharedInstance(shared_component, entities[index]);
			}
			else {
				instances[index] = entity_manager->GetSharedComponentInstance(shared_component, entities[index]);
			}
		}

		size_t shuffle_index = 0;
		EntitySharedGroup groupping;
		for (size_t index = 0; index < entities.size; index++) {
			// This will be the instance for this new group
			SharedInstance group_instance = instances[index];
			groupping.instance = group_instance;
			groupping.offset = (unsigned int)shuffle_index;
			groupping.size = 1;

			shuffle_index++;
			index++;
			for (; index < entities.size; index++) {
				if (instances[index] == group_instance) {
					entities.Swap(shuffle_index, index);
					shuffle_index++;
					groupping.size++;
				}
			}
			groups.Add(groupping);
		}
	}

	void EntityManager::GroupEntitiesBySharedInstance(Stream<Entity> entities, Component shared_component, AdditionStream<EntitySharedGroup> groups) {
		GroupEntitiesBySharedInstanceImplementation<false>(this, entities, shared_component, groups);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::GroupEntitiesBySharedInstanceWithMissing(Stream<Entity> entities, Component shared_component, AdditionStream<EntitySharedGroup> groups) {
		GroupEntitiesBySharedInstanceImplementation<true>(this, entities, shared_component, groups);
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

	void EntityManager::PerformEntityComponentOperationsCommit(Entity entity, const PerformEntityComponentOperationsData& data) {
		ECS_CRASH_CONDITION_RETURN_VOID(data.added_unique_components_data.size == 0 || data.added_unique_components_data.size == data.added_unique_components.count, 
			"EntityManager: Invalid number of unique component data buffers specified for call `PerformEntityComponentOperationsCommit`");

		EntityInfo* entity_info = m_entity_pool->GetInfoPtr(entity);

		// Retrieve the current entity signature
		ComponentSignature unique_signature;
		SharedComponentSignature shared_signature;
		GetEntityCompleteSignatureStable(*entity_info, &unique_signature, &shared_signature);

		Component final_unique_signature_storage[ECS_ARCHETYPE_MAX_COMPONENTS];
		// These can be pointers to the data already existant in the owning archetype of the entity or the user specified
		// Data for new components. Parallel to the unique components
		const void* final_unique_components_data[ECS_ARCHETYPE_MAX_COMPONENTS];
		Component final_shared_signature_storage[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		SharedInstance final_shared_signature_instances_storage[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];

		// These are just some buffer of bytes that can be used to copy the data from when the
		// User hasn't specified the component data for the new unique components
		char component_stack_storage[ECS_COMPONENT_MAX_BYTE_SIZE];

		// Compose the final signature
		unique_signature.WriteTo(final_unique_signature_storage);
		unique_signature.indices = final_unique_signature_storage;
		
		// Retrieve the unique components of the entity.
		ArchetypeBase* original_base_archetype = GetBase(*entity_info);
		for (size_t index = 0; index < unique_signature.count; index++) {
			final_unique_components_data[index] = original_base_archetype->GetComponentByIndex(*entity_info, index);
		}

		shared_signature.WriteTo(final_shared_signature_storage, final_shared_signature_instances_storage);
		shared_signature.indices = final_shared_signature_storage;
		shared_signature.instances = final_shared_signature_instances_storage;
			
		// Remove the unique components
		for (size_t index = 0; index < data.removed_unique_components.count; index++) {
			unsigned char signature_index = unique_signature.Find(data.removed_unique_components[index]);
			ECS_CRASH_CONDITION_RETURN_VOID(signature_index != UCHAR_MAX, "EntityManager: Trying to remove unique component {#} from entity {#} when there is no attached component of this type.",
				GetComponentName(data.removed_unique_components[index]), EntityName(this, entity));
			// Remove the unique component pointer as well
			final_unique_components_data[signature_index] = final_unique_components_data[unique_signature.count - 1];
			unique_signature.RemoveSwapBack(signature_index);
		}
		ECS_CRASH_CONDITION_RETURN_VOID(unique_signature.count + data.added_unique_components.count <= ECS_ARCHETYPE_MAX_COMPONENTS, "EntityManager: Trying to add too many unique components to entity {#}.", EntityName(this, entity));

		// Remove the shared components
		for (size_t index = 0; index < data.removed_shared_components.count; index++) {
			unsigned char signature_index = shared_signature.Find(data.removed_shared_components[index]);
			ECS_CRASH_CONDITION_RETURN_VOID(signature_index != UCHAR_MAX, "EntityManager: Trying to remove shared component {#} from entity {#} when there is no attached component of this type.",
				GetSharedComponentName(data.removed_shared_components[index]), EntityName(this, entity));
			shared_signature.RemoveSwapBack(signature_index);
		}
		// We can't do an easy check for the total number of shared components since there might be updates.

		// Add the unique components
		for (size_t index = 0; index < data.added_unique_components.count; index++) {
			unsigned char signature_index = unique_signature.Find(data.added_unique_components[index]);
			ECS_CRASH_CONDITION_RETURN_VOID(signature_index == UCHAR_MAX, "EntityManager: Trying to add duplicate unique component {#} for entity {#}.",
				GetComponentName(data.added_unique_components[index]), EntityName(this, entity));
			// Add the component data from the array, if present
			if (data.added_unique_components_data.size > 0) {
				final_unique_components_data[unique_signature.count] = data.added_unique_components_data[index];
			}
			else {
				// Set the data to the stack storage, even tho it is a wasted copy, it allows
				// For a single call to the copy data function
				final_unique_components_data[unique_signature.count] = component_stack_storage;
			}
			unique_signature[unique_signature.count++] = data.added_unique_components[index];
		}

		// Add or update the shared components
		for (size_t index = 0; index < data.added_or_updated_shared_components.count; index++) {
			unsigned char signature_index = shared_signature.Find(data.added_or_updated_shared_components.indices[index]);
			if (signature_index == UCHAR_MAX) {
				// It is a new addition
				ECS_CRASH_CONDITION_RETURN_VOID(shared_signature.count < ECS_ARCHETYPE_MAX_SHARED_COMPONENTS, "EntityManager: Trying to add too many shared components to entity {#}.", EntityName(this, entity));
				shared_signature.indices[shared_signature.count] = data.added_or_updated_shared_components.indices[index];
				shared_signature.instances[shared_signature.count] = data.added_or_updated_shared_components.instances[index];
				shared_signature.count++;
			}
			else {
				// It already existed, update the instance
				shared_signature.instances[signature_index] = data.added_or_updated_shared_components.instances[index];
			}
		}

		// We can now commit the changes. Find the new destination archetype and copy the new data there, before
		// Removing the entity from the previous archetype.
		uint2 destination_archetype_indices = FindOrCreateArchetypeBase(unique_signature, shared_signature);
		ArchetypeBase* destination_base_archetype = GetBase(destination_archetype_indices.x, destination_archetype_indices.y);
		unsigned int entity_new_stream_index = destination_base_archetype->AddEntity(entity);
		destination_base_archetype->CopyByEntity({ entity_new_stream_index, 1 }, final_unique_components_data, unique_signature);

		original_base_archetype->RemoveEntity(entity_info->stream_index, m_entity_pool);
		
		// At last, update the entity info
		entity_info->main_archetype = destination_archetype_indices.x;
		entity_info->base_archetype = destination_archetype_indices.y;
		entity_info->stream_index = entity_new_stream_index;
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
			data->entities.buffer = (Entity*)OffsetPointer(data, sizeof(DeferredRemoveEntitiesFromHierarchy));
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

		buffer = AlignPointer(buffer, alignof(Entity));
		data->entities = GetEntitiesFromActionParameters(entities, parameters, buffer);

		WriteCommandStream(this, parameters, { DataPointer(allocation, DEFERRED_ENTITY_REMOVE_SHARED_COMPONENT), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterComponentCommit(
		Component component, 
		unsigned int size, 
		Stream<char> name,
		const ComponentFunctions* functions
	) {
		ECS_STACK_CAPACITY_STREAM(char, component_name_storage, 16);
		Stream<char> component_name = name;
		if (name.size == 0) {
			ConvertIntToChars(component_name_storage, component.value);
			component_name = component_name_storage;
		}

		ECS_CRASH_CONDITION(
			size <= ECS_COMPONENT_MAX_BYTE_SIZE,
			"EntityManager: Unique component {#} is too large.",
			component_name
		);

		if (m_unique_components.size <= component.value) {
			size_t old_capacity = m_unique_components.size;
			void* allocation = Allocate(SmallAllocator(), sizeof(ComponentInfo) * (component.value + 1));
			m_unique_components.CopyTo(allocation);
			Deallocate(SmallAllocator(), m_unique_components.buffer);

			m_unique_components.InitializeFromBuffer(allocation, component.value + 1);
			for (size_t index = old_capacity; index < m_unique_components.size; index++) {
				m_unique_components[index].size = -1;
			}

			// Update the component info reference to all archetypes and base archetypes
			for (unsigned int main_index = 0; main_index < m_archetypes.size; main_index++) {
				Archetype* archetype = GetArchetype(main_index);
				archetype->m_unique_infos = m_unique_components.buffer;
				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					ArchetypeBase* base = archetype->GetBase(base_index);
					base->m_infos = m_unique_components.buffer;
				}
			}
		}
		// If the size is different from -1, it means there is a component actually allocated to this slot
		ECS_CRASH_CONDITION(!ExistsComponent(component), "EntityManager: Creating component {#} at position {#} failed. It already exists.",
			component_name, component.value);
		m_unique_components[component.value].size = size;
		m_unique_components[component.value].name = StringCopy(SmallAllocator(), component_name);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 16, ECS_MB);
		ComponentFunctions auto_generated_functions;
		if (m_auto_generate_component_functions_functor != nullptr && functions == nullptr) {
			EntityManagerAutoGenerateComponentFunctionsFunctorData functor_data;
			functor_data.component = component;
			functor_data.component_name = component_name;
			functor_data.component_type = ECS_COMPONENT_UNIQUE;
			functor_data.temporary_allocator = &stack_allocator;
			functor_data.compare_entry = nullptr;
			functor_data.user_data = m_auto_generate_component_functions_data.buffer;
			auto_generated_functions = m_auto_generate_component_functions_functor(&functor_data);
			functions = &auto_generated_functions;
		}

		if (functions != nullptr) {
			bool is_something_set = functions->allocator_size != 0 || functions->copy_function != nullptr
				|| functions->deallocate_function != nullptr;
			bool is_something_not_set = functions->allocator_size == 0 || functions->copy_function == nullptr
				|| functions->deallocate_function == nullptr;
			ECS_CRASH_CONDITION((is_something_set && !is_something_not_set) || (!is_something_set && is_something_not_set),
				"EntityManager: Creating unique component {#} has invalid user defined functions. Make sure they are properly set", component_name);

			CreateAllocatorForComponent(this, m_unique_components[component.value], functions->allocator_size);
			m_unique_components[component.value].SetComponentFunctions(functions, ComponentAllocator());
		}
		else {
			m_unique_components[component.value].ResetComponentFunctions();
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterSharedComponentCommit(
		Component component, 
		unsigned int size, 
		Stream<char> name,
		const ComponentFunctions* component_functions,
		SharedComponentCompareEntry compare_entry
	) {
		ECS_STACK_CAPACITY_STREAM(char, component_name_storage, 64);
		Stream<char> component_name = name;
		if (name.size == 0) {
			ConvertIntToChars(component_name_storage, component.value);
			component_name = component_name_storage;
		}

		ECS_CRASH_CONDITION(
			size <= ECS_COMPONENT_MAX_BYTE_SIZE,
			"EntityManager: Shared component {#} is too large",
			component_name
		);

		if (m_shared_components.size <= component.value) {
			size_t old_capacity = m_shared_components.size;
			void* allocation = m_small_memory_manager.Allocate(sizeof(SharedComponentInfo) * (component.value + 1));
			m_shared_components.CopyTo(allocation);
			m_small_memory_manager.Deallocate(m_shared_components.buffer);

			m_shared_components.InitializeFromBuffer(allocation, component.value + 1);
			for (size_t index = old_capacity; index < m_shared_components.size; index++) {
				m_shared_components[index].info.size = -1;
			}
		}

		// If the size is different from -1, it means there is a component actually allocated to this slot
		ECS_CRASH_CONDITION(m_shared_components[component.value].info.size == -1,
			"EntityManager: Trying to create shared component {#} when it already exists {#} at that slot.", component_name, GetSharedComponentName(component));

		m_shared_components[component.value].info.size = size;
		m_shared_components[component.value].instances.Initialize(SmallAllocator(), 0);
		m_shared_components[component.value].named_instances.Initialize(SmallAllocator(), 0);
		m_shared_components[component.value].info.name = StringCopy(SmallAllocator(), component_name);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 16, ECS_MB);
		ComponentFunctions auto_generated_functions;
		SharedComponentCompareEntry auto_generated_compare_entry;
		if (m_auto_generate_component_functions_functor != nullptr && component_functions == nullptr) {
			EntityManagerAutoGenerateComponentFunctionsFunctorData functor_data;
			functor_data.component = component;
			functor_data.component_name = component_name;
			functor_data.component_type = ECS_COMPONENT_SHARED;
			functor_data.temporary_allocator = &stack_allocator;
			functor_data.compare_entry = &auto_generated_compare_entry;
			functor_data.user_data = m_auto_generate_component_functions_data.buffer;
			auto_generated_functions = m_auto_generate_component_functions_functor(&functor_data);
			component_functions = &auto_generated_functions;

			if (compare_entry.function == nullptr) {
				compare_entry = auto_generated_compare_entry;
			}
		}

		if (component_functions != nullptr) {
			bool is_something_set = component_functions->allocator_size != 0 || component_functions->copy_function != nullptr
				|| component_functions->deallocate_function != nullptr;
			bool is_something_not_set = component_functions->allocator_size == 0 || component_functions->copy_function == nullptr
				|| component_functions->deallocate_function == nullptr;
			// Crash if the allocator size is specified but the buffer offsets are not
			ECS_CRASH_CONDITION((is_something_set && !is_something_not_set) || (!is_something_set && is_something_not_set),
				"EntityManager: Trying to create shared component {#} with user defined functions but invalid. Check the allocator size or the assigned functions", component_name);
			CreateAllocatorForComponent(this, m_shared_components[component.value].info, component_functions->allocator_size);
			m_shared_components[component.value].info.SetComponentFunctions(component_functions, ComponentAllocator());
		}
		else {
			m_shared_components[component.value].info.ResetComponentFunctions();
		}

		if (compare_entry.function != nullptr) {
			m_shared_components[component.value].compare_entry = compare_entry;
			if (compare_entry.use_copy_deallocate_data) {
				m_shared_components[component.value].compare_entry.data = m_shared_components[component.value].info.data;
			}
			else {
				m_shared_components[component.value].compare_entry.data = CopyableCopy(compare_entry.data, ComponentAllocator());
			}
		}
		else {
			m_shared_components[component.value].compare_entry = {};
		}

		// The named instances table should start with size 0 and only create it when actually needing the tags
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::RegisterGlobalComponentCommit(
		Component component, 
		unsigned int size, 
		const void* data, 
		Stream<char> name, 
		const ComponentFunctions* component_functions
	) {
		ECS_STACK_CAPACITY_STREAM(char, component_name_storage, 64);
		Stream<char> component_name = name;
		if (name.size == 0) {
			ConvertIntToChars(component_name_storage, component.value);
			component_name = component_name_storage;
		}

		size_t existing_index = SearchBytes(m_global_components, m_global_component_count, component.value);
		ECS_CRASH_CONDITION_RETURN_DEDUCE(existing_index == -1, "EntityManager: Trying to create global component {#} when it already exists", component_name);

		ECS_CRASH_CONDITION_RETURN_DEDUCE(
			size <= ECS_COMPONENT_MAX_BYTE_SIZE,
			"EntityManager: Global component {#} is too large",
			component_name
		);

		// Allocate a new slot in the SoA stream
		// Firstly write the void* and then the Component
		if (m_global_component_count == m_global_component_capacity) {
			ResizeGlobalComponents(this, (unsigned int)((float)m_global_component_capacity * 1.5f + 3), true);
		}

		ComponentInfo* component_info = m_global_components_info + m_global_component_count;
		component_info->size = size;
		component_info->data = {};
		component_info->name = StringCopy(SmallAllocator(), component_name);
		component_info->allocator = nullptr;

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 16, ECS_MB);
		ComponentFunctions auto_generated_functions;
		if (m_auto_generate_component_functions_functor != nullptr && component_functions == nullptr) {
			EntityManagerAutoGenerateComponentFunctionsFunctorData functor_data;
			functor_data.component = component;
			functor_data.component_name = component_name;
			functor_data.component_type = ECS_COMPONENT_GLOBAL;
			functor_data.temporary_allocator = &stack_allocator;
			functor_data.compare_entry = nullptr;
			functor_data.user_data = m_auto_generate_component_functions_data.buffer;
			auto_generated_functions = m_auto_generate_component_functions_functor(&functor_data);
			component_functions = &auto_generated_functions;
		}

		if (component_functions != nullptr) {
			component_info->SetComponentFunctions(component_functions, ComponentAllocator());
		}
		else {
			component_info->ResetComponentFunctions();
		}

		void* component_allocation = m_small_memory_manager.Allocate(size);
		m_global_components_data[m_global_component_count] = component_allocation;
		if (data != nullptr) {
			memcpy(component_allocation, data, size);
		}
		m_global_components[m_global_component_count] = component;

		m_global_component_count++;
		return component_allocation;
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

	void EntityManager::RegisterSharedInstanceForValueCommit(Component component, SharedInstance instance, const void* data, bool copy_buffers) {
		// If no component is allocated at that slot, fail
		unsigned int component_size = m_shared_components[component.value].info.size;
		ECS_CRASH_CONDITION(component_size != -1, "EntityManager: Trying to create a forced shared instance of shared component {#} failed. "
			"There is no such component.", GetSharedComponentName(component));

		// Allocate the memory
		void* allocation = m_small_memory_manager.Allocate(component_size);
		if (data != nullptr) {
			memcpy(allocation, data, component_size);
		}

		ECS_CRASH_CONDITION(m_shared_components[component.value].instances.ExistsItem(instance.value), "EntityManager: Too many shared instances created for component {#}.",
			GetSharedComponentName(component));
		m_shared_components[component.value].instances.AllocateIndex(instance.value);

		if (data != nullptr && copy_buffers) {
			// Call the copy function
			m_shared_components[component.value].info.TryCallCopyFunction(allocation, data, false);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::RegisterSharedInstance(
		Component component, 
		const void* instance_data, 
		bool copy_buffers, 
		EntityManagerCommandStream* command_stream, 
		DebugInfo debug_info
	) {
		size_t allocation_size = sizeof(DeferredCreateSharedInstance) + (instance_data != nullptr ? m_shared_components[component.value].info.size : 0);

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		DeferredCreateSharedInstance* data = (DeferredCreateSharedInstance*)allocation;
		data->component = component;
		if (instance_data != nullptr) {
			data->data = OffsetPointer(allocation, sizeof(DeferredCreateSharedInstance));
			memcpy((void*)data->data, instance_data, m_shared_components[component.value].info.size);
			if (copy_buffers) {
				// Use the temporary allocator. Lock it now such that all allocations can be made
				// Single threaded, without fighting for the lock.
				if (m_shared_components[component.value].info.copy_function != nullptr) {
					m_temporary_allocator.Lock();
					__try {
						m_shared_components[component.value].info.CallCopyFunction((void*)data->data, instance_data, false, TemporaryAllocatorSingleThreaded());
					}
					__finally {
						m_temporary_allocator.Unlock();
					}
				}
			}
		}
		else {
			data->data = nullptr;
		}
		data->copy_buffers = copy_buffers;

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
		size_t allocation_size = sizeof(DeferredCreateNamedSharedInstance) + identifier.size;
		unsigned int component_size = SharedComponentSize(component);
		if (instance_data != nullptr) {
			allocation_size += component_size;
		}

		void* allocation = AllocateTemporaryBuffer(allocation_size);
		uintptr_t allocation_ptr = (uintptr_t)allocation;

		DeferredCreateNamedSharedInstance* data = (DeferredCreateNamedSharedInstance*)allocation;
		allocation_ptr += sizeof(*data);

		data->component = component;
		data->identifier.InitializeAndCopy(allocation_ptr, identifier);

		data->data = (void*)allocation_ptr;
		if (instance_data != nullptr) {
			memcpy((void*)data->data, instance_data, component_size);
			if (copy_buffers) {
				// Use the temporary allocator. Lock it now such that all allocations can be made
				// Single threaded, without fighting for the lock.
				if (m_shared_components[component.value].info.copy_function != nullptr) {
					m_temporary_allocator.Lock();
					__try {
						m_shared_components[component.value].info.CallCopyFunction((void*)data->data, instance_data, false, TemporaryAllocatorSingleThreaded());
					}
					__finally {
						m_temporary_allocator.Unlock();
					}
				}
			}
		}
		data->copy_buffers = copy_buffers;

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

	unsigned int EntityManager::RegisterQueryCommit(const ArchetypeQuery& query)
	{
		return m_query_cache->AddQuery(query);
	}

	unsigned int EntityManager::RegisterQueryCommit(const ArchetypeQueryExclude& query)
	{
		return m_query_cache->AddQuery(query);
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
		ECS_CRASH_CONDITION(ExistsSharedComponent(component), "EntityManager: There is no shared component {#} when trying to resize it.", component.value);

		m_shared_components[component.value].info.size = new_size;
		m_shared_components[component.value].instances.stream.ForEach([&](void*& data) {
			m_small_memory_manager.Deallocate(data);
			void* new_allocation = m_small_memory_manager.Allocate(new_size);
			data = new_allocation;
		});
	}

	void* EntityManager::ResizeGlobalComponent(Component component, unsigned int new_size) {
		ECS_CRASH_CONDITION(ExistsGlobalComponent(component), "EntityManager: There is no global component {#} when trying to resize it.", component.value);

		size_t global_component_index = SearchBytes(m_global_components, m_global_component_count, component.value);
		m_global_components_data[global_component_index] = m_small_memory_manager.Reallocate(m_global_components_data[global_component_index], new_size);
		return m_global_components_data[global_component_index];
	}

	// --------------------------------------------------------------------------------------------------------------------

	template<ECS_COMPONENT_TYPE component_type>
	static AllocatorPolymorphic ResizeComponentAllocatorImpl(EntityManager* entity_manager, Component component, size_t new_allocation_size) {
		bool exists;
		const char* crash_string = nullptr;
		if constexpr (component_type == ECS_COMPONENT_UNIQUE) {
			exists = entity_manager->ExistsComponent(component);
			crash_string = "EntityManager: There is no component {#} when trying to resize its allocator.";		
		}
		else if constexpr (component_type == ECS_COMPONENT_SHARED) {
			exists = entity_manager->ExistsSharedComponent(component);
			crash_string = "EntityManager: There is no shared component {#} when trying to resize its allocator.";
		}
		else {
			exists = entity_manager->ExistsGlobalComponent(component);
			crash_string = "EntityManager: There is no global component {#} when trying to resize its allocator.";
		}

		ECS_CRASH_CONDITION_RETURN(exists, nullptr, crash_string, component.value);

		AllocatorPolymorphic previous_allocator;
		ComponentInfo* info = nullptr;
		if constexpr (component_type == ECS_COMPONENT_UNIQUE) {
			previous_allocator = entity_manager->GetComponentAllocator(component);
			info = &entity_manager->m_unique_components[component.value];
		}
		else if constexpr (component_type == ECS_COMPONENT_SHARED) {
			previous_allocator = entity_manager->GetSharedComponentAllocator(component);
			info = &entity_manager->m_shared_components[component.value].info;
		}
		else {
			previous_allocator = entity_manager->GetGlobalComponentAllocator(component);
			info = entity_manager->m_global_components_info + component.value;
		}

		if (previous_allocator.allocator != nullptr) {
			if constexpr (component_type != ECS_COMPONENT_GLOBAL) {
				entity_manager->m_memory_manager->Deallocate(previous_allocator.allocator);
			}
		}

		CreateAllocatorForComponent(entity_manager, *info, new_allocation_size);
		return entity_manager->GetComponentAllocatorFromType(component, component_type);
	}

	AllocatorPolymorphic EntityManager::ResizeComponentAllocator(Component component, size_t new_allocation_size)
	{
		return ResizeComponentAllocatorImpl<ECS_COMPONENT_UNIQUE>(this, component, new_allocation_size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic EntityManager::ResizeSharedComponentAllocator(Component component, size_t new_allocation_size)
	{
		return ResizeComponentAllocatorImpl<ECS_COMPONENT_SHARED>(this, component, new_allocation_size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::Reset() {
		// Free the allocator of the query cache
		FreeAllocatorFrom(m_query_cache->allocator, m_memory_manager->m_backup);

		m_hierarchy_allocator->Free();
		m_memory_manager->Clear();
		m_entity_pool->Reset();

		// We need to maintain the auto generate function
		ECS_STACK_VOID_STREAM(auto_generate_data_storage, ECS_KB * 8);
		Stream<void> auto_generator_data = m_auto_generate_component_functions_data;
		EntityManagerAutoGenerateComponentFunctionsFunctor auto_generator_functor = m_auto_generate_component_functions_functor;
		if (m_auto_generate_component_functions_data.size > 0) {
			auto_generate_data_storage.CopyOther(auto_generator_data, 1);
			auto_generator_data = auto_generate_data_storage;
		}

		EntityManagerDescriptor descriptor;
		descriptor.memory_manager = m_memory_manager;
		descriptor.entity_pool = m_entity_pool;
		descriptor.deferred_action_capacity = m_deferred_actions.capacity;
		*this = EntityManager(descriptor);

		// The auto generator functor data is allocated from the entity manager, so we need to reallocate it after the assignment
		m_auto_generate_component_functions_functor = auto_generator_functor;
		m_auto_generate_component_functions_data = CopyNonZero(&m_small_memory_manager, auto_generator_data);

		// Change the allocator of the hierarchy allocator, is going to point to the temporary memory. The roots allocator needs to be changed too
		// The same goes for the query cache with the entity manager reference
		m_hierarchy.allocator = m_hierarchy_allocator;

		// The allocator for the query cache is stable because it is allocated from the memory manager
		m_query_cache->entity_manager = this;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::SetSharedComponentData(Component component, SharedInstance instance, const void* data)
	{
		ECS_CRASH_CONDITION(ExistsSharedComponent(component), "EntityManager: The component {#} doesn't exist when trying to set shared component data.", component.value);

		ECS_CRASH_CONDITION(ExistsSharedInstanceOnly(component, instance), "EntityManager: The instance {#} doesn't exist when trying to set "
			"shared component data for component {#}.", GetSharedComponentName(component), instance.value);

		unsigned int component_size = SharedComponentSize(component);
		memcpy(m_shared_components[component.value].instances[instance.value], data, component_size);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::SetNamedSharedComponentData(Component component, Stream<char> identifier, const void* data) {
		ECS_CRASH_CONDITION(component.value < m_shared_components.size, "EntityManager: Shared component {#} is out of bounds when trying to set named shared instance {#}.",
			component.value, identifier);

		ECS_CRASH_CONDITION(ExistsSharedComponent(component), "EntityManager: Shared component {#} does not exist when trying to set named shared instance {#}.", 
			component.value, identifier);

		SharedInstance instance;
		ECS_CRASH_CONDITION(
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
			void* entity_buffer = OffsetPointer(data, sizeof(DeferredSetEntityTag));
			entities.CopyTo(entity_buffer);
			data->entities = { entity_buffer, entities.size };
		}

		data->tag = tag;
		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_SET_ENTITY_TAGS), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::SetEntityComponentCommit(Entity entity, Component component, const void* data, bool allocate_buffers, bool deallocate_previous_buffers)
	{
		SetEntityComponentsCommit(entity, { &component, 1 }, &data, allocate_buffers, deallocate_previous_buffers);
	}

	void EntityManager::SetEntityComponentsCommit(
		Entity entity, 
		ComponentSignature component_signature, 
		const void** data, 
		bool allocate_buffers, 
		bool deallocate_previous_buffers
	)
	{
		for (unsigned char index = 0; index < component_signature.count; index++) {
			unsigned int component_size = ComponentSize(component_signature[index]);
			void* component = GetComponent(entity, component_signature[index]);

			memcpy(component, data[index], component_size);
			if (allocate_buffers) {
				m_unique_components[component_signature[index]].TryCallCopyFunction(component, data[index], deallocate_previous_buffers);
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::SetAutoGenerateComponentFunctionsFunctor(EntityManagerAutoGenerateComponentFunctionsFunctor functor, void* data, size_t data_size) {
		if (m_auto_generate_component_functions_data.size > 0) {
			m_small_memory_manager.Deallocate(m_auto_generate_component_functions_data.buffer);
		}
		m_auto_generate_component_functions_functor = functor;
		m_auto_generate_component_functions_data = CopyNonZero(&m_small_memory_manager, Stream<void>(data, data_size));
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::SwapArchetypeCommit(unsigned int previous_index, unsigned int new_index)
	{
		// Ensure that the indices are valid
		ECS_CRASH_CONDITION(previous_index < m_archetypes.size, "EntityManager: Trying to move an archetype with an invalid previous index");
		ECS_CRASH_CONDITION(new_index < m_archetypes.size, "EntityManager: Trying to move an archetype with an invalid new index");

		// Early exit if the indices are the same
		if (previous_index == new_index) {
			return;
		}

		// The entries can be swapped inside the archetypes array
		m_archetypes.Swap(previous_index, new_index);

		// We must swap the vector component signatures
		swap(*GetArchetypeUniqueComponentsPtr(previous_index), *GetArchetypeUniqueComponentsPtr(new_index));
		swap(*GetArchetypeSharedComponentsPtr(previous_index), *GetArchetypeSharedComponentsPtr(new_index));

		// Then the archetype query cache must be updated.
		m_query_cache->SwapArchetype(previous_index, new_index);


		// At last, the entity infos must be updated for the entities that belongs to these archetypes.

		auto patch_entity_infos = [this](unsigned int archetype_index) -> void {
			for (unsigned int base_index = 0; base_index < m_archetypes[archetype_index].m_base_archetypes.size; base_index++) {
				const ArchetypeBase& base_archetype = m_archetypes[archetype_index].m_base_archetypes[base_index].archetype;
				unsigned int entity_count = base_archetype.EntityCount();
				for (unsigned int stream_index = 0; stream_index < entity_count; stream_index++) {
					// Use no checks call since the entities stored here should be valid, except for a weird corruption
					// Case where its not worth considering
					EntityInfo* entity_info = m_entity_pool->GetInfoPtrNoChecks(base_archetype.m_entities[stream_index]);
					entity_info->main_archetype = archetype_index;
				}
			}
		};

		// The archetypes have already been swapped inside m_archetypes.
		patch_entity_infos(previous_index);
		patch_entity_infos(new_index);
	}

	void EntityManager::SwapArchetypeBaseCommit(unsigned int archetype_index, unsigned int previous_base_index, unsigned int new_base_index) {
		// Validate the parameters
		ECS_CRASH_CONDITION(archetype_index < m_archetypes.size, "EntityManager: Trying to swap a base archetype for an invalid main archetype!");
		ECS_CRASH_CONDITION(previous_base_index < m_archetypes[archetype_index].GetBaseCount(), "EntityManager: Trying to swap a base archetype with an incorrect previous index!");
		ECS_CRASH_CONDITION(new_base_index < m_archetypes[archetype_index].GetBaseCount(), "EntityManager: Trying to swap a base archetype with an incorrect new index!");
	
		// Early exit if the indices are the same
		if (previous_base_index == new_base_index) {
			return;
		}

		// Things are less involved for this case as opposed to main archetypes.
		// They only need to be swapped inside the main archetype's array and the entity info
		// References must be patched up.
		m_archetypes[archetype_index].m_base_archetypes.Swap(previous_base_index, new_base_index);

		auto patch_references = [this, archetype_index](unsigned int base_index) -> void {
			const ArchetypeBase& base_archetype = m_archetypes[archetype_index].m_base_archetypes[base_index].archetype;
			unsigned int entity_count = base_archetype.EntityCount();
			for (unsigned int index = 0; index < entity_count; index++) {
				// Use no checks call since the entities stored here should be valid, except for a weird corruption
				// Case where its not worth considering
				EntityInfo* info = m_entity_pool->GetInfoPtrNoChecks(base_archetype.m_entities[index]);
				info->base_archetype = base_index;
			}
		};

		// The archetypes have already been swapped inside m_archetypes.m_base_archetypes
		patch_references(previous_base_index);
		patch_references(new_base_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::TryGetComponent(Entity entity, Component component) {
		return (void*)((const EntityManager*)this)->TryGetComponent(entity, component);
	}

	const void* EntityManager::TryGetComponent(Entity entity, Component component) const {
		EntityInfo info = GetEntityInfo(entity);
		return TryGetComponent(info, component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::TryGetComponent(EntityInfo info, Component component) {
		return (void*)((const EntityManager*)this)->TryGetComponent(info, component);
	}

	const void* EntityManager::TryGetComponent(EntityInfo info, Component component) const {
		const ArchetypeBase* base = GetBase(info.main_archetype, info.base_archetype);
		unsigned char component_index = base->FindComponentIndex(component);
		if (component_index == UCHAR_MAX) {
			return nullptr;
		}
		return base->GetComponentByIndex(info.stream_index, component_index);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::TryGetSharedComponent(EntityInfo info, Component component) {
		return (void*)((const EntityManager*)this)->TryGetSharedComponent(info, component);
	}

	const void* EntityManager::TryGetSharedComponent(EntityInfo info, Component component) const {
		const Archetype* archetype = GetArchetype(info.main_archetype);
		SharedInstance instance = archetype->GetBaseInstance(component, info.base_archetype);
		if (instance.value == -1) {
			return nullptr;
		}
		return GetSharedData(component, instance);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::TryGetSharedComponent(Entity entity, Component component) {
		return (void*)((const EntityManager*)this)->TryGetSharedComponent(entity, component);
	}

	const void* EntityManager::TryGetSharedComponent(Entity entity, Component component) const {
		EntityInfo info = GetEntityInfo(entity);
		return TryGetSharedComponent(info, component);
	}

	// --------------------------------------------------------------------------------------------------------------------

	void* EntityManager::TryGetGlobalComponent(Component component)
	{
		return (void*)((const EntityManager*)this)->TryGetGlobalComponent(component);
	}

	const void* EntityManager::TryGetGlobalComponent(Component component) const
	{
		size_t index = SearchBytes(m_global_components, m_global_component_count, component.value);
		if (index == -1) {
			return nullptr;
		}
		return m_global_components_data[index];
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::TryGetComponentSharedInstance(Component component, Entity entity) const {
		EntityInfo info = GetEntityInfo(entity);
		return TryGetComponentSharedInstance(component, info);
	}

	// --------------------------------------------------------------------------------------------------------------------

	SharedInstance EntityManager::TryGetComponentSharedInstance(Component component, EntityInfo info) const {
		const Archetype* archetype = GetArchetype(info.main_archetype);
		return archetype->GetBaseInstance(component, info.base_archetype);
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManager::TryMergeSharedInstanceCommit(Component component, SharedInstance instance, bool remove_empty_archetypes)
	{
		ECS_CRASH_CONDITION_RETURN(ExistsSharedInstance(component, instance), false, "EntityManager: Trying to merge invalid shared instance {#} for component {#}", instance.value,
			GetSharedComponentName(component));

		const void* instance_data = GetSharedData(component, instance);
		bool was_matched = ForEachSharedInstance<true>(component, [this, component, instance, instance_data](SharedInstance current_instance) {
			if (current_instance != instance) {
				bool are_equal = false;
				const void* current_instance_data = GetSharedData(component, current_instance);
				if (m_shared_components[component.value].compare_entry.function != nullptr) {
					SharedComponentCompareFunctionData compare_data;
					compare_data.function_data = m_shared_components[component.value].compare_entry.data;
					compare_data.first = instance_data;
					compare_data.second = current_instance_data;
					are_equal = m_shared_components[component.value].compare_entry.function(&compare_data);
				}
				else {
					are_equal = memcmp(instance_data, current_instance_data, m_shared_components[component.value].info.size) == 0;
				}

				if (are_equal) {
					// We can use a slightly less efficient but straightforwarwd algorithm -
					// Remove the shared component for the entities that contain it and then
					// Readd it again
					ArchetypeQuery query;
					Component component_copy = component;
					query.shared.ConvertComponents({ &component_copy, 1 });
					ForEachArchetype(query, [this, component, instance, current_instance](Archetype* archetype) {
						// We need this since the compiler complains that the captured component and instance are const
						Component component_copy = component;
						SharedInstance instance_copy = instance;
						SharedComponentSignature shared_signature = { &component_copy, &instance_copy, 1 };
						unsigned int base_index = archetype->FindBaseIndex(shared_signature);
						if (base_index != -1) {
							ArchetypeBase* base_archetype = archetype->GetBase(base_index);
							// We need to copy the entities to a temporary buffer since removing
							// The shared instance will remove them from this base archetype
							unsigned int entity_count = base_archetype->EntityCount();
							Entity* temporary_entities = (Entity*)m_memory_manager->Allocate(sizeof(Entity) * entity_count);
							base_archetype->GetEntitiesCopy(temporary_entities);
							RemoveSharedComponentCommit({ temporary_entities, entity_count }, shared_signature.ComponentSignature());
							AddSharedComponentCommit({ temporary_entities, entity_count }, component_copy, current_instance, true);
						}
						});
					return true;
				}
			}
			return false;
		});

		if (was_matched) {
			UnregisterSharedInstanceCommit(component, instance);
			if (remove_empty_archetypes) {
				DestroyArchetypesBaseEmptyCommit(true);
			}
		}
		return was_matched;
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
			data->entities.buffer = (Entity*)OffsetPointer(data, sizeof(DeferredTryRemoveEntitiesFromHierarchy));
			entities.CopyTo(data->entities.buffer);
			data->entities.size = entities.size;
		}

		data->default_destroy_children = default_child_destroy;
		WriteCommandStream(this, parameters, { DataPointer(data, DEFERRED_TRY_REMOVE_ENTITIES_FROM_HIERARCHY), debug_info });
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::UnregisterComponentCommit(Component component)
	{
		ECS_CRASH_CONDITION(component.value < m_unique_components.size, "EntityManager: Incorrect component index {#} when trying to delete it.", component.value);
		// -1 Signals it's empty - a component was never associated to this slot
		ECS_CRASH_CONDITION(ExistsComponent(component), "EntityManager: Trying to destroy component {#} when it doesn't exist.",
			component.value);

		CopyableDeallocate(m_unique_components[component.value].data, ComponentAllocator());

		// If it has an allocator, deallocate it
		DeallocateAllocatorForComponent(this, m_unique_components[component.value].allocator);
		m_unique_components[component.value].allocator = nullptr;

		m_small_memory_manager.Deallocate(m_unique_components[component.value].name.buffer);
		m_unique_components[component.value].size = -1;
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::UnregisterSharedComponentCommit(Component component)
	{
		ECS_CRASH_CONDITION(component.value < m_shared_components.size, "EntityManager: Invalid shared component {#} when trying to destroy it.", component.value);
		// -1 Signals it's empty - a component was never associated to this slot
		ECS_CRASH_CONDITION(ExistsSharedComponent(component), "EntityManager: Trying to destroy shared component {#} when it doesn't exist.",
			component.value);

		// If it has an allocator deallocate it
		DeallocateAllocatorForComponent(this, m_shared_components[component.value].info.allocator);
		m_shared_components[component.value].info.allocator = nullptr;

		// Deallocate every instance as well
		m_shared_components[component.value].instances.stream.ForEachConst([&](void* data) {
			Deallocate(SmallAllocator(), data);
		});

		if (!m_shared_components[component.value].compare_entry.use_copy_deallocate_data) {
			CopyableDeallocate(m_shared_components[component.value].compare_entry.data, ComponentAllocator());
		}
		CopyableDeallocate(m_shared_components[component.value].info.data, ComponentAllocator());

		m_shared_components[component.value].instances.FreeBuffer();
		m_shared_components[component.value].info.size = -1;
		m_small_memory_manager.Deallocate(m_shared_components[component.value].info.name.buffer);
		const void* table_buffer = m_shared_components[component.value].named_instances.GetAllocatedBuffer();
		if (table_buffer != nullptr && m_shared_components[component.value].named_instances.GetCapacity() > 0) {
			// Deallocate the table
			m_small_memory_manager.Deallocate(table_buffer);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------

	void EntityManager::UnregisterGlobalComponentCommit(Component component)
	{
		size_t index = SearchBytes(m_global_components, m_global_component_count, component.value);
		ECS_CRASH_CONDITION(index != -1, "Missing global component {#} when trying to unregister it", component.value);

		DeallocateGlobalComponent(this, index);

		m_global_component_count--;
		m_global_components[index] = m_global_components[m_global_component_count];
		m_global_components_data[index] = m_global_components_data[m_global_component_count];
		m_global_components_info[index] = m_global_components_info[m_global_component_count];
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
		ECS_CRASH_CONDITION(ExistsSharedComponent(component), "EntityManager: Incorrect shared component {#} when trying to destroy shared instance {#}. ",
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
		ECS_CRASH_CONDITION(ExistsSharedComponent(component), "EntityManager: Incorrect shared component {#} when trying to register call to destroy shared instance {#}.",
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
		ECS_CRASH_CONDITION_RETURN(ExistsSharedComponent(component), false, "EntityManager: Shared component {#} is invalid when trying to determine "
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
		ECS_CRASH_CONDITION(ExistsSharedComponent(component), "EntityManager: Shared component {#} is invalid when trying to determine "
			"unreferenced shared instances.", GetSharedComponentName(component));

		ForEachUnreferencedSharedInstance(component, [&](SharedInstance instance, const void* data) {
			instances->AddAssert(instance);
		});
	}

	// --------------------------------------------------------------------------------------------------------------------

	void CreateEntityManagerWithPool(
		EntityManager* entity_manager,
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
		new (entity_manager_allocator) MemoryManager(allocator_size, allocator_pool_count, allocator_new_size, global_memory_manager);

		allocation = OffsetPointer(allocation, sizeof(MemoryManager));
		EntityPool* entity_pool = (EntityPool*)allocation;
		*entity_pool = EntityPool(entity_manager_allocator, entity_pool_power_of_two);

		EntityManagerDescriptor descriptor;
		descriptor.entity_pool = entity_pool;
		descriptor.memory_manager = entity_manager_allocator;

		*entity_manager = EntityManager(descriptor);
		entity_manager->m_query_cache->entity_manager = entity_manager;
	}

	// --------------------------------------------------------------------------------------------------------------------

	bool EntityManagerMoveEntityToEntityInfo(EntityManager* entity_manager, Entity entity, EntityInfo info, ComponentSignature components_to_copy) {
		EntityInfo* current_info = entity_manager->m_entity_pool->GetInfoPtr(entity);

		// If the source and target archetypes are the same and the stream index is the same, we can stop.
		// If either of these is not the same, the algorithm is the same.
		if (current_info->stream_index == info.stream_index && current_info->main_archetype == info.main_archetype && current_info->base_archetype == info.base_archetype) {
			return true;
		}

		ArchetypeBase* source_base_archetype = entity_manager->GetBase(current_info->main_archetype, current_info->base_archetype);
		ArchetypeBase* target_base_archetype = entity_manager->GetBase(info.main_archetype, info.base_archetype);

		const void* components_to_copy_data[ECS_ARCHETYPE_MAX_COMPONENTS];
		for (decltype(components_to_copy.count) index = 0; index < components_to_copy.count; index++) {
			components_to_copy_data[index] = source_base_archetype->GetComponent(*current_info, components_to_copy[index]);
		}

		// We must manually remove the entity from the base archetype
		Archetype* source_main_archetype = entity_manager->GetArchetype(current_info->main_archetype);
		// Determine the components which need to be deallocated.
		Component components_to_deallocate[ECS_ARCHETYPE_MAX_COMPONENTS];
		size_t components_to_deallocate_count = 0;
		ComponentSignature source_archetype_signature = source_main_archetype->GetUniqueSignature();
		for (decltype(source_archetype_signature.count) index = 0; index < source_archetype_signature.count; index++) {
			if (components_to_copy.Find(source_archetype_signature[index]) == UCHAR_MAX) {
				components_to_deallocate[components_to_deallocate_count++] = source_archetype_signature[index];
			}
		}

		// Remove the entity from the source archetype using a stack scope, since it needs to happen after the copy of
		// The source components has been performed.
		auto remove_entity_scope = StackScope([&]() {
			source_main_archetype->CallEntityDeallocate(*current_info, { components_to_deallocate, (unsigned char)components_to_deallocate_count });
			source_base_archetype->RemoveEntity(current_info->stream_index, entity_manager->m_entity_pool); 
		});

		// If the entity cannot be created at its desired location, create it as is and return false
		if (info.stream_index > target_base_archetype->m_size) {
			current_info->stream_index = target_base_archetype->AddEntity(entity, components_to_copy, components_to_copy_data);
			current_info->main_archetype = info.main_archetype;
			current_info->base_archetype = info.base_archetype;
			return false;
		}

		// The entity can be created at the desired location. To avoid making an unnecessary swap, move the target index
		// In the last place and then overwrite that index with this entity.

		// Add the target entity once more, such that its components are properly copied.
		const void* target_entity_buffer_data[ECS_ARCHETYPE_MAX_COMPONENTS];
		ComponentSignature target_archetype_signature = entity_manager->GetArchetype(info.main_archetype)->GetUniqueSignature();
		for (decltype(target_archetype_signature.count) index = 0; index < target_archetype_signature.count; index++) {
			target_entity_buffer_data[index] = target_base_archetype->GetComponentByIndex(info.stream_index, index);
		}
		// We also need to update that entity's info stream index.
		EntityInfo* target_entity_to_be_swapped_info = entity_manager->m_entity_pool->GetInfoPtr(target_base_archetype->m_entities[info.stream_index]);
		target_entity_to_be_swapped_info->stream_index = target_base_archetype->AddEntity(target_base_archetype->m_entities[info.stream_index], target_archetype_signature, target_entity_buffer_data);

		// Now, overwrite the components of the target with the current data.
		target_base_archetype->m_entities[info.stream_index] = entity;
		target_base_archetype->CopyByComponents({ (unsigned int)info.stream_index, (unsigned int)1 }, components_to_copy_data, components_to_copy);
		current_info->stream_index = info.stream_index;
		current_info->main_archetype = info.main_archetype;
		current_info->base_archetype = info.base_archetype;
		return true;
	}

	// --------------------------------------------------------------------------------------------------------------------

}