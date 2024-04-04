#pragma once
#include "../../Dependencies/cgltf/cgltf.h"
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"
#include "../Utilities/PointerUtilities.h"
#include "../Utilities/StringUtilities.h"
#include "../Rendering/RenderingStructures.h"
#include "../Allocators/AllocatorTypes.h"
#include "../Math/AABB.h"
#include "../Resources/ResourceTypes.h"

namespace ECSEngine {

	struct GLTFData {
		cgltf_data* data;
		size_t mesh_count;
	};
	struct MemoryManager;
	struct Graphics;

	struct GLTFMesh {
		GLTFMesh() {}
		
		GLTFMesh(const GLTFMesh& other) = default;
		GLTFMesh& operator = (const GLTFMesh& other) = default;

		Stream<char> name = { nullptr, 0 };
		Stream<float3> positions;
		Stream<float3> normals;
		Stream<float2> uvs;
		Stream<Color> colors;
		Stream<float4> skin_weights;
		Stream<uint4> skin_influences;
		Stream<unsigned int> indices;
	};

	// If it fails, it returns a data pointer nullptr
	// Cgltf uses const char* internally, so a conversion must be done
	ECSENGINE_API GLTFData LoadGLTFFile(Stream<wchar_t> path, AllocatorPolymorphic allocator = { nullptr }, CapacityStream<char>* error_message = nullptr);

	// If it fails it returns a data pointer nullptr
	ECSENGINE_API GLTFData LoadGLTFFile(Stream<char> path, AllocatorPolymorphic allocator = { nullptr }, CapacityStream<char>* error_message = nullptr);

	// If it fails it returns a data pointer nullptr
	ECSENGINE_API GLTFData LoadGLTFFileFromMemory(Stream<void> file_data, AllocatorPolymorphic allocator = { nullptr }, CapacityStream<char>* error_message = nullptr);

	ECSENGINE_API bool LoadMeshFromGLTF(
		GLTFData data,
		GLTFMesh& mesh,
		AllocatorPolymorphic allocator,
		unsigned int mesh_index = 0,
		bool invert_z_axis = true,
		CapacityStream<char>* error_message = nullptr
	);

	ECSENGINE_API bool LoadMaterialFromGLTF(
		GLTFData data,
		PBRMaterial& material,
		AllocatorPolymorphic allocator,
		unsigned int mesh_index = 0,
		CapacityStream<char>* error_message = nullptr
	);

	ECSENGINE_API bool LoadMeshesFromGLTF(
		GLTFData data,
		GLTFMesh* meshes,
		AllocatorPolymorphic allocator,
		bool invert_z_axis = true,
		CapacityStream<char>* error_message = nullptr
	);

	struct GLTFMeshBufferSizes {
		unsigned int count[ECS_MESH_BUFFER_COUNT] = { 0 };
		unsigned int index_count = 0;
		unsigned int name_count = 0;
		unsigned int submesh_name_count = 0;
	};

	// Returns true if the sizes could be determined.
	ECSENGINE_API bool DetermineMeshesFromGLTFBufferSizes(
		GLTFData data,
		GLTFMeshBufferSizes* sizes,
		CapacityStream<char>* error_message = nullptr
	);

	struct LoadCoalescedMeshFromGLTFOptions {
		AllocatorPolymorphic temporary_buffer_allocator = { nullptr };
		AllocatorPolymorphic permanent_allocator = { nullptr };
		CapacityStream<char>* error_message = nullptr;
		bool allocate_submesh_name = false;
		bool coalesce_submesh_name_allocations = true;
		float scale_factor = 1.0f;

		// If this is set, then it will deduce the AABBScalar of the object
		bool deduce_submesh_bounds = true;
		// If this is set, then it will calculate the midpoint of the object
		// and offset the vertices such that the origin is at the center of
		// the object
		bool center_object_midpoint = false;
	};

