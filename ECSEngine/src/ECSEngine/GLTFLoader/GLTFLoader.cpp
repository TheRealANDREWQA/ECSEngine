// Credit to Hands-On Game Animation Programming by Gabor Szauer
#include "ecspch.h"
#include "GLTFLoader.h"
#include "../Utilities/FunctionTemplates.h"
#include "../Rendering/RenderingStructures.h"
#include "../Utilities/FunctionInterfaces.h"
#include "../Rendering/Graphics.h"
#include "../Allocators/AllocatorPolymorphic.h"

ECS_CONTAINERS;

namespace ECSEngine {

	// Helpers
	namespace {

		// -------------------------------------------------------------------------------------------------------------------------------

		Stream<float> GetScalarValues(
			AllocatorPolymorphic allocator,
			const cgltf_accessor* accessor, 
			unsigned int component_count,
			bool* success
		) {
			Stream<float> values;

			values.buffer = (float*)Allocate(allocator, component_count * accessor->count * sizeof(float));
			values.size = accessor->count * component_count;
			for (size_t index = 0; index < accessor->count; index++) {
				*success &= cgltf_accessor_read_float(accessor, index, values.buffer + index * component_count, component_count);
			}

			return values;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		unsigned int GetNodeIndex(const cgltf_node* target, const cgltf_node* nodes, unsigned int node_count) {
			if (target == nullptr) {
				return -1;
			}

			size_t difference = target - nodes;
			if (difference < node_count) {
				return difference;
			}

			return -1;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		// Consider ignoring the tangents
		bool MeshFromAttribute(
			GLTFMesh& mesh,
			const cgltf_attribute* attribute,
			const cgltf_skin* skin,
			const cgltf_node* nodes,
			unsigned int current_nodex_index,
			unsigned int node_count,
			AllocatorPolymorphic allocator,
			CapacityStream<char>* error_message
		) {
			cgltf_attribute_type attribute_type = attribute->type;
			const cgltf_accessor* accessor = attribute->data;
			unsigned int component_count = 0;

			bool is_valid = attribute_type != cgltf_attribute_type_invalid;

			if (!is_valid) {
				if (error_message != nullptr) {
					ECS_FORMAT_STRING(*error_message, "Mesh attribute has invalid type. Attribute name is {0}.", attribute->name);
				}
				return false;
			}

			if (accessor->type != cgltf_type_vec2 && accessor->type != cgltf_type_vec3 && accessor->type != cgltf_type_vec4) {
				if (error_message != nullptr) {
					ECS_FORMAT_STRING(*error_message, "Mesh data type is invalid. Expected float2, float3 or float4 but found {0}.", accessor->type);
				}
				return false;
			}
			component_count = accessor->type;

			Stream<float> values = GetScalarValues(allocator, accessor, component_count, &is_valid);
			if (!is_valid) {
				if (error_message != nullptr) {
					ECS_FORMAT_STRING(*error_message, "Mesh data values are invalid. Attribute name is {0}.", attribute->name);
				}
				return false;
			}
			unsigned int accessor_count = accessor->count;

			switch (attribute_type) {
			case cgltf_attribute_type_position: {
				mesh.positions = Stream<float3>(values.buffer, accessor_count);
				float world_matrix[16];
				cgltf_node_transform_world(nodes + current_nodex_index, world_matrix);

				Matrix ecs_matrix(world_matrix);
				if (ecs_matrix != MatrixIdentity()) {
					size_t count = mesh.positions.size;
					size_t index = 0;

					constexpr size_t MASK_LAST_BIT = ~0x0000000000000001;
					size_t masked_count = count & MASK_LAST_BIT;
					for (; index < masked_count; index += 2) {
						TransformPoint(mesh.positions[index], mesh.positions[index + 1], ecs_matrix);
					}
					if (index < count) {
						mesh.positions[count - 1] = TransformPoint(mesh.positions[count - 1], ecs_matrix);
					}
				}
				break;
			}
			case cgltf_attribute_type_texcoord:
				mesh.uvs = Stream<float2>(values.buffer, accessor_count);
				break;
			case cgltf_attribute_type_weights:
				mesh.skin_weights = Stream<float4>(values.buffer, accessor_count);
				break;
			case cgltf_attribute_type_normal:
			{
				mesh.normals = Stream<float3>(values.buffer, accessor_count);
				float matrix[16];
				cgltf_node_transform_world(nodes + current_nodex_index, matrix);
				Vector4 last_row = VectorGlobals::QUATERNION_IDENTITY_4;
				Vector8 extended_last_row(last_row, ZeroVector4());

				Matrix ecs_matrix(matrix);
				ecs_matrix.v[1].value = blend8<0, 1, 2, 3, 8, 9, 10, 11>(ecs_matrix.v[1].value, extended_last_row);

				Vector4 tolerance(0.000001f);
				if (ecs_matrix != MatrixIdentity()) {
					for (size_t index = 0; index < accessor_count; index++) {
						Vector4 normal(mesh.normals[index]);
						//normal = permute4<0, 2, 1, V_DC>(normal);
						if (horizontal_and((SquareLength3(normal) < tolerance))) {
							normal = VectorGlobals::UP_4;
							/*if (error_message != nullptr) {
								ECS_FORMAT_STRING(*error_message, "Mesh normal data is invalid. A normal has a squared length smaller than the tolerance. Index is {0}", index);
							}
							return false;*/
						}

						// Transform the normal
						normal = MatrixVectorMultiply(normal, ecs_matrix);

						normal = Normalize3(normal);
						normal.StorePartialConstant<3>(mesh.normals.buffer + index);
					}
				}
				else {
					for (size_t index = 0; index < accessor_count; index++) {
						Vector4 normal(mesh.normals[index]);
						//normal = permute4<0, 2, 1, V_DC>(normal);
						if (horizontal_and((SquareLength3(normal) < tolerance))) {
							normal = VectorGlobals::UP_4;
							/*if (error_message != nullptr) {
								ECS_FORMAT_STRING(*error_message, "Mesh normal data is invalid. A normal has a squared length smaller than the tolerance. Index is {0}", index);
							}
							return false;*/
						}
						normal = Normalize3(normal);
						normal.StorePartialConstant<3>(mesh.normals.buffer + index);
					}
				}
				break;
			}
			case cgltf_attribute_type_joints:
			{
				// These indices are skin relative. This function has no information about the
				// skin that is being parsed. Add +0.5f to round, since we can't read integers
				mesh.skin_influences = Stream<uint4>(values.buffer, accessor_count);
				for (size_t index = 0; index < accessor_count; index++) {
					unsigned int buffer_index = index * component_count;
					uint4 joints(
						(unsigned int)(values[buffer_index + 0] + 0.5f),
						(unsigned int)(values[buffer_index + 1] + 0.5f),
						(unsigned int)(values[buffer_index + 2] + 0.5f),
						(unsigned int)(values[buffer_index + 3] + 0.5f)
					);

					joints.x = GetNodeIndex(skin->joints[joints.x], nodes, node_count);
					joints.y = GetNodeIndex(skin->joints[joints.y], nodes, node_count);
					joints.z = GetNodeIndex(skin->joints[joints.z], nodes, node_count);
					joints.w = GetNodeIndex(skin->joints[joints.w], nodes, node_count);

					is_valid &= joints.x != -1 && joints.y != -1 && joints.z != -1 && joints.w != -1;

					if (!is_valid) {
						if (error_message != nullptr) {
							ECS_FORMAT_STRING(*error_message, "Mesh skin influences are invalid. Joint index is {0}.", index);
						}
						return false;
					}

					joints.x = function::ClampMax(0u, joints.x);
					joints.y = function::ClampMax(0u, joints.y);
					joints.z = function::ClampMax(0u, joints.z);
					joints.w = function::ClampMax(0u, joints.w);

					mesh.skin_influences[index] = joints;
				}
				break;
			}
			case cgltf_attribute_type_tangent:
			{
				mesh.tangents = Stream<float4>(values.buffer, accessor_count);
				float matrix[16];
				cgltf_node_transform_world(nodes + current_nodex_index, matrix);

				Matrix ecs_matrix(matrix);

				Vector4 tolerance(0.000001f);
				if (ecs_matrix != MatrixIdentity()) {
					for (size_t index = 0; index < accessor_count; index++) {
						Vector4 tangent(mesh.tangents[index]);
						if (horizontal_and((SquareLength3(tangent) < tolerance))) {
							tangent = VectorGlobals::RIGHT_4;
							/*if (error_message != nullptr) {
								ECS_FORMAT_STRING(*error_message, "Mesh tangent data is invalid. A tangent has a squared length smaller than the tolerance. Index is {0}", index);
							}
							return false;*/
						}

						// Transform the tangent
						tangent = MatrixVectorMultiply(tangent, ecs_matrix);

						tangent = Normalize3(tangent);
						tangent.StorePartialConstant<3>(mesh.tangents.buffer + index);
					}
				}
				else {
					for (size_t index = 0; index < accessor_count; index++) {
						Vector4 tangent(mesh.tangents[index]);
						if (horizontal_and((SquareLength3(tangent) < tolerance))) {
							tangent = VectorGlobals::RIGHT_4;
							/*if (error_message != nullptr) {
								ECS_FORMAT_STRING(*error_message, "Mesh tangent data is invalid. A tangent has a squared length smaller than the tolerance. Index is {0}", index);
							}
							return false;*/
						}
						tangent = Normalize3(tangent);
						tangent.StorePartialConstant<3>(mesh.tangents.buffer + index);
					}
				}
				break;
			}
			// TODO: Determine if the colors are stored in float or in unsigned char fashion
			case cgltf_attribute_type_color:
			{
				mesh.colors = Stream<Color>(values.buffer, accessor_count);
				break;
			}
			}

			return true;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		bool ValidateMesh(const GLTFMesh& mesh) {
			// Validation - all vertex buffers must have the same size
			size_t size = mesh.positions.size;

			if (mesh.normals.buffer != nullptr) {
				if (size != mesh.normals.size) {
					return false;
				}
			}

			if (mesh.skin_influences.buffer != nullptr) {
				if (size != mesh.skin_influences.size) {
					return false;
				}
			}

			if (mesh.skin_weights.buffer != nullptr) {
				if (size != mesh.skin_weights.size) {
					return false;
				}
			}

			if (mesh.uvs.buffer != nullptr) {
				if (size != mesh.uvs.size) {
					return false;
				}
			}

			return true;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		size_t GLTFMeshCount(const cgltf_data* data) {
			size_t count = 0;

			unsigned int node_count = data->nodes_count;
			const cgltf_node* nodes = data->nodes;

			for (size_t index = 0; index < node_count; index++) {
				if (nodes[index].mesh != nullptr) {
					count += nodes[index].mesh->primitives_count;
				}
			}

			return count;
		}

		// -------------------------------------------------------------------------------------------------------------------------------
		Stream<char> GetMaterialName(const cgltf_primitive* primitives, unsigned int primitive_index) {
			const cgltf_material* gltf_material = primitives[primitive_index].material;

			return ToStream(gltf_material->name);
		}

	}

	// -------------------------------------------------------------------------------------------------------------------------------

	GLTFData LoadGLTFFile(const char* path, CapacityStream<char>* error_message)
	{
		GLTFData data;
		data.mesh_count = 0;

		cgltf_options options;
		memset(&options, 0, sizeof(cgltf_options));
		cgltf_result result = cgltf_parse_file(&options, path, &data.data);

		if (result != cgltf_result_success) {
			if (error_message != nullptr) {
				ECS_FORMAT_STRING(*error_message, "Could not load {0}. Parsing failed.", path);
			}
			data.data = nullptr;
			return data;
		}
		result = cgltf_load_buffers(&options, data.data, path);
		if (result != cgltf_result_success) {
			cgltf_free(data.data);
			if (error_message != nullptr) {
				ECS_FORMAT_STRING(*error_message, "Could not load {0}. Loading buffers failed.", path);
			}
			data.data = nullptr;
			return data;
		}
		result = cgltf_validate(data.data);
		if (result != cgltf_result_success) {
			cgltf_free(data.data);
			if (error_message != nullptr) {
				ECS_FORMAT_STRING(*error_message, "Invalid file {0}. Validation failed.", path);
			}
			data.data = nullptr;
			return data;
		}

		data.mesh_count = GLTFMeshCount(data.data);
		
		return data;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	GLTFData LoadGLTFFile(Stream<char> path, CapacityStream<char>* error_message)
	{
		ECS_TEMP_ASCII_STRING(temp_path, 512);
		temp_path.Copy(path);
		temp_path[path.size] = '\0';
		return LoadGLTFFile(temp_path.buffer, error_message);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	GLTFData LoadGLTFFile(const wchar_t* path, CapacityStream<char>* error_message) 
	{
		return LoadGLTFFile(ToStream(path), error_message);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	GLTFData LoadGLTFFile(Stream<wchar_t> path, CapacityStream<char>* error_message)
	{
		ECS_TEMP_ASCII_STRING(temp_path, 512);
		function::ConvertWideCharsToASCII(path, temp_path);
		temp_path[temp_path.size] = '\0';
		return LoadGLTFFile(temp_path.buffer, error_message);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMeshFromGLTF(
		GLTFMesh& mesh, 
		AllocatorPolymorphic allocator,
		const cgltf_node* nodes, 
		unsigned int node_index, 
		unsigned int node_count, 
		bool invert_z_axis,
		CapacityStream<char>* error_message
	) {
		unsigned int primitive_count = nodes[node_index].mesh->primitives_count;

		for (size_t primitive_index = 0; primitive_index < primitive_count; primitive_index++) {
			const cgltf_primitive* primitive = &nodes[node_index].mesh->primitives[primitive_index];
			unsigned int attribute_count = primitive->attributes_count;

			for (unsigned int attribute_index = 0; attribute_index < attribute_count; attribute_index++) {
				const cgltf_attribute* attribute = &primitive->attributes[attribute_index];
				bool is_valid = MeshFromAttribute(
					mesh,
					attribute,
					nodes[node_index].skin,
					nodes,
					node_index,
					node_count,
					allocator,
					error_message
				);
				if (!is_valid) {
					return false;
				}
			}

			mesh.name = nodes[node_index].name;

			if (primitive->indices != nullptr) {
				unsigned int index_count = primitive->indices->count;

				mesh.indices = Stream<unsigned int>(Allocate(allocator, (sizeof(unsigned int) * index_count)), index_count);
				for (unsigned int index_index = 0; index_index < index_count; index_index++) {
					mesh.indices[index_index] = cgltf_accessor_read_index(primitive->indices, index_index);
				}
			}

			// Invert the z axis if necessary
			if (invert_z_axis) {
				// Invert the position Z axis
				for (size_t subindex = 0; subindex < mesh.positions.size; subindex++) {
					mesh.positions[subindex].z = -mesh.positions[subindex].z;
				}

				// Invert the normals Z axis
				for (size_t subindex = 0; subindex < mesh.normals.size; subindex++) {
					mesh.normals[subindex].z = -mesh.normals[subindex].z;
				}

				// Invert the tangents Z axis
				for (size_t subindex = 0; subindex < mesh.tangents.size; subindex++) {
					mesh.tangents[subindex].z = -mesh.tangents[subindex].z;
					mesh.tangents[subindex].w = -mesh.tangents[subindex].w;
				}

				// The winding order of the vertices must be changed
				// Start from 1 in order to avoid an add
				for (size_t subindex = 1; subindex < mesh.indices.size; subindex += 3) {
					// Swap the last the vertices of each triangle
					mesh.indices.Swap(subindex, subindex + 1);
				}
			}
		}
		return true;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMaterialFromGLTF(
		PBRMaterial& material,
		AllocatorPolymorphic allocator,
		const cgltf_node* nodes,
		unsigned int node_index,
		CapacityStream<char>* error_message
	) {
		unsigned int primitive_count = nodes[node_index].mesh->primitives_count;

		for (size_t primitive_index = 0; primitive_index < primitive_count; primitive_index++) {
			const cgltf_material* gltf_material = nodes[node_index].mesh->primitives[primitive_index].material;

			if (gltf_material != nullptr) {
				// material name
				Stream<char> material_name = ToStream(gltf_material->name);

				ECS_TEMP_STRING(temp_texture_names, 1024);

				PBRMaterialMapping mappings[ECS_PBR_MATERIAL_MAPPING_COUNT];
				size_t mapping_count = 0;

				auto add_mapping = [&](const char* name, PBRMaterialTextureIndex mapping) {
					Stream<char> texture_name = ToStream(name);
					unsigned int old_texture_size = temp_texture_names.size;
					function::ConvertASCIIToWide(temp_texture_names, texture_name);
					mappings[mapping_count].texture = { temp_texture_names.buffer + old_texture_size, texture_name.size };
					mappings[mapping_count].index = mapping;
					mapping_count++;
				};

				material.emissive_factor = gltf_material->emissive_factor;
				if (gltf_material->has_pbr_metallic_roughness) {
					material.tint = Color(gltf_material->pbr_metallic_roughness.base_color_factor);
					material.metallic_factor = gltf_material->pbr_metallic_roughness.metallic_factor;
					material.roughness_factor = gltf_material->pbr_metallic_roughness.roughness_factor;
					if (gltf_material->pbr_metallic_roughness.base_color_texture.texture != nullptr) {
						if (gltf_material->pbr_metallic_roughness.base_color_texture.texture->image->name != nullptr) {
							add_mapping(gltf_material->pbr_metallic_roughness.base_color_texture.texture->image->name, ECS_PBR_MATERIAL_COLOR);
						}
						// Set non existing texture
						else {
							material.color_texture = { nullptr,0 };
						}
					}
					// Set non existing texture
					else {
						material.color_texture = { nullptr, 0 };
					}

					if (gltf_material->pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr) {
						if (gltf_material->pbr_metallic_roughness.metallic_roughness_texture.texture->image->name != nullptr) {
							char* hyphon = strchr(gltf_material->pbr_metallic_roughness.metallic_roughness_texture.texture->image->name, '-');
							if (hyphon != nullptr) {
								*hyphon = '\0';
								add_mapping(gltf_material->pbr_metallic_roughness.metallic_roughness_texture.texture->image->name, ECS_PBR_MATERIAL_METALLIC);
								add_mapping(hyphon + 1, ECS_PBR_MATERIAL_ROUGHNESS);
							}
							// Do not add it at all if it isn't specified - could be roughness, could be roughness
							// Set non existing texture
							else {
								material.metallic_texture = { nullptr, 0 };
								material.roughness_texture = { nullptr, 0 };
							}
						}
						// Set non existing texture
						else {
							material.metallic_texture = { nullptr, 0 };
							material.roughness_texture = { nullptr, 0 };
						}
					}
					// Set non existing texture
					else {
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

				if (gltf_material->emissive_texture.texture != nullptr) {
					if (gltf_material->emissive_texture.texture->image->name != nullptr) {
						add_mapping(gltf_material->emissive_texture.texture->image->name, ECS_PBR_MATERIAL_EMISSIVE);
					}
					// Set non existing texture
					else {
						material.emissive_texture = { nullptr, 0 };
					}
				}
				// Set non existing texture
				else {
					material.emissive_texture = { nullptr, 0 };
				}

				if (gltf_material->normal_texture.texture != nullptr) {
					if (gltf_material->normal_texture.texture->image->name != nullptr) {
						add_mapping(gltf_material->normal_texture.texture->image->name, ECS_PBR_MATERIAL_NORMAL);
					}
					// Set non existing texture
					else {
						material.normal_texture = { nullptr, 0 };
					}
				}
				// Set non existing texture
				else {
					material.normal_texture = { nullptr, 0 };
				}

				if (gltf_material->occlusion_texture.texture != nullptr) {
					if (gltf_material->occlusion_texture.texture->image->name != nullptr) {
						add_mapping(gltf_material->occlusion_texture.texture->image->name, ECS_PBR_MATERIAL_OCCLUSION);
					}
					// Set non existing texture
					else {
						material.occlusion_texture = { nullptr, 0 };
					}
				}
				// Set non existing texture
				else {
					material.occlusion_texture = { nullptr,0 };
				}

				AllocatePBRMaterial(material, material_name, Stream<PBRMaterialMapping>(mappings, mapping_count), allocator);
			}
		}
		return true;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMeshFromGLTF(
		GLTFData data,
		GLTFMesh& mesh, 
		AllocatorPolymorphic allocator,
		unsigned int mesh_index, 
		bool invert_z_axis,
		CapacityStream<char>* error_message
	) {
		unsigned int node_count = data.data->nodes_count;
		const cgltf_node* nodes = data.data->nodes;

		unsigned int current_mesh_index = 0;
		for (size_t index = 0; index < node_count; index++) {
			if (nodes[index].mesh != nullptr) {
				if (mesh_index == current_mesh_index) {
					return LoadMeshFromGLTF(mesh, allocator, nodes, index, node_count, invert_z_axis, error_message);
				}
				current_mesh_index++;
			}
		}

		if (error_message != nullptr) {
			ECS_FORMAT_STRING(*error_message, "No mesh with index {0} has been found. The file contains only {1}", mesh_index, current_mesh_index);
		}
		return false;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMaterialFromGLTF(GLTFData data, PBRMaterial& material, AllocatorPolymorphic allocator, unsigned int mesh_index, containers::CapacityStream<char>* error_message)
	{
		unsigned int node_count = data.data->nodes_count;
		const cgltf_node* nodes = data.data->nodes;

		unsigned int current_mesh_index = 0;
		for (size_t index = 0; index < node_count; index++) {
			if (nodes[index].mesh != nullptr) {
				if (mesh_index == current_mesh_index) {
					return LoadMaterialFromGLTF(material, allocator, nodes, index, error_message);
				}
				current_mesh_index++;
			}
		}

		if (error_message != nullptr) {
			ECS_FORMAT_STRING(*error_message, "No mesh with index {0} has been found. The file contains only {1}", mesh_index, current_mesh_index);
		}
		return false;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMeshesFromGLTF(
		GLTFData data,
		GLTFMesh* meshes,
		AllocatorPolymorphic allocator,
		bool invert_z_axis,
		containers::CapacityStream<char>* error_message
	) {
		unsigned int node_count = data.data->nodes_count;
		const cgltf_node* nodes = data.data->nodes;

		unsigned int mesh_index = 0;
		for (size_t index = 0; index < node_count; index++) {
			if (nodes[index].mesh != nullptr) {
				bool success = LoadMeshFromGLTF(
					meshes[mesh_index],
					allocator,
					nodes,
					index,
					node_count,
					invert_z_axis,
					error_message
				);
				if (!success) {
					ECS_FORMAT_TEMP_STRING(additional_info, "The mesh index is {0}", mesh_index);
					error_message->AddStreamSafe(additional_info);
					return false;
				}
				meshes[mesh_index].name = nodes[index].name;
				mesh_index++;
			}
		}

		return true;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMaterialsFromGLTF(GLTFData data, PBRMaterial* materials, AllocatorPolymorphic allocator, containers::CapacityStream<char>* error_message)
	{
		unsigned int node_count = data.data->nodes_count;
		const cgltf_node* nodes = data.data->nodes;

		size_t material_index = 0;
		for (size_t index = 0; index < node_count; index++) {
			if (nodes[index].mesh != nullptr) {
				bool success = LoadMaterialFromGLTF(materials[material_index], allocator, nodes, index, error_message);
				if (!success) {
					ECS_FORMAT_TEMP_STRING(additional_info, "The material index is {0}.", material_index);
					error_message->AddStreamSafe(additional_info);
					return false;
				}
				material_index++;
			}
		}

		return true;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadDisjointMaterial(
		const cgltf_node* nodes,
		unsigned int index,
		Stream<PBRMaterial>& materials,
		Stream<unsigned int>& submesh_material_index, 
		AllocatorPolymorphic allocator,
		CapacityStream<char>* error_message
	) {
		// Get the name
		Stream<char> material_name = GetMaterialName(nodes[index].mesh->primitives, 0);

		// If it already exists - do no reload it
		bool exists = false;
		for (size_t subindex = 0; subindex < materials.size && !exists; subindex++) {
			if (function::CompareStrings(material_name, ToStream(materials[subindex].name))) {
				submesh_material_index.Add(subindex);
				exists = true;
			}
		}

		if (!exists) {
			submesh_material_index.Add(materials.size);

			// Load the material as normally
			bool success = LoadMaterialFromGLTF(materials[materials.size], allocator, nodes, index, error_message);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(additional_info, "The material index is {0}.", materials.size);
				error_message->AddStreamSafe(additional_info);
				return false;
			}
			materials.size++;
		}
		return true;
	}

	bool LoadDisjointMaterialsFromGLTF(
		GLTFData data, 
		Stream<PBRMaterial>& materials, 
		Stream<unsigned int>& submesh_material_index, 
		AllocatorPolymorphic allocator, 
		CapacityStream<char>* error_message
	)
	{
		unsigned int node_count = data.data->nodes_count;
		const cgltf_node* nodes = data.data->nodes;

		for (size_t index = 0; index < node_count; index++) {
			if (nodes[index].mesh != nullptr) {
				if (nodes[index].mesh->primitives->material != nullptr) {
					bool success = LoadDisjointMaterial(nodes, index, materials, submesh_material_index, allocator, error_message);
					if (!success) {
						return false;
					}
				}
			}
		}

		return true;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMeshesAndMaterialsFromGLTF(
		GLTFData data,
		GLTFMesh* meshes, 
		PBRMaterial* materials,
		AllocatorPolymorphic allocator,
		bool invert_z_axis,
		CapacityStream<char>* error_message
	)
	{
		unsigned int node_count = data.data->nodes_count;
		const cgltf_node* nodes = data.data->nodes;

		size_t mesh_material_index = 0;
		for (size_t index = 0; index < node_count; index++) {
			if (nodes[index].mesh != nullptr) {
				bool success = LoadMeshFromGLTF(
					meshes[mesh_material_index],
					allocator,
					nodes,
					index,
					node_count,
					invert_z_axis,
					error_message
				);

				success &= LoadMaterialFromGLTF(
					materials[mesh_material_index],
					allocator,
					nodes,
					index,
					error_message
				);

				if (!success) {
					ECS_FORMAT_TEMP_STRING(additional_info, "The material index is {0}.", mesh_material_index);
					error_message->AddStreamSafe(additional_info);
					return false;
				}

				mesh_material_index++;
			}
		}

		return true;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMeshesAndDisjointMaterialsFromGLTF(
		GLTFData data,
		GLTFMesh* meshes,
		Stream<PBRMaterial>& materials,
		unsigned int* _submesh_material_index,
		AllocatorPolymorphic allocator,
		bool invert_z_axis,
		CapacityStream<char>* error_message
	)
	{
		unsigned int node_count = data.data->nodes_count;
		const cgltf_node* nodes = data.data->nodes;
		Stream<unsigned int> submesh_material_index(_submesh_material_index, 0);

		size_t mesh_index = 0;
		for (size_t index = 0; index < node_count; index++) {
			if (nodes[index].mesh != nullptr) {
				bool success = LoadMeshFromGLTF(
					meshes[mesh_index],
					allocator,
					nodes,
					index,
					node_count,
					invert_z_axis,
					error_message
				);

				if (nodes[index].mesh->primitives->material != nullptr) {
					success &= LoadDisjointMaterial(nodes, index, materials, submesh_material_index, allocator, error_message);
				}

				if (!success) {
					ECS_FORMAT_TEMP_STRING(additional_info, "The material index is {0}.", mesh_index);
					error_message->AddStreamSafe(additional_info);
					return false;
				}

				mesh_index++;
			}
		}

		return true;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	Mesh GLTFMeshToMesh(Graphics* graphics, const GLTFMesh& gltf_mesh)
	{
		Mesh mesh;

		if (gltf_mesh.name != nullptr) {
			mesh.name = function::StringCopy(graphics->m_allocator, gltf_mesh.name).buffer;
		}
		else {
			mesh.name = nullptr;
		}

		// Positions
		if (gltf_mesh.positions.buffer != nullptr) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_POSITION;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(float3), gltf_mesh.positions.size, gltf_mesh.positions.buffer);
		}

		// Normals
		if (gltf_mesh.normals.buffer != nullptr) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_NORMAL;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(float3), gltf_mesh.normals.size, gltf_mesh.normals.buffer);
		}

		// UVs
		if (gltf_mesh.uvs.buffer != nullptr) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_UV;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(float2), gltf_mesh.uvs.size, gltf_mesh.uvs.buffer);
		}

		// Vertex Colors
		if (gltf_mesh.colors.buffer != nullptr) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_COLOR;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(Color), gltf_mesh.colors.size, gltf_mesh.colors.buffer);
		}

		// Tangents
		if (gltf_mesh.tangents.buffer != nullptr) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_TANGENT;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(float4), gltf_mesh.tangents.size, gltf_mesh.tangents.buffer);
		}

		// Bone weights
		if (gltf_mesh.skin_weights.buffer != nullptr) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_BONE_WEIGHT;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(float4), gltf_mesh.skin_weights.size, gltf_mesh.skin_weights.buffer);
		}

		// Bone influences
		if (gltf_mesh.skin_influences.buffer != nullptr) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_BONE_INFLUENCE;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(uint4), gltf_mesh.skin_influences.size, gltf_mesh.skin_influences.buffer);
		}

		// Indices
		if (gltf_mesh.indices.buffer != nullptr) {
			mesh.index_buffer = graphics->CreateIndexBuffer(gltf_mesh.indices);
		}

		return mesh;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void GLTFMeshesToMeshes(Graphics* graphics, const GLTFMesh* gltf_meshes, Mesh* meshes, size_t count) {
		for (size_t index = 0; index < count; index++) {
			meshes[index] = GLTFMeshToMesh(graphics, gltf_meshes[index]);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	Mesh GLTFMeshesToMergedMesh(
		Graphics* graphics, 
		GLTFMesh* gltf_meshes, 
		Submesh* submeshes,
		unsigned int* submesh_material_index, 
		size_t material_count, 
		size_t count
	) {
		constexpr size_t COUNT_MAX = 2048;

		Mesh mesh;

		size_t total_vertex_buffer_count = 0;
		size_t total_index_buffer_count = 0;

		ECS_ASSERT(count <= COUNT_MAX);
		unsigned int* sorted_indices = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * count);

		// The x component will hold the total vertex count
		// The y component will hold the total index count
		uint2* material_counts = (uint2*)ECS_STACK_ALLOC(sizeof(uint2) * material_count);
		memset(material_counts, 0, sizeof(uint2) * material_count);

		size_t submesh_offset = 0;
		// For every material run through the submeshes to combine those that have the same material
		for (size_t index = 0; index < material_count; index++) {
			for (size_t subindex = 0; subindex < count; subindex++) {
				if (submesh_material_index[subindex] == index) {
					// Indices must be adjusted per material
					for (size_t indices_index = 0; indices_index < gltf_meshes[subindex].indices.size; indices_index++) {
						gltf_meshes[subindex].indices[indices_index] += material_counts[index].x;
					}

					total_vertex_buffer_count += gltf_meshes[subindex].positions.size;
					total_index_buffer_count += gltf_meshes[subindex].indices.size;

					material_counts[index].x += gltf_meshes[subindex].positions.size;
					material_counts[index].y += gltf_meshes[subindex].indices.size;

					// Keep a sorted list of the indices for every material
					sorted_indices[submesh_offset++] = subindex;
				}
			}
		}

		// Create the aggregate mesh
		
		auto create_vertex_type = [&](unsigned int sizeof_type, ECS_MESH_INDEX index, unsigned char stream_byte_offset) {
			mesh.mapping[mesh.mapping_count] = index;
			// Create a main vertex buffer that is DEFAULT usage and then copy into it smaller vertex buffers
			VertexBuffer vertex_buffer = graphics->CreateVertexBuffer(sizeof_type, total_vertex_buffer_count, {}, D3D11_USAGE_DEFAULT, 0);
			size_t vertex_buffer_offset = 0;

			for (size_t index = 0; index < count; index++) {
				void** stream_buffer = (void**)function::OffsetPointer(&gltf_meshes[sorted_indices[index]], stream_byte_offset);
				size_t* stream_size = (size_t*)function::OffsetPointer(&gltf_meshes[sorted_indices[index]], stream_byte_offset + sizeof(void*));
				size_t converted_stream_size = *stream_size * sizeof_type;

				// Create a staging buffer that can be copied to the main buffer
				size_t buffer_size = converted_stream_size;
				VertexBuffer current_buffer = graphics->CreateVertexBuffer(
					sizeof_type,
					buffer_size,
					*stream_buffer
				);
				CopyBufferSubresource(vertex_buffer, vertex_buffer_offset, current_buffer, 0, buffer_size, graphics->GetContext());
				vertex_buffer_offset += buffer_size;
				graphics->RemovePossibleResourceFromTracking(current_buffer.Interface());
				current_buffer.Release();
			}
			mesh.vertex_buffers[mesh.mapping_count++] = vertex_buffer;
		};

		// Positions
		if (gltf_meshes[0].positions.buffer != nullptr) {
			create_vertex_type(sizeof(float3), ECS_MESH_POSITION, offsetof(GLTFMesh, positions));
		}

		// Normals
		if (gltf_meshes[0].normals.buffer != nullptr) {
			create_vertex_type(sizeof(float3), ECS_MESH_NORMAL, offsetof(GLTFMesh, normals));
		}

		// UVs
		if (gltf_meshes[0].uvs.buffer != nullptr) {
			create_vertex_type(sizeof(float2), ECS_MESH_UV, offsetof(GLTFMesh, uvs));
		}

		// Vertex Colors
		if (gltf_meshes[0].colors.buffer != nullptr) {
			create_vertex_type(sizeof(Color), ECS_MESH_COLOR, offsetof(GLTFMesh, colors));
		}

		// Tangents
		if (gltf_meshes[0].tangents.buffer != nullptr) {
			create_vertex_type(sizeof(float3), ECS_MESH_TANGENT, offsetof(GLTFMesh, tangents));
		}

		// Bone weights
		if (gltf_meshes[0].skin_weights.buffer != nullptr) {
			create_vertex_type(sizeof(float4), ECS_MESH_BONE_WEIGHT, offsetof(GLTFMesh, skin_weights));
		}

		// Bone influences
		if (gltf_meshes[0].skin_influences.buffer != nullptr) {
			create_vertex_type(sizeof(uint4), ECS_MESH_BONE_INFLUENCE, offsetof(GLTFMesh, skin_influences));
		}

		// Indices
		if (gltf_meshes[0].indices.buffer != nullptr) {
			// Create a main index buffer that is DEFAULT usage and then copy into it the smaller index buffers
			mesh.index_buffer = graphics->CreateIndexBuffer(4, total_index_buffer_count);
			size_t index_buffer_offset = 0;

			for (size_t index = 0; index < count; index++) {
				// Create a staging buffer that can be copied to the main buffer
				size_t buffer_size = gltf_meshes[sorted_indices[index]].indices.size;
				IndexBuffer current_buffer = graphics->CreateIndexBuffer(gltf_meshes[sorted_indices[index]].indices);
				CopyBufferSubresource(mesh.index_buffer, index_buffer_offset, current_buffer, 0, buffer_size, graphics->GetContext());
				index_buffer_offset += buffer_size;
				graphics->RemovePossibleResourceFromTracking(current_buffer.Interface());
				current_buffer.Release();
			}
		}

		// Propragate the names
		for (size_t index = 0; index < count; index++) {
			if (gltf_meshes[index].name != nullptr) {
				submeshes[index].name = function::StringCopy(graphics->m_allocator, gltf_meshes[index].name).buffer;
			}
			else {
				submeshes[index].name = nullptr;
			}
		}

		size_t submesh_vertex_offset = 0;
		size_t submesh_index_offset = 0;
		// Submeshes
		for (size_t index = 0; index < material_count; index++) {
			submeshes[index].index_buffer_offset = submesh_index_offset;
			submeshes[index].vertex_buffer_offset = submesh_vertex_offset;
			submeshes[index].index_count = material_counts[index].y;
			submeshes[index].vertex_count = material_counts[index].x;
			submesh_vertex_offset += material_counts[index].x;
			submesh_index_offset += material_counts[index].y;
		}
		submeshes[material_count].index_buffer_offset = total_index_buffer_count;
		submeshes[material_count].vertex_buffer_offset = total_vertex_buffer_count;

		return mesh;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void FreeGLTFMesh(const GLTFMesh& mesh, AllocatorPolymorphic allocator) {
		if (mesh.positions.buffer != nullptr) {
			Deallocate(allocator, mesh.positions.buffer);
		}
		if (mesh.indices.buffer != nullptr) {
			Deallocate(allocator, mesh.indices.buffer);
		}
		if (mesh.normals.buffer != nullptr) {
			Deallocate(allocator, mesh.normals.buffer);
		}
		if (mesh.skin_influences.buffer != nullptr) {
			Deallocate(allocator, mesh.skin_influences.buffer);
		}
		if (mesh.skin_weights.buffer != nullptr) {
			Deallocate(allocator, mesh.skin_weights.buffer);
		}
		if (mesh.uvs.buffer != nullptr) {
			Deallocate(allocator, mesh.uvs.buffer);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void FreeGLTFMeshes(const GLTFMesh* meshes, size_t count, AllocatorPolymorphic allocator) {
		for (size_t index = 0; index < count; index++) {
			FreeGLTFMesh(meshes[index], allocator);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void FreeGLTFFile(GLTFData data)
	{
		cgltf_free(data.data);
	}

}