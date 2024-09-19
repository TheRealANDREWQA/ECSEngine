#include "editorpch.h"
#include "ECSEngineModule.h"
#include "ECSEngineComponents.h"
#include "../Inspector.h"
#include "../../Editor/EditorState.h"
#include "../../Sandbox/SandboxEntityOperations.h"
#include "../../Sandbox/Sandbox.h"
#include "../../Assets/EditorSandboxAssets.h"
#include "InspectorUtilities.h"
#include "../../Modules/Module.h"
#include "../AssetOverrides.h"
#include "../../Assets/Prefab.h"
#include "../OpenPrefab.h"
#include "../Common.h"

using namespace ECSEngine;
ECS_TOOLS;

#define INSPECTOR_DRAW_ENTITY_NAME_INPUT_CAPACITY (256)

// ----------------------------------------------------------------------------------------------------------------------------

static void InspectorComponentUIIInstanceName(Stream<char> component_name, Stream<char> base_entity_name, unsigned int sandbox_index, CapacityStream<char>& instance_name) {
	instance_name.CopyOther(component_name);
	instance_name.AddStream(ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
	instance_name.AddStream(base_entity_name);
	ConvertIntToChars(instance_name, sandbox_index);
}

static Stream<char> InspectorComponentNameFromUIInstanceName(Stream<char> instance_name) {
	Stream<char> pattern = FindFirstToken(instance_name, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
	ECS_ASSERT(pattern.size > 0);

	return { instance_name.buffer, instance_name.size - pattern.size };
}

// ----------------------------------------------------------------------------------------------------------------------------

// The name structure is embedded in the structure
struct InspectorDrawEntityData {
	struct MatchingInputs {
		// This is a pointer in order to be stable - if we would have
		// Used the address of CapacityStream<void> directly from
		// The stream, the address could have changed. We also need
		// To record the field name in order to know to update the buffers
		// Later on. The field name needs to be the full name for nested fields
		// For example capacity_inputs.field_name if there is a nested field
		struct Input {
			CapacityStream<void>* input;
			Stream<char> field_name;
		};

		Stream<char> component_name;
		Stream<Input> capacity_inputs;
	};

	// Keep the instance and its pointer. When the components shift around the archetype
	// the UIReflectionInstance will incorrectly keep pointing to the same place when in fact
	// the component was moved or the entity changed the archetype
	struct CreatedInstance {
		Stream<char> name;
		void* pointer_bound;

		// This pointer is used to determine when the allocator for the component
		// Has changed, to update the resizable buffers
		void* allocator_pointer;
	};

	// If a component has a link component, we need to create it and have it live multiple frames
	// Cannot have it be a temporary because the UI requires it to be stable
	struct LinkComponent {
		Stream<char> name;
		void* data;
		void* previous_data;

		// We keep a copy of the target data such that we trigger a conversion from target to link
		// only if they are different
		void* target_data_copy;

		// This is the allocator used to make allocations for the fields of the target_data_copy
		MemoryManager target_allocator;

		void* apply_modifier_initial_target_data;
		bool is_apply_modifier_in_progress;
		// If there is an UI change triggered with a background asset registration,
		// We need to block the conversion from the target to the link since it
		// Will change it back to the original value
		unsigned char is_ui_change_triggered;
	};

	ECS_INLINE AllocatorPolymorphic Allocator() {
		return &allocator;
	}

	ECS_INLINE MemoryManager CreateLinkTargetAllocator(EditorState* editor_state) {
		return MemoryManager(ECS_KB * 32, ECS_KB, ECS_KB * 512, editor_state->EditorAllocator());
	}

	ECS_INLINE AllocatorPolymorphic TargetAllocator(unsigned int link_index) {
		return &link_components[link_index].target_allocator;
	}

	bool IsSharedComponentAndNoLink(const EditorState* editor_state, Stream<char> component_name) {
		bool is_shared = editor_state->editor_components.IsSharedComponent(component_name);
		if (is_shared) {
			bool has_link_component = editor_state->editor_components.IsLinkComponentTarget(component_name);
			if (!has_link_component) {
				return true;
			}
		}
		return false;
	}

	unsigned int AddCreatedInstance(EditorState* editor_state, Stream<char> name, void* pointer_bound, void* allocator_pointer) {
		name = StringCopy(Allocator(), name);

		void* old_buffer = created_instances.buffer;
		size_t previous_size = created_instances.size;
		void* allocation = allocator.Allocate(created_instances.MemoryOf(created_instances.size + 1));
		uintptr_t ptr = (uintptr_t)allocation;
		created_instances.InitializeAndCopy(ptr, created_instances);

		// Check to see if the component is shared and has no link component. If that is the case, we need
		// To bind a pointer that is different from the shared instance storage. We do this in order to
		// Not have the storage be changed when the user interacts with the UI. After a change, we see if
		// there is an instance with that data already and change the instance only, or we create a new instance
		// And assign that one
		Stream<char> component_name = InspectorComponentNameFromUIInstanceName(name);
		bool is_shared_no_link = IsSharedComponentAndNoLink(editor_state, component_name);
		if (is_shared_no_link) {
			size_t byte_size = editor_state->editor_components.GetComponentByteSize(component_name);
			void* component_allocation = allocator.Allocate(byte_size);
			memcpy(component_allocation, pointer_bound, byte_size);
			pointer_bound = component_allocation;
		}

		unsigned int index = created_instances.Add({ name, pointer_bound, allocator_pointer });
		if (previous_size > 0) {
			allocator.Deallocate(old_buffer);
		}
		return index;
	}

	// Resizes the matching inputs
	unsigned int AddComponent(EditorState* editor_state, Stream<char> component_name) {
		void* allocation = allocator.Allocate(matching_inputs.MemoryOf(matching_inputs.size + 1));
		uintptr_t ptr = (uintptr_t)allocation;

		if (matching_inputs.size > 0) {
			allocator.Deallocate(matching_inputs.buffer);
		}
		matching_inputs.InitializeAndCopy(ptr, matching_inputs);

		unsigned int write_index = matching_inputs.size;
		matching_inputs[write_index].component_name = component_name;
		matching_inputs[write_index].capacity_inputs = { nullptr, 0 };
		matching_inputs.size++;

		return write_index;
	}

	// Assumes that the set function was called first
	// The field name needs to be the full field name for nested fields
	// Including the parents separated with a . in between like normal structs
	CapacityStream<void>* AddComponentInput(
		EditorState* editor_state, 
		Stream<char> component_name, 
		Stream<char> field_name, 
		size_t input_capacity, 
		size_t element_byte_size
	) {
		unsigned int index = FindMatchingInput(component_name);
		ECS_ASSERT(index != -1);
		void* allocation = allocator.Allocate(input_capacity * element_byte_size + sizeof(CapacityStream<void>) + field_name.CopySize());
		CapacityStream<void>* new_input = (CapacityStream<void>*)allocation;
		uintptr_t ptr = (uintptr_t)allocation;
		ptr += sizeof(CapacityStream<void>);
		new_input->InitializeFromBuffer(ptr, 0, input_capacity);
		unsigned int insert_index = matching_inputs[index].capacity_inputs.Add({ new_input, field_name.CopyTo(ptr) });
		return matching_inputs[index].capacity_inputs[insert_index].input;
	}

	// Returns the index at which it can be found
	unsigned int AddLinkComponent(EditorState* editor_state, Stream<char> name) {
		// Add it to the matching inputs
		AddComponent(editor_state, name);

		void* allocation = allocator.Allocate(link_components.MemoryOf(link_components.size + 1));
		uintptr_t ptr = (uintptr_t)allocation;

		unsigned int write_index = link_components.size;

		if (link_components.size > 0) {
			allocator.Deallocate(link_components.buffer);
		}
		link_components.InitializeAndCopy(ptr, link_components);

		// We need to allocate the link component twice and the target data once
		link_components[write_index].name = name.Copy(&allocator);
		unsigned short byte_size = editor_state->editor_components.GetComponentByteSize(name);
		link_components[write_index].data = allocator.Allocate(byte_size);
		link_components[write_index].previous_data = allocator.Allocate(byte_size);
		link_components[write_index].apply_modifier_initial_target_data = nullptr;
		link_components[write_index].is_apply_modifier_in_progress = false;
		link_components[write_index].target_allocator = CreateLinkTargetAllocator(editor_state);
		link_components[write_index].is_ui_change_triggered = false;

		Stream<char> target_name = editor_state->editor_components.GetComponentFromLink(name);
		const Reflection::ReflectionType* target_type = editor_state->editor_components.GetType(target_name);
		unsigned short target_type_byte_size = Reflection::GetReflectionTypeByteSize(target_type);
		link_components[write_index].target_data_copy = allocator.Allocate(target_type_byte_size);
		editor_state->editor_components.internal_manager->SetInstanceDefaultData(target_type, link_components[write_index].target_data_copy);

		const Reflection::ReflectionType* link_type = editor_state->editor_components.GetType(name);
		ECSEngine::ResetLinkComponent(editor_state->editor_components.internal_manager, link_type, link_components[write_index].data);
		ECSEngine::ResetLinkComponent(editor_state->editor_components.internal_manager, link_type, link_components[write_index].previous_data);

		link_components.size++;
		return write_index;
	}

	// Deallocates everything and deletes the UI instances
	void Clear(EditorState* editor_state) {
		if (created_instances.size > 0) {
			for (size_t index = 0; index < created_instances.size; index++) {
				if (editor_state->module_reflection->GetInstance(created_instances[index].name) != nullptr) {
					// It is from the module side
					editor_state->module_reflection->DestroyInstance(created_instances[index].name);
				}
				else {
					// It must be from the engine side - or it was removed entirely from source code
					// In that case, we don't have to destroy the instance
					if (editor_state->ui_reflection->GetInstance(created_instances[index].name) != nullptr) {
						editor_state->ui_reflection->DestroyInstance(created_instances[index].name);
					}
				}
			}
		}

		// Free the main allocator and the target allocator for each link component
		for (size_t index = 0; index < link_components.size; index++) {
			link_components[index].target_allocator.Free();
		}
		allocator.Free();
	}

	void ClearComponent(EditorState* editor_state, unsigned int index) {
		for (size_t buffer_index = 0; buffer_index < matching_inputs[index].capacity_inputs.size; buffer_index++) {
			allocator.Deallocate(matching_inputs[index].capacity_inputs[buffer_index].input);
			matching_inputs[index].capacity_inputs[buffer_index].input->size = 0;
		}
		if (matching_inputs[index].capacity_inputs.size > 0) {
			allocator.Deallocate(matching_inputs[index].capacity_inputs.buffer);
		}
		matching_inputs[index].capacity_inputs.size = 0;
	}

	void ClearLinkComponent(EditorState* editor_state, unsigned int index, unsigned int sandbox_index) {
		// Unregister the asset handles
		UnregisterSandboxLinkComponent(editor_state, sandbox_index, link_components[index].data, link_components[index].name);

		ClearLinkComponentSkipAssets(editor_state, index);
	}

	// Clears the component without unregistering the asset handles
	void ClearLinkComponentSkipAssets(EditorState* editor_state, unsigned int index) {
		unsigned int matching_input_index = FindMatchingInput(link_components[index].name);
		ClearComponent(editor_state, matching_input_index);
		// Reallocate the link component - since it might have changed byte size
		unsigned short new_byte_size = editor_state->editor_components.GetComponentByteSize(link_components[index].name);
		ECS_ASSERT(new_byte_size != USHORT_MAX);

		allocator.Deallocate(link_components[index].data);
		allocator.Deallocate(link_components[index].previous_data);
		allocator.Deallocate(link_components[index].target_data_copy);
		link_components[index].data = allocator.Allocate(new_byte_size);
		link_components[index].previous_data = allocator.Allocate(new_byte_size);
		Stream<char> target_name = editor_state->editor_components.GetComponentFromLink(link_components[index].name);
		unsigned short target_byte_size = editor_state->editor_components.GetComponentByteSize(target_name);
		link_components[index].target_data_copy = allocator.Allocate(target_byte_size);
		editor_state->editor_components.internal_manager->SetInstanceDefaultData(target_name, link_components[index].target_data_copy);

		const Reflection::ReflectionType* link_type = editor_state->editor_components.GetType(link_components[index].name);
		ECSEngine::ResetLinkComponent(editor_state->editor_components.internal_manager, link_type, link_components[index].data);
		ECSEngine::ResetLinkComponent(editor_state->editor_components.internal_manager, link_type, link_components[index].previous_data);
	}

	const void* TargetComponentData(const EditorState* editor_state, unsigned int sandbox_index, unsigned int link_index) const {
		if (is_global_component) {
			ECS_ASSERT(link_index == 0);
			// There is a single link
			return GetSandboxGlobalComponent(editor_state, sandbox_index, global_component);
		}
		else {
			Stream<char> target_name = editor_state->editor_components.GetComponentFromLink(link_components[link_index].name);
			Component component = editor_state->editor_components.GetComponentID(target_name);
			bool is_shared = editor_state->editor_components.IsSharedComponent(target_name);

			return GetSandboxEntityComponentEx(editor_state, sandbox_index, entity, component, is_shared);
		}
	}

	void* TargetComponentData(EditorState* editor_state, unsigned int sandbox_index, unsigned int link_index) const {
		if (is_global_component) {
			ECS_ASSERT(link_index == 0);
			// There is a single link
			return GetSandboxGlobalComponent(editor_state, sandbox_index, global_component);
		}
		else {
			Stream<char> target_name = editor_state->editor_components.GetComponentFromLink(link_components[link_index].name);
			Component component = editor_state->editor_components.GetComponentID(target_name);
			bool is_shared = editor_state->editor_components.IsSharedComponent(target_name);

			return GetSandboxEntityComponentEx(editor_state, sandbox_index, entity, component, is_shared);
		}
	}

	void CopyLinkComponentData(const EditorState* editor_state, unsigned int sandbox_index, unsigned int link_index) {
		ClearAllocator(TargetAllocator(link_index));

		const void* target_data = TargetComponentData(editor_state, sandbox_index, link_index);
		Stream<char> target_name = editor_state->editor_components.GetComponentFromLink(link_components[link_index].name);
		const Reflection::ReflectionType* target_type = editor_state->editor_components.GetType(target_name);
		Reflection::CopyReflectionDataOptions copy_options;
		copy_options.allocator = TargetAllocator(link_index);
		copy_options.always_allocate_for_buffers = true;
		Reflection::CopyReflectionTypeInstance(
			editor_state->editor_components.internal_manager, 
			target_type, 
			target_data, 
			link_components[link_index].target_data_copy, 
			&copy_options
		);
	}

	Stream<char> CreatedInstanceComponentName(unsigned int index) const {
		Stream<char> separator = FindFirstToken(created_instances[index].name, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
		ECS_ASSERT(separator.size > 0);
		return { created_instances[index].name.buffer, created_instances[index].name.size - separator.size };
	}

	ECS_INLINE void DeallocateLinkApplyModifierAndResetData(const EditorState* editor_state, unsigned int link_index) {
		allocator.Deallocate(link_components[link_index].apply_modifier_initial_target_data);
		link_components[link_index].apply_modifier_initial_target_data = nullptr;
		link_components[link_index].is_apply_modifier_in_progress = false;

		const Reflection::ReflectionType* reflection_type = editor_state->editor_components.GetType(link_components[link_index].name);
		ResetModifierFieldsLinkComponent(reflection_type, link_components[link_index].data);
		Reflection::CopyReflectionDataOptions copy_options;
		copy_options.allocator = Allocator();
		copy_options.always_allocate_for_buffers = true;
		Reflection::CopyReflectionTypeInstance(
			editor_state->editor_components.internal_manager,
			reflection_type,
			link_components[link_index].data,
			link_components[link_index].previous_data,
			&copy_options
		);
	}

	ECS_INLINE unsigned int FindMatchingInput(Stream<char> component_name) const {
		return FindString(component_name, matching_inputs, [](MatchingInputs input) { return input.component_name; });
	}

	ECS_INLINE unsigned int FindCreatedInstance(Stream<char> name) const {
		return FindString(name, created_instances, [](CreatedInstance instance) { return instance.name; });
	}

	unsigned int FindCreatedInstanceByComponentName(unsigned int sandbox_index, Stream<char> component_name) {
		ECS_STACK_CAPACITY_STREAM(char, full_name, 1024);
		ECS_STACK_CAPACITY_STREAM(char, entity_name, 512);
		entity.ToString(entity_name, true);
		InspectorComponentUIIInstanceName(component_name, entity_name, sandbox_index, full_name);
		return FindCreatedInstance(full_name);
	}

	ECS_INLINE unsigned int FindLinkComponent(Stream<char> name) const {
		return FindString(name, link_components, [](LinkComponent component) { return component.name; });
	}

	// Finds a link component based on the fact that the pointer must belong to the link data
	unsigned int FindLinkComponentFromPointer(const EditorState* editor_state, const void* pointer) const {
		for (unsigned int index = 0; index < link_components.size; index++) {
			unsigned short byte_size = editor_state->editor_components.GetComponentByteSize(link_components[index].name);
			if (IsPointerRange(link_components[index].data, byte_size, pointer)) {
				return index;
			}
		}
		return -1;
	}

	bool IsCreatedInstanceValid(EditorState* editor_state, unsigned int index) const {
		if (editor_state->module_reflection->GetInstance(created_instances[index].name) != nullptr) {
			return true;
		}

		if (editor_state->ui_reflection->GetInstance(created_instances[index].name) != nullptr) {
			return true;
		}

		return false;
	}

	void InitializeLinkApplyModifierData(const EditorState* editor_state, unsigned int sandbox_index, unsigned int link_index) {
		Stream<char> target_type_name = editor_state->editor_components.GetComponentFromLink(link_components[link_index].name);
		const Reflection::ReflectionType* target_type = editor_state->editor_components.GetType(target_type_name);
		size_t byte_size = Reflection::GetReflectionTypeByteSize(target_type);
		link_components[link_index].apply_modifier_initial_target_data = allocator.Allocate(byte_size);
		
		Component target_component = editor_state->editor_components.GetComponentID(target_type_name);
		const void* component_data = nullptr;
		if (is_global_component) {
			component_data = GetSandboxGlobalComponent(editor_state, sandbox_index, global_component);
		}
		else {
			bool is_shared = editor_state->editor_components.IsSharedComponent(target_type_name);
			component_data = GetSandboxEntityComponentEx(editor_state, sandbox_index, entity, target_component, is_shared);
		}
		Reflection::CopyReflectionDataOptions copy_options;
		copy_options.allocator = &allocator;
		copy_options.always_allocate_for_buffers = true;
		Reflection::CopyReflectionTypeInstance(
			editor_state->editor_components.internal_manager,
			target_type_name,
			component_data,
			link_components[link_index].apply_modifier_initial_target_data,
			&copy_options
		);

		link_components[link_index].is_apply_modifier_in_progress = true;
	}

	void RemoveCreatedInstance(EditorState* editor_state, unsigned int index) {
		// Don't forget to deallocate the pointer bound in case it is shared and without a link
		Stream<char> component_name = InspectorComponentNameFromUIInstanceName(created_instances[index].name);
		if (IsSharedComponentAndNoLink(editor_state, component_name)) {
			allocator.Deallocate(created_instances[index].pointer_bound);
		}

		if (editor_state->module_reflection->GetInstance(created_instances[index].name) != nullptr) {
			// It is from the module side
			editor_state->module_reflection->DestroyInstance(created_instances[index].name);
		}
		else {
			// It must be from the engine side - or it was removed entirely from source code
			// In that case, we don't need to destroy any instance
			if (editor_state->ui_reflection->GetInstance(created_instances[index].name) != nullptr) {
				editor_state->ui_reflection->DestroyInstance(created_instances[index].name);
			}
		}
		allocator.Deallocate(created_instances[index].name.buffer);
		created_instances.RemoveSwapBack(index);
	}

	void RemoveCreatedInstance(EditorState* editor_state, Stream<char> component_name) {
		for (size_t index = 0; index < created_instances.size; index++) {
			// The name of the component is at the beginning of each instance name
			if (memcmp(created_instances[index].name.buffer, component_name.buffer, component_name.size * sizeof(char)) == 0) {
				RemoveCreatedInstance(editor_state, index);
				break;
			}
		}
	}

	// Removes all the memory allocated for the capacity inputs of that component
	void RemoveComponent(EditorState* editor_state, Stream<char> component_name) {
		unsigned int index = FindMatchingInput(component_name);
		if (index != -1) {
			// Some components can be missing the matching inputs (they have no buffers)
			ClearComponent(editor_state, index);
			matching_inputs.RemoveSwapBack(index);
		}
		RemoveCreatedInstance(editor_state, component_name);
	}

	void RemoveLinkComponent(EditorState* editor_state, unsigned int index) {
		Stream<char> component_name = link_components[index].name;
		link_components[index].name.Deallocate(&allocator);
		allocator.Deallocate(link_components[index].data);
		allocator.Deallocate(link_components[index].previous_data);
		allocator.Deallocate(link_components[index].target_data_copy);
		FreeAllocatorFrom(TargetAllocator(index), editor_state->EditorAllocator());
		if (link_components[index].apply_modifier_initial_target_data != nullptr) {
			allocator.Deallocate(link_components[index].apply_modifier_initial_target_data);
		}
		link_components.RemoveSwapBack(index);

		// Also remove it from the matching inputs, we can safely use the name
		// Since deallocating it won't overwrite its data
		RemoveComponent(editor_state, component_name);
	}

	void RemoveLinkComponent(EditorState* editor_state, Stream<char> name) {
		unsigned int index = FindLinkComponent(name);
		ECS_ASSERT(index != -1);
		RemoveLinkComponent(editor_state, index);
	}

	void ResetComponent(EditorState* editor_state, unsigned int matching_index) {
		if (matching_index == -1) {
			// Don't do anything
			return;
		}

		for (size_t index = 0; index < matching_inputs[matching_index].capacity_inputs.size; index++) {
			matching_inputs[matching_index].capacity_inputs[index].input->size = 0;
		}
	}

	void ResetLinkComponent(EditorState* editor_state, unsigned int link_index, unsigned int sandbox_index) {
		unsigned int matching_input_index = FindMatchingInput(link_components[link_index].name);
		ResetComponent(editor_state, matching_input_index);

		const Reflection::ReflectionType* link_type = editor_state->editor_components.GetType(link_components[link_index].name);
		ECSEngine::ResetLinkComponent(editor_state->editor_components.internal_manager, link_type, link_components[link_index].data);
		ECSEngine::ResetLinkComponent(editor_state->editor_components.internal_manager, link_type, link_components[link_index].previous_data);
	}

	void SetComponentInputCount(EditorState* editor_state, Stream<char> component_name, size_t count) {
		unsigned int index = FindMatchingInput(component_name);
		ECS_ASSERT(index != -1);
		void* allocation = allocator.Allocate(matching_inputs[index].capacity_inputs.MemoryOf(count));
		matching_inputs[index].capacity_inputs.InitializeFromBuffer(allocation, 0);
	}

	// This removes components which no longer are attached to the entity
	// Only works for the entity case
	void UpdateStoredComponents(EditorState* editor_state, unsigned int sandbox_index) {
		const EntityManager* active_entity_manager = ActiveEntityManager(editor_state, sandbox_index);
		ComponentSignature unique_signature = active_entity_manager->GetEntitySignatureStable(entity);
		SharedComponentSignature shared_signature = active_entity_manager->GetEntitySharedSignatureStable(entity);
		
		ECS_STACK_CAPACITY_STREAM(bool, valid_created_instances, 512);
		ECS_ASSERT(valid_created_instances.capacity >= created_instances.size);
		memset(valid_created_instances.buffer, 0, valid_created_instances.MemoryOf(valid_created_instances.capacity));

		ECS_STACK_CAPACITY_STREAM(char, entity_name_storage, 512);
		entity.ToString(entity_name_storage, true);
		Stream<char> entity_name = entity_name_storage;

		ECS_STACK_CAPACITY_STREAM(char, full_component_name_storage, 1024);

		auto validate_component = [&](Stream<char> component_name) {
			Stream<char> link_name = editor_state->editor_components.GetLinkComponentForTarget(component_name);
			if (link_name.size > 0) {
				component_name = link_name;
			}
			full_component_name_storage.size = 0;
			InspectorComponentUIIInstanceName(component_name, entity_name, sandbox_index, full_component_name_storage);
			unsigned int created_instance_index = FindCreatedInstance(full_component_name_storage);
			if (created_instance_index != -1) {
				valid_created_instances[created_instance_index] = true;
			}
		};

		for (size_t index = 0; index < unique_signature.count; index++) {
			Stream<char> component_name = active_entity_manager->GetComponentName(unique_signature[index]);
			validate_component(component_name);
		}

		for (size_t index = 0; index < shared_signature.count; index++) {
			Stream<char> component_name = active_entity_manager->GetSharedComponentName(shared_signature.indices[index]);
			validate_component(component_name);
		}

		for (size_t index = 0; index < created_instances.size; index++) {
			if (!valid_created_instances[index]) {
				Stream<char> component_name = InspectorComponentNameFromUIInstanceName(created_instances[index].name);
				unsigned int link_index = FindLinkComponent(component_name);
				if (link_index != -1) {
					RemoveLinkComponent(editor_state, component_name);
				}
				else {
					RemoveComponent(editor_state, component_name);
				}
			}
		}
	}

	void UpdateLinkComponentsApplyModifier(const EditorState* editor_state) {
		for (size_t index = 0; index < link_components.size; index++) {
			if (!link_components[index].is_apply_modifier_in_progress && link_components[index].apply_modifier_initial_target_data != nullptr) {
				DeallocateLinkApplyModifierAndResetData(editor_state, index);
			}
			if (editor_state->ui_system->m_mouse->IsRaised(ECS_MOUSE_LEFT)) {
				// This is a hack for the moment. Think of a way such that when an input doesn't produce a change
				// (Like when the mouse is not moved but held down), that it won't trigger a deallocate
				link_components[index].is_apply_modifier_in_progress = false;
			}
		}
	}

	void UIUpdateLinkComponent(EditorState* editor_state, unsigned int sandbox_index, unsigned int link_index, bool apply_modifier) {
		Stream<char> link_name = link_components[link_index].name;

		void* link_data = link_components[link_index].data;
		void* previous_link_data = link_components[link_index].previous_data;
		const void* previous_target_data = link_components[link_index].target_data_copy;
		SandboxUpdateLinkComponentForEntityInfo info;
		info.apply_modifier_function = apply_modifier;

		if (apply_modifier) {
			if (link_components[link_index].apply_modifier_initial_target_data == nullptr) {
				InitializeLinkApplyModifierData(editor_state, sandbox_index, link_index);
			}
			link_components[link_index].is_apply_modifier_in_progress = true;
			info.target_previous_data = link_components[link_index].apply_modifier_initial_target_data;
		}

		Stream<char> target_name = editor_state->editor_components.GetComponentFromLink(link_name);
		if (is_global_component) {
			ConvertEditorLinkComponentToTarget(
				editor_state,
				link_name,
				ActiveEntityManager(editor_state, sandbox_index)->GetGlobalComponent(global_component),
				link_data,
				previous_target_data,
				previous_link_data,
				apply_modifier
			);
		}
		else {
			bool is_shared = editor_state->editor_components.IsSharedComponent(target_name);
			if (is_shared) {
				SandboxUpdateSharedLinkComponentForEntity(editor_state, sandbox_index, link_data, link_name, entity, previous_link_data, info);
			}
			else {
				SandboxUpdateUniqueLinkComponentForEntity(editor_state, sandbox_index, link_data, link_name, entity, previous_link_data, info);
			}
		}

		const Reflection::ReflectionType* link_type = editor_state->editor_components.GetType(link_name);
		if (!apply_modifier) {
			Reflection::CopyReflectionDataOptions copy_options;
			copy_options.allocator = Allocator();
			copy_options.always_allocate_for_buffers = true;
			copy_options.deallocate_existing_buffers = true;
			Reflection::CopyReflectionTypeInstance(
				editor_state->editor_components.internal_manager,
				link_type,
				link_data,
				previous_link_data,
				&copy_options
			);
		}
		// The else branch is executed before the update
		
		// Update the target data copy
		ClearAllocator(TargetAllocator(link_index));
		const void* target_data = TargetComponentData(editor_state, sandbox_index, link_index);
		Reflection::CopyReflectionDataOptions copy_options;
		copy_options.allocator = TargetAllocator(link_index);
		copy_options.always_allocate_for_buffers = true;
		Reflection::CopyReflectionTypeInstance(
			editor_state->editor_components.internal_manager,
			target_name,
			target_data,
			link_components[link_index].target_data_copy,
			&copy_options
		);
	}

	// Updates all link components whose targets have changed
	void UpdateLinkComponentsFromTargets(const EditorState* editor_state, unsigned int sandbox_index) {
		for (size_t index = 0; index < link_components.size; index++) {
			// Perform this check only if there is no pending UI change
			if (link_components[index].is_ui_change_triggered == 0) {
				const void* target_data = TargetComponentData(editor_state, sandbox_index, index);
				void* current_data = link_components[index].target_data_copy;
				Stream<char> link_name = link_components[index].name;
				Stream<char> target_name = editor_state->editor_components.GetComponentFromLink(link_name);

				ECS_STACK_CAPACITY_STREAM(Reflection::CompareReflectionTypeInstanceBlittableType, blittable_types, 32);
				size_t blittable_count = ECS_ASSET_TARGET_FIELD_NAMES_SIZE();
				// This will include the misc Stream<void> with a pointer but that is no problem, shouldn't really happen in code
				for (size_t blittable_index = 0; blittable_index < blittable_count; blittable_index++) {
					blittable_types.AddAssert({ ECS_ASSET_TARGET_FIELD_NAMES[blittable_index].name, Reflection::ReflectionStreamFieldType::Pointer });
				}
				Reflection::CompareReflectionTypeInstancesOptions compare_options;
				compare_options.blittable_types = blittable_types;
				if (!Reflection::CompareReflectionTypeInstances(editor_state->editor_components.internal_manager, target_name, target_data, current_data, 1, &compare_options)) {
					// We need to convert the target to the link and update the target data copy
					bool success = ConvertEditorTargetToLinkComponent(
						editor_state,
						link_name,
						target_data,
						link_components[index].data,
						link_components[index].target_data_copy,
						link_components[index].previous_data,
						Allocator()
					);
					if (!success) {
						if (!is_global_component) {
							ECS_STACK_CAPACITY_STREAM(char, entity_name_storage, 512);
							Stream<char> entity_name = GetSandboxEntityName(editor_state, sandbox_index, entity, entity_name_storage);

							ECS_FORMAT_TEMP_STRING(error_message, "Failed to convert target to link component {#} for entity {#} in sandbox {#}.",
								link_components[index].name, entity_name, sandbox_index);
							EditorSetConsoleError(error_message);
						}
						else {
							Stream<char> component_name = editor_state->editor_components.ComponentFromID(global_component, ECS_COMPONENT_GLOBAL);
							ECS_FORMAT_TEMP_STRING(error_message, "Failed to convert target to link component {#} for global component {#} in sandbox {#}.",
								link_components[index].name, component_name, sandbox_index);
						}
					}
					else {
						const Reflection::ReflectionType* link_type = editor_state->editor_components.GetType(link_name);
						Reflection::CopyReflectionDataOptions copy_options;
						copy_options.allocator = Allocator();
						copy_options.always_allocate_for_buffers = true;
						copy_options.deallocate_existing_buffers = true;
						Reflection::CopyReflectionTypeInstance(
							editor_state->editor_components.internal_manager,
							link_type,
							link_components[index].data,
							link_components[index].previous_data,
							&copy_options
						);
					}

					// Also copy the target data copy
					ClearAllocator(TargetAllocator(index));
					Reflection::CopyReflectionDataOptions copy_options;
					copy_options.allocator = TargetAllocator(index);
					copy_options.always_allocate_for_buffers = true;
					Reflection::CopyReflectionTypeInstance(
						editor_state->editor_components.internal_manager,
						target_name,
						target_data,
						current_data,
						&copy_options
					);
				}
			}
		}

		// Include in this function the case for shared components without link
		// They have a "pseudo" link component in the sense that the data is detached from
		// The shared instance in order to not affect them all
		for (size_t index = 0; index < created_instances.size; index++) {
			Stream<char> component_name = InspectorComponentNameFromUIInstanceName(created_instances[index].name);
			if (IsSharedComponentAndNoLink(editor_state, component_name)) {
				const void* shared_data = GetSandboxEntityComponentEx(editor_state, sandbox_index, entity, editor_state->editor_components.GetComponentID(component_name), true);
				size_t byte_size = editor_state->editor_components.GetComponentByteSize(component_name);
				memcpy(created_instances[index].pointer_bound, shared_data, byte_size);
			}
		}
	}

	void UpdateComponentAllocators(EditorState* editor_state, unsigned int sandbox_index) {
		for (size_t index = 0; index < created_instances.size; index++) {
			UIReflectionDrawer* ui_drawer = editor_state->module_reflection;
			UIReflectionInstance* instance = ui_drawer->GetInstance(created_instances[index].name);
			if (instance == nullptr) {
				ui_drawer = editor_state->ui_reflection;
				instance = ui_drawer->GetInstance(created_instances[index].name);
				ECS_ASSERT(instance != nullptr);
			}

			Stream<char> component_name = CreatedInstanceComponentName(index);
			if (editor_state->editor_components.IsLinkComponent(component_name)) {
				component_name = editor_state->editor_components.GetComponentFromLink(component_name);
			}
			Component component = editor_state->editor_components.GetComponentID(component_name);
			ECS_COMPONENT_TYPE component_type = editor_state->editor_components.GetComponentType(component_name);
			AllocatorPolymorphic component_allocator = ActiveEntityManager(editor_state, sandbox_index)->GetComponentAllocatorFromType(component, component_type);
			if (component_allocator.allocator != created_instances[index].allocator_pointer) {
				// Reassign the allocator
				ui_drawer->AssignInstanceResizableAllocator(instance, component_allocator, false);
				created_instances[index].allocator_pointer = component_allocator.allocator;
			}
		}
	}

	// Use the same implementation for global components and entities - global components
	// can be treated like entities with a single unique component
	bool header_state[ECS_ARCHETYPE_MAX_COMPONENTS + ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
	bool is_debug_draw_enabled[ECS_ARCHETYPE_MAX_COMPONENTS + ECS_ARCHETYPE_MAX_SHARED_COMPONENTS];
	bool is_global_component;
	bool is_initialized;
	union {
		Entity entity;
		Component global_component;
	};
	CapacityStream<char> name_input;
	// Used for text input, file input and directory input in order to set their capacity streams
	// Every component gets a resizable stream with capacity streams for their fields
	Stream<MatchingInputs> matching_inputs;

	Stream<LinkComponent> link_components;

	// Destroy these when changing to another entity such that when returning to it it won't crash
	Stream<CreatedInstance> created_instances;

	// All allocations for internal components need to be made from this allocator
	MemoryManager allocator;

	// Store this. We need to detect when the scene is changed
	// Such that we can invalidate the entries from here
	CapacityStream<wchar_t> scene_path;

	// This is used for the callback to not set the scene as dirty
	// When transitioning from runtime data to scene data. We need
	// 3 entries because number inputs trigger 1 frame later after
	// The transition, so we need the state from 2 frames ago
	EDITOR_SANDBOX_VIEWPORT last_frames_data_source[3];
};

ECS_INLINE static Stream<char> InspectorTargetName(
	const InspectorDrawEntityData* draw_data, 
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	CapacityStream<char> name_storage
) {
	if (draw_data->is_global_component) {
		return ActiveEntityManager(editor_state, sandbox_index)->GetGlobalComponentName(draw_data->global_component);
	}
	else {
		return GetSandboxEntityName(editor_state, sandbox_index, draw_data->entity, name_storage);
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

static void DetermineDebugDrawStates(const EditorState* editor_state, unsigned int sandbox_index, InspectorDrawEntityData* draw_data) {
	if (draw_data->is_global_component) {
		// Just check for the single global component
		draw_data->is_debug_draw_enabled[0] = IsSandboxDebugDrawComponentEnabled(editor_state, sandbox_index, draw_data->global_component, ECS_COMPONENT_GLOBAL);
	}
	else {
		ComponentSignature unique_signature = SandboxEntityUniqueComponents(editor_state, sandbox_index, draw_data->entity);
		ComponentSignature shared_signature = SandboxEntitySharedComponents(editor_state, sandbox_index, draw_data->entity);
		for (unsigned int index = 0; index < unique_signature.count; index++) {
			draw_data->is_debug_draw_enabled[index] = IsSandboxDebugDrawComponentEnabled(editor_state, sandbox_index, unique_signature[index], ECS_COMPONENT_UNIQUE);
		}
		for (unsigned int index = 0; index < shared_signature.count; index++) {
			draw_data->is_debug_draw_enabled[index + unique_signature.count] = IsSandboxDebugDrawComponentEnabled(
				editor_state, 
				sandbox_index, 
				shared_signature[index], 
				ECS_COMPONENT_SHARED
			);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorCleanEntity(EditorState* editor_state, unsigned int inspector_index, void* _data) {
	InspectorDrawEntityData* data = (InspectorDrawEntityData*)_data;
	data->Clear(editor_state);
}

// ----------------------------------------------------------------------------------------------------------------------------

// This is what the click handlers will use
struct AddComponentCallbackData {
	InspectorDrawEntityData* draw_data;
	EditorState* editor_state;
	unsigned int sandbox_index;
	Stream<char> component_name;
};

void AddComponentCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	AddComponentCallbackData* data = (AddComponentCallbackData*)_data;
	Stream<char> link_target = data->editor_state->editor_components.GetComponentFromLink(data->component_name);
	AddSandboxEntityComponentEx(data->editor_state, data->sandbox_index, data->draw_data->entity, link_target.size > 0 ? link_target : data->component_name);
	// Re-render the sandbox as well
	RenderSandboxViewports(data->editor_state, data->sandbox_index);

	// Redetermine the debug states
	DetermineDebugDrawStates(data->editor_state, data->sandbox_index, data->draw_data);
};

// ----------------------------------------------------------------------------------------------------------------------------

struct RemoveComponentCallbackData {
	InspectorDrawEntityData* draw_data;
	EditorState* editor_state;
	unsigned int sandbox_index;
	Stream<char> component_name;
};

void RemoveComponentCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	RemoveComponentCallbackData* data = (RemoveComponentCallbackData*)_data;
	Stream<char> link_target = data->editor_state->editor_components.GetComponentFromLink(data->component_name);
	if (link_target.size > 0) {
		data->draw_data->RemoveLinkComponent(data->editor_state, data->component_name);
		data->component_name = link_target;
	}
	else {
		data->draw_data->RemoveComponent(data->editor_state, data->component_name);
	}
	RemoveSandboxEntityComponentEx(data->editor_state, data->sandbox_index, data->draw_data->entity, data->component_name);
	// Re-render the sandbox as well
	RenderSandboxViewports(data->editor_state, data->sandbox_index);

	// Redetermine the debug states
	DetermineDebugDrawStates(data->editor_state, data->sandbox_index, data->draw_data);
}

// ----------------------------------------------------------------------------------------------------------------------------

struct EnableDebugDrawCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	Stream<char> component_name;
};

void EnableDebugDrawCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EnableDebugDrawCallbackData* data = (EnableDebugDrawCallbackData*)_data;
	bool* flag = (bool*)_additional_data;
	Stream<char> component_name = data->editor_state->editor_components.GetComponentFromLink(data->component_name);
	if (component_name.size == 0) {
		component_name = data->component_name;
	}
	Component component = data->editor_state->editor_components.GetComponentID(component_name);
	ECS_COMPONENT_TYPE type = data->editor_state->editor_components.GetComponentType(component_name);
	if (*flag) {
		// Now it is enable
		AddSandboxDebugDrawComponent(data->editor_state, data->sandbox_index, component, type);
	}
	else {
		// Now it is disabled
		RemoveSandboxDebugDrawComponent(data->editor_state, data->sandbox_index, component, type);
	}

	// Rerender the viewport
	RenderSandboxViewports(data->editor_state, data->sandbox_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

struct ResetComponentCallbackData {
	InspectorDrawEntityData* draw_data;
	EditorState* editor_state;
	unsigned int sandbox_index;
	Stream<char> component_name;
};

void ResetComponentCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ResetComponentCallbackData* data = (ResetComponentCallbackData*)_data;
	Stream<char> target = data->editor_state->editor_components.GetComponentFromLink(data->component_name);
	if (target.size > 0) {
		// Clear the link component
		unsigned int link_index = data->draw_data->FindLinkComponent(data->component_name);
		ECS_ASSERT(link_index != -1);
		data->draw_data->ResetLinkComponent(data->editor_state, link_index, data->sandbox_index);
		
		// Make the reset on the target
		data->component_name = target;
	}
	else {
		// Clear the normal component
		unsigned int matching_index = data->draw_data->FindMatchingInput(data->component_name);
		data->draw_data->ResetComponent(data->editor_state, matching_index);
	}

	if (data->draw_data->is_global_component) {
		ResetSandboxGlobalComponent(data->editor_state, data->sandbox_index, data->draw_data->global_component);
	}
	else {
		ResetSandboxEntityComponent(data->editor_state, data->sandbox_index, data->draw_data->entity, data->component_name);
	}
	// Re-render the sandbox as well
	RenderSandboxViewports(data->editor_state, data->sandbox_index);
	SetSandboxSceneDirty(data->editor_state, data->sandbox_index);
}

// ----------------------------------------------------------------------------------------------------------------------------

struct InspectorComponentCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	bool apply_modifier;

	Stream<char> component_name;
	InspectorDrawEntityData* draw_data;
};

void InspectorComponentCallback(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	InspectorComponentCallbackData* data = (InspectorComponentCallbackData*)_data;
	
	EditorState* editor_state = data->editor_state;
	unsigned int sandbox_index = data->sandbox_index;
	Entity entity = data->draw_data->entity;
	EntityManager* active_manager = ActiveEntityManager(editor_state, sandbox_index);
	bool is_global_component = data->draw_data->is_global_component;
	Component global_component = data->draw_data->global_component;

	// Number inputs trigger one frame later after the change, that's why we are using
	// The data source from 2 frames ago
	if (data->draw_data->last_frames_data_source[0] == GetSandboxActiveViewport(editor_state, sandbox_index)) {
		SetSandboxSceneDirty(editor_state, sandbox_index);
	}

	Stream<char> component_name = data->component_name;
	const Reflection::ReflectionType* type = editor_state->editor_components.GetType(component_name);

	Stream<char> target = editor_state->editor_components.GetComponentFromLink(component_name);
	const Reflection::ReflectionType* target_type = editor_state->editor_components.GetType(target);
	const Reflection::ReflectionType* info_type = target_type != nullptr ? target_type : type;

	bool is_shared = IsReflectionTypeSharedComponent(info_type);
	Component component = GetReflectionTypeComponent(info_type);

	if (is_shared) {
		// We need to change the shared instance
		if (data->draw_data->IsSharedComponentAndNoLink(editor_state, component_name)) {
			unsigned int created_index = data->draw_data->FindCreatedInstanceByComponentName(sandbox_index, component_name);
			ECS_ASSERT(created_index != -1);
			SharedInstance new_instance = FindOrCreateSandboxSharedComponentInstance(editor_state, sandbox_index, component, data->draw_data->created_instances[created_index].pointer_bound);
			SetSandboxEntitySharedInstance(editor_state, sandbox_index, entity, component, new_instance);
		}
	}

	unsigned int matching_index = data->draw_data->FindMatchingInput(component_name);
	if (matching_index != -1) {
		// Check to see if it has buffers and copy them if they have changed
		unsigned int ui_input_count = data->draw_data->matching_inputs[matching_index].capacity_inputs.size;
		if (ui_input_count > 0) {
			MemoryArena* arena = nullptr;
			void* component_data = nullptr;
			if (data->draw_data->is_global_component) {
				arena = active_manager->GetGlobalComponentAllocator(component);
				component_data = active_manager->GetGlobalComponent(component);
			}
			else {
				if (is_shared) {
					arena = active_manager->GetSharedComponentAllocator(component);
					SharedInstance shared_instance = SandboxEntitySharedInstance(editor_state, sandbox_index, entity, component);
					component_data = GetSandboxSharedInstance(
						editor_state,
						sandbox_index,
						component,
						shared_instance
					);
				}
				else {
					arena = active_manager->GetComponentAllocator(component);
					component_data = GetSandboxEntityComponent(editor_state, sandbox_index, entity, component);
				}
			}

			// Use the module reflection manager
			const Reflection::ReflectionManager* reflection_manager = editor_state->ModuleReflectionManager();
			const Reflection::ReflectionType* component_type = reflection_manager->GetType(component_name);
			Reflection::SetReflectionTypeInstanceBufferOptions set_buffer_options;
			set_buffer_options.allocator = arena;
			set_buffer_options.checked_copy = true;
			set_buffer_options.deallocate_existing = true;

			for (unsigned int index = 0; index < ui_input_count; index++) {
				Reflection::ReflectionTypeFieldDeep deep_field = Reflection::FindReflectionTypeFieldDeep(
					reflection_manager,
					component_type,
					data->draw_data->matching_inputs[matching_index].capacity_inputs[index].field_name
				);
				ECS_ASSERT_FORMAT(deep_field.field_index != -1, "Critical error - invalid reflection state. Missing deep field for type {#}", type->name);
				Reflection::SetReflectionTypeInstanceBuffer(
					deep_field, 
					component_data, 
					*data->draw_data->matching_inputs[matching_index].capacity_inputs[index].input, 
					&set_buffer_options
				);
			}
		}
	}

	// Now for link types also update the asset handle values - if they have changed
	Stream<char> ecs_component_name = component_name;
	if (target.size > 0) {
		unsigned int linked_index = data->draw_data->FindLinkComponent(component_name);
		ECS_ASSERT(linked_index != -1);
		data->draw_data->UIUpdateLinkComponent(editor_state, data->sandbox_index, linked_index, data->apply_modifier);

		ecs_component_name = target;
	}

	NotifySandboxEntityComponentChange(editor_state, sandbox_index, entity, ecs_component_name);

	// Re-render the sandbox - for the scene and the game as well
	RenderSandbox(data->editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
	RenderSandbox(data->editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
	system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
}

// ----------------------------------------------------------------------------------------------------------------------------

// There can be at max 3 buttons - the debug draw one is optional
#define HEADER_BUTTON_COUNT 3

// There must be HEADER_BUTTON_COUNT of buttons. Stack memory should be at least ECS_KB in size
static void InspectorEntityHeaderConstructButtons(
	InspectorDrawEntityData* draw_data,
	UIDrawer* drawer, 
	EditorState* editor_state,
	unsigned int sandbox_index,
	Stream<char> component_name,
	UIConfigCollapsingHeaderButton* header_buttons,
	void* stack_memory,
	bool* debug_draw_enabled
) {
	EnableDebugDrawCallbackData* enable_data = (EnableDebugDrawCallbackData*)stack_memory;
	if (debug_draw_enabled != nullptr) {
		enable_data->editor_state = editor_state;
		enable_data->component_name = component_name;
		enable_data->sandbox_index = sandbox_index;

		stack_memory = OffsetPointer(stack_memory, sizeof(*enable_data));
	}

	ResetComponentCallbackData* reset_data = (ResetComponentCallbackData*)stack_memory;
	reset_data->component_name = component_name;
	reset_data->sandbox_index = sandbox_index;
	reset_data->editor_state = editor_state;
	reset_data->draw_data = draw_data;

	stack_memory = OffsetPointer(stack_memory, sizeof(*reset_data));
	RemoveComponentCallbackData* remove_data = (RemoveComponentCallbackData*)stack_memory;
	remove_data->component_name = component_name;
	remove_data->draw_data = draw_data;
	remove_data->sandbox_index = sandbox_index;
	remove_data->editor_state = editor_state;

	size_t current_index = 0;
	if (debug_draw_enabled != nullptr) {
		// The enable button
		header_buttons[current_index].alignment = ECS_UI_ALIGN_RIGHT;
		header_buttons[current_index].type = ECS_UI_COLLAPSING_HEADER_BUTTON_CHECK_BOX;
		header_buttons[current_index].data.check_box_flag = debug_draw_enabled;

		header_buttons[current_index].handler = { EnableDebugDrawCallback, enable_data, sizeof(*enable_data) };
		current_index++;
	}

	// The reset button
	header_buttons[current_index].alignment = ECS_UI_ALIGN_RIGHT;
	header_buttons[current_index].type = ECS_UI_COLLAPSING_HEADER_BUTTON_IMAGE_BUTTON;
	header_buttons[current_index].data.image_texture = ECS_TOOLS_UI_TEXTURE_COG;
	header_buttons[current_index].data.image_color = drawer->color_theme.text;

	header_buttons[current_index].handler = { ResetComponentCallback, reset_data, sizeof(*reset_data) };
	current_index++;

	// The X, delete component button
	header_buttons[current_index].alignment = ECS_UI_ALIGN_RIGHT;
	header_buttons[current_index].type = ECS_UI_COLLAPSING_HEADER_BUTTON_IMAGE_BUTTON;
	header_buttons[current_index].data.image_texture = ECS_TOOLS_UI_TEXTURE_X;
	header_buttons[current_index].data.image_color = drawer->color_theme.text;

	header_buttons[current_index].handler = { RemoveComponentCallback, remove_data, sizeof(*remove_data) };
	current_index++;
}

// ----------------------------------------------------------------------------------------------------------------------------

struct DrawComponentsBaseInfo {
	EditorState* editor_state;
	unsigned int sandbox_index;
	InspectorDrawEntityData* data;
	UIDrawer* drawer;
	EntityManager* entity_manager;
	Stream<char> base_entity_name;
	UIDrawConfig* config;
};

// The GetData functor receives a size_t index to retrieve the data corresponding to that component
// From the signature
template<typename GetData>
static void DrawComponents(
	const DrawComponentsBaseInfo* base_info,
	ComponentSignature signature, 
	ECS_COMPONENT_TYPE component_type, 
	unsigned int header_state_offset,
	Stream<EditorSandbox::LockedEntityComponent> locked_components,
	bool locked_global_component,
	GetData&& get_current_data
) {
	// Forward the member fields
	EditorState* editor_state = base_info->editor_state;
	unsigned int sandbox_index = base_info->sandbox_index;
	InspectorDrawEntityData* data = base_info->data;
	UIDrawer* drawer = base_info->drawer;
	EntityManager* entity_manager = base_info->entity_manager;
	Stream<char> base_entity_name = base_info->base_entity_name;
	UIDrawConfig* config = base_info->config;

	ECS_STACK_CAPACITY_STREAM(char, instance_name, 512);
	UIReflectionDrawConfig ui_draw_configs[(unsigned int)UIReflectionElement::Count];
	memset(ui_draw_configs, 0, sizeof(ui_draw_configs));

	ECS_STACK_CAPACITY_STREAM(char, ui_draw_configs_stack_memory, ECS_KB);

	InspectorComponentCallbackData change_component_data;
	change_component_data.editor_state = editor_state;
	change_component_data.sandbox_index = sandbox_index;
	change_component_data.draw_data = data;
	change_component_data.apply_modifier = false;
	UIActionHandler modify_value_handler = { InspectorComponentCallback, &change_component_data, sizeof(change_component_data) };
	size_t written_configs = UIReflectionDrawConfigSplatCallback(
		{ ui_draw_configs, std::size(ui_draw_configs) },
		modify_value_handler,
		ECS_UI_REFLECTION_DRAW_CONFIG_SPLAT_ALL,
		ui_draw_configs_stack_memory.buffer,
		false
	);

	Stream<UIReflectionDrawConfig> valid_ui_draw_configs = { ui_draw_configs, written_configs };

	for (size_t index = 0; index < signature.count; index++) {
		// Verify that the instance exists because it might have been destroyed in the meantime
		instance_name.size = 0;
		Stream<char> current_component_name = editor_state->editor_components.ComponentFromID(signature[index].value, component_type);
		ECS_ASSERT(current_component_name.size > 0);
		Stream<char> original_component_name = current_component_name;

		// Check to see if the component has a link to it
		Stream<char> link_component = editor_state->editor_components.GetLinkComponentForTarget(current_component_name);
		void* current_component = nullptr;
		if (link_component.size > 0) {
			// Replace the name with the link name
			current_component_name = link_component;
			unsigned int link_index = data->FindLinkComponent(link_component);
			// The component is initialized down bellow
			if (link_index != -1) {
				current_component = data->link_components[link_index].data;
			}
		}
		else {
			// Get the data from the entity manager if not a link component
			current_component = get_current_data(index);
		}

		// We need to firstly check that the component is not omitted from the UI
		const Reflection::ReflectionManager* global_reflection_manager = editor_state->GlobalReflectionManager();
		const Reflection::ReflectionType* component_reflection_type = global_reflection_manager->GetType(current_component_name);
		if (!IsUIReflectionTypeOmitted(component_reflection_type)) {
			change_component_data.component_name = current_component_name;
			Stream<char> base_instance_name = data->is_global_component ? current_component_name : base_entity_name;
			InspectorComponentUIIInstanceName(current_component_name, base_instance_name, sandbox_index, instance_name);

			// Check to see whether or not the component is from the engine side or from the module side
			UIReflectionDrawer* ui_drawer = editor_state->module_reflection;
			UIReflectionType* type = editor_state->ui_reflection->GetType(current_component_name);
			if (type != nullptr) {
				// Engine side
				ui_drawer = editor_state->ui_reflection;
			}
			// Else module side

			UIReflectionInstance* instance = ui_drawer->GetInstance(instance_name);

			auto set_instance_inputs = [&]() {
				// Check to see if it has any buffers and bind their allocators
				AllocatorPolymorphic component_allocator = entity_manager->GetComponentAllocatorFromType(signature[index], component_type);
				if (component_allocator.allocator != nullptr) {
					unsigned int exists_component = data->FindMatchingInput(current_component_name);
					if (exists_component != -1) {
						data->ClearComponent(editor_state, exists_component);
					}
					else {
						// The link component needs to be initialized before this call
						if (link_component.size == 0) {
							data->AddComponent(editor_state, current_component_name);
						}
					}

					unsigned int capacity_stream_index = data->FindMatchingInput(current_component_name);

					ui_drawer->AssignInstanceResizableAllocator(instance, component_allocator, false);
					// For every text input, directory input or file input, allocate a separate buffer
					// such that we don't use the component allocator to hold that data
					const UIReflectionType* ui_type = ui_drawer->GetType(instance->type_name);

					ECS_STACK_CAPACITY_STREAM(Reflection::ReflectionNestedFieldIndex, input_indices, 64);
					ui_drawer->GetTypeMatchingFields(ui_type, UIReflectionElement::TextInput, input_indices);
					unsigned int text_input_count = input_indices.size;
					ui_drawer->GetTypeMatchingFields(ui_type, UIReflectionElement::DirectoryInput, input_indices);
					unsigned int directory_input_count = input_indices.size - text_input_count;
					ui_drawer->GetTypeMatchingFields(ui_type, UIReflectionElement::FileInput, input_indices);
					unsigned int file_input_count = input_indices.size - text_input_count - directory_input_count;
					data->SetComponentInputCount(editor_state, current_component_name, input_indices.size);

					auto bind_text_input = [&](unsigned int index, CapacityStream<void>* capacity_stream) {
						ui_drawer->BindInstanceTextInput(instance, { &index, 1 }, (CapacityStream<char>*)capacity_stream);
					};

					auto bind_directory_input = [&](unsigned int index, CapacityStream<void>* capacity_stream) {
						ui_drawer->BindInstanceDirectoryInput(instance, { &index, 1 }, (CapacityStream<wchar_t>*)capacity_stream);
					};

					auto bind_file_input = [&](unsigned int index, CapacityStream<void>* capacity_stream) {
						ui_drawer->BindInstanceFileInput(instance, { &index, 1 }, (CapacityStream<wchar_t>*)capacity_stream);
					};

					auto create_input_stream = [&](unsigned int count, unsigned int base_offset, size_t element_size, auto bind_function) {
						ECS_STACK_CAPACITY_STREAM(char, nested_field_name, 512);
						for (unsigned int index = 0; index < count; index++) {
							nested_field_name.size = 0;
							Stream<char> nested_name = ui_drawer->GetTypeFieldNestedName(ui_type, input_indices[index + base_offset], nested_field_name);

							CapacityStream<void>* capacity_stream = data->AddComponentInput(editor_state, current_component_name, nested_name, 256, element_size);
							CapacityStream<void> current_stream = ui_drawer->GetInputStreamNested(instance, input_indices[index + base_offset]);

							unsigned int copy_size = current_stream.size <= capacity_stream->capacity ? current_stream.size : capacity_stream->capacity;
							if (copy_size > 0) {
								memcpy(capacity_stream->buffer, current_stream.buffer, current_stream.size * element_size);
								capacity_stream->size = copy_size;
							}
							bind_function(index + base_offset, capacity_stream);
						}
					};

					create_input_stream(text_input_count, 0, sizeof(char), bind_text_input);
					create_input_stream(directory_input_count, text_input_count, sizeof(wchar_t), bind_directory_input);
					create_input_stream(file_input_count, text_input_count + directory_input_count, sizeof(wchar_t), bind_file_input);
				}
			};

			if (instance == nullptr) {
				instance = ui_drawer->CreateInstance(instance_name, current_component_name);
				// Check to see if the created instance already exists. It might happen that if the
				// component is updated, the instance will become invalidated but it won't be removed
				// from the created instance stream resulting in a duplicate
				unsigned int created_instance_index = data->FindCreatedInstance(instance_name);
				if (created_instance_index == -1) {
					AllocatorPolymorphic component_allocator = entity_manager->GetComponentAllocatorFromType(signature[index], component_type);
					created_instance_index = data->AddCreatedInstance(editor_state, instance_name, current_component, component_allocator.allocator);
				}
				else {
					// If it exists, only set the pointer to the same value
					data->created_instances[created_instance_index].pointer_bound = current_component;
				}

				if (link_component.size > 0) {
					// Check to see if it already exists
					unsigned int link_index = data->FindLinkComponent(link_component);

					if (link_index == -1) {
						link_index = data->AddLinkComponent(editor_state, link_component);
						// Update the created instance bound pointer to the newly created one
						if (data->created_instances[created_instance_index].pointer_bound == nullptr) {
							data->created_instances[created_instance_index].pointer_bound = data->link_components[link_index].data;
						}
					}
					else {
						data->ClearLinkComponentSkipAssets(editor_state, link_index);
					}
					current_component = data->link_components[link_index].data;

					bool success = false;
					if (!data->is_global_component) {
						success = ConvertSandboxTargetToLinkComponent(
							editor_state,
							sandbox_index,
							link_component,
							data->entity,
							current_component,
							data->link_components[link_index].previous_data,
							data->link_components[link_index].target_data_copy
						);
					}
					else {
						success = ConvertEditorTargetToLinkComponent(
							editor_state,
							link_component,
							entity_manager->GetGlobalComponent(data->global_component),
							current_component,
							data->link_components[link_index].target_data_copy,
							data->link_components[link_index].previous_data
						);
					}
					if (!success) {
						if (!data->is_global_component) {
							ECS_STACK_CAPACITY_STREAM(char, entity_name_storage, 512);
							Stream<char> entity_name = GetSandboxEntityName(editor_state, sandbox_index, data->entity, entity_name_storage);

							ECS_FORMAT_TEMP_STRING(error_message, "Failed to convert target to link component {#} for entity {#} in sandbox {#}.", link_component, entity_name, sandbox_index);
							EditorSetConsoleError(error_message);
						}
						else {
							ECS_FORMAT_TEMP_STRING(error_message, "Failed to convert global component {#} to link component in sandbox {#}.",
								entity_manager->GetGlobalComponentName(data->global_component), sandbox_index);
							EditorSetConsoleError(error_message);
						}
					}
					else {
						const Reflection::ReflectionType* link_type = editor_state->editor_components.GetType(link_component);
						Reflection::CopyReflectionDataOptions copy_options;
						copy_options.allocator = editor_state->EditorAllocator();
						copy_options.always_allocate_for_buffers = true;
						copy_options.deallocate_existing_buffers = true;
						Reflection::CopyReflectionTypeInstance(
							editor_state->editor_components.internal_manager,
							link_type,
							current_component,
							data->link_components[link_index].previous_data,
							&copy_options
						);
					}
				}
				// There is a special case here. If this is a shared component without any link, we must
				// Bind the pointers to the allocated pointer, not the component one
				if (data->IsSharedComponentAndNoLink(editor_state, current_component_name)) {
					ui_drawer->BindInstancePtrs(instance, data->created_instances[created_instance_index].pointer_bound);
				}
				else {
					ui_drawer->BindInstancePtrs(instance, current_component);
				}
				if (link_component.size > 0) {
					// We need a different modify value handler since we need to
					// Not increment the reference count of the asset in runtime mode when it is already loaded
					auto modify_asset_value = [](ActionData* action_data) {
						UI_UNPACK_ACTION_DATA;

						const InspectorComponentCallbackData* inspector_data = (const InspectorComponentCallbackData*)_data;
						const AssetOverrideCallbackAdditionalInfo* callback_info = (const AssetOverrideCallbackAdditionalInfo*)_additional_data;

						EditorState* editor_state = inspector_data->editor_state;
						unsigned int sandbox_index = inspector_data->sandbox_index;

						EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, sandbox_index);
						if (IsSandboxStateRuntime(sandbox_state)) {
							unsigned int handle = callback_info->handle;
							ECS_ASSET_TYPE type = callback_info->type;

							// Get the reference count of the asset
							unsigned int reference_count = GetAssetReferenceCount(editor_state, handle, type);
							if (callback_info->is_selection) {
								// If the asset is loaded for the first time, we shouldn't do anyting
								if (reference_count > 1) {
									// We need to reduce the count by one
									DecrementAssetReference(editor_state, handle, type, sandbox_index);
								}
							}
							else {
								// Here, where the asset reference count is decremented, we need to always increase the count by one
								IncrementAssetReferenceInSandbox(editor_state, handle, type, sandbox_index);
							}
						}
						else {
							// If selection, we need to unregister the previous asset
							if (callback_info->is_selection) {
								UnregisterSandboxAsset(editor_state, sandbox_index, callback_info->previous_handle, callback_info->type);
							}
						}

						InspectorComponentCallback(action_data);
						// We also need to decrement the counter in order to signal that the asset load is finished
						unsigned int linked_index = inspector_data->draw_data->FindLinkComponent(inspector_data->component_name);
						ECS_ASSERT(linked_index != -1);
						inspector_data->draw_data->link_components[linked_index].is_ui_change_triggered--;
					};

					struct RegisterAssetHandlerChangeData {
						const EditorState* editor_state;
						InspectorDrawEntityData* draw_data;
					};
					auto register_asset_handler_change = [](ActionData* action_data) {
						UI_UNPACK_ACTION_DATA;

						RegisterAssetHandlerChangeData* data = (RegisterAssetHandlerChangeData*)_data;
						AssetOverrideCallbackRegistrationAdditionalInfo* additional_data = (AssetOverrideCallbackRegistrationAdditionalInfo*)_additional_data;
						// To determine which asset handle has been changed, use the handle pointer from the additional info
						unsigned int link_index = data->draw_data->FindLinkComponentFromPointer(data->editor_state, additional_data->handle);
						ECS_ASSERT(link_index != -1);
						data->draw_data->link_components[link_index].is_ui_change_triggered++;
					};

					RegisterAssetHandlerChangeData register_handler_data = { editor_state, data };
					UIActionHandler modify_asset_handler = modify_value_handler;
					modify_asset_handler.action = modify_asset_value;
					// We must do this after the pointer have been bound
					AssetOverrideBindInstanceOverrides(
						ui_drawer, 
						instance, 
						sandbox_index, 
						modify_asset_handler, 
						{ register_asset_handler_change, &register_handler_data, sizeof(register_handler_data) }, 
						true
					);
				}
				set_instance_inputs();
			}

			bool link_needs_apply_modifier = false;

			if (link_component.size > 0) {
				link_needs_apply_modifier = NeedsApplyModifierLinkComponent(editor_state, link_component);
			}

			unsigned int instance_index = data->FindCreatedInstance(instance->name);
			ECS_ASSERT(instance_index != -1);
			bool is_shared_no_link = data->IsSharedComponentAndNoLink(editor_state, current_component_name);
			// If it is shared no link, we don't need to rebind since we use a wrapper component
			// That modifies the actual shared instances using the callback
			if (!is_shared_no_link && data->created_instances[instance_index].pointer_bound != current_component) {
				data->created_instances[instance_index].pointer_bound = current_component;
				ui_drawer->RebindInstancePtrs(instance, current_component);
				set_instance_inputs();
			}

			bool has_debug_draw = ExistsSandboxDebugDrawComponentFunction(editor_state, sandbox_index, signature[index], component_type);
			size_t header_count = has_debug_draw ? HEADER_BUTTON_COUNT : HEADER_BUTTON_COUNT - 1;
			bool* debug_draw_state = data->is_debug_draw_enabled + header_state_offset + index;

			ECS_STACK_VOID_STREAM(button_stack_allocation, ECS_KB);
			UIConfigCollapsingHeaderButton header_buttons[HEADER_BUTTON_COUNT];
			InspectorEntityHeaderConstructButtons(
				data,
				drawer,
				editor_state,
				sandbox_index,
				current_component_name,
				header_buttons,
				button_stack_allocation.buffer,
				has_debug_draw ? debug_draw_state : nullptr
			);

			UIDrawConfig collapsing_config;
			UIConfigCollapsingHeaderButtons config_buttons;
			config_buttons.buttons = { header_buttons, header_count };
			if (data->is_global_component) {
				// Don't add the remove button for global components
				config_buttons.buttons.size--;
			}
			collapsing_config.AddFlag(config_buttons);

			Stream<char> component_name_to_display = instance->name;
			if (link_component.size > 0) {
				// Try to display it without the _Link if it has one
				component_name_to_display = GetReflectionTypeLinkNameBase(component_name_to_display);
			}

			ECS_STACK_CAPACITY_STREAM(UIReflectionDrawInstanceFieldTagOption, field_tag_options, 32);
			UIConfigActiveState inactive_state;
			inactive_state.state = false;
			if (link_needs_apply_modifier) {
				// The first field disables the fields which are not modifiers
				field_tag_options[0].disable_additional_configs = true;
				field_tag_options[0].exclude_match = true;
				field_tag_options[0].tag = STRING(ECS_LINK_MODIFIER_FIELD);
				field_tag_options[0].draw_config.config_count = 0;
				UIReflectionDrawConfigAddConfig(&field_tag_options[0].draw_config, &inactive_state);

				// The second field adds the callback to set the boolean apply modifier for the change data
				field_tag_options[1].tag = STRING(ECS_LINK_MODIFIER_FIELD);
				auto matched_field_callback = [](UIReflectionDrawInstanceFieldTagCallbackData* data) {
					InspectorComponentCallbackData* callback_data = (InspectorComponentCallbackData*)data->user_data;
					callback_data->apply_modifier = true;
				};
				field_tag_options[1].callback = matched_field_callback;
				field_tag_options[1].callback_data = &change_component_data;

				field_tag_options.size = 2;
				field_tag_options.AssertCapacity();
			}

			// Check to see if the component is locked
			UIConfigActiveState component_active_state;
			component_active_state.state = true;
			if (component_type == ECS_COMPONENT_GLOBAL) {
				component_active_state.state = !locked_global_component;
			}
			else {
				bool is_shared = component_type == ECS_COMPONENT_SHARED;
				for (size_t locked_index = 0; locked_index < locked_components.size; locked_index++) {
					if (locked_components[locked_index].component == signature[index] && locked_components[locked_index].is_shared == is_shared) {
						component_active_state.state = false;
						break;
					}
				}
			}
			collapsing_config.AddFlag(component_active_state);

			drawer->CollapsingHeader(
				UI_CONFIG_COLLAPSING_HEADER_BUTTONS | UI_CONFIG_ACTIVE_STATE, 
				collapsing_config, 
				component_name_to_display, 
				data->header_state + index + header_state_offset, 
				[&]() {
				UIReflectionDrawInstanceOptions options;
				options.drawer = drawer;
				options.config = config;
				options.global_configuration = UI_CONFIG_NAME_PADDING | UI_CONFIG_ELEMENT_NAME_FIRST | UI_CONFIG_WINDOW_DEPENDENT_SIZE;
				options.additional_configs = valid_ui_draw_configs;
				options.field_tag_options = field_tag_options;
				ui_drawer->DrawInstance(instance, &options);
			});

			collapsing_config.flag_count--;
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

static void DrawAddComponentMenu(
	EditorState* editor_state,
	unsigned int sandbox_index,
	InspectorDrawEntityData* data, 
	UIDrawer* drawer,
	ComponentSignature unique_signature,
	SharedComponentSignature shared_signature
) {
	struct DrawComponentMenuFunctor {
		// Returns true if the component should appear in the menu
		bool IsUniqueEnabled(Component component) const {
			return unique_components.Find(component) == UCHAR_MAX;
		}

		// Returns true if the component should appear in the menu
		bool IsSharedEnabled(Component component) const {
			return shared_components.Find(component) == UCHAR_MAX;
		}

		// Fills the data for a click handler
		void Fill(void* callback_data_untyped, Stream<char> component_name) {
			AddComponentCallbackData* callback_data = (AddComponentCallbackData*)callback_data_untyped;
			*callback_data = { data, editor_state, sandbox_index, component_name };
		}

		// Return true if the unique entries should be made unavailable
		bool LimitUnique() const {
			return unique_components.count == ECS_ARCHETYPE_MAX_COMPONENTS;
		}

		// Return true if the shared entries should be made unavailable
		bool LimitShared() const {
			return shared_components.count == ECS_ARCHETYPE_MAX_SHARED_COMPONENTS;
		}

		EditorState* editor_state;
		InspectorDrawEntityData* data;
		unsigned int sandbox_index;
		ComponentSignature unique_components;
		SharedComponentSignature shared_components;
	};
	DrawComponentMenuFunctor draw_menu_functor = { editor_state, data, sandbox_index, unique_signature, shared_signature };

	UIDrawConfig config;
	UIConfigAlignElement align_element;
	align_element.horizontal = ECS_UI_ALIGN_MIDDLE;
	config.AddFlag(align_element);

	DrawChooseComponentMenu(
		editor_state,
		*drawer,
		UI_CONFIG_ALIGN_ELEMENT,
		config,
		"Add Component",
		{ AddComponentCallback, nullptr, sizeof(AddComponentCallbackData), ECS_UI_DRAW_NORMAL },
		draw_menu_functor
	);
}

// ----------------------------------------------------------------------------------------------------------------------------

void InspectorDrawEntity(EditorState* editor_state, unsigned int inspector_index, void* _data, UIDrawer* drawer) {
	unsigned int sandbox_index = GetInspectorTargetSandbox(editor_state, inspector_index);
	InspectorDrawEntityData* data = (InspectorDrawEntityData*)_data;

	float previous_row_y_offset = drawer->layout.next_row_y_offset;
	drawer->SetNextRowYOffset(previous_row_y_offset * 0.75f);
	auto restore_previous_row_offset = StackScope([previous_row_y_offset, drawer]() {
		drawer->SetNextRowYOffset(previous_row_y_offset);
	});

	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	EDITOR_SANDBOX_VIEWPORT viewport_source = GetSandboxActiveViewport(editor_state, sandbox_index);
	EDITOR_SANDBOX_VIEWPORT last_data_source = data->last_frames_data_source[ECS_COUNTOF(data->last_frames_data_source) - 1];
	if (viewport_source != last_data_source) {
		// Destroy all the instances that were created
		// When transitioning from runtime to scene, we have to invalidate
		// All buffers that the components might have because the runtime world
		// Was cleared already
		if (viewport_source == EDITOR_SANDBOX_VIEWPORT_SCENE && last_data_source == EDITOR_SANDBOX_VIEWPORT_RUNTIME) {
			// Invalidate the buffers
			for (size_t index = 0; index < data->created_instances.size; index++) {
				UIReflectionDrawer* ui_drawer = editor_state->module_reflection;
				UIReflectionInstance* instance = ui_drawer->GetInstance(data->created_instances[index].name);
				if (instance == nullptr) {
					ui_drawer = editor_state->ui_reflection;
					instance = ui_drawer->GetInstance(data->created_instances[index].name);
					ECS_ASSERT(instance != nullptr);
				}

				ui_drawer->InvalidateStandaloneInstanceInputs(instance);
			}
		}

		size_t link_component_count = data->link_components.size;
		for (size_t index = 0; index < link_component_count; index++) {
			data->RemoveLinkComponent(editor_state, 0);
		}
		size_t matching_input_count = data->matching_inputs.size;
		for (size_t index = 0; index < matching_input_count; index++) {
			data->RemoveComponent(editor_state, data->matching_inputs[0].component_name);
		}
		size_t created_instance_count = data->created_instances.size;
		for (size_t index = 0; index < created_instance_count; index++) {
			data->RemoveCreatedInstance(editor_state, 0);
		}

		data->link_components = {};
		data->matching_inputs = {};
		data->created_instances = {};
		data->allocator.Clear();
	}
	for (size_t index = 0; index < ECS_COUNTOF(data->last_frames_data_source) - 1; index++) {
		data->last_frames_data_source[index] = data->last_frames_data_source[index + 1];
	}
	data->last_frames_data_source[ECS_COUNTOF(data->last_frames_data_source) - 1] = viewport_source;

	// Check to see if the entity or global component still exists - else revert to draw nothing
	if (!data->is_global_component) {
		if (!entity_manager->ExistsEntity(data->entity)) {
			ChangeInspectorToNothing(editor_state, inspector_index);
			return;
		}
	}
	else {
		if (entity_manager->TryGetGlobalComponent(data->global_component) == nullptr) {
			ChangeInspectorToNothing(editor_state, inspector_index);
			return;
		}
	}

	// For each created instance, verify that its instance still exists - it can happen that the reflection
	// type was removed and we haven't picked up on that yet
	for (unsigned int index = 0; index < data->created_instances.size; index++) {
		if (!data->IsCreatedInstanceValid(editor_state, index)) {
			Stream<char> component_name = data->CreatedInstanceComponentName(index);
			unsigned int link_index = data->FindLinkComponent(component_name);
			// If it is a link component, remove it as such, else as a normal component
			if (link_index != -1) {
				data->RemoveLinkComponent(editor_state, component_name);
			}
			else {
				data->RemoveComponent(editor_state, component_name);
			}
			index--;
		}
	}

	if (!data->is_global_component) {
		// Before drawing, we need to remove all components which have been removed from the entity but are still being stored here
		// This needs to be done only for the entity case
		data->UpdateStoredComponents(editor_state, sandbox_index);
	}

	// We should update the component allocators before actually drawing
	data->UpdateComponentAllocators(editor_state, sandbox_index);

	// Before drawing the components, go through the link components and deallocate the apply modifiers data
	data->UpdateLinkComponentsApplyModifier(editor_state);

	// Perform the update from target to link for normal components
	data->UpdateLinkComponentsFromTargets(editor_state, sandbox_index);

	Color icon_color = drawer->color_theme.theme;
	icon_color = RGBToHSV(icon_color);
	icon_color.value = ClampMin(icon_color.value, (unsigned char)200);
	icon_color = HSVToRGB(icon_color);
	InspectorIcon(drawer, ECS_TOOLS_UI_TEXTURE_FILE_MESH, icon_color);

	UIDrawConfig config;

	ECS_STACK_CAPACITY_STREAM(char, base_entity_name, 256);
	if (!data->is_global_component) {
		Component name_component = editor_state->editor_components.GetComponentID(STRING(Name));
		Name* name = (Name*)entity_manager->TryGetComponent(data->entity, name_component);

		data->entity.ToString(base_entity_name, true);

		if (name != nullptr) {
			struct CallbackData {
				EditorState* editor_state;
				unsigned int sandbox_index;
				InspectorDrawEntityData* data;
			};

			auto name_input_callback = [](ActionData* action_data) {
				UI_UNPACK_ACTION_DATA;

				CallbackData* data = (CallbackData*)_data;
				ChangeSandboxEntityName(data->editor_state, data->sandbox_index, data->data->entity, data->data->name_input);
			};

			CallbackData callback_data;
			callback_data.editor_state = editor_state;
			callback_data.sandbox_index = sandbox_index;
			callback_data.data = data;
			UIConfigTextInputCallback input_callback = { { name_input_callback, &callback_data, sizeof(callback_data) } };
			config.AddFlag(input_callback);

			UIConfigWindowDependentSize transform;
			transform.scale_factor.x = drawer->GetWindowSizeScaleUntilBorder().x;
			config.AddFlag(transform);

			// Cannot use a constant name because it will be transported to other entities when changing to them
			// without changing the buffer being pointed
			UIDrawerTextInput* text_input = drawer->TextInput(
				UI_CONFIG_WINDOW_DEPENDENT_SIZE | UI_CONFIG_TEXT_INPUT_NO_NAME | UI_CONFIG_TEXT_INPUT_CALLBACK | UI_CONFIG_ALIGN_TO_ROW_Y,
				config,
				base_entity_name,
				&data->name_input
			);
			if (!text_input->is_currently_selected) {
				if (name->name != *text_input->text) {
					text_input->text->CopyOther(name->name);
				}
			}
			drawer->NextRow();

			UIConfigAlignElement align_element;
			align_element.horizontal = ECS_UI_ALIGN_MIDDLE;
			config.AddFlag(align_element);
			drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y | UI_CONFIG_ALIGN_ELEMENT, config, base_entity_name);
			config.flag_count = 0;
		}
		else {
			drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, base_entity_name);
		}
	}
	else {
		ECS_STACK_CAPACITY_STREAM(char, name_storage, 512);
		Stream<char> component_name = InspectorTargetName(data, editor_state, sandbox_index, name_storage);
		drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, component_name);
	}

	drawer->NextRow();

	ComponentSignature unique_signature;
	SharedComponentSignature shared_signature;
	if (!data->is_global_component) {
		unique_signature = entity_manager->GetEntitySignatureStable(data->entity);
		shared_signature = entity_manager->GetEntitySharedSignatureStable(data->entity);

		// If we have a prefab component, we should display the information about it
		unsigned char prefab_idx = unique_signature.Find(PrefabComponent::ID());
		if (prefab_idx != UCHAR_MAX) {
			drawer->CrossLine();

			drawer->current_row_y_scale = drawer->GetSquareScale().y;
			PrefabComponent* prefab = entity_manager->GetComponent<PrefabComponent>(data->entity);
			//if (ExistsPrefabID(editor_state, prefab->id)) {
				Stream<wchar_t> prefab_path = GetPrefabPath(editor_state, prefab->id);
				ECS_FORMAT_TEMP_STRING(prefab_string, "Prefab: {#}", prefab_path);
				drawer->Text(UI_CONFIG_ALIGN_TO_ROW_Y, config, prefab_string);

				struct DetachEntityCallbackData {
					EditorState* editor_state;
					unsigned int index;
				};

				auto detach_entity_callback = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;
					DetachEntityCallbackData* data = (DetachEntityCallbackData*)_data;
					SetSandboxSceneDirty(data->editor_state, data->index);
				};

				DetachEntityCallbackData detach_data = { editor_state, sandbox_index };
				UIConfigCheckBoxCallback detach_check_box_callback;
				detach_check_box_callback.handler = { detach_entity_callback, &detach_data, sizeof(detach_data) };
				config.AddFlag(detach_check_box_callback);

				UIConfigCheckBoxDefault detach_default;
				detach_default.value = true;
				config.AddFlag(detach_default);

				UIConfigAlignElement align_check_box;
				align_check_box.horizontal = ECS_UI_ALIGN_RIGHT;
				config.AddFlag(align_check_box);
				drawer->CheckBox(UI_CONFIG_CHECK_BOX_CALLBACK | UI_CONFIG_CHECK_BOX_DEFAULT | UI_CONFIG_ALIGN_ELEMENT, config, "Detached", &prefab->detached);
				drawer->NextRow();

				UIDrawerRowLayout row_layout = drawer->GenerateRowLayout();
				row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });
				row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });
				size_t button_configuration = 0;
				row_layout.GetTransform(config, button_configuration);

				OpenPrefabActionData open_prefab_data;
				open_prefab_data.editor_state = editor_state;
				open_prefab_data.inspector_index = inspector_index;
				open_prefab_data.launching_sandbox = sandbox_index;
				open_prefab_data.prefab_id = prefab->id;
				drawer->Button(
					button_configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X,
					config,
					"Edit",
					{ OpenPrefabAction, &open_prefab_data, sizeof(open_prefab_data), ECS_UI_DRAW_SYSTEM }
				);

				button_configuration = 0;
				config.flag_count = 0;
				row_layout.GetTransform(config, button_configuration);
				struct HighlightPrefabPathData {
					EditorState* editor_state;
					unsigned int prefab_id;
				};
				auto highlight_prefab_path = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					HighlightPrefabPathData* data = (HighlightPrefabPathData*)_data;
					ECS_STACK_CAPACITY_STREAM(wchar_t, prefab_absolute_path_storage, 512);
					Stream<wchar_t> prefab_absolute_path = GetPrefabAbsolutePath(data->editor_state, data->prefab_id, prefab_absolute_path_storage);
					ChangeFileExplorerFile(data->editor_state, prefab_absolute_path);
					system->SetActiveWindow(FILE_EXPLORER_WINDOW_NAME);
				};
				HighlightPrefabPathData highlight_prefab_data = { editor_state, prefab->id };
				drawer->Button(
					button_configuration | UI_CONFIG_LABEL_DO_NOT_GET_TEXT_SCALE_X,
					config,
					"Highlight",
					{ highlight_prefab_path, &highlight_prefab_data, sizeof(highlight_prefab_data) }
				);
				drawer->NextRow();

				drawer->CrossLine();
				config.flag_count = 0;
			//}
		}
	}

	UIConfigNamePadding name_padding;
	name_padding.alignment = ECS_UI_ALIGN_LEFT;
	name_padding.total_length = 0.12f;
	config.AddFlag(name_padding);

	UIConfigWindowDependentSize window_dependent_size;
	config.AddFlag(window_dependent_size);

	auto get_unique_data = [&](size_t index) {
		return entity_manager->GetComponentWithIndex(data->entity, index);
	};

	auto get_shared_data = [&](size_t index) {
		return entity_manager->GetSharedData(shared_signature.indices[index], shared_signature.instances[index]);
	};

	auto get_global_data = [&](size_t index) {
		return entity_manager->GetGlobalComponent(data->global_component);
	};

	DrawComponentsBaseInfo draw_base_info;
	draw_base_info.editor_state = editor_state;
	draw_base_info.sandbox_index = sandbox_index;
	draw_base_info.data = data;
	draw_base_info.drawer = drawer;
	draw_base_info.entity_manager = entity_manager;
	draw_base_info.base_entity_name = base_entity_name;
	draw_base_info.config = &config;

	ECS_STACK_CAPACITY_STREAM(EditorSandbox::LockedEntityComponent, locked_components, ECS_ARCHETYPE_MAX_COMPONENTS + ECS_ARCHETYPE_MAX_SHARED_COMPONENTS);
	bool is_global_component_locked = false;

	// Retrieve the locked global and entity components for this entity such that we can disable
	// The UI when those are indeeed locked
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	sandbox->locked_components_lock.Lock();

	if (!data->is_global_component) {
		for (unsigned int index = 0; index < sandbox->locked_entity_components.size; index++) {
			if (sandbox->locked_entity_components[index].entity == data->entity) {
				locked_components.Add(sandbox->locked_entity_components[index]);
			}
		}
	}
	else {
		// Check only the global components
		for (unsigned int index = 0; index < sandbox->locked_global_components.size; index++) {
			if (sandbox->locked_global_components[index] == data->global_component) {
				is_global_component_locked = true;
				break;
			}
		}
	}

	sandbox->locked_components_lock.Unlock();
	
	// Now draw the entity using the reflection drawer
	if (!data->is_global_component) {
		DrawComponents(
			&draw_base_info,
			unique_signature, 
			ECS_COMPONENT_UNIQUE,
			0,
			locked_components,
			is_global_component_locked,
			get_unique_data
		);
		DrawComponents(
			&draw_base_info,
			{ shared_signature.indices, shared_signature.count }, 
			ECS_COMPONENT_SHARED, 
			unique_signature.count,
			locked_components,
			is_global_component_locked,
			get_shared_data
		);
	}
	else {
		DrawComponents(
			&draw_base_info,
			{ &data->global_component, 1 }, 
			ECS_COMPONENT_GLOBAL, 
			0,
			locked_components,
			is_global_component_locked,
			get_global_data
		);
	}

	config.flag_count = 0;

	if (!data->is_global_component) {
		DrawAddComponentMenu(
			editor_state,
			sandbox_index,
			data,
			drawer,
			unique_signature,
			shared_signature
		);
	}
}

