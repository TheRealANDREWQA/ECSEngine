// ECS_REFLECT
#include "editorpch.h"
#include "ECSEngine.h"
#include "../DrawFunction2.h"
#include "../DrawFunction.h"
#include "../../resource.h"
#include "../Test.h"
#include "../UI/ToolbarUI.h"
#include "../UI/Hub.h"
#include "EditorParameters.h"
#include "../Project/ProjectOperations.h"
#include "EditorState.h"
#include "../UI/FileExplorerData.h"
#include "EditorEvent.h"
#include "../Modules/Module.h"
#include "EditorPalette.h"
#include "EntryPoint.h"
#include "../UI/NotificationBar.h"
#include "../UI/InspectorData.h"
#include "../UI/Inspector.h"
#include <DbgHelp.h>

#define ERROR_BOX_MESSAGE WM_USER + 1
#define ERROR_BOX_CODE -2

//#define TEST_C_ARRAYS

using namespace ECSEngine;
using namespace ECSEngine::Tools;

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "Shcore.lib")

#ifdef TEST_C_ARRAYS

#include <list>

typedef struct {
	void* buffer;
	size_t size;
	size_t capacity;
	size_t element_size;
} ContiguousStream;

typedef struct {
	void** buffer;
	size_t size;
	size_t capacity;
	size_t element_size;
} PointerStream;

#pragma region Contiguous Stream

size_t ResizeCapacity(size_t capacity) {
	return (size_t)(1.5f * (float)capacity + 1);
}

void* GetElement(ContiguousStream stream, size_t index) {
	return function::OffsetPointer(stream.buffer, index * stream.element_size);
}

void SetElement(ContiguousStream stream, size_t index, const void* new_value) {
	void* element = GetElement(stream, index);
	memcpy(element, new_value, stream.element_size);
}

void Add(ContiguousStream* stream, const void* element) {
	if (stream->size == stream->capacity) {
		stream->capacity = ResizeCapacity(stream->capacity);
		if (stream->buffer != NULL) {
			stream->buffer = realloc(stream->buffer, stream->capacity * stream->element_size);
		}
		else {
			stream->buffer = malloc(stream->element_size * stream->capacity);
		}
	}

	SetElement(*stream, stream->size, element);
	stream->size++;
}

ContiguousStream CreateStream(size_t capacity, size_t element_size) {
	if (capacity > 0) {
		void* buffer = malloc(element_size * capacity);
		return { buffer, 0, capacity, element_size };
	}
	else {
		return { NULL, 0, 0, element_size };
	}
}

void FreeStream(ContiguousStream stream) {
	if (stream.buffer != NULL) {
		free(stream.buffer);
	}
}

void Remove(ContiguousStream* stream, size_t index) {
	memmove(function::OffsetPointer(stream->buffer, stream->element_size * index), function::OffsetPointer(stream->buffer, stream->element_size * (index + 1)), stream->element_size * (stream->size - index - 1));
	stream->size--;
}

void RemoveSwapBack(ContiguousStream* stream, size_t index) {
	stream->size--;
	memcpy(function::OffsetPointer(stream->buffer, stream->element_size * index), function::OffsetPointer(stream->buffer, stream->element_size * stream->size), stream->element_size);
}

size_t Find(ContiguousStream stream, const void* element) {
	for (size_t index = 0; index < stream.size; index++) {
		if (memcmp(function::OffsetPointer(stream.buffer, stream.element_size * index), element, stream.element_size) == 0) {
			return index;
		}
	}
	return -1;
}

size_t FindFunctor(ContiguousStream stream, const void* element, int (*find_functor)(const void* array_element, const void* element)) {
	for (size_t index = 0; index < stream.size; index++) {
		if (find_functor(function::OffsetPointer(stream.buffer, stream.element_size * index), element) == 1) {
			return index;
		}
	}
	return -1;
}

void Insert(ContiguousStream* stream, size_t index, const void* element) {
	memmove(
		function::OffsetPointer(stream->buffer, (index + 1) * stream->element_size),
		function::OffsetPointer(stream->buffer, index * stream->element_size),
		stream->element_size * (stream->size - index)
	);
	SetElement(*stream, index, element);
}

void Resize(ContiguousStream* stream, size_t new_capacity) {
	stream->capacity = new_capacity;
	if (stream->buffer != NULL) {
		stream->buffer = realloc(stream->buffer, stream->capacity * stream->element_size);
	}
	else {
		stream->buffer = malloc(stream->element_size * stream->capacity);
	}
}

void Reserve(ContiguousStream* stream, size_t element_count) {
	if (stream->size + element_count > stream->capacity) {
		Resize(stream, ResizeCapacity(stream->capacity));
	}
}


#pragma endregion

#pragma region Pointer Stream

void SetElementAllocation(PointerStream stream, size_t index, const void* new_value) {
	void* value = malloc(stream.element_size);
	memcpy(value, new_value, stream.element_size);
	stream.buffer[index] = value;
}

void Resize(PointerStream* stream, size_t new_capacity) {
	stream->capacity = new_capacity;
	if (stream->buffer != NULL) {
		stream->buffer = (void**)realloc(stream->buffer, stream->capacity * sizeof(void*));
	}
	else {
		stream->buffer = (void**)malloc(stream->element_size * sizeof(void*));
	}
}

void Add(PointerStream* stream, const void* element) {
	if (stream->size == stream->capacity) {
		Resize(stream, ResizeCapacity(stream->capacity));
	}

	SetElementAllocation(*stream, stream->size, element);
	stream->size++;
}

PointerStream CreatePointerStream(size_t capacity, size_t element_size) {
	if (capacity > 0) {
		void* buffer = malloc(sizeof(void*) * capacity);
		return { (void**)buffer, 0, capacity, element_size };
	}
	else {
		return { NULL, 0, 0, element_size };
	}
}

void FreeStream(PointerStream stream) {
	if (stream.buffer != NULL) {
		for (size_t index = 0; index < stream.size; index++) {
			free(stream.buffer[index]);
		}
		free(stream.buffer);
	}
}

void* GetElement(PointerStream stream, size_t index) {
	return stream.buffer[index];
}

void Remove(PointerStream* stream, size_t index) {
	free(stream->buffer[index]);
	memmove(stream->buffer + index, stream->buffer + index + 1, sizeof(void*) * (stream->size - index - 1));
	stream->size--;
}

void RemoveSwapBack(PointerStream* stream, size_t index) {
	free(stream->buffer[index]);
	stream->size--;
	stream->buffer[index] = stream->buffer[stream->size];
}

size_t Find(PointerStream stream, const void* element) {
	for (size_t index = 0; index < stream.size; index++) {
		if (memcmp(GetElement(stream, index), element, stream.element_size) == 0) {
			return index;
		}
	}
	return -1;
}

size_t FindFunctor(PointerStream stream, const void* element, int (*find_functor)(const void* array_element, const void* element)) {
	for (size_t index = 0; index < stream.size; index++) {
		if (find_functor(GetElement(stream, index), element) == 1) {
			return index;
		}
	}
	return -1;
}

void Reserve(PointerStream* stream, size_t element_count) {
	if (stream->size + element_count > stream->capacity) {
		Resize(stream, ResizeCapacity(stream->capacity));
	}
}

void SetElement(PointerStream stream, size_t index, void* value) {
	stream.buffer[index] = value;
}

#pragma endregion

#endif

class Editor : public ECSEngine::Application {
public:
	Editor(int width, int height, LPCWSTR name) noexcept;
	Editor() : timer("Editor")/*, width(0), height(0)*/ {};
	~Editor();
	Editor(const Editor&) = delete;
	Editor& operator = (const Editor&) = delete;

public:

	void ChangeCursor(ECSEngine::CursorType type) override {
		if (type != current_cursor) {
			SetCursor(cursors[(unsigned int)type]);
			SetClassLong(hWnd, -12, (LONG)cursors[(unsigned int)type]);
			current_cursor = type;
		}
	}

	ECSEngine::CursorType GetCurrentCursor() const override {
		return current_cursor;
	}

