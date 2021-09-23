#pragma once
#include "../Core.h"
#include "../Allocators/MemoryManager.h"
#include "../Allocators/LinearAllocator.h"
#include "Archetype.h"

#ifndef ECS_ENTITY_MANAGER_MAX_DYNAMIC_ARCHETYPES
#define ECS_ENTITY_MANAGER_MAX_DYNAMIC_ARCHETYPES 1 << 8
#endif

#ifndef ECS_ENTITY_MANAGER_MAX_STATIC_ARCHETYPES
#define ECS_ENTITY_MANAGER_MAX_STATIC_ARCHETYPES 1 << 6
#endif

#ifndef ECS_ENTITY_MANAGER_DEFAULT_MAX_ARCHETYPE_COUNT_FOR_NEW_ARCHETYPE
#define ECS_ENTITY_MANAGER_DEFAULT_MAX_ARCHETYPE_COUNT_FOR_NEW_ARCHETYPE 8
#endif

namespace ECSEngine {

	ECS_CONTAINERS;

	class ECSENGINE_API EntityManager
	{
	public:
		EntityManager(
			MemoryManager* memory_manager, 
			EntityPool* entity_pool,
			unsigned int max_dynamic_archetype_count = ECS_ENTITY_MANAGER_MAX_DYNAMIC_ARCHETYPES,
			unsigned int max_static_archetype_count = ECS_ENTITY_MANAGER_MAX_STATIC_ARCHETYPES
		);

		// SINGLE THREADED
		unsigned int AddComponent(unsigned int entity, unsigned char component);

		// SINGLE THREADED
		unsigned int AddComponent(unsigned int entity, unsigned char component, void* data);
		
		// SINGLE THREADED
		unsigned int AddComponent(unsigned int entity, const unsigned char* components);

		// SINGLE THREADED
		unsigned int AddComponent(unsigned int entity, const unsigned char* components, void** data);

		// entities must belong to the same base archetype 
		// SINGLE THREADED
		unsigned int AddComponent(const Stream<unsigned int>& entities, unsigned char component);

		// entities must belong to the same base archetype; data is a buffer that hold components contiguously
		// SINGLE THREADED
		unsigned int AddComponent(const Stream<unsigned int>& entities, unsigned char component, void* data);

		// entities must belong to the same base archetype
		// SINGLE THREADED
		unsigned int AddComponent(const Stream<unsigned int>& entities, const unsigned char* components);

		// entities must belong to the same base archetype; data parsed by component 
		// data -> AAA BBB CCC; contiguous
		// SINGLE THREADED
		unsigned int AddComponent(const Stream<unsigned int>& entities, const unsigned char* components, void** data);

		// entities must belong to the same base archetype; data parsed by components
		// data -> AAA BBB CCC; non-contiguous; last parameter is dummy
		// SINGLE THREADED
		unsigned int AddComponent(const Stream<unsigned int>& entities, const unsigned char* components, void** data, bool non_contiguous);

		// entities must belong to the same base archetype; data parsed by entity
		// data -> ABC ABC ABC ABC; non-contiguous; last parameter is dummy
		// SINGLE THREADED
		unsigned int AddComponent(const Stream<unsigned int>& entities, const unsigned char* components, void** data, unsigned int non_contiguous_and_parsed_by_entity);

		// entities must belong to the same base archetype
		// SINGLE THREADED
		unsigned int AddSharedComponent(const Stream<unsigned int>& entities, const unsigned char* components, const unsigned short* subindices);

		// it will do a linear seach in order to see if an archetype of that signature was already created in order to not create another one
		unsigned int CreateArchetype(
			ArchetypeBaseDescriptor descriptor,
			const unsigned char* components, 
			const unsigned char* shared_components, 
			unsigned int max_base_archetypes,
			MemoryManager* memory_manager = nullptr
		);

		unsigned int CreateArchetypeBase(
			ArchetypeBaseDescriptor descriptor, 
			const unsigned char* components, 
			const unsigned char* shared_components, 
			const unsigned short* shared_subindices
		);

		// shared_components must still be provided in order to have the shared components placed in right order
		unsigned int CreateArchetypeBase(
			ArchetypeBaseDescriptor descriptor,
			unsigned int archetype_index, 
			const unsigned char* shared_components,
			const unsigned short* shared_subindices
		);

		// it will do a linear search in order to see if an archetype of that signature was already created in order to not create another one
		unsigned int CreateStaticArchetype(
			ArchetypeBaseDescriptor descriptor, 
			const unsigned char* components, 
			const unsigned char* shared_components,
			unsigned int max_base_archetypes,
			MemoryManager* memory_manager = nullptr
		);
		
