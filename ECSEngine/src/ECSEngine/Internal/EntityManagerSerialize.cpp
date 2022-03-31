#include "ecspch.h"
#include "EntityManager.h"
#include "EntityManagerSerialize.h"
#include "../Utilities/Serialization/SerializationHelpers.h"

#define ENTITY_MANAGER_SERIALIZE_VERSION 1

#define ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY ECS_MB * 500

namespace ECSEngine {

	struct SerializeEntityManagerHeader {
		unsigned int version;
		unsigned int archetype_count;
		unsigned int hierarchy_count;
		unsigned int component_count;
		unsigned int shared_component_count;
	};

	struct ComponentPair {
		Component component;
		ComponentInfo size;
	};

	struct SharedComponentPair {
		Component component;
		ComponentInfo size;
		unsigned short shared_instance_count;
		unsigned short named_shared_instance_count;
	};

	struct ArchetypeHeader {
		unsigned short base_count;
		unsigned char unique_count;
		unsigned char shared_count;
	};

	// ------------------------------------------------------------------------------------------------------------------------------------------

	bool SerializeEntityManager(
		const EntityManager* entity_manager,
		Stream<wchar_t> filename,
		const SerializeEntityManagerComponentTable* component_table,
		void* extra_component_table_data,
		const SerializeEntityManagerSharedComponentTable* shared_component_table,
		void* extra_shared_component_table_data
	)
	{
		ECS_FILE_HANDLE file_handle = -1;
		ECS_FILE_STATUS_FLAGS status = FileCreate(filename, &file_handle, ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE);
		if (status != ECS_FILE_STATUS_OK) {
			return false;
		}

		// Write the header first
		SerializeEntityManagerHeader header;
		header.version = ENTITY_MANAGER_SERIALIZE_VERSION;
		header.archetype_count = entity_manager->m_archetypes.size;
		header.hierarchy_count = 0;
		header.component_count = 0;
		header.shared_component_count = 0;
		
		// Determine the component count
		for (unsigned int index = 0; index < entity_manager->m_unique_components.size; index++) {
			header.component_count += entity_manager->ExistsComponent({ (unsigned short)index });
		}

		for (unsigned int index = 0; index < entity_manager->m_shared_components.size; index++) {
			header.shared_component_count += entity_manager->ExistsSharedComponent({ (unsigned short)index });
		}

		// Determine the hierarchy count
		for (unsigned int index = 0; index < entity_manager->m_hierarchies.size; index++) {
			header.hierarchy_count += entity_manager->ExistsHierarchy(index);
		}

		bool success = WriteFile(file_handle, { &header, sizeof(header) });
		if (!success) {
			return false;
		}

		void* component_buffering = malloc(ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
		unsigned int component_buffering_size = 0;

		auto error_lambda = [=]() {
			free(component_buffering);
			CloseFile(file_handle);
			RemoveFile(filename);
		};

		// Serialize the entity manager archetypes and components first, followed by the entity pool and then by the hierarchies
		// The components and shared components are serialized first
		
		// The unique components are written as a pair { Component, ComponentInfo }
		ComponentPair* component_pairs = (ComponentPair*)component_buffering;
		// Use the header's component count as a write counter
		header.component_count = 0;
		for (unsigned int index = 0; index < entity_manager->m_unique_components.size; index++) {
			if (entity_manager->ExistsComponent({ (unsigned short)index })) {
				component_pairs[header.component_count++] = { (unsigned short)index, entity_manager->m_unique_components[index] };
			}
		}
		success = WriteFile(file_handle, { component_pairs, sizeof(ComponentPair) * header.component_count });
		if (!success) {
			error_lambda();
			return false;
		}

		// Write the shared component pairs and then each shared instance
		SharedComponentPair* shared_component_pairs = (SharedComponentPair*)component_buffering;
		// Use the header's component count as a write counter
		header.shared_component_count = 0;
		for (unsigned int index = 0; index < entity_manager->m_shared_components.size; index++) {
			Component current_component = { (unsigned short)index };
			if (entity_manager->ExistsSharedComponent(current_component)) {
				shared_component_pairs[header.shared_component_count++] = { 
					current_component,
					entity_manager->m_shared_components[index].info,
					(unsigned short)entity_manager->m_shared_components[index].instances.size,
					(unsigned short)entity_manager->m_shared_components[index].named_instances.GetCount()
				};
			}
		}
		component_buffering_size = sizeof(SharedComponentPair) * header.shared_component_count;
		ECS_ASSERT(component_buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
		uintptr_t shared_component_buffering_instances_ptr = (uintptr_t)function::OffsetPointer(component_buffering, component_buffering_size);

		// Now write the named shared instances
		for (unsigned int index = 0; index < entity_manager->m_shared_components.size; index++) {
			Component current_component = { (unsigned short)index };
			if (entity_manager->ExistsSharedComponent(current_component)) {
				// Write the pairs as shared instance, identifier size and at the end the buffers
				// This is done like this in order to avoid on deserialization to read small chunks
				// In this way all the pairs can be read at a time and then the identifiers
				unsigned int named_instances_capacity = entity_manager->m_shared_components[index].named_instances.GetExtendedCapacity();
				const ResourceIdentifier* identifiers = entity_manager->m_shared_components[index].named_instances.GetIdentifiers();

				// The pairs of instance and sizes
				for (unsigned int subindex = 0; subindex < named_instances_capacity; subindex++) {
					if (entity_manager->m_shared_components[index].named_instances.IsItemAt(subindex)) {
						SharedInstance instance = entity_manager->m_shared_components[index].named_instances.GetValueFromIndex(subindex);
						Write(&shared_component_buffering_instances_ptr, &instance, sizeof(instance));
						// It will make it easier to access them as a pair of unsigned shorts when deserializing
						unsigned short short_size = (unsigned short)identifiers[subindex].size;
						Write(&shared_component_buffering_instances_ptr, &short_size, sizeof(short_size));
					}
				}
			}
		}

		// Write the identifiers pointer data now
		for (unsigned int index = 0; index < entity_manager->m_shared_components.size; index++) {
			Component current_component = { (unsigned short)index };
			if (entity_manager->ExistsSharedComponent(current_component)) {
				unsigned int named_instances_capacity = entity_manager->m_shared_components[index].named_instances.GetExtendedCapacity();
				const ResourceIdentifier* identifiers = entity_manager->m_shared_components[index].named_instances.GetIdentifiers();
				// Now the identifiers themselves
				for (unsigned int subindex = 0; subindex < named_instances_capacity; subindex++) {
					if (entity_manager->m_shared_components[index].named_instances.IsItemAt(subindex)) {
						Write(&shared_component_buffering_instances_ptr, identifiers[subindex].ptr, identifiers[subindex].size);
					}
				}
			}
		}
		component_buffering_size = shared_component_buffering_instances_ptr - (uintptr_t)component_buffering;
		ECS_ASSERT(component_buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

		// Now write the instances data - take into account the shared component table
		if (shared_component_table == nullptr) {
			for (unsigned int index = 0; index < entity_manager->m_shared_components.size; index++) {
				Component current_component = { (unsigned short)index };
				if (entity_manager->ExistsSharedComponent(current_component)) {
					unsigned short component_size = entity_manager->m_shared_components[index].info.size;
					for (unsigned int instance_index = 0; instance_index < entity_manager->m_shared_components[index].instances.size; instance_index++) {
						// Write the instances blitted
						Write(&shared_component_buffering_instances_ptr, entity_manager->m_shared_components[index].instances[instance_index], component_size);
					}
				}
			}
			component_buffering_size = shared_component_buffering_instances_ptr - (uintptr_t)component_buffering;
			ECS_ASSERT(component_buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
		}
		else {
			// The instance data sizes for each shared component must be written before the actual data if the extract function is used
			// Because otherwise the data cannot be recovered
			for (unsigned int index = 0; index < entity_manager->m_shared_components.size; index++) {
				Component current_component = { (unsigned short)index };
				if (entity_manager->ExistsSharedComponent(current_component)) {
					SerializeEntityManagerExtractSharedComponent extract_function;
					if (shared_component_table->TryGetValue<false>(current_component, extract_function)) {
						// If a function is provided, write the counts at the start of the buffer such that they
						// can be bulk read on deserialization
						
						// Flush the buffering - the instance sizes must be written
						success = WriteFile(file_handle, { component_buffering, component_buffering_size });
						if (!success) {
							error_lambda();
							return false;
						}

						unsigned int* instance_data_sizes = (unsigned int*)component_buffering;
						component_buffering_size = sizeof(unsigned int) * entity_manager->m_shared_components[index].instances.size;

						for (unsigned int instance = 0; instance < entity_manager->m_shared_components[index].instances.size; instance++) {
							unsigned int write_count = extract_function(
								{ (unsigned short)instance },
								entity_manager->m_shared_components[index].instances[instance],
								function::OffsetPointer(component_buffering, component_buffering_size),
								ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY - component_buffering_size,
								extra_shared_component_table_data
							);
							// Assert that the buffering is enough - otherwise it complicates things a lot
							ECS_ASSERT(write_count + component_buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
							component_buffering_size += write_count;
							// Write the count into the the header of sizes
							instance_data_sizes[index] = write_count;
						}
					}
					else {
						// Same as the other loop with the table missing - blit the data into the buffering
						unsigned short component_size = entity_manager->m_shared_components[index].info.size;
						for (unsigned int instance_index = 0; instance_index < entity_manager->m_shared_components[index].instances.size; instance_index++) {
							// Write the instaces blitted
							Write(&shared_component_buffering_instances_ptr, entity_manager->m_shared_components[index].instances[instance_index], component_size);
						}
						component_buffering_size = shared_component_buffering_instances_ptr - (uintptr_t)component_buffering;
						ECS_ASSERT(component_buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
					}
				}
			}
		}

		// Flush the buffering - if any data is still left
		if (component_buffering_size > 0) {
			success = WriteFile(file_handle, { component_buffering, component_buffering_size });
			if (!success) {
				error_lambda();
				return false;
			}
			component_buffering_size = 0;
		}


		// For each archetype, we must serialize the shared and unique components, and then each base archetype
		// For each base archetype its shared components must be written alongside the buffers (the entities can be omitted 
		// because they can be deduced from the entity pool)

		// Go through the archetypes and serialize a "header" such that reading the data is going to be loaded in chunks
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			ComponentSignature unique = archetype->GetUniqueSignature();
			ComponentSignature shared = archetype->GetSharedSignature();
			unsigned short base_count = archetype->GetBaseCount();

			ArchetypeHeader* archetype_header = (ArchetypeHeader*)function::OffsetPointer(component_buffering, component_buffering_size);

			archetype_header->base_count = base_count;
			archetype_header->shared_count = shared.count;
			archetype_header->unique_count = unique.count;

			component_buffering_size += sizeof(*archetype_header);
		}

		// The unique and shared components can be written now
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			ComponentSignature unique = archetype->GetUniqueSignature();
			ComponentSignature shared = archetype->GetSharedSignature();
			unsigned short base_count = archetype->GetBaseCount();

			uintptr_t temp_ptr = (uintptr_t)function::OffsetPointer(component_buffering, component_buffering_size);

			Write(&temp_ptr, unique.indices, sizeof(Component) * unique.count);
			Write(&temp_ptr, shared.indices, sizeof(Component) * shared.count);

			// Reset the size
			component_buffering_size = temp_ptr - (uintptr_t)component_buffering;
		}

		// The shared instances can be written now
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			ComponentSignature unique = archetype->GetUniqueSignature();
			ComponentSignature shared = archetype->GetSharedSignature();
			unsigned short base_count = archetype->GetBaseCount();

			uintptr_t temp_ptr = (uintptr_t)function::OffsetPointer(component_buffering, component_buffering_size);

			// Write the shared instances for each base archetype in a single stream
			for (size_t base_index = 0; base_index < base_count; base_index++) {
				Write(&temp_ptr, archetype->GetBaseInstances(base_index), sizeof(SharedInstance) * shared.count);
			}

			// Reset the size
			component_buffering_size = temp_ptr - (uintptr_t)component_buffering;
		}

		// The base archetype sizes can be written now
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			unsigned short base_count = archetype->GetBaseCount();

			uintptr_t temp_ptr = (uintptr_t)function::OffsetPointer(component_buffering, component_buffering_size);
			// Write the size of each base archetype
			for (size_t base_index = 0; base_index < base_count; base_index++) {
				const ArchetypeBase* base = archetype->GetBase(base_index);
				Write(&temp_ptr, &base->m_size, sizeof(base->m_size));
			}

			// Reset the size
			component_buffering_size = temp_ptr - (uintptr_t)component_buffering;
		}

		// Now commit to the stream the archetype headers
		success = WriteFile(file_handle, { component_buffering, component_buffering_size });
		if (!success) {
			error_lambda();
			return false;
		}
		component_buffering_size = 0;

		// Now serialize the actual unique components - finally!
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			ComponentSignature unique = archetype->GetUniqueSignature();
			unsigned short base_count = archetype->GetBaseCount();

			// The size of each archetype was previously written
			// Now only the buffers must be flushed
			if (component_table == nullptr) {
				for (size_t base_index = 0; base_index < base_count; base_index++) {
					const ArchetypeBase* base = archetype->GetBase(base_index);
					for (size_t component_index = 0; component_index < unique.count; component_index++) {
						success = WriteFile(file_handle, { base->m_buffers[component_index], base->m_size * base->m_infos[unique.indices[component_index].value].size });
						if (!success) {
							error_lambda();
							return false;
						}
					}
				}
			}
			else {
				for (size_t base_index = 0; base_index < base_count; base_index++) {
					ArchetypeBase* base = (ArchetypeBase*)archetype->GetBase(base_index);
					for (size_t component_index = 0; component_index < unique.count; component_index++) {
						// Get the serializer function - if any
						SerializeEntityManagerExtractComponent extract_function;
						if (component_table->TryGetValue<false>(unique.indices[component_index], extract_function)) {
							// Walk through each entity individually
							// The size of each write must be written before the actual data 
							// such that the sizes can be read in bulk
							
							// Flush the current buffering
							success = WriteFile(file_handle, { component_buffering, component_buffering_size });
							if (!success) {
								error_lambda();
								return false;
							}

							unsigned int* entity_component_size = (unsigned int*)component_buffering;
							component_buffering_size = sizeof(unsigned int) * base->m_size;

							for (unsigned int entity_index = 0; entity_index < base->m_size; entity_index++) {
								unsigned int write_count = extract_function(
									base->GetComponentByIndex(entity_index, component_index),
									function::OffsetPointer(component_buffering, component_buffering_size),
									ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY - component_buffering_size,
									extra_component_table_data
								);
								ECS_ASSERT(write_count + component_buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
								entity_component_size[entity_index] = write_count;
								component_buffering_size += write_count;
							}
						}
						else {
							// Same as above - blit the data directly to the file
							success = WriteFile(file_handle, { base->m_buffers[component_index], base->m_size * base->m_infos[unique.indices[component_index].value].size });
							if (!success) {
								error_lambda();
								return false;
							}
						}
					}
				}
			}
		}

		// If there is still data left in the component buffering - commit it
		if (component_buffering_size > 0) {
			success = WriteFile(file_handle, { component_buffering, component_buffering_size });
			if (!success) {
				error_lambda();
				return false;
			}
			component_buffering_size = 0;
		}

		// Write the entity pool
		success = SerializeEntityPool(entity_manager->m_entity_pool, file_handle);
		if (!success) {
			error_lambda();
			return false;
		}

		// Write the hierarchies
		unsigned short* hierarchy_indices = (unsigned short*)component_buffering;
		unsigned int hierarchy_count = 0;
		for (unsigned short index = 0; index < entity_manager->m_hierarchies.size; index++) {
			if (entity_manager->ExistsHierarchy(index)) {
				hierarchy_indices[hierarchy_count++] = index;
			}
		}
		success = WriteFile(file_handle, { hierarchy_indices, sizeof(unsigned short) * hierarchy_count });
		if (!success) {
			error_lambda();
			return false;
		}

		for (unsigned short index = 0; index < entity_manager->m_hierarchies.size; index++) {
			if (entity_manager->ExistsHierarchy(index)) {
				success = SerializeEntityHierarchy(&entity_manager->m_hierarchies[index].hierarchy, file_handle);
				if (!success) {
					error_lambda();
					return false;
				}
			}
		}

		CloseFile(file_handle);
		free(component_buffering);
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		Stream<wchar_t> filename,
		const DeserializeEntityManagerComponentTable* component_table,
		void* extra_component_table_data,
		const DeserializeEntityManagerSharedComponentTable* shared_component_table,
		void* extra_shared_component_table_data
	)
	{
		ECS_FILE_HANDLE file_handle = -1;
		ECS_FILE_STATUS_FLAGS status = OpenFile(filename, &file_handle, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_BINARY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL);
		if (status != ECS_FILE_STATUS_OK) {
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_OPEN_FILE;
		}

		void* component_buffering = malloc(ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
		unsigned int component_buffering_size = 0;

		auto error_lambda = [=]() {
			free(component_buffering);
			CloseFile(file_handle);
		};

		// Read the header first
		SerializeEntityManagerHeader header;
		bool success = ReadFile(file_handle, { &header, sizeof(header) });
		if (!success) {
			error_lambda();
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
		}

		if (header.version != ENTITY_MANAGER_SERIALIZE_VERSION || header.hierarchy_count > USHORT_MAX || header.archetype_count > ECS_MAIN_ARCHETYPE_MAX_COUNT) {
			error_lambda();
			return ECS_DESERIALIZE_ENTITY_MANAGER_HEADER_IS_INVALID;
		}

		// Too many components
		if (header.component_count > USHORT_MAX || header.shared_component_count > USHORT_MAX) {
			error_lambda();
			return ECS_DESERIALIZE_ENTITY_MANAGER_HEADER_IS_INVALID;
		}

		// Read the ComponentPairs and the SharedComponentPairs first
		unsigned int component_pair_size = header.component_count * sizeof(ComponentPair);
		unsigned int shared_component_pair_size = header.shared_component_count * sizeof(SharedComponentPair);

		success = ReadFile(file_handle, { component_buffering, component_pair_size + shared_component_pair_size });
		if (!success) {
			error_lambda();
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
		}
		ComponentPair* component_pairs = (ComponentPair*)component_buffering;
		SharedComponentPair* shared_component_pairs = (SharedComponentPair*)function::OffsetPointer(component_buffering, component_pair_size);
		
		// Now determine how many named shared instances are
		unsigned int named_shared_instances_count = 0;
		for (unsigned int index = 0; index < header.shared_component_count; index++) {
			named_shared_instances_count += shared_component_pairs[index].named_shared_instance_count;
		}

		// Read all the "headers" of the named shared instances - the shared instance itself and the size of the pointer
		struct NamedSharedInstanceHeader {
			SharedInstance instance;
			unsigned short identifier_size;
		};
		NamedSharedInstanceHeader* named_headers = (NamedSharedInstanceHeader*)function::OffsetPointer(shared_component_pairs, shared_component_pair_size);
		success = ReadFile(file_handle, { named_headers, sizeof(NamedSharedInstanceHeader) * named_shared_instances_count });
		if (!success) {
			error_lambda();
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
		}

		// Now read the identifier buffer data from the named shared instances
		unsigned int named_shared_instances_identifier_total_size = 0;
		for (unsigned int index = 0; index < named_shared_instances_count; index++) {
			named_shared_instances_identifier_total_size += named_headers[index].identifier_size;
		}
		void* named_shared_instances_identifiers = named_headers + named_shared_instances_count;
		success = ReadFile(file_handle, { named_shared_instances_identifiers, named_shared_instances_identifier_total_size });
		if (!success) {
			error_lambda();
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
		}

		component_buffering_size = (uintptr_t)function::OffsetPointer(named_shared_instances_identifiers, named_shared_instances_identifier_total_size) - (uintptr_t)component_buffering;
		ECS_ASSERT(component_buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

		// Now the components and the shared components must be created such that when creating the shared components
		// the instances can be added rightaway
		for (unsigned int index = 0; index < header.component_count; index++) {
			entity_manager->RegisterComponentCommit(component_pairs[index].component, component_pairs[index].size.size);
		}
		for (unsigned int index = 0; index < header.shared_component_count; index++) {
			entity_manager->RegisterSharedComponentCommit(shared_component_pairs[index].component, shared_component_pairs[index].size.size);
		}

		// Now the actual shared instance data must be read - either the extracted one or the blitted one
		if (shared_component_table == nullptr) {
			// The data was blitted
			for (unsigned int index = 0; index < header.shared_component_count; index++) {
				void* current_instance_data = function::OffsetPointer(component_buffering, component_buffering_size);
				
				unsigned int component_size = shared_component_pairs[index].size.size;
				unsigned int write_size = component_size * shared_component_pairs[index].shared_instance_count;
				success = ReadFile(file_handle, { current_instance_data, write_size });
				if (!success) {
					error_lambda();
					return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
				}
				// The component buffering size doesn't need to be updated - the updates are performed now
				ECS_ASSERT(component_buffering_size + write_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

				// Create the shared instances now
				for (unsigned int shared_instance = 0; shared_instance < shared_component_pairs[index].shared_instance_count; shared_instance++) {
					entity_manager->RegisterSharedInstanceCommit({ (unsigned short)index }, function::OffsetPointer(current_instance_data, component_size* shared_instance));
				}
			}
		}
		else {
			// The data possibly was extracted
			for (unsigned int index = 0; index < header.shared_component_count; index++) {
				DeserializeEntityManagerExtractSharedComponent extract_component;
				if (shared_component_table->TryGetValue<false>(shared_component_pairs[index].component, extract_component)) {
					// The data was extracted
					// Read the header of sizes
					unsigned int* instances_sizes = (unsigned int*)function::OffsetPointer(component_buffering, component_buffering_size);
					unsigned int current_buffering_size = component_buffering_size;

					component_buffering_size += sizeof(unsigned int) * shared_component_pairs[index].shared_instance_count;

					success = ReadFile(file_handle, { instances_sizes, sizeof(unsigned int) * shared_component_pairs[index].shared_instance_count });
					if (!success) {
						error_lambda();
						return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
					}

					// Determine the size of the all instances such that they can be read in bulk
					unsigned int total_instance_size = 0;
					for (unsigned int subindex = 0; subindex < shared_component_pairs[index].shared_instance_count; subindex++) {
						total_instance_size += instances_sizes[subindex];
					}

					// Read in bulk now the instance data
					ECS_ASSERT(component_buffering_size + total_instance_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
					void* instances_data = function::OffsetPointer(component_buffering, component_buffering_size);
					void* write_data = function::OffsetPointer(instances_data, total_instance_size);
					success = ReadFile(file_handle, { instances_data, total_instance_size });
					if (!success) {
						error_lambda();
						return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
					}

					unsigned int current_instance_offset = 0;
					for (unsigned int shared_instance = 0; shared_instance < shared_component_pairs[index].shared_instance_count; shared_instance++) {
						// Now call the extract function
						bool is_data_valid = extract_component(
							{ (unsigned short)shared_instance },
							function::OffsetPointer(instances_data, current_instance_offset),
							instances_sizes[shared_instance],
							write_data,
							extra_shared_component_table_data
						);
						if (!is_data_valid) {
							error_lambda();
							return ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID;
						}

						// Commit the shared instance
						entity_manager->RegisterSharedInstanceCommit(shared_component_pairs[index].component, write_data);
					}

					// The size doesn't need to be updated - the data is commited rightaway - this will ensure that the 
					// data is always hot in cache
					component_buffering_size = current_buffering_size;
				}
				else {
					// Same as the other loop
					void* current_instance_data = function::OffsetPointer(component_buffering, component_buffering_size);

					unsigned int component_size = shared_component_pairs[index].size.size;
					unsigned int write_size = component_size * shared_component_pairs[index].shared_instance_count;
					success = ReadFile(file_handle, { current_instance_data, write_size });
					if (!success) {
						error_lambda();
						return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
					}
					ECS_ASSERT(component_buffering_size + write_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

					// Create the shared instances now
					for (unsigned int shared_instance = 0; shared_instance < shared_component_pairs[index].shared_instance_count; shared_instance++) {
						entity_manager->RegisterSharedInstanceCommit({ (unsigned short)index }, function::OffsetPointer(current_instance_data, component_size * shared_instance));
					}
				}
			}
		}

		// After the instances have been created, we can now bind the named tags to them
		named_shared_instances_identifier_total_size = 0;
		named_shared_instances_count = 0;
		for (unsigned int index = 0; index < header.shared_component_count; index++) {
			for (unsigned int instance_index = 0; instance_index < shared_component_pairs[index].shared_instance_count; instance_index++) {
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
		component_buffering_size = 0;

		ArchetypeHeader* archetype_headers = (ArchetypeHeader*)function::OffsetPointer(component_buffering, component_buffering_size);
		component_buffering_size += sizeof(ArchetypeHeader) * header.archetype_count;

		success = ReadFile(file_handle, { archetype_headers, sizeof(ArchetypeHeader) * header.archetype_count });
		if (!success) {
			error_lambda();
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
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

		success = ReadFile(
			file_handle, 
			{ function::OffsetPointer(component_buffering, component_buffering_size), base_archetypes_instance_size + base_archetypes_counts_size 
			 + archetypes_component_signature_size }
		);
		if (!success) {
			error_lambda();
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
		}
		
		Component* archetype_component_signatures = (Component*)function::OffsetPointer(component_buffering, component_buffering_size);
		component_buffering_size += archetypes_component_signature_size;
		SharedInstance* base_archetypes_instances = (SharedInstance*)function::OffsetPointer(component_buffering, component_buffering_size);
		component_buffering_size += base_archetypes_instance_size;
		unsigned int* base_archetypes_sizes = (unsigned int*)function::OffsetPointer(component_buffering, component_buffering_size);
		component_buffering_size += base_archetypes_counts_size;
		ECS_ASSERT(component_buffering_size <= ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

		unsigned int component_signature_offset = 0;
		unsigned int base_archetype_instances_offset = 0;
		unsigned int base_archetype_sizes_offset = 0;
		for (unsigned int index = 0; index < header.archetype_count; index++) {
			// Create the archetypes
			ComponentSignature unique;
			unique.indices = archetype_component_signatures + component_signature_offset;
			unique.count = archetype_headers[index].unique_count;

			ComponentSignature shared;
			shared.indices = archetype_component_signatures + component_signature_offset + archetype_headers[index].unique_count;
			shared.count = archetype_headers[index].shared_count;
			unsigned short archetype_index = entity_manager->CreateArchetypeCommit(unique, shared);
			SharedComponentSignature shared_signature;
			shared_signature.indices = shared.indices;
			shared_signature.count = shared.count;
			for (unsigned int base_index = 0; base_index < archetype_headers[index].base_count; base_index++) {
				shared_signature.instances = base_archetypes_instances + base_archetype_instances_offset;
				// Create the base archetypes
				entity_manager->CreateArchetypeBaseCommit(archetype_index, shared_signature, base_archetypes_sizes[base_archetype_sizes_offset]);

				// Bump forward the shared indices
				base_archetype_instances_offset += shared.count;
				base_archetype_sizes_offset++;
			}
			// Bump forward the component offset
			component_signature_offset += unique.count + shared.count;
		}

		// The buffering can be bumped back, it is no longer needed
		component_buffering_size = 0;

		// Only the component data of the base archetypes remains to be restored, the entities must be deduces from the
		// entity pool
		for (unsigned int index = 0; index < header.archetype_count; index++) {
			Archetype* archetype = entity_manager->GetArchetype(index);
			unsigned int base_count = archetype->GetBaseCount();
			ComponentSignature unique = archetype->GetUniqueSignature();

			if (component_table == nullptr) {
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					// Just read into the archetype
					ArchetypeBase* base = archetype->GetBase(base_index);
					for (unsigned int component_index = 0; component_index < unique.count; component_index++) {
						success = ReadFile(
							file_handle,
							{ base->m_buffers[component_index], entity_manager->m_unique_components[unique.indices[component_index].value].size * base->m_capacity }
						);
						if (!success) {
							error_lambda();
							return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
						}
					}
				}
			}
			else {
				// The order of the loops cannot be interchanged - the serialized data is written for each base together
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					ArchetypeBase* base = archetype->GetBase(base_index);
					for (unsigned int component_index = 0; component_index < unique.count; component_index++) {
						DeserializeEntityManagerExtractComponent extract_function;
						if (component_table->TryGetValue<false>(unique.indices[component_index], extract_function)) {
							// Use the buffering to read in
							// At first a "header" of sizes must be read
							success = ReadFile(
								file_handle,
								{ component_buffering, sizeof(unsigned int) * base->m_capacity }
							);
							if (!success) {
								error_lambda();
								return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
							}
							component_buffering_size = sizeof(unsigned int) * base->m_capacity;

							unsigned int* entity_serialized_data_size = (unsigned int*)component_buffering;
							// Now calculate the total size of the component data
							unsigned int total_component_data = 0;
							for (unsigned int entity_index = 0; entity_index < base->m_capacity; entity_index++) {
								total_component_data += entity_serialized_data_size[entity_index];
							}

							const void* starting_component_data = function::OffsetPointer(component_buffering, component_buffering_size);
							success = ReadFile(
								file_handle,
								{ starting_component_data, total_component_data }
							);
							if (!success) {
								error_lambda();
								return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
							}

							// Now call the extract function for each component
							unsigned int component_offset = 0;
							void* archetype_buffer = base->m_buffers[component_index];
							unsigned short component_size = entity_manager->m_unique_components[unique.indices[component_index].value].size;
							for (unsigned int entity_index = 0; entity_index < base->m_capacity; entity_index++) {
								bool is_data_valid = extract_function(
									function::OffsetPointer(starting_component_data, component_offset),
									entity_serialized_data_size[entity_index],
									function::OffsetPointer(archetype_buffer, entity_index * component_size),
									extra_component_table_data
								);
								if (!is_data_valid) {
									error_lambda();
									return ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID;
								}
							}
						}
						else {
							// The same is in the other branch
							success = ReadFile(
								file_handle,
								{ base->m_buffers[component_index], entity_manager->m_unique_components[unique.indices[component_index].value].size * base->m_capacity }
							);
							if (!success) {
								error_lambda();
								return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
							}
						}
					}
				}
			}
		}

		// The base archetypes are missing their entities, but these will be restored after reading the entity pool,
		// which can be read now
		success = DeserializeEntityPool(entity_manager->m_entity_pool, file_handle);
		if (!success) {
			error_lambda();
			return ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID;
		}
		component_buffering_size = 0;

		CapacityStream<unsigned int> pool_chunk_indices(component_buffering, 0, ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY / sizeof(unsigned int));

		// Walk through the list of entities and write them inside the base archetypes
		for (unsigned int pool_index = 0; pool_index < entity_manager->m_entity_pool->m_entity_infos.size; pool_index++) {
			if (entity_manager->m_entity_pool->m_entity_infos[pool_index].is_in_use) {
				pool_chunk_indices.size = 0;
				entity_manager->m_entity_pool->m_entity_infos[pool_index].stream.GetItemIndices(pool_chunk_indices);
				for (unsigned int entity_index = 0; entity_index < pool_chunk_indices.size; entity_index++) {
					// Construct the entity from the entity info
					EntityInfo info = entity_manager->m_entity_pool->m_entity_infos[pool_index].stream[pool_chunk_indices[entity_index]];
					Entity entity = entity_manager->m_entity_pool->GetEntityFromPosition(pool_index, pool_chunk_indices[entity_index]);
					// Set the entity generation count the same as in the info
					entity.generation_count = info.generation_count;

					// Now set the according archetype's entity inside the entities buffer
					Archetype* archetype = entity_manager->GetArchetype(info.main_archetype);
					ArchetypeBase* base = archetype->GetBase(info.base_archetype);
					base->m_entities[info.stream_index] = entity;
				}
			}
		}

		// At last, the entity hierarchies can be deserialized
		// But first a stream of bools need to be read (indicating if a hierarchy is present or not)
		success = ReadFile(file_handle, { component_buffering, sizeof(unsigned short) * header.hierarchy_count });
		if (!success) {
			error_lambda();
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
		}

		unsigned short* hierarchy_indices = (unsigned short*)component_buffering;
		for (unsigned int hierarchy_index = 0; hierarchy_index < header.hierarchy_count; hierarchy_index++) {
			entity_manager->CreateHierarchy(hierarchy_indices[hierarchy_index]);
			success = DeserializeEntityHierarchy(&entity_manager->m_hierarchies[hierarchy_indices[hierarchy_index]].hierarchy, file_handle);
			if (!success) {
				error_lambda();
				return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
			}
		}

		// Everything should be reconstructed now

		free(component_buffering);
		CloseFile(file_handle);
		return ECS_DESERIALIZE_ENTITY_MANAGER_OK;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	// It can simply fallthrough - the components are already unique
	unsigned int EntityManagerHashComponent::Hash(Component component)
	{
		return component.value;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

}