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
#include "../Utilities/ReaderWriterInterface.h"
#include "../Utilities/InMemoryReaderWriter.h"

#define ENTITY_MANAGER_SERIALIZE_VERSION 0

// This is a large value that is used to buffer the header writes, which require a bit of buffering
#define ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY ECS_MB * 64

namespace ECSEngine {

	using namespace internal;

	typedef ComponentFunctions ComponentFixup;

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
		GlobalComponentPair* global_component_pairs;
		Component names (the characters only);
		SharedComponent names (the characters only);
		GlobalComponent names (the characters only);

		SharedComponentInstances - these are needed in order to remapp the instances stored in the base archetypes
		to the new instances that are created sequentially with RegisterSharedInstance

		InitializeComponent data (bytes);
		InitializeSharedComponent data (bytes);
		InitializeGlobalComponent data (bytes);

		NamedSharedInstances* tuple (instance, ushort identifier size) followed by identifier.ptr stream;

		SharedComponent data (for each instance);

		GlobalComponent data with size prefix;

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

	bool SerializeEntityManagerHeaderSection(
		const EntityManager* entity_manager,
		WriteInstrument* write_instrument,
		const SerializeEntityManagerOptions* options,
		CapacityStream<void>* buffering,
		SerializeEntityManagerHeaderSectionOutput* output
	) {
		SerializeEntityManagerHeaderSectionOutput default_output;
		if (output == nullptr) {
			output = &default_output;
			ZeroOut(&default_output);
		}

		CapacityStream<void> local_buffering;
		unsigned int initial_buffering_size = 0;
		if (buffering == nullptr) {
			local_buffering.Initialize(ECS_MALLOC_ALLOCATOR, ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);
			buffering = &local_buffering;
		}
		else {
			initial_buffering_size = buffering->size;
		}

		// Releases the memory for the local buffering, if it was allocated by us
		auto local_buffering_release = StackScope([&]() {
			if (buffering == &local_buffering) {
				local_buffering.Deallocate(ECS_MALLOC_ALLOCATOR);
			}
			else {
				buffering->size = initial_buffering_size;
			}
		});

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(local_stack_allocator, ECS_KB * 64, ECS_MB);
		AllocatorPolymorphic stack_allocator = &local_stack_allocator;
		if (output->unique_components.has_value || output->shared_components.has_value) {
			stack_allocator = output->temporary_allocator;
		}

		// We will need this value when we rewrite the header
		size_t initial_write_instrument_offset = write_instrument->GetOffset();

		// Write the header first
		SerializeEntityManagerHeader* header = buffering->Reserve<SerializeEntityManagerHeader>();
		header->version = ENTITY_MANAGER_SERIALIZE_VERSION;
		header->archetype_count = entity_manager->m_archetypes.size;
		header->component_count = 0;
		header->shared_component_count = 0;
		header->global_component_count = entity_manager->m_global_component_count;
		memset(header->reserved, 0, sizeof(header->reserved));

		// Determine the component count and initialize the base component pairs
		ComponentPair* component_pairs = (ComponentPair*)OffsetPointer(*buffering);
		for (unsigned int index = 0; index < entity_manager->m_unique_components.size; index++) {
			Component component = { (short)index };

			bool exists = entity_manager->ExistsComponent(component);
			header->component_count += exists;

			if (exists) {
				ComponentPair* current_unique_pair = buffering->Reserve<ComponentPair>();
				// Check to see if it has a name and get its version
				SerializeEntityManagerComponentInfo component_info = options->component_table->GetValue(component);

				// Write the serialize version
				current_unique_pair->serialize_version = component_info.version;
				current_unique_pair->name_size = component_info.name.size;
				current_unique_pair->component_byte_size = entity_manager->m_unique_components[index].size;
				current_unique_pair->component = component;
				current_unique_pair->header_data_size = 0;
			}
		}

		SharedComponentPair* shared_component_pairs = (SharedComponentPair*)OffsetPointer(*buffering);
		for (unsigned int index = 0; index < entity_manager->m_shared_components.size; index++) {
			Component component = { (short)index };

			bool exists = entity_manager->ExistsSharedComponent({ (short)index });
			header->shared_component_count += exists;

			if (exists) {
				SharedComponentPair* current_shared_pair = buffering->Reserve<SharedComponentPair>();

				// Check to see if it has a name and get its version
				SerializeEntityManagerSharedComponentInfo component_info = options->shared_component_table->GetValue(component);

				// Write the serialize version
				current_shared_pair->serialize_version = component_info.version;
				current_shared_pair->component = component;
				current_shared_pair->component_byte_size = entity_manager->m_shared_components[index].info.size;
				current_shared_pair->name_size = component_info.name.size;
				current_shared_pair->instance_count = (unsigned short)entity_manager->m_shared_components[index].instances.stream.size;
				current_shared_pair->named_instance_count = (unsigned short)entity_manager->m_shared_components[index].named_instances.GetCount();
				current_shared_pair->header_data_size = 0;
			}
		}

		// Determine the component count
		GlobalComponentPair* global_component_pairs = (GlobalComponentPair*)OffsetPointer(*buffering);
		for (unsigned int index = 0; index < entity_manager->m_global_component_count; index++) {
			GlobalComponentPair* current_global_pair = buffering->Reserve<GlobalComponentPair>();
			Component component = entity_manager->m_global_components[index];

			// Check to see if it has a name and get its version
			SerializeEntityManagerGlobalComponentInfo component_info = options->global_component_table->GetValue(component);

			// Write the serialize version
			current_global_pair->serialize_version = component_info.version;
			current_global_pair->name_size = component_info.name.size;
			current_global_pair->component_byte_size = entity_manager->m_global_components_info[index].size;
			current_global_pair->component = component;
			current_global_pair->header_data_size = 0;
		}

		// Keep a stack copy of the components and shared_components that are registered such that it will make the iteration over the components faster
		Component* registered_components = (Component*)Allocate(stack_allocator, sizeof(Component) * header->component_count);
		Component* registered_shared_components = (Component*)Allocate(stack_allocator, sizeof(Component) * header->shared_component_count);

		for (unsigned short index = 0; index < header->component_count; index++) {
			registered_components[index] = component_pairs[index].component;
		}

		for (unsigned short index = 0; index < header->shared_component_count; index++) {
			registered_shared_components[index] = shared_component_pairs[index].component;
		}

		if (output->unique_components.has_value) {
			output->unique_components.value = { registered_components, header->component_count };
		}
		if (output->shared_components.has_value) {
			output->shared_components.value = { registered_shared_components, header->shared_component_count };
		}

		// Write the names now - components, shared components and global components
		auto register_component_names = [&](Stream<Component> components, auto component_table) {
			for (unsigned int index = 0; index < components.size; index++) {
				Component component = components[index];

				// Check to see if it has a name
				auto component_info = component_table->GetValue(component);
				if (component_info.name.size > 0) {
					memcpy(buffering->Reserve(component_info.name.size), component_info.name.buffer, component_info.name.size);
				}
			}
		};

		register_component_names({ registered_components, header->component_count }, options->component_table);
		register_component_names({ registered_shared_components, header->shared_component_count }, options->shared_component_table);
		register_component_names({ entity_manager->m_global_components, entity_manager->m_global_component_count }, options->global_component_table);

		// Tell the instrument to leave space for the header structures that we will be finalizing later on
		if (!write_instrument->AppendUninitialized(buffering->size - initial_buffering_size)) {
			return false;
		}

		// Now write the headers data for each component
		// component_pairs_stream need to be a stream with the component_pairs as the buffer
		// The header_data is used to have the correct type to be passed to the header function
		// It will be filled with the same fields as the unique component (at the moment all header
		// data is the same)
		auto write_header_component_data = [&](auto component_pairs_stream, auto component_table, auto* header_data) {
			size_t write_offset = write_instrument->GetOffset();
			for (size_t index = 0; index < component_pairs_stream.size; index++) {
				auto component_info = component_table->GetValue(component_pairs_stream[index].component);
				if (component_info.header_function != nullptr) {
					header_data->write_instrument = write_instrument;
					header_data->extra_data = component_info.extra_data;
					if (!component_info.header_function(header_data)) {
						return false;
					}

					size_t new_write_offset = write_instrument->GetOffset();
					size_t header_data_size = new_write_offset - write_offset;
					// Ensure that the header data size fits
					if (!EnsureUnsignedIntegerIsInRange<unsigned int>(header_data_size)) {
						return false;
					}
					component_pairs_stream[index].header_data_size = header_data_size;
					write_offset = new_write_offset;
				}
			}

			return true;
		};

		SerializeEntityManagerHeaderComponentData unique_header_data;
		bool success = write_header_component_data(Stream<ComponentPair>(component_pairs, header->component_count), options->component_table, &unique_header_data);
		if (!success) {
			return false;
		}

		SerializeEntityManagerHeaderSharedComponentData shared_header_data;
		success = write_header_component_data(Stream<SharedComponentPair>(
			shared_component_pairs,
			header->shared_component_count),
			options->shared_component_table,
			&shared_header_data
		);
		if (!success) {
			return false;
		}

		SerializeEntityManagerHeaderGlobalComponentData global_header_data;
		success = write_header_component_data(Stream<GlobalComponentPair>(
			global_component_pairs,
			header->global_component_count),
			options->global_component_table,
			&global_header_data
		);
		if (!success) {
			return false;
		}

		// The data has been finalized, it can be written now
		if (!write_instrument->WriteUninitializedData(initial_write_instrument_offset, OffsetPointer(buffering->buffer, initial_buffering_size), buffering->size)) {
			return false;
		}

		return true;
	}

