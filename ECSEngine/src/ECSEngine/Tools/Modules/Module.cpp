#include "ecspch.h"
#include "Module.h"
#include "../../Utilities/Function.h"
#include "../../Internal/World.h"
#include "../../Allocators/AllocatorPolymorphic.h"
#include "../../Utilities/FunctionTemplates.h"

ECS_CONTAINERS;

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------------------------

	GetModuleFunctionData GetModuleFunction(Stream<wchar_t> path)
	{
		GetModuleFunctionData data;

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
		return data;
	}

	// -----------------------------------------------------------------------------------------------------------

	GetModuleGraphicsFunctionData GetModuleGraphicsFunction(Stream<wchar_t> path) {
		GetModuleGraphicsFunctionData data;

		ECS_TEMP_STRING(library_path, 256);
		library_path.Copy(path);
		library_path[library_path.size] = L'\0';

		HMODULE module_handle = LoadLibrary(library_path.buffer);

		if (module_handle == nullptr) {
			data.code = ECS_GET_MODULE_FAULTY_PATH;
			data.draw = nullptr;
			data.initialize = nullptr;
			data.release = nullptr;
			data.os_module_handle = nullptr;

			return data;
		}

		data.draw = (ModuleGraphicsDraw)GetProcAddress(module_handle, ECS_MODULE_GRAPHICS_DRAW_FUNCTION_NAME);
		data.initialize = (ModuleGraphicsInitialize)GetProcAddress(module_handle, ECS_MODULE_GRAPHICS_INITIALIZE_FUNCTION_NAME);
		data.release = (ModuleGraphicsRelease)GetProcAddress(module_handle, ECS_MODULE_GRAPHICS_RELEASE_FUNCTION_NAME);

		if (data.draw == nullptr || data.initialize == nullptr || data.release == nullptr) {
			FreeLibrary(module_handle);
			data.code = ECS_GET_MODULE_FUNCTION_MISSING;
			data.draw = nullptr;
			data.initialize = nullptr;
			data.release = nullptr;
			data.os_module_handle = nullptr;
			return data;
		}

		data.code = ECS_GET_MODULE_OK;
		data.os_module_handle = module_handle;
		return data;
	}

	// -----------------------------------------------------------------------------------------------------------

	bool HasModuleFunction(Stream<wchar_t> path)
	{
		auto data = GetModuleFunction(path);
		if (data.code == ECS_GET_MODULE_OK) {
			FreeModuleFunction(data);
			return true;
		}
		return false;
	}

	// -----------------------------------------------------------------------------------------------------------

	bool HasModuleGraphicsFunction(Stream<wchar_t> path) {
		auto data = GetModuleGraphicsFunction(path);
		if (data.code == ECS_GET_MODULE_OK) {
			FreeModuleGraphicsFunction(data);
			return true;
		}
		return false;
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<TaskGraphElement> GetModuleTasks(World* world, GetModuleFunctionData function_data, AllocatorPolymorphic allocator)
	{
		constexpr size_t MAX_TASKS = 32;
		constexpr size_t MAX_DEPENDENCIES_PER_TASK = 8;
		Stream<TaskGraphElement> stream;

		void* temp_allocation = ECS_STACK_ALLOC(sizeof(TaskGraphElement) * MAX_TASKS + sizeof(Stream<char>) * MAX_TASKS * MAX_DEPENDENCIES_PER_TASK);
		stream.InitializeFromBuffer(temp_allocation, 0);
		for (size_t index = 0; index < MAX_TASKS; index++) {
			stream[index].task.name = nullptr;
		}

		function_data.function(world, stream);

		size_t total_size = sizeof(TaskGraphElement) * stream.size;
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

		Stream<TaskGraphElement> temp_stream = stream;
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

		FreeModuleFunction(function_data);

		return stream;
	}

	// -----------------------------------------------------------------------------------------------------------

	Stream<TaskGraphElement> GetModuleTasks(
		World* world,
		Stream<wchar_t> module_path,
		AllocatorPolymorphic allocator,
		CapacityStream<char>* error_message
	) {
		GetModuleFunctionData function_data = GetModuleFunction(module_path);
		if (function_data.code == ECS_GET_MODULE_FAULTY_PATH) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Could not load module at {0}. The path does not exist or access denied.", module_path);
			return { nullptr, 0 };
		}
		else if (function_data.code == ECS_GET_MODULE_FAULTY_PATH) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "Could not retrieve module function at {0}.", module_path);
			return  { nullptr, 0 };
		}

		return GetModuleTasks(world, function_data, allocator);
	}

	// -----------------------------------------------------------------------------------------------------------

	void DeallocateModuleTasks(containers::Stream<TaskGraphElement> tasks, AllocatorPolymorphic allocator) {
		Deallocate(allocator, tasks.buffer);
	}

	// -----------------------------------------------------------------------------------------------------------

	void FreeModuleFunction(GetModuleFunctionData data) {
		BOOL success = FreeLibrary(data.os_module_handle);
	}

	// -----------------------------------------------------------------------------------------------------------

	void FreeModuleGraphicsFunction(const GetModuleGraphicsFunctionData& data) {
		BOOL success = FreeLibrary(data.os_module_handle);
	}

	// -----------------------------------------------------------------------------------------------------------

	void InitializeGraphicsModule(GetModuleGraphicsFunctionData& data, World* world, void* data_reflection) {
		data.initialized_data = data.initialize(world, data_reflection);
	}

	// -----------------------------------------------------------------------------------------------------------

	void ReleaseGraphicsModule(GetModuleGraphicsFunctionData& data, World* world) {
		data.release(world, data.initialized_data);
		FreeModuleGraphicsFunction(data);
	}

	// -----------------------------------------------------------------------------------------------------------

	// -----------------------------------------------------------------------------------------------------------

	// -----------------------------------------------------------------------------------------------------------
}