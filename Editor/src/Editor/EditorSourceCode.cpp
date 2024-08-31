#include "editorpch.h"
#include "EditorState.h"

#define TICK_MILLISECONDS 2000
#define GIT_DIRECTORY L".git"

using namespace ECSEngine;

// TODO: Currently, the modules are located in a different location as opposed to the project.
// Use an exception for this case, in the future, maybe it is a good idea to move the modules
// Inside the project itself, such that they can be referenced relative to it.

// The git directory itself should not be included in the stored value

void DetermineProjectSourceCodeGitDirectory(EditorState* editor_state) {
	editor_state->source_code_git_directory.Deallocate(editor_state->EditorAllocator());

	Stream<wchar_t> git_directory_name = GIT_DIRECTORY;
	ECS_STACK_CAPACITY_STREAM(wchar_t, git_directory, 512);
	git_directory.CopyOther(editor_state->project_file->path);
	git_directory.AddAssert(ECS_OS_PATH_SEPARATOR);
	git_directory.AddStreamAssert(git_directory_name);
	if (ExistsFileOrFolder(git_directory)) {
		// Use this project specific git location
		editor_state->source_code_git_directory = editor_state->project_file->path.Copy(editor_state->EditorAllocator());
	}
	else {
		// Check the modules location. If all modules have the same root git directory, use it
		// Otherwise let the user know that we could not select one of it.
		ECS_STACK_CAPACITY_STREAM(wchar_t, candidate_git_directory, 512);

		unsigned int module_count = editor_state->project_modules->size;
		for (unsigned int index = 0; index < module_count; index++) {
			Stream<wchar_t> module_solution_path = editor_state->project_modules->buffer[index].solution_path;
			module_solution_path = PathParent(module_solution_path);
			
			while (module_solution_path.size > 0) {
				git_directory.CopyOther(module_solution_path);
				git_directory.AddAssert(ECS_OS_PATH_SEPARATOR);
				git_directory.AddStreamAssert(git_directory_name);
				if (ExistsFileOrFolder(git_directory)) {
					if (candidate_git_directory.size > 0) {
						if (git_directory != candidate_git_directory) {
							ECS_FORMAT_TEMP_STRING(message, "There are multiple source code git candidates. The first two are {#} and {#}.", git_directory, candidate_git_directory);
							EditorSetConsoleWarn(message);
							// Do not continue
							return;
						}
					}
					else {
						candidate_git_directory.CopyOther(git_directory);
					}
					break;
				}

				module_solution_path = PathParent(module_solution_path);
			}
		}

		// If there is a candidate, use it
		if (candidate_git_directory.size > 0) {
			candidate_git_directory = PathParent(candidate_git_directory);
			editor_state->source_code_git_directory = candidate_git_directory.Copy(editor_state->EditorAllocator());
		}
		else {
			// Warn the user that no git directory could be found
			EditorSetConsoleWarn("Could not locate git directory for the given project.");
		}
	}
}

void UpdateProjectSourceCodeInfo(EditorState* editor_state) {

}

void TickProjectSourceCodeInfo(EditorState* editor_state) {

}