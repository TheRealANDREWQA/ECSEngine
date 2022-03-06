#pragma once
#include "ecspch.h"
#include "../Core.h"
#include "Stream.h"
#include "../Utilities/Function.h"

#ifndef ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR
#define ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR 85
#endif

#ifndef ECS_HASHTABLE_DYNAMIC_GROW_FACTOR
#define ECS_HASHTABLE_DYNAMIC_GROW_FACTOR 1.5f
#endif

namespace ECSEngine {

#define ECS_RESOURCE_IDENTIFIER(name) ResourceIdentifier identifier = ResourceIdentifier(name, strlen(name));

	// filename can be used as a general purpose pointer if other identifier than the filename is used
	// Compare function uses AVX2 32 byte SIMD char compare
	struct ECSENGINE_API ResourceIdentifier {
		ResourceIdentifier();
		ResourceIdentifier(const char* filename);
		ResourceIdentifier(const wchar_t* filename);
		// if the identifier is something other than a LPCWSTR path
		ResourceIdentifier(const void* id, unsigned int size);
		ResourceIdentifier(Stream<void> identifier);

		ResourceIdentifier(const ResourceIdentifier& other) = default;
		ResourceIdentifier& operator = (const ResourceIdentifier& other) = default;

		bool operator == (const ResourceIdentifier& other) const;

		bool Compare(const ResourceIdentifier& other) const;

		const void* ptr;
		unsigned int size;
	};

	struct ECSENGINE_API HashFunctionMultiplyString {
		static unsigned int Hash(Stream<const char> string);
		static unsigned int Hash(Stream<const wchar_t> string);
		static unsigned int Hash(const char* string);
		static unsigned int Hash(const wchar_t* string);
		static unsigned int Hash(const void* identifier, unsigned int identifier_size);
		static unsigned int Hash(ResourceIdentifier identifier);
	};

	struct ECSENGINE_API HashFunctionPowerOfTwo {
	public:
		HashFunctionPowerOfTwo() {}
		HashFunctionPowerOfTwo(size_t additional_info) {}
		unsigned int operator ()(unsigned int key, unsigned int capacity) const {
			return key & (capacity - 1);
		}

		static unsigned int Next(unsigned int capacity) {
			// If the value is really small make it to 16
			if (capacity < 16) {
				return 16;
			}
			return function::PowerOfTwoGreater(capacity).x;
		}
	};

	struct ECSENGINE_API HashFunctionPrimeNumber {
	public:
		HashFunctionPrimeNumber() {}
		HashFunctionPrimeNumber(size_t additional_info) {}

		unsigned int operator () (unsigned int key, unsigned int capacity) const {
			switch (capacity) {
			case 11:
				return key % 11u;
			case 23:
				return key % 23u;
			case 41:
				return key % 41u;
			case 67:
				return key % 67u;
			case 97:
				return key % 97u;
			case 179:
				return key % 179u;
			case 331:
				return key % 331u;
			case 503:
				return key % 503u;
			case 617:
				return key % 617u;
			case 773:
				return key % 773u;
			case 919:
				return key % 919u;
			case 1063:
				return key % 1063u;
			case 1237:
				return key % 1237u;
			case 1511:
				return key % 1511u;
			case 1777:
				return key % 1777u;
			case 2003:
				return key % 2003u;
			case 2381:
				return key % 2381u;
			case 2707:
				return key % 2707u;
			case 3049:
				return key % 3049u;
			case 3463:
				return key % 3463u;
			case 3911:
				return key % 3911u;
			case 4603:
				return key % 4603u;
			case 5231:
				return key % 5231u;
			case 6007:
				return key % 6007u;
			case 7333:
				return key % 7333u;
			case 8731:
				return key % 8731u;
			case 9973:
				return key % 9973u;
			case 12203:
				return key % 12203u;
			case 16339:
				return key % 16339u;
			case 20521:
				return key % 20521u;
			case 27127:
				return key % 27127u;
			case 33587:
				return key % 33587u;
			case 40187:
				return key % 40187u;
			case 48611:
				return key % 48611u;
			case 58169:
				return key % 58169u;
			case 71707:
				return key % 71707u;
			case 82601:
				return key % 82601u;
			case 94777:
				return key % 94777u;
			case 115153:
				return key % 115153u;
			case 127453:
				return key % 127453u;
			case 149011:
				return key % 149011u;
			case 181889:
				return key % 181889u;
			case 219533:
				return key % 219533u;
			case 260959:
				return key % 260959u;
			case 299993:
				return key % 299993u;
			case 354779:
				return key % 354779u;
			case 435263:
				return key % 435263u;
			case 511087:
				return key % 511087u;
			case 587731:
				return key % 587731u;
			case 668201:
				return key % 668201u;
			case 781087:
				return key % 781807u;
			case 900001:
				return key % 900001u;
			case 999983:
				return key % 999983u;
			}
		}

