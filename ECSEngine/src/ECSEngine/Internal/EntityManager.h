#pragma once
#include "../Core.h"
#include "../Allocators/MemoryManager.h"
#include "../Allocators/LinearAllocator.h"
#include "Archetype.h"
#include "../Containers/AtomicStream.h"

#ifndef ECS_ENTITY_MANAGER_DEFAULT_MAX_ARCHETYPE_COUNT_FOR_NEW_ARCHETYPE
#define ECS_ENTITY_MANAGER_DEFAULT_MAX_ARCHETYPE_COUNT_FOR_NEW_ARCHETYPE 8
#endif

#ifndef ECS_ENTITY_MANAGER_DEFERRED_ACTION_CAPACITY
#define ECS_ENTITY_MANAGER_DEFERRED_ACTION_CAPACITY (1 << 16)
#endif

namespace ECSEngine {

	ECS_CONTAINERS;

	struct EntityManager;

	enum DeferredActionType : unsigned char;

	struct ECSENGINE_API DeferredAction {
		void* data;
		DeferredActionType type;
	};

	/*typedef void (*EntityManagerEventCallback)(EntityManager*, void*);

	struct ECSENGINE_API EntityManagerEvent {
		EntityManagerEventCallback callback;
		void* data;
		size_t data_size;
	};

	enum EntityManagerEventType : unsigned char {
		ECS_ENTITY_MANAGER_EVENT_TRACK_ENTITIES,
		ECS_ENTITY_MANAGER_EVENT_TRACK
	};*/

	struct ECSENGINE_API EntityManagerDescriptor {
		MemoryManager* memory_manager;
		EntityPool* entity_pool;
		unsigned int deferred_action_capacity = ECS_ENTITY_MANAGER_DEFERRED_ACTION_CAPACITY;
	};

	using EntityManagerCommandStream = CapacityStream<DeferredAction>;

	enum EntityManagerCopyEntityDataType {
		// Splats the same value of the component to all entities
		ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_SPLAT,
		// Data parsed by components layout AAAAA BBBBB CCCCC; Each entity has a separate pointer for each component
		ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_COMPONENTS_SCATTERED,
		// Data parsed by components layout AAAAA BBBBB CCCCC; Each component gets a pointer - data is blitted to locations
		ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_COMPONENTS_CONTIGUOUS,
		// Data parsed by entities ABC ABC ABC ABC ABC; A, B and C each have a pointer
		ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY,
		// Data parsed by entities ABC ABC ABC ABC ABC; A single pointer to the start of the structure
		ECS_ENTITY_MANAGER_COPY_ENTITY_DATA_BY_ENTITY_CONTIGUOUS
	};

	struct ECSENGINE_API DeferredActionParameters {
		EntityManagerCommandStream* command_stream = nullptr;
		bool data_is_stable = false;
		bool entities_are_stable = false;
	};

	struct ECSENGINE_API EntityManager
	{
	public:
		EntityManager(const EntityManagerDescriptor& descriptor);

		// ---------------------------------------------------------------------------------------------------

		void AddComponentCommit(Entity entity, Component component);

		// Deferred Call
		void AddComponent(Entity entity, Component component, DeferredActionParameters parameters = {});

		// ---------------------------------------------------------------------------------------------------

		void AddComponentCommit(Entity entity, Component component, const void* data);

		// Deferred Call
		void AddComponent(Entity entity, Component component, const void* data, DeferredActionParameters parameters = {});

		// ---------------------------------------------------------------------------------------------------

		void AddComponentCommit(Entity entity, ComponentSignature components);

		// Deferred Call - it will register it inside the command stream
		void AddComponent(Entity entity, ComponentSignature components, DeferredActionParameters = {});

		// ---------------------------------------------------------------------------------------------------

		void AddComponentCommit(Entity entity, ComponentSignature components, const void** data);

		// Deferred Call - it will register it inside the command stream
		void AddComponent(Entity entity, ComponentSignature components, const void** data, DeferredActionParameters = {});

		// ---------------------------------------------------------------------------------------------------

		// entities must belong to the same base archetype 
		void AddComponentCommit(Stream<Entity> entities, Component component);

		// Deferred Call - it will register it inside a command stream
		// entities must belong to the same archetype
		void AddComponent(Stream<Entity> entities, Component component, DeferredActionParameters parameters = {});

