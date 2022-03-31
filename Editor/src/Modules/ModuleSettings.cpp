#include "editorpch.h"
#include "ModuleSettings.h"
#include "Editor\EditorState.h"
#include "..\Project\ProjectFolders.h"
#include "Module.h"
#include "ModuleFile.h"

using namespace ECSEngine;
using namespace ECSEngine::Reflection;

const size_t STREAM_TYPE_ALLOCATION = 10;

// -----------------------------------------------------------------------------------------------------------------------------

void CreateModuleSettings(EditorState* editor_state, unsigned int module_index)
{
	//unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);

	//unsigned int instance_count = editor_state->module_reflection->CreateTypesAndInstancesForHierarchy(hierarchy_index, true);
	//ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, instance_indices, instance_count);
	//editor_state->module_reflection->GetHierarchyInstances(hierarchy_index, instance_indices);

	//if (instance_indices.size > 0) {
	//	// For each instance, create a corresponding void* data buffer
	//	ReflectionManager* reflection_manager = editor_state->module_reflection->reflection;

	//	// Allocate a buffer for the instance pointers
	//	EditorModule* editor_module = editor_state->project_modules->buffer + module_index;
	//	editor_module->reflected_settings.Initialize(editor_state->editor_allocator, editor_module->reflected_settings.NextCapacity(instance_indices.size));

	//	for (size_t index = 0; index < instance_indices.size; index++) {
	//		const char* type_name = editor_state->module_reflection->GetInstance(instance_indices[index]).type_name;
	//		ReflectionType type = reflection_manager->GetType(type_name);
	//		size_t type_size = GetTypeByteSize(type);

	//		// Allocate the memory for an instance of the type
	//		void* instance_memory = editor_state->editor_allocator->Allocate(type_size);
	//		// Set the memory to 0
	//		memset(instance_memory, 0, type_size);

	//		// This will alias the name of the UI reflection type and that's fine
	//		editor_state->project_modules->buffer[module_index].reflected_settings.Insert(instance_memory, type_name);

	//		// If it has default values, make sure to initialize them
	//		for (size_t field_index = 0; field_index < type.fields.size; field_index++) {
	//			if (type.fields[field_index].info.has_default_value) {
	//				memcpy(
	//					function::OffsetPointer(instance_memory, type.fields[field_index].info.pointer_offset),
	//					&type.fields[field_index].info.default_bool,
	//					type.fields[field_index].info.byte_size
	//				);
	//			}

	//			ReflectionStreamFieldType stream_type = type.fields[field_index].info.stream_type;
	//			if (stream_type == ReflectionStreamFieldType::Stream) {
	//				void* memory_allocation = editor_state->editor_allocator->Allocate(type.fields[field_index].info.stream_byte_size * STREAM_TYPE_ALLOCATION);
	//				// Copy the pointer into that location
	//				memcpy(function::OffsetPointer(instance_memory, type.fields[field_index].info.pointer_offset), &memory_allocation, sizeof(void*));
	//				// Set the size to 0
	//				memset(function::OffsetPointer(instance_memory, type.fields[field_index].info.pointer_offset + 8), 0, sizeof(size_t));
	//			}
	//			else if (stream_type == ReflectionStreamFieldType::CapacityStream || stream_type == ReflectionStreamFieldType::ResizableStream) {
	//				// It works for both the capacity stream and the resizable stream

	//				void* memory_allocation = editor_state->editor_allocator->Allocate(type.fields[field_index].info.stream_byte_size * STREAM_TYPE_ALLOCATION);
	//				// Copy the pointer into that location
	//				memcpy(function::OffsetPointer(instance_memory, type.fields[field_index].info.pointer_offset), &memory_allocation, sizeof(void*));
	//				// Set the size to 0
	//				memset(function::OffsetPointer(instance_memory, type.fields[field_index].info.pointer_offset + 8), 0, sizeof(unsigned int));

	//				unsigned int capacity = STREAM_TYPE_ALLOCATION;
	//				// Set the capacity
	//				memcpy(function::OffsetPointer(instance_memory, type.fields[field_index].info.pointer_offset + 12), &capacity, sizeof(capacity));
	//			}
	//		}

	//		UIReflectionInstance* instance = editor_state->module_reflection->GetInstancePtr(instance_indices[index]);
	//		editor_state->module_reflection->BindInstancePtrs(instance, instance_memory);

	//		// Must rebind capacity for stream types - except the capacity stream
	//		for (size_t field_index = 0; field_index < type.fields.size; field_index++) {
	//			ReflectionStreamFieldType stream_type = type.fields[index].info.stream_type;
	//			if (stream_type == ReflectionStreamFieldType::Stream) {
	//				UIReflectionBindStreamCapacity bind_capacity;
	//				bind_capacity.field_name = type.fields[index].name;
	//				bind_capacity.capacity = STREAM_TYPE_ALLOCATION;
	//				editor_state->module_reflection->BindInstanceStreamCapacity(instance, { &bind_capacity, 1 });
	//			}
	//		}
	//	}
	//}
}

