#include "editorpch.h"
#include "ECSEngineReflection.h"
#include "EditorComponents.h"
#include "EditorState.h"

#include "../Modules/Module.h"
#include "ECSEngineSerialization.h"
#include "ECSEngineSerializationHelpers.h"
#include "ECSEngineComponents.h"

using namespace ECSEngine;
using namespace ECSEngine::Reflection;

#define HASH_TABLE_DEFAULT_CAPACITY 64

#define ARENA_CAPACITY 50'000
#define ARENA_COUNT 2
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

Component EditorComponents::GetComponentIDWithLink(Stream<char> name) const
{
	Stream<char> target = GetComponentFromLink(name);
	if (target.size > 0) {
		return GetComponentID(target);
	}
	return GetComponentID(name);
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

Stream<char> EditorComponents::GetComponentFromLink(Stream<char> name) const
{
	for (size_t index = 0; index < link_components.size; index++) {
		if (function::CompareStrings(link_components[index].name, name)) {
			return link_components[index].target;
		}
	}
	return { nullptr, 0 };
}

// ----------------------------------------------------------------------------------------------

const ReflectionType* EditorComponents::GetType(Stream<char> name) const
{
	ReflectionType* type = nullptr;
	internal_manager->type_definitions.TryGetValuePtr(name, type);
	return type;
}

// ----------------------------------------------------------------------------------------------

const ReflectionType* EditorComponents::GetType(unsigned int module_index, unsigned int type_index) const
{
	return GetType(loaded_modules[module_index].types[type_index]);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetUniqueLinkComponents(CapacityStream<const ReflectionType*>& link_types) const
{
	ECSEngine::GetUniqueLinkComponents(internal_manager, link_types);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetSharedLinkComponents(CapacityStream<const ReflectionType*>& link_types) const
{
	ECSEngine::GetSharedLinkComponents(internal_manager, link_types);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetLinkComponents(CapacityStream<const ReflectionType*>& unique_types, CapacityStream<const ReflectionType*>& shared_types) const
{
	ECSEngine::GetUniqueAndSharedLinkComponents(internal_manager, unique_types, shared_types);
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

bool EditorComponents::IsLinkComponent(Stream<char> name) const
{
	ReflectionType type;
	if (internal_manager->TryGetType(name, type)) {
		if (IsReflectionTypeLinkComponent(&type)) {
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::IsLinkComponentTarget(ECSEngine::Stream<char> name) const
{
	return GetLinkComponentForTarget(name).size > 0;
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::ModuleComponentCount(Stream<char> name) const
{
	unsigned int module_index = FindModule(name);
	ECS_ASSERT(module_index != -1);
	return ModuleComponentCount(module_index);
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::ModuleComponentCount(unsigned int module_index) const
{
	unsigned int count = 0;
	for (size_t index = 0; index < loaded_modules[module_index].types.size; index++) {
		const ReflectionType* type = internal_manager->type_definitions.GetValuePtr(loaded_modules[module_index].types[index]);
		count += IsReflectionTypeComponent(type);
	}
	return count;
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::ModuleSharedComponentCount(ECSEngine::Stream<char> name) const
{
	unsigned int module_index = FindModule(name);
	ECS_ASSERT(module_index != -1);
	return ModuleSharedComponentCount(module_index);
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::ModuleSharedComponentCount(unsigned int module_index) const
{
	unsigned int count = 0;
	for (size_t index = 0; index < loaded_modules[module_index].types.size; index++) {
		const ReflectionType* type = internal_manager->type_definitions.GetValuePtr(loaded_modules[module_index].types[index]);
		count += IsReflectionTypeSharedComponent(type);
	}
	return count;
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::ModuleIndexFromReflection(const EditorState* editor_state, unsigned int module_index) const
{
	ECS_STACK_CAPACITY_STREAM(char, library_name_storage, 512);
	function::ConvertWideCharsToASCII(editor_state->project_modules->buffer[module_index].library_name, library_name_storage);

	Stream<char> library_name = library_name_storage;
	return function::FindString(library_name, loaded_modules.ToStream(), [](const LoadedModule& loaded_module) {
		return loaded_module.name;
	});
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::ReflectionModuleIndex(const EditorState* editor_state, unsigned int loaded_module_index) const
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, library_name, 512);
	function::ConvertASCIIToWide(library_name, loaded_modules[loaded_module_index].name);
	return GetModuleIndexFromName(editor_state, library_name);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetModuleComponentIndices(unsigned int module_index, CapacityStream<unsigned int>* module_indices, bool replace_links) const
{
	for (size_t index = 0; index < loaded_modules[module_index].types.size; index++) {
		const ReflectionType* type = internal_manager->type_definitions.GetValuePtr(loaded_modules[module_index].types[index]);
		if (IsReflectionTypeComponent(type)) {
			unsigned int add_index = index;

			if (replace_links) {
				Stream<char> link_name = GetLinkComponentForTarget(type->name);
				if (link_name.size > 0) {
					// It has a link
					size_t subindex = 0;
					for (; subindex < loaded_modules[module_index].types.size; subindex++) {
						if (function::CompareStrings(link_name, loaded_modules[module_index].types[subindex])) {
							add_index = subindex;
							break;
						}
					}
					ECS_ASSERT(subindex < loaded_modules[module_index].types.size);
				}
			}
			module_indices->Add(add_index);
		}
	}
	module_indices->AssertCapacity();
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetModuleSharedComponentIndices(unsigned int module_index, CapacityStream<unsigned int>* module_indices, bool replace_links) const
{
	for (size_t index = 0; index < loaded_modules[module_index].types.size; index++) {
		const ReflectionType* type = internal_manager->type_definitions.GetValuePtr(loaded_modules[module_index].types[index]);
		if (IsReflectionTypeSharedComponent(type)) {
			unsigned int add_index = index;
			if (replace_links) {
				Stream<char> link_name = GetLinkComponentForTarget(type->name);
				if (link_name.size > 0) {
					// It has a link
					size_t subindex = 0;
					for (; subindex < loaded_modules[module_index].types.size; subindex++) {
						if (function::CompareStrings(link_name, loaded_modules[module_index].types[subindex])) {
							add_index = subindex;
							break;
						}
					}
					ECS_ASSERT(subindex < loaded_modules[module_index].types.size);
				}
			}
			module_indices->Add(add_index);
		}
	}
	module_indices->AssertCapacity();
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::RecoverData(EntityManager* entity_manager, const ReflectionManager* reflection_manager, Stream<char> component_name, Stream<SpinLock> locks)
{
	const ReflectionType* current_type = reflection_manager->GetType(component_name);
	ReflectionType* old_type = internal_manager->GetType(component_name);

	Component component = { (short)old_type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };

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
	if (has_locks) {
		ECS_ASSERT(locks.size == entity_manager->GetArchetypeCount() + 1);
	}

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

			if (has_locks) {
				// Lock the memory manager allocator
				locks[locks.size - 1].lock();
			}

			MemoryArena* arena = entity_manager->GetComponentAllocator(component);
			if (new_allocator_size == old_allocator_size) {
				arena->Clear();
			}
			else {
				// Resize the arena
				arena = entity_manager->ResizeComponentAllocator(component, new_allocator_size);
			}

			if (has_locks) {
				locks[locks.size - 1].unlock();
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
						locks[locks.size - 1].lock();
					}

					// Need to resize the component allocator
					arena = entity_manager->ResizeComponentAllocator(component, new_allocator_size);

					if (has_locks) {
						locks[locks.size - 1].unlock();
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
			entity_manager->m_unique_components[component].size = new_size;

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

						// We need to lock because resize does an allocation inside
						if (has_locks) {
							locks[locks.size - 1].lock();
						}
						// The size 0 it tells the base to just resize.
						// Now everything will be in its place
						base->Resize(previous_capacity);
						if (has_locks) {
							locks[locks.size - 1].unlock();
						}

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
						locks[locks.size - 1].lock();
					}

					// Need to resize the component allocator
					arena = entity_manager->ResizeComponentAllocator(component, new_allocator_size);

					if (has_locks) {
						locks[locks.size - 1].unlock();
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
				entity_manager->m_shared_components[component].instances.stream.ForEachConst([&](void* data) {
					Serialize(internal_manager, old_type, data, ptr, &serialize_options);
				});

				// Deallocate the arena or resize it
				MemoryArena* arena = nullptr;
				if (new_allocator_size != old_allocator_size) {
					if (has_locks) {
						locks[locks.size - 1].lock();
					}
					arena = entity_manager->ResizeSharedComponentAllocator(component, new_allocator_size);
					if (has_locks) {
						locks[locks.size - 1].unlock();
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
				entity_manager->m_shared_components[component].instances.stream.ForEachConst([&](void* data) {
					ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, current_type, data, ptr, &deserialize_options);
					ECS_ASSERT(code == ECS_DESERIALIZE_OK);
				});
			}
			else {
				MemoryArena* arena = nullptr;
				if (new_allocator_size != old_allocator_size) {
					if (has_locks) {
						locks[locks.size - 1].lock();
					}
					arena = entity_manager->ResizeSharedComponentAllocator(component, new_allocator_size);
					if (has_locks) {
						locks[locks.size - 1].unlock();
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

				entity_manager->m_shared_components[component].instances.stream.ForEachConst([&](void* data) {
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
			entity_manager->m_shared_components[component].instances.stream.ForEachConst([&](void* data) {
				Serialize(internal_manager, old_type, data, ptr, &serialize_options);
			});

			if (has_locks) {
				locks[locks.size - 1].lock();
			}
			entity_manager->ResizeSharedComponent(component, new_size);
			if (has_locks) {
				locks[locks.size - 1].unlock();
			}

			entity_manager->m_shared_components[component].instances.stream.ForEachConst([&](void* data) {
				ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, current_type, data, ptr, &deserialize_options);
				ECS_ASSERT(code == ECS_DESERIALIZE_OK);
			});
		}
	}

	free(temporary_allocation);
	stack_allocator.ClearBackup();
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::AddComponentToManager(EntityManager* entity_manager, Stream<char> component_name, SpinLock* lock)
{
	const ReflectionType* internal_type = internal_manager->GetType(component_name);
	bool is_component = IsReflectionTypeComponent(internal_type);
	
	ECS_STACK_CAPACITY_STREAM(ComponentBuffer, component_buffers, ECS_COMPONENT_INFO_MAX_BUFFER_COUNT);

	Component component = { (short)internal_type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
	double allocator_size_d = internal_type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
	size_t allocator_size = allocator_size_d == DBL_MAX ? 0 : (size_t)allocator_size_d;
	if (allocator_size > 0) {
		GetReflectionTypeRuntimeBuffers(internal_type, component_buffers);
	}

	// Lock the small memory manager in order to commit the type
	if (lock != nullptr) {
		lock->lock();
	}
	// Check to see if it already exists. If it does, don't commit it
	if (is_component) {
		if (!entity_manager->ExistsComponent(component)) {
			entity_manager->RegisterComponentCommit(component, GetReflectionTypeByteSize(internal_type), allocator_size, component_buffers);
		}
	}
	else {
		if (!entity_manager->ExistsSharedComponent(component)) {
			entity_manager->RegisterSharedComponentCommit(component, GetReflectionTypeByteSize(internal_type), allocator_size, component_buffers);
		}
	}
	if (lock != nullptr) {
		lock->unlock();
	}
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ChangeComponentID(EntityManager* entity_manager, Stream<char> component_name, short new_id, SpinLock* lock)
{
	ReflectionType* type = internal_manager->type_definitions.GetValuePtr(component_name);
	unsigned short byte_size = (unsigned short)GetReflectionTypeByteSize(type);

	bool is_component = IsReflectionTypeComponent(type);

	double allocator_size_float = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
	size_t allocator_size = 0;

	short old_id = (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
	ECS_ASSERT(old_id != DBL_MAX);

	Component old_component = { old_id };
	Component new_component = { new_id };

	MemoryArena* arena = nullptr;
	if (lock != nullptr) {
		lock->lock();
	}
	if (is_component) {
		entity_manager->ChangeComponentIndex(old_component, new_component);
	}
	else {
		entity_manager->ChangeSharedComponentIndex(old_component, new_component);
	}
	if (lock != nullptr) {
		lock->unlock();
	}
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::EmptyEventStream()
{
	events.FreeBuffer();
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::FinalizeEvent(const ReflectionManager* reflection_manager, UIReflectionDrawer* ui_drawer, EditorComponentEvent event)
{
	// At the moment only the calls that need to update the component

	switch (event.type) {
	case EDITOR_COMPONENT_EVENT_IS_ADDED:
	{
		// Update the UIDrawer
		const ReflectionType* type = reflection_manager->GetType(event.name);
		UIReflectionType* ui_type = ui_drawer->CreateType(type);
		// Change all buffers to resizable
		// And disable their writes
		ui_drawer->ConvertTypeStreamsToResizable(ui_type);
		ui_drawer->DisableInputWrites(ui_type);
	}
	break;
	case EDITOR_COMPONENT_EVENT_IS_REMOVED:
	{
		// Update the UIDrawer, the destroy call will destroy all instances of that type
		// If it still exists. If the module is released all at once, then this type might not exist
		if (ui_drawer->GetTypePtr(event.name) != nullptr) {
			ui_drawer->DestroyType(event.name);
		}
	}
	break;
	case EDITOR_COMPONENT_EVENT_ALLOCATOR_SIZE_CHANGED:
	{
		UpdateComponent(reflection_manager, event.name);
	}
	break;
	case EDITOR_COMPONENT_EVENT_DIFFERENT_COMPONENT_DIFFERENT_ID:
	{
		event.type = EDITOR_COMPONENT_EVENT_HAS_CHANGED;
		FinalizeEvent(reflection_manager, ui_drawer, event);
	}
	break;
	case EDITOR_COMPONENT_EVENT_HAS_CHANGED:
	{
		UpdateComponent(reflection_manager, event.name);

		// Update the UIDrawer as well
		// Destroy the component to invalidate all current instances and then recreate it
		event.type = EDITOR_COMPONENT_EVENT_IS_REMOVED;
		FinalizeEvent(reflection_manager, ui_drawer, event);
		
		event.type = EDITOR_COMPONENT_EVENT_IS_ADDED;
		FinalizeEvent(reflection_manager, ui_drawer, event);
	}
	break;
	case EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID:
	{
		UpdateComponent(reflection_manager, event.name);
	}
	break;
	}
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::FindModule(Stream<char> name) const
{
	for (unsigned int index = 0; index < loaded_modules.size; index++) {
		if (function::CompareStrings(loaded_modules[index].name, name)) {
			return index;
		}
	}
	return -1;
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::FindComponentModule(Stream<char> name) const
{
	for (unsigned int index = 0; index < loaded_modules.size; index++) {
		unsigned int subindex = function::FindString(name, loaded_modules[index].types);
		if (subindex != -1) {
			return index;
		}
	}
	return -1;
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::FindComponentModuleInReflection(const EditorState* editor_state, Stream<char> name) const
{
	unsigned int in_loaded_modules_index = FindComponentModule(name);
	if (in_loaded_modules_index == -1) {
		return -1;
	}
	return ReflectionModuleIndex(editor_state, in_loaded_modules_index);
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
		return !SearchIsBlittable(internal_manager, &reflection_type);
	}

	ECS_ASSERT(false);
	return false;
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::HasLinkComponentDLLFunction(const EditorState* editor_state, Stream<char> component_name) const
{
	const ReflectionType* type = GetType(component_name);
	ECS_ASSERT(type != nullptr);

	bool needs_dll = Reflection::GetReflectionTypeLinkComponentNeedsDLL(type);
	if (!needs_dll) {
		return true;
	}

	unsigned int reflection_module_index = FindComponentModuleInReflection(editor_state, component_name);
	if (reflection_module_index == -1) {
		return false;
	}

	EDITOR_MODULE_CONFIGURATION configuration = GetModuleLoadedConfiguration(editor_state, reflection_module_index);
	return configuration != EDITOR_MODULE_CONFIGURATION_COUNT;
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::HasLinkComponentDLLFunction(const EditorState* editor_state, const EntityManager* entity_manager, bool2 select_unique_shared) const
{
	bool has_failed = false;

	auto component_functor = [&](Component component) {
		Stream<char> component_name = TypeFromID(component.value, false);
		Stream<char> link_component = GetLinkComponentForTarget(component_name);
		if (link_component.size > 0) {
			// Test it
			has_failed |= !HasLinkComponentDLLFunction(editor_state, link_component);
			return has_failed;
		}
		// Continue the iteration
		return false;
	};

	if (select_unique_shared.x) {
		entity_manager->ForEachComponent<true>(component_functor);

		if (has_failed) {
			return false;
		}
	}

	if (select_unique_shared.y) {
		entity_manager->ForEachSharedComponent<true>(component_functor);

		if (has_failed) {
			return false;
		}
	}
	return true;
}

// ----------------------------------------------------------------------------------------------

Stream<char> EditorComponents::GetLinkComponentForTarget(Stream<char> name) const
{
	for (size_t index = 0; index < link_components.size; index++) {
		if (function::CompareStrings(link_components[index].target, name)) {
			return link_components[index].name;
		}
	}
	return { nullptr, 0 };
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::GetLinkComponentDLLStatus(Stream<char> name) const
{
	ReflectionType type;
	if (internal_manager->TryGetType(name, type)) {
		return GetReflectionTypeLinkComponentNeedsDLL(&type);
	}
	return false;
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetTypeDependencies(Stream<char> type, CapacityStream<Stream<char>>* dependencies) const
{
	GetReflectionTypeDependentTypes(internal_manager, GetType(type), *dependencies);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetModuleTypesDependencies(unsigned int loaded_module_index, CapacityStream<unsigned int>* module_indices, const EditorState* editor_state) const
{
	ECS_STACK_CAPACITY_STREAM(Stream<char>, type_dependencies, 128);

	size_t type_count = loaded_modules[loaded_module_index].types.size;
	for (size_t index = 0; index < type_count; index++) {
		type_dependencies.size = 0;
		GetTypeDependencies(loaded_modules[loaded_module_index].types[index], &type_dependencies);

		for (unsigned int subindex = 0; subindex < type_dependencies.size; subindex++) {
			// Determine the module of each type dependency
			unsigned int module_index = FindComponentModule(type_dependencies[subindex]);
			if (editor_state != nullptr) {
				// The 0'th module is the ECS one - this one should be pushed
				if (module_index != 0) {
					unsigned int reflection_index = ReflectionModuleIndex(editor_state, module_index);
					ECS_ASSERT(reflection_index != -1);
					bool exists = function::SearchBytes(module_indices->buffer, module_indices->size, reflection_index, sizeof(reflection_index)) != -1;
					if (!exists) {
						module_indices->AddSafe(reflection_index);
					}
				}
			}
			else {
				bool exists = function::SearchBytes(module_indices->buffer, module_indices->size, module_index, sizeof(module_index)) != -1;
				if (!exists) {
					module_indices->AddSafe(module_index);
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetModulesTypesDependentUpon(unsigned int loaded_module_index, ECSEngine::CapacityStream<unsigned int>* module_indices, const EditorState* editor_state) const
{
	ECS_STACK_CAPACITY_STREAM(unsigned int, dependent_indices, 512);

	for (unsigned int index = 0; index < loaded_modules.size; index++) {
		if (index != loaded_module_index) {
			dependent_indices.size = 0;
			GetModuleTypesDependencies(index, &dependent_indices);
			bool exists = function::SearchBytes(dependent_indices.buffer, dependent_indices.size, loaded_module_index, sizeof(loaded_module_index)) != -1;
			if (exists) {
				unsigned int index_to_add = index;
				if (editor_state != nullptr) {
					index_to_add = ReflectionModuleIndex(editor_state, index);
				}
				module_indices->AddSafe(index_to_add);
			}
		}
	}
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

void EditorComponents::PushUserEvent(EditorComponentEvent event)
{
	ECS_ASSERT(!IsEditorComponentHandledInternally(event.type));
	events.Add(event);
}

// ----------------------------------------------------------------------------------------------

// This works for non components as well
void EditorComponents::RemoveType(Stream<char> name)
{
	bool is_link_component = IsLinkComponent(name);
	if (is_link_component) {
		// Remove it from the link streams (module and global)
		unsigned int global_index = function::FindString(name, link_components.ToStream(), [](LinkComponent link) {
			return link.name;
		});
		ECS_ASSERT(global_index != -1);
		// Nothing needs to be deallocated, they just reference the names inside the internal ReflectionType
		link_components.RemoveSwapBack(global_index);

		unsigned int module_index = 0;
		for (; module_index < loaded_modules.size; module_index++) {
			unsigned int idx = function::FindString(name, loaded_modules[module_index].link_components);
			if (idx != -1) {
				loaded_modules[module_index].link_components.RemoveSwapBack(idx);
				break;
			}
		}
		ECS_ASSERT(module_index != loaded_modules.size);
	}

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

void EditorComponents::RemoveTypeFromManager(EntityManager* entity_manager, Component component, bool shared, SpinLock* lock) const
{
	ECS_STACK_CAPACITY_STREAM(unsigned int, archetype_indices, ECS_MAIN_ARCHETYPE_MAX_COUNT);

	// Lock the small memory manager
	if (lock != nullptr) {
		lock->lock();
	}

	ComponentSignature signature = { &component, 1 };
	ArchetypeQuery query;
	if (shared) {
		// Get all archetypes with that component and remove it from them
		query.shared.ConvertComponents(signature);
		entity_manager->GetArchetypes(query, archetype_indices);

		for (unsigned int index = 0; index < archetype_indices.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(archetype_indices[index]);
			unsigned int base_count = archetype->GetBaseCount();
			for (unsigned int base_index = 0; base_index < base_count; base_index++) {
				const ArchetypeBase* base = archetype->GetBase(base_index);
				unsigned int entity_count = base->EntityCount();
				const Entity* entities = base->m_entities;
				entity_manager->RemoveSharedComponentCommit({ entities, entity_count }, signature);
			}
		}

		// Destroy all the matched archetypes now
		for (unsigned int index = 0; index < archetype_indices.size; index++) {
			entity_manager->DestroyArchetypeCommit(archetype_indices[index]);
		}

		entity_manager->UnregisterSharedComponentCommit(component);
	}
	else {
		// Get all archetypes with that component and remove it from them
		query.unique.ConvertComponents(signature);
		entity_manager->GetArchetypes(query, archetype_indices);

		for (unsigned int index = 0; index < archetype_indices.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(archetype_indices[index]);
			unsigned int base_count = archetype->GetBaseCount();
			for (unsigned int base_index = 0; base_index < base_count; base_index++) {
				const ArchetypeBase* base = archetype->GetBase(base_index);
				unsigned int entity_count = base->EntityCount();
				const Entity* entities = base->m_entities;
				entity_manager->RemoveComponentCommit({ entities, entity_count }, signature);
			}
		}

		// Destroy all matched archetypes now
		for (unsigned int index = 0; index < archetype_indices.size; index++) {
			entity_manager->DestroyArchetypeCommit(archetype_indices[index]);
		}

		entity_manager->UnregisterComponentCommit(component);
	}
	if (lock != nullptr) {
		lock->unlock();
	}
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::RemoveModuleFromManager(EntityManager* entity_manager, unsigned int loaded_module_index) const
{
	// Go through the types and see which is a component or shared component
	for (size_t index = 0; index < loaded_modules[loaded_module_index].types.size; index++) {
		const ReflectionType* type = GetType(loaded_modules[loaded_module_index].types[index]);
		if (IsReflectionTypeComponent(type)) {
			// Unique component
			Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
			RemoveTypeFromManager(entity_manager, component, false);
		}
		else if (IsReflectionTypeSharedComponent(type)) {
			// Shared component
			Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
			RemoveTypeFromManager(entity_manager, component, true);
		}
	}
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ResetComponent(Stream<char> component_name, void* component_data) const
{
	const ReflectionType* type = internal_manager->GetType(component_name);
	internal_manager->SetInstanceDefaultData(type, component_data);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ResetComponent(ECSEngine::Component component, void* component_data, bool shared) const
{
	Stream<char> name = TypeFromID(component, shared);
	ECS_ASSERT(name.size != 0);
	ResetComponent(name, component_data);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ResetComponents(ComponentSignature component_signature, void* stack_memory, void** component_buffers) const
{
	for (unsigned int index = 0; index < component_signature.count; index++) {
		Stream<char> component_name = TypeFromID(component_signature[index], false);

		// Determine the byte size
		unsigned short byte_size = GetComponentByteSize(component_name);
		component_buffers[index] = stack_memory;
		stack_memory = function::OffsetPointer(stack_memory, byte_size);
		ResetComponent(component_name, component_buffers[index]);
	}
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ResetComponentFromManager(EntityManager* entity_manager, Stream<char> component_name, Entity entity) const
{
	Stream<char> target = GetComponentFromLink(component_name);
	if (target.size > 0) {
		component_name = target;
	}

	Component component = GetComponentID(component_name);
	// If can't find it, fail
	if (component.value == -1) {
		ECS_FORMAT_TEMP_STRING(warn_message, "Failed to reset the component {#} for entity {#}.", component_name, entity.value);
		EditorSetConsoleWarn(warn_message);
		return;
	}

	// If it is not registered, fail
	if (!entity_manager->ExistsComponent(component)) {
		ECS_FORMAT_TEMP_STRING(warn_message, "Failed to reset the component {#} for entity {#}.", component_name, entity.value);
		EditorSetConsoleWarn(warn_message);
		return;
	}

	if (!entity_manager->ExistsEntity(entity)) {
		ECS_FORMAT_TEMP_STRING(warn_message, "Failed to reset the component {#} for entity {#}.", component_name, entity.value);
		EditorSetConsoleWarn(warn_message);
		return;
	}

	MemoryArena* arena = entity_manager->GetComponentAllocator(component);
	void* entity_data = entity_manager->GetComponent(entity, component);

	const ComponentInfo* info = &entity_manager->m_unique_components[component.value];

	// Deallocate the buffers, if any
	for (unsigned char deallocate_index = 0; deallocate_index < info->component_buffers_count; deallocate_index++) {
		ComponentBufferDeallocate(info->component_buffers[deallocate_index], arena, entity_data);
	}

	internal_manager->SetInstanceDefaultData(component_name, entity_data);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::RemoveModule(unsigned int loaded_module_index)
{
	
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::ResolveEvent(EntityManager* entity_manager, const ReflectionManager* reflection_manager, EditorComponentEvent event, Stream<SpinLock> locks)
{
	switch (event.type) {
	case EDITOR_COMPONENT_EVENT_IS_ADDED:
	{
		if (IsComponent(event.name)) {
			AddComponentToManager(entity_manager, event.name, locks.buffer + locks.size - 1);
			return true;
		}
		if (IsLinkComponent(event.name)) {
			// Return true, the purpose of the IS_ADDED for link components is to register it for the UI
			// and inside its structures
			return true;
		}
	}
		break;
	case EDITOR_COMPONENT_EVENT_IS_REMOVED:
	{
		if (!event.is_link_component) {
			RemoveTypeFromManager(entity_manager, { event.new_id }, event.is_shared, locks.buffer + locks.size - 1);
		}
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
			ChangeComponentID(entity_manager, event.name, event.new_id, locks.buffer + locks.size - 1);
			return true;
		}
	}
		break;
	case EDITOR_COMPONENT_EVENT_HAS_CHANGED:
	{
		RecoverData(entity_manager, reflection_manager, event.name, locks);
		return true;
	}
		break;
	case EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID:
		if (IsComponent(event.name)) {
			ChangeComponentID(entity_manager, event.name, event.new_id, locks.buffer + locks.size - 1);
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
			double _allocator_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
			size_t allocator_size = _allocator_size == DBL_MAX ? 0 : (size_t)_allocator_size;

			ECS_STACK_CAPACITY_STREAM(ComponentBuffer, component_buffers, ECS_COMPONENT_INFO_MAX_BUFFER_COUNT);
			if (allocator_size > 0) {
				GetReflectionTypeRuntimeBuffers(type, component_buffers);
			}
 
			data->entity_manager->RegisterComponentCommit(component_id, GetReflectionTypeByteSize(type), allocator_size, component_buffers);
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
			double _allocator_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
			size_t allocator_size = _allocator_size == DBL_MAX ? 0 : (size_t)_allocator_size;

			ECS_STACK_CAPACITY_STREAM(ComponentBuffer, component_buffers, ECS_COMPONENT_INFO_MAX_BUFFER_COUNT);
			if (allocator_size > 0) {
				GetReflectionTypeRuntimeBuffers(type, component_buffers);
			}
			data->entity_manager->RegisterSharedComponentCommit(component_id, GetReflectionTypeByteSize(type), allocator_size, component_buffers);
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

void EditorComponents::UpdateComponent(const ReflectionManager* reflection_manager, Stream<char> component_name)
{
	unsigned int internal_index = internal_manager->type_definitions.Find(component_name);
	ReflectionType* old_type = internal_manager->type_definitions.GetValuePtrFromIndex(internal_index);
	const ReflectionType* current_type = reflection_manager->GetType(component_name);

	// Locate the name inside the module type list and update the reference to the new name
	unsigned int module_index = 0;
	unsigned int type_list_index = 0;
	for (; module_index < loaded_modules.size; module_index++) {
		type_list_index = function::FindString(component_name, loaded_modules[module_index].types);
		if (type_list_index != -1) {
			break;
		}
	}
	ECS_ASSERT(module_index < loaded_modules.size);

	// Now we need to update this type to be the new one
	DeallocateTs(allocator.allocator, allocator.allocator_type, old_type->name.buffer);
	size_t copy_size = current_type->CopySize();
	void* allocation = AllocateTs(allocator.allocator, allocator.allocator_type, copy_size);
	uintptr_t ptr = (uintptr_t)allocation;
	*old_type = current_type->CopyTo(ptr);

	// Update the reference to the name - also the one from the internal reflection manager needs to be updated
	loaded_modules[module_index].types[type_list_index] = old_type->name;
	*internal_manager->type_definitions.GetIdentifierPtrFromIndex(internal_index) = old_type->name;
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
	link_components.Initialize(allocator, 0);

	internal_manager = (ReflectionManager*)Allocate(allocator, sizeof(ReflectionManager));
	memset(internal_manager, 0, sizeof(*internal_manager));
	internal_manager->folders.allocator = allocator;
	internal_manager->type_definitions.Initialize(allocator, 0);
	internal_manager->blittable_types.Initialize(allocator, 1);
	internal_manager->AddBlittableException(STRING(Stream<void>), sizeof(Stream<void>), alignof(Stream<void>));
	internal_manager->InitializeFieldTable();
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

	unsigned int module_index = FindModule(module_name);

	ECS_STACK_CAPACITY_STREAM(unsigned int, component_indices, 512);
	ECS_STACK_CAPACITY_STREAM(unsigned int, hierarchy_types, ECS_KB * 2);

	ReflectionManagerGetQuery query;
	query.indices = &hierarchy_types;
	reflection_manager->GetHierarchyTypes(query, hierarchy_index);
	hierarchy_types.AssertCapacity();

	// Walk through the list of the types and separate the components (unique, shared and link) from the rest of the types
	for (int32_t index = 0; index < (int32_t)hierarchy_types.size; index++) {
		const ReflectionType* type = reflection_manager->GetType(hierarchy_types[index]);
		if (IsReflectionTypeComponent(type) || IsReflectionTypeSharedComponent(type)) {
			component_indices.AddSafe(hierarchy_types[index]);
			hierarchy_types.RemoveSwapBack(index);
			index--;
		}
	}

	auto add_new_type = [&](const ReflectionType* type) {
		// If it is a link component, generate an add type event which will then be handled
		// (we need to create an UI type for it)
		bool is_link_component = IsReflectionTypeLinkComponent(type);
		Stream<char> link_target = { nullptr, 0 };
		if (is_link_component) {
			// If it doesn't have a target, generate an LINK_MISSING_TARGET event
			link_target = GetReflectionTypeLinkComponentTarget(type);
			if (link_target.size == 0) {
				events.Add({ EDITOR_COMPONENT_EVENT_LINK_MISSING_TARGET, type->name });
				return;
			}

			unsigned int target_index = internal_manager->type_definitions.Find(link_target);
			if (target_index != -1) {
				// The target exists. Check to see that it has a valid component
				const ReflectionType* target_type = internal_manager->type_definitions.GetValuePtrFromIndex(target_index);
				if (IsReflectionTypeComponent(target_type) || IsReflectionTypeSharedComponent(target_type)) {
					// Verify that the types are matched
					if (!ValidateLinkComponent(type, target_type)) {
						// Generate a mismatch event
						events.Add({ EDITOR_COMPONENT_EVENT_LINK_MISMATCH_FOR_DEFAULT, type->name });
					}
					else {
						// Generate an event
						events.Add({ EDITOR_COMPONENT_EVENT_IS_ADDED, type->name });
					}
				}
				else {
					// Generate an invalid event
					events.Add({ EDITOR_COMPONENT_EVENT_LINK_INVALID_TARGET, type->name });
				}
			}
		}

		// Copy the reflection_type into a separate allocation
		size_t copy_size = type->CopySize();
		void* allocation = Allocate(allocator, copy_size);

		uintptr_t ptr = (uintptr_t)allocation;
		ReflectionType copied_type = type->CopyTo(ptr);

		// The name of the copied type is stable
		InsertIntoDynamicTable(internal_manager->type_definitions, allocator, copied_type, copied_type.name);
		added_types.Add(copied_type.name);

		if (is_link_component) {
			// Add the copied_type name to the list of link components for the module
			loaded_modules[module_index].link_components.Add(copied_type.name);

			// Add it to the link stream. This will be revisited later on in order to check that the target exists
			link_target = function::StringCopy(allocator, link_target);
			link_components.Add({ copied_type.name, link_target });
		}
	};

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

				double allocator_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);

				bool has_buffers = !SearchIsBlittable(reflection_manager, type);
				if (allocator_size != DBL_MAX) {
					if (!has_buffers) {
						events.Add({ EDITOR_COMPONENT_EVENT_HAS_ALLOCATOR_BUT_NO_BUFFERS, type->name });
						continue;
					}
				}
				else {
					if (has_buffers) {
						events.Add({ EDITOR_COMPONENT_EVENT_HAS_BUFFERS_BUT_NO_ALLOCATOR, type->name });
						continue;
					}
				}

				events.Add({ EDITOR_COMPONENT_EVENT_IS_ADDED, type->name });
			}

			add_new_type(type);
		}
	};

	auto register_existing_types = [&](Stream<unsigned int> indices, auto check_id) {
		for (unsigned int index = 0; index < indices.size; index++) {
			const ReflectionType* type = reflection_manager->GetType(indices[index]);
			unsigned int old_type_index = internal_manager->type_definitions.Find(type->name);

			bool is_trivially_copyable = SearchIsBlittable(reflection_manager, type);
			double buffer_size = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);

			if (!is_trivially_copyable) {
				if (buffer_size == DBL_MAX) {
					events.Add({ EDITOR_COMPONENT_EVENT_HAS_BUFFERS_BUT_NO_ALLOCATOR, type->name, type->name });
					continue;
				}
			}
			else {
				if (buffer_size != DBL_MAX) {
					events.Add({ EDITOR_COMPONENT_EVENT_HAS_ALLOCATOR_BUT_NO_BUFFERS, type->name });
					continue;
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

					events.Add({ EDITOR_COMPONENT_EVENT_IS_ADDED, type->name });
				}
				add_new_type(type);
			}
			else {
				ReflectionType* old_type = internal_manager->type_definitions.GetValuePtrFromIndex(old_type_index);
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

					double old_id = old_type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
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

				// If it passed all tests, verify if the default values have changed. If they did, then copy them
				for (size_t field_index = 0; field_index < type->fields.size; field_index++) {
					if (type->fields[field_index].info.has_default_value) {
						old_type->fields[field_index].info.has_default_value = true;
						memcpy(&old_type->fields[field_index].info.default_bool, &type->fields[field_index].info.default_bool, old_type->fields[field_index].info.byte_size);
					}
				}
			}
		}
	};

	// Check to see if the module has any components already stored
	if (module_index != -1) {
		// Copy the module names into the temporary list
		temporary_module_types.Copy(loaded_modules[module_index].types);

		register_existing_types(component_indices, std::true_type{});
		register_existing_types(hierarchy_types, std::false_type{});

		for (unsigned int index = 0; index < temporary_module_types.size; index++) {
			const ReflectionType* type = internal_manager->type_definitions.GetValuePtr(temporary_module_types[index]);
			bool is_link_component = IsReflectionTypeLinkComponent(type);
			bool is_shared = IsReflectionTypeSharedComponent(type);
			if (IsReflectionTypeComponent(type) || is_shared || is_link_component) {
				// Emit a removed event
				if (!is_link_component) {
					events.Add({ EDITOR_COMPONENT_EVENT_IS_REMOVED, type->name, type->name, {(short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION)}, is_shared, false });
				}
				else {
					events.Add({ EDITOR_COMPONENT_EVENT_IS_REMOVED, type->name, type->name, -1, false, true });
				}
			}
			// Even if it is a component we can remove it now since the RemoveTypeFromManager doesn't require the internal component to be alive
			RemoveType(type->name);
		}

		if (added_types.size > 0) {
			// The add events are generated inside the registration
			size_t new_size = added_types.size + loaded_modules[module_index].types.size;
			void* new_allocation = Allocate(allocator, loaded_modules[module_index].types.MemoryOf(new_size));
			loaded_modules[module_index].types.CopyTo(new_allocation);
			loaded_modules[module_index].types.buffer = (Stream<char>*)new_allocation;
			loaded_modules[module_index].types.AddStream(added_types);
		}
	}
	else {
		module_index = loaded_modules.Add({ function::StringCopy(allocator, module_name) });
		loaded_modules[module_index].link_components.Initialize(allocator, 0);
		register_types(component_indices, std::true_type{});
		register_types(hierarchy_types, std::false_type{});

		loaded_modules[module_index].types.InitializeAndCopy(allocator, added_types);
	}

	auto invalid_link = [&](unsigned int index) {
		Stream<char> link_name = link_components[index].name;
		// Invalid target, generate an event
		events.Add({ EDITOR_COMPONENT_EVENT_LINK_INVALID_TARGET, link_name });
		// Remove this type from the link stream
		link_components.RemoveSwapBack(index);
		// Also remove it from the module link stream
		unsigned int module_link_index = function::FindString(link_name, loaded_modules[module_index].link_components);
		ECS_ASSERT(module_link_index != -1);
		loaded_modules[module_index].link_components.RemoveSwapBack(module_link_index);
	};

	// Verify the link components for links with undetermined targets
	for (unsigned int index = 0; index < link_components.size; index++) {
		ReflectionType type;
		if (internal_manager->TryGetType(link_components[index].target, type)) {
			bool is_component = IsReflectionTypeComponent(&type) || IsReflectionTypeSharedComponent(&type);
			if (is_component) {
				// The target is valid
				if (type.name.buffer != link_components[index].target.buffer) {
					Deallocate(allocator, link_components[index].target.buffer);
					// Previously not determined, now it is
					link_components[index].target.buffer = type.name.buffer;
				}
			}
			else {
				invalid_link(index);
			}
		}
		else {
			invalid_link(index);
		}
	}
}

// ----------------------------------------------------------------------------------------------

Stream<char> EditorComponents::TypeFromID(short id, bool shared) const
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
	case EDITOR_COMPONENT_EVENT_IS_ADDED:
	case EDITOR_COMPONENT_EVENT_IS_REMOVED:
	case EDITOR_COMPONENT_EVENT_HAS_CHANGED:
	case EDITOR_COMPONENT_EVENT_DEPENDENCY_CHANGED:
	case EDITOR_COMPONENT_EVENT_DIFFERENT_COMPONENT_DIFFERENT_ID:
	case EDITOR_COMPONENT_EVENT_ALLOCATOR_SIZE_CHANGED:
	case EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID:
		return true;
	}

	return false;
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
	SpinLock* finalize_event_lock;
};

ECS_THREAD_TASK(ExecuteComponentEvent) {
	ExecuteComponentEventData* data = (ExecuteComponentEventData*)_data;

	bool was_handled = true;
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
		was_handled &= data->editor_state->editor_components.ResolveEvent(
			&data->editor_state->sandboxes[index].scene_entities,
			data->editor_state->module_reflection->reflection,
			data->event_to_handle,
			data->scene_spin_locks[index]
		);
	}

	if (!was_handled) {
		data->unhandled_events->Add(data->event_to_handle);
	}
	else {
		data->finalize_event_lock->lock();
		data->editor_state->editor_components.FinalizeEvent(data->editor_state->module_reflection->reflection, data->editor_state->module_reflection, data->event_to_handle);
		data->finalize_event_lock->unlock();
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
		"HAS_ALLOCATOR_BUT_NO_BUFFERS: Remove the allocator function.",
		"LINK_COMPONENT_MISSING_TARGET: Give the component its target.",
		"LINK_COMPONENT_INVALID_TARGET: The target of the link is not a component.",
		"LINK_MISMATCH_FOR_DEFAULT: The link component has default conversion but the target cannot be converted to."
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
		else if (component_event == EDITOR_COMPONENT_EVENT_HAS_ALLOCATOR_BUT_NO_BUFFERS) {
			type_string = " HAS_ALLOCATOR_NO_BUFFERS";
		}
		else if (component_event == EDITOR_COMPONENT_EVENT_LINK_MISSING_TARGET) {
			type_string = " LINK_MISSING_TARGET";
		}
		else if (component_event == EDITOR_COMPONENT_EVENT_LINK_INVALID_TARGET) {
			type_string = " LINK_INVALID_TARGET";
		}
		else if (component_event == EDITOR_COMPONENT_EVENT_LINK_MISMATCH_FOR_DEFAULT) {
			type_string = " LINK_MISMATCH_FOR_DEFAULT";
		}

		drawer.Text(data->user_events[index].name);	
		drawer.Indent(-1.0f);
		drawer.Text(type_string);
		drawer.Indent(-1.0f);

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
		size_t total_allocation_size = sizeof(Semaphore) + sizeof(SpinLock) + sizeof(Stream<SpinLock>) * editor_state->sandboxes.size * 2 + 
			sizeof(EditorComponentEvent) * editor_state->editor_components.events.size * editor_state->sandboxes.size + sizeof(AtomicStream<EditorComponentEvent>);
		for (unsigned int index = 0; index < editor_state->sandboxes.size; index++) {
			if (GetSandboxState(editor_state, index) == EDITOR_SANDBOX_PAUSED) {
				total_allocation_size += sizeof(SpinLock) * (editor_state->sandboxes[index].sandbox_world.entity_manager->GetArchetypeCount() + 1);
			}
			total_allocation_size += sizeof(SpinLock) * (editor_state->sandboxes[index].scene_entities.GetArchetypeCount() + 1);
		}

		void* allocation = editor_state->multithreaded_editor_allocator->Allocate_ts(total_allocation_size);
		Semaphore* semaphore = (Semaphore*)allocation;
		uintptr_t ptr = (uintptr_t)allocation;
		ptr += sizeof(*semaphore);

		SpinLock* finalize_event_lock = (SpinLock*)ptr;
		finalize_event_lock->unlock();
		ptr += sizeof(SpinLock);

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
				archetype_count++;
				runtime_locks[index].InitializeFromBuffer(ptr, archetype_count);
				memset(runtime_locks[index].buffer, 0, sizeof(SpinLock) * archetype_count);
			}
			else {
				runtime_locks[index] = { nullptr, 0 };
			}

			unsigned int archetype_count = editor_state->sandboxes[index].scene_entities.GetArchetypeCount();
			archetype_count++;
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
			execute_data.finalize_event_lock = finalize_event_lock;
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
		else if (component_event.type == EDITOR_COMPONENT_EVENT_HAS_BUFFERS_BUT_NO_ALLOCATOR || component_event.type == EDITOR_COMPONENT_EVENT_HAS_ALLOCATOR_BUT_NO_BUFFERS) {
			ReflectionType type;
			if (editor_state->module_reflection->reflection->TryGetType(component_event.name, type)) {
				bool is_trivially_copyable = SearchIsBlittable(editor_state->module_reflection->reflection, &type);

				double evaluation = type.GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
				if (evaluation != DBL_MAX && !is_trivially_copyable) {
					user_events.Remove(index);
					index--;
					continue;
				}
				else if (evaluation == DBL_MAX && is_trivially_copyable) {
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
		else if (component_event.type == EDITOR_COMPONENT_EVENT_LINK_MISSING_TARGET) {
			ReflectionType type;
			if (editor_state->module_reflection->reflection->TryGetType(component_event.name, type)) {
				if (IsReflectionTypeLinkComponent(&type)) {
					Stream<char> target = GetReflectionTypeLinkComponentTarget(&type);
					if (target.size > 0 && editor_state->editor_components.IsComponent(target)) {
						// Add an invalid target event if the target is invalid
						if (!editor_state->editor_components.IsComponent(target)) {
							user_events.Add({ EDITOR_COMPONENT_EVENT_LINK_INVALID_TARGET, component_event.name });
						}
						// If the validation fails, add a mismatch event
						else if (!ValidateLinkComponent(&type, editor_state->editor_components.GetType(target))) {
							user_events.Add({ EDITOR_COMPONENT_EVENT_LINK_MISMATCH_FOR_DEFAULT, component_event.name });
						}
						user_events.Remove(index);
						index--;
						continue;
					}
				}
				else {
					// Changed type or it doesn't exist no more
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
		else if (component_event.type == EDITOR_COMPONENT_EVENT_LINK_INVALID_TARGET) {
			ReflectionType type;
			if (editor_state->module_reflection->reflection->TryGetType(component_event.name, type)) {
				if (IsReflectionTypeLinkComponent(&type)) {
					Stream<char> target = GetReflectionTypeLinkComponentTarget(&type);
					if (target.size > 0 && editor_state->editor_components.IsComponent(target)) {
						// Now it has a target and a valid one
						// If the validation fails, add a mismatch event
						if (!ValidateLinkComponent(&type, editor_state->editor_components.GetType(target))) {
							user_events.Add({ EDITOR_COMPONENT_EVENT_LINK_MISMATCH_FOR_DEFAULT, component_event.name });
						}
						user_events.Remove(index);
						index--;
						continue;
					}
				}
				else {
					// Changed type or it doesn't exist no more
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
		else if (component_event.type == EDITOR_COMPONENT_EVENT_LINK_MISMATCH_FOR_DEFAULT) {
			ReflectionType type;
			if (editor_state->module_reflection->reflection->TryGetType(component_event.name, type)) {
				if (IsReflectionTypeLinkComponent(&type)) {
					Stream<char> target = GetReflectionTypeLinkComponentTarget(&type);
					if (target.size > 0 && editor_state->editor_components.IsComponent(target)) {
						// Now it has a target and a valid one
						// Verify that the type can be converted to
						if (ValidateLinkComponent(&type, editor_state->editor_components.GetType(target))) {
							user_events.Remove(index);
							index--;
							continue;
						}
					}
				}
				else {
					// Changed type or it doesn't exist no more
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