	bool SerializeEntityManager(
		const EntityManager* entity_manager,
		WriteInstrument* write_instrument,
		CapacityStream<void>& buffering,
		const SerializeEntityManagerOptions* options,
		const SerializeEntityManagerHeaderSectionOutput* output
	) {
		// Ensure that the output contains the shared components
		ECS_ASSERT(output->shared_components.has_value);
		Stream<Component> registered_shared_components = output->shared_components.value;

		buffering.size = 0;
		// Write the shared instances for each shared component now
		for (size_t index = 0; index < registered_shared_components.size; index++) {
			CapacityStream<SharedInstance> instances = { buffering.buffer, 0, (unsigned int)(buffering.capacity / sizeof(SharedInstance)) };
			entity_manager->GetSharedComponentInstanceAll(registered_shared_components[index], instances);
			if (instances.size > 0) {
				if (!write_instrument->Write(instances.buffer, instances.CopySize())) {
					return false;
				}
			}
		}

		bool success = true;
		// Now write the named shared instances - firstly the instance and the identifier size
		for (unsigned int index = 0; index < registered_shared_components.size; index++) {
			Component current_component = registered_shared_components[index];
			// Write the pairs as shared instance, identifier size and at the end the buffers
			// This is done like this in order to avoid on deserialization to read small chunks
			// In this way all the pairs can be read at a time and then the identifiers
			entity_manager->m_shared_components[current_component].named_instances.ForEachConst([&](SharedInstance instance, ResourceIdentifier identifier) {
				success &= EnsureUnsignedIntegerIsInRange<unsigned short>(identifier.size);
				NamedSharedInstanceHeader named_instance_header = { instance, (unsigned short)identifier.size };
				success &= write_instrument->Write(&named_instance_header);
				});
		}
		if (!success) {
			return false;
		}

		// Write the identifiers pointer data now
		for (unsigned int index = 0; index < registered_shared_components.size; index++) {
			Component current_component = registered_shared_components[index];
			entity_manager->m_shared_components[current_component].named_instances.ForEachConst([&](SharedInstance instance, ResourceIdentifier identifier) {
				success &= write_instrument->Write(identifier.ptr, identifier.size);
			});
		}
		if (!success) {
			return false;
		}

		// The instance data sizes for each shared component must be written before the actual data if the extract function is used
		// Because otherwise the data cannot be recovered
		for (unsigned int index = 0; index < registered_shared_components.size; index++) {
			Component current_component = registered_shared_components[index];
			SerializeEntityManagerSharedComponentInfo component_info = options->shared_component_table->GetValue(current_component);

			// Prefix the actual instance data size with the write size of each instance. Use the buffering to hold
			// The integer sizes and also append into the write instrument, such that we don't have to shift the actual
			// Instance writes.
			buffering.size = 0;
			size_t instance_data_size_instrument_offset = write_instrument->GetOffset();
			size_t instance_data_sizes_byte_size = sizeof(unsigned int) * entity_manager->m_shared_components[current_component].instances.stream.size;
			unsigned int* instance_data_sizes = (unsigned int*)buffering.Reserve(instance_data_sizes_byte_size);
			if (instance_data_sizes_byte_size > 0) {
				if (!write_instrument->AppendUninitialized(instance_data_sizes_byte_size)) {
					return false;
				}
			}

			size_t shared_component_write_offset = write_instrument->GetOffset();
			unsigned int write_index = 0;
			if (entity_manager->m_shared_components[current_component].instances.stream.ForEachIndex<true>([&](unsigned int instance_index) {
				SerializeEntityManagerSharedComponentData function_data;
				function_data.write_instrument = write_instrument;
				function_data.component_data = entity_manager->m_shared_components[current_component].instances[instance_index];
				function_data.extra_data = component_info.extra_data;
				function_data.instance = { (short)instance_index };

				if (!component_info.function(&function_data)) {
					return true;
				}

				size_t current_instrument_offset = write_instrument->GetOffset();
				size_t current_write_size = current_instrument_offset - shared_component_write_offset;
				if (!EnsureUnsignedIntegerIsInRange<unsigned int>(current_write_size)) {
					return true;
				}

				// Write the count into the the header of sizes
				instance_data_sizes[write_index++] = (unsigned int)current_write_size;
				shared_component_write_offset = current_instrument_offset;
				return false;
				}))
			{
				return false;
			}

			// Go back and write the instance data sizes
			if (buffering.size > 0) {
				if (!write_instrument->WriteUninitializedData(instance_data_size_instrument_offset, buffering.buffer, buffering.size)) {
					return false;
				}
			}
		}

		// We must serialize the global components now
		// Write before each serialization the size of the data
		size_t global_component_write_offset = write_instrument->GetOffset();
		for (unsigned int index = 0; index < entity_manager->m_global_component_count; index++) {
			SerializeEntityManagerGlobalComponentInfo component_info = options->global_component_table->GetValue(entity_manager->m_global_components[index]);
			unsigned int global_component_size = 0;
			if (!write_instrument->AppendUninitialized(sizeof(global_component_size))) {
				return false;
			}

			SerializeEntityManagerGlobalComponentData function_data;
			function_data.write_instrument = write_instrument;
			function_data.components = entity_manager->m_global_components_data[index];
			function_data.extra_data = component_info.extra_data;
			function_data.count = 1;

			if (!component_info.function(&function_data)) {
				return false;
			}

			size_t current_write_offset = write_instrument->GetOffset();
			// Subtract the size of the component size that is included in the difference
			size_t write_difference = current_write_offset - global_component_write_offset - sizeof(global_component_size);
			if (!EnsureUnsignedIntegerIsInRange<unsigned int>(write_difference)) {
				return false;
			}
			// Write the count into the prefix size
			global_component_size = (unsigned int)write_difference;
			if (!write_instrument->WriteUninitializedData(global_component_write_offset, &global_component_size, sizeof(global_component_size))) {
				return false;
			}

			global_component_write_offset = current_write_offset;
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

			ArchetypeHeader archetype_header;
			archetype_header.base_count = base_count;
			archetype_header.shared_count = shared.count;
			archetype_header.unique_count = unique.count;

			if (!write_instrument->Write(&archetype_header)) {
				return false;
			}
		}

		// The unique and shared components can be written now
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			ComponentSignature unique = archetype->GetUniqueSignature();
			ComponentSignature shared = archetype->GetSharedSignature();

			if (!write_instrument->Write(unique.indices, unique.count * sizeof(unique.indices[0]))) {
				return false;
			}
			if (!write_instrument->Write(shared.indices, shared.count * sizeof(shared.indices[0]))) {
				return false;
			}
		}

		// The shared instances can be written now
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			ComponentSignature unique = archetype->GetUniqueSignature();
			ComponentSignature shared = archetype->GetSharedSignature();
			unsigned int base_count = archetype->GetBaseCount();

