#include "editorpch.h"
#include "DeterminismTest.h"
#include "../Sandbox/Sandbox.h"
#include "../Sandbox/SandboxReplay.h"
#include "../UI/SandboxUI.h"

Optional<InputStateDeterminismTestRunData> InstantiateInputStateDeterminismTest(EditorState* editor_state, const InputStateDeterminismTestOptions& options)
{
    Optional<InputStateDeterminismTestRunData> return_value;
    
    // Returns a valid sandbox handle if it succeeded, else -1
    auto initialize_sandbox = [editor_state](Stream<wchar_t> replay_file, EDITOR_SANDBOX_RECORDING_TYPE recording_type) -> unsigned int {
        unsigned int sandbox_handle = CreateSandboxTemporary(editor_state);

        // Allow the sandbox to be run/paused/stepped. By default, temporary sandboxes cannot do that
        SetSandboxRunStates(editor_state, sandbox_handle, true, true, true);

        // Set the replay file
        EDITOR_SANDBOX_RECORDING_TYPE input_recording_type = SetSandboxReplay(editor_state, sandbox_handle, replay_file, true);
        if (input_recording_type != recording_type) {
            if (input_recording_type != EDITOR_SANDBOX_RECORDING_TYPE_COUNT) {
                Stream<char> recording_string;
                switch (recording_type) {
                case EDITOR_SANDBOX_RECORDING_INPUT:
                    recording_string = "an input";
                    break;
                case EDITOR_SANDBOX_RECORDING_STATE:
                    recording_string = "a state";
                    break;
                default:
                    ECS_ASSERT(false, "Unimplemented code path");
                }

                // If this is a valid type, but not the same as the recording type, inform the user
                EDITOR_LOG_FORMAT_ERROR("The provided replay file {#} is not {#} replay file, as needed", replay_file, recording_string);
            }

            // Destroy the temporary sandbox
            DestroySandbox(editor_state, sandbox_handle);
            return -1;
        }

        return sandbox_handle;
    };

    // Start with the input replay
    unsigned int input_sandbox = initialize_sandbox(options.input_replay_file, EDITOR_SANDBOX_RECORDING_INPUT);
    if (input_sandbox == -1) {
        return return_value;
    }

    unsigned int state_sandbox = initialize_sandbox(options.state_replay_file, EDITOR_SANDBOX_RECORDING_STATE);
    if (state_sandbox == -1) {
        // We have to destroy the input sandbox
        DestroySandbox(editor_state, input_sandbox);
        return return_value;
    }

    // Both sandboxes could be created, we can continue
    if (!options.headless) {
        unsigned int active_sandbox = GetActiveSandboxIncludeScene(editor_state);

        if (active_sandbox != -1) {
            // If not headless, create the visualize windows. For the sandbox, do not split the view.
            // Do that for the second
            DuplicateSandboxUIForDifferentSandbox(editor_state, active_sandbox, input_sandbox, false);
            DuplicateSandboxUIForDifferentSandbox(editor_state, active_sandbox, state_sandbox, true);
        }
    }

    return_value.value.input_sandbox_handle = input_sandbox;
    return_value.value.state_sandbox_handle = state_sandbox;
    return_value.has_value = true;

    return return_value;
}