		unsigned int CreateStaticArchetypeBase(
			ArchetypeBaseDescriptor descriptor,
			const unsigned char* components,
			const unsigned char* shared_components, 
			const unsigned short* shared_subindices
		);

		// shared_components must still be provided in order to have the shared components placed in right order
		unsigned int CreateStaticArchetypeBase(
			ArchetypeBaseDescriptor descriptor, 
			unsigned int archetype_index, 
			const unsigned char* shared_components,
			const unsigned short* shared_subindices
		);
		
		// entity infos must be set manually
		unsigned int CreateEntities(unsigned int count);

		unsigned int CreateEntities(unsigned int count, unsigned int archetype_index, unsigned int archetype_base_index);

		unsigned int CreateEntities(
			unsigned int count, 
			const unsigned char* components, 
			const unsigned char* shared_components, 
			const unsigned short* shared_subindices
		);

		// shared_components is needed to insert correctly into the archetype base
		unsigned int ECS_VECTORCALL CreateEntities(
			unsigned int count, 
			VectorComponentSignature archetype_signature,
			VectorComponentSignature archetype_shared_signature,
			const unsigned char* components,
			const unsigned char* shared_components,
			const unsigned short* shared_subindices
		);
		
		unsigned int CreateStaticEntities(unsigned int count, unsigned int archetype_index, unsigned int archetype_subindex);

		unsigned int CreateStaticEntities(
			unsigned int count,
			const unsigned char* components,
			const unsigned char* shared_components,
			const unsigned short* shared_subindices
		);

		unsigned int ECS_VECTORCALL CreateStaticEntities(
			unsigned int count,
			VectorComponentSignature archetype_signature,
			VectorComponentSignature archetype_shared_signature,
			const unsigned char* components,
			const unsigned char* shared_components,
			const unsigned short* shared_subindices
		);

		void DeleteEntity(unsigned int entity);

		// deletes one by one
		void DeleteEntities(const Stream<unsigned int>& entities);

		void DeleteEntities(const Stream<unsigned int>& entities, bool same_base_archetype);

		// entities that are in the same sequence should not appear since it will delete each sequence of each entity
		void DeleteStaticEntities(const Stream<unsigned int>& entities);

		void DestroyArchetype(unsigned int archetype_index);

		void DestroyArchetype(const unsigned char* components, const unsigned char* shared_components);

		void DestroyArchetypeBase(unsigned int archetype_index, unsigned int archetype_subindex);

		void DestroyArchetypeBase(
			const unsigned char* components, 
			const unsigned char* shared_components, 
			const unsigned short* shared_subindices
		);

		void DestroyStaticArchetype(unsigned int archetype_index);

		void DestroyStaticArchetype(const unsigned char* components, const unsigned char* shared_components);

		void DestroyStaticArchetypeBase(unsigned int archetype_index, unsigned int archetype_subindex);

		void DestroyStaticArchetypeBase(
			const unsigned char* components, 
			const unsigned char* shared_components, 
			const unsigned short* shared_subindices
		);

		unsigned int ECS_VECTORCALL FindArchetype(
			VectorComponentSignature unique_signature,
			VectorComponentSignature shared_signature
		) const;

		unsigned int FindArchetype(
			const unsigned char* unique_components,
			const unsigned char* shared_components
		) const;

		ArchetypeBase* ECS_VECTORCALL FindArchetype(
			VectorComponentSignature unique_signature,
			VectorComponentSignature shared_signature,
			const unsigned char* shared_components,
			const unsigned short* shared_subindices
		) const;

		ArchetypeBase* FindArchetype(
			const unsigned char* unique_components,
			const unsigned char* shared_components,
			const unsigned short* shared_subindices
		) const;

		unsigned int ECS_VECTORCALL FindStaticArchetype(
			VectorComponentSignature unique_signature,
			VectorComponentSignature shared_signature
		) const;

		unsigned int FindStaticArchetype(
			const unsigned char* unique_components,
			const unsigned char* shared_components
		) const;

		StaticArchetypeBase* FindStaticArchetype(
			const unsigned char* unique_components,
			const unsigned char* shared_components,
			const unsigned short* shared_subindices
		) const;

		StaticArchetypeBase* ECS_VECTORCALL FindStaticArchetype(
			VectorComponentSignature unique_signature,
			VectorComponentSignature shared_signature,
			const unsigned char* shared_components,
			const unsigned short* shared_subindices
		) const;

		// for GUI purposes
		void GetArchetypeStream(Stream<Archetype<ArchetypeBase>>& archetypes) const;

		// for GUI porposes
		void GetStaticArchetypeStream(Stream<Archetype<StaticArchetypeBase>>& archetypes) const;

		void GetArchetypes(const ArchetypeQuery* query, Stream<ArchetypeBase*>& archetypes) const;

