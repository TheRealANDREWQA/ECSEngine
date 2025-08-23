#include "editorpch.h"
#include "TestingUI.h"
#include "../Testing/DeterminismTest.h"

// Contains both the options and the test run data
struct InputStateDeterminismTestCombined {
	InputStateDeterminismTestOptions options;
	InputStateDeterminismTestRunData run_data;
};

struct TestingUIData {
	EditorState* editor_state;
	ResizableStream<InputStateDeterminismTestCombined> input_state_determinism;
};

void TestingUIWindowSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory) {
	TestingUIData* data = stack_memory->Reserve<TestingUIData>();
	data->editor_state = editor_state;
	memset(data->asset_opened_headers, 0, sizeof(data->asset_opened_headers));
	memset(data->resource_manager_opened_headers, 0, sizeof(data->resource_manager_opened_headers));
	data->retained_mode_timer.SetUninitialized();

	descriptor.draw = AssetExplorerDraw;
	descriptor.retained_mode = AssetExplorerRetainedMode;

	descriptor.window_name = ASSET_EXPLORER_WINDOW_NAME;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);
}

void TestingUIDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
}

void CreateTestingUI(EditorState* editor_state) {
}

void CreateTestingUIWindowAction(ActionData* action_data) {
}

unsigned int CreateTestingUIWindow(EditorState* editor_state) {
	return 0;
}
