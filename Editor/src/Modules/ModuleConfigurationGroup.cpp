#include "ModuleConfigurationGroup.h"
#include "../Editor/EditorState.h"
#include "Module.h"
#include "../Project/ProjectOperations.h"
#include "../Editor/EditorEvent.h"

constexpr const wchar_t* MODULE_CONFIGURATION_GROUP_FILE_EXTENSION = L".ecsmodulegroup";
constexpr unsigned int MODULE_CONFIGURATION_GROUP_FILE_VERSION = 0;

constexpr size_t MODULE_CONFIGURATION_GROUP_FILE_MAXIMUM_GROUPS = 16;
constexpr size_t MODULE_CONFIGURATION_GROUP_FILE_MAXIMUM_BYTE_SIZE = 10'000;

using namespace ECSEngine;
ECS_CONTAINERS;

// ---------------------------------------------------------------------------------------------------------------------------

void AddModuleConfigurationGroup(
	EditorState* editor_state,
	Stream<char> name,
	Stream<unsigned int> library_masks,
	EditorModuleConfiguration* configurations
) {
	AddModuleConfigurationGroup(editor_state, CreateModuleConfigurationGroup(editor_state, name, library_masks, configurations));
}

// ---------------------------------------------------------------------------------------------------------------------------

void AddModuleConfigurationGroup(EditorState* editor_state, ModuleConfigurationGroup group)
{
	editor_state->module_configuration_groups.Add(group);
}

// ---------------------------------------------------------------------------------------------------------------------------

ModuleConfigurationGroup CreateModuleConfigurationGroup(
	EditorState* editor_state,
	Stream<char> name,
	Stream<unsigned int> library_masks,
	EditorModuleConfiguration* configurations
) {
	ModuleConfigurationGroup group;

	ProjectModules* modules = (ProjectModules*)editor_state->project_modules;

	// Coallesced allocation
	// Null terminated string for the name
	size_t total_memory = sizeof(char) * (name.size + 1);
	for (size_t index = 0; index < library_masks.size; index++) {
		total_memory += sizeof(wchar_t) * modules->buffer[library_masks[index]].library_name.size;
	}
	total_memory += sizeof(Stream<wchar_t>) * library_masks.size;
	total_memory += sizeof(EditorModuleConfiguration) * library_masks.size;

	void* allocation = editor_state->editor_allocator->Allocate(total_memory);
	uintptr_t ptr = (uintptr_t)allocation;
	group.libraries.InitializeFromBuffer(ptr, library_masks.size);
	for (size_t index = 0; index < library_masks.size; index++) {
		group.libraries[index].InitializeAndCopy(ptr, modules->buffer[library_masks[index]].library_name);
	}
	group.name.InitializeAndCopy(ptr, name);
	group.name[name.size] = '\0';
	ptr += 1;
	group.configurations = (EditorModuleConfiguration*)ptr;
	memcpy(group.configurations, configurations, sizeof(EditorModuleConfiguration) * library_masks.size);

	return group;
}

// ---------------------------------------------------------------------------------------------------------------------------

void DeallocateModuleConfigurationGroup(EditorState* editor_state, unsigned int index) {
	ECS_ASSERT(index < editor_state->module_configuration_groups.size);
	editor_state->editor_allocator->Deallocate(editor_state->module_configuration_groups[index].libraries.buffer);
}

// ---------------------------------------------------------------------------------------------------------------------------

