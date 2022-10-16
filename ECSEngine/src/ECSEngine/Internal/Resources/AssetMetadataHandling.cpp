#include "ecspch.h"
#include "AssetMetadataHandling.h"

#include "ResourceManager.h"
#include "AssetDatabase.h"
#include "../../Allocators/ResizableLinearAllocator.h"
#include "../../Utilities/Path.h"
#include "../../Utilities/OSFunctions.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------

	void MeshMetadataIdentifier(const MeshMetadata* metadata, CapacityStream<void>& identifier)
	{
		identifier.Add(&metadata->scale_factor);
		identifier.Add(&metadata->invert_z_axis);
		identifier.Add(&metadata->optimize_level);
	}

	// ------------------------------------------------------------------------------------------------------

	void TextureMetadataIdentifier(const TextureMetadata* metadata, CapacityStream<void>& identifier)
	{
		identifier.Add(&metadata->compression_type);
		identifier.Add(&metadata->generate_mip_maps);
		identifier.Add(&metadata->sRGB);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	size_t GetTimeStamp(size_t time_stamp, Stream<wchar_t> file) {
		return time_stamp == 0 ? OS::GetFileLastWrite(file) : time_stamp;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	bool CreateMeshHelper(const MeshMetadata* metadata, Stream<wchar_t> mount_point, Functor&& functor) {
		if (metadata->file.size == 0) {
			// There is no path, fail
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPath(metadata->file, mount_point, absolute_path);

		ResourceManagerLoadDesc load_descriptor;
		load_descriptor.load_flags = metadata->invert_z_axis ? 0 : ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT;

		ECS_STACK_VOID_STREAM(suffix, 512);
		MeshMetadataIdentifier(metadata, suffix);
		load_descriptor.identifier_suffix = suffix;
		return functor(file_path, load_descriptor);
	}

	bool CreateMeshFromMetadata(ResourceManager* resource_manager, const MeshMetadata* metadata, Stream<wchar_t> mount_point)
	{
		return CreateMeshHelper(metadata, mount_point, [&](Stream<wchar_t> file_path, ResourceManagerLoadDesc load_descriptor) {
			return resource_manager->LoadCoallescedMesh(file_path, metadata->scale_factor, load_descriptor) != nullptr;
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateMeshFromMetadataEx(ResourceManager* resource_manager, const MeshMetadata* metadata, Stream<GLTFMesh> meshes, CreateAssetFromMetadataExData* ex_data)
	{
		return CreateMeshHelper(metadata, ex_data->mount_point, [&](Stream<wchar_t> file_path, ResourceManagerLoadDesc load_descriptor) {
			size_t time_stamp = GetTimeStamp(ex_data->time_stamp, file_path);
			ResourceManagerExDesc ex_desc;
			ex_desc.filename = file_path;
			ex_desc.time_stamp = time_stamp;
			ex_desc.push_lock = ex_data->manager_lock;
			return resource_manager->LoadCoallescedMeshImplementationEx(meshes, metadata->scale_factor, load_descriptor, &ex_desc) != nullptr;
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	bool CreateTextureHelper(ResourceManager* resource_manager, const TextureMetadata* metadata, Stream<wchar_t> mount_point, Functor&& functor) {
		if (metadata->file.size == 0) {
			// There is no file, fail
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPath(metadata->file, mount_point, absolute_path);

		ResourceManagerTextureDesc texture_descriptor;
		texture_descriptor.misc_flags = metadata->generate_mip_maps ? ECS_GRAPHICS_MISC_GENERATE_MIPS : ECS_GRAPHICS_MISC_NONE;
		texture_descriptor.context = resource_manager->m_graphics->GetContext();
		texture_descriptor.compression = metadata->compression_type;
		texture_descriptor.srgb = metadata->sRGB;

		ECS_STACK_VOID_STREAM(suffix, 512);
		TextureMetadataIdentifier(metadata, suffix);
		ResourceManagerLoadDesc load_desc;
		load_desc.identifier_suffix = suffix;

		return functor(file_path, &texture_descriptor, load_desc);
	}

	bool CreateTextureFromMetadata(ResourceManager* resource_manager, const TextureMetadata* metadata, Stream<wchar_t> mount_point)
	{
		return CreateTextureHelper(resource_manager, metadata, mount_point, [&](auto file_path, const auto* texture_descriptor, auto load_desc) {
			return resource_manager->LoadTexture(file_path, texture_descriptor, load_desc).Interface() != nullptr;
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateTextureFromMetadataEx(
		ResourceManager* resource_manager,
		const TextureMetadata* metadata,
		DecodedTexture texture,
		SpinLock* gpu_spin_lock,
		CreateAssetFromMetadataExData* ex_data
	)
	{
		return CreateTextureHelper(resource_manager, metadata, ex_data->mount_point, [&](auto file_path, const auto* texture_descriptor, auto load_desc) {
			size_t time_stamp = GetTimeStamp(ex_data->time_stamp, file_path);
			load_desc.gpu_lock = gpu_spin_lock;

			ResourceManagerExDesc ex_desc;
			ex_desc.filename = file_path;
			ex_desc.time_stamp = time_stamp;
			ex_desc.push_lock = ex_data->manager_lock;
			return resource_manager->LoadTextureImplementationEx(texture, texture_descriptor, load_desc, &ex_desc).Interface() != nullptr;
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void CreateSamplerFromMetadata(ResourceManager* resource_manager, GPUSamplerMetadata* metadata)
	{
		SamplerDescriptor descriptor;
		descriptor.max_anisotropic_level = metadata->anisotropic_level;
		descriptor.address_type_u = metadata->address_mode;
		descriptor.address_type_v = metadata->address_mode;
		descriptor.address_type_w = metadata->address_mode;
		descriptor.filter_type = metadata->filter_mode;
		metadata->sampler = resource_manager->m_graphics->CreateSamplerState(descriptor);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	bool CreateShaderHelper(ShaderMetadata* metadata, Stream<wchar_t> mount_point, Functor&& functor) {
		if (metadata->file.size == 0) {
			// Return false, no file assigned
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPathOnlyRel(metadata->file, mount_point, absolute_path);

		ShaderCompileOptions compile_options;
		compile_options.macros = metadata->macros;
		compile_options.compile_flags = metadata->compile_flag;

		ECS_STACK_VOID_STREAM(suffix, 512);
		ShaderMetadataIdentifier(metadata, suffix);
		ResourceManagerLoadDesc load_desc;
		load_desc.identifier_suffix = suffix;

		void* shader_interface = functor(file_path, compile_options, load_desc);
		metadata->shader_interface = shader_interface;

		return shader_interface != nullptr;
	}

	bool CreateShaderFromMetadata(ResourceManager* resource_manager, ShaderMetadata* metadata, Stream<wchar_t> mount_point)
	{
		return CreateShaderHelper(metadata, mount_point, [&](auto file_path, auto compile_options, auto load_desc) {
			return resource_manager->LoadShader(file_path, metadata->shader_type, &metadata->source_code, &metadata->byte_code, compile_options, load_desc);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateShaderFromMetadataEx(
		ResourceManager* resource_manager,
		ShaderMetadata* metadata,
		Stream<char> source_code,
		CreateAssetFromMetadataExData* ex_data
	)
	{
		return CreateShaderHelper(metadata, ex_data->mount_point, [&](auto file_path, auto compile_options, auto load_desc) {
			size_t time_stamp = GetTimeStamp(ex_data->time_stamp, file_path);
			ResourceManagerExDesc ex_desc;
			ex_desc.filename = file_path;
			ex_desc.time_stamp = time_stamp;
			ex_desc.push_lock = ex_data->manager_lock;
			metadata->source_code = source_code;
			return resource_manager->LoadShaderImplementationEx(source_code, metadata->shader_type, &metadata->byte_code, compile_options, load_desc, &ex_desc);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void ShaderMetadataIdentifier(const ShaderMetadata* metadata, CapacityStream<void>& identifier)
	{
		identifier.Add(&metadata->compile_flag);
		identifier.Add(&metadata->shader_type);
		for (size_t index = 0; index < metadata->macros.size; index++) {
			identifier.Add({ metadata->macros[index].name, strlen(metadata->macros[index].name) });
			identifier.Add({ metadata->macros[index].definition, strlen(metadata->macros[index].definition) });
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateMaterialFromMetadata(ResourceManager* resource_manager, AssetDatabase* asset_database, MaterialAsset* material, Stream<wchar_t> mount_point, bool dont_load_referenced)
	{
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
		AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&stack_allocator);
		
		UserMaterial user_material;
		ConvertMaterialAssetToUserMaterial(asset_database, material, &user_material, allocator, mount_point);
		
		if (material->material_pointer == nullptr) {
			AllocatorPolymorphic database_allocator = asset_database->Allocator();
			database_allocator.allocation_type = ECS_ALLOCATION_MULTI;
			material->material_pointer = (Material*)Allocate(database_allocator, sizeof(Material));
		}

		ResourceManagerLoadDesc load_desc;
		load_desc.load_flags |= dont_load_referenced ? ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_LOAD : 0;
		bool success = resource_manager->LoadUserMaterial(&user_material, (Material*)material->material_pointer, load_desc);

		stack_allocator.ClearBackup();
		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void ConvertMaterialAssetToUserMaterial(
		const AssetDatabase* database,
		const MaterialAsset* material, 
		UserMaterial* user_material, 
		AllocatorPolymorphic allocator,
		Stream<wchar_t> mount_point
	)
	{
		user_material->textures.Initialize(allocator, material->textures.size);
		user_material->buffers.Initialize(allocator, material->buffers.size);

		for (size_t index = 0; index < material->textures.size; index++) {
			const TextureMetadata* texture_metadata = database->GetTextureConst(material->textures[index].metadata_handle);
			Stream<wchar_t> final_texture_path = function::MountPath(texture_metadata->file, mount_point, allocator);

			user_material->textures[index].filename = final_texture_path;
			user_material->textures[index].srgb = texture_metadata->sRGB;
			user_material->textures[index].slot = material->textures[index].slot;
			user_material->textures[index].generate_mips = texture_metadata->generate_mip_maps;
			user_material->textures[index].compression = texture_metadata->compression_type;
			user_material->textures[index].shader_type = ECS_SHADER_PIXEL;
		}

		for (size_t index = 0; index < material->buffers.size; index++) {
			user_material->buffers[index].data = material->buffers[index].data;
			user_material->buffers[index].dynamic = material->buffers[index].dynamic;
			user_material->buffers[index].shader_type = material->buffers[index].shader_type;
			user_material->buffers[index].slot = material->buffers[index].slot;
		}

		const ShaderMetadata* pixel_shader_metadata = database->GetShaderConst(material->pixel_shader_handle);
		const ShaderMetadata* vertex_shader_metadata = database->GetShaderConst(material->vertex_shader_handle);

		// Mount the shaders only if they are not in absolute paths already
		Stream<wchar_t> pixel_shader_path = function::MountPathOnlyRel(pixel_shader_metadata->file, mount_point, allocator);
		user_material->pixel_shader = pixel_shader_path;
		user_material->pixel_compile_options.compile_flags = pixel_shader_metadata->compile_flag;
		user_material->pixel_compile_options.macros = pixel_shader_metadata->macros;
		user_material->pixel_compile_options.target = ECS_SHADER_TARGET_5_0;

		Stream<wchar_t> vertex_shader_path = function::MountPathOnlyRel(vertex_shader_metadata->file, mount_point, allocator);
		user_material->vertex_shader = vertex_shader_path;
		user_material->vertex_compile_options.compile_flags = vertex_shader_metadata->compile_flag;
		user_material->vertex_compile_options.macros = vertex_shader_metadata->macros;
		user_material->vertex_compile_options.target = ECS_SHADER_TARGET_5_0;

		user_material->generate_unique_name_from_setting = true;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateMiscAssetFromMetadata(ResourceManager* resource_manager, MiscAsset* misc_asset, Stream<wchar_t> mount)
	{
		if (misc_asset->file.size == 0) {
			// No file assigned
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);
		Stream<wchar_t> final_path = function::MountPathOnlyRel(misc_asset->file, mount, storage);
		ResizableStream<void> data = *resource_manager->LoadMisc<true>(final_path);
		misc_asset->data = { data.buffer, data.size };

		return misc_asset->data.size > 0 || misc_asset->data.buffer != nullptr;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateAssetFromMetadata(ResourceManager* resource_manager, AssetDatabase* database, void* asset, ECS_ASSET_TYPE type, Stream<wchar_t> mount_point)
	{
		bool success = true;

		switch (type) {
		case ECS_ASSET_MESH: 
		{
			success = CreateMeshFromMetadata(resource_manager, (const MeshMetadata*)asset, mount_point);
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			success = CreateTextureFromMetadata(resource_manager, (const TextureMetadata*)asset, mount_point);
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			CreateSamplerFromMetadata(resource_manager, (GPUSamplerMetadata*)asset);
		}
		break;
		case ECS_ASSET_SHADER:
		{
			success = CreateShaderFromMetadata(resource_manager, (ShaderMetadata*)asset, mount_point);
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			success = CreateMaterialFromMetadata(resource_manager, database, (MaterialAsset*)asset, mount_point);
		}
		break;
		case ECS_ASSET_MISC:
		{
			success = CreateMiscAssetFromMetadata(resource_manager, (MiscAsset*)asset, mount_point);
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	template<typename Metadata>
	bool IsTypeFromMetadataLoaded(const ResourceManager* resource_manager, const Metadata* metadata, Stream<wchar_t> mount_point, ResourceType type, Stream<void> suffix) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPathOnlyRel(metadata->file, mount_point, absolute_path);

		return resource_manager->GetResourceIndex(file_path, type, suffix) != -1;
	}

	bool IsMeshFromMetadataLoaded(const ResourceManager* resource_manager, const MeshMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_VOID_STREAM(suffix, 512);
		MeshMetadataIdentifier(metadata, suffix);
		return IsTypeFromMetadataLoaded(resource_manager, metadata, mount_point, ResourceType::Mesh, suffix);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsTextureFromMetadataLoaded(const ResourceManager* resource_manager, const TextureMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_VOID_STREAM(suffix, 512);
		TextureMetadataIdentifier(metadata, suffix);
		return IsTypeFromMetadataLoaded(resource_manager, metadata, mount_point, ResourceType::Texture, suffix);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsGPUSamplerFromMetadataLoaded(const GPUSamplerMetadata* metadata)
	{
		return metadata->sampler.Interface() != nullptr;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsShaderFromMetadataLoaded(const ResourceManager* resource_manager, const ShaderMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_VOID_STREAM(suffix, 512);
		ShaderMetadataIdentifier(metadata, suffix);

		ResourceType resource_type = FromShaderTypeToResourceType(metadata->shader_type);
		return IsTypeFromMetadataLoaded(resource_manager, metadata, mount_point, resource_type, suffix);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsMaterialFromMetadataLoaded(const MaterialAsset* metadata)
	{
		return metadata->material_pointer != nullptr;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsMiscFromMetadataLoaded(const ResourceManager* resource_manager, const MiscAsset* metadata, Stream<wchar_t> mount_point)
	{
		return IsTypeFromMetadataLoaded(resource_manager, metadata, mount_point, ResourceType::Misc, {});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void GetDatabaseMissingAssets(
		const ResourceManager* resource_manager,
		const AssetDatabase* database, 
		CapacityStream<unsigned int>* missing_handles, 
		Stream<wchar_t> mount_point
	)
	{
		auto get_missing_assets = [&](ECS_ASSET_TYPE type, auto sparse_set, auto is_loaded) {
			auto stream_type = sparse_set.ToStream();
			for (size_t index = 0; index < stream_type.size; index++) {
				unsigned int handle = sparse_set.GetHandleFromIndex(index);
				if (!is_loaded(database->GetAssetConst(handle, type))) {
					missing_handles[type].AddSafe(handle);
				}
			}
		};

		get_missing_assets(ECS_ASSET_MESH, database->mesh_metadata,
			[mount_point, resource_manager](const void* asset) {
				return IsMeshFromMetadataLoaded(resource_manager, (const MeshMetadata*)asset, mount_point);
			}
		);
		get_missing_assets(ECS_ASSET_TEXTURE, database->texture_metadata,
			[mount_point, resource_manager](const void* asset) {
				return IsTextureFromMetadataLoaded(resource_manager, (const TextureMetadata*)asset, mount_point);
			}
		);
		get_missing_assets(ECS_ASSET_SHADER, database->shader_metadata,
			[mount_point, resource_manager](const void* asset) {
				return IsShaderFromMetadataLoaded(resource_manager, (const ShaderMetadata*)asset, mount_point);
			}
		);
		get_missing_assets(ECS_ASSET_MISC, database->misc_asset,
			[mount_point, resource_manager](const void* asset) {
				return IsMiscFromMetadataLoaded(resource_manager, (const MiscAsset*)asset, mount_point);
			}
		);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool HasMissingAssets(CapacityStream<unsigned int>* missing_handles)
	{
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			if (missing_handles[index].size > 0) {
				return true;
			}
		}
		return false;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateMeshFromMetadata(ResourceManager* resource_manager, MeshMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPath(metadata->file, mount_point, absolute_path);

		ECS_STACK_VOID_STREAM(suffix, 512);
		MeshMetadataIdentifier(metadata, suffix);
		ResourceManagerLoadDesc load_desc;
		load_desc.identifier_suffix = suffix;

		bool unloaded = resource_manager->TryUnloadResource<true>(file_path, ResourceType::CoallescedMesh, load_desc);
		metadata->mesh_pointer = nullptr;
		return unloaded;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateTextureFromMetadata(ResourceManager* resource_manager, TextureMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPath(metadata->file, mount_point, absolute_path);

		ECS_STACK_VOID_STREAM(suffix, 512);
		TextureMetadataIdentifier(metadata, suffix);
		ResourceManagerLoadDesc load_desc;
		load_desc.identifier_suffix = suffix;
		bool unloaded = resource_manager->TryUnloadResource<true>(file_path, ResourceType::Texture, load_desc);
		metadata->texture.view = nullptr;
		return unloaded;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void DeallocateSamplerFromMetadata(ResourceManager* resource_manager, GPUSamplerMetadata* metadata)
	{
		resource_manager->m_graphics->FreeResource(metadata->sampler);
		metadata->sampler.sampler = nullptr;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateShaderFromMetadata(ResourceManager* resource_manager, ShaderMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPathOnlyRel(metadata->file, mount_point, absolute_path);

		ECS_STACK_VOID_STREAM(suffix, 512);
		ShaderMetadataIdentifier(metadata, suffix);
		ResourceManagerLoadDesc load_desc;
		load_desc.identifier_suffix = suffix;

		ResourceType type = FromShaderTypeToResourceType(metadata->shader_type);
		bool unloaded = resource_manager->TryUnloadResource<true>(file_path, type, load_desc);

		metadata->shader_interface = nullptr;
		metadata->source_code = { nullptr, 0 };
		metadata->byte_code = { nullptr, 0 };

		return unloaded;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void DeallocateMaterialFromMetadata(ResourceManager* resource_manager, MaterialAsset* material, const AssetDatabase* database, Stream<wchar_t> mount_point)
	{
		// Deallocate the textures and the buffers
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);

		UserMaterial user_material;
		ConvertMaterialAssetToUserMaterial(database, material, &user_material, GetAllocatorPolymorphic(&stack_allocator), mount_point);
		resource_manager->UnloadUserMaterial(&user_material, (Material*)material->material_pointer);

		DeallocateIfBelongs(database->Allocator(), material->material_pointer);
		material->material_pointer = nullptr;

		stack_allocator.ClearBackup();
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateMiscAssetFromMetadata(ResourceManager* resource_manager, MiscAsset* misc, Stream<wchar_t> mount_point)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);
		Stream<wchar_t> final_path = function::MountPathOnlyRel(misc->file, mount_point, storage);
		bool unloaded = resource_manager->TryUnloadResource<true>(final_path, ResourceType::Misc);

		misc->data = { nullptr, 0 };	
		return unloaded;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateAssetFromMetadata(ResourceManager* resource_manager, AssetDatabase* database, unsigned int handle, ECS_ASSET_TYPE type, Stream<wchar_t> mount_point)
	{
		void* asset = database->GetAsset(handle, type);

		switch (type) {
		case ECS_ASSET_MESH:
		{
			return DeallocateMeshFromMetadata(resource_manager, (MeshMetadata*)asset, mount_point);
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			return DeallocateTextureFromMetadata(resource_manager, (TextureMetadata*)asset, mount_point);
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			DeallocateSamplerFromMetadata(resource_manager, (GPUSamplerMetadata*)asset);
			return true;
		}
		break;
		case ECS_ASSET_SHADER:
		{
			return DeallocateShaderFromMetadata(resource_manager, (ShaderMetadata*)asset, mount_point);
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			DeallocateMaterialFromMetadata(resource_manager, (MaterialAsset*)asset, database, mount_point);
			return true;
		}
		break;
		case ECS_ASSET_MISC:
		{
			return DeallocateMiscAssetFromMetadata(resource_manager, (MiscAsset*)asset, mount_point);
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
		return false;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void SetShaderMetadataSourceCode(ShaderMetadata* metadata, Stream<char> source_code)
	{
		metadata->source_code = source_code;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void SetShaderMetadataByteCode(ShaderMetadata* metadata, Stream<void> byte_code)
	{
		metadata->byte_code = byte_code;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void SetMiscData(MiscAsset* misc_asset, Stream<void> data)
	{
		misc_asset->data = data;
	}

	// -------------------------------------------------------------------------------------------------------------------------

}