#include "editorpch.h"
#include "EditorState.h"
#include "EditorEvent.h"

#define TICK_MILLISECONDS 5000
#define GIT_DIRECTORY L".git"

// Use ticking for the monitoring as well
#define MONITOR_TICK_MILLISECONDS 200
// If after this duration the git command has not finished, consider
// That the command failed
#define MONITOR_EXIT_MILLISECONDS 10000

static const Stream<wchar_t> BRANCH_FILE = L"branch.txt";
static const Stream<wchar_t> HASH_FILE = L"hash.txt";

using namespace ECSEngine;

// TODO: Currently, the modules are located in a different location as opposed to the project.
// Use an exception for this case, in the future, maybe it is a good idea to move the modules
// Inside the project itself, such that they can be referenced relative to it.

// The git directory itself (i.e. .git) should not be included in the stored value

// Extracts the current branch out of the output of the branch command, by referencing it directly in the given data
// Returns an empty string if an error has occured
static Stream<char> ExtractCurrentBranch(Stream<char> branch_file_data) {
	// Use as a search *_ where the underscore is a space
	// This will yield the active branch
	const Stream<char> indicator_string = "* ";

	Stream<char> active_branch_indicator = FindFirstToken(branch_file_data, indicator_string);
	if (active_branch_indicator.size > 0) {
		// Return everything until the end of the line
		Stream<char> line_end = FindFirstCharacter(active_branch_indicator, '\n');
		if (line_end.size > 0) {
			return active_branch_indicator.StartDifference(line_end).SliceAt(indicator_string.size);
		}
	}

	// An error has occured
	return {};
}

// Extracts the current commit hash out of the output from the git log command, by referencing the value directly in the given data
// Returns an empty string if an error has occured
static Stream<char> ExtractCommitHash(Stream<char> hash_file_data) {
	// Include the space as well
	const Stream<char> commit_word = "commit ";

	// This one is simple, on the first line, extract after the word commit until the end of the line
	if (hash_file_data.StartsWith(commit_word)) {
		Stream<char> first_line_end = FindFirstCharacter(hash_file_data, '\n');
		if (first_line_end.size > 0) {
			Stream<char> first_line = hash_file_data.StartDifference(first_line_end);
			return first_line.SliceAt(commit_word.size);
		}
	}

	// An error has occured
	return {};
}

void InitializeProjectSourceCode(EditorState* editor_state) {
	editor_state->source_code_git_directory = {};
	editor_state->source_code_branch_name = {};
	editor_state->source_code_commit_hash = {};
}

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

struct MonitorGitInfoStatusData {
	// This timer is used to check at certain intervals but also
	// To exit the event when a long enough period has passed
	Timer timer;
};

// Waits until the git shell command finishes execution and updates the info
static EDITOR_EVENT(MonitorGitInfoStatus) {
	MonitorGitInfoStatusData* data = (MonitorGitInfoStatusData*)_data;
	if (data->timer.GetDuration(ECS_TIMER_DURATION_MS) >= MONITOR_EXIT_MILLISECONDS) {
		// Exit
		return false;
	}

	if (data->timer.GetDurationSinceMarker(ECS_TIMER_DURATION_MS) >= MONITOR_TICK_MILLISECONDS) {
		// Check to see that both files exist
		ECS_STACK_CAPACITY_STREAM(wchar_t, branch_file_path, 512);
		branch_file_path.CopyOther(editor_state->source_code_git_directory);
		branch_file_path.AddAssert(ECS_OS_PATH_SEPARATOR);
		branch_file_path.AddStreamAssert(BRANCH_FILE);

		if (ExistsFileOrFolder(branch_file_path)) {
			ECS_STACK_CAPACITY_STREAM(wchar_t, hash_file_path, 512);
			hash_file_path.CopyOther(editor_state->source_code_git_directory);
			hash_file_path.AddAssert(ECS_OS_PATH_SEPARATOR);
			hash_file_path.AddStreamAssert(HASH_FILE);
			
			if (ExistsFileOrFolder(hash_file_path)) {
				// Both files exist, try reading from them
				ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);

				Stream<char> branch_file_data = ReadWholeFileText(branch_file_path, &stack_allocator);
				if (branch_file_data.size > 0) {
					Stream<char> hash_file_data = ReadWholeFileText(hash_file_path, &stack_allocator);
					if (hash_file_data.size > 0) {
						Stream<char> current_branch = ExtractCurrentBranch(branch_file_data);
						if (current_branch.size > 0) {
							Stream<char> current_commit_hash = ExtractCommitHash(hash_file_data);
							if (current_commit_hash.size > 0) {
								// Update the editor state
								editor_state->source_code_branch_name.Deallocate(editor_state->EditorAllocator());
								editor_state->source_code_commit_hash.Deallocate(editor_state->EditorAllocator());

								editor_state->source_code_branch_name = current_branch.Copy(editor_state->EditorAllocator());
								editor_state->source_code_commit_hash = current_commit_hash.Copy(editor_state->EditorAllocator());
							}
						}

						// Both files are present, can exit
						// Before that, remove both files
						if (!RemoveFile(branch_file_path)) {
							EditorSetConsoleWarn("Could not remove the source code branch file after querying git.", ECS_CONSOLE_VERBOSITY_DETAILED);
						}
						if (!RemoveFile(hash_file_path)) {
							EditorSetConsoleWarn("Could not remove the source code hash file after querying git.", ECS_CONSOLE_VERBOSITY_DETAILED);
						}

						return false;
					}
					
				}
			}
		}

		// Reset the marker
		data->timer.SetMarker();
	}
	// Continue waiting
	return true;
}

