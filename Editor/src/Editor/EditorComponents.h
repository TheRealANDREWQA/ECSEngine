#pragma once
#include "ECSEngineContainers.h"
#include "../Sandbox/SandboxTypes.h"

namespace ECSEngine {
	struct EntityManager;
	namespace Reflection {
		struct ReflectionManager;
	}
	namespace Tools {
		struct UIReflectionDrawer;
	}
}

struct EditorState;

enum EDITOR_COMPONENT_EVENT : unsigned char {
	// Handled internally, generated only for components - we need to broadcast the addition to all entity manager
	EDITOR_COMPONENT_EVENT_IS_ADDED,
	// Handled internally, generated only for components - we need to broadcast the removal to all entity managers
	EDITOR_COMPONENT_EVENT_IS_REMOVED,
	// Handled internally
	EDITOR_COMPONENT_EVENT_HAS_CHANGED,
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
	// Handled by the user
	EDITOR_COMPONENT_EVENT_HAS_ALLOCATOR_BUT_NO_BUFFERS,
	// Handled internally
	EDITOR_COMPONENT_EVENT_DEPENDENCY_CHANGED,
	// Handled by the user
	EDITOR_COMPONENT_EVENT_LINK_MISSING_TARGET,
	// Handled by the user
	EDITOR_COMPONENT_EVENT_LINK_INVALID_TARGET,
	// Handled by the user
	EDITOR_COMPONENT_EVENT_LINK_MISMATCH_FOR_DEFAULT,
	EDITOR_COMPONENT_EVENT_COUNT
};

bool IsEditorComponentHandledInternally(EDITOR_COMPONENT_EVENT event_type);

struct EditorComponentEvent {
	EDITOR_COMPONENT_EVENT type;
	ECSEngine::Stream<char> name;

	ECSEngine::Stream<char> conflicting_name; // used by the SAME_ID event

	short new_id; // used by the changed ID events or IS_REMOVED
	bool is_shared; // used only by IS_REMOVED
	bool is_link_component; // used by IS_REMOVED
};

// Keeps the reflection_type as well such that when a component is changed the data can still be appropriately preserved
struct EditorComponents {
	struct LoadedModule {
		ECSEngine::Stream<char> name;
		ECSEngine::Stream<ECSEngine::Stream<char>> types;
		// These just reference the link_components stream outside.
		// The name is not allocated separately, it aliases the one in there
		ECSEngine::ResizableStream<ECSEngine::Stream<char>> link_components;
	};
	
	struct LinkComponent {
		// Only the target is separately allocated - the name is the allocated and kept inside the hash table
		// of the reflection manager and does not need to be deallocated by us
		ECS_INLINE void Deallocate(ECSEngine::AllocatorPolymorphic allocator) const {
			target.Deallocate(allocator);
		}

		ECSEngine::Stream<char> name;
		ECSEngine::Stream<char> target;
	};

	struct ResolveEventOptions {
		ECS_INLINE ECSEngine::SpinLock* EntityManagerLock() {
			return EntityManagerLock(archetype_locks);
		}
		
		ECS_INLINE static ECSEngine::SpinLock* EntityManagerLock(ECSEngine::Stream<ECSEngine::SpinLock> archetype_locks) {
			return archetype_locks.buffer + archetype_locks.size - 1;
		}

		// These locks are used to prevents multiple threads on performing actions on the same archetypes
		ECSEngine::Stream<ECSEngine::SpinLock> archetype_locks = { nullptr, 0 };
		// These locks are used to indicate which components have already had their data recovered
		ECSEngine::Stream<ECSEngine::SpinLock> component_recover_data_locks = { nullptr, 0 };
		// These locks are used to indicate which shared components have already had their data recovered
		ECSEngine::Stream<ECSEngine::SpinLock> shared_component_recover_data_locks = { nullptr, 0 };
	};

	// Extra options used by the event
	union FinalizeEventOptions {
		bool IS_REMOVED_should_remove_type = true;
	};

	void AddType(const ECSEngine::Reflection::ReflectionType* type, unsigned int module_index);

	// Registers a new component to the entity manager
	// Can a provide an optional lock that is used to lock the entity manager in order to execute the operation
	void AddComponentToManager(ECSEngine::EntityManager* entity_manager, ECSEngine::Stream<char> component_name, ECSEngine::SpinLock* lock = nullptr);
	
	// Can a provide an optional lock that is used to lock the entity manager in order to execute the operation
	void ChangeComponentID(ECSEngine::EntityManager* entity_manager, ECSEngine::Stream<char> component_name, short new_id, ECSEngine::SpinLock* lock = nullptr);

	void EmptyEventStream();

	// Should be called if the calls for resolve event have been applied successfully on all entity managers
	// It does clean up job like updating the internal component after applying the changes
	void FinalizeEvent(
		EditorState* editor_state,
		const ECSEngine::Reflection::ReflectionManager* reflection_manager, 
		EditorComponentEvent event,
		FinalizeEventOptions options = { true }
	);

	// Returns the index inside the loaded_modules if the module has components recorded here, else -1.
	unsigned int FindModule(ECSEngine::Stream<char> name) const;

	// Returns the index inside the loaded modules of the module that contains the given component/type
	unsigned int FindComponentModule(ECSEngine::Stream<char> name) const;

	// Returns the index of the module that contains that component. The index references the module index that can be used
	// with the other module function (inside Module.h)
	// Returns -1 if it doesn't exist
	unsigned int FindComponentModuleInReflection(const EditorState* editor_state, ECSEngine::Stream<char> name) const;

	void ForEachComponent(void (*Functor)(ECSEngine::Reflection::ReflectionType* type, void* _data), void* _data);

	void ForEachComponent(void (*Functor)(const ECSEngine::Reflection::ReflectionType* type, void* _data), void* _data) const;

	void ForEachSharedComponent(void (*Functor)(ECSEngine::Reflection::ReflectionType* type, void* _data), void* _data);

	void ForEachSharedComponent(void (*Functor)(const ECSEngine::Reflection::ReflectionType* type, void* _data), void* _data) const;

	// Returns -1 if it doesn't find it
	ECSEngine::Component GetComponentID(ECSEngine::Stream<char> name) const;

	// If the given name is a link component, it will follow the link and return the ID for its link
	// Returns -1 if it doesn't find it
	ECSEngine::Component GetComponentIDWithLink(ECSEngine::Stream<char> name) const;

	// Returns USHORT_MAX if it doesn't find it
	unsigned short GetComponentByteSize(ECSEngine::Stream<char> name) const;

	// Returns the target if it is a link component, else { nullptr, 0 }
	ECSEngine::Stream<char> GetComponentFromLink(ECSEngine::Stream<char> name) const;

	const ECSEngine::Reflection::ReflectionType* GetType(ECSEngine::Stream<char> name) const;

	const ECSEngine::Reflection::ReflectionType* GetType(unsigned int module_index, unsigned int type_index) const;

	const ECSEngine::Reflection::ReflectionType* GetType(ECSEngine::Component component, bool shared) const;

	void GetUniqueLinkComponents(ECSEngine::CapacityStream<const ECSEngine::Reflection::ReflectionType*>& link_types) const;

	void GetSharedLinkComponents(ECSEngine::CapacityStream<const ECSEngine::Reflection::ReflectionType*>& link_types) const;

	// Both unique and shared types
	void GetLinkComponents(
		ECSEngine::CapacityStream<const ECSEngine::Reflection::ReflectionType*>& unique_types, 
		ECSEngine::CapacityStream<const ECSEngine::Reflection::ReflectionType*>& shared_types
	) const;

	// Fills in the events which must be handled by the user
	// It will eliminate these events from the internal buffer
	void GetUserEvents(ECSEngine::CapacityStream<EditorComponentEvent>& events);

	// The indices should be used to index into loaded modules. If the replace links parameter is set to true
	// then instead of giving the index of the component it will replace it with its link instead
	void GetModuleComponentIndices(unsigned int module_index, ECSEngine::CapacityStream<unsigned int>* module_indices, bool replace_links = false) const;

	// The indices should be used to index into loaded modules.  If the replace links parameter is set to true
	// then instead of giving the index of the component it will replace it with its link instead
	void GetModuleSharedComponentIndices(unsigned int module_index, ECSEngine::CapacityStream<unsigned int>* module_indices, bool replace_links = false) const;

	// At the moment, a single link component can be assigned to each component. It returns { nullptr, 0 }
	// If it doesn't exist
	ECSEngine::Stream<char> GetLinkComponentForTarget(ECSEngine::Stream<char> name) const;

	// Returns true if the link components needs to be built using DLL functions
	bool GetLinkComponentDLLStatus(ECSEngine::Stream<char> name) const;

	// Fills in the names of the types that the given type depends upon
	void GetTypeDependencies(
		ECSEngine::Stream<char> type,
		ECSEngine::CapacityStream<ECSEngine::Stream<char>>* dependencies
	) const;

	// Fills in the indices of the modules inside loaded_modules that the given module depends upon.
	// If the editor_state is given then it will fill in with the ProjectModules* indices instead
	// of the loaded_modules ones. If it depends on an ECS type then it will skip it - it will always consider it to be available
	void GetModuleTypesDependencies(unsigned int loaded_module_index, ECSEngine::CapacityStream<unsigned int>* module_indices, const EditorState* editor_state = nullptr) const;

	// Fills in the indices of the modules inside loaded_modules that depend upon the given module.
	// If the editor_state is given then it will fill in with the ProjectModules* indices instead
	// of the loaded_modules ones.
	void GetModulesTypesDependentUpon(unsigned int loaded_module_index, ECSEngine::CapacityStream<unsigned int>* module_indices, const EditorState* editor_state = nullptr) const;

	// Returns true if the component name contains non trivially copyable types
	bool HasBuffers(ECSEngine::Stream<char> component_name) const;

	// If the component doesn't need a DLL or the DLL function is loaded then it will return true.
	// Else false
	bool HasLinkComponentDLLFunction(const EditorState* modules, ECSEngine::Stream<char> component_name) const;

	// Returns true if all the component (unique and/or shared) which have link components have
	// their DLL function ready
	bool HasLinkComponentDLLFunction(const EditorState* modules, const ECSEngine::EntityManager* entity_manager, ECSEngine::bool2 select_unique_shared = { true, true }) const;
	
	// Returns true if the unique/shared component has assets, else false
	bool HasComponentAssets(ECSEngine::Stream<char> component_name) const;

	// Returns true if the unique/shared component has assets, else false
	bool HasComponentAssets(ECSEngine::Component component, bool shared) const;

	// Returns true if the component (unique or shared) exists or not
	bool IsComponent(ECSEngine::Stream<char> name) const;

	// Returns true if the unique component exists or not
	bool IsUniqueComponent(ECSEngine::Stream<char> name) const;

	// Returns true if the shared component exists or not
	bool IsSharedComponent(ECSEngine::Stream<char> name) const;

	// Returns true if it is a link component
	bool IsLinkComponent(ECSEngine::Stream<char> name) const;

	// Returns true if it is a target of a link component
	bool IsLinkComponentTarget(ECSEngine::Stream<char> name) const;

	unsigned int ModuleComponentCount(ECSEngine::Stream<char> name) const;

	unsigned int ModuleComponentCount(unsigned int index) const;

	unsigned int ModuleSharedComponentCount(ECSEngine::Stream<char> name) const;

	unsigned int ModuleSharedComponentCount(unsigned int index) const;

	// Returns the loaded_modules index given the index from ProjectModules*
	unsigned int ModuleIndexFromReflection(const EditorState* editor_state, unsigned int module_index) const;

	// Returns the module index inside the ProjectModules* given the index inside this structure
	unsigned int ReflectionModuleIndex(const EditorState* editor_state, unsigned int loaded_module_index) const;

	// Returns true if the component has changed since last time, else false.
	bool NeedsUpdate(ECSEngine::Stream<char> component, const ECSEngine::Reflection::ReflectionManager* reflection_manager) const;

	// Adds a user event to the stream
	void PushUserEvent(EditorComponentEvent event);

	// Moves the data inside the entity manager to be in accordance to the new component
	// The archetype locks needs to have the archetype count + 1 spin locks allocated (the last one is used in order
	// to syncronize operations on the whole entity manager like changing the ID of a component)
	// It doesn't not update the type to the new one
	void RecoverData(
		EditorState* editor_state,
		unsigned int sandbox_index,
		EDITOR_SANDBOX_VIEWPORT viewport,
		const ECSEngine::Reflection::ReflectionManager* reflection_manager,
		ECSEngine::Stream<char> component_name,
		ResolveEventOptions* options = nullptr
	);

	// Allocates the spinlocks needed to be used with RecoverData (archetype count + 1 spin locks) 
	// (the last one is used in order to syncronize operations on the whole entity manager like changing the ID of a component)
	static ECSEngine::Stream<ECSEngine::SpinLock> RecoverDataLocks(
		const ECSEngine::EntityManager* entity_manager, 
		ECSEngine::AllocatorPolymorphic allocator
	);

	static ECSEngine::Stream<ECSEngine::SpinLock> RecoverComponentLocks(
		const ECSEngine::EntityManager* entity_manager, 
		ECSEngine::AllocatorPolymorphic allocator
	);

	static ECSEngine::Stream<ECSEngine::SpinLock> RecoverSharedComponentLocks(
		const ECSEngine::EntityManager* entity_manager,
		ECSEngine::AllocatorPolymorphic allocator
	);

	// Returns the total byte size needed to allocate the spin locks for this entity manager
	static size_t RecoverDataLocksSize(const ECSEngine::EntityManager* entity_manager);

	// Returns the total byte size needed to allocate the spin locks for this entity manager
	static size_t RecoverComponentLocksSize(const ECSEngine::EntityManager* entity_manager);

	// Returns the total byte size needed to allocate the spin locks for this entity manager
	static size_t RecoverSharedComponentLocksSize(const ECSEngine::EntityManager* entity_manager);

	// Returns the total byte size needed to allocate the spin locks for this entity manager
	// (all 3 spin lock streams)
	static size_t ResolveEventOptionsSize(const ECSEngine::EntityManager* entity_manager);

	// Initializes spinlocks needed to be used with RecoverData from a given buffer
	static ECSEngine::Stream<ECSEngine::SpinLock> RecoverDataLocks(const ECSEngine::EntityManager* entity_manager, uintptr_t& ptr);

	static ECSEngine::Stream<ECSEngine::SpinLock> RecoverComponentLocks(const ECSEngine::EntityManager* entity_manager, uintptr_t& ptr);

	static ECSEngine::Stream<ECSEngine::SpinLock> RecoverSharedComponentLocks(const ECSEngine::EntityManager* entity_manager, uintptr_t& ptr);

	static ResolveEventOptions ResolveEventOptionsFromBuffer(const ECSEngine::EntityManager* entity_manager, uintptr_t& ptr);

	// Removes all internal types that were associated with the given module. Also, it will update the entity managers
	// in order to remove all the components and systems that were associated with that module
	void RemoveModule(EditorState* editor_state, unsigned int loaded_module_index);

	// Removes all the components that came from the given module
	void RemoveModuleFromManager(
		EditorState* editor_state, 
		unsigned int sandbox_index, 
		EDITOR_SANDBOX_VIEWPORT viewport, 
		unsigned int loaded_module_index
	) const;

	// Removes it only from the internal storage, not from the entity managers
	// User ResolveEvent if it is a component alongside the entity managers to be updated
	void RemoveType(ECSEngine::Stream<char> name);

	// Removes the component from the manager.
	void RemoveTypeFromManager(
		EditorState* editor_state,
		unsigned int sandbox_index,
		EDITOR_SANDBOX_VIEWPORT viewport, 
		ECSEngine::Component component, 
		bool shared, 
		ECSEngine::SpinLock* lock = nullptr
	) const;

	// Does not deallocate any buffers. It will only set the default values.
	void ResetComponent(ECSEngine::Stream<char> component_name, void* component_data) const;

	// Does not deallocate any buffers. It will only set the default values.
	void ResetComponent(ECSEngine::Component component, void* component_data, bool shared) const;

	// Allocates the buffers for an entity from the stack memory into the component_buffers parameter
	// and sets the data to its default values. The buffers will be set to 0.
	void ResetComponents(ECSEngine::ComponentSignature component_signature, void* stack_memory, void** component_buffers) const;

	// For link components it will reset the target component, without doing anything to the link component.
	// It works only for unique and link components to unique components
	// Sets the component to default values. If it has buffers, it will deallocate them.
	void ResetComponentFromManager(ECSEngine::EntityManager* entity_manager, ECSEngine::Stream<char> component_name, ECSEngine::Entity entity) const;

	// Returns true if it handled the event, else false when the event needs to be reprocessed later on (for example when resizing
	// a component allocator and the component has not yet been registered because of the event)
	// Modifies the current data stored in the entity manager such that it conforms to the new component
	// Tries to maintain the data already stored.
	// The archetype locks needs to have the archetype count spin locks allocated for the events that require recovering the data
	bool ResolveEvent(
		EditorState* editor_state,
		unsigned int sandbox_index,
		EDITOR_SANDBOX_VIEWPORT viewport,
		const ECSEngine::Reflection::ReflectionManager* reflection_manager, 
		EditorComponentEvent event,
		ResolveEventOptions* options = nullptr
	);

	// Registers all unique and shared components alongisde their respective allocators
	void SetManagerComponents(ECSEngine::EntityManager* entity_manager);

	// Allocates all the component/shared component allocators that are needed
	void SetManagerComponentAllocators(ECSEngine::EntityManager* entity_manager);

	void UpdateComponent(const ECSEngine::Reflection::ReflectionManager* reflection_manager, ECSEngine::Stream<char> component_name);

	// The conflict will refer to the name already stored in the EditorComponents
	// or in the reflection_manager for the same_id component conflict
	void UpdateComponents(
		EditorState* editor_state,
		const ECSEngine::Reflection::ReflectionManager* reflection_manager,
		unsigned int hierarchy_index,
		ECSEngine::Stream<char> module_name
	);

	// Returns { nullptr, 0 } if it doesn't find it. Else a stable reference of the name
	ECSEngine::Stream<char> TypeFromID(short id, bool shared) const;

	// Initializes a default allocator. The size must be the one given from default allocator size
	void Initialize(void* buffer);

	void Initialize(ECSEngine::AllocatorPolymorphic allocator);

	static size_t DefaultAllocatorSize();

	ECSEngine::AllocatorPolymorphic allocator;
	ECSEngine::ResizableStream<LoadedModule> loaded_modules;
	// Keep a list of the link components such that they can easily referenced
	// When trying to determine if a component has a link or not
	ECSEngine::ResizableStream<LinkComponent> link_components;

	ECSEngine::ResizableStream<EditorComponentEvent> events;

	// This is used by the functions that need to traverse user defined fields of the components.
	// In most cases, they should be dumb data, but it would be annoying not to support user defined types
	// or containers. The types are handled manually because we are not using the process folder functionality
	// Both unique and shared components are stored here (into the type_definition table)
	ECSEngine::Reflection::ReflectionManager* internal_manager;
};

void TickEditorComponents(EditorState* editor_state);

// Uses a multithreaded approach. It will execute all tasks that can be done in parallel
// It will fill in the user events for those that cannot be handled internally
void EditorStateResolveComponentEvents(EditorState* editor_state, ECSEngine::CapacityStream<EditorComponentEvent>& user_events);

// Removes all user events that have become outdated
void EditorStateRemoveOutdatedEvents(EditorState* editor_state, ECSEngine::ResizableStream<EditorComponentEvent>& user_events);