	void WriteTextToClipboard(const char* text) override {
		assert(OpenClipboard(hWnd) == TRUE);
		assert(EmptyClipboard() == TRUE);

		size_t text_size = strlen((const char*)text);
		auto handle = GlobalAlloc(GMEM_MOVEABLE, text_size + 1);
		auto new_handle = GlobalLock(handle);
		memcpy(new_handle, text, sizeof(unsigned char) * text_size);
		char* reinterpretation = (char*)new_handle;
		reinterpretation[text_size] = '\0';
		GlobalUnlock(handle);
		SetClipboardData(CF_TEXT, handle);
		assert(CloseClipboard() == TRUE);
	}

	unsigned int CopyTextFromClipboard(char* text, unsigned int max_size) override {
		if (IsClipboardFormatAvailable(CF_TEXT)) {
			assert(OpenClipboard(hWnd) == TRUE);
			HANDLE global_handle = GetClipboardData(CF_TEXT);
			HANDLE data_handle = GlobalLock(global_handle);
			unsigned int copy_count = strlen((const char*)data_handle);
			copy_count = ECSEngine::function::Select(copy_count > max_size - 1, max_size - 1, copy_count);
			memcpy(text, data_handle, copy_count);
			GlobalUnlock(global_handle);
			assert(CloseClipboard() == TRUE);
			return copy_count;
		}
		else {
			text = (char*)"";
			return 0;
		}
	}

	void* GetOSWindowHandle() override {
		return hWnd;
	}

