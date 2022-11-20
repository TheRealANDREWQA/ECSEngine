#include "ecspch.h"
#include "AssetDatabaseReference.h"
#include "AssetDatabase.h"

#include "../../Allocators/ResizableLinearAllocator.h"
#include "../../Utilities/File.h"
#include "../../Utilities/Reflection/Reflection.h"
#include "../../Utilities/Serialization/Binary/Serialization.h"
#include "../../Utilities/Serialization/SerializationHelpers.h"
#include "AssetMetadataSerialize.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------

	AssetDatabaseReference::AssetDatabaseReference() : database(nullptr) {}

	// ------------------------------------------------------------------------------------------------

	AssetDatabaseReference::AssetDatabaseReference(AssetDatabase* database, AllocatorPolymorphic allocator) : database(database)
	{
		mesh_metadata.Initialize(allocator, 0);
		texture_metadata.Initialize(allocator, 0);
		gpu_sampler_metadata.Initialize(allocator, 0);
		shader_metadata.Initialize(allocator, 0);
		material_asset.Initialize(allocator, 0);
		misc_asset.Initialize(allocator, 0);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddMesh(unsigned int handle, bool increment_count)
	{
		AddAsset(handle, ECS_ASSET_MESH, increment_count);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddTexture(unsigned int handle, bool increment_count)
	{
		AddAsset(handle, ECS_ASSET_TEXTURE, increment_count);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddGPUSampler(unsigned int handle, bool increment_count)
	{
		AddAsset(handle, ECS_ASSET_GPU_SAMPLER, increment_count);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddShader(unsigned int handle, bool increment_count)
	{
		AddAsset(handle, ECS_ASSET_SHADER, increment_count);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddMaterial(unsigned int handle, bool increment_count)
	{
		AddAsset(handle, ECS_ASSET_MATERIAL, increment_count);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddMisc(unsigned int handle, bool increment_count)
	{
		AddAsset(handle, ECS_ASSET_MISC, increment_count);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddAsset(unsigned int handle, ECS_ASSET_TYPE type, bool increment_count)
	{
		ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
		streams[type].Add(handle);

		if (increment_count) {
			database->AddAsset(handle, type);
		}
	}

	// ------------------------------------------------------------------------------------------------

	unsigned int AssetDatabaseReference::AddAsset(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, bool* loaded_now)
	{
		unsigned int handle = database->AddAsset(name, file, type, loaded_now);
		if (handle != -1) {
			AddAsset(handle, type);
		}
		return handle;
	}

	// ------------------------------------------------------------------------------------------------

	AssetDatabaseReference AssetDatabaseReference::Copy(AllocatorPolymorphic allocator) const
	{
		AssetDatabaseReference copy;
		
		copy.mesh_metadata.InitializeAndCopy(allocator, mesh_metadata);
		copy.texture_metadata.InitializeAndCopy(allocator, texture_metadata);
		copy.gpu_sampler_metadata.InitializeAndCopy(allocator, gpu_sampler_metadata);
		copy.shader_metadata.InitializeAndCopy(allocator, shader_metadata);
		copy.material_asset.InitializeAndCopy(allocator, material_asset);
		copy.misc_asset.InitializeAndCopy(allocator, misc_asset);
		copy.database = database;

		return copy;
	}

	// ------------------------------------------------------------------------------------------------

	MeshMetadata* AssetDatabaseReference::GetMesh(unsigned int index)
	{
		return (MeshMetadata*)GetAsset(index, ECS_ASSET_MESH);
	}

	// ------------------------------------------------------------------------------------------------

	TextureMetadata* AssetDatabaseReference::GetTexture(unsigned int index)
	{
		return (TextureMetadata*)GetAsset(index, ECS_ASSET_TEXTURE);
	}

	// ------------------------------------------------------------------------------------------------

	GPUSamplerMetadata* AssetDatabaseReference::GetGPUSampler(unsigned int index)
	{
		return (GPUSamplerMetadata*)GetAsset(index, ECS_ASSET_GPU_SAMPLER);
	}

	// ------------------------------------------------------------------------------------------------

	ShaderMetadata* AssetDatabaseReference::GetShader(unsigned int index)
	{
		return (ShaderMetadata*)GetAsset(index, ECS_ASSET_SHADER);
	}

	// ------------------------------------------------------------------------------------------------

	MaterialAsset* AssetDatabaseReference::GetMaterial(unsigned int index)
	{
		return (MaterialAsset*)GetAsset(index, ECS_ASSET_MATERIAL);
	}

	// ------------------------------------------------------------------------------------------------

	MiscAsset* AssetDatabaseReference::GetMisc(unsigned int index)
	{
		return (MiscAsset*)GetAsset(index, ECS_ASSET_MISC);
	}

	// ------------------------------------------------------------------------------------------------

	void* AssetDatabaseReference::GetAsset(unsigned int index, ECS_ASSET_TYPE type)
	{
		return database->GetAsset(GetHandle(index, type), type);
	}

	// ------------------------------------------------------------------------------------------------

	unsigned int AssetDatabaseReference::GetHandle(unsigned int index, ECS_ASSET_TYPE type) const
	{
		ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
		return streams[type][index];
	}

	// ------------------------------------------------------------------------------------------------

	unsigned int AssetDatabaseReference::GetIndex(unsigned int handle, ECS_ASSET_TYPE type) const
	{
		ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
		return function::SearchBytes(streams[type].buffer, streams[type].size, handle, sizeof(handle));
	}

	// ------------------------------------------------------------------------------------------------

	AssetDatabase* AssetDatabaseReference::GetDatabase() const
	{
		return database;
	}

	// ------------------------------------------------------------------------------------------------

	unsigned int AssetDatabaseReference::GetCount(ECS_ASSET_TYPE type) const
	{
		ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
		return streams[type].size;
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveMesh(unsigned int index, MeshMetadata* storage)
	{
		return RemoveAsset(index, ECS_ASSET_MESH);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveTexture(unsigned int index, TextureMetadata* storage)
	{
		return RemoveAsset(index, ECS_ASSET_TEXTURE);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveGPUSampler(unsigned int index, GPUSamplerMetadata* storage)
	{
		return RemoveAsset(index, ECS_ASSET_GPU_SAMPLER);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveShader(unsigned int index, ShaderMetadata* storage)
	{
		return RemoveAsset(index, ECS_ASSET_SHADER);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveMaterial(unsigned int index, MaterialAsset* storage)
	{
		return RemoveAsset(index, ECS_ASSET_MATERIAL);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveMisc(unsigned int index, MiscAsset* storage)
	{
		return RemoveAsset(index, ECS_ASSET_MISC);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveAsset(unsigned int index, ECS_ASSET_TYPE type, void* storage)
	{
		unsigned int handle = GetHandle(index, type);
		RemoveAssetThisOnly(index, type);
		return database->RemoveAsset(handle, type, storage);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::RemoveAssetThisOnly(unsigned int index, ECS_ASSET_TYPE type)
	{
		ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
		streams[type].RemoveSwapBack(index);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::Reset()
	{
		mesh_metadata.FreeBuffer();
		texture_metadata.FreeBuffer();
		gpu_sampler_metadata.FreeBuffer();
		shader_metadata.FreeBuffer();
		material_asset.FreeBuffer();
		misc_asset.FreeBuffer();
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::IncrementReferenceCounts()
	{
		ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)index;

			for (unsigned int stream_index = 0; stream_index < streams[index].size; stream_index++) {
				unsigned int handle = GetHandle(stream_index, asset_type);
				database->AddAsset(handle, asset_type);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::ToStandalone(AllocatorPolymorphic allocator, AssetDatabase* out_database) const {
		out_database->SetAllocator(allocator);

		out_database->reflection_manager = database->reflection_manager;
		out_database->SetFileLocation({ nullptr, 0 });
		out_database->mesh_metadata.ResizeNoCopy(mesh_metadata.size);
		out_database->texture_metadata.ResizeNoCopy(texture_metadata.size);
		out_database->gpu_sampler_metadata.ResizeNoCopy(gpu_sampler_metadata.size);
		out_database->shader_metadata.ResizeNoCopy(shader_metadata.size);
		out_database->material_asset.ResizeNoCopy(material_asset.size);
		out_database->misc_asset.ResizeNoCopy(misc_asset.size);

		auto set_value = [&](auto stream, ECS_ASSET_TYPE type) {
			if (stream.size > 0) {
				// Create a temporary copy of the stream and eliminate the duplicates
				ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, stream_copy, stream.size);
				ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, reference_count, stream.size);
				stream_copy.Copy(stream);

				// If the stream copy has size of 1, we still need to initialize the first reference count to 1
				bool had_duplicates = false;
				for (unsigned int index = 0; index < stream_copy.size - 1; index++) {
					had_duplicates = false;
					reference_count[index] = 1;
					unsigned int offset = index + 1;
					unsigned int current_find = function::SearchBytes(stream_copy.buffer + offset, stream_copy.size - offset, stream_copy[index], sizeof(stream_copy[index]));
					while (current_find != -1) {
						had_duplicates = true;
						reference_count[index]++;
						stream_copy.RemoveSwapBack(current_find + offset);
						offset += current_find;
						current_find = function::SearchBytes(stream_copy.buffer + offset, stream_copy.size - offset, stream_copy[index], sizeof(stream_copy[index]));
					}
				}
				// If the last value didn't have duplicates, then we need to initialize its reference count to 1
				// since it will get skipped
				if (!had_duplicates) {
					reference_count[stream_copy.size - 1] = 1;
				}

				for (unsigned int index = 0; index < stream_copy.size; index++) {
					out_database->AddAssetInternal(database->GetAsset(stream_copy[index], type), type, reference_count[index]);
				}
			}
		};

		set_value(mesh_metadata, ECS_ASSET_MESH);
		set_value(texture_metadata, ECS_ASSET_TEXTURE);
		set_value(gpu_sampler_metadata, ECS_ASSET_GPU_SAMPLER);
		set_value(shader_metadata, ECS_ASSET_SHADER);
		set_value(material_asset, ECS_ASSET_MATERIAL);
		set_value(misc_asset, ECS_ASSET_MISC);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::FromStandalone(const AssetDatabase* standalone_database, AssetDatabaseReferenceFromStandaloneOptions options) {
		mesh_metadata.ResizeNoCopy(standalone_database->mesh_metadata.set.size);
		texture_metadata.ResizeNoCopy(standalone_database->texture_metadata.set.size);
		gpu_sampler_metadata.ResizeNoCopy(standalone_database->gpu_sampler_metadata.set.size);
		shader_metadata.ResizeNoCopy(standalone_database->shader_metadata.set.size);
		material_asset.ResizeNoCopy(standalone_database->material_asset.set.size);
		misc_asset.ResizeNoCopy(standalone_database->misc_asset.set.size);

		auto set_values = [&](const auto& standalone_metadata, auto& reference_metadata, ECS_ASSET_TYPE asset_type) {
			auto stream = standalone_metadata.set.ToStream();
			for (size_t index = 0; index < stream.size; index++) {
				unsigned int original_handle = standalone_metadata.GetHandleFromIndex(index);
				unsigned int reference_count = standalone_metadata[original_handle].reference_count;

				Stream<wchar_t> file = GetAssetFile(&stream[index].value, asset_type);
				reference_metadata.Add(database->AddAsset(stream[index].value.name, file, asset_type));

				unsigned int last_index = reference_metadata.size - 1;
				unsigned int added_handle = reference_metadata[last_index];

				// If it is the first time it is added, then also copy the pointer
				if (database->GetReferenceCount(reference_metadata[last_index], asset_type) == 1) {
					// Copy the pointer
					Stream<void> standalone_asset = GetAssetFromMetadata(&stream[index].value, asset_type);
					SetAssetToMetadata(database->GetAsset(added_handle, asset_type), asset_type, standalone_asset);
				}

				// There must in total reference_count values of the previous handle in order to preserve the reference count
				// from this asset database reference
				for (unsigned int subindex = 1; subindex < reference_count; subindex++) {
					AddAsset(added_handle, asset_type, true);
				}

				if (options.handle_remapping != nullptr) {
					// Add the remapping even when the values are identical
					options.handle_remapping[asset_type].AddSafe({ original_handle, reference_metadata[last_index] });
				}
			}
		};

		set_values(standalone_database->mesh_metadata, mesh_metadata, ECS_ASSET_MESH);
		set_values(standalone_database->texture_metadata, texture_metadata, ECS_ASSET_TEXTURE);
		set_values(standalone_database->gpu_sampler_metadata, gpu_sampler_metadata, ECS_ASSET_GPU_SAMPLER);
		set_values(standalone_database->shader_metadata, shader_metadata, ECS_ASSET_SHADER);
		set_values(standalone_database->material_asset, material_asset, ECS_ASSET_MATERIAL);
		set_values(standalone_database->misc_asset, misc_asset, ECS_ASSET_MISC);

		// If it has pointer remapping, iterate the master database and report the changed values
		if (options.pointer_remapping != nullptr) {
			auto pointer_remap = [&](const auto& standalone_metadata, ECS_ASSET_TYPE asset_type) {
				auto stream = standalone_metadata.ToStream();
				// Create a mask of indices which were not yet taken
				// And then remove it when an asset is found to have that index
				ECS_STACK_CAPACITY_STREAM(unsigned int, free_indices, ECS_ASSET_RANDOMIZED_ASSET_LIMIT);
				free_indices.size = ECS_ASSET_RANDOMIZED_ASSET_LIMIT;
				function::MakeSequence(free_indices, 1);

				for (size_t index = 0; index < stream.size; index++) {
					unsigned int current_handle = database->GetAssetHandleFromIndex(index, asset_type);
					void* asset = database->GetAsset(current_handle, asset_type);
					Stream<void> asset_pointer = GetAssetFromMetadata(asset, asset_type);

					unsigned int old_randomized_index = ExtractRandomizedAssetValue(asset_pointer.buffer, asset_type);
					
					if (old_randomized_index <= ECS_ASSET_RANDOMIZED_ASSET_LIMIT) {
						unsigned int found_index = function::SearchBytes(free_indices.buffer, free_indices.size, old_randomized_index, sizeof(old_randomized_index));

						// The index has not yet been found
						if (found_index != -1) {
							free_indices.RemoveSwapBack(found_index);
						}
						else {
							ECS_ASSERT(free_indices.size > 0);

							// The index is already in use
							// Choose the last index to be the new index
							free_indices.size--;
							unsigned int new_index = free_indices[free_indices.size];
							options.pointer_remapping[asset_type].AddSafe({ old_randomized_index, new_index, current_handle });

							// Change the mapping for this asset
							SetRandomizedAssetToMetadata(asset, asset_type, new_index);
						}
					}
				}
				
			};

			pointer_remap(database->mesh_metadata, ECS_ASSET_MESH);
			pointer_remap(database->texture_metadata, ECS_ASSET_TEXTURE);
			pointer_remap(database->gpu_sampler_metadata, ECS_ASSET_GPU_SAMPLER);
			pointer_remap(database->shader_metadata, ECS_ASSET_SHADER);
			pointer_remap(database->material_asset, ECS_ASSET_MATERIAL);
			pointer_remap(database->misc_asset, ECS_ASSET_MISC);
		}
	}

	// ------------------------------------------------------------------------------------------------

	template<typename ReturnType, typename Functor>
	ReturnType SerializeStandalone(
		const AssetDatabaseReference* reference,
		const Reflection::ReflectionManager* reflection_manager, 
		Functor&& functor
	) {
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(temp_allocator, ECS_KB * 256, ECS_MB);
		AssetDatabase temp_database;

		temp_database.reflection_manager = reflection_manager;
		AllocatorPolymorphic allocator_polymorphic = GetAllocatorPolymorphic(&temp_allocator);
		reference->ToStandalone(allocator_polymorphic, &temp_database);

		ReturnType return_value = functor(&temp_database);

		temp_allocator.ClearBackup();
		return return_value;
	}

	bool AssetDatabaseReference::SerializeStandalone(const Reflection::ReflectionManager* reflection_manager, Stream<wchar_t> file) const {
		return ECSEngine::SerializeStandalone<bool>(this, reflection_manager, [&](const AssetDatabase* out_database) {
			return SerializeAssetDatabase(out_database, file) == ECS_SERIALIZE_OK;
		});
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::SerializeStandalone(const Reflection::ReflectionManager* reflection_manager, uintptr_t& ptr) const {
		return ECSEngine::SerializeStandalone<bool>(this, reflection_manager, [&](const AssetDatabase* out_database) {
			return SerializeAssetDatabase(out_database, ptr) == ECS_SERIALIZE_OK;
		});
	}

	// ------------------------------------------------------------------------------------------------

	Stream<void> AssetDatabaseReference::SerializeStandalone(const Reflection::ReflectionManager* reflection_manager, AllocatorPolymorphic allocator) const {
		return ECSEngine::SerializeStandalone<Stream<void>>(this, reflection_manager, [&](const AssetDatabase* out_database) {
			size_t serialize_size = SerializeAssetDatabaseSize(out_database);
			if (serialize_size == -1) {
				return Stream<void>(nullptr, 0);
			}

			void* allocation = AllocateEx(allocator, serialize_size);
			uintptr_t ptr = (uintptr_t)allocation;
			ECS_SERIALIZE_CODE serialize_code = SerializeAssetDatabase(out_database, ptr);
			if (serialize_code == ECS_SERIALIZE_OK) {
				return Stream<void>(allocation, serialize_size);
			}
			else {
				// Deallocate the buffer and return nullptr
				DeallocateEx(allocator, allocation);
				return Stream<void>(nullptr, 0);
			}
		});
	}

	// ------------------------------------------------------------------------------------------------

	size_t AssetDatabaseReference::SerializeStandaloneSize(const Reflection::ReflectionManager* reflection_manager) const {
		return ECSEngine::SerializeStandalone<size_t>(this, reflection_manager, [&](const AssetDatabase* out_database) {
			return SerializeAssetDatabaseSize(out_database);
		});
	}

	// ------------------------------------------------------------------------------------------------

	template<typename Functor>
	size_t DeserializeStandalone(const Reflection::ReflectionManager* reflection_manager, Functor&& functor) {
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(temp_allocator, ECS_KB * 64, ECS_MB * 4);
		AssetDatabase temp_database;
		temp_database.reflection_manager = reflection_manager;

		AllocatorPolymorphic allocator_polymorphic = GetAllocatorPolymorphic(&temp_allocator);
		temp_database.SetAllocator(allocator_polymorphic);

		size_t return_value = functor(&temp_database);

		temp_allocator.ClearBackup();
		return return_value;
	}

	bool AssetDatabaseReference::DeserializeStandalone(const Reflection::ReflectionManager* reflection_manager, Stream<wchar_t> file, AssetDatabaseReferenceFromStandaloneOptions options) {
		return (bool)ECSEngine::DeserializeStandalone(reflection_manager, [&](AssetDatabase* database) {
			bool success = DeserializeAssetDatabase(database, file) == ECS_DESERIALIZE_OK;
			if (success) {
				FromStandalone(database, options);
			}
			return success;
		});
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::DeserializeStandalone(const Reflection::ReflectionManager* reflection_manager, uintptr_t& ptr, AssetDatabaseReferenceFromStandaloneOptions options) {
		return (bool)ECSEngine::DeserializeStandalone(reflection_manager, [&](AssetDatabase* database) {
			bool success = DeserializeAssetDatabase(database, ptr) == ECS_DESERIALIZE_OK;
			if (success) {
				FromStandalone(database, options);
			}
			return success;
		});
	}

	// ------------------------------------------------------------------------------------------------

	size_t AssetDatabaseReference::DeserializeSize(const Reflection::ReflectionManager* reflection_manager, uintptr_t ptr) {
		return ECSEngine::DeserializeStandalone(reflection_manager, [&](AssetDatabase* database) {
			return DeserializeAssetDatabaseSize(reflection_manager, ptr);
		});
	}

	// ------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------

}
