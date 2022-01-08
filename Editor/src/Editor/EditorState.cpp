#include "editorpch.h"
#include "EditorState.h"
#include "../UI/DirectoryExplorer.h"
#include "../UI/ModuleExplorer.h"
#include "../UI/FileExplorer.h"
#include "ECSEngineApplicationUtilities.h"
#include "EditorParameters.h"
#include "EditorEvent.h"
#include "../Modules/ModuleConfigurationGroup.h"
#include "../Modules/Module.h"

using namespace ECSEngine;

void TickPendingTasks(EditorState* editor_state) {
	// If there are pending tasks left to add and the flag is not set
	if (editor_state->pending_background_tasks.GetSize() > 0 && !function::HasFlag(editor_state->flags, EDITOR_STATE_DO_NOT_ADD_TASKS)) {
		EditorState::BackgroundTask background_task;
		while (editor_state->pending_background_tasks.Pop(background_task)) {
			editor_state->task_manager->AddDynamicTaskAndWake(background_task.task, background_task.data_size);
		}
	}
}

void EditorStateProjectTick(EditorState* editor_state) {
	DirectoryExplorerTick(editor_state);
	ModuleExplorerTick(editor_state);
	FileExplorerTick(editor_state);

	TickPendingTasks(editor_state);
}

void EditorStateHubTick(EditorState* editor_state) {
	TickPendingTasks(editor_state);
}

void EditorStateAddBackgroundTask(EditorState* editor_state, ECSEngine::ThreadTask task, size_t task_data_size)
{
	if (!function::HasFlag(editor_state->flags, EDITOR_STATE_DO_NOT_ADD_TASKS)) {
		editor_state->task_manager->AddDynamicTaskAndWake(task, task_data_size);
	}
	else {
		editor_state->pending_background_tasks.Push({ task, task_data_size });
	}
}

bool EditorStateDoNotAddBackgroundTasks(EditorState* editor_state)
{
	return function::HasFlag(editor_state->flags, EDITOR_STATE_DO_NOT_ADD_TASKS);
}

