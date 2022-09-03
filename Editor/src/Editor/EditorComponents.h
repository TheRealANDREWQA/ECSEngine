#pragma once
#include "ECSEngineContainers.h"

namespace ECSEngine {
	struct EntityManager;
	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}
}

enum EDITOR_COMPONENT_CONFLICT : unsigned char {
	EDITOR_COMPONENT_CONFLICT_HAS_CHANGED,
	EDITOR_COMPONENT_CONFLICT_IS_REMOVED,
	EDITOR_COMPONENT_CONFLICT_SAME_ID,
	EDITOR_COMPONENT_CONFLICT_COUNT
};

struct EditorComponentConflict {
	ECSEngine::Stream<char> first_component;
	ECSEngine::Stream<char> second_component;
	EDITOR_COMPONENT_CONFLICT conflict;
};

// Keeps the reflection_type as well such that when a component is changed the data can still be appropriately preserved
struct EditorComponents {
	struct ComponentData {
		unsigned short id;
		unsigned short byte_size;
		ECSEngine::Reflection::ReflectionType* reflection_type;
	};

	struct LoadedModule {
		ECSEngine::Stream<char> name;
		ECSEngine::Stream<ECSEngine::Stream<char>> components;
	};

	// Returns true if the component name contains non trivially copyable types
	bool HasBuffers(ECSEngine::Stream<char> component_name) const;

	// Returns -1 if it doesn't find it
	unsigned short GetComponentID(ECSEngine::Stream<char> name) const;

	// Returns -1 if it doesn't find it
	unsigned short GetSharedComponentID(ECSEngine::Stream<char> name) const;

	unsigned short GetComponentByteSize(ECSEngine::Stream<char> name) const;

	unsigned short GetSharedComponentByteSize(ECSEngine::Stream<char> name) const;

	// Returns the index inside the loaded_modules if the module has components recorded here, else -1.
	unsigned int IsModule(ECSEngine::Stream<char> name) const;

	// Returns true if the component has changed since last time, else false.
	bool NeedsResolveConflict(ECSEngine::Stream<char> component, const ECSEngine::Reflection::ReflectionManager* reflection_manager) const;

	void RemoveComponent(ECSEngine::Stream<char> name);

	// Modifies the current data stored in the entity manager such that it conforms to the new component
	// Tries to maintain the data already stored
	void ResolveConflict(ECSEngine::EntityManager* entity_manager, const ECSEngine::Reflection::ReflectionManager* reflection_manager, ECSEngine::Stream<char> component_name);

	// The conflict will refer to the name already stored in the EditorComponents
	// or in the reflection_manager for the same_id component conflict
	void UpdateComponents(
		const ECSEngine::Reflection::ReflectionManager* reflection_manager,
		unsigned int hierarchy_index,
		ECSEngine::Stream<char> module_name,
		ECSEngine::CapacityStream<EditorComponentConflict>& conflicts
	);

	// Initializes a default allocator. The size must be the one given from default allocator size
	void Initialize(void* buffer);

	void Initialize(ECSEngine::AllocatorPolymorphic allocator);

	static size_t DefaultAllocatorSize();

	ECSEngine::AllocatorPolymorphic allocator;
	ECSEngine::HashTableDefault<ComponentData> components;
	ECSEngine::HashTableDefault<ComponentData> shared_components;
	ECSEngine::ResizableStream<LoadedModule> loaded_modules;
};