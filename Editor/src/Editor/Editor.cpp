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

#define ERROR_BOX_MESSAGE WM_USER + 1
#define ERROR_BOX_CODE -2
#define ECS_COMPONENT

using namespace ECSEngine;
using namespace ECSEngine::Tools;

#pragma comment(lib, "Shcore.lib")

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
		using namespace ECSEngine::containers;
		using namespace ECSEngine::Tools;
		
		EditorState editor_state;
		EditorStateInitialize(this, &editor_state, hWnd, mouse, keyboard);

		ResourceManager* resource_manager = editor_state.ResourceManager();
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

		//AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&memory_manager);
		//success = LoadMeshesFromGLTF(gltf_data, gltf_meshes, allocator, &ERROR_MESSAGE);
		///*GLTFMeshesToMeshes(&graphics, gltf_meshes, meshes, gltf_data.mesh_count);*/

		//Stream<PBRMaterial> materials(_materials, 0);
		//Stream<unsigned int> submesh_material_index(_submesh_material_index, 0);
		//success = LoadDisjointMaterialsFromGLTF(gltf_data, materials, submesh_material_index, allocator, &ERROR_MESSAGE);
		//GLTFMeshesToMeshes(&graphics, gltf_meshes, meshes, gltf_data.mesh_count);
		//memset(submesh_material_index.buffer, 0, sizeof(unsigned int) * gltf_data.mesh_count);
		//materials.size = 1;
		//Mesh merged_mesh = GLTFMeshesToMergedMesh(&graphics, gltf_meshes, _submeshes, _submesh_material_index, materials.size, gltf_data.mesh_count);
		//Mesh normal_merged_mesh = MeshesToSubmeshes(&graphics, Stream<Mesh>(meshes, gltf_data.mesh_count), _normal_submeshes);
		//FreeGLTFMeshes(gltf_meshes, gltf_data.mesh_count, allocator);
		//FreeGLTFFile(gltf_data);

		//PBRMaterial created_material = CreatePBRMaterialFromName(ToStream("Cerberus"), ToStream("Cerberus"), ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets"), allocator);
		//Material cerberus_material = PBRToMaterial(&resource_manager, created_material);
		//PBRMesh* cerberus = resource_manager.LoadPBRMesh(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\cerberus_textures.glb");
		//Material cerberus_material = PBRToMaterial(&resource_manager, cerberus->materials[0], ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets"));

		Stream<char> shader_source;

		VertexShader forward_lighting_v_shader = resource_manager->LoadVertexShaderImplementation(ECS_VERTEX_SHADER_SOURCE(ForwardLighting), &shader_source);
		PixelShader forward_lighting_p_shader = resource_manager->LoadPixelShaderImplementation(ECS_PIXEL_SHADER_SOURCE(ForwardLighting));
		PixelShader forward_lighting_no_normal_p_shader = resource_manager->LoadPixelShaderImplementation(ECS_PIXEL_SHADER_SOURCE(ForwardLightingNoNormal));
		InputLayout forward_lighting_layout = graphics->ReflectVertexShaderInput(forward_lighting_v_shader, shader_source);
		resource_manager->Deallocate(shader_source.buffer);
		
		ShaderCompileOptions options;
		ShaderMacro _macros[32];
		Stream<ShaderMacro> macros(_macros, 0);
		macros[0] = { "COLOR_TEXTURE", "" };
		macros[1] = { "ROUGHNESS_TEXTURE", "" };
		macros[2] = { "OCCLUSION_TEXTURE", "" };
		macros[3] = { "NORMAL_TEXTURE", "" };
		macros.size = 4;
		options.macros = macros;
		PixelShader PBR_pixel_shader = resource_manager->LoadPixelShaderImplementation(ECS_PIXEL_SHADER_SOURCE(PBR), nullptr, options);
		VertexShader PBR_vertex_shader = resource_manager->LoadVertexShaderImplementation(ECS_VERTEX_SHADER_SOURCE(PBR), &shader_source);
		InputLayout PBR_layout = graphics->ReflectVertexShaderInput(PBR_vertex_shader, shader_source);
		resource_manager->Deallocate(shader_source.buffer);

		ConstantBuffer obj_buffer = graphics->CreateConstantBuffer(sizeof(float) * 32);

		ConstantBuffer specular_factors = graphics->CreateConstantBuffer(sizeof(float) * 2);

		ConstantBuffer hemispheric_ambient_light = Shaders::CreateHemisphericConstantBuffer(graphics);
		ConstantBuffer directional_light = Shaders::CreateDirectionalLightBuffer(graphics);
		ConstantBuffer camera_position_buffer = Shaders::CreateCameraPositionBuffer(graphics);
		ConstantBuffer point_light = Shaders::CreatePointLightBuffer(graphics);
		ConstantBuffer spot_light = Shaders::CreateSpotLightBuffer(graphics);
		ConstantBuffer capsule_light = Shaders::CreateCapsuleLightBuffer(graphics);
		ConstantBuffer pbr_lights = graphics->CreateConstantBuffer(sizeof(float4) * 8 + sizeof(float4) * 4);
		ConstantBuffer pbr_pixel_values = Shaders::CreatePBRPixelConstants(graphics);
		ConstantBuffer pbr_vertex_values = Shaders::CreatePBRVertexConstants(graphics);

		Shaders::SetCapsuleLight(capsule_light, graphics, float3(0.0f, 0.0f, 20.0f), float3(0.0f, 1.0f, 0.0f), 10.0f, 1.0f, 2.0f, ColorFloat(50.0f, 50.0f, 50.0f));

		const ColorFloat COLORS[] = {
			{1.0f, 0, 0},
			{0, 1.0f, 0},
			{0, 0, 1.0f},
			{1.0f, 1.0f, 0},
			{1.0f, 0, 1.0f},
			{0, 1.0f, 1.0f}
		};
		ConstantBuffer index_color = graphics->CreateConstantBuffer(sizeof(ColorFloat) * 6, COLORS);

		D3D11_SAMPLER_DESC descriptor = {};
		descriptor.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		descriptor.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		descriptor.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		descriptor.Filter = D3D11_FILTER_ANISOTROPIC;
		descriptor.MaxAnisotropy = 16;
		SamplerState sampler = graphics->CreateSamplerState(descriptor);

		Camera camera;
		camera.translation = { 0.0f, 0.0f, 0.0f };
		//camera.SetPerspectiveProjectionFOV(45.0f, (float)width / (float)height, -1.0f, 1.0f);
		ResourceManagerTextureDesc plank_descriptor;
		//plank_descriptor.context = graphics.m_context.Get();
		plank_descriptor.usage = D3D11_USAGE_DEFAULT;
		//ResourceView plank_texture = resource_manager.LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\brown_planks_03_diff_1k.jpg", plank_descriptor);
		//ResourceView plank_normal_texture = resource_manager.LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\brown_planks_03_nor_dx_1k.jpg", plank_descriptor);
		//ResourceView plank_roughness = resource_manager.LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\brown_planks_03_rough_1k.jpg", plank_descriptor);
		////ResourceView plank_metallic = resource_manager.LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\brown_planks_03_diff_1k.jpg", plank_descriptor);
		//ResourceView plank_ao = resource_manager.LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\brown_planks_03_ao_1k.jpg", plank_descriptor);
		//ECS_ASSERT(plank_texture.view != nullptr && plank_normal_texture.view != nullptr && plank_roughness.view != nullptr && plank_ao.view != nullptr);

		MemoryManager debug_drawer_memory(5'000'000, 1024, 2'500'000, editor_state.GlobalMemoryManager());
		DebugDrawer debug_drawer(&debug_drawer_memory, resource_manager, 1);
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

		ResourceManagerTextureDesc texture_desc;
		texture_desc.allocator = GetAllocatorPolymorphic(editor_state.GlobalMemoryManager());
		ResourceView environment_map = resource_manager->LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ice_Lake\\Ice_Lake_Ref.hdr", texture_desc);
		//ResourceView environment_map = resource_manager.LoadTexture(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\Ridgecrest_Road\\Ridgecrest_Road_Ref.hdr", texture_desc);
		TextureCube converted_cube = ConvertTextureToCube(graphics, environment_map, DXGI_FORMAT_R16G16B16A16_FLOAT, { 1024, 1024 });
		ResourceView converted_cube_view = graphics->CreateTextureShaderViewResource(converted_cube);

		VertexBuffer cube_v_buffer;
		IndexBuffer cube_v_index;
		CreateCubeVertexBuffer(graphics, 30.0f, cube_v_buffer, cube_v_index);

		TextureCube diffuse_environment = ConvertEnvironmentMapToDiffuseIBL(converted_cube_view, graphics, { 64, 64 }, 200);
		ResourceView diffuse_view = graphics->CreateTextureShaderViewResource(diffuse_environment);

		TextureCube specular_environment = ConvertEnvironmentMapToSpecularIBL(converted_cube_view, graphics, { 1024, 1024 }, 512);
		ResourceView specular_view = graphics->CreateTextureShaderViewResource(specular_environment);
		D3D11_TEXTURE2D_DESC specular_descriptor;
		specular_environment.tex->GetDesc(&specular_descriptor);

		Texture2D brdf_lut = CreateBRDFIntegrationLUT(graphics, { 512, 512 }, 256);
		ResourceView brdf_lut_view = graphics->CreateTextureShaderViewResource(brdf_lut);

		ConstantBuffer converted_cube_constants = graphics->CreateConstantBuffer(sizeof(Matrix));
		
		float specular_max_mip = (float)specular_descriptor.MipLevels - 1.0f;
		ConstantBuffer environment_constants = Shaders::CreatePBRPixelEnvironmentConstant(graphics);

		ConstantBuffer skybox_vertex_constant = Shaders::CreatePBRSkyboxVertexConstant(graphics);

		/*cerberus_material.pixel_textures[5] = diffuse_view;
		cerberus_material.pixel_textures[6] = specular_view;
		cerberus_material.pixel_textures[7] = brdf_lut_view;*/

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

			EditorEvent editor_event;
			while (editor_state.event_queue.Pop(editor_event)) {
				EditorEventFunction event_function = (EditorEventFunction)editor_event.function;
				event_function(&editor_state, editor_event.data);
			}

			static bool CAMERA_CHANGED = true;

			if (!IsIconic(hWnd)) {
				auto mouse_state = mouse.GetState();
				auto keyboard_state = keyboard.GetState();
				mouse.UpdateState();
				mouse.UpdateTracker();
				keyboard.UpdateState();
				keyboard.UpdateTracker();

				unsigned int VALUE = 0;

				//unsigned int window_index = ui.GetWindowFromName("Game");
				//if (window_index == -1)
				//	window_index = 0;
				//float aspect_ratio = ui.m_windows[window_index].transform.scale.x / ui.m_windows[window_index].transform.scale.y * new_width / new_height;

				//Shaders::SetPBRPixelEnvironmentConstant(environment_constants, &graphics, { specular_max_mip, environment_diffuse_factor, environment_specular_factor });

				//graphics.ClearBackBuffer(0.0f, 0.0f, 0.0f);
				//const float colors[4] = { 0.3f, 0.6f, 0.95f, 1.0f };

				//if (mouse_state->MiddleButton()) {
				//	float2 mouse_position = ui.GetNormalizeMousePosition();
				//	float2 delta = ui.GetMouseDelta(mouse_position);

				//	float3 right_vector = GetRightVector(camera.rotation);
				//	float3 up_vector = GetUpVector(camera.rotation);

				//	float factor = 10.0f;

				//	if (keyboard_state->IsKeyDown(HID::Key::LeftShift)) {
				//		factor = 2.5f;
				//	}

				//	VALUE = 4;

				//	camera.translation -= right_vector * float3::Splat(delta.x * factor) - up_vector * float3::Splat(delta.y * factor);
				//	CAMERA_CHANGED = true;
				//}
				//if (mouse_state->RightButton()) {
				//	float factor = 75.0f;
				//	float2 mouse_position = ui.GetNormalizeMousePosition();
				//	float2 delta = ui.GetMouseDelta(mouse_position);

				//	if (keyboard_state->IsKeyDown(HID::Key::LeftShift)) {
				//		factor = 10.0f;
				//	}

				//	VALUE = 4;

				//	camera.rotation.x += delta.y * factor;
				//	camera.rotation.y += delta.x * factor;
				//	CAMERA_CHANGED = true;
				//}

				//int scroll_delta = mouse_state->ScrollDelta();
				//if (scroll_delta != 0) {
				//	float factor = 0.015f;

				//	VALUE = 4;

				//	if (keyboard_state->IsKeyDown(HID::Key::LeftShift)) {
				//		factor = 0.005f;
				//	}

				//	float3 forward_vector = GetForwardVector(camera.rotation);

				//	camera.translation += forward_vector * float3::Splat(scroll_delta * factor);
				//	CAMERA_CHANGED = true;
				//}

				//HID::MouseTracker* mouse_tracker = mouse.GetTracker();
				//if (mouse_tracker->RightButton() == MBPRESSED || mouse_tracker->MiddleButton() == MBPRESSED) {
				//	mouse.EnableRawInput();
				//}
				//else if (mouse_tracker->RightButton() == MBRELEASED || mouse_tracker->MiddleButton() == MBRELEASED) {
				//	mouse.DisableRawInput();
				//}

				//if (CAMERA_CHANGED) {
				//	graphics.m_context->ClearDepthStencilView(viewport_depth_view.view, D3D11_CLEAR_DEPTH, 1.0f, 0);
				//	//graphics.m_context->ClearRenderTargetView(viewport_render_view.target, colors);

				//	graphics.BindRenderTargetView(viewport_render_view, viewport_depth_view);

				//	Matrix cube_matrix = MatrixTranspose(camera.GetProjectionViewMatrix());

				//	graphics.DisableDepth();
				//	graphics.DisableCulling();
				//	graphics.BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_TEXTURE_CUBE);
				//	graphics.BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				//	graphics.BindVertexBuffer(cube_v_buffer);
				//	graphics.BindIndexBuffer(cube_v_index);
				//	if (diffuse_cube) {
				//		graphics.BindPixelResourceView(diffuse_view);
				//	}
				//	else if (specular_cube) {
				//		graphics.BindPixelResourceView(specular_view);
				//	}
				//	else {
				//		graphics.BindPixelResourceView(converted_cube_view);
				//	}
				//	Shaders::SetPBRSkyboxVertexConstant(skybox_vertex_constant, &graphics, camera.rotation, camera.projection);
				//	graphics.BindVertexConstantBuffer(skybox_vertex_constant);
				//	graphics.DrawIndexed(cube_v_index.count);
				//	graphics.EnableDepth();
				//	graphics.EnableCulling();

				//	graphics.BindVertexShader(PBR_vertex_shader);
				//	graphics.BindPixelShader(PBR_pixel_shader);
				//	graphics.BindInputLayout(PBR_layout);

				//	graphics.BindSamplerState(graphics.m_shader_helpers[0].pixel_sampler, 1);

				//	//graphics.BindPixelResourceView(plank_texture);
				//	//graphics.BindPixelResourceView(plank_normal_texture, 1);
				//	////graphics.BindPixelResourceView(plank_metallic, 2);
				//	//graphics.BindPixelResourceView(plank_roughness, 3);
				//	//graphics.BindPixelResourceView(plank_ao, 4);
				//	//graphics.BindPixelResourceView(diffuse_view, 5);
				//	//graphics.BindPixelResourceView(specular_view, 6);
				//	//graphics.BindPixelResourceView(brdf_lut_view, 7);

				//	Shaders::SetCameraPosition(camera_position_buffer, &graphics, camera.translation);

				//	ConstantBuffer vertex_constant_buffers[1];
				//	vertex_constant_buffers[0] = pbr_vertex_values;
				//	graphics.BindVertexConstantBuffers(Stream<ConstantBuffer>(vertex_constant_buffers, std::size(vertex_constant_buffers)));

				//	Shaders::SetDirectionalLight(directional_light, &graphics, LIGHT_DIRECTION, LIGHT_INTENSITY);

				//	Shaders::SetPointLight(point_light, &graphics, float3(sin(timer.GetDurationSinceMarker_ms() * 0.0001f) * 4.0f, 0.0f, 20.0f), 2.5f, 1.5f, ColorFloat(1.0f, 1.0f, 1.0f));
				//	ColorFloat spot_light_color = ColorFloat(3.0f, 3.0f, 3.0f)/* * cos(timer.GetDurationSinceMarker_ms() * 0.000001f)*/;
				//	Shaders::SetSpotLight(spot_light, &graphics, float3(0.0f, 8.0f, 20.0f), float3(sin(timer.GetDurationSinceMarker_ms() * 0.0001f) * 0.5f, -1.0f, 0.0f), 15.0f, 22.0f, 15.0f, 2.0f, 2.0f, spot_light_color);

				//	float* normal_strength_data = (float*)graphics.MapBuffer(normal_strength_buffer.buffer);
				//	*normal_strength_data = normal_strength;
				//	graphics.UnmapBuffer(normal_strength_buffer.buffer);

				//	float2* _pbr_values = (float2*)graphics.MapBuffer(pbr_pixel_values.buffer);
				//	*_pbr_values = { metallic, roughness };
				//	graphics.UnmapBuffer(pbr_pixel_values.buffer);

				//	float4* _pbr_lights = (float4*)graphics.MapBuffer(pbr_lights.buffer);
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
				//	graphics.UnmapBuffer(pbr_lights.buffer);

				//	Shaders::PBRPixelConstants pixel_constants;
				//	pixel_constants.tint = tint;
				//	pixel_constants.normal_strength = normal_strength;
				//	pixel_constants.metallic_factor = metallic;
				//	pixel_constants.roughness_factor = roughness;
				//	Shaders::SetPBRPixelConstants(pbr_pixel_values, &graphics, pixel_constants);

				//	ConstantBuffer pixel_constant_buffer[5];
				//	pixel_constant_buffer[0] = camera_position_buffer;
				//	pixel_constant_buffer[1] = environment_constants;
				//	pixel_constant_buffer[2] = pbr_pixel_values;
				//	pixel_constant_buffer[3] = pbr_lights;
				//	pixel_constant_buffer[4] = directional_light;

				//	graphics.BindPixelConstantBuffers({ pixel_constant_buffer, std::size(pixel_constant_buffer) });

				//	camera.SetPerspectiveProjectionFOV(60.0f, aspect_ratio, 0.05f, 1000.0f);

				//	Matrix camera_matrix = camera.GetProjectionViewMatrix();

				//	//Matrix world_matrices[1];
				//	//for (size_t subindex = 0; subindex < 1; subindex++) {
				//	//	void* obj_ptr = graphics.MapBuffer(obj_buffer.buffer);
				//	//	float* reinter = (float*)obj_ptr;
				//	//	DirectX::XMMATRIX* reinterpretation = (DirectX::XMMATRIX*)obj_ptr;

				//	//	Matrix matrix = /*MatrixRotationZ(sin(timer.GetDurationSinceMarker_ms() * 0.0005f) * 0.0f) **/ MatrixRotationY(0.0f) * MatrixRotationX(0.0f)
				//	//		* MatrixTranslation(0.0f, 0.0f, 20.0f + subindex * 10.0f);
				//	//	Matrix world_matrix = matrix;
				//	//	world_matrices[subindex] = world_matrix;
				//	//	Matrix transpose = MatrixTranspose(matrix);
				//	//	transpose.Store(reinter);

				//	//	Matrix MVP_matrix = matrix * camera_matrix;
				//	//	matrix = MatrixTranspose(MVP_matrix);
				//	//	matrix.Store(reinter + 16);

				//	//	graphics.UnmapBuffer(obj_buffer.buffer);
				//	//	graphics.BindSamplerState(sampler);

				//	//	ECS_MESH_INDEX mapping[3];
				//	//	mapping[0] = ECS_MESH_POSITION;
				//	//	mapping[1] = ECS_MESH_NORMAL;
				//	//	mapping[2] = ECS_MESH_UV;

				//	//	Shaders::SetPBRVertexConstants(pbr_vertex_values, &graphics, MatrixTranspose(world_matrices[subindex]), MatrixTranspose(MVP_matrix), uv_tiling, uv_offsets);

				//	//	graphics.BindMesh(normal_merged_mesh, Stream<ECS_MESH_INDEX>(mapping, std::size(mapping)));
				//	//	graphics.DrawIndexed(normal_merged_mesh.index_buffer.count);
				//	//}
				//}

				graphics->BindRenderTargetViewFromInitialViews();

				//CAMERA_CHANGED = false;

				editor_state.Tick();

				frame_pacing = editor_state.ui_system->DoFrame();
				frame_pacing = std::max(frame_pacing, VALUE);

				graphics->SwapBuffers(0);
				mouse.SetPreviousPositionAndScroll();
				editor_state.TaskManager()->ResetTaskAllocator();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_AMOUNT[frame_pacing]));
		}

		editor_state.TaskManager()->SleepUntilDynamicTasksFinish();

		DestroyGraphics(graphics);

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
		ECSEngine::containers::Stream<HCURSOR> cursors;
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
	cursors = ECSEngine::containers::Stream<HCURSOR>(cursor_stream, ECS_CURSOR_COUNT);
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