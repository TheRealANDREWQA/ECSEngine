#include "ecspch.h"
#include "../Utilities/Function.h"
#include "InternalStructures.h"

namespace ECSEngine {

	void VectorComponentSignature::ConvertComponents(const unsigned char* components) {
		uint64_t signature[4] = { 0, 0, 0, 0 };
		for (size_t i = 1; i <= components[0]; i++) {
			unsigned char division_by_64 = components[i] >> 6;
			signature[division_by_64] |= (uint64_t)1 << (components[i] - (division_by_64 << 6));
		}
		value = Vec32uc().load(signature);
	}

	void VectorComponentSignature::ConvertComponents(const unsigned char* components1, const unsigned char* components2) {
		uint64_t signature[4] = { 0, 0, 0, 0 };
		for (size_t i = 1; i <= components1[0]; i++) {
			unsigned char division_by_64 = components1[i] >> 6;
			signature[division_by_64] |= (uint64_t)1 << (components1[i] - (division_by_64 << 6));
		}
		for (size_t i = 1; i <= components2[0]; i++) {
			unsigned char division_by_64 = components2[i] >> 6;
			signature[division_by_64] |= (uint64_t)1 << (components2[i] - (division_by_64 << 6));
		}
		value = Vec32uc().load(signature);
	}

	void VectorComponentSignature::ConvertComponents(const Stream<ComponentInfo>& infos) {
		uint64_t signature[4] = { 0, 0, 0, 0 };
		for (size_t index = 0; index < infos.size; index++) {
			unsigned char division_by_64 = infos[index].index >> 6;
			signature[division_by_64] |= (uint64_t)1 << (infos[index].index - (division_by_64 << 6));
		}
		value = Vec32uc().load(signature);
	}

	bool VectorComponentSignature::HasComponents(const unsigned char* components) const {
		return HasComponents(VectorComponentSignature(components));
	}

	bool VectorComponentSignature::ExcludesComponents(const unsigned char* components) const {
		return ExcludesComponents(VectorComponentSignature(components));
	}

	bool VectorComponentSignature::ExcludesComponents(const unsigned char* components, VectorComponentSignature zero_signature) const {
		return ExcludesComponents(VectorComponentSignature(components), zero_signature);
	}

	EntityPool::EntityPool(
		unsigned int power_of_two,
		unsigned int arena_count, 
		unsigned int pool_block_count,
		MemoryManager* memory_manager
	) 
		: m_arena_count(arena_count), m_pool_size(1 << power_of_two), m_memory_manager(memory_manager), 
		m_pool_block_count(pool_block_count), m_power_of_two(power_of_two)
	{
		void* allocation = memory_manager->Allocate(MemoryOf(arena_count), alignof(BlockRange));
		m_pools.buffer = (BlockRange*)allocation;
		m_pools.size = 0;

		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(BlockRange) * arena_count;
		buffer = function::align_pointer(buffer, alignof(EntityInfo*));
		m_entity_infos = (EntityInfo**)buffer;
	}

	void EntityPool::CreatePool() {
		ECS_ASSERT(m_pools.size < m_arena_count);
		void* allocation = m_memory_manager->Allocate(BlockRange::MemoryOf(m_pool_block_count) + sizeof(EntityInfo) * m_pool_size + 8, alignof(size_t));
		m_pools[m_pools.size] = BlockRange(allocation, m_pool_block_count, m_pool_size);
		m_entity_infos[m_pools.size++] = (EntityInfo*)function::align_pointer((uintptr_t)allocation + 
		BlockRange::MemoryOf(m_pool_block_count), alignof(EntityInfo));
	}

	EntityInfo EntityPool::GetEntityInfo(unsigned int entity) const {
		size_t pool_index = entity >> m_power_of_two;
		size_t entity_pool_index = entity & (m_pool_size - 1);
		return m_entity_infos[pool_index][entity_pool_index];
	}

	EntityInfo* EntityPool::GetEntityInfoBuffer(unsigned int buffer_index) const {
		return m_entity_infos[buffer_index];
	}

	unsigned int EntityPool::GetSequence(unsigned int size) {
		for (size_t index = 0; index < m_pools.size; index++) {
			size_t offset = m_pools[index].Request(size);
			if (offset != 0xFFFFFFFFFFFFFFFF) {
				return offset + index * m_pool_size;
			}
		}
		CreatePool();
		return m_pools[m_pools.size - 1].Request(size) + (m_pools.size - 1) * m_pool_size;
	}

	unsigned int EntityPool::GetSequenceFast(unsigned int size) {
		for (int64_t index = m_pools.size - 1; index >= 0; index--) {
			size_t offset = m_pools[index].Request(size);
			if (offset != 0xFFFFFFFFFFFFFFFF) {
				return offset + index * m_pool_size;
			}
		}
		return 0xFFFFFFFF;
	}

