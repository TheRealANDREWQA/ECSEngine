#pragma once
#include "ECSEngineEntities.h"
#include "ECSEngineForEach.h"
#include "ECSEngineAssets.h"
#include "EditorSandboxTypes.h"

using namespace ECSEngine;

struct EditorState;

/*
	For all functions calls in this file where viewport is specified and left to COUNT it means
	that it should use the active entity manager. If given it will use the entity manager from that viewport
*/

// When it is not playing it returns the scene entities manager, when playing the runtime one
const EntityManager* ActiveEntityManager(const EditorState* editor_state, unsigned int sandbox_index);

// When it is not playing it returns the scene entities manager, when playing the runtime one
EntityManager* ActiveEntityManager(EditorState* editor_state, unsigned int sandbox_index);

// Does nothing if the entity doesn't exist
void AddSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Does nothing if the entity doesn't exist. If the shared instance is -1 it will add it with the default shared instance
void AddSandboxEntitySharedComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name, 
	SharedInstance instance = { -1 },
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// It forwards to the unique or shared variant
void AddSandboxEntityComponentEx(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

void AttachEntityName(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> name,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

void ChangeEntityName(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> new_name,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// An empty entity
Entity CreateSandboxEntity(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT);

Entity CreateSandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	ComponentSignature unique, 
	SharedComponentSignature shared,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Creates an identical copy of the entity and returns it. If for some reason the entity doesn't exist
// it returns -1
Entity CopySandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Creates an identical copy of the entity and returns it. If for some reason the entity doesn't exist
// false (else true). Can give an entity buffer such that you can do some other operations on the newly copied entities
bool CopySandboxEntities(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	unsigned int count, 
	Entity* copied_entities = nullptr,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Returns true if it succeeded in the conversion. It can fail if the necessary DLL function is not yet loaded or there is a
// mismatch between the types. The allocator is used for the buffer allocations (if it is nullptr then it will just reference
// the non asset fields)
bool ConvertTargetToLinkComponent(
	EditorState* editor_state,
	unsigned int sandbox_index, 
	Stream<char> link_component, 
	Entity entity, 
	void* link_data, 
	AllocatorPolymorphic allocator = { nullptr },
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Returns true if it succeeded in the conversion. It can fail if the necessary DLL function is not yet loaded or there is a
// mismatch between the types. The allocator is used for the buffer allocations (if it is nullptr then it will just reference
// the non asset fields)
bool ConvertTargetToLinkComponent(
	EditorState* editor_state,
	Stream<char> link_component,
	const void* target_data,
	void* link_data,
	AllocatorPolymorphic allocator = { nullptr }
);

// Returns true if it succeeded in the conversion. It can fail if the necessary DLL function is not yet loaded or there is a
// mismatch between the types. The allocator is used for the buffer allocations (if it is nullptr then it will just reference
// the non asset fields)
bool ConvertLinkComponentToTarget(
	EditorState* editor_state,
	unsigned int sandbox_index,
	Stream<char> link_component,
	Entity entity,
	const void* link_data,
	AllocatorPolymorphic allocator = { nullptr },
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Returns true if it succeeded in the conversion. It can fail if the necessary DLL function is not yet loaded or there is a
// mismatch between the types. The allocator is used for the buffer allocations (if it is nullptr then it will just reference
// the non asset fields)
bool ConvertLinkComponentToTarget(
	EditorState* editor_state,
	Stream<char> link_component,
	void* target_data,
	const void* link_data,
	AllocatorPolymorphic allocator = { nullptr }
);

// This does not remove the asset handles that the shared instances have
void DeleteSandboxUnreferencedSharedInstances(
	EditorState* editor_state,
	unsigned int sandbox_index,
	Component shared_component,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Creates an identical copy of the entity and returns it. If for some reason the entity doesn't exist it returns -1
void DeleteSandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Readonly. It returns the archetype's components
// Does nothing if the entity doesn't exist
ComponentSignature EntityUniqueComponents(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Readonly. It returns the archetype's components
// Does nothing if the entity doesn't exist
ComponentSignature EntitySharedComponents(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Readonly. It returns the archetype's instances
// Does nothing if the entity doesn't exist
SharedComponentSignature EntitySharedInstances(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Returns -1 if it the entity doesn't exist or the entity doesn't have the component
SharedInstance EntitySharedInstance(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Searches for a shared instance that matches the given data or, if it doesn't exist, it will create one.
SharedInstance FindOrCreateSharedComponentInstance(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	const void* data,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Returns -1 if it doesn't exist
Entity GetSandboxEntity(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<char> name,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// If it doesn't exist, it will create it
SharedInstance GetSharedComponentDefaultInstance(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Unique components only. Returns nullptr if the component doesn't exist or the entity is invalid
void* GetSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Unique components only. Returns nullptr if the component doesn't exist or the entity is invalid
const void* GetSandboxEntityComponent(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Shared components only. Returns nullptr if the component doesn't exist or the shared instance is invalid
void* GetSandboxSharedInstance(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	SharedInstance instance,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Shared components only. Returns nullptr if the component doesn't exist or the shared instance is invalid
const void* GetSandboxSharedInstance(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component, 
	SharedInstance instance,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Returns a component from an entity unique or shared. If it doesn't exist it returns nullptr
void* GetSandboxEntityComponentEx(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	bool shared,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Returns a component from an entity unique or shared. If it doesn't exist it returns nullptr
const void* GetSandboxEntityComponentEx(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	bool shared,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

MemoryArena* GetComponentAllocator(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

MemoryArena* GetSharedComponentAllocator(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Component component,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Might reference the internal storage or the given one
Stream<char> GetEntityName(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	CapacityStream<char> storage,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Fills in the asset handles for the given component of the entity
// (some can repeat if the component has multiple handles of the same type)
void GetSandboxEntityComponentAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Fills in the asset handles for the given shared component of the entity
// (some can repeat if the component has multiple handles of the same type)
void GetSandboxEntitySharedComponentAssets(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Component component, 
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Fills in the asset handles for the given shared instance
// (some can repeat if the component has multiple handles of the same type)
void GetSandboxSharedInstanceComponentAssets(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	Component component,
	SharedInstance shared_instance,
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Fills in the asset handles that the entity uses (some can repeat if they appear multiple
// times in the same component or in different components)
void GetSandboxEntityAssets(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	Entity entity,
	CapacityStream<AssetTypedHandle>* handles,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

ECSEngine::EntityManager* GetSandboxEntityManager(
	EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

const ECSEngine::EntityManager* GetSandboxEntityManager(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

void ParentSandboxEntity(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity child, 
	Entity parent,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

void ParentSandboxEntities(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<Entity> children, 
	Entity parent,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Does nothing if the entity doesn't exist
void RemoveSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Does nothing if the entity doesn't exist
void RemoveSandboxEntitySharedComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Selects based upon which type the component is (unique or shared)
void RemoveSandboxEntityComponentEx(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

void RemoveSandboxEntityFromHierarchy(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

namespace ECSEngine {
	struct LinkComponentAssetField;
}

// If the asset_fields is provided it will remove only those fields specified
void RemoveSandboxComponentAssets(
	EditorState* editor_state,
	unsigned int sandbox_index,
	Component component,
	const void* data,
	bool shared,
	Stream<LinkComponentAssetField> asset_fields = { nullptr, 0 }
);

// Reverts the componet to default values
void ResetSandboxEntityComponent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Entity entity, 
	Stream<char> component_name,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// Single threaded at the moment
void SandboxForEachEntity(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ForEachEntityFunctor functor,
	void* functor_data,
	ComponentSignature unique_signature,
	ComponentSignature shared_signature,
	ComponentSignature unique_exclude_signature = {},
	ComponentSignature shared_exclude_signature = {},
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// The functor takes as parameters Archetype*, ArchetypeBase*, Entity, void** unique_components
template<bool early_exit = false, typename ArchetypeInitialize, typename Functor>
void SandboxForAllUniqueComponents(
	EditorState* editor_state,
	unsigned int sandbox_index,
	ArchetypeInitialize&& archetype_initialize,
	Functor&& functor,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
) {
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	entity_manager->ForEachEntity<early_exit>(archetype_initialize, [](Archetype* archetype, ArchetypeBase* base_archetype) {}, functor);
}

// The functor receives as parameters const Archetype*, const ArchetypeBase*, Entity, void** unique_components
template<bool early_exit = false, typename ArchetypeInitialize, typename Functor>
void SandboxForAllUniqueComponents(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ArchetypeInitialize&& archetype_initialize,
	Functor&& functor,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
) {
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	entity_manager->ForEachEntity<early_exit>(archetype_initialize, [](const Archetype* archetype, const ArchetypeBase* base_archetype) {}, functor);
}

// Return true to early exit, else false
template<bool early_exit = false, typename ComponentFunctor, typename Functor>
void SandboxForAllSharedComponents(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	ComponentFunctor&& component_functor,
	Functor&& functor,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
) {
	const EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);
	entity_manager->ForAllSharedInstances<early_exit>(component_functor, functor);
}

// Unique component only. Splats the corresponding asset fields from the link component into their entity manager storage
// If the link component needs to be built but the dll function is missing it returns false else true
bool SandboxSplatLinkComponentAssetFields(
	EditorState* editor_state,
	unsigned int sandbox_index,
	const void* link_component,
	Stream<char> component_name,
	bool give_error_when_failing = true,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);

// It will change the shared instance of the entity, it won't change the data of the current shared instance
// Called only when asset fields are changed
bool SandboxUpdateLinkComponentForEntity(
	EditorState* editor_state,
	unsigned int sandbox_index,
	const void* link_component,
	Stream<char> component_name,
	Entity entity,
	bool give_error_when_failing = true,
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_COUNT
);