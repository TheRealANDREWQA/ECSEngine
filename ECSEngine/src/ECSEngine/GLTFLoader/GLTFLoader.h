#pragma once
#include "../../Dependencies/cgltf/cgltf.h"
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"
#include "../Rendering/RenderingStructures.h"

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

	// Assumes that no other thread is using the same allocator;
	// The allocator must be thread local
	ECSENGINE_API bool LoadMeshFromGLTFFile(
		GLTFData data, 
		GLTFMesh& mesh, 
		MemoryManager* allocator, 
		unsigned int mesh_index = 0, 
		containers::CapacityStream<char>* error_message = nullptr
	);

	// Thread safe allocations
	ECSENGINE_API bool LoadMeshFromGLTFFileTs(
		GLTFData data, 
		GLTFMesh& mesh, 
		MemoryManager* allocator, 
		unsigned int mesh_index = 0, 
		containers::CapacityStream<char>* error_message = nullptr
	);

	// Assumes that no other thread is using the same allocator;
	// The allocator must be thread local; GLTFData contains the mesh count
	// that can be used to determine the size of the GLTFMesh strean
	ECSENGINE_API bool LoadMeshesFromGLTFFile(
		GLTFData data,
		GLTFMesh* meshes,
		MemoryManager* allocator,
		containers::CapacityStream<char>* error_message = nullptr
	);

	// Thread safe allocations; GLTFData contains the mesh count
	// that can be used to determine the size of the GLTFMesh strean
	ECSENGINE_API bool LoadMeshesFromGLTFFileTs(
		GLTFData data,
		GLTFMesh* meshes,
		MemoryManager* allocator,
		containers::CapacityStream<char>* error_message = nullptr
	);

	// Creates the appropriate vertex and index buffers
	ECSENGINE_API void GLTFMeshToMesh(Graphics* graphics, const GLTFMesh& gltf_mesh, Mesh& mesh);

	// Creates the appropriate vertex and index buffers
	ECSENGINE_API void GLTFMeshesToMeshes(Graphics* graphics, const GLTFMesh* gltf_meshes, Mesh* meshes, size_t count);

	// Not thread safe
	ECSENGINE_API void FreeGLTFMesh(const GLTFMesh& mesh, MemoryManager* allocator);

	// Thread safe
	ECSENGINE_API void FreeGLTFMeshTs(const GLTFMesh& mesh, MemoryManager* allocator);

	// Not thread safe
	ECSENGINE_API void FreeGLTFMeshes(const GLTFMesh* meshes, size_t count, MemoryManager* allocator);

	// Thread safe
	ECSENGINE_API void FreeGLTFMeshesTs(const GLTFMesh* meshes, size_t count, MemoryManager* allocator);

	ECSENGINE_API void FreeGLTFFile(GLTFData data);
}