		// ---------------------------------------------------------------------------------------------------

		// entities must belong to the same base archetype; data will be used to initialize all components to the same value
		void AddComponentCommit(Stream<Entity> entities, Component component, const void* data);

		// Deferred Call - it will register it inside a command stream
		// entities must belong to the same base archetype; data will be used to initialize all components to the same value
		void AddComponent(Stream<Entity> entities, Component component, const void* data, DeferredActionParameters parameters = {});

		// ---------------------------------------------------------------------------------------------------

		// entities must belong to the same base archetype
		void AddComponentCommit(Stream<Entity> entities, ComponentSignature components);

		// Deferred Call - it will register it inside a command stream
		// entities must belong to the same base archetype
		void AddComponent(Stream<Entity> entities, ComponentSignature components, DeferredActionParameters parameters = {});

		// ---------------------------------------------------------------------------------------------------

		// entities must belong to the same base archetype
		void AddComponentCommit(Stream<Entity> entities, ComponentSignature components, const void** data, EntityManagerCopyEntityDataType copy_type);

		// Deferred Call - it will register it inside a command stream
		// entities must belong to the same base archetype; data parsed by component
		// data -> A B C ; each entity will have as components A, B, C
		// components.count pointers must be present in data
		void AddComponent(
			Stream<Entity> entities,
			ComponentSignature components,
			const void** data,
			EntityManagerCopyEntityDataType copy_type,
			DeferredActionParameters parameters = {}
		);

		// ---------------------------------------------------------------------------------------------------

		void AddSharedComponentCommit(Entity entity, Component shared_component, SharedInstance instance);

		// Deferred Call
		void AddSharedComponent(Entity entity, Component shared_component, SharedInstance instance, DeferredActionParameters parameters = {});

		// ---------------------------------------------------------------------------------------------------

		// entities must belong to the same base archetype
		void AddSharedComponentCommit(Stream<Entity> entities, Component shared_component, SharedInstance instance);

		// Deferred Call
		// entities must belong to the same base archetype
		void AddSharedComponent(Stream<Entity> entities, Component shared_component, SharedInstance instance, DeferredActionParameters parameters = {});

		// ---------------------------------------------------------------------------------------------------

		void AddSharedComponentCommit(Entity entity, SharedComponentSignature components);

		// Deferred Call
		void AddSharedComponent(Entity entity, SharedComponentSignature components, DeferredActionParameters parameters = {});

		// ---------------------------------------------------------------------------------------------------

		// entities must belong to the same base archetype
		void AddSharedComponentCommit(Stream<Entity> entities, SharedComponentSignature components);

		// Deferred Call
		// entities must belong to the same base archetype
		void AddSharedComponent(Stream<Entity> entities, SharedComponentSignature components, DeferredActionParameters parameters = {});

		// ---------------------------------------------------------------------------------------------------

		// Thread safe - if the value is exceedingly high, it will go to the global memory manager with a request under lock
		void* AllocateTemporaryBuffer(size_t size, size_t alignment = 8);

		// ---------------------------------------------------------------------------------------------------

		// It will commit the archetype addition
		unsigned short CreateArchetypeCommit(ComponentSignature unique_signature, ComponentSignature shared_signature);

		// Deferred Call
		void CreateArchetype(ComponentSignature unique_signature, ComponentSignature shared_signature, EntityManagerCommandStream* command_stream = nullptr);

		// ---------------------------------------------------------------------------------------------------

		// It will commit the archetype addition
		// If the main archetype does not exist, it will create it
		ushort2 CreateArchetypeBaseCommit(
			ComponentSignature unique_signature, 
			SharedComponentSignature shared_signature, 
			unsigned int archetype_chunk_size = ECS_ARCHETYPE_DEFAULT_CHUNK_SIZE_FOR_NEW_BASE
		);

		// Deferred Call
		// If the main archetype does not exist, it will create it
		void CreateArchetypeBase(
			ComponentSignature unique_signature,
			SharedComponentSignature shared_signature,
			EntityManagerCommandStream* command_stream = nullptr,
			unsigned int archetype_chunk_size = ECS_ARCHETYPE_DEFAULT_CHUNK_SIZE_FOR_NEW_BASE
		);

		// ---------------------------------------------------------------------------------------------------