	// Coallesces on the CPU side the values to be directly copied to the GPU
	// This allows to have no synchronization when loading multithreadely multiple resources
	// There must be data.mesh_count meshes allocated. It will deallocate the buffer allocated
	// if it fails. The permanent allocator is used for submesh name allocations. If that is not desired,
	// then it will simply skip the allocator
	ECSENGINE_API bool LoadCoalescedMeshFromGLTF(
		GLTFData data,
		const GLTFMeshBufferSizes* sizes,
		GLTFMesh* mesh,
		Submesh* submeshes,
		bool invert_z_axis = true,
		LoadCoalescedMeshFromGLTFOptions* options = nullptr
	);

	// Coallesces on the CPU side the values to be directly copied to the GPU
	// This allows to have no synchronization when loading multithreadely multiple resources
	// There must be data.mesh_count meshes allocated. It will deallocate the buffer allocated
	// if it fails. It does the extra step of determining the sizes before the actual allocation is done
	ECSENGINE_API bool LoadCoalescedMeshFromGLTF(
		GLTFData data,
		GLTFMesh* mesh,
		Submesh* submeshes,
		bool invert_z_axis = true,
		LoadCoalescedMeshFromGLTFOptions* options = nullptr
	);
		
	// For each mesh it will create a separate material
	ECSENGINE_API bool LoadMaterialsFromGLTF(
		GLTFData data,
		CapacityStream<PBRMaterial>* materials,
		AllocatorPolymorphic allocator,
		CapacityStream<char>* error_message = nullptr
	);

	// It will discard materials that repeat - at most data.count materials can be created
	// The unsigned int stream will contain indices for each submesh in order to identify 
	// the material that is using
	ECSENGINE_API bool LoadDisjointMaterialsFromGLTF(
		GLTFData data,
		Stream<PBRMaterial>& materials,
		Stream<unsigned int>& submesh_material_index,
		AllocatorPolymorphic allocator,
		CapacityStream<char>* error_message = nullptr
	);

	// GLTFData contains the mesh count that can be used to determine the size of the GLTFMesh stream
	// It creates a pbr material for every mesh
	ECSENGINE_API bool LoadMeshesAndMaterialsFromGLTF(
		GLTFData data,
		GLTFMesh* meshes,
		PBRMaterial* materials,
		AllocatorPolymorphic allocator,
		bool invert_z_axis = true,
		CapacityStream<char>* error_message = nullptr
	);

	// GLTFData contains the mesh count that can be used to determine the size of the GLTFMesh stream
	// It creates only unique pbr materials and an index mask for each submesh to reference the original 
	// material
	ECSENGINE_API bool LoadMeshesAndDisjointMaterialsFromGLTF(
		GLTFData data,
		GLTFMesh* meshes,
		Stream<PBRMaterial>& materials,
		unsigned int* submesh_material_index,
		AllocatorPolymorphic allocator,
		bool invert_z_axis = true,
		CapacityStream<char>* error_message = nullptr
	);

	// Can run on multiple threads
	// Creates the appropriate vertex and index buffers
	// Currently misc_flags can be set to ECS_GRAPHICS_RESOURCE_SHARED to enable sharing of the vertex buffers across devices
	ECSENGINE_API Mesh GLTFMeshToMesh(
		Graphics* graphics, 
		const GLTFMesh& gltf_mesh, 
		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE,
		bool compute_bounding_box = true
	);

	// Can run on multiple threads
	// Creates the appropriate vertex and index buffers
	// Currently misc_flags can be set to ECS_GRAPHICS_RESOURCE_SHARED to enable sharing of the vertex buffers across devices
	ECSENGINE_API void GLTFMeshesToMeshes(
		Graphics* graphics, 
		const GLTFMesh* gltf_meshes, 
		Mesh* meshes, 
		size_t count, 
		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE
	);

	// SINGLE THREADED - relies on the context to copy the resources
	// Merges the submeshes that have the same material into the same buffer
	// Material count submeshes will be created
	// The returned mesh will have no name associated with it
	// The submeshes will inherit the mesh name
	// Currently misc_flags can be set to ECS_GRAPHICS_RESOURCE_SHARED to enable sharing of the vertex buffers across devices
	// The gltf_meshes indices_offset value will be modified
	ECSENGINE_API Mesh GLTFMeshesToMergedMesh(
		Graphics* graphics, 
		Stream<GLTFMesh> gltf_meshes, 
		Submesh* submeshes,
		unsigned int* submesh_material_index, 
		size_t material_count,
		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE
	);

