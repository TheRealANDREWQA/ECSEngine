#include "ecspch.h"
#include "ResourceManager.h"
#include "../Rendering/GraphicsHelpers.h"
#include "../Rendering/Compression/TextureCompression.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../GLTF/GLTFLoader.h"
#include "../Utilities/Path.h"
#include "../Utilities/File.h"
#include "../Utilities/ForEachFiles.h"
#include "../Rendering/ShaderInclude.h"
#include "../Rendering/Shader Application Stage/PBR.h"
#include "../OS/FileOS.h"

#define ECS_RESOURCE_MANAGER_CHECK_RESOURCE

#define ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_INITIAL_SIZE ECS_MB * 2
#define ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_BACKUP_SIZE (ECS_MB * 10)

#define ECS_RESOURCE_MANAGER_MULTITHREADED_ALLOCATOR_CAPACITY ECS_MB
#define ECS_RESOURCE_MANAGER_MULTITHREADED_ALLOCATOR_BACKUP_CAPACITY ECS_MB * 5

#define ECS_RESOURCE_MANAGER_INCLUDE_ALLOCATOR_CAPACITY ECS_KB * 32
#define ECS_RESOURCE_MANAGER_INCLUDE_ALLOCATOR_BACKUP_CAPACITY ECS_MB

namespace ECSEngine {

#define ECS_RESOURCE_MANAGER_INITIAL_LOAD_INCREMENT_COUNT 1