			// Write the shared instances for each base archetype in a single stream
			for (size_t base_index = 0; base_index < base_count; base_index++) {
				if (!write_instrument->Write(archetype->GetBaseInstances(base_index), sizeof(SharedInstance) * shared.count)) {
					return false;
				}
			}
		}

		// The base archetype sizes can be written now
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			unsigned int base_count = archetype->GetBaseCount();

			// Write the size of each base archetype
			for (size_t base_index = 0; base_index < base_count; base_index++) {
				const ArchetypeBase* base = archetype->GetBase(base_index);
				ECS_ASSERT(base->m_size > 0);
				if (!write_instrument->Write(&base->m_size)) {
					return false;
				}
			}
		}

		// Now serialize the actual unique components - finally!
		size_t unique_component_write_offset = write_instrument->GetOffset();
		for (size_t index = 0; index < entity_manager->m_archetypes.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(index);
			ComponentSignature unique = archetype->GetUniqueSignature();
			unsigned int base_count = archetype->GetBaseCount();

			// The size of each archetype was previously written
			// Now only the buffers must be flushed
			for (size_t base_index = 0; base_index < base_count; base_index++) {
				ArchetypeBase* base = (ArchetypeBase*)archetype->GetBase(base_index);
				for (size_t component_index = 0; component_index < unique.count; component_index++) {
					// Add a prefix size for how long the serialization of these components is
					unsigned int current_write_count = 0;
					if (!write_instrument->AppendUninitialized(sizeof(current_write_count))) {
						return false;
					}

					const SerializeEntityManagerComponentInfo* component_info = options->component_table->GetValuePtr(unique.indices[component_index]);
					SerializeEntityManagerComponentData function_data;
					function_data.write_instrument = write_instrument;
					function_data.components = base->GetComponentByIndex(0, component_index);
					function_data.count = base->m_size;
					function_data.extra_data = component_info->extra_data;

					if (!component_info->function(&function_data)) {
						return false;
					}

					size_t current_offset = write_instrument->GetOffset();
					// Don't forget to subtract the write count from the write difference
					size_t write_difference = current_offset - unique_component_write_offset - sizeof(current_write_count);
					if (!EnsureUnsignedIntegerIsInRange<unsigned int>(write_difference)) {
						return false;
					}

					current_write_count = (unsigned int)write_difference;
					if (!write_instrument->WriteUninitializedData(unique_component_write_offset, &current_write_count, sizeof(current_write_count))) {
						return false;
					}
					unique_component_write_offset = current_offset;
				}
			}
		}

		// Write the entity pool
		if (!SerializeEntityPool(entity_manager->m_entity_pool, write_instrument)) {
			return false;
		}

		return SerializeEntityHierarchy(&entity_manager->m_hierarchy, write_instrument);
	}

	bool SerializeEntityManager(
		const EntityManager* entity_manager,
		const FileWriteInstrumentTarget& target,
		const SerializeEntityManagerOptions* options
	)
	{
		// Try to create the write instrument first, in case it fails, early exit
		// Don't use buffering in case we must create the instrument. We are maintaining our own buffering
		// In this function
		return target.Write(CapacityStream<void>(), nullptr, [&](WriteInstrument* write_instrument) {
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB * 32);

			CapacityStream<void> buffering;
			buffering.Initialize(ECS_MALLOC_ALLOCATOR, ENTITY_MANAGER_COMPONENT_BUFFERING_CAPACITY);

			// Create a stack scope to release this automatically
			auto deallocate_resources = StackScope([&]() {
				buffering.Deallocate(ECS_MALLOC_ALLOCATOR);
			});

			SerializeEntityManagerHeaderSectionOutput header_output;
			header_output.temporary_allocator = &stack_allocator;
			header_output.shared_components.has_value = true;
			if (!SerializeEntityManagerHeaderSection(entity_manager, write_instrument, options, &buffering, &header_output)) {
				return false;
			}

			return SerializeEntityManager(entity_manager, write_instrument, buffering, options, &header_output);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	static void DeserializeEntityManagerInitializeRemappSharedInstances(
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
	static void DeserializeEntityManagerRemappSharedInstances(
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
			shared_pair_index[index] = SearchBytes(component_stream, shared_pair_count, shared_signature_basic[index].value);
			ECS_ASSERT(shared_pair_index[index] != -1);
		}

		unsigned int base_count = archetype->GetBaseCount();
		for (unsigned int index = 0; index < base_count; index++) {
			SharedInstance* base_instances = archetype->GetBaseInstances(index);
			for (unsigned char component_index = 0; component_index < signature_count; component_index++) {
				unsigned int component_offset = shared_instance_offsets[shared_pair_index[component_index]];
				unsigned int instance_count = shared_instance_offsets[shared_pair_index[component_index] + 1] - component_offset;
				size_t remapp_value = SearchBytes(shared_instances + component_offset, instance_count, base_instances[component_index].value);
				ECS_ASSERT(remapp_value != -1);
				base_instances[component_index] = { (short)remapp_value };
			}
		}
	}

	// -------------------------------------------------------------------------------------------------------------------------------------------

	Optional<DeserializeEntityManagerHeaderSectionData> DeserializeEntityManagerHeaderSection(
		ReadInstrument* read_instrument,
		AllocatorPolymorphic temporary_allocator,
		const DeserializeEntityManagerOptions* options
	) {
		// Read the header first
		SerializeEntityManagerHeader header;
		if (!read_instrument->ReadAlways(&header)) {
			ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "Failed to read deserialize header");
			return {};
		}

		// Validate the header parameters
		if (header.version != ENTITY_MANAGER_SERIALIZE_VERSION || header.archetype_count > ECS_MAIN_ARCHETYPE_MAX_COUNT) {
			ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "The deserialize header is invalid/corrupted (invalid version/archetype count)");
			return {};
		}

		// Too many components
		if (header.component_count > SHORT_MAX || header.shared_component_count > SHORT_MAX || header.global_component_count > SHORT_MAX) {
			ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "The deserialize header is invalid/corrupted (component counts are too large)");
			return {};
		}

		// Read the ComponentPairs, the SharedComponentPairs and the GlobalComponentPairs first
		size_t component_pair_size = header.component_count * sizeof(ComponentPair);
		size_t shared_component_pair_size = header.shared_component_count * sizeof(SharedComponentPair);
		size_t global_component_pair_size = header.global_component_count * sizeof(GlobalComponentPair);

		bool allocation_failure = false;
		auto allocate_and_read_failure = [&](Stream<char> data_section_name) {
			if (options->detailed_error_string) {
				if (allocation_failure) {
					ECS_FORMAT_TEMP_STRING(message, "The data section \"{#}\" does not fit into the buffering", data_section_name);
					options->detailed_error_string->AddStreamAssert(message);
				}
				else {
					ECS_FORMAT_TEMP_STRING(message, "The data section \"{#}\" could not be read from disk", data_section_name);
					options->detailed_error_string->AddStreamAssert(message);
				}
			}
		};

		ComponentPair* component_pairs = (ComponentPair*)read_instrument->ReadOrReferenceDataPointer(
			temporary_allocator,
			component_pair_size + shared_component_pair_size + global_component_pair_size,
			nullptr,
			&allocation_failure
		);
		if (component_pairs == nullptr) {
			allocate_and_read_failure("Component Pairs");
			return {};
		}
		SharedComponentPair* shared_component_pairs = (SharedComponentPair*)OffsetPointer(component_pairs, component_pair_size);
		GlobalComponentPair* global_component_pairs = (GlobalComponentPair*)OffsetPointer(shared_component_pairs, shared_component_pair_size);

		size_t component_name_total_size = 0;
		size_t header_component_total_size = 0;
		for (size_t index = 0; index < header.component_count; index++) {
			component_name_total_size += component_pairs[index].name_size;
			header_component_total_size += component_pairs[index].header_data_size;
		}

		// Now determine how many named shared instances are and the name sizes
		size_t shared_component_name_total_size = 0;
		size_t header_shared_component_total_size = 0;
		for (size_t index = 0; index < header.shared_component_count; index++) {
			shared_component_name_total_size += shared_component_pairs[index].name_size;
			header_shared_component_total_size += shared_component_pairs[index].header_data_size;
		}

		size_t global_name_total_size = 0;
		size_t header_global_component_total_size = 0;
		for (size_t index = 0; index < header.global_component_count; index++) {
			global_name_total_size += global_component_pairs[index].name_size;
			header_global_component_total_size += global_component_pairs[index].header_data_size;
		}

		// Read all the names, for the unique and shared alongside, the headers for the components and the headers of the named shared instances - 
		// the shared instance itself and the size of the pointer

		size_t total_size_to_read = component_name_total_size
			+ shared_component_name_total_size + header_component_total_size + header_shared_component_total_size
			+ global_name_total_size + header_global_component_total_size;
		void* unique_name_characters = read_instrument->ReadOrReferenceDataPointer(temporary_allocator, total_size_to_read, &allocation_failure);
		if (unique_name_characters == nullptr) {
			allocate_and_read_failure("Component Headers");
			return {};
		}

		void* shared_name_characters = OffsetPointer(unique_name_characters, component_name_total_size);
		void* global_name_characters = OffsetPointer(shared_name_characters, shared_component_name_total_size);
		void* header_component_data = OffsetPointer(global_name_characters, global_name_total_size);
		void* header_shared_component_data = OffsetPointer(header_component_data, header_component_total_size);
		void* header_global_component_data = OffsetPointer(header_shared_component_data, header_shared_component_total_size);

		unsigned int* component_name_offsets = (unsigned int*)Allocate(temporary_allocator, sizeof(unsigned int) * header.component_count);
		unsigned int* shared_component_name_offsets = (unsigned int*)Allocate(temporary_allocator, sizeof(unsigned int) * header.shared_component_count);
		unsigned int* global_component_name_offsets = (unsigned int*)Allocate(temporary_allocator, sizeof(unsigned int) * header.global_component_count);

		// Construct the name lookup tables
		auto construct_name_offsets = [](unsigned int* name_offsets, auto component_pair_stream) {
			size_t offset = 0;
			for (size_t index = 0; index < component_pair_stream.size; index++) {
				if (component_pair_stream[index].name_size > 0) {
					name_offsets[index] = offset;
					offset += component_pair_stream[index].name_size;
				}
			}
			return offset;
		};

		construct_name_offsets(component_name_offsets, Stream<ComponentPair>(component_pairs, header.component_count));
		construct_name_offsets(shared_component_name_offsets, Stream<SharedComponentPair>(shared_component_pairs, header.shared_component_count));
		construct_name_offsets(global_component_name_offsets, Stream<GlobalComponentPair>(global_component_pairs, header.global_component_count));

		auto get_unique_component_name = [&](unsigned int index) {
			return Stream<char>(OffsetPointer(unique_name_characters, component_name_offsets[index]), component_pairs[index].name_size);
		};
		auto get_shared_component_name = [&](unsigned int index) {
			return Stream<char>(OffsetPointer(shared_name_characters, shared_component_name_offsets[index]), shared_component_pairs[index].name_size);
		};
		auto get_global_component_name = [&](unsigned int index) {
			return Stream<char>(OffsetPointer(global_name_characters, global_component_name_offsets[index]), global_component_pairs[index].name_size);
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
			auto get_name_functor
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

				auto iterate_table = [&](Stream<char> component_name) {
					ResourceIdentifier name = component_name;
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
					Stream<char> current_component_name = get_name_functor(component_pair_index);

					const void* untyped_component_info = component_table->TryGetValuePtr(component);
					if (untyped_component_info != nullptr) {
						component_info = (decltype(component_info))untyped_component_info;
						if (component_info->name.size == 0) {
							// Iterate through the table
							component_info = nullptr;
							iterate_table(current_component_name);
						}
						else {
							if (component_info->name != current_component_name) {
								component_info = nullptr;
								iterate_table(current_component_name);
							}
							else {
								found_at = component;
							}
						}
					}
					else {
						iterate_table(current_component_name);
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
			return search_matching_component(
				component,
				header.component_count,
				component_pairs,
				options->component_table,
				get_unique_component_name
			);
		};

		auto search_matching_component_shared = [&](Component component) {
			return search_matching_component(
				component,
				header.shared_component_count,
				shared_component_pairs,
				options->shared_component_table,
				get_shared_component_name
			);
		};

		auto search_matching_component_global = [&](Component component) {
			return search_matching_component(
				component,
				header.global_component_count,
				global_component_pairs,
				options->global_component_table,
				get_global_component_name
			);
		};

		// The header_data parameter is needed in order to provide the proper functor argument, it is
		// Not used to fill out data.
		auto initialize_cached_infos = [options, temporary_allocator](
			auto component_pair_stream,
			auto& cached_component_infos,
			auto& search_matching_component,
			auto* header_data,
			void** header_component_data,
			Stream<char> component_type_string,
			auto get_name_functor
			) -> bool {
				cached_component_infos.Initialize(temporary_allocator, PowerOfTwoGreater(component_pair_stream.size));

				for (unsigned int index = 0; index < component_pair_stream.size; index++) {
					// Insert the entry no matter if the header data is there or not
					unsigned int insert_position = 0;
					cached_component_infos.Insert(
						search_matching_component(component_pair_stream[index].component), 
						component_pair_stream[index].component, 
						insert_position
					);

					if (component_pair_stream[index].header_data_size > 0) {
						const auto* cached_info = cached_component_infos.GetValuePtrFromIndex(insert_position);
						if (cached_info->info != nullptr) {
							if (cached_info->info->header_function != nullptr) {
								// TODO: Determine if reading all the component data in bulk is more advantageous
								// Than relying on a reasonable buffering from the read instrument itself

								// Create an in memory instrument of the data that it corresponds to this component.
								uintptr_t header_component_data_ptr = (uintptr_t)*header_component_data;
								InMemoryReadInstrument local_read_instrument(header_component_data_ptr, component_pair_stream[index].header_data_size);

								header_data->extra_data = cached_info->info->extra_data;
								header_data->read_instrument = &local_read_instrument;
								header_data->size = component_pair_stream[index].header_data_size;
								header_data->version = cached_info->version;

								bool is_valid = cached_info->info->header_function(header_data);
								if (!is_valid) {
									ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "The {#} component {#} header is invalid", component_type_string,
										get_name_functor(index));
									return false;
								}
							}
						}
						*header_component_data = OffsetPointer(*header_component_data, component_pair_stream[index].header_data_size);
						// If the component is not found, then the initialization is skipped
					}
				}
				return true;
		};

		DeserializeEntityManagerHeaderSectionData header_section;
		header_section.header = header;

		header_section.component_pairs = { component_pairs, header.component_count };
		header_section.shared_component_pairs = { shared_component_pairs, header.shared_component_count };
		header_section.global_component_pairs = { global_component_pairs, header.global_component_count };

		header_section.unique_name_characters = (const char*)unique_name_characters;
		header_section.shared_name_characters = (const char*)shared_name_characters;
		header_section.global_name_characters = (const char*)global_name_characters;

		header_section.unique_name_offsets = component_name_offsets;
		header_section.shared_name_offsets = shared_component_name_offsets;
		header_section.global_name_offsets = global_component_name_offsets;

		DeserializeEntityManagerHeaderComponentData header_unique_data;
		if (!initialize_cached_infos(
			Stream<ComponentPair>(component_pairs, header.component_count),
			header_section.cached_unique_infos,
			search_matching_component_unique,
			&header_unique_data,
			&header_component_data,
			"unique",
			get_unique_component_name
		)) {
			return {};
		}
		ECS_ASSERT(header_component_data <= header_shared_component_data);

		DeserializeEntityManagerHeaderSharedComponentData header_shared_data;
		if (!initialize_cached_infos(
			Stream<SharedComponentPair>(shared_component_pairs, header.shared_component_count),
			header_section.cached_shared_infos,
			search_matching_component_shared,
			&header_shared_data,
			&header_shared_component_data,
			"shared",
			get_shared_component_name
		)) {
			return {};
		}
		ECS_ASSERT(header_shared_component_data <= header_global_component_data);

		DeserializeEntityManagerHeaderGlobalComponentData header_global_data;
		if (!initialize_cached_infos(
			Stream<GlobalComponentPair>(global_component_pairs, header.global_component_count),
			header_section.cached_global_infos,
			search_matching_component_global,
			&header_global_data,
			&header_global_component_data,
			"global",
			get_global_component_name
		)) {
			return {};
		}
		
		return header_section;
	}

	// -------------------------------------------------------------------------------------------------------------------------------------------

	ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		const FileReadInstrumentTarget& read_target,
		const DeserializeEntityManagerOptions* options
	) {
		ECS_ASSERT(options->component_table && options->shared_component_table && options->global_component_table, "Deserialize Entity Manager requires"
			" all component tables to be specified");

		ECS_DESERIALIZE_ENTITY_MANAGER_STATUS status;
		read_target.Read(options->detailed_error_string, [&](ReadInstrument* read_instrument) {
			// If the read instrument is size determination, fail, we don't support that
			if (read_instrument->IsSizeDetermination()) {
				ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "Deserialize Entity Manager does not accept size determination ReadInstrument");
				status = ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
				return false;
			}

			bool previous_assert_crash = ECS_GLOBAL_ASSERT_CRASH;
			ECS_GLOBAL_ASSERT_CRASH = true;

			// Call the crash handler recovery such that it won't try to crash the entire editor
			ECS_SET_RECOVERY_CRASH_HANDLER(false) {
				ResetRecoveryCrashHandler();
				ECS_GLOBAL_ASSERT_CRASH = previous_assert_crash;
				ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "Unknown error - corrupt data");
				status = ECS_DESERIALIZE_ENTITY_MANAGER_FILE_IS_CORRUPT;
				return false;
			}

			// We will also need a large buffering allocator, because we might need to read a lot of data at once
			ResizableLinearAllocator temporary_allocator(ECS_MB * 50, ECS_MB * 100, ECS_MALLOC_ALLOCATOR);

			struct Deallocator {
				ECS_INLINE void operator()() {
					// Don't forget to reset the crash handler
					temporary_allocator->Free();
					ResetRecoveryCrashHandler();
					ECS_GLOBAL_ASSERT_CRASH = assert_crash_value;
				}

				ResizableLinearAllocator* temporary_allocator;
				bool assert_crash_value;
			};

			StackScope<Deallocator> scope_deallocator({ &temporary_allocator, previous_assert_crash });

			Optional<DeserializeEntityManagerHeaderSectionData> header_section_optional = DeserializeEntityManagerHeaderSection(read_instrument, &temporary_allocator, options);
			if (!header_section_optional.has_value) {
				status = ECS_DESERIALIZE_ENTITY_MANAGER_HEADER_IS_INVALID;
				return false;
			}

			status = DeserializeEntityManager(entity_manager, read_instrument, header_section_optional.value, options, &temporary_allocator);
			return status == ECS_DESERIALIZE_ENTITY_MANAGER_OK;
		});

		return status;
	}

	ECS_DESERIALIZE_ENTITY_MANAGER_STATUS DeserializeEntityManager(
		EntityManager* entity_manager,
		ReadInstrument* read_instrument,
		const DeserializeEntityManagerHeaderSectionData& header_section,
		const DeserializeEntityManagerOptions* options,
		ResizableLinearAllocator* temporary_allocator
	) {
		bool previous_assert_crash = ECS_GLOBAL_ASSERT_CRASH;
		ECS_GLOBAL_ASSERT_CRASH = true;

		// Call the crash handler recovery such that it won't try to crash the entire editor
		ECS_SET_RECOVERY_CRASH_HANDLER(false) {
			ResetRecoveryCrashHandler();
			ECS_GLOBAL_ASSERT_CRASH = previous_assert_crash;
			ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "Unknown error - corrupt data");
			return ECS_DESERIALIZE_ENTITY_MANAGER_FILE_IS_CORRUPT;
		}

		struct Deallocator {
			ECS_INLINE void operator()() {
				ResetRecoveryCrashHandler();
				ECS_GLOBAL_ASSERT_CRASH = assert_crash_value;
			}

			bool assert_crash_value;
		};

		StackScope<Deallocator> scope_deallocator({ previous_assert_crash });

		bool allocation_failure = false;
		auto allocate_and_read_failure = [&](Stream<char> data_section_name) {
			if (options->detailed_error_string) {
				if (allocation_failure) {
					ECS_FORMAT_TEMP_STRING(message, "The data section \"{#}\" does not fit into the buffering", data_section_name);
					options->detailed_error_string->AddStreamAssert(message);
				}
				else {
					ECS_FORMAT_TEMP_STRING(message, "The data section \"{#}\" could not be read from disk", data_section_name);
					options->detailed_error_string->AddStreamAssert(message);
				}
			}
			return allocation_failure ? ECS_DESERIALIZE_ENTITY_MANAGER_TOO_MUCH_DATA : ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
		};

		// Now read the identifier buffer data from the named shared instances
		size_t named_shared_instance_total_count = 0;
		size_t shared_instance_total_count = 0;
		for (size_t index = 0; index < header_section.SharedComponentCount(); index++) {
			named_shared_instance_total_count += header_section.shared_component_pairs[index].named_instance_count;
			shared_instance_total_count += header_section.shared_component_pairs[index].instance_count;
		}

		SharedInstance* registered_shared_instances = (SharedInstance*)read_instrument->ReadOrReferenceDataPointer(
			temporary_allocator,
			shared_instance_total_count * sizeof(SharedInstance) + named_shared_instance_total_count * sizeof(NamedSharedInstanceHeader),
			&allocation_failure
		);
		if (registered_shared_instances == nullptr) {
			return allocate_and_read_failure("Registered shared instances");
		}

		NamedSharedInstanceHeader* named_shared_instance_headers = (NamedSharedInstanceHeader*)OffsetPointer(registered_shared_instances, shared_instance_total_count * sizeof(SharedInstance));

		size_t named_shared_instances_identifier_total_size = 0;
		for (size_t index = 0; index < named_shared_instance_total_count; index++) {
			named_shared_instances_identifier_total_size += named_shared_instance_headers[index].identifier_size;
		}

		void* named_shared_instances_identifiers = read_instrument->ReadOrReferenceDataPointer(
			temporary_allocator,
			named_shared_instances_identifier_total_size,
			&allocation_failure
		);
		if (named_shared_instances_identifiers == nullptr) {
			return allocate_and_read_failure("Named shared instance identifiers");
		}

		// Now the components and the shared components must be created such that when creating the shared components
		// the instances can be added right away

		// Don't use the component byte size in the component pairs
		// That is the byte size for the serialized data, not the one for the new type
		for (size_t index = 0; index < header_section.ComponentCount(); index++) {
			// Modified the behaviour. If a component is not found but there is no instance of it,
			// Allow the deserialization to continue since we can skip the header

			//if (cached_component_infos[index].info == nullptr) {
			//	// Fail, there is a component missing
			//	return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_IS_MISSING;
			//}

			const auto* cached_info = header_section.cached_unique_infos.GetValuePtr(header_section.component_pairs[index].component);
			if (cached_info->info != nullptr) {
				const auto* component_fixup = &cached_info->info->component_fixup;

				// The byte size cannot be 0
				if (component_fixup->component_byte_size == 0) {
					ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "Illegal byte size of 0 for unique component {#} fixup", header_section.GetComponentNameByIteration(index));
					return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_FIXUP_IS_MISSING;
				}

				entity_manager->RegisterComponentCommit(
					header_section.component_pairs[index].component,
					component_fixup->component_byte_size,
					cached_info->info->name,
					&component_fixup->component_functions
				);
			}
		}

		for (size_t index = 0; index < header_section.SharedComponentCount(); index++) {
			// Modified the behaviour. If a component is not found but there is no instance of it,
			// Allow the deserialization to continue since we can skip the header
			const auto* cached_info = header_section.cached_shared_infos.GetValuePtr(header_section.shared_component_pairs[index].component);
			if (cached_info->info == nullptr && (header_section.shared_component_pairs[index].instance_count > 0 ||
				header_section.shared_component_pairs[index].named_instance_count > 0)) {
				if (!options->remove_missing_components) {
					ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "The shared component fixup for {#} is missing", header_section.GetSharedComponentNameByIteration(index));
					return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_IS_MISSING;
				}
			}

			// If the info is still nullptr after this call, it means that the component was removed
			// And never used. We can just skip it
			if (cached_info->info != nullptr) {
				const auto* component_fixup = &cached_info->info->component_fixup;

				// If even after this call the component byte size is still 0, then fail
				if (component_fixup->component_byte_size == 0) {
					ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "Illegal byte size of 0 for shared component {#} fixup", header_section.GetSharedComponentNameByIteration(index));
					return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_FIXUP_IS_MISSING;
				}

				entity_manager->RegisterSharedComponentCommit(
					header_section.shared_component_pairs[index].component,
					component_fixup->component_byte_size,
					cached_info->info->name,
					&component_fixup->component_functions,
					component_fixup->compare_entry
				);
			}
		}

		for (size_t index = 0; index < header_section.GlobalComponentCount(); index++) {
			const auto* cached_info = header_section.cached_global_infos.GetValuePtr(header_section.global_component_pairs[index].component);
			if (cached_info->info == nullptr) {
				if (!options->remove_missing_components) {
					// Fail, there is a component missing
					ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "The global component fixup for {#} is missing", header_section.GetGlobalComponentNameByIteration(index));
					return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_IS_MISSING;
				}
			}

			// We still need to check in case the remove_missing_components flag is set to true
			if (cached_info->info != nullptr) {
				const auto* component_fixup = &cached_info->info->component_fixup;

				// The byte size cannot be 0
				if (component_fixup->component_byte_size == 0) {
					ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "Illegal byte size of 0 for global component {#} fixup", header_section.GetGlobalComponentNameByIteration(index));
					return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_FIXUP_IS_MISSING;
				}

				// We can register the component directly with the new ID - such that we don't need to perform any remapping
				entity_manager->RegisterGlobalComponentCommit(
					cached_info->found_at,
					component_fixup->component_byte_size,
					nullptr,
					cached_info->info->name,
					&component_fixup->component_functions
				);
			}
		}

		// Now the shared component data
		for (size_t index = 0; index < header_section.SharedComponentCount(); index++) {
			bool exists_shared_component = entity_manager->ExistsSharedComponent(header_section.shared_component_pairs[index].component);
			temporary_allocator->SetMarker();

			// Read the header of sizes
			unsigned int* instances_sizes = (unsigned int*)read_instrument->ReadOrReferenceDataPointer(
				temporary_allocator,
				sizeof(unsigned int) * header_section.shared_component_pairs[index].instance_count,
				&allocation_failure
			);
			if (instances_sizes == nullptr) {
				ECS_FORMAT_TEMP_STRING(section_name, "Instance byte sizes for shared component {#}", header_section.GetSharedComponentNameByIteration(index));
				return allocate_and_read_failure(section_name);
			}

			// Determine the size of the all instances such that they can be read in bulk
			unsigned int total_instance_size = 0;
			for (unsigned int subindex = 0; subindex < header_section.shared_component_pairs[index].instance_count; subindex++) {
				total_instance_size += instances_sizes[subindex];
			}

			// Read in bulk now the instance data
			void* instances_data = read_instrument->ReadOrReferenceDataPointer(temporary_allocator, total_instance_size, &allocation_failure);
			if (instances_data == nullptr) {
				ECS_FORMAT_TEMP_STRING(section_name, "Instance byte data for shared component {#}", header_section.GetSharedComponentNameByIteration(index));
				return allocate_and_read_failure(section_name);
			}

			// The steps before that need to be performed no matter if the component is ignored or not

			if (exists_shared_component) {
				// If it has a name, search for its match
				const DeserializeEntityManagerSharedComponentInfo* component_info = header_section.cached_shared_infos.GetValuePtr(header_section.shared_component_pairs[index].component)->info;

				AllocatorPolymorphic component_allocator = entity_manager->GetSharedComponentAllocator(header_section.shared_component_pairs[index].component);
				ECS_STACK_CAPACITY_STREAM(size_t, component_storage, 1024);

				unsigned int current_instance_offset = 0;
				for (unsigned int shared_instance = 0; shared_instance < header_section.shared_component_pairs[index].instance_count; shared_instance++) {
					// TODO: Determine if reading the instances in bulk like we do now is more effective
					// Than relying on the internal buffering of the read instrument.
					// Create an in memory instrument for the data of this instance
					uintptr_t instance_data_ptr = (uintptr_t)OffsetPointer(instances_data, current_instance_offset);
					InMemoryReadInstrument instance_read_instrument(instance_data_ptr, instances_sizes[shared_instance]);

					DeserializeEntityManagerSharedComponentData function_data;
					function_data.data_size = instances_sizes[shared_instance];
					function_data.component = component_storage.buffer;
					function_data.read_instrument = &instance_read_instrument;
					function_data.extra_data = component_info->extra_data;
					function_data.instance = { (short)shared_instance };
					function_data.version = header_section.shared_component_pairs[index].serialize_version;
					function_data.component_allocator = component_allocator;

					// Now call the extract function
					bool is_data_valid = component_info->function(&function_data);
					if (!is_data_valid) {
						ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "A shared instance for component {#} has corrupted data",
							header_section.GetSharedComponentNameByIteration(index));
						return ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID;
					}

					// Commit the shared instance - we don't have to copy the buffers
					// Since the data was read using the component allocator
					entity_manager->RegisterSharedInstanceCommit(header_section.shared_component_pairs[index].component, component_storage.buffer, false);
					current_instance_offset += instances_sizes[shared_instance];
				}
			}

			// The size doesn't need to be updated - the data is commited right away - this will ensure that the 
			// data is always hot in cache
			temporary_allocator->ReturnToMarker();
		}

		// After the instances have been created, we can now bind the named tags to them
		named_shared_instances_identifier_total_size = 0;
		size_t named_shared_instances_count = 0;
		for (size_t index = 0; index < header_section.SharedComponentCount(); index++) {
			unsigned int named_instance_count = header_section.shared_component_pairs[index].named_instance_count;
			// In case the shared component exists in the file but not in the entity manager, we need to ignore
			// Its instances
			bool exists_shared_component = entity_manager->ExistsSharedComponent(header_section.shared_component_pairs[index].component);

			for (unsigned int instance_index = 0; instance_index < named_instance_count; instance_index++) {
				if (exists_shared_component) {
					entity_manager->RegisterNamedSharedInstanceCommit(
						header_section.shared_component_pairs[index].component,
						{ OffsetPointer(named_shared_instances_identifiers, named_shared_instances_identifier_total_size), named_shared_instance_headers[named_shared_instances_count].identifier_size },
						named_shared_instance_headers[named_shared_instances_count].instance
					);
				}

				named_shared_instances_identifier_total_size += named_shared_instance_headers[named_shared_instances_count].identifier_size;
				named_shared_instances_count++;
			}
		}

		// Now we need to read the global components
		for (size_t index = 0; index < header_section.GlobalComponentCount(); index++) {
			bool exists = entity_manager->ExistsGlobalComponent(header_section.global_component_pairs[index].component);

			unsigned int write_size = 0;
			if (!read_instrument->Read(&write_size)) {
				ECS_FORMAT_TEMP_STRING(section, "Byte size for global component {#}", header_section.GetGlobalComponentNameByIteration(index));
				return allocate_and_read_failure(section);
			}

			// The steps beforehand need to be performed no matter if the component is ignored or not		
			if (exists) {
				const auto* cached_info = header_section.cached_global_infos.GetValuePtr(header_section.global_component_pairs[index].component);

				// If it has a name, search for its match
				const DeserializeEntityManagerGlobalComponentInfo* component_info = cached_info->info;

				// Allocate the data from the entity manager allocator
				void* global_data = entity_manager->GetGlobalComponent(cached_info->found_at);

				// Push a new subinstrument, such that the component can reason about itself and we prevent
				// The component from reading overbounds
				ReadInstrument::SubinstrumentData subinstrument_data;
				auto subinstrument = read_instrument->PushSubinstrument(&subinstrument_data, write_size);

				DeserializeEntityManagerComponentData function_data;
				function_data.components = global_data;
				function_data.read_instrument = read_instrument;
				function_data.extra_data = component_info->extra_data;
				function_data.version = header_section.global_component_pairs[index].serialize_version;
				// Global components have their allocator as the main entity manager allocator, since they
				// Might need a lot of memory
				function_data.component_allocator = entity_manager->MainAllocator();
				function_data.count = 1;

				// Now call the extract function
				bool is_data_valid = component_info->function(&function_data);
				if (!is_data_valid) {
					if (options->detailed_error_string) {
						ECS_FORMAT_TEMP_STRING(message, "The global component {#} has corrupted data", header_section.GetGlobalComponentNameByIteration(index));
						options->detailed_error_string->AddStreamAssert(message);
					}
					return ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID;
				}
			}
		}

		ArchetypeHeader* archetype_headers = (ArchetypeHeader*)read_instrument->ReadOrReferenceDataPointer(
			temporary_allocator,
			sizeof(ArchetypeHeader) * header_section.header.archetype_count,
			&allocation_failure
		);
		if (archetype_headers == nullptr) {
			return allocate_and_read_failure("Archetype Headers");
		}

		// Now calculate how much is needed to read the shared instances and the base archetype sizes
		unsigned int base_archetypes_counts_size = 0;
		unsigned int base_archetypes_instance_size = 0;
		unsigned int archetypes_component_signature_size = 0;
		for (unsigned int index = 0; index < header_section.header.archetype_count; index++) {
			// Archetype shared and unique components
			archetypes_component_signature_size += sizeof(Component) * (archetype_headers[index].shared_count + archetype_headers[index].unique_count);

			// The base archetype shared instances
			base_archetypes_instance_size += sizeof(SharedInstance) * archetype_headers[index].shared_count * archetype_headers[index].base_count;
			// The base archetype sizes
			base_archetypes_counts_size += sizeof(unsigned int) * archetype_headers[index].base_count;
		}

		Component* archetype_component_signatures = (Component*)read_instrument->ReadOrReferenceDataPointer(
			temporary_allocator,
			archetypes_component_signature_size + base_archetypes_instance_size + base_archetypes_counts_size,
			&allocation_failure
		);
		if (archetype_component_signatures == nullptr) {
			return allocate_and_read_failure("Archetype Signatures and Counts");
		}
		SharedInstance* base_archetypes_instances = (SharedInstance*)OffsetPointer(archetype_component_signatures, archetypes_component_signature_size);
		unsigned int* base_archetypes_sizes = (unsigned int*)OffsetPointer(base_archetypes_instances, base_archetypes_instance_size);

		// This is used to map from the header index to the actual entity manager index
		unsigned int* archetype_mappings = (unsigned int*)temporary_allocator->Allocate(sizeof(unsigned int) * header_section.header.archetype_count);
		// This is used to map from the archetype base index to the actual archetype base index (since it can be found
		// when a component was removed)
		unsigned int* archetype_base_mappings = (unsigned int*)temporary_allocator->Allocate(base_archetypes_counts_size);
		// This is used to remap the entities stream index when some entities are moved to another base archetype when there are
		// Missing components. We record the base size when that base is found such that we can offset the stream index with that value
		unsigned int* archetype_base_count_mappings = (unsigned int*)temporary_allocator->Allocate(base_archetypes_counts_size);

		unsigned int component_signature_offset = 0;
		unsigned int base_archetype_instances_offset = 0;
		unsigned int base_archetype_sizes_offset = 0;
		for (unsigned int index = 0; index < header_section.header.archetype_count; index++) {
			// Create the archetypes
			ComponentSignature unique;
			unique.indices = archetype_component_signatures + component_signature_offset;
			unique.count = archetype_headers[index].unique_count;

			bool missing_shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
			memset(missing_shared_components, false, sizeof(missing_shared_components));
			ComponentSignature shared;
			shared.indices = unique.indices + unique.count;
			shared.count = archetype_headers[index].shared_count;

			unsigned char original_unique_count = unique.count;
			unsigned char original_shared_count = shared.count;

			// Check to see if one of the unique components is missing
			for (unsigned char component_index = 0; component_index < unique.count; component_index++) {
				if (!entity_manager->ExistsComponent(unique.indices[component_index])) {
					unique.count--;
					unique.indices[component_index] = unique.indices[unique.count];
					component_index--;
				}
			}

			unsigned char missing_index = 0;
			for (unsigned char shared_index = 0; shared_index < shared.count; shared_index++) {
				if (!entity_manager->ExistsSharedComponent(shared.indices[shared_index])) {
					missing_shared_components[missing_index] = true;
					shared.count--;
					shared.indices[shared_index] = shared.indices[shared.count];
					shared_index--;
				}
				missing_index++;
			}

			if (unique.count != original_unique_count || shared.count != original_shared_count) {
				if (!options->remove_missing_components) {
					// Fail
					ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "An archetype has missing unique components");
					return ECS_DESERIALIZE_ENTITY_MANAGER_COMPONENT_IS_MISSING;
				}
			}

			// In order to handle the case when a component is removed, use the FindOrCreate functions
			unsigned int archetype_index = entity_manager->FindOrCreateArchetype(unique, shared);
			// Need to fill in the mapping
			archetype_mappings[index] = archetype_index;

			SharedComponentSignature shared_signature;
			shared_signature.indices = shared.indices;
			shared_signature.count = shared.count;
			for (unsigned int base_index = 0; base_index < archetype_headers[index].base_count; base_index++) {
				shared_signature.instances = base_archetypes_instances + base_archetype_instances_offset;
				// We need to shuffle the instances according to the order the components were shuffled
				unsigned char current_shared_count = original_shared_count;
				for (unsigned char shared_index = 0; shared_index < current_shared_count; shared_index++) {
					if (missing_shared_components[shared_index]) {
						current_shared_count--;
						shared_signature.instances[shared_index] = shared_signature.instances[current_shared_count];
						shared_index--;
					}
				}

				// Find/Create the base archetype
				unsigned int existing_base_index = entity_manager->GetArchetype(archetype_index)->FindBaseIndex(shared_signature);
				if (existing_base_index != -1) {
					// In case it exists, we need to reserve new space for the entities
					ArchetypeBase* base = entity_manager->GetBase(archetype_index, existing_base_index);
					base->Reserve(base_archetypes_sizes[base_archetype_sizes_offset]);
				}
				else {
					existing_base_index = entity_manager->CreateArchetypeBaseCommit(archetype_index, shared_signature, base_archetypes_sizes[base_archetype_sizes_offset]);
				}
				// Fill in the mapping
				archetype_base_mappings[base_archetype_sizes_offset] = existing_base_index;

				ArchetypeBase* base = entity_manager->GetBase(archetype_index, existing_base_index);
				// We need to record the base size before incrementing with the current value
				archetype_base_count_mappings[base_archetype_sizes_offset] = base->m_size;
				// Set the size of the base now - it's going to be used again when deserializing
				base->m_size += base_archetypes_sizes[base_archetype_sizes_offset];

				// Bump forward the shared indices
				base_archetype_instances_offset += original_shared_count;
				base_archetype_sizes_offset++;
			}
			// Bump forward the component offset
			component_signature_offset += original_unique_count + original_shared_count;
		}

		// The buffering cannot be bumped back, the component pairs are still needed

		component_signature_offset = 0;
		base_archetype_sizes_offset = 0;
		// Only the component data of the base archetypes remains to be restored, the entities must be deduces from the entity pool
		for (unsigned int index = 0; index < header_section.header.archetype_count; index++) {
			Archetype* archetype = entity_manager->GetArchetype(archetype_mappings[index]);

			// We need the unique signature of the current archetype - but we also need the signature for the original
			// Archetype from the file in order to skip the components that are missing
			ComponentSignature unique = archetype->GetUniqueSignature();

			ComponentSignature original_unique_signature = { archetype_component_signatures + component_signature_offset, archetype_headers[index].unique_count };

			// The order of the loops cannot be interchanged - the serialized data is written for each base together
			for (unsigned int base_index = 0; base_index < archetype_headers[index].base_count; base_index++) {
				ArchetypeBase* base = archetype->GetBase(archetype_base_mappings[base_archetype_sizes_offset]);
				for (unsigned int component_index = 0; component_index < original_unique_signature.count; component_index++) {
					unsigned char current_unique_index = unique.Find(original_unique_signature[component_index]);

					Component current_component;
					const CachedComponentInfo* component_info;
					if (current_unique_index != UCHAR_MAX) {
						current_component = unique.indices[current_unique_index];
						component_info = header_section.cached_unique_infos.GetValuePtr(current_component);
					}

					// Use the buffering to read in. Read the write count first
					unsigned int data_size = 0;
					if (!read_instrument->Read(&data_size)) {
						ECS_FORMAT_TEMP_STRING(section, "Data size for component {#} while reading an archetype could not be read", component_info->info->name);
						return allocate_and_read_failure(section);
					}

					if (current_unique_index != UCHAR_MAX) {
						// Now call the extract function for each component
						void* archetype_buffer = base->m_buffers[current_unique_index];
						unsigned int component_size = entity_manager->m_unique_components[current_component.value].size;

						DeserializeEntityManagerComponentData function_data;
						function_data.count = base->m_size;
						function_data.extra_data = component_info->info->extra_data;
						function_data.read_instrument = read_instrument;
						function_data.components = archetype_buffer;
						function_data.version = component_info->version;
						function_data.component_allocator = entity_manager->GetComponentAllocator(current_component);

						bool is_data_valid = component_info->info->function(&function_data);
						if (!is_data_valid) {
							ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "Data for component {#} for an archetype is corrupted", component_info->info->name);
							return ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID;
						}
					}
				}

				base_archetype_sizes_offset++;
			}

			component_signature_offset += archetype_headers[index].unique_count + archetype_headers[index].shared_count;
		}

		// We need to record the signatures because when updating components we need to match the old signatures
		// We can't place the archetype signatures on the stack because of the alignment bug of the stack trace
		// Use the entity manager allocator for this
		size_t vector_signature_size = sizeof(VectorComponentSignature) * entity_manager->m_archetypes.size * 2;
		VectorComponentSignature* old_archetype_signatures = (VectorComponentSignature*)entity_manager->m_memory_manager->Allocate(vector_signature_size);
		memcpy(old_archetype_signatures, entity_manager->m_archetype_vector_signatures, vector_signature_size);

