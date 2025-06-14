#include "ecspch.h"
#include "Hashing.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	bool ResourceIdentifier::operator == (const ResourceIdentifier& other) const {
		return Compare(other);
	}

	// ------------------------------------------------------------------------------------------------------------

	size_t ResourceIdentifier::CopySize() const
	{
		return size;
	}

	// ------------------------------------------------------------------------------------------------------------

	ResourceIdentifier ResourceIdentifier::CopyTo(uintptr_t& ptr_to_copy) const
	{
		void* initial_ptr = (void*)ptr_to_copy;
		memcpy(initial_ptr, ptr, size);
		ptr_to_copy += size;
		return { initial_ptr, size };
	}

	// ------------------------------------------------------------------------------------------------------------

	ResourceIdentifier ResourceIdentifier::Copy(AllocatorPolymorphic allocator) const
	{
		void* allocation = Allocate(allocator, size);
		memcpy(allocation, ptr, size);
		return { allocation, size };
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ResourceIdentifier::Compare(const ResourceIdentifier& other) const {
		if (size != other.size) {
			return false;
		}
		else {
			size_t index = 0;
			Vec32uc char_compare, other_char_compare;
			while (size - index > char_compare.size()) {
				char_compare.load(OffsetPointer(ptr, index));
				other_char_compare.load(OffsetPointer(other.ptr, index));
				if (horizontal_and(char_compare == other_char_compare) == false) {
					return false;
				}
				index += char_compare.size();
			}
			char_compare.load_partial(size - index, OffsetPointer(ptr, index));
			other_char_compare.load_partial(size - index, OffsetPointer(other.ptr, index));
			return horizontal_and(char_compare == other_char_compare);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ResourceIdentifier::Deallocate(AllocatorPolymorphic allocator) const
	{
		ECSEngine::Deallocate(allocator, ptr);
	}

	// ------------------------------------------------------------------------------------------------------------

	unsigned int ResourceIdentifier::Hash() const
	{
		//// Value must be clipped to 3 bytes only - that's the precision of the hash tables
		//const char* string = (const char*)ptr;

		//unsigned int sum = 0;
		//for (size_t index = 0; index < size; index++) {
		//	sum += (unsigned int)string[index] * index;
		//}

		//return sum * (unsigned int)size;

		// It seems that this hash is better as a general use case
		return fnv1a(AsASCII());
	}

	// ------------------------------------------------------------------------------------------------------------

	ResourceIdentifier ResourceIdentifier::WithSuffix(ResourceIdentifier base, CapacityStream<void> temp_buffer, Stream<void> suffix)
	{
		if (suffix.size == 0) {
			return base;
		}
		else {
			temp_buffer.CopyOther(base.ptr, base.size);
			temp_buffer.Add(suffix);
			return temp_buffer;
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	Vec32cb ECS_VECTORCALL HashTableFindSIMDKernel(unsigned int index, unsigned char* m_metadata, unsigned char key_hash_bits, unsigned char hash_bits_mask) {
		// The SIMD registers are way slower in non optimized builds
		Vec32uc key_bits(key_hash_bits), elements, ignore_distance(hash_bits_mask);
		// Only elements that hashed to this slot should be checked
		Vec32uc corresponding_distance(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32);

		// Exclude elements that have distance different from the distance to the current slot
		elements.load(m_metadata + index);
		Vec32uc element_hash_bits = elements & ignore_distance;
		auto are_hashed_to_same_slot = (elements >> 3) == corresponding_distance;
		auto match = element_hash_bits == key_bits;
		match &= are_hashed_to_same_slot;

		return match;
	}

	// ------------------------------------------------------------------------------------------------------------

	unsigned int fnv1a(Stream<void> data)
	{
		const char* characters = (char*)data.buffer;
		constexpr unsigned int PRIME = 0x1000193;
		unsigned int hash = 0x811c9dc5;

		for (size_t index = 0; index < data.size; index++) {
			hash = hash ^ characters[index];
			hash *= PRIME;
		}

		return hash;
	}

	unsigned int Cantor(unsigned int a, unsigned int b) {
		return ((((a + b + 1) * (a + b)) >> 1) + b) * 0x8da6b343;
	}

	unsigned int Cantor(unsigned int a, unsigned int b, unsigned int c) {
		return Cantor(a, Cantor(b, c));
	}

	template<typename IntType>
	ECS_INLINE static unsigned int CantorArrayImpl(Stream<IntType> values) {
		unsigned int value = 0;
		if (values.size == 0) {
			return value;
		}
		else if (values.size == 1) {
			return values[0];
		}
		else {
			value = Cantor(values[0], values[1]);
			for (size_t index = 2; index < values.size; index++) {
				value = Cantor(value, values[index]);
			}
			return value;
		}
	}

	unsigned int Cantor(Stream<unsigned char> values) {
		return CantorArrayImpl(values);
	}

	unsigned int Cantor(Stream<unsigned short> values) {
		return CantorArrayImpl(values);
	}

	unsigned int Cantor(Stream<unsigned int> values) {
		return CantorArrayImpl(values);
	}

	unsigned int CantorAdaptive(Stream<void> values) {
		Stream<unsigned int> unsigned_values = { values.buffer, values.size / sizeof(unsigned int) };
		unsigned int value = Cantor(unsigned_values);
		if (unsigned_values.size * sizeof(unsigned int) < values.size) {
			// There are some remaining bytes - coalesce these into an unsigned int
			unsigned int last_value = 0;
			values.SliceAt(unsigned_values.size * sizeof(unsigned int)).CopyTo(&last_value);
			value = Cantor(value, last_value);
		}
		return value;
	}

	// ------------------------------------------------------------------------------------------------------------

	unsigned int Djb2Hash(unsigned int x, unsigned int y, unsigned int z)
	{
		// This function doesn't seem to work that well, since for linear
		// Probing it can create long chains of consecutive values since
		// The z value is added last
		unsigned int hash = 5381;
		hash = ((hash << 5) + hash) + x;
		hash = ((hash << 5) + hash) + y;
		hash = ((hash << 5) + hash) + z;
		return hash;
	}

	// ------------------------------------------------------------------------------------------------------------

}