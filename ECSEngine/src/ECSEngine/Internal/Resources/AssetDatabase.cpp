#include "ecspch.h"
#include "AssetDatabase.h"
#include "../../Utilities/File.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "../../Utilities/Serialization/Binary/Serialization.h"
#include "../../Utilities/Reflection/Reflection.h"
#include "../../Utilities/Path.h"
#include "../../Allocators/ResizableLinearAllocator.h"
#include "../../Utilities/ForEachFiles.h"

#include "AssetMetadataSerialize.h"

#define EXTENSION L".meta"
#define PATH_SEPARATOR L"___"

#define SEPARATOR_CHAR '~'

namespace ECSEngine {

	// --------------------------------------------------------------------------------------

	void AssetDatabaseFileDirectory(Stream<wchar_t> file_location, CapacityStream<wchar_t>& path, ECS_ASSET_TYPE type)
	{
		path.Copy(file_location);
		function::ConvertASCIIToWide(path, Stream<char>(ConvertAssetTypeString(type)));
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
	unsigned int AddAssetDefault(AssetDatabase* database, AssetSet& set, Stream<char> name, Stream<wchar_t> file, const char* serialize_string, ECS_ASSET_TYPE type, bool* loaded_now) {
		unsigned int handle = database->FindAsset(name, file, type);

		if (handle == -1) {
			AssetType metadata;

			if (loaded_now != nullptr) {
				*loaded_now = true;
			}

			name = function::StringCopy(database->Allocator(), name);
			if (file.size > 0) {
				file = function::StringCopy(database->Allocator(), file);
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
		return AddAssetDefault<MeshMetadata>(this, mesh_metadata, name, file, STRING(MeshMetadata), ECS_ASSET_MESH, loaded_now);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMeshInternal(const MeshMetadata* metadata)
	{
		return mesh_metadata.Add({ *metadata, 1 });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddTexture(Stream<char> name, Stream<wchar_t> file, bool* loaded_now)
	{
		return AddAssetDefault<TextureMetadata>(this, texture_metadata, name, file, STRING(TextureMetadata), ECS_ASSET_TEXTURE, loaded_now);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddTextureInternal(const TextureMetadata* metadata)
	{
		return texture_metadata.Add({ *metadata, 1 });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddGPUSampler(Stream<char> name, bool* loaded_now)
	{
		return AddAssetDefault<GPUSamplerMetadata>(this, gpu_sampler_metadata, name, {}, STRING(GPUSamplerMetadata), ECS_ASSET_GPU_SAMPLER, loaded_now);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddGPUSamplerInternal(const GPUSamplerMetadata* metadata)
	{
		return gpu_sampler_metadata.Add({ *metadata, 1 });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddShader(Stream<char> name, Stream<wchar_t> file, bool* loaded_now)
	{
		return AddAssetDefault<ShaderMetadata>(this, shader_metadata, name, file, STRING(ShaderMetadata), ECS_ASSET_SHADER, loaded_now);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddShaderInternal(const ShaderMetadata* metadata)
	{
		return shader_metadata.Add({ *metadata, 1 });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMaterial(Stream<char> name, bool* loaded_now)
	{
		unsigned int handle = FindMaterial(name);

		if (handle == -1) {
			MaterialAsset asset;

			if (loaded_now != nullptr) {
				*loaded_now = true;
			}

			memset(&asset, 0, sizeof(asset));
			ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
			FileLocationMaterial(name, path);

			if (ExistsFileOrFolder(path)) {
				const size_t STACK_ALLOCATION_CAPACITY = ECS_KB * 4;
				void* stack_allocation = ECS_STACK_ALLOC(STACK_ALLOCATION_CAPACITY);
				LinearAllocator linear_allocator(stack_allocation, STACK_ALLOCATION_CAPACITY);

				DeserializeOptions options;
				options.backup_allocator = GetAllocatorPolymorphic(&linear_allocator);
				options.file_allocator = options.backup_allocator;
				ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, reflection_manager->GetType(STRING(MaterialAsset)), &asset, path, &options);
				if (code == ECS_DESERIALIZE_OK) {
					MaterialAsset allocated_asset = asset.Copy(material_asset.allocator);
					// The file allocation doesn't need to be deallocated since its allocated from the stack
					return material_asset.Add({ allocated_asset, 1 });
				}
				else {
					return -1;
				}
			}
			else {
				name = function::StringCopy(material_asset.allocator, name);
				asset.Default(name, {});
				WriteMaterialFile(&asset);
				return material_asset.Add({ asset, 1 });
			}
		}
		else {
			if (loaded_now != nullptr) {
				*loaded_now = false;
			}

			material_asset[handle].reference_count++;
			return handle;
		}
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMaterialInternal(const MaterialAsset* metadata)
	{
		return material_asset.Add({ *metadata, 1 });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMisc(Stream<char> name, Stream<wchar_t> file, bool* loaded_now)
	{
		return AddAssetDefault<MiscAsset>(this, misc_asset, name, file, STRING(MiscAsset), ECS_ASSET_MISC, loaded_now);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMiscInternal(const MiscAsset* metadata)
	{
		return misc_asset.Add({ *metadata, 1 });
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

	typedef unsigned int (AssetDatabase::* AddAssetInternalFunction)(const void* buffer);

	AddAssetInternalFunction ADD_ASSET_INTERNAL_FUNCTION[] = {
		(AddAssetInternalFunction)&AssetDatabase::AddMeshInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddTextureInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddGPUSamplerInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddShaderInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddMaterialInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddMiscInternal
	};

	unsigned int AssetDatabase::AddAssetInternal(const void* asset, ECS_ASSET_TYPE type)
	{
		return (this->*ADD_ASSET_INTERNAL_FUNCTION[type])(asset);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::AddAsset(unsigned int handle, ECS_ASSET_TYPE type)
	{
		switch (type) {
		case ECS_ASSET_MESH:
			mesh_metadata[handle].reference_count++;
			break;
		case ECS_ASSET_TEXTURE:
			texture_metadata[handle].reference_count++;
			break;
		case ECS_ASSET_GPU_SAMPLER:
			gpu_sampler_metadata[handle].reference_count++;
			break;
		case ECS_ASSET_SHADER:
			shader_metadata[handle].reference_count++;
			break;
		case ECS_ASSET_MATERIAL:
			material_asset[handle].reference_count++;
			break;
		case ECS_ASSET_MISC:
			misc_asset[handle].reference_count++;
			break;
		}
	}

	// --------------------------------------------------------------------------------------

	AllocatorPolymorphic AssetDatabase::Allocator() const
	{
		return mesh_metadata.allocator;
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
				result.AddAssetInternal(database->GetAssetConst(stream[index], type), type);
			}
		};

		copy_stream(result.mesh_metadata, ECS_ASSET_MESH);
		copy_stream(result.texture_metadata, ECS_ASSET_TEXTURE);
		copy_stream(result.gpu_sampler_metadata, ECS_ASSET_GPU_SAMPLER);
		copy_stream(result.shader_metadata, ECS_ASSET_SHADER);
		copy_stream(result.material_asset, ECS_ASSET_MATERIAL);
		copy_stream(result.misc_asset, ECS_ASSET_MISC);

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

	unsigned int AssetDatabase::FindMesh(Stream<char> name, Stream<wchar_t> file) const
	{
		return mesh_metadata.FindFunctor([&](const ReferenceCounted<MeshMetadata>& compare) {
			return function::CompareStrings(compare.value.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindTexture(Stream<char> name, Stream<wchar_t> file) const
	{
		return texture_metadata.FindFunctor([&](const ReferenceCounted<TextureMetadata>& compare) {
			return function::CompareStrings(compare.value.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindGPUSampler(Stream<char> name) const
	{
		return gpu_sampler_metadata.FindFunctor([&](const ReferenceCounted<GPUSamplerMetadata>& compare) {
			return function::CompareStrings(compare.value.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindShader(Stream<char> name, Stream<wchar_t> file) const
	{
		return shader_metadata.FindFunctor([&](const ReferenceCounted<ShaderMetadata>& compare) {
			return function::CompareStrings(compare.value.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMaterial(Stream<char> name) const
	{
		return material_asset.FindFunctor([&](const ReferenceCounted<MaterialAsset>& compare) {
			return function::CompareStrings(compare.value.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMisc(Stream<char> name, Stream<wchar_t> file) const
	{
		return misc_asset.FindFunctor([&](const ReferenceCounted<MiscAsset>& compare) {
			return function::CompareStrings(compare.value.name, name);
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

	// Add types can be compared using a bit check.
	template<typename SparseSet>
	unsigned int FindAssetExImplementation(const SparseSet* set, const void* pointer) {
		auto stream = set->ToStream();
		for (size_t index = 0; index < stream.size; index++) {
			if (stream[index].value.Pointer() == pointer) {
				return set->GetHandleFromIndex(index);
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMeshEx(const CoallescedMesh* mesh) const
	{
		return FindAssetExImplementation(&mesh_metadata, mesh);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindTextureEx(ResourceView resource_view) const
	{
		return FindAssetExImplementation(&texture_metadata, resource_view.Interface());
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindGPUSamplerEx(SamplerState sampler_state) const
	{
		return FindAssetExImplementation(&gpu_sampler_metadata, sampler_state.Interface());
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindShaderEx(const void* shader_interface) const
	{
		return FindAssetExImplementation(&shader_metadata, &shader_interface);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMaterialEx(const Material* material) const
	{
		return FindAssetExImplementation(&material_asset, material);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMiscEx(Stream<void> data) const
	{
		return FindAssetExImplementation(&misc_asset, data.buffer);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindAssetEx(Stream<void> data, ECS_ASSET_TYPE type) const
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return FindMeshEx((const CoallescedMesh*)data.buffer);
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
		if (file.size > 0) {
			// Place the file first separated from the name by 3 _
			// Also replace the '/' and '\\' from the file with '_'
			ECS_STACK_CAPACITY_STREAM(wchar_t, temp_file, 512);
			temp_file.Copy(file);
			function::ReplaceCharacter(temp_file, ECS_OS_PATH_SEPARATOR_ASCII, SEPARATOR_CHAR);
			function::ReplaceCharacter(temp_file, ECS_OS_PATH_SEPARATOR_ASCII_REL, SEPARATOR_CHAR);
			path.AddStream(temp_file);
			path.AddStream(PATH_SEPARATOR);
		}
		function::ConvertASCIIToWide(path, name);
		path.AddStreamSafe(EXTENSION);
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
				Stream<wchar_t>* current_path = (Stream<wchar_t>*)function::OffsetPointer(&stream[index].value, sizeof(Stream<char>));
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
		return *(Stream<char>*)GetAssetConst(handle, type);
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
			if (function::CompareStrings(file, current_file)) {
				handles.AddSafe(handle);
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
			if (function::CompareStrings(data->file, asset_file)) {
				ECS_STACK_CAPACITY_STREAM(char, asset_name, 512);
				AssetDatabase::ExtractNameFromFile(file, asset_name);
				data->stream->Add(function::StringCopy(data->stream->allocator, asset_name));
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

			Stream<wchar_t> filename = function::PathFilename(file);
			data->stream->Add(function::StringCopy(data->stream->allocator, filename));
			return true;
		};

		FunctorData data = { &stream };
		ForEachFileInDirectory(folder, &data, functor);
		return stream;
	}

	// --------------------------------------------------------------------------------------

	template<typename AssetType>
	bool RemoveAssetImpl(AssetType& type, unsigned int handle) {
		auto& metadata = type[handle];
		metadata.reference_count--;

		if (metadata.reference_count == 0) {
			// Deallocate the memory
			metadata.value.DeallocateMemory(type.allocator);
			type.RemoveSwapBack(handle);
			return true;
		}
		return false;
	}

	// --------------------------------------------------------------------------------------

	bool ReadAssetFileImpl(const AssetDatabase* database, Stream<char> name, Stream<wchar_t> file, void* asset, const char* asset_string, ECS_ASSET_TYPE asset_type) {
		if (database->metadata_file_location.size > 0) {
			ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
			database->FileLocationAsset(name, file, path, asset_type);

			ECS_DESERIALIZE_CODE serialize_code = Deserialize(database->reflection_manager, database->reflection_manager->GetType(asset_string), asset, path);
			if (serialize_code == ECS_DESERIALIZE_OK) {
				return true;
			}
			return false;
		}
		return true;
	}

	bool AssetDatabase::ReadMeshFile(Stream<char> name, Stream<wchar_t> file, MeshMetadata* metadata) const {
		return ReadAssetFileImpl(this, name, file, metadata, STRING(MeshMetadata), ECS_ASSET_MESH);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadTextureFile(Stream<char> name, Stream<wchar_t> file, TextureMetadata* metadata) const {
		return ReadAssetFileImpl(this, name, file, metadata, STRING(TextureMetadata), ECS_ASSET_TEXTURE);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadGPUSamplerFile(Stream<char> name, GPUSamplerMetadata* metadata) const {
		return ReadAssetFileImpl(this, name, {}, metadata, STRING(GPUSamplerMetadata), ECS_ASSET_GPU_SAMPLER);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadShaderFile(Stream<char> name, Stream<wchar_t> file, ShaderMetadata* metadata) const {
		return ReadAssetFileImpl(this, name, file, metadata, STRING(ShaderMetadata), ECS_ASSET_MESH);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadMaterialFile(Stream<char> name, MaterialAsset* asset) const {
		return ReadAssetFileImpl(this, name, {}, asset, STRING(MaterialAsset), ECS_ASSET_MATERIAL);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadMiscFile(Stream<char> name, Stream<wchar_t> file, MiscAsset* asset) const {
		// At the moment there is nothing to be read from the misc asset
		return true;
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::ReadAssetFile(Stream<char> name, Stream<wchar_t> file, void* metadata, ECS_ASSET_TYPE asset_type) const {
		switch (asset_type) {
		case ECS_ASSET_MESH:
		{
			return ReadMeshFile(name, file, (MeshMetadata*)metadata);
		}
		case ECS_ASSET_TEXTURE:
		{
			return ReadTextureFile(name, file, (TextureMetadata*)metadata);
		}
		case ECS_ASSET_GPU_SAMPLER:
		{
			return ReadGPUSamplerFile(name, (GPUSamplerMetadata*)metadata);
		}
		case ECS_ASSET_SHADER:
		{
			return ReadShaderFile(name, file, (ShaderMetadata*)metadata);
		}
		case ECS_ASSET_MATERIAL:
		{
			return ReadMaterialFile(name, (MaterialAsset*)metadata);
		}
		case ECS_ASSET_MISC:
		{
			return ReadMiscFile(name, file, (MiscAsset*)metadata);
		}
		default:
			ECS_ASSERT(false, "Invalid asset type.");
		}
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMesh(unsigned int handle)
	{
		return RemoveAssetImpl(mesh_metadata, handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveTexture(unsigned int handle)
	{
		return RemoveAssetImpl(texture_metadata, handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveGPUSampler(unsigned int handle)
	{
		return RemoveAssetImpl(gpu_sampler_metadata, handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveShader(unsigned int handle)
	{
		return RemoveAssetImpl(shader_metadata, handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMaterial(unsigned int handle)
	{
		return RemoveAssetImpl(material_asset, handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMisc(unsigned int handle)
	{
		return RemoveAssetImpl(misc_asset, handle);
	}

	// --------------------------------------------------------------------------------------

	typedef bool (AssetDatabase::* RemoveFunctionHandle)(unsigned int handle);

	RemoveFunctionHandle REMOVE_FUNCTIONS_HANDLE[] = {
		&AssetDatabase::RemoveMesh,
		&AssetDatabase::RemoveTexture,
		&AssetDatabase::RemoveGPUSampler,
		&AssetDatabase::RemoveShader,
		&AssetDatabase::RemoveMaterial,
		&AssetDatabase::RemoveMisc
	};

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveAsset(unsigned int handle, ECS_ASSET_TYPE type)
	{
		return (this->*REMOVE_FUNCTIONS_HANDLE[type])(handle);
	}

	// --------------------------------------------------------------------------------------

	template<typename AssetType>
	void RemoveAssetForcedImpl(AssetType& type, unsigned int handle) {
		auto& metadata = type[handle];

		// Deallocate the memory
		metadata.value.DeallocateMemory(type.allocator);
		type.RemoveSwapBack(handle);
	}

	// Does not destroy the file. It will remove the asset no matter what the reference count is
	void AssetDatabase::RemoveAssetForced(unsigned int handle, ECS_ASSET_TYPE type) {
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
			metadata_file_location.size = _file_location.size + 1;

			if (function::PathIsAbsolute(_file_location)) {
				if (metadata_file_location[metadata_file_location.size - 1] != ECS_OS_PATH_SEPARATOR) {
					metadata_file_location[metadata_file_location.size - 1] = ECS_OS_PATH_SEPARATOR;
				}
			}
			else {
				if (metadata_file_location[metadata_file_location.size - 1] != ECS_OS_PATH_SEPARATOR_REL) {
					metadata_file_location[metadata_file_location.size - 1] = ECS_OS_PATH_SEPARATOR_REL;
				}
			}

			metadata_file_location[metadata_file_location.size] = L'\0';
		}
		else {
			metadata_file_location = { nullptr, 0 };
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
				handles.AddSafe(sparse.GetHandleFromIndex(index));
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
		Stream<char> previous_name;
		Stream<wchar_t> previous_file;
		if (update_files) {
			previous_name = database->GetAssetName(handle, type);
			previous_file = database->GetAssetPath(handle, type);
		}

		T* old = (T*)database->GetAsset(handle, type);
		old->DeallocateMemory(database->Allocator());
		*old = ((T*)metadata)->Copy(database->Allocator());

		if (update_files) {
			// Get the update name and file
			Stream<char> new_name = database->GetAssetName(handle, type);
			Stream<wchar_t> new_file = database->GetAssetPath(handle, type);

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
				RemoveFile(renamed_file);
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

	template<typename MetadataSparse>
	Stream<AssetDatabaseSameTargetAsset> GetSameTargetAssets(AllocatorPolymorphic allocator, MetadataSparse sparse) {
		auto data = sparse.ToStream();
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(AssetDatabaseSameTargetAsset, temp_assets, data.size);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, indices_to_check, data.size);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, current_asset_same, data.size);

		indices_to_check.size = data.size;
		function::MakeSequence(indices_to_check);

		// The indices_to_check will contain only indices that are actually valid
		for (unsigned int index = 0; index < indices_to_check.size; index++) {
			unsigned int current_index = indices_to_check[0];
			unsigned int add_index = temp_assets.Add({ sparse.GetHandleFromIndex(current_index), { nullptr, 0 } });

			// Determine the count of the other assets with the same target
			current_asset_same.size = 0;
			current_asset_same.Add(current_index);

			for (unsigned int subindex = 1; subindex < indices_to_check.size; subindex++) {
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
	bool WriteAssetFileImpl(const AssetDatabase* database, const AssetType* asset, const char* asset_string, ECS_ASSET_TYPE asset_type) {
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

			ECS_SERIALIZE_CODE serialize_code = Serialize(database->reflection_manager, database->reflection_manager->GetType(asset_string), asset, path);
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

	typedef bool (AssetDatabase::* WriteAssetFileFunction)(const void* metadata) const;

	WriteAssetFileFunction WRITE_ASSET_FILE[] = {
		(WriteAssetFileFunction)&AssetDatabase::WriteMeshFile,
		(WriteAssetFileFunction)&AssetDatabase::WriteTextureFile,
		(WriteAssetFileFunction)&AssetDatabase::WriteGPUSamplerFile,
		(WriteAssetFileFunction)&AssetDatabase::WriteShaderFile,
		(WriteAssetFileFunction)&AssetDatabase::WriteMaterialFile,
		(WriteAssetFileFunction)&AssetDatabase::WriteMiscFile
	};

	bool AssetDatabase::WriteAssetFile(const void* asset, ECS_ASSET_TYPE type) const
	{
		return (this->*WRITE_ASSET_FILE[type])(asset);
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
		Stream<wchar_t> filename = function::PathFilename(path);
		Stream<wchar_t> separator = function::FindFirstToken(filename, PATH_SEPARATOR);
		if (separator.size == 0) {
			// No separator, copy the entire filename
			function::ConvertWideCharsToASCII(filename, name);
		}
		else {
			// Copy everything after the separator except the extension
			size_t size = wcslen(PATH_SEPARATOR);
			separator.buffer += size;
			separator.size -= size;

			Stream<wchar_t> wide_name = function::PathStem(separator);
			function::ConvertWideCharsToASCII(wide_name, name);
		}
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::ExtractFileFromFile(Stream<wchar_t> path, CapacityStream<wchar_t>& file)
	{
		Stream<wchar_t> filename = function::PathFilename(path);
		Stream<wchar_t> separator = function::FindFirstToken(filename, PATH_SEPARATOR);
		if (separator.size == 0) {
			// No file
			return;
		}
		file.Copy({ filename.buffer, (unsigned int)(separator.buffer - filename.buffer) });
		// If the first character is drive letter, then convert to an absolute path
		if ((file[0] >= L'C' || file[0] <= L'Z') && file[1] == L':') {
			function::ReplaceCharacter(file, function::Character<wchar_t>(SEPARATOR_CHAR), ECS_OS_PATH_SEPARATOR);
		}
		else {
			// Relative path, replace with '\'
			function::ReplaceCharacter(file, function::Character<wchar_t>(SEPARATOR_CHAR), ECS_OS_PATH_SEPARATOR_REL);
		}
	}

	// --------------------------------------------------------------------------------------

	template<typename Functor>
	size_t SerializeAssetDatabaseImpl(const AssetDatabase* database, Functor&& functor) {
		ECS_STACK_CAPACITY_STREAM(SerializeOmitField, omit_fields, 64);
		Stream<char> fields_to_keep[] = {
			STRING(name),
			STRING(path)
		};
		GetSerializeOmitFieldsFromExclude(database->reflection_manager, omit_fields, STRING(MeshMetadata), { fields_to_keep, std::size(fields_to_keep) });
		GetSerializeOmitFieldsFromExclude(database->reflection_manager, omit_fields, STRING(TextureMetadata), { fields_to_keep, std::size(fields_to_keep) });
		GetSerializeOmitFieldsFromExclude(database->reflection_manager, omit_fields, STRING(GPUSamplerMetadata), { fields_to_keep, std::size(fields_to_keep) });
		GetSerializeOmitFieldsFromExclude(database->reflection_manager, omit_fields, STRING(ShaderMetadata), { fields_to_keep, std::size(fields_to_keep) });

		SerializeOptions options;
		options.omit_fields = omit_fields;

		SetSerializeCustomMaterialAssetDatabase(database);
		size_t return_value = functor(&options);
		ClearSerializeCustomTypeUserData(Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET);

		return return_value;
	}

	ECS_SERIALIZE_CODE SerializeAssetDatabase(const AssetDatabase* database, Stream<wchar_t> file)
	{
		return (ECS_SERIALIZE_CODE)SerializeAssetDatabaseImpl(database, [&](SerializeOptions* options) {
			return (size_t)Serialize(database->reflection_manager, database->reflection_manager->GetType(STRING(AssetDatabase)), database, file, options);
		});
	}

	// --------------------------------------------------------------------------------------

	ECS_SERIALIZE_CODE SerializeAssetDatabase(const AssetDatabase* database, uintptr_t& ptr)
	{
		return (ECS_SERIALIZE_CODE)SerializeAssetDatabaseImpl(database, [&](SerializeOptions* options) {
			return (size_t)Serialize(database->reflection_manager, database->reflection_manager->GetType(STRING(AssetDatabase)), database, ptr, options);
		});
	}

	// --------------------------------------------------------------------------------------

	size_t SerializeAssetDatabaseSize(const AssetDatabase* database)
	{
		return SerializeAssetDatabaseImpl(database, [&](SerializeOptions* options) {
			return SerializeSize(database->reflection_manager, database->reflection_manager->GetType(STRING(AssetDatabase)), database, options);
		});
	}

	// --------------------------------------------------------------------------------------

	template<typename Functor>
	ECS_DESERIALIZE_CODE DeserializeAssetDatabaseImpl(AssetDatabase* database, bool reference_count_zero, Functor&& functor) {
		reference_count_zero = !reference_count_zero;

		const size_t STACK_CAPACITY = ECS_KB * 128;
		void* stack_allocation = ECS_STACK_ALLOC(STACK_CAPACITY);

		ResizableLinearAllocator _allocator(stack_allocation, STACK_CAPACITY, ECS_MB, { nullptr });
		AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&_allocator);

		DeserializeOptions options;
		options.backup_allocator = allocator;
		options.file_allocator = allocator;
		options.field_allocator = allocator;

		SetSerializeCustomMaterialAssetDatabase(database);

		ECS_DESERIALIZE_CODE code = functor(&options);

		ClearSerializeCustomTypeUserData(Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET);
		// If it failed, just exit
		if (code != ECS_DESERIALIZE_OK) {
			_allocator.ClearBackup();
			return code;
		}

		// Allocate all the fields now
		// Make the reference count 0/1 for all asset types. Either the database references will update the reference count
		// or through some other way these are kept alive

		AllocatorPolymorphic database_allocator = database->Allocator();

		auto update_asset_type = [database_allocator, reference_count_zero](auto stream_type) {
			for (size_t index = 0; index < stream_type.size; index++) {
				stream_type[index].reference_count = reference_count_zero;
				stream_type[index].value = stream_type[index].value.Copy(database_allocator);
			}
		};

		update_asset_type(database->mesh_metadata.ToStream());
		update_asset_type(database->texture_metadata.ToStream());
		update_asset_type(database->gpu_sampler_metadata.ToStream());
		update_asset_type(database->shader_metadata.ToStream());
		update_asset_type(database->material_asset.ToStream());
		update_asset_type(database->misc_asset.ToStream());

		_allocator.ClearBackup();
		return code;
	}

	ECS_DESERIALIZE_CODE DeserializeAssetDatabase(AssetDatabase* database, Stream<wchar_t> file, bool reference_count_zero)
	{
		return DeserializeAssetDatabaseImpl(database, reference_count_zero, [&](DeserializeOptions* options) {
			return Deserialize(database->reflection_manager, database->reflection_manager->GetType(STRING(AssetDatabase)), database, file, options);
		});
	}

	// --------------------------------------------------------------------------------------

	ECS_DESERIALIZE_CODE DeserializeAssetDatabase(AssetDatabase* database, uintptr_t& ptr, bool reference_count_zero)
	{
		return DeserializeAssetDatabaseImpl(database, reference_count_zero, [&](DeserializeOptions* options) {
			return Deserialize(database->reflection_manager, database->reflection_manager->GetType(STRING(AssetDatabase)), database, ptr, options);
		});
	}

	// --------------------------------------------------------------------------------------

	size_t DeserializeAssetDatabaseSize(const Reflection::ReflectionManager* reflection_manager, uintptr_t ptr)
	{
		return DeserializeSize(reflection_manager, reflection_manager->GetType(STRING(AssetDatabase)), ptr);
	}

	// --------------------------------------------------------------------------------------

}