	// SINGLE THREADED - relies on the context to copy the resources
	// Merges the submeshes that have the same material into the same buffer
	// Material count submeshes will be created
	// The returned mesh will have no name associated with it
	// The submeshes will inherit the mesh name
	// Currently misc_flags can be set to ECS_GRAPHICS_RESOURCE_SHARED to enable sharing of the vertex buffers across devices
	ECSENGINE_API Mesh GLTFMeshesToMergedMesh(
		Graphics* graphics,
		Stream<GLTFMesh> gltf_meshes,
		Submesh* submeshes,
		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE
	);

	// Returns a coalesced mesh with 0 submeshes if it fails.
	// It needs a temporary buffer allocator such that it can coallesce the buffers
	// on the CPU side and them upload them directly to GPU. The coalesced_mesh_allocator
	// is used for the CoalescedMesh submesh allocation
	ECSENGINE_API CoalescedMesh LoadCoalescedMeshFromGLTFToGPU(
		Graphics* graphics,
		GLTFData gltf_data,
		AllocatorPolymorphic coalesced_mesh_allocator,
		bool invert_z_axis,
		LoadCoalescedMeshFromGLTFOptions* options = nullptr
	);

	// Fills in the given coalesced_mesh and its submeshes
	// It needs a temporary buffer allocator such that it can coallesce the buffers
	// on the CPU side and them upload them directly to GPU.
	ECSENGINE_API bool LoadCoalescedMeshFromGLTFToGPU(
		Graphics* graphics,
		GLTFData gltf_data,
		CoalescedMesh* coalesced_mesh,
		bool invert_z_axis,
		LoadCoalescedMeshFromGLTFOptions* options = nullptr
	);

	ECSENGINE_API void FreeGLTFMesh(const GLTFMesh& mesh, AllocatorPolymorphic allocator);

	ECSENGINE_API void FreeGLTFMeshes(Stream<GLTFMesh> meshes, AllocatorPolymorphic allocator);

	ECSENGINE_API void FreeCoalescedGLTFMesh(const GLTFMesh& mesh, AllocatorPolymorphic allocator);

	ECSENGINE_API void FreeGLTFFile(GLTFData data);

	ECSENGINE_API void ScaleGLTFMeshes(Stream<GLTFMesh> meshes, float scale_factor);
	
	ECSENGINE_API AABBScalar GetGLTFMeshBoundingBox(const GLTFMesh* mesh);

	ECSENGINE_API void GetGLTFMeshesBoundingBox(Stream<GLTFMesh> meshes, AABBScalar* bounding_boxes);

	ECSENGINE_API AABBScalar GetGLTFMeshesCombinedBoundingBox(Stream<GLTFMesh> meshes);

	// Updates the location of the vertices such that the origin is at the center of the object
	// Returns the translation that was performed (the center of the previous mesh)
	ECSENGINE_API float3 GLTFMeshOriginToCenter(const GLTFMesh* mesh);

	namespace internal {

