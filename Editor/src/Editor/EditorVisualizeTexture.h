#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineUI.h"

struct EditorState;

struct EditorVisualizeTexture {
	ECSEngine::HashTableDefault<ECSEngine::Tools::VisualizeTextureSelectElement> mapping;
};

// Single threaded
// The texture needs to be previously set
void ChangeVisualizeTexture(EditorState* editor_state, ECSEngine::Stream<char> name, ECSEngine::Texture2D texture);

// Single threaded
// The texture needs to exist
void RemoveVisualizeTexture(EditorState* editor_state, ECSEngine::Stream<char> name);

// Single threaded
// This can be used to set a new entry or to modify an existing one
// If the select_element texture is left at nullptr and the element exists, then it will not modify the texture
void SetVisualizeTexture(EditorState* editor_state, ECSEngine::Tools::VisualizeTextureSelectElement select_element);

// Returns the number of visualize textures
unsigned GetVisualizeTextureCount(const EditorState* editor_state);

// The names will reference the values inside the stored mapping
// These names are valid as long as one of the entries is not removed from the mapping
void GetVisualizeTextureNames(const EditorState* editor_state, ECSEngine::CapacityStream<ECSEngine::Stream<char>>* names);

void GetVisualizeTextureElements(const EditorState* editor_state, ECSEngine::CapacityStream<ECSEngine::Tools::VisualizeTextureSelectElement>* select_elements);

void UpdateVisualizeTextureSandboxReferences(EditorState* editor_state, unsigned int previous_sandbox_index, unsigned int new_sandbox_index);