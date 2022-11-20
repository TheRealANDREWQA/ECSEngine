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

	ResourceType FromShaderTypeToResourceType(ECS_SHADER_TYPE shader_type) {
		ResourceType type;
		switch (shader_type)
		{
		case ECS_SHADER_VERTEX:
			type = ResourceType::VertexShader;
			break;
		case ECS_SHADER_PIXEL:
			type = ResourceType::PixelShader;
			break;
		case ECS_SHADER_DOMAIN:
			type = ResourceType::DomainShader;
			break;
		case ECS_SHADER_HULL:
			type = ResourceType::HullShader;
			break;
		case ECS_SHADER_GEOMETRY:
			type = ResourceType::GeometryShader;
			break;
		case ECS_SHADER_COMPUTE:
			type = ResourceType::ComputeShader;
			break;
		default:
			ECS_ASSERT(false);
			break;
		}
		return type;
	}

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
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_name, 512);

		unsigned int type_int = (unsigned int)type;		
		ResourceIdentifier identifier = ResourceIdentifier::WithSuffix(path, fully_specified_name, load_descriptor.identifier_suffix);

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
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);

		unsigned int type_int = (unsigned int)type;
		ResourceIdentifier identifier = ResourceIdentifier::WithSuffix(path, fully_specified_identifier, load_descriptor.identifier_suffix);

		auto register_resource = [=](unsigned short initial_increment) {
			// plus 7 bytes for padding and account for the L'\0'
			void* allocation = resource_manager->Allocate(allocation_size + identifier.size + sizeof(wchar_t) + 7);
			ECS_ASSERT(allocation != nullptr, "Allocating memory for a resource failed");
			memcpy(allocation, identifier.ptr, identifier.size + sizeof(wchar_t));

			void* handler_allocation = (void*)function::AlignPointer((uintptr_t)allocation + identifier.size + sizeof(wchar_t), 8);

			memset(handler_allocation, 0, allocation_size);
			void* data = handler(handler_allocation);

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

		unsigned int hashed_position = resource_manager->m_resource_types[type_int].Find(identifier);
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

	void UnloadShaderHandler(void* parameter, ResourceManager* resource_manager) {
		VertexShaderStorage* storage = (VertexShaderStorage*)parameter;
		resource_manager->m_graphics->FreeResource(storage->shader);
		AllocatorPolymorphic allocator = resource_manager->Allocator();
		DeallocateIfBelongs(allocator, storage->source_code.buffer);
		DeallocateIfBelongs(allocator, storage->byte_code.buffer);
	}

	template<ECS_SHADER_TYPE shader_type>
	void DeleteShader(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadShader<false>(index, shader_type, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void UnloadMisc(void* parameter, ResourceManager* resource_manager) {
		ResizableStream<void>* data = (ResizableStream<void>*)parameter;
		data->FreeBuffer();
	}

	void DeleteMisc(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadMisc<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	constexpr DeleteFunction DELETE_FUNCTIONS[] = {
		DeleteTexture, 
		DeleteTextFile,
		DeleteMeshes,
		DeleteCoallescedMesh,
		DeleteMaterials,
		DeletePBRMesh,
		DeleteShader<ECS_SHADER_VERTEX>,
		DeleteShader<ECS_SHADER_PIXEL>,
		DeleteShader<ECS_SHADER_HULL>,
		DeleteShader<ECS_SHADER_DOMAIN>,
		DeleteShader<ECS_SHADER_GEOMETRY>,
		DeleteShader<ECS_SHADER_COMPUTE>,
		DeleteMisc
	};
	
	constexpr UnloadFunction UNLOAD_FUNCTIONS[] = {
		UnloadTextureHandler,
		UnloadTextFileHandler,
		UnloadMeshesHandler,
		UnloadCoallescedMeshHandler,
		UnloadMaterialsHandler,
		UnloadPBRMeshHandler,
		UnloadShaderHandler,
		UnloadShaderHandler,
		UnloadShaderHandler,
		UnloadShaderHandler,
		UnloadShaderHandler,
		UnloadShaderHandler,
		UnloadMisc
	};

	static_assert(std::size(DELETE_FUNCTIONS) == (unsigned int)ResourceType::TypeCount);
	static_assert(std::size(UNLOAD_FUNCTIONS) == (unsigned int)ResourceType::TypeCount);

	// ---------------------------------------------------------------------------------------------------------------------------

	void AddResourceEx(ResourceManager* resource_manager, ResourceType type, void* data, ResourceManagerLoadDesc load_descriptor, ResourceManagerExDesc* ex_desc) {
		if (ex_desc != nullptr && ex_desc->HasFilename()) {
			ex_desc->Lock();
			resource_manager->AddResource(ex_desc->filename, type, data, ex_desc->time_stamp, load_descriptor.identifier_suffix);
			ex_desc->Unlock();
		}
	}

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
	void ResourceManager::AddResource(ResourceIdentifier identifier, ResourceType resource_type, void* resource, size_t time_stamp, Stream<void> suffix, unsigned short reference_count)
	{
		ResourceManagerEntry entry;
		entry.data_pointer.SetPointer(resource);
		entry.data_pointer.SetData(reference_count);
		entry.time_stamp = time_stamp;

		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		identifier = ResourceIdentifier::WithSuffix(identifier, fully_specified_identifier, suffix);

		// The identifier needs to be allocated, account for the L'\0'
		void* allocation = m_memory->Allocate(identifier.size + sizeof(wchar_t), alignof(wchar_t));
		memcpy(allocation, identifier.ptr, identifier.size + sizeof(wchar_t));
		identifier.ptr = allocation;

		unsigned int resource_type_int = (unsigned int)resource_type;
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

	bool ResourceManager::Exists(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix) const
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		identifier = ResourceIdentifier::WithSuffix(identifier, fully_specified_identifier, suffix);

		if (m_resource_types[(unsigned int)type].GetCapacity() == 0) {
			return false;
		}
		int hashed_position = m_resource_types[(unsigned int)type].Find(identifier);
		return hashed_position != -1;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::Exists(ResourceIdentifier identifier, ResourceType type, unsigned int& table_index, Stream<void> suffix) const {
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		identifier = ResourceIdentifier::WithSuffix(identifier, fully_specified_identifier, suffix);

		if (m_resource_types[(unsigned int)type].GetCapacity() == 0) {
			return false;
		}

		table_index = m_resource_types[(unsigned int)type].Find(identifier);
		return table_index != -1;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::EvictResource(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		identifier = ResourceIdentifier::WithSuffix(identifier, fully_specified_identifier, suffix);

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

	unsigned int ResourceManager::GetResourceIndex(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix) const
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		identifier = ResourceIdentifier::WithSuffix(identifier, fully_specified_identifier, suffix);

		if (m_resource_types[(unsigned int)type].GetCapacity() == 0) {
			return -1;
		}

		return m_resource_types[(unsigned int)type].Find(identifier);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void* ResourceManager::GetResource(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix)
	{
		return (void*)((const ResourceManager*)this)->GetResource(identifier, type, suffix);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	const void* ResourceManager::GetResource(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix) const
	{
		// It is valid for the failure case as well
		return GetEntry(identifier, type, suffix).data_pointer.GetPointer();
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	size_t ResourceManager::GetTimeStamp(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix) const
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		identifier = ResourceIdentifier::WithSuffix(identifier, fully_specified_identifier, suffix);

		ResourceManagerEntry entry;
		if (m_resource_types[(unsigned int)type].TryGetValue(identifier, entry)) {
			return entry.time_stamp;
		}
		return -1;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceManagerEntry ResourceManager::GetEntry(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix) const
	{
		unsigned int index = GetResourceIndex(identifier, type, suffix);
		if (index == -1) {
			return { DataPointer(nullptr), (size_t)-1 };
		}
		return m_resource_types[(unsigned int)type].GetValueFromIndex(index);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceManagerEntry* ResourceManager::GetEntryPtr(ResourceIdentifier identifier, ResourceType type, Stream<void> suffix)
	{
		unsigned int index = GetResourceIndex(identifier, type, suffix);
		if (index == -1) {
			return nullptr;
		}
		return m_resource_types[(unsigned int)type].GetValuePtrFromIndex(index);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::IsResourceOutdated(ResourceIdentifier identifier, ResourceType type, size_t new_stamp, Stream<void> suffix)
	{
		ResourceManagerEntry entry = GetEntry(identifier, type, suffix);
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
						AddResource(identifier, (ResourceType)index, entry.data_pointer.GetPointer(), entry.time_stamp, { nullptr, 0 }, entry.data_pointer.GetData());
					});
				}
				else {
					switch ((ResourceType)index) {
					case ResourceType::TextFile:
					case ResourceType::PBRMaterial:
						// These types don't need to be transferred on the other graphics object
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							AddResource(identifier, (ResourceType)index, entry.data_pointer.GetPointer(), entry.time_stamp, { nullptr, 0 }, entry.data_pointer.GetData());
						});
						break;
					case ResourceType::VertexShader:
					case ResourceType::ComputeShader:
					case ResourceType::DomainShader:
					case ResourceType::GeometryShader:
					case ResourceType::HullShader:
					case ResourceType::PixelShader:
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							// Need to transfer the shader
						});
						break;
					case ResourceType::CoallescedMesh:
						// Need to copy the mesh buffers
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							CoallescedMesh* mesh = (CoallescedMesh*)entry.data_pointer.GetPointer();
							CoallescedMesh new_mesh = m_graphics->TransferCoallescedMesh(mesh);
							AddResource(identifier, (ResourceType)index, &new_mesh, entry.time_stamp, { nullptr, 0 }, entry.data_pointer.GetData());
						});
						break;
					case ResourceType::Mesh:
						// Need to copy the mesh buffers
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							Mesh* mesh = (Mesh*)entry.data_pointer.GetPointer();
							Mesh new_mesh = m_graphics->TransferMesh(mesh);
							AddResource(identifier, (ResourceType)index, &new_mesh, entry.time_stamp, { nullptr, 0 }, entry.data_pointer.GetData());
						});
						break;
					case ResourceType::PBRMesh:
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							PBRMesh* pbr_mesh = (PBRMesh*)entry.data_pointer.GetPointer();
							PBRMesh new_mesh = m_graphics->TransferPBRMesh(pbr_mesh);
							AddResource(identifier, (ResourceType)index, &new_mesh, entry.time_stamp, { nullptr, 0 }, entry.data_pointer.GetData());
						});
						break;
					case ResourceType::Texture:
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							ResourceView view = (ID3D11ShaderResourceView*)entry.data_pointer.GetPointer();
							ResourceView new_view = TransferGPUView(view, m_graphics->GetDevice());
							AddResource(identifier, (ResourceType)index, new_view.view, entry.time_stamp, { nullptr, 0 }, entry.data_pointer.GetData());
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

	template<bool reference_counted>
	ResourceView ResourceManager::LoadTexture(
		Stream<wchar_t> filename,
		const ResourceManagerTextureDesc* descriptor,
		ResourceManagerLoadDesc load_descriptor
	)
	{
		void* texture_view = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::Texture, 
			load_descriptor,
			[&]() {
			return LoadTextureImplementation(filename, descriptor, load_descriptor).view;
		});

		return (ID3D11ShaderResourceView*)texture_view;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(ResourceView, ResourceManager::LoadTexture, Stream<wchar_t>, const ResourceManagerTextureDesc*, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceView ResourceManager::LoadTextureImplementation(
		Stream<wchar_t> filename,
		const ResourceManagerTextureDesc* descriptor,
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

		DirectX::ScratchImage image;
		image.SetAllocator(allocator, allocate_function, deallocate_function);
		if (is_hdr_texture) {
			bool apply_tonemapping = function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEXTURE_HDR_TONEMAP);
			result = DirectX::LoadFromHDRFile(filename.buffer, nullptr, image, apply_tonemapping);
		}
		else if (is_tga_texture) {
			result = DirectX::LoadFromTGAFile(filename.buffer, nullptr, image);
		}
		else {
			DirectX::WIC_FLAGS srgb_flag = descriptor->srgb ? DirectX::WIC_FLAGS_DEFAULT_SRGB : DirectX::WIC_FLAGS_IGNORE_SRGB;
			result = DirectX::LoadFromWICFile(filename.buffer, DirectX::WIC_FLAGS_FORCE_RGB | srgb_flag, nullptr, image);

		}
		if (FAILED(result)) {
			return nullptr;
		}

		DecodedTexture decoded_texture;
		decoded_texture.data = { image.GetPixels(), image.GetPixelsSize() };
		const auto metadata = image.GetMetadata();
		uint2 size = { (unsigned int)metadata.width, (unsigned int)metadata.height };
		decoded_texture.format = GetGraphicsFormatFromNative(metadata.format);
		decoded_texture.width = size.x;
		decoded_texture.height = size.y;
	
		return LoadTextureImplementationEx(decoded_texture, descriptor, load_descriptor);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceView ResourceManager::LoadTextureImplementationEx(
		DecodedTexture decoded_texture,
		const ResourceManagerTextureDesc* descriptor,
		ResourceManagerLoadDesc load_descriptor,
		ResourceManagerExDesc* ex_desc
	)
	{
		ResourceView texture_view;
		texture_view.view = nullptr;

		bool is_compression = descriptor->compression != ECS_TEXTURE_COMPRESSION_EX_NONE;
		bool temporary = function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY);

		if (is_compression) {
			DirectX::ScratchImage image;

			DirectX::Image new_image;
			new_image.pixels = (uint8_t*)decoded_texture.data.buffer;
			new_image.width = decoded_texture.width;
			new_image.height = decoded_texture.height;
			new_image.format = GetGraphicsNativeFormat(decoded_texture.format);
			ECS_ASSERT(!FAILED(DirectX::ComputePitch(new_image.format, new_image.width, new_image.height, new_image.rowPitch, new_image.slicePitch)));

			if (function::HasFlag(descriptor->misc_flags, ECS_GRAPHICS_MISC_GENERATE_MIPS) || descriptor->context != nullptr) {
				void* new_allocation = malloc(new_image.slicePitch);
				memcpy(new_allocation, new_image.pixels, new_image.slicePitch);
				new_image.pixels = (uint8_t*)new_allocation;
				HRESULT result = DirectX::GenerateMipMaps(new_image, DirectX::TEX_FILTER_LINEAR, 0, image);
				free(new_allocation);
				if (FAILED(result)) {
					return nullptr;
				}
			}

			Stream<void> data[64];
			const auto* images = image.GetImages();
			for (size_t index = 0; index < image.GetImageCount(); index++) {
				data[index] = { images[index].pixels, images[index].slicePitch };
			}

			CompressTextureDescriptor compress_descriptor;
			compress_descriptor.spin_lock = load_descriptor.gpu_lock;
			Texture2D texture = CompressTexture(m_graphics, Stream<Stream<void>>(data, image.GetImageCount()), images[0].width, images[0].height, descriptor->compression, temporary, compress_descriptor);
			if (texture.Interface() == nullptr) {
				return nullptr;
			}
			texture_view = m_graphics->CreateTextureShaderViewResource(texture, temporary);
		}
		else {
			if (descriptor->context != nullptr) {
				// Lock the gpu lock, if any
				if (load_descriptor.gpu_lock != nullptr) {
					load_descriptor.gpu_lock->lock();
				}
				texture_view = m_graphics->CreateTextureWithMips(decoded_texture.data, decoded_texture.format, { decoded_texture.width, decoded_texture.height }, temporary);
				if (load_descriptor.gpu_lock != nullptr) {
					load_descriptor.gpu_lock->unlock();
				}
			}
			else {
				GraphicsTexture2DDescriptor graphics_descriptor;
				graphics_descriptor.size = { decoded_texture.width, decoded_texture.height };
				graphics_descriptor.mip_data = { &decoded_texture.data, 1 };
				graphics_descriptor.mip_levels = 1;
				graphics_descriptor.format = decoded_texture.format;

				Texture2D texture = m_graphics->CreateTexture(&graphics_descriptor, temporary);
				texture_view = m_graphics->CreateTextureShaderViewResource(texture, temporary);
			}
		}

		AddResourceEx(this, ResourceType::Texture, texture_view.view, load_descriptor, ex_desc);

		return texture_view;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes from a gltf file
	template<bool reference_counted>
	Stream<Mesh>* ResourceManager::LoadMeshes(
		Stream<wchar_t> filename,
		float scale_factor,
		ResourceManagerLoadDesc load_descriptor
	) {
		void* meshes = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::Mesh,
			load_descriptor,
			[=]() {
				return LoadMeshImplementation(filename, scale_factor, load_descriptor);
		});

		return (Stream<Mesh>*)meshes;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Stream<Mesh>*, ResourceManager::LoadMeshes, Stream<wchar_t>, float, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes from a gltf file
	Stream<Mesh>* ResourceManager::LoadMeshImplementation(Stream<wchar_t> filename, float scale_factor, ResourceManagerLoadDesc load_descriptor) {
		ECS_TEMP_STRING(_path, 512);
		GLTFData data = LoadGLTFFile(filename);
		// The load failed
		if (data.data == nullptr) {
			return nullptr;
		}

		// This call should not insert it
		Stream<Mesh>* meshes = LoadMeshImplementationEx(&data, scale_factor, load_descriptor);

		// Free the gltf file data and the gltf meshes
		FreeGLTFFile(data);
		return meshes;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	Stream<Mesh>* ResourceManager::LoadMeshImplementationEx(const GLTFData* data, float scale_factor, ResourceManagerLoadDesc load_descriptor, ResourceManagerExDesc* ex_desc)
	{
		ECS_ASSERT(data->mesh_count < 200);

		GLTFMesh* gltf_meshes = (GLTFMesh*)ECS_STACK_ALLOC(sizeof(GLTFMesh) * data->mesh_count);
		// Nullptr and 0 everything
		memset(gltf_meshes, 0, sizeof(GLTFMesh) * data->mesh_count);
		AllocatorPolymorphic allocator = Allocator();

		bool has_invert = !function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT);
		bool success = LoadMeshesFromGLTF(*data, gltf_meshes, allocator, has_invert);
		// The load failed
		if (!success) {
			return nullptr;
		}

		load_descriptor.load_flags |= ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK;
		Stream<Mesh>* meshes = LoadMeshImplementationEx({ gltf_meshes, data->mesh_count }, scale_factor, load_descriptor, ex_desc);
		FreeGLTFMeshes(gltf_meshes, data->mesh_count, allocator);

		return meshes;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	Stream<Mesh>* ResourceManager::LoadMeshImplementationEx(Stream<GLTFMesh> gltf_meshes, float scale_factor, ResourceManagerLoadDesc load_descriptor, ResourceManagerExDesc* ex_desc)
	{
		// Scale the meshes, the function already checks for scale of 1.0f
		ScaleGLTFMeshes(gltf_meshes.buffer, gltf_meshes.size, scale_factor);

		void* allocation = nullptr;
		// Calculate the allocation size
		size_t allocation_size = sizeof(Stream<Mesh>);
		allocation_size += sizeof(Mesh) * gltf_meshes.size;

		// Allocate the needed memeory
		allocation = Allocate(allocation_size);
		Stream<Mesh>* meshes = (Stream<Mesh>*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(Stream<Mesh>);
		meshes->InitializeFromBuffer(buffer, gltf_meshes.size);

		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE;
		// Convert the gltf meshes into multiple meshes
		GLTFMeshesToMeshes(m_graphics, gltf_meshes.buffer, meshes->buffer, meshes->size, misc_flags);

		if (!function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK)) {
			// Rescale the meshes to their original size such that on further processing they will be the same
			ScaleGLTFMeshes(gltf_meshes.buffer, gltf_meshes.size, 1.0f / scale_factor);
		}

		AddResourceEx(this, ResourceType::Mesh, meshes, load_descriptor, ex_desc);

		return meshes;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	CoallescedMesh* ResourceManager::LoadCoallescedMesh(Stream<wchar_t> filename, float scale_factor, ResourceManagerLoadDesc load_descriptor)
	{
		void* meshes = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::CoallescedMesh,
			load_descriptor,
			[=]() {
				return LoadCoallescedMeshImplementation(filename, scale_factor, load_descriptor);
			});

		return (CoallescedMesh*)meshes;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(CoallescedMesh*, ResourceManager::LoadCoallescedMesh, Stream<wchar_t>, float, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	CoallescedMesh* ResourceManager::LoadCoallescedMeshImplementation(Stream<wchar_t> filename, float scale_factor, ResourceManagerLoadDesc load_descriptor)
	{
		ECS_TEMP_STRING(_path, 512);
		GLTFData data = LoadGLTFFile(filename);
		// The load failed
		if (data.data == nullptr || data.mesh_count == 0) {
			return nullptr;
		}
		CoallescedMesh* coallesced_mesh = LoadCoallescedMeshImplementationEx(&data, scale_factor, load_descriptor);
		FreeGLTFFile(data);
		return coallesced_mesh;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	CoallescedMesh* ResourceManager::LoadCoallescedMeshImplementationEx(
		const GLTFData* data,
		float scale_factor, 
		ResourceManagerLoadDesc load_descriptor, 
		ResourceManagerExDesc* ex_desc
	)
	{
		bool has_invert = !function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT);

		AllocatorPolymorphic buffer_allocator = { nullptr };
		// Determine how much memory do the buffers take. If they fit the under a small amount, use the normal allocator.
		// Else use the global allocator
		if (data->data->bin_size <= ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_BACKUP_SIZE || data->data->json_size <= ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_BACKUP_SIZE) {
			buffer_allocator = GetAllocatorPolymorphic(m_memory->m_backup);
		}

		// Calculate the allocation size
		size_t allocation_size = sizeof(CoallescedMesh) + sizeof(Submesh) * data->mesh_count;
		// Allocate the needed memory
		void* allocation = Allocate(allocation_size);
		CoallescedMesh* mesh = (CoallescedMesh*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(CoallescedMesh);
		mesh->submeshes.InitializeFromBuffer(buffer, data->mesh_count);

		bool success = LoadCoallescedMeshFromGLTFToGPU(m_graphics, *data, mesh, has_invert, buffer_allocator);
		if (!success) {
			Deallocate(allocation);
			mesh = nullptr;
		}

		return mesh;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	CoallescedMesh* ResourceManager::LoadCoallescedMeshImplementationEx(
		Stream<GLTFMesh> gltf_meshes, 
		float scale_factor, 
		ResourceManagerLoadDesc load_descriptor, 
		ResourceManagerExDesc* ex_desc
	)
	{
		// Calculate the allocation size
		size_t allocation_size = sizeof(CoallescedMesh) + sizeof(Submesh) * gltf_meshes.size;
		// Allocate the needed memory
		void* allocation = Allocate(allocation_size);
		CoallescedMesh* mesh = (CoallescedMesh*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(CoallescedMesh);
		mesh->submeshes.InitializeFromBuffer(buffer, gltf_meshes.size);

		// Scale the gltf meshes, they already have a built in check for scale of 1.0f
		ScaleGLTFMeshes(gltf_meshes.buffer, gltf_meshes.size, scale_factor);

		Mesh* temporary_meshes = (Mesh*)ECS_STACK_ALLOC(sizeof(Mesh) * gltf_meshes.size);

		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE;
		// Convert the gltf meshes into multiple meshes and then convert these to an aggregated mesh
		GLTFMeshesToMeshes(m_graphics, gltf_meshes.buffer, temporary_meshes, gltf_meshes.size);

		// Convert now to aggregated mesh
		mesh->mesh = MeshesToSubmeshes(m_graphics, { temporary_meshes, gltf_meshes.size }, mesh->submeshes.buffer, misc_flags);

		AddResourceEx(this, ResourceType::CoallescedMesh, mesh, load_descriptor, ex_desc);

		if (!function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_COALLESCED_MESH_EX_DO_NOT_SCALE_BACK)) {
			// Rescale the meshes to their original size such that on further processing they will be the same
			ScaleGLTFMeshes(gltf_meshes.buffer, gltf_meshes.size, 1.0f / scale_factor);
		}
		
		return mesh;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	CoallescedMesh* ResourceManager::LoadCoallescedMeshImplementationEx(
		const GLTFMesh* gltf_mesh, 
		Stream<Submesh> submeshes,
		float scale_factor,
		ResourceManagerLoadDesc load_descriptor, 
		ResourceManagerExDesc* ex_desc
	)
	{
		// Calculate the allocation size
		size_t allocation_size = sizeof(CoallescedMesh) + sizeof(Submesh) * submeshes.size;
		// Allocate the needed memory
		void* allocation = Allocate(allocation_size);
		CoallescedMesh* mesh = (CoallescedMesh*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(CoallescedMesh);
		mesh->submeshes.InitializeAndCopy(buffer, submeshes);

		ScaleGLTFMeshes(gltf_mesh, 1, scale_factor);

		mesh->mesh = GLTFMeshToMesh(m_graphics, *gltf_mesh);

		AddResourceEx(this, ResourceType::CoallescedMesh, mesh, load_descriptor, ex_desc);

		if (!function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_COALLESCED_MESH_EX_DO_NOT_SCALE_BACK)) {
			// Rescale the meshes to their original size such that on further processing they will be the same
			ScaleGLTFMeshes(gltf_mesh, 1, 1.0f / scale_factor);
		}

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
			ResourceType::PBRMaterial,
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

	bool ResourceManager::LoadUserMaterial(const UserMaterial* user_material, Material* converted_material, ResourceManagerLoadDesc load_descriptor)
	{
		memset(converted_material, 0, sizeof(*converted_material));

		ECS_ASSERT(user_material->vertex_shader.size > 0, "User material needs vertex shader.");
		ECS_ASSERT(user_material->pixel_shader.size > 0, "User material needs pixel shader.");

		bool dont_load = function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_LOAD);

		Stream<char> source_code = { nullptr, 0 };
		Stream<void> byte_code;
		if (dont_load) {
			converted_material->vertex_shader = LoadVertexShaderImplementation(user_material->vertex_shader, &source_code, &byte_code, user_material->vertex_compile_options, load_descriptor);
		}
		else {
			converted_material->vertex_shader = *LoadVertexShader<true>(user_material->vertex_shader, &source_code, &byte_code, user_material->vertex_compile_options, load_descriptor);
		}

		if (converted_material->vertex_shader.shader == nullptr) {
			return false;
		}

		InputLayout input_layout = m_graphics->ReflectVertexShaderInput(source_code, byte_code);
		if (input_layout.layout == nullptr) {
			// Release the source code before
			Deallocate(source_code.buffer);
			return false;
		}
		converted_material->layout = input_layout;

		ECS_STACK_CAPACITY_STREAM(ECS_MESH_INDEX, mesh_indices, ECS_MESH_BUFFER_COUNT);
		m_graphics->ReflectVertexBufferMapping(source_code, mesh_indices);
		
		// The source code now can be released
		Deallocate(source_code.buffer);

		if (dont_load) {
			converted_material->pixel_shader = LoadPixelShaderImplementation(user_material->pixel_shader, nullptr, user_material->pixel_compile_options, load_descriptor);
		}
		else {
			converted_material->pixel_shader = *LoadPixelShader<true>(user_material->pixel_shader, nullptr, user_material->pixel_compile_options, load_descriptor);
		}

		auto deallocate_vertex_shader = [&]() {
			// Deallocate the vertex shader
			if (dont_load) {
				UnloadShaderImplementation(converted_material->vertex_shader.Interface(), ECS_SHADER_VERTEX);
			}
			else {
				UnloadVertexShader<true>(user_material->vertex_shader);
			}
		};

		if (converted_material->pixel_shader.shader == nullptr) {
			deallocate_vertex_shader();
			return false;
		}

		auto deallocate_pixel_shader = [&]() {
			if (dont_load) {
				UnloadShaderImplementation(converted_material->pixel_shader.Interface(), ECS_SHADER_PIXEL);
			}
			else {
				UnloadPixelShader<true>(user_material->pixel_shader);
			}
		};

		for (size_t index = 0; index < user_material->textures.size; index++) {
			ResourceManagerTextureDesc texture_desc;
			texture_desc.context = m_graphics->GetContext();
			texture_desc.compression = user_material->textures[index].compression;
			texture_desc.misc_flags = user_material->textures[index].generate_mips ? ECS_GRAPHICS_MISC_GENERATE_MIPS : ECS_GRAPHICS_MISC_NONE;
			texture_desc.srgb = user_material->textures[index].srgb;

			ResourceManagerLoadDesc manager_desc;
			ECS_STACK_VOID_STREAM(settings_suffix, 512);
			Stream<void> suffix = load_descriptor.identifier_suffix;
			if (user_material->generate_unique_name_from_setting) {
				user_material->textures[index].GenerateSettingsSuffix(settings_suffix);
				suffix = settings_suffix;
			}
			manager_desc.identifier_suffix = suffix;

			ResourceView resource_view;
			if (dont_load) {
				resource_view = LoadTextureImplementation(user_material->textures[index].filename, &texture_desc, manager_desc);
			}
			else {
				resource_view = LoadTexture<true>(user_material->textures[index].filename, &texture_desc, manager_desc);
			}
			if (resource_view.view == nullptr) {
				// Deallocate the shaders
				deallocate_vertex_shader();
				deallocate_pixel_shader();

				// Deallocate all textures before
				converted_material->v_texture_count = 0;
				converted_material->p_texture_count = 0;
				for (size_t subindex = 0; subindex < index; subindex++) {
					if (dont_load) {
						switch (user_material->textures[subindex].shader_type) {
						case ECS_SHADER_VERTEX:
							resource_view = converted_material->v_textures[converted_material->v_texture_count++];
							break;
						case ECS_SHADER_PIXEL:
							resource_view = converted_material->p_textures[converted_material->p_texture_count++];
							break;
						}
						UnloadTextureImplementation(resource_view);
					}
					else {
						suffix = load_descriptor.identifier_suffix;
						if (user_material->generate_unique_name_from_setting) {
							user_material->textures[subindex].GenerateSettingsSuffix(settings_suffix);
							suffix = settings_suffix;
						}
						manager_desc.identifier_suffix = suffix;
						UnloadTexture<true>(user_material->textures[subindex].filename, manager_desc);
					}
				}

				return false;
			}

			switch (user_material->textures[index].shader_type) {
			case ECS_SHADER_VERTEX:
				converted_material->v_textures[converted_material->v_texture_count] = resource_view;
				converted_material->v_texture_slot[converted_material->v_texture_count] = user_material->textures[index].slot;
				converted_material->v_texture_count++;
				break;
			case ECS_SHADER_PIXEL:
				converted_material->p_textures[converted_material->p_texture_count] = resource_view;
				converted_material->p_texture_slot[converted_material->p_texture_count] = user_material->textures[index].slot;
				converted_material->p_texture_count++;
				break;
			}
		}

		for (size_t index = 0; index < user_material->buffers.size; index++) {
			ConstantBuffer buffer;
			ECS_ASSERT(user_material->buffers[index].dynamic || user_material->buffers[index].data.buffer != nullptr);
			if (user_material->buffers[index].dynamic) {
				if (user_material->buffers[index].data.buffer == nullptr) {
					buffer = m_graphics->CreateConstantBuffer(user_material->buffers[index].data.size, user_material->buffers[index].data.buffer);
				}
				else {
					buffer = m_graphics->CreateConstantBuffer(user_material->buffers[index].data.size);
				}
			}
			else {
				buffer = m_graphics->CreateConstantBuffer(user_material->buffers[index].data.size, user_material->buffers[index].data.buffer);
			}

			if (user_material->buffers[index].shader_type == ECS_SHADER_VERTEX) {
				unsigned char v_index = converted_material->v_buffer_count;
				converted_material->v_buffers[v_index] = buffer;
				converted_material->v_buffer_slot[v_index] = user_material->buffers[index].slot;
				converted_material->v_buffer_count++;
			}
			else {
				unsigned char p_index = converted_material->v_buffer_count;
				converted_material->p_buffers[p_index] = buffer;
				converted_material->p_buffer_slot[p_index] = user_material->buffers[index].slot;
				converted_material->p_buffer_count++;
			}
		}

		return true;
	}

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

		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE;
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

	void* LoadShaderInternalImplementation(ResourceManager* manager, Stream<wchar_t> filename, ShaderCompileOptions options, ResourceManagerLoadDesc load_descriptor,
		Stream<char>* shader_source_code, Stream<void>* byte_code, ECS_SHADER_TYPE type) {
		Path path_extension = function::PathExtensionBoth(filename);
		bool is_byte_code = function::CompareStrings(path_extension, L".cso");

		void* shader = nullptr;
		Stream<void> contents = { nullptr, 0 };
		AllocatorPolymorphic allocator_polymorphic = GetAllocatorPolymorphic(manager->m_memory, ECS_ALLOCATION_MULTI);

		if (is_byte_code) {
			ECS_ASSERT(shader_source_code == nullptr, "Cannot retrieve shader source code from binary shader.");
			contents = ReadWholeFileBinary(filename.buffer, allocator_polymorphic);
			shader = manager->m_graphics->CreateShader(filename, type, function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY));
		}
		else {
			Stream<char> source_code = ReadWholeFileText(filename.buffer, allocator_polymorphic);
			ShaderIncludeFiles include(manager->m_memory, manager->m_shader_directory);
			shader = manager->m_graphics->CreateShaderFromSource(
				source_code,
				type, 
				&include,
				options, 
				byte_code,
				function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY)
			);
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

	// Convert from source code to shader
	void* LoadShaderInternalImplementationEx(
		ResourceManager* manager,
		Stream<char> source_code,
		ECS_SHADER_TYPE shader_type,
		Stream<void>* byte_code,
		ShaderCompileOptions options,
		ResourceManagerLoadDesc load_descriptor,
		ResourceManagerExDesc* ex_desc
	) {
		ResourceType type = FromShaderTypeToResourceType(shader_type);

		ShaderIncludeFiles include(manager->m_memory, manager->m_shader_directory);
		void* shader = manager->m_graphics->CreateShaderFromSource(
			source_code,
			shader_type,
			&include,
			options,
			byte_code,
			function::HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY)
		);

		if (ex_desc != nullptr && ex_desc->HasFilename()) {
			ex_desc->Lock();

			void* allocation = manager->Allocate(sizeof(VertexShaderStorage));
			VertexShaderStorage* storage = (VertexShaderStorage*)allocation;
			// Doesn't matter the type here
			storage->shader = (ID3D11VertexShader*)shader;
			storage->source_code = { nullptr, 0 };
			storage->byte_code = { nullptr, 0 };
			manager->AddResource(ex_desc->filename, type, allocation, ex_desc->time_stamp, load_descriptor.identifier_suffix);

			ex_desc->Unlock();
		}

		return shader;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	VertexShader* ResourceManager::LoadVertexShader(
		Stream<wchar_t> filename, 
		Stream<char>* source_code,
		Stream<void>* byte_code,
		ShaderCompileOptions options,
		ResourceManagerLoadDesc load_descriptor
	)
	{
		return (VertexShader*)LoadShader<reference_counted>(filename, ECS_SHADER_COMPUTE, source_code, byte_code, options, load_descriptor);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(VertexShader*, ResourceManager::LoadVertexShader, Stream<wchar_t>, Stream<char>*, Stream<void>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	VertexShader ResourceManager::LoadVertexShaderImplementation(
		Stream<wchar_t> filename, 
		Stream<char>* shader_source_code, 
		Stream<void>* byte_code,
		ShaderCompileOptions options, 
		ResourceManagerLoadDesc load_descriptor
	)
	{
		return VertexShader::FromInterface(LoadShaderImplementation(filename, ECS_SHADER_VERTEX, shader_source_code, byte_code, options, load_descriptor));
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	VertexShader ResourceManager::LoadVertexShaderImplementationEx(
		Stream<char> source_code, 
		Stream<void>* byte_code,
		ShaderCompileOptions options, 
		ResourceManagerLoadDesc load_descriptor, 
		ResourceManagerExDesc* ex_desc
	)
	{
		return VertexShader::FromInterface(LoadShaderImplementationEx(source_code, ECS_SHADER_VERTEX, byte_code, options, load_descriptor, ex_desc));
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
		return (PixelShader*)LoadShader<reference_counted>(filename, ECS_SHADER_PIXEL, shader_source_code, nullptr, options, load_descriptor);
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
		return PixelShader::FromInterface(LoadShaderImplementation(filename, ECS_SHADER_PIXEL, shader_source_code, nullptr, options, load_descriptor));
	}

	PixelShader ResourceManager::LoadPixelShaderImplementationEx(Stream<char> source_code, ShaderCompileOptions options, ResourceManagerLoadDesc load_descriptor, ResourceManagerExDesc* ex_desc)
	{
		return PixelShader::FromInterface(LoadShaderImplementationEx(source_code, ECS_SHADER_PIXEL, nullptr, options, load_descriptor, ex_desc));
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
		return (HullShader*)LoadShader<reference_counted>(filename, ECS_SHADER_HULL, shader_source_code, nullptr, options, load_descriptor);
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
		return HullShader::FromInterface(LoadShaderImplementation(filename, ECS_SHADER_HULL, shader_source_code, nullptr, options, load_descriptor));
	}

	HullShader ResourceManager::LoadHullShaderImplementationEx(Stream<char> source_code, ShaderCompileOptions options, ResourceManagerLoadDesc load_descriptor, ResourceManagerExDesc* ex_desc)
	{
		return HullShader::FromInterface(LoadShaderImplementationEx(source_code, ECS_SHADER_HULL, nullptr, options, load_descriptor, ex_desc));
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
		return (DomainShader*)LoadShader<reference_counted>(filename, ECS_SHADER_DOMAIN, shader_source_code, nullptr, options, load_descriptor);
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
		return DomainShader::FromInterface(LoadShaderImplementation(filename, ECS_SHADER_DOMAIN, shader_source_code, nullptr, options, load_descriptor));
	}

	DomainShader ResourceManager::LoadDomainShaderImplementationEx(Stream<char> source_code, ShaderCompileOptions options, ResourceManagerLoadDesc load_descriptor, ResourceManagerExDesc* ex_desc)
	{
		return DomainShader::FromInterface(LoadShaderImplementationEx(source_code, ECS_SHADER_DOMAIN, nullptr, options, load_descriptor, ex_desc));
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
		return (GeometryShader*)LoadShader<reference_counted>(filename, ECS_SHADER_GEOMETRY, shader_source_code, nullptr, options, load_descriptor);
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
		return GeometryShader::FromInterface(LoadShaderImplementation(filename, ECS_SHADER_GEOMETRY, shader_source_code, nullptr, options, load_descriptor));
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	GeometryShader ResourceManager::LoadGeometryShaderImplementationEx(Stream<char> source_code, ShaderCompileOptions options, ResourceManagerLoadDesc load_descriptor, ResourceManagerExDesc* ex_desc)
	{
		return GeometryShader::FromInterface(LoadShaderImplementationEx(source_code, ECS_SHADER_GEOMETRY, nullptr, options, load_descriptor, ex_desc));
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
		return (ComputeShader*)LoadShader<reference_counted>(filename, ECS_SHADER_COMPUTE, shader_source_code, nullptr, options, load_descriptor);
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
		return ComputeShader::FromInterface(LoadShaderImplementation(filename, ECS_SHADER_COMPUTE, shader_source_code, nullptr, options, load_descriptor));
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	ComputeShader ResourceManager::LoadComputeShaderImplementationEx(Stream<char> source_code, ShaderCompileOptions options, ResourceManagerLoadDesc load_descriptor, ResourceManagerExDesc* ex_desc)
	{
		return ComputeShader::FromInterface(LoadShaderImplementationEx(source_code, ECS_SHADER_COMPUTE, nullptr, options, load_descriptor, ex_desc));
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void* ResourceManager::LoadShader(
		Stream<wchar_t> filename, 
		ECS_SHADER_TYPE shader_type, 
		Stream<char>* shader_source_code,
		Stream<void>* byte_code,
		ShaderCompileOptions options,
		ResourceManagerLoadDesc load_descriptor
	)
	{
		ResourceType type = FromShaderTypeToResourceType(shader_type);

		bool _is_loaded_first_time = false;
		bool* is_loaded_first_time = load_descriptor.reference_counted_is_loaded != nullptr ? load_descriptor.reference_counted_is_loaded : &_is_loaded_first_time;

		// Use as allocation the biggest storage type - that is the one of the vertex shader
		void* shader = LoadResource<reference_counted>(
			this,
			filename,
			type,
			sizeof(VertexShaderStorage),
			load_descriptor,
			[=](void* allocation) {
				VertexShaderStorage* shader = (VertexShaderStorage*)allocation;
				shader->shader = (ID3D11VertexShader*)LoadShaderImplementation(filename, shader_type, shader_source_code, byte_code, options, load_descriptor);

				if (shader_source_code != nullptr) {
					shader->source_code = *shader_source_code;
				}
				else {
					shader->source_code = { nullptr, 0 };
				}

				if (byte_code != nullptr && shader_type == ECS_SHADER_VERTEX) {
					void* byte_code_allocation = Allocate(byte_code->size);
					memcpy(byte_code_allocation, byte_code->buffer, byte_code->size);
					shader->byte_code = *byte_code;
					shader->byte_code.buffer = byte_code_allocation;
				}
				else {
					shader->byte_code = { nullptr, 0 };
				}

				return allocation;
		});

		if constexpr (reference_counted) {
			if (shader_source_code != nullptr || byte_code != nullptr) {
				if (!*is_loaded_first_time) {
					// Need to get the source code or the byte code
					VertexShaderStorage* storage = (VertexShaderStorage*)shader;
					// For byte code we need the source code as well
					if (shader_source_code != nullptr || byte_code != nullptr) {
						if (storage->source_code.size == 0) {
							// Load the file and store it inside the current storage
							Stream<char> source_code = ReadWholeFileText(filename, Allocator());
							storage->source_code = source_code;
						}
						if (shader_source_code != nullptr) {
							*shader_source_code = storage->source_code;
						}
					}
					if (byte_code != nullptr) {
						if (storage->byte_code.size == 0) {
							ShaderIncludeFiles include_files(m_memory, m_shader_directory);
							storage->byte_code = m_graphics->CompileShaderToByteCode(storage->source_code, ECS_SHADER_VERTEX, &include_files, Allocator(), options);
						}
						*byte_code = storage->byte_code;
					}

				}
			}
		}

		return shader;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void*, ResourceManager::LoadShader, Stream<wchar_t>, ECS_SHADER_TYPE, Stream<char>*, Stream<void>*, ShaderCompileOptions, ResourceManagerLoadDesc);

	// ---------------------------------------------------------------------------------------------------------------------------

	void* ResourceManager::LoadShaderImplementation(
		Stream<wchar_t> filename, 
		ECS_SHADER_TYPE shader_type, 
		Stream<char>* shader_source_code, 
		Stream<void>* byte_code,
		ShaderCompileOptions options, 
		ResourceManagerLoadDesc load_descriptor
	)
	{
		return LoadShaderInternalImplementation(this, filename, options, load_descriptor, shader_source_code, byte_code, shader_type);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void* ResourceManager::LoadShaderImplementationEx(
		Stream<char> source_code,
		ECS_SHADER_TYPE shader_type, 
		Stream<void>* byte_code,
		ShaderCompileOptions options, 
		ResourceManagerLoadDesc load_descriptor,
		ResourceManagerExDesc* ex_desc
	)
	{
		return LoadShaderInternalImplementationEx(this, source_code, shader_type, byte_code, options, load_descriptor, ex_desc);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	ResizableStream<void>* ResourceManager::LoadMisc(Stream<wchar_t> filename, AllocatorPolymorphic allocator, ResourceManagerLoadDesc load_descriptor)
	{
		return (ResizableStream<void>*)LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::Misc,
			sizeof(ResizableStream<void>),
			load_descriptor,
			[=](void* allocation) {
				ResizableStream<void>* stream = (ResizableStream<void>*)allocation;
				*stream = LoadMiscImplementation(filename, allocator, load_descriptor);
				return allocation;
		});
	}

	ECS_TEMPLATE_FUNCTION_BOOL(ResizableStream<void>*, ResourceManager::LoadMisc, Stream<wchar_t>, AllocatorPolymorphic, ResourceManagerLoadDesc);

	ResizableStream<void> ResourceManager::LoadMiscImplementation(Stream<wchar_t> filename, AllocatorPolymorphic allocator, ResourceManagerLoadDesc load_descriptor)
	{
		Stream<void> data = ReadWholeFileBinary(filename, allocator);
		ResizableStream<void> resizable_data;
		resizable_data.buffer = data.buffer;
		resizable_data.size = data.size;
		resizable_data.capacity = data.size;
		resizable_data.allocator = allocator;
		return resizable_data;
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

#define EXPORT_UNLOAD(name) ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::name, Stream<wchar_t>, ResourceManagerLoadDesc); ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::name, unsigned int, size_t);

	template<bool reference_counted>
	void ResourceManager::UnloadTextFile(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::TextFile, load_desc);
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
	void ResourceManager::UnloadTexture(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::Texture, load_desc);
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
	void ResourceManager::UnloadMeshes(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc) {
		UnloadResource<reference_counted>(filename, ResourceType::Mesh, load_desc);
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
	void ResourceManager::UnloadCoallescedMesh(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::CoallescedMesh, load_desc);
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
	void ResourceManager::UnloadMaterials(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc) {
		UnloadResource<reference_counted>(filename, ResourceType::PBRMaterial, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadMaterials(unsigned int index, size_t flags) {
		UnloadResource<reference_counted>(index, ResourceType::PBRMaterial, flags);
	}

	void ResourceManager::UnloadMaterialsImplementation(Stream<PBRMaterial>* materials, size_t flags)
	{
		UnloadMaterialsHandler(materials, this);
	}

	EXPORT_UNLOAD(UnloadMaterials);

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::UnloadUserMaterial(const UserMaterial* user_material, Material* material, ResourceManagerLoadDesc load_desc)
	{
		bool dont_load = function::HasFlag(load_desc.load_flags, ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_LOAD);
		if (dont_load) {
			UnloadShaderImplementation(material->vertex_shader.Interface(), ECS_SHADER_VERTEX);
			UnloadShaderImplementation(material->pixel_shader.Interface(), ECS_SHADER_PIXEL);
		}
		else {
			UnloadVertexShader<true>(user_material->vertex_shader, load_desc);
			UnloadPixelShader<true>(user_material->pixel_shader, load_desc);
		}
		m_graphics->FreeResource(material->layout);

		auto remove_entry = [](unsigned char slot, auto* buffers, unsigned char& buffer_count, unsigned char* slots) {
			unsigned char subindex = 0;

			// Weird situation, the compiler keeps asking for a valid initialization for a reference as if
			// entry is a reference when in fact it should be just the type of element from the buffers pointer
			decltype(*buffers) entry = buffers[0];
			for (; subindex < buffer_count; subindex++) {
				if (slots[subindex] == slot) {
					entry = buffers[subindex];
					buffer_count--;
					buffers[subindex] = buffers[buffer_count];
					slots[subindex] = slots[buffer_count];
					return entry;
				}
			}
			ECS_ASSERT(false);
		};

		for (size_t index = 0; index < user_material->textures.size; index++) {
			ResourceManagerLoadDesc manager_desc;
			ECS_STACK_VOID_STREAM(settings_suffix, 512);
			Stream<void> suffix = load_desc.identifier_suffix;
			if (user_material->generate_unique_name_from_setting) {
				user_material->textures[index].GenerateSettingsSuffix(settings_suffix);
				suffix = settings_suffix;
			}
			manager_desc.identifier_suffix = suffix;
			ResourceView resource_view = remove_entry(user_material->textures[index].slot, material->p_textures, material->p_texture_count, material->p_texture_slot);

			if (dont_load) {
				UnloadTextureImplementation(resource_view);
			}
			else {
				UnloadTexture<true>(user_material->textures[index].filename, manager_desc);
			}		
		}

		for (size_t index = 0; index < user_material->buffers.size; index++) {
			ConstantBuffer buffer;
			if (user_material->buffers[index].shader_type == ECS_SHADER_VERTEX) {
				buffer = remove_entry(user_material->buffers[index].slot, material->v_buffers, material->v_buffer_count, material->v_buffer_slot);
			}
			else {
				buffer = remove_entry(user_material->buffers[index].slot, material->p_buffers, material->p_buffer_count, material->p_buffer_slot);
			}

			m_graphics->FreeResource(buffer);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadPBRMesh(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc) {
		UnloadResource<reference_counted>(filename, ResourceType::PBRMesh, load_desc);
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
	void ResourceManager::UnloadVertexShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::VertexShader, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadVertexShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::VertexShader, flags);
	}

	EXPORT_UNLOAD(UnloadVertexShader);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadPixelShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::PixelShader, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadPixelShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::PixelShader, flags);
	}

	EXPORT_UNLOAD(UnloadPixelShader);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadHullShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::HullShader, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadHullShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::HullShader, flags);
	}

	EXPORT_UNLOAD(UnloadHullShader);
	
	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadDomainShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::DomainShader, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadDomainShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::DomainShader, flags);
	}

	EXPORT_UNLOAD(UnloadDomainShader);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadGeometryShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::GeometryShader, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadGeometryShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::GeometryShader, flags);
	}

	EXPORT_UNLOAD(UnloadGeometryShader);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadComputeShader(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::ComputeShader, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadComputeShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::ComputeShader, flags);
	}

	EXPORT_UNLOAD(UnloadComputeShader);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadShader(Stream<wchar_t> filename, ECS_SHADER_TYPE type, ResourceManagerLoadDesc load_desc)
	{
		switch (type) {
		case ECS_SHADER_VERTEX:
			UnloadVertexShader(filename, load_desc);
			break;
		case ECS_SHADER_PIXEL:
			UnloadPixelShader(filename, load_desc);
			break;
		case ECS_SHADER_GEOMETRY:
			UnloadGeometryShader(filename, load_desc);
			break;
		case ECS_SHADER_HULL:
			UnloadHullShader(filename, load_desc);
			break;
		case ECS_SHADER_DOMAIN:
			UnloadDomainShader(filename, load_desc);
			break;
		case ECS_SHADER_COMPUTE:
			UnloadComputeShader(filename, load_desc);
			break;
		default:
			ECS_ASSERT(false);
		}
	}
	
	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadShader, Stream<wchar_t>, ECS_SHADER_TYPE, ResourceManagerLoadDesc);

	template<bool reference_counted>
	void ResourceManager::UnloadShader(unsigned int index, ECS_SHADER_TYPE type, size_t flags)
	{
		switch (type) {
		case ECS_SHADER_VERTEX:
			UnloadVertexShader(index, flags);
			break;
		case ECS_SHADER_PIXEL:
			UnloadPixelShader(index, flags);
			break;
		case ECS_SHADER_GEOMETRY:
			UnloadGeometryShader(index, flags);
			break;
		case ECS_SHADER_HULL:
			UnloadHullShader(index, flags);
			break;
		case ECS_SHADER_DOMAIN:
			UnloadDomainShader(index, flags);
			break;
		case ECS_SHADER_COMPUTE:
			UnloadComputeShader(index, flags);
			break;
		default:
			ECS_ASSERT(false);
		}
	}
	
	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadShader, unsigned int, ECS_SHADER_TYPE, size_t);

	void ResourceManager::UnloadShaderImplementation(void* shader_interface, ECS_SHADER_TYPE type, size_t flags)
	{
		ResourceType resource_type;
		switch (type) {
		case ECS_SHADER_VERTEX:
			resource_type = ResourceType::VertexShader;
			break;
		case ECS_SHADER_PIXEL:
			resource_type = ResourceType::PixelShader;
			break;
		case ECS_SHADER_GEOMETRY:
			resource_type = ResourceType::GeometryShader;
			break;
		case ECS_SHADER_HULL:
			resource_type = ResourceType::HullShader;
			break;
		case ECS_SHADER_DOMAIN:
			resource_type = ResourceType::DomainShader;
			break;
		case ECS_SHADER_COMPUTE:
			resource_type = ResourceType::ComputeShader;
			break;
		default:
			ECS_ASSERT(false);
		}

		UnloadResourceImplementation(shader_interface, resource_type, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadMisc(Stream<wchar_t> filename, ResourceManagerLoadDesc load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::Misc, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadMisc(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::Misc, flags);
	}

	void ResourceManager::UnloadMiscImplementation(ResizableStream<void> data, size_t flags)
	{
		data.FreeBuffer();
	}

	EXPORT_UNLOAD(UnloadMisc);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadResource(Stream<wchar_t> filename, ResourceType type, ResourceManagerLoadDesc load_desc)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		ResourceIdentifier identifier = ResourceIdentifier::WithSuffix(filename, fully_specified_identifier, load_desc.identifier_suffix);
		DeleteResource<reference_counted>(this, identifier, type, load_desc.load_flags);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadResource, Stream<wchar_t>, ResourceType, ResourceManagerLoadDesc);

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

	template<bool reference_counted>
	bool ResourceManager::TryUnloadResource(Stream<wchar_t> filename, ResourceType type, ResourceManagerLoadDesc load_desc)
	{
		unsigned int index = GetResourceIndex(filename, type, load_desc.identifier_suffix);
		if (index != -1) {
			UnloadResource<reference_counted>(index, type, load_desc.load_flags);
			return true;
		}
		return false;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, ResourceManager::TryUnloadResource, Stream<wchar_t>, ResourceType, ResourceManagerLoadDesc);

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

	bool PBRToMaterial(ResourceManager* resource_manager, const PBRMaterial& pbr, Stream<wchar_t> folder_to_search, Material* material)
	{
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
			ECS_TEXTURE_COMPRESSION_EX compression;
		};

		Mapping _mapping[ECS_PBR_MATERIAL_MAPPING_COUNT];
		Stream<Mapping> mappings(_mapping, 0);
		LinearAllocator _temporary_allocator(memory, ECS_KB * 8);
		AllocatorPolymorphic temporary_allocator = GetAllocatorPolymorphic(&_temporary_allocator);

		unsigned int macro_texture_count = 0;

		if (pbr.color_texture.buffer != nullptr && pbr.color_texture.size > 0) {
			shader_macros.Add({ "COLOR_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.color_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_COLOR, ECS_TEXTURE_COMPRESSION_EX_COLOR });
			macro_texture_count++;
		}

		if (pbr.emissive_texture.buffer != nullptr && pbr.emissive_texture.size > 0) {
			shader_macros.Add({ "EMISSIVE_TEXTURE", "" });
			
			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.emissive_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_EMISSIVE, ECS_TEXTURE_COMPRESSION_EX_COLOR });
			macro_texture_count++;
		}

		if (pbr.metallic_texture.buffer != nullptr && pbr.metallic_texture.size > 0) {
			shader_macros.Add({ "METALLIC_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.metallic_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_METALLIC, ECS_TEXTURE_COMPRESSION_EX_GRAYSCALE });
			macro_texture_count++;
		}

		if (pbr.normal_texture.buffer != nullptr && pbr.normal_texture.size > 0) {
			shader_macros.Add({ "NORMAL_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.normal_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_NORMAL, ECS_TEXTURE_COMPRESSION_EX_TANGENT_SPACE_NORMAL });
			macro_texture_count++;
		}

		if (pbr.occlusion_texture.buffer != nullptr && pbr.occlusion_texture.size > 0) {
			shader_macros.Add({ "OCCLUSION_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.occlusion_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_OCCLUSION, ECS_TEXTURE_COMPRESSION_EX_GRAYSCALE });
			macro_texture_count++;
		}

		if (pbr.roughness_texture.buffer != nullptr && pbr.roughness_texture.size > 0) {
			shader_macros.Add({ "ROUGHNESS_TEXTURE", "" });

			Stream<wchar_t> texture = function::StringCopy(temporary_allocator, pbr.roughness_texture);
			mappings.Add({ texture, ECS_PBR_MATERIAL_ROUGHNESS, ECS_TEXTURE_COMPRESSION_EX_GRAYSCALE });
			macro_texture_count++;
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
			return false;
		}

		GraphicsContext* context = resource_manager->m_graphics->GetContext();

		ResourceManagerTextureDesc texture_descriptor;
		texture_descriptor.context = context;
		texture_descriptor.usage = ECS_GRAPHICS_USAGE_DEFAULT;

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
		Stream<void> byte_code;
		material->vertex_shader = resource_manager->LoadVertexShaderImplementation(ECS_VERTEX_SHADER_SOURCE(PBR), &shader_source, &byte_code);
		ECS_ASSERT(material->vertex_shader.shader != nullptr);
		material->layout = resource_manager->m_graphics->ReflectVertexShaderInput(shader_source, byte_code);
		bool success = resource_manager->m_graphics->ReflectVertexBufferMapping(shader_source, vertex_mappings);
		ECS_ASSERT(success);
		memcpy(material->vertex_buffer_mappings, _vertex_mappings, vertex_mappings.size * sizeof(ECS_MESH_INDEX));
		material->vertex_buffer_mapping_count = vertex_mappings.size;
		resource_manager->Deallocate(shader_source.buffer);

		material->pixel_shader = resource_manager->LoadPixelShaderImplementation(ECS_PIXEL_SHADER_SOURCE(PBR), nullptr, options);
		ECS_ASSERT(material->pixel_shader.shader != nullptr);

		material->v_buffers[0] = Shaders::CreatePBRVertexConstants(resource_manager->m_graphics);
		material->v_buffer_slot[0] = 0;
		material->v_buffer_count = 1;

		for (size_t index = 0; index < macro_texture_count; index++) {
			material->p_textures[index] = texture_views[mappings[index].index];
			// The slot is + 3 because of the environment maps
			material->p_texture_slot[index] = 3 + mappings[index].index;
		}
		material->p_texture_count = macro_texture_count;

		material->p_buffers[0] = Shaders::CreatePBRPixelConstants(resource_manager->m_graphics);
		material->p_buffer_slot[0] = 2;
		material->p_buffer_count = 1;

		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

#pragma endregion

}