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
		
		auto write_size_for_stream = [&write_size, data](auto stream) {
			for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
				write_size += Write(data->stream, &stream[index].size, sizeof(unsigned short), data->write_data);
			}
		};

		write_size += WriteWithSizeShort(data->stream, asset->name.buffer, asset->name.size * sizeof(char), data->write_data);
		write_size_for_stream(asset->textures);
		write_size_for_stream(asset->samplers);
		write_size_for_stream(asset->buffers);
		write_size += Write(data->stream, &asset->vertex_shader_handle, sizeof(asset->vertex_shader_handle), data->write_data);
		write_size += Write(data->stream, &asset->pixel_shader_handle, sizeof(asset->pixel_shader_handle), data->write_data);

		for (size_t type = 0; type < ECS_MATERIAL_SHADER_COUNT; type++) {
			// Get the names of the resources being referenced and serialize these as well
			for (size_t index = 0; index < asset->textures[type].size; index++) {
				MaterialAssetResource texture = asset->textures[type][index];
				Stream<char> current_name = { nullptr, 0 };
				Stream<wchar_t> file = { nullptr, 0 };

				if (texture.metadata_handle != -1) {
					const auto* texture_metadata = database->GetTextureConst(texture.metadata_handle);
					current_name = texture_metadata->name;
					file = texture_metadata->file;
				}

				write_size += WriteWithSizeShort(data->stream, current_name, data->write_data);
				write_size += WriteWithSizeShort(data->stream, file, data->write_data);
				write_size += Write(data->stream, &texture.slot, sizeof(texture.slot), data->write_data);
			}

			// Get the names of the resources being referenced and serialize these as well
			for (size_t index = 0; index < asset->samplers[type].size; index++) {
				MaterialAssetResource sampler = asset->samplers[type][index];
				Stream<char> current_name = { nullptr, 0 };
				if (sampler.metadata_handle) {
					const auto* sampler_metadata = database->GetGPUSamplerConst(sampler.metadata_handle);
					current_name = sampler_metadata->name;
				}
				write_size += WriteWithSizeShort(data->stream, current_name, data->write_data);
				write_size += Write(data->stream, &sampler.slot, sizeof(sampler.slot), data->write_data);
			}

			for (size_t index = 0; index < asset->buffers[type].size; index++) {
				MaterialAssetBuffer buffer = asset->buffers[type][index];
				write_size += Write(data->stream, &buffer.slot, sizeof(buffer.slot), data->write_data);
				write_size += Write(data->stream, &buffer.dynamic, sizeof(buffer.dynamic), data->write_data);
				if (!asset->buffers[type][index].dynamic) {
					// If static, the data needs to be specified
					ECS_ASSERT(buffer.data.buffer != nullptr);
					write_size += WriteWithSizeShort(data->stream, buffer.data, data->write_data);
				}
				else {
					// Write a bool indicating whether or not there is data
					bool has_data = buffer.data.buffer != nullptr;
					write_size += Write(data->stream, &has_data, sizeof(has_data), data->write_data);
					if (has_data) {
						write_size += WriteWithSizeShort(data->stream, buffer.data, data->write_data);
					}
					else {
						write_size += Write(data->stream, &buffer.data.size, sizeof(unsigned short), data->write_data);
					}
				}
			}
		}
		
		Stream<char> vertex_name = { nullptr, 0 };
		Stream<wchar_t> vertex_file = { nullptr, 0 };
		if (asset->vertex_shader_handle != -1) {
			const auto* vertex_shader = database->GetShaderConst(asset->vertex_shader_handle);
			vertex_name = vertex_shader->name;
			vertex_file = vertex_shader->file;
		}
		write_size += WriteWithSizeShort(data->stream, vertex_name, data->write_data);
		write_size += WriteWithSizeShort(data->stream, vertex_file, data->write_data);

		Stream<char> pixel_name = { nullptr, 0 };
		Stream<wchar_t> pixel_file = { nullptr, 0 };
		if (asset->pixel_shader_handle != -1) {
			const auto* pixel_shader = database->GetShaderConst(asset->pixel_shader_handle);
			pixel_name = pixel_shader->name;
			pixel_file = pixel_shader->file;
		}
		write_size += WriteWithSizeShort(data->stream, pixel_name, data->write_data);
		write_size += WriteWithSizeShort(data->stream, pixel_file, data->write_data);

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

		Stream<char> asset_name = { nullptr, 0 };
		if (data->read_data) {
			asset_name = ReadAllocateDataShort<true>(data->stream, allocator).As<char>();
		}
		else {
			read_size += IgnoreWithSizeShort(data->stream);
		}
		
		unsigned short counts[ECS_MATERIAL_SHADER_COUNT * 3];
		Read<true>(data->stream, counts, sizeof(counts));

		ECS_STACK_CAPACITY_STREAM(char, name_buffer, 256);
		ECS_STACK_CAPACITY_STREAM(wchar_t, file_buffer, 256);

		if (data->read_data) {
			unsigned int int_counts[ECS_MATERIAL_SHADER_COUNT * 3];
			for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT * 3; index++) {
				int_counts[index] = counts[index];
			}

			// Allocate the buffers
			memset(asset, 0, sizeof(*asset));
			asset->Resize(int_counts, allocator, true);
			asset->name = asset_name;
		}

		Read(data->stream, &asset->vertex_shader_handle, sizeof(asset->vertex_shader_handle), data->read_data);
		Read(data->stream, &asset->pixel_shader_handle, sizeof(asset->pixel_shader_handle), data->read_data);

		for (size_t type = 0; type < ECS_MATERIAL_SHADER_COUNT; type++) {
			// Read the names now. Then use the database to get the handles for those resources
			for (unsigned short index = 0; index < asset->textures[type].size; index++) {
				unsigned short name_size = 0;
				read_size += ReadWithSizeShort(data->stream, name_buffer.buffer, name_size, data->read_data);
				name_buffer.size = name_size / sizeof(char);

				unsigned short file_size = 0;
				read_size += ReadWithSizeShort(data->stream, file_buffer.buffer, file_size, data->read_data);
				file_buffer.size = file_size / sizeof(wchar_t);

				unsigned char slot = 0;
				Read(data->stream, &slot, sizeof(slot), data->read_data);
				if (data->read_data) {
					unsigned int handle = -1;
					if (name_buffer.size > 0 && file_buffer.size > 0) {
						handle = database->AddTexture(name_buffer, file_buffer);
					}
					asset->textures[type][index].metadata_handle = handle;
					asset->textures[type][index].slot = slot;
				}
			}

			for (unsigned short index = 0; index < asset->samplers[type].size; index++) {
				unsigned short name_size = 0;
				read_size += ReadWithSizeShort(data->stream, name_buffer.buffer, name_size, data->read_data);
				name_buffer.size = name_size / sizeof(char);

				unsigned char slot = 0;
				Read(data->stream, &slot, sizeof(slot), data->read_data);
				if (data->read_data) {
					unsigned int handle = -1;
					if (name_buffer.size > 0 && file_buffer.size > 0) {
						handle = database->AddGPUSampler(name_buffer);
					}
					asset->samplers[type][index].metadata_handle = handle;
					asset->samplers[type][index].slot = slot;
				}
			}

			for (unsigned short index = 0; index < asset->buffers[type].size; index++) {
				unsigned char slot;
				bool dynamic;
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
						current_data.size = data_size;
					}
				}

				if (data->read_data) {
					asset->buffers[type][index].data = current_data;
					asset->buffers[type][index].dynamic = dynamic;
					asset->buffers[type][index].slot = slot;
				}
				read_size += current_data.size;
			}
		}

		if (data->read_data) {
			unsigned short vertex_shader_name_size = 0;
			read_size += ReadWithSizeShort<true>(data->stream, name_buffer.buffer, vertex_shader_name_size);
			name_buffer.size = vertex_shader_name_size / sizeof(char);

			unsigned short vertex_shader_file_size = 0;
			read_size += ReadWithSizeShort<true>(data->stream, file_buffer.buffer, vertex_shader_file_size);
			file_buffer.size = vertex_shader_file_size / sizeof(wchar_t);

			unsigned int vertex_handle = -1;
			if (name_buffer.size > 0 && file_buffer.size > 0) {
				vertex_handle = database->AddShader(name_buffer, file_buffer);
			}
			asset->vertex_shader_handle = vertex_handle;

			unsigned short pixel_shader_name_size = 0;
			read_size += ReadWithSizeShort<true>(data->stream, name_buffer.buffer, pixel_shader_name_size);
			name_buffer.size = pixel_shader_name_size;

			unsigned short pixel_shader_file_size = 0;
			read_size += ReadWithSizeShort<true>(data->stream, file_buffer.buffer, pixel_shader_file_size);
			file_buffer.size = pixel_shader_file_size / sizeof(wchar_t);

			unsigned int pixel_handle = -1;
			if (name_buffer.size > 0 && file_buffer.size > 0) {
				pixel_handle = database->AddShader(name_buffer, file_buffer);
			}
			asset->pixel_shader_handle = pixel_handle;
		}
		else {
			// The vertex shader name + file
			IgnoreWithSizeShort(data->stream);
			IgnoreWithSizeShort(data->stream);
			// The pixel shader name + file
			IgnoreWithSizeShort(data->stream);
			IgnoreWithSizeShort(data->stream);
		}

		return read_size;
	}

	// --------------------------------------------------------------------------------------------

	ECS_REFLECTION_CUSTOM_TYPE_IS_BLITTABLE_FUNCTION(MaterialAsset) {
		return false;
	}

	// --------------------------------------------------------------------------------------------

	ECS_REFLECTION_CUSTOM_TYPE_COPY_FUNCTION(MaterialAsset) {
		MaterialAsset* source = (MaterialAsset*)data->source;
		MaterialAsset* destination = (MaterialAsset*)data->destination;
		*destination = source->Copy(data->allocator);
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