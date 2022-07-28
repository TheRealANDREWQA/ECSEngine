#include "ecspch.h"
#include "Hashing.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	ResourceIdentifier::ResourceIdentifier() : ptr(nullptr), size(0) {}

	ResourceIdentifier::ResourceIdentifier(const char* _ptr) : ptr(_ptr), size(strlen(_ptr)) {}

	ResourceIdentifier::ResourceIdentifier(const wchar_t* _ptr) : ptr(_ptr), size(wcslen(_ptr) * sizeof(wchar_t)) {}

	ResourceIdentifier::ResourceIdentifier(const void* id, unsigned int size) : ptr(id), size(size) {}

	ResourceIdentifier::ResourceIdentifier(Stream<void> identifier) : ptr(identifier.buffer), size(identifier.size) {}

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

}