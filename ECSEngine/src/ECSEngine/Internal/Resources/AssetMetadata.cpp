#include "ecspch.h"
#include "AssetMetadata.h"
#include "../../Utilities/File.h"
#include "../../Utilities/Function.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "../../Utilities/Path.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------------------------------

	Stream<char> ECS_ASSET_METADATA_MACROS[] = {
		STRING(ECS_MESH_HANDLE),
		STRING(ECS_TEXTURE_HANDLE),
		STRING(ECS_GPU_SAMPLER_HANDLE),
		STRING(ECS_SHADER_HANDLE),
		STRING(ECS_MATERIAL_HANDLE),
		STRING(ECS_MISC_HANDLE)
	};

	ECS_ASSET_TYPE FindAssetMetadataMacro(Stream<char> string) {
		size_t count = sizeof(ECS_ASSET_METADATA_MACROS) / sizeof(Stream<char>);
		for (size_t index = 0; index < count; index++) {
			if (string.size == ECS_ASSET_METADATA_MACROS[index].size && memcmp(string.buffer, ECS_ASSET_METADATA_MACROS[index].buffer, string.size * sizeof(char)) == 0) {
				return (ECS_ASSET_TYPE)index;
			}
		}
		return ECS_ASSET_TYPE_COUNT;
	}
	
	// ------------------------------------------------------------------------------------------------------

	// Meant to be used only with an increment of 1 for the new counts
	void MaterialAssetRelocate(
		MaterialAsset* material, 
		unsigned int new_texture_count,
		unsigned int new_sampler_count,
		unsigned int new_buffer_count,
		AllocatorPolymorphic allocator
	) 
	{
		// Allocate a new buffer
		size_t total_count = sizeof(MaterialAssetResource) * (new_texture_count + new_sampler_count) + sizeof(MaterialAssetBuffer) * new_buffer_count;
		void* allocation = AllocateEx(allocator, total_count);

		// Copy into it
		MaterialAssetResource* resources = (MaterialAssetResource*)allocation;
		uintptr_t ptr = (uintptr_t)resources;
		material->textures.CopyTo(ptr);
		material->samplers.CopyTo(ptr);
		material->buffers.CopyTo(ptr);

		// Release the old buffer
		if (material->textures.buffer != nullptr) {
			DeallocateEx(allocator, material->textures.buffer);
		}

		ptr = (uintptr_t)resources;
		material->textures.InitializeFromBuffer(ptr, new_texture_count);
		material->samplers.InitializeFromBuffer(ptr, new_sampler_count);
		material->buffers.InitializeFromBuffer(ptr, new_buffer_count);
	}

	// ------------------------------------------------------------------------------------------------------

	MaterialAsset::MaterialAsset(Stream<char> _name, AllocatorPolymorphic allocator)
	{
		name = function::StringCopy(allocator, _name);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::AddTexture(MaterialAssetResource texture, AllocatorPolymorphic allocator)
	{
		MaterialAssetRelocate(this, textures.size + 1, samplers.size, buffers.size, allocator);
		textures[textures.size - 1] = texture;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::AddSampler(MaterialAssetResource sampler, AllocatorPolymorphic allocator)
	{
		MaterialAssetRelocate(this, textures.size, samplers.size + 1, buffers.size, allocator);
		samplers[samplers.size - 1] = sampler;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::AddBuffer(MaterialAssetBuffer buffer, AllocatorPolymorphic allocator)
	{
		if (buffer.data.buffer != nullptr) {
			buffer.data = function::Copy(allocator, buffer.data);
		}
		MaterialAssetRelocate(this, textures.size, samplers.size, buffers.size + 1, allocator);
		buffers[buffers.size - 1] = buffer;
	}

	// ------------------------------------------------------------------------------------------------------

	MaterialAsset MaterialAsset::Copy(AllocatorPolymorphic allocator) const
	{
		MaterialAsset material;

		material.name = function::StringCopy(allocator, name);
		material.Resize(textures.size, samplers.size, buffers.size, allocator);
		material.textures.Copy(textures);
		material.samplers.Copy(samplers);
		material.buffers.Copy(buffers);

		for (size_t index = 0; index < buffers.size; index++) {
			if (buffers[index].data.buffer != nullptr) {
				material.buffers[index].data = function::Copy(allocator, buffers[index].data);
			}
		}

		material.pixel_shader_handle = pixel_shader_handle;
		material.vertex_shader_handle = vertex_shader_handle;
		material.material_pointer = material_pointer;

		return material;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);

		for (size_t index = 0; index < buffers.size; index++) {
			if (buffers[index].data.buffer != nullptr) {
				DeallocateIfBelongs(allocator, buffers[index].data.buffer);
			}
		}

		DeallocateIfBelongs(allocator, textures.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::Default(Stream<char> _name, Stream<wchar_t> _file)
	{
		memset(this, 0, sizeof(*this));
		name = _name;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::RemoveTexture(unsigned int index, AllocatorPolymorphic allocator)
	{
		textures.RemoveSwapBack(index);
		// The size was already decremented
		MaterialAssetRelocate(this, textures.size, samplers.size, buffers.size, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::RemoveSampler(unsigned int index, AllocatorPolymorphic allocator)
	{
		samplers.RemoveSwapBack(index);
		// The size was already decremented
		MaterialAssetRelocate(this, textures.size, samplers.size, buffers.size, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::RemoveBuffer(unsigned int index, AllocatorPolymorphic allocator)
	{
		buffers.RemoveSwapBack(index);
		// The size was already decremented
		MaterialAssetRelocate(this, textures.size, samplers.size, buffers.size, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::Resize(unsigned int texture_count, unsigned int sampler_count, unsigned int buffer_count, AllocatorPolymorphic allocator)
	{
		MaterialAssetRelocate(this, texture_count, sampler_count, buffer_count, allocator);
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
		file = { nullptr, 0 };
		shader_type = ECS_SHADER_PIXEL;
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
		ShaderMetadata metadata = ShaderMetadata(name, macros, allocator);

		metadata.compile_flag = compile_flag;
		metadata.file = function::StringCopy(allocator, file);
		metadata.shader_type = shader_type;
		metadata.shader_interface = shader_interface;

		return metadata;
	}

	// ------------------------------------------------------------------------------------------------------

	bool ShaderMetadata::SameTarget(const ShaderMetadata* other) const
	{
		if (shader_type != other->shader_type || compile_flag != other->compile_flag) {
			return false;
		}
		
		if (!function::CompareStrings(file, other->file)) {
			return false;
		}

		if (macros.size != other->macros.size) {
			return false;
		}

		for (size_t index = 0; index < macros.size; index++) {
			if (!function::CompareStrings(macros[index].definition, other->macros[index].definition) ||
				!function::CompareStrings(macros[index].name, other->macros[index].name)) {
				return false;
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
		DeallocateIfBelongs(allocator, file.buffer);

		if (BelongsToAllocator(allocator, macros.buffer)) {
			for (size_t index = 0; index < macros.size; index++) {
				Deallocate(allocator, macros[index].name);
				Deallocate(allocator, macros[index].definition);
			}

			Deallocate(allocator, macros.buffer);
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::Default(Stream<char> _name, Stream<wchar_t> _file)
	{
		memset(this, 0, sizeof(*this));
		name = _name;
		file = _file;
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
		DeallocateIfBelongs(allocator, file.buffer);
		DeallocateIfBelongs(allocator, name.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	MiscAsset MiscAsset::Copy(AllocatorPolymorphic allocator) const
	{
		MiscAsset asset;
		asset.file = function::StringCopy(allocator, file);
		asset.name = function::StringCopy(allocator, name);
		return asset;
	}

	// ------------------------------------------------------------------------------------------------------

	bool MiscAsset::SameTarget(const MiscAsset* other) const
	{
		return function::CompareStrings(file, other->file);
	}

	// ------------------------------------------------------------------------------------------------------

	void MiscAsset::Default(Stream<char> _name, Stream<wchar_t> _file) {
		name = _name;
		file = _file;
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

	void GPUSamplerMetadata::Default(Stream<char> _name, Stream<wchar_t> _file) {
		name = _name;
		filter_mode = ECS_SAMPLER_FILTER_ANISOTROPIC;
		address_mode = ECS_SAMPLER_ADDRESS_WRAP;
		anisotropic_level = 8;
	}

	// ------------------------------------------------------------------------------------------------------

	void TextureMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
		DeallocateIfBelongs(allocator, file.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	TextureMetadata TextureMetadata::Copy(AllocatorPolymorphic allocator) const
	{
		TextureMetadata metadata;
		memcpy(&metadata, this, sizeof(metadata));
		metadata.name = function::StringCopy(allocator, name);
		metadata.file = function::StringCopy(allocator, file);
		return metadata;
	}

	// ------------------------------------------------------------------------------------------------------

	void TextureMetadata::Default(Stream<char> _name, Stream<wchar_t> _file)
	{
		name = _name;
		sRGB = false;
		generate_mip_maps = true;
		file = _file;

		compression_type = ECS_TEXTURE_COMPRESSION_EX_NONE;
	}

	// ------------------------------------------------------------------------------------------------------

	bool TextureMetadata::SameTarget(const TextureMetadata* other) const
	{
		return function::CompareStrings(file, other->file) && sRGB == other->sRGB && generate_mip_maps && other->generate_mip_maps
			&& compression_type == other->compression_type;
	}

	// ------------------------------------------------------------------------------------------------------

	void MeshMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
		DeallocateIfBelongs(allocator, file.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	MeshMetadata MeshMetadata::Copy(AllocatorPolymorphic allocator) const
	{
		MeshMetadata metadata;
		memcpy(&metadata, this, sizeof(metadata));
		metadata.name = function::StringCopy(allocator, name);
		metadata.file = function::StringCopy(allocator, file);
		return metadata;
	}

	// ------------------------------------------------------------------------------------------------------

	void MeshMetadata::Default(Stream<char> _name, Stream<wchar_t> _file)
	{
		name = _name;
		scale_factor = 1.0f;
		invert_z_axis = true;
		file = _file;
		optimize_level = ECS_ASSET_MESH_OPTIMIZE_NONE;
	}

	// ------------------------------------------------------------------------------------------------------

	bool MeshMetadata::SameTarget(const MeshMetadata* other) const
	{
		return function::CompareStrings(file, other->file) && scale_factor == other->scale_factor && invert_z_axis == other->invert_z_axis
			&& optimize_level == other->optimize_level;
	}

	// ------------------------------------------------------------------------------------------------------

	const char* ECS_ASSET_TYPE_CONVERSION[] = {
		"Mesh",
		"Texture",
		"GPUSampler",
		"Shader",
		"Material",
		"Misc"
	};

	static_assert(std::size(ECS_ASSET_TYPE_CONVERSION) == ECS_ASSET_TYPE_COUNT);

	const char* ConvertAssetTypeString(ECS_ASSET_TYPE type)
	{
		return ECS_ASSET_TYPE_CONVERSION[type];
	}

	// ------------------------------------------------------------------------------------------------------

	void DeallocateAssetBase(const void* asset, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator)
	{
		switch (type) {
		case ECS_ASSET_MESH:
		{
			((MeshMetadata*)asset)->DeallocateMemory(allocator);
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			((TextureMetadata*)asset)->DeallocateMemory(allocator);
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			((GPUSamplerMetadata*)asset)->DeallocateMemory(allocator);
		}
		break;
		case ECS_ASSET_SHADER:
		{
			((ShaderMetadata*)asset)->DeallocateMemory(allocator);
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			((MaterialAsset*)asset)->DeallocateMemory(allocator);
		}
		break;
		case ECS_ASSET_MISC:
		{
			((MiscAsset*)asset)->DeallocateMemory(allocator);
		}
		break;
		}
	}

	// ------------------------------------------------------------------------------------------------------

	Stream<wchar_t> GetAssetFile(const void* asset, ECS_ASSET_TYPE type)
	{
		switch (type) {
		case ECS_ASSET_MESH:
		{
			return ((MeshMetadata*)asset)->file;
		}
		case ECS_ASSET_TEXTURE:
		{
			return ((TextureMetadata*)asset)->file;
		}
		case ECS_ASSET_GPU_SAMPLER:
		{
			return {};
		}
		case ECS_ASSET_SHADER:
		{
			return ((ShaderMetadata*)asset)->file;
		}
		case ECS_ASSET_MATERIAL:
		{
			return {};
		}
		case ECS_ASSET_MISC:
		{
			return ((MiscAsset*)asset)->file;
		}
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void CreateDefaultAsset(void* asset, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type)
	{
		switch (type) {
		case ECS_ASSET_MESH:
		{
			((MeshMetadata*)asset)->Default(name, file);
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			((TextureMetadata*)asset)->Default(name, file);
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			((GPUSamplerMetadata*)asset)->Default(name, file);
		}
		break;
		case ECS_ASSET_SHADER:
		{
			((ShaderMetadata*)asset)->Default(name, file);
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			((MaterialAsset*)asset)->Default(name, file);
		}
		break;
		case ECS_ASSET_MISC:
		{
			((MiscAsset*)asset)->Default(name, file);
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type.");
		}
	}

	// ------------------------------------------------------------------------------------------------------

	Stream<void> GetAssetFromMetadata(const void* metadata, ECS_ASSET_TYPE type)
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return { ((MeshMetadata*)metadata)->mesh_pointer, 0 };
		case ECS_ASSET_TEXTURE:
			return { ((TextureMetadata*)metadata)->texture.Interface(), 0 };
		case ECS_ASSET_GPU_SAMPLER:
			return { ((GPUSamplerMetadata*)metadata)->sampler.Interface(), 0 };
		case ECS_ASSET_SHADER:
			return { ((ShaderMetadata*)metadata)->shader_interface, 0 };
		case ECS_ASSET_MATERIAL:
			return { ((MaterialAsset*)metadata)->material_pointer, 0 };
		case ECS_ASSET_MISC:
			return ((MiscAsset*)metadata)->data;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
		return { nullptr, 0 };
	}

	// ------------------------------------------------------------------------------------------------------

}