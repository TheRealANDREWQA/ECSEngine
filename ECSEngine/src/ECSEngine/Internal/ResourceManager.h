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

constexpr size_t ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS = 128;
constexpr size_t ECS_RESOURCE_MANAGER_DEFAULT_RESOURCE_COUNT = 256;
constexpr size_t ECS_RESOURCE_MANAGER_TEMPORARY_ELEMENTS = 8;
constexpr size_t ECS_RESOURCE_MANAGER_MAX_FOLDER_COUNT = 6;

namespace ECSEngine {

	ECS_CONTAINERS;

	using ResourceManagerHash = HashFunctionAdditiveString;
	using ResourceManagerAllocator = MemoryManager;
	// Embed the reference count inside the pointer
	using ResourceManagerTable = IdentifierHashTable<DataPointer, ResourceIdentifier, HashFunctionPowerOfTwo>;

	enum class ResourceTypes : unsigned char {
		Texture,
		TextureResource,
		TextFile,
		TypeCount
	};

	constexpr const char* ECS_RESOURCE_TYPE_NAMES[] = { "Texture", "TextureResource", "TextFile" };
	static_assert(std::size(ECS_RESOURCE_TYPE_NAMES) == (unsigned int)ResourceTypes::TypeCount);
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
		struct ResourceType {
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

		template<bool delete_if_zero = true>
		void DecrementReferenceCount(ResourceTypes type, unsigned int amount);

		// Pops it off the stack
		void DeleteResourcePath(unsigned int thread_index = 0);

		// Checks to see if the resource exists
		bool Exists(ResourceIdentifier identifier, ResourceTypes type) const;

		// Checks to see if the resource exists and returns its position as index inside the hash table
		bool Exists(ResourceIdentifier identifier, ResourceTypes type, unsigned int& table_index) const;

		// It will trigger an assert if the resource was not found
		int GetResourceIndex(ResourceIdentifier identifier, ResourceTypes type) const;

		// It will trigger an assert if the resource was not found
		void* GetResource(ResourceIdentifier identifier, ResourceTypes type);

		// It will create all resource types with default resource count
		void InitializeDefaultTypes();

		void IncrementReferenceCount(ResourceTypes type, unsigned int amount);

		// resource folder path different from -1 will use the folder in the specified thread position
		template<bool reference_counted = false>
		char* LoadTextFile(
			const char* filename, 
			size_t& size, 
			size_t load_flags = 1,
			unsigned int resource_folder_path_index = -1
		);

		// resource folder path different from -1 will use the folder in the specified thread position
		template<bool reference_counted = false>
		char* LoadTextFile(
			const wchar_t* filename,
			size_t& size,
			size_t load_flags = 1,
			unsigned int resource_folder_path_index = -1
		);

		char* LoadTextFileTemporary(const char* filename, size_t& size, unsigned int& handle, unsigned int thread_index = 0, bool use_path = false);

		char* LoadTextFileTemporary(const wchar_t* filename, size_t& size, unsigned int& handle, unsigned int thread_index = 0, bool use_path = false);

		char* LoadTextFileImplementation(std::ifstream& input, size_t* size);

		void OpenFile(const char* filename, std::ifstream& input, unsigned int resource_folder_path_index = -1);

		void OpenFile(const wchar_t* filename, std::ifstream& input, unsigned int resource_folder_path_index = -1);

		// In order to generate mip-maps, the context must be supplied
		template<bool reference_counted = false>
		ID3D11ShaderResourceView* LoadTexture(
			const wchar_t* filename,
			ResourceManagerTextureDesc& descriptor,
			size_t load_flags = 1,
			unsigned int resource_folder_path_index = -1
		);

		ID3D11ShaderResourceView* LoadTextureTemporary(
			const wchar_t* filename,
			unsigned int& handle,
			ResourceManagerTextureDesc& descriptor,
			unsigned int thread_index = 0,
			bool use_path = false
		);

		ID3D11ShaderResourceView* LoadTextureImplementation(
			const wchar_t* filename, 
			ResourceManagerTextureDesc* descriptor, 
			unsigned int resource_folder_path_index = -1
		);

		template<bool reference_counted = false>
		ID3D11Resource* LoadTextureResource(
			const wchar_t* filename,
			ResourceManagerTextureDesc& descriptor,
			size_t load_flags = 1,
			unsigned int resource_folder_path_index = -1
		);

		ID3D11Resource* LoadTextureResourceTemporary(
			const wchar_t* filename,
			unsigned int& handle,
			ResourceManagerTextureDesc& descriptor,
			unsigned int thread_index = 0,
			bool use_path = false
		);

		ID3D11Resource* LoadTextureResourceImplementation(
			const wchar_t* filename,
			ResourceManagerTextureDesc* descriptor,
			unsigned int resource_folder_path_index = -1
		);

		template<bool reference_counted = false>
		void UnloadTextFile(const char* filename, size_t flags = 1);

		template<bool reference_counted = false>
		void UnloadTextFile(const wchar_t* filename, size_t flags = 1);

		template<bool reference_counted = false>
		void UnloadTextFile(unsigned int index, size_t flags = 1);

		void UnloadTextFileTemporary(unsigned int handle, unsigned int thread_index = 0);

		template<bool reference_counted = false>
		void UnloadTexture(const wchar_t* filename, size_t flags = 1);

		template<bool reference_counted = false>
		void UnloadTexture(unsigned int index, size_t flags = 1);

		void UnloadTextureTemporary(unsigned int handle, unsigned int thread_index = 0);

		template<bool reference_counted = false>
		void UnloadTextureResource(const wchar_t* filename, size_t flags = 1);

		template<bool reference_counted = false>
		void UnloadTextureResource(unsigned int index, size_t flags = 1);

		void UnloadTextureResourceTemporary(unsigned int handle, unsigned int thread_index = 0);

	//private:
		Graphics* m_graphics;
		ResourceManagerAllocator* m_memory;
		ResizableStream<ResourceType, ResourceManagerAllocator> m_resource_types;
		CapacityStream<ResourceFolder> m_resource_folder_path;
		CapacityStream<StableReferenceStream<void*>> m_temporary_resources;
	};

}

