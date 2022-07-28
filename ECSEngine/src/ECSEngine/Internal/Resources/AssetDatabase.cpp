#include "ecspch.h"
#include "AssetDatabase.h"
#include "../../Utilities/File.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "../../Utilities/Serialization/Binary/Serialization.h"
#include "../../Utilities/Reflection/Reflection.h"
#include "../../Utilities/Path.h"
#include "../../Allocators/ResizableLinearAllocator.h"

#define SERIALIZE_VERSION 1

#define MESH_FOLDER L"Mesh"
#define TEXTURE_FOLDER L"Texture"
#define GPU_BUFFER_FOLDER L"GPUBuffer"
#define GPU_SAMPLER_FOLDER L"GPUSampler"
#define SHADER_FOLDER L"Shader"
#define MATERIAL_FOLDER L"Material"
#define MISC_FOLDER L"Misc"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------

	AssetDatabase::AssetDatabase(Stream<wchar_t> _file_location, AllocatorPolymorphic allocator, Reflection::ReflectionManager* _reflection_manager)
		: reflection_manager(_reflection_manager) 
	{
		SetAllocator(allocator);
		SetFileLocation(_file_location);
	}
	
	// --------------------------------------------------------------------------------------

	template<typename AssetType, typename AssetSet>
	unsigned int AddAssetDefault(AssetDatabase* database, AssetSet& set, Stream<char> name, const char* serialize_string, ECS_ASSET_TYPE type) {
		unsigned int handle = database->FindAsset(name, type);

		if (handle == -1) {
			AssetType metadata;

			ECS_STACK_CAPACITY_STREAM(wchar_t, path, 256);
			database->FileLocationAsset(path, type);

			DeserializeOptions options;
			options.field_allocator = set.allocator;
			ECS_DESERIALIZE_CODE code = Deserialize(database->reflection_manager, database->reflection_manager->GetType(serialize_string), &metadata, path, &options);
			if (code == ECS_DESERIALIZE_OK) {
				return set.Add({ metadata, 1 });
			}
			else {
				return -1;
			}
		}
		else {
			set[handle].reference_count++;
			return handle;
		}
	}

	unsigned int AssetDatabase::AddMesh(Stream<char> name)
	{
		return AddAssetDefault<MeshMetadata>(this, mesh_metadata, name, STRING(MeshMetadata), ECS_ASSET_MESH);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddTexture(Stream<char> name)
	{
		return AddAssetDefault<TextureMetadata>(this, texture_metadata, name, STRING(TextureMetadata), ECS_ASSET_TEXTURE);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddGPUBuffer(Stream<char> name)
	{
		return AddAssetDefault<GPUBufferMetadata>(this, gpu_buffer_metadata, name, STRING(GPUBufferMetadata), ECS_ASSET_GPU_BUFFER);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddGPUSampler(Stream<char> name)
	{
		return AddAssetDefault<GPUSamplerMetadata>(this, gpu_sampler_metadata, name, STRING(GPUSamplerMetadata), ECS_ASSET_GPU_SAMPLER);
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::AddShader(Stream<char> name)
	{
		return AddAssetDefault<ShaderMetadata>(this, shader_metadata, name, STRING(ShaderMetadata), ECS_ASSET_SHADER);
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

	unsigned int AssetDatabase::AddMisc(Stream<wchar_t> path)
	{
		return AddAssetDefault<MiscAsset>(this, misc_asset, Stream<char>(path.buffer, path.size), STRING(MiscAsset), ECS_ASSET_MISC);
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
		return mesh_metadata.FindFunctor([&](const ReferenceCountedAsset<MeshMetadata>& compare) {
			return function::CompareStrings(compare.asset.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindTexture(Stream<char> name) const
	{
		return texture_metadata.FindFunctor([&](const ReferenceCountedAsset<TextureMetadata>& compare) {
			return function::CompareStrings(compare.asset.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindGPUBuffer(Stream<char> name) const
	{
		return gpu_buffer_metadata.FindFunctor([&](const ReferenceCountedAsset<GPUBufferMetadata>& compare) {
			return function::CompareStrings(compare.asset.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindGPUSampler(Stream<char> name) const
	{
		return gpu_sampler_metadata.FindFunctor([&](const ReferenceCountedAsset<GPUSamplerMetadata>& compare) {
			return function::CompareStrings(compare.asset.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindShader(Stream<char> name) const
	{
		return shader_metadata.FindFunctor([&](const ReferenceCountedAsset<ShaderMetadata>& compare) {
			return function::CompareStrings(compare.asset.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMaterial(Stream<char> name) const
	{
		return material_asset.FindFunctor([&](const ReferenceCountedAsset<MaterialAsset>& compare) {
			return function::CompareStrings(compare.asset.name, name);
		});
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMisc(Stream<wchar_t> path) const
	{
		return misc_asset.FindFunctor([&](const ReferenceCountedAsset<MiscAsset>& compare) {
			return function::CompareStrings(compare.asset.path, path);
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
		path.AddStreamSafe(ToStream(MESH_FOLDER));
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationTexture(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(ToStream(TEXTURE_FOLDER));
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationGPUBuffer(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(ToStream(GPU_BUFFER_FOLDER));
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationGPUSampler(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(ToStream(GPU_SAMPLER_FOLDER));
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationShader(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(ToStream(SHADER_FOLDER));
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationMaterial(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(ToStream(MATERIAL_FOLDER));
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::FileLocationMisc(CapacityStream<wchar_t>& path) const
	{
		path.Copy(file_location);
		path.AddStreamSafe(ToStream(MISC_FOLDER));
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
		return &mesh_metadata[handle].asset;
	}

	// --------------------------------------------------------------------------------------

	TextureMetadata* AssetDatabase::GetTexture(unsigned int handle)
	{
		return &texture_metadata[handle].asset;
	}

	// --------------------------------------------------------------------------------------

	GPUBufferMetadata* AssetDatabase::GetGPUBuffer(unsigned int handle)
	{
		return &gpu_buffer_metadata[handle].asset;
	}

	// --------------------------------------------------------------------------------------

	GPUSamplerMetadata* AssetDatabase::GetGPUSampler(unsigned int handle)
	{
		return &gpu_sampler_metadata[handle].asset;
	}

	// --------------------------------------------------------------------------------------

	ShaderMetadata* AssetDatabase::GetShader(unsigned int handle)
	{
		return &shader_metadata[handle].asset;
	}

	// --------------------------------------------------------------------------------------

	MaterialAsset* AssetDatabase::GetMaterial(unsigned int handle)
	{
		return &material_asset[handle].asset;
	}

	// --------------------------------------------------------------------------------------

	MiscAsset* AssetDatabase::GetMisc(unsigned int handle)
	{
		return &misc_asset[handle].asset;
	}

	// --------------------------------------------------------------------------------------

	typedef void* (AssetDatabase::* GetAssetFunction)(unsigned int handle);

	GetAssetFunction GET_FUNCTIONS[] = {
		(GetAssetFunction)&AssetDatabase::GetMesh,
		(GetAssetFunction)&AssetDatabase::GetTexture,
		(GetAssetFunction)&AssetDatabase::GetGPUBuffer,
		(GetAssetFunction)&AssetDatabase::GetGPUSampler,
		(GetAssetFunction)&AssetDatabase::GetShader,
		(GetAssetFunction)&AssetDatabase::GetMaterial,
		(GetAssetFunction)&AssetDatabase::GetMisc
	};

	void* AssetDatabase::GetAsset(unsigned int handle, ECS_ASSET_TYPE type)
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
			metadata.asset.DeallocateMemory(type.allocator);
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

	// --------------------------------------------------------------------------------------

	ECS_DESERIALIZE_CODE DeserializeAssetDatabase(AssetDatabase* database, Stream<wchar_t> file)
	{
		const size_t STACK_CAPACITY = ECS_KB * 128;
		void* stack_allocation = ECS_STACK_ALLOC(STACK_CAPACITY);

		ResizableLinearAllocator _allocator(stack_allocation, STACK_CAPACITY, ECS_MB, { nullptr });
		AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&_allocator);

		DeserializeOptions options;
		options.backup_allocator = allocator;
		options.file_allocator = allocator;
		options.field_allocator = allocator;

		ECS_DESERIALIZE_CODE code = Deserialize(database->reflection_manager, database->reflection_manager->GetType(STRING(AssetDatabase)), database, file, &options);
		// If it failed, just exit
		if (code != ECS_DESERIALIZE_OK) {
			_allocator.ClearBackup();
			return code;
		}

		// Allocate all the fields now
		// Make the reference count 0 for all asset types. The database references will update the reference count

		AllocatorPolymorphic database_allocator = database->Allocator();

		auto update_asset_type = [database_allocator](auto stream_type) {
			for (size_t index = 0; index < stream_type.size; index++) {
				stream_type[index].reference_count = 0;
				stream_type[index].asset = stream_type[index].asset.Copy(database_allocator);
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

	// --------------------------------------------------------------------------------------

}
