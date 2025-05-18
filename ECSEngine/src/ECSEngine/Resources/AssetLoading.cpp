#include "ecspch.h"
#include "AssetMetadataHandling.h"
#include "AssetLoading.h"
#include "../Resources/ResourceManager.h"
#include "../Multithreading/TaskManager.h"
#include "AssetDatabase.h"
#include "AssetDatabaseReference.h"
#include "../GLTF/GLTFLoader.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Utilities/Path.h"
#include "../ECS/World.h"
#include "../OS/FileOS.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	struct MeshLoadData {
		ECS_INLINE bool IsValid() const {
			return (submeshes.size > 0 && submeshes.buffer != nullptr) || time_stamp > 0;
		}

		GLTFMesh coalesced_mesh;
		Stream<Submesh> submeshes;
		size_t time_stamp;
		Stream<unsigned int> different_handles;
	};

	struct TextureLoadData {
		ECS_INLINE bool IsValid() const {
			return time_stamp > 0 || texture.data.size > 0;
		}

		DecodedTexture texture;
		size_t time_stamp;
		Stream<unsigned int> different_handles;
	};

	struct ShaderData {
		ECS_INLINE bool IsValid() const {
			return time_stamp > 0 || source_code.size > 0;
		}

		Stream<char> source_code;
		size_t time_stamp;
		Stream<unsigned int> different_handles;
	};

	struct MiscData {
		Stream<void> data;
		size_t time_stamp;
		Stream<unsigned int> different_handles;
	};

	enum CONTROL_BLOCK_DIMENSION : unsigned char {
		CONTROL_BLOCK_NONE,
		CONTROL_BLOCK_PACKED,
		CONTROL_BLOCK_MULTI_PACKED
	};

	union ControlBlockExtra {
		ECS_INLINE ControlBlockExtra() {}

		struct {
			PackedFile* packed_file;
		};
		struct {
			MultiPackedFile* multi_packed_file;
			Stream<PackedFile> multi_packed_inputs;
		};
			
		CONTROL_BLOCK_DIMENSION dimension;
	};

	// A master block of data that all the threads will reference
	struct AssetLoadingControlBlock {
		ECS_INLINE AllocatorPolymorphic GetThreadAllocator(unsigned int index) {
			return &global_managers[index];
		}

		// Sets the success flag to false
		void Fail(LoadAssetFailure failure) {
			if (load_info.success != nullptr) {
				*load_info.success = false;
			}

			if (load_info.load_failures != nullptr) {
				load_info.load_failures->Add(failure);
			}
		}

		// Returns true if the handle was successfully loaded
		template<typename StreamType>
		bool ExistsIndex(StreamType stream, unsigned int handle, ECS_ASSET_TYPE type) const {
			for (size_t index = 0; index < stream.size; index++) {
				size_t subindex = SearchBytes(stream[index].different_handles.buffer, stream[index].different_handles.size, handle, sizeof(handle));
				if (subindex != -1) {
					return stream[index].IsValid();
				}
			}
			// Check to see if it already exists in the asset database and it was skipped
			if (IsAssetPointerValid(GetAssetFromMetadata(database->GetAssetConst(handle, type), type).buffer)) {
				return true;
			}

			// It wasn't found - a processing failure
			return false;
		}

		// Returns true if the texture for that handle is loaded successfully, else false
		ECS_INLINE bool ExistsTexture(unsigned int handle) const {
			return ExistsIndex(textures, handle, ECS_ASSET_TEXTURE);
		}

		// Returns true if the shader for that handle is loaded successfully, else false
		ECS_INLINE bool ExistsShader(unsigned int handle) const {
			return ExistsIndex(shaders, handle, ECS_ASSET_SHADER);
		}

		GlobalMemoryManager* global_managers;
		// Instead of using the database allocator or the resource manager allocator
		// That could be used at the same time, create a Malloc based allocator and make
		// Our allocations from here
		ResizableLinearAllocator persistent_allocator;

		AssetDatabase* database;
		ResourceManager* resource_manager;
		// These contain the textures unprocessed
		Stream<TextureLoadData> textures;
		// These contain the meshes data unprocessed
		Stream<MeshLoadData> meshes;
		Stream<ShaderData> shaders;
		Stream<MiscData> miscs;

		LoadAssetInfo load_info;

		ControlBlockExtra extra;

		// This pointer is hoisted here because it can point
		// To a lock that the user supplies in the load_info.
		// The field gpu_lock is used in case no user supplied lock is provided.
		SpinLock* gpu_lock_ptr;

		// This lock is used to syncronize access to the immediate GPU context
	private:
		char padding[ECS_CACHE_LINE_SIZE];
	public:
		SpinLock gpu_lock;
	private:
		char padding2[ECS_CACHE_LINE_SIZE];
	};

	// It is the same for all types of preload - meshes, textures, shaders or miscs
	struct PreloadTaskData {
		AssetLoadingControlBlock* control_block;
		unsigned int write_index;
	};

	struct ProcessTaskData {
		AssetLoadingControlBlock* control_block;
		unsigned int write_index;
		unsigned int subhandle_index;
	};

	// ------------------------------------------------------------------------------------------------------------

	static void CallCallback(
		AssetLoadingControlBlock* control_block, 
		ThreadTask* tasks, 
		unsigned int thread_id, 
		unsigned int handle, 
		void* metadata, 
		ECS_ASSET_TYPE asset_type
	) {
		if (tasks[asset_type].function != nullptr) {
			LoadAssetOnSuccessData data;
			data.metadata = metadata;
			data.handle = handle;
			data.database = control_block->database;
			data.resource_manager = control_block->resource_manager;
			data.type = asset_type;
			data.data = tasks[asset_type].data;

			tasks[asset_type].function(thread_id, nullptr, &data);
		}
	}

	static void CallOnPreloadCallback(AssetLoadingControlBlock* control_block, unsigned int thread_id, unsigned int handle, void* metadata, ECS_ASSET_TYPE asset_type) {
		CallCallback(control_block, control_block->load_info.preload_on_success, thread_id, handle, metadata, asset_type);
	}

	static void CallOnProcessCallback(AssetLoadingControlBlock* control_block, unsigned int thread_id, unsigned int handle, void* metadata, ECS_ASSET_TYPE asset_type) {
		CallCallback(control_block, control_block->load_info.process_on_success, thread_id, handle, metadata, asset_type);
	}