		// Returns true if a material was found, else false
		// The functor receives as parameters 
		// (Stream<char> material_name, size_t mapping_count, PBRMaterialMapping* mappings, Stream<void>* texture_data, TextureExtension* texture_extensions)
		// Mappings, texture_data, texture_extensions are parallel arrays that have the count mapping_count
		template<typename Functor>
		bool ForMaterialFromGLTF(
			const cgltf_node* nodes,
			unsigned int node_index,
			PBRMaterial& material,
			Functor&& functor
		) {
			unsigned int primitive_count = nodes[node_index].mesh->primitives_count;

			for (size_t primitive_index = 0; primitive_index < primitive_count; primitive_index++) {
				const cgltf_material* gltf_material = nodes[node_index].mesh->primitives[primitive_index].material;

				if (gltf_material != nullptr) {
					// material name
					Stream<char> material_name = gltf_material->name;

					ECS_STACK_CAPACITY_STREAM(wchar_t, temp_texture_names, 1024);

					// Parallel arrays
					PBRMaterialMapping mappings[ECS_PBR_MATERIAL_MAPPING_COUNT];
					Stream<void> texture_mapping_data[ECS_PBR_MATERIAL_MAPPING_COUNT];
					TextureExtension texture_extensions[ECS_PBR_MATERIAL_MAPPING_COUNT];
					size_t mapping_count = 0;

					auto add_mapping = [&](Stream<char> texture_name, PBRMaterialTextureIndex mapping, Stream<void> texture_data, TextureExtension texture_extension) {
						unsigned int old_texture_size = temp_texture_names.size;
						// If it has an extension, remove it
						Stream<char> dot = FindFirstCharacter(texture_name, '.');
						if (dot.size > 0) {
							texture_name = texture_name.StartDifference(dot);
						}
						
						ConvertASCIIToWide(temp_texture_names, texture_name);
						mappings[mapping_count].texture = { temp_texture_names.buffer + old_texture_size, texture_name.size };
						mappings[mapping_count].index = mapping;
						texture_mapping_data[mapping_count] = texture_data;
						texture_extensions[mapping_count] = texture_extension;
						mapping_count++;
					};

					auto get_texture_mapping_data = [&](const cgltf_texture* texture) {
						if (texture != nullptr) {
							if (texture->image->buffer_view != nullptr) {
								if (texture->image->buffer_view->buffer != nullptr) {
									const void* buffer_pointer = texture->image->buffer_view->buffer->data;
									size_t offset = texture->image->buffer_view->offset;
									size_t size = texture->image->buffer_view->size;
									return Stream<void>(
										OffsetPointer(buffer_pointer, offset),
										size
									);
								}
							}
						}
						return Stream<void>(nullptr, 0);
					};

					auto get_extension = [](const cgltf_texture* texture) {
						Stream<char> mime_type = texture->image->mime_type;
						if (mime_type == "image/jpeg" || mime_type == "image/jpg") {
							return ECS_TEXTURE_EXTENSION_JPG;
						}
						else if (mime_type == "image/png") {
							return ECS_TEXTURE_EXTENSION_PNG;
						}
						else {
							return ECS_TEXTURE_EXTENSION_COUNT;
						}
					};

					// Returns true if it has the texture, else false
					auto add_texture_basic = [&](const cgltf_texture* texture, PBRMaterialTextureIndex texture_index) {
						bool has_texture = false;
						if (texture != nullptr) {
							if (texture->image->name != nullptr) {
								add_mapping(texture->image->name, texture_index, get_texture_mapping_data(texture), get_extension(texture));
								has_texture = true;
							}
							else {
								// Check the buffer view
								if (texture->image->buffer_view != nullptr) {
									if (texture->image->buffer_view->name != nullptr) {
										add_mapping(texture->image->buffer_view->name, texture_index, get_texture_mapping_data(texture), get_extension(texture));
										has_texture = true;
									}
								}
							}
						}

						return has_texture;
					};

					// Occlusion, Roughness, Metallic are packed into a single texture
					// The channels are in that order: R - Occlusion, G - Roughness, B - Metallic
					Stream<char> roughness_roots[] = {
						"roughness",
						"Roughness",
						"rough",
						"Rough"
					};

					Stream<char> metallic_roots[] = {
						"metallic",
						"Metallic",
						"met",
						"Met"
					};

					Stream<char> roughness_root;
					Stream<char> metallic_root;

					material.emissive_factor = gltf_material->emissive_factor;
					if (gltf_material->has_pbr_metallic_roughness) {
						const cgltf_pbr_metallic_roughness* pbr = &gltf_material->pbr_metallic_roughness;

						material.tint = Color(pbr->base_color_factor);
						material.metallic_factor = pbr->metallic_factor;
						material.roughness_factor = pbr->roughness_factor;

						if (!add_texture_basic(pbr->base_color_texture.texture, ECS_PBR_MATERIAL_COLOR)) {
							// Set non existing texture
							material.color_texture = { nullptr, 0 };
						}

						bool has_metallic_roughness = false;
						auto determine_metallic_roughness = [&](Stream<char> name, const cgltf_texture* texture) {
							Stream<char> hyphon = FindFirstCharacter(name, '-');
							TextureExtension texture_extension = get_extension(texture);
							Stream<void> texture_data = get_texture_mapping_data(texture);

							if (hyphon.size > 0) {
								has_metallic_roughness = true;
								Stream<char> metallic_name = name.StartDifference(hyphon);
								add_mapping(metallic_name, ECS_PBR_MATERIAL_METALLIC, texture_data, texture_extension);
								add_mapping(hyphon.AdvanceReturn(), ECS_PBR_MATERIAL_ROUGHNESS, texture_data, texture_extension);
							}
							else {
								// Try the second technique - search for the roughness/Roughness strings and use the part of the metallic name
								// without the metallic suffix
								for (size_t index = 0; index < ECS_COUNTOF(roughness_roots) && roughness_root.size == 0; index++) {
									roughness_root = FindFirstToken(name, roughness_roots[index]);
									roughness_root.size = roughness_root.size > 0 ? roughness_roots[index].size : 0;
								}

								if (roughness_root.size > 0) {
									Stream<char> metallic_name = name.StartDifference(roughness_root);
									for (size_t index = 0; index < ECS_COUNTOF(metallic_roots) && metallic_root.size == 0; index++) {
										metallic_root = FindFirstToken(name, metallic_roots[index]);
										metallic_root.size = metallic_root.size > 0 ? metallic_roots[index].size : 0;
									}

									has_metallic_roughness = true;
									if (metallic_root.size > 0) {
										Stream<char> metallic_stem = metallic_name.StartDifference(metallic_root);
										ECS_STACK_CAPACITY_STREAM(char, temp_roughness_name, 512);
										temp_roughness_name.CopyOther(metallic_stem);
										temp_roughness_name.AddStreamAssert(roughness_root);
										add_mapping(metallic_name, ECS_PBR_MATERIAL_METALLIC, texture_data, texture_extension);
										add_mapping(temp_roughness_name, ECS_PBR_MATERIAL_ROUGHNESS, texture_data, texture_extension);
									}
									else {
										// The metallic is missing, replace the roughness root with the metallic root
										ECS_STACK_CAPACITY_STREAM(char, temp_metallic_name, 512);
										temp_metallic_name.CopyOther(name);
										temp_metallic_name = ReplaceToken(temp_metallic_name, roughness_root, metallic_roots[0]);
										add_mapping(temp_metallic_name, ECS_PBR_MATERIAL_METALLIC, texture_data, texture_extension);
										add_mapping(name, ECS_PBR_MATERIAL_ROUGHNESS, texture_data, texture_extension);
									}
								}
								else {
									for (size_t index = 0; index < ECS_COUNTOF(metallic_roots) && metallic_root.size == 0; index++) {
										metallic_root = FindFirstToken(name, metallic_roots[index]);
										metallic_root.size = metallic_root.size > 0 ? metallic_roots[index].size : 0;
									}

									if (metallic_root.size > 0) {
										has_metallic_roughness = true;

										// The roughness is missing, replace the metallic root with the roughness root
										ECS_STACK_CAPACITY_STREAM(char, temp_roughness_name, 512);
										temp_roughness_name.CopyOther(name);
										temp_roughness_name = ReplaceToken(temp_roughness_name, metallic_root, roughness_roots[0]);
										add_mapping(temp_roughness_name, ECS_PBR_MATERIAL_ROUGHNESS, texture_data, texture_extension);
										add_mapping(name, ECS_PBR_MATERIAL_METALLIC, texture_data, texture_extension);
									}
								}
							}
						};

						if (pbr->metallic_roughness_texture.texture != nullptr) {
							const cgltf_texture* metallic_roughness_texture = pbr->metallic_roughness_texture.texture;
							if (metallic_roughness_texture->image->name != nullptr) {
								determine_metallic_roughness(metallic_roughness_texture->image->name, metallic_roughness_texture);
							}
							else {
								// Check the buffer view
								if (metallic_roughness_texture->image->buffer_view != nullptr) {
									if (metallic_roughness_texture->image->buffer_view->name != nullptr) {
										determine_metallic_roughness(metallic_roughness_texture->image->buffer_view->name, metallic_roughness_texture);
									}
								}
							}
						}

						if (!has_metallic_roughness) {
							// Set non existing texture
							material.metallic_texture = { nullptr, 0 };
							material.roughness_texture = { nullptr, 0 };
						}
					}
					// Set non existing textures
					else {
						material.color_texture = { nullptr, 0 };
						material.metallic_texture = { nullptr, 0 };
						material.roughness_texture = { nullptr, 0 };
					}

					if (!add_texture_basic(gltf_material->emissive_texture.texture, ECS_PBR_MATERIAL_EMISSIVE)) {
						// Set non existing texture
						material.emissive_texture = { nullptr, 0 };
					}

					if (!add_texture_basic(gltf_material->normal_texture.texture, ECS_PBR_MATERIAL_NORMAL)) {
						// Set non existing texture
						material.normal_texture = { nullptr, 0 };
					}

					bool has_occlusion_texture = false;
					const cgltf_texture* occlusion_texture = gltf_material->occlusion_texture.texture;
					if (occlusion_texture != nullptr) {
						auto process_occlusion_texture = [&](Stream<char> name) {
							auto add = [&](Stream<char> name) {
								Stream<void> texture_data = get_texture_mapping_data(occlusion_texture);
								TextureExtension texture_extension = get_extension(occlusion_texture);
								has_occlusion_texture = true;
								add_mapping(name, ECS_PBR_MATERIAL_OCCLUSION, texture_data, texture_extension);
							};

							ECS_STACK_CAPACITY_STREAM(char, temp_name, 512);
							if (roughness_root.size > 0 && metallic_root.size > 0) {
								unsigned int roughness_root_offset = roughness_root.buffer - name.buffer;
								unsigned int metallic_root_offset = metallic_root.buffer - name.buffer;

								temp_name.CopyOther(name);
								if (roughness_root_offset < metallic_root_offset) {
									temp_name.Remove(metallic_root_offset, metallic_root.size);
									temp_name = ReplaceToken(temp_name, roughness_root, "occlusion");
								}
								else {
									temp_name.Remove(roughness_root_offset, roughness_root.size);
									temp_name = ReplaceToken(temp_name, metallic_root, "occlusion");
								}

								add(temp_name);
							}
							else if (roughness_root.size > 0) {
								temp_name.CopyOther(name);
								temp_name = ReplaceToken(temp_name, roughness_root, "occlusion");

								add(temp_name);
							}
							else if (metallic_root.size > 0) {
								temp_name.CopyOther(name);
								temp_name = ReplaceToken(temp_name, metallic_root, "occlusion");

								add(temp_name);
							}
							// If there is no roughness_root or metallic_root, don't include the occlusion
						};

						if (occlusion_texture->image->name != nullptr) {
							process_occlusion_texture(occlusion_texture->image->name);
						}
						else {
							// Check the buffer view
							if (occlusion_texture->image->buffer_view != nullptr) {
								if (occlusion_texture->image->buffer_view->name != nullptr) {
									process_occlusion_texture(occlusion_texture->image->buffer_view->name);
								}
							}
						}
					}

					if (!has_occlusion_texture) {
						// Set non existing texture
						material.occlusion_texture = { nullptr,0 };
					}

					functor(material_name, mapping_count, mappings, texture_mapping_data, texture_extensions);
					return true;
				}
			}
			return false;
		}

		// The functor receives a const cgltf_node* nodes (the beginning of all nodes), a size_t node index and a size_t node count
		// It must return false if an error was encountered
		template<typename Functor>
		bool ForEachMeshInGLTF(
			GLTFData data,
			Functor&& functor
		) {
			size_t node_count = data.data->nodes_count;
			const cgltf_node* nodes = data.data->nodes;

			unsigned int current_mesh_index = 0;
			for (size_t index = 0; index < node_count; index++) {
				if (nodes[index].mesh != nullptr) {
					if (!functor(nodes, index, node_count)) {
						return false;
					}
				}
			}
			return true;
		}

	}

}