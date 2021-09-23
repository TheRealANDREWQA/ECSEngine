#pragma once
#include "ecspch.h"
#include "../Core.h"
#include "Stream.h"
#include "../Utilities/Function.h"

#ifndef ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR
#define ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR 85
#endif

namespace ECSEngine {

	namespace containers {

		class ECSENGINE_API HashFunctionPowerOfTwo {
		public:
			HashFunctionPowerOfTwo() {}
			HashFunctionPowerOfTwo(size_t additional_info) {}
			unsigned int operator ()(unsigned int key, unsigned int capacity) const {
				return key & (capacity - 1);
			}
		};

		class HashFunctionPrimeNumber {
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

		class ECSENGINE_API HashFunctionFibonacci {
		public:
			HashFunctionFibonacci() : m_shift_amount(64) {}
			HashFunctionFibonacci(size_t additional_info) : m_shift_amount(64 - additional_info) {}
			unsigned int operator () (unsigned int key, unsigned int capacity) const {
				return (key * 11400714819323198485llu) >> m_shift_amount;
			}
		private:
			size_t m_shift_amount;
		};

		class ECSENGINE_API HashFunctionXORFibonacci {
		public:
			HashFunctionXORFibonacci() : m_shift_amount(64) {}
			HashFunctionXORFibonacci(size_t additional_info) : m_shift_amount(64 - additional_info) {}
			unsigned int operator() (unsigned int key, unsigned int capacity) const {
				key ^= key >> m_shift_amount;
				return (key * 11400714819323198485llu) >> m_shift_amount;
			}
		private:
			size_t m_shift_amount;
		};
		
		class ECSENGINE_API HashFunctionFolding {
		public:
			HashFunctionFolding() {}
			HashFunctionFolding(size_t additional_info) {}
			unsigned int operator() (unsigned int key, unsigned int capacity) const {
				return (key & 0x0000FFFF + key & 0xFFFF0000) & (capacity - 1);
			}
		};

		/* Robin Hood hash table that stores in separate buffers keys and values. Keys are unsigned integers
		* that have stored in the highest byte the robin hood distance in order to not create an additional metadata
		* buffer, not get an extra cache miss and use SIMD probing. Distance 0 means empty slot, distance 1 means that 
		* the key is where it hashed. The capacity should be set to a power of two when using power of two hash function.
		* There are 256 padding elements at the end in order to eliminate wrap around and to effectively use only SIMD
		* operations. Use MemoryOf to get the number of bytes needed for the buffers
		*/
		template<typename T, typename HashFunction>
		class HashTable {
		public:
			HashTable() : m_keys(nullptr), m_values(nullptr), m_capacity(0), m_count(0), m_maximum_probe_count(0),
				m_function(HashFunction(0)) {}
			HashTable(void* buffer, size_t capacity, size_t additional_info = 0) {
				InitializeFromBuffer(buffer, capacity, additional_info);
			}

			HashTable(const HashTable& other) = default;
			HashTable(HashTable&& other) = default;

			HashTable<T, HashFunction>& operator = (const HashTable<T, HashFunction>& other) = default;
			HashTable<T, HashFunction>& operator = (HashTable<T, HashFunction>&& other) = default;

			void Clear() {
				for (size_t index = 0; index < m_capacity; index++) {
					m_keys[index] = (unsigned int)0;
				}
				m_maximum_probe_count = 0;
				m_count = 0;
			}

			int Find(unsigned int key) const {
				// calculating the index for the array with the hash function
				unsigned int index = m_function(key, m_capacity);
				size_t i = index;

				// preparing SIMD variables and to check 8 elements at once
				Vec8ui elements, keys(key), ignore_distance(0x00FFFFFF);
				Vec8ib match;
				int flag = -1;

				for (; flag == -1 && i < index + m_maximum_probe_count; i += elements.size()) {
					elements.load(m_keys + i);
					match = (elements & ignore_distance) == keys;
					flag = horizontal_find_first(match);
				}
					
				if (flag == -1) {
					return -1;
				}
				return i + flag - elements.size();
			}