		// It will commit the archetype addition
		unsigned short CreateArchetypeBaseCommit(
			unsigned short archetype_index,
			SharedComponentSignature shared_signature,
			unsigned int archetype_chunk_size = ECS_ARCHETYPE_DEFAULT_CHUNK_SIZE_FOR_NEW_BASE
		);

		// Deferred Call
		void CreateArchetypeBase(
			unsigned short archetype_index,
			SharedComponentSignature shared_signature,
			EntityManagerCommandStream* command_stream = nullptr,
			unsigned int archetype_chunk_size = ECS_ARCHETYPE_DEFAULT_CHUNK_SIZE_FOR_NEW_BASE
		);

		// ---------------------------------------------------------------------------------------------------

		Entity CreateEntityCommit(ComponentSignature unique_components, SharedComponentSignature shared_components);

		// It will search for an archetype that matches the given signatures
		// If it doesn't exist, it will create a new one
		void CreateEntity(ComponentSignature unique_components, SharedComponentSignature shared_components, EntityManagerCommandStream* command_stream = nullptr);

		// ---------------------------------------------------------------------------------------------------

		// It will search for an archetype that matches the given signatures
		// If it doesn't exist, it will create a new one
		void CreateEntitiesCommit(
			unsigned int count,
			ComponentSignature unique_components,
			SharedComponentSignature shared_components,
			Entity* entities = nullptr
		);

		// Deferred Call
		// The entities generated cannot be determined right now since it is deferred for later
		// Filling a buffer with the entities generated will induce a syncronization barrier which may be too costly
		// It will search for an archetype that matches the given signatures
		// If it doesn't exist, it will create a new one
		void CreateEntities(
			unsigned int count,
			ComponentSignature unique_components,
			SharedComponentSignature shared_components,
			EntityManagerCommandStream* command_stream = nullptr
		);

		// ---------------------------------------------------------------------------------------------------

		// It will search for an archetype that matches the given signatures
		// If it doesn't exist, it will create a new one
		void CreateEntitiesCommit(
			unsigned int count,
			ComponentSignature unique_components,
			SharedComponentSignature shared_components,
			ComponentSignature components_with_data,
			const void** data,
			EntityManagerCopyEntityDataType copy_type,
			Entity* entities = nullptr
		);

		// Deferred Call
		// The entities generated cannot be determined right now since it is deferred for later
		// Filling a buffer with the entities generated will induce a syncronization barrier which may be too costly
		// It will search for an archetype that matches the given signatures
		// If it doesn't exist, it will create a new one
		void CreateEntities(
			unsigned int count,
			ComponentSignature unique_components,
			SharedComponentSignature shared_components,
			ComponentSignature components_with_data,
			const void** data,
			EntityManagerCopyEntityDataType copy_type,
			EntityManagerCommandStream* command_stream = nullptr
		);

		// ---------------------------------------------------------------------------------------------------

		void DeleteEntityCommit(Entity entity);

		// Deferred Call - it will register it inside the command stream
		void DeleteEntity(Entity entity, EntityManagerCommandStream* command_stream = nullptr);

		// ---------------------------------------------------------------------------------------------------

		void DeleteEntitiesCommit(Stream<Entity> entities);

		// Deferred Call 
		void DeleteEntities(Stream<Entity> entities, DeferredActionParameters parameters = {});

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeCommit(unsigned short archetype_index);

		// Deferred Call
		void DestroyArchetype(unsigned short archetype_index, EntityManagerCommandStream* command_stream = nullptr);

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeCommit(ComponentSignature unique_components, ComponentSignature shared_components);

		// Deferred Call
		void DestroyArchetype(ComponentSignature unique_components, ComponentSignature shared_components, EntityManagerCommandStream* command_stream = nullptr);

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeBaseCommit(unsigned short archetype_index, unsigned short archetype_subindex);

		// Deferred Call
		void DestroyArchetypeBase(unsigned short archetype_index, unsigned short archetype_subindex, EntityManagerCommandStream* command_stream = nullptr);

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeBaseCommit(ComponentSignature unique_components, SharedComponentSignature shared_components);

		// Deferred Call - it will register it inside the command stream
		void DestroyArchetypeBase(ComponentSignature unique_components, SharedComponentSignature shared_components, EntityManagerCommandStream* command_stream = nullptr);

