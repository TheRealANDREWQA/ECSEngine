#pragma once
#include "../../Core.h"
#include "ecspch.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Containers/Stream.h"
#include "../../Containers/HashTable.h"
#include "../../Containers/Stacks.h"
#include "../InternalStructures.h"
#include "../../Utilities/Function.h"
#include "../../Containers/DataPointer.h"
#include "../../Rendering/Graphics.h"
#include "../../Rendering/Compression/TextureCompressionTypes.h"
#include "../../Internal/Multithreading/TaskManager.h"
#include "ResourceTypes.h"

#define ECS_RESOURCE_MANAGER_FLAG_DEFAULT 1

#define ECS_RESOURCE_MANAGER_TEMPORARY (1 << 16)
#define ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT (1 << 17)
#define ECS_RESOURCE_MANAGER_SHARED_RESOURCE (1 << 18)

namespace ECSEngine {

	using ResourceManagerAllocator = MemoryManager;
	
	// The time stamp can be used to compare against a new stamp and check to see if the resource is out of date
	struct ResourceManagerEntry {
		DataPointer data_pointer;
		size_t time_stamp;
	};

	// Embedd the reference count inside the pointer
	using ResourceManagerTable = HashTableDefault<ResourceManagerEntry>;

	constexpr size_t ECS_RESOURCE_MANAGER_MASK_INCREMENT_COUNT = USHORT_MAX;

	// The allocator can be provided for multithreaded loading of the textures
	struct ECSENGINE_API ResourceManagerTextureDesc {
		GraphicsContext* context = nullptr;
		ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DEFAULT;
		ECS_GRAPHICS_BIND_TYPE bind_flags = ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		ECS_GRAPHICS_CPU_ACCESS cpu_flags = ECS_GRAPHICS_CPU_ACCESS_NONE;
		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE;
		AllocatorPolymorphic allocator = { nullptr };
		ECS_TEXTURE_COMPRESSION_EX compression = (ECS_TEXTURE_COMPRESSION_EX)-1;
	};

	ECSENGINE_API MemoryManager DefaultResourceManagerAllocator(GlobalMemoryManager* global_allocator);

	ECSENGINE_API size_t DefaultResourceManagerAllocatorSize();

	struct ResourceManagerLoadDesc {
		size_t load_flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT;
		bool* reference_counted_is_loaded = nullptr;
		unsigned int thread_index = 0;
	};

	// Defining ECS_RESOURCE_MANAGER_CHECK_RESOURCE will make AddResource check if the resource exists already 
	struct ECSENGINE_API ResourceManager
	{
		ResourceManager(
			ResourceManagerAllocator* memory,
			Graphics* graphics,
			unsigned int thread_count
		);

		ResourceManager(const ResourceManager& other) = default;
		ResourceManager& operator = (const ResourceManager& other) = default;

		// Inserts the resource into the table without loading - it might allocate in order to maintain resource invariation
		// Reference count USHORT_MAX means no reference counting. The time stamp is optional (0 if the stamp is not needed)
		void AddResource(
			ResourceIdentifier identifier, 
			ResourceType resource_type,
			void* resource, 
			size_t time_stamp = 0, 
			unsigned short reference_count = USHORT_MAX
		);

		void AddShaderDirectory(Stream<wchar_t> directory);

		void* Allocate(size_t size);

		void* AllocateTs(size_t size);

		AllocatorPolymorphic Allocator() const;

		AllocatorPolymorphic AllocatorTs() const;

		void Deallocate(const void* data);

		void DeallocateTs(const void* data);

		template<bool delete_if_zero = true>
		void DecrementReferenceCount(ResourceType type, unsigned int amount);

		// Checks to see if the resource exists
		bool Exists(ResourceIdentifier identifier, ResourceType type) const;

		// Checks to see if the resource exists and returns its position as index inside the hash table
		bool Exists(ResourceIdentifier identifier, ResourceType type, unsigned int& table_index) const;

		// Removes a resource from the table - it does not unload it.
		void EvictResource(ResourceIdentifier identifier, ResourceType type);

		// Removes all resources from the tables without unloading them that appear in the other resource manager
		void EvictResourcesFrom(const ResourceManager* other);

		// Walks through the table and removes all resources which are outdated - implies that all resource identifiers have as their
		// ptr the path to the resource
		void EvictOutdatedResources(ResourceType type);

		// It returns -1 if the resource doesn't exist
		int GetResourceIndex(ResourceIdentifier identifier, ResourceType type) const;

		// It returns nullptr if the resource doesn't exist
		void* GetResource(ResourceIdentifier identifier, ResourceType type);

		// Returns -1 if the identifier doesn't exist
		size_t GetTimeStamp(ResourceIdentifier identifier, ResourceType type) const;

		// Returns an entry with the data pointer set to nullptr and the time stamp to -1
		ResourceManagerEntry GetEntry(ResourceIdentifier identifier, ResourceType type) const;

		// Returns nullptr if the identifier doesn't exist
		ResourceManagerEntry* GetEntryPtr(ResourceIdentifier identifier, ResourceType type);

		// Returns true whether or not the given stamp is greater than the stamp currently registered. If the entry doesn't exist,
		// it returns false
		bool IsResourceOutdated(ResourceIdentifier identifier, ResourceType type, size_t new_stamp);

		void IncrementReferenceCount(ResourceType type, unsigned int amount);

		// It will insert the resources already loaded from the other resource manager into the instance's
		// tables. The timestamps will be 0, and will have no reference count. If the graphics object
		// differs between the 2 resource managers, then it will create another shared instance for the
		// GPU resources which require this
		void InheritResources(const ResourceManager* other);

