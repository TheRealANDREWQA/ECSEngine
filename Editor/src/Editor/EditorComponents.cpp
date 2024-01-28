#include "editorpch.h"
#include "ECSEngineReflection.h"
#include "EditorComponents.h"
#include "EditorState.h"

#include "../Modules/Module.h"
#include "ECSEngineSerialization.h"
#include "ECSEngineSerializationHelpers.h"
#include "ECSEngineComponentsAll.h"
#include "../Editor/EditorEvent.h"
#include "../Sandbox/Sandbox.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Sandbox/SandboxModule.h"

using namespace ECSEngine;
using namespace ECSEngine::Reflection;

#define HASH_TABLE_DEFAULT_CAPACITY 64

#define ARENA_CAPACITY 3 * 100'000
#define ARENA_COUNT 3
#define ARENA_BLOCK_COUNT 1024

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetAllComponentNames(AdditionStream<Stream<char>> names, ECS_COMPONENT_TYPE component_type) const
{
	internal_manager->type_definitions.ForEachConst([&](const ReflectionType& reflection_type, ResourceIdentifier identifier) {
		if (IsReflectionTypeComponentType(&reflection_type, component_type)) {
			names.Add(reflection_type.name);
		}
	});
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetAllComponentNamesForModule(
	const EditorState* editor_state,
	AdditionStream<Stream<char>> names,
	unsigned int module_index,
	ECS_COMPONENT_TYPE component_type
) const
{
	unsigned int loaded_module_index = ModuleIndexFromReflection(editor_state, module_index);
	ECS_ASSERT_FORMAT(loaded_module_index != -1, "EditorComponents: Invalid module index - the module {#} has not been reflected.", module_index);

	Stream<Stream<char>> module_types = loaded_modules[loaded_module_index].types;
	for (size_t index = 0; index < module_types.size; index++) {
		ECS_COMPONENT_TYPE current_component_type = GetComponentType(module_types[index]);
		if (current_component_type != ECS_COMPONENT_TYPE_COUNT) {
			if (component_type == ECS_COMPONENT_TYPE_COUNT || component_type == current_component_type) {
				names.Add(module_types[index]);
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------

Component EditorComponents::GetComponentID(Stream<char> name) const
{
	ReflectionType type;
	if (internal_manager->type_definitions.TryGetValue(name, type)) {
		return GetReflectionTypeComponent(&type);
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
		if (link_components[index].name == name) {
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

const ReflectionType* EditorComponents::GetType(Component component, ECS_COMPONENT_TYPE type) const
{
	return GetType(ComponentFromID(component.value, type));
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

void EditorComponents::GetGlobalLinkComponents(ECSEngine::CapacityStream<const ECSEngine::Reflection::ReflectionType*>& link_types) const
{
	ECSEngine::GetGlobalLinkComponents(internal_manager, link_types);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::GetLinkComponents(
	CapacityStream<const ReflectionType*>& unique_types, 
	CapacityStream<const ReflectionType*>& shared_types,
	CapacityStream<const ReflectionType*>& global_types
) const
{
	ECSEngine::GetAllLinkComponents(internal_manager, unique_types, shared_types, global_types);
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

bool EditorComponents::IsGlobalComponent(ECSEngine::Stream<char> name) const
{
	ReflectionType type;
	if (internal_manager->TryGetType(name, type)) {
		if (IsReflectionTypeGlobalComponent(&type)) {
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

unsigned int EditorComponents::ModuleComponentCount(Stream<char> name, ECS_COMPONENT_TYPE component_type) const
{
	unsigned int module_index = FindModule(name);
	ECS_ASSERT(module_index != -1);
	return ModuleComponentCount(module_index, component_type);
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::ModuleComponentCount(unsigned int module_index, ECS_COMPONENT_TYPE component_type) const
{
	unsigned int count = 0;
	for (size_t index = 0; index < loaded_modules[module_index].types.size; index++) {
		const ReflectionType* type = internal_manager->type_definitions.GetValuePtr(loaded_modules[module_index].types[index]);
		count += GetReflectionTypeComponentType(type) == component_type;
	}
	return count;
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::ModuleIndexFromReflection(const EditorState* editor_state, unsigned int module_index) const
{
	ECS_STACK_CAPACITY_STREAM(char, library_name_storage, 512);
	ConvertWideCharsToASCII(editor_state->project_modules->buffer[module_index].library_name, library_name_storage);

	Stream<char> library_name = library_name_storage;
	return FindString(library_name, loaded_modules.ToStream(), [](const LoadedModule& loaded_module) {
		return loaded_module.name;
	});
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::ReflectionModuleIndex(const EditorState* editor_state, unsigned int loaded_module_index) const
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, library_name, 512);
	ConvertASCIIToWide(library_name, loaded_modules[loaded_module_index].name);
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
						if (link_name == loaded_modules[module_index].types[subindex]) {
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
						if (link_name == loaded_modules[module_index].types[subindex]) {
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

void EditorComponents::RecoverData(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	const ReflectionManager* reflection_manager, 
	Stream<char> component_name, 
	ResolveEventOptions* options
)
{
	Stream<SpinLock> archetype_locks = {};
	SpinLock* entity_manager_lock = nullptr;
	if (options != nullptr) {
		archetype_locks = options->archetype_locks;
		entity_manager_lock = options->EntityManagerLock();
	}

	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

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
	AllocatorPolymorphic allocator = &stack_allocator;

	SerializeFieldTable(internal_manager, old_type, ptr);
	ptr = (uintptr_t)stack_allocation;
	DeserializeFieldTable field_table = DeserializeFieldTableFromData(ptr, allocator);
	ECS_ASSERT(field_table.types.size > 0);

	// Used when needing to move buffers of data from components
	const size_t TEMPORARY_ALLOCATION_CAPACITY = ECS_MB * 500;
	void* temporary_allocation = malloc(TEMPORARY_ALLOCATION_CAPACITY);
	//LinearAllocator temporary_allocator(temporary_allocation, TEMPORARY_ALLOCATION_CAPACITY);
	//AllocatorPolymorphic temporary_alloc = GetAllocatorPolymorphic(&temporary_allocator);

	DeserializeOptions deserialize_options;
	deserialize_options.read_type_table = false;
	deserialize_options.verify_dependent_types = false;
	deserialize_options.backup_allocator = allocator;
	deserialize_options.field_allocator = allocator;
	deserialize_options.field_table = &field_table;
	deserialize_options.default_initialize_missing_fields = true;

	Reflection::ReflectionManager deserialized_temporary_manager;
	deserialized_temporary_manager.type_definitions.Initialize(&stack_allocator, HashTableCapacityForElements(field_table.types.size));
	field_table.ToNormalReflection(&deserialized_temporary_manager, &stack_allocator);
	deserialize_options.deserialized_field_manager = &deserialized_temporary_manager;

	SerializeOptions serialize_options;
	serialize_options.write_type_table = false;
	serialize_options.verify_dependent_types = false;

	ECS_STACK_CAPACITY_STREAM(unsigned int, matching_archetypes, ECS_MAIN_ARCHETYPE_MAX_COUNT);
	ComponentSignature component_signature;
	component_signature.indices = &component;
	component_signature.count = 1;

	bool has_buffers = HasBuffers(component_name);
	bool has_locks = archetype_locks.size > 0;
	if (has_locks) {
		ECS_ASSERT(archetype_locks.size == (RecoverDataLocksSize(entity_manager) / sizeof(SpinLock)));
	}

	ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, missing_asset_fields, 512);
	GetLinkComponentTargetMissingAssetFields(old_type, current_type, &missing_asset_fields);

	size_t old_size = GetReflectionTypeByteSize(old_type);
	size_t new_size = GetReflectionTypeByteSize(current_type);

	if (IsReflectionTypeComponent(old_type)) {
		ArchetypeQuery query;
		query.unique.ConvertComponents(component_signature);
		entity_manager->GetArchetypes(query, matching_archetypes);

		if (missing_asset_fields.size > 0) {
			// Remove the asset fields in a separate pass for easier logic handling
			for (unsigned int index = 0; index < matching_archetypes.size; index++) {
				if (has_locks) {
					archetype_locks[matching_archetypes[index]].Lock();
				}

				const Archetype* archetype = entity_manager->GetArchetype(matching_archetypes[index]);
				unsigned char component_index = archetype->FindUniqueComponentIndex(component);
				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					const ArchetypeBase* base_archetype = archetype->GetBase(base_index);
					unsigned int entity_count = base_archetype->EntityCount();
					for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
						const void* entity_data = base_archetype->GetComponentByIndex(entity_index, component_index);
						RemoveSandboxComponentAssets(editor_state, sandbox_index, component, entity_data, ECS_COMPONENT_UNIQUE, missing_asset_fields);
					}
				}

				if (has_locks) {
					archetype_locks[matching_archetypes[index]].Unlock();
				}
			}
		}

		auto has_buffer_prepass = [&]() {
			ptr = (uintptr_t)temporary_allocation;

			// Need to go through all entities and serialize them because we need to deallocate the component allocator
			// and then reallocate all the data

			for (unsigned int index = 0; index < matching_archetypes.size; index++) {
				if (has_locks) {
					// Lock the archetype
					archetype_locks[matching_archetypes[index]].Lock();
				}
				Archetype* archetype = entity_manager->GetArchetype(matching_archetypes[index]);
				unsigned char component_index = archetype->FindUniqueComponentIndex(component);

				unsigned int base_count = archetype->GetBaseCount();
				for (unsigned int base_index = 0; base_index < base_count; base_index++) {
					ArchetypeBase* base = archetype->GetBase(base_index);
					unsigned int entity_count = base->EntityCount();		

					const void* component_buffer = base->GetComponentByIndex(0, component_index);
					for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
						const void* entity_data = OffsetPointer(component_buffer, entity_index * old_size);
						Serialize(internal_manager, old_type, entity_data, ptr, &serialize_options);
					}
				}

				if (has_locks) {
					archetype_locks[matching_archetypes[index]].Unlock();
				}
			}

			ECS_ASSERT(ptr - (uintptr_t)temporary_allocation <= TEMPORARY_ALLOCATION_CAPACITY);

			if (has_locks) {
				// Lock the memory manager allocator
				entity_manager_lock->Lock();
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
				entity_manager_lock->Unlock();
			}

			if (arena != nullptr) {
				AllocatorPolymorphic component_allocator = arena;
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
						archetype_locks[matching_archetypes[index]].Lock();
					}

					Archetype* archetype = entity_manager->GetArchetype(matching_archetypes[index]);
					unsigned char component_index = archetype->FindUniqueComponentIndex(component);

					unsigned int base_count = archetype->GetBaseCount();
					for (unsigned int base_index = 0; base_index < base_count; base_index++) {
						ArchetypeBase* base = archetype->GetBase(base_index);
						unsigned int entity_count = base->EntityCount();

						void* component_buffer = base->GetComponentByIndex(0, component_index);

						for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
							ECS_DESERIALIZE_CODE code = Deserialize(
								reflection_manager,
								current_type,
								OffsetPointer(component_buffer, entity_index * old_size),
								ptr,
								&deserialize_options
							);
							ECS_ASSERT(code == ECS_DESERIALIZE_OK);
						}
					}

					if (has_locks) {
						archetype_locks[matching_archetypes[index]].Unlock();
					}
				}
			}
			else {
				MemoryArena* arena = nullptr;
				if (new_allocator_size != old_allocator_size) {
					if (has_locks) {
						entity_manager_lock->Lock();
					}

					// Need to resize the component allocator
					arena = entity_manager->ResizeComponentAllocator(component, new_allocator_size);

					if (has_locks) {
						entity_manager_lock->Unlock();
					}
				}
				else {
					arena = entity_manager->GetComponentAllocator(component);
				}
				
				if (arena != nullptr) {
					AllocatorPolymorphic alloc = arena;
					deserialize_options.field_allocator = alloc;
					deserialize_options.backup_allocator = alloc;
				}

				for (unsigned int index = 0; index < matching_archetypes.size; index++) {
					if (has_locks) {
						archetype_locks[matching_archetypes[index]].Lock();
					}
					Archetype* archetype = entity_manager->GetArchetype(matching_archetypes[index]);
					unsigned char component_index = archetype->FindUniqueComponentIndex(component);

					unsigned int base_count = archetype->GetBaseCount();
					for (unsigned int base_index = 0; base_index < base_count; base_index++) {
						ArchetypeBase* base = archetype->GetBase(base_index);
						unsigned int entity_count = base->EntityCount();

						const void* component_buffer = base->GetComponentByIndex(0, component_index);
						for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
							ptr = (uintptr_t)temporary_allocation;
							void* entity_data = OffsetPointer(component_buffer, entity_index * old_size);
							// Copy to temporary memory and then deserialize back
							Serialize(internal_manager, old_type, entity_data, ptr, &serialize_options);
							ptr = (uintptr_t)temporary_allocation;
							Deserialize(reflection_manager, current_type, entity_data, ptr, &deserialize_options);
						}
					}

					if (has_locks) {
						archetype_locks[matching_archetypes[index]].Unlock();
					}
				}
			}
		}
		else {
			entity_manager->m_unique_components[component].size = new_size;

			// First functor receives (void* second, const void* source, size_t copy_size) and must return how many bytes it wrote
			// Second functor receives the void* to the new component memory and a void* to the old data
			auto resize_archetype = [&](auto same_component_copy, auto same_component_handler) {
				for (unsigned int index = 0; index < matching_archetypes.size; index++) {
					if (has_locks) {
						archetype_locks[matching_archetypes[index]].Lock();
					}
					Archetype* archetype = entity_manager->GetArchetype(matching_archetypes[index]);
					ComponentSignature current_signature = archetype->GetUniqueSignature();

					ECS_STACK_CAPACITY_STREAM(unsigned short, component_sizes, 256);
					for (unsigned int component_index = 0; component_index < current_signature.count; component_index++) {
						component_sizes[component_index] = entity_manager->ComponentSize(current_signature.indices[component_index]);
					}
					
					// Use this to copy the the entities into a temporary buffer since resizing a base will lose the entities
					ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(temporary_allocator, ECS_KB * 256, ECS_MB);

					unsigned int base_count = archetype->GetBaseCount();
					for (unsigned int base_index = 0; base_index < base_count; base_index++) {
						ArchetypeBase* base = archetype->GetBase(base_index);
						unsigned int entity_count = base->EntityCount();
						void** previous_base_buffers = base->m_buffers;

						temporary_allocator.Clear();

						Entity* entities_copy = (Entity*)temporary_allocator.Allocate(sizeof(Entity) * entity_count);
						memcpy(entities_copy, base->m_entities, sizeof(Entity) * entity_count);

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
						unsigned int previous_size = base->m_size;
						base->m_size = 0;

						// We need to lock because resize does an allocation inside
						if (has_locks) {
							entity_manager_lock->Lock();
						}
						// The size 0 it tells the base to just resize.
						// Now everything will be in its place
						base->Resize(previous_capacity);
						if (has_locks) {
							entity_manager_lock->Unlock();
						}

						base->m_size = previous_size;
						// Copy back the entity values
						memcpy(base->m_entities, entities_copy, sizeof(Entity) * entity_count);

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
										OffsetPointer(new_buffers[component_index], entity_index * new_size),
										OffsetPointer(previous_buffers[component_index], entity_index * old_size)
									);
								}
							}
						}
					}

					if (has_locks) {
						archetype_locks[matching_archetypes[index]].Unlock();
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
						entity_manager_lock->Lock();
					}

					// Need to resize the component allocator
					arena = entity_manager->ResizeComponentAllocator(component, new_allocator_size);

					if (has_locks) {
						entity_manager_lock->Unlock();
					}
				}
				else {
					arena = entity_manager->GetComponentAllocator(component);
				}

				if (arena != nullptr) {
					AllocatorPolymorphic alloc = arena;
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
		// If it has missing asset fields, do a prepass and unregister them
		if (missing_asset_fields.size > 0) {
			entity_manager->ForEachSharedInstance(component, [&](SharedInstance instance) {
				const void* component_data = entity_manager->GetSharedData(component, instance);
				RemoveSandboxComponentAssets(editor_state, sandbox_index, component, component_data, ECS_COMPONENT_SHARED, missing_asset_fields);
			});
		}


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
						entity_manager_lock->Lock();
					}
					arena = entity_manager->ResizeSharedComponentAllocator(component, new_allocator_size);
					if (has_locks) {
						entity_manager_lock->Unlock();
					}
				}
				else {
					arena = entity_manager->GetSharedComponentAllocator(component);
					if (arena != nullptr) {
						arena->Clear();
					}
				}

				if (arena != nullptr) {
					AllocatorPolymorphic alloc = arena;
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
						entity_manager_lock->Lock();
					}
					arena = entity_manager->ResizeSharedComponentAllocator(component, new_allocator_size);
					if (has_locks) {
						entity_manager_lock->Unlock();
					}
				}
				else {
					arena = entity_manager->GetSharedComponentAllocator(component);
				}

				if (arena != nullptr) {
					AllocatorPolymorphic alloc = arena;
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
				entity_manager_lock->Lock();
			}
			entity_manager->ResizeSharedComponent(component, new_size);
			if (has_locks) {
				entity_manager_lock->Unlock();
			}

			ptr = (uintptr_t)temporary_allocation;
			entity_manager->m_shared_components[component].instances.stream.ForEachConst([&](void* data) {
				ECS_DESERIALIZE_CODE code = Deserialize(reflection_manager, current_type, data, ptr, &deserialize_options);
				ECS_ASSERT(code == ECS_DESERIALIZE_OK);
			});
		}
	}

	free(temporary_allocation);
}

// ----------------------------------------------------------------------------------------------

Stream<SpinLock> RecoverLocks(
	const EntityManager* entity_manager,
	AllocatorPolymorphic allocator,
	size_t(*SizeFunction)(const EntityManager*),
	Stream<SpinLock>(*CreateFunction)(const EntityManager*, uintptr_t&)
) {
	size_t allocation_size = SizeFunction(entity_manager);
	void* allocation = Allocate(allocator, allocation_size);
	uintptr_t allocation_ptr = (uintptr_t)allocation;
	return CreateFunction(entity_manager, allocation_ptr);
}

Stream<SpinLock> EditorComponents::RecoverDataLocks(const EntityManager* entity_manager, AllocatorPolymorphic allocator)
{
	return RecoverLocks(entity_manager, allocator, RecoverDataLocksSize, RecoverDataLocks);
}

// ----------------------------------------------------------------------------------------------

Stream<SpinLock> EditorComponents::RecoverComponentLocks(const EntityManager* entity_manager, AllocatorPolymorphic allocator)
{
	return RecoverLocks(entity_manager, allocator, RecoverComponentLocksSize, RecoverComponentLocks);
}

// ----------------------------------------------------------------------------------------------

Stream<SpinLock> EditorComponents::RecoverSharedComponentLocks(const EntityManager* entity_manager, AllocatorPolymorphic allocator)
{
	return RecoverLocks(entity_manager, allocator, RecoverSharedComponentLocksSize, RecoverSharedComponentLocks);
}

// ----------------------------------------------------------------------------------------------

size_t EditorComponents::RecoverDataLocksSize(const EntityManager* entity_manager)
{
	// +1 for the entity manager at the end - the lock is used mostly for changing component ID's
	return sizeof(SpinLock) * (entity_manager->GetArchetypeCount() + 1);
}

// ----------------------------------------------------------------------------------------------

size_t EditorComponents::RecoverComponentLocksSize(const EntityManager* entity_manager)
{
	return sizeof(SpinLock) * (entity_manager->GetMaxComponent().value + 1);
}

// ----------------------------------------------------------------------------------------------

size_t EditorComponents::RecoverSharedComponentLocksSize(const EntityManager* entity_manager)
{
	return sizeof(SpinLock) * (entity_manager->GetMaxSharedComponent().value + 1);
}

// ----------------------------------------------------------------------------------------------

size_t EditorComponents::ResolveEventOptionsSize(const ECSEngine::EntityManager* entity_manager)
{
	return RecoverDataLocksSize(entity_manager) + RecoverComponentLocksSize(entity_manager) + RecoverSharedComponentLocksSize(entity_manager);
}

// ----------------------------------------------------------------------------------------------

Stream<SpinLock> RecoverLocks(
	const EntityManager* entity_manager,
	uintptr_t& ptr,
	size_t (*SizeFunction)(const EntityManager*)
) {
	size_t total_count = SizeFunction(entity_manager) / sizeof(SpinLock);
	Stream<SpinLock> result = Stream<SpinLock>();
	result.InitializeFromBuffer(ptr, total_count);
	// Set the value to 0 such that all are unlocked
	memset(result.buffer, 0, result.MemoryOf(total_count));
	return result;
}

Stream<SpinLock> EditorComponents::RecoverDataLocks(const EntityManager* entity_manager, uintptr_t& ptr)
{
	return RecoverLocks(entity_manager, ptr, RecoverDataLocksSize);
}

// ----------------------------------------------------------------------------------------------

Stream<SpinLock> EditorComponents::RecoverComponentLocks(const EntityManager* entity_manager, uintptr_t& ptr)
{
	return RecoverLocks(entity_manager, ptr, RecoverComponentLocksSize);
}

// ----------------------------------------------------------------------------------------------

Stream<SpinLock> EditorComponents::RecoverSharedComponentLocks(const EntityManager* entity_manager, uintptr_t& ptr)
{
	return RecoverLocks(entity_manager, ptr, RecoverSharedComponentLocksSize);
}

// ----------------------------------------------------------------------------------------------

EditorComponents::ResolveEventOptions EditorComponents::ResolveEventOptionsFromBuffer(const EntityManager* entity_manager, uintptr_t& ptr)
{
	return { RecoverDataLocks(entity_manager, ptr), RecoverComponentLocks(entity_manager, ptr), RecoverSharedComponentLocks(entity_manager, ptr) };
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::AddType(const ReflectionType* type, unsigned int module_index)
{
	// Copy the reflection_type into a separate allocation
	size_t copy_size = type->CopySize();
	void* allocation = Allocate(allocator, copy_size);

	uintptr_t ptr = (uintptr_t)allocation;
	ReflectionType copied_type = type->CopyTo(ptr);

	// The name of the copied type is stable
	Stream<char> allocated_name = StringCopy(allocator, copied_type.name);
	internal_manager->type_definitions.InsertDynamic(allocator, copied_type, allocated_name);

	bool is_link_component = IsReflectionTypeLinkComponent(type);

	if (is_link_component) {
		Stream<char> link_target = GetReflectionTypeLinkComponentTarget(type);

		// Add the copied_type name to the list of link components for the module
		loaded_modules[module_index].link_components.Add(allocated_name);

		// Add it to the link stream. This will be revisited later on in order to check that the target exists
		link_target = StringCopy(allocator, link_target);
		link_components.Add({ allocated_name, link_target });
	}
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::AddComponentToManager(EntityManager* entity_manager, Stream<char> component_name, SpinLock* lock)
{
	const ReflectionType* internal_type = internal_manager->GetType(component_name);
	bool is_component = IsReflectionTypeComponent(internal_type);
	
	Component component = { (short)internal_type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
	ECS_STACK_VOID_STREAM(stack_memory, 256);
	ComponentFunctions component_functions = GetReflectionTypeRuntimeComponentFunctions(internal_type, &stack_memory);

	// Lock the small memory manager in order to commit the type
	if (lock != nullptr) {
		lock->Lock();
	}
	// Check to see if it already exists. If it does, don't commit it
	if (is_component) {
		if (!entity_manager->ExistsComponent(component)) {
			entity_manager->RegisterComponentCommit(component, GetReflectionTypeByteSize(internal_type), component_name, &component_functions);
		}
	}
	else {
		if (!entity_manager->ExistsSharedComponent(component)) {
			entity_manager->RegisterSharedComponentCommit(component, GetReflectionTypeByteSize(internal_type), component_name, &component_functions);
		}
	}
	if (lock != nullptr) {
		lock->Unlock();
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
		lock->Lock();
	}
	if (is_component) {
		entity_manager->ChangeComponentIndex(old_component, new_component);
	}
	else {
		entity_manager->ChangeSharedComponentIndex(old_component, new_component);
	}
	if (lock != nullptr) {
		lock->Unlock();
	}
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::EmptyEventStream()
{
	events.FreeBuffer();
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::FillAllComponents(CapacityStream<Component>* components, ECS_COMPONENT_TYPE component_type) const
{
	internal_manager->type_definitions.ForEachConst([&](const ReflectionType& reflection_type, ResourceIdentifier identifier) {
		if (GetReflectionTypeComponentType(&reflection_type) == component_type) {
			components->AddAssert(GetReflectionTypeComponent(&reflection_type));
		}
	});
}

// ----------------------------------------------------------------------------------------------

struct FinalizeEventSingleThreadedData {
	EditorComponentEvent event_;
	const ReflectionManager* reflection_manager;
	
	// Extra information used by some events
	// At the moment only the IS_REMOVED is using this
	EditorComponents::FinalizeEventOptions finalize_options;
	Component old_component_id; // Used by changed ID events
};

// When the finalize event needs to do something on the main thread
// it will push an event for that purpose. For example when creating UI types
EDITOR_EVENT(FinalizeEventSingleThreaded) {
	FinalizeEventSingleThreadedData* data = (FinalizeEventSingleThreadedData*)_data;

	switch (data->event_.type) {
	case EDITOR_COMPONENT_EVENT_IS_ADDED:
	{
		// Update the UIDrawer
		Stream<char> name = data->event_.name;

		UIReflectionDrawer* ui_reflection = editor_state->module_reflection;
		if (data->reflection_manager == editor_state->ui_reflection->reflection) {
			ui_reflection = editor_state->ui_reflection;
		}

		// If the UIReflectionType doesn't already exists, create it
		UIReflectionType* ui_type = ui_reflection->GetType(name);
		if (ui_type == nullptr) {
			ui_type = ui_reflection->CreateType(name);
		}
		
		// Change all buffers to resizable
		// And disable their writes
		ui_reflection->ConvertTypeStreamsToResizable(ui_type);
		ui_reflection->DisableInputWrites(ui_type);
	}
	break;
	case EDITOR_COMPONENT_EVENT_IS_REMOVED:
	{
		UIReflectionDrawer* ui_reflection = editor_state->module_reflection;
		if (data->reflection_manager == editor_state->ui_reflection->reflection) {
			ui_reflection = editor_state->ui_reflection;
		}

		// Update the UIDrawer, the destroy call will destroy all instances of that type
		// If it still exists. If the module is released all at once, then this type might not exist
		if (ui_reflection->GetType(data->event_.name) != nullptr) {
			ui_reflection->DestroyType(data->event_.name);
		}

		// Here we must also remove the component from debug draws
		// We must do this before removing the type from the editor components
		Component component = editor_state->editor_components.GetComponentID(data->event_.name);
		ECS_COMPONENT_TYPE component_type = data->event_.component_type;
		if (component.Valid() && component_type != ECS_COMPONENT_TYPE_COUNT) {
			unsigned int sandbox_count = GetSandboxCount(editor_state);
			for (unsigned int index = 0; index < sandbox_count; index++) {
				EditorSandbox* sandbox = GetSandbox(editor_state, index);
				// This performs the removal only if the component exists
				RemoveSandboxDebugDrawComponent(editor_state, index, component, component_type);
			}
		}

		if (data->finalize_options.should_remove_type) {
			// We can now remove the type
			editor_state->editor_components.RemoveType(data->event_.name);
		}
	}
	break;
	case EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID:
	case EDITOR_COMPONENT_EVENT_DIFFERENT_COMPONENT_DIFFERENT_ID:
	{
		// Perform the change of the component ID for the debug draw
		ECS_COMPONENT_TYPE component_type = editor_state->editor_components.GetComponentType(data->event_.name);
		if (component_type != ECS_COMPONENT_TYPE_COUNT) {
			unsigned int sandbox_count = GetSandboxCount(editor_state);
			for (unsigned int index = 0; index < sandbox_count; index++) {
				EditorSandbox* sandbox = GetSandbox(editor_state, index);
				ChangeSandboxDebugDrawComponent(editor_state, index, data->old_component_id, data->event_.new_id, component_type);
			}
		}
	}
	break;
	default:
		ECS_ASSERT(false);
	}

	return false;
}

void EditorComponents::FinalizeEvent(
	EditorState* editor_state, 
	const ReflectionManager* reflection_manager, 
	EditorComponentEvent event, 
	FinalizeEventOptions options
)
{
	switch (event.type) {
	case EDITOR_COMPONENT_EVENT_IS_ADDED:
	{
		// Push an event - the main thread could be adding another UI type in the meantime
		FinalizeEventSingleThreadedData single_threaded_data;
		single_threaded_data.event_ = event;
		single_threaded_data.reflection_manager = reflection_manager;
		single_threaded_data.finalize_options = options;
		EditorAddEvent(editor_state, FinalizeEventSingleThreaded, &single_threaded_data, sizeof(single_threaded_data));
	}
	break;
	case EDITOR_COMPONENT_EVENT_IS_REMOVED:
	{
		// Push an event - the main thread could be adding another UI type in the meantime
		FinalizeEventSingleThreadedData single_threaded_data;
		single_threaded_data.event_ = event;
		single_threaded_data.reflection_manager = reflection_manager;
		single_threaded_data.finalize_options = options;
		EditorAddEvent(editor_state, FinalizeEventSingleThreaded, &single_threaded_data, sizeof(single_threaded_data));
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
		FinalizeEvent(editor_state, reflection_manager, event);
	}
	break;
	case EDITOR_COMPONENT_EVENT_HAS_CHANGED:
	case EDITOR_COMPONENT_EVENT_DEPENDENCY_CHANGED:
	case EDITOR_COMPONENT_EVENT_PROMOTED_TO_COMPONENT:
	case EDITOR_COMPONENT_EVENT_COMPONENT_DEMOTED:
	{
		UpdateComponent(reflection_manager, event.name);

		// If it is a component, also finalize with remove and add to invalidate the UI types
		if (IsComponent(event.name) || IsLinkComponent(event.name)) {
			// Update the UIDrawer as well
			// Destroy the component to invalidate all current instances and then recreate it
			event.type = EDITOR_COMPONENT_EVENT_IS_REMOVED;
			options.should_remove_type = false;
			FinalizeEvent(editor_state, reflection_manager, event, options);

			event.type = EDITOR_COMPONENT_EVENT_IS_ADDED;
			FinalizeEvent(editor_state, reflection_manager, event, options);
		}
	}
	break;
	case EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID:
	{
		// We also need to add an event to handle the change of component id for the debug draw
		FinalizeEventSingleThreadedData single_threaded_data;
		single_threaded_data.event_ = event;
		single_threaded_data.reflection_manager = reflection_manager;
		single_threaded_data.finalize_options = options;
		single_threaded_data.old_component_id = editor_state->editor_components.GetComponentID(event.name);
		EditorAddEvent(editor_state, FinalizeEventSingleThreaded, &single_threaded_data, sizeof(single_threaded_data));

		UpdateComponent(reflection_manager, event.name);
	}
	break;
	}
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::FindModule(Stream<char> name) const
{
	for (unsigned int index = 0; index < loaded_modules.size; index++) {
		if (loaded_modules[index].name == name) {
			return index;
		}
	}
	return -1;
}

// ----------------------------------------------------------------------------------------------

unsigned int EditorComponents::FindComponentModule(Stream<char> name) const
{
	for (unsigned int index = 0; index < loaded_modules.size; index++) {
		unsigned int subindex = FindString(name, loaded_modules[index].types);
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
		return !IsBlittableWithPointer(&reflection_type);
	}

	ECS_ASSERT(false);
	return false;
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::HasLinkComponentDLLFunction(const EditorState* editor_state, Stream<char> component_name) const
{
	const ReflectionType* type = GetType(component_name);
	ECS_ASSERT(type != nullptr);

	bool needs_dll = GetReflectionTypeLinkComponentNeedsDLL(type);
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

bool EditorComponents::HasLinkComponentDLLFunction(const EditorState* editor_state, const EntityManager* entity_manager, bool3 select_unique_shared_global) const
{
	bool has_failed = false;

	auto component_functor_impl = [&](Component component, ECS_COMPONENT_TYPE component_type) {
		Stream<char> component_name = ComponentFromID(component.value, component_type);
		Stream<char> link_component = GetLinkComponentForTarget(component_name);
		if (link_component.size > 0) {
			// Test it
			has_failed |= !HasLinkComponentDLLFunction(editor_state, link_component);
			return has_failed;
		}
		// Continue the iteration
		return false;
	};

	auto component_functor = [&](Component component) {
		return component_functor_impl(component, ECS_COMPONENT_UNIQUE);
	};

	auto shared_component_functor = [&](Component component) {
		return component_functor_impl(component, ECS_COMPONENT_SHARED);
	};

	auto global_component_functor = [&](const void* global_data, Component component) {
		return component_functor_impl(component, ECS_COMPONENT_GLOBAL);
	};

	if (select_unique_shared_global.x) {
		entity_manager->ForEachComponent<true>(component_functor);

		if (has_failed) {
			return false;
		}
	}

	if (select_unique_shared_global.y) {
		entity_manager->ForEachSharedComponent<true>(shared_component_functor);

		if (has_failed) {
			return false;
		}
	}

	if (select_unique_shared_global.z) {
		entity_manager->ForAllGlobalComponents<true>(global_component_functor);

		if (has_failed) {
			return false;
		}
	}

	return true;
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::HasComponentAssets(Stream<char> component_name) const
{
	const ReflectionType* type = GetType(component_name);
	return HasAssetFieldsTargetComponent(type);
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::HasComponentAssets(Component component, ECS_COMPONENT_TYPE component_type) const
{
	return HasComponentAssets(ComponentFromID(component, component_type));
}

// ----------------------------------------------------------------------------------------------

Stream<char> EditorComponents::GetLinkComponentForTarget(Stream<char> name) const
{
	for (size_t index = 0; index < link_components.size; index++) {
		if (link_components[index].target == name) {
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

ECS_COMPONENT_TYPE EditorComponents::GetComponentType(Stream<char> name) const
{
	ReflectionType type;
	if (internal_manager->TryGetType(name, type)) {
		return GetReflectionTypeComponentType(&type);
	}
	return ECS_COMPONENT_TYPE_COUNT;
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
					bool exists = SearchBytes(module_indices->ToStream(), reflection_index) != -1;
					if (!exists) {
						module_indices->AddAssert(reflection_index);
					}
				}
			}
			else {
				bool exists = SearchBytes(module_indices->ToStream(), module_index) != -1;
				if (!exists) {
					module_indices->AddAssert(module_index);
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
			bool exists = SearchBytes(dependent_indices.ToStream(), loaded_module_index) != -1;
			if (exists) {
				unsigned int index_to_add = index;
				if (editor_state != nullptr) {
					index_to_add = ReflectionModuleIndex(editor_state, index);
				}
				module_indices->AddAssert(index_to_add);
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
		unsigned int global_index = FindString(name, link_components.ToStream(), [](LinkComponent link) {
			return link.name;
		});
		ECS_ASSERT(global_index != -1);

		// Deallocate the buffers
		link_components[global_index].Deallocate(allocator);
		link_components.RemoveSwapBack(global_index);

		unsigned int module_index = 0;
		for (; module_index < loaded_modules.size; module_index++) {
			unsigned int idx = FindString(name, loaded_modules[module_index].link_components);
			if (idx != -1) {
				// This just referenced the value inside the link_components[global_index]
				loaded_modules[module_index].link_components.RemoveSwapBack(idx);
				break;
			}
		}
		ECS_ASSERT(module_index != loaded_modules.size);
	}

	// Eliminate it from the loaded_modules reference first
	unsigned int index = 0;
	for (; index < loaded_modules.size; index++) {
		unsigned int subindex = FindString(name, loaded_modules[index].types);
		if (subindex != -1) {
			// Deallocate the string stored here
			loaded_modules[index].types[subindex].Deallocate(allocator);
			loaded_modules[index].types.RemoveSwapBack(subindex);
			break;
		}
	}
	ECS_ASSERT(index < loaded_modules.size);

	// Also remove it from the internal_manager
	index = internal_manager->type_definitions.Find(name);
	ECS_ASSERT(index != -1);

	// Deallocate it
	internal_manager->type_definitions.GetValuePtrFromIndex(index)->DeallocateCoalesced(allocator);
	internal_manager->type_definitions.EraseFromIndex(index);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::RemoveLinkType(EditorState* editor_state, Stream<char> name)
{
	if (editor_state->ui_reflection->GetType(name) != nullptr) {
		editor_state->ui_reflection->DestroyType(name);
	}
	if (editor_state->module_reflection->GetType(name) != nullptr) {
		editor_state->ui_reflection->DestroyType(name);
	}

	RemoveType(name);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::RemoveTypeFromManager(
	EditorState* editor_state,
	unsigned int sandbox_index,
	EDITOR_SANDBOX_VIEWPORT viewport,
	Component component, 
	ECS_COMPONENT_TYPE component_type,
	SpinLock* lock
) const
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	ECS_STACK_CAPACITY_STREAM(unsigned int, archetype_indices, ECS_MAIN_ARCHETYPE_MAX_COUNT);

	// Lock the small memory manager
	if (lock != nullptr) {
		lock->Lock();
	}

	bool has_assets = editor_state->editor_components.HasComponentAssets(component, component_type);

	ComponentSignature signature = { &component, 1 };
	ArchetypeQuery query;
	if (component_type == ECS_COMPONENT_SHARED) {
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

		// Destroy all the matched archetypes now - since they will be empty
		entity_manager->DestroyArchetypesCommit(archetype_indices);

		// Check to see if the component has assets that we should remove
		if (has_assets) {
			// Now for each shared component remove the asset handles
			entity_manager->ForEachSharedInstance(component, [&](SharedInstance instance) {
				const void* instance_data = entity_manager->GetSharedData(component, instance);
				RemoveSandboxComponentAssets(editor_state, sandbox_index, component, instance_data, component_type);
			});
		}

		entity_manager->UnregisterSharedComponentCommit(component);
	}
	else if (component_type == ECS_COMPONENT_UNIQUE) {
		// Get all archetypes with that component and remove it from them
		query.unique.ConvertComponents(signature);
		entity_manager->GetArchetypes(query, archetype_indices);

		for (unsigned int index = 0; index < archetype_indices.size; index++) {
			const Archetype* archetype = entity_manager->GetArchetype(archetype_indices[index]);
			unsigned int base_count = archetype->GetBaseCount();
			unsigned char component_index = archetype->FindUniqueComponentIndex(component);

			for (unsigned int base_index = 0; base_index < base_count; base_index++) {
				const ArchetypeBase* base = archetype->GetBase(base_index);
				unsigned int entity_count = base->EntityCount();
				const Entity* entities = base->m_entities;

				// Determine if we need to remove assets
				if (has_assets) {
					for (unsigned int entity_index = 0; entity_index < entity_count; entity_index++) {
						RemoveSandboxComponentAssets(editor_state, sandbox_index, component, base->GetComponentByIndex(entity_index, component_index), component_type);
					}
				}

				entity_manager->RemoveComponentCommit({ entities, entity_count }, signature);
			}
		}

		// Destroy all matched archetypes now - since they will be empty
		entity_manager->DestroyArchetypesCommit(archetype_indices);
		entity_manager->UnregisterComponentCommit(component);
	}
	else if (component_type == ECS_COMPONENT_GLOBAL) {
		// Here we simply just need to remove the global component, nothing else to do
		RemoveSandboxGlobalComponent(editor_state, sandbox_index, component, viewport);
	}
	else {
		ECS_ASSERT(false, "Invalid component type for RemoveTypeFromManager");
	}

	SetSandboxSceneDirty(editor_state, sandbox_index, viewport);
	if (lock != nullptr) {
		lock->Unlock();
	}
}

// ----------------------------------------------------------------------------------------------


void EditorComponents::ResetComponentBasic(Stream<char> component_name, void* component_data) const
{
	const ReflectionType* type = internal_manager->GetType(component_name);
	internal_manager->SetInstanceDefaultData(type, component_data);
}

void EditorComponents::ResetComponentBasic(Component component, void* component_data, ECS_COMPONENT_TYPE type) const
{
	Stream<char> name = ComponentFromID(component, type);
	ECS_ASSERT(name.size != 0);
	ResetComponentBasic(name, component_data);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::RemoveModuleFromManager(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	unsigned int loaded_module_index
) const
{
	// Go through the types and see which is a component or shared component
	for (size_t index = 0; index < loaded_modules[loaded_module_index].types.size; index++) {
		const ReflectionType* type = GetType(loaded_modules[loaded_module_index].types[index]);
		Component component = { (short)type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) };
		if (IsReflectionTypeComponent(type)) {
			// Unique component		
			RemoveTypeFromManager(editor_state, sandbox_index, viewport, component, ECS_COMPONENT_UNIQUE);
		}
		else if (IsReflectionTypeSharedComponent(type)) {
			// Shared component
			RemoveTypeFromManager(editor_state, sandbox_index, viewport, component, ECS_COMPONENT_UNIQUE);
		}
		else if (IsReflectionTypeGlobalComponent(type)) {
			// Global component
			RemoveTypeFromManager(editor_state, sandbox_index, viewport, component, ECS_COMPONENT_GLOBAL);
		}
	}
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ResetGlobalComponent(Component component, void* component_data) const
{
	Stream<char> name = ComponentFromID(component, ECS_COMPONENT_GLOBAL);
	ECS_ASSERT(name.size != 0);

	const ReflectionType* reflection_type = internal_manager->GetType(name);
	internal_manager->SetInstanceDefaultData(reflection_type, component_data);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::ResetComponent(EditorState* editor_state, unsigned int sandbox_index, Stream<char> component_name, Entity entity, ECS_COMPONENT_TYPE type) const
{
	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index);
	if (!entity_manager->ExistsEntity(entity)) {
		ECS_FORMAT_TEMP_STRING(warn_message, "Failed to reset the component {#} for entity {#}.", component_name, entity.value);
		EditorSetConsoleWarn(warn_message);
		return;
	}

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
	bool exists_component = false;
	if (type == ECS_COMPONENT_UNIQUE) {
		exists_component = entity_manager->ExistsComponent(component);
	}
	else {
		exists_component = entity_manager->ExistsSharedComponent(component);
	}
	if (!exists_component) {
		ECS_FORMAT_TEMP_STRING(warn_message, "Failed to reset the component {#} for entity {#}.", component_name, entity.value);
		EditorSetConsoleWarn(warn_message);
		return;
	}
	void* entity_data = nullptr;
	if (type == ECS_COMPONENT_UNIQUE) {
		entity_data = entity_manager->GetComponent(entity, component);
	}
	else {
		entity_data = entity_manager->GetSharedComponent(entity, component);
	}
	
	// Call the deallocate for both cases
	EditorModuleComponentBuildEntry build_entry = GetModuleComponentBuildEntry(editor_state, component_name);
	if (build_entry.entry.function != nullptr) {
		if (type == ECS_COMPONENT_UNIQUE) {
			CallModuleComponentBuildFunctionUnique(editor_state, sandbox_index, &build_entry, { &entity, 1 }, component);
		}
		else {
			CallModuleComponentBuildFunctionShared(editor_state, sandbox_index, &build_entry, component, entity_manager->GetSharedComponentInstance(component, entity), entity);
		}
	}
	else {
		entity_manager->TryCallEntityComponentDeallocateCommit(entity, component);
		internal_manager->SetInstanceDefaultData(component_name, entity_data);
	}
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::RemoveModule(EditorState* editor_state, unsigned int loaded_module_index)
{
	unsigned int sandbox_count = GetSandboxCount(editor_state);
	// Remove the module from the entity managers firstly and then actually deallocate it
	for (unsigned int sandbox_index = 0; sandbox_index < sandbox_count; sandbox_index++) {
		RemoveModuleFromManager(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE, loaded_module_index);
		RemoveModuleFromManager(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME, loaded_module_index);
	}

	LoadedModule* loaded_module = &loaded_modules[loaded_module_index];
	for (size_t type_index = 0; type_index < loaded_module->types.size; type_index++) {
		Stream<char> current_type = loaded_module->types[type_index];

		// Remove the type from the internal reflection
		unsigned int reflection_index = internal_manager->type_definitions.Find(current_type);
		ECS_ASSERT(reflection_index != -1);

		internal_manager->type_definitions.GetValuePtrFromIndex(reflection_index)->DeallocateCoalesced(allocator);
		internal_manager->type_definitions.EraseFromIndex(reflection_index);

		// Locate the name in link components
		unsigned int link_index = FindString(current_type, loaded_module->link_components);
		if (link_index != -1) {
			// Don't need to deallocate - it's the same string as the one in the types stream
			loaded_module->link_components.RemoveSwapBack(link_index);

			link_index = FindString(current_type, link_components.ToStream(), [](LinkComponent link) {
				return link.name;
			});
			ECS_ASSERT(link_index != -1);

			link_components[link_index].Deallocate(allocator);
			link_components.RemoveSwapBack(link_index);
		}

		// Deallocate the type name since it is allocated separately
		loaded_module->types[type_index].Deallocate(allocator);
	}

	// If there are remaining link components there must be some sort of error
	ECS_ASSERT(loaded_module->link_components.size == 0);

	// Can now deallocate the loaded_module
	loaded_module->name.Deallocate(allocator);
	loaded_module->types.Deallocate(allocator);
	loaded_module->link_components.FreeBuffer();

	// Now remove the main stream
	loaded_modules.RemoveSwapBack(loaded_module_index);
}

// ----------------------------------------------------------------------------------------------

bool EditorComponents::ResolveEvent(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	EDITOR_SANDBOX_VIEWPORT viewport, 
	const ReflectionManager* reflection_manager, 
	EditorComponentEvent event, 
	ResolveEventOptions* options
)
{
	SpinLock* entity_manager_lock = nullptr;
	if (options) {
		entity_manager_lock = options->EntityManagerLock();
	}

	EntityManager* entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, viewport);

	// If it is promoted as a component we just need to add it to the component manager
	switch (event.type) {
	case EDITOR_COMPONENT_EVENT_IS_ADDED:
	case EDITOR_COMPONENT_EVENT_PROMOTED_TO_COMPONENT:
	{
		if (event.type == EDITOR_COMPONENT_EVENT_PROMOTED_TO_COMPONENT) {
			// When promoting, we need to update the internal manager first
			// It will be updated again in the finalize but that is not really a problem
			UpdateComponent(reflection_manager, event.name);
		}

		if (IsComponent(event.name)) {
			AddComponentToManager(entity_manager, event.name, entity_manager_lock);
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
	case EDITOR_COMPONENT_EVENT_COMPONENT_DEMOTED:
	{
		RemoveTypeFromManager(editor_state, sandbox_index, viewport, { event.new_id }, event.component_type, entity_manager_lock);
		return true;
	}
		break;
	case EDITOR_COMPONENT_EVENT_ALLOCATOR_SIZE_CHANGED:
	{
		// Only if the component exists
		if (IsComponent(event.name)) {
			RecoverData(editor_state, sandbox_index, viewport, reflection_manager, event.name, options);
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
			if (GetReflectionTypeComponentType(&type) != ECS_COMPONENT_TYPE_COUNT) {
				if (DependsUpon(internal_manager, &type, event.name)) {
					RecoverData(editor_state, sandbox_index, viewport, reflection_manager, type.name, options);
				}
			}
		});
		return true;
	}
		break;
	case EDITOR_COMPONENT_EVENT_DIFFERENT_COMPONENT_DIFFERENT_ID:
	{
		// Recover the data firstly and then update the component references
		if (IsComponent(event.name)) {
			RecoverData(editor_state, sandbox_index, viewport, reflection_manager, event.name, options);
			ChangeComponentID(entity_manager, event.name, event.new_id, entity_manager_lock);
			return true;
		}
	}
		break;
	case EDITOR_COMPONENT_EVENT_HAS_CHANGED:
	{
		RecoverData(editor_state, sandbox_index, viewport, reflection_manager, event.name, options);
		return true;
	}
		break;
	case EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID:
		if (IsComponent(event.name)) {
			ChangeComponentID(entity_manager, event.name, event.new_id, entity_manager_lock);
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
			ECS_STACK_VOID_STREAM(stack_memory, 256);
			ComponentFunctions component_functions = GetReflectionTypeRuntimeComponentFunctions(type, &stack_memory);
			data->entity_manager->RegisterComponentCommit(component_id, GetReflectionTypeByteSize(type), type->name, &component_functions);
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
			ECS_STACK_VOID_STREAM(stack_memory, 256);
			ComponentFunctions component_functions = GetReflectionTypeRuntimeComponentFunctions(type, &stack_memory);
			data->entity_manager->RegisterSharedComponentCommit(component_id, GetReflectionTypeByteSize(type), type->name, &component_functions);
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
	// Make sure that the type exists in a module
	unsigned int module_index = 0;
	unsigned int type_list_index = 0;
	for (; module_index < loaded_modules.size; module_index++) {
		type_list_index = FindString(component_name, loaded_modules[module_index].types);
		if (type_list_index != -1) {
			break;
		}
	}
	ECS_ASSERT(module_index < loaded_modules.size);

	unsigned int internal_index = internal_manager->type_definitions.Find(component_name);
	ReflectionType* old_type = internal_manager->type_definitions.GetValuePtrFromIndex(internal_index);
	const ReflectionType* current_type = reflection_manager->GetType(component_name);

	// Now we need to update this type to be the new one
	old_type->DeallocateCoalesced({ allocator.allocator, allocator.allocator_type, ECS_ALLOCATION_MULTI });
	*old_type = current_type->CopyCoalesced({ allocator.allocator, allocator.allocator_type, ECS_ALLOCATION_MULTI });
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::Initialize(void* buffer)
{
	MemoryArena* arena = (MemoryArena*)buffer;
	CreateBaseAllocatorInfo info;
	info.allocator_type = ECS_ALLOCATOR_ARENA;
	*arena = MemoryArena(OffsetPointer(arena, sizeof(*arena)), ARENA_COUNT, ARENA_CAPACITY, ARENA_BLOCK_COUNT);
	InitializeAllocator(arena);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::InitializeAllocator(AllocatorPolymorphic _allocator)
{
	allocator = _allocator;
	loaded_modules.Initialize(allocator, 0);
	events.Initialize(_allocator, 0);
	link_components.Initialize(allocator, 0);

	internal_manager = (ReflectionManager*)Allocate(allocator, sizeof(ReflectionManager));
	*internal_manager = ReflectionManager(allocator, 0, 0);
}

// ----------------------------------------------------------------------------------------------

size_t EditorComponents::DefaultAllocatorSize()
{
	return sizeof(MemoryArena) + MemoryArena::MemoryOf(ARENA_COUNT, ARENA_CAPACITY, ECS_ALLOCATOR_MULTIPOOL);
}

// ----------------------------------------------------------------------------------------------

void EditorComponents::UpdateComponents(
	EditorState* editor_state,
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
		if (GetReflectionTypeComponentType(type) != ECS_COMPONENT_TYPE_COUNT) {
			component_indices.AddAssert(hierarchy_types[index]);
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
				events.Add({ EDITOR_COMPONENT_EVENT_LINK_MISSING_TARGET, type->name.Copy(allocator) });
				return;
			}

			unsigned int target_index = internal_manager->type_definitions.Find(link_target);
			if (target_index != -1) {
				// The target exists. Check to see that it has a valid component
				const ReflectionType* target_type = internal_manager->type_definitions.GetValuePtrFromIndex(target_index);
				if (GetReflectionTypeComponentType(target_type) != ECS_COMPONENT_TYPE_COUNT) {
					// Verify that the types are matched
					if (!ValidateLinkComponent(type, target_type)) {
						// Generate a mismatch event
						events.Add({ EDITOR_COMPONENT_EVENT_LINK_MISMATCH_FOR_DEFAULT, type->name.Copy(allocator) });
						//return;
					}
					else {
						// Generate an event
						events.Add({ EDITOR_COMPONENT_EVENT_IS_ADDED, type->name.Copy(allocator) });
					}
				}
				else {
					// We don't need to do anything here - we should normally emit an invalid target
					// Event but there is the case that a type gets promoted to a component and its link
					// type is processed before the actual type and here we would wrongly add the event
					// There is a loop at the end that verifies the link components and will correctly handle this case
				}
			}
		}

		AddType(type, module_index);

		// Also add the name to the added types - we need to add the allocated name which is that of the identifier
		unsigned int type_index = internal_manager->type_definitions.Find(type->name);
		ECS_ASSERT(type_index != -1);
		ResourceIdentifier allocated_identifier = internal_manager->type_definitions.GetIdentifierFromIndex(type_index);
		added_types.Add(allocated_identifier.AsASCII());
	};

	// Only for types that don't already exist
	auto register_types = [&](Stream<unsigned int> indices, auto check_id) {
		for (unsigned int index = 0; index < indices.size; index++) {
			const ReflectionType* type = reflection_manager->GetType(indices[index]);
			if constexpr (check_id) {
				ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT validate_result = ValidateReflectionTypeComponent(type);

				if (validate_result == ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ID_FUNCTION) {
					Stream<char> name_copy = type->name.Copy(allocator);
					events.Add({ EDITOR_COMPONENT_EVENT_IS_MISSING_FUNCTION, name_copy, {}, ECS_COMPONENT_ID_FUNCTION });
					continue;
				}

				if (validate_result == ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ALLOCATOR_SIZE_FUNCTION) {
					Stream<char> name_copy = type->name.Copy(allocator);
					events.Add({ EDITOR_COMPONENT_EVENT_IS_MISSING_FUNCTION, name_copy, {}, ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION });
					continue;
				}

				if (validate_result == ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_IS_SHARED_FUNCTION) {
					Stream<char> name_copy = type->name.Copy(allocator);
					events.Add({ EDITOR_COMPONENT_EVENT_IS_MISSING_FUNCTION, name_copy, {}, ECS_COMPONENT_IS_SHARED_FUNCTION });
					continue;
				}

				if (validate_result == ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_NO_BUFFERS_BUT_ALLOCATOR_SIZE_FUNCTION) {
					events.Add({ EDITOR_COMPONENT_EVENT_HAS_ALLOCATOR_BUT_NO_BUFFERS, type->name.Copy(allocator) });
					continue;
				}

				Component type_component = GetReflectionTypeComponent(type);
				ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(type);
				Stream<char> existing_type = ComponentFromID(type_component, component_type);
				if (existing_type.size > 0) {
					// A conflict - multiple components with the same ID
					events.Add({ EDITOR_COMPONENT_EVENT_SAME_ID, type->name.Copy(allocator), existing_type.Copy(allocator) });
					continue;
				}

				events.Add({ EDITOR_COMPONENT_EVENT_IS_ADDED, type->name.Copy(allocator) });
			}

			add_new_type(type);
		}
	};

	auto register_existing_types = [&](Stream<unsigned int> indices, auto check_id) {
		for (unsigned int index = 0; index < indices.size; index++) {
			const ReflectionType* type = reflection_manager->GetType(indices[index]);
			unsigned int old_type_index = internal_manager->type_definitions.Find(type->name);

			ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT validate_result = ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_NOT_A_COMPONENT;
			if constexpr (check_id) {
				validate_result = ValidateReflectionTypeComponent(type);
				if (validate_result == ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ALLOCATOR_SIZE_FUNCTION) {
					events.Add({ EDITOR_COMPONENT_EVENT_IS_MISSING_FUNCTION, type->name.Copy(allocator), {}, ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION });
					continue;
				}
				else if (validate_result == ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_NO_BUFFERS_BUT_ALLOCATOR_SIZE_FUNCTION) {
					events.Add({ EDITOR_COMPONENT_EVENT_HAS_ALLOCATOR_BUT_NO_BUFFERS, type->name.Copy(allocator) });
					continue;
				}
			}

			// It doesn't exist
			if (old_type_index == -1) {
				if constexpr (check_id) {
					if (validate_result == ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ID_FUNCTION) {
						events.Add({ EDITOR_COMPONENT_EVENT_IS_MISSING_FUNCTION, type->name.Copy(allocator), {}, ECS_COMPONENT_ID_FUNCTION });
						continue;
					}

					Component current_component = GetReflectionTypeComponent(type);
					ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(type);
					Stream<char> existing_type = ComponentFromID(current_component, component_type);
					if (existing_type.size > 0) {
						// A conflict - multiple components with the same ID
						events.Add({ EDITOR_COMPONENT_EVENT_SAME_ID, type->name.Copy(allocator), existing_type.Copy(allocator) });
						continue;
					}

					events.Add({ EDITOR_COMPONENT_EVENT_IS_ADDED, type->name.Copy(allocator) });
				}
				add_new_type(type);
			}
			else {
				ReflectionType* old_type = internal_manager->type_definitions.GetValuePtrFromIndex(old_type_index);
				// Remove it from the temporary list of module types
				unsigned int temporary_list_index = FindString(type->name, temporary_module_types);
				ECS_ASSERT(temporary_list_index != -1);
				temporary_module_types.RemoveSwapBack(temporary_list_index);

				ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(type);
				ECS_COMPONENT_TYPE old_component_type = GetReflectionTypeComponentType(old_type);

				if (old_component_type == ECS_COMPONENT_TYPE_COUNT && component_type != ECS_COMPONENT_TYPE_COUNT) {
					// We need to emit a promoted event
					events.Add({ EDITOR_COMPONENT_EVENT_PROMOTED_TO_COMPONENT, type->name.Copy(allocator) });
					// We don't need to continue here - we still need to check the other conditions
				}
				else if (old_component_type != ECS_COMPONENT_TYPE_COUNT && component_type == ECS_COMPONENT_TYPE_COUNT) {
					Component old_component = GetReflectionTypeComponent(old_type);
					events.Add({ EDITOR_COMPONENT_EVENT_COMPONENT_DEMOTED, type->name.Copy(allocator), {}, {}, old_component, old_component_type });
					// We don't need to continue here - we still need to check the other conditions
				}

				if constexpr (check_id) {
					if (validate_result == ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ID_FUNCTION) {
						events.Add({ EDITOR_COMPONENT_EVENT_IS_MISSING_FUNCTION, type->name.Copy(allocator), {}, ECS_COMPONENT_ID_FUNCTION });
						continue;
					}
					
					Component current_component = GetReflectionTypeComponent(type);
					Component old_id = GetReflectionTypeComponent(old_type);
					if (old_id == current_component) {
						// Same ID, check if they changed
						if (!CompareReflectionTypes(internal_manager, reflection_manager, old_type, type)) {
							events.Add({ EDITOR_COMPONENT_EVENT_HAS_CHANGED, type->name.Copy(allocator), {} });
							continue;
						}

						// Check if the allocator size has changed
						size_t current_allocator_size = GetReflectionComponentAllocatorSize(type);
						size_t old_allocator_size = GetReflectionComponentAllocatorSize(old_type);
						if (current_allocator_size != old_allocator_size) {
							events.Add({ EDITOR_COMPONENT_EVENT_ALLOCATOR_SIZE_CHANGED, type->name.Copy(allocator), {} });
							continue;
						}
					}
					else {
						// Same component but it changed its ID and possibly itself
						Stream<char> existing_type_id = ComponentFromID(current_component, component_type);
						if (existing_type_id.size > 0) {
							// There is already a type with that ID
							// Generate an event and prevent further processing
							events.Add({ EDITOR_COMPONENT_EVENT_SAME_ID, type->name.Copy(allocator), existing_type_id.Copy(allocator) });
						}
						else {
							if (!CompareReflectionTypes(internal_manager, reflection_manager, old_type, type)) {
								events.Add({ EDITOR_COMPONENT_EVENT_DIFFERENT_COMPONENT_DIFFERENT_ID, type->name.Copy(allocator), {}, {}, current_component.value });
							}
							else {
								events.Add({ EDITOR_COMPONENT_EVENT_SAME_COMPONENT_DIFFERENT_ID, type->name.Copy(allocator), {}, {}, current_component.value });
							}
						}
						continue;
					}
				}
				else {
					// Check to see if it changed
					if (!CompareReflectionTypes(internal_manager, reflection_manager, old_type, type)) {
						// Not a component but it changed.
						events.Add({ EDITOR_COMPONENT_EVENT_DEPENDENCY_CHANGED, type->name.Copy(allocator), {} });
						continue;
					}
				}

				// If it passed all tests, verify if the default values have changed. If they did, then copy them
				for (size_t field_index = 0; field_index < type->fields.size; field_index++) {
					if (type->fields[field_index].info.has_default_value) {
						old_type->fields[field_index].info.has_default_value = true;
						memcpy(
							&old_type->fields[field_index].info.default_bool, 
							&type->fields[field_index].info.default_bool, 
							old_type->fields[field_index].info.byte_size
						);
					}
				}
			}
		}
	};

	// Check to see if the module has any components already stored
	if (module_index != -1) {
		// Copy the module names into the temporary list
		temporary_module_types.CopyOther(loaded_modules[module_index].types);

		register_existing_types(component_indices, std::true_type{});
		register_existing_types(hierarchy_types, std::false_type{});

		for (unsigned int index = 0; index < temporary_module_types.size; index++) {
			const ReflectionType* type = internal_manager->type_definitions.GetValuePtr(temporary_module_types[index]);
			bool is_link_component = IsReflectionTypeLinkComponent(type);
			ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(type);
			if (component_type != ECS_COMPONENT_TYPE_COUNT || is_link_component) {
				// Emit a removed event
				if (!is_link_component) {
					events.Add({ 
						EDITOR_COMPONENT_EVENT_IS_REMOVED, 
						type->name.Copy(allocator), 
						{},
						{}, 
						GetReflectionTypeComponent(type), 
						component_type, 
						false 
					});
					// We cannot remove the type since it is needed for asset determination
					// Remove it once the event is finalized
				}
				else {
					// We can remove it now
					// Also destroy the UI type
					RemoveLinkType(editor_state, type->name);
				}
			}
			else {
				// We can remove it safely since it is not a component
				RemoveType(type->name);
			}
		}

		if (added_types.size > 0) {
			// The add events are generated inside the registration
			loaded_modules[module_index].types.Expand(allocator, added_types, true);
		}
	}
	else {
		module_index = loaded_modules.Add({ StringCopy(allocator, module_name) });
		loaded_modules[module_index].link_components.Initialize(allocator, 0);
		register_types(component_indices, std::true_type{});
		register_types(hierarchy_types, std::false_type{});

		loaded_modules[module_index].types.InitializeAndCopy(allocator, added_types);
	}

	auto invalid_link = [&](const ReflectionType* target_type, unsigned int index) {
		Stream<char> link_name = link_components[index].name;
		if (target_type == nullptr || !IsReflectionTypeMaybeComponent(target_type)) {
			// Invalid target, generate an event
			events.Add({ EDITOR_COMPONENT_EVENT_LINK_INVALID_TARGET, link_name.Copy(allocator) });
			RemoveLinkType(editor_state, link_name);
		}
	};

	// Verify the link components for links with undetermined targets
	for (unsigned int index = 0; index < link_components.size; index++) {
		ReflectionType type;
		if (internal_manager->TryGetType(link_components[index].target, type)) {
			bool is_component = GetReflectionTypeComponentType(&type) != ECS_COMPONENT_TYPE_COUNT;
			if (!is_component) {
				invalid_link(&type, index);
			}
		}
		else {
			invalid_link(nullptr, index);
		}
	}
}

// ----------------------------------------------------------------------------------------------

Stream<char> EditorComponents::ComponentFromID(short id, ECS_COMPONENT_TYPE component_type) const
{
	Stream<char> name = { nullptr, 0 };
	internal_manager->type_definitions.ForEachConst<true>([&](const ReflectionType& type, ResourceIdentifier identifier) {
		double evaluation = type.GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
		if (evaluation != DBL_MAX && (unsigned short)evaluation == id) {
			if (component_type == GetReflectionTypeComponentType(&type)) {
				name = type.name;
				return true;
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
	case EDITOR_COMPONENT_EVENT_PROMOTED_TO_COMPONENT:
	case EDITOR_COMPONENT_EVENT_COMPONENT_DEMOTED:
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
	ResizableStream<EditorComponentEvent>* handled_events;
	EditorComponentEvent event_to_handle;

	Stream<EditorComponents::ResolveEventOptions> scene_options;
	Stream<EditorComponents::ResolveEventOptions> runtime_options;

	// This semaphore is used to deallocate the data when all threads have finished
	// The allocation is here at the semaphore
	Semaphore* semaphore;
	SpinLock* finalize_event_lock;
};

struct FinalExecuteComponentEventData {
	ResizableStream<EditorComponentEvent>* handled_events;
	AtomicStream<EditorComponentEvent>* unhandled_events;
};

EDITOR_EVENT(FinalExecuteComponentEvent) {
	FinalExecuteComponentEventData* data = (FinalExecuteComponentEventData*)_data;
	for (unsigned int index = 0; index < data->handled_events->size; index++) {
		data->handled_events->buffer[index].name.Deallocate(editor_state->editor_components.allocator);
		if (data->handled_events->buffer[index].type == EDITOR_COMPONENT_EVENT_SAME_ID) {
			data->handled_events->buffer[index].conflicting_name.Deallocate(editor_state->editor_components.allocator);
		}
	}
	
	data->handled_events->FreeBuffer();
	editor_state->editor_components.events.AddStream(data->unhandled_events->ToStream());
	Deallocate(editor_state->editor_components.allocator, data->handled_events);
	Deallocate(editor_state->editor_components.allocator, data->unhandled_events);

	return false;
}

ECS_THREAD_TASK(ExecuteComponentEvent) {
	ExecuteComponentEventData* data = (ExecuteComponentEventData*)_data;

	bool was_handled = true;
	SandboxAction(data->editor_state, -1, [&](unsigned int sandbox_index) {
		// Update the runtime entity manager - if it paused or running
		EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(data->editor_state, sandbox_index);
		if (sandbox_state != EDITOR_SANDBOX_SCENE) {
			data->editor_state->editor_components.ResolveEvent(
				data->editor_state,
				sandbox_index,
				EDITOR_SANDBOX_VIEWPORT_RUNTIME,
				data->editor_state->module_reflection->reflection,
				data->event_to_handle,
				data->runtime_options.buffer + sandbox_index
			);
		}

		// Now handle the scene manager
		was_handled &= data->editor_state->editor_components.ResolveEvent(
			data->editor_state,
			sandbox_index,
			EDITOR_SANDBOX_VIEWPORT_SCENE,
			data->editor_state->module_reflection->reflection,
			data->event_to_handle,
			data->scene_options.buffer + sandbox_index
		);
	});

	if (!was_handled) {
		data->unhandled_events->Add(data->event_to_handle);
	}
	else {
		data->finalize_event_lock->Lock();
		data->editor_state->editor_components.FinalizeEvent(
			data->editor_state, 
			data->editor_state->module_reflection->reflection, 
			data->event_to_handle
		);
		data->handled_events->Add(data->event_to_handle);
		data->finalize_event_lock->Unlock();
	}

	unsigned int previous_count = data->semaphore->Exit();
	if (previous_count == 1) {
		FinalExecuteComponentEventData final_event_data;
		final_event_data.handled_events = data->handled_events;
		final_event_data.unhandled_events = data->unhandled_events;
		EditorAddEvent(data->editor_state, FinalExecuteComponentEvent, &final_event_data, sizeof(final_event_data));

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
		ConvertWideCharsToASCII(module->library_name, library_name);

		unsigned int hierarchy_index = GetModuleReflectionHierarchyIndex(data->editor_state, index);
		data->editor_state->editor_components.UpdateComponents(
			data->editor_state, 
			data->editor_state->module_reflection->reflection, 
			hierarchy_index, 
			library_name
		);
	}

	ReleaseLockedWindow(action_data);
}

void UserEventsWindow(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	UIDrawConfig config;

	UserEventsWindowData* data = (UserEventsWindowData*)window_data;
	
	EditorStateRemoveOutdatedEvents(data->editor_state, data->user_events);

	drawer.Text("There are events that need to be handled. Instructions:");
	drawer.NextRow();
	Stream<char> label_lists[] = {
		"SAME_ID: Modify the source file for the conflicting components such that they have different IDs",
		"MISSING_FUNCTION: Write the appropiate function for the type",
		"HAS_ALLOCATOR_BUT_NO_BUFFERS: Remove the allocator function",
		"LINK_COMPONENT_MISSING_TARGET: Give the component its target",
		"LINK_COMPONENT_INVALID_TARGET: The target of the link is not a component",
		"LINK_MISMATCH_FOR_DEFAULT: The link component has default conversion but the target cannot be converted to"
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
		Stream<char> type_string = "";

		ECS_STACK_CAPACITY_STREAM(char, temporary_string, 512);

		// Verify if it has been satisfied
		if (component_event == EDITOR_COMPONENT_EVENT_SAME_ID) {
			type_string = " SAME_ID with ";
		}
		else if (component_event == EDITOR_COMPONENT_EVENT_IS_MISSING_FUNCTION) {
			ECS_FORMAT_STRING(temporary_string, " IS_MISSING the function {#}", data->user_events[index].missing_function_name);
			type_string = temporary_string;
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

			// Also, pause all running sandboxes since the user cannot interact with them
			// And they could produce unexpected results
			unsigned int sandbox_count = GetSandboxCount(editor_state);
			for (unsigned int sandbox_index = 0; sandbox_index < sandbox_count; sandbox_index++) {
				if (GetSandboxState(editor_state, sandbox_index) == EDITOR_SANDBOX_RUNNING) {
					PauseSandboxWorld(editor_state, sandbox_index);
				}
			}
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

		size_t sandbox_count = GetSandboxCount(editor_state);
		// For each sandbox we must create its appropriate spin locks
		size_t total_allocation_size = sizeof(Semaphore) + sizeof(SpinLock) + sizeof(EditorComponents::ResolveEventOptions) *
			 sandbox_count * 2;
		SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
			total_allocation_size += EditorComponents::ResolveEventOptionsSize(GetSandbox(editor_state, sandbox_index)->sandbox_world.entity_manager);
			total_allocation_size += EditorComponents::ResolveEventOptionsSize(&GetSandbox(editor_state, sandbox_index)->scene_entities);
		});

		void* allocation = editor_state->multithreaded_editor_allocator->Allocate_ts(total_allocation_size);
		Semaphore* semaphore = (Semaphore*)allocation;
		uintptr_t ptr = (uintptr_t)allocation;
		ptr += sizeof(*semaphore);

		SpinLock* finalize_event_lock = (SpinLock*)ptr;
		finalize_event_lock->Unlock();
		ptr += sizeof(SpinLock);

		AtomicStream<EditorComponentEvent>* unhandled_events = (AtomicStream<EditorComponentEvent>*)Allocate(
			editor_state->editor_components.allocator, 
			sizeof(AtomicStream<EditorComponentEvent>)
		);
		unhandled_events->Initialize(editor_state->editor_components.allocator, editor_state->editor_components.events.size * sandbox_count);
		ResizableStream<EditorComponentEvent>* handled_events = (ResizableStream<EditorComponentEvent>*)Allocate(
			editor_state->editor_components.allocator,
			sizeof(ResizableStream<EditorComponentEvent>)
		);
		handled_events->Initialize(editor_state->MultithreadedEditorAllocator(), 0);

		Stream<EditorComponents::ResolveEventOptions> runtime_options;
		Stream<EditorComponents::ResolveEventOptions> scene_options;

		runtime_options.InitializeFromBuffer(ptr, sandbox_count);
		scene_options.InitializeFromBuffer(ptr, sandbox_count);

		SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
			runtime_options[sandbox_index] = EditorComponents::ResolveEventOptionsFromBuffer(GetSandbox(editor_state, sandbox_index)->sandbox_world.entity_manager, ptr);
			scene_options[sandbox_index] = EditorComponents::ResolveEventOptionsFromBuffer(&GetSandbox(editor_state, sandbox_index)->scene_entities, ptr);
		});

		semaphore->Enter(editor_state->editor_components.events.size);

		for (unsigned int index = 0; index < editor_state->editor_components.events.size; index++) {
			EditorComponentEvent event = editor_state->editor_components.events[index];

			// Increase the prevent launch flag
			EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);

			ExecuteComponentEventData execute_data;
			execute_data.editor_state = editor_state;
			execute_data.event_to_handle = event;
			execute_data.semaphore = semaphore;
			execute_data.scene_options = scene_options;
			execute_data.runtime_options = runtime_options;
			execute_data.unhandled_events = unhandled_events;
			execute_data.finalize_event_lock = finalize_event_lock;
			execute_data.handled_events = handled_events;
			EditorStateAddBackgroundTask(editor_state, ECS_THREAD_TASK_NAME(ExecuteComponentEvent, &execute_data, sizeof(execute_data)));
		}

		editor_state->editor_components.EmptyEventStream();

		EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);
	}
}

// ----------------------------------------------------------------------------------------------

void EditorStateRemoveOutdatedEvents(EditorState* editor_state, ResizableStream<EditorComponentEvent>& user_events)
{
	auto remove_event_step = [&](unsigned int& index) {
		user_events[index].name.Deallocate(editor_state->editor_components.allocator);
		user_events.Remove(index);
		index--;
	};

	for (unsigned int index = 0; index < user_events.size; index++) {
		EditorComponentEvent component_event = user_events[index];
		// Verify if it has been satisfied
		if (component_event.type == EDITOR_COMPONENT_EVENT_SAME_ID) {
			auto remove_same_id = [&](unsigned int& index) {
				// Here we also need to deallocate the second name
				user_events[index].conflicting_name.Deallocate(editor_state->editor_components.allocator);
				remove_event_step(index);
			};

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
				remove_same_id(index);
				continue;
			}

			Component first_component = GetReflectionTypeComponent(&first_type);
			Component second_component = GetReflectionTypeComponent(&second_type);

			if (first_component.value == -1 || second_component.value == -1) {
				// If any of the these is not found, remove the event
				remove_same_id(index);
				continue;
			}

			// Their ids have changed
			if (first_component != second_component) {
				remove_same_id(index);
				continue;
			}
		}
		else if (component_event.type == EDITOR_COMPONENT_EVENT_IS_MISSING_FUNCTION) {
			ReflectionType type;
			if (editor_state->module_reflection->reflection->TryGetType(component_event.name, type)) {
				if (component_event.missing_function_name == ECS_COMPONENT_ID_FUNCTION) {
					Component component_id = GetReflectionTypeComponent(&type);
					if (component_id.value != -1) {
						remove_event_step(index);
						continue;
					}
				}
				else if (component_event.missing_function_name == ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION) {
					size_t allocator_size = GetReflectionComponentAllocatorSize(&type);
					bool has_buffers = HasReflectionTypeComponentBuffers(&type);
					if (has_buffers && allocator_size != 0) {
						remove_event_step(index);
						continue;
					}
					else if (!has_buffers && allocator_size == 0) {
						remove_event_step(index);
						continue;
					}
				}
				else if (component_event.missing_function_name == ECS_COMPONENT_IS_SHARED_FUNCTION) {
					ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(&type);
					if (component_type != ECS_COMPONENT_TYPE_COUNT) {
						remove_event_step(index);
						continue;
					}
				}
			}
			else {
				// Remove the event if the type has disappeared from the module reflection
				remove_event_step(index);
				continue;
			}
		}
		else if (component_event.type == EDITOR_COMPONENT_EVENT_HAS_ALLOCATOR_BUT_NO_BUFFERS) {
			ReflectionType type;
			if (editor_state->module_reflection->reflection->TryGetType(component_event.name, type)) {
				bool has_buffers = HasReflectionTypeComponentBuffers(&type);
				size_t allocator_size = GetReflectionComponentAllocatorSize(&type);
				if (allocator_size != 0 && has_buffers) {
					remove_event_step(index);
					continue;
				}
				else if (allocator_size == 0 && !has_buffers) {
					remove_event_step(index);
					continue;
				}
			}
			else {
				// Remove the event if the type has disapperead from the module reflection
				remove_event_step(index);
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
							// We need to allocate the name
							component_event.name.InitializeAndCopy(editor_state->editor_components.allocator, component_event.name);
							user_events.Add({ 
								EDITOR_COMPONENT_EVENT_LINK_INVALID_TARGET,
								component_event.name.Copy(editor_state->editor_components.allocator)
							});
						}
						// If the validation fails, add a mismatch event
						else if (!ValidateLinkComponent(&type, editor_state->editor_components.GetType(target))) {
							user_events.Add({ EDITOR_COMPONENT_EVENT_LINK_MISMATCH_FOR_DEFAULT, component_event.name.Copy(editor_state->editor_components.allocator) });
						}
						remove_event_step(index);
						continue;
					}
				}
				else {
					// Changed type or it doesn't exist no more
					remove_event_step(index);
					continue;
				}
			}
			else {
				// Remove the event if the type has disapperead from the module reflection
				remove_event_step(index);
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
							user_events.Add({ EDITOR_COMPONENT_EVENT_LINK_MISMATCH_FOR_DEFAULT, component_event.name.Copy(editor_state->editor_components.allocator) });
						}
						remove_event_step(index);
						continue;
					}
				}
				else {
					// Changed type or it doesn't exist no more
					remove_event_step(index);
					continue;
				}
			}
			else {
				// Remove the event if the type has disapperead from the module reflection
				remove_event_step(index);
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
							remove_event_step(index);
							continue;
						}
					}
				}
				else {
					// Changed type or it doesn't exist no more
					remove_event_step(index);
					continue;
				}
			}
			else {
				// Remove the event if the type has disapperead from the module reflection
				remove_event_step(index);
				continue;
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------
