#include "ecspch.h"
#include "GLTFExportTextures.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Resources/ResourceTypes.h"
#include "../Resources/WriteTexture.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../Rendering/TextureOperations.h"
#include "../Utilities/Path.h"
#include "../Multithreading/TaskManager.h"
#include "../Containers/AtomicStream.h"
#include "../Utilities/StringUtilities.h"
#include "../Utilities/File.h"

#define DEFAULT_COMPRESSION_QUALITY 0.9f

namespace ECSEngine {
	
	static void GetExportTextureWriteName(Stream<wchar_t> directory, Stream<wchar_t> texture_filename, CapacityStream<wchar_t>& final_path) {
		final_path.CopyOther(directory);
		final_path.Add(ECS_OS_PATH_SEPARATOR);
		final_path.AddStreamAssert(texture_filename);
		// It doesn't have an extension, add it now
		bool add_extension = true;
		Stream<wchar_t> extension = PathExtension(final_path);
		if (extension.size > 0) {
			if (extension == ECS_TEXTURE_EXTENSIONS[ECS_TEXTURE_EXTENSION_JPG]) {
				add_extension = false;
			}
			else {
				final_path = final_path.StartDifference(extension);
			}
		}
		if (add_extension) {
			final_path.AddStreamAssert(ECS_TEXTURE_EXTENSIONS[ECS_TEXTURE_EXTENSION_JPG]);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void GLTFExportTexturesOptionsAllocateAll(GLTFExportTexturesOptions* options, size_t failure_string_capacity, AllocatorPolymorphic allocator)
	{
		size_t allocation_size = sizeof(AtomicStream<char>) + sizeof(Semaphore) + sizeof(char) * failure_string_capacity;
		void* allocation = Allocate(allocator, allocation_size);
		uintptr_t allocation_ptr = (uintptr_t)allocation;

		AtomicStream<char>* write_error_message = (AtomicStream<char>*)allocation_ptr;
		allocation_ptr += sizeof(AtomicStream<char>);
		write_error_message->InitializeFromBuffer(allocation_ptr, 0, failure_string_capacity);
		Semaphore* finish_semaphore = (Semaphore*)allocation_ptr;
		finish_semaphore->ClearCount();

		options->failure_string = write_error_message;
		options->semaphore = finish_semaphore;
	}

	// ------------------------------------------------------------------------------------------------------------

	void GLTFExportTexturesOptionsDeallocateAll(const GLTFExportTexturesOptions* options, AllocatorPolymorphic allocator) 
	{
		Deallocate(allocator, options->failure_string);
	}

	// ------------------------------------------------------------------------------------------------------------

	void GLTFGetExportTextures(GLTFData gltf_data, CapacityStream<GLTFExportTexture>* export_textures, AllocatorPolymorphic allocator, bool unique_material_entries)
	{
		// Keep track of all materials found up until now for unique entries
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
		ResizableStream<Stream<char>> tracked_materials;
		tracked_materials.Initialize(&stack_allocator, 0);

		internal::ForEachMeshInGLTF(gltf_data, [&](const cgltf_node* nodes, size_t node_index, size_t node_count) {
			PBRMaterial placeholder;
			internal::ForMaterialFromGLTF(nodes, node_index, placeholder, 
				[&](Stream<char> material_name, size_t mapping_count, PBRMaterialMapping* mappings, Stream<void>* texture_data, TextureExtension* texture_extensions) {
					auto add_entries = [&]() {
						// Occlusion - Roughness - Metallic texture
						// Cache this since it is the same and we shouldn't decode it multiple times
						DecodedTexture orm_texture;

						for (size_t index = 0; index < mapping_count; index++) {
							GLTFExportTexture export_texture;
							export_texture.texture_filename = mappings[index].texture.Copy(allocator);
							export_texture.texture_index = mappings[index].index;

							size_t channel_count = 4;
							DecodedTexture current_decoded_texture;
							if (export_texture.texture_index == ECS_PBR_MATERIAL_METALLIC || export_texture.texture_index == ECS_PBR_MATERIAL_ROUGHNESS
								|| export_texture.texture_index == ECS_PBR_MATERIAL_OCCLUSION) {
								if (orm_texture.data.size == 0) {
									orm_texture = DecodeTexture(texture_data[index], texture_extensions[index], ECS_MALLOC_ALLOCATOR);
								}

								size_t channel_to_copy = 0;
								if (export_texture.texture_index == ECS_PBR_MATERIAL_METALLIC) {
									channel_to_copy = 2;
								}
								else if (export_texture.texture_index == ECS_PBR_MATERIAL_ROUGHNESS) {
									channel_to_copy = 1;
								}

								// Convert the 4 channel texture into single channel
								Stream<void> orm_texture_data = orm_texture.data;
								ConvertTextureToSingleChannel(
									{ &orm_texture_data, 1 }, 
									orm_texture.width, 
									orm_texture.height, 
									4, 
									channel_to_copy
								);
								current_decoded_texture = orm_texture;
								current_decoded_texture.data = orm_texture_data;
								channel_count = 1;
							}
							else {
								current_decoded_texture = DecodeTexture(texture_data[index], texture_extensions[index], ECS_MALLOC_ALLOCATOR);
							}

							export_texture.dimensions.x = current_decoded_texture.width;
							export_texture.dimensions.y = current_decoded_texture.height;
							export_texture.data = current_decoded_texture.data;
							export_texture.channel_count = channel_count;

							export_textures->AddAssert(export_texture);
						}

						// Deallocate the orm_texture data, if present
						if (orm_texture.data.size > 0) {
							Free(orm_texture.data.buffer);
						}
					};

					if (unique_material_entries) {
						bool is_tracked = FindString(material_name, tracked_materials.ToStream()) != -1;
						if (!is_tracked) {
							tracked_materials.Add(material_name.Copy(&stack_allocator));
							add_entries();
						}
					}
					else {
						add_entries();
					}
				});
			return true;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	void GLTFGetExportTexturesNames(GLTFData gltf_data, CapacityStream<PBRMaterialMapping>* mappings, AllocatorPolymorphic allocator, bool unique_material_entries)
	{
		// Keep track of all materials found up until now for unique entries
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
		ResizableStream<Stream<char>> tracked_materials;
		tracked_materials.Initialize(&stack_allocator, 0);

		internal::ForEachMeshInGLTF(gltf_data, [&](const cgltf_node* nodes, size_t node_index, size_t node_count) {
			PBRMaterial placeholder;
			internal::ForMaterialFromGLTF(nodes, node_index, placeholder,
				[&](Stream<char> material_name, size_t mapping_count, PBRMaterialMapping* current_mappings, Stream<void>* texture_data, TextureExtension* texture_extensions) {
					auto add_entries = [&]() {
						for (size_t index = 0; index < mapping_count; index++) {
							PBRMaterialMapping current_mapping = current_mappings[index];
							current_mapping.texture.InitializeAndCopy(allocator, current_mapping.texture);
							mappings->AddAssert(current_mapping);
						}
					};

					if (unique_material_entries) {
						bool is_tracked = FindString(material_name, tracked_materials.ToStream()) != -1;
						if (!is_tracked) {
							tracked_materials.Add(material_name.Copy(&stack_allocator));
							add_entries();
						}
					}
					else {
						add_entries();
					}
				});
			return true;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	bool GLTFWriteExportTextures(Stream<wchar_t> directory, Stream<GLTFExportTexture> export_textures, CapacityStream<unsigned int>* failed_textures)
	{
		bool success = true;

		// All textures are written as .jpg
		for (size_t index = 0; index < export_textures.size; index++) {
			ECS_STACK_CAPACITY_STREAM(wchar_t, texture_path, 512);
			GetExportTextureWriteName(directory, export_textures[index].texture_filename, texture_path);

			WriteJPGTextureOptions options;
			options.srgb = export_textures[index].texture_index == ECS_PBR_MATERIAL_COLOR;
			options.compression_quality = DEFAULT_COMPRESSION_QUALITY;

			bool current_success = WriteJPGTexture(
				texture_path,
				export_textures[index].data.buffer, 
				export_textures[index].dimensions, 
				export_textures[index].channel_count, 
				options
			);
			if (!current_success && failed_textures != nullptr) {
				failed_textures->AddAssert(index);
			}
			success &= current_success;
		}

		return success;
	}

	// ------------------------------------------------------------------------------------------------------------

	void GLTFDeallocateExportTextureNames(Stream<PBRMaterialMapping> mappings, AllocatorPolymorphic allocator)
	{
		for (size_t index = 0; index < mappings.size; index++) {
			mappings[index].texture.Deallocate(allocator);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void GLTFDeallocateExportTextures(Stream<GLTFExportTexture> export_textures, AllocatorPolymorphic allocator)
	{
		for (size_t index = 0; index < export_textures.size; index++) {
			export_textures[index].Deallocate(allocator);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	struct ExportOrmTextureData {
		GLTFExportTexturesOptions options;
		Stream<void> texture_data;
		TextureExtension texture_extension;
		Stream<wchar_t> directory;

		Stream<wchar_t> occlusion_filename;
		Stream<wchar_t> metallic_filename;
		Stream<wchar_t> roughness_filename;
	};

	ECS_THREAD_TASK(ExportOrmTexture) {
		ExportOrmTextureData* data = (ExportOrmTextureData*)_data;

		DecodedTexture decoded_texture = DecodeTexture(data->texture_data, data->texture_extension, ECS_MALLOC_ALLOCATOR);

		WriteJPGTextureOptions write_options;
		write_options.compression_quality = DEFAULT_COMPRESSION_QUALITY;
		Stream<Stream<void>> input_data = { &decoded_texture.data, 1 };
		ECS_STACK_CAPACITY_STREAM(wchar_t, texture_path, 512);

		auto write_texture = [&](Stream<wchar_t> texture_filename, size_t channel_index) {
			if (texture_filename.size > 0) {
				Stream<Stream<void>> single_channel_data = ConvertTextureToSingleChannel(input_data, decoded_texture.width, decoded_texture.height, 4, channel_index);
				GetExportTextureWriteName(data->directory, texture_filename, texture_path);
				bool success = WriteJPGTexture(texture_path, single_channel_data[0].buffer, { decoded_texture.width, decoded_texture.height }, 1, write_options);
				if (!success) {
					if (data->options.failure_string != nullptr) {
						ECS_FORMAT_TEMP_STRING(message, "Failed to write texture {#}\n", texture_path);
						data->options.failure_string->AddStream(message);
					}
				}

				Free(single_channel_data.buffer);
				// Deallocate the texture name as well
				Free(texture_filename.buffer);
			}
		};

		write_texture(data->occlusion_filename, 0);
		write_texture(data->roughness_filename, 1);
		write_texture(data->metallic_filename, 2);

		Free(decoded_texture.data.buffer);

		unsigned int exit_count = data->options.semaphore->ExitEx();
		if (exit_count == 0) {
			// Deallocate the directory
			Free(data->directory.buffer);
		}
	}

	struct ExportEncodedTextureData {
		GLTFExportTexturesOptions options;
		Stream<void> encoded_data;
		Stream<wchar_t> texture_filename;
		Stream<wchar_t> directory;
	};

	ECS_THREAD_TASK(ExportEncodedTexture) {
		ExportEncodedTextureData* data = (ExportEncodedTextureData*)_data;

		// We simply have to write the binary data directly into a file
		ECS_STACK_CAPACITY_STREAM(wchar_t, texture_path, 512);
		GetExportTextureWriteName(data->directory, data->texture_filename, texture_path);
		bool success = WriteBufferToFileBinary(texture_path, data->encoded_data) == ECS_FILE_STATUS_OK;
		if (!success) {
			if (data->options.failure_string != nullptr) {
				ECS_FORMAT_TEMP_STRING(message, "Failed to write texture {#}\n", texture_path);
				data->options.failure_string->AddStream(message);
			}
		}

		// Deallocate the texture filename - the encoded data comes from GLTF, we shouldn't free that
		Free(data->texture_filename.buffer);
		unsigned int exit_count = data->options.semaphore->ExitEx();
		if (exit_count == 0) {
			// Deallocate the directory
			Free(data->directory.buffer);
		}
	}

	bool GLTFExportTexturesDirectly(GLTFData gltf_data, Stream<wchar_t> directory, GLTFExportTexturesOptions* options)
	{
		// Keep track of all materials found up until now for unique entries
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
		ResizableStream<Stream<char>> tracked_materials;
		tracked_materials.Initialize(&stack_allocator, 0);

		wchar_t* stable_directory = (wchar_t*)Malloc(directory.MemoryOf(directory.size));
		directory.CopyTo(stable_directory);
		directory.buffer = stable_directory;

		// Increase the semaphore once such that we make sure that we launch all tasks correctly
		// without having one finish before we launch them all and incorrectly report to the user that
		// We have finished
		options->semaphore->Enter();

		Stream<wchar_t> standard_texture_names[ECS_PBR_MATERIAL_MAPPING_COUNT] = {
			L"diffuse",
			L"normal",
			L"metallic",
			L"roughness",
			L"occlusion",
			L"emissive"
		};

		bool at_least_one_export = false;
		internal::ForEachMeshInGLTF(gltf_data, [&](const cgltf_node* nodes, size_t node_index, size_t node_count) {
			PBRMaterial placeholder;
			internal::ForMaterialFromGLTF(nodes, node_index, placeholder,
				[&](Stream<char> material_name, size_t mapping_count, PBRMaterialMapping* mappings, Stream<void>* texture_data, TextureExtension* texture_extensions) {
					auto add_entries = [&]() {
						Stream<wchar_t> occlusion_name;
						Stream<wchar_t> roughness_name;
						Stream<wchar_t> metallic_name;
						TextureExtension orm_extension;
						Stream<void> orm_data;

						Stream<wchar_t> prefix;
						if (options->use_standard_texture_names) {
							if (mapping_count > 1) {
								ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, texture_names, ECS_PBR_MATERIAL_MAPPING_COUNT);
								for (size_t index = 0; index < mapping_count; index++) {
									texture_names[index] = mappings[index].texture;
								}
								texture_names.size = mapping_count;
								prefix = StringsPrefix(texture_names);
							}
							else {
								// Check for an underscore when there is a single texture
								Stream<wchar_t> underscore = FindFirstCharacter(mappings[0].texture, L'_');
								if (underscore.size > 0) {
									// Advance such that we get the underscore in the prefix
									underscore.Advance();
									prefix = mappings[0].texture.StartDifference(underscore);
								}
							}
						}

						for (size_t index = 0; index < mapping_count; index++) {
							DecodedTexture current_decoded_texture;
							PBRMaterialTextureIndex pbr_index = mappings[index].index;

							// Verify that the texture is matched in the options
							if (options->textures_to_write.size > 0) {
								unsigned int found_index = options->textures_to_write.Find(mappings[index].texture, [](PBRMaterialMapping mapping) {
									return mapping.texture;
								});
								if (found_index == -1) {
									// Skip the entry
									continue;
								}
							}

							Stream<wchar_t> texture_name = mappings[index].texture;
							// Allocate the string large enough such that we cover the use standard name case
							wchar_t* stable_texture_name = (wchar_t*)Malloc(texture_name.MemoryOf(texture_name.size + standard_texture_names[pbr_index].size));
							if (options->use_standard_texture_names) {
								if (prefix.size > 0) {
									prefix.CopyTo(stable_texture_name);
									standard_texture_names[pbr_index].CopyTo(stable_texture_name + prefix.size);
									texture_name.size = prefix.size + standard_texture_names[pbr_index].size;
								}
								else {
									texture_name.CopyTo(stable_texture_name);
								}
							}
							else {
								texture_name.CopyTo(stable_texture_name);
							}
							texture_name.buffer = stable_texture_name;

							if (pbr_index == ECS_PBR_MATERIAL_METALLIC) {
								metallic_name = texture_name;
								orm_extension = texture_extensions[index];
								orm_data = texture_data[index];
							}
							else if (pbr_index == ECS_PBR_MATERIAL_ROUGHNESS) {
								roughness_name = texture_name;
								orm_extension = texture_extensions[index];
								orm_data = texture_data[index];
							}
							else if (pbr_index == ECS_PBR_MATERIAL_OCCLUSION) {
								occlusion_name = texture_name;
								orm_extension = texture_extensions[index];
								orm_data = texture_data[index];
							}
							else {
								// Color, emissive or normal texture
								ExportEncodedTextureData thread_data;
								thread_data.directory = directory;
								thread_data.encoded_data = texture_data[index];
								thread_data.options = *options;
								thread_data.texture_filename = texture_name;

								// Increase the semaphore count
								options->semaphore->Enter();
								options->task_manager->AddDynamicTaskAndWake(ECS_THREAD_TASK_NAME(ExportEncodedTexture, &thread_data, sizeof(thread_data)));
							}
							at_least_one_export = true;
						}

						// Check to see if we need an ORM thread task as well
						if (occlusion_name.size > 0 || roughness_name.size > 0 || metallic_name.size > 0) {
							ExportOrmTextureData thread_data;
							thread_data.directory = directory;
							thread_data.options = *options;
							thread_data.texture_data = orm_data;
							thread_data.texture_extension = orm_extension;

							thread_data.metallic_filename = metallic_name;
							thread_data.roughness_filename = roughness_name;
							thread_data.occlusion_filename = occlusion_name;

							// Increase the semaphore count
							options->semaphore->Enter();
							options->task_manager->AddDynamicTaskAndWake(ECS_THREAD_TASK_NAME(ExportOrmTexture, &thread_data, sizeof(thread_data)));
						}
					};

					bool is_tracked = FindString(material_name, tracked_materials.ToStream()) != -1;
					if (!is_tracked) {
						tracked_materials.Add(material_name.Copy(&stack_allocator));
						add_entries();
					}
				});
			return true;
		});

		unsigned int exit_count = options->semaphore->ExitEx();
		if (exit_count == 0) {
			// Deallocate the directory
			Free(directory.buffer);
		}
		
		return at_least_one_export;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool GLTFExportTexturesToFolderSearch(
		Stream<wchar_t> current_directory,
		Stream<wchar_t> root_stop,
		CapacityStream<wchar_t>& write_path
	) {
		Stream<wchar_t> SEARCH_FOLDER_NAMES[] = {
			L"Textures",
			L"Texture",
			L"Material",
			L"Materials"
		};
		write_path.CopyOther(current_directory);

		while (write_path.size >= root_stop.size) {
			unsigned int initial_size = write_path.size;
			for (size_t index = 0; index < ECS_COUNTOF(SEARCH_FOLDER_NAMES); index++) {
				write_path.Add(ECS_OS_PATH_SEPARATOR);
				write_path.AddStreamAssert(SEARCH_FOLDER_NAMES[index]);
				if (ExistsFileOrFolder(write_path)) {
					return true;
				}
				write_path.size = initial_size;
			}
			write_path.size = PathParentBoth(write_path).size;
		}

		write_path.CopyOther(current_directory);
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------

}