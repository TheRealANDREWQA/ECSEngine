// ECS_REFLECT
#include "editorpch.h"
#include "ECSEngine.h"
#include "../DrawFunction2.h"
#include "../DrawFunction.h"
#include "../../resource.h"
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
#include "ECSEngineBenchmark.h"
#include "ECSEngineMath.h"

#define ERROR_BOX_MESSAGE WM_USER + 1
#define ERROR_BOX_CODE -2

using namespace ECSEngine;
using namespace ECSEngine::Tools;

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "Shcore.lib")

class Editor : public ECSEngine::Application {
public:
	Editor(int width, int height, LPCWSTR name);
	Editor() {}
	~Editor();
	Editor(const Editor&) = delete;
	Editor& operator = (const Editor&) = delete;

public:

	void ChangeCursor(ECSEngine::ECS_CURSOR_TYPE type) override {
		if (type != current_cursor) {
			SetCursor(cursors[(unsigned int)type]);
			SetClassLong(hWnd, -12, (LONG)cursors[(unsigned int)type]);
			current_cursor = type;
		}
	}

	ECSEngine::ECS_CURSOR_TYPE GetCurrentCursor() const override {
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
			copy_count = copy_count > max_size - 1 ? max_size - 1 : copy_count;
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

	uint2 GetCursorPosition() const {
		return ECSEngine::OS::GetCursorPosition();
	}

	void SetCursorPosition(uint2 position) {
		ECSEngine::OS::SetCursorPosition(position);
	}

	void SetCursorPositionRelative(uint2 position) {
		ECSEngine::OS::SetCursorPositionRelative(GetOSWindowHandle(), position);
	}

	int Run() override {
		using namespace ECSEngine;
		using namespace ECSEngine::Tools;

		EditorState editor_state;
		EditorStateBaseInitialize(&editor_state, hWnd, &mouse, &keyboard);
		EditorStateInitialize(this, &editor_state, hWnd, &mouse, &keyboard);

		ResourceManager* resource_manager = editor_state.ui_resource_manager;
		Graphics* graphics = editor_state.UIGraphics();

		Hub(&editor_state);

		MSG message;
		BOOL result = 0;

		VertexBuffer vertex_buffer;
		IndexBuffer index_buffer;

		//PBRMaterial created_material = CreatePBRMaterialFromName("Cerberus"), ToStream("Cerberus"), ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets"), allocator;
		////PBRMaterial cerberus_material = PBRToMaterial(resource_manager, created_material, L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets");
		//PBRMesh* cerberus = resource_manager->LoadPBRMesh(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\cerberus_textures.glb");
		//PBRMaterial cerberus_material = PBRToMaterial(resource_manager, cerberus->materials[0], L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets");

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
			L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ram_Bronze Ram_BaseColor.jpg",
			L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ram_Bronze Ram_Metallic.jpg",
			L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ram_Bronze Ram_Roughness.jpg",
			L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ram_Bronze Ram_Normal.jpg",
			L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Graphics.h",
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
		//	L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\packed.bin",
		//	PackFunctor
		//);

		//ECS_FILE_HANDLE packed_file = -1;
		//ECS_FILE_STATUS_FLAGS status__ = OpenFile(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\packed.bin"), &packed_file, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_BINARY;
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

		//ECS_STACK_CAPACITY_STREAM(char, console_message, 512);
		//console_message.Copy("Duration: ");

		//timer.SetMarker();

		//ResourceManagerTextureDesc descriptor;
		//descriptor.context = graphics->GetContext();
		//descriptor.compression = ECS_TEXTURE_COMPRESSION_EX::ColorMap;
		////ResourceView view = editor_state.resource_manager->LoadTextureImplementation(L"Resources/4k2.jpg", &descriptor);

		////Stream<void> buffer = ReadWholeFileBinary(L"Resources/comp_BC7.dds");
		//
		//size_t duration = timer.GetDurationSinceMarker_ms();
		//function::ConvertIntToChars(console_message, duration);
		//console_message.Add('\n');
		//console_message.Add('\0');
		//OutputDebugStringA(console_message.buffer);

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

		static wchar_t WIDE_CHARS[256];
		static Stream<wchar_t> WIDE_CHAR_STREAM(WIDE_CHARS, 0);

		ConstantBuffer normal_strength_buffer = graphics->CreateConstantBuffer(sizeof(float));

		//MyStruct my_struct;
		//my_struct.boolean = false;
		//my_struct.characters = "Not too many characters.";
		//my_struct.double0 = 0.0;
		//my_struct.double3 = { 25.1, 26.66666, 10.03308 };
		//my_struct.float0 = -2.5f;
		//my_struct.float3 = { -10.023f, -249.607f, 102523.030f };
		//my_struct.float4 = { 21738128.9312f, 2312389.0f, -32185309.0f, 0.0f };
		//my_struct.integer0 = 523;
		//my_struct.integer1 = 10320;
		//my_struct.integer2 = -3295;
		//my_struct.stream_characters = L"Wide characters. \\Interesting";
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
		////bool serialize_success = TextSerialize(editor_state.ui_reflection->reflection->GetType(STRING(MyStruct)), &my_struct, L"C:\\Users\\Andrei\\C++\\MyFile.txt");
		//bool deserialize_success = TextDeserialize(editor_state.ui_reflection->reflection->GetType(STRING(MyStruct)), &new_struct, memory_pool, L"C:\\Users\\Andrei\\C++\\MyFile.txt");

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

		//CoalescedMesh* cerberus_mesh = resource_manager->LoadCoalescedMesh(
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

		/*char MEMORY[ECS_KB * 64];
		uintptr_t memory_ptr = (uintptr_t)MEMORY;

		const Reflection::ReflectionManager* reflection_manager = editor_state.ReflectionManager();
		ShaderMetadata metadata;

		char LINEAR_ALLOCATOR_MEMORY[ECS_KB * 16];
		LinearAllocator _linear_allocator(LINEAR_ALLOCATOR_MEMORY, ECS_KB * 16);
		AllocatorPolymorphic linear_allocator = GetAllocatorPolymorphic(editor_state.editor_allocator);

		metadata.name = "Name";
		metadata.path = L"Path";
		metadata.AddMacro("PogChamp", "New", linear_allocator);
		metadata.AddMacro("Hey there", "Totally", linear_allocator);

		ECS_SERIALIZE_CODE serialize_code = Serialize(reflection_manager, reflection_manager->GetType(STRING(ShaderMetadata)), &metadata, memory_ptr);
		ECS_ASSERT(serialize_code == ECS_SERIALIZE_OK);

		memory_ptr = (uintptr_t)MEMORY;
		ShaderMetadata deserialized_metadata;

		DeserializeOptions options;
		options.backup_allocator = linear_allocator;
		options.field_allocator = linear_allocator;
		ECS_DESERIALIZE_CODE deserialize_code = Deserialize(reflection_manager, reflection_manager->GetType(STRING(ShaderMetadata)), &deserialized_metadata,
			memory_ptr, &options);
		ECS_ASSERT(deserialize_code == ECS_DESERIALIZE_OK);

		uintptr_t database_ptr = memory_ptr;
		AssetDatabase database;
		database.SetAllocator(linear_allocator);

		database.AddShader(metadata, false);

		serialize_code = Serialize(reflection_manager, reflection_manager->GetType(STRING(AssetDatabase)), &database, memory_ptr);
		ECS_ASSERT(serialize_code == ECS_SERIALIZE_OK);

		AssetDatabase deserialized_database;
		deserialize_code = Deserialize(reflection_manager, reflection_manager->GetType(STRING(AssetDatabase)), &deserialized_database,
			database_ptr);
		ECS_ASSERT(deserialize_code == ECS_DESERIALIZE_OK);

		ECS_ASSERT(function::CompareStrings(deserialized_database.shader_metadata[0].macros[0].name, metadata.macros[0].name));
		ECS_ASSERT(function::CompareStrings(deserialized_database.shader_metadata[0].macros[0].definition, metadata.macros[0].definition));
		ECS_ASSERT(function::CompareStrings(deserialized_database.shader_metadata[0].macros[1].name, metadata.macros[1].name));
		ECS_ASSERT(function::CompareStrings(deserialized_database.shader_metadata[0].macros[1].definition, metadata.macros[1].definition));

		ECS_ASSERT(BelongsToAllocator(linear_allocator, deserialized_metadata.name.buffer));
		ECS_ASSERT(BelongsToAllocator(linear_allocator, deserialized_metadata.path.buffer));
		ECS_ASSERT(BelongsToAllocator(linear_allocator, deserialized_metadata.macros.buffer));
		ECS_ASSERT(BelongsToAllocator(linear_allocator, deserialized_metadata.macros[0].name));
		ECS_ASSERT(BelongsToAllocator(linear_allocator, deserialized_metadata.macros[0].definition));
		ECS_ASSERT(BelongsToAllocator(linear_allocator, deserialized_metadata.macros[1].name));
		ECS_ASSERT(BelongsToAllocator(linear_allocator, deserialized_metadata.macros[1].definition));

		Deallocate(linear_allocator, deserialized_metadata.name.buffer);
		Deallocate(linear_allocator, deserialized_metadata.path.buffer);
		Deallocate(linear_allocator, deserialized_metadata.macros.buffer);
		Deallocate(linear_allocator, deserialized_metadata.macros[0].name);
		Deallocate(linear_allocator, deserialized_metadata.macros[0].definition);
		Deallocate(linear_allocator, deserialized_metadata.macros[1].name);
		Deallocate(linear_allocator, deserialized_metadata.macros[1].definition);*/

		//Stream<char> names[] = {
		//	"SearchBytes",
		//	"FindFirstToken",
		//	"Std",
		//	"CString",
		//	"Loop"
		//};

		//typedef unsigned short t;
		//BenchmarkState benchmark_state(editor_state.EditorAllocator(), 5, nullptr, names);
		//benchmark_state.options.element_size = sizeof(t);
		//benchmark_state.options.max_step_count = 18;
		////benchmark_state.options.timed_run = 50;

		//while (benchmark_state.KeepRunning()) {
		//	Stream<void> iteration_buffer = benchmark_state.GetIterationBuffer();

		//	Stream<t> ts = { iteration_buffer.buffer, iteration_buffer.size / sizeof(t) };
		//	//function::MakeSequence(ts);
		//	char* it = (char*)iteration_buffer.buffer;
		//	it[iteration_buffer.size - 1] = '\0';
		//	for (size_t index = 0; index < iteration_buffer.size - 1; index++) {
		//		if (it[index] > 1) {
		//			it[index] = 'a';
		//		}
		//		else {
		//			it[index] = 'b';
		//		}
		//	}
		//	//t ptr = (t)ts.buffer;
		//	t ptr = -1;

		//	while (benchmark_state.Run()) {
		//		Stream<void> current_buffer = benchmark_state.GetCurrentBuffer();
		//		size_t indexu = function::SearchBytes(current_buffer.buffer, current_buffer.size / sizeof(ptr), (size_t)ptr, sizeof(ptr));
		//		benchmark_state.DoNotOptimize(indexu);
		//	}

		//	while (benchmark_state.Run()) {
		//		Stream<void> current_buffer = benchmark_state.GetCurrentBuffer();
		//		auto resultu = function::FindFirstToken(Stream<char>(current_buffer.buffer, current_buffer.size), { &ptr, sizeof(ptr) });
		//		benchmark_state.DoNotOptimize(resultu.buffer == nullptr ? -1 : resultu.size);
		//	}

		//	while (benchmark_state.Run()) {
		//		Stream<void> current_buffer = benchmark_state.GetCurrentBuffer();
		//		std::string_view view = { (const char*)current_buffer.buffer, current_buffer.size };
		//		auto resultuu = view.find((const char*)&ptr, sizeof(ptr));
		//		benchmark_state.DoNotOptimize(resultuu);
		//	}

		//	char null_terminated[16];
		//	memcpy(null_terminated, &ptr, sizeof(ptr));
		//	null_terminated[sizeof(ptr)] = '\0';
		//	while (benchmark_state.Run()) {
		//		Stream<void> current_buffer = benchmark_state.GetCurrentBuffer();
		//		const char* c_string = (const char*)current_buffer.buffer;
		//		const char* result = strstr(c_string, null_terminated);
		//		benchmark_state.DoNotOptimize((size_t)result);
		//	}

		//	while (benchmark_state.Run()) {
		//		Stream<void> current_buffer = benchmark_state.GetCurrentBuffer();

		//		Stream<t> ts = { current_buffer.buffer, current_buffer.size / sizeof(t) };
		//		size_t subindex = 0;
		//		for (size_t i = 0; i < ts.size; i++) {
		//			if (ts[i] == ptr) {
		//				subindex = i;
		//				break;
		//			}
		//		}
		//		benchmark_state.DoNotOptimize(subindex);
		//	}
		//}

		//ECS_STACK_CAPACITY_STREAM(char, bench_string, ECS_KB * 64);
		//benchmark_state.GetString(bench_string, false);
		//bench_string[bench_string.size] = '\0';
		//OutputDebugStringA(bench_string.buffer);

		//ResourceManagerTextureDesc texture_desc;
		//texture_desc.misc_flags = ECS_GRAPHICS_MISC_SHARED | ECS_GRAPHICS_MISC_GENERATE_MIPS;
		//texture_desc.context = graphics->GetContext();
		//ResourceView view = resource_manager->LoadTexture(L"Resources/FileText.png", &texture_desc);

		//Graphics graphics_copy(graphics);
		//VertexBuffer buffer = graphics->CreateVertexBuffer(sizeof(float3), 1000, false, ECS_GRAPHICS_USAGE_DEFAULT, ECS_GRAPHICS_CPU_ACCESS_NONE, ECS_GRAPHICS_MISC_SHARED);
		//VertexBuffer new_buffer = TransferGPUResource(buffer, graphics_copy.GetDevice());

		//graphics->BindVertexBuffer(buffer);
		//graphics_copy.BindVertexBuffer(new_buffer);

		//ResourceView new_view = TransferGPUView(view, graphics_copy.GetDevice());
		//graphics_copy.BindPixelResourceView(new_view);
		////graphics_copy.BindSamplerState(graphics_copy.m_shader_helpers[0].pixel_sampler);
		//PixelShader pixel_shader = graphics_copy.m_shader_helpers[0].pixel;

		//IDXGIResource* dxgi_resource;
		//HRESULT resultssss = pixel_shader.shader->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgi_resource);
		//graphics_copy.BindPixelShader(pixel_shader);

		DebugDrawer debug_drawer;
		MemoryManager debug_drawer_allocator = DebugDrawer::DefaultAllocator(editor_state.GlobalMemoryManager());
		debug_drawer.Initialize(&debug_drawer_allocator, editor_state.UIResourceManager(), 1);

		// TESTING STUFF
		//// Also create the instanced framebuffers
		//Texture2DDescriptor instanced_framebuffer_descriptor;
		//instanced_framebuffer_descriptor.format = ECS_GRAPHICS_FORMAT_R32_UINT;
		//instanced_framebuffer_descriptor.bind_flag = ECS_GRAPHICS_BIND_RENDER_TARGET;
		//instanced_framebuffer_descriptor.mip_levels = 1;
		//instanced_framebuffer_descriptor.size = graphics->GetWindowSize();
		//Texture2D instanced_framebuffer_texture = graphics->CreateTexture(&instanced_framebuffer_descriptor);
		//RenderTargetView RENDER_TARGET = graphics->CreateRenderTargetView(instanced_framebuffer_texture);

		//Texture2DDescriptor instanced_depth_stencil_descriptor;
		//instanced_depth_stencil_descriptor.format = ECS_GRAPHICS_FORMAT_D32_FLOAT;
		//instanced_depth_stencil_descriptor.bind_flag = ECS_GRAPHICS_BIND_DEPTH_STENCIL;
		//instanced_depth_stencil_descriptor.mip_levels = 1;
		//instanced_depth_stencil_descriptor.size = graphics->GetWindowSize();
		//Texture2D instanced_depth_texture = graphics->CreateTexture(&instanced_depth_stencil_descriptor);
		//DepthStencilView DEPTH_STENCIL = graphics->CreateDepthStencilView(instanced_depth_texture);

		while (true) {
			auto run_application = [&](char application_quit_value) {
				while (result == 0 && application_quit == application_quit_value) {
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
						unsigned int VALUE = 0;

						graphics->ClearBackBuffer(0.0f, 0.0f, 0.0f);

						timer.SetMarker();

						//if (CAMERA_CHANGED) {
							//graphics->m_context->ClearDepthStencilView(editor_state.viewport_texture_depth.view, D3D11_CLEAR_DEPTH, 1.0f, 0);
							//graphics.m_context->ClearRenderTargetView(editor_state.viewport_render_target, colors);

						//	Matrix cube_matrix = MatrixGPU(camera.GetProjectionViewMatrix());

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
						//		Matrix transpose = MatrixGPU(matrix);
						//		transpose.Store(reinter);

						//		Matrix MVP_matrix = matrix * camera_matrix;
						//		matrix = MatrixGPU(MVP_matrix);
						//		matrix.Store(reinter + 16);

						//		graphics->UnmapBuffer(obj_buffer.buffer);
						//		graphics->BindSamplerState(sampler);

						//		ECS_MESH_INDEX mapping[3];
						//		mapping[0] = ECS_MESH_POSITION;
						//		mapping[1] = ECS_MESH_NORMAL;
						//		mapping[2] = ECS_MESH_UV;

						//		Shaders::SetPBRVertexConstants(pbr_vertex_values, graphics, MatrixGPU(world_matrices[subindex]), MatrixGPU(MVP_matrix), uv_tiling, uv_offsets);

						//		graphics->BindMesh(normal_merged_mesh, Stream<ECS_MESH_INDEX>(mapping, std::size(mapping)));
						//		graphics->DrawIndexed(normal_merged_mesh.index_buffer.count);
						//	}
						//}

						graphics->BindRenderTargetViewFromInitialViews();

						editor_state.Tick();

						frame_pacing = editor_state.ui_system->DoFrame();
						frame_pacing = std::max(frame_pacing, VALUE);

						// Refresh the graphics object since it might be changed
						graphics = editor_state.UIGraphics();

						//CoalescedMesh* trireme = editor_state.UIResourceManager()->LoadCoalescedMesh<true>(L"C:\\Users\\Andrei\\DivideEtImpera\\Assets\\trireme2_should_be_fine.glb");
						//const Matrix CAMERA_MATRIX = MatrixPerspectiveFOV(60.0f, 16.0f / 10.0f, 0.035f, 1000.0f);
						//debug_drawer.UpdateCameraMatrix(CAMERA_MATRIX);
						//float3 TRANSLATION = { 0.0f, 0.0f, 5.0f };
						////debug_drawer.DrawSphere(TRANSLATION, 0.2f, ECS_COLOR_GREEN);
						////graphics->DisableWireframe();

						//debug_drawer.DrawRectangle(
						//	{ 1.0f, 0.0f, 1.0f },
						//	{ 0.0f, 1.0f, 1.0f },
						//	AxisXColor(),
						//	{ false, true }
						//);

						//debug_drawer.DrawRectangle(
						//	{ 0.0f, 1.0f, 1.0f },
						//	{ 1.0f, 0.0f, 1.0f },
						//	AxisYColor(),
						//	{ false, true }
						//);
						//debug_drawer.DrawAll(1.0f);

						//HighlightObjectElement elements[2];
						//elements[0].is_submesh = false;
						//elements[0].mesh = &trireme->mesh;
						//elements[0].gpu_mvp_matrix = MatrixMVPToGPU(MatrixTranslation(TRANSLATION), MatrixIdentity(), MatrixScale({0.2f, 0.2f, 0.2f}), CAMERA_MATRIX);

						//elements[1] = elements[0];
						//elements[1].gpu_mvp_matrix = MatrixMVPToGPU(MatrixTranslation(TRANSLATION + float3(0.0f, 0.5f, 0.0f)), MatrixIdentity(), MatrixScale({ 0.2f, 0.2f, 0.2f }), CAMERA_MATRIX);
						//HighlightObject(graphics, ECS_COLOR_RED, { elements, 1 });
						//HighlightObject(graphics, ECS_COLOR_MAGENTA, { elements + 1, 1 });

						/*GenerateInstanceFramebufferElement generate_element;
						generate_element.base.is_submesh = false;
						generate_element.base.mesh = &trireme->mesh;
						generate_element.base.gpu_mvp_matrix = MatrixMVPToGPU(MatrixTranslation(TRANSLATION), MatrixIdentity(), MatrixScale({ 0.2f, 0.2f, 0.2f }), CAMERA_MATRIX);
						generate_element.instance_index = 100000;
						generate_element.pixel_thickness = 10;

						GenerateInstanceFramebuffer(graphics, { &generate_element, 1 }, RENDER_TARGET, DEPTH_STENCIL);

						unsigned int instance_index = GetInstanceFromFramebuffer(graphics, RENDER_TARGET, { 1156, 787 });
						ECS_FORMAT_TEMP_STRING(CONSOLE_MESSAGE, "Instance index: {#}", instance_index);*/

						bool removed = graphics->SwapBuffers(0);
						if (removed) {
							__debugbreak();
						}

						mouse.Update();
						keyboard.Update();
					}

					std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_AMOUNT[frame_pacing]));
				}
			};