	int Run() override {
		using namespace ECSEngine;
		using namespace ECSEngine::Tools;

		unsigned int ecs_runtime_index = 0;
#ifdef ECSENGINE_RELEASE
		ecs_runtime_index = 2;
#endif

#ifdef ECSENGINE_DISTRIBUTION
		ecs_runtime_index = 4;
#endif
		OS::InitializeSymbolicLinksPaths({ ECS_RUNTIME_PDB_PATHS + ecs_runtime_index, 2 });
		
		EditorState editor_state;
		EditorStateInitialize(this, &editor_state, hWnd, mouse, keyboard);
		
		ResourceManager* resource_manager = editor_state.resource_manager;
		Graphics* graphics = editor_state.Graphics();

		Hub(&editor_state);

		MSG message;
		BOOL result = 0;

		VertexBuffer vertex_buffer;
		IndexBuffer index_buffer;

		//ECS_TEMP_ASCII_STRING(ERROR_MESSAGE, 256);
		//GLTFData gltf_data = LoadGLTFFile(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\trireme2material.glb", &ERROR_MESSAGE);
		////GLTFData gltf_data = LoadGLTFFile(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Cerberus.glb", &ERROR_MESSAGE);
		////GLTFData gltf_data = LoadGLTFFile(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\trireme2_flipped.glb", &ERROR_MESSAGE);
		////GLTFData gltf_data = LoadGLTFFile(L"C:\\Users\\Andrei\\C++\\ECSEngine\\Editor\\Resources\\DebugPrimitives\\Alphabet.glb", &ERROR_MESSAGE);
		//GLTFMesh gltf_meshes[128];
		//Mesh meshes[128];
		//PBRMaterial _materials[128];
		//unsigned int _submesh_material_index[128];
		//Submesh _submeshes[128];
		//Submesh _normal_submeshes[128];

		//AllocatorPolymorphic allocator = GetAllocatorPolymorphic(editor_state.GlobalMemoryManager());
		//bool success = LoadMeshesFromGLTF(gltf_data, gltf_meshes, allocator, &ERROR_MESSAGE);
		///*GLTFMeshesToMeshes(&graphics, gltf_meshes, meshes, gltf_data.mesh_count);*/

		//Stream<PBRMaterial> materials(_materials, 0);
		//Stream<unsigned int> submesh_material_index(_submesh_material_index, 0);
		//success = LoadDisjointMaterialsFromGLTF(gltf_data, materials, submesh_material_index, allocator, &ERROR_MESSAGE);
		//GLTFMeshesToMeshes(graphics, gltf_meshes, meshes, gltf_data.mesh_count);
		//memset(submesh_material_index.buffer, 0, sizeof(unsigned int) * gltf_data.mesh_count);
		//materials.size = 1;
		//Mesh merged_mesh = GLTFMeshesToMergedMesh(graphics, gltf_meshes, _submeshes, _submesh_material_index, materials.size, gltf_data.mesh_count);
		//Mesh normal_merged_mesh = MeshesToSubmeshes(graphics, Stream<Mesh>(meshes, gltf_data.mesh_count), _normal_submeshes);
		//FreeGLTFMeshes(gltf_meshes, gltf_data.mesh_count, allocator);
		//FreeGLTFFile(gltf_data);

		//PBRMaterial created_material = CreatePBRMaterialFromName(ToStream("Cerberus"), ToStream("Cerberus"), ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets"), allocator);
		////Material cerberus_material = PBRToMaterial(resource_manager, created_material, ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets"));
		//PBRMesh* cerberus = resource_manager->LoadPBRMesh(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\cerberus_textures.glb");
		//Material cerberus_material = PBRToMaterial(resource_manager, cerberus->materials[0], ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets"));

		//Stream<char> shader_source;

		//VertexShader forward_lighting_v_shader = resource_manager->LoadVertexShaderImplementation(ECS_VERTEX_SHADER_SOURCE(ForwardLighting), &shader_source);
		//PixelShader forward_lighting_p_shader = resource_manager->LoadPixelShaderImplementation(ECS_PIXEL_SHADER_SOURCE(ForwardLighting));
		//PixelShader forward_lighting_no_normal_p_shader = resource_manager->LoadPixelShaderImplementation(ECS_PIXEL_SHADER_SOURCE(ForwardLightingNoNormal));
		//InputLayout forward_lighting_layout = graphics->ReflectVertexShaderInput(forward_lighting_v_shader, shader_source);
		//resource_manager->Deallocate(shader_source.buffer);
		//
		//ShaderCompileOptions options;
		//ShaderMacro _macros[32];
		//Stream<ShaderMacro> macros(_macros, 0);
		//macros[0] = { "COLOR_TEXTURE", "" };
		//macros[1] = { "ROUGHNESS_TEXTURE", "" };
		//macros[2] = { "OCCLUSION_TEXTURE", "" };
		//macros[3] = { "NORMAL_TEXTURE", "" };
		//macros.size = 4;
		//options.macros = macros;
		//PixelShader PBR_pixel_shader = resource_manager->LoadPixelShaderImplementation(ECS_PIXEL_SHADER_SOURCE(PBR), nullptr, options);
		//VertexShader PBR_vertex_shader = resource_manager->LoadVertexShaderImplementation(ECS_VERTEX_SHADER_SOURCE(PBR), &shader_source);
		//InputLayout PBR_layout = graphics->ReflectVertexShaderInput(PBR_vertex_shader, shader_source);
		//resource_manager->Deallocate(shader_source.buffer);

		//ConstantBuffer obj_buffer = graphics->CreateConstantBuffer(sizeof(float) * 32);

		//ConstantBuffer specular_factors = graphics->CreateConstantBuffer(sizeof(float) * 2);

		//ConstantBuffer hemispheric_ambient_light = Shaders::CreateHemisphericConstantBuffer(graphics);
		//ConstantBuffer directional_light = Shaders::CreateDirectionalLightBuffer(graphics);
		//ConstantBuffer camera_position_buffer = Shaders::CreateCameraPositionBuffer(graphics);
		//ConstantBuffer point_light = Shaders::CreatePointLightBuffer(graphics);
		//ConstantBuffer spot_light = Shaders::CreateSpotLightBuffer(graphics);
		//ConstantBuffer capsule_light = Shaders::CreateCapsuleLightBuffer(graphics);
		//ConstantBuffer pbr_lights = graphics->CreateConstantBuffer(sizeof(float4) * 8 + sizeof(float4) * 4);
		//ConstantBuffer pbr_pixel_values = Shaders::CreatePBRPixelConstants(graphics);
		//ConstantBuffer pbr_vertex_values = Shaders::CreatePBRVertexConstants(graphics);

		//Shaders::SetCapsuleLight(capsule_light, graphics, float3(0.0f, 0.0f, 20.0f), float3(0.0f, 1.0f, 0.0f), 10.0f, 1.0f, 2.0f, ColorFloat(50.0f, 50.0f, 50.0f));

		//const ColorFloat COLORS[] = {
		//	{1.0f, 0, 0},
		//	{0, 1.0f, 0},
		//	{0, 0, 1.0f},
		//	{1.0f, 1.0f, 0},
		//	{1.0f, 0, 1.0f},
		//	{0, 1.0f, 1.0f}
		//};
		//ConstantBuffer index_color = graphics->CreateConstantBuffer(sizeof(ColorFloat) * 6, COLORS);

		//D3D11_SAMPLER_DESC descriptor = {};
		//descriptor.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		//descriptor.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		//descriptor.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		//descriptor.Filter = D3D11_FILTER_ANISOTROPIC;
		//descriptor.MaxAnisotropy = 16;
		//SamplerState sampler = graphics->CreateSamplerState(descriptor);

		Stream<wchar_t> files_to_be_packed[] = {
			ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ram_Bronze Ram_BaseColor.jpg"),
			ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ram_Bronze Ram_Metallic.jpg"),
			ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ram_Bronze Ram_Roughness.jpg"),
			ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ram_Bronze Ram_Normal.jpg"),
			ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Graphics.h"),
		};

		/*timer.SetMarker();
		void* memory = malloc(ECS_GB);
		EncryptBuffer({ memory, ECS_GB }, 50);
		size_t duration = timer.GetDurationSinceMarker_us();

		ECS_STACK_CAPACITY_STREAM(char, CHARACTERS, 64);
		function::ConvertIntToChars(CHARACTERS, duration);
		OutputDebugStringA(CHARACTERS.buffer);*/

		//auto PackFunctor = [](Stream<void> contents, void* _data) {
		//	EncryptBuffer(contents, contents.size);
		//	return contents;
		//};

		//bool is_file_text[] = {
		//	false,
		//	false,
		//	false,
		//	false,
		//	true
		//};
		//unsigned int status = PackFiles(
		//	{ files_to_be_packed, std::size(files_to_be_packed) },
		//	{ is_file_text, std::size(is_file_text) }, 
		//	ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\packed.bin"),
		//	PackFunctor
		//);

		//ECS_FILE_HANDLE packed_file = -1;
		//ECS_FILE_STATUS_FLAGS status__ = OpenFile(ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\packed.bin"), &packed_file, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_BINARY);
		//PackFilesLookupTable lookup_table = GetPackedFileLookupTable(packed_file);

		//timer.SetMarker();
		//for (size_t index = 0; index < std::size(files_to_be_packed); index++) {
		//	Stream<void> file_content = UnpackFile(files_to_be_packed[index], &lookup_table, packed_file);
		//	DecryptBuffer(file_content, file_content.size);
		//	Stream<void> original_content = ReadWholeFile(files_to_be_packed[index], !is_file_text[index]);

		//	ECS_ASSERT(file_content.size == original_content.size);
		//	ECS_ASSERT(memcmp(file_content.buffer, original_content.buffer, original_content.size) == 0);
		//	//ECS_ASSERT(memcmp(file_content.buffer, memory, original_content.size) != 0);
		//}
		//size_t duration = timer.GetDurationSinceMarker_us();
		//ECS_STACK_CAPACITY_STREAM(char, CHARACTERS, 64);
		//function::ConvertIntToChars(CHARACTERS, duration);
		//OutputDebugStringA(CHARACTERS.buffer);

#ifdef TEST_C_ARRAYS

		const int array_test_size = 100'000;

		ContiguousStream contiguous_stream = CreateStream(0, sizeof(int));
		PointerStream pointer_stream = CreatePointerStream(array_test_size, sizeof(int));
		
		std::list<int> list;
		//list.resize(array_test_size + 10);

		timer.SetMarker();

		for (int index = 0; index < array_test_size; index++) {
			Add(&contiguous_stream, &index);
		}

		size_t contiguous_stream_populate_time = timer.GetDurationSinceMarker_ns();

		timer.SetMarker();

		ContiguousStream fuzz_stream = CreateStream(100'000'000, sizeof(int));
		for (int index = 0; index < array_test_size; index++) {
			Add(&pointer_stream, &index);
#if 1
			for (int subindex = 0; subindex < 5; subindex++) {
				Add(&fuzz_stream, malloc(sizeof(int)));
			}
#endif
		}

		size_t pointer_stream_populate_time = timer.GetDurationSinceMarker_ns();

		timer.SetMarker();
		
		for (int index = 0; index < array_test_size; index++) {
			list.push_back(index);
#if 1
			for (int subindex = 0; subindex < 5; subindex++) {
				Add(&fuzz_stream, malloc(sizeof(int) * 7));
			}
#endif
		}

		size_t list_populate_time = timer.GetDurationSinceMarker_ns();

		timer.SetMarker();

		int sum = 0;
		for (int index = 0; index < array_test_size; index++) {
			sum += *(int*)GetElement(contiguous_stream, index);
		}

		size_t contiguous_stream_access_time = timer.GetDurationSinceMarker_ns();

		ECS_STACK_CAPACITY_STREAM(char, CHARS, 256);
		function::ConvertIntToChars(CHARS, sum);
		CHARS.Add('\n');
		CHARS.Add('\0');
		OutputDebugStringA(CHARS.buffer);

		timer.SetMarker();

		sum = 0;
		for (int index = 0; index < array_test_size; index++) {
			sum += *(int*)GetElement(pointer_stream, index);
		}

		size_t pointer_stream_access_time = timer.GetDurationSinceMarker_ns();

		CHARS.size = 0;
		function::ConvertIntToChars(CHARS, sum);
		CHARS.Add('\n');
		CHARS.Add('\0');
		OutputDebugStringA(CHARS.buffer);

		timer.SetMarker();

		sum = 0;
		for (int element : list) {
			sum += element;
		}

		size_t list_access_time = timer.GetDurationSinceMarker_ns();

		CHARS.size = 0;
		function::ConvertIntToChars(CHARS, sum);
		CHARS.Add('\n');
		CHARS.Add('\0');
		OutputDebugStringA(CHARS.buffer);

		size_t insert_position = array_test_size / 2;
		//size_t insert_position = array_test_size / 10 * 9;
		timer.SetMarker();
		for (int index = 0; index < 10; index++) {
			Insert(&contiguous_stream, insert_position, &index);
		}
		size_t array_insert = timer.GetDurationSinceMarker_ns();

		timer.SetMarker();
		for (int index = 0; index < 10; index++) {
			auto insert_iterator = list.begin();
			std::advance(insert_iterator, insert_position);
			list.insert(insert_iterator, index);
		}
		size_t list_insert = timer.GetDurationSinceMarker_ns();

		timer.SetMarker();
		FreeStream(contiguous_stream);
		size_t contiguous_stream_free_time = timer.GetDurationSinceMarker_ns();

		timer.SetMarker();
		FreeStream(pointer_stream);
		size_t pointer_stream_free_time = timer.GetDurationSinceMarker_ns();

		timer.SetMarker();
		list.clear();
		size_t list_free_time = timer.GetDurationSinceMarker_ns();

		CHARS.size = 0;
		CHARS.AddStream(ToStream("Populate time: "));
		function::ConvertIntToChars(CHARS, contiguous_stream_populate_time);
		CHARS.Add(' ');
		function::ConvertIntToChars(CHARS, pointer_stream_populate_time);
		CHARS.Add(' ');
		function::ConvertIntToChars(CHARS, list_populate_time);
		CHARS.Add('\n');
		
		CHARS.AddStream(ToStream("Access time: "));
		function::ConvertIntToChars(CHARS, contiguous_stream_access_time);
		CHARS.Add(' ');
		function::ConvertIntToChars(CHARS, pointer_stream_access_time);
		CHARS.Add(' ');
		function::ConvertIntToChars(CHARS, list_access_time);
		CHARS.Add('\n');

		CHARS.AddStream(ToStream("Free time: "));
		function::ConvertIntToChars(CHARS, contiguous_stream_free_time);
		CHARS.Add(' ');
		function::ConvertIntToChars(CHARS, pointer_stream_free_time);
		CHARS.Add(' ');
		function::ConvertIntToChars(CHARS, list_free_time);
		CHARS.Add('\n');

		function::ConvertIntToChars(CHARS, array_insert);
		CHARS.Add(' ');
		function::ConvertIntToChars(CHARS, list_insert);
		CHARS.Add('\n');

		float ratio_1_add = (float)pointer_stream_populate_time / (float)contiguous_stream_populate_time;
		float ratio_2_add = (float)list_populate_time / (float)contiguous_stream_populate_time;
		float ratio_1_iterate = (float)pointer_stream_access_time / (float)contiguous_stream_access_time;
		float ratio_2_iterate = (float)list_access_time / (float)contiguous_stream_access_time;
		float ratio_1_free = (float)pointer_stream_free_time  / (float)contiguous_stream_free_time;
		float ratio_2_free = (float)list_free_time / (float)contiguous_stream_free_time;

		float ratio_insert = (float)list_insert / (float)array_insert;
 
		CHARS.AddStream(ToStream("Populate ratios: "));
		function::ConvertFloatToChars(CHARS, ratio_1_add);
		CHARS.Add(' ');
		function::ConvertFloatToChars(CHARS, ratio_2_add);
		CHARS.Add('\n');
		CHARS.AddStream(ToStream("Access ratios: "));
		function::ConvertFloatToChars(CHARS, ratio_1_iterate);
		CHARS.Add(' ');
		function::ConvertFloatToChars(CHARS, ratio_2_iterate);
		CHARS.Add('\n');
		CHARS.AddStream(ToStream("Free ratios: "));
		function::ConvertFloatToChars(CHARS, ratio_1_free);
		CHARS.Add(' ');
		function::ConvertFloatToChars(CHARS, ratio_2_free);
		CHARS.Add('\n');
		CHARS.AddStream(ToStream("Insert ratio: "));
		function::ConvertFloatToChars(CHARS, ratio_insert, 8);
		CHARS.Add('\n');

		CHARS.Add('\0');
		OutputDebugStringA(CHARS.buffer);
		

#endif

		Camera camera;
		camera.translation = { 0.0f, 0.0f, 0.0f };
		//ResourceManagerTextureDesc plank_descriptor;
		//plank_descriptor.context = graphics->m_context;
		//plank_descriptor.usage = D3D11_USAGE_DEFAULT;
		//ResourceView plank_texture = resource_manager->LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\brown_planks_03_diff_1k.jpg", &plank_descriptor);
		//ResourceView plank_normal_texture = resource_manager->LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\brown_planks_03_nor_dx_1k.jpg", &plank_descriptor);
		//ResourceView plank_roughness = resource_manager->LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\brown_planks_03_rough_1k.jpg", &plank_descriptor);
		////ResourceView plank_metallic = resource_manager.LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\brown_planks_03_diff_1k.jpg", plank_descriptor);
		//ResourceView plank_ao = resource_manager->LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\brown_planks_03_ao_1k.jpg", &plank_descriptor);
		//ECS_ASSERT(plank_texture.view != nullptr && plank_normal_texture.view != nullptr && plank_roughness.view != nullptr && plank_ao.view != nullptr);

		/*MemoryManager debug_drawer_memory(5'000'000, 1024, 2'500'000, editor_state.GlobalMemoryManager());
		DebugDrawer debug_drawer(&debug_drawer_memory, resource_manager, 1);*/
		float3 LIGHT_DIRECTION(0.0f, -1.0f, 0.0f);
		ColorFloat LIGHT_INTENSITY(1.0f, 1.0f, 1.0f, 0.0f);


		static bool normal_map = true;
		static float normal_strength = 0.0f;
		static float metallic = 1.0f;
		static float roughness = 0.0f;
		static float3 pbr_light_pos[4] = { float3(0.0f, 0.0f, 20.0f), float3(-3.0f, 0.0f, 20.0f), float3(3.0f, 0.0f, 20.0f), float3(5.0f, 2.0f, 20.0f) };
		static ColorFloat pbr_light_color[4] = { ColorFloat(1.0f, 1.0f, 1.0f, 1.0f), ColorFloat(100.0f, 20.0f, 20.0f, 1.0f), ColorFloat(0.2f, 0.9f, 0.1f, 1.0f), ColorFloat(0.3f, 0.2f, 0.9f, 1.0f) };
		static float pbr_light_range[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		ConstantBuffer normal_strength_buffer = graphics->CreateConstantBuffer(sizeof(float));

		InjectWindowElement inject_elements[16];
		InjectWindowSection section[1];
		section[0].elements = Stream<InjectWindowElement>(inject_elements, std::size(inject_elements));
		section[0].name = "Directional Light";

		inject_elements[0].name = "Direction";
		inject_elements[0].basic_type_string = STRING(float3);
		inject_elements[0].data = &LIGHT_DIRECTION;
		inject_elements[0].stream_type = Reflection::ReflectionStreamFieldType::Basic;

		inject_elements[1].name = "Color";
		inject_elements[1].basic_type_string = STRING(ColorFloat);
		inject_elements[1].data = &LIGHT_INTENSITY;
		inject_elements[1].stream_type = Reflection::ReflectionStreamFieldType::Basic;

		inject_elements[2].name = "Normal map";
		inject_elements[2].basic_type_string = STRING(bool);
		inject_elements[2].data = &normal_map;

		inject_elements[3].name = "Normal strength";
		inject_elements[3].basic_type_string = STRING(float);
		inject_elements[3].data = &normal_strength;

		inject_elements[4].name = "Metallic";
		inject_elements[4].basic_type_string = STRING(float);
		inject_elements[4].data = &metallic;

		inject_elements[5].name = "Roughness";
		inject_elements[5].basic_type_string = STRING(float);
		inject_elements[5].data = &roughness;

		inject_elements[6].name = "PBR light positions";
		inject_elements[6].basic_type_string = STRING(float3);
		inject_elements[6].data = pbr_light_pos;
		inject_elements[6].stream_type = Reflection::ReflectionStreamFieldType::Pointer;
		inject_elements[6].stream_capacity = 4;
		inject_elements[6].stream_size = 4;

		inject_elements[7].name = "PBR light colors";
		inject_elements[7].basic_type_string = STRING(ColorFloat);
		inject_elements[7].data = pbr_light_color;
		inject_elements[7].stream_type = Reflection::ReflectionStreamFieldType::Pointer;
		inject_elements[7].stream_capacity = 4;
		inject_elements[7].stream_size = 4;

		inject_elements[8].name = "PBR light ranges";
		inject_elements[8].basic_type_string = STRING(float);
		inject_elements[8].data = pbr_light_range;
		inject_elements[8].stream_type = Reflection::ReflectionStreamFieldType::Pointer;
		inject_elements[8].stream_capacity = 4;
		inject_elements[8].stream_size = 4;

		static bool diffuse_cube = false;
		static bool specular_cube = false;
		static float2 uv_tiling = { 10.0f, 10.0f };
		static float2 uv_offsets = { 0.0f, 0.0f };

		inject_elements[9].name = "Diffuse Cube";
		inject_elements[9].basic_type_string = STRING(bool);
		inject_elements[9].data = &diffuse_cube;

		inject_elements[10].name = "Specular Cube";
		inject_elements[10].basic_type_string = STRING(bool);
		inject_elements[10].data = &specular_cube;

		inject_elements[11].name = "UV tiling";
		inject_elements[11].basic_type_string = STRING(float2);
		inject_elements[11].data = &uv_tiling;

		inject_elements[12].name = "UV offsets";
		inject_elements[12].basic_type_string = STRING(float2);
		inject_elements[12].data = &uv_offsets;

		static Color tint = Color((unsigned char)255, 255, 255, 255);

		inject_elements[13].name = "Tint";
		inject_elements[13].basic_type_string = STRING(Color);
		inject_elements[13].data = &tint;

		static float environment_diffuse_factor = 1.0f;
		static float environment_specular_factor = 1.0f;
		
		inject_elements[14].name = "Environment diffuse";
		inject_elements[14].basic_type_string = STRING(float);
		inject_elements[14].data = &environment_diffuse_factor;

		inject_elements[15].name = "Environment specular";
		inject_elements[15].basic_type_string = STRING(float);
		inject_elements[15].data = &environment_specular_factor;

		editor_state.inject_data.ui_reflection = editor_state.ui_reflection;
		editor_state.inject_data.sections = Stream<InjectWindowSection>(section, std::size(section));
		editor_state.inject_window_name = "Inject Window";

		//MyStruct my_struct;
		//my_struct.boolean = false;
		//my_struct.characters = ToStream("Not too many characters.");
		//my_struct.double0 = 0.0;
		//my_struct.double3 = { 25.1, 26.66666, 10.03308 };
		//my_struct.float0 = -2.5f;
		//my_struct.float3 = { -10.023f, -249.607f, 102523.030f };
		//my_struct.float4 = { 21738128.9312f, 2312389.0f, -32185309.0f, 0.0f };
		//my_struct.integer0 = 523;
		//my_struct.integer1 = 10320;
		//my_struct.integer2 = -3295;
		//my_struct.stream_characters = ToStream(L"Wide characters. \\Interesting");
		//float float_values[64];
		//for (size_t index = 0; index < 64; index++) {
		//	float_values[index] = index;
		//}
		//my_struct.float_values = { float_values, 64 };
		//my_struct.ascii_string = "Does this work?";
		//my_struct.wide_char = L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets";

		//MyStruct new_struct;
		//memset(&new_struct, 0, sizeof(MyStruct));
		//void* allocation = malloc(10'000);
		//CapacityStream<void> memory_pool(allocation, 0, 10'000);
		////bool serialize_success = TextSerialize(editor_state.ui_reflection->reflection->GetType(STRING(MyStruct)), &my_struct, ToStream(L"C:\\Users\\Andrei\\C++\\MyFile.txt"));
		//bool deserialize_success = TextDeserialize(editor_state.ui_reflection->reflection->GetType(STRING(MyStruct)), &new_struct, memory_pool, ToStream(L"C:\\Users\\Andrei\\C++\\MyFile.txt"));

		//ResourceManagerTextureDesc texture_desc;
		//texture_desc.allocator = GetAllocatorPolymorphic(editor_state.GlobalMemoryManager());
		//ResourceView environment_map = resource_manager->LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ice_Lake\\Ice_Lake_Ref.hdr", &texture_desc);
		////ResourceView environment_map = resource_manager.LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ridgecrest_Road\\Ridgecrest_Road_Ref.hdr", texture_desc);
		//TextureCube converted_cube = ConvertTextureToCube(graphics, environment_map, DXGI_FORMAT_R16G16B16A16_FLOAT, { 1024, 1024 });
		//ResourceView converted_cube_view = graphics->CreateTextureShaderViewResource(converted_cube);

		//VertexBuffer cube_v_buffer;
		//IndexBuffer cube_v_index;
		//CreateCubeVertexBuffer(graphics, 30.0f, cube_v_buffer, cube_v_index);

		//TextureCube diffuse_environment = ConvertEnvironmentMapToDiffuseIBL(converted_cube_view, graphics, { 64, 64 }, 200);
		//ResourceView diffuse_view = graphics->CreateTextureShaderViewResource(diffuse_environment);

		//TextureCube specular_environment = ConvertEnvironmentMapToSpecularIBL(converted_cube_view, graphics, { 1024, 1024 }, 512);
		//ResourceView specular_view = graphics->CreateTextureShaderViewResource(specular_environment);
		//D3D11_TEXTURE2D_DESC specular_descriptor;
		//specular_environment.tex->GetDesc(&specular_descriptor);

		//Texture2D brdf_lut = CreateBRDFIntegrationLUT(graphics, { 512, 512 }, 256);
		//ResourceView brdf_lut_view = graphics->CreateTextureShaderViewResource(brdf_lut);

		//ConstantBuffer converted_cube_constants = graphics->CreateConstantBuffer(sizeof(Matrix));
		//
		//float specular_max_mip = (float)specular_descriptor.MipLevels - 1.0f;
		//ConstantBuffer environment_constants = Shaders::CreatePBRPixelEnvironmentConstant(graphics);

		//ConstantBuffer skybox_vertex_constant = Shaders::CreatePBRSkyboxVertexConstant(graphics);

		//cerberus_material.pixel_textures[5] = diffuse_view;
		//cerberus_material.pixel_textures[6] = specular_view;
		//cerberus_material.pixel_textures[7] = brdf_lut_view;

		//CoallescedMesh* cerberus_mesh = resource_manager->LoadCoallescedMesh(
		//	L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\trireme2material.glb", 
		//	{ ECS_RESOURCE_MANAGER_SHARED_RESOURCE }
		//);
		//GLTFThumbnail thumbnail = GLTFGenerateThumbnail(graphics, { 512, 512 }, &cerberus_mesh->mesh);
		//
		//GraphicsDevice* device = nullptr;
		//GraphicsContext* context = nullptr;
		// create device, front and back buffers, swap chain and rendering context
		//HRESULT device_result = D3D11CreateDevice(
		//	nullptr,
		//	D3D_DRIVER_TYPE_HARDWARE,
		//	nullptr,
		//	0,
		//	nullptr,
		//	0,
		//	D3D11_SDK_VERSION,
		//	&device,
		//	nullptr,
		//	&context
		//);

		//ResourceManagerTextureDesc texture_desc;
		//texture_desc.context = graphics->GetContext();
		//texture_desc.misc_flags = D3D11_RESOURCE_MISC_SHARED;
		//ResourceView texture_view = resource_manager->LoadTextureImplementation(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\brown_planks_03_diff_1k.jpg", &texture_desc);

		//Texture2D old_texture = GetResource(texture_view);
		//Texture2D new_texture = TransferGPUResource(old_texture, device);
		//IndexBuffer new_index_buffer = TransferGPUResource(cerberus_mesh->mesh.index_buffer, device);

		//ID3D11Resource* texture_resource = GetResource(texture_view);

		///*IDXGIKeyedMutex* mutex;
		//device_result = texture_resource->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&mutex);
		//device_result = mutex->AcquireSync(0, 0);*/

		//IDXGIResource* dxgi_resource;
		//device_result = texture_resource->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgi_resource);

		//HANDLE shared_handle;
		//device_result = dxgi_resource->GetSharedHandle(&shared_handle);

		//ID3D11Resource* tex_res;
		//device_result = device->OpenSharedResource(shared_handle, __uuidof(ID3D11Resource), (void**)&tex_res);

		///*ID3D11Texture2D* tex;
		//device_result = tex_res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);*/
		//ID3D11ShaderResourceView* view;
		//device_result = device->CreateShaderResourceView(tex_res, nullptr, &view);

		//device_result = cerberus_mesh->mesh.index_buffer.buffer->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgi_resource);
		//device_result = dxgi_resource->GetSharedHandle(&shared_handle);
		//ID3D11Buffer* buffer;
		//device_result = device->OpenSharedResource(shared_handle, __uuidof(ID3D11Buffer), (void**)&buffer);

		//UINT COUNTU = cerberus_mesh->mesh.vertex_buffers[0].buffer->AddRef();
		//COUNTU = cerberus_mesh->mesh.vertex_buffers[0].buffer->Release();

		//device_result = cerberus_mesh->mesh.vertex_buffers[0].buffer->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgi_resource);
		//device_result = dxgi_resource->GetSharedHandle(&shared_handle);
		//ID3D11Buffer* v_buffer;
		//device_result = device->OpenSharedResource(shared_handle, __uuidof(ID3D11Buffer), (void**)&v_buffer);

		//COUNTU = dxgi_resource->Release();
		//UINT COUNT = v_buffer->Release();
		//COUNT = cerberus_mesh->mesh.vertex_buffers[0].buffer->Release();

		//context->OMSetBlendState(graphics->m_blend_enabled.state, nullptr, 0);
		//context->PSSetShader(graphics->m_shader_helpers[0].pixel.shader, nullptr, 0);
		//context->VSSetShader(graphics->m_shader_helpers[0].vertex.shader, nullptr, 0);
		//context->IASetInputLayout(graphics->m_shader_helpers[0].input_layout.layout);
		//context->PSSetSamplers(0, 1, &graphics->m_shader_helpers[0].pixel_sampler.sampler);
		//context->PSSetShaderResources(0, 1, &view);
		//context->IASetIndexBuffer(buffer, DXGI_FORMAT_R32_UINT, 0);
		//UINT offsets[] = { 0, 0 };
		//context->IASetVertexBuffers(0, 1, &v_buffer, &cerberus_mesh->mesh.vertex_buffers[0].stride, offsets);

		//const size_t SIGNATURE_COUNT = 256;
		//const size_t COMPONENT_COUNT = 12;
		//Component components[SIGNATURE_COUNT * COMPONENT_COUNT];
		//ComponentSignature signatures[SIGNATURE_COUNT];
		//bool RESULTS[SIGNATURE_COUNT];
		//
		//size_t counter = 0;
		//for (size_t index = 0; index < SIGNATURE_COUNT; index++) {
		//	//signatures[index] = new ComponentSignature(components + COMPONENT_COUNT * index, COMPONENT_COUNT);
		//	signatures[index].indices = components + COMPONENT_COUNT * index;
		//	signatures[index].count = COMPONENT_COUNT;

		//	for (size_t subindex = 0; subindex < COMPONENT_COUNT; subindex++) {
		//		signatures[index].indices[subindex].value = counter % (2);
		//		counter++;
		//	}
		//}

		//bool has_signatures = true;
		//Component query_components[COMPONENT_COUNT];
		//ComponentSignature query(query_components, COMPONENT_COUNT);

		//for (size_t index = 0; index < COMPONENT_COUNT; index++) {
		//	//query_components[index] = components[rand() % (SIGNATURE_COUNT * COMPONENT_COUNT)];
		//	query_components[index].value = index;
		//}

		//timer.SetMarker();
		//for (size_t iteration = 0; iteration < 1024; iteration++) {
		//	for (size_t index = 0; index < SIGNATURE_COUNT; index++) {
		//		RESULTS[index] = HasComponents(query, signatures[index]);
		//		has_signatures &= RESULTS[index];
		//	}
		//}
		//size_t duration = timer.GetDurationSinceMarker_us();
		//ECS_STACK_CAPACITY_STREAM(char, output, 512);
		//function::ConvertIntToChars(output, has_signatures);
		//output.Add(' ');
		//function::ConvertIntToChars(output, duration);
		//output.Add(' ');
		//
		//VectorComponentSignature vector_query(query);
		//VectorComponentSignature vectors[SIGNATURE_COUNT];
		//for (size_t index = 0; index < SIGNATURE_COUNT; index++) {
		//	vectors[index].ConvertComponents(signatures[index]);
		//}

		//bool VECTOR_RESULTS[SIGNATURE_COUNT];
		//timer.SetMarker();
		//bool new_signature = true;

		//for (size_t iteration = 0; iteration < 1024; iteration++) {
		//	for (size_t index = 0; index < SIGNATURE_COUNT; index++) {
		//		VECTOR_RESULTS[index] = vectors[index].HasComponents(vector_query);
		//		//VECTOR_RESULTS[index] = VectorComponentSignatureHasComponents(vectors[index], vector_query, COMPONENT_COUNT);
		//		new_signature &= VECTOR_RESULTS[index];
		//	}
		//}
		//duration = timer.GetDurationSinceMarker_us();

		//for (size_t index = 0; index < SIGNATURE_COUNT; index++) {
		//	ECS_ASSERT(VECTOR_RESULTS[index] == RESULTS[index]);
		//}

		///*VECTOR_RESULTS[0] = vector_query.HasComponents(*vectors[348]);
		//VECTOR_RESULTS[1] = vector_query.HasComponents(*vectors[370]);*/
		//function::ConvertIntToChars(output, new_signature);
		//output.Add(' ');
		//function::ConvertIntToChars(output, duration);
		//output.Add('\n');
		//output[output.size] = '\0';
		//OutputDebugStringA(output.buffer);

		while (result == 0) {
			while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
				switch (message.message) {
				case WM_QUIT:
					result = -1;
					break;
				}
				TranslateMessage(&message);
				DispatchMessage(&message);
			}

			unsigned int frame_pacing = 0;

			static bool CAMERA_CHANGED = true;

			if (!IsIconic(hWnd)) {
				auto mouse_state = mouse.GetState();
				auto keyboard_state = keyboard.GetState();
				mouse.UpdateState();
				mouse.UpdateTracker();
				keyboard.UpdateState();
				keyboard.UpdateTracker();

				unsigned int VALUE = 0;

				unsigned int window_index = editor_state.ui_system->GetWindowFromName("Game");
				if (window_index == -1)
					window_index = 0;
				float aspect_ratio = editor_state.ui_system->m_windows[window_index].transform.scale.x / editor_state.ui_system->m_windows[window_index].transform.scale.y
					* graphics->m_window_size.x / graphics->m_window_size.y;

				//Shaders::SetPBRPixelEnvironmentConstant(environment_constants, graphics, { specular_max_mip, environment_diffuse_factor, environment_specular_factor });

				graphics->ClearBackBuffer(0.0f, 0.0f, 0.0f);
				const float colors[4] = { 0.3f, 0.6f, 0.95f, 1.0f };

				timer.SetMarker();

				float horizontal_rotation = 0.0f;
				float vertical_rotation = 0.0f;

				if (mouse_state->MiddleButton()) {
					float2 mouse_position = editor_state.ui_system->GetNormalizeMousePosition();
					float2 delta = editor_state.ui_system->GetMouseDelta(mouse_position);

					float3 right_vector = GetRightVector(camera.rotation);
					float3 up_vector = GetUpVector(camera.rotation);

					float factor = 10.0f;

					if (keyboard_state->IsKeyDown(HID::Key::LeftShift)) {
						factor = 2.5f;
					}

					VALUE = 4;

					camera.translation -= right_vector * float3::Splat(delta.x * factor) - up_vector * float3::Splat(delta.y * factor);
					CAMERA_CHANGED = true;
				}
				if (mouse_state->LeftButton()) {
					float factor = 75.0f;
					float2 mouse_position = editor_state.ui_system->GetNormalizeMousePosition();
					float2 delta = editor_state.ui_system->GetMouseDelta(mouse_position);

					if (keyboard_state->IsKeyDown(HID::Key::LeftShift)) {
						factor = 10.0f;
					}

					VALUE = 4;

					camera.rotation.x += delta.y * factor;
					camera.rotation.y += delta.x * factor;
					CAMERA_CHANGED = true;

					horizontal_rotation -= delta.x * factor;
					vertical_rotation -= delta.y * factor;
				}

				int scroll_delta = mouse_state->ScrollDelta();
				if (scroll_delta != 0) {
					float factor = 0.015f;

					VALUE = 4;

					if (keyboard_state->IsKeyDown(HID::Key::LeftShift)) {
						factor = 0.005f;
					}

					float3 forward_vector = GetForwardVector(camera.rotation);

					camera.translation += forward_vector * float3::Splat(scroll_delta * factor);
					CAMERA_CHANGED = true;
				}

				HID::MouseTracker* mouse_tracker = mouse.GetTracker();
				if (mouse_tracker->RightButton() == MBPRESSED || mouse_tracker->MiddleButton() == MBPRESSED || mouse_tracker->LeftButton() == MBPRESSED) {
					mouse.EnableRawInput();
				}
				else if (mouse_tracker->RightButton() == MBRELEASED || mouse_tracker->MiddleButton() == MBRELEASED || mouse_tracker->LeftButton() == MBRELEASED) {
					mouse.DisableRawInput();
				}

				//if (CAMERA_CHANGED) {
					//graphics->m_context->ClearDepthStencilView(editor_state.viewport_texture_depth.view, D3D11_CLEAR_DEPTH, 1.0f, 0);
					//graphics.m_context->ClearRenderTargetView(editor_state.viewport_render_target, colors);

				//	Matrix cube_matrix = MatrixTranspose(camera.GetProjectionViewMatrix());

				//	graphics->DisableDepth();
				//	graphics->DisableCulling();
				//	graphics->BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_TEXTURE_CUBE);
				//	graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				//	graphics->BindVertexBuffer(cube_v_buffer);
				//	graphics->BindIndexBuffer(cube_v_index);
				//	if (diffuse_cube) {
				//		graphics->BindPixelResourceView(diffuse_view);
				//	}
				//	else if (specular_cube) {
				//		graphics->BindPixelResourceView(specular_view);
				//	}
				//	else {
				//		graphics->BindPixelResourceView(converted_cube_view);
				//	}
				//	Shaders::SetPBRSkyboxVertexConstant(skybox_vertex_constant, graphics, camera.rotation, camera.projection);
				//	graphics->BindVertexConstantBuffer(skybox_vertex_constant);
				//	graphics->DrawIndexed(cube_v_index.count);
				//	graphics->EnableDepth();
				//	graphics->EnableCulling();

				//	graphics->BindVertexShader(PBR_vertex_shader);
				//	graphics->BindPixelShader(PBR_pixel_shader);
				//	graphics->BindInputLayout(PBR_layout);

				//	graphics->BindSamplerState(graphics->m_shader_helpers[0].pixel_sampler, 1);

				//	graphics->BindPixelResourceView(plank_texture);
				//	graphics->BindPixelResourceView(plank_normal_texture, 1);
				//	//graphics.BindPixelResourceView(plank_metallic, 2);
				//	graphics->BindPixelResourceView(plank_roughness, 3);
				//	graphics->BindPixelResourceView(plank_ao, 4);
				//	graphics->BindPixelResourceView(diffuse_view, 5);
				//	graphics->BindPixelResourceView(specular_view, 6);
				//	graphics->BindPixelResourceView(brdf_lut_view, 7);

				//	Shaders::SetCameraPosition(camera_position_buffer, graphics, camera.translation);

				//	ConstantBuffer vertex_constant_buffers[1];
				//	vertex_constant_buffers[0] = pbr_vertex_values;
				//	graphics->BindVertexConstantBuffers(Stream<ConstantBuffer>(vertex_constant_buffers, std::size(vertex_constant_buffers)));

				//	Shaders::SetDirectionalLight(directional_light, graphics, LIGHT_DIRECTION, LIGHT_INTENSITY);

				//	Shaders::SetPointLight(point_light, graphics, float3(sin(timer.GetDurationSinceMarker_ms() * 0.0001f) * 4.0f, 0.0f, 20.0f), 2.5f, 1.5f, ColorFloat(1.0f, 1.0f, 1.0f));
				//	ColorFloat spot_light_color = ColorFloat(3.0f, 3.0f, 3.0f)/* * cos(timer.GetDurationSinceMarker_ms() * 0.000001f)*/;
				//	Shaders::SetSpotLight(spot_light, graphics, float3(0.0f, 8.0f, 20.0f), float3(sin(timer.GetDurationSinceMarker_ms() * 0.0001f) * 0.5f, -1.0f, 0.0f), 15.0f, 22.0f, 15.0f, 2.0f, 2.0f, spot_light_color);

				//	float* normal_strength_data = (float*)graphics->MapBuffer(normal_strength_buffer.buffer);
				//	*normal_strength_data = normal_strength;
				//	graphics->UnmapBuffer(normal_strength_buffer.buffer);

				//	float2* _pbr_values = (float2*)graphics->MapBuffer(pbr_pixel_values.buffer);
				//	*_pbr_values = { metallic, roughness };
				//	graphics->UnmapBuffer(pbr_pixel_values.buffer);

				//	float4* _pbr_lights = (float4*)graphics->MapBuffer(pbr_lights.buffer);
				//	for (size_t index = 0; index < 4; index++) {
				//		_pbr_lights[0] = { pbr_light_pos[index].x, pbr_light_pos[index].y, pbr_light_pos[index].z, 0.0f };
				//		_pbr_lights++;
				//	}
				//	for (size_t index = 0; index < 4; index++) {
				//		_pbr_lights[0] = { pbr_light_color[index].red, pbr_light_color[index].green, pbr_light_color[index].blue, pbr_light_color[index].alpha };
				//		_pbr_lights++;
				//	}
				//	for (size_t index = 0; index < 4; index++) {
				//		_pbr_lights[0].x = pbr_light_range[index];
				//		_pbr_lights++;
				//	}
				//	graphics->UnmapBuffer(pbr_lights.buffer);

				//	Shaders::PBRPixelConstants pixel_constants;
				//	pixel_constants.tint = tint;
				//	pixel_constants.normal_strength = normal_strength;
				//	pixel_constants.metallic_factor = metallic;
				//	pixel_constants.roughness_factor = roughness;
				//	Shaders::SetPBRPixelConstants(pbr_pixel_values, graphics, pixel_constants);

				//	ConstantBuffer pixel_constant_buffer[5];
				//	pixel_constant_buffer[0] = camera_position_buffer;
				//	pixel_constant_buffer[1] = environment_constants;
				//	pixel_constant_buffer[2] = pbr_pixel_values;
				//	pixel_constant_buffer[3] = pbr_lights;
				//	pixel_constant_buffer[4] = directional_light;

				//	graphics->BindPixelConstantBuffers({ pixel_constant_buffer, std::size(pixel_constant_buffer) });

				//	camera.SetPerspectiveProjectionFOV(60.0f, aspect_ratio, 0.05f, 1000.0f);

				//	Matrix camera_matrix = camera.GetProjectionViewMatrix();

				//	Matrix world_matrices[1];
				//	for (size_t subindex = 0; subindex < 1; subindex++) {
				//		void* obj_ptr = graphics->MapBuffer(obj_buffer.buffer);
				//		float* reinter = (float*)obj_ptr;
				//		DirectX::XMMATRIX* reinterpretation = (DirectX::XMMATRIX*)obj_ptr;

				//		Matrix matrix = /*MatrixRotationZ(sin(timer.GetDurationSinceMarker_ms() * 0.0005f) * 0.0f) **/ MatrixRotationY(0.0f) * MatrixRotationX(0.0f)
				//			* MatrixTranslation(0.0f, 0.0f, 20.0f + subindex * 10.0f);
				//		Matrix world_matrix = matrix;
				//		world_matrices[subindex] = world_matrix;
				//		Matrix transpose = MatrixTranspose(matrix);
				//		transpose.Store(reinter);

				//		Matrix MVP_matrix = matrix * camera_matrix;
				//		matrix = MatrixTranspose(MVP_matrix);
				//		matrix.Store(reinter + 16);

				//		graphics->UnmapBuffer(obj_buffer.buffer);
				//		graphics->BindSamplerState(sampler);

				//		ECS_MESH_INDEX mapping[3];
				//		mapping[0] = ECS_MESH_POSITION;
				//		mapping[1] = ECS_MESH_NORMAL;
				//		mapping[2] = ECS_MESH_UV;

				//		Shaders::SetPBRVertexConstants(pbr_vertex_values, graphics, MatrixTranspose(world_matrices[subindex]), MatrixTranspose(MVP_matrix), uv_tiling, uv_offsets);

				//		graphics->BindMesh(normal_merged_mesh, Stream<ECS_MESH_INDEX>(mapping, std::size(mapping)));
				//		graphics->DrawIndexed(normal_merged_mesh.index_buffer.count);
				//	}
				//}

				graphics->BindRenderTargetViewFromInitialViews();

				//CAMERA_CHANGED = false;

				editor_state.Tick();

				/*size_t milliseconds = timer.GetDurationSinceMarker_ms();
				ECS_STACK_CAPACITY_STREAM(char, milliseconds_duration, 64);
				function::ConvertIntToChars(milliseconds_duration, milliseconds);
				milliseconds_duration[milliseconds_duration.size] = ' ';
				milliseconds_duration[milliseconds_duration.size + 1] = '\0';
				OutputDebugStringA(milliseconds_duration.buffer);*/

				frame_pacing = editor_state.ui_system->DoFrame();
				frame_pacing = std::max(frame_pacing, VALUE);

				//milliseconds = timer.GetDurationSinceMarker_ms();
				//milliseconds_duration.size = 0;
				////ECS_STACK_CAPACITY_STREAM(char, milliseconds_duration, 64);
				//function::ConvertIntToChars(milliseconds_duration, milliseconds);
				//milliseconds_duration[milliseconds_duration.size] = '\n';
				//milliseconds_duration[milliseconds_duration.size + 1] = '\0';
				//OutputDebugStringA(milliseconds_duration.buffer);

				graphics->SwapBuffers(0);
				mouse.SetPreviousPositionAndScroll();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_AMOUNT[frame_pacing]));
		}

