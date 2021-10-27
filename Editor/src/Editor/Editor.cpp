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
#include "../UI/FileExplorer.h"
#include "EditorEvent.h"
#include "../Modules/Module.h"
#include "../UI/InspectorData.h"
#include "EntryPoint.h"

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

	int Run() override {
		using namespace ECSEngine;
		using namespace ECSEngine::containers;
		using namespace ECSEngine::Tools;
#if 1
		GlobalMemoryManager global_memory_manager(150'000'000, 256, 50'000'000);
		GraphicsDescriptor graphics_descriptor;
		RECT window_rectangle;
		GetClientRect(hWnd, &window_rectangle);

		DEVICE_SCALE_FACTOR monitor_scaling;
#ifdef EDITOR_GET_MONITOR_SCALE
		HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONULL);
		HRESULT monitor_scaling_success = GetScaleFactorForMonitor(monitor, &monitor_scaling);
#else
		monitor_scaling = (DEVICE_SCALE_FACTOR)150;
#endif

		//SystemParametersInfo

		LONG width = window_rectangle.right - window_rectangle.left;
		LONG height = window_rectangle.bottom - window_rectangle.top;

		unsigned int new_width = static_cast<unsigned int>(width * monitor_scaling / 100.0f);
		unsigned int new_height = static_cast<unsigned int>(height * monitor_scaling / 100.0f);
#if 0
		new_width = 2560;
		new_height = 1537;
