#pragma once
#include "ECSEngineUI.h"

constexpr const char* FILE_EXPLORER_WINDOW_NAME = "File Explorer";

using FileExplorerFunctorTable = ECSEngine::containers::IdentifierHashTable<ECSEngine::Tools::Action, ECSEngine::ResourceIdentifier, ECSEngine::HashFunctionPowerOfTwo>;

struct EditorState;

// Needs to be initialized by the editor
struct FileExplorerData {
	ECSEngine::containers::CapacityStream<wchar_t> current_directory;
	ECSEngine::containers::CapacityStream<wchar_t> current_file;
	ECSEngine::containers::CapacityStream<char> filter_stream;
	ECSEngine::LinearAllocator allocator;
	FileExplorerFunctorTable file_functors;
	ECSEngine::containers::CapacityStream<ECSEngine::Tools::UIActionHandler> add_handlers;
	ECSEngine::containers::CapacityStream<ECSEngine::Tools::UIActionHandler> file_right_click_handlers;
	ECSEngine::containers::CapacityStream<ECSEngine::Tools::UIActionHandler> folder_right_click_handlers;
	// Right click handlers take as arguments Stream<wchar_t>, so an extra must be set
	// By the right click callback in order to have them behave correctly
	ECSEngine::containers::Stream<wchar_t> right_click_stream;
	size_t flags;
};

// Stack memory size should be at least 512
void FileExplorerSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

template<bool initialize>
void FileExplorerDraw(void* window_data, void* drawer_descriptor);

void CreateFileExplorer(EditorState* editor_state);

unsigned int CreateFileExplorerWindow(EditorState* editor_state);

void CreateFileExplorerAction(ECSEngine::Tools::ActionData* action_data);

void ChangeFileExplorerDirectory(EditorState* editor_state, Stream<wchar_t> path);

void ChangeFileExplorerFile(EditorState* editor_state, Stream<wchar_t> path);