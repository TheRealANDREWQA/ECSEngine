#pragma once
#include "EditorEventDef.h"
#include "ECSEngineContainers.h"
#include "EditorStateTypes.h"

void EditorEventLock(EditorState* editor_state);

void EditorEventUnlock(EditorState* editor_state);

// Can be called from multiple threads. It returns the pointer of data allocated or given if the event_data_size is 0
void* EditorAddEvent(EditorState* editor_state, EditorEventFunction function, void* event_data, size_t event_data_size = 0);

// Returns true if it finds an event with the given function. You should leave thread safe on
// For most use cases
bool EditorHasEvent(EditorState* editor_state, EditorEventFunction function, bool thread_safe = true);

// Returns the data for an event if it finds the given function else nullptr. You should leave thread safe on
// For most use cases
void* EditorGetEventData(EditorState* editor_state, EditorEventFunction function, bool thread_safe = true);

// Fills in the stream with the data pointers for all events currently stored with the given function.
// You should leave thread safe on for most use cases
void EditorGetEventTypeData(EditorState* editor_state, EditorEventFunction function, ECSEngine::CapacityStream<void*>* data, bool thread_safe = true);

// This function can be used while inside an event about other events
// This function is necessary such that all relevants are taken into account
// The other function will return invalid results in such a case
void EditorGetEventTypeDataWhileInsideEvent(const EditorState* editor_state, EditorEventFunction function, ECSEngine::CapacityStream<void*>* data);

// Can be called from multiple threads.
// It will execute the given event after a certain other event has finished executing
// It does a memcpy comparison between the data. Returns the data stored
// for this added event
void* EditorAddEventAfter(
	EditorState* editor_state,
	EditorEventFunction function,
	void* event_data,
	size_t event_data_size,
	EditorEventFunction event_to_finish,
	void* event_to_finish_compare_data,
	size_t event_to_finish_compare_data_size
);

// Can be called from multiple threads.
// It add 2 events, the second one waiting for the first one to finish before executing
void EditorAddEventWithContinuation(
	EditorState* editor_state,
	EditorEventFunction first_function,
	void* first_event_data,
	size_t first_event_data_size,
	EditorEventFunction second_function,
	void* second_event_data,
	size_t second_event_data_size
);

template<typename Functor>
EDITOR_EVENT(EditorEventExecuteFunctor) {
	Functor* functor = (Functor*)_data;
	(*functor)();
}

template<typename Functor>
struct EventExecuteFunctorWaitFlagData {
	char functor_storage[sizeof(Functor)];
	EDITOR_STATE_FLAGS flag;
	bool set;
};

template<typename Functor>
EDITOR_EVENT(EditorEventExecuteFunctorWaitFlag) {
	EventExecuteFunctorWaitFlagData<Functor>* data = (EventExecuteFunctorWaitFlagData<Functor>*)_data;
	bool is_set = EditorStateHasFlag(editor_state, data->flag);
	is_set = data->set ? is_set : !is_set;
	if (is_set) {
		Functor* functor = (Functor*)data->functor_storage;
		(*functor)();
		return true;
	}
	return false;
}

template<typename Functor>
void EditorAddEventFunctor(EditorState* editor_state, Functor&& functor) {
	EditorAddEvent(editor_state, EditorEventExecuteFunctor<Functor>, &functor, sizeof(functor));
}

template<typename Functor>
void EditorAddEventFunctorWaitFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag, bool set, Functor&& functor) {
	EventExecuteFunctorWaitFlagData<Functor> event_data;
	// Don't rely on the assignment operator - for lambdas it is deleted
	memcpy(event_data.functor_storage, &functor, sizeof(functor));
	event_data.flag = flag;
	event_data.set = set;
	EditorAddEvent(editor_state, EditorEventExecuteFunctorWaitFlag<Functor>, &event_data, sizeof(event_data));
}

// The functor receives the data as void* and must return true if a match is found
template<typename Functor>
bool EditorHasEventTest(EditorState* editor_state, EditorEventFunction event_function, Functor&& compare_functor) {
	bool result = false;
	EditorEventLock(editor_state);
	ECS_STACK_CAPACITY_STREAM(void*, event_data, 512);
	EditorGetEventTypeData(editor_state, event_function, &event_data, false);
	for (unsigned int index = 0; index < event_data.size && !result; index++) {
		result = compare_functor(event_data[index]);
	}
	EditorEventUnlock(editor_state);
	return result;
}