#include "ecspch.h"
#include "DLL.h"
#include <DbgHelp.h>
#include "../Utilities/StackScope.h"

#pragma comment(lib, "dbghelp.lib")

bool SYM_INITIALIZED = false;

namespace ECSEngine {

	namespace OS {

		// -----------------------------------------------------------------------------------------------------

		void* LoadDLL(Stream<wchar_t> path)
		{
			NULL_TERMINATE_WIDE(path);
			return LoadLibraryEx(path.buffer, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
		}

		// -----------------------------------------------------------------------------------------------------

		void UnloadDLL(void* module_handle)
		{
			FreeLibrary((HMODULE)module_handle);
		}

		// -----------------------------------------------------------------------------------------------------

		void* GetDLLSymbol(void* module_handle, const char* symbol_name)
		{
			return GetProcAddress((HMODULE)module_handle, symbol_name);
		}

		// -----------------------------------------------------------------------------------------------------

		void InitializeSymbolicLinksPaths(Stream<Stream<wchar_t>> module_paths)
		{
			ECS_STACK_CAPACITY_STREAM(wchar_t, search_paths, ECS_KB * 8);
			for (size_t index = 0; index < module_paths.size; index++) {
				search_paths.AddStream(module_paths[index]);
				search_paths.AddAssert(L':');
			}
			// This has to be size - 1 to replace the last colon that was added in the loop
			unsigned int null_terminator_index = search_paths.size == 0 ? 0 : search_paths.size - 1;
			search_paths[null_terminator_index] = L'\0';

			bool success = false;
			if (!SYM_INITIALIZED) {
				success = SymInitializeW(GetCurrentProcess(), search_paths.buffer, true);
				SYM_INITIALIZED = true;
			}
			else {
				success = SymSetSearchPathW(GetCurrentProcess(), search_paths.buffer);
			}

			ECS_ASSERT(success, "Initializing symbolic link paths failed.");
		}

		// -----------------------------------------------------------------------------------------------------

#if 0

		struct Data {
			CapacityStream<Stream<wchar_t>> modules;
			AllocatorPolymorphic stack_allocator;
		};

		static BOOL EnumerateModules(PCWSTR ModuleName, DWORD64 BaseOfDll, PVOID UserContext) {
			Data* data = (Data*)UserContext;
			Stream<wchar_t> module = ModuleName;
			module.size++;
			data->modules.AddAssert(module.Copy(data->stack_allocator));
			return TRUE;
		}

		// This is useful only when for some reason symbols are not loaded
		// And want to inspector what modules are loaded
		static void EnumerateModulesDebug() {
			ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, modules, 512);
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
			Data data = { modules, &stack_allocator };
			BOOL success = SymEnumerateModulesW64(GetCurrentProcess(), EnumerateModules, &data);
		}

#endif

		void SetSymbolicLinksPaths(Stream<Stream<wchar_t>> module_paths)
		{
			ECS_STACK_CAPACITY_STREAM(wchar_t, search_paths, ECS_KB * 16);
			for (size_t index = 0; index < module_paths.size; index++) {
				search_paths.AddStream(module_paths[index]);
				search_paths.AddAssert(L';');
			}
			search_paths[search_paths.size - 1] = L'\0';

			bool success = SymSetSearchPathW(GetCurrentProcess(), search_paths.buffer);
			ECS_ASSERT(success, "Setting symbolic link paths failed.");
		}

		// -----------------------------------------------------------------------------------------------------

		void RefreshDLLSymbols()
		{
			SymRefreshModuleList(GetCurrentProcess());
		}

		// -----------------------------------------------------------------------------------------------------

		bool LoadDLLSymbols(void* module_handle)
		{
			ECS_STACK_CAPACITY_STREAM(wchar_t, module_name, 512);
			DWORD written_size = GetModuleFileName((HMODULE)module_handle, module_name.buffer, module_name.capacity);
			if (written_size == 0 || written_size == module_name.capacity) {
				return false;
			}
			else {
				return LoadDLLSymbols(module_name, module_handle);
			}
		}

		// -----------------------------------------------------------------------------------------------------

		bool LoadDLLSymbols(Stream<wchar_t> module_name, void* module_handle)
		{
			NULL_TERMINATE_WIDE(module_name);
			HANDLE process_handle = GetCurrentProcess();
			if (module_handle == nullptr) {
				module_handle = GetModuleHandle(module_name.buffer);
			}
			DWORD64 base_address = SymLoadModuleExW(process_handle, nullptr, module_name.buffer, nullptr, (DWORD64)module_handle, 0, nullptr, 0);
			if (base_address != 0) {
				return true;
			}
			else {
				// If the module is already loaded, it returns 0 for the base address but the last error is success
				return GetLastError() == ERROR_SUCCESS;
			}
		}

		// -----------------------------------------------------------------------------------------------------

		bool UnloadDLLSymbols(void* module_handle)
		{
			return SymUnloadModule64(GetCurrentProcess(), (DWORD64)module_handle);
		}

		// -----------------------------------------------------------------------------------------------------

	}

}