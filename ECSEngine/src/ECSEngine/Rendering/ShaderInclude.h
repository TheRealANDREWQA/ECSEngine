#pragma once
#include "../Allocators/MemoryManager.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	class ShaderIncludeFiles : public ID3DInclude {
	public:
		ShaderIncludeFiles(MemoryManager* memory, Stream<Stream<wchar_t>> shader_directory);

		HRESULT Open(D3D_INCLUDE_TYPE include_type, LPCSTR filename, LPCVOID parent_data, LPCVOID* data_pointer, UINT* byte_pointer) override;

		HRESULT Close(LPCVOID data) override;

		MemoryManager* memory;
		Stream<Stream<wchar_t>> shader_directory;
	};

}