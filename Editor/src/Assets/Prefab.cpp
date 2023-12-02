#include "editorpch.h"
#include "../Editor/EditorState.h"
#include "ECSEngineComponents.h"
#include "../Sandbox/SandboxEntityOperations.h"

ECS_INLINE static AllocatorPolymorphic PrefabAllocator(const EditorState* editor_state) {
	return GetAllocatorPolymorphic(&editor_state->prefabs_allocator);
}

unsigned int AddPrefabID(EditorState* editor_state, Stream<wchar_t> path, unsigned int increment_count) {
	unsigned int existing_id = FindPrefabID(editor_state, path);
	if (existing_id == -1) {
		// We need to create a new entry
		path = path.Copy(PrefabAllocator(editor_state));
		return editor_state->prefabs.Add({ 1, path });
	}
	else {
		IncrementPrefabID(editor_state, existing_id, increment_count);
		return existing_id;
	}
}

void AddPrefabComponentToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, Stream<wchar_t> relative_assets_path)
{
	unsigned int id = AddPrefabID(editor_state, relative_assets_path);
	AddSandboxEntityComponent(editor_state, sandbox_index, entity, STRING(PrefabComponent));
	PrefabComponent* component = GetSandboxEntityComponent<PrefabComponent>(editor_state, sandbox_index, entity);
	component->id = id;
	component->detached = false;
}

void AddPrefabComponentToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, unsigned int id)
{
	IncrementPrefabID(editor_state, id);
	AddSandboxEntityComponent(editor_state, sandbox_index, entity, STRING(PrefabComponent));
	PrefabComponent* component = GetSandboxEntityComponent<PrefabComponent>(editor_state, sandbox_index, entity);
	component->id = id;
	component->detached = false;
}

unsigned int DecrementPrefabID(EditorState* editor_state, unsigned int id, unsigned int decrement_count)
{
	editor_state->prefabs[id].reference_count -= decrement_count;
	return editor_state->prefabs[id].reference_count;
}

unsigned int FindPrefabID(const EditorState* editor_state, Stream<wchar_t> path) {
	return editor_state->prefabs.Find(PrefabInstance{ 0, path });
}

Stream<wchar_t> GetPrefabPath(const EditorState* editor_state, unsigned int id)
{
	return editor_state->prefabs[id].path;
}

unsigned int IncrementPrefabID(EditorState* editor_state, unsigned int id, unsigned int increment_count) {
	editor_state->prefabs[id].reference_count += increment_count;
	return editor_state->prefabs[id].reference_count;
}

unsigned int RemovePrefabID(EditorState* editor_state, Stream<wchar_t> path, unsigned int decrement_count) {
	unsigned int id = FindPrefabID(editor_state, path);
	ECS_ASSERT(id != -1);
	return RemovePrefabID(editor_state, id);
}

unsigned int RemovePrefabID(EditorState* editor_state, unsigned int id, unsigned int decrement_count) {
	PrefabInstance& instance = editor_state->prefabs[id];
	ECS_ASSERT(instance.reference_count >= decrement_count);
	unsigned int reference_count = DecrementPrefabID(editor_state, id, decrement_count);
	if (reference_count == 0) {
		instance.path.Deallocate(PrefabAllocator(editor_state));
		editor_state->prefabs.RemoveSwapBack(id);
	}
	return instance.reference_count;
}