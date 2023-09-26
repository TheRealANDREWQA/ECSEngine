#pragma once
#include "../Core.h"
#include "GLTFLoader.h"

namespace ECSEngine {

	struct Semaphore;
	struct TaskManager;

	template<typename T>
	struct AtomicStream;

	struct GLTFExportTexturesOptions {
		TaskManager* task_manager;
		Semaphore* semaphore;
		AtomicStream<char>* failure_string = nullptr;

		// If this is given, it will only the textures specified here
		Stream<PBRMaterialMapping> textures_to_write = {};
	};

	// This does not allocate textures_to_write
	// Deallocate with GLTFExportTexturesOptionsDeallocateAll
	ECSENGINE_API void GLTFExportTexturesOptionsAllocateAll(GLTFExportTexturesOptions* options, size_t failure_string_capacity, AllocatorPolymorphic allocator);

	ECSENGINE_API void GLTFExportTexturesOptionsDeallocateAll(const GLTFExportTexturesOptions* options, AllocatorPolymorphic allocator);

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
		bool unique_material_entries = true
	);

	// You must not free the gltf_data in order to use the export textures
	// By default, it will keep only unique entries (if you want for each mesh to have the material, then use false)
	// The allocator is used to allocate the names of the textures. This variant runs on multiple threads
	ECSENGINE_API void GLTFGetExportTexturesNames(
		GLTFData gltf_data,
		CapacityStream<PBRMaterialMapping>* mappings,
		AllocatorPolymorphic allocator,
		bool unique_material_entries = true
	);


	// Returns true if all textures succeeded, else false. It will continue to write textures even after one has failed.
	// If provided, it can fill the indices of the export textures that have failed
	ECSENGINE_API bool GLTFWriteExportTextures(Stream<wchar_t> directory, Stream<GLTFExportTexture> export_textures, CapacityStream<unsigned int>* failed_textures = nullptr);
	
	// It does not deallocate the stream
	ECSENGINE_API void GLTFDeallocateExportTextureNames(Stream<PBRMaterialMapping> mappings, AllocatorPolymorphic allocator);

	// It does not deallocate the stream
	ECSENGINE_API void GLTFDeallocateExportTextures(Stream<GLTFExportTexture> export_textures, AllocatorPolymorphic allocator);

	// It will continue to write textures even after one has failed.
	// The only way to determine if a texture write failed, is to give a failure string
	// And check the size - if it is non zero, then a failure took place
	// Returns true if there are textures that are actually exported, else false
	ECSENGINE_API bool GLTFExportTexturesDirectly(
		GLTFData gltf_data,
		Stream<wchar_t> directory,
		GLTFExportTexturesOptions* options
	);

	// It does a simple search to locate a Textures/Texture/Material/Materials folder. It will start from the given directory
	// And search upwards until it reaches the root_stop after which it will stop and fill in the write_path
	// the current directory. Returns true if it found such a directory, else false
	ECSENGINE_API bool GLTFExportTexturesToFolderSearch(
		Stream<wchar_t> current_directory,
		Stream<wchar_t> root_stop,
		CapacityStream<wchar_t>& write_path
	);

}