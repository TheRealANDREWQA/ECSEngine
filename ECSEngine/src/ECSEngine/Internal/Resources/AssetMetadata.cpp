#include "ecspch.h"
#include "AssetMetadata.h"
#include "../../Utilities/File.h"
#include "../../Utilities/Function.h"
#include "../../Utilities/FunctionInterfaces.h"

#define MESH_METADATA_VERSION 1
#define TEXTURE_METADATA_VERSION 1
#define GPU_BUFFER_METADATA_VERSION 1
#define GPU_SAMPLER_METADATA_VERSION 1
#define SHADER_METADATA_VERSION 1
#define MATERIAL_METADATA_VERSION 1

namespace ECSEngine {

	void SerializeMemcpyMetadata(void* metadata, unsigned char version, size_t sizeof_metadata, uintptr_t& buffer) {
		unsigned char* metadata_version = (unsigned char*)buffer;
		*metadata_version = MESH_METADATA_VERSION;

		buffer += sizeof(unsigned char);
		memcpy((void*)buffer, metadata, sizeof_metadata);
		buffer += sizeof_metadata;
	}

	ECS_ASSET_METADATA_CODE DeserializeMemcpyMetadata(void* metadata, unsigned char version, size_t sizeof_metadata, uintptr_t& buffer) {
		unsigned char* metadata_version = (unsigned char*)buffer;
		if (*metadata_version != version) {
			buffer += sizeof_metadata + sizeof(unsigned char);
			return ECS_ASSET_METADATA_VERSION_MISMATCH;
		}

		memcpy(metadata, (void*)(buffer + sizeof(unsigned char)), sizeof_metadata);
		buffer += sizeof_metadata + sizeof(unsigned char);
		return ECS_ASSET_METADATA_OK;
	}

	// ------------------------------------------------------------------------------------------------------

	void SerializeMeshMetadata(MeshMetadata metadata, uintptr_t& buffer)
	{
		SerializeMemcpyMetadata(&metadata, MESH_METADATA_VERSION, sizeof(metadata), buffer);
	}

	size_t SerializeMeshMetadataSize() {
		return sizeof(MeshMetadata) + sizeof(unsigned char);
	}

	// ------------------------------------------------------------------------------------------------------

