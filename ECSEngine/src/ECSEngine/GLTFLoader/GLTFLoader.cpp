// Credit to Hands-On Game Animation Programming by Gabor Szauer
#include "ecspch.h"
#include "GLTFLoader.h"
#include "../Utilities/FunctionTemplates.h"
#include "../Rendering/RenderingStructures.h"
#include "../Allocators/MemoryManager.h"
#include "../Utilities/FunctionInterfaces.h"
#include "../Rendering/Graphics.h"

ECS_CONTAINERS;

namespace ECSEngine {

	// Helpers
	namespace {

		Stream<float> GetScalarValues(MemoryManager* allocator, const cgltf_accessor* accessor, unsigned int component_count, bool* success) {
			Stream<float> values;

			values.buffer = (float*)allocator->Allocate(component_count * accessor->count * sizeof(float));
			values.size = accessor->count * component_count;
			for (size_t index = 0; index < accessor->count; index++) {
				*success &= cgltf_accessor_read_float(accessor, index, values.buffer + index * component_count, component_count);
			}

			return values;
		}

		Stream<float> GetScalarValuesTs(MemoryManager* allocator, const cgltf_accessor* accessor, unsigned int component_count, bool* success) {
			Stream<float> values;

			values.buffer = (float*)allocator->Allocate_ts(component_count * accessor->count * sizeof(float));
			values.size = 0;

			for (size_t index = 0; index < accessor->count; index++) {
				*success &= cgltf_accessor_read_float(accessor, index, values.buffer + index * component_count, component_count);
			}

			return values;
		}

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

