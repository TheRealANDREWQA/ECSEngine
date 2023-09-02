#include "ecspch.h"
#include "EntityManager.h"
#include "EntityManagerSerialize.h"

#include "../Utilities/Serialization/SerializationHelpers.h"
#include "../Utilities/StackScope.h"

#include "../Utilities/Reflection/Reflection.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "LinkComponents.h"
#include "ComponentHelpers.h"

#include "../Utilities/CrashHandler.h"
#include "../Utilities/Crash.h"

#define ENTITY_MANAGER_SERIALIZE_VERSION 0

#define ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY ECS_MB * 100

namespace ECSEngine {

	struct SerializeEntityManagerHeader {
		unsigned int version;
		unsigned int archetype_count;
		unsigned short component_count;
		unsigned short shared_component_count;
	};

	// The allocator size and the buffer offsets
	struct ComponentFixup {
		size_t allocator_size;
		ComponentBuffer component_buffers[ECS_COMPONENT_INFO_MAX_BUFFER_COUNT];
		unsigned short component_buffer_count;
	};

	// We are not storing the allocator size nor the buffer offsets because
	// the type could have changed since the serialization. Push the responsability
	// onto the serialize function to give us these parameters
	struct ComponentPair {
		unsigned int header_data_size;
		unsigned short component_byte_size;
		Component component;
		unsigned char serialize_version;
		unsigned char name_size;
	};

	// We are not storing the allocator size nor the buffer offsets because
	// the type could have changed since the serialization. Push the responsability
	// onto the serialize function to give us these parameters
	struct SharedComponentPair {
		unsigned int header_data_size;
		unsigned short component_byte_size;
		Component component;
		unsigned char serialize_version;
		unsigned char name_size;

		unsigned short instance_count;
		unsigned short named_instance_count;
	};

	struct ArchetypeHeader {
		unsigned short base_count;
		unsigned char unique_count;
		unsigned char shared_count;
	};

	/*
		Serialization Order:
		
		SerializeEntityManagerHeader header;

		ComponentPair* component_pairs;
		SharedComponentPair* shared_component_pairs;
		Component names (the characters only);
		SharedComponent names (the characters only);

		SharedComponentInstances - these are needed in order to remapp the instances stored in the base archetypes
		to the new instances that are created sequentially with RegisterSharedInstance

		InitializeComponent data (bytes);
		InitializeSharedComponent data (bytes);

		NamedSharedInstances* tuple (instance, ushort identifier size) followed by identifier.ptr stream;

		SharedComponent data (for each instance);
		ArchetypeHeader* archetype_headers;
		ArchetypeSignatures (unique and shared) indices only;
		ArchetypeSharedInstances values only;
		ArchetypeBase sizes unsigned ints;

		ArchetypeBase unique components;

		Component Remapping (when a component was changed from 2 -> 102 for example)

		EntityPool;
		EntityHierarchy
	*/

	// ------------------------------------------------------------------------------------------------------------------------------------------

