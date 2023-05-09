// Credit to Hands-On Game Animation Programming by Gabor Szauer
// For some loading code snippets
#include "ecspch.h"
#include "GLTFLoader.h"
#include "../Rendering/RenderingStructures.h"
#include "../Utilities/FunctionInterfaces.h"
#include "../Rendering/Graphics.h"
#include "../Allocators/AllocatorPolymorphic.h"

namespace ECSEngine {

	// Helpers
	namespace {

		// -------------------------------------------------------------------------------------------------------------------------------

		// This makes an allocation
		Stream<float> GetScalarValues(
			AllocatorPolymorphic allocator,
			const cgltf_accessor* accessor, 
			unsigned int component_count,
			bool* success
		) {
			Stream<float> values;

			values.buffer = (float*)Allocate(allocator, component_count * accessor->count * sizeof(float));
			ECS_ASSERT(values.buffer != nullptr);

			values.size = accessor->count * component_count;
			for (size_t index = 0; index < accessor->count && *success; index++) {
				*success &= cgltf_accessor_read_float(accessor, index, values.buffer + index * component_count, component_count);
			}

			return values;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		// Does not make an allocation
		bool GetScalarValues(const cgltf_accessor* accessor, unsigned int component_count, Stream<float>* values) {
			values->size = accessor->count * component_count;
			bool success = true;
			for (size_t index = 0; index < accessor->count && success; index++) {
				success &= cgltf_accessor_read_float(accessor, index, values->buffer + index * component_count, component_count);
			}
			return success;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		unsigned int GetNodeIndex(const cgltf_node* target, const cgltf_node* nodes, size_t node_count) {
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

		// It returns -1 if it fails
		unsigned int MeshBufferSizeFromAttribute(
			const cgltf_attribute* attribute,
			CapacityStream<char>* error_message
		) {
			cgltf_attribute_type attribute_type = attribute->type;
			const cgltf_accessor* accessor = attribute->data;

			// Ignore tangents
			if (attribute_type == cgltf_attribute_type_tangent) {
				return 0;
			}

			bool is_valid = attribute_type != cgltf_attribute_type_invalid;

			if (!is_valid) {
				if (error_message != nullptr) {
					ECS_FORMAT_STRING(*error_message, "Mesh attribute has invalid type. Attribute name is {#}.", attribute->name);
				}
				return -1;
			}

			if (accessor->type != cgltf_type_vec2 && accessor->type != cgltf_type_vec3 && accessor->type != cgltf_type_vec4) {
				if (error_message != nullptr) {
					ECS_FORMAT_STRING(*error_message, "Mesh data type is invalid. Expected float2, float3 or float4 but found {#}.", accessor->type);
				}
				return -1;
			}

			return accessor->count;
		}

		void AddToMeshBufferSizes(GLTFMeshBufferSizes* sizes, cgltf_attribute_type attribute_type, unsigned int count) {
			switch (attribute_type) {
			case cgltf_attribute_type_position:
				sizes->count[ECS_MESH_POSITION] += count;
				break;
			case cgltf_attribute_type_texcoord:
				sizes->count[ECS_MESH_UV] += count;
				break;
			case cgltf_attribute_type_normal:
				sizes->count[ECS_MESH_NORMAL] += count;
				break;
			case cgltf_attribute_type_joints:
				sizes->count[ECS_MESH_BONE_INFLUENCE] += count;
				break;
			case cgltf_attribute_type_weights:
				sizes->count[ECS_MESH_BONE_WEIGHT] += count;
				break;
			case cgltf_attribute_type_color:
				sizes->count[ECS_MESH_COLOR] += count;
				break;
			}
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		void ProcessMeshPositions(Stream<float3> mesh_positions, const cgltf_node* nodes, size_t current_node_index) {
			float world_matrix[16];
			cgltf_node_transform_world(nodes + current_node_index, world_matrix);

			Matrix ecs_matrix(world_matrix);
			if (ecs_matrix != MatrixIdentity()) {
				size_t count = mesh_positions.size;
				size_t index = 0;

				constexpr size_t MASK_LAST_BIT = ~0x0000000000000001;
				size_t masked_count = count & MASK_LAST_BIT;
				for (; index < masked_count; index += 2) {
					TransformPoint(mesh_positions[index], mesh_positions[index + 1], ecs_matrix);
				}
				if (index < count) {
					mesh_positions[count - 1] = TransformPoint(mesh_positions[count - 1], ecs_matrix);
				}
			}
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		void ProcessMeshNormals(Stream<float3> mesh_normals, const cgltf_node* nodes, size_t current_node_index) {
			float matrix[16];
			cgltf_node_transform_world(nodes + current_node_index, matrix);
			Vector4 last_row = VectorGlobals::QUATERNION_IDENTITY_4;
			Vector8 extended_last_row(last_row, ZeroVector4());

			Matrix ecs_matrix(matrix);
			ecs_matrix.v[1].value = blend8<0, 1, 2, 3, 8, 9, 10, 11>(ecs_matrix.v[1].value, extended_last_row);

			Vector4 tolerance(0.000001f);

			auto loop = [&](auto perform_world_transformation) {
				for (size_t index = 0; index < mesh_normals.size; index++) {
					Vector4 normal(mesh_normals[index]);
					//normal = permute4<0, 2, 1, V_DC>(normal);
					if (horizontal_and((SquareLength3(normal) < tolerance))) {
						normal = VectorGlobals::UP_4;
						/*if (error_message != nullptr) {
							ECS_FORMAT_STRING(*error_message, "Mesh normal data is invalid. A normal has a squared length smaller than the tolerance. Index is {#}", index);
						}
						return false;*/
					}

					if constexpr (perform_world_transformation) {
						// Transform the normal
						normal = MatrixVectorMultiply(normal, ecs_matrix);
					}

					normal = Normalize3(normal);
					normal.StorePartialConstant<3>(mesh_normals.buffer + index);
				}
			};

			if (ecs_matrix != MatrixIdentity()) {
				// Perform normalization + world transformation
				loop(std::true_type{});
			}
			else {
				// Perform only normalization
				loop(std::false_type{});
			}
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		bool ProcessMeshBoneInfluences(
			Stream<float> file_values,
			Stream<uint4> influences, 
			const cgltf_node* nodes, 
			const cgltf_skin* skin, 
			size_t current_node_index, 
			size_t node_count, 
			CapacityStream<char>* error_message
		) {
			bool is_valid = true;

			for (size_t index = 0; index < influences.size; index++) {
				size_t buffer_index = index * 4;

				uint4 joints(
					(unsigned int)(file_values[buffer_index + 0] + 0.5f),
					(unsigned int)(file_values[buffer_index + 1] + 0.5f),
					(unsigned int)(file_values[buffer_index + 2] + 0.5f),
					(unsigned int)(file_values[buffer_index + 3] + 0.5f)
				);

				joints.x = GetNodeIndex(skin->joints[joints.x], nodes, node_count);
				joints.y = GetNodeIndex(skin->joints[joints.y], nodes, node_count);
				joints.z = GetNodeIndex(skin->joints[joints.z], nodes, node_count);
				joints.w = GetNodeIndex(skin->joints[joints.w], nodes, node_count);

				is_valid &= joints.x != -1 && joints.y != -1 && joints.z != -1 && joints.w != -1;

				if (!is_valid) {
					if (error_message != nullptr) {
						ECS_FORMAT_STRING(*error_message, "Mesh skin influences are invalid. Joint index is {#}.", index);
					}
					return false;
				}

				joints.x = function::ClampMax(0u, joints.x);
				joints.y = function::ClampMax(0u, joints.y);
				joints.z = function::ClampMax(0u, joints.z);
				joints.w = function::ClampMax(0u, joints.w);

				influences[index] = joints;
			}
			return true;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		bool MeshFromAttribute(
			GLTFMesh& mesh,
			const cgltf_attribute* attribute,
			const cgltf_skin* skin,
			const cgltf_node* nodes,
			size_t current_node_index,
			size_t node_count,
			AllocatorPolymorphic allocator,
			CapacityStream<char>* error_message
		) {
			unsigned int accessor_count = MeshBufferSizeFromAttribute(attribute, error_message);
			if (accessor_count == -1) {
				return false;
			}

			cgltf_attribute_type attribute_type = attribute->type;
			const cgltf_accessor* accessor = attribute->data;
			unsigned int component_count = accessor->type;

			bool is_valid = true;
			Stream<float> values = GetScalarValues(allocator, accessor, component_count, &is_valid);
			if (!is_valid) {
				if (error_message != nullptr) {
					ECS_FORMAT_STRING(*error_message, "Mesh data values are invalid. Attribute name is {#}.", attribute->name);
				}
				return false;
			}
			

			switch (attribute_type) {
			case cgltf_attribute_type_position: 
			{
				mesh.positions = Stream<float3>(values.buffer, accessor_count);
				ProcessMeshPositions(mesh.positions, nodes, current_node_index);
			}
			break;
			case cgltf_attribute_type_texcoord:
				mesh.uvs = Stream<float2>(values.buffer, accessor_count);
				break;
			case cgltf_attribute_type_weights:
				mesh.skin_weights = Stream<float4>(values.buffer, accessor_count);
				break;
			case cgltf_attribute_type_normal:
			{
				mesh.normals = Stream<float3>(values.buffer, accessor_count);
				ProcessMeshNormals(mesh.normals, nodes, current_node_index);
			}
			break;
			case cgltf_attribute_type_joints:
			{
				// These indices are skin relative. This function has no information about the
				// skin that is being parsed. Add +0.5f to round, since we can't read integers
				mesh.skin_influences = Stream<uint4>(values.buffer, accessor_count);
				bool success = ProcessMeshBoneInfluences(values, mesh.skin_influences, nodes, skin, current_node_index, node_count, error_message);
				if (!success) {
					return false;
				}
			}
			break;
			// TODO: Determine if the colors are stored in float or in unsigned char fashion
			case cgltf_attribute_type_color:
			{
				mesh.colors = Stream<Color>(values.buffer, accessor_count);
			}
			break;
			}

			return true;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		bool CoallescedMeshFromAttribute(
			const GLTFMesh& mesh,
			GLTFMeshBufferSizes* buffer_sizes,
			const cgltf_attribute* attribute,
			const cgltf_skin* skin,
			const cgltf_node* nodes,
			size_t current_node_index,
			size_t node_count,
			CapacityStream<char>* error_message
		) {
			unsigned int accessor_count = MeshBufferSizeFromAttribute(attribute, error_message);
			if (accessor_count == -1) {
				return false;
			}

			cgltf_attribute_type attribute_type = attribute->type;
			const cgltf_accessor* accessor = attribute->data;
			unsigned int component_count = accessor->type;

			Stream<float> values;
			switch (attribute_type) {
			case cgltf_attribute_type_position:
				values.buffer = (float*)(mesh.positions.buffer + buffer_sizes->count[ECS_MESH_POSITION]);
				buffer_sizes->count[ECS_MESH_POSITION] += accessor_count;
				break;
			case cgltf_attribute_type_normal:
				values.buffer = (float*)(mesh.normals.buffer + buffer_sizes->count[ECS_MESH_NORMAL]);
				buffer_sizes->count[ECS_MESH_NORMAL] += accessor_count;
				break;
			case cgltf_attribute_type_texcoord:
				values.buffer = (float*)(mesh.uvs.buffer + buffer_sizes->count[ECS_MESH_UV]);
				buffer_sizes->count[ECS_MESH_UV] += accessor_count;
				break;
			case cgltf_attribute_type_joints:
				values.buffer = (float*)(mesh.skin_influences.buffer + buffer_sizes->count[ECS_MESH_BONE_INFLUENCE]);
				buffer_sizes->count[ECS_MESH_BONE_INFLUENCE] += accessor_count;
				break;
			case cgltf_attribute_type_weights:
				values.buffer = (float*)(mesh.skin_weights.buffer + buffer_sizes->count[ECS_MESH_BONE_WEIGHT]);
				buffer_sizes->count[ECS_MESH_BONE_WEIGHT] += accessor_count;
				break;
			case cgltf_attribute_type_color:
				values.buffer = (float*)(mesh.colors.buffer + buffer_sizes->count[ECS_MESH_COLOR]);
				buffer_sizes->count[ECS_MESH_COLOR] += accessor_count;
				break;
			case cgltf_attribute_type_tangent:
				// Ignore tangents - we must return here
				return true;
			default:
				ECS_ASSERT(false, "Invalid cgltf attribute type");
			}

			bool is_valid = GetScalarValues(accessor, component_count, &values);
			if (!is_valid) {
				if (error_message != nullptr) {
					ECS_FORMAT_STRING(*error_message, "Mesh data values are invalid. Attribute name is {#}.", attribute->name);
				}
				return false;
			}

			switch (attribute_type) {
			case cgltf_attribute_type_position: {
				Stream<float3> mesh_positions = Stream<float3>(values.buffer, accessor_count);
				ProcessMeshPositions(mesh_positions, nodes, current_node_index);
			}
			break;
			case cgltf_attribute_type_texcoord:
				// We don't have to do any processing for the UVs
				break;
			case cgltf_attribute_type_weights:
				// No processing for skin weights
				break;
			case cgltf_attribute_type_normal:
			{
				Stream<float3> mesh_normals = Stream<float3>(values.buffer, accessor_count);
				ProcessMeshNormals(mesh_normals, nodes, current_node_index);
			}
			break;
			case cgltf_attribute_type_joints:
			{
				// These indices are skin relative. This function has no information about the
				// skin that is being parsed. Add +0.5f to round, since we can't read integers
				Stream<uint4> influences = Stream<uint4>(values.buffer, accessor_count);
				bool success = ProcessMeshBoneInfluences(values, influences, nodes, skin, current_node_index, node_count, error_message);
				return success;
			}
			break;
			// TODO: Determine if the colors are stored in float or in unsigned char fashion
			case cgltf_attribute_type_color:
			{
			}
			break;
			case cgltf_attribute_type_tangent:
			{
				// Ignore tangents - they should be already ignored from the 1st switch - but just in case
				return true;
			}
			break;
			default:
				ECS_ASSERT(false, "Invalid attribute_type");
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

			return gltf_material->name;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		void InvertMeshZAxis(Stream<float3> positions, Stream<float3> normals, Stream<unsigned int> indices) {
			// Invert the position Z axis
			for (size_t subindex = 0; subindex < positions.size; subindex++) {
				positions[subindex].z = -positions[subindex].z;
			}

			// Invert the normals Z axis
			for (size_t subindex = 0; subindex < normals.size; subindex++) {
				normals[subindex].z = -normals[subindex].z;
			}

			// The winding order of the vertices must be changed
			// Start from 1 in order to avoid an add
			for (size_t subindex = 1; subindex < indices.size; subindex += 3) {
				// Swap the last the vertices of each triangle
				indices.Swap(subindex, subindex + 1);
			}
		}

		// -------------------------------------------------------------------------------------------------------------------------------

	}

	// -------------------------------------------------------------------------------------------------------------------------------
	
	void* cgltf_allocate(void* user_data, size_t size) {
		AllocatorPolymorphic* allocator = (AllocatorPolymorphic*)user_data;
		return AllocateEx(*allocator, size);
	}

	void cgltf_free(void* user_data, void* buffer) {
		AllocatorPolymorphic* allocator = (AllocatorPolymorphic*)user_data;
		if (buffer != nullptr) {
			DeallocateEx(*allocator, buffer);
		}
	}

	// The functor only needs to parse the data from memory/disk
	template<typename Functor>
	GLTFData LoadGLTFFileImpl(CapacityStream<char>* error_message, Stream<char> path, AllocatorPolymorphic allocator, Functor&& functor) {
		GLTFData data;
		data.mesh_count = 0;

		cgltf_options options;
		memset(&options, 0, sizeof(cgltf_options));
		AllocatorPolymorphic* _allocator = (AllocatorPolymorphic*)malloc(sizeof(AllocatorPolymorphic));
		memcpy(_allocator, &allocator, sizeof(allocator));
		options.memory.user_data = _allocator;
		
		options.memory.alloc = cgltf_allocate;
		options.memory.free = cgltf_free;

		cgltf_result result = functor(&options, &data.data);

		if (result != cgltf_result_success) {
			if (error_message != nullptr) {
				ECS_FORMAT_STRING(*error_message, "Could not load gltf file {#}. Parsing failed.", path);
			}
			data.data = nullptr;
			return data;
		}
		result = cgltf_load_buffers(&options, data.data, path.buffer);
		if (result != cgltf_result_success) {
			cgltf_free(data.data);
			if (error_message != nullptr) {
				ECS_FORMAT_STRING(*error_message, "Could not load gltf file {#}. Loading buffers failed.", path);
			}
			data.data = nullptr;
			return data;
		}
		result = cgltf_validate(data.data);
		if (result != cgltf_result_success) {
			cgltf_free(data.data);
			if (error_message != nullptr) {
				ECS_FORMAT_STRING(*error_message, "Invalid gltf file {#}. Validation failed.", path);
			}
			data.data = nullptr;
			return data;
		}

		data.mesh_count = GLTFMeshCount(data.data);
		return data;
	}

	GLTFData LoadGLTFFile(Stream<char> path, AllocatorPolymorphic allocator, CapacityStream<char>* error_message)
	{
		NULL_TERMINATE(path);
		return LoadGLTFFileImpl(error_message, path, allocator, [=](const cgltf_options* options, cgltf_data** data) {
			return cgltf_parse_file(options, path.buffer, data);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	GLTFData LoadGLTFFile(Stream<wchar_t> path, AllocatorPolymorphic allocator, CapacityStream<char>* error_message)
	{
		ECS_TEMP_ASCII_STRING(temp_path, 512);
		function::ConvertWideCharsToASCII(path, temp_path);
		temp_path[temp_path.size] = '\0';
		return LoadGLTFFile(temp_path, allocator, error_message);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	GLTFData LoadGLTFFileFromMemory(Stream<void> file_data, AllocatorPolymorphic allocator, CapacityStream<char>* error_message) {
		return LoadGLTFFileImpl(error_message, "in memory", allocator, [=](const cgltf_options* options, cgltf_data** data) {
			return cgltf_parse(options, file_data.buffer, file_data.size, data);
		});
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMeshBufferSizesFromGLTF(const cgltf_node* node, GLTFMeshBufferSizes* buffer_sizes, CapacityStream<char>* error_message) {
		size_t primitive_count = node->mesh->primitives_count;

		for (size_t primitive_index = 0; primitive_index < primitive_count; primitive_index++) {
			const cgltf_primitive* primitive = &node->mesh->primitives[primitive_index];
			unsigned int attribute_count = primitive->attributes_count;

			for (unsigned int attribute_index = 0; attribute_index < attribute_count; attribute_index++) {
				const cgltf_attribute* attribute = &primitive->attributes[attribute_index];
				unsigned int count = MeshBufferSizeFromAttribute(attribute, error_message);
				if (count == -1) {
					return false;
				}

				AddToMeshBufferSizes(buffer_sizes, attribute->type, count);
			}

			//buffer_sizes->submesh_name_count += node->mesh->name

			if (primitive->indices != nullptr) {
				buffer_sizes->index_count += primitive->indices->count;
			}
		}
		buffer_sizes->submesh_name_count += strlen(node->name);

		return true;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMeshFromGLTF(
		GLTFMesh& mesh, 
		AllocatorPolymorphic allocator,
		const cgltf_node* nodes, 
		size_t node_index, 
		size_t node_count, 
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
				InvertMeshZAxis(mesh.positions, mesh.normals, mesh.indices);
			}
		}
		return true;
	}

	// Does not load the name
	bool LoadCoallescedMeshFromGLTF(
		GLTFMesh& mesh,
		GLTFMeshBufferSizes* buffer_sizes,
		const cgltf_node* nodes,
		size_t node_index,
		size_t node_count,
		bool invert_z_axis,
		CapacityStream<char>* error_message
	) {
		unsigned int primitive_count = nodes[node_index].mesh->primitives_count;

		for (size_t primitive_index = 0; primitive_index < primitive_count; primitive_index++) {
			const cgltf_primitive* primitive = &nodes[node_index].mesh->primitives[primitive_index];
			unsigned int attribute_count = primitive->attributes_count;

			Stream<float3> positions = { mesh.positions.buffer + buffer_sizes->count[ECS_MESH_POSITION], buffer_sizes->count[ECS_MESH_POSITION] };
			Stream<float3> normals = { mesh.normals.buffer + buffer_sizes->count[ECS_MESH_NORMAL], buffer_sizes->count[ECS_MESH_NORMAL] };

			for (unsigned int attribute_index = 0; attribute_index < attribute_count; attribute_index++) {
				const cgltf_attribute* attribute = &primitive->attributes[attribute_index];

				GLTFMeshBufferSizes before_sizes = *buffer_sizes;
				bool is_valid = CoallescedMeshFromAttribute(
					mesh,
					buffer_sizes,
					attribute,
					nodes[node_index].skin,
					nodes,
					node_index,
					node_count,
					error_message
				);
				if (!is_valid) {
					return false;
				}
			}

			positions.size = buffer_sizes->count[ECS_MESH_POSITION] - positions.size;
			normals.size = buffer_sizes->count[ECS_MESH_NORMAL] - normals.size;
			ECS_ASSERT(positions.size == normals.size, "Mismatch betwenn mesh position and normal vertex count.");

			Stream<unsigned int> indices = { nullptr, 0 };
			if (primitive->indices != nullptr) {
				unsigned int index_count = primitive->indices->count;

				indices = { mesh.indices.buffer + buffer_sizes->index_count, index_count };
				buffer_sizes->index_count += index_count;
				for (unsigned int index_index = 0; index_index < index_count; index_index++) {
					indices[index_index] = cgltf_accessor_read_index(primitive->indices, index_index);
				}
			}

			// Invert the z axis if necessary
			if (invert_z_axis) {
				InvertMeshZAxis(positions, normals, indices);
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
				Stream<char> material_name = gltf_material->name;

				ECS_TEMP_STRING(temp_texture_names, 1024);

				PBRMaterialMapping mappings[ECS_PBR_MATERIAL_MAPPING_COUNT];
				size_t mapping_count = 0;

				auto add_mapping = [&](const char* name, PBRMaterialTextureIndex mapping) {
					Stream<char> texture_name = name;
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

	// The functor receives a const cgltf_node*, a size_t node index and a size_t node count
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

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMeshFromGLTF(
		GLTFData data,
		GLTFMesh& mesh, 
		AllocatorPolymorphic allocator,
		unsigned int mesh_index, 
		bool invert_z_axis,
		CapacityStream<char>* error_message
	) {
		unsigned int current_mesh_index = 0;
		bool success = ForEachMeshInGLTF(data, [&](const cgltf_node* nodes, size_t index, size_t node_count) {
			if (mesh_index == current_mesh_index) {
				return LoadMeshFromGLTF(mesh, allocator, nodes, index, node_count, invert_z_axis, error_message);
			}
			current_mesh_index++;
			return true;
		});

		if (success) {
			if (current_mesh_index < mesh_index) {
				if (error_message != nullptr) {
					ECS_FORMAT_STRING(*error_message, "No mesh with index {#} has been found. The file contains only {1}", mesh_index, current_mesh_index);
				}
				return false;
			}
		}
		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMaterialFromGLTF(GLTFData data, PBRMaterial& material, AllocatorPolymorphic allocator, unsigned int mesh_index, CapacityStream<char>* error_message)
	{
		unsigned int current_mesh_index = 0;
		bool success = ForEachMeshInGLTF(data, [&](const cgltf_node* nodes, size_t index, size_t node_count) {
			if (mesh_index == current_mesh_index) {
				return LoadMaterialFromGLTF(material, allocator, nodes, index, error_message);
			}
			current_mesh_index++;
			return true;
		});

		if (success) {
			if (current_mesh_index < mesh_index) {
				if (error_message != nullptr) {
					ECS_FORMAT_STRING(*error_message, "No mesh with index {#} has been found. The file contains only {1}", mesh_index, current_mesh_index);
				}
				return false;
			}
		}
		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMeshesFromGLTF(
		GLTFData data,
		GLTFMesh* meshes,
		AllocatorPolymorphic allocator,
		bool invert_z_axis,
		CapacityStream<char>* error_message
	) {
		unsigned int mesh_index = 0;
		bool success = ForEachMeshInGLTF(data, [&](const cgltf_node* nodes, size_t index, size_t node_count) {
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
				ECS_FORMAT_TEMP_STRING(additional_info, "The mesh index is {#}", mesh_index);
				error_message->AddStreamSafe(additional_info);
				return false;
			}
			meshes[mesh_index].name = nodes[index].name;
			mesh_index++;
			return true;
		});

		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool DetermineMeshesFromGLTFBufferSizes(GLTFData data, GLTFMeshBufferSizes* sizes, CapacityStream<char>* error_message)
	{
		bool success = ForEachMeshInGLTF(data, [&](const cgltf_node* nodes, size_t index, size_t node_count) {
			return LoadMeshBufferSizesFromGLTF(&nodes[index], sizes, error_message);
		});

		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadCoallescedMeshFromGLTF(
		GLTFData data, 
		const GLTFMeshBufferSizes* sizes, 
		GLTFMesh* mesh,
		Submesh* submeshes,
		bool invert_z_axis, 
		LoadCoallescedMeshFromGLTFOptions* options
	)
	{
		bool allocate_names = false;
		bool coallesce_names = false;
		CapacityStream<char>* error_message = nullptr;
		AllocatorPolymorphic temporary_allocator = { nullptr };
		AllocatorPolymorphic permanent_allocator = { nullptr };

		if (options != nullptr) {
			allocate_names = options->allocate_submesh_name;
			coallesce_names = options->coallesce_submesh_name_allocations;
			error_message = options->error_message;
			temporary_allocator = options->temporary_buffer_allocator;
			permanent_allocator = options->permanent_allocator;
		}

		// Preallocate the buffers
		size_t total_bytes_to_allocate = 0;
		for (size_t index = 0; index < ECS_MESH_BUFFER_COUNT; index++) {
			size_t current_byte_size = (size_t)sizes->count[index] * GetMeshIndexElementByteSize((ECS_MESH_INDEX)index);
			total_bytes_to_allocate += current_byte_size;
		}

		total_bytes_to_allocate += sizes->index_count * sizeof(unsigned int);
		total_bytes_to_allocate += sizes->name_count * sizeof(char);

		void* allocation = AllocateEx(temporary_allocator, total_bytes_to_allocate);
		uintptr_t ptr = (uintptr_t)allocation;
		mesh->positions.InitializeFromBuffer(ptr, sizes->count[ECS_MESH_POSITION]);
		mesh->normals.InitializeFromBuffer(ptr, sizes->count[ECS_MESH_NORMAL]);
		mesh->uvs.InitializeFromBuffer(ptr, sizes->count[ECS_MESH_UV]);
		mesh->skin_weights.InitializeFromBuffer(ptr, sizes->count[ECS_MESH_BONE_WEIGHT]);
		mesh->skin_influences.InitializeFromBuffer(ptr, sizes->count[ECS_MESH_BONE_INFLUENCE]);
		mesh->colors.InitializeFromBuffer(ptr, sizes->count[ECS_MESH_COLOR]);
		mesh->indices.InitializeFromBuffer(ptr, sizes->index_count);

		void* submesh_name_allocation = nullptr;
		if (allocate_names) {
			if (coallesce_names) {
				submesh_name_allocation = AllocateEx(permanent_allocator, sizes->submesh_name_count * sizeof(char));
			}
		}
		uintptr_t submesh_name_ptr = (uintptr_t)submesh_name_allocation;

		GLTFMeshBufferSizes resetted_buffer_sizes;

		unsigned int submesh_index = 0;
		bool success = ForEachMeshInGLTF(data, [&](const cgltf_node* nodes, size_t index, size_t node_count) {
			GLTFMeshBufferSizes previous_buffer_size = resetted_buffer_sizes;
			bool success = LoadCoallescedMeshFromGLTF(*mesh, &resetted_buffer_sizes, nodes, index, node_count, invert_z_axis, error_message);
			if (success) {
				Submesh submesh;
				submesh.index_buffer_offset = previous_buffer_size.index_count;
				submesh.vertex_buffer_offset = previous_buffer_size.count[ECS_MESH_POSITION];
				submesh.index_count = resetted_buffer_sizes.index_count - previous_buffer_size.index_count;
				submesh.vertex_count = resetted_buffer_sizes.count[ECS_MESH_POSITION] - previous_buffer_size.count[ECS_MESH_POSITION];

				Stream<char> name = nodes[index].name;
				if (allocate_names) {
					if (!coallesce_names) {
						submesh_name_allocation = AllocateEx(permanent_allocator, strlen(nodes[index].name) * sizeof(char));
						submesh_name_ptr = (uintptr_t)submesh_name_allocation;
					}
					submesh.name.InitializeFromBuffer(submesh_name_ptr, name.size);
					submesh.name.Copy(name);
				}
				submeshes[submesh_index++] = submesh;
			}
			return success;
		});

		if (!success) {
			// Deallocate the buffer
			FreeCoallescedGLTFMesh(*mesh, temporary_allocator);
		}
		else {
			// We need to remap the indices according to the vertex offset of the submesh
			for (size_t index = 0; index < data.mesh_count; index++) {
				for (unsigned int index_index = 0; index_index < submeshes[index].index_count; index_index++) {
					mesh->indices[submeshes[index].index_buffer_offset + index_index] += submeshes[index].vertex_buffer_offset;
				}
			}
		}
		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadCoallescedMeshFromGLTF(
		GLTFData data, 
		GLTFMesh* mesh, 
		Submesh* submeshes, 
		bool invert_z_axis, 
		LoadCoallescedMeshFromGLTFOptions* options
	)
	{
		CapacityStream<char>* error_message = nullptr;
		if (options != nullptr) {
			error_message = options->error_message;
		}

		GLTFMeshBufferSizes buffer_sizes;
		bool success = DetermineMeshesFromGLTFBufferSizes(data, &buffer_sizes, error_message);
		if (!success) {
			return false;
		}

		return LoadCoallescedMeshFromGLTF(data, &buffer_sizes, mesh, submeshes, invert_z_axis, options);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMaterialsFromGLTF(GLTFData data, PBRMaterial* materials, AllocatorPolymorphic allocator, CapacityStream<char>* error_message)
	{
		unsigned int node_count = data.data->nodes_count;
		const cgltf_node* nodes = data.data->nodes;

		size_t material_index = 0;
		for (size_t index = 0; index < node_count; index++) {
			if (nodes[index].mesh != nullptr) {
				bool success = LoadMaterialFromGLTF(materials[material_index], allocator, nodes, index, error_message);
				if (!success) {
					ECS_FORMAT_TEMP_STRING(additional_info, "The material index is {#}.", material_index);
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
			if (function::CompareStrings(material_name, materials[subindex].name)) {
				submesh_material_index.Add(subindex);
				exists = true;
			}
		}

		if (!exists) {
			submesh_material_index.Add(materials.size);

			// Load the material as normally
			bool success = LoadMaterialFromGLTF(materials[materials.size], allocator, nodes, index, error_message);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(additional_info, "The material index is {#}.", materials.size);
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
					ECS_FORMAT_TEMP_STRING(additional_info, "The material index is {#}.", mesh_material_index);
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
					ECS_FORMAT_TEMP_STRING(additional_info, "The material index is {#}.", mesh_index);
					error_message->AddStreamSafe(additional_info);
					return false;
				}

				mesh_index++;
			}
		}

		return true;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	Mesh GLTFMeshToMesh(Graphics* graphics, const GLTFMesh& gltf_mesh, ECS_GRAPHICS_MISC_FLAGS misc_flags)
	{
		Mesh mesh;

		if (gltf_mesh.name.buffer != nullptr) {
			mesh.name = function::StringCopy(graphics->Allocator(), gltf_mesh.name);
		}
		else {
			mesh.name = { nullptr, 0 };
		}

		const ECS_GRAPHICS_USAGE buffer_usage = ECS_GRAPHICS_USAGE_IMMUTABLE;
		const ECS_GRAPHICS_CPU_ACCESS cpu_access = ECS_GRAPHICS_CPU_ACCESS_NONE;

		// Positions
		if (gltf_mesh.positions.size > 0) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_POSITION;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(float3), gltf_mesh.positions.size, gltf_mesh.positions.buffer, 
				false, buffer_usage, cpu_access, misc_flags);
		}

		// Normals
		if (gltf_mesh.normals.size > 0) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_NORMAL;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(float3), gltf_mesh.normals.size, gltf_mesh.normals.buffer,
				false, buffer_usage, cpu_access, misc_flags);
		}

		// UVs
		if (gltf_mesh.uvs.size > 0) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_UV;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(float2), gltf_mesh.uvs.size, gltf_mesh.uvs.buffer,
				false, buffer_usage, cpu_access, misc_flags);
		}

		// Vertex Colors
		if (gltf_mesh.colors.size > 0) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_COLOR;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(Color), gltf_mesh.colors.size, gltf_mesh.colors.buffer,
				false, buffer_usage, cpu_access, misc_flags);
		}

		// Bone weights
		if (gltf_mesh.skin_weights.size > 0) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_BONE_WEIGHT;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(float4), gltf_mesh.skin_weights.size, gltf_mesh.skin_weights.buffer,
				false, buffer_usage, cpu_access, misc_flags);
		}

		// Bone influences
		if (gltf_mesh.skin_influences.size > 0) {
			mesh.mapping[mesh.mapping_count] = ECS_MESH_BONE_INFLUENCE;
			mesh.vertex_buffers[mesh.mapping_count++] = graphics->CreateVertexBuffer(sizeof(uint4), gltf_mesh.skin_influences.size, gltf_mesh.skin_influences.buffer,
				false, buffer_usage, cpu_access, misc_flags);
		}

		// Indices
		if (gltf_mesh.indices.size > 0) {
			mesh.index_buffer = graphics->CreateIndexBuffer(sizeof(unsigned int), gltf_mesh.indices.size, gltf_mesh.indices.buffer, false, ECS_GRAPHICS_USAGE_DEFAULT, cpu_access, misc_flags);
		}

		return mesh;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void GLTFMeshesToMeshes(Graphics* graphics, const GLTFMesh* gltf_meshes, Mesh* meshes, size_t count, ECS_GRAPHICS_MISC_FLAGS misc_flags) {
		for (size_t index = 0; index < count; index++) {
			meshes[index] = GLTFMeshToMesh(graphics, gltf_meshes[index], misc_flags);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	Mesh GLTFMeshesToMergedMesh(
		Graphics* graphics, 
		GLTFMesh* gltf_meshes, 
		Submesh* submeshes,
		unsigned int* submesh_material_index, 
		size_t material_count, 
		size_t count,
		ECS_GRAPHICS_MISC_FLAGS misc_flags
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
		
		auto create_vertex_type = [&](unsigned int sizeof_type, ECS_MESH_INDEX mapping_index, unsigned char stream_byte_offset) {
			mesh.mapping[mesh.mapping_count] = mapping_index;
			// Create a main vertex buffer that is DEFAULT usage and then copy into it smaller vertex buffers
			VertexBuffer vertex_buffer = graphics->CreateVertexBuffer(
				sizeof_type,
				total_vertex_buffer_count,
				{}, 
				ECS_GRAPHICS_USAGE_DEFAULT, 
				ECS_GRAPHICS_CPU_ACCESS_NONE,
				misc_flags
			);
			size_t vertex_buffer_offset = 0;

			for (size_t index = 0; index < count; index++) {
				void** stream_buffer = (void**)function::OffsetPointer(&gltf_meshes[sorted_indices[index]], stream_byte_offset);
				size_t* stream_size = (size_t*)function::OffsetPointer(&gltf_meshes[sorted_indices[index]], stream_byte_offset + sizeof(void*));

				// Create a staging buffer that can be copied to the main buffer
				size_t buffer_size = *stream_size;
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
			mesh.index_buffer = graphics->CreateIndexBuffer(4, total_index_buffer_count, false, ECS_GRAPHICS_USAGE_DEFAULT, ECS_GRAPHICS_CPU_ACCESS_NONE, misc_flags);
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
			if (gltf_meshes[index].name.buffer != nullptr) {
				submeshes[index].name = function::StringCopy(graphics->Allocator(), gltf_meshes[index].name);
			}
			else {
				submeshes[index].name = { nullptr, 0 };
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

	CoallescedMesh LoadCoallescedMeshFromGLTFToGPU(
		Graphics* graphics, 
		GLTFData gltf_data, 
		AllocatorPolymorphic coallesced_mesh_allocator,
		bool invert_z_axis,
		LoadCoallescedMeshFromGLTFOptions* options
	)
	{
		CoallescedMesh result;
		memset(&result, 0, sizeof(result));

		GLTFMesh gltf_mesh;

		AllocatorPolymorphic temporary_buffer_allocator = { nullptr };
		CapacityStream<char>* error_message = nullptr;
		bool allocate_submesh_names = false;
		bool coallesced_submesh_names = true;
		AllocatorPolymorphic previous_permanent_allocator = { nullptr };

		if (options != nullptr) {
			temporary_buffer_allocator = options->temporary_buffer_allocator;
			error_message = options->error_message;
			allocate_submesh_names = options->allocate_submesh_name;
			coallesced_submesh_names = options->coallesce_submesh_name_allocations;
			previous_permanent_allocator = options->permanent_allocator;
			options->permanent_allocator = coallesced_mesh_allocator;
		}

		Submesh* submeshes = (Submesh*)Allocate(coallesced_mesh_allocator, sizeof(Submesh) * gltf_data.mesh_count);
		bool success = LoadCoallescedMeshFromGLTF(gltf_data, &gltf_mesh, submeshes, invert_z_axis, options);
		if (success) {
			result.mesh = GLTFMeshToMesh(graphics, gltf_mesh);
			result.submeshes = { submeshes, gltf_data.mesh_count };

		/*	if (allocate_submesh_names) {
				if (coallesced_submesh_names) {
					StreamCoallescedInplaceDeepCopy(result.submeshes, coallesced_mesh_allocator);
				}
				else {
					StreamInPlaceDeepCopy(result.submeshes, coallesced_mesh_allocator);
				}
			}*/

			// The buffers need to be deallocated
			FreeCoallescedGLTFMesh(gltf_mesh, temporary_buffer_allocator);
		}

		if (options) {
			options->permanent_allocator = previous_permanent_allocator;
		}
		return result;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadCoallescedMeshFromGLTFToGPU(
		Graphics* graphics, 
		GLTFData gltf_data, 
		CoallescedMesh* coallesced_mesh, 
		bool invert_z_axis, 
		LoadCoallescedMeshFromGLTFOptions* options
	)
	{
		GLTFMesh gltf_mesh;

		CapacityStream<char>* error_message = nullptr;
		AllocatorPolymorphic temporary_buffer_allocator = { nullptr };
		if (options != nullptr) {
			error_message = options->error_message;
			temporary_buffer_allocator = options->temporary_buffer_allocator;
		}

		GLTFMeshBufferSizes buffer_sizes;
		bool success = DetermineMeshesFromGLTFBufferSizes(gltf_data, &buffer_sizes, error_message);
		if (!success) {
			return false;
		}

		success = LoadCoallescedMeshFromGLTF(gltf_data, &buffer_sizes, &gltf_mesh, coallesced_mesh->submeshes.buffer, invert_z_axis, options);
		if (success) {
			coallesced_mesh->mesh = GLTFMeshToMesh(graphics, gltf_mesh);
		}
		// In case of failure it will still have the buffers allocated
		FreeCoallescedGLTFMesh(gltf_mesh, temporary_buffer_allocator);

		return success;
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

	void FreeCoallescedGLTFMesh(const GLTFMesh& mesh, AllocatorPolymorphic allocator)
	{
		// Everything is coallesced into a single big allocation
		if (mesh.positions.buffer != nullptr) {
			DeallocateEx(allocator, mesh.positions.buffer);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void FreeGLTFFile(GLTFData data)
	{
		// It has an allocator written into it
		void* allocator_polymorphic = data.data->memory.user_data;
		cgltf_free(data.data);
		free(allocator_polymorphic);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void ScaleGLTFMeshes(const GLTFMesh* gltf_meshes, size_t count, float scale_factor)
	{
		if (scale_factor != 1.0f) {
			// Scaling with the same factor on all axis basically means multiplying the coordinates by the given scale factor
			Vector8 splatted_factor(scale_factor);

			for (size_t index = 0; index < count; index++) {
				size_t vertex_count = gltf_meshes[index].positions.size;

				size_t float_count = vertex_count * 3;
				size_t simd_size = 8;
				size_t simd_count = function::GetSimdCount(float_count, simd_size);

				float* float_positions = (float*)gltf_meshes[index].positions.buffer;
				for (size_t float_index = 0; float_index < simd_count; float_index += simd_size) {
					Vector8 positions(float_positions + float_index);
					positions *= splatted_factor;
					positions.Store(float_positions + float_index);
				}
				// There is a remainder vertex
				if (simd_count < vertex_count) {
					for (size_t float_index = simd_count; float_index < float_count; float_index++) {
						float_positions[float_index] *= scale_factor;
					}
				}
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

}