	ECS_ASSET_METADATA_CODE DeserializeMeshMetadata(MeshMetadata* metadata, uintptr_t& buffer)
	{
		return DeserializeMemcpyMetadata(metadata, MESH_METADATA_VERSION, sizeof(*metadata), buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	void SerializeTextureMetadata(TextureMetadata metadata, uintptr_t& buffer)
	{
		SerializeMemcpyMetadata(&metadata, TEXTURE_METADATA_VERSION, sizeof(metadata), buffer);
	}

	size_t SerializeTextureMetadataSize() {
		return sizeof(TextureMetadata) + sizeof(unsigned char);
	}

	// ------------------------------------------------------------------------------------------------------

	ECS_ASSET_METADATA_CODE DeserializeTextureMetadata(TextureMetadata* metadata, uintptr_t& buffer)
	{
		return DeserializeMemcpyMetadata(metadata, TEXTURE_METADATA_VERSION, sizeof(*metadata), buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	void SerializeGPUBufferMetadata(GPUBufferMetadata metadata, uintptr_t& buffer) {
		SerializeMemcpyMetadata(&metadata, GPU_BUFFER_METADATA_VERSION, sizeof(metadata), buffer);
	}

	size_t SerializeGPUBufferMetadataSize() {
		return sizeof(GPUBufferMetadata) + sizeof(unsigned char);
	}

	// ------------------------------------------------------------------------------------------------------

	ECS_ASSET_METADATA_CODE DeserializeGPUBufferMetadata(GPUBufferMetadata* metadata, uintptr_t& buffer) {
		return DeserializeMemcpyMetadata(metadata, GPU_BUFFER_METADATA_VERSION, sizeof(*metadata), buffer);
	}
	// ------------------------------------------------------------------------------------------------------

	void SerializeGPUSamplerMetadata(GPUSamplerMetadata metadata, uintptr_t& buffer) {
		SerializeMemcpyMetadata(&metadata, GPU_SAMPLER_METADATA_VERSION, sizeof(metadata), buffer);
	}

	size_t SerializeGPUSamplerMetadataSize() {
		return sizeof(GPUSamplerMetadata) + sizeof(unsigned char);
	}

	// ------------------------------------------------------------------------------------------------------

	ECS_ASSET_METADATA_CODE DeserializeGPUSamplerMetadata(GPUSamplerMetadata* metadata, uintptr_t& buffer) {
		return DeserializeMemcpyMetadata(metadata, GPU_SAMPLER_METADATA_VERSION, sizeof(*metadata), buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	void SerializeShaderMetadata(ShaderMetadata metadata, uintptr_t& buffer) {
		unsigned char* version = (unsigned char*)buffer;
		*version = SHADER_METADATA_VERSION;

		buffer += sizeof(unsigned char);

		// Write the count of macros and the sizes of each string
		unsigned short* macro_count = (unsigned short*)buffer;
		*macro_count = metadata.macros.size;

		buffer += sizeof(unsigned short);
		unsigned short* macro_sizes = macro_count + 1;
		for (size_t index = 0; index < metadata.macros.size; index++) {
			macro_sizes[index * 2] = strlen(metadata.macros[index].name);
			macro_sizes[index * 2 + 1] = strlen(metadata.macros[index].definition);
		}

		buffer += sizeof(unsigned short) * metadata.macros.size * 2;

		char* characters = (char*)buffer;
		for (size_t index = 0; index < metadata.macros.size; index++) {
			memcpy(characters, metadata.macros[index].name, macro_sizes[index * 2]);
			characters += macro_sizes[index * 2];
			memcpy(characters, metadata.macros[index].definition, macro_sizes[index * 2 + 1]);
			characters += macro_sizes[index * 2 + 1];
		}

		buffer = (uintptr_t)characters;
	}

	size_t SerializeShaderMetadataSize(ShaderMetadata metadata) {
		// Version + macro count
		size_t size = sizeof(unsigned char) + sizeof(unsigned short);

		// Sizes of the strings
		size += sizeof(unsigned short) * metadata.macros.size * 2;

		// The actual strings
		for (size_t index = 0; index < metadata.macros.size; index++) {
			size += strlen(metadata.macros[index].name);
			size += strlen(metadata.macros[index].definition);
		}

		return size;
	}

	// ------------------------------------------------------------------------------------------------------

	ECS_ASSET_METADATA_CODE DeserializeShaderMetadata(ShaderMetadata* metadata, uintptr_t& buffer, AllocatorPolymorphic allocator) {
		unsigned char* version = (unsigned char*)buffer;
		if (*version != SHADER_METADATA_VERSION) {
			return ECS_ASSET_METADATA_VERSION_MISMATCH;
		}

		buffer += sizeof(unsigned char);
		unsigned short* macro_count = (unsigned short*)buffer;
		buffer += sizeof(unsigned short);

		unsigned short* macro_sizes = (unsigned short*)buffer;
		buffer += sizeof(unsigned short) * (*macro_count) * 2;

		metadata->macros.buffer = (ShaderMacro*)AllocateEx(allocator, sizeof(ShaderMacro) * *macro_count);
		metadata->macros.size = *macro_count;
		for (size_t index = 0; index < *macro_count; index++) {
			unsigned short name_size = macro_sizes[index * 2];
			unsigned short definition_size = macro_sizes[index * 2 + 1];

			// Null terminate the strings
			char* name_allocation = (char*)AllocateEx(allocator, name_size + sizeof(char));
			char* definition_allocation = (char*)AllocateEx(allocator, definition_size + sizeof(char));

			memcpy(name_allocation, (void*)buffer, name_size);
			name_allocation[name_size] = '\0';
			buffer += name_size;

			memcpy(definition_allocation, (void*)buffer, definition_size);
			definition_allocation[definition_size] = '\0';
			buffer += definition_size;

			metadata->macros[index].name = name_allocation;
			metadata->macros[index].definition = definition_allocation;
		}

		return ECS_ASSET_METADATA_OK;
	}

	size_t DeserializeShaderMetadataSize(uintptr_t buffer) {
		uintptr_t initial_buffer = buffer;
		unsigned char* version = (unsigned char*)buffer;
		if (*version != SHADER_METADATA_VERSION) {
			return -1;
		}

		buffer += sizeof(unsigned char);
		unsigned short* macro_count = (unsigned short*)buffer;
		buffer += sizeof(unsigned short);
		
		unsigned short* macro_sizes = (unsigned short*)buffer;
		buffer += sizeof(unsigned short) * (*macro_count) * 2;

		for (size_t index = 0; index < *macro_count; index++) {
			buffer += macro_sizes[index * 2] + 1;
			buffer += macro_sizes[index * 2 + 1] + 1;
		}

		return buffer - initial_buffer;
	}

	// ------------------------------------------------------------------------------------------------------

	bool SerializeMaterialAsset(MaterialAsset material, Stream<wchar_t> file)
	{
		size_t buffer_size = SerializeMaterialAssetSize(material);
		void* stack_buffer = ECS_STACK_ALLOC(buffer_size);
		uintptr_t buffer = (uintptr_t)stack_buffer;
		SerializeMaterialAsset(material, buffer);

		return WriteBufferToFileBinary(file, { stack_buffer, buffer_size }) == ECS_FILE_STATUS_OK;
	}

	void SerializeMaterialAsset(MaterialAsset material, uintptr_t& buffer)
	{
		// Write the version first
		unsigned char* version = (unsigned char*)buffer;
		*version = MATERIAL_METADATA_VERSION;

		buffer += sizeof(unsigned char);

		unsigned int* shader_indices = (unsigned int*)buffer;
		memcpy(shader_indices, material.shaders, sizeof(unsigned int) * (ECS_SHADER_TYPE_COUNT - 1));
		buffer += sizeof(unsigned int) * (ECS_SHADER_TYPE_COUNT - 1);
		
		// The sizes of the streams
		unsigned short* stream_sizes = (unsigned short*)buffer;

		stream_sizes[0] = material.textures.size;
		stream_sizes[1] = material.buffers.size;
		stream_sizes[2] = material.samplers.size;
		buffer += sizeof(unsigned short) * 3;

		MaterialAssetResource* resources = (MaterialAssetResource*)buffer;
		// Now the metadata indices
		memcpy(resources, material.textures.buffer, material.textures.size * sizeof(MaterialAssetResource));
		resources += material.textures.size;
		memcpy(resources, material.buffers.buffer, material.buffers.size * sizeof(MaterialAssetResource));
		resources += material.samplers.size;
		memcpy(resources, material.samplers.buffer, material.samplers.size * sizeof(MaterialAssetResource));
		
		buffer += sizeof(MaterialAssetResource) * (material.textures.size + material.buffers.size + material.samplers.size);
	}

	size_t SerializeMaterialAssetSize(MaterialAsset material) {
		// unsigned shorts - For the sizes of each stream and the version as unsigned char
		size_t size = sizeof(unsigned short) * 3 + sizeof(unsigned int) * ECS_SHADER_TYPE_COUNT + sizeof(unsigned char);

		// The sizes of the metadata references
		size += sizeof(MaterialAssetResource) * (material.textures.size + material.buffers.size + material.samplers.size);

		return size;
	}

	// ------------------------------------------------------------------------------------------------------

	ECS_ASSET_METADATA_CODE DeserializeMaterialAsset(MaterialAsset* material, Stream<wchar_t> file, AllocatorPolymorphic allocator)
	{
		const size_t stack_buffer_size = ECS_KB * 2;

		void* stack_buffer = ECS_STACK_ALLOC(stack_buffer_size);
		LinearAllocator stack_allocator(stack_buffer, stack_buffer_size);

		Stream<void> data = ReadWholeFileBinary(file, GetAllocatorPolymorphic(&stack_allocator));
		if (data.buffer == nullptr) {
			return ECS_ASSET_METADATA_FAILED_TO_READ;
		}

		uintptr_t buffer = (uintptr_t)data.buffer;
		return DeserializeMaterialAsset(material, buffer, allocator);
	}

	ECS_ASSET_METADATA_CODE DeserializeMaterialAsset(MaterialAsset* material, uintptr_t& buffer, AllocatorPolymorphic allocator)
	{
		unsigned char* version = (unsigned char*)buffer;
		if (*version != MATERIAL_METADATA_VERSION) {
			// Abort the propagation of the buffer displacement. Can't know for sure what data is left in the buffer
			return ECS_ASSET_METADATA_VERSION_MISMATCH;
		}

		buffer += sizeof(unsigned char);
		unsigned int* shader_indices = (unsigned int*)buffer;
		buffer += sizeof(unsigned int) * (ECS_SHADER_TYPE_COUNT - 1);
		memcpy(material->shaders, shader_indices, sizeof(unsigned int) * (ECS_SHADER_TYPE_COUNT - 1));
		
		unsigned short* stream_sizes = (unsigned short*)buffer;
		buffer += sizeof(unsigned short) * 3;

		if (stream_sizes[0] > 64 || stream_sizes[1] > 64 || stream_sizes[2] > 64) {
			return ECS_ASSET_METADATA_VERSION_INVALID_DATA;
		}

		material->textures.size = stream_sizes[0];
		material->buffers.size = stream_sizes[1];
		material->samplers.size = stream_sizes[2];

		size_t coallesced_allocation_size = sizeof(MaterialAssetResource) * (stream_sizes[0] + stream_sizes[1] + stream_sizes[2]);
		void* allocation = AllocateEx(allocator, coallesced_allocation_size);

		material->textures.buffer = (MaterialAssetResource*)allocation;
		material->buffers.buffer = (MaterialAssetResource*)function::OffsetPointer(allocation, sizeof(MaterialAssetResource) * stream_sizes[0]);
		material->samplers.buffer = (MaterialAssetResource*)function::OffsetPointer(material->buffers.buffer, sizeof(MaterialAssetResource) * stream_sizes[1]);

		memcpy(allocation, (void*)buffer, coallesced_allocation_size);
		buffer += coallesced_allocation_size;
		return ECS_ASSET_METADATA_OK;
	}

	ECS_ASSET_METADATA_CODE DeserializeMaterialAsset(MaterialAsset* material, uintptr_t& buffer)
	{
		unsigned char* version = (unsigned char*)buffer;
		if (*version != MATERIAL_METADATA_VERSION) {
			// Abort the propagation of the buffer displacement. Can't know for sure what data is left in the buffer
			return ECS_ASSET_METADATA_VERSION_MISMATCH;
		}

		buffer += sizeof(unsigned char);
		unsigned int* shader_indices = (unsigned int*)buffer;
		buffer += sizeof(unsigned int) * (ECS_SHADER_TYPE_COUNT - 1);
		memcpy(material->shaders, shader_indices, sizeof(unsigned int) * (ECS_SHADER_TYPE_COUNT - 1));

		unsigned short* stream_sizes = (unsigned short*)buffer;
		buffer += sizeof(unsigned short) * 3;

		if (stream_sizes[0] > 64 || stream_sizes[1] > 64 || stream_sizes[2] > 64) {
			return ECS_ASSET_METADATA_VERSION_INVALID_DATA;
		}

		material->textures.size = stream_sizes[0];
		material->buffers.size = stream_sizes[1];
		material->samplers.size = stream_sizes[2];

		material->textures.buffer = (MaterialAssetResource*)buffer;
		material->buffers.buffer = (MaterialAssetResource*)function::OffsetPointer(material->textures.buffer, sizeof(MaterialAssetResource) * stream_sizes[0]);
		material->samplers.buffer = (MaterialAssetResource*)function::OffsetPointer(material->buffers.buffer, sizeof(MaterialAssetResource) * stream_sizes[1]);

		buffer = (uintptr_t)material->samplers.buffer + sizeof(MaterialAssetResource) * stream_sizes[2];
		return ECS_ASSET_METADATA_OK;
	}

	size_t DeserializeMaterialAssetSize(uintptr_t buffer) {
		unsigned char* version = (unsigned char*)buffer;
		if (*version != MATERIAL_METADATA_VERSION) {
			return -1;
		}

		buffer += sizeof(unsigned int) * (ECS_SHADER_TYPE_COUNT - 1);
		unsigned short* stream_sizes = (unsigned short*)buffer;

		return sizeof(MaterialAssetResource) * (stream_sizes[0] + stream_sizes[1] + stream_sizes[2]) + sizeof(unsigned char) + 
			sizeof(unsigned int) * (ECS_SHADER_TYPE_COUNT - 1) + sizeof(unsigned short) * 3;
	}

	// ------------------------------------------------------------------------------------------------------

	// Meant to be used only with an increment of 1 for the new counts
	void MaterialAssetRelocate(
		MaterialAsset* material, 
		unsigned int new_texture_count,
		unsigned int new_buffer_count,
		unsigned int new_sampler_count,
		AllocatorPolymorphic allocator
	) 
	{
		// Allocate a new buffer
		void* allocation = AllocateEx(allocator, sizeof(MaterialAssetResource) * (new_texture_count + new_buffer_count + new_sampler_count));

		// Copy into it
		MaterialAssetResource* resources = (MaterialAssetResource*)allocation;
		material->textures.CopyTo(resources);
		material->buffers.CopyTo(resources + new_texture_count);
		material->samplers.CopyTo(resources + new_texture_count + new_buffer_count);

		// Release the old buffer
		if (material->textures.buffer != nullptr) {
			DeallocateEx(allocator, material->textures.buffer);
		}

		material->textures.buffer = resources;
		material->buffers.buffer = resources + new_texture_count;
		material->samplers.buffer = resources + new_texture_count + new_buffer_count;

		material->textures.size = new_texture_count;
		material->buffers.size = new_buffer_count;
		material->samplers.size = new_sampler_count;
	}

	void MaterialAsset::AddTexture(MaterialAssetResource texture, AllocatorPolymorphic allocator)
	{
		MaterialAssetRelocate(this, textures.size + 1, buffers.size, samplers.size, allocator);
		textures[textures.size - 1] = texture;
	}

	void MaterialAsset::AddBuffer(MaterialAssetResource buffer, AllocatorPolymorphic allocator)
	{
		MaterialAssetRelocate(this, textures.size , buffers.size + 1, samplers.size, allocator);
		buffers[buffers.size - 1] = buffer;
	}

	void MaterialAsset::AddSampler(MaterialAssetResource sampler, AllocatorPolymorphic allocator)
	{
		MaterialAssetRelocate(this, textures.size, buffers.size, samplers.size + 1, allocator);
		samplers[samplers.size - 1] = sampler;
	}

	MaterialAsset MaterialAsset::Copy(void* buffer)
	{
		MaterialAsset material;

		MaterialAssetResource* resources = (MaterialAssetResource*)buffer;
		material.textures.buffer = resources;
		material.textures.Copy(textures);

		material.buffers.buffer = resources + textures.size;
		material.buffers.Copy(buffers);

		material.samplers.buffer = material.buffers.buffer + buffers.size;
		material.samplers.Copy(samplers);

		memcpy(material.shaders, shaders, sizeof(unsigned int) * (ECS_SHADER_TYPE_COUNT - 1));
		return material;
	}

	size_t MaterialAsset::CopySize() const
	{
		return sizeof(MaterialAssetResource) * (textures.size + buffers.size + samplers.size);
	}

	void MaterialAsset::RemoveTexture(unsigned int index, AllocatorPolymorphic allocator)
	{
		textures.RemoveSwapBack(index);
		MaterialAssetRelocate(this, textures.size, buffers.size, samplers.size, allocator);
	}

	void MaterialAsset::RemoveBuffer(unsigned int index, AllocatorPolymorphic allocator)
	{
		buffers.RemoveSwapBack(index);
		MaterialAssetRelocate(this, textures.size, buffers.size, samplers.size, allocator);
	}

	void MaterialAsset::RemoveSampler(unsigned int index, AllocatorPolymorphic allocator)
	{
		samplers.RemoveSwapBack(index);
		MaterialAssetRelocate(this, textures.size, buffers.size, samplers.size, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	ShaderMetadata::ShaderMetadata() : macros(nullptr, 0) {}

	// ------------------------------------------------------------------------------------------------------

	ShaderMetadata::ShaderMetadata(Stream<ShaderMacro> _macros, AllocatorPolymorphic allocator)
	{
		macros.buffer = (ShaderMacro*)AllocateEx(allocator, _macros.size * sizeof(ShaderMacro));
		
		// Now allocate every string separately
		macros.size = _macros.size;
		for (size_t index = 0; index < _macros.size; index++) {
			macros[index].name = function::StringCopy(allocator, _macros[index].name).buffer;
			macros[index].definition = function::StringCopy(allocator, _macros[index].definition).buffer;
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::AddMacro(const char* name, const char* definition, AllocatorPolymorphic allocator)
	{
		// Copy the string pointers to a temporary stack such that the memory can be freed first and then
		// reallocated
		ShaderMacro* temp_macros = (ShaderMacro*)ECS_STACK_ALLOC(sizeof(ShaderMacro) * macros.size);
		memcpy(temp_macros, macros.buffer, sizeof(ShaderMacro) * macros.size);

		DeallocateEx(allocator, macros.buffer);
		macros.buffer = (ShaderMacro*)AllocateEx(allocator, sizeof(ShaderMacro) * (macros.size + 1));
		memcpy(macros.buffer, temp_macros, sizeof(ShaderMacro) * macros.size);

		// Allocate the name and the definition separately
		const char* new_name = function::StringCopy(allocator, name).buffer;
		const char* new_definition = function::StringCopy(allocator, definition).buffer;

		macros.Add({ new_name, new_definition });
	}

	void ShaderMetadata::RemoveMacro(size_t index, AllocatorPolymorphic allocator)
	{
		DeallocateEx(allocator, (void*)macros[index].name);
		DeallocateEx(allocator, (void*)macros[index].definition);

		macros.RemoveSwapBack(index);
		ShaderMacro* temp_macros = (ShaderMacro*)ECS_STACK_ALLOC(sizeof(ShaderMacro) * macros.size);
		macros.CopyTo(temp_macros);
		DeallocateEx(allocator, macros.buffer);
		
		macros.buffer = (ShaderMacro*)AllocateEx(allocator, sizeof(ShaderMacro) * macros.size);
		memcpy(macros.buffer, temp_macros, sizeof(ShaderMacro) * macros.size);
	}

	void ShaderMetadata::RemoveMacro(const char* name, AllocatorPolymorphic allocator)
	{
		size_t index = SearchMacro(name);
		ECS_ASSERT(index != -1);
		RemoveMacro(index, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::UpdateMacro(size_t index, const char* new_definition, AllocatorPolymorphic allocator)
	{
		DeallocateEx(allocator, (void*)macros[index].definition);
		macros[index].definition = function::StringCopy(allocator, new_definition).buffer;
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::UpdateMacro(const char* name, const char* new_definition, AllocatorPolymorphic allocator)
	{
		size_t index = SearchMacro(name);
		ECS_ASSERT(index != -1);
		UpdateMacro(index, new_definition, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	size_t ShaderMetadata::SearchMacro(const char* name) const
	{
		for (size_t index = 0; index < macros.size; index++) {
			if (function::CompareStrings(macros[index].name, name)) {
				return index;
			}
		}

		return -1;
	}

	// ------------------------------------------------------------------------------------------------------

}