		static unsigned int Next(unsigned int capacity) {
			switch (capacity) {
			case 0:
				return 11;
			case 11:
				return 23;
			case 23:
				return 41;
			case 41:
				return 67;
			case 67:
				return 97;
			case 97:
				return 179;
			case 179:
				return 331;
			case 331:
				return 503;
			case 503:
				return 617;
			case 617:
				return 773;
			case 773:
				return 919;
			case 919:
				return 1063;
			case 1063:
				return 1237;
			case 1237:
				return 1511;
			case 1511:
				return 1777;
			case 1777:
				return 2003;
			case 2003:
				return 2381;
			case 2381:
				return 2707;
			case 2707:
				return 3049;
			case 3049:
				return 3463;
			case 3463:
				return 3911;
			case 3911:
				return 4603;
			case 4603:
				return 5231;
			case 5231:
				return 6007;
			case 6007:
				return 7333;
			case 7333:
				return 8731;
			case 8731:
				return 9973;
			case 9973:
				return 12203;
			case 12203:
				return 16339;
			case 16339:
				return 20521;
			case 20521:
				return 27127;
			case 27127:
				return 33587;
			case 33587:
				return 40187;
			case 40187:
				return 48611;
			case 48611:
				return 58169;
			case 58169:
				return 71707;
			case 71707:
				return 82601;
			case 82601:
				return 94777;
			case 94777:
				return 115153;
			case 115153:
				return 127453;
			case 127453:
				return 149011;
			case 149011:
				return 181889;
			case 181889:
				return 219533;
			case 219533:
				return 260959;
			case 260959:
				return 299993;
			case 299993:
				return 354779;
			case 354779:
				return 435263;
			case 435263:
				return 511087;
			case 511087:
				return 587731;
			case 587731:
				return 668201;
			case 668201:
				return 781087;
			case 781087:
				return 900001;
			case 900001:
				return 999983;
			default:
				ECS_ASSERT(false);
			}

			return -1;
		}

		static size_t PrimeCapacity(size_t index) {
			switch (index) {
			case 1:
				return 11;
			case 2:
				return 23;
			case 3:
				return 41;
			case 4:
				return 67;
			case 5:
				return 97;
			case 6:
				return 179;
			case 7:
				return 331;
			case 8:
				return 503;
			case 9:
				return 617;
			case 10:
				return 773;
			case 11:
				return 919;
			case 12:
				return 1063;
			case 13:
				return 1237;
			case 14:
				return 1511;
			case 15:
				return 1777;
			case 16:
				return 2003;
			case 17:
				return 2381;
			case 18:
				return 2707;
			case 19:
				return 3049;
			case 20:
				return 3463;
			case 21:
				return 3911;
			case 22:
				return 4603;
			case 23:
				return 5231;
			case 24:
				return 6007;
			case 25:
				return 7333;
			case 26:
				return 8731;
			case 27:
				return 9973;
			case 28:
				return 12203;
			case 29:
				return 16339;
			case 30:
				return 20521;
			case 31:
				return 27127;
			case 32:
				return 33587;
			case 33:
				return 40187;
			case 34:
				return 48611;
			case 35:
				return 58169;
			case 36:
				return 71707;
			case 37:
				return 82601;
			case 38:
				return 94777;
			case 39:
				return 115153;
			case 40:
				return 127453;
			case 41:
				return 149011;
			case 42:
				return 181889;
			case 43:
				return 219533;
			case 44:
				return 260959;
			case 45:
				return 299993;
			case 46:
				return 354779;
			case 47:
				return 435263;
			case 48:
				return 511087;
			case 49:
				return 587731;
			case 50:
				return 668201;
			case 51:
				return 781087;
			case 52:
				return 900001;
			case 53:
				return 999983;
			}
		}
	};

	struct ECSENGINE_API HashFunctionFibonacci {
	public:
		HashFunctionFibonacci() : m_shift_amount(64) {}
		HashFunctionFibonacci(size_t additional_info) : m_shift_amount(64 - additional_info) {}
		unsigned int operator () (unsigned int key, unsigned int capacity) const {
			return (key * 11400714819323198485llu) >> m_shift_amount;
		}

