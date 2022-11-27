#include "ecspch.h"
#include "AssetMetadataHandling.h"

#include "ResourceManager.h"
#include "AssetDatabase.h"
#include "AssetDatabaseReference.h"
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
	bool CreateMeshHelper(MeshMetadata* metadata, Stream<wchar_t> mount_point, Functor&& functor) {
		if (metadata->file.size == 0) {
			// There is no path, fail
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPathOnlyRel(metadata->file, mount_point, absolute_path);

		ResourceManagerLoadDesc load_descriptor;
		load_descriptor.load_flags = metadata->invert_z_axis ? 0 : ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT;

		ECS_STACK_VOID_STREAM(suffix, 512);
		MeshMetadataIdentifier(metadata, suffix);
		load_descriptor.identifier_suffix = suffix;
		CoallescedMesh* mesh = functor(file_path, load_descriptor);
		if (mesh != nullptr) {
			metadata->mesh_pointer = mesh;
			return true;
		}
		return false;
	}

	bool CreateMeshFromMetadata(ResourceManager* resource_manager, MeshMetadata* metadata, Stream<wchar_t> mount_point)
	{
		return CreateMeshHelper(metadata, mount_point, [&](Stream<wchar_t> file_path, ResourceManagerLoadDesc load_descriptor) {
			return resource_manager->LoadCoallescedMesh(file_path, metadata->scale_factor, load_descriptor);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateMeshFromMetadataEx(ResourceManager* resource_manager, MeshMetadata* metadata, Stream<GLTFMesh> meshes, CreateAssetFromMetadataExData* ex_data)
	{
		return CreateMeshHelper(metadata, ex_data->mount_point, [&](Stream<wchar_t> file_path, ResourceManagerLoadDesc load_descriptor) {
			size_t time_stamp = GetTimeStamp(ex_data->time_stamp, file_path);
			ResourceManagerExDesc ex_desc;
			ex_desc.filename = file_path;
			ex_desc.time_stamp = time_stamp;
			ex_desc.push_lock = ex_data->manager_lock;
			return resource_manager->LoadCoallescedMeshImplementationEx(meshes, metadata->scale_factor, load_descriptor, &ex_desc);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void CreateMeshFromMetadataEx(
		ResourceManager* resource_manager, 
		MeshMetadata* metadata, 
		const GLTFMesh* coallesced_mesh, 
		Stream<Submesh> submeshes, 
		CreateAssetFromMetadataExData* ex_data
	)
	{
		ECS_ASSERT(CreateMeshHelper(metadata, ex_data->mount_point, [&](Stream<wchar_t> file_path, ResourceManagerLoadDesc load_descriptor) {
			size_t time_stamp = GetTimeStamp(ex_data->time_stamp, file_path);
			ResourceManagerExDesc ex_desc;
			ex_desc.filename = file_path;
			ex_desc.time_stamp = time_stamp;
			ex_desc.push_lock = ex_data->manager_lock;
			return resource_manager->LoadCoallescedMeshImplementationEx(coallesced_mesh, submeshes, metadata->scale_factor, load_descriptor, &ex_desc);
		}));
	}

	// -------------------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	bool CreateTextureHelper(ResourceManager* resource_manager, TextureMetadata* metadata, Stream<wchar_t> mount_point, Functor&& functor) {
		if (metadata->file.size == 0) {
			// There is no file, fail
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPathOnlyRel(metadata->file, mount_point, absolute_path);

		ResourceManagerTextureDesc texture_descriptor;
		texture_descriptor.misc_flags = metadata->generate_mip_maps ? ECS_GRAPHICS_MISC_GENERATE_MIPS : ECS_GRAPHICS_MISC_NONE;
		texture_descriptor.context = resource_manager->m_graphics->GetContext();
		texture_descriptor.compression = metadata->compression_type;
		texture_descriptor.srgb = metadata->sRGB;

		ECS_STACK_VOID_STREAM(suffix, 512);
		TextureMetadataIdentifier(metadata, suffix);
		ResourceManagerLoadDesc load_desc;
		load_desc.identifier_suffix = suffix;

		ResourceView loaded_texture = functor(file_path, &texture_descriptor, load_desc);
		if (loaded_texture.Interface() != nullptr) {
			metadata->texture = loaded_texture;
			return true;
		}
		return false;
	}

	bool CreateTextureFromMetadata(ResourceManager* resource_manager, TextureMetadata* metadata, Stream<wchar_t> mount_point)
	{
		return CreateTextureHelper(resource_manager, metadata, mount_point, [&](auto file_path, const auto* texture_descriptor, auto load_desc) {
			return resource_manager->LoadTexture(file_path, texture_descriptor, load_desc);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateTextureFromMetadataEx(
		ResourceManager* resource_manager,
		TextureMetadata* metadata,
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
			return resource_manager->LoadTextureImplementationEx(texture, texture_descriptor, load_desc, &ex_desc);
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

		ShaderInterface shader_interface = functor(file_path, compile_options, load_desc);
		if (shader_interface != nullptr) {
			metadata->shader_interface = shader_interface;
			return true;
		}
		return false;
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
	
		Material runtime_material;
		ResourceManagerLoadDesc load_desc;
		load_desc.load_flags |= dont_load_referenced ? ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS : 0;
		bool success = resource_manager->LoadUserMaterial(&user_material, &runtime_material, load_desc);
		stack_allocator.ClearBackup();

		if (success) {
			if (material->material_pointer == nullptr) {
				AllocatorPolymorphic database_allocator = asset_database->Allocator();
				database_allocator.allocation_type = ECS_ALLOCATION_MULTI;
				material->material_pointer = (Material*)Allocate(database_allocator, sizeof(Material));
			}
			memcpy(material->material_pointer, &runtime_material, sizeof(runtime_material));
			return true;
		}
		return false;
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
		if (data.size > 0 && data.buffer != nullptr) {
			misc_asset->data = { data.buffer, data.size };
			return true;
		}
		return false;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateAssetFromMetadata(ResourceManager* resource_manager, AssetDatabase* database, void* asset, ECS_ASSET_TYPE type, Stream<wchar_t> mount_point)
	{
		bool success = true;

		switch (type) {
		case ECS_ASSET_MESH: 
		{
			success = CreateMeshFromMetadata(resource_manager, (MeshMetadata*)asset, mount_point);
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			success = CreateTextureFromMetadata(resource_manager, (TextureMetadata*)asset, mount_point);
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

	void AssetMetadataIdentifier(const void* metadata, ECS_ASSET_TYPE type, CapacityStream<void>& identifier)
	{
		switch (type) {
		case ECS_ASSET_MESH:
			MeshMetadataIdentifier((const MeshMetadata*)metadata, identifier);
			break;
		case ECS_ASSET_TEXTURE:
			TextureMetadataIdentifier((const TextureMetadata*)metadata, identifier);
			break;
		case ECS_ASSET_SHADER:
			ShaderMetadataIdentifier((const ShaderMetadata*)metadata, identifier);
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_MATERIAL:
		case ECS_ASSET_MISC:
			break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void AssetMetadataResourceIdentifier(const void* metadata, ECS_ASSET_TYPE type, CapacityStream<void>& path_identifier, Stream<wchar_t> mount_point)
	{
		switch (type) {
		case ECS_ASSET_MESH:
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_SHADER:
		case ECS_ASSET_MISC:
		{
			Stream<wchar_t> file = GetAssetFile(metadata, type);
			ECS_STACK_VOID_STREAM(identifier, 512);

			if (type != ECS_ASSET_MISC) {
				AssetMetadataIdentifier(metadata, type, identifier);
			}

			CapacityStream<wchar_t> wide_identifier;
			wide_identifier.InitializeFromBuffer(path_identifier.buffer, 0, path_identifier.capacity);
			file = function::MountPathOnlyRel(file, mount_point, wide_identifier);
			identifier.CopyTo(file.buffer + file.size);
			path_identifier.size = file.size + identifier.size;
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_MATERIAL:
		{

		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
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
		return IsTypeFromMetadataLoaded(resource_manager, metadata, mount_point, ResourceType::CoallescedMesh, suffix);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsTextureFromMetadataLoaded(const ResourceManager* resource_manager, const TextureMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_VOID_STREAM(suffix, 512);
		TextureMetadataIdentifier(metadata, suffix);
		return IsTypeFromMetadataLoaded(resource_manager, metadata, mount_point, ResourceType::Texture, suffix);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsGPUSamplerFromMetadataLoaded(const GPUSamplerMetadata* metadata, bool randomized_asset)
	{
		if (!randomized_asset) {
			return metadata->sampler.Interface() != nullptr;
		}
		else {
			return (size_t)metadata->sampler.Interface() >= ECS_ASSET_RANDOMIZED_ASSET_LIMIT;
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsShaderFromMetadataLoaded(const ResourceManager* resource_manager, const ShaderMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_VOID_STREAM(suffix, 512);
		ShaderMetadataIdentifier(metadata, suffix);

		return IsTypeFromMetadataLoaded(resource_manager, metadata, mount_point, ResourceType::Shader, suffix);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsMaterialFromMetadataLoaded(const MaterialAsset* metadata, bool randomized_asset)
	{
		if (!randomized_asset) {
			return metadata->material_pointer != nullptr;
		}
		else {
			return (size_t)metadata->material_pointer >= ECS_ASSET_RANDOMIZED_ASSET_LIMIT;
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsMiscFromMetadataLoaded(const ResourceManager* resource_manager, const MiscAsset* metadata, Stream<wchar_t> mount_point)
	{
		return IsTypeFromMetadataLoaded(resource_manager, metadata, mount_point, ResourceType::Misc, {});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsAssetFromMetadataLoaded(const ResourceManager* resource_manager, const void* metadata, ECS_ASSET_TYPE type, Stream<wchar_t> mount_point, bool randomized_asset)
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return IsMeshFromMetadataLoaded(resource_manager, (const MeshMetadata*)metadata, mount_point);
		case ECS_ASSET_TEXTURE:
			return IsTextureFromMetadataLoaded(resource_manager, (const TextureMetadata*)metadata, mount_point);
		case ECS_ASSET_GPU_SAMPLER:
			return IsGPUSamplerFromMetadataLoaded((const GPUSamplerMetadata*)metadata, randomized_asset);
		case ECS_ASSET_SHADER:
			return IsShaderFromMetadataLoaded(resource_manager, (const ShaderMetadata*)metadata, mount_point);
		case ECS_ASSET_MATERIAL:
			return IsMaterialFromMetadataLoaded((const MaterialAsset*)metadata, randomized_asset);
		case ECS_ASSET_MISC:
			return IsMiscFromMetadataLoaded(resource_manager, (const MiscAsset*)metadata, mount_point);
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
		return false;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void GetDatabaseMissingAssets(
		const ResourceManager* resource_manager,
		const AssetDatabase* database, 
		CapacityStream<unsigned int>* missing_handles, 
		Stream<wchar_t> mount_point,
		bool randomized_assets
	)
	{
		auto get_missing_assets = [&](ECS_ASSET_TYPE type, auto sparse_set) {
			auto stream_type = sparse_set.ToStream();
			for (size_t index = 0; index < stream_type.size; index++) {
				unsigned int handle = sparse_set.GetHandleFromIndex(index);
				if (!IsAssetFromMetadataLoaded(resource_manager, database->GetAssetConst(handle, type), type, mount_point, randomized_assets)) {
					missing_handles[type].AddSafe(handle);
				}
			}
		};

		get_missing_assets(ECS_ASSET_MESH, database->mesh_metadata);
		get_missing_assets(ECS_ASSET_TEXTURE, database->texture_metadata);
		get_missing_assets(ECS_ASSET_GPU_SAMPLER, database->gpu_sampler_metadata);
		get_missing_assets(ECS_ASSET_SHADER, database->shader_metadata);
		get_missing_assets(ECS_ASSET_MATERIAL, database->material_asset);
		get_missing_assets(ECS_ASSET_MISC, database->misc_asset);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void GetDatabaseMissingAssets(
		const ResourceManager* resource_manager,
		const AssetDatabaseReference* database,
		CapacityStream<unsigned int>* missing_handles,
		Stream<wchar_t> mount_point,
		bool randomized_assets
	) {
		auto get_missing_assets = [&](ECS_ASSET_TYPE type) {
			const AssetDatabase* main_database = database->GetDatabase();
			unsigned int count = database->GetCount(type);

			for (unsigned int index = 0; index < count; index++) {
				unsigned int handle = database->GetHandle(index, type);
				if (!IsAssetFromMetadataLoaded(resource_manager, main_database->GetAssetConst(handle, type), type, mount_point, randomized_assets)) {
					missing_handles[type].AddSafe(handle);
				}
			}
		};

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			get_missing_assets((ECS_ASSET_TYPE)index);
		}
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
		Stream<wchar_t> file_path = function::MountPathOnlyRel(metadata->file, mount_point, absolute_path);

		ECS_STACK_VOID_STREAM(suffix, 512);
		MeshMetadataIdentifier(metadata, suffix);
		ResourceManagerLoadDesc load_desc;
		load_desc.identifier_suffix = suffix;

		return resource_manager->TryUnloadResource(file_path, ResourceType::CoallescedMesh, load_desc);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateTextureFromMetadata(ResourceManager* resource_manager, TextureMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = function::MountPathOnlyRel(metadata->file, mount_point, absolute_path);

		ECS_STACK_VOID_STREAM(suffix, 512);
		TextureMetadataIdentifier(metadata, suffix);
		ResourceManagerLoadDesc load_desc;
		load_desc.identifier_suffix = suffix;
		return resource_manager->TryUnloadResource(file_path, ResourceType::Texture, load_desc);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void DeallocateSamplerFromMetadata(ResourceManager* resource_manager, GPUSamplerMetadata* metadata)
	{
		resource_manager->m_graphics->FreeResource(metadata->sampler);
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

		bool unloaded = resource_manager->TryUnloadResource(file_path, ResourceType::Shader, load_desc);

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

		stack_allocator.ClearBackup();
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateMiscAssetFromMetadata(ResourceManager* resource_manager, MiscAsset* misc, Stream<wchar_t> mount_point)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);
		Stream<wchar_t> final_path = function::MountPathOnlyRel(misc->file, mount_point, storage);
		return resource_manager->TryUnloadResource(final_path, ResourceType::Misc);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateAssetFromMetadata(ResourceManager* resource_manager, AssetDatabase* database, void* metadata, ECS_ASSET_TYPE type, Stream<wchar_t> mount_point)
	{
		switch (type) {
		case ECS_ASSET_MESH:
		{
			return DeallocateMeshFromMetadata(resource_manager, (MeshMetadata*)metadata, mount_point);
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			return DeallocateTextureFromMetadata(resource_manager, (TextureMetadata*)metadata, mount_point);
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			DeallocateSamplerFromMetadata(resource_manager, (GPUSamplerMetadata*)metadata);
			return true;
		}
		break;
		case ECS_ASSET_SHADER:
		{
			return DeallocateShaderFromMetadata(resource_manager, (ShaderMetadata*)metadata, mount_point);
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			DeallocateMaterialFromMetadata(resource_manager, (MaterialAsset*)metadata, database, mount_point);
			return true;
		}
		break;
		case ECS_ASSET_MISC:
		{
			return DeallocateMiscAssetFromMetadata(resource_manager, (MiscAsset*)metadata, mount_point);
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
		return false;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DoesMaterialDependOn(const MaterialAsset* material, const void* other_metadata, ECS_ASSET_TYPE type)
	{
		switch (type) {
		case ECS_ASSET_TEXTURE: 
		{
			TextureMetadata* metadata = (TextureMetadata*)other_metadata;
			return material->material_pointer->ContainsTexture(metadata->texture);
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			GPUSamplerMetadata* metadata = (GPUSamplerMetadata*)other_metadata;
			return material->material_pointer->ContainsSampler(metadata->sampler);
		}
		break;
		case ECS_ASSET_SHADER:
		{
			ShaderMetadata* metadata = (ShaderMetadata*)other_metadata;
			if (metadata->shader_type == ECS_SHADER_VERTEX) {
				return material->material_pointer->ContainsShader(VertexShader::FromInterface(metadata->shader_interface));
			}
			else if (metadata->shader_type == ECS_SHADER_PIXEL) {
				return material->material_pointer->ContainsShader(PixelShader::FromInterface(metadata->shader_interface));
			}
			else {
				return false;
			}
		}
		break;
		case ECS_ASSET_MESH:
		case ECS_ASSET_MATERIAL:
		case ECS_ASSET_MISC:
			return false;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

	Stream<Stream<unsigned int>> GetDependentAssetsFor(const AssetDatabase* database, const void* metadata, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator)
	{
		Stream<Stream<unsigned int>> handles;
		handles.Initialize(allocator, ECS_ASSET_TYPE_COUNT);
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			handles[index] = { nullptr, 0 };
		}

		if (type == ECS_ASSET_MESH || type == ECS_ASSET_MISC || type == ECS_ASSET_MATERIAL) {
			return handles;
		}

		if (type != ECS_ASSET_TEXTURE && type != ECS_ASSET_GPU_SAMPLER && type != ECS_ASSET_SHADER) {
			ECS_ASSERT(false, "Invalid asset type");
		}

		unsigned int material_count = database->GetAssetCount(ECS_ASSET_MATERIAL);
		handles[ECS_ASSET_MATERIAL].Initialize(allocator, material_count);
		handles[ECS_ASSET_MATERIAL].size = 0;
		for (unsigned int index = 0; index < material_count; index++) {
			unsigned int handle = database->GetAssetHandleFromIndex(index, ECS_ASSET_MATERIAL);
			const MaterialAsset* asset = database->GetMaterialConst(handle);

			if (DoesMaterialDependOn(asset, metadata, type)) {
				handles[ECS_ASSET_MATERIAL].Add(handle);
			}
		}
		return handles;
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