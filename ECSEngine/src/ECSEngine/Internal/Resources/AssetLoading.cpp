#include "ecspch.h"
#include "AssetMetadataHandling.h"
#include "AssetLoading.h"
#include "../Resources/ResourceManager.h"
#include "../../Internal/Multithreading/TaskManager.h"
#include "AssetDatabase.h"
#include "../../GLTF/GLTFLoader.h"
#include "../../Allocators/ResizableLinearAllocator.h"
#include "../../Utilities/Path.h"
#include "../../Internal/World.h"
#include "../../Utilities/OSFunctions.h"

namespace ECSEngine {

	AllocatorPolymorphic GetPersistentAllocator(AssetDatabase* database) {
		return database->Allocator();
	}

	// ------------------------------------------------------------------------------------------------------------

	struct MeshLoadData {
		ECS_INLINE bool IsValid() const {
			return (data.data != nullptr && data.mesh_count > 0) || time_stamp > 0;
		}

		GLTFData data;
		GLTFMesh* meshes;
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
		ControlBlockExtra() {}

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
		inline AllocatorPolymorphic GetThreadAllocator(unsigned int index) {
			return GetAllocatorPolymorphic(&global_managers[index]);
		}

		// Sets the success flag to false
		inline void Fail(LoadAssetFailure failure) {
			if (load_info.success != nullptr) {
				*load_info.success = false;
			}

			if (load_info.load_failures != nullptr) {
				load_info.load_failures->Add(failure);
			}
		}

		template<typename StreamType>
		unsigned int FindIndex(StreamType stream, unsigned int handle) const {
			for (size_t index = 0; index < stream.size; index++) {
				size_t subindex = function::SearchBytes(stream[index].different_handles.buffer, stream[index].different_handles.size, handle, sizeof(handle));
				if (subindex != -1) {
					return stream[index].IsValid() ? index : -1;
				}
			}
			// It wasn't found - a processing failure
			return -1;
		}

		// Returns the index inside the textures stream where the given handle is located
		// It returns -1 if the slot could not be loaded/processed
		unsigned int FindTexture(unsigned int handle) const {
			return FindIndex(textures, handle);
		}

		// Returns the index inside the textures stream where the given handle is located
		// It returns -1 if the slot could not be loaded/processed
		unsigned int FindShader(unsigned int handle) const {
			return FindIndex(shaders, handle);
		}

		GlobalMemoryManager* global_managers;

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