// -----------------------------------------------------------------------------------------------------------------------------

void DestroyModuleSettings(EditorState* editor_state, unsigned int module_index)
{
	//EDITOR_STATE(editor_state);

	//EditorModule* editor_module = editor_state->project_modules->buffer + module_index;
	//unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	//ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, type_indices, editor_state->module_reflection->GetTypeCount());
	//editor_state->module_reflection->GetHierarchyTypes(hierarchy_index, type_indices);

	//if (type_indices.size > 0) {
	//	// Must deallocate every instance
	//	for (size_t index = 0; index < type_indices.size; index++) {
	//		ReflectionType type = editor_state->module_reflection->reflection->GetType(editor_state->module_reflection->GetType(type_indices[index]).name);
	//		const void* instance_memory = editor_module->reflected_settings.GetValue(type.name);

	//		for (size_t field_index = 0; field_index < type.fields.size; field_index++) {
	//			ReflectionStreamFieldType stream_type = type.fields[field_index].info.stream_type;
	//			void* data_to_deallocate = *(void**)function::OffsetPointer(instance_memory, type.fields[field_index].info.pointer_offset);

	//			// Deallocate the streams of data, if any
	//			if (stream_type == ReflectionStreamFieldType::Stream || stream_type == ReflectionStreamFieldType::CapacityStream || stream_type == ReflectionStreamFieldType::ResizableStream) {
	//				editor_allocator->Deallocate(data_to_deallocate);
	//			}
	//		}

	//		editor_allocator->Deallocate(instance_memory);
	//	}

	//	if (editor_module->reflected_settings.GetAllocatedBuffer() != nullptr) {
	//		// Deallocate the buffer for the reflected settings
	//		editor_allocator->Deallocate(editor_module->reflected_settings.GetAllocatedBuffer());
	//		editor_module->reflected_settings = HashTableDefault<void*>();
	//	}

	//	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	//	// Destroy all instances and types
	//	editor_state->module_reflection->DestroyAllFromFolderHierarchy(hierarchy_index);
	//}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool LoadModuleSettings(EditorState* editor_state, unsigned int module_index)
{
	//// Use text deserialization
	//// Cannot use the deserialization directly from file - must manually walk through all types and check that it exists
	//unsigned int type_count = editor_state->module_reflection->GetTypeCount();
	//ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, indices, type_count);

	//unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	//editor_state->module_reflection->GetHierarchyTypes(hierarchy_index, indices);

	//if (indices.size > 0) {
	//	ECS_STACK_CAPACITY_STREAM_DYNAMIC(Reflection::ReflectionType, module_types, indices.size);
	//	ECS_STACK_CAPACITY_STREAM_DYNAMIC(void*, instance_memory, indices.size);

	//	// Get the reflection types in order to serialize
	//	Reflection::ReflectionManager* reflection = editor_state->module_reflection->reflection;
	//	for (size_t index = 0; index < indices.size; index++) {
	//		module_types[index] = reflection->GetType(editor_state->module_reflection->GetType(indices[index]).name);
	//		instance_memory[index] = editor_state->project_modules->buffer[module_index].reflected_settings.GetValue(module_types[index].name);
	//	}
	//	module_types.size = indices.size;

	//	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_setting_path, 512);
	//	GetModuleSettingsFilePath(editor_state, module_index, absolute_setting_path);
	//	Stream<void> file_data = ReadWholeFileText(absolute_setting_path, GetAllocatorPolymorphic(editor_state->editor_allocator));
	//	if (file_data.buffer == nullptr) {
	//		return false;
	//	}

	//	// Walk through the file, ignore types that are not reflected
	//	uintptr_t file_ptr = (uintptr_t)file_data.buffer;
	//	const char* file_char = (const char*)file_ptr;

	//	auto get_type_index = [module_types](Stream<char> name) {
	//		for (size_t index = 0; index < module_types.size; index++) {
	//			if (function::CompareStrings(ToStream(module_types[index].name), name)) {
	//				return index;
	//			}
	//		}
	//		return (size_t)-1;
	//	};

	//	char _pointer_data_stream[ECS_KB * 2];
	//	CapacityStream<void> pointer_data_stream(_pointer_data_stream, 0, ECS_KB * 2);

	//	while (file_ptr - (uintptr_t)file_data.buffer < file_data.size) {
	//		file_char = (const char*)file_ptr;
	//		const char* new_line = strchr(file_char, '\n');
	//		if (new_line == nullptr) {
	//			return false;
	//		}

	//		file_ptr = (uintptr_t)new_line + 1;
	//		Stream<char> type_name(file_char, new_line - file_char);

	//		size_t name_index = get_type_index(type_name);
	//		if (name_index != -1) {
	//			// Deallocate the streams for that instance
	//			for (size_t field_index = 0; field_index < module_types[name_index].fields.size; field_index++) {
	//				ReflectionStreamFieldType stream_type = module_types[name_index].fields[field_index].info.stream_type;
	//				const void** field_memory = (const void**)function::OffsetPointer(instance_memory[name_index], module_types[name_index].fields[field_index].info.pointer_offset);

	//				if (stream_type == ReflectionStreamFieldType::Stream || stream_type == ReflectionStreamFieldType::CapacityStream || stream_type == ReflectionStreamFieldType::ResizableStream) {
	//					editor_state->editor_allocator->Deallocate(*field_memory);
	//				}
	//			}

	//			// Deserialize the type
	//			ECS_TEXT_DESERIALIZE_STATUS deserialize_status = TextDeserialize(module_types[name_index], instance_memory[name_index], pointer_data_stream, file_ptr);

	//			UIReflectionInstance* instance = editor_state->module_reflection->GetInstancePtr(module_types[name_index].name);
	//			for (size_t field_index = 0; field_index < module_types[name_index].fields.size; field_index++) {
	//				ReflectionStreamFieldType stream_type = module_types[name_index].fields[field_index].info.stream_type;
	//				void** field_memory = (void**)function::OffsetPointer(instance_memory[name_index], module_types[name_index].fields[field_index].info.pointer_offset);
	//				void* old_memory = *field_memory;
	//				size_t stream_size = 0;

	//				if (stream_type == ReflectionStreamFieldType::Stream) {
	//					stream_size = *(size_t*)function::OffsetPointer(field_memory, 8);
	//					*field_memory = editor_state->editor_allocator->Allocate((stream_size + 10) * module_types[name_index].fields[field_index].info.stream_byte_size);

	//					UIReflectionBindStreamCapacity bind_capacity;
	//					bind_capacity.field_name = module_types[name_index].fields[field_index].name;
	//					bind_capacity.capacity = stream_size + 10;
	//					editor_state->module_reflection->BindInstanceStreamCapacity(instance, { &bind_capacity, 1 });

	//					bind_capacity.capacity = stream_size;
	//					editor_state->module_reflection->BindInstanceStreamSize(instance, { &bind_capacity, 1 });

	//					UIReflectionBindStreamBuffer bind_buffer;
	//					bind_buffer.field_name = module_types[name_index].fields[field_index].name;
	//					bind_buffer.new_buffer = *field_memory;
	//					editor_state->module_reflection->BindInstanceStreamBuffer(instance, { &bind_buffer, 1 });

	//					memcpy(*field_memory, old_memory, stream_size * module_types[name_index].fields[field_index].info.stream_byte_size);
	//				}
	//				else if (stream_type == ReflectionStreamFieldType::CapacityStream || stream_type == ReflectionStreamFieldType::ResizableStream) {
	//					stream_size = *(unsigned int*)function::OffsetPointer(field_memory, 8);
	//					*field_memory = editor_state->editor_allocator->Allocate((stream_size + 10) * module_types[name_index].fields[field_index].info.stream_byte_size);

	//					CapacityStream<void>* stream = (CapacityStream<void>*)field_memory;
	//					stream->capacity = stream_size + 10;

	//					memcpy(*field_memory, old_memory, stream_size * module_types[name_index].fields[field_index].info.stream_byte_size);
	//				}
	//			}
	//		}

	//		const char* end_serialize_string = strstr(new_line + 1, ECS_END_TEXT_SERIALIZE_STRING);
	//		if (end_serialize_string == nullptr) {
	//			break;
	//		}
	//		// Add one more than the string size so as to get to the next line
	//		file_ptr = (uintptr_t)end_serialize_string + strlen(ECS_END_TEXT_SERIALIZE_STRING);
	//	}

	//	// Deallocate the file content
	//	editor_state->editor_allocator->Deallocate(file_data.buffer);
	//}
	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool SaveModuleSettings(EditorState* editor_state, unsigned int module_index)
{
	//// Use text serialization
	//// Cannot use the serialization directly into the file - must manually walk through all types and serialize it
	//// into a memory buffer
	//unsigned int type_count = editor_state->module_reflection->GetTypeCount();
	//ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, indices, type_count);

	//unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	//editor_state->module_reflection->GetHierarchyTypes(hierarchy_index, indices);

	//if (indices.size > 0) {
	//	ECS_STACK_CAPACITY_STREAM_DYNAMIC(Reflection::ReflectionType, module_types, indices.size);
	//	ECS_STACK_CAPACITY_STREAM_DYNAMIC(void*, instance_memory, indices.size);

	//	// Get the reflection types in order to serialize
	//	Reflection::ReflectionManager* reflection = editor_state->module_reflection->reflection;
	//	for (size_t index = 0; index < indices.size; index++) {
	//		module_types[index] = reflection->GetType(editor_state->module_reflection->GetType(indices[index]).name);
	//		instance_memory[index] = editor_state->project_modules->buffer[module_index].reflected_settings.GetValue(module_types[index].name);

	//		// Update the stream sizes for the stream types
	//		for (size_t field_index = 0; field_index < module_types[index].fields.size; field_index++) {
	//			ReflectionStreamFieldType stream_type = module_types[index].fields[field_index].info.stream_type;
	//			void** stream_data = (void**)function::OffsetPointer(instance_memory[index], module_types[index].fields[field_index].info.pointer_offset);

	//			if (stream_type == ReflectionStreamFieldType::Stream) {
	//				UIReflectionBindStreamCapacity size;
	//				size.field_name = module_types[index].fields[field_index].name;
	//				editor_state->module_reflection->GetInstanceStreamSizes(module_types[index].name, { &size, 1 });

	//				Stream<void>* stream = (Stream<void>*)stream_data;
	//				stream->size = size.capacity;
	//			}
	//		}
	//	}

	//	// Now determine how much memory is needed in order to allocate a single buffer and serialize into it
	//	size_t buffer_size = 0;
	//	for (size_t index = 0; index < indices.size; index++) {
	//		// The name of the type will also be serialized
	//		buffer_size += strlen(module_types[index].name) + 1;

	//		// Determine which instance memory we have
	//		buffer_size += TextSerializeSize(module_types[index], instance_memory[index]);
	//	}

	//	// Allocate a buffer of the corresponding size
	//	void* buffer_allocation = editor_state->editor_allocator->Allocate(buffer_size);

	//	// Write the data into the buffer
	//	uintptr_t buffer = (uintptr_t)buffer_allocation;
	//	for (size_t index = 0; index < indices.size; index++) {
	//		// Write the name first
	//		size_t name_size = strlen(module_types[index].name);
	//		memcpy((void*)buffer, module_types[index].name, name_size * sizeof(char));
	//		buffer += name_size;

	//		char* new_line = (char*)buffer;
	//		*new_line = '\n';
	//		buffer += 1;

	//		// Now write the type data
	//		TextSerialize(module_types[index], instance_memory[index], buffer);
	//	}

	//	size_t difference = buffer - (uintptr_t)buffer_allocation;
	//	ECS_ASSERT(difference <= buffer_size);

	//	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_setting_path, 512);
	//	GetModuleSettingsFilePath(editor_state, module_index, absolute_setting_path);

	//	// Now write the buffer to the file
	//	bool success = WriteBufferToFileText(absolute_setting_path, { buffer_allocation, difference }) == ECS_FILE_STATUS_OK;

	//	// Deallocate the buffer
	//	editor_state->editor_allocator->Deallocate(buffer_allocation);
	//	return success;
	//}

	return true;
}

// -----------------------------------------------------------------------------------------------------------------------------

void SetModuleDefaultSettings(EditorState* editor_state, unsigned int module_index)
{
	/*unsigned int type_count = editor_state->module_reflection->GetTypeCount();
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, indices, type_count);

	unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(editor_state, module_index);
	editor_state->module_reflection->GetHierarchyInstances(hierarchy_index, indices);

	if (indices.size > 0) {
		for (size_t index = 0; index < indices.size; index++) {
			UIReflectionInstance* ui_instance = editor_state->module_reflection->GetInstancePtr(indices[index]);
			ReflectionType type = editor_state->module_reflection->reflection->GetType(ui_instance->name);
			void* instance_memory = editor_state->project_modules->buffer[module_index].reflected_settings.GetValue(ui_instance->name);

			for (size_t field_index = 0; field_index < type.fields.size; field_index++) {
				void* field_memory = function::OffsetPointer(instance_memory, type.fields[field_index].info.pointer_offset);
				
				ReflectionStreamFieldType stream_type = type.fields[field_index].info.stream_type;
				if (stream_type == ReflectionStreamFieldType::Basic) {
					if (type.fields[field_index].info.has_default_value) {
						memcpy(field_memory, &type.fields[field_index].info.default_bool, type.fields[field_index].info.byte_size);
					}
					else {
						memset(field_memory, 0, type.fields[field_index].info.byte_size);
					}
				}
				else if (stream_type == ReflectionStreamFieldType::Stream) {
					UIReflectionBindStreamCapacity stream_capacity;
					stream_capacity.field_name = type.fields[field_index].name;
					stream_capacity.capacity = 0;
					editor_state->module_reflection->BindInstanceStreamSize(ui_instance, { &stream_capacity, 1 });
				}
				else if (stream_type == ReflectionStreamFieldType::CapacityStream || stream_type == ReflectionStreamFieldType::ResizableStream) {
					unsigned int* size = (unsigned int*)function::OffsetPointer(field_memory, 8);
					*size = 0;
				}
			}
		}
	}*/
}

// -----------------------------------------------------------------------------------------------------------------------------

void ChangeModuleSettings(EditorState* editor_state, Stream<wchar_t> new_path, unsigned int module_index)
{
	//// Deallocate the current one if not nullptr
	//EditorModule* editor_module = editor_state->project_modules->buffer + module_index;
	//if (editor_module->current_settings_path.buffer != nullptr) {
	//	editor_state->editor_allocator->Deallocate(editor_module->current_settings_path.buffer);
	//}

	//if (new_path.size > 0) {
	//	editor_module->current_settings_path = function::StringCopy(editor_state->editor_allocator, new_path);

	//	bool success = SaveModuleFile(editor_state);
	//	if (!success) {
	//		EditorSetConsoleWarn(ToStream("Could not save the project module file after changing module settings."));
	//	}
	//}
	//else {
	//	editor_module->current_settings_path.buffer = nullptr;
	//	editor_module->current_settings_path.size = 0;
	//}
}

// -----------------------------------------------------------------------------------------------------------------------------

void SwitchModuleSettings(EditorState* editor_state, Stream<wchar_t> new_path, unsigned int module_index)
{
	SaveModuleSettings(editor_state, module_index);
	DestroyModuleSettings(editor_state, module_index);
	ChangeModuleSettings(editor_state, new_path, module_index);
	CreateModuleSettings(editor_state, module_index);
	SaveModuleSettings(editor_state, module_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetModuleSettingsFilePath(const EditorState* editor_state, unsigned int module_index, CapacityStream<wchar_t>& path)
{
	/*GetProjectConfigurationModuleFolder(editor_state, path);
	path.Add(ECS_OS_PATH_SEPARATOR);
	path.AddStream(editor_state->project_modules->buffer[module_index].library_name);
	path.Add(L'_');
	path.AddStreamSafe(editor_state->project_modules->buffer[module_index].current_settings_path);
	path[path.size] = L'\0';*/
}

// -----------------------------------------------------------------------------------------------------------------------------
