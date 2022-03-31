#include "ecspch.h"
#include "AssetDatabase.h"
#include "../../Utilities/File.h"
#include "../../Utilities/FunctionInterfaces.h"

#define SERIALIZE_VERSION 1

namespace ECSEngine {

	// --------------------------------------------------------------------------------------

	AssetDatabase::AssetDatabase(AllocatorPolymorphic allocator) : mesh_asset(allocator, 0), texture_asset(allocator, 0), gpu_buffer_asset(allocator, 0),
	gpu_sampler_asset(allocator, 0), shader_asset(allocator, 0), material_asset(allocator, 0) {}
	
	// --------------------------------------------------------------------------------------

	void AssetDatabase::AddMesh(MeshMetadata metadata, Stream<wchar_t> path, Stream<char> name)
	{
		// The path and the name must be allocated
		function::CoallesceStreams(mesh_asset.allocator, name, path);
		mesh_asset.Add({ metadata, path, name });
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::AddTexture(TextureMetadata metadata, Stream<wchar_t> path, Stream<char> name)
	{
		// The path and the name must be allocated
		function::CoallesceStreams(texture_asset.allocator, name, path);
		texture_asset.Add({ metadata, path, name });
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::AddGPUBuffer(GPUBufferMetadata metadata, Stream<wchar_t> path, Stream<char> name)
	{
		// The path and the name must be allocated
		function::CoallesceStreams(gpu_buffer_asset.allocator, name, path);
		gpu_buffer_asset.Add({ metadata, path, name });
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::AddGPUSampler(GPUSamplerMetadata metadata, Stream<wchar_t> path, Stream<char> name)
	{
		// The path and the name must be allocated
		function::CoallesceStreams(gpu_sampler_asset.allocator, name, path);
		gpu_sampler_asset.Add({ metadata, path, name });
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::AddShader(ShaderMetadata metadata, Stream<wchar_t> path, Stream<char> name)
	{
		// The path and the name must be allocated
		function::CoallesceStreams(shader_asset.allocator, name, path);

		// Each string gets a separate allocation - easier to modify
		for (unsigned int index = 0; index < metadata.macros.size; index++) {
			metadata.macros[index].name = function::StringCopy(shader_asset.allocator, metadata.macros[index].name).buffer;
			metadata.macros[index].definition = function::StringCopy(shader_asset.allocator, metadata.macros[index].definition).buffer;
		}

		shader_asset.Add({ metadata, path, name });
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::AddMaterial(MaterialAsset material, Stream<char> name)
	{
		// The streams must be copied here
		size_t allocation_size = name.size * sizeof(char);
		allocation_size += material.CopySize();

		void* allocation = Allocate(material_asset.allocator, allocation_size);
		name.CopyTo(allocation);
		name.buffer = (char*)allocation;

		allocation = function::OffsetPointer(allocation, sizeof(char) * name.size);
		material = material.Copy(allocation);
		material_asset.Add({ material, name });
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::AddMisc(Stream<wchar_t> path)
	{
		misc_asset.Add(function::StringCopy(misc_asset.allocator, path));
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMesh(Stream<char> name) const
	{
		for (unsigned int index = 0; index < mesh_asset.size; index++) {
			if (function::CompareStrings(mesh_asset[index].name, name)) {
				return index;
			}
		}

		return -1;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindTexture(Stream<char> name) const
	{
		for (unsigned int index = 0; index < texture_asset.size; index++) {
			if (function::CompareStrings(texture_asset[index].name, name)) {
				return index;
			}
		}

		return -1;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindGPUBuffer(Stream<char> name) const
	{
		for (unsigned int index = 0; index < gpu_buffer_asset.size; index++) {
			if (function::CompareStrings(gpu_buffer_asset[index].name, name)) {
				return index;
			}
		}

		return -1;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindGPUSampler(Stream<char> name) const
	{
		for (unsigned int index = 0; index < gpu_sampler_asset.size; index++) {
			if (function::CompareStrings(gpu_sampler_asset[index].name, name)) {
				return index;
			}
		}

		return -1;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindShader(Stream<char> name) const
	{
		for (unsigned int index = 0; index < shader_asset.size; index++) {
			if (function::CompareStrings(shader_asset[index].name, name)) {
				return index;
			}
		}

		return -1;
	}

	// --------------------------------------------------------------------------------------

	unsigned int AssetDatabase::FindMaterial(Stream<char> name) const
	{
		for (unsigned int index = 0; index < material_asset.size; index++) {
			if (function::CompareStrings(material_asset[index].name, name)) {
				return index;
			}
		}

		return -1;
	}

	unsigned int AssetDatabase::FindMisc(Stream<wchar_t> path) const
	{
		Stream<Stream<wchar_t>> misc_paths(misc_asset.buffer, misc_asset.size);
		return function::FindString(path, misc_paths);
	}

	// --------------------------------------------------------------------------------------

	ShaderMetadata* AssetDatabase::GetShader(Stream<char> name)
	{
		unsigned int index = FindShader(name);
		return index == -1 ? nullptr : &shader_asset[index].metadata;
	}

	// --------------------------------------------------------------------------------------

	AllocatorPolymorphic AssetDatabase::Allocator() const
	{
		return mesh_asset.allocator;
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMesh(Stream<char> name)
	{
		unsigned int index = FindMesh(name);
		if (index == -1) {
			return false;
		}

		RemoveMeshIndex(index);
		return true;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RemoveMeshIndex(unsigned int index)
	{
		// Deallocate the memory
		Deallocate(mesh_asset.allocator, mesh_asset[index].name.buffer);
		mesh_asset.RemoveSwapBack(index);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveTexture(Stream<char> name)
	{
		unsigned int index = FindTexture(name);
		if (index == -1) {
			return false;
		}

		RemoveTextureIndex(index);
		return true;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RemoveTextureIndex(unsigned int index)
	{
		// Deallocate the memory
		Deallocate(texture_asset.allocator, texture_asset[index].name.buffer);

		texture_asset.RemoveSwapBack(index);
		// Fix any material reference to the last element
		if (index != texture_asset.size) {
			unsigned int replaced_index = index;
			for (index = 0; index < material_asset.size; index++) {
				for (unsigned int subindex = 0; subindex < material_asset[index].asset.textures.size; subindex++) {
					// Can exit from this inner loop. But some other materials might reference this exact texture
					// Can't exit from the outer loop
					if (material_asset[index].asset.textures[subindex].metadata_index == texture_asset.size) {
						material_asset[index].asset.textures[subindex].metadata_index = replaced_index;
						break;
					}
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveGPUBuffer(Stream<char> name)
	{
		unsigned int index = FindGPUBuffer(name);
		if (index == -1) {
			return false;
		}

		RemoveGPUBUfferIndex(index);
		return true;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RemoveGPUBUfferIndex(unsigned int index)
	{
		// Deallocate the memory
		Deallocate(gpu_buffer_asset.allocator, gpu_buffer_asset[index].name.buffer);

		gpu_buffer_asset.RemoveSwapBack(index);
		// Fix any material reference to the last element
		if (index != gpu_buffer_asset.size) {
			unsigned int replaced_index = index;
			for (index = 0; index < material_asset.size; index++) {
				for (unsigned int subindex = 0; subindex < material_asset[index].asset.buffers.size; subindex++) {
					// Can exit from this inner loop. But some other materials might reference this exact texture
					// Can't exit from the outer loop
					if (material_asset[index].asset.buffers[subindex].metadata_index == gpu_buffer_asset.size) {
						material_asset[index].asset.buffers[subindex].metadata_index = replaced_index;
						break;
					}
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveGPUSampler(Stream<char> name)
	{
		unsigned int index = FindGPUSampler(name);
		if (index == -1) {
			return false;
		}

		RemoveGPUSamplerIndex(index);
		return true;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RemoveGPUSamplerIndex(unsigned int index)
	{
		// Deallocate the memory
		Deallocate(gpu_sampler_asset.allocator, gpu_sampler_asset[index].name.buffer);

		gpu_sampler_asset.RemoveSwapBack(index);
		// Fix any material reference to the last element
		if (index != gpu_sampler_asset.size) {
			unsigned int replaced_index = index;
			for (index = 0; index < material_asset.size; index++) {
				for (unsigned int subindex = 0; subindex < material_asset[index].asset.samplers.size; subindex++) {
					// Can exit from this inner loop. But some other materials might reference this exact texture
					// Can't exit from the outer loop
					if (material_asset[index].asset.samplers[subindex].metadata_index == gpu_sampler_asset.size) {
						material_asset[index].asset.samplers[subindex].metadata_index = replaced_index;
						break;
					}
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveShader(Stream<char> name)
	{
		unsigned int index = FindShader(name);
		if (index == -1) {
			return false;
		}

		RemoveShaderIndex(index);
		return true;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RemoveShaderIndex(unsigned int index)
	{
		// Deallocate the memory
		Deallocate(shader_asset.allocator, shader_asset[index].name.buffer);

		shader_asset.RemoveSwapBack(index);
		// Fix any material reference to the last element
		if (index != shader_asset.size) {
			unsigned int replaced_index = index;
			for (index = 0; index < material_asset.size; index++) {
				for (unsigned int subindex = 0; subindex < ECS_SHADER_TYPE_COUNT - 1; subindex++) {
					// Can exit from this inner loop. But some other materials might reference this exact texture
					// Can't exit from the outer loop
					if (material_asset[index].asset.shaders[subindex] == shader_asset.size) {
						material_asset[index].asset.shaders[subindex] = replaced_index;
						break;
					}
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMaterial(Stream<char> name)
	{
		unsigned int index = FindMaterial(name);
		if (index == -1) {
			return false;
		}

		RemoveMaterialIndex(index);
		return true;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RemoveMaterialIndex(unsigned int index)
	{
		// Deallocate the memory
		Deallocate(material_asset.allocator, material_asset[index].name.buffer);

		material_asset.RemoveSwapBack(index);
	}

	// --------------------------------------------------------------------------------------

	bool AssetDatabase::RemoveMisc(Stream<wchar_t> path)
	{
		unsigned int index = FindMisc(path);
		if (index == -1) {
			return false;
		}

		RemoveMiscIndex(index);
		return true;
	}

	// --------------------------------------------------------------------------------------

	void AssetDatabase::RemoveMiscIndex(unsigned int index)
	{
		Deallocate(misc_asset.allocator, misc_asset[index].buffer);
		misc_asset.RemoveSwapBack(index);
	}

	// --------------------------------------------------------------------------------------

	bool SerializeAssetDatabase(const AssetDatabase* database, Stream<wchar_t> file)
	{
		const size_t MAX_STACK_BUFFER_SIZE = ECS_KB * 256;

		ECS_FILE_HANDLE file_handle = 0;
		ECS_FILE_STATUS_FLAGS status = FileCreate(file, &file_handle, ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE);
		if (status != ECS_FILE_STATUS_OK) {
			return false;
		}

		size_t buffer_size = SerializeAssetDatabaseSize(database);
		void* buffer = nullptr;
		if (buffer_size < MAX_STACK_BUFFER_SIZE) {
			buffer = ECS_STACK_ALLOC(buffer_size);
		}
		else {
			buffer = malloc(buffer_size);
		}

		uintptr_t ptr = (uintptr_t)buffer;
		SerializeAssetDatabase(database, ptr);
		bool success = WriteFile(file_handle, { buffer, buffer_size });

		if (buffer_size >= MAX_STACK_BUFFER_SIZE) {
			free(buffer);
		}
		CloseFile(file_handle);
		return success;
	}

	// --------------------------------------------------------------------------------------

	void SerializeAssetDatabase(const AssetDatabase* database, uintptr_t& buffer)
	{
		unsigned char* version = (unsigned char*)buffer;
		*version = SERIALIZE_VERSION;

		buffer += sizeof(unsigned char);

		// Write the sizes of the database at the beginning followed by the sizes of the paths
		unsigned int* stream_sizes = (unsigned int*)buffer;
		stream_sizes[ECS_ASSET_MESH] = database->mesh_asset.size;
		stream_sizes[ECS_ASSET_TEXTURE] = database->texture_asset.size;
		stream_sizes[ECS_ASSET_GPU_BUFFER] = database->gpu_buffer_asset.size;
		stream_sizes[ECS_ASSET_GPU_SAMPLER] = database->gpu_sampler_asset.size;
		stream_sizes[ECS_ASSET_SHADER] = database->shader_asset.size;
		stream_sizes[ECS_ASSET_MATERIAL] = database->material_asset.size;
		stream_sizes[ECS_ASSET_MISC] = database->misc_asset.size;

		buffer += sizeof(unsigned int) * ECS_ASSET_TYPE_COUNT;

#pragma region Names sizes

		// The sizes of the paths can be written as unsigned shorts
		unsigned short* name_sizes = (unsigned short*)buffer;
		for (unsigned int index = 0; index < database->mesh_asset.size; index++) {
			*name_sizes = database->mesh_asset[index].name.size;
			name_sizes++;
			buffer += sizeof(unsigned short);
		}

		for (unsigned int index = 0; index < database->texture_asset.size; index++) {
			*name_sizes = database->texture_asset[index].name.size;
			name_sizes++;
			buffer += sizeof(unsigned short);
		}

		for (unsigned int index = 0; index < database->gpu_buffer_asset.size; index++) {
			*name_sizes = database->gpu_buffer_asset[index].name.size;
			name_sizes++;
			buffer += sizeof(unsigned short);
		}

		for (unsigned int index = 0; index < database->gpu_sampler_asset.size; index++) {
			*name_sizes = database->gpu_sampler_asset[index].name.size;
			name_sizes++;
			buffer += sizeof(unsigned short);
		}

		for (unsigned int index = 0; index < database->shader_asset.size; index++) {
			*name_sizes = database->shader_asset[index].name.size;
			name_sizes++;
			buffer += sizeof(unsigned short);
		}

		for (unsigned int index = 0; index < database->material_asset.size; index++) {
			*name_sizes = database->material_asset[index].name.size;
			name_sizes++;
			buffer += sizeof(unsigned short);
		}

#pragma endregion

#pragma region Path sizes

		// The sizes of the paths can be written as unsigned shorts
		unsigned short* path_sizes = (unsigned short*)buffer;
		for (unsigned int index = 0; index < database->mesh_asset.size; index++) {
			*path_sizes = database->mesh_asset[index].path.size;
			path_sizes++;
			buffer += sizeof(unsigned short);
		}

		for (unsigned int index = 0; index < database->texture_asset.size; index++) {
			*path_sizes = database->texture_asset[index].path.size;
			path_sizes++;
			buffer += sizeof(unsigned short);
		}

		for (unsigned int index = 0; index < database->gpu_buffer_asset.size; index++) {
			*path_sizes = database->gpu_buffer_asset[index].path.size;
			path_sizes++;
			buffer += sizeof(unsigned short);
		}

		for (unsigned int index = 0; index < database->gpu_sampler_asset.size; index++) {
			*path_sizes = database->gpu_sampler_asset[index].path.size;
			path_sizes++;
			buffer += sizeof(unsigned short);
		}

		for (unsigned int index = 0; index < database->shader_asset.size; index++) {
			*path_sizes = database->shader_asset[index].path.size;
			path_sizes++;
			buffer += sizeof(unsigned short);
		}

		for (unsigned int index = 0; index < database->misc_asset.size; index++) {
			*path_sizes = database->misc_asset[index].size;
			path_sizes++;
			buffer += sizeof(unsigned short);
		}

#pragma endregion

#pragma region Metadatas and assets

		// Now write the metadatas
		for (unsigned int index = 0; index < database->mesh_asset.size; index++) {
			SerializeMeshMetadata(database->mesh_asset[index].metadata, buffer);
		}

		for (unsigned int index = 0; index < database->texture_asset.size; index++) {
			SerializeTextureMetadata(database->texture_asset[index].metadata, buffer);
		}

		for (unsigned int index = 0; index < database->gpu_buffer_asset.size; index++) {
			SerializeGPUBufferMetadata(database->gpu_buffer_asset[index].metadata, buffer);
		}

		for (unsigned int index = 0; index < database->gpu_sampler_asset.size; index++) {
			SerializeGPUSamplerMetadata(database->gpu_sampler_asset[index].metadata, buffer);
		}

		for (unsigned int index = 0; index < database->shader_asset.size; index++) {
			SerializeShaderMetadata(database->shader_asset[index].metadata, buffer);
		}

		for (unsigned int index = 0; index < database->material_asset.size; index++) {
			SerializeMaterialAsset(database->material_asset[index].asset, buffer);
		}

#pragma endregion

#pragma region Name and path strings

		// Now the name strings
		for (unsigned int index = 0; index < database->mesh_asset.size; index++) {
			database->mesh_asset[index].name.CopyTo(buffer);
			database->mesh_asset[index].path.CopyTo(buffer);
		}

		for (unsigned int index = 0; index < database->texture_asset.size; index++) {
			database->texture_asset[index].name.CopyTo(buffer);
			database->texture_asset[index].path.CopyTo(buffer);
		}

		for (unsigned int index = 0; index < database->gpu_buffer_asset.size; index++) {
			database->gpu_buffer_asset[index].name.CopyTo(buffer);
			database->gpu_buffer_asset[index].path.CopyTo(buffer);
		}

		for (unsigned int index = 0; index < database->gpu_sampler_asset.size; index++) {
			database->gpu_sampler_asset[index].name.CopyTo(buffer);
			database->gpu_sampler_asset[index].path.CopyTo(buffer);
		}

		for (unsigned int index = 0; index < database->shader_asset.size; index++) {
			database->shader_asset[index].name.CopyTo(buffer);
			database->shader_asset[index].path.CopyTo(buffer);
		}

		for (unsigned int index = 0; index < database->material_asset.size; index++) {
			database->material_asset[index].name.CopyTo(buffer);
		}

		for (unsigned int index = 0; index < database->misc_asset.size; index++) {
			database->misc_asset[index].CopyTo(buffer);
		}

#pragma endregion

	}

	// --------------------------------------------------------------------------------------

	size_t SerializeAssetDatabaseSize(const AssetDatabase* database)
	{
		// Version + the sizes of the stream
		size_t size = sizeof(unsigned char) + sizeof(unsigned int) * ECS_ASSET_TYPE_COUNT;
		
		size += (sizeof(unsigned short) * 2 + SerializeMeshMetadataSize()) * database->mesh_asset.size;
		for (unsigned int index = 0; index < database->mesh_asset.size; index++) {
			size += database->mesh_asset[index].path.size * sizeof(wchar_t);
			size += database->mesh_asset[index].name.size * sizeof(char);
		}

		size += (sizeof(unsigned short) * 2 + SerializeTextureMetadataSize()) * database->texture_asset.size;
		for (unsigned int index = 0; index < database->texture_asset.size; index++) {
			size += database->texture_asset[index].path.size * sizeof(wchar_t);
			size += database->texture_asset[index].name.size * sizeof(char);
		}

		size += (sizeof(unsigned short) * 2 + SerializeGPUBufferMetadataSize()) * database->gpu_buffer_asset.size;
		for (unsigned int index = 0; index < database->gpu_buffer_asset.size; index++) {
			size += database->gpu_buffer_asset[index].path.size * sizeof(wchar_t);
			size += database->gpu_buffer_asset[index].name.size * sizeof(char);
		}

		size += (sizeof(unsigned short) * 2 + SerializeGPUSamplerMetadataSize()) * database->gpu_sampler_asset.size;
		for (unsigned int index = 0; index < database->gpu_sampler_asset.size; index++) {
			size += database->gpu_sampler_asset[index].path.size * sizeof(wchar_t);
			size += database->gpu_sampler_asset[index].name.size * sizeof(char);
		}

		size += sizeof(unsigned short) * 2 * database->shader_asset.size;
		for (unsigned int index = 0; index < database->shader_asset.size; index++) {
			size += database->shader_asset[index].path.size * sizeof(wchar_t);
			size += database->shader_asset[index].name.size * sizeof(char);
			size += SerializeShaderMetadataSize(database->shader_asset[index].metadata);
		}

		size += sizeof(unsigned short) * database->material_asset.size;
		for (unsigned int index = 0; index < database->material_asset.size; index++) {
			size += database->material_asset[index].name.size * sizeof(char);
			size += SerializeMaterialAssetSize(database->material_asset[index].asset);
		}

		size += sizeof(unsigned short) * database->misc_asset.size;
		for (unsigned int index = 0; index < database->misc_asset.size; index++) {
			size += database->misc_asset[index].size * sizeof(wchar_t);
		}

		return size;
	}

	// --------------------------------------------------------------------------------------

	bool DeserializeAssetDatabase(AssetDatabase* database, Stream<wchar_t> file)
	{
		// Read the whole file into a memory buffer and then commit into the database
		Stream<void> content = ReadWholeFileBinary(file);
		if (content.buffer != nullptr) {
			uintptr_t buffer = (uintptr_t)content.buffer;
			bool success = DeserializeAssetDatabase(database, buffer);
			free(content.buffer);
			return success;
		}

		return false;
	}

	// --------------------------------------------------------------------------------------

	bool DeserializeAssetDatabase(AssetDatabase* database, uintptr_t& buffer)
	{
		unsigned char* version = (unsigned char*)buffer;
		if (*version != SERIALIZE_VERSION) {
			return false;
		}

		buffer += sizeof(unsigned char);
		unsigned int* stream_sizes = (unsigned int*)buffer;
		unsigned int total_stream_size = 0;

		buffer += sizeof(unsigned int) * ECS_ASSET_TYPE_COUNT;

		// For the moment assert that the size is smaller than an unsigned short
		for (unsigned int index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			if (stream_sizes[index] > USHORT_MAX) {
				return -1;
			}
			total_stream_size += stream_sizes[index];
		}

		unsigned short* name_sizes = (unsigned short*)buffer;
		// For the moment assert that the path is smaller than a unsigned char
		for (unsigned int index = 0; index < total_stream_size - stream_sizes[ECS_ASSET_MISC]; index++) {
			if (name_sizes[index] > UCHAR_MAX) {
				return -1;
			}
		}
		// The misc resource don't have a path
		buffer += sizeof(unsigned short) * (total_stream_size - stream_sizes[ECS_ASSET_MISC]);

		unsigned short* path_sizes = (unsigned short*)buffer;
		// For the moment assert that the path size is smaller than a unsigned char
		for (unsigned int index = 0; index < total_stream_size - stream_sizes[ECS_ASSET_MATERIAL]; index++) {
			if (path_sizes[index] > UCHAR_MAX) {
				return -1;
			}
		}
		// The material doesn't have a path
		buffer += sizeof(unsigned short) * (total_stream_size - stream_sizes[ECS_ASSET_MATERIAL]);

		// Now the metadata comes
		// Leave the size of the streams equal to 0 when deserializing the metadata
		// When deserializing the name or the path, we can use the add function in order to 
		// avoid creating a dependency here

		// The meshes
		database->mesh_asset.FreeBuffer();
		database->mesh_asset.ReserveNewElements(stream_sizes[ECS_ASSET_MESH]);
		for (unsigned int index = 0; index < stream_sizes[ECS_ASSET_MESH]; index++) {
			ECS_ASSET_METADATA_CODE code = DeserializeMeshMetadata(&database->mesh_asset[index].metadata, buffer);
			if (code != ECS_ASSET_METADATA_OK) {
				return false;
			}
		}

		// The textures
		database->texture_asset.FreeBuffer();
		database->texture_asset.ReserveNewElements(stream_sizes[ECS_ASSET_TEXTURE]);
		for (unsigned int index = 0; index < stream_sizes[ECS_ASSET_TEXTURE]; index++) {
			ECS_ASSET_METADATA_CODE code = DeserializeTextureMetadata(&database->texture_asset[index].metadata, buffer);
			if (code != ECS_ASSET_METADATA_OK) {
				return false;
			}
		}

		// The gpu buffers
		database->gpu_buffer_asset.FreeBuffer();
		database->gpu_buffer_asset.ReserveNewElements(stream_sizes[ECS_ASSET_GPU_BUFFER]);
		for (unsigned int index = 0; index < stream_sizes[ECS_ASSET_GPU_BUFFER]; index++) {
			ECS_ASSET_METADATA_CODE code = DeserializeGPUBufferMetadata(&database->gpu_buffer_asset[index].metadata, buffer);
			if (code != ECS_ASSET_METADATA_OK) {
				return false;
			}
		}

		// The gpu samplers
		database->gpu_sampler_asset.FreeBuffer();
		database->gpu_sampler_asset.ReserveNewElements(stream_sizes[ECS_ASSET_GPU_SAMPLER]);
		for (unsigned int index = 0; index < stream_sizes[ECS_ASSET_GPU_SAMPLER]; index++) {
			ECS_ASSET_METADATA_CODE code = DeserializeGPUSamplerMetadata(&database->gpu_sampler_asset[index].metadata, buffer);
			if (code != ECS_ASSET_METADATA_OK) {
				return false;
			}
		}

		// The shaders
		database->shader_asset.FreeBuffer();
		database->shader_asset.ReserveNewElements(stream_sizes[ECS_ASSET_SHADER]);
		for (unsigned int index = 0; index < stream_sizes[ECS_ASSET_SHADER]; index++) {
			ECS_ASSET_METADATA_CODE code = DeserializeShaderMetadata(&database->shader_asset[index].metadata, buffer, database->shader_asset.allocator);
			if (code != ECS_ASSET_METADATA_OK) {
				return false;
			}
		}

		// The materials
		database->material_asset.FreeBuffer();
		database->material_asset.ReserveNewElements(stream_sizes[ECS_ASSET_MATERIAL]);
		for (unsigned int index = 0; index < stream_sizes[ECS_ASSET_MATERIAL]; index++) {
			ECS_ASSET_METADATA_CODE code = DeserializeMaterialAsset(&database->material_asset[index].asset, buffer, database->material_asset.allocator);
			if (code != ECS_ASSET_METADATA_OK) {
				return false;
			}
		}

		// The misc resources will be read at the end of the path read
		database->misc_asset.FreeBuffer();
		database->misc_asset.ReserveNewElements(stream_sizes[ECS_ASSET_MATERIAL]);

		// Now the paths and the names need to be read

		// The names
		unsigned int name_path_size_counter = 0;

		auto load_name_path = [&](unsigned int loop_count, auto add_function) {
			for (unsigned int index = 0; index < loop_count; index++) {
				char* name = (char*)buffer;
				wchar_t* path = (wchar_t*)(buffer + name_sizes[name_path_size_counter] * sizeof(char));

				Stream<wchar_t> stream_path = { path, path_sizes[name_path_size_counter] };
				Stream<char> stream_name = { name, name_sizes[name_path_size_counter] };
				add_function(index, stream_path, stream_name);
				buffer += name_sizes[name_path_size_counter] * sizeof(char) + path_sizes[name_path_size_counter] * sizeof(wchar_t);

				name_path_size_counter++;
			}
		};

		load_name_path(stream_sizes[ECS_ASSET_MESH], [database](unsigned int index, Stream<wchar_t> path, Stream<char> name) {
			database->AddMesh(database->mesh_asset[index].metadata, path, name);
		});

		load_name_path(stream_sizes[ECS_ASSET_TEXTURE], [database](unsigned int index, Stream<wchar_t> path, Stream<char> name) {
			database->AddTexture(database->texture_asset[index].metadata, path, name);
		});

		load_name_path(stream_sizes[ECS_ASSET_GPU_BUFFER], [database](unsigned int index, Stream<wchar_t> path, Stream<char> name) {
			database->AddGPUBuffer(database->gpu_buffer_asset[index].metadata, path, name);
		});

		load_name_path(stream_sizes[ECS_ASSET_GPU_SAMPLER], [database](unsigned int index, Stream<wchar_t> path, Stream<char> name) {
			database->AddGPUSampler(database->gpu_sampler_asset[index].metadata, path, name);
		});

		load_name_path(stream_sizes[ECS_ASSET_SHADER], [database](unsigned int index, Stream<wchar_t> path, Stream<char> name) {
			database->AddShader(database->shader_asset[index].metadata, path, name);
		});
		
		unsigned int path_size_counter_before_material = name_path_size_counter;
		// The materials only have a name
		for (unsigned int index = 0; index < stream_sizes[ECS_ASSET_MATERIAL]; index++) {
			char* name = (char*)buffer;

			Stream<char> stream_name = { name, name_sizes[name_path_size_counter] };
			database->AddMaterial(database->material_asset[index].asset, stream_name);
			buffer += name_sizes[name_path_size_counter] * sizeof(char);
			name_path_size_counter++;
		}
		
		// The misc resources only have a path
		for (unsigned int index = 0; index < stream_sizes[ECS_ASSET_MISC]; index++) {
			database->AddMisc({ (wchar_t*)buffer, path_sizes[path_size_counter_before_material] });
			buffer += name_sizes[path_size_counter_before_material] * sizeof(wchar_t);
			path_size_counter_before_material++;
		}

		return true;
	}

	// --------------------------------------------------------------------------------------

	size_t DeserializeAssetDatabaseSize(uintptr_t buffer)
	{
		uintptr_t initial_buffer = buffer;
		unsigned char* version = (unsigned char*)buffer;
		if (*version != SERIALIZE_VERSION) {
			return -1;
		}

		buffer += sizeof(unsigned char);
		unsigned int* stream_sizes = (unsigned int*)buffer;
		unsigned int total_stream_size = 0;
		// For the moment assert that the size is smaller than an unsigned short
		for (unsigned int index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			if (stream_sizes[index] > USHORT_MAX) {
				return -1;
			}
			total_stream_size += stream_sizes[index];
		}

		buffer += sizeof(unsigned int) * ECS_ASSET_TYPE_COUNT;
		
		unsigned short* name_sizes = (unsigned short*)buffer;
		unsigned int total_name_size = 0;
		// For the moment assert that the path size is smaller than a unsigned char
		for (unsigned int index = 0; index < total_stream_size - stream_sizes[ECS_ASSET_MISC]; index++) {
			if (name_sizes[index] > UCHAR_MAX) {
				return -1;
			}
			total_name_size += name_sizes[index];
		}

		buffer += sizeof(unsigned short) * (total_stream_size - stream_sizes[ECS_ASSET_MISC]);

		unsigned short* path_sizes = (unsigned short*)buffer;
		unsigned int total_path_size = 0;
		// For the moment assert that the path size is smaller than a unsigned char
		// Skip the materials - they don't have any path
		for (unsigned int index = 0; index < total_stream_size - stream_sizes[ECS_ASSET_MATERIAL]; index++) {
			if (path_sizes[index] > UCHAR_MAX) {
				return -1;
			}
			total_path_size += path_sizes[index];
		}

		// Skip the materials - they don't have any path
		buffer += sizeof(unsigned short) * (total_stream_size - stream_sizes[ECS_ASSET_MATERIAL]);
		
		// Now the metadata comes
		buffer += SerializeMeshMetadataSize() * stream_sizes[ECS_ASSET_MESH];
		buffer += SerializeTextureMetadataSize() * stream_sizes[ECS_ASSET_TEXTURE];
		buffer += SerializeGPUBufferMetadataSize() * stream_sizes[ECS_ASSET_GPU_BUFFER];
		buffer += SerializeGPUSamplerMetadataSize() * stream_sizes[ECS_ASSET_GPU_SAMPLER];

		for (unsigned int index = 0; index < stream_sizes[ECS_ASSET_SHADER]; index++) {
			size_t current_size = DeserializeShaderMetadataSize(buffer);
			if (current_size == -1) {
				return -1;
			}
			buffer += current_size;
		}

		for (unsigned int index = 0; index < stream_sizes[ECS_ASSET_MATERIAL]; index++) {
			size_t current_size = DeserializeMaterialAssetSize(buffer);
			if (current_size == -1) {
				return -1;
			}
			buffer += current_size;
		}

		// Now the actual strings follow - but we don't care about their data
		return buffer - initial_buffer + total_path_size * sizeof(wchar_t) + total_name_size * sizeof(char);
	}

	// --------------------------------------------------------------------------------------

}
