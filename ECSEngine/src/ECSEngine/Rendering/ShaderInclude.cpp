#include "ecspch.h"
#include "ShaderInclude.h"
#include "../Utilities/File.h"
#include "../Utilities/Function.h"
#include "../Utilities/Path.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Utilities/ForEachFiles.h"

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------------------

	ShaderIncludeFiles::ShaderIncludeFiles(MemoryManager* _memory, Stream<Stream<wchar_t>> _shader_directory) : memory(_memory), shader_directory(_shader_directory) {}

	// ----------------------------------------------------------------------------------------------------------------------------------

	HRESULT ShaderIncludeFiles::Open(D3D_INCLUDE_TYPE include_type, LPCSTR filename, LPCVOID parent_data, LPCVOID* data_pointer, UINT* byte_pointer)
	{
		if (include_type != D3D_INCLUDE_LOCAL && include_type != D3D_INCLUDE_SYSTEM) {
			return E_FAIL;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, current_path, 512);
		ECS_STACK_CAPACITY_STREAM(wchar_t, include_filename, 128);
		Stream<char> include_filename_ascii = function::PathFilename(filename, ECS_OS_PATH_SEPARATOR_ASCII_REL);
		function::ConvertASCIIToWide(include_filename, include_filename_ascii);

		struct SearchData {
			LPCVOID* data_pointer;
			UINT* byte_pointer;
			MemoryManager* manager;
			Stream<wchar_t> include_filename;
		};

		SearchData search_data = { data_pointer, byte_pointer, memory, include_filename };

		// Search every directory for that file
		auto search_file = [](Stream<wchar_t> path, void* _data) {
			SearchData* data = (SearchData*)_data;

			Stream<wchar_t> current_filename = function::PathFilename(path);

			if (function::CompareStrings(current_filename, data->include_filename)) {
				Stream<char> file_data = ReadWholeFileText(path, GetAllocatorPolymorphic(data->manager, ECS_ALLOCATION_MULTI));
				if (file_data.buffer != nullptr) {
					*data->byte_pointer = file_data.size;
					*data->data_pointer = file_data.buffer;
				}
				return false;
			}
			return true;
		};

		Stream<wchar_t> extension[1] = { L".hlsli" };
		*byte_pointer = 0;
		for (size_t index = 0; index < shader_directory.size && *byte_pointer == 0; index++) {
			ForEachFileInDirectoryRecursiveWithExtension(shader_directory[index], { &extension, std::size(extension) }, &search_data, search_file);
		}
		if (*byte_pointer > 0) {
			return S_OK;
		}

		return E_FAIL;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	HRESULT ShaderIncludeFiles::Close(LPCVOID data)
	{
		memory->Deallocate_ts(data);
		return S_OK;
	}

	// ----------------------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------------------
}