			// Use this if duplicates happen just for searching, erasing might not work for duplicates (can erase either of the duplicates)
			// It will put all the values found in the stream
			void FindAll(unsigned int key, Stream<T>& values) const {
				// calculating the index for the array with the hash function
				unsigned int index = m_function(key, m_capacity);
				size_t i = index;

				// preparing SIMD variables and to check 8 elements at once
				Vec8ui elements, keys(key), ignore_distance(0x00FFFFFF);
				Vec8ib match;
				unsigned int temp[8];
				int flag = -1;

				for (; i < index + m_maximum_probe_count; i += 8) {
					elements.load(m_keys + i);
					match = (elements & ignore_distance) == keys;
					if (horizontal_or(match)) {
						match.store(temp);
						for (size_t _index = 0; _index < 8; _index++) {
							if (temp[_index] != 0) {
								values.Add(m_values[i + _index]);
							}
						}
					}
				}
			}

			// the return value tells the caller if the hash table needs to grow or allocate another hash table
			bool Insert(unsigned int key, T value) {
				unsigned int dummy;
				return Insert(key, value, dummy);
			}

			// the return value tells the caller if the hash table needs to grow or allocate another hash table
			bool Insert(unsigned int key, T value, unsigned int& position) {
				// calculating the index at which the key wants to be
				unsigned int index = m_function(key, m_capacity);
				unsigned int hash = index;
				unsigned int distance = 1;

				// probing the next slots
				while (true) {
					// getting the element's distance
					unsigned int element_distance = (m_keys[index] & 0xFF000000) >> 24;

					// if the element is "poorer" than us, we keep probing 
					while (element_distance > distance) {
						distance++;
						index++;
						element_distance = (m_keys[index] & 0xFF000000) >> 24;
					}

					// if the slot is empty, the key can be placed here
					if (element_distance == 0) {
						key |= (distance << 24);
						m_keys[index] = key;
						m_values[index] = value;
						m_count++;
						m_maximum_probe_count = distance > m_maximum_probe_count ? distance : m_maximum_probe_count;
						position = index;
						break;
					}

					// if it finds a richer slot than us, we swap the keys and keep searching to find 
					// an empty slot for the richer slot
					else {
						unsigned int key_temp = m_keys[index];
						T value_temp = m_values[index];
						m_keys[index] = key | (distance << 24);
						m_values[index] = value;
						m_maximum_probe_count = distance > m_maximum_probe_count ? distance : m_maximum_probe_count;
						distance = ((key_temp & 0xFF000000) >> 24) + 1;
						key = key_temp & 0x00FFFFFF;
						value = value_temp;
						index++;
					}
				}
				if (m_count * 100 / m_capacity > ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR || m_maximum_probe_count == 255)
					return true;
				return false;
			}

			void Erase(unsigned int key) {
				// determining the next index and clearing the current slot
				int index = Find(key);
				EraseFromIndex(index);
			}

			void EraseFromIndex(unsigned int index) {
				// getting the distance of the next slot;
				unsigned int distance = (m_keys[index] & 0xFF000000) >> 24;

				// while we find a slot that is not empty and that is not in its ideal hash position
				// backward shift the elements
				while (distance > 1) {
					distance--;
					m_keys[index - 1] = (m_keys[index] & 0x00FFFFFF) | (distance << 24);
					m_values[index - 1] = m_values[index];
					index++;
					distance = (m_keys[index] & 0xFF000000) >> 24;
				}
				m_keys[index - 1] = (unsigned int)0x00000000;
				m_count--;
			}

			void ResetProbeCount() {
				m_maximum_probe_count = 0;
				// account for padding elements
				for (size_t index = 0; index < m_capacity + 256; index++) {
					unsigned int distance = (m_keys[index] & 0xFF000000) >> 24;
					m_maximum_probe_count = distance > m_maximum_probe_count ? distance : m_maximum_probe_count;
				}
			}

			void ResetProbeCountToZero() {
				m_maximum_probe_count = 0;
			}

			bool IsItemAt(unsigned int index) const {
				return m_keys[index] != 0;
			}

			const void* GetAllocatedBuffer() const {
				return m_values;
			}

			T GetValue(unsigned int key) const {
				int index = Find(key);
				ECS_ASSERT(index != -1);
				return m_values[index];
			}

			T GetValueFromIndex(size_t index) const {
				ECS_ASSERT(index >= 0 && index < m_capacity);
				return m_values[index];
			}

			T* GetValuePtr(unsigned int key) const {
				int index = Find(key);
				ECS_ASSERT(index != -1);
				return m_values + index;
			}

			T* GetValuePtrFromIndex(size_t index) const {
				ECS_ASSERT(index >= 0 && index < m_capacity);
				return m_values + index;
			}

			size_t GetMaximumProbeCount() const {
				return m_maximum_probe_count;
			}

			size_t GetCount() const {
				return m_count;
			}

			const unsigned int* GetKeys() const {
				return m_keys;
			}

			const T* GetValues() const {
				return m_values;
			}

			Stream<T> GetValueStream() {
				return Stream<T>(m_values, m_capacity);
			}

			void SetValue(T value, unsigned int key) {
				int index = Find(key);
				ECS_ASSERT(index != -1);

				m_values[index] = value;
			}

			void SetValue(size_t index, T value) {
				ECS_ASSERT(index >= 0 && index < m_capacity);
				m_values[index] = value;
			}

			bool TryGetValue(unsigned int key, T& value) const {
				int index = Find(key);
				if (index != -1) {
					value = m_values[index];
					return true;
				}
				else {
					return false;
				}
			}

			bool TryGetValuePtr(unsigned int key, T*& pointer) const {
				int index = Find(key);
				if (index != -1) {
					pointer = m_values + index;
					return true;
				}
				else {
					return false;
				}
			}

			static size_t MemoryOf(size_t number) {
				return (sizeof(unsigned int) + sizeof(T)) * (number + 256);
			}

			template<typename Allocator>
			void Initialize(Allocator* allocator, size_t max_element_count, size_t additional_info = 0) {
				size_t memory_size = MemoryOf(max_element_count);
				void* allocation = allocator->Allocate(max_element_count, 8);
				InitializeFromBuffer(allocation, max_element_count, additional_info);
			}

			void InitializeFromBuffer(void* buffer, size_t capacity, size_t additional_info = 0) {
				m_values = (T*)buffer;
				uintptr_t ptr = (uintptr_t)buffer;
				m_keys = (unsigned int*)(ptr + sizeof(T) * (capacity + 256));

				// account for padding elements
				for (size_t index = 0; index < capacity + 256; index++) {
					m_keys[index] = (unsigned int)0;
				}

				m_capacity = capacity;
				m_count = 0;
				m_maximum_probe_count = 0;
				m_function = HashFunction(additional_info);
			}

			void InitializeFromBuffer(uintptr_t& buffer, size_t capacity, size_t additional_info = 0) {
				InitializeFromBuffer((void*)buffer, capacity, additional_info);
				buffer += MemoryOf(capacity);
			}

		private:
			unsigned int* m_keys;
			T* m_values;
			size_t m_capacity;
			size_t m_count;
			size_t m_maximum_probe_count;
			HashFunction m_function;
		};

		/* Robin Hood hashing identifier hash table that stores in separate buffers keys and values. Keys are
		* unsigned integers that have stored in the highest byte the robin hood distance in order to not create an
		* additional metadata buffer, not get an extra cache miss and use SIMD probing. Distance 0 means empty slot,
		* distance 1 means that the key is where it hashed. The capacity should be set to a power of two when using
		* power of two hash function. There are 256 padding elements at the end in order to eliminate wrap around and
		* to effectively use only SIMD operations. Use MemoryOf to get the number of bytes needed for the buffers.
		* It does a identifier check when trying to retrieve the value. Identifier type must implement Compare
		* function because it supports checking for equality with a user defined operation other than assignment operator
		* e. g. for pointers, they might be equal if the data they point to is the same
		* Identifier must be stable, this table only keeps the pointer not the data pointed to
		*/
		template<typename T, typename Identifier, typename HashFunction>
		class IdentifierHashTable {
		public:
			IdentifierHashTable() : m_keys(nullptr), m_values(nullptr), m_identifiers(nullptr), m_capacity(0), m_count(0), m_maximum_probe_count(0),
				m_function(HashFunction(0)) {}
			IdentifierHashTable(void* buffer, size_t capacity, size_t additional_info = 0) {
				InitializeFromBuffer(buffer, capacity, additional_info);
			}

			IdentifierHashTable(const IdentifierHashTable& other) = default;
			IdentifierHashTable(IdentifierHashTable&& other) = default;

			IdentifierHashTable<T, Identifier, HashFunction>& operator = (const IdentifierHashTable<T, Identifier, HashFunction>& other) = default;
			IdentifierHashTable<T, Identifier, HashFunction>& operator = (IdentifierHashTable<T, Identifier, HashFunction>&& other) = default;

			void Clear() {
				for (size_t index = 0; index < m_capacity; index++) {
					m_keys[index] = (unsigned int)0;
				}
				m_count = 0;
				m_maximum_probe_count = 0;
			}

			unsigned int Find(unsigned int key) const {
				// calculating the index for the array with the hash function
				unsigned int index = m_function(key, m_capacity);
				size_t i = index;

				// preparing SIMD variables and to check 8 elements at once
				Vec8ui elements, keys(key), ignore_distance(0x00FFFFFF);
				Vec8ib match;
				int flag = -1;

				for (; flag == -1 && i < index + m_maximum_probe_count; i += elements.size()) {
					elements.load(m_keys + i);
					match = (elements & ignore_distance) == keys;
					flag = horizontal_find_first(match);
				}

				if (flag == -1) {
					return -1;
				}
				return i + flag - elements.size();
			}

			template<bool use_compare_function = true>
			unsigned int Find(unsigned int key, Identifier identifier) const {
				// calculating the index for the array with the hash function
				unsigned int index = m_function(key, m_capacity);
				size_t i = index;

				// preparing SIMD variables and to check 8 elements at once
				Vec8ui elements, keys(key), ignore_distance(0x00FFFFFF);
				Vec8ib match;
				unsigned int temp[8];
				int flag = -1;

				for (; flag == -1 && i < index + m_maximum_probe_count; i += 8) {
					elements.load(m_keys + i);
					match = (elements & ignore_distance) == keys;
					if (horizontal_or(match)) {
						match.store(temp);
						for (size_t _index = 0; _index < 8; _index++) {
							if constexpr (!use_compare_function) {
								if (temp[_index] != 0 && m_identifiers[i + _index] == identifier) {
									return i + _index;
								}
							}
							else {
								if (temp[_index] != 0 && m_identifiers[i + _index].Compare(identifier)) {
									return i + _index;
								}
							}
						}
					}
				}

				return -1;
			}

			// the return value tells the caller if the hash table needs to grow or allocate another hash table
			bool Insert(unsigned int key, T value, Identifier identifier) {
				unsigned int dummy;
				return Insert(key, value, identifier, dummy);
			}

			// the return value tells the caller if the hash table needs to grow or allocate another hash table
			bool Insert(unsigned int key, T value, Identifier identifier, unsigned int& position) {
				// calculating the index at which the key wants to be
				unsigned int index = m_function(key, m_capacity);
				unsigned int hash = index;
				unsigned int distance = 1;

				// probing the next slots
				while (true) {
					// getting the element's distance
					unsigned int element_distance = (m_keys[index] & 0xFF000000) >> 24;

					// if the element is "poorer" than us, we keep probing 
					while (element_distance > distance) {
						distance++;
						index++;
						element_distance = (m_keys[index] & 0xFF000000) >> 24;
					}

					// if the slot is empty, the key can be placed here
					if (element_distance == 0) {
						key |= (distance << 24);
						m_keys[index] = key;
						m_values[index] = value;
						m_identifiers[index] = identifier;
						m_count++;
						m_maximum_probe_count = distance > m_maximum_probe_count ? distance : m_maximum_probe_count;
						position = index;
						break;
					}

					// if it finds a richer slot than us, we swap the keys and keep searching to find 
					// an empty slot for the richer slot
					else {
						unsigned int key_temp = m_keys[index];
						T value_temp = m_values[index];
						Identifier identifier_temp = m_identifiers[index];
						m_keys[index] = key | (distance << 24);
						m_values[index] = value;
						m_identifiers[index] = identifier;
						m_maximum_probe_count = distance > m_maximum_probe_count ? distance : m_maximum_probe_count;
						distance = ((key_temp & 0xFF000000) >> 24) + 1;
						key = key_temp & 0x00FFFFFF;
						value = value_temp;
						identifier = identifier_temp;
						index++;
					}
				}
				if (m_count * 100 / m_capacity > ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR || m_maximum_probe_count == 255)
					return true;
				return false;
			}

			template<bool use_compare_function = true>
			void Erase(unsigned int key, Identifier identifier) {
				// determining the next index and clearing the current slot
				int index = Find<use_compare_function>(key, identifier);
				EraseFromIndex(index);
			}

			void EraseFromIndex(unsigned int index) {
				// getting the distance of the next slot;
				index++;
				unsigned int distance = (m_keys[index] & 0xFF000000) >> 24;

				// while we find a slot that is not empty and that is not in its ideal hash position
				// backward shift the elements
				while (distance > 1) {
					distance--;
					m_keys[index - 1] = (m_keys[index] & 0x00FFFFFF) | (distance << 24);
					m_values[index - 1] = m_values[index];
					m_identifiers[index - 1] = m_identifiers[index];
					index++;
					distance = (m_keys[index] & 0xFF000000) >> 24;
				}
				m_keys[index - 1] = (unsigned int)0x00000000;
				m_count--;
			}

			void ResetProbeCount() {
				m_maximum_probe_count = 0;
				// account for padding elements
				for (size_t index = 0; index < m_capacity + 256; index++) {
					unsigned int distance = (m_keys[index] & 0xFF000000) >> 24;
					m_maximum_probe_count = distance > m_maximum_probe_count ? distance : m_maximum_probe_count;
				}
			}

			void ResetProbeCountToZero() {
				m_maximum_probe_count = 0;
			}

			bool IsItemAt(unsigned int index) const {
				return m_keys[index] != 0;
			}

			const void* GetAllocatedBuffer() const {
				return m_values;
			}

			template<bool use_compare_function = true> 
			T GetValue(unsigned int key_or_index, Identifier identifier) const {
				int index = Find<use_compare_function>(key_or_index, identifier);
				ECS_ASSERT(index != -1);
				return m_values[index];
			}

			template<bool use_compare_function = true>
			T* GetValuePtr(unsigned int key, Identifier identifier) const {
				int index = Find<use_compare_function>(key, identifier);
				ECS_ASSERT(index != -1);
				return m_values + index;
			}

			T GetValueFromIndex(size_t index) const {
				ECS_ASSERT(index >= 0 && index < m_capacity);
				return m_values[index];
			}

			T* GetValuePtrFromIndex(size_t index) const {
				ECS_ASSERT(index >= 0 && index < m_capacity);
				return m_values + index;
			}


			size_t GetMaximumProbeCount() const {
				return m_maximum_probe_count;
			}

			size_t GetCount() const {
				return m_count;
			}

			size_t GetCapacity() const {
				return m_capacity;
			}

			unsigned int* GetKeys() {
				return m_keys;
			}

			const unsigned int* GetConstKeys() const {
				return m_keys;
			}

			T* GetValues() {
				return m_values;
			}

			const T* GetValues() const {
				return m_values;
			}

			Stream<T> GetValueStream() {
				return Stream<T>(m_values, m_capacity);
			}

			const Identifier* GetIdentifiers() const {
				return m_identifiers;
			}

			template<bool use_compare_function = false>
			void SetValue(T value, unsigned int key, Identifier identifier) {
				int index = Find<use_compare_function>(key, identifier);
				ECS_ASSERT(index != -1);

				m_values[index] = value;
			}

			void SetValue(size_t index, T value) {
				ECS_ASSERT(index >= 0 && index < m_capacity);
				m_values[index] = value;
			}

			template<bool use_compare_function = true>
			bool TryGetValue(unsigned int key_or_index, Identifier identifier, T& value) const {
				int index = Find<use_compare_function>(key_or_index, identifier);
				if (index != -1) {
					value = m_values[index];
					return true;
				}
				else {
					return false;
				}
			}

			template<bool use_compare_function = true>
			bool TryGetValuePtr(unsigned int key, Identifier identifier, T*& pointer) const {
				int index = Find<use_compare_function>(key, identifier);
				if (index != -1) {
					pointer = m_values + index;
					return true;
				}
				else {
					return false;
				}
			}

			static size_t MemoryOf(size_t number) {
				return (sizeof(unsigned int) + sizeof(T) + sizeof(Identifier)) * (number + 256);
			}

			void InitializeFromBuffer(void* buffer, size_t capacity, size_t additional_info = 0) {
				m_values = (T*)buffer;
				uintptr_t ptr = (uintptr_t)buffer;
				ptr += sizeof(T) * (capacity + 256);
				m_identifiers = (Identifier*)ptr;
				ptr += sizeof(Identifier) * (capacity + 256);
				m_keys = (unsigned int*)ptr;

				// make distance 0 for keys, account for padding elements
				for (size_t index = 0; index < capacity + 256; index++) {
					m_keys[index] = (unsigned int)0;
				}

				m_capacity = capacity;
				m_count = 0;
				m_maximum_probe_count = 0;
				m_function = HashFunction(additional_info);
			}

			void InitializeFromBuffer(uintptr_t& buffer, size_t capacity, size_t additional_info = 0) {
				InitializeFromBuffer((void*)buffer, capacity, additional_info);
				buffer += MemoryOf(capacity);
			}

			template<typename Allocator>
			void Initialize(Allocator* allocator, size_t capacity, size_t additional_info = 0) {
				size_t memory_size = MemoryOf(capacity);
				void* allocation = allocator->Allocate(memory_size, 8);
				InitializeFromBuffer(allocation, capacity, additional_info);
			}

			// The buffer given must be allocated with MemoryOf
			template<typename IdentifierHashFunction>
			const void* Grow(void* buffer, size_t new_capacity) {
				const unsigned int* old_keys = m_keys;
				const T* old_values = m_values;
				const Identifier* old_identifiers = m_identifiers;
				const HashFunction old_hash = m_function;
				size_t old_capacity = m_capacity;

				InitializeFromBuffer(buffer, new_capacity);
				m_function = old_hash;

				for (size_t index = 0; index < old_capacity; index++) {
					bool is_item = old_keys[index] != 0;
					if (is_item) {
						unsigned int hash = IdentifierHashFunction::Hash(old_identifiers[index]);
						hash = m_function(hash, m_capacity);
						Insert(hash, old_values[index], old_identifiers[index]);
					}
				}

				return old_values;
			}

		private:
			unsigned int* m_keys;
			T* m_values;
			Identifier* m_identifiers;
			size_t m_capacity;
			size_t m_count;
			size_t m_maximum_probe_count;
			HashFunction m_function;
		};

		// Helper function that eases the process of dynamic identifier hash tables; it takes care of allocating and
		// deallocating when necessary
		template<typename HashFunction, typename Table, typename Allocator, typename Value>
		void InsertToDynamicTable(Table& table, Allocator* allocator, Value value, Stream<void> identifier) {
			unsigned int hash = HashFunction::Hash(identifier);

			if (table.Insert(hash, value, identifier)) {
				size_t new_capacity = (size_t)((float)table.GetCapacity() * 1.5f + 1);
				void* new_allocation = allocator->Allocate(table.MemoryOf(new_capacity));
				const void* old_allocation = table.Grow<HashFunction>(new_allocation, new_capacity);
				allocator->Deallocate(old_allocation);
			}
		}

	}

}