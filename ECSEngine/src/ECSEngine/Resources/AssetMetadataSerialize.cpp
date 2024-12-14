#include "ecspch.h"
#include "AssetMetadata.h"
#include "AssetMetadataSerialize.h"
#include "AssetDatabase.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Utilities/Reflection/ReflectionCustomTypes.h"

namespace ECSEngine {

	using namespace Reflection;

	bool MaterialAssetCustomTypeInterface::Match(Reflection::ReflectionCustomTypeMatchData* data)
	{
		return data->definition == STRING(MaterialAsset);
	}

	ulong2 MaterialAssetCustomTypeInterface::GetByteSize(Reflection::ReflectionCustomTypeByteSizeData* data)
	{
		return { sizeof(MaterialAsset), alignof(MaterialAsset) };
	}

	void MaterialAssetCustomTypeInterface::GetDependentTypes(Reflection::ReflectionCustomTypeDependentTypesData* data) {}

	bool MaterialAssetCustomTypeInterface::IsBlittable(Reflection::ReflectionCustomTypeIsBlittableData* data)
	{
		return false;
	}

	void MaterialAssetCustomTypeInterface::Copy(Reflection::ReflectionCustomTypeCopyData* data)
	{
		const MaterialAsset* source = (const MaterialAsset*)data->source;
		MaterialAsset* destination = (MaterialAsset*)data->destination;
		if (data->deallocate_existing_data) {
			destination->DeallocateMemory(data->allocator);
		}
		*destination = source->Copy(data->allocator);
	}

	bool MaterialAssetCustomTypeInterface::Compare(Reflection::ReflectionCustomTypeCompareData* data)
	{
		MaterialAsset* first = (MaterialAsset*)data->first;
		MaterialAsset* second = (MaterialAsset*)data->second;

		return first->name == second->name && first->CompareOptions(second);
	}

	void MaterialAssetCustomTypeInterface::Deallocate(Reflection::ReflectionCustomTypeDeallocateData* data) {
		for (size_t index = 0; index < data->element_count; index++) {
			void* current_source = OffsetPointer(data->source, index * sizeof(MaterialAsset));
			MaterialAsset* asset = (MaterialAsset*)current_source;
			asset->DeallocateMemory(data->allocator);
		}
	}

	// --------------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_WRITE_FUNCTION(MaterialAsset) {
		const void* user_data = ECS_SERIALIZE_CUSTOM_TYPES[Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET].user_data;
		ECS_ASSERT(user_data != nullptr);
		
		const AssetDatabase* database = (const AssetDatabase*)user_data;
		const MaterialAsset* asset = (const MaterialAsset*)data->data;

