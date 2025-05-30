#include "ecspch.h"
#include "AssetMetadataHandling.h"

#include "ResourceManager.h"
#include "AssetDatabase.h"
#include "AssetDatabaseReference.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Utilities/Path.h"
#include "../OS/FileOS.h"
#include "../Utilities/FilePreprocessor.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------

	void MeshMetadataIdentifier(const MeshMetadata* metadata, CapacityStream<void>& identifier)
	{
		identifier.Add(&metadata->scale_factor);
		identifier.Add(&metadata->invert_z_axis);
		identifier.Add(&metadata->optimize_level);
		identifier.Add(&metadata->origin_to_object_center);

		// Add the name as well in order to differentiate between distinct settings with the same options
		identifier.Add(metadata->name);
	}

	// ------------------------------------------------------------------------------------------------------

	void TextureMetadataIdentifier(const TextureMetadata* metadata, CapacityStream<void>& identifier)
	{
		identifier.Add(&metadata->sRGB);
		identifier.Add(&metadata->generate_mip_maps);
		identifier.Add(&metadata->compression_type);

		// Add the name as well in order to differentiate between distinct settings with same options
		identifier.Add(metadata->name);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	size_t GetTimeStamp(size_t time_stamp, Stream<wchar_t> file) {
		return time_stamp == 0 ? OS::GetFileLastWrite(file) : time_stamp;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	static bool CreateMeshHelper(MeshMetadata* metadata, Stream<wchar_t> mount_point, Functor&& functor) {
		if (metadata->file.size == 0) {
			// There is no path, fail
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = MountPathOnlyRel(metadata->file, mount_point, absolute_path);

		ResourceManagerLoadDesc load_descriptor;
		load_descriptor.load_flags = metadata->invert_z_axis ? 0 : ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT;
		if (metadata->origin_to_object_center) {
			load_descriptor.load_flags = SetFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_COALESCED_MESH_ORIGIN_TO_CENTER);
		}

		ECS_STACK_VOID_STREAM(suffix, 512);
		MeshMetadataIdentifier(metadata, suffix);
		load_descriptor.identifier_suffix = suffix;
		CoalescedMesh* mesh = functor(file_path, load_descriptor);
		if (mesh != nullptr) {
			metadata->mesh_pointer = mesh;
			return true;
		}
		return false;
	}

	bool CreateMeshFromMetadata(ResourceManager* resource_manager, MeshMetadata* metadata, Stream<wchar_t> mount_point)
	{
		return CreateMeshHelper(metadata, mount_point, [&](Stream<wchar_t> file_path, const ResourceManagerLoadDesc& load_descriptor) {
			// Use reference counting
			return resource_manager->LoadCoalescedMesh<true>(file_path, metadata->scale_factor, load_descriptor);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateMeshFromMetadataEx(ResourceManager* resource_manager, MeshMetadata* metadata, Stream<GLTFMesh> meshes, CreateAssetFromMetadataExData* ex_data)
	{
		return CreateMeshHelper(metadata, ex_data->mount_point, [&](Stream<wchar_t> file_path, const ResourceManagerLoadDesc& load_descriptor) {
			size_t time_stamp = GetTimeStamp(ex_data->time_stamp, file_path);
			ResourceManagerExDesc ex_desc;
			ex_desc.filename = file_path;
			ex_desc.time_stamp = time_stamp;
			ex_desc.multithreaded = ex_data->multithreaded;
			return resource_manager->LoadCoalescedMeshImplementationEx(meshes, ex_data->multithreaded, metadata->scale_factor, load_descriptor, &ex_desc);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void CreateMeshFromMetadataEx(
		ResourceManager* resource_manager, 
		MeshMetadata* metadata, 
		const GLTFMesh* coalesced_mesh, 
		Stream<Submesh> submeshes, 
		CreateAssetFromMetadataExData* ex_data
	)
	{
		ECS_ASSERT(CreateMeshHelper(metadata, ex_data->mount_point, [&](Stream<wchar_t> file_path, const ResourceManagerLoadDesc& load_descriptor) {
			size_t time_stamp = GetTimeStamp(ex_data->time_stamp, file_path);
			ResourceManagerExDesc ex_desc;
			ex_desc.filename = file_path;
			ex_desc.time_stamp = time_stamp;
			ex_desc.multithreaded = ex_data->multithreaded;
			ex_desc.reference_count = ex_data->reference_count;
			return resource_manager->LoadCoalescedMeshImplementationEx(coalesced_mesh, submeshes, ex_data->multithreaded, metadata->scale_factor, load_descriptor, &ex_desc);
		}));
	}

	// -------------------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	static bool CreateTextureHelper(ResourceManager* resource_manager, TextureMetadata* metadata, Stream<wchar_t> mount_point, Functor&& functor) {
		if (metadata->file.size == 0) {
			// There is no file, fail
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = MountPathOnlyRel(metadata->file, mount_point, absolute_path);

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
		return CreateTextureHelper(resource_manager, metadata, mount_point, [&](Stream<wchar_t> file_path, const ResourceManagerTextureDesc* texture_descriptor, const ResourceManagerLoadDesc& load_desc) {
			// Use reference counting
			return resource_manager->LoadTexture<true>(file_path, texture_descriptor, load_desc);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateTextureFromMetadataEx(
		ResourceManager* resource_manager,
		TextureMetadata* metadata,
		DecodedTexture texture,
		SpinLock* gpu_lock,
		CreateAssetFromMetadataExData* ex_data
	)
	{
		return CreateTextureHelper(resource_manager, metadata, ex_data->mount_point, [&](Stream<wchar_t> file_path, const ResourceManagerTextureDesc* texture_descriptor, ResourceManagerLoadDesc& load_desc) {
			size_t time_stamp = GetTimeStamp(ex_data->time_stamp, file_path);
			load_desc.gpu_lock = gpu_lock;

			ResourceManagerExDesc ex_desc;
			ex_desc.filename = file_path;
			ex_desc.time_stamp = time_stamp;
			ex_desc.multithreaded = ex_data->multithreaded;
			ex_desc.reference_count = ex_data->reference_count;
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
	static bool CreateShaderHelper(ShaderMetadata* metadata, Stream<wchar_t> mount_point, Functor&& functor) {
		if (metadata->file.size == 0) {
			// Return false, no file assigned
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = MountPathOnlyRel(metadata->file, mount_point, absolute_path);

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
		return CreateShaderHelper(metadata, mount_point, [&](Stream<wchar_t> file_path, ShaderCompileOptions compile_options, const ResourceManagerLoadDesc& load_desc) {
			// Use reference counting
			return resource_manager->LoadShader<true>(file_path, metadata->shader_type, {}, compile_options, load_desc);
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
		return CreateShaderHelper(metadata, ex_data->mount_point, [&](Stream<wchar_t> file_path, ShaderCompileOptions compile_options, const ResourceManagerLoadDesc& load_desc) {
			size_t time_stamp = GetTimeStamp(ex_data->time_stamp, file_path);
			ResourceManagerExDesc ex_desc;
			ex_desc.filename = file_path;
			ex_desc.time_stamp = time_stamp;
			ex_desc.multithreaded = ex_data->multithreaded;
			ex_desc.reference_count = ex_data->reference_count;
			return resource_manager->LoadShaderImplementationEx(source_code, metadata->shader_type, ex_data->multithreaded, {}, compile_options, load_desc, &ex_desc);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void ShaderMetadataIdentifier(const ShaderMetadata* metadata, CapacityStream<void>& identifier)
	{
		ShaderCompileOptions options;
		options.compile_flags = metadata->compile_flag;
		options.target = ECS_SHADER_TARGET_5_0;
		options.macros = metadata->macros;
		GenerateShaderCompileOptionsSuffix(options, identifier, metadata->name);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateMaterialFromMetadata(ResourceManager* resource_manager, AssetDatabase* asset_database, MaterialAsset* material, Stream<wchar_t> mount_point, bool dont_load_referenced)
	{
		// Validate the material first
		if (!ValidateAssetMetadataOptions(material, ECS_ASSET_MATERIAL)) {
			return false;
		}

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
		AllocatorPolymorphic allocator = &stack_allocator;
		
		// At the moment allow unbound resources - samplers or textures
		// Instead bind nullptr resources - for samplers it will generate warnings but that is fine

		// For the GPU samplers, if they are not created, we must create them
		for (size_t type = 0; type < ECS_MATERIAL_SHADER_COUNT; type++) {
			for (size_t index = 0; index < material->samplers[type].size; index++) {
				if (material->samplers[type][index].metadata_handle == -1) {
					//return false;
					// Skip the current sampler
					continue;
				}
				GPUSamplerMetadata* metadata = asset_database->GetGPUSampler(material->samplers[type][index].metadata_handle);
				if (!IsAssetPointerValid(metadata->Pointer())) {
					// Create the pointer
					CreateSamplerFromMetadata(resource_manager, metadata);
				}
			}
		}

		UserMaterial user_material;
		ConvertMaterialAssetToUserMaterial(asset_database, material, &user_material, allocator, mount_point);
	
		Material runtime_material;
		ResourceManagerLoadDesc load_desc;
		load_desc.load_flags |= dont_load_referenced ? ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS : 0;
		bool success = resource_manager->LoadUserMaterial(&user_material, &runtime_material, load_desc);

		if (success) {
			AllocatorPolymorphic database_allocator = asset_database->Allocator();
			if (!IsAssetPointerValid(material->material_pointer)) {
				database_allocator.allocation_type = ECS_ALLOCATION_MULTI;
				material->material_pointer = (Material*)Allocate(database_allocator, sizeof(Material));
			}
			//else {
			//	// Allocate first such that we get a new value and then deallocate (it is useful in order to avoid getting the same pointer back)
			//	Material* old_pointer = material->material_pointer;
			//	material->material_pointer = (Material*)Allocate(database_allocator, sizeof(Material));
			//	Deallocate(database_allocator, old_pointer);
			//}
			memcpy(material->material_pointer, &runtime_material, sizeof(runtime_material));
			// Also update the asset pointers for its dependencies when they are loaded the first time
			ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, typed_handles, 128);
			material->GetDependencies(&typed_handles);
			for (unsigned int index = 0; index < typed_handles.size; index++) {
				void* metadata = asset_database->GetAsset(typed_handles[index].handle, typed_handles[index].type);
				Stream<void> current_asset = GetAssetFromMetadata(metadata, typed_handles[index].type);
				if (!IsAssetPointerFromMetadataValid(current_asset)) {
					current_asset = AssetFromResourceManager(resource_manager, metadata, typed_handles[index].type, mount_point);
					// Update the pointer
					SetAssetToMetadata(metadata, typed_handles[index].type, current_asset);
				}
			}

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
		unsigned int texture_total_count = 0;
		unsigned int buffer_total_count = 0;
		unsigned int sampler_total_count = 0;
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			texture_total_count += material->textures[index].size;
			buffer_total_count += material->buffers[index].size;
			sampler_total_count += material->samplers[index].size;
		}

		user_material->textures.Initialize(allocator, texture_total_count);
		user_material->buffers.Initialize(allocator, buffer_total_count);
		user_material->samplers.Initialize(allocator, sampler_total_count);

		texture_total_count = 0;
		buffer_total_count = 0;
		sampler_total_count = 0;
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			ECS_SHADER_TYPE shader_type = GetShaderFromMaterialShaderType((ECS_MATERIAL_SHADER)index);
			for (size_t subindex = 0; subindex < material->textures[index].size; subindex++) {
				user_material->textures[texture_total_count].shader_type = shader_type;
				user_material->textures[texture_total_count].slot = material->textures[index][subindex].slot;
				if (material->textures[index][subindex].metadata_handle != -1) {
					const TextureMetadata* texture_metadata = database->GetTextureConst(material->textures[index][subindex].metadata_handle);
					Stream<wchar_t> final_texture_path = MountPath(texture_metadata->file, mount_point, allocator);

					user_material->textures[texture_total_count].filename = final_texture_path;
					user_material->textures[texture_total_count].srgb = texture_metadata->sRGB;
					user_material->textures[texture_total_count].generate_mips = texture_metadata->generate_mip_maps;
					user_material->textures[texture_total_count].compression = texture_metadata->compression_type;
					user_material->textures[texture_total_count].name = texture_metadata->name;
				}
				else {
					user_material->textures[texture_total_count].filename = { nullptr, 0 };
				}
				texture_total_count++;
			}

			for (size_t subindex = 0; subindex < material->buffers[index].size; subindex++) {
				user_material->buffers[buffer_total_count].data = material->buffers[index][subindex].data;
				user_material->buffers[buffer_total_count].dynamic = material->buffers[index][subindex].dynamic;
				user_material->buffers[buffer_total_count].shader_type = shader_type;
				user_material->buffers[buffer_total_count].slot = material->buffers[index][subindex].slot;

				user_material->buffers[buffer_total_count].tags = { nullptr, 0 };
				if (material->buffers[index][subindex].reflection_type != nullptr) {
					// Check to see if the material has Color fields - these are passed as float4
					// For the constant buffers and we need to splat the color into a color float
					Stream<Reflection::ReflectionField> fields = material->buffers[index][subindex].reflection_type->fields;
					bool has_color_fields = false;
					for (size_t field_index = 0; field_index < fields.size && !has_color_fields; field_index++) {
						if (fields[field_index].definition == STRING(Color)) {
							has_color_fields = true;
						}
					}

					if (has_color_fields) {
						// We need to allocate a new data pointer from the temporary allocator such that
						// We can override the color value with the color float value
						Stream<void> material_data = user_material->buffers[buffer_total_count].data;
						user_material->buffers[buffer_total_count].data.buffer = Allocate(allocator, material_data.size);
						material_data.CopyTo(user_material->buffers[buffer_total_count].data.buffer);
					}

					// Now we need to add the existing tags and change the colors to color floats
					for (size_t field_index = 0; field_index < fields.size; field_index++) {
						if (fields[field_index].tag.size > 0) {
							user_material->buffers[buffer_total_count].tags.AddResize({ fields[field_index].tag, fields[field_index].info.pointer_offset }, allocator, true);
						}
						if (fields[field_index].definition == "Color") {
							// We need to change the field value from a color into a color float
							if (field_index < fields.size - 1) {
								ECS_ASSERT(fields[field_index].info.pointer_offset + sizeof(ColorFloat) <= fields[field_index + 1].info.pointer_offset);
							}
							else {
								ECS_ASSERT(fields[field_index].info.pointer_offset + sizeof(ColorFloat) <= material->buffers[index][subindex].reflection_type->byte_size);
							}
							Color* color_value = (Color*)OffsetPointer(user_material->buffers[buffer_total_count].data.buffer, fields[field_index].info.pointer_offset);
							ColorFloat* color_float_value = (ColorFloat*)color_value;
							Color current_color_value = *color_value;
							*color_float_value = current_color_value;
						}
					}

					if (user_material->buffers[buffer_total_count].tags.size > 0) {
						// Now remove all tags that come from compound elements
						for (size_t tag_index = 0; tag_index < user_material->buffers[buffer_total_count].tags.size - 1; tag_index++) {
							if (user_material->buffers[buffer_total_count].tags[tag_index].string ==
								user_material->buffers[buffer_total_count].tags[tag_index + 1].string) {
								user_material->buffers[buffer_total_count].tags.Remove(tag_index + 1);
								tag_index--;
							}
						}
					}
				}

				buffer_total_count++;
			}

			for (size_t subindex = 0; subindex < material->samplers[index].size; subindex++) {
				user_material->samplers[sampler_total_count].shader_type = shader_type;
				user_material->samplers[sampler_total_count].slot = material->samplers[index][subindex].slot;
				if (material->samplers[index][subindex].metadata_handle != -1) {
					const GPUSamplerMetadata* gpu_sampler = database->GetGPUSamplerConst(material->samplers[index][subindex].metadata_handle);
					user_material->samplers[sampler_total_count].state = gpu_sampler->sampler;
				}
				else {
					user_material->samplers[sampler_total_count].state = { nullptr };
				}
				sampler_total_count++;
			}
		}

		if (material->pixel_shader_handle != -1) {
			const ShaderMetadata* pixel_shader_metadata = database->GetShaderConst(material->pixel_shader_handle);

			// Mount the shaders only if they are not in absolute paths already
			Stream<wchar_t> pixel_shader_path = MountPathOnlyRel(pixel_shader_metadata->file, mount_point, allocator);
			user_material->pixel_shader = pixel_shader_path;
			user_material->pixel_compile_options.compile_flags = pixel_shader_metadata->compile_flag;
			user_material->pixel_compile_options.macros = pixel_shader_metadata->macros;
			user_material->pixel_compile_options.target = ECS_SHADER_TARGET_5_0;
			user_material->pixel_shader_name = pixel_shader_metadata->name;
		}
		else {
			user_material->pixel_shader = { nullptr, 0 };
		}

		if (material->vertex_shader_handle != -1) {
			const ShaderMetadata* vertex_shader_metadata = database->GetShaderConst(material->vertex_shader_handle);

			Stream<wchar_t> vertex_shader_path = MountPathOnlyRel(vertex_shader_metadata->file, mount_point, allocator);
			user_material->vertex_shader = vertex_shader_path;
			user_material->vertex_compile_options.compile_flags = vertex_shader_metadata->compile_flag;
			user_material->vertex_compile_options.macros = vertex_shader_metadata->macros;
			user_material->vertex_compile_options.target = ECS_SHADER_TARGET_5_0;
			user_material->vertex_shader_name = vertex_shader_metadata->name;
		}
		else {
			user_material->vertex_shader = { nullptr, 0 };
		}

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
		Stream<wchar_t> final_path = MountPathOnlyRel(misc_asset->file, mount, storage);
		ResizableStream<void> data = *resource_manager->LoadMisc<true>(final_path);
		if (data.size > 0 && data.buffer != nullptr) {
			misc_asset->data = { data.buffer, data.size };
			return true;
		}
		return false;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void MiscMetadataIdentifier(const MiscAsset* misc, CapacityStream<void>& identifier)
	{
		identifier.Add(misc->name);
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
			break;
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_MATERIAL:
			break;
		case ECS_ASSET_MISC:
			MiscMetadataIdentifier((const MiscAsset*)metadata, identifier);
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
			AssetMetadataIdentifier(metadata, type, identifier);

			CapacityStream<wchar_t> wide_identifier;
			wide_identifier.InitializeFromBuffer(path_identifier.buffer, 0, path_identifier.capacity);
			file = MountPathOnlyRel(file, mount_point, wide_identifier);
			identifier.CopyTo(file.buffer + file.size);
			path_identifier.size = file.size + identifier.size;
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_MATERIAL:
		{
			// Nothing to do here - these are not stored in the resource manager
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

	Stream<void> AssetFromResourceManager(const ResourceManager* resource_manager, const void* metadata, ECS_ASSET_TYPE type, Stream<wchar_t> mount_point)
	{
		if (type == ECS_ASSET_GPU_SAMPLER || type == ECS_ASSET_MATERIAL) {
			return GetAssetFromMetadata(metadata, type);
		}
		else if (type != ECS_ASSET_MESH && type != ECS_ASSET_TEXTURE && type != ECS_ASSET_SHADER && type != ECS_ASSET_MISC) {
			ECS_ASSERT(false);
			return { nullptr, 0 };
		}
		else {
			ECS_STACK_VOID_STREAM(suffix, 512);
			AssetMetadataIdentifier(metadata, type, suffix);
			ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
			Stream<wchar_t> file_path = MountPathOnlyRel(GetAssetFile(metadata, type), mount_point, absolute_path);

			const void* resource = resource_manager->GetResource(file_path, AssetTypeToResourceType(type), suffix);
			if (type == ECS_ASSET_MISC) {
				if (resource != nullptr) {
					ResizableStream<void>* data = (ResizableStream<void>*)resource;
					return data->ToStream();
				}
				else {
					return { nullptr, 0 };
				}
			}
			return { resource, 0 };
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsMeshFromMetadataLoaded(const ResourceManager* resource_manager, const MeshMetadata* metadata, Stream<wchar_t> mount_point)
	{
		return IsAssetPointerValid(AssetFromResourceManager(resource_manager, metadata, ECS_ASSET_MESH, mount_point).buffer);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsMeshFromMetadataLoadedAndAssign(const ResourceManager* resource_manager, MeshMetadata* metadata, Stream<wchar_t> mount_point, bool* has_assigned)
	{
		void* resource_pointer = AssetFromResourceManager(resource_manager, metadata, ECS_ASSET_MESH, mount_point).buffer;
		if (metadata->Pointer() != resource_pointer) {
			if (has_assigned != nullptr) {
				*has_assigned = true;
			}
			*metadata->PtrToPointer() = resource_pointer;
		}
		return IsAssetPointerValid(resource_pointer);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsTextureFromMetadataLoaded(const ResourceManager* resource_manager, const TextureMetadata* metadata, Stream<wchar_t> mount_point)
	{
		return IsAssetPointerValid(AssetFromResourceManager(resource_manager, metadata, ECS_ASSET_TEXTURE, mount_point).buffer);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsTextureFromMetadataLoadedAndAssign(const ResourceManager* resource_manager, TextureMetadata* metadata, Stream<wchar_t> mount_point, bool* has_assigned)
	{
		void* resource_pointer = AssetFromResourceManager(resource_manager, metadata, ECS_ASSET_TEXTURE, mount_point).buffer;
		if (metadata->Pointer() != resource_pointer) {
			if (has_assigned != nullptr) {
				*has_assigned = true;
			}
			*metadata->PtrToPointer() = resource_pointer;
		}
		return IsAssetPointerValid(resource_pointer);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsGPUSamplerFromMetadataLoaded(const GPUSamplerMetadata* metadata, bool randomized_asset)
	{
		if (!randomized_asset) {
			return metadata->sampler.Interface() != nullptr;
		}
		else {
			return IsAssetPointerValid(metadata->sampler.Interface());
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsShaderFromMetadataLoaded(const ResourceManager* resource_manager, const ShaderMetadata* metadata, Stream<wchar_t> mount_point)
	{
		return IsAssetPointerValid(AssetFromResourceManager(resource_manager, metadata, ECS_ASSET_SHADER, mount_point).buffer);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsShaderFromMetadataLoadedAndAssign(const ResourceManager* resource_manager, ShaderMetadata* metadata, Stream<wchar_t> mount_point, bool* has_assigned)
	{
		void* resource_pointer = AssetFromResourceManager(resource_manager, metadata, ECS_ASSET_SHADER, mount_point).buffer;
		if (metadata->Pointer() != resource_pointer) {
			if (has_assigned != nullptr) {
				*has_assigned = true;
			}
			*metadata->PtrToPointer() = resource_pointer;
		}
		return IsAssetPointerValid(resource_pointer);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsMaterialFromMetadataLoaded(const MaterialAsset* metadata, bool randomized_asset)
	{
		if (!randomized_asset) {
			return metadata->material_pointer != nullptr;
		}
		else {
			return IsAssetPointerValid(metadata->material_pointer);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

	// The functors receives as parameters (const AssetMetadataType* metadata, Stream<wchar_t> mount_point)
	template<typename TextureFunctor, typename SamplerFunctor, typename ShaderFunctor>
	bool IsMaterialFromMetadataLoadedExImplementation(
		const MaterialAsset* metadata,
		const AssetDatabase* database,
		IsMaterialFromMetadataLoadedExDesc* load_desc,
		TextureFunctor&& texture_functor,
		SamplerFunctor&& sampler_functor,
		ShaderFunctor&& shader_functor
	) {
		bool has_typed_handles = false;
		bool has_error_string = false;
		bool has_segmened_error_string = false;
		bool early_exit = true;
		Stream<wchar_t> mount_point = { nullptr, 0 };

		if (load_desc != nullptr) {
			load_desc->dependencies_are_ready = false;
			has_typed_handles = load_desc->missing_handles != nullptr;
			has_error_string = load_desc->error_string != nullptr;
			has_segmened_error_string = load_desc->segmented_error_string != nullptr;
			ECS_ASSERT(!has_segmened_error_string || has_error_string);

			mount_point = load_desc->mount_point;

			early_exit &= !has_typed_handles;
			early_exit &= !has_error_string;
			early_exit &= !has_segmened_error_string;
		}

		bool success = true;

		bool is_loaded = IsMaterialFromMetadataLoaded(metadata, true);
		if (!is_loaded && early_exit) {
			return false;
		}

		auto fail = [&](unsigned int handle, ECS_ASSET_TYPE type, const char* not_loaded_string, const char* invalid_handle_string, Stream<char> asset_name = { nullptr, 0 }) {
			success = false;
			if (has_typed_handles) {
				load_desc->missing_handles->AddAssert({ handle, type });
			}

			Stream<char> previous_error_string = *load_desc->error_string;
			if (has_error_string) {
				if (load_desc->error_string->size == 0) {
					// Add a basic message
					load_desc->error_string->AddStreamAssert("The material has missing/unspecified referenced assets:\n");
					if (has_segmened_error_string) {
						load_desc->segmented_error_string->AddAssert(*load_desc->error_string);
						load_desc->segmented_error_string->buffer[0].size--;
					}
					previous_error_string = *load_desc->error_string;
				}

				if (handle == -1) {
					if (asset_name.size == 0) {
						load_desc->error_string->AddStreamAssert(invalid_handle_string);
					}
					else {
						FormatString(*load_desc->error_string, invalid_handle_string, asset_name);
					}
				}
				else {
					ECS_STACK_CAPACITY_STREAM(char, asset_string, 256);
					AssetToString(database->GetAssetConst(handle, type), type, asset_string);
					FormatString(*load_desc->error_string, not_loaded_string, asset_string);
				}
				load_desc->error_string->AddAssert('\n');
			}

			if (has_segmened_error_string) {
				load_desc->segmented_error_string->AddAssert({ previous_error_string.buffer + previous_error_string.size, load_desc->error_string->size - previous_error_string.size - 1 });
			}
		};

		// Start with the textures
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			for (size_t subindex = 0; subindex < metadata->textures[index].size; subindex++) {
				unsigned int handle = metadata->textures[index][subindex].metadata_handle;
				if (handle == -1 || !texture_functor(database->GetTextureConst(handle), mount_point)) {
					if (early_exit) {
						return false;
					}
					else {
						fail(handle, ECS_ASSET_TEXTURE, "Texture {#} is not loaded", "Texture {#} is unspecified", metadata->textures[index][subindex].name);
					}
				}
			}
		}

		// Continue with the shaders
		if (metadata->vertex_shader_handle == -1 || !shader_functor(database->GetShaderConst(metadata->vertex_shader_handle), mount_point)) {
			if (early_exit) {
				return false;
			}
			else {
				fail(metadata->vertex_shader_handle, ECS_ASSET_SHADER, "The vertex shader {#} is not loaded", "The vertex shader is unspecified");
			}
		}

		if (metadata->pixel_shader_handle == -1 || !shader_functor(database->GetShaderConst(metadata->pixel_shader_handle), mount_point)) {
			if (early_exit) {
				return false;
			}
			else {
				fail(metadata->pixel_shader_handle, ECS_ASSET_SHADER, "The pixel shader {#} is not loaded", "The pixel shader is unspecified");
			}
		}

		// Verify the samplers as well (if they are empty)
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			for (size_t subindex = 0; subindex < metadata->samplers[index].size; subindex++) {
				unsigned int handle = metadata->samplers[index][subindex].metadata_handle;
				if (handle == -1 || !sampler_functor(database->GetGPUSamplerConst(handle), mount_point)) {
					if (early_exit) {
						return false;
					}
					else {
						fail(handle, ECS_ASSET_GPU_SAMPLER, "GPU Sampler {#} is not created", "The GPU Sampler {#} is unspecified", metadata->samplers[index][subindex].name);
					}
				}
			}
		}

		// Add a basic error message
		if (has_error_string) {
			if (success && !is_loaded) {
				if (load_desc != nullptr) {
					load_desc->dependencies_are_ready = true;
					load_desc->error_string->AddStreamAssert("The material is not created but dependencies are ready");
					if (has_segmened_error_string) {
						load_desc->segmented_error_string->AddAssert(*load_desc->error_string);
					}
					if (has_typed_handles) {
						load_desc->missing_handles->AddAssert({ 0, ECS_ASSET_MATERIAL });
					}
				}
			}
		}

		return success && is_loaded;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsMaterialFromMetadataLoadedEx(
		const MaterialAsset* metadata, 
		const ResourceManager* resource_manager, 
		const AssetDatabase* database, 
		IsMaterialFromMetadataLoadedExDesc* load_desc
	)
	{
		return IsMaterialFromMetadataLoadedExImplementation(
			metadata,
			database,
			load_desc,
			[resource_manager](const TextureMetadata* texture, Stream<wchar_t> mount_point) {
				return IsTextureFromMetadataLoaded(resource_manager, texture, mount_point);
			},
			[resource_manager](const GPUSamplerMetadata* sampler, Stream<wchar_t> mount_point) {
				return IsGPUSamplerFromMetadataLoaded(sampler, true);
			},
			[resource_manager](const ShaderMetadata* shader, Stream<wchar_t> mount_point) {
				return IsShaderFromMetadataLoaded(resource_manager, shader, mount_point);
			}
		);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsMaterialFromMetadataLoadedEx(const MaterialAsset* metadata, const AssetDatabase* database, IsMaterialFromMetadataLoadedExDesc* load_desc)
	{
		return IsMaterialFromMetadataLoadedExImplementation(
			metadata,
			database,
			load_desc,
			[](const TextureMetadata* texture, Stream<wchar_t> mount_point) {
				return IsAssetFromMetadataValid(texture, ECS_ASSET_TEXTURE);
			},
			[](const GPUSamplerMetadata* sampler, Stream<wchar_t> mount_point) {
				return IsAssetFromMetadataValid(sampler, ECS_ASSET_GPU_SAMPLER);
			},
			[](const ShaderMetadata* shader, Stream<wchar_t> mount_point) {
				return IsAssetFromMetadataValid(shader, ECS_ASSET_SHADER);
			}
		);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsMiscFromMetadataLoaded(const ResourceManager* resource_manager, const MiscAsset* metadata, Stream<wchar_t> mount_point)
	{
		return IsAssetPointerValid(AssetFromResourceManager(resource_manager, metadata, ECS_ASSET_MISC, mount_point).buffer);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool IsMiscFromMetadataLoadedAndAssign(const ResourceManager* resource_manager, MiscAsset* metadata, Stream<wchar_t> mount_point, bool* has_assigned)
	{
		Stream<void> resolved_asset = AssetFromResourceManager(resource_manager, metadata, ECS_ASSET_MISC, mount_point);
		if (metadata->data.data != resolved_asset) {
			if (has_assigned != nullptr) {
				*has_assigned = true;
			}
			metadata->data.data = resolved_asset;
		}
		return IsAssetPointerValid(resolved_asset.buffer);
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

	bool IsAssetFromMetadataLoadedAndAssign(const ResourceManager* resource_manager, void* metadata, ECS_ASSET_TYPE type, Stream<wchar_t> mount_point, bool randomized_asset, bool* has_assigned)
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return IsMeshFromMetadataLoadedAndAssign(resource_manager, (MeshMetadata*)metadata, mount_point, has_assigned);
		case ECS_ASSET_TEXTURE:
			return IsTextureFromMetadataLoadedAndAssign(resource_manager, (TextureMetadata*)metadata, mount_point, has_assigned);
		case ECS_ASSET_GPU_SAMPLER:
			return IsGPUSamplerFromMetadataLoaded((const GPUSamplerMetadata*)metadata, randomized_asset);
		case ECS_ASSET_SHADER:
			return IsShaderFromMetadataLoadedAndAssign(resource_manager, (ShaderMetadata*)metadata, mount_point, has_assigned);
		case ECS_ASSET_MATERIAL:
			return IsMaterialFromMetadataLoaded((const MaterialAsset*)metadata, randomized_asset);
		case ECS_ASSET_MISC:
			return IsMiscFromMetadataLoadedAndAssign(resource_manager, (MiscAsset*)metadata, mount_point, has_assigned);
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
					missing_handles[type].AddAssert(handle);
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
			AssetDatabase* main_database = database->GetDatabase();
			unsigned int count = database->GetCount(type);

			for (unsigned int index = 0; index < count; index++) {
				unsigned int handle = database->GetHandle(index, type);
				const void* current_asset = main_database->GetAssetConst(handle, type);
				// Check to see if the handle was already added
				unsigned int existing_index = SearchBytes(missing_handles[type].buffer, missing_handles[type].size, handle);
				if (existing_index == -1 && !IsAssetFromMetadataLoaded(resource_manager, current_asset, type, mount_point, randomized_assets)) {
					missing_handles[type].AddAssert(handle);
					// Add its dependencies as well if they don't exist already
					ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
					GetAssetDependencies(current_asset, type, &dependencies);
					for (unsigned int subindex = 0; subindex < dependencies.size; subindex++) {
						CapacityStream<unsigned int>* current_handles = missing_handles + dependencies[subindex].type;
						existing_index = SearchBytes(
							current_handles->buffer,
							current_handles->size,
							dependencies[subindex].handle
						);
						current_asset = main_database->GetAssetConst(dependencies[subindex].handle, dependencies[subindex].type);
						if (existing_index == -1 && !IsAssetFromMetadataLoaded(resource_manager, current_asset, dependencies[subindex].type, mount_point, randomized_assets)) {
							current_handles->AddAssert(dependencies[subindex].handle);
						}
					}
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

	bool DeallocateMeshFromMetadata(ResourceManager* resource_manager, const MeshMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = MountPathOnlyRel(metadata->file, mount_point, absolute_path);

		ECS_STACK_VOID_STREAM(suffix, 512);
		MeshMetadataIdentifier(metadata, suffix);
		ResourceManagerLoadDesc load_desc;
		load_desc.identifier_suffix = suffix;

		return resource_manager->TryUnloadResource(file_path, ResourceType::CoalescedMesh, load_desc);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateTextureFromMetadata(ResourceManager* resource_manager, const TextureMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = MountPathOnlyRel(metadata->file, mount_point, absolute_path);

		ECS_STACK_VOID_STREAM(suffix, 512);
		TextureMetadataIdentifier(metadata, suffix);
		ResourceManagerLoadDesc load_desc;
		load_desc.identifier_suffix = suffix;
		return resource_manager->TryUnloadResource(file_path, ResourceType::Texture, load_desc);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void DeallocateSamplerFromMetadata(ResourceManager* resource_manager, const GPUSamplerMetadata* metadata)
	{
		// We don't need to deallocate the sampler - it is automatically managed by the Graphics object
		//resource_manager->m_graphics->FreeResource(metadata->sampler);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateShaderFromMetadata(ResourceManager* resource_manager, const ShaderMetadata* metadata, Stream<wchar_t> mount_point)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
		Stream<wchar_t> file_path = MountPathOnlyRel(metadata->file, mount_point, absolute_path);

		ECS_STACK_VOID_STREAM(suffix, 512);
		ShaderMetadataIdentifier(metadata, suffix);
		ResourceManagerLoadDesc load_desc;
		load_desc.identifier_suffix = suffix;

		bool unloaded = resource_manager->TryUnloadResource(file_path, ResourceType::Shader, load_desc);
		return unloaded;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void DeallocateMaterialFromMetadata(
		ResourceManager* resource_manager, 
		const MaterialAsset* material, 
		const AssetDatabase* database, 
		Stream<wchar_t> mount_point,
		bool check_resource
	)
	{
		// Deallocate the textures and the buffers
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);

		UserMaterial user_material;
		ConvertMaterialAssetToUserMaterial(database, material, &user_material, &stack_allocator, mount_point);
		ResourceManagerLoadDesc load_desc;
		load_desc.load_flags = check_resource ? ECS_RESOURCE_MANAGER_USER_MATERIAL_CHECK_RESOURCE : 0;
		load_desc.load_flags |= ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_FREE_SAMPLERS;
		resource_manager->UnloadUserMaterial(&user_material, (Material*)material->material_pointer, load_desc);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateMiscAssetFromMetadata(ResourceManager* resource_manager, const MiscAsset* misc, Stream<wchar_t> mount_point)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);
		Stream<wchar_t> final_path = MountPathOnlyRel(misc->file, mount_point, storage);
		return resource_manager->TryUnloadResource(final_path, ResourceType::Misc);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool DeallocateAssetFromMetadata(
		ResourceManager* resource_manager, 
		const AssetDatabase* database, 
		const void* metadata, 
		ECS_ASSET_TYPE type,
		Stream<wchar_t> mount_point,
		DeallocateAssetFromMetadataOptions options
	)
	{
		// If the pointer is randomized, skip the deallocation
		if (!IsAssetFromMetadataValid(metadata, type)) {
			return true;
		}

		switch (type) {
		case ECS_ASSET_MESH:
		{
			return DeallocateMeshFromMetadata(resource_manager, (const MeshMetadata*)metadata, mount_point);
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			return DeallocateTextureFromMetadata(resource_manager, (const TextureMetadata*)metadata, mount_point);
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			DeallocateSamplerFromMetadata(resource_manager, (const GPUSamplerMetadata*)metadata);
			return true;
		}
		break;
		case ECS_ASSET_SHADER:
		{
			return DeallocateShaderFromMetadata(resource_manager, (const ShaderMetadata*)metadata, mount_point);
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			DeallocateMaterialFromMetadata(resource_manager, (const MaterialAsset*)metadata, database, mount_point, options.material_check_resource);
			return true;
		}
		break;
		case ECS_ASSET_MISC:
		{
			return DeallocateMiscAssetFromMetadata(resource_manager, (const MiscAsset*)metadata, mount_point);
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
		return false;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool2 ReloadAssetFromMetadata(
		ResourceManager* resource_manager, 
		AssetDatabase* database, 
		void* metadata, 
		ECS_ASSET_TYPE type, 
		Stream<wchar_t> mount_point,
		CapacityStream<AssetTypedHandle>* remove_dependencies
	)
	{
		switch (type) {
		case ECS_ASSET_MESH:
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		case ECS_ASSET_MISC:
		{
			// For all asset types except materials do the deallocation first and then do the creation
			bool success = DeallocateAssetFromMetadata(resource_manager, database, metadata, type, mount_point);
			if (success) {
				return { true, CreateAssetFromMetadata(resource_manager, database, metadata, type, mount_point) };
			}
		}
			break;
		case ECS_ASSET_MATERIAL:
		{
			MaterialAsset* current_material = (MaterialAsset*)metadata;

			// Reload the shader dependencies - they will check to see if anything has changed and only perform
			// modifications if they have actually changed. Don't check the result of the reloading of parameters
			// If it fails then the parameters will stay the same.
			if (current_material->vertex_shader_handle != -1) {
				const ShaderMetadata* vertex_shader = database->GetShaderConst(current_material->vertex_shader_handle);
				ReloadMaterialMetadataFromShader(
					resource_manager, 
					database, 
					current_material, 
					vertex_shader, 
					mount_point,
					remove_dependencies
				);
			}
			if (current_material->pixel_shader_handle != -1) {
				const ShaderMetadata* pixel_shader = database->GetShaderConst(current_material->pixel_shader_handle);
				ReloadMaterialMetadataFromShader(
					resource_manager,
					database,
					current_material,
					pixel_shader,
					mount_point,
					remove_dependencies
				);
			}

			// For materials do the creation into a temporary and then do the deallocation.
			// This is in order to have the material increment textures and shaders that are still in use
			// without deloading them and having them to be reloaded
			MaterialAsset temporary;
			memcpy(&temporary, current_material, sizeof(temporary));
			// Change the pointer value such that it will not in case the creation fails

			Material temporary_material;
			temporary.material_pointer = &temporary_material;

			DeallocateAssetFromMetadataOptions options;
			options.material_check_resource = true;
			bool success = CreateAssetFromMetadata(resource_manager, database, &temporary, ECS_ASSET_MATERIAL, mount_point);		
			bool deallocation_success = DeallocateAssetFromMetadata(resource_manager, database, metadata, type, mount_point, options);
			if (deallocation_success) {
				// Copy the material contents
				// If the material pointer is not yet allocated because the material previously failed creating
				// then allocate the pointer now
				if (!IsAssetPointerValid(current_material->material_pointer)) {
					current_material->material_pointer = (Material*)Allocate(database->Allocator(), sizeof(Material));
				}
				memcpy(current_material->material_pointer, &temporary_material, sizeof(temporary_material));
			}
			else {
				// Try to deallocate the newly created asset since we couldn't deallocate the previous one
				ECS_ASSERT(DeallocateAssetFromMetadata(resource_manager, database, &temporary, type, mount_point, options));
			}

			return { deallocation_success, success };
		}
			break;
		default:
			ECS_ASSERT(false, "Invalid asset type.");
		}
		
		ECS_ASSERT(false);
		return { false, false };
	}

	// -------------------------------------------------------------------------------------------------------------------------

	ReloadAssetResult ReloadAssetFromMetadata(
		ResourceManager* resource_manager, 
		AssetDatabase* database, 
		const void* previous_metadata, 
		void* current_metadata, 
		ECS_ASSET_TYPE type, 
		Stream<wchar_t> mount_point,
		CapacityStream<AssetTypedHandle>* remove_dependencies
	)
	{
		ReloadAssetResult reload_result;

		CompareAssetsResult compare_result = CompareAssetOptionsEx(previous_metadata, current_metadata, type);

		switch (type) {
		case ECS_ASSET_MESH:
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		case ECS_ASSET_MISC:
			reload_result.is_different = !compare_result.is_the_same;
			if (reload_result.is_different) {
				DeallocateAssetFromMetadataOptions options;
				options.material_check_resource = true;
				// Deallocate the old asset and the create it back - these assets have no dependencies
				reload_result.success.x = DeallocateAssetFromMetadata(resource_manager, database, previous_metadata, type, mount_point, options);
				if (reload_result.success.x) {
					reload_result.success.y = CreateAssetFromMetadata(resource_manager, database, current_metadata, type, mount_point);
				}
			}
			break;
		case ECS_ASSET_MATERIAL:
			reload_result.is_different = compare_result.material_result.is_different;
			if (reload_result.is_different) {
				MaterialAsset* current_material = (MaterialAsset*)current_metadata;
				const MaterialAsset* previous_material = (const MaterialAsset*)previous_metadata;

				// Write into a temporary material first the old contents and then use the reload function
				// At last copy into the current material pointers
				Material temporary_material;
				ReloadUserMaterialOptions reload_material;
				if (IsAssetPointerValid(previous_material->material_pointer)) {
					reload_material.reload_buffers = compare_result.material_result.buffers_different;
					reload_material.reload_pixel_shader = compare_result.material_result.pixel_shader_different;
					reload_material.reload_vertex_shader = compare_result.material_result.vertex_shader_different;
					reload_material.reload_samplers = compare_result.material_result.samplers_different;
					reload_material.reload_textures = compare_result.material_result.textures_different;
					memcpy(&temporary_material, previous_material->material_pointer, sizeof(temporary_material));
				}
				else {
					// Zero out the temporary material if the previous pointer is nullptr
					memset(&temporary_material, 0, sizeof(temporary_material));
					// Also signal to the reload that all GPU resources need to be reloaded
					memset(&reload_material, 1, sizeof(reload_material));
				}

				// If a GPU sampler is not created from the newly added one, create it
				for (size_t shader = 0; shader < ECS_MATERIAL_SHADER_COUNT; shader++) {
					for (size_t index = 0; index < current_material->samplers[shader].size; index++) {
						unsigned int handle = current_material->samplers[shader][index].metadata_handle;
						if (handle != -1) {
							GPUSamplerMetadata* metadata = database->GetGPUSampler(handle);
							if (!IsAssetPointerValid(metadata->Pointer())) {
								CreateSamplerFromMetadata(resource_manager, metadata);
							}
						}
					}
				}

				UserMaterial previous_user_material;
				UserMaterial current_user_material;
				ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
				ConvertMaterialAssetToUserMaterial(database, current_material, &current_user_material, &stack_allocator, mount_point);
				// Perform the conversion only if the previous was valid
				if (IsAssetPointerValid(previous_material->material_pointer)) {
					ConvertMaterialAssetToUserMaterial(database, previous_material, &previous_user_material, &stack_allocator, mount_point);
				}
				else {
					memset(&previous_user_material, 0, sizeof(previous_user_material));
				}

				ResourceManagerLoadDesc load_desc;
				load_desc.load_flags |= ECS_RESOURCE_MANAGER_USER_MATERIAL_CHECK_RESOURCE;
				load_desc.load_flags |= ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_FREE_SAMPLERS;
				reload_result.success.x = ReloadUserMaterial(resource_manager, &previous_user_material, &current_user_material, &temporary_material, reload_material, load_desc);
				reload_result.success.y = reload_result.success.x;

				if (reload_result.success.x) {
					// If the current pointer is not allocated, allocate it
					if (!IsAssetPointerValid(current_material->material_pointer)) {
						current_material->material_pointer = (Material*)Allocate(database->Allocator(), sizeof(Material));
					}
					memcpy(current_material->material_pointer, &temporary_material, sizeof(temporary_material));
				}
			}
			break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		return reload_result;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	ECS_RELOAD_ASSET_METADATA_STATUS ReloadMaterialMetadataFromShader(
		ResourceManager* resource_manager,
		AssetDatabase* database,
		MaterialAsset* material,
		const ShaderMetadata* shader, 
		Stream<wchar_t> mount_point,
		CapacityStream<AssetTypedHandle>* remove_dependencies
	)
	{
		ChangeMaterialDependencies change_dependencies;
		change_dependencies.database = database;
		change_dependencies.handles = remove_dependencies;
		if (material->vertex_shader_handle != -1) {
			const ShaderMetadata* vertex_shader = database->GetShaderConst(material->vertex_shader_handle);
			if (vertex_shader == shader) {
				// Reflect the shader again
				return ReflectMaterialShaderParameters(
					resource_manager,
					&change_dependencies,
					material, 
					vertex_shader, 
					ECS_MATERIAL_SHADER_VERTEX, 
					database->Allocator(), 
					mount_point
				);
			}
		}

		if (material->pixel_shader_handle != -1) {
			const ShaderMetadata* pixel_shader = database->GetShaderConst(material->pixel_shader_handle);
			if (pixel_shader == shader) {
				return ReflectMaterialShaderParameters(
					resource_manager, 
					&change_dependencies,
					material, 
					pixel_shader, 
					ECS_MATERIAL_SHADER_PIXEL, 
					database->Allocator(), 
					mount_point
				);
			}
		}

		return ECS_RELOAD_ASSET_METADATA_NO_CHANGE;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	ECS_RELOAD_ASSET_METADATA_STATUS ReloadAssetMetadataFromTargetDependency(
		ResourceManager* resource_manager, 
		AssetDatabase* database, 
		void* metadata, 
		ECS_ASSET_TYPE type, 
		const void* changed_metadata, 
		ECS_ASSET_TYPE changed_metadata_type, 
		Stream<wchar_t> mount_point,
		CapacityStream<AssetTypedHandle>* remove_dependencies
	)
	{
		if (IsAssetTypeMetadataWithDependencies(type)) {
			ECS_STACK_CAPACITY_STREAM(ECS_ASSET_TYPE, dependent_types, 16);
			FillAssetTypeMetadataWithDependencies(type, &dependent_types);

			unsigned int index = 0;
			for (; index < dependent_types.size; index++) {
				if (dependent_types[index] == changed_metadata_type) {
					break;
				}
			}

			if (index < dependent_types.size) {
				// The types are conformant - check now the values
				if (type == ECS_ASSET_MATERIAL) {
					MaterialAsset* material = (MaterialAsset*)metadata;
					const ShaderMetadata* shader = (const ShaderMetadata*)changed_metadata;

					return ReloadMaterialMetadataFromShader(resource_manager, database, material, shader, mount_point, remove_dependencies);
				}
				else {
					ECS_ASSERT(false);
				}
			}
			return ECS_RELOAD_ASSET_METADATA_NO_CHANGE;
		}
		return ECS_RELOAD_ASSET_METADATA_NO_CHANGE;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void ConvertMaterialFromDatabaseToDatabase(
		const MaterialAsset* material, 
		MaterialAsset* output_material,
		const AssetDatabase* original_database, 
		AssetDatabase* target_database, 
		AllocatorPolymorphic allocator,
		ConvertAssetFromDatabaseToDatabaseOptions options
	)
	{
		bool force_addition = options.force_addition;

		unsigned int counts[ECS_MATERIAL_SHADER_COUNT * 3];
		material->WriteCounts(true, true, true, counts);
		output_material->Resize(counts, allocator, true);
		output_material->reflection_manager = material->reflection_manager;
		output_material->material_pointer = material->material_pointer;
		output_material->name = material->name;
		if (!options.handles_only) {
			output_material->name = StringCopy(allocator, material->name);
		}

		unsigned int vertex_handle = material->vertex_shader_handle;
		if (vertex_handle != -1) {
			const ShaderMetadata* shader = original_database->GetShaderConst(vertex_handle);
			Stream<char> name = shader->name;
			Stream<wchar_t> file = shader->file;
			vertex_handle = target_database->FindAsset(name, file, ECS_ASSET_SHADER);
			if (force_addition && vertex_handle == -1) {
				vertex_handle = target_database->AddAssetInternal(shader, ECS_ASSET_SHADER);
			}
		}
		output_material->vertex_shader_handle = vertex_handle;

		unsigned int pixel_handle = material->pixel_shader_handle;
		if (pixel_handle != -1) {
			const ShaderMetadata* shader = original_database->GetShaderConst(pixel_handle);
			Stream<char> name = shader->name;
			Stream<wchar_t> file = shader->file;
			pixel_handle = target_database->FindAsset(name, file, ECS_ASSET_SHADER);
			if (force_addition && pixel_handle == -1) {
				pixel_handle = target_database->AddAssetInternal(shader, ECS_ASSET_SHADER);
			}
		}
		output_material->pixel_shader_handle = pixel_handle;

		auto loop = [=](auto is_handles_only) {
			for (unsigned int index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
				for (unsigned int subindex = 0; subindex < counts[index]; subindex++) {
					output_material->textures[index][subindex].slot = material->textures[index][subindex].slot;
					
					unsigned int current_handle = material->textures[index][subindex].metadata_handle;
					if (current_handle != -1) {
						const TextureMetadata* texture = original_database->GetTextureConst(current_handle);
						Stream<char> name = texture->name;
						Stream<wchar_t> file = texture->file;
						current_handle = target_database->FindAsset(name, file, ECS_ASSET_TEXTURE);
						if (force_addition && current_handle == -1) {
							current_handle = target_database->AddAssetInternal(texture, ECS_ASSET_TEXTURE);
						}
					}
					output_material->textures[index][subindex].metadata_handle = current_handle;

					Stream<char> name = material->textures[index][subindex].name;
					if constexpr (!is_handles_only) {
						name = StringCopy(allocator, name);
					}
					output_material->textures[index][subindex].name = name;
				}

				for (unsigned int subindex = 0; subindex < counts[index + ECS_MATERIAL_SHADER_COUNT]; subindex++) {
					output_material->samplers[index][subindex].slot = material->samplers[index][subindex].slot;

					unsigned int current_handle = material->samplers[index][subindex].metadata_handle;
					if (current_handle != -1) {
						const GPUSamplerMetadata* sampler = original_database->GetGPUSamplerConst(current_handle);
						Stream<char> name = sampler->name;
						current_handle = target_database->FindAsset(name, {}, ECS_ASSET_GPU_SAMPLER);
						if (force_addition && current_handle == -1) {
							current_handle = target_database->AddAssetInternal(sampler, ECS_ASSET_GPU_SAMPLER);
						}
					}
					output_material->samplers[index][subindex].metadata_handle = current_handle;

					Stream<char> name = material->samplers[index][subindex].name;
					if constexpr (!is_handles_only) {
						name = StringCopy(allocator, name);
					}
					output_material->samplers[index][subindex].name = name;
				}

				memcpy(output_material->buffers[index].buffer, material->buffers[index].buffer, sizeof(*material->buffers[index].buffer) * material->buffers[index].size);
				if constexpr (!is_handles_only) {
					for (unsigned int subindex = 0; subindex < counts[index + ECS_MATERIAL_SHADER_COUNT * 2]; subindex++) {
						output_material->buffers[index][subindex].name = StringCopy(allocator, material->buffers[index][subindex].name);
						output_material->buffers[index][subindex].data = Copy(allocator, material->buffers[index][subindex].data);
					}
				}
			}
		};

		if (options.handles_only) {
			loop(std::true_type{});
		}
		else {
			loop(std::false_type{});
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void ConvertAssetFromDatabaseToDatabase(
		const void* source_asset, 
		void* target_asset, 
		const AssetDatabase* original_database, 
		AssetDatabase* target_database, 
		AllocatorPolymorphic allocator, 
		ECS_ASSET_TYPE asset_type, 
		ConvertAssetFromDatabaseToDatabaseOptions options
	)
	{
		switch (asset_type) {
		case ECS_ASSET_MESH:
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		case ECS_ASSET_MISC:
			if (options.do_not_copy_with_allocator) {
				size_t asset_size = AssetMetadataByteSize(asset_type);
				memcpy(target_asset, source_asset, asset_size);
			}
			else {
				CopyAssetBase(target_asset, source_asset, asset_type, allocator);
			}
			break;
		case ECS_ASSET_MATERIAL:
			ConvertMaterialFromDatabaseToDatabase((const MaterialAsset*)source_asset, (MaterialAsset*)target_asset, original_database, target_database, allocator, options);
			break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool ChangeMaterialBuffersFromReflectedParameters(
		MaterialAsset* material, 
		const ReflectedShader* reflected_shader, 
		ECS_MATERIAL_SHADER shader, 
		AllocatorPolymorphic allocator
	)
	{
		Stream<Reflection::ReflectionType> cbuffers = *reflected_shader->buffer_options.constant_buffer_reflection;

		bool are_different = false;
		if (cbuffers.size != material->buffers[shader].size) {
			are_different = true;
		}
		else {
			for (size_t index = 0; index < cbuffers.size && !are_different; index++) {
				size_t existing_index = material->FindBuffer(cbuffers[index].name, shader);
				if (existing_index == -1) {
					are_different = true;
				}
				else {
					if (reflected_shader->buffers->buffer[index].register_index != material->buffers[shader][existing_index].slot) {
						are_different = true;
					}
					else {
						// Compare the byte size
						if (Reflection::GetReflectionTypeByteSize(cbuffers.buffer + index) != material->buffers[shader][existing_index].data.size) {
							are_different = true;
						}
						else {
							// Compare the reflection type now
							if (material->buffers[shader][existing_index].reflection_type == nullptr) {
								are_different = true;
							}
							else {
								// At the moment they cannot have nested structures - use the same reflection manager
								if (!Reflection::CompareReflectionTypes(
									material->reflection_manager,
									material->reflection_manager,
									cbuffers.buffer + index,
									material->buffers[shader][existing_index].reflection_type
								)) {
									are_different = true;
								}
								else {
									if (!Reflection::CompareReflectionTypesTags(
										cbuffers.buffer + index,
										material->buffers[shader][existing_index].reflection_type,
										true,
										true
									)) {
										are_different = true;
									}
								}
							}
						}
					}
				}
			}
		}

		if (are_different) {
			MaterialAsset temporary_material;
			memcpy(&temporary_material, material, sizeof(temporary_material));
			ECS_STACK_CAPACITY_STREAM(MaterialAssetBuffer, previous_material_buffers, 512);
			previous_material_buffers.CopyOther(material->buffers[shader]);

			material->ResizeBufferNewValue(cbuffers.size, shader, allocator);
			material->reflection_manager->ClearTypesFromAllocator();
			temporary_material.buffers[shader] = previous_material_buffers;

			for (unsigned int index = 0; index < reflected_shader->buffers->size; index++) {
				Stream<char> current_name = cbuffers[index].name;
				size_t type_byte_size = Reflection::GetReflectionTypeByteSize(cbuffers.buffer + index);
				material->buffers[shader][index].data.buffer = Allocate(allocator, type_byte_size);
				material->buffers[shader][index].data.size = type_byte_size;
				material->buffers[shader][index].slot = reflected_shader->buffers->buffer[index].register_index;
				material->buffers[shader][index].dynamic = true;
				material->buffers[shader][index].name = StringCopy(allocator, current_name);
				// Allocate the pointer again
				material->buffers[shader][index].reflection_type = (Reflection::ReflectionType*)Allocate(allocator, sizeof(Reflection::ReflectionType));
				*material->buffers[shader][index].reflection_type = cbuffers[index].Copy(allocator);

				material->reflection_manager->AddType(material->buffers[shader][index].reflection_type);

				// Search to see if it exists before
				size_t existing_index = temporary_material.FindBuffer(current_name, shader);
				if (existing_index != -1) {
					material->RetrieveBufferData(
						index,
						shader,
						temporary_material.buffers[shader][existing_index].reflection_type,
						temporary_material.buffers[shader][existing_index].data.buffer,
						allocator
					);
				}
				else {
					material->reflection_manager->SetInstanceDefaultData(cbuffers.buffer + index, material->buffers[shader][index].data.buffer);
				}
			}

			// Now deallocate the previous buffer
			temporary_material.DeallocateBuffers(allocator, true, shader);
		}

		return are_different;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool ChangeMaterialTexturesFromReflectedParameters(
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material, 
		const ReflectedShader* reflected_shader, 
		ECS_MATERIAL_SHADER shader, 
		AllocatorPolymorphic allocator
	)
	{
		Stream<ShaderReflectedTexture> textures = *reflected_shader->textures;

		bool are_different = false;
		if (textures.size != material->textures[shader].size) {
			are_different = true;
		}
		else {
			for (size_t index = 0; index < textures.size && !are_different; index++) {
				size_t existing_index = material->FindTexture(textures[index].name, shader);
				if (existing_index == -1) {
					are_different = true;
				}
				else {
					if (textures[index].register_index != material->textures[shader][existing_index].slot) {
						are_different = true;
					}
				}
			}
		}

		if (are_different) {
			MaterialAsset temporary_material;
			memcpy(&temporary_material, material, sizeof(temporary_material));
			material->ResizeTexturesNewValue(textures.size, shader, allocator);

			for (size_t index = 0; index < textures.size; index++) {
				material->textures[shader][index].metadata_handle = -1;
				material->textures[shader][index].slot = textures[index].register_index;
				material->textures[shader][index].name = StringCopy(allocator, textures[index].name);

				size_t existing_index = temporary_material.FindTexture(textures[index].name, shader);
				if (existing_index != -1) {
					material->textures[shader][index].metadata_handle = temporary_material.textures[shader][existing_index].metadata_handle;
				}
			}

			// Determine which previous texture are to be removed
			for (size_t index = 0; index < temporary_material.textures[shader].size; index++) {
				if (temporary_material.textures[shader][index].metadata_handle != -1) {
					bool exists = material->FindTexture(temporary_material.textures[shader][index].name, shader) != -1;
					if (!exists) {
						if (change_dependencies->handles != nullptr) {
							change_dependencies->handles->AddAssert({ temporary_material.textures[shader][index].metadata_handle, ECS_ASSET_TEXTURE });
						}
						else {
							change_dependencies->database->RemoveAsset(temporary_material.textures[shader][index].metadata_handle, ECS_ASSET_TEXTURE);
						}
					}
				}
			}

			// Now deallocate the previous data
			temporary_material.DeallocateTextures(allocator, shader);
		}

		return are_different;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool ChangeMaterialSamplersFromReflectedParameters(
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material, 
		const ReflectedShader* reflected_shader, 
		ECS_MATERIAL_SHADER shader, 
		AllocatorPolymorphic allocator
	)
	{
		Stream<ShaderReflectedSampler> samplers = *reflected_shader->samplers;

		bool are_different = false;
		if (samplers.size != material->samplers[shader].size) {
			are_different = true;
		}
		else {
			for (size_t index = 0; index < samplers.size && !are_different; index++) {
				size_t existing_index = material->FindSampler(samplers[index].name, shader);
				if (existing_index == -1) {
					are_different = true;
				}
				else {
					if (samplers[index].register_index != material->samplers[shader][existing_index].slot) {
						are_different = true;
					}
				}
			}
		}

		if (are_different) {
			MaterialAsset temporary_material;
			memcpy(&temporary_material, material, sizeof(temporary_material));
			material->ResizeSamplersNewValue(samplers.size, shader, allocator);

			for (size_t index = 0; index < samplers.size; index++) {
				material->samplers[shader][index].metadata_handle = -1;
				material->samplers[shader][index].slot = samplers[index].register_index;
				material->samplers[shader][index].name = StringCopy(allocator, samplers[index].name);

				size_t existing_index = temporary_material.FindSampler(samplers[index].name, shader);
				if (existing_index != -1) {
					material->samplers[shader][index].metadata_handle = temporary_material.samplers[shader][existing_index].metadata_handle;
				}
			}

			// Determine which previous samplers are to be removed
			for (size_t index = 0; index < temporary_material.samplers[shader].size; index++) {
				if (temporary_material.samplers[shader][index].metadata_handle != -1) {
					bool exists = material->FindSampler(temporary_material.samplers[shader][index].name, shader) != -1;
					if (!exists) {
						if (change_dependencies->handles != nullptr) {
							change_dependencies->handles->AddAssert({ temporary_material.samplers[shader][index].metadata_handle, ECS_ASSET_GPU_SAMPLER });
						}
						else {
							change_dependencies->database->RemoveAsset(temporary_material.samplers[shader][index].metadata_handle, ECS_ASSET_GPU_SAMPLER);
						}
					}
				}
			}

			// Now deallocate the previous data
			temporary_material.DeallocateSamplers(allocator, shader);
		}
		
		return are_different;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool ChangeMaterialFromReflectedShader(
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material, 
		const ReflectedShader* reflected_shader, 
		ECS_MATERIAL_SHADER shader,
		AllocatorPolymorphic allocator
	)
	{
		bool changed = ChangeMaterialBuffersFromReflectedParameters(material, reflected_shader, shader, allocator);
		changed |= ChangeMaterialTexturesFromReflectedParameters(change_dependencies, material, reflected_shader, shader, allocator);
		changed |= ChangeMaterialSamplersFromReflectedParameters(change_dependencies, material, reflected_shader, shader, allocator);
		return changed;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	ECS_RELOAD_ASSET_METADATA_STATUS ReflectMaterialShaderParameters(
		const ResourceManager* resource_manager,
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material, 
		const ShaderMetadata* shader_metadata,
		ECS_MATERIAL_SHADER shader_type,
		AllocatorPolymorphic allocator,
		Stream<wchar_t> mount_point
	)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path_storage, 512);
		Stream<wchar_t> absolute_path = MountPathOnlyRel(shader_metadata->file, mount_point, absolute_path_storage);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);

		Stream<char> source_code = ReadWholeFileText(absolute_path, &stack_allocator);
		ECS_RELOAD_ASSET_METADATA_STATUS status = ECS_RELOAD_ASSET_METADATA_NO_CHANGE;
		if (source_code.size > 0) {
			// Preprocess the file - taking into account the shader macro definitions
			ECS_STACK_CAPACITY_STREAM(Stream<char>, macro_names, 128);
			ECS_ASSERT(macro_names.capacity >= shader_metadata->macros.size);

			for (size_t index = 0; index < shader_metadata->macros.size; index++) {
				macro_names[index] = shader_metadata->macros[index].name;
			}
			macro_names.size = shader_metadata->macros.size;
			source_code = PreprocessCFile(source_code, macro_names);

			ECS_STACK_CAPACITY_STREAM(ShaderReflectedBuffer, reflected_buffers, ECS_SHADER_MAX_CONSTANT_BUFFER_SLOT);
			ECS_STACK_CAPACITY_STREAM(ShaderReflectedSampler, reflected_samplers, ECS_SHADER_MAX_SAMPLER_SLOT);
			ECS_STACK_CAPACITY_STREAM(ShaderReflectedTexture, reflected_textures, ECS_SHADER_MAX_TEXTURE_SLOT);
			ECS_STACK_CAPACITY_STREAM(Reflection::ReflectionType, reflected_constant_buffer_types, ECS_SHADER_MAX_CONSTANT_BUFFER_SLOT);

			// Load the new values
			ReflectedShader reflected_shader;
			reflected_shader.allocator = &stack_allocator;

			reflected_shader.buffers = &reflected_buffers;
			reflected_shader.buffer_options.only_constant_buffers = true;
			reflected_shader.buffer_options.constant_buffer_reflection = &reflected_constant_buffer_types;

			reflected_shader.textures = &reflected_textures;
			reflected_shader.samplers = &reflected_samplers;

			bool success = resource_manager->m_graphics->ReflectShader(source_code, &reflected_shader);
			if (success) {
				bool changed = ChangeMaterialFromReflectedShader(change_dependencies, material, &reflected_shader, shader_type, allocator);
				status = changed ? ECS_RELOAD_ASSET_METADATA_CHANGED : ECS_RELOAD_ASSET_METADATA_NO_CHANGE;
			}
			else {
				status = ECS_RELOAD_ASSET_METADATA_FAILED_READING;
			}
		}

		// We could deallocate the material - but it will result in data loss
		// on subsequent calls - so let it the same
		return status;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	ECS_RELOAD_ASSET_METADATA_STATUS ReloadMaterialShaderParameters(
		const ResourceManager* resource_manager, 
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material, 
		AllocatorPolymorphic allocator, 
		Stream<wchar_t> mount_point
	)
	{
		ECS_RELOAD_ASSET_METADATA_STATUS status = ECS_RELOAD_ASSET_METADATA_NO_CHANGE;

		auto change_status = [&](ECS_RELOAD_ASSET_METADATA_STATUS new_status) {
			if (new_status == ECS_RELOAD_ASSET_METADATA_CHANGED) {
				if (status != ECS_RELOAD_ASSET_METADATA_FAILED_READING) {
					status = ECS_RELOAD_ASSET_METADATA_CHANGED;
				}
			}
			else if (new_status == ECS_RELOAD_ASSET_METADATA_FAILED_READING) {
				status = ECS_RELOAD_ASSET_METADATA_FAILED_READING;
			}
		};

		auto apply_shader = [&](unsigned int shader_handle, ECS_MATERIAL_SHADER shader_type) {
			if (shader_handle != -1) {
				const ShaderMetadata* shader_metadata = change_dependencies->database->GetShaderConst(shader_handle);
				ECS_RELOAD_ASSET_METADATA_STATUS shader_status = ReflectMaterialShaderParameters(
					resource_manager,
					change_dependencies,
					material,
					shader_metadata,
					shader_type,
					allocator,
					mount_point
				);
				change_status(shader_status);
			}
		};

		apply_shader(material->vertex_shader_handle, ECS_MATERIAL_SHADER_VERTEX);
		apply_shader(material->pixel_shader_handle, ECS_MATERIAL_SHADER_PIXEL);

		return status;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	ECS_RELOAD_ASSET_METADATA_STATUS ReloadMaterialMetadataFromFilesParameters(
		const ResourceManager* resource_manager, 
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material, 
		AllocatorPolymorphic allocator,
		Stream<wchar_t> mount_point
	)
	{
		// At the moment just forward the call
		return ReloadMaterialShaderParameters(resource_manager, change_dependencies, material, allocator, mount_point);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	ECS_RELOAD_ASSET_METADATA_STATUS ReloadAssetMetadataFromFilesParameters(
		const ResourceManager* resource_manager, 
		const AssetDatabase* database, 
		void* metadata, 
		ECS_ASSET_TYPE type, 
		AllocatorPolymorphic allocator, 
		ReloadAssetMetadataFromFilesOptions* options
	)
	{
		switch (type) {
		case ECS_ASSET_MESH:
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		case ECS_ASSET_MISC:
			// Nothing to do for these assets
			break;
		case ECS_ASSET_MATERIAL:
		{
			MaterialAsset* material_metadata = (MaterialAsset*)metadata;
			return ReloadMaterialMetadataFromFilesParameters(resource_manager, &options->material_change_dependencies, material_metadata, allocator, options->mount_point);
		}
		break;
		default:
			ECS_ASSERT(false);
		}

		return ECS_RELOAD_ASSET_METADATA_NO_CHANGE;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	template<typename DatabaseType>
	bool ReloadAssetMetadataFromFilesParametersImpl(
		const ResourceManager* resource_manager,
		DatabaseType* database,
		ReloadAssetMetadataFromFilesOptions* options
	) {
		bool success = true;

		AllocatorPolymorphic allocator;
		AssetDatabase* database_ptr;
		Stream<wchar_t> mount_point = options->mount_point;
		CapacityStream<AssetTypedHandle>* modified_assets = options->modified_assets;

		if constexpr (std::is_same_v<DatabaseType, AssetDatabase>) {
			allocator = database->Allocator();
			database_ptr = database;
		}
		else {
			allocator = database->database->Allocator();
			database_ptr = database->database;
		}
		options->material_change_dependencies.database = database_ptr;

		ForEach(ECS_ASSET_TYPES_METADATA_WITH_DEPENDENCY, [&](AssetTypePair type_pair)
			{
				ECS_ASSET_TYPE asset_type = type_pair.main_type;
				database->ForEachAsset(asset_type, [&](unsigned int handle) {
					void* current_metadata = database->GetAsset(handle, asset_type);
					ECS_RELOAD_ASSET_METADATA_STATUS status = ReloadAssetMetadataFromFilesParameters(
						resource_manager,
						database_ptr,
						current_metadata,
						asset_type,
						allocator,
						options
					);
					if (status == ECS_RELOAD_ASSET_METADATA_CHANGED) {
						if (modified_assets != nullptr) {
							modified_assets->AddAssert({ handle, asset_type });
						}
					}
					else if (status == ECS_RELOAD_ASSET_METADATA_FAILED_READING) {
						success = false;
					}
				});
			}
		);
		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool ReloadAssetMetadataFromFilesParameters(
		const ResourceManager* resource_manager, 
		AssetDatabase* database, 
		ReloadAssetMetadataFromFilesOptions* options
	)
	{
		return ReloadAssetMetadataFromFilesParametersImpl(resource_manager, database, options);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool ReloadAssetMetadataFromFilesParameters(
		const ResourceManager* resource_manager, 
		AssetDatabaseReference* database_reference, 
		ReloadAssetMetadataFromFilesOptions* options
	)
	{
		return ReloadAssetMetadataFromFilesParametersImpl(resource_manager, database_reference, options);
	}

	// -------------------------------------------------------------------------------------------------------------------------

	bool CreateMaterialAssetFromPBRMaterial(AssetDatabase* database, MaterialAsset* material_asset, const PBRMaterial* pbr_material, AllocatorPolymorphic allocator)
	{
		// Determine which macros need to set for the pixel shader
		// And set the textures accordingly in the material asset
		ShaderMetadata* pixel_metadata = database->GetShader(material_asset->pixel_shader_handle);
		pixel_metadata->DeallocateMacros(allocator);

		// Deallocate the textures such that we can add them in the ifs
		material_asset->DeallocateTextures(allocator, ECS_MATERIAL_SHADER_PIXEL, false);
		// The deallocate doesn't change the size
		material_asset->textures[ECS_MATERIAL_SHADER_PIXEL].size = 0;

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);

		bool success = true;
		auto add_texture = [&](Stream<wchar_t> texture, Stream<char> macro_string, Stream<char> display_name, unsigned char slot) {
			if (texture.size > 0 && success) {
				// Get the default setting for the texture, or any one of them if there is no "Default" setting
				Stream<Stream<char>> settings = database->GetMetadatasForFile(texture, ECS_ASSET_TEXTURE, &stack_allocator);
				if (settings.size > 0) {
					pixel_metadata->AddMacro(macro_string, "", allocator);
					unsigned int default_texture_index = FindString("Default", settings);
					Stream<char> setting_name = default_texture_index == -1 ? settings[0] : "Default";
					unsigned int handle = database->AddTexture(setting_name, texture);
					if (handle != -1) {
						material_asset->AddTexture(ECS_MATERIAL_SHADER_PIXEL, display_name, handle, slot, allocator);
					}
					else {
						success = false;
					}
				}
				stack_allocator.Clear();
			}
		};

		add_texture(pbr_material->color_texture, STRING(COLOR_TEXTURE), "ColorMap", 3);
		add_texture(pbr_material->normal_texture, STRING(NORMAL_TEXTURE), "NormalMap", 4);
		add_texture(pbr_material->metallic_texture, STRING(METALLIC_TEXTURE), "MetallicMap", 5);
		add_texture(pbr_material->roughness_texture, STRING(ROUGHNESS_TEXTURE), "RoughnessMap", 6);
		add_texture(pbr_material->occlusion_texture, STRING(OCCLUSION_TEXTURE), "AmbientOcclusionMap", 7);

		if (!success) {
			return false;
		}

		// Set the factors for the constant buffers
		size_t modifiers_cb_index = material_asset->FindBuffer(STRING(Modifiers), ECS_MATERIAL_SHADER_PIXEL);
		if (modifiers_cb_index == -1) {
			return false;
		}

		MaterialAssetBuffer* modifiers_cb = &material_asset->buffers[ECS_MATERIAL_SHADER_PIXEL][modifiers_cb_index];
		Reflection::CopyIntoReflectionFieldValue(modifiers_cb->reflection_type, "tint", modifiers_cb->data.buffer, &pbr_material->tint);
		Reflection::CopyIntoReflectionFieldValue(modifiers_cb->reflection_type, "metallic_factor", modifiers_cb->data.buffer, &pbr_material->metallic_factor);
		Reflection::CopyIntoReflectionFieldValue(modifiers_cb->reflection_type, "roughness_factor", modifiers_cb->data.buffer, &pbr_material->roughness_factor);
		Reflection::CopyIntoReflectionFieldValue(modifiers_cb->reflection_type, "emissive_factor", modifiers_cb->data.buffer, &pbr_material->emissive_factor);

		// Now write the metadata files
		if (database->WriteShaderFile(pixel_metadata)) {
			if (database->WriteMaterialFile(material_asset)) {
				return true;
			}
		}

		return false;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	// Helper function that iterates over the asset database in order to apply a protection/unprotection to resource manager resources.
	// The ActionFunctor receives as parameters (const void* asset_pointer, ResourceType resource_type).
	template<typename ActionFunctor>
	static void ProtectUnprotectAssetDatabaseResources(const AssetDatabase* database, ActionFunctor&& action_functor) {
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)index;
			ResourceType resource_type = AssetTypeToResourceType(asset_type);
			if (resource_type != ResourceType::TypeCount) {
				database->ForEachAsset(asset_type, [&](unsigned int handle) {
					const void* asset_pointer = GetAssetFromMetadata(database->GetAssetConst(handle, asset_type), asset_type).buffer;
					action_functor(asset_pointer, resource_type);
				});
			}
		}
		// NOTE: Misc assets are looked upon their buffer pointer, instead of the pointer to the buffer structure inside
		// The resource manager, which means they can never succeed. But don't create a special case for it, it is niche anyway.
	}

	void ProtectAssetDatabaseResources(const AssetDatabase* database, ResourceManager* resource_manager) {
		ProtectUnprotectAssetDatabaseResources(database, [resource_manager](const void* asset_pointer, ResourceType resource_type) {
			resource_manager->ProtectResource(asset_pointer, resource_type, false);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void UnprotectAssetDatabaseResources(const AssetDatabase* database, ResourceManager* resource_manager) {
		ProtectUnprotectAssetDatabaseResources(database, [resource_manager](const void* asset_pointer, ResourceType resource_type) {
			resource_manager->UnprotectResource(asset_pointer, resource_type, false);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------

	void AssetTypeLockGPU(ECS_ASSET_TYPE type, SpinLock* gpu_lock) {
		// Only textures and materials need this lock
		switch (type) {
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_MATERIAL:
			gpu_lock->Lock();
			break;
		case ECS_ASSET_MESH:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		case ECS_ASSET_MISC:
			break;
		}
	}

	void AssetTypeUnlockGPU(ECS_ASSET_TYPE type, SpinLock* gpu_lock) {
		// Only textures and materials need this lock
		switch (type) {
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_MATERIAL:
			gpu_lock->Unlock();
			break;
		case ECS_ASSET_MESH:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		case ECS_ASSET_MISC:
			break;
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------

}