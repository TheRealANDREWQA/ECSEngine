#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Allocators/MemoryManager.h"
#include "../Containers/Stream.h"
#include "../Containers/HashTable.h"
#include "../Containers/Stacks.h"
#include "InternalStructures.h"
#include "../Utilities/Function.h"
#include "../Utilities/Assert.h"
#include "../Containers/DataPointer.h"
#include "../../../Dependencies/DirectXTK/Inc/WICTextureLoader.h"
#include "../Rendering/Graphics.h"
#include "../Rendering/Compression/TextureCompressionTypes.h"
#include "../Internal/Multithreading/TaskManager.h"

constexpr size_t ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS = 128;
constexpr size_t ECS_RESOURCE_MANAGER_DEFAULT_RESOURCE_COUNT = 256;
constexpr size_t ECS_RESOURCE_MANAGER_TEMPORARY_ELEMENTS = 8;
constexpr size_t ECS_RESOURCE_MANAGER_MAX_FOLDER_COUNT = 6;

constexpr size_t ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT = (size_t)1 << 16;

namespace ECSEngine {

	ECS_CONTAINERS;

	using ResourceManagerHash = HashFunctionMultiplyString;
	using ResourceManagerAllocator = MemoryManager;
	// Embed the reference count inside the pointer
	using ResourceManagerTable = IdentifierHashTable<DataPointer, ResourceIdentifier, HashFunctionPowerOfTwo>;

	enum class ResourceType : unsigned char {
		Texture,
		TextFile,
		Mesh,
		CoallescedMesh,
		Material,
		PBRMesh,
		TypeCount
	};

	constexpr const char* ECS_RESOURCE_TYPE_NAMES[] = { "Texture", "TextFile", "Mesh", "CoallescedMesh", "Material", "PBRMesh" };
	static_assert(std::size(ECS_RESOURCE_TYPE_NAMES) == (unsigned int)ResourceType::TypeCount);
	constexpr size_t ECS_RESOURCE_INCREMENT_COUNT = USHORT_MAX;

	struct ECSENGINE_API ResourceManagerTextureDesc {
		GraphicsContext* context = nullptr;
		D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE;
		unsigned int bindFlags = D3D11_BIND_SHADER_RESOURCE;
		unsigned int cpuAccessFlags = 0;
		unsigned int miscFlags = 0;
		size_t max_size = 0;
		DirectX::WIC_LOADER_FLAGS loader_flag = DirectX::WIC_LOADER_DEFAULT;
		TextureCompressionExplicit compression = (TextureCompressionExplicit)-1;
	};

	// Defining ECS_RESOURCE_MANAGER_CHECK_RESOURCE will make AddResource check if the resource exists already 
	struct ECSENGINE_API ResourceManager
	{
		struct InternalResourceType {
			const char* name;
			ResourceManagerTable table;
		};
		
		struct ResourceFolder {
			CapacityStream<char> characters;
			wchar_t* wide_characters;
			Stack<unsigned int> folder_offsets;
		};

	//public:
		ResourceManager(
			ResourceManagerAllocator* memory,
			Graphics* graphics,
			unsigned int thread_count,
			const char* resource_folder_path
		);

		// Each thread has its own resource path
		void AddResourcePath(const char* subpath, unsigned int thread_index = 0);

		// Each thread has its own resource path
		void AddResourcePath(const wchar_t* subpath, unsigned int thread_index = 0);

		// Create a new resource type
		void AddResourceType(const char* type_name, unsigned int resource_count = ECS_RESOURCE_MANAGER_DEFAULT_RESOURCE_COUNT);

		void* Allocate(size_t size);

		AllocatorPolymorphic Allocator();

		void Deallocate(const void* data);

		template<bool delete_if_zero = true>
		void DecrementReferenceCount(ResourceType type, unsigned int amount);

		// Pops it off the stack
		void DeleteResourcePath(unsigned int thread_index = 0);

		// Checks to see if the resource exists
		bool Exists(ResourceIdentifier identifier, ResourceType type) const;

		// Checks to see if the resource exists and returns its position as index inside the hash table
		bool Exists(ResourceIdentifier identifier, ResourceType type, unsigned int& table_index) const;

		// It will trigger an assert if the resource was not found
		int GetResourceIndex(ResourceIdentifier identifier, ResourceType type) const;

		// It will trigger an assert if the resource was not found
		void* GetResource(ResourceIdentifier identifier, ResourceType type);

		// It will create all resource types with default resource count
		void InitializeDefaultTypes();

		void IncrementReferenceCount(ResourceType type, unsigned int amount);

		// resource folder path different from -1 will use the folder in the specified thread position
		template<bool reference_counted = false>
		char* LoadTextFile(
			const char* filename, 
			size_t& size, 
			size_t load_flags = 1,
			bool* reference_counted_is_loaded = nullptr,
			unsigned int resource_folder_path_index = -1
		);

		// resource folder path different from -1 will use the folder in the specified thread position
		template<bool reference_counted = false>
		char* LoadTextFile(
			const wchar_t* filename,
			size_t& size,
			size_t load_flags = 1,
			bool* reference_counted_is_loaded = nullptr,
			unsigned int resource_folder_path_index = -1
		);