		// resource folder path different from -1 will use the folder in the specified thread position
		template<bool reference_counted = false>
		char* LoadTextFile(
			Stream<wchar_t> filename,
			size_t* size,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		char* LoadTextFileImplementation(Stream<wchar_t> file, size_t* size);

		// Same as OpenFile the function, the only difference is that it takes into consideration the thread path
		// The file must be released if successful
		bool OpenFile(Stream<wchar_t> filename, ECS_FILE_HANDLE* file, bool binary = true, unsigned int thread_index = 0);

		// In order to generate mip-maps, the context must be supplied
		template<bool reference_counted = false>
		ResourceView LoadTexture(
			Stream<wchar_t> filename,
			ResourceManagerTextureDesc* descriptor,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// In order to generate mip-maps, the context must be supplied
		ResourceView LoadTextureImplementation(
			Stream<wchar_t> filename, 
			ResourceManagerTextureDesc* descriptor,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all meshes from a gltf file
		template<bool reference_counted = false>
		Stream<Mesh>* LoadMeshes(
			Stream<wchar_t> filename,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all meshes from a gltf file
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		Stream<Mesh>* LoadMeshImplementation(Stream<wchar_t> filename, ResourceManagerLoadDesc load_descriptor = {});

		// Loads all meshes from a gltf file and creates a coallesced mesh
		template<bool reference_counted = false>
		CoallescedMesh* LoadCoallescedMesh(
			Stream<wchar_t> filename,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all meshes from a gltf file and creates a coallesced mesh
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		CoallescedMesh* LoadCoallescedMeshImplementation(Stream<wchar_t> filename, ResourceManagerLoadDesc load_descriptor = {});

		// Loads all materials from a gltf file
		template<bool reference_counted = false>
		Stream<PBRMaterial>* LoadMaterials(
			Stream<wchar_t> filename,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all materials from a gltf file
		Stream<PBRMaterial>* LoadMaterialsImplementation(Stream<wchar_t> filename, ResourceManagerLoadDesc load_descriptor = {});

		// Loads all meshes and materials from a gltf file, combines the meshes into a single one sorted by material submeshes
		template<bool reference_counted = false>
		PBRMesh* LoadPBRMesh(
			Stream<wchar_t> filename,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all meshes and materials from a gltf file, combines the meshes into a single one sorted by material submeshes
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		PBRMesh* LoadPBRMeshImplementation(Stream<wchar_t> filename, ResourceManagerLoadDesc load_descriptor = {});

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary using shader compile options and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		template<bool reference_counted = false>
		VertexShader* LoadVertexShader(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr, 
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		VertexShader LoadVertexShaderImplementation(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		template<bool reference_counted = false>
		PixelShader* LoadPixelShader(
			Stream<wchar_t> filename,
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		PixelShader LoadPixelShaderImplementation(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		template<bool reference_counted = false>
		HullShader* LoadHullShader(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		HullShader LoadHullShaderImplementation(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		template<bool reference_counted = false>
		DomainShader* LoadDomainShader(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		DomainShader LoadDomainShaderImplementation(
			Stream<wchar_t> filename,
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		template<bool reference_counted = false>
		GeometryShader* LoadGeometryShader(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		GeometryShader LoadGeometryShaderImplementation(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		template<bool reference_counted = false>
		ComputeShader* LoadComputeShader(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		ComputeShader LoadComputeShaderImplementation(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// ---------------------------------------------------------------------------------------------------------------------------
		
		// Reassigns a value to a resource that has been loaded; the resource is first destroyed then reassigned
		void RebindResource(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource);

		// Reassigns a value to a resource that has been loaded; no destruction is being done
		void RebindResourceNoDestruction(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource);

		void RemoveReferenceCountForResource(ResourceIdentifier identifier, ResourceType resource_type);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadTextFile(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadTextFile(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadTextFileImplementation(char* data, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadTexture(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadTexture(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadTextureImplementation(ResourceView texture, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadMeshes(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadMeshes(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadMeshesImplementation(Stream<Mesh>* meshes, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadCoallescedMesh(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadCoallescedMesh(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadCoallescedMeshImplementation(CoallescedMesh* mesh, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);
			
		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadMaterials(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadMaterials(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadMaterialsImplementation(Stream<PBRMaterial>* materials, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadPBRMesh(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadPBRMesh(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadPBRMeshImplementation(PBRMesh* mesh, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadVertexShader(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadVertexShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadVertexShaderImplementation(VertexShader vertex_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadPixelShader(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadPixelShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadPixelShaderImplementation(PixelShader pixel_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadHullShader(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadHullShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadHullShaderImplementation(HullShader hull_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadDomainShader(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadDomainShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadDomainShaderImplementation(DomainShader domain_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadGeometryShader(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadGeometryShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadGeometryShaderImplementation(GeometryShader geometry_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadComputeShader(Stream<wchar_t> filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadComputeShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadComputeShaderImplementation(ComputeShader compute_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadResource(Stream<wchar_t> filename, ResourceType type, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadResource(unsigned int index, ResourceType type, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadResourceImplementation(void* resource, ResourceType type, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		// Unloads everything
		void UnloadAll();

		// Unloads all resources of a type
		void UnloadAll(ResourceType resource_type);

		// ---------------------------------------------------------------------------------------------------------------------------

	//private:
		Graphics* m_graphics;
		ResourceManagerAllocator* m_memory;
		Stream<ResourceManagerTable> m_resource_types;
		ResizableStream<Stream<wchar_t>> m_shader_directory;
	};

#pragma region Free functions

	// --------------------------------------------------------------------------------------------------------------------

	// The name nullptr means it failed
	ECSENGINE_API Material PBRToMaterial(ResourceManager* resource_manager, const PBRMaterial& pbr, Stream<wchar_t> folder_to_search);

	// --------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------

#pragma endregion

}