// The branch file and the hash file must not have whitespaces in them
static void GitCommandString(CapacityStream<wchar_t>& string, Stream<wchar_t> branch_file, Stream<wchar_t> hash_file) {
	string.CopyOther(L"git branch > ");
	string.AddStreamAssert(branch_file);
	string.AddStreamAssert(L" && git log -1 > ");
	string.AddStreamAssert(hash_file);
}

static void GitBranchFileCommandLine(CapacityStream<wchar_t>& string) {
	string.CopyOther(L"git branch");
}

static void GitBranchHashCommandLine(CapacityStream<wchar_t>& string) {

}

void UpdateProjectSourceCodeInfo(EditorState* editor_state) {
	if (editor_state->source_code_git_directory.size == 0) {
		return;
	}

	// Remove both files, if they exist already such that we don't read
	// previous data
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_file_path, 512);
	absolute_file_path.CopyOther(editor_state->source_code_git_directory);
	absolute_file_path.AddAssert(ECS_OS_PATH_SEPARATOR);
	absolute_file_path.AddStreamAssert(BRANCH_FILE);
	// Do not assert that it succeeded, since it may not have been there
	RemoveFile(absolute_file_path);
	
	absolute_file_path.size = editor_state->source_code_git_directory.size;
	absolute_file_path.AddAssert(ECS_OS_PATH_SEPARATOR);
	absolute_file_path.AddStreamAssert(HASH_FILE);
	// Do not assert
	RemoveFile(absolute_file_path);

	// Run the git command which returns the info we need
	// The git branch command will return the active command, while git log -1 returns the last commit hash
	/*ECS_STACK_CAPACITY_STREAM(wchar_t, shell_command, 512);
	GitCommandString(shell_command, BRANCH_FILE, HASH_FILE);*/

	//OS::CreateProcessWithHandle("git", )
	//
	//bool success = OS::ShellRunCommand(shell_command, editor_state->source_code_git_directory);
	//if (!success) {
	//	// Emit a warning
	//	EditorSetConsoleWarn("Failed to update source code branch and hash info. Could not launch shell command.", ECS_CONSOLE_VERBOSITY_DETAILED);
	//}
	//else {
	//	// Add an event to wait for the execution to finish
	//	MonitorGitInfoStatusData monitor_data;
	//	monitor_data.timer.SetNewStart();
	//	// Delay the marker such that it gets triggered the first time it enters in the event
	//	monitor_data.timer.DelayMarker(-MONITOR_TICK_MILLISECONDS, ECS_TIMER_DURATION_MS);
	//	EditorAddEvent(editor_state, MonitorGitInfoStatus, &monitor_data, sizeof(monitor_data));
	//}
}

void TickProjectSourceCodeInfo(EditorState* editor_state) {
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_SOURCE_CODE_INFO, TICK_MILLISECONDS)) {
		// Check to see if there is an active update event. If it is, then don't add another one
		if (!EditorHasEvent(editor_state, MonitorGitInfoStatus)) {
			UpdateProjectSourceCodeInfo(editor_state);
		}
	}
}