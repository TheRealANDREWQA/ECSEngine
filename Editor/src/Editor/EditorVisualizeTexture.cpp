#include "editorpch.h"
#include "EditorVisualizeTexture.h"
#include "EditorState.h"

using namespace ECSEngine;

void ChangeVisualizeTexture(EditorState* editor_state, ECSEngine::Stream<char> name, ECSEngine::Texture2D texture) {
	VisualizeTextureSelectElement* select_element = editor_state->visualize_texture.mapping.GetValuePtr(name);
	select_element->texture = texture;
}

void RemoveVisualizeTexture(EditorState* editor_state, ECSEngine::Stream<char> name) {
	// We need to deallocate the name as well
	unsigned int index = editor_state->visualize_texture.mapping.Find(name);
	ECS_ASSERT(index != -1, "Did not find visualize texture");
	Stream<char> stored_name = editor_state->visualize_texture.mapping.GetIdentifierFromIndex(index).AsASCII();
	stored_name.Deallocate(editor_state->EditorAllocator());
	editor_state->visualize_texture.mapping.EraseFromIndex(index);
}

void SetVisualizeTexture(EditorState* editor_state, ECSEngine::Tools::VisualizeTextureSelectElement select_element) {
	// Check to see if the element exists or not
	VisualizeTextureSelectElement* table_select_element;
	if (editor_state->visualize_texture.mapping.TryGetValuePtr(select_element.name, table_select_element)) {
		Stream<char> previous_name = table_select_element->name;
		*table_select_element = select_element;
		table_select_element->name = previous_name;
	}
	else {
		// We must insert it
		select_element.name = StringCopy(editor_state->EditorAllocator(), select_element.name);
		InsertIntoDynamicTable(editor_state->visualize_texture.mapping, editor_state->EditorAllocator(), select_element, ResourceIdentifier(select_element.name));
	}
}

ECS_INLINE unsigned GetVisualizeTextureCount(const EditorState* editor_state) {
	return editor_state->visualize_texture.mapping.GetCount();
}

void GetVisualizeTextureNames(const EditorState* editor_state, ECSEngine::CapacityStream<ECSEngine::Stream<char>>* names) {
	ECS_ASSERT(names->capacity - names->size >= GetVisualizeTextureCount(editor_state));

	editor_state->visualize_texture.mapping.ForEachConst([&](const VisualizeTextureSelectElement& element, ResourceIdentifier identifier) {
		names->Add(element.name);
		});

	insertion_sort(names->buffer, names->size, 1, [](Stream<char> left, Stream<char> right) {
		return StringLexicographicCompare(left, right);
	});
}

void GetVisualizeTextureElements(const EditorState* editor_state, ECSEngine::CapacityStream<ECSEngine::Tools::VisualizeTextureSelectElement>* select_elements) {
	ECS_ASSERT(select_elements->capacity - select_elements->size >= GetVisualizeTextureCount(editor_state));

	editor_state->visualize_texture.mapping.ForEachConst([&](const VisualizeTextureSelectElement& element, ResourceIdentifier identifier) {
		select_elements->Add(element);
	});

	insertion_sort(select_elements->buffer, select_elements->size, 1, [](const VisualizeTextureSelectElement& left, const VisualizeTextureSelectElement& right) {
		return StringLexicographicCompare(left.name, right.name);
	});
}