		// ---------------------------------------------------------------------------------------------------

		// It will look for an exact match
		// Returns -1 if it doesn't exist
		unsigned short ECS_VECTORCALL FindArchetype(ArchetypeQuery query) const;

		// It will look for an exact match
		// Returns nullptr if it doesn't exist
		Archetype* ECS_VECTORCALL FindArchetypePtr(ArchetypeQuery query);

		// ---------------------------------------------------------------------------------------------------

		// It will look for an exact match
		// Returns -1 if it doesn't exist
		unsigned short ECS_VECTORCALL FindArchetypeExclude(ArchetypeQueryExclude query) const;

		// It will look for an exact match
		// Returns nullptr if it doesn't exist
		Archetype* ECS_VECTORCALL FindArchetypeExcludePtr(ArchetypeQueryExclude query);

		// ---------------------------------------------------------------------------------------------------

		// It will look for an exact match
		// Returns -1, -1 if the main archetype cannot be found
		// Returns main_index, -1 if the main archetype is found but the base is not
		ushort2 ECS_VECTORCALL FindArchetypeBase(
			ArchetypeQuery query,
			VectorComponentSignature shared_instances
		) const;

		// It will look for an exact match
		// Returns nullptr if the base archetype cannot be found
		ArchetypeBase* ECS_VECTORCALL FindArchetypeBasePtr(
			ArchetypeQuery query,
			VectorComponentSignature shared_instances
		);

		// ---------------------------------------------------------------------------------------------------

		// It will look for an exact match
		// Returns -1, -1 if the main archetype cannot be found
		// Returns main_index, -1 if the main archetype is found but the base is not
		ushort2 ECS_VECTORCALL FindArchetypeBaseExclude(
			ArchetypeQueryExclude query,
			VectorComponentSignature shared_instances
		) const;

		// It will look for an exact match
		// Returns nullptr if the base archetype cannot be found
		ArchetypeBase* ECS_VECTORCALL FindArchetypeBaseExcludePtr(
			ArchetypeQueryExclude query,
			VectorComponentSignature shared_instances
		);

		// ---------------------------------------------------------------------------------------------------

		// It requires a syncronization barrier!! If the archetype does not exist, then it will commit the creation of a new one
		unsigned short FindOrCreateArchetype(ComponentSignature unique_signature, ComponentSignature shared_signature);

		ushort2 FindOrCreateArchetypeBase(
			ComponentSignature unique_signature,
			SharedComponentSignature shared_signature,
			unsigned int chunk_size = ECS_ARCHETYPE_DEFAULT_CHUNK_SIZE_FOR_NEW_BASE
		);

		// Executes all deferred calls
		void Flush();

		// Executes all deferred calls inside a command stream
		void Flush(EntityManagerCommandStream command_stream);

		Archetype* GetArchetype(unsigned short index);

		const Archetype* GetArchetype(unsigned short index) const;

		ArchetypeBase* GetArchetypeBase(unsigned short main_index, unsigned short base_index);

		const ArchetypeBase* GetArchetypeBase(unsigned short main_index, unsigned short base_index) const;

		// It requires a syncronization barrier!! If the archetype does not exist, then it will commit the creation of a new one
		Archetype* GetOrCreateArchetype(ComponentSignature unique_signature, ComponentSignature shared_signature);

		ArchetypeBase* GetOrCreateArchetypeBase(
			ComponentSignature unique_signature, 
			SharedComponentSignature shared_signature,
			unsigned int chunk_size = ECS_ARCHETYPE_DEFAULT_CHUNK_SIZE_FOR_NEW_BASE
		);

		// It will fill in the indices of the archetypes that verify the query
		void ECS_VECTORCALL GetArchetypes(ArchetypeQuery query, CapacityStream<unsigned short>& archetypes) const;

		// It will fill in the pointers of the archetypes that verify the query
		void ECS_VECTORCALL GetArchetypesPtrs(ArchetypeQuery query, CapacityStream<Archetype*>& archetypes) const;

		// It will fill in the indices of the archetypes that verify the query
		void ECS_VECTORCALL GetArchetypesExclude(ArchetypeQueryExclude query, CapacityStream<unsigned short>& archetypes) const;

		// It will fill in the pointers of the archetypes that verify the query
		void ECS_VECTORCALL GetArchetypesExcludePtrs(ArchetypeQueryExclude query, CapacityStream<Archetype*>& archetypes) const;

