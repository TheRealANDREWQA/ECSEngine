#include "ecspch.h"
#include "AssetDatabase.h"
#include "../Utilities/File.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Utilities/Reflection/ReflectionCustomTypes.h"
#include "../Utilities/Path.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Utilities/ForEachFiles.h"
#include "../Utilities/StreamUtilities.h"
#include "../Utilities/BufferedFileReaderWriter.h"

#include "AssetMetadataSerialize.h"

#define PATH_SEPARATOR L"___"

#define SEPARATOR_CHAR '~'
#define COLON_CHAR_REPLACEMENT '`'

namespace ECSEngine {

	// --------------------------------------------------------------------------------------

	void AssetDatabaseSnapshot::Deallocate(AllocatorPolymorphic allocator)
	{
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			if (stream_sizes[index] > 0) {
				ECSEngine::DeallocateEx(allocator, reference_counts[index]);
			}
		}
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabaseFileDirectory(Stream<wchar_t> file_location, CapacityStream<wchar_t>& path, ECS_ASSET_TYPE type)
	{
		path.CopyOther(file_location);
		ConvertASCIIToWide(path, Stream<char>(ConvertAssetTypeString(type)));
		path.Add(ECS_OS_PATH_SEPARATOR);
	}

	// --------------------------------------------------------------------------------------

	AssetDatabase::AssetDatabase(AllocatorPolymorphic allocator,  const Reflection::ReflectionManager* _reflection_manager)
		: reflection_manager(_reflection_manager), metadata_file_location(nullptr, 0)
	{
		SetAllocator(allocator);
	}
	
	// --------------------------------------------------------------------------------------

