#pragma once
#include "ECSEngineUI.h"

constexpr const char* FILE_EXPLORER_WINDOW_NAME = "File Explorer";

using FileExplorerFunctorTable = ECSEngine::containers::HashTable<ECSEngine::Tools::Action, ECSEngine::ResourceIdentifier, ECSEngine::HashFunctionPowerOfTwo, ECSEngine::HashFunctionMultiplyString>;

struct EditorState;

struct FileExplorerPreloadTexture {
	ECSEngine::containers::Stream<wchar_t> path;
	size_t last_write_time;
	ECSEngine::ResourceView texture;
};

struct FileExplorerData {
	ECSEngine::containers::CapacityStream<wchar_t> current_directory;
	ECSEngine::containers::ResizableStream<ECSEngine::containers::Stream<wchar_t>, ECSEngine::MemoryManager> selected_files;
	ECSEngine::containers::Stream<ECSEngine::containers::Stream<wchar_t>> copied_files;
	ECSEngine::containers::CapacityStream<char> filter_stream;
	ECSEngine::LinearAllocator temporary_allocator;
	FileExplorerFunctorTable file_functors;
	ECSEngine::containers::CapacityStream<ECSEngine::Tools::UIActionHandler> add_handlers;
	ECSEngine::containers::CapacityStream<ECSEngine::Tools::UIActionHandler> file_right_click_handlers;
	ECSEngine::containers::CapacityStream<ECSEngine::Tools::UIActionHandler> folder_right_click_handlers;
	ECSEngine::containers::CapacityStream<ECSEngine::Tools::UIActionHandler> deselection_right_click_handlers;
	// Right click handlers take as arguments Stream<wchar_t>, so an extra must be set
	// By the right click callback in order to have them behave correctly
	ECSEngine::containers::Stream<wchar_t> right_click_stream;
	ECSEngine::containers::ResizableStream<FileExplorerPreloadTexture, ECSEngine::MemoryManager> preloaded_textures;
	// Each thread will commit its results into this staging textures that will be moved into the main pool
	// By the main thread back in the next frame
	ECSEngine::containers::Stream<ECSEngine::containers::Stream<FileExplorerPreloadTexture>> staging_preloaded_textures;
	unsigned int starting_shift_index;
	unsigned int ending_shift_index;
	unsigned int flags;
};

void InitializeFileExplorer(FileExplorerData* file_explorer_data, ECSEngine::MemoryManager* allocator);

// Stack memory size should be at least 512
void FileExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

template<bool initialize>
void FileExplorerDraw(void* window_data, void* drawer_descriptor);

void CreateFileExplorer(EditorState* editor_state);

unsigned int CreateFileExplorerWindow(EditorState* editor_state);

void CreateFileExplorerAction(ECSEngine::Tools::ActionData* action_data);

// Index is used to reset the shift indices, can be omitted by external setters
void ChangeFileExplorerDirectory(EditorState* editor_state, Stream<wchar_t> path, unsigned int index = -1);

// Index is used to reset the shift indices, can be omitted by external setters
void ChangeFileExplorerFile(EditorState* editor_state, Stream<wchar_t> path, unsigned int index = -1);