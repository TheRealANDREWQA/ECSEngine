#include "ecspch.h"
#include "Hashing.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	ResourceIdentifier::ResourceIdentifier() : ptr(nullptr), size(0) {}

	ResourceIdentifier::ResourceIdentifier(const char* _ptr) : ptr(_ptr), size(strlen(_ptr)) {}

	ResourceIdentifier::ResourceIdentifier(const wchar_t* _ptr) : ptr(_ptr), size(wcslen(_ptr) * sizeof(wchar_t)) {}

	ResourceIdentifier::ResourceIdentifier(const void* id, unsigned int size) : ptr(id), size(size) {}

	ResourceIdentifier::ResourceIdentifier(Stream<void> identifier) : ptr(identifier.buffer), size(identifier.size) {}

	ResourceIdentifier::ResourceIdentifier(Stream<char> identifier) : ptr(identifier.buffer), size(identifier.size) {}

	ResourceIdentifier::ResourceIdentifier(Stream<wchar_t> identifier) : ptr(identifier.buffer), size(identifier.size * sizeof(wchar_t)) {}

	// ------------------------------------------------------------------------------------------------------------

	bool ResourceIdentifier::operator == (const ResourceIdentifier& other) const {
		return size == other.size && ptr == other.ptr;
	}

	// ------------------------------------------------------------------------------------------------------------

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

	unsigned int ResourceIdentifier::Hash() const
	{
		// Value must be clipped to 3 bytes only - that's the precision of the hash tables
		const char* string = (const char*)ptr;

		unsigned int sum = 0;
		for (size_t index = 0; index < size; index++) {
			sum += string[index] * index;
		}

		return sum * (unsigned int)size;
	}

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

}