	// The handler must implement a void* parameter for additional info and must return void*
	// If it returns a nullptr, it means the load failed so do not add it to the table
	template<bool reference_counted, typename Handler>
	static void* LoadResource(
		ResourceManager* resource_manager,
		Stream<wchar_t> path,
		ResourceType type,
		const ResourceManagerLoadDesc& load_descriptor,
		Handler&& handler
	) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_name, 512);

		unsigned int type_int = (unsigned int)type;		
		ResourceIdentifier identifier = ResourceIdentifier::WithSuffix(path, fully_specified_name, load_descriptor.identifier_suffix);

		auto register_resource = [=](void* data, unsigned short reference_count) {
			// Account for the L'\0'
			void* allocation = Copy(resource_manager->Allocator(), identifier.ptr, identifier.size + 2);

			ResourceManagerEntry entry;
			entry.data = data;
			entry.reference_count = reference_count;
			entry.time_stamp = OS::GetFileLastWrite(path);
			entry.suffix_size = load_descriptor.identifier_suffix.size;
			entry.is_protected = false;

			ResourceIdentifier new_identifier(allocation, identifier.size);
			resource_manager->m_resource_types[type_int].InsertDynamic(resource_manager->m_memory, entry, new_identifier);
		};

		if constexpr (!reference_counted) {
#ifdef ECS_RESOURCE_MANAGER_CHECK_RESOURCE
			bool exists = resource_manager->Exists(identifier, type);
			ECS_ASSERT(!exists, "The resource already exists!");
#endif
			void* data = handler();
			if (data != nullptr) {
				register_resource(data, USHORT_MAX);
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
					register_resource(data, increment_count);

					if (load_descriptor.reference_counted_is_loaded != nullptr) {
						*load_descriptor.reference_counted_is_loaded = true;
					}
				}
				return data;
			}
			else {
				resource_manager->IncrementReferenceCount(type, table_index, increment_count);

				if (load_descriptor.reference_counted_is_loaded != nullptr) {
					*load_descriptor.reference_counted_is_loaded = false;
				}
				return resource_manager->GetResourceFromIndex(table_index, type);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// The handler must implement a void* parameter for additional info and a void* for its allocation and must return void*
	template<bool reference_counted, typename Handler>
	static void* LoadResource(
		ResourceManager* resource_manager,
		Stream<wchar_t> path,
		ResourceType type,
		size_t allocation_size,
		const ResourceManagerLoadDesc& load_descriptor,
		Handler&& handler
	) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);

		unsigned int type_int = (unsigned int)type;
		ResourceIdentifier identifier = ResourceIdentifier::WithSuffix(path, fully_specified_identifier, load_descriptor.identifier_suffix);

		auto register_resource = [=](unsigned short initial_increment) {
			// plus 7 bytes for padding and account for the L'\0'
			void* allocation = resource_manager->m_memory->Allocate(allocation_size + identifier.size + sizeof(wchar_t) + 7);
			ECS_ASSERT(allocation != nullptr, "Allocating memory for a resource failed");
			memcpy(allocation, identifier.ptr, identifier.size + sizeof(wchar_t));

			void* handler_allocation = (void*)AlignPointer((uintptr_t)allocation + identifier.size + sizeof(wchar_t), 8);

			memset(handler_allocation, 0, allocation_size);
			void* data = handler(handler_allocation);

			if (data != nullptr) {
				ResourceManagerEntry entry;
				entry.data = data;
				entry.reference_count = initial_increment;
				entry.time_stamp = OS::GetFileLastWrite(path);
				entry.suffix_size = load_descriptor.identifier_suffix.size;
				entry.is_protected = false;

				ResourceIdentifier new_identifier(allocation, identifier.size);
				resource_manager->m_resource_types[type_int].InsertDynamic(resource_manager->m_memory, entry, new_identifier);

				if (load_descriptor.reference_counted_is_loaded != nullptr) {
					*load_descriptor.reference_counted_is_loaded = true;
				}
			}
			// The load failed, release the allocation
			else {
				resource_manager->m_memory->Deallocate(allocation);
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
			resource_manager->IncrementReferenceCount(type, table_index, increment_count);

			if (load_descriptor.reference_counted_is_loaded != nullptr) {
				*load_descriptor.reference_counted_is_loaded = false;
			}

			return resource_manager->GetResourceFromIndex(table_index, type);
		}

		// This path is never taken - it will fall into the else branch
		return nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	static void DeleteResource(ResourceManager* resource_manager, unsigned int index, ResourceType type, size_t flags) {
		unsigned int type_int = (unsigned int)type;

		ResourceManagerEntry* entry = resource_manager->m_resource_types[type_int].GetValuePtrFromIndex(index);
		ECS_ASSERT_FORMAT(!entry->is_protected, "ResourceManager: Trying to unload protected resource {#} of type {#}.", resource_manager->m_resource_types[type_int].GetIdentifierFromIndex(index), ResourceTypeString(type));

		UnloadFunction handler = UNLOAD_FUNCTIONS[type_int];

		auto delete_resource = [=]() {
			handler(entry->data, resource_manager, entry->multithreaded_allocation);
			ResourceIdentifier identifier = resource_manager->m_resource_types[type_int].GetIdentifierFromIndex(index);
			resource_manager->m_memory->Deallocate(identifier.ptr);
			resource_manager->m_resource_types[type_int].EraseFromIndex(index);
		};
		if constexpr (!reference_counted) {
			delete_resource();
		}
		else {
			unsigned short increment_count = flags & ECS_RESOURCE_MANAGER_MASK_INCREMENT_COUNT;
			entry->reference_count = SaturateSub(entry->reference_count, increment_count);
			if (entry->reference_count == 0) {
				delete_resource();
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	static void DeleteResource(ResourceManager* resource_manager, ResourceIdentifier identifier, ResourceType type, size_t flags) {
		unsigned int type_int = (unsigned int)type;

		unsigned int hashed_position = resource_manager->m_resource_types[type_int].Find(identifier);
		ECS_ASSERT(hashed_position != -1, "Trying to delete a resource that has not yet been loaded!");

		DeleteResource<reference_counted>(resource_manager, hashed_position, type, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	typedef void (*DeleteFunction)(ResourceManager* resource_manager, unsigned int index, size_t flags);
	typedef void (*UnloadFunction)(void* parameter, ResourceManager* resource_manager, bool multithreaded_allocation);

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadTextureHandler(void* parameter, ResourceManager* resource_manager, bool multithreaded_allocation) {
		ID3D11ShaderResourceView* reinterpretation = (ID3D11ShaderResourceView*)parameter;
		resource_manager->m_graphics->FreeView(ResourceView(reinterpretation));
	}

	static void DeleteTexture(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadTexture<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadTextFileHandler(void* parameter, ResourceManager* resource_manager, bool multithreaded_allocation) { Deallocate(multithreaded_allocation ? resource_manager->AllocatorTs() : resource_manager->Allocator(), parameter); }

	static void DeleteTextFile(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadTextFile<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadMeshesHandler(void* parameter, ResourceManager* resource_manager, bool multithreaded_allocation) {
		Stream<Mesh>* data = (Stream<Mesh>*)parameter;
		for (size_t index = 0; index < data->size; index++) {
			resource_manager->m_graphics->FreeMesh(data->buffer[index]);
		}
	}

	static void DeleteMeshes(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadMeshes<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadMaterialsHandler(void* parameter, ResourceManager* resource_manager, bool multithreaded_allocation) {
		Stream<PBRMaterial>* data = (Stream<PBRMaterial>*)parameter;
		AllocatorPolymorphic allocator = multithreaded_allocation ? resource_manager->AllocatorTs() : resource_manager->AllocatorTs();

		for (size_t index = 0; index < data->size; index++) {
			FreePBRMaterial(data->buffer[index], allocator);
		}
	}

	static void DeleteMaterials(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadMaterials(index, flags);
	}
	
	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadPBRMeshHandler(void* parameter, ResourceManager* resource_manager, bool multithreaded_allocation) {
		PBRMesh* data = (PBRMesh*)parameter;
		AllocatorPolymorphic allocator = multithreaded_allocation ? resource_manager->AllocatorTs() : resource_manager->Allocator();

		// The mesh must be released, alongside the pbr materials
		for (size_t index = 0; index < data->mesh.submeshes.size; index++) {
			FreePBRMaterial(data->materials[index], allocator);
		}
		FreeCoalescedMesh(resource_manager->m_graphics, &data->mesh, true, allocator);
	}

	static void DeletePBRMesh(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadPBRMesh<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadCoalescedMeshHandler(void* parameter, ResourceManager* resource_manager, bool multithreaded_allocation) {
		// Free the coalesced mesh - the submeshes get the deallocated at the same time as this resource
		CoalescedMesh* mesh = (CoalescedMesh*)parameter;
		FreeCoalescedMesh(resource_manager->m_graphics, mesh, true, multithreaded_allocation ? resource_manager->AllocatorTs() : resource_manager->Allocator());
	}

	static void DeleteCoalescedMesh(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadCoalescedMesh<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadShaderHandler(void* parameter, ResourceManager* resource_manager, bool multithreaded_allocation) {
		VertexShaderStorage* storage = (VertexShaderStorage*)parameter;
		resource_manager->m_graphics->FreeResource(storage->shader);
		AllocatorPolymorphic allocator = multithreaded_allocation ? resource_manager->AllocatorTs() : resource_manager->Allocator();
		storage->source_code.Deallocate(allocator);
		storage->byte_code.Deallocate(allocator);
	}

	static void DeleteShader(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadShader<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadMisc(void* parameter, ResourceManager* resource_manager, bool multithreaded_allocation) {
		ResizableStream<void>* data = (ResizableStream<void>*)parameter;
		data->FreeBuffer();
	}

	static void DeleteMisc(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->UnloadMisc<false>(index, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// These are just placeholders - they shouldn't be called
	static void UnloadTimeStamp(void* parameter, ResourceManager* resource_manager, bool multithreaded_allocation) {}

	static void DeleteTimeStamp(ResourceManager* manager, unsigned int index, size_t flags) {
		manager->EvictResource(index, ResourceType::TimeStamp);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	constexpr DeleteFunction DELETE_FUNCTIONS[] = {
		DeleteTexture, 
		DeleteTextFile,
		DeleteMeshes,
		DeleteCoalescedMesh,
		DeleteMaterials,
		DeletePBRMesh,
		DeleteShader,
		DeleteMisc,
		DeleteTimeStamp
	};
	
	constexpr UnloadFunction UNLOAD_FUNCTIONS[] = {
		UnloadTextureHandler,
		UnloadTextFileHandler,
		UnloadMeshesHandler,
		UnloadCoalescedMeshHandler,
		UnloadMaterialsHandler,
		UnloadPBRMeshHandler,
		UnloadShaderHandler,
		UnloadMisc,
		UnloadTimeStamp
	};

	static_assert(ECS_COUNTOF(DELETE_FUNCTIONS) == (unsigned int)ResourceType::TypeCount);
	static_assert(ECS_COUNTOF(UNLOAD_FUNCTIONS) == (unsigned int)ResourceType::TypeCount);

	// ---------------------------------------------------------------------------------------------------------------------------

	ECS_INLINE static Stream<void> GetResourceEntrySuffix(const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
		return { OffsetPointer(identifier.ptr, identifier.size - entry.suffix_size), entry.suffix_size };
	}

	ECS_INLINE static ResourceIdentifier GetResourceEntryIdentifierWithoutSuffix(const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
		return { identifier.ptr, identifier.size - entry.suffix_size };
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void AddResourceEx(ResourceManager* resource_manager, ResourceType type, void* data, bool multithreaded_allocation, const ResourceManagerLoadDesc& load_descriptor, const ResourceManagerExDesc* ex_desc) {
		if (ex_desc != nullptr && ex_desc->HasFilename()) {
			ex_desc->Lock(resource_manager);
			__try {
				resource_manager->AddResource(ex_desc->filename, type, data, multithreaded_allocation, ex_desc->time_stamp, load_descriptor.identifier_suffix, ex_desc->reference_count);
			}
			__finally {
				// Always release the lock in case of a crash
				ex_desc->Unlock(resource_manager);
			}
		}
	}

	AllocatorPolymorphic LoadShaderExtraInformation::GetAllocator(ResourceManager* resource_manager, bool multithreaded_allocator) const {
		return allocator.allocator != nullptr ? allocator : (multithreaded_allocator ? resource_manager->AllocatorTs() : resource_manager->Allocator());
	}

	static void LoadShaderExtraInformationHandleInputLayout(
		void* shader_interface, 
		Stream<char> source_code, 
		Stream<void> byte_code, 
		ECS_SHADER_TYPE shader_type, 
		ResourceManager* resource_manager, 
		const LoadShaderExtraInformation& extra_information
	) {
		if (shader_interface != nullptr && shader_type == ECS_SHADER_VERTEX && extra_information.input_layout != nullptr && byte_code.size > 0) {
			*extra_information.input_layout = resource_manager->m_graphics->ReflectVertexShaderInput(source_code, byte_code);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceManager::ResourceManager(
		ResourceManagerAllocator* memory,
		Graphics* graphics
	) : m_memory(memory), m_graphics(graphics) {
		m_resource_types.Initialize(m_memory, (unsigned int)ResourceType::TypeCount);
		// Set to 0 the initial data for these resource types such that if a type is not being constructed but it is
		// accessed it will cause a crash
		memset(m_resource_types.buffer, 0, m_resource_types.MemoryOf(m_resource_types.size));

		m_shader_directory.Initialize(m_memory, 0);

		// Set the initial shader directory
		AddShaderDirectory(ECS_SHADER_DIRECTORY);

		// Initialize the multithreaded allocator with a small size
		m_multithreaded_allocator = MemoryManager(ECS_RESOURCE_MANAGER_MULTITHREADED_ALLOCATOR_CAPACITY, ECS_KB, ECS_RESOURCE_MANAGER_MULTITHREADED_ALLOCATOR_BACKUP_CAPACITY, m_memory);

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
	void ResourceManager::AddResource(
		ResourceIdentifier identifier, 
		ResourceType resource_type, 
		void* resource,
		bool multithreaded_allocation,
		size_t time_stamp, 
		Stream<void> suffix, 
		unsigned short reference_count
	)
	{
		ResourceManagerEntry entry;
		entry.data = resource;
		entry.reference_count = reference_count;
		entry.time_stamp = time_stamp;
		entry.suffix_size = suffix.size;
		entry.is_protected = false;
		entry.multithreaded_allocation = multithreaded_allocation;

		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		identifier = ResourceIdentifier::WithSuffix(identifier, fully_specified_identifier, suffix);

		// The identifier needs to be allocated, account for the L'\0'
		void* allocation = m_memory->Allocate(identifier.size + sizeof(wchar_t), alignof(wchar_t));
		memcpy(allocation, identifier.ptr, identifier.size);
		identifier.ptr = allocation;
		wchar_t* wide_allocation = (wchar_t*)allocation;
		wide_allocation[identifier.size] = L'\0';

		unsigned int resource_type_int = (unsigned int)resource_type;
		m_resource_types[resource_type_int].InsertDynamic(m_memory, entry, identifier);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::AddTimeStamp(ResourceIdentifier identifier, size_t time_stamp, bool reference_counted, Stream<void> suffix)
	{
		if (reference_counted) {
			// Check to see if it exists and increment it if it does
			unsigned int index = GetResourceIndex(identifier, ResourceType::TimeStamp, suffix);
			if (index == -1) {
				time_stamp = time_stamp == 0 ? OS::GetFileLastWrite(identifier.AsWide()) : time_stamp;
				AddResource(identifier, ResourceType::TimeStamp, nullptr, false, time_stamp, suffix, 1);
				return true;
			}
			else {
				IncrementReferenceCount(ResourceType::TimeStamp, index, 1);
			}

		}
		else {
			time_stamp = time_stamp == 0 ? OS::GetFileLastWrite(identifier.AsWide()) : time_stamp;
			AddResource(identifier, ResourceType::TimeStamp, nullptr, false, time_stamp, suffix);
		}
		return false;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::AddShaderDirectory(Stream<wchar_t> directory)
	{
		m_shader_directory.Add(StringCopy(Allocator(), directory));
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	size_t ResourceManager::ChangeTimeStamp(ResourceIdentifier identifier, ResourceType resource_type, size_t new_time_stamp, Stream<void> suffix)
	{
		ResourceManagerEntry* entry = GetEntryPtr(identifier, resource_type, suffix);
		ECS_ASSERT(entry != nullptr);
		size_t old_time_stamp = entry->time_stamp;
		entry->time_stamp = new_time_stamp;
		return old_time_stamp;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool delete_if_zero>
	void ResourceManager::DecrementReferenceCounts(ResourceType type, unsigned short amount)
	{
		unsigned int type_int = (unsigned int)type;
		m_resource_types[type_int].ForEachIndex([&](unsigned int index) {
			ResourceManagerEntry* entry = m_resource_types[type_int].GetValuePtrFromIndex(index);
			ECS_ASSERT_FORMAT(!entry->is_protected, "ResourceManager: Trying to decrement reference count for protected resource {#} of type {#} "
				"during a decrement call for all entries.", m_resource_types[type_int].GetIdentifierFromIndex(index), ResourceTypeString(type));
			unsigned short reference_count = entry->reference_count;

			if (reference_count != USHORT_MAX) {
				entry->reference_count = SaturateSub(entry->reference_count, amount);
				if constexpr (delete_if_zero) {
					if (entry->reference_count == 0) {
						DELETE_FUNCTIONS[type_int](this, index, 1);
						return true;
					}
				}
			}

			return false;
		});
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::DecrementReferenceCounts, ResourceType, unsigned short);

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::DecrementReferenceCount(ResourceType type, unsigned int resource_index, unsigned short amount)
	{
		ResourceManagerEntry* entry = m_resource_types[(unsigned int)type].GetValuePtrFromIndex(resource_index);
		ECS_ASSERT_FORMAT(!entry->is_protected, "ResourceManager: Trying to decrement the reference count of protected resource {#} of type {#}.",
			m_resource_types[(unsigned int)type].GetIdentifierFromIndex(resource_index), ResourceTypeString(type));
		entry->reference_count = SaturateSub(entry->reference_count, amount);
		return entry->reference_count == 0;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	unsigned int ResourceManager::FindResourceFromPointer(void* resource, ResourceType type) const
	{
		unsigned int final_resource_index = -1;
		ForEachResourceIndex<true>(type, [&](unsigned int resource_index) {
			if (resource == GetResourceFromIndex(resource_index, type)) {
				final_resource_index = resource_index;
				return true;
			}
			return false;
		});

		return final_resource_index;
	}

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
		ECS_ASSERT_FORMAT(table_index != -1, "ResourceManager: Trying to evict resource {#} of type {#} that doesn't exist.", identifier, ResourceTypeString(type));

		EvictResource(table_index, type);;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::EvictResource(unsigned int index, ResourceType type)
	{
		unsigned int type_int = (unsigned int)type;

		ResourceManagerEntry entry = m_resource_types[type_int].GetValueFromIndex(index);
		ResourceIdentifier identifier = m_resource_types[type_int].GetIdentifierFromIndex(index);
		ECS_ASSERT_FORMAT(!entry.is_protected, "ResourceManager: Trying to evict protected resource {#} of type {#}.", identifier, ResourceTypeString(type));

		Deallocate(Allocator(), identifier.ptr);
		m_resource_types[type_int].EraseFromIndex(index);
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

	void ResourceManager::EvictOutdatedResources(ResourceType type, EvictOutdatedResourcesOptions* options)
	{
		unsigned int int_type = (unsigned int)type;
		// Iterate through all resources and check their new stamps
		m_resource_types[int_type].ForEachIndex([&](unsigned int index) {
			// Get the new stamp
			ResourceIdentifier identifier = m_resource_types[int_type].GetIdentifierFromIndex(index);
			size_t new_stamp = OS::GetFileLastWrite({ identifier.ptr, identifier.size / sizeof(wchar_t) });
			if (new_stamp > m_resource_types[int_type].GetValueFromIndex(index).time_stamp) {
				if (options->removed_identifiers.IsInitialized()) {
					options->removed_identifiers.Add(identifier.Copy(options->allocator));
				}
				if (options->removed_values.IsInitialized()) {
					options->removed_values.Add(m_resource_types[int_type].GetValueFromIndex(index).data);
				}

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
		return GetEntry(identifier, type, suffix).data;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void* ResourceManager::GetResourceFromIndex(unsigned int index, ResourceType type) const
	{
		return m_resource_types[(unsigned int)type].GetValueFromIndex(index).data;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceIdentifier ResourceManager::GetResourceIdentifierFromIndex(unsigned int index, ResourceType type) const
	{
		return m_resource_types[(unsigned int)type].GetIdentifierFromIndex(index);
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
			return { nullptr, (size_t)-1, 0, 0 };
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

	ResourceManagerSnapshot ResourceManager::GetSnapshot(AllocatorPolymorphic snapshot_allocator) const
	{
		ResourceManagerSnapshot snapshot;

		size_t total_size_to_allocate = 0;
		// Determine the total byte size first
		for (unsigned int index = 0; index < (unsigned int)ResourceType::TypeCount; index++) {
			total_size_to_allocate += snapshot.resources[0].MemoryOf(m_resource_types[index].GetCount()) + alignof(void*);
			m_resource_types[index].ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
				total_size_to_allocate += identifier.size;
			});
		}

		void* allocation = ECSEngine::Allocate(snapshot_allocator, total_size_to_allocate);
		uintptr_t ptr = (uintptr_t)allocation;
		for (unsigned int index = 0; index < (unsigned int)ResourceType::TypeCount; index++) {
			ptr = AlignPointer(ptr, alignof(ResourceManagerSnapshot::Resource));
			snapshot.resources[index].InitializeFromBuffer(ptr, m_resource_types[index].GetCount());
			snapshot.resources[index].size = 0;
			m_resource_types[index].ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
				identifier = identifier.CopyTo(ptr);
				snapshot.resources[index].Add({ entry.data, identifier, entry.reference_count, entry.suffix_size });
			});
		}

		return snapshot;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	unsigned int ResourceManager::GetReferenceCount(ResourceType type, unsigned int resource_index) const
	{
		const ResourceManagerEntry* entry = m_resource_types[(unsigned int)type].GetValuePtrFromIndex(resource_index);
		return entry->reference_count;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	unsigned int ResourceManager::GetResourceCount(ResourceType type) const
	{
		return m_resource_types[(unsigned int)type].GetCount();
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

	void ResourceManager::IncrementReferenceCount(ResourceType type, unsigned int resource_index, unsigned short amount)
	{
		ResourceManagerEntry* entry = m_resource_types[(unsigned int)type].GetValuePtrFromIndex(resource_index);
		ECS_ASSERT_FORMAT(!entry->is_protected, "ResourceManager: Trying to increment the reference count of protected resource {#} of type {#}.", 
			m_resource_types[(unsigned int)type].GetIdentifierFromIndex(resource_index), ResourceTypeString(type));
		entry->reference_count = SaturateAdd(entry->reference_count, amount);
		if (entry->reference_count == USHORT_MAX) {
			entry->reference_count = USHORT_MAX - 1;
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::IncrementReferenceCounts(ResourceType type, unsigned short amount)
	{
		unsigned int type_int = (unsigned int)type;
		m_resource_types[type_int].ForEach([&](ResourceManagerEntry& entry, ResourceIdentifier identifier) {
			ECS_ASSERT_FORMAT(!entry.is_protected, "ResourceManager: Trying to increment the reference count of protected resource {#} of type {#} during a call to increment"
				" the reference count for all entries.", identifier, ResourceTypeString(type));
			entry.reference_count = SaturateAdd(entry.reference_count, amount);
			if (entry.reference_count == USHORT_MAX) {
				entry.reference_count = USHORT_MAX - 1;
			}
		});
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::InheritResources(const ResourceManager* other)
	{
		for (size_t index = 0; index < (unsigned int)ResourceType::TypeCount; index++) {
			auto& table = other->m_resource_types[index];

			// TODO: Determine what to do about the element's allocations - they might need to be copied over to this instance's allocator
			unsigned int other_capacity = table.GetCapacity();
			if (other_capacity > 0) {
				// Now insert all the resources
				if (other->m_graphics == m_graphics) {
					// Can just reference the resource
					table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
						AddResource(
							GetResourceEntryIdentifierWithoutSuffix(entry, identifier), 
							(ResourceType)index, 
							entry.data, 
							false, 
							entry.time_stamp, 
							GetResourceEntrySuffix(entry, identifier), 
							entry.reference_count
						);
					});
				}
				else {
					auto add_entry = [this](void* data, size_t resource_index, const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
						AddResource(
							GetResourceEntryIdentifierWithoutSuffix(entry, identifier),
							(ResourceType)resource_index,
							data,
							false,
							entry.time_stamp,
							GetResourceEntrySuffix(entry, identifier),
							entry.reference_count
						);
					};

					switch ((ResourceType)index) {
					case ResourceType::TextFile:
					case ResourceType::PBRMaterial:
						// These types don't need to be transferred on the other graphics object
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							add_entry(entry.data, index, entry, identifier);
						});
						break;
					case ResourceType::Shader:
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							// TODO: Transfer the shader
							add_entry(entry.data, index, entry, identifier);
						});
						break;
					case ResourceType::CoalescedMesh:
						// Need to copy the mesh buffers
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							CoalescedMesh* mesh = (CoalescedMesh*)entry.data;
							CoalescedMesh new_mesh = m_graphics->TransferCoalescedMesh(mesh);
							add_entry(&new_mesh, index, entry, identifier);
						});
						break;
					case ResourceType::Mesh:
						// Need to copy the mesh buffers
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							Mesh* mesh = (Mesh*)entry.data;
							Mesh new_mesh = m_graphics->TransferMesh(mesh);
							add_entry(&new_mesh, index, entry, identifier);
						});
						break;
					case ResourceType::PBRMesh:
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							PBRMesh* pbr_mesh = (PBRMesh*)entry.data;
							PBRMesh new_mesh = m_graphics->TransferPBRMesh(pbr_mesh);
							add_entry(&new_mesh, index, entry, identifier);
						});
						break;
					case ResourceType::Texture:
						table.ForEachConst([&](const ResourceManagerEntry& entry, ResourceIdentifier identifier) {
							ResourceView view = (ID3D11ShaderResourceView*)entry.data;
							ResourceView new_view = TransferGPUView(view, m_graphics->GetDevice());
							add_entry(new_view.view, index, entry, identifier);
						});
						break;
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	Stream<char>* ResourceManager::LoadTextFile(
		Stream<wchar_t> filename,
		const ResourceManagerLoadDesc& load_descriptor
	)
	{
		return (Stream<char>*)LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::TextFile,
			sizeof(Stream<char>),
			load_descriptor,
			[=](void* allocation) {
				Stream<char> data = LoadTextFileImplementation(filename, false);
				if (data.buffer != nullptr) {
					Stream<char>* stream = (Stream<char>*)allocation;
					*stream = data;
				}
				return data.buffer;
			}
		);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Stream<char>*, ResourceManager::LoadTextFile, Stream<wchar_t>, const ResourceManagerLoadDesc&);

	// ---------------------------------------------------------------------------------------------------------------------------

	Stream<char> ResourceManager::LoadTextFileImplementation(Stream<wchar_t> file, bool multithreaded_allocation)
	{
		return ReadWholeFileText(file, multithreaded_allocation ? AllocatorTs() : Allocator());
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	ResourceView ResourceManager::LoadTexture(
		Stream<wchar_t> filename,
		const ResourceManagerTextureDesc* descriptor,
		const ResourceManagerLoadDesc& load_descriptor
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

	ECS_TEMPLATE_FUNCTION_BOOL(ResourceView, ResourceManager::LoadTexture, Stream<wchar_t>, const ResourceManagerTextureDesc*, const ResourceManagerLoadDesc&);

	// ---------------------------------------------------------------------------------------------------------------------------

	ResourceView ResourceManager::LoadTextureImplementation(
		Stream<wchar_t> filename,
		const ResourceManagerTextureDesc* descriptor,
		const ResourceManagerLoadDesc& load_descriptor
	)
	{
		NULL_TERMINATE_WIDE(filename);

		// Determine the texture extension - HDR textures must take a different path
		Stream<wchar_t> texture_path = filename;
		Path extension = PathExtensionBoth(texture_path);
		ECS_ASSERT(extension.size > 0, "No extension could be identified");

		bool is_hdr_texture = extension == L".hdr";
		bool is_tga_texture = extension == L".tga";

		HRESULT result;
		
		void* allocator = nullptr;
		if (descriptor->allocator.allocator != nullptr) {
			allocator = descriptor->allocator.allocator;
		}

		DirectX::ScratchImage image;
		SetInternalImageAllocator(&image, descriptor->allocator);
		if (is_hdr_texture) {
			bool apply_tonemapping = HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEXTURE_HDR_TONEMAP);
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
		const ResourceManagerLoadDesc& load_descriptor,
		const ResourceManagerExDesc* ex_desc
	)
	{
		ResourceView texture_view;
		texture_view.view = nullptr;

		bool is_compression = descriptor->compression != ECS_TEXTURE_COMPRESSION_EX_NONE;
		bool temporary = HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY);

		if (is_compression) {
			DirectX::ScratchImage image;

			DirectX::Image new_image;
			new_image.pixels = (uint8_t*)decoded_texture.data.buffer;
			new_image.width = decoded_texture.width;
			new_image.height = decoded_texture.height;
			new_image.format = GetGraphicsNativeFormat(decoded_texture.format);
			ECS_ASSERT(!FAILED(DirectX::ComputePitch(new_image.format, new_image.width, new_image.height, new_image.rowPitch, new_image.slicePitch)));

			if (HasFlag(descriptor->misc_flags, ECS_GRAPHICS_MISC_GENERATE_MIPS) || descriptor->context != nullptr) {
				void* new_allocation = Malloc(new_image.slicePitch);
				HRESULT result = S_OK;
				__try {
					memcpy(new_allocation, new_image.pixels, new_image.slicePitch);
					new_image.pixels = (uint8_t*)new_allocation;
					result = DirectX::GenerateMipMaps(new_image, DirectX::TEX_FILTER_LINEAR, 0, image);
				}
				__finally {
					// Always deallocate this allocation, such that we don't leak it in case of a crash, since this can be quite large
					Free(new_allocation);
				}

				if (FAILED(result)) {
					return nullptr;
				}
			}

			ECS_STACK_CAPACITY_STREAM(Stream<void>, data, 64);
			data.AssertCapacity(image.GetImageCount());
			const auto* images = image.GetImages();
			for (size_t index = 0; index < image.GetImageCount(); index++) {
				data[index] = { images[index].pixels, images[index].slicePitch };
			}
			data.size = image.GetImageCount();

			CompressTextureDescriptor compress_descriptor;
			compress_descriptor.gpu_lock = load_descriptor.gpu_lock;
			if (descriptor->srgb) {
				compress_descriptor.flags |= ECS_TEXTURE_COMPRESS_SRGB;
			}
			Texture2D texture = CompressTexture(m_graphics, data, images[0].width, images[0].height, descriptor->compression, temporary, compress_descriptor);
			if (texture.Interface() == nullptr) {
				return nullptr;
			}
			texture_view = m_graphics->CreateTextureShaderViewResource(texture, temporary);
		}
		else {
			if (descriptor->context != nullptr) {
				// Lock the gpu lock, if any
				load_descriptor.GPULock();
				__try {
					texture_view = m_graphics->CreateTextureWithMips(decoded_texture.data, decoded_texture.format, { decoded_texture.width, decoded_texture.height }, temporary);
				}
				__finally {
					// Perform the unlock even when faced with a crash
					load_descriptor.GPUUnlock();
				}
			}
			else {
				Texture2DDescriptor graphics_descriptor;
				graphics_descriptor.size = { decoded_texture.width, decoded_texture.height };
				graphics_descriptor.mip_data = { &decoded_texture.data, 1 };
				graphics_descriptor.mip_levels = 1;
				graphics_descriptor.format = decoded_texture.format;
				graphics_descriptor.misc_flag = descriptor->misc_flags;
				graphics_descriptor.usage = descriptor->usage;

				Texture2D texture = m_graphics->CreateTexture(&graphics_descriptor, temporary);
				texture_view = m_graphics->CreateTextureShaderViewResource(texture, temporary);
			}
		}

		AddResourceEx(this, ResourceType::Texture, texture_view.view, false, load_descriptor, ex_desc);
		return texture_view;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes from a gltf file
	template<bool reference_counted>
	Stream<Mesh>* ResourceManager::LoadMeshes(
		Stream<wchar_t> filename,
		float scale_factor,
		const ResourceManagerLoadDesc& load_descriptor
	) {
		void* meshes = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::Mesh,
			load_descriptor,
			[=]() {
				return LoadMeshImplementation(filename, false, scale_factor, load_descriptor);
		});

		return (Stream<Mesh>*)meshes;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Stream<Mesh>*, ResourceManager::LoadMeshes, Stream<wchar_t>, float, const ResourceManagerLoadDesc&);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes from a gltf file
	Stream<Mesh>* ResourceManager::LoadMeshImplementation(Stream<wchar_t> filename, bool multithreaded_allocation, float scale_factor, const ResourceManagerLoadDesc& load_descriptor) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, _path, 512);
		GLTFData data = LoadGLTFFile(filename);
		// The load failed
		if (data.data == nullptr) {
			return nullptr;
		}

		// This call should not insert it
		Stream<Mesh>* meshes = LoadMeshImplementationEx(&data, multithreaded_allocation, scale_factor, load_descriptor);

		// Free the gltf file data and the gltf meshes
		FreeGLTFFile(data);
		return meshes;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	Stream<Mesh>* ResourceManager::LoadMeshImplementationEx(const GLTFData* data, bool multithreaded_allocation, float scale_factor, const ResourceManagerLoadDesc& load_descriptor, const ResourceManagerExDesc* ex_desc)
	{
		ECS_ASSERT(data->mesh_count < 200);

		GLTFMesh* gltf_meshes = (GLTFMesh*)ECS_STACK_ALLOC(sizeof(GLTFMesh) * data->mesh_count);
		// Nullptr and 0 everything
		memset(gltf_meshes, 0, sizeof(GLTFMesh) * data->mesh_count);
		AllocatorPolymorphic allocator = multithreaded_allocation ? AllocatorTs() : Allocator();

		bool has_invert = !HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT);
		bool success = LoadMeshesFromGLTF(*data, gltf_meshes, allocator, has_invert);
		// The load failed
		if (!success) {
			return nullptr;
		}

		ResourceManagerLoadDesc load_descriptor_modified = load_descriptor;
		load_descriptor_modified.load_flags |= ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK;
		Stream<Mesh>* meshes = LoadMeshImplementationEx({ gltf_meshes, data->mesh_count }, multithreaded_allocation, scale_factor, load_descriptor_modified, ex_desc);
		FreeGLTFMeshes({ gltf_meshes, data->mesh_count }, allocator);

		return meshes;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	Stream<Mesh>* ResourceManager::LoadMeshImplementationEx(Stream<GLTFMesh> gltf_meshes, bool multithreaded_allocation, float scale_factor, const ResourceManagerLoadDesc& load_descriptor, const ResourceManagerExDesc* ex_desc)
	{
		// Scale the meshes, the function already checks for scale of 1.0f
		ScaleGLTFMeshes(gltf_meshes, scale_factor);

		void* allocation = nullptr;
		// Calculate the allocation size
		size_t allocation_size = sizeof(Stream<Mesh>);
		allocation_size += sizeof(Mesh) * gltf_meshes.size;

		AllocatorPolymorphic allocator = multithreaded_allocation ? AllocatorTs() : Allocator();

		// Allocate the needed memeory
		allocation = Allocate(allocator, allocation_size);
		Stream<Mesh>* meshes = (Stream<Mesh>*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(Stream<Mesh>);
		meshes->InitializeFromBuffer(buffer, gltf_meshes.size);

		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE;
		// Convert the gltf meshes into multiple meshes
		GLTFMeshesToMeshes(m_graphics, gltf_meshes.buffer, meshes->buffer, meshes->size, misc_flags);

		if (!HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_MESH_EX_DO_NOT_SCALE_BACK)) {
			// Rescale the meshes to their original size such that on further processing they will be the same
			ScaleGLTFMeshes(gltf_meshes, 1.0f / scale_factor);
		}

		AddResourceEx(this, ResourceType::Mesh, meshes, multithreaded_allocation, load_descriptor, ex_desc);

		return meshes;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	CoalescedMesh* ResourceManager::LoadCoalescedMesh(Stream<wchar_t> filename, float scale_factor, const ResourceManagerLoadDesc& load_descriptor)
	{
		void* meshes = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::CoalescedMesh,
			load_descriptor,
			[=]() {
				return LoadCoalescedMeshImplementation(filename, false, scale_factor, load_descriptor);
			});

		return (CoalescedMesh*)meshes;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(CoalescedMesh*, ResourceManager::LoadCoalescedMesh, Stream<wchar_t>, float, const ResourceManagerLoadDesc&);

	// ---------------------------------------------------------------------------------------------------------------------------

	CoalescedMesh* ResourceManager::LoadCoalescedMeshImplementation(Stream<wchar_t> filename, bool multithreaded_allocation, float scale_factor, const ResourceManagerLoadDesc& load_descriptor)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, _path, 512);
		GLTFData data = LoadGLTFFile(filename);
		// The load failed
		if (data.data == nullptr || data.mesh_count == 0) {
			return nullptr;
		}
		CoalescedMesh* coalesced_mesh = LoadCoalescedMeshImplementationEx(&data, multithreaded_allocation, scale_factor, load_descriptor);
		FreeGLTFFile(data);
		return coalesced_mesh;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	CoalescedMesh* ResourceManager::LoadCoalescedMeshImplementationEx(
		const GLTFData* data,
		bool multithreaded_allocation,
		float scale_factor, 
		const ResourceManagerLoadDesc& load_descriptor, 
		const ResourceManagerExDesc* ex_desc
	)
	{
		bool has_invert = !HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT);
		bool has_origin_to_center = HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_COALESCED_MESH_ORIGIN_TO_CENTER);

		AllocatorPolymorphic allocator = multithreaded_allocation ? AllocatorTs() : Allocator();

		// Calculate the allocation size
		size_t allocation_size = sizeof(CoalescedMesh) + sizeof(Submesh) * data->mesh_count;
		// Allocate the needed memory
		void* allocation = Allocate(allocator, allocation_size);
		CoalescedMesh* mesh = (CoalescedMesh*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(CoalescedMesh);
		mesh->submeshes.InitializeFromBuffer(buffer, data->mesh_count);

		LoadCoalescedMeshFromGLTFOptions options;
		options.allocate_submesh_name = true;
		options.permanent_allocator = allocator;
		options.scale_factor = scale_factor;
		options.center_object_midpoint = has_origin_to_center;
		bool success = LoadCoalescedMeshFromGLTFToGPU(m_graphics, *data, mesh, has_invert, &options);
		if (!success) {
			Deallocate(allocator, allocation);
			mesh = nullptr;
		}

		return mesh;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	CoalescedMesh* ResourceManager::LoadCoalescedMeshImplementationEx(
		Stream<GLTFMesh> gltf_meshes,
		bool multithreaded_allocation,
		float scale_factor, 
		const ResourceManagerLoadDesc& load_descriptor, 
		const ResourceManagerExDesc* ex_desc
	)
	{
		AllocatorPolymorphic allocator = multithreaded_allocation ? AllocatorTs() : Allocator();

		// Calculate the allocation size
		size_t allocation_size = sizeof(CoalescedMesh) + sizeof(Submesh) * gltf_meshes.size;
		// Allocate the needed memory
		void* allocation = Allocate(allocator, allocation_size);
		CoalescedMesh* mesh = (CoalescedMesh*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(CoalescedMesh);
		mesh->submeshes.InitializeFromBuffer(buffer, gltf_meshes.size);

		// Scale the gltf meshes, they already have a built in check for scale of 1.0f
		ScaleGLTFMeshes(gltf_meshes, scale_factor);

		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE;
		mesh->mesh = GLTFMeshesToMergedMesh(m_graphics, gltf_meshes, mesh->submeshes.buffer, misc_flags);

		AddResourceEx(this, ResourceType::CoalescedMesh, mesh, multithreaded_allocation, load_descriptor, ex_desc);

		if (!HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_COALESCED_MESH_EX_DO_NOT_SCALE_BACK)) {
			// Rescale the meshes to their original size such that on further processing they will be the same
			ScaleGLTFMeshes(gltf_meshes, 1.0f / scale_factor);
		}
		
		return mesh;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	CoalescedMesh* ResourceManager::LoadCoalescedMeshImplementationEx(
		const GLTFMesh* gltf_mesh,
		Stream<Submesh> submeshes,
		bool multithreaded_allocation,
		float scale_factor,
		const ResourceManagerLoadDesc& load_descriptor, 
		const ResourceManagerExDesc* ex_desc
	)
	{
		AllocatorPolymorphic allocator = multithreaded_allocation ? AllocatorTs() : Allocator();

		// Calculate the allocation size
		size_t allocation_size = sizeof(CoalescedMesh) + sizeof(Submesh) * submeshes.size;
		// Allocate the needed memory
		void* allocation = Allocate(allocator, allocation_size);
		CoalescedMesh* mesh = (CoalescedMesh*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(CoalescedMesh);
		mesh->submeshes.InitializeAndCopy(buffer, submeshes);

		// We also need to coallesce the names of the submeshes
		StreamCoalescedInplaceDeepCopy(mesh->submeshes, AllocatorTs());

		// We should perform the origin operation before scaling
		if (HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_COALESCED_MESH_ORIGIN_TO_CENTER)) {
			GLTFMeshOriginToCenter(gltf_mesh);
		}
		ScaleGLTFMeshes({ gltf_mesh, 1 }, scale_factor);
		
		mesh->mesh = GLTFMeshToMesh(m_graphics, *gltf_mesh);

		AddResourceEx(this, ResourceType::CoalescedMesh, mesh, multithreaded_allocation, load_descriptor, ex_desc);

		if (!HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_COALESCED_MESH_EX_DO_NOT_SCALE_BACK)) {
			// Rescale the meshes to their original size such that on further processing they will be the same
			ScaleGLTFMeshes({ gltf_mesh, 1 }, 1.0f / scale_factor);
		}

		return mesh;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all materials from a gltf file
	template<bool reference_counted>
	Stream<PBRMaterial>* ResourceManager::LoadMaterials(
		Stream<wchar_t> filename,
		const ResourceManagerLoadDesc& load_descriptor
	) {
		void* materials = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::PBRMaterial,
			load_descriptor,
			[=]() {
				return LoadMaterialsImplementation(filename, false, load_descriptor);
			});

		return (Stream<PBRMaterial>*)materials;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(Stream<PBRMaterial>*, ResourceManager::LoadMaterials, Stream<wchar_t>, const ResourceManagerLoadDesc&);

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all materials from a gltf file
	Stream<PBRMaterial>* ResourceManager::LoadMaterialsImplementation(Stream<wchar_t> filename, bool multithreaded_allocation, const ResourceManagerLoadDesc& load_descriptor) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, _path, 512);
		GLTFData data = LoadGLTFFile(filename);
		// The load failed
		if (data.data == nullptr) {
			return nullptr;
		}
		ECS_ASSERT(data.mesh_count < 200);

		PBRMaterial* _materials = (PBRMaterial*)ECS_STACK_ALLOC((sizeof(PBRMaterial) + sizeof(unsigned int)) * data.mesh_count);
		Stream<PBRMaterial> materials(_materials, 0);
		Stream<unsigned int> material_mask(OffsetPointer(_materials, sizeof(PBRMaterial) * data.mesh_count), 0);

		AllocatorPolymorphic allocator = multithreaded_allocation ? AllocatorTs() : Allocator();
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

		void* allocation = Allocate(allocator, allocation_size);
		Stream<PBRMaterial>* pbr_materials = (Stream<PBRMaterial>*)allocation;
		pbr_materials->InitializeFromBuffer(OffsetPointer(allocation, sizeof(Stream<PBRMaterial>)), materials.size);
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
		const ResourceManagerLoadDesc& load_descriptor
	) {
		void* mesh = LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::PBRMesh,
			load_descriptor,
			[=]() {
				return LoadPBRMeshImplementation(filename, false, load_descriptor);
		});

		return (PBRMesh*)mesh;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(PBRMesh*, ResourceManager::LoadPBRMesh, Stream<wchar_t>, const ResourceManagerLoadDesc&);

	// ---------------------------------------------------------------------------------------------------------------------------

#pragma region Unload User Material Individual Functions

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadUserMaterialShader(
		ResourceManager* resource_manager,
		const UserMaterial* user_material,
		Stream<wchar_t> shader_path,
		void* shader_interface,
		ShaderCompileOptions compile_options,
		Stream<char> shader_name,
		ECS_SHADER_TYPE shader_type,
		const ResourceManagerLoadDesc& load_descriptor
	) {
		bool check_resource_before_unload = HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_USER_MATERIAL_CHECK_RESOURCE);

		// It can happen that the material does not have a shader assigned (for varying reasons)
		if (shader_path.size > 0) {
			bool dont_load = HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS);

			// Deallocate the vertex shader
			if (dont_load) {
				resource_manager->UnloadShaderImplementation(shader_interface, shader_type);
			}
			else {
				ResourceManagerLoadDesc current_desc = load_descriptor;
				ECS_STACK_VOID_STREAM(suffix, 512);
				if (user_material->generate_unique_name_from_setting) {
					GenerateShaderCompileOptionsSuffix(compile_options, suffix, shader_name);
				}
				current_desc.identifier_suffix = suffix;
				if (!check_resource_before_unload || resource_manager->Exists(shader_path, ResourceType::Shader, suffix)) {
					resource_manager->UnloadShader<true>(shader_path, current_desc);
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadUserMaterialVertexShader(
		ResourceManager* resource_manager,
		const UserMaterial* user_material,
		Material* material,
		const ResourceManagerLoadDesc& load_descriptor
	) {
		UnloadUserMaterialShader(
			resource_manager,
			user_material,
			user_material->vertex_shader,
			material->vertex_shader.Interface(),
			user_material->vertex_compile_options,
			user_material->vertex_shader_name,
			ECS_SHADER_VERTEX,
			load_descriptor
		);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadUserMaterialPixelShader(
		ResourceManager* resource_manager,
		const UserMaterial* user_material,
		Material* material,
		const ResourceManagerLoadDesc& load_descriptor
	) {
		UnloadUserMaterialShader(
			resource_manager,
			user_material,
			user_material->pixel_shader,
			material->pixel_shader.Interface(),
			user_material->pixel_compile_options,
			user_material->pixel_shader_name,
			ECS_SHADER_PIXEL,
			load_descriptor
		);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<typename Entry, typename Entries, typename Slots>
	static Entry RemoveUserMaterialEntry(unsigned char slot, Entries& entries, Slots& slots) {
		unsigned char index = 0;

		// When using decltype(*buffers), the compiler actually generates a reference type instead of a value type
		// Extremely annoying bug since it is counter intuitive. Has to do this work around
		auto entry = entries[0];
		for (; index < entries.size; index++) {
			if (slots[index] == slot) {
				entry = entries[index];
				entries.RemoveSwapBack(index);
				slots.RemoveSwapBack(index);
				return entry;
			}
		}

		// Return a nullptr resource - it wasn't created last time
		return { nullptr };
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadUserMaterialTextures(
		ResourceManager* resource_manager,
		const UserMaterial* user_material,
		Material* material,
		const ResourceManagerLoadDesc& load_descriptor
	) {
		bool dont_load = HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS);
		bool check_resource_before_unload = HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_USER_MATERIAL_CHECK_RESOURCE);

		// Use temporaries as to not modify the material
		ECS_STACK_CAPACITY_STREAM(ResourceView, temporary_p_textures, 16);
		ECS_STACK_CAPACITY_STREAM(unsigned char, temporary_p_texture_slots, 16);
		ECS_STACK_CAPACITY_STREAM(ResourceView, temporary_v_textures, 16);
		ECS_STACK_CAPACITY_STREAM(unsigned char, temporary_v_texture_slots, 16);
		temporary_p_textures.CopyOther(Stream<ResourceView>(material->p_textures, material->p_texture_count));
		temporary_p_texture_slots.CopyOther(Stream<unsigned char>(material->p_texture_slot, material->p_texture_count));
		temporary_v_textures.CopyOther(Stream<ResourceView>(material->v_textures, material->v_texture_count));
		temporary_v_texture_slots.CopyOther(Stream<unsigned char>(material->v_texture_slot, material->v_texture_count));

		for (size_t index = 0; index < user_material->textures.size; index++) {
			ResourceView resource_view;
			if (user_material->textures[index].shader_type == ECS_SHADER_VERTEX) {
				resource_view = RemoveUserMaterialEntry<ResourceView>(user_material->textures[index].slot, temporary_v_textures, temporary_v_texture_slots);
			}
			else {
				resource_view = RemoveUserMaterialEntry<ResourceView>(user_material->textures[index].slot, temporary_p_textures, temporary_p_texture_slots);
			}

			// Only if the resource view is specified
			if (resource_view.Interface() != nullptr) {
				if (dont_load) {
					resource_manager->UnloadTextureImplementation(resource_view);
				}
				else {
					ResourceManagerLoadDesc manager_desc;
					ECS_STACK_VOID_STREAM(settings_suffix, 512);
					Stream<void> suffix = load_descriptor.identifier_suffix;
					if (user_material->generate_unique_name_from_setting) {
						user_material->textures[index].GenerateSettingsSuffix(settings_suffix);
						suffix = settings_suffix;
					}
					manager_desc.identifier_suffix = suffix;
					if (!check_resource_before_unload || resource_manager->Exists(user_material->textures[index].filename, ResourceType::Texture, suffix)) {
						resource_manager->UnloadTexture<true>(user_material->textures[index].filename, manager_desc);
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadUserMaterialSamplers(
		ResourceManager* resource_manager,
		const UserMaterial* user_material
	) {
		for (size_t index = 0; index < user_material->samplers.size; index++) {
			if (user_material->samplers[index].state.Interface() != nullptr) {
				resource_manager->m_graphics->FreeResource(user_material->samplers[index].state);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void UnloadUserMaterialBuffers(
		ResourceManager* resource_manager,
		const UserMaterial* user_material,
		Material* material
	) {
		ECS_STACK_CAPACITY_STREAM(ConstantBuffer, temporary_p_buffers, 16);
		ECS_STACK_CAPACITY_STREAM(unsigned char, temporary_p_buffer_slots, 16);
		ECS_STACK_CAPACITY_STREAM(ConstantBuffer, temporary_v_buffers, 16);
		ECS_STACK_CAPACITY_STREAM(unsigned char, temporary_v_buffer_slots, 16);
		temporary_p_buffers.CopyOther(Stream<ConstantBuffer>(material->p_buffers, material->p_buffer_count));
		temporary_p_buffer_slots.CopyOther(Stream<unsigned char>(material->p_buffer_slot, material->p_buffer_count));
		temporary_v_buffers.CopyOther(Stream<ConstantBuffer>(material->v_buffers, material->v_buffer_count));
		temporary_v_buffer_slots.CopyOther(Stream<unsigned char>(material->v_buffer_slot, material->v_buffer_count));

		for (size_t index = 0; index < user_material->buffers.size; index++) {
			ConstantBuffer buffer;
			if (user_material->buffers[index].shader_type == ECS_SHADER_VERTEX) {
				buffer = RemoveUserMaterialEntry<ConstantBuffer>(user_material->buffers[index].slot, temporary_v_buffers, temporary_v_buffer_slots);
			}
			else {
				buffer = RemoveUserMaterialEntry<ConstantBuffer>(user_material->buffers[index].slot, temporary_p_buffers, temporary_p_buffer_slots);
			}

			// Only if the buffer is specified
			if (buffer.Interface() != nullptr) {
				resource_manager->m_graphics->FreeResource(buffer);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Load User Material Individual Functions

	// Returns true if it succeded else false
	static bool LoadUserMaterialVertexShader(
		ResourceManager* resource_manager, 
		const UserMaterial* user_material, 
		Material* converted_material,
		const ResourceManagerLoadDesc& load_descriptor
	) {
		bool dont_load = HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS);

		Stream<char> source_code;
		Stream<void> byte_code;

		LoadShaderExtraInformation load_extra_information;
		load_extra_information.source_code = &source_code;
		load_extra_information.byte_code = &byte_code;

		ShaderInterface vertex_shader = { nullptr };
		if (dont_load) {
			// We use the single-threaded load because the main UserMaterial function should acquire the lock for us
			vertex_shader = resource_manager->LoadShaderImplementation(
				user_material->vertex_shader, 
				ECS_SHADER_VERTEX,
				false,
				load_extra_information,
				user_material->vertex_compile_options, 
				load_descriptor
			);
		}
		else {
			ResourceManagerLoadDesc current_desc = load_descriptor;
			ECS_STACK_VOID_STREAM(suffix, 512);
			if (user_material->generate_unique_name_from_setting) {
				GenerateShaderCompileOptionsSuffix(user_material->vertex_compile_options, suffix, user_material->vertex_shader_name);
			}
			current_desc.identifier_suffix = suffix;
			vertex_shader = resource_manager->LoadShader<true>(
				user_material->vertex_shader, 
				ECS_SHADER_VERTEX, 
				load_extra_information,
				user_material->vertex_compile_options, 
				current_desc
			);
		}

		auto code_deallocator = StackScope([&]() {
			source_code.Deallocate(resource_manager->Allocator());
			byte_code.Deallocate(resource_manager->Allocator());
		});

		converted_material->vertex_shader = vertex_shader;

		if (converted_material->vertex_shader.Interface() == nullptr) {
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(ECS_MESH_INDEX, mesh_indices, ECS_MESH_BUFFER_COUNT);
		bool success = resource_manager->m_graphics->ReflectVertexBufferMapping(source_code, mesh_indices);
		if (!success) {
			// Unload the vertex shader
			UnloadUserMaterialVertexShader(resource_manager, user_material, converted_material, load_descriptor);
			return false;
		}

		converted_material->vertex_buffer_mapping_count = mesh_indices.size;
		mesh_indices.CopyTo(converted_material->vertex_buffer_mappings);

		InputLayout input_layout = resource_manager->m_graphics->ReflectVertexShaderInput(source_code, byte_code);
		if (input_layout.layout == nullptr) {
			// Unload the vertex shader
			UnloadUserMaterialVertexShader(resource_manager, user_material, converted_material, load_descriptor);
			return false;
		}
		converted_material->layout = input_layout;

		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// Returns true if it succeded, else false
	static bool LoadUserMaterialPixelShader(
		ResourceManager* resource_manager, 
		const UserMaterial* user_material, 
		Material* converted_material,
		const ResourceManagerLoadDesc& load_descriptor
	) {
		bool dont_load = HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS);

		if (dont_load) {
			// We use the single-threaded load because the main UserMaterial function should acquire the lock for us
			converted_material->pixel_shader = resource_manager->LoadShaderImplementation(
				user_material->pixel_shader, 
				ECS_SHADER_PIXEL,
				false,
				{},
				user_material->pixel_compile_options, 
				load_descriptor
			);
		}
		else {
			ResourceManagerLoadDesc current_desc = load_descriptor;
			ECS_STACK_VOID_STREAM(suffix, 512);
			if (user_material->generate_unique_name_from_setting) {
				GenerateShaderCompileOptionsSuffix(user_material->pixel_compile_options, suffix, user_material->pixel_shader_name);
			}
			current_desc.identifier_suffix = suffix;
			converted_material->pixel_shader = resource_manager->LoadShader<true>(
				user_material->pixel_shader, 
				ECS_SHADER_PIXEL, 
				{},
				user_material->pixel_compile_options, 
				current_desc
			);
		}

		return converted_material->pixel_shader.Interface() != nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static bool LoadUserMaterialTextures(
		ResourceManager* resource_manager,
		const UserMaterial* user_material,
		Material* converted_material,
		const ResourceManagerLoadDesc& load_descriptor
	) {
		converted_material->ResetTextures();
		bool dont_load = HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_INSERT_COMPONENTS);

		for (size_t index = 0; index < user_material->textures.size; index++) {
			ResourceView resource_view = { nullptr };

			if (user_material->textures[index].filename.size > 0) {
				ResourceManagerTextureDesc texture_desc;
				texture_desc.context = resource_manager->m_graphics->GetContext();
				texture_desc.compression = user_material->textures[index].compression;
				texture_desc.misc_flags = user_material->textures[index].generate_mips ? ECS_GRAPHICS_MISC_GENERATE_MIPS : ECS_GRAPHICS_MISC_NONE;
				texture_desc.srgb = user_material->textures[index].srgb;

				ResourceManagerLoadDesc manager_desc = load_descriptor;
				ECS_STACK_VOID_STREAM(settings_suffix, 512);
				Stream<void> suffix = load_descriptor.identifier_suffix;
				if (user_material->generate_unique_name_from_setting) {
					user_material->textures[index].GenerateSettingsSuffix(settings_suffix);
					suffix = settings_suffix;
				}
				manager_desc.identifier_suffix = suffix;

				if (dont_load) {
					resource_view = resource_manager->LoadTextureImplementation(user_material->textures[index].filename, &texture_desc, manager_desc);
				}
				else {
					resource_view = resource_manager->LoadTexture<true>(user_material->textures[index].filename, &texture_desc, manager_desc);
				}
				if (resource_view.view == nullptr) {
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
							resource_manager->UnloadTextureImplementation(resource_view);
						}
						else {
							suffix = load_descriptor.identifier_suffix;
							if (user_material->generate_unique_name_from_setting) {
								user_material->textures[subindex].GenerateSettingsSuffix(settings_suffix);
								suffix = settings_suffix;
							}
							manager_desc.identifier_suffix = suffix;
							resource_manager->UnloadTexture<true>(user_material->textures[subindex].filename, manager_desc);
						}
					}

					return false;
				}
			}

			converted_material->AddResourceView(resource_view, user_material->textures[index].slot, user_material->textures[index].shader_type == ECS_SHADER_VERTEX);
		}

		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void LoadUserMaterialSamplers(
		ResourceManager* resource_manager,
		const UserMaterial* user_material,
		Material* converted_material
	) {
		converted_material->ResetSamplers();
		for (size_t index = 0; index < user_material->samplers.size; index++) {
			SamplerState sampler = user_material->samplers[index].state;
			converted_material->AddSampler(sampler, user_material->samplers[index].slot, user_material->samplers[index].shader_type == ECS_SHADER_VERTEX);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	static void LoadUserMaterialBuffers(
		ResourceManager* resource_manager,
		const UserMaterial* user_material,
		Material* converted_material
	) {
		converted_material->ResetConstantBuffers();
		for (size_t index = 0; index < user_material->buffers.size; index++) {
			ConstantBuffer buffer = { nullptr };
			if (user_material->buffers[index].dynamic || user_material->buffers[index].data.buffer != nullptr) {
				if (user_material->buffers[index].dynamic) {
					if (user_material->buffers[index].data.buffer != nullptr) {
						buffer = resource_manager->m_graphics->CreateConstantBuffer(user_material->buffers[index].data.size, user_material->buffers[index].data.buffer);
					}
					else {
						buffer = resource_manager->m_graphics->CreateConstantBuffer(user_material->buffers[index].data.size);
					}
				}
				else {
					// Can make this IMMUTABLE
					buffer = resource_manager->m_graphics->CreateConstantBuffer(user_material->buffers[index].data.size, user_material->buffers[index].data.buffer, false, ECS_GRAPHICS_USAGE_IMMUTABLE);
				}
			}

			unsigned char add_index = converted_material->AddConstantBuffer(buffer, user_material->buffers[index].slot, user_material->buffers[index].shader_type == ECS_SHADER_VERTEX);

			// Check for tags
			for (unsigned int subindex = 0; subindex < user_material->buffers[index].tags.size; subindex++) {
				converted_material->AddTag(
					user_material->buffers[index].tags[subindex].string, 
					user_material->buffers[index].tags[subindex].byte_offset,
					add_index,
					user_material->buffers[index].shader_type == ECS_SHADER_VERTEX
				);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::LoadUserMaterial(const UserMaterial* user_material, Material* converted_material, const ResourceManagerLoadDesc& load_descriptor, const ResourceManagerUserMaterialExtraInfo& extra_info)
	{
		memset(converted_material, 0, sizeof(*converted_material));

		ECS_ASSERT(user_material->vertex_shader.size > 0, "User material needs vertex shader.");
		ECS_ASSERT(user_material->pixel_shader.size > 0, "User material needs pixel shader.");

		// The shaders and textures require multithreaded resource manager locking, if specified in the extra info
		extra_info.Lock(this);
		bool success = true;
		__try {
			if (!LoadUserMaterialVertexShader(this, user_material, converted_material, load_descriptor)) {
				success = false;
				__leave;
			}

			if (!LoadUserMaterialPixelShader(this, user_material, converted_material, load_descriptor)) {
				UnloadUserMaterialVertexShader(this, user_material, converted_material, load_descriptor);
				success = false;
				__leave;
			}

			if (!LoadUserMaterialTextures(this, user_material, converted_material, load_descriptor)) {
				UnloadUserMaterialVertexShader(this, user_material, converted_material, load_descriptor);
				UnloadUserMaterialPixelShader(this, user_material, converted_material, load_descriptor);
				success = false;
				__leave;
			}
		}
		__finally {
			// Always release the lock
			extra_info.Unlock(this);
		}

		if (!success) {
			return false;
		}

		// The buffers and samplers don't require any resource manager locking
		LoadUserMaterialBuffers(this, user_material, converted_material);
		LoadUserMaterialSamplers(this, user_material, converted_material);

		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// Loads all meshes and materials from a gltf file, combines the meshes into a single one sorted by material submeshes
	PBRMesh* ResourceManager::LoadPBRMeshImplementation(Stream<wchar_t> filename, bool multithreaded_allocation, const ResourceManagerLoadDesc& load_descriptor) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, _path, 512);
		GLTFData data = LoadGLTFFile(filename);
		// The load failed
		if (data.data == nullptr) {
			return nullptr;
		}
		ECS_ASSERT(data.mesh_count < 200);

		AllocatorPolymorphic allocator = multithreaded_allocation ? AllocatorTs() : Allocator();

		void* initial_allocation = Allocate(allocator, (sizeof(GLTFMesh) + sizeof(PBRMaterial) + sizeof(unsigned int)) * data.mesh_count);
		GLTFMesh* gltf_meshes = (GLTFMesh*)initial_allocation;
		Stream<PBRMaterial> pbr_materials(OffsetPointer(gltf_meshes, sizeof(GLTFMesh) * data.mesh_count), 0);
		unsigned int* material_masks = (unsigned int*)OffsetPointer(pbr_materials.buffer, sizeof(PBRMaterial) * data.mesh_count);
		// Make every stream 0 for the gltf meshes
		memset(gltf_meshes, 0, sizeof(GLTFMesh) * data.mesh_count);

		bool has_invert = !HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_MESH_DISABLE_Z_INVERT);
		bool success = LoadMeshesAndDisjointMaterialsFromGLTF(data, gltf_meshes, pbr_materials, material_masks, allocator, has_invert);
		// The load failed
		if (!success) {
			// Free the gltf data and the initial allocation
			FreeGLTFFile(data);
			Deallocate(allocator, initial_allocation);
			return nullptr;
		}

		// Calculate the allocation size
		size_t allocation_size = sizeof(PBRMesh);
		allocation_size += (sizeof(Submesh) + sizeof(PBRMaterial)) * pbr_materials.size;

		// Allocate the instance
		void* allocation = Allocate(allocator, allocation_size);
		PBRMesh* mesh = (PBRMesh*)allocation;
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += sizeof(PBRMesh);

		mesh->mesh.submeshes.buffer = (Submesh*)buffer;
		buffer += sizeof(Submesh) * pbr_materials.size;

		mesh->materials = (PBRMaterial*)buffer;

		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE;
		mesh->mesh.mesh = GLTFMeshesToMergedMesh(m_graphics, { gltf_meshes, data.mesh_count }, mesh->mesh.submeshes.buffer, material_masks, pbr_materials.size, misc_flags);
		mesh->mesh.submeshes.size = pbr_materials.size;

		// Copy the pbr materials to this new buffer
		memcpy(mesh->materials, pbr_materials.buffer, sizeof(PBRMaterial) * pbr_materials.size);

		// Free the gltf data, the gltf meshes and the initial allocation
		FreeGLTFFile(data);
		FreeGLTFMeshes({ gltf_meshes, data.mesh_count }, allocator);
		Deallocate(allocator, initial_allocation);

		return mesh;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	// The shader source code or byte code are allocated from the multithreaded allocator
	static ShaderInterface LoadShaderInternalImplementation(ResourceManager* manager, Stream<wchar_t> filename, bool multithreaded_allocation, ShaderCompileOptions options, const ResourceManagerLoadDesc& load_descriptor,
		const LoadShaderExtraInformation& extra_information, ECS_SHADER_TYPE type) {
		Path path_extension = PathExtensionBoth(filename);
		bool is_byte_code = path_extension == L".cso";

		void* shader = nullptr;
		AllocatorPolymorphic allocator_polymorphic = extra_information.GetAllocator(manager, multithreaded_allocation);

		if (is_byte_code) {
			ECS_ASSERT(extra_information.source_code == nullptr, "Cannot retrieve shader source code from a binary shader.");
			ECS_ASSERT(type != ECS_SHADER_VERTEX || extra_information.input_layout == nullptr, "Cannot create vertex shader input layout from a binary file.");
			Stream<void> byte_code = ReadWholeFileBinary(filename, allocator_polymorphic);
			if (byte_code.size > 0) {
				shader = manager->m_graphics->CreateShader(byte_code, type, HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY));
				if (extra_information.byte_code != nullptr) {
					if (shader != nullptr) {
						*extra_information.byte_code = byte_code;
					}
					else {
						// Deallocate the byte code
						Deallocate(allocator_polymorphic, byte_code.buffer);
					}
				}
				else {
					// Deallocate the byte code
					Deallocate(allocator_polymorphic, byte_code.buffer);
				}
			}
		}
		else {
			// Create a temporary allocator for the include allocator, that takes its data from the multithreaded allocator
			MemoryManager temporary_allocator(ECS_RESOURCE_MANAGER_INCLUDE_ALLOCATOR_CAPACITY, 128, ECS_RESOURCE_MANAGER_INCLUDE_ALLOCATOR_BACKUP_CAPACITY, manager->AllocatorTs());

			Stream<char> source_code = ReadWholeFileText(filename, allocator_polymorphic);
			if (source_code.size > 0) {
				Stream<void> byte_code;
				bool should_retrieve_byte_code = extra_information.byte_code != nullptr || extra_information.input_layout != nullptr;
				
				ShaderIncludeFiles include(&temporary_allocator, manager->m_shader_directory);
				__try {
					shader = manager->m_graphics->CreateShaderFromSource(
						source_code,
						type,
						&include,
						options,
						should_retrieve_byte_code ? &byte_code : nullptr,
						allocator_polymorphic,
						HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY)
					);
				}
				__finally {
					// Deallocate the allocator even when the creation fails
					temporary_allocator.Free();
				}

				// Create the input layout
				LoadShaderExtraInformationHandleInputLayout(shader, source_code, byte_code, type, manager, extra_information);

				if (extra_information.source_code != nullptr) {
					if (shader != nullptr) {
						*extra_information.source_code = source_code;
					}
					else {
						// Deallocate the source code
						Deallocate(allocator_polymorphic, source_code.buffer);
					}
				}
				else {
					// Deallocate the source code
					Deallocate(allocator_polymorphic, source_code.buffer);
				}

				if (byte_code.size > 0) {
					if (extra_information.byte_code != nullptr) {
						*extra_information.byte_code = byte_code;
					}
					else {
						Deallocate(allocator_polymorphic, byte_code.buffer);
					}
				}
			}
		}

		return { shader };
	}

	// Convert from source code to shader
	static ShaderInterface LoadShaderInternalImplementationEx(
		ResourceManager* manager,
		Stream<char> source_code,
		ECS_SHADER_TYPE shader_type,
		bool multithreaded_allocation,
		const LoadShaderExtraInformation& extra_information,
		ShaderCompileOptions options,
		const ResourceManagerLoadDesc& load_descriptor,
		const ResourceManagerExDesc* ex_desc
	) {
		// Create a special allocator for the include policy, we cannot use the main manager allocator
		// Even with multithreaded access because there are still single threaded functions that run on
		// It and those are not aware of the multithreaded aspect. We cannot use it as a backup allocator either
		// Because of the same reason. This is why we are using the multithreaded allocator as a backup
		MemoryManager include_allocator(ECS_RESOURCE_MANAGER_INCLUDE_ALLOCATOR_CAPACITY, 128, ECS_RESOURCE_MANAGER_INCLUDE_ALLOCATOR_BACKUP_CAPACITY, manager->AllocatorTs());
		ShaderIncludeFiles include(&include_allocator, manager->m_shader_directory);
		void* shader = nullptr;
		__try {
			AllocatorPolymorphic byte_code_allocator = extra_information.GetAllocator(manager, multithreaded_allocation);

			Stream<void> byte_code;
			bool should_retrieve_byte_code = extra_information.byte_code != nullptr || extra_information.input_layout != nullptr;
			shader = manager->m_graphics->CreateShaderFromSource(
				source_code,
				shader_type,
				&include,
				options,
				should_retrieve_byte_code ? &byte_code : nullptr,
				byte_code_allocator,
				HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_TEMPORARY)
			);
			LoadShaderExtraInformationHandleInputLayout(shader, source_code, byte_code, shader_type, manager, extra_information);

			if (byte_code.size > 0) {
				if (extra_information.byte_code != nullptr) {
					*extra_information.byte_code = byte_code;
				}
				else {
					Deallocate(byte_code_allocator, byte_code.buffer);
				}
			}
		}
		__finally {
			// Deallocate the allocator - even when the create fails
			include_allocator.Free();
		}

		if (shader != nullptr) {
			if (ex_desc != nullptr && ex_desc->HasFilename()) {
				ex_desc->Lock(manager);

				__try {
					// We are in a locked section code, can use the main allocator
					void* allocation = manager->m_memory->Allocate(sizeof(VertexShaderStorage));
					VertexShaderStorage* storage = (VertexShaderStorage*)allocation;

					// Doesn't matter the type here
					storage->shader = (ID3D11VertexShader*)shader;
					storage->source_code = { nullptr, 0 };
					storage->byte_code = { nullptr, 0 };
					manager->AddResource(
						ex_desc->filename,
						ResourceType::Shader,
						allocation,
						false,
						ex_desc->time_stamp,
						load_descriptor.identifier_suffix,
						ex_desc->reference_count
					);
				}
				__finally {
					// Perform the unlock even when faced with a crash
					ex_desc->Unlock(manager);
				}
			}
		}

		return { shader };
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	ShaderInterface ResourceManager::LoadShader(
		Stream<wchar_t> filename, 
		ECS_SHADER_TYPE shader_type, 
		const LoadShaderExtraInformation& extra_information,
		ShaderCompileOptions options,
		const ResourceManagerLoadDesc& load_descriptor
	)
	{
		bool _is_loaded_first_time = false;
		bool* is_loaded_first_time = load_descriptor.reference_counted_is_loaded != nullptr ? load_descriptor.reference_counted_is_loaded : &_is_loaded_first_time;

		// Use as allocation the biggest storage type - that is the one of the vertex shader
		VertexShaderStorage* shader = (VertexShaderStorage*)LoadResource<reference_counted>(
			this,
			filename,
			ResourceType::Shader,
			sizeof(VertexShaderStorage),
			load_descriptor,
			[=](void* allocation) {
				VertexShaderStorage* shader = (VertexShaderStorage*)allocation;
				shader->shader = LoadShaderImplementation(filename, shader_type, false, extra_information, options, load_descriptor);

				if (shader->shader.Interface() != nullptr) {
					shader->source_code = extra_information.source_code != nullptr ? extra_information.source_code->Copy(Allocator()) : Stream<char>();

					if (shader_type == ECS_SHADER_VERTEX) {
						shader->byte_code = extra_information.byte_code != nullptr ? extra_information.byte_code->Copy(Allocator()) : Stream<void>();
					}
					else {
						shader->byte_code = { nullptr, 0 };
					}
					return allocation;
				}
				return (void*)nullptr;
		});

		if (shader != nullptr) {
			if constexpr (reference_counted) {
				if (extra_information.source_code != nullptr || extra_information.byte_code != nullptr) {
					if (!*is_loaded_first_time) {
						// Need to get the source code or the byte code
						// For byte code we need the source code as well
						if (shader->source_code.size == 0) {
							// Load the file and store it inside the current storage
							// Use the multithreaded allocator in order to remain consistent
							// With the other load functions that use it as well
							Stream<char> source_code = ReadWholeFileText(filename, Allocator());
							shader->source_code = source_code;
						}
						if (extra_information.source_code != nullptr) {
							// Make a copy, such that the caller is the owner of the data
							*extra_information.source_code = shader->source_code.Copy(extra_information.GetAllocator(this, false));
						}

						if ((extra_information.byte_code != nullptr || extra_information.input_layout != nullptr) && shader_type == ECS_SHADER_VERTEX) {
							if (shader->byte_code.size == 0) {
								MemoryManager include_allocator(ECS_RESOURCE_MANAGER_INCLUDE_ALLOCATOR_CAPACITY, 128, ECS_RESOURCE_MANAGER_INCLUDE_ALLOCATOR_BACKUP_CAPACITY, Allocator());
								ShaderIncludeFiles include_files(&include_allocator, m_shader_directory);
								__try {
									if (shader->source_code.size > 0) {
										shader->byte_code = m_graphics->CompileShaderToByteCode(shader->source_code, shader_type, &include_files, Allocator(), options);
									}
									else {
										shader->byte_code = {};
									}
								}
								__finally {
									include_allocator.Free();
								}
							}
							// Make a copy, such that the caller is the owner of the data
							if (extra_information.byte_code != nullptr) {
								*extra_information.byte_code = shader->byte_code.Copy(extra_information.GetAllocator(this, false));
							}

							if (extra_information.input_layout != nullptr) {
								if (shader->source_code.size > 0 && shader->byte_code.size > 0) {
									LoadShaderExtraInformationHandleInputLayout(shader, shader->source_code, shader->byte_code, shader_type, this, extra_information);
								}
							}
						}
					}
				}
			}

			return { shader->shader.Interface() };
		}
		return { nullptr };
	}

	ECS_TEMPLATE_FUNCTION_BOOL(ShaderInterface, ResourceManager::LoadShader, Stream<wchar_t>, ECS_SHADER_TYPE, const LoadShaderExtraInformation& extra_information, ShaderCompileOptions, const ResourceManagerLoadDesc&);

	// ---------------------------------------------------------------------------------------------------------------------------

	ShaderInterface ResourceManager::LoadShaderImplementation(
		Stream<wchar_t> filename, 
		ECS_SHADER_TYPE shader_type,
		bool multithreaded_allocation,
		const LoadShaderExtraInformation& extra_information,
		ShaderCompileOptions options, 
		const ResourceManagerLoadDesc& load_descriptor
	)
	{
		return LoadShaderInternalImplementation(this, filename, multithreaded_allocation, options, load_descriptor, extra_information, shader_type);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	ShaderInterface ResourceManager::LoadShaderImplementationEx(
		Stream<char> source_code,
		ECS_SHADER_TYPE shader_type,
		bool multithreaded_allocation,
		const LoadShaderExtraInformation& extra_information,
		ShaderCompileOptions options, 
		const ResourceManagerLoadDesc& load_descriptor,
		const ResourceManagerExDesc* ex_desc
	)
	{
		return LoadShaderInternalImplementationEx(this, source_code, shader_type, multithreaded_allocation, extra_information, options, load_descriptor, ex_desc);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	ResizableStream<void>* ResourceManager::LoadMisc(Stream<wchar_t> filename, AllocatorPolymorphic allocator, const ResourceManagerLoadDesc& load_descriptor)
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

	ECS_TEMPLATE_FUNCTION_BOOL(ResizableStream<void>*, ResourceManager::LoadMisc, Stream<wchar_t>, AllocatorPolymorphic, const ResourceManagerLoadDesc&);

	ResizableStream<void> ResourceManager::LoadMiscImplementation(Stream<wchar_t> filename, AllocatorPolymorphic allocator, const ResourceManagerLoadDesc& load_descriptor)
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

	void ResourceManager::ProtectResource(ResourceIdentifier identifier, ResourceType resource_type, Stream<void> suffix) {
		ResourceManagerEntry* entry = GetEntryPtr(identifier, resource_type, suffix);
		ECS_ASSERT_FORMAT(entry != nullptr, "ResourceManager: Trying to protect resource {#} of type {#} and with suffix {#}, but it doesn't exist.", identifier, ResourceTypeString(resource_type), suffix);
		entry->is_protected = true;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::ProtectResource(const void* resource, ResourceType resource_type, bool assert_if_not_found) {
		bool was_found = m_resource_types[(unsigned int)resource_type].ForEachIndexConst<true>([&](unsigned int entry_index) {
			ResourceManagerEntry* entry = m_resource_types[(unsigned int)resource_type].GetValuePtrFromIndex(entry_index);
			if (entry->data == resource) {
				entry->is_protected = true;
				return true;
			}
			return false;
		});
		if (assert_if_not_found) {
			ECS_ASSERT_FORMAT(was_found, "ResourceManager: Trying to protect a resource by pointer of type {#}, but it was not found.", ResourceTypeString(resource_type));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::UnprotectResource(ResourceIdentifier identifier, ResourceType resource_type, Stream<void> suffix) {
		ResourceManagerEntry* entry = GetEntryPtr(identifier, resource_type, suffix);
		ECS_ASSERT_FORMAT(entry != nullptr, "ResourceManager: Trying to unprotect resource {#} of type {#} and with suffix {#}, but it doesn't exist.", identifier, ResourceTypeString(resource_type), suffix);
		ECS_ASSERT_FORMAT(entry->is_protected, "ResourceManager: Trying to unprotect resource {#} of type {#} and with suffix {#}, but it was not protected beforehand.",
			identifier, ResourceTypeString(resource_type), suffix);
		entry->is_protected = false;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::UnprotectResource(const void* resource, ResourceType resource_type, bool assert_if_not_found) {
		bool was_found = m_resource_types[(unsigned int)resource_type].ForEachIndexConst<true>([&](unsigned int entry_index) {
			ResourceManagerEntry* entry = m_resource_types[(unsigned int)resource_type].GetValuePtrFromIndex(entry_index);
			if (entry->data == resource) {
				entry->is_protected = false;
				return true;
			}
			return false;
		});
		if (assert_if_not_found) {
			ECS_ASSERT_FORMAT(was_found, "ResourceManager: Trying to unprotect a resource by pointer of type {#}, but it was not found.", ResourceTypeString(resource_type));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::RestoreSnapshot(const ResourceManagerSnapshot& snapshot, CapacityStream<char>* mismatch_string)
	{
		// This will hold a boolean value that tells the runtime if the resource in the snapshot was found
		ECS_STACK_CAPACITY_STREAM(bool, is_valid, ECS_KB * 64);

		bool return_value = true;
		for (unsigned int index = 0; index < (unsigned int)ResourceType::TypeCount; index++) {
			ResourceType current_type = (ResourceType)index;
			Stream<char> current_type_string = ResourceTypeString(current_type);
			is_valid.size = 0;
			// Mark all resources as false at the beginning
			memset(is_valid.buffer, 0, is_valid.MemoryOf(is_valid.capacity));

			m_resource_types[index].ForEachIndex([&](unsigned int resource_index) {
				ResourceManagerEntry* entry = m_resource_types[index].GetValuePtrFromIndex(resource_index);
				ResourceIdentifier identifier = m_resource_types[index].GetIdentifierFromIndex(resource_index);

				ResourceIdentifier identifier_without_suffix = { identifier.ptr, identifier.size - entry->suffix_size };

				// Check to see if the table resource is found in the snapshot.
				// If it doesn't exist, then it was added in the meantime and must be deleted
				size_t snapshot_index = snapshot.Find(identifier, current_type);
				if (snapshot_index == -1) {
					if (mismatch_string != nullptr) {
						FormatString(
							*mismatch_string, 
							"Resource {#}, type {#}, was added in between snapshots and deleted\n", 
							identifier_without_suffix.AsWide(),
							current_type_string
						);
					}
					UnloadResource(resource_index, current_type);
					return_value = false;
					return true;
				}
				else {
					// Mark the snapshot entry is valid
					is_valid[snapshot_index] = true;
				}
				return false;
			});

			// Now verify the snapshot entries to see which are missing
			for (size_t subindex = 0; subindex < snapshot.resources[index].size; subindex++) {
				if (!is_valid[subindex]) {
					if (mismatch_string != nullptr) {
						ResourceIdentifier identifier_without_suffix = snapshot.resources[index][subindex].identifier;
						identifier_without_suffix.size -= snapshot.resources[index][subindex].suffix_size;
						FormatString(
							*mismatch_string, 
							"Resource {#}, type {#}, was removed in between snapshots\n", 
							identifier_without_suffix.AsWide(),
							current_type_string
						);
					}
					return_value = false;
				}
			}
		}

		return return_value;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::RebindResource(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource, bool multithreaded_allocation, Stream<void> suffix)
	{
		unsigned int int_type = (unsigned int)resource_type;
		
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		identifier = ResourceIdentifier::WithSuffix(identifier, fully_specified_identifier, suffix);
		ResourceManagerEntry* entry = m_resource_types[int_type].TryGetValuePtr(identifier);

		ECS_ASSERT_FORMAT(entry != nullptr, "Could not rebind resource of type {#}", ResourceTypeString(resource_type));

		UNLOAD_FUNCTIONS[int_type](entry->data, this, entry->multithreaded_allocation);

		//if (success) {
		entry->data = new_resource;
		entry->multithreaded_allocation = multithreaded_allocation;
		//}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::RebindResourceNoDestruction(ResourceIdentifier identifier, ResourceType resource_type, void* new_resource, bool multithreaded_allocation, Stream<void> suffix)
	{
		unsigned int int_type = (unsigned int)resource_type;
		

		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		identifier = ResourceIdentifier::WithSuffix(identifier, fully_specified_identifier, suffix);
		ResourceManagerEntry* entry = m_resource_types[int_type].TryGetValuePtr(identifier);

		ECS_ASSERT_FORMAT(entry != nullptr, "Could not rebind resource of type {#}", ResourceTypeString(resource_type));
		//if (success) {
		entry->data = new_resource;
		entry->multithreaded_allocation = multithreaded_allocation;
		//}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::RemoveReferenceCountForResource(ResourceIdentifier identifier, ResourceType resource_type, Stream<void> suffix)
	{
		unsigned int type_index = (unsigned int)resource_type;

		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		identifier = ResourceIdentifier::WithSuffix(identifier, fully_specified_identifier, suffix);
		ResourceManagerEntry* entry = m_resource_types[type_index].TryGetValuePtr(identifier);

		ECS_ASSERT_FORMAT(entry != nullptr, "Could not remove reference count for resource of type {#}", ResourceTypeString(resource_type));
		entry->reference_count = USHORT_MAX;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ResourceManager::RemoveTimeStamp(ResourceIdentifier identifier, bool reference_count, Stream<void> suffix)
	{
		unsigned int resource_index = GetResourceIndex(identifier, ResourceType::TimeStamp, suffix);
		ECS_ASSERT(resource_index != -1, "Failed to remove time stamp");

		if (reference_count) {
			if (DecrementReferenceCount(ResourceType::TimeStamp, resource_index, 1)) {
				EvictResource(resource_index, ResourceType::TimeStamp);
				return true;
			}
		}
		else {
			EvictResource(resource_index, ResourceType::TimeStamp);
		}
		return false;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

#define EXPORT_UNLOAD(name) ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::name, Stream<wchar_t>, const ResourceManagerLoadDesc&); ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::name, unsigned int, size_t);

	template<bool reference_counted>
	void ResourceManager::UnloadTextFile(Stream<wchar_t> filename, const ResourceManagerLoadDesc& load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::TextFile, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadTextFile(unsigned int index, size_t flags) {
		UnloadResource<reference_counted>(index, ResourceType::TextFile, flags);
	}

	void ResourceManager::UnloadTextFileImplementation(char* data, bool multithreaded_allocation, size_t flags)
	{
		UnloadTextFileHandler(data, this, multithreaded_allocation);
	}

	EXPORT_UNLOAD(UnloadTextFile);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadTexture(Stream<wchar_t> filename, const ResourceManagerLoadDesc& load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::Texture, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadTexture(unsigned int index, size_t flags) {
		UnloadResource<reference_counted>(index, ResourceType::Texture, flags);
	}

	void ResourceManager::UnloadTextureImplementation(ResourceView texture, size_t flags)
	{
		// The multithreaded flag has no relevancy here
		UnloadTextureHandler(texture.view, this, false);
	}

	EXPORT_UNLOAD(UnloadTexture);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadMeshes(Stream<wchar_t> filename, const ResourceManagerLoadDesc& load_desc) {
		UnloadResource<reference_counted>(filename, ResourceType::Mesh, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadMeshes(unsigned int index, size_t flags) {;
		UnloadResource<reference_counted>(index, ResourceType::Mesh, flags);
	}

	void ResourceManager::UnloadMeshesImplementation(Stream<Mesh>* meshes, size_t flags)
	{
		// The multithreaded flag has no relevancy here
		UnloadMeshesHandler(meshes, this, false);
	}

	EXPORT_UNLOAD(UnloadMeshes);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadCoalescedMesh(Stream<wchar_t> filename, const ResourceManagerLoadDesc& load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::CoalescedMesh, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadCoalescedMesh(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::CoalescedMesh, flags);
	}

	void ResourceManager::UnloadCoalescedMeshImplementation(CoalescedMesh* mesh, bool multithreaded_allocation, size_t flags)
	{
		UnloadCoalescedMeshHandler(mesh, this, multithreaded_allocation);
	}

	EXPORT_UNLOAD(UnloadCoalescedMesh);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadMaterials(Stream<wchar_t> filename, const ResourceManagerLoadDesc& load_desc) {
		UnloadResource<reference_counted>(filename, ResourceType::PBRMaterial, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadMaterials(unsigned int index, size_t flags) {
		UnloadResource<reference_counted>(index, ResourceType::PBRMaterial, flags);
	}

	void ResourceManager::UnloadMaterialsImplementation(Stream<PBRMaterial>* materials, bool multithreaded_allocation, size_t flags)
	{
		UnloadMaterialsHandler(materials, this, multithreaded_allocation);
	}

	EXPORT_UNLOAD(UnloadMaterials);

	// ---------------------------------------------------------------------------------------------------------------------------

	void ResourceManager::UnloadUserMaterial(const UserMaterial* user_material, Material* material, const ResourceManagerLoadDesc& load_desc, const ResourceManagerUserMaterialExtraInfo& extra_info)
	{
		// Acquire the lock for the shaders and textures, the samplers and buffers don't need locking
		extra_info.Lock(this);
		__try {
			UnloadUserMaterialVertexShader(this, user_material, material, load_desc);
			UnloadUserMaterialPixelShader(this, user_material, material, load_desc);
			UnloadUserMaterialTextures(this, user_material, material, load_desc);
		}
		__finally {
			extra_info.Unlock(this);
		}

		UnloadUserMaterialBuffers(this, user_material, material);

		bool free_samplers = !HasFlag(load_desc.load_flags, ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_FREE_SAMPLERS);
		if (free_samplers) {
			UnloadUserMaterialSamplers(this, user_material);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadPBRMesh(Stream<wchar_t> filename, const ResourceManagerLoadDesc& load_desc) {
		UnloadResource<reference_counted>(filename, ResourceType::PBRMesh, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadPBRMesh(unsigned int index, size_t flags) {
		UnloadResource<reference_counted>(index, ResourceType::PBRMesh, flags);
	}

	void ResourceManager::UnloadPBRMeshImplementation(PBRMesh* mesh, bool multithreaded_allocation, size_t flags)
	{
		UnloadPBRMeshHandler(mesh, this, multithreaded_allocation);
	}

	EXPORT_UNLOAD(UnloadPBRMesh);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadShader(Stream<wchar_t> filename, const ResourceManagerLoadDesc& load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::Shader, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadShader(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::Shader, flags);
	}

	EXPORT_UNLOAD(UnloadShader);

	void ResourceManager::UnloadShaderImplementation(void* shader_interface, bool multithreaded_allocation, size_t flags)
	{
		UnloadResourceImplementation(shader_interface, ResourceType::Shader, flags);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadMisc(Stream<wchar_t> filename, const ResourceManagerLoadDesc& load_desc)
	{
		UnloadResource<reference_counted>(filename, ResourceType::Misc, load_desc);
	}

	template<bool reference_counted>
	void ResourceManager::UnloadMisc(unsigned int index, size_t flags)
	{
		UnloadResource<reference_counted>(index, ResourceType::Misc, flags);
	}

	void ResourceManager::UnloadMiscImplementation(ResizableStream<void>& data, size_t flags)
	{
		data.FreeBuffer();
	}

	EXPORT_UNLOAD(UnloadMisc);

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	void ResourceManager::UnloadResource(Stream<wchar_t> filename, ResourceType type, const ResourceManagerLoadDesc& load_desc)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, fully_specified_identifier, 512);
		ResourceIdentifier identifier = ResourceIdentifier::WithSuffix(filename, fully_specified_identifier, load_desc.identifier_suffix);
		DeleteResource<reference_counted>(this, identifier, type, load_desc.load_flags);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadResource, Stream<wchar_t>, ResourceType, const ResourceManagerLoadDesc&);

	template<bool reference_counted>
	void ResourceManager::UnloadResource(unsigned int index, ResourceType type, size_t flags)
	{
		DeleteResource<reference_counted>(this, index, type, flags);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResourceManager::UnloadResource, unsigned int, ResourceType, size_t);

	void ResourceManager::UnloadResourceImplementation(void* resource, ResourceType type, bool multithreaded_allocation, size_t flags)
	{
		UNLOAD_FUNCTIONS[(unsigned int)type](resource, this, multithreaded_allocation);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	template<bool reference_counted>
	bool ResourceManager::TryUnloadResource(Stream<wchar_t> filename, ResourceType type, const ResourceManagerLoadDesc& load_desc)
	{
		unsigned int index = GetResourceIndex(filename, type, load_desc.identifier_suffix);
		if (index != -1) {
			UnloadResource<reference_counted>(index, type, load_desc.load_flags);
			return true;
		}
		return false;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, ResourceManager::TryUnloadResource, Stream<wchar_t>, ResourceType, const ResourceManagerLoadDesc&);

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
						ResourceManagerEntry* entry = m_resource_types[index].GetValuePtrFromIndex(subindex);
						UnloadResourceImplementation(entry->data, (ResourceType)index, entry->multithreaded_allocation);

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

	void DefaultResourceManagerAllocator(ResourceManagerAllocator* allocator, GlobalMemoryManager* global_allocator)
	{
		new (allocator) ResourceManagerAllocator(
			ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_INITIAL_SIZE, 
			2048, 
			ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_BACKUP_SIZE, 
			global_allocator
		);
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	size_t DefaultResourceManagerAllocatorSize()
	{
		return ECS_RESOURCE_MANAGER_DEFAULT_MEMORY_INITIAL_SIZE;
	}

	void ResourceManagerUserMaterialExtraInfo::Lock(ResourceManager* resource_manager) const {
		if (multithreaded) {
			resource_manager->Lock();
		}
	}

	void ResourceManagerUserMaterialExtraInfo::Unlock(ResourceManager* resource_manager) const {
		if (multithreaded) {
			resource_manager->Unlock();
		}
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool PBRToMaterial(ResourceManager* resource_manager, const PBRMaterial& pbr, Stream<wchar_t> folder_to_search, Material* material)
	{
		ECS_ASSERT(false, "PBRToMaterial Function is deprecated");
		return false;

		//ShaderMacro _shader_macros[64];
		//Stream<ShaderMacro> shader_macros(_shader_macros, 0);
		//ResourceView texture_views[ECS_PBR_MATERIAL_MAPPING_COUNT] = { nullptr };

		//// For each texture, check to see if it exists and add the macro and load the texture
		//// The textures will be missing their extension, so do a search using .jpg, .png and .tiff, .bmp
		//Stream<wchar_t> texture_extensions[] = {
		//	L".jpg",
		//	L".png",
		//	L".tiff",
		//	L".bmp"
		//};

		//wchar_t* memory = (wchar_t*)ECS_STACK_ALLOC(sizeof(wchar_t) * ECS_KB * 8);

		//struct Mapping {
		//	Stream<wchar_t> texture;
		//	PBRMaterialTextureIndex index;
		//	ECS_TEXTURE_COMPRESSION_EX compression;
		//};

		//Mapping _mapping[ECS_PBR_MATERIAL_MAPPING_COUNT];
		//Stream<Mapping> mappings(_mapping, 0);
		//LinearAllocator _temporary_allocator(memory, ECS_KB * 8);
		//AllocatorPolymorphic temporary_allocator = &_temporary_allocator;

		//unsigned int macro_texture_count = 0;

		//if (pbr.color_texture.buffer != nullptr && pbr.color_texture.size > 0) {
		//	shader_macros.Add({ "COLOR_TEXTURE", "" });

		//	Stream<wchar_t> texture = StringCopy(temporary_allocator, pbr.color_texture);
		//	mappings.Add({ texture, ECS_PBR_MATERIAL_COLOR, ECS_TEXTURE_COMPRESSION_EX_COLOR });
		//	macro_texture_count++;
		//}

		//if (pbr.emissive_texture.buffer != nullptr && pbr.emissive_texture.size > 0) {
		//	shader_macros.Add({ "EMISSIVE_TEXTURE", "" });
		//	
		//	Stream<wchar_t> texture = StringCopy(temporary_allocator, pbr.emissive_texture);
		//	mappings.Add({ texture, ECS_PBR_MATERIAL_EMISSIVE, ECS_TEXTURE_COMPRESSION_EX_COLOR });
		//	macro_texture_count++;
		//}

		//if (pbr.metallic_texture.buffer != nullptr && pbr.metallic_texture.size > 0) {
		//	shader_macros.Add({ "METALLIC_TEXTURE", "" });

		//	Stream<wchar_t> texture = StringCopy(temporary_allocator, pbr.metallic_texture);
		//	mappings.Add({ texture, ECS_PBR_MATERIAL_METALLIC, ECS_TEXTURE_COMPRESSION_EX_GRAYSCALE });
		//	macro_texture_count++;
		//}

		//if (pbr.normal_texture.buffer != nullptr && pbr.normal_texture.size > 0) {
		//	shader_macros.Add({ "NORMAL_TEXTURE", "" });

		//	Stream<wchar_t> texture = StringCopy(temporary_allocator, pbr.normal_texture);
		//	mappings.Add({ texture, ECS_PBR_MATERIAL_NORMAL, ECS_TEXTURE_COMPRESSION_EX_TANGENT_SPACE_NORMAL });
		//	macro_texture_count++;
		//}

		//if (pbr.occlusion_texture.buffer != nullptr && pbr.occlusion_texture.size > 0) {
		//	shader_macros.Add({ "OCCLUSION_TEXTURE", "" });

		//	Stream<wchar_t> texture = StringCopy(temporary_allocator, pbr.occlusion_texture);
		//	mappings.Add({ texture, ECS_PBR_MATERIAL_OCCLUSION, ECS_TEXTURE_COMPRESSION_EX_GRAYSCALE });
		//	macro_texture_count++;
		//}

		//if (pbr.roughness_texture.buffer != nullptr && pbr.roughness_texture.size > 0) {
		//	shader_macros.Add({ "ROUGHNESS_TEXTURE", "" });

		//	Stream<wchar_t> texture = StringCopy(temporary_allocator, pbr.roughness_texture);
		//	mappings.Add({ texture, ECS_PBR_MATERIAL_ROUGHNESS, ECS_TEXTURE_COMPRESSION_EX_GRAYSCALE });
		//	macro_texture_count++;
		//}

		//struct FunctorData {
		//	AllocatorPolymorphic allocator;
		//	Stream<Mapping>* mappings;
		//};

		//// Search the directory for the textures
		//auto search_functor = [](Stream<wchar_t> path, void* _data) {
		//	FunctorData* data = (FunctorData*)_data;

		//	Stream<wchar_t> filename = PathStem(path);

		//	for (size_t index = 0; index < data->mappings->size; index++) {
		//		if (filename == data->mappings->buffer[index].texture) {
		//			Stream<wchar_t> new_texture = StringCopy(data->allocator, path);
		//			data->mappings->buffer[index].texture = new_texture;
		//			data->mappings->Swap(index, data->mappings->size - 1);
		//			data->mappings->size--;
		//			return data->mappings->size > 0;
		//		}
		//	}
		//	return data->mappings->size > 0;
		//};

		//size_t texture_count = mappings.size;
		//FunctorData functor_data = { temporary_allocator, &mappings };
		//ForEachFileInDirectoryRecursiveWithExtension(folder_to_search, { texture_extensions, ECS_COUNTOF(texture_extensions) }, &functor_data, search_functor);
		//ECS_ASSERT(mappings.size == 0);
		//if (mappings.size != 0) {
		//	return false;
		//}

		//GraphicsContext* context = resource_manager->m_graphics->GetContext();

		//ResourceManagerTextureDesc texture_descriptor;
		//texture_descriptor.context = context;
		//texture_descriptor.usage = ECS_GRAPHICS_USAGE_DEFAULT;

		//for (size_t index = 0; index < texture_count; index++) {
		//	texture_descriptor.compression = mappings[index].compression;
		//	texture_views[mappings[index].index] = resource_manager->LoadTextureImplementation(mappings[index].texture.buffer, &texture_descriptor);
		//}

		//// Compile the shaders and reflect their buffers and structures
		//ShaderCompileOptions options;
		//options.macros = shader_macros;

		//ECS_MESH_INDEX _vertex_mappings[8];
		//CapacityStream<ECS_MESH_INDEX> vertex_mappings(_vertex_mappings, 0, 8);

		//Stream<char> shader_source;
		//Stream<void> byte_code;
		//material->vertex_shader = resource_manager->LoadShaderImplementation(ECS_VERTEX_SHADER_SOURCE(PBR), ECS_SHADER_VERTEX, &shader_source, &byte_code);
		//ECS_ASSERT(material->vertex_shader.shader != nullptr);
		//material->layout = resource_manager->m_graphics->ReflectVertexShaderInput(shader_source, byte_code);
		//bool success = resource_manager->m_graphics->ReflectVertexBufferMapping(shader_source, vertex_mappings);
		//ECS_ASSERT(success);
		//memcpy(material->vertex_buffer_mappings, _vertex_mappings, vertex_mappings.size * sizeof(ECS_MESH_INDEX));
		//material->vertex_buffer_mapping_count = vertex_mappings.size;
		//resource_manager->Deallocate(shader_source.buffer);

		//material->pixel_shader = resource_manager->LoadShaderImplementation(ECS_PIXEL_SHADER_SOURCE(PBR), ECS_SHADER_PIXEL, nullptr, nullptr, options);
		//ECS_ASSERT(material->pixel_shader.shader != nullptr);

		//material->v_buffers[0] = Shaders::CreatePBRVertexConstants(resource_manager->m_graphics);
		//material->v_buffer_slot[0] = 0;
		//material->v_buffer_count = 1;

		//for (size_t index = 0; index < macro_texture_count; index++) {
		//	material->p_textures[index] = texture_views[mappings[index].index];
		//	// The slot is + 3 because of the environment maps
		//	material->p_texture_slot[index] = 3 + mappings[index].index;
		//}
		//material->p_texture_count = macro_texture_count;

		//material->p_buffers[0] = Shaders::CreatePBRPixelConstants(resource_manager->m_graphics);
		//material->p_buffer_slot[0] = 2;
		//material->p_buffer_count = 1;

		//return true;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	bool ReloadUserMaterial(
		ResourceManager* resource_manager,
		const UserMaterial* old_user_material,
		const UserMaterial* new_user_material,
		Material* material,
		ReloadUserMaterialOptions options,
		const ResourceManagerLoadDesc& load_descriptor,
		const ResourceManagerUserMaterialExtraInfo& extra_info
	)
	{
		Material temporary_material;

		// TODO: Determine what to do on failure for these cases

		// Validate the new material - if it is not valid do not proceed.
		if (new_user_material->vertex_shader.size == 0 || new_user_material->pixel_shader.size == 0) {
			// Deallocate the material and return
			//resource_manager->UnloadUserMaterial(old_user_material, material, load_descriptor);
			return false;
		}

		// When reloading resources, firstly load and then unload the old values because
		// if some resources have not changed loading will increment the reference count
		// and prevent them from being released from memory.

		// Acquire the multithreading lock, if needed. It is required by the shaders and textures, the samplers
		// And the buffers don't need that.
		bool success = true;
		extra_info.Lock(resource_manager);
		__try {
			if (options.reload_vertex_shader) {
				success = LoadUserMaterialVertexShader(resource_manager, new_user_material, &temporary_material, load_descriptor);
				if (!success) {
					// Deallocate the material
					//resource_manager->UnloadUserMaterial(old_user_material, material, load_descriptor);
					success = false;
					__leave;
				}

				UnloadUserMaterialVertexShader(resource_manager, old_user_material, material, load_descriptor);
			}

			if (options.reload_pixel_shader) {
				success = LoadUserMaterialPixelShader(resource_manager, new_user_material, &temporary_material, load_descriptor);
				if (!success) {
					// We don't have to unload the vertex shader if it was reloaded - that is not the correct behaviour
					// Deallocate the material
					//resource_manager->UnloadUserMaterial(old_user_material, material, load_descriptor);
					success = false;
					__leave;
				}

				UnloadUserMaterialPixelShader(resource_manager, old_user_material, material, load_descriptor);
			}

			if (options.reload_textures) {
				success = LoadUserMaterialTextures(resource_manager, new_user_material, &temporary_material, load_descriptor);
				if (!success) {
					// We don't have to unload the shaders if they were reloaded - that is not the correct behaviour
					// Deallocate the material
					//resource_manager->UnloadUserMaterial(old_user_material, material, load_descriptor);
					success = false;
					__leave;
				}

				UnloadUserMaterialTextures(resource_manager, old_user_material, material, load_descriptor);
			}
		}
		__finally {
			extra_info.Unlock(resource_manager);
		}

		if (!success) {
			return false;
		}

		if (options.reload_buffers) {
			// Here we can unload and then load since these are not affected by reference counts
			UnloadUserMaterialBuffers(resource_manager, old_user_material, material);
			// Reset the buffers
			material->ResetConstantBuffers();
			LoadUserMaterialBuffers(resource_manager, new_user_material, material);
		}

		if (options.reload_samplers) {
			if (!HasFlag(load_descriptor.load_flags, ECS_RESOURCE_MANAGER_USER_MATERIAL_DONT_FREE_SAMPLERS)) {
				// Here we can unload and then load since these are not affected by reference counts
				UnloadUserMaterialSamplers(resource_manager, old_user_material);
			}
			material->ResetSamplers();
			LoadUserMaterialSamplers(resource_manager, new_user_material, material);
		}

		// Copy now the values from the temporary material - only for shaders and textures
		if (options.reload_vertex_shader) {
			material->CopyVertexShader(&temporary_material);
		}
		if (options.reload_pixel_shader) {
			material->CopyPixelShader(&temporary_material);
		}
		if (options.reload_textures) {
			material->CopyTextures(&temporary_material);
		}

		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

#pragma endregion

}