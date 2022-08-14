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

#define ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_INITIAL_SIZE ECS_MB
#define ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_BACKUP_SIZE (ECS_MB * 10)

namespace ECSEngine {

#define ECS_RESOURCE_MANAGER_INITIAL_LOAD_INCREMENT_COUNT 100


	// The handler must implement a void* parameter for additional info and must return void*
	// If it returns a nullptr, it means the load failed so do not add it to the table
	template<bool reference_counted, typename Handler>
	void* LoadResource(
		ResourceManager* resource_manager,
		Stream<wchar_t> path,
		ResourceType type,
		ResourceManagerLoadDesc load_descriptor,
		Handler&& handler
	) {
		unsigned int type_int = (unsigned int)type;
		ResourceIdentifier identifier(path);

		auto register_resource = [=](void* data) {
			// Account for the L'\0'
			void* allocation = function::Copy(resource_manager->Allocator(), identifier.ptr, identifier.size + 2);

			ResourceManagerEntry entry;
			entry.data_pointer.SetPointer(data);
			entry.data_pointer.SetData(USHORT_MAX);
			entry.time_stamp = OS::GetFileLastWrite(path);

			ResourceIdentifier new_identifier(allocation, identifier.size);
			InsertIntoDynamicTable(resource_manager->m_resource_types[type_int], resource_manager->m_memory, entry, new_identifier);
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

			ResourceManagerEntry* entry = resource_manager->m_resource_types[type_int].GetValuePtrFromIndex(table_index);
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
		Stream<wchar_t> path,
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

			allocation = (void*)function::AlignPointer((uintptr_t)allocation + identifier.size + sizeof(wchar_t), 8);

			memset(allocation, 0, allocation_size);
			void* data = handler(allocation);

			if (data != nullptr) {
				ResourceManagerEntry entry;
				entry.data_pointer.SetPointer(data);
				entry.data_pointer.SetData(initial_increment);
				entry.time_stamp = OS::GetFileLastWrite(path);

				ResourceIdentifier new_identifier(allocation, identifier.size);
				InsertIntoDynamicTable(resource_manager->m_resource_types[type_int], resource_manager->m_memory, entry, new_identifier);

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
				return register_resource(ECS_RESOURCE_MANAGER_INITIAL_LOAD_INCREMENT_COUNT);
			}

			unsigned short increment_count = load_descriptor.load_flags & ECS_RESOURCE_MANAGER_MASK_INCREMENT_COUNT;
			ResourceManagerEntry* entry = resource_manager->m_resource_types[type_int].GetValuePtrFromIndex(table_index);
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

		ResourceManagerEntry entry = resource_manager->m_resource_types[type_int].GetValueFromIndex(index);

		void (*handler)(void*, ResourceManager*) = UNLOAD_FUNCTIONS[type_int];

		auto delete_resource = [=]() {
			void* data = entry.data_pointer.GetPointer();
			handler(data, resource_manager);
			resource_manager->Deallocate(resource_manager->m_resource_types[type_int].GetIdentifierFromIndex(index).ptr);
			resource_manager->m_resource_types[type_int].EraseFromIndex(index);
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

		int hashed_position = resource_manager->m_resource_types[type_int].Find<true>(identifier);
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
		FreeCoallescedMesh(resource_manager->m_graphics, &data->mesh);
	}

	void DeletePBRMesh(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadPBRMesh<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadCoallescedMeshHandler(void* parameter, ResourceManager* resource_manager) {
		// Free the coallesced mesh - the submeshes get the deallocated at the same time as this resource
		CoallescedMesh* mesh = (CoallescedMesh*)parameter;
		FreeCoallescedMesh(resource_manager->m_graphics, mesh);
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
		m_resource_types.Initialize(m_memory, (unsigned int)ResourceType::TypeCount);
		// Set to 0 the initial data for these resource types such that if a type is not being constructed but it is
		// accessed it will cause a crash
		memset(m_resource_types.buffer, 0, m_resource_types.MemoryOf(m_resource_types.size));

		m_shader_directory.Initialize(GetAllocatorPolymorphic(m_memory), 0);

		// Set the initial shader directory
		AddShaderDirectory(ECS_SHADER_DIRECTORY);

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

	// TODO: Implement invariance allocations - some resources need when unloaded to have deallocated some buffers and those might not come
	// from the resource manager
	void ResourceManager::AddResource(ResourceIdentifier identifier, ResourceType resource_type, void* resource, size_t time_stamp, unsigned short reference_count)
	{
		ResourceManagerEntry entry;
		entry.data_pointer.SetPointer(resource);
		entry.data_pointer.SetData(reference_count);
		entry.time_stamp = time_stamp;

		// The identifier needs to be allocated, account for the L'\0'
		void* allocation = m_memory->Allocate(identifier.size + sizeof(wchar_t), 2);
		memcpy(allocation, identifier.ptr, identifier.size + sizeof(wchar_t));
		identifier.ptr = allocation;

		unsigned int resource_type_int = (unsigned int)resource_type;
		// If it is not yet allocated, then allocate with a small capacity at first
		if (m_resource_types[resource_type_int].GetCapacity() == 0) {
			m_resource_types[resource_type_int].Initialize(m_memory, 16);
		}

		InsertIntoDynamicTable(m_resource_types[resource_type_int], m_memory, entry, identifier);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::AddShaderDirectory(Stream<wchar_t> directory)
	{
		m_shader_directory.Add(function::StringCopy(Allocator(), directory));
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

	AllocatorPolymorphic ResourceManager::Allocator() const
	{
		return GetAllocatorPolymorphic(m_memory);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic ResourceManager::AllocatorTs() const
	{
		return GetAllocatorPolymorphic(m_memory, ECS_ALLOCATION_MULTI);
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
		m_resource_types[type_int].ForEachIndex([&](unsigned int index) {
			ResourceManagerEntry* entry = m_resource_types[type_int].GetValuePtrFromIndex(index);
			unsigned short value = entry->data_pointer.GetData();

			if (value != USHORT_MAX) {
				unsigned short count = entry->data_pointer.DecrementData(amount);
				if constexpr (delete_if_zero) {
					if (count == 0) {
						DELETE_FUNCTIONS[type_int](this, index, 1);
					}
				}
			}

			return false;
		});
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::DecrementReferenceCount, ResourceType, unsigned int);

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::Exists(ResourceIdentifier identifier, ResourceType type) const
	{
		if (m_resource_types[(unsigned int)type].GetCapacity() == 0) {
			return false;
		}
		int hashed_position = m_resource_types[(unsigned int)type].Find<true>(identifier);
		return hashed_position != -1;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::Exists(ResourceIdentifier identifier, ResourceType type, unsigned int& table_index) const {
		if (m_resource_types[(unsigned int)type].GetCapacity() == 0) {
			return false;
		}

		table_index = m_resource_types[(unsigned int)type].Find<true>(identifier);
		return table_index != -1;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::EvictResource(ResourceIdentifier identifier, ResourceType type)
	{
		unsigned int type_int = (unsigned int)type;
		unsigned int table_index = m_resource_types[type_int].Find(identifier);
		ECS_ASSERT(table_index != -1, "Trying to evict a resource from ResourceManager that doesn't exist.");

		ResourceManagerEntry entry = m_resource_types[type_int].GetValueFromIndex(table_index);
		identifier = m_resource_types[type_int].GetIdentifierFromIndex(table_index);

		void* data = entry.data_pointer.GetPointer();
		Deallocate(identifier.ptr);
		m_resource_types[type_int].EraseFromIndex(table_index);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::EvictResourcesFrom(const ResourceManager* other)
	{
		for (size_t index = 0; index < (size_t)ResourceType::TypeCount; index++) {
			auto& other_table = other->m_resource_types[index];
			
			other_table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
				EvictResource(identifier, (ResourceType)index);
			});
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::EvictOutdatedResources(ResourceType type)
	{
		unsigned int int_type = (unsigned int)type;
		// Iterate through all resources and check their new stamps
		m_resource_types[int_type].ForEachIndex([&](unsigned int index) {
			// Get the new stamp
			size_t new_stamp = OS::GetFileLastWrite((const wchar_t*)m_resource_types[int_type].GetIdentifierFromIndex(index).ptr);
			if (new_stamp > m_resource_types[int_type].GetValueFromIndex(index).time_stamp) {
				// Kick this resource
				DELETE_FUNCTIONS[int_type](this, index, 1);
				// Decrement the index because other resources can move into this place
				return true;
			}

			return false;
		});
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	int ResourceManager::GetResourceIndex(ResourceIdentifier identifier, ResourceType type) const
	{
		int index = m_resource_types[(unsigned int)type].Find<true>(identifier);

		ECS_ASSERT(index != -1, "The resource was not found");
		return index;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void* ResourceManager::GetResource(ResourceIdentifier identifier, ResourceType type)
	{
		int hashed_position = m_resource_types[(unsigned int)type].Find<true>(identifier);
		if (hashed_position != -1) {
			return m_resource_types[(unsigned int)type].GetValueFromIndex(hashed_position).data_pointer.GetPointer();
		}
		ECS_ASSERT(false, "The resource was not found");
		return nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	size_t ResourceManager::GetTimeStamp(ResourceIdentifier identifier, ResourceType type) const
	{
		ResourceManagerEntry entry;
		if (m_resource_types[(unsigned int)type].TryGetValue(identifier, entry)) {
			return entry.time_stamp;
		}
		return -1;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceManagerEntry ResourceManager::GetEntry(ResourceIdentifier identifier, ResourceType type) const
	{
		ResourceManagerEntry entry;
		if (m_resource_types[(unsigned int)type].TryGetValue(identifier, entry)) {
			return entry;
		}
		return { DataPointer(nullptr), (size_t)-1 };
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceManagerEntry* ResourceManager::GetEntryPtr(ResourceIdentifier identifier, ResourceType type)
	{
		ResourceManagerEntry* entry;
		if (m_resource_types[(unsigned int)type].TryGetValuePtr(identifier, entry)) {
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

	void ResourceManager::IncrementReferenceCount(ResourceType type, unsigned int amount)
	{
		unsigned int type_int = (unsigned int)type;
		m_resource_types[type_int].ForEach([&](ResourceManagerEntry& entry, ResourceIdentifier identifier) {
			unsigned short count = entry.data_pointer.IncrementData(amount);
			if (count == USHORT_MAX) {
				entry.data_pointer.SetData(USHORT_MAX - 1);
			}
		});
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::InheritResources(const ResourceManager* other)
	{
		for (size_t index = 0; index < (unsigned int)ResourceType::TypeCount; index++) {
			auto& table = other->m_resource_types[index];

			unsigned int other_capacity = table.GetCapacity();
			if (other_capacity > 0) {
				// Now insert all the resources
				if (other->m_graphics == m_graphics) {
					// Can just reference the resource
					table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
						AddResource(identifier, (ResourceType)index, entry.data_pointer.GetPointer(), entry.time_stamp, entry.data_pointer.GetData());
					});
				}
				else {
					switch ((ResourceType)index) {
					case ResourceType::ComputeShader:
					case ResourceType::DomainShader:
					case ResourceType::GeometryShader:
					case ResourceType::HullShader:
					case ResourceType::PixelShader:
					case ResourceType::TextFile:
					case ResourceType::VertexShader:
					case ResourceType::Material:
						// Can just reference the resource for these types
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							AddResource(identifier, (ResourceType)index, entry.data_pointer.GetPointer(), entry.time_stamp, entry.data_pointer.GetData());
						});
						break;
					case ResourceType::CoallescedMesh:
						// Need to copy the mesh buffers
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							CoallescedMesh* mesh = (CoallescedMesh*)entry.data_pointer.GetPointer();
							CoallescedMesh new_mesh = m_graphics->TransferCoallescedMesh(mesh);
							AddResource(identifier, (ResourceType)index, &new_mesh, entry.time_stamp, entry.data_pointer.GetData());
						});
						break;
					case ResourceType::Mesh:
						// Need to copy the mesh buffers
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							Mesh* mesh = (Mesh*)entry.data_pointer.GetPointer();
							Mesh new_mesh = m_graphics->TransferMesh(mesh);
							AddResource(identifier, (ResourceType)index, &new_mesh, entry.time_stamp, entry.data_pointer.GetData());
						});
						break;
					case ResourceType::PBRMesh:
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							PBRMesh* pbr_mesh = (PBRMesh*)entry.data_pointer.GetPointer();
							PBRMesh new_mesh = m_graphics->TransferPBRMesh(pbr_mesh);
							AddResource(identifier, (ResourceType)index, &new_mesh, entry.time_stamp, entry.data_pointer.GetData());
						});
						break;
					case ResourceType::Texture:
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							ResourceView view = (ID3D11ShaderResourceView*)entry.data_pointer.GetPointer();
							ResourceView new_view = TransferGPUView(view, m_graphics->GetDevice());
							AddResource(identifier, (ResourceType)index, new_view.view, entry.time_stamp, entry.data_pointer.GetData());
						});
						break;
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	char* ResourceManager::LoadTextFile(
		Stream<wchar_t> filename,
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
				return LoadTextFileImplementation(filename, size);
			}
		);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(char*, ResourceManager::LoadTextFile, Stream<wchar_t>, size_t*, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	char* ResourceManager::LoadTextFileImplementation(Stream<wchar_t> file, size_t* parameter)
	{
		Stream<char> contents = ReadWholeFileText(file, Allocator());
		if (contents.buffer != nullptr) {
			*parameter = contents.size;
			return contents.buffer;
		}
		return nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::OpenFile(Stream<wchar_t> filename, ECS_FILE_HANDLE* input, bool binary, unsigned int thread_index)
	{
		ECS_TEMP_STRING(path, 512);
		ECS_FILE_ACCESS_FLAGS access_flags = binary ? ECS_FILE_ACCESS_BINARY : ECS_FILE_ACCESS_TEXT;
		access_flags |= ECS_FILE_ACCESS_READ_ONLY;
		ECS_FILE_STATUS_FLAGS status = ECSEngine::OpenFile(filename, input, access_flags);

		return status == ECS_FILE_STATUS_OK;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	ResourceView ResourceManager::LoadTexture(
		Stream<wchar_t> filename,
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

	ECS_TEMPLATE_FUNCTION_BOOL(ResourceView, ResourceManager::LoadTexture, Stream<wchar_t>, ResourceManagerTextureDesc*, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceView ResourceManager::LoadTextureImplementation(
		Stream<wchar_t> filename,
		ResourceManagerTextureDesc* descriptor,
		ResourceManagerLoadDesc load_descriptor
	)
	{
		NULL_TERMINATE_WIDE(filename);

		ID3D11ShaderResourceView* texture_view = nullptr;
		ID3D11Resource* resource = nullptr;

		// Determine the texture extension - HDR textures must take a different path
		Stream<wchar_t> texture_path = filename;
		Path extension = function::PathExtensionBoth(texture_path);
		ECS_ASSERT(extension.size > 0, "No extension could be identified");

		bool is_hdr_texture = function::CompareStrings(extension, L".hdr");
		bool is_tga_texture = function::CompareStrings(extension, L".tga");

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
			result = DirectX::LoadFromHDRFile(filename.buffer, nullptr, image);
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
			result = DirectX::LoadFromTGAFile(filename.buffer, nullptr, image);
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
			result = DirectX::LoadFromWICFile(filename.buffer, DirectX::WIC_FLAGS_FORCE_RGB | DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image);
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
			m_graphics->AddInternalResource(ResourceView(texture_view), ECS_DEBUG_INFO);
			m_graphics->AddInternalResource(texture, ECS_DEBUG_INFO);
		}

		return texture_view;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes from a gltf file
	template<bool reference_counted>
	Stream<Mesh>* ResourceManager::LoadMeshes(
		Stream<wchar_t> filename,
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

	ECS_TEMPLATE_FUNCTION_BOOL(Stream<Mesh>*, ResourceManager::LoadMeshes, Stream<wchar_t>, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes from a gltf file
	Stream<Mesh>* ResourceManager::LoadMeshImplementation(Stream<wchar_t> filename, ResourceManagerLoadDesc load_descriptor) {
		ECS_TEMP_STRING(_path, 512);
		GLTFData data = LoadGLTFFile(filename);
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
	CoallescedMesh* ResourceManager::LoadCoallescedMesh(Stream<wchar_t> filename, ResourceManagerLoadDesc load_descriptor)
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

	ECS_TEMPLATE_FUNCTION_BOOL(CoallescedMesh*, ResourceManager::LoadCoallescedMesh, Stream<wchar_t>, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	CoallescedMesh* ResourceManager::LoadCoallescedMeshImplementation(Stream<wchar_t> filename, ResourceManagerLoadDesc load_descriptor)
	{
		ECS_TEMP_STRING(_path, 512);
		GLTFData data = LoadGLTFFile(filename);
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
		Stream<wchar_t> filename,
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

	ECS_TEMPLATE_FUNCTION_BOOL(Stream<PBRMaterial>*, ResourceManager::LoadMaterials, Stream<wchar_t>, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all materials from a gltf file
	Stream<PBRMaterial>* ResourceManager::LoadMaterialsImplementation(Stream<wchar_t> filename, ResourceManagerLoadDesc load_descriptor) {
		ECS_TEMP_STRING(_path, 512);
		GLTFData data = LoadGLTFFile(filename);
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
		Stream<wchar_t> filename,
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

	ECS_TEMPLATE_FUNCTION_BOOL(PBRMesh*, ResourceManager::LoadPBRMesh, Stream<wchar_t>, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes and materials from a gltf file, combines the meshes into a single one sorted by material submeshes
	PBRMesh* ResourceManager::LoadPBRMeshImplementation(Stream<wchar_t> filename, ResourceManagerLoadDesc load_descriptor) {
		ECS_TEMP_STRING(_path, 512);
		GLTFData data = LoadGLTFFile(filename);
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
	Shader LoadShaderImplementation(ResourceManager* manager, Stream<wchar_t> filename, ShaderCompileOptions options, ResourceManagerLoadDesc load_descriptor,
		Stream<char>* shader_source_code, ByteCodeHandler&& byte_code_handler, SourceHandler&& source_handler) {
		Shader shader;

		ECS_TEMP_STRING(path, 512);
		Path path_extension = function::PathExtensionBoth(filename);
		bool is_byte_code = function::CompareStrings(path_extension, L".cso");

		Stream<void> contents = { nullptr, 0 };
		AllocatorPolymorphic allocator_polymorphic = GetAllocatorPolymorphic(manager->m_memory, ECS_ALLOCATION_TYPE::ECS_ALLOCATION_MULTI);

		if (is_byte_code) {
			contents = ReadWholeFileBinary(filename.buffer, allocator_polymorphic);
			shader = byte_code_handler(contents);
		}
		else {
			Stream<char> source_code = ReadWholeFileText(filename.buffer, allocator_polymorphic);
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
		Stream<wchar_t> filename, 
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

	ECS_TEMPLATE_FUNCTION_BOOL(VertexShader*, ResourceManager::LoadVertexShader, Stream<wchar_t>, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	VertexShader ResourceManager::LoadVertexShaderImplementation(
		Stream<wchar_t> filename, 
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
		Stream<wchar_t> filename, 
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

	ECS_TEMPLATE_FUNCTION_BOOL(PixelShader*, ResourceManager::LoadPixelShader, Stream<wchar_t>, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	PixelShader ResourceManager::LoadPixelShaderImplementation(
		Stream<wchar_t> filename, 
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
		Stream<wchar_t> filename,
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

	ECS_TEMPLATE_FUNCTION_BOOL(HullShader*, ResourceManager::LoadHullShader, Stream<wchar_t>, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	HullShader ResourceManager::LoadHullShaderImplementation(
		Stream<wchar_t> filename,
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
		Stream<wchar_t> filename, 
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

	ECS_TEMPLATE_FUNCTION_BOOL(DomainShader*, ResourceManager::LoadDomainShader, Stream<wchar_t>, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	DomainShader ResourceManager::LoadDomainShaderImplementation(
		Stream<wchar_t> filename, 
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
		Stream<wchar_t> filename, 
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

	ECS_TEMPLATE_FUNCTION_BOOL(GeometryShader*, ResourceManager::LoadGeometryShader, Stream<wchar_t>, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	GeometryShader ResourceManager::LoadGeometryShaderImplementation(
		Stream<wchar_t> filename,
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
		Stream<wchar_t> filename,
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

	ECS_TEMPLATE_FUNCTION_BOOL(ComputeShader*, ResourceManager::LoadComputeShader, Stream<wchar_t>, Stream<char>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	ComputeShader ResourceManager::LoadComputeShaderImplementation(
		Stream<wchar_t> filename, 
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
		bool success = m_resource_types[int_type].TryGetValuePtr(identifier, entry);
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
		bool success = m_resource_types[int_type].TryGetValuePtr(identifier, entry);

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
		bool success = m_resource_types[type_index].TryGetValuePtr(identifier, entry);

		ECS_ASSERT(success, "Could not remove reference count for resource");
		entry->data_pointer.SetData(USHORT_MAX);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

#define EXPORT_UNLOAD(name) ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::name, Stream<wchar_t>, size_t); ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::name, unsigned int, size_t);

	template<bool reference_counted>
	void ResourceManager::UnloadTextFile(Stream<wchar_t> filename, size_t flags)
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
	void ResourceManager::UnloadTexture(Stream<wchar_t> filename, size_t flags)
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
	void ResourceManager::UnloadMeshes(Stream<wchar_t> filename, size_t flags) {
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
	void ResourceManager::UnloadCoallescedMesh(Stream<wchar_t> filename, size_t flags)
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
	void ResourceManager::UnloadMaterials(Stream<wchar_t> filename, size_t flags) {
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
	void ResourceManager::UnloadPBRMesh(Stream<wchar_t> filename, size_t flags) {
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
	void ResourceManager::UnloadVertexShader(Stream<wchar_t> filename, size_t flags)
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
	void ResourceManager::UnloadPixelShader(Stream<wchar_t> filename, size_t flags)
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
	void ResourceManager::UnloadHullShader(Stream<wchar_t> filename, size_t flags)
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
	void ResourceManager::UnloadDomainShader(Stream<wchar_t> filename, size_t flags)
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
	void ResourceManager::UnloadGeometryShader(Stream<wchar_t> filename, size_t flags)
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
	void ResourceManager::UnloadComputeShader(Stream<wchar_t> filename, size_t flags)
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
	void ResourceManager::UnloadResource(Stream<wchar_t> filename, ResourceType type, size_t flags)
	{
		DeleteResource<reference_counted>(this, filename, type, flags);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadResource, Stream<wchar_t>, ResourceType, size_t);

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
			unsigned int type_resource_count = m_resource_types[index].GetCount();
			if (type_resource_count > 0) {
				unsigned int table_capacity = m_resource_types[index].GetExtendedCapacity();
				unsigned int unload_count = 0;
				for (size_t subindex = 0; subindex < table_capacity && unload_count < type_resource_count; subindex++) {
					if (m_resource_types[index].IsItemAt(subindex)) {
						ResourceManagerEntry entry = m_resource_types[index].GetValueFromIndex(subindex);
						UnloadResourceImplementation(entry.data_pointer.GetPointer(), (ResourceType)index);

						// Helps to early exit if the resources are clumped together towards the beginning of the table
						unload_count++;
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::UnloadAll(ResourceType resource_type)
	{
		unsigned int int_type = (unsigned int)resource_type;
		m_resource_types[int_type].ForEachIndex([&](unsigned int index) {
			UnloadResource(index, resource_type);
			return true;
		});
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// ---------------------------------------------------------------------------------------------------------------------------

#pragma region Free functions

	// ---------------------------------------------------------------------------------------------------------------------------

	MemoryManager DefaultResourceManagerAllocator(GlobalMemoryManager* global_allocator)
	{
		return MemoryManager(ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_INITIAL_SIZE, 2048, ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_BACKUP_SIZE, global_allocator);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	size_t DefaultResourceManagerAllocatorSize()
	{
		return ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_INITIAL_SIZE;
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
		Stream<wchar_t> texture_extensions[] = {
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
		LinearAllocator _temporary_allocator(memory, ECS_KB * 8);
		AllocatorPolymorphic temporary_allocator = GetAllocatorPolymorphic(&_temporary_allocator);

		if (pbr.color_texture.buffer != nullptr && pbr.color_texture.size > 0) {
			shader_macros.Add({ "COLOR_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.color_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_COLOR, TextureCompressionExplicit::ColorMap });
		}

		if (pbr.emissive_texture.buffer != nullptr && pbr.emissive_texture.size > 0) {
			shader_macros.Add({ "EMISSIVE_TEXTURE", "" });
			
			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.emissive_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_EMISSIVE, TextureCompressionExplicit::ColorMap });
		}

		if (pbr.metallic_texture.buffer != nullptr && pbr.metallic_texture.size > 0) {
			shader_macros.Add({ "METALLIC_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.metallic_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_METALLIC, TextureCompressionExplicit::GrayscaleMap });
		}

		if (pbr.normal_texture.buffer != nullptr && pbr.normal_texture.size > 0) {
			shader_macros.Add({ "NORMAL_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.normal_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_NORMAL, TextureCompressionExplicit::NormalMapLowQuality });
		}

		if (pbr.occlusion_texture.buffer != nullptr && pbr.occlusion_texture.size > 0) {
			shader_macros.Add({ "OCCLUSION_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.occlusion_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_OCCLUSION, TextureCompressionExplicit::GrayscaleMap });
		}

		if (pbr.roughness_texture.buffer != nullptr && pbr.roughness_texture.size > 0) {
			shader_macros.Add({ "ROUGHNESS_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.roughness_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_ROUGHNESS, TextureCompressionExplicit::GrayscaleMap });
		}

		struct FunctorData {
			AllocatorPolymorphic allocator;
			Stream<Mapping>* mappings;
		};

		// Search the directory for the textures
		auto search_functor = [](Stream<wchar_t> path, void* _data) {
			FunctorData* data = (FunctorData*)_data;

			Stream<wchar_t> filename = function::PathStem(path);

			for (size_t index = 0; index < data->mappings->size; index++) {
				if (function::CompareStrings(filename, data->mappings->buffer[index].texture)) {
					Stream<wchar_t> new_texture = function::StringCopy(data->allocator, path);
					data->mappings->buffer[index].texture = new_texture;
					data->mappings->Swap(index, data->mappings->size - 1);
					data->mappings->size--;
					return data->mappings->size > 0;
				}
			}
			return data->mappings->size > 0;
		};

		size_t texture_count = mappings.size;
		FunctorData functor_data = { temporary_allocator, &mappings };
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

		result.name = function::StringCopy(resource_manager->AllocatorTs(), pbr.name).buffer;

		return result;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

#pragma endregion

}