// ----------------------------------------------------------------------------------------------------------------------------

template<typename CompareFunctor, typename SetInfoFunctor>
static void ChangeInspectorToEntityOrGlobalComponentImpl(
	EditorState* editor_state,
	unsigned int sandbox_index,
	unsigned int inspector_index,
	CompareFunctor&& compare_functor,
	SetInfoFunctor&& set_info_functor
) {
	// Look to see if there is already an inspector with that entity and highlight it instead
	if (inspector_index != -1) {
		if (editor_state->inspector_manager.data[inspector_index].draw_function == InspectorDrawEntity) {
			InspectorDrawEntityData* draw_data = (InspectorDrawEntityData*)GetInspectorDrawFunctionData(editor_state, inspector_index);
			if (compare_functor(inspector_index, draw_data)) {
				// Don't do anything if it is the same entity, just highlight it
				ECS_STACK_CAPACITY_STREAM(char, inspector_name, 64);
				GetInspectorName(inspector_index, inspector_name);
				editor_state->ui_system->SetActiveWindow(inspector_name);
				return;
			}
		}
	}
	else {
		ECS_STACK_CAPACITY_STREAM(unsigned int, inspector_entity_target, MAX_INSPECTOR_WINDOWS);
		FindInspectorWithDrawFunction(editor_state, InspectorDrawEntity, &inspector_entity_target, sandbox_index);
		for (unsigned int index = 0; index < inspector_entity_target.size; index++) {
			InspectorDrawEntityData* draw_data = (InspectorDrawEntityData*)GetInspectorDrawFunctionData(editor_state, inspector_entity_target[index]);
			if (compare_functor(inspector_entity_target[index], draw_data)) {
				// Highlight this inspector
				ECS_STACK_CAPACITY_STREAM(char, inspector_name, 64);
				GetInspectorName(inspector_entity_target[index], inspector_name);
				editor_state->ui_system->SetActiveWindowInBorder(inspector_name);
				return;
			}
		}
	}

	ECS_STACK_VOID_STREAM(_draw_data, ECS_KB * 4);
	InspectorDrawEntityData* draw_data = _draw_data.Reserve<InspectorDrawEntityData>();
	set_info_functor(draw_data);
	draw_data->name_input.buffer = nullptr;
	draw_data->name_input.size = 0;
	draw_data->name_input.capacity = INSPECTOR_DRAW_ENTITY_NAME_INPUT_CAPACITY;
	draw_data->matching_inputs.size = 0;
	draw_data->created_instances.size = 0;
	draw_data->link_components.size = 0;
	draw_data->is_initialized = false;
	memset(draw_data->last_frames_data_source, EDITOR_SANDBOX_VIEWPORT_COUNT, sizeof(draw_data->last_frames_data_source));

	memset(draw_data->header_state, 1, sizeof(bool) * (ECS_ARCHETYPE_MAX_COMPONENTS + ECS_ARCHETYPE_MAX_SHARED_COMPONENTS));

	DetermineDebugDrawStates(editor_state, sandbox_index, draw_data);

	inspector_index = ChangeInspectorDrawFunction(
		editor_state,
		inspector_index,
		{ InspectorDrawEntity, InspectorCleanEntity },
		draw_data,
		sizeof(*draw_data) + INSPECTOR_DRAW_ENTITY_NAME_INPUT_CAPACITY,
		sandbox_index
	);
	if (inspector_index != -1) {
		SetLastInspectorTargetInitialize(editor_state, inspector_index, [](EditorState* editor_state, void* _data, unsigned int inspector_index) {
			InspectorDrawEntityData* draw_data = (InspectorDrawEntityData*)_data;
			draw_data->name_input.buffer = (char*)OffsetPointer(draw_data, sizeof(*draw_data));
			draw_data->allocator = MemoryManager(ECS_KB * 48, ECS_KB, ECS_KB * 256, editor_state->EditorAllocator());
		});
	}
}