	template<typename AssetType, typename AssetSet>
	unsigned int AddAssetDefault(AssetDatabase* database, AssetSet& set, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, bool* loaded_now) {
		unsigned int handle = database->FindAsset(name, file, type);

		if (handle == -1) {
			AssetType metadata;

			if (loaded_now != nullptr) {
				*loaded_now = true;
			}

			name = StringCopy(database->Allocator(), name);
			if (file.size > 0) {
				file = StringCopy(database->Allocator(), file);
			}
			metadata.Default(name, file);

			if (database->metadata_file_location.size > 0) {
				ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
				database->FileLocationAsset(name, file, path, type);

				if (ExistsFileOrFolder(path)) {
					bool success = database->ReadAssetFile(name, file, &metadata, type);
					if (success) {
						return set.Add({ metadata, 1 });
					}
					else {
						// Also deallocate the name
						Deallocate(database->Allocator(), name.buffer);
						if (file.size > 0) {
							Deallocate(database->Allocator(), file.buffer);
						}
						return -1;
					}
				}
				else {
					database->WriteAssetFile(&metadata, type);
					return set.Add({ metadata, 1 });
				}
			}
			else {
				database->WriteAssetFile(&metadata, type);
				return set.Add({ metadata, 1 });
			}
		}
		else {
			if (loaded_now != nullptr) {
				*loaded_now = false;
			}

			set[handle].reference_count++;
			return handle;
		}
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMesh(Stream<char> name, Stream<wchar_t> file, bool* loaded_now)
	{
		return AddAssetDefault<MeshMetadata>(this, mesh_metadata, name, file, ECS_ASSET_MESH, loaded_now);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMeshInternal(const MeshMetadata* metadata, unsigned int reference_count)
	{
		return mesh_metadata.Add({ metadata->Copy(Allocator()), reference_count });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddTexture(Stream<char> name, Stream<wchar_t> file, bool* loaded_now)
	{
		return AddAssetDefault<TextureMetadata>(this, texture_metadata, name, file, ECS_ASSET_TEXTURE, loaded_now);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddTextureInternal(const TextureMetadata* metadata, unsigned int reference_count)
	{
		return texture_metadata.Add({ metadata->Copy(Allocator()), reference_count });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddGPUSampler(Stream<char> name, bool* loaded_now)
	{
		return AddAssetDefault<GPUSamplerMetadata>(this, gpu_sampler_metadata, name, {}, ECS_ASSET_GPU_SAMPLER, loaded_now);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddGPUSamplerInternal(const GPUSamplerMetadata* metadata, unsigned int reference_count)
	{
		return gpu_sampler_metadata.Add({ metadata->Copy(Allocator()), reference_count });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddShader(Stream<char> name, Stream<wchar_t> file, bool* loaded_now)
	{
		return AddAssetDefault<ShaderMetadata>(this, shader_metadata, name, file, ECS_ASSET_SHADER, loaded_now);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddShaderInternal(const ShaderMetadata* metadata, unsigned int reference_count)
	{
		return shader_metadata.Add({ metadata->Copy(Allocator()), reference_count });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMaterial(Stream<char> name, bool* loaded_now)
	{
		return AddAssetDefault<MaterialAsset>(this, material_asset, name, {}, ECS_ASSET_MATERIAL, loaded_now);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMaterialInternal(const MaterialAsset* metadata, unsigned int reference_count)
	{
		return material_asset.Add({ metadata->Copy(Allocator()), reference_count });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMisc(Stream<char> name, Stream<wchar_t> file, bool* loaded_now)
	{
		return AddAssetDefault<MiscAsset>(this, misc_asset, name, file, ECS_ASSET_MISC, loaded_now);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMiscInternal(const MiscAsset* metadata, unsigned int reference_count)
	{
		return misc_asset.Add({ metadata->Copy(Allocator()), reference_count });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddAsset(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, bool* loaded_now)
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return AddMesh(name, file, loaded_now);
		case ECS_ASSET_TEXTURE:
			return AddTexture(name, file, loaded_now);
		case ECS_ASSET_GPU_SAMPLER:
			return AddGPUSampler(name, loaded_now);
		case ECS_ASSET_SHADER:
			return AddShader(name, file, loaded_now);
		case ECS_ASSET_MATERIAL:
			return AddMaterial(name, loaded_now);
		case ECS_ASSET_MISC:
			return AddMisc(name, file, loaded_now);
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		return -1;
	}

	// --------------------------------------------------------------------------------------

	typedef unsigned int (AssetDatabase::* AddAssetInternalFunction)(const void* buffer, unsigned int reference_count);

	AddAssetInternalFunction ADD_ASSET_INTERNAL_FUNCTION[] = {
		(AddAssetInternalFunction)&AssetDatabase::AddMeshInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddTextureInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddGPUSamplerInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddShaderInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddMaterialInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddMiscInternal
	};

	unsigned int AssetDatabase::AddAssetInternal(const void* asset, ECS_ASSET_TYPE type, unsigned int reference_count)
	{
		return (this->*ADD_ASSET_INTERNAL_FUNCTION[type])(asset, reference_count);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddFindAssetInternal(const void* asset, ECS_ASSET_TYPE type, unsigned int reference_count)
	{
		unsigned int existing_handle = FindAsset(ECSEngine::GetAssetName(asset, type), ECSEngine::GetAssetFile(asset, type), type);
		if (existing_handle != -1) {
			AddAsset(existing_handle, type, reference_count);
			return existing_handle;
		}
		return AddAssetInternal(asset, type, reference_count);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::AddAsset(unsigned int handle, ECS_ASSET_TYPE type, unsigned int reference_count)
	{
		switch (type) {
		case ECS_ASSET_MESH:
			mesh_metadata[handle].reference_count += reference_count;
			break;
		case ECS_ASSET_TEXTURE:
			texture_metadata[handle].reference_count += reference_count;
			break;
		case ECS_ASSET_GPU_SAMPLER:
			gpu_sampler_metadata[handle].reference_count += reference_count;
			break;
		case ECS_ASSET_SHADER:
			shader_metadata[handle].reference_count += reference_count;
			break;
		case ECS_ASSET_MATERIAL:
			material_asset[handle].reference_count += reference_count;
			break;
		case ECS_ASSET_MISC:
			misc_asset[handle].reference_count += reference_count;
			break;
		}
	}

	// --------------------------------------------------------------------------------------

	template<typename StreamType>
	AssetDatabase CopyImpl(const AssetDatabase* database, StreamType* handle_mask, AllocatorPolymorphic allocator) {
		AssetDatabase result;
		result.SetAllocator(allocator);

		auto copy_stream = [&](auto& sparse_set, ECS_ASSET_TYPE type) {
			auto stream = handle_mask[type];
			sparse_set.ResizeNoCopy(stream.size);

			for (size_t index = 0; index < stream.size; index++) {
				unsigned int standalone_reference_count = database->GetReferenceCountStandalone(stream[index], type);
				result.AddFindAssetInternal(database->GetAssetConst(stream[index], type), type, standalone_reference_count);
			}
		};

		copy_stream(result.mesh_metadata, ECS_ASSET_MESH);
		copy_stream(result.texture_metadata, ECS_ASSET_TEXTURE);
		copy_stream(result.gpu_sampler_metadata, ECS_ASSET_GPU_SAMPLER);
		copy_stream(result.shader_metadata, ECS_ASSET_SHADER);
		copy_stream(result.material_asset, ECS_ASSET_MATERIAL);
		copy_stream(result.misc_asset, ECS_ASSET_MISC);

		// Now we need for all assets that have dependencies to include their dependencies here
		// and change the mapping
		for (size_t index = 0; index < ECS_COUNTOF(ECS_ASSET_TYPES_WITH_DEPENDENCIES); index++) {
			ECS_ASSET_TYPE current_type = ECS_ASSET_TYPES_WITH_DEPENDENCIES[index];
			result.ForEachAsset(current_type, [&](unsigned int handle) {
				void* asset = result.GetAsset(handle, current_type);
				ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
				GetAssetDependencies(asset, current_type, &dependencies);
				for (unsigned int subindex = 0; subindex < dependencies.size; subindex++) {
					const void* dependency = database->GetAssetConst(dependencies[subindex].handle, dependencies[subindex].type);
					unsigned int new_handle = result.AddFindAssetInternal(dependency, dependencies[subindex].type);
					dependencies[subindex].handle = new_handle;
				}
				RemapAssetDependencies(asset, current_type, dependencies);
			});
		}

		return result;
	}

	AssetDatabase AssetDatabase::Copy(Stream<unsigned int>* handle_mask, AllocatorPolymorphic allocator) const {
		return CopyImpl(this, handle_mask, allocator);
	}

	// --------------------------------------------------------------------------------------

	AssetDatabase AssetDatabase::Copy(CapacityStream<unsigned int>* handle_mask, AllocatorPolymorphic allocator) const {
		return CopyImpl(this, handle_mask, allocator);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::CopyAssetPointers(const AssetDatabase* other, Stream<unsigned int>* asset_mask)
	{
		auto loop = [=](auto use_mask) {
			for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
				unsigned int asset_count;
				if constexpr (use_mask) {
					asset_count = asset_mask[current_type].size;
				}
				else {
					asset_count = other->GetAssetCount(current_type);
				}
				for (unsigned int subindex = 0; subindex < asset_count; subindex++) {
					unsigned int other_handle;
					if constexpr (use_mask) {
						other_handle = asset_mask[current_type][subindex];
					}
					else {
						other_handle = other->GetAssetHandleFromIndex(subindex, current_type);
					}

					const void* metadata = other->GetAssetConst(other_handle, current_type);

					unsigned int current_handle = FindAsset(ECSEngine::GetAssetName(metadata, current_type), GetAssetFile(metadata, current_type), current_type);
					if (current_handle != -1) {
						Stream<void> asset = GetAssetFromMetadata(metadata, current_type);
						SetAssetToMetadata(GetAsset(current_handle, current_type), current_type, asset);
					}
				}
			}
		};

		if (asset_mask != nullptr) {
			loop(std::true_type{});
		}
		else {
			loop(std::false_type{});
		}
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::CreateMetadataFolders() const
	{
		return CreateMetadataFolders(metadata_file_location);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::CreateMetadataFolders(Stream<wchar_t> file_location)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, path, 512);
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			AssetDatabaseFileDirectory(file_location, path, (ECS_ASSET_TYPE)index);
			if (!ExistsFileOrFolder(path)) {
				if (!CreateFolder(path)) {
					return false;
				}
			}
		}
		return true;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::DeallocateStreams()
	{
		mesh_metadata.FreeBuffer();
		texture_metadata.FreeBuffer();
		gpu_sampler_metadata.FreeBuffer();
		shader_metadata.FreeBuffer();
		material_asset.FreeBuffer();
		misc_asset.FreeBuffer();
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::DependsUpon(const void* main_asset, ECS_ASSET_TYPE type, const void* referenced_asset, ECS_ASSET_TYPE referenced_type) const
	{
		if (IsAssetTypeReferenceable(referenced_type)) {
			ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
			GetAssetDependencies(main_asset, type, &dependencies);

			if (dependencies.size > 0) {
				unsigned int handle = FindAssetEx(GetAssetFromMetadata(referenced_asset, referenced_type), referenced_type);
				if (handle != -1) {
					for (unsigned int index = 0; index < dependencies.size; index++) {
						if (dependencies[index].type == referenced_type && dependencies[index].handle) {
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::Exists(unsigned int handle, ECS_ASSET_TYPE type) const
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return mesh_metadata.set.Exists(handle);
		case ECS_ASSET_TEXTURE:
			return texture_metadata.set.Exists(handle);
		case ECS_ASSET_GPU_SAMPLER:
			return gpu_sampler_metadata.set.Exists(handle);
		case ECS_ASSET_SHADER:
			return shader_metadata.set.Exists(handle);
		case ECS_ASSET_MATERIAL:
			return material_asset.set.Exists(handle);
		case ECS_ASSET_MISC:
			return misc_asset.set.Exists(handle);
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		// Shouldn't be reached
		return false;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMesh(Stream<char> name, Stream<wchar_t> file) const
	{
		return mesh_metadata.FindFunctor([&](const ReferenceCounted<MeshMetadata>& compare) {
			return compare.value.name == name && compare.value.file == file;
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindTexture(Stream<char> name, Stream<wchar_t> file) const
	{
		return texture_metadata.FindFunctor([&](const ReferenceCounted<TextureMetadata>& compare) {
			return compare.value.name == name && compare.value.file == file;
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindGPUSampler(Stream<char> name) const
	{
		return gpu_sampler_metadata.FindFunctor([&](const ReferenceCounted<GPUSamplerMetadata>& compare) {
			return compare.value.name == name;
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindShader(Stream<char> name, Stream<wchar_t> file) const
	{
		return shader_metadata.FindFunctor([&](const ReferenceCounted<ShaderMetadata>& compare) {
			return compare.value.name == name && compare.value.file == file;
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMaterial(Stream<char> name) const
	{
		return material_asset.FindFunctor([&](const ReferenceCounted<MaterialAsset>& compare) {
			return compare.value.name == name;
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMisc(Stream<char> name, Stream<wchar_t> file) const
	{
		return misc_asset.FindFunctor([&](const ReferenceCounted<MiscAsset>& compare) {
			return compare.value.name == name && compare.value.file == file;
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindAsset(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type) const
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return FindMesh(name, file);
		case ECS_ASSET_TEXTURE:
			return FindTexture(name, file);
		case ECS_ASSET_GPU_SAMPLER:
			return FindGPUSampler(name);
		case ECS_ASSET_SHADER:
			return FindShader(name, file);
		case ECS_ASSET_MATERIAL:
			return FindMaterial(name);
		case ECS_ASSET_MISC:
			return FindMisc(name, file);
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
		return -1;
	}

	// If the compare size is greater than 0 than it will also do the byte compare (besides the pointer compare)
	template<typename SparseSet>
	unsigned int FindAssetExImplementation(const SparseSet* set, const void* pointer, size_t compare_size) {
		auto stream = set->ToStream();
		for (size_t index = 0; index < stream.size; index++) {
			const void* current_pointer = stream[index].value.Pointer();
			if (CompareAssetPointers(pointer, current_pointer, compare_size)) {
				return set->GetHandleFromIndex(index);
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMeshEx(const CoalescedMesh* mesh) const
	{
		return FindAssetExImplementation(&mesh_metadata, mesh, sizeof(CoalescedMesh));
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindTextureEx(ResourceView resource_view) const
	{
		return FindAssetExImplementation(&texture_metadata, resource_view.Interface(), 0);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindGPUSamplerEx(SamplerState sampler_state) const
	{
		return FindAssetExImplementation(&gpu_sampler_metadata, sampler_state.Interface(), 0);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindShaderEx(const void* shader_interface) const
	{
		return FindAssetExImplementation(&shader_metadata, shader_interface, 0);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMaterialEx(const Material* material) const
	{
		return FindAssetExImplementation(&material_asset, material, sizeof(Material));
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMiscEx(Stream<void> data) const
	{
		return FindAssetExImplementation(&misc_asset, data.buffer, 0);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindAssetEx(Stream<void> data, ECS_ASSET_TYPE type) const
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return FindMeshEx((const CoalescedMesh*)data.buffer);
		case ECS_ASSET_TEXTURE:
			return FindTextureEx((ID3D11ShaderResourceView*)data.buffer);
		case ECS_ASSET_GPU_SAMPLER:
			return FindGPUSamplerEx((ID3D11SamplerState*)data.buffer);
		case ECS_ASSET_SHADER:
			return FindShaderEx(data.buffer);
		case ECS_ASSET_MATERIAL:
			return FindMaterialEx((const Material*)data.buffer);
		case ECS_ASSET_MISC:
			return FindMiscEx(data);
		default:
			ECS_ASSERT(false, "Invalid asset type.");
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindAssetEx(const AssetDatabase* other, unsigned int other_handle, ECS_ASSET_TYPE type) const
	{
		const void* other_asset = other->GetAssetConst(other_handle, type);
		Stream<char> name = ECSEngine::GetAssetName(other_asset, type);
		Stream<wchar_t> file = GetAssetFile(other_asset, type);

		return FindAsset(name, file, type);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationMesh(Stream<char> name, Stream<wchar_t> file, CapacityStream<wchar_t>& path) const
	{
		FileLocationAsset(name, file, path, ECS_ASSET_MESH);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationTexture(Stream<char> name, Stream<wchar_t> file, CapacityStream<wchar_t>& path) const
	{
		FileLocationAsset(name, file, path, ECS_ASSET_TEXTURE);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationGPUSampler(Stream<char> name, CapacityStream<wchar_t>& path) const
	{
		FileLocationAsset(name, {}, path, ECS_ASSET_GPU_SAMPLER);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationShader(Stream<char> name, Stream<wchar_t> file, CapacityStream<wchar_t>& path) const
	{
		FileLocationAsset(name, file, path, ECS_ASSET_SHADER);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationMaterial(Stream<char> name, CapacityStream<wchar_t>& path) const
	{
		FileLocationAsset(name, {}, path, ECS_ASSET_MATERIAL);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationMisc(Stream<char> name, Stream<wchar_t> file, CapacityStream<wchar_t>& path) const
	{
		FileLocationAsset(name, file, path, ECS_ASSET_MISC);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationAsset(Stream<char> name, Stream<wchar_t> file, CapacityStream<wchar_t>& path, ECS_ASSET_TYPE type) const
	{
		AssetDatabaseFileDirectory(metadata_file_location, path, type);
		// Shaders are a special case - they have front facing thunk files in the editor side
		// That makes having the file to be specified very hard to work with - that's why use
		// The relative path of the thunk itself to deduce the location
		if (file.size > 0 && type != ECS_ASSET_SHADER) {
			ECS_STACK_CAPACITY_STREAM(wchar_t, temp_file, 512);
			// If the file is an absolute file, then we must replace the drive qualification
			// such that the OS doesn't think we try to use the current path of the given drive
			// so for C:\... replace it with a pair of separators C~`~ and the colon replaced since
			// it can make the OS fail
			if (PathIsAbsolute(file)) {
				temp_file.Add(file[0]);
				temp_file.Add(SEPARATOR_CHAR);
				temp_file.Add(COLON_CHAR_REPLACEMENT);
				temp_file.Add(SEPARATOR_CHAR);
				temp_file.AddStream({ file.buffer + 2, file.size - 2 });
			}
			else {
				temp_file.CopyOther(file);
			}

			// Place the file first separated from the name by 3 _
			// Also replace the '/' and '\\' from the file with '~'

			ReplaceCharacter(temp_file, ECS_OS_PATH_SEPARATOR_ASCII, SEPARATOR_CHAR);
			ReplaceCharacter(temp_file, ECS_OS_PATH_SEPARATOR_ASCII_REL, SEPARATOR_CHAR);
			path.AddStream(temp_file);
			path.AddStream(PATH_SEPARATOR);
		}

		// If the name contains relative path separators then replace these with the separator char
		unsigned int path_size = path.size;
		ConvertASCIIToWide(path, name);
		Stream<wchar_t> converted_name = { path.buffer + path_size, name.size };
		ReplaceCharacter(converted_name, ECS_OS_PATH_SEPARATOR_REL, SEPARATOR_CHAR);
		ReplaceCharacter(converted_name, ECS_OS_PATH_SEPARATOR, SEPARATOR_CHAR);
		path.AddStreamAssert(ECS_ASSET_DATABASE_FILE_EXTENSION);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FillAssetNames(ECS_ASSET_TYPE type, CapacityStream<Stream<char>>& names) const
	{
		// The name is the first member field of all types;
		auto iterate = [&names](auto sparse_set) {
			auto stream = sparse_set.ToStream();
			for (size_t index = 0; index < stream.size; index++) {
				Stream<char>* current_name = (Stream<char>*)&stream[index].value;
				names.Add(*current_name);
			}
		};

		switch (type) {
		case ECS_ASSET_MESH:
			iterate(mesh_metadata);
			break;
		case ECS_ASSET_TEXTURE:
			iterate(texture_metadata);
			break;
		case ECS_ASSET_GPU_SAMPLER:
			iterate(gpu_sampler_metadata);
			break;
		case ECS_ASSET_SHADER:
			iterate(shader_metadata);
			break;
		case ECS_ASSET_MATERIAL:
			iterate(material_asset);
			break;
		case ECS_ASSET_MISC:
			iterate(misc_asset);
			break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FillAssetPaths(ECS_ASSET_TYPE type, CapacityStream<Stream<wchar_t>>& paths) const
	{
		// The path is the second member field of all types that support it
		auto iterate = [&paths](auto sparse_set) {
			auto stream = sparse_set.ToStream();
			for (size_t index = 0; index < stream.size; index++) {
				Stream<wchar_t>* current_path = (Stream<wchar_t>*)OffsetPointer(&stream[index].value, sizeof(Stream<char>));
				paths.Add(*current_path);
			}
		};

		switch (type) {
		case ECS_ASSET_MESH:
			iterate(mesh_metadata);
			break;
		case ECS_ASSET_TEXTURE:
			iterate(texture_metadata);
			break;
		case ECS_ASSET_GPU_SAMPLER:
			break;
		case ECS_ASSET_SHADER:
			iterate(shader_metadata);
			break;
		case ECS_ASSET_MATERIAL:
			break;
		case ECS_ASSET_MISC:
			iterate(misc_asset);
			break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
	}

	// --------------------------------------------------------------------------------------

	MeshMetadata* AssetDatabase::GetMesh(unsigned int handle)
	{
		return (MeshMetadata*)GetMeshConst(handle);
	}

	// --------------------------------------------------------------------------------------

	const MeshMetadata* AssetDatabase::GetMeshConst(unsigned int handle) const
	{
		return &mesh_metadata[handle].value;
	}

	// --------------------------------------------------------------------------------------

	TextureMetadata* AssetDatabase::GetTexture(unsigned int handle)
	{
		return (TextureMetadata*)GetTextureConst(handle);
	}

	// --------------------------------------------------------------------------------------

	const TextureMetadata* AssetDatabase::GetTextureConst(unsigned int handle) const
	{
		return &texture_metadata[handle].value;
	}

	// --------------------------------------------------------------------------------------

	GPUSamplerMetadata* AssetDatabase::GetGPUSampler(unsigned int handle)
	{
		return (GPUSamplerMetadata*)GetGPUSamplerConst(handle);
	}

	// --------------------------------------------------------------------------------------

	const GPUSamplerMetadata* AssetDatabase::GetGPUSamplerConst(unsigned int handle) const
	{
		return &gpu_sampler_metadata[handle].value;
	}

	// --------------------------------------------------------------------------------------

	ShaderMetadata* AssetDatabase::GetShader(unsigned int handle)
	{
		return (ShaderMetadata*)GetShaderConst(handle);
	}

	// --------------------------------------------------------------------------------------

	const ShaderMetadata* AssetDatabase::GetShaderConst(unsigned int handle) const
	{
		return &shader_metadata[handle].value;
	}

	// --------------------------------------------------------------------------------------

	MaterialAsset* AssetDatabase::GetMaterial(unsigned int handle)
	{
		return (MaterialAsset*)GetMaterialConst(handle);
	}

	// --------------------------------------------------------------------------------------

	const MaterialAsset* AssetDatabase::GetMaterialConst(unsigned int handle) const
	{
		return &material_asset[handle].value;
	}

	// --------------------------------------------------------------------------------------

	MiscAsset* AssetDatabase::GetMisc(unsigned int handle)
	{
		return (MiscAsset*)GetMiscConst(handle);
	}

	// --------------------------------------------------------------------------------------

	const MiscAsset* AssetDatabase::GetMiscConst(unsigned int handle) const
	{
		return &misc_asset[handle].value;
	}

	// --------------------------------------------------------------------------------------

	void* AssetDatabase::GetAsset(unsigned int handle, ECS_ASSET_TYPE type)
	{
		return (void*)GetAssetConst(handle, type);
	}

	// --------------------------------------------------------------------------------------

	typedef const void* (AssetDatabase::* GetAssetFunction)(unsigned int handle) const;

	GetAssetFunction GET_FUNCTIONS[] = {
		(GetAssetFunction)&AssetDatabase::GetMeshConst,
		(GetAssetFunction)&AssetDatabase::GetTextureConst,
		(GetAssetFunction)&AssetDatabase::GetGPUSamplerConst,
		(GetAssetFunction)&AssetDatabase::GetShaderConst,
		(GetAssetFunction)&AssetDatabase::GetMaterialConst,
		(GetAssetFunction)&AssetDatabase::GetMiscConst
	};

	const void* AssetDatabase::GetAssetConst(unsigned int handle, ECS_ASSET_TYPE type) const
	{
		return (this->*GET_FUNCTIONS[type])(handle);
	}

	// --------------------------------------------------------------------------------------

	Stream<char> AssetDatabase::GetAssetName(unsigned int handle, ECS_ASSET_TYPE type) const {
		return ECSEngine::GetAssetName(GetAssetConst(handle, type), type);
	}

	// --------------------------------------------------------------------------------------

	// For samplers and materials which don't have a file it returns { nullptr, 0 }
	Stream<wchar_t> AssetDatabase::GetAssetPath(unsigned int handle, ECS_ASSET_TYPE type) const {
		return GetAssetFile(GetAssetConst(handle, type), type);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::GetMetadatasForFile(Stream<wchar_t> file, ECS_ASSET_TYPE type, CapacityStream<unsigned int>& handles) const
	{
		ResizableSparseSet<ReferenceCounted<MeshMetadata>>* sparse_sets = (ResizableSparseSet<ReferenceCounted<MeshMetadata>>*)this;
		unsigned int count = sparse_sets[type].set.size;

		for (unsigned int index = 0; index < count; index++) {
			unsigned int handle = sparse_sets[type].GetHandleFromIndex(index);
			Stream<wchar_t> current_file = GetAssetPath(handle, type);
			if (file == current_file) {
				handles.AddAssert(handle);
			}
		}
	}

	// --------------------------------------------------------------------------------------

	Stream<Stream<char>> AssetDatabase::GetMetadatasForFile(Stream<wchar_t> file, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator) const
	{
		// Start with a decent size in order to avoid too many small allocations
		ResizableStream<Stream<char>> stream(allocator, 64);

		ECS_STACK_CAPACITY_STREAM(wchar_t, folder, 512);
		AssetDatabaseFileDirectory(metadata_file_location, folder, type);

		struct FunctorData {
			ResizableStream<Stream<char>>* stream;
			Stream<wchar_t> file;
		};

		auto functor = [](Stream<wchar_t> file, void* _data) {
			FunctorData* data = (FunctorData*)_data;

			ECS_STACK_CAPACITY_STREAM(wchar_t, asset_file, 512);
			AssetDatabase::ExtractFileFromFile(file, asset_file);
			if (data->file == asset_file) {
				ECS_STACK_CAPACITY_STREAM(char, asset_name, 512);
				AssetDatabase::ExtractNameFromFile(file, asset_name);
				data->stream->Add(StringCopy(data->stream->allocator, asset_name));
			}

			return true;
		};

		FunctorData data = { &stream, file };
		ForEachFileInDirectory(folder, &data, functor);
		return stream;
	}

	// --------------------------------------------------------------------------------------

	Stream<Stream<wchar_t>> AssetDatabase::GetMetadatasForType(ECS_ASSET_TYPE type, AllocatorPolymorphic allocator) const
	{
		// Start with a decent size in order to avoid too many small allocations
		ResizableStream<Stream<wchar_t>> stream(allocator, 64);

		ECS_STACK_CAPACITY_STREAM(wchar_t, folder, 512);
		AssetDatabaseFileDirectory(metadata_file_location, folder, type);

		struct FunctorData {
			ResizableStream<Stream<wchar_t>>* stream;
		};

		auto functor = [](Stream<wchar_t> file, void* _data) {
			FunctorData* data = (FunctorData*)_data;

			Stream<wchar_t> filename = PathFilename(file);
			data->stream->Add(StringCopy(data->stream->allocator, filename));
			return true;
		};

		FunctorData data = { &stream };
		ForEachFileInDirectory(folder, &data, functor);
		return stream;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::GetAssetCount(ECS_ASSET_TYPE type) const
	{
		ResizableSparseSet<char>* sparse_sets = (ResizableSparseSet<char>*)this;
		return sparse_sets[type].set.size;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::GetMaxAssetCount() const
	{
		unsigned int max_value = 0;

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			max_value = max(max_value, GetAssetCount((ECS_ASSET_TYPE)index));
		}

		return max_value;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::GetAssetHandleFromIndex(unsigned int index, ECS_ASSET_TYPE type) const
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return mesh_metadata.GetHandleFromIndex(index);
		case ECS_ASSET_TEXTURE:
			return texture_metadata.GetHandleFromIndex(index);
		case ECS_ASSET_GPU_SAMPLER:
			return gpu_sampler_metadata.GetHandleFromIndex(index);
		case ECS_ASSET_SHADER:
			return shader_metadata.GetHandleFromIndex(index);
		case ECS_ASSET_MATERIAL:
			return material_asset.GetHandleFromIndex(index);
		case ECS_ASSET_MISC:
			return misc_asset.GetHandleFromIndex(index);
		}

		ECS_ASSERT(false, "Invalid asset type");
		return -1;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::GetIndexFromAssetHandle(unsigned int handle, ECS_ASSET_TYPE type) const
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return mesh_metadata.GetIndexFromHandle(handle);
		case ECS_ASSET_TEXTURE:
			return texture_metadata.GetIndexFromHandle(handle);
		case ECS_ASSET_GPU_SAMPLER:
			return gpu_sampler_metadata.GetIndexFromHandle(handle);
		case ECS_ASSET_SHADER:
			return shader_metadata.GetIndexFromHandle(handle);
		case ECS_ASSET_MATERIAL:
			return material_asset.GetIndexFromHandle(handle);
		case ECS_ASSET_MISC:
			return misc_asset.GetIndexFromHandle(handle);
		}

		ECS_ASSERT(false, "Invalid asset type");
		return -1;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::GetReferenceCount(unsigned int handle, ECS_ASSET_TYPE type) const
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return mesh_metadata[handle].reference_count;
		case ECS_ASSET_TEXTURE:
			return texture_metadata[handle].reference_count;
		case ECS_ASSET_GPU_SAMPLER:
			return gpu_sampler_metadata[handle].reference_count;
		case ECS_ASSET_SHADER:
			return shader_metadata[handle].reference_count;
		case ECS_ASSET_MATERIAL:
			return material_asset[handle].reference_count;
		case ECS_ASSET_MISC:
			return misc_asset[handle].reference_count;
		default:
			ECS_ASSERT(false, "Invalid asset type");
			return -1;
		}
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::GetReferenceCountStandalone(unsigned int handle, ECS_ASSET_TYPE type) const
	{
		unsigned int reference_count = GetReferenceCount(handle, type);
		if (ExistsStaticArray(type, ECS_ASSET_TYPES_NOT_REFERENCEABLE)) {
			return reference_count;
		}

		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 128);
		ForEach(ECS_ASSET_TYPES_WITH_DEPENDENCIES, [&](ECS_ASSET_TYPE current_type) {
			// Potential references from other asset types
			unsigned int count = GetAssetCount(current_type);
			for (unsigned int index = 0; index < count; index++) {
				dependencies.size = 0;
				GetAssetDependencies(GetAssetConst(GetAssetHandleFromIndex(index, current_type), current_type), current_type, &dependencies);

				for (unsigned int subindex = 0; subindex < dependencies.size; subindex++) {
					if (dependencies[subindex].type == type && dependencies[subindex].handle == handle) {
						reference_count--;
					}
				}
			}
		});

		return reference_count;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::GetReferenceCountsStandalone(ECS_ASSET_TYPE type, CapacityStream<uint2>* counts) const
	{
		unsigned int asset_count = GetAssetCount(type);
		ECS_ASSERT(counts->capacity - counts->size >= asset_count);
		for (unsigned int index = 0; index < asset_count; index++) {
			unsigned int current_handle = GetAssetHandleFromIndex(index, type);
			counts->AddAssert({ current_handle, GetReferenceCount(current_handle, type) });
		}

		if (ExistsStaticArray(type, ECS_ASSET_TYPES_NOT_REFERENCEABLE)) {
			return;
		}

		// Possibly referenced by others
		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 128);
		ForEach(ECS_ASSET_TYPES_WITH_DEPENDENCIES, [&](ECS_ASSET_TYPE asset_type) {
			unsigned int count = GetAssetCount(asset_type);
			for (unsigned int index = 0; index < count; index++) {
				dependencies.size = 0;
				const void* metadata = GetAssetConst(GetAssetHandleFromIndex(index, asset_type), asset_type);
				GetAssetDependencies(metadata, asset_type, &dependencies);

				for (unsigned int subindex = 0; subindex < dependencies.size; subindex++) {
					if (dependencies[subindex].type == type) {
						unsigned int current_index = GetIndexFromAssetHandle(dependencies[subindex].handle, type);
						counts->buffer[current_index].y--;
					}
				}
			}
		});
	}

	// --------------------------------------------------------------------------------------

	// The functor takes as parameter a (size_t index) which returns the current ECS_ASSET_TYPE
	template<typename TypeFunctor>
	Stream<Stream<unsigned int>> GetDependentAssetsForImpl(
		const AssetDatabase* database,
		const void* metadata,
		ECS_ASSET_TYPE type,
		AllocatorPolymorphic allocator,
		bool include_itself,
		size_t asset_types_to_check,
		TypeFunctor&& type_functor
	) {
		Stream<Stream<unsigned int>> handles;
		handles.Initialize(allocator, ECS_ASSET_TYPE_COUNT);
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			handles[index] = { nullptr, 0 };
		}

		if (include_itself) {
			unsigned int current_handle = database->FindAssetEx(GetAssetFromMetadata(metadata, type), type);
			if (current_handle != -1) {
				handles[type].AddResize(current_handle, allocator);
			}
		}

		if (ExistsStaticArray(type, ECS_ASSET_TYPES_NOT_REFERENCEABLE)) {
			return handles;
		}

		// Verify that it is a referenceable type
		ECS_ASSERT(ExistsStaticArray(type, ECS_ASSET_TYPES_REFERENCEABLE), "Invalid asset type");

		unsigned int current_handle = database->FindAssetEx(GetAssetFromMetadata(metadata, type), type);

		for (size_t index = 0; index < asset_types_to_check; index++) {
			ECS_ASSET_TYPE current_type = type_functor(index);

			unsigned int count = database->GetAssetCount(current_type);
			handles[current_type].Initialize(allocator, count);
			handles[current_type].size = 0;
			for (unsigned int subindex = 0; subindex < count; subindex++) {
				unsigned int handle = database->GetAssetHandleFromIndex(subindex, current_type);
				const void* asset = database->GetAssetConst(handle, current_type);
				if (DoesAssetReferenceOtherAsset(current_handle, type, asset, current_type)) {
					handles[current_type].Add(handle);
				}
			}
		}

		return handles;
	}

	Stream<Stream<unsigned int>> AssetDatabase::GetDependentAssetsFor(
		const void* metadata, 
		ECS_ASSET_TYPE type, 
		AllocatorPolymorphic allocator, 
		bool include_itself
	) const
	{
		return GetDependentAssetsForImpl(this, metadata, type, allocator, include_itself, ECS_COUNTOF(ECS_ASSET_TYPES_WITH_DEPENDENCIES), [&](size_t index) {
			return ECS_ASSET_TYPES_WITH_DEPENDENCIES[index];
		});
	}

	// --------------------------------------------------------------------------------------

	Stream<Stream<unsigned int>> AssetDatabase::GetMetadataDependentAssetsFor(
		const void* metadata, 
		ECS_ASSET_TYPE type, 
		AllocatorPolymorphic allocator, 
		bool include_itself
	) const
	{
		return GetDependentAssetsForImpl(this, metadata, type, allocator, include_itself, ECS_COUNTOF(ECS_ASSET_TYPES_METADATA_WITH_DEPENDENCY), [&](size_t index) {
			return ECS_ASSET_TYPES_METADATA_WITH_DEPENDENCY[index].main_type;
		});
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::GetDependencies(unsigned int handle, ECS_ASSET_TYPE type, CapacityStream<AssetTypedHandle>* dependencies) const
	{
		GetAssetDependencies(GetAssetConst(handle, type), type, dependencies);
	}

	// --------------------------------------------------------------------------------------

	AssetDatabaseSnapshot AssetDatabase::GetSnapshot(AllocatorPolymorphic allocator) const
	{
		AssetDatabaseSnapshot snapshot;

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
			snapshot.stream_sizes[index] = GetAssetCount(current_type);
			if (snapshot.stream_sizes[index] > 0) {
				snapshot.reference_counts[index] = (unsigned int*)AllocateEx(allocator, sizeof(unsigned int) * snapshot.stream_sizes[index]);
				for (unsigned int subindex = 0; subindex < snapshot.stream_sizes[index]; subindex++) {
					snapshot.reference_counts[index][subindex] = GetReferenceCount(GetAssetHandleFromIndex(subindex, current_type), current_type);
				}
			}
			else {
				snapshot.reference_counts[index] = nullptr;
			}
		}

		return snapshot;
	}
	
	// --------------------------------------------------------------------------------------

	void AssetDatabase::GetGPUResources(AdditionStream<void*> resources) const {
		ForEachAsset([this, &resources](unsigned int handle, ECS_ASSET_TYPE type) {
			const void* metadata = GetAssetConst(handle, type);
			GetAssetGPUResources(metadata, type, resources);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::GetRandomizedPointer(ECS_ASSET_TYPE type) const
	{
		unsigned int count = GetAssetCount(type);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, randomized_values, count);
		RandomizedPointerList(count, type, randomized_values);
		return randomized_values[0];
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::IsAssetReferencedByOtherAsset(unsigned int handle, ECS_ASSET_TYPE type) const
	{
		if (ExistsStaticArray(type, ECS_ASSET_TYPES_NOT_REFERENCEABLE)) {
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
		return ForEach<true>(ECS_ASSET_TYPES_WITH_DEPENDENCIES, [&](ECS_ASSET_TYPE asset_type) {
			unsigned int count = GetAssetCount(asset_type);
			for (unsigned int index = 0; index < count; index++) {
				dependencies.size = 0;
				const void* asset = GetAssetConst(GetAssetHandleFromIndex(index, asset_type), asset_type);
				GetAssetDependencies(asset, asset_type, &dependencies);
				for (unsigned int subindex = 0; subindex < dependencies.size; subindex++) {
					if (handle == dependencies[subindex].handle && type == dependencies[subindex].type) {
						return true;
					}
				}
			}
			return false;
		});
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RandomizedPointerList(unsigned int maximum_count, ECS_ASSET_TYPE type, CapacityStream<unsigned int>& values) const
	{
		ECS_ASSERT(maximum_count <= ECS_ASSET_RANDOMIZED_ASSET_LIMIT);

		CapacityStream<unsigned int>* randomized_values = &values;
		if (values.capacity < maximum_count) {
			// It is allocated with _alloca, so it will outlive the scope until the end of the function
			ECS_STACK_CAPACITY_STREAM(unsigned int, _randomized_values, ECS_ASSET_RANDOMIZED_ASSET_LIMIT);
			randomized_values = &_randomized_values;
		}

		randomized_values->size = maximum_count;
		MakeSequence(*randomized_values, 1);

		unsigned int count = GetAssetCount(type);
		for (unsigned int index = 0; index < count; index++) {
			unsigned int current_handle = GetAssetHandleFromIndex(index, type);

			const void* metadata = GetAssetConst(current_handle, type);
			Stream<void> asset_value = GetAssetFromMetadata(metadata, type);
			if (asset_value.buffer != nullptr && !IsAssetPointerFromMetadataValid(asset_value)) {
				// It is a randomized asset
				unsigned int randomized_value = ExtractRandomizedAssetValue(asset_value.buffer, type);
				size_t valid_index = SearchBytes(
					randomized_values->buffer,
					randomized_values->size,
					randomized_value,
					sizeof(randomized_value)
				);

				ECS_ASSERT(valid_index != -1);
				randomized_values->RemoveSwapBack(valid_index);
			}
		}

		if (randomized_values != &values) {
			values.CopyOther(*randomized_values);
		}
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::RandomizePointer(unsigned int handle, ECS_ASSET_TYPE type)
	{
		// Pick the remaining randomized asset value
		void* metadata = GetAsset(handle, type);
		return RandomizePointer(metadata, type);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::RandomizePointer(void* metadata, ECS_ASSET_TYPE type) const {
		if (IsAssetPointerFromMetadataValid(GetAssetFromMetadata(metadata, type))) {
			unsigned int count = GetAssetCount(type);
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, valid_indices, count);
			RandomizedPointerList(count, type, valid_indices);
			SetRandomizedAssetToMetadata(metadata, type, valid_indices[0]);

			return valid_indices[0];
		}
		return (unsigned int)GetAssetFromMetadata(metadata, type).buffer;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RandomizePointers(AssetDatabaseSnapshot snapshot)
	{
		unsigned int max_count = 0;
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			unsigned int current_count = GetAssetCount((ECS_ASSET_TYPE)index);
			max_count = max(current_count, max_count);
		}

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, randomized_values, max_count);
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
			unsigned int current_count = GetAssetCount((ECS_ASSET_TYPE)index);
			unsigned int difference = current_count - snapshot.stream_sizes[current_type];

			if (difference > 0) {
				randomized_values.size = 0;
				RandomizedPointerList(max_count, current_type, randomized_values);

				ECS_ASSERT(randomized_values.size >= difference);

				for (unsigned int subindex = 0; subindex < difference; subindex++) {
					unsigned int handle = GetAssetHandleFromIndex(subindex + snapshot.stream_sizes[current_type], current_type);
					void* metadata = GetAsset(handle, current_type);
					SetRandomizedAssetToMetadata(metadata, current_type, randomized_values[subindex]);
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RemapAssetDependencies(const AssetDatabase* other)
	{
		ForEach(ECS_ASSET_TYPES_WITH_DEPENDENCIES, [this, other](ECS_ASSET_TYPE type) {
			unsigned int asset_count = GetAssetCount(type);
			for (unsigned int index = 0; index < asset_count; index++) {
				ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);

				void* current_asset = GetAsset(GetAssetHandleFromIndex(index, type), type);
				Stream<char> asset_name = ECSEngine::GetAssetName(current_asset, type);
				Stream<wchar_t> asset_file = GetAssetFile(current_asset, type);

				unsigned int other_handle = other->FindAsset(asset_name, asset_file, type);
				ECS_ASSERT(other_handle != -1);

				const void* other_asset = other->GetAssetConst(other_handle, type);
				GetAssetDependencies(other_asset, type, &dependencies);

				for (unsigned int subindex = 0; subindex < dependencies.size; subindex++) {
					const void* other_dependency = other->GetAssetConst(dependencies[subindex].handle, dependencies[subindex].type);
					Stream<char> dependency_name = ECSEngine::GetAssetName(other_dependency, dependencies[subindex].type);
					Stream<wchar_t> dependency_file = GetAssetFile(other_dependency, dependencies[subindex].type);
					dependencies[subindex].handle = FindAsset(dependency_name, dependency_file, dependencies[subindex].type);
				}

				ECSEngine::RemapAssetDependencies(current_asset, type, dependencies);
			}
		});
	}

	// --------------------------------------------------------------------------------------

	// It will fill in the structures and perform the needed operations for the options selected in the remove_info
	void ExecuteAssetDatabaseRemoveInfo(
		AssetDatabase* database, 
		const void* metadata, 
		ECS_ASSET_TYPE asset_type, 
		AssetDatabaseRemoveInfo* remove_info,
		bool asset_will_be_removed
	) {
		bool has_info = remove_info != nullptr;

		if (has_info) {
			unsigned int starting_dependency_size = 0;

			if (remove_info->dependencies != nullptr) {
				GetAssetDependencies(metadata, asset_type, remove_info->dependencies);
				starting_dependency_size = remove_info->dependencies->size;
			}

			if (remove_info->remove_dependencies && (!remove_info->remove_dependencies_only_if_main_asset_removed || asset_will_be_removed)) {
				bool has_storage_dependencies = remove_info->storage_dependencies != nullptr;
				if (has_storage_dependencies) {
					ECS_ASSERT(remove_info->storage_dependencies_allocation.capacity > 0);
				}

				ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, temp_dependencies, 512);
				Stream<AssetTypedHandle> dependencies;
				if (remove_info->dependencies == nullptr) {
					GetAssetDependencies(metadata, asset_type, &temp_dependencies);
					dependencies = temp_dependencies;
				}
				else {
					dependencies = *remove_info->dependencies;
				}

				for (unsigned int index = starting_dependency_size; index < dependencies.size; index++) {
					if (has_storage_dependencies) {
						size_t asset_metadata_byte_size = AssetMetadataByteSize(dependencies[index].type);
						remove_info->storage_dependencies_allocation.AssertCapacity(asset_metadata_byte_size);
						remove_info->storage = OffsetPointer(remove_info->storage_dependencies_allocation);
						remove_info->storage_dependencies_allocation.size += asset_metadata_byte_size;
					}
					bool evicted = database->RemoveAsset(dependencies[index].handle, dependencies[index].type, 1, remove_info);
					if (evicted && has_storage_dependencies) {
						remove_info->storage_dependencies->AddAssert({ remove_info->storage, dependencies[index].type });
					}
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------

	template<typename AssetType>
	bool RemoveAssetImpl(
		AssetDatabase* database, 
		AssetType& type, 
		unsigned int handle, 
		ECS_ASSET_TYPE asset_type,
		unsigned int decrement_count,
		void* storage, 
		AssetDatabaseRemoveInfo* remove_info
	) {
		auto& metadata = type[handle];
		bool asset_will_be_removed = metadata.reference_count <= decrement_count;
		ExecuteAssetDatabaseRemoveInfo(database, &type[handle].value, asset_type, remove_info, asset_will_be_removed);

		if (asset_will_be_removed) {
			if (storage != nullptr) {
				memcpy(storage, &metadata.value, sizeof(metadata.value));
			}

			// Deallocate the memory
			metadata.value.DeallocateMemory(type.allocator);
			type.RemoveSwapBack(handle);
			return true;
		}
		else {
			metadata.reference_count -= decrement_count;
		}
		return false;
	}

	// --------------------------------------------------------------------------------------

	static bool ReadAssetFileImpl(
		const AssetDatabase* database,
		Stream<wchar_t> metadata_file,
		void* asset,
		const char* asset_string,
		ECS_ASSET_TYPE asset_type,
		AllocatorPolymorphic allocator
	) {
		if (database->metadata_file_location.size > 0) {
			if (allocator.allocator == nullptr) {
				allocator = database->Allocator();
			}
			DeserializeOptions deserialize_options;
			deserialize_options.field_allocator = allocator;

			SetSerializeCustomMaterialAssetDatabase(database);

			ECS_DESERIALIZE_CODE serialize_code = DeserializeEx(database->reflection_manager, asset_string, asset, metadata_file, &deserialize_options);

			SetSerializeCustomMaterialAssetDatabase((AssetDatabase*)nullptr);

			if (serialize_code == ECS_DESERIALIZE_OK) {
				return true;
			}
			return false;
		}
		return true;
	}

	static bool ReadAssetFileImpl(
		const AssetDatabase* database, 
		Stream<char> name, 
		Stream<wchar_t> file, 
		void* asset, 
		const char* asset_string, 
		ECS_ASSET_TYPE asset_type,
		AllocatorPolymorphic allocator,
		bool default_initialize_if_missing
	) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
		database->FileLocationAsset(name, file, path, asset_type);
		if (default_initialize_if_missing) {
			if (!ExistsFileOrFolder(path)) {
				CreateDefaultAsset(asset, name, file, asset_type);
				return true;
			}
		}
		return ReadAssetFileImpl(database, path, asset, asset_string, asset_type, allocator);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadMeshFile(Stream<char> name, Stream<wchar_t> file, MeshMetadata* metadata, bool default_initialize_if_missing) const {
		return ReadAssetFileImpl(this, name, file, metadata, STRING(MeshMetadata), ECS_ASSET_MESH, { nullptr }, default_initialize_if_missing);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadTextureFile(Stream<char> name, Stream<wchar_t> file, TextureMetadata* metadata, bool default_initialize_if_missing) const {
		return ReadAssetFileImpl(this, name, file, metadata, STRING(TextureMetadata), ECS_ASSET_TEXTURE, { nullptr }, default_initialize_if_missing);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadGPUSamplerFile(Stream<char> name, GPUSamplerMetadata* metadata, bool default_initialize_if_missing) const {
		return ReadAssetFileImpl(this, name, {}, metadata, STRING(GPUSamplerMetadata), ECS_ASSET_GPU_SAMPLER, { nullptr }, default_initialize_if_missing);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadShaderFile(Stream<char> name, ShaderMetadata* metadata, AllocatorPolymorphic allocator, bool default_initialize_if_missing) const {
		return ReadAssetFileImpl(this, name, {}, metadata, STRING(ShaderMetadata), ECS_ASSET_SHADER, allocator, default_initialize_if_missing);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadMaterialFile(Stream<char> name, MaterialAsset* asset, AllocatorPolymorphic allocator, bool default_initialize_if_missing) const {
		return ReadAssetFileImpl(this, name, {}, asset, STRING(MaterialAsset), ECS_ASSET_MATERIAL, allocator, default_initialize_if_missing);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadMiscFile(Stream<char> name, Stream<wchar_t> file, MiscAsset* asset, bool default_initialize_if_missing) const {
		// At the moment there is nothing to be read from the misc asset
		return true;
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadAssetFile(
		Stream<char> name, 
		Stream<wchar_t> file, 
		void* metadata, 
		ECS_ASSET_TYPE asset_type, 
		AllocatorPolymorphic allocator, 
		bool default_initialize_if_missing
	) const {
		switch (asset_type) {
		case ECS_ASSET_MESH:
		{
			return ReadMeshFile(name, file, (MeshMetadata*)metadata, default_initialize_if_missing);
		}
		case ECS_ASSET_TEXTURE:
		{
			return ReadTextureFile(name, file, (TextureMetadata*)metadata, default_initialize_if_missing);
		}
		case ECS_ASSET_GPU_SAMPLER:
		{
			return ReadGPUSamplerFile(name, (GPUSamplerMetadata*)metadata, default_initialize_if_missing);
		}
		case ECS_ASSET_SHADER:
		{
			return ReadShaderFile(name, (ShaderMetadata*)metadata, allocator, default_initialize_if_missing);
		}
		case ECS_ASSET_MATERIAL:
		{
			return ReadMaterialFile(name, (MaterialAsset*)metadata, allocator, default_initialize_if_missing);
		}
		case ECS_ASSET_MISC:
		{
			return ReadMiscFile(name, file, (MiscAsset*)metadata, default_initialize_if_missing);
		}
		default:
			ECS_ASSERT(false, "Invalid asset type.");
		}
		return false;
	}

	bool AssetDatabase::ReadAssetFile(
		Stream<wchar_t> metadata_file, 
		void* metadata, 
		ECS_ASSET_TYPE asset_type, 
		AllocatorPolymorphic allocator
	)
	{
		switch (asset_type) {
		case ECS_ASSET_MESH:
			return ReadAssetFileImpl(this, metadata_file, metadata, STRING(MeshMetadata), asset_type, allocator);
			break;
		case ECS_ASSET_TEXTURE:
			return ReadAssetFileImpl(this, metadata_file, metadata, STRING(TextureMetadata), asset_type, allocator);
			break;
		case ECS_ASSET_GPU_SAMPLER:
			return ReadAssetFileImpl(this, metadata_file, metadata, STRING(GPUSamplerMetadata), asset_type, allocator);
			break;
		case ECS_ASSET_SHADER:
			return ReadAssetFileImpl(this, metadata_file, metadata, STRING(ShaderMetadata), asset_type, allocator);
			break;
		case ECS_ASSET_MATERIAL:
			return ReadAssetFileImpl(this, metadata_file, metadata, STRING(MaterialAsset), asset_type, allocator);
			break;
		case ECS_ASSET_MISC:
			// At the moment, this asset doesn't have any metadata file
			return true;
			break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		return false;
	}

	// --------------------------------------------------------------------------------------

	// The rename functor receives as parameters (void* metadata, AllocatorPolymorphic allocator)
	// The update metadata files functor receives as parameters (Stream<char> previous_name, Stream<wchar_t> previous_file)
	template<typename RenameFunctor, typename UpdateMetadataFilesFunctor>
	static bool RenameAssetImpl(
		AssetDatabase* database, 
		unsigned int handle, 
		ECS_ASSET_TYPE type, 
		bool* update_dependency_metadata_files,
		RenameFunctor&& rename_functor,
		UpdateMetadataFilesFunctor&& update_metadata_files_functor
	) {
		void* metadata = database->GetAsset(handle, type);
		size_t metadata_storage[AssetMetadataMaxSizetSize()];
		// We can rename directly here. It will deallocate the previous name, but it is fine
		// Since all deallocates use DeallocateIfBelongs and won't trigger an error when trying
		// To update the asset
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(temporary_allocator, ECS_KB * 128, ECS_MB);
		Stream<char> previous_name = {};
		Stream<wchar_t> previous_file = {};
		if (update_dependency_metadata_files != nullptr) {
			previous_name = ECSEngine::GetAssetName(metadata, type).Copy(&temporary_allocator);
			previous_file = ECSEngine::GetAssetFile(metadata, type).Copy(&temporary_allocator);
		}
		CopyAssetBase(metadata_storage, metadata, type, &temporary_allocator);
		rename_functor(metadata_storage, database->Allocator());
		bool success = database->UpdateAsset(handle, metadata_storage, type);
		if (success && update_dependency_metadata_files != nullptr) {
			*update_dependency_metadata_files = update_metadata_files_functor(previous_name, previous_file);
		}
		return success;
	}
	
	bool AssetDatabase::RenameAsset(unsigned int handle, ECS_ASSET_TYPE type, Stream<char> new_name, bool* update_dependency_metadata_files)
	{
		return RenameAssetImpl(this, handle, type, update_dependency_metadata_files, [=](void* metadata, AllocatorPolymorphic allocator) {
			ECSEngine::RenameAsset(metadata, type, new_name, allocator);
		}, [=](Stream<char> previous_name, Stream<wchar_t> previous_file) {
			return RenameAssetUpdateMetadataFiles(previous_name, previous_file, new_name, type);
		});
	}

	// --------------------------------------------------------------------------------------

	// The functor is called with (void* dependency, Stream<char> dependency_name, Stream<wchar_t> dependency_file, AllocatorPolymorphic allocator)
	// It must return true if there is a match, and if there is, it also performs the renaming
	template<typename CompareRenameFunctor>
	static bool RenameAssetUpdateMetadataFilesImpl(const AssetDatabase* database, ECS_ASSET_TYPE type, CompareRenameFunctor&& compare_rename_functor) {
		if (ECSEngine::ExistsStaticArray(type, ECS_ASSET_TYPES_REFERENCEABLE)) {
			bool success = true;
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB);
			AllocatorPolymorphic stack_allocator = &_stack_allocator;
			// We could load the metadata files directly into our database, but to keep things
			// Clean and to not have to do any cleanup here, create a temporary database that uses
			// This stack allocator
			AssetDatabase temporary_database;
			ECSEngine::ForEach(ECS_ASSET_TYPES_WITH_DEPENDENCIES, [&](ECS_ASSET_TYPE with_dependency_type) {
				_stack_allocator.Clear();
				// Reinitialize again the temporary database
				temporary_database = AssetDatabase(stack_allocator, database->reflection_manager);
				temporary_database.SetFileLocation(database->metadata_file_location);
				Stream<Stream<wchar_t>> files = database->GetMetadatasForType(with_dependency_type, stack_allocator);

				size_t metadata_storage[AssetMetadataMaxSizetSize()];
				ECS_STACK_CAPACITY_STREAM(wchar_t, metadata_folder, 512);
				AssetDatabaseFileDirectory(database->metadata_file_location, metadata_folder, with_dependency_type);
				metadata_folder.Add(ECS_OS_PATH_SEPARATOR);
				unsigned int metadata_folder_size = metadata_folder.size;
				for (size_t index = 0; index < files.size; index++) {
					metadata_folder.size = metadata_folder_size;
					metadata_folder.AddStreamAssert(files[index]);
					// Read the metadata file and compare
					bool current_success = temporary_database.ReadAssetFile(
						metadata_folder,
						metadata_storage,
						with_dependency_type
					);
					if (!current_success) {
						success = false;
					}
					else {
						// Check to see if this appears in the dependency for this file asset
						ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
						GetAssetDependencies(metadata_storage, with_dependency_type, &dependencies);
						for (unsigned int dependency_index = 0; dependency_index < dependencies.size; dependency_index++) {
							ECS_ASSET_TYPE dependency_type = dependencies[dependency_index].type;
							if (dependency_type == type) {
								void* metadata = temporary_database.GetAsset(dependencies[dependency_index].handle, dependency_type);
								Stream<char> dependency_name = ECSEngine::GetAssetName(metadata, dependency_type);
								Stream<wchar_t> dependency_file = ECSEngine::GetAssetFile(metadata, dependency_type);
								if (compare_rename_functor(metadata, dependency_name, dependency_file, temporary_database.Allocator())) {
									// Write the metadata file again
									current_success = temporary_database.WriteAssetFile(metadata_storage, with_dependency_type);
									if (!current_success) {
										success = false;
									}
									break;
								}
							}
						}
					}
				}
				});
			return success;
		}
		return true;
	}

	bool AssetDatabase::RenameAssetUpdateMetadataFiles(Stream<char> previous_name, Stream<wchar_t> file, Stream<char> new_name, ECS_ASSET_TYPE type) const
	{
		return RenameAssetUpdateMetadataFilesImpl(this, type, [&](
			void* dependency, 
			Stream<char> dependency_name, 
			Stream<wchar_t> dependency_file, 
			AllocatorPolymorphic allocator
		) {
			if (dependency_name == previous_name && dependency_file == file) {
				ECSEngine::RenameAsset(dependency, type, new_name, allocator);
				return true;
			}
			return false;
		});
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RenameAssetFile(unsigned int handle, ECS_ASSET_TYPE type, Stream<wchar_t> new_path, bool* update_dependency_metadata_files)
	{
		return RenameAssetImpl(this, handle, type, update_dependency_metadata_files, [=](void* metadata, AllocatorPolymorphic allocator) {
			ECSEngine::RenameAssetFile(metadata, type, new_path, allocator);
			}, [=](Stream<char> previous_name, Stream<wchar_t> previous_file) {
				return RenameAssetFileUpdateMetadataFiles(previous_file, previous_name, new_path, type);
		});
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RenameAssetFileUpdateMetadataFiles(Stream<wchar_t> previous_file, Stream<char> name, Stream<wchar_t> new_file, ECS_ASSET_TYPE type) const
	{
		return RenameAssetUpdateMetadataFilesImpl(this, type, [&](
			void* dependency,
			Stream<char> dependency_name,
			Stream<wchar_t> dependency_file,
			AllocatorPolymorphic allocator
		) {
				if (dependency_name == name && dependency_file == previous_file) {
					ECSEngine::RenameAssetFile(dependency, type, new_file, allocator);
					return true;
				}
				return false;
		});
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMesh(unsigned int handle, unsigned int decrement_count, MeshMetadata* storage)
	{
		return RemoveAssetImpl(this, mesh_metadata, handle, ECS_ASSET_MESH, decrement_count, storage, nullptr);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveTexture(unsigned int handle, unsigned int decrement_count, TextureMetadata* storage)
	{
		return RemoveAssetImpl(this, texture_metadata, handle, ECS_ASSET_TEXTURE, decrement_count, storage, nullptr);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveGPUSampler(unsigned int handle, unsigned int decrement_count, GPUSamplerMetadata* storage)
	{
		return RemoveAssetImpl(this, gpu_sampler_metadata, handle, ECS_ASSET_GPU_SAMPLER, decrement_count, storage, nullptr);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveShader(unsigned int handle, unsigned int decrement_count, ShaderMetadata* storage)
	{
		return RemoveAssetImpl(this, shader_metadata, handle, ECS_ASSET_SHADER, decrement_count, storage, nullptr);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMaterial(unsigned int handle, unsigned int decrement_count, AssetDatabaseRemoveInfo* remove_info)
	{
		void* storage = remove_info != nullptr ? remove_info->storage : nullptr;
		return RemoveAssetImpl(this, material_asset, handle, ECS_ASSET_MATERIAL, decrement_count, storage, remove_info);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMisc(unsigned int handle, unsigned int decrement_count, MiscAsset* storage)
	{
		return RemoveAssetImpl(this, misc_asset, handle, ECS_ASSET_MISC, decrement_count, storage, nullptr);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveAsset(unsigned int handle, ECS_ASSET_TYPE type, unsigned int decrement_count, AssetDatabaseRemoveInfo* remove_info)
	{
		void* storage = remove_info != nullptr ? remove_info->storage : nullptr;

		switch (type) {
		case ECS_ASSET_MESH:
			return RemoveMesh(handle, decrement_count, (MeshMetadata*)storage);
		case ECS_ASSET_TEXTURE:
			return RemoveTexture(handle, decrement_count, (TextureMetadata*)storage);
		case ECS_ASSET_GPU_SAMPLER:
			return RemoveGPUSampler(handle, decrement_count, (GPUSamplerMetadata*)storage);
		case ECS_ASSET_SHADER:
			return RemoveShader(handle, decrement_count, (ShaderMetadata*)storage);
		case ECS_ASSET_MATERIAL:
			return RemoveMaterial(handle, decrement_count, remove_info);
		case ECS_ASSET_MISC:
			return RemoveMisc(handle, decrement_count, (MiscAsset*)storage);
		}
		ECS_ASSERT(false, "Invalid asset type");
		return false;
	}

	// --------------------------------------------------------------------------------------

	template<typename AssetType>
	void RemoveAssetForcedImpl(AssetType& type, unsigned int handle) {
		auto& metadata = type[handle];

		// Deallocate the memory
		metadata.value.DeallocateMemory(type.allocator);
		type.RemoveSwapBack(handle);
	}

	void AssetDatabase::RemoveAssetForced(unsigned int handle, ECS_ASSET_TYPE type, AssetDatabaseRemoveInfo* remove_info) {
		const void* metadata = GetAssetConst(handle, type);
		ExecuteAssetDatabaseRemoveInfo(this, metadata, type, remove_info, true);
		if (remove_info != nullptr && remove_info->storage != nullptr) {
			memcpy(remove_info->storage, metadata, AssetMetadataByteSize(type));
		}

		switch (type) {
		case ECS_ASSET_MESH:
		{
			RemoveAssetForcedImpl(mesh_metadata, handle);
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			RemoveAssetForcedImpl(texture_metadata, handle);
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			RemoveAssetForcedImpl(gpu_sampler_metadata, handle);
		}
		break;
		case ECS_ASSET_SHADER:
		{
			RemoveAssetForcedImpl(shader_metadata, handle);
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			RemoveAssetForcedImpl(material_asset, handle);
		}
		break;
		case ECS_ASSET_MISC:
		{
			RemoveAssetForcedImpl(misc_asset, handle);
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type")
		}
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RemoveAssetDependencies(const void* asset, ECS_ASSET_TYPE type)
	{
		if (type == ECS_ASSET_MATERIAL) {
			ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, typed_handles, 128);
			const MaterialAsset* material = (const MaterialAsset*)asset;
			material->GetDependencies(&typed_handles);
			for (unsigned int index = 0; index < typed_handles.size; index++) {
				RemoveAsset(typed_handles[index].handle, typed_handles[index].type);
			}
		}
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RemoveAssetDependencies(unsigned int handle, ECS_ASSET_TYPE type)
	{
		if (IsAssetTypeWithDependencies(type)) {
			RemoveAssetDependencies(GetMaterial(handle), type);
		}
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RestoreSnapshot(AssetDatabaseSnapshot snapshot)
	{
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)index;
			unsigned int current_count = GetAssetCount(asset_type);
			for (unsigned int subindex = snapshot.stream_sizes[asset_type]; subindex < current_count; subindex++) {
				RemoveAssetForced(GetAssetHandleFromIndex(subindex, asset_type), asset_type);
			}
			for (unsigned int subindex = 0; subindex < snapshot.stream_sizes[asset_type]; subindex++) {
				unsigned int snapshot_reference_count = snapshot.reference_counts[asset_type][subindex];
				SetAssetReferenceCount(GetAssetHandleFromIndex(subindex, asset_type), asset_type, snapshot_reference_count);
			}
		}
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::SetAllocator(AllocatorPolymorphic allocator)
	{
		mesh_metadata.allocator = allocator;
		texture_metadata.allocator = allocator;
		gpu_sampler_metadata.allocator = allocator;
		shader_metadata.allocator = allocator;
		material_asset.allocator = allocator;
		misc_asset.allocator = allocator;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::SetFileLocation(Stream<wchar_t> _file_location)
	{
		if (metadata_file_location.buffer != nullptr) {
			Deallocate(mesh_metadata.allocator, metadata_file_location.buffer);
		}

		if (_file_location.size > 0) {
			size_t size_to_allocate = sizeof(wchar_t) * (_file_location.size + 2);
			void* allocation = Allocate(mesh_metadata.allocator, size_to_allocate);
			memcpy(allocation, _file_location.buffer, _file_location.size * sizeof(wchar_t));
			metadata_file_location.buffer = (wchar_t*)allocation;
			metadata_file_location.size = _file_location.size;

			if (PathIsAbsolute(_file_location)) {
				if (metadata_file_location[metadata_file_location.size - 1] != ECS_OS_PATH_SEPARATOR) {
					metadata_file_location.Add(ECS_OS_PATH_SEPARATOR);
				}
			}
			else {
				if (metadata_file_location[metadata_file_location.size - 1] != ECS_OS_PATH_SEPARATOR_REL) {
					metadata_file_location.Add(ECS_OS_PATH_SEPARATOR_REL);
				}
			}

			metadata_file_location[metadata_file_location.size] = L'\0';
		}
		else {
			metadata_file_location = { nullptr, 0 };
		}
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::SetAssetReferenceCount(unsigned int handle, ECS_ASSET_TYPE asset_type, unsigned int new_reference_count)
	{
		switch (asset_type) {
		case ECS_ASSET_MESH:
			mesh_metadata[handle].reference_count = new_reference_count;
			break;
		case ECS_ASSET_TEXTURE:
			texture_metadata[handle].reference_count = new_reference_count;
			break;
		case ECS_ASSET_GPU_SAMPLER:
			gpu_sampler_metadata[handle].reference_count = new_reference_count;
			break;
		case ECS_ASSET_SHADER:
			shader_metadata[handle].reference_count = new_reference_count;
			break;
		case ECS_ASSET_MATERIAL:
			material_asset[handle].reference_count = new_reference_count;
			break;
		case ECS_ASSET_MISC:
			misc_asset[handle].reference_count = new_reference_count;
			break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
	}

	// --------------------------------------------------------------------------------------

	template<typename MetadataSparse>
	void GetUniqueMetadata(CapacityStream<unsigned int>& handles, MetadataSparse sparse) {
		auto data = sparse.ToStream();
		for (size_t index = 0; index < data.size; index++) {
			int64_t subindex = (int64_t)index - 1;
			for (;  subindex >= 0; subindex--) {
				if (data[index].value.SameTarget(&data[subindex].value)) {
					break;
				}
			}

			// If it passed all attemps
			if (subindex < 0) {
				handles.AddAssert(sparse.GetHandleFromIndex(index));
			}
		}
	}

	void AssetDatabase::UniqueMeshes(CapacityStream<unsigned int>& handles) const
	{
		GetUniqueMetadata(handles, mesh_metadata);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::UniqueTextures(CapacityStream<unsigned int>& handles) const
	{
		GetUniqueMetadata(handles, texture_metadata);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::UniqueShaders(CapacityStream<unsigned int>& handles) const
	{
		GetUniqueMetadata(handles, shader_metadata);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::UniqueMiscs(CapacityStream<unsigned int>& handles) const
	{
		GetUniqueMetadata(handles, misc_asset);
	}

	// --------------------------------------------------------------------------------------
	
	template<typename T>
	bool UpdateAssetImpl(AssetDatabase* database, unsigned int handle, const void* metadata, ECS_ASSET_TYPE type, bool update_files) {
		Stream<char> previous_name = database->GetAssetName(handle, type);
		Stream<wchar_t> previous_file = database->GetAssetPath(handle, type);

		Stream<char> new_name = GetAssetName(metadata, type);
		Stream<wchar_t> new_file = GetAssetFile(metadata, type);

		// Check to see that an identical asset doesn't already exist
		unsigned int count = database->GetAssetCount(type);
		for (unsigned int subindex = 0; subindex < count; subindex++) {
			unsigned int subindex_handle = database->GetAssetHandleFromIndex(subindex, type);
			if (subindex_handle != handle) {
				const void* subindex_metadata = database->GetAssetConst(subindex_handle, type);
				Stream<char> subindex_name = GetAssetName(subindex_metadata, type);
				Stream<wchar_t> subindex_file = GetAssetFile(subindex_metadata, type);
				if (subindex_name == new_name && subindex_file == new_file) {
					// An identical asset already exists
					return false;
				}
			}
		}
		
		T* old = (T*)database->GetAsset(handle, type);
		old->DeallocateMemory(database->Allocator());
		*old = ((T*)metadata)->Copy(database->Allocator());

		if (update_files) {
			// Get the update name and file

			// If the name has changed, try writing to the new file before destroying the old one
			ECS_STACK_CAPACITY_STREAM(wchar_t, existing_file, 512);
			database->FileLocationAsset(new_name, new_file, existing_file, type);

			Stream<wchar_t> renamed_file = { nullptr, 0 };
			if (ExistsFileOrFolder(existing_file)) {
				// Rename the existing file.
				renamed_file = existing_file;
				renamed_file.AddStream(L".temp");
				bool success = RenameFileAbsolute(existing_file, renamed_file);
				if (!success) {
					renamed_file = { nullptr, 0 };
				}
			}

			bool success = database->WriteAssetFile(database->GetAsset(handle, type), type);
			if (!success) {
				// Restore the previous file if it exists
				if (renamed_file.size > 0) {
					RenameFileAbsolute(renamed_file, existing_file);
				}
			}
			else {
				// Delete the renamed file if it exists
				// Also, remove the original metadata file
				if (renamed_file.size > 0) {
					RemoveFile(renamed_file);
				}
				ECS_STACK_CAPACITY_STREAM(wchar_t, original_metadata_file, 512);
				database->FileLocationAsset(previous_name, previous_file, original_metadata_file, type);
				RemoveFile(original_metadata_file);
			}
			return success;
		}
		return true;
	}

	bool AssetDatabase::UpdateMesh(unsigned int handle, const MeshMetadata* metadata, bool update_files) {
		return UpdateAssetImpl<MeshMetadata>(this, handle, metadata, ECS_ASSET_MESH, update_files);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::UpdateTexture(unsigned int handle, const TextureMetadata* metadata, bool update_files) {
		return UpdateAssetImpl<TextureMetadata>(this, handle, metadata, ECS_ASSET_TEXTURE, update_files);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::UpdateGPUSampler(unsigned int handle, const GPUSamplerMetadata* metadata, bool update_files) {
		return UpdateAssetImpl<GPUSamplerMetadata>(this, handle, metadata, ECS_ASSET_GPU_SAMPLER, update_files);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::UpdateShader(unsigned int handle, const ShaderMetadata* metadata, bool update_files) {
		return UpdateAssetImpl<ShaderMetadata>(this, handle, metadata, ECS_ASSET_SHADER, update_files);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::UpdateMaterial(unsigned int handle, const MaterialAsset* material, bool update_files) {
		return UpdateAssetImpl<MaterialAsset>(this, handle, material, ECS_ASSET_MATERIAL, update_files);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::UpdateMisc(unsigned int handle, const MiscAsset* misc, bool update_files) {
		return UpdateAssetImpl<MiscAsset>(this, handle, misc, ECS_ASSET_MISC, update_files);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::UpdateAsset(unsigned int handle, const void* asset, ECS_ASSET_TYPE type, bool update_files) {
		switch (type) {
		case ECS_ASSET_MESH:
			return UpdateMesh(handle, (const MeshMetadata*)asset, update_files);
			break;
		case ECS_ASSET_TEXTURE:
			return UpdateTexture(handle, (const TextureMetadata*)asset, update_files);
			break;
		case ECS_ASSET_GPU_SAMPLER:
			return UpdateGPUSampler(handle, (const GPUSamplerMetadata*)asset, update_files);
			break;
		case ECS_ASSET_SHADER:
			return UpdateShader(handle, (const ShaderMetadata*)asset, update_files);
			break;
		case ECS_ASSET_MATERIAL:
			return UpdateMaterial(handle, (const MaterialAsset*)asset, update_files);
			break;
		case ECS_ASSET_MISC:
			return UpdateMisc(handle, (const MiscAsset*)asset, update_files);
			break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		return false;
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::UpdateAssetFromFile(unsigned int handle, ECS_ASSET_TYPE type)
	{
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
		AllocatorPolymorphic stack_allocator_polymorphic = &stack_allocator;

		size_t temporary_asset[AssetMetadataMaxSizetSize()];
		Stream<char> asset_name = GetAssetName(handle, type);
		Stream<wchar_t> asset_file = GetAssetPath(handle, type);

		ECS_STACK_CAPACITY_STREAM(char, temporary_name, 512);
		ECS_STACK_CAPACITY_STREAM(wchar_t, temporary_file, 512);
		temporary_name = StringCopy(stack_allocator_polymorphic, asset_name);
		temporary_file = StringCopy(stack_allocator_polymorphic, asset_file);

		CreateDefaultAsset(temporary_asset, temporary_name, temporary_file, type);
		bool success = ReadAssetFile(asset_name, asset_file, temporary_asset, type, stack_allocator_polymorphic);
		if (success) {
			// Remove the old dependencies
			RemoveAssetDependencies(handle, type);

			// Now copy the new data into the stable allocator
			void* metadata = GetAsset(handle, type);
			DeallocateAssetBase(metadata, type, Allocator());
			CopyAssetBase(metadata, temporary_asset, type, Allocator());
		}

		return success;
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::UpdateAssetsFromFiles(CapacityStream<AssetTypedHandle>* modified_assets)
	{
		bool success = true;
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
		ForEachAsset([&](unsigned int handle, ECS_ASSET_TYPE asset_type) {
			void* current_metadata = GetAsset(handle, asset_type);

			// Load the asset into a temporary and then get its dependencies
			size_t temporary_asset[AssetMetadataMaxSizetSize()];
			CreateDefaultAsset(temporary_asset, { nullptr, 0 }, { nullptr, 0 }, asset_type);
			bool current_success = ReadAssetFile(
				ECSEngine::GetAssetName(current_metadata, asset_type),
				GetAssetFile(current_metadata, asset_type),
				temporary_asset,
				asset_type,
				&stack_allocator,
				true
			);
			if (current_success) {
				// Now we only need to remove the old dependencies
				ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, current_dependencies, 512);
				GetAssetDependencies(current_metadata, asset_type, &current_dependencies);

				// Before finishing we must remove the dependencies for the pre-existing entry
				for (unsigned int index = 0; index < current_dependencies.size; index++) {
					RemoveAsset(current_dependencies[index].handle, current_dependencies[index].type);
				}

				if (modified_assets != nullptr) {
					// Determine if they are different
					if (!CompareAssetOptions(current_metadata, temporary_asset, asset_type)) {
						modified_assets->AddAssert({ handle, asset_type });
					}
				}

				// Now update the asset to contain the new data - this will also deallocate
				// the previous data
				CopyAssetOptions(current_metadata, temporary_asset, asset_type, Allocator());
			}
			else {
				success = false;
			}
		});

		return success;
	}

	// --------------------------------------------------------------------------------------
	
	bool AssetDatabase::UpdateAssetsWithDependenciesFromFiles(CapacityStream<AssetTypedHandle>* modified_assets)
	{
		bool success = true;
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
		ForEach(ECS_ASSET_TYPES_WITH_DEPENDENCIES, [&](ECS_ASSET_TYPE asset_type) {
			ForEachAsset(asset_type, [&](unsigned int handle) {
				void* current_metadata = GetAsset(handle, asset_type);

				// Load the asset into a temporary and then get its dependencies
				size_t temporary_asset[AssetMetadataMaxSizetSize()];
				CreateDefaultAsset(temporary_asset, { nullptr, 0 }, { nullptr, 0 }, asset_type);
				bool current_success = ReadAssetFile(
					ECSEngine::GetAssetName(current_metadata, asset_type), 
					GetAssetFile(current_metadata, asset_type), 
					temporary_asset, 
					asset_type, 
					&stack_allocator,
					true
				);
				if (current_success) {
					// Now we only need to remove the old dependencies
					ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, current_dependencies, 512);
					GetAssetDependencies(current_metadata, asset_type, &current_dependencies);

					// Before finishing we must remove the dependencies for the pre-existing entry
					for (unsigned int index = 0; index < current_dependencies.size; index++) {
						RemoveAsset(current_dependencies[index].handle, current_dependencies[index].type);
					}

					if (modified_assets != nullptr) {
						// Determine if they are different
						if (!CompareAssetOptions(current_metadata, temporary_asset, asset_type)) {
							modified_assets->AddAssert({ handle, asset_type });
						}
					}

					// Now update the asset to contain the new data - this will also deallocate
					// the previous data
					CopyAssetOptions(current_metadata, temporary_asset, asset_type, Allocator());
				}
				else {
					success = false;
				}
			});	
		});

		return success;
	}

	// --------------------------------------------------------------------------------------

	template<typename MetadataSparse>
	static Stream<AssetDatabaseSameTargetAsset> GetSameTargetAssets(AllocatorPolymorphic allocator, MetadataSparse sparse) {
		auto data = sparse.ToStream();
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(AssetDatabaseSameTargetAsset, temp_assets, data.size);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, indices_to_check, data.size);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, current_asset_same, data.size);

		indices_to_check.size = data.size;
		MakeSequence(indices_to_check);

		// The indices_to_check will contain only indices that are actually valid
		while (indices_to_check.size > 0) {
			// Always take the first element from the indices to check
			unsigned int current_index = indices_to_check[0];
			unsigned int add_index = temp_assets.Add({ sparse.GetHandleFromIndex(current_index), { nullptr, 0 } });

			// Remove the first value from indices to check
			indices_to_check.RemoveSwapBack(0);

			// Determine the count of the other assets with the same target
			current_asset_same.size = 0;
			current_asset_same.Add(current_index);

			// We can start again from the 1st index because we eliminated the first element
			for (unsigned int subindex = 0; subindex < indices_to_check.size; subindex++) {
				unsigned int current_subindex = indices_to_check[subindex];
				if (data[current_index].value.SameTarget(&data[current_subindex].value)) {
					// Add the asset to the current list and remove it from the indices to check
					current_asset_same.Add(current_subindex);
					indices_to_check.RemoveSwapBack(subindex);
					// Decrement to keep checking the same element
					subindex--;
				}
			}

			// Allocate the new list
			Stream<unsigned int> allocated_list;
			allocated_list.InitializeAndCopy(allocator, current_asset_same);
			temp_assets[add_index].other_handles = allocated_list;
		}

		Stream<AssetDatabaseSameTargetAsset> allocated_assets;
		allocated_assets.InitializeAndCopy(allocator, temp_assets);
		return allocated_assets;
	}

	// --------------------------------------------------------------------------------------

	template<typename AssetType>
	bool WriteAssetFileImpl(const AssetDatabase* database, const AssetType* asset, Stream<char> asset_string, ECS_ASSET_TYPE asset_type) {
		if (database->metadata_file_location.size > 0) {
			Stream<wchar_t> file = GetAssetFile(asset, asset_type);

			ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
			database->FileLocationAsset(asset->name, file, path, asset_type);

			// Check to see if it already exists and rename it to a temp if it does
			Stream<wchar_t> renamed_file = { nullptr, 0 };
			if (ExistsFileOrFolder(path)) {
				// Rename the file
				renamed_file = path;
				renamed_file.AddStream(L".temp");
				bool success = RenameFileAbsolute(path, renamed_file);
				if (!success) {
					// Make it nullptr to indicate that the renaming failed
					renamed_file = { nullptr, 0 };
				}
			}

			ECS_STACK_CAPACITY_STREAM(SerializeOmitField, omit_fields, 2);
			omit_fields[0].type = asset_string;
			omit_fields[0].name = STRING(name);
			omit_fields.size = 1;
			if (asset_type != ECS_ASSET_SHADER) {
				omit_fields[1].type = asset_string;
				omit_fields[1].name = STRING(file);
				omit_fields.size++;
			}

			SetSerializeCustomMaterialAssetDatabase(database);

			SerializeOptions serialize_options;
			serialize_options.omit_fields = omit_fields;
			ECS_SERIALIZE_CODE serialize_code = SerializeEx(database->reflection_manager, asset_string, asset, path, &serialize_options);

			SetSerializeCustomMaterialAssetDatabase((AssetDatabase*)nullptr);

			if (serialize_code == ECS_SERIALIZE_OK) {
				// If the renamed file exists, delete it
				if (renamed_file.size > 0) {
					// If it fails, then we cannot report to the user
					RemoveFile(renamed_file);
				}
				return true;
			}

			// Restore the renamed file
			if (renamed_file.size > 0) {
				// If it fails, then we cannot report to the user
				RenameFileAbsolute(renamed_file, path);
			}
			return false;
		}
		return true;
	}

	bool AssetDatabase::WriteMeshFile(const MeshMetadata* metadata) const
	{
		return WriteAssetFileImpl(this, metadata, STRING(MeshMetadata), ECS_ASSET_MESH);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::WriteTextureFile(const TextureMetadata* metadata) const
	{
		return WriteAssetFileImpl(this, metadata, STRING(TextureMetadata), ECS_ASSET_TEXTURE);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::WriteGPUSamplerFile(const GPUSamplerMetadata* metadata) const
	{
		return WriteAssetFileImpl(this, metadata, STRING(GPUSamplerMetadata), ECS_ASSET_GPU_SAMPLER);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::WriteShaderFile(const ShaderMetadata* metadata) const
	{
		return WriteAssetFileImpl(this, metadata, STRING(ShaderMetadata), ECS_ASSET_SHADER);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::WriteMaterialFile(const MaterialAsset* metadata) const
	{
		return WriteAssetFileImpl(this, metadata, STRING(MaterialAsset), ECS_ASSET_MATERIAL);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::WriteMiscFile(const MiscAsset* metadata) const
	{
		return WriteAssetFileImpl(this, metadata, STRING(MiscAsset), ECS_ASSET_MISC);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::WriteAssetFile(const void* asset, ECS_ASSET_TYPE type) const
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return WriteMeshFile((const MeshMetadata*)asset);
		case ECS_ASSET_TEXTURE:
			return WriteTextureFile((const TextureMetadata*)asset);
		case ECS_ASSET_GPU_SAMPLER:
			return WriteGPUSamplerFile((const GPUSamplerMetadata*)asset);
		case ECS_ASSET_SHADER:
			return WriteShaderFile((const ShaderMetadata*)asset);
		case ECS_ASSET_MATERIAL:
			return WriteMaterialFile((const MaterialAsset*)asset);
		case ECS_ASSET_MISC:
			return WriteMiscFile((const MiscAsset*)asset);
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		// Shouldn't be reached
		return false;
	}

	Stream<AssetDatabaseSameTargetAsset> AssetDatabase::SameTargetMeshes(AllocatorPolymorphic allocator) const
	{
		return GetSameTargetAssets(allocator, mesh_metadata);
	}

	// --------------------------------------------------------------------------------------

	Stream<AssetDatabaseSameTargetAsset> AssetDatabase::SameTargetTextures(AllocatorPolymorphic allocator) const
	{
		return GetSameTargetAssets(allocator, texture_metadata);
	}

	// --------------------------------------------------------------------------------------

	Stream<AssetDatabaseSameTargetAsset> AssetDatabase::SameTargetShaders(AllocatorPolymorphic allocator) const
	{
		return GetSameTargetAssets(allocator, shader_metadata);
	}

	// --------------------------------------------------------------------------------------

	Stream<AssetDatabaseSameTargetAsset> AssetDatabase::SameTargetMiscs(AllocatorPolymorphic allocator) const
	{
		return GetSameTargetAssets(allocator, misc_asset);
	}

	// --------------------------------------------------------------------------------------

	AssetDatabaseSameTargetAll AssetDatabase::SameTargetAll(AllocatorPolymorphic allocator) const
	{
		AssetDatabaseSameTargetAll all;

		all.meshes = SameTargetMeshes(allocator);
		all.textures = SameTargetTextures(allocator);
		all.shaders = SameTargetShaders(allocator);
		all.miscs = SameTargetMiscs(allocator);

		return all;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::ExtractNameFromFile(Stream<wchar_t> path, CapacityStream<char>& name)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, wide_name, 512);
		ExtractNameFromFileWide(path, wide_name);
		ConvertWideCharsToASCII(wide_name, name);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::ExtractNameFromFileWide(Stream<wchar_t> path, CapacityStream<wchar_t>& name)
	{
		Stream<wchar_t> filename = PathFilename(path);
		Stream<wchar_t> separator = FindFirstToken(filename, PATH_SEPARATOR);
		if (separator.size == 0) {
			// No separator, copy the entire filename
			name.CopyOther(PathStem(filename));
		}
		else {
			// Copy everything after the separator except the extension
			size_t size = wcslen(PATH_SEPARATOR);
			separator.buffer += size;
			separator.size -= size;

			Stream<wchar_t> wide_name = PathStem(separator);
			name.CopyOther(wide_name);
		}

		// If it contains relative path separators, put them back
		ReplaceCharacter(name, TEXT(SEPARATOR_CHAR), ECS_OS_PATH_SEPARATOR_REL);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::ExtractFileFromFile(Stream<wchar_t> path, CapacityStream<wchar_t>& file)
	{
		Stream<wchar_t> filename = PathFilename(path);
		Stream<wchar_t> separator = FindFirstToken(filename, PATH_SEPARATOR);
		if (separator.size == 0) {
			// No file
			return;
		}
		Stream<wchar_t> file_slice = { filename.buffer, (unsigned int)(separator.buffer - filename.buffer) };

		if (file_slice.size > 3) {
			// If the first character is drive letter with a pair of separators and a colon then convert to an absolute path
			if ((file_slice[0] >= L'C' && file_slice[0] <= L'Z') && file_slice[1] == SEPARATOR_CHAR && file_slice[2] == COLON_CHAR_REPLACEMENT && file_slice[3] == SEPARATOR_CHAR) {
				file.Add(file_slice[0]);
				file.Add(L':');
				file.AddStreamSafe({ file_slice.buffer + 4, file_slice.size - 4 });
				ReplaceCharacter(file, Character<wchar_t>(SEPARATOR_CHAR), ECS_OS_PATH_SEPARATOR);
			}
			else {
				file.AddStreamSafe(file_slice);
				// Relative path, replace with '\'
				ReplaceCharacter(file, Character<wchar_t>(SEPARATOR_CHAR), ECS_OS_PATH_SEPARATOR_REL);
			}
		}
		else {
			// Can only be a relative path
			file.AddStreamSafe(file_slice);
			ReplaceCharacter(file, Character<wchar_t>(SEPARATOR_CHAR), ECS_OS_PATH_SEPARATOR_ASCII_REL);
		}
	}

	// --------------------------------------------------------------------------------------

	Stream<wchar_t> AssetDatabase::ExtractAssetTargetFromFile(Stream<wchar_t> path, Stream<wchar_t> base_path, CapacityStream<wchar_t>& target_path)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, file, 512);
		ExtractFileFromFile(path, file);
		if (file.size == 0) {
			ExtractNameFromFileWide(path, file);
			ReplaceCharacter(file, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
		}
		return MountPathOnlyRel(file, base_path, target_path);
	}

	// --------------------------------------------------------------------------------------

	Stream<wchar_t> AssetDatabase::ExtractAssetTargetFromFile(Stream<wchar_t> path, CapacityStream<wchar_t>& target_path) {
		unsigned int previous_size = target_path.size;
		ExtractFileFromFile(path, target_path);
		if (previous_size == target_path.size) {
			ExtractNameFromFileWide(path, target_path);
		}
		return target_path;
	}

	// --------------------------------------------------------------------------------------

	ECS_SERIALIZE_CODE SerializeAssetDatabase(const AssetDatabase* database, WriteInstrument* write_instrument)
	{
		ECS_STACK_CAPACITY_STREAM(SerializeOmitField, omit_fields, 64);
		Stream<char> fields_to_keep[] = {
			STRING(name),
			STRING(file)
		};
		GetSerializeOmitFieldsFromExclude(database->reflection_manager, omit_fields, STRING(MeshMetadata), { fields_to_keep, ECS_COUNTOF(fields_to_keep) });
		GetSerializeOmitFieldsFromExclude(database->reflection_manager, omit_fields, STRING(TextureMetadata), { fields_to_keep, ECS_COUNTOF(fields_to_keep) });
		GetSerializeOmitFieldsFromExclude(database->reflection_manager, omit_fields, STRING(GPUSamplerMetadata), { fields_to_keep, ECS_COUNTOF(fields_to_keep) });
		GetSerializeOmitFieldsFromExclude(database->reflection_manager, omit_fields, STRING(ShaderMetadata), { fields_to_keep, ECS_COUNTOF(fields_to_keep) });
		GetSerializeOmitFieldsFromExclude(database->reflection_manager, omit_fields, STRING(MiscAsset), { fields_to_keep, ECS_COUNTOF(fields_to_keep) });

		// The material asset is not reflected - the omits must not be listed

		SerializeOptions options;
		options.omit_fields = omit_fields;

		SetSerializeCustomMaterialAssetDatabase(database);
		ECS_SERIALIZE_CODE return_value = Serialize(database->reflection_manager, database->reflection_manager->GetType(STRING(AssetDatabase)), database, write_instrument, &options);
		ClearSerializeCustomTypeUserData(Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET);

		return return_value;
	}

	// --------------------------------------------------------------------------------------

	ECS_DESERIALIZE_CODE DeserializeAssetDatabase(AssetDatabase* database, ReadInstrument* read_instrument, DeserializeAssetDatabaseOptions options)
	{
		DeserializeOptions deserialize_options;
		deserialize_options.file_allocator = database->Allocator();
		deserialize_options.field_allocator = database->Allocator();

		SetSerializeCustomMaterialAssetDatabase(database);
		SetSerializeCustomMaterialDoNotIncrementDependencies(true);

		ECS_DESERIALIZE_CODE code = Deserialize(database->reflection_manager, database->reflection_manager->GetType(STRING(AssetDatabase)), database, read_instrument, &deserialize_options);

		ClearSerializeCustomTypeUserData(Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET);
		SetSerializeCustomMaterialDoNotIncrementDependencies(false);

		if (options.default_initialize_other_fields) {
			ECS_ASSET_TYPE current_type = ECS_ASSET_MESH;
			auto basic_functor = [&](unsigned int handle) {
				void* asset = database->GetAsset(handle, current_type);
				Stream<wchar_t> path = GetAssetFile(asset, current_type);
				Stream<char> name = GetAssetName(asset, current_type);
				CreateDefaultAsset(asset, name, path, current_type);
				SetAssetToMetadata(asset, current_type, { nullptr, 0 });
			};

			ECS_ASSET_TYPE basic_functor_types[] = {
				ECS_ASSET_MESH,
				ECS_ASSET_TEXTURE,
				ECS_ASSET_GPU_SAMPLER,
				ECS_ASSET_SHADER,
				ECS_ASSET_MISC
			};

			ForEach(basic_functor_types, [&](ECS_ASSET_TYPE type) {
				current_type = type;
				database->ForEachAsset(current_type, basic_functor);
			});

			// For materials we need to keep all the related information like shader handles,
			// texture handles, reflection manager. Only the pointer needs to be set to nullptr
			database->ForEachAsset(ECS_ASSET_MATERIAL, [&](unsigned int handle) {
				MaterialAsset* material = database->GetMaterial(handle);
				material->material_pointer = nullptr;
			});
		}

		return code;
	}

	// --------------------------------------------------------------------------------------

}
