#include "ecspch.h"
#include "InternalStructures.h"
#include "VectorComponentSignature.h"
#include "../Utilities/Crash.h"
#include "../Utilities/Serialization/SerializationHelpers.h"
#include "../Math/MathHelpers.h"
#include "../Utilities/ReaderWriterInterface.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	static StableReferenceStream<EntityInfo, true> EntityPoolAllocatePool(EntityPool* entity_pool) {
		unsigned int pool_capacity = 1 << entity_pool->m_pool_power_of_two;

		size_t allocation_size = entity_pool->m_entity_infos.buffer->stream.MemoryOf(pool_capacity);
		void* allocation = entity_pool->m_memory_manager->Allocate(allocation_size);
		memset(allocation, 0, allocation_size);

		return StableReferenceStream<EntityInfo, true>(allocation, pool_capacity);
	}

	// ------------------------------------------------------------------------------------------------------------

	ComponentSignature ComponentSignature::Copy(uintptr_t& ptr) const
	{
		ComponentSignature new_signature;

		new_signature.indices = (Component*)ptr;
		new_signature.count = count;
		memcpy(new_signature.indices, indices, sizeof(Component) * count);
		ptr += sizeof(Component) * count;

		return new_signature;
	}

	// ------------------------------------------------------------------------------------------------------------

	Entity StringToEntity(Stream<char> string)
	{
		Stream<char> parenthese = FindFirstCharacter(string, '(');

		Stream<char> string_to_parse = string;
		if (parenthese.buffer != nullptr) {
			string_to_parse = { string.buffer, PointerDifference(string_to_parse.buffer, string.buffer) };
		}

		return Entity((unsigned int)ConvertCharactersToInt(string_to_parse));
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityPool::EntityPool(
		MemoryManager* memory_manager,
		unsigned int pool_power_of_two
	) : m_memory_manager(memory_manager), m_pool_power_of_two(pool_power_of_two), m_entity_infos(memory_manager, 1) {}
	
	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::CreatePool() {
		// Walk through the entity info stream and look for an empty slot and place the allocation there
		// Cannot remove swap back chunks because the entity index calculation takes into consideration the initial
		// chunk position. Using reallocation tables that would offset chunk indices does not seem like a good idea
		// since adds latency to the lookup
		auto info_stream = EntityPoolAllocatePool(this);
		for (size_t index = 0; index < m_entity_infos.size; index++) {
			if (!m_entity_infos[index].is_in_use) {
				m_entity_infos[index].is_in_use = true;
				m_entity_infos[index].stream = info_stream;
				return;
			}
		}
		m_entity_infos.Add({info_stream, true});
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::CopyEntities(const EntityPool* entity_pool)
	{
		// Assume that everything is deallocated before
		// Everything can be just memcpy'ed after the correct number of pools have been allocated
		ECS_CRASH_CONDITION(m_entity_infos.size == 0, "EntityPool: Copying entity pool failed. The destination pool is not allocated.");
		ECS_CRASH_CONDITION(m_pool_power_of_two == entity_pool->m_pool_power_of_two, "EntityPool: Copying entity pool failed. The power of two of the chunks is different.");

		for (size_t index = 0; index < entity_pool->m_entity_infos.size; index++) {
			CreatePool();
			// Set the is in use flag
			m_entity_infos[index].is_in_use = entity_pool->m_entity_infos[index].is_in_use;

			if (m_entity_infos[index].is_in_use) {
				m_entity_infos[index].stream.Copy(entity_pool->m_entity_infos[index].stream);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE static unsigned int GetEntityIndexFromPoolOffset(unsigned int pool_index, unsigned int pool_power_of_two, unsigned int index) {
		return (pool_index << pool_power_of_two) + index;
	}

	ECS_INLINE static uint2 GetPoolAndEntityIndex(const EntityPool* entity_pool, Entity entity) {
		return { entity.index >> entity_pool->m_pool_power_of_two, entity.index & ((1 << entity_pool->m_pool_power_of_two) - 1) };
	}

	static Entity EntityPoolAllocateImplementation(EntityPool* entity_pool, unsigned short archetype = -1, unsigned short base_archetype = -1, unsigned int stream_index = -1) {	
		EntityInfo info;
		info.main_archetype = archetype;
		info.base_archetype = base_archetype;
		info.stream_index = stream_index;
		info.layer = 0;
		info.tags = 0;

		auto construct_info = [=](unsigned int pool_index) {
			unsigned int entity_index = entity_pool->m_entity_infos[pool_index].stream.ReserveOne();
			EntityInfo* current_info = entity_pool->m_entity_infos[pool_index].stream.ElementPointer(entity_index);
			unsigned char new_generation_count = current_info->generation_count + 1;
			memcpy(current_info, &info, sizeof(info));
			current_info->generation_count = new_generation_count == 0 ? 1 : new_generation_count;

			entity_index = GetEntityIndexFromPoolOffset(pool_index, entity_pool->m_pool_power_of_two, entity_index);
			return Entity(entity_index, current_info->generation_count);
		};
		
		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			if (entity_pool->m_entity_infos[index].stream.size < entity_pool->m_entity_infos[index].stream.capacity) {
				return construct_info(index);
			}
		}

		// All pools are full - create a new one and allocate from it
		entity_pool->CreatePool();
		return construct_info(entity_pool->m_entity_infos.size - 1);
	}

	Entity EntityPool::Allocate()
	{
		return EntityPoolAllocateImplementation(this);
	}

	// ------------------------------------------------------------------------------------------------------------

	Entity EntityPool::Allocate(unsigned int archetype, unsigned int base_archetype, unsigned int stream_index)
	{
		return EntityPoolAllocateImplementation(this, archetype, base_archetype, stream_index);
	}

	// ------------------------------------------------------------------------------------------------------------

	struct EntityPoolAllocateAdditionalData {
		const unsigned int* stream_indices;
		uint2 archetype_indices;
		unsigned int copy_position;
	};

	enum EntityPoolAllocateType {
		ENTITY_POOL_ALLOCATE_NO_DATA,
		ENTITY_POOL_ALLOCATE_WITH_INFOS,
		ENTITY_POOL_ALLOCATE_WITH_POSITION
	};
	
	template<EntityPoolAllocateType additional_data_type>
	static void EntityPoolAllocateImplementation(EntityPool* entity_pool, Stream<Entity> entities, EntityPoolAllocateAdditionalData additional_data = {}) {
		auto loop_iteration = [&entities, entity_pool, additional_data](unsigned int index) {
			if constexpr (additional_data_type == ENTITY_POOL_ALLOCATE_WITH_INFOS) {
				entity_pool->m_entity_infos[index].stream.Reserve({ entities.buffer, entities.size });
				for (size_t entity_index = 0; entity_index < entities.size; entity_index++) {
					EntityInfo* info = entity_pool->m_entity_infos[index].stream.ElementPointer(entities[entity_index].value);
					info->tags = 0;
					info->layer = 0;
					info->base_archetype = additional_data.archetype_indices.y;
					info->main_archetype = additional_data.archetype_indices.x;
					info->stream_index = additional_data.stream_indices[entity_index];

					// Set the appropriate entity now
					entities[entity_index].index = GetEntityIndexFromPoolOffset(index, entity_pool->m_pool_power_of_two, entities[entity_index].value);
					entities[entity_index].generation_count = info->generation_count;
				}
			}
			else if constexpr (additional_data_type == ENTITY_POOL_ALLOCATE_WITH_POSITION) {
				for (size_t entity_index = 0; entity_index < entities.size; entity_index++) {
					unsigned int info_index = entity_pool->m_entity_infos[index].stream.ReserveOne();

					EntityInfo* current_info = entity_pool->m_entity_infos[index].stream.ElementPointer(info_index);
					current_info->base_archetype = additional_data.archetype_indices.y;
					current_info->main_archetype = additional_data.archetype_indices.x;
					current_info->stream_index = additional_data.copy_position + entity_index;

					current_info->tags = 0;
					current_info->layer = 0;

					// This will be a handle that can be used to index into this specific chunk of the entity pool
					entities[entity_index].index = GetEntityIndexFromPoolOffset(index, entity_pool->m_pool_power_of_two, info_index);
					entities[entity_index].generation_count = current_info->generation_count;
				}
			}
			else {
				entity_pool->m_entity_infos[index].stream.Reserve({ entities.buffer, entities.size });
				for (size_t entity_index = 0; entity_index < entities.size; entity_index++) {
					EntityInfo* info = entity_pool->m_entity_infos[index].stream.ElementPointer(entities[entity_index].value);

					info->tags = 0;
					info->layer = 0;

					info->base_archetype = -1;
					info->main_archetype = -1;
					info->stream_index = -1;

					// Set the appropriate entity now
					entities[entity_index].index = GetEntityIndexFromPoolOffset(index, entity_pool->m_pool_power_of_two, entities[entity_index].value);
					entities[entity_index].generation_count = info->generation_count;
				}
			}
		};

		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			if (entity_pool->m_entity_infos[index].stream.size + entities.size < entity_pool->m_entity_infos[index].stream.capacity) {
				loop_iteration(index);
				return;
			}
		}

		// All pools are full - create a new one and allocate from it
		entity_pool->CreatePool();
		unsigned int pool_index = entity_pool->m_entity_infos.size - 1;
		loop_iteration(pool_index);
	}

	void EntityPool::Allocate(Stream<Entity> entities)
	{
		EntityPoolAllocateImplementation<ENTITY_POOL_ALLOCATE_NO_DATA>(this, entities);
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::Allocate(Stream<Entity> entities, unsigned int archetype, unsigned int base_archetype, const unsigned int* stream_indices)
	{
		EntityPoolAllocateImplementation<ENTITY_POOL_ALLOCATE_WITH_INFOS>(this, entities, { stream_indices, { archetype, base_archetype } });
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::Allocate(Stream<Entity> entities, uint2 archetype_indices, unsigned int copy_position)
	{
		EntityPoolAllocateImplementation<ENTITY_POOL_ALLOCATE_WITH_POSITION>(this, entities, { nullptr, archetype_indices, copy_position });
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::AllocateSpecific(Stream<Entity> entities, uint2 archetype_indices, unsigned int copy_position) {
		for (size_t index = 0; index < entities.size; index++) {
			uint2 entity_indices = GetPoolAndEntityIndex(this, entities[index]);
			// If it is out of bounds, resize the entity infos
			if (entity_indices.x >= m_entity_infos.size) {
				m_entity_infos.Reserve(entity_indices.x - m_entity_infos.size + 1);
				for (size_t subindex = m_entity_infos.size; subindex <= entity_indices.x; subindex++) {
					m_entity_infos[subindex].is_in_use = false;
				}
				m_entity_infos.size = entity_indices.x + 1;
			}

			// If the stable reference stream is not in use, allocate it
			if (!m_entity_infos[entity_indices.x].is_in_use) {
				m_entity_infos[entity_indices.x].stream = EntityPoolAllocatePool(this);
				m_entity_infos[entity_indices.x].is_in_use = true;
			}
			else {
				ECS_CRASH_CONDITION_RETURN_VOID(
					m_entity_infos[entity_indices.x].stream[entity_indices.y].generation_count == entities[index].generation_count,
					"EntityPool: Entity {#} already exists when trying to create a specific entity",
					entities[index].value
				);
			}

			m_entity_infos[entity_indices.x].stream.AllocateIndex(entity_indices.y);

			EntityInfo* current_info = m_entity_infos[index].stream.ElementPointer(entity_indices.y);
			current_info->base_archetype = archetype_indices.y;
			current_info->main_archetype = archetype_indices.x;
			current_info->stream_index = copy_position + index;

			current_info->tags = 0;
			current_info->layer = 0;
			// Set the generation count to the one from the entity
			current_info->generation_count = entities[index].generation_count;
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	static EntityInfo GetInfoCrashCheck(
		const EntityPool* entity_pool,
		Entity entity,
		const char* file,
		const char* function,
		unsigned int line
	) {
		uint2 entity_indices = GetPoolAndEntityIndex(entity_pool, entity);
		ECS_CRASH_CONDITION_RETURN_EX(
			entity_indices.x < entity_pool->m_entity_infos.size && entity_pool->m_entity_infos[entity_indices.x].is_in_use,
			{},
			"EntityPool: Invalid entity {#} when trying to retrieve EntityInfo.",
			file,
			function,
			line,
			entity.index
		);

		EntityInfo info = entity_pool->m_entity_infos[entity_indices.x].stream[entity_indices.y];
		bool is_valid = info.generation_count == entity.generation_count;
		if (!is_valid) {
			if (info.generation_count == 0) {
				ECS_CRASH_EX(
					"EntityPool: Generation counter mismatch for entity {#}. The entity is deleted.",
					file,
					function,
					line,
					entity.index
				);
				return {};
			}
			else {
				ECS_CRASH_EX(
					"EntityPool: Generation counter mismatch for entity {#}. Entity counter {#}, info counter {#}.",
					file,
					function,
					line,
					entity.index,
					entity.generation_count,
					info.generation_count
				);
				return {};
			}
		}

		return info;
	}

	static EntityInfo* GetInfoCrashCheck(
		EntityPool* entity_pool,
		Entity entity, 
		const char* file,
		const char* function,
		unsigned int line
	) {
		uint2 entity_indices = GetPoolAndEntityIndex(entity_pool, entity);
		ECS_CRASH_CONDITION_RETURN_EX(
			entity_indices.x < entity_pool->m_entity_infos.size && entity_pool->m_entity_infos[entity_indices.x].is_in_use,
			nullptr,
			"EntityPool: Invalid entity {#} when trying to retrieve EntityInfo.",
			file,
			function,
			line,
			entity.value
		);

		EntityInfo* info = entity_pool->m_entity_infos[entity_indices.x].stream.ElementPointer(entity_indices.y);
		bool is_valid = info->generation_count == entity.generation_count;
		if (!is_valid) {
			if (info->generation_count == 0) {
				ECS_CRASH_EX(
					"EntityPool: Generation counter mismatch for entity {#}. The entity is deleted.",
					file,
					function,
					line,
					entity.index
				);
				return nullptr;
			}
			else {
				ECS_CRASH_EX(
					"EntityPool: Generation counter mismatch for entity {#}. Entity counter {#}, info counter {#}.",
					file,
					function,
					line,
					entity.index,
					entity.generation_count,
					info->generation_count
				);
				return nullptr;
			}
		}

		return info;
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::ClearTag(Entity entity, unsigned char tag) {
		EntityInfo* info = GetInfoCrashCheck(
			this, 
			entity, 
			ECS_LOCATION
		);
		info->tags &= ~(1 << tag);
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::Deallocate(Entity entity)
	{
		uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
		ECS_CRASH_CONDITION(
			entity_indices.x < m_entity_infos.size && m_entity_infos[entity_indices.x].is_in_use,
			"EntityPool: Incorrect entity {#} when trying to delete it.",
			entity.index
		);

		EntityInfo* info = m_entity_infos[entity_indices.x].stream.ElementPointer(entity_indices.y);
		// Check that they have the same generation counter
		ECS_CRASH_CONDITION(info->generation_count == entity.generation_count, "EntityPool: Trying to delete an entity {#} which has already been deleted.", entity.index);
		m_entity_infos[entity_indices.x].stream.Remove(entity_indices.y);
		// Signal that the entity has been removed by increasing the generation counter
		info->generation_count++;

		if (m_entity_infos[entity_indices.x].stream.size == 0) {
			DeallocatePool(entity_indices.x);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::Deallocate(Stream<Entity> entities)
	{
		for (size_t index = 0; index < entities.size; index++) {
			Deallocate(entities[index]);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::DeallocatePool(unsigned int pool_index)
	{
		// Deallocate the buffer and mark as unused
		m_memory_manager->Deallocate(m_entity_infos[pool_index].stream.buffer);
		// When iterating for an allocation, since the size and the capacity are both set to 0
		// The iteration will just skip this block - as it should
		memset(m_entity_infos.buffer + pool_index, 0, sizeof(TaggedStableReferenceStream));
	}

	// ------------------------------------------------------------------------------------------------------------

	bool EntityPool::IsValid(Entity entity) const {
		uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
		if (entity_indices.x >= m_entity_infos.size || !m_entity_infos[entity_indices.x].is_in_use) {
			return false;
		}
		if (m_entity_infos[entity_indices.x].stream.ExistsItem(entity_indices.y)) {
			EntityInfo info = m_entity_infos[entity_indices.x].stream[entity_indices.y];
			return info.generation_count == entity.generation_count;
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool EntityPool::HasTag(Entity entity, unsigned char tag) const
	{
		EntityInfo info = GetInfoCrashCheck(
			this, 
			entity, 
			ECS_LOCATION
		);

		return (info.tags & tag) != 0;
	}

	// ------------------------------------------------------------------------------------------------------------

	Entity EntityPool::GetEntityFromPosition(unsigned int chunk_index, unsigned int stream_index) const
	{
		return Entity(chunk_index * (1 << m_pool_power_of_two) + stream_index);
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityInfo EntityPool::GetInfo(Entity entity) const {
		return GetInfoCrashCheck(
			this,
			entity,
			ECS_LOCATION
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityInfo EntityPool::GetInfoNoChecks(Entity entity) const
	{
		uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
		return m_entity_infos[entity_indices.x].stream[entity_indices.y];
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityInfo* EntityPool::GetInfoPtr(Entity entity)
	{
		return GetInfoCrashCheck(
			this,
			entity,
			ECS_LOCATION
		);
	}

	// ------------------------------------------------------------------------------------------------------------

	EntityInfo* EntityPool::GetInfoPtrNoChecks(Entity entity)
	{
		uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
		return m_entity_infos[entity_indices.x].stream.ElementPointer(entity_indices.y);
	}
	
	// ------------------------------------------------------------------------------------------------------------

	unsigned int EntityPool::GetCount() const
	{
		unsigned int total = 0;
		for (unsigned int index = 0; index < m_entity_infos.size; index++) {
			if (m_entity_infos[index].is_in_use) {
				total += m_entity_infos[index].stream.size;
			}
		}

		return total;
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE unsigned int GetMaxUnusedEntityValue(unsigned int bit_count) {
		ECS_ASSERT(bit_count >= 16);
		return (1 << bit_count) - 2;
	}

	Entity EntityPool::GetVirtualEntity(unsigned int bit_count) const
	{
		const size_t ITERATION_STOP_COUNT = 1'000;

		const unsigned int max_value = GetMaxUnusedEntityValue(bit_count);
		// Iterate from the high values until a value is found to be empty. Stop after an iteration count
		for (size_t index = 0; index < ITERATION_STOP_COUNT; index++) {
			unsigned int entity_index = max_value - (unsigned int)index;
			if (!IsValid(entity_index)) {
				return entity_index;
			}
		}

		return Entity::Invalid();
	}

	// ------------------------------------------------------------------------------------------------------------

	bool EntityPool::GetVirtualEntities(Stream<Entity> entities, unsigned int bit_count) const
	{
		const size_t total_iterations = ClampMin<size_t>(entities.size * 2, 1000);
		size_t current_count = 0;
		const unsigned int max_value = GetMaxUnusedEntityValue(bit_count);
		// Iterate from the high values until a value is found to be empty. Stop after an iteration count
		for (size_t index = 0; index < total_iterations; index++) {
			unsigned int entity_index = max_value - (unsigned int)index;
			if (!IsValid(entity_index)) {
				entities[current_count++] = entity_index;
				if (current_count == entities.size) {
					return true;
				}
			}
		}

		return false;
	}

	// ------------------------------------------------------------------------------------------------------------

	Entity EntityPool::GetVirtualEntity(Stream<Entity> excluded_entities, unsigned int bit_count) const
	{
		const size_t ITERATION_STOP_COUNT = 1'000 + excluded_entities.size;
		unsigned int uint_exclude_size = (unsigned int)excluded_entities.size;
		const unsigned int max_value = GetMaxUnusedEntityValue(bit_count);
		for (size_t index = 0; index < ITERATION_STOP_COUNT; index++) {
			Entity entity = max_value - uint_exclude_size - index;
			// Check to see if this entity exists in the excluded_entities
			if (SearchBytes(excluded_entities, entity) == -1) {
				if (!IsValid(entity)) {
					return entity;
				}
			}
		}

		return Entity::Invalid();
	}

	// ------------------------------------------------------------------------------------------------------------

	bool EntityPool::GetVirtualEntities(Stream<Entity> entities, Stream<Entity> excluded_entities, unsigned int bit_count) const
	{
		const size_t ITERATION_STOP_COUNT = ClampMin<size_t>(entities.size * 2 + excluded_entities.size, 1'000);
		unsigned int uint_exclude_size = (unsigned int)excluded_entities.size;
		size_t current_count = 0;
		const unsigned int max_value = GetMaxUnusedEntityValue(bit_count);
		for (size_t index = 0; index < ITERATION_STOP_COUNT; index++) {
			Entity entity = max_value - uint_exclude_size - index;
			// Check to see if this entity exists in the excluded entities
			if (SearchBytes(excluded_entities, entity) == -1) {
				if (!IsValid(entity)) {
					entities[current_count++] = entity;
					if (entities.size == current_count) {
						return true;
					}
				}
			}
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------

	unsigned int EntityPool::IsEntityAt(unsigned int stream_index) const
	{
		Entity entity;
		entity.index = stream_index;
		uint2 indices = GetPoolAndEntityIndex(this, entity);

		if (indices.x < m_entity_infos.size) {
			if (m_entity_infos[indices.x].is_in_use) {
				// It exists, return its generation count
				return m_entity_infos[indices.x].stream[indices.y].generation_count;
			}
		}
		return -1;
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::Reset()
	{
		m_memory_manager->Clear();	
		*this = EntityPool(m_memory_manager, m_pool_power_of_two);
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::SetEntityInfo(Entity entity, unsigned int archetype, unsigned int base_archetype, unsigned int stream_index) {
		EntityInfo* info = GetInfoCrashCheck(this, entity, ECS_LOCATION);

		info->base_archetype = base_archetype;
		info->main_archetype = archetype;
		info->stream_index = stream_index;
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::SetTag(Entity entity, unsigned char tag) {
		EntityInfo* info = GetInfoCrashCheck(this, entity, ECS_LOCATION);
		info->tags |= (size_t)1 << (size_t)tag;
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::SetLayer(Entity entity, unsigned int layer) {
		EntityInfo* info = GetInfoCrashCheck(this, entity, ECS_LOCATION);
		info->layer = layer;
	}

	// ------------------------------------------------------------------------------------------------------------

	const EntityInfo* EntityPool::TryGetEntityInfo(Entity entity) const {
		if (IsValid(entity)) {
			uint2 entity_indices = GetPoolAndEntityIndex(this, entity);
			return m_entity_infos[entity_indices.x].stream.ElementPointer(entity_indices.y);
		}
		return nullptr;
	}

	// ------------------------------------------------------------------------------------------------------------

#define ENTITY_POOL_SERIALIZE_VERSION 0

	// When EntityInfo was using unsigned int instead of size_t as base type, it results in this type using
	// 12 bytes, because there is no padding, but when upgrading to size_t, there are additional bytes added,
	// Which ruin the serialization byte size. For this reason, use this special pack for this type only.
#pragma pack(1)
	struct SerializeEntityInfo {
		EntityInfo info;
		Entity entity;
	};
#pragma pack()

	struct SerializeEntityPoolHeader {
		unsigned int version;
		unsigned int entity_count;
	};

	bool SerializeEntityPool(const EntityPool* entity_pool, WriteInstrument* write_instrument)
	{
		// Write the header first
		SerializeEntityPoolHeader header;
		header.version = ENTITY_POOL_SERIALIZE_VERSION;
		header.entity_count = entity_pool->m_entity_infos.size;

		unsigned int entity_count = 0;
		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			entity_count += entity_pool->m_entity_infos[index].is_in_use * entity_pool->m_entity_infos[index].stream.size;
		}
		header.entity_count = entity_count;

		if (!write_instrument->Write(&header)) {
			return false;
		}

		// Write the compacted entities now
		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			if (entity_pool->m_entity_infos[index].is_in_use) {
				entity_pool->m_entity_infos[index].stream.ForEachIndex([&](unsigned int entity_index) {
					SerializeEntityInfo info;
					info.entity = entity_pool->GetEntityFromPosition(index, entity_index);
					info.info = entity_pool->m_entity_infos[index].stream[entity_index];
					if (!write_instrument->Write(&info)) {
						return false;
					}
				});
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool DeserializeEntityPool(EntityPool* entity_pool, ReadInstrument* read_instrument)
	{
		// Read the header first
		SerializeEntityPoolHeader header;
		if (!read_instrument->ReadAlways(&header)) {
			return false;
		}

		if (header.version != ENTITY_POOL_SERIALIZE_VERSION || (header.entity_count > ECS_ENTITY_MAX_COUNT)) {
			return false;
		}

		// Deallocate all pools in use
		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			entity_pool->DeallocatePool(index);
		}
		entity_pool->m_entity_infos.FreeBuffer();

		size_t serialize_infos_size = sizeof(SerializeEntityInfo) * header.entity_count;
		ECS_MALLOCA_ALLOCATOR_SCOPED(scoped_allocation, serialize_infos_size, ECS_KB * 64, ECS_MALLOC_ALLOCATOR);
		SerializeEntityInfo* serialize_infos = (SerializeEntityInfo*)scoped_allocation.buffer;
		if (!read_instrument->Read(serialize_infos, serialize_infos_size)) {
			return false;
		}

		// Walk through the entities and determine the "biggest one" in order to preallocate the streams
		Entity highest_entity = { 0 };
		for (unsigned int index = 0; index < header.entity_count; index++) {
			highest_entity.index = max(highest_entity.index, serialize_infos[index].entity.index);
		}

		// Create the necessary pools - add a pool if the modulo is different from 0
		unsigned int necessary_pool_count = (highest_entity.index >> entity_pool->m_pool_power_of_two) +
			((highest_entity.index % (1 << entity_pool->m_pool_power_of_two)) != 0);
		necessary_pool_count = necessary_pool_count == 0 ? 1 : necessary_pool_count;
		for (unsigned int index = 0; index < necessary_pool_count; index++) {
			entity_pool->CreatePool();
		}

		// Update the entity infos
		for (unsigned int index = 0; index < header.entity_count; index++) {
			uint2 stream_index = GetPoolAndEntityIndex(entity_pool, serialize_infos[index].entity);
			// Need to allocate the index
			entity_pool->m_entity_infos[stream_index.x].stream.AllocateIndex(stream_index.y);

			EntityInfo* info = entity_pool->GetInfoPtrNoChecks(serialize_infos[index].entity);
			*info = serialize_infos[index].info;
		}

		// Set the is_in_use status for the chunks
		for (unsigned int index = 0; index < necessary_pool_count; index++) {
			entity_pool->m_entity_infos[index].is_in_use = entity_pool->m_entity_infos[index].stream.size > 0;
			if (!entity_pool->m_entity_infos[index].is_in_use) {
				entity_pool->DeallocatePool(index);
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentInfo::CallCopyFunction(void* destination, const void* source, bool deallocate_previous) const {
		CallCopyFunction(destination, source, deallocate_previous, allocator);
	}

	void ComponentInfo::CallCopyFunction(void* destination, const void* source, bool deallocate_previous, AllocatorPolymorphic override_allocator) const {
		ComponentCopyFunctionData copy_data;
		copy_data.allocator = override_allocator;
		copy_data.destination = destination;
		copy_data.source = source;
		copy_data.function_data = data;
		copy_data.deallocate_previous = deallocate_previous;
		copy_function(&copy_data);
	}

	void ComponentInfo::CallDeallocateFunction(void* _data) const {
		CallDeallocateFunction(_data, allocator);
	}

	void ComponentInfo::CallDeallocateFunction(void* _data, AllocatorPolymorphic override_allocator) const
	{
		ComponentDeallocateFunctionData deallocate_data;
		deallocate_data.allocator = override_allocator;
		deallocate_data.data = _data;
		deallocate_data.function_data = data;
		deallocate_function(&deallocate_data);
	}

	// ------------------------------------------------------------------------------------------------------------

	void Entity::ToString(CapacityStream<char>& string, bool extended_string) const
	{
		string.AddStreamAssert("Entity ");
		ConvertIntToChars(string, value);

		if (extended_string) {
			string.AddStreamAssert(" (Index - ");
			ConvertIntToChars(string, index);
			string.AddStreamAssert(", generation - ");
			ConvertIntToChars(string, generation_count);
			string.AddAssert(')');
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPair::ToString(CapacityStream<char>& string, bool extended_string) const
	{
		string.AddStreamAssert("Pair {");
		first.ToString(string, extended_string);
		string.AddStreamAssert(", ");
		second.ToString(string, extended_string);
		string.AddAssert('}');
	}

	// ------------------------------------------------------------------------------------------------------------

}