void GetModuleConfigurationGroupFilePath(EditorState* editor_state, CapacityStream<wchar_t>& path) {
	path.size = 0;
	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	path.Copy(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(project_file->project_name);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(ToStream(MODULE_CONFIGURATION_GROUP_FILE_EXTENSION));
	path[path.size] = L'\0';
}

// ---------------------------------------------------------------------------------------------------------------------------

unsigned int GetModuleConfigurationGroupIndex(EditorState* editor_state, Stream<char> name) {
	for (size_t index = 0; index < editor_state->module_configuration_groups.size; index++) {
		if (function::CompareStrings(name, editor_state->module_configuration_groups[index].name)) {
			return index;
		}
	}
	return -1;
}

// ---------------------------------------------------------------------------------------------------------------------------

void RemoveModuleConfigurationGroup(EditorState* editor_state, unsigned int index) {
	DeallocateModuleConfigurationGroup(editor_state, index);
	editor_state->module_configuration_groups.RemoveSwapBack(index);
}

// ---------------------------------------------------------------------------------------------------------------------------

void RemoveModuleConfigurationGroup(EditorState* editor_state, Stream<char> name) {
	unsigned int group_index = GetModuleConfigurationGroupIndex(editor_state, name);
	if (group_index != -1) {
		RemoveModuleConfigurationGroup(editor_state, group_index);
	}
	ECS_FORMAT_TEMP_STRING(error_message, "Could not find {0} module configuration group.", name);
	EditorSetConsoleError(editor_state, error_message);
}

// ---------------------------------------------------------------------------------------------------------------------------

void ResetModuleConfigurationGroups(EditorState* editor_state)
{
	for (size_t index = 0; index < editor_state->module_configuration_groups.size; index++) {
		DeallocateModuleConfigurationGroup(editor_state, index);
	}
	editor_state->module_configuration_groups.FreeBuffer();
}

// ---------------------------------------------------------------------------------------------------------------------------

bool LoadModuleConfigurationGroupFile(EditorState* editor_state) {
	ECS_TEMP_STRING(file_path, 256);
	GetModuleConfigurationGroupFilePath(editor_state, file_path);

	std::ifstream stream(std::wstring(file_path.buffer, file_path.buffer + file_path.size));
	if (stream.good()) {
		// Read the whole file into a temporary buffer
		size_t file_size = function::GetFileByteSize(stream);
		ECS_ASSERT(file_size < MODULE_CONFIGURATION_GROUP_FILE_MAXIMUM_BYTE_SIZE);
		void* allocation = editor_state->editor_allocator->Allocate(file_size);
		stream.read((char*)allocation, file_size);

		if (stream.good()) {
			uintptr_t file_ptr = (uintptr_t)allocation;

			// Make sure it is the correct file version
			char header[64];
			CapacityStream<void> header_stream(header, 0, 64);
			DeserializeMultisectionHeader(file_ptr, header_stream);
			unsigned int* header_version = (unsigned int*)header;
			ECS_ASSERT(*header_version == MODULE_CONFIGURATION_GROUP_FILE_VERSION);

			// Get the count of groups and the count of libraries for each group
			size_t group_count = DeserializeMultisectionCount(file_ptr);
			ECS_ASSERT(group_count < MODULE_CONFIGURATION_GROUP_FILE_MAXIMUM_GROUPS);

			size_t* _group_stream_counts = (size_t*)ECS_STACK_ALLOC(sizeof(size_t) * group_count);
			size_t* _group_byte_size = (size_t*)ECS_STACK_ALLOC(sizeof(size_t) * group_count);
			CapacityStream<size_t> group_stream_counts(_group_stream_counts, 0, group_count);
			CapacityStream<size_t> group_byte_size(_group_byte_size, 0, group_count);
			DeserializeMultisectionStreamCount(file_ptr, group_stream_counts);

			// The first section of each group is the name, followed by the configurations and
			// finally the library names. The stream of streams of library names is not stored directly
			// The count of libraries can be infered from the total number of sections
			DeserializeMultisectionPerSectionSize(file_ptr, group_byte_size);

			// Get the total stream count
			size_t total_stream_count = 0;
			for (size_t index = 0; index < group_count; index++) {
				total_stream_count += group_stream_counts[index];
			}
			void* stream_allocation = editor_state->editor_allocator->Allocate(sizeof(Stream<void>) * total_stream_count + sizeof(Stream<Stream<void>>) * group_count);
			CapacityStream<SerializeMultisectionData> multisections(stream_allocation, 0, group_count);
			uintptr_t stream_allocation_ptr = (uintptr_t)stream_allocation;
			stream_allocation_ptr += sizeof(Stream<Stream<void>>) * group_count;
			for (size_t index = 0; index < group_count; index++) {
				multisections[index].data.InitializeFromBuffer(stream_allocation_ptr, group_stream_counts[index]);
			}

			void* _group_per_stream_size = editor_state->editor_allocator->Allocate(sizeof(size_t) * total_stream_count + sizeof(Stream<size_t>) * group_count);
			uintptr_t group_per_stream_size_ptr = (uintptr_t)_group_per_stream_size;
			CapacityStream<Stream<size_t>> group_per_stream_size;
			// The deserialization call will need the size set to 0 at first
			group_per_stream_size.InitializeFromBuffer(group_per_stream_size_ptr, 0, group_count);
			for (size_t index = 0; index < group_count; index++) {
				group_per_stream_size[index].InitializeFromBuffer(group_per_stream_size_ptr, group_stream_counts[index]);
			}
			DeserializeMultisectionStreamSizes(file_ptr, group_per_stream_size);

			ModuleConfigurationGroup* groups = (ModuleConfigurationGroup*)ECS_STACK_ALLOC(sizeof(ModuleConfigurationGroup) * group_count);

			// Allocate the count of bytes for each group separately
			for (size_t index = 0; index < group_count; index++) {
				size_t library_count = group_stream_counts[index] - 2;
				void* group_allocation = editor_state->editor_allocator->Allocate(group_byte_size[index] + sizeof(Stream<Stream<wchar_t>>) * library_count);
				// First the library buffer must be set
				uintptr_t group_ptr = (uintptr_t)group_allocation;
				group_ptr += sizeof(Stream<wchar_t>) * library_count;
				Stream<wchar_t>* libraries = (Stream<wchar_t>*)group_allocation;

				// Then each individual library
				for (size_t library_index = 2; library_index < group_count; library_index) {
					multisections[index].data[library_index].buffer = (void*)group_ptr;
					libraries[library_index - 2].InitializeFromBuffer(group_ptr, group_per_stream_size[index][library_index] / sizeof(wchar_t));
				}
				// The name - located at index 0 
				multisections[index].data[0].buffer = (void*)group_ptr;
				groups[index].name = { (const char*)group_ptr, group_per_stream_size[index][0] };
				group_ptr += group_per_stream_size[index][0];

				// The configurations
				multisections[index].data[1].buffer = (void*)group_ptr;
				groups[index].configurations = (EditorModuleConfiguration*)group_ptr;

				groups[index].libraries = { libraries, library_count };
			}
			DeserializeMultisection(file_ptr, multisections);

			ValidateModuleConfigurationGroups(editor_state, Stream<ModuleConfigurationGroup>(groups, group_count));
			// Add the group only if it has at least a library
			for (size_t index = 0; index < group_count; index++) {
				if (groups[index].libraries.size > 0) {
					AddModuleConfigurationGroup(editor_state, groups[index]);
				}
			}

			editor_state->editor_allocator->Deallocate(_group_per_stream_size);
			editor_state->editor_allocator->Deallocate(stream_allocation);
			editor_state->editor_allocator->Deallocate(allocation);
			return true;
		}
		editor_state->editor_allocator->Deallocate(allocation);
	}
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------------

bool SaveModuleConfigurationGroupFile(EditorState* editor_state) {
	ECS_TEMP_STRING(file_path, 256);
	GetModuleConfigurationGroupFilePath(editor_state, file_path);

	std::ofstream stream(std::wstring(file_path.buffer, file_path.buffer + file_path.size));
	if (stream.good()) {
		Stream<ModuleConfigurationGroup> groups(editor_state->module_configuration_groups.buffer, editor_state->module_configuration_groups.size);

		// Make the multisection allocation
		SerializeMultisectionData* _multisections = (SerializeMultisectionData*)ECS_STACK_ALLOC(sizeof(SerializeMultisectionData) * groups.size);
		// Calculate the total streams needed
		size_t total_streams = 0;
		for (size_t index = 0; index < groups.size; index++) {
			total_streams += 2 + groups[index].libraries.size;
		}
		Stream<void>* streams = (Stream<void>*)ECS_STACK_ALLOC(sizeof(Stream<void>) * total_streams);
		Stream<SerializeMultisectionData> multisections(_multisections, groups.size);

		// Populate the multisections
		for (size_t index = 0; index < groups.size; index++) {
			// First the name
			multisections[index].data[0] = groups[index].name;
			multisections[index].data[1] = { groups[index].configurations, groups[index].libraries.size + 1 };
			for (size_t subindex = 0; subindex < groups[index].libraries.size; subindex++) {
				multisections[index].data[subindex + 2] = groups[index].libraries[subindex];
			}
		}

		char header[64];
		unsigned int* header_version = (unsigned int*)header;
		*header_version = MODULE_CONFIGURATION_GROUP_FILE_VERSION;

		// Serialize
		return SerializeMultisection(stream, multisections, { header, 64 });
	}
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------------

void ValidateModuleConfigurationGroups(EditorState* editor_state, Stream<ModuleConfigurationGroup> groups) {
	for (size_t index = 0; index < groups.size; index++) {
		// Check the configurations
		for (size_t subindex = 0; subindex <= groups[index].libraries.size; subindex++) {
			if (groups[index].configurations[subindex] >= EditorModuleConfiguration::Count) {
				groups[index].configurations[subindex] = EditorModuleConfiguration::Debug;
			}
		}

		for (int64_t subindex = 0; subindex < (int64_t)groups[index].libraries.size; subindex++) {
			// If it doesn't exist - remove it
			if (ProjectModuleIndexFromName(editor_state, groups[index].libraries[subindex]) == -1) {
				// The configuration must be swapped manually
				groups[index].configurations[subindex] = groups[index].configurations[groups[index].libraries.size - 1];
				groups[index].libraries.RemoveSwapBack(subindex);
				subindex--;
			}
		}
	}

}

// ---------------------------------------------------------------------------------------------------------------------------