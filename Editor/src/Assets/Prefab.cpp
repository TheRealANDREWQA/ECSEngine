#include "editorpch.h"
#include "../Editor/EditorState.h"
#include "ECSEngineComponents.h"

ECS_INLINE static AllocatorPolymorphic PrefabAllocator(const EditorState* editor_state) {
	return GetAllocatorPolymorphic(&editor_state->prefabs_allocator);
}

unsigned int AddPrefabID(EditorState* editor_state, Stream<wchar_t> path) {
	unsigned int existing_id = FindPrefabID(editor_state, path);
	if (existing_id == -1) {
		// We need to create a new entry
		path = path.Copy(PrefabAllocator(editor_state));
		return editor_state->prefabs.Add({ 1, path });
	}
	else {
		IncrementPrefabID(editor_state, existing_id);
		return existing_id;
	}
}

void AddPrefabComponentToEntity(EditorState* editor_state, EntityManager* entity_manager, Entity entity, Stream<wchar_t> relative_assets_path)
{
	unsigned int id = AddPrefabID(editor_state, relative_assets_path);
	if (entity_manager->ExistsEntity(entity)) {
		//entity_manager->AddComponentCommit(entity, PrefabComponent::ID(), )
	}
}

unsigned int FindPrefabID(const EditorState* editor_state, Stream<wchar_t> path) {
	return editor_state->prefabs.Find(PrefabInstance{ 0, path });
}

unsigned int IncrementPrefabID(EditorState* editor_state, unsigned int id) {
	editor_state->prefabs[id].reference_count++;
	return editor_state->prefabs[id].reference_count;
}

unsigned int RemovePrefabID(EditorState* editor_state, Stream<wchar_t> path) {
	unsigned int id = FindPrefabID(editor_state, path);
	ECS_ASSERT(id != -1);
	return RemovePrefabID(editor_state, id);
}

unsigned int RemovePrefabID(EditorState* editor_state, unsigned int id) {
	PrefabInstance& instance = editor_state->prefabs[id];
	instance.reference_count--;
	if (instance.reference_count == 0) {
		// We need to remove the entry
		instance.path.Deallocate(PrefabAllocator(editor_state));
		editor_state->prefabs.RemoveSwapBack(id);
		return 0;
	}
	return instance.reference_count;
}