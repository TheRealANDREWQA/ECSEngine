#pragma once
#include "../Core.h"
#include "../Allocators/MemoryManager.h"
#include "../Containers/Stream.h"
#include "../Containers/HashTable.h"
#include "../Containers/Stacks.h"
#include "../Containers/DataPointer.h"
#include "../Rendering/Graphics.h"
#include "../Rendering/TextureOperations.h"
#include "../Rendering/Compression/TextureCompressionTypes.h"
#include "../Multithreading/TaskManager.h"
#include "ResourceTypes.h"

#define ECS_RESOURCE_MANAGER_FLAG_DEFAULT 1

#define ECS_RESOURCE_MANAGER_TEMPORARY (1 << 16)
#define ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT (1 << 17)

// For the variant that takes the GLTFMeshes, if the scale is not 1.0f, this disables the
// rescaling of the meshes (it rescales them back to appear the same to the outside caller)
#define ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK (1 << 18)

// For the variant that takes the GLTFMeshes, if the scale is not 1.0f, this disables the
// rescaling of the meshes (it rescales them back to appear the same to the outside caller)
#define ECS_RESOURCE_MANAGER_COALESCED_MESH_EX_DO_NOT_SCALE_BACK (1 << 19)

// It will bring the origin to the center of the object such that
// the geometry will properly resemble the location
#define ECS_RESOURCE_MANAGER_COALESCED_MESH_ORIGIN_TO_CENTER (1 << 20)

// Applies a tonemap on the texture pixels before sending to the GPU (useful for thumbnails)
#define ECS_RESOURCE_MANAGER_TEXTURE_HDR_TONEMAP (1 << 17)

// Tells the loader to not try to insert into the resource table (in order to avoid 
// multithreading race conditions). This applies both for loading and unloading
#define ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS (1 << 18)

// Tells the unloader that some resources might have already been unloaded
// and to check each resource before attempting to unload it. This is activated
// only when the flag ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS is also set
#define ECS_RESOURCE_MANAGER_USER_MATERIAL_CHECK_RESOURCE (1 << 19)

// Can specify whether or not the samplers should be freed during the unloading phase
#define ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_FREE_SAMPLERS (1 << 20)


namespace ECSEngine {

	typedef MemoryManager ResourceManagerAllocator;

	// The time stamp can be used to compare against a new stamp and check to see if the resource is out of date
	struct ResourceManagerEntry {
		void* data;
		size_t time_stamp;
		unsigned short reference_count;
		unsigned short suffix_size;
		// If a resource is protected, it means that any decrement or increment reference count call on this resource will
		// Fail with a soft crash.
		bool is_protected = false;
	};

	typedef HashTableDefault<ResourceManagerEntry> ResourceManagerTable;

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

	struct GLTFData;
	struct GLTFMesh;

	struct ResourceManagerLoadDesc {
		ECS_INLINE void GPULock() {
			if (gpu_lock != nullptr) {
				gpu_lock->Lock();
			}
		}

