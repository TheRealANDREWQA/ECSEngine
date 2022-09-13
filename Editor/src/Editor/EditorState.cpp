#include "editorpch.h"
#include "EditorState.h"
#include "../UI/DirectoryExplorer.h"
#include "../UI/FileExplorer.h"
#include "ECSEngineApplicationUtilities.h"
#include "EditorParameters.h"
#include "EditorEvent.h"
#include "../Modules/Module.h"
#include "../Project/ProjectFolders.h"
#include "../Project/ProjectBackup.h"

#include "../UI/Scene.h"
#include "ECSEngineComponents.h"

using namespace ECSEngine;

bool DISPLAY_LOCKED_FILES_SIZE = false;

#define LAZY_EVALUATION_CREATE_DEFAULT_METAFILES_THRESHOLD 500
#define LAZY_EVALUATION_MODULE_STATUS 500
#define LAZY_EVALUATION_GRAPHICS_MODULE_STATUS 300
#define LAZY_EVALUATION_TASK_ALLOCATOR_RESET 500
#define LAZY_EVALUATION_RUNTIME_SETTINGS 500

// ----------------------------------------------------------------------------------------------------------------------------

// This will be used by the UI for the runtime settings. Initialize it once
void CreateWorldDescriptorUIReflectionType(EditorState* editor_state)
{
	UIReflectionDrawer* ui_reflection = editor_state->ui_reflection;
	UIReflectionType* type = ui_reflection->CreateType(STRING(WorldDescriptor));

	WorldDescriptor min_bounds = GetWorldDescriptorMinBounds();
	WorldDescriptor max_bounds = GetWorldDescriptorMaxBounds();
	WorldDescriptor default_values = GetDefaultWorldDescriptor();

	ui_reflection->BindTypeLowerBounds(type, &min_bounds);
	ui_reflection->BindTypeUpperBounds(type, &max_bounds);
	ui_reflection->BindTypeDefaultData(type, &default_values);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorSetConsoleError(Stream<char> error_message, ECS_CONSOLE_VERBOSITY verbosity) {
	Console* console = GetConsole();
	console->Error(error_message, EDITOR_CONSOLE_SYSTEM_NAME, verbosity);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorSetConsoleWarn(Stream<char> error_message, ECS_CONSOLE_VERBOSITY verbosity) {
	Console* console = GetConsole();
	console->Warn(error_message, EDITOR_CONSOLE_SYSTEM_NAME, verbosity);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorSetConsoleInfo(Stream<char> error_message, ECS_CONSOLE_VERBOSITY verbosity) {
	Console* console = GetConsole();
	console->Info(error_message, EDITOR_CONSOLE_SYSTEM_NAME, verbosity);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorSetConsoleTrace(Stream<char> error_message, ECS_CONSOLE_VERBOSITY verbosity) {
	Console* console = GetConsole();
	console->Trace(error_message, EDITOR_CONSOLE_SYSTEM_NAME, verbosity);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateSetFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag) {
	editor_state->flags[flag].fetch_add(1, ECS_RELAXED);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateClearFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag) {
	editor_state->flags[flag].fetch_sub(1, ECS_RELAXED);
}

// -----------------------------------------------------------------------------------------------------------------

bool EditorStateHasFlag(const EditorState* editor_state, EDITOR_STATE_FLAGS flag) {
	return editor_state->flags[flag].load(ECS_RELAXED) != 0;
}

// -----------------------------------------------------------------------------------------------------------------

void TickModuleStatus(EditorState* editor_state) {
	EDITOR_STATE(editor_state);
	ProjectModules* project_modules = editor_state->project_modules;

	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_UPDATE_MODULE_STATUS, LAZY_EVALUATION_MODULE_STATUS)) {
		for (size_t index = 0; index < project_modules->size; index++) {
			bool is_solution_updated = UpdateModuleSolutionLastWrite(editor_state, index);
			if (is_solution_updated) {
				ReflectModule(editor_state, index);
			}

			for (size_t configuration_index = 0; configuration_index < EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
				EDITOR_MODULE_CONFIGURATION configuration = (EDITOR_MODULE_CONFIGURATION)configuration_index;
				EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);

				if (UpdateModuleLibraryLastWrite(editor_state, index, configuration)) {
					bool success = false;

					if (info->library_last_write_time != 0) {
						success = HasModuleFunction(editor_state, index, configuration);
					}
					SetModuleLoadStatus(&project_modules->buffer[index], success, configuration);
				}
				if (is_solution_updated && info->load_status == EDITOR_MODULE_LOAD_GOOD) {
					info->load_status = EDITOR_MODULE_LOAD_OUT_OF_DATE;
				}
			}
		}

		// Inform the user when many .pdb.locked files gathered
		size_t locked_files_size = GetVisualStudioLockedFilesSize(editor_state);
		if (locked_files_size > ECS_GB) {
			if (!DISPLAY_LOCKED_FILES_SIZE) {
				ECS_FORMAT_TEMP_STRING(console_output, "Visual studio locked files size surpassed 1 GB (actual size is {#}). Consider deleting the files now"
					" if the debugger is not opened or by reopening the project.", locked_files_size);
				EditorSetConsoleWarn(console_output);
				DISPLAY_LOCKED_FILES_SIZE = true;
			}
		}
		else {
			DISPLAY_LOCKED_FILES_SIZE = false;
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------

void TickSandboxRuntimeSettings(EditorState* editor_state) {
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_RUNTIME_SETTINGS, LAZY_EVALUATION_RUNTIME_SETTINGS)) {
		ReloadSandboxRuntimeSettings(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------

void TickEvents(EditorState* editor_state) {
	// Get the event count and do the events that are now in the queue. Because some events might push them back
	// in the event queue and doing that will generate an infinite loop
	unsigned int event_count = editor_state->event_queue.GetSize();

	EditorEvent editor_event;
	unsigned int event_index = 0;
	while (event_index < event_count) {
		editor_state->event_queue.PopNonAtomic(editor_event);
		EditorEventFunction event_function = (EditorEventFunction)editor_event.function;
		event_function(editor_state, editor_event.data);
		event_index++;
	}
}

// -----------------------------------------------------------------------------------------------------------------

void TickLazyEvaluationCounters(EditorState* editor_state) {
	size_t frame_difference = editor_state->lazy_evalution_timer.GetDurationSinceMarker_ms();

	for (size_t index = 0; index < EDITOR_LAZY_EVALUATION_COUNTERS_COUNT; index++) {
		// Clamp the difference such that it won't wrapp arround
		if (USHORT_MAX - editor_state->lazy_evaluation_counters[index] < frame_difference) {
			editor_state->lazy_evaluation_counters[index] = USHORT_MAX;
		}
		else {
			editor_state->lazy_evaluation_counters[index] += frame_difference;
		}
	}
	editor_state->lazy_evalution_timer.SetMarker();
}

// -----------------------------------------------------------------------------------------------------------------

void TickGPUTasks(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	if (editor_state->gpu_tasks.GetSize() > 0 && !EditorStateHasFlag(editor_state, EDITOR_STATE_DO_NOT_ADD_TASKS)) {
		ThreadTask gpu_task;
		while (editor_state->gpu_tasks.Pop(gpu_task)) {
			gpu_task.function(0, nullptr, gpu_task.data);
			if (gpu_task.data_size > 0) {
				multithreaded_editor_allocator->Deallocate(gpu_task.data);
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------

void TickPendingTasks(EditorState* editor_state) {
	// If there are pending tasks left to add and the flag is not set
	if (editor_state->pending_background_tasks.GetSize() > 0 && !EditorStateHasFlag(editor_state, EDITOR_STATE_DO_NOT_ADD_TASKS)) {
		ThreadTask background_task;
		while (editor_state->pending_background_tasks.Pop(background_task)) {
			editor_state->task_manager->AddDynamicTaskAndWake(background_task);
			if (background_task.data_size > 0) {
				editor_state->multithreaded_editor_allocator->Deallocate(background_task.data);
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateProjectTick(EditorState* editor_state) {
	TickLazyEvaluationCounters(editor_state);

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_FREEZE_TICKS)) {
		if (ProjectNeedsBackup(editor_state)) {
			bool autosave_success = SaveProjectBackup(editor_state);
			if (autosave_success) {
				ResetProjectNeedsBackup(editor_state);
			}
		}

		TickModuleStatus(editor_state);
		TickSandboxRuntimeSettings(editor_state);

		DirectoryExplorerTick(editor_state);
		FileExplorerTick(editor_state);

		TickEvents(editor_state);
		TickPendingTasks(editor_state);

		TickEditorComponents(editor_state);
	}
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateHubTick(EditorState* editor_state) {
	TickEvents(editor_state);
	TickPendingTasks(editor_state);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateAddBackgroundTask(EditorState* editor_state, ECSEngine::ThreadTask task)
{
	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_DO_NOT_ADD_TASKS)) {
		editor_state->task_manager->AddDynamicTaskAndWake(task);
	}
	else {
		EDITOR_STATE(editor_state);
		task.data = task.data_size > 0 ? function::Copy(GetAllocatorPolymorphic(multithreaded_editor_allocator, ECS_ALLOCATION_MULTI), task.data, task.data_size) : task.data;
		editor_state->pending_background_tasks.Push(task);
	}
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateAddGPUTask(EditorState* editor_state, ECSEngine::ThreadTask task)
{
	EDITOR_STATE(editor_state);
	task.data = task.data_size > 0 ? function::Copy(GetAllocatorPolymorphic(multithreaded_editor_allocator, ECS_ALLOCATION_MULTI), task.data, task.data_size) : task.data;
	editor_state->gpu_tasks.Push(task);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateInitialize(Application* application, EditorState* editor_state, HWND hWnd, HID::Mouse& mouse, HID::Keyboard& keyboard)
{
	// Create every single member using new for easier class construction
	editor_state->editor_tick = TickPendingTasks;

	GlobalMemoryManager* global_memory_manager = new GlobalMemoryManager(GLOBAL_MEMORY_COUNT, 512, GLOBAL_MEMORY_RESERVE_COUNT);
	Graphics* graphics = (Graphics*)malloc(sizeof(Graphics));
	CreateGraphicsForProcess(graphics, hWnd, global_memory_manager);

	MemoryManager* editor_allocator = new MemoryManager(2'000'000, 4096, 10'000'000, global_memory_manager);
	editor_state->editor_allocator = editor_allocator;
	AllocatorPolymorphic polymorphic_editor_allocator = GetAllocatorPolymorphic(editor_allocator);

	MemoryManager* multithreaded_editor_allocator = new MemoryManager(50'000'000, 4096, 50'000'000, global_memory_manager);
	editor_state->multithreaded_editor_allocator = multithreaded_editor_allocator;

	TaskManager* editor_task_manager = (TaskManager*)malloc(sizeof(TaskManager));
	new (editor_task_manager) TaskManager(std::thread::hardware_concurrency(), global_memory_manager, 1000, 100'000);
	editor_state->task_manager = editor_task_manager;

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
	editor_state->resource_manager = resource_manager;

	ThreadWrapperCountTasksData wrapper_data;
	wrapper_data.count.store(0, ECS_RELAXED);
	editor_task_manager->ChangeDynamicWrapperMode(ThreadWrapperCountTasks, &wrapper_data, sizeof(wrapper_data));
	editor_task_manager->CreateThreads();

	editor_task_manager->SetThreadPriorities(ECS_THREAD_PRIORITY_LOW);

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
		graphics->m_window_size,
		global_memory_manager
	);
	ui->BindWindowHandler(WindowHandler, WindowHandlerInitializer, sizeof(ECSEngine::Tools::UIDefaultWindowHandler));
	editor_state->ui_system = ui;

	Reflection::ReflectionManager* editor_reflection_manager = (Reflection::ReflectionManager*)malloc(sizeof(Reflection::ReflectionManager));
	*editor_reflection_manager = Reflection::ReflectionManager(editor_allocator);
	editor_reflection_manager->CreateFolderHierarchy(L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src");
	editor_reflection_manager->CreateFolderHierarchy(L"C:\\Users\\Andrei\\C++\\ECSEngine\\Editor\\src");
	ECS_TEMP_ASCII_STRING(error_message, 256);
	bool success = editor_reflection_manager->ProcessFolderHierarchy((unsigned int)0, /*editor_task_manager,*/ &error_message);
	ECS_ASSERT(success);

	success = editor_reflection_manager->ProcessFolderHierarchy((unsigned int)1, /*editor_task_manager,*/ &error_message);
	ECS_ASSERT(success);

	size_t editor_component_allocator_size = editor_state->editor_components.DefaultAllocatorSize();
	void* editor_component_allocator_buffer = editor_allocator->Allocate(editor_component_allocator_size);
	editor_state->editor_components.Initialize(editor_component_allocator_buffer);

	// Update the editor components
	editor_state->editor_components.UpdateComponents(editor_reflection_manager, 0, "ECSEngine");
	editor_state->editor_components.EmptyEventStream();

	UIReflectionDrawer* editor_ui_reflection = (UIReflectionDrawer*)malloc(sizeof(UIReflectionDrawer));
	*editor_ui_reflection = UIReflectionDrawer(resizable_arena, editor_reflection_manager);
	editor_state->ui_reflection = editor_ui_reflection;

	Reflection::ReflectionManager* module_reflection_manager = (Reflection::ReflectionManager*)malloc(sizeof(Reflection::ReflectionManager));
	*module_reflection_manager = Reflection::ReflectionManager(editor_allocator);

	// Inherit the constants from the ui_reflection
	module_reflection_manager->InheritConstants(editor_reflection_manager);

	UIReflectionDrawer* module_ui_reflection = (UIReflectionDrawer*)malloc(sizeof(UIReflectionDrawer));
	*module_ui_reflection = UIReflectionDrawer(resizable_arena, module_reflection_manager);
	editor_state->module_reflection = module_ui_reflection;

	// Create all the engine components as types into the ui_reflection
	for (size_t index = 0; index < std::size(ECS_COMPONENTS); index++) {
		editor_ui_reflection->CreateType(editor_reflection_manager->GetType(ECS_COMPONENTS[index]));
	}

	HubData* hub_data = (HubData*)malloc(sizeof(HubData));
	hub_data->projects.Initialize(editor_allocator, 0, EDITOR_HUB_PROJECT_CAPACITY);
	hub_data->projects.size = 0;
	hub_data->ui_reflection = editor_ui_reflection;
	editor_state->hub_data = hub_data;
	
	ProjectFile* project_file = (ProjectFile*)malloc(sizeof(ProjectFile));
	project_file->project_name.Initialize(resizable_arena, 0, 64);
	project_file->path.Initialize(resizable_arena, 0, 256);
	editor_state->project_file = project_file;

	MemoryManager* console_memory_manager = (MemoryManager*)malloc(sizeof(MemoryManager));
	*console_memory_manager = DefaultConsoleAllocator(global_memory_manager);
	SetConsole(console_memory_manager, editor_task_manager, CONSOLE_RELATIVE_DUMP_PATH);

	GetConsole()->AddSystemFilterString(EDITOR_CONSOLE_SYSTEM_NAME);

	FileExplorerData* file_explorer_data = (FileExplorerData*)calloc(1, sizeof(FileExplorerData));
	editor_state->file_explorer_data = file_explorer_data;
	InitializeFileExplorer(editor_state);

	EditorEvent* editor_events = (EditorEvent*)calloc(EDITOR_EVENT_QUEUE_CAPACITY, sizeof(EditorEvent));
	editor_state->event_queue.InitializeFromBuffer(editor_events, EDITOR_EVENT_QUEUE_CAPACITY);

	ProjectModules* project_modules = (ProjectModules*)malloc(sizeof(ProjectModules));
	project_modules->Initialize(polymorphic_editor_allocator, 0);
	editor_state->project_modules = project_modules;

	for (size_t index = 0; index < EDITOR_MODULE_CONFIGURATION_COUNT; index++) {
		editor_state->launched_module_compilation[index].Initialize(polymorphic_editor_allocator, 4);
	}
	editor_state->launched_module_compilation_lock.unlock();

	InitializeInspectorManager(editor_state);
	
	// Allocate the lazy evaluation counters
	editor_state->lazy_evaluation_counters = (unsigned short*)malloc(EDITOR_LAZY_EVALUATION_COUNTERS_COUNT * sizeof(unsigned short));
	// Make all counters USHORT_MAX to trigger the lazy evaluation at first
	memset(editor_state->lazy_evaluation_counters, 0xFF, sizeof(unsigned short) * EDITOR_LAZY_EVALUATION_COUNTERS_COUNT);

	editor_state->lazy_evalution_timer.SetNewStart();

	// This needs to be called last
	InitializeSandboxes(editor_state);

	CreateWorldDescriptorUIReflectionType(editor_state);

	unsigned int ecs_runtime_index = 0;
#ifdef ECSENGINE_RELEASE
	ecs_runtime_index = 2;
#endif

#ifdef ECSENGINE_DISTRIBUTION
	ecs_runtime_index = 4;
#endif

	Stream<wchar_t> pdb_paths[2] = {
		 ECS_RUNTIME_PDB_PATHS[ecs_runtime_index],
		 ECS_RUNTIME_PDB_PATHS[ecs_runtime_index + 1]
	};
	OS::InitializeSymbolicLinksPaths({ pdb_paths, std::size(pdb_paths) });

	// Initialize the gpu_tasks and background tasks queues
	editor_state->gpu_tasks.m_queue.Initialize(editor_state->EditorAllocator(), 8);
	editor_state->pending_background_tasks.m_queue.Initialize(editor_state->EditorAllocator(), 8);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateDestroy(EditorState* editor_state) {
	editor_state->editor_allocator->m_backup->Free();
	DestroyGraphics(editor_state->ui_system->m_graphics);

	// If necessary, free all the malloc's for the individual allocations - but there are very small and insignificant
	// Not worth freeing them since EditorStateInitialize won't be called more than a couple of times during the runtime of 
	// an instance of the process
}

// -----------------------------------------------------------------------------------------------------------------

unsigned short EditorStateLazyEvaluation(EditorState* editor_state, unsigned int index)
{
	return editor_state->lazy_evaluation_counters[index];
}

// -----------------------------------------------------------------------------------------------------------------

bool EditorStateLazyEvaluationTrue(EditorState* editor_state, unsigned int index, unsigned short duration)
{
	if (editor_state->lazy_evaluation_counters[index] >= duration) {
		editor_state->lazy_evaluation_counters[index] = 0;
		return true;
	}
	return false;
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateLazyEvaluationSetMax(EditorState* editor_state, unsigned int index)
{
	editor_state->lazy_evaluation_counters[index] = USHORT_MAX;
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateLazyEvaluationSet(EditorState* editor_state, unsigned int index, unsigned short value)
{
	editor_state->lazy_evaluation_counters[index] = value;
}

// -----------------------------------------------------------------------------------------------------------------

struct ApplicationQuitHandlerData {
	char* response;
};

void ApplicationQuitHandler(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ApplicationQuitHandlerData* data = (ApplicationQuitHandlerData*)_data;
	SaveScenePopUpResult* result = (SaveScenePopUpResult*)_additional_data;

	if (result->cancel_call) {
		*data->response = 0;
	}
	else {
		*data->response = 1;
	}
}

void EditorStateApplicationQuit(EditorState* editor_state, char* response)
{
	ApplicationQuitHandlerData quit_data;
	quit_data.response = response;

	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, sandbox_indices, editor_state->sandboxes.size);
	sandbox_indices.size = editor_state->sandboxes.size;
	function::MakeSequence(sandbox_indices);
	CreateSaveScenePopUp(editor_state, sandbox_indices, { ApplicationQuitHandler, &quit_data, sizeof(quit_data) });
}

// -----------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------
