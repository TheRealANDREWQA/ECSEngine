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

		const char* name;
		containers::Stream<float3> positions;
		containers::Stream<float3> normals;
		containers::Stream<float2> uvs;
		containers::Stream<Color> colors;
		containers::Stream<float4> skin_weights;
		containers::Stream<uint4> skin_influences;
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
		bool invert_z_axis = true,
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
		bool invert_z_axis = true,
		containers::CapacityStream<char>* error_message = nullptr
	);

	// For each mesh it will create a separate material
	ECSENGINE_API bool LoadMaterialsFromGLTF(
		GLTFData data,
		PBRMaterial* materials,
		AllocatorPolymorphic allocator,
		containers::CapacityStream<char>* error_message = nullptr
	);

	// It will discard materials that repeat - at most data.count materials can be created
	// The unsigned int stream will contain indices for each submesh in order to identify 
	// the material that is using
	ECSENGINE_API bool LoadDisjointMaterialsFromGLTF(
		GLTFData data,
		containers::Stream<PBRMaterial>& materials,
		containers::Stream<unsigned int>& submesh_material_index,
		AllocatorPolymorphic allocator,
		containers::CapacityStream<char>* error_message = nullptr
	);

	// GLTFData contains the mesh count that can be used to determine the size of the GLTFMesh stream
	// It creates a pbr material for every mesh
	ECSENGINE_API bool LoadMeshesAndMaterialsFromGLTF(
		GLTFData data,
		GLTFMesh* meshes,
		PBRMaterial* materials,
		AllocatorPolymorphic allocator,
		bool invert_z_axis = true,
		containers::CapacityStream<char>* error_message = nullptr
	);

	// GLTFData contains the mesh count that can be used to determine the size of the GLTFMesh stream
	// It creates only unique pbr materials and an index mask for each submesh to reference the original 
	// material
	ECSENGINE_API bool LoadMeshesAndDisjointMaterialsFromGLTF(
		GLTFData data,
		GLTFMesh* meshes,
		containers::Stream<PBRMaterial>& materials,
		unsigned int* submesh_material_index,
		AllocatorPolymorphic allocator,
		bool invert_z_axis = true,
		containers::CapacityStream<char>* error_message = nullptr
	);

	// Can run on multiple threads
	// Creates the appropriate vertex and index buffers
	// Currently misc_flags can be set to D3D11_RESOURCE_MISC_SHARED to enable sharing of the vertex buffers across devices
	ECSENGINE_API Mesh GLTFMeshToMesh(Graphics* graphics, const GLTFMesh& gltf_mesh, unsigned int misc_flags = 0);

	// Can run on multiple threads
	// Creates the appropriate vertex and index buffers
	// Currently misc_flags can be set to D3D11_RESOURCE_MISC_SHARED to enable sharing of the vertex buffers across devices
	ECSENGINE_API void GLTFMeshesToMeshes(Graphics* graphics, const GLTFMesh* gltf_meshes, Mesh* meshes, size_t count, unsigned int misc_flags = 0);

	// SINGLE THREADED - relies on the context to copy the resources
	// Merges the submeshes that have the same material into the same buffer
	// Material count submeshes will be created
	// The returned mesh will have no name associated with it
	// The submeshes will inherit the mesh name
	// Currently misc_flags can be set to D3D11_RESOURCE_MISC_SHARED to enable sharing of the vertex buffers across devices
	ECSENGINE_API Mesh GLTFMeshesToMergedMesh(
		Graphics* graphics, 
		GLTFMesh* gltf_meshes, 
		Submesh* submeshes,
		unsigned int* submesh_material_index, 
		size_t material_count,
		size_t count,
		unsigned int misc_flags = 0
	);

	ECSENGINE_API void FreeGLTFMesh(const GLTFMesh& mesh, AllocatorPolymorphic allocator);

	ECSENGINE_API void FreeGLTFMeshes(const GLTFMesh* meshes, size_t count, AllocatorPolymorphic allocator);

	ECSENGINE_API void FreeGLTFFile(GLTFData data);
}