#include "ecspch.h"
#include "AssetMetadata.h"
#include "../Utilities/File.h"
#include "../Utilities/Path.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Utilities/StreamUtilities.h"

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------------------------------

	AssetFieldTarget ECS_ASSET_TARGET_FIELD_NAMES[] = {
		{ STRING(CoalescedMesh), ECS_ASSET_MESH },
		{ STRING(ResourceView), ECS_ASSET_TEXTURE },
		{ STRING(SamplerState), ECS_ASSET_GPU_SAMPLER },
		{ STRING(VertexShader), ECS_ASSET_SHADER, ECS_SHADER_VERTEX },
		{ STRING(PixelShader), ECS_ASSET_SHADER, ECS_SHADER_PIXEL },
		{ STRING(ComputeShader), ECS_ASSET_SHADER, ECS_SHADER_COMPUTE },
		{ STRING(Material), ECS_ASSET_MATERIAL },
		{ STRING(MiscAssetData), ECS_ASSET_MISC }
	};

	size_t ECS_ASSET_TARGET_FIELD_NAMES_SIZE()
	{
		return ECS_COUNTOF(ECS_ASSET_TARGET_FIELD_NAMES);
	}

	// ------------------------------------------------------------------------------------------------------

	bool AssetHasFile(ECS_ASSET_TYPE type)
	{
		return type == ECS_ASSET_MESH || type == ECS_ASSET_TEXTURE || type == ECS_ASSET_SHADER || type == ECS_ASSET_MISC;
	}

	// ------------------------------------------------------------------------------------------------------

	ResourceType AssetTypeToResourceType(ECS_ASSET_TYPE type)
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return ResourceType::CoalescedMesh;
		case ECS_ASSET_TEXTURE:
			return ResourceType::Texture;
		case ECS_ASSET_GPU_SAMPLER:
			return ResourceType::TypeCount;
		case ECS_ASSET_SHADER:
			return ResourceType::Shader;
		case ECS_ASSET_MATERIAL:
			return ResourceType::TypeCount;
		case ECS_ASSET_MISC:
			return ResourceType::Misc;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		return ResourceType::TypeCount;
	}

	// ------------------------------------------------------------------------------------------------------

	AssetTypeEx FindAssetTargetField(Stream<char> string)
	{
		size_t count = ECS_COUNTOF(ECS_ASSET_TARGET_FIELD_NAMES);
		for (size_t index = 0; index < count; index++) {
			if (string.size == ECS_ASSET_TARGET_FIELD_NAMES[index].name.size &&
				memcmp(string.buffer, ECS_ASSET_TARGET_FIELD_NAMES[index].name.buffer, ECS_ASSET_TARGET_FIELD_NAMES[index].name.size) == 0) {
				return ECS_ASSET_TARGET_FIELD_NAMES[index].type;
			}
		}
		return { ECS_ASSET_TYPE_COUNT };
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

	static_assert(ECS_COUNTOF(ECS_ASSET_TYPE_CONVERSION) == ECS_ASSET_TYPE_COUNT);

	const char* ConvertAssetTypeString(ECS_ASSET_TYPE type)
	{
		return ECS_ASSET_TYPE_CONVERSION[type];
	}
	
	// ------------------------------------------------------------------------------------------------------

	size_t AssetMetadataByteSize(ECS_ASSET_TYPE type)
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return sizeof(MeshMetadata);
		case ECS_ASSET_TEXTURE:
			return sizeof(TextureMetadata);
		case ECS_ASSET_GPU_SAMPLER:
			return sizeof(GPUSamplerMetadata);
		case ECS_ASSET_SHADER:
			return sizeof(ShaderMetadata);
		case ECS_ASSET_MATERIAL:
			return sizeof(MaterialAsset);
		case ECS_ASSET_MISC:
			return sizeof(MiscAsset);
		}
		ECS_ASSERT(false, "Incorrect asset type");

		// Shouldn't reach here
		return 0;
	}

	// ------------------------------------------------------------------------------------------------------

	size_t TargetAssetMetadataByteSize(ECS_ASSET_TYPE type)
	{
		switch (type) {
		case ECS_ASSET_MESH:
			return sizeof(CoalescedMesh);
		case ECS_ASSET_MATERIAL:
			return sizeof(Material);
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		case ECS_ASSET_MISC:
			return 0;
		default:
			ECS_ASSERT(false, "Incorrect asset type");
		}

		// Shouldn't reach here
		return 0;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAssetRelocate(
		MaterialAsset* material, 
		const unsigned int* texture_count,
		const unsigned int* sampler_count,
		const unsigned int* buffer_count,
		AllocatorPolymorphic allocator,
		bool do_not_copy
	) 
	{
		unsigned int total_texture_count = 0;
		unsigned int total_sampler_count = 0;
		unsigned int total_buffer_count = 0;
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			total_texture_count += texture_count[index];
			total_sampler_count += sampler_count[index];
			total_buffer_count += buffer_count[index];
		}

		// Allocate a new buffer
		size_t total_count = sizeof(MaterialAssetResource) * (total_texture_count + total_sampler_count) + sizeof(MaterialAssetBuffer) * total_buffer_count;
		void* allocation = Allocate(allocator, total_count);

		uintptr_t ptr = (uintptr_t)allocation;
		if (!do_not_copy) {
			// Copy into it
			for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
				CopyStreamAndMemset(ptr, sizeof(MaterialAssetResource) * texture_count[index], material->textures[index]);
			}
			for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
				CopyStreamAndMemset(ptr, sizeof(MaterialAssetResource) * sampler_count[index], material->samplers[index]);
			}
			for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
				CopyStreamAndMemset(ptr, sizeof(MaterialAssetBuffer) * buffer_count[index], material->buffers[index]);
			}
		}

		// Release the old buffer
		if (material->textures[0].buffer != nullptr) {
			DeallocateIfBelongs(allocator, material->textures[0].buffer);
		}

		ptr = (uintptr_t)allocation;
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			material->textures[index].InitializeFromBuffer(ptr, texture_count[index]);
		}
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			material->samplers[index].InitializeFromBuffer(ptr, sampler_count[index]);
		}
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			material->buffers[index].InitializeFromBuffer(ptr, buffer_count[index]);
		}
	}

	// ------------------------------------------------------------------------------------------------------

	// The functor takes as parameter the ECS_MATERIAL_SHADER to apply the functor to
	template<typename Functor>
	static void ForEachShaderType(ECS_MATERIAL_SHADER shader, Functor&& functor) {
		if (shader == ECS_MATERIAL_SHADER_COUNT) {
			for (size_t type = 0; type < ECS_MATERIAL_SHADER_COUNT; type++) {
				functor((ECS_MATERIAL_SHADER)type);
			}
		}
		else {
			functor(shader);
		}
	}

	// ------------------------------------------------------------------------------------------------------

	MaterialAsset::MaterialAsset(Stream<char> _name, AllocatorPolymorphic allocator)
	{
		name = StringCopy(allocator, _name);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::AddTexture(ECS_MATERIAL_SHADER shader, Stream<char> _name, unsigned int handle, unsigned char slot, AllocatorPolymorphic allocator)
	{
		unsigned int count = textures[shader].size;
		ResizeTexturesNewValue(count + 1, shader, allocator);
		textures[shader][count] = { _name.Copy(allocator), handle, slot };
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::AddSampler(ECS_MATERIAL_SHADER shader, Stream<char> _name, unsigned int handle, unsigned char slot, AllocatorPolymorphic allocator)
	{
		unsigned int count = samplers[shader].size;
		ResizeSamplersNewValue(count + 1, shader, allocator);
		samplers[shader][count] = { _name.Copy(allocator), handle, slot };
	}

	// ------------------------------------------------------------------------------------------------------

	CompareMaterialsResult MaterialAsset::Compare(const MaterialAsset* other) const
	{
		CompareMaterialsResult result;

		result.name_different = name != other->name;
		result.vertex_shader_different = vertex_shader_handle != other->vertex_shader_handle;
		result.pixel_shader_different = pixel_shader_handle != other->pixel_shader_handle;
		result.textures_different = false;
		result.samplers_different = false;
		result.buffers_different = false;

		for (size_t shader = 0; shader < ECS_MATERIAL_SHADER_COUNT; shader++) {
			ECS_MATERIAL_SHADER current_shader = (ECS_MATERIAL_SHADER)shader;
			result.buffers_different |= buffers[shader].size != other->buffers[shader].size;
			result.textures_different |= textures[shader].size != other->textures[shader].size;
			result.samplers_different |= samplers[shader].size != other->samplers[shader].size;

			if (!result.buffers_different) {
				if (reflection_manager != nullptr && other->reflection_manager != nullptr) {
					for (size_t index = 0; index < buffers[shader].size && !result.buffers_different; index++) {
						size_t buffer_index = other->FindBuffer(buffers[shader][index].name, buffers[shader][index].slot, current_shader);
						if (buffer_index == -1) {
							result.buffers_different = true;
						}
						else {
							if (buffers[shader][index].data.buffer == nullptr) {
								if (other->buffers[shader][buffer_index].data.buffer != nullptr) {
									result.buffers_different = true;
								}
							}
							else if (other->buffers[shader][buffer_index].data.buffer == nullptr) {
								result.buffers_different = true;
							}
							else {
								if (buffers[shader][index].reflection_type != nullptr && other->buffers[shader][buffer_index].reflection_type != nullptr) {
									bool same_reflection_type = Reflection::CompareReflectionTypes(
										reflection_manager,
										other->reflection_manager,
										buffers[shader][index].reflection_type,
										other->buffers[shader][buffer_index].reflection_type
									);
									if (same_reflection_type) {
										bool same_tags = Reflection::CompareReflectionTypesTags(
											buffers[shader][index].reflection_type,
											other->buffers[shader][buffer_index].reflection_type,
											true,
											true
										);
										if (same_tags) {
											// Compare the instance data
											bool same_instance_data = Reflection::CompareReflectionTypeInstances(
												reflection_manager,
												buffers[shader][index].reflection_type,
												buffers[shader][index].data.buffer,
												other->buffers[shader][buffer_index].data.buffer
											);
											if (!same_instance_data) {
												result.buffers_different = true;
											}
										}
										else {
											result.buffers_different = true;
										}
									}
									else {
										result.buffers_different = true;
									}
								}
								else {
									// Assume they are different if they don't have reflection types
									result.buffers_different = true;
								}
							}
						}
					}
				}
				else {
					// Assume they are different if they don't have reflection manager
					result.buffers_different = true;
				}
			}

			if (!result.textures_different) {
				for (size_t index = 0; index < textures[shader].size && !result.textures_different; index++) {
					size_t texture_index = other->FindTexture(textures[shader][index].name, textures[shader][index].slot, current_shader);
					if (texture_index == -1) {
						result.textures_different = true;
					}
					else {
						result.textures_different |= textures[shader][index].metadata_handle != other->textures[shader][texture_index].metadata_handle;
					}
				}
			}

			if (!result.samplers_different) {
				for (size_t index = 0; index < samplers[shader].size && !result.samplers_different; index++) {
					size_t sampler_index = other->FindSampler(samplers[shader][index].name, samplers[shader][index].slot, current_shader);
					if (sampler_index == -1) {
						result.samplers_different = true;
					}
					else {
						result.samplers_different |= samplers[shader][index].metadata_handle != other->samplers[shader][sampler_index].metadata_handle;
					}
				}
			}
		}

		result.is_different = result.buffers_different || result.name_different || result.pixel_shader_different
			|| result.samplers_different || result.textures_different;

		return result;
	}

	// ------------------------------------------------------------------------------------------------------

	bool MaterialAsset::CompareOptions(const MaterialAsset* other) const
	{
		bool same_shaders = vertex_shader_handle == other->vertex_shader_handle && pixel_shader_handle == other->pixel_shader_handle;
		if (same_shaders) {
			for (size_t type = 0; type < ECS_MATERIAL_SHADER_COUNT; type++) {
				if (textures[type].size != other->textures[type].size) {
					return false;
				}
				if (samplers[type].size != other->samplers[type].size) {
					return false;
				}
				if (buffers[type].size != other->buffers[type].size) {
					return false;
				}

				for (size_t index = 0; index < textures[type].size; index++) {
					MaterialAssetResource texture = textures[type][index];
					MaterialAssetResource other_texture = other->textures[type][index];
					if (texture.slot != other_texture.slot || texture.metadata_handle != other_texture.metadata_handle) {
						return false;
					}
				}
				
				for (size_t index = 0; index < samplers[type].size; index++) {
					MaterialAssetResource sampler = samplers[type][index];
					MaterialAssetResource other_sampler = other->samplers[type][index];
					if (sampler.slot != other_sampler.slot || sampler.metadata_handle != other_sampler.metadata_handle) {
						return false;
					}
				}

				for (size_t index = 0; index < buffers[type].size; index++) {
					const MaterialAssetBuffer* buffer = buffers[type].buffer + index;
					const MaterialAssetBuffer* other_buffer = other->buffers[type].buffer + index;
					if (buffer->dynamic != other_buffer->dynamic || buffer->slot != other_buffer->slot || buffer->data.size != other_buffer->data.size) {
						return false;
					}

					// If they have different byte representation, then they are different
					if (memcmp(buffer->data.buffer, other_buffer->data.buffer, buffer->data.size) != 0) {
						return false;
					}
				}
			}
			return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::CopyOptions(const MaterialAsset* other, AllocatorPolymorphic allocator)
	{
		Stream<char> previous_name = name;
		Material* previous_material = material_pointer;

		// Make the name empty such that it will not be deallocated when calling DeallocateMemory
		name = { nullptr, 0 };
		DeallocateMemory(allocator);
		*this = other->Copy(allocator);
		Deallocate(allocator, name.buffer);

		// Also restore the previous pointer
		name = previous_name;
		material_pointer = previous_material;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::CopyAndResize(const MaterialAsset* asset, AllocatorPolymorphic allocator, bool allocate_name)
	{
		Resize(asset, allocator, true);
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			textures[index].CopyOther(asset->textures[index]);
			samplers[index].CopyOther(asset->samplers[index]);
			buffers[index].CopyOther(asset->buffers[index]);
		}

		vertex_shader_handle = asset->vertex_shader_handle;
		pixel_shader_handle = asset->pixel_shader_handle;

		reflection_manager = asset->reflection_manager;
		material_pointer = asset->material_pointer;
		
		if (allocate_name) {
			name = StringCopy(allocator, asset->name);
		}
		else {
			name = asset->name;
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::CopyMatchingNames(const MaterialAsset* asset, ECS_MATERIAL_SHADER shader) {
		ForEachShaderType(shader, [&](ECS_MATERIAL_SHADER shader) {
			ECS_MATERIAL_SHADER material_shader = shader;
			for (size_t index = 0; index < textures[material_shader].size; index++) {
				size_t matching_index = asset->textures[material_shader].Find(textures[material_shader][index].name, [](auto value) {
					return value.name;
					});

				if (matching_index != -1) {
					textures[material_shader][index].slot = asset->textures[material_shader][matching_index].slot;
				}
			}
			for (size_t index = 0; index < samplers[material_shader].size; index++) {
				size_t matching_index = asset->samplers[material_shader].Find(samplers[material_shader][index].name, [](auto value) {
					return value.name;
					});

				if (matching_index != -1) {
					samplers[material_shader][index].slot = asset->samplers[material_shader][matching_index].slot;
				}
			}
			for (size_t index = 0; index < buffers[material_shader].size; index++) {
				size_t matching_index = asset->buffers[material_shader].Find(buffers[material_shader][index].name, [](auto value) {
					return value.name;
					});

				if (matching_index != -1) {
					buffers[material_shader][index].dynamic = asset->buffers[material_shader][matching_index].dynamic;
					buffers[material_shader][index].slot = asset->buffers[material_shader][matching_index].slot;

					if (asset->buffers[material_shader][matching_index].reflection_type != nullptr &&
						buffers[material_shader][index].reflection_type != nullptr && reflection_manager != nullptr
						&& asset->reflection_manager != nullptr) {
						Reflection::CopyReflectionDataOptions copy_options;
						copy_options.set_padding_bytes_to_zero = true;
						Reflection::CopyReflectionTypeToNewVersion(
							asset->reflection_manager,
							reflection_manager,
							asset->buffers[material_shader][matching_index].reflection_type,
							buffers[material_shader][index].reflection_type,
							asset->buffers[material_shader][matching_index].data.buffer,
							buffers[material_shader][index].data.buffer,
							&copy_options
						);
					}
				}
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------

	MaterialAsset MaterialAsset::Copy(AllocatorPolymorphic allocator) const
	{
		MaterialAsset material;
		material.CopyAndResize(this, allocator, true);

		if (reflection_manager != nullptr) {
			material.reflection_manager = (Reflection::ReflectionManager*)Allocate(allocator, sizeof(Reflection::ReflectionManager));
			*material.reflection_manager = Reflection::ReflectionManager(allocator, 0, 0);
			reflection_manager->CopyTypes(material.reflection_manager);
		}

		for (size_t type = 0; type < ECS_MATERIAL_SHADER_COUNT; type++) {
			for (size_t index = 0; index < textures[type].size; index++) {
				material.textures[type][index].name = StringCopy(allocator, textures[type][index].name);
			}
			for (size_t index = 0; index < samplers[type].size; index++) {
				material.samplers[type][index].name = StringCopy(allocator, samplers[type][index].name);
			}
			for (size_t index = 0; index < buffers[type].size; index++) {
				material.buffers[type][index].name = StringCopy(allocator, buffers[type][index].name);
				material.buffers[type][index].data.buffer = Allocate(allocator, buffers[type][index].data.size);
				// At the moment consider everything blittable
				memcpy(material.buffers[type][index].data.buffer, buffers[type][index].data.buffer, buffers[type][index].data.size);
				if (buffers[type][index].reflection_type != nullptr) {
					material.buffers[type][index].reflection_type = (Reflection::ReflectionType*)Allocate(allocator, sizeof(Reflection::ReflectionType));
					if (material.reflection_manager != nullptr) {
						*material.buffers[type][index].reflection_type = *material.reflection_manager->GetType(buffers[type][index].reflection_type->name);
					}
					else {
						*material.buffers[type][index].reflection_type = buffers[type][index].reflection_type->Copy(allocator);
					}
				}
			}
		}

		return material;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::DeallocateBufferReflectionTypes(AllocatorPolymorphic allocator, bool deallocate_pointer, ECS_MATERIAL_SHADER shader) const
	{
		ForEachShaderType(shader, [&](ECS_MATERIAL_SHADER shader) {
			for (size_t index = 0; index < buffers[shader].size; index++) {
				if (buffers[shader][index].reflection_type != nullptr) {
					if (deallocate_pointer) {
						DeallocateIfBelongs(allocator, buffers[shader][index].reflection_type);
					}
					buffers[shader][index].reflection_type->Deallocate(allocator);
				}
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::DeallocateBufferPointers(AllocatorPolymorphic allocator, ECS_MATERIAL_SHADER shader) const {
		ForEachShaderType(shader, [&](ECS_MATERIAL_SHADER shader) {
			for (size_t index = 0; index < buffers[shader].size; index++) {
				DeallocateIfBelongs(allocator, buffers[shader][index].data.buffer);
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::DeallocateNames(AllocatorPolymorphic allocator, ECS_MATERIAL_SHADER shader) const
	{
		ForEachShaderType(shader, [&](ECS_MATERIAL_SHADER shader) {
			for (size_t index = 0; index < textures[shader].size; index++) {
				textures[shader][index].name.DeallocateIfBelongs(allocator);
			}
			for (size_t index = 0; index < samplers[shader].size; index++) {
				samplers[shader][index].name.DeallocateIfBelongs(allocator);
			}
			for (size_t index = 0; index < buffers[shader].size; index++) {
				buffers[shader][index].name.DeallocateIfBelongs(allocator);
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::DeallocateBuffers(AllocatorPolymorphic allocator, bool include_reflection_type, ECS_MATERIAL_SHADER shader, bool assert_if_not_found) const
	{
		auto check = [assert_if_not_found](bool was_deallocated) {
			if (assert_if_not_found) {
				ECS_ASSERT(was_deallocated);
			}
		};

		ForEachShaderType(shader, [&](ECS_MATERIAL_SHADER shader) {
			for (size_t index = 0; index < buffers[shader].size; index++) {
				check(buffers[shader][index].name.DeallocateIfBelongs(allocator));
				check(DeallocateIfBelongs(allocator, buffers[shader][index].data.buffer));
				if (include_reflection_type) {
					if (buffers[shader][index].reflection_type != nullptr) {
						buffers[shader][index].reflection_type->Deallocate(allocator);
						check(DeallocateIfBelongs(allocator, buffers[shader][index].reflection_type));
					}
				}
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::DeallocateTextures(AllocatorPolymorphic allocator, ECS_MATERIAL_SHADER shader, bool assert_if_not_found) const
	{
		auto check = [assert_if_not_found](bool check) {
			if (assert_if_not_found) {
				ECS_ASSERT(check);
			}
		};
		
		ForEachShaderType(shader, [&](ECS_MATERIAL_SHADER shader) {		
			for (size_t index = 0; index < textures[shader].size; index++) {
				check(textures[shader][index].name.DeallocateIfBelongs(allocator));
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::DeallocateSamplers(AllocatorPolymorphic allocator, ECS_MATERIAL_SHADER shader, bool assert_if_not_found) const
	{
		auto check = [assert_if_not_found](bool check) {
			if (assert_if_not_found) {
				ECS_ASSERT(check);
			}
		};

		ForEachShaderType(shader, [&](ECS_MATERIAL_SHADER shader) {
			for (size_t index = 0; index < samplers[shader].size; index++) {
				check(samplers[shader][index].name.DeallocateIfBelongs(allocator));
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
		if (textures[0].buffer != nullptr) {
			ECS_ASSERT(DeallocateIfBelongs(allocator, textures[0].buffer));
		}

		DeallocateBuffers(allocator);
		DeallocateSamplers(allocator);
		DeallocateTextures(allocator);
		bool belongs = DeallocateIfBelongs(allocator, reflection_manager);
		if (belongs) {
			reflection_manager->ClearFromAllocator(true);
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::Default(Stream<char> _name, Stream<wchar_t> _file)
	{
		memset(this, 0, sizeof(*this));
		name = _name;
		pixel_shader_handle = -1;
		vertex_shader_handle = -1;
	}

	// ------------------------------------------------------------------------------------------------------

	size_t MaterialAsset::FindBuffer(Stream<char> _name, ECS_MATERIAL_SHADER shader) const
	{
		for (size_t index = 0; index < buffers[shader].size; index++) {
			if (buffers[shader][index].name == _name) {
				return index;
			}
		}
		return -1;
	}

	// ------------------------------------------------------------------------------------------------------

	size_t MaterialAsset::FindBuffer(Stream<char> _name, unsigned char slot, ECS_MATERIAL_SHADER shader) const
	{
		for (size_t index = 0; index < buffers[shader].size; index++) {
			if (buffers[shader][index].name == _name && buffers[shader][index].slot == slot) {
				return index;
			}
		}
		return -1;
	}

	// ------------------------------------------------------------------------------------------------------

	size_t MaterialAsset::FindTexture(Stream<char> _name, ECS_MATERIAL_SHADER shader) const
	{
		for (size_t index = 0; index < textures[shader].size; index++) {
			if (textures[shader][index].name == _name) {
				return index;
			}
		}
		return -1;
	}

	// ------------------------------------------------------------------------------------------------------

	size_t MaterialAsset::FindTexture(Stream<char> _name, unsigned char slot, ECS_MATERIAL_SHADER shader) const
	{
		for (size_t index = 0; index < textures[shader].size; index++) {
			if (textures[shader][index].name == _name && textures[shader][index].slot == slot) {
				return index;
			}
		}
		return -1;
	}

	// ------------------------------------------------------------------------------------------------------

	size_t MaterialAsset::FindSampler(Stream<char> _name, ECS_MATERIAL_SHADER shader) const
	{
		for (size_t index = 0; index < samplers[shader].size; index++) {
			if (samplers[shader][index].name == _name) {
				return index;
			}
		}
		return -1;
	}

	// ------------------------------------------------------------------------------------------------------

	size_t MaterialAsset::FindSampler(Stream<char> _name, unsigned char slot, ECS_MATERIAL_SHADER shader) const
	{
		for (size_t index = 0; index < samplers[shader].size; index++) {
			if (samplers[shader][index].name == _name && samplers[shader][index].slot == slot) {
				return index;
			}
		}
		return -1;
	}

	// ------------------------------------------------------------------------------------------------------

	size_t MaterialAsset::GetTextureTotalCount() const {
		size_t total = 0;
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			total += textures[index].size;
		}
		return total;
	}

	// ------------------------------------------------------------------------------------------------------

	size_t MaterialAsset::GetSamplerTotalCount() const {
		size_t total = 0;
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			total += samplers[index].size;
		}
		return total;
	}

	// ------------------------------------------------------------------------------------------------------

	size_t MaterialAsset::GetBufferTotalCount() const {
		size_t total = 0;
		for (size_t index = 0; index < ECS_MATERIAL_SHADER_COUNT; index++) {
			total += buffers[index].size;
		}
		return total;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::Rename(Stream<char> new_name, AllocatorPolymorphic allocator)
	{
		DeallocateIfBelongs(allocator, name.buffer);
		name = new_name.Copy(allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::Resize(
		const unsigned int* texture_count,
		const unsigned int* sampler_count,
		const unsigned int* buffer_count,
		AllocatorPolymorphic allocator,
		bool do_not_copy
	)
	{
		MaterialAssetRelocate(this, texture_count, sampler_count, buffer_count, allocator, do_not_copy);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::Resize(const unsigned int* counts, AllocatorPolymorphic allocator, bool do_not_copy)
	{
		Resize(counts, counts + ECS_MATERIAL_SHADER_COUNT, counts + ECS_MATERIAL_SHADER_COUNT * 2, allocator, do_not_copy);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::Resize(const MaterialAsset* other, AllocatorPolymorphic allocator, bool do_not_copy)
	{
		unsigned int counts[ECS_MATERIAL_SHADER_COUNT * 3];
		other->WriteCounts(true, true, true, counts);
		Resize(counts, allocator, do_not_copy);
	}

	// ------------------------------------------------------------------------------------------------------

	// The functor receives a single parameter, the resize count
	template<typename Functor>
	void MaterialAssetResizeSingleValue(
		MaterialAsset* material,
		AllocatorPolymorphic allocator,
		bool do_not_copy,
		Functor&& functor
	) {
		unsigned int resize_counts[ECS_MATERIAL_SHADER_COUNT * 3];
		material->WriteCounts(true, true, true, resize_counts);

		functor(resize_counts);

		material->Resize(resize_counts, allocator, do_not_copy);
	}

	void MaterialAsset::ResizeBufferNewValue(
		unsigned int value,
		ECS_MATERIAL_SHADER shader_type,
		AllocatorPolymorphic allocator,
		bool do_not_copy
	) {
		MaterialAssetResizeSingleValue(this, allocator, do_not_copy, [=](unsigned int* resize_counts) {
			if (shader_type == ECS_MATERIAL_SHADER_VERTEX) {
				WriteBufferCount(value, buffers[ECS_MATERIAL_SHADER_PIXEL].size, resize_counts);
			}
			else {
				WriteBufferCount(buffers[ECS_MATERIAL_SHADER_VERTEX].size, value, resize_counts);
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::ResizeTexturesNewValue(
		unsigned int value,
		ECS_MATERIAL_SHADER shader_type,
		AllocatorPolymorphic allocator,
		bool do_not_copy
	) {
		MaterialAssetResizeSingleValue(this, allocator, do_not_copy, [=](unsigned int* resize_counts) {
			if (shader_type == ECS_MATERIAL_SHADER_VERTEX) {
				WriteTextureCount(value, textures[ECS_MATERIAL_SHADER_PIXEL].size, resize_counts);
			}
			else {
				WriteTextureCount(textures[ECS_MATERIAL_SHADER_VERTEX].size, value, resize_counts);
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::ResizeSamplersNewValue(
		unsigned int value,
		ECS_MATERIAL_SHADER shader_type,
		AllocatorPolymorphic allocator,
		bool do_not_copy
	) {
		MaterialAssetResizeSingleValue(this, allocator, do_not_copy, [=](unsigned int* resize_counts) {
			if (shader_type == ECS_MATERIAL_SHADER_VERTEX) {
				WriteSamplerCount(value, samplers[ECS_MATERIAL_SHADER_PIXEL].size, resize_counts);
			}
			else {
				WriteSamplerCount(samplers[ECS_MATERIAL_SHADER_VERTEX].size, value, resize_counts);
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::RetrieveBufferData(
		size_t current_index, 
		ECS_MATERIAL_SHADER shader, 
		const Reflection::ReflectionType* other_type, 
		const void* data,
		AllocatorPolymorphic allocator
	)
	{
		const MaterialAssetBuffer* current_buffer = &buffers[shader][current_index];

		if (Reflection::CompareReflectionTypes(reflection_manager, reflection_manager, other_type, current_buffer->reflection_type)) {
			memcpy(current_buffer->data.buffer, data, current_buffer->data.size);
		}
		else {
			Reflection::CopyReflectionDataOptions copy_options;
			copy_options.allocator = allocator;
			copy_options.always_allocate_for_buffers = true;
			// It has changed. We need to convert the old type to the new type
			Reflection::CopyReflectionTypeToNewVersion(
				reflection_manager,
				reflection_manager,
				other_type,
				current_buffer->reflection_type,
				data,
				current_buffer->data.buffer,
				&copy_options
			);
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::WriteCounts(bool write_texture, bool write_sampler, bool write_buffers, unsigned int* counts) const
	{
		auto write_stream = [counts](bool write, unsigned int offset, auto stream) {
			if (write) {
				counts[offset + ECS_MATERIAL_SHADER_VERTEX] = stream[ECS_MATERIAL_SHADER_VERTEX].size;
				counts[offset + ECS_MATERIAL_SHADER_PIXEL] = stream[ECS_MATERIAL_SHADER_PIXEL].size;
			}
		};

		write_stream(write_texture, 0, textures);
		write_stream(write_sampler, ECS_MATERIAL_SHADER_COUNT, samplers);
		write_stream(write_buffers, ECS_MATERIAL_SHADER_COUNT * 2, buffers);
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::WriteTextureCount(unsigned int vertex, unsigned int pixel, unsigned int* counts) {
		counts[ECS_MATERIAL_SHADER_VERTEX] = vertex;
		counts[ECS_MATERIAL_SHADER_PIXEL] = pixel;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::WriteSamplerCount(unsigned int vertex, unsigned int pixel, unsigned int* counts) {
		counts[ECS_MATERIAL_SHADER_COUNT + ECS_MATERIAL_SHADER_VERTEX] = vertex;
		counts[ECS_MATERIAL_SHADER_COUNT + ECS_MATERIAL_SHADER_PIXEL] = pixel;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::WriteBufferCount(unsigned int vertex, unsigned int pixel, unsigned int* counts) {
		counts[ECS_MATERIAL_SHADER_COUNT * 2 + ECS_MATERIAL_SHADER_VERTEX] = vertex;
		counts[ECS_MATERIAL_SHADER_COUNT * 2 + ECS_MATERIAL_SHADER_PIXEL] = pixel;
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::GetDependencies(CapacityStream<AssetTypedHandle>* handles) const
	{
		if (vertex_shader_handle != -1) {
			handles->AddAssert({ vertex_shader_handle, ECS_ASSET_SHADER });
		}

		if (pixel_shader_handle != -1) {
			handles->AddAssert({ pixel_shader_handle, ECS_ASSET_SHADER });
		}

		for (size_t type = 0; type < ECS_MATERIAL_SHADER_COUNT; type++) {
			for (size_t index = 0; index < textures[type].size; index++) {
				if (textures[type][index].metadata_handle != -1) {
					handles->AddAssert({ textures[type][index].metadata_handle, ECS_ASSET_TEXTURE });
				}
			}
			for (size_t index = 0; index < samplers[type].size; index++) {
				if (samplers[type][index].metadata_handle != -1) {
					handles->AddAssert({ samplers[type][index].metadata_handle, ECS_ASSET_GPU_SAMPLER });
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::GetDependenciesForMetadata(CapacityStream<AssetTypedHandle>* handles) const
	{
		if (vertex_shader_handle != -1) {
			handles->AddAssert({ vertex_shader_handle, ECS_ASSET_SHADER });
		}
		
		if (pixel_shader_handle != -1) {
			handles->AddAssert({ pixel_shader_handle, ECS_ASSET_SHADER });
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void MaterialAsset::RemapDependencies(Stream<AssetTypedHandle> handles)
	{
		size_t current_handles_size = 0;

		auto get_handle = [&current_handles_size, handles](ECS_ASSET_TYPE type) {
			ECS_ASSERT(handles.size > current_handles_size);
			ECS_ASSERT(handles[current_handles_size].type == type);

			return handles[current_handles_size++].handle;
		};
 
		if (vertex_shader_handle != -1) {
			vertex_shader_handle = get_handle(ECS_ASSET_SHADER);
		}

		if (pixel_shader_handle != -1) {
			pixel_shader_handle = get_handle(ECS_ASSET_SHADER);
		}

		for (size_t type = 0; type < ECS_MATERIAL_SHADER_COUNT; type++) {
			for (size_t index = 0; index < textures[type].size; index++) {
				if (textures[type][index].metadata_handle != -1) {
					textures[type][index].metadata_handle = get_handle(ECS_ASSET_TEXTURE);
				}
			}
			for (size_t index = 0; index < samplers[type].size; index++) {
				if (samplers[type][index].metadata_handle != -1) {
					samplers[type][index].metadata_handle = get_handle(ECS_ASSET_GPU_SAMPLER);
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------

	bool MaterialAsset::DependsUpon(const void* metadata, ECS_ASSET_TYPE type) const
	{
		switch (type) {
		case ECS_ASSET_TEXTURE:
		{
			TextureMetadata* texture_metadata = (TextureMetadata*)metadata;
			return material_pointer->ContainsTexture(texture_metadata->texture);
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			GPUSamplerMetadata* gpu_sampler_metadata = (GPUSamplerMetadata*)metadata;
			return material_pointer->ContainsSampler(gpu_sampler_metadata->sampler);
		}
		break;
		case ECS_ASSET_SHADER:
		{
			ShaderMetadata* shader_metadata = (ShaderMetadata*)metadata;
			if (shader_metadata->shader_type == ECS_SHADER_VERTEX) {
				return material_pointer->ContainsShader(VertexShader::FromInterface(shader_metadata->shader_interface));
			}
			else if (shader_metadata->shader_type == ECS_SHADER_PIXEL) {
				return material_pointer->ContainsShader(PixelShader::FromInterface(shader_metadata->shader_interface));
			}
			else {
				return false;
			}
		}
		break;
		case ECS_ASSET_MESH:
		case ECS_ASSET_MATERIAL:
		case ECS_ASSET_MISC:
			return false;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		// Shouldn't be reached here
		return false;
	}

	// ------------------------------------------------------------------------------------------------------

	ShaderMetadata::ShaderMetadata(Stream<char> _name, Stream<ShaderMacro> _macros, AllocatorPolymorphic allocator)
	{
		macros.buffer = (ShaderMacro*)Allocate(allocator, _macros.size * sizeof(ShaderMacro));
		
		// Now allocate every string separately
		macros.size = _macros.size;
		for (size_t index = 0; index < _macros.size; index++) {
			macros[index].name = StringCopy(allocator, _macros[index].name);
			macros[index].definition = StringCopy(allocator, _macros[index].definition);
		}

		name = StringCopy(allocator, _name);
		file = { nullptr, 0 };
		shader_type = ECS_SHADER_PIXEL;
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::AddMacro(Stream<char> macro_name, Stream<char> definition, AllocatorPolymorphic allocator)
	{
		// Copy the string pointers to a temporary stack such that the memory can be freed first and then
		// reallocated
		ShaderMacro* temp_macros = (ShaderMacro*)ECS_STACK_ALLOC(sizeof(ShaderMacro) * macros.size);
		
		if (macros.size > 0) {
			memcpy(temp_macros, macros.buffer, sizeof(ShaderMacro) * macros.size);
			Deallocate(allocator, macros.buffer);
		}

		macros.buffer = (ShaderMacro*)Allocate(allocator, sizeof(ShaderMacro) * (macros.size + 1));
		memcpy(macros.buffer, temp_macros, sizeof(ShaderMacro) * macros.size);

		// Allocate the name and the definition separately
		Stream<char> new_name = StringCopy(allocator, macro_name);
		Stream<char> new_definition = StringCopy(allocator, definition);

		macros.Add({ new_name, new_definition });
	}

	// ------------------------------------------------------------------------------------------------------

	bool ShaderMetadata::Compare(const ShaderMetadata* other) const
	{
		return name == other->name && file == other->file && CompareOptions(other);
	}

	// ------------------------------------------------------------------------------------------------------

	bool ShaderMetadata::CompareOptions(const ShaderMetadata* other) const
	{
		bool compare = shader_type == other->shader_type && compile_flag == other->compile_flag;
		if (compare) {
			if (macros.size == other->macros.size) {
				for (size_t index = 0; index < macros.size; index++) {
					if (!macros[index].Compare(other->macros[index])) {
						return false;
					}
				}
				return true;
			}
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::CopyOptions(const ShaderMetadata* other, AllocatorPolymorphic allocator)
	{
		shader_type = other->shader_type;
		compile_flag = other->compile_flag;

		DeallocateMacros(allocator);
		macros = StreamDeepCopy(other->macros, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	ShaderMetadata ShaderMetadata::Copy(AllocatorPolymorphic allocator) const
	{
		ShaderMetadata metadata = ShaderMetadata(name, macros, allocator);

		metadata.compile_flag = compile_flag;
		metadata.file = StringCopy(allocator, file);
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
		
		if (file != other->file) {
			return false;
		}

		if (macros.size != other->macros.size) {
			return false;
		}

		for (size_t index = 0; index < macros.size; index++) {
			if (macros[index].definition != other->macros[index].definition || macros[index].name != other->macros[index].name) {
				return false;
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::DeallocateMacros(AllocatorPolymorphic allocator) const
	{
		if (BelongsToAllocator(allocator, macros.buffer)) {
			for (size_t index = 0; index < macros.size; index++) {
				macros[index].name.Deallocate(allocator);
				macros[index].definition.Deallocate(allocator);
			}

			macros.Deallocate(allocator);
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
		DeallocateIfBelongs(allocator, file.buffer);

		DeallocateMacros(allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::Default(Stream<char> _name, Stream<wchar_t> _file)
	{
		memset(this, 0, sizeof(*this));
		name = _name;
		file = _file;
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::RemoveMacro(unsigned int index, AllocatorPolymorphic allocator)
	{
		macros[index].name.Deallocate(allocator);
		macros[index].definition.Deallocate(allocator);

		macros.RemoveSwapBack(index);
		ShaderMacro* temp_macros = (ShaderMacro*)ECS_STACK_ALLOC(sizeof(ShaderMacro) * macros.size);
		macros.CopyTo(temp_macros);
		Deallocate(allocator, macros.buffer);
		
		macros.buffer = (ShaderMacro*)Allocate(allocator, sizeof(ShaderMacro) * macros.size);
		memcpy(macros.buffer, temp_macros, sizeof(ShaderMacro) * macros.size);
	}

	void ShaderMetadata::RemoveMacro(Stream<char> macro_name, AllocatorPolymorphic allocator)
	{
		unsigned int index = FindMacro(macro_name);
		ECS_ASSERT(index != -1);
		RemoveMacro(index, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::UpdateMacro(unsigned int index, Stream<char> new_definition, AllocatorPolymorphic allocator)
	{
		macros[index].definition.Deallocate(allocator);
		macros[index].definition = StringCopy(allocator, new_definition);
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::UpdateMacro(Stream<char> macro_name, Stream<char> new_definition, AllocatorPolymorphic allocator)
	{
		unsigned int index = FindMacro(macro_name);
		ECS_ASSERT(index != -1);
		UpdateMacro(index, new_definition, allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	unsigned int ShaderMetadata::FindMacro(Stream<char> macro_name) const
	{
		for (size_t index = 0; index < macros.size; index++) {
			if (macros[index].name == macro_name) {
				return index;
			}
		}

		return -1;
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::Rename(Stream<char> new_name, AllocatorPolymorphic allocator)
	{
		name.DeallocateIfBelongs(allocator);
		name = StringCopy(allocator, new_name);
	}

	// ------------------------------------------------------------------------------------------------------

	void ShaderMetadata::RenameFile(Stream<wchar_t> new_file, AllocatorPolymorphic allocator)
	{
		file.DeallocateIfBelongs(allocator);
		file = StringCopy(allocator, new_file);
	}

	// ------------------------------------------------------------------------------------------------------

	void MiscAsset::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, file.buffer);
		DeallocateIfBelongs(allocator, name.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	bool MiscAsset::Compare(const MiscAsset* other) const
	{
		return name == other->name && file == other->file && data.AsStream().Equals(other->data.AsStream());
	}

	// ------------------------------------------------------------------------------------------------------

	MiscAsset MiscAsset::Copy(AllocatorPolymorphic allocator) const
	{
		MiscAsset asset;
		asset.file = StringCopy(allocator, file);
		asset.name = StringCopy(allocator, name);
		return asset;
	}

	// ------------------------------------------------------------------------------------------------------

	bool MiscAsset::SameTarget(const MiscAsset* other) const
	{
		return file == other->file;
	}

	// ------------------------------------------------------------------------------------------------------

	void MiscAsset::Default(Stream<char> _name, Stream<wchar_t> _file) {
		name = _name;
		file = _file;
		data = { nullptr, 0 };
	}

	// ------------------------------------------------------------------------------------------------------

	void MiscAsset::Rename(Stream<char> new_name, AllocatorPolymorphic allocator)
	{
		name.DeallocateIfBelongs(allocator);
		name = StringCopy(allocator, new_name);
	}

	// ------------------------------------------------------------------------------------------------------

	void MiscAsset::RenameFile(Stream<wchar_t> new_file, AllocatorPolymorphic allocator)
	{
		file.DeallocateIfBelongs(allocator);
		file = StringCopy(allocator, new_file);
	}

	// ------------------------------------------------------------------------------------------------------

	void GPUSamplerMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	bool GPUSamplerMetadata::Compare(const GPUSamplerMetadata* other) const
	{
		return name == other->name && CompareOptions(other);
	}

	// ------------------------------------------------------------------------------------------------------

	bool GPUSamplerMetadata::CompareOptions(const GPUSamplerMetadata* other) const
	{
		bool compare = address_mode == other->address_mode && filter_mode == other->filter_mode;
		if (compare) {
			if (filter_mode == ECS_SAMPLER_FILTER_ANISOTROPIC) {
				return anisotropic_level == other->anisotropic_level;
			}
			return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------

	void GPUSamplerMetadata::CopyOptions(const GPUSamplerMetadata* other)
	{
		address_mode = other->address_mode;
		filter_mode = other->filter_mode;
		anisotropic_level = other->anisotropic_level;
	}

	// ------------------------------------------------------------------------------------------------------

	GPUSamplerMetadata GPUSamplerMetadata::Copy(AllocatorPolymorphic allocator) const
	{
		GPUSamplerMetadata metadata;
		memcpy(&metadata, this, sizeof(metadata));
		metadata.name = StringCopy(allocator, name);
		return metadata;
	}

	// ------------------------------------------------------------------------------------------------------

	void GPUSamplerMetadata::Default(Stream<char> _name, Stream<wchar_t> _file) {
		name = _name;
		filter_mode = ECS_SAMPLER_FILTER_ANISOTROPIC;
		address_mode = ECS_SAMPLER_ADDRESS_WRAP;
		anisotropic_level = 8;
		sampler = { nullptr };
	}

	// ------------------------------------------------------------------------------------------------------

	void GPUSamplerMetadata::Rename(Stream<char> new_name, AllocatorPolymorphic allocator)
	{
		DeallocateIfBelongs(allocator, name.buffer);
		name = new_name.Copy(allocator);
	}

	// ------------------------------------------------------------------------------------------------------

	void TextureMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
		DeallocateIfBelongs(allocator, file.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	bool TextureMetadata::Compare(const TextureMetadata* other) const
	{
		return name == other->name && file == other->file && CompareOptions(other);
	}

	// ------------------------------------------------------------------------------------------------------

	bool TextureMetadata::CompareOptions(const TextureMetadata* other) const
	{
		return sRGB == other->sRGB && generate_mip_maps == other->generate_mip_maps && compression_type == other->compression_type;
	}

	// ------------------------------------------------------------------------------------------------------

	void TextureMetadata::CopyOptions(const TextureMetadata* other)
	{
		sRGB = other->sRGB;
		generate_mip_maps = other->generate_mip_maps;
		compression_type = other->compression_type;
	}

	// ------------------------------------------------------------------------------------------------------

	TextureMetadata TextureMetadata::Copy(AllocatorPolymorphic allocator) const
	{
		TextureMetadata metadata;
		memcpy(&metadata, this, sizeof(metadata));
		metadata.name = StringCopy(allocator, name);
		metadata.file = StringCopy(allocator, file);
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
		texture = { nullptr };
	}

	// ------------------------------------------------------------------------------------------------------

	void TextureMetadata::Rename(Stream<char> new_name, AllocatorPolymorphic allocator)
	{
		name.DeallocateIfBelongs(allocator);
		name = StringCopy(allocator, new_name);
	}

	// ------------------------------------------------------------------------------------------------------

	void TextureMetadata::RenameFile(Stream<wchar_t> new_file, AllocatorPolymorphic allocator)
	{
		file.DeallocateIfBelongs(allocator);
		file = StringCopy(allocator, new_file);
	}

	// ------------------------------------------------------------------------------------------------------

	bool TextureMetadata::SameTarget(const TextureMetadata* other) const
	{
		return file == other->file && sRGB == other->sRGB && generate_mip_maps && other->generate_mip_maps
			&& compression_type == other->compression_type;
	}

	// ------------------------------------------------------------------------------------------------------

	void MeshMetadata::DeallocateMemory(AllocatorPolymorphic allocator) const
	{
		DeallocateIfBelongs(allocator, name.buffer);
		DeallocateIfBelongs(allocator, file.buffer);
	}

	// ------------------------------------------------------------------------------------------------------

	bool MeshMetadata::Compare(const MeshMetadata* other) const
	{
		return name == other->name && file == other->file && CompareOptions(other);
	}

	// ------------------------------------------------------------------------------------------------------

	bool MeshMetadata::CompareOptions(const MeshMetadata* other) const
	{
		return scale_factor == other->scale_factor && invert_z_axis == other->invert_z_axis && optimize_level == other->optimize_level
			&& origin_to_object_center == other->origin_to_object_center;
	}

	// ------------------------------------------------------------------------------------------------------

	void MeshMetadata::CopyOptions(const MeshMetadata* other)
	{
		scale_factor = other->scale_factor;
		invert_z_axis = other->invert_z_axis;
		optimize_level = other->optimize_level;
		origin_to_object_center = other->origin_to_object_center;
	}

	// ------------------------------------------------------------------------------------------------------

	MeshMetadata MeshMetadata::Copy(AllocatorPolymorphic allocator) const
	{
		MeshMetadata metadata;
		memcpy(&metadata, this, sizeof(metadata));
		metadata.name = StringCopy(allocator, name);
		metadata.file = StringCopy(allocator, file);
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
		origin_to_object_center = true;

		mesh_pointer = nullptr;
	}

	// ------------------------------------------------------------------------------------------------------

	void MeshMetadata::Rename(Stream<char> new_name, AllocatorPolymorphic allocator)
	{
		name.DeallocateIfBelongs(allocator);
		name = StringCopy(allocator, new_name);
	}

	// ------------------------------------------------------------------------------------------------------

	void MeshMetadata::RenameFile(Stream<wchar_t> new_file, AllocatorPolymorphic allocator)
	{
		file.DeallocateIfBelongs(allocator);
		file = StringCopy(allocator, new_file);
	}

	// ------------------------------------------------------------------------------------------------------

	bool MeshMetadata::SameTarget(const MeshMetadata* other) const
	{
		return file == other->file && scale_factor == other->scale_factor && invert_z_axis == other->invert_z_axis
			&& optimize_level == other->optimize_level;
	}

	// ------------------------------------------------------------------------------------------------------

#define ASSET_SWITCH(asset_type, function_to_call) \
	switch(asset_type) { \
	case ECS_ASSET_MESH: \
	{ \
	function_to_call(ECS_ASSET_MESH, MeshMetadata); \
	} \
	break; \
	case ECS_ASSET_TEXTURE: \
	{ \
		function_to_call(ECS_ASSET_TEXTURE, TextureMetadata); \
	} \
	break; \
	case ECS_ASSET_GPU_SAMPLER: \
	{ \
		function_to_call(ECS_ASSET_GPU_SAMPLER, GPUSamplerMetadata); \
	} \
	break; \
	case ECS_ASSET_SHADER: \
	{ \
		function_to_call(ECS_ASSET_SHADER, ShaderMetadata); \
	} \
	break; \
	case ECS_ASSET_MATERIAL: \
	{ \
		function_to_call(ECS_ASSET_MATERIAL, MaterialAsset); \
	} \
	break; \
	case ECS_ASSET_MISC: \
	{ \
		function_to_call(ECS_ASSET_MISC, MiscAsset); \
	} \
	break; \
	default: \
		ECS_ASSERT(false, "Invalid asset type"); \
	}

	// ------------------------------------------------------------------------------------------------------

	void CopyAssetBase(void* destination, const void* source, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator) {
#define CASE(asset_type, metadata_type) metadata_type* typed_destination = (metadata_type*)destination; *typed_destination = ((metadata_type*)source)->Copy(allocator);

		ASSET_SWITCH(type, CASE);

#undef CASE
	}

	// ------------------------------------------------------------------------------------------------------

	void CopyAssetOptions(void* destination, const void* source, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator)
	{
		switch (type) {
		case ECS_ASSET_MESH:
		{
			MeshMetadata* metadata = (MeshMetadata*)destination;
			metadata->CopyOptions((MeshMetadata*)source);
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			TextureMetadata* metadata = (TextureMetadata*)destination;
			metadata->CopyOptions((TextureMetadata*)source);
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			GPUSamplerMetadata* metadata = (GPUSamplerMetadata*)destination;
			metadata->CopyOptions((GPUSamplerMetadata*)source);
		}
		break;
		case ECS_ASSET_SHADER:
		{
			ShaderMetadata* metadata = (ShaderMetadata*)destination;
			metadata->CopyOptions((ShaderMetadata*)source, allocator);
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			MaterialAsset* metadata = (MaterialAsset*)destination;
			metadata->CopyOptions((MaterialAsset*)source, allocator);
		}
		break;
		case ECS_ASSET_MISC:
		{
			MiscAsset* metadata = (MiscAsset*)destination;
			metadata->CopyOptions((MiscAsset*)source, allocator);
		}
		break;
		default:
			ECS_ASSERT(false);
		}
	}


	// ------------------------------------------------------------------------------------------------------

	void DeallocateAssetBase(const void* asset, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator)
	{
#define CASE(asset_type, metadata_type) ((metadata_type*)asset)->DeallocateMemory(allocator);
		
		ASSET_SWITCH(type, CASE);

#undef CASE
	}

	// ------------------------------------------------------------------------------------------------------

	Stream<wchar_t> GetAssetFile(const void* asset, ECS_ASSET_TYPE type)
	{
		switch (type) {
		case ECS_ASSET_MESH:
		{
			return ((MeshMetadata*)asset)->file;
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			return ((TextureMetadata*)asset)->file;
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			return {};
		}
		break;
		case ECS_ASSET_SHADER:
		{
			return ((ShaderMetadata*)asset)->file;
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			return {};
		}
		break;
		case ECS_ASSET_MISC:
		{
			return ((MiscAsset*)asset)->file;
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		// Shouldn't be reached here
		return {};
	}

	// ------------------------------------------------------------------------------------------------------

	Stream<char> GetAssetName(const void* asset, ECS_ASSET_TYPE type) {
		return *(Stream<char>*)asset;
	}

	// ------------------------------------------------------------------------------------------------------

	void CreateDefaultAsset(void* asset, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type)
	{
#define CASE(asset_type, metadata_type) ((metadata_type*)asset)->Default(name, file);

		ASSET_SWITCH(type, CASE);

#undef CASE
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
			return ((MiscAsset*)metadata)->data.AsStream();
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
		return { nullptr, 0 };
	}

	// ------------------------------------------------------------------------------------------------------

	void SetAssetToMetadata(void* metadata, ECS_ASSET_TYPE type, Stream<void> asset)
	{
		switch (type) {
		case ECS_ASSET_MESH:
		{
			MeshMetadata* mesh = (MeshMetadata*)metadata;
			mesh->mesh_pointer = (CoalescedMesh*)asset.buffer;
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			TextureMetadata* texture = (TextureMetadata*)metadata;
			texture->texture = (ID3D11ShaderResourceView*)asset.buffer;
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			GPUSamplerMetadata* sampler = (GPUSamplerMetadata*)metadata;
			sampler->sampler = (ID3D11SamplerState*)asset.buffer;
		}
		break;
		case ECS_ASSET_SHADER:
		{
			ShaderMetadata* shader = (ShaderMetadata*)metadata;
			shader->shader_interface = asset.buffer;
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			MaterialAsset* material = (MaterialAsset*)metadata;
			material->material_pointer = (Material*)asset.buffer;
		}
		break;
		case ECS_ASSET_MISC:
		{
			MiscAsset* misc = (MiscAsset*)metadata;
			misc->data = asset;
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type.");
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void SetValidAssetToMetadata(void* metadata, ECS_ASSET_TYPE type) {
		SetAssetToMetadata(metadata, type, { (void*)-1, 0 });
	}

	// ------------------------------------------------------------------------------------------------------

	void SetRandomizedAssetToMetadata(void* metadata, ECS_ASSET_TYPE type, unsigned int index) {
		switch (type) {
		case ECS_ASSET_MESH:
		{
			MeshMetadata* mesh = (MeshMetadata*)metadata;
			mesh->mesh_pointer = (CoalescedMesh*)index;
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			TextureMetadata* texture = (TextureMetadata*)metadata;
			texture->texture = (ID3D11ShaderResourceView*)index;
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		{
			GPUSamplerMetadata* sampler = (GPUSamplerMetadata*)metadata;
			sampler->sampler = (ID3D11SamplerState*)index;
		}
		break;
		case ECS_ASSET_SHADER:
		{
			ShaderMetadata* shader = (ShaderMetadata*)metadata;
			shader->shader_interface = (void*)index;
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			MaterialAsset* material = (MaterialAsset*)metadata;
			material->material_pointer = (Material*)index;

		}
		break;
		case ECS_ASSET_MISC:
		{
			MiscAsset* misc = (MiscAsset*)metadata;
			misc->data = Stream<void>{ (void*)index, 0 };
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid asset type.");
		}
	}

	// ------------------------------------------------------------------------------------------------------

	bool CompareAssetPointers(const void* first, const void* second, ECS_ASSET_TYPE type) {
		return CompareAssetPointers(first, second, AssetMetadataByteSize(type));
	}

	// ------------------------------------------------------------------------------------------------------

	bool CompareAssetPointers(const void* first, const void* second, size_t compare_size) {
		if (first == second) {
			return true;
		}

		if (IsAssetPointerValid(first) && IsAssetPointerValid(second)) {
			if (compare_size > 0 && memcmp(first, second, compare_size) == 0) {
				return true;
			}
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------

	void AssetToString(const void* metadata, ECS_ASSET_TYPE type, CapacityStream<char>& string, bool long_format)
	{
		AssetToString(GetAssetName(metadata, type), GetAssetFile(metadata, type), string, long_format);
	}

	// ------------------------------------------------------------------------------------------------------

	void AssetToString(Stream<char> name, Stream<wchar_t> file, CapacityStream<char>& string, bool long_format)
	{
		if (file.size == 0) {
			string.AddStreamSafe(name);
		}
		else {
			Stream<wchar_t> path_to_write = file;
			if (!long_format) {
				path_to_write = PathFilenameBoth(path_to_write);
			}

			FormatString(string, "{#} ({#})", path_to_write, name);
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void GetAssetDependencies(const void* metadata, ECS_ASSET_TYPE type, CapacityStream<AssetTypedHandle>* handles)
	{
		if (type == ECS_ASSET_MATERIAL) {
			((MaterialAsset*)metadata)->GetDependencies(handles);
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void GetAssetDependenciesForMetadata(const void* metadata, ECS_ASSET_TYPE type, CapacityStream<AssetTypedHandle>* handles) {
		if (type == ECS_ASSET_MATERIAL) {
			((MaterialAsset*)metadata)->GetDependenciesForMetadata(handles);
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void RemapAssetDependencies(void* metadata, ECS_ASSET_TYPE type, Stream<AssetTypedHandle> handles) {
		if (type == ECS_ASSET_MATERIAL) {
			((MaterialAsset*)metadata)->RemapDependencies(handles);
		}
	}

	// ------------------------------------------------------------------------------------------------------

	void RenameAsset(void* metadata, ECS_ASSET_TYPE type, Stream<char> new_name, AllocatorPolymorphic allocator) {
#define CASE(asset_type, metadata_type) ((metadata_type*)metadata)->Rename(new_name, allocator);

		ASSET_SWITCH(type, CASE);

#undef CASE
	}

	// ------------------------------------------------------------------------------------------------------

	void RenameAssetFile(void* untyped_metadata, ECS_ASSET_TYPE type, Stream<wchar_t> new_file, AllocatorPolymorphic allocator) {
		switch(type) {
		case ECS_ASSET_MESH:
		{
			MeshMetadata* metadata = (MeshMetadata*)untyped_metadata;
			metadata->RenameFile(new_file, allocator);
		}
		break;
		case ECS_ASSET_TEXTURE:
		{
			TextureMetadata* metadata = (TextureMetadata*)untyped_metadata;
			metadata->RenameFile(new_file, allocator);
		}
		break;
		case ECS_ASSET_SHADER:
		{
			ShaderMetadata* metadata = (ShaderMetadata*)untyped_metadata;
			metadata->RenameFile(new_file, allocator);
		}
		break;
		case ECS_ASSET_MISC:
		{
			MiscAsset* metadata = (MiscAsset*)untyped_metadata;
			metadata->RenameFile(new_file, allocator);
		}
		break;
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_MATERIAL:
			break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}
	}

	// ------------------------------------------------------------------------------------------------------

	bool DoesAssetReferenceOtherAsset(unsigned int handle, ECS_ASSET_TYPE handle_type, const void* asset, ECS_ASSET_TYPE type) 
	{
		if (ExistsStaticArray(type, ECS_ASSET_TYPES_WITH_DEPENDENCIES) && ExistsStaticArray(handle_type, ECS_ASSET_TYPES_REFERENCEABLE)) {
			ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
			GetAssetDependencies(asset, type, &dependencies);
			for (unsigned int index = 0; index < dependencies.size; index++) {
				if (dependencies[index].handle == handle && dependencies[index].type == handle_type) {
					return true;
				}
			}
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------

	bool CompareAssetOptions(const void* first, const void* second, ECS_ASSET_TYPE type) {
#define CASE(asset_type, metadata_type)	{ \
											const metadata_type* first_metadata = (const metadata_type*)first; \
											return first_metadata->CompareOptions((const metadata_type*)second); \
										} 

		ASSET_SWITCH(type, CASE);

#undef CASE

		return false;
	}

	// ------------------------------------------------------------------------------------------------------

	CompareAssetsResult CompareAssetOptionsEx(const void* first, const void* second, ECS_ASSET_TYPE type) {
		switch (type) {
		case ECS_ASSET_MESH:
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		case ECS_ASSET_MISC:
			return { CompareAssetOptions(first, second, type) };
		case ECS_ASSET_MATERIAL:
			return { CompareAssets(first, second, type) };
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		// Shouldn't be reached here
		return CompareAssetsResult(false);
	}

	// ------------------------------------------------------------------------------------------------------

	CompareAssetsResult CompareAssets(const void* first, const void* second, ECS_ASSET_TYPE type) {
#define CASE(asset_type, metadata_type)	{ \
											const metadata_type* first_metadata = (const metadata_type*)first; \
											return { first_metadata->Compare((const metadata_type*)second) }; \
										} 

		ASSET_SWITCH(type, CASE);

#undef CASE

		// Shouldn't be reached here
		return CompareAssetsResult(false);
	}

	// ------------------------------------------------------------------------------------------------------

	bool ValidateAssetMetadataOptions(const void* metadata, ECS_ASSET_TYPE type, ValidateAssetMetadataOptionsDesc options) {
		switch (type) {
		case ECS_ASSET_MESH:
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		case ECS_ASSET_MISC:
			return true;
		case ECS_ASSET_MATERIAL:
			if (options.materials_validate_all) {
				return ValidateAssetDependencies(metadata, type);
			}
			else {
				MaterialAsset* material = (MaterialAsset*)metadata;
				return material->vertex_shader_handle != -1 && material->pixel_shader_handle != -1;
			}
			break;
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		// Shouldn't be reached
		return false;
	}

	// ------------------------------------------------------------------------------------------------------

	bool ValidateAssetDependencies(const void* metadata, ECS_ASSET_TYPE type) {
		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
		GetAssetDependencies(metadata, type, &dependencies);

		for (unsigned int index = 0; index < dependencies.size; index++) {
			if (dependencies[index].handle == -1) {
				return false;
			}
		}
		return true;
	}

	// -------------------------------------------------------------------------------------------------------------------------

	ECS_INLINE static void GetGPUResources(const MeshMetadata* metadata, AdditionStream<void*> resources) {
		metadata->mesh_pointer->mesh.GetGPUResources(resources);
	}

	ECS_INLINE static void GetGPUResources(const TextureMetadata* metadata, AdditionStream<void*> resources) {
		resources.Add(metadata->texture.Interface());
	}

	ECS_INLINE static void GetGPUResources(const GPUSamplerMetadata* metadata, AdditionStream<void*> resources) {
		resources.Add(metadata->sampler.Interface());
	}

	ECS_INLINE static void GetGPUResources(const ShaderMetadata* metadata, AdditionStream<void*> resources) {
		resources.Add(metadata->shader_interface);
	}

	ECS_INLINE static void GetGPUResources(const MaterialAsset* metadata, AdditionStream<void*> resources) {
		// This might add textures and samplers multiple times, but it's alright if that happens
		metadata->material_pointer->GetGPUResources(resources);
	}

	ECS_INLINE static void GetGPUResources(const MiscAsset* metadata, AdditionStream<void*> resources) {}

	void GetAssetGPUResources(const void* metadata, ECS_ASSET_TYPE asset_type, AdditionStream<void*> resources) {
		// Early exit if the asset pointer is not valid
		if (!IsAssetFromMetadataValid(metadata, asset_type)) {
			return;
		}

#define CASE(asset_type, metadata_type) 	{ \
											const metadata_type* typed_metadata = (const metadata_type*)metadata; \
											GetGPUResources(typed_metadata, resources); \
										} 

		ASSET_SWITCH(asset_type, CASE);
	}

	// ------------------------------------------------------------------------------------------------------
}