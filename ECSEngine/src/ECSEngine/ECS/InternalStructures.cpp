#include "ecspch.h"
#include "InternalStructures.h"
#include "VectorComponentSignature.h"
#include "../Utilities/Crash.h"
#include "../Utilities/Serialization/SerializationHelpers.h"
#include "../Math/MathHelpers.h"

namespace ECSEngine {

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

	void EntityToString(Entity entity, CapacityStream<char>& string, bool extended_string)
	{
		string.AddStream("Entity ");
		ConvertIntToChars(string, entity.value);

		if (extended_string) {
			string.AddStream(" (Index - ");
			ConvertIntToChars(string, entity.index);
			string.AddStream(", generation - ");
			ConvertIntToChars(string, entity.generation_count);
			string.Add(')');
		}

		string.AssertCapacity();
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
		unsigned int pool_capacity = 1 << m_pool_power_of_two;

		size_t allocation_size = m_entity_infos.buffer->stream.MemoryOf(pool_capacity);
		void* allocation = m_memory_manager->Allocate(allocation_size);
		memset(allocation, 0, allocation_size);

		// Walk through the entity info stream and look for an empty slot and place the allocation there
		// Cannot remove swap back chunks because the entity index calculation takes into consideration the initial
		// chunk position. Using reallocation tables that would offset chunk indices does not seem like a good idea
		// since adds latency to the lookup
		auto info_stream = StableReferenceStream<EntityInfo, true>(allocation, pool_capacity);
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

	Entity EntityPool::AllocateEx(unsigned int archetype, unsigned int base_archetype, unsigned int stream_index)
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
	void EntityPoolAllocateImplementation(EntityPool* entity_pool, Stream<Entity> entities, EntityPoolAllocateAdditionalData additional_data = {}) {
		auto loop_iteration = [&entities, entity_pool, additional_data](unsigned int index) {
			if constexpr (additional_data_type == ENTITY_POOL_ALLOCATE_WITH_INFOS) {
				entity_pool->m_entity_infos[index].stream.Reserve({ entities.buffer, entities.size });
				// Increase the generation count for those entities and generate the appropriate infos
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
				// Increase the generation count for those entities and generate the appropriate infos
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

	void EntityPool::AllocateEx(Stream<Entity> entities, unsigned int archetype, unsigned int base_archetype, const unsigned int* stream_indices)
	{
		EntityPoolAllocateImplementation<ENTITY_POOL_ALLOCATE_WITH_INFOS>(this, entities, { stream_indices, { archetype, base_archetype } });
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::AllocateEx(Stream<Entity> entities, uint2 archetype_indices, unsigned int copy_position)
	{
		EntityPoolAllocateImplementation<ENTITY_POOL_ALLOCATE_WITH_POSITION>(this, entities, { nullptr, archetype_indices, copy_position });
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
			entity.index
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
				if (m_entity_infos[indices.x].stream[indices.y].generation_count != 0) {
					// It exists, return its generation count
					return m_entity_infos[indices.x].stream[indices.y].generation_count;
				}
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
		info->tags |= 1 << tag;
	}

	// ------------------------------------------------------------------------------------------------------------

	void EntityPool::SetLayer(Entity entity, unsigned int layer) {
		EntityInfo* info = GetInfoCrashCheck(this, entity, ECS_LOCATION);
		info->layer = layer;
	}

	// ------------------------------------------------------------------------------------------------------------

#define ENTITY_POOL_SERIALIZE_VERSION 0

	struct SerializeEntityInfo {
		EntityInfo info;
		Entity entity;
	};

	struct SerializeEntityPoolHeader {
		unsigned int version;
		unsigned int entity_count;
	};


	// ------------------------------------------------------------------------------------------------------------

	bool SerializeEntityPool(const EntityPool* entity_pool, ECS_FILE_HANDLE file)
	{
		size_t serialize_size = SerializeEntityPoolSize(entity_pool);
		void* buffering = Malloc(serialize_size);

		uintptr_t ptr = (uintptr_t)buffering;
		SerializeEntityPool(entity_pool, ptr);

		bool success = WriteFile(file, { buffering, serialize_size });

		Free(buffering);

		return success;
	}

	// ------------------------------------------------------------------------------------------------------------

	template<bool write_data>
	size_t SerializeEntityPoolImpl(const EntityPool* entity_pool, uintptr_t& stream) {
		size_t total_write_size = 0;
		
		// Write the header first
		SerializeEntityPoolHeader header;
		header.version = ENTITY_POOL_SERIALIZE_VERSION;
		header.entity_count = entity_pool->m_entity_infos.size;

		unsigned int entity_count = 0;
		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			entity_count += entity_pool->m_entity_infos[index].is_in_use * entity_pool->m_entity_infos[index].stream.size;
		}
		header.entity_count = entity_count;

		total_write_size += Write<write_data>(&stream, &header, sizeof(header));

		// Write the compacted entities now
		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			if (entity_pool->m_entity_infos[index].is_in_use) {
				entity_pool->m_entity_infos[index].stream.ForEachIndex([&](unsigned int entity_index) {
					SerializeEntityInfo info;
					info.entity = entity_pool->GetEntityFromPosition(index, entity_index);
					info.info = entity_pool->m_entity_infos[index].stream[entity_index];
					total_write_size += Write<write_data>(&stream, &info, sizeof(info));
				});
			}
		}

		return total_write_size;
	}

	void SerializeEntityPool(const EntityPool* entity_pool, uintptr_t& stream)
	{
		SerializeEntityPoolImpl<true>(entity_pool, stream);
	}

	// ------------------------------------------------------------------------------------------------------------

	size_t SerializeEntityPoolSize(const EntityPool* entity_pool)
	{
		uintptr_t dummy;
		return SerializeEntityPoolImpl<false>(entity_pool, dummy);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool DeserializeEntityPool(EntityPool* entity_pool, ECS_FILE_HANDLE file)
	{
		// Read the header first
		SerializeEntityPoolHeader header;
		bool success = ReadFileExact(file, { &header, sizeof(header) });

		if (!success || header.version != ENTITY_POOL_SERIALIZE_VERSION || (header.entity_count > ECS_MB * 100)) {
			return false;
		}

		// Deallocate all currently pools in use
		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			entity_pool->DeallocatePool(index);
		}
		entity_pool->m_entity_infos.FreeBuffer();

		SerializeEntityInfo* serialize_infos = (SerializeEntityInfo*)Malloc(sizeof(SerializeEntityInfo) * header.entity_count);
		success = ReadFileExact(file, { serialize_infos, sizeof(SerializeEntityInfo) * header.entity_count });

		if (success) {
			// Walk through the entities and determine the "biggest one" in order to preallocate the streams
			Entity highest_entity = { 0 };
			for (unsigned int index = 0; index < header.entity_count; index++) {
				highest_entity.index = max(highest_entity.index, serialize_infos[index].entity.index);
			}

			// Create the necessary pools - add a pool if the modulo is different from 0
			unsigned int divident = highest_entity.index >> entity_pool->m_pool_power_of_two;
			unsigned int remainder = ((highest_entity.index % (1 << entity_pool->m_pool_power_of_two)) != 0) ? 1 : 0;
			unsigned int necessary_pool_count = divident + remainder;
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
		}

		Free(serialize_infos);
		return success;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool DeserializeEntityPool(EntityPool* entity_pool, uintptr_t& stream)
	{
		// Read the header first
		SerializeEntityPoolHeader header;
		Read<true>(&stream, &header, sizeof(header));

		if (header.version != ENTITY_POOL_SERIALIZE_VERSION || (header.entity_count > ECS_MB * 100)) {
			return false;
		}

		// Deallocate all currently pools in use
		for (unsigned int index = 0; index < entity_pool->m_entity_infos.size; index++) {
			entity_pool->DeallocatePool(index);
		}
		entity_pool->m_entity_infos.FreeBuffer();

		SerializeEntityInfo* serialize_infos = (SerializeEntityInfo*)stream;
		stream += sizeof(SerializeEntityInfo) * header.entity_count;
			
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

	size_t DeserializeEntityPoolSize(uintptr_t stream) {
		SerializeEntityPoolHeader header;
		Read<true>(&stream, &header, sizeof(header));

		if (header.version != ENTITY_POOL_SERIALIZE_VERSION || header.entity_count > ECS_MB * 100) {
			return -1;
		}
		return header.entity_count;
	}

	// ------------------------------------------------------------------------------------------------------------

	unsigned short ComponentBufferPerEntryByteSize(const ComponentBuffer& component_buffer) {
		if (component_buffer.is_soa_pointer) {
			unsigned short total_size = component_buffer.element_byte_size;
			for (unsigned int index = 0; index < component_buffer.soa_pointer_count; index++) {
				total_size += component_buffer.soa_pointer_element_byte_sizes[index];
			}
			return total_size;
		}
		else {
			return component_buffer.element_byte_size;
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferSetSizeValue(const ComponentBuffer& component_buffer, void* target, size_t size)
	{
		SetIntValueUnsigned(OffsetPointer(target, component_buffer.size_offset), (ECS_INT_TYPE)component_buffer.size_int_type, size);
	}

	// ------------------------------------------------------------------------------------------------------------

	size_t ComponentBufferGetSizeValue(const ComponentBuffer& component_buffer, const void* target)
	{
		return GetIntValueUnsigned(OffsetPointer(target, component_buffer.size_offset), (ECS_INT_TYPE)component_buffer.size_int_type);
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferSetCapacityValue(const ComponentBuffer& component_buffer, void* target, size_t capacity) {
		SetIntValueUnsigned(OffsetPointer(target, component_buffer.capacity_offset), (ECS_INT_TYPE)component_buffer.capacity_int_type, capacity);
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferSetCapacityValueSafe(const ComponentBuffer& component_buffer, void* target, size_t capacity) {
		if (component_buffer.has_capacity_field) {
			ComponentBufferSetCapacityValue(component_buffer, target, capacity);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	size_t ComponentBufferGetCapacityValue(const ComponentBuffer& component_buffer, const void* target) {
		return GetIntValueUnsigned(OffsetPointer(target, component_buffer.capacity_offset), (ECS_INT_TYPE)component_buffer.capacity_int_type);
	}

	// ------------------------------------------------------------------------------------------------------------

	size_t ComponentBufferGetCapacityValueSafe(const ComponentBuffer& component_buffer, const void* target, size_t missing_value) {
		if (component_buffer.has_capacity_field) {
			return ComponentBufferGetCapacityValue(component_buffer, target);
		}
		else {
			return missing_value;
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	size_t ComponentBufferAlignment(const ComponentBuffer& component_buffer)
	{
		return min(PowerOfTwoGreater(ComponentBufferPerEntryByteSize(component_buffer)), alignof(void*));
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferSetSoAPointers(const ComponentBuffer& component_buffer, void* target, void* assign_pointer, size_t capacity) {
		void** owning_pointer = (void**)OffsetPointer(target, component_buffer.pointer_offset);
		*owning_pointer = assign_pointer;
		ComponentBufferSetSizeValue(component_buffer, target, capacity);
		
		assign_pointer = OffsetPointer(assign_pointer, (size_t)component_buffer.element_byte_size * capacity);
		for (unsigned int index = 0; index < component_buffer.soa_pointer_count; index++) {
			void** soa_pointer = (void**)OffsetPointer(target, component_buffer.soa_pointer_offsets[index]);
			*soa_pointer = assign_pointer;
			assign_pointer = OffsetPointer(assign_pointer, (size_t)component_buffer.soa_pointer_element_byte_sizes[index] * capacity);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferSetAndCopySoAPointers(
		const ComponentBuffer& component_buffer,
		void* target,
		void* assign_pointer,
		size_t assign_pointer_size,
		const void* source_data,
		size_t source_data_size,
		size_t source_data_capacity
	) {
		size_t copy_count = source_data_size;

		void** owning_pointer = (void**)OffsetPointer(target, component_buffer.pointer_offset);
		*owning_pointer = assign_pointer;
		if (assign_pointer != source_data) {
			memmove(assign_pointer, source_data, (size_t)component_buffer.element_byte_size * copy_count);
		}
		ComponentBufferSetSizeValue(component_buffer, target, source_data_size);
		ComponentBufferSetCapacityValueSafe(component_buffer, target, source_data_capacity);
		assign_pointer = OffsetPointer(assign_pointer, (size_t)component_buffer.element_byte_size * assign_pointer_size);
		source_data = OffsetPointer(source_data, (size_t)component_buffer.element_byte_size * source_data_capacity);
		
		for (unsigned int index = 0; index < component_buffer.soa_pointer_count; index++) {
			void** soa_pointer = (void**)OffsetPointer(target, component_buffer.soa_pointer_offsets[index]);
			*soa_pointer = assign_pointer;
			memmove(assign_pointer, source_data, (size_t)component_buffer.soa_pointer_element_byte_sizes[index] * copy_count);
			
			assign_pointer = OffsetPointer(assign_pointer, (size_t)component_buffer.element_byte_size * assign_pointer_size);
			source_data = OffsetPointer(source_data, (size_t)component_buffer.element_byte_size * source_data_capacity);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferCopy(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination)
	{
		if (component_buffer.is_data_pointer) {
			ComponentBufferCopyDataPointer(component_buffer, allocator, source, destination);
		}
		else {
			ComponentBufferCopyStream(component_buffer, allocator, source, destination);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferCopyDataPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination)
	{
		ComponentBufferCopyDataPointer(component_buffer, allocator, ComponentBufferGetStreamDataPointer(component_buffer, source), destination);
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferCopyStream(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination)
	{
		Stream<void> source_data = ComponentBufferGetStreamNormalPointer(component_buffer, source);
		ComponentBufferCopyStream(component_buffer, allocator, source_data, ComponentBufferGetCapacityValueSafe(component_buffer, source, source_data.size), destination);
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferCopy(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, Stream<void> data, size_t capacity, void* destination)
	{
		if (component_buffer.is_data_pointer) {
			ComponentBufferCopyDataPointer(component_buffer, allocator, data, destination);
		}
		else {
			ComponentBufferCopyStream(component_buffer, allocator, data, capacity, destination);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferCopyStream(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, Stream<void> data, size_t capacity, void* destination)
	{
		ECS_ASSERT(capacity >= data.size);

		size_t per_entry_byte_size = (size_t)ComponentBufferPerEntryByteSize(component_buffer);
		size_t copy_size = capacity * per_entry_byte_size;
		void* allocation = nullptr;
		if (copy_size > 0) {
			size_t alignment = ComponentBufferAlignment(component_buffer);
			allocation = allocator->Allocate(copy_size, alignment);
		}

		if (!component_buffer.is_soa_pointer) {
			memcpy(allocation, data.buffer, data.size * per_entry_byte_size);
			void** destination_pointer = (void**)OffsetPointer(destination, component_buffer.pointer_offset);
			*destination_pointer = allocation;
			ComponentBufferSetSizeValue(component_buffer, destination, data.size);
			ComponentBufferSetCapacityValueSafe(component_buffer, destination, capacity);
		}
		else {
			ComponentBufferSetAndCopySoAPointers(component_buffer, destination, allocation, capacity, data.buffer, data.size, capacity);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferCopyDataPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, Stream<void> data, void* destination)
	{
		size_t copy_size = data.size * component_buffer.element_byte_size;

		void* allocation = nullptr;
		if (copy_size > 0) {
			size_t alignment = ComponentBufferAlignment(component_buffer);
			allocation = allocator->Allocate(copy_size, alignment);
			memcpy(allocation, data.buffer, copy_size);
		}

		DataPointer* destination_pointer = (DataPointer*)OffsetPointer(destination, component_buffer.pointer_offset);
		destination_pointer->SetPointer(allocation);
		destination_pointer->SetData(data.size);
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferCopyStreamChecked(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, Stream<void> data, size_t capacity, void* destination)
	{
		Stream<void> destination_data = ComponentBufferGetStream(component_buffer, destination);
		if (data.size != destination_data.size) {
			ComponentBufferDeallocate(component_buffer, allocator, destination);
			ComponentBufferCopy(component_buffer, allocator, data, capacity, destination);
		}
		else {
			// Test the data
			bool are_different = false;
			if (component_buffer.is_soa_pointer) {
				are_different = memcmp(destination_data.buffer, data.buffer, (size_t)component_buffer.element_byte_size * data.size) != 0;

				unsigned int index = 0;
				size_t soa_capacity = ComponentBufferGetCapacityValueSafe(component_buffer, destination, destination_data.size);
				data.buffer = OffsetPointer(data.buffer, capacity * (size_t)component_buffer.element_byte_size);
				destination_data.buffer = OffsetPointer(destination_data.buffer, soa_capacity * (size_t)component_buffer.element_byte_size);
				while (index < component_buffer.soa_pointer_count && !are_different) {
					size_t current_element_byte_size = (size_t)component_buffer.soa_pointer_element_byte_sizes[index];
					are_different = memcmp(destination_data.buffer, data.buffer, current_element_byte_size * data.size) != 0;
					index++;
					data.buffer = OffsetPointer(data.buffer, capacity * current_element_byte_size);
					destination_data.buffer = OffsetPointer(destination_data.buffer, soa_capacity * current_element_byte_size);
				}
			}
			else {
				size_t check_size = destination_data.size * (size_t)ComponentBufferPerEntryByteSize(component_buffer);
				are_different = memcmp(destination_data.buffer, data.buffer, check_size) != 0;
			}

			if (are_different) {
				ComponentBufferDeallocate(component_buffer, allocator, destination);
				ComponentBufferCopy(component_buffer, allocator, data, capacity, destination);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferDeallocate(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, void* source)
	{
		if (component_buffer.is_data_pointer) {
			ComponentBufferDeallocateDataPointer(component_buffer, allocator, source);
		}
		else {
			ComponentBufferDeallocateNormalPointer(component_buffer, allocator, source);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferDeallocateDataPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, void* source)
	{
		DataPointer* data_pointer = (DataPointer*)OffsetPointer(source, component_buffer.pointer_offset);
		const void* buffer = data_pointer->GetPointer();
		if (buffer != nullptr) {
			allocator->Deallocate(buffer);
		}
		// Set this to nullptr for safe measure
		data_pointer->pointer = nullptr;
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferDeallocateNormalPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, void* source)
	{
		void** ptr_to_pointer = (void**)OffsetPointer(source, component_buffer.pointer_offset);
		if (*ptr_to_pointer != nullptr) {
			allocator->Deallocate(*ptr_to_pointer);
		}
		*ptr_to_pointer = nullptr;
		// Set the size to 0 as well
		ComponentBufferSetSizeValue(component_buffer, source, 0);
		if (component_buffer.is_soa_pointer) {
			// Make all pointers nullptr for safe measure
			ComponentBufferSetSoAPointers(component_buffer, source, nullptr, 0);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferReallocate(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination) {
		if (component_buffer.is_data_pointer) {
			ComponentBufferReallocateDataPointer(component_buffer, allocator, source, destination);
		}
		else {
			ComponentBufferReallocateNormalPointer(component_buffer, allocator, source, destination);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferReallocateDataPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination) {
		const DataPointer* source_pointer = (const DataPointer*)OffsetPointer(source, component_buffer.pointer_offset);
		unsigned short element_count = source_pointer->GetData();
		if (element_count > 0) {
			DataPointer* destination_pointer = (DataPointer*)OffsetPointer(destination, component_buffer.pointer_offset);
			size_t allocation_size = (size_t)element_count * (size_t)component_buffer.element_byte_size;
			size_t alignment = ComponentBufferAlignment(component_buffer);

			void* reallocation = nullptr;
			if (destination_pointer->GetData() > 0) {
				reallocation = allocator->Reallocate(destination_pointer->GetPointer(), allocation_size, alignment);
			}
			else {
				reallocation = allocator->Allocate(allocation_size, alignment);
			}
			memcpy(reallocation, source_pointer->GetPointer(), allocation_size);
			destination_pointer->SetPointer(reallocation);
			destination_pointer->SetData(element_count);
		}
		else {
			ComponentBufferDeallocateDataPointer(component_buffer, allocator, destination);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferReallocateNormalPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination) {
		Stream<void> data = ComponentBufferGetStreamNormalPointer(component_buffer, source);
		if (data.size > 0) {
			size_t element_byte_size = (size_t)ComponentBufferPerEntryByteSize(component_buffer);
			size_t capacity = ComponentBufferGetCapacityValueSafe(component_buffer, source, data.size);
			size_t allocation_size = capacity * element_byte_size;
			size_t alignment = ComponentBufferAlignment(component_buffer);

			void** pointer = (void**)OffsetPointer(destination, component_buffer.pointer_offset);
			size_t size_value = GetIntValueUnsigned(OffsetPointer(destination, component_buffer.size_offset), (ECS_INT_TYPE)component_buffer.size_int_type);
			if (size_value > 0) {
				*pointer = allocator->Reallocate(*pointer, allocation_size, alignment);
			}
			else {
				*pointer = allocator->Allocate(allocation_size, alignment);
			}

			if (!component_buffer.is_soa_pointer) {
				const void* source_data_ptr = *(const void**)OffsetPointer(source, component_buffer.pointer_offset);
				memcpy(*pointer, source_data_ptr, allocation_size);
				ComponentBufferSetSizeValue(component_buffer, destination, data.size);
				ComponentBufferSetCapacityValueSafe(component_buffer, destination, capacity);
			}
			else {
				ComponentBufferSetAndCopySoAPointers(component_buffer, destination, *pointer, capacity, data.buffer, data.size, capacity);
			}
		}
		else {
			// Just deallocate
			ComponentBufferDeallocateNormalPointer(component_buffer, allocator, destination);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	Stream<void> ComponentBufferGetStream(const ComponentBuffer& component_buffer, const void* source)
	{
		if (component_buffer.is_data_pointer) {
			return ComponentBufferGetStreamDataPointer(component_buffer, source);
		}
		else {
			return ComponentBufferGetStreamNormalPointer(component_buffer, source);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	Stream<void> ComponentBufferGetStreamNormalPointer(const ComponentBuffer& component_buffer, const void* source) {
		const void** ptr = (const void**)OffsetPointer(source, component_buffer.pointer_offset);
		return { *ptr, ComponentBufferGetSizeValue(component_buffer, source) };
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferSetStream(const ComponentBuffer& component_buffer, void* destination, Stream<void> data)
	{
		if (component_buffer.is_data_pointer) {
			ComponentBufferSetStreamDataPointer(component_buffer, destination, data);
		}
		else {
			ComponentBufferSetStreamNormalPointer(component_buffer, destination, data);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferSetStreamDataPointer(const ComponentBuffer& component_buffer, void* destination, Stream<void> data)
	{
		DataPointer* destination_pointer = (DataPointer*)OffsetPointer(destination, component_buffer.pointer_offset);
		*destination_pointer = data;
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentBufferSetStreamNormalPointer(const ComponentBuffer& component_buffer, void* destination, Stream<void> data)
	{
		void** destination_ptr = (void**)OffsetPointer(destination, component_buffer.pointer_offset);
		*destination_ptr = data.buffer;
		ComponentBufferSetSizeValue(component_buffer, destination, data.size);
	}

	// ------------------------------------------------------------------------------------------------------------

	Stream<void> ComponentBufferGetStreamDataPointer(const ComponentBuffer& component_buffer, const void* source) {
		const DataPointer* data_pointer = (const DataPointer*)OffsetPointer(source, component_buffer.pointer_offset);
		return { data_pointer->GetPointer(), data_pointer->GetData() };
	}

	// ------------------------------------------------------------------------------------------------------------

	void ComponentInfo::CallCopyFunction(void* destination, const void* source, bool deallocate_previous) const 
	{
		ComponentCopyFunctionData copy_data;
		copy_data.allocator = allocator;
		copy_data.destination = destination;
		copy_data.source = source;
		copy_data.function_data = copy_deallocate_data.buffer;
		copy_data.deallocate_previous = deallocate_previous;
		copy_function(&copy_data);
	}

	void ComponentInfo::CallDeallocateFunction(void* data) const
	{
		ComponentDeallocateFunctionData deallocate_data;
		deallocate_data.allocator = allocator;
		deallocate_data.data = data;
		deallocate_data.function_data = copy_deallocate_data.buffer;
		deallocate_function(&deallocate_data);
	}

	void ComponentInfo::TryCallCopyFunction(void* destination, const void* source, bool deallocate_previous) const
	{
		if (copy_function != nullptr) {
			CallCopyFunction(destination, source, deallocate_previous);
		}
	}

	void ComponentInfo::TryCallDeallocateFunction(void* data) const
	{
		if (deallocate_function != nullptr) {
			CallDeallocateFunction(data);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

}