#pragma region Remap components

		bool* component_exists = (bool*)entity_manager->m_memory_manager->Allocate(sizeof(bool) * entity_manager->m_unique_components.size);
		ComponentInfo* before_remapping_component_infos = (ComponentInfo*)entity_manager->m_memory_manager->Allocate(sizeof(ComponentInfo) * entity_manager->m_unique_components.size);

		/*
			AWESOME BUG. So having any instance of a VectorComponentSignature on the "stack" (basically in the registers)
			will result in a failure when using longjmp to exit out of the function. Why that is, I have no clue. There is
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
				before_remapping_component_infos[index] = entity_manager->m_unique_components[index];
				component_exists[index] = true;
			}
			else {
				component_exists[index] = false;
			}
		}

		for (size_t index = 0; index < unique_component_count; index++) {
			if (component_exists[index]) {
				Component current_component = { (short)index };
				const CachedComponentInfo* cached_info = header_section.cached_unique_infos.GetValuePtr(current_component);
				if (cached_info->found_at != current_component) {
					entity_manager->ChangeComponentIndex(current_component, cached_info->found_at);
				}
			}
		}

		// Deallocate the allocations
		entity_manager->m_memory_manager->Deallocate(component_exists);
		entity_manager->m_memory_manager->Deallocate(before_remapping_component_infos);

#pragma endregion

#pragma region Remap shared instances

		// We need to remap the shared instances before remapping the shared components since it will cause
		// A crash since some components won't match
		unsigned int* remap_instance_offsets = (unsigned int*)entity_manager->m_memory_manager->Allocate(sizeof(unsigned int) * (header_section.SharedComponentCount() + 1));
		Component* remap_components = (Component*)entity_manager->m_memory_manager->Allocate(sizeof(Component) * header_section.SharedComponentCount());
		DeserializeEntityManagerInitializeRemappSharedInstances(header_section.shared_component_pairs.buffer, remap_components, remap_instance_offsets, header_section.SharedComponentCount());
		unsigned int archetype_count = entity_manager->GetArchetypeCount();
		for (unsigned int index = 0; index < archetype_count; index++) {
			DeserializeEntityManagerRemappSharedInstances(
				remap_components,
				registered_shared_instances,
				remap_instance_offsets,
				header_section.SharedComponentCount(),
				entity_manager->GetArchetype(index)
			);
		}

		entity_manager->m_memory_manager->Deallocate(remap_instance_offsets);
		entity_manager->m_memory_manager->Deallocate(remap_components);

#pragma endregion

#pragma region Remap Shared Components

		bool* shared_component_exists = (bool*)entity_manager->m_memory_manager->Allocate(sizeof(bool) * entity_manager->m_shared_components.size);

		// Now determine if a component has changed the ID and broadcast that change to the archetypes as well
		// We need to store this component count since new components might be remapped and change the size
		size_t shared_component_count = entity_manager->m_shared_components.size;
		for (size_t index = 0; index < shared_component_count; index++) {
			Component component = { (short)index };
			if (entity_manager->ExistsSharedComponent(component)) {
				before_remapping_component_infos[index] = entity_manager->m_shared_components[index].info;
				shared_component_exists[index] = true;
			}
			else {
				shared_component_exists[index] = false;
			}
		}

		for (size_t index = 0; index < shared_component_count; index++) {
			if (component_exists[index]) {
				Component current_component = { (short)index };
				const CachedSharedComponentInfo* cached_info = header_section.cached_shared_infos.GetValuePtr(current_component);

				ECS_ASSERT(cached_info->found_at.value != -1);
				short new_id = cached_info->found_at;
				if (new_id != index) {
					entity_manager->ChangeSharedComponentIndex(index, new_id);
				}
			}
		}

		// Make the deallocations
		entity_manager->m_memory_manager->Deallocate(shared_component_exists);
		entity_manager->m_memory_manager->Deallocate(old_archetype_signatures);

#pragma endregion

		// The base archetypes are missing their entities, but these will be restored after reading the entity pool,
		// which can be read now
		if (!DeserializeEntityPool(entity_manager->m_entity_pool, read_instrument)) {
			ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "Failed to deserialize the entity pool");
			return ECS_DESERIALIZE_ENTITY_MANAGER_DATA_IS_INVALID;
		}

		// Construct a helper array with the offsets of the base mappings such that they can be easily referenced by the
		// Loop down bellow
		unsigned int* archetype_base_mappings_offsets = (unsigned int*)temporary_allocator->Allocate(sizeof(unsigned int) * header_section.header.archetype_count);
		if (header_section.header.archetype_count > 0) {
			archetype_base_mappings_offsets[0] = 0;
			for (unsigned int index = 1; index < header_section.header.archetype_count; index++) {
				archetype_base_mappings_offsets[index] = archetype_base_mappings_offsets[index - 1] + archetype_headers[index - 1].base_count;
			}
		}

		// Walk through the list of entities and write them inside the base archetypes
		for (unsigned int pool_index = 0; pool_index < entity_manager->m_entity_pool->m_entity_infos.size; pool_index++) {
			if (entity_manager->m_entity_pool->m_entity_infos[pool_index].is_in_use) {
				entity_manager->m_entity_pool->m_entity_infos[pool_index].stream.ForEachIndex([&](unsigned int entity_index) {
					// Construct the entity from the entity info
					EntityInfo* info = &entity_manager->m_entity_pool->m_entity_infos[pool_index].stream[entity_index];
					Entity entity = entity_manager->m_entity_pool->GetEntityFromPosition(pool_index, entity_index);
					// Set the entity generation count the same as in the info
					entity.generation_count = info->generation_count;

					// Now set the according archetype's entity inside the entities buffer

					unsigned int main_archetype = info->main_archetype;
					unsigned int base_archetype = info->base_archetype;
					// We need to remap the entities here since the archetypes might have changed when missing components
					// Are allowed
					if (options->remove_missing_components) {
						// First do the stream index, then the base index and at last the main archetype in this order
						// Since the calculation of them is dependent on those other values
						info->stream_index += archetype_base_count_mappings[archetype_base_mappings_offsets[main_archetype] + base_archetype];
						info->base_archetype = archetype_base_mappings[archetype_base_mappings_offsets[main_archetype] + base_archetype];
						info->main_archetype = archetype_mappings[main_archetype];

						base_archetype = info->base_archetype;
						main_archetype = info->main_archetype;
					}
					Archetype* archetype = entity_manager->GetArchetype(main_archetype);
					ArchetypeBase* base = archetype->GetBase(base_archetype);
					base->m_entities[info->stream_index] = entity;
				});
			}
		}

		entity_manager->m_hierarchy_allocator->Clear();
		entity_manager->m_hierarchy = EntityHierarchy(entity_manager->m_hierarchy_allocator);
		if (!DeserializeEntityHierarchy(&entity_manager->m_hierarchy, read_instrument)) {
			ECS_FORMAT_ERROR_MESSAGE(options->detailed_error_string, "Failed to deserialize the entity hierarchy");
			return ECS_DESERIALIZE_ENTITY_MANAGER_FAILED_TO_READ;
		}

		// Everything should be reconstructed now
		return ECS_DESERIALIZE_ENTITY_MANAGER_OK;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

#pragma region Standard Functors (non link components)

	// ------------------------------------------------------------------------------------------------------------------------------------------

	struct ReflectionSerializeComponentData {
		const Reflection::ReflectionManager* reflection_manager;
		const Reflection::ReflectionType* type;
		bool is_blittable;
	};

	bool ReflectionSerializeEntityManagerComponent(SerializeEntityManagerComponentData* data) {
		ReflectionSerializeComponentData* functor_data = (ReflectionSerializeComponentData*)data->extra_data;
		
		size_t type_byte_size = Reflection::GetReflectionTypeByteSize(functor_data->type);
		if (functor_data->is_blittable) {
			size_t write_size = type_byte_size * data->count;
			return data->write_instrument->Write(data->components, write_size);
		}
		else {
			SerializeOptions options;
			options.write_type_table = false;

			const void* current_component = data->components;
			for (unsigned int index = 0; index < data->count; index++) {
				if (Serialize(functor_data->reflection_manager, functor_data->type, current_component, data->write_instrument, &options) != ECS_SERIALIZE_OK) {
					return false;
				}

				current_component = OffsetPointer(current_component, type_byte_size);
			}

			return true;
		}
	}

	bool ReflectionSerializeEntityManagerHeaderComponent(SerializeEntityManagerHeaderComponentData* data) {
		ReflectionSerializeComponentData* functor_data = (ReflectionSerializeComponentData*)data->extra_data;
		// Write the type table
		functor_data->is_blittable = IsBlittable(functor_data->type);
		return SerializeFieldTable(functor_data->reflection_manager, functor_data->type, data->write_instrument);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	typedef ReflectionSerializeComponentData ReflectionSerializeSharedComponentData;

	static_assert(sizeof(ReflectionSerializeSharedComponentData) == sizeof(ReflectionSerializeComponentData),
		"Both reflection serialize structures must be the same");

	unsigned int ReflectionSerializeEntityManagerSharedComponent(SerializeEntityManagerSharedComponentData* data) {
		// The same as the unique version, just the count is 1. The extra data is type punned to the unique data
		SerializeEntityManagerComponentData unique_data;
		unique_data.write_instrument = data->write_instrument;
		unique_data.components = data->component_data;
		unique_data.count = 1;
		unique_data.extra_data = data->extra_data;
		return ReflectionSerializeEntityManagerComponent(&unique_data);
	}

	unsigned int ReflectionSerializeEntityManagerHeaderSharedComponent(SerializeEntityManagerHeaderSharedComponentData* data) {
		// At the moment can call the unique componet variant
		SerializeEntityManagerHeaderComponentData unique_data;
		unique_data.write_instrument = data->write_instrument;
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
		// We need this flag because we need a bit of special handling
		// For the blittable case
		bool is_file_blittable;
		// This boolean is set to true by global components, which can have a type
		// Allocator embedded in them, and we want to initialize it, else it's false
		bool initialize_type_allocator;
		// This is used only by the is_file_blittable case
		size_t file_byte_size;
	};

	bool ReflectionDeserializeEntityManagerComponent(DeserializeEntityManagerComponentData* data) {
		ReflectionDeserializeComponentData* functor_data = (ReflectionDeserializeComponentData*)data->extra_data;

		size_t type_byte_size = Reflection::GetReflectionTypeByteSize(functor_data->type);
		if (functor_data->is_unchanged_and_blittable) {
			return data->read_instrument->Read(data->components, data->count * type_byte_size);
		}
		else {
			// Must use the deserialize to take care of the data that can be read
			DeserializeOptions options;
			options.read_type_table = false;
			options.field_table = &functor_data->field_table;
			options.verify_dependent_types = false;
			options.field_allocator = data->component_allocator;
			options.default_initialize_missing_fields = true;
			if (functor_data->initialize_type_allocator) {
				options.initialize_type_allocators = true;
				options.use_type_field_allocators = true;
			}

			void* current_component = data->components;

			// Hoist the if outside the for
			if (functor_data->is_file_blittable) {
				// We need this separate case for the following reason
				// Take this structure for example
				// struct {
				//   unsigned int a;
				//	 unsigned short b;
				// }
				// It has a total byte size of 8, but if it were
				// To be serialized using Serialize, it will write
				// Only 6 bytes into the stream. If we are to use
				// the bulk blittable with byte size of 8, then if
				// We are to use the deserialize, it will be skipping
				// Only 6 bytes per entry, instead of the full 8. So
				// We need to account for this fact.
				// Perform a separate single iteration to determine how many bytes
				// A single iteration uses, such that we know how much to ignore per iteration
				if (data->count == 0) {
					return true;
				}

				size_t initial_instrument_offset = data->read_instrument->GetOffset();
				if (Deserialize(functor_data->reflection_manager, functor_data->type, current_component, data->read_instrument, &options) != ECS_DESERIALIZE_OK) {
					return false;
				}
				size_t final_instrument_offset = data->read_instrument->GetOffset();
				size_t per_iteration_read_size = final_instrument_offset - initial_instrument_offset;
				ECS_ASSERT(per_iteration_read_size <= type_byte_size, "Unexpected code path - component file blittable structure during deserialization "
					"used more bytes than in file!");

				size_t per_iteration_ignore_size = type_byte_size - per_iteration_read_size;
				if (!data->read_instrument->Ignore(per_iteration_ignore_size)) {
					return false;
				}
				for (unsigned int index = 0; index < data->count - 1; index++) {
					bool success = true;
					success &= Deserialize(functor_data->reflection_manager, functor_data->type, current_component, data->read_instrument, &options) != ECS_DESERIALIZE_OK;
					success &= data->read_instrument->Ignore(per_iteration_ignore_size);
					if (!success) {
						return false;
					}

					current_component = OffsetPointer(current_component, type_byte_size);
				}
			}
			else {
				for (unsigned int index = 0; index < data->count; index++) {
					if (Deserialize(functor_data->reflection_manager, functor_data->type, current_component, data->read_instrument, &options) != ECS_DESERIALIZE_OK) {
						return false;
					}

					current_component = OffsetPointer(current_component, type_byte_size);
				}
			}
		}

		return true;
	}

	bool ReflectionDeserializeEntityManagerHeaderComponent(DeserializeEntityManagerHeaderComponentData* data) {
		ReflectionDeserializeComponentData* functor_data = (ReflectionDeserializeComponentData*)data->extra_data;

		// Extract the field table
		Optional<DeserializeFieldTable> field_table_optional = DeserializeFieldTableFromData(data->read_instrument, functor_data->allocator);
		if (!field_table_optional.has_value) {
			return false;
		}
		functor_data->field_table = field_table_optional.value;

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
		functor_data->is_file_blittable = functor_data->field_table.IsBlittable(type_index);
		functor_data->file_byte_size = functor_data->field_table.TypeByteSize(type_index);
		// This is a special case for the global component, where we need to initialize the type allocator
		functor_data->initialize_type_allocator = GetReflectionTypeComponentType(functor_data->type) == ECS_COMPONENT_GLOBAL;

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
		unique_data.read_instrument = data->read_instrument;
		unique_data.version = data->version;
		unique_data.component_allocator = data->component_allocator;
		return ReflectionDeserializeEntityManagerComponent(&unique_data);
	}

	bool ReflectionDeserializeEntityManagerHeaderSharedComponent(DeserializeEntityManagerHeaderSharedComponentData* data) {
		// Same as the unique header. Could type pun the data ptr to a unique ptr, but lets be safe and type pun only the 
		// ReflectionDeserializeSharedComponentData into a ReflectionDeserializeComponentData
		DeserializeEntityManagerHeaderComponentData unique_data;
		unique_data.extra_data = data->extra_data;
		unique_data.read_instrument = data->read_instrument;
		unique_data.size = data->size;
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
		// Set the count to 1, such that the normal component serialize will write a single component at a time
		data->count = 1;

		ConvertToAndFromLinkBaseData convert_base_data;
		convert_base_data.link_type = functor_data->normal_base_data.type;
		convert_base_data.target_type = functor_data->link_base_data.target_type;
		convert_base_data.asset_fields = functor_data->link_base_data.asset_fields;
		convert_base_data.asset_database = functor_data->link_base_data.asset_database;
		// We need both functions otherwise the function fails
		convert_base_data.module_link = { functor_data->link_base_data.function, functor_data->link_base_data.reverse_function };
		convert_base_data.reflection_manager = functor_data->normal_base_data.reflection_manager;

		const void* initial_components = data->components;

		for (unsigned int index = 0; index < initial_count; index++) {
			const void* source = OffsetPointer(initial_components, index * target_byte_size);

			bool success = ConvertFromTargetToLinkComponent(
				&convert_base_data,
				source,
				link_component_storage,
				nullptr,
				nullptr
			);
			if (!success) {
				return false;
			}

			data->extra_data = &functor_data->normal_base_data;
			data->components = link_component_storage;
			if (!ReflectionSerializeEntityManagerComponent(data)) {
				return false;
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	unsigned int ReflectionSerializeEntityManagerHeaderLinkComponent(SerializeEntityManagerHeaderComponentData* data) {
		// Just forward to the normal call
		ReflectionSerializeLinkComponentData* functor_data = (ReflectionSerializeLinkComponentData*)data->extra_data;
		data->extra_data = &functor_data->normal_base_data;
		return ReflectionSerializeEntityManagerHeaderComponent(data);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	typedef ReflectionSerializeLinkComponentData ReflectionSerializeLinkSharedComponentData;

	unsigned int ReflectionSerializeEntityManagerLinkSharedComponent(SerializeEntityManagerSharedComponentData* data) {
		ReflectionSerializeLinkComponentData* functor_data = (ReflectionSerializeLinkComponentData*)data->extra_data;
		size_t link_component_storage[ECS_KB];

		ConvertToAndFromLinkBaseData convert_base_data;
		convert_base_data.link_type = functor_data->normal_base_data.type;
		convert_base_data.target_type = functor_data->link_base_data.target_type;
		convert_base_data.asset_fields = functor_data->link_base_data.asset_fields;
		convert_base_data.asset_database = functor_data->link_base_data.asset_database;
		// We need both functions otherwise the function fails
		convert_base_data.module_link = { functor_data->link_base_data.function, functor_data->link_base_data.reverse_function };
		convert_base_data.reflection_manager = functor_data->normal_base_data.reflection_manager;

		bool success = ConvertFromTargetToLinkComponent(
			&convert_base_data,
			data->component_data,
			link_component_storage,
			nullptr,
			nullptr
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
		// We need both functions otherwise the function fails
		convert_base_data.module_link = { functor_data->link_base_data.function, functor_data->link_base_data.reverse_function };
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
				OffsetPointer(initial_component, index * target_byte_size),
				nullptr,
				nullptr,
				false
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
		// We need both functions otherwise the function fails
		convert_base_data.module_link = { functor_data->link_base_data.function, functor_data->link_base_data.reverse_function };
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
			initial_component,
			nullptr,
			nullptr,
			false
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
	static void CreateSerializeDeserializeEntityManagerComponentTable(
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
		total_count = PowerOfTwoGreater(total_count);

		if (table.GetCount() == 0) {
			table.Initialize(allocator, total_count);
		}
		else {
			void* new_buffer = Allocate(allocator, table.MemoryOf(total_count));
			const void* old_buffer = table.Resize(new_buffer, total_count);
			Deallocate(allocator, old_buffer);
		}

		// Now can start adding the types
		// Hoist the if check for the override outside the loop
		if (overrides.size > 0) {
			for (unsigned int index = 0; index < type_indices.size; index++) {
				const Reflection::ReflectionType* type = reflection_manager->GetType(type_indices[index]);
				if (GetReflectionTypeComponentType(type) != ECS_COMPONENT_TYPE_COUNT) {
					// Check to see if it is overwritten
					if (override_components != nullptr) {
						// Check the component
						Component type_component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
						if (SearchBytes(override_components, overrides.size, type_component.value) == -1) {
							// There is no override
							functor(type);
						}
					}
					else {
						unsigned int overwrite_index = 0;
						for (; overwrite_index < overrides.size; overwrite_index++) {
							if (overrides[overwrite_index].name == type->name) {
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
				if (GetReflectionTypeComponentType(type) != ECS_COMPONENT_TYPE_COUNT) {
					functor(type);
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	template<typename Table, typename SerializeInfo, typename CheckFunctor>
	static void CreateSerializeEntityManagerComponentTableImpl(
		Table& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<SerializeInfo> overrides,
		Component* override_components,
		Stream<unsigned int> hierarchy_indices,
		void* serialize_function,
		void* serialize_header_function,
		CheckFunctor&& check_functor
	) {
		CreateSerializeDeserializeEntityManagerComponentTable(table, reflection_manager, allocator, overrides, override_components, hierarchy_indices, [&](const Reflection::ReflectionType* type) {
			if (check_functor(type)) {
				SerializeInfo info;
				info.function = (decltype(info.function))serialize_function;
				info.header_function = (decltype(info.header_function))serialize_header_function;
				ReflectionSerializeComponentData* data = (ReflectionSerializeComponentData*)Allocate(allocator, sizeof(ReflectionSerializeSharedComponentData));
				data->reflection_manager = reflection_manager;
				data->type = type;

				info.extra_data = data;
				info.name = type->name;
				// The version is not really needed
				info.version = 0;

				Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };

				table.Insert(info, component);
			}
		});
	}

	template<typename OverrideType>
	static void ConvertLinkTypesToSerializeEntityManagerImpl(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		OverrideType* overrides,
		void* serialize_function,
		void* serialize_header_function
	) {
		for (size_t index = 0; index < link_types.size; index++) {
			Stream<char> target = GetReflectionTypeLinkComponentTarget(link_types[index]);
			const Reflection::ReflectionType* target_type = reflection_manager->GetType(target);

			OverrideType info;
			info.function = (decltype(info.function))serialize_function;
			info.header_function = (decltype(info.header_function))serialize_header_function;
			ReflectionSerializeLinkComponentData* data = (ReflectionSerializeLinkComponentData*)Allocate(allocator, sizeof(ReflectionSerializeLinkComponentData));

			ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromLinkComponent(link_types[index], asset_fields);

			data->normal_base_data.reflection_manager = reflection_manager;
			data->normal_base_data.type = link_types[index];
			data->link_base_data.target_type = target_type;
			data->link_base_data.asset_database = database;
			data->link_base_data.reverse_function = module_links[index].reverse_function;
			data->link_base_data.function = module_links[index].build_function;
			data->link_base_data.asset_fields.InitializeAndCopy(allocator, asset_fields);

			info.extra_data = data;
			info.name = target_type->name;
			// The version is not really needed
			info.version = 0;

			overrides[index] = info;
		}
	}

	void ConvertLinkTypesToSerializeEntityManagerUnique(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator, 
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		SerializeEntityManagerComponentInfo* overrides
	)
	{		
		ConvertLinkTypesToSerializeEntityManagerImpl(
			reflection_manager,
			database,
			allocator,
			link_types,
			module_links,
			overrides,
			ReflectionSerializeEntityManagerLinkComponent,
			ReflectionSerializeEntityManagerHeaderLinkComponent
		);
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
		CreateSerializeEntityManagerComponentTableImpl(
			table,
			reflection_manager,
			allocator,
			overrides,
			override_components,
			hierarchy_indices,
			ReflectionSerializeEntityManagerComponent,
			ReflectionSerializeEntityManagerHeaderComponent,
			[](const Reflection::ReflectionType* type) {
				return IsReflectionTypeComponent(type);
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	template<typename TableType, typename InfoType>
	static void AddEntityManagerComponentTableOverridesImpl(TableType& table, const Reflection::ReflectionManager* reflection_manager, Stream<InfoType> overrides) {
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
			table.Insert(overrides[index], component);
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
		ConvertLinkTypesToSerializeEntityManagerImpl(
			reflection_manager,
			asset_database,
			allocator,
			link_types,
			module_links,
			overrides,
			ReflectionSerializeEntityManagerLinkSharedComponent,
			ReflectionSerializeEntityManagerHeaderLinkSharedComponent
		);
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
		CreateSerializeEntityManagerComponentTableImpl(
			table,
			reflection_manager,
			allocator,
			overrides,
			override_components,
			hierarchy_indices,
			ReflectionSerializeEntityManagerSharedComponent,
			ReflectionSerializeEntityManagerHeaderSharedComponent,
			[](const Reflection::ReflectionType* type) {
				return IsReflectionTypeSharedComponent(type);
			}
		);
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

	void ConvertLinkTypesToSerializeEntityManagerGlobal(
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database, 
		AllocatorPolymorphic allocator, 
		Stream<const Reflection::ReflectionType*> link_types, 
		Stream<ModuleLinkComponentTarget> module_links, 
		SerializeEntityManagerGlobalComponentInfo* overrides
	)
	{
		ConvertLinkTypesToSerializeEntityManagerImpl(
			reflection_manager,
			database,
			allocator,
			link_types,
			module_links,
			overrides,
			ReflectionSerializeEntityManagerLinkComponent,
			ReflectionSerializeEntityManagerHeaderLinkComponent
		);
	}

	void CreateSerializeEntityManagerGlobalComponentTable(
		SerializeEntityManagerGlobalComponentTable& table, 
		const Reflection::ReflectionManager* reflection_manager, 
		AllocatorPolymorphic allocator, 
		Stream<SerializeEntityManagerGlobalComponentInfo> overrides, 
		Component* override_components, 
		Stream<unsigned int> hierarchy_indices
	)
	{
		CreateSerializeEntityManagerComponentTableImpl(
			table,
			reflection_manager,
			allocator,
			overrides,
			override_components,
			hierarchy_indices,
			ReflectionSerializeEntityManagerComponent,
			ReflectionSerializeEntityManagerHeaderComponent,
			[](const Reflection::ReflectionType* type) {
				return IsReflectionTypeGlobalComponent(type);
			}
		);
	}

	void AddSerializeEntityManagerGlobalComponentTableOverrides(
		SerializeEntityManagerGlobalComponentTable& table, 
		const Reflection::ReflectionManager* reflection_manager, 
		Stream<SerializeEntityManagerGlobalComponentInfo> overrides
	)
	{
		AddEntityManagerComponentTableOverridesImpl(table, reflection_manager, overrides);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	// Receive the functions as typeless void* and cast them inside
	template<typename OverrideType>
	static void ConvertLinkTypesToDeserializeEntityManagerImpl(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		Stream<ModuleComponentFunctions> module_component_functions,
		OverrideType* overrides,
		void* deserialize_function,
		void* deserialize_header_function
	) {
		static_assert(sizeof(ReflectionDeserializeLinkComponentData) == sizeof(ReflectionDeserializeLinkSharedComponentData),
			"Both deserialize structures must be the same or change the algorithm");
		static_assert(offsetof(ReflectionDeserializeLinkComponentData, normal_base_data) == offsetof(ReflectionDeserializeLinkSharedComponentData, normal_base_data),
			"Both deserialize structures must be the same or change the algorithm");
		static_assert(offsetof(ReflectionDeserializeLinkComponentData, link_base_data) == offsetof(ReflectionDeserializeLinkSharedComponentData, link_base_data),
			"Both deserialize structures must be the same or change the algorithm");

		for (size_t index = 0; index < link_types.size; index++) {
			Stream<char> target = GetReflectionTypeLinkComponentTarget(link_types[index]);
			const Reflection::ReflectionType* target_type = reflection_manager->GetType(target);

			OverrideType info;
			info.function = (decltype(info.function))deserialize_function;
			info.header_function = (decltype(info.header_function))deserialize_header_function;

			ReflectionDeserializeLinkComponentData* data = (ReflectionDeserializeLinkComponentData*)Allocate(allocator, sizeof(ReflectionDeserializeLinkComponentData));
			data->normal_base_data.reflection_manager = reflection_manager;
			data->normal_base_data.type = link_types[index];
			data->normal_base_data.allocator = allocator;

			ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromLinkComponent(link_types[index], asset_fields);

			data->link_base_data.asset_database = database;
			data->link_base_data.asset_fields.InitializeAndCopy(allocator, asset_fields);
			// Put both functions - even tho only the build is actually needed
			// A function inside the link check verifies that both are set
			data->link_base_data.function = module_links[index].build_function;
			data->link_base_data.reverse_function = module_links[index].reverse_function;
			data->link_base_data.target_type = target_type;

			info.extra_data = data;
			info.name = target_type->name;
			info.component_fixup.component_byte_size = Reflection::GetReflectionTypeByteSize(target_type);
			info.component_fixup.component_functions.allocator_size = GetReflectionComponentAllocatorSize(target_type);
			
			// Check to see if we find a module defined fixup. If we do not, then auto create one
			size_t existing_fixup_index = module_component_functions.Find(target, [](const ModuleComponentFunctions& functions) {
				return functions.component_name;
			});
			if (existing_fixup_index == -1) {
				info.component_fixup.component_functions = GetReflectionTypeRuntimeComponentFunctions(reflection_manager, target_type, allocator);
				info.component_fixup.compare_entry = GetReflectionTypeRuntimeCompareEntry(reflection_manager, target_type, allocator);
			}
			else {
				// Set the functions only if their values are non null. These can be specified for the debug draw
				// Or the build function, and if that is the case, we still want to generate reflection functions
				if (module_component_functions[existing_fixup_index].copy_function == nullptr &&
					module_component_functions[existing_fixup_index].deallocate_function == nullptr) {
					info.component_fixup.component_functions = GetReflectionTypeRuntimeComponentFunctions(reflection_manager, target_type, allocator);
				}
				else {
					module_component_functions[existing_fixup_index].SetComponentFunctionsTo(
						&info.component_fixup.component_functions,
						info.component_fixup.component_functions.allocator_size
					);
				}
				if (module_component_functions[existing_fixup_index].compare_function == nullptr) {
					info.component_fixup.compare_entry = GetReflectionTypeRuntimeCompareEntry(reflection_manager, target_type, allocator);
				}
				else {
					module_component_functions[existing_fixup_index].SetCompareEntryTo(&info.component_fixup.compare_entry);
				}
			}

			overrides[index] = info;
		}
	}

	template<typename Table, typename DeserializeInfo, typename CheckFunctor>
	static void CreateDeserializeEntityManagerComponentTableImpl(
		Table& table,
		const Reflection::ReflectionManager* reflection_manager,
		AllocatorPolymorphic allocator,
		Stream<ModuleComponentFunctions> module_component_functions,
		Stream<DeserializeInfo> overrides,
		Component* component_overrides,
		Stream<unsigned int> hierarchy_indices,
		void* deserialize_function,
		void* deserialize_header_function,
		CheckFunctor&& check_functor
	) {
		CreateSerializeDeserializeEntityManagerComponentTable(table, reflection_manager, allocator, overrides, component_overrides, hierarchy_indices, [&](const Reflection::ReflectionType* type) {
			if (check_functor(type)) {
				DeserializeInfo info;
				info.function = (decltype(info.function))deserialize_function;
				info.header_function = (decltype(info.header_function))deserialize_header_function;
				ReflectionDeserializeComponentData* data = (ReflectionDeserializeComponentData*)Allocate(allocator, sizeof(ReflectionDeserializeComponentData));
				data->reflection_manager = reflection_manager;
				data->type = type;
				data->allocator = allocator;

				info.extra_data = data;
				info.name = type->name;
				info.component_fixup.component_byte_size = Reflection::GetReflectionTypeByteSize(type);
				info.component_fixup.component_functions.allocator_size = GetReflectionComponentAllocatorSize(type);

				size_t existing_component_functions_index = module_component_functions.Find(type->name, [](const ModuleComponentFunctions& entry) {
					return entry.component_name;
				});
				if (existing_component_functions_index == -1) {
					info.component_fixup.component_functions = GetReflectionTypeRuntimeComponentFunctions(reflection_manager, type, allocator);
					info.component_fixup.compare_entry = GetReflectionTypeRuntimeCompareEntry(reflection_manager, type, allocator);
				}
				else {
					if (module_component_functions[existing_component_functions_index].copy_function == nullptr &&
						module_component_functions[existing_component_functions_index].deallocate_function == nullptr) {
						info.component_fixup.component_functions = GetReflectionTypeRuntimeComponentFunctions(reflection_manager, type, allocator);
					}
					else {
						module_component_functions[existing_component_functions_index].SetComponentFunctionsTo(
							&info.component_fixup.component_functions,
							info.component_fixup.component_functions.allocator_size
						);
					}
					if (module_component_functions[existing_component_functions_index].compare_function == nullptr) {
						info.component_fixup.compare_entry = GetReflectionTypeRuntimeCompareEntry(reflection_manager, type, allocator);
					}
					else {
						module_component_functions[existing_component_functions_index].SetCompareEntryTo(&info.component_fixup.compare_entry);
					}
				}

				Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
				table.Insert(info, component);
			}
		});
	}

	void ConvertLinkTypesToDeserializeEntityManagerUnique(
		const Reflection::ReflectionManager* reflection_manager,
		const AssetDatabase* asset_database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		Stream<ModuleComponentFunctions> module_component_functions,
		DeserializeEntityManagerComponentInfo* overrides
	)
	{
		ConvertLinkTypesToDeserializeEntityManagerImpl(
			reflection_manager,
			asset_database,
			allocator,
			link_types,
			module_links,
			module_component_functions,
			overrides,
			ReflectionDeserializeEntityManagerLinkComponent,
			ReflectionDeserializeEntityManagerHeaderLinkComponent
		);
	}

	void CreateDeserializeEntityManagerComponentTable(
		DeserializeEntityManagerComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager, 
		AllocatorPolymorphic allocator,
		Stream<ModuleComponentFunctions> module_component_functions,
		Stream<DeserializeEntityManagerComponentInfo> overrides,
		Component* override_components,
		Stream<unsigned int> hierarchy_indices
	)
	{	
		CreateDeserializeEntityManagerComponentTableImpl(
			table,
			reflection_manager,
			allocator,
			module_component_functions,
			overrides,
			override_components,
			hierarchy_indices,
			ReflectionDeserializeEntityManagerComponent,
			ReflectionDeserializeEntityManagerHeaderComponent,
			[](const Reflection::ReflectionType* type) {
				return IsReflectionTypeComponent(type);
			}
		);
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
		const AssetDatabase* asset_database,
		AllocatorPolymorphic allocator,
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		Stream<ModuleComponentFunctions> module_component_functions,
		DeserializeEntityManagerSharedComponentInfo* overrides
	)
	{
		ConvertLinkTypesToDeserializeEntityManagerImpl(
			reflection_manager,
			asset_database,
			allocator,
			link_types,
			module_links,
			module_component_functions,
			overrides,
			ReflectionDeserializeEntityManagerLinkSharedComponent,
			ReflectionDeserializeEntityManagerHeaderLinkSharedComponent
		);
	}

	void CreateDeserializeEntityManagerSharedComponentTable(
		DeserializeEntityManagerSharedComponentTable& table,
		const Reflection::ReflectionManager* reflection_manager, 
		AllocatorPolymorphic allocator,
		Stream<ModuleComponentFunctions> module_component_functions,
		Stream<DeserializeEntityManagerSharedComponentInfo> overrides,
		Component* override_components,
		Stream<unsigned int> hierarchy_indices
	)
	{
		CreateDeserializeEntityManagerComponentTableImpl(
			table,
			reflection_manager,
			allocator,
			module_component_functions,
			overrides,
			override_components,
			hierarchy_indices,
			ReflectionDeserializeEntityManagerSharedComponent,
			ReflectionDeserializeEntityManagerHeaderSharedComponent,
			[](const Reflection::ReflectionType* type) {
				return IsReflectionTypeSharedComponent(type);
			}
		);
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

	void ConvertLinkTypesToDeserializeEntityManagerGlobal(
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* database, 
		AllocatorPolymorphic allocator, 
		Stream<const Reflection::ReflectionType*> link_types,
		Stream<ModuleLinkComponentTarget> module_links,
		DeserializeEntityManagerGlobalComponentInfo* overrides
	)
	{
		ConvertLinkTypesToDeserializeEntityManagerImpl(
			reflection_manager,
			database,
			allocator,
			link_types,
			module_links,
			{},
			overrides,
			ReflectionDeserializeEntityManagerLinkComponent,
			ReflectionDeserializeEntityManagerHeaderLinkComponent
		);
	}

	void CreateDeserializeEntityManagerGlobalComponentTable(
		DeserializeEntityManagerGlobalComponentTable& table, 
		const Reflection::ReflectionManager* reflection_manager, 
		AllocatorPolymorphic allocator, 
		Stream<DeserializeEntityManagerGlobalComponentInfo> overrides, 
		Component* override_components, 
		Stream<unsigned int> hierarchy_indices
	)
	{
		CreateDeserializeEntityManagerComponentTableImpl(
			table,
			reflection_manager,
			allocator,
			{},
			overrides,
			override_components,
			hierarchy_indices,
			ReflectionDeserializeEntityManagerComponent,
			ReflectionDeserializeEntityManagerHeaderComponent,
			[](const Reflection::ReflectionType* type) {
				return IsReflectionTypeGlobalComponent(type);
			}
		);
	}

	void AddDeserializeEntityManagerGlobalComponentTableOverrides(
		DeserializeEntityManagerGlobalComponentTable& table, 
		const Reflection::ReflectionManager* reflection_manager, 
		Stream<DeserializeEntityManagerGlobalComponentInfo> overrides
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
			const Reflection::ReflectionType* current_type = current_reflection_manager->TryGetType(current_name);
			if (current_type != nullptr) {
				// Check to see if the file type was a link type and now it's not
				Stream<char> target_type_name = GetReflectionTypeLinkNameBase(current_name);
				if (target_type_name.size < current_name.size) {
					// It was a link type - check to see if the target exists
					const Reflection::ReflectionType* target_type = current_reflection_manager->TryGetType(target_type_name);
					if (target_type != nullptr) {
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
					const Reflection::ReflectionType* link_type = current_reflection_manager->TryGetType(link_type_name);
					if (link_type != nullptr) {
						// Get the target of this link type - if it is the same proceed
						target_type_name = GetReflectionTypeLinkComponentTarget(link_type);
						if (target_type_name == current_name) {
							// Reference the link type name
							deserialize_field_table->types[index].name = link_type->name;
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