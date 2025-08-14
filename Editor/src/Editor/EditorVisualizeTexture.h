#pragma once
#include "ECSEngineContainers.h"
#include "ECSEngineUI.h"

struct EditorState;

struct EditorVisualizeTexture {
	ECSEngine::HashTableDefault<ECSEngine::Tools::VisualizeTextureSelectElement> mapping;
};

// Note: For all the following functions, the sandbox handle can be -1 in order to indicate that this is a general element, 
// without being bound to a specific sandbox. If it is specified, it will concatenate the name with the sandbox' name in order to make it unique

// Single threaded
// The texture needs to be previously set
void ChangeVisualizeTexture(EditorState* editor_state, ECSEngine::Stream<char> name, ECSEngine::Texture2D texture, unsigned int sandbox_handle);

// Single threaded
// The texture needs to exist
void RemoveVisualizeTexture(EditorState* editor_state, ECSEngine::Stream<char> name, unsigned int sandbox_handle);

// Single threaded
// This can be used to set a new entry or to modify an existing one
// If the select_element texture is left at nullptr and the element exists, then it will not modify the texture

void SetVisualizeTexture(EditorState* editor_state, const ECSEngine::Tools::VisualizeTextureSelectElement& select_element, unsigned int sandbox_handle);

// Returns the number of visualize textures
unsigned GetVisualizeTextureCount(const EditorState* editor_state);

// The names will reference the values inside the stored mapping
// These names are valid as long as one of the entries is not removed from the mapping
// These names contain the sandbox appended part
void GetVisualizeTextureNames(const EditorState* editor_state, ECSEngine::CapacityStream<ECSEngine::Stream<char>>* names);

void GetVisualizeTextureElements(const EditorState* editor_state, ECSEngine::CapacityStream<ECSEngine::Tools::VisualizeTextureSelectElement>* select_elements);

void UpdateVisualizeTextureSandboxNameReferences(EditorState* editor_state, unsigned int sandbox_handle, ECSEngine::Stream<char> new_sandbox_name);