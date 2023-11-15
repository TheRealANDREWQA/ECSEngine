#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	namespace OS {

		// Returns nullptr if it failed
		ECSENGINE_API void* LoadDLL(Stream<wchar_t> path);

		ECSENGINE_API void UnloadDLL(void* module_handle);

		// Returns nullptr if there is no such symbol
		ECSENGINE_API void* GetDLLSymbol(void* module_handle, const char* symbol_name);

		ECSENGINE_API void InitializeSymbolicLinksPaths(Stream<Stream<wchar_t>> module_paths);

		ECSENGINE_API void SetSymbolicLinksPaths(Stream<Stream<wchar_t>> module_paths);

		// Looks at the loaded modules by the process and loads the symbol information
		// For them such that stack unwinding can recognize the symbols correctly
		ECSENGINE_API void RefreshDLLSymbols();

		// This will load the debugging symbols related to the module
		// Returns true if it succeeded, else false
		ECSENGINE_API bool LoadDLLSymbols(void* module_handle);

		// This will load the debugging symbols related to the module. If the module handle
		// is nullptr, it will try to deduce it from the name (the dll should be loaded previously)
		// Returns true if it succeeded, else false
		ECSENGINE_API bool LoadDLLSymbols(Stream<wchar_t> module_name, void* module_handle);

		// This will unload the debugging symbols related to the module
		// Returns true if it succeeded, else false
		ECSENGINE_API bool UnloadDLLSymbols(void* module_handle);

	}

}