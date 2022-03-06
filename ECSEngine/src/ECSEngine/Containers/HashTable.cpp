#include "ecspch.h"
#include "HashTable.h"

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

}