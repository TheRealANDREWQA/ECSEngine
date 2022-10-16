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
		return function::CompareStrings(data->definition, STRING(MaterialAsset));
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
		write_size += Write(data->stream, &asset->samplers.size, sizeof(unsigned short), data->write_data);
		write_size += Write(data->stream, &asset->buffers.size, sizeof(unsigned short), data->write_data);
		write_size += Write(data->stream, &asset->vertex_shader_handle, sizeof(asset->vertex_shader_handle), data->write_data);
		write_size += Write(data->stream, &asset->pixel_shader_handle, sizeof(asset->pixel_shader_handle), data->write_data);

		// Get the names of the resources being referenced and serialize these as well
		for (size_t index = 0; index < asset->textures.size; index++) {
			const auto* texture = database->GetTextureConst(asset->textures[index].metadata_handle);
			Stream<char> current_name = texture->name;
			Stream<wchar_t> file = texture->file;
			write_size += WriteWithSizeShort(data->stream, current_name, data->write_data);
			write_size += WriteWithSizeShort(data->stream, file, data->write_data);
			write_size += Write(data->stream, &asset->textures[index].slot, sizeof(asset->textures[index].slot), data->write_data);
		}

		// Get the names of the resources being referenced and serialize these as well
		for (size_t index = 0; index < asset->samplers.size; index++) {
			const auto* sampler = database->GetGPUSamplerConst(asset->samplers[index].metadata_handle);
			Stream<char> current_name = sampler->name;
			write_size += WriteWithSizeShort(data->stream, current_name, data->write_data);
			write_size += Write(data->stream, &asset->textures[index].slot, sizeof(asset->textures[index].slot), data->write_data);
		}

		for (size_t index = 0; index < asset->buffers.size; index++) {
			write_size += Write(data->stream, &asset->buffers[index].shader_type, sizeof(asset->buffers[index].shader_type), data->write_data);
			write_size += Write(data->stream, &asset->buffers[index].slot, sizeof(asset->buffers[index].slot), data->write_data);
			write_size += Write(data->stream, &asset->buffers[index].dynamic, sizeof(asset->buffers[index].dynamic), data->write_data);
			if (!asset->buffers[index].dynamic) {
				// If static, the data needs to be specified
				ECS_ASSERT(asset->buffers[index].data.buffer != nullptr);
				write_size += WriteWithSizeShort(data->stream, asset->buffers[index].data, data->write_data);
			}
			else {
				// Write a bool indicating whether or not there is data
				bool has_data = asset->buffers[index].data.buffer != nullptr;
				write_size += Write(data->stream, &has_data, sizeof(has_data), data->write_data);
				if (has_data) {
					write_size += WriteWithSizeShort(data->stream, asset->buffers[index].data, data->write_data);
				}
				else {
					write_size += Write(data->stream, &asset->buffers[index].data.size, sizeof(unsigned short), data->write_data);
				}
			}
		}

		const auto* vertex_shader = database->GetShaderConst(asset->vertex_shader_handle);
		write_size += WriteWithSizeShort(data->stream, vertex_shader->name, data->write_data);
		write_size += WriteWithSizeShort(data->stream, vertex_shader->file, data->write_data);

		const auto* pixel_shader = database->GetShaderConst(asset->vertex_shader_handle);
		write_size += WriteWithSizeShort(data->stream, pixel_shader->name, data->write_data);
		write_size += WriteWithSizeShort(data->stream, pixel_shader->file, data->write_data);

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
		AllocatorPolymorphic allocator = data->options->field_allocator;

		if (data->read_data) {
			Stream<void> name = ReadAllocateDataShort<true>(data->stream, allocator);
		}
		else {
			read_size += IgnoreWithSizeShort(data->stream);
		}
		
		unsigned short texture_size = 0;
		unsigned short sampler_size = 0;
		unsigned short buffer_size = 0;
		Read<true>(data->stream, &texture_size, sizeof(unsigned short));
		Read<true>(data->stream, &sampler_size, sizeof(unsigned short));
		Read<true>(data->stream, &buffer_size, sizeof(unsigned short));

		ECS_STACK_CAPACITY_STREAM(char, name_buffer, 256);
		ECS_STACK_CAPACITY_STREAM(wchar_t, file_buffer, 256);

		if (data->read_data) {
			// Allocate the buffers;
			memset(asset, 0, sizeof(*asset));
			asset->Resize(texture_size, sampler_size, buffer_size, allocator);
		}
		Read(data->stream, &asset->vertex_shader_handle, sizeof(asset->vertex_shader_handle), data->read_data);
		Read(data->stream, &asset->pixel_shader_handle, sizeof(asset->pixel_shader_handle), data->read_data);

		// Read the names now. Then use the database to get the handles for those resources
		for (unsigned short index = 0; index < texture_size; index++) {
			unsigned short name_size = 0;
			read_size += ReadWithSizeShort(data->stream, name_buffer.buffer, name_size, data->read_data);
			name_buffer.size = name_size / sizeof(char);

			unsigned short file_size = 0;
			read_size += ReadWithSizeShort(data->stream, file_buffer.buffer, file_size, data->read_data);
			file_buffer.size = file_size / sizeof(wchar_t);

			unsigned char slot = 0;
			Read(data->stream, &slot, sizeof(slot), data->read_data);
			if (data->read_data) {
				unsigned int handle = database->AddTexture(name_buffer, file_buffer);
				asset->textures[index].metadata_handle = handle;
				asset->textures[index].slot = slot;
			}
		}

		for (unsigned short index = 0; index < sampler_size; index++) {
			unsigned short name_size = 0;
			read_size += ReadWithSizeShort(data->stream, name_buffer.buffer, name_size, data->read_data);
			name_buffer.size = name_size / sizeof(char);

			unsigned char slot = 0;
			Read(data->stream, &slot, sizeof(slot), data->read_data);
			if (data->read_data) {
				unsigned int handle = database->AddGPUSampler(name_buffer);
				asset->samplers[index].metadata_handle = handle;
				asset->textures[index].slot = slot;
			}
		}

		for (unsigned short index = 0; index < buffer_size; index++) {
			ECS_SHADER_TYPE shader_type;
			unsigned char slot;
			bool dynamic;
			Read<true>(data->stream, &shader_type, sizeof(shader_type));
			Read<true>(data->stream, &slot, sizeof(slot));
			Read<true>(data->stream, &dynamic, sizeof(dynamic));

			Stream<void> current_data = { nullptr, 0 };
			if (!dynamic) {
				current_data = ReadAllocateDataShort(data->stream, allocator, data->read_data);
			}
			else {
				bool has_data;
				Read<true>(data->stream, &has_data, sizeof(has_data));
				if (has_data) {
					current_data = ReadAllocateDataShort(data->stream, allocator, data->read_data);
				}
				else {
					unsigned short data_size;
					Read<true>(data->stream, &data_size, sizeof(data_size));
					Ignore(data->stream, data_size);
					current_data.size = data_size;
				}
			}

			if (data->read_data) {
				asset->buffers[index].data = current_data;
				asset->buffers[index].dynamic = dynamic;
				asset->buffers[index].shader_type = shader_type;
				asset->buffers[index].slot = slot;
			}
			read_size += current_data.size;
		}

		if (data->read_data) {
			unsigned short vertex_shader_name_size = 0;
			read_size += ReadWithSizeShort<true>(data->stream, name_buffer.buffer, vertex_shader_name_size);
			name_buffer.size = vertex_shader_name_size / sizeof(char);

			unsigned short vertex_shader_file_size = 0;
			read_size += ReadWithSizeShort<true>(data->stream, file_buffer.buffer, vertex_shader_file_size);
			file_buffer.size = vertex_shader_file_size / sizeof(wchar_t);

			unsigned int vertex_handle = database->AddShader(name_buffer, file_buffer);
			asset->vertex_shader_handle = vertex_handle;

			unsigned short pixel_shader_name_size = 0;
			read_size += ReadWithSizeShort<true>(data->stream, name_buffer.buffer, pixel_shader_name_size);
			name_buffer.size = pixel_shader_name_size;

			unsigned short pixel_shader_file_size = 0;
			read_size += ReadWithSizeShort<true>(data->stream, file_buffer.buffer, pixel_shader_file_size);
			file_buffer.size = pixel_shader_file_size / sizeof(wchar_t);

			unsigned int pixel_handle = database->AddShader(name_buffer, file_buffer);
			asset->pixel_shader_handle = pixel_handle;
		}
		else {
			// The vertex shader name
			IgnoreWithSizeShort(data->stream);
			// The pixel shader name
			IgnoreWithSizeShort(data->stream);
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