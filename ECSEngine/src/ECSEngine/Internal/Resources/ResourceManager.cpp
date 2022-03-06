#include "ecspch.h"
#include "ResourceManager.h"
#include "../../Rendering/GraphicsHelpers.h"
#include "../../Rendering/Compression/TextureCompression.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../../GLTF/GLTFLoader.h"
#include "../../Utilities/Path.h"
#include "../../Utilities/File.h"
#include "../../Utilities/ForEachFiles.h"
#include "../../Rendering/ShaderInclude.h"
#include "../../Rendering/Shader Application Stage/PBR.h"
#include "../../Rendering/DirectXTexHelpers.h"
#include "../../Utilities/OSFunctions.h"

#define ECS_RESOURCE_MANAGER_CHECK_RESOURCE

constexpr size_t ECS_RESOURCE_MANAGER_DEFAULT_RESOURCE_COUNT = 256;
constexpr size_t ECS_RESOURCE_MANAGER_MAX_THREAD_PATH = 8;
constexpr size_t ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS = 128;

constexpr size_t ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_BACKUP_SIZE = ECS_MB * 10;

namespace ECSEngine {

#define ECS_RESOURCE_MANAGER_INITIAL_LOAD_INCREMENT_COUNT 100


	// The handler must implement a void* parameter for additional info and must return void*
	// If it returns a nullptr, it means the load failed so do not add it to the table
	template<bool reference_counted, typename Handler>
	void* LoadResource(
		ResourceManager* resource_manager,
		const wchar_t* path,
		ResourceType type,
		ResourceManagerLoadDesc load_descriptor,
		Handler&& handler
	) {
		unsigned int type_int = (unsigned int)type;
		ResourceIdentifier identifier(path);

		auto register_resource = [=](void* data) {
			// Account for the L'\0'
			void* allocation = function::Copy(resource_manager->m_memory, identifier.ptr, identifier.size + 2);

			ResourceManagerEntry entry;
			entry.data_pointer.SetPointer(data);
			entry.data_pointer.SetData(USHORT_MAX);
			entry.time_stamp = OS::GetFileLastWrite(path);

			bool is_table_full = resource_manager->m_resource_types[type_int].table.Insert(entry, {allocation, identifier.size});
			ECS_ASSERT(!is_table_full, "Table is too full or too many collisions!");
		};

		if constexpr (!reference_counted) {
#ifdef ECS_RESOURCE_MANAGER_CHECK_RESOURCE
			bool exists = resource_manager->Exists(identifier, type);
			ECS_ASSERT(!exists, "The resource already exists!");
#endif
			void* data = handler();
			if (data != nullptr) {
				register_resource(data);
			}
			return data;
		}
		else {
			unsigned int table_index;
			bool exists = resource_manager->Exists(identifier, type, table_index);

			unsigned short increment_count = load_descriptor.load_flags & ECS_RESOURCE_MANAGER_MASK_INCREMENT_COUNT;
			if (!exists) {
				void* data = handler();

				if (data != nullptr) {
					register_resource(data);

					if (load_descriptor.reference_counted_is_loaded != nullptr) {
						*load_descriptor.reference_counted_is_loaded = true;
					}
				}
				return data;
			}

			ResourceManagerEntry* entry = resource_manager->m_resource_types[type_int].table.GetValuePtrFromIndex(table_index);
			unsigned short count = entry->data_pointer.IncrementData(increment_count);
			if (count == USHORT_MAX) {
				entry->data_pointer.SetData(USHORT_MAX - 1);
			}

			if (load_descriptor.reference_counted_is_loaded != nullptr) {
				*load_descriptor.reference_counted_is_loaded = false;
			}
			return entry->data_pointer.GetPointer();
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// The handler must implement a void* parameter for additional info and a void* for its allocation and must return void*
	template<bool reference_counted, typename Handler>
	void* LoadResource(
		ResourceManager* resource_manager,
		const wchar_t* path,
		ResourceType type,
		size_t allocation_size,
		ResourceManagerLoadDesc load_descriptor,
		Handler&& handler
	) {
		unsigned int type_int = (unsigned int)type;
		ResourceIdentifier identifier(path);

		auto register_resource = [=](unsigned short initial_increment) {
			// plus 7 bytes for padding and account for the L'\0'
			void* allocation = resource_manager->Allocate(allocation_size + identifier.size + sizeof(wchar_t) + 7);
			ECS_ASSERT(allocation != nullptr, "Allocating memory for a resource failed");
			memcpy(allocation, identifier.ptr, identifier.size + sizeof(wchar_t));

			allocation = (void*)function::align_pointer((uintptr_t)allocation + identifier.size + sizeof(wchar_t), 8);

			memset(allocation, 0, allocation_size);
			void* data = handler(allocation);

			if (data != nullptr) {
				ResourceManagerEntry entry;
				entry.data_pointer.SetPointer(data);
				entry.data_pointer.SetData(initial_increment);
				entry.time_stamp = OS::GetFileLastWrite(path);

				bool is_table_full = resource_manager->m_resource_types[type_int].table.Insert(entry, {allocation, identifier.size});
				ECS_ASSERT(!is_table_full, "Table is too full or too many collisions!");

				if (load_descriptor.reference_counted_is_loaded != nullptr) {
					*load_descriptor.reference_counted_is_loaded = true;
				}
			}
			// The load failed, release the allocation
			else {
				resource_manager->Deallocate(allocation);
			}

			return data;
		};

		if constexpr (!reference_counted) {
#ifdef ECS_RESOURCE_MANAGER_CHECK_RESOURCE
			bool exists = resource_manager->Exists(identifier, type);
			ECS_ASSERT(!exists, "Resource already exists!");
#endif

			return register_resource(USHORT_MAX);
		}
		else {
			unsigned int table_index;
			bool exists = resource_manager->Exists(identifier, type, table_index);

			if (!exists) {
				//// plus 7 bytes for padding
				//void* allocation = resource_manager->Allocate(allocation_size + identifier.size + 7);
				//ECS_ASSERT(allocation != nullptr, "Allocating memory for a resource failed");
				//memcpy(allocation, identifier.ptr, identifier.size);
				//identifier.ptr = allocation;

				//allocation = (void*)function::align_pointer((uintptr_t)allocation + identifier.size, 8);

				//memset(allocation, 0, allocation_size);
				//void* data = handler(allocation);

				//if (data != nullptr) {
				//	DataPointer data_pointer(data);
				//	data_pointer.SetData(ECS_RESOURCE_MANAGER_INITIAL_LOAD_INCREMENT_COUNT);

				//	bool is_table_full = resource_manager->m_resource_types[type_int].table.Insert(data_pointer, identifier);
				//	ECS_ASSERT(!is_table_full, "Table is too full or too many collisions!");

				//	if (load_descriptor.reference_counted_is_loaded != nullptr) {
				//		*load_descriptor.reference_counted_is_loaded = true;
				//	}
				//}
				//// The load failed, release the allocation
				//else {
				//	resource_manager->Deallocate(allocation);
				//}
				//return data;
				return register_resource(ECS_RESOURCE_MANAGER_INITIAL_LOAD_INCREMENT_COUNT);
			}

			unsigned short increment_count = load_descriptor.load_flags & ECS_RESOURCE_MANAGER_MASK_INCREMENT_COUNT;
			ResourceManagerEntry* entry = resource_manager->m_resource_types[type_int].table.GetValuePtrFromIndex(table_index);
			unsigned short count = entry->data_pointer.IncrementData(increment_count);
			if (count == USHORT_MAX) {
				entry->data_pointer.SetData(USHORT_MAX - 1);
			}

			if (load_descriptor.reference_counted_is_loaded != nullptr) {
				*load_descriptor.reference_counted_is_loaded = false;
			}

			return entry->data_pointer.GetPointer();
		}

		// This path is never taken - it will fall into the else branch
		return nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void DeleteResource(ResourceManager* resource_manager, unsigned int index, ResourceType type, size_t flags) {
		unsigned int type_int = (unsigned int)type;

		ResourceManagerEntry entry = resource_manager->m_resource_types[type_int].table.GetValueFromIndex(index);

		void (*handler)(void*, ResourceManager*) = UNLOAD_FUNCTIONS[type_int];

		auto delete_resource = [=]() {
			const ResourceIdentifier* identifiers = resource_manager->m_resource_types[type_int].table.GetIdentifiers();

			void* data = entry.data_pointer.GetPointer();
			handler(data, resource_manager);
			resource_manager->Deallocate(identifiers[index].ptr);
			resource_manager->m_resource_types[type_int].table.EraseFromIndex(index);
		};
		if constexpr (!reference_counted) {
			delete_resource();
		}
		else {
			unsigned short increment_count = flags & ECS_RESOURCE_MANAGER_MASK_INCREMENT_COUNT;
			unsigned short count = entry.data_pointer.DecrementData(increment_count);
			if (count == 0) {
				delete_resource();
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void DeleteResource(ResourceManager* resource_manager, ResourceIdentifier identifier, ResourceType type, size_t flags) {
		unsigned int type_int = (unsigned int)type;

		int hashed_position = resource_manager->m_resource_types[type_int].table.Find<true>(identifier);
		ECS_ASSERT(hashed_position != -1, "Trying to delete a resource that has not yet been loaded!");

		DeleteResource<reference_counted>(resource_manager, hashed_position, type, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	using DeleteFunction = void (*)(ResourceManager*, unsigned int, size_t);
	using UnloadFunction = void (*)(void*, ResourceManager*);

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadTextureHandler(void* parameter, ResourceManager* resource_manager) {
		ID3D11ShaderResourceView* reinterpretation = (ID3D11ShaderResourceView*)parameter;
		resource_manager->m_graphics->FreeResourceView(reinterpretation);
	}

	void DeleteTexture(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadTexture<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadTextFileHandler(void* parameter, ResourceManager* resource_manager) { resource_manager->m_memory->Deallocate(parameter); }

	void DeleteTextFile(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadTextFile<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadMeshesHandler(void* parameter, ResourceManager* resource_manager) {
		Stream<Mesh>* data = (Stream<Mesh>*)parameter;
		for (size_t index = 0; index < data->size; index++) {
			resource_manager->m_graphics->FreeMesh(data->buffer[index]);
		}
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
	}

	void DeletePBRMesh(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadPBRMesh<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadCoallescedMeshHandler(void* parameter, ResourceManager* resource_manager) {
		// Free the coallesced mesh - the submeshes get the deallocated at the same time as this resource
		CoallescedMesh* mesh = (CoallescedMesh*)parameter;

		resource_manager->m_graphics->FreeMesh(mesh->mesh);
	}

	void DeleteCoallescedMesh(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadCoallescedMesh<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadVertexShaderHandler(void* parameter, ResourceManager* resource_manager) {
		VertexShader* shader = (VertexShader*)parameter;
		resource_manager->m_graphics->FreeResource(*shader);
		if (shader->byte_code.buffer != nullptr) {
			resource_manager->Deallocate(shader->byte_code.buffer);
		}
	}

	void DeleteVertexShader(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadVertexShader<false>(index, flags);
	}

	template<typename ShaderType>
	void UnloadShaderHandler(void* parameter, ResourceManager* resource_manager) {
		ShaderType* shader = (ShaderType*)parameter;
		resource_manager->m_graphics->FreeResource(*shader);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void DeletePixelShader(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadPixelShader<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void DeleteHullShader(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadHullShader<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void DeleteDomainShader(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadDomainShader<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void DeleteGeometryShader(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadGeometryShader<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void DeleteComputeShader(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadComputeShader<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	constexpr DeleteFunction DELETE_FUNCTIONS[] = {
		DeleteTexture, 
		DeleteTextFile,
		DeleteMeshes,
		DeleteCoallescedMesh,
		DeleteMaterials,
		DeletePBRMesh,
		DeleteVertexShader,
		DeletePixelShader,
		DeleteHullShader,
		DeleteDomainShader,
		DeleteGeometryShader,
		DeleteComputeShader
	};
	
	constexpr UnloadFunction UNLOAD_FUNCTIONS[] = {
		UnloadTextureHandler,
		UnloadTextFileHandler,
		UnloadMeshesHandler,
		UnloadCoallescedMeshHandler,
		UnloadMaterialsHandler,
		UnloadPBRMeshHandler,
		UnloadVertexShaderHandler,
		UnloadShaderHandler<PixelShader>,
		UnloadShaderHandler<HullShader>,
		UnloadShaderHandler<DomainShader>,
		UnloadShaderHandler<GeometryShader>,
		UnloadShaderHandler<ComputeShader>
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
		total_memory += sizeof(ThreadResource) * thread_count;
		
		// for alignment
		total_memory += 16;
		void* allocation = memory->Allocate(total_memory, ECS_CACHE_LINE_SIZE);

		// zeroing the whole memory
		memset(allocation, 0, total_memory);

		uintptr_t buffer = (uintptr_t)allocation;

		m_thread_resources.InitializeFromBuffer(buffer, thread_count, thread_count);
		for (size_t index = 0; index < thread_count; index++) {
			m_thread_resources[index].characters.InitializeFromBuffer(buffer, 0, ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS);
			m_thread_resources[index].offsets.InitializeFromBuffer(buffer, 0, ECS_RESOURCE_MANAGER_MAX_THREAD_PATH);
		}

		m_resource_types.Initialize(m_memory, (unsigned int)ResourceType::TypeCount);
		// Set to 0 the initial data for these resource types such that if a type is not being constructed but it is
		// accessed it will cause a crash
		memset(m_resource_types.buffer, 0, m_resource_types.MemoryOf(m_resource_types.size));

		m_shader_directory.Initialize(m_memory, 0, 256);

		// Set the initial shader directory
		SetShaderDirectory(ToStream(ECS_SHADER_DIRECTORY));

//		// initializing WIC loader
//#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
//		Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
//		if (FAILED(initialize))
//			// error
//#else
//		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
//		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(hr, L"Initializing WIC loader failed!", true);
//#endif
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::AddResourcePath(const char* subpath, unsigned int thread_index) {
		size_t path_size = strnlen_s(subpath, ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS);;		
		ECS_ASSERT(m_thread_resources[thread_index].characters.size + path_size < m_thread_resources[thread_index].characters.capacity, "Not enough characters in the resource folder path");

		m_thread_resources[thread_index].offsets.AddSafe(m_thread_resources[thread_index].characters.size);
		function::ConvertASCIIToWide(m_thread_resources[thread_index].characters, Stream<char>(subpath, path_size));
		m_thread_resources[thread_index].characters[m_thread_resources[thread_index].characters.size] = L'\0';
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::AddResourcePath(const wchar_t* subpath, unsigned int thread_index) {
		size_t path_size = wcsnlen_s(subpath, ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS);;
		ECS_ASSERT(m_thread_resources[thread_index].characters.size + path_size < m_thread_resources[thread_index].characters.capacity, "Not enough characters in the resource folder path");

		m_thread_resources[thread_index].offsets.AddSafe(m_thread_resources[thread_index].characters.size);
		m_thread_resources[thread_index].characters.AddStream(Stream<wchar_t>(subpath, path_size));
		m_thread_resources[thread_index].characters[m_thread_resources[thread_index].characters.size] = '\0';
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::AddResourceType(ResourceType resource_type, unsigned int resource_count)
	{
		InternalResourceType type;
		size_t name_length = strnlen_s(ECS_RESOURCE_TYPE_NAMES[(unsigned int)resource_type], ECS_RESOURCE_MANAGER_PATH_STRING_CHARACTERS);
		char* name_allocation = (char*)Allocate(name_length);
		type.name = name_allocation;
		memcpy(name_allocation, ECS_RESOURCE_TYPE_NAMES[(unsigned int)resource_type], sizeof(char) * name_length);
		
		size_t table_size = type.table.MemoryOf(resource_count);
		void* allocation = Allocate(table_size);
		memset(allocation, 0, table_size);
		type.table.InitializeFromBuffer(allocation, resource_count);

		m_resource_types[(unsigned int)resource_type] = type;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// TODO: Implement invariance allocations - some resources need when unloaded to have deallocated some buffers and those might not come
	// from the resource manager
	void ResourceManager::AddResource(const wchar_t* path, ResourceType resource_type, void* resource, size_t time_stamp, unsigned short reference_count)
	{
		ResourceManagerEntry entry;
		entry.data_pointer.SetPointer(resource);
		entry.data_pointer.SetData(reference_count);
		entry.time_stamp = time_stamp;

		ResourceIdentifier identifier(path);

		// The identifier needs to be allocated, account for the L'\0'
		void* allocation = m_memory->Allocate(identifier.size + sizeof(wchar_t), 2);
		memcpy(allocation, identifier.ptr, identifier.size + sizeof(wchar_t));
		ECS_ASSERT(!m_resource_types[(unsigned int)resource_type].table.Insert(entry, {allocation, identifier.size}));
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void* ResourceManager::Allocate(size_t size)
	{
		return m_memory->Allocate(size);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void* ResourceManager::AllocateTs(size_t size)
	{
		return m_memory->Allocate_ts(size);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic ResourceManager::Allocator()
	{
		return GetAllocatorPolymorphic(m_memory);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::Deallocate(const void* data)
	{
		m_memory->Deallocate(data);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::DeallocateTs(const void* data)
	{
		m_memory->Deallocate_ts(data);
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
				ResourceManagerEntry* ptr = value_stream.buffer + index;
				unsigned short value = ptr->data_pointer.GetData();

				if (value != USHORT_MAX) {
					unsigned short count = ptr->data_pointer.DecrementData(amount);
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
		ECS_ASSERT(m_thread_resources[thread_index].offsets.size > 0);
		m_thread_resources[thread_index].offsets.size--;
		m_thread_resources[thread_index].characters.size = m_thread_resources[thread_index].offsets[m_thread_resources[thread_index].offsets.size];
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::Exists(ResourceIdentifier identifier, ResourceType type) const
	{
		int hashed_position = m_resource_types[(unsigned int)type].table.Find<true>(identifier);
		return hashed_position != -1;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::Exists(ResourceIdentifier identifier, ResourceType type, unsigned int& table_index) const {
		table_index = m_resource_types[(unsigned int)type].table.Find<true>(identifier);
		return table_index != -1;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::EvictOutdatedResources(ResourceType type)
	{
		unsigned int int_type = (unsigned int)type;
		const ResourceIdentifier* identifiers = m_resource_types[int_type].table.GetIdentifiers();
		int table_capacity = m_resource_types[int_type].table.GetExtendedCapacity();

		// Iterate through all resources and check their new stamps
		for (int index = 0; index < table_capacity; index++) {
			if (m_resource_types[int_type].table.IsItemAt(index)) {
				// Get the new stamp
				size_t new_stamp = OS::GetFileLastWrite((const wchar_t*)identifiers[index].ptr);
				if (new_stamp > m_resource_types[int_type].table.GetValueFromIndex(index).time_stamp) {
					// Kick this resource
					DELETE_FUNCTIONS[int_type](this, index, 1);
					// Decrement the index because other resources can move into this place
					index--;
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	int ResourceManager::GetResourceIndex(ResourceIdentifier identifier, ResourceType type) const
	{
		int index = m_resource_types[(unsigned int)type].table.Find<true>(identifier);

		ECS_ASSERT(index != -1, "The resource was not found");
		return index;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void* ResourceManager::GetResource(ResourceIdentifier identifier, ResourceType type)
	{
		int hashed_position = m_resource_types[(unsigned int)type].table.Find<true>(identifier);
		if (hashed_position != -1) {
			return m_resource_types[(unsigned int)type].table.GetValueFromIndex(hashed_position).data_pointer.GetPointer();
		}
		ECS_ASSERT(false, "The resource was not found");
		return nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	size_t ResourceManager::GetThreadPath(wchar_t* characters, unsigned int thread_index) const
	{
		if (m_thread_resources[thread_index].offsets.size > 0) {
			m_thread_resources[thread_index].characters.CopyTo(characters);
			return m_thread_resources[thread_index].characters.size;
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

	// ---------------------------------------------------------------------------------------------------------------------------

	Stream<wchar_t> ResourceManager::GetThreadPath(wchar_t* characters, const char* current_path, unsigned int thread_index) const
	{
		size_t current_path_size = strlen(current_path);
		size_t offset = GetThreadPath(characters, thread_index);
		function::ConvertASCIIToWide(characters + offset, { current_path, current_path_size }, 512);
		characters[offset + current_path_size] = L'\0';
		return { characters, offset + current_path_size };
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	size_t ResourceManager::GetTimeStamp(ResourceIdentifier identifier, ResourceType type) const
	{
		ResourceManagerEntry entry;
		if (m_resource_types[(unsigned int)type].table.TryGetValue(identifier, entry)) {
			return entry.time_stamp;
		}
		return -1;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceManagerEntry ResourceManager::GetEntry(ResourceIdentifier identifier, ResourceType type) const
	{
		ResourceManagerEntry entry;
		if (m_resource_types[(unsigned int)type].table.TryGetValue(identifier, entry)) {
			return entry;
		}
		return { DataPointer(nullptr), (size_t)-1 };
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceManagerEntry* ResourceManager::GetEntryPtr(ResourceIdentifier identifier, ResourceType type)
	{
		ResourceManagerEntry* entry;
		if (m_resource_types[(unsigned int)type].table.TryGetValuePtr(identifier, entry)) {
			return entry;
		}
		return nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::IsResourceOutdated(ResourceIdentifier identifier, ResourceType type, size_t new_stamp)
	{
		ResourceManagerEntry entry = GetEntry(identifier, type);
		// If the entry exists, do the compare
		if (entry.time_stamp != -1) {
			return entry.time_stamp < new_stamp;
		}
		return false;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::InitializeDefaultTypes() {
		for (size_t index = 0; index < (unsigned int)ResourceType::TypeCount; index++) {
			AddResourceType((ResourceType)index, ECS_RESOURCE_MANAGER_DEFAULT_RESOURCE_COUNT);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::IncrementReferenceCount(ResourceType type, unsigned int amount)
	{
		unsigned int type_int = (unsigned int)type;
		auto value_stream = m_resource_types[type_int].table.GetValueStream();
		for (size_t index = 0; index < value_stream.size; index++) {
			if (m_resource_types[type_int].table.IsItemAt(index)) {
				ResourceManagerEntry* ptr = value_stream.buffer + index;
				unsigned short count = ptr->data_pointer.IncrementData(amount);
				if (count == USHORT_MAX) {
					ptr->data_pointer.SetData(USHORT_MAX - 1);
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	char* ResourceManager::LoadTextFile(
		const wchar_t* filename,
		size_t* size, 
		ResourceManagerLoadDesc load_descriptor
	)
	{
		return (char*)LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::TextFile, 
			load_descriptor,
			[=]() {
				ECS_STACK_CAPACITY_STREAM(wchar_t, path, 512);
				GetThreadPath(path.buffer, filename, load_descriptor.thread_index);
				return LoadTextFileImplementation(path.buffer, size);
			}
		);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(char*, ResourceManager::LoadTextFile, const wchar_t*, size_t*, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	char* ResourceManager::LoadTextFileImplementation(const wchar_t* file, size_t* parameter)
	{
		Stream<char> contents = ReadWholeFileText(file, Allocator());
		if (contents.buffer != nullptr) {
			*parameter = contents.size;
			return contents.buffer;
		}
		return nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::OpenFile(const wchar_t* filename, ECS_FILE_HANDLE* input, bool binary, unsigned int thread_index)
	{
		ECS_TEMP_STRING(path, 512);
		Stream<wchar_t> current_path = GetThreadPath(path.buffer, filename, thread_index);
		ECS_FILE_ACCESS_FLAGS access_flags = binary ? ECS_FILE_ACCESS_BINARY : ECS_FILE_ACCESS_TEXT;
		access_flags |= ECS_FILE_ACCESS_READ_ONLY;
		ECS_FILE_STATUS_FLAGS status = ECSEngine::OpenFile(filename, input, access_flags);

		return status == ECS_FILE_STATUS_OK;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	ResourceView ResourceManager::LoadTexture(
		const wchar_t* filename,
		ResourceManagerTextureDesc* descriptor,
		ResourceManagerLoadDesc load_descriptor
	)
	{
		void* texture_view = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::Texture, 
			load_descriptor,
			[=]() {
			return LoadTextureImplementation(filename, descriptor, load_descriptor).view;
		});

		return (ID3D11ShaderResourceView*)texture_view;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(ResourceView, ResourceManager::LoadTexture, const wchar_t*, ResourceManagerTextureDesc*, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceView ResourceManager::LoadTextureImplementation(
		const wchar_t* filename,
		ResourceManagerTextureDesc* descriptor,
		ResourceManagerLoadDesc load_descriptor
	)
	{
		ID3D11ShaderResourceView* texture_view = nullptr;
		ID3D11Resource* resource = nullptr;

		// Determine the texture extension - HDR textures must take a different path
		Stream<wchar_t> texture_path = ToStream(filename);
		Path extension = function::PathExtensionBoth(texture_path);
		ECS_ASSERT(extension.size > 0, "No extension could be identified");

		bool is_hdr_texture = function::CompareStrings(extension, ToStream(L".hdr"));
		bool is_tga_texture = function::CompareStrings(extension, ToStream(L".tga"));

		ECS_TEMP_STRING(_path, 512);
		Stream<wchar_t> path = GetThreadPath(_path.buffer, filename, load_descriptor.thread_index);
		HRESULT result;
		
		AllocateFunction allocate_function = DirectX::ECSDefaultAllocation;
		DeallocateMutableFunction deallocate_function = DirectX::ECSDefaultDeallocation;
		void* allocator = nullptr;
		if (descriptor->allocator.allocator != nullptr) {
			allocator = descriptor->allocator.allocator;
			allocate_function = GetAllocateFunction(descriptor->allocator);
			deallocate_function = GetDeallocateMutableFunction(descriptor->allocator);
		}

		Texture2D texture;
		bool is_compression = (unsigned char)descriptor->compression != (unsigned char)-1;
		bool temporary = function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY);
		temporary &= is_compression;

		DirectX::ScratchImage image;
		image.SetAllocator(allocator, allocate_function, deallocate_function);
		if (is_hdr_texture) {
			result = DirectX::LoadFromHDRFile(path.buffer, nullptr, image);
			//// Convert to a DX11 resource
			//GraphicsTexture2DDescriptor dx_descriptor;
			//const auto& metadata = image.GetMetadata();
			//dx_descriptor.format = metadata.format;
			//dx_descriptor.array_size = 1;
			//dx_descriptor.bind_flag = (D3D11_BIND_FLAG)descriptor->bindFlags;
			//dx_descriptor.cpu_flag = (D3D11_CPU_ACCESS_FLAG)descriptor->cpuAccessFlags;
			//Stream<void> image_data = { image.GetPixels(), image.GetPixelsSize() };
			//dx_descriptor.mip_data = { &image_data, 1 };
			//
			//dx_descriptor.misc_flag = descriptor->miscFlags;
			//dx_descriptor.usage = descriptor->usage;
			//dx_descriptor.size = { (unsigned int)metadata.width, (unsigned int)metadata.height };

			//// No context provided - don't generate mips
			//if (descriptor->context == nullptr) {
			//	dx_descriptor.mip_levels = 1;
			//	texture = m_graphics->CreateTexture(&dx_descriptor, temporary);
			//	texture_view = m_graphics->CreateTextureShaderViewResource(texture, true).view;
			//}
			//else {
			//	ECS_ASSERT(descriptor->context == m_graphics->GetContext());

			//	// Make the initial data nullptr
			//	dx_descriptor.mip_levels = 0;
			//	dx_descriptor.mip_data = { nullptr,0 };
			//	texture = m_graphics->CreateTexture(&dx_descriptor, temporary);

			//	// Update the mip 0
			//	UpdateTextureResource(texture, image.GetPixels(), image.GetPixelsSize(), descriptor->context);

			//	ResourceView view = m_graphics->CreateTextureShaderViewResource(texture, true);

			//	// Generate mips
			//	m_graphics->GenerateMips(view);
			//	texture_view = view.view;
			//}
		}
		else if (is_tga_texture) {
			result = DirectX::LoadFromTGAFile(path.buffer, nullptr, image);
			//// Convert to a DX11 resource
			//GraphicsTexture2DDescriptor dx_descriptor;
			//auto metadata = image.GetMetadata();
			//dx_descriptor.format = metadata.format;
			//dx_descriptor.array_size = 1;
			//dx_descriptor.bind_flag = (D3D11_BIND_FLAG)descriptor->bindFlags;
			//dx_descriptor.cpu_flag = (D3D11_CPU_ACCESS_FLAG)descriptor->cpuAccessFlags;

			//Stream<void> image_data = { image.GetPixels(), image.GetPixelsSize() };
			//dx_descriptor.mip_data = { &image_data, 1 };
			//dx_descriptor.misc_flag = descriptor->miscFlags;
			//dx_descriptor.usage = descriptor->usage;
			//dx_descriptor.size = { (unsigned int)metadata.width, (unsigned int)metadata.height };

			//// No context provided - don't generate mips
			//if (descriptor->context == nullptr) {
			//	dx_descriptor.mip_levels = 1;
			//	texture = m_graphics->CreateTexture(&dx_descriptor, temporary);
			//	texture_view = m_graphics->CreateTextureShaderViewResource(texture, true).view;
			//}
			//else {
			//	ECS_ASSERT(descriptor->context == m_graphics->GetContext());

			//	// Make the initial data nullptr
			//	dx_descriptor.mip_levels = 0;
			//	dx_descriptor.mip_data = { nullptr, 0 };
			//	texture = m_graphics->CreateTexture(&dx_descriptor, temporary);

			//	// Update the mip 0
			//	UpdateTextureResource(texture, image.GetPixels(), image.GetPixelsSize(), descriptor->context);

			//	ResourceView view = m_graphics->CreateTextureShaderViewResource(texture, true);

			//	// Generate mips
			//	m_graphics->GenerateMips(view);
			//	texture_view = view.view;
			//}
		}
		else {
			result = DirectX::LoadFromWICFile(path.buffer, DirectX::WIC_FLAGS_FORCE_RGB | DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image);
		}
		if (FAILED(result)) {
			return nullptr;
		}

		descriptor->misc_flags |= function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_SHARED_RESOURCE) ? D3D11_RESOURCE_MISC_SHARED : 0;

		DXTexCreateTextureEx(
			m_graphics->GetDevice(),
			&image,
			descriptor->bind_flags,
			descriptor->usage,
			descriptor->misc_flags,
			descriptor->cpu_flags,
			&texture.tex,
			&texture_view,
			descriptor->context
		);

		if (is_compression) {
			// Release the old view
			unsigned int count = texture_view->Release();
			bool success = CompressTexture(m_graphics, texture, descriptor->compression);
			if (!success) {
				return nullptr;
			}

			texture_view = m_graphics->CreateTextureShaderViewResource(texture, true).view;
		}

		if (!temporary) {
			m_graphics->AddInternalResource(ResourceView(texture_view));
			m_graphics->AddInternalResource(texture);
		}

		return texture_view;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes from a gltf file
	template<bool reference_counted>
	Stream<Mesh>* ResourceManager::LoadMeshes(
		const wchar_t* filename,
		ResourceManagerLoadDesc load_descriptor
	) {
		void* meshes = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::Mesh,
			load_descriptor,
			[=]() {
				return LoadMeshImplementation(filename, load_descriptor);
			});

		return (Stream<Mesh>*)meshes;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Stream<Mesh>*, ResourceManager::LoadMeshes, const wchar_t*, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes from a gltf file
	Stream<Mesh>* ResourceManager::LoadMeshImplementation(const wchar_t* filename, ResourceManagerLoadDesc load_descriptor) {
		ECS_TEMP_STRING(_path, 512);
		Stream<wchar_t> path = GetThreadPath(_path.buffer, filename, load_descriptor.thread_index);

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

		bool has_invert = !function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT);
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

		unsigned int misc_flags = function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_SHARED_RESOURCE) ? D3D11_RESOURCE_MISC_SHARED : 0;
		// Convert the gltf meshes into multiple meshes
		GLTFMeshesToMeshes(m_graphics, gltf_meshes, meshes->buffer, meshes->size, misc_flags);

		// Free the gltf file data and the gltf meshes
		FreeGLTFFile(data);
		FreeGLTFMeshes(gltf_meshes, data.mesh_count, allocator);
		return meshes;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	CoallescedMesh* ResourceManager::LoadCoallescedMesh(const wchar_t* filename, ResourceManagerLoadDesc load_descriptor)
	{
		void* meshes = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::CoallescedMesh,
			load_descriptor,
			[=]() {
				return LoadCoallescedMeshImplementation(filename, load_descriptor);
			});

		return (CoallescedMesh*)meshes;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(CoallescedMesh*, ResourceManager::LoadCoallescedMesh, const wchar_t*, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	CoallescedMesh* ResourceManager::LoadCoallescedMeshImplementation(const wchar_t* filename, ResourceManagerLoadDesc load_descriptor)
	{
		ECS_TEMP_STRING(_path, 512);
		Stream<wchar_t> path = GetThreadPath(_path.buffer, filename, load_descriptor.thread_index);

		GLTFData data = LoadGLTFFile(path);
		// The load failed
		if (data.data == nullptr || data.mesh_count == 0) {
			return nullptr;
		}
		ECS_ASSERT(data.mesh_count < 200);

		GLTFMesh* gltf_meshes = (GLTFMesh*)ECS_STACK_ALLOC(sizeof(GLTFMesh) * data.mesh_count);
		// Nullptr and 0 everything
		memset(gltf_meshes, 0, sizeof(GLTFMesh) * data.mesh_count);
		AllocatorPolymorphic allocator = Allocator();

		// Determine how much memory do the buffers take. If they fit the under a small amount, use the normal allocator.
		// Else use the global allocator
		if (data.data->bin_size > ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_BACKUP_SIZE || data.data->json_size > ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_BACKUP_SIZE) {
			allocator = GetAllocatorPolymorphic(m_memory->m_backup);
		}

		bool has_invert = !function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT);
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
		
		unsigned int misc_flags = function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_SHARED_RESOURCE) ? D3D11_RESOURCE_MISC_SHARED : 0;
		// Convert the gltf meshes into multiple meshes and then convert these to an aggregated mesh
		GLTFMeshesToMeshes(m_graphics, gltf_meshes, temporary_meshes, data.mesh_count);

		// Convert now to aggregated mesh
		mesh->mesh = MeshesToSubmeshes(m_graphics, { temporary_meshes, data.mesh_count }, mesh->submeshes.buffer, misc_flags);

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
		ResourceManagerLoadDesc load_descriptor
	) {
		void* materials = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::Material,
			load_descriptor,
			[=]() {
				return LoadMaterialsImplementation(filename, load_descriptor);
			});

		return (Stream<PBRMaterial>*)materials;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Stream<PBRMaterial>*, ResourceManager::LoadMaterials, const wchar_t*, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all materials from a gltf file
	Stream<PBRMaterial>* ResourceManager::LoadMaterialsImplementation(const wchar_t* filename, ResourceManagerLoadDesc load_descriptor) {
		ECS_TEMP_STRING(_path, 512);
		Stream<wchar_t> path = GetThreadPath(_path.buffer, filename, load_descriptor.thread_index);

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
		ResourceManagerLoadDesc load_descriptor
	) {
		void* mesh = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::PBRMesh,
			load_descriptor,
			[=]() {
				return LoadPBRMeshImplementation(filename, load_descriptor);
		});

		return (PBRMesh*)mesh;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(PBRMesh*, ResourceManager::LoadPBRMesh, const wchar_t*, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes and materials from a gltf file, combines the meshes into a single one sorted by material submeshes
	PBRMesh* ResourceManager::LoadPBRMeshImplementation(const wchar_t* filename, ResourceManagerLoadDesc load_descriptor) {
		ECS_TEMP_STRING(_path, 512);
		Stream<wchar_t> path = GetThreadPath(_path.buffer, filename, load_descriptor.thread_index);

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
		// Make every stream 0 for the gltf meshes
		memset(gltf_meshes, 0, sizeof(GLTFMesh) * data.mesh_count);

		bool has_invert = !function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT);
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

		unsigned int misc_flags = function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_SHARED_RESOURCE) ? D3D11_RESOURCE_MISC_SHARED : 0;
		mesh->mesh.mesh = GLTFMeshesToMergedMesh(m_graphics, gltf_meshes, mesh->mesh.submeshes.buffer, material_masks, pbr_materials.size, data.mesh_count, misc_flags);
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

	template<typename Shader, typename ByteCodeHandler, typename SourceHandler>
	Shader LoadShaderImplementation(ResourceManager* manager, const wchar_t* filename, ShaderCompileOptions options, ResourceManagerLoadDesc load_descriptor,
		Stream<char>* shader_source_code, ByteCodeHandler&& byte_code_handler, SourceHandler&& source_handler) {
		Shader shader;

		ECS_TEMP_STRING(path, 512);
		Stream<wchar_t> stream_filename = manager->GetThreadPath(path.buffer, filename, load_descriptor.thread_index);

		Path path_extension = function::PathExtensionBoth(stream_filename);
		bool is_byte_code = function::CompareStrings(path_extension, ToStream(L".cso"));

		Stream<void> contents = { nullptr, 0 };
		AllocatorPolymorphic allocator_polymorphic = GetAllocatorPolymorphic(manager->m_memory, AllocationType::MultiThreaded);

		if (is_byte_code) {
			contents = ReadWholeFileBinary(stream_filename.buffer, allocator_polymorphic);
			shader = byte_code_handler(contents);
		}
		else {
			Stream<char> source_code = ReadWholeFileText(stream_filename.buffer, allocator_polymorphic);
			shader = source_handler(source_code, options);
			contents = source_code;
		}

		if (shader_source_code == nullptr) {
			manager->m_memory->Deallocate_ts(contents.buffer);
		}
		else {
			*shader_source_code = Stream<char>(contents.buffer, contents.size);
		}

		return shader;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	VertexShader* ResourceManager::LoadVertexShader(
		const wchar_t* filename, 
		Stream<char>* source_code, 
		ShaderCompileOptions options,
		ResourceManagerLoadDesc load_descriptor
	)
	{
		void* shader = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::VertexShader,
			sizeof(VertexShader),
			load_descriptor,
			[=](void* allocation) {
				VertexShader* shader = (VertexShader*)allocation;
				*shader = LoadVertexShaderImplementation(filename, source_code, options, load_descriptor);
				return allocation;
			});

		return (VertexShader*)shader;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(VertexShader*, ResourceManager::LoadVertexShader, const wchar_t*, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	VertexShader ResourceManager::LoadVertexShaderImplementation(
		const wchar_t* filename, 
		Stream<char>* shader_source_code, 
		ShaderCompileOptions options, 
		ResourceManagerLoadDesc load_descriptor
	)
	{
		return LoadShaderImplementation<VertexShader>(this, filename, options, load_descriptor, shader_source_code, [&](Stream<void> byte_code) {
			return m_graphics->CreateVertexShader(byte_code, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY));
		},
		[&](Stream<char> source_code, ShaderCompileOptions options) {
			ShaderIncludeFiles include(m_memory, m_shader_directory);
			VertexShader shader = m_graphics->CreateVertexShaderFromSource(
				source_code,
				&include,
				function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY),
				options
		);
			return shader;
		});
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	PixelShader* ResourceManager::LoadPixelShader(
		const wchar_t* filename, 
		Stream<char>* shader_source_code,
		ShaderCompileOptions options, 
		ResourceManagerLoadDesc load_descriptor
	)
	{
		void* shader = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::PixelShader,
			sizeof(PixelShader),
			load_descriptor,
			[=](void* allocation) {
				PixelShader* shader = (PixelShader*)allocation;
				*shader = LoadPixelShaderImplementation(filename, shader_source_code, options, load_descriptor);
				return allocation;
			});

		return (PixelShader*)shader;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(PixelShader*, ResourceManager::LoadPixelShader, const wchar_t*, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	PixelShader ResourceManager::LoadPixelShaderImplementation(
		const wchar_t* filename, 
		Stream<char>* shader_source_code,
		ShaderCompileOptions options, 
		ResourceManagerLoadDesc load_descriptor
	)
	{
		return LoadShaderImplementation<PixelShader>(this, filename, options, load_descriptor, shader_source_code, [&](Stream<void> byte_code) {
			return m_graphics->CreatePixelShader(byte_code, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY));
			},
			[&](Stream<char> source_code, ShaderCompileOptions options) {
				ShaderIncludeFiles include(m_memory, m_shader_directory);
				return m_graphics->CreatePixelShaderFromSource(source_code, &include, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY), options);
		});
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	HullShader* ResourceManager::LoadHullShader(
		const wchar_t* filename,
		Stream<char>* shader_source_code, 
		ShaderCompileOptions options, 
		ResourceManagerLoadDesc load_descriptor
	)
	{
		void* shader = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::HullShader,
			sizeof(HullShader),
			load_descriptor,
			[=](void* allocation) {
				HullShader* shader = (HullShader*)allocation;
				*shader = LoadHullShaderImplementation(filename, shader_source_code, options, load_descriptor);
				return allocation;
			});

		return (HullShader*)shader;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(HullShader*, ResourceManager::LoadHullShader, const wchar_t*, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	HullShader ResourceManager::LoadHullShaderImplementation(
		const wchar_t* filename,
		Stream<char>* shader_source_code, 
		ShaderCompileOptions options, 
		ResourceManagerLoadDesc load_descriptor
	)
	{
		return LoadShaderImplementation<HullShader>(this, filename, options, load_descriptor, shader_source_code, [&](Stream<void> byte_code) {
			return m_graphics->CreateHullShader(byte_code, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY));
			},
			[&](Stream<char> source_code, ShaderCompileOptions options) {
				ShaderIncludeFiles include(m_memory, m_shader_directory);
				return m_graphics->CreateHullShaderFromSource(source_code, &include, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY), options);
			});
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	DomainShader* ResourceManager::LoadDomainShader(
		const wchar_t* filename, 
		Stream<char>* shader_source_code,
		ShaderCompileOptions options, 
		ResourceManagerLoadDesc load_descriptor
	)
	{
		void* shader = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::DomainShader,
			sizeof(DomainShader),
			load_descriptor,
			[=](void* allocation) {
				DomainShader* shader = (DomainShader*)allocation;
				*shader = LoadDomainShaderImplementation(filename, shader_source_code, options, load_descriptor);
				return allocation;
			});

		return (DomainShader*)shader;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(DomainShader*, ResourceManager::LoadDomainShader, const wchar_t*, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	DomainShader ResourceManager::LoadDomainShaderImplementation(
		const wchar_t* filename, 
		Stream<char>* shader_source_code,
		ShaderCompileOptions options, 
		ResourceManagerLoadDesc load_descriptor
	)
	{
		return LoadShaderImplementation<DomainShader>(this, filename, options, load_descriptor, shader_source_code, [&](Stream<void> byte_code) {
			return m_graphics->CreateDomainShader(byte_code, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY));
			},
			[&](Stream<char> source_code, ShaderCompileOptions options) {
				ShaderIncludeFiles include(m_memory, m_shader_directory);
				return m_graphics->CreateDomainShaderFromSource(source_code, &include, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY), options);
			});
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	GeometryShader* ResourceManager::LoadGeometryShader(
		const wchar_t* filename, 
		Stream<char>* shader_source_code,
		ShaderCompileOptions options,
		ResourceManagerLoadDesc load_descriptor
	)
	{
		void* shader = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::GeometryShader,
			sizeof(GeometryShader),
			load_descriptor,
			[=](void* allocation) {
				GeometryShader* shader = (GeometryShader*)allocation;
				*shader = LoadGeometryShaderImplementation(filename, shader_source_code, options, load_descriptor);
				return allocation;
			});

		return (GeometryShader*)shader;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(GeometryShader*, ResourceManager::LoadGeometryShader, const wchar_t*, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	GeometryShader ResourceManager::LoadGeometryShaderImplementation(
		const wchar_t* filename,
		Stream<char>* shader_source_code,
		ShaderCompileOptions options,
		ResourceManagerLoadDesc load_descriptor
	)
	{
		return LoadShaderImplementation<GeometryShader>(this, filename, options, load_descriptor, shader_source_code, [&](Stream<void> byte_code) {
			return m_graphics->CreateGeometryShader(byte_code, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY));
			},
			[&](Stream<char> source_code, ShaderCompileOptions options) {
				ShaderIncludeFiles include(m_memory, m_shader_directory);
				return m_graphics->CreateGeometryShaderFromSource(source_code, &include, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY), options);
			});
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	ComputeShader* ResourceManager::LoadComputeShader(
		const wchar_t* filename,
		Stream<char>* shader_source_code,
		ShaderCompileOptions options,
		ResourceManagerLoadDesc load_descriptor
	)
	{
		void* shader = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::ComputeShader,
			sizeof(ComputeShader),
			load_descriptor,
			[=](void* allocation) {
				ComputeShader* shader = (ComputeShader*)allocation;
				*shader = LoadComputeShaderImplementation(filename, shader_source_code, options, load_descriptor);
				return allocation;
			});

		return (ComputeShader*)shader;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(ComputeShader*, ResourceManager::LoadComputeShader, const wchar_t*, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	ComputeShader ResourceManager::LoadComputeShaderImplementation(
		const wchar_t* filename, 
		Stream<char>* shader_source_code,
		ShaderCompileOptions options,
		ResourceManagerLoadDesc load_descriptor
	)
	{
		return LoadShaderImplementation<ComputeShader>(this, filename, options, load_descriptor, shader_source_code, [&](Stream<void> byte_code) {
			return m_graphics->CreateComputeShader(byte_code, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY));
			},
			[&](Stream<char> source_code, ShaderCompileOptions options) {
				ShaderIncludeFiles include(m_memory, m_shader_directory);
				return m_graphics->CreateComputeShaderFromSource(source_code, &include, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY), options);
			});
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::RebindResource(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource)
	{
		unsigned int int_type = (unsigned int)resource_type;

		ResourceManagerEntry* entry;
		bool success = m_resource_types[int_type].table.TryGetValuePtr(identifier, entry);
		ECS_ASSERT(success, "Could not rebind resource");

		UNLOAD_FUNCTIONS[int_type](entry->data_pointer.GetPointer(), this);

		//if (success) {
		entry->data_pointer.SetPointer(new_resource);
		//}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::RebindResourceNoDestruction(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource)
	{
		unsigned int int_type = (unsigned int)resource_type;
		ResourceManagerEntry* entry;
		bool success = m_resource_types[int_type].table.TryGetValuePtr(identifier, entry);

		ECS_ASSERT(success, "Could not rebind resource");
		//if (success) {
		entry->data_pointer.SetPointer(new_resource);
		//}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::RemoveReferenceCountForResource(ResourceIdentifier identifier, ResourceType resource_type)
	{
		unsigned int type_index = (unsigned int)resource_type;
		ResourceManagerEntry* entry;
		bool success = m_resource_types[type_index].table.TryGetValuePtr(identifier, entry);

		ECS_ASSERT(success, "Could not remove reference count for resource");
		entry->data_pointer.SetData(USHORT_MAX);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::SetShaderDirectory(Stream<wchar_t> directory)
	{
		if (m_shader_directory.buffer != nullptr && m_shader_directory.size > 0) {
			m_memory->Deallocate(m_shader_directory.buffer);
		}

		m_shader_directory.buffer = function::StringCopy(m_memory, directory).buffer;
		m_shader_directory.size = directory.size;
		m_shader_directory.AssertCapacity();
	}

	// ---------------------------------------------------------------------------------------------------------------------------

#define EXPORT_UNLOAD(name) ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::name, const wchar_t*, size_t); ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::name, unsigned int, size_t);

	template<bool reference_counted>
	void ResourceManager::UnloadTextFile(const wchar_t* filename, size_t flags)
	{
		UnloadResource<reference_counted>(filename, ResourceType::TextFile, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadTextFile(unsigned int index, size_t flags) {
		UnloadResource<reference_counted>(index, ResourceType::TextFile, flags);
	}

	void ResourceManager::UnloadTextFileImplementation(char* data, size_t flags)
	{
		UnloadTextFileHandler(data, this);
	}

	EXPORT_UNLOAD(UnloadTextFile);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadTexture(const wchar_t* filename, size_t flags)
	{
		UnloadResource<reference_counted>(filename, ResourceType::Texture, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadTexture(unsigned int index, size_t flags) {
		UnloadResource<reference_counted>(index, ResourceType::Texture, flags);
	}

	void ResourceManager::UnloadTextureImplementation(ResourceView texture, size_t flags)
	{
		UnloadTextureHandler(texture.view, this);
	}

	EXPORT_UNLOAD(UnloadTexture);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadMeshes(const wchar_t* filename, size_t flags) {
		UnloadResource<reference_counted>(filename, ResourceType::Mesh, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadMeshes(unsigned int index, size_t flags) {;
		UnloadResource<reference_counted>(index, ResourceType::Mesh, flags);
	}

	void ResourceManager::UnloadMeshesImplementation(Stream<Mesh>* meshes, size_t flags)
	{
		UnloadMeshesHandler(meshes, this);
	}

	EXPORT_UNLOAD(UnloadMeshes);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadCoallescedMesh(const wchar_t* filename, size_t flags)
	{
		UnloadResource<reference_counted>(filename, ResourceType::CoallescedMesh, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadCoallescedMesh(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::CoallescedMesh, flags);
	}

	void ResourceManager::UnloadCoallescedMeshImplementation(CoallescedMesh* mesh, size_t flags)
	{
		UnloadCoallescedMeshHandler(mesh, this);
	}

	EXPORT_UNLOAD(UnloadCoallescedMesh);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadMaterials(const wchar_t* filename, size_t flags) {
		UnloadResource<reference_counted>(filename, ResourceType::Material, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadMaterials(unsigned int index, size_t flags) {
		UnloadResource<reference_counted>(index, ResourceType::Material, flags);
	}

	void ResourceManager::UnloadMaterialsImplementation(Stream<PBRMaterial>* materials, size_t flags)
	{
		UnloadMaterialsHandler(materials, this);
	}

	EXPORT_UNLOAD(UnloadMaterials);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadPBRMesh(const wchar_t* filename, size_t flags) {
		UnloadResource<reference_counted>(filename, ResourceType::PBRMesh, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadPBRMesh(unsigned int index, size_t flags) {
		UnloadResource<reference_counted>(index, ResourceType::PBRMesh, flags);
	}

	void ResourceManager::UnloadPBRMeshImplementation(PBRMesh* mesh, size_t flags)
	{
		UnloadPBRMeshHandler(mesh, this);
	}

	EXPORT_UNLOAD(UnloadPBRMesh);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadVertexShader(const wchar_t* filename, size_t flags)
	{
		UnloadResource<reference_counted>(filename, ResourceType::VertexShader, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadVertexShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::VertexShader, flags);
	}

	void ResourceManager::UnloadVertexShaderImplementation(VertexShader vertex_shader, size_t flags)
	{
		UnloadVertexShaderHandler(&vertex_shader, this);
	}

	EXPORT_UNLOAD(UnloadVertexShader);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadPixelShader(const wchar_t* filename, size_t flags)
	{
		UnloadResource<reference_counted>(filename, ResourceType::PixelShader, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadPixelShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::PixelShader, flags);
	}

	void ResourceManager::UnloadPixelShaderImplementation(PixelShader pixel_shader, size_t flags)
	{
		UnloadShaderHandler<PixelShader>(&pixel_shader, this);
	}

	EXPORT_UNLOAD(UnloadPixelShader);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadHullShader(const wchar_t* filename, size_t flags)
	{
		UnloadResource<reference_counted>(filename, ResourceType::HullShader, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadHullShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::HullShader, flags);
	}

	void ResourceManager::UnloadHullShaderImplementation(HullShader hull_shader, size_t flags)
	{
		UnloadShaderHandler<HullShader>(&hull_shader, this);
	}

	EXPORT_UNLOAD(UnloadHullShader);
	
	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadDomainShader(const wchar_t* filename, size_t flags)
	{
		UnloadResource<reference_counted>(filename, ResourceType::DomainShader, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadDomainShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::DomainShader, flags);
	}

	void ResourceManager::UnloadDomainShaderImplementation(DomainShader domain_shader, size_t flags)
	{
		UnloadShaderHandler<DomainShader>(&domain_shader, this);
	}

	EXPORT_UNLOAD(UnloadDomainShader);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadGeometryShader(const wchar_t* filename, size_t flags)
	{
		UnloadResource<reference_counted>(filename, ResourceType::GeometryShader, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadGeometryShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::GeometryShader, flags);
	}

	void ResourceManager::UnloadGeometryShaderImplementation(GeometryShader geometry_shader, size_t flags)
	{
		UnloadShaderHandler<GeometryShader>(&geometry_shader, this);
	}

	EXPORT_UNLOAD(UnloadGeometryShader);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadComputeShader(const wchar_t* filename, size_t flags)
	{
		UnloadResource<reference_counted>(filename, ResourceType::ComputeShader, flags);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadComputeShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::ComputeShader, flags);
	}

	void ResourceManager::UnloadComputeShaderImplementation(ComputeShader compute_shader, size_t flags)
	{
		UnloadShaderHandler<ComputeShader>(&compute_shader, this);
	}

	EXPORT_UNLOAD(UnloadComputeShader);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadResource(const wchar_t* filename, ResourceType type, size_t flags)
	{
		DeleteResource<reference_counted>(this, filename, type, flags);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadResource, const wchar_t*, ResourceType, size_t);

	template<bool reference_counted>
	void ResourceManager::UnloadResource(unsigned int index, ResourceType type, size_t flags)
	{
		DeleteResource<reference_counted>(this, index, type, flags);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadResource, unsigned int, ResourceType, size_t);

	void ResourceManager::UnloadResourceImplementation(void* resource, ResourceType type, size_t flags)
	{
		UNLOAD_FUNCTIONS[(unsigned int)type](resource, this);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::UnloadAll()
	{
		for (size_t index = 0; index < m_resource_types.size; index++) {
			// If the types exists, iterate through the table and unload those resources
			if (m_resource_types[index].name != nullptr) {
				unsigned int type_resource_count = m_resource_types[index].table.GetCount();
				if (type_resource_count > 0) {
					unsigned int table_capacity = m_resource_types[index].table.GetExtendedCapacity();
					unsigned int unload_count = 0;
					for (size_t subindex = 0; subindex < table_capacity && unload_count < type_resource_count; subindex++) {
						if (m_resource_types[index].table.IsItemAt(subindex)) {
							ResourceManagerEntry entry = m_resource_types[index].table.GetValueFromIndex(subindex);
							UnloadResourceImplementation(entry.data_pointer.GetPointer(), (ResourceType)index);

							// Helps to early exit if the resources are clumped together towards the beginning of the table
							unload_count++;
						}
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::UnloadAll(ResourceType resource_type)
	{
		unsigned int int_type = (unsigned int)resource_type;
		unsigned int count = m_resource_types[int_type].table.GetExtendedCapacity();
		for (unsigned int index = 0; index < count; index++) {
			if (m_resource_types[int_type].table.IsItemAt(index)) {
				UnloadResource(index, resource_type);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// ---------------------------------------------------------------------------------------------------------------------------

#pragma region Free functions

	// ---------------------------------------------------------------------------------------------------------------------------

	MemoryManager DefaultResourceManagerAllocator(GlobalMemoryManager* global_allocator)
	{
		return MemoryManager(1'000'000, 2048, ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_BACKUP_SIZE, global_allocator);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	Material PBRToMaterial(ResourceManager* resource_manager, const PBRMaterial& pbr, Stream<wchar_t> folder_to_search)
	{
		Material result;

		result.name = nullptr;

		ShaderMacro _shader_macros[64];
		Stream<ShaderMacro> shader_macros(_shader_macros, 0);
		ResourceView texture_views[ECS_PBR_MATERIAL_MAPPING_COUNT] = { nullptr };

		// For each texture, check to see if it exists and add the macro and load the texture
		// The textures will be missing their extension, so do a search using .jpg, .png and .tiff, .bmp
		const wchar_t* texture_extensions[] = {
			L".jpg",
			L".png",
			L".tiff",
			L".bmp"
		};

		wchar_t* memory = (wchar_t*)ECS_STACK_ALLOC(sizeof(wchar_t) * ECS_KB * 8);

		struct Mapping {
			Stream<wchar_t> texture;
			PBRMaterialTextureIndex index;
			TextureCompressionExplicit compression;
		};

		Mapping _mapping[ECS_PBR_MATERIAL_MAPPING_COUNT];
		Stream<Mapping> mappings(_mapping, 0);
		LinearAllocator temporary_allocator(memory, ECS_KB * 8);

		if (pbr.color_texture.buffer != nullptr && pbr.color_texture.size > 0) {
			shader_macros.Add({ "COLOR_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(&temporary_allocator, pbr.color_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_COLOR, TextureCompressionExplicit::ColorMap });
		}

		if (pbr.emissive_texture.buffer != nullptr && pbr.emissive_texture.size > 0) {
			shader_macros.Add({ "EMISSIVE_TEXTURE", "" });
			
			Stream<wchar_t> texture = function::StringCopy(&temporary_allocator, pbr.emissive_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_EMISSIVE, TextureCompressionExplicit::ColorMap });
		}

		if (pbr.metallic_texture.buffer != nullptr && pbr.metallic_texture.size > 0) {
			shader_macros.Add({ "METALLIC_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(&temporary_allocator, pbr.metallic_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_METALLIC, TextureCompressionExplicit::GrayscaleMap });
		}

		if (pbr.normal_texture.buffer != nullptr && pbr.normal_texture.size > 0) {
			shader_macros.Add({ "NORMAL_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(&temporary_allocator, pbr.normal_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_NORMAL, TextureCompressionExplicit::NormalMapLowQuality });
		}

		if (pbr.occlusion_texture.buffer != nullptr && pbr.occlusion_texture.size > 0) {
			shader_macros.Add({ "OCCLUSION_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(&temporary_allocator, pbr.occlusion_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_OCCLUSION, TextureCompressionExplicit::GrayscaleMap });
		}

		if (pbr.roughness_texture.buffer != nullptr && pbr.roughness_texture.size > 0) {
			shader_macros.Add({ "ROUGHNESS_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(&temporary_allocator, pbr.roughness_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_ROUGHNESS, TextureCompressionExplicit::GrayscaleMap });
		}

		struct FunctorData {
			LinearAllocator* allocator;
			Stream<Mapping>* mappings;
		};

		// Search the directory for the textures
		auto search_functor = [](const wchar_t* path, void* _data) {
			FunctorData* data = (FunctorData*)_data;

			Stream<wchar_t> stream_path = ToStream(path);
			Stream<wchar_t> filename = function::PathStem(stream_path);

			for (size_t index = 0; index < data->mappings->size; index++) {
				if (function::CompareStrings(filename, data->mappings->buffer[index].texture)) {
					Stream<wchar_t> new_texture = function::StringCopy(data->allocator, stream_path);
					data->mappings->buffer[index].texture = new_texture;
					data->mappings->Swap(index, data->mappings->size - 1);
					data->mappings->size--;
					return data->mappings->size > 0;
				}
			}
			return data->mappings->size > 0;
		};

		size_t texture_count = mappings.size;
		FunctorData functor_data = { &temporary_allocator, &mappings };
		ForEachFileInDirectoryRecursiveWithExtension(folder_to_search, { texture_extensions, std::size(texture_extensions) }, &functor_data, search_functor);
		ECS_ASSERT(mappings.size == 0);
		if (mappings.size != 0) {
			return result;
		}

		GraphicsContext* context = resource_manager->m_graphics->GetContext();

		ResourceManagerTextureDesc texture_descriptor;
		texture_descriptor.context = context;
		texture_descriptor.usage = D3D11_USAGE_DEFAULT;

		for (size_t index = 0; index < texture_count; index++) {
			texture_descriptor.compression = mappings[index].compression;
			texture_views[mappings[index].index] = resource_manager->LoadTextureImplementation(mappings[index].texture.buffer, &texture_descriptor);
		}

		// Compile the shaders and reflect their buffers and structures
		ShaderCompileOptions options;
		options.macros = shader_macros;

		ECS_MESH_INDEX _vertex_mappings[8];
		CapacityStream<ECS_MESH_INDEX> vertex_mappings(_vertex_mappings, 0, 8);

		Stream<char> shader_source;
		result.vertex_shader = resource_manager->LoadVertexShaderImplementation(ECS_VERTEX_SHADER_SOURCE(PBR), &shader_source);
		ECS_ASSERT(result.vertex_shader.shader != nullptr);
		result.layout = resource_manager->m_graphics->ReflectVertexShaderInput(result.vertex_shader, shader_source);
		bool success = resource_manager->m_graphics->ReflectVertexBufferMapping(shader_source, vertex_mappings);
		ECS_ASSERT(success);
		memcpy(result.vertex_buffer_mappings, _vertex_mappings, vertex_mappings.size * sizeof(ECS_MESH_INDEX));
		result.vertex_buffer_mapping_count = vertex_mappings.size;
		resource_manager->Deallocate(shader_source.buffer);

		result.pixel_shader = resource_manager->LoadPixelShaderImplementation(ECS_PIXEL_SHADER_SOURCE(PBR), nullptr, options);
		ECS_ASSERT(result.pixel_shader.shader != nullptr);

		result.vc_buffers[0] = Shaders::CreatePBRVertexConstants(resource_manager->m_graphics);
		result.vc_buffer_count = 1;

		memcpy(result.pixel_textures, texture_views, sizeof(ResourceView) * ECS_PBR_MATERIAL_MAPPING_COUNT);
		// Environment diffuse
		result.pixel_textures[5] = nullptr;
		// Environment specular
		result.pixel_textures[6] = nullptr;
		// Environment brdf lut
		result.pixel_textures[7] = nullptr;
		result.pixel_texture_count = 8;

		result.name = function::StringCopyTs(resource_manager->m_memory, pbr.name).buffer;

		return result;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

#pragma endregion

}