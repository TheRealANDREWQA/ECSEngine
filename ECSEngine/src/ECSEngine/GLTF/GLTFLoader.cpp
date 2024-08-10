// Credit to Hands-On Game Animation Programming by Gabor Szauer
// For some loading code snippets
#include "ecspch.h"
#include "GLTFLoader.h"
#include "../Rendering/RenderingStructures.h"
#include "../Rendering/Graphics.h"
#include "../Rendering/GraphicsHelpers.h"
#include "../Allocators/AllocatorPolymorphic.h"

namespace ECSEngine {

	// Helpers
	namespace {

		// -------------------------------------------------------------------------------------------------------------------------------

		// This makes an allocation
		static Stream<float> GetScalarValues(
			AllocatorPolymorphic allocator,
			const cgltf_accessor* accessor, 
			unsigned int component_count,
			bool* success
		) {
			Stream<float> values;

			values.buffer = (float*)AllocateEx(allocator, component_count * accessor->count * sizeof(float));
			ECS_ASSERT(values.buffer != nullptr);

			values.size = accessor->count * component_count;
			for (size_t index = 0; index < accessor->count && *success; index++) {
				*success &= (bool)cgltf_accessor_read_float(accessor, index, values.buffer + index * component_count, component_count);
			}

			return values;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		// Does not make an allocation
		static bool GetScalarValues(const cgltf_accessor* accessor, unsigned int component_count, Stream<float>* values) {
			values->size = accessor->count * component_count;
			bool success = true;
			for (size_t index = 0; index < accessor->count && success; index++) {
				success &= (bool)cgltf_accessor_read_float(accessor, index, values->buffer + index * component_count, component_count);
			}
			return success;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		static unsigned int GetNodeIndex(const cgltf_node* target, const cgltf_node* nodes, size_t node_count) {
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
		static unsigned int MeshBufferSizeFromAttribute(
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
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Mesh attribute has invalid type. Attribute name is {#}.", attribute->name);
				return -1;
			}

			if (accessor->type != cgltf_type_vec2 && accessor->type != cgltf_type_vec3 && accessor->type != cgltf_type_vec4) {
				// Matches cgltf_type
				static Stream<char> cgltf_type_strings[] = {
					"invalid",
					"scalar",
					"float2",
					"float3",
					"float4",
					"matrix2",
					"matrix3",
					"matrix4"
				};
				
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Mesh data type is invalid. Expected float2, float3 or float4 but found {#}.", cgltf_type_strings[accessor->type]);
				return -1;
			}

			return accessor->count;
		}

		static void AddToMeshBufferSizes(GLTFMeshBufferSizes* sizes, cgltf_attribute_type attribute_type, unsigned int count) {
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

		static void ProcessMeshPositions(Stream<float3> mesh_positions, const cgltf_node* nodes, size_t current_node_index) {
			float world_matrix[16];
			cgltf_node_transform_world(nodes + current_node_index, world_matrix);

			Matrix ecs_matrix(world_matrix);
			if (ecs_matrix != MatrixIdentity()) {
				ApplySIMD(mesh_positions, mesh_positions, Vector3::ElementCount(), Vector3::ElementCount(), [ecs_matrix](const float3* input, float3* output, size_t count) {
					Vector3 positions = Vector3().Gather(input);
					Vector3 transformed_positions = TransformPoint(positions, ecs_matrix).xyz();
					transformed_positions.Scatter(output);
					return count;
				});
			}
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		static void ProcessMeshNormals(Stream<float3> mesh_normals, const cgltf_node* nodes, size_t current_node_index) {
			float4 matrix[sizeof(Matrix) / sizeof(float4)];
			cgltf_node_transform_world(nodes + current_node_index, (float*)matrix);
			// Eliminate any position from the matrix
			matrix[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
			Matrix ecs_matrix((const float*)matrix);

			const float TOLERANCE_VAL = 0.000001f;
			Vec8f tolerance(TOLERANCE_VAL);

			// Takes a lambda that transforms the normal if needed
			auto loop = [&](Vector3 (ECS_VECTORCALL *perform_world_transformation)(Vector3 normal, Matrix transform_matrix)) {
				ApplySIMD(mesh_normals, mesh_normals, Vector3::ElementCount(), Vector3::ElementCount(), [&](const float3* input, float3* output, size_t count) {
					Vector3 normal = Vector3().Gather(input);
					//normal = PerLanePermute<0, 2, 1, V_DC>(normal);
					SIMDVectorMask less_than_tolerance_mask = SquareLength(normal) < tolerance;
					Vector3 up_vector = UpVector();
					normal = Select(less_than_tolerance_mask, up_vector, normal);
					// We could say error if the normal data seems invalid
					/*if (error_message != nullptr) {
							FormatString(*error_message, "Mesh normal data is invalid. A normal has a squared length smaller than the tolerance. Index is {#}", index);
						}
					return false;*/

					// Transform the normal
					normal = perform_world_transformation(normal, ecs_matrix);

					normal = NormalizeIfNot(normal, count);
					normal.Scatter(output);
					return count;
				});
			};

			if (ecs_matrix != MatrixIdentity()) {
				// Perform normalization + world transformation
				loop([](Vector3 normal, Matrix transform_matrix) {
					return MatrixVectorMultiply(normal, transform_matrix);
				});
			}
			else {
				// Perform only normalization
				loop([](Vector3 normal, Matrix transform_matrix) {
					return normal;
				});
			}
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		static bool ProcessMeshBoneInfluences(
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
					ECS_FORMAT_ERROR_MESSAGE(error_message, "Mesh skin influences are invalid. Joint index is {#}.", index);
					return false;
				}

				joints.x = ClampMax(0u, joints.x);
				joints.y = ClampMax(0u, joints.y);
				joints.z = ClampMax(0u, joints.z);
				joints.w = ClampMax(0u, joints.w);

				influences[index] = joints;
			}
			return true;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		static bool MeshFromAttribute(
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
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Mesh data values are invalid. Attribute name is {#}.", attribute->name);
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

		static bool CoalescedMeshFromAttribute(
			const GLTFMesh& mesh,
			GLTFMeshBufferSizes* buffer_sizes,
			const cgltf_attribute* attribute,
			const cgltf_skin* skin,
			const cgltf_node* nodes,
			size_t current_node_index,
			size_t node_count,
			CapacityStream<char>* error_message
		) {
			if (strcmp(attribute->name, "TEXCOORD_1") == 0) {
				// Ignore multiple sets of UVs
				return true;
			}

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
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Mesh data values are invalid. Attribute name is {#}.", attribute->name);
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

		static bool ValidateMesh(const GLTFMesh& mesh) {
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

		static size_t GLTFMeshCount(const cgltf_data* data) {
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
		
		static Stream<char> GetMaterialName(const cgltf_primitive* primitives, unsigned int primitive_index) {
			const cgltf_material* gltf_material = primitives[primitive_index].material;
			return gltf_material->name;
		}

		// -------------------------------------------------------------------------------------------------------------------------------

		static void InvertMeshZAxis(Stream<float3> positions, Stream<float3> normals, Stream<unsigned int> indices) {
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
		AllocatorPolymorphic* _allocator = (AllocatorPolymorphic*)Malloc(sizeof(AllocatorPolymorphic));
		memcpy(_allocator, &allocator, sizeof(allocator));
		options.memory.user_data = _allocator;
		
		options.memory.alloc = cgltf_allocate;
		options.memory.free = cgltf_free;

		cgltf_result result = functor(&options, &data.data);

		if (result != cgltf_result_success) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Could not load gltf file {#}. Parsing failed.", path);
			data.data = nullptr;
			return data;
		}
		result = cgltf_load_buffers(&options, data.data, path.buffer);
		if (result != cgltf_result_success) {
			cgltf_free(data.data);
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Could not load gltf file {#}. Loading buffers failed.", path);
			data.data = nullptr;
			return data;
		}
		result = cgltf_validate(data.data);
		if (result != cgltf_result_success) {
			cgltf_free(data.data);
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Invalid gltf file {#}. Validation failed.", path);
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
		ECS_STACK_CAPACITY_STREAM(char, temp_path, 512);
		ConvertWideCharsToASCII(path, temp_path);
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

				mesh.indices = Stream<unsigned int>(AllocateEx(allocator, (sizeof(unsigned int) * index_count)), index_count);
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
	bool LoadCoalescedMeshFromGLTF(
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
				bool is_valid = CoalescedMeshFromAttribute(
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
			ECS_ASSERT(positions.size == normals.size, "Mismatch between mesh position and normal vertex count.");

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

	// Returns true if a material was found, else false
	static bool LoadMaterialFromGLTF(
		PBRMaterial& material,
		AllocatorPolymorphic allocator,
		const cgltf_node* nodes,
		unsigned int node_index,
		CapacityStream<char>* error_message
	) {
		return internal::ForMaterialFromGLTF(
			nodes,
			node_index,
			material,
			[&](Stream<char> material_name, size_t mapping_count, PBRMaterialMapping* mappings, Stream<void>* texture_mapping_data, TextureExtension* texture_extension) {
				AllocatePBRMaterial(material, material_name, Stream<PBRMaterialMapping>(mappings, mapping_count), allocator);
			}
		);
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
		bool success = internal::ForEachMeshInGLTF(data, [&](const cgltf_node* nodes, size_t index, size_t node_count) {
			if (mesh_index == current_mesh_index) {
				return LoadMeshFromGLTF(mesh, allocator, nodes, index, node_count, invert_z_axis, error_message);
			}
			current_mesh_index++;
			return true;
		});

		if (success) {
			if (current_mesh_index < mesh_index) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "No mesh with index {#} has been found. The file contains only {#}", mesh_index, current_mesh_index);
				return false;
			}
		}
		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMaterialFromGLTF(GLTFData data, PBRMaterial& material, AllocatorPolymorphic allocator, unsigned int mesh_index, CapacityStream<char>* error_message)
	{
		unsigned int current_mesh_index = 0;
		bool success = internal::ForEachMeshInGLTF(data, [&](const cgltf_node* nodes, size_t index, size_t node_count) {
			if (mesh_index == current_mesh_index) {
				return LoadMaterialFromGLTF(material, allocator, nodes, index, error_message);
			}
			current_mesh_index++;
			return true;
		});

		if (success) {
			if (current_mesh_index < mesh_index) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "No mesh with index {#} has been found. The file contains only {#}", mesh_index, current_mesh_index);
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
		bool success = internal::ForEachMeshInGLTF(data, [&](const cgltf_node* nodes, size_t index, size_t node_count) {
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
		bool success = internal::ForEachMeshInGLTF(data, [&](const cgltf_node* nodes, size_t index, size_t node_count) {
			return LoadMeshBufferSizesFromGLTF(&nodes[index], sizes, error_message);
		});

		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadCoalescedMeshFromGLTF(
		GLTFData data, 
		const GLTFMeshBufferSizes* sizes, 
		GLTFMesh* mesh,
		Submesh* submeshes,
		bool invert_z_axis, 
		LoadCoalescedMeshFromGLTFOptions* options
	)
	{
		bool allocate_names = false;
		bool coallesce_names = false;
		bool determine_submesh_bounding_box = true;
		CapacityStream<char>* error_message = nullptr;
		AllocatorPolymorphic temporary_allocator = { nullptr };
		AllocatorPolymorphic permanent_allocator = { nullptr };
		float scale_factor = 1.0f;

		if (options != nullptr) {
			allocate_names = options->allocate_submesh_name;
			coallesce_names = options->coalesce_submesh_name_allocations;
			error_message = options->error_message;
			temporary_allocator = options->temporary_buffer_allocator;
			permanent_allocator = options->permanent_allocator;
			scale_factor = options->scale_factor;
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
		bool success = internal::ForEachMeshInGLTF(data, [&](const cgltf_node* nodes, size_t index, size_t node_count) {
			GLTFMeshBufferSizes previous_buffer_size = resetted_buffer_sizes;
			bool success = LoadCoalescedMeshFromGLTF(*mesh, &resetted_buffer_sizes, nodes, index, node_count, invert_z_axis, error_message);
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
					submesh.name.CopyOther(name);
				}
				
				if (determine_submesh_bounding_box) {
					Stream<float3> positions = { mesh->positions.buffer + submesh.vertex_buffer_offset, submesh.vertex_count };
					submesh.bounds = GetMeshBoundingBox(positions);
				}
				else {
					submesh.bounds = InfiniteAABBScalar();
				}

				submeshes[submesh_index++] = submesh;
			}
			return success;
		});

		if (!success) {
			// Deallocate the buffer
			FreeCoalescedGLTFMesh(*mesh, temporary_allocator);
		}
		else {
			// We need to remap the indices according to the vertex offset of the submesh
			for (size_t index = 0; index < data.mesh_count; index++) {
				for (unsigned int index_index = 0; index_index < submeshes[index].index_count; index_index++) {
					mesh->indices[submeshes[index].index_buffer_offset + index_index] += submeshes[index].vertex_buffer_offset;
				}
			}

			// We also need to scale the gltf mesh, if required
			if (scale_factor != 1.0f) {
				ScaleGLTFMeshes({ mesh, 1 }, scale_factor);
				// Check the bounding boxes as well - those need to be scaled here
				if (determine_submesh_bounding_box) {
					for (size_t index = 0; index < data.mesh_count; index++) {
						submeshes[index].bounds = ScaleAABBFromOrigin(submeshes[index].bounds, float3::Splat(scale_factor));
					}
				}
			}

			if (options->center_object_midpoint) {
				float3 translation = GLTFMeshOriginToCenter(mesh);
				// Translate the aabb for each submesh as well
				for (size_t index = 0; index < data.mesh_count; index++) {
					submeshes[index].bounds = TranslateAABB(submeshes[index].bounds, -translation);
				}
			}
		}
		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadCoalescedMeshFromGLTF(
		GLTFData data, 
		GLTFMesh* mesh, 
		Submesh* submeshes, 
		bool invert_z_axis, 
		LoadCoalescedMeshFromGLTFOptions* options
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

		return LoadCoalescedMeshFromGLTF(data, &buffer_sizes, mesh, submeshes, invert_z_axis, options);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadMaterialsFromGLTF(GLTFData data, CapacityStream<PBRMaterial>* materials, AllocatorPolymorphic allocator, CapacityStream<char>* error_message)
	{
		unsigned int node_count = data.data->nodes_count;
		const cgltf_node* nodes = data.data->nodes;

		for (size_t index = 0; index < node_count; index++) {
			if (nodes[index].mesh != nullptr) {
				// No matter what if the material is present or not, we should
				bool was_loaded = LoadMaterialFromGLTF(materials->buffer[materials->size], allocator, nodes, index, error_message);
				materials->size++;
				materials->AssertCapacity();
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
			if (material_name == materials[subindex].name) {
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

	Mesh GLTFMeshToMesh(Graphics* graphics, const GLTFMesh& gltf_mesh, ECS_GRAPHICS_MISC_FLAGS misc_flags, bool compute_bounding_box)
	{
		Mesh mesh;

		if (gltf_mesh.name.buffer != nullptr) {
			mesh.name = StringCopy(graphics->Allocator(), gltf_mesh.name);
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

		if (compute_bounding_box) {
			mesh.bounds = GetGLTFMeshBoundingBox(&gltf_mesh);
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
		Stream<GLTFMesh> gltf_meshes,
		Submesh* submeshes,
		unsigned int* submesh_material_index,
		size_t material_count,
		ECS_GRAPHICS_MISC_FLAGS misc_flags
	) {
		constexpr size_t COUNT_MAX = 2048;

		Mesh mesh;

		size_t total_vertex_buffer_count = 0;
		size_t total_index_buffer_count = 0;

		ECS_STACK_CAPACITY_STREAM(unsigned int, sorted_indices_storage, 2048);
		ECS_ASSERT(gltf_meshes.size <= sorted_indices_storage.capacity);
		unsigned int* sorted_indices = sorted_indices_storage.buffer;

		// The x component will hold the total vertex count
		// The y component will hold the total index count
		uint2* material_counts = (uint2*)ECS_STACK_ALLOC(sizeof(uint2) * material_count);
		memset(material_counts, 0, sizeof(uint2) * material_count);

		size_t submesh_offset = 0;
		// For every material run through the submeshes to combine those that have the same material
		for (size_t index = 0; index < material_count; index++) {
			submeshes[index].bounds = InfiniteAABBScalar();
			for (size_t subindex = 0; subindex < gltf_meshes.size; subindex++) {
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
				false,
				ECS_GRAPHICS_USAGE_DEFAULT,
				ECS_GRAPHICS_CPU_ACCESS_NONE,
				misc_flags
			);
			size_t vertex_buffer_offset = 0;

			for (size_t index = 0; index < gltf_meshes.size; index++) {
				void** stream_buffer = (void**)OffsetPointer(&gltf_meshes[sorted_indices[index]], stream_byte_offset);
				size_t* stream_size = (size_t*)OffsetPointer(&gltf_meshes[sorted_indices[index]], stream_byte_offset + sizeof(void*));

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
		if (gltf_meshes[0].positions.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_POSITION), ECS_MESH_POSITION, offsetof(GLTFMesh, positions));
		}

		// Normals
		if (gltf_meshes[0].normals.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_NORMAL), ECS_MESH_NORMAL, offsetof(GLTFMesh, normals));
		}

		// UVs
		if (gltf_meshes[0].uvs.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_UV), ECS_MESH_UV, offsetof(GLTFMesh, uvs));
		}

		// Vertex Colors
		if (gltf_meshes[0].colors.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_COLOR), ECS_MESH_COLOR, offsetof(GLTFMesh, colors));
		}

		// Bone weights
		if (gltf_meshes[0].skin_weights.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_BONE_WEIGHT), ECS_MESH_BONE_WEIGHT, offsetof(GLTFMesh, skin_weights));
		}

		// Bone influences
		if (gltf_meshes[0].skin_influences.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_BONE_INFLUENCE), ECS_MESH_BONE_INFLUENCE, offsetof(GLTFMesh, skin_influences));
		}

		// Indices
		if (gltf_meshes[0].indices.size > 0) {
			// Create a main index buffer that is DEFAULT usage and then copy into it the smaller index buffers
			mesh.index_buffer = graphics->CreateIndexBuffer(4, total_index_buffer_count, false, ECS_GRAPHICS_USAGE_DEFAULT, ECS_GRAPHICS_CPU_ACCESS_NONE, misc_flags);
			size_t index_buffer_offset = 0;

			for (size_t index = 0; index < gltf_meshes.size; index++) {
				// Create a staging buffer that can be copied to the main buffer
				size_t buffer_size = gltf_meshes[sorted_indices[index]].indices.size;
				IndexBuffer current_buffer = graphics->CreateIndexBuffer(gltf_meshes[sorted_indices[index]].indices);
				CopyBufferSubresource(mesh.index_buffer, index_buffer_offset, current_buffer, 0, buffer_size, graphics->GetContext());
				index_buffer_offset += buffer_size;
				graphics->RemovePossibleResourceFromTracking(current_buffer.Interface());
				current_buffer.Release();
			}
		}
		
		// Compute the coalesced bounding box with the intermediate submesh bounding boxes
		mesh.bounds = ReverseInfiniteAABBScalar();
		for (size_t index = 0; index < gltf_meshes.size; index++) {
			AABBScalar current_bounding_box = GetGLTFMeshBoundingBox(gltf_meshes.buffer + index);
			mesh.bounds = GetCombinedAABB(mesh.bounds, submeshes[index].bounds);
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

	Mesh GLTFMeshesToMergedMesh(Graphics* graphics, Stream<GLTFMesh> gltf_meshes, Submesh* submeshes, ECS_GRAPHICS_MISC_FLAGS misc_flags)
	{
		Mesh mesh;

		size_t total_vertex_buffer_count = 0;
		size_t total_index_buffer_count = 0;
		for (size_t index = 0; index < gltf_meshes.size; index++) {
			total_vertex_buffer_count += gltf_meshes[index].positions.size;
			total_index_buffer_count += gltf_meshes[index].indices.size;
		}

		auto create_vertex_type = [&](unsigned int sizeof_type, ECS_MESH_INDEX mapping_index, unsigned char stream_byte_offset) {
			mesh.mapping[mesh.mapping_count] = mapping_index;
			// Create a main vertex buffer that is DEFAULT usage and then copy into it smaller vertex buffers
			VertexBuffer vertex_buffer = graphics->CreateVertexBuffer(
				sizeof_type,
				total_vertex_buffer_count,
				false,
				ECS_GRAPHICS_USAGE_DEFAULT,
				ECS_GRAPHICS_CPU_ACCESS_WRITE,
				misc_flags
			);
			size_t vertex_buffer_offset = 0;

			// Map the current buffer and then write into it
			void* mapped_data = graphics->MapBuffer(vertex_buffer.buffer);

			for (size_t index = 0; index < gltf_meshes.size; index++) {
				void** stream_buffer = (void**)OffsetPointer(&gltf_meshes[index], stream_byte_offset);
				size_t* stream_size = (size_t*)OffsetPointer(&gltf_meshes[index], stream_byte_offset + sizeof(void*));

				memcpy(mapped_data, *stream_buffer, *stream_size * sizeof_type);
				mapped_data = OffsetPointer(mapped_data, *stream_size * sizeof_type);
			}
			mesh.vertex_buffers[mesh.mapping_count++] = vertex_buffer;
			graphics->UnmapBuffer(vertex_buffer.buffer);
		};

		// Positions
		if (gltf_meshes[0].positions.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_POSITION), ECS_MESH_POSITION, offsetof(GLTFMesh, positions));
		}

		// Normals
		if (gltf_meshes[0].normals.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_NORMAL), ECS_MESH_NORMAL, offsetof(GLTFMesh, normals));
		}

		// UVs
		if (gltf_meshes[0].uvs.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_UV), ECS_MESH_UV, offsetof(GLTFMesh, uvs));
		}

		// Vertex Colors
		if (gltf_meshes[0].colors.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_COLOR), ECS_MESH_COLOR, offsetof(GLTFMesh, colors));
		}

		// Bone weights
		if (gltf_meshes[0].skin_weights.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_BONE_WEIGHT), ECS_MESH_BONE_WEIGHT, offsetof(GLTFMesh, skin_weights));
		}

		// Bone influences
		if (gltf_meshes[0].skin_influences.size > 0) {
			create_vertex_type(GetMeshIndexElementByteSize(ECS_MESH_BONE_INFLUENCE), ECS_MESH_BONE_INFLUENCE, offsetof(GLTFMesh, skin_influences));
		}

		// Indices
		if (gltf_meshes[0].indices.buffer != nullptr) {
			// Create a main index buffer that is DEFAULT usage and then copy into it the smaller index buffers
			mesh.index_buffer = graphics->CreateIndexBuffer(4, total_index_buffer_count, false, ECS_GRAPHICS_USAGE_DEFAULT, ECS_GRAPHICS_CPU_ACCESS_WRITE, misc_flags);
			size_t index_buffer_offset = 0;

			void* mapped_data = graphics->MapBuffer(mesh.index_buffer.buffer);

			for (size_t index = 0; index < gltf_meshes.size; index++) {
				gltf_meshes[index].indices.CopyTo(mapped_data);
				mapped_data = OffsetPointer(mapped_data, gltf_meshes[index].indices.MemoryOf(gltf_meshes[index].indices.size));
			}

			graphics->UnmapBuffer(mesh.index_buffer.buffer);
		}

		// Now fill in the submeshes and calculate the bounds
		total_vertex_buffer_count = 0;
		total_index_buffer_count = 0;
		mesh.bounds = ReverseInfiniteAABBScalar();
		for (size_t index = 0; index < gltf_meshes.size; index++) {
			submeshes[index].vertex_buffer_offset = total_vertex_buffer_count;
			submeshes[index].index_buffer_offset = total_index_buffer_count;
			submeshes[index].vertex_count = gltf_meshes[index].positions.size;
			submeshes[index].index_count = gltf_meshes[index].indices.size;

			total_vertex_buffer_count += gltf_meshes[index].positions.size;
			total_index_buffer_count += gltf_meshes[index].indices.size;

			submeshes[index].name = { nullptr, 0 };
			submeshes[index].bounds = GetGLTFMeshBoundingBox(gltf_meshes.buffer + index);
			mesh.bounds = GetCombinedAABB(mesh.bounds, submeshes[index].bounds);
		}

		return mesh;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	CoalescedMesh LoadCoalescedMeshFromGLTFToGPU(
		Graphics* graphics, 
		GLTFData gltf_data, 
		AllocatorPolymorphic coalesced_mesh_allocator,
		bool invert_z_axis,
		LoadCoalescedMeshFromGLTFOptions* options
	)
	{
		CoalescedMesh result;
		memset(&result, 0, sizeof(result));

		GLTFMesh gltf_mesh;

		AllocatorPolymorphic temporary_buffer_allocator = { nullptr };
		CapacityStream<char>* error_message = nullptr;
		bool allocate_submesh_names = false;
		bool coalesced_submesh_names = true;
		AllocatorPolymorphic previous_permanent_allocator = { nullptr };

		if (options != nullptr) {
			temporary_buffer_allocator = options->temporary_buffer_allocator;
			error_message = options->error_message;
			allocate_submesh_names = options->allocate_submesh_name;
			coalesced_submesh_names = options->coalesce_submesh_name_allocations;
			previous_permanent_allocator = options->permanent_allocator;
			options->permanent_allocator = coalesced_mesh_allocator;
		}

		Submesh* submeshes = (Submesh*)Allocate(coalesced_mesh_allocator, sizeof(Submesh) * gltf_data.mesh_count);
		bool success = LoadCoalescedMeshFromGLTF(gltf_data, &gltf_mesh, submeshes, invert_z_axis, options);
		if (success) {
			result.mesh = GLTFMeshToMesh(graphics, gltf_mesh, ECS_GRAPHICS_MISC_NONE, false);
			result.submeshes = { submeshes, gltf_data.mesh_count };
			result.mesh.bounds = GetSubmeshesBoundingBox(result.submeshes);

			// The buffers need to be deallocated
			FreeCoalescedGLTFMesh(gltf_mesh, temporary_buffer_allocator);
		}

		if (options) {
			options->permanent_allocator = previous_permanent_allocator;
		}
		return result;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	bool LoadCoalescedMeshFromGLTFToGPU(
		Graphics* graphics, 
		GLTFData gltf_data, 
		CoalescedMesh* coalesced_mesh, 
		bool invert_z_axis, 
		LoadCoalescedMeshFromGLTFOptions* options
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

		success = LoadCoalescedMeshFromGLTF(gltf_data, &buffer_sizes, &gltf_mesh, coalesced_mesh->submeshes.buffer, invert_z_axis, options);
		if (success) {
			coalesced_mesh->mesh = GLTFMeshToMesh(graphics, gltf_mesh, ECS_GRAPHICS_MISC_NONE, false);
			coalesced_mesh->mesh.bounds = GetSubmeshesBoundingBox(coalesced_mesh->submeshes);
		}
		// In case of failure it will still have the buffers allocated
		FreeCoalescedGLTFMesh(gltf_mesh, temporary_buffer_allocator);

		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void FreeGLTFMesh(const GLTFMesh& mesh, AllocatorPolymorphic allocator) {
		if (mesh.positions.buffer != nullptr) {
			DeallocateEx(allocator, mesh.positions.buffer);
		}
		if (mesh.indices.buffer != nullptr) {
			DeallocateEx(allocator, mesh.indices.buffer);
		}
		if (mesh.normals.buffer != nullptr) {
			DeallocateEx(allocator, mesh.normals.buffer);
		}
		if (mesh.skin_influences.buffer != nullptr) {
			DeallocateEx(allocator, mesh.skin_influences.buffer);
		}
		if (mesh.skin_weights.buffer != nullptr) {
			DeallocateEx(allocator, mesh.skin_weights.buffer);
		}
		if (mesh.uvs.buffer != nullptr) {
			DeallocateEx(allocator, mesh.uvs.buffer);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void FreeGLTFMeshes(Stream<GLTFMesh> meshes, AllocatorPolymorphic allocator) {
		for (size_t index = 0; index < meshes.size; index++) {
			FreeGLTFMesh(meshes[index], allocator);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void FreeCoalescedGLTFMesh(const GLTFMesh& mesh, AllocatorPolymorphic allocator)
	{
		// Everything is coalesced into a single big allocation
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
		Free(allocator_polymorphic);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void ScaleGLTFMeshes(Stream<GLTFMesh> meshes, float scale_factor)
	{
		if (scale_factor != 1.0f) {
			// Scaling with the same factor on all axis basically means multiplying the coordinates by the given scale factor
			Vec8f splatted_factor(scale_factor);

			for (size_t index = 0; index < meshes.size; index++) {
				size_t vertex_count = meshes[index].positions.size;
				Stream<float> float_positions = { meshes[index].positions.buffer, vertex_count * 3 };
				ApplySIMD(float_positions, float_positions, splatted_factor.size(), splatted_factor.size(), [=](const float* input, float* output, size_t count) {
					Vec8f positions = Vec8f().load(input);
					positions *= splatted_factor;
					positions.store(output);
					return count;
				});
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	AABBScalar GetGLTFMeshBoundingBox(const GLTFMesh* mesh)
	{
		return GetMeshBoundingBox(mesh->positions);
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	void GetGLTFMeshesBoundingBox(Stream<GLTFMesh> meshes, AABBScalar* bounding_boxes)
	{
		for (size_t index = 0; index < meshes.size; index++) {
			bounding_boxes[index] = GetGLTFMeshBoundingBox(meshes.buffer + index);
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	AABBScalar GetGLTFMeshesCombinedBoundingBox(Stream<GLTFMesh> meshes)
	{
		AABBScalar bounding_box = ReverseInfiniteAABBScalar();
		for (size_t index = 0; index < meshes.size; index++) {
			AABBScalar current_bounding_box = GetGLTFMeshBoundingBox(meshes.buffer + index);
			bounding_box = GetCombinedAABB(bounding_box, current_bounding_box);
		}
		
		return bounding_box;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

	float3 GLTFMeshOriginToCenter(const GLTFMesh* mesh)
	{
		float3 midpoint = CalculateFloat3Midpoint(mesh->positions);
		ApplyFloat3Subtraction(mesh->positions, midpoint);
		return midpoint;
	}

	// -------------------------------------------------------------------------------------------------------------------------------

}