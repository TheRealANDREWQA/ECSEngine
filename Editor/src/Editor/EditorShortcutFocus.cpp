#include "editorpch.h"
#include "EditorShortcutFocus.h"
#include "EditorState.h"

void EditorShortcutFocus::AddActionEntry(EditorShortcutFocusAction action, const Copyable* action_data, EDITOR_SHORTCUT_FOCUS_TYPE focus_type, unsigned int sandbox_index, unsigned char priority)
{
	Copyable* copied_action_data = CopyableCopy(action_data, action_entries.allocator);
	action_entries.Add({ sandbox_index, priority, focus_type, copied_action_data, action });
}

void EditorShortcutFocus::HandleActionEntries(EditorState* editor_state)
{
	// Although this is not strictly required, do this to avoid some extra work if not needed
	if (action_entries.size == 0) {
		return;
	}

	const size_t MAX_CAPACITY = 512;

	// For each entry type, find all of its matches per sandbox and then pick the highest priority one
	ECS_STACK_CAPACITY_STREAM(unsigned int, entries_to_iterate, MAX_CAPACITY);
	ECS_ASSERT(action_entries.size <= entries_to_iterate.capacity, "Too many Editor shortcut focus entries!");

	// Keep an index array to know which entries to keep looking at
	entries_to_iterate.size = action_entries.size;
	MakeSequence(entries_to_iterate);

	// This array will hold the indices inside the entries array of the current matching sequence
	ECS_STACK_CAPACITY_STREAM(unsigned int, current_matched_indices, MAX_CAPACITY);

	while (entries_to_iterate.size > 0) {
		current_matched_indices.size = 0;

		// Take the first entry and match for the same sandbox index and focus type
		EDITOR_SHORTCUT_FOCUS_TYPE current_focus = action_entries[entries_to_iterate[0]].type;
		unsigned int sandbox_index = action_entries[entries_to_iterate[0]].sandbox_index;
		unsigned char highest_priority = action_entries[entries_to_iterate[0]].priority;
		unsigned int highest_priority_index = entries_to_iterate[0];

		for (unsigned int index = 1; index < entries_to_iterate.size; index++) {
			if (action_entries[entries_to_iterate[index]].type == current_focus && action_entries[entries_to_iterate[index]].sandbox_index == sandbox_index) {
				current_matched_indices.Add(entries_to_iterate[index]);
				if (highest_priority < action_entries[entries_to_iterate[index]].priority) {
					highest_priority = action_entries[entries_to_iterate[index]].priority;
					highest_priority_index = entries_to_iterate[index];
				}
			}
		}

		// Call the functor for the highest priority action
		action_entries[highest_priority_index].action(editor_state, action_entries[highest_priority_index].sandbox_index, action_entries[highest_priority_index].action_data);

		// Remove all the current entries from the iteration indices
		for (unsigned int index = 0; index < current_matched_indices.size; index++) {
			entries_to_iterate.RemoveSwapBackByValue(current_matched_indices[index], "Internal error - Editor shortcut focus entry index is missing!");
		}

		// Remove the initial one as well
		entries_to_iterate.RemoveSwapBack(0);
	}

	// Deallocate the copyables and clear the entries
	for (unsigned int index = 0; index < action_entries.size; index++) {
		CopyableDeallocate(action_entries[index].action_data, &allocator);
	}
	action_entries.Reset();
}

void EditorShortcutFocus::Initialize(AllocatorPolymorphic _allocator) {
	allocator = MemoryManager(ECS_KB * 4, ECS_KB, ECS_KB * 32, _allocator);
	// Use a small initial size
	action_entries.Initialize(&allocator, 4);
	// Initialize with a small value
	id_table.Initialize(&allocator, 4);
}

