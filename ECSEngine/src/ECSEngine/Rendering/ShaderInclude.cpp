#include "ecspch.h"
#include "ShaderInclude.h"
#include "../Utilities/File.h"
#include "../Utilities/Function.h"
#include "../Utilities/Path.h"

ECS_CONTAINERS;

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------------------

	ShaderIncludeFiles::ShaderIncludeFiles(MemoryManager* _memory, Stream<wchar_t> _shader_directory) : memory(_memory), shader_directory(_shader_directory) {}

	// ----------------------------------------------------------------------------------------------------------------------------------

	HRESULT ShaderIncludeFiles::Open(D3D_INCLUDE_TYPE include_type, LPCSTR filename, LPCVOID parent_data, LPCVOID* data_pointer, UINT* byte_pointer)
	{
		if (include_type != D3D_INCLUDE_LOCAL && include_type != D3D_INCLUDE_SYSTEM) {
			return E_FAIL;
		}

		ECS_TEMP_STRING(current_path, 512);
		ECS_TEMP_STRING(include_filename, 128);
		Stream<char> include_filename_ascii = function::PathFilename(ToStream(filename), ECS_OS_PATH_SEPARATOR_ASCII_REL);
		function::ConvertASCIIToWide(include_filename, include_filename_ascii);

		struct SearchData {
			LPCVOID* data_pointer;
			UINT* byte_pointer;
			MemoryManager* manager;
			Stream<wchar_t> include_filename;
		};

		SearchData search_data = { data_pointer, byte_pointer, memory, include_filename };

		// Search every directory for that file
		auto search_file = [](const std::filesystem::path& path, void* _data) {
			SearchData* data = (SearchData*)_data;

			Stream<wchar_t> current_path = ToStream(path.c_str());
			Stream<wchar_t> current_filename = function::PathFilename(current_path);

			if (function::CompareStrings(current_filename, data->include_filename)) {
				std::ifstream stream(std::wstring(current_path.buffer, current_path.buffer + current_path.size));
				if (stream.good()) {
					size_t file_size = function::GetFileByteSize(stream);
					void* allocation = data->manager->Allocate_ts(file_size);
					stream.read((char*)allocation, file_size);
					size_t read_count = stream.gcount();

					*data->data_pointer = allocation;
					*data->byte_pointer = read_count;
					return false;
				}
			}
			return true;
		};

		const wchar_t* extension[1] = { L".hlsli" };
		*byte_pointer = 0;
		ForEachFileInDirectoryRecursiveWithExtension(shader_directory, { &extension, std::size(extension) }, &search_data, search_file);
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