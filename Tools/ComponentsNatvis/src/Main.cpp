#include <stdio.h>
#include "ECSEngineContainersCommon.h"
#include "ECSEngineReflection.h"
#include "ECSEngineUtilities.h"
#include "ECSEngineComponentsAll.h"

using namespace ECSEngine;
using namespace ECSEngine::Reflection;

int main(int argc, const char** argv) {
	if (argc != 3) {
		fprintf(stderr, "Incorrect usage. Call this command with 2 parameters:\n\tthe location of the folder hierarchy to inspect\n\tthe output file location\nIf the given output file already exists, then it will open it and if it finds the definition for the Component entry it will delete it and regenerate it.");
		return 1;
	}

	// Ensure that the paths being passed in are not too long
	size_t folder_hierarchy_length = strlen(argv[1]);
	if (folder_hierarchy_length > 512) {
		fprintf(stderr, "The provided folder path is invalid -- too long");
		return 1;
	}

	size_t output_file_length = strlen(argv[2]);
	if (output_file_length > 512) {
		fprintf(stderr, "The provided output file is invalid -- too long");
		return 1;
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, folder_hierarchy, 512);
	ConvertASCIIToWide(folder_hierarchy, Stream<char>{ argv[1], folder_hierarchy_length });
	
	// Create the folder hierarchy and process it
	ReflectionManager reflection_manager(ECS_MALLOC_ALLOCATOR);
	reflection_manager.CreateFolderHierarchy(folder_hierarchy);
	ECS_STACK_CAPACITY_STREAM(char, error_message, ECS_KB * 2);
	if (!reflection_manager.ProcessFolderHierarchy(0, &error_message)) {
		fprintf(stderr, "Failed to reflect the folder hierarchy");
		return 1;
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, output_file, 512);
	ConvertASCIIToWide(output_file, { argv[2], output_file_length });

	// Use this to know whether or not we should create a new file or use the existing one
	bool does_output_file_exist = ExistsFileOrFolder(output_file);

	// When the file already exists, create a temporary of it, and restore it in case the write fails
	ECS_STACK_CAPACITY_STREAM(wchar_t, renamed_file_path, 600);
	renamed_file_path.CopyOther(output_file);
	renamed_file_path.AddStreamAssert(L".temp");

	bool is_restore_renamed_file_created = false;
	bool should_delete_restore_file = false;
	auto restore_renamed_file = StackScope([&]() {
		if (is_restore_renamed_file_created) {
			if (should_delete_restore_file) {
				// Delete the temporary file
				RemoveFile(renamed_file_path);
			}
			else {
				if (!RemoveFile(output_file)) {
					fprintf(stderr, "Failed to remove existing file when trying to restore the temporary");
				}

				if (!RenameFile(renamed_file_path, PathFilenameBoth(output_file))) {
					fprintf(stderr, "Failed to restore the initial file from the temporary");
				}
			}
		}
	});

	ECS_FILE_HANDLE file = -1;
	ResizableStream<char> file_contents(ECS_MALLOC_ALLOCATOR, 0);
	size_t insert_location = 0;
	
	if (does_output_file_exist) {
		if (OpenFile(output_file, &file, ECS_FILE_ACCESS_READ_WRITE | ECS_FILE_ACCESS_TEXT, &error_message) != ECS_FILE_STATUS_OK) {
			NULL_TERMINATE(error_message);
			fprintf(stderr, "Failed to open output file. Reason: %s", error_message.buffer);
			return 1;
		}

		// If the file exists, read the entire contents of the file
		size_t file_byte_size = GetFileByteSize(file);
		file_contents.Resize(file_byte_size);
		file_contents.size = file_byte_size;

		size_t read_count = ReadFromFile(file, file_contents.ToStream());
		if (read_count == -1) {
			fprintf(stderr, "Failed to read output file contents");
			return 1;
		}

		// The size of the contents might be less due to the fact that this is a text file
		file_contents.size = read_count;

		// Create a temporary with the current files content
		if (WriteBufferToFileBinary(renamed_file_path, file_contents.ToStream()) != ECS_FILE_STATUS_OK) {
			fprintf(stderr, "Failed to create the temporary file");
			return 1;
		}

		is_restore_renamed_file_created = true;
		// Set this to true, in case the resize fails, we should remove the temporary
		should_delete_restore_file = true;

		// Resize the current file to be empty
		if (!ResizeFile(file, 0)) {
			fprintf(stderr, "Failed to resize the output file");
			return 1;
		}

		// Now the delete flag must be set to false, since the resize was performed
		should_delete_restore_file = false;
		
		if (!SetFileCursorBool(file, 0, ECS_FILE_SEEK_BEG)) {
			fprintf(stderr, "Failed to move file cursor to the beginning");
			return 1;
		}

		Stream<char> component_natvis_tag = FindFirstToken(file_contents, "<Type Name=\"ECSEngine::Component\">");
		if (component_natvis_tag.size > 0) {
			Stream<char> closing_tag = "</Type>";
			// Remove this natvis tag, but find its end first
			Stream<char> end_component_natvis_tag = FindFirstToken(component_natvis_tag, closing_tag);
			if (end_component_natvis_tag.size == 0) {
				fprintf(stderr, "Found component natvis entry, but did not find its enclosing tag");
				return 1;
			}

			// Remove everything
			file_contents.Remove(component_natvis_tag.buffer - file_contents.buffer, component_natvis_tag.size - end_component_natvis_tag.size + closing_tag.size);
			insert_location = component_natvis_tag.buffer - file_contents.buffer;
		}
		else {
			// Add this entry at the end
			Stream<char> end_token = "</AutoVisualizer>";
			Stream<char> content_end_token = FindFirstToken(file_contents, end_token);
			if (content_end_token.size == 0) {
				fprintf(stderr, "Failed to find end token in output file");
				return 1;
			}

			insert_location = content_end_token.buffer - file_contents.buffer;
		}
	}
	else {
		if (FileCreate(output_file, &file, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TEXT, ECS_FILE_CREATE_READ_WRITE, &error_message) != ECS_FILE_STATUS_OK) {
			NULL_TERMINATE(error_message);
			fprintf(stderr, "Failed to create output file. Reason: %s", error_message.buffer);
			return 1;
		}

		// Create the default file contents
		file_contents.AddStream("<?xml version=\"1.0\" encoding=\"utf - 8\"?>\n");
		file_contents.AddStream("<AutoVisualizer xmlns = \"http://schemas.microsoft.com/vstudio/debugger/natvis/2010\">\n\t");
		// The insert location should be here
		insert_location = file_contents.size;
		file_contents.AddStream("</AutoVisualizer>");
	}

	// Construct the new definition now
	ResizableStream<char> new_definition(ECS_MALLOC_ALLOCATOR, ECS_KB * 4);
	new_definition.AddStream("\t<Type Name=\"ECSEngine::Component\">\n");

	// For each component value, determine all of its matches, since we want to have the natvis check the condition only once
	struct ComponentMatches {
		const ReflectionType* types[ECS_COMPONENT_TYPE_COUNT];
	};
	HashTable<ComponentMatches, Component, HashFunctionPowerOfTwo> component_matches;
	component_matches.Initialize(ECS_MALLOC_ALLOCATOR, ECS_KB);

	reflection_manager.type_definitions.ForEachConst([&](const ReflectionType& type, ResourceIdentifier identifier) -> void {
		ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(&type);
		if (component_type != ECS_COMPONENT_TYPE_COUNT) {
			Component component_value = GetReflectionTypeComponent(&type);
			if (component_value != Component::Invalid()) {
				ComponentMatches* component_match = component_matches.TryGetValuePtr(GetReflectionTypeComponent(&type));
				if (component_match == nullptr) {
					ComponentMatches default_match;
					ZeroOut(&default_match);
					unsigned int table_insert_location = -1;
					component_matches.InsertDynamic(ECS_MALLOC_ALLOCATOR, default_match, component_value, table_insert_location);

					component_match = component_matches.GetValuePtrFromIndex(table_insert_location);
				}

				component_match->types[component_type] = &type;
			}
		}
	});

	component_matches.ForEachConst([&](const ComponentMatches& matches, Component component) -> void {
		// The format is this one for the line
		// <DisplayString Condition="value == 0">Unique|Shared|Global ({value})</DisplayString>

		new_definition.AddStream("\t\t<DisplayString Condition=\"value == ");
		ConvertIntToChars(new_definition, component.value);
		new_definition.AddStream("\">");

		bool is_first_valid_entry = true;
		for (size_t index = 0; index < ECS_COMPONENT_TYPE_COUNT; index++) {
			if (matches.types[index] != nullptr) {
				if (!is_first_valid_entry) {
					new_definition.AddStream(" | ");
				}
				else {
					is_first_valid_entry = false;
				}
				new_definition.AddStream(matches.types[index]->name);

				if (index == ECS_COMPONENT_UNIQUE) {
					new_definition.AddStream("(U)");
				}
				else if (index == ECS_COMPONENT_SHARED) {
					new_definition.AddStream("(S)");
				}
				else if (index == ECS_COMPONENT_GLOBAL) {
					new_definition.AddStream("(G)");
				}
			}
		}
		new_definition.AddStream(" ({value})</DisplayString>\n");
	});

	new_definition.AddStream("\t</Type>\n");

	// Insert this string into the file contents
	file_contents.Insert(insert_location, new_definition);

	// Write the contents now
	if (!WriteFile(file, file_contents.ToStream())) {
		fprintf(stderr, "Failed to write the output to the file");
		return 1;
	}

	// Ensure that the temporary file is deleted
	should_delete_restore_file = true;
	CloseFile(file);

	return 0;
}