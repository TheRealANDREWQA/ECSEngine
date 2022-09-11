#include "editorpch.h"
#include "ECSEngineReflection.h"
#include "EditorComponents.h"
#include "EditorState.h"

#include "../Modules/Module.h"
#include "ECSEngineSerialization.h"
#include "ECSEngineSerializationHelpers.h"

using namespace ECSEngine;
using namespace ECSEngine::Reflection;

#define HASH_TABLE_DEFAULT_CAPACITY 64

#define ARENA_CAPACITY 50'000
#define ARENA_COUNT 4
#define ARENA_BLOCK_COUNT 1024

// ----------------------------------------------------------------------------------------------

Component EditorComponents::GetComponentID(Stream<char> name) const
{
	ReflectionType type;
	if (internal_manager->type_definitions.TryGetValue(name, type)) {
		double evaluation = type.GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
		if (evaluation == DBL_MAX) {
			return Component{ -1 };
		}
		return Component{ (short)evaluation };
	}
	return Component{ -1 };
}

// ----------------------------------------------------------------------------------------------

unsigned short EditorComponents::GetComponentByteSize(Stream<char> name) const
{
	ReflectionType type;
	if (internal_manager->type_definitions.TryGetValue(name, type)) {
		return (unsigned short)GetReflectionTypeByteSize(&type);
	}
	return USHORT_MAX;
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetUserEvents(CapacityStream<EditorComponentEvent>& user_events)
{
	for (int32_t index = 0; index < (int32_t)events.size; index++) {
		if (!IsEditorComponentHandledInternally(events[index].type)) {
			user_events.Add(events[index]);
			events.RemoveSwapBack(index);
		}
	}

	user_events.AssertCapacity();
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

bool EditorComponents::IsComponent(Stream<char> name) const
{
	ReflectionType type;
	if (internal_manager->TryGetType(name, type)) {
		if (IsReflectionTypeComponent(&type) || IsReflectionTypeSharedComponent(&type)) {
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::IsUniqueComponent(Stream<char> name) const
{
	ReflectionType type;
	if (internal_manager->TryGetType(name, type)) {
		if (IsReflectionTypeComponent(&type)) {
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::IsSharedComponent(Stream<char> name) const
{
	ReflectionType type;
	if (internal_manager->TryGetType(name, type)) {
		if (IsReflectionTypeSharedComponent(&type)) {
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::RecoverData(EntityManager* entity_manager, const ReflectionManager* reflection_manager, Stream<char> component_name, Stream<SpinLock> locks)
{
	const ReflectionType* current_type = reflection_manager->GetType(component_name);
	ReflectionType* old_type = internal_manager->GetType(component_name);

	Component component = { (unsigned short)old_type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };

	size_t new_allocator_size = (size_t)current_type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
	size_t old_allocator_size = (size_t)old_type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);

	// Use a stack allocation in order to write the field table
	// It is a bit inefficient to write the table to then read it again
	// But it is not worth creating a separate method for that
	// Especially since not a lot of changes are going to be done
	const size_t ALLOCATION_SIZE = ECS_KB * 32;
	void* stack_allocation = ECS_STACK_ALLOC(ALLOCATION_SIZE);
	uintptr_t ptr = (uintptr_t)stack_allocation;

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 8);
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&stack_allocator);

	SerializeFieldTable(internal_manager, old_type, ptr);
	ptr = (uintptr_t)stack_allocation;
	DeserializeFieldTable field_table = DeserializeFieldTableFromData(ptr, allocator);
	ECS_ASSERT(field_table.types.size > 0);

	// Used when needing to move buffers of data from components
	const size_t TEMPORARY_ALLOCATION_CAPACITY = ECS_MB * 500;
	void* temporary_allocation = malloc(ECS_MB * 500);
	//LinearAllocator temporary_allocator(temporary_allocation, TEMPORARY_ALLOCATION_CAPACITY);
	//AllocatorPolymorphic temporary_alloc = GetAllocatorPolymorphic(&temporary_allocator);

	DeserializeOptions deserialize_options;
	deserialize_options.read_type_table = false;
	deserialize_options.verify_dependent_types = false;
	deserialize_options.backup_allocator = allocator;
	deserialize_options.field_allocator = allocator;
	deserialize_options.field_table = &field_table;

	SerializeOptions serialize_options;
	serialize_options.write_type_table = false;
	serialize_options.verify_dependent_types = false;

	ECS_STACK_CAPACITY_STREAM(unsigned int, matching_archetypes, ECS_MAIN_ARCHETYPE_MAX_COUNT);
	ComponentSignature component_signature;
	component_signature.indices = &component;
	component_signature.count = 1;

	bool has_buffers = HasBuffers(component_name);
	bool has_locks = locks.size > 0;

	if (IsReflectionTypeComponent(old_type)) {
		ArchetypeQuery query;
		query.unique.ConvertComponents(component_signature);
		entity_manager->GetArchetypes(query, matching_archetypes);

		size_t old_size = GetReflectionTypeByteSize(old_type);
		size_t new_size = GetReflectionTypeByteSize(current_type);

		auto has_buffer_prepass = [&]() {
			ptr = (uintptr_t)temporary_allocation;

			// Need to go through all entities and serialize them because we need to deallocate the component allocator
			// and then reallocate all the data
			for (unsigned int index = 0; index < matching_archetypes.size; index++) {
				if (has_locks) {
					// Lock the archetype
					locks[matching_archetypes[index]].lock();
				}
				Archetype* archetype = entity_manager->GetArchetype(matching_archetypes[index]);
				unsigned char component_index = archetype->FindUniqueComponentIndex(component);

				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					ArchetypeBase* base = archetype->GetBase(base_count);
					unsigned int entity_count = base->EntityCount();

					const void* component = base->GetComponentByIndex(0, component_index);
					for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
						Serialize(internal_manager, old_type, function::OffsetPointer(component, entity_index * old_size), ptr, &serialize_options);
					}
				}

				if (has_locks) {
					locks[matching_archetypes[index]].unlock();
				}
			}

			ECS_ASSERT(ptr - (uintptr_t)temporary_allocation <= TEMPORARY_ALLOCATION_CAPACITY);

			MemoryArena* arena = entity_manager->GetComponentAllocator(component);
			if (new_allocator_size == old_allocator_size) {
				arena->Clear();
			}
			else {
				if (has_locks) {
					// Lock the memory manager allocator
					entity_manager->m_memory_manager->Lock();
				}

				// Resize the arena
				arena = entity_manager->ResizeComponentAllocator(component, new_allocator_size);

				if (has_locks) {
					entity_manager->m_memory_manager->Unlock();
				}
			}

			if (arena != nullptr) {
				AllocatorPolymorphic component_allocator = GetAllocatorPolymorphic(arena);
				deserialize_options.field_allocator = component_allocator;
				deserialize_options.backup_allocator = component_allocator;
			}
		};

		if (new_size == old_size) {
			// No need to resize the archetypes
			if (has_buffers) {
				has_buffer_prepass();

				ptr = (uintptr_t)temporary_allocation;

				// Now deserialize the data
				for (unsigned int index = 0; index < matching_archetypes.size; index++) {
					if (has_locks) {
						locks[matching_archetypes[index]].lock();
					}

					Archetype* archetype = entity_manager->GetArchetype(matching_archetypes[index]);
					unsigned char component_index = archetype->FindUniqueComponentIndex(component);

					unsigned int base_count = archetype->GetBaseCount();
					for (unsigned int base_index = 0; base_index < base_count; base_index++) {
						ArchetypeBase* base = archetype->GetBase(base_count);
						unsigned int entity_count = base->EntityCount();

						void* component = base->GetComponentByIndex(0, component_index);
						for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
							ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, current_type, function::OffsetPointer(component, entity_index * old_size), ptr, &deserialize_options);
							ECS_ASSERT(code == ECS_DESERIALIZE_OK);
						}
					}

					if (has_locks) {
						locks[matching_archetypes[index]].unlock();
					}
				}
			}
			else {
				MemoryArena* arena = nullptr;
				if (new_allocator_size != old_allocator_size) {
					if (has_locks) {
						entity_manager->m_memory_manager->Lock();
					}

					// Need to resize the component allocator
					arena = entity_manager->ResizeComponentAllocator(component, new_allocator_size);

					if (has_locks) {
						entity_manager->m_memory_manager->Unlock();
					}
				}
				else {
					arena = entity_manager->GetComponentAllocator(component);
				}
				
				if (arena != nullptr) {
					AllocatorPolymorphic alloc = GetAllocatorPolymorphic(arena);
					deserialize_options.field_allocator = alloc;
					deserialize_options.backup_allocator = alloc;
				}

				for (unsigned int index = 0; index < matching_archetypes.size; index++) {
					if (has_locks) {
						locks[matching_archetypes[index]].lock();
					}
					Archetype* archetype = entity_manager->GetArchetype(matching_archetypes[index]);
					unsigned char component_index = archetype->FindUniqueComponentIndex(component);

					unsigned int base_count = archetype->GetBaseCount();
					for (unsigned int base_index = 0; base_index < base_count; base_index++) {
						ArchetypeBase* base = archetype->GetBase(base_count);
						unsigned int entity_count = base->EntityCount();

						const void* component = base->GetComponentByIndex(0, component_index);
						for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
							ptr = (uintptr_t)temporary_allocation;
							// Copy to temporary memory and then deserialize back
							Serialize(internal_manager, old_type, function::OffsetPointer(component, entity_index * old_size), ptr, &serialize_options);
							ptr = (uintptr_t)temporary_allocation;
							Deserialize(reflection_manager, current_type, function::OffsetPointer(component, entity_index * old_size), ptr, &deserialize_options);
						}
					}

					if (has_locks) {
						locks[matching_archetypes[index]].unlock();
					}
				}
			}
		}
		else {
			entity_manager->m_unique_components[component.value].size = new_size;

			// First functor receives (void* destination, const void* source, size_t copy_size) and must return how many bytes it wrote
			// Second functor receives the void* to the new component memory and a void* to the old data
			auto resize_archetype = [&](auto same_component_copy, auto same_component_handler) {
				for (unsigned int index = 0; index < matching_archetypes.size; index++) {
					if (has_locks) {
						locks[matching_archetypes[index]].lock();
					}
					Archetype* archetype = entity_manager->GetArchetype(matching_archetypes[index]);
					ComponentSignature current_signature = archetype->GetUniqueSignature();

					ECS_STACK_CAPACITY_STREAM(unsigned short, component_sizes, 256);
					for (unsigned int component_index = 0; component_index < current_signature.count; component_index++) {
						component_sizes[component_index] = entity_manager->ComponentSize(current_signature.indices[component_index]);
					}

					unsigned int base_count = archetype->GetBaseCount();
					for (unsigned int base_index = 0; base_index < base_count; base_index++) {
						ArchetypeBase* base = archetype->GetBase(base_count);
						unsigned int entity_count = base->EntityCount();
						void** previous_base_buffers = base->m_buffers;

						// Need to copy these on the stack because they will get deallocated when resizing
						ECS_STACK_CAPACITY_STREAM(void*, previous_buffers, 64);
						memcpy(previous_buffers.buffer, previous_base_buffers, sizeof(void*) * current_signature.count);

						uintptr_t copy_ptr = ptr;

						for (unsigned int component_index = 0; component_index < current_signature.count; component_index++) {
							size_t copy_size = (size_t)component_sizes[component_index] * (size_t)entity_count;
							if (current_signature.indices[component_index] != component) {
								memcpy((void*)copy_ptr, previous_buffers[component_index], copy_size);
								copy_ptr += copy_size;
							}
							else {
								copy_ptr += same_component_copy((void*)copy_ptr, previous_buffers[component_index], copy_size);
							}
						}

						unsigned int previous_capacity = base->m_capacity;
						base->m_size = 0;

						// The size 0 it tells the base to just resize.
						// Now everything will be in its place
						base->Resize(previous_capacity);

						void** new_buffers = base->m_buffers;

						// Now copy back
						copy_ptr = ptr;
						for (unsigned int component_index = 0; component_index < current_signature.count; component_index++) {
							if (current_signature.indices[component_index] != component) {
								size_t copy_size = (size_t)component_sizes[component_index] * (size_t)entity_count;
								memcpy(new_buffers[component_index], previous_buffers[component_index], copy_size);
								copy_ptr += copy_size;
							}
							else {
								// Need to deserialize
								for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
									same_component_handler(
										function::OffsetPointer(new_buffers[component_index], entity_index * new_size),
										function::OffsetPointer(previous_buffers[component_index], entity_index * old_size)
									);
								}
							}
						}
					}

					if (has_locks) {
						locks[matching_archetypes[index]].unlock();
					}
				}
			};

			// Need to relocate the archetypes and update the size
			if (has_buffers) {
				// This is kinda complicated. Need to do a prepass and serialize all the data from the current component after which
				// we must clear the allocator, resize it if necessary. Then the new size for the component needs to be recorded and then
				// resize each base archetype separately with a manual resize
				has_buffer_prepass();

				uintptr_t deserialized_ptr = (uintptr_t)temporary_allocation;
				// Previous buffer not used
				auto deserialize_handler = [&](void* pointer, void* previous_data) {
					ECS_DESERIALIZE_CODE code = Deserialize(
						reflection_manager,
						current_type,
						pointer,
						deserialized_ptr,
						&deserialize_options
					);
					ECS_ASSERT(code == ECS_DESERIALIZE_OK);
				};
				
				auto no_copy = [](void* destination, const void* source, size_t copy_size) {
					return 0;
				};

				resize_archetype(no_copy, deserialize_handler);
			}
			else {
				MemoryArena* arena = nullptr;
				if (new_allocator_size != old_allocator_size) {
					if (has_locks) {
						entity_manager->m_memory_manager->Lock();
					}

					// Need to resize the component allocator
					arena = entity_manager->ResizeComponentAllocator(component, new_allocator_size);

					if (has_locks) {
						entity_manager->m_memory_manager->Unlock();
					}
				}
				else {
					arena = entity_manager->GetComponentAllocator(component);
				}

				if (arena != nullptr) {
					AllocatorPolymorphic alloc = GetAllocatorPolymorphic(arena);
					deserialize_options.field_allocator = alloc;
					deserialize_options.backup_allocator = alloc;
				}

				auto initial_copy_same_component = [](void* destination, const void* source, size_t copy_size) {
					memcpy(destination, source, copy_size);
					return copy_size;
				};

				auto deserialized_copy = [&](void* pointer, void* previous_buffer) {
					ptr = (uintptr_t)temporary_allocation;
					Serialize(
						internal_manager,
						old_type,
						previous_buffer,
						ptr,
						&serialize_options
					);

					ptr = (uintptr_t)temporary_allocation;
					ECS_DESERIALIZE_CODE code = Deserialize(
						reflection_manager,
						current_type,
						pointer,
						ptr,
						&deserialize_options
					);
					ECS_ASSERT(code == ECS_DESERIALIZE_OK);
				};

				resize_archetype(initial_copy_same_component, deserialized_copy);
			}
		}
	}
	else {
		size_t old_size = GetReflectionTypeByteSize(old_type);
		size_t new_size = GetReflectionTypeByteSize(current_type);

		if (old_size == new_size) {
			if (has_buffers) {
				// Must a do a prepass and serialize the components into the temporary buffer
				ptr = (uintptr_t)temporary_allocation;
				entity_manager->m_shared_components[component.value].instances.stream.ForEachConst([&](void* data) {
					Serialize(internal_manager, old_type, data, ptr, &serialize_options);
				});

				// Deallocate the arena or resize it
				MemoryArena* arena = nullptr;
				if (new_allocator_size != old_allocator_size) {
					if (has_locks) {
						entity_manager->m_memory_manager->Lock();
					}
					arena = entity_manager->ResizeSharedComponentAllocator(component, new_allocator_size);
					if (has_locks) {
						entity_manager->m_memory_manager->Unlock();
					}
				}
				else {
					arena = entity_manager->GetSharedComponentAllocator(component);
					if (arena != nullptr) {
						arena->Clear();
					}
				}

				if (arena != nullptr) {
					AllocatorPolymorphic alloc = GetAllocatorPolymorphic(arena);
					deserialize_options.backup_allocator = alloc;
					deserialize_options.field_allocator = alloc;
				}

				ptr = (uintptr_t)temporary_allocation;
				entity_manager->m_shared_components[component.value].instances.stream.ForEachConst([&](void* data) {
					ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, current_type, data, ptr, &deserialize_options);
					ECS_ASSERT(code == ECS_DESERIALIZE_OK);
				});
			}
			else {
				MemoryArena* arena = nullptr;
				if (new_allocator_size != old_allocator_size) {
					if (has_locks) {
						entity_manager->m_memory_manager->Lock();
					}
					arena = entity_manager->ResizeSharedComponentAllocator(component, new_allocator_size);
					if (has_locks) {
						entity_manager->m_memory_manager->Unlock();
					}
				}
				else {
					arena = entity_manager->GetSharedComponentAllocator(component);
				}

				if (arena != nullptr) {
					AllocatorPolymorphic alloc = GetAllocatorPolymorphic(arena);
					deserialize_options.backup_allocator = alloc;
					deserialize_options.field_allocator = alloc;
				}

				entity_manager->m_shared_components[component.value].instances.stream.ForEachConst([&](void* data) {
					ptr = (uintptr_t)temporary_allocation;
					Serialize(internal_manager, old_type, data, ptr, &serialize_options);
					ptr = (uintptr_t)temporary_allocation;
					ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, current_type, data, ptr, &deserialize_options);
					ECS_ASSERT(code == ECS_DESERIALIZE_OK);
				});
			}
		}
		else {
			// Need to reallocate the shared instances
			// First copy all of them into a temporary buffer and then deserialize from it
			ptr = (uintptr_t)temporary_allocation;
			entity_manager->m_shared_components[component.value].instances.stream.ForEachConst([&](void* data) {
				Serialize(internal_manager, old_type, data, ptr, &serialize_options);
			});

			if (has_locks) {
				entity_manager->m_memory_manager->Lock();
			}
			entity_manager->ResizeSharedComponent(component, new_size);
			if (has_locks) {
				entity_manager->m_memory_manager->Unlock();
			}

			entity_manager->m_shared_components[component.value].instances.stream.ForEachConst([&](void* data) {
				ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, current_type, data, ptr, &deserialize_options);
				ECS_ASSERT(code == ECS_DESERIALIZE_OK);
			});
		}
	}

	// Now we need to update this type to be the new one
	DeallocateTs(allocator.allocator, allocator.allocator_type, old_type->name.buffer);
	size_t copy_size = current_type->CopySize();
	void* allocation = AllocateTs(allocator.allocator, allocator.allocator_type, copy_size);
	ptr = (uintptr_t)allocation;
	*old_type = current_type->Copy(ptr);

	free(temporary_allocation);
	stack_allocator.ClearBackup();
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::AddComponentToManager(EntityManager* entity_manager, Stream<char> component_name)
{
	const ReflectionType* internal_type = internal_manager->GetType(component_name);
	bool is_component = IsReflectionTypeComponent(internal_type);
	
	Component component = { (short)internal_type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
	double allocator_size_d = internal_type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
	size_t allocator_size = allocator_size_d == DBL_MAX ? 0 : (size_t)allocator_size_d;

	// Lock the small memory manager in order to commit the type
	entity_manager->m_small_memory_manager.Lock();
	if (is_component) {
		entity_manager->RegisterComponentCommit(component, GetReflectionTypeByteSize(internal_type), allocator_size);
	}
	else {
		entity_manager->RegisterSharedComponentCommit(component, GetReflectionTypeByteSize(internal_type), allocator_size);
	}
	entity_manager->m_small_memory_manager.Unlock();
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ChangeComponentID(EntityManager* entity_manager, Stream<char> component_name, short new_id)
{
	ReflectionType* type = internal_manager->type_definitions.GetValuePtr(component_name);
	unsigned short byte_size = (unsigned short)GetReflectionTypeByteSize(type);

	bool is_component = IsReflectionTypeComponent(type);

	double allocator_size_float = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
	size_t allocator_size = 0;

	unsigned short old_id = type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
	ECS_ASSERT(old_id != DBL_MAX);

	Component old_component = { old_id };
	Component new_component = { new_id };

	MemoryArena* arena = nullptr;
	if (is_component) {
		arena = entity_manager->GetComponentAllocator(old_component);
		// Always use size 0 allocator size because we will transfer the arena to the new slot
		// In this way we avoid creating a new allocator and transfer all the existing data to it

		// We also need to lock the small memory manager
		entity_manager->m_small_memory_manager.Lock();
		entity_manager->RegisterComponentCommit(new_component, byte_size, 0);

		if (arena != nullptr) {
			entity_manager->m_unique_components[old_id].allocator = nullptr;
			entity_manager->m_unique_components[new_id].allocator = arena;
		}

		// Now destroy the old component
		entity_manager->UnregisterComponentCommit(old_component);
		entity_manager->m_small_memory_manager.Unlock();
	}
	else {
		arena = entity_manager->GetSharedComponentAllocator(old_component);

		entity_manager->m_small_memory_manager.Lock();
		entity_manager->RegisterSharedComponentCommit(new_component, byte_size, 0);

		if (arena != nullptr) {
			entity_manager->m_shared_components[old_id].info.allocator = nullptr;
			entity_manager->m_shared_components[new_id].info.allocator = arena;
		}

		// Now destroy the old component
		entity_manager->UnregisterSharedComponentCommit(old_component);
		entity_manager->m_small_memory_manager.Unlock();
	}
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::EmptyEventStream()
{
	events.FreeBuffer();
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ForEachComponent(void (*Functor)(ReflectionType* type, void* _data), void* _data)
{
	internal_manager->type_definitions.ForEach([&](ReflectionType& type, ResourceIdentifier identifier) {
		if (IsReflectionTypeComponent(&type)) {
			Functor(&type, _data);
		}
	});
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ForEachComponent(void (*Functor)(const ReflectionType* type, void* _data), void* _data) const
{
	internal_manager->type_definitions.ForEachConst([&](const ReflectionType& type, ResourceIdentifier identifier) {
		if (IsReflectionTypeComponent(&type)) {
			Functor(&type, _data);
		}
	});
}

// ----------------------------------------------------------------------------------------------


void EditorComponents::ForEachSharedComponent(void(*Functor)(ECSEngine::Reflection::ReflectionType* type, void* _data), void* _data)
{
	internal_manager->type_definitions.ForEach([&](ReflectionType& type, ResourceIdentifier identifier) {
		if (IsReflectionTypeSharedComponent(&type)) {
			Functor(&type, _data);
		}
	});
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ForEachSharedComponent(void(*Functor)(const ECSEngine::Reflection::ReflectionType* type, void* _data), void* _data) const
{
	internal_manager->type_definitions.ForEachConst([&](const ReflectionType& type, ResourceIdentifier identifier) {
		if (IsReflectionTypeSharedComponent(&type)) {
			Functor(&type, _data);
		}
	});
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::HasBuffers(Stream<char> name) const
{
	ReflectionType reflection_type;
	if (internal_manager->type_definitions.TryGetValue(name, reflection_type)) {
		return !IsTriviallyCopyable(internal_manager, &reflection_type);
	}

	ECS_ASSERT(false);
	return false;
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::NeedsUpdate(Stream<char> component, const ReflectionManager* reflection_manager) const
{
	ReflectionType type;
	if (internal_manager->type_definitions.TryGetValue(component, type)) {
		return !CompareReflectionTypes(internal_manager, reflection_manager, &type, reflection_manager->GetType(component));
	}

	ECS_ASSERT(false);
	return false;
}

// ----------------------------------------------------------------------------------------------

// This works for non components as well
void EditorComponents::RemoveType(Stream<char> name)
{
	// Eliminate it from the loaded_modules reference first
	unsigned int index = 0;
	for (; index < loaded_modules.size; index++) {
		unsigned int subindex = function::FindString(name, loaded_modules[index].types);
		if (subindex != -1) {
			loaded_modules[index].types.RemoveSwapBack(subindex);
			break;
		}
	}
	ECS_ASSERT(index < loaded_modules.size);

	// Also remove it from the internal_manager
	index = internal_manager->type_definitions.Find(name);
	ECS_ASSERT(index != -1);

	// Deallocate it
	Deallocate(allocator, internal_manager->type_definitions.GetValuePtrFromIndex(index)->name.buffer);
	internal_manager->type_definitions.EraseFromIndex(index);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::RemoveTypeFromManager(EntityManager* entity_manager, Component component, bool shared) const
{
	// Lock the small memory manager
	entity_manager->m_small_memory_manager.Lock();
	if (shared) {
		entity_manager->UnregisterSharedComponentCommit(component);
	}
	else {
		entity_manager->UnregisterComponentCommit(component);
	}
	entity_manager->m_small_memory_manager.Unlock();
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::ResolveEvent(EntityManager* entity_manager, const ReflectionManager* reflection_manager, EditorComponentEvent event, Stream<SpinLock> locks)
{
	switch (event.type) {
	case EDITOR_COMPONENT_EVENT_IS_ADDED:
	{
		if (IsComponent(event.name)) {
			AddComponentToManager(entity_manager, event.name);
			return true;
		}
	}
		break;
	case EDITOR_COMPONENT_EVENT_IS_REMOVED:
	{
		RemoveTypeFromManager(entity_manager, { event.new_id }, event.is_shared);
		return true;
	}
		break;
	case EDITOR_COMPONENT_EVENT_ALLOCATOR_SIZE_CHANGED:
	{
		// Only if the component exists
		if (IsComponent(event.name)) {
			RecoverData(entity_manager, reflection_manager, event.name, locks);
			return true;
		}
	}
		break;
	case EDITOR_COMPONENT_EVENT_DEPENDENCY_CHANGED:
	{
		// Find all components that reference those types and update them as well
		// For multithreading purposes, assume that not many types depend on the same type
		// to warrant a further split
		internal_manager->type_definitions.ForEachConst([&](const ReflectionType& type, ResourceIdentifier identifier) {
			if (DependsUpon(internal_manager, &type, event.name)) {
				RecoverData(entity_manager, reflection_manager, type.name, locks);
			}
		});
		return true;
	}
		break;
	case EDITOR_COMPONENT_EVENT_DIFFERENT_COMPONENT_DIFFERENT_ID:
	{
		// Recover the data firstly and then update the component references
		if (IsComponent(event.name)) {
			RecoverData(entity_manager, reflection_manager, event.name, locks);
			ChangeComponentID(entity_manager, event.name, event.new_id);
			return true;
		}
	}
		break;
	case EDITOR_COMPONENT_EVENT_HAS_CHANGED:
		RecoverData(entity_manager, reflection_manager, event.name, locks);
		return true;
		break;
	case EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID:
		if (IsComponent(event.name)) {
			ChangeComponentID(entity_manager, event.name, event.new_id);
			return true;
		}
		break;
	}

	return false;
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::SetManagerComponents(EntityManager* entity_manager)
{
	struct FunctorData {
		EntityManager* entity_manager;
	};

	FunctorData functor_data = { entity_manager };
	auto functor = [](const ReflectionType* type, void* _data) {
		FunctorData* data = (FunctorData*)_data;
		Component component_id = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
		if (!data->entity_manager->ExistsComponent(component_id)) {
			double allocator_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
			data->entity_manager->RegisterComponentCommit(component_id, GetReflectionTypeByteSize(type), allocator_size == DBL_MAX ? 0 : (size_t)allocator_size);
		}
	};

	struct SharedFunctorData {
		EntityManager* entity_manager;
	};

	SharedFunctorData shared_functor_data = { entity_manager };
	auto shared_functor = [](const ReflectionType* type, void* _data) {
		SharedFunctorData* data = (SharedFunctorData*)_data;
		Component component_id = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
		if (!data->entity_manager->ExistsSharedComponent(component_id)) {
			double allocator_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
			data->entity_manager->RegisterSharedComponentCommit(component_id, GetReflectionTypeByteSize(type), allocator_size == DBL_MAX ? 0 : (size_t)allocator_size);
		}
	};

	ForEachComponent(functor, &functor_data);
	ForEachSharedComponent(shared_functor, &shared_functor_data);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::SetManagerComponentAllocators(EntityManager* entity_manager)
{
	struct FunctorData {
		EntityManager* entity_manager;
	};

	auto functor = [](const Reflection::ReflectionType* type, void* _data) {
		FunctorData* data = (FunctorData*)_data;
		double allocator_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
		Component component_id = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
		if (allocator_size != DBL_MAX) {
			data->entity_manager->ResizeComponentAllocator(component_id, (size_t)allocator_size);
		}
	};
	FunctorData functor_data;
	functor_data.entity_manager = entity_manager;
	ForEachComponent(functor, &functor_data);

	struct SharedFunctorData {
		EntityManager* entity_manager;
	};

	auto shared_functor = [](const Reflection::ReflectionType* type, void* _data) {
		SharedFunctorData* data = (SharedFunctorData*)_data;
		double allocator_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
		Component component_id = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
		if (allocator_size != DBL_MAX) {
			data->entity_manager->ResizeSharedComponentAllocator(component_id, (size_t)allocator_size);
		}
	};
	SharedFunctorData shared_data;
	shared_data.entity_manager = entity_manager;
	ForEachSharedComponent(shared_functor, &shared_data);
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
	loaded_modules.Initialize(allocator, 0);
	events.Initialize(_allocator, 0);

	internal_manager = (ReflectionManager*)Allocate(allocator, sizeof(ReflectionManager));
	internal_manager->type_definitions.Initialize(allocator, 0);
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
	Stream<char> module_name
)
{
	ECS_STACK_CAPACITY_STREAM(Stream<char>, added_types, 512);
	ECS_STACK_CAPACITY_STREAM(Stream<char>, temporary_module_types, 512);

	unsigned int module_index = IsModule(module_name);

	// Only for types that don't already exist
	auto register_types = [&](Stream<unsigned int> indices, auto check_id) {
		for (unsigned int index = 0; index < indices.size; index++) {
			const ReflectionType* type = reflection_manager->GetType(indices[index]);
			if constexpr (check_id) {
				double evaluation = type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
				if (evaluation == DBL_MAX) {
					events.Add({ EDITOR_COMPONENT_EVENT_IS_MISSING_ID, type->name });
					continue;
				}

				Stream<char> existing_type = TypeFromID((unsigned short)evaluation, IsReflectionTypeSharedComponent(type));
				if (existing_type.size > 0) {
					// A conflict - multiple components with the same ID
					events.Add({ EDITOR_COMPONENT_EVENT_SAME_ID, type->name, existing_type });
					continue;
				}
			}
			// Copy the reflection_type into a separate allocation
			size_t copy_size = type->CopySize();
			void* allocation = Allocate(allocator, copy_size);

			uintptr_t ptr = (uintptr_t)allocation;
			ReflectionType copied_type = type->Copy(ptr);
				
			// The name of the copied type is stable
			InsertIntoDynamicTable(internal_manager->type_definitions, allocator, copied_type, copied_type.name);
			added_types.Add(copied_type.name);
		}
	};

	auto register_existing_types = [&](Stream<unsigned int> indices, auto check_id) {
		for (unsigned int index = 0; index < indices.size; index++) {
			const ReflectionType* type = reflection_manager->GetType(indices[index]);
			unsigned int old_type_index = internal_manager->type_definitions.Find(type->name);

			bool is_trivially_copyable = IsTriviallyCopyable(reflection_manager, type);
			if (!is_trivially_copyable) {
				double buffer_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
				if (buffer_size == DBL_MAX) {
					events.Add({ EDITOR_COMPONENT_EVENT_HAS_BUFFERS_BUT_NO_ALLOCATOR, type->name, type->name });
				}
			}

			// It doesn't exist
			if (old_type_index == -1) {
				if constexpr (check_id) {
					double evaluation = type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
					if (evaluation == DBL_MAX) {
						events.Add({ EDITOR_COMPONENT_EVENT_IS_MISSING_ID, type->name });
						continue;
					}

					Stream<char> existing_type = TypeFromID((unsigned short)evaluation, IsReflectionTypeSharedComponent(type));
					if (existing_type.size > 0) {
						// A conflict - multiple components with the same ID
						events.Add({ EDITOR_COMPONENT_EVENT_SAME_ID, type->name, existing_type });
						continue;
					}
				}
				// Copy the reflection_type into a separate allocation
				size_t copy_size = type->CopySize();
				void* allocation = Allocate(allocator, copy_size);

				uintptr_t ptr = (uintptr_t)allocation;
				ReflectionType copied_type = type->Copy(ptr);

				// The name of the copied type is stable
				InsertIntoDynamicTable(internal_manager->type_definitions, allocator, copied_type, copied_type.name);
				added_types.Add(copied_type.name);
			}
			else {
				const ReflectionType* old_type = internal_manager->type_definitions.GetValuePtrFromIndex(old_type_index);
				// Remove it from the temporary list of module types
				unsigned int temporary_list_index = function::FindString(type->name, temporary_module_types);
				ECS_ASSERT(temporary_list_index != -1);
				temporary_module_types.RemoveSwapBack(temporary_list_index);

				if constexpr (check_id) {
					double new_id = type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
					if (new_id == DBL_MAX) {
						events.Add({ EDITOR_COMPONENT_EVENT_IS_MISSING_ID, type->name, type->name });
						continue;
					}

					double old_id = type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
					ECS_ASSERT(old_id != DBL_MAX, "EditorComponents: An old type is promoted to a component.");

					if (old_id == new_id) {
						// Same ID, check if they changed
						if (!CompareReflectionTypes(internal_manager, reflection_manager, old_type, type)) {
							events.Add({ EDITOR_COMPONENT_EVENT_HAS_CHANGED, type->name, type->name });
							continue;
						}

						// Check if the allocator size has changed
						double allocator_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
						double old_allocator_size = old_type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
						if (allocator_size != old_allocator_size) {
							events.Add({ EDITOR_COMPONENT_EVENT_ALLOCATOR_SIZE_CHANGED, type->name, type->name });
							continue;
						}
					}
					else {
						// Same component but it changed its ID and possibly itself
						Stream<char> existing_type_id = TypeFromID((unsigned short)new_id, IsReflectionTypeSharedComponent(type));
						if (existing_type_id.size > 0) {
							// There is already a type with that ID
							// Generate an event and prevent further processing
							events.Add({ EDITOR_COMPONENT_EVENT_SAME_ID, type->name, existing_type_id });
						}
						else {
							if (!CompareReflectionTypes(internal_manager, reflection_manager, old_type, type)) {
								events.Add({ EDITOR_COMPONENT_EVENT_DIFFERENT_COMPONENT_DIFFERENT_ID, type->name, type->name, (short)new_id });
							}
							else {
								events.Add({ EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID, type->name, type->name, (short)new_id });
							}
						}
						continue;
					}
				}
				else {
					// Check to see if it changed
					if (!CompareReflectionTypes(internal_manager, reflection_manager, old_type, type)) {
						// Not a component but it changed.
						events.Add({ EDITOR_COMPONENT_EVENT_DEPENDENCY_CHANGED, type->name, type->name });
						continue;
					}
				}
			}
		}
	};

	ECS_STACK_CAPACITY_STREAM(unsigned int, component_indices, 512);
	ECS_STACK_CAPACITY_STREAM(unsigned int, hierarchy_types, ECS_KB * 2);

	ReflectionManagerGetQuery query;
	query.indices = &hierarchy_types;
	reflection_manager->GetHierarchyTypes(hierarchy_index, query);
	hierarchy_types.AssertCapacity();

	// Walk through the list of the types and separate the components (unique and shared) from the rest of the types
	for (int32_t index = 0; index < (int32_t)hierarchy_types.size; index++) {
		const ReflectionType* type = reflection_manager->GetType(hierarchy_types[index]);
		if (IsReflectionTypeComponent(type) || IsReflectionTypeSharedComponent(type)) {
			component_indices.AddSafe(hierarchy_types[index]);
			hierarchy_types.RemoveSwapBack(index);
			index--;
		}
	}

	// Check to see if the module has any components already stored
	if (module_index != -1) {
		// Copy the module names into the temporary list
		temporary_module_types.Copy(loaded_modules[module_index].types);

		register_existing_types(component_indices, std::true_type{});
		register_existing_types(hierarchy_types, std::false_type{});

		for (unsigned int index = 0; index < temporary_module_types.size; index++) {
			const ReflectionType* type = internal_manager->type_definitions.GetValuePtr(temporary_module_types[index]);
			if (IsReflectionTypeComponent(type) || IsReflectionTypeSharedComponent(type)) {
				// Emit a removed event
				events.Add({ EDITOR_COMPONENT_EVENT_IS_REMOVED, type->name, type->name, {(short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION)}, IsReflectionTypeSharedComponent(type) });
			}
			else {
				// Not a component, deallocate it now
				RemoveType(type->name);
				unsigned int idx = function::FindString(type->name, loaded_modules[module_index].types);
				loaded_modules[module_index].types.RemoveSwapBack(idx);
			}
		}

		if (added_types.size > 0) {
			size_t new_size = added_types.size + loaded_modules[module_index].types.size;
			void* new_allocation = Allocate(allocator, loaded_modules[module_index].types.MemoryOf(new_size));
			loaded_modules[module_index].types.CopyTo(new_allocation);
			loaded_modules[module_index].types.buffer = (Stream<char>*)new_allocation;
			loaded_modules[module_index].types.AddStream(added_types);
		}
	}
	else {
		register_types(component_indices, std::true_type{});
		register_types(hierarchy_types, std::false_type{});

		module_index = loaded_modules.Add({ function::StringCopy(allocator, module_name) });
		loaded_modules[module_index].types.InitializeAndCopy(allocator, added_types);
	}
}

// ----------------------------------------------------------------------------------------------

Stream<char> EditorComponents::TypeFromID(unsigned short id, bool shared)
{
	Stream<char> name = { nullptr, 0 };
	internal_manager->type_definitions.ForEachConst<true>([&](const ReflectionType& type, ResourceIdentifier identifier) {
		double evaluation = type.GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
		if (evaluation != DBL_MAX && (unsigned short)evaluation == id) {
			if (shared) {
				if (IsReflectionTypeSharedComponent(&type)) {
					name = type.name;
					return true;
				}
			}
			else {
				if (IsReflectionTypeComponent(&type)) {
					name = type.name;
					return true;
				}
			}
		}
		return false;
	});

	return name;
}

// ----------------------------------------------------------------------------------------------

bool IsEditorComponentHandledInternally(EDITOR_COMPONENT_EVENT event_type)
{
	switch (event_type)
	{
	case EDITOR_COMPONENT_EVENT_IS_REMOVED:
	case EDITOR_COMPONENT_EVENT_HAS_CHANGED:
	case EDITOR_COMPONENT_EVENT_DEPENDENCY_CHANGED:
	case EDITOR_COMPONENT_EVENT_DIFFERENT_COMPONENT_DIFFERENT_ID:
	case EDITOR_COMPONENT_EVENT_ALLOCATOR_SIZE_CHANGED:
	case EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID:
		return true;
	case EDITOR_COMPONENT_EVENT_IS_MISSING_ID:
	case EDITOR_COMPONENT_EVENT_HAS_BUFFERS_BUT_NO_ALLOCATOR:
	case EDITOR_COMPONENT_EVENT_SAME_ID:
		return false;
	default:
		ECS_ASSERT(false);
	}
}

// ----------------------------------------------------------------------------------------------

// For fine grained locking use the locks on the memory managers for the archetype base

struct ExecuteComponentEventData {
	EditorState* editor_state;
	// In case some events cannot be processed (like in the case that a component has not been registered yet)
	// put these back to be reprocessed later on
	AtomicStream<EditorComponentEvent>* unhandled_events;
	EditorComponentEvent event_to_handle;
	Stream<Stream<SpinLock>> scene_spin_locks;
	Stream<Stream<SpinLock>> runtime_spin_locks;

	// This semaphore is used to deallocate the data when all threads have finished
	// The allocation is here at the semaphore
	Semaphore* semaphore;
};

ECS_THREAD_TASK(ExecuteComponentEvent) {
	ExecuteComponentEventData* data = (ExecuteComponentEventData*)_data;

	for (unsigned int index = 0; index < data->editor_state->sandboxes.size; index++) {
		if (GetSandboxState(data->editor_state, index) == EDITOR_SANDBOX_PAUSED) {
			// Update the runtime entity manager as well
			data->editor_state->editor_components.ResolveEvent(
				data->editor_state->sandboxes[index].sandbox_world.entity_manager,
				data->editor_state->module_reflection->reflection,
				data->event_to_handle, 
				data->runtime_spin_locks[index]
			);
		}

		// Now handle the scene manager
		bool was_handled = data->editor_state->editor_components.ResolveEvent(
			&data->editor_state->sandboxes[index].scene_entities,
			data->editor_state->module_reflection->reflection,
			data->event_to_handle,
			data->scene_spin_locks[index]
		);
		if (!was_handled) {
			data->unhandled_events->Add(data->event_to_handle);
		}
	}

	unsigned int previous_count = data->semaphore->Exit();
	if (previous_count == 1) {
		// Add all the unhandled events back to the main buffer
		data->editor_state->editor_components.events.AddStream(data->unhandled_events->ToStream());

		// We are the final ones
		data->editor_state->multithreaded_editor_allocator->Deallocate_ts(data->semaphore);
	}
	EditorStateClearFlag(data->editor_state, EDITOR_STATE_PREVENT_LAUNCH);
}

struct UserEventsWindowData {
	EditorState* editor_state;
	ResizableStream<EditorComponentEvent> user_events;
};

void UserEventsWindowDestroy(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	UserEventsWindowData* data = (UserEventsWindowData*)_additional_data;
	data->user_events.FreeBuffer();

	// Go through all modules and retry to add them in order to add the components that are missing
	for (unsigned int index = 0; index < data->editor_state->project_modules->size; index++) {
		const EditorModule* module = data->editor_state->project_modules->buffer + index;
		ECS_STACK_CAPACITY_STREAM(char, library_name, 512);
		function::ConvertWideCharsToASCII(module->library_name, library_name);

		unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(data->editor_state, index);
		data->editor_state->editor_components.UpdateComponents(data->editor_state->module_reflection->reflection, hierarchy_index, library_name);
	}

	ReleaseLockedWindow(action_data);
}

void UserEventsWindow(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	UIDrawConfig config;

	UserEventsWindowData* data = (UserEventsWindowData*)window_data;
	
	EditorStateRemoveOutdatedEvents(data->editor_state, data->user_events);

	drawer.Text("There are events that need to be handled. Instructions:");
	drawer.NextRow();
	Stream<char> label_lists[] = {
		"SAME_ID: Modify the source file for the conflicting components such that they have different IDs.",
		"MISSING_ID: Give the component an ID.",
		"HAS_BUFFERS_NO_ALLOCATOR: Give the component an allocator size.",
	};
	drawer.LabelList(UI_CONFIG_LABEL_LIST_NO_NAME, config, "List", { label_lists, std::size(label_lists) });
	drawer.NextRow();
	drawer.CrossLine();

	drawer.NextRow();
	drawer.Text("Events:");
	drawer.OffsetNextRow(drawer.layout.node_indentation);
	drawer.NextRow();
	for (int32_t index = 0; index < (int32_t)data->user_events.size; index++) {
		EDITOR_COMPONENT_EVENT component_event = data->user_events[index].type;
		const char* type_string = "";

		// Verify if it has been satisfied
		if (component_event == EDITOR_COMPONENT_EVENT_SAME_ID) {
			type_string = " SAME_ID with ";
		}
		else if (component_event == EDITOR_COMPONENT_EVENT_IS_MISSING_ID) {
			type_string = " MISSING_ID";
		}
		else if (component_event == EDITOR_COMPONENT_EVENT_HAS_BUFFERS_BUT_NO_ALLOCATOR) {
			type_string = " HAS_BUFFERS_NO_ALLOCATOR";
		}

		drawer.Text(data->user_events[index].name);	
		drawer.Text(type_string);

		if (component_event == EDITOR_COMPONENT_EVENT_SAME_ID) {
			drawer.Text(data->user_events[index].conflicting_name);
		}
		drawer.NextRow();
	}

	if (data->user_events.size == 0) {
		// Destroy the current window by pushing a frame handler
		drawer.system->PushDestroyWindowHandler(drawer.window_index);
	}
}

// ----------------------------------------------------------------------------------------------

void TickEditorComponents(EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(EditorComponentEvent, user_events, 512);
	EditorStateResolveComponentEvents(editor_state, user_events);

	if (user_events.size > 0) {
		const char* WINDOW_NAME = "User events";
		unsigned int window_index = editor_state->ui_system->GetWindowFromName(WINDOW_NAME);
		if (window_index == -1) {
			UserEventsWindowData window_data;
			window_data.editor_state = editor_state;
			window_data.user_events.InitializeAndCopy(editor_state->EditorAllocator(), user_events);

			UIWindowDescriptor descriptor;

			descriptor.draw = UserEventsWindow;
			descriptor.window_data = &window_data;
			descriptor.window_data_size = sizeof(window_data);
			descriptor.window_name = WINDOW_NAME;

			descriptor.initial_position_x = 0.0f;
			descriptor.initial_position_y = 0.0f;
			descriptor.initial_size_x = 0.5f;
			descriptor.initial_size_y = 0.5f;

			descriptor.destroy_action = UserEventsWindowDestroy;

			editor_state->ui_system->CreateWindowAndDockspace(descriptor, UI_POP_UP_WINDOW_ALL ^ UI_POP_UP_WINDOW_FIT_TO_CONTENT | UI_DOCKSPACE_BORDER_FLAG_NO_CLOSE_X);
		}
		else {
			// Append to the list of the window
			UserEventsWindowData* window_data = (UserEventsWindowData*)editor_state->ui_system->GetWindowData(window_index);
			window_data->user_events.AddStream(user_events);
		}
	}
}

// ----------------------------------------------------------------------------------------------

void EditorStateResolveComponentEvents(EditorState* editor_state, CapacityStream<EditorComponentEvent>& user_events)
{
	if (editor_state->editor_components.events.size > 0) {
		EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);

		editor_state->editor_components.GetUserEvents(user_events);

		// For each sandbox we must create its appropriate spin locks
		size_t total_allocation_size = sizeof(Semaphore) + sizeof(Stream<SpinLock>) * editor_state->sandboxes.size * 2 + 
			sizeof(EditorComponentEvent) * editor_state->editor_components.events.size * editor_state->sandboxes.size + sizeof(AtomicStream<EditorComponentEvent>);
		for (unsigned int index = 0; index < editor_state->sandboxes.size; index++) {
			if (GetSandboxState(editor_state, index) == EDITOR_SANDBOX_PAUSED) {
				total_allocation_size += sizeof(SpinLock) * editor_state->sandboxes[index].sandbox_world.entity_manager->GetArchetypeCount();
			}
			total_allocation_size += sizeof(SpinLock) * editor_state->sandboxes[index].scene_entities.GetArchetypeCount();
		}

		void* allocation = editor_state->multithreaded_editor_allocator->Allocate_ts(total_allocation_size);
		Semaphore* semaphore = (Semaphore*)allocation;
		uintptr_t ptr = (uintptr_t)allocation;
		ptr += sizeof(*semaphore);

		AtomicStream<EditorComponentEvent>* unhandled_events = (AtomicStream<EditorComponentEvent>*)ptr;
		ptr += sizeof(AtomicStream<EditorComponentEvent>);
		unhandled_events->InitializeFromBuffer(ptr, 0, editor_state->editor_components.events.size * editor_state->sandboxes.size);

		Stream<Stream<SpinLock>> runtime_locks;
		runtime_locks.InitializeFromBuffer(ptr, editor_state->sandboxes.size);

		Stream<Stream<SpinLock>> scene_locks;
		scene_locks.InitializeFromBuffer(ptr, editor_state->sandboxes.size);

		for (unsigned int index = 0; index < editor_state->sandboxes.size; index++) {
			if (GetSandboxState(editor_state, index) == EDITOR_SANDBOX_PAUSED) {
				unsigned int archetype_count = editor_state->sandboxes[index].sandbox_world.entity_manager->GetArchetypeCount();
				runtime_locks[index].InitializeFromBuffer(ptr, archetype_count);
				memset(runtime_locks[index].buffer, 0, sizeof(SpinLock) * archetype_count);
			}
			else {
				runtime_locks[index] = { nullptr, 0 };
			}

			unsigned int archetype_count = editor_state->sandboxes[index].scene_entities.GetArchetypeCount();
			scene_locks[index].InitializeFromBuffer(ptr, archetype_count);
			memset(scene_locks[index].buffer, 0, sizeof(SpinLock) * archetype_count);
		}

		semaphore->Enter(editor_state->editor_components.events.size);

		for (unsigned int index = 0; index < editor_state->editor_components.events.size; index++) {
			EditorComponentEvent event = editor_state->editor_components.events[index];

			// Increase the prevent launch flag
			EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);

			ExecuteComponentEventData execute_data;
			execute_data.editor_state = editor_state;
			execute_data.event_to_handle = event;
			execute_data.semaphore = semaphore;
			execute_data.runtime_spin_locks = runtime_locks;
			execute_data.scene_spin_locks = scene_locks;
			execute_data.unhandled_events = unhandled_events;
			EditorStateAddBackgroundTask(editor_state, ECS_THREAD_TASK_NAME(ExecuteComponentEvent, &execute_data, sizeof(execute_data)));
		}

		editor_state->editor_components.EmptyEventStream();

		EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);
	}
}

// ----------------------------------------------------------------------------------------------

void EditorStateRemoveOutdatedEvents(EditorState* editor_state, ResizableStream<EditorComponentEvent>& user_events)
{
	for (unsigned int index = 0; index < user_events.size; index++) {
		EditorComponentEvent component_event = user_events[index];
		// Verify if it has been satisfied
		if (component_event.type == EDITOR_COMPONENT_EVENT_SAME_ID) {
			ReflectionType first_type;
			first_type.name.size = 0;
			ReflectionType second_type;
			second_type.name.size = 0;

			// If one of the types has been removed, then remove the event
			// Else if their component ids have changed
			if (!editor_state->module_reflection->reflection->TryGetType(component_event.name, first_type)) {
				editor_state->ui_reflection->reflection->TryGetType(component_event.name, first_type);
			}

			if (!editor_state->module_reflection->reflection->TryGetType(component_event.conflicting_name, second_type)) {
				editor_state->ui_reflection->reflection->TryGetType(component_event.conflicting_name, second_type);
			}

			if (first_type.name.size == 0 || second_type.name.size == 0) {
				user_events.Remove(index);
				index--;
				continue;
			}

			double first_id = first_type.GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
			double second_id = second_type.GetEvaluation(ECS_COMPONENT_ID_FUNCTION);

			if (first_id == DBL_MAX || second_id == DBL_MAX) {
				// If any of the these is not found, remove the event
				user_events.Remove(index);
				index--;
				continue;
			}

			// Their ids have changed
			if (first_id != second_id) {
				user_events.Remove(index);
				index--;
				continue;
			}
		}
		else if (component_event.type == EDITOR_COMPONENT_EVENT_IS_MISSING_ID) {
			ReflectionType type;
			if (editor_state->module_reflection->reflection->TryGetType(component_event.name, type)) {
				double evaluation = type.GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
				if (evaluation != DBL_MAX) {
					user_events.Remove(index);
					index--;
					continue;
				}
			}
			else {
				// Remove the event if the type has disapperead from the module reflection
				user_events.Remove(index);
				index--;
				continue;
			}
		}
		else if (component_event.type == EDITOR_COMPONENT_EVENT_HAS_BUFFERS_BUT_NO_ALLOCATOR) {
			ReflectionType type;
			if (editor_state->module_reflection->reflection->TryGetType(component_event.name, type)) {
				double evaluation = type.GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
				if (evaluation != DBL_MAX) {
					user_events.Remove(index);
					index--;
					continue;
				}
			}
			else {
				// Remove the event if the type has disapperead from the module reflection
				user_events.Remove(index);
				index--;
				continue;
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------