		void* GetComponent(Entity entity, Component component) const;

		void* GetComponentWithIndex(Entity entity, unsigned char component_index) const;

		// Data must have components.count pointers
		void GetComponent(Entity entity, ComponentSignature components, void** data) const;

		// Data must have components.count pointers
		void GetComponentWithIndex(Entity entity, ComponentSignature components, void** data) const;

		EntityInfo GetEntityInfo(Entity entity) const;

		ComponentSignature GetEntitySignature(Entity entity, Component* components) const;

		// It will point to the archetype's signature - Do not modify!!
		ComponentSignature GetEntitySignatureStable(Entity entity) const;

		SharedComponentSignature GetEntitySharedSignature(Entity entity, Component* shared, SharedInstance* instances);

		// It will point to the archetypes' signature (the base and the main) - Do not modify!!
		SharedComponentSignature GetEntitySharedSignatureStable(Entity entity);

		void GetEntityCompleteSignature(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared);

		// It will point to the archetypes' signature (the base and the main) - Do not modify!!
		void GetEntityCompleteSignatureStable(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared);

		// It will fill in the entities stream
		// Consider the other variant that only aliases the buffers so no copies are needed
		void ECS_VECTORCALL GetEntities(
			ArchetypeQuery query, 
			CapacityStream<Entity>& entities
		) const;

		// It will alias the archetype entities buffers - no need to copy
		// Make sure the values are not modified
		void ECS_VECTORCALL GetEntities(ArchetypeQuery query, Stream<Entity>* entities) const;

		// It will fill in the entities stream
		void ECS_VECTORCALL GetEntitiesExclude(
			ArchetypeQueryExclude query,
			CapacityStream<Entity>& entities
		) const;

		// It will alias the archetype entities buffers - no need to copy
		// Make sure the values are not modified
		void ECS_VECTORCALL GetEntitiesExclude(ArchetypeQueryExclude query, Stream<Entity>* entities) const;

		SharedInstance GetSharedComponentInstance(Component component, const void* data) const;

		SharedInstance GetNamedSharedComponentInstance(Component component, ResourceIdentifier identifier) const;

		void RegisterComponentCommit(Component component, unsigned short size);

		void RegisterComponent(Component component, unsigned short size, EntityManagerCommandStream* command_stream = nullptr);

		void RegisterSharedComponentCommit(Component component, unsigned short size);

		void RegisterSharedComponent(Component component, unsigned short size, EntityManagerCommandStream* command_stream = nullptr);

		SharedInstance RegisterSharedInstanceCommit(Component component, const void* data);

		void RegisterSharedInstance(Component component, const void* data, EntityManagerCommandStream* command_stream = nullptr);

		SharedInstance RegisterNamedSharedInstanceCommit(Component component, ResourceIdentifier identifier, const void* data);

		void RegisterNamedSharedInstance(Component component, ResourceIdentifier identifier, const void* data, EntityManagerCommandStream* command_stream = nullptr);

		// If the component or the instance doesn't exist, it will assert
		void SetSharedComponentData(Component component, SharedInstance instance, const void* data);

		// If the component or the instance doesn't exist, it will assert
		void SetNamedSharedComponentData(Component component, ResourceIdentifier identifier, const void* data);

	//private:

		struct ECSENGINE_API TemporaryAllocator {
			TemporaryAllocator();
			TemporaryAllocator(GlobalMemoryManager* backup);

			void* Allocate(size_t size, size_t alignment = 8);
			void Clear();

			SpinLock lock;
			GlobalMemoryManager* backup;
			CapacityStream<AtomicStream<char>> buffers;
			// Keep track of allocations that went straight to the global allocator
			CapacityStream<void*> backup_allocations;
		};

		MemoryManager m_small_memory_manager;
		ResizableStream<Archetype> m_archetypes;
		// Size represents the actual capacity
		Stream<ComponentInfo> m_unique_components;
		// Size represents the actual capacity
		Stream<SharedComponentInfo> m_shared_components;
		AtomicStream<DeferredAction> m_deferred_actions;
		MemoryManager* m_memory_manager;
		EntityPool* m_entity_pool;
		TemporaryAllocator m_temporary_allocator;
	};

}

