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
	// This is a structure needed to use the retained mode - in order to detect
	// Changes to the content. Everything that is used for these streams
	// Is allocated from the allocator inside here
	struct {
		ECSEngine::ResizableStream<ECSEngine::Stream<wchar_t>> elements;
		ECSEngine::ResizableStream<FileExplorerPreloadTexture> preload_texture_check;
		ECSEngine::ResizableStream<FileExplorerMeshThumbnailWithPath> mesh_thumbnail_check;
		ECSEngine::MemoryManager retained_mode_allocator;
	};

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