void EditorShortcutFocus::DecrementIDReferenceCounts() {
	id_table.ForEach([&](IDEntries& id_entries, IDType id_type) {
		ResizableStream<IDEntry>& entries = id_entries.entries;
		if (entries.size > 0) {
			for (unsigned int index = 0; index < entries.size - 1; index++) {
				entries[index].reference_count--;
				if (entries[index].reference_count == 0) {
					// We shouldn't use remove swap back because the last entry signifies
					// The active one.
					entries.Remove(index);
					index--;
				}
			}

			// Test the last entry separately - since for that entry, we should recalculate the active one
			entries[entries.size - 1].reference_count--;
			if (entries[entries.size - 1].reference_count == 0) {
				entries.size--;
				if (entries.size > 0) {
					// Recalculate the highest priority handler
					unsigned int highest_priority_index = 0;
					unsigned char highest_priority = entries[0].priority;
					for (unsigned int index = 1; index < entries.size; index++) {
						if (entries[index].priority > highest_priority) {
							highest_priority = entries[index].priority;
							highest_priority_index = index;
						}
					}

					// Swap the highest priority entry to the last position
					entries.Swap(highest_priority_index, entries.size - 1);
				}
			}
		}
	});
	// We don't have to necessarly remove the entries from the id_table if the entry remains empty
}

bool EditorShortcutFocus::ExistsID(EDITOR_SHORTCUT_FOCUS_TYPE focus_type, unsigned int sandbox_index, unsigned int current_id) {
	IDType id_type = { sandbox_index, focus_type };
	IDEntries entries;
	if (id_table.TryGetValue(id_type, entries)) {
		return entries.entries.Find(current_id, [](IDEntry entry) {
			return entry.id;
		}) != -1;
	}
	return false;
}

bool EditorShortcutFocus::IsIDActive(EDITOR_SHORTCUT_FOCUS_TYPE focus_type, unsigned int sandbox_index, unsigned int current_id) {
	IDType id_type = { sandbox_index, focus_type };
	IDEntries entries;
	if (id_table.TryGetValue(id_type, entries)) {
		return entries.entries.Last().id == current_id;
	}
	return false;
}

unsigned int EditorShortcutFocus::RegisterForAction(EDITOR_SHORTCUT_FOCUS_TYPE focus_type, unsigned int sandbox_index, unsigned char priority) {
	IDType id_type = { sandbox_index, focus_type };
	IDEntries* entries;
	if (!id_table.TryGetValuePtr(id_type, entries)) {
		IDEntries default_entries;
		default_entries.current_id = 0;
		default_entries.has_overflowed = false;
		default_entries.entries.Initialize(&allocator, 2);
		id_table.InsertDynamic(&allocator, default_entries, id_type);
		entries = id_table.GetValuePtr(id_type);
	}
	
	unsigned int entry_id = entries->current_id;
	if (entries->has_overflowed) {
		// We need to ensure that we don't collide an existing ID, shouldn't really happen, but as a safety measure, use it
		while (entries->entries.Find(entry_id, [](IDEntry entry) {
			return entry.id;
			}) != -1) {
			// Keep incrementing the ID
			entries->current_id++;
			entry_id++;
		}
	}
	
	entries->current_id++;
	if (entries->current_id == 0) {
		entries->has_overflowed = true;
	}

	// Insert the entry with a reference count of 2
	entries->entries.Add({ priority, 2, entry_id });
	// Compare the entry with the previous last entry - which was the highest priority one. If this has a lower or equal priority, then swap them
	if (entries->entries.size > 1) {
		if (entries->entries[entries->entries.size - 2].priority >= priority) {
			entries->entries.Swap(entries->entries.size - 2, entries->entries.size - 1);
		}
	}
	return entry_id;
}

unsigned int EditorShortcutFocus::IncrementOrRegisterForAction(EDITOR_SHORTCUT_FOCUS_TYPE focus_type, unsigned int sandbox_index, unsigned char priority, unsigned int current_id) {
	IDType id_type = { sandbox_index, focus_type };
	IDEntries entries;
	if (id_table.TryGetValue(id_type, entries)) {
		unsigned int entry_index = entries.entries.Find(current_id, [](IDEntry entry) {
			return entry.id;
			});
		if (entry_index != -1) {
			entries.entries[entry_index].reference_count++;
			return current_id;
		}
		else {
			// Register a new entry
			return RegisterForAction(focus_type, sandbox_index, priority);
		}
	}
	return RegisterForAction(focus_type, sandbox_index, priority);
}

void TickEditorShortcutFocus(EditorState* editor_state) {
	editor_state->shortcut_focus.HandleActionEntries(editor_state);
	editor_state->shortcut_focus.DecrementIDReferenceCounts();
}