		// this lock is used to syncronize access to the immediate GPU context
	private:
		char padding[ECS_CACHE_LINE_SIZE];
	public:
		SpinLock gpu_lock;
	private:
		char padding2[ECS_CACHE_LINE_SIZE];
	public:
		SpinLock manager_lock;
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

#pragma region Processing Tasks

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(ProcessMesh) {
		ProcessTaskData* data = (ProcessTaskData*)_data;
		auto* meshes = &data->control_block->meshes[data->write_index];
		const MeshMetadata* metadata = data->control_block->database->GetMeshConst(meshes->different_handles[data->subhandle_index]);
		CreateAssetFromMetadataExData ex_data = {
			&data->control_block->manager_lock,
			meshes->time_stamp,
			data->control_block->load_info.mount_point
		};

		bool success = CreateMeshFromMetadataEx(
			data->control_block->resource_manager,
			metadata, 
			{ meshes->meshes, meshes->data.mesh_count },
			&ex_data
		);
		if (!success) {
			LoadAssetFailure failure;
			failure.processing_failure = true;
			failure.dependency_failure = false;
			failure.asset_type = ECS_ASSET_MESH;
			failure.handle = meshes->different_handles[data->subhandle_index];
		
			data->control_block->Fail(failure);
			// Make the handle -1 to indicate that the processing failed
			meshes->different_handles[data->subhandle_index] = -1;
		}

		// Exit from the semaphore - use ex exit
		data->control_block->load_info.finish_semaphore->ExitEx();
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(ProcessTexture) {
		ProcessTaskData* data = (ProcessTaskData*)_data;
		auto* textures = &data->control_block->textures[data->write_index];
		const TextureMetadata* metadata = data->control_block->database->GetTextureConst(textures->different_handles[data->subhandle_index]);

		CreateAssetFromMetadataExData ex_data = {
			&data->control_block->manager_lock,
			textures->time_stamp,
			data->control_block->load_info.mount_point
		};

		bool success = CreateTextureFromMetadataEx(
			data->control_block->resource_manager, 
			metadata, 
			textures->texture,
			&data->control_block->gpu_lock,
			&ex_data
		);
		if (!success) {
			LoadAssetFailure failure;
			failure.processing_failure = true;
			failure.dependency_failure = false;
			failure.asset_type = ECS_ASSET_TEXTURE;
			failure.handle = textures->different_handles[data->subhandle_index];

			data->control_block->Fail(failure);
			// Make the handle -1 to indicate that the processing has failed
			textures->different_handles[data->subhandle_index] = -1;
		}

		// Exit from the semaphore - use ex exit
		data->control_block->load_info.finish_semaphore->ExitEx();
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(ProcessShader) {
		ProcessTaskData* data = (ProcessTaskData*)_data;
		auto* shaders = &data->control_block->shaders[data->write_index];
		ShaderMetadata* metadata = data->control_block->database->GetShader(shaders->different_handles[data->subhandle_index]);

		CreateAssetFromMetadataExData ex_data = {
			&data->control_block->manager_lock,
			shaders->time_stamp,
			data->control_block->load_info.mount_point
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
			failure.handle = shaders->different_handles[data->subhandle_index];

			data->control_block->Fail(failure);
			// Make the handle -1 to indicate a processing failure
			shaders->different_handles[data->subhandle_index] = -1;
		}

		// Exit from the semaphore - use ex exit
		data->control_block->load_info.finish_semaphore->ExitEx();
	}

	// ------------------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ------------------------------------------------------------------------------------------------------------

	void SpawnProcessingTasks(
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
	void PreloadMeshHelper(World* world, void* _data, unsigned int thread_index, Functor&& functor) {
		PreloadTaskData* data = (PreloadTaskData*)_data;

		auto* mesh_block_pointer = &data->control_block->meshes[data->write_index];
		unsigned int mesh_handle = mesh_block_pointer->different_handles[0];
		const MeshMetadata* metadata = data->control_block->database->GetMeshConst(mesh_handle);
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPath(metadata->file, data->control_block->load_info.mount_point, absolute_path);

		AllocatorPolymorphic allocator = data->control_block->GetThreadAllocator(thread_index);
		GLTFData gltf_data = functor(file_path, allocator, data->control_block);
		if (gltf_data.data == nullptr) {
			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_MESH;
			failure.dependency_failure = false;
			failure.processing_failure = false;
			failure.handle = mesh_handle;

			data->control_block->Fail(failure);
			return;
		}

		mesh_block_pointer->data = gltf_data;
		mesh_block_pointer->time_stamp = OS::GetFileLastWrite(file_path);

		GLTFMesh* gltf_meshes = (GLTFMesh*)Allocate(allocator, sizeof(GLTFMesh) * gltf_data.mesh_count);
		bool success = LoadMeshesFromGLTF(gltf_data, gltf_meshes, allocator, metadata->invert_z_axis);
		FreeGLTFFile(gltf_data);
		if (success) {
			data->control_block->meshes[data->write_index].meshes = gltf_meshes;
			// Generate the processing tasks
			SpawnProcessingTasks(data->control_block, world->task_manager, ProcessMesh, mesh_block_pointer->different_handles.size, data->write_index, thread_index);
		}
		else {
			Deallocate(allocator, gltf_meshes);

			mesh_block_pointer->meshes = nullptr;
			mesh_block_pointer->data.data = nullptr;
			mesh_block_pointer->data.mesh_count = 0;

			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_MESH;
			failure.dependency_failure = false;
			failure.processing_failure = false;
			failure.handle = mesh_handle;
			data->control_block->Fail(failure);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	// The functor only reads the data in the appropriate way
	template<typename Functor>
	void PreloadTextureHelper(World* world, void* _data, unsigned int thread_index, Functor&& functor) {
		PreloadTaskData* data = (PreloadTaskData*)_data;

		auto* texture_block_pointer = &data->control_block->textures[data->write_index];
		unsigned int texture_handle = texture_block_pointer->different_handles[0];
		const TextureMetadata* metadata = data->control_block->database->GetTextureConst(texture_handle);
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPath(metadata->file, data->control_block->load_info.mount_point, absolute_path);

		AllocatorPolymorphic allocator = data->control_block->GetThreadAllocator(thread_index);
		Stream<void> file_data = functor(file_path, allocator, data->control_block);
		if (file_data.size == 0) {
			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_TEXTURE;
			failure.dependency_failure = false;
			failure.processing_failure = false;
			failure.handle = texture_handle;

			data->control_block->Fail(failure);
			return;
		}
		DecodedTexture decoded_texture = DecodeTexture(file_data, file_path, allocator);
		Deallocate(allocator, file_data.buffer);

		if (decoded_texture.data.size == 0) {
			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_TEXTURE;
			failure.dependency_failure = false;
			failure.processing_failure = false;
			failure.handle = texture_handle;

			data->control_block->Fail(failure);
			return;
		}

		texture_block_pointer->texture = decoded_texture;
		texture_block_pointer->time_stamp = OS::GetFileLastWrite(file_path);

		// Generate the processing tasks
		SpawnProcessingTasks(data->control_block, world->task_manager, ProcessTexture, texture_block_pointer->different_handles.size, data->write_index, thread_index);
	
		// Now we can exit from our own task
		data->control_block->load_info.finish_semaphore->Exit();
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	void PreloadShaderHelper(World* world, void* _data, unsigned int thread_index, Functor&& functor) {
		PreloadTaskData* data = (PreloadTaskData*)_data;

		auto* shader_block_pointer = &data->control_block->shaders[data->write_index];
		unsigned int shader_handle = shader_block_pointer->different_handles[0];
		const ShaderMetadata* metadata = data->control_block->database->GetShaderConst(shader_handle);
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPathOnlyRel(metadata->file, data->control_block->load_info.mount_point, absolute_path);

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
			return;
		}

		shader_block_pointer->source_code = file_data;
		shader_block_pointer->time_stamp = OS::GetFileLastWrite(file_path);

		// Generate the processing tasks
		SpawnProcessingTasks(data->control_block, world->task_manager, ProcessShader, shader_block_pointer->different_handles.size, data->write_index, thread_index);

		// Now we can exit from our own task
		data->control_block->load_info.finish_semaphore->Exit();
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	void PreloadMiscHelper(World* world, void* _data, unsigned int thread_index, Functor&& functor) {
		PreloadTaskData* data = (PreloadTaskData*)_data;

		auto* misc_block_pointer = &data->control_block->miscs[data->write_index];
		unsigned int misc_handle = misc_block_pointer->different_handles[0];
		MiscAsset* metadata = data->control_block->database->GetMisc(misc_handle);
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPathOnlyRel(metadata->file, data->control_block->load_info.mount_point, absolute_path);

		// Use malloc as allocator since these can get quite big
		Stream<void> file_data = functor(file_path, data->control_block);
		if (file_data.size == 0) {
			LoadAssetFailure failure;
			failure.asset_type = ECS_ASSET_MISC;
			failure.dependency_failure = false;
			failure.processing_failure = false;
			failure.handle = misc_handle;

			misc_block_pointer->data = { nullptr, 0 };
			data->control_block->Fail(failure);
			return;
		}

		misc_block_pointer->data = file_data;
		misc_block_pointer->time_stamp = OS::GetFileLastWrite(file_path);

		ResizableStream<void> misc_data_resizable;
		misc_data_resizable.allocator = { nullptr };
		misc_data_resizable.buffer = file_data.buffer;
		misc_data_resizable.size = file_data.size;
		misc_data_resizable.capacity = file_data.size;
		// Insert the resource into the resource manager
		data->control_block->manager_lock.lock();

		data->control_block->resource_manager->AddResource(file_path, ResourceType::Misc, &misc_data_resizable, misc_block_pointer->time_stamp);

		data->control_block->manager_lock.unlock();

		// Go through all the misc handles and set their data
		for (size_t index = 0; index < misc_block_pointer->different_handles.size; index++) {
			MiscAsset* current_asset = data->control_block->database->GetMisc(misc_block_pointer->different_handles[index]);
			SetMiscData(current_asset, file_data);
		}

		// Now we can exit from our own task
		data->control_block->load_info.finish_semaphore->Exit();
	}

	// ------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Preload Tasks

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadMeshTask) {
		PreloadMeshHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, const auto* control_block) {
			return LoadGLTFFile(file_path, allocator);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadTextureTask) {
		PreloadTextureHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, const auto* control_block) {
			return ReadWholeFileBinary(file_path, allocator);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadShaderTask) {
		PreloadShaderHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, const auto* control_block) {
			return ReadWholeFileText(file_path, allocator);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadMiscTask) {
		PreloadMiscHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, const auto* control_block) {
			return ReadWholeFileBinary(file_path);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Single Packed File

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadPackedMeshTask) {
		PreloadMeshHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			Stream<void> data = UnpackFile(file_path, control_block->extra.packed_file, allocator);
			GLTFData gltf_data = LoadGLTFFileFromMemory(data, allocator);
			DeallocateEx(allocator, data.buffer);
			return gltf_data;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadPackedTextureTask) {
		PreloadTextureHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			return UnpackFile(file_path, control_block->extra.packed_file, allocator);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadPackedShaderTask) {
		PreloadShaderHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			Stream<void> source_code = UnpackFile(file_path, control_block->extra.packed_file, allocator);
			return Stream<char>(source_code.buffer, source_code.size);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadPackedMiscTask) {
		PreloadMiscHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AssetLoadingControlBlock* control_block) {
			return UnpackFile(file_path, control_block->extra.packed_file);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Multi Packed File

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadMultiPackedMeshTask) {
		PreloadMeshHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			unsigned int packed_index = GetMultiPackedFileIndex(file_path, control_block->extra.multi_packed_file);
			Stream<void> data = UnpackFile(file_path, &control_block->extra.multi_packed_inputs[packed_index], allocator);
			GLTFData gltf_data = LoadGLTFFileFromMemory(data, allocator);
			DeallocateEx(allocator, data.buffer);
			return gltf_data;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadMultiPackedTextureTask) {
		PreloadTextureHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			unsigned int packed_index = GetMultiPackedFileIndex(file_path, control_block->extra.multi_packed_file);
			return UnpackFile(file_path, &control_block->extra.multi_packed_inputs[packed_index], allocator);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadMultiPackedShaderTask) {
		PreloadShaderHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AllocatorPolymorphic allocator, AssetLoadingControlBlock* control_block) {
			unsigned int packed_index = GetMultiPackedFileIndex(file_path, control_block->extra.multi_packed_file);
			Stream<void> data = UnpackFile(file_path, &control_block->extra.multi_packed_inputs[packed_index], allocator);
			return Stream<char>(data.buffer, data.size);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(PreloadMultiPackedMiscTask) {
		PreloadMiscHelper(world, _data, thread_id, [](Stream<wchar_t> file_path, AssetLoadingControlBlock* control_block) {
			unsigned int packed_index = GetMultiPackedFileIndex(file_path, control_block->extra.multi_packed_file);
			return UnpackFile(file_path, &control_block->extra.multi_packed_inputs[packed_index]);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ------------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(ProcessFinalTask) {
		AssetLoadingControlBlock* data = (AssetLoadingControlBlock*)_data;
	
		// Create the samplers - this can be done in parallel with other tasks
		auto samplers = data->database->gpu_sampler_metadata.ToStream();

		// These cannot fail
		for (size_t index = 0; index < samplers.size; index++) {
			CreateSamplerFromMetadata(data->resource_manager, &samplers[index].value);
		}

		// Wait until all threads have finished - the value is 2 since
		// by default the semaphore should have 1 and another one incremented for this task
		data->load_info.finish_semaphore->TickWait(5'000, 2);

		auto materials = data->database->material_asset.ToStream();
		for (size_t index = 0; index < materials.size; index++) {
			MaterialAsset* material_asset = &materials[index].value;
			// Check to see that the corresponding shaders and textures have been loaded
			unsigned int pixel_shader_index = data->FindShader(material_asset->pixel_shader_handle);

			auto fail = [data](size_t index, bool dependency) {
				LoadAssetFailure failure;
				failure.asset_type = ECS_ASSET_MATERIAL;
				failure.dependency_failure = dependency;
				failure.processing_failure = !dependency;
				failure.handle = data->database->material_asset.GetHandleFromIndex(index);

				data->Fail(failure);
			};

			if (pixel_shader_index == -1) {
				fail(index, true);
				continue;
			}

			unsigned int vertex_shader_index = data->FindShader(material_asset->vertex_shader_handle);
			if (vertex_shader_index == -1) {
				fail(index, true);
				continue;
			}

			size_t subindex = 0;
			for (; subindex < material_asset->textures.size; subindex++) {
				unsigned int texture_index = data->FindTexture(material_asset->textures[subindex].metadata_handle);
				if (texture_index == -1) {
					fail(index, true);
					continue;
				}
			}

			// Everything is in order. Can pass the material to the creation call
			bool success = CreateMaterialFromMetadata(data->resource_manager, data->database, material_asset, data->load_info.mount_point);
			if (!success) {
				fail(index, false);
			}
		}

		// Deallocate all resources - starting with the thread allocators
		unsigned int thread_count = world->task_manager->GetThreadCount();
		for (unsigned int index = 0; index < thread_count; index++) {
			data->global_managers[index].Free();
		}

		AllocatorPolymorphic persistent_allocator = GetPersistentAllocator(data->database);
		Deallocate(persistent_allocator, data->global_managers);
		if (data->extra.dimension == CONTROL_BLOCK_PACKED) {
			HashTableDeallocateIdentifiers(data->extra.packed_file->lookup_table, persistent_allocator);
			Deallocate(persistent_allocator, data->extra.packed_file->lookup_table.GetAllocatedBuffer());
			CloseFile(data->extra.packed_file->file_handle);
		}
		else if (data->extra.dimension == CONTROL_BLOCK_MULTI_PACKED) {
			data->extra.multi_packed_file->Deallocate(persistent_allocator);
			for (size_t index = 0; index < data->extra.multi_packed_inputs.size; index++) {
				HashTableDeallocateWithIdentifiers(data->extra.multi_packed_inputs[index].lookup_table, persistent_allocator);
				CloseFile(data->extra.multi_packed_inputs[index].file_handle);
			}
		}

		// Now the control block itself and the its streams
		for (size_t index = 0; index < data->meshes.size; index++) {
			Deallocate(persistent_allocator, data->meshes[index].different_handles.buffer);
			// The meshes and the cgltf data are automatically freed since the thread allocator is freed
		}

		for (size_t index = 0; index < data->textures.size; index++) {
			Deallocate(persistent_allocator, data->textures[index].different_handles.buffer);
			// The texture data is automatically deallocated since the thread allocator is freed
		}

		for (size_t index = 0; index < data->shaders.size; index++) {
			Deallocate(persistent_allocator, data->shaders[index].different_handles.buffer);
			// The shader source code is automatically deallocated since the thread allocator is freed
		}

		for (size_t index = 0; index < data->miscs.size; index++) {
			Deallocate(persistent_allocator, data->miscs[index].different_handles.buffer);
			// The misc data doesn't need to be deallocated
		}

		if (data->meshes.size > 0) {
			Deallocate(persistent_allocator, data->meshes.buffer);
		}
		if (data->textures.size > 0) {
			Deallocate(persistent_allocator, data->textures.buffer);
		}
		if (data->shaders.size > 0) {
			Deallocate(persistent_allocator, data->shaders.buffer);
		}
		Deallocate(persistent_allocator, data);

		// Deallocate the mount point if any
		DeallocateIfBelongs(persistent_allocator, data->load_info.mount_point.buffer);

		// Exit from the semaphore
		data->load_info.finish_semaphore->ExitEx();
	}

	// ------------------------------------------------------------------------------------------------------------

	// Must be deallocated by the last thread task
	AssetLoadingControlBlock* InitializeControlBlock(
		AssetDatabase* database, 
		ResourceManager* resource_manager,
		const LoadAssetInfo* load_info, 
		const ControlBlockExtra* extra,
		unsigned int thread_count
	) {
		// Start by getting all the unique handles and preload them, then apply the processing upon that data
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 12);
		AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&stack_allocator);

		AssetDatabaseSameTargetAll same_target = database->SameTargetAll(allocator);

		// Create the control block
		// Allocate it from the database allocator or the resource manager allocator
		AllocatorPolymorphic persistent_allocator = database->Allocator();

		AssetLoadingControlBlock* control_block = (AssetLoadingControlBlock*)Allocate(persistent_allocator, sizeof(AssetLoadingControlBlock));
		Stream<wchar_t> mount_point = load_info->mount_point;
		if (mount_point.size > 0) {
			mount_point = function::StringCopy(persistent_allocator, mount_point);
		}

		control_block->load_info = *load_info;
		control_block->load_info.mount_point = mount_point;
		control_block->gpu_lock.unlock();
		control_block->database = database;
		control_block->resource_manager = resource_manager;
		control_block->extra = *extra;

		if (control_block->load_info.finish_semaphore->count.load(ECS_RELAXED) == 0) {
			// Make it 1
			control_block->load_info.finish_semaphore->count.store(1, ECS_RELAXED);
		}
		
		control_block->meshes.Initialize(persistent_allocator, same_target.meshes.size);
		control_block->textures.Initialize(persistent_allocator, same_target.textures.size);
		control_block->shaders.Initialize(persistent_allocator, same_target.shaders.size);
		control_block->miscs.Initialize(persistent_allocator, same_target.miscs.size);

		for (size_t index = 0; index < same_target.meshes.size; index++) {
			control_block->meshes[index].different_handles.InitializeAndCopy(persistent_allocator, same_target.meshes[index].other_handles);
			control_block->meshes[index].data.data = nullptr;
		}

		for (size_t index = 0; index < same_target.textures.size; index++) {
			control_block->textures[index].different_handles.InitializeAndCopy(persistent_allocator, same_target.textures[index].other_handles);
		}

		for (size_t index = 0; index < same_target.shaders.size; index++) {
			control_block->shaders[index].different_handles.InitializeAndCopy(persistent_allocator, same_target.shaders[index].other_handles);
		}

		for (size_t index = 0; index < same_target.miscs.size; index++) {
			control_block->miscs[index].different_handles.InitializeAndCopy(persistent_allocator, same_target.miscs[index].other_handles);
		}

		const size_t ALLOCATOR_SIZE = ECS_MB * 50;
		const size_t ALLOCATOR_BACKUP = ECS_MB * 200;

		// Create the global allocators
		control_block->global_managers = (GlobalMemoryManager*)Allocate(persistent_allocator, sizeof(GlobalMemoryManager) * thread_count);
		for (unsigned int index = 0; index < thread_count; index++) {
			control_block->global_managers[index] = GlobalMemoryManager(ALLOCATOR_SIZE, ECS_KB * 8, ALLOCATOR_BACKUP);
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

	void LaunchPreloadTasks(LaunchPreloadTasksData* task_data) {
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

	void LoadAssetsImpl(
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

		AllocatorPolymorphic allocator = GetPersistentAllocator(database);
		PackedFile* allocated_packed_file = (PackedFile*)Allocate(allocator, sizeof(PackedFile));
		HashTableCopyWithIdentifiers(packed_file->lookup_table, allocated_packed_file->lookup_table, allocator);
		allocated_packed_file->file_handle = packed_file->file_handle;

		ControlBlockExtra extra;
		extra.packed_file = allocated_packed_file;
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

		AllocatorPolymorphic allocator = GetPersistentAllocator(database);

		// Copy the input multi packed file and its stream
		MultiPackedFile* allocated_multi_packed_file = (MultiPackedFile*)Allocate(allocator, sizeof(MultiPackedFile));
		*allocated_multi_packed_file = multi_packed_file->Copy(allocator);

		PackedFile* allocated_packed_files = (PackedFile*)Allocate(allocator, sizeof(PackedFile) * packed_files.size);
		for (size_t index = 0; index < packed_files.size; index++) {
			HashTableCopyWithIdentifiers(packed_files[index].lookup_table, allocated_packed_files[index].lookup_table, allocator);
			allocated_packed_files[index].file_handle = packed_files[index].file_handle;
		}

		ControlBlockExtra extra;
		extra.multi_packed_file = allocated_multi_packed_file;
		extra.multi_packed_inputs = { allocated_packed_files, packed_files.size };
		extra.dimension = CONTROL_BLOCK_MULTI_PACKED;

		LoadAssetsImpl(database, resource_manager, task_manager, load_info, &extra, &preload_functions);
	}

	// ------------------------------------------------------------------------------------------------------------

	void UnloadAssets(AssetDatabase* database, ResourceManager* resource_manager, Stream<wchar_t> mount_point, CapacityStream<MissingAsset>* missing_assets)
	{
		auto mesh_stream = database->mesh_metadata.ToStream();
		auto texture_stream = database->texture_metadata.ToStream();
		auto gpu_sampler_stream = database->gpu_sampler_metadata.ToStream();
		auto shader_stream = database->shader_metadata.ToStream();
		auto material_stream = database->material_asset.ToStream();
		auto misc_stream = database->misc_asset.ToStream();

		for (size_t index = 0; index < mesh_stream.size; index++) {
			auto* mesh = &mesh_stream[index].value;
			if (IsMeshFromMetadataLoaded(resource_manager, mesh, mount_point)) {
				DeallocateMeshFromMetadata(resource_manager, mesh, mount_point);
			}
		}

		for (size_t index = 0; index < texture_stream.size; index++) {
			auto* texture = &texture_stream[index].value;
			if (IsTextureFromMetadataLoaded(resource_manager, texture, mount_point)) {
				DeallocateTextureFromMetadata(resource_manager, texture, mount_point);
			}
		}

		for (size_t index = 0; index < gpu_sampler_stream.size; index++) {
			DeallocateSamplerFromMetadata(resource_manager, &gpu_sampler_stream[index].value);
		}

		for (size_t index = 0; index < shader_stream.size; index++) {
			auto* shader = &shader_stream[index].value;
			if (IsShaderFromMetadataLoaded(resource_manager, shader, mount_point)) {
				DeallocateShaderFromMetadata(resource_manager, shader, mount_point);
			}
		}

		for (size_t index = 0; index < material_stream.size; index++) {
			DeallocateMaterialFromMetadata(resource_manager, &material_stream[index].value, database, mount_point);
		}

		for (size_t index = 0; index < misc_stream.size; index++) {
			auto* misc = &misc_stream[index].value;
			if (IsMiscFromMetadataLoaded(resource_manager, misc, mount_point)) {
				DeallocateMiscAssetFromMetadata(resource_manager, misc, mount_point);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void LoadAssetFailure::ToString(const AssetDatabase* database, CapacityStream<char>& characters) const
	{
		// The name is always the first field
		const void* asset = database->GetAssetConst(handle, asset_type);
		Stream<char> name = *(Stream<char>*)asset;

		Stream<char> type_string = ConvertAssetTypeString(asset_type);
		const char* failure_string = nullptr;
		if (processing_failure) {
			failure_string = "LoadFailure: Could not process asset {#}, type {#}.";
		}
		else {
			if (dependency_failure) {
				failure_string = "LoadFailure: Asset {#}, type {#}, could not be loaded because a dependency failed.";
			}
			else {
				failure_string = "LoadFailure: Could not read data from disk for asset {#}, type {#}.";
			}
		}

		ECS_FORMAT_STRING(characters, failure_string, name, type_string);
	}

	// ------------------------------------------------------------------------------------------------------------

}