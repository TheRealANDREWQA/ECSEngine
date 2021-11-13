#include "ecspch.h"
#include "Module.h"
#include "../../Utilities/Function.h"
#include "../../Internal/World.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../../Utilities/FunctionTemplates.h"

ECS_CONTAINERS;

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------

	bool IsModule(Stream<wchar_t> path)
	{
		auto data = LoadModule(path);
		if (data.code == ECS_GET_MODULE_OK) {
			ReleaseModule(data);
			return true;
		}
		return false;
	}

	// -----------------------------------------------------------------------------------------------------------

	Module LoadModule(Stream<wchar_t> path)
	{
		Module data;

		ECS_TEMP_STRING(library_path, 256);
		library_path.Copy(path);
		library_path[library_path.size] = L'\0';

		HMODULE module_handle = LoadLibrary(library_path.buffer);

		if (module_handle == nullptr) {
			data.code = ECS_GET_MODULE_FAULTY_PATH;
			data.function = nullptr;
			data.os_module_handle = nullptr;

			return data;
		}

		data.function = (ModuleFunction)GetProcAddress(module_handle, ECS_MODULE_FUNCTION_NAME);
		if (data.function == nullptr) {
			FreeLibrary(module_handle);
			data.code = ECS_GET_MODULE_FUNCTION_MISSING;
			data.function = nullptr;
			data.os_module_handle = nullptr;
			return data;
		}

		data.code = ECS_GET_MODULE_OK;
		data.os_module_handle = module_handle;
		data.tasks = { nullptr, 0 };

		return data;
	}

	// -----------------------------------------------------------------------------------------------------------

	Module LoadModule(containers::Stream<wchar_t> path, World* world, AllocatorPolymorphic allocator) {
		Module module = LoadModule(path);
		if (module.code == ECS_GET_MODULE_OK) {
			LoadModuleTasks(world, module, allocator);
		}
		return module;
	}

	// -----------------------------------------------------------------------------------------------------------

	void LoadModuleTasks(World* world, Module* module, AllocatorPolymorphic allocator)
	{
		module->tasks = LoadModuleTasks(world, *module, allocator);
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<TaskDependencyElement> LoadModuleTasks(World* world, Module module, AllocatorPolymorphic allocator)
	{
		constexpr size_t MAX_TASKS = 32;
		constexpr size_t MAX_DEPENDENCIES_PER_TASK = 8;
		Stream<TaskDependencyElement> stream;

		void* temp_allocation = ECS_STACK_ALLOC(sizeof(TaskDependencyElement) * MAX_TASKS + sizeof(Stream<char>) * MAX_TASKS * MAX_DEPENDENCIES_PER_TASK);
		stream.InitializeFromBuffer(temp_allocation, 0);
		for (size_t index = 0; index < MAX_TASKS; index++) {
			stream[index].task.name = nullptr;
		}

		module.function(world, stream);

		size_t total_size = sizeof(TaskDependencyElement) * stream.size;
		for (size_t index = 0; index < stream.size; index++) {
			if (stream[index].task.name != nullptr) {
				total_size += sizeof(char) * (strlen(stream[index].task.name) + 1);
			}

			total_size += stream[index].task_name.size * sizeof(char);
			for (size_t subindex = 0; subindex < stream[index].dependencies.size; subindex++) {
				total_size += stream[index].dependencies[subindex].size * sizeof(char);
			}
		}

		void* allocation = Allocate(allocator, total_size);
		uintptr_t buffer = (uintptr_t)allocation;

		Stream<TaskDependencyElement> temp_stream = stream;
		stream.InitializeFromBuffer(buffer, temp_stream.size);
		for (size_t index = 0; index < temp_stream.size; index++) {
			stream[index].task = temp_stream[index].task;
			stream[index].task_name.InitializeAndCopy(buffer, temp_stream[index].task_name);

			if (temp_stream[index].task.name != nullptr) {
				char* ptr = (char*)buffer;
				size_t name_size = strlen(temp_stream[index].task.name) + 1;

				memcpy(ptr, temp_stream[index].task.name, name_size * sizeof(char));
				stream[index].task.name = ptr;

				buffer += name_size;
			}

			for (size_t subindex = 0; subindex < temp_stream[index].dependencies.size; subindex++) {
				stream[index].dependencies[subindex].InitializeAndCopy(buffer, temp_stream[index].dependencies[subindex]);
			}
		}

		return stream;
	}

	// -----------------------------------------------------------------------------------------------------------

	void ReleaseModule(Module module) {
		BOOL success = FreeLibrary(module.os_module_handle);
	}

	// -----------------------------------------------------------------------------------------------------------

	void ReleaseModule(Module module, AllocatorPolymorphic allocator) {
		ReleaseModule(module);
		if (module.tasks.buffer != nullptr && module.tasks.size > 0) {
			Deallocate(allocator, module.tasks.buffer);
		}
	}

	// -----------------------------------------------------------------------------------------------------------

	// -----------------------------------------------------------------------------------------------------------
}