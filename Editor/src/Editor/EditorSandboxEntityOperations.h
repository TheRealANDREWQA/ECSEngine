#pragma once
#include "ECSEngineEntities.h"

using namespace ECSEngine;

struct EditorState;

// When it is not playing it returns the scene entities manager, when playing the runtime one
const EntityManager* ActiveEntityManager(const EditorState* editor_state, unsigned int sandbox_index);

// When it is not playing it returns the scene entities manager, when playing the runtime one
EntityManager* ActiveEntityManager(EditorState* editor_state, unsigned int sandbox_index);

// Does nothing if the entity doesn't exist
void AddSandboxEntityComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name);

// Does nothing if the entity doesn't exist
void AddSandboxEntitySharedComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name);

// It forwards to the unique or shared variant
void AddSandboxEntityComponentEx(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name);

void AttachEntityName(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> name);

void ChangeEntityName(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> new_name);

// An empty entity
Entity CreateSandboxEntity(EditorState* editor_state, unsigned int sandbox_index);

Entity CreateSandboxEntity(EditorState* editor_state, unsigned int sandbox_index, ComponentSignature unique, SharedComponentSignature shared);

// Creates an identical copy of the entity and returns it. If for some reason the entity doesn't exist
// it returns -1
Entity CopySandboxEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity);

// Creates an identical copy of the entity and returns it. If for some reason the entity doesn't exist
// false (else true). Can give an entity buffer such that you can do some other operations on the newly copied entities
bool CopySandboxEntities(EditorState* editor_state, unsigned int sandbox_index, Entity entity, unsigned int count, Entity* copied_entities = nullptr);

// Creates an identical copy of the entity and returns it. If for some reason the entity doesn't exist
// it returns -1
void DeleteSandboxEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity);

// Readonly. It returns the archetypes' components
// Does nothing if the entity doesn't exist
ComponentSignature EntityUniqueComponents(const EditorState* editor_state, unsigned int sandbox_index, Entity entity);

// Readonly. It returns the archetypes' components
// Does nothing if the entity doesn't exist
ComponentSignature EntitySharedComponents(const EditorState* editor_state, unsigned int sandbox_index, Entity entity);

// Readonly. It returns the archetypes' instances
// Does nothing if the entity doesn't exist
SharedComponentSignature EntitySharedInstances(const EditorState* editor_state, unsigned int sandbox_index, Entity entity);

// Returns -1 if it doesn't exist
Entity GetSandboxEntity(const EditorState* editor_state, unsigned int sandbox_index, Stream<char> name);

// If it doesn't exist, it will create it
SharedInstance GetSharedComponentDefaultInstance(EditorState* editor_state, unsigned int sandbox_index, Component component);

void ParentSandboxEntity(EditorState* editor_state, unsigned int sandbox_index, Entity child, Entity parent);

void ParentSandboxEntities(EditorState* editor_state, unsigned int sandbox_index, Stream<Entity> children, Entity parent);

// Does nothing if the entity doesn't exist
void RemoveSandboxEntityComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name);

// Does nothing if the entity doesn't exist
void RemoveSandboxEntitySharedComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name);

// Selects based upon which type the component is (unique or shared)
void RemoveSandboxEntityComponentEx(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name);

void RemoveSandboxEntityFromHierarchy(EditorState* editor_state, unsigned int sandbox_index, Entity entity);

// Reverts the unique component to default values
void ResetSandboxEntityComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name);