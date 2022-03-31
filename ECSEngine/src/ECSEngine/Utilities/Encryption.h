#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

#define ENCRYPT_HASH_SHIFT_AMOUNT 32

namespace ECSEngine {

	// This is basically a fibonacci hash
	struct EncryptHash {
		static inline unsigned int Hash(unsigned int key) {
			return (unsigned int)(((unsigned long long)(key * 11400714819323198485llu)) >> ENCRYPT_HASH_SHIFT_AMOUNT);
		}
	};

	ECSENGINE_API void EncryptBuffer(Stream<void> data, unsigned int key);

	ECSENGINE_API void DecryptBuffer(Stream<void> data, unsigned int key);

	ECSENGINE_API void EncryptFastBuffer(Stream<void> data, unsigned int key);

	ECSENGINE_API void DecryptFastBuffer(Stream<void> data, unsigned int key);

	// It might result in weaker cypts than the 4 byte encryption
	ECSENGINE_API void EncryptBufferByte(Stream<void> data, unsigned int key);

	ECSENGINE_API void DecryptBufferByte(Stream<void> data, unsigned int key);

	// The encryption will be separate for each byte
	// It might result in weaker crypts than the 4 byte encryption
	ECSENGINE_API void EncryptFastBufferByte(Stream<void> data, unsigned int key);

	// The encryption will be separate for each byte
	ECSENGINE_API void DecryptFastBufferByte(Stream<void> data, unsigned int key);

}