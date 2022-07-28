#pragma once
#include "ecspch.h"
#include "../Core.h"
#include "../Math/VCLExtensions.h"
#include "Hashing.h"

#ifndef ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR
#define ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR 90
#endif

namespace ECSEngine {

	/* Robin Hood hashing identifier hash table that stores in separate buffers keys and values. Keys are
	* unsigned chars that have stored in the highest 5 bits the robin hood distance in order to not create an
	* additional metadata buffer, not get an extra cache miss and use SIMD probing and in the other 3 bits the last bits of the hash key. 
	* Distance 0 means empty slot, distance 1 means that the key is where it hashed. The maximum distance an element can have is 31. 
	* The capacity should be set to a power of two when using power of two hash function. 
	* There are 32 padding elements at the end in order to eliminate wrap around and to effectively use only SIMD operations.
	* Use MemoryOf to get the number of bytes needed for the buffers. It does a identifier check when trying to retrieve the value.
	* Identifier type must implement Compare function because it supports checking for equality with a user defined operation other
	* than assignment operator e. g. for pointers, they might be equal if the data they point to is the same
	* Identifier must be stable, this table only keeps the pointer, not the data pointed to.
	*/
	template<typename T, typename Identifier, typename TableHashFunction, typename ObjectHashFunction = ObjectHashFallthrough>
	struct HashTable {
	public:
#define ECS_HASH_TABLE_DISTANCE_MASK 0xF8
#define ECS_HASH_TABLE_HASH_BITS_MASK 0x07

		HashTable() : m_metadata(nullptr), m_values(nullptr), m_identifiers(nullptr), m_capacity(0), m_count(0), m_function(TableHashFunction(0)) {}
		HashTable(void* buffer, unsigned int capacity, size_t additional_info = 0) {
			InitializeFromBuffer(buffer, capacity, additional_info);
		}

		HashTable(const HashTable& other) = default;
		HashTable<T, Identifier, TableHashFunction, ObjectHashFunction>& operator = (const HashTable<T, Identifier, TableHashFunction, ObjectHashFunction>& other) = default;

		void Clear() {
			memset(m_metadata, 0, sizeof(unsigned char) * (m_capacity + 31));
			m_count = 0;
		}

		template<bool use_compare_function = true>
		unsigned int Find(Identifier identifier) const {
			unsigned int key = ObjectHashFunction::Hash(identifier);

			// calculating the index for the array with the hash function
			unsigned int index = m_function(key, m_capacity);

			unsigned char key_hash_bits = key;
			key_hash_bits &= ECS_HASH_TABLE_HASH_BITS_MASK;

			Vec32uc key_bits(key_hash_bits), elements, ignore_distance(ECS_HASH_TABLE_HASH_BITS_MASK);
			// Only elements that hashed to this slot should be checked
			Vec32uc corresponding_distance(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32);

			// Exclude elements that have distance different from the distance to the current slot
			elements.load(m_metadata + index);				
			Vec32uc element_hash_bits = elements & ignore_distance;
			auto are_hashed_to_same_slot = (elements >> 3) == corresponding_distance;
			auto match = element_hash_bits == key_bits;
			match &= are_hashed_to_same_slot;

			unsigned int return_value = -1;
			ForEachBit(match, [&](unsigned int bit_index) {
				if constexpr (!use_compare_function) {
					if (m_identifiers[index + bit_index] == identifier) {
						return_value = index + bit_index;
						return true;
					}
				}
				else {
					if (m_identifiers[index + bit_index].Compare(identifier)) {
						return_value = index + bit_index;
						return true;
					}
				}
				return false;
			});

			return return_value;
		}

		// It will be called with a single argument - the index of the element
		// It must return true if the current element is being deleted. Else false
		template<typename Functor>
		void ForEachIndex(Functor&& functor) {
			unsigned int extended_capacity = GetExtendedCapacity();
			for (int index = 0; index < (int)extended_capacity; index++) {
				if (IsItemAt(index)) {
					index -= functor(index);
				}
			}
		}

		template<typename Functor>
		void ForEachIndexConst(Functor&& functor) const {
			unsigned int extended_capacity = GetExtendedCapacity();
			for (unsigned int index = 0; index < extended_capacity; index++) {
				if (IsItemAt(index)) {
					functor(index);
				}
			}
		}

		// First parameter - the value, the second one - the identifier
		template<typename Functor>
		void ForEach(Functor&& functor) {
			unsigned int extended_capacity = GetExtendedCapacity();
			for (unsigned int index = 0; index < extended_capacity; index++) {
				if (IsItemAt(index)) {
					functor(m_values[index], m_identifiers[index]);
				}
			}
		}

		// Const variant
		template<typename Functor>
		void ForEachConst(Functor&& functor) const {
			unsigned int extended_capacity = GetExtendedCapacity();
			for (unsigned int index = 0; index < extended_capacity; index++) {
				if (IsItemAt(index)) {
					functor(m_values[index], m_identifiers[index]);
				}
			}
		}

		// the return value tells the caller if the hash table needs to grow or allocate another hash table
		bool Insert(T value, Identifier identifier) {
			unsigned int dummy;
			return Insert(value, identifier, dummy);
		}

		// the return value tells the caller if the hash table needs to grow or allocate another hash table
		bool Insert(T value, Identifier identifier, unsigned int& position) {
			unsigned int key = ObjectHashFunction::Hash(identifier);
			// Signal that the position has not yet been determined
			position = -1;

			// calculating the index at which the key wants to be
			unsigned int index = m_function(key, m_capacity);
			unsigned int hash = index;
			unsigned int distance = 1;

			unsigned char hash_key_bits = key;
			hash_key_bits &= ECS_HASH_TABLE_HASH_BITS_MASK;

			// probing the next slots
			while (true) {
				// getting the element's distance
				unsigned int element_distance = GetElementDistance(index);

				// if the element is "poorer" than us, we keep probing 
				while (element_distance > distance) {
					distance++;
					index++;
					element_distance = GetElementDistance(index);
				}

				// if the slot is empty, the key can be placed here
				if (element_distance == 0) {
					hash_key_bits |= distance << 3;
					m_metadata[index] = hash_key_bits;
					m_values[index] = value;
					m_identifiers[index] = identifier;
					m_count++;
					position = position == -1 ? index : position;
					break;
				}

				// if it finds a richer slot than us, we swap the keys and keep searching to find 
				// an empty slot for the richer slot
				else {
					unsigned char metadata_temp = m_metadata[index];
					T value_temp = m_values[index];
					Identifier identifier_temp = m_identifiers[index];
					m_metadata[index] = hash_key_bits | (distance << 3);
					m_values[index] = value;
					m_identifiers[index] = identifier;
					distance = (metadata_temp >> 3) + 1;
					hash_key_bits = metadata_temp & ECS_HASH_TABLE_HASH_BITS_MASK;
					value = value_temp;
					identifier = identifier_temp;
					position = position == -1 ? index : position;
					index++;
				}
			}
			if ((m_count * 100 / m_capacity > ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR) || distance >= 31)
				return true;
			return false;
		}

		template<bool use_compare_function = true>
		void Erase(Identifier identifier) {
			// determining the next index and clearing the current slot
			int index = Find<use_compare_function>(identifier);
			ECS_ASSERT(index != -1);
			EraseFromIndex(index);
		}

		void EraseFromIndex(unsigned int index) {
			// getting the distance of the next slot;
			index++;
			unsigned char distance = GetElementDistance(index);

			// while we find a slot that is not empty and that is not in its ideal hash position
			// backward shift the elements
			while (distance > 1) {
				distance--;
				m_metadata[index - 1] = (distance << 3) | GetElementHash(index);
				m_values[index - 1] = m_values[index];
				m_identifiers[index - 1] = m_identifiers[index];
				index++;
				distance = GetElementDistance(index);
			}
			m_metadata[index - 1] = 0;
			m_count--;
		}

		ECS_INLINE bool IsItemAt(unsigned int index) const {
			return m_metadata[index] != 0;
		}

		ECS_INLINE const void* GetAllocatedBuffer() const {
			return m_values;
		}

		template<bool use_compare_function = true> 
		T GetValue(Identifier identifier) const {
			int index = Find<use_compare_function>(identifier);
			ECS_ASSERT(index != -1);
			return m_values[index];
		}

		template<bool use_compare_function = true>
		T* GetValuePtr(Identifier identifier) const {
			int index = Find<use_compare_function>(identifier);
			ECS_ASSERT(index != -1);
			return m_values + index;
		}

		// The extended capacity is used to check bounds validity
		ECS_INLINE T GetValueFromIndex(unsigned int index) const {
			ECS_ASSERT(index <= GetExtendedCapacity());
			return m_values[index];
		}

		// The extended capacity is used to check bounds validity
		ECS_INLINE T* GetValuePtrFromIndex(unsigned int index) const {
			ECS_ASSERT(index <= GetExtendedCapacity());
			return m_values + index;
		}

		ECS_INLINE unsigned int GetCount() const {
			return m_count;
		}

		ECS_INLINE unsigned int GetCapacity() const {
			return m_capacity;
		}

		ECS_INLINE unsigned int GetExtendedCapacity() const {
			return m_capacity == 0 ? 0 : m_capacity + 31;
		}

		ECS_INLINE unsigned int* GetMetadata() {
			return m_metadata;
		}

		ECS_INLINE const unsigned int* GetConstMetadata() const {
			return m_metadata;
		}

		ECS_INLINE unsigned char GetElementDistance(unsigned int index) const {
			ECS_ASSERT(index <= GetExtendedCapacity());
			return m_metadata[index] >> 3;
		}

		ECS_INLINE unsigned char GetElementHash(unsigned int index) const {
			ECS_ASSERT(index <= GetExtendedCapacity());
			return m_metadata[index] & ECS_HASH_TABLE_HASH_BITS_MASK;
		}

		ECS_INLINE T* GetValues() {
			return m_values;
		}

		ECS_INLINE const T* GetValues() const {
			return m_values;
		}

		ECS_INLINE Stream<T> GetValueStream() {
			return Stream<T>(m_values, m_capacity + 31);
		}

		ECS_INLINE const Identifier* GetIdentifiers() const {
			return m_identifiers;
		}

		void GetElementIndices(CapacityStream<unsigned int>& indices) const {
			for (unsigned int index = 0; index < GetExtendedCapacity(); index++) {
				if (IsItemAt(index)) {
					indices.AddSafe(index);
				}
			}
		}

		template<bool use_compare_function = false>
		void SetValue(T value, Identifier identifier) {
			int index = Find<use_compare_function>(identifier);
			ECS_ASSERT(index != -1);

			m_values[index] = value;
		}

		// The extended capacity is used to check bounds validity
		ECS_INLINE void SetValue(unsigned int index, T value) {
			ECS_ASSERT(index <= GetExtendedCapacity());
			m_values[index] = value;
		}

		template<bool use_compare_function = true>
		bool TryGetValue(Identifier identifier, T& value) const {
			int index = Find<use_compare_function>(identifier);
			if (index != -1) {
				value = m_values[index];
				return true;
			}
			else {
				return false;
			}
		}

		template<bool use_compare_function = true>
		bool TryGetValuePtr(Identifier identifier, T*& pointer) const {
			int index = Find<use_compare_function>(identifier);
			if (index != -1) {
				pointer = m_values + index;
				return true;
			}
			else {
				return false;
			}
		}

		ECS_INLINE static size_t MemoryOf(unsigned int number) {
			return (sizeof(unsigned char) + sizeof(T) + sizeof(Identifier)) * (number + 31);
		}

		// Used by insert dynamic to determine which is the next suitable capacity 
		// for the container to grow at
		ECS_INLINE static unsigned int NextCapacity(unsigned int capacity) {
			return TableHashFunction::Next(capacity);
		}

		// Equivalent to memcpy'ing the data from the other table
		void Copy(AllocatorPolymorphic allocator, const HashTable<T, Identifier, TableHashFunction, ObjectHashFunction>* table) {
			size_t table_size = MemoryOf(table->GetCapacity());
			void* allocation = AllocateEx(allocator, table_size);

			InitializeFromBuffer(allocation, table->GetCapacity(), 0);
			m_function = table->m_function;

			// Now blit the data - it can just be memcpy'ed from the other
			memcpy(allocation, table->GetAllocatedBuffer(), table_size);
		}

		// It will set the buffers accordingly to the buffer. It does not modify anything
		// This function is used mostly for serialization, deserialization purposes
		void SetBuffers(void* buffer, unsigned int capacity) {
			unsigned int extended_capacity = capacity + 31;

			m_values = (T*)buffer;
			uintptr_t ptr = (uintptr_t)buffer;
			ptr += sizeof(T) * extended_capacity;
			m_identifiers = (Identifier*)ptr;
			ptr += sizeof(Identifier) * extended_capacity;
			m_metadata = (unsigned char*)ptr;

			m_capacity = capacity;
			m_count = 0;
		}

		void InitializeFromBuffer(void* buffer, unsigned int capacity, size_t additional_info = 0) {
			unsigned int extended_capacity = capacity + 31;
			SetBuffers(buffer, capacity);

			// make distance 0 for keys, account for padding elements
			memset(m_metadata, 0, sizeof(unsigned char) * extended_capacity);
			m_function = TableHashFunction(additional_info);
		}

		void InitializeFromBuffer(uintptr_t& buffer, unsigned int capacity, size_t additional_info = 0) {
			InitializeFromBuffer((void*)buffer, capacity, additional_info);
			buffer += MemoryOf(capacity);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int capacity, size_t additional_info = 0) {
			size_t memory_size = MemoryOf(capacity);
			void* allocation = allocator->Allocate(memory_size, 8);
			InitializeFromBuffer(allocation, capacity, additional_info);
		}

		// The buffer given must be allocated with MemoryOf
		const void* Grow(void* buffer, unsigned int new_capacity) {
			const unsigned char* old_metadata = m_metadata;
			const T* old_values = m_values;
			const Identifier* old_identifiers = m_identifiers;
			const TableHashFunction old_hash = m_function;
			unsigned int old_capacity = m_capacity;

			InitializeFromBuffer(buffer, new_capacity);
			m_function = old_hash;

			if (old_capacity > 0) {
				unsigned int extended_old_capacity = old_capacity + 31;
				for (unsigned int index = 0; index < extended_old_capacity; index++) {
					bool is_item = old_metadata[index] != 0;
					if (is_item) {
						Insert(old_values[index], old_identifiers[index]);
					}
				}
			}

			return old_values;
		}

	//private:
		unsigned char* m_metadata;
		T* m_values;
		Identifier* m_identifiers;
		unsigned int m_capacity;
		unsigned int m_count;
		TableHashFunction m_function;

#undef ECS_HASH_TABLE_DISTANCE_MASK
#undef ECS_HASH_TABLE_HASH_BITS_MASK
	};

	// Helper function that eases the process of dynamic identifier hash tables; it takes care of allocating and
	// deallocating when necessary
	template<typename Table, typename Allocator, typename Value, typename Identifier>
	void InsertIntoDynamicTable(Table& table, Allocator* allocator, Value value, Identifier identifier) {
		auto grow = [&]() {
			unsigned int old_capacity = table.GetCapacity();

			unsigned int new_capacity = Table::NextCapacity(table.GetCapacity());
			void* new_allocation = allocator->Allocate(table.MemoryOf(new_capacity));
			const void* old_allocation = table.Grow(new_allocation, new_capacity);
			if (old_capacity > 0) {
				allocator->Deallocate(old_allocation);
			}
		};

		if (table.GetCapacity() == 0) {
			grow();
		}
		if (table.Insert(value, identifier)) {
			grow();
		}
	}

	template<typename T>
	using HashTableDefault = HashTable<T, ResourceIdentifier, HashFunctionPowerOfTwo>;

}