		bool has_reflection_buffers = false;
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			for (size_t subindex = 0; subindex < asset->buffers[index].size && !has_reflection_buffers; subindex++) {
				has_reflection_buffers |= asset->buffers[index][subindex].reflection_type != nullptr;
			}
		}

		ECS_ASSERT(!has_reflection_buffers || asset->reflection_manager != nullptr);

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

				write_size += Write(data->stream, &texture.slot, sizeof(texture.slot), data->write_data);

				// This is the name of the assigned texture
				write_size += WriteWithSizeShort(data->stream, current_name, data->write_data);
				write_size += WriteWithSizeShort(data->stream, file, data->write_data);

				// This is the name in the file as variable (i.e. Texture2D my_texture -> my_texture; it is not
				// the name of the texture assigned through the UI). This is written last because it helps on deserialization
				write_size += WriteWithSizeShort(data->stream, texture.name, data->write_data);
			}

			// Get the names of the resources being referenced and serialize these as well
			for (size_t index = 0; index < asset->samplers[type].size; index++) {
				MaterialAssetResource sampler = asset->samplers[type][index];
				Stream<char> current_name = { nullptr, 0 };
				if (sampler.metadata_handle != -1) {
					const auto* sampler_metadata = database->GetGPUSamplerConst(sampler.metadata_handle);
					current_name = sampler_metadata->name;
				}

				write_size += Write(data->stream, &sampler.slot, sizeof(sampler.slot), data->write_data);
				write_size += WriteWithSizeShort(data->stream, current_name, data->write_data);

				// This is the name in the file as variable (i.e. SamplerState my_sampler -> my_sampler; it is not
				// the name of the sampler assigned through the UI). This is written last because it helps on deserialization
				write_size += WriteWithSizeShort(data->stream, sampler.name, data->write_data);
			}

			// Returns -1 if it fails, else 0
			auto write_buffer_data = [&](size_t index) {
				MaterialAssetBuffer buffer = asset->buffers[type][index];
				SerializeOptions serialize_options;
				serialize_options.write_type_table_tags = true;
				
				auto serialize = [&]() {
					if (data->write_data) {
						ECS_SERIALIZE_CODE code = Serialize(asset->reflection_manager, buffer.reflection_type, buffer.data.buffer, *data->stream, &serialize_options);
						if (code != ECS_SERIALIZE_OK) {
							return -1;
						}
					}
					else {
						write_size += SerializeSize(asset->reflection_manager, buffer.reflection_type, buffer.data.buffer, &serialize_options);
					}
					return 0;
				};

				bool has_reflection_type = buffer.reflection_type != nullptr;
				// Write a boolean indicating
				write_size += Write(data->stream, &has_reflection_type, sizeof(has_reflection_type), data->write_data);
				if (has_reflection_type) {
					if (!buffer.dynamic) {
						ECS_ASSERT(buffer.data.buffer != nullptr);
						if (serialize() == -1) {
							return -1;
						}
					}
					else {
						bool has_data = buffer.data.buffer != nullptr;
						write_size += Write(data->stream, &has_data, sizeof(has_data), data->write_data);
						if (has_data) {
							if (serialize() == -1) {
								return -1;
							}
						}
					}
				}
				else {
					bool has_data = buffer.data.buffer != nullptr;
					// Write a bool indicating whether or not it has data
					write_size += Write(data->stream, &has_data, sizeof(has_data), data->write_data);
					if (has_data) {
						write_size += WriteWithSizeShort(data->stream, buffer.data, data->write_data);
					}
				}
				return 0;
			};

			for (size_t index = 0; index < asset->buffers[type].size; index++) {
				MaterialAssetBuffer buffer = asset->buffers[type][index];
				write_size += Write(data->stream, &buffer.slot, sizeof(buffer.slot), data->write_data);
				write_size += Write(data->stream, &buffer.dynamic, sizeof(buffer.dynamic), data->write_data);
				if (buffer.reflection_type != nullptr) {
					write_size += WriteWithSizeShort(data->stream, buffer.reflection_type->name, data->write_data);
				}
				else {
					// This is the name in the file as variable (i.e. cbuffer my_buf -> my_buf)
					write_size += WriteWithSizeShort(data->stream, buffer.name, data->write_data);
				}
				if (write_buffer_data(index) == -1) {
					return -1;
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
		bool do_not_increment_dependencies = ECS_SERIALIZE_CUSTOM_TYPES[Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET].switches[ECS_ASSET_MATERIAL_SERIALIZE_DO_NOT_INCREMENT_DEPENDENCIES];
		if (data->read_data) {
			ECS_ASSERT(user_data != nullptr);
		}

		AssetDatabase* database = (AssetDatabase*)user_data;
		MaterialAsset* asset = (MaterialAsset*)data->data;

		size_t read_size = 0;
		AllocatorPolymorphic allocator = data->options->field_allocator;

		if (data->read_data && (asset->reflection_manager == nullptr || data->was_allocated)) {
			asset->reflection_manager = (Reflection::ReflectionManager*)Allocate(allocator, sizeof(Reflection::ReflectionManager));
			*asset->reflection_manager = Reflection::ReflectionManager(allocator, 0, 0);
		}

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
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
		AllocatorPolymorphic temp_allocator = &stack_allocator;

		if (data->read_data) {
			unsigned int int_counts[ECS_MATERIAL_SHADER_COUNT * 3];
			for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT * 3; index++) {
				int_counts[index] = counts[index];
			}

			// Allocate the buffers
			Reflection::ReflectionManager* reflection_manager = asset->reflection_manager;
			memset(asset, 0, sizeof(*asset));
			asset->reflection_manager = reflection_manager;
			asset->Resize(int_counts, allocator, true);
			asset->name = asset_name;
		}

		Read(data->stream, &asset->vertex_shader_handle, sizeof(asset->vertex_shader_handle), data->read_data);
		Read(data->stream, &asset->pixel_shader_handle, sizeof(asset->pixel_shader_handle), data->read_data);

		for (size_t type = 0; type < ECS_MATERIAL_SHADER_COUNT; type++) {
			// Read the names now. Then use the database to get the handles for those resources
			for (unsigned short index = 0; index < asset->textures[type].size; index++) {
				unsigned char slot = 0;
				Read(data->stream, &slot, sizeof(slot), data->read_data);

				if (data->read_data) {
					unsigned short name_size = 0;
					read_size += ReadWithSizeShort<true>(data->stream, name_buffer.buffer, name_size);
					name_buffer.size = name_size / sizeof(char);

					unsigned short file_size = 0;
					read_size += ReadWithSizeShort<true>(data->stream, file_buffer.buffer, file_size);
					file_buffer.size = file_size / sizeof(wchar_t);

					unsigned int handle = -1;
					if (name_buffer.size > 0 && file_buffer.size > 0) {
						if (do_not_increment_dependencies) {
							handle = database->FindTexture(name_buffer, file_buffer);
						}
						else {
							handle = database->AddTexture(name_buffer, file_buffer);
						}
					}
					asset->textures[type][index].metadata_handle = handle;
					asset->textures[type][index].slot = slot;
					asset->textures[type][index].name = ReadAllocateDataShort<true>(data->stream, allocator).As<char>();
				}
				else {
					// Ignoring the name and the file of the target texture
					IgnoreWithSizeShort(data->stream);
					IgnoreWithSizeShort(data->stream);
					// Ignoring the source code name
					read_size += IgnoreWithSizeShort(data->stream);
				}
			}

			for (unsigned short index = 0; index < asset->samplers[type].size; index++) {
				unsigned char slot = 0;
				Read(data->stream, &slot, sizeof(slot), data->read_data);

				if (data->read_data) {
					unsigned short name_size = 0;
					read_size += ReadWithSizeShort<true>(data->stream, name_buffer.buffer, name_size);
					name_buffer.size = name_size / sizeof(char);

					unsigned int handle = -1;
					if (name_buffer.size > 0) {
						if (do_not_increment_dependencies) {
							handle = database->FindGPUSampler(name_buffer);
						}
						else {
							handle = database->AddGPUSampler(name_buffer);
						}
					}
					asset->samplers[type][index].metadata_handle = handle;
					asset->samplers[type][index].slot = slot;
					asset->samplers[type][index].name = ReadAllocateDataShort<true>(data->stream, allocator).As<char>();
				}
				else {
					// Ignoring the sampler metadata name
					IgnoreWithSizeShort(data->stream);
					// Ignoring the source code name
					read_size += IgnoreWithSizeShort(data->stream);
				}
			}

			for (unsigned short index = 0; index < asset->buffers[type].size; index++) {
				unsigned char slot;
				bool dynamic;
				Read<true>(data->stream, &slot, sizeof(slot));
				Read<true>(data->stream, &dynamic, sizeof(dynamic));
				
				if (data->read_data) {
					// Read the name
					asset->buffers[type][index].name = ReadAllocateDataShort<true>(data->stream, allocator).As<char>();
					asset->buffers[type][index].dynamic = dynamic;
					asset->buffers[type][index].slot = slot;

					read_size += asset->buffers[type][index].name.size;
				}
				else {
					read_size += IgnoreWithSizeShort(data->stream);
				}

				// Read the bool indicating whether or not it has the reflection type
				bool has_reflection_type;
				Read<true>(data->stream, &has_reflection_type, sizeof(has_reflection_type));

				auto read_data_with_reflection_type = [&]() {
					if (data->read_data) {
						// Read the deserialize field table
						DeserializeFieldTableOptions field_options;
						field_options.read_type_tags = true;
						DeserializeFieldTable field_table = DeserializeFieldTableFromData(*data->stream, temp_allocator, &field_options);
						void* allocation = nullptr;
						size_t byte_size = ECS_KB;
						Reflection::ReflectionType* type_to_be_deserialized = nullptr;

						if (field_table.types.size == 0) {
							// The deserialize field table is invalid
							allocation = Allocate(allocator, byte_size);
							memset(allocation, 0, byte_size);
						}
						else {
							Reflection::ReflectionManager* reflection_manager = asset->reflection_manager;
							field_table.ToNormalReflection(reflection_manager, allocator, true, true);

							type_to_be_deserialized = reflection_manager->GetType(field_table.types[0].name);
							allocation = Allocate(allocator, type_to_be_deserialized->byte_size);
							byte_size = type_to_be_deserialized->byte_size;
							// Memset the allocation such that padding bytes are reset to 0
							memset(allocation, 0, byte_size);

							DeserializeOptions options;
							options.deserialized_field_manager = reflection_manager;
							options.read_type_table = false;
							options.field_allocator = allocator;
							options.backup_allocator = allocator;
							options.field_table = &field_table;
							ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, type_to_be_deserialized, allocation, *data->stream, &options);
							if (code != ECS_DESERIALIZE_OK) {
								// Just set the data to defaults
								reflection_manager->SetInstanceDefaultData(type_to_be_deserialized, allocation);
							}
						}

						asset->buffers[type][index].data = { allocation, byte_size };
						if (type_to_be_deserialized != nullptr) {
							asset->buffers[type][index].reflection_type = (Reflection::ReflectionType*)Allocate(allocator, sizeof(Reflection::ReflectionType));
							// Copy the type - we can store the type as is directly because in the hash table it
							// can change positions when inserting
							*asset->buffers[type][index].reflection_type = *type_to_be_deserialized;
						}
					}
					else {
						// Ignore the serialize field
						IgnoreDeserialize(*data->stream);
					}
				};

				auto read_data_without_reflection_type = [&]() {
					if (data->read_data) {
						asset->buffers[type][index].data = ReadAllocateDataShort<true>(data->stream, allocator);
						asset->buffers[type][index].reflection_type = nullptr;

						read_size += asset->buffers[type][index].data.size;
					}
					else {
						read_size += IgnoreWithSizeShort(data->stream);
					}
				};

				bool has_data;
				if (has_reflection_type) {
					if (!dynamic) {
						read_data_with_reflection_type();
					}
					else {
						Read<true>(data->stream, &has_data, sizeof(has_data));
						if (has_data) {
							read_data_with_reflection_type();
						}
					}
				}
				else {
					if (!dynamic) {
						read_data_without_reflection_type();
					}
					else {
						Read<true>(data->stream, &has_data, sizeof(has_data));
						if (has_data) {
							read_data_without_reflection_type();
						}
					}
				}
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
				if (do_not_increment_dependencies) {
					vertex_handle = database->FindShader(name_buffer, file_buffer);
				}
				else {
					vertex_handle = database->AddShader(name_buffer, file_buffer);
				}
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
				if (do_not_increment_dependencies) {
					pixel_handle = database->FindShader(name_buffer, file_buffer);
				}
				else {
					pixel_handle = database->AddShader(name_buffer, file_buffer);
				}
			}
			asset->pixel_shader_handle = pixel_handle;
		}
		else {
			// The vertex shader name + file
			read_size += IgnoreWithSizeShort(data->stream);
			read_size += IgnoreWithSizeShort(data->stream);
			// The pixel shader name + file
			read_size += IgnoreWithSizeShort(data->stream);
			read_size += IgnoreWithSizeShort(data->stream);
		}

		return read_size;
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

	bool SetSerializeCustomMaterialDoNotIncrementDependencies(bool status)
	{
		return SetSerializeCustomTypeSwitch(Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET, ECS_ASSET_MATERIAL_SERIALIZE_DO_NOT_INCREMENT_DEPENDENCIES, status);
	}

	// --------------------------------------------------------------------------------------------

}