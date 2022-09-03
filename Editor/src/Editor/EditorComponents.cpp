#include "editorpch.h"
#include "EditorComponents.h"
#include "ECSEngineReflection.h"
#include "EditorComponents.h"

using namespace ECSEngine;
using namespace ECSEngine::Reflection;

#define HASH_TABLE_DEFAULT_CAPACITY 64

#define ARENA_CAPACITY 50'000
#define ARENA_COUNT 4
#define ARENA_BLOCK_COUNT 1024

// ----------------------------------------------------------------------------------------------

unsigned short EditorComponents::GetComponentID(Stream<char> name) const
{
	ComponentData data;
	if (components.TryGetValue(name, data)) {
		return data.id;
	}
	return -1;
}

// ----------------------------------------------------------------------------------------------

unsigned short EditorComponents::GetSharedComponentID(Stream<char> name) const
{
	ComponentData data;
	if (shared_components.TryGetValue(name, data)) {
		return data.id;
	}
	return -1;
}

// ----------------------------------------------------------------------------------------------

unsigned short EditorComponents::GetComponentByteSize(Stream<char> name) const
{
	ComponentData data;
	if (components.TryGetValue(name, data)) {
		return data.byte_size;
	}
	return -1;
}

// ----------------------------------------------------------------------------------------------

unsigned short EditorComponents::GetSharedComponentByteSize(Stream<char> name) const
{
	ComponentData data;
	if (shared_components.TryGetValue(name, data)) {
		return data.byte_size;
	}
	return -1;
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::IsModule(Stream<char> name) const
{
	for (unsigned int index = 0; index < loaded_modules.size; index++) {
		if (function::CompareStrings(loaded_modules[index].name, name)) {
			return index;
		}
	}
	return -1;
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::HasBuffers(Stream<char> name) const
{
	ComponentData component_data;
	if (components.TryGetValue(name, component_data)) {

	}
	else {
		if (shared_components.TryGetValue(name, component_data)) {

		}
		else {
			// Not a component or not recorded
			return false;
		}
	}
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::NeedsResolveConflict(Stream<char> component, const ReflectionManager* reflection_manager) const
{
	return false;
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::RemoveComponent(Stream<char> name)
{
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ResolveConflict(EntityManager* entity_manager, const ReflectionManager* reflection_manager, Stream<char> component_name)
{
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::Initialize(void* buffer)
{
	MemoryArena* arena = (MemoryArena*)buffer;
	*arena = MemoryArena(function::OffsetPointer(arena, sizeof(*arena)), ARENA_CAPACITY, ARENA_COUNT, ARENA_BLOCK_COUNT);
	Initialize(GetAllocatorPolymorphic(arena));
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::Initialize(AllocatorPolymorphic _allocator)
{
	allocator = _allocator;
	components.Initialize(allocator, HASH_TABLE_DEFAULT_CAPACITY);
	shared_components.Initialize(allocator, HASH_TABLE_DEFAULT_CAPACITY);
	loaded_modules.Initialize(allocator, 0);
}

// ----------------------------------------------------------------------------------------------

size_t EditorComponents::DefaultAllocatorSize()
{
	return sizeof(MemoryArena) + MemoryArena::MemoryOf(ARENA_CAPACITY, ARENA_COUNT, ARENA_BLOCK_COUNT);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::UpdateComponents(
	const ReflectionManager* reflection_manager, 
	unsigned int hierarchy_index, 
	Stream<char> module_name,
	CapacityStream<EditorComponentConflict>& conflicts
)
{
	// Check to see if the module has any components already stored
	bool is_module = IsModule(module_name);
	if (is_module) {
		// Get their 
	}

	ECS_STACK_CAPACITY_STREAM(unsigned int, component_indices, 512);
	ECS_STACK_CAPACITY_STREAM(unsigned int, shared_indices, 512);
	reflection_manager->GetHierarchyComponentTypes(hierarchy_index, &component_indices, &shared_indices);

	components.Initialize(allocator, component_indices.size);
	shared_components.Initialize(allocator, shared_indices.size);

	//auto register_components = [&](Stream<SearchPair>& pair, CapacityStream<unsigned int> indices) {
	//	for (unsigned int index = 0; index < indices.size; index++) {
	//		const ReflectionType* type = reflection_manager->GetType(indices[index]);
	//		double evaluation = type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
	//		ECS_ASSERT(evaluation != DBL_MAX, "Engine component is missing ID function");

	//		// Can alias the name from the reflection. That hierarchy will not get removed
	//		pair[index].name = type->name;
	//		pair[index].id = (unsigned short)evaluation;
	//		pair[index].byte_size = GetReflectionTypeByteSize(type);
	//	}
	//};

	//register_components(components, component_indices);
	//register_components(shared_components, shared_indices);
}

// ----------------------------------------------------------------------------------------------
