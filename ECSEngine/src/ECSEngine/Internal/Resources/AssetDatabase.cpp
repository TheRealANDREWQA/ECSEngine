#include "ecspch.h"
#include "AssetDatabase.h"
#include "../../Utilities/File.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "../../Utilities/Serialization/Binary/Serialization.h"
#include "../../Utilities/Reflection/Reflection.h"
#include "../../Utilities/Path.h"
#include "../../Allocators/ResizableLinearAllocator.h"

#include "AssetMetadataSerialize.h"

#define MESH_FOLDER L"Mesh"
#define TEXTURE_FOLDER L"Texture"
#define GPU_BUFFER_FOLDER L"GPUBuffer"
#define GPU_SAMPLER_FOLDER L"GPUSampler"
#define SHADER_FOLDER L"Shader"
#define MATERIAL_FOLDER L"Material"
#define MISC_FOLDER L"Misc"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------

	AssetDatabase::AssetDatabase(AllocatorPolymorphic allocator,  const Reflection::ReflectionManager* _reflection_manager)
		: reflection_manager(_reflection_manager), file_location(nullptr, 0)
	{
		SetAllocator(allocator);
	}
	
	// --------------------------------------------------------------------------------------

	template<typename AssetType, typename AssetSet>
	unsigned int AddAssetDefault(AssetDatabase* database, AssetSet& set, Stream<char> name, const char* serialize_string, ECS_ASSET_TYPE type) {
		unsigned int handle = database->FindAsset(name, type);

		if (handle == -1) {
			AssetType metadata;

			name = function::StringCopy(database->Allocator(), name);
			metadata.Default(name);

			if (database->file_location.size > 0) {
				ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
				database->FileLocationAsset(path, type);

				if (ExistsFileOrFolder(path)) {
					DeserializeOptions options;
					options.field_allocator = set.allocator;
					ECS_DESERIALIZE_CODE code = Deserialize(database->reflection_manager, database->reflection_manager->GetType(serialize_string), &metadata, path, &options);
					if (code == ECS_DESERIALIZE_OK) {
						return set.Add({ metadata, 1 });
					}
					else {
						// Also deallocate the name
						Deallocate(database->Allocator(), name.buffer);
						return -1;
					}
				}
				else {
					return set.Add({ metadata, 1 });
				}
			}
			else {
				return set.Add({ metadata, 1 });
			}
		}
		else {
			set[handle].reference_count++;
			return handle;
		}
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMesh(Stream<char> name)
	{
		return AddAssetDefault<MeshMetadata>(this, mesh_metadata, name, STRING(MeshMetadata), ECS_ASSET_MESH);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMeshInternal(const MeshMetadata* metadata)
	{
		return mesh_metadata.Add({ *metadata, 1 });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddTexture(Stream<char> name)
	{
		return AddAssetDefault<TextureMetadata>(this, texture_metadata, name, STRING(TextureMetadata), ECS_ASSET_TEXTURE);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddTextureInternal(const TextureMetadata* metadata)
	{
		return texture_metadata.Add({ *metadata, 1 });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddGPUBuffer(Stream<char> name)
	{
		return AddAssetDefault<GPUBufferMetadata>(this, gpu_buffer_metadata, name, STRING(GPUBufferMetadata), ECS_ASSET_GPU_BUFFER);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddGPUBufferInternal(const GPUBufferMetadata* metadata)
	{
		return gpu_buffer_metadata.Add({ *metadata, 1 });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddGPUSampler(Stream<char> name)
	{
		return AddAssetDefault<GPUSamplerMetadata>(this, gpu_sampler_metadata, name, STRING(GPUSamplerMetadata), ECS_ASSET_GPU_SAMPLER);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddGPUSamplerInternal(const GPUSamplerMetadata* metadata)
	{
		return gpu_sampler_metadata.Add({ *metadata, 1 });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddShader(Stream<char> name)
	{
		return AddAssetDefault<ShaderMetadata>(this, shader_metadata, name, STRING(ShaderMetadata), ECS_ASSET_SHADER);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddShaderInternal(const ShaderMetadata* metadata)
	{
		return shader_metadata.Add({ *metadata, 1 });
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMaterial(Stream<char> name)
	{
		unsigned int handle = FindMaterial(name);

		if (handle == -1) {
			MaterialAsset asset;

			ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
			FileLocationMaterial(path);

			const size_t STACK_ALLOCATION_CAPACITY = ECS_KB * 4;
			void* stack_allocation = ECS_STACK_ALLOC(STACK_ALLOCATION_CAPACITY);
			LinearAllocator linear_allocator(stack_allocation, STACK_ALLOCATION_CAPACITY);

			DeserializeOptions options;
			options.backup_allocator = GetAllocatorPolymorphic(&linear_allocator);
			options.file_allocator = options.backup_allocator;
			ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, reflection_manager->GetType(STRING(MaterialAsset)), &asset, path, &options);
			if (code == ECS_DESERIALIZE_OK) {
				void* allocation = Allocate(material_asset.allocator, asset.CopySize());
				MaterialAsset allocated_asset = asset.Copy(allocation);

				// The file allocation doesn't need to be deallocated since its allocated from the stack
				return material_asset.Add({ allocated_asset, 1 });
			}
			else {
				return -1;
			}
		}
		else {
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

	unsigned int AssetDatabase::AddMisc(Stream<wchar_t> path)
	{
		return AddAssetDefault<MiscAsset>(this, misc_asset, Stream<char>(path.buffer, path.size), STRING(MiscAsset), ECS_ASSET_MISC);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddMiscInternal(const MiscAsset* metadata)
	{
		return misc_asset.Add({ *metadata, 1 });
	}

	// --------------------------------------------------------------------------------------

	typedef unsigned int (AssetDatabase::* AddAssetFunction)(Stream<char> name);

	AddAssetFunction ADD_ASSET_FUNCTION[] = {
		&AssetDatabase::AddMesh,
		&AssetDatabase::AddTexture,
		&AssetDatabase::AddGPUBuffer,
		&AssetDatabase::AddGPUSampler,
		&AssetDatabase::AddShader,
		&AssetDatabase::AddMaterial
	};

	unsigned int AssetDatabase::AddAsset(Stream<char> name, ECS_ASSET_TYPE type)
	{
		if (type == ECS_ASSET_MISC) {
			return AddMisc(Stream<wchar_t>(name.buffer, name.size));
		}
		return (this->*ADD_ASSET_FUNCTION[type])(name);
	}

	// --------------------------------------------------------------------------------------

	typedef unsigned int (AssetDatabase::* AddAssetInternalFunction)(const void* buffer);

	AddAssetInternalFunction ADD_ASSET_INTERNAL_FUNCTION[] = {
		(AddAssetInternalFunction)&AssetDatabase::AddMeshInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddTextureInternal,
		(AddAssetInternalFunction)&AssetDatabase::AddGPUBufferInternal,
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
		case ECS_ASSET_GPU_BUFFER:
			gpu_buffer_metadata[handle].reference_count++;
			break;
		case ECS_ASSET_GPU_SAMPLER:
			gpu_sampler_metadata[handle].reference_count++;
			break;
		case ECS_ASSET_SHADER:
			shader_metadata[handle].reference_count++;
			break;
		case ECS_ASSET_MATERIAL:
			material_asset[handle].reference_count++;
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

	template<typename AssetType>
	bool CreateAssetFileImpl(const AssetDatabase* database, const AssetType* asset, const char* asset_string, ECS_ASSET_TYPE asset_type) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
		database->FileLocationAsset(path, asset_type);

		ECS_SERIALIZE_CODE serialize_code = Serialize(database->reflection_manager, database->reflection_manager->GetType(asset_string), asset, path);
		if (serialize_code == ECS_SERIALIZE_OK) {
			return true;
		}
		return false;
	}

	bool AssetDatabase::CreateMeshFile(const MeshMetadata* metadata) const
	{
		return CreateAssetFileImpl(this, metadata, STRING(MeshMetadata), ECS_ASSET_MESH);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::CreateTextureFile(const TextureMetadata* metadata) const
	{
		return CreateAssetFileImpl(this, metadata, STRING(TextureMetadata), ECS_ASSET_TEXTURE);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::CreateGPUBufferFile(const GPUBufferMetadata* metadata) const
	{
		return CreateAssetFileImpl(this, metadata, STRING(GPUBufferMetadata), ECS_ASSET_GPU_BUFFER);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::CreateGPUSamplerFile(const GPUSamplerMetadata* metadata) const
	{
		return CreateAssetFileImpl(this, metadata, STRING(GPUSampler), ECS_ASSET_GPU_SAMPLER);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::CreateShaderFile(const ShaderMetadata* metadata) const
	{
		return CreateAssetFileImpl(this, metadata, STRING(ShaderMetadata), ECS_ASSET_SHADER);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::CreateMaterialFile(const MaterialAsset* metadata) const
	{
		return CreateAssetFileImpl(this, metadata, STRING(MaterialAsset), ECS_ASSET_MATERIAL);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::CreateMiscFile(const MiscAsset* metadata) const
	{
		return CreateAssetFileImpl(this, metadata, STRING(MiscAsset), ECS_ASSET_MISC);
	}

	// --------------------------------------------------------------------------------------

	typedef bool (AssetDatabase::* CreateAssetFileFunction)(const void* metadata) const;

	CreateAssetFileFunction CREATE_ASSET_FILE[] = {
		(CreateAssetFileFunction)&AssetDatabase::CreateMeshFile,
		(CreateAssetFileFunction)&AssetDatabase::CreateTextureFile,
		(CreateAssetFileFunction)&AssetDatabase::CreateGPUBufferFile,
		(CreateAssetFileFunction)&AssetDatabase::CreateGPUSamplerFile,
		(CreateAssetFileFunction)&AssetDatabase::CreateShaderFile,
		(CreateAssetFileFunction)&AssetDatabase::CreateMaterialFile,
		(CreateAssetFileFunction)&AssetDatabase::CreateMiscFile
	};

	bool AssetDatabase::CreateAssetFile(const void* asset, ECS_ASSET_TYPE type) const
	{
		return (this->*CREATE_ASSET_FILE[type])(asset);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMesh(Stream<char> name) const
	{
		return mesh_metadata.FindFunctor([&](const ReferenceCounted<MeshMetadata>& compare) {
			return function::CompareStrings(compare.value.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindTexture(Stream<char> name) const
	{
		return texture_metadata.FindFunctor([&](const ReferenceCounted<TextureMetadata>& compare) {
			return function::CompareStrings(compare.value.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindGPUBuffer(Stream<char> name) const
	{
		return gpu_buffer_metadata.FindFunctor([&](const ReferenceCounted<GPUBufferMetadata>& compare) {
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

	unsigned int AssetDatabase::FindShader(Stream<char> name) const
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

	unsigned int AssetDatabase::FindMisc(Stream<wchar_t> path) const
	{
		return misc_asset.FindFunctor([&](const ReferenceCounted<MiscAsset>& compare) {
			return function::CompareStrings(compare.value.path, path);
		});
	}

	// --------------------------------------------------------------------------------------

	typedef unsigned int (AssetDatabase::* FindAssetFunction)(Stream<char>) const;

	FindAssetFunction FIND_FUNCTIONS[] = {
		&AssetDatabase::FindMesh,
		&AssetDatabase::FindTexture,
		&AssetDatabase::FindGPUBuffer,
		&AssetDatabase::FindGPUSampler,
		&AssetDatabase::FindShader,
		&AssetDatabase::FindMaterial
	};

	unsigned int AssetDatabase::FindAsset(Stream<char> name, ECS_ASSET_TYPE type) const
	{
		if (type == ECS_ASSET_MISC) {
			return FindMisc(Stream<wchar_t>(name.buffer, name.size));
		}
		return (this->*FIND_FUNCTIONS[type])(name);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationMesh(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(MESH_FOLDER);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationTexture(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(TEXTURE_FOLDER);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationGPUBuffer(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(GPU_BUFFER_FOLDER);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationGPUSampler(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(GPU_SAMPLER_FOLDER);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationShader(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(SHADER_FOLDER);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationMaterial(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(MATERIAL_FOLDER);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationMisc(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(MISC_FOLDER);
	}

	// --------------------------------------------------------------------------------------

	typedef void (AssetDatabase::* FileLocationFunction)(CapacityStream<wchar_t>&) const;

	FileLocationFunction FILE_LOCATIONS_FUNCTIONS[] = {
		&AssetDatabase::FileLocationMesh,
		&AssetDatabase::FileLocationTexture,
		&AssetDatabase::FileLocationGPUBuffer,
		&AssetDatabase::FileLocationGPUSampler,
		&AssetDatabase::FileLocationShader,
		&AssetDatabase::FileLocationMaterial,
		&AssetDatabase::FileLocationMisc
	};

	void AssetDatabase::FileLocationAsset(CapacityStream<wchar_t>& path, ECS_ASSET_TYPE type) const
	{
		(this->*FILE_LOCATIONS_FUNCTIONS[type])(path);
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

	GPUBufferMetadata* AssetDatabase::GetGPUBuffer(unsigned int handle)
	{
		return (GPUBufferMetadata*)GetGPUBufferConst(handle);
	}

	// --------------------------------------------------------------------------------------

	const GPUBufferMetadata* AssetDatabase::GetGPUBufferConst(unsigned int handle) const
	{
		return &gpu_buffer_metadata[handle].value;
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
		(GetAssetFunction)&AssetDatabase::GetGPUBufferConst,
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

	bool AssetDatabase::RemoveMesh(Stream<char> name)
	{
		unsigned int handle = FindMesh(name);
		if (handle == -1) {
			return false;
		}

		return RemoveMesh(handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMesh(unsigned int handle)
	{
		return RemoveAssetImpl(mesh_metadata, handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveTexture(Stream<char> name)
	{
		unsigned int handle = FindTexture(name);
		if (handle == -1) {
			return false;
		}

		return RemoveTexture(handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveTexture(unsigned int handle)
	{
		return RemoveAssetImpl(texture_metadata, handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveGPUBuffer(Stream<char> name)
	{
		unsigned int handle = FindGPUBuffer(name);
		if (handle == -1) {
			return false;
		}

		return RemoveGPUBuffer(handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveGPUBuffer(unsigned int handle)
	{
		return RemoveAssetImpl(gpu_buffer_metadata, handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveGPUSampler(Stream<char> name)
	{
		unsigned int handle = FindGPUSampler(name);
		if (handle == -1) {
			return false;
		}

		return RemoveGPUSampler(handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveGPUSampler(unsigned int handle)
	{
		return RemoveAssetImpl(gpu_sampler_metadata, handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveShader(Stream<char> name)
	{
		unsigned int handle = FindShader(name);
		if (handle == -1) {
			return false;
		}

		return RemoveShader(handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveShader(unsigned int handle)
	{
		return RemoveAssetImpl(shader_metadata, handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMaterial(Stream<char> name)
	{
		unsigned int handle = FindMaterial(name);
		if (handle == -1) {
			return false;
		}

		return RemoveMaterial(handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMaterial(unsigned int handle)
	{
		return RemoveAssetImpl(material_asset, handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMisc(Stream<wchar_t> path)
	{
		unsigned int handle = FindMisc(path);
		if (handle == -1) {
			return false;
		}

		return RemoveMisc(handle);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMisc(unsigned int handle)
	{
		return RemoveAssetImpl(misc_asset, handle);
	}

	// --------------------------------------------------------------------------------------

	typedef bool (AssetDatabase::* RemoveFunctionName)(Stream<char> name);

	RemoveFunctionName REMOVE_FUNCTIONS_NAME[] = {
		&AssetDatabase::RemoveMesh,
		&AssetDatabase::RemoveTexture,
		&AssetDatabase::RemoveGPUBuffer,
		&AssetDatabase::RemoveGPUSampler,
		&AssetDatabase::RemoveShader,
		&AssetDatabase::RemoveMaterial,
	};

	typedef bool (AssetDatabase::* RemoveFunctionHandle)(unsigned int handle);

	RemoveFunctionHandle REMOVE_FUNCTIONS_HANDLE[] = {
		&AssetDatabase::RemoveMesh,
		&AssetDatabase::RemoveTexture,
		&AssetDatabase::RemoveGPUBuffer,
		&AssetDatabase::RemoveGPUSampler,
		&AssetDatabase::RemoveShader,
		&AssetDatabase::RemoveMaterial,
	};

	bool AssetDatabase::RemoveAsset(Stream<char> name, ECS_ASSET_TYPE type)
	{
		if (ECS_ASSET_MISC == type) {
			return RemoveMisc(Stream<wchar_t>(name.buffer, name.size));
		}
		return (this->*REMOVE_FUNCTIONS_NAME[type])(name);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveAsset(unsigned int handle, ECS_ASSET_TYPE type)
	{
		return (this->*REMOVE_FUNCTIONS_HANDLE[type])(handle);
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::SetAllocator(AllocatorPolymorphic allocator)
	{
		mesh_metadata.allocator = allocator;
		texture_metadata.allocator = allocator;
		gpu_buffer_metadata.allocator = allocator;
		gpu_sampler_metadata.allocator = allocator;
		shader_metadata.allocator = allocator;
		material_asset.allocator = allocator;
		misc_asset.allocator = allocator;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::SetFileLocation(Stream<wchar_t> _file_location)
	{
		if (file_location.buffer != nullptr) {
			Deallocate(mesh_metadata.allocator, file_location.buffer);
		}

		if (_file_location.size > 0) {
			size_t size_to_allocate = sizeof(wchar_t) * (_file_location.size + 2);
			void* allocation = Allocate(mesh_metadata.allocator, size_to_allocate);
			memcpy(allocation, _file_location.buffer, _file_location.size * sizeof(wchar_t));
			file_location.buffer = (wchar_t*)allocation;
			file_location.size = _file_location.size + 1;

			if (function::PathIsAbsolute(_file_location)) {
				if (file_location[file_location.size - 1] != ECS_OS_PATH_SEPARATOR) {
					file_location[file_location.size - 1] = ECS_OS_PATH_SEPARATOR;
				}
			}
			else {
				if (file_location[file_location.size - 1] != ECS_OS_PATH_SEPARATOR_REL) {
					file_location[file_location.size - 1] = ECS_OS_PATH_SEPARATOR_REL;
				}
			}

			file_location[file_location.size] = L'\0';
		}
		else {
			file_location = { nullptr, 0 };
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
		GetSerializeOmitFieldsFromExclude(database->reflection_manager, omit_fields, STRING(GPUBufferMetadata), { fields_to_keep, std::size(fields_to_keep) });
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
		update_asset_type(database->gpu_buffer_metadata.ToStream());
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