		editor_state.task_manager->SleepUntilDynamicTasksFinish();
		//ChangeInspectorToNothing(&editor_state);
		DestroyGraphics(editor_state.Graphics());

		if (result == -1)
			return -1;
		else
			return message.wParam;
	}

	private:
		static LRESULT CALLBACK HandleMessageSetup(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept;
		static LRESULT CALLBACK HandleMessageForward(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept;
		LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept;

		//int width;
		//int height;
		HWND hWnd;
		ECSEngine::Timer timer;
		ECSEngine::HID::Mouse mouse;
		ECSEngine::HID::Keyboard keyboard;
		ECSEngine::Stream<HCURSOR> cursors;
		ECSEngine::CursorType current_cursor;

		// singleton that manages registering and unregistering the editor class
		class EditorClass {
		public:
			static LPCWSTR GetName() noexcept;
			static HINSTANCE GetInstance() noexcept;
			EditorClass() noexcept;
			~EditorClass();
		private:
			EditorClass(const EditorClass& ) = delete;
			EditorClass& operator = (const EditorClass&) = delete;
			static constexpr LPCWSTR editorClassName = L"Editor Window";
			static EditorClass editorInstance;
			HINSTANCE hInstance;
		};
}; // Editor

Editor::EditorClass Editor::EditorClass::editorInstance;
Editor::EditorClass::EditorClass() noexcept : hInstance( GetModuleHandle(nullptr))  {
	WNDCLASSEX editor_class = { 0 };
	editor_class.cbSize = sizeof(editor_class);
	editor_class.style = CS_OWNDC;
	editor_class.lpfnWndProc = HandleMessageSetup;
	editor_class.cbClsExtra = 0;
	editor_class.cbWndExtra = 0;
	editor_class.hInstance = GetInstance();
	editor_class.hIcon = static_cast<HICON>( LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 128, 128, 0 ));
	editor_class.hCursor = nullptr;
	editor_class.hbrBackground = nullptr;
	editor_class.lpszMenuName = nullptr;
	editor_class.lpszClassName = GetName();
	editor_class.hIconSm = static_cast<HICON>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32, 0));;
	
	// Set application thread DPI to system aware - we are using DX11 to do the scaling
	DPI_AWARENESS_CONTEXT context = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
	RegisterClassEx(&editor_class);
}
Editor::EditorClass::~EditorClass() {
	UnregisterClass(editorClassName, GetInstance());
}
HINSTANCE Editor::EditorClass::GetInstance() noexcept {
	return editorInstance.hInstance;
}
LPCWSTR Editor::EditorClass::GetName() noexcept {
	return editorClassName;
}

