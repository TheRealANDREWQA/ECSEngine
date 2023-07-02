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
		bool coallesce_submesh_name_allocations = true;
		float scale_factor = 1.0f;
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
		PBRMaterial* materials,
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
	// Currently misc_flags can be set to D3D11_RESOURCE_MISC_SHARED to enable sharing of the vertex buffers across devices
	ECSENGINE_API Mesh GLTFMeshToMesh(Graphics* graphics, const GLTFMesh& gltf_mesh, ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE);

	// Can run on multiple threads
	// Creates the appropriate vertex and index buffers
	// Currently misc_flags can be set to D3D11_RESOURCE_MISC_SHARED to enable sharing of the vertex buffers across devices
	ECSENGINE_API void GLTFMeshesToMeshes(Graphics* graphics, const GLTFMesh* gltf_meshes, Mesh* meshes, size_t count, ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE);

	// SINGLE THREADED - relies on the context to copy the resources
	// Merges the submeshes that have the same material into the same buffer
	// PBRMaterial count submeshes will be created
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
		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE
	);

	// Returns a coallesced mesh with 0 submeshes if it fails.
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

	ECSENGINE_API void FreeGLTFMeshes(const GLTFMesh* meshes, size_t count, AllocatorPolymorphic allocator);

	ECSENGINE_API void FreeCoallescedGLTFMesh(const GLTFMesh& mesh, AllocatorPolymorphic allocator);

	ECSENGINE_API void FreeGLTFFile(GLTFData data);

	ECSENGINE_API void ScaleGLTFMeshes(const GLTFMesh* gltf_meshes, size_t count, float scale_factor);

}