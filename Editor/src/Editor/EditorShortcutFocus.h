#pragma once
#include "ECSEngineContainersCommon.h"

struct EditorState;

enum EDITOR_SHORTCUT_FOCUS_TYPE : unsigned char {
	EDITOR_SHORTCUT_FOCUS_SANDBOX_BASIC_OPERATIONS,
	EDITOR_SHORTCUT_FOCUS_TYPE_COUNT
};

// This enum is more of a convenience, in order to not inject magic constants throughout the code
enum EDITOR_SHORTCUT_FOCUS_PRIORITY : unsigned char {
	EDITOR_SHORTCUT_FOCUS_PRIORITY0,
	EDITOR_SHORTCUT_FOCUS_PRIORITY1,
	EDITOR_SHORTCUT_FOCUS_PRIORITY2,
	EDITOR_SHORTCUT_FOCUS_PRIORITY3,
	EDITOR_SHORTCUT_FOCUS_PRIORITY4,
};

typedef void (*EditorShortcutFocusAction)(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Copyable* data);

// A structure that help in arbitrating between multiple shortcut handlers, either by registering the action and then performing the solve
// At that point, or by requesting priority IDs.
struct EditorShortcutFocus {
	// This is not threadsafe, must be used on a single thread!
	void AddActionEntry(EditorShortcutFocusAction action, const ECSEngine::Copyable* action_data, EDITOR_SHORTCUT_FOCUS_TYPE focus_type, unsigned int sandbox_index, unsigned char priority);

	// Convenience function that converts a lambda (that has as parameters (EditorState*, unsigned int sandbox_index)) into a shortcut action and its copyable data. The lambda
	// Must take any local scope variables by value, not by reference, otherwise this will fail!
	// This is not threadsafe, must be used on a single thread!
	template<typename Lambda>
	void AddActionEntry(EDITOR_SHORTCUT_FOCUS_TYPE focus_type, unsigned int sandbox_index, unsigned char priority, Lambda&& lambda) {
		// Create a wrapper struct that represents a copyable without any buffers that are required
		struct Wrapper : Copyable {
			ECS_INLINE Wrapper(Lambda&& _lambda) : Copyable(sizeof(Lambda)), lambda(_lambda) {}

			void CopyBuffers(const void* other, AllocatorPolymorphic allocator) override {}

			void DeallocateBuffers(AllocatorPolymorphic allocator) override {}

			Lambda lambda;
		};

		auto wrapper_callback = [](EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Copyable* data) {
			Wrapper* wrapper = (Wrapper*)data;
			wrapper->lambda(editor_state, sandbox_index);
		};
		Wrapper wrapper(lambda);
		AddActionEntry(wrapper_callback, &wrapper, focus_type, sandbox_index, priority);
	}

	void HandleActionEntries(EditorState* editor_state);

	void Initialize(ECSEngine::AllocatorPolymorphic allocator);

	void DecrementIDReferenceCounts();

	// Returns true if the ID of for the specified action and sandbox is valid, else false
	bool ExistsID(EDITOR_SHORTCUT_FOCUS_TYPE focus_type, unsigned int sandbox_index, unsigned int current_id);

	// If the current_id is not a valid ID, then it will register for a new ID and returns that value. If it is valid, then it will increment the reference count
	// For that ID such that it doesn't get removed and returns the same ID value.
	unsigned int IncrementOrRegisterForAction(EDITOR_SHORTCUT_FOCUS_TYPE focus_type, unsigned int sandbox_index, unsigned char priority, unsigned int current_id);

	// Returns true if the given ID is the active one for the specified action type, else false
	bool IsIDActive(EDITOR_SHORTCUT_FOCUS_TYPE focus_type, unsigned int sandbox_index, unsigned int current_id);

	// Returns a new ID for an action.
	unsigned int RegisterForAction(EDITOR_SHORTCUT_FOCUS_TYPE focus_type, unsigned int sandbox_index, unsigned char priority);

	struct ActionEntry {
		// If the focus is tied to a specific sandbox, this will differentiate between them
		// If the entry is global per editor, then this value should be -1
		unsigned int sandbox_index;
		// Higher priorities have higher precedence over lower ones (i.e. higher priorities will be chosen over lower ones).
		// When there are multiple entries with the same priority, one of them will be chosen, without any particular reason (this is why
		// Avoid having the same priority if you don't want randomness - unless the actions are the same, in which case they can share the priority)
		unsigned char priority;
		EDITOR_SHORTCUT_FOCUS_TYPE type;
		ECSEngine::Copyable* action_data;
		EditorShortcutFocusAction action;
	};

	struct IDType {
		ECS_INLINE unsigned int Hash() const {
			return Cantor(sandbox_index, (unsigned int)type);
		}

		ECS_INLINE bool operator == (IDType other) const {
			return sandbox_index == other.sandbox_index && type == other.type;
		}

		unsigned int sandbox_index;
		EDITOR_SHORTCUT_FOCUS_TYPE type;
	};

	struct IDEntry {
		unsigned char priority;
		// The reference count is used to automatically remove entries that no longer register themselves
		unsigned char reference_count;
		unsigned int id;
	};

	struct IDEntries {
		unsigned int current_id;
		// This boolean is used as an optimization - when we overflow, perform some checks to ensure that we don't collide the IDs
		bool has_overflowed;
		ECSEngine::ResizableStream<IDEntry> entries;
	};

	typedef ECSEngine::HashTable<IDEntries, IDType, ECSEngine::HashFunctionPowerOfTwo> IDTable;

	// This is a small allocator maintained in order to reduce pressure on the main allocator
	MemoryManager allocator;
	ECSEngine::ResizableStream<ActionEntry> action_entries;
	IDTable id_table;
};

void TickEditorShortcutFocus(EditorState* editor_state);