Editor::Editor(int _width, int _height, LPCWSTR name) noexcept : timer("Editor")
{
	// calculate window size based on desired client region
	RECT windowRegion;
	windowRegion.left = 0;
	windowRegion.right = _width;
	windowRegion.bottom = _height;
	windowRegion.top = 0;
	AdjustWindowRect(&windowRegion, WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU, FALSE);

	//width = _width;
	//height = _height;

	HCURSOR* cursor_stream = new HCURSOR[ECS_CURSOR_COUNT];

	// hInstance is null because these are predefined cursors
	cursors = ECSEngine::Stream<HCURSOR>(cursor_stream, ECS_CURSOR_COUNT);
	cursors[(unsigned int)ECSEngine::CursorType::AppStarting] = LoadCursor(NULL, IDC_APPSTARTING);
	cursors[(unsigned int)ECSEngine::CursorType::Cross] = LoadCursor(NULL, IDC_CROSS);
	cursors[(unsigned int)ECSEngine::CursorType::Default] = LoadCursor(NULL, IDC_ARROW);
	cursors[(unsigned int)ECSEngine::CursorType::Hand] = LoadCursor(NULL, IDC_HAND);
	cursors[(unsigned int)ECSEngine::CursorType::Help] = LoadCursor(NULL, IDC_HELP);
	cursors[(unsigned int)ECSEngine::CursorType::IBeam] = LoadCursor(NULL, IDC_IBEAM);
	cursors[(unsigned int)ECSEngine::CursorType::SizeAll] = LoadCursor(NULL, IDC_SIZEALL);
	cursors[(unsigned int)ECSEngine::CursorType::SizeEW] = LoadCursor(NULL, IDC_SIZEWE);
	cursors[(unsigned int)ECSEngine::CursorType::SizeNESW] = LoadCursor(NULL, IDC_SIZENESW);
	cursors[(unsigned int)ECSEngine::CursorType::SizeNS] = LoadCursor(NULL, IDC_SIZENS);
	cursors[(unsigned int)ECSEngine::CursorType::SizeNWSE] = LoadCursor(NULL, IDC_SIZENWSE);
	cursors[(unsigned int)ECSEngine::CursorType::Wait] = LoadCursor(NULL, IDC_WAIT);

	ChangeCursor(ECSEngine::CursorType::Default);

	// create window and get Handle
	hWnd = CreateWindow(
		EditorClass::GetName(),
		name,
		WS_OVERLAPPEDWINDOW /*^ WS_THICKFRAME*/,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRegion.right - windowRegion.left,
		windowRegion.bottom - windowRegion.top,
		nullptr,
		nullptr,
		EditorClass::GetInstance(),
		this
	);

	// Register raw mouse input
	RAWINPUTDEVICE raw_device = {};
	raw_device.usUsagePage = 0x01; // mouse page
	raw_device.usUsage = 0x02; // mouse usage
	raw_device.dwFlags = 0;
	raw_device.hwndTarget = nullptr;
	if (!RegisterRawInputDevices(&raw_device, 1, sizeof(raw_device))) {
		abort();
	}

	POINT cursor_pos = {};
	GetCursorPos(&cursor_pos);
	mouse.SetPosition(cursor_pos.x, cursor_pos.y);

	// show window since default is hidden
	ShowWindow(hWnd, SW_SHOWMAXIMIZED);
	UpdateWindow(hWnd);
	RECT rect;
	GetClientRect(hWnd, &rect);
}
Editor::~Editor() {
	delete[] cursors.buffer;
	DestroyWindow(hWnd);
}