void ChangeInspectorToEntity(EditorState* editor_state, unsigned int sandbox_index, Entity entity, unsigned int inspector_index)
{
	ChangeInspectorToEntityOrGlobalComponentImpl(editor_state, sandbox_index, inspector_index,
		[=](unsigned int inspector_index, const InspectorDrawEntityData* draw_data) {
			return sandbox_index == GetInspectorTargetSandbox(editor_state, inspector_index) && 
				!draw_data->is_global_component && draw_data->entity == entity;
		},
		[=](InspectorDrawEntityData* draw_data) {
			draw_data->is_global_component = false;
			draw_data->entity = entity;
		});
}

// ----------------------------------------------------------------------------------------------------------------------------

void ChangeInspectorToGlobalComponent(EditorState* editor_state, unsigned int sandbox_index, Component component, unsigned int inspector_index) {
	ChangeInspectorToEntityOrGlobalComponentImpl(editor_state, sandbox_index, inspector_index,
		[=](unsigned int inspector_index, const InspectorDrawEntityData* draw_data) {
			return sandbox_index == GetInspectorTargetSandbox(editor_state, inspector_index) &&
				draw_data->is_global_component && draw_data->global_component == component;
		},
		[=](InspectorDrawEntityData* draw_data) {
			draw_data->is_global_component = true;
			draw_data->global_component = component;
		});
}

// ----------------------------------------------------------------------------------------------------------------------------

bool IsInspectorDrawEntity(const EditorState* editor_state, unsigned int inspector_index, bool include_global_components)
{
	InspectorDrawFunction draw_function = GetInspectorDrawFunction(editor_state, inspector_index);
	if (draw_function == InspectorDrawEntity) {
		if (include_global_components) {
			return true;
		}
		else {
			const InspectorDrawEntityData* draw_data = (const InspectorDrawEntityData*)GetInspectorDrawFunctionData(editor_state, inspector_index);
			return !draw_data->is_global_component;
		}
	}
	return false;
}

// ----------------------------------------------------------------------------------------------------------------------------