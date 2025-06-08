#include "editorpch.h"
#include "EditorState.h"
#include "../UI/DirectoryExplorer.h"
#include "../UI/FileExplorer.h"
#include "ECSEngineApplicationUtilities.h"
#include "EditorParameters.h"
#include "EditorEvent.h"
#include "EditorGeneralInputTick.h"
#include "EditorSourceCode.h"
#include "../Modules/Module.h"
#include "../Project/ProjectFolders.h"
#include "../Project/ProjectBackup.h"
#include "../Project/ProjectUITemplate.h"
#include "../Sandbox/Sandbox.h"
#include "../Sandbox/SandboxProfiling.h"
#include "../Sandbox/SandboxModule.h"

#include "../UI/CreateScene.h"
#include "ECSEngineComponents.h"
#include "ECSEngineLinkComponents.h"
#include "../Assets/AssetManagement.h"
#include "../Assets/AssetTick.h"
#include "../UI/AssetOverrides.h"
#include "EditorInputMapping.h"

#include "../UI/VisualizeTexture.h"

using namespace ECSEngine;

bool DISPLAY_LOCKED_FILES_SIZE = false;

#define LAZY_EVALUATION_CREATE_DEFAULT_METAFILES_THRESHOLD 500
#define LAZY_EVALUATION_MODULE_STATUS 500
#define LAZY_EVALUATION_GRAPHICS_MODULE_STATUS 300
#define LAZY_EVALUATION_TASK_ALLOCATOR_RESET 500

// These are used to differentiate between the main thread, the background threads,
// And the simulation threads
static size_t MAIN_THREAD_ID;
static Stream<size_t> BACKGROUND_THREADS_ID;
static Stream<CrashHandler> BACKGROUND_THREAD_SPECIFIC_CRASH_HANDLER;

static bool MAIN_THREAD_INTERCEPT_CRASH_HANDLER;
static CrashHandler BACKGROUND_THREADS_CRASH_HANDLER = { DebugbreakCrashHandler, nullptr };
static CrashHandler SIMULATION_THREADS_CRASH_HANDLER = { DebugbreakCrashHandler, nullptr };

// ----------------------------------------------------------------------------------------------------------------------------

// This is the crash handler that is set at the beginning and is not changed throughout the
// Execution of the editor
static void EditorCrashHandler(void* data, Stream<char> error_string) {
	// Get the running thread ID
	size_t running_thread_id = OS::GetCurrentThreadID();
	// Compare it with the main thread and the background threads IDs
	if (MAIN_THREAD_ID == running_thread_id) {
		if (MAIN_THREAD_INTERCEPT_CRASH_HANDLER) {
			// If we have the intercept crash handler set, cause an exception
			// Through a memory violation such that the SEH handler is executed
			int* invalid_ptr = (int*)0x0;
			*invalid_ptr = 0;
		}
		else {
			// Just debug break at the moment in the other case
		__debugbreak();

			if (ECS_GLOBAL_ASSERT_WRITE_DEBUG_ALLOCATOR_CALLS) {
				DebugAllocatorManagerWriteState();
			}
		}
	}
	else {
		size_t index = 0;
		for (; index < BACKGROUND_THREADS_ID.size; index++) {
			if (BACKGROUND_THREADS_ID[index] == running_thread_id) {
				// Run the background thread crash handler - the priority
				// Being the specific one
				if (BACKGROUND_THREAD_SPECIFIC_CRASH_HANDLER[index].function != nullptr) {
					BACKGROUND_THREAD_SPECIFIC_CRASH_HANDLER[index].function(BACKGROUND_THREAD_SPECIFIC_CRASH_HANDLER[index].data, error_string);
				}
				else {
					BACKGROUND_THREADS_CRASH_HANDLER.function(BACKGROUND_THREADS_CRASH_HANDLER.data, error_string);
				}
				break;
			}
		}

		if (index == BACKGROUND_THREADS_ID.size) {
			// Not the main thread or background thread - it must be a simulation thread
			// Or a render scene thread, but we can use for both of them the simulation
			// Crash handler
			SIMULATION_THREADS_CRASH_HANDLER.function(SIMULATION_THREADS_CRASH_HANDLER.data, error_string);
		}
	}
}

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

void EditorSetConsoleError(Stream<char> error_message, ECS_CONSOLE_VERBOSITY verbosity, const UIActionHandler& clickable_action) {
	Console* console = GetConsole();
	console->Error(error_message, { EDITOR_CONSOLE_SYSTEM_NAME, verbosity, clickable_action });
}

// -----------------------------------------------------------------------------------------------------------------

