#include "editorpch.h"
#include "EditorEvent.h"
#include "EditorState.h"

using namespace ECSEngine;
ECS_TOOLS;

#define WAIT_FOR_QUEUE_SPACE_SLEEP_TICK 5

void AllocateMemory(EditorState* editor_state, EditorEvent& editor_event, void* data, size_t data_size) {
	editor_event.data = CopyNonZero(editor_state->MultithreadedEditorAllocator(), data, data_size);
	editor_event.data_size = data_size;
}

void DeallocateMemory(EditorState* editor_state, EditorEvent editor_event) {
	if (editor_event.data_size > 0) {
		Deallocate(editor_state->MultithreadedEditorAllocator(), editor_event.data);
	}
}

void* EditorAddEvent(EditorState* editor_state, EditorEventFunction function, void* event_data, size_t event_data_size) {
	EditorEvent editor_event;
	editor_event.function = function;
	AllocateMemory(editor_state, editor_event, event_data, event_data_size);
	editor_event.data_size = event_data_size;

	editor_state->event_queue.Push(editor_event);
	return editor_event.data;
}

void* EditorEventLastData(EditorState* editor_state)
{
	EditorEvent event_;
	if (editor_state->event_queue.Peek(event_)) {
		return event_.data;
	}
	return nullptr;
}

bool EditorHasEvent(const EditorState* editor_state, EditorEventFunction function)
{
	return editor_state->event_queue.ForEach<true>([function](EditorEvent event_) {
		return event_.function == function;
	});
}

void* EditorGetEventData(const EditorState* editor_state, EditorEventFunction function)
{
	void* ptr = nullptr;
	editor_state->event_queue.ForEach<true>([&ptr, function](EditorEvent event_) {
		if (event_.function == function) {
			ptr = event_.data;
			return true;
		}
		return false;
	});
	return ptr;
}

void EditorGetEventTypeData(const EditorState* editor_state, EditorEventFunction function, ECSEngine::CapacityStream<void*>* data)
{
	editor_state->event_queue.ForEach([data, function](EditorEvent event_) {
		if (event_.function == function) {
			data->AddAssert(event_.data);
		}
	});
}

struct WaitEventWrapperData {
	EditorEventFunction wait_function;
	void* wait_function_data;
	size_t wait_function_data_size;

	EditorEventFunction execute_function;
	void* execute_function_data;
	size_t execute_function_data_size;
};

EDITOR_EVENT(WaitEventWrapper) {
	WaitEventWrapperData* data = (WaitEventWrapperData*)_data;
	ECS_STACK_CAPACITY_STREAM(void*, target_data, 512);
	EditorGetEventTypeData(editor_state, data->wait_function, &target_data);
	bool exists = false;
	for (unsigned int index = 0; index < target_data.size; index++) {
		if (data->wait_function_data_size > 0) {
			if (memcmp(data->wait_function_data, target_data[index], data->wait_function_data_size) == 0) {
				exists = true;
				break;
			}
		}
		else {
			if (target_data[index] == data->wait_function_data) {
				exists = true;
				break;
			}
		}
	}

	if (!exists) {
		// We can add the wrapper event to be executed
		EditorAddEvent(editor_state, data->execute_function, data->execute_function_data, data->execute_function_data_size);
		if (data->execute_function_data_size > 0) {
			Deallocate(editor_state->EditorAllocator(), data->execute_function_data);
		}
		if (data->wait_function_data_size > 0) {
			Deallocate(editor_state->EditorAllocator(), data->wait_function_data);
		}
		return false;
	}
	return true;
}

void* EditorAddEventAfter(
	EditorState* editor_state, 
	EditorEventFunction function, 
	void* event_data, 
	size_t event_data_size, 
	EditorEventFunction event_to_finish, 
	void* event_to_finish_compare_data, 
	size_t event_to_finish_compare_data_size
)
{
	WaitEventWrapperData wrapper_data;
	wrapper_data.execute_function = function;
	wrapper_data.execute_function_data = CopyNonZero(editor_state->EditorAllocator(), event_data, event_data_size);;
	wrapper_data.execute_function_data_size = event_data_size;
	wrapper_data.wait_function = event_to_finish;
	wrapper_data.wait_function_data = CopyNonZero(editor_state->EditorAllocator(), event_to_finish_compare_data, event_to_finish_compare_data_size);
	wrapper_data.wait_function_data_size = event_to_finish_compare_data_size;
	
	EditorAddEvent(editor_state, WaitEventWrapper, &wrapper_data, sizeof(wrapper_data));
	return wrapper_data.execute_function_data;
}

void EditorAddEventWithContinuation(
	EditorState* editor_state, 
	EditorEventFunction first_function, 
	void* first_event_data, 
	size_t first_event_data_size,
	EditorEventFunction second_function,
	void* second_event_data,
	size_t second_event_data_size
)
{
	first_event_data = EditorAddEvent(editor_state, first_function, first_event_data, first_event_data_size);
	EditorAddEventAfter(editor_state, second_function, second_event_data, second_event_data_size, first_function, first_event_data, first_event_data_size);
}
