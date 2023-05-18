#pragma once
#include "EditorEventDef.h"
#include "ECSEngineContainers.h"
#include "EditorStateTypes.h"

// Single threaded. It returns the pointer of data allocated or given if the event_data_size is 0
void* EditorAddEvent(EditorState* editor_state, EditorEventFunction function, void* event_data, size_t event_data_size = 0);

// Single threaded.
// It will take the pointer as is but will set the event data size different from 0
// in order to be deallocated; useful for example when the data is coallesced from the editor
// allocator and it must still be deallocated
void EditorAddEventWithPointer(EditorState* editor_state, EditorEventFunction function, void* event_data);

// Returns the last event data
void* EditorEventLastData(const EditorState* editor_state);

// Returns true if it finds an event with the given function
bool EditorHasEvent(const EditorState* editor_state, EditorEventFunction function);

// Returns the data for an event if it finds the givne function else nullptr
void* EditorGetEventData(const EditorState* editor_state, EditorEventFunction function);

// Fills in the stream with the data pointers for all events currently stored with the given function
void EditorGetEventTypeData(const EditorState* editor_state, EditorEventFunction function, ECSEngine::CapacityStream<void*>* data);

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
		return true;
	}
	Functor* functor = (Functor*)data->functor_storage;
	(*functor)();
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