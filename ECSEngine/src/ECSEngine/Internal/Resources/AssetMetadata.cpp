#include "ecspch.h"
#include "AssetMetadata.h"
#include "../../Utilities/File.h"
#include "../../Utilities/Function.h"
#include "../../Utilities/FunctionInterfaces.h"

namespace ECSEngine {
	
	// ------------------------------------------------------------------------------------------------------

	// Meant to be used only with an increment of 1 for the new counts
	void MaterialAssetRelocate(
		MaterialAsset* material, 
		unsigned int new_texture_count,
		unsigned int new_buffer_count,
		unsigned int new_sampler_count,
		unsigned int new_shader_count,
		AllocatorPolymorphic allocator
	) 
	{
		// Allocate a new buffer
		unsigned int total_count = new_texture_count + new_buffer_count + new_sampler_count + new_shader_count;
		void* allocation = AllocateEx(allocator, sizeof(MaterialAssetResource) * total_count);

		// Copy into it
		MaterialAssetResource* resources = (MaterialAssetResource*)allocation;
		material->textures.CopyTo(resources);
		material->buffers.CopyTo(resources + new_texture_count);
		material->samplers.CopyTo(resources + new_texture_count + new_buffer_count);
		material->shaders.CopyTo(resources + total_count - new_shader_count);

		// Release the old buffer
		if (material->textures.buffer != nullptr) {
			DeallocateEx(allocator, material->textures.buffer);
		}

		material->textures.buffer = resources;
		material->buffers.buffer = resources + new_texture_count;
		material->samplers.buffer = resources + new_texture_count + new_buffer_count;
		material->shaders.buffer = resources + total_count - new_buffer_count;

		material->textures.size = new_texture_count;
		material->buffers.size = new_buffer_count;
		material->samplers.size = new_sampler_count;
		material->shaders.size = new_shader_count;
	}

	// ------------------------------------------------------------------------------------------------------