		static unsigned int Next(unsigned int capacity) {
			return (unsigned int)((float)capacity * ECS_HASHTABLE_DYNAMIC_GROW_FACTOR + 2);
		}

	//private:
		size_t m_shift_amount;
	};

	struct ECSENGINE_API HashFunctionXORFibonacci {
	public:
		HashFunctionXORFibonacci() : m_shift_amount(64) {}
		HashFunctionXORFibonacci(size_t additional_info) : m_shift_amount(64 - additional_info) {}
		unsigned int operator() (unsigned int key, unsigned int capacity) const {
			key ^= key >> m_shift_amount;
			return (key * 11400714819323198485llu) >> m_shift_amount;
		}

		static unsigned int Next(unsigned int capacity) {
			return (unsigned int)((float)capacity * ECS_HASHTABLE_DYNAMIC_GROW_FACTOR + 2);
		}

	//private:
		size_t m_shift_amount;
	};
		
	struct ECSENGINE_API HashFunctionFolding {
	public:
		HashFunctionFolding() {}
		HashFunctionFolding(size_t additional_info) {}
		unsigned int operator() (unsigned int key, unsigned int capacity) const {
			return (key & 0x0000FFFF + (key & 0xFFFF0000) >> 16) & (capacity - 1);
		}

		static unsigned int Next(unsigned int capacity) {
			// If the value is really small, make it 16
			if (capacity < 16) {
				return 16;
			}
			return function::PowerOfTwoGreater(capacity).x;
		}
	};

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
	template<typename T, typename Identifier, typename TableHashFunction, typename ObjectHashFunction>
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

			if (horizontal_or(match)) {
				unsigned int match_bits = to_bits(match);
				unsigned long vector_index = 0;
				unsigned long offset = 0;

				// Keep an offset because when a false positive is detected, that bit must be eliminated and in order to avoid
				// using masks, shift to the right to make that bit 0
				while (_BitScanForward(&vector_index, match_bits)) {
					if constexpr (!use_compare_function) {
						if (m_identifiers[index + offset + vector_index] == identifier) {
							return index + offset + vector_index;
						}
					}
					else {
						if (m_identifiers[index + offset + vector_index].Compare(identifier)) {
							return index + offset + vector_index;
						}
					}
					match_bits >>= vector_index + 1;
					offset += vector_index + 1;
				}
			}

