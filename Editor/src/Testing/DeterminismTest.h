#pragma once
#include "ECSEngineContainersCommon.h"

struct EditorState;

// This test consists of comparing an input replay simulation to another state replay simulation
// And ensure that the input replay produces the same results as the state replay, since the state
// Replay is the golden reference
struct InputStateDeterminismTestOptions {
	// If true, then no graphical output will be provided to the user,
	// Only the final result
	bool headless;
	// TODO: In the future, we might want to allow the user to specify if the modules
	// That the files were written with, or they should run with the current module versions
	// (the code is relevant only to the input replay, the state replay might have the components
	// affected).
	//bool override_modules;
	ECSEngine::Stream<wchar_t> input_replay_file;
	ECSEngine::Stream<wchar_t> state_replay_file;
};

// This is data that is used by the determinism test in order to make the comparison.
struct InputStateDeterminismTestRunData {

};

// It uses 2 temporary sandboxes to perform the test. One test is running with the input replay,
// While the other one is running using the state replay. The sandboxes can be used like any other
// Sandboxes by the user.
// Returns true if it could instantiate the input state determinism test, else false.
bool InstantiateInputStateDeterminismTest(EditorState* editor_state, const InputStateDeterminismTestOptions& options);