#endif

		void* debug_allocation = global_memory_manager.Allocate(sizeof(unsigned int) * 1024);
		MemoryManager memory_manager(100'000'000, 1024, 10'000'000, &global_memory_manager);
		memory_manager.SetDebugBuffer(debug_allocation);

		graphics_descriptor.window_size = { new_width, new_height };
		graphics_descriptor.gamma_corrected = false;
		graphics_descriptor.allocator = &memory_manager;
		Graphics graphics = Graphics(hWnd, &graphics_descriptor);

		GraphicsTexture2DDescriptor viewport_texture_descriptor;
		viewport_texture_descriptor.size = graphics_descriptor.window_size;
		viewport_texture_descriptor.bind_flag = static_cast<D3D11_BIND_FLAG>(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
		viewport_texture_descriptor.mip_levels = 1u;
		//viewport_texture_descriptor.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		Texture2D viewport_texture = graphics.CreateTexture(&viewport_texture_descriptor);
		RenderTargetView viewport_render_view = graphics.CreateRenderTargetView(viewport_texture);

		viewport_texture_descriptor.bind_flag = D3D11_BIND_DEPTH_STENCIL;
		viewport_texture_descriptor.format = DXGI_FORMAT_D32_FLOAT;
		Texture2D viewport_depth_texture = graphics.CreateTexture(&viewport_texture_descriptor);
		DepthStencilView viewport_depth_view = graphics.CreateDepthStencilView(viewport_depth_texture);

		TaskManager task_manager(std::thread::hardware_concurrency(), &memory_manager, 32);
		SystemManager system_manager = SystemManager(&task_manager, &memory_manager);
		World editor_world = World(&global_memory_manager, nullptr, &system_manager, nullptr);
		task_manager.m_world = &editor_world;
		ResourceManager resource_manager(&memory_manager, &graphics, std::thread::hardware_concurrency(), "Resources\\");

		task_manager.ChangeDynamicWrapperMode(TaskManagerWrapper::CountTasks);

		LinearAllocator linear_allocator = LinearAllocator(global_memory_manager.Allocate(1'000), 1'000);
		LinearAllocator* temp_allocators[] = { 
			&linear_allocator, 
			&linear_allocator,
			&linear_allocator, 
			&linear_allocator, 
			&linear_allocator, 
			&linear_allocator, 
			&linear_allocator,
			&linear_allocator
		};
		MemoryManager* temp_memory_managers[] = {
			&memory_manager,
			&memory_manager,
			&memory_manager,
			&memory_manager,
			&memory_manager,
			&memory_manager,
			&memory_manager,
			&memory_manager
		};
		task_manager.ReserveTasks(1);

		auto sleep_task = [](unsigned int thread_index, World* world, void* data) {
			world->task_manager->SleepThread(thread_index);
			world->task_manager->DecrementThreadTaskIndex(thread_index);
		};

		ThreadTask wait_task = { sleep_task, nullptr };
		task_manager.SetTask(wait_task, 0);
		task_manager.CreateThreads(temp_allocators, temp_memory_managers);

		resource_manager.InitializeDefaultTypes();

		ResizableMemoryArena resizable_arena = ResizableMemoryArena(
			&global_memory_manager, 
#if 1
			UI_RESIZABLE_ARENA_INITIAL_MEMORY, 
			UI_RESIZABLE_ARENA_ALLOCATOR_INITIAL_COUNT, 
			UI_RESIZABLE_ARENA_INITIAL_BLOCK_COUNT,
			UI_RESIZABLE_ARENA_RESERVE_MEMORY,
			UI_RESIZABLE_ARENA_ALLOCATOR_RESERVE_COUNT,
			UI_RESIZABLE_ARENA_RESERVE_BLOCK_COUNT
#else
			1'000'000,
			8,
			256,
			250'000,
			4,
			256
#endif
		);

		mouse.AttachToProcess({ hWnd });
		mouse.SetAbsoluteMode();
		keyboard = HID::Keyboard(&global_memory_manager);

		UISystem ui(
			this,
#ifdef ECS_TOOLS_UI_MEMORY_ARENA
			& arena,
#else
#ifdef ECS_TOOLS_UI_RESIZABLE_MEMORY_ARENA
			&resizable_arena,
#else
			&memory_manager,
#endif
#endif
			&keyboard,
			&mouse,
			&graphics,
			&resource_manager,
			&task_manager,
#if 1
			FONT_PATH,
			FONT_DESCRIPTOR_PATH,
#else
			L"Fontv5.tiff",
			"FontDescription.txt",

#endif
			uint2(width, height),
			&global_memory_manager
		);

		ui.BindWindowHandler(WindowHandler, WindowHandlerInitializer, sizeof(ECSEngine::Tools::UIDefaultWindowHandler));

		Reflection::ReflectionManager reflection_manager(&memory_manager);
		reflection_manager.CreateFolderHierarchy(L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src");
		reflection_manager.CreateFolderHierarchy(L"C:\\Users\\Andrei\\C++\\ECSEngine\\Editor\\src");
		const char* error_message = nullptr;
		wchar_t faulty_path[256];
		bool success = reflection_manager.ProcessFolderHierarchy((unsigned int)0, error_message, faulty_path);
		success = reflection_manager.ProcessFolderHierarchy((unsigned int)1, error_message, faulty_path);

		UIReflectionDrawer ui_reflection(&resizable_arena, &reflection_manager);

		EditorState editor_state;

		editor_state.task_manager = &task_manager;
		HubData hub_data;
		hub_data.projects.Initialize(&memory_manager, 0, EDITOR_HUB_PROJECT_CAPACITY);
		hub_data.projects.size = 0;
		hub_data.ui_reflection = &ui_reflection;
		ProjectFile project_file;
		project_file.source_dll_name.Initialize(&resizable_arena, 0, 64);
		project_file.project_name.Initialize(&resizable_arena, 0, 64);
		project_file.path.Initialize(&resizable_arena, 0, 256);

		UISpriteTexture viewport_sprite_texture;
	
		//viewport_sprite_texture = graphics.CreateTextureShaderView(viewport_texture, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0, 1);
		viewport_sprite_texture = graphics.CreateTextureShaderView(viewport_texture, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1);

		editor_state.hub_data = &hub_data;
		editor_state.project_file = &project_file;
		editor_state.ui_reflection = &ui_reflection;
		editor_state.ui_system = &ui;
		editor_state.viewport_texture = &viewport_sprite_texture;
		editor_state.editor_allocator = &memory_manager;

		MemoryManager console_memory_manager = MemoryManager(1'000'000, 256, 1'000'000, &global_memory_manager);
		Console console = Console(&console_memory_manager, &task_manager, CONSOLE_RELATIVE_DUMP_PATH);
		editor_state.console = &console;
		console.AddSystemFilterString(EDITOR_CONSOLE_SYSTEM_NAME);
		console.AddSystemFilterString("Animation");
		console.AddSystemFilterString("Rendering");
		console.AddSystemFilterString("Audio");
		console.AddSystemFilterString("Physics");

		InitializeModuleConfigurations(&editor_state);

		FileExplorerData file_explorer_data;
		InitializeFileExplorer(&file_explorer_data, &memory_manager);
		editor_state.file_explorer_data = &file_explorer_data;

		EditorEvent editor_events[EDITOR_EVENT_QUEUE_CAPACITY];
		editor_state.event_queue.InitializeFromBuffer(editor_events, EDITOR_EVENT_QUEUE_CAPACITY);

		ProjectModules project_modules(&memory_manager, 1);
		editor_state.project_modules = &project_modules;

		TaskGraph project_task_graph(&memory_manager);
		editor_state.project_task_graph = &project_task_graph;

		InspectorData inspector_data;
		editor_state.inspector_data = &inspector_data;

		World scene_worlds[EDITOR_SCENE_BUFFERING_COUNT];

		editor_state.active_world = 0;
		editor_state.worlds.InitializeFromBuffer(scene_worlds, 3, 3);

		ResetProjectGraphicsModule(&editor_state);

		Hub(&editor_state);

		MSG message;
		BOOL result = 0;

		VertexBuffer vertex_buffer;
		IndexBuffer index_buffer;

		ECS_TEMP_ASCII_STRING(ERROR_MESSAGE, 256);
		GLTFData gltf_data = LoadGLTFFile(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets\\trireme2material.glb", &ERROR_MESSAGE);
		GLTFMesh gltf_meshes[128];
		Mesh meshes[128];
		PBRMaterial _materials[128];

		AllocatorPolymorphic allocator = { &memory_manager, AllocatorType::MemoryManager, AllocationType::SingleThreaded };
		success = LoadMeshesFromGLTF(gltf_data, gltf_meshes, allocator, &ERROR_MESSAGE);
		GLTFMeshesToMeshes(&graphics, gltf_meshes, meshes, gltf_data.mesh_count);

		Stream<PBRMaterial> materials(_materials, 0);
		success = LoadDisjointMaterialsFromGLTF(gltf_data, materials, allocator, &ERROR_MESSAGE);
		FreeGLTFMeshes(gltf_meshes, gltf_data.mesh_count, allocator);
		FreeGLTFFile(gltf_data);

		PBRMaterial created_material = CreatePBRMaterialFromName(ToStream("brown_planks"), ToStream("brown_planks_03"), ToStream(L"C:\\Users\\Andrei\\ECSEngineProjects\\Assets"), allocator);

		CreateCubeVertexBuffer(&graphics, 0.25f, vertex_buffer, index_buffer);

		ShaderFromSourceOptions compile_options;
		VertexShader cube_shader = graphics.CreateVertexShaderFromSource(ToStream(L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Vertex\\StaticMesh.hlsl"), compile_options);
		PixelShader pixel_shader = graphics.CreatePixelShaderFromSource(ToStream(L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Pixel\\StaticMesh.hlsl"), compile_options);
		InputLayout layout = graphics.ReflectVertexShaderInput(cube_shader, L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Vertex\\StaticMesh.hlsl");

		VertexShader forward_lighting_v_shader = graphics.CreateVertexShaderFromSource(ToStream(L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Vertex\\ForwardLighting.hlsl"), compile_options);
		PixelShader forward_lighting_p_shader = graphics.CreatePixelShaderFromSource(ToStream(L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Pixel\\ForwardLighting.hlsl"), compile_options);
		InputLayout forward_lighting_layout = graphics.ReflectVertexShaderInput(forward_lighting_v_shader, L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Vertex\\ForwardLighting.hlsl");
		
		VertexShader debug_v_shader = graphics.CreateVertexShaderFromSource(ToStream(L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Vertex\\DebugDraw.hlsl"), compile_options);
		InputLayout debug_layout = graphics.ReflectVertexShaderInput(debug_v_shader, L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Vertex\\DebugDraw.hlsl");

		ConstantBuffer obj_buffer = graphics.CreateConstantBuffer(sizeof(float) * 32);

		ConstantBuffer specular_factors = graphics.CreateConstantBuffer(sizeof(float) * 2);

		ConstantBuffer hemispheric_ambient_light = Shaders::CreateHemisphericConstantBuffer(&graphics);
		ConstantBuffer directional_light = Shaders::CreateDirectionalLightBuffer(&graphics);
		ConstantBuffer camera_position_buffer = Shaders::CreateCameraPositionBuffer(&graphics);
		ConstantBuffer point_light = Shaders::CreatePointLightBuffer(&graphics);
		ConstantBuffer spot_light = Shaders::CreateSpotLightBuffer(&graphics);
		ConstantBuffer capsule_light = Shaders::CreateCapsuleLightBuffer(&graphics);

		Shaders::SetDirectionalLight(directional_light, &graphics, { 0.0f, -1.0f, 0.0f }, ColorFloat(0.5f, 0.5f, 0.5f, 1.0f));

		float* _specular_factors = (float*)graphics.MapBuffer(specular_factors.buffer);
		_specular_factors[0] = 100.0f;
		_specular_factors[1] = 1.25f;
		graphics.UnmapBuffer(specular_factors.buffer);

		Shaders::SetHemisphericConstants(hemispheric_ambient_light, &graphics, ColorFloat(0.0f, 0.0f, 0.0f), ColorFloat(0.25f, 0.25f, 0.25f));

		Shaders::SetCapsuleLight(capsule_light, &graphics, float3(0.0f, 0.0f, 20.0f), float3(0.0f, 1.0f, 0.0f), 10.0f, 1.0f, 2.0f, ColorFloat(50.0f, 50.0f, 50.0f));

		const ColorFloat COLORS[] = {
			{1.0f, 0, 0},
			{0, 1.0f, 0},
			{0, 0, 1.0f},
			{1.0f, 1.0f, 0},
			{1.0f, 0, 1.0f},
			{0, 1.0f, 1.0f}
		};
		ConstantBuffer index_color = graphics.CreateConstantBuffer(sizeof(ColorFloat) * 6, COLORS);

		D3D11_SAMPLER_DESC descriptor = {};
		descriptor.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		descriptor.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		descriptor.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		descriptor.Filter = D3D11_FILTER_ANISOTROPIC;
		descriptor.MaxAnisotropy = 16;
		SamplerState sampler = graphics.CreateSamplerState(descriptor);

		Camera camera;
		camera.translation = { 0.0f, 0.0f, 0.0f };
		//camera.SetPerspectiveProjectionFOV(45.0f, (float)width / (float)height, -1.0f, 1.0f);
		ResourceManagerTextureDesc plank_descriptor;
		plank_descriptor.context = graphics.m_context.Get();
		plank_descriptor.usage = D3D11_USAGE_DEFAULT;
		ResourceView plank_texture = resource_manager.LoadTexture(L"C:\\Users\\Andrei\\Blender\\BlenderTextures\\Trireme\\textures\\brown_planks_03_diff_1k.jpg", plank_descriptor);

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

			if (!IsIconic(hWnd)) {
				auto mouse_state = mouse.GetState();
				auto keyboard_state = keyboard.GetState();
				mouse.UpdateState();
				mouse.UpdateTracker();
				keyboard.UpdateState();
				keyboard.UpdateTracker();

				graphics.ClearBackBuffer(0.0f, 0.0f, 0.0f);
				const float colors[4] = { 0.2f, 0.5f, 0.8f, 1.0f };
				graphics.m_context->ClearDepthStencilView(viewport_depth_view.view, D3D11_CLEAR_DEPTH, 1.0f, 0);
				graphics.m_context->ClearRenderTargetView(viewport_render_view.target, colors);

				graphics.BindRenderTargetView(viewport_render_view, viewport_depth_view);

				graphics.BindVertexShader(forward_lighting_v_shader);
				graphics.BindPixelShader(forward_lighting_p_shader);
				graphics.BindInputLayout(forward_lighting_layout);

				graphics.BindPixelResourceView(plank_texture);

				if (mouse_state->MiddleButton()) {
					float2 mouse_position = ui.GetNormalizeMousePosition();
					float2 delta = ui.GetMouseDelta(mouse_position);

					float3 right_vector = GetRightVector(camera.rotation);
					float3 up_vector = GetUpVector(camera.rotation);

					float factor = 10.0f;

					if (keyboard_state->IsKeyDown(HID::Key::LeftShift)) {
						factor = 2.5f;
					}

					camera.translation -= right_vector * float3::Splat(delta.x * factor) - up_vector * float3::Splat(delta.y * factor);
				}
				if (mouse_state->RightButton()) {
					float factor = 30.0f;
					float2 mouse_position = ui.GetNormalizeMousePosition();
					float2 delta = ui.GetMouseDelta(mouse_position);

					if (keyboard_state->IsKeyDown(HID::Key::LeftShift)) {
						factor = 7.5f;
					}

					camera.rotation.x += delta.y * factor;
					camera.rotation.y += delta.x * factor;
				}

				int scroll_delta = mouse_state->ScrollDelta();
				if (scroll_delta != 0) {
					float factor = 0.015f;

					if (keyboard_state->IsKeyDown(HID::Key::LeftShift)) {
						factor = 0.005f;
					}

					float3 forward_vector = GetForwardVector(camera.rotation);

					camera.translation += forward_vector * float3::Splat(scroll_delta * factor);
				}

				unsigned int window_index = ui.GetWindowFromName("Game");
				if (window_index == -1)
					window_index = 0;


				float ____value = ui.m_windows[window_index].transform.scale.x / ui.m_windows[window_index].transform.scale.y * new_width / new_height;
				camera.SetPerspectiveProjectionFOV(60.0f, ____value, 0.05f, 1000.0f);

				void* obj_ptr = graphics.MapBuffer(obj_buffer.buffer);
				float* reinter = (float*)obj_ptr;
				DirectX::XMMATRIX* reinterpretation = (DirectX::XMMATRIX*)obj_ptr;

				Matrix matrix = MatrixRotationZ(sin(timer.GetDurationSinceMarker_ms() * 0.0005f) * 5.0f) * MatrixRotationY(0.0f) * MatrixRotationX(0.0f)
					* MatrixTranslation(0.0f, 0.0f, 20.0f);
				Matrix transpose = MatrixTranspose(matrix);

				/*DirectX::XMMATRIX directx_matrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixRotationZ(DegToRad(0.0f))
					* DirectX::XMMatrixRotationY(0.0f) * DirectX::XMMatrixRotationX(0.0f) * DirectX::XMMatrixTranslation(0.0f, 0.0f, 20.0f));
				memcpy(reinter, &directx_matrix, sizeof(directx_matrix));*/
				transpose.Store(reinter);
				Matrix camera_matrix = camera.GetProjectionViewMatrix();

				//DirectX::XMMATRIX directx_camera_matrix = DirectX::XMMatrixTranslation(-camera.translation.x, -camera.translation.y, -camera.translation.z) 
				//	* DirectX::XMMatrixRotationZ(DegToRad(-camera.rotation.z)) * DirectX::XMMatrixRotationY(DegToRad(-camera.rotation.y)) * DirectX::XMMatrixRotationX(DegToRad(-camera.rotation.x))
				//	* DirectX::XMMatrixPerspectiveFovLH(DegToRad(60.0f), ____value, 0.05f, 1000.0f);
				//
				//DirectX::XMMATRIX inverted_directx = DirectX::XMMatrixTranspose(DirectX::XMMatrixTranspose(directx_matrix) * directx_camera_matrix);
				//
				//memcpy(reinter + 16, &inverted_directx, sizeof(inverted_directx));
				matrix = MatrixTranspose(matrix * camera_matrix);
				matrix.Store(reinter + 16);	

				graphics.UnmapBuffer(obj_buffer.buffer);

				/*float3* light_pos_data = (float3*)graphics.MapBuffer(light_buffer.buffer);
				*light_pos_data = { sin(timer.GetDuration_ms() * 0.0005f) * 0.0f, 1.0f, 20.0f };
				graphics.UnmapBuffer(light_buffer.buffer);*/

				Shaders::SetCameraPosition(camera_position_buffer, &graphics, camera.translation);

				ConstantBuffer vertex_constant_buffers[2];
				vertex_constant_buffers[0] = obj_buffer;
				vertex_constant_buffers[1] = camera_position_buffer;
				graphics.BindVertexConstantBuffers(Stream<ConstantBuffer>(vertex_constant_buffers, std::size(vertex_constant_buffers)));

				Shaders::SetPointLight(point_light, &graphics, float3(sin(timer.GetDurationSinceMarker_ms() * 0.0001f) * 4.0f, 0.0f, 20.0f), 2.5f, 1.5f, ColorFloat(1.0f, 1.0f, 1.0f));
				Shaders::SetSpotLight(spot_light, &graphics, float3(0.0f, 8.0f, 20.0f), float3(0.5f, -1.0f, 0.0f), 15.0f, 22.0f, 15.0f, 2.0f, 2.0f, ColorFloat(11.0f, 11.0f, 11.0f));

				ConstantBuffer pixel_constant_buffers[6];
				pixel_constant_buffers[0] = hemispheric_ambient_light;
				pixel_constant_buffers[1] = directional_light;
				pixel_constant_buffers[2] = specular_factors;
				pixel_constant_buffers[3] = point_light;
				pixel_constant_buffers[4] = spot_light;
				pixel_constant_buffers[5] = capsule_light;

				graphics.BindPixelConstantBuffers(Stream<ConstantBuffer>(pixel_constant_buffers, std::size(pixel_constant_buffers)));

				/*graphics.BindVertexBuffer(vertex_buffer);
				graphics.BindIndexBuffer(index_buffer);
				graphics.BindTopology({ D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST });*/

				graphics.BindSamplerState(sampler);
				for (size_t index = 0; index < gltf_data.mesh_count; index++) {
					ECS_MESH_INDEX mapping[3];
					mapping[0] = ECS_MESH_POSITION;
					mapping[1] = ECS_MESH_UV;
					mapping[2] = ECS_MESH_NORMAL;
					graphics.BindMesh(meshes[index], Stream<ECS_MESH_INDEX>(mapping, 3));

					graphics.DrawIndexed(meshes[index].index_buffer.count);
				}
				/*graphics.BindMesh(meshes[60]);
				graphics.DrawIndexed(meshes[60].index_buffer.count);*/

				//obj_ptr = graphics.MapBuffer(obj_buffer.buffer);
				//reinterpretation = (DirectX::XMMATRIX*)obj_ptr;
				//matrix = MatrixRotationY(60.0f * sin(timer.GetDurationSinceMarker_ms() * .0005f)) * MatrixRotationX(45.0f) * MatrixTranslation(sin(timer.GetDurationSinceMarker_ms() * 0.0005f), 0.0f, 2.0f);
				////camera.translation = {cos(timer.GetDurationSinceMarker_ms() * 0.001f), 0.0f, sin(timer.GetDurationSinceMarker_ms() * 0.005f)};
				//camera_matrix = camera.GetProjectionViewMatrix();
				//matrix = MatrixTranspose(matrix * camera_matrix);
				//matrix.Store((float*)reinterpretation);
				////*reinterpretation = DirectX::XMMatrixTranspose(
				////	/*DirectX::XMMatrixRotationZ(timer.GetDurationSinceMarker_ms() * 0.0005f) * */
				////	DirectX::XMMatrixRotationY(DegToRad(60.0f)) * 
				////	DirectX::XMMatrixTranslation(0.0f, 0.0f, 2.0f /*+ sin(timer.GetDurationSinceMarker_ms() * 0.005f)*/) *
				////	DirectX::XMMatrixPerspectiveLH(ui.m_windows[window_index].transform.scale.x, ui.m_windows[window_index].transform.scale.y * (float)height / width /*/ ui.m_windows[window_index].transform.scale.x*/, 0.5f, 10.0f)
				////);			

				//graphics.UnmapBuffer(obj_buffer.buffer);

				//graphics.DrawIndexed(36);

				graphics.BindRenderTargetViewFromInitialViews();

				frame_pacing = ui.DoFrame();

				graphics.SwapBuffers(0);
				mouse.SetPreviousPositionAndScroll();
				task_manager.ResetTaskAllocator();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_AMOUNT[frame_pacing]));
		}

		task_manager.SleepUntilDynamicTasksFinish();

		if (result == -1)
			return -1;
		else
			return message.wParam;
#endif
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
	case WM_INPUT :
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