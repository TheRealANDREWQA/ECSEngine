#include "editorpch.h"
#include "ModuleConfigurationGroup.h"
#include "../Editor/EditorState.h"
#include "Module.h"
#include "../Project/ProjectOperations.h"
#include "../Editor/EditorEvent.h"
#include "ModuleFile.h"

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

void ApplyModuleConfigurationGroup(EditorState* editor_state, unsigned int group_index)
{
	const ModuleConfigurationGroup* group = editor_state->module_configuration_groups.buffer + group_index;
	ProjectModules* modules = (ProjectModules*)editor_state->project_modules;

	bool has_library_changed = false;
	for (size_t index = 0; index < modules->size; index++) {
		size_t subindex = 0;
		bool has_been_found = false;
		for (; subindex < group->libraries.size && !has_been_found; subindex++) {
			unsigned int in_group_index = function::FindString(modules->buffer[index].library_name, group->libraries);
			if (in_group_index != -1) {
				has_been_found = true;
				ChangeProjectModuleConfiguration(editor_state, index, group->configurations[in_group_index]);
			}
		}
		has_library_changed |= has_been_found;

		// The fallback configuration is set if it does not belong to the group
		if (!has_been_found) {
			ChangeProjectModuleConfiguration(editor_state, index, group->configurations[group->libraries.size]);
		}
	}

	// At least one library had it's configuration changed
	if (has_library_changed) {
		bool success = SaveModuleFile(editor_state);
		if (!success) {
			EditorSetConsoleError(editor_state, ToStream("An error occured when saving the module file after configuration group application."));
		}
	}
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
	total_memory += sizeof(EditorModuleConfiguration) * (library_masks.size + 1);

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
	memcpy(group.configurations, configurations, sizeof(EditorModuleConfiguration) * (library_masks.size + 1));

	return group;
}

// ---------------------------------------------------------------------------------------------------------------------------

ModuleConfigurationGroup CreateModuleConfigurationGroup(
	EditorState* editor_state,
	Stream<char> name,
	Stream<Stream<wchar_t>> libraries,
	EditorModuleConfiguration* configurations
) {
	ModuleConfigurationGroup group;

	// Coallesced allocation
	// Null terminated string for the name
	size_t total_memory = sizeof(char) * (name.size + 1);
	for (size_t index = 0; index < libraries.size; index++) {
		total_memory += sizeof(wchar_t) * libraries[index].size;
	}
	total_memory += sizeof(Stream<wchar_t>) * libraries.size;
	total_memory += sizeof(EditorModuleConfiguration) * libraries.size;

	void* allocation = editor_state->editor_allocator->Allocate(total_memory);
	uintptr_t ptr = (uintptr_t)allocation;
	group.libraries.InitializeFromBuffer(ptr, libraries.size);
	for (size_t index = 0; index < libraries.size; index++) {
		group.libraries[index].InitializeAndCopy(ptr, libraries[index]);
	}
	group.name.InitializeAndCopy(ptr, name);
	group.name[name.size] = '\0';
	ptr += 1;
	group.configurations = (EditorModuleConfiguration*)ptr;
	memcpy(group.configurations, configurations, sizeof(EditorModuleConfiguration) * libraries.size);

	return group;
}

// ---------------------------------------------------------------------------------------------------------------------------

void DeallocateModuleConfigurationGroup(EditorState* editor_state, unsigned int index) {
	ECS_ASSERT(index < editor_state->module_configuration_groups.size);
	editor_state->editor_allocator->Deallocate(editor_state->module_configuration_groups[index].libraries.buffer);
}

// ---------------------------------------------------------------------------------------------------------------------------

void DeleteModuleConfigurationGroupFile(EditorState* editor_state)
{
	ECS_TEMP_STRING(path, 256);
	GetModuleConfigurationGroupFilePath(editor_state, path);
	if (ExistsFileOrFolder(path)) {
		RemoveFile(path);
	}
	else {
		EditorSetConsoleError(editor_state, ToStream("An error occured when deleting the module configuration group file."));
	}
}

// ---------------------------------------------------------------------------------------------------------------------------

