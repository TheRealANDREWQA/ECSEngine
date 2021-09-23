#include "ecspch.h"
#include "EntityManager.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

	EntityManager::EntityManager(
		MemoryManager* memory_manager,
		EntityPool* entity_pool,
		unsigned int max_dynamic_archetype_count,
		unsigned int max_static_archetype_count
	) : m_memory_manager(memory_manager), m_entity_pool(entity_pool),
		m_max_dynamic_archetype_count(max_dynamic_archetype_count), m_max_static_archetype_count(max_static_archetype_count)
	{
		void* allocation = m_memory_manager->Allocate(MemoryOf(max_dynamic_archetype_count, max_static_archetype_count), ECS_CACHE_LINE_SIZE);
		uintptr_t buffer = (uintptr_t)allocation;
		m_archetypes.buffer = (Archetype<ArchetypeBase>*)allocation;

		buffer += sizeof(Archetype<ArchetypeBase>) * max_dynamic_archetype_count;
		buffer = function::align_pointer(buffer, alignof(Archetype<StaticArchetypeBase>));
		m_static_archetypes.buffer = (Archetype<StaticArchetypeBase>*)buffer;

		buffer += sizeof(Archetype<StaticArchetypeBase>) * max_static_archetype_count;
		buffer = function::align_pointer(buffer, alignof(ArchetypeInfo));
		m_archetype_info = (ArchetypeInfo*)buffer;

		buffer += sizeof(ArchetypeInfo) * max_dynamic_archetype_count;
		buffer = function::align_pointer(buffer, alignof(ArchetypeInfo));
		m_static_archetype_info = (ArchetypeInfo*)buffer;

		buffer += sizeof(ArchetypeInfo) * max_static_archetype_count;
		buffer = function::align_pointer(buffer, alignof(ComponentInfo));
		m_unique_components.buffer = (ComponentInfo*)buffer;

		buffer += sizeof(ComponentInfo) * 256;
		buffer = function::align_pointer(buffer, alignof(ComponentInfo));
		m_shared_components.buffer = (ComponentInfo*)buffer;

		buffer += sizeof(ComponentInfo) * 256;
		buffer = function::align_pointer(buffer, alignof(Stream<void>));
		m_shared_component_data = (Stream<void>*)buffer;

		buffer += sizeof(Stream<void>) * 256;
		buffer = function::align_pointer(buffer, alignof(unsigned int));
		m_shared_component_capacity = (unsigned int*)buffer;

		m_archetypes.size = 0;
		m_static_archetypes.size = 0;
		m_unique_components.size = 0;
		m_shared_components.size = 0;
	}

	unsigned int EntityManager::AddComponent(unsigned int entity, unsigned char component) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		ArchetypeBase* archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		// it will point to the archetype buffer; no need to allocate memory
		Stream<ComponentInfo> unique_infos;
		m_archetypes[info.archetype].GetComponentInfo(unique_infos);

		// it will point to the archetype buffer; no need to allocate memory
		Stream<ComponentInfo> shared_infos;
		m_archetypes[info.archetype].GetSharedComponentInfo(shared_infos);

		void* data[32];
		unsigned char components[32];
		unsigned char shared_components[32];
		unsigned short shared_subindices[32];
		m_archetypes[info.archetype].GetArchetypeSharedSubindices(info.archetype_subindex, shared_subindices);
		archetype->GetComponent(entity, info, data);

		memcpy(
			components,
			m_archetype_info[info.archetype].components, 
			sizeof(unsigned char) * m_archetype_info[info.archetype].components[0] + 1
		);
		unsigned char copy_count = components[0];
		components[0]++;
		components[components[0]] = component;

		memcpy(
			shared_components, 
			m_archetype_info[info.archetype].shared_components, 
			sizeof(unsigned char) * m_archetype_info[info.archetype].shared_components[0] + 1
		);

		unsigned int new_entity = m_entity_pool->GetSequence(1);
		unsigned int new_archetype_index = FindArchetype(components, shared_components);

		unsigned int new_archetype_subindex;
		ArchetypeBase* new_archetype_base = m_archetypes[new_archetype_index].FindArchetypeBase(shared_subindices, new_archetype_subindex);
		if (new_archetype_base == nullptr) {
			new_archetype_base = m_archetypes[new_archetype_index].CreateBaseArchetype(shared_subindices);
			new_archetype_subindex = m_archetypes[new_archetype_index].GetArchetypeBaseCount() - 1;
		}

		EntityInfo new_info;
		new_info.archetype = new_archetype_index;
		new_info.archetype_subindex = new_archetype_subindex;

		unsigned int offset;
		unsigned int chunk_index;
		unsigned int sequence_index;
		new_archetype_base->AddEntities(new_entity, 1, offset, chunk_index, sequence_index, true);

		new_info.sequence = sequence_index;
		new_info.chunk = chunk_index;
		m_entity_pool->SetEntityInfo(new_entity, new_info);
		unsigned int entity_index_in_table = new_archetype_base->GetEntityIndex(entity, new_info);
		for (size_t index = 1; index <= copy_count; index++) {
			unsigned char component_index = new_archetype_base->FindComponentIndex(components[index]);
			new_archetype_base->UpdateComponent(entity_index_in_table, component_index, data[index - 1], new_info, unsigned int(1));
		}

		unsigned int deleted_sequence_first;
		Stream<unsigned int> deleted_sequence_stream(&deleted_sequence_first, 0);
		archetype->RemoveEntities(Stream<unsigned int>(&entity, 1), m_entity_pool, deleted_sequence_stream);
		for (size_t index = 0; index < deleted_sequence_stream.size; index++) {
			m_entity_pool->FreeSequence(deleted_sequence_stream[index]);
		}

		return new_entity;
	}

	unsigned int EntityManager::AddComponent(unsigned int entity, unsigned char component, void* data) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		ArchetypeBase* archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		void* data_array[32];
		unsigned char components[32];
		unsigned char shared_components[32];
		unsigned short shared_subindices[32];
		m_archetypes[info.archetype].GetArchetypeSharedSubindices(info.archetype_subindex, shared_subindices);
		archetype->GetComponent(entity, info, data_array);

		memcpy(
			components,
			m_archetype_info[info.archetype].components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].components[0] + 1
		);
		data_array[components[0]] = data;
		components[0]++;
		components[components[0]] = component;
		unsigned char copy_count = components[0];

		memcpy(
			shared_components,
			m_archetype_info[info.archetype].shared_components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].shared_components[0] + 1
		);

		unsigned int new_entity = m_entity_pool->GetSequence(1);
		unsigned int new_archetype_index = FindArchetype(components, shared_components);

		unsigned int new_archetype_subindex;
		ArchetypeBase* new_archetype_base = m_archetypes[new_archetype_index].FindArchetypeBase(shared_subindices, new_archetype_subindex);
		if (new_archetype_base == nullptr) {
			new_archetype_base = m_archetypes[new_archetype_index].CreateBaseArchetype(shared_subindices);
			new_archetype_subindex = m_archetypes[new_archetype_index].GetArchetypeBaseCount() - 1;
		}

		EntityInfo new_info;
		new_info.archetype = new_archetype_index;
		new_info.archetype_subindex = new_archetype_subindex;

		unsigned int offset;
		unsigned int chunk_index;
		unsigned int sequence_index;
		new_archetype_base->AddEntities(new_entity, 1, offset, chunk_index, sequence_index, true);

		new_info.sequence = sequence_index;
		new_info.chunk = chunk_index;
		m_entity_pool->SetEntityInfo(new_entity, new_info);
		unsigned int entity_index_in_table = new_archetype_base->GetEntityIndex(entity, new_info);
		for (size_t index = 1; index <= copy_count; index++) {
			unsigned char component_index = new_archetype_base->FindComponentIndex(components[index]);
			new_archetype_base->UpdateComponent(entity_index_in_table, component_index, data_array[index - 1], new_info, unsigned int(1));
		}

		unsigned int deleted_sequence_first;
		Stream<unsigned int> deleted_sequence_stream(&deleted_sequence_first, 0);
		archetype->RemoveEntities(Stream<unsigned int>(&entity, 1), m_entity_pool, deleted_sequence_stream);
		for (size_t index = 0; index < deleted_sequence_stream.size; index++) {
			m_entity_pool->FreeSequence(deleted_sequence_stream[index]);
		}

		return new_entity;
	}

	unsigned int EntityManager::AddComponent(unsigned int entity, const unsigned char* component_order) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		ArchetypeBase* archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		void* data_array[32];
		unsigned char components[32];
		unsigned char shared_components[32];
		unsigned short shared_subindices[32];
		m_archetypes[info.archetype].GetArchetypeSharedSubindices(info.archetype_subindex, shared_subindices);
		archetype->GetComponent(entity, info, data_array);

		memcpy(
			components,
			m_archetype_info[info.archetype].components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].components[0] + 1
		);
		unsigned int copy_count = components[0];
		for (size_t index = 1; index <= component_order[0]; index++) {
			components[components[0] + index] = component_order[index];
		}
		components[0] += component_order[0];

		memcpy(
			shared_components,
			m_archetype_info[info.archetype].shared_components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].shared_components[0] + 1
		);

		unsigned int new_entity = m_entity_pool->GetSequence(1);
		unsigned int new_archetype_index = FindArchetype(components, shared_components);

		unsigned int new_archetype_subindex;
		ArchetypeBase* new_archetype_base = m_archetypes[new_archetype_index].FindArchetypeBase(shared_subindices, new_archetype_subindex);
		if (new_archetype_base == nullptr) {
			new_archetype_base = m_archetypes[new_archetype_index].CreateBaseArchetype(shared_subindices);
			new_archetype_subindex = m_archetypes[new_archetype_index].GetArchetypeBaseCount() - 1;
		}

		EntityInfo new_info;
		new_info.archetype = new_archetype_index;
		new_info.archetype_subindex = new_archetype_subindex;

		unsigned int offset;
		unsigned int chunk_index;
		unsigned int sequence_index;
		new_archetype_base->AddEntities(new_entity, 1, offset, chunk_index, sequence_index, true);

		new_info.sequence = sequence_index;
		new_info.chunk = chunk_index;
		m_entity_pool->SetEntityInfo(new_entity, new_info);
		unsigned int entity_index_in_table = new_archetype_base->GetEntityIndex(entity, new_info);
		for (size_t index = 1; index <= copy_count; index++) {
			unsigned char component_index = new_archetype_base->FindComponentIndex(components[index]);
			new_archetype_base->UpdateComponent(entity_index_in_table, component_index, data_array[index - 1], new_info, unsigned int(1));
		}

		unsigned int deleted_sequence_first;
		Stream<unsigned int> deleted_sequence_stream(&deleted_sequence_first, 0);
		archetype->RemoveEntities(Stream<unsigned int>(&entity, 1), m_entity_pool, deleted_sequence_stream);
		for (size_t index = 0; index < deleted_sequence_stream.size; index++) {
			m_entity_pool->FreeSequence(deleted_sequence_stream[index]);
		}

		return new_entity;
	}

	unsigned int EntityManager::AddComponent(unsigned int entity, const unsigned char* component_order, void** data) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		ArchetypeBase* archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		void* data_array[32];
		unsigned char components[32];
		unsigned char shared_components[32];
		unsigned short shared_subindices[32];
		m_archetypes[info.archetype].GetArchetypeSharedSubindices(info.archetype_subindex, shared_subindices);
		archetype->GetComponent(entity, info, data_array);

		memcpy(
			components,
			m_archetype_info[info.archetype].components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].components[0] + 1
		);
		for (size_t index = 1; index <= component_order[0]; index++) {
			components[components[0] + index] = component_order[index];
			data_array[components[0] + index - 1] = data[index - 1];
		}
		components[0] += component_order[0];
		unsigned int copy_count = components[0];

		memcpy(
			shared_components,
			m_archetype_info[info.archetype].shared_components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].shared_components[0] + 1
		);

		unsigned int new_entity = m_entity_pool->GetSequence(1);
		unsigned int new_archetype_index = FindArchetype(components, shared_components);

		unsigned int new_archetype_subindex;
		ArchetypeBase* new_archetype_base = m_archetypes[new_archetype_index].FindArchetypeBase(shared_subindices, new_archetype_subindex);
		if (new_archetype_base == nullptr) {
			new_archetype_base = m_archetypes[new_archetype_index].CreateBaseArchetype(shared_subindices);
			new_archetype_subindex = m_archetypes[new_archetype_index].GetArchetypeBaseCount() - 1;
		}

		EntityInfo new_info;
		new_info.archetype = new_archetype_index;
		new_info.archetype_subindex = new_archetype_subindex;

		unsigned int offset;
		unsigned int chunk_index;
		unsigned int sequence_index;
		new_archetype_base->AddEntities(new_entity, 1, offset, chunk_index, sequence_index, true);

		new_info.sequence = sequence_index;
		new_info.chunk = chunk_index;
		m_entity_pool->SetEntityInfo(new_entity, new_info);
		unsigned int entity_index_in_table = new_archetype_base->GetEntityIndex(entity, new_info);
		for (size_t index = 1; index <= copy_count; index++) {
			unsigned char component_index = new_archetype_base->FindComponentIndex(components[index]);
			new_archetype_base->UpdateComponent(entity_index_in_table, component_index, data_array[index - 1], new_info, unsigned int(1));
		}

		unsigned int deleted_sequence_first;
		Stream<unsigned int> deleted_sequence_stream(&deleted_sequence_first, 0);
		archetype->RemoveEntities(Stream<unsigned int>(&entity, 1), m_entity_pool, deleted_sequence_stream);
		for (size_t index = 0; index < deleted_sequence_stream.size; index++) {
			m_entity_pool->FreeSequence(deleted_sequence_stream[index]);
		}

		return new_entity;
	}

	unsigned int EntityManager::AddComponent(const Stream<unsigned int>& entities, unsigned char component) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entities[0]);
		ArchetypeBase* archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		void* data_array[32];
		unsigned char components[32];
		unsigned char shared_components[32];
		unsigned short shared_subindices[32];
		m_archetypes[info.archetype].GetArchetypeSharedSubindices(info.archetype_subindex, shared_subindices);

		memcpy(
			components,
			m_archetype_info[info.archetype].components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].components[0] + 1
		);
		unsigned char copy_count = components[0];
		components[0]++;
		components[components[0]] = component;

		memcpy(
			shared_components,
			m_archetype_info[info.archetype].shared_components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].shared_components[0] + 1
		);

		unsigned int new_first = m_entity_pool->GetSequenceFast(entities.size);
		unsigned int new_archetype_index = FindArchetype(components, shared_components);

		unsigned int new_archetype_subindex;
		ArchetypeBase* new_archetype_base = m_archetypes[new_archetype_index].FindArchetypeBase(shared_subindices, new_archetype_subindex);
		if (new_archetype_base == nullptr) {
			new_archetype_base = m_archetypes[new_archetype_index].CreateBaseArchetype(shared_subindices);
			new_archetype_subindex = m_archetypes[new_archetype_index].GetArchetypeBaseCount() - 1;
		}

		unsigned char archetype_component[32];
		for (size_t index = 1; index <= copy_count; index++) {
			archetype_component[index - 1] = new_archetype_base->FindComponentIndex(components[index]);
		}

		unsigned int _offsets[256];
		unsigned int sizes[256];
		unsigned int sequences[256];
		Stream<unsigned int> offsets(_offsets, 0);
		new_archetype_base->AddEntities(new_first, entities.size, offsets, sizes, sequences);

		EntityInfo new_info;
		new_info.archetype = new_archetype_index;
		new_info.archetype_subindex = new_archetype_subindex;
		void* entity_data[32];
		size_t entity_offset = 0;

		Stream<void*> buffer_stream;
		archetype->GetBuffers(buffer_stream);

		for (size_t index = 0; index < offsets.size; index++) {
			new_info.chunk = index;
			new_info.sequence = sequences[index];
			m_entity_pool->SetEntityInfoToSequence(new_first + offsets[index], sizes[index], new_info);
			unsigned int first_entity_in_sequence_index = new_archetype_base->GetEntityIndex(new_first + entity_offset, new_info);
			for (size_t entity_index = 0; entity_index < sizes[index]; entity_index++) {
				archetype->GetComponent(
					entities[entity_index + entity_offset], 
					m_entity_pool->GetEntityInfo(entities[entity_index + entity_offset]),
					entity_data
				);
				for (size_t component_index = 0; component_index < copy_count; component_index++) {
					new_archetype_base->UpdateComponent(
						first_entity_in_sequence_index + entity_index,
						archetype_component[component_index],
						entity_data[component_index],
						new_info,
						unsigned int(1)
					);
				}
			}
			entity_offset += sizes[index];
		}

		unsigned int deleted_sequence_first[32];
		Stream<unsigned int> deleted_sequence_stream(deleted_sequence_first, 0);
		archetype->RemoveEntities(entities, m_entity_pool, deleted_sequence_stream);
		for (size_t index = 0; index < deleted_sequence_stream.size; index++) {
			m_entity_pool->FreeSequence(deleted_sequence_stream[index]);
		}

		return new_first;
	}

	unsigned int EntityManager::AddComponent(const Stream<unsigned int>& entities, unsigned char component, void* data) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entities[0]);
		ArchetypeBase* archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		void* data_array[32];
		unsigned char components[32];
		unsigned char shared_components[32];
		unsigned short shared_subindices[32];
		m_archetypes[info.archetype].GetArchetypeSharedSubindices(info.archetype_subindex, shared_subindices);

		memcpy(
			components,
			m_archetype_info[info.archetype].components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].components[0] + 1
		);
		unsigned char copy_count = components[0];
		components[0];
		components[components[0]] = component;

		memcpy(
			shared_components,
			m_archetype_info[info.archetype].shared_components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].shared_components[0] + 1
		);

		unsigned int new_first = m_entity_pool->GetSequenceFast(entities.size);
		unsigned int new_archetype_index = FindArchetype(components, shared_components);

		unsigned int new_archetype_subindex;
		ArchetypeBase* new_archetype_base = m_archetypes[new_archetype_index].FindArchetypeBase(shared_subindices, new_archetype_subindex);
		if (new_archetype_base == nullptr) {
			new_archetype_base = m_archetypes[new_archetype_index].CreateBaseArchetype(shared_subindices);
			new_archetype_subindex = m_archetypes[new_archetype_index].GetArchetypeBaseCount() - 1;
		}

		unsigned char archetype_component[32];
		for (size_t index = 1; index <= copy_count; index++) {
			archetype_component[index - 1] = new_archetype_base->FindComponentIndex(components[index]);
		}

		unsigned int _offsets[256];
		unsigned int sizes[256];
		unsigned int sequences[256];
		Stream<unsigned int> offsets(_offsets, 0);
		new_archetype_base->AddEntities(new_first, entities.size, offsets, sizes, sequences);

		EntityInfo new_info;
		new_info.archetype = new_archetype_index;
		new_info.archetype_subindex = new_archetype_subindex;
		void* entity_data[32];
		size_t entity_offset = 0;

		void* archetype_component_to_add_buffer[32];
		unsigned char search_component[2] = { 1, component };
		archetype->GetBuffers(Stream<void*>(archetype_component_to_add_buffer, 0), search_component);

		unsigned int component_to_add_size = m_unique_components[component].size;
		for (size_t index = 0; index < offsets.size; index++) {
			new_info.chunk = index;
			new_info.sequence = sequences[index];
			m_entity_pool->SetEntityInfoToSequence(new_first + offsets[index], sizes[index], new_info);
			unsigned int first_entity_in_sequence_index = new_archetype_base->GetEntityIndex(new_first + entity_offset, new_info);
			for (size_t entity_index = 0; entity_index < sizes[index]; entity_index++) {
				archetype->GetComponent(
					entities[entity_index + entity_offset],
					m_entity_pool->GetEntityInfo(entities[entity_index + entity_offset]),
					entity_data
				);
				for (size_t component_index = 0; component_index < copy_count; component_index++) {
					new_archetype_base->UpdateComponent(
						first_entity_in_sequence_index + entity_index,
						archetype_component[component_index],
						entity_data[component_index],
						new_info,
						unsigned int(1)
					);
				}
			}
			memcpy(
				(void*)((uintptr_t)archetype_component_to_add_buffer[index] + first_entity_in_sequence_index * component_to_add_size),
				(void*)((uintptr_t)data + entity_offset * component_to_add_size),
				sizes[index] * component_to_add_size
			);
			entity_offset += sizes[index];
		}

		unsigned int deleted_sequence_first[32];
		Stream<unsigned int> deleted_sequence_stream(deleted_sequence_first, 0);
		archetype->RemoveEntities(entities, m_entity_pool, deleted_sequence_stream);
		for (size_t index = 0; index < deleted_sequence_stream.size; index++) {
			m_entity_pool->FreeSequence(deleted_sequence_stream[index]);
		}

		return new_first;
	}

	unsigned int EntityManager::AddComponent(const Stream<unsigned int>& entities, const unsigned char* component_order) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entities[0]);
		ArchetypeBase* archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		void* data_array[32];
		unsigned char components[32];
		unsigned char shared_components[32];
		unsigned short shared_subindices[32];
		m_archetypes[info.archetype].GetArchetypeSharedSubindices(info.archetype_subindex, shared_subindices);

		memcpy(
			components,
			m_archetype_info[info.archetype].components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].components[0] + 1
		);
		unsigned char copy_count = components[0];
		for (size_t index = 1; index <= component_order[0]; index++) {
			components[components[0] + index] = component_order[index];
		}
		components[0] += component_order[0];

		memcpy(
			shared_components,
			m_archetype_info[info.archetype].shared_components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].shared_components[0] + 1
		);

		unsigned int new_first = m_entity_pool->GetSequenceFast(entities.size);
		unsigned int new_archetype_index = FindArchetype(components, shared_components);

		unsigned int new_archetype_subindex;
		ArchetypeBase* new_archetype_base = m_archetypes[new_archetype_index].FindArchetypeBase(shared_subindices, new_archetype_subindex);
		if (new_archetype_base == nullptr) {
			new_archetype_base = m_archetypes[new_archetype_index].CreateBaseArchetype(shared_subindices);
			new_archetype_subindex = m_archetypes[new_archetype_index].GetArchetypeBaseCount() - 1;
		}

		unsigned char archetype_component[32];
		for (size_t index = 1; index <= copy_count; index++) {
			archetype_component[index - 1] = new_archetype_base->FindComponentIndex(components[index]);
		}

		unsigned int _offsets[256];
		unsigned int sizes[256];
		unsigned int sequences[256];
		Stream<unsigned int> offsets(_offsets, 0);
		new_archetype_base->AddEntities(new_first, entities.size, offsets, sizes, sequences);

		EntityInfo new_info;
		new_info.archetype = new_archetype_index;
		new_info.archetype_subindex = new_archetype_subindex;
		void* entity_data[32];
		size_t entity_offset = 0;

		Stream<void*> buffer_stream;
		archetype->GetBuffers(buffer_stream);

		for (size_t index = 0; index < offsets.size; index++) {
			new_info.chunk = index;
			new_info.sequence = sequences[index];
			m_entity_pool->SetEntityInfoToSequence(new_first + offsets[index], sizes[index], new_info);
			unsigned int first_entity_in_sequence_index = new_archetype_base->GetEntityIndex(new_first + entity_offset, new_info);
			for (size_t entity_index = 0; entity_index < sizes[index]; entity_index++) {
				archetype->GetComponent(
					entities[entity_index + entity_offset],
					m_entity_pool->GetEntityInfo(entities[entity_index + entity_offset]),
					entity_data
				);
				for (size_t component_index = 0; component_index < copy_count; component_index++) {
					new_archetype_base->UpdateComponent(
						first_entity_in_sequence_index + entity_index,
						archetype_component[component_index],
						entity_data[component_index],
						new_info,
						true
					);
				}
			}
			entity_offset += sizes[index];
		}

		unsigned int deleted_sequence_first[32];
		Stream<unsigned int> deleted_sequence_stream(deleted_sequence_first, 0);
		archetype->RemoveEntities(entities, m_entity_pool, deleted_sequence_stream);
		for (size_t index = 0; index < deleted_sequence_stream.size; index++) {
			m_entity_pool->FreeSequence(deleted_sequence_stream[index]);
		}

		return new_first;
	}

	unsigned int EntityManager::AddComponent(const Stream<unsigned int>& entities, const unsigned char* component_order, void** data) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entities[0]);
		ArchetypeBase* archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		void* data_array[32];
		unsigned char components[32];
		unsigned char shared_components[32];
		unsigned short shared_subindices[32];
		m_archetypes[info.archetype].GetArchetypeSharedSubindices(info.archetype_subindex, shared_subindices);

		memcpy(
			components,
			m_archetype_info[info.archetype].components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].components[0] + 1
		);
		unsigned char copy_count = components[0];
		for (size_t index = 1; index <= component_order[0]; index++) {
			components[components[0] + index] = component_order[index];
		}
		components[0] += component_order[0];

		memcpy(
			shared_components,
			m_archetype_info[info.archetype].shared_components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].shared_components[0] + 1
		);

		unsigned int new_first = m_entity_pool->GetSequenceFast(entities.size);
		unsigned int new_archetype_index = FindArchetype(components, shared_components);

		unsigned int new_archetype_subindex;
		ArchetypeBase* new_archetype_base = m_archetypes[new_archetype_index].FindArchetypeBase(shared_subindices, new_archetype_subindex);
		if (new_archetype_base == nullptr) {
			new_archetype_base = m_archetypes[new_archetype_index].CreateBaseArchetype(shared_subindices);
			new_archetype_subindex = m_archetypes[new_archetype_index].GetArchetypeBaseCount() - 1;
		}

		unsigned char archetype_component[32];
		for (size_t index = 1; index <= copy_count; index++) {
			archetype_component[index - 1] = new_archetype_base->FindComponentIndex(components[index]);
		}

		unsigned int _offsets[256];
		unsigned int sizes[256];
		unsigned int sequences[256];
		Stream<unsigned int> offsets(_offsets, 0);
		new_archetype_base->AddEntities(new_first, entities.size, offsets, sizes, sequences);

		EntityInfo new_info;
		new_info.archetype = new_archetype_index;
		new_info.archetype_subindex = new_archetype_subindex;
		void* entity_data[32];
		size_t entity_offset = 0;

		void* archetype_component_to_add_buffers[512];
		archetype->GetBuffers(Stream<void*>(archetype_component_to_add_buffers, 0), component_order);

		unsigned int component_to_add_size[32];
		for (size_t index = 1; index <= component_order[0]; index++) {
			component_to_add_size[index - 1] = m_unique_components[component_order[index]].size;
		}
		for (size_t index = 0; index < offsets.size; index++) {
			new_info.chunk = index;
			new_info.sequence = sequences[index];
			m_entity_pool->SetEntityInfoToSequence(new_first + offsets[index], sizes[index], new_info);
			unsigned int first_entity_in_sequence_index = new_archetype_base->GetEntityIndex(new_first + entity_offset, new_info);
			for (size_t entity_index = 0; entity_index < sizes[index]; entity_index++) {
				archetype->GetComponent(
					entities[entity_index + entity_offset],
					m_entity_pool->GetEntityInfo(entities[entity_index + entity_offset]),
					entity_data
				);
				for (size_t component_index = 0; component_index < copy_count; component_index++) {
					new_archetype_base->UpdateComponent(
						first_entity_in_sequence_index + entity_index,
						archetype_component[component_index],
						entity_data[component_index],
						new_info,
						unsigned int(1)
					);
				}
			}
			for (size_t subindex = 0; subindex < component_order[0]; subindex++) {
				memcpy(
					(void*)((uintptr_t)archetype_component_to_add_buffers[index + offsets.size * subindex] 
						+ first_entity_in_sequence_index * component_to_add_size[subindex]),
					(void*)((uintptr_t)data[subindex] + entity_offset * component_to_add_size[subindex]),
					sizes[index] * component_to_add_size[subindex]
				);
			}
			entity_offset += sizes[index];
		}

		unsigned int deleted_sequence_first[32];
		Stream<unsigned int> deleted_sequence_stream(deleted_sequence_first, 0);
		archetype->RemoveEntities(entities, m_entity_pool, deleted_sequence_stream);
		for (size_t index = 0; index < deleted_sequence_stream.size; index++) {
			m_entity_pool->FreeSequence(deleted_sequence_stream[index]);
		}

		return new_first;
	}

	unsigned int EntityManager::AddComponent(const Stream<unsigned int>& entities, const unsigned char* component_order, void** data, bool non_contiguous) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entities[0]);
		ArchetypeBase* archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		void* data_array[32];
		unsigned char components[32];
		unsigned char shared_components[32];
		unsigned short shared_subindices[32];
		m_archetypes[info.archetype].GetArchetypeSharedSubindices(info.archetype_subindex, shared_subindices);

		memcpy(
			components,
			m_archetype_info[info.archetype].components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].components[0] + 1
		);
		for (size_t index = 1; index <= component_order[0]; index++) {
			components[components[0] + index] = component_order[index];
		}
		unsigned char initial_component_count = components[0];
		components[0] += component_order[0];
		unsigned char copy_count = components[0];

		memcpy(
			shared_components,
			m_archetype_info[info.archetype].shared_components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].shared_components[0] + 1
		);

		unsigned int new_first = m_entity_pool->GetSequenceFast(entities.size);
		unsigned int new_archetype_index = FindArchetype(components, shared_components);

		unsigned int new_archetype_subindex;
		ArchetypeBase* new_archetype_base = m_archetypes[new_archetype_index].FindArchetypeBase(shared_subindices, new_archetype_subindex);
		if (new_archetype_base == nullptr) {
			new_archetype_base = m_archetypes[new_archetype_index].CreateBaseArchetype(shared_subindices);
			new_archetype_subindex = m_archetypes[new_archetype_index].GetArchetypeBaseCount() - 1;
		}

		unsigned char archetype_component[32];
		for (size_t index = 1; index <= copy_count; index++) {
			archetype_component[index - 1] = new_archetype_base->FindComponentIndex(components[index]);
		}

		unsigned int _offsets[256];
		unsigned int sizes[256];
		unsigned int sequences[256];
		Stream<unsigned int> offsets(_offsets, 0);
		new_archetype_base->AddEntities(new_first, entities.size, offsets, sizes, sequences);

		EntityInfo new_info;
		new_info.archetype = new_archetype_index;
		new_info.archetype_subindex = new_archetype_subindex;
		void* entity_data[32];
		size_t entity_offset = 0;

		for (size_t index = 0; index < offsets.size; index++) {
			new_info.chunk = index;
			new_info.sequence = sequences[index];
			m_entity_pool->SetEntityInfoToSequence(new_first + offsets[index], sizes[index], new_info);
			unsigned int first_entity_in_sequence_index = new_archetype_base->GetEntityIndex(new_first + entity_offset, new_info);
			for (size_t entity_index = 0; entity_index < sizes[index]; entity_index++) {
				archetype->GetComponent(
					entities[entity_index + entity_offset],
					m_entity_pool->GetEntityInfo(entities[entity_index + entity_offset]),
					entity_data
				);
				for (size_t component_index = 0; component_index < initial_component_count; component_index++) {
					entity_data[component_index + initial_component_count + 1] = data[entity_offset + entity_index + component_index * entities.size];
				}
				for (size_t component_index = 0; component_index < copy_count; component_index++) {
					new_archetype_base->UpdateComponent(
						first_entity_in_sequence_index + entity_index,
						archetype_component[component_index],
						entity_data[component_index],
						new_info,
						unsigned int(1)
					);
				}
			}
			entity_offset += sizes[index];
		}

		unsigned int deleted_sequence_first[32];
		Stream<unsigned int> deleted_sequence_stream(deleted_sequence_first, 0);
		archetype->RemoveEntities(entities, m_entity_pool, deleted_sequence_stream);
		for (size_t index = 0; index < deleted_sequence_stream.size; index++) {
			m_entity_pool->FreeSequence(deleted_sequence_stream[index]);
		}

		return new_first;
	}

	unsigned int EntityManager::AddComponent(const Stream<unsigned int>& entities, const unsigned char* component_order, void** data, unsigned int non_contiguous_and_parsed_by_entity) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entities[0]);
		ArchetypeBase* archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		void* data_array[32];
		unsigned char components[32];
		unsigned char shared_components[32];
		unsigned short shared_subindices[32];
		m_archetypes[info.archetype].GetArchetypeSharedSubindices(info.archetype_subindex, shared_subindices);

		memcpy(
			components,
			m_archetype_info[info.archetype].components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].components[0] + 1
		);
		for (size_t index = 1; index <= component_order[0]; index++) {
			components[components[0] + index] = component_order[index];
		}
		unsigned char initial_component_count = components[0];
		components[0] += component_order[0];
		unsigned char copy_count = components[0];

		memcpy(
			shared_components,
			m_archetype_info[info.archetype].shared_components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].shared_components[0] + 1
		);

		unsigned int new_first = m_entity_pool->GetSequenceFast(entities.size);
		unsigned int new_archetype_index = FindArchetype(components, shared_components);

		unsigned int new_archetype_subindex;
		ArchetypeBase* new_archetype_base = m_archetypes[new_archetype_index].FindArchetypeBase(shared_subindices, new_archetype_subindex);
		if (new_archetype_base == nullptr) {
			new_archetype_base = m_archetypes[new_archetype_index].CreateBaseArchetype(shared_subindices);
			new_archetype_subindex = m_archetypes[new_archetype_index].GetArchetypeBaseCount() - 1;
		}

		unsigned char archetype_component[32];
		for (size_t index = 1; index <= copy_count; index++) {
			archetype_component[index - 1] = new_archetype_base->FindComponentIndex(components[index]);
		}

		unsigned int _offsets[256];
		unsigned int sizes[256];
		unsigned int sequences[256];
		Stream<unsigned int> offsets(_offsets, 0);
		new_archetype_base->AddEntities(new_first, entities.size, offsets, sizes, sequences);

		EntityInfo new_info;
		new_info.archetype = new_archetype_index;
		new_info.archetype_subindex = new_archetype_subindex;
		void* entity_data[32];
		size_t entity_offset = 0;

		for (size_t index = 0; index < offsets.size; index++) {
			new_info.chunk = index;
			new_info.sequence = sequences[index];
			m_entity_pool->SetEntityInfoToSequence(new_first + offsets[index], sizes[index], new_info);
			unsigned int first_entity_in_sequence_index = new_archetype_base->GetEntityIndex(new_first + entity_offset, new_info);
			for (size_t entity_index = 0; entity_index < sizes[index]; entity_index++) {
				archetype->GetComponent(
					entities[entity_index + entity_offset],
					m_entity_pool->GetEntityInfo(entities[entity_index + entity_offset]),
					entity_data
				);
				for (size_t component_index = 0; component_index < initial_component_count; component_index++) {
					entity_data[component_index + initial_component_count + 1] = data[(entity_offset + entity_index) * component_order[0] + component_index];
				}
				for (size_t component_index = 0; component_index < copy_count; component_index++) {
					new_archetype_base->UpdateComponent(
						first_entity_in_sequence_index + entity_index,
						archetype_component[component_index],
						entity_data[component_index],
						new_info,
						unsigned int(1)
					);
				}
			}
			entity_offset += sizes[index];
		}

		unsigned int deleted_sequence_first[32];
		Stream<unsigned int> deleted_sequence_stream(deleted_sequence_first, 0);
		archetype->RemoveEntities(entities, m_entity_pool, deleted_sequence_stream);
		for (size_t index = 0; index < deleted_sequence_stream.size; index++) {
			m_entity_pool->FreeSequence(deleted_sequence_stream[index]);
		}

		return new_first;
	}

	unsigned int EntityManager::AddSharedComponent(const Stream<unsigned int>& entities, const unsigned char* component_order, const unsigned short* subindices) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entities[0]);
		ArchetypeBase* archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		void* data_array[32];
		unsigned char components[32];
		unsigned char shared_components[32];
		unsigned short shared_subindices[32];
		m_archetypes[info.archetype].GetArchetypeSharedSubindices(info.archetype_subindex, shared_subindices);

		memcpy(
			components,
			m_archetype_info[info.archetype].components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].components[0] + 1
		);

		memcpy(
			shared_components,
			m_archetype_info[info.archetype].shared_components,
			sizeof(unsigned char) * m_archetype_info[info.archetype].shared_components[0] + 1
		);

		for (size_t index = 1; index <= component_order[0]; index++) {
			shared_components[shared_components[0] + index] = component_order[index];
			shared_subindices[shared_components[0] + index - 1] = subindices[index - 1];
		}
		shared_components[0] += component_order[0];

		unsigned int new_first = m_entity_pool->GetSequenceFast(entities.size);
		unsigned int new_archetype_index = FindArchetype(components, shared_components);

		unsigned int new_archetype_subindex;
		ArchetypeBase* new_archetype_base = m_archetypes[new_archetype_index].FindArchetypeBase(shared_subindices, new_archetype_subindex);
		if (new_archetype_base == nullptr) {
			new_archetype_base = m_archetypes[new_archetype_index].CreateBaseArchetype(shared_subindices);
			new_archetype_subindex = m_archetypes[new_archetype_index].GetArchetypeBaseCount() - 1;
		}

		unsigned char archetype_component[32];
		for (size_t index = 1; index <= components[0]; index++) {
			archetype_component[index - 1] = new_archetype_base->FindComponentIndex(components[index]);
		}

		unsigned int _offsets[256];
		unsigned int sizes[256];
		unsigned int sequences[256];
		Stream<unsigned int> offsets(_offsets, 0);
		new_archetype_base->AddEntities(new_first, entities.size, offsets, sizes, sequences);

		EntityInfo new_info;
		new_info.archetype = new_archetype_index;
		new_info.archetype_subindex = new_archetype_subindex;
		void* entity_data[32];
		size_t entity_offset = 0;

		for (size_t index = 0; index < offsets.size; index++) {
			new_info.chunk = index;
			new_info.sequence = sequences[index];
			m_entity_pool->SetEntityInfoToSequence(new_first + offsets[index], sizes[index], new_info);
			unsigned int first_entity_in_sequence_index = new_archetype_base->GetEntityIndex(new_first + entity_offset, new_info);
			for (size_t entity_index = 0; entity_index < sizes[index]; entity_index++) {
				archetype->GetComponent(
					entities[entity_index + entity_offset],
					m_entity_pool->GetEntityInfo(entities[entity_index + entity_offset]),
					entity_data
				);
				for (size_t component_index = 0; component_index < components[0]; component_index++) {
					new_archetype_base->UpdateComponent(
						first_entity_in_sequence_index + entity_index,
						archetype_component[component_index],
						entity_data[component_index],
						new_info,
						unsigned int(1)
					);
				}
			}
			entity_offset += sizes[index];
		}

		return new_first;
	}

	unsigned int EntityManager::CreateArchetype(
		ArchetypeBaseDescriptor descriptor,
		const unsigned char* components, 
		const unsigned char* shared_components, 
		unsigned int max_base_archetypes,
		MemoryManager* memory_manager
	) {
		unsigned int archetype_index = FindArchetype(components, shared_components);
		if (archetype_index != 0xFFFFFFFF) {
			// error handling - Recreating the archetype
			ECS_ASSERT(false, "Recreating the archetype");
			return 0xFFFFFFFF;
		}
		else {
			ComponentInfo info[32];
			ComponentInfo shared_info[32];
			for (size_t index = 1; index <= components[0]; index++) {
				info[index - 1] = m_unique_components[components[index]];
			}
			for (size_t index = 1; index <= shared_components[0]; index++) {
				shared_info[index - 1] = m_shared_components[shared_components[index]];
			}
			if (memory_manager == nullptr) {
				m_archetypes[m_archetypes.size++] = Archetype<ArchetypeBase>(
					max_base_archetypes, 
					m_memory_manager,
					Stream<ComponentInfo>(info, components[0]),
					Stream<ComponentInfo>(shared_info, shared_components[0])
				);
			}
			else {
				m_archetypes[m_archetypes.size++] = Archetype<ArchetypeBase>(
					max_base_archetypes, 
					memory_manager,
					Stream<ComponentInfo>(info, components[0]),
					Stream<ComponentInfo>(shared_info, shared_components[0])
				);
			}
			memcpy(m_archetype_info[m_archetypes.size - 1].components, components, sizeof(unsigned char) * (components[0] + 1));
			memcpy(
				m_archetype_info[m_archetypes.size - 1].shared_components, 
				shared_components,
				sizeof(unsigned char) * (shared_components[0] + 1)
			);
			return m_archetypes.size - 1;
		}
	}

	unsigned int EntityManager::CreateArchetypeBase(
		ArchetypeBaseDescriptor descriptor, 
		const unsigned char* components, 
		const unsigned char* shared_components, 
		const unsigned short* shared_subindices
	) {
		unsigned int archetype_index = FindArchetype(components, shared_components);
		if (archetype_index == 0xFFFFFFFF) {
			// error handling - Recreating the archetype
			ECS_ASSERT(false, "Recreating the archetype");
			return 0xFFFFFFFF;
		}
		else {
			return CreateArchetypeBase(descriptor, archetype_index, shared_components, shared_subindices);
		}
	}

	unsigned int EntityManager::CreateArchetypeBase(
		ArchetypeBaseDescriptor descriptor,
		unsigned int archetype_index,
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) {
		m_archetypes[archetype_index].CreateBaseArchetype(shared_components, descriptor, shared_subindices);
		return m_archetypes[archetype_index].GetArchetypeBaseCount() - 1;
	}

	unsigned int EntityManager::CreateStaticArchetype(
		ArchetypeBaseDescriptor descriptor,
		const unsigned char* components,
		const unsigned char* shared_components,
		unsigned int max_base_archetypes,
		MemoryManager* memory_manager
	) {
		unsigned int archetype_index = FindStaticArchetype(components, shared_components);
		if (archetype_index != 0xFFFFFFFF) {
			// error handling - Recreating static archetype
			ECS_ASSERT(false, "Recreating the static archetype");
			return 0xFFFFFFFF;
		}
		else {
			ComponentInfo info[32];
			ComponentInfo shared_info[32];
			for (size_t index = 1; index <= components[0]; index++) {
				info[index - 1] = m_unique_components[components[index]];
			}
			for (size_t index = 1; index <= shared_components[0]; index++) {
				shared_info[index - 1] = m_shared_components[shared_components[index]];
			}
			if (memory_manager == nullptr) {
				m_static_archetypes[m_static_archetypes.size++] = Archetype<StaticArchetypeBase>(
					max_base_archetypes, m_memory_manager,
					Stream<ComponentInfo>(info, components[0]),
					Stream<ComponentInfo>(shared_info, shared_components[0])
				);
			}
			else {
				m_static_archetypes[m_static_archetypes.size++] = Archetype<StaticArchetypeBase>(
					max_base_archetypes, memory_manager,
					Stream<ComponentInfo>(info, components[0]),
					Stream<ComponentInfo>(shared_info, shared_components[0])
				);
			}
			memcpy(m_archetype_info[m_archetypes.size - 1].components, components, sizeof(unsigned char) * (components[0] + 1));
			memcpy(
				m_archetype_info[m_archetypes.size - 1].shared_components,
				shared_components,
				sizeof(unsigned char) * (shared_components[0] + 1)
			);
			return m_archetypes.size - 1;
		}
	}

	unsigned int EntityManager::CreateStaticArchetypeBase(
		ArchetypeBaseDescriptor descriptor,
		const unsigned char* components,
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) {
		unsigned int archetype_index = FindStaticArchetype(components, shared_components);
		if (archetype_index == 0xFFFFFFFF) {
			// error handling - "Trying to create base static archetype when there is no such archetype"
			ECS_ASSERT(false, "Trying to create base static archetype when there is no such archetype");
			return archetype_index;
		}
		else {
			CreateStaticArchetypeBase(descriptor, archetype_index, shared_components, shared_subindices);
		}
	}

	unsigned int EntityManager::CreateStaticArchetypeBase(
		ArchetypeBaseDescriptor descriptor,
		unsigned int archetype_index,
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) {
		m_static_archetypes[archetype_index].CreateBaseArchetype(shared_components, descriptor, shared_subindices);
		return m_static_archetypes[archetype_index].GetArchetypeBaseCount() - 1;
	}
	
	unsigned int EntityManager::CreateEntities(unsigned int count) {
		unsigned int first = m_entity_pool->GetSequence(count);
		return first;
	}

	unsigned int EntityManager::CreateEntities(unsigned int count, unsigned int archetype_index, unsigned int archetype_base_index) {
		unsigned int first = m_entity_pool->GetSequence(count);
		ArchetypeBase* archetype_base = m_archetypes[archetype_index].GetArchetypeBase(archetype_base_index);

		unsigned int _offset[256];
		unsigned int sizes[256];
		unsigned int sequence_index[256];
		Stream<unsigned int> offsets(_offset, 0);
		archetype_base->AddEntities(first, count, offsets, sizes, sequence_index);

		EntityInfo info;
		info.archetype = archetype_index;
		info.archetype_subindex = archetype_base_index;
		size_t entity_offset = 0;

		for (size_t index = 0; index < offsets.size; index++) {
			info.chunk = index;
			info.sequence = sequence_index[index];
			m_entity_pool->SetEntityInfoToSequence(first + entity_offset, sizes[index], info);
			entity_offset += sizes[index];
		}
		return first;
	}

	unsigned int EntityManager::CreateEntities(
		unsigned int count, 
		const unsigned char* components, 
		const unsigned char* shared_components, 
		const unsigned short* shared_subindices
	) {
		return CreateEntities(
			count, 
			VectorComponentSignature(components), 
			VectorComponentSignature(shared_components), 
			components, 
			shared_components, 
			shared_subindices
		);
	}

	unsigned int EntityManager::CreateEntities(
		unsigned int count,
		VectorComponentSignature signature,
		VectorComponentSignature shared_signature,
		const unsigned char* components,
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) {
		unsigned int archetype_index = FindArchetype(signature, shared_signature);
		unsigned int archetype_subindex;
		if (archetype_index == 0xFFFFFFFF) {
			// error handling - THIS IS A WARN - "Trying to create entities that do not have archetype!"
			assert(false);
			ArchetypeBaseDescriptor descriptor;
			descriptor.chunk_size = count;
			descriptor.initial_chunk_count = ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_INITIAL_CHUNK_COUNT;
			descriptor.max_chunk_count = ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_CHUNK_COUNT;
			descriptor.max_sequence_count = ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_SEQUENCE_COUNT;
			archetype_index = CreateArchetype(
				descriptor,
				components,
				shared_components,
				ECS_ENTITY_MANAGER_DEFAULT_MAX_ARCHETYPE_COUNT_FOR_NEW_ARCHETYPE
			);
			ArchetypeBase* base_archetype = m_archetypes[archetype_index].CreateBaseArchetype(
				shared_components,
				shared_subindices
			);
			return CreateEntities(count, archetype_index, 0);
		}
		else {
			unsigned int archetype_subindex;
			m_archetypes[archetype_index].FindArchetypeBase(shared_subindices, shared_components, archetype_subindex);
			if (archetype_subindex == 0xFFFFFFFF) {
				// warn handling - "Trying to create entities that do not have a base archetype!"
				assert(false);
				m_archetypes[archetype_index].CreateBaseArchetype(shared_components, shared_subindices);
				archetype_subindex = m_archetypes[archetype_index].GetArchetypeBaseCount() - 1;
			}
			return CreateEntities(count, archetype_index, archetype_subindex);
		}
	}

	unsigned int EntityManager::CreateStaticEntities(unsigned int count, unsigned int archetype_index, unsigned int archetype_subindex) {
		unsigned int first = m_entity_pool->GetSequence(count);
		StaticArchetypeBase* archetype_base = m_static_archetypes[archetype_index].GetArchetypeBase(archetype_subindex);

		unsigned int _offset[256];
		unsigned int sizes[256];
		unsigned int sequence_index[256];
		Stream<unsigned int> offsets(_offset, 0);
		archetype_base->AddEntities(first, count, offsets, sizes, sequence_index);
		
		EntityInfo info;
		info.archetype = archetype_index;
		info.archetype_subindex = archetype_subindex;

		size_t entity_offset = 0;
		for (size_t index = 0; index < offsets.size; index++) {
			info.chunk = index;
			info.sequence = sequence_index[index];
			m_entity_pool->SetEntityInfoToSequence(first + entity_offset, sizes[index], info);
			entity_offset += sizes[index];
		}
		return first;
	}

	unsigned int EntityManager::CreateStaticEntities(
		unsigned int count, 
		const unsigned char* components, 
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) {
		return CreateStaticEntities(
			count, 
			VectorComponentSignature(components), 
			VectorComponentSignature(shared_components), 
			components, 
			shared_components, 
			shared_subindices
		);
	}

	unsigned int EntityManager::CreateStaticEntities(
		unsigned int count, 
		VectorComponentSignature signature,
		VectorComponentSignature shared_signature,
		const unsigned char* components,
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) {
		unsigned int archetype_index = FindStaticArchetype(signature, shared_signature);
		unsigned int archetype_subindex;
		if (archetype_index == 0xFFFFFFFF) {
			// warn handling - "Trying to create static entities that do not have archetype!"
			ECS_ASSERT(false, "Trying to create static entities that do not have archetype!");
			ArchetypeBaseDescriptor descriptor;
			descriptor.chunk_size = count;
			descriptor.initial_chunk_count = ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_INITIAL_CHUNK_COUNT;
			descriptor.max_chunk_count = ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_CHUNK_COUNT;
			descriptor.max_sequence_count = ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_SEQUENCE_COUNT;
			unsigned int static_archetype_index = CreateStaticArchetype(
				descriptor,
				components,
				shared_components,
				ECS_ENTITY_MANAGER_DEFAULT_MAX_ARCHETYPE_COUNT_FOR_NEW_ARCHETYPE
			);
			StaticArchetypeBase* base_archetype = m_static_archetypes[static_archetype_index].CreateBaseArchetype(
				shared_components,
				shared_subindices
			);
			return CreateStaticEntities(count, static_archetype_index, 0);
		}
		else {
			unsigned int archetype_subindex;
			m_static_archetypes[archetype_index].FindArchetypeBase(shared_subindices, shared_components, archetype_subindex);
			if (archetype_subindex == 0xFFFFFFFF) {
				// warn handling - "Trying to create static entities that do not have a base archetype!"
				ECS_ASSERT(false, "Trying to create static entities that do not have a base archetype!");
				m_static_archetypes[archetype_index].CreateBaseArchetype(shared_components, shared_subindices);
				archetype_subindex = m_static_archetypes[archetype_index].GetArchetypeBaseCount() - 1;
			}
			return CreateStaticEntities(count, archetype_index, archetype_subindex);
		}
	}

	void EntityManager::DeleteEntity(unsigned int entity) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		ArchetypeBase* base_archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		unsigned int deleted_sequence_first;
		Stream<unsigned int> entity_stream(&entity, 1);
		Stream<unsigned int> deleted_sequence_stream(&deleted_sequence_first, 0);
		base_archetype->RemoveEntities(entity_stream, m_entity_pool, deleted_sequence_stream);

		if (deleted_sequence_stream.size == 1) {
			m_entity_pool->FreeSequence(deleted_sequence_first);
		}
	}

	void EntityManager::DeleteEntities(const Stream<unsigned int>& entities) {
		for (size_t index = 0; index < entities.size; index++) {
			DeleteEntity(entities[index]);
		}
	}

	void EntityManager::DeleteEntities(const Stream<unsigned int>& entities, bool same_base_archetype) {
		EntityInfo info = m_entity_pool->GetEntityInfo(entities[0]);
		ArchetypeBase* base_archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

		unsigned int deleted_sequences[1024];
		Stream<unsigned int> deleted_sequences_stream(deleted_sequences, 0);
		base_archetype->RemoveEntities(entities, m_entity_pool, deleted_sequences_stream);

		for (size_t index = 0; index < deleted_sequences_stream.size; index++) {
			m_entity_pool->FreeSequence(deleted_sequences_stream[index]);
		}
	}

	void EntityManager::DeleteStaticEntities(const Stream<unsigned int>& entities) {
		for (size_t index = 0; index < entities.size; index++) {
			EntityInfo info = m_entity_pool->GetEntityInfo(entities[index]);
			StaticArchetypeBase* base_archetype = m_static_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);

			base_archetype->DeleteSequence(info.chunk, info.sequence);
			m_entity_pool->FreeSequence(entities[index]);
		}
	}

	void EntityManager::DestroyArchetype(unsigned int archetype_index) {
		// free entities from the entity pool
		unsigned int archetype_count = m_archetypes[archetype_index].GetArchetypeBaseCount();

		// no need to allocate memory since it will point to the same buffers as those inside archetypes
		Stream<Stream<Sequence>> sequences;
		unsigned int* entity_buffers[256];
		for (size_t index = 0; index < archetype_count; index++) {
			// getting sequences
			ArchetypeBase* base_archetype = m_archetypes[archetype_index].GetArchetypeBase(index);
			base_archetype->GetEntities(sequences, entity_buffers);

			// iterating over every sequence and free it from the entity pool
			for (size_t chunk_index = 0; chunk_index < sequences.size; chunk_index++) {
				for (size_t sequence_index = 0; sequence_index < sequences[chunk_index].size; sequence_index++) {
					m_entity_pool->FreeSequence(sequences[chunk_index][sequence_index].first);
				}
			}
		}
		// replacing the archetype
		m_archetypes[archetype_index].Deallocate();
		m_archetypes.size--;
		m_archetypes[archetype_index] = m_archetypes[m_archetypes.size];

		// patching up references to the archetype that replaced it
		archetype_count = m_archetypes[archetype_index].GetArchetypeBaseCount();
		EntityInfo info;
		info.archetype = archetype_index;
		for (size_t index = 0; index < archetype_count; index++) {
			ArchetypeBase* archetype_base = m_archetypes[archetype_index].GetArchetypeBase(index);
			archetype_base->GetEntities(sequences, entity_buffers);

			info.archetype_subindex = index;
			// iterating over every sequence and setting the new info the entities that are still contained in it
			for (size_t chunk_index = 0; chunk_index < sequences.size; chunk_index++) {
				info.chunk = chunk_index;
				for (size_t sequence_index = 0; sequence_index < sequences[chunk_index].size; sequence_index++) {
					info.sequence = sequence_index;
					for (size_t entity_index = 0; entity_index < sequences[chunk_index][sequence_index].size; entity_index++) {
						const Sequence* sequence_pointer = &sequences[chunk_index][sequence_index];
						m_entity_pool->SetEntityInfo(
							sequence_pointer->first + entity_buffers[chunk_index][sequence_pointer->buffer_start + entity_index],
							info
						);
					}
				}
			}
		}
	}

	void EntityManager::DestroyArchetype(const unsigned char* components, const unsigned char* shared_components) {
		unsigned int archetype_index = FindArchetype(components, shared_components);
		if (archetype_index == 0xFFFFFFFF) {
			// error handling - "Trying to destroy an archetype that does not exist"
			assert(false);
			return;
		}
		DestroyArchetype(archetype_index);
	}

	void EntityManager::DestroyArchetypeBase(unsigned int archetype_index, unsigned int archetype_subindex) {
		Stream<Stream<Sequence>> sequences;
		unsigned int* entity_buffers[256];

		// getting sequences
		ArchetypeBase* base_archetype = m_archetypes[archetype_index].GetArchetypeBase(archetype_subindex);
		base_archetype->GetEntities(sequences, entity_buffers);

		// iterating over every sequence and free it from the entity pool
		for (size_t chunk_index = 0; chunk_index < sequences.size; chunk_index++) {
			for (size_t sequence_index = 0; sequence_index < sequences[chunk_index].size; sequence_index++) {
				m_entity_pool->FreeSequence(sequences[chunk_index][sequence_index].first);
			}
		}
		m_archetypes[archetype_index].DestroyBase(archetype_subindex);

		// patching up references to the base archetype that replaced it
		base_archetype = m_archetypes[archetype_index].GetArchetypeBase(archetype_subindex);
		base_archetype->GetEntities(sequences, entity_buffers);

		EntityInfo info;
		info.archetype = archetype_index;
		info.archetype_subindex = archetype_subindex;
		for (size_t chunk_index = 0; chunk_index < sequences.size; chunk_index++) {
			info.chunk = chunk_index;
			for (size_t sequence_index = 0; sequence_index < sequences[chunk_index].size; sequence_index++) {
				info.sequence = sequence_index;
				for (size_t entity_index = 0; entity_index < sequences[chunk_index][sequence_index].size; entity_index++) {
					const Sequence* sequence_pointer = &sequences[chunk_index][sequence_index];
					m_entity_pool->SetEntityInfo(
						sequence_pointer->first + entity_buffers[chunk_index][sequence_pointer->buffer_start + entity_index],
						info
					);
				}
			}
		}
	}

	void EntityManager::DestroyArchetypeBase(
		const unsigned char* components,
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) {
		unsigned int archetype_index = FindArchetype(components, shared_components);
		if (archetype_index != 0xFFFFFFFF) {
			unsigned int archetype_subindex;
			m_archetypes[archetype_index].FindArchetypeBase(shared_subindices, shared_components, archetype_subindex);
			if (archetype_index == 0xFFFFFFFF) {
				// error handling - "Trying to destroy a base archetype that does not exist"
				ECS_ASSERT(false, "Trying to destroy a base archetypes that does not exist");
				return;
			}
			DestroyArchetypeBase(archetype_index, archetype_subindex);
		}
		// error handling - "Trying to destroy a base archetype that does not exist"
		ECS_ASSERT(false, "Trying to destroy a base archetype that does not exist");
	}

	void EntityManager::DestroyStaticArchetype(unsigned int archetype_index) {
		// free entities from the entity pool
		unsigned int archetype_count = m_static_archetypes[archetype_index].GetArchetypeBaseCount();

		// no need to allocate memory since it will point to the same buffers as those inside archetypes
		Stream<Stream<Sequence>> sequences;
		for (size_t index = 0; index < archetype_count; index++) {
			// getting sequences
			StaticArchetypeBase* base_archetype = m_static_archetypes[archetype_index].GetArchetypeBase(index);
			base_archetype->GetEntities(sequences);

			// iterating over every sequence and free it from the entity pool
			for (size_t chunk_index = 0; chunk_index < sequences.size; chunk_index++) {
				for (size_t sequence_index = 0; sequence_index < sequences[chunk_index].size; sequence_index++) {
					m_entity_pool->FreeSequence(sequences[chunk_index][sequence_index].first);
				}
			}
		}
		// replacing the archetype
		m_static_archetypes[archetype_index].Deallocate();
		m_static_archetypes.size--;
		m_static_archetypes[archetype_index] = m_static_archetypes[m_static_archetypes.size];
		archetype_count = m_static_archetypes[archetype_index].GetArchetypeBaseCount();

		// patching up references to the archetype that replaced it
		EntityInfo info;
		info.archetype = archetype_index;
		for (size_t index = 0; index < archetype_count; index++) {
			StaticArchetypeBase* archetype_base = m_static_archetypes[archetype_index].GetArchetypeBase(index);
			archetype_base->GetEntities(sequences);

			info.archetype_subindex = index;
			// iterating over every sequence and setting the new info the entities that are still contained in it
			for (size_t chunk_index = 0; chunk_index < sequences.size; chunk_index++) {
				info.chunk = chunk_index;
				for (size_t sequence_index = 0; sequence_index < sequences[chunk_index].size; sequence_index++) {
					info.sequence = sequence_index;
					m_entity_pool->SetEntityInfoToSequence(
						sequences[chunk_index][sequence_index].first, 
						sequences[chunk_index][sequence_index].size,
						info
					);
				}
			}
		}
	}

	void EntityManager::DestroyStaticArchetype(const unsigned char* components, const unsigned char* shared_components) {
		unsigned int archetype_index = FindStaticArchetype(components, shared_components);
		if (archetype_index == 0xFFFFFFFF) {
			// error handling - "Trying to destroy a static archetype that does not exist"
			assert(false);
			return;
		}
		DestroyStaticArchetype(archetype_index);
	}

	void EntityManager::DestroyStaticArchetypeBase(unsigned int archetype_index, unsigned int archetype_subindex) {
		Stream<Stream<Sequence>> sequences;

		// getting sequences
		StaticArchetypeBase* base_archetype = m_static_archetypes[archetype_index].GetArchetypeBase(archetype_subindex);
		base_archetype->GetEntities(sequences);

		// iterating over every sequence and free it from the entity pool
		for (size_t chunk_index = 0; chunk_index < sequences.size; chunk_index++) {
			for (size_t sequence_index = 0; sequence_index < sequences[chunk_index].size; sequence_index++) {
				m_entity_pool->FreeSequence(sequences[chunk_index][sequence_index].first);
			}
		}
		m_static_archetypes[archetype_index].DestroyBase(archetype_subindex);

		// patching up references to the base archetype that replaced it
		base_archetype = m_static_archetypes[archetype_index].GetArchetypeBase(archetype_subindex);
		base_archetype->GetEntities(sequences);

		EntityInfo info;
		info.archetype = archetype_index;
		info.archetype_subindex = archetype_subindex;
		for (size_t chunk_index = 0; chunk_index < sequences.size; chunk_index++) {
			info.chunk = chunk_index;
			for (size_t sequence_index = 0; sequence_index < sequences[chunk_index].size; sequence_index++) {
				info.sequence = sequence_index;
				m_entity_pool->SetEntityInfoToSequence(
					sequences[chunk_index][sequence_index].first, 
					sequences[chunk_index][sequence_index].size,
					info
				);
			}
		}
	}

	void EntityManager::DestroyStaticArchetypeBase(
		const unsigned char* components,
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) {
		unsigned int archetype_index = FindStaticArchetype(components, shared_components);
		if (archetype_index != 0xFFFFFFFF) {
			unsigned int archetype_subindex;
			m_static_archetypes[archetype_index].FindArchetypeBase(shared_subindices, shared_components, archetype_subindex);
			if (archetype_index == 0xFFFFFFFF) {
				// error handling - "Trying to destroy a static base archetype that does not exist"
				ECS_ASSERT(false, "Trying to destroy a static base archetype that does not exist");
				return;
			}
			DestroyStaticArchetypeBase(archetype_index, archetype_subindex);
		}
		// error handling - "Trying to destroy a static base archetype that does not exist"
		ECS_ASSERT(false, "Trying to destroy a static base archetype that does not exist");
	}

	unsigned int ECS_VECTORCALL EntityManager::FindArchetype(
		VectorComponentSignature unique_signature,
		VectorComponentSignature shared_signature
	) const {
		for (size_t index = 0; index < m_archetypes.size; index++) {
			if (m_archetypes[index].GetComponentSignature().HasComponents(unique_signature)
				&& m_archetypes[index].GetSharedComponentSignature().HasComponents(shared_signature))
			{
				return index;
			}
		}
		return 0xFFFFFFFF;
	}

	unsigned int EntityManager::FindArchetype(
		const unsigned char* unique_components, 
		const unsigned char* shared_components
	) const {
		return FindArchetype(VectorComponentSignature(unique_components), VectorComponentSignature(shared_components));
	}

	ArchetypeBase* ECS_VECTORCALL EntityManager::FindArchetype(
		VectorComponentSignature unique_signature,
		VectorComponentSignature shared_signature,
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) const {
		unsigned int archetype_index = FindArchetype(unique_signature, shared_signature);
		if (archetype_index == 0xFFFFFFFF)
			return nullptr;
		return m_archetypes[archetype_index].FindArchetypeBase(shared_subindices, shared_components);
	}

	ArchetypeBase* EntityManager::FindArchetype(
		const unsigned char* unique_components,
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) const {
		return FindArchetype(
			VectorComponentSignature(unique_components), 
			VectorComponentSignature(shared_components), 
			shared_components, 
			shared_subindices
		);
	}

	unsigned int ECS_VECTORCALL EntityManager::FindStaticArchetype(
		VectorComponentSignature unique_signature,
		VectorComponentSignature shared_signature
	) const {
		for (size_t index = 0; index < m_static_archetypes.size; index++) {
			if (m_static_archetypes[index].GetComponentSignature().HasComponents(unique_signature) && 
				m_static_archetypes[index].GetSharedComponentSignature().HasComponents(shared_signature)) {
				return index;
			}
		}
		return 0xFFFFFFFF;
	}

	unsigned int EntityManager::FindStaticArchetype(
		const unsigned char* unique_components,
		const unsigned char* shared_components
	) const {
		return FindStaticArchetype(VectorComponentSignature(unique_components), VectorComponentSignature(shared_components));
	}

	StaticArchetypeBase* EntityManager::FindStaticArchetype(
		const unsigned char* unique_components,
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) const {
		unsigned int archetype_index = FindStaticArchetype(unique_components, shared_components);
		if (archetype_index == 0xFFFFFFFF)
			return nullptr;
		return m_static_archetypes[archetype_index].FindArchetypeBase(shared_subindices, shared_components);
	}

	StaticArchetypeBase* ECS_VECTORCALL EntityManager::FindStaticArchetype(
		VectorComponentSignature unique_signature,
		VectorComponentSignature shared_signature,
		const unsigned char* shared_components,
		const unsigned short* shared_subindices
	) const {
		unsigned int archetype_index = FindStaticArchetype(unique_signature, shared_signature);
		if (archetype_index == 0xFFFFFFFF)
			return nullptr;
		return m_static_archetypes[archetype_index].FindArchetypeBase(shared_subindices, shared_components);
	}

	void EntityManager::GetArchetypeStream(Stream<Archetype<ArchetypeBase>>& archetypes) const {
		archetypes = m_archetypes;
	}

	void EntityManager::GetStaticArchetypeStream(Stream<Archetype<StaticArchetypeBase>>& archetypes) const {
		archetypes = m_static_archetypes;
	}

	void EntityManager::GetArchetypes(const ArchetypeQuery* query, Stream<ArchetypeBase*>& archetypes) const {
		for (size_t index = 0; index < m_archetypes.size; index++) {
			VectorComponentSignature signature(m_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_archetype_info[index].shared_components);
			if (signature.HasComponents(query->unique) && shared_signature.HasComponents(query->shared)) {
				unsigned int archetype_count = m_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
					archetypes[archetypes.size++] = m_archetypes[index].GetArchetypeBase(archetype_index);
				}
			}
		}
	}

	void EntityManager::GetArchetypes(const ArchetypeQueryExclude* query, Stream<ArchetypeBase*>& archetypes) const {
		for (size_t index = 0; index < m_archetypes.size; index++) {
			VectorComponentSignature signature(m_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_archetype_info[index].shared_components);
			if (signature.HasComponents(query->unique) && signature.ExcludesComponents(query->unique_excluded) &&
				shared_signature.HasComponents(query->shared) && shared_signature.HasComponents(query->shared_excluded) ) {
				unsigned int archetype_count = m_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
					archetypes[archetypes.size++] = m_archetypes[index].GetArchetypeBase(archetype_index);
				}
			}
		}
	}

	void EntityManager::GetArchetypes(
		const unsigned char* unique_components,
		const unsigned char* shared_components,
		Stream<ArchetypeBase*>& archetypes
	) const {
		VectorComponentSignature search_signature(unique_components);
		VectorComponentSignature shared_search_signature(shared_components);

		for (size_t index = 0; index < m_archetypes.size; index++) {
			VectorComponentSignature signature(m_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_archetype_info[index].shared_components);
			if (signature.HasComponents(search_signature) && shared_signature.HasComponents(shared_search_signature)) {
				unsigned int archetype_count = m_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
					archetypes[archetypes.size++] = m_archetypes[index].GetArchetypeBase(archetype_index);
				}
			}
		}
	}

	void EntityManager::GetArchetypes(
		const unsigned char* unique_components,
		const unsigned char* shared_components,
		const unsigned char* exclude_components,
		const unsigned char* exclude_shared_components,
		Stream<ArchetypeBase*>& archetypes
	) const {
		VectorComponentSignature search_signature(unique_components);
		VectorComponentSignature shared_search_signature(shared_components);
		VectorComponentSignature exclude_signature(exclude_components);
		VectorComponentSignature exclude_shared_signature(exclude_shared_components);
		VectorComponentSignature zero_signature(0);

		for (size_t index = 0; index < m_archetypes.size; index++) {
			VectorComponentSignature signature(m_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_archetype_info[index].shared_components);
			if (signature.HasComponents(search_signature) && signature.ExcludesComponents(exclude_signature, zero_signature) 
				&& shared_signature.HasComponents(shared_search_signature) && 
				shared_signature.ExcludesComponents(exclude_shared_signature, zero_signature)) 
			{
				unsigned int archetype_count = m_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
					archetypes[archetypes.size++] = m_archetypes[index].GetArchetypeBase(archetype_index);
				}
			}
		}
	}

	void EntityManager::GetStaticArchetypes(const ArchetypeQuery* query, Stream<StaticArchetypeBase*>& archetypes) const {
		for (size_t index = 0; index < m_static_archetypes.size; index++) {
			VectorComponentSignature signature(m_static_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_static_archetype_info[index].shared_components);
			if (signature.HasComponents(query->unique) && shared_signature.HasComponents(query->shared)) {
				unsigned int archetype_count = m_static_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
					archetypes[archetypes.size++] = m_static_archetypes[index].GetArchetypeBase(archetype_index);
				}
			}
		}
	}

	void EntityManager::GetStaticArchetypes(const ArchetypeQueryExclude *query, Stream<StaticArchetypeBase*>& archetypes) const {
		for (size_t index = 0; index < m_static_archetypes.size; index++) {
			VectorComponentSignature signature(m_static_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_static_archetype_info[index].shared_components);
			if (signature.HasComponents(query->unique) && signature.ExcludesComponents(query->unique_excluded) && 
				shared_signature.HasComponents(query->shared) && signature.ExcludesComponents(query->shared_excluded)) {
				unsigned int archetype_count = m_static_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
					archetypes[archetypes.size++] = m_static_archetypes[index].GetArchetypeBase(archetype_index);
				}
			}
		}
	}

	void EntityManager::GetStaticArchetypes(
		const unsigned char* unique_components,
		const unsigned char* shared_components,
		Stream<StaticArchetypeBase*>& archetypes
	) const {
		VectorComponentSignature search_signature(unique_components);
		VectorComponentSignature shared_search_signature(shared_components);

		for (size_t index = 0; index < m_static_archetypes.size; index++) {
			VectorComponentSignature signature(m_static_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_static_archetype_info[index].shared_components);
			if (signature.HasComponents(search_signature) && shared_signature.HasComponents(shared_search_signature)) {
				unsigned int archetype_count = m_static_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
					archetypes[archetypes.size++] = m_static_archetypes[index].GetArchetypeBase(archetype_index);
				}
			}
		}
	}

	void EntityManager::GetStaticArchetypes(
		const unsigned char* unique_components,
		const unsigned char* shared_components,
		const unsigned char* exclude_components,
		const unsigned char* exclude_shared_components,
		Stream<StaticArchetypeBase*>& archetypes
	) const {
		VectorComponentSignature search_signature(unique_components);
		VectorComponentSignature shared_search_signature(shared_components);
		VectorComponentSignature exclude_signature(exclude_components);
		VectorComponentSignature exclude_shared_signature(exclude_shared_components);
		VectorComponentSignature zero_signature(0);

		for (size_t index = 0; index < m_static_archetypes.size; index++) {
			VectorComponentSignature signature(m_static_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_static_archetype_info[index].shared_components);
			if (signature.HasComponents(search_signature) && signature.ExcludesComponents(exclude_signature, zero_signature)
				&& shared_signature.HasComponents(shared_search_signature) &&
				shared_signature.ExcludesComponents(exclude_shared_signature, zero_signature))
			{
				unsigned int archetype_count = m_static_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_index = 0; archetype_index < archetype_count; archetype_index++) {
					archetypes[archetypes.size++] = m_static_archetypes[index].GetArchetypeBase(archetype_index);
				}
			}
		}
	}

	void* EntityManager::GetComponent(unsigned int entity, unsigned char component) const {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		return m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex)->GetComponent(entity, component, info);
	}

	void* EntityManager::GetComponentWithIndex(unsigned int entity, unsigned char component_index) const {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		ArchetypeBase* base_archetype = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);
		unsigned int entity_index = base_archetype->GetEntityIndex(entity);
		return base_archetype->GetComponent(entity_index, component_index, info, unsigned int(1));
	}

	void EntityManager::GetComponent(unsigned int entity, const unsigned char* components, Stream<void*>& data) const {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		ArchetypeBase* archetype_base = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);
		unsigned int entity_index = archetype_base->GetEntityIndex(entity, info);
		for (size_t index = 1; index <= components[0]; index++) {
			unsigned char component_index = archetype_base->FindComponentIndex(components[index]);
			data[index - 1] = archetype_base->GetComponent(entity_index, component_index, info, unsigned int(1));
		}
		data.size = components[0];
	}

	void EntityManager::GetComponentWithIndex(
		unsigned int entity, 
		const unsigned char* component_indices, 
		Stream<void*>& data
	) const {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		ArchetypeBase* archetype_base = m_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);
		unsigned int entity_index = archetype_base->GetEntityIndex(entity, info);
		for (size_t index = 1; index <= component_indices[0]; index++) {
			data[index - 1] = archetype_base->GetComponent(entity_index, component_indices[index], info, unsigned int(1));
		}
		data.size = component_indices[0];
	}

	void* EntityManager::GetComponentStatic(unsigned int entity, unsigned char component) const {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		return m_static_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex)->GetComponent(entity, component, info);
	}

	void* EntityManager::GetComponentStaticWithIndex(unsigned int entity, unsigned char component_index) const {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		StaticArchetypeBase* base_archetype = m_static_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);
		unsigned int entity_index = base_archetype->GetEntityIndex(entity);
		return base_archetype->GetComponent(entity_index, component_index, info, unsigned int(1));
	}

	void EntityManager::GetComponentStatic(unsigned int entity, const unsigned char* components, Stream<void*>& data) const {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		StaticArchetypeBase* archetype_base = m_static_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);
		unsigned int entity_index = archetype_base->GetEntityIndex(entity, info);
		for (size_t index = 1; index <= components[0]; index++) {
			unsigned char component_index = archetype_base->FindComponentIndex(components[index]);
			data[index - 1] = archetype_base->GetComponent(entity_index, component_index, info, unsigned int(1));
		}
		data.size = components[0];
	}

	void EntityManager::GetComponentStaticWithIndex(
		unsigned int entity, 
		const unsigned char* component_indices,
		Stream<void*>& data
	) const {
		EntityInfo info = m_entity_pool->GetEntityInfo(entity);
		StaticArchetypeBase* archetype_base = m_static_archetypes[info.archetype].GetArchetypeBase(info.archetype_subindex);
		unsigned int entity_index = archetype_base->GetEntityIndex(entity, info);
		for (size_t index = 1; index <= component_indices[0]; index++) {
			data[index - 1] = archetype_base->GetComponent(entity_index, component_indices[index], info, unsigned int(1));
		}
		data.size = component_indices[0];
	}

	EntityInfo EntityManager::GetEntityInfo(unsigned int entity) const {
		return m_entity_pool->GetEntityInfo(entity);
	}

	void EntityManager::GetEntities(
		const ArchetypeQuery* query, 
		Stream<Stream<Stream<Sequence>>>& sequences,
		unsigned int*** entity_buffers
	) const {
		for (size_t index = 0; index < m_archetypes.size; index++) {
			VectorComponentSignature signature(m_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_archetype_info[index].shared_components);
			if (signature.HasComponents(query->unique) && shared_signature.HasComponents(query->shared)) {
				unsigned int archetype_count = m_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_base_index = 0; archetype_base_index < archetype_count; archetype_base_index++) {
					m_archetypes[index].GetArchetypeBase(archetype_base_index)->GetSequences(
						sequences[sequences.size], 
						entity_buffers[sequences.size]
					);
					sequences.size++;
				}
			}
		}
	}

	void EntityManager::GetEntities(
		const ArchetypeQueryExclude* query,
		Stream<Stream<Stream<Sequence>>>& sequences,
		unsigned int*** entity_buffers
	) const {
		VectorComponentSignature zero_signature(0);
		for (size_t index = 0; index < m_archetypes.size; index++) {
			VectorComponentSignature signature(m_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_archetype_info[index].shared_components);
			if (signature.HasComponents(query->unique) && signature.ExcludesComponents(query->unique_excluded, zero_signature) 
				&& shared_signature.HasComponents(query->shared) 
				&& shared_signature.ExcludesComponents(query->shared_excluded, zero_signature)) 
			{
				unsigned int archetype_count = m_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_base_index = 0; archetype_base_index < archetype_count; archetype_base_index++) {
					m_archetypes[index].GetArchetypeBase(archetype_base_index)->GetSequences(
						sequences[sequences.size], 
						entity_buffers[sequences.size]
					);
					sequences.size++;
				}
			}
		}
	}

	void EntityManager::GetStaticEntities(
		const ArchetypeQuery* query,
		Stream<Stream<Stream<Sequence>>>& sequences
	) const {
		for (size_t index = 0; index < m_static_archetypes.size; index++) {
			VectorComponentSignature signature(m_static_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_static_archetype_info[index].shared_components);
			if (signature.HasComponents(query->unique) && shared_signature.HasComponents(query->shared)) {
				unsigned int archetype_count = m_static_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_base_index = 0; archetype_base_index < archetype_count; archetype_base_index++) {
					m_static_archetypes[index].GetArchetypeBase(archetype_base_index)->GetSequences(sequences[sequences.size++]);
				}
			}
		}
	}

	void EntityManager::GetStaticEntities(
		const ArchetypeQueryExclude* query,
		Stream<Stream<Stream<Sequence>>>& sequences
	) const {
		VectorComponentSignature zero_signature(0);
		for (size_t index = 0; index < m_static_archetypes.size; index++) {
			VectorComponentSignature signature(m_static_archetype_info[index].components);
			VectorComponentSignature shared_signature(m_static_archetype_info[index].shared_components);
			if (signature.HasComponents(query->unique) && signature.ExcludesComponents(query->unique_excluded) 
				&& shared_signature.HasComponents(query->shared) && shared_signature.ExcludesComponents(query->shared_excluded)) {
				unsigned int archetype_count = m_static_archetypes[index].GetArchetypeBaseCount();
				for (size_t archetype_base_index = 0; archetype_base_index < archetype_count; archetype_base_index++) {
					m_static_archetypes[index].GetArchetypeBase(archetype_base_index)->GetSequences(sequences[sequences.size++]);
				}
			}
		}
	}

	unsigned int EntityManager::GetSharedComponentSubindex(unsigned char component, void* data) const {
		ECS_ASSERT(component >= 0 && component < 256);
		size_t index = 0;
		size_t size = m_shared_components[component].size;
		while (index < m_shared_component_data[component].size) {
			if (memcmp((void*)((uintptr_t)m_shared_component_data[component].buffer + index * size), data, size)) {
				return index;
			}
		}
		return 0xFFFFFFFF;
	}

	unsigned int EntityManager::GetSharedComponentSubindex(unsigned char component, void* data, unsigned int size) const {
		ECS_ASSERT(component >= 0 && component < 256);
		size_t index = 0;
		while (index < m_shared_component_data[component].size) {
			if (memcmp((void*)((uintptr_t)m_shared_component_data[component].buffer + index * size), data, size)) {
				return index;
			}
		}
		return 0xFFFFFFFF;
	}

	void EntityManager::GetSharedComponentDataStream(Stream<void*>& data, const unsigned char* components) const
	{
		for (size_t index = 1; index <= components[0]; index++) {
			data[index - 1] = m_shared_component_data[components[index]].buffer;
		}
		data.size = components[0];
	}

	void EntityManager::RebalanceArchetype(
		unsigned int archetype_index,
		unsigned char archetype_subindex,
		RebalanceArchetypeDescriptor descriptor,
		LinearAllocator* local_allocator
	) {
		ECS_ASSERT(descriptor.chunk_index != 0xFF);
		local_allocator->SetMarker();

		unsigned int* sequence_first = (unsigned int*)local_allocator->Allocate(
			sizeof(unsigned int) * ECS_ARCHETYPE_BASE_REBALANCE_BUFFER,
			alignof(unsigned int)
		);
		Stream<unsigned int> sequence_stream(sequence_first, 0);
		ArchetypeBase* base_archetype = m_archetypes[archetype_index].GetArchetypeBase(archetype_subindex);

		EntityInfo info;
		info.archetype = archetype_index;
		info.archetype_subindex = archetype_subindex;
		info.chunk = descriptor.chunk_index;
		unsigned int new_sequence_index;

		if (descriptor.min_sequence_size == 0xFFFFFFFF || descriptor.max_sequence_size == 0xFFFFFFFF) {
			if (descriptor.count == 0xFFFF) {
				unsigned int entity_count = base_archetype->GetRebalanceEntityCount(
					descriptor.chunk_index, 
					new_sequence_index,
					sequence_stream
				);
				info.sequence = new_sequence_index;
				for (size_t index = 0; index < sequence_stream.size; index++) {
					m_entity_pool->FreeSequence(sequence_stream[index]);
				}
				unsigned int new_entity_first = m_entity_pool->GetSequence(entity_count);
				m_entity_pool->SetEntityInfoToSequence(new_entity_first, entity_count, info);

				base_archetype->Rebalance(new_entity_first, descriptor.chunk_index, entity_count);
			}
			else {
				unsigned int entity_count = base_archetype->GetRebalanceEntityCount(
					descriptor.chunk_index,
					descriptor.count,
					new_sequence_index,
					sequence_stream
				);
				info.sequence = new_sequence_index;
				for (size_t index = 0; index < sequence_stream.size; index++) {
					m_entity_pool->FreeSequence(sequence_stream[index]);
				}
				unsigned int new_entity_first = m_entity_pool->GetSequence(entity_count);
				m_entity_pool->SetEntityInfoToSequence(new_entity_first, entity_count, info);

				base_archetype->Rebalance(new_entity_first, descriptor.chunk_index, descriptor.count, entity_count);
			}
		}
		else {
			if (descriptor.count == 0xFFFF) {
				unsigned int entity_count = base_archetype->GetRebalanceEntityCount(
					descriptor.chunk_index,
					descriptor.min_sequence_size,
					descriptor.max_sequence_size,
					new_sequence_index,
					sequence_stream
				);
				info.sequence = new_sequence_index;
				for (size_t index = 0; index < sequence_stream.size; index++) {
					m_entity_pool->FreeSequence(sequence_stream[index]);
				}
				unsigned int new_entity_first = m_entity_pool->GetSequence(entity_count);
				m_entity_pool->SetEntityInfoToSequence(new_entity_first, entity_count, info);

				base_archetype->Rebalance(
					new_entity_first, 
					descriptor.chunk_index, 
					descriptor.min_sequence_size,
					descriptor.max_sequence_size, 
					entity_count
				);
			}
			else {
				unsigned int entity_count = base_archetype->GetRebalanceEntityCount(
					descriptor.chunk_index,
					descriptor.min_sequence_size,
					descriptor.max_sequence_size,
					descriptor.count,
					new_sequence_index,
					sequence_stream
				);
				info.sequence = new_sequence_index;
				for (size_t index = 0; index < sequence_stream.size; index++) {
					m_entity_pool->FreeSequence(sequence_stream[index]);
				}
				unsigned int new_entity_first = m_entity_pool->GetSequence(entity_count);
				m_entity_pool->SetEntityInfoToSequence(new_entity_first, entity_count, info);

				base_archetype->Rebalance(
					new_entity_first,
					descriptor.chunk_index,
					descriptor.min_sequence_size,
					descriptor.max_sequence_size,
					descriptor.count,
					entity_count
				);
			}
		}

		local_allocator->Deallocate(nullptr);
	}

	unsigned int EntityManager::RegisterComponent(unsigned short size, unsigned char alignment) {
		m_unique_components[m_unique_components.size].alignment = alignment;
		m_unique_components[m_unique_components.size].index = m_unique_components.size;
		m_unique_components[m_unique_components.size].size = size;
		m_unique_components.size++;
		return m_unique_components.size - 1;
	}

	unsigned int EntityManager::RegisterSharedComponent(unsigned short size, unsigned char alignment, unsigned short count) {
		m_shared_components[m_shared_components.size].alignment = alignment;
		m_shared_components[m_shared_components.size].index = m_shared_components.size;
		m_shared_components[m_shared_components.size].size = size;

		m_shared_component_capacity[m_shared_components.size] = count;
		m_shared_component_data[m_shared_components.size].buffer = m_memory_manager->Allocate(size * count, alignment);
		memset(m_shared_component_data[m_shared_components.size].buffer, 0, size * count);

		m_shared_components.size++;
		return m_shared_components.size - 1;
	}

	unsigned int EntityManager::RegisterSharedComponentData(unsigned char component, void* data) {
		ECS_ASSERT(component >= 0 && component < 256 && m_shared_component_capacity[component] > m_shared_component_data[component].size
		,"Trying to register more shared component data than it was previously allocated");

		size_t size = m_shared_components[component].size;
		memcpy(
			(void*)((uintptr_t)m_shared_component_data[component].buffer + size * m_shared_component_data[component].size), 
			data, 
			size
		);
		m_shared_component_data[component].size++;
		return m_shared_component_data[component].size - 1;
	}

	void EntityManager::SetSharedComponentData(unsigned char component, unsigned short subindex, void* data) {
		ECS_ASSERT(component >= 0 && component < 256 && subindex >= 0 && subindex < m_shared_component_data[component].size);

		size_t size = m_shared_components[component].size;
		SetSharedComponentData(component, subindex, data, size);
	}

	void EntityManager::SetSharedComponentData(unsigned char component, unsigned short subindex, void* data, unsigned int size) {
		ECS_ASSERT(component >= 0 && component < 256 && subindex >= 0 && subindex < m_shared_component_data[component].size);
		
		memcpy(
			(void*)((uintptr_t)m_shared_component_data[component].buffer + size * subindex),
			data,
			size
		);
	}

	size_t EntityManager::MemoryOf(unsigned int max_dynamic_archetype_count, unsigned int max_static_archetype_count) {
		size_t total = 0;
		total += sizeof(Archetype<ArchetypeBase>) * max_dynamic_archetype_count;
		total += sizeof(Archetype<StaticArchetypeBase>) * max_static_archetype_count;
		total += sizeof(ComponentInfo) * 256;
		total += sizeof(ComponentInfo) * 256;
		total += sizeof(ArchetypeInfo) * max_dynamic_archetype_count;
		total += sizeof(ArchetypeInfo) * max_static_archetype_count;
		total += sizeof(Stream<void>) * 256;
		total += sizeof(unsigned int) * 256;
		return total + ECS_CACHE_LINE_SIZE;
	}

}