#include "ecspch.h"
#include "Module.h"
#include "../../ECS/World.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../../Utilities/Path.h"
#include "../UI/UIStructures.h"
#include "../../OS/DLL.h"
#include "../../Multithreading/TaskScheduler.h"
#include "../../Utilities/Crash.h"

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------

	bool FindModule(Stream<wchar_t> path)
	{
		auto module = LoadModule(path);
		if (module.code == ECS_GET_MODULE_OK) {
			ReleaseModule(&module);
			return true;
		}
		return false;
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<Stream<char>> GetModuleDLLDependencies(Stream<wchar_t> path, AllocatorPolymorphic allocator, bool omit_system_dlls)
	{
		Stream<Stream<char>> result = { nullptr, 0 };
		ECS_STACK_CAPACITY_STREAM(Stream<char>, dependencies, 512);

		// Read the file as binary, but interpret it as text
		Stream<void> file = ReadWholeFileBinary(path);
		if (file.buffer != nullptr) {
			Stream<char> text = { file.buffer, file.size };
			Stream<char> token = ECS_MODULE_EXTENSION_ASCII;

			Stream<char> last_dll = text;
			Stream<char> dll = FindFirstToken(text, token);
			while (dll.size > 0) {
				Stream<char> dll_name = SkipUntilCharacterReverse(dll.buffer + token.size - 1, last_dll.buffer, '\0');
				if (FindString(dll_name, dependencies) == -1) {
					dependencies.AddAssert(dll_name);
				}

				dll.Advance(token.size);
				last_dll = dll;
				dll = FindFirstToken(dll, token);
			}

			Stream<wchar_t> filename = PathFilename(path);
			ECS_STACK_CAPACITY_STREAM(char, char_filename, 512);
			ConvertWideCharsToASCII(filename, char_filename);
			unsigned int self_reference = FindString(char_filename, dependencies);
			if (self_reference != -1) {
				dependencies.RemoveSwapBack(self_reference);
			}

			static Stream<char> system_dlls[] = {
				"KERNEL32.dll",
				"VCRUNTIME140.dll",
				"api-ms-win-crt-runtime-l1-1-0.dll",
			};

			if (omit_system_dlls) {
				for (unsigned int index = 0; index < std::size(system_dlls); index++) {
					unsigned int dependency_index = FindString(system_dlls[index], dependencies);
					if (dependency_index != -1) {
						dependencies.RemoveSwapBack(dependency_index);
					}
				}
			}

			free(file.buffer);
			if (dependencies.size > 0) {
				result = StreamCoalescedDeepCopy(dependencies, allocator);
			}
		}
		return result;
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<Stream<char>> GetModuleDLLDependenciesFromVCX(Stream<wchar_t> path, AllocatorPolymorphic allocator, bool omit_system_dlls)
	{
		Stream<Stream<char>> result = { nullptr, 0 };
		ECS_STACK_CAPACITY_STREAM(Stream<char>, dependencies, 512);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
		// Read the file as binary, but interpret it as text
		Stream<void> file = ReadWholeFileBinary(path, (&stack_allocator));
		if (file.buffer != nullptr) {
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);

			Stream<char> text = { file.buffer, file.size };
			Stream<char> token = "<AdditionalDependencies>";
			Stream<char> end_token = "</AdditionalDependencies>";

			Stream<char> text_token = FindFirstToken(text, token);
			if (text_token.size > 0) {
				text_token.Advance(token.size);
				Stream<char> text_end_token = FindFirstToken(text_token, end_token);
				if (text_end_token.size > 0) {
					char semicolon = ';';
					Stream<char> parse_range;
					parse_range.buffer = text_token.buffer;
					parse_range.size = text_end_token.buffer - text_token.buffer;
					Stream<char> current_lib_end = FindFirstCharacter(parse_range, semicolon);
					while (current_lib_end.size > 0) {
						Stream<char> current_range = { parse_range.buffer, PointerDifference(current_lib_end.buffer, parse_range.buffer) / sizeof(char) };
						dependencies.Add(current_range);
						size_t advance_count = current_lib_end.buffer - parse_range.buffer + 1;
						parse_range.buffer = current_lib_end.buffer + 1;
						parse_range.size -= advance_count;
						current_lib_end = FindFirstCharacter(parse_range, semicolon);
					}

					static Stream<char> system_dlls[] = {
						"kernel32.lib",
						"user32.lib",
						"gdi32.lib",
						"winspool.lib",
						"comdlg32.lib",
						"advapi32.lib",
						"shell32.lib",
						"ole32.lib",
						"oleaut32.lib",
						"uuid.lib",
						"odbc32.lib",
						"odbccp32.lib"
					};

					if (omit_system_dlls) {
						for (unsigned int index = 0; index < std::size(system_dlls); index++) {
							unsigned int dependency_index = FindString(system_dlls[index], dependencies);
							if (dependency_index != -1) {
								dependencies.RemoveSwapBack(dependency_index);
							}
						}
					}
				}
			}

			if (dependencies.size > 0) {
				for (unsigned int index = 0; index < dependencies.size; index++) {
					dependencies[index] = PathFilenameBoth(dependencies[index]);
					Stream<char> extension = FindFirstCharacter(dependencies[index], '.');
					// Some variable dlls might not have an extension (e.g. $(ECSEngineLibDebug))
					if (extension.size > 0) {
						// Change the extension to .dll
						extension[1] = 'd';
						extension[2] = 'l';
						extension[3] = 'l';
					}
				}
				result = StreamCoalescedDeepCopy(dependencies, allocator);
			}
		}

		return result;
	}

	// -----------------------------------------------------------------------------------------------------------

	void* GetModuleHandleFromPath(Stream<char> module_path)
	{
		NULL_TERMINATE(module_path);
		return GetModuleHandleA(module_path.buffer);
	}

	// -----------------------------------------------------------------------------------------------------------

	void* GetModuleHandleFromPath(Stream<wchar_t> module_path)
	{
		NULL_TERMINATE_WIDE(module_path);
		return GetModuleHandleW(module_path.buffer);
	}

	// -----------------------------------------------------------------------------------------------------------

	Module LoadModule(Stream<wchar_t> path, bool* load_debugging_symbols)
	{
		Module module;
		memset(&module, 0, sizeof(module));

		void* module_handle = OS::LoadDLL(path);

		if (module_handle == nullptr) {
			DWORD error = GetLastError();
			module.code = ECS_GET_MODULE_FAULTY_PATH;
			return module;
		}

		module.task_function = (ModuleTaskFunction)OS::GetDLLSymbol(module_handle, STRING(ModuleTaskFunction));
		if (module.task_function == nullptr) {
			OS::UnloadDLL(module_handle);
			module.code = ECS_GET_MODULE_FUNCTION_MISSING;
			return module;
		}

		// Now the optional functions
		module.build_functions = (ModuleBuildFunctions)OS::GetDLLSymbol(module_handle, STRING(ModuleBuildFunctions));
		module.ui_function = (ModuleUIFunction)OS::GetDLLSymbol(module_handle, STRING(ModuleUIFunction));
		module.link_components = (ModuleRegisterLinkComponentFunction)OS::GetDLLSymbol(module_handle, STRING(ModuleRegisterLinkComponentFunction));
		module.serialize_function = (ModuleSerializeComponentFunction)OS::GetDLLSymbol(module_handle, STRING(ModuleSerializeComponentFunction));
		module.set_world = (ModuleSetCurrentWorld)OS::GetDLLSymbol(module_handle, STRING(ModuleSetCurrentWorld));
		module.extra_information = (ModuleRegisterExtraInformationFunction)OS::GetDLLSymbol(module_handle, STRING(ModuleRegisterExtraInformationFunction));
		module.debug_draw = (ModuleRegisterDebugDrawFunction)OS::GetDLLSymbol(module_handle, STRING(ModuleRegisterDebugDrawFunction));
		module.debug_draw_tasks = (ModuleRegisterDebugDrawTaskElementsFunction)OS::GetDLLSymbol(module_handle, STRING(ModuleRegisterDebugDrawTaskElementsFunction));
		module.component_functions = (ModuleRegisterComponentFunctionsFunction)OS::GetDLLSymbol(module_handle, STRING(ModuleRegisterComponentFunctionsFunction));

		module.code = ECS_GET_MODULE_OK;
		module.os_module_handle = module_handle;

		if (load_debugging_symbols != nullptr) {
			*load_debugging_symbols = OS::LoadDLLSymbols(path, module_handle);
		}

		return module;
	}

	// -----------------------------------------------------------------------------------------------------------

	void LoadAppliedModule(AppliedModule* module, AllocatorPolymorphic allocator, CapacityStream<char>* error_message)
	{
		module->build_asset_types = LoadModuleBuildAssetTypes(&module->base_module, allocator, error_message);
		module->link_components = LoadModuleLinkComponentTargets(&module->base_module, allocator, error_message);
		module->tasks = LoadModuleTasks(&module->base_module, allocator, error_message);
		module->serialize_streams = LoadModuleSerializeComponentFunctors(&module->base_module, allocator);
		module->ui_descriptors = LoadModuleUIDescriptors(&module->base_module, allocator);
		module->extra_information = LoadModuleExtraInformation(&module->base_module, allocator);
		module->debug_draw_elements = LoadModuleDebugDrawElements(&module->base_module, allocator);
		module->debug_draw_task_elements = LoadModuleDebugDrawTaskElements(&module->base_module, allocator, error_message);
		module->component_functions = LoadModuleComponentFunctions(&module->base_module, allocator);
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<TaskSchedulerElement> LoadModuleTasks(const Module* module, AllocatorPolymorphic allocator, CapacityStream<char>* error_message)
	{
		const size_t STACK_MEMORY_CAPACITY = ECS_KB * 64;
		void* stack_allocation = ECS_STACK_ALLOC(STACK_MEMORY_CAPACITY);
		LinearAllocator temp_allocator(stack_allocation, STACK_MEMORY_CAPACITY);
		AllocatorPolymorphic poly_allocator = &temp_allocator;

		ECS_STACK_CAPACITY_STREAM(TaskSchedulerElement, schedule_tasks, 128);

		ModuleTaskFunctionData task_data;
		task_data.tasks = &schedule_tasks;
		task_data.allocator = poly_allocator;
		task_data.maintain_order_in_group = false;

		module->task_function(&task_data);
		schedule_tasks.AssertCapacity();

		Stream<TaskSchedulerElement> tasks = schedule_tasks;
		ValidateModuleTasks(tasks, error_message);

		if (task_data.maintain_order_in_group) {
			// Add dependencies to the same group elements
			ECS_STACK_CAPACITY_STREAM(unsigned int, same_group_elements, 128);
			for (size_t group_index = 0; group_index < ECS_THREAD_TASK_GROUP_COUNT; group_index++) {
				same_group_elements.size = 0;
				for (unsigned int index = 0; index < schedule_tasks.size; index++) {
					if (schedule_tasks[index].task_group == group_index) {
						same_group_elements.AddAssert(index);
					}
				}

				for (unsigned int index = 1; index < same_group_elements.size; index++) {
					TaskDependency dependency = { schedule_tasks[same_group_elements[index - 1]].task_name };
					schedule_tasks[same_group_elements[index]].task_dependencies.AddResize(dependency, poly_allocator);
				}
			}
		}

		return StreamCoalescedDeepCopy(tasks, allocator);
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
		function_data.allocator = &stack_allocator;
		module->ui_function(&function_data);
		window_descriptors.AssertCapacity();

		size_t total_memory = sizeof(Tools::UIWindowDescriptor) * window_descriptors.size;
		total_memory += stack_allocator.m_top;

		Tools::UIWindowDescriptor* descriptors = (Tools::UIWindowDescriptor*)AllocateEx(allocator, total_memory);
		void* buffer = OffsetPointer(descriptors, sizeof(Tools::UIWindowDescriptor) * window_descriptors.size);
		// Copy all the data allocated from the allocator
		memcpy(buffer, stack_allocation, stack_allocator.m_top);

		// The names must be remapped, alongside any window data
		window_descriptors.CopyTo(descriptors);
		for (size_t index = 0; index < window_descriptors.size; index++) {
			// Change the name
			descriptors[index].window_name.buffer = (char*)RemapPointerIfInRange(stack_allocation, LINEAR_ALLOCATOR_SIZE, buffer, descriptors[index].window_name.buffer);

			// Change the window data, if any
			if (window_descriptors[index].window_data_size > 0) {
				descriptors[index].window_data = RemapPointerIfInRange(stack_allocation, LINEAR_ALLOCATOR_SIZE, buffer, descriptors[index].window_data);
			}

			// Change the private window data, if any
			if (window_descriptors[index].private_action_data_size > 0) {
				descriptors[index].private_action_data = RemapPointerIfInRange(stack_allocation, LINEAR_ALLOCATOR_SIZE, buffer, descriptors[index].private_action_data);
			}

			// Change the destroy window data, if any
			if (window_descriptors[index].destroy_action_data_size > 0) {
				descriptors[index].destroy_action_data = RemapPointerIfInRange(stack_allocation, LINEAR_ALLOCATOR_SIZE, buffer, descriptors[index].destroy_action_data);
			}
		}

		return { descriptors, window_descriptors.size };
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<ModuleBuildAssetType> LoadModuleBuildAssetTypes(const Module* module, AllocatorPolymorphic allocator, CapacityStream<char>* error_message)
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
		function_data.allocator = &extra_memory;
		module->build_functions(&function_data);
		asset_types.AssertCapacity();

		Stream<ModuleBuildAssetType> asset_type_stream(asset_types);
		ValidateModuleBuildAssetTypes(asset_type_stream, error_message);

		size_t total_memory = sizeof(ModuleBuildAssetType) * asset_types.size + extra_memory.m_top;

		ModuleBuildAssetType* types = (ModuleBuildAssetType*)AllocateEx(allocator, total_memory);
		asset_type_stream.CopyTo(types);
		void* buffer = OffsetPointer(types, sizeof(ModuleBuildAssetType) * asset_type_stream.size);
		// Memcpy all the data from the allocator
		memcpy(buffer, _extra_memory, extra_memory.m_top);

		// Remapp all the pointers into this space
		for (size_t index = 0; index < asset_type_stream.size; index++) {
			// The extension
			types[index].extension.buffer = (wchar_t*)RemapPointerIfInRange(_extra_memory, EXTRA_MEMORY_SIZE, buffer, types[index].extension.buffer);

			// The dependencies
			if (types[index].dependencies.size > 0) {
				types[index].dependencies.buffer = (ECS_MODULE_BUILD_DEPENDENCY*)RemapPointerIfInRange(_extra_memory, EXTRA_MEMORY_SIZE, buffer, types[index].dependencies.buffer);
			}
			else {
				types[index].dependencies = { nullptr, 0 };
			}

			// The editor name
			if (types[index].asset_editor_name.size > 0) {
				types[index].asset_editor_name.buffer = (char*)RemapPointerIfInRange(_extra_memory, EXTRA_MEMORY_SIZE, buffer, types[index].asset_editor_name.buffer);
			}

			// The metadata name
			if (types[index].asset_metadata_name.size > 0) {
				types[index].asset_metadata_name.buffer = (char*)RemapPointerIfInRange(_extra_memory, EXTRA_MEMORY_SIZE, buffer, types[index].asset_metadata_name.buffer);
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

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 8, ECS_MB);

		ModuleSerializeComponentFunctionData function_data;
		function_data.serialize_components = &serialize_components;
		function_data.deserialize_components = &deserialize_components;
		function_data.serialize_shared_components = &serialize_shared_components;
		function_data.deserialize_shared_components = &deserialize_shared_components;
		function_data.allocator = &_stack_allocator;
		module->serialize_function(&function_data);

		serialize_components.AssertCapacity();
		deserialize_components.AssertCapacity();
		serialize_shared_components.AssertCapacity();
		deserialize_shared_components.AssertCapacity();
		ECS_ASSERT(serialize_components.size == deserialize_components.size);
		ECS_ASSERT(serialize_shared_components.size == deserialize_shared_components.size);

		// Make a single coalesced allocation
		size_t total_memory = StreamCoalescedDeepCopySize(serialize_components) + StreamCoalescedDeepCopySize(deserialize_components) + StreamCoalescedDeepCopySize(serialize_shared_components)
			+ StreamCoalescedDeepCopySize(deserialize_shared_components);

		ModuleSerializeComponentStreams streams;
		void* allocation = AllocateEx(allocator, total_memory);
		uintptr_t buffer = (uintptr_t)allocation;

		streams.serialize_components = StreamCoalescedDeepCopy(serialize_components, buffer);
		streams.deserialize_components = StreamCoalescedDeepCopy(deserialize_components, buffer);
		streams.serialize_shared_components = StreamCoalescedDeepCopy(serialize_shared_components, buffer);
		streams.deserialize_shared_components = StreamCoalescedDeepCopy(deserialize_shared_components, buffer);

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

	Stream<ModuleLinkComponentTarget> LoadModuleLinkComponentTargets(
		ModuleRegisterLinkComponentFunction register_function, 
		AllocatorPolymorphic allocator, 
		CapacityStream<char>* error_message
	)
	{
		const size_t MAX_LINKS = 512;
		ECS_STACK_CAPACITY_STREAM(ModuleLinkComponentTarget, link_components, MAX_LINKS);

		const size_t MAX_COMPONENT_NAME_SIZE = ECS_KB * 16;
		ECS_STACK_CAPACITY_STREAM(char, link_component_names, MAX_COMPONENT_NAME_SIZE);
		LinearAllocator temp_allocator(link_component_names.buffer, MAX_COMPONENT_NAME_SIZE);

		ModuleRegisterLinkComponentFunctionData function_data;
		function_data.functions = &link_components;
		function_data.allocator = &temp_allocator;
		register_function(&function_data);

		link_components.AssertCapacity();
		Stream<ModuleLinkComponentTarget> valid_links(link_components);
		ValidateModuleLinkComponentTargets(valid_links, error_message);

		// Calculate the total byte size
		size_t total_memory = sizeof(ModuleLinkComponentTarget) * valid_links.size + temp_allocator.m_top;
		ModuleLinkComponentTarget* targets = (ModuleLinkComponentTarget*)AllocateEx(allocator, total_memory);
		valid_links.CopyTo(targets);
		void* buffer = OffsetPointer(targets, sizeof(ModuleLinkComponentTarget) * valid_links.size);
		memcpy(buffer, temp_allocator.m_buffer, temp_allocator.m_top);

		for (size_t index = 0; index < valid_links.size; index++) {
			targets[index].component_name.buffer = (char*)RemapPointerIfInRange(temp_allocator.m_buffer, MAX_COMPONENT_NAME_SIZE, buffer, targets[index].component_name.buffer);
		}
		return { targets, valid_links.size };
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<ModuleLinkComponentTarget> LoadModuleLinkComponentTargets(const Module* module, AllocatorPolymorphic allocator, CapacityStream<char>* error_message)
	{
		if (!module->link_components) {
			return { nullptr, 0 };
		}

		return LoadModuleLinkComponentTargets(module->link_components, allocator, error_message);
	}

	// -----------------------------------------------------------------------------------------------------------

	ModuleExtraInformation LoadModuleExtraInformation(const Module* module, AllocatorPolymorphic allocator)
	{
		if (!module->extra_information) {
			return { {nullptr, 0} };
		}

		return LoadModuleExtraInformation(module->extra_information, allocator);
	}

	ModuleExtraInformation LoadModuleExtraInformation(ModuleRegisterExtraInformationFunction function, AllocatorPolymorphic allocator)
	{
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 64, ECS_MB);

		ModuleRegisterExtraInformationFunctionData function_data;
		function_data.allocator = &_stack_allocator;
		function_data.extra_information.Initialize(function_data.allocator, 0);
		function(&function_data);

		Stream<ModuleMiscInfo> coalesced_data = StreamCoalescedDeepCopy(function_data.extra_information.ToStream(), allocator);
		return { coalesced_data };
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<ModuleDebugDrawElement> LoadModuleDebugDrawElements(const Module* module, AllocatorPolymorphic allocator)
	{
		if (!module->debug_draw) {
			return {};
		}

		return LoadModuleDebugDrawElements(module->debug_draw, allocator);
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<ModuleDebugDrawElement> LoadModuleDebugDrawElements(ModuleRegisterDebugDrawFunction function, AllocatorPolymorphic allocator)
	{
		ECS_STACK_CAPACITY_STREAM(ModuleDebugDrawElement, debug_draw_elements, ECS_KB * 2);
		ModuleRegisterDebugDrawFunctionData function_data;
		function_data.elements = &debug_draw_elements;
		function(&function_data);

		return debug_draw_elements.Copy(allocator);
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<ModuleDebugDrawTaskElement> LoadModuleDebugDrawTaskElements(
		const Module* module, 
		AllocatorPolymorphic allocator, 
		CapacityStream<char>* error_message
	)
	{
		if (!module->debug_draw_tasks) {
			return {};
		}

		return LoadModuleDebugDrawTaskElements(module->debug_draw_tasks, allocator, error_message);
	}

	Stream<ModuleDebugDrawTaskElement> LoadModuleDebugDrawTaskElements(
		ModuleRegisterDebugDrawTaskElementsFunction function, 
		AllocatorPolymorphic allocator,
		CapacityStream<char>* error_message
	)
	{
		const size_t STACK_MEMORY_CAPACITY = ECS_KB * 64;
		void* stack_allocation = ECS_STACK_ALLOC(STACK_MEMORY_CAPACITY);
		LinearAllocator temp_allocator(stack_allocation, STACK_MEMORY_CAPACITY);
		AllocatorPolymorphic poly_allocator = &temp_allocator;

		ECS_STACK_CAPACITY_STREAM(ModuleDebugDrawTaskElement, elements, 128);

		ModuleRegisterDebugDrawTaskElementsData register_data;
		register_data.elements = &elements;

		function(&register_data);
		elements.AssertCapacity();

		Stream<ModuleDebugDrawTaskElement> stream_elements = elements;
		ValidateModuleDebugDrawTaskElements(stream_elements, error_message);
		return StreamCoalescedDeepCopy(stream_elements, allocator);
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<ModuleComponentFunctions> LoadModuleComponentFunctions(
		const Module* module,
		AllocatorPolymorphic allocator
	) {
		if (!module->component_functions) {
			return {};
		}

		return LoadModuleComponentFunctions(module->component_functions, allocator);
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<ModuleComponentFunctions> LoadModuleComponentFunctions(
		ModuleRegisterComponentFunctionsFunction function,
		AllocatorPolymorphic allocator
	) {
		ECS_STACK_CAPACITY_STREAM(ModuleComponentFunctions, reset_functions, 1024);

		ModuleRegisterComponentFunctionsData register_data;
		register_data.functions = &reset_functions;
		function(&register_data);
		reset_functions.AssertCapacity();

		return StreamCoalescedDeepCopy(reset_functions, allocator);
	}

	// -----------------------------------------------------------------------------------------------------------

	void ReleaseModule(Module* module, bool* unload_debugging_symbols) {
		if (module->code != ECS_GET_MODULE_FAULTY_PATH) {
			ReleaseModuleHandle(module->os_module_handle, unload_debugging_symbols);
		}
		module->code = ECS_GET_MODULE_FAULTY_PATH;
	}

	// -----------------------------------------------------------------------------------------------------------

	void ReleaseModuleHandle(void* handle, bool* unload_debugging_symbols)
	{
		// This must be done before unloading the dll otherwise it will be invalid
		if (unload_debugging_symbols != nullptr) {
			OS::UnloadDLLSymbols(handle);
		}
		OS::UnloadDLL(handle);
	}

	// -----------------------------------------------------------------------------------------------------------

	void ReleaseAppliedModuleStreams(AppliedModule* module, AllocatorPolymorphic allocator)
	{
		if (module->build_asset_types.size > 0) {
			DeallocateIfBelongs(allocator, module->build_asset_types.buffer);
			module->build_asset_types = { nullptr, 0 };
		}

		if (module->link_components.size > 0) {
			DeallocateIfBelongs(allocator, module->link_components.buffer);
			module->link_components = { nullptr, 0 };
		}

		DeallocateIfBelongs(allocator, module->serialize_streams.GetAllocatedBuffer());
		memset(&module->serialize_streams, 0, sizeof(module->serialize_streams));

		if (module->tasks.size > 0) {
			DeallocateIfBelongs(allocator, module->tasks.buffer);
			module->tasks = { nullptr, 0 };
		}

		if (module->ui_descriptors.size > 0) {
			DeallocateIfBelongs(allocator, module->ui_descriptors.buffer);
			module->ui_descriptors = { nullptr, 0 };
		}

		if (module->extra_information.pairs.size > 0) {
			DeallocateIfBelongs(allocator, module->extra_information.pairs.buffer);
			module->extra_information.pairs = { nullptr, 0 };
		}

		if (module->debug_draw_elements.size > 0) {
			DeallocateIfBelongs(allocator, module->debug_draw_elements.buffer);
			module->debug_draw_elements = { nullptr, 0 };
		}

		if (module->debug_draw_task_elements.size > 0) {
			DeallocateIfBelongs(allocator, module->debug_draw_task_elements.buffer);
			module->debug_draw_task_elements = { nullptr, 0 };
		}

		if (module->component_functions.size > 0) {
			DeallocateIfBelongs(allocator, module->component_functions.buffer);
			module->component_functions = { nullptr, 0 };
		}
	}

	// -----------------------------------------------------------------------------------------------------------

	void ReleaseAppliedModule(AppliedModule* module, AllocatorPolymorphic allocator, bool* unload_debugging_symbols)
	{
		ReleaseAppliedModuleStreams(module, allocator);
		ReleaseModule(&module->base_module, unload_debugging_symbols);
	}

	// -----------------------------------------------------------------------------------------------------------

	bool ValidateModuleTask(const TaskSchedulerElement& element, CapacityStream<char>* error_message)
	{
		if (element.task_name.size == 0) {
			if (error_message != nullptr) {
				ECS_STACK_CAPACITY_STREAM(char, group_name, 64);
				TaskGroupToString(element.task_group, group_name);

				if (element.task_dependencies.size > 0) {
					ECS_STACK_CAPACITY_STREAM(char, temp_string, 512);
					StreamToString(element.task_dependencies, temp_string);
					ECS_FORMAT_STRING(*error_message, "Task with dependencies {#} in group {#} has no name specified", temp_string, group_name);
				}
				else {
					ECS_FORMAT_STRING(*error_message, "Task in group {#} has no name specified", group_name);
				}
			}

			return true;
		}
		return false;
	}

	// -----------------------------------------------------------------------------------------------------------

	// The functor must return true when an element is invalid
	template<typename StreamType, typename InvalidateFunctor>
	size_t ValidateModuleInformation(
		StreamType& elements, 
		InvalidateFunctor&& invalidate_functor
	)
	{
		size_t initial_count = elements.size;

		for (int64_t index = 0; index < (int64_t)elements.size; index++) {
			if (invalidate_functor(elements[index])) {
				elements.Swap(index, elements.size - 1);
				index--;
				elements.size--;
				continue;
			}
		}

		return initial_count;
	}

	// -----------------------------------------------------------------------------------------------------------

	size_t ValidateModuleTasks(Stream<TaskSchedulerElement>& tasks, CapacityStream<char>* error_message) 
	{
		return ValidateModuleInformation(tasks, [=](const TaskSchedulerElement& element) {
			return ValidateModuleTask(element, error_message);
		});
	}

	// -----------------------------------------------------------------------------------------------------------

	size_t ValidateModuleBuildAssetTypes(Stream<ModuleBuildAssetType>& build_types, CapacityStream<char>* error_message)
	{
		return ValidateModuleInformation(build_types, [=](const ModuleBuildAssetType& element) {
			if (element.extension.buffer == nullptr || element.extension.size == 0) {
				if (error_message != nullptr) {
					if (element.asset_editor_name.size > 0) {
						ECS_FORMAT_STRING(*error_message, "Build Asset Type {#} is missing the extension", element.asset_editor_name);
					}
					else if (element.asset_metadata_name.size > 0) {
						ECS_FORMAT_STRING(*error_message, "Build Asset Type with metadata {#} is missing the extension", element.asset_metadata_name);
					}
					else {
						error_message->AddStreamSafe("Build Asset Type is missing the extension");
					}
				}

				return true;
			}

			if (element.build_function == nullptr) {
				if (error_message != nullptr) {
					if (element.asset_editor_name.size > 0) {
						ECS_FORMAT_STRING(*error_message, "Build Asset Type {#} is missing the build function", element.asset_editor_name);
					}
					else if (element.asset_metadata_name.size > 0) {
						ECS_FORMAT_STRING(*error_message, "Build Asset Type with metadata {#} is missing the build function", element.asset_metadata_name);
					}
					else {
						error_message->AddStreamSafe("Build Asset Type is missing the build function");
					}
				}

				return true;
			}
			return false;
		});
	}

	// -----------------------------------------------------------------------------------------------------------

	size_t ValidateModuleLinkComponentTargets(Stream<ModuleLinkComponentTarget>& link_targets, CapacityStream<char>* error_message)
	{
		return ValidateModuleInformation(link_targets, [=](const ModuleLinkComponentTarget& element) {
			if (element.component_name.buffer == nullptr || element.component_name.size == 0) {
				if (error_message != nullptr) {
					error_message->AddStreamSafe("A link component is missing the component name");
				}
				return true;
			}

			if (element.build_function == nullptr || element.reverse_function == nullptr) {
				if (error_message != nullptr) {
					ECS_FORMAT_STRING(*error_message, "User defined conversion for link component {#} is missing the build or the reverse function", element.component_name);
				}
				return true;
			}

			return false;
		});
	}

	// -----------------------------------------------------------------------------------------------------------

	size_t ValidateModuleDebugDrawTaskElements(Stream<ModuleDebugDrawTaskElement>& elements, CapacityStream<char>* error_message)
	{
		return ValidateModuleInformation(elements, [=](const ModuleDebugDrawTaskElement& element) {
			return ValidateModuleTask(element.base_element, error_message);
		});
	}

	// -----------------------------------------------------------------------------------------------------------

	// In order to inherit the data of another task, we must do a trick
	// We cannot inherit it directly, we ned to record the name here and
	// Then look it up in the task manager the first time this function is called
	// and keep the index cached. The name characters are immediately after this
	// structure
	struct ModuleDebugDrawWrapperData {
		bool is_enabled;
		InputMappingElement input_element;
		unsigned int inherit_data_name_size;
		ThreadFunction function;
		void* inherit_data;
	};

	static ECS_THREAD_TASK(ModuleDebugDrawWrapper) {
		ModuleDebugDrawWrapperData* data = (ModuleDebugDrawWrapperData*)_data;
		// Check the mapping before that
		if (data->input_element.IsInitialized()) {
			if (data->input_element.IsTriggered(world->mouse, world->keyboard)) {
				data->is_enabled = !data->is_enabled;
			}
		}

		if (data->is_enabled) {
			if (data->inherit_data_name_size > 0) {
				if (data->inherit_data == nullptr) {
					Stream<char> inherit_name = { OffsetPointer(data, sizeof(*data)), data->inherit_data_name_size };
					unsigned int task_index = world->task_manager->FindTask(inherit_name);
					ECS_CRASH_CONDITION(task_index != -1, "Failed to find task {#} to reference data", inherit_name);
					data->inherit_data = world->task_manager->GetTaskPtr(task_index)->data;
				}
			}
			void* function_data = data->inherit_data;
			data->function(thread_id, world, function_data);
		}
	}

	struct ModuleDebugDrawWrapperInitializeData {
		Stream<char> initialize_task_name;
		ThreadFunction run_function;
		ThreadFunction initialize_function;
		InputMappingElement button_element;
	};

	static ECS_THREAD_TASK(ModuleDebugDrawWrapperInitialize) {
		StaticThreadTaskInitializeInfo* initialize_info = (StaticThreadTaskInitializeInfo*)_data;
		ModuleDebugDrawWrapperInitializeData* data = (ModuleDebugDrawWrapperInitializeData*)initialize_info->initialize_data;
		if (data->initialize_function != nullptr) {
			data->initialize_function(thread_id, world, initialize_info);
		}

		ModuleDebugDrawWrapperData* function_data = (ModuleDebugDrawWrapperData*)initialize_info->frame_data->buffer;
		function_data->function = data->run_function;
		function_data->is_enabled = false;
		function_data->inherit_data_name_size = data->initialize_task_name.size;
		function_data->inherit_data = nullptr;
		function_data->input_element = data->button_element;
		unsigned int write_size = sizeof(*function_data);
		if (data->initialize_task_name.size > 0) {
			write_size += data->initialize_task_name.size;
			ECS_ASSERT(write_size <= initialize_info->frame_data->capacity);
			data->initialize_task_name.CopyTo(OffsetPointer(function_data, sizeof(*function_data)));
		}
		initialize_info->frame_data->size = write_size;
	}

	ModuleComponentBuildFunction GetModuleResetFunction(const AppliedModule* applied_module, Stream<char> component_name) {
		size_t index = applied_module->component_functions.Find(component_name, [](ModuleComponentFunctions element) {
			return element.component_name;
		});
		if (index != -1) {
			return applied_module->component_functions[index].reset_function;
		}
		return nullptr;
	}

	void AddModuleDebugDrawTaskElementsToScheduler(TaskScheduler* scheduler, Stream<ModuleDebugDrawTaskElement> elements, bool scene_order)
	{
		for (size_t index = 0; index < elements.size; index++) {
			if (!elements[index].scene_only || scene_order) {
				ModuleDebugDrawWrapperInitializeData initialize_data;
				initialize_data.initialize_function = elements[index].base_element.initialize_task_function;
				initialize_data.initialize_task_name = elements[index].base_element.initialize_data_task_name;
				initialize_data.run_function = elements[index].base_element.task_function;
				initialize_data.button_element = elements[index].input_element;

				TaskSchedulerElement element = elements[index].base_element;
				element.initialize_task_function = ModuleDebugDrawWrapperInitialize;
				element.task_function = ModuleDebugDrawWrapper;
				element.SetInitializeData(&initialize_data, sizeof(initialize_data));
				scheduler->Add(element);
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------

	void RetrieveModuleDebugDrawTaskElementsInitializeData(const TaskScheduler* scheduler, TaskManager* target_manager, const TaskManager* source_manager)
	{
		for (unsigned int index = 0; index < scheduler->elements.size; index++) {
			if (scheduler->elements[index].initialize_data_task_name.size > 0) {
				unsigned int task_index = target_manager->FindTask(scheduler->elements[index].task_name);
				if (task_index != -1) {
					ThreadTask* target_task = target_manager->GetTaskPtr(task_index);
					unsigned int source_index = source_manager->FindTask(scheduler->elements[index].initialize_data_task_name);
					if (source_index != -1) {
						ModuleDebugDrawWrapperData* wrapper_data = (ModuleDebugDrawWrapperData*)target_task->data;
						ThreadTask source_task = source_manager->GetTask(source_index);
						wrapper_data->inherit_data = source_task.data;
					}
				}
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------

	void DisableModuleDebugDrawTaskElement(TaskManager* task_manager, Stream<char> task_name) {
		unsigned int task_index = task_manager->FindTask(task_name);
		ECS_ASSERT_FORMAT(task_index != -1, "Failed to disable debug draw task {#}", task_name);
		void* task_data = task_manager->GetTaskPtr(task_index)->data;
		ModuleDebugDrawWrapperData* data = (ModuleDebugDrawWrapperData*)task_data;
		data->is_enabled = false;
	}

	void EnableModuleDebugDrawTaskElement(TaskManager* task_manager, Stream<char> task_name) {
		unsigned int task_index = task_manager->FindTask(task_name);
		ECS_ASSERT_FORMAT(task_index != -1, "Failed to enable debug draw task {#}", task_name);
		void* task_data = task_manager->GetTaskPtr(task_index)->data;
		ModuleDebugDrawWrapperData* data = (ModuleDebugDrawWrapperData*)task_data;
		data->is_enabled = true;
	}

	bool FlipModuleDebugDrawTaskElement(TaskManager* task_manager, Stream<char> task_name) {
		unsigned int task_index = task_manager->FindTask(task_name);
		ECS_ASSERT_FORMAT(task_index != -1, "Failed to flip state for debug draw task {#}", task_name);
		void* task_data = task_manager->GetTaskPtr(task_index)->data;
		ModuleDebugDrawWrapperData* data = (ModuleDebugDrawWrapperData*)task_data;
		data->is_enabled = !data->is_enabled;
		return data->is_enabled;
	}

	// -----------------------------------------------------------------------------------------------------------

	bool IsModuleDebugDrawTaskElementEnabled(const TaskManager* task_manager, Stream<char> task_name)
	{
		unsigned int task_index = task_manager->FindTask(task_name);
		ECS_ASSERT_FORMAT(task_index != -1, "Failed to get enabled status for debug draw task {#}", task_name);
		void* task_data = task_manager->GetTask(task_index).data;
		ModuleDebugDrawWrapperData* data = (ModuleDebugDrawWrapperData*)task_data;
		return data->is_enabled;
	}

	// -----------------------------------------------------------------------------------------------------------

	void SetEnabledModuleDebugDrawTaskElements(TaskManager* task_manager, Stream<Stream<char>> task_names)
	{
		for (size_t index = 0; index < task_names.size; index++) {
			unsigned int task_index = task_manager->FindTask(task_names[index]);
			if (task_index != -1) {
				ThreadTask* target_task = task_manager->GetTaskPtr(task_index);
				ModuleDebugDrawWrapperData* data = (ModuleDebugDrawWrapperData*)target_task->data;
				data->is_enabled = true;
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------

	// -----------------------------------------------------------------------------------------------------------
}