	void EntityPool::FreeSequence(unsigned int first) {
		size_t pool_index = first >> m_power_of_two;
		size_t offset = first & (m_pool_size - 1);
		m_pools[pool_index].Free(offset);
	}

	void EntityPool::SetEntityInfo(unsigned int entity, EntityInfo new_info) {
		size_t pool_index = entity >> m_power_of_two;
		size_t entity_index = entity & (m_pool_size - 1);
		m_entity_infos[pool_index][entity_index] = new_info;
	}

	void EntityPool::SetEntityInfoToSequence(unsigned int first_entity, unsigned int size, EntityInfo new_info) {
		size_t pool_index = first_entity >> m_power_of_two;
		size_t entity_index = first_entity & (m_pool_size - 1);
		for (size_t index = 0; index < size; index++) {
			m_entity_infos[pool_index][entity_index + index] = new_info;
		}
	}

	size_t EntityPool::MemoryOf(unsigned int pool_count) {
		return (sizeof(EntityInfo*) + sizeof(BlockRange)) * pool_count;
	}

	ArchetypeQuery::ArchetypeQuery() {}

	ArchetypeQuery::ArchetypeQuery(const unsigned char* components, const unsigned char* shared_components)
	{
		unique = VectorComponentSignature(components);
		shared = VectorComponentSignature(shared_components);
	}

	ArchetypeQueryExclude::ArchetypeQueryExclude() {}

	ArchetypeQueryExclude::ArchetypeQueryExclude(
		const unsigned char* components, 
		const unsigned char* shared_components, 
		const unsigned char* exclude_components,
		const unsigned char* exclude_shared
	)
	{
		unique = VectorComponentSignature(components);
		shared = VectorComponentSignature(shared_components);
		unique_excluded = VectorComponentSignature(exclude_components);
		shared_excluded = VectorComponentSignature(exclude_shared);
	}

	unsigned int HashFunctionMultiplyString::Hash(Stream<const char> string)
	{
		// Value must be clipped to 3 bytes only - that's the precision of the hash tables
		unsigned int sum = 0;
		for (size_t index = 0; index < string.size; index++) {
			sum += string[index] * index;
		}
		return sum * (unsigned int)string.size;
	}

	unsigned int HashFunctionMultiplyString::Hash(Stream<const wchar_t> string) {
		return Hash(Stream<const char>((void*)string.buffer, string.size * sizeof(wchar_t)));
	}

	unsigned int HashFunctionMultiplyString::Hash(const char* string) {
		return Hash(Stream<const char>((void*)string, strlen(string)));
	}

	unsigned int HashFunctionMultiplyString::Hash(const wchar_t* string) {
		return Hash(Stream<const char>((void*)string, sizeof(wchar_t) * wcslen(string)));
	}

	unsigned int HashFunctionMultiplyString::Hash(const void* identifier, unsigned int identifier_size) {
		return Hash(Stream<const char>((void*)identifier, identifier_size));
	}

	unsigned int HashFunctionMultiplyString::Hash(ResourceIdentifier identifier)
	{
		return Hash(identifier.ptr, identifier.size);
	}

	ResourceIdentifier::ResourceIdentifier() : ptr(nullptr), size(0) {}

	ResourceIdentifier::ResourceIdentifier(const char* _ptr) : ptr(_ptr), size(strlen(_ptr)) {}

	ResourceIdentifier::ResourceIdentifier(const wchar_t* _ptr) : ptr(_ptr), size(wcslen(_ptr) * sizeof(wchar_t)) {}

	ResourceIdentifier::ResourceIdentifier(const void* id, unsigned int size) : ptr(id), size(size) {}

	ResourceIdentifier::ResourceIdentifier(Stream<void> identifier) : ptr(identifier.buffer), size(identifier.size) {}

	bool ResourceIdentifier::operator == (const ResourceIdentifier& other) const {
		return size == other.size && ptr == other.ptr;
	}

	bool ResourceIdentifier::Compare(const ResourceIdentifier& other) const {
		if (size != other.size)
			return false;
		else {
			size_t index = 0;
			Vec32uc char_compare, other_char_compare;
			while (size - index > char_compare.size()) {
				char_compare.load(function::OffsetPointer(ptr, index));
				other_char_compare.load(function::OffsetPointer(other.ptr, index));
				if (horizontal_and(char_compare == other_char_compare) == false) {
					return false;
				}
				index += char_compare.size();
			}
			char_compare.load_partial(size - index, function::OffsetPointer(ptr, index));
			other_char_compare.load_partial(size - index, function::OffsetPointer(other.ptr, index));
			return horizontal_and(char_compare == other_char_compare);
		}
	}

}