void EditorStateInitialize(Application* application, EditorState* editor_state, HWND hWnd, HID::Mouse& mouse, HID::Keyboard& keyboard)
{
	// Create every single member using new for easier class construction
	editor_state->editor_tick = TickPendingTasks;

	GlobalMemoryManager* global_memory_manager = new GlobalMemoryManager(GLOBAL_MEMORY_COUNT, 512, GLOBAL_MEMORY_RESERVE_COUNT);
	Graphics* graphics = (Graphics*)malloc(sizeof(Graphics));
	CreateGraphicsForProcess(graphics, hWnd, global_memory_manager);

	MemoryManager* editor_allocator = new MemoryManager(100'000'000, 4096, 50'000'000, global_memory_manager);

	GraphicsTexture2DDescriptor viewport_texture_descriptor;
	viewport_texture_descriptor.size = graphics->m_window_size;
	viewport_texture_descriptor.bind_flag = static_cast<D3D11_BIND_FLAG>(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	viewport_texture_descriptor.mip_levels = 1u;
	//viewport_texture_descriptor.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	Texture2D viewport_texture = graphics->CreateTexture(&viewport_texture_descriptor);
	RenderTargetView viewport_render_view = graphics->CreateRenderTargetView(viewport_texture);

	UISpriteTexture* viewport_sprite_texture = (UISpriteTexture*)malloc(sizeof(UISpriteTexture));

	//*viewport_sprite_texture = graphics->CreateTextureShaderView(viewport_texture, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0, 1);
	*viewport_sprite_texture = graphics->CreateTextureShaderView(viewport_texture, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1);

	viewport_texture_descriptor.bind_flag = D3D11_BIND_DEPTH_STENCIL;
	viewport_texture_descriptor.format = DXGI_FORMAT_D32_FLOAT;
	Texture2D viewport_depth_texture = graphics->CreateTexture(&viewport_texture_descriptor);
	DepthStencilView viewport_depth_view = graphics->CreateDepthStencilView(viewport_depth_texture);

	TaskManager* editor_task_manager = (TaskManager*)malloc(sizeof(TaskManager));
	*editor_task_manager = TaskManager(std::thread::hardware_concurrency(), editor_allocator, 32);

#if 1
	MemoryManager* system_manager_allocator = (MemoryManager*)malloc(sizeof(MemoryManager)); 
	*system_manager_allocator = DefaultSystemManagerAllocator(global_memory_manager);

	SystemManager* system_manager = (SystemManager*)malloc(sizeof(SystemManager));
	*system_manager = SystemManager(editor_task_manager, system_manager_allocator);
#endif

	World* dummy_world = (World*)malloc(sizeof(World));
	*dummy_world = World(global_memory_manager, nullptr, system_manager, nullptr);

	// The task wrappers use the 
	editor_task_manager->m_world = nullptr;

	unsigned int thread_count = std::thread::hardware_concurrency();

	MemoryManager* ui_resource_manager_allocator = (MemoryManager*)malloc(sizeof(MemoryManager));
	*ui_resource_manager_allocator = DefaultResourceManagerAllocator(global_memory_manager);
	ResourceManager* ui_resource_manager = (ResourceManager*)malloc(sizeof(ResourceManager));
	*ui_resource_manager = ResourceManager(ui_resource_manager_allocator, graphics, thread_count);

	MemoryManager* resource_manager_allocator = (MemoryManager*)malloc(sizeof(MemoryManager));
	*resource_manager_allocator = DefaultResourceManagerAllocator(global_memory_manager);

	ResourceManager* resource_manager = (ResourceManager*)malloc(sizeof(ResourceManager));
	*resource_manager = ResourceManager(resource_manager_allocator, graphics, thread_count);

	editor_task_manager->ChangeDynamicWrapperMode(TaskManagerWrapper::CountTasks);
	editor_task_manager->ReserveTasks(1);

	auto sleep_task = [](unsigned int thread_index, World* world, void* data) {
		TaskManager* task_manager = (TaskManager*)data;
		task_manager->SleepThread(thread_index);
		task_manager->DecrementThreadTaskIndex(thread_index);
	};

	ThreadTask wait_task = { sleep_task, editor_task_manager };
	editor_task_manager->SetTask(wait_task, 0);
	editor_task_manager->CreateThreads();

	resource_manager->InitializeDefaultTypes();
	ui_resource_manager->InitializeDefaultTypes();

	ResizableMemoryArena* resizable_arena = (ResizableMemoryArena*)malloc(sizeof(ResizableMemoryArena));
	*resizable_arena = DefaultUISystemAllocator(global_memory_manager);

	mouse.AttachToProcess({ hWnd });
	mouse.SetAbsoluteMode();
	keyboard = HID::Keyboard(global_memory_manager);

	UISystem* ui = (UISystem*)malloc(sizeof(UISystem));
	new (ui) UISystem(
		application,
		resizable_arena,
		&keyboard,
		&mouse,
		graphics,
		ui_resource_manager,
		editor_task_manager,
#if 1
		FONT_PATH,
		FONT_DESCRIPTOR_PATH,
#else
		L"Fontv5.tiff",
		L"FontDescription.txt",

#endif
		graphics->m_window_size,
		global_memory_manager
	);

	ui->BindWindowHandler(WindowHandler, WindowHandlerInitializer, sizeof(ECSEngine::Tools::UIDefaultWindowHandler));

	Reflection::ReflectionManager* editor_reflection_manager = (Reflection::ReflectionManager*)malloc(sizeof(Reflection::ReflectionManager));
	*editor_reflection_manager = Reflection::ReflectionManager(editor_allocator);
	editor_reflection_manager->CreateFolderHierarchy(L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src");
	editor_reflection_manager->CreateFolderHierarchy(L"C:\\Users\\Andrei\\C++\\ECSEngine\\Editor\\src");
	ECS_TEMP_ASCII_STRING(error_message, 256);
	bool success = editor_reflection_manager->ProcessFolderHierarchy((unsigned int)0, editor_task_manager, &error_message);
	success = editor_reflection_manager->ProcessFolderHierarchy((unsigned int)1, editor_task_manager, &error_message);

	UIReflectionDrawer* editor_ui_reflection = (UIReflectionDrawer*)malloc(sizeof(UIReflectionDrawer));
	*editor_ui_reflection = UIReflectionDrawer(resizable_arena, editor_reflection_manager);

	Reflection::ReflectionManager* module_reflection_manager = (Reflection::ReflectionManager*)malloc(sizeof(Reflection::ReflectionManager));
	*module_reflection_manager = Reflection::ReflectionManager(editor_allocator);

	UIReflectionDrawer* module_ui_reflection = (UIReflectionDrawer*)malloc(sizeof(UIReflectionDrawer));
	*module_ui_reflection = UIReflectionDrawer(resizable_arena, module_reflection_manager);

	editor_state->task_manager = editor_task_manager;
	HubData* hub_data = (HubData*)malloc(sizeof(HubData));
	hub_data->projects.Initialize(editor_allocator, 0, EDITOR_HUB_PROJECT_CAPACITY);
	hub_data->projects.size = 0;
	hub_data->ui_reflection = editor_ui_reflection;
	
	ProjectFile* project_file = (ProjectFile*)malloc(sizeof(ProjectFile));
	project_file->source_dll_name.Initialize(resizable_arena, 0, 64);
	project_file->project_name.Initialize(resizable_arena, 0, 64);
	project_file->path.Initialize(resizable_arena, 0, 256);

	editor_state->hub_data = hub_data;
	editor_state->project_file = project_file;
	editor_state->ui_reflection = editor_ui_reflection;
	editor_state->module_reflection = module_ui_reflection;
	editor_state->ui_system = ui;
	editor_state->viewport_texture = viewport_sprite_texture;
	editor_state->editor_allocator = editor_allocator;

	MemoryManager* console_memory_manager = (MemoryManager*)malloc(sizeof(MemoryManager));
	*console_memory_manager = DefaultConsoleAllocator(global_memory_manager);
	Console* console = (Console*)malloc(sizeof(Console));
	*console = Console(console_memory_manager, editor_task_manager, CONSOLE_RELATIVE_DUMP_PATH);
	editor_state->console = console;

	console->AddSystemFilterString(EDITOR_CONSOLE_SYSTEM_NAME);
	InitializeModuleConfigurations(editor_state);

	FileExplorerData* file_explorer_data = (FileExplorerData*)calloc(1, sizeof(FileExplorerData));
	editor_state->file_explorer_data = file_explorer_data;

	EditorEvent* editor_events = (EditorEvent*)calloc(EDITOR_EVENT_QUEUE_CAPACITY, sizeof(EditorEvent));
	editor_state->event_queue.InitializeFromBuffer(editor_events, EDITOR_EVENT_QUEUE_CAPACITY);

	ProjectModules* project_modules = (ProjectModules*)malloc(sizeof(ProjectModules));
	*project_modules = ProjectModules(editor_allocator, 1);
	editor_state->project_modules = project_modules;
	editor_state->module_configuration_groups.Initialize(editor_allocator, 0);

	TaskDependencies* project_task_graph = (TaskDependencies*)calloc(1, sizeof(TaskDependencies));
	*project_task_graph = TaskDependencies(editor_allocator);
	editor_state->project_task_graph = project_task_graph;

	InspectorData* inspector_data = (InspectorData*)calloc(1, sizeof(InspectorData));
	editor_state->inspector_data = inspector_data;

	World* scene_worlds = (World*)calloc(EDITOR_SCENE_BUFFERING_COUNT, sizeof(World));
	editor_state->active_world = 0;
	editor_state->worlds.InitializeFromBuffer(scene_worlds, EDITOR_SCENE_BUFFERING_COUNT, EDITOR_SCENE_BUFFERING_COUNT);

	ResetProjectGraphicsModule(editor_state);
}

void EditorStateDestroy(EditorState* editor_state) {
	editor_state->editor_allocator->m_backup->ReleaseResources();
	DestroyGraphics(editor_state->ui_system->m_graphics);

	// If necessary, free all the malloc's for the individual allocations - but there are very small and insignificant
	// Not worth freeing them since EditorStateInitialize won't be called more than a couple of times during the runtime of 
	// an instance of the process
}
