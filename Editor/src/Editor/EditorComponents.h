#pragma once
#include "ECSEngineContainers.h"

namespace ECSEngine {
	struct EntityManager;
	namespace Reflection {
		struct ReflectionManager;
	}
}

enum EDITOR_COMPONENT_EVENT : unsigned char {
	// Handled internally
	EDITOR_COMPONENT_EVENT_HAS_CHANGED,
	// Handled internally if not a component, else user handled
	EDITOR_COMPONENT_EVENT_IS_REMOVED,
	// Handled by the user
	EDITOR_COMPONENT_EVENT_SAME_ID,
	// Handled by the user
	EDITOR_COMPONENT_EVENT_IS_MISSING_ID,
	// Handled internally
	EDITOR_COMPONENT_EVENT_ALLOCATOR_SIZE_CHANGED,
	// Handled internally
	EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID,
	// Handled internally
	EDITOR_COMPONENT_EVENT_DIFFERENT_COMPONENT_DIFFERENT_ID,
	// Handled by the user
	EDITOR_COMPONENT_EVENT_HAS_BUFFERS_BUT_NO_ALLOCATOR,
	// Handled internally
	EDITOR_COMPONENT_EVENT_DEPENDENCY_CHANGED,
	EDITOR_COMPONENT_EVENT_COUNT
};

bool IsEditorComponentHandledInternally(EDITOR_COMPONENT_EVENT event_type);

struct EditorComponentEvent {
	EDITOR_COMPONENT_EVENT type;
	ECSEngine::Stream<char> name;
	ECSEngine::Stream<char> conflicting_name;
};

// Keeps the reflection_type as well such that when a component is changed the data can still be appropriately preserved
struct EditorComponents {
	struct LoadedModule {
		ECSEngine::Stream<char> name;
		ECSEngine::Stream<ECSEngine::Stream<char>> types;
	};
	
	void ChangeComponentID(ECSEngine::EntityManager* entity_manager, ECSEngine::Stream<char> component_name, unsigned short new_id);

	void ForEachComponent(void (*Functor)(ECSEngine::Reflection::ReflectionType* type, void* _data), void* _data);

	void ForEachComponent(void (*Functor)(const ECSEngine::Reflection::ReflectionType* type, void* _data), void* _data) const;

	void ForEachSharedComponent(void (*Functor)(ECSEngine::Reflection::ReflectionType* type, void* _data), void* _data);

	void ForEachSharedComponent(void (*Functor)(const ECSEngine::Reflection::ReflectionType* type, void* _data), void* _data) const;

	// Returns USHORT_MAX if it doesn't find it
	ECSEngine::Component GetComponentID(ECSEngine::Stream<char> name) const;

	// Returns USHORT_MAX if it doesn't find it
	unsigned short GetComponentByteSize(ECSEngine::Stream<char> name) const;

	// Fills in the events which must be handled by the user
	// It will eliminate these events from the internal buffer
	void GetUserEvents(ECSEngine::CapacityStream<EditorComponentEvent>& events);

	// Returns true if the component name contains non trivially copyable types
	bool HasBuffers(ECSEngine::Stream<char> component_name) const;

	// Returns the index inside the loaded_modules if the module has components recorded here, else -1.
	unsigned int IsModule(ECSEngine::Stream<char> name) const;

	// Moves the data inside the entity manager to be in accordance to the new component
	// The archetype locks needs to have the archetype count spin locks allocated
	void RecoverData(
		ECSEngine::EntityManager* entity_manager,
		const ECSEngine::Reflection::ReflectionManager* reflection_manager,
		ECSEngine::Stream<char> component_name,
		ECSEngine::Stream<ECSEngine::SpinLock> archetype_locks = { nullptr, 0 }
	);

	// Returns true if the component has changed since last time, else false.
	bool NeedsUpdate(ECSEngine::Stream<char> component, const ECSEngine::Reflection::ReflectionManager* reflection_manager) const;

	void RemoveType(ECSEngine::Stream<char> name);

	// Modifies the current data stored in the entity manager such that it conforms to the new component
	// Tries to maintain the data already stored
	// The archetype locks needs to have the archetype count spin locks allocated for the events that require recovering the data
	void ResolveEvent(
		ECSEngine::EntityManager* entity_manager,
		const ECSEngine::Reflection::ReflectionManager* reflection_manager, 
		EditorComponentEvent event,
		ECSEngine::Stream<ECSEngine::SpinLock> archetype_locks = { nullptr, 0 }
	);

	// The conflict will refer to the name already stored in the EditorComponents
	// or in the reflection_manager for the same_id component conflict
	void UpdateComponents(
		const ECSEngine::Reflection::ReflectionManager* reflection_manager,
		unsigned int hierarchy_index,
		ECSEngine::Stream<char> module_name
	);

	// Returns { nullptr, 0 } if it doesn't find it. Else a stable reference of the name
	ECSEngine::Stream<char> TypeFromID(unsigned short id, bool shared);

	// Initializes a default allocator. The size must be the one given from default allocator size
	void Initialize(void* buffer);

	void Initialize(ECSEngine::AllocatorPolymorphic allocator);

	static size_t DefaultAllocatorSize();

	ECSEngine::AllocatorPolymorphic allocator;
	ECSEngine::ResizableStream<LoadedModule> loaded_modules;

	ECSEngine::ResizableStream<EditorComponentEvent> events;

	// This is used by the functions that need to traverse user defined fields of the components.
	// In most cases, they should be dumb data, but it would be annoying not to support user defined types
	// or containers. The types are handled manually because we are not using the process folder functionality
	// Both unique and shared components are stored here (into the type_definition table)
	ECSEngine::Reflection::ReflectionManager* internal_manager;
};

struct EditorState;

// Uses a multithreaded approach. It will execute all tasks that can be done in parallel
// It will fill in the user events for those that cannot be handled internally
void EditorStateResolveComponentEvents(EditorState* editor_state, ECSEngine::CapacityStream<EditorComponentEvent>& user_events);