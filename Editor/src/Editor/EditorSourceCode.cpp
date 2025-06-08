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

// For the output of the git command, it will extract the source code branch name and commit hash
// And update the fields in the editor state. In case an error is encountered, then it will output
// A warning to the console
static void ExtractProjectSourceCodeInformation(EditorState* editor_state, Stream<char> git_command_output) {
	// The output is of the form:
	// branch
	// hash
	// On separate lines, like they are shown here. If the HEAD is detached from a branch,
	// The first line will contain an error message starting fatal

	// We can deallocate the existing data here
	editor_state->source_code_branch_name.Deallocate(editor_state->EditorAllocator());
	editor_state->source_code_commit_hash.Deallocate(editor_state->EditorAllocator());

	Stream<char> first_line_break = FindFirstCharacter(git_command_output, '\n');
	if (first_line_break.size == 0) {
		EditorSetConsoleWarn("Unrecognized git command output. Source code information might be invalid.", ECS_CONSOLE_VERBOSITY_DETAILED);
	}
	else {
		Stream<char> second_line_break = FindFirstCharacter(first_line_break.AdvanceReturn(), '\n');
		if (second_line_break.size == 0) {
			EditorSetConsoleWarn("Unrecognized git command output. Source code information might be invalid.", ECS_CONSOLE_VERBOSITY_DETAILED);
		}
		else {
			// Both lines are valid. On Windows at least, there are additional \r characters
			// At the end of both the branch name and commit hash, probably because it outputs
			// Using a text format, so eliminate them.
			if (!first_line_break.StartsWith("fatal")) {
				editor_state->source_code_branch_name.InitializeAndCopy(
					editor_state->EditorAllocator(), 
					TrimWhitespaceEx(git_command_output.StartDifference(first_line_break)));
				while (editor_state->source_code_branch_name.size > 0 && editor_state->source_code_branch_name.Last() == '\r') {
					editor_state->source_code_branch_name.size--;
				}
			}
			editor_state->source_code_commit_hash.InitializeAndCopy(
				editor_state->EditorAllocator(), 
				TrimWhitespaceEx(first_line_break.AdvanceReturn().StartDifference(second_line_break))
			);
			while (editor_state->source_code_commit_hash.size > 0 && editor_state->source_code_commit_hash.Last() == '\r') {
				editor_state->source_code_commit_hash.size--;
			}
		}
	}
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
	OS::ProcessHandle git_process;
};

// Waits until the git shell command finishes execution and updates the info
static EDITOR_EVENT(MonitorGitInfoStatus) {
	MonitorGitInfoStatusData* data = (MonitorGitInfoStatusData*)_data;
	if (data->timer.GetDuration(ECS_TIMER_DURATION_MS) >= MONITOR_EXIT_MILLISECONDS) {
		// Exit
		data->git_process.Close();
		return false;
	}

	if (data->timer.GetDurationSinceMarker(ECS_TIMER_DURATION_MS) >= MONITOR_TICK_MILLISECONDS) {
		// Check to see if the process has finished
		Optional<bool> is_process_finished = data->git_process.CheckIsFinished();
		if (is_process_finished.has_value && is_process_finished.value) {
			ECS_STACK_CAPACITY_STREAM(char, command_output, 512);
			size_t command_bytes_count = data->git_process.ReadFromProcessBytes(command_output.buffer, command_output.capacity);
			if (command_bytes_count == -1) {
				// Failure, deallocate the process and fail
				EditorSetConsoleWarn("Failed to read git command output. Source code information might be incorrect.", ECS_CONSOLE_VERBOSITY_DETAILED);
			}
			else
			{
				command_output.size = command_bytes_count;
				ExtractProjectSourceCodeInformation(editor_state, command_output);
			}
			return false;
		}

		// Reset the marker
		data->timer.SetMarker();
	}
	// Continue waiting
	return true;
}

static void GitBranchAndCommitCommandLine(CapacityStream<wchar_t>& string) {
	string.CopyOther(L"powershell -Command \"echo $(git symbolic-ref --short HEAD) $(git rev-parse HEAD)\"");
}

void UpdateProjectSourceCodeInfo(EditorState* editor_state) {
	if (editor_state->powershell_executable_path.size == 0 || editor_state->source_code_git_directory.size == 0) {
		return;
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, git_command, 512);
	GitBranchAndCommitCommandLine(git_command);

	// The same options can be used for the git branch and git commit 
	OS::CreateProcessOptions create_process_options;
	create_process_options.create_pipe_failure_is_process_failure = true;
	create_process_options.create_read_pipe = true;
	create_process_options.starting_directory = editor_state->source_code_git_directory;

	Optional<OS::ProcessHandle> git_process = OS::CreateProcessWithHandle(editor_state->powershell_executable_path, git_command, create_process_options);
	if (git_process.has_value) {
		MonitorGitInfoStatusData monitor_data;
		monitor_data.git_process = git_process.value;
		monitor_data.timer.SetNewStart();
		// Delay the marker such that it gets triggered the first time it enters in the event
		monitor_data.timer.DelayMarker(-MONITOR_TICK_MILLISECONDS, ECS_TIMER_DURATION_MS);
		EditorAddEvent(editor_state, MonitorGitInfoStatus, &monitor_data, sizeof(monitor_data));
	}
	else {
		EditorSetConsoleWarn("Failed to query git in order to update the source code information. Could not launch the git process", ECS_CONSOLE_VERBOSITY_DETAILED);
	}
}

void TickProjectSourceCodeInfo(EditorState* editor_state) {
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_SOURCE_CODE_INFO, TICK_MILLISECONDS)) {
		// Check to see if there is an active update event. If it is, then don't add another one
		if (!EditorHasEvent(editor_state, MonitorGitInfoStatus)) {
			UpdateProjectSourceCodeInfo(editor_state);
		}
	}
}