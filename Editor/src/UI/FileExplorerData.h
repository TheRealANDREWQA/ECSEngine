#pragma once
#include "ECSEngineUI.h"

using FileExplorerFunctorTable = ECSEngine::HashTableDefault<ECSEngine::Tools::Action>;

struct EditorState;

struct FileExplorerPreloadTexture {
	ECSEngine::Stream<wchar_t> path;
	size_t last_write_time;
	ECSEngine::ResourceView texture;
};

struct FileExplorerMeshThumbnail {
	ECSEngine::ResourceView texture;
	size_t last_write_time;
	bool could_be_read;
};

struct FileExplorerData {
	ECSEngine::CapacityStream<wchar_t> current_directory;
	ECSEngine::ResizableStream<ECSEngine::Stream<wchar_t>> selected_files;
	ECSEngine::Stream<ECSEngine::Stream<wchar_t>> copied_files;
	ECSEngine::CapacityStream<char> filter_stream;
	ECSEngine::LinearAllocator temporary_allocator;
	FileExplorerFunctorTable file_functors;
	ECSEngine::CapacityStream<ECSEngine::Tools::UIActionHandler> add_handlers;
	ECSEngine::CapacityStream<ECSEngine::Tools::UIActionHandler> file_right_click_handlers;
	ECSEngine::CapacityStream<ECSEngine::Tools::UIActionHandler> folder_right_click_handlers;
	ECSEngine::CapacityStream<ECSEngine::Tools::UIActionHandler> deselection_right_click_handlers;
	// Right click handlers take as arguments Stream<wchar_t>, so an extra must be set
	// By the right click callback in order to have them behave correctly
	ECSEngine::Stream<wchar_t> right_click_stream;
	ECSEngine::ResizableStream<FileExplorerPreloadTexture> preloaded_textures;
	// Each thread will commit its results into this staging textures that will be moved into the main pool
	// By the main thread back in the next frame
	ECSEngine::ResizableStream<FileExplorerPreloadTexture> staging_preloaded_textures;

	// Here we will keep the mesh thumbnails. If a mesh cannot be read record that so that we won't keep
	// retrying.
	ECSEngine::HashTableDefault<FileExplorerMeshThumbnail> mesh_thumbnails;

	unsigned int starting_shift_index;
	unsigned int ending_shift_index;
	// Cannot use the same flags for preloading because a separate thread will update the END_PRELOAD flag possibly at the same
	// time as a COPY flag is being written
	unsigned int preload_flags;
	unsigned int flags;
};

void InitializeFileExplorer(EditorState* editor_state);