#include "editorpch.h"
#include "Common.h"
#include "../Editor/EditorState.h"

using namespace ECSEngine;

void DestroyIndexedWindows(EditorState* editor_state, Stream<char> window_root, unsigned int starting_value, unsigned int max_count) {
	ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
	window_name.Copy(window_root);

	for (unsigned int index = starting_value; index < max_count; index++) {
		window_name.size = window_root.size;
		function::ConvertIntToChars(window_name, index);

		unsigned int window_index = editor_state->ui_system->GetWindowFromName(window_name);
		if (window_index != -1) {
			editor_state->ui_system->DestroyWindowEx(window_index);
		}
	}
}