#include "ecspch.h"
#include "ResourceManager.h"
#include "../Rendering/GraphicsHelpers.h"
#include "../Rendering/Compression/TextureCompression.h"
#include "../Utilities/FunctionInterfaces.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../GLTFLoader/GLTFLoader.h"
#include "../Utilities/Path.h"

#define ECS_RESOURCE_MANAGER_CHECK_RESOURCE

constexpr size_t ECS_RESOURCE_MANAGER_DEFAULT_RESOURCE_COUNT = 256;
constexpr size_t ECS_RESOURCE_MANAGER_MAX_THREAD_PATH = 8;
constexpr size_t ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS = 128;

namespace ECSEngine {

	ECS_CONTAINERS;

#define ECS_RESOURCE_MANAGER_INITIAL_LOAD_INCREMENT_COUNT 100

	// The handler must implement a void* parameter for additional info and must return void*
	// If it returns a nullptr, it means the load failed so do not add it to the table
	template<bool reference_counted, typename Handler>
	void* AddResource(
		ResourceManager* resource_manager,
		ResourceIdentifier identifier,
		ResourceType type,
		void* handler_parameter,
		size_t flags,
		bool* reference_counted_is_loaded,
		Handler&& handler
	) {
		unsigned int type_int = (unsigned int)type;

		if constexpr (!reference_counted) {
#ifdef ECS_RESOURCE_MANAGER_CHECK_RESOURCE
			bool exists = resource_manager->Exists(identifier, type);
			ECS_ASSERT(!exists, "The resource already exists!");
#endif
			void* data = handler(handler_parameter);

			if (data != nullptr) {
				void* allocation = function::Copy(resource_manager->m_memory, identifier.ptr, identifier.size);
				identifier.ptr = allocation;
				unsigned int hash_index = ResourceManagerHash::Hash(identifier);

				DataPointer data_pointer(data);
				data_pointer.SetData(USHORT_MAX);

				bool is_table_full = resource_manager->m_resource_types[type_int].table.Insert(hash_index, data_pointer, identifier);
				ECS_ASSERT(!is_table_full, "Table is too full or too many collisions!");
			}
			return data;
		}
		else {
			unsigned int table_index;
			bool exists = resource_manager->Exists(identifier, type, table_index);

			unsigned short increment_count = flags & ECS_RESOURCE_INCREMENT_COUNT;
			if (!exists) {
				void* data = handler(handler_parameter);

				if (data != nullptr) {
					void* allocation = function::Copy(resource_manager->m_memory, identifier.ptr, identifier.size);
					identifier.ptr = allocation;
					unsigned int hash_index = ResourceManagerHash::Hash(identifier);

					DataPointer data_pointer(data);
					data_pointer.SetData(ECS_RESOURCE_MANAGER_INITIAL_LOAD_INCREMENT_COUNT);

					bool is_table_full = resource_manager->m_resource_types[type_int].table.Insert(hash_index, data_pointer, identifier);
					ECS_ASSERT(!is_table_full, "Table is too full or too many collisions!");

					if (reference_counted_is_loaded != nullptr) {
						*reference_counted_is_loaded = true;
					}
				}
				return data;
			}

			DataPointer* data_pointer = resource_manager->m_resource_types[type_int].table.GetValuePtrFromIndex(table_index);
			unsigned short count = data_pointer->IncrementData(increment_count);
			if (count == USHORT_MAX) {
				data_pointer->SetData(USHORT_MAX - 1);
			}

			if (reference_counted_is_loaded != nullptr) {
				*reference_counted_is_loaded = false;
			}
			return data_pointer->GetPointer();
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// The handler must implement a void* parameter for additional info and a void* for its allocation and must return void*
	template<bool reference_counted, typename Handler>
	void* AddResource(
		ResourceManager* resource_manager,
		ResourceIdentifier identifier,
		ResourceType type,
		void* handler_parameter,
		size_t parameter_size,
		size_t flags,
		bool* reference_counted_is_loaded,
		Handler&& handler
	) {
		unsigned int type_int = (unsigned int)type;

		if constexpr (!reference_counted) {
#ifdef ECS_RESOURCE_MANAGER_CHECK_RESOURCE
			bool exists = resource_manager->Exists(identifier, type);
			ECS_ASSERT(!exists, "Resource already exists!");
#endif

			// plus 7 bytes for padding
			void* allocation = resource_manager->Allocate(parameter_size + identifier.size + 7);
			ECS_ASSERT(allocation != nullptr, "Allocating memory for a resource failed");
			memcpy(allocation, identifier.ptr, identifier.size);
			identifier.ptr = allocation;

			allocation = (void*)function::align_pointer((uintptr_t)allocation + identifier.size, 8);

			memset(allocation, 0, parameter_size);
			void* data = handler(handler_parameter, allocation);

			if (data != nullptr) {
				DataPointer data_pointer(data);
				data_pointer.SetData(USHORT_MAX);

				unsigned int hash_index = ResourceManagerHash::Hash(identifier);
				bool is_table_full = resource_manager->m_resource_types[type_int].table.Insert(hash_index, data_pointer, identifier);
				ECS_ASSERT(!is_table_full, "Table is too full or too many collisions!");
			}
			// The load failed, release the allocation
			else {
				resource_manager->Deallocate(allocation);
			}
			return data;
		}
		else {
			unsigned int table_index;
			bool exists = resource_manager->Exists(identifier, type, table_index);

			unsigned short increment_count = flags & ECS_RESOURCE_INCREMENT_COUNT;
			if (!exists) {
				// plus 7 bytes for padding
				void* allocation = resource_manager->Allocate(parameter_size + identifier.size + 7);
				ECS_ASSERT(allocation != nullptr, "Allocating memory for a resource failed");
				memcpy(allocation, identifier.ptr, identifier.size);
				identifier.ptr = allocation;

				allocation = (void*)function::align_pointer((uintptr_t)allocation + identifier.size, 8);

				memset(allocation, 0, parameter_size);
				void* data = handler(handler_parameter, allocation);

				if (data != nullptr) {
					DataPointer data_pointer(data);
					data_pointer.SetData(ECS_RESOURCE_MANAGER_INITIAL_LOAD_INCREMENT_COUNT);

					unsigned int hash_index = ResourceManagerHash::Hash(identifier);

					bool is_table_full = resource_manager->m_resource_types[type_int].table.Insert(hash_index, data_pointer, identifier);
					ECS_ASSERT(!is_table_full, "Table is too full or too many collisions!");

					if (reference_counted_is_loaded != nullptr) {
						*reference_counted_is_loaded = true;
					}
				}
				// The load failed, release the allocation
				else {
					resource_manager->Deallocate(allocation);
				}
				return data;
			}

			DataPointer* data_pointer = resource_manager->m_resource_types[type_int].table.GetValuePtrFromIndex(table_index);
			unsigned short count = data_pointer->IncrementData(increment_count);
			if (count == USHORT_MAX) {
				data_pointer->SetData(USHORT_MAX - 1);
			}

			if (reference_counted_is_loaded != nullptr) {
				*reference_counted_is_loaded = false;
			}
			return data_pointer->GetPointer();
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted, typename Handler>
	void DeleteResource(ResourceManager* resource_manager, unsigned int index, ResourceType type, size_t flags, Handler&& handler) {
		unsigned int type_int = (unsigned int)type;

		DataPointer data_pointer = resource_manager->m_resource_types[type_int].table.GetValueFromIndex(index);

		auto delete_resource = [=]() {
			const ResourceIdentifier* identifiers = resource_manager->m_resource_types[type_int].table.GetIdentifiers();

			void* data = data_pointer.GetPointer();
			handler(data, resource_manager);
			resource_manager->Deallocate(identifiers[index].ptr);
			resource_manager->m_resource_types[type_int].table.EraseFromIndex(index);
		};
		if constexpr (!reference_counted) {
			delete_resource();
		}
		else {
			unsigned short increment_count = flags & ECS_RESOURCE_INCREMENT_COUNT;
			unsigned short count = data_pointer.DecrementData(increment_count);
			if (count == 0) {
				delete_resource();
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// The handler must implement a void* parameter as this is how it receives the resource
	// and the ResourceManager* pointer
	template<bool reference_counted, typename Handler>
	void DeleteResource(ResourceManager* resource_manager, ResourceIdentifier identifier, ResourceType type, size_t flags, Handler&& handler) {
		unsigned int type_int = (unsigned int)type;

		unsigned int hash_index = ResourceManagerHash::Hash(identifier);
		int hashed_position = resource_manager->m_resource_types[type_int].table.Find<true>(hash_index, identifier);
		ECS_ASSERT(hashed_position != -1, "Trying to delete a resource that has not yet been loaded!");

		DeleteResource<reference_counted>(resource_manager, hashed_position, type, flags, handler);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	using DeleteFunction = void (*)(ResourceManager*, unsigned int, size_t);
	// Return true means deallocate the data given, return false do not deallocate it
	using UnloadFunction = void (*)(void*, ResourceManager*);

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadTextureHandler(void* parameter, ResourceManager* resource_manager) {
		ID3D11ShaderResourceView* reinterpretation = (ID3D11ShaderResourceView*)parameter;
		ReleaseShaderView(reinterpretation);
	}

	void DeleteTexture(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadTexture<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadTextFileHandler(void* parameter, ResourceManager* resource_manager) {}

	void DeleteTextFile(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadTextFile<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadMeshesHandler(void* parameter, ResourceManager* resource_manager) {
		Stream<Mesh>* data = (Stream<Mesh>*)parameter;
		for (size_t index = 0; index < data->size; index++) {
			resource_manager->m_graphics->FreeMesh(data->buffer[index]);
		}
		resource_manager->Deallocate(data);
	}

	void DeleteMeshes(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadMeshes<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadMaterialsHandler(void* parameter, ResourceManager* resource_manager) {
		Stream<PBRMaterial>* data = (Stream<PBRMaterial>*)parameter;
		AllocatorPolymorphic allocator = resource_manager->Allocator();

		for (size_t index = 0; index < data->size; index++) {
			FreePBRMaterial(data->buffer[index], allocator);
		}
		resource_manager->Deallocate(data);
	}

	void DeleteMaterials(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadMaterials(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadPBRMeshHandler(void* parameter, ResourceManager* resource_manager) {
		PBRMesh* data = (PBRMesh*)parameter;
		AllocatorPolymorphic allocator = resource_manager->Allocator();

		// The mesh must be released, alongside the pbr materials
		for (size_t index = 0; index < data->mesh.submeshes.size; index++) {
			FreePBRMaterial(data->materials[index], allocator);
		}
		resource_manager->m_graphics->FreeMesh(data->mesh.mesh);
		resource_manager->Deallocate(data);
	}

	void DeletePBRMesh(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadPBRMesh<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadCoallescedMeshHandler(void* parameter, ResourceManager* resource_manager) {
	
	}

	void DeleteCoallescedMesh(ResourceManager* manager, unsigned int index, size_t flags) {
	
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	constexpr DeleteFunction DELETE_FUNCTIONS[] = {
		DeleteTexture, 
		DeleteTextFile,
		DeleteMeshes,
		DeleteCoallescedMesh,
		DeleteMaterials,
		DeletePBRMesh
	};
	
	constexpr UnloadFunction UNLOAD_FUNCTIONS[] = {
		UnloadTextureHandler,
		UnloadTextFileHandler,
		UnloadMeshesHandler,
		UnloadCoallescedMeshHandler,
		UnloadMaterialsHandler,
		UnloadPBRMeshHandler
	};

	static_assert(std::size(DELETE_FUNCTIONS) == (unsigned int)ResourceType::TypeCount);
	static_assert(std::size(UNLOAD_FUNCTIONS) == (unsigned int)ResourceType::TypeCount);

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceManager::ResourceManager(
		ResourceManagerAllocator* memory,
		Graphics* graphics,
		unsigned int thread_count
	) : m_memory(memory), m_graphics(graphics) {
		size_t total_memory = 0;

		total_memory += sizeof(unsigned int) * ECS_RESOURCE_MANAGER_MAX_THREAD_PATH * thread_count;
		total_memory += sizeof(wchar_t) * ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS * thread_count;
		total_memory += sizeof(ThreadPath) * thread_count;
		
		// for alignment
		total_memory += 16;
		void* allocation = memory->Allocate(total_memory, ECS_CACHE_LINE_SIZE);

		// zeroing the whole memory
		memset(allocation, 0, total_memory);

		uintptr_t buffer = (uintptr_t)allocation;

		m_folder_path.InitializeFromBuffer(buffer, thread_count, thread_count);
		for (size_t index = 0; index < thread_count; index++) {
			m_folder_path[index].characters.InitializeFromBuffer(buffer, 0, ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS);
			m_folder_path[index].offsets.InitializeFromBuffer(buffer, 0, ECS_RESOURCE_MANAGER_MAX_THREAD_PATH);
		}

		m_resource_types = ResizableStream<InternalResourceType, ResourceManagerAllocator>(m_memory, 0);

		// initializing WIC loader
#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
		Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
		if (FAILED(initialize))
			// error
#else
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(hr, L"Initializing WIC loader failed!", true);
#endif
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::AddResourcePath(const char* subpath, unsigned int thread_index) {
		size_t path_size = strnlen_s(subpath, ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS);;		
		ECS_ASSERT(m_folder_path[thread_index].characters.size + path_size < m_folder_path[thread_index].characters.capacity, "Not enough characters in the resource folder path");

		m_folder_path[thread_index].offsets.AddSafe(m_folder_path[thread_index].characters.size);
		function::ConvertASCIIToWide(m_folder_path[thread_index].characters, Stream<char>(subpath, path_size));
		m_folder_path[thread_index].characters[m_folder_path[thread_index].characters.size] = L'\0';
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::AddResourcePath(const wchar_t* subpath, unsigned int thread_index) {
		size_t path_size = wcsnlen_s(subpath, ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS);;
		ECS_ASSERT(m_folder_path[thread_index].characters.size + path_size < m_folder_path[thread_index].characters.capacity, "Not enough characters in the resource folder path");

		m_folder_path[thread_index].offsets.AddSafe(m_folder_path[thread_index].characters.size);
		m_folder_path[thread_index].characters.AddStream(Stream<wchar_t>(subpath, path_size));
		m_folder_path[thread_index].characters[m_folder_path[thread_index].characters.size] = '\0';
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::AddResourceType(const char* type_name, unsigned int resource_count)
	{
		InternalResourceType type;
		size_t name_length = strnlen_s(type_name, ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS);
		char* name_allocation = (char*)Allocate(name_length);
		type.name = name_allocation;
		memcpy(name_allocation, type_name, sizeof(char) * name_length);
		
		size_t table_size = type.table.MemoryOf(resource_count);
		void* allocation = Allocate(table_size);
		memset(allocation, 0, table_size);
		type.table.InitializeFromBuffer(allocation, resource_count);
		m_resource_types.Add(type);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void* ResourceManager::Allocate(size_t size)
	{
		return m_memory->Allocate(size);
	}

	AllocatorPolymorphic ResourceManager::Allocator()
	{
		return { m_memory, AllocatorType::MemoryManager, AllocationType::SingleThreaded };
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::Deallocate(const void* data)
	{
		m_memory->Deallocate(data);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool delete_if_zero>
	void ResourceManager::DecrementReferenceCount(ResourceType type, unsigned int amount)
	{
		unsigned int type_int = (unsigned int)type;
		auto value_stream = m_resource_types[type_int].table.GetValueStream();
		const ResourceIdentifier* identifiers = m_resource_types[type_int].table.GetIdentifiers();

		for (size_t index = 0; index < value_stream.size; index++) {
			if (m_resource_types[type_int].table.IsItemAt(index)) {
				DataPointer* ptr = value_stream.buffer + index;
				unsigned short value = ptr->GetData();

				if (value != USHORT_MAX) {
					unsigned short count = ptr->DecrementData(amount);
					if constexpr (delete_if_zero) {
						if (count == 0) {
							DELETE_FUNCTIONS[type_int](this, index, 1);
						}
					}
				}
			}
		}
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::DecrementReferenceCount, ResourceType, unsigned int);

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::DeleteResourcePath(unsigned int thread_index)
	{
		ECS_ASSERT(m_folder_path[thread_index].offsets.size > 0);
		m_folder_path[thread_index].offsets.size--;
		m_folder_path[thread_index].characters.size = m_folder_path[thread_index].offsets[m_folder_path[thread_index].offsets.size];
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::Exists(ResourceIdentifier identifier, ResourceType type) const
	{
		unsigned int hash_index = ResourceManagerHash::Hash(identifier);
		int hashed_position = m_resource_types[(unsigned int)type].table.Find<true>(hash_index, identifier);
		return hashed_position != -1;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::Exists(ResourceIdentifier identifier, ResourceType type, unsigned int& table_index) const {
		unsigned int hash_index = ResourceManagerHash::Hash(identifier);
		table_index = m_resource_types[(unsigned int)type].table.Find<true>(hash_index, identifier);
		return table_index != -1;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	int ResourceManager::GetResourceIndex(ResourceIdentifier identifier, ResourceType type) const
	{
		unsigned int hash_index = ResourceManagerHash::Hash(identifier);
		int index = m_resource_types[(unsigned int)type].table.Find<true>(hash_index, identifier);

		ECS_ASSERT(index != -1, "The resource was not found");
		return index;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void* ResourceManager::GetResource(ResourceIdentifier identifier, ResourceType type)
	{
		unsigned int hash_index = ResourceManagerHash::Hash(identifier);
		int hashed_position = m_resource_types[(unsigned int)type].table.Find<true>(hash_index, identifier);
		if (hashed_position != -1) {
			return m_resource_types[(unsigned int)type].table.GetValueFromIndex(hashed_position).GetPointer();
		}
		ECS_ASSERT(false, "The resource was not found");
		return nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	size_t ResourceManager::GetThreadPath(wchar_t* characters, unsigned int thread_index) const
	{
		if (m_folder_path[thread_index].offsets.size > 0) {
			m_folder_path[thread_index].characters.CopyTo(characters);
			return m_folder_path[thread_index].characters.size;
		}
		return 0;
	}

	Stream<wchar_t> ResourceManager::GetThreadPath(wchar_t* characters, const wchar_t* current_path, unsigned int thread_index) const
	{
		size_t current_path_size = wcslen(current_path);
		size_t offset = GetThreadPath(characters, thread_index);
		memcpy(characters + offset, current_path, sizeof(wchar_t) * current_path_size);
		characters[offset + current_path_size] = L'\0';
		return { characters, offset + current_path_size };
	}

	Stream<wchar_t> ResourceManager::GetThreadPath(wchar_t* characters, const char* current_path, unsigned int thread_index) const
	{
		size_t current_path_size = strlen(current_path);
		size_t offset = GetThreadPath(characters, thread_index);
		function::ConvertASCIIToWide(characters + offset, { current_path, current_path_size }, 512);
		characters[offset + current_path_size] = L'\0';
		return { characters, offset + current_path_size };
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::InitializeDefaultTypes() {
		for (size_t index = 0; index < (unsigned int)ResourceType::TypeCount; index++) {
			AddResourceType(ECS_RESOURCE_TYPE_NAMES[index], ECS_RESOURCE_MANAGER_DEFAULT_RESOURCE_COUNT);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::IncrementReferenceCount(ResourceType type, unsigned int amount)
	{
		unsigned int type_int = (unsigned int)type;
		auto value_stream = m_resource_types[type_int].table.GetValueStream();
		for (size_t index = 0; index < value_stream.size; index++) {
			if (m_resource_types[type_int].table.IsItemAt(index)) {
				DataPointer* ptr = value_stream.buffer + index;
				unsigned short count = ptr->IncrementData(amount);
				if (count == USHORT_MAX) {
					ptr->SetData(USHORT_MAX - 1);
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	char* ResourceManager::LoadTextFile(
		const char* filename, 
		size_t& size, 
		size_t load_flags,
		unsigned int thread_index,
		bool* reference_counted_is_loaded
	)
	{
		return (char*)AddResource<reference_counted>(
			this, 
			filename,
			ResourceType::TextFile,
			(void*)&size,
			load_flags, 
			reference_counted_is_loaded,
			[=](void* parameter) {
			std::ifstream input;
			OpenFile(filename, input, thread_index);

			return LoadTextFileImplementation(input, (size_t*)parameter);
		});
	}

	ECS_TEMPLATE_FUNCTION_BOOL(char*, ResourceManager::LoadTextFile, const char*, size_t&, size_t, unsigned int, bool*);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	char* ResourceManager::LoadTextFile(
		const wchar_t* filename,
		size_t& size, 
		size_t load_flags,
		unsigned int thread_index,
		bool* reference_counted_is_loaded
	)
	{
		return (char*)AddResource<reference_counted>(
			this,
			ResourceIdentifier(filename),
			ResourceType::TextFile, 
			(void*)&size, 
			load_flags, 
			reference_counted_is_loaded,
			[=](void* parameter) {
				std::ifstream input;
				OpenFile(filename, input, thread_index);
				
				return LoadTextFileImplementation(input, (size_t*)parameter);
			}
		);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(char*, ResourceManager::LoadTextFile, const wchar_t*, size_t&, size_t, unsigned int, bool*);

	// ---------------------------------------------------------------------------------------------------------------------------

	char* ResourceManager::LoadTextFileImplementation(std::ifstream& input, size_t* parameter)
	{
		constexpr size_t temp_characters = 4096;

		size_t size = *parameter;
		if (size == 0) {
			size = function::GetFileByteSize(input);
		}
		void* allocation = m_memory->Allocate(size, 1);
		if (allocation == nullptr) {
			return (char*)nullptr;
		}

		input.seekg(0, std::ifstream::beg);
		input.read((char*)allocation, 10'000'000'000);
		size_t gcount = input.gcount();
		ECS_ASSERT(gcount <= size);
		*(size_t*)parameter = input.gcount();
		return (char*)allocation;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::OpenFile(const char* filename, std::ifstream& input, unsigned int thread_index)
	{
		ECS_TEMP_STRING(path, 512);
		Stream<wchar_t> current_path = GetThreadPath(path.buffer, filename, thread_index);
		input.open(current_path.buffer, std::ifstream::in);
		
		ECS_ASSERT(input.good(), "Opening a file failed!");
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::OpenFile(const wchar_t* filename, std::ifstream& input, unsigned int thread_index)
	{
		ECS_TEMP_STRING(path, 512);
		Stream<wchar_t> current_path = GetThreadPath(path.buffer, filename, thread_index);
		input.open(current_path.buffer, std::ifstream::in);

		ECS_ASSERT(input.good(), "Opening a file failed!");
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	ResourceView ResourceManager::LoadTexture(
		const wchar_t* filename,
		ResourceManagerTextureDesc& descriptor,
		size_t load_flags,
		unsigned int thread_index,
		bool* reference_counted_is_loaded
	)
	{
		void* texture_view = AddResource<reference_counted>(
			this,
			filename,
			ResourceType::Texture, 
			&descriptor,
			load_flags, 
			reference_counted_is_loaded,
			[=](void* parameter) {
			ResourceManagerTextureDesc* reinterpretation = (ResourceManagerTextureDesc*)parameter;
			return LoadTextureImplementation(filename, reinterpretation, thread_index).view;
		});

		return (ID3D11ShaderResourceView*)texture_view;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(ResourceView, ResourceManager::LoadTexture, const wchar_t*, ResourceManagerTextureDesc&, size_t, unsigned int, bool*);

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceView ResourceManager::LoadTextureImplementation(
		const wchar_t* filename,
		ResourceManagerTextureDesc* descriptor,
		unsigned int thread_index
	)
	{
		ID3D11ShaderResourceView* texture_view = nullptr;
		ID3D11Resource* resource = nullptr;

		// Determine the texture extension - HDR textures must take a different path
		Stream<wchar_t> texture_path = ToStream(filename);
		Path2 extension = function::PathExtensionBoth(texture_path);
		ECS_ASSERT(extension.absolute.size > 0 || extension.relative.size > 0, "No extension could be identified");

		bool is_hdr_texture = false;
		if (extension.absolute.size > 0) {
			is_hdr_texture = function::CompareStrings(extension.absolute, ToStream(L".hdr"));
		}
		else {
			is_hdr_texture = function::CompareStrings(extension.relative, ToStream(L".hdr"));
		}

		ECS_TEMP_STRING(_path, 512);
		Stream<wchar_t> path = GetThreadPath(_path.buffer, filename, thread_index);
		HRESULT result;
		
		if (is_hdr_texture) {
			DirectX::ScratchImage image;
			result = DirectX::LoadFromHDRFile(path.buffer, nullptr, image);
			if (FAILED(result)) {
				return nullptr;
			}
			
			// Convert to a DX11 resource
			GraphicsTexture2DDescriptor dx_descriptor;
			auto metadata = image.GetMetadata();
			dx_descriptor.format = metadata.format;
			dx_descriptor.array_size = 1;
			dx_descriptor.bind_flag = (D3D11_BIND_FLAG)descriptor->bindFlags;
			dx_descriptor.cpu_flag = (D3D11_CPU_ACCESS_FLAG)descriptor->cpuAccessFlags;
			dx_descriptor.initial_data = image.GetPixels();
			size_t row_pitch, slice_pitch;
			result = DirectX::ComputePitch(metadata.format, metadata.width, metadata.height, row_pitch, slice_pitch);
			if (FAILED(result)) {
				return nullptr;
			}
			dx_descriptor.memory_pitch = row_pitch;
			dx_descriptor.memory_slice_pitch = slice_pitch;
			dx_descriptor.misc_flag = descriptor->miscFlags;
			dx_descriptor.usage = descriptor->usage;
			dx_descriptor.size = { (unsigned int)metadata.width, (unsigned int)metadata.height };

			// No context provided - don't generate mips
			if (descriptor->context == nullptr) {
				dx_descriptor.mip_levels = 1;
				Texture2D texture = m_graphics->CreateTexture(&dx_descriptor);
				texture_view = m_graphics->CreateTextureShaderViewResource(texture).view;
			}
			else {
				ECS_ASSERT(descriptor->context == m_graphics->GetContext());

				// Make the initial data nullptr
				dx_descriptor.mip_levels = 0;
				dx_descriptor.initial_data = nullptr;
				dx_descriptor.memory_pitch = 0;
				dx_descriptor.memory_slice_pitch = 0;
				Texture2D texture = m_graphics->CreateTexture(&dx_descriptor);

				// Update the mip 0
				UpdateTextureResource(texture, image.GetPixels(), image.GetPixelsSize(), descriptor->context);

				ResourceView view = m_graphics->CreateTextureShaderViewResource(texture);

				// Generate mips
				m_graphics->GenerateMips(view);
				texture_view = view.view;
			}
			
		}
		else {
			result = DirectX::CreateWICTextureFromFileEx(
				m_graphics->GetDevice(),
				descriptor->context,
				path.buffer,
				descriptor->max_size,
				descriptor->usage,
				descriptor->bindFlags,
				descriptor->cpuAccessFlags,
				descriptor->miscFlags,
				descriptor->loader_flag,
				&resource,
				&texture_view
			);
			if (FAILED(result)) {
				return nullptr;
			}
		}

		if ((unsigned char)descriptor->compression != (unsigned char)-1) {
			Texture2D texture(resource);
			bool success = CompressTexture(texture, descriptor->compression);
			if (!success) {
				return texture_view;
			}

			texture_view = m_graphics->CreateTextureShaderView(texture).view;
		}

		return texture_view;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes from a gltf file
	template<bool reference_counted>
	Stream<Mesh>* ResourceManager::LoadMeshes(
		const wchar_t* filename,
		size_t load_flags,
		unsigned int thread_index,
		bool* reference_counted_is_loaded
	) {
		void* meshes = AddResource<reference_counted>(
			this,
			filename,
			ResourceType::Mesh,
			nullptr,
			load_flags,
			reference_counted_is_loaded,
			[=](void* parameter) {
				return LoadMeshImplementation(filename, load_flags, thread_index);
			});

		return (Stream<Mesh>*)meshes;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Stream<Mesh>*, ResourceManager::LoadMeshes, const wchar_t*, size_t, unsigned int, bool*);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes from a gltf file
	Stream<Mesh>* ResourceManager::LoadMeshImplementation(const wchar_t* filename, size_t load_flags, unsigned int thread_index) {
		ECS_TEMP_STRING(_path, 512);
		Stream<wchar_t> path = GetThreadPath(_path.buffer, filename, thread_index);

		GLTFData data = LoadGLTFFile(path);
		// The load failed
		if (data.data == nullptr) {
			return nullptr;
		}
		ECS_ASSERT(data.mesh_count < 200);

		GLTFMesh* gltf_meshes = (GLTFMesh*)ECS_STACK_ALLOC(sizeof(GLTFMesh) * data.mesh_count);
		// Nullptr and 0 everything
		memset(gltf_meshes, 0, sizeof(GLTFMesh) * data.mesh_count);
		AllocatorPolymorphic allocator = Allocator();

		bool has_invert = !function::HasFlag(load_flags, ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT);
		bool success = LoadMeshesFromGLTF(data, gltf_meshes, allocator, has_invert);
		// The load failed
		if (!success) {
			// The gltf data must be freed
			FreeGLTFFile(data);
			return nullptr;
		}

		void* allocation = nullptr;
		// Calculate the allocation size
		size_t allocation_size = sizeof(Stream<Mesh>);
		allocation_size += sizeof(Mesh) * data.mesh_count;

		// Allocate the needed memeory
		allocation = Allocate(allocation_size);
		Stream<Mesh>* meshes = (Stream<Mesh>*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(Stream<Mesh>);
		meshes->InitializeFromBuffer(buffer, data.mesh_count);

		// Convert the gltf meshes into multiple meshes
		GLTFMeshesToMeshes(m_graphics, gltf_meshes, meshes->buffer, meshes->size);

		// Free the gltf file data and the gltf meshes
		FreeGLTFFile(data);
		FreeGLTFMeshes(gltf_meshes, data.mesh_count, allocator);
		return meshes;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	CoallescedMesh* ResourceManager::LoadCoallescedMesh(const wchar_t* filename, size_t load_flags, unsigned int thread_index, bool* reference_counted_is_loaded)
	{
		void* meshes = AddResource<reference_counted>(
			this,
			filename,
			ResourceType::CoallescedMesh,
			nullptr,
			load_flags,
			reference_counted_is_loaded,
			[=](void* parameter) {
				return LoadCoallescedMeshImplementation(filename, load_flags, thread_index);
			});

		return (CoallescedMesh*)meshes;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(CoallescedMesh*, ResourceManager::LoadCoallescedMesh, const wchar_t*, size_t, unsigned int, bool*);

	// ---------------------------------------------------------------------------------------------------------------------------

	CoallescedMesh* ResourceManager::LoadCoallescedMeshImplementation(const wchar_t* filename, size_t load_flags, unsigned int thread_index)
	{
		ECS_TEMP_STRING(_path, 512);
		Stream<wchar_t> path = GetThreadPath(_path.buffer, filename, thread_index);

		GLTFData data = LoadGLTFFile(path);
		// The load failed
		if (data.data == nullptr) {
			return nullptr;
		}
		ECS_ASSERT(data.mesh_count < 200);

		GLTFMesh* gltf_meshes = (GLTFMesh*)ECS_STACK_ALLOC(sizeof(GLTFMesh) * data.mesh_count);
		// Nullptr and 0 everything
		memset(gltf_meshes, 0, sizeof(GLTFMesh) * data.mesh_count);
		AllocatorPolymorphic allocator = Allocator();

		bool has_invert = !function::HasFlag(load_flags, ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT);
		bool success = LoadMeshesFromGLTF(data, gltf_meshes, allocator, has_invert);
		// The load failed
		if (!success) {
			// The gltf data must be freed
			FreeGLTFFile(data);
			return nullptr;
		}


		void* allocation = nullptr;
		// Calculate the allocation size
		size_t allocation_size = sizeof(CoallescedMesh) + sizeof(Submesh) * data.mesh_count;

		Mesh* temporary_meshes = (Mesh*)ECS_STACK_ALLOC(sizeof(Mesh) * data.mesh_count);

		// Allocate the needed memory
		allocation = Allocate(allocation_size);
		CoallescedMesh* mesh = (CoallescedMesh*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(CoallescedMesh);
		mesh->submeshes.InitializeFromBuffer(buffer, data.mesh_count);

		// Convert the gltf meshes into multiple meshes and then convert these to an aggregated mesh
		GLTFMeshesToMeshes(m_graphics, gltf_meshes, temporary_meshes, data.mesh_count);

		// Convert now to aggregated mesh
		mesh->mesh = MeshesToSubmeshes(m_graphics, { temporary_meshes, data.mesh_count }, mesh->submeshes.buffer);

		// Free the gltf file data and the gltf meshes
		FreeGLTFFile(data);
		FreeGLTFMeshes(gltf_meshes, data.mesh_count, allocator);
		return mesh;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all materials from a gltf file
	template<bool reference_counted>
	Stream<PBRMaterial>* ResourceManager::LoadMaterials(
		const wchar_t* filename,
		size_t load_flags,
		unsigned int thread_index,
		bool* reference_counted_is_loaded
	) {
		void* materials = AddResource<reference_counted>(
			this,
			filename,
			ResourceType::Material,
			nullptr,
			load_flags,
			reference_counted_is_loaded,
			[=](void* parameter) {
				return LoadMaterialsImplementation(filename, load_flags, thread_index);
			});

		return (Stream<PBRMaterial>*)materials;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Stream<PBRMaterial>*, ResourceManager::LoadMaterials, const wchar_t*, size_t, unsigned int, bool*);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all materials from a gltf file
	Stream<PBRMaterial>* ResourceManager::LoadMaterialsImplementation(const wchar_t* filename, size_t load_flags, unsigned int thread_index) {
		ECS_TEMP_STRING(_path, 512);
		Stream<wchar_t> path = GetThreadPath(_path.buffer, filename, thread_index);

		GLTFData data = LoadGLTFFile(path);
		// The load failed
		if (data.data == nullptr) {
			return nullptr;
		}
		ECS_ASSERT(data.mesh_count < 200);

		PBRMaterial* _materials = (PBRMaterial*)ECS_STACK_ALLOC((sizeof(PBRMaterial) + sizeof(unsigned int)) * data.mesh_count);
		Stream<PBRMaterial> materials(_materials, 0);
		Stream<unsigned int> material_mask(function::OffsetPointer(_materials, sizeof(PBRMaterial) * data.mesh_count), 0);

		AllocatorPolymorphic allocator = Allocator();
		bool success = LoadDisjointMaterialsFromGLTF(data, materials, material_mask, allocator);

		// The load failed
		if (!success) {
			// The gltf data must be freed
			FreeGLTFFile(data);
			return nullptr;
		}

		// Calculate the total memory needed - the pbr materials already allocated memory for themselves with
		// the call to load the disjoint materials
		size_t allocation_size = sizeof(Stream<PBRMaterial>);
		allocation_size += sizeof(PBRMaterial) * materials.size;

		void* allocation = Allocate(allocation_size);
		Stream<PBRMaterial>* pbr_materials = (Stream<PBRMaterial>*)allocation;
		pbr_materials->InitializeFromBuffer(function::OffsetPointer(allocation, sizeof(Stream<PBRMaterial>)), materials.size);
		memcpy(pbr_materials->buffer, materials.buffer, sizeof(PBRMaterial) * materials.size);

		// Free the gltf data
		FreeGLTFFile(data);

		return pbr_materials;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes and materials from a gltf file, combines the meshes into a single one sorted by material submeshes
	template<bool reference_counted>
	PBRMesh* ResourceManager::LoadPBRMesh(
		const wchar_t* filename,
		size_t load_flags,
		unsigned int thread_index,
		bool* reference_counted_is_loaded
	) {
		void* mesh = AddResource<reference_counted>(
			this,
			filename,
			ResourceType::PBRMesh,
			nullptr,
			load_flags,
			reference_counted_is_loaded,
			[=](void* parameter) {
				return LoadPBRMeshImplementation(filename, load_flags, thread_index);
			});

		return (PBRMesh*)mesh;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(PBRMesh*, ResourceManager::LoadPBRMesh, const wchar_t*, size_t, unsigned int, bool*);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes and materials from a gltf file, combines the meshes into a single one sorted by material submeshes
	PBRMesh* ResourceManager::LoadPBRMeshImplementation(const wchar_t* filename, size_t load_flags, unsigned int thread_index) {
		ECS_TEMP_STRING(_path, 512);
		Stream<wchar_t> path = GetThreadPath(_path.buffer, filename, thread_index);

		GLTFData data = LoadGLTFFile(path);
		// The load failed
		if (data.data == nullptr) {
			return nullptr;
		}
		ECS_ASSERT(data.mesh_count < 200);

		void* initial_allocation = Allocate((sizeof(GLTFMesh) + sizeof(PBRMaterial) + sizeof(unsigned int)) * data.mesh_count);
		GLTFMesh* gltf_meshes = (GLTFMesh*)initial_allocation;
		Stream<PBRMaterial> pbr_materials(function::OffsetPointer(gltf_meshes, sizeof(GLTFMesh) * data.mesh_count), 0);
		unsigned int* material_masks = (unsigned int*)function::OffsetPointer(pbr_materials.buffer, sizeof(PBRMaterial) * data.mesh_count);
		AllocatorPolymorphic allocator = Allocator();

		bool has_invert = !function::HasFlag(load_flags, ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT);
		bool success = LoadMeshesAndDisjointMaterialsFromGLTF(data, gltf_meshes, pbr_materials, material_masks, allocator, has_invert);
		// The load failed
		if (!success) {
			// Free the gltf data and the initial allocation
			FreeGLTFFile(data);
			Deallocate(initial_allocation);
			return nullptr;
		}

		// Calculate the allocation size
		size_t allocation_size = sizeof(PBRMesh);
		allocation_size += (sizeof(Submesh) + sizeof(PBRMaterial)) * pbr_materials.size;

		// Allocate the instance
		void* allocation = Allocate(allocation_size);
		PBRMesh* mesh = (PBRMesh*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(PBRMesh);

		mesh->mesh.submeshes.buffer = (Submesh*)buffer;
		buffer += sizeof(Submesh) * pbr_materials.size;

		mesh->materials = (PBRMaterial*)buffer;

		mesh->mesh.mesh = GLTFMeshesToMergedMesh(m_graphics, gltf_meshes, mesh->mesh.submeshes.buffer, material_masks, pbr_materials.size, data.mesh_count);
		mesh->mesh.submeshes.size = pbr_materials.size;

		// Copy the pbr materials to this new buffer
		memcpy(mesh->materials, pbr_materials.buffer, sizeof(PBRMaterial) * pbr_materials.size);

		// Free the gltf data, the gltf meshes and the initial allocation
		FreeGLTFFile(data);
		FreeGLTFMeshes(gltf_meshes, data.mesh_count, allocator);
		Deallocate(initial_allocation);

		return mesh;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::RebindResource(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource)
	{
		unsigned int hash_index = ResourceManagerHash::Hash(identifier);
		unsigned int int_type = (unsigned int)resource_type;

		DataPointer* data_pointer;
		bool success = m_resource_types[int_type].table.TryGetValuePtr(hash_index, identifier, data_pointer);
		ECS_ASSERT(success, "Could not rebind resource");

		UNLOAD_FUNCTIONS[int_type](data_pointer->GetPointer(), this);

		//if (success) {
		data_pointer->SetPointer(new_resource);
		//}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::RebindResourceNoDestruction(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource)
	{
		unsigned int hash_index = ResourceManagerHash::Hash(identifier);
		unsigned int int_type = (unsigned int)resource_type;
		DataPointer* data_pointer;
		bool success = m_resource_types[int_type].table.TryGetValuePtr(hash_index, identifier, data_pointer);

		ECS_ASSERT(success, "Could not rebind resource");
		//if (success) {
		data_pointer->SetPointer(new_resource);
		//}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::RemoveReferenceCountForResource(ResourceIdentifier identifier, ResourceType resource_type)
	{
		unsigned int hash_index = ResourceManagerHash::Hash(identifier);
		unsigned int type_index = (unsigned int)resource_type;
		DataPointer* data_pointer;
		bool success = m_resource_types[type_index].table.TryGetValuePtr(hash_index, identifier, data_pointer);

		ECS_ASSERT(success, "Could not remove reference count for resource");
		data_pointer->SetData(USHORT_MAX);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadTextFile(const char* filename, size_t flags)
	{
		DeleteResource<reference_counted>(this, { filename, (unsigned int)strlen(filename) }, ResourceType::TextFile, flags, UnloadTextFileHandler);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadTextFile, const char*, size_t);

	template<bool reference_counted>
	void ResourceManager::UnloadTextFile(const wchar_t* filename, size_t flags)
	{
		DeleteResource<reference_counted>(this, filename, ResourceType::TextFile, flags, UnloadTextFileHandler);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadTextFile, const wchar_t*, size_t);

	template<bool reference_counted>
	void ResourceManager::UnloadTextFile(unsigned int index, size_t flags) {
		DeleteResource<reference_counted>(this, index, ResourceType::TextFile, flags, UnloadTextFileHandler);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadTextFile, unsigned int, size_t);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadTexture(const wchar_t* filename, size_t flags)
	{
		DeleteResource<reference_counted>(this, filename, ResourceType::Texture, flags, UnloadTextureHandler);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadTexture, const wchar_t*, size_t);

	template<bool reference_counted>
	void ResourceManager::UnloadTexture(unsigned int index, size_t flags) {
		DeleteResource<reference_counted>(this, index, ResourceType::Texture, flags, UnloadTextureHandler);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadMeshes(const wchar_t* filename, size_t flags) {
		DeleteResource<reference_counted>(this, filename, ResourceType::Mesh, flags, UnloadMeshesHandler);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadMeshes(unsigned int index, size_t flags) {
		DeleteResource<reference_counted>(this, index, ResourceType::Mesh, flags, UnloadMeshesHandler);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadCoallescedMesh(const wchar_t* filename, size_t flags)
	{
		DeleteResource<reference_counted>(this, filename, ResourceType::CoallescedMesh, flags, UnloadCoallescedMeshHandler);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadCoallescedMesh(unsigned int index, size_t flags)
	{
		DeleteResource<reference_counted>(this, index, ResourceType::CoallescedMesh, flags, UnloadCoallescedMeshHandler);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadMaterials(const wchar_t* filename, size_t flags) {
		DeleteResource<reference_counted>(this, filename, ResourceType::Material, flags, UnloadMaterialsHandler);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadMaterials(unsigned int index, size_t flags) {
		DeleteResource<reference_counted>(this, index, ResourceType::Material, flags, UnloadMaterialsHandler);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadPBRMesh(const wchar_t* filename, size_t flags) {
		DeleteResource<reference_counted>(this, filename, ResourceType::PBRMesh, flags, UnloadPBRMeshHandler);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadPBRMesh(unsigned int index, size_t flags) {
		DeleteResource<reference_counted>(this, index, ResourceType::PBRMesh, flags, UnloadPBRMeshHandler);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

#pragma region Free functions

	// ---------------------------------------------------------------------------------------------------------------------------

	Material PBRToMaterial(ResourceManager* resource_manager, const PBRMaterial& pbr)
	{
		Material result;




		return result;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

#pragma endregion

}