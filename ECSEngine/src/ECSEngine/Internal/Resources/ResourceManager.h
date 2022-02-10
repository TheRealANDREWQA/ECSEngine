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

	ECS_CONTAINERS;

	using ResourceManagerHash = HashFunctionMultiplyString;
	using ResourceManagerAllocator = MemoryManager;
	
	// The time stamp can be used to compare against a new stamp and check to see if the resource is out of date
	struct ResourceManagerEntry {
		DataPointer data_pointer;
		size_t time_stamp;
	};

	// Embedd the reference count inside the pointer
	using ResourceManagerTable = HashTable<ResourceManagerEntry, ResourceIdentifier, HashFunctionPowerOfTwo, ResourceManagerHash>;

	constexpr size_t ECS_RESOURCE_MANAGER_MASK_INCREMENT_COUNT = USHORT_MAX;

	// The allocator can be provided for multithreaded loading of the textures
	struct ECSENGINE_API ResourceManagerTextureDesc {
		GraphicsContext* context = nullptr;
		D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
		unsigned int bind_flags = D3D11_BIND_SHADER_RESOURCE;
		unsigned int cpu_flags = 0;
		unsigned int misc_flags = 0;
		AllocatorPolymorphic allocator = { nullptr };
		TextureCompressionExplicit compression = (TextureCompressionExplicit)-1;
	};

	ECSENGINE_API MemoryManager DefaultResourceManagerAllocator(GlobalMemoryManager* global_allocator);

	struct ResourceManagerLoadDesc {
		size_t load_flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT;
		bool* reference_counted_is_loaded = nullptr;
		unsigned int thread_index = 0;
	};

	// Defining ECS_RESOURCE_MANAGER_CHECK_RESOURCE will make AddResource check if the resource exists already 
	struct ECSENGINE_API ResourceManager
	{
		struct InternalResourceType {
			const char* name;
			ResourceManagerTable table;
		};

		struct ThreadResource {
			CapacityStream<wchar_t> characters;
			CapacityStream<unsigned int> offsets;
		};

	//public:
		ResourceManager(
			ResourceManagerAllocator* memory,
			Graphics* graphics,
			unsigned int thread_count
		);

		ResourceManager(const ResourceManager& other) = default;
		ResourceManager& operator = (const ResourceManager& other) = default;

		// Each thread has its own resource path
		void AddResourcePath(const char* subpath, unsigned int thread_index = 0);

		// Each thread has its own resource path
		void AddResourcePath(const wchar_t* subpath, unsigned int thread_index = 0);

		// Create a new resource type - must be power of two size
		void AddResourceType(ResourceType type, unsigned int resource_count);

		// Inserts the resource into the table without loading - it might allocate in order to maintain resource invariation
		// Reference count USHORT_MAX means no reference counting. The time stamp is optional (0 if the stamp is not needed)
		void AddResource(
			const wchar_t* path, 
			ResourceType resource_type,
			void* resource, 
			size_t time_stamp = 0, 
			unsigned short reference_count = USHORT_MAX
		);

		void* Allocate(size_t size);

		void* AllocateTs(size_t size);

		AllocatorPolymorphic Allocator();

		void Deallocate(const void* data);

		void DeallocateTs(const void* data);

		template<bool delete_if_zero = true>
		void DecrementReferenceCount(ResourceType type, unsigned int amount);

		// Pops it off the stack
		void DeleteResourcePath(unsigned int thread_index = 0);

		// Checks to see if the resource exists
		bool Exists(ResourceIdentifier identifier, ResourceType type) const;

		// Checks to see if the resource exists and returns its position as index inside the hash table
		bool Exists(ResourceIdentifier identifier, ResourceType type, unsigned int& table_index) const;

		// Walks through the table and removes all resources which are outdated - implies that all resource identifiers have as their
		// ptr the path to the resource
		void EvictOutdatedResources(ResourceType type);

		// It returns -1 if the resource doesn't exist
		int GetResourceIndex(ResourceIdentifier identifier, ResourceType type) const;

		// It returns nullptr if the resource doesn't exist
		void* GetResource(ResourceIdentifier identifier, ResourceType type);

		// Returns the amount of characters written
		size_t GetThreadPath(wchar_t* characters, unsigned int thread_index = 0) const;

		// Returns the complete path by concatenating the thread path and the given path
		Stream<wchar_t> GetThreadPath(wchar_t* characters, const wchar_t* current_path, unsigned int thread_index = 0) const;

		// Returns the complete path by concatenating the thread path and the given path
		Stream<wchar_t> GetThreadPath(wchar_t* characters, const char* current_path, unsigned int thread_index = 0) const;

		// Returns -1 if the identifier doesn't exist
		size_t GetTimeStamp(ResourceIdentifier identifier, ResourceType type) const;

		// Returns an entry with the data pointer set to nullptr and the time stamp to -1
		ResourceManagerEntry GetEntry(ResourceIdentifier identifier, ResourceType type) const;

		// Returns nullptr if the identifier doesn't exist
		ResourceManagerEntry* GetEntryPtr(ResourceIdentifier identifier, ResourceType type);

		// Returns true whether or not the given stamp is greater than the stamp currently registered. If the entry doesn't exist,
		// it returns false
		bool IsResourceOutdated(ResourceIdentifier identifier, ResourceType type, size_t new_stamp);

		// It will create all resource types with default resource count
		void InitializeDefaultTypes();

		void IncrementReferenceCount(ResourceType type, unsigned int amount);

		// resource folder path different from -1 will use the folder in the specified thread position
		template<bool reference_counted = false>
		char* LoadTextFile(
			const wchar_t* filename,
			size_t* size,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		char* LoadTextFileImplementation(const wchar_t* file, size_t* size);

		// Same as OpenFile the function, the only difference is that it takes into consideration the thread path
		// The file must be released if successful
		bool OpenFile(const wchar_t* filename, ECS_FILE_HANDLE* file, bool binary = true, unsigned int thread_index = 0);

		// In order to generate mip-maps, the context must be supplied
		template<bool reference_counted = false>
		ResourceView LoadTexture(
			const wchar_t* filename,
			ResourceManagerTextureDesc* descriptor,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// In order to generate mip-maps, the context must be supplied
		ResourceView LoadTextureImplementation(
			const wchar_t* filename, 
			ResourceManagerTextureDesc* descriptor,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all meshes from a gltf file
		template<bool reference_counted = false>
		Stream<Mesh>* LoadMeshes(
			const wchar_t* filename,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all meshes from a gltf file
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		Stream<Mesh>* LoadMeshImplementation(const wchar_t* filename, ResourceManagerLoadDesc load_descriptor = {});

		// Loads all meshes from a gltf file and creates a coallesced mesh
		template<bool reference_counted = false>
		CoallescedMesh* LoadCoallescedMesh(
			const wchar_t* filename,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all meshes from a gltf file and creates a coallesced mesh
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		CoallescedMesh* LoadCoallescedMeshImplementation(const wchar_t* filename, ResourceManagerLoadDesc load_descriptor = {});

		// Loads all materials from a gltf file
		template<bool reference_counted = false>
		Stream<PBRMaterial>* LoadMaterials(
			const wchar_t* filename,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all materials from a gltf file
		Stream<PBRMaterial>* LoadMaterialsImplementation(const wchar_t* filename, ResourceManagerLoadDesc load_descriptor = {});

		// Loads all meshes and materials from a gltf file, combines the meshes into a single one sorted by material submeshes
		template<bool reference_counted = false>
		PBRMesh* LoadPBRMesh(
			const wchar_t* filename,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all meshes and materials from a gltf file, combines the meshes into a single one sorted by material submeshes
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		PBRMesh* LoadPBRMeshImplementation(const wchar_t* filename, ResourceManagerLoadDesc load_descriptor = {});

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary using shader compile options and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// It is the responsability of the calling code to release the shader source code using Deallocate/DeallocateTs
		template<bool reference_counted = false>
		VertexShader* LoadVertexShader(
			const wchar_t* filename, 
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
			const wchar_t* filename, 
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
			const wchar_t* filename,
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
			const wchar_t* filename, 
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
			const wchar_t* filename, 
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
			const wchar_t* filename, 
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
			const wchar_t* filename, 
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
			const wchar_t* filename,
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
			const wchar_t* filename, 
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
			const wchar_t* filename, 
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
			const wchar_t* filename, 
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
			const wchar_t* filename, 
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

		void SetShaderDirectory(Stream<wchar_t> directory);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadTextFile(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadTextFile(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadTextFileImplementation(char* data, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadTexture(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadTexture(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadTextureImplementation(ResourceView texture, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadMeshes(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadMeshes(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadMeshesImplementation(Stream<Mesh>* meshes, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadCoallescedMesh(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadCoallescedMesh(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadCoallescedMeshImplementation(CoallescedMesh* mesh, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);
			
		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadMaterials(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadMaterials(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadMaterialsImplementation(Stream<PBRMaterial>* materials, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadPBRMesh(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadPBRMesh(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadPBRMeshImplementation(PBRMesh* mesh, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadVertexShader(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadVertexShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadVertexShaderImplementation(VertexShader vertex_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadPixelShader(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadPixelShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadPixelShaderImplementation(PixelShader pixel_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadHullShader(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadHullShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadHullShaderImplementation(HullShader hull_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadDomainShader(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadDomainShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadDomainShaderImplementation(DomainShader domain_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadGeometryShader(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadGeometryShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadGeometryShaderImplementation(GeometryShader geometry_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadComputeShader(const wchar_t* filename, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadComputeShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadComputeShaderImplementation(ComputeShader compute_shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadResource(const wchar_t* filename, ResourceType type, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		template<bool reference_counted = false>
		void UnloadResource(unsigned int index, ResourceType type, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadResourceImplementation(void* resource, ResourceType type, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		// Unloads all resources of a type
		void UnloadAll(ResourceType resource_type);

		// ---------------------------------------------------------------------------------------------------------------------------

	//private:
		Graphics* m_graphics;
		ResourceManagerAllocator* m_memory;
		ResizableStream<InternalResourceType> m_resource_types;
		CapacityStream<ThreadResource> m_thread_resources;
		CapacityStream<wchar_t> m_shader_directory;
	};

#pragma region Free functions

	// --------------------------------------------------------------------------------------------------------------------

	// The name nullptr means it failed
	ECSENGINE_API Material PBRToMaterial(ResourceManager* resource_manager, const PBRMaterial& pbr, Stream<wchar_t> folder_to_search);

	// --------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------

#pragma endregion

}

