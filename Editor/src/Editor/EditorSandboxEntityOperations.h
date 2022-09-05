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

// An empty entity
Entity CreateSandboxEntity(EditorState* editor_state, unsigned int sandbox_index);

Entity CreateSandboxEntity(EditorState* editor_state, unsigned int sandbox_index, ComponentSignature unique, SharedComponentSignature shared);

// Creates an identical copy of the entity and returns it. If for some reason the entity doesn't exist
// it returns -1
Entity CopySandboxEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity);

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

// Does nothing if the entity doesn't exist
void RemoveSandboxEntityComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name);

// Does nothing if the entity doesn't exist
void RemoveSandboxEntitySharedComponent(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<char> component_name);