			return -1;
		}

		// the return value tells the caller if the hash table needs to grow or allocate another hash table
		bool Insert(T value, Identifier identifier) {
			unsigned int dummy;
			return Insert(value, identifier, dummy);
		}

		// the return value tells the caller if the hash table needs to grow or allocate another hash table
		bool Insert(T value, Identifier identifier, unsigned int& position) {
			unsigned int key = ObjectHashFunction::Hash(identifier);

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
					position = index;
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

		void InitializeFromBuffer(void* buffer, unsigned int capacity, size_t additional_info = 0) {
			unsigned int extended_capacity = capacity + 31;

			m_values = (T*)buffer;
			uintptr_t ptr = (uintptr_t)buffer;
			ptr += sizeof(T) * extended_capacity;
			m_identifiers = (Identifier*)ptr;
			ptr += sizeof(Identifier) * extended_capacity;
			m_metadata = (unsigned char*)ptr;

			// make distance 0 for keys, account for padding elements
			memset(m_metadata, 0, sizeof(unsigned char) * extended_capacity);

			m_capacity = capacity;
			m_count = 0;
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

			for (unsigned int index = 0; index < old_capacity; index++) {
				bool is_item = old_metadata[index] != 0;
				if (is_item) {
					Insert(old_values[index], old_identifiers[index]);
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

	/* Robin Hood hashing identifier hash table that stores in separate buffers keys and values. Keys are
	* unsigned shorts that have stored the robin hood distance as the highest 6 bits and the other 10 bits for key bits
	* in order to not create an additional metadata buffer, not get an extra cache miss and use SIMD probing.
	* Distance 0 means empty slot, distance 1 means that the key is where it hashed. The maximum distance an element can have is 63.
	* The capacity should be set to a power of two when using power of two hash function.
	* There are 63 padding elements at the end in order to eliminate wrap around and to effectively use only SIMD operations.
	* Use MemoryOf to get the number of bytes needed for the buffers. It does a identifier check when trying to retrieve the value.
	* Identifier type must implement Compare function because it supports checking for equality with a user defined operation other
	* than assignment operator e. g. for pointers, they might be equal if the data they point to is the same
	* Identifier must be stable, this table only keeps the pointer, not the data pointed to.
	*/
	template<typename T, typename Identifier, typename TableHashFunction, typename ObjectHashFunction>
	struct ExtendedHashTable {
	public:
#define ECS_EXTENDED_HASH_TABLE_DISTANCE_MASK 0xFB00
#define ECS_EXTENDED_HASH_TABLE_HASH_BITS_MAKK 0x03FF

		ExtendedHashTable() : m_metadata(nullptr), m_values(nullptr), m_identifiers(nullptr), m_capacity(0), m_count(0), m_function(TableHashFunction(0)) {}
		ExtendedHashTable(void* buffer, unsigned int capacity, size_t additional_info = 0) {
			InitializeFromBuffer(buffer, capacity, additional_info);
		}

		ExtendedHashTable(const ExtendedHashTable& other) = default;
		ExtendedHashTable<T, Identifier, TableHashFunction, ObjectHashFunction>& operator = (const ExtendedHashTable<T, Identifier, TableHashFunction, ObjectHashFunction>& other) = default;

		void Clear() {
			for (size_t index = 0; index < m_capacity + 63; index++) {
				m_metadata[index] = (unsigned char)0;
			}
			m_count = 0;
		}

		template<bool use_compare_function = true>
		unsigned int Find(Identifier identifier) const {
			unsigned int key = ObjectHashFunction::Hash(identifier);

			// calculating the index for the array with the hash function
			unsigned int index = m_function(key, m_capacity);

			unsigned short key_hash_bits = key;
			key_hash_bits &= ECS_EXTENDED_HASH_TABLE_HASH_BITS_MAKK;

			Vec16us key_bits(key_hash_bits), elements, ignore_distance(ECS_EXTENDED_HASH_TABLE_HASH_BITS_MAKK);
			Vec16us zero = _mm256_setzero_si256();

			// Using a lambda will introduce extra overhead - (calling convention?) - by checking the return explicitly
			// Using a macro is kind of ugly - it is what it is tho
				
#define ITERATION(index) elements.load(m_metadata + index); \
			Vec16us element_hash_bits = elements & ignore_distance; \
			auto are_zero = elements == zero; \
			auto match = element_hash_bits == key_bits; \
			match &= ~are_zero; \
\
			if (horizontal_or(match)) { \
				unsigned int match_bits = to_bits(match); \
				unsigned long vector_index = 0; \
				unsigned long offset = 0; \
\
				/* Keep an offset because when a false positive is detected, that bit must be eliminated and in order to avoid */ \
				/* using masks, shift to the right to make that bit 0 */ \
				while (_BitScanForward(&vector_index, match_bits)) { \
					if constexpr (!use_compare_function) { \
						if (m_identifiers[index + offset + vector_index] == identifier) { \
							return index + offset + vector_index; \
						} \
					} \
					else { \
						if (m_identifiers[index + offset + vector_index].Compare(identifier)) { \
							return index + offset + vector_index; \
						} \
					} \
					match_bits >>= vector_index + 1; \
					offset += vector_index + 1; \
				} \
			}

			ITERATION(index);
			ITERATION(index + 16);
			ITERATION(index + 32);
			ITERATION(index + 48);

			return -1;
		}

		// the return value tells the caller if the hash table needs to grow or allocate another hash table
		bool Insert(T value, Identifier identifier) {
			unsigned int dummy;
			return Insert(value, identifier, dummy);
		}

		// the return value tells the caller if the hash table needs to grow or allocate another hash table
		bool Insert(T value, Identifier identifier, unsigned int& position) {
			unsigned int key = ObjectHashFunction::Hash(identifier);

			// calculating the index at which the key wants to be
			unsigned int index = m_function(key, m_capacity);
			unsigned int hash = index;
			unsigned int distance = 1;

			unsigned short hash_key_bits = key;
			hash_key_bits &= ECS_EXTENDED_HASH_TABLE_HASH_BITS_MAKK;

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
					hash_key_bits |= distance << 10;
					m_metadata[index] = hash_key_bits;
					m_values[index] = value;
					m_identifiers[index] = identifier;
					m_count++;
					position = index;
					break;
				}

				// if it finds a richer slot than us, we swap the keys and keep searching to find 
				// an empty slot for the richer slot
				else {
					unsigned char metadata_temp = m_metadata[index];
					T value_temp = m_values[index];
					Identifier identifier_temp = m_identifiers[index];
					m_metadata[index] = hash_key_bits | (distance << 10);
					m_values[index] = value;
					m_identifiers[index] = identifier;
					distance = (metadata_temp >> 10) + 1;
					hash_key_bits = metadata_temp & ECS_EXTENDED_HASH_TABLE_HASH_BITS_MAKK;
					value = value_temp;
					identifier = identifier_temp;
					index++;
				}
			}
			if (m_count * 100 / m_capacity > ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR || distance >= 63)
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
				m_metadata[index - 1] = (distance << 10) | GetElementHash(index);
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
			return m_capacity + 63;
		}

		ECS_INLINE unsigned short* GetMetadata() {
			return m_metadata;
		}

		ECS_INLINE const unsigned short* GetConstMetadata() const {
			return m_metadata;
		}

		ECS_INLINE unsigned char GetElementDistance(unsigned int index) const {
			ECS_ASSERT(index <= GetExtendedCapacity());
			return m_metadata[index] >> 10;
		}

		ECS_INLINE unsigned short GetElementHash(unsigned int index) const {
			ECS_ASSERT(index <= GetExtendedCapacity());
			return m_metadata[index] & ECS_EXTENDED_HASH_TABLE_HASH_BITS_MAKK;
		}

		ECS_INLINE T* GetValues() {
			return m_values;
		}

		ECS_INLINE const T* GetValues() const {
			return m_values;
		}

		ECS_INLINE Stream<T> GetValueStream() {
			return Stream<T>(m_values, m_capacity + 63);
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
			return (sizeof(unsigned short) + sizeof(T) + sizeof(Identifier)) * (number + 63);
		}

		// Used by insert dynamic to determine which is the next suitable capacity 
		// for the container to grow at
		ECS_INLINE static unsigned int NextCapacity(unsigned int capacity) {
			return TableHashFunction::Next(capacity);
		}

		void InitializeFromBuffer(void* buffer, unsigned int capacity, size_t additional_info = 0) {
			unsigned int extended_capacity = capacity + 63;

			m_values = (T*)buffer;
			uintptr_t ptr = (uintptr_t)buffer;
			ptr += sizeof(T) * extended_capacity;
			m_identifiers = (Identifier*)ptr;
			ptr += sizeof(Identifier) * extended_capacity;
			m_metadata = (unsigned short*)ptr;

			// make distance 0 for keys, account for padding elements
			memset(m_metadata, 0, sizeof(unsigned short) * extended_capacity);

			m_capacity = capacity;
			m_count = 0;
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
			const unsigned short* old_metadata = m_metadata;
			const T* old_values = m_values;
			const Identifier* old_identifiers = m_identifiers;
			const TableHashFunction old_hash = m_function;
			unsigned int old_capacity = m_capacity;

			InitializeFromBuffer(buffer, new_capacity);
			m_function = old_hash;

			for (unsigned int index = 0; index < old_capacity; index++) {
				bool is_item = old_metadata[index] != 0;
				if (is_item) {
					unsigned int hash = ObjectHashFunction::Hash(old_identifiers[index]);
					hash = m_function(hash, m_capacity);
					Insert(hash, old_values[index], old_identifiers[index]);
				}
			}

			return old_values;
		}

		//private:
		unsigned short* m_metadata;
		T* m_values;
		Identifier* m_identifiers;
		unsigned int m_capacity;
		unsigned int m_count;
		TableHashFunction m_function;

#undef ECS_EXTENDED_HASH_TABLE_DISTANCE_MASK
#undef ECS_EXTENDED_HASH_TABLE_HASH_BITS_MASK
	};

	// Helper function that eases the process of dynamic identifier hash tables; it takes care of allocating and
	// deallocating when necessary
	template<typename Table, typename Allocator, typename Value, typename Identifier>
	void InsertToDynamicTable(Table& table, Allocator* allocator, Value value, Identifier identifier) {
		auto grow = [&]() {
			unsigned int new_capacity = Table::NextCapacity(table.GetCapacity());
			void* new_allocation = allocator->Allocate(table.MemoryOf(new_capacity));
			const void* old_allocation = table.Grow(new_allocation, new_capacity);
			allocator->Deallocate(old_allocation);
		};

		if (table.GetCapacity() == 0) {
			grow();
		}
		if (table.Insert(value, identifier)) {
			grow();
		}
	}

	template<typename T>
	using HashTableDefault = HashTable<T, ResourceIdentifier, HashFunctionPowerOfTwo, HashFunctionMultiplyString>;

}