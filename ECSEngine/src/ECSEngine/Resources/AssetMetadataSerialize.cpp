#include "ecspch.h"
#include "AssetMetadata.h"
#include "AssetMetadataSerialize.h"
#include "AssetDatabase.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Utilities/Reflection/ReflectionCustomTypes.h"
#include "../Utilities/Serialization/SerializeIntVariableLength.h"
#include "../Utilities/ReaderWriterInterface.h"

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

	void MaterialAssetCustomTypeInterface::GetDependencies(Reflection::ReflectionCustomTypeDependenciesData* data) {}

	bool MaterialAssetCustomTypeInterface::IsBlittable(Reflection::ReflectionCustomTypeIsBlittableData* data)
	{
		return false;
	}

	void MaterialAssetCustomTypeInterface::Copy(Reflection::ReflectionCustomTypeCopyData* data)
	{
		const MaterialAsset* source = (const MaterialAsset*)data->source;
		MaterialAsset* destination = (MaterialAsset*)data->destination;
		if (data->options.deallocate_existing_data) {
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

		bool success = true;
		WriteInstrument* write_instrument = data->write_instrument;
		
		const AssetDatabase* database = (const AssetDatabase*)user_data;
		const MaterialAsset* asset = (const MaterialAsset*)data->data;

		bool has_reflection_buffers = false;
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			for (size_t subindex = 0; subindex < asset->buffers[index].size && !has_reflection_buffers; subindex++) {
				has_reflection_buffers |= asset->buffers[index][subindex].reflection_type != nullptr;
			}
		}

		ECS_ASSERT(!has_reflection_buffers || asset->reflection_manager != nullptr);

		auto write_size_for_stream = [write_instrument, &success, data](auto stream) {
			for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
				success &= SerializeIntVariableLengthBool(write_instrument, stream[index].size);
			}
		};

		success &= write_instrument->WriteWithSizeVariableLength(asset->name);
		write_size_for_stream(asset->textures);
		write_size_for_stream(asset->samplers);
		write_size_for_stream(asset->buffers);
		success &= write_instrument->Write(&asset->vertex_shader_handle);
		success &= write_instrument->Write(&asset->pixel_shader_handle);
		if (!success) {
			return false;
		}

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

				success &= write_instrument->Write(&texture.slot);

				// This is the name of the assigned texture
				success &= write_instrument->WriteWithSizeVariableLength(current_name);
				success &= write_instrument->WriteWithSizeVariableLength(file);

				// This is the name in the file as variable (i.e. Texture2D my_texture -> my_texture; it is not
				// the name of the texture assigned through the UI). This is written last because it helps on deserialization
				success &= write_instrument->WriteWithSizeVariableLength(texture.name);
			}

			// Get the names of the resources being referenced and serialize these as well
			for (size_t index = 0; index < asset->samplers[type].size; index++) {
				MaterialAssetResource sampler = asset->samplers[type][index];
				Stream<char> current_name = { nullptr, 0 };
				if (sampler.metadata_handle != -1) {
					const auto* sampler_metadata = database->GetGPUSamplerConst(sampler.metadata_handle);
					current_name = sampler_metadata->name;
				}

				success &= write_instrument->Write(&sampler.slot);
				success &= write_instrument->WriteWithSizeVariableLength(current_name);

				// This is the name in the file as variable (i.e. SamplerState my_sampler -> my_sampler; it is not
				// the name of the sampler assigned through the UI). This is written last because it helps on deserialization
				success &= write_instrument->WriteWithSizeVariableLength(sampler.name);
			}

			// Returns -1 if it fails, else 0
			auto write_buffer_data = [&](size_t index) {
				MaterialAssetBuffer buffer = asset->buffers[type][index];
				SerializeOptions serialize_options;
				serialize_options.write_type_table_tags = true;
				
				//auto serialize = [&]() {
				//	if (data->write_data) {
				//		ECS_SERIALIZE_CODE code = Serialize(asset->reflection_manager, buffer.reflection_type, buffer.data.buffer, *data->stream, &serialize_options);
				//		if (code != ECS_SERIALIZE_OK) {
				//			return -1;
				//		}
				//	}
				//	else {
				//		write_size += SerializeSize(asset->reflection_manager, buffer.reflection_type, buffer.data.buffer, &serialize_options);
				//	}
				//	return 0;
				//};

				bool has_reflection_type = buffer.reflection_type != nullptr;
				// Write a boolean indicating
				success &= write_instrument->Write(&has_reflection_type);
				if (has_reflection_type) {
					if (!buffer.dynamic) {
						ECS_ASSERT(buffer.data.buffer != nullptr);
						success &= Serialize(asset->reflection_manager, buffer.reflection_type, buffer.data.buffer, write_instrument, &serialize_options) == ECS_SERIALIZE_OK;
					}
					else {
						bool has_data = buffer.data.buffer != nullptr;
						success &= write_instrument->Write(&has_data);
						if (has_data) {
							success &= Serialize(asset->reflection_manager, buffer.reflection_type, buffer.data.buffer, write_instrument, &serialize_options) == ECS_SERIALIZE_OK;
						}
					}
				}
				else {
					bool has_data = buffer.data.buffer != nullptr;
					// Write a bool indicating whether or not it has data
					success &= write_instrument->Write(&has_data);
					if (has_data) {
						success &= write_instrument->WriteWithSizeVariableLength(buffer.data);
					}
				}
			};

			for (size_t index = 0; index < asset->buffers[type].size; index++) {
				MaterialAssetBuffer buffer = asset->buffers[type][index];
				success &= write_instrument->Write(&buffer.slot);
				success &= write_instrument->Write(&buffer.dynamic);
				if (buffer.reflection_type != nullptr) {
					success &= write_instrument->WriteWithSizeVariableLength(buffer.reflection_type->name);
				}
				else {
					// This is the name in the file as variable (i.e. cbuffer my_buf -> my_buf)
					success &= write_instrument->WriteWithSizeVariableLength(buffer.name);
				}
				write_buffer_data(index);
			}

			// Perform an intermediary check after some handful operations were done
			if (!success) {
				return false;
			}
		}
		
		Stream<char> vertex_name = { nullptr, 0 };
		Stream<wchar_t> vertex_file = { nullptr, 0 };
		if (asset->vertex_shader_handle != -1) {
			const auto* vertex_shader = database->GetShaderConst(asset->vertex_shader_handle);
			vertex_name = vertex_shader->name;
			vertex_file = vertex_shader->file;
		}
		success &= write_instrument->WriteWithSizeVariableLength(vertex_name);
		success &= write_instrument->WriteWithSizeVariableLength(vertex_file);

		Stream<char> pixel_name = { nullptr, 0 };
		Stream<wchar_t> pixel_file = { nullptr, 0 };
		if (asset->pixel_shader_handle != -1) {
			const auto* pixel_shader = database->GetShaderConst(asset->pixel_shader_handle);
			pixel_name = pixel_shader->name;
			pixel_file = pixel_shader->file;
		}
		success &= write_instrument->WriteWithSizeVariableLength(pixel_name);
		success &= write_instrument->WriteWithSizeVariableLength(pixel_file);

		return success;
	}

	// --------------------------------------------------------------------------------------------

	ECS_SERIALIZE_CUSTOM_TYPE_READ_FUNCTION(MaterialAsset) {
		if (data->custom_types_version[ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET] != ECS_SERIALIZE_CUSTOM_TYPE_MATERIAL_ASSET_VERSION) {
			return false;
		}

		bool success = true;
		ReadInstrument* read_instrument = data->read_instrument;
		bool read_data = !data->ShouldIgnoreData();

		void* user_data = ECS_SERIALIZE_CUSTOM_TYPES[Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET].user_data;
		bool do_not_increment_dependencies = ECS_SERIALIZE_CUSTOM_TYPES[Reflection::ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET].switches[ECS_ASSET_MATERIAL_SERIALIZE_DO_NOT_INCREMENT_DEPENDENCIES];
		if (read_data) {
			ECS_ASSERT(user_data != nullptr);
		}

		AssetDatabase* database = (AssetDatabase*)user_data;
		MaterialAsset* asset = (MaterialAsset*)data->data;

		AllocatorPolymorphic allocator = data->options->field_allocator;

		if (read_data && (asset->reflection_manager == nullptr || data->was_allocated)) {
			asset->reflection_manager = (Reflection::ReflectionManager*)Allocate(allocator, sizeof(Reflection::ReflectionManager));
			*asset->reflection_manager = Reflection::ReflectionManager(allocator, 0, 0);
		}

		Stream<char> asset_name = { nullptr, 0 };
		unsigned int stream_integer_counts[ECS_MATERIAL_SHADER_COUNT * 3];
		if (read_data) {
			success &= read_instrument->ReadWithSizeVariableLength(asset_name, allocator);
			// Helper function that reads the stream sizes for the
			for (size_t index = 0; index < ECS_COUNTOF(stream_integer_counts); index++) {
				success &= DeserializeIntVariableLengthBool(read_instrument, stream_integer_counts[index]);
			}
		}
		else {
			success &= read_instrument->IgnoreWithSizeVariableLength<char>();
			for (size_t index = 0; index < ECS_COUNTOF(stream_integer_counts); index++) {
				success &= IgnoreUnsignedIntVariableLengthBool(read_instrument);
			}
		}

		// Early exit after reading the counts if those could not be determined
		if (!success) {
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(char, name_buffer, 256);
		ECS_STACK_CAPACITY_STREAM(wchar_t, file_buffer, 256);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
		AllocatorPolymorphic temp_allocator = &stack_allocator;

		if (read_data) {
			// Allocate the buffers
			Reflection::ReflectionManager* reflection_manager = asset->reflection_manager;
			memset(asset, 0, sizeof(*asset));
			asset->reflection_manager = reflection_manager;
			asset->Resize(stream_integer_counts, allocator, true);
			asset->name = asset_name;
		}

		success &= read_instrument->Read(&asset->vertex_shader_handle);
		success &= read_instrument->Read(&asset->pixel_shader_handle);

		for (size_t type = 0; type < ECS_MATERIAL_SHADER_COUNT; type++) {
			// Read the names now. Then use the database to get the handles for those resources
			for (unsigned short index = 0; index < asset->textures[type].size; index++) {
				unsigned char slot = 0;
				success &= read_instrument->Read(&slot);

				if (read_data) {
					name_buffer.size = 0;
					file_buffer.size = 0;
					success &= read_instrument->ReadWithSizeVariableLength(name_buffer);
					success &= read_instrument->ReadWithSizeVariableLength(file_buffer);
					if (!success) {
						return false;
					}

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
					success &= read_instrument->ReadWithSizeVariableLength(asset->textures[type][index].name, allocator);
				}
				else {
					// Ignoring the name and the file of the target texture
					success &= read_instrument->IgnoreWithSizeVariableLength(name_buffer.ToStream());
					success &= read_instrument->IgnoreWithSizeVariableLength(file_buffer.ToStream());
					// Ignoring the source code name
					success &= read_instrument->IgnoreWithSizeVariableLength(asset->textures[type][index].name);
				}
			}

			for (unsigned short index = 0; index < asset->samplers[type].size; index++) {
				unsigned char slot = 0;
				success &= read_instrument->Read(&slot);

				if (read_data) {
					name_buffer.size = 0;
					success &= read_instrument->ReadWithSizeVariableLength(name_buffer);
					if (!success) {
						return false;
					}

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
					success &= read_instrument->ReadWithSizeVariableLength(asset->samplers[type][index].name, allocator);
				}
				else {
					// Ignoring the sampler metadata name
					success &= read_instrument->IgnoreWithSizeVariableLength(name_buffer.ToStream());
					// Ignoring the source code name
					success &= read_instrument->IgnoreWithSizeVariableLength(asset->samplers[type][index].name);
				}
			}

			for (unsigned short index = 0; index < asset->buffers[type].size; index++) {
				unsigned char slot;
				bool dynamic;
				success &= read_instrument->Read(&slot);
				success &= read_instrument->Read(&dynamic);
				
				if (read_data) {
					// Read the name
					success &= read_instrument->ReadWithSizeVariableLength(asset->buffers[type][index].name, allocator);
					asset->buffers[type][index].dynamic = dynamic;
					asset->buffers[type][index].slot = slot;
				}
				else {
					success &= read_instrument->IgnoreWithSizeVariableLength(asset->buffers[type][index].name);
				}

				// Read the bool indicating whether or not it has the reflection type
				bool has_reflection_type;
				success &= read_instrument->ReadAlways(&has_reflection_type);

				if (!success) {
					return false;
				}

				auto read_data_with_reflection_type = [&]() {
					if (read_data) {
						// Read the deserialize field table
						DeserializeFieldTableOptions field_options;
						field_options.read_type_tags = true;
						Optional<DeserializeFieldTable> field_table = DeserializeFieldTableFromData(read_instrument, temp_allocator, &field_options);
						void* allocation = nullptr;
						size_t byte_size = ECS_KB;
						Reflection::ReflectionType* type_to_be_deserialized = nullptr;

						if (!field_table.has_value) {
							// The deserialize field table is invalid
							allocation = Allocate(allocator, byte_size);
							memset(allocation, 0, byte_size);
						}
						else {
							Reflection::ReflectionManager* reflection_manager = asset->reflection_manager;
							field_table.value.ToNormalReflection(reflection_manager, allocator, true, true);

							type_to_be_deserialized = reflection_manager->GetType(field_table.value.types[0].name);
							allocation = Allocate(allocator, type_to_be_deserialized->byte_size);
							byte_size = type_to_be_deserialized->byte_size;
							// Memset the allocation such that padding bytes are reset to 0
							memset(allocation, 0, byte_size);

							DeserializeOptions options;
							options.deserialized_field_manager = reflection_manager;
							options.read_type_table = false;
							options.field_allocator = allocator;
							options.field_table = &field_table.value;
							ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, type_to_be_deserialized, allocation, read_instrument, &options);
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
						success &= IgnoreDeserialize(read_instrument);
					}
				};

				auto read_data_without_reflection_type = [&]() {
					if (read_data) {
						success &= read_instrument->ReadWithSizeVariableLength(asset->buffers[type][index].data, allocator);
						asset->buffers[type][index].reflection_type = nullptr;
					}
					else {
						success &= read_instrument->IgnoreWithSizeVariableLength(asset->buffers[type][index].data);
					}
				};

				bool has_data;
				if (has_reflection_type) {
					if (!dynamic) {
						read_data_with_reflection_type();
					}
					else {
						success &= read_instrument->ReadAlways(&has_data);
						if (!success) {
							return false;
						}
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
						success &= read_instrument->ReadAlways(&has_data);
						if (!success) {
							return false;
						}
						if (has_data) {
							read_data_without_reflection_type();
						}
					}
				}

				if (!success) {
					return false;
				}
			}
		}

		if (read_data) {
			name_buffer.size = 0;
			success &= read_instrument->ReadWithSizeVariableLength(name_buffer);

			file_buffer.size = 0;
			success &= read_instrument->ReadWithSizeVariableLength(file_buffer);

			if (!success) {
				return false;
			}

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

			name_buffer.size = 0;
			success &= read_instrument->ReadWithSizeVariableLength(name_buffer);

			file_buffer.size = 0;
			success &= read_instrument->ReadWithSizeVariableLength(file_buffer);

			if (!success) {
				return false;
			}

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
			success &= read_instrument->IgnoreWithSizeVariableLength(name_buffer.ToStream());
			success &= read_instrument->IgnoreWithSizeVariableLength(file_buffer.ToStream());
			// The pixel shader name + file
			success &= read_instrument->IgnoreWithSizeVariableLength(name_buffer.ToStream());
			success &= read_instrument->IgnoreWithSizeVariableLength(file_buffer.ToStream());
		}

		return success;
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