LRESULT WINAPI Editor::HandleMessageSetup(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {

	if (message == WM_NCCREATE) {

		// extract pointer to window class from creation data
		const CREATESTRUCTW* const createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
		Editor* const editor_pointer = static_cast<Editor*>(createStruct->lpCreateParams);

		// set WinAPI managed user data to store pointer to editor class
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(editor_pointer));

		// set WinAPI procedure to forwarder
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Editor::HandleMessageForward));

		// forward message
		return editor_pointer->HandleMessageForward(hWnd, message, wParam, lParam);
	}
}

LRESULT WINAPI Editor::HandleMessageForward(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {

	// retrieve pointer to editor class
	Editor* editor = reinterpret_cast<Editor*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	//if (message == WM_SIZE) {
	//	float new_width = LOWORD(lParam);
	//	float new_height = HIWORD(lParam);
	//	if (new_width != 0.0f && new_height != 0.0f) {
	//		if (editor->graphics != nullptr) {
	//			editor->graphics->SetNewSize(hWnd, new_width, new_height);
	//			//std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//		}
	//	}
	//}

	return editor->HandleMessage(hWnd, message, wParam, lParam);
}

LRESULT Editor::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {

	switch (message) {
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_ACTIVATEAPP:
		keyboard.Procedure({ message, wParam, lParam });
		mouse.Procedure({ message, wParam, lParam });
		break;
	case WM_INPUT:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEHOVER:
		mouse.Procedure({ message, wParam, lParam }); 
		break;
	case WM_CHAR:
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
	case WM_SYSKEYDOWN:
		keyboard.Procedure({ message, wParam, lParam });
		break;
	case WM_SETCURSOR:
		if (cursors[(unsigned int)current_cursor] != GetCursor()) {
			SetCursor(cursors[(unsigned int)current_cursor]);
		}
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

ECSEngine::Application* ECSEngine::CreateApplication(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	return new Editor(ECS_TOOLS_UI_DESIGNED_WIDTH, ECS_TOOLS_UI_DESIGNED_HEIGHT, L"ECSEngine Editor");
}
ECSEngine::Application* ECSEngine::CreateApplication() {
	return new Editor(ECS_TOOLS_UI_DESIGNED_WIDTH, ECS_TOOLS_UI_DESIGNED_HEIGHT, L"ECSEngine Editor");
}