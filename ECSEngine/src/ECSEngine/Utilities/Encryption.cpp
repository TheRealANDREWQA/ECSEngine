#include "ecspch.h"
#include "Encryption.h"

#define TABLE_CAPACITY 128

namespace ECSEngine {

	// ---------------------------------------------------------------------------------------------------------

	void EncryptBuffer(Stream<void> data, unsigned int key)
	{
		Vec8ui simd_key;
		const size_t SIMD_capacity = simd_key.size();

		// For every 32 byte chunk, use 8 keys inside a SIMD register in order
		// to have fast encryption and decryption
		unsigned int chunk_count = data.size >> 5;
		unsigned int* reinterpretation = (unsigned int*)data.buffer;

		unsigned int keys[SIMD_capacity];

		keys[0] = EncryptHash::Hash(key);
		for (unsigned int index = 0; index < SIMD_capacity - 1; index++) {
			keys[index + 1] = EncryptHash::Hash(keys[index]);
		}
		simd_key.load(keys);

		for (unsigned int index = 0; index < chunk_count; index++) {
			unsigned int* pointer = reinterpretation + index * SIMD_capacity;
			Vec8ui current_items = Vec8ui().load(pointer);
			current_items ^= simd_key;
			current_items.store(pointer);
		}

		// For the remainder of the elements, just invert the bits
		unsigned int remainder = data.size & (SIMD_capacity * 4 - 1);
		unsigned char* characters = (unsigned char*)data.buffer;
		unsigned int offset = chunk_count << 5;
		for (unsigned int index = 0; index < remainder; index++) {
			characters[offset + index] = ~characters[offset + index];
		}
	}

	// ---------------------------------------------------------------------------------------------------------

	void DecryptBuffer(Stream<void> data, unsigned int key)
	{
		// This is the same process for decryption - the process is symmetric
		EncryptBuffer(data, key);
	}

	// ---------------------------------------------------------------------------------------------------------

	// Just a regular XOR encoding with a single 4 byte key
	void EncryptFastBuffer(Stream<void> data, unsigned int key)
	{
		Vec8ui simd_key(key);
		const size_t SIMD_capacity = simd_key.size();

		unsigned int chunk_count = data.size / SIMD_capacity;
		unsigned int* reinterpretation = (unsigned int*)data.buffer;

		for (unsigned int index = 0; index < chunk_count; index++) {
			unsigned int* pointer = reinterpretation + index * SIMD_capacity;
			Vec8ui current_elements = Vec8ui().load(pointer);
			current_elements ^= simd_key;
			current_elements.store(pointer);
		}

		unsigned int remainder = data.size & 3;
		// For the remainder just flip the bits
		unsigned char* chars = (unsigned char*)data.buffer;
		unsigned int offset = chunk_count << 5;
		for (unsigned int index = 0; index < remainder; index++) {
			chars[offset + index] = ~chars[offset + index];
		}
	}

	// ---------------------------------------------------------------------------------------------------------

	// Just a regular XOR encoding with a single 4 byte key
	void DecryptFastBuffer(Stream<void> data, unsigned int key)
	{
		// The encryption - decryption process is symmetric
		EncryptFastBuffer(data, key);
	}

	// ---------------------------------------------------------------------------------------------------------

	void EncryptBufferByte(Stream<void> data, unsigned int key) {
		Vec32uc encrypt_key;
		const size_t SIMD_capacity = encrypt_key.size();

		unsigned char keys[SIMD_capacity];
		keys[0] = EncryptHash::Hash(key);
		for (size_t index = 0; index < SIMD_capacity - 1; index++) {
			keys[index + 1] = EncryptHash::Hash(keys[index]);
		}

		encrypt_key.load(keys);
		unsigned int chunk_count = data.size / SIMD_capacity;
		unsigned char* characters = (unsigned char*)data.buffer;
		for (unsigned int index = 0; index < chunk_count; index++) {
			unsigned char* pointer = characters + index * SIMD_capacity;
			Vec32uc current_elements = Vec32uc().load(pointer);
			current_elements ^= encrypt_key;
			current_elements.store(pointer);
		}

		unsigned int remainder = data.size % SIMD_capacity;
		unsigned int offset = chunk_count * SIMD_capacity;
		for (unsigned int index = 0; index < remainder; index++) {
			characters[offset + index] = ~characters[offset + index];
		}
	}

	// ---------------------------------------------------------------------------------------------------------
	
	// The process is symmetric
	void DecryptBufferByte(Stream<void> data, unsigned int key) {
		EncryptBufferByte(data, key);
	}

	// ---------------------------------------------------------------------------------------------------------

	void EncryptFastBufferByte(Stream<void> data, unsigned int key) {
		Vec32uc encrypt_key((unsigned char)key);
		const size_t SIMD_capacity = encrypt_key.size();

		unsigned int chunk_count = data.size / SIMD_capacity;
		unsigned char* characters = (unsigned char*)data.buffer;
		for (unsigned int index = 0; index < chunk_count; index++) {
			unsigned char* pointer = characters + index * SIMD_capacity;
			Vec32uc current_elements = Vec32uc().load(pointer);
			current_elements ^= encrypt_key;
			current_elements.store(pointer);
		}

		unsigned int remainder = data.size % SIMD_capacity;
		unsigned int offset = chunk_count * SIMD_capacity;
		for (unsigned int index = 0; index < remainder; index++) {
			characters[offset + index] = ~characters[offset + index];
		}
	}

	// ---------------------------------------------------------------------------------------------------------

	// The process is symmetric
	void DecryptFastBufferByte(Stream<void> data, unsigned int key) {
		EncryptFastBufferByte(data, key);
	}

	// ---------------------------------------------------------------------------------------------------------

}