void GetModuleConfigurationGroupFilePath(EditorState* editor_state, CapacityStream<wchar_t>& path) {
	path.size = 0;
	const ProjectFile* project_file = (const ProjectFile*)editor_state->project_file;
	path.Copy(project_file->path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(project_file->project_name);
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

	Stream<void> content = ReadWholeFileBinary(file_path, GetAllocatorPolymorphic(editor_state->editor_allocator));
	if (content.buffer != nullptr) {
		// Read the whole file into a temporary buffer
		ECS_ASSERT(content.size < MODULE_CONFIGURATION_GROUP_FILE_MAXIMUM_BYTE_SIZE);

		uintptr_t file_ptr = (uintptr_t)content.buffer;

		// Make sure it is the correct file version
		char header[64];
		CapacityStream<void> header_stream(header, 0, 64);
		DeserializeMultisectionHeader(file_ptr, header_stream);
		unsigned int* header_version = (unsigned int*)header;
		ECS_ASSERT(*header_version == MODULE_CONFIGURATION_GROUP_FILE_VERSION);

		// Get the count of groups and the count of libraries for each group
		size_t group_count = DeserializeMultisectionCount(file_ptr, header_stream.size);
		ECS_ASSERT(group_count < MODULE_CONFIGURATION_GROUP_FILE_MAXIMUM_GROUPS);
		if (group_count == 0) {
			ResetModuleConfigurationGroups(editor_state);
			EditorSetConsoleWarn(editor_state, ToStream("The module configuration group file is empty."));
			editor_state->editor_allocator->Deallocate(content.buffer);
			return false;
		}

		SerializeMultisectionData* _multisections = (SerializeMultisectionData*)ECS_STACK_ALLOC(sizeof(SerializeMultisectionData) * group_count);
		CapacityStream<SerializeMultisectionData> multisections(_multisections, 0, group_count);

		const size_t MAX_STREAMS = 512;
		void* _stack_allocation = ECS_STACK_ALLOC(sizeof(Stream<void>) * MAX_STREAMS);
		CapacityStream<void> memory_pool(_stack_allocation, 0, sizeof(Stream<void>) * MAX_STREAMS);

		DeserializeMultisection(multisections, memory_pool, file_ptr);
		ModuleConfigurationGroup* groups = (ModuleConfigurationGroup*)ECS_STACK_ALLOC(sizeof(ModuleConfigurationGroup) * group_count);

		for (size_t index = 0; index < group_count; index++) {
			size_t library_count = multisections[index].data.size - 2;
			size_t group_byte_size = 0;
			for (size_t subindex = 0; subindex < multisections[index].data.size; subindex++) {
				group_byte_size += multisections[index].data[subindex].size;
			}

			void* group_allocation = editor_state->editor_allocator->Allocate(group_byte_size + sizeof(Stream<Stream<wchar_t>>) * library_count);
			uintptr_t group_ptr = (uintptr_t)group_allocation;
			group_ptr += sizeof(Stream<wchar_t>) * library_count;
			Stream<wchar_t>* libraries = (Stream<wchar_t>*)group_allocation;

			// Then each individual library
			for (size_t library_index = 2; library_index < multisections[index].data.size; library_index++) {
				multisections[index].data[library_index].buffer = (void*)group_ptr;
				libraries[library_index - 2].InitializeFromBuffer(group_ptr, multisections[index].data[library_index].size / sizeof(wchar_t));
				multisections[index].data[library_index].CopyTo(libraries[library_index - 2].buffer);
			}

			// The name - located at index 0 
			multisections[index].data[0].buffer = (void*)group_ptr;
			groups[index].name = { (const char*)group_ptr, multisections[index].data[0].size };
			// Account for the null terminator
			group_ptr += multisections[index].data[0].size;

			// The configurations
			multisections[index].data[1].buffer = (void*)group_ptr;
			groups[index].configurations = (EditorModuleConfiguration*)group_ptr;

			groups[index].libraries = { libraries, library_count };

			// Now copy the data into the group name and configurations
			multisections[index].data[0].CopyTo(groups[index].name.buffer);
			multisections[index].data[1].CopyTo(groups[index].configurations);
		}

		//// Allocate the count of bytes for each group separately
		//for (size_t index = 0; index < group_count; index++) {
		//	size_t library_count = group_stream_counts[index] - 2;
		//	void* group_allocation = editor_state->editor_allocator->Allocate(group_byte_size[index] + sizeof(Stream<Stream<wchar_t>>) * library_count);
		//	// First the library buffer must be set
		//	uintptr_t group_ptr = (uintptr_t)group_allocation;
		//	group_ptr += sizeof(Stream<wchar_t>) * library_count;
		//	Stream<wchar_t>* libraries = (Stream<wchar_t>*)group_allocation;

		//	// Then each individual library
		//	for (size_t library_index = 2; library_index < group_stream_counts[index]; library_index++) {
		//		multisections[index].data[library_index].buffer = (void*)group_ptr;
		//		libraries[library_index - 2].InitializeFromBuffer(group_ptr, group_per_stream_size[index][library_index] / sizeof(wchar_t));
		//	}
		//	// The name - located at index 0 
		//	multisections[index].data[0].buffer = (void*)group_ptr;
		//	groups[index].name = { (const char*)group_ptr, group_per_stream_size[index][0] };

		//	// It will also load the null terminator - remove it from the size of the stream
		//	groups[index].name.size--;
		//	group_ptr += group_per_stream_size[index][0];

		//	// The configurations
		//	multisections[index].data[1].buffer = (void*)group_ptr;
		//	groups[index].configurations = (EditorModuleConfiguration*)group_ptr;

		//	groups[index].libraries = { libraries, library_count };
		//}

		ValidateModuleConfigurationGroups(editor_state, Stream<ModuleConfigurationGroup>(groups, group_count));
		// Add the group only if it has at least a library
		for (size_t index = 0; index < group_count; index++) {
			if (groups[index].libraries.size > 0) {
				AddModuleConfigurationGroup(editor_state, groups[index]);
			}
		}

		editor_state->editor_allocator->Deallocate(content.buffer);
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------------

bool SaveModuleConfigurationGroupFile(EditorState* editor_state) {
	ECS_TEMP_STRING(file_path, 256);
	GetModuleConfigurationGroupFilePath(editor_state, file_path);

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
		size_t current_stream_count = groups[index].libraries.size + 2;
		multisections[index].data.buffer = streams;
		multisections[index].data.size = groups[index].libraries.size + 2;
		multisections[index].name = nullptr;
		streams += current_stream_count;

		// First the name
		multisections[index].data[0] = { groups[index].name.buffer, (groups[index].name.size + 1) * sizeof(char) };
		multisections[index].data[1] = { groups[index].configurations, (groups[index].libraries.size + 1) * sizeof(EditorModuleConfiguration) };
		for (size_t subindex = 0; subindex < groups[index].libraries.size; subindex++) {
			multisections[index].data[subindex + 2] = groups[index].libraries[subindex];
		}
	}

	char header[64];
	unsigned int* header_version = (unsigned int*)header;
	*header_version = MODULE_CONFIGURATION_GROUP_FILE_VERSION;

	// Serialize
	return SerializeMultisection(multisections, file_path, { nullptr }, { header, 64 });
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

void UpdateModuleConfigurationGroup(EditorState* editor_state, unsigned int index, ModuleConfigurationGroup group)
{
	editor_state->module_configuration_groups[index] = group;
}

// ---------------------------------------------------------------------------------------------------------------------------

struct ModuleConfigurationGroupAddWizardData {
	EditorState* editor_state;
	unsigned int group_index;
};

// Group index -1 means that the wizard should create a new group, not edit an existing one
struct ModuleConfigurationAddActionData {
	// The module name is useful only when creating a new group, i.e. when group index is -1
	CapacityStream<char>* module_name;
	Stream<unsigned short>* new_module_indices;
	bool* new_module_configuration_states;
	unsigned char* new_module_configurations;
	EditorState* editor_state;
	unsigned int group_index;
	// This is used for communicating with the caller in order to 
	bool destroy_window = false;
};

// ---------------------------------------------------------------------------------------------------------------------------

void ModuleConfigurationGroupAddAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ModuleConfigurationAddActionData* data = (ModuleConfigurationAddActionData*)_data;

	ProjectModules* modules = (ProjectModules*)data->editor_state->project_modules;

	// If the group index is different from -1, i.e. edit an existing group
	if (data->group_index != -1) {
		ModuleConfigurationGroup* group = data->editor_state->module_configuration_groups.buffer + data->group_index;
		Stream<Stream<wchar_t>> group_libraries(ECS_STACK_ALLOC(sizeof(Stream<wchar_t>) * modules->size), group->libraries.size);
		group_libraries.Copy(group->libraries);
		for (size_t index = 0; index < data->new_module_indices->size; index++) {
			if (data->new_module_configuration_states[index]) {
				group_libraries.Add(modules->buffer[data->new_module_indices->buffer[index]].library_name);
			}
		}
		EditorModuleConfiguration* configurations = (EditorModuleConfiguration*)ECS_STACK_ALLOC(sizeof(EditorModuleConfiguration) * modules->size);
		memcpy(configurations, group->configurations, group->libraries.size * sizeof(EditorModuleConfiguration));
		memcpy(configurations + group->libraries.size, data->new_module_configurations, sizeof(EditorModuleConfiguration) * data->new_module_indices->size);

		// Set the fallback configuration
		configurations[group->libraries.size + data->new_module_indices->size] = group->configurations[group->libraries.size];
		ModuleConfigurationGroup new_group = CreateModuleConfigurationGroup(data->editor_state, group->name, group_libraries, configurations);
		DeallocateModuleConfigurationGroup(data->editor_state, data->group_index);
		UpdateModuleConfigurationGroup(data->editor_state, data->group_index, new_group);

		bool success = SaveModuleConfigurationGroupFile(data->editor_state);
		if (!success) {
			EditorSetConsoleError(data->editor_state, ToStream("An error occured when saving the module configuration group file."));
		}
		data->destroy_window = true;
	}
	else {
		// Create a new group from the selection
		// Check that it has a name with a size of at least 1
		if (data->module_name->size == 0) {
			CreateErrorMessageWindow(system, "Cannot create a configuration group with an empty name!");
		}

		unsigned int* filtered_indices = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * data->new_module_indices->size);
		EditorModuleConfiguration* configurations = (EditorModuleConfiguration*)ECS_STACK_ALLOC(sizeof(EditorModuleConfiguration) * (data->new_module_indices->size + 1));

		// Walk through the states and select the libraries to be added
		unsigned int valid_libraries = 0;
		for (size_t index = 0; index < data->new_module_indices->size; index++) {
			if (data->new_module_configuration_states[index]) {
				filtered_indices[valid_libraries] = data->new_module_indices->buffer[index];
				configurations[valid_libraries] = (EditorModuleConfiguration)data->new_module_configurations[index];
				valid_libraries++;
			}
		}
		if (valid_libraries == 0) {
			CreateErrorMessageWindow(system, "Cannot create an empty configuration group!");
			return;
		}
		configurations[valid_libraries] = (EditorModuleConfiguration)data->new_module_configurations[data->new_module_indices->size];

		AddModuleConfigurationGroup(data->editor_state, *data->module_name, Stream<unsigned int>(filtered_indices, valid_libraries), configurations);

		bool success = SaveModuleConfigurationGroupFile(data->editor_state);
		if (!success) {
			EditorSetConsoleError(data->editor_state, ToStream("An error occured when saving the module configuration group file."));
		}
		data->destroy_window = true;
	}
}

// ---------------------------------------------------------------------------------------------------------------------------

void ModuleConfigurationGroupAddWizard(void* window_data, void* drawer_descriptor, bool initialize) {
	const const char* NEW_MODULE_INDICES_NAME = "Resource";
	const const char* NEW_MODULE_CONFIGURATION_STATES = "States";
	const const char* NEW_MODULE_CONFIGURATIONS = "Configurations";
	const const char* NEW_MODULE_RESOURCE_NAME = "Module Name Input";
	const const char* ADD_BUTTON_TEXT = "Add";
	const const char* CANCEL_BUTTON_TEXT = "Cancel";

	UI_PREPARE_DRAWER(initialize);

	ModuleConfigurationGroupAddWizardData* data = (ModuleConfigurationGroupAddWizardData*)window_data;
	EditorState* editor_state = data->editor_state;
	const ProjectModules* project_modules = (const ProjectModules*)editor_state->project_modules;

	Stream<unsigned short>* new_module_indices = nullptr;
	bool* new_module_configuration_states = nullptr;
	unsigned char* new_module_configurations = nullptr;
	CapacityStream<char>* new_module_name = nullptr;
	if (initialize) {
		unsigned short* _new_module_indices = (unsigned short*)ECS_STACK_ALLOC(sizeof(unsigned short) * project_modules->size);
		Stream<unsigned short> new_module_indices_stream(_new_module_indices, project_modules->size);
		function::MakeSequence(new_module_indices_stream);

		if (!HasGraphicsModule(editor_state)) {
			new_module_indices_stream.buffer++;
			new_module_indices_stream.size--;
		}

		if (data->group_index != -1) {
			ModuleConfigurationGroup* group = editor_state->module_configuration_groups.buffer + data->group_index;
			for (size_t index = 0; index < group->libraries.size; index++) {
				unsigned int module_index = ProjectModuleIndexFromName(editor_state, group->libraries[index]);
				ECS_ASSERT(module_index != -1);
				for (size_t subindex = 0; subindex < new_module_indices_stream.size; subindex++) {
					if (new_module_indices_stream[subindex] == module_index) {
						new_module_indices_stream.RemoveSwapBack(subindex);
						break;
					}
				}
			}
			ECS_ASSERT(new_module_indices_stream.size > 0);
		}

		size_t stream_size = sizeof(Stream<unsigned short>) + sizeof(unsigned short) * new_module_indices_stream.size;
		new_module_indices = (Stream<unsigned short>*)function::CoallesceStreamWithData(drawer.GetMainAllocatorBufferAndStoreAsResource(NEW_MODULE_INDICES_NAME, stream_size), new_module_indices_stream.size);
		new_module_indices->Copy(new_module_indices_stream);

		new_module_configuration_states = (bool*)drawer.GetMainAllocatorBufferAndStoreAsResource(NEW_MODULE_CONFIGURATION_STATES, sizeof(bool) * new_module_indices_stream.size);
		memset(new_module_configuration_states, 0, sizeof(bool) * new_module_indices_stream.size);

		// +1 for the fallback config - in case a new group is created
		new_module_configurations = (unsigned char*)drawer.GetMainAllocatorBufferAndStoreAsResource(NEW_MODULE_CONFIGURATIONS, sizeof(unsigned char) * (new_module_indices_stream.size + 1));
		memset(new_module_configurations, 0, sizeof(unsigned char) * (new_module_indices->size + 1));

		new_module_name = (CapacityStream<char>*)function::CoallesceCapacityStreamWithData(
			drawer.GetMainAllocatorBufferAndStoreAsResource(NEW_MODULE_RESOURCE_NAME, sizeof(CapacityStream<char>) + sizeof(char) * 256), 
			0,
			256
		);
		new_module_name->buffer[0] = '\0';
	}
	else {
		new_module_indices = (Stream<unsigned short>*)drawer.GetResource(NEW_MODULE_INDICES_NAME);
		new_module_configuration_states = (bool*)drawer.GetResource(NEW_MODULE_CONFIGURATION_STATES);
		new_module_configurations = (unsigned char*)drawer.GetResource(NEW_MODULE_CONFIGURATIONS);
		new_module_name = (CapacityStream<char>*)drawer.GetResource(NEW_MODULE_RESOURCE_NAME);
	}

	// Text input for the module name if creating a new module
	if (data->group_index == -1) {
		drawer.TextInput("Module Name", new_module_name);
		drawer.NextRow(2.0f);
		drawer.CrossLine();
		drawer.NextRow();
	}

	UIDrawConfig config;
	ECS_TEMP_ASCII_STRING(ascii_library, 256);
	Stream<const char*> combo_labels = Stream<const char*>(MODULE_CONFIGURATIONS, std::size(MODULE_CONFIGURATIONS));
	for (size_t index = 0; index < new_module_indices->size; index++) {
		ascii_library.size = 0;
		function::ConvertWideCharsToASCII(project_modules->buffer[new_module_indices->buffer[index]].library_name, ascii_library);
		ascii_library[ascii_library.size] = '\0';
		drawer.CheckBox(UI_CONFIG_DO_NOT_CACHE, config, ascii_library.buffer, new_module_configuration_states + index);
		drawer.ComboBox(ascii_library.buffer, combo_labels, combo_labels.size, new_module_configurations + index);
		drawer.NextRow();
	}

	// The fallback configuration if a new group is created
	if (data->group_index == -1) {
		drawer.CrossLine();
		drawer.NextRow();
		drawer.ComboBox("Default configuration", combo_labels, combo_labels.size, new_module_configurations + new_module_indices->size);
		drawer.NextRow();
	}

	// The Add and Cancel buttons
	UIConfigAbsoluteTransform transform;
	transform.scale = drawer.GetLabelScale(ADD_BUTTON_TEXT);
	transform.position = drawer.GetAlignedToBottom(transform.scale.y);
	config.AddFlag(transform);

	auto add_action = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		ModuleConfigurationAddActionData* data = (ModuleConfigurationAddActionData*)_data;
		ModuleConfigurationGroupAddAction(action_data);

		if (data->destroy_window) {
			// Destroy the window
			CloseXBorderClickableAction(action_data);
		}
	};

	struct EnterCallbackData {
		ModuleConfigurationAddActionData add_data;
		unsigned int window_index;
	};

	EnterCallbackData callback_data;
	callback_data.add_data.editor_state = editor_state;
	callback_data.add_data.group_index = data->group_index;
	callback_data.add_data.new_module_configurations = new_module_configurations;
	callback_data.add_data.new_module_configuration_states = new_module_configuration_states;
	callback_data.add_data.new_module_indices = new_module_indices;
	callback_data.add_data.module_name = new_module_name;

	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM, config, ADD_BUTTON_TEXT, { add_action, &callback_data, sizeof(ModuleConfigurationAddActionData), UIDrawPhase::System });
	config.flag_count--;

	// The cancel button
	transform.scale = drawer.GetLabelScale(CANCEL_BUTTON_TEXT);
	transform.position.x = drawer.GetAlignedToRight(transform.scale.x).x;
	config.AddFlag(transform);

	drawer.Button(UI_CONFIG_ABSOLUTE_TRANSFORM,config, CANCEL_BUTTON_TEXT, { CloseXBorderClickableAction, nullptr, 0, UIDrawPhase::System });

	// Check to see if Enter was pressed - if it is, then call the add function
	auto enter_callback = [](ActionData* action_data) {
		UI_UNPACK_ACTION_DATA;

		EnterCallbackData* callback_data = (EnterCallbackData*)_data;
		ModuleConfigurationGroupAddAction(action_data);
		if (callback_data->add_data.destroy_window) {
			system->RemoveWindowFromDockspaceRegion(callback_data->window_index);
			system->PopFrameHandler();
		}
	};

	if (drawer.system->m_keyboard_tracker->IsKeyPressed(HID::Key::Enter)) {
		drawer.system->PushFrameHandler({ enter_callback, &callback_data, sizeof(callback_data) });
	}
}

// ---------------------------------------------------------------------------------------------------------------------------

unsigned int CreateModuleConfigurationGroupAddWizard(EditorState* editor_state, unsigned int group_index) {
	UIWindowDescriptor descriptor;

	descriptor.draw = ModuleConfigurationGroupAddWizard;
	descriptor.destroy_action = ReleaseLockedWindow;

	descriptor.initial_position_x = 0.0f;
	descriptor.initial_position_y = 0.0f;
	descriptor.initial_size_x = 10.0f;
	descriptor.initial_size_y = 10.0f;

	ModuleConfigurationGroupAddWizardData window_data;
	window_data.editor_state = editor_state;
	window_data.group_index = group_index;
	descriptor.window_data = &window_data;
	descriptor.window_data_size = sizeof(window_data);
	descriptor.window_name = "Add/Edit Module Configuration Group";

	return editor_state->ui_system->CreateWindowAndDockspace(descriptor, UI_DOCKSPACE_POP_UP_WINDOW | UI_DOCKSPACE_LOCK_WINDOW | UI_DOCKSPACE_NO_DOCKING | UI_POP_UP_WINDOW_ALL);
}

// ---------------------------------------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------------------------------------