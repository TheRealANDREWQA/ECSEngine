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
using namespace ECSEngine;

enum EDITOR_COMPONENT_EVENT : unsigned char {
	// Handled internally, generated only for components - we need to broadcast the addition to all entity manager
	EDITOR_COMPONENT_EVENT_IS_ADDED,
	// Handled internally, generated only for components - we need to broadcast the removal to all entity managers
	EDITOR_COMPONENT_EVENT_IS_REMOVED,
	// Handled internally
	EDITOR_COMPONENT_EVENT_HAS_CHANGED,
	// Handled internally
	EDITOR_COMPONENT_EVENT_PROMOTED_TO_COMPONENT,
	// Handled internally
	EDITOR_COMPONENT_EVENT_COMPONENT_DEMOTED,
	// Handled by the user
	EDITOR_COMPONENT_EVENT_SAME_ID,
	// Handled by the user
	EDITOR_COMPONENT_EVENT_IS_MISSING_FUNCTION,
	// Handled internally
	EDITOR_COMPONENT_EVENT_ALLOCATOR_SIZE_CHANGED,
	// Handled internally
	EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID,
	// Handled internally
	EDITOR_COMPONENT_EVENT_DIFFERENT_COMPONENT_DIFFERENT_ID,
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
	Stream<char> name;

	Stream<char> conflicting_name; // used by the SAME_ID event
	Stream<char> missing_function_name; // used by the IS_MISSING_FUNCTION

	short new_id; // used by the changed ID events or IS_REMOVED
	ECS_COMPONENT_TYPE component_type; // used only by IS_REMOVED
	bool is_link_component; // used by IS_REMOVED
};

// Keeps the reflection_type as well such that when a component is changed the data can still be appropriately preserved
struct EditorComponents {
	struct LoadedModule {
		Stream<char> name;
		Stream<Stream<char>> types;
		// These just reference the link_components stream outside.
		// The name is not allocated separately, it aliases the one in there
		ResizableStream<Stream<char>> link_components;
	};
	
	struct LinkComponent {
		// Only the target is separately allocated - the name is the allocated and kept inside the hash table
		// of the reflection manager and does not need to be deallocated by us
		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) const {
			target.Deallocate(allocator);
		}

		Stream<char> name;
		Stream<char> target;
	};

	struct ResolveEventOptions {
		ECS_INLINE SpinLock* EntityManagerLock() {
			return EntityManagerLock(archetype_locks);
		}
		
		ECS_INLINE static SpinLock* EntityManagerLock(Stream<SpinLock> archetype_locks) {
			return archetype_locks.size == 0 ? nullptr : archetype_locks.buffer + archetype_locks.size - 1;
		}

		// These locks are used to prevents multiple threads on performing actions on the same archetypes
		Stream<SpinLock> archetype_locks = { nullptr, 0 };
		// These locks are used to indicate which components have already had their data recovered
		Stream<SpinLock> component_recover_data_locks = { nullptr, 0 };
		// These locks are used to indicate which shared components have already had their data recovered
		Stream<SpinLock> shared_component_recover_data_locks = { nullptr, 0 };
	};

	// Extra options used by the event
	union FinalizeEventOptions {
		bool should_remove_type = true;
	};

	void AddType(const Reflection::ReflectionType* type, unsigned int module_index);

	// Registers a new component to the entity manager
	// Can a provide an optional lock that is used to lock the entity manager in order to execute the operation
	void AddComponentToManager(EntityManager* entity_manager, Stream<char> component_name, SpinLock* lock = nullptr);
	
	// Can a provide an optional lock that is used to lock the entity manager in order to execute the operation
	void ChangeComponentID(EntityManager* entity_manager, Stream<char> component_name, short new_id, SpinLock* lock = nullptr);

	void EmptyEventStream();

	// Fills in all the components of a certain type
	void FillAllComponents(CapacityStream<Component>* components, ECS_COMPONENT_TYPE component_type) const;

	// Should be called if the calls for resolve event have been applied successfully on all entity managers
	// It does clean up job like updating the internal component after applying the changes
	void FinalizeEvent(
		EditorState* editor_state,
		const Reflection::ReflectionManager* reflection_manager, 
		EditorComponentEvent event,
		FinalizeEventOptions options = { true }
	);

	// Returns the index inside the loaded_modules if the module has components recorded here, else -1.
	unsigned int FindModule(Stream<char> name) const;

	// Returns the index inside the loaded modules of the module that contains the given component/type
	unsigned int FindComponentModule(Stream<char> name) const;

	// Returns the index of the module that contains that component. The index references the module index that can be used
	// with the other module function (inside Module.h)
	// Returns -1 if it doesn't exist
	unsigned int FindComponentModuleInReflection(const EditorState* editor_state, Stream<char> name) const;

	void ForEachComponent(void (*Functor)(Reflection::ReflectionType* type, void* _data), void* _data);

	void ForEachComponent(void (*Functor)(const Reflection::ReflectionType* type, void* _data), void* _data) const;

	void ForEachSharedComponent(void (*Functor)(Reflection::ReflectionType* type, void* _data), void* _data);

	void ForEachSharedComponent(void (*Functor)(const Reflection::ReflectionType* type, void* _data), void* _data) const;

	// Fills in the names of all components for a given type, or if the component type is COUNT,
	// For all components, including global ones
	void GetAllComponentNames(AdditionStream<Stream<char>> names, ECS_COMPONENT_TYPE component_type) const;
	
	// Returns -1 if it doesn't find it
	Component GetComponentID(Stream<char> name) const;

	// If the given name is a link component, it will follow the link and return the ID for its link
	// Returns -1 if it doesn't find it
	Component GetComponentIDWithLink(Stream<char> name) const;

	// Returns USHORT_MAX if it doesn't find it
	unsigned short GetComponentByteSize(Stream<char> name) const;

	// Returns the target if it is a link component, else { nullptr, 0 }
	Stream<char> GetComponentFromLink(Stream<char> name) const;

	const Reflection::ReflectionType* GetType(Stream<char> name) const;

	const Reflection::ReflectionType* GetType(unsigned int module_index, unsigned int type_index) const;

	const Reflection::ReflectionType* GetType(Component component, ECS_COMPONENT_TYPE type) const;

	void GetUniqueLinkComponents(CapacityStream<const Reflection::ReflectionType*>& link_types) const;

	void GetSharedLinkComponents(CapacityStream<const Reflection::ReflectionType*>& link_types) const;

	void GetGlobalLinkComponents(CapacityStream<const Reflection::ReflectionType*>& link_types) const;

	// Both unique and shared types
	void GetLinkComponents(
		CapacityStream<const Reflection::ReflectionType*>& unique_types, 
		CapacityStream<const Reflection::ReflectionType*>& shared_types,
		CapacityStream<const Reflection::ReflectionType*>& global_types
	) const;

	// Fills in the events which must be handled by the user
	// It will eliminate these events from the internal buffer
	void GetUserEvents(CapacityStream<EditorComponentEvent>& events);

	// The indices should be used to index into loaded modules. If the replace links parameter is set to true
	// then instead of giving the index of the component it will replace it with its link instead
	void GetModuleComponentIndices(unsigned int module_index, CapacityStream<unsigned int>* module_indices, bool replace_links = false) const;

	// The indices should be used to index into loaded modules.  If the replace links parameter is set to true
	// then instead of giving the index of the component it will replace it with its link instead
	void GetModuleSharedComponentIndices(unsigned int module_index, CapacityStream<unsigned int>* module_indices, bool replace_links = false) const;

	// At the moment, a single link component can be assigned to each component. It returns { nullptr, 0 }
	// If it doesn't exist
	Stream<char> GetLinkComponentForTarget(Stream<char> name) const;

	// Returns true if the link components needs to be built using DLL functions
	bool GetLinkComponentDLLStatus(Stream<char> name) const;
	
	ECS_COMPONENT_TYPE GetComponentType(Stream<char> name) const;

	// Fills in the names of the types that the given type depends upon
	void GetTypeDependencies(
		Stream<char> type,
		CapacityStream<Stream<char>>* dependencies
	) const;

	// Fills in the indices of the modules inside loaded_modules that the given module depends upon.
	// If the editor_state is given then it will fill in with the ProjectModules* indices instead
	// of the loaded_modules ones. If it depends on an ECS type then it will skip it - it will always consider it to be available
	void GetModuleTypesDependencies(unsigned int loaded_module_index, CapacityStream<unsigned int>* module_indices, const EditorState* editor_state = nullptr) const;

	// Fills in the indices of the modules inside loaded_modules that depend upon the given module.
	// If the editor_state is given then it will fill in with the ProjectModules* indices instead
	// of the loaded_modules ones.
	void GetModulesTypesDependentUpon(unsigned int loaded_module_index, CapacityStream<unsigned int>* module_indices, const EditorState* editor_state = nullptr) const;

	// Returns true if the component name contains non trivially copyable types
	bool HasBuffers(Stream<char> component_name) const;

	// If the component doesn't need a DLL or the DLL function is loaded then it will return true.
	// Else false
	bool HasLinkComponentDLLFunction(const EditorState* modules, Stream<char> component_name) const;

	// Returns true if all the component (unique and/or shared) which have link components have
	// their DLL function ready
	bool HasLinkComponentDLLFunction(
		const EditorState* modules, 
		const EntityManager* entity_manager, 
		bool3 select_unique_shared_global = { true, true, true }
	) const;
	
	// Returns true if the unique/shared/global component has assets, else false
	bool HasComponentAssets(Stream<char> component_name) const;

	// Returns true if the unique/shared component has assets, else false
	bool HasComponentAssets(Component component, ECS_COMPONENT_TYPE type) const;

	// Returns true if the component (unique, shared or global) exists or not
	bool IsComponent(Stream<char> name) const;

	// Returns true if the unique component exists or not
	bool IsUniqueComponent(Stream<char> name) const;

	// Returns true if the shared component exists or not
	bool IsSharedComponent(Stream<char> name) const;

	// Returns true if the global component exists or not
	bool IsGlobalComponent(Stream<char> name) const;

	// Returns true if it is a link component
	bool IsLinkComponent(Stream<char> name) const;

	// Returns true if it is a target of a link component
	bool IsLinkComponentTarget(Stream<char> name) const;

	unsigned int ModuleComponentCount(Stream<char> name, ECS_COMPONENT_TYPE type) const;

	unsigned int ModuleComponentCount(unsigned int index, ECS_COMPONENT_TYPE type) const;

	// Returns the loaded_modules index given the index from ProjectModules*
	unsigned int ModuleIndexFromReflection(const EditorState* editor_state, unsigned int module_index) const;

	// Returns the module index inside the ProjectModules* given the index inside this structure
	unsigned int ReflectionModuleIndex(const EditorState* editor_state, unsigned int loaded_module_index) const;

	// Returns true if the component has changed since last time, else false.
	bool NeedsUpdate(Stream<char> component, const Reflection::ReflectionManager* reflection_manager) const;

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
		const Reflection::ReflectionManager* reflection_manager,
		Stream<char> component_name,
		ResolveEventOptions* options = nullptr
	);

	// Allocates the spinlocks needed to be used with RecoverData (archetype count + 1 spin locks) 
	// (the last one is used in order to syncronize operations on the whole entity manager like changing the ID of a component)
	static Stream<SpinLock> RecoverDataLocks(
		const EntityManager* entity_manager, 
		AllocatorPolymorphic allocator
	);

	static Stream<SpinLock> RecoverComponentLocks(
		const EntityManager* entity_manager, 
		AllocatorPolymorphic allocator
	);

	static Stream<SpinLock> RecoverSharedComponentLocks(
		const EntityManager* entity_manager,
		AllocatorPolymorphic allocator
	);

	// Returns the total byte size needed to allocate the spin locks for this entity manager
	static size_t RecoverDataLocksSize(const EntityManager* entity_manager);

	// Returns the total byte size needed to allocate the spin locks for this entity manager
	static size_t RecoverComponentLocksSize(const EntityManager* entity_manager);

	// Returns the total byte size needed to allocate the spin locks for this entity manager
	static size_t RecoverSharedComponentLocksSize(const EntityManager* entity_manager);

	// Returns the total byte size needed to allocate the spin locks for this entity manager
	// (all 3 spin lock streams)
	static size_t ResolveEventOptionsSize(const EntityManager* entity_manager);

	// Initializes spinlocks needed to be used with RecoverData from a given buffer
	static Stream<SpinLock> RecoverDataLocks(const EntityManager* entity_manager, uintptr_t& ptr);

	static Stream<SpinLock> RecoverComponentLocks(const EntityManager* entity_manager, uintptr_t& ptr);

	static Stream<SpinLock> RecoverSharedComponentLocks(const EntityManager* entity_manager, uintptr_t& ptr);

	static ResolveEventOptions ResolveEventOptionsFromBuffer(const EntityManager* entity_manager, uintptr_t& ptr);

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
	// Use ResolveEvent if it is a component alongside the entity managers to be updated
	void RemoveType(Stream<char> name);

	// It will remove the link type from the UI reflection as well
	void RemoveLinkType(EditorState* editor_state, Stream<char> name);

	// Removes the component from the manager.
	void RemoveTypeFromManager(
		EditorState* editor_state,
		unsigned int sandbox_index,
		EDITOR_SANDBOX_VIEWPORT viewport, 
		Component component, 
		ECS_COMPONENT_TYPE type,
		SpinLock* lock = nullptr
	) const;

	// This function does not take into consideration the module reset function
	void ResetComponentBasic(Stream<char> component_name, void* component_data) const;

	// This function does not take into consideration the module reset function
	void ResetComponentBasic(Component component, void* component_data, ECS_COMPONENT_TYPE type) const;

	// For link components it will reset the target component, without doing anything to the link component.
	// It works for unique and shared components - for global components there is a separate function
	// Sets the component to default values. If it has buffers, it will deallocate them.
	void ResetComponent(EditorState* editor_state, unsigned int sandbox_index, Stream<char> component_name, Entity entity, ECS_COMPONENT_TYPE type) const;

	// Does not deallocate any buffers or remove asset references that would be overwritten. It will only set the default values.
	void ResetGlobalComponent(Component component, void* component_data) const;

	// Returns true if it handled the event, else false when the event needs to be reprocessed later on (for example when resizing
	// a component allocator and the component has not yet been registered because of the event)
	// Modifies the current data stored in the entity manager such that it conforms to the new component
	// Tries to maintain the data already stored.
	// The archetype locks needs to have the archetype count spin locks allocated for the events that require recovering the data
	bool ResolveEvent(
		EditorState* editor_state,
		unsigned int sandbox_index,
		EDITOR_SANDBOX_VIEWPORT viewport,
		const Reflection::ReflectionManager* reflection_manager, 
		EditorComponentEvent event,
		ResolveEventOptions* options = nullptr
	);

	// Registers all unique and shared components alongisde their respective allocators
	void SetManagerComponents(EntityManager* entity_manager);

	// Allocates all the component/shared component allocators that are needed
	void SetManagerComponentAllocators(EntityManager* entity_manager);

	void UpdateComponent(const Reflection::ReflectionManager* reflection_manager, Stream<char> component_name);

	// The conflict will refer to the name already stored in the EditorComponents
	// or in the reflection_manager for the same_id component conflict
	void UpdateComponents(
		EditorState* editor_state,
		const Reflection::ReflectionManager* reflection_manager,
		unsigned int hierarchy_index,
		Stream<char> module_name
	);

	// Returns { nullptr, 0 } if it doesn't find it. Else a stable reference of the name
	Stream<char> ComponentFromID(short id, ECS_COMPONENT_TYPE) const;

	// Initializes a default allocator. The size must be the one given from default allocator size
	void Initialize(void* buffer);

	void InitializeAllocator(AllocatorPolymorphic allocator);

	static size_t DefaultAllocatorSize();

	AllocatorPolymorphic allocator;
	ResizableStream<LoadedModule> loaded_modules;
	// Keep a list of the link components such that they can easily referenced
	// When trying to determine if a component has a link or not
	ResizableStream<LinkComponent> link_components;

	ResizableStream<EditorComponentEvent> events;

	// This is used by the functions that need to traverse user defined fields of the components.
	// In most cases, they should be dumb data, but it would be annoying not to support user defined types
	// or containers. The types are handled manually because we are not using the process folder functionality
	// Both unique and shared components are stored here (into the type_definition table)
	Reflection::ReflectionManager* internal_manager;
};

void TickEditorComponents(EditorState* editor_state);

// Uses a multithreaded approach. It will execute all tasks that can be done in parallel
// It will fill in the user events for those that cannot be handled internally
void EditorStateResolveComponentEvents(EditorState* editor_state, CapacityStream<EditorComponentEvent>& user_events);

// Removes all user events that have become outdated
void EditorStateRemoveOutdatedEvents(EditorState* editor_state, ResizableStream<EditorComponentEvent>& user_events);