	MaterialAsset::MaterialAsset(Stream<char> _name, AllocatorPolymorphic allocator)
	{
		name = function::StringCopy(allocator, _name);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::AddTexture(MaterialAssetResource texture, AllocatorPolymorphic allocator)
	{
		MaterialAssetRelocate(this, textures.size + 1, buffers.size, samplers.size, shaders.size, allocator);
		textures[textures.size - 1] = texture;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::AddBuffer(MaterialAssetResource buffer, AllocatorPolymorphic allocator)
	{
		MaterialAssetRelocate(this, textures.size , buffers.size + 1, samplers.size, shaders.size, allocator);
		buffers[buffers.size - 1] = buffer;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::AddSampler(MaterialAssetResource sampler, AllocatorPolymorphic allocator)
	{
		MaterialAssetRelocate(this, textures.size, buffers.size, samplers.size + 1, shaders.size, allocator);
		samplers[samplers.size - 1] = sampler;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::AddShader(MaterialAssetResource shader, AllocatorPolymorphic allocator)
	{
		MaterialAssetRelocate(this, textures.size, buffers.size, samplers.size, shaders.size + 1, allocator);
		shaders[shaders.size - 1] = shader;
	}

	// ------------------------------------------------------------------------------------------------------

	MaterialAsset MaterialAsset::Copy(void* buffer) const
	{
		MaterialAsset material;

		MaterialAssetResource* resources = (MaterialAssetResource*)buffer;
		material.textures.buffer = resources;
		material.textures.Copy(textures);

		material.buffers.buffer = resources + textures.size;
		material.buffers.Copy(buffers);

		material.samplers.buffer = material.buffers.buffer + buffers.size;
		material.samplers.Copy(samplers);

		material.shaders.buffer = material.samplers.buffer + samplers.size;
		material.shaders.Copy(shaders);
		return material;
	}

	// ------------------------------------------------------------------------------------------------------

	MaterialAsset MaterialAsset::Copy(AllocatorPolymorphic allocator) const
	{
		return Copy(Allocate(allocator, CopySize()));
	}

	// ------------------------------------------------------------------------------------------------------

	size_t MaterialAsset::CopySize() const
	{
		return sizeof(MaterialAssetResource) * (textures.size + buffers.size + samplers.size + shaders.size);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		if (BelongsToAllocator(allocator, name.buffer)) {
			Deallocate(allocator, name.buffer);
		}

		if (BelongsToAllocator(allocator, textures.buffer)) {
			Deallocate(allocator, textures.buffer);
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::RemoveTexture(unsigned int index, AllocatorPolymorphic allocator)
	{
		textures.RemoveSwapBack(index);
		// The size was already decremented
		MaterialAssetRelocate(this, textures.size, buffers.size, samplers.size, shaders.size, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::RemoveBuffer(unsigned int index, AllocatorPolymorphic allocator)
	{
		buffers.RemoveSwapBack(index);
		// The size was already decremented
		MaterialAssetRelocate(this, textures.size, buffers.size, samplers.size, shaders.size, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::RemoveSampler(unsigned int index, AllocatorPolymorphic allocator)
	{
		samplers.RemoveSwapBack(index);
		// The size was already decremented
		MaterialAssetRelocate(this, textures.size, buffers.size, samplers.size, shaders.size, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::RemoveShader(unsigned int index, AllocatorPolymorphic allocator)
	{
		shaders.RemoveSwapBack(index);
		// The size was already decremented
		MaterialAssetRelocate(this, textures.size, buffers.size, samplers.size, shaders.size, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	ShaderMetadata::ShaderMetadata() : name(nullptr, 0), macros(nullptr, 0) {}

	// ------------------------------------------------------------------------------------------------------

	ShaderMetadata::ShaderMetadata(Stream<char> _name, Stream<ShaderMacro> _macros, AllocatorPolymorphic allocator)
	{
		macros.buffer = (ShaderMacro*)AllocateEx(allocator, _macros.size * sizeof(ShaderMacro));
		
		// Now allocate every string separately
		macros.size = _macros.size;
		for (size_t index = 0; index < _macros.size; index++) {
			macros[index].name = function::StringCopy(allocator, _macros[index].name).buffer;
			macros[index].definition = function::StringCopy(allocator, _macros[index].definition).buffer;
		}

		name = function::StringCopy(allocator, _name);
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::AddMacro(const char* name, const char* definition, AllocatorPolymorphic allocator)
	{
		// Copy the string pointers to a temporary stack such that the memory can be freed first and then
		// reallocated
		ShaderMacro* temp_macros = (ShaderMacro*)ECS_STACK_ALLOC(sizeof(ShaderMacro) * macros.size);
		
		if (macros.size > 0) {
			memcpy(temp_macros, macros.buffer, sizeof(ShaderMacro) * macros.size);
			DeallocateEx(allocator, macros.buffer);
		}

		macros.buffer = (ShaderMacro*)AllocateEx(allocator, sizeof(ShaderMacro) * (macros.size + 1));
		memcpy(macros.buffer, temp_macros, sizeof(ShaderMacro) * macros.size);

		// Allocate the name and the definition separately
		const char* new_name = function::StringCopy(allocator, name).buffer;
		const char* new_definition = function::StringCopy(allocator, definition).buffer;

		macros.Add({ new_name, new_definition });
	}

	// ------------------------------------------------------------------------------------------------------

	ShaderMetadata ShaderMetadata::Copy(AllocatorPolymorphic allocator) const
	{
		return ShaderMetadata(name, macros, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);

		if (BelongsToAllocator(allocator, macros.buffer)) {
			for (size_t index = 0; index < macros.size; index++) {
				Deallocate(allocator, macros[index].name);
				Deallocate(allocator, macros[index].definition);
			}

			Deallocate(allocator, macros.buffer);
		}
	}

	// ------------------------------------------------------------------------------------------------------

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

	void MiscAsset::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, path.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	MiscAsset MiscAsset::Copy(AllocatorPolymorphic allocator) const
	{
		MiscAsset asset;
		asset.path = function::StringCopy(allocator, path);
		return asset;
	}

	// ------------------------------------------------------------------------------------------------------

	void GPUSamplerMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	GPUSamplerMetadata GPUSamplerMetadata::Copy(AllocatorPolymorphic allocator) const
	{
		GPUSamplerMetadata metadata;
		memcpy(&metadata, this, sizeof(metadata));
		metadata.name = function::StringCopy(allocator, name);
		return metadata;
	}

	// ------------------------------------------------------------------------------------------------------

	void GPUBufferMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	GPUBufferMetadata GPUBufferMetadata::Copy(AllocatorPolymorphic allocator) const
	{
		GPUBufferMetadata metadata;
		memcpy(&metadata, this, sizeof(metadata));
		metadata.name = function::StringCopy(allocator, name);
		return metadata;
	}

	// ------------------------------------------------------------------------------------------------------

	void TextureMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	TextureMetadata TextureMetadata::Copy(AllocatorPolymorphic allocator) const
	{
		TextureMetadata metadata;
		memcpy(&metadata, this, sizeof(metadata));
		metadata.name = function::StringCopy(allocator, name);
		return metadata;
	}

	// ------------------------------------------------------------------------------------------------------

	void MeshMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	MeshMetadata MeshMetadata::Copy(AllocatorPolymorphic allocator) const
	{
		MeshMetadata metadata;
		memcpy(&metadata, this, sizeof(metadata));
		metadata.name = function::StringCopy(allocator, name);
		return metadata;
	}

	// ------------------------------------------------------------------------------------------------------

}