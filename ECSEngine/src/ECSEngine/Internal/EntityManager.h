#pragma once
#include "../Core.h"
#include "../Allocators/MemoryManager.h"
#include "../Allocators/LinearAllocator.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "Archetype.h"
#include "../Containers/AtomicStream.h"
#include "EntityHierarchy.h"
#include "../Containers/DataPointer.h"

#ifndef ECS_ENTITY_MANAGER_DEFAULT_MAX_ARCHETYPE_COUNT_FOR_NEW_ARCHETYPE
#define ECS_ENTITY_MANAGER_DEFAULT_MAX_ARCHETYPE_COUNT_FOR_NEW_ARCHETYPE 8
#endif

#ifndef ECS_ENTITY_MANAGER_DEFERRED_ACTION_CAPACITY
#define ECS_ENTITY_MANAGER_DEFERRED_ACTION_CAPACITY (1 << 12)
#endif

namespace ECSEngine {

	struct EntityManager;
	struct ArchetypeQueryCache;

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

	struct EntityManagerDescriptor {
		MemoryManager* memory_manager;
		EntityPool* entity_pool;
		unsigned int deferred_action_capacity = ECS_ENTITY_MANAGER_DEFERRED_ACTION_CAPACITY;
	};

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

	struct DeferredActionParameters {
		EntityManagerCommandStream* command_stream = nullptr;
		bool data_is_stable = false;
		bool entities_are_stable = false;
	};

	struct EntityPair {
		Entity parent;
		Entity child;
	};

	struct ECSENGINE_API EntityManager
	{
	public:
		EntityManager(const EntityManagerDescriptor& descriptor);

		// Use this this if you want to pass this buffer to the deferred calls with a stable flag.
		// This avoids making unnecessary copies between buffers. Do not ask ridiculous sizes. Try for the smallest.
		// If the data size cannot be predicted and can be potentially large, precondition the data into a stack buffer
		// and then use the deferred call to copy it
		void* AllocateTemporaryBuffer(size_t size, size_t alignment = 8);

		// ---------------------------------------------------------------------------------------------------

		void AddComponentCommit(Entity entity, Component component);

		// Deferred Call
		void AddComponent(Entity entity, Component component, DeferredActionParameters parameters = {}, DebugInfo debug_info = { ECS_LOCATION });

		// ---------------------------------------------------------------------------------------------------

		void AddComponentCommit(Entity entity, Component component, const void* data);