	bool SerializeEntityManager(
		const EntityManager* entity_manager,
		Stream<wchar_t> filename,
		const SerializeEntityManagerComponentTable* component_table,
		const SerializeEntityManagerSharedComponentTable* shared_component_table
	)
	{
		ECS_FILE_HANDLE file_handle = -1;
		ECS_FILE_STATUS_FLAGS status = FileCreate(filename, &file_handle, ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE);
		if (status != ECS_FILE_STATUS_OK) {
			return false;
		}

		bool success = SerializeEntityManager(entity_manager, file_handle, component_table, shared_component_table);
		CloseFile(file_handle);
		if (!success) {
			RemoveFile(filename);
		}

		return success;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	bool SerializeEntityManager(const EntityManager* entity_manager, ECS_FILE_HANDLE file_handle, const SerializeEntityManagerComponentTable* component_table, const SerializeEntityManagerSharedComponentTable* shared_component_table)
	{
		void* buffering = malloc(ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
		unsigned int buffering_size = 0;

		struct DeallocateResources {
			void operator() () const {
				free(component_buffering);
			}

			void* component_buffering;
		};

		StackScope<DeallocateResources> deallocate_resources({ buffering });

		// Write the header first
		SerializeEntityManagerHeader* header = (SerializeEntityManagerHeader*)buffering;
		buffering_size = sizeof(*header);

		header->version = ENTITY_MANAGER_SERIALIZE_VERSION;
		header->archetype_count = entity_manager->m_archetypes.size;
		header->component_count = 0;
		header->shared_component_count = 0;

		ComponentPair* component_pairs = (ComponentPair*)function::OffsetPointer(buffering, buffering_size);

		// Determine the component count
		ComponentPair* current_unique_pair = component_pairs;
		for (unsigned int index = 0; index < entity_manager->m_unique_components.size; index++) {
			Component component = { (short)index };

			bool exists = entity_manager->ExistsComponent(component);
			header->component_count += exists;

			if (exists) {
				// Check to see if it has a name and get its version
				SerializeEntityManagerComponentInfo component_info = component_table->GetValue(component);

				// Write the serialize version
				current_unique_pair->serialize_version = component_info.version;
				current_unique_pair->name_size = component_info.name.size;
				current_unique_pair->component_byte_size = entity_manager->m_unique_components[index].size;
				current_unique_pair->component = component;
				current_unique_pair->header_data_size = 0;

				current_unique_pair++;
				buffering_size += sizeof(ComponentPair);
			}
		}

		SharedComponentPair* shared_component_pairs = (SharedComponentPair*)function::OffsetPointer(buffering, buffering_size);

		SharedComponentPair* current_shared_pair = shared_component_pairs;
		for (unsigned int index = 0; index < entity_manager->m_shared_components.size; index++) {
			Component component = { (short)index };

			bool exists = entity_manager->ExistsSharedComponent({ (short)index });
			header->shared_component_count += exists;

			if (exists) {
				// Check to see if it has a name and get its version
				SerializeEntityManagerSharedComponentInfo component_info = shared_component_table->GetValue(component);

				// Write the serialize version
				current_shared_pair->serialize_version = component_info.version;
				current_shared_pair->component = component;
				current_shared_pair->component_byte_size = entity_manager->m_shared_components[index].info.size;
				current_shared_pair->name_size = component_info.name.size;
				current_shared_pair->instance_count = (unsigned short)entity_manager->m_shared_components[index].instances.stream.size;
				current_shared_pair->named_instance_count = (unsigned short)entity_manager->m_shared_components[index].named_instances.GetCount();
				current_shared_pair->header_data_size = 0;

				current_shared_pair++;
				buffering_size += sizeof(SharedComponentPair);
			}
		}

		ECS_ASSERT(buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

		// Keep a stack copy of the components and shared_components that are registered such that it will make the iteration over the components faster
		Component* registered_components = (Component*)ECS_STACK_ALLOC(sizeof(Component) * header->component_count);
		Component* registered_shared_components = (Component*)ECS_STACK_ALLOC(sizeof(Component) * header->shared_component_count);
		
		for (unsigned short index = 0; index < header->component_count; index++) {
			registered_components[index] = component_pairs[index].component;
		}

		for (unsigned short index = 0; index < header->shared_component_count; index++) {
			registered_shared_components[index] = shared_component_pairs[index].component;
		}

		// Write the names now - components and then shared components
		for (unsigned int index = 0; index < header->component_count; index++) {
			Component component = registered_components[index];

			// Check to see if it has a name and get its version
			SerializeEntityManagerComponentInfo component_info = component_table->GetValue(component);

			if (component_info.name.size > 0) {
				memcpy(function::OffsetPointer(buffering, buffering_size), component_info.name.buffer, component_info.name.size);
				buffering_size += component_info.name.size;
			}
		}

		for (unsigned int index = 0; index < header->shared_component_count; index++) {
			Component component = registered_shared_components[index];

			// Check to see if it has a name and get its version
			SerializeEntityManagerSharedComponentInfo component_info = shared_component_table->GetValue(component);

			if (component_info.name.size > 0) {
				memcpy(function::OffsetPointer(buffering, buffering_size), component_info.name.buffer, component_info.name.size);
				buffering_size += component_info.name.size;
			}
		}

		ECS_ASSERT(buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

		for (unsigned int index = 0; index < header->shared_component_count; index++) {
			CapacityStream<SharedInstance> instances = { function::OffsetPointer(buffering, buffering_size), 0, ECS_KB * 64 };
			entity_manager->GetSharedComponentInstanceAll(shared_component_pairs[index].component, instances);
			buffering_size += instances.MemoryOf(instances.size);
		}

		ECS_ASSERT(buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

		void* header_component_data = function::OffsetPointer(buffering, buffering_size);
		for (unsigned int index = 0; index < header->component_count; index++) {
			SerializeEntityManagerComponentInfo component_info = component_table->GetValue(component_pairs[index].component);
			if (component_info.header_function != nullptr) {
				SerializeEntityManagerHeaderComponentData header_data;
				header_data.buffer = header_component_data;
				header_data.buffer_capacity = ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY - buffering_size;
				header_data.extra_data = component_info.extra_data;

				unsigned int write_size = component_info.header_function(&header_data);
				if (write_size == -1) {
					return false;
				}
				ECS_ASSERT(write_size <= header_data.buffer_capacity);

				component_pairs[index].header_data_size = write_size;
				header_component_data = function::OffsetPointer(header_component_data, write_size);
				buffering_size += write_size;
			}
		}

		ECS_ASSERT(buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
		void* header_shared_component_data = function::OffsetPointer(buffering, buffering_size);
		for (unsigned int index = 0; index < header->shared_component_count; index++) {
			SerializeEntityManagerSharedComponentInfo component_info = shared_component_table->GetValue(shared_component_pairs[index].component);
			if (component_info.header_function != nullptr) {
				SerializeEntityManagerHeaderSharedComponentData header_data;
				header_data.buffer = header_shared_component_data;
				header_data.buffer_capacity = ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY - buffering_size;
				header_data.extra_data = component_info.extra_data;

				unsigned int write_size = component_info.header_function(&header_data);
				if (write_size == -1) {
					return false;
				}
				ECS_ASSERT(write_size <= header_data.buffer_capacity);

				shared_component_pairs[index].header_data_size = write_size;
				header_shared_component_data = function::OffsetPointer(header_shared_component_data, write_size);
				buffering_size += write_size;
			}
		}

		ECS_ASSERT(buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
		uintptr_t shared_component_buffering_instances_ptr = (uintptr_t)function::OffsetPointer(buffering, buffering_size);

		// Now write the named shared instances
		for (unsigned int index = 0; index < header->shared_component_count; index++) {
			Component current_component = registered_shared_components[index];
			// Write the pairs as shared instance, identifier size and at the end the buffers
			// This is done like this in order to avoid on deserialization to read small chunks
			// In this way all the pairs can be read at a time and then the identifiers
			entity_manager->m_shared_components[current_component].named_instances.ForEachConst([&](SharedInstance instance, ResourceIdentifier identifier) {
				Write<true>(&shared_component_buffering_instances_ptr, &instance, sizeof(instance));
				// It will make it easier to access them as a pair of unsigned shorts when deserializing
				unsigned short short_size = (unsigned short)identifier.size;
				Write<true>(&shared_component_buffering_instances_ptr, &short_size, sizeof(short_size));
				buffering_size += sizeof(instance) + sizeof(short_size);
			});
		}

		// Write the identifiers pointer data now
		for (unsigned int index = 0; index < header->shared_component_count; index++) {
			Component current_component = registered_shared_components[index];
			entity_manager->m_shared_components[current_component].named_instances.ForEachConst([&](SharedInstance instance, ResourceIdentifier identifier) {
				Write<true>(&shared_component_buffering_instances_ptr, identifier.ptr, identifier.size);
				buffering_size += identifier.size;
			});
		}
		ECS_ASSERT(buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

		// The instance data sizes for each shared component must be written before the actual data if the extract function is used
		// Because otherwise the data cannot be recovered
		for (unsigned int index = 0; index < header->shared_component_count; index++) {
			Component current_component = registered_shared_components[index];
			SerializeEntityManagerSharedComponentInfo component_info = shared_component_table->GetValue(current_component);
			unsigned int* instance_data_sizes = (unsigned int*)function::OffsetPointer(buffering, buffering_size);
			buffering_size += sizeof(unsigned int) * entity_manager->m_shared_components[current_component].instances.stream.size;

			unsigned int write_index = 0;
			bool write_success = true;
			entity_manager->m_shared_components[current_component].instances.stream.ForEachIndex([&](unsigned int instance_index) {
				SerializeEntityManagerSharedComponentData function_data;
				function_data.buffer = function::OffsetPointer(buffering, buffering_size);
				function_data.buffer_capacity = ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY - buffering_size;
				function_data.component_data = entity_manager->m_shared_components[current_component].instances[instance_index];
				function_data.extra_data = component_info.extra_data;
				function_data.instance = { (short)instance_index };

				unsigned int write_count = component_info.function(&function_data);
				if (write_count == -1) {
					write_success = false;
					return;
				}

				// Assert that the buffering is enough - otherwise it complicates things a lot
				ECS_ASSERT(write_count + buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
				buffering_size += write_count;
				// Write the count into the the header of sizes
				instance_data_sizes[write_index++] = write_count;
			});

			if (!write_success) {
				return false;
			}
		}

		bool success = true;

		// Flush the buffering - if any data is still left
		if (buffering_size > 0) {
			success = WriteFile(file_handle, { buffering, buffering_size });
			if (!success) {
				return false;
			}
			buffering_size = 0;
		}


		// For each archetype, we must serialize the shared and unique components, and then each base archetype
		// For each base archetype its shared components must be written alongside the buffers (the entities can be omitted 
		// because they can be deduced from the entity pool)

		// Go through the archetypes and serialize a "header" such that reading the data is going to be loaded in chunks
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			ComponentSignature unique = archetype->GetUniqueSignature();
			ComponentSignature shared = archetype->GetSharedSignature();
			unsigned int base_count = archetype->GetBaseCount();

			ArchetypeHeader* archetype_header = (ArchetypeHeader*)function::OffsetPointer(buffering, buffering_size);

			archetype_header->base_count = base_count;
			archetype_header->shared_count = shared.count;
			archetype_header->unique_count = unique.count;

			buffering_size += sizeof(*archetype_header);
		}

		// The unique and shared components can be written now
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			ComponentSignature unique = archetype->GetUniqueSignature();
			ComponentSignature shared = archetype->GetSharedSignature();
			unsigned int base_count = archetype->GetBaseCount();

			uintptr_t temp_ptr = (uintptr_t)function::OffsetPointer(buffering, buffering_size);

			Write<true>(&temp_ptr, unique.indices, sizeof(Component) * unique.count);
			Write<true>(&temp_ptr, shared.indices, sizeof(Component) * shared.count);

			// Reset the size
			buffering_size += sizeof(Component) * (unique.count + shared.count);
		}

		// The shared instances can be written now
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			ComponentSignature unique = archetype->GetUniqueSignature();
			ComponentSignature shared = archetype->GetSharedSignature();
			unsigned int base_count = archetype->GetBaseCount();

			uintptr_t temp_ptr = (uintptr_t)function::OffsetPointer(buffering, buffering_size);

			// Write the shared instances for each base archetype in a single stream
			for (size_t base_index = 0; base_index < base_count; base_index++) {
				Write<true>(&temp_ptr, archetype->GetBaseInstances(base_index), sizeof(SharedInstance) * shared.count);
				buffering_size += sizeof(SharedInstance) * shared.count;
			}
		}

		// The base archetype sizes can be written now
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			unsigned int base_count = archetype->GetBaseCount();

			uintptr_t temp_ptr = (uintptr_t)function::OffsetPointer(buffering, buffering_size);
			// Write the size of each base archetype
			for (size_t base_index = 0; base_index < base_count; base_index++) {
				const ArchetypeBase* base = archetype->GetBase(base_index);
				ECS_ASSERT(base->m_size > 0);
				Write<true>(&temp_ptr, &base->m_size, sizeof(base->m_size));
				buffering_size += sizeof(base->m_size);
			}
		}

		ECS_ASSERT(buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

		// Now serialize the actual unique components - finally!
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			ComponentSignature unique = archetype->GetUniqueSignature();
			unsigned int base_count = archetype->GetBaseCount();

			// The size of each archetype was previously written
			// Now only the buffers must be flushed
			for (size_t base_index = 0; base_index < base_count; base_index++) {
				ArchetypeBase* base = (ArchetypeBase*)archetype->GetBase(base_index);
				for (size_t component_index = 0; component_index < unique.count; component_index++) {
					unsigned int* current_write_count = (unsigned int*)function::OffsetPointer(buffering, buffering_size);;
					buffering_size += sizeof(unsigned int);

					SerializeEntityManagerComponentInfo component_info = component_table->GetValue(unique.indices[component_index]);
					SerializeEntityManagerComponentData function_data;
					function_data.buffer = function::OffsetPointer(current_write_count, sizeof(unsigned int));
					function_data.buffer_capacity = ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY - buffering_size;
					function_data.components = base->GetComponentByIndex(0, component_index);
					function_data.count = base->m_size;
					function_data.extra_data = component_info.extra_data;

					unsigned int write_count = component_info.function(&function_data);
					if (write_count == -1) {
						return false;
					}
					ECS_ASSERT(write_count <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

					*current_write_count = write_count;

					if (write_count + buffering_size > ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY) {
						// Commit to file now
						success = WriteFile(file_handle, { buffering, buffering_size });
						if (!success) {
							return false;
						}
						buffering_size = 0;
						// Recall the function again
						write_count = component_info.function(&function_data);
						ECS_ASSERT(write_count <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
						buffering_size += write_count;
					}
					else {
						buffering_size += write_count;
					}
				}
			}
		}

		// If there is still data left in the component buffering - commit it
		if (buffering_size > 0) {
			success = WriteFile(file_handle, { buffering, buffering_size });
			if (!success) {
				return false;
			}
			buffering_size = 0;
		}

		// Write the entity pool
		success = SerializeEntityPool(entity_manager->m_entity_pool, file_handle);
		if (!success) {
			return false;
		}

		return SerializeEntityHierarchy(&entity_manager->m_hierarchy, file_handle);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void DeserializeEntityManagerInitializeRemappSharedInstances(
		const SharedComponentPair* shared_pairs,
		Component* shared_components,
		unsigned int* shared_instance_offsets,
		unsigned int shared_pair_count
	) {
		shared_instance_offsets[0] = 0;

		for (unsigned int index = 1; index <= shared_pair_count; index++) {
			unsigned int index_minus_1 = index - 1;
			shared_instance_offsets[index] = shared_instance_offsets[index_minus_1] + shared_pairs[index_minus_1].instance_count;
			shared_components[index_minus_1] = shared_pairs[index_minus_1].component;
		}
	}

	// The shared pairs needs to be a separate buffer in order to have fast SIMD compare
	// The shared instance offsets are the offsets to reach that component's instances
	// The shared_instance_offsets[shared_pair_count] needs to be the total count of instances
	void DeserializeEntityManagerRemappSharedInstances(
		const Component* component_stream,
		const SharedInstance* shared_instances,
		const unsigned int* shared_instance_offsets,
		unsigned int shared_pair_count,
		Archetype* archetype
	) {
		if (archetype->GetSharedSignature().count == 0) {
			return;
		}

		ComponentSignature shared_signature_basic = archetype->GetSharedSignature();
		unsigned char signature_count = shared_signature_basic.count;
		unsigned int shared_pair_index[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
		
		unsigned char found_component_count = 0;

		for (unsigned char index = 0; index < signature_count; index++) {
			shared_pair_index[index] = function::SearchBytes(component_stream, shared_pair_count, shared_signature_basic[index].value, sizeof(Component));
			ECS_ASSERT(shared_pair_index[index] != -1);
		}

		unsigned int base_count = archetype->GetBaseCount();
		for (unsigned int index = 0; index < base_count; index++) {
			SharedInstance* base_instances = archetype->GetBaseInstances(index);
			for (unsigned char component_index = 0; component_index < signature_count; component_index++) {
				unsigned int component_offset = shared_instance_offsets[shared_pair_index[component_index]];
				unsigned int instance_count = shared_instance_offsets[shared_pair_index[component_index] + 1] - component_offset;
				size_t remapp_value = function::SearchBytes(shared_instances + component_offset, instance_count, base_instances[component_index].value, sizeof(SharedInstance));
				ECS_ASSERT(remapp_value != -1);
				base_instances[component_index] = { (short)remapp_value };
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		Stream<wchar_t> filename,
		const DeserializeEntityManagerComponentTable* component_table,
		const DeserializeEntityManagerSharedComponentTable* shared_component_table
	)
	{
		ECS_FILE_HANDLE file_handle = -1;
		ECS_FILE_STATUS_FLAGS status = OpenFile(filename, &file_handle, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_BINARY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL);
		if (status != ECS_FILE_STATUS_OK) {
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_OPEN_FILE;
		}

		ECS_DESERIALIZE_ENTITY_MANAGER_STATUS deserialize_status = DeserializeEntityManager(entity_manager, file_handle, component_table, shared_component_table);
		CloseFile(file_handle);
		return deserialize_status;
	}

	// The functor must implement the functions:
	// void* AllocateAndRead(size_t size, bool* allocation_failure); Return nullptr if the read failed or the allocation
	// bool ReadInto(void* ptr, size_t size);
	// void FreeBuffering();
	// void MarkBufferingResetPoint();
	// void ResetBuffering();
	// void ClearBuffering();
	// bool HasBuffering();
	// auto ReadInstrument(); Must be uintptr_t& for the in memory version and ECS_FILE_HANDLE for the file version
	template<typename Functor>
	ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManagerImpl(
		EntityManager* entity_manager,
		const DeserializeEntityManagerComponentTable* component_table,
		const DeserializeEntityManagerSharedComponentTable* shared_component_table,
		Functor&& functor
	) {
		bool previous_assert_crash = ECS_GLOBAL_ASSERT_CRASH;
		ECS_GLOBAL_ASSERT_CRASH = true;

		// Call the crash handler recovery such that it won't try to crash the entire editor
		ECS_SET_RECOVERY_CRASH_HANDLER(false) {
			ResetRecoveryCrashHandler();
			functor.FreeBuffering();
			ECS_GLOBAL_ASSERT_CRASH = previous_assert_crash;
			return ECS_DESERIALIZE_ENTITY_MANAGER_FILE_IS_CORRUPT;
		}

		struct Deallocator {
			void operator()() {
				// Don't forget to reset the crash handler
				functor.FreeBuffering();
				ResetRecoveryCrashHandler();
				ECS_GLOBAL_ASSERT_CRASH = assert_crash_value;
			}

			Functor functor;
			bool assert_crash_value;
		};

		StackScope<Deallocator> scope_deallocator({ functor, previous_assert_crash });

		// Read the header first
		SerializeEntityManagerHeader header;
		bool success = functor.ReadInto(&header, sizeof(header));
		if (!success) {
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
		}

		if (header.version != ENTITY_MANAGER_SERIALIZE_VERSION || header.archetype_count > ECS_MAIN_ARCHETYPE_MAX_COUNT) {
			return ECS_DESERIALIZE_ENTITY_MANAGER_HEADER_IS_INVALID;
		}

		// Too many components
		if (header.component_count > USHORT_MAX || header.shared_component_count > USHORT_MAX) {
			return ECS_DESERIALIZE_ENTITY_MANAGER_HEADER_IS_INVALID;
		}

		// Read the ComponentPairs and the SharedComponentPairs first
		unsigned int component_pair_size = header.component_count * sizeof(ComponentPair);
		unsigned int shared_component_pair_size = header.shared_component_count * sizeof(SharedComponentPair);

		bool allocation_failure = false;
		auto allocate_and_read_failure = [&]() {
			return allocation_failure ? ECS_DESERIALIZE_ENTITY_MANAGER_TOO_MUCH_DATA : ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
		};

		ComponentPair* component_pairs = (ComponentPair*)functor.AllocateAndRead(component_pair_size + shared_component_pair_size, &allocation_failure);
		if (component_pairs == nullptr) {
			return allocate_and_read_failure();
		}
		SharedComponentPair* shared_component_pairs = (SharedComponentPair*)function::OffsetPointer(component_pairs, component_pair_size);

		unsigned int component_name_total_size = 0;
		unsigned int header_component_total_size = 0;
		for (unsigned int index = 0; index < header.component_count; index++) {
			component_name_total_size += component_pairs[index].name_size;
			header_component_total_size += component_pairs[index].header_data_size;
		}

		// Now determine how many named shared instances are and the name sizes
		unsigned int shared_instance_total_count = 0;
		unsigned int shared_component_name_total_size = 0;
		unsigned int named_shared_instances_count = 0;
		unsigned int header_shared_component_total_size = 0;
		for (unsigned int index = 0; index < header.shared_component_count; index++) {
			named_shared_instances_count += shared_component_pairs[index].named_instance_count;
			shared_component_name_total_size += shared_component_pairs[index].name_size;
			header_shared_component_total_size += shared_component_pairs[index].header_data_size;
			shared_instance_total_count += shared_component_pairs[index].instance_count;
		}

		// Read all the names, for the unique and shared alongside, the headers for the components and the headers of the named shared instances - 
		// the shared instance itself and the size of the pointer
		struct NamedSharedInstanceHeader {
			SharedInstance instance;
			unsigned short identifier_size;
		};

		unsigned int total_size_to_read = sizeof(NamedSharedInstanceHeader) * named_shared_instances_count + component_name_total_size
			+ shared_component_name_total_size + header_component_total_size + header_shared_component_total_size + sizeof(SharedInstance) * shared_instance_total_count;
		void* unique_name_characters = functor.AllocateAndRead(total_size_to_read, &allocation_failure);
		if (unique_name_characters == nullptr) {
			return allocate_and_read_failure();
		}

		void* shared_name_characters = function::OffsetPointer(unique_name_characters, component_name_total_size);
		SharedInstance* registered_shared_instances = (SharedInstance*)function::OffsetPointer(shared_name_characters, shared_component_name_total_size);
		NamedSharedInstanceHeader* named_headers = (NamedSharedInstanceHeader*)function::OffsetPointer(registered_shared_instances, sizeof(SharedInstance) * shared_instance_total_count);
		void* header_component_data = function::OffsetPointer(named_headers, sizeof(NamedSharedInstanceHeader) * named_shared_instances_count);
		void* header_shared_component_data = function::OffsetPointer(header_component_data, header_component_total_size);

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, component_name_offsets, header.component_count);
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, shared_component_name_offsets, header.shared_component_count);

		// Construct the named lookup tables
		unsigned int current_unique_name_offset = 0;
		for (unsigned int index = 0; index < header.component_count; index++) {
			if (component_pairs[index].name_size > 0) {
				component_name_offsets[index] = current_unique_name_offset;
				current_unique_name_offset += component_pairs[index].name_size;
			}
		}

		unsigned int current_shared_name_offset = 0;
		for (unsigned int index = 0; index < header.shared_component_count; index++) {
			if (shared_component_pairs[index].name_size > 0) {
				shared_component_name_offsets[index] = current_shared_name_offset;
				current_shared_name_offset += shared_component_pairs[index].name_size;
			}
		}

		struct CachedComponentInfo {
			const DeserializeEntityManagerComponentInfo* info;
			Component found_at;
			unsigned char version;
		};

		struct CachedSharedComponentInfo {
			ECS_INLINE CachedSharedComponentInfo() {}
			ECS_INLINE CachedSharedComponentInfo(CachedComponentInfo simple_info) {
				info = (decltype(info))simple_info.info;
				found_at = simple_info.found_at;
			}

			const DeserializeEntityManagerSharedComponentInfo* info;
			Component found_at;
		};

		// Here we assert that the 2 structures are the same. We need this for the moment
		static_assert(offsetof(DeserializeEntityManagerComponentInfo, function) == offsetof(DeserializeEntityManagerSharedComponentInfo, function));
		static_assert(offsetof(DeserializeEntityManagerComponentInfo, header_function) == offsetof(DeserializeEntityManagerSharedComponentInfo, header_function));
		static_assert(offsetof(DeserializeEntityManagerComponentInfo, extra_data) == offsetof(DeserializeEntityManagerSharedComponentInfo, extra_data));
		static_assert(offsetof(DeserializeEntityManagerComponentInfo, component_fixup) == offsetof(DeserializeEntityManagerSharedComponentInfo, component_fixup));
		static_assert(offsetof(DeserializeEntityManagerComponentInfo, name) == offsetof(DeserializeEntityManagerSharedComponentInfo, name));
		static_assert(sizeof(DeserializeEntityManagerComponentInfo) == sizeof(DeserializeEntityManagerSharedComponentInfo));

		auto search_matching_component = [](
			Component component,
			unsigned int component_count,
			const auto* pairs,
			const auto* component_table,
			void* name_characters,
			CapacityStream<unsigned int> name_offsets
			) {
				const DeserializeEntityManagerComponentInfo* component_info = nullptr;

				// Search the version for this functor and the name
				unsigned int component_pair_index = 0;
				for (; component_pair_index < component_count; component_pair_index++) {
					if (pairs[component_pair_index].component == component) {
						break;
					}
				}
				ECS_ASSERT(component_pair_index < component_count);

				unsigned char serialize_version = pairs[component_pair_index].serialize_version;
				Component found_at = { -1 };

				auto iterate_table = [&](unsigned int name_offset, unsigned char name_size) {
					ResourceIdentifier name(function::OffsetPointer(name_characters, name_offset), name_size);
					component_table->ForEachConst<true>([&](const auto& info, Component current_component) {
						if (info.name.size > 0) {
							ResourceIdentifier identifier(info.name.buffer, info.name.size);
							if (identifier.Compare(name)) {
								component_info = (const DeserializeEntityManagerComponentInfo*)&info;
								found_at = current_component;
								return true;
							}
						}
						return false;
						});
				};

				if (pairs[component_pair_index].name_size > 0) {
					const void* untyped_component_info;
					if (component_table->TryGetValuePtrUntyped(component, untyped_component_info)) {
						component_info = (decltype(component_info))untyped_component_info;
						if (component_info->name.size == 0) {
							// Iterate through the table
							component_info = nullptr;
							iterate_table(name_offsets[component_pair_index], pairs[component_pair_index].name_size);
						}
						else {
							if (!function::CompareStrings(component_info->name,
								Stream<char>(
									function::OffsetPointer(name_characters, name_offsets[component_pair_index]),
									pairs[component_pair_index].name_size
									)
							)) {
								component_info = nullptr;
								iterate_table(name_offsets[component_pair_index], pairs[component_pair_index].name_size);
							}
							else {
								found_at = component;
							}
						}
					}
					else {
						iterate_table(name_offsets[component_pair_index], pairs[component_pair_index].name_size);
					}
				}
				else {
					component_info = (decltype(component_info))component_table->GetValuePtr(component);
					if (component_info != nullptr) {
						found_at = component;
					}
				}

				return CachedComponentInfo{ component_info, found_at, serialize_version };
		};

		auto search_matching_component_unique = [&](Component component) {
			return search_matching_component(component, header.component_count, component_pairs, component_table, unique_name_characters, component_name_offsets);
		};

		auto search_matching_shared_component = [&](Component component) {
			return search_matching_component(component, header.shared_component_count, shared_component_pairs, shared_component_table, shared_name_characters, shared_component_name_offsets);
		};

		CachedComponentInfo* cached_component_infos = (CachedComponentInfo*)ECS_STACK_ALLOC(sizeof(CachedComponentInfo) * header.component_count);
		CachedSharedComponentInfo* cached_shared_infos = (CachedSharedComponentInfo*)ECS_STACK_ALLOC(sizeof(CachedSharedComponentInfo) * header.shared_component_count);

		// Set cached infos pointers to nullptr to indicate that they have not yet been determined
		memset(cached_component_infos, 0, sizeof(const DeserializeEntityManagerComponentInfo*) * header.component_count);
		memset(cached_shared_infos, 0, sizeof(const DeserializeEntityManagerSharedComponentInfo*) * header.shared_component_count);

		// Initialize the components and the shared components from the headers
		for (unsigned int index = 0; index < header.component_count; index++) {
			if (component_pairs[index].header_data_size > 0) {
				cached_component_infos[index] = search_matching_component_unique(component_pairs[index].component);
				if (cached_component_infos[index].info != nullptr) {
					if (cached_component_infos[index].info->header_function != nullptr) {
						DeserializeEntityManagerHeaderComponentData header_data;
						header_data.extra_data = cached_component_infos[index].info->extra_data;
						header_data.file_data = header_component_data;
						header_data.size = component_pairs[index].header_data_size;
						header_data.version = cached_component_infos[index].version;

						bool is_valid = cached_component_infos[index].info->header_function(&header_data);
						if (!is_valid) {
							return ECS_DESERIALIZE_ENTITY_MANAGER_HEADER_IS_INVALID;
						}
					}
				}
				header_component_data = function::OffsetPointer(header_component_data, component_pairs[index].header_data_size);
				// If the component is not found, then the initialization is skipped
			}
		}
		ECS_ASSERT(header_component_data <= header_shared_component_data);

		for (unsigned int index = 0; index < header.shared_component_count; index++) {
			if (shared_component_pairs[index].header_data_size > 0) {
				cached_shared_infos[index] = search_matching_shared_component(shared_component_pairs[index].component);
				if (cached_shared_infos[index].info != nullptr) {
					if (cached_shared_infos[index].info->header_function != nullptr) {
						DeserializeEntityManagerHeaderSharedComponentData header_data;
						header_data.extra_data = cached_shared_infos[index].info->extra_data;
						header_data.file_data = header_shared_component_data;
						header_data.data_size = shared_component_pairs[index].header_data_size;
						header_data.version = shared_component_pairs[index].serialize_version;

						bool is_valid = cached_shared_infos[index].info->header_function(&header_data);
						if (!is_valid) {
							return ECS_DESERIALIZE_ENTITY_MANAGER_HEADER_IS_INVALID;
						}
					}
				}
				header_shared_component_data = function::OffsetPointer(header_shared_component_data, shared_component_pairs[index].header_data_size);
				// If the component is not found, then the initialization is skipped
			}
		}

		// Now read the identifier buffer data from the named shared instances
		unsigned int named_shared_instances_identifier_total_size = 0;
		for (unsigned int index = 0; index < named_shared_instances_count; index++) {
			named_shared_instances_identifier_total_size += named_headers[index].identifier_size;
		}

		void* named_shared_instances_identifiers = functor.AllocateAndRead(named_shared_instances_identifier_total_size, &allocation_failure);
		if (named_shared_instances_identifiers == nullptr) {
			return allocate_and_read_failure();
		}

		// Now the components and the shared components must be created such that when creating the shared components
		// the instances can be added rightaway

		// Don't use the component byte size in the component pairs
		// That is the byte size for the serialized data, not the one for the new type
		for (unsigned int index = 0; index < header.component_count; index++) {
			if (cached_component_infos[index].info == nullptr) {
				cached_component_infos[index] = search_matching_component_unique(component_pairs[index].component);
			}

			// Modified the behaviour. If a component is not found but there is no instance of it,
			// Allow the deserialization to continue since we can skip the header

			//if (cached_component_infos[index].info == nullptr) {
			//	// Fail, there is a component missing
			//	return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_IS_MISSING;
			//}

			if (cached_component_infos[index].info != nullptr) {
				const auto* component_fixup = &cached_component_infos[index].info->component_fixup;

				// The byte size cannot be 0
				if (component_fixup->component_byte_size == 0) {
					return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_FIXUP_IS_MISSING;
				}

				entity_manager->RegisterComponentCommit(
					component_pairs[index].component,
					component_fixup->component_byte_size,
					component_fixup->allocator_size,
					cached_component_infos[index].info->name,
					{ component_fixup->component_buffers, component_fixup->component_buffer_count }
				);
			}
		}
		for (unsigned int index = 0; index < header.shared_component_count; index++) {
			if (cached_shared_infos[index].info == nullptr) {
				cached_shared_infos[index] = search_matching_shared_component(shared_component_pairs[index].component);
			}

			// Modified the behaviour. If a component is not found but there is no instance of it,
			// Allow the deserialization to continue since we can skip the header
			if (cached_shared_infos[index].info == nullptr && (shared_component_pairs[index].instance_count > 0 ||
				shared_component_pairs[index].named_instance_count > 0)) {
				return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_IS_MISSING;
			}

			const auto* component_fixup = &cached_shared_infos[index].info->component_fixup;

			// If even after this call the component byte size is still 0, then fail
			if (component_fixup->component_byte_size == 0) {
				return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_FIXUP_IS_MISSING;
			}

			entity_manager->RegisterSharedComponentCommit(
				shared_component_pairs[index].component,
				component_fixup->component_byte_size,
				component_fixup->allocator_size,
				cached_shared_infos[index].info->name,
				{ component_fixup->component_buffers, component_fixup->component_buffer_count }
			);
		}

		// Now the shared component data
		for (unsigned int index = 0; index < header.shared_component_count; index++) {
			if (entity_manager->ExistsSharedComponent(shared_component_pairs[index].component)) {
				// If it has a name, search for its match
				const DeserializeEntityManagerSharedComponentInfo* component_info = cached_shared_infos[index].info;

				functor.MarkBufferingResetPoint();

				// Read the header of sizes
				unsigned int* instances_sizes = (unsigned int*)functor.AllocateAndRead(sizeof(unsigned int) * shared_component_pairs[index].instance_count, &allocation_failure);
				if (instances_sizes == nullptr) {
					return allocate_and_read_failure();
				}

				// Determine the size of the all instances such that they can be read in bulk
				unsigned int total_instance_size = 0;
				for (unsigned int subindex = 0; subindex < shared_component_pairs[index].instance_count; subindex++) {
					total_instance_size += instances_sizes[subindex];
				}

				// Read in bulk now the instance data
				void* instances_data = functor.AllocateAndRead(total_instance_size, &allocation_failure);
				if (instances_data == nullptr) {
					return allocate_and_read_failure();
				}

				AllocatorPolymorphic component_allocator = entity_manager->GetSharedComponentAllocatorPolymorphic(shared_component_pairs[index].component);
				ECS_STACK_CAPACITY_STREAM(size_t, component_storage, 1024);

				unsigned int current_instance_offset = 0;
				for (unsigned int shared_instance = 0; shared_instance < shared_component_pairs[index].instance_count; shared_instance++) {
					DeserializeEntityManagerSharedComponentData function_data;
					function_data.data_size = instances_sizes[shared_instance];
					function_data.component = component_storage.buffer;
					function_data.file_data = function::OffsetPointer(instances_data, current_instance_offset);
					function_data.extra_data = component_info->extra_data;
					function_data.instance = { (short)shared_instance };
					function_data.version = shared_component_pairs[index].serialize_version;
					function_data.component_allocator = component_allocator;

					// Now call the extract function
					bool is_data_valid = component_info->function(&function_data);
					if (!is_data_valid) {
						return ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID;
					}

					// Commit the shared instance
					entity_manager->RegisterSharedInstanceCommit(shared_component_pairs[index].component, component_storage.buffer);
					current_instance_offset += instances_sizes[shared_instance];
				}

				// The size doesn't need to be updated - the data is commited right away - this will ensure that the 
				// data is always hot in cache
				functor.ResetBuffering();
			}
		}

		// After the instances have been created, we can now bind the named tags to them
		named_shared_instances_identifier_total_size = 0;
		named_shared_instances_count = 0;
		for (unsigned int index = 0; index < header.shared_component_count; index++) {
			for (unsigned int instance_index = 0; instance_index < shared_component_pairs[index].named_instance_count; instance_index++) {
				entity_manager->RegisterNamedSharedInstanceCommit(
					shared_component_pairs[index].component,
					{ function::OffsetPointer(named_shared_instances_identifiers, named_shared_instances_identifier_total_size), named_headers[named_shared_instances_count].identifier_size },
					named_headers[named_shared_instances_count].instance
				);

				named_shared_instances_identifier_total_size += named_headers[named_shared_instances_count].identifier_size;
				named_shared_instances_count++;
			}
		}

		// The buffering can be freed now - all the component and shared component data was read and commited into the entity manager
		functor.ClearBuffering();

		ArchetypeHeader* archetype_headers = (ArchetypeHeader*)functor.AllocateAndRead(sizeof(ArchetypeHeader) * header.archetype_count, &allocation_failure);
		if (archetype_headers == nullptr) {
			return allocate_and_read_failure();
		}

		// Now calculate how much is needed to read the shared instances and the base archetype sizes
		unsigned int base_archetypes_counts_size = 0;
		unsigned int base_archetypes_instance_size = 0;
		unsigned int archetypes_component_signature_size = 0;
		for (unsigned int index = 0; index < header.archetype_count; index++) {
			// Archetype shared and unique components
			archetypes_component_signature_size += sizeof(Component) * (archetype_headers[index].shared_count + archetype_headers[index].unique_count);

			// The base archetype shared instances
			base_archetypes_instance_size += sizeof(SharedInstance) * archetype_headers[index].shared_count * archetype_headers[index].base_count;
			// The base archetype sizes
			base_archetypes_counts_size += sizeof(unsigned int) * archetype_headers[index].base_count;
		}

		Component* archetype_component_signatures = (Component*)functor.AllocateAndRead(archetypes_component_signature_size 
			+ base_archetypes_instance_size + base_archetypes_counts_size, &allocation_failure);
		if (archetype_component_signatures == nullptr) {
			return allocate_and_read_failure();
		}
		SharedInstance* base_archetypes_instances = (SharedInstance*)function::OffsetPointer(archetype_component_signatures, archetypes_component_signature_size);
		unsigned int* base_archetypes_sizes = (unsigned int*)function::OffsetPointer(base_archetypes_instances, base_archetypes_instance_size);	

		unsigned int component_signature_offset = 0;
		unsigned int base_archetype_instances_offset = 0;
		unsigned int base_archetype_sizes_offset = 0;
		for (unsigned int index = 0; index < header.archetype_count; index++) {
			// Create the archetypes
			ComponentSignature unique;
			unique.indices = archetype_component_signatures + component_signature_offset;
			unique.count = archetype_headers[index].unique_count;

			bool unique_component_is_missing = false;
			// Check to see if one of the unique components is missing
			for (unsigned char component_index = 0; component_index < unique.count && !unique_component_is_missing; component_index++) {
				if (!entity_manager->ExistsComponent(unique.indices[component_index])) {
					unique_component_is_missing = true;
				}
			}

			if (unique_component_is_missing) {
				// Fail
				return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_IS_MISSING;
			}

			ComponentSignature shared;
			shared.indices = archetype_component_signatures + component_signature_offset + archetype_headers[index].unique_count;
			shared.count = archetype_headers[index].shared_count;
			unsigned int archetype_index = entity_manager->CreateArchetypeCommit(unique, shared);
			SharedComponentSignature shared_signature;
			shared_signature.indices = shared.indices;
			shared_signature.count = shared.count;
			for (unsigned int base_index = 0; base_index < archetype_headers[index].base_count; base_index++) {
				shared_signature.instances = base_archetypes_instances + base_archetype_instances_offset;
				// Create the base archetypes
				entity_manager->CreateArchetypeBaseCommit(archetype_index, shared_signature, base_archetypes_sizes[base_archetype_sizes_offset]);

				ArchetypeBase* base = entity_manager->GetBase(archetype_index, base_index);
				// Set the size of the base now - it's going to be used again when deserializing
				base->m_size = base_archetypes_sizes[base_archetype_sizes_offset];

				// We also need to update the shared instance mapping

				// Bump forward the shared indices
				base_archetype_instances_offset += shared.count;
				base_archetype_sizes_offset++;
			}
			// Bump forward the component offset
			component_signature_offset += unique.count + shared.count;
		}

		// Before bumping back the buffering size we need to copy the unique names because they will be used in
		// the search matching component function again, can't rely on the cached versions because it is based on the component
		// and the cached infos don't have that.
		// Another thing that we need to keep is the base archetype sizes.
		unsigned int unique_name_total_size = current_unique_name_offset;
		if (functor.HasBuffering()) {
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(char, stack_unique_component_names, unique_name_total_size);
			memcpy(stack_unique_component_names.buffer, unique_name_characters, unique_name_total_size);
			unique_name_characters = stack_unique_component_names.buffer;
		}

		// The buffering can be bumped back, it is no longer needed
		functor.ClearBuffering();

		// Only the component data of the base archetypes remains to be restored, the entities must be deduces from the entity pool
		for (unsigned int index = 0; index < header.archetype_count; index++) {
			Archetype* archetype = entity_manager->GetArchetype(index);
			unsigned int base_count = archetype->GetBaseCount();
			ComponentSignature unique = archetype->GetUniqueSignature();

			// The order of the loops cannot be interchanged - the serialized data is written for each base together
			for (unsigned int base_index = 0; base_index < base_count; base_index++) {
				ArchetypeBase* base = archetype->GetBase(base_index);
				for (unsigned int component_index = 0; component_index < unique.count; component_index++) {
					auto component_info = search_matching_component_unique(unique.indices[component_index]);

					// Use the buffering to read in. Read the write count first
					unsigned int* data_size = (unsigned int*)functor.AllocateAndRead(sizeof(unsigned int), &allocation_failure);
					if (data_size == nullptr) {
						return allocate_and_read_failure();
					}
					const void* starting_component_data = functor.AllocateAndRead(*data_size, &allocation_failure);
					if (starting_component_data == nullptr) {
						return allocate_and_read_failure();
					}

					// Now call the extract function for each component
					void* archetype_buffer = base->m_buffers[component_index];
					unsigned int component_size = entity_manager->m_unique_components[unique.indices[component_index].value].size;

					DeserializeEntityManagerComponentData function_data;
					function_data.count = base->m_size;
					function_data.extra_data = component_info.info->extra_data;
					function_data.file_data = starting_component_data;
					function_data.components = archetype_buffer;
					function_data.version = component_info.version;
					function_data.component_allocator = entity_manager->GetComponentAllocatorPolymorphic(unique.indices[component_index]);

					bool is_data_valid = component_info.info->function(&function_data);
					if (!is_data_valid) {
						return ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID;
					}
				}
			}
		}

		functor.ClearBuffering();

		// We need to record the signatures because when updating components we need to match the old signatures
		// We can't place the archetype signatures on the stack because of the alignment bug of the stack trace
		// Use the entity manager allocator for this
		size_t vector_signature_size = sizeof(VectorComponentSignature) * entity_manager->m_archetypes.size * 2;
		VectorComponentSignature* old_archetype_signatures = (VectorComponentSignature*)entity_manager->m_memory_manager->Allocate(vector_signature_size);
		memcpy(old_archetype_signatures, entity_manager->m_archetype_vector_signatures, vector_signature_size);

#pragma region Remap components

		bool* component_exists = (bool*)entity_manager->m_memory_manager->Allocate(sizeof(bool) * entity_manager->m_unique_components.size);
		MemoryArena** component_arena_pointers = (MemoryArena**)entity_manager->m_memory_manager->Allocate(sizeof(MemoryArena*) * entity_manager->m_unique_components.size);

		/*
			AWESOME BUG. So having any instance of a VectorComponentSignature on the "stack" (basically in the registers)
			will result in a failure when using longjmp to exit out of the function. Why that is I have no clue. There is
			no where an explanation to this. A curious observation that I made is that when optimizations are disabled the
			compiler writes the registers into addresses on the stack to be easily referenced by the debugger. In some cases
			turning on optimizations will result in eliding these writes and longjmp proceeds successfully. So it might be related
			to stack spilling or stack writing of the register values.
		*/

		// Now determine if a component has changed the ID and broadcast that change to the archetypes as well
		// We need to store this component count since new components might be remapped and change the size
		size_t unique_component_count = entity_manager->m_unique_components.size;
		for (size_t index = 0; index < unique_component_count; index++) {
			Component component = { (short)index };
			if (entity_manager->ExistsComponent(component)) {
				component_arena_pointers[index] = entity_manager->GetComponentAllocator(component);
				component_exists[index] = true;
			}
			else {
				component_exists[index] = false;
			}
		}

		for (size_t index = 0; index < unique_component_count; index++) {
			if (component_exists[index]) {
				Component current_component = { (short)index };
				auto info = search_matching_component_unique(current_component);
				if (info.found_at != current_component) {
					// Register the component with allocator size of 0
					// Since we are going to redirect the previous allocator
					entity_manager->RegisterComponentCommit(
						info.found_at,
						info.info->component_fixup.component_byte_size,
						0,
						info.info->name,
						{ info.info->component_fixup.component_buffers, info.info->component_fixup.component_buffer_count }
					);
					entity_manager->m_unique_components[info.found_at].allocator = component_arena_pointers[index];

					// Make the previous component unallocated such that when it is unregistered
					// It won't deallocate it
					entity_manager->m_unique_components[index].allocator = nullptr;

					size_t _component_vector[sizeof(VectorComponentSignature) / sizeof(size_t)];
					VectorComponentSignature* component_vector = (VectorComponentSignature*)_component_vector;
					component_vector->ConvertComponents({ &current_component, 1 });
					unsigned char inside_archetype_index;

					// Update the archetypes as well
					for (size_t subindex = 0; subindex < entity_manager->m_archetypes.size; subindex++) {
						GetEntityManagerUniqueVectorSignaturePtr(old_archetype_signatures, subindex)->Find(*component_vector, &inside_archetype_index);
						if (inside_archetype_index != UCHAR_MAX) {
							// Replace the component
							entity_manager->m_archetypes[subindex].m_unique_components.indices[inside_archetype_index].value = info.found_at;
							entity_manager->GetArchetypeUniqueComponentsPtr(subindex)->ConvertComponents(entity_manager->m_archetypes[subindex].m_unique_components);
						}
					}

					entity_manager->UnregisterComponentCommit(current_component);
				}
			}
		}

		// Deallocate the allocations
		entity_manager->m_memory_manager->Deallocate(component_exists);
		entity_manager->m_memory_manager->Deallocate(component_arena_pointers);

#pragma endregion

#pragma region Remap shared instances

		// We need to remap the shared instances before remapping the shared components since it will cause
		// A crash since some components won't match
		unsigned int* remap_instance_offsets = (unsigned int*)entity_manager->m_memory_manager->Allocate(sizeof(unsigned int) * (header.shared_component_count + 1));
		Component* remap_components = (Component*)entity_manager->m_memory_manager->Allocate(sizeof(Component) * header.shared_component_count);
		DeserializeEntityManagerInitializeRemappSharedInstances(shared_component_pairs, remap_components, remap_instance_offsets, header.shared_component_count);
		unsigned int archetype_count = entity_manager->GetArchetypeCount();
		for (unsigned int index = 0; index < archetype_count; index++) {
			DeserializeEntityManagerRemappSharedInstances(
				remap_components,
				registered_shared_instances,
				remap_instance_offsets,
				header.shared_component_count,
				entity_manager->GetArchetype(index)
			);
		}

		entity_manager->m_memory_manager->Deallocate(remap_instance_offsets);
		entity_manager->m_memory_manager->Deallocate(remap_components);

#pragma endregion

#pragma region Remap Shared Components

		bool* shared_component_exists = (bool*)entity_manager->m_memory_manager->Allocate(sizeof(bool) * entity_manager->m_shared_components.size);
		MemoryArena** shared_component_arena_pointers = (MemoryArena**)entity_manager->m_memory_manager->Allocate(sizeof(MemoryArena*) * entity_manager->m_shared_components.size);

		// Now determine if a component has changed the ID and broadcast that change to the archetypes as well
		// We need to store this component count since new components might be remapped and change the size
		size_t shared_component_count = entity_manager->m_shared_components.size;
		for (size_t index = 0; index < shared_component_count; index++) {
			Component component = { (short)index };
			if (entity_manager->ExistsSharedComponent(component)) {
				shared_component_arena_pointers[index] = entity_manager->GetSharedComponentAllocator(component);
				shared_component_exists[index] = true;
			}
			else {
				shared_component_exists[index] = false;
			}
		}

		for (size_t index = 0; index < shared_component_count; index++) {
			if (component_exists[index]) {
				Component current_component = { (short)index };
				auto info = search_matching_shared_component(current_component);

				ECS_ASSERT(info.found_at.value != -1);
				short new_id = info.found_at;
				if (new_id != index) {
					// Register this new component - set the allocator size to 0 such that we can set it later on
					entity_manager->RegisterSharedComponentCommit(
						new_id,
						info.info->component_fixup.component_byte_size,
						0,
						info.info->name,
						{ info.info->component_fixup.component_buffers, info.info->component_fixup.component_buffer_count }
					);

					// Also copy the instances and named instances
					entity_manager->m_shared_components[new_id].info.allocator = shared_component_arena_pointers[index];
					entity_manager->m_shared_components[new_id].instances = entity_manager->m_shared_components[index].instances;
					entity_manager->m_shared_components[new_id].named_instances = entity_manager->m_shared_components[index].named_instances;
					// And reset them on the previous component such that when we call unregister the data already stored
					// Won't be deallocated. The same applies to the allocator
					entity_manager->m_shared_components[index].instances.Initialize(entity_manager->m_shared_components[index].instances.allocator, 0);
					entity_manager->m_shared_components[index].named_instances.Clear();
					entity_manager->m_shared_components[index].info.allocator = nullptr;

					size_t _component_vector[sizeof(VectorComponentSignature) / sizeof(size_t)];
					VectorComponentSignature* component_vector = (VectorComponentSignature*)_component_vector;
					component_vector->ConvertComponents({ &current_component, 1 });
					unsigned char inside_archetype_index;

					// Update the archetypes as well
					for (size_t subindex = 0; subindex < entity_manager->m_archetypes.size; subindex++) {
						GetEntityManagerSharedVectorSignaturePtr(old_archetype_signatures, subindex)->Find(*component_vector, &inside_archetype_index);
						if (inside_archetype_index != UCHAR_MAX) {
							// Replace the component
							entity_manager->m_archetypes[subindex].m_shared_components.indices[inside_archetype_index].value = new_id;
							entity_manager->GetArchetypeSharedComponentsPtr(subindex)->ConvertComponents(entity_manager->m_archetypes[subindex].m_shared_components);
						}
					}

					entity_manager->UnregisterSharedComponentCommit(current_component);
				}
			}
		}

		// Make the deallocations
		entity_manager->m_memory_manager->Deallocate(shared_component_exists);
		entity_manager->m_memory_manager->Deallocate(shared_component_arena_pointers);
		entity_manager->m_memory_manager->Deallocate(old_archetype_signatures);

#pragma endregion

		auto read_instrument = functor.ReadInstrument();

		// The base archetypes are missing their entities, but these will be restored after reading the entity pool,
		// which can be read now
		success = DeserializeEntityPool(entity_manager->m_entity_pool, read_instrument);
		if (!success) {
			return ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID;
		}

		// Walk through the list of entities and write them inside the base archetypes
		for (unsigned int pool_index = 0; pool_index < entity_manager->m_entity_pool->m_entity_infos.size; pool_index++) {
			if (entity_manager->m_entity_pool->m_entity_infos[pool_index].is_in_use) {
				entity_manager->m_entity_pool->m_entity_infos[pool_index].stream.ForEachIndex([&](unsigned int entity_index) {
					// Construct the entity from the entity info
					EntityInfo info = entity_manager->m_entity_pool->m_entity_infos[pool_index].stream[entity_index];
					Entity entity = entity_manager->m_entity_pool->GetEntityFromPosition(pool_index, entity_index);
					// Set the entity generation count the same as in the info
					entity.generation_count = info.generation_count;

					// Now set the according archetype's entity inside the entities buffer
					Archetype* archetype = entity_manager->GetArchetype(info.main_archetype);
					ArchetypeBase* base = archetype->GetBase(info.base_archetype);
					base->m_entities[info.stream_index] = entity;
				});
			}
		}

		entity_manager->m_hierarchy_allocator->Clear();
		entity_manager->m_hierarchy = EntityHierarchy(entity_manager->m_hierarchy_allocator);
		success = DeserializeEntityHierarchy(&entity_manager->m_hierarchy, read_instrument);
		if (!success) {
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
		}

		// Everything should be reconstructed now
		return ECS_DESERIALIZE_ENTITY_MANAGER_OK;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager, 
		ECS_FILE_HANDLE file_handle, 
		const DeserializeEntityManagerComponentTable* component_table, 
		const DeserializeEntityManagerSharedComponentTable* shared_component_table
	)
	{
		void* buffering = malloc(ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
		struct Functor {
			ECS_INLINE bool ReadInto(void* ptr, size_t size) {
				return ReadFile(file_handle, { ptr, size });
			}

			void* AllocateAndRead(size_t size, bool* allocation_has_failed) {
				if (buffering_size + size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY) {
					void* pointer = function::OffsetPointer(buffering, buffering_size);
					buffering_size += size;
					bool success = ReadInto(pointer, size);
					return success ? pointer : nullptr;
				}
				else {
					*allocation_has_failed = true;
					return nullptr;
				}
			}

			ECS_INLINE void FreeBuffering() {
				free(buffering);
			}
			
			ECS_INLINE void MarkBufferingResetPoint() {
				buffering_marker = buffering_size;
			}

			ECS_INLINE void ResetBuffering() {
				buffering_size = buffering_marker;
			}

			ECS_INLINE void ClearBuffering() {
				buffering_size = 0;
			}

			ECS_INLINE bool HasBuffering() const {
				return true;
			}

			ECS_INLINE ECS_FILE_HANDLE ReadInstrument() const {
				return file_handle;
			}

			void* buffering;
			unsigned int buffering_size;
			unsigned int buffering_marker;
			ECS_FILE_HANDLE file_handle;
		};

		return DeserializeEntityManagerImpl(entity_manager, component_table, shared_component_table, Functor{ buffering, 0, 0, file_handle });
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager, 
		uintptr_t& ptr, 
		const DeserializeEntityManagerComponentTable* component_table, 
		const DeserializeEntityManagerSharedComponentTable* shared_component_table
	)
	{
		struct Functor {
			ECS_INLINE bool ReadInto(void* write_pointer, size_t size) {
				Read<true>(ptr, write_pointer, size);
				return true;
			}

			ECS_INLINE void* AllocateAndRead(size_t size, bool* allocation_has_failed) {
				void* pointer = (void*)*ptr;
				*ptr += size;
				return pointer;
			}

			ECS_INLINE void FreeBuffering() {}

			ECS_INLINE void MarkBufferingResetPoint() {}

			ECS_INLINE void ResetBuffering() {}

			ECS_INLINE void ClearBuffering() {}

			ECS_INLINE bool HasBuffering() const {
				return false;
			}

			ECS_INLINE uintptr_t& ReadInstrument() const {
				return *ptr;
			}

			uintptr_t* ptr;
		};

		return DeserializeEntityManagerImpl(entity_manager, component_table, shared_component_table, Functor{ &ptr });
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

#pragma region Standard Functors (non link components)

	// ------------------------------------------------------------------------------------------------------------------------------------------

	struct ReflectionSerializeComponentData {
		const Reflection::ReflectionManager* reflection_manager;
		const Reflection::ReflectionType* type;
		bool is_blittable;
	};

	unsigned int ReflectionSerializeEntityManagerComponent(SerializeEntityManagerComponentData* data) {
		ReflectionSerializeComponentData* functor_data = (ReflectionSerializeComponentData*)data->extra_data;
		
		size_t type_byte_size = Reflection::GetReflectionTypeByteSize(functor_data->type);
		if (functor_data->is_blittable) {
			size_t write_size = type_byte_size * data->count;
			if (data->buffer_capacity < write_size) {
				return write_size;
			}
			memcpy(data->buffer, data->components, write_size);
			return write_size;
		}
		else {
			SerializeOptions options;
			options.write_type_table = false;

			// Do a prescan to determine the total write size
			const void* current_component = data->components;
			size_t total_write_size = 0;
			for (unsigned int index = 0; index < data->count; index++) {
				size_t serialize_size = SerializeSize(functor_data->reflection_manager, functor_data->type, current_component, &options);
				if (serialize_size == -1) {
					return -1;
				}

				current_component = function::OffsetPointer(current_component, type_byte_size);
				total_write_size += serialize_size;
			}

			if (total_write_size <= data->buffer_capacity) {
				current_component = data->components;
				uintptr_t ptr = (uintptr_t)data->buffer;
				for (unsigned int index = 0; index < data->count; index++) {
					ECS_SERIALIZE_CODE code = Serialize(functor_data->reflection_manager, functor_data->type, current_component, ptr, &options);
					if (code != ECS_SERIALIZE_OK) {
						return -1;
					}

					current_component = function::OffsetPointer(current_component, type_byte_size);
				}
			}

			return total_write_size;
		}
	}

	unsigned int ReflectionSerializeEntityManagerHeaderComponent(SerializeEntityManagerHeaderComponentData* data) {
		ReflectionSerializeComponentData* functor_data = (ReflectionSerializeComponentData*)data->extra_data;
		// Write the type table
		uintptr_t ptr = (uintptr_t)data->buffer;
		functor_data->is_blittable = IsBlittable(functor_data->type);
		SerializeFieldTable(functor_data->reflection_manager, functor_data->type, ptr);

		return ptr - (uintptr_t)data->buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	// Type punned with the unique data
	struct ReflectionSerializeSharedComponentData {
		const Reflection::ReflectionManager* reflection_manager;
		const Reflection::ReflectionType* type;
		bool is_trivially_copyable;
	};

	static_assert(sizeof(ReflectionSerializeSharedComponentData) == sizeof(ReflectionSerializeComponentData));

	unsigned int ReflectionSerializeEntityManagerSharedComponent(SerializeEntityManagerSharedComponentData* data) {
		// The same as the unique version, just the count is 1. The extra data is type punned to the unique data
		SerializeEntityManagerComponentData unique_data;
		unique_data.buffer = data->buffer;
		unique_data.buffer_capacity = data->buffer_capacity;
		unique_data.components = data->component_data;
		unique_data.count = 1;
		unique_data.extra_data = data->extra_data;
		return ReflectionSerializeEntityManagerComponent(&unique_data);
	}

	unsigned int ReflectionSerializeEntityManagerHeaderSharedComponent(SerializeEntityManagerHeaderSharedComponentData* data) {
		// At the moment can call the unique componet variant
		SerializeEntityManagerHeaderComponentData unique_data;
		unique_data.buffer = data->buffer;
		unique_data.buffer_capacity = data->buffer_capacity;
		unique_data.extra_data = data->extra_data;

		// Could have type punned the data with the unique data, but this is "safer"
		return ReflectionSerializeEntityManagerHeaderComponent(&unique_data);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	struct ReflectionDeserializeComponentData {
		const Reflection::ReflectionManager* reflection_manager;
		const Reflection::ReflectionType* type;
		DeserializeFieldTable field_table;
		// This allocator is used for the field table
		AllocatorPolymorphic allocator;
		bool is_unchanged_and_blittable;
	};

	bool ReflectionDeserializeEntityManagerComponent(DeserializeEntityManagerComponentData* data) {
		ReflectionDeserializeComponentData* functor_data = (ReflectionDeserializeComponentData*)data->extra_data;

		size_t type_byte_size = Reflection::GetReflectionTypeByteSize(functor_data->type);
		if (functor_data->is_unchanged_and_blittable) {
			memcpy(data->components, data->file_data, data->count * type_byte_size);
			// This line is useful for linked components
			data->file_data = function::OffsetPointer(data->file_data, data->count * type_byte_size);
		}
		else {
			// Must use the deserialize to take care of the data that can be read
			DeserializeOptions options;
			options.read_type_table = false;
			options.field_table = &functor_data->field_table;
			options.verify_dependent_types = false;
			options.field_allocator = data->component_allocator;
			options.backup_allocator = data->component_allocator;
			options.default_initialize_missing_fields = true;

			uintptr_t ptr = (uintptr_t)data->file_data;
			void* current_component = data->components;
			for (unsigned int index = 0; index < data->count; index++) {
				ECS_DESERIALIZE_CODE code = Deserialize(functor_data->reflection_manager, functor_data->type, current_component, ptr, &options);
				if (code != ECS_DESERIALIZE_OK) {
					return false;
				}

				current_component = function::OffsetPointer(current_component, type_byte_size);
			}
			// This line is useful for linked components
			data->file_data = (void*)ptr;
		}

		return true;
	}

	bool ReflectionDeserializeEntityManagerHeaderComponent(DeserializeEntityManagerHeaderComponentData* data) {
		ReflectionDeserializeComponentData* functor_data = (ReflectionDeserializeComponentData*)data->extra_data;

		// Extract the field table
		uintptr_t ptr = (uintptr_t)data->file_data;
		functor_data->field_table = DeserializeFieldTableFromData(ptr, functor_data->allocator);
		if (functor_data->field_table.types.size == 0) {
			return false;
		}

		unsigned int type_index = functor_data->field_table.TypeIndex(functor_data->type->name);
		// If cannot find this type, then there might be a link type mismatch
		if (type_index == -1) {
			// Check if strict deserialization is enabled
			if (functor_data->type->GetEvaluation(ECS_COMPONENT_STRICT_DESERIALIZE) != DBL_MAX) {
				// Abort
				return false;
			}

			// Perform the mirroring
			MirrorDeserializeEntityManagerLinkTypes(functor_data->reflection_manager, &functor_data->field_table);

			// Now retest the name
			type_index = functor_data->field_table.TypeIndex(functor_data->type->name);
		}

		if (type_index == -1) {
			return false;
		}

		bool is_unchanged = functor_data->field_table.IsUnchanged(type_index, functor_data->reflection_manager, functor_data->type);
		if (is_unchanged) {
			functor_data->is_unchanged_and_blittable = IsBlittable(functor_data->type);
		}
		else {
			functor_data->is_unchanged_and_blittable = false;
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	typedef ReflectionDeserializeComponentData ReflectionDeserializeSharedComponentData;

	bool ReflectionDeserializeEntityManagerSharedComponent(DeserializeEntityManagerSharedComponentData* data) {
		// Same as the unique component, just the count is 1
		DeserializeEntityManagerComponentData unique_data;
		unique_data.components = data->component;
		unique_data.count = 1;
		unique_data.extra_data = data->extra_data;
		unique_data.file_data = data->file_data;
		unique_data.version = data->version;
		unique_data.component_allocator = data->component_allocator;
		return ReflectionDeserializeEntityManagerComponent(&unique_data);
	}

	bool ReflectionDeserializeEntityManagerHeaderSharedComponent(DeserializeEntityManagerHeaderSharedComponentData* data) {
		// Same as the unique header. Could type pun the data ptr to a unique ptr, but lets be safe and type pun only the 
		// ReflectionDeserializeSharedComponentData into a ReflectionDeserializeComponentData
		DeserializeEntityManagerHeaderComponentData unique_data;
		unique_data.extra_data = data->extra_data;
		unique_data.file_data = data->file_data;
		unique_data.size = data->data_size;
		unique_data.version = data->version;
		return ReflectionDeserializeEntityManagerHeaderComponent(&unique_data);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Link Functors (for link functions)

	// ------------------------------------------------------------------------------------------------------------------------------------------

	struct ReflectionSerializeLinkComponentBase {
		const AssetDatabase* asset_database;
		ModuleLinkComponentReverseFunction reverse_function;
		ModuleLinkComponentFunction function;
		const Reflection::ReflectionType* target_type;
		Stream<LinkComponentAssetField> asset_fields;
		unsigned int written_count_before;
		unsigned int skipped_count_before;
	};

	struct ReflectionSerializeLinkComponentData {
		ReflectionSerializeComponentData normal_base_data;
		ReflectionSerializeLinkComponentBase link_base_data;
	};

	unsigned int ReflectionSerializeEntityManagerLinkComponent(SerializeEntityManagerComponentData* data) {
		ReflectionSerializeLinkComponentData* functor_data = (ReflectionSerializeLinkComponentData*)data->extra_data;
		size_t link_component_storage[ECS_KB];

		unsigned int target_byte_size = Reflection::GetReflectionTypeByteSize(functor_data->link_base_data.target_type);

		unsigned int initial_count = data->count;
		data->count = 1;

		// The serialize function is not dependent on the buffer capacity, so we can decrement it accordingly
		unsigned int total_write_size = 0;
		unsigned int starting_index = functor_data->link_base_data.skipped_count_before > 0 ? functor_data->link_base_data.written_count_before : 0;

		functor_data->link_base_data.skipped_count_before = 0;
		functor_data->link_base_data.written_count_before = 0;

		ConvertToAndFromLinkBaseData convert_base_data;
		convert_base_data.link_type = functor_data->normal_base_data.type;
		convert_base_data.target_type = functor_data->link_base_data.target_type;
		convert_base_data.asset_fields = functor_data->link_base_data.asset_fields;
		convert_base_data.asset_database = functor_data->link_base_data.asset_database;
		convert_base_data.module_link = { nullptr, functor_data->link_base_data.reverse_function };
		convert_base_data.reflection_manager = functor_data->normal_base_data.reflection_manager;

		const void* initial_components = data->components;

		for (unsigned int index = starting_index; index < initial_count; index++) {
			const void* source = function::OffsetPointer(initial_components, index * target_byte_size);

			bool success = ConvertFromTargetToLinkComponent(
				&convert_base_data,
				source,
				link_component_storage
			);
			if (!success) {
				return -1;
			}

			data->extra_data = &functor_data->normal_base_data;
			data->components = source;
			unsigned int current_write_size = ReflectionSerializeEntityManagerComponent(data);
			if (current_write_size <= data->buffer_capacity) {
				data->buffer_capacity -= current_write_size;
				data->buffer = function::OffsetPointer(data->buffer, current_write_size);
				functor_data->link_base_data.skipped_count_before++;
			}
			else {
				functor_data->link_base_data.written_count_before++;
			}
			total_write_size += current_write_size;
		}

		return total_write_size;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	unsigned int ReflectionSerializeEntityManagerHeaderLinkComponent(SerializeEntityManagerHeaderComponentData* data) {
		// Just forward to the normal call
		ReflectionSerializeLinkComponentData* functor_data = (ReflectionSerializeLinkComponentData*)data->extra_data;
		data->extra_data = &functor_data->normal_base_data;
		return ReflectionSerializeEntityManagerHeaderComponent(data);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	struct ReflectionSerializeLinkSharedComponentData {
		ReflectionSerializeSharedComponentData normal_base_data;
		ReflectionSerializeLinkComponentBase link_base_data;
	};

	unsigned int ReflectionSerializeEntityManagerLinkSharedComponent(SerializeEntityManagerSharedComponentData* data) {
		ReflectionSerializeLinkComponentData* functor_data = (ReflectionSerializeLinkComponentData*)data->extra_data;
		size_t link_component_storage[ECS_KB];

		ConvertToAndFromLinkBaseData convert_base_data;
		convert_base_data.link_type = functor_data->normal_base_data.type;
		convert_base_data.target_type = functor_data->link_base_data.target_type;
		convert_base_data.asset_fields = functor_data->link_base_data.asset_fields;
		convert_base_data.asset_database = functor_data->link_base_data.asset_database;
		convert_base_data.module_link = { nullptr, functor_data->link_base_data.reverse_function };
		convert_base_data.reflection_manager = functor_data->normal_base_data.reflection_manager;

		bool success = ConvertFromTargetToLinkComponent(
			&convert_base_data,
			data->component_data,
			link_component_storage
		);
		if (!success) {
			return -1;
		}

		data->extra_data = &functor_data->normal_base_data;
		data->component_data = link_component_storage;
		return ReflectionSerializeEntityManagerSharedComponent(data);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	unsigned int ReflectionSerializeEntityManagerHeaderLinkSharedComponent(SerializeEntityManagerHeaderSharedComponentData* data) {
		// Just forward to the normal call
		ReflectionSerializeLinkSharedComponentData* functor_data = (ReflectionSerializeLinkSharedComponentData*)data->extra_data;
		data->extra_data = &functor_data->normal_base_data;
		return ReflectionSerializeEntityManagerHeaderSharedComponent(data);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	struct ReflectionDeserializeLinkComponentData {
		ReflectionDeserializeComponentData normal_base_data;
		ReflectionSerializeLinkComponentBase link_base_data;
	};

	bool ReflectionDeserializeEntityManagerLinkComponent(DeserializeEntityManagerComponentData* data) {
		ReflectionDeserializeLinkComponentData* functor_data = (ReflectionDeserializeLinkComponentData*)data->extra_data;

		size_t link_component_storage[ECS_KB];

		// Deserialize the link component first
		data->extra_data = &functor_data->normal_base_data;
		unsigned int initial_count = data->count;

		// The allocator doesn't need to be specified since the basic deserialization will use the
		// allocator for any buffers needed
		ConvertToAndFromLinkBaseData convert_base_data;
		convert_base_data.link_type = functor_data->normal_base_data.type;
		convert_base_data.target_type = functor_data->link_base_data.target_type;
		convert_base_data.asset_fields = functor_data->link_base_data.asset_fields;
		convert_base_data.asset_database = functor_data->link_base_data.asset_database;
		convert_base_data.module_link = { functor_data->link_base_data.function, nullptr };
		convert_base_data.reflection_manager = functor_data->normal_base_data.reflection_manager;
		
		void* initial_component = data->components;
		data->count = 1;
		data->extra_data = &functor_data->normal_base_data;
		data->components = link_component_storage;

		size_t target_byte_size = Reflection::GetReflectionTypeByteSize(functor_data->link_base_data.target_type);

		for (unsigned int index = 0; index < initial_count; index++) {
			bool success = ReflectionDeserializeEntityManagerComponent(data);
			if (!success) {
				return false;
			}

			// Transform from link to actual component
			success = ConvertLinkComponentToTarget(
				&convert_base_data,
				link_component_storage,
				function::OffsetPointer(initial_component, index * target_byte_size)
			);
			if (!success) {
				return false;
			}
		}

		return true;
	}

	bool ReflectionDeserializeEntityManagerHeaderLinkComponent(DeserializeEntityManagerHeaderComponentData* data) {
		ReflectionDeserializeLinkComponentData* functor_data = (ReflectionDeserializeLinkComponentData*)data->extra_data;
		data->extra_data = &functor_data->normal_base_data;
		return ReflectionDeserializeEntityManagerHeaderComponent(data);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	struct ReflectionDeserializeLinkSharedComponentData {
		ReflectionDeserializeSharedComponentData normal_base_data;
		ReflectionSerializeLinkComponentBase link_base_data;
	};

	bool ReflectionDeserializeEntityManagerLinkSharedComponent(DeserializeEntityManagerSharedComponentData* data) {
		ReflectionDeserializeLinkSharedComponentData* functor_data = (ReflectionDeserializeLinkSharedComponentData*)data->extra_data;

		size_t link_component_storage[ECS_KB];

		// Deserialize the link component first
		data->extra_data = &functor_data->normal_base_data;

		// The allocator doesn't need to be specified since the basic deserialization will use the
		// allocator for any buffers needed
		ConvertToAndFromLinkBaseData convert_base_data;
		convert_base_data.link_type = functor_data->normal_base_data.type;
		convert_base_data.target_type = functor_data->link_base_data.target_type;
		convert_base_data.asset_fields = functor_data->link_base_data.asset_fields;
		convert_base_data.asset_database = functor_data->link_base_data.asset_database;
		convert_base_data.module_link = { functor_data->link_base_data.function, nullptr };
		convert_base_data.reflection_manager = functor_data->normal_base_data.reflection_manager;

		void* initial_component = data->component;

		data->extra_data = &functor_data->normal_base_data;
		data->component = link_component_storage;

		bool success = ReflectionDeserializeEntityManagerSharedComponent(data);
		if (!success) {
			return false;
		}

		// Transform from link to actual component
		success = ConvertLinkComponentToTarget(
			&convert_base_data,
			link_component_storage,
			initial_component
		);
		if (!success) {
			return false;
		}
		return true;
	}

	bool ReflectionDeserializeEntityManagerHeaderLinkSharedComponent(DeserializeEntityManagerHeaderSharedComponentData* data) {
		ReflectionDeserializeLinkComponentData* functor_data = (ReflectionDeserializeLinkComponentData*)data->extra_data;
		data->extra_data = &functor_data->normal_base_data;
		return ReflectionDeserializeEntityManagerHeaderSharedComponent(data);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ------------------------------------------------------------------------------------------------------------------------------------------

	template<typename TableType, typename OverrideType, typename Functor>
	void CreateSerializeDeserializeEntityManagerComponentTable(
		TableType& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<OverrideType> overrides,
		Component* override_components,
		Stream<unsigned int> hierarchy_indices,
		Functor&& functor
	) {
		// Calculate the total count
		size_t total_count = table.GetCount() + overrides.size;

		unsigned int type_count = reflection_manager->GetTypeCount();
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, type_indices, type_count);

		Reflection::ReflectionManagerGetQuery query;
		query.indices = &type_indices;
		if (hierarchy_indices.size == 0) {
			reflection_manager->GetHierarchyTypes(query);
		}
		else {
			// Get the indices of all types and then allocate the table
			for (unsigned int index = 0; index < hierarchy_indices.size; index++) {
				reflection_manager->GetHierarchyTypes(query, hierarchy_indices[index]);
			}
		}

		// Allocate a table of this capacity
		total_count += (float)type_indices.size * 100 / ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR;
		total_count = function::PowerOfTwoGreater(total_count);

		if (table.GetCount() == 0) {
			table.Initialize(allocator, total_count);
		}
		else {
			void* new_buffer = Allocate(allocator, table.MemoryOf(total_count));
			const void* old_buffer = table.Grow(new_buffer, total_count);
			Deallocate(allocator, old_buffer);
		}

		// Now can start adding the types
		// Hoist the if check for the override outside the loop
		if (overrides.size > 0) {
			for (unsigned int index = 0; index < type_indices.size; index++) {
				const Reflection::ReflectionType* type = reflection_manager->GetType(type_indices[index]);
				if (IsReflectionTypeComponent(type) || IsReflectionTypeSharedComponent(type)) {
					// Check to see if it is overwritten
					if (override_components != nullptr) {
						// Check the component
						Component type_component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
						if (function::SearchBytes(override_components, overrides.size, type_component.value, sizeof(type_component)) == -1) {
							// There is no override
							functor(type);
						}
					}
					else {
						unsigned int overwrite_index = 0;
						for (; overwrite_index < overrides.size; overwrite_index++) {
							if (function::CompareStrings(overrides[overwrite_index].name, type->name)) {
								break;
							}
						}

						if (overwrite_index == overrides.size) {
							functor(type);
						}
					}
				}
			}
		}
		else {
			// No overrides, can add these directly
			for (unsigned int index = 0; index < type_indices.size; index++) {
				const Reflection::ReflectionType* type = reflection_manager->GetType(type_indices[index]);
				if (IsReflectionTypeComponent(type) || IsReflectionTypeSharedComponent(type)) {
					functor(type);
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void ConvertLinkTypesToSerializeEntityManagerUnique(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator, 
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		SerializeEntityManagerComponentInfo* overrides
	)
	{
		for (size_t index = 0; index < link_types.size; index++) {
			Stream<char> target = GetReflectionTypeLinkComponentTarget(link_types[index]);
			const Reflection::ReflectionType* target_type = reflection_manager->GetType(target);

			SerializeEntityManagerComponentInfo info;
			info.function = ReflectionSerializeEntityManagerLinkComponent;
			info.header_function = ReflectionSerializeEntityManagerHeaderLinkComponent;
			ReflectionSerializeLinkComponentData* data = (ReflectionSerializeLinkComponentData*)Allocate(allocator, sizeof(ReflectionSerializeLinkComponentData));

			ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromLinkComponent(link_types[index], asset_fields);

			data->normal_base_data.reflection_manager = reflection_manager;
			data->normal_base_data.type = link_types[index];
			data->link_base_data.target_type = target_type;
			data->link_base_data.asset_database = database;
			data->link_base_data.reverse_function = module_links[index].reverse_function;
			data->link_base_data.asset_fields.InitializeAndCopy(allocator, asset_fields);
			data->link_base_data.written_count_before = 0;
			data->link_base_data.skipped_count_before = 0;

			info.extra_data = data;
			info.name = target_type->name;
			// The version is not really needed
			info.version = 0;

			overrides[index] = info;
		}
	}


	void CreateSerializeEntityManagerComponentTable(
		SerializeEntityManagerComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<SerializeEntityManagerComponentInfo> overrides,
		Component* override_components,
		Stream<unsigned int> hierarchy_indices
	)
	{
		CreateSerializeDeserializeEntityManagerComponentTable(table, reflection_manager, allocator, overrides, override_components, hierarchy_indices, [&](const Reflection::ReflectionType* type) {
			if (IsReflectionTypeComponent(type)) {
				SerializeEntityManagerComponentInfo info;
				info.function = ReflectionSerializeEntityManagerComponent;
				info.header_function = ReflectionSerializeEntityManagerHeaderComponent;
				ReflectionSerializeComponentData* data = (ReflectionSerializeComponentData*)Allocate(allocator, sizeof(ReflectionSerializeComponentData));
				data->reflection_manager = reflection_manager;
				data->type = type;

				info.extra_data = data;
				info.name = type->name;
				// The version is not really needed
				info.version = 0;

				Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };

				ECS_ASSERT(!table.Insert(info, component));
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	template<typename TableType, typename InfoType>
	void AddEntityManagerComponentTableOverridesImpl(TableType& table, const Reflection::ReflectionManager* reflection_manager, Stream<InfoType> overrides) {
		for (size_t index = 0; index < overrides.size; index++) {
			const Reflection::ReflectionType* type = reflection_manager->GetType(overrides[index].name);
			if (IsReflectionTypeLinkComponent(type)) {
				Stream<char> target = GetReflectionTypeLinkComponentTarget(type);
				if (target.size > 0) {
					type = reflection_manager->GetType(target);
				}
			}

			double evaluation = type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
			ECS_ASSERT(evaluation != DBL_MAX);

			Component component = { (short)evaluation };
			ECS_ASSERT(!table.Insert(overrides[index], component));
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void AddSerializeEntityManagerComponentTableOverrides(
		SerializeEntityManagerComponentTable& table, 
		const Reflection::ReflectionManager* reflection_manager, 
		Stream<SerializeEntityManagerComponentInfo> overrides
	)
	{
		AddEntityManagerComponentTableOverridesImpl(table, reflection_manager, overrides);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void ConvertLinkTypesToSerializeEntityManagerShared(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		AllocatorPolymorphic allocator, 
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		SerializeEntityManagerSharedComponentInfo* overrides
	)
	{
		for (size_t index = 0; index < link_types.size; index++) {
			Stream<char> target = GetReflectionTypeLinkComponentTarget(link_types[index]);
			const Reflection::ReflectionType* target_type = reflection_manager->GetType(target);

			SerializeEntityManagerSharedComponentInfo info;
			info.function = ReflectionSerializeEntityManagerLinkSharedComponent;
			info.header_function = ReflectionSerializeEntityManagerHeaderLinkSharedComponent;

			ReflectionSerializeLinkSharedComponentData* data = (ReflectionSerializeLinkSharedComponentData*)Allocate(allocator, sizeof(ReflectionSerializeLinkSharedComponentData));
			data->normal_base_data.reflection_manager = reflection_manager;
			data->normal_base_data.type = link_types[index];

			ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromLinkComponent(link_types[index], asset_fields);

			data->link_base_data.asset_database = asset_database;
			data->link_base_data.reverse_function = module_links[index].reverse_function;
			data->link_base_data.target_type = target_type;
			data->link_base_data.asset_fields.InitializeAndCopy(allocator, asset_fields);
			data->link_base_data.skipped_count_before = 0;
			data->link_base_data.written_count_before = 0;

			info.extra_data = data;
			info.name = target_type->name;
			// The version is not really needed
			info.version = 0;

			overrides[index] = info;
		}
	}

	void CreateSerializeEntityManagerSharedComponentTable(
		SerializeEntityManagerSharedComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager, 
		AllocatorPolymorphic allocator,
		Stream<SerializeEntityManagerSharedComponentInfo> overrides,
		Component* override_components,
		Stream<unsigned int> hierarchy_indices
	)
	{
		CreateSerializeDeserializeEntityManagerComponentTable(table, reflection_manager, allocator, overrides, override_components, hierarchy_indices, [&](const Reflection::ReflectionType* type) {
			if (IsReflectionTypeSharedComponent(type)) {
				SerializeEntityManagerSharedComponentInfo info;
				info.function = ReflectionSerializeEntityManagerSharedComponent;
				info.header_function = ReflectionSerializeEntityManagerHeaderSharedComponent;
				ReflectionSerializeSharedComponentData* data = (ReflectionSerializeSharedComponentData*)Allocate(allocator, sizeof(ReflectionSerializeSharedComponentData));
				data->reflection_manager = reflection_manager;
				data->type = type;

				info.extra_data = data;
				info.name = type->name;
				// The version is not really needed
				info.version = 0;

				Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };

				ECS_ASSERT(!table.Insert(info, component));
			}
		});
	}

	void AddSerializeEntityManagerSharedComponentTableOverrides(
		SerializeEntityManagerSharedComponentTable& table, 
		const Reflection::ReflectionManager* reflection_manager, 
		Stream<SerializeEntityManagerSharedComponentInfo> overrides
	)
	{
		AddEntityManagerComponentTableOverridesImpl(table, reflection_manager, overrides);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void ConvertLinkTypesToDeserializeEntityManagerUnique(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		DeserializeEntityManagerComponentInfo* overrides
	)
	{
		for (size_t index = 0; index < link_types.size; index++) {
			Stream<char> target = GetReflectionTypeLinkComponentTarget(link_types[index]);
			const Reflection::ReflectionType* target_type = reflection_manager->GetType(target);

			DeserializeEntityManagerComponentInfo info;
			info.function = ReflectionDeserializeEntityManagerLinkComponent;
			info.header_function = ReflectionDeserializeEntityManagerHeaderLinkComponent;
			
			ReflectionDeserializeLinkComponentData* data = (ReflectionDeserializeLinkComponentData*)Allocate(allocator, sizeof(ReflectionDeserializeLinkComponentData));
			data->normal_base_data.reflection_manager = reflection_manager;
			data->normal_base_data.type = link_types[index];
			data->normal_base_data.allocator = allocator;

			ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromLinkComponent(link_types[index], asset_fields);
			
			data->link_base_data.asset_database = asset_database;
			data->link_base_data.asset_fields.InitializeAndCopy(allocator, asset_fields);
			data->link_base_data.function = module_links[index].build_function;
			data->link_base_data.skipped_count_before = 0;
			data->link_base_data.written_count_before = 0;
			data->link_base_data.target_type = target_type;

			info.extra_data = data;
			info.name = target_type->name;
			info.component_fixup.component_byte_size = Reflection::GetReflectionTypeByteSize(target_type);
			double _allocator_size = target_type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
			info.component_fixup.allocator_size = _allocator_size != DBL_MAX ? (size_t)_allocator_size : 0;
			if (info.component_fixup.allocator_size > 0) {
				// Look for the buffers
				CapacityStream<ComponentBuffer> component_buffers = { info.component_fixup.component_buffers, 0, ECS_COMPONENT_INFO_MAX_BUFFER_COUNT };
				GetReflectionTypeRuntimeBuffers(target_type, component_buffers);
				info.component_fixup.component_buffer_count = component_buffers.size;
			}

			overrides[index] = info;
		}
	}

	void CreateDeserializeEntityManagerComponentTable(
		DeserializeEntityManagerComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager, 
		AllocatorPolymorphic allocator,
		Stream<DeserializeEntityManagerComponentInfo> overrides,
		Component* override_components,
		Stream<unsigned int> hierarchy_indices
	)
	{
		CreateSerializeDeserializeEntityManagerComponentTable(table, reflection_manager, allocator, overrides, override_components, hierarchy_indices, [&](const Reflection::ReflectionType* type) {
			if (IsReflectionTypeComponent(type)) {
				DeserializeEntityManagerComponentInfo info;
				info.function = ReflectionDeserializeEntityManagerComponent;
				info.header_function = ReflectionDeserializeEntityManagerHeaderComponent;
				ReflectionDeserializeComponentData* data = (ReflectionDeserializeComponentData*)Allocate(allocator, sizeof(ReflectionDeserializeComponentData));
				data->reflection_manager = reflection_manager;
				data->type = type;
				data->allocator = allocator;

				info.extra_data = data;
				info.name = type->name;
				info.component_fixup.component_byte_size = Reflection::GetReflectionTypeByteSize(type);
				double _allocator_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
				info.component_fixup.allocator_size = _allocator_size != DBL_MAX ? (size_t)_allocator_size : 0;
				if (info.component_fixup.allocator_size > 0) {
					// Look for the buffers
					CapacityStream<ComponentBuffer> component_buffers = { info.component_fixup.component_buffers, 0, ECS_COMPONENT_INFO_MAX_BUFFER_COUNT };
					GetReflectionTypeRuntimeBuffers(type, component_buffers);
					info.component_fixup.component_buffer_count = component_buffers.size;
				}
				Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };

				ECS_ASSERT(!table.Insert(info, component));
			}
		});
	}

	void AddDeserializeEntityManagerComponentTableOverrides(
		DeserializeEntityManagerComponentTable& table, 
		const Reflection::ReflectionManager* reflection_manager,
		Stream<DeserializeEntityManagerComponentInfo> overrides
	)
	{
		AddEntityManagerComponentTableOverridesImpl(table, reflection_manager, overrides);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void ConvertLinkTypesToDeserializeEntityManagerShared(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		DeserializeEntityManagerSharedComponentInfo* overrides
	)
	{
		for (size_t index = 0; index < link_types.size; index++) {
			Stream<char> target = GetReflectionTypeLinkComponentTarget(link_types[index]);
			const Reflection::ReflectionType* target_type = reflection_manager->GetType(target);

			DeserializeEntityManagerSharedComponentInfo info;
			info.function = ReflectionDeserializeEntityManagerLinkSharedComponent;
			info.header_function = ReflectionDeserializeEntityManagerHeaderLinkSharedComponent;

			ReflectionDeserializeLinkSharedComponentData* data = (ReflectionDeserializeLinkSharedComponentData*)Allocate(allocator, sizeof(ReflectionDeserializeLinkSharedComponentData));
			
			data->normal_base_data.reflection_manager = reflection_manager;
			data->normal_base_data.type = link_types[index];
			data->normal_base_data.allocator = allocator;

			ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromLinkComponent(link_types[index], asset_fields);

			data->link_base_data.asset_database = database;
			data->link_base_data.asset_fields.InitializeAndCopy(allocator, asset_fields);
			data->link_base_data.function = module_links[index].build_function;
			data->link_base_data.skipped_count_before = 0;
			data->link_base_data.written_count_before = 0;
			data->link_base_data.target_type = target_type;

			info.extra_data = data;
			info.name = target_type->name;
			info.component_fixup.component_byte_size = Reflection::GetReflectionTypeByteSize(target_type);
			double _allocator_size = target_type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
			info.component_fixup.allocator_size = _allocator_size != DBL_MAX ? (size_t)_allocator_size : 0;
			if (info.component_fixup.allocator_size > 0) {
				// Look for the buffers
				CapacityStream<ComponentBuffer> component_buffers = { info.component_fixup.component_buffers, 0, ECS_COMPONENT_INFO_MAX_BUFFER_COUNT };
				GetReflectionTypeRuntimeBuffers(target_type, component_buffers);
				info.component_fixup.component_buffer_count = component_buffers.size;
			}
			
			overrides[index] = info;
		}
	}

	void CreateDeserializeEntityManagerSharedComponentTable(
		DeserializeEntityManagerSharedComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager, 
		AllocatorPolymorphic allocator,
		Stream<DeserializeEntityManagerSharedComponentInfo> overrides,
		Component* override_components,
		Stream<unsigned int> hierarchy_indices
	)
	{
		CreateSerializeDeserializeEntityManagerComponentTable(table, reflection_manager, allocator, overrides, override_components, hierarchy_indices, [&](const Reflection::ReflectionType* type) {
			if (IsReflectionTypeSharedComponent(type)) {
				DeserializeEntityManagerSharedComponentInfo info;
				info.function = ReflectionDeserializeEntityManagerSharedComponent;
				info.header_function = ReflectionDeserializeEntityManagerHeaderSharedComponent;
				ReflectionDeserializeSharedComponentData* data = (ReflectionDeserializeSharedComponentData*)Allocate(allocator, sizeof(ReflectionDeserializeSharedComponentData));
				data->reflection_manager = reflection_manager;
				data->type = type;
				data->allocator = allocator;

				info.extra_data = data;
				info.name = type->name;
				info.component_fixup.component_byte_size = Reflection::GetReflectionTypeByteSize(type);
				double _allocator_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
				info.component_fixup.allocator_size = _allocator_size != DBL_MAX ? (size_t)_allocator_size : 0;
				if (info.component_fixup.allocator_size > 0) {
					// Look for the buffers
					CapacityStream<ComponentBuffer> component_buffers = { info.component_fixup.component_buffers, 0, ECS_COMPONENT_INFO_MAX_BUFFER_COUNT };
					GetReflectionTypeRuntimeBuffers(type, component_buffers);
					info.component_fixup.component_buffer_count = component_buffers.size;
				}
				Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };

				ECS_ASSERT(!table.Insert(info, component));
			}
		});
	}

	void AddDeserializeEntityManagerSharedComponentTableOverrides(
		DeserializeEntityManagerSharedComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager, 
		Stream<DeserializeEntityManagerSharedComponentInfo> overrides
	)
	{
		AddEntityManagerComponentTableOverridesImpl(table, reflection_manager, overrides);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void MirrorDeserializeEntityManagerLinkTypes(
		const Reflection::ReflectionManager* current_reflection_manager, 
		DeserializeFieldTable* deserialize_field_table
	)
	{
		for (size_t index = 0; index < deserialize_field_table->types.size; index++) {
			// If the type table doesn't exist then proceed with the proxy
			Stream<char> current_name = deserialize_field_table->types[index].name;
			Reflection::ReflectionType current_type;
			if (!current_reflection_manager->TryGetType(current_name, current_type)) {
				// Check to see if the file type was a link type and now it's not
				Stream<char> target_type_name = GetReflectionTypeLinkNameBase(current_name);
				if (target_type_name.size < current_name.size) {
					// It was a link type - check to see if the target exists
					Reflection::ReflectionType target_type;
					if (current_reflection_manager->TryGetType(target_type_name, target_type)) {
						// Convert the name of the file type to this name
						// No need for an allocation since it fits into the already allocated name
						deserialize_field_table->types[index].name = target_type_name;
					}
					else {
						// The type might not exist at all - don't do anything
					}
				}
				else {
					// Check to see if now it has a link and previously it didn't
					ECS_STACK_CAPACITY_STREAM(char, link_type_name_storage, 512);
					Stream<char> link_type_name = GetReflectionTypeLinkComponentName(current_name, link_type_name_storage);
					Reflection::ReflectionType link_type;
					if (current_reflection_manager->TryGetType(link_type_name, link_type)) {
						// Get the target of this link type - if it is the same proceed
						target_type_name = GetReflectionTypeLinkComponentTarget(&link_type);
						if (target_type_name == current_name) {
							// Reference the link type name
							deserialize_field_table->types[index].name = link_type.name;
						}
					}
					else {
						// The type might not exist at all - don't do anything
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------


}