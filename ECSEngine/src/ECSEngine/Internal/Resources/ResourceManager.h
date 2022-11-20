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
#include "../../Rendering/GraphicsHelpers.h"
#include "../../Rendering/Compression/TextureCompressionTypes.h"
#include "../../Internal/Multithreading/TaskManager.h"
#include "ResourceTypes.h"

#define ECS_RESOURCE_MANAGER_FLAG_DEFAULT 1

#define ECS_RESOURCE_MANAGER_TEMPORARY (1 << 16)
#define ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT (1 << 17)

// For the variant that takes the GLTFMeshes, if the scale is not 1.0f, this disables the
// rescaling of the meshes (it rescales them back to appear the same to the outside caller)
#define ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK (1 << 18)

// For the variant that takes the GLTFMeshes, if the scale is not 1.0f, this disables the
// rescaling of the meshes (it rescales them back to appear the same to the outside caller)
#define ECS_RESOURCE_MANAGER_COALLESCED_MESH_EX_DO_NOT_SCALE_BACK (1 << 19)

// Applies a tonemap on the texture pixels before sending to the GPU (useful for thumbnails)
#define ECS_RESOURCE_MANAGER_TEXTURE_HDR_TONEMAP (1 << 17)

// Tells the loader to not try to insert into the resource table (in order to avoid 
// multithreading race conditions). This applies both for loading and unloading
#define ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_LOAD (1 << 18)

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
	// For the generation of the mip maps only the context needs to be set. The misc flag is optional
	struct ResourceManagerTextureDesc {
		GraphicsContext* context = nullptr;
		ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DEFAULT;
		ECS_GRAPHICS_BIND_TYPE bind_flags = ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		ECS_GRAPHICS_CPU_ACCESS cpu_flags = ECS_GRAPHICS_CPU_ACCESS_NONE;
		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE;
		ECS_TEXTURE_COMPRESSION_EX compression = ECS_TEXTURE_COMPRESSION_EX_NONE;
		bool srgb = false;
		AllocatorPolymorphic allocator = { nullptr };
	};

	ECSENGINE_API ResourceManagerAllocator DefaultResourceManagerAllocator(GlobalMemoryManager* global_allocator);

	ECSENGINE_API size_t DefaultResourceManagerAllocatorSize();

	ECSENGINE_API ResourceType FromShaderTypeToResourceType(ECS_SHADER_TYPE shader_type);

	struct GLTFData;
	struct GLTFMesh;

	struct ResourceManagerLoadDesc {
		// This can be used as unload flags as well
		size_t load_flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT;
		// This can be used for unload as well
		bool* reference_counted_is_loaded = nullptr;
		// A suffix used to differentiate between different load parameters on the same resource
		// For example if having multiple variants of the same texture with different compression
		// In this way a unique name can be generated such that they map to different resource.
		// You need to come up in a way to differentiate between the parameters and the identifier
		// (a byte concatenation should be good enough)
		Stream<void> identifier_suffix = { nullptr, 0 };
		// Only for graphics resources. If they need access to the immediate context then they will acquire this
		// lock before doing the operation
		SpinLock* gpu_lock = nullptr;
	};

	struct ResourceManagerExDesc {
		ECS_INLINE bool HasFilename() const {
			return filename.size > 0;
		}

		ECS_INLINE void Lock() {
			if (push_lock != nullptr) {
				push_lock->lock();
			}
		}

		ECS_INLINE void Unlock() {
			if (push_lock != nullptr) {
				push_lock->unlock();
			}
		}

		Stream<wchar_t> filename = { nullptr, 0 };
		size_t time_stamp = 0;
		SpinLock* push_lock = nullptr;
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
			Stream<void> suffix = { nullptr, 0 },
			unsigned short reference_count = USHORT_MAX
		);

		void AddShaderDirectory(Stream<wchar_t> directory);

		void* Allocate(size_t size);

		void* AllocateTs(size_t size);

		AllocatorPolymorphic Allocator() const;

		AllocatorPolymorphic AllocatorTs() const;

		void Deallocate(const void* data);

		void DeallocateTs(const void* data);

		// Decrements the reference count all resources of that type
		template<bool delete_if_zero = true>
		void DecrementReferenceCount(ResourceType type, unsigned int amount);

		// Checks to see if the resource exists
		bool Exists(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = { nullptr, 0 }) const;

		// Checks to see if the resource exists and returns its position as index inside the hash table
		bool Exists(ResourceIdentifier identifier, ResourceType type, unsigned int& table_index, Stream<void> suffix = { nullptr, 0 }) const;

		// Removes a resource from the table - it does not unload it.
		void EvictResource(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = { nullptr, 0 });

		// Removes all resources from the tables without unloading them that appear in the other resource manager
		void EvictResourcesFrom(const ResourceManager* other);

		// Walks through the table and removes all resources which are outdated - implies that all resource identifiers have as their
		// ptr the path to the resource
		void EvictOutdatedResources(ResourceType type);

		// It returns -1 if the resource doesn't exist
		unsigned int GetResourceIndex(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = { nullptr, 0 }) const;

		// It returns nullptr if the resource doesn't exist
		void* GetResource(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = { nullptr, 0 });

		// It returns nullptr if the resource doesn't exist
		const void* GetResource(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = { nullptr, 0 }) const;

		// Returns -1 if the identifier doesn't exist
		size_t GetTimeStamp(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = { nullptr, 0 }) const;

		// Returns an entry with the data pointer set to nullptr and the time stamp to -1
		ResourceManagerEntry GetEntry(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = { nullptr, 0 }) const;

		// Returns nullptr if the identifier doesn't exist
		ResourceManagerEntry* GetEntryPtr(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = { nullptr, 0 });

		// Returns true whether or not the given stamp is greater than the stamp currently registered. If the entry doesn't exist,
		// it returns false. The suffix is optional (can either bake it into the identifier or give it to the function to do it)
		bool IsResourceOutdated(ResourceIdentifier identifier, ResourceType type, size_t new_stamp, Stream<void> suffix = { nullptr, 0 });

		// Increases the referece count for all resources of that type
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

		// In order to generate mip-maps, the context must be supplied
		// FLAGS: ECS_RESOURCE_MANAGER_TEXTURE_HDR_TONEMAP
		template<bool reference_counted = false>
		ResourceView LoadTexture(
			Stream<wchar_t> filename,
			const ResourceManagerTextureDesc* descriptor,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// In order to generate mip-maps, the context must be supplied
		// FLAGS: ECS_RESOURCE_MANAGER_TEXTURE_HDR_TONEMAP
		ResourceView LoadTextureImplementation(
			Stream<wchar_t> filename, 
			const ResourceManagerTextureDesc* descriptor,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// A more detailed version that doesn't load the file data nor deallocate it when it's done using it
		// Instead only applies the given options. The time stamp is needed only when specifying filename
		ResourceView LoadTextureImplementationEx(
			DecodedTexture decoded_texture,
			const ResourceManagerTextureDesc* descriptor,
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// Loads all meshes from a gltf file. If it fails it returns nullptr. It allocates memory from the allocator
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		template<bool reference_counted = false>
		Stream<Mesh>* LoadMeshes(
			Stream<wchar_t> filename,
			float scale_factor = 1.0f,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all meshes from a gltf file
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		Stream<Mesh>* LoadMeshImplementation(Stream<wchar_t> filename, float scale_factor = 1.0f, ResourceManagerLoadDesc load_descriptor = {});

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr. It allocates memory from the allocator
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT.
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		Stream<Mesh>* LoadMeshImplementationEx(
			const GLTFData* data, 
			float scale_factor = 1.0f,
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr. It allocates memory from the allocator
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT, ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK.
		// It will also insert it into the table
		Stream<Mesh>* LoadMeshImplementationEx(
			Stream<GLTFMesh> meshes,
			float scale_factor = 1.0f, 
			ResourceManagerLoadDesc load_descriptor = {}, 
			ResourceManagerExDesc* ex_desc = {}
		);

		// Loads all meshes from a gltf file and creates a coallesced mesh
		template<bool reference_counted = false>
		CoallescedMesh* LoadCoallescedMesh(
			Stream<wchar_t> filename,
			float scale_factor = 1.0f,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all meshes from a gltf file and creates a coallesced mesh
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		CoallescedMesh* LoadCoallescedMeshImplementation(Stream<wchar_t> filename, float scale_factor = 1.0f, ResourceManagerLoadDesc load_descriptor = {});

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr. It allocates memory from the allocator
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT, ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK.
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		CoallescedMesh* LoadCoallescedMeshImplementationEx(
			const GLTFData* data,
			float scale_factor = 1.0f,
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr. It allocates memory from the allocator
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT, ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK.
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		CoallescedMesh* LoadCoallescedMeshImplementationEx(
			Stream<GLTFMesh> gltf_meshes,
			float scale_factor = 1.0f,
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// A more detailed version that directly takes the data and does not deallocate it
		// This function cannot fail (unless GPU memory runs out). It allocates memory from the allocator.
		// The submeshes are allocated as well, they are not referenced
		// Flags: ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		CoallescedMesh* LoadCoallescedMeshImplementationEx(
			const GLTFMesh* gltf_mesh,
			Stream<Submesh> submeshes,
			float scale_factor = 1.0f,
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// Loads all materials from a gltf file
		template<bool reference_counted = false>
		Stream<PBRMaterial>* LoadMaterials(
			Stream<wchar_t> filename,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all materials from a gltf file
		Stream<PBRMaterial>* LoadMaterialsImplementation(Stream<wchar_t> filename, ResourceManagerLoadDesc load_descriptor = {});

		// Converts a list of textures into a material. If it fails (a path doesn't exist, or a shader is invalid)
		// it returns false and rolls back any loads that have been done
		bool LoadUserMaterial(
			const UserMaterial* user_material,
			Material* converted_material,
			ResourceManagerLoadDesc load_descriptor = {}
		);

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
		template<bool reference_counted = false>
		VertexShader* LoadVertexShader(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			Stream<void>* byte_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		VertexShader LoadVertexShaderImplementation(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			Stream<void>* byte_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr.
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		VertexShader LoadVertexShaderImplementationEx(
			Stream<char> source_code,
			Stream<void>* byte_code = nullptr,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
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
		PixelShader LoadPixelShaderImplementation(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr.
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		PixelShader LoadPixelShaderImplementationEx(
			Stream<char> source_code,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
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
		HullShader LoadHullShaderImplementation(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr.
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		HullShader LoadHullShaderImplementationEx(
			Stream<char> source_code,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
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
		DomainShader LoadDomainShaderImplementation(
			Stream<wchar_t> filename,
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr.
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		DomainShader LoadDomainShaderImplementationEx(
			Stream<char> source_code,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
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
		GeometryShader LoadGeometryShaderImplementation(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr.
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		GeometryShader LoadGeometryShaderImplementationEx(
			Stream<char> source_code,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
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
		ComputeShader LoadComputeShaderImplementation(
			Stream<wchar_t> filename, 
			Stream<char>* shader_source_code = nullptr,
			ShaderCompileOptions options = {}, 
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr.
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		ComputeShader LoadComputeShaderImplementationEx(
			Stream<char> source_code,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// ---------------------------------------------------------------------------------------------------------------------------

		// Returns a pointer to the shader type (for vertex shader a VertexShader*)
		template<bool reference_counted = false>
		void* LoadShader(
			Stream<wchar_t> filename,
			ECS_SHADER_TYPE shader_type,
			Stream<char>* shader_source_code = nullptr,
			Stream<void>* byte_code = nullptr,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Returns a pointer to the shader type (for vertex shader a VertexShader*)
		void* LoadShaderImplementation(
			Stream<wchar_t> filename,
			ECS_SHADER_TYPE shader_type,
			Stream<char>* shader_source_code = nullptr,
			Stream<void>* byte_code = nullptr,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Returns a pointer to the interface type (for vertex shader a ID3D11VertexShader*)
		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr. It allocates memory from the allocator if the filename is specified
		void* LoadShaderImplementationEx(
			Stream<char> source_code,
			ECS_SHADER_TYPE shader_type,
			Stream<void>* byte_code = nullptr,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// ---------------------------------------------------------------------------------------------------------------------------

		// If it fails, it returns { nullptr, 0 }. This is the same as reading a binary file and storing it
		// such that it doesn't get loaded multiple times.
		template<bool reference_counted = false>
		ResizableStream<void>* LoadMisc(Stream<wchar_t> filename, AllocatorPolymorphic allocator = { nullptr }, ResourceManagerLoadDesc load_descriptor = {});
		
		ResizableStream<void> LoadMiscImplementation(Stream<wchar_t> filename, AllocatorPolymorphic allocator = { nullptr }, ResourceManagerLoadDesc load_descriptor = {});

		// ---------------------------------------------------------------------------------------------------------------------------
		
		// Reassigns a value to a resource that has been loaded; the resource is first destroyed then reassigned
		void RebindResource(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource);

		// Reassigns a value to a resource that has been loaded; no destruction is being done
		void RebindResourceNoDestruction(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource);

		void RemoveReferenceCountForResource(ResourceIdentifier identifier, ResourceType resource_type);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadTextFile(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadTextFile(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadTextFileImplementation(char* data, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadTexture(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadTexture(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadTextureImplementation(ResourceView texture, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadMeshes(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadMeshes(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadMeshesImplementation(Stream<Mesh>* meshes, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadCoallescedMesh(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadCoallescedMesh(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadCoallescedMeshImplementation(CoallescedMesh* mesh, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);
			
		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadMaterials(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadMaterials(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadMaterialsImplementation(Stream<PBRMaterial>* materials, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		// The material will have the textures and the buffers removed from the material
		void UnloadUserMaterial(const UserMaterial* user_material, Material* material, ResourceManagerLoadDesc load_desc = {});

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadPBRMesh(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadPBRMesh(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadPBRMeshImplementation(PBRMesh* mesh, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadVertexShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadVertexShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadPixelShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadPixelShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadHullShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadHullShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadDomainShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadDomainShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadGeometryShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadGeometryShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadComputeShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadComputeShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadShader(Stream<wchar_t> filename, ECS_SHADER_TYPE type, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadShader(unsigned int index, ECS_SHADER_TYPE type, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadShaderImplementation(void* shader, ECS_SHADER_TYPE type, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadMisc(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadMisc(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadMiscImplementation(ResizableStream<void> data, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadResource(Stream<wchar_t> filename, ResourceType type, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadResource(unsigned int index, ResourceType type, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadResourceImplementation(void* resource, ResourceType type, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// Returns true if the resource was unloaded
		template<bool reference_counted = false>
		bool TryUnloadResource(Stream<wchar_t> filename, ResourceType type, ResourceManagerLoadDesc load_desc = {});

		// ---------------------------------------------------------------------------------------------------------------------------

		// Unloads everything
		void UnloadAll();

		// Unloads all resources of a type
		void UnloadAll(ResourceType resource_type);

		// ---------------------------------------------------------------------------------------------------------------------------

		Graphics* m_graphics;
		ResourceManagerAllocator* m_memory;
		Stream<ResourceManagerTable> m_resource_types;
		ResizableStream<Stream<wchar_t>> m_shader_directory;
	};

#pragma region Free functions

	

	// --------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool PBRToMaterial(ResourceManager* resource_manager, const PBRMaterial& pbr, Stream<wchar_t> folder_to_search, Material* material);

	// --------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------

#pragma endregion

}

