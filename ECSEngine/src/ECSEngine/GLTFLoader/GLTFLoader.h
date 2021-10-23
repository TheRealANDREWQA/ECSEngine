#pragma once
#include "../../Dependencies/cgltf/cgltf.h"
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"
#include "../Rendering/RenderingStructures.h"
#include "../Allocators/AllocatorTypes.h"

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

		containers::Stream<float3> positions;
		containers::Stream<float3> normals;
		containers::Stream<float2> uvs;
		containers::Stream<Color> colors;
		containers::Stream<float4> skin_weights;
		containers::Stream<uint4> skin_influences;
		containers::Stream<float3> tangents;
		containers::Stream<unsigned int> indices;
	};

	// Cgltf uses const char* internally, so a conversion must be done
	ECSENGINE_API GLTFData LoadGLTFFile(const wchar_t* path, containers::CapacityStream<char>* error_message = nullptr);

	// Cgltf uses const char* internally, so a conversion must be done
	ECSENGINE_API GLTFData LoadGLTFFile(containers::Stream<wchar_t> path, containers::CapacityStream<char>* error_message = nullptr);

	ECSENGINE_API GLTFData LoadGLTFFile(const char* path, containers::CapacityStream<char>* error_message = nullptr);

	ECSENGINE_API GLTFData LoadGLTFFile(containers::Stream<char> path, containers::CapacityStream<char>* error_message = nullptr);

	ECSENGINE_API bool LoadMeshFromGLTF(
		GLTFData data, 
		GLTFMesh& mesh, 
		AllocatorPolymorphic allocator,
		unsigned int mesh_index = 0, 
		containers::CapacityStream<char>* error_message = nullptr
	);

	ECSENGINE_API bool LoadMaterialFromGLTF(
		GLTFData data,
		PBRMaterial& material,
		AllocatorPolymorphic allocator,
		unsigned int mesh_index = 0,
		containers::CapacityStream<char>* error_message = nullptr
	);

	ECSENGINE_API bool LoadMeshesFromGLTF(
		GLTFData data,
		GLTFMesh* meshes,
		AllocatorPolymorphic allocator,
		containers::CapacityStream<char>* error_message = nullptr
	);

	// For each mesh it will create a separate material
	ECSENGINE_API bool LoadMaterialsFromGLTF(
		GLTFData data,
		PBRMaterial* materials,
		AllocatorPolymorphic allocator,
		containers::CapacityStream<char>* error_message = nullptr
	);

	// It will discard materials that repeat - at most
	// data.count materials can be created
	ECSENGINE_API bool LoadDisjointMaterialsFromGLTF(
		GLTFData data,
		containers::Stream<PBRMaterial>& materials,
		AllocatorPolymorphic allocator,
		containers::CapacityStream<char>* error_message = nullptr
	);

	// GLTFData contains the mesh count that can be used to determine the size of the GLTFMesh strean
	ECSENGINE_API bool LoadMeshesAndMaterialsFromGLTF(
		GLTFData data,
		GLTFMesh* meshes,
		PBRMaterial* materials,
		AllocatorPolymorphic allocator,
		containers::CapacityStream<char>* error_message = nullptr
	);

	// Creates the appropriate vertex and index buffers
	ECSENGINE_API Mesh GLTFMeshToMesh(Graphics* graphics, const GLTFMesh& gltf_mesh);

	// Creates the appropriate vertex and index buffers
	ECSENGINE_API void GLTFMeshesToMeshes(Graphics* graphics, const GLTFMesh* gltf_meshes, Mesh* meshes, size_t count);

	ECSENGINE_API void FreeGLTFMesh(const GLTFMesh& mesh, AllocatorPolymorphic allocator);

	ECSENGINE_API void FreeGLTFMeshes(const GLTFMesh* meshes, size_t count, AllocatorPolymorphic allocator);

	ECSENGINE_API void FreeGLTFFile(GLTFData data);
}