void EditorSetConsoleWarn(Stream<char> error_message, ECS_CONSOLE_VERBOSITY verbosity, const UIActionHandler& clickable_action) {
	Console* console = GetConsole();
	console->Warn(error_message, { EDITOR_CONSOLE_SYSTEM_NAME, verbosity, clickable_action });
}

// -----------------------------------------------------------------------------------------------------------------

void EditorSetConsoleInfo(Stream<char> error_message, ECS_CONSOLE_VERBOSITY verbosity, const UIActionHandler& clickable_action) {
	Console* console = GetConsole();
	console->Info(error_message, { EDITOR_CONSOLE_SYSTEM_NAME, verbosity, clickable_action });
}

// -----------------------------------------------------------------------------------------------------------------

void EditorSetConsoleTrace(Stream<char> error_message, ECS_CONSOLE_VERBOSITY verbosity, const UIActionHandler& clickable_action) {
	Console* console = GetConsole();
	console->Trace(error_message, { EDITOR_CONSOLE_SYSTEM_NAME, verbosity, clickable_action });
}

// -----------------------------------------------------------------------------------------------------------------

size_t EditorStateSetFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag) {
	return editor_state->flags[flag].fetch_add(1, ECS_RELAXED);
}

// -----------------------------------------------------------------------------------------------------------------

size_t EditorStateClearFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag) {
	return editor_state->flags[flag].fetch_sub(1, ECS_RELAXED);
}

// -----------------------------------------------------------------------------------------------------------------

bool EditorStateTrySetFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag, size_t compare_value, size_t new_value)
{
	return editor_state->flags[flag].compare_exchange_strong(compare_value, new_value);
}

// -----------------------------------------------------------------------------------------------------------------

