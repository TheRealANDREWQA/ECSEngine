#pragma once
#include "ECSEngineUI.h"

typedef ECSEngine::HashTableDefault<ECSEngine::Tools::Action> FileExplorerFunctorTable;

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

struct FileExplorerMeshThumbnailWithPath {
	ECSEngine::Stream<wchar_t> path;
	FileExplorerMeshThumbnail thumbnail;
};

struct FileExplorerData {
	// This flag is used to redraw the explorer when a change is being made
	// The callbacks or events/background tasks need to set this in order to
	// Have the UI redrawn
	bool should_redraw;

	// This is an array with all the files/directory which have been displayed
	// in the last draw call. It is used when new items have been added or removed
	// to make a redraw in that case
	ECSEngine::ResizableStream<ECSEngine::Stream<wchar_t>> displayed_items;
	// Use a special allocator for the displayed items such that we can clear the
	// memory just by clearing this allocator. Use 2 such allocators in order to
	// Have one keep the old data and the other one be used for the new data that will
	// be compared with the old one. In case they are different, we just have clear the
	// old allocator and change the allocator index
	ECSEngine::ResizableLinearAllocator displayed_items_allocator[2];
	unsigned int displayed_items_allocator_index;

	ECSEngine::CapacityStream<wchar_t> current_directory;
	ECSEngine::ResizableStream<ECSEngine::Stream<wchar_t>> selected_files;
	ECSEngine::Stream<ECSEngine::Stream<wchar_t>> copied_files;
	ECSEngine::CapacityStream<char> filter_stream;
	ECSEngine::LinearAllocator temporary_allocator;
	FileExplorerFunctorTable file_functors;

	ECSEngine::CapacityStream<ECSEngine::Tools::UIActionHandler> add_handlers;
	ECSEngine::CapacityStream<ECSEngine::Tools::UIActionHandler> file_right_click_handlers;
	ECSEngine::CapacityStream<ECSEngine::Tools::UIActionHandler> folder_right_click_handlers;
	ECSEngine::CapacityStream<ECSEngine::Tools::UIActionHandler> mesh_export_materials_click_handlers;

	ECSEngine::CapacityStream<ECSEngine::Tools::UIActionHandler> deselection_menu_handlers;
	ECSEngine::CapacityStream<ECSEngine::Tools::UIActionHandler> deselection_create_menu_handlers;
	// For the asset creation
	ECSEngine::Tools::TextInputWizardData* deselection_create_menu_handler_data;
	ECSEngine::Tools::UIDrawerMenuState deselection_menu_create_state;

	// Right click handlers take as arguments Stream<wchar_t>, so an extra must be set
	// By the right click callback in order to have them behave correctly
	ECSEngine::Stream<wchar_t> right_click_stream;
	ECSEngine::ResizableStream<FileExplorerPreloadTexture> preloaded_textures;
	// Each thread will commit its results into this staging textures that will be moved into the main pool
	// By the main thread back in the next frame
	ECSEngine::CapacityStream<FileExplorerPreloadTexture> staging_preloaded_textures;

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

// Returns a nullptr view if it doesn't exist
// Implemented in FileExplorer.cpp
ECSEngine::ResourceView FileExplorerGetMeshThumbnail(const EditorState* editor_state, ECSEngine::Stream<wchar_t> absolute_path);