#pragma once
#include "../Core.h"
#include "../Math/VCLExtensions.h"
#include "Hashing.h"
#include "../Utilities/Iterator.h"
#include "../Utilities/Utilities.h"

#ifndef ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR
#define ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR 95
#endif

namespace ECSEngine {

	template<typename Value, typename Identifier>
	struct TablePair {
		Value value;
		Identifier identifier;
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
		static_assert(!std::is_same_v<T, HashTableEmptyValue> || SoA, "Look up table without any value must be SoA!");
		static_assert(sizeof(TableHashFunction) < alignof(void*), "Hash table hash function can have at max the native max alignment as byte size!");

		typedef T Value;
		typedef Identifier Identifier;
		typedef std::conditional_t<sizeof(Identifier) <= sizeof(void*), Identifier, const Identifier&> IdentifierParameter;
		typedef std::conditional_t<sizeof(Value) <= sizeof(void*), Value, const Value&> ValueParameter;

		struct Pair {
			T value;
			Identifier identifier;
		};

		struct PairPtr {
			T* value;
			Identifier* identifier;
		};

		struct ConstPairPtr {
			const T* value;
			const Identifier* identifier;
		};

#define ECS_HASH_TABLE_DISTANCE_MASK 0xF8
#define ECS_HASH_TABLE_HASH_BITS_MASK 0x07
#define ECS_HASH_TABLE_PADDING_ELEMENT_COUNT 31

		HashTable() : m_metadata(nullptr), m_buffer(nullptr), m_identifiers(nullptr), m_capacity(0), m_count(0), m_function(TableHashFunction(0)) {}
		HashTable(void* buffer, unsigned int capacity, size_t additional_info = 0) {
			InitializeFromBuffer(buffer, capacity, additional_info);
		}

		HashTable(const HashTable& other) = default;
		HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>& operator = (const HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>& other) = default;

	private:
		template<typename = std::enable_if_t<SoA == true>>
		ECS_INLINE static size_t GetValueSize() {
			size_t value = sizeof(T);
			if constexpr (std::is_same_v<T, HashTableEmptyValue>) {
				value = 0;
			}
			return value;
		}

	public:

		void Clear() {
			if (m_capacity > 0) {
				memset(m_metadata, 0, sizeof(unsigned char) * (m_capacity + ECS_HASH_TABLE_PADDING_ELEMENT_COUNT));
				m_count = 0;
			}
		}

		unsigned int Find(IdentifierParameter identifier) const {
			if (GetCount() == 0) {
				return -1;
			}

			unsigned int key = ObjectHashFunction::Hash(identifier);

			// calculating the index for the array with the hash function
			unsigned int index = m_function(key, m_capacity);

			unsigned char key_hash_bits = key;
			key_hash_bits &= ECS_HASH_TABLE_HASH_BITS_MASK;

			Vec32cb match = HashTableFindSIMDKernel(index, m_metadata, key_hash_bits, ECS_HASH_TABLE_HASH_BITS_MASK);

			unsigned int return_value = -1;
			ForEachBit(match, [&](unsigned int bit_index) {
				Identifier current_identifier = GetIdentifierFromIndex(index + bit_index);

				if (current_identifier == identifier) {
					return_value = index + bit_index;
					return true;
				}

				return false;
			});

			return return_value;
		}

		// It will be called with a single argument - the index of the element
		// For the early exit it must return true when to exit, else false.
		// For non early exit it must return true if the current element is being deleted (you must do this!), else false.
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachIndex(Functor&& functor) {
			unsigned int extended_capacity = GetExtendedCapacity();
			for (unsigned int index = 0; index < extended_capacity; index++) {
				if (IsItemAt(index)) {
					if constexpr (early_exit) {
						if (functor(index)) {
							return true;
						}
					}
					else {
						index -= functor(index);
					}
				}
			}
			return false;
		}

		// For the early exit it must return true when to exit, else false.
		// Else void return
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachIndexConst(Functor&& functor) const {
			unsigned int extended_capacity = GetExtendedCapacity();
			for (unsigned int index = 0; index < extended_capacity; index++) {
				if (IsItemAt(index)) {
					if constexpr (early_exit) {
						if (functor(index)) {
							return true;
						}
					}
					else {
						functor(index);
					}
				}
			}
			return false;
		}