bool EditorStateHasFlag(const EditorState* editor_state, EDITOR_STATE_FLAGS flag) {
	return editor_state->flags[flag].load(ECS_RELAXED) != 0;
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateWaitFlag(size_t sleep_milliseconds, const EditorState* editor_state, EDITOR_STATE_FLAGS flag, bool set)
{
	TickWait<'!'>(sleep_milliseconds, editor_state->flags[flag], (size_t)set);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateWaitFlagCount(size_t sleep_milliseconds, const EditorState* editor_state, EDITOR_STATE_FLAGS flag, size_t count)
{
	TickWait<'!'>(sleep_milliseconds, editor_state->flags[flag], count);
}

// -----------------------------------------------------------------------------------------------------------------

void TickModuleStatus(EditorState* editor_state) {
	ProjectModules* project_modules = editor_state->project_modules;

	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_UPDATE_MODULE_STATUS, LAZY_EVALUATION_MODULE_STATUS)) {
		for (size_t index = 0; index < project_modules->size; index++) {
			bool is_solution_updated = UpdateModuleSolutionLastWrite(editor_state, index);
			if (is_solution_updated) {
				ReflectModule(editor_state, index);
				bool is_graphics_module = IsGraphicsModule(editor_state, index);
				if (is_graphics_module) {
					// For each configuration determine if it is used by sandboxes to see if we need to recompile
					ECS_STACK_CAPACITY_STREAM(unsigned int, dependency_sandboxes, 512);
					for (size_t configuration_index = 0; configuration_index < EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
						EDITOR_MODULE_CONFIGURATION current_configuration = (EDITOR_MODULE_CONFIGURATION)configuration_index;
						if (GetSandboxesForModule(editor_state, index, current_configuration, &dependency_sandboxes)) {
							// Launch the build command for that module and configuration, if not already being compiled
							if (!IsModuleBeingCompiled(editor_state, index, current_configuration)) {
								BuildModule(editor_state, index, current_configuration);
							}

							// Re-render the sandboxes that depend on this module
							for (unsigned int sandbox_index = 0; sandbox_index < dependency_sandboxes.size; sandbox_index++) {
								RenderSandbox(editor_state, dependency_sandboxes[sandbox_index], EDITOR_SANDBOX_VIEWPORT_SCENE, { 0,0 }, true);
							}
						}
					}
				}
			}

			for (size_t configuration_index = 0; configuration_index < EDITOR_MODULE_CONFIGURATION_COUNT; configuration_index++) {
				EDITOR_MODULE_CONFIGURATION configuration = (EDITOR_MODULE_CONFIGURATION)configuration_index;
				EditorModuleInfo* info = GetModuleInfo(editor_state, index, configuration);

				if (UpdateModuleLibraryLastWrite(editor_state, index, configuration)) {
					bool success = false;

					if (info->library_last_write_time != 0) {
						success = HasModuleFunction(editor_state, index, configuration);
					}
					SetModuleLoadStatus(editor_state, index, success, configuration);
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

void TickEvents(EditorState* editor_state) {
	// Get the event count and do the events that are now in the queue. Because some events might push them back
	// in the event queue and doing that will generate an infinite loop
	editor_state->readd_events.Clear();
	editor_state->tick_processing_events.Clear();
	unsigned int event_count = editor_state->event_queue.PopRangeAll(&editor_state->tick_processing_events);
	editor_state->tick_processing_events_index = 0;
	
	for (unsigned int event_index = 0; event_index < event_count; event_index++) {
		editor_state->tick_processing_events_index = event_index;

		EditorEvent editor_event = editor_state->tick_processing_events[event_index];
		EditorEventFunction event_function = (EditorEventFunction)editor_event.function;
		bool needs_push = event_function(editor_state, editor_event.data);
		if (needs_push) {
			editor_state->readd_events.Add(editor_event);
		}
		else {
			if (editor_event.data_size > 0) {
				Deallocate(editor_state->MultithreadedEditorAllocator(), editor_event.data);
			}
		}
	}

	editor_state->event_queue.PushRange(editor_state->readd_events);
}

// -----------------------------------------------------------------------------------------------------------------

void TickLazyEvaluationCounters(EditorState* editor_state) {
	size_t frame_difference = editor_state->lazy_evalution_timer.GetDurationSinceMarker(ECS_TIMER_DURATION_MS);

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
	if (editor_state->gpu_tasks.GetSize() > 0 && !EditorStateHasFlag(editor_state, EDITOR_STATE_DO_NOT_ADD_TASKS)) {
		ThreadTask gpu_task;
		while (editor_state->gpu_tasks.Pop(gpu_task)) {
			gpu_task.function(0, nullptr, gpu_task.data);
			if (gpu_task.data_size > 0) {
				Deallocate(editor_state->MultithreadedEditorAllocator(), gpu_task.data);
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
				editor_state->multithreaded_editor_allocator->DeallocateTs(background_task.data);
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------------------

void TickSaveProjectUIAutomatically(EditorState* editor_state) {
	const size_t SAVE_PROJECT_AUTOMATICALLY_TICK = 1000;

	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_SAVE_PROJECT_UI, SAVE_PROJECT_AUTOMATICALLY_TICK)) {
		ProjectFile* project_file = editor_state->project_file;

		ECS_STACK_CAPACITY_STREAM(wchar_t, template_path, 256);
		template_path.CopyOther(project_file->path);
		template_path.Add(ECS_OS_PATH_SEPARATOR);
		template_path.AddStreamSafe(PROJECT_CURRENT_UI_TEMPLATE);

		CapacityStream<char> error_message = { nullptr, 0, 0 };
		bool success = SaveProjectUITemplate(editor_state->ui_system, { template_path }, error_message);
		if (!success) {
			EditorSetConsoleError("Automatic project UI save failed.");
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateProjectTick(EditorState* editor_state) {
	TickLazyEvaluationCounters(editor_state);

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_FREEZE_TICKS)) {
		if (ProjectNeedsBackup(editor_state)) {
			bool autosave_success = SaveProjectBackup(editor_state);
			// Reset the project backup anyway, since it will cause an explosion of error messages if left to retry
			ResetProjectNeedsBackup(editor_state);
		}

		TickProjectSourceCodeInfo(editor_state);

		TickUpdateSandboxHIDInputs(editor_state);
		TickModuleStatus(editor_state);

		TickSandboxes(editor_state);
		TickSandboxUpdateMasterButtons(editor_state);

		TickDirectoryExplorer(editor_state);
		TickFileExplorer(editor_state);

		TickUpdateModulesDLLImports(editor_state);

		TickPendingTasks(editor_state);
		TickEditorComponents(editor_state);
		TickAsset(editor_state);
		TickEvents(editor_state);

		// At the end we need to tick the sandboxes that are running
		TickModuleSettingsRefresh(editor_state);
		TickSandboxRuntimes(editor_state);

		TickSaveProjectUIAutomatically(editor_state);

		TickEditorGeneralInput(editor_state);
		TickPrefabFileChange(editor_state);
		TickPrefabUpdateActiveIDs(editor_state);
		TickEditorShortcutFocus(editor_state);
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
		task.data = task.data_size > 0 ? Copy(editor_state->MultithreadedEditorAllocator(), task.data, task.data_size) : task.data;
		editor_state->pending_background_tasks.Push(task);
	}
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateAddGPUTask(EditorState* editor_state, ThreadTask task)
{
	task.data = task.data_size > 0 ? Copy(editor_state->MultithreadedEditorAllocator(), task.data, task.data_size) : task.data;
	editor_state->gpu_tasks.Push(task);
}

// -----------------------------------------------------------------------------------------------------------------

// There needs to be a separate graphics initialization of the runtime and the initialization
// of the rest of the runtime (like the resource manager and the asset database)
void PreinitializeRuntime(EditorState* editor_state) {
	// Use separate allocators for them. Coallesce all the types into a single allocation
	// The resizable memory arena is for the asset database
	MemoryManager* runtime_resource_manager_allocator = (MemoryManager*)editor_state->multithreaded_editor_allocator->AllocateTs(sizeof(MemoryManager)
		+ sizeof(ResourceManager) + sizeof(ResizableMemoryArena) + sizeof(AssetDatabase));

	DefaultResourceManagerAllocator(runtime_resource_manager_allocator, editor_state->GlobalMemoryManager());

	// Create the resource manager - it already has the shader directory set for the ECSEngine Shaders
	editor_state->runtime_resource_manager = (ResourceManager*)OffsetPointer(runtime_resource_manager_allocator, sizeof(*runtime_resource_manager_allocator));
	*editor_state->runtime_resource_manager = ResourceManager(runtime_resource_manager_allocator, editor_state->RuntimeGraphics());

	// And the asset database
	ResizableMemoryArena* database_arena = (ResizableMemoryArena*)OffsetPointer(editor_state->runtime_resource_manager, sizeof(*editor_state->runtime_resource_manager));
	CreateResizableMemoryArena(database_arena, ECS_MB, 4, ECS_KB * 4, editor_state->GlobalMemoryManager(), ECS_MB * 5, 4, ECS_KB * 4);

	AssetDatabase* database = (AssetDatabase*)OffsetPointer(database_arena, sizeof(*database_arena));
	*database = AssetDatabase(database_arena, editor_state->ui_reflection->reflection);
	editor_state->asset_database = database;
}

void InitializeRuntime(EditorState* editor_state) {
	PreinitializeRuntime(editor_state);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateBaseInitialize(EditorState* editor_state, HWND hwnd, Mouse* mouse, Keyboard* keyboard)
{
	OS::InitializePhysicalMemoryPageSize();

	GlobalMemoryManager* hub_allocator = (GlobalMemoryManager*)Malloc(sizeof(GlobalMemoryManager));
	CreateGlobalMemoryManager(hub_allocator, ECS_KB * 16, 512, ECS_KB * 16);

	HubData* hub_data = (HubData*)Malloc(sizeof(HubData));
	hub_data->projects.Initialize(hub_allocator, 0, EDITOR_HUB_PROJECT_CAPACITY);
	hub_data->projects.size = 0;
	hub_data->allocator = hub_allocator;
	editor_state->hub_data = hub_data;

	GlobalMemoryManager* console_global_memory = (GlobalMemoryManager*)Malloc(sizeof(GlobalMemoryManager));
	CreateGlobalMemoryManager(console_global_memory, ECS_MB * 10, 64, ECS_MB * 5);
	MemoryManager* console_memory_manager = (MemoryManager*)Malloc(sizeof(MemoryManager));
	DefaultConsoleStableAllocator(console_memory_manager, console_global_memory);
	AtomicStream<char> console_message_allocator = DefaultConsoleMessageAllocator(console_global_memory);
	SetConsole(console_memory_manager, console_message_allocator, L"TempDump.txt");

	*keyboard = Keyboard(hub_allocator);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateInitialize(Application* application, EditorState* editor_state, HWND hWnd, Mouse* mouse, Keyboard* keyboard, uint2 monitor_size)
{
	// Initialize the Debug SettingsAllocator Manager
	DebugAllocatorManagerDescriptor debug_allocator_manager_descriptor;
	debug_allocator_manager_descriptor.capacity = ECS_DEBUG_ALLOCATOR_MANAGER_CAPACITY_MEDIUM;
	debug_allocator_manager_descriptor.enable_global_write_to_file = true;
	DebugAllocatorManagerInitialize(&debug_allocator_manager_descriptor);

	InitializeProjectSourceCode(editor_state);

	// Create every single member using new for easier class construction
	editor_state->editor_tick = TickPendingTasks;
	editor_state->disable_editor_sandbox_write = false;
	memset(editor_state->flags, 0, sizeof(editor_state->flags));

	GlobalMemoryManager* global_memory_manager = (GlobalMemoryManager*)Malloc(sizeof(GlobalMemoryManager));
	CreateGlobalMemoryManager(global_memory_manager, GLOBAL_MEMORY_COUNT, 512, GLOBAL_MEMORY_RESERVE_COUNT);
	Graphics* graphics = (Graphics*)Malloc(sizeof(Graphics));
	CreateGraphicsForProcess(graphics, hWnd, global_memory_manager, true);
	editor_state->graphics = graphics;

	MemoryManager* editor_allocator = new MemoryManager(MEMORY_MANAGER_CAPACITY, 4 * ECS_KB, MEMORY_MANAGER_RESERVE_CAPACITY, global_memory_manager);
	editor_state->editor_allocator = editor_allocator;
	
	AllocatorPolymorphic polymorphic_editor_allocator = editor_allocator;

	MemoryManager* multithreaded_editor_allocator = new MemoryManager(
		MULTITHREADED_MEMORY_MANAGER_CAPACITY, 
		4 * ECS_KB, 
		MULTITHREADED_MEMORY_MANAGER_RESERVE_CAPACITY, 
		global_memory_manager
	);
	editor_state->multithreaded_editor_allocator = multithreaded_editor_allocator;

	TaskManager* editor_task_manager = (TaskManager*)Malloc(sizeof(TaskManager));
	new (editor_task_manager) TaskManager(std::thread::hardware_concurrency(), global_memory_manager, 1'000, 100);
	editor_state->task_manager = editor_task_manager;
	editor_task_manager->PushExceptionHandler(HandleAllSandboxPhysicalMemoryException, editor_state, 0);
	editor_task_manager->SetWaitType(ECS_TASK_MANAGER_WAIT_SLEEP | ECS_TASK_MANAGER_WAIT_STEAL);

	TaskManager* render_task_manager = (TaskManager*)Malloc(sizeof(TaskManager));
	new (render_task_manager) TaskManager(std::thread::hardware_concurrency(), global_memory_manager, 1'000, 100);
	editor_state->render_task_manager = render_task_manager;
	render_task_manager->PushExceptionHandler(HandleAllSandboxPhysicalMemoryException, editor_state, 0);

	// Make a wrapper world that only references this task manager
	World* task_manager_world = (World*)Malloc(sizeof(World));
	task_manager_world->task_manager = editor_task_manager;
	editor_task_manager->m_world = task_manager_world;

	MemoryManager* ui_resource_manager_allocator = (MemoryManager*)Malloc(sizeof(MemoryManager));
	DefaultResourceManagerAllocator(ui_resource_manager_allocator, global_memory_manager);
	ResourceManager* ui_resource_manager = (ResourceManager*)Malloc(sizeof(ResourceManager));
	new (ui_resource_manager) ResourceManager(ui_resource_manager_allocator, graphics);

	editor_state->ui_resource_manager = ui_resource_manager;

	ThreadWrapperCountTasksData wrapper_data;
	wrapper_data.count.store(0, ECS_RELAXED);
	editor_task_manager->ChangeDynamicWrapperMode({ ThreadWrapperCountTasks, &wrapper_data, sizeof(wrapper_data) });
	editor_task_manager->CreateThreads();
	render_task_manager->CreateThreads();
	editor_task_manager->SetThreadPriorities(OS::ECS_THREAD_PRIORITY_LOW);
	render_task_manager->SetThreadPriorities(OS::ECS_THREAD_PRIORITY_VERY_LOW);

	UIToolsAllocator* resizable_arena = (UIToolsAllocator*)Malloc(sizeof(UIToolsAllocator));
	DefaultUISystemAllocator(resizable_arena, global_memory_manager);

	UISystem* ui = (UISystem*)Malloc(sizeof(UISystem));
	new (ui) UISystem(
		application,
		resizable_arena,
		keyboard,
		mouse,
		graphics,
		ui_resource_manager,
		editor_task_manager,
		graphics->m_window_size,
		monitor_size,
		global_memory_manager
	);
	ui->BindWindowHandler(WindowHandler, WindowHandlerInitializer, sizeof(Tools::UIDefaultWindowHandler));
	editor_state->ui_system = ui;

	size_t editor_component_allocator_size = editor_state->editor_components.DefaultAllocatorSize();
	void* editor_component_allocator_buffer = editor_allocator->Allocate(editor_component_allocator_size);
	editor_state->editor_components.Initialize(editor_component_allocator_buffer);

	editor_state->event_queue.Initialize(editor_state->EditorAllocator(), EDITOR_EVENT_QUEUE_CAPACITY);
	editor_state->readd_events.Initialize(editor_state->EditorAllocator(), 8);

	// Register the link components for the engine components
	editor_state->ecs_link_components = LoadModuleLinkComponentTargets(RegisterECSLinkComponents, editor_state->EditorAllocator());
	editor_state->ecs_extra_information = LoadModuleExtraInformation(RegisterECSModuleExtraInformation, editor_state->EditorAllocator());
	editor_state->ecs_component_functions = LoadModuleComponentFunctions(RegisterECSComponentFunctions, editor_state->EditorAllocator());

	// Create a temporary reflection manager that parses the ECSEngine and Editor locations, that the module reflection
	// Will then inherit all types. In this way, these internal types are not associated with a folder hierarchy inside
	// The module reflection and it allows us to convert module index to hierarchy index directly, without adjustments.
	Reflection::ReflectionManager editor_reflection_manager(editor_allocator);
	editor_reflection_manager.CreateFolderHierarchy(L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src");
	editor_reflection_manager.CreateFolderHierarchy(L"C:\\Users\\Andrei\\C++\\ECSEngine\\Editor\\src");
	ECS_STACK_CAPACITY_STREAM(char, error_message, 256);
	bool success = editor_reflection_manager.ProcessFolderHierarchy((unsigned int)0, editor_task_manager, &error_message);
	ECS_ASSERT(success);
	// Create all the link types for the components inside the reflection manager
	CreateLinkTypesForComponents(&editor_reflection_manager, 0);

	success = editor_reflection_manager.ProcessFolderHierarchy((unsigned int)1, editor_task_manager, &error_message);
	ECS_ASSERT(success);

	// Update the editor components - these must be always the first "module" for the editor components
	// A function which checks to see if the component is from ECSEngine or not uses this presumption
	editor_state->editor_components.UpdateComponents(editor_state, &editor_reflection_manager, 0, "ECSEngine");
	// Finalize every event
	for (unsigned int index = 0; index < editor_state->editor_components.events.size; index++) {
		editor_state->editor_components.FinalizeEvent(editor_state, &editor_reflection_manager, editor_state->editor_components.events[index]);
	}
	editor_state->editor_components.EmptyEventStream();

	Reflection::ReflectionManager* module_reflection_manager = (Reflection::ReflectionManager*)Malloc(sizeof(Reflection::ReflectionManager));
	*module_reflection_manager = Reflection::ReflectionManager(editor_allocator);

	// Inherit everything from the ui_reflection. Bind them to the hierarchy -2, such that they don't conflict with the -1 hierarchy
	// Which is used for other purposes, of inheriting instantiated templates
	module_reflection_manager->AddAllFrom(&editor_reflection_manager, -2);
	editor_state->module_reflection = module_reflection_manager;

	editor_reflection_manager.Free();

	UIReflectionDrawer* editor_ui_reflection = (UIReflectionDrawer*)Malloc(sizeof(UIReflectionDrawer));
	*editor_ui_reflection = UIReflectionDrawer(resizable_arena, module_reflection_manager);
	editor_state->ui_reflection = editor_ui_reflection;

	ECS_STACK_LINEAR_ALLOCATOR(ui_asset_override_allocator, ECS_KB * 2);

	// Override the module reflection with the assets overrides
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(UIReflectionFieldOverride, ui_asset_overrides, AssetUIOverrideCount());
	GetEntityComponentUIOverrides(editor_state, ui_asset_overrides.buffer, &ui_asset_override_allocator);
	// Set the overrides on the module reflection and on the engine side since we have some link components there as well
	for (size_t index = 0; index < ui_asset_overrides.capacity; index++) {
		editor_state->ui_reflection->SetFieldOverride(ui_asset_overrides.buffer + index);
	}
	
	ProjectFile* project_file = (ProjectFile*)Malloc(sizeof(ProjectFile));
	project_file->project_name.Initialize(resizable_arena, 0, 64);
	project_file->path.Initialize(resizable_arena, 0, 256);
	editor_state->project_file = project_file;

	Console* console = GetConsole();
	console->stable_allocator->Clear();
	SetConsole(console->stable_allocator, console->message_allocator, L"TempDump.txt");

	GetConsole()->AddSystemFilterString(EDITOR_CONSOLE_SYSTEM_NAME);

	FileExplorerData* file_explorer_data = (FileExplorerData*)Malloc(sizeof(FileExplorerData));
	editor_state->file_explorer_data = file_explorer_data;
	InitializeFileExplorer(editor_state);

	ProjectModules* project_modules = (ProjectModules*)Malloc(sizeof(ProjectModules));
	project_modules->Initialize(polymorphic_editor_allocator, 0);
	editor_state->project_modules = project_modules;

	for (size_t index = 0; index < EDITOR_MODULE_CONFIGURATION_COUNT; index++) {
		editor_state->launched_module_compilation[index].Initialize(polymorphic_editor_allocator, 4);
	}
	editor_state->launched_module_compilation_lock.Unlock();

	InitializeInspectorManager(editor_state);
	
	// Allocate the lazy evaluation counters
	editor_state->lazy_evaluation_counters = (unsigned short*)Malloc(EDITOR_LAZY_EVALUATION_COUNTERS_COUNT * sizeof(unsigned short));
	// Make all counters USHORT_MAX to trigger the lazy evaluation at first
	memset(editor_state->lazy_evaluation_counters, 0xFF, sizeof(unsigned short) * EDITOR_LAZY_EVALUATION_COUNTERS_COUNT);

	editor_state->lazy_evalution_timer.SetNewStart();

	// Allocate the input mapping
	InitializeInputMapping(editor_state);

	// This needs to be called last
	InitializeSandboxes(editor_state);

	CreateWorldDescriptorUIReflectionType(editor_state);
	OS::InitializeSymbolicLinksPaths({});

	// Initialize the gpu_tasks, background tasks queues, the loading assets array
	// And the prefabs container
	editor_state->gpu_tasks.m_queue.Initialize(editor_state->EditorAllocator(), 8);
	editor_state->pending_background_tasks.Initialize(editor_state->EditorAllocator(), 8);
	editor_state->loading_assets.Initialize(editor_state->EditorAllocator(), 0);
	editor_state->prefabs.Initialize(editor_state->EditorAllocator(), 16);
	editor_state->prefabs_allocator = MemoryManager(ECS_MB * 2, ECS_KB * 4, ECS_MB * 8, editor_state->EditorAllocator());
	editor_state->tick_processing_events.Initialize(editor_state->EditorAllocator(), 8);

	// This will be run asynchronously for the graphics object
	InitializeRuntime(editor_state);

	editor_state->Mouse()->AttachToWindow(application->GetOSWindowHandle());
	// Change the dump type to none during the hub phase
	console->SetDumpType(ECS_CONSOLE_DUMP_NONE);

	size_t gpu_vendor_id;
	size_t gpu_device_id;
	ECS_STACK_CAPACITY_STREAM(char, gpu_description, 512);
	editor_state->graphics->GetPhysicalDeviceDescription(gpu_description, &gpu_vendor_id, &gpu_device_id);

	ECS_INITIALIZE_GPU_STATS_STATUS gpu_stats_status = InitializeGPUStatsRecording(gpu_description, gpu_vendor_id, gpu_device_id);
	// In case the gpu stats could not be initialized because of the vendor not being supported, don't emit an error message
	if (gpu_stats_status == ECS_INITIALIZE_GPU_STATS_FAILURE) {
		EditorSetConsoleError("Failed to initialize GPU stats recording support");
	}

	EditorRegisterMainThreadID();
	EditorRegisterBackgroundThreadsID(editor_state);

	// Permanently redirect all asserts to the general crash handler
	ECS_GLOBAL_ASSERT_CRASH = true;
	SetCrashHandler(EditorCrashHandler, nullptr);

	// Initialize the shortcut focus
	editor_state->shortcut_focus.Initialize(editor_state->EditorAllocator());

	editor_state->frame_timer.SetNewStart();
	editor_state->frame_delta_time = 0.0f;
	editor_state->elevated_frame_pacing = ECS_UI_FRAME_PACING_NONE;

	// Find the powershell executable
	ECS_STACK_CAPACITY_STREAM(wchar_t, powershell_executable_storage, 512);
	Stream<wchar_t> powershell_executable = OS::SearchForExecutable(L"powershell.exe", powershell_executable_storage);
	editor_state->powershell_executable_path = powershell_executable.Copy(editor_state->EditorAllocator());
	if (editor_state->powershell_executable_path.size == 0) {
		EditorSetConsoleWarn("Failed to find powershell executable location. Source code branch and hash information won't be available");
	}
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateDestroy(EditorState* editor_state) {
	// Destroy the threads for all sandboxes
	unsigned int sandbox_count = GetSandboxCount(editor_state);
	for (unsigned int index = 0; index < sandbox_count; index++) {
		EditorSandbox* sandbox = GetSandbox(editor_state, index);
		sandbox->sandbox_world.task_manager->DestroyThreads();
	}

	// Destroy the editor state threads
	editor_state->task_manager->DestroyThreads();
	editor_state->render_task_manager->DestroyThreads();

	DestroyGraphics(editor_state->ui_system->m_graphics);
	FreeAllocator(editor_state->GlobalMemoryManager());

	// If necessary, free all the Malloc's for the individual allocations - but there are very small and insignificant
	// Not worth freeing them since EditorStateInitialize won't be called more than a couple of times during the runtime of 
	// an instance of the process (at the moment only once)
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

void EditorStateLazyEvaluationTrigger(EditorState* editor_state, unsigned int index)
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
	EDITOR_APPLICATION_QUIT_RESPONSE* response;
};

void ApplicationQuitHandler(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ApplicationQuitHandlerData* data = (ApplicationQuitHandlerData*)_data;
	SaveScenePopUpResult* result = (SaveScenePopUpResult*)_additional_data;

	if (result->cancel_call) {
		*data->response = EDITOR_APPLICATION_QUIT_ABORTED;
	}
	else {
		*data->response = EDITOR_APPLICATION_QUIT_APPROVED;
	}
}

void EditorStateApplicationQuit(EditorState* editor_state, EDITOR_APPLICATION_QUIT_RESPONSE* response)
{
	ApplicationQuitHandlerData quit_data;
	quit_data.response = response;

	// Exclude temporary sandboxes from consideration
	unsigned int sandbox_count = GetSandboxCount(editor_state, true);
	ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, sandbox_indices, sandbox_count);
	sandbox_indices.size = sandbox_indices.capacity;
	MakeSequence(sandbox_indices);
	CreateSaveScenePopUp(editor_state, sandbox_indices, { ApplicationQuitHandler, &quit_data, sizeof(quit_data) });
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateSetDatabasePath(EditorState* editor_state)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, folder, 512);
	GetProjectMetadataFolder(editor_state, folder);
	editor_state->asset_database->SetFileLocation(folder);
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateBeforeExitCleanup(EditorState* editor_state)
{
	// At the moment, this is not really necessary
	/*editor_state->task_manager->SleepUntilDynamicTasksFinish();
	
	SandboxAction(editor_state, -1, [&](unsigned int sandbox_index) {
		DestroySandbox(editor_state, 0);
	});

	DestroyGraphics(editor_state->RuntimeGraphics());
	DestroyGraphics(editor_state->UIGraphics());*/

	// The the gpu stats recording as well
	FreeGPUStatsRecording();
}

// -----------------------------------------------------------------------------------------------------------------

void EditorRegisterMainThreadID() {
	MAIN_THREAD_ID = OS::GetCurrentThreadID();
}

void EditorRegisterBackgroundThreadsID(const EditorState* editor_state) {
	unsigned int thread_count = editor_state->task_manager->GetThreadCount();
	BACKGROUND_THREADS_ID.Resize(ECS_MALLOC_ALLOCATOR, thread_count);
	BACKGROUND_THREAD_SPECIFIC_CRASH_HANDLER.Resize(ECS_MALLOC_ALLOCATOR, thread_count);
	for (unsigned int index = 0; index < editor_state->task_manager->GetThreadCount(); index++) {
		BACKGROUND_THREADS_ID[index] = OS::GetThreadID(editor_state->task_manager->m_thread_handles[index]);
	}
	memset(BACKGROUND_THREAD_SPECIFIC_CRASH_HANDLER.buffer, 0, BACKGROUND_THREAD_SPECIFIC_CRASH_HANDLER.MemoryOf(thread_count));
}

void EditorClearMainThreadCrashHandlerIntercept() {
	MAIN_THREAD_INTERCEPT_CRASH_HANDLER = false;
}

void EditorSetMainThreadCrashHandlerIntercept() {
	MAIN_THREAD_INTERCEPT_CRASH_HANDLER = true;
}

// -----------------------------------------------------------------------------------------------------------------

static void InduceSEHCrashHandler(void* data, Stream<char> error_string) {
	// This guarantees a memory exception
	int* invalid_ptr = (int*)0x0;
	*invalid_ptr = 0;
}

CrashHandler EditorInduceSEHCrashHandler() {
	return { InduceSEHCrashHandler, nullptr };
}

CrashHandler EditorGetSimulationThreadsCrashHandler() {
	return SIMULATION_THREADS_CRASH_HANDLER;
}

CrashHandler EditorGetBackgroundThreadsCrashHandler() {
	return BACKGROUND_THREADS_CRASH_HANDLER;
}

// -----------------------------------------------------------------------------------------------------------------

CrashHandler EditorSetSimulationThreadsCrashHandler(CrashHandler crash_handler) {
	CrashHandler previous_handler = SIMULATION_THREADS_CRASH_HANDLER;
	SIMULATION_THREADS_CRASH_HANDLER = crash_handler;
	return previous_handler;
}

CrashHandler EditorSetBackgroundThreadsCrashHandler(CrashHandler crash_handler) {
	CrashHandler previous_handler = BACKGROUND_THREADS_CRASH_HANDLER;
	BACKGROUND_THREADS_CRASH_HANDLER = crash_handler;
	return previous_handler;
}

CrashHandler EditorSetBackgroundThreadSpecificCrashHandler(CrashHandler crash_handler) {
	size_t running_thread_id = OS::GetCurrentThreadID();
	for (size_t index = 0; index < BACKGROUND_THREADS_ID.size; index++) {
		if (BACKGROUND_THREADS_ID[index] == running_thread_id) {
			CrashHandler previous_handler = BACKGROUND_THREAD_SPECIFIC_CRASH_HANDLER[index];
			BACKGROUND_THREAD_SPECIFIC_CRASH_HANDLER[index] = crash_handler;
			return previous_handler;
		}
	}

	// Shouldn't happen
	__debugbreak();
	abort();
	return {};
}

// -----------------------------------------------------------------------------------------------------------------
