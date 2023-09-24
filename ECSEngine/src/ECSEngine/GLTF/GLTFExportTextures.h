#pragma once
#include "../Core.h"
#include "GLTFLoader.h"

namespace ECSEngine {

	struct GLTFExportTexture {
		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) const {
			texture_filename.Deallocate(allocator);
			// The data buffer is always allocated with malloc
			free(data.buffer);
		}

		Stream<wchar_t> texture_filename;
		Stream<void> data;
		uint2 dimensions;
		PBRMaterialTextureIndex texture_index;
		// This is either 4 or 1. 1 is for metallic, roughness and occlusion (if present), else 4
		unsigned char channel_count;
	};

	// You must not free the gltf_data in order to use the export textures
	// By default, it will keep only unique entries (if you want for each mesh to have the material, then use false)
	// The allocator is used to allocate the names of the textures
	ECSENGINE_API void GLTFGetExportTextures(
		GLTFData gltf_data, 
		CapacityStream<GLTFExportTexture>* export_textures, 
		AllocatorPolymorphic allocator, 
		bool unique_entries = true
	);

	// Returns true if all textures succeeded, else false. It will continue to write textures even after one has failed.
	// If provided, it can fill the indices of the export textures that have failed
	ECSENGINE_API bool GLTFWriteExportTextures(Stream<wchar_t> directory, Stream<GLTFExportTexture> export_textures, CapacityStream<unsigned int>* failed_textures = nullptr);

	// It does not deallocate the stream
	ECSENGINE_API void GLTFDeallocateExportTextures(Stream<GLTFExportTexture> export_textures, AllocatorPolymorphic allocator);

	struct TaskManager;
	template<typename T>
	struct AtomicStream;
	struct Semaphore;

	struct GLTFExportTexturesDirectlyOptions {
		TaskManager* task_manager;
		Semaphore* semaphore;
		AtomicStream<char>* failure_string = nullptr;
	};

	// It will continue to write textures even after one has failed.
	// The only way to determine if a texture write failed, is to give a failure string
	// And check the size - if it is non zero, than a failure took place
	ECSENGINE_API void GLTFExportTexturesDirectly(
		GLTFData gltf_data,
		Stream<wchar_t> directory,
		GLTFExportTexturesDirectlyOptions* options
	);

}