		void GetArchetypes(const ArchetypeQueryExclude* query, Stream<ArchetypeBase*>& archetypes) const;

		void GetArchetypes(
			const unsigned char* unique_components,
			const unsigned char* shared_components,
			Stream<ArchetypeBase*>& archetypes
		) const;

		void GetArchetypes(
			const unsigned char* unique_components,
			const unsigned char* shared_components,
			const unsigned char* exclude_components,
			const unsigned char* exclude_shared_components,
			Stream<ArchetypeBase*>& archetypes
		) const;

		void GetStaticArchetypes(const ArchetypeQuery* query, Stream<StaticArchetypeBase*>& archetypes) const;

		void GetStaticArchetypes(const ArchetypeQueryExclude* query, Stream<StaticArchetypeBase*>& archetypes) const;

		void GetStaticArchetypes(
			const unsigned char* unqiue_components,
			const unsigned char* shared_components,
			Stream<StaticArchetypeBase*>& archetypes
		) const;

		void GetStaticArchetypes(
			const unsigned char* unique_components,
			const unsigned char* shared_components,
			const unsigned char* exclude_components,
			const unsigned char* exclude_shared_components,
			Stream<StaticArchetypeBase*>& archetypes
		) const;

		void* GetComponent(unsigned int entity, unsigned char component) const;

		void* GetComponentWithIndex(unsigned int entity, unsigned char component_index) const;

		void GetComponent(unsigned int entity, const unsigned char* components, Stream<void*>& data) const;

		void GetComponentWithIndex(unsigned int entity, const unsigned char* component_indices, Stream<void*>& data) const;

		void* GetComponentStatic(unsigned int entity, unsigned char component) const;

		void* GetComponentStaticWithIndex(unsigned int entity, unsigned char component_index) const;

		void GetComponentStatic(unsigned int entity, const unsigned char* components, Stream<void*>& data) const;

		void GetComponentStaticWithIndex(
			unsigned int entity, 
			const unsigned char* component_indices,
			Stream<void*>& data) 
		const;

		EntityInfo GetEntityInfo(unsigned int entity) const;

		void GetEntities(
			const ArchetypeQuery* query, 
			Stream<Stream<Stream<Sequence>>>& sequences, 
			unsigned int*** entity_buffers
		) const;

		void GetEntities(
			const ArchetypeQueryExclude* query,
			Stream<Stream<Stream<Sequence>>>& sequences,
			unsigned int*** entity_buffers
		) const;

		void GetStaticEntities(
			const ArchetypeQuery* query, 
			Stream<Stream<Stream<Sequence>>>& sequences
		) const;

		void GetStaticEntities(
			const ArchetypeQueryExclude* query,
			Stream<Stream<Stream<Sequence>>>& sequences
		) const;

		// 1 read from shared components for size
		unsigned int GetSharedComponentSubindex(unsigned char component, void* data) const;

		// size parameter is needed in order to avoid looking up for it in the shared component buffer
		unsigned int GetSharedComponentSubindex(unsigned char component, void* data, unsigned int size) const;

		void GetSharedComponentDataStream(Stream<void*>& data, const unsigned char* components) const;

		void RebalanceArchetype(
			unsigned int archetype_index, 
			unsigned char archetype_subindex, 
			RebalanceArchetypeDescriptor descriptor,
			LinearAllocator* local_allocator
		);

		unsigned int RegisterComponent(unsigned short size, unsigned char alignment);

		unsigned int RegisterSharedComponent(unsigned short size, unsigned char alignment, unsigned short count);

		unsigned int RegisterSharedComponentData(unsigned char component, void* data);

		// needs to look-up in the component info buffer for size; 1 cache miss potential
		void SetSharedComponentData(unsigned char component, unsigned short subindex, void* data);

		// size parameter avoids looking up into the component info buffer
		void SetSharedComponentData(unsigned char component, unsigned short subindex, void* data, unsigned int size);

		static size_t MemoryOf(unsigned int max_dynamic_archetype_count, unsigned int max_static_archetype_count);
	//private:
		Stream<Archetype<ArchetypeBase>> m_archetypes;
		Stream<Archetype<StaticArchetypeBase>> m_static_archetypes;
		Stream<ComponentInfo> m_unique_components;
		Stream<ComponentInfo> m_shared_components;
		ArchetypeInfo* m_archetype_info;
		ArchetypeInfo* m_static_archetype_info;
		Stream<void>* m_shared_component_data;
		unsigned int* m_shared_component_capacity;
		MemoryManager* m_memory_manager;
		EntityPool* m_entity_pool;
		unsigned int m_max_dynamic_archetype_count;
		unsigned int m_max_static_archetype_count;
	};

}