		// Deferred Call
		void AddComponent(
			Entity entity,
			Component component,
			const void* data,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		void AddComponentCommit(Entity entity, ComponentSignature components);

		// Deferred Call - it will register it inside the command stream
		void AddComponent(Entity entity, ComponentSignature components, DeferredActionParameters = {}, DebugInfo debug_info = { ECS_LOCATION });

		// ---------------------------------------------------------------------------------------------------

		void AddComponentCommit(Entity entity, ComponentSignature components, const void** data);

		// Deferred Call - it will register it inside the command stream
		void AddComponent(
			Entity entity,
			ComponentSignature components,
			const void** data,
			DeferredActionParameters = {},
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		// entities must belong to the same base archetype 
		void AddComponentCommit(Stream<Entity> entities, Component component);

		// Deferred Call - it will register it inside a command stream
		// entities must belong to the same archetype
		void AddComponent(
			Stream<Entity> entities,
			Component component,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		// entities must belong to the same base archetype; data will be used to initialize all components to the same value
		void AddComponentCommit(Stream<Entity> entities, Component component, const void* data);

		// Deferred Call - it will register it inside a command stream
		// entities must belong to the same base archetype; data will be used to initialize all components to the same value
		void AddComponent(Stream<Entity> entities, Component component, const void* data, DeferredActionParameters parameters = {},
			DebugInfo debug_info = { ECS_LOCATION });

		// ---------------------------------------------------------------------------------------------------

		// entities must belong to the same base archetype
		void AddComponentCommit(Stream<Entity> entities, ComponentSignature components);

		// Deferred Call - it will register it inside a command stream
		// entities must belong to the same base archetype
		void AddComponent(Stream<Entity> entities, ComponentSignature components, DeferredActionParameters parameters = {},
			DebugInfo debug_info = { ECS_LOCATION });

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
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		void AddSharedComponentCommit(Entity entity, Component shared_component, SharedInstance instance);

		// Deferred Call
		void AddSharedComponent(
			Entity entity,
			Component shared_component,
			SharedInstance instance,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		// entities must belong to the same base archetype
		void AddSharedComponentCommit(Stream<Entity> entities, Component shared_component, SharedInstance instance);

		// Deferred Call
		// entities must belong to the same base archetype
		void AddSharedComponent(
			Stream<Entity> entities,
			Component shared_component,
			SharedInstance instance,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		void AddSharedComponentCommit(Entity entity, SharedComponentSignature components);

		// Deferred Call
		void AddSharedComponent(
			Entity entity,
			SharedComponentSignature components,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		// Sets the child to have as parent the given entity. The child must not be previously found in the hierarchy
		// If the parent is set to -1, then the child will be inserted as a root
		void AddEntityToHierarchyCommit(Stream<EntityPair> pairs);

		// Deferred Call
		// Sets the child to have as parent the given entity. The child must not be previously found in the hierarchy
		// If the parent is set to -1, then the child will be inserted as a root
		void AddEntityToHierarchy(
			Stream<EntityPair> pairs,
			DeferredActionParameters parameters,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// Sets the child to have as parent the given entity. The child must not be previously found in the hierarchy
		// If the parent is set to -1, then the child will be inserted as a root
		void AddEntityToHierarchyCommit(const Entity* parents, const Entity* children, unsigned int count);

		// Deferred call
		// Sets the child to have as parent the given entity. The child must not be previously found in the hierarchy
		// If the parent is set to -1, then the child will be inserted as a root
		void AddEntityToHierarchy(const Entity* parents, const Entity* children, unsigned int count, DeferredActionParameters parameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		// Sets the entities to be children of the given parent.
		void AddEntitiesToParentCommit(Stream<Entity> entities, Entity parent);

		// Deferred call
		// Sets the entities to be children of the given parent.
		void AddEntitiesToParent(Stream<Entity> entities, Entity parent, DeferredActionParameters parameters, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		// entities must belong to the same base archetype
		void AddSharedComponentCommit(Stream<Entity> entities, SharedComponentSignature components);

		// Deferred Call
		// entities must belong to the same base archetype
		void AddSharedComponent(
			Stream<Entity> entities,
			SharedComponentSignature components,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		// Mostly for editor purposes, like when a component ID is changed (this function is given
		// because the vector signature needs to be changed besides the ComponentSignature)
		// It changes the registration of the component as well
		void ChangeComponentIndex(Component old_component, Component new_component);

		// Mostly for editor purposes, like when a component ID is changed (this function is given
		// because the vector signature needs to be changed besides the ComponentSignature)
		// It changes the registration of the component as well
		void ChangeSharedComponentIndex(Component old_component, Component new_component);

		// ---------------------------------------------------------------------------------------------------

		void ChangeEntityParentCommit(Stream<EntityPair> pairs);

		void ChangeEntityParent(
			Stream<EntityPair> pairs,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		void ChangeOrSetEntityParentCommit(Stream<EntityPair> pairs);

		void ChangeOrSetEntityParent(
			Stream<EntityPair> pairs, 
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------
	
		// The tag should be only the bit index, not the actual value
		void ClearEntityTagCommit(Stream<Entity> entities, unsigned char tag);

		// Deferred call
		// The tag should be only the bit index, not the actual value
		void ClearEntityTag(Stream<Entity> entities, unsigned char tag, DeferredActionParameters parameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		void ClearFrame();

		// It will copy everything. The components, the shared components, the archetypes, the entities inside the entity pool
		void CopyOther(const EntityManager* entity_manager);

		// ---------------------------------------------------------------------------------------------------

		// Deferred call
		void CopyEntity(Entity entity, unsigned int count, bool copy_children = true, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = ECS_DEBUG_INFO);

		// If the copies are desired, then give a copies buffer. It will only populate the top most entity (not every single child)
		void CopyEntityCommit(Entity entity, unsigned int count, bool copy_children = true, Entity* copies = nullptr);

		// ---------------------------------------------------------------------------------------------------

		// It will commit the archetype addition
		unsigned int CreateArchetypeCommit(ComponentSignature unique_signature, ComponentSignature shared_signature);

		// Deferred Call
		void CreateArchetype(
			ComponentSignature unique_signature,
			ComponentSignature shared_signature,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		// It will commit the archetype addition
		// If the main archetype does not exist, it will create it
		uint2 CreateArchetypeBaseCommit(
			ComponentSignature unique_signature,
			SharedComponentSignature shared_signature,
			unsigned int starting_size = ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT
		);

		// Deferred Call
		// If the main archetype does not exist, it will create it
		void CreateArchetypeBase(
			ComponentSignature unique_signature,
			SharedComponentSignature shared_signature,
			EntityManagerCommandStream* command_stream = nullptr,
			unsigned int starting_size = ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		// It will commit the archetype addition
		unsigned int CreateArchetypeBaseCommit(
			unsigned int archetype_index,
			SharedComponentSignature shared_signature,
			unsigned int starting_size = ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT
		);

		// Deferred Call
		void CreateArchetypeBase(
			unsigned int archetype_index,
			SharedComponentSignature shared_signature,
			EntityManagerCommandStream* command_stream = nullptr,
			unsigned int starting_size = ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		Entity CreateEntityCommit(ComponentSignature unique_components, SharedComponentSignature shared_components, bool exclude_from_hierarchy = false);

		// It will search for an archetype that matches the given signatures
		// If it doesn't exist, it will create a new one
		void CreateEntity(
			ComponentSignature unique_components,
			SharedComponentSignature shared_components,
			bool exclude_from_hierarchy = false,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		// It will search for an archetype that matches the given signatures
		// If it doesn't exist, it will create a new one
		void CreateEntitiesCommit(
			unsigned int count,
			ComponentSignature unique_components,
			SharedComponentSignature shared_components,
			bool exclude_from_hierarchy = false,
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
			bool exclude_from_hierarchy = false,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = { ECS_LOCATION }
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
			bool exclude_from_hierarchy = false,
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
			bool exclude_from_hierarchy = false,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// Returns the byte size of a component
		unsigned int ComponentSize(Component component) const;

		// Returns the byte size of a component
		unsigned int SharedComponentSize(Component component) const;

		// ---------------------------------------------------------------------------------------------------

		void DeleteEntityCommit(Entity entity);

		// Deferred Call - it will register it inside the command stream
		void DeleteEntity(Entity entity, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = { ECS_LOCATION });

		// ---------------------------------------------------------------------------------------------------

		void DeleteEntitiesCommit(Stream<Entity> entities);

		// Deferred Call 
		void DeleteEntities(Stream<Entity> entities, DeferredActionParameters parameters = {}, DebugInfo debug_info = { ECS_LOCATION });

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeCommit(unsigned int archetype_index);

		// Deferred Call
		void DestroyArchetype(unsigned int archetype_index, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = { ECS_LOCATION });

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeCommit(ComponentSignature unique_components, ComponentSignature shared_components);

		// Deferred Call
		void DestroyArchetype(
			ComponentSignature unique_components, 
			ComponentSignature shared_components, 
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeBaseCommit(unsigned int archetype_index, unsigned int archetype_subindex);

		// Deferred Call
		void DestroyArchetypeBase(
			unsigned int archetype_index,
			unsigned int archetype_subindex,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeBaseCommit(ComponentSignature unique_components, SharedComponentSignature shared_components);

		// Deferred Call - it will register it inside the command stream
		void DestroyArchetypeBase(
			ComponentSignature unique_components, 
			SharedComponentSignature shared_components, 
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		void EndFrame();

		// ---------------------------------------------------------------------------------------------------

		// Returns the index of the component inside the archetype component list
		unsigned char FindArchetypeUniqueComponent(unsigned int archetype_index, Component component) const;

		// Returns the indices of the components inside the archetype component list
		void FindArchetypeUniqueComponent(unsigned int archetype_index, ComponentSignature components, unsigned char* indices) const;

		// Returns the indices of the components inside the archetype component list
		void ECS_VECTORCALL FindArchetypeUniqueComponentVector(unsigned int archetype_index, VectorComponentSignature components, unsigned char* indices) const;

		// ---------------------------------------------------------------------------------------------------

		unsigned char FindArchetypeSharedComponent(unsigned int archetype_index, Component component) const;

		void FindArchetypeSharedComponent(unsigned int archetype_index, ComponentSignature components, unsigned char* indices) const;

		void ECS_VECTORCALL FindArchetypeSharedComponentVector(unsigned int archetype_index, VectorComponentSignature components, unsigned char* indices) const;

		// ---------------------------------------------------------------------------------------------------

		// It will look for an exact match
		// Returns -1 if it doesn't exist
		unsigned int ECS_VECTORCALL FindArchetype(ArchetypeQuery query) const;

		// It will look for an exact match
		// Returns nullptr if it doesn't exist
		Archetype* ECS_VECTORCALL FindArchetypePtr(ArchetypeQuery query);

		// ---------------------------------------------------------------------------------------------------

		// It will look for an exact match
		// Returns -1 if it doesn't exist
		unsigned int ECS_VECTORCALL FindArchetypeExclude(ArchetypeQueryExclude query) const;

		// It will look for an exact match
		// Returns nullptr if it doesn't exist
		Archetype* ECS_VECTORCALL FindArchetypeExcludePtr(ArchetypeQueryExclude query);

		// ---------------------------------------------------------------------------------------------------

		// It will look for an exact match
		// Returns -1, -1 if the main archetype cannot be found
		// Returns main_index, -1 if the main archetype is found but the base is not
		uint2 ECS_VECTORCALL FindBase(
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
		uint2 ECS_VECTORCALL FindArchetypeBaseExclude(
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
		unsigned int FindOrCreateArchetype(ComponentSignature unique_signature, ComponentSignature shared_signature);

		uint2 FindOrCreateArchetypeBase(
			ComponentSignature unique_signature,
			SharedComponentSignature shared_signature,
			unsigned int starting_size = ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT
		);

		// Executes all deferred calls, including clearing tags and setting them
		void Flush();

		// Executes all deferred calls inside a command stream
		void Flush(EntityManagerCommandStream command_stream);

		// Verifies if the entity is still valid. It might become invalid if another system deleted it in the meantime
		bool ExistsEntity(Entity entity) const;

		// Verifies if a component already exists at that slot
		bool ExistsComponent(Component component) const;

		// Verifies if a shared component was already allocated at that slot
		bool ExistsSharedComponent(Component component) const;

		// Verifies if a shared instance is a valid instance - checks to see if the component also exists
		bool ExistsSharedInstance(Component component, SharedInstance instance) const;

		unsigned int GetArchetypeCount() const;

		Archetype* GetArchetype(unsigned int index);

		const Archetype* GetArchetype(unsigned int index) const;

		ArchetypeBase* GetBase(unsigned int main_index, unsigned int base_index);

		const ArchetypeBase* GetBase(unsigned int main_index, unsigned int base_index) const;

		// It requires a syncronization barrier!! If the archetype does not exist, then it will commit the creation of a new one
		Archetype* GetOrCreateArchetype(ComponentSignature unique_signature, ComponentSignature shared_signature);

		ArchetypeBase* GetOrCreateArchetypeBase(
			ComponentSignature unique_signature, 
			SharedComponentSignature shared_signature,
			unsigned int starting_size = ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT
		);

		// It will fill in the indices of the archetypes that verify the query
		void ECS_VECTORCALL GetArchetypes(ArchetypeQuery query, CapacityStream<unsigned int>& archetypes) const;

		// It will fill in the pointers of the archetypes that verify the query
		void ECS_VECTORCALL GetArchetypesPtrs(ArchetypeQuery query, CapacityStream<Archetype*>& archetypes) const;

		// It will fill in the indices of the archetypes that verify the query
		void ECS_VECTORCALL GetArchetypes(ArchetypeQueryExclude query, CapacityStream<unsigned int>& archetypes) const;

		// It will fill in the pointers of the archetypes that verify the query
		void ECS_VECTORCALL GetArchetypesPtrs(ArchetypeQueryExclude query, CapacityStream<Archetype*>& archetypes) const;

		void* GetComponent(Entity entity, Component component);

		const void* GetComponent(Entity entity, Component component) const;

		void* GetComponentWithIndex(Entity entity, unsigned char component_index);

		const void* GetComponentWithIndex(Entity entity, unsigned char component_index) const;

		// Data must have components.count pointers
		void GetComponent(Entity entity, ComponentSignature components, void** data) const;

		// Data must have components.count pointers
		void GetComponentWithIndex(Entity entity, ComponentSignature components, void** data) const;

		MemoryArena* GetComponentAllocator(Component component);

		AllocatorPolymorphic GetComponentAllocatorPolymorphic(Component component);

		MemoryArena* GetSharedComponentAllocator(Component component);

		AllocatorPolymorphic GetSharedComponentAllocatorPolymorphic(Component component);

		// Returns how many entities exist
		unsigned int GetEntityCount() const;

		// Returns the entity which is alive at the indicated stream index, or an invalid entity if it doesn't exist
		Entity GetEntityFromIndex(unsigned int stream_index) const;

		EntityInfo GetEntityInfo(Entity entity) const;

		ComponentSignature GetEntitySignature(Entity entity, Component* components) const;

		// It will point to the archetype's signature - Do not modify!!
		ComponentSignature GetEntitySignatureStable(Entity entity) const;

		SharedComponentSignature GetEntitySharedSignature(Entity entity, Component* shared, SharedInstance* instances) const;

		// It will point to the archetypes' signature (the base and the main) - Do not modify!!
		SharedComponentSignature GetEntitySharedSignatureStable(Entity entity) const;

		void GetEntityCompleteSignature(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared) const;

		// It will point to the archetypes' signature (the base and the main) - Do not modify!!
		void GetEntityCompleteSignatureStable(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared) const;

		// It will fill in the entities stream
		// Consider the other variant that only aliases the buffers so no copies are needed
		void ECS_VECTORCALL GetEntities(
			ArchetypeQuery query, 
			CapacityStream<Entity>* entities
		) const;

		// It will alias the archetype entities buffers - no need to copy
		// Make sure the values are not modified
		void ECS_VECTORCALL GetEntities(ArchetypeQuery query, CapacityStream<Entity*>* entities) const;

		// It will fill in the entities stream
		void ECS_VECTORCALL GetEntitiesExclude(
			ArchetypeQueryExclude query,
			CapacityStream<Entity>* entities
		) const;

		// It will alias the archetype entities buffers - no need to copy
		// Make sure the values are not modified
		void ECS_VECTORCALL GetEntitiesExclude(ArchetypeQueryExclude query, CapacityStream<Entity*>* entities) const;

		void* GetSharedData(Component component, SharedInstance instance);

		void* GetNamedSharedData(Component component, ResourceIdentifier identifier);

		SharedInstance GetSharedComponentInstance(Component component, const void* data) const;

		SharedInstance GetNamedSharedComponentInstance(Component component, ResourceIdentifier identifier) const;

		// These are vector components which are much faster to use than normal components
		VectorComponentSignature ECS_VECTORCALL GetArchetypeUniqueComponents(unsigned int archetype_index) const;

		// These are vector components which are much faster to use than normal components
		VectorComponentSignature ECS_VECTORCALL GetArchetypeSharedComponents(unsigned int archetype_index) const;

		// These are vector components which are much faster to use than normal components
		VectorComponentSignature* GetArchetypeUniqueComponentsPtr(unsigned int archetype_index);

		// These are vector components which are much faster to use than normal components
		VectorComponentSignature* GetArchetypeSharedComponentsPtr(unsigned int archetype_index);

		// Returns -1 if the entity doesn't have a parent (it is a root or doesn't exist)
		Entity GetEntityParent(Entity child) const;

		// Aliases the internal structures. Do not modify, readonly. If the static_storage is provided
		// then if the pointer is to a static storage, it will copy to it instead (useful if getting
		// the children and then doing operations like adding/removing entities from hierarchy)
		// If the parent does not exist in the hierarchy, { nullptr, 0 } is returned
		Stream<Entity> GetHierarchyChildren(Entity parent, Entity* static_storage = nullptr) const;

		// It will copy the children into the stream. If the parent does not exist, nothing will be copied
		void GetHierarchyChildrenCopy(Entity parent, CapacityStream<Entity>& children) const;

		// Returns the indices of the archetypes that match the given query
		Stream<unsigned int> GetQueryResults(unsigned int handle) const;

		// It will return the components of that query
		ArchetypeQuery ECS_VECTORCALL GetQueryComponents(unsigned int handle) const;

		// It does both at the same time to avoid some checks
		void GetQueryResultsAndComponents(unsigned int handle, Stream<unsigned int>& results, ArchetypeQuery& query) const;

		// Tag should only be the bit index, not the actual value
		bool HasEntityTag(Entity entity, unsigned char tag) const;

		bool HasComponent(Entity entity, Component component) const;

		bool HasComponents(Entity entity, VectorComponentSignature components) const;

		bool HasSharedComponent(Entity entity, Component component) const;

		bool HasSharedComponents(Entity entity, VectorComponentSignature components) const;

		bool HasSharedInstance(Entity entity, Component component, SharedInstance shared_instance) const;

		bool HasSharedInstances(Entity entity, VectorComponentSignature components, const SharedInstance* instances, unsigned char component_count) const;

		// ---------------------------------------------------------------------------------------------------

		void RemoveEntityFromHierarchyCommit(Stream<Entity> entities, bool default_child_destroy = true);

		// If the destroy children flag is set to false, it will flip the behaviour of the state stored in the hierarchy
		// Such that you can choose which behaviour to use
		void RemoveEntityFromHierarchy(
			Stream<Entity> entities,
			DeferredActionParameters parameters = {},
			bool default_child_destroy = true,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		void RemoveComponentCommit(Entity entity, ComponentSignature components);

		void RemoveComponentCommit(Stream<Entity> entities, ComponentSignature components);

		// Deferred call
		void RemoveType(Entity entity, ComponentSignature components, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = { ECS_LOCATION });

		// Deferred call
		void RemoveType(Stream<Entity> entities, ComponentSignature components, DeferredActionParameters parameters = {}, DebugInfo debug_info = { ECS_LOCATION });	

		// ---------------------------------------------------------------------------------------------------

		void RemoveSharedComponentCommit(Entity entity, ComponentSignature components);

		void RemoveSharedComponentCommit(Stream<Entity> entity, ComponentSignature components);

		void RemoveSharedComponent(Entity entity, ComponentSignature components, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = { ECS_LOCATION });

		void RemoveSharedComponent(Stream<Entity> entities, ComponentSignature components, DeferredActionParameters parameters = {}, DebugInfo debug_info = { ECS_LOCATION });

		// ---------------------------------------------------------------------------------------------------

		// Each component can have an allocator that can be used to keep the data together
		// If the size is 0, then no allocator is reserved. This is helpful also for in editor
		// use because all the allocations will happen from this allocator. If the allocator is needed,
		// alongside it the buffer offsets must be provided such that destroying an entity its buffers be freed.
		// You can specify the buffer being a data pointer in order to save memory or a normal stream (in that case
		// it will treat the next 4 bytes after the pointer to be a size). Both sizes, that for a stream or data pointer,
		// need to be expressed in bytes. Provide accessors for the component in order to make usage easier. This last point
		// about size is only needed if you intend on using the CopyEntities function. Outside of that the runtime doesn't depend on it
		void RegisterComponentCommit(Component component, unsigned int size, size_t allocator_size = 0, Stream<ComponentBuffer> component_buffers = { nullptr, 0 });

		// Deferred call
		// Each component can have an allocator that can be used to keep the data together
		// If the size is 0, then no allocator is reserved. This is helpful also for in editor
		// use because all the allocations will happen from this allocator. If the allocator is needed,
		// alongside it the buffer offsets must be provided such that destroying an entity its buffers be freed.
		// You can specify the buffer being a data pointer in order to save memory or a normal stream (in that case
		// it will treat the next 4 bytes after the pointer to be a size). Both sizes, that for a stream or data pointer,
		// need to be expressed in bytes. Provide accessors for the component in order to make usage easier. This last point
		// about size is only needed if you intend on using the CopyEntities function. Outside of that the runtime doesn't depend on it
		void RegisterComponent(
			Component component, 
			unsigned int size,
			size_t allocator_size = 0,
			Stream<ComponentBuffer> buffer_offsets = { nullptr, 0 },
			EntityManagerCommandStream* command_stream = nullptr, 
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		// Each component can have an allocator that can be used to keep the data together
		// If the size is 0, then no allocator is reserved. This is helpful also for in editor
		// use because all the allocations will happen from this allocator. If the allocator is needed,
		// alongside it the buffer offsets must be provided such that destroying an entity its buffers be freed
		void RegisterSharedComponentCommit(Component component, unsigned int size, size_t allocator_size = 0, Stream<ComponentBuffer> buffer_offset = { nullptr, 0 });

		// Deferred call
		// Each component can have an allocator that can be used to keep the data together
		// If the size is 0, then no allocator is reserved. This is helpful also for in editor
		// use because all the allocations will happen from this allocator. If the allocator is needed,
		// alongside it the buffer offsets must be provided such that destroying an entity its buffers be freed
		void RegisterSharedComponent(
			Component component, 
			unsigned int size,
			size_t allocator_size = 0,
			Stream<ComponentBuffer> buffer_offset = { nullptr, 0 },
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		// The value is stable.
		SharedInstance RegisterSharedInstanceCommit(Component component, const void* data, bool copy_buffers = true);

		// The value is stable
		// Deferred call
		void RegisterSharedInstance(
			Component component, 
			const void* data, 
			bool copy_buffers = true,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		// The value is stable
		SharedInstance RegisterNamedSharedInstanceCommit(Component component, ResourceIdentifier identifier, const void* data, bool copy_buffers = true);
		
		// The value is stable
		// Deferred call
		void RegisterNamedSharedInstance(
			Component component, 
			ResourceIdentifier identifier, 
			const void* data, 
			bool copy_buffers = true,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		void RegisterNamedSharedInstanceCommit(Component component, ResourceIdentifier identifier, SharedInstance instance);

		// Deferred call
		void RegisterNamedSharedInstance(
			Component component,
			ResourceIdentifier identifier,
			SharedInstance instance,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = { ECS_LOCATION }
		);

		// ---------------------------------------------------------------------------------------------------

		// Returns a handle to be used to access the query results.
		unsigned int RegisterQuery(ArchetypeQuery query);

		// Returns a handle to be used to access the query results.
		unsigned int RegisterQuery(ArchetypeQueryExclude query);

		// ---------------------------------------------------------------------------------------------------

		// Atomically adds it to the pending commands stream
		void RegisterPendingCommandStream(EntityManagerCommandStream command_stream);

		// ---------------------------------------------------------------------------------------------------

		// Immediate call. It will deallocate the data used by the shared instances and reallocate
		// the data but it will not copy any old data.
		void ResizeSharedComponent(Component component, unsigned int new_size);

		// ---------------------------------------------------------------------------------------------------

		// It does not copy any data stored previously. If the new allocation size is 0,
		// then it will deallocate it and return nullptr. Else deallocates and reallocates a new
		// one and returns it
		MemoryArena* ResizeComponentAllocator(Component component, size_t new_allocation_size);

		// It does not copy any data stored previously. If the new allocation size is 0,
		// then it will deallocate it and return nullptr. Else deallocates and reallocates a new
		// one and returns it
		MemoryArena* ResizeSharedComponentAllocator(Component component, size_t new_allocation_size);

		// ---------------------------------------------------------------------------------------------------
		
		// It returns to the state where nothing is allocated. As if it is newly created
		void Reset();

		// ---------------------------------------------------------------------------------------------------

		// If the component or the instance doesn't exist, it will assert
		void SetSharedComponentData(Component component, SharedInstance instance, const void* data);

		// If the component or the instance doesn't exist, it will assert
		void SetNamedSharedComponentData(Component component, ResourceIdentifier identifier, const void* data);

		// ---------------------------------------------------------------------------------------------------

		// The tag should only be the bit index, not the actual value
		void SetEntityTagCommit(Stream<Entity> entities, unsigned char tag);

		// Deferred call
		// The tag should only be the bit index, not the actual value
		void SetEntityTag(Stream<Entity> entities, unsigned char tag, DeferredActionParameters parameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		// If the entity doesn't have the component, it will return nullptr
		void* TryGetComponent(Entity entity, Component component);

		// If the entity doesn't have the component, it will return nullptr
		const void* TryGetComponent(Entity entity, Component component) const;

		// ---------------------------------------------------------------------------------------------------

		// Returns true if the entity was removed, else false
		bool TryRemoveEntityFromHierarchyCommit(Entity entity, bool default_child_destroy = true);

		// Returns true if all entities were removed, else false
		bool TryRemoveEntityFromHierarchyCommit(Stream<Entity> entities, bool default_child_destroy = true);

		void TryRemoveEntityFromHierarchy(Stream<Entity> entities, bool default_child_destroy, DeferredActionParameters parameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);
		
		// ---------------------------------------------------------------------------------------------------

		// Frees the slot used by that component.
		void UnregisterComponentCommit(Component component);

		// Deferred call
		// Frees the slot used by that component.
		void UnregisterComponent(Component component, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = { ECS_LOCATION });

		// ---------------------------------------------------------------------------------------------------

		// Frees the slot used by that component.
		void UnregisterSharedComponentCommit(Component component);

		// Deferred call
		// Frees the slot used by that component.
		void UnregisterSharedComponent(Component component, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = { ECS_LOCATION });

		// ---------------------------------------------------------------------------------------------------

		MemoryManager m_small_memory_manager;
		
		// Alongside this vector, the vector components will be kept in sync to allow
		// Fast iteration of the components
		ResizableStream<Archetype> m_archetypes;
		VectorComponentSignature* m_archetype_vector_signatures;

		// Size represents the actual capacity
		Stream<ComponentInfo> m_unique_components;
		// Size represents the actual capacity
		Stream<SharedComponentInfo> m_shared_components;

		AtomicStream<DeferredAction> m_deferred_actions;
		AtomicStream<EntityManagerCommandStream> m_pending_command_streams;
		ArchetypeQueryCache* m_query_cache;

		MemoryManager m_hierarchy_allocator;
		EntityHierarchy m_hierarchy;

		MemoryManager* m_memory_manager;
		EntityPool* m_entity_pool;
		ResizableLinearAllocator m_temporary_allocator;
	};

	// Creates the allocator for the entity pool, the entity pool itself, the entity manager allocator,
	// and the entity manager itself
	ECSENGINE_API EntityManager CreateEntityManagerWithPool(
		size_t allocator_size, 
		size_t allocator_pool_count,
		size_t allocator_new_size, 
		unsigned int entity_pool_power_of_two,
		GlobalMemoryManager* global_memory_manager
	);

}

