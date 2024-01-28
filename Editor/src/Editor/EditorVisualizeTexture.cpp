#include "editorpch.h"
#include "EditorVisualizeTexture.h"
#include "EditorState.h"

using namespace ECSEngine;
ECS_TOOLS;

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
		editor_state->visualize_texture.mapping.InsertDynamic(editor_state->EditorAllocator(), select_element, ResourceIdentifier(select_element.name));
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

	InsertionSort(names->buffer, names->size, 1, [](Stream<char> left, Stream<char> right) {
		return StringLexicographicCompare(left, right);
	});
}

void GetVisualizeTextureElements(const EditorState* editor_state, ECSEngine::CapacityStream<ECSEngine::Tools::VisualizeTextureSelectElement>* select_elements) {
	ECS_ASSERT(select_elements->capacity - select_elements->size >= GetVisualizeTextureCount(editor_state));

	editor_state->visualize_texture.mapping.ForEachConst([&](const VisualizeTextureSelectElement& element, ResourceIdentifier identifier) {
		select_elements->Add(element);
	});

	InsertionSort(select_elements->buffer, select_elements->size, 1, [](const VisualizeTextureSelectElement& left, const VisualizeTextureSelectElement& right) {
		return StringLexicographicCompare(left.name, right.name);
	});
}

void UpdateVisualizeTextureSandboxReferences(EditorState* editor_state, unsigned int previous_sandbox_index, unsigned int new_sandbox_index)
{
	// When changing the element name, we need to remove the entry and add it back since the hash depends on the value
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
	ECS_STACK_CAPACITY_STREAM(VisualizeTextureSelectElement, add_elements, 512);

	editor_state->visualize_texture.mapping.ForEachIndex([&](unsigned int index) {
		const VisualizeTextureSelectElement* element = editor_state->visualize_texture.mapping.GetValuePtrFromIndex(index);
		size_t digit_count = 0;
		int64_t existing_index = ConvertCharactersToInt(element->name, digit_count);
		if (digit_count > 0) {
			if (existing_index == previous_sandbox_index) {
				VisualizeTextureSelectElement new_entry = *element;
				new_entry.name = element->name.Copy(&stack_allocator);
				Stream<char> digit = FindFirstCharacter(new_entry.name, (char)previous_sandbox_index + '0');
				digit[0] = (char)new_sandbox_index + '0';
				add_elements.AddAssert(new_entry);

				// We need to remove this value
				// This call will have the entry removed, and we need to return true to have a correct iteration
				RemoveVisualizeTexture(editor_state, element->name);
				return true;
			}
		}
		return false;
	});

	for (unsigned int index = 0; index < add_elements.size; index++) {
		SetVisualizeTexture(editor_state, add_elements[index]);
	}
}