		char* LoadTextFileImplementation(std::ifstream& input, size_t* size);

		void OpenFile(const char* filename, std::ifstream& input, unsigned int resource_folder_path_index = -1);

		void OpenFile(const wchar_t* filename, std::ifstream& input, unsigned int resource_folder_path_index = -1);

		// In order to generate mip-maps, the context must be supplied
		template<bool reference_counted = false>
		ResourceView LoadTexture(
			const wchar_t* filename,
			ResourceManagerTextureDesc& descriptor,
			size_t load_flags = 1,
			bool* reference_counted_is_loaded = nullptr,
			unsigned int resource_folder_path_index = -1
		);

		// In order to generate mip-maps, the context must be supplied
		ResourceView LoadTextureImplementation(
			const wchar_t* filename, 
			ResourceManagerTextureDesc* descriptor, 
			unsigned int resource_folder_path_index = -1
		);

		// Loads all meshes from a gltf file
		template<bool reference_counted = false>
		Stream<Mesh>* LoadMeshes(
			const wchar_t* filename,
			size_t load_flags = 1,
			bool* reference_counted_is_loaded = nullptr,
			unsigned int resource_folder_path_index = -1
		);

		// Loads all meshes from a gltf file
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		Stream<Mesh>* LoadMeshImplementation(const wchar_t* filename, size_t load_flags, unsigned int resource_folder_path_index = -1);

		// Loads all meshes from a gltf file and creates a coallesced mesh
		template<bool reference_counted = false>
		CoallescedMesh* LoadCoallescedMesh(
			const wchar_t* filename,
			size_t load_flags = 1,
			bool* reference_counted_is_loaded = nullptr,
			unsigned int resource_folder_path_index = -1
		);

		// Loads all meshes from a gltf file and creates a coallesced mesh
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		CoallescedMesh* LoadCoallescedMeshImplementation(const wchar_t* filename, size_t load_flags, unsigned int resource_folder_path_index = -1);

		// Loads all materials from a gltf file
		template<bool reference_counted = false>
		containers::Stream<PBRMaterial>* LoadMaterials(
			const wchar_t* filename,
			size_t load_flags = 1,
			bool* reference_counted_is_loaded = nullptr,
			unsigned int resource_folder_path_index = -1
		);

		// Loads all materials from a gltf file
		containers::Stream<PBRMaterial>* LoadMaterialsImplementation(const wchar_t* filename, size_t load_flags, unsigned int resource_folder_path_index = -1);

		// Loads all meshes and materials from a gltf file, combines the meshes into a single one sorted by material submeshes
		template<bool reference_counted = false>
		PBRMesh* LoadPBRMesh(
			const wchar_t* filename,
			size_t load_flags = 1,
			bool* reference_counted_is_loaded = nullptr,
			unsigned int resource_folder_path_index = -1
		);

		// Loads all meshes and materials from a gltf file, combines the meshes into a single one sorted by material submeshes
		// Flags: ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT
		PBRMesh* LoadPBRMeshImplementation(const wchar_t* filename, size_t load_flags, unsigned int resource_folder_path_index = -1);

		// Reassigns a value to a resource that has been loaded; the resource is first destroyed than reassigned
		void RebindResource(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource);

		// Reassigns a value to a resource that has been loaded; no destruction is being done
		void RebindResourceNoDestruction(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource);

		void RemoveReferenceCountForResource(ResourceIdentifier identifier, ResourceType resource_type);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadTextFile(const char* filename, size_t flags = 1);

		template<bool reference_counted = false>
		void UnloadTextFile(const wchar_t* filename, size_t flags = 1);

		template<bool reference_counted = false>
		void UnloadTextFile(unsigned int index, size_t flags = 1);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadTexture(const wchar_t* filename, size_t flags = 1);

		template<bool reference_counted = false>
		void UnloadTexture(unsigned int index, size_t flags = 1);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadMeshes(const wchar_t* filename, size_t flags = 1);

		template<bool reference_counted = false>
		void UnloadMeshes(unsigned int index, size_t flags = 1);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadCoallescedMesh(const wchar_t* filename, size_t flags = 1);

		template<bool reference_counted = false>
		void UnloadCoallescedMesh(unsigned int index, size_t flags = 1);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadMaterials(const wchar_t* filename, size_t flags = 1);

		template<bool reference_counted = false>
		void UnloadMaterials(unsigned int index, size_t flags = 1);

		// ---------------------------------------------------------------------------------------------------------------------------

		template<bool reference_counted = false>
		void UnloadPBRMesh(const wchar_t* filename, size_t flags = 1);

		template<bool reference_counted = false>
		void UnloadPBRMesh(unsigned int index, size_t flags = 1);

		// ---------------------------------------------------------------------------------------------------------------------------

	//private:
		Graphics* m_graphics;
		ResourceManagerAllocator* m_memory;
		ResizableStream<InternalResourceType, ResourceManagerAllocator> m_resource_types;
		CapacityStream<ResourceFolder> m_resource_folder_path;
	};

#pragma region Free functions

	// --------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Material PBRToMaterial(ResourceManager* resource_manager, const PBRMaterial& pbr);

	// --------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------

#pragma endregion

}

