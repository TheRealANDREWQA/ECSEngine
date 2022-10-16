#include "ecspch.h"
#include "Module.h"
#include "../../Utilities/Function.h"
#include "../../Internal/World.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../../Utilities/FunctionInterfaces.h"


namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------

	bool IsModule(Stream<wchar_t> path)
	{
		auto module = LoadModule(path);
		if (module.code == ECS_GET_MODULE_OK) {
			ReleaseModule(&module);
			return true;
		}
		return false;
	}

	// -----------------------------------------------------------------------------------------------------------

	Module LoadModule(Stream<wchar_t> path)
	{
		Module module;
		memset(&module, 0, sizeof(module));

		ECS_TEMP_STRING(library_path, 256);
		if (path[path.size] != L'\0') {
			library_path.Copy(path);
			library_path[library_path.size] = L'\0';
		}
		else {
			library_path = path;
		}

		HMODULE module_handle = LoadLibrary(library_path.buffer);

		if (module_handle == nullptr) {
			module.code = ECS_GET_MODULE_FAULTY_PATH;
			return module;
		}

		module.task_function = (ModuleTaskFunction)GetProcAddress(module_handle, STRING(ModuleTaskFunction));
		if (module.task_function == nullptr) {
			FreeLibrary(module_handle);
			module.code = ECS_GET_MODULE_FUNCTION_MISSING;
			return module;
		}

		// Now the optional functions
		module.build_functions = (ModuleBuildFunctions)GetProcAddress(module_handle, STRING(ModuleBuildFunctions));
		module.ui_function = (ModuleUIFunction)GetProcAddress(module_handle, STRING(ModuleUIFunction));
		module.link_components = (ModuleRegisterLinkComponentFunction)GetProcAddress(module_handle, STRING(ModuleRegisterLinkComponentFunction));
		module.serialize_function = (ModuleSerializeComponentFunction)GetProcAddress(module_handle, STRING(ModuleSerializeComponentFunction));
		module.set_world = (ModuleSetCurrentWorld)GetProcAddress(module_handle, STRING(ModuleSetCurrentWorld));

		module.code = ECS_GET_MODULE_OK;
		module.os_module_handle = module_handle;

		return module;
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<TaskSchedulerElement> LoadModuleTasks(const Module* module, AllocatorPolymorphic allocator)
	{
		const size_t STACK_MEMORY_CAPACITY = ECS_KB * 64;
		void* stack_allocation = ECS_STACK_ALLOC(STACK_MEMORY_CAPACITY);
		LinearAllocator temp_allocator(stack_allocation, STACK_MEMORY_CAPACITY);
		AllocatorPolymorphic poly_allocator = GetAllocatorPolymorphic(&temp_allocator);

		ECS_STACK_CAPACITY_STREAM(TaskSchedulerElement, schedule_tasks, 128);

		ModuleTaskFunctionData task_data;
		task_data.tasks = &schedule_tasks;
		task_data.allocator = poly_allocator;

		module->task_function(&task_data);
		schedule_tasks.AssertCapacity();

		return StreamDeepCopy(schedule_tasks, allocator);	
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<Tools::UIWindowDescriptor> LoadModuleUIDescriptors(const Module* module, AllocatorPolymorphic allocator)
	{
		if (!module->ui_function) {
			return { nullptr, 0 };
		}

		const size_t MAX_DESCRIPTORS = 32;
		ECS_STACK_CAPACITY_STREAM(Tools::UIWindowDescriptor, window_descriptors, MAX_DESCRIPTORS);

		size_t LINEAR_ALLOCATOR_SIZE = ECS_KB * 64;
		void* stack_allocation = ECS_STACK_ALLOC(LINEAR_ALLOCATOR_SIZE);
		LinearAllocator stack_allocator(stack_allocation, LINEAR_ALLOCATOR_SIZE);

		ModuleUIFunctionData function_data;
		function_data.window_descriptors = &window_descriptors;
		function_data.allocator = GetAllocatorPolymorphic(&stack_allocator);
		module->ui_function(&function_data);
		window_descriptors.AssertCapacity();

		size_t total_memory = sizeof(Tools::UIWindowDescriptor) * window_descriptors.size;
		total_memory += stack_allocator.m_top;

		Tools::UIWindowDescriptor* descriptors = (Tools::UIWindowDescriptor*)AllocateEx(allocator, total_memory);
		void* buffer = function::OffsetPointer(descriptors, sizeof(Tools::UIWindowDescriptor) * window_descriptors.size);
		// Copy all the data allocated from the allocator
		memcpy(buffer, stack_allocation, stack_allocator.m_top);

		// The names must be remapped, alongside any window data
		window_descriptors.CopyTo(descriptors);
		for (size_t index = 0; index < window_descriptors.size; index++) {
			// Change the name
			descriptors[index].window_name.buffer = (char*)function::RemapPointerIfInRange(stack_allocation, LINEAR_ALLOCATOR_SIZE, buffer, descriptors[index].window_name.buffer);

			// Change the window data, if any
			if (window_descriptors[index].window_data_size > 0) {
				descriptors[index].window_data = function::RemapPointerIfInRange(stack_allocation, LINEAR_ALLOCATOR_SIZE, buffer, descriptors[index].window_data);
			}

			// Change the private window data, if any
			if (window_descriptors[index].private_action_data_size > 0) {
				descriptors[index].private_action_data = function::RemapPointerIfInRange(stack_allocation, LINEAR_ALLOCATOR_SIZE, buffer, descriptors[index].private_action_data);
			}

			// Change the destroy window data, if any
			if (window_descriptors[index].destroy_action_data_size > 0) {
				descriptors[index].destroy_action_data = function::RemapPointerIfInRange(stack_allocation, LINEAR_ALLOCATOR_SIZE, buffer, descriptors[index].destroy_action_data);
			}
		}

		return { descriptors, window_descriptors.size };
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<ModuleBuildAssetType> LoadModuleBuildAssetTypes(const Module* module, AllocatorPolymorphic allocator)
	{
		if (!module->build_functions) {
			return { nullptr, 0 };
		}

		const size_t MAX_TYPES = 32;
		ECS_STACK_CAPACITY_STREAM(ModuleBuildAssetType, asset_types, MAX_TYPES);
		// This memory is used by the function for the dependency types
		const size_t EXTRA_MEMORY_SIZE = ECS_KB * 32;
		char _extra_memory[EXTRA_MEMORY_SIZE];
		LinearAllocator extra_memory(_extra_memory, EXTRA_MEMORY_SIZE);

		ModuleBuildFunctionsData function_data;
		function_data.asset_types = &asset_types;
		function_data.allocator = GetAllocatorPolymorphic(&extra_memory);
		module->build_functions(&function_data);
		asset_types.AssertCapacity();

		Stream<ModuleBuildAssetType> asset_type_stream(asset_types);
		ValidateModuleBuildAssetTypes(asset_type_stream);

		size_t total_memory = sizeof(ModuleBuildAssetType) * asset_types.size + extra_memory.m_top;

		ModuleBuildAssetType* types = (ModuleBuildAssetType*)AllocateEx(allocator, total_memory);
		asset_type_stream.CopyTo(types);
		void* buffer = function::OffsetPointer(types, sizeof(ModuleBuildAssetType) * asset_type_stream.size);
		// Memcpy all the data from the allocator
		memcpy(buffer, _extra_memory, extra_memory.m_top);

		// Remapp all the pointers into this space
		for (size_t index = 0; index < asset_type_stream.size; index++) {
			// The extension
			types[index].extension.buffer = (wchar_t*)function::RemapPointerIfInRange(_extra_memory, EXTRA_MEMORY_SIZE, buffer, types[index].extension.buffer);

			// The dependencies
			if (types[index].dependencies.size > 0) {
				types[index].dependencies.buffer = (ECS_MODULE_BUILD_DEPENDENCY*)function::RemapPointerIfInRange(_extra_memory, EXTRA_MEMORY_SIZE, buffer, types[index].dependencies.buffer);
			}
			else {
				types[index].dependencies = { nullptr, 0 };
			}

			// The editor name
			if (types[index].asset_editor_name.size > 0) {
				types[index].asset_editor_name.buffer = (char*)function::RemapPointerIfInRange(_extra_memory, EXTRA_MEMORY_SIZE, buffer, types[index].asset_editor_name.buffer);
			}

			// The metadata name
			if (types[index].asset_metadata_name.size > 0) {
				types[index].asset_metadata_name.buffer = (char*)function::RemapPointerIfInRange(_extra_memory, EXTRA_MEMORY_SIZE, buffer, types[index].asset_metadata_name.buffer);
			}
		}

		return { types, asset_type_stream.size };
	}

	// -----------------------------------------------------------------------------------------------------------

	ModuleSerializeComponentStreams LoadModuleSerializeComponentFunctors(const Module* module, AllocatorPolymorphic allocator)
	{
		if (!module->serialize_function) {
			return {};
		}

		const size_t MAX_COMPONENTS = 32;
		ECS_STACK_CAPACITY_STREAM(SerializeEntityManagerComponentInfo, serialize_components, MAX_COMPONENTS);
		ECS_STACK_CAPACITY_STREAM(DeserializeEntityManagerComponentInfo, deserialize_components, MAX_COMPONENTS);
		ECS_STACK_CAPACITY_STREAM(SerializeEntityManagerSharedComponentInfo, serialize_shared_components, MAX_COMPONENTS);
		ECS_STACK_CAPACITY_STREAM(DeserializeEntityManagerSharedComponentInfo, deserialize_shared_components, MAX_COMPONENTS);

		const size_t TEMP_MEMORY_SIZE = ECS_KB * 8;
		char _temp_memory[TEMP_MEMORY_SIZE];
		LinearAllocator temp_memory(_temp_memory, TEMP_MEMORY_SIZE);

		ModuleSerializeComponentFunctionData function_data;
		function_data.serialize_components = &serialize_components;
		function_data.deserialize_components = &deserialize_components;
		function_data.serialize_shared_components = &serialize_shared_components;
		function_data.deserialize_shared_components = &deserialize_shared_components;
		function_data.allocator = GetAllocatorPolymorphic(&temp_memory);
		module->serialize_function(&function_data);

		serialize_components.AssertCapacity();
		deserialize_components.AssertCapacity();
		serialize_shared_components.AssertCapacity();
		deserialize_shared_components.AssertCapacity();
		ECS_ASSERT(serialize_components.size == deserialize_components.size);
		ECS_ASSERT(serialize_shared_components.size == deserialize_shared_components.size);

		// Make a single coallesced allocation
		size_t total_memory = StreamDeepCopySize(serialize_components) + StreamDeepCopySize(deserialize_components) + StreamDeepCopySize(serialize_shared_components)
			+ StreamDeepCopySize(deserialize_shared_components);

		ModuleSerializeComponentStreams streams;
		void* allocation = AllocateEx(allocator, total_memory);
		uintptr_t buffer = (uintptr_t)allocation;

		streams.serialize_components = StreamDeepCopy(serialize_components, buffer);
		streams.deserialize_components = StreamDeepCopy(deserialize_components, buffer);
		streams.serialize_shared_components = StreamDeepCopy(serialize_shared_components, buffer);
		streams.deserialize_shared_components = StreamDeepCopy(deserialize_shared_components, buffer);

		if (streams.serialize_components.size == 0) {
			streams.serialize_components.buffer = nullptr;
		}

		if (streams.deserialize_components.size == 0) {
			streams.deserialize_components.buffer = nullptr;
		}

		if (streams.serialize_shared_components.size == 0) {
			streams.serialize_shared_components.buffer = nullptr;
		}

		if (streams.deserialize_shared_components.size == 0) {
			streams.deserialize_shared_components.buffer = nullptr;
		}

		return streams;
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<ModuleLinkComponentTarget> LoadModuleLinkComponentTargets(const Module* module, AllocatorPolymorphic allocator)
	{
		if (!module->link_components) {
			return { nullptr, 0 };
		}

		const size_t MAX_LINKS = 32;
		ECS_STACK_CAPACITY_STREAM(ModuleLinkComponentTarget, link_components, MAX_LINKS);

		const size_t MAX_COMPONENT_NAME_SIZE = ECS_KB;
		ECS_STACK_CAPACITY_STREAM(char, link_component_names, MAX_COMPONENT_NAME_SIZE);
		LinearAllocator temp_allocator(link_component_names.buffer, MAX_COMPONENT_NAME_SIZE);

		ModuleRegisterLinkComponentFunctionData function_data;
		function_data.functions = &link_components;
		function_data.allocator = GetAllocatorPolymorphic(&temp_allocator);
		module->link_components(&function_data);

		link_components.AssertCapacity();
		Stream<ModuleLinkComponentTarget> valid_links(link_components);
		ValidateModuleLinkComponentTargets(valid_links);

		// Calculate the total byte size
		size_t total_memory = sizeof(ModuleLinkComponentTarget) * valid_links.size + temp_allocator.m_top;
		ModuleLinkComponentTarget* targets = (ModuleLinkComponentTarget*)AllocateEx(allocator, total_memory);
		valid_links.CopyTo(targets);
		void* buffer = function::OffsetPointer(targets, sizeof(ModuleLinkComponentTarget) * valid_links.size);
		memcpy(buffer, temp_allocator.m_buffer, temp_allocator.m_top);

		for (size_t index = 0; index < valid_links.size; index++) {
			targets[index].component_name.buffer = (char*)function::RemapPointerIfInRange(temp_allocator.m_buffer, MAX_COMPONENT_NAME_SIZE, buffer, targets[index].component_name.buffer);
		}

		return { targets, valid_links.size };
	}

	// -----------------------------------------------------------------------------------------------------------

	void ReleaseModule(Module* module) {
		BOOL success = FreeLibrary(module->os_module_handle);
		module->code = ECS_GET_MODULE_FAULTY_PATH;
	}

	// -----------------------------------------------------------------------------------------------------------

	void ValidateModuleBuildAssetTypes(Stream<ModuleBuildAssetType>& build_types)
	{
		// Validate that the names are properly provided, as well the extension and the functions
		for (int32_t index = 0; index < (int32_t)build_types.size; index++) {
			if (build_types[index].extension.buffer == nullptr || build_types[index].extension.size == 0) {
				build_types.Swap(index, build_types.size - 1);
				index--;
				continue;
			}

			if (build_types[index].build_function == nullptr) {
				build_types.Swap(index, build_types.size - 1);
				index--;
				continue;
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------

	void ValidateModuleLinkComponentTargets(Stream<ModuleLinkComponentTarget>& link_targets)
	{
		// Validate that the names are properly provided, aswell as the extension and the functions
		for (int32_t index = 0; index < (int32_t)link_targets.size; index++) {
			if (link_targets[index].component_name.buffer == nullptr || link_targets[index].component_name.size == 0) {
				link_targets.Swap(index, link_targets.size - 1);
				index--;
				continue;
			}

			if (link_targets[index].build_function == nullptr || link_targets[index].reverse_function == nullptr) {
				link_targets.Swap(index, link_targets.size - 1);
				index--;
				continue;
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------

	// -----------------------------------------------------------------------------------------------------------
}