		ECS_INLINE void GPUUnlock() {
			if (gpu_lock != nullptr) {
				gpu_lock->Unlock();
			}
		}

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
				push_lock->Lock();
			}
		}

		ECS_INLINE void Unlock() {
			if (push_lock != nullptr) {
				push_lock->Unlock();
			}
		}

		Stream<wchar_t> filename = { nullptr, 0 };
		size_t time_stamp = 0;
		SpinLock* push_lock = nullptr;

		// Can optionally specify the reference count for the resource to be inserted
		// If it is left at default, it will be non-reference counted
		unsigned int reference_count = USHORT_MAX;
	};

	struct ResourceManagerSnapshot {
		struct Resource {
			void* data;
			ResourceIdentifier identifier;
			unsigned short reference_count;
			unsigned short suffix_size;
		};

		ECS_INLINE size_t Find(ResourceIdentifier identifier, ResourceType type) const {
			return resources[(unsigned int)type].Find(identifier, [](const Resource& resource) {
				return resource.identifier;
			});
		}

		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) {
			// Everything is allocated in a single coalesced allocation
			ECSEngine::Deallocate(allocator, resources[0].buffer);
		}

		Stream<Resource> resources[(unsigned int)ResourceType::TypeCount];
	};

	struct EvictOutdatedResourcesOptions {
		AllocatorPolymorphic allocator = {};
		AdditionStream<ResourceIdentifier> removed_identifiers = {};
		AdditionStream<void*> removed_values = {};
	};

	// Defining ECS_RESOURCE_MANAGER_CHECK_RESOURCE will make AddResource check if the resource exists already 
	struct ECSENGINE_API ResourceManager
	{
		ResourceManager(
			ResourceManagerAllocator* memory,
			Graphics* graphics
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
			Stream<void> suffix = {},
			unsigned short reference_count = USHORT_MAX
		);

		// This holds a time stamp in the time stamp table.
		// Returns true when the reference counted is set to true and it is the first time it is added
		bool AddTimeStamp(
			ResourceIdentifier identifier,
			size_t time_stamp = 0,
			bool reference_counted = false,
			Stream<void> suffix = {}
		);

		void AddShaderDirectory(Stream<wchar_t> directory);

		ECS_INLINE void* Allocate(size_t size) {
			return m_memory->Allocate(size);
		}

		ECS_INLINE void* AllocateTs(size_t size) {
			return m_memory->Allocate_ts(size);
		}

		ECS_INLINE AllocatorPolymorphic Allocator() const {
			return m_memory;
		}

		ECS_INLINE AllocatorPolymorphic AllocatorTs() const {
			return AllocatorPolymorphic(m_memory).AsMulti();
		}

		// Returns the old time stamp
		size_t ChangeTimeStamp(ResourceIdentifier identifier, ResourceType resource_type, size_t new_time_stamp, Stream<void> suffix = {});

		ECS_INLINE void Deallocate(const void* data) {
			m_memory->Deallocate(data);
		}

		ECS_INLINE void DeallocateTs(const void* data) {
			m_memory->Deallocate_ts(data);
		}

		// Decrements the reference count all resources of that type
		template<bool delete_if_zero = true>
		void DecrementReferenceCounts(ResourceType type, unsigned short amount);

		// Returns true if the resource has reached a reference count of 0.
		bool DecrementReferenceCount(ResourceType type, unsigned int resource_index, unsigned short amount);

		unsigned int FindResourceFromPointer(void* resource, ResourceType type) const;

		// Checks to see if the resource exists
		bool Exists(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = {}) const;

		// Checks to see if the resource exists and returns its position as index inside the hash table
		bool Exists(ResourceIdentifier identifier, ResourceType type, unsigned int& table_index, Stream<void> suffix = {}) const;

		// Removes a resource from the table - it does not unload it. (it just removes it from the table)
		void EvictResource(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = {});
		
		// Removes a resource from the table - it does not unload it. (it just removes it from the table)
		void EvictResource(unsigned int index, ResourceType type);

		// Removes all resources from the tables without unloading them that appear in the other resource manager
		void EvictResourcesFrom(const ResourceManager* other);

		// Walks through the table and removes all resources which are outdated - implies that all resource identifiers have as their
		// ptr the path to the resource
		void EvictOutdatedResources(ResourceType type, EvictOutdatedResourcesOptions* options = {});

		// The functor will receive as parameter the ResourceIdentifier of the resource. If the resource was added
		// with a suffix, the identifier includes the suffix. For early exit it must return true to exit
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachResourceIdentifier(ResourceType resource_type, Functor&& functor) const {
			return m_resource_types[(unsigned int)resource_type].ForEachConst<early_exit>([&](ResourceManagerEntry entry, ResourceIdentifier identifier) {
				if constexpr (early_exit) {
					return functor(identifier);
				}
				else {
					functor(identifier);
				}
			});
		}

		// The functor will receive as parameter the ResourceIdentifier of the resource without the suffix and a second unsigned short
		// describing the suffix size. For early exit it must return true to exit. Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachResourceIdentifierNoSuffix(ResourceType resource_type, Functor&& functor) const {
			return m_resource_types[(unsigned int)resource_type].ForEachConst<early_exit>([&](ResourceManagerEntry entry, ResourceIdentifier identifier) {
				identifier.size -= entry.suffix_size;
				if constexpr (early_exit) {
					return functor(identifier, entry.suffix_size);
				}
				else {
					functor(identifier, entry.suffix_size);
				}
			});
		}

		// The functor will receive as parameter a void* with the resource. For early exit it must return true to exit.
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachResource(ResourceType resource_type, Functor&& functor) const {
			return m_resource_types[(unsigned int)resource_type].ForEachConst<early_exit>([&](ResourceManagerEntry entry, ResourceIdentifier identifier) {
				if constexpr (early_exit) {
					return functor(entry.data);
				}
				else {
					functor(entry.data);
				}
			});
		}

		template<bool early_exit = false, typename Functor>
		bool ForEachResourceIndex(ResourceType resource_type, Functor&& functor) const {
			return m_resource_types[(unsigned int)resource_type].ForEachIndexConst<early_exit>([&](unsigned int resource_index) {
				if constexpr (early_exit) {
					return functor(resource_index);
				}
				else {
					functor(resource_index);
				}
			});
		}

		// It returns -1 if the resource doesn't exist
		unsigned int GetResourceIndex(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = {}) const;

		// It returns nullptr if the resource doesn't exist
		void* GetResource(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = {});

		// It returns nullptr if the resource doesn't exist
		const void* GetResource(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = {}) const;

		void* GetResourceFromIndex(unsigned int index, ResourceType type) const;

		ResourceIdentifier GetResourceIdentifierFromIndex(unsigned int index, ResourceType type) const;

		// Returns -1 if the identifier doesn't exist
		size_t GetTimeStamp(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = {}) const;

		// Returns an entry with the data pointer set to nullptr and the time stamp to -1
		ResourceManagerEntry GetEntry(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = {}) const;

		// Returns nullptr if the identifier doesn't exist
		ResourceManagerEntry* GetEntryPtr(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix = {});

		// Returns the current state of the resources inside - including identifiers and suffix sizes
		ResourceManagerSnapshot GetSnapshot(AllocatorPolymorphic allocator) const;
		
		unsigned int GetReferenceCount(ResourceType type, unsigned int resource_index) const;

		// Returns the number of currently loaded resource for the given type
		unsigned int GetResourceCount(ResourceType type) const;

		// Returns true whether or not the given stamp is greater than the stamp currently registered. If the entry doesn't exist,
		// it returns false. The suffix is optional (can either bake it into the identifier or give it to the function to do it)
		bool IsResourceOutdated(ResourceIdentifier identifier, ResourceType type, size_t new_stamp, Stream<void> suffix = {});

		void IncrementReferenceCount(ResourceType type, unsigned int resource_index, unsigned short amount);

		// Increases the referece count for all resources of that type
		void IncrementReferenceCounts(ResourceType type, unsigned short amount);

		// It will insert the resources already loaded from the other resource manager into the instance's
		// tables. The timestamps will be 0, and will have no reference count. If the graphics object
		// differs between the 2 resource managers, then it will create another shared instance for the
		// GPU resources which require this
		void InheritResources(const ResourceManager* other);

		// The file is allocated from the current allocator of the resource manager
		template<bool reference_counted = false>
		Stream<char>* LoadTextFile(
			Stream<wchar_t> filename,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// The file is allocated from the current allocator of the resource manager
		Stream<char> LoadTextFileImplementation(Stream<wchar_t> file);

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

		// Loads all meshes from a gltf file and creates a coalesced mesh
		template<bool reference_counted = false>
		CoalescedMesh* LoadCoalescedMesh(
			Stream<wchar_t> filename,
			float scale_factor = 1.0f,
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// Loads all meshes from a gltf file and creates a coalesced mesh
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		CoalescedMesh* LoadCoalescedMeshImplementation(Stream<wchar_t> filename, float scale_factor = 1.0f, ResourceManagerLoadDesc load_descriptor = {});

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr. It allocates memory from the allocator
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT, ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK.
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		CoalescedMesh* LoadCoalescedMeshImplementationEx(
			const GLTFData* data,
			float scale_factor = 1.0f,
			ResourceManagerLoadDesc load_descriptor = {},
			ResourceManagerExDesc* ex_desc = {}
		);

		// A more detailed version that directly takes the data and does not deallocate it
		// If it fails it returns nullptr. It allocates memory from the allocator
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT, ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK.
		// It will also insert it into the table if the filename is not { nullptr, 0 }
		CoalescedMesh* LoadCoalescedMeshImplementationEx(
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
		CoalescedMesh* LoadCoalescedMeshImplementationEx(
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
		// it returns false and rolls back any loads that have been done.
		// Flags: ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS
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

		// ---------------------------------------------------------------------------------------------------------------------------

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary using shader compile options and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// Returns a pointer to the shader type (for vertex shader a VertexShader*)
		template<bool reference_counted = false>
		ShaderInterface LoadShader(
			Stream<wchar_t> filename,
			ECS_SHADER_TYPE shader_type,
			Stream<char>* shader_source_code = nullptr,
			Stream<void>* byte_code = nullptr,
			ShaderCompileOptions options = {},
			ResourceManagerLoadDesc load_descriptor = {}
		);

		// If a .cso is specified, it will be loaded into memory and simply passed to D3D
		// If a .hlsl is specified, it will be compiled into binary using shader compile options and then passed to D3D
		// An optional pointer to an Stream<char> can be specified such that for .hlsl the calling code receives
		// The shader byte code for further use i.e. reflect the textures, reflect the vertex input layout
		// Returns a pointer to the shader type (for vertex shader a VertexShader*)
		ShaderInterface LoadShaderImplementation(
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
		ShaderInterface LoadShaderImplementationEx(
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

		void ProtectResource(ResourceIdentifier identifier, ResourceType resource_type, Stream<void> suffix = {});

		void UnprotectResource(ResourceIdentifier identifier, ResourceType resource_type, Stream<void> suffix = {});

		// ---------------------------------------------------------------------------------------------------------------------------
		
		// Brings back the state of the resource manager to the recorded snapshot. Can optionally give a string to be built with the
		// mismatches that are between the two states. Returns true if there are no mismatches, else false
		// It doesn't deallocate the snapshot (it must be done outside)!
		bool RestoreSnapshot(const ResourceManagerSnapshot& snapshot, CapacityStream<char>* mismatch_string = nullptr);

		// Reassigns a value to a resource that has been loaded; the resource is first destroyed then reassigned
		void RebindResource(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource, Stream<void> suffix = {});

		// Reassigns a value to a resource that has been loaded; no destruction is being done
		void RebindResourceNoDestruction(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource, Stream<void> suffix = {});

		void RemoveReferenceCountForResource(ResourceIdentifier identifier, ResourceType resource_type, Stream<void> suffix = {});

		// Returns true when reference counted is set to true and the time stamp was removed
		bool RemoveTimeStamp(ResourceIdentifier identifier, bool reference_count = false, Stream<void> suffix = {});

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
		void UnloadCoalescedMesh(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadCoalescedMesh(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadCoalescedMeshImplementation(CoalescedMesh* mesh, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);
			
		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadMaterials(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadMaterials(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadMaterialsImplementation(Stream<PBRMaterial>* materials, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		// The material will have the textures and the buffers removed from the material
		// Flags: ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS, ECS_RESOURCE_MANAGER_USER_MATERIAL_CHECK_RESOURCE
		// (the latter in conjunction with the first flag)
		void UnloadUserMaterial(const UserMaterial* user_material, Material* material, ResourceManagerLoadDesc load_desc = {});

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadPBRMesh(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadPBRMesh(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadPBRMeshImplementation(PBRMesh* mesh, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc = {});

		template<bool reference_counted = false>
		void UnloadShader(unsigned int index, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

		void UnloadShaderImplementation(void* shader, size_t flags = ECS_RESOURCE_MANAGER_FLAG_DEFAULT);

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

	struct ReloadUserMaterialOptions {
		bool reload_vertex_shader = false;
		bool reload_pixel_shader = false;
		bool reload_samplers = false;
		bool reload_textures = false;
		bool reload_buffers = false;
	};

	// Material should be the data that corresponds to the old_user_material. The new values will be filled in inside
	// this material pointer. It returns true if it succeeded, else false. When it fails it will maintain a consistent
	// state (a state where all the old_user_material resources were deallocated and no new resources are loaded) - the
	// same state as calling deallocate on the old material. If it fails simply discard the contents in the material
	// Flags:
	// ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS
	// ECS_RESOURCE_MANAGER_USER_MATERIAL_CHECK_RESOURCE
	ECSENGINE_API bool ReloadUserMaterial(
		ResourceManager* resource_manager, 
		const UserMaterial* old_user_material,
		const UserMaterial* new_user_material,
		Material* material, 
		ReloadUserMaterialOptions options,
		ResourceManagerLoadDesc load_descriptor = {}
	);

	// --------------------------------------------------------------------------------------------------------------------

#pragma endregion

}