		template<Stream<float> (*GetValues)(MemoryManager*, const cgltf_accessor*, unsigned int, bool*)>
		bool MeshFromAttributeImpl(
			GLTFMesh& mesh,
			const cgltf_attribute* attribute,
			const cgltf_skin* skin,
			const cgltf_node* nodes,
			unsigned int current_nodex_index,
			unsigned int node_count,
			MemoryManager* allocator,
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

			Stream<float> values = GetValues(allocator, accessor, component_count, &is_valid);
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

				Matrix ecs_matrix(matrix);

				Vector4 tolerance(0.000001f);
				if (ecs_matrix != MatrixIdentity()) {
					for (size_t index = 0; index < accessor_count; index++) {
						Vector4 normal(mesh.normals[index]);
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
				mesh.tangents = Stream<float3>(values.buffer, accessor_count);
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

						// Transform the normal
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

		// This will load a singular attribute e.g. position
		bool MeshFromAttribute(
			GLTFMesh& mesh,
			const cgltf_attribute* attribute,
			const cgltf_skin* skin,
			const cgltf_node* nodes,
			unsigned int current_node_index,
			unsigned int node_count,
			MemoryManager* allocator,
			CapacityStream<char>* error_message
		) {
			return MeshFromAttributeImpl<GetScalarValues>(mesh, attribute, skin, nodes, current_node_index, node_count, allocator, error_message);
		}

		// This will load a singular attribute e.g. position
		bool MeshFromAttributeTs(
			GLTFMesh& mesh,
			const cgltf_attribute* attribute,
			const cgltf_skin* skin,
			const cgltf_node* nodes,
			unsigned int current_node_index,
			unsigned int node_count,
			MemoryManager* allocator,
			CapacityStream<char>* error_message
		) {
			return MeshFromAttributeImpl<GetScalarValuesTs>(mesh, attribute, skin, nodes, current_node_index, node_count, allocator, error_message);
		}

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

	}

	GLTFData LoadGLTFFile(const char* path, CapacityStream<char>* error_message)
	{
		GLTFData data;

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

	GLTFData LoadGLTFFile(Stream<char> path, CapacityStream<char>* error_message)
	{
		ECS_TEMP_ASCII_STRING(temp_path, 512);
		temp_path.Copy(path);
		temp_path[path.size] = '\0';
		return LoadGLTFFile(temp_path.buffer, error_message);
	}

	GLTFData LoadGLTFFile(const wchar_t* path, CapacityStream<char>* error_message) 
	{
		return LoadGLTFFile(ToStream(path), error_message);
	}

	GLTFData LoadGLTFFile(Stream<wchar_t> path, CapacityStream<char>* error_message)
	{
		ECS_TEMP_ASCII_STRING(temp_path, 512);
		function::ConvertWideCharsToASCII(path, temp_path);
		temp_path[path.size] = '\0';
		return LoadGLTFFile(temp_path.buffer, error_message);
	}

	void* Allocate(MemoryManager* allocator, size_t size) {
		return allocator->Allocate(size);
	}

	void* AllocateTs(MemoryManager* allocator, size_t size) {
		return allocator->Allocate_ts(size);
	}
	
	template<bool (*MeshFromAttributeFunction)(
		GLTFMesh&,
		const cgltf_attribute*,
		const cgltf_skin*,
		const cgltf_node*,
		unsigned int, 
		unsigned int, 
		MemoryManager*, 
		CapacityStream<char>*
	), void* (*AllocateFunction)(MemoryManager*, size_t)>
	bool LoadMeshFromGLTFFileImpl(GLTFData data, GLTFMesh& mesh, MemoryManager* allocator, unsigned int mesh_index, CapacityStream<char>* error_message) {
		unsigned int node_count = data.data->nodes_count;
		const cgltf_node* nodes = data.data->nodes;

		unsigned int current_mesh_index = 0;
		for (size_t index = 0; index < node_count; index++) {
			if (nodes[index].mesh != nullptr) {
				if (current_mesh_index == mesh_index) {
					unsigned int primitive_count = nodes[index].mesh->primitives_count;

					for (size_t primitive_index = 0; primitive_index < primitive_count; primitive_index++) {
						const cgltf_primitive* primitive = &nodes[index].mesh->primitives[primitive_index];
						unsigned int attribute_count = primitive->attributes_count;

						for (unsigned int attribute_index = 0; attribute_index < attribute_count; attribute_index++) {
							const cgltf_attribute* attribute = &primitive->attributes[attribute_index];
							bool is_valid = MeshFromAttributeFunction(mesh, attribute, nodes[index].skin, nodes, index, node_count, allocator, error_message);
							if (!is_valid) {
								return false;
							}
						}

						if (primitive->indices != nullptr) {
							unsigned int index_count = primitive->indices->count;

							mesh.indices = Stream<unsigned int>(AllocateFunction(allocator, (sizeof(unsigned int) * index_count)), index_count);
							for (unsigned int index_index = 0; index_index < index_count; index_index++) {
								mesh.indices[index_index] = cgltf_accessor_read_index(primitive->indices, index_index);
							}
						}
					}
					return true;
				}
				current_mesh_index++;
			}
		}

		if (error_message != nullptr) {
			ECS_FORMAT_STRING(*error_message, "No mesh with index {0} has been found. The file contains only {1}", mesh_index, current_mesh_index);
		}
		return false;
	}

	bool LoadMeshFromGLTFFile(GLTFData data, GLTFMesh& mesh, MemoryManager* allocator, unsigned int mesh_index, CapacityStream<char>* error_message) {
		return LoadMeshFromGLTFFileImpl<MeshFromAttribute, Allocate>(data, mesh, allocator, mesh_index, error_message);
	}

	bool LoadMeshFromGLTFFileTs(GLTFData data, GLTFMesh& mesh, MemoryManager* allocator, unsigned int mesh_index, CapacityStream<char>* error_message) {
		return LoadMeshFromGLTFFileImpl<MeshFromAttributeTs, AllocateTs>(data, mesh, allocator, mesh_index, error_message);
	}

	template<bool (*MeshFromAttributeFunction)(
		GLTFMesh&,
		const cgltf_attribute*,
		const cgltf_skin*, 
		const cgltf_node*, 
		unsigned int,
		unsigned int, 
		MemoryManager*, 
		CapacityStream<char>*
	), void* (*AllocateFunction)(MemoryManager*, size_t)>
	bool LoadMeshesFromGLTFFileImpl(GLTFData data, GLTFMesh* meshes, MemoryManager* allocator, CapacityStream<char>* error_message) {
		unsigned int node_count = data.data->nodes_count;
		const cgltf_node* nodes = data.data->nodes;

		unsigned int mesh_index = 0;
		for (size_t index = 0; index < node_count; index++) {
			if (nodes[index].mesh != nullptr) {
				unsigned int primitive_count = nodes[index].mesh->primitives_count;

				for (size_t primitive_index = 0; primitive_index < primitive_count; primitive_index++) {
					const cgltf_primitive* primitive = &nodes[index].mesh->primitives[primitive_index];
					unsigned int attribute_count = primitive->attributes_count;

					for (unsigned int attribute_index = 0; attribute_index < attribute_count; attribute_index++) {
						const cgltf_attribute* attribute = &primitive->attributes[attribute_index];
						bool is_valid = MeshFromAttributeFunction(meshes[mesh_index], attribute, nodes[index].skin, nodes, index, node_count, allocator, error_message);
						if (!is_valid) {
							if (error_message != nullptr) {
								ECS_FORMAT_TEMP_STRING(additional_info, "The mesh index is {0}.", mesh_index);
								error_message->AddStreamSafe(additional_info);
							}
							return false;
						}
					}

					if (primitive->indices != nullptr) {
						unsigned int index_count = primitive->indices->count;

						meshes[mesh_index].indices = Stream<unsigned int>(AllocateFunction(allocator, (sizeof(unsigned int) * index_count)), index_count);
						for (unsigned int index_index = 0; index_index < index_count; index_index++) {
							meshes[mesh_index].indices[index_index] = cgltf_accessor_read_index(primitive->indices, index_index);
						}
					}
				}
				mesh_index++;
			}
		}

		return true;
	}

	bool LoadMeshesFromGLTFFile(
		GLTFData data,
		GLTFMesh* meshes,
		MemoryManager* allocator,
		containers::CapacityStream<char>* error_message
	) {
		return LoadMeshesFromGLTFFileImpl<MeshFromAttribute, Allocate>(data, meshes, allocator, error_message);
	}

	bool LoadMeshesFromGLTFFileTs(
		GLTFData data,
		GLTFMesh* meshes,
		MemoryManager* allocator,
		containers::CapacityStream<char>* error_message
	) {
		return LoadMeshesFromGLTFFileImpl<MeshFromAttributeTs, AllocateTs>(data, meshes, allocator, error_message);
	}

	void GLTFMeshToMesh(Graphics* graphics, const GLTFMesh& gltf_mesh, Mesh& mesh)
	{
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
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(float3), gltf_mesh.tangents.size, gltf_mesh.tangents.buffer);
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
	}

	// Creates the appropriate vertex and index buffers
	void GLTFMeshesToMeshes(Graphics* graphics, const GLTFMesh* gltf_meshes, Mesh* meshes, size_t count) {
		for (size_t index = 0; index < count; index++) {
			GLTFMeshToMesh(graphics, gltf_meshes[index], meshes[index]);
		}
	}

	void Deallocate(MemoryManager* allocator, const void* buffer) {
		allocator->Deallocate(buffer);
	}

	void DeallocateTs(MemoryManager* allocator, const void* buffer) {
		allocator->Deallocate_ts(buffer);
	}

	template<void (*Deallocate)(MemoryManager* allocator, const void* buffer)>
	void FreeGLTFMeshImpl(const GLTFMesh& mesh, MemoryManager* allocator) {
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

	void FreeGLTFMesh(const GLTFMesh& mesh, MemoryManager* allocator) {
		FreeGLTFMeshImpl<Deallocate>(mesh, allocator);
	}

	void FreeGLTFMeshTs(const GLTFMesh& mesh, MemoryManager* allocator) {
		FreeGLTFMeshImpl<DeallocateTs>(mesh, allocator);
	}

	void FreeGLTFMeshes(const GLTFMesh* meshes, size_t count, MemoryManager* allocator) {
		for (size_t index = 0; index < count; index++) {
			FreeGLTFMesh(meshes[index], allocator);
		}
	}

	void FreeGLTFMeshesTs(const GLTFMesh* meshes, size_t count, MemoryManager* allocator) {
		for (size_t index = 0; index < count; index++) {
			FreeGLTFMeshTs(meshes[index], allocator);
		}
	}

	void FreeGLTFFile(GLTFData data)
	{
		cgltf_free(data.data);
	}

}