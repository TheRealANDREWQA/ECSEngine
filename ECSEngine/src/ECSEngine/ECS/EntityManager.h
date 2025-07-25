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

	/* 
		Note: Global components should have a type allocator, from which all descending allocations
		are made from. This type allocator should be created from the entity manager main allocator,
		since some functions rely on this behavior.
	*/

	/*typedef void (*EntityManagerEventCallback)(EntityManager*, void*, ECS_ENTITY_MANAGER_EVENT_TYPE);

	struct EntityManagerEvent {
		EntityManagerEventCallback callback;
		void* data;
		size_t data_size;
		ComponentSignature unique_signature;
		SharedComponentSignature shared_signature;
	};

	enum ECS_ENTITY_MANAGER_EVENT_TYPE {
		ECS_ENTITY_MANAGER_EVENT_ENTITIES_CREATE = 1 << 0,
		ECS_ENTITY_MANAGER_EVENT_ENTITIES_DELETE = 1 << 1,
		ECS_ENTITY_MANAGER_EVENT_ENTITIES_ADD_COMPONENT = 1 << 2,
		ECS_ENTITY_MANAGER_EVENT_ENTITIES_REMOVE_COMPONENT = 1 << 3,
		ECS_ENTITY_MANAGER_EVENT_ENTITIES_ADD_SHARED_COMPONENT = 1 << 4,
		ECS_ENTITY_MANAGER_EVENT_ENTITIES_REMOVE_SHARED_COMPONENT = 1 << 5,
		ECS_ENTITY_MANAGER_EVENT_ENTITIES_COPY = 1 << 6,
		ECS_ENTITY_MANAGER_EVENT_ARCHETYPE_CREATE = 1 << 7,
		ECS_ENTITY_MANAGER_EVENT_ARCHETYPE_DELETE = 1 << 8,
		ECS_ENTITY_MANAGER_EVENT_ARCHETYPE_BASE_CREATE = 1 << 9,
		ECS_ENTITY_MANAGER_EVENT_ARCHETYPE_BASE_DELETE = 1 << 10,
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

	struct ChangeSharedComponentElement {
		Entity entity;
		Component component;
		SharedInstance new_instance;
	};

	struct EntitySharedGroup {
		unsigned int offset;
		unsigned int size;
		SharedInstance instance;
	};

	struct EntityManagerAutoGenerateComponentFunctionsFunctorData {
		AllocatorPolymorphic temporary_allocator;
		Component component;
		Stream<char> component_name;
		ECS_COMPONENT_TYPE component_type;
		void* user_data;

		// This should be set when the component type is SHARED
		SharedComponentCompareEntry* compare_entry;
	};

	// This structure encompasses all the necessary information needed for the EntityManager::PerformEntityComponentOperations call
	struct PerformEntityComponentOperationsData {
		// --------------------------- Mandatory ------------------------------------
		ComponentSignature added_unique_components;
		ComponentSignature removed_unique_components;
		// This field can contain both shared components that should be added, but
		// Also those whose shared instance should be updated
		SharedComponentSignature added_or_updated_shared_components;
		ComponentSignature removed_shared_components;


		// --------------------------- Optional -------------------------------------
		// Should be parallel to added_unique_components and contain an identical count of entries as `added_unique_components`
		Stream<const void*> added_unique_components_data = {};
	};

	// A functor that is called by the entity manager to generate component functions when a component that has missing
	// Component functions is registered. This improves the QoL for developing inside the Editor for Global components,
	// Since the user inside the module cannot specify auto generated component functions. For this reason, this functor
	// Is added such that the editor sets this functor and the component functions will be auto generated even when the
	// User hasn't specified the component functions
	typedef ComponentFunctions (*EntityManagerAutoGenerateComponentFunctionsFunctor)(EntityManagerAutoGenerateComponentFunctionsFunctorData* data);

	struct ECSENGINE_API EntityManager
	{
	public:
		EntityManager() {}
		EntityManager(const EntityManagerDescriptor& descriptor);

		EntityManager& operator = (const EntityManager& other);

		ECS_INLINE AllocatorPolymorphic SmallAllocator() {
			return &m_small_memory_manager;
		}

		// This is an internal allocator that is used for the component infos
		// This is not the allocator of a certain component
		ECS_INLINE AllocatorPolymorphic ComponentAllocator() {
			return &m_component_memory_manager;
		}

		ECS_INLINE AllocatorPolymorphic MainAllocator() const {
			return m_memory_manager;
		}

		// The returned temporary allocator is multithreaded
		ECS_INLINE AllocatorPolymorphic TemporaryAllocator() {
			return AllocatorPolymorphic(&m_temporary_allocator).AsMulti();
		}

		// The returned temporary allocator is single threaded
		ECS_INLINE AllocatorPolymorphic TemporaryAllocatorSingleThreaded() {
			return &m_temporary_allocator;
		}

		// Use this this if you want to pass this buffer to the deferred calls with a stable flag.
		// This avoids making unnecessary copies between buffers. Do not ask ridiculous sizes. Try for the smallest.
		// If the data size cannot be predicted and can be potentially large, precondition the data into a stack buffer
		// and then use the deferred call to copy it
		void* AllocateTemporaryBuffer(size_t size, size_t alignment = alignof(void*));

		// It will sort the entities by their base archetype and call the functor
		// With a sequence of contiguous entities that are from the same base archetype
		// You can choose between in place shuffles inside the entities, or creating a temporary
		// Array where the entries will be put
		void ApplySortingEntityFunctor(
			Stream<Entity> entities, 
			void (*functor)(EntityManager* entity_manager, Stream<Entity> entities, void* user_data), 
			void* user_data,
			bool same_array_shuffle
		);

		// It will sort the entities by their base archetype and call the functor
		// With a sequence of contiguous entities that are from the same base archetype
		// It will shuffle the elements in place inside the entities stream
		template<typename Functor>
		void ApplySortingEntityFunctor(Stream<Entity> entities, bool same_array_shuffle, Functor functor) {
			auto wrapper = [](EntityManager* entity_manager, Stream<Entity> entities, void* user_data) {
				Functor* functor = (Functor*)user_data;
				(*functor)(entity_manager, entities);
			};
			ApplySortingEntityFunctor(entities, wrapper, &functor, same_array_shuffle);
		}

		// ---------------------------------------------------------------------------------------------------

		void AddComponentCommit(Entity entity, Component component);

		// Deferred Call
		void AddComponent(Entity entity, Component component, DeferredActionParameters parameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		void AddComponentCommit(Entity entity, Component component, const void* data);

		// Deferred Call
		void AddComponent(
			Entity entity,
			Component component,
			const void* data,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void AddComponentsCommit(Entity entity, ComponentSignature components);

		// Deferred Call - it will register it inside the command stream
		void AddComponents(Entity entity, ComponentSignature components, DeferredActionParameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		void AddComponentsCommit(Entity entity, ComponentSignature components, const void** data);

		// Deferred Call - it will register it inside the command stream
		void AddComponents(
			Entity entity,
			ComponentSignature components,
			const void** data,
			DeferredActionParameters = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void AddComponentCommit(Stream<Entity> entities, Component component, bool entities_belong_to_same_base_archetype);

		// Deferred Call - it will register it inside a command stream
		void AddComponent(
			Stream<Entity> entities,
			Component component,
			bool entities_belong_to_same_base_archetype,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		// Data will be used to initialize all components to the same value
		void AddComponentCommit(Stream<Entity> entities, Component component, const void* data, bool entities_belong_to_same_base_archetype);

		// Deferred Call - it will register it inside a command stream
		// Data will be used to initialize all components to the same value
		void AddComponent(
			Stream<Entity> entities, 
			Component component, 
			const void* data, 
			bool entities_belong_to_same_base_archetype, 
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void AddComponentsCommit(Stream<Entity> entities, ComponentSignature components, bool entities_belong_to_same_base_archetype);

		// Deferred Call - it will register it inside a command stream
		void AddComponents(
			Stream<Entity> entities, 
			ComponentSignature components, 
			bool entities_belong_to_same_base_archetype,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void AddComponentsCommit(
			Stream<Entity> entities, 
			ComponentSignature components, 
			const void** data, 
			EntityManagerCopyEntityDataType copy_type,
			bool entities_belong_to_same_base_archetype
		);

		// Deferred Call - it will register it inside a command stream
		// data -> A B C ; each entity will have as components A, B, C
		// components.count pointers must be present in data
		void AddComponents(
			Stream<Entity> entities,
			ComponentSignature components,
			const void** data,
			EntityManagerCopyEntityDataType copy_type,
			bool entities_belong_to_same_base_archetype,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void AddSharedComponentCommit(Entity entity, Component shared_component, SharedInstance instance);

		// Deferred Call
		void AddSharedComponent(
			Entity entity,
			Component shared_component,
			SharedInstance instance,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void AddSharedComponentCommit(Stream<Entity> entities, Component shared_component, SharedInstance instance, bool entities_belong_to_same_base_archetype);

		// Deferred Call
		void AddSharedComponent(
			Stream<Entity> entities,
			Component shared_component,
			SharedInstance instance,
			bool entities_belong_to_same_base_archetype,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void AddSharedComponentsCommit(Entity entity, SharedComponentSignature components);

		// Deferred Call
		void AddSharedComponents(
			Entity entity,
			SharedComponentSignature components,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void AddSharedComponentsCommit(Stream<Entity> entities, SharedComponentSignature components, bool entities_belong_to_same_base_archetype);

		// Deferred Call
		void AddSharedComponents(
			Stream<Entity> entities,
			SharedComponentSignature components,
			bool entities_belong_to_same_base_archetype,
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

		// Single-threaded. Adds all the entities from the other entity manager into this one
		// The unique/shared/global components that do not exist, are registered as well
		// You can optionally give an array of entities to be filled in with the newly created entities
		void AddEntityManager(const EntityManager* other, CapacityStream<Entity>* created_entities = nullptr);

		// ---------------------------------------------------------------------------------------------------

		// Deallocates any buffers used inside the entity for unique components
		// If the component doesn't have buffers, it will crash
		void CallEntityComponentDeallocateCommit(Entity entity, Component component);

		// If the given component doesn't have buffers, it will do nothing
		// Else same behaviour as the other deallocate function
		void TryCallEntityComponentDeallocateCommit(Entity entity, Component component);

		// Deallocates any buffers used inside the shared instance. Valid only for shared components
		// If there is no deallocate function, it will probably crash
		void CallComponentSharedInstanceDeallocateCommit(Component component, SharedInstance instance);

		// Deallocates any buffers used inside the shared instance. Valid only for shared components
		// If there are no buffers, it will do nothing.
		void TryCallComponentSharedInstanceDeallocateCommit(Component component, SharedInstance instance);

		// ---------------------------------------------------------------------------------------------------

		// Mostly for editor purposes, like when a component ID is changed (this function is given
		// because the vector signature needs to be changed besides the ComponentSignature)
		// It changes the registration of the component as well
		void ChangeComponentIndex(Component old_component, Component new_component);

		// Mostly for editor purposes, like when a component ID is changed (this function is given
		// because the vector signature needs to be changed besides the ComponentSignature)
		// It changes the registration of the component as well
		void ChangeSharedComponentIndex(Component old_component, Component new_component);

		// Mostly for editor purposes, like when a component ID is changed
		void ChangeGlobalComponentIndex(Component old_component, Component new_component);

		// ---------------------------------------------------------------------------------------------------

		// Deferred call
		// Entities can belong to different archetypes, the same is valid for components and instances
		// Can optionally specify if it should destroy the base archetype if it becomes empty
		// Or to not crash in case it is the same instance
		void ChangeEntitySharedInstance(
			Stream<ChangeSharedComponentElement> elements,
			bool possibly_the_same_instance = false,
			DeferredActionParameters parameters = {},
			bool destroy_base_archetype = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// Entities can belong to different archetypes, the same is valid for components and instances
		// Can optionally specify if it should destroy the base archetype if it becomes empty
		// Or to not crash in case it is the same instance
		void ChangeEntitySharedInstanceCommit(
			Stream<ChangeSharedComponentElement> elements, 
			bool possibly_the_same_instance = false, 
			bool destroy_base_archetype = false
		);

		// When the entity already has the shared component but the instance needs to be changed.
		// Can set the possibly_the_same_instance to not crash in case it is the same instance
		// It returns the old shared instance of that entity. It has no deferred version at the moment.
		// If the destroy archetypes is set, for the x component it will destroy the archetype if there are no more entities in it,
		// for the y component it will destroy the base archetype if there are no more entities in it.
		SharedInstance ChangeEntitySharedInstanceCommit(
			Entity entity, 
			Component component, 
			SharedInstance new_instance, 
			bool possibly_the_same_instance = false, 
			bool destroy_base_archetype = false
		);

		// ---------------------------------------------------------------------------------------------------

		void ChangeEntityParentCommit(Stream<EntityPair> pairs);

		void ChangeEntityParent(
			Stream<EntityPair> pairs,
			DeferredActionParameters parameters = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void ChangeComponentFunctionsCommit(Component component, const ComponentFunctions* functions);

		void ChangeSharedComponentFunctionsCommit(Component component, const ComponentFunctions* functions, SharedComponentCompareEntry compare_entry);

		// ---------------------------------------------------------------------------------------------------
	
		// The tag should be only the bit index, not the actual value
		void ClearEntityTagCommit(Stream<Entity> entities, unsigned char tag);

		// Deferred call
		// The tag should be only the bit index, not the actual value
		void ClearEntityTag(Stream<Entity> entities, unsigned char tag, DeferredActionParameters parameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		// Clears everything, it will be the same as if creating a new entity manager. If the maintain components flag is set,
		// Then the unique and shared components will be carried over to the new instance
		void ClearAll(bool maintain_components = false);

		void ClearFrame();

		void ClearCache();

		// It will copy everything. The components, the shared components, the archetypes, the entities inside the entity pool
		void CopyOther(const EntityManager* entity_manager);

		// Copies the current state of the query cache
		void CopyQueryCache(ArchetypeQueryCache* query_cache, AllocatorPolymorphic allocator);

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
			DebugInfo debug_info = ECS_DEBUG_INFO
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
			DebugInfo debug_info = ECS_DEBUG_INFO
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
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		// The values inside the entity are assumed to be initialized manually
		// It will search for an archetype that matches the given signatures
		// If it doesn't exist, it will create a new one
		Entity CreateEntityCommit(ComponentSignature unique_components, SharedComponentSignature shared_components, bool exclude_from_hierarchy = false);

		// Deferred Call
		// The values inside the entity are assumed to be initialized manually
		// It will search for an archetype that matches the given signatures
		// If it doesn't exist, it will create a new one
		void CreateEntity(
			ComponentSignature unique_components,
			SharedComponentSignature shared_components,
			bool exclude_from_hierarchy = false,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = ECS_DEBUG_INFO
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

		// The same as the other overload, but it creates specific entity values
		// It will crash if any of the provided entities already exists.
		// Returns the indices of the archetype where these entities have been added
		uint2 CreateSpecificEntitiesCommit(
			Stream<Entity> entities,
			ComponentSignature unique_components,
			SharedComponentSignature shared_components,
			bool exclude_from_hierarchy = false
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
			DebugInfo debug_info = ECS_DEBUG_INFO
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
			bool copy_buffers = true,
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
			bool copy_buffers = true,
			bool exclude_from_hierarchy = false,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		// Creates a new entity manager with a brand new entity hierarchy, entity pool and archetype query cache
		// The given entities are not necessarly identified with the same identifiers. The can be remapped
		EntityManager CreateSubset(Stream<Entity> entities, MemoryManager* memory_manager) const;

		// ---------------------------------------------------------------------------------------------------

		// Returns the byte size of a component
		unsigned int ComponentSize(Component component) const;

		// Returns the byte size of a shared component instance
		unsigned int SharedComponentSize(Component component) const;

		// Returns the byte size of the global component
		unsigned int GlobalComponentSize(Component component) const;

		// ---------------------------------------------------------------------------------------------------

		void DeleteEntityCommit(Entity entity);

		// Deferred Call - it will register it inside the command stream
		void DeleteEntity(Entity entity, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		void DeleteEntitiesCommit(Stream<Entity> entities);

		// Deferred Call 
		void DeleteEntities(Stream<Entity> entities, DeferredActionParameters parameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeCommit(unsigned int archetype_index);

		// Deferred Call
		void DestroyArchetype(unsigned int archetype_index, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeCommit(ComponentSignature unique_components, ComponentSignature shared_components);

		// Deferred Call
		void DestroyArchetype(
			ComponentSignature unique_components, 
			ComponentSignature shared_components, 
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		// Destroys all archetypes with the given indices - it takes into account that archetypes might be swapped
		// and the indices become different
		void DestroyArchetypesCommit(IteratorInterface<const unsigned int>* indices);

		// Specialization to allow for smaller sized integers. The principle is the same as the other overload.
		// Destroys all archetypes with the given indices - it takes into account that archetypes might be swapped
		// and the indices become different
		void DestroyArchetypesCommit(IteratorInterface<const unsigned short>* indices);

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeBaseCommit(unsigned int archetype_index, unsigned int archetype_subindex);

		// Deferred Call
		void DestroyArchetypeBase(
			unsigned int archetype_index,
			unsigned int archetype_subindex,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void DestroyArchetypeBaseCommit(ComponentSignature unique_components, SharedComponentSignature shared_components);

		// Deferred Call - it will register it inside the command stream
		void DestroyArchetypeBase(
			ComponentSignature unique_components, 
			SharedComponentSignature shared_components, 
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		// It will destroy all the archetypes that are empty (they don't contain any entities)
		// Single threaded
		void DestroyArchetypesEmptyCommit();

		// It will destroy all the base archetype that are empty (that don't contain any entities)
		// Single threaded
		void DestroyArchetypesBaseEmptyCommit(bool destroy_main_archetypes = false);

		// ---------------------------------------------------------------------------------------------------

		// Destroys the archetype if it is empty (it has no entities).
		void DestroyArchetypeEmptyCommit(unsigned int main_index);

		// Destroy the base archetype if it empty (it has no entities). If the main_propagation flag is true,
		// then it will try to destroy the main archetype if the base one is destroyed.
		void DestroyArchetypeBaseEmptyCommit(unsigned int main_index, unsigned int base_index, bool main_propagation = false);

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

		// Single threaded. If the archetype does not exist, then it will commit the creation of a new one
		unsigned int FindOrCreateArchetype(ComponentSignature unique_signature, ComponentSignature shared_signature);

		// Single threaded. If the archetype does not exist, then it will commit the creation of a new one
		uint2 FindOrCreateArchetypeBase(
			ComponentSignature unique_signature,
			SharedComponentSignature shared_signature,
			unsigned int starting_size = ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT
		);

		// ---------------------------------------------------------------------------------------------------

		SharedInstance FindOrCreateSharedInstanceCommit(Component component, const void* data);

		// ---------------------------------------------------------------------------------------------------

		// Return true to exit early, if desired.
		// The functor receives the component as a Component
		// This iterates over all unique components in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachComponent(Functor&& functor) const {
			size_t component_count = m_unique_components.size;
			for (size_t index = 0; index < component_count; index++) {
				Component current_component = { (short)index };
				if (ExistsComponent(current_component)) {
					if constexpr (early_exit) {
						if (functor(current_component)) {
							return true;
						}
					}
					else {
						functor(current_component);
					}
				}
			}
			return false;
		}

		// Return true to exit early, if desired.
		// The functor receives the component as a Component
		// This iterates over all shared components in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachSharedComponent(Functor&& functor) const {
			size_t component_count = m_shared_components.size;
			for (size_t index = 0; index < component_count; index++) {
				Component current_component = { (short)index };
				if (ExistsSharedComponent(current_component)) {
					if constexpr (early_exit) {
						if (functor(current_component)) {
							return true;
						}
					}
					else {
						functor(current_component);
					}
				}
			}
			return false;
		}

		// Return true to exit early, if desired.
		// The functor receives the shared instance as a SharedInstance
		// This iterates over all shared instances of a certain component in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachSharedInstance(Component component, Functor&& functor) const {
			ECS_ASSERT(ExistsSharedComponent(component));
			return m_shared_components[component.value].instances.stream.ForEachIndex<early_exit>([&](unsigned int index) {
				SharedInstance current_instance = { (short)index };
				if constexpr (early_exit) {
					return functor(current_instance);
				}
				else {
					functor(current_instance);
				}
			});
		}

		// Return true if you want to early exit. The component functor receives only the Component,
		// The functor receives the Component + SharedInstance
		template<bool early_exit = false, typename ComponentFunctor, typename Functor>
		bool ForAllSharedInstances(ComponentFunctor&& component_functor, Functor&& functor) const {
			return ForEachSharedComponent<early_exit>([&](Component component) {
				bool early_return = false;
				component_functor(component);

				ForEachSharedInstance<early_exit>(component, [&](SharedInstance instance) {
					if constexpr (early_exit) {
						early_return = functor(component, instance);
					}
					else {
						functor(component, instance);
					}
					return early_return;
					});
				return early_return;
			});
		}

		// The functor receives as parameters (void* data, Component component) 
		// Return true if you want to early exit.
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForAllGlobalComponents(Functor&& functor) const {
			unsigned int count = m_global_component_count;
			for (unsigned int index = 0; index < count; index++) {
				if constexpr (early_exit) {
					if (functor(m_global_components_data[index], m_global_components[index])) {
						return true;
					}
				}
				else {
					functor(m_global_components_data[index], m_global_components[index]);
				}
			}
			return false;
		}

		// Return true to exit early, if desired.
		// The functor receives the archetype as a Archetype*
		// This iterates over all archetypes in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachArchetype(Functor&& functor) {
			unsigned int archetype_count = GetArchetypeCount();
			for (unsigned int archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
				Archetype* archetype = GetArchetype(archetype_index);
				if constexpr (early_exit) {
					if (functor(archetype)) {
						return true;
					}
				}
				else {
					functor(archetype);
				}
			}
			return false;
		}

		// CONST VARIANT
		// Return true to exit early, if desired.
		// The functor receives the archetype as a const Archetype*
		// This iterates over all archetypes in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachArchetype(Functor&& functor) const {
			unsigned int archetype_count = GetArchetypeCount();
			for (unsigned int archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
				const Archetype* archetype = GetArchetype(archetype_index);
				if constexpr (early_exit) {
					if (functor(archetype)) {
						return true;
					}
				}
				else {
					functor(archetype);
				}
			}
			return false;
		}

		// Return true to exit early, if desired.
		// The functor receives the main archetype as Archetype*
		// This iterates over all archetypes that match a certain signature in the entity manager
		// Does not make use of the query cache
		template<bool early_exit = false, typename Functor>
		bool ECS_VECTORCALL ForEachArchetype(ArchetypeQuery query, Functor&& functor) {
			unsigned int archetype_count = GetArchetypeCount();
			for (unsigned int archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
				VectorComponentSignature unique_signature = GetArchetypeUniqueComponents(archetype_index);
				VectorComponentSignature shared_signature = GetArchetypeSharedComponents(archetype_index);
				if (query.Verifies(unique_signature, shared_signature)) {
					Archetype* archetype = GetArchetype(archetype_index);
					if constexpr (early_exit) {
						if (functor(archetype)) {
							return true;
						}
					}
					else {
						functor(archetype);
					}
				}
			}
			return false;
		}

		// CONST VARIANT
		// Return true to exit early, if desired.
		// The functor receives the main archetype as const Archetype*
		// This iterates over all archetypes that match a certain signature in the entity manager
		// Does not make use of the query cache
		template<bool early_exit = false, typename Functor>
		bool ECS_VECTORCALL ForEachArchetype(ArchetypeQuery query, Functor&& functor) const {
			unsigned int archetype_count = GetArchetypeCount();
			for (unsigned int archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
				VectorComponentSignature unique_signature = GetArchetypeUniqueComponents(archetype_index);
				VectorComponentSignature shared_signature = GetArchetypeSharedComponents(archetype_index);
				if (query.Verifies(unique_signature, shared_signature)) {
					const Archetype* archetype = GetArchetype(archetype_index);
					if constexpr (early_exit) {
						if (functor(archetype)) {
							return true;
						}
					}
					else {
						functor(archetype);
					}
				}
			}
			return false;
		}

		// Return true to exit early, if desired.
		// The functor receives (Archetype*, unsigned int base_index)
		// This iterates over all base archetypes in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachBaseArchetype(Functor&& functor) {
			unsigned int archetype_count = GetArchetypeCount();
			for (unsigned int archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
				Archetype* archetype = GetArchetype(archetype_index);
				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					if constexpr (early_exit) {
						if (functor(archetype, base_index)) {
							return true;
						}
					}
					else {
						functor(archetype, base_index);
					}
				}
			}
			return false;
		}

		// CONST VARIANT
		// Return true to exit early, if desired.
		// The functor receives (const Archetype*, unsigned int base_index)
		// This iterates over all base archetypes in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachBaseArchetype(Functor&& functor) const {
			unsigned int archetype_count = GetArchetypeCount();
			for (unsigned int archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
				const Archetype* archetype = GetArchetype(archetype_index);
				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					if constexpr (early_exit) {
						if (functor(archetype, base_index)) {
							return true;
						}
					}
					else {
						functor(archetype, base_index);
					}
				}
			}
			return false;
		}

		// Return true to exit early, if desired.
		// The functor receives the main archetype as Archetype*, the base archetype as ArchetypeBase*, the entity as Entity, void** unique_components
		// The ArchetypeInitializeFunctor receives an Archetype*, the ArchetypeBaseInitializeFunctor receives an (Archetype* archetype, unsigned int base_index)
		// This iterates over all entities in the entity manager.
		template<bool early_exit = false, typename ArchetypeInitializeFunctor, typename ArchetypeBaseInitializeFunctor, typename Functor>
		bool ForEachEntity(ArchetypeInitializeFunctor&& archetype_initialize, ArchetypeBaseInitializeFunctor&& base_initialize, Functor&& functor) {
			unsigned int archetype_count = GetArchetypeCount();
			for (unsigned int archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
				Archetype* archetype = GetArchetype(archetype_index);
				archetype_initialize(archetype);

				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					ArchetypeBase* base_archetype = archetype->GetBase(base_index);
					base_initialize(archetype, base_index);

					unsigned int entity_count = base_archetype->EntityCount();

					void* entity_components[ECS_ARCHETYPE_MAX_COMPONENTS];
					unsigned int component_sizes[ECS_ARCHETYPE_MAX_COMPONENTS];

					ComponentSignature unique_signature = archetype->GetUniqueSignature();
					unsigned char unique_count = unique_signature.count;

					memcpy(entity_components, base_archetype->m_buffers, sizeof(*base_archetype->m_buffers) * unique_count);
					for (unsigned char component_index = 0; component_index < unique_count; component_index++) {
						component_sizes[component_index] = ComponentSize(unique_signature[component_index]);
					}

					for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
						if constexpr (early_exit) {
							if (functor(archetype, base_archetype, base_archetype->m_entities[entity_index], entity_components)) {
								return true;
							}
						}
						else {
							functor(archetype, base_archetype, base_archetype->m_entities[entity_index], entity_components);
						}

						for (unsigned char component_index = 0; component_index < unique_count; component_index++) {
							entity_components[component_index] = OffsetPointer(entity_components[component_index], component_sizes[component_index]);
						}
					}
				}
			}
			return false;
		}

		// CONST VARIANT
		// Return true to exit early, if desired.
		// The functor receives the main archetype as const Archetype*, the base archetype as const ArchetypeBase*, the entity as Entity, void** unique_components
		// The ArchetypeInitializeFunctor receives an const Archetype*, the ArchetypeBaseInitializeFunctor receives a (const Archetype* archetype, unsigned int base_index)
		// This iterates over all entities in the entity manager.
		template<bool early_exit = false, typename ArchetypeInitializeFunctor, typename ArchetypeBaseInitializeFunctor, typename Functor>
		bool ForEachEntity(ArchetypeInitializeFunctor&& archetype_initialize, ArchetypeBaseInitializeFunctor&& base_initialize, Functor&& functor) const {
			unsigned int archetype_count = GetArchetypeCount();
			for (unsigned int archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
				const Archetype* archetype = GetArchetype(archetype_index);
				archetype_initialize(archetype);

				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					const ArchetypeBase* base_archetype = archetype->GetBase(base_index);
					base_initialize(archetype, base_index);

					unsigned int entity_count = base_archetype->EntityCount();

					void* entity_components[ECS_ARCHETYPE_MAX_COMPONENTS];
					unsigned int component_sizes[ECS_ARCHETYPE_MAX_COMPONENTS];

					ComponentSignature unique_signature = archetype->GetUniqueSignature();
					unsigned char unique_count = unique_signature.count;

					memcpy(entity_components, base_archetype->m_buffers, sizeof(*base_archetype->m_buffers) * unique_count);
					for (unsigned char component_index = 0; component_index < unique_count; component_index++) {
						component_sizes[component_index] = ComponentSize(unique_signature[component_index]);
					}

					for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
						if constexpr (early_exit) {
							if (functor(archetype, base_archetype, base_archetype->m_entities[entity_index], entity_components)) {
								return true;
							}
						}
						else {
							functor(archetype, base_archetype, base_archetype->m_entities[entity_index], entity_components);
						}

						for (unsigned char component_index = 0; component_index < unique_count; component_index++) {
							entity_components[component_index] = OffsetPointer(entity_components[component_index], component_sizes[component_index]);
						}
					}
				}
			}
			return false;
		}

		// Return true to exit early, if desired.
		// The functor receives the main archetype as Archetype*, the base archetype as ArchetypeBase*, the entity as Entity, void** unique_components
		// This iterates over all entities in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachEntity(Functor&& functor) {
			return ForEachEntity<early_exit>([](Archetype* archetype) {}, [](Archetype* archetype, unsigned int base_index) {}, functor);
		}

		// CONST VARIANT
		// Return true to exit early, if desired.
		// The functor receives the main archetype as const Archetype*, the base archetype as const ArchetypeBase*, the entity as Entity, void** unique_components
		// This iterates over all entities in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachEntity(Functor&& functor) const {
			return ForEachEntity<early_exit>([](const Archetype* archetype) {}, [](const Archetype* archetype, unsigned int base_index) {}, functor);
		}

		// Return true in the functor to exit early, if desired.
		// Returns true if it early exited, else false.
		// The functor receives (Entity entity) as arguments
		// This iterates over all entities in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachEntitySimple(Functor&& functor) const {
			unsigned int archetype_count = GetArchetypeCount();
			for (unsigned int archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
				const Archetype* archetype = GetArchetype(archetype_index);

				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					const ArchetypeBase* base_archetype = archetype->GetBase(base_index);

					unsigned int entity_count = base_archetype->EntityCount();
					for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
						if constexpr (early_exit) {
							if (functor(base_archetype->m_entities[entity_index])) {
								return true;
							}
						}
						else {
							functor(base_archetype->m_entities[entity_index]);
						}
					}
				}
			}
			return false;
		}

		// Return true to exit early, if desired.
		// The functor receives the entity as Entity and the data as void*
		// This iterates over all unique components in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachEntityComponent(Component component, Functor&& functor) {
			ArchetypeQuery query;
			query.unique.ConvertComponents({ &component, 1 });
			return ForEachArchetype<early_exit>(query, [&](Archetype* archetype) {
				unsigned int base_count = archetype->GetBaseCount();
				unsigned char component_index = archetype->FindUniqueComponentIndex(component);
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					ArchetypeBase* archetype_base = archetype->GetBase(base_index);
					unsigned int entity_count = archetype_base->EntityCount();
					for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
						Entity entity = archetype_base->GetEntityAtIndex(entity_index);
						void* component_data = archetype_base->GetComponentByIndex(entity_index, component_index);
						if constexpr (early_exit) {
							return functor(entity, component_data);
						}
						else {
							functor(entity, component_data);
						}
					}
				}
			});
		}

		// CONST VARIANT
		// Return true to exit early, if desired.
		// The functor receives the entity as Entity and the data as const void*
		// This iterates over all unique components in the entity manager
		template<bool early_exit = false, typename Functor>
		bool ForEachEntityComponent(Component component, Functor&& functor) const {
			ArchetypeQuery query;
			query.unique.ConvertComponents({ &component, 1 });
			return ForEachArchetype<early_exit>(query, [&](const Archetype* archetype) {
				unsigned int base_count = archetype->GetBaseCount();
				unsigned char component_index = archetype->FindUniqueComponentIndex(component);
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					const ArchetypeBase* archetype_base = archetype->GetBase(base_index);
					unsigned int entity_count = archetype_base->EntityCount();
					for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
						Entity entity = archetype_base->GetEntityAtIndex(entity_index);
						const void* component_data = archetype_base->GetComponentByIndex(entity_index, component_index);
						if constexpr (early_exit) {
							return functor(entity, component_data);
						}
						else {
							functor(entity, component_data);
						}
					}
				}
			});
		}

		// CONST VARIANT
		// Return true to exit early, if desired.
		// The functor receives as parameters (Entity entity, const void** unique_components, const void** shared_components)
		// This iterates over all entities that satisfy the given query
		// This variant uses a query to determine the archetypes to apply the functor on
		// The variant in ForEach.h uses template arguments to deduce the Query and can early exit
		// Compared to the untyped functor variant
		template<bool early_exit = false, typename Functor>
		bool ForEachEntityWithSignature(ArchetypeQuery query, Functor&& functor) const {
			return ForEachArchetype<early_exit>(query, [&](const Archetype* archetype) {
				unsigned int base_count = archetype->GetBaseCount();

				Component shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
				Component shared_components_mask[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
				ComponentSignature shared_signature = query.shared.ToNormalSignature(shared_components);
				memcpy(shared_components_mask, shared_components, sizeof(Component) * shared_signature.count);
				archetype->FindSharedComponents({ shared_components_mask, shared_signature.count });

				Component unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
				ComponentSignature unique_signature = query.unique.ToNormalSignature(unique_components);
				unsigned char archetype_unique_component_index[ECS_ARCHETYPE_MAX_COMPONENTS];
				unsigned int unique_components_byte_size[ECS_ARCHETYPE_MAX_COMPONENTS];
				for (unsigned int unique_index = 0; unique_index < unique_signature.count; unique_index++) {
					archetype_unique_component_index[unique_index] = archetype->FindUniqueComponentIndex(unique_components[unique_index]);
					unique_components_byte_size[unique_index] = ComponentSize(unique_components[unique_index]);
				}				

				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					const ArchetypeBase* base = archetype->GetBase(base_index);
					unsigned int entity_count = base->EntityCount();
					
					const void* shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
					const void* unique_components[ECS_ARCHETYPE_MAX_COMPONENTS];
					// Get all the shared components according to the signature
					const SharedInstance* shared_instances = archetype->GetBaseInstances(base_index);
					for (unsigned int shared_index = 0; shared_index < shared_signature.count; shared_index++) {
						shared_components[shared_index] = GetSharedData(shared_signature.indices[shared_index], shared_instances[shared_components_mask[shared_index].value]);
					}

					for (unsigned int unique_index = 0; unique_index < unique_signature.count; unique_index++) {
						unique_components[unique_index] = base->GetComponentByIndex(0, archetype_unique_component_index[unique_index]);
					}

					for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
						if constexpr (early_exit) {
							if (functor(base->GetEntityAtIndex(entity_index), unique_components, shared_components)) {
								return true;
							}
						}
						else {
							functor(base->GetEntityAtIndex(entity_index), unique_components, shared_components);
						}
						for (unsigned int unique_index = 0; unique_index < unique_signature.count; unique_index++) {
							unique_components[unique_index] = OffsetPointer(unique_components[unique_index], unique_components_byte_size[unique_index]);
						}
					}
				}

				return false;
			});
		}

		template<bool early_exit = false, typename Functor>
		bool ForEachEntityWithSharedInstance(Component component, SharedInstance instance, Functor&& functor) const {
			ArchetypeQuery query;
			query.shared.ConvertComponents({ &component, 1 });
			return ForEachArchetype<early_exit>(query, [&](const Archetype* archetype) {
				SharedComponentSignature shared_signature = { &component, &instance, 1 };
				unsigned int base_index = archetype->FindBaseIndex(shared_signature);
				if (base_index != -1) {
					const ArchetypeBase* base = archetype->GetBase(base_index);
					unsigned int entity_count = base->EntityCount();
					for (unsigned int index = 0; index < entity_count; index++) {
						if constexpr (early_exit) {
							if (functor(base->GetEntityAtIndex(index))) {
								return true;
							}
						}
						else {
							functor(base->GetEntityAtIndex(index));
						}
					}
				}
				return false;
			});
		}

		// ---------------------------------------------------------------------------------------------------

		// Executes all deferred calls, including clearing tags and setting them
		void Flush();

		// Executes all deferred calls inside a command stream
		void Flush(EntityManagerCommandStream command_stream);

		// Verifies if the entity is still valid. It might become invalid if another system deleted it in the meantime
		ECS_INLINE bool ExistsEntity(Entity entity) const {
			return m_entity_pool->IsValid(entity);
		}

		// Returns true if the given entity exists in the hierarchy, else false
		ECS_INLINE bool ExistsEntityInHierarchy(Entity entity) const {
			return m_hierarchy.Exists(entity);
		}

		// Verifies if a component already exists at that slot
		ECS_INLINE bool ExistsComponent(Component component) const {
			return component.value < m_unique_components.size && m_unique_components[component.value].size != -1;
		}

		// Verifies if a component already exists at that slot
		template<typename T>
		ECS_INLINE bool ExistsComponent() const {
			return ExistsComponent(T::ID());
		}

		// Verifies if a shared component was already allocated at that slot
		ECS_INLINE bool ExistsSharedComponent(Component component) const {
			return component.value < m_shared_components.size && m_shared_components[component.value].info.size != -1;
		}

		// Verifies if a shared component was already allocated at that slot
		template<typename T>
		ECS_INLINE bool ExistsSharedComponent() const {
			return ExistsSharedComponent(T::ID());
		}

		// Verifies if a global component was already allocated at that slot
		ECS_INLINE bool ExistsGlobalComponent(Component component) const {
			return SearchBytes(m_global_components, m_global_component_count, component.value) != -1;
		}

		// Verifies if a global component was already allocated at that slot
		template<typename T>
		ECS_INLINE bool ExistsGlobalComponent() const {
			return ExistsGlobalComponent(T::ID());
		}

		// Verifies if a shared instance is a valid instance - checks to see if the component also exists
		ECS_INLINE bool ExistsSharedInstance(Component component, SharedInstance instance) const {
			return ExistsSharedComponent(component) && ExistsSharedInstanceOnly(component, instance);
		}

		// Verifies if a shared instance is a valid instance - checks to see if the component also exists
		// Returns in the x component the shared component status and in the y the instance status
		ECS_INLINE bool2 ExistsSharedInstanceEx(Component component, SharedInstance instance) const {
			if (!ExistsSharedComponent(component)) {
				return { false, false };
			}

			return { true, ExistsSharedInstanceOnly(component, instance) };
		}

		// Verifies if a shared instance is a valid instance - does not check to see if the component also exist
		ECS_INLINE bool ExistsSharedInstanceOnly(Component component, SharedInstance instance) const {
			return m_shared_components[component.value].instances.stream.ExistsItem(instance.value);
		}

		ECS_INLINE unsigned int GetArchetypeCount() const {
			return m_archetypes.size;
		}

		Archetype* GetArchetype(unsigned int index);

		const Archetype* GetArchetype(unsigned int index) const;

		ArchetypeBase* GetBase(unsigned int main_index, unsigned int base_index);

		const ArchetypeBase* GetBase(unsigned int main_index, unsigned int base_index) const;

		ECS_INLINE ArchetypeBase* GetBase(EntityInfo info) {
			return GetBase(info.main_archetype, info.base_archetype);
		}

		ECS_INLINE const ArchetypeBase* GetBase(EntityInfo info) const {
			return GetBase(info.main_archetype, info.base_archetype);
		}

		// It requires a syncronization barrier!! If the archetype does not exist, then it will commit the creation of a new one
		Archetype* GetOrCreateArchetype(ComponentSignature unique_signature, ComponentSignature shared_signature);

		ArchetypeBase* GetOrCreateArchetypeBase(
			ComponentSignature unique_signature, 
			SharedComponentSignature shared_signature,
			unsigned int starting_size = ECS_ARCHETYPE_DEFAULT_BASE_RESERVE_COUNT
		);

		// It will fill in the indices of the archetypes that verify the query
		void GetArchetypes(const ArchetypeQuery& query, CapacityStream<unsigned int>& archetypes) const;

		// It will fill in the indices of the archetypes that verify the query
		void GetArchetypes(const ArchetypeQueryExclude& query, CapacityStream<unsigned int>& archetypes) const;

		// It will fill in the indices of the archetypes that verify the query
		void GetArchetypes(const ArchetypeQueryOptional& query, CapacityStream<unsigned int>& archetypes) const;

		// It will fill in the indices of the archetypes that verify the query
		void GetArchetypes(const ArchetypeQueryExcludeOptional& query, CapacityStream<unsigned int>& archetypes) const;

		// It will fill in the indices of the archetypes that verify the query (this will check and use
		// the appropriate query - it will perform the SIMD conversion as well)
		void GetArchetypes(const ArchetypeQueryDescriptor& query_descriptor, CapacityStream<unsigned int>& archetypes) const;

		void* GetComponent(Entity entity, Component component);

		const void* GetComponent(Entity entity, Component component) const;

		// This overload is faster than the entity overload, if the entity info is already known
		void* GetComponent(EntityInfo info, Component component);

		// This overload is faster than the entity overload, if the entity info is already known
		const void* GetComponent(EntityInfo info, Component component) const;

		// Component index should be the index of the component in the archetype signature
		void* GetComponentWithIndex(Entity entity, unsigned char component_index);
		
		// Component index should be the index of the component in the archetype signature
		const void* GetComponentWithIndex(Entity entity, unsigned char component_index) const;

		// Component index should be the index of the component in the archetype signature
		// This overload is faster than the entity overload, if the entity info is already known
		void* GetComponentWithIndex(EntityInfo info, unsigned char component_index);

		// Component index should be the index of the component in the archetype signature
		// This overload is faster than the entity overload, if the entity info is already known
		const void* GetComponentWithIndex(EntityInfo info, unsigned char component_index) const;

		// Data must have components.count pointers
		void GetComponent(Entity entity, ComponentSignature components, void** data) const;

		// Data must have components.count pointers
		void GetComponentWithIndex(Entity entity, ComponentSignature components, void** data) const;

		void* GetSharedComponent(Entity entity, Component component);

		const void* GetSharedComponent(Entity entity, Component component) const;

		// This overload is faster than the entity overload, if the entity info is already known
		void* GetSharedComponent(EntityInfo info, Component component);

		// This overload is faster than the entity overload, if the entity info is already known
		const void* GetSharedComponent(EntityInfo info, Component component) const;

		// Works for both unique and shared
		template<typename T>
		ECS_INLINE T* GetComponent(Entity entity) {
			return (T*)((const EntityManager*)this)->GetComponent<T>(entity);
		}

		// Works for both unique and shared
		template<typename T>
		ECS_INLINE const T* GetComponent(Entity entity) const {
			if constexpr (T::IsShared()) {
				return (const T*)GetSharedComponent(entity, T::ID());
			}
			else {
				return (const T*)GetComponent(entity, T::ID());
			}
		}

		void* GetGlobalComponent(Component component);

		const void* GetGlobalComponent(Component component) const;

		template<typename T>
		ECS_INLINE T* GetGlobalComponent() {
			return (T*)GetGlobalComponent(T::ID());
		}

		template<typename T>
		ECS_INLINE const T* GetGlobalComponent() const {
			return (const T*)GetGlobalComponent(T::ID());
		}

		AllocatorPolymorphic GetComponentAllocator(Component component) const;

		AllocatorPolymorphic GetSharedComponentAllocator(Component component) const;

		// If the global component doesn't have an allocator (because it doesn't have buffers for example),
		// It returns nullptr
		AllocatorPolymorphic GetGlobalComponentAllocator(Component component) const;

		AllocatorPolymorphic GetComponentAllocatorFromType(Component component, ECS_COMPONENT_TYPE type) const;

		// Returns how many entities exist
		ECS_INLINE unsigned int GetEntityCount() const {
			return m_entity_pool->GetCount();
		}

		// Returns the number of entities that have this component
		unsigned int GetEntityCountForComponent(Component component) const;

		// Returns the number of entities that have this shared component
		unsigned int GetEntityCountForSharedComponent(Component component) const;

		// Returns the entity which is alive at the indicated stream index, or an invalid entity if it doesn't exist
		Entity GetEntityFromIndex(unsigned int stream_index) const;

		// Returns the entity which corresponds to the entity info (if the info is not valid to begin with, then
		// The returned value will not be valid as well)
		ECS_INLINE Entity GetEntityFromInfo(EntityInfo info) const {
			return GetEntityFromIndex(info.stream_index);
		}

		// Returns the entity info for an entity that is valid, it will ensure that the entity is indeed valid
		ECS_INLINE EntityInfo GetEntityInfo(Entity entity) const {
			return m_entity_pool->GetInfo(entity);
		}

		ComponentSignature GetEntitySignature(Entity entity, Component* components) const;

		// This overload is faster than the entity overload, in case this information is readily available
		ComponentSignature GetEntitySignature(EntityInfo info, Component* components) const;

		// It will point to the archetype's signature - Do not modify!!
		ComponentSignature GetEntitySignatureStable(Entity entity) const;

		// This overload is faster than the entity overload, in case this information is readily available
		// It will point to the archetype's signature - Do not modify!!
		ComponentSignature GetEntitySignatureStable(EntityInfo info) const;

		SharedComponentSignature GetEntitySharedSignature(Entity entity, Component* shared, SharedInstance* instances) const;
		
		// This overload is faster than the entity overload, in case this information is readily available
		SharedComponentSignature GetEntitySharedSignature(EntityInfo info, Component* shared, SharedInstance* instances) const;

		// It will point to the archetypes' signature (the base and the main) - Do not modify!!
		SharedComponentSignature GetEntitySharedSignatureStable(Entity entity) const;

		// This overload is faster than the entity overload, in case this information is readily available
		// It will point to the archetypes' signature (the base and the main) - Do not modify!!
		SharedComponentSignature GetEntitySharedSignatureStable(EntityInfo info) const;

		void GetEntityCompleteSignature(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared) const;

		// This overload is faster than the entity overload, in case this information is readily available
		void GetEntityCompleteSignature(EntityInfo info, ComponentSignature* unique, SharedComponentSignature* shared) const;

		// It will point to the archetypes' signature (the base and the main) - Do not modify!!
		void GetEntityCompleteSignatureStable(Entity entity, ComponentSignature* unique, SharedComponentSignature* shared) const;
		
		// This overload is faster than the entity overload, in case this information is readily available
		// It will point to the archetypes' signature (the base and the main) - Do not modify!!
		void GetEntityCompleteSignatureStable(EntityInfo info, ComponentSignature* unique, SharedComponentSignature* shared) const;

		// Returns a stringified version of the component. If the name was not specified when inserting the component
		// then it will just use the index as string
		Stream<char> GetComponentName(Component component) const;

		// Returns a stringified version of the shared component. If the name was not specified when inserting the component
		// then it will just use the index as string
		Stream<char> GetSharedComponentName(Component component) const;

		// Returns a stringified version of the shared component. If the name was not specified when inserting the component
		// then it will just use the index as string
		Stream<char> GetGlobalComponentName(Component component) const;

		// Returns a stringified concatenation of the names of the components. It will use the given storage to build the string
		Stream<char> GetComponentSignatureString(ComponentSignature signature, CapacityStream<char>& storage, Stream<char> separator = ", ") const;

		// Returns a stringified concatenation of the names of the shared components. It will use the given storage to build the string
		Stream<char> GetSharedComponentSignatureString(ComponentSignature signature, CapacityStream<char>& storage, Stream<char> separator = ", ") const;

		// Returns a stringified concatenation of the names of the components. It will use the given storage to build the string
		Stream<char> GetComponentSignatureString(VectorComponentSignature signature, CapacityStream<char>& storage, Stream<char> separator = ", ") const;
		
		// Returns a stringified concatenation of the names of the shared components. It will use the given storage to build the string
		Stream<char> GetSharedComponentSignatureString(VectorComponentSignature signature, CapacityStream<char>& storage, Stream<char> separator = ", ") const;

		// It will fill in the entities stream
		// Consider the other variant that only aliases the buffers so no copies are needed
		void ECS_VECTORCALL GetEntities(
			const ArchetypeQuery& query, 
			CapacityStream<Entity>* entities
		) const;

		// It will alias the archetype entities buffers - no need to copy
		// Make sure the values are not modified
		void ECS_VECTORCALL GetEntities(const ArchetypeQuery& query, CapacityStream<Entity*>* entities) const;

		// It will fill in the entities stream
		void ECS_VECTORCALL GetEntitiesExclude(
			const ArchetypeQueryExclude& query,
			CapacityStream<Entity>* entities
		) const;

		// It will alias the archetype entities buffers - no need to copy
		// Make sure the values are not modified
		void ECS_VECTORCALL GetEntitiesExclude(const ArchetypeQueryExclude& query, CapacityStream<Entity*>* entities) const;

		void* GetSharedData(Component component, SharedInstance instance);

		const void* GetSharedData(Component component, SharedInstance instance) const;

		template<typename ComponentType>
		ECS_INLINE ComponentType* GetSharedData(SharedInstance instance) {
			return (ComponentType*)GetSharedData(ComponentType::ID(), instance);
		}

		template<typename ComponentType>
		ECS_INLINE const ComponentType* GetSharedData(SharedInstance instance) const {
			return (ComponentType*)GetSharedData(ComponentType::ID(), instance);
		}

		// Returns the number of shared component named instances. It ensures that the component is valid
		unsigned int GetNamedSharedInstanceCount(Component component) const;

		void* GetNamedSharedData(Component component, Stream<char> identifier);

		const void* GetNamedSharedData(Component component, Stream<char> identifier) const;

		SharedInstance GetSharedComponentInstance(Component component, const void* data) const;

		SharedInstance GetSharedComponentInstance(Component component, Entity entity) const;

		// This overload is faster than the entity variant, if this info is readily available
		SharedInstance GetSharedComponentInstance(Component component, EntityInfo info) const;

		SharedInstance GetNamedSharedComponentInstance(Component component, Stream<char> identifier) const;

		// If there is an existing shared instance with that data, it will return that shared instance
		// Else, it create a new entry and return it. Can optionally give a boolean that will be set to true
		// if a new instance was created
		SharedInstance GetOrCreateSharedComponentInstanceCommit(Component component, const void* data, bool* created_instance = nullptr);

		// Fills in all the shared instances that are registered for that component
		void GetSharedComponentInstanceAll(Component component, CapacityStream<SharedInstance>& shared_instances) const;

		// Returns the number of instances created for the given component
		unsigned int GetSharedComponentInstanceCount(Component component) const;

		// These are vector components which are much faster to use than normal components
		VectorComponentSignature ECS_VECTORCALL GetArchetypeUniqueComponents(unsigned int archetype_index) const;

		// These are vector components which are much faster to use than normal components
		VectorComponentSignature ECS_VECTORCALL GetArchetypeSharedComponents(unsigned int archetype_index) const;

		// These are vector components which are much faster to use than normal components
		VectorComponentSignature* GetArchetypeUniqueComponentsPtr(unsigned int archetype_index);

		// These are vector components which are much faster to use than normal components
		VectorComponentSignature* GetArchetypeSharedComponentsPtr(unsigned int archetype_index);

		// Returns Entity::Invalid() if the entity doesn't have a parent (it is a root or doesn't exist)
		Entity GetEntityParent(Entity child) const;

		// Adds all the entities that reference the given shared instance
		void GetEntitiesForSharedInstance(Component component, SharedInstance instance, AdditionStream<Entity> entities) const;

		// Fills in the children of the entity
		void GetChildren(Entity parent, AdditionStream<Entity> children) const;

		// It will copy the children into the stream. If the parent does not exist, nothing will be copied
		void GetChildrenNested(Entity parent, AdditionStream<Entity> children) const;

		// Returns an iterator for the direct children of the entity
		EntityHierarchy::ChildIterator GetChildIterator(Entity parent) const;

		// Returns an iterator for all direct and indirect (children of children) of the entity
		EntityHierarchy::NestedChildIterator GetNestedChildIterator(Entity parent) const;

		// Returns the indices of the archetypes that match the given query
		Stream<unsigned int> GetQueryResults(unsigned int handle) const;

		// It will return the components of that query
		ArchetypeQuery ECS_VECTORCALL GetQueryComponents(unsigned int handle) const;

		// It does both at the same time to avoid some checks
		ArchetypeQueryResult ECS_VECTORCALL GetQueryResultsAndComponents(unsigned int handle) const;

		// Returns the index of the unique component with the maximum index
		Component GetMaxComponent() const;

		// Returns the index of the shared component with the maximum index
		Component GetMaxSharedComponent() const;

		ECS_INLINE unsigned int GetGlobalComponentCount() const {
			return m_global_component_count;
		}

		// Returns the number of entities that have this shared instance in use
		unsigned int GetNumberOfEntitiesForSharedInstance(Component component, SharedInstance instance) const;

		// Groups the given entities by their shared component, such that entities with the same instance
		// Are contiguous. The groups are written into the out stream, while also reshuffling the entities
		// Inside the given entities stream.
		// It does not accept entities that do not have the component! (it will crash)
		void GroupEntitiesBySharedInstance(Stream<Entity> entities, Component shared_component, AdditionStream<EntitySharedGroup> groups);

		// Groups the given entities by their shared component, such that entities with the same instance
		// Are contiguous. The groups are written into the out stream, while also reshuffling the entities
		// Inside the given entities stream.
		// It does accept entities that do not have the component, these entities having their own group 
		void GroupEntitiesBySharedInstanceWithMissing(Stream<Entity> entities, Component shared_component, AdditionStream<EntitySharedGroup> groups);

		// Tag should only be the bit index, not the actual value
		bool HasEntityTag(Entity entity, unsigned char tag) const;

		bool HasComponent(Entity entity, Component component) const;

		bool HasComponents(Entity entity, VectorComponentSignature components) const;

		bool HasSharedComponent(Entity entity, Component component) const;

		bool HasSharedComponents(Entity entity, VectorComponentSignature components) const;

		bool HasSharedInstance(Entity entity, Component component, SharedInstance shared_instance) const;

		bool HasSharedInstances(Entity entity, VectorComponentSignature components, const SharedInstance* instances, unsigned char component_count) const;

		// Works for both unique and shared components
		template<typename T>
		ECS_INLINE bool HasComponent(Entity entity) const {
			if constexpr (T::IsShared()) {
				return HasSharedComponent(entity, T::ID());
			}
			else {
				return HasComponent(entity, T::ID());
			}
		}

		// ---------------------------------------------------------------------------------------------------

		// Performs a bulk of operations on a single entity such that the data is transferred across archetypes
		// Only once. It is the most efficient way of adding/deleting unique/shared components at once. At the moment,
		// There is only the commit version, if the deferred version will be needed, it will be added later on.
		void PerformEntityComponentOperationsCommit(Entity entity, const PerformEntityComponentOperationsData& data);

		// ---------------------------------------------------------------------------------------------------

		void RemoveEntityFromHierarchyCommit(Stream<Entity> entities, bool default_child_destroy = true);

		// If the destroy children flag is set to false, it will flip the behaviour of the state stored in the hierarchy
		// Such that you can choose which behaviour to use
		void RemoveEntityFromHierarchy(
			Stream<Entity> entities,
			DeferredActionParameters parameters = {},
			bool default_child_destroy = true,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void RemoveComponentCommit(Entity entity, ComponentSignature components);

		void RemoveComponentCommit(Stream<Entity> entities, ComponentSignature components);

		// Deferred call
		void RemoveComponent(Entity entity, ComponentSignature components, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Deferred call
		void RemoveComponent(Stream<Entity> entities, ComponentSignature components, DeferredActionParameters parameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);	

		// ---------------------------------------------------------------------------------------------------

		void RemoveSharedComponentCommit(Entity entity, ComponentSignature components);

		void RemoveSharedComponentCommit(Stream<Entity> entity, ComponentSignature components);

		void RemoveSharedComponent(Entity entity, ComponentSignature components, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = ECS_DEBUG_INFO);

		void RemoveSharedComponent(Stream<Entity> entities, ComponentSignature components, DeferredActionParameters parameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		// Each component can have an allocator that can be used to keep the data together
		// If the size is 0, then no allocator is reserved. This is helpful also for in editor
		// use because all the allocations will happen from this allocator. If the allocator is needed,
		// alongside it, the buffer offsets must be provided such that destroying an entity its buffers be freed.
		// You can specify the buffer being a data pointer in order to save on memory. The element byte size and the size offset
		// are only needed when using the CopyEntities function. Outside of that the runtime doesn't depend on it.
		// When deallocating if the pointer is nullptr then it will skip the deallocation
		// The name is optional - for debugging purposes only
		void RegisterComponentCommit(
			Component component, 
			unsigned int size,
			Stream<char> name = { nullptr, 0 },
			const ComponentFunctions* functions = nullptr
		);

		// ---------------------------------------------------------------------------------------------------

		// Each component can have an allocator that can be used to keep the data together
		// If the size is 0, then no allocator is reserved. This is helpful also for in editor
		// use because all the allocations will happen from this allocator. If the allocator is needed,
		// alongside it, the buffer offsets must be provided such that destroying an entity its buffers be freed
		// The name is optional - for debugging purposes only
		void RegisterSharedComponentCommit(
			Component component, 
			unsigned int size, 
			Stream<char> name = { nullptr, 0 },
			const ComponentFunctions* functions = nullptr,
			SharedComponentCompareEntry compare_entry = {}
		);

		// ---------------------------------------------------------------------------------------------------

		// The name is optional - for debugging purposes only
		// It returns the newly allocated data back to you - if there are any buffers you must
		// Copy them manually. You can set the data parameter to nullptr if you don't want to copy
		// Anything initially, you just want the storage to be allocated. The copy function is needed
		// Only if you plan on using the function CopyOther, which needs it to properly copy it. The
		// Deallocate is needed no matter what
		void* RegisterGlobalComponentCommit(
			Component component,
			unsigned int size,
			const void* data,
			Stream<char> name = { nullptr, 0 },
			const ComponentFunctions* functions = nullptr
		);

		// The name is optional - for debugging purposes only
		// It returns the newly allocated data back to you - if there are any buffers you must
		// Copy them manually. You can set the data parameter to nullptr if you don't want to copy
		// Anything initially, you just want the storage to be allocated. The copy function is needed
		// Only if you plan on using the function CopyOther, which needs it to properly copy it. The
		// Deallocate is needed no matter what
		template<typename T>
		T* RegisterGlobalComponentCommit(
			const T* data,
			const ComponentFunctions* functions = nullptr
		) {
			return (T*)RegisterGlobalComponentCommit(
				T::ID(),
				sizeof(T),
				data,
				GetTemplateName<T>(),
				functions
			);
		}

		// ---------------------------------------------------------------------------------------------------

		// The value is stable. If the data pointer is nullptr, it will not copy anything
		SharedInstance RegisterSharedInstanceCommit(Component component, const void* data, bool copy_buffers = true);

		// Registers a shared instance for a specific handle value for the provided component. The instance
		// Must not exist already. If data pointer is nullptr, it will not copy anything
		void RegisterSharedInstanceForValueCommit(Component component, SharedInstance instance, const void* data, bool copy_buffers = true);

		// The value is stable. If the data pointer is nullptr, it will not copy anything
		// Deferred call
		void RegisterSharedInstance(
			Component component, 
			const void* data, 
			bool copy_buffers = true,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		// The value is stable. If the data pointer is nullptr, it will not copy anything
		SharedInstance RegisterNamedSharedInstanceCommit(Component component, Stream<char> identifier, const void* data, bool copy_buffers = true);
		
		// The value is stable. If the data pointer is nullptr, it will not copy anything
		// Deferred call
		void RegisterNamedSharedInstance(
			Component component, 
			Stream<char> identifier, 
			const void* data, 
			bool copy_buffers = true,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		void RegisterNamedSharedInstanceCommit(Component component, Stream<char> identifier, SharedInstance instance);

		// Deferred call
		void RegisterNamedSharedInstance(
			Component component,
			Stream<char> identifier,
			SharedInstance instance,
			EntityManagerCommandStream* command_stream = nullptr,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// ---------------------------------------------------------------------------------------------------

		// Single threaded
		// Returns a handle to be used to access the query results
		// For queries that contain optional components, you must add the query for the mandatory part
		// You must then handle the optional part
		unsigned int RegisterQueryCommit(const ArchetypeQuery& query);

		// Single threaded
		// Returns a handle to be used to access the query results.
		// For queries that contain optional components, you must add the query for the mandatory part
		// You must then handle the optional part
		unsigned int RegisterQueryCommit(const ArchetypeQueryExclude& query);

		// Backtracks the query cache to another state
		void RestoreQueryCache(const ArchetypeQueryCache* query_cache);

		// ---------------------------------------------------------------------------------------------------

		// Atomically adds it to the pending commands stream
		void RegisterPendingCommandStream(EntityManagerCommandStream command_stream);

		// ---------------------------------------------------------------------------------------------------

		// Immediate call. It will deallocate the data used by the shared instances and reallocate
		// the data but it will not copy any old data.
		void ResizeSharedComponent(Component component, unsigned int new_size);

		// Immediate call. It will deallocate the previous data and reallocate a new block, without
		// Copying the existing data. Returns the newly allocated pointer
		void* ResizeGlobalComponent(Component component, unsigned int new_size);

		// ---------------------------------------------------------------------------------------------------

		// It does not copy any data stored previously. If the new allocation size is 0,
		// then it will deallocate it and return nullptr. Else deallocates and reallocates a new
		// one and returns it
		AllocatorPolymorphic ResizeComponentAllocator(Component component, size_t new_allocation_size);

		// It does not copy any data stored previously. If the new allocation size is 0,
		// then it will deallocate it and return nullptr. Else deallocates and reallocates a new
		// one and returns it
		AllocatorPolymorphic ResizeSharedComponentAllocator(Component component, size_t new_allocation_size);

		// ---------------------------------------------------------------------------------------------------
		
		// It returns to the state where nothing is allocated. As if it is newly created
		void Reset();

		// ---------------------------------------------------------------------------------------------------

		// If the component or the instance doesn't exist, it will assert
		void SetSharedComponentData(Component component, SharedInstance instance, const void* data);

		// If the component or the instance doesn't exist, it will assert
		void SetNamedSharedComponentData(Component component, Stream<char> identifier, const void* data);

		// ---------------------------------------------------------------------------------------------------

		// The tag should only be the bit index, not the actual value
		void SetEntityTagCommit(Stream<Entity> entities, unsigned char tag);

		// Deferred call
		// The tag should only be the bit index, not the actual value
		void SetEntityTag(Stream<Entity> entities, unsigned char tag, DeferredActionParameters parameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		// Takes care of the buffer copy (and deallocation if the entity already has data in it), if specified
		// The deallocate previous buffers is used only if allocate buffers is set to true
		void SetEntityComponentCommit(Entity entity, Component component, const void* data, bool allocate_buffers = true, bool deallocate_previous_buffers = true);

		// Takes care of the buffer copy (and deallocation if the entity already has data in it), if specified
		// The deallocate previous buffers is used only if allocate buffers is set to true
		void SetEntityComponentsCommit(Entity entity, ComponentSignature component_signature, const void** data, bool allocate_buffers = true, bool deallocate_previous_buffers = true);

		// ---------------------------------------------------------------------------------------------------

		// Sets the auto generator for the component functions. The data will be copied if data_size is different
		// From 0, else it will reference it directly.
		void SetAutoGenerateComponentFunctionsFunctor(EntityManagerAutoGenerateComponentFunctionsFunctor functor, void* data, size_t data_size);

		// ---------------------------------------------------------------------------------------------------

		// This action shouldn't be necessary for normal user code. It was added mostly for the change set
		// Function which requires this operation type. Swaps an archetype from an index to another index
		// And patches up all references to that archetype from other internal structures.
		void SwapArchetypeCommit(unsigned int previous_index, unsigned int new_index);

		// This action shouldn't be necessary for normal user code. It was added mostly for the change set
		// Function which requires this operation type. Swaps a base archetype from an index to another index
		// And patches up all references of that base archetype from other internal structures.
		void SwapArchetypeBaseCommit(unsigned int archetype_index, unsigned int previous_base_index, unsigned int new_base_index);

		// ---------------------------------------------------------------------------------------------------

		// If the entity doesn't have the component, it will return nullptr
		void* TryGetComponent(Entity entity, Component component);

		// If the entity doesn't have the component, it will return nullptr
		const void* TryGetComponent(Entity entity, Component component) const;

		// If the entity doesn't have the component, it will return nullptr. It is faster than the entity variant, 
		// Since it can readily use the entity info data to locate the component fast.
		void* TryGetComponent(EntityInfo info, Component component);

		// If the entity doesn't have the component, it will return nullptr. It is faster than the entity variant, 
		// Since it can readily use the entity info data to locate the component fast.
		const void* TryGetComponent(EntityInfo info, Component component) const;

		// If the entity doesn't have the shared component, it will return nullptr
		void* TryGetSharedComponent(Entity entity, Component component);

		// If the entity doesn't have the shared component, it will return nullptr
		const void* TryGetSharedComponent(Entity entity, Component component) const;

		// If the entity doesn't have the shared component, it will return nullptr. It is faster than the entity variant, 
		// Since it can readily use the entity info data to locate the component fast.
		void* TryGetSharedComponent(EntityInfo info, Component component);

		// If the entity doesn't have the shared component, it will return nullptr. It is faster than the entity variant, 
		// Since it can readily use the entity info data to locate the component fast.
		const void* TryGetSharedComponent(EntityInfo info, Component component) const;

		// If the component was not registered or not set, it returns nullptr
		void* TryGetGlobalComponent(Component component);

		// If the component was not registered or not set, it returns nullptr
		const void* TryGetGlobalComponent(Component component) const;

		template<typename T>
		ECS_INLINE T* TryGetComponent(Entity entity) {
			return (T*)((const EntityManager*)this)->TryGetComponent<T>(entity);
		}
		
		// This overload is faster than the entity one if this information is readily available
		template<typename T>
		ECS_INLINE T* TryGetComponent(EntityInfo info) {
			return (T*)((const EntityManager*)this)->TryGetComponent<T>(info);
		}

		template<typename T>
		ECS_INLINE const T* TryGetComponent(Entity entity) const {
			if constexpr (T::IsShared()) {
				return (const T*)TryGetSharedComponent(entity, T::ID());
			}
			else {
				return (const T*)TryGetComponent(entity, T::ID());
			}
		}

		// This overload is faster than the entity one if this information is readily available
		template<typename T>
		ECS_INLINE const T* TryGetComponent(EntityInfo info) const {
			if constexpr (T::IsShared()) {
				return (const T*)TryGetSharedComponent(info, T::ID());
			}
			else {
				return (const T*)TryGetComponent(info, T::ID());
			}
		}

		template<typename T>
		ECS_INLINE T* TryGetGlobalComponent() {
			return (T*)((const EntityManager*)this)->TryGetGlobalComponent(T::ID());
		}

		template<typename T>
		ECS_INLINE const T* TryGetGlobalComponent() const {
			return (const T*)TryGetGlobalComponent(T::ID());
		}

		// It tries to retrieve the shared instance that the entity has. If the entity does not have the shared component,
		// It will return an invalid shared instance.
		SharedInstance TryGetComponentSharedInstance(Component component, Entity entity) const;

		// It tries to retrieve the shared instance that the entity has. If the entity does not have the shared component,
		// It will return an invalid shared instance. This overload is faster if the entity info is known as opposed to the
		// Entity overload
		SharedInstance TryGetComponentSharedInstance(Component component, EntityInfo info) const;

		// Returns the entity info that corresponds to the entity, if it is valid, else nullptr
		ECS_INLINE const EntityInfo* TryGetEntityInfo(Entity entity) const {
			return m_entity_pool->TryGetEntityInfo(entity);
		}

		// ---------------------------------------------------------------------------------------------------

		// It will search to see if there is an existing instance that has the same data. In that case,
		// It will unregister this one and reroute all entities that reference this shared instance to
		// That one
		bool TryMergeSharedInstanceCommit(Component component, SharedInstance instance, bool remove_empty_archetypes = true);

		// ---------------------------------------------------------------------------------------------------

		// Returns true if the entity was removed, else false
		bool TryRemoveEntityFromHierarchyCommit(Entity entity, bool default_child_destroy = true);

		// Returns true if all entities were removed, else false
		bool TryRemoveEntityFromHierarchyCommit(Stream<Entity> entities, bool default_child_destroy = true);

		void TryRemoveEntityFromHierarchy(Stream<Entity> entities, bool default_child_destroy, DeferredActionParameters parameters = {}, DebugInfo debug_info = ECS_DEBUG_INFO);
		
		// ---------------------------------------------------------------------------------------------------

		// Frees the slot used by that component.
		void UnregisterComponentCommit(Component component);

		// ---------------------------------------------------------------------------------------------------

		// Frees the slot used by that component.
		void UnregisterSharedComponentCommit(Component component);

		// ---------------------------------------------------------------------------------------------------

		void UnregisterGlobalComponentCommit(Component component);

		// ---------------------------------------------------------------------------------------------------

		void UnregisterSharedInstanceCommit(Component component, SharedInstance instance);

		// Deferred call
		void UnregisterSharedInstance(Component component, SharedInstance instance, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		void UnregisterNamedSharedInstanceCommit(Component component, Stream<char> name);

		// Deferred call
		void UnregisterNamedSharedInstance(Component component, Stream<char> name, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		// Returns true if the instance was deleted, else false.
		bool UnregisterUnreferencedSharedInstanceCommit(Component component, SharedInstance instance);

		// Deferred call
		void UnregisterUnreferencedSharedInstance(Component component, SharedInstance instance, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns true if the shared instance is no longer present in any archetype
		bool IsUnreferencedSharedInstance(Component component, SharedInstance instance) const;

		// ---------------------------------------------------------------------------------------------------

		void UnregisterUnreferencedSharedInstancesCommit(Component component);

		// Deferred call
		void UnregisterUnreferencedSharedInstances(Component component, EntityManagerCommandStream* command_stream = nullptr, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ---------------------------------------------------------------------------------------------------

		// Returns true if the entity info references inside the entity pool match the archetypes where the entities are stored, else false
		bool ValidateEntityInfos() const;

		// ---------------------------------------------------------------------------------------------------

		// Fills in the shared instances that are not referenced by any archetype
		void GetUnreferencedSharedInstances(Component component, CapacityStream<SharedInstance>* instances) const;

	private:
		// The bitmask must have allocated the capacity of the instances stream of the component of bools
		// The function will memset the booleans inside
		void UnreferencedSharedInstanceBitmask(Component component, CapacityStream<bool> bitmask) const;
	public:

		// The functor receives as parameters (SharedInstance instance, void* data)
		// Return true in the functor to early exit, if desired
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachUnreferencedSharedInstance(Component component, Functor&& functor) {
			unsigned int max_instances = m_shared_components[component].instances.stream.capacity;
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(bool, instance_bitmask, max_instances);
			UnreferencedSharedInstanceBitmask(component, instance_bitmask);

			for (unsigned int index = 0; index < max_instances; index++) {
				if (!instance_bitmask[index]) {
					SharedInstance instance = { (short)index };
					void* data = GetSharedData(component, instance);
					if constexpr (early_exit) {
						if (functor(instance, data)) {
							return true;
						}
					}
					else {
						functor(instance, data);
					}
				}
			}
			return false;
		}

		// CONST VARIANT
		// The functor receives as parameters (SharedInstance instance, const void* data)
		// Return true in the functor to early exit, if desired
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachUnreferencedSharedInstance(Component component, Functor&& functor) const {
			unsigned int max_instances = m_shared_components[component].instances.stream.capacity;
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(bool, instance_bitmask, max_instances);
			UnreferencedSharedInstanceBitmask(component, instance_bitmask);

			for (unsigned int index = 0; index < max_instances; index++) {
				if (!instance_bitmask[index]) {
					SharedInstance instance = { (short)index };
					const void* data = GetSharedData(component, instance);
					if constexpr (early_exit) {
						if (functor(instance, data)) {
							return true;
						}
					}
					else {
						functor(instance, data);
					}
				}
			}
			return false;
		}

		// ---------------------------------------------------------------------------------------------------

		MemoryManager m_small_memory_manager;
		// This is as an allocator for allocations
		// That regard the components
		MemoryManager m_component_memory_manager;
		
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

		MemoryManager* m_hierarchy_allocator;
		EntityHierarchy m_hierarchy;

		// The global components are stored in a SoA manner in order to use a fast SIMD
		// Determination in order to obtain the global component
		void** m_global_components_data;
		Component* m_global_components;
		ComponentInfo* m_global_components_info;
		unsigned int m_global_component_count;
		unsigned int m_global_component_capacity;

		MemoryManager* m_memory_manager;
		EntityPool* m_entity_pool;
		ResizableLinearAllocator m_temporary_allocator;
		
		// See the comment for the functor for the reasoning behind these fields. The data for the functor
		// Must be blittable, if the data size is different from 0
		Stream<void> m_auto_generate_component_functions_data;
		EntityManagerAutoGenerateComponentFunctionsFunctor m_auto_generate_component_functions_functor;
	};

	// Uses the internal indexing of the entity manager
	ECS_INLINE VectorComponentSignature ECS_VECTORCALL GetEntityManagerUniqueVectorSignature(const VectorComponentSignature* signatures, unsigned int index) {
		return signatures[(size_t)index << 1];
	}

	// Uses the internal indexing of the entity manager
	ECS_INLINE VectorComponentSignature ECS_VECTORCALL GetEntityManagerSharedVectorSignature(const VectorComponentSignature* signatures, unsigned int index) {
		return signatures[((size_t)index << 1) + 1];
	}

	// Uses the internal indexing of the entity manager
	ECS_INLINE VectorComponentSignature* GetEntityManagerUniqueVectorSignaturePtr(VectorComponentSignature* signatures, unsigned int index) {
		return signatures + ((size_t)index << 1);
	}

	// Uses the internal indexing of the entity manager
	ECS_INLINE VectorComponentSignature* GetEntityManagerSharedVectorSignaturePtr(VectorComponentSignature* signatures, unsigned int index) {
		return signatures + ((size_t)index << 1) + 1;
	}

	// Creates the allocator for the entity pool, the entity pool itself, the entity manager allocator,
	// and the entity manager itself.
	ECSENGINE_API void CreateEntityManagerWithPool(
		EntityManager* entity_manager,
		size_t allocator_size, 
		size_t allocator_pool_count,
		size_t allocator_new_size, 
		unsigned int entity_pool_power_of_two,
		GlobalMemoryManager* global_memory_manager
	);

	// INTERNAL
	// This is mostly an internal function to be used by the delta change set deserializer
	// In order to move an entity to a specific archetype and location. You need to provide
	// As an argument the components you want to copy, since the entity will be removed from
	// The base archetype. If the entity cannot be placed at the exact stream specified in info,
	// (because not enough entities have been created), it will return false, else true (the entity
	// Was placed at info.stream_index).
	ECSENGINE_API bool EntityManagerMoveEntityToEntityInfo(EntityManager* entity_manager, Entity entity, EntityInfo info, ComponentSignature components_to_copy);

}

