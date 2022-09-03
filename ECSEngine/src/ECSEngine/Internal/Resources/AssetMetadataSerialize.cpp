#include "ecspch.h"
#include "AssetMetadata.h"
#include "AssetMetadataSerialize.h"
#include "AssetDatabase.h"
#include "../../Utilities/Serialization/Binary/Serialization.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------

	ECS_REFLECTION_CUSTOM_TYPE_DEPENDENT_TYPES_FUNCTION(MaterialAsset) {}

	// --------------------------------------------------------------------------------------------

	ECS_REFLECTION_CUSTOM_TYPE_MATCH_FUNCTION(MaterialAsset) {
		return Reflection::ReflectionCustomTypeMatchTemplate(data, STRING(MaterialAsset));
	}

	// --------------------------------------------------------------------------------------------

	ECS_REFLECTION_CUSTOM_TYPE_BYTE_SIZE_FUNCTION(MaterialAsset) {
		return { sizeof(MaterialAsset), alignof(MaterialAsset) };
	}

	// --------------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(MaterialAsset) {
		const void* user_data = ECS_SERIALIZE_CUSTOM_TYPES[Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET].user_data;
		ECS_ASSERT(user_data != nullptr);
		
		const AssetDatabase* database = (const AssetDatabase*)user_data;
		const MaterialAsset* asset = (const MaterialAsset*)data->data;

		// Write the sizes of the streams as unsigned shorts to not consume too much memory
		size_t write_size = 0;
		
		write_size += WriteWithSizeShort(data->stream, asset->name.buffer, asset->name.size * sizeof(char), data->write_data);
		write_size += Write(data->stream, &asset->textures.size, sizeof(unsigned short), data->write_data);
		write_size += Write(data->stream, &asset->buffers.size, sizeof(unsigned short), data->write_data);
		write_size += Write(data->stream, &asset->samplers.size, sizeof(unsigned short), data->write_data);
		write_size += Write(data->stream, &asset->shaders.size, sizeof(unsigned short), data->write_data);

		// Get the names of the resources being referenced and serialize these as well
		for (size_t index = 0; index < asset->textures.size; index++) {
			Stream<char> current_name = database->GetTextureConst(asset->textures[index].metadata_handle)->name;
			write_size += WriteWithSizeShort(data->stream, current_name.buffer, current_name.size, data->write_data);
			write_size += Write(data->stream, &asset->textures[index].slot, sizeof(asset->textures[index].slot), data->write_data);
		}

		for (size_t index = 0; index < asset->buffers.size; index++) {
			Stream<char> current_name = database->GetGPUBufferConst(asset->buffers[index].metadata_handle)->name;
			write_size += WriteWithSizeShort(data->stream, current_name.buffer, current_name.size, data->write_data);
			write_size += Write(data->stream, &asset->buffers[index].slot, sizeof(asset->buffers[index].slot), data->write_data);
		}

		for (size_t index = 0; index < asset->samplers.size; index++) {
			Stream<char> current_name = database->GetGPUSamplerConst(asset->samplers[index].metadata_handle)->name;
			write_size += WriteWithSizeShort(data->stream, current_name.buffer, current_name.size, data->write_data);
			write_size += Write(data->stream, &asset->samplers[index].slot, sizeof(asset->samplers[index].slot), data->write_data);
		}

		for (size_t index = 0; index < asset->shaders.size; index++) {
			Stream<char> current_name = database->GetShaderConst(asset->shaders[index])->name;
			write_size += WriteWithSizeShort(data->stream, current_name.buffer, current_name.size, data->write_data);
		}

		return write_size;
	}

	// --------------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(MaterialAsset) {
		if (data->version != ECS_SERIALIZE_CUSTOM_TYPE_MATERIAL_ASSET_VERSION) {
			return -1;
		}

		void* user_data = ECS_SERIALIZE_CUSTOM_TYPES[Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET].user_data;
		if (data->read_data) {
			ECS_ASSERT(user_data != nullptr);
		}

		AssetDatabase* database = (AssetDatabase*)user_data;
		MaterialAsset* asset = (MaterialAsset*)data->data;

		size_t read_size = 0;
		unsigned short name_size = 0;
		read_size += ReferenceDataWithSizeShort(data->stream, (void**)&asset->name.buffer, name_size, data->read_data);
		asset->name.size = name_size;
		
		unsigned short texture_size = 0;
		unsigned short buffer_size = 0;
		unsigned short sampler_size = 0;
		unsigned short shader_size = 0;
		Read<true>(data->stream, &texture_size, sizeof(unsigned short));
		Read<true>(data->stream, &buffer_size, sizeof(unsigned short));
		Read<true>(data->stream, &sampler_size, sizeof(unsigned short));
		Read<true>(data->stream, &shader_size, sizeof(unsigned short));

		ECS_STACK_CAPACITY_STREAM(char, name_buffer, 256);

		if (data->read_data) {
			AllocatorPolymorphic allocator = data->options->backup_allocator;
			// Allocate the buffers;
			asset->textures.Initialize(allocator, texture_size);
			asset->buffers.Initialize(allocator, buffer_size);
			asset->samplers.Initialize(allocator, sampler_size);
			asset->shaders.Initialize(allocator, shader_size);

			// Read the names now. Then use the database to get the handles for those resources
			for (size_t index = 0; index < texture_size; index++) {
				unsigned short name_size = 0;
				ReadWithSizeShort<true>(data->stream, name_buffer.buffer, name_size);
				name_buffer.size = name_size;

				unsigned char slot = 0;
				unsigned int handle = database->AddTexture(name_buffer);
				Read<true>(data->stream, &slot, sizeof(slot));
				asset->textures[index].metadata_handle = handle;
				asset->textures[index].slot = slot;
			}

			for (size_t index = 0; index < buffer_size; index++) {
				unsigned short name_size = 0;
				ReadWithSizeShort<true>(data->stream, name_buffer.buffer, name_size);
				name_buffer.size = name_size;

				unsigned char slot = 0;
				unsigned int handle = database->AddGPUBuffer(name_buffer);
				Read<true>(data->stream, &slot, sizeof(slot));
				asset->buffers[index].metadata_handle = handle;
				asset->buffers[index].slot = slot;
			}

			for (size_t index = 0; index < sampler_size; index++) {
				unsigned short name_size = 0;
				ReadWithSizeShort<true>(data->stream, name_buffer.buffer, name_size);
				name_buffer.size = name_size;

				unsigned char slot = 0;
				unsigned int handle = database->AddGPUSampler(name_buffer);
				Read<true>(data->stream, &slot, sizeof(slot));
				asset->samplers[index].metadata_handle = handle;
				asset->samplers[index].slot = slot;
			}

			for (size_t index = 0; index < shader_size; index++) {
				unsigned short name_size = 0;
				ReadWithSizeShort<true>(data->stream, name_buffer.buffer, name_size);
				name_buffer.size = name_size;

				unsigned int handle = database->AddTexture(name_buffer);
				asset->shaders[index] = handle;
			}
		}
		else {
			MaterialAsset temp_asset;
			temp_asset.textures.size = texture_size;
			temp_asset.buffers.size = buffer_size;
			temp_asset.samplers.size = sampler_size;
			temp_asset.shaders.size = shader_size;
			read_size += temp_asset.CopySize();
		}

		return read_size;
	}

	// --------------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_IS_TRIVIALLY_COPYABLE_FUNCTION(MaterialAsset) {
		return false;
	}

	// --------------------------------------------------------------------------------------------

	void SetSerializeCustomMaterialAssetDatabase(const AssetDatabase* database) {
		ECS_SERIALIZE_CUSTOM_TYPES[Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET].user_data = (void*)database;
	}

	// --------------------------------------------------------------------------------------------

	void SetSerializeCustomMaterialAssetDatabase(AssetDatabase* database) {
		ECS_SERIALIZE_CUSTOM_TYPES[Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET].user_data = database;
	}

	// --------------------------------------------------------------------------------------------

}