			run_application(0);

			EditorStateApplicationQuit(&editor_state, &application_quit);

			run_application(-1);
			if (application_quit == 1) {
				break;
			}

			application_quit = 0;
		}

		editor_state.task_manager->SleepUntilDynamicTasksFinish();
		unsigned int sandbox_count = editor_state.sandboxes.size;

		for (size_t index = 0; index < sandbox_count; index++) {
			DestroySandbox(&editor_state, 0);
		}
		DestroyGraphics(editor_state.RuntimeGraphics());
		DestroyGraphics(editor_state.UIGraphics());

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
		ECSEngine::Mouse mouse;
		ECSEngine::Keyboard keyboard;
		ECSEngine::Stream<HCURSOR> cursors;
		ECSEngine::ECS_CURSOR_TYPE current_cursor;
		char application_quit;

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

//ECS_OPTIMIZE_END;

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

Editor::Editor(int _width, int _height, LPCWSTR name)
{
	timer.SetNewStart();
	application_quit = 0;

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
	cursors = Stream<HCURSOR>(cursor_stream, ECS_CURSOR_COUNT);
	cursors[(unsigned int)ECS_CURSOR_APP_STARTING] = LoadCursor(NULL, IDC_APPSTARTING);
	cursors[(unsigned int)ECS_CURSOR_CROSS] = LoadCursor(NULL, IDC_CROSS);
	cursors[(unsigned int)ECS_CURSOR_DEFAULT] = LoadCursor(NULL, IDC_ARROW);
	cursors[(unsigned int)ECS_CURSOR_HAND] = LoadCursor(NULL, IDC_HAND);
	cursors[(unsigned int)ECS_CURSOR_HELP] = LoadCursor(NULL, IDC_HELP);
	cursors[(unsigned int)ECS_CURSOR_IBEAM] = LoadCursor(NULL, IDC_IBEAM);
	cursors[(unsigned int)ECS_CURSOR_SIZE_ALL] = LoadCursor(NULL, IDC_SIZEALL);
	cursors[(unsigned int)ECS_CURSOR_SIZE_EW] = LoadCursor(NULL, IDC_SIZEWE);
	cursors[(unsigned int)ECS_CURSOR_SIZE_NESW] = LoadCursor(NULL, IDC_SIZENESW);
	cursors[(unsigned int)ECS_CURSOR_SIZE_NS] = LoadCursor(NULL, IDC_SIZENS);
	cursors[(unsigned int)ECS_CURSOR_SIZE_NWSE] = LoadCursor(NULL, IDC_SIZENWSE);
	cursors[(unsigned int)ECS_CURSOR_WAIT] = LoadCursor(NULL, IDC_WAIT);

	ChangeCursor(ECS_CURSOR_DEFAULT);

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

	ECS_ASSERT(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE));
	GetWindowRect(hWnd, &windowRegion);
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
	return 0;
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
		application_quit = -1;
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