		// For the early exit it must return true when to exit, else false.
		// Else void return.
		// First parameter - the value, the second one - the identifier
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) {
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
								return true;
							}
						}
						else {
							functor(values[index], identifiers[index]);
						}
					}
					else {
						if constexpr (early_exit) {
							if (functor(pairs[index].value, pairs[index].identifier)) {
								return true;
							}
						}
						else {
							functor(pairs[index].value, pairs[index].identifier);
						}
					}
				}
			}
			return false;
		}

		// Const variant
		// For the early exit it must return true when to exit, else false.
		// Else void return.
		// First parameter - the value, the second one - the identifier
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachConst(Functor&& functor) const {
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
								return true;
							}
						}
						else {
							functor(values[index], identifiers[index]);
						}
					}
					else {
						if constexpr (early_exit) {
							if (functor(pairs[index].value, pairs[index].identifier)) {
								return true;
							}
						}
						else {
							functor(pairs[index].value, pairs[index].identifier);
						}
					}
				}
			}
			return false;
		}

		private:
			// Returns true if a resize needs to take place, else false.
			// If the crash if_not_space is set to true, it will crash if there is not
			// Enough space for the entry. The parameters are on purpose passed by value, since that's how they are needed
			template<bool crash_if_not_space>
			bool InsertImpl(T value, Identifier identifier, unsigned int& position) {
				if constexpr (crash_if_not_space) {
					ECS_ASSERT(m_count * 100 / m_capacity <= ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR, "Hash table capacity exceeded");
				}

				unsigned int key = ObjectHashFunction::Hash(identifier);
				// Signal that the position has not yet been determined
				position = -1;

				// calculating the index at which the key wants to be
				unsigned int index = m_function(key, m_capacity);
				unsigned int hash = index;
				unsigned int distance = 1;
				unsigned int max_distance = 1;

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
						if constexpr (crash_if_not_space) {
							ECS_ASSERT(distance <= ECS_HASH_TABLE_PADDING_ELEMENT_COUNT, "Hash table capacity not enough (too many collisions)");
						}
						else {
							max_distance = max(max_distance, distance);
							// Clamp the distance to ECS_HASH_TABLE_PADDING_ELEMENT_COUNT in case it overflows to 32, and that can make the
							// Metadata empty and lead to other inserts or erases thinking this slot is empty
							// When it is not
							distance = min(distance, (unsigned int)ECS_HASH_TABLE_PADDING_ELEMENT_COUNT);
						}
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

						if constexpr (crash_if_not_space) {
							ECS_ASSERT(distance <= ECS_HASH_TABLE_PADDING_ELEMENT_COUNT, "Hash table capacity not enough (too many collisions)");
						}
						else {
							max_distance = max(max_distance, distance);
							// Clamp the distance to ECS_HASH_TABLE_PADDING_ELEMENT_COUNT in case it overflows to 32, and that can make the
							// Metadata empty and lead to other inserts or erases thinking this slot is empty
							// When it is not
							distance = min(distance, (unsigned int)ECS_HASH_TABLE_PADDING_ELEMENT_COUNT);
						}
						m_metadata[index] = hash_key_bits | (distance << 3);
						distance = (metadata_temp >> 3) + 1;

						hash_key_bits = metadata_temp & ECS_HASH_TABLE_HASH_BITS_MASK;
						position = position == -1 ? index : position;
						index++;
					}
				}

				if constexpr (crash_if_not_space) {
					return false;
				}
				else {
					if (max_distance >= ECS_HASH_TABLE_PADDING_ELEMENT_COUNT || (m_count * 100 / m_capacity > ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR)) {
						return true;
					}
					return false;
				}
			}

		public:

		// It will assert if there is not enough space
		void Insert(ValueParameter value, IdentifierParameter identifier, unsigned int& position) {
			InsertImpl<true>(value, identifier, position);
		}

		// It will assert if there is not enough space
		void Insert(ValueParameter value, IdentifierParameter identifier) {
			unsigned int dummy;
			Insert(value, identifier, dummy);
		}

		template<typename Allocator>
		bool InsertDynamic(Allocator* allocator, ValueParameter value, IdentifierParameter identifier, DebugInfo debug_info = ECS_DEBUG_INFO) {
			auto grow = [&]() {
				unsigned int old_capacity = GetCapacity();

				unsigned int new_capacity = NextCapacity(GetCapacity());
				void* new_allocation = allocator->Allocate(MemoryOf(new_capacity), alignof(void*), debug_info);
				void* old_allocation = Resize(new_allocation, new_capacity);
				if (old_capacity > 0) {
					allocator->Deallocate(old_allocation, debug_info);
				}
			};

			unsigned int initial_capacity = GetCapacity();
			if (GetCapacity() == 0) {
				grow();
			}
			unsigned int insert_location;
			if (InsertImpl<false>(value, identifier, insert_location)) {
				grow();
			}
			return initial_capacity != GetCapacity();
		}

		// It will grow the hash table to accomodate the new entry
		// Returns true if it grew, else false
		bool InsertDynamic(AllocatorPolymorphic allocator, ValueParameter value, IdentifierParameter identifier, DebugInfo debug_info = ECS_DEBUG_INFO) {
			auto grow = [&]() {
				unsigned int old_capacity = GetCapacity();

				unsigned int new_capacity = NextCapacity(GetCapacity());
				void* new_allocation = AllocateEx(allocator, MemoryOf(new_capacity), alignof(void*), debug_info);
				void* old_allocation = Resize(new_allocation, new_capacity);
				if (old_capacity > 0) {
					DeallocateEx(allocator, old_allocation, debug_info);
				}
			};

			unsigned int initial_capacity = GetCapacity();
			if (GetCapacity() == 0) {
				grow();
			}
			unsigned int insert_location;
			if (InsertImpl<false>(value, identifier, insert_location)) {
				grow();
			}
			return initial_capacity != GetCapacity();
		}

		void Erase(IdentifierParameter identifier) {
			// determining the next index and clearing the current slot
			int index = Find(identifier);
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

		// For a given index, it will return the index of the first entry that
		// Is at count elements away from the current element, which may or may not hold an actual value,
		// (it starts verifying from the given index, so if there is an element at the given index and count is 1,
		// It will return the index value + 1). It returns -1 if there are not count valid entries starting from the
		// given index
		unsigned int SkipElements(unsigned int index, unsigned int count) const {
			// If out of bounds, early exit
			if (index >= GetExtendedCapacity() || count == 0) {
				return -1;
			}

			// We can use SIMD for this, since the metadata is sequential
			// And a comparison with 0 suffices
			Vec32uc simd_zero = ZeroVectorInteger();
			while (index < GetExtendedCapacity()) {
				Vec32uc current_metadata;
				current_metadata.load(m_metadata + index);
				Vec32cb is_valid_mask = current_metadata != simd_zero;
				unsigned int valid_mask_int = to_bits(is_valid_mask);
				unsigned int valid_entry_count = CountBits(valid_mask_int);
				if (valid_entry_count >= count) {
					// Iterate over the bits until count is exhausted
					unsigned int bit = 1;
					while (count > 0)
					{
						if ((valid_mask_int & bit) != 0) {
							count--;
						}
						index++;
					}

					return index >= GetExtendedCapacity() ? -1 : index;
				}

				count -= valid_entry_count;
				index += simd_zero.size();
			}

			// It can exit with a valid entry only inside the while
			return -1;
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
			return m_capacity == 0 ? 0 : m_capacity + ECS_HASH_TABLE_PADDING_ELEMENT_COUNT;
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
			if constexpr (std::is_same_v<T, HashTableEmptyValue>) {
				return nullptr;
			}
			return (T*)m_buffer;
		}

		// Only valid in SoA type
		template<typename = std::enable_if_t<SoA == true>>
		ECS_INLINE const T* GetValues() const {
			if constexpr (std::is_same_v<T, HashTableEmptyValue>) {
				return nullptr;
			}
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
				if constexpr (std::is_same_v<T, HashTableEmptyValue>) {
					return {};
				}
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
				if constexpr (std::is_same_v<T, HashTableEmptyValue>) {
					return nullptr;
				}
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

		template<typename = std::enable_if_t<!std::is_same_v<T, HashTableEmptyValue>>>
		T GetValue(IdentifierParameter identifier) const {
			unsigned int index = Find(identifier);
			ECS_ASSERT(index != -1);
			return GetValueFromIndex(index);
		}

		template<typename = std::enable_if_t<!std::is_same_v<T, HashTableEmptyValue>>>
		T* GetValuePtr(IdentifierParameter identifier) {
			unsigned int index = Find(identifier);
			ECS_ASSERT(index != -1);
			return GetValuePtrFromIndex(index);
		}

		template<typename = std::enable_if_t<!std::is_same_v<T, HashTableEmptyValue>>>
		const T* GetValuePtr(IdentifierParameter identifier) const {
			unsigned int index = Find(identifier);
			ECS_ASSERT(index != -1);
			return GetValuePtrFromIndex(index);
		}

		void GetElementIndices(CapacityStream<unsigned int>& indices) const {
			for (unsigned int index = 0; index < GetExtendedCapacity(); index++) {
				if (IsItemAt(index)) {
					indices.AddAssert(index);
				}
			}
		}

		// If you have a pointer to a value from the table and want to get its index,
		// Use this function
		template<typename = std::enable_if_t<!std::is_same_v<T, HashTableEmptyValue>>>
		ECS_INLINE unsigned int ValuePtrIndex(const T* ptr) const {
			if constexpr (SoA) {
				return ptr - GetValues();
			}
			else {
				const Pair* pairs = GetPairs();
				return PointerDifference(ptr, pairs) / sizeof(Pair);
			}
		}

		// If you have a pointer to an identifier from the table and want to get its index,
		// Use this function
		ECS_INLINE unsigned int IdentifierPtrIndex(const Identifier* identifier) const {
			if constexpr (SoA) {
				return identifier - GetIdentifiers();
			}
			else {
				const Pair* pairs = GetPairs();
				return PointerDifference(OffsetPointer(identifier, -sizeof(T)), pairs) / sizeof(Pair);
			}
		}

		ECS_INLINE void GetEntry(unsigned int index, T& value, Identifier& identifier) const {
			value = GetValueFromIndex(index);
			identifier = GetIdentifierFromIndex(index);
		}

		ECS_INLINE void SetEntry(unsigned int index, ValueParameter value, IdentifierParameter identifier) {
			if constexpr (!std::is_same_v<T, HashTableEmptyValue>) {
				*GetValuePtrFromIndex(index) = value;
			}
			*GetIdentifierPtrFromIndex(index) = identifier;
		}

		template<typename = std::enable_if_t<!std::is_same_v<T, HashTableEmptyValue>>>
		void SetValue(ValueParameter value, IdentifierParameter identifier) {
			int index = Find(identifier);
			ECS_ASSERT(index != -1);
			SetValue(index, value);
		}

		// The extended capacity is used to check bounds validity
		template<typename = std::enable_if_t<!std::is_same_v<T, HashTableEmptyValue>>>
		ECS_INLINE void SetValueByIndex(unsigned int index, ValueParameter value) {
			*GetValuePtrFromIndex(index) = value;
		}

		template<typename = std::enable_if_t<!std::is_same_v<T, HashTableEmptyValue>>>
		bool TryGetValue(IdentifierParameter identifier, T& value) const {
			if (GetCount() == 0) {
				return false;
			}

			unsigned int index = Find(identifier);
			if (index != -1) {
				value = GetValueFromIndex(index);
				return true;
			}
			else {
				return false;
			}
		}

		// Returns nullptr if it doesn't find it
		template<typename = std::enable_if_t<!std::is_same_v<T, HashTableEmptyValue>>>
		T* TryGetValuePtr(IdentifierParameter identifier) {
			if (GetCount() == 0) {
				return nullptr;
			}

			unsigned int index = Find(identifier);
			if (index != -1) {
				return GetValuePtrFromIndex(index);
			}
			return nullptr;
		}

		// This is provided for some edge cases when you use templates and you know
		// That the target type is fine and just wanna retrieve the pointer
		//template<typename = std::enable_if_t<!std::is_same_v<T, HashTableEmptyValue>>>
		//bool TryGetValuePtrUntyped(IdentifierParameter identifier, void*& pointer) {
		//	T* ptr = nullptr;
		//	if (TryGetValuePtr(identifier, ptr)) {
		//		pointer = (void*)ptr;
		//		return true;
		//	}
		//	return false;
		//}
		
		// Returns nullptr if it doesn't find it
		template<typename = std::enable_if_t<!std::is_same_v<T, HashTableEmptyValue>>>
		const T* TryGetValuePtr(IdentifierParameter identifier) const {
			if (GetCount() == 0) {
				return nullptr;
			}

			unsigned int index = Find(identifier);
			if (index != -1) {
				return GetValuePtrFromIndex(index);
			}
			return nullptr;
		}

		// This is provided for some edge cases when you use templates and you know
		// That the target type is fine and just wanna retrieve the pointer
		//template<typename = std::enable_if_t<!std::is_same_v<T, HashTableEmptyValue>>>
		//bool TryGetValuePtrUntyped(IdentifierParameter identifier, const void*& pointer) const {
		//	const T* ptr = nullptr;
		//	if (TryGetValuePtr(identifier, ptr)) {
		//		pointer = (const void*)ptr;
		//		return true;
		//	}
		//	return false;
		//}

		ECS_INLINE static size_t MemoryOf(unsigned int number) {
			if (number == 0) {
				return 0;
			}

			if constexpr (SoA) {
				return (sizeof(unsigned char) + GetValueSize() + sizeof(Identifier)) * (number + ECS_HASH_TABLE_PADDING_ELEMENT_COUNT);
			}
			else {
				return (sizeof(unsigned char) + sizeof(Pair)) * (number + ECS_HASH_TABLE_PADDING_ELEMENT_COUNT);
			}
		}

		// Used by insert dynamic to determine which is the next suitable capacity 
		// for the container to grow at
		ECS_INLINE static unsigned int NextCapacity(unsigned int capacity) {
			return TableHashFunction::Next(capacity);
		}

		void CopyDistanceMetadata(const unsigned char* metadata) {
			unsigned int extended_capacity = GetExtendedCapacity();
			memcpy(m_metadata, metadata, sizeof(unsigned char) * extended_capacity);
		}

		void CopyDistanceMetadata(const HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>* table) {
			CopyDistanceMetadata(table->m_metadata);
		}

		// Equivalent to memcpy'ing the data from the other table
		void Copy(AllocatorPolymorphic allocator, const HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>* table, DebugInfo debug_info = ECS_DEBUG_INFO) {
			size_t table_size = MemoryOf(table->GetCapacity());
			void* allocation = AllocateEx(allocator, table_size, alignof(void*), debug_info);

			InitializeFromBuffer(allocation, table->GetCapacity(), 0);
			m_function = table->m_function;

			// Now blit the data - it can just be memcpy'ed from the other
			memcpy(allocation, table->GetAllocatedBuffer(), table_size);
			m_count = table->m_count;
		}

		// Deallocates the pointer if it is not nullptr and the capacity is
		// bigger than 0
		void Deallocate(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) const {
			if (GetAllocatedBuffer() != nullptr && GetCapacity() > 0) {
				ECSEngine::Deallocate(allocator, GetAllocatedBuffer(), debug_info);
			}
		}

		// It will set the internal hash buffers accordingly to the new hash buffer. It does not modify anything
		// This function is used mostly for serialization, deserialization purposes
		void SetBuffers(void* buffer, unsigned int capacity) {
			unsigned int extended_capacity = capacity + ECS_HASH_TABLE_PADDING_ELEMENT_COUNT;

			uintptr_t ptr = (uintptr_t)buffer;
			m_buffer = (decltype(m_buffer))buffer;
			if constexpr (SoA) {
				ptr += GetValueSize() * extended_capacity;
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

		// Buffers will be nullptr, count and capacity 0
		void Reset() {
			m_buffer = nullptr;
			m_metadata = nullptr;
			m_identifiers = nullptr;
			m_count = 0;
			m_capacity = 0;
		}

		bool CanTrim() const {
			// If the next capacity for the current size is different from the current capacity,
			// It means that we can trim the table
			unsigned int next_capacity = NextCapacity(GetCount());
			return next_capacity != GetCapacity();
		}

		void Trim(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
			unsigned int new_capacity = NextCapacity(GetCount());
			if (new_capacity < GetCapacity()) {
				// We can't use reallocate since it interferes with the process of adding the new values
				void* new_allocation = AllocateEx(allocator, MemoryOf(new_capacity), alignof(void*), debug_info);
				void* old_allocation = Resize(new_allocation, new_capacity);
				DeallocateEx(allocator, old_allocation, debug_info);
			}
		}

		void InitializeFromBuffer(void* buffer, unsigned int capacity, size_t additional_info = 0) {
			unsigned int extended_capacity = capacity + ECS_HASH_TABLE_PADDING_ELEMENT_COUNT;
			SetBuffers(buffer, capacity);

			if (capacity > 0 && buffer != nullptr) {
				// make distance 0 for keys, account for padding elements
				memset(m_metadata, 0, sizeof(unsigned char) * extended_capacity);
			}
			m_function = TableHashFunction(additional_info);
		}

		void InitializeFromBuffer(uintptr_t& buffer, unsigned int capacity, size_t additional_info = 0) {
			InitializeFromBuffer((void*)buffer, capacity, additional_info);
			buffer += MemoryOf(capacity);
		}

		template<typename Allocator>
		void Initialize(Allocator* allocator, unsigned int capacity, size_t additional_info = 0, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (capacity > 0) {
				size_t memory_size = MemoryOf(capacity);
				void* allocation = allocator->Allocate(memory_size, alignof(void*), debug_info);
				InitializeFromBuffer(allocation, capacity, additional_info);
			}
			else {
				InitializeFromBuffer(nullptr, 0, additional_info);
			}
		}

		void Initialize(AllocatorPolymorphic allocator, unsigned int capacity, size_t additional_info = 0, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (capacity > 0) {
				size_t memory_size = MemoryOf(capacity);
				void* allocation = Allocate(allocator, memory_size, alignof(void*), debug_info);
				InitializeFromBuffer(allocation, capacity, additional_info);
			}
			else {
				InitializeFromBuffer(nullptr, 0, additional_info);
			}
		}

		// This function is special. It will initialize a hash table that
		// Has as padding elements at the end size - 1 entries (the metadata slots are still
		// Allocated, but the space for the types and identifiers themselves is not allocated)
		// Thus helping to reduce the memory consumption for small tables that have a known
		// Element count that will not change. This function should not be used in other
		// Contexts besides the one described!
		void InitializeSmallFixed(
			AllocatorPolymorphic allocator, 
			unsigned int size,
			size_t additional_info = 0, 
			DebugInfo debug_info = ECS_DEBUG_INFO
		) {
			if (size > 0) {
				unsigned int capacity = NextCapacity(((float)size * 100 / ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR) + 1);
				// Clamp to the maximum number of padding elements
				unsigned int padding_elements = size - 1 > ECS_HASH_TABLE_PADDING_ELEMENT_COUNT ? ECS_HASH_TABLE_PADDING_ELEMENT_COUNT : size - 1;
				unsigned int extended_capacity = capacity + padding_elements;

				size_t metadata_size = sizeof(*m_metadata) * (capacity + ECS_HASH_TABLE_PADDING_ELEMENT_COUNT);
				size_t total_size = metadata_size;
				if constexpr (SoA) {
					total_size += (GetValueSize() + sizeof(Identifier)) * extended_capacity;
				}
				else {
					total_size += sizeof(Pair) * extended_capacity;
				}
				void* allocation = Allocate(allocator, total_size);

				m_buffer = (decltype(m_buffer))allocation;
				if (SoA) {
					m_identifiers = (Identifier*)OffsetPointer(allocation, sizeof(T) * extended_capacity);
				}
				m_metadata = (unsigned char*)OffsetPointer(allocation, total_size - metadata_size);
				memset(m_metadata, 0, sizeof(*m_metadata) * metadata_size);

				m_count = 0;
				m_capacity = capacity;
				m_function = TableHashFunction(additional_info);
			}
			else {
				InitializeFromBuffer(nullptr, 0, additional_info);
			}
		}

		// The buffer given must be allocated with MemoryOf
		void* Resize(void* buffer, unsigned int new_capacity) {
			const unsigned char* old_metadata = m_metadata;
			void* old_buffer = m_buffer;
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

		// Value type must be T or const T
		template<typename HashTableType, typename ValueType>
		struct ValueIterator : IteratorInterface<ValueType> {
			ECS_INLINE ValueIterator(HashTableType* table) : table(*table), index(0), IteratorInterface<ValueType>(table->GetCount()) {}
			
			ValueType* Get() override {
				ValueType* value = nullptr;
				while (!table.IsItemAt(index)) {
					index++;
				}
				value = table.GetValuePtrFromIndex(index);
				index++;
				return value;
			}

			bool IsContiguous() const override {
				return false;
			}

			IteratorInterface<ValueType>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) override {
				ValueIterator<HashTableType, ValueType>* iterator = (ValueIterator<HashTableType, ValueType>*)AllocateEx(allocator, sizeof(ValueIterator<HashTableType, ValueType>));
				*iterator = ValueIterator<HashTableType, ValueType>(&table);
				iterator->index = index;
				index = table.SkipElements(index, count);
				ECS_ASSERT(index != -1, "Creating hash table sub iterator failed! The given subrange is too large.");
				return iterator;
			}

			HashTableType table;
			unsigned int index;
		};

		// Identifier type must be Identifier or const Identifier
		template<typename HashTableType, typename IdentifierType>
		struct IdentifierIterator : IteratorInterface<IdentifierType> {
			ECS_INLINE IdentifierIterator(HashTableType* table) : table(*table), remaining_count(table->GetCount()), index(0) {}

			IdentifierType* Get() override {
				IdentifierType* identifier = nullptr;
				while (!table.IsItemAt(index)) {
					index++;
				}
				identifier = table.GetIdentifierPtrFromIndex(index);
				index++;
				return identifier;
			}

			bool IsContiguous() const override {
				return false;
			}

			IteratorInterface<IdentifierType>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) override {
				IdentifierIterator<HashTableType, IdentifierType>* iterator = (IdentifierIterator<HashTableType, IdentifierType>*)AllocateEx(allocator, sizeof(IdentifierIterator<HashTableType, IdentifierType>));
				new (iterator) IdentifierIterator<HashTableType, IdentifierType>(&table);
				iterator->index = index;
				index = table.SkipElements(index, count);
				ECS_ASSERT(index != -1, "Creating hash table sub iterator failed! The given subrange is too large.");
				return iterator;
			}

			HashTableType table;
			unsigned int index;
		};

		// Pair type must be PairPtr or ConstPairPtr
		template<typename HashTableType, typename PairType>
		struct PairIterator : IteratorInterface<PairType> {
			ECS_INLINE PairIterator(HashTableType* table) : table(*table), index(0), IteratorInterface<ValueType>(table->GetCount()) {}

			PairType* Get() override {
				while (!table.IsItemAt(index)) {
					index++;
				}
				pair.value = table.GetValuePtrFromIndex(index);
				pair.identifier = table.GetIdentifierPtrFromIndex(index);
				index++;
				return &pair;
			}

			bool IsContiguous() const override {
				return false;
			}

			IteratorInterface<PairType>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) override {
				PairIterator<HashTableType, PairType>* iterator = (PairIterator<HashTableType, PairType>*)AllocateEx(allocator, sizeof(PairIterator<HashTableType, PairType>));
				new (iterator) PairIterator<HashTableType, PairType>(&table);
				iterator->index = index;
				index = table.SkipElements(index, count);
				ECS_ASSERT(index != -1, "Creating hash table sub iterator failed! The given subrange is too large.");
				return iterator;
			}

			HashTableType table;
			PairType pair;
			unsigned int index;
		};

		ECS_INLINE ValueIterator<const HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>, const T> ConstValueIterator() const {
			return { this };
		}

		ECS_INLINE IdentifierIterator<const HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>, const Identifier> ConstIdentifierIterator() const {
			return { this };
		}

		ECS_INLINE PairIterator<const HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>, ConstPairPtr> ConstPairIterator() const {
			return { this };
		}

		ECS_INLINE ValueIterator<HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>, T> MutableValueIterator() {
			return { this };
		}

		ECS_INLINE IdentifierIterator<HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>, Identifier> MutableIdentifierIterator() {
			return { this };
		}

		ECS_INLINE PairIterator<HashTable<T, Identifier, TableHashFunction, ObjectHashFunction, SoA>, PairPtr> MutablePairIterator() {
			return { this };
		}

		unsigned char* m_metadata;
		// This is used in order for the .natvis visualization to work
		std::conditional_t<SoA, Value*, Pair*> m_buffer;
		Identifier* m_identifiers;
		unsigned int m_capacity;
		unsigned int m_count;
		TableHashFunction m_function;

#undef ECS_HASH_TABLE_DISTANCE_MASK
#undef ECS_HASH_TABLE_HASH_BITS_MASK
	};

	ECS_INLINE size_t HashTableCapacityForElements(size_t count) {
		return count * 100 / ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR + 1;
	}

	ECS_INLINE size_t HashTablePowerOfTwoCapacityForElements(size_t count) {
		return PowerOfTwoGreater(HashTableCapacityForElements(count));
	}

	template<typename T>
	using HashTableDefault = HashTable<T, ResourceIdentifier, HashFunctionPowerOfTwo>;

	template<typename Identifier, typename TableHashFunction, typename ObjectHashFunction = ObjectHashFallthrough>
	using HashTableEmpty = HashTable<HashTableEmptyValue, Identifier, TableHashFunction, ObjectHashFunction, true>;

	// The identifier must have the function Identifier Copy(AllocatorPolymorphic allocator) const;
	template<typename Table>
	void HashTableCopyIdentifiers(const Table& source, Table& destination, AllocatorPolymorphic allocator) {
		source.ForEachIndexConst([&](unsigned int index) {
			auto current_identifier = source.GetIdentifierFromIndex(index);
			auto* identifier = destination.GetIdentifierPtrFromIndex(index);
			*identifier = current_identifier.Copy(allocator);
		});
	}

	// The value must have the function Value Copy(AllocatorPolymorphic allocator) const; 
	template<typename Table>
	void HashTableCopyValues(const Table& source, Table& destination, AllocatorPolymorphic allocator) {
		source.ForEachIndexConst([&](unsigned int index) {
			auto current_value = source.GetValueFromIndex(index);
			auto* value = destination.GetValuePtrFromIndex(index);
			*value = current_value.Copy(allocator);
		});
	}

	template<typename Value, typename Identifier>
	ECS_INLINE Identifier HashTableCopyRetargetIdentifierDefault(const Value* value, Identifier identifier) {
		return identifier;
	}

	// The identifier must have the function Identifier Copy(AllocatorPolymorphic allocator) const;
	// The value must have the function Value Copy(AllocatorPolymorphic allocator) const;
	// Can optionally specify a retarget identifier lambda to change what the identifier is
	// The arguments are (const Value*, Identifier)
	template<bool copy_values, bool copy_identifiers, typename Table, typename RetargetIdentifier>
	void HashTableCopy(
		const Table& source,
		Table& destination,
		AllocatorPolymorphic allocator,
		RetargetIdentifier&& retarget_identifier
	) {
		if constexpr (!copy_identifiers && !copy_values) {
			destination.Copy(allocator, &source);
		}
		else {
			destination.Initialize(allocator, source.GetCapacity());
			destination.m_function = source.m_function;
			destination.CopyDistanceMetadata(&source);
			destination.m_count = source.m_count;

			source.ForEachIndexConst([&](unsigned int index) {
				auto current_value = source.GetValueFromIndex(index);
				auto* value = destination.GetValuePtrFromIndex(index);
				if constexpr (copy_values) {
					*value = current_value.Copy(allocator);
				}
				else {
					*value = current_value;
				}

				auto current_identifier = source.GetIdentifierFromIndex(index);
				auto* identifier = destination.GetIdentifierPtrFromIndex(index);
				if constexpr (copy_identifiers) {
					*identifier = current_identifier.Copy(allocator);
				}
				else {
					*identifier = retarget_identifier(value, current_identifier);
				}
			});
		}
	}

	// The identifier must have the function Identifier Copy(AllocatorPolymorphic allocator) const;
	// The value must have the function Value Copy(AllocatorPolymorphic allocator) const;
	template<bool copy_values, bool copy_identifiers, typename Table>
	ECS_INLINE void HashTableCopy(
		const Table& source, 
		Table& destination, 
		AllocatorPolymorphic allocator
	) {
		HashTableCopy<copy_values, copy_identifiers>(source, destination, allocator, HashTableCopyRetargetIdentifierDefault<typename Table::Value, typename Table::Identifier>);
	}

	// The identifier must have the function void Deallocate(AllocatorPolymorphic allocator);
	template<typename Table>
	ECS_INLINE void HashTableDeallocateIdentifiers(const Table& source, AllocatorPolymorphic allocator) {
		source.ForEachConst([&](auto value, auto identifier) {
			identifier.Deallocate(allocator);
		});
	}

	// The value must have the function void Deallocate(AllocatorPolymorphic allocator);
	template<typename Table>
	ECS_INLINE void HashTableDeallocateValues(const Table& source, AllocatorPolymorphic allocator) {
		source.ForEachConst([&](auto value, auto identifier) {
			value.Deallocate(allocator);
		});
	}

	// The identifier must have the function void Deallocate(AllocatorPolymorphic allocator);
	// The value must have the function void Deallocate(AllocatorPolymorphic allocator);
	template<bool deallocate_values, bool deallocate_identifiers, typename Table>
	void HashTableDeallocate(const Table& source, AllocatorPolymorphic allocator) {
		if (source.GetCapacity() > 0 && source.GetAllocatedBuffer() != nullptr) {
			if constexpr (deallocate_values || deallocate_identifiers) {
				source.ForEachConst([&](const auto& value, const auto& identifier) {
					if constexpr (deallocate_values) {
						value.Deallocate(allocator);
					}
					if constexpr (deallocate_identifiers) {
						identifier.Deallocate(allocator);
					}
				});
			}
			Deallocate(allocator, source.GetAllocatedBuffer());
		}
	}

	// Both the Value and the Identifier need to have the functions for single allocations
	// size_t CopySize() const;
	// Type CopyTo(uintptr_t& ptr) const;
	// For multiple allocations only the Type Copy(AllocatorPolymorphic) const; function is needed
	template<typename Table>
	void HashTableDeepCopy(const Table& source, Table& destination, AllocatorPolymorphic allocator, bool single_allocation, DebugInfo debug_info) {	
		size_t table_copy_size = source.MemoryOf(source.GetCapacity());

		if (single_allocation) {
			size_t total_size = table_copy_size;
			source.ForEachConst([&](const auto& value, const auto& identifier) {
				total_size += value.CopySize();
				total_size += identifier.CopySize();
			});

			void* allocation = Allocate(allocator, total_size, alignof(void*), debug_info);
			memcpy(allocation, source.GetAllocatedBuffer(), table_copy_size);
			destination.SetBuffers(allocation, source.GetCapacity());
			allocation = OffsetPointer(allocation, table_copy_size);
			uintptr_t ptr = (uintptr_t)allocation;
			source.ForEachIndexConst([&](unsigned int index) {
				const auto* value = source.GetValuePtrFromIndex(index);
				auto* copy_value = destination.GetValuePtrFromIndex(index);
				*copy_value = value->CopyTo(ptr);

				const auto* identifier = source.GetIdentifierPtrFromIndex(index);
				auto* copy_identifier = destination.GetIdentifierPtrFromIndex(index);
				*copy_identifier = identifier->CopyTo(ptr);

				return false;
			});
		}
		else {
			void* allocation = Allocate(allocator, table_copy_size, alignof(void*), debug_info);
			memcpy(allocation, source.GetAllocatedBuffer(), table_copy_size);
			destination.SetBuffers(allocation, source.GetCapacity());

			source.ForEachIndexConst([&](unsigned int index) {
				const auto* value = source.GetValuePtrFromIndex(index);
				auto* copy_value = destination.GetValuePtrFromIndex(index);
				*copy_value = value->Copy(allocator);

				const auto* identifier = source.GetIdentifierPtrFromIndex(index);
				auto* copy_identifier = destination.GetIdentifierPtrFromIndex(index);
				*copy_identifier = identifier->Copy(allocator);

				return false;
			});
		}
	}

	// Same constraints as HashTableDeepCopy
	// Both the Value and the Identifier need to have the functions for single allocations
	// size_t CopySize() const;
	// Type CopyTo(uintptr_t& ptr) const;
	// For multiple allocations only the Type Copy(AllocatorPolymorphic) const; function is needed
	/*template<typename Table>
	Stream<TablePair<Table::T, Table::Identifier>> HashTableDeepCopyToStream(const Table& table, AllocatorPolymorphic allocator, bool single_allocation) {
		Stream<TablePair<Table::T, Table::Identifier>> result; 

		size_t stream_size = result.MemoryOf(table.m_size);

		if (single_allocation) {
			size_t total_size = stream_size;
			table.ForEachConst([&](const auto& value, const auto& identifier) {
				total_size += value.CopySize();
				total_size += identifier.CopySize();
			});

			void* allocation = Allocate(allocator, total_size);
			uintptr_t ptr = (uintptr_t)allocation;
			result.InitializeFromBuffer(ptr, table.m_size);
			result.size = 0;

			table.ForEachConst([&](const auto& value, const auto& identifier) {
				result.Add({ value.CopyTo(ptr), identifier.CopyTo(ptr) });
			});
		}
		else {
			void* allocation = Allocate(allocator, stream_size);
			result.InitializeFromBuffer(allocation, 0);

			table.ForEachConst([&](const auto& value, const auto& identifier) {
				result.Add({ value.Copy(allocator), identifier.Copy(allocator) });
			});
		}

		return result;
	}*/
}