#pragma region Processing Tasks

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(ProcessMesh) {
		ProcessTaskData* data = (ProcessTaskData*)_data;
		auto* meshes = &data->control_block->meshes[data->write_index];
		unsigned int handle = meshes->different_handles[data->subhandle_index];
		MeshMetadata* metadata = data->control_block->database->GetMesh(handle);

		// Use the reference count from the database
		CreateAssetFromMetadataExData ex_data = {
			true,
			meshes->time_stamp,
			data->control_block->load_info.mount_point,
			data->control_block->database->GetReferenceCountStandalone(handle, ECS_ASSET_MESH)
		};

		CreateMeshFromMetadataEx(
			data->control_block->resource_manager,
			metadata, 
			&meshes->coalesced_mesh,
			meshes->submeshes,
			&ex_data
		);
		// At the moment this cannot fail

		//if (!success) {
		//	LoadAssetFailure failure;
		//	failure.processing_failure = true;
		//	failure.dependency_failure = false;
		//	failure.asset_type = ECS_ASSET_MESH;
		//	failure.handle = handle;
		//
		//	data->control_block->Fail(failure);
		//	// Make the handle -1 to indicate that the processing failed
		//	meshes->different_handles[data->subhandle_index] = -1;
		//}
		
		CallOnProcessCallback(data->control_block, thread_id, handle, metadata, ECS_ASSET_MESH);

		// Exit from the semaphore - use ex exit
		data->control_block->load_info.finish_semaphore->ExitEx();
	}

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(ProcessTexture) {
		ProcessTaskData* data = (ProcessTaskData*)_data;
		auto* textures = &data->control_block->textures[data->write_index];
		unsigned int handle = textures->different_handles[data->subhandle_index];
		TextureMetadata* metadata = data->control_block->database->GetTexture(handle);

		// Use the reference count from the database
		CreateAssetFromMetadataExData ex_data = {
			true,
			textures->time_stamp,
			data->control_block->load_info.mount_point,
			data->control_block->database->GetReferenceCountStandalone(handle, ECS_ASSET_TEXTURE)
		};

		bool success = CreateTextureFromMetadataEx(
			data->control_block->resource_manager, 
			metadata, 
			textures->texture,
			data->control_block->gpu_lock_ptr,
			&ex_data
		);
		if (!success) {
			LoadAssetFailure failure;
			failure.processing_failure = true;
			failure.dependency_failure = false;
			failure.asset_type = ECS_ASSET_TEXTURE;
			failure.handle = handle;

			data->control_block->Fail(failure);
			// Make the handle -1 to indicate that the processing has failed
			textures->different_handles[data->subhandle_index] = -1;
		}
		else {
			CallOnProcessCallback(data->control_block, thread_id, handle, metadata, ECS_ASSET_TEXTURE);
		}

		// Exit from the semaphore - use ex exit
		data->control_block->load_info.finish_semaphore->ExitEx();
	}

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(ProcessShader) {
		ProcessTaskData* data = (ProcessTaskData*)_data;
		auto* shaders = &data->control_block->shaders[data->write_index];
		unsigned int handle = shaders->different_handles[data->subhandle_index];
		ShaderMetadata* metadata = data->control_block->database->GetShader(handle);

		// Use the reference count from the database
		CreateAssetFromMetadataExData ex_data = {
			true,
			shaders->time_stamp,
			data->control_block->load_info.mount_point,
			data->control_block->database->GetReferenceCountStandalone(handle, ECS_ASSET_SHADER)
		};

		bool success = CreateShaderFromMetadataEx(
			data->control_block->resource_manager, 
			metadata, 
			shaders->source_code,
			&ex_data
		);
		if (!success) {
			LoadAssetFailure failure;
			failure.processing_failure = true;
			failure.dependency_failure = false;
			failure.asset_type = ECS_ASSET_SHADER;
			failure.handle = handle;

			data->control_block->Fail(failure);
			// Make the handle -1 to indicate a processing failure
			shaders->different_handles[data->subhandle_index] = -1;
		}
		else {
			CallOnProcessCallback(data->control_block, thread_id, handle, metadata, ECS_ASSET_SHADER);
		}

		// Exit from the semaphore - use ex exit
		data->control_block->load_info.finish_semaphore->ExitEx();
	}

	// ------------------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ------------------------------------------------------------------------------------------------------------

	static void SpawnProcessingTasks(
		AssetLoadingControlBlock* block,
		TaskManager* task_manager,
		ThreadFunction thread_function,
		unsigned int count,
		unsigned int write_index,
		unsigned int thread_index
	) {
		for (unsigned int index = 0; index < count; index++) {
			ProcessTaskData process_data;
			process_data.control_block = block;
			process_data.subhandle_index = index;
			process_data.write_index = write_index;

			task_manager->AddDynamicTaskWithAffinity({ thread_function, &process_data, sizeof(process_data) }, thread_index);
		}
		block->load_info.finish_semaphore->Enter(count);
	}

	// ------------------------------------------------------------------------------------------------------------

#pragma region Helper Tasks

	// ------------------------------------------------------------------------------------------------------------

	// The functor only reads the data in the appropriate way
	template<typename Functor>
	static void PreloadMeshHelper(World* world, void* _data, unsigned int thread_index, Functor&& functor) {
		PreloadTaskData* data = (PreloadTaskData*)_data;

		auto* mesh_block_pointer = &data->control_block->meshes[data->write_index];
		unsigned int mesh_handle = mesh_block_pointer->different_handles[0];
		MeshMetadata* metadata = data->control_block->database->GetMesh(mesh_handle);
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = MountPathOnlyRel(metadata->file, data->control_block->load_info.mount_point, absolute_path);

		AllocatorPolymorphic allocator = data->control_block->GetThreadAllocator(thread_index);
		GLTFData gltf_data = functor(file_path, allocator, data->control_block);
		if (gltf_data.data == nullptr) {
			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_MESH;
			failure.dependency_failure = false;
			failure.processing_failure = false;
			failure.handle = mesh_handle;

			data->control_block->Fail(failure);

			// We need to signal it
			data->control_block->load_info.finish_semaphore->ExitEx();
			return;
		}

		mesh_block_pointer->time_stamp = OS::GetFileLastWrite(file_path);

		Submesh* submeshes = (Submesh*)Allocate(allocator, sizeof(Submesh) * gltf_data.mesh_count);

		LoadCoalescedMeshFromGLTFOptions load_options;
		load_options.allocate_submesh_name = true;
		load_options.temporary_buffer_allocator = allocator;
		bool success = LoadCoalescedMeshFromGLTF(gltf_data, &mesh_block_pointer->coalesced_mesh, submeshes, metadata->invert_z_axis, &load_options);
		FreeGLTFFile(gltf_data);
		if (success) {
			mesh_block_pointer->submeshes = { submeshes, gltf_data.mesh_count };

			// Call the callback before that
			CallOnPreloadCallback(data->control_block, thread_index, mesh_handle, metadata, ECS_ASSET_MESH);

			// Generate the processing tasks
			SpawnProcessingTasks(data->control_block, world->task_manager, ProcessMesh, mesh_block_pointer->different_handles.size, data->write_index, thread_index);
		}
		else {
			Deallocate(allocator, submeshes);

			mesh_block_pointer->submeshes = { nullptr, 0 };

			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_MESH;
			failure.dependency_failure = false;
			failure.processing_failure = false;
			failure.handle = mesh_handle;
			data->control_block->Fail(failure);
		}

		// Now we can exit from our own task
		data->control_block->load_info.finish_semaphore->ExitEx();
	}

	// ------------------------------------------------------------------------------------------------------------

	// The functor only reads the data in the appropriate way
	template<typename Functor>
	static void PreloadTextureHelper(World* world, void* _data, unsigned int thread_index, Functor&& functor) {
		PreloadTaskData* data = (PreloadTaskData*)_data;

		auto* texture_block_pointer = &data->control_block->textures[data->write_index];
		unsigned int texture_handle = texture_block_pointer->different_handles[0];
		TextureMetadata* metadata = data->control_block->database->GetTexture(texture_handle);
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = MountPathOnlyRel(metadata->file, data->control_block->load_info.mount_point, absolute_path);

		AllocatorPolymorphic allocator = data->control_block->GetThreadAllocator(thread_index);
		Stream<void> file_data = functor(file_path, allocator, data->control_block);
		if (file_data.size == 0) {
			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_TEXTURE;
			failure.dependency_failure = false;
			failure.processing_failure = false;
			failure.handle = texture_handle;

			data->control_block->Fail(failure);
			// We need to signal it
			data->control_block->load_info.finish_semaphore->ExitEx();
			return;
		}

		size_t decode_flags = metadata->sRGB ? ECS_DECODE_TEXTURE_FORCE_SRGB : ECS_DECODE_TEXTURE_NO_SRGB;
		DecodedTexture decoded_texture = DecodeTexture(file_data, file_path, allocator, decode_flags);
		Deallocate(allocator, file_data.buffer);

		if (decoded_texture.data.size == 0) {
			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_TEXTURE;
			failure.dependency_failure = false;
			failure.processing_failure = false;
			failure.handle = texture_handle;

			data->control_block->Fail(failure);

			// We need to signal it
			data->control_block->load_info.finish_semaphore->ExitEx();
			return;
		}

		texture_block_pointer->texture = decoded_texture;
		texture_block_pointer->time_stamp = OS::GetFileLastWrite(file_path);

		// Call the callback
		CallOnPreloadCallback(data->control_block, thread_index, texture_handle, metadata, ECS_ASSET_TEXTURE);

		// Generate the processing tasks
		SpawnProcessingTasks(data->control_block, world->task_manager, ProcessTexture, texture_block_pointer->different_handles.size, data->write_index, thread_index);
	
		// Now we can exit from our own task
		data->control_block->load_info.finish_semaphore->ExitEx();
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	static void PreloadShaderHelper(World* world, void* _data, unsigned int thread_index, Functor&& functor) {
		PreloadTaskData* data = (PreloadTaskData*)_data;

		auto* shader_block_pointer = &data->control_block->shaders[data->write_index];
		unsigned int shader_handle = shader_block_pointer->different_handles[0];
		ShaderMetadata* metadata = data->control_block->database->GetShader(shader_handle);
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = MountPathOnlyRel(metadata->file, data->control_block->load_info.mount_point, absolute_path);

		AllocatorPolymorphic allocator = data->control_block->GetThreadAllocator(thread_index);
		Stream<char> file_data = functor(file_path, allocator, data->control_block);
		if (file_data.size == 0) {
			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_SHADER;
			failure.dependency_failure = false;
			failure.processing_failure = false;
			failure.handle = shader_handle;

			shader_block_pointer->source_code = { nullptr, 0 };
			data->control_block->Fail(failure);

			// We need to signal it
			data->control_block->load_info.finish_semaphore->ExitEx();
			return;
		}

		shader_block_pointer->source_code = file_data;
		shader_block_pointer->time_stamp = OS::GetFileLastWrite(file_path);

		// Call the callback
		CallOnPreloadCallback(data->control_block, thread_index, shader_handle, metadata, ECS_ASSET_SHADER);

		// Generate the processing tasks
		SpawnProcessingTasks(data->control_block, world->task_manager, ProcessShader, shader_block_pointer->different_handles.size, data->write_index, thread_index);

		// Now we can exit from our own task
		data->control_block->load_info.finish_semaphore->ExitEx();
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	static void PreloadMiscHelper(World* world, void* _data, unsigned int thread_index, Functor&& functor) {
		PreloadTaskData* data = (PreloadTaskData*)_data;

		auto* misc_block_pointer = &data->control_block->miscs[data->write_index];
		unsigned int misc_handle = misc_block_pointer->different_handles[0];
		MiscAsset* metadata = data->control_block->database->GetMisc(misc_handle);
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = MountPathOnlyRel(metadata->file, data->control_block->load_info.mount_point, absolute_path);

		Stream<void> file_data = functor(file_path, data->control_block);
		if (file_data.size == 0) {
			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_MISC;
			failure.dependency_failure = false;
			failure.processing_failure = false;
			failure.handle = misc_handle;

			misc_block_pointer->data = { nullptr, 0 };
			data->control_block->Fail(failure);

			// We need to signal it
			data->control_block->load_info.finish_semaphore->ExitEx();
			return;
		}

		misc_block_pointer->data = file_data;
		misc_block_pointer->time_stamp = OS::GetFileLastWrite(file_path);

		ResizableStream<void> misc_data_resizable;
		misc_data_resizable.allocator = ECS_MALLOC_ALLOCATOR;
		misc_data_resizable.buffer = file_data.buffer;
		misc_data_resizable.size = file_data.size;
		misc_data_resizable.capacity = file_data.size;

		// Insert the resource into the resource manager
		data->control_block->resource_manager->Lock();
		__try {
			data->control_block->resource_manager->AddResource(file_path, ResourceType::Misc, &misc_data_resizable, false, misc_block_pointer->time_stamp);
		}
		__finally {
			data->control_block->resource_manager->Unlock();
		}

		// Go through all the misc handles and set their data
		for (size_t index = 0; index < misc_block_pointer->different_handles.size; index++) {
			MiscAsset* current_asset = data->control_block->database->GetMisc(misc_block_pointer->different_handles[index]);
			SetMiscData(current_asset, file_data);
		}

		// Call the callback
		CallOnPreloadCallback(data->control_block, thread_index, misc_handle, metadata, ECS_ASSET_MISC);

		// Now we can exit from our own task
		data->control_block->load_info.finish_semaphore->ExitEx();
	}

	// ------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Preload Tasks

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadMeshTask) {
		PreloadMeshHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, const auto* control_block) {
			return LoadGLTFFile(file_path, allocator);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadTextureTask) {
		PreloadTextureHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, const auto* control_block) {
			return ReadWholeFileBinary(file_path, allocator);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadShaderTask) {
		PreloadShaderHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, const auto* control_block) {
			return ReadWholeFileText(file_path, allocator);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadMiscTask) {
		PreloadMiscHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, const auto* control_block) {
			return ReadWholeFileBinary(file_path);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Single Packed File

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadPackedMeshTask) {
		PreloadMeshHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			Stream<void> data = UnpackFile(file_path, control_block->extra.packed_file, allocator);
			GLTFData gltf_data = LoadGLTFFileFromMemory(data, allocator);
			Deallocate(allocator, data.buffer);
			return gltf_data;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadPackedTextureTask) {
		PreloadTextureHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			return UnpackFile(file_path, control_block->extra.packed_file, allocator);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadPackedShaderTask) {
		PreloadShaderHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			Stream<void> source_code = UnpackFile(file_path, control_block->extra.packed_file, allocator);
			return Stream<char>(source_code.buffer, source_code.size);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadPackedMiscTask) {
		PreloadMiscHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AssetLoadingControlBlock* control_block) {
			return UnpackFile(file_path, control_block->extra.packed_file);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Multi Packed File

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadMultiPackedMeshTask) {
		PreloadMeshHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			unsigned int packed_index = GetMultiPackedFileIndex(file_path, control_block->extra.multi_packed_file);
			Stream<void> data = UnpackFile(file_path, &control_block->extra.multi_packed_inputs[packed_index], allocator);
			GLTFData gltf_data = LoadGLTFFileFromMemory(data, allocator);
			Deallocate(allocator, data.buffer);
			return gltf_data;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadMultiPackedTextureTask) {
		PreloadTextureHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			unsigned int packed_index = GetMultiPackedFileIndex(file_path, control_block->extra.multi_packed_file);
			return UnpackFile(file_path, &control_block->extra.multi_packed_inputs[packed_index], allocator);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadMultiPackedShaderTask) {
		PreloadShaderHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			unsigned int packed_index = GetMultiPackedFileIndex(file_path, control_block->extra.multi_packed_file);
			Stream<void> data = UnpackFile(file_path, &control_block->extra.multi_packed_inputs[packed_index], allocator);
			return Stream<char>(data.buffer, data.size);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(PreloadMultiPackedMiscTask) {
		PreloadMiscHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AssetLoadingControlBlock* control_block) {
			unsigned int packed_index = GetMultiPackedFileIndex(file_path, control_block->extra.multi_packed_file);
			return UnpackFile(file_path, &control_block->extra.multi_packed_inputs[packed_index]);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ------------------------------------------------------------------------------------------------------------

	static ECS_THREAD_TASK(ProcessFinalTask) {
		AssetLoadingControlBlock* data = (AssetLoadingControlBlock*)_data;
		
		// Create the samplers - this can be done in parallel with other tasks
		auto samplers = data->database->gpu_sampler_metadata.ToStream();

		// These cannot fail
		for (size_t index = 0; index < samplers.size; index++) {
			GPUSamplerMetadata* metadata = &samplers[index].value;
			CreateSamplerFromMetadata(data->resource_manager, metadata);

			// Call the callback, if any
			CallOnPreloadCallback(data, thread_id, data->database->GetAssetHandleFromIndex(index, ECS_ASSET_GPU_SAMPLER), metadata, ECS_ASSET_GPU_SAMPLER);
		}

		// Before waiting for the finish semaphore, we need to fully pop our thread queue such that
		// We don't wait for the semaphore while nobody runs that task
		TaskManager* task_manager = world->task_manager;
		ThreadQueue* thread_queue = task_manager->GetThreadQueue(thread_id);
		DynamicThreadTask this_thread_queue_task;
		while (thread_queue->Pop(this_thread_queue_task)) {
			task_manager->ExecuteDynamicTask(this_thread_queue_task.task, thread_id, thread_id);
		}

		// Wait until all threads have finished - the value is 2 since
		// By default the semaphore should have 1 and another one incremented for this task
		data->load_info.finish_semaphore->TickWait(15, 2);

		Stream<ReferenceCounted<MaterialAsset>> materials = data->database->material_asset.ToStream();

		auto fail = [data](size_t index, bool dependency) {
			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_MATERIAL;
			failure.dependency_failure = dependency;
			failure.processing_failure = !dependency;
			failure.handle = data->database->material_asset.GetHandleFromIndex(index);

			data->Fail(failure);
		};

		// Currently, we don't to acquire the GPU lock because the resources that the
		// Materials use should already be loaded, we only need the resource manager to be locked
		// Because the lookup of the resources needs to be protected
		data->resource_manager->Lock();

		__try {
			for (size_t index = 0; index < materials.size; index++) {
				MaterialAsset* material_asset = &materials[index].value;
				unsigned int material_handle = data->database->GetAssetHandleFromIndex(index, ECS_ASSET_MATERIAL);
				bool is_valid = ValidateAssetMetadataOptions(material_asset, ECS_ASSET_MATERIAL);

				if (is_valid) {
					// Check to see that the corresponding shaders and textures have been loaded
					bool exists_pixel_shader = data->ExistsShader(material_asset->pixel_shader_handle);
					if (!exists_pixel_shader) {
						fail(index, true);
						continue;
					}

					bool exists_vertex_shader = data->ExistsShader(material_asset->vertex_shader_handle);
					if (!exists_vertex_shader) {
						fail(index, true);
						continue;
					}

					bool texture_failure = false;
					Stream<MaterialAssetResource> combined_textures = material_asset->GetCombinedTextures();
					size_t subindex = 0;
					for (; subindex < combined_textures.size; subindex++) {
						if (combined_textures[subindex].metadata_handle != -1) {
							bool exists_texture = data->ExistsTexture(combined_textures[subindex].metadata_handle);
							if (!exists_texture) {
								fail(index, true);
								texture_failure = true;
								break;
							}
						}
					}

					if (!texture_failure) {
						// Everything is in order. Can pass the material to the creation call
						bool success = CreateMaterialFromMetadata(data->resource_manager, data->database, material_asset, data->load_info.mount_point);
						if (!success) {
							fail(index, false);
						}
						else {
							// Call the callback
							CallOnPreloadCallback(data, thread_id, material_handle, material_asset, ECS_ASSET_MATERIAL);
						}
					}
				}
			}
		}
		__finally {
			// Release the lock
			data->resource_manager->Unlock();
		}

		// Deallocate all resources - starting with the thread allocators
		unsigned int thread_count = world->task_manager->GetThreadCount();
		for (unsigned int index = 0; index < thread_count; index++) {
			data->global_managers[index].Free();
		}

		// We need to take these out from the data, since the data was allocated from
		// The persistent allocator with Malloc, it will result in a crash since it
		// Can unmap these memory regions
		Semaphore* finish_semaphore = data->load_info.finish_semaphore;
		ResizableLinearAllocator persistent_allocator = data->persistent_allocator;

		// This is the code needed to deallocate every single allocation 
		// But since it is a temporary allocator, we can simply deallocate the entire allocator
		persistent_allocator.Free();

		// AllocatorPolymorphic persistent_allocator = &data->persistent_allocator;
		//Deallocate(persistent_allocator, data->global_managers);
		//if (data->extra.dimension == CONTROL_BLOCK_PACKED) {
		//	HashTableDeallocateIdentifiers(data->extra.packed_file->lookup_table, persistent_allocator);
		//	Deallocate(persistent_allocator, data->extra.packed_file->lookup_table.GetAllocatedBuffer());
		//	CloseFile(data->extra.packed_file->file_handle);
		//}
		//else if (data->extra.dimension == CONTROL_BLOCK_MULTI_PACKED) {
		//	data->extra.multi_packed_file->Deallocate(persistent_allocator);
		//	for (size_t index = 0; index < data->extra.multi_packed_inputs.size; index++) {
		//		HashTableDeallocate<false, true>(data->extra.multi_packed_inputs[index].lookup_table, persistent_allocator);
		//		CloseFile(data->extra.multi_packed_inputs[index].file_handle);
		//	}
		//}

		//// Now the control block itself and the its streams
		//for (size_t index = 0; index < data->meshes.size; index++) {
		//	Deallocate(persistent_allocator, data->meshes[index].different_handles.buffer);
		//	// The meshes and the cgltf data are automatically freed since the thread allocator is freed
		//}

		//for (size_t index = 0; index < data->textures.size; index++) {
		//	Deallocate(persistent_allocator, data->textures[index].different_handles.buffer);
		//	// The texture data is automatically deallocated since the thread allocator is freed
		//}

		//for (size_t index = 0; index < data->shaders.size; index++) {
		//	Deallocate(persistent_allocator, data->shaders[index].different_handles.buffer);
		//	// The shader source code is automatically deallocated since the thread allocator is freed
		//}

		//for (size_t index = 0; index < data->miscs.size; index++) {
		//	Deallocate(persistent_allocator, data->miscs[index].different_handles.buffer);
		//	// The misc data doesn't need to be deallocated
		//}

		//if (data->meshes.size > 0) {
		//	Deallocate(persistent_allocator, data->meshes.buffer);
		//}
		//if (data->textures.size > 0) {
		//	Deallocate(persistent_allocator, data->textures.buffer);
		//}
		//if (data->shaders.size > 0) {
		//	Deallocate(persistent_allocator, data->shaders.buffer);
		//}
		//Deallocate(persistent_allocator, data);

		//// Deallocate the mount point if any
		//DeallocateIfBelongs(persistent_allocator, data->load_info.mount_point.buffer);

		//// Deallocate the thread task data
		//for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		//	if (data->load_info.preload_on_success[index].function != nullptr && data->load_info.preload_on_success[index].data_size > 0) {
		//		Deallocate(persistent_allocator, data->load_info.preload_on_success[index].data);
		//	}
		//	if (data->load_info.process_on_success[index].function != nullptr && data->load_info.process_on_success[index].data_size > 0) {
		//		Deallocate(persistent_allocator, data->load_info.process_on_success[index].data);
		//	}
		//}

		// Exit from the semaphore
		finish_semaphore->ExitEx(2);
	}

	// ------------------------------------------------------------------------------------------------------------

	// Must be deallocated by the last thread task
	static AssetLoadingControlBlock* InitializeControlBlock(
		AssetDatabase* database, 
		ResourceManager* resource_manager,
		const LoadAssetInfo* load_info, 
		const ControlBlockExtra* extra,
		unsigned int thread_count
	) {
		// Start by getting all the unique handles and preload them, then apply the processing upon that data
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 12);
		AllocatorPolymorphic allocator = &stack_allocator;

		// For the packed case, we need to copy the hash table and the packed file
		ResizableLinearAllocator persistent_allocator{ ECS_MB, ECS_MB * 24, ECS_MALLOC_ALLOCATOR };

		AssetDatabaseSameTargetAll same_target = database->SameTargetAll(allocator);
		// Go through all current assets and check to see if they exist already in the resource manager
		// If they do, we can skip them
		auto remove_existing_assets = [&](Stream<AssetDatabaseSameTargetAsset>& elements, ECS_ASSET_TYPE type) {
			for (size_t index = 0; index < elements.size; index++) {
				// Use the assign overload such that any discrepancies between the resource manager
				// And the asset database are resolved here, otherwise, later on in the dependency
				// Resolve of assets that have dependencies, they can fail because it will check the
				// Asset database only to see if the resource is loaded. If we assigned the resource manager value, 
				// We need to update all metadatas that reference the same handle - since they also might have their
				// Value desynched.
				void* asset_metadata = database->GetAsset(elements[index].handle, type);
				bool has_assigned = false;
				if (IsAssetFromMetadataLoadedAndAssign(resource_manager, asset_metadata, type, load_info->mount_point, false, &has_assigned)) {
					if (has_assigned) {
						Stream<void> asset_pointer_value = GetAssetFromMetadata(asset_metadata, type);
						for (size_t subindex = 0; subindex < elements[index].other_handles.size; subindex++) {
							SetAssetToMetadata(database->GetAsset(elements[index].other_handles[subindex], type), type, asset_pointer_value);
						}
					}
					elements.RemoveSwapBack(index);
					index--;
				}
			}
		};
		remove_existing_assets(same_target.meshes, ECS_ASSET_MESH);
		remove_existing_assets(same_target.textures, ECS_ASSET_TEXTURE);
		remove_existing_assets(same_target.shaders, ECS_ASSET_SHADER);
		remove_existing_assets(same_target.miscs, ECS_ASSET_MISC);

		// Create the control block

		AssetLoadingControlBlock* control_block = (AssetLoadingControlBlock*)persistent_allocator.Allocate(sizeof(AssetLoadingControlBlock));
		Stream<wchar_t> mount_point = load_info->mount_point;
		if (mount_point.size > 0) {
			mount_point = StringCopy(&persistent_allocator, mount_point);
		}

		memcpy(&control_block->load_info, load_info, sizeof(*load_info));
		control_block->load_info.mount_point = mount_point;
		control_block->persistent_allocator = persistent_allocator;

		// Copy the thread task data, if necessary
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			if (load_info->preload_on_success[index].function != nullptr) {
				control_block->load_info.preload_on_success[index].data = CopyNonZero(
					&persistent_allocator, 
					load_info->preload_on_success[index].data, 
					load_info->preload_on_success[index].data_size
				);
			}
			if (load_info->process_on_success[index].function != nullptr) {
				control_block->load_info.process_on_success[index].data = CopyNonZero(
					&persistent_allocator,
					load_info->process_on_success[index].data,
					load_info->process_on_success[index].data_size
				);
			}
		}

		control_block->gpu_lock.Clear();
		control_block->database = database;
		control_block->resource_manager = resource_manager;
		control_block->extra = *extra;

		// Determine if the user supplied locks, else use these ones allocated
		control_block->gpu_lock_ptr = load_info->gpu_lock != nullptr ? load_info->gpu_lock : &control_block->gpu_lock;

		if (extra->dimension == CONTROL_BLOCK_PACKED) {
			PackedFile* allocated_packed_file = (PackedFile*)persistent_allocator.Allocate(sizeof(PackedFile));
			HashTableCopy<false, true>(extra->packed_file->lookup_table, allocated_packed_file->lookup_table, &persistent_allocator);
			allocated_packed_file->file_handle = extra->packed_file->file_handle;
			control_block->extra.packed_file = allocated_packed_file;
		}
		// And for the multi packed case we need to make another allocation as well
		else if (extra->dimension == CONTROL_BLOCK_MULTI_PACKED) {
			// Copy the input multi packed file and its stream
			MultiPackedFile* allocated_multi_packed_file = (MultiPackedFile*)persistent_allocator.Allocate(sizeof(MultiPackedFile));
			*allocated_multi_packed_file = extra->multi_packed_file->Copy(&persistent_allocator);

			PackedFile* allocated_packed_files = (PackedFile*)persistent_allocator.Allocate(sizeof(PackedFile) * extra->multi_packed_inputs.size);
			for (size_t index = 0; index < extra->multi_packed_inputs.size; index++) {
				HashTableCopy<false, true>(extra->multi_packed_inputs[index].lookup_table, allocated_packed_files[index].lookup_table, &persistent_allocator);
				allocated_packed_files[index].file_handle = extra->multi_packed_inputs[index].file_handle;
			}

			control_block->extra.multi_packed_file = allocated_multi_packed_file;
			control_block->extra.multi_packed_inputs = { allocated_packed_files, extra->multi_packed_inputs.size };
		}

		if (control_block->load_info.finish_semaphore->count.load(ECS_RELAXED) == 0) {
			// Make it 1
			control_block->load_info.finish_semaphore->count.store(1, ECS_RELAXED);
		}
		
		control_block->meshes.Initialize(&persistent_allocator, same_target.meshes.size);
		control_block->textures.Initialize(&persistent_allocator, same_target.textures.size);
		control_block->shaders.Initialize(&persistent_allocator, same_target.shaders.size);
		control_block->miscs.Initialize(&persistent_allocator, same_target.miscs.size);

		for (size_t index = 0; index < same_target.meshes.size; index++) {
			control_block->meshes[index].different_handles.InitializeAndCopy(&persistent_allocator, same_target.meshes[index].other_handles);
			control_block->meshes[index].submeshes = { nullptr, 0 };
			control_block->meshes[index].coalesced_mesh.name = { nullptr, 0 };
		}

		for (size_t index = 0; index < same_target.textures.size; index++) {
			control_block->textures[index].different_handles.InitializeAndCopy(&persistent_allocator, same_target.textures[index].other_handles);
		}

		for (size_t index = 0; index < same_target.shaders.size; index++) {
			control_block->shaders[index].different_handles.InitializeAndCopy(&persistent_allocator, same_target.shaders[index].other_handles);
		}

		for (size_t index = 0; index < same_target.miscs.size; index++) {
			control_block->miscs[index].different_handles.InitializeAndCopy(&persistent_allocator, same_target.miscs[index].other_handles);
		}

		const size_t ALLOCATOR_SIZE = ECS_MB * 200;
		const size_t ALLOCATOR_BACKUP = ECS_MB * 500;

		// Create the global allocators
		control_block->global_managers = (GlobalMemoryManager*)persistent_allocator.Allocate(sizeof(GlobalMemoryManager) * thread_count);
		for (unsigned int index = 0; index < thread_count; index++) {
			CreateGlobalMemoryManager(&control_block->global_managers[index], ALLOCATOR_SIZE, ECS_KB * 8, ALLOCATOR_BACKUP);
		}

		return control_block;
	}

	// ------------------------------------------------------------------------------------------------------------

	struct PreloadTaskFunctions {
		ThreadFunction mesh_task;
		ThreadFunction texture_task;
		ThreadFunction shader_task;
		ThreadFunction miscs_task;
	};

	struct LaunchPreloadTasksData {
		AssetLoadingControlBlock* control_block;
		TaskManager* task_manager;
		PreloadTaskFunctions functions;
	};

	static void LaunchPreloadTasks(LaunchPreloadTasksData* task_data) {
		// It doesn't matter the order of the preloads - except for the shaders
		// They must be loaded first such that when the process material thread task is executed
		// the most amount (likely all) shaders are already loaded
		auto launch_tasks = [task_data](auto stream_type, ThreadFunction thread_function) {
			for (size_t index = 0; index < stream_type.size; index++) {
				PreloadTaskData data;
				data.control_block = task_data->control_block;
				data.write_index = index;

				task_data->task_manager->AddDynamicTaskAndWake({ thread_function, &data, sizeof(data) });
			}
			task_data->control_block->load_info.finish_semaphore->Enter(stream_type.size);
		};

		launch_tasks(task_data->control_block->shaders, task_data->functions.shader_task);
		launch_tasks(task_data->control_block->meshes, task_data->functions.mesh_task);
		launch_tasks(task_data->control_block->textures, task_data->functions.texture_task);
		launch_tasks(task_data->control_block->miscs, task_data->functions.miscs_task);

		// Add the final task of processing
		task_data->task_manager->AddDynamicTaskAndWake({ ProcessFinalTask, task_data->control_block, 0 });
		task_data->control_block->load_info.finish_semaphore->Enter();
	}

	static void LoadAssetsImpl(
		AssetDatabase* database, 
		ResourceManager* resource_manager, 
		TaskManager* task_manager,
		const LoadAssetInfo* load_info, 
		const ControlBlockExtra* extra,
		const PreloadTaskFunctions* preload_functions
	) {
		AssetLoadingControlBlock* control_block = InitializeControlBlock(database, resource_manager, load_info, extra, task_manager->GetThreadCount());
		LaunchPreloadTasksData preload_data;
		preload_data.control_block = control_block;
		preload_data.task_manager = task_manager;
		preload_data.functions = *preload_functions;
		LaunchPreloadTasks(&preload_data);
	}

	// ------------------------------------------------------------------------------------------------------------

	void LoadAssets(
		AssetDatabase* database,
		ResourceManager* resource_manager,
		TaskManager* task_manager,
		const LoadAssetInfo* load_info
	)
	{
		PreloadTaskFunctions preload_functions;

		preload_functions.mesh_task = PreloadMeshTask;
		preload_functions.texture_task = PreloadTextureTask;
		preload_functions.shader_task = PreloadShaderTask;
		preload_functions.miscs_task = PreloadMiscTask;

		ControlBlockExtra extra;
		extra.dimension = CONTROL_BLOCK_NONE;

		LoadAssetsImpl(database, resource_manager, task_manager, load_info, &extra, &preload_functions);
	}

	// ------------------------------------------------------------------------------------------------------------

	void LoadAssetsPacked(
		AssetDatabase* database,
		ResourceManager* resource_manager,
		TaskManager* task_manager,
		const PackedFile* packed_file,
		const LoadAssetInfo* load_info
	) {
		PreloadTaskFunctions preload_functions;

		preload_functions.mesh_task = PreloadPackedMeshTask;
		preload_functions.texture_task = PreloadPackedTextureTask;
		preload_functions.shader_task = PreloadPackedShaderTask;
		preload_functions.miscs_task = PreloadPackedMiscTask;

		ControlBlockExtra extra;
		// We will initialize a copy inside the load assets impl
		extra.packed_file = (PackedFile*)packed_file;
		extra.dimension = CONTROL_BLOCK_PACKED;

		LoadAssetsImpl(database, resource_manager, task_manager, load_info, &extra, &preload_functions);
	}

	// ------------------------------------------------------------------------------------------------------------

	void LoadAssetsPacked(
		AssetDatabase* database,
		ResourceManager* resource_manager,
		TaskManager* task_manager,
		Stream<PackedFile> packed_files,
		const MultiPackedFile* multi_packed_file,
		const LoadAssetInfo* load_info
	)
	{
		PreloadTaskFunctions preload_functions;

		preload_functions.mesh_task = PreloadMultiPackedMeshTask;
		preload_functions.texture_task = PreloadMultiPackedTextureTask;
		preload_functions.shader_task = PreloadMultiPackedShaderTask;
		preload_functions.miscs_task = PreloadMultiPackedMiscTask;

		// These will be allocated from a persistent allocator inside the Load assets impl
		ControlBlockExtra extra;
		extra.multi_packed_file = (MultiPackedFile*)multi_packed_file;
		extra.multi_packed_inputs = packed_files;
		extra.dimension = CONTROL_BLOCK_MULTI_PACKED;

		LoadAssetsImpl(database, resource_manager, task_manager, load_info, &extra, &preload_functions);
	}

	// ------------------------------------------------------------------------------------------------------------

	void DeallocateAssetsWithRemapping(
		AssetDatabase* database, 
		ResourceManager* resource_manager, 
		const DeallocateAssetsWithRemappingOptions* options,
		CapacityStream<MissingAsset>* missing_assets
	)
	{
		auto iterate = [=](ECS_ASSET_TYPE type, auto use_mask) {
			unsigned int count;
			if constexpr (use_mask) {
				count = options->asset_mask[type].size;
			}
			else {
				count = database->GetAssetCount(type);
			}

			for (unsigned int index = 0; index < count; index++) {
				unsigned int handle;
				if constexpr (use_mask) {
					handle = options->asset_mask[type][index];
				}
				else {
					handle = database->GetAssetHandleFromIndex(index, type);
				}

				void* metadata = database->GetAsset(handle, type);
				if (options->decrement_dependencies) {
					// Check to see if it has dependencies and try to get them
					ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
					GetAssetDependencies(metadata, type, &dependencies);
					for (unsigned int dependency_index = 0; dependency_index < dependencies.size; dependency_index++) {
						unsigned int dependency_handle = dependencies[dependency_index].handle;
						ECS_ASSET_TYPE dependency_type = dependencies[dependency_index].type;
						const void* dependency = database->GetAssetConst(dependency_handle, dependency_type);
						if (IsAssetFromMetadataLoaded(resource_manager, dependency, dependency_type, options->mount_point)) {
							DeallocateAssetFromMetadata(resource_manager, database, dependency, dependency_type, options->mount_point);
							database->RemoveAssetForced(dependency_handle, dependency_type);
						}
					}
				}
				if (IsAssetFromMetadataLoaded(resource_manager, metadata, type, options->mount_point)) {
					DeallocateAssetFromMetadata(resource_manager, database, metadata, type, options->mount_point);
					database->RemoveAssetForced(handle, type);
				}
			}
		};

		if (options->asset_mask != nullptr) {
			for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				iterate((ECS_ASSET_TYPE)index, std::true_type{});
			}
		}
		else {
			for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				iterate((ECS_ASSET_TYPE)index, std::false_type{});
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void DeallocateAssetsWithRemapping(
		AssetDatabaseReference* database_reference, 
		ResourceManager* resource_manager, 
		const DeallocateAssetsWithRemappingOptions* options
	)
	{
		AssetDatabase* database = database_reference->GetDatabase();

		auto loop = [=](auto use_mask) {
			for (size_t asset_index = 0; asset_index < ECS_ASSET_TYPE_COUNT; asset_index++) {
				ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)asset_index;
				unsigned int current_count;
				if constexpr (use_mask) {
					current_count = options->asset_mask[current_type].size;
				}
				else {
					current_count = database_reference->GetCount(current_type);
				}

				for (unsigned int index = 0; index < current_count; index++) {
					unsigned int index_to_remove;
					if constexpr (use_mask) {
						index_to_remove = database_reference->GetIndex(options->asset_mask[current_type][index], current_type);
					}
					else {
						index_to_remove = index;
					}

					// Verify if, for some reason, the dependency loading is disabled
					if (options->decrement_dependencies) {
						database_reference->RemoveAssetWithAction(index_to_remove, current_type, [&](unsigned int handle, ECS_ASSET_TYPE type, const void* metadata) {
							DeallocateAssetFromMetadata(resource_manager, database, metadata, type, options->mount_point);
						});
					}
					else {
						database_reference->RemoveAssetWithAction<true>(index_to_remove, current_type, [&](unsigned int handle, ECS_ASSET_TYPE type, const void* metadata) {
							DeallocateAssetFromMetadata(resource_manager, database, metadata, type, options->mount_point);
						});
					}
				}
			}
		};

		if (options->asset_mask != nullptr) {
			loop(std::true_type{});
		}
		else {
			loop(std::false_type{});
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	bool LoadAssetHasFailed(Stream<LoadAssetFailure> failures, unsigned int handle, ECS_ASSET_TYPE type)
	{
		for (size_t index = 0; index < failures.size; index++) {
			if (failures[index].handle == handle && failures[index].asset_type == type) {
				return true;
			}
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------

	void LoadAssetFailure::ToString(const AssetDatabase* database, CapacityStream<char>& characters) const
	{
		// The name is always the first field
		Stream<char> name = database->GetAssetName(handle, asset_type);
		Stream<wchar_t> file = database->GetAssetPath(handle, asset_type);

		Stream<char> type_string = ConvertAssetTypeString(asset_type);
		const char* failure_string_base = nullptr;
		if (processing_failure) {
			failure_string_base = "LoadFailure: Could not process asset {#}, type {#}";
		}
		else {
			if (dependency_failure) {
				failure_string_base = "LoadFailure: Asset {#}, type {#}, could not be loaded because a dependency failed";
			}
			else {
				failure_string_base = "LoadFailure: Could not read data from disk for asset {#}, type {#}";
			}
		}

		if (file.size > 0) {
			ECS_STACK_CAPACITY_STREAM(char, failure_string, 512);
			failure_string.CopyOther(failure_string_base);
			failure_string.AddStream(" with target file {#}");
			failure_string.Add('\0');
			FormatString(characters, failure_string.buffer, name, type_string, file);
		}
		else {
			FormatString(characters, failure_string_base, name, type_string);
		}

	}

	// ------------------------------------------------------------------------------------------------------------

}