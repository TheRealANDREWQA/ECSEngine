#pragma once
#include "../Core.h"
#include "../Math/VCLExtensions.h"
#include "Hashing.h"

#ifndef ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR
#define ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR 95
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
	* Optionally can specify SoA if you want the Identifier* and T* to be separated
	*/
	template<
		typename T, 
		typename Identifier,
		typename TableHashFunction,
		typename ObjectHashFunction = ObjectHashFallthrough,
		bool SoA = false
	>
	struct HashTable {
		struct Pair {
			T value;
			Identifier identifier;
		};

#define ECS_HASH_TABLE_DISTANCE_MASK 0xF8
#define ECS_HASH_TABLE_HASH_BITS_MASK 0x07

		HashTable() : m_metadata(nullptr), m_buffer(nullptr), m_identifiers(nullptr), m_capacity(0), m_count(0), m_function(TableHashFunction(0)) {}
		HashTable(void* buffer, unsigned int capacity, size_t additional_info = 0) {
			InitializeFromBuffer(buffer, capacity, additional_info);
		}

		HashTable(const HashTable& other) = default;
		HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>& operator = (const HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>& other) = default;

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

			//
			//  key_bits(key_hash_bits), elements, ignore_distance(ECS_HASH_TABLE_HASH_BITS_MASK);
			//// Only elements that hashed to this slot should be checked
			//Vec32uc corresponding_distance(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32);

			//// Exclude elements that have distance different from the distance to the current slot
			//elements.load(m_metadata + index);				
			//Vec32uc element_hash_bits = elements & ignore_distance;
			//auto are_hashed_to_same_slot = (elements >> 3) == corresponding_distance;
			//auto match = element_hash_bits == key_bits;
			//match &= are_hashed_to_same_slot;

			Vec32cb match = HashTableFindSIMDKernel(index, m_metadata, key_hash_bits, ECS_HASH_TABLE_HASH_BITS_MASK);

			unsigned int return_value = -1;
			ForEachBit(match, [&](unsigned int bit_index) {
				Identifier current_identifier = GetIdentifierFromIndex(index + bit_index);

				if constexpr (!use_compare_function) {
					if (current_identifier == identifier) {
						return_value = index + bit_index;
						return true;
					}
				}
				else {
					if (current_identifier.Compare(identifier)) {
						return_value = index + bit_index;
						return true;
					}
				}
				return false;
			});

			return return_value;
		}

		// It will be called with a single argument - the index of the element
		// For the early exit it must return true when to exit, else false.
		// For non early exit it must return true if the current element is being deleted, else false.
		template<bool early_exit = false, typename Functor>
		void ForEachIndex(Functor&& functor) {
			unsigned int extended_capacity = GetExtendedCapacity();
			for (int index = 0; index < (int)extended_capacity; index++) {
				if (IsItemAt(index)) {
					if constexpr (early_exit) {
						if (functor(index)) {
							break;
						}
					}
					else {
						index -= functor(index);
					}
				}
			}
		}

		// For the early exit it must return true when to exit, else false.
		// Else void return
		template<bool early_exit = false, typename Functor>
		void ForEachIndexConst(Functor&& functor) const {
			unsigned int extended_capacity = GetExtendedCapacity();
			for (unsigned int index = 0; index < extended_capacity; index++) {
				if (IsItemAt(index)) {
					if constexpr (early_exit) {
						if (functor(index)) {
							break;
						}
					}
					else {
						functor(index);
					}
				}
			}
		}

		// For the early exit it must return true when to exit, else false.
		// Else void return.
		// First parameter - the value, the second one - the identifier
		template<bool early_exit = false, typename Functor>
		void ForEach(Functor&& functor) {
			unsigned int extended_capacity = GetExtendedCapacity();
			Identifier* identifiers = nullptr;
			T* values = nullptr;
			Pair* pairs = nullptr;

			if constexpr (SoA) {
				identifiers = GetIdentifiers();
				values = GetValues();
			}
			else {
				pairs = GetPairs();
			}

			for (unsigned int index = 0; index < extended_capacity; index++) {
				if (IsItemAt(index)) {
					if constexpr (SoA) {
						if constexpr (early_exit) {
							if (functor(values[index], identifiers[index])) {
								break;
							}
						}
						else {
							functor(values[index], identifiers[index]);
						}
					}
					else {
						if constexpr (early_exit) {
							if (functor(pairs[index].value, pairs[index].identifier)) {
								break;
							}
						}
						else {
							functor(pairs[index].value, pairs[index].identifier);
						}
					}
				}
			}
		}

		// Const variant
		template<bool early_exit = false, typename Functor>
		void ForEachConst(Functor&& functor) const {
			unsigned int extended_capacity = GetExtendedCapacity();
			const Identifier* identifiers = nullptr;
			const T* values = nullptr;
			const Pair* pairs = nullptr;

			if constexpr (SoA) {
				identifiers = GetIdentifiers();
				values = GetValues();
			}
			else {
				pairs = GetPairs();
			}

			for (unsigned int index = 0; index < extended_capacity; index++) {
				if (IsItemAt(index)) {
					if constexpr (SoA) {
						if constexpr (early_exit) {
							if (functor(values[index], identifiers[index])) {
								break;
							}
						}
						else {
							functor(values[index], identifiers[index]);
						}
					}
					else {
						if constexpr (early_exit) {
							if (functor(pairs[index].value, pairs[index].identifier)) {
								break;
							}
						}
						else {
							functor(pairs[index].value, pairs[index].identifier);
						}
					}
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
					SetEntry(index, value, identifier);
					m_count++;
					position = position == -1 ? index : position;
					break;
				}

				// if it finds a richer slot than us, we swap the keys and keep searching to find 
				// an empty slot for the richer slot
				else {
					unsigned char metadata_temp = m_metadata[index];
					
					T current_value = value;
					Identifier current_identifier = identifier;
					GetEntry(index, value, identifier);
					SetEntry(index, current_value, current_identifier);

					m_metadata[index] = hash_key_bits | (distance << 3);
					distance = (metadata_temp >> 3) + 1;
					
					hash_key_bits = metadata_temp & ECS_HASH_TABLE_HASH_BITS_MASK;
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
				
				T current_value;
				Identifier current_identifier;
				GetEntry(index, current_value, current_identifier);
				SetEntry(index - 1, current_value, current_identifier);
				
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
			return m_buffer;
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

		// Only valid in SoA type
		template<typename = std::enable_if_t<SoA == true>>
		ECS_INLINE T* GetValues() {
			return (T*)m_buffer;
		}

		// Only valid in SoA type
		template<typename = std::enable_if_t<SoA == true>>
		ECS_INLINE const T* GetValues() const {
			return (const T*)m_buffer;
		}

		// Only valid in SoA type
		template<typename = std::enable_if_t<SoA == true>>
		ECS_INLINE Stream<T> GetValueStream() {
			return Stream<T>(GetValues(), GetExtendedCapacity());
		}

		// Only valid in SoA type
		template<typename = std::enable_if_t<SoA == true>>
		ECS_INLINE Identifier* GetIdentifiers() {
			return m_identifiers;
		}

		// Only valid in SoA type
		template<typename = std::enable_if_t<SoA == true>>
		ECS_INLINE const Identifier* GetIdentifiers() const {
			return m_identifiers;
		}

		// Only valid in AoS type
		template<typename = std::enable_if_t<SoA == false>>
		ECS_INLINE Pair* GetPairs() {
			return (Pair*)m_buffer;
		}

		// Only valid in AoS type
		template<typename = std::enable_if_t<SoA == false>>
		ECS_INLINE const Pair* GetPairs() const {
			return (const Pair*)m_buffer;
		}

		// The extended capacity is used to check bounds validity
		ECS_INLINE T GetValueFromIndex(unsigned int index) const {
			if constexpr (SoA) {
				const T* values = GetValues();
				return values[index];
			}
			else {
				const Pair* pairs = GetPairs();
				return pairs[index].value;
			}
		}

		// The extended capacity is used to check bounds validity
		ECS_INLINE T* GetValuePtrFromIndex(unsigned int index) {
			if constexpr (SoA) {
				T* values = GetValues();
				return values + index;
			}
			else {
				Pair* pairs = GetPairs();
				return &pairs[index].value;
			}
		}

		ECS_INLINE const T* GetValuePtrFromIndex(unsigned int index) const {
			if constexpr (SoA) {
				const T* values = GetValues();
				return values + index;
			}
			else {
				const Pair* pairs = GetPairs();
				return &pairs[index].value;
			}
		}

		ECS_INLINE Identifier GetIdentifierFromIndex(unsigned int index) const {
			if constexpr (SoA) {
				const Identifier* identifiers = GetIdentifiers();
				return identifiers[index];
			}
			else {
				const Pair* pairs = GetPairs();
				return pairs[index].identifier;
			}
		}

		ECS_INLINE Identifier* GetIdentifierPtrFromIndex(unsigned int index) {
			if constexpr (SoA) {
				Identifier* identifiers = GetIdentifiers();
				return identifiers + index;
			}
			else {
				Pair* pairs = GetPairs();
				return &pairs[index].identifier;
			}
		}

		ECS_INLINE const Identifier* GetIdentifierPtrFromIndex(unsigned int index) const {
			if constexpr (SoA) {
				const Identifier* identifiers = GetIdentifiers();
				return identifiers + index;
			}
			else {
				const Pair* pairs = GetPairs();
				return &pairs[index].identifier;
			}
		}

		template<bool use_compare_function = true>
		T GetValue(Identifier identifier) const {
			unsigned int index = Find<use_compare_function>(identifier);
			ECS_ASSERT(index != -1);
			return GetValueFromIndex(index);
		}

		template<bool use_compare_function = true>
		T* GetValuePtr(Identifier identifier) {
			unsigned int index = Find<use_compare_function>(identifier);
			ECS_ASSERT(index != -1);
			return GetValuePtrFromIndex(index);
		}

		template<bool use_compare_function = true>
		const T* GetValuePtr(Identifier identifier) const {
			unsigned int index = Find<use_compare_function>(identifier);
			ECS_ASSERT(index != -1);
			return GetValuePtrFromIndex(index);
		}

		void GetElementIndices(CapacityStream<unsigned int>& indices) const {
			for (unsigned int index = 0; index < GetExtendedCapacity(); index++) {
				if (IsItemAt(index)) {
					indices.AddSafe(index);
				}
			}
		}

		// If you have a pointer to a value from the table and wants to get its index
		// use this function
		ECS_INLINE unsigned int ValuePtrIndex(const T* ptr) const {
			if constexpr (SoA) {
				return ptr - GetValues();
			}
			else {
				const Pair* pairs = GetPairs();
				return function::PointerDifference(ptr, pairs) / sizeof(Pair);
			}
		}

		ECS_INLINE unsigned int IdentifierPtrIndex(const Identifier* identifier) const {
			if constexpr (SoA) {
				return identifier - GetIdentifiers();
			}
			else {
				const Pair* pairs = GetPairs();
				return function::PointerDifference(function::OffsetPointer(identifier, -sizeof(T)), pairs) / sizeof(Pair);
			}
		}

		ECS_INLINE void GetEntry(unsigned int index, T& value, Identifier& identifier) const {
			value = GetValueFromIndex(index);
			identifier = GetIdentifierFromIndex(index);
		}

		ECS_INLINE void SetEntry(unsigned int index, T value, Identifier identifier) {
			*GetValuePtrFromIndex(index) = value;
			*GetIdentifierPtrFromIndex(index) = identifier;
		}

		template<bool use_compare_function = false>
		void SetValue(T value, Identifier identifier) {
			int index = Find<use_compare_function>(identifier);
			ECS_ASSERT(index != -1);
			SetValue(index, value);
		}

		// The extended capacity is used to check bounds validity
		ECS_INLINE void SetValue(unsigned int index, T value) {
			*GetValuePtrFromIndex(index) = value;
		}

		template<bool use_compare_function = true>
		bool TryGetValue(Identifier identifier, T& value) const {
			unsigned int index = Find<use_compare_function>(identifier);
			if (index != -1) {
				value = GetValueFromIndex(index);
				return true;
			}
			else {
				return false;
			}
		}

		template<bool use_compare_function = true>
		bool TryGetValuePtr(Identifier identifier, T*& pointer) {
			unsigned int index = Find<use_compare_function>(identifier);
			if (index != -1) {
				pointer = GetValuePtrFromIndex(index);
				return true;
			}
			else {
				return false;
			}
		}

		ECS_INLINE static size_t MemoryOf(unsigned int number) {
			if constexpr (SoA) {
				return (sizeof(unsigned char) + sizeof(T) + sizeof(Identifier)) * (number + 31);
			}
			else {
				return (sizeof(unsigned char) + sizeof(Pair)) * (number + 31);
			}
		}

		// Used by insert dynamic to determine which is the next suitable capacity 
		// for the container to grow at
		ECS_INLINE static unsigned int NextCapacity(unsigned int capacity) {
			return TableHashFunction::Next(capacity);
		}

		// Equivalent to memcpy'ing the data from the other table
		void Copy(AllocatorPolymorphic allocator, const HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>* table) {
			size_t table_size = MemoryOf(table->GetCapacity());
			void* allocation = AllocateEx(allocator, table_size);

			InitializeFromBuffer(allocation, table->GetCapacity(), 0);
			m_function = table->m_function;

			// Now blit the data - it can just be memcpy'ed from the other
			memcpy(allocation, table->GetAllocatedBuffer(), table_size);
		}

		// It will set the internal hash buffers accordingly to the new hash buffer. It does not modify anything
		// This function is used mostly for serialization, deserialization purposes
		void SetBuffers(void* buffer, const unsigned int capacity) {
			unsigned int extended_capacity = capacity + 31;

			uintptr_t ptr = (uintptr_t)buffer;
			m_buffer = buffer;
			if constexpr (SoA) {
				ptr += sizeof(T)* extended_capacity;
				m_identifiers = (Identifier*)ptr;
				ptr += sizeof(Identifier) * extended_capacity;
			}
			else {
				ptr += sizeof(Pair) * extended_capacity;
				m_identifiers = nullptr;
			}

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

		void Initialize(AllocatorPolymorphic allocator, unsigned int capacity, size_t additional_info = 0) {
			size_t memory_size = MemoryOf(capacity);
			void* allocation = Allocate(allocator, memory_size, 8);
			InitializeFromBuffer(allocation, capacity, additional_info);
		}

		// The buffer given must be allocated with MemoryOf
		const void* Grow(void* buffer, unsigned int new_capacity) {
			const unsigned char* old_metadata = m_metadata;
			const void* old_buffer = m_buffer;
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
						if constexpr (SoA) {
							const T* old_values = (T*)old_buffer;
							Insert(old_values[index], old_identifiers[index]);
						}
						else {
							const Pair* old_pairs = (const Pair*)old_buffer;
							Insert(old_pairs[index].value, old_pairs[index].identifier);
						}
					}
				}
			}

			return old_buffer;
		}

		unsigned char* m_metadata;
		void* m_buffer;
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

	template<typename Table, typename Value, typename Identifier>
	void InsertIntoDynamicTable(Table& table, AllocatorPolymorphic allocator, Value value, Identifier identifier) {
		auto grow = [&]() {
			unsigned int old_capacity = table.GetCapacity();

			unsigned int new_capacity = Table::NextCapacity(table.GetCapacity());
			void* new_allocation = AllocateEx(allocator, table.MemoryOf(new_capacity));
			const void* old_allocation = table.Grow(new_allocation, new_capacity);
			if (old_capacity > 0) {
				DeallocateEx(allocator, old_allocation);
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