#include "editorpch.h"
#include "EditorVisualizeTexture.h"
#include "EditorState.h"
#include "../Sandbox/SandboxAccessor.h"

using namespace ECSEngine;
ECS_TOOLS;

// Returns the full visualize texture element name from the base element name while taking into consideration the sandbox handle
static Stream<char> GetFullName(const EditorState* editor_state, AllocatorPolymorphic allocator, Stream<char> element_name, unsigned int sandbox_handle) {
	Stream<char> full_name;

	if (sandbox_handle != -1) {
		Stream<char> sandbox_name = GetSandbox(editor_state, sandbox_handle)->name;
		full_name.Initialize(allocator, element_name.size + sandbox_name.size + 1);
		full_name.CopyOther(element_name);
		full_name.Add(' ');
		full_name.AddStream(sandbox_name);
	}
	else {
		full_name = element_name.Copy(allocator);
	}

	return full_name;
}

void ChangeVisualizeTexture(EditorState* editor_state, Stream<char> name, Texture2D texture, unsigned int sandbox_handle) {
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 16, ECS_MB * 32);
	VisualizeTextureSelectElement* select_element = editor_state->visualize_texture.mapping.GetValuePtr(GetFullName(editor_state, &stack_allocator, name, sandbox_handle));
	select_element->texture = texture;
}

void RemoveVisualizeTexture(EditorState* editor_state, Stream<char> name, unsigned int sandbox_handle) {
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 16, ECS_MB * 32);

	// We need to deallocate the name as well
	unsigned int index = editor_state->visualize_texture.mapping.Find(GetFullName(editor_state, &stack_allocator, name, sandbox_handle));
	ECS_ASSERT_FORMAT(index != -1, "Did not find visualize texture {#}", name);
	Stream<char> stored_name = editor_state->visualize_texture.mapping.GetIdentifierFromIndex(index).AsASCII();
	stored_name.Deallocate(editor_state->EditorAllocator());
	editor_state->visualize_texture.mapping.EraseFromIndex(index);
}

void SetVisualizeTexture(EditorState* editor_state, const VisualizeTextureSelectElement& select_element, unsigned int sandbox_handle) {
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 16, ECS_MB * 32);

	Stream<char> full_name = GetFullName(editor_state, &stack_allocator, select_element.name, sandbox_handle);

	// Check to see if the element exists or not
	VisualizeTextureSelectElement* table_select_element = editor_state->visualize_texture.mapping.TryGetValuePtr(full_name);
	if (table_select_element != nullptr) {
		Stream<char> previous_name = table_select_element->name;
		*table_select_element = select_element;
		table_select_element->name = previous_name;
	}
	else {
		// We must insert it
		VisualizeTextureSelectElement element_copy = select_element;
		element_copy.name = StringCopy(editor_state->EditorAllocator(), full_name);
		editor_state->visualize_texture.mapping.InsertDynamic(editor_state->EditorAllocator(), element_copy, ResourceIdentifier(element_copy.name));
	}
}

ECS_INLINE unsigned GetVisualizeTextureCount(const EditorState* editor_state) {
	return editor_state->visualize_texture.mapping.GetCount();
}

void GetVisualizeTextureNames(const EditorState* editor_state, CapacityStream<Stream<char>>* names) {
	ECS_ASSERT(names->capacity - names->size >= GetVisualizeTextureCount(editor_state));

	editor_state->visualize_texture.mapping.ForEachConst([&](const VisualizeTextureSelectElement& element, ResourceIdentifier identifier) {
		names->Add(element.name);
		});

	InsertionSort(names->buffer, names->size, 1, [](Stream<char> left, Stream<char> right) {
		return StringLexicographicCompare(left, right);
	});
}

void GetVisualizeTextureElements(const EditorState* editor_state, CapacityStream<VisualizeTextureSelectElement>* select_elements) {
	ECS_ASSERT(select_elements->capacity - select_elements->size >= GetVisualizeTextureCount(editor_state));

	editor_state->visualize_texture.mapping.ForEachConst([&](const VisualizeTextureSelectElement& element, ResourceIdentifier identifier) {
		select_elements->Add(element);
	});

	InsertionSort(select_elements->buffer, select_elements->size, 1, [](const VisualizeTextureSelectElement& left, const VisualizeTextureSelectElement& right) {
		return StringLexicographicCompare(left.name, right.name);
	});
}

void UpdateVisualizeTextureSandboxNameReferences(EditorState* editor_state, unsigned int sandbox_handle, Stream<char> new_name)
{
	// When changing the element name, we need to remove the entry and add it back since the hash depends on the value
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB * 32);
	ECS_STACK_CAPACITY_STREAM(VisualizeTextureSelectElement, add_elements, 512);

	Stream<char> current_name = GetSandbox(editor_state, sandbox_handle)->name;

	editor_state->visualize_texture.mapping.ForEachIndex([&](unsigned int index) {
		const VisualizeTextureSelectElement* element = editor_state->visualize_texture.mapping.GetValuePtrFromIndex(index);
		// The sandbox name is appended at the end. So check for matching of the name at the end of the element's name
		if (element->name.EndsWith(current_name)) {
			VisualizeTextureSelectElement entry = *element;
			Stream<char> base_name = element->name;
			base_name.size -= current_name.size + 1;

			// Set again the base name for the element, not the full name
			entry.name = base_name.Copy(&stack_allocator);
			add_elements.AddAssert(entry);

			// We need to remove this value
			// This call will have the entry removed, and we need to return true to have a correct iteration
			RemoveVisualizeTexture(editor_state, entry.name, sandbox_handle);
			return true;
		}
		return false;
	});

	for (unsigned int index = 0; index < add_elements.size; index++) {
		SetVisualizeTexture(editor_state, add_elements[index], sandbox_handle);
	}
}
