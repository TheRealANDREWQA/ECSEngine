#pragma once
#include "ECSEngineUI.h"

using namespace ECSEngine;
ECS_CONTAINERS;
ECS_TOOLS;

struct OSFileExplorerGetFileData {
	CapacityStream<wchar_t> path;
	CapacityStream<char> error_message = {nullptr, 0, 0};
	const wchar_t* initial_directory = nullptr;
	const wchar_t* filter = nullptr;
	size_t filter_count = 0;
	HWND hWnd = NULL;
};

struct OSFileExplorerGetDirectoryData {
	CapacityStream<wchar_t> path;
	CapacityStream<char> error_message = {nullptr, 0, 0};
};

struct OSFileExplorerGetFileActionData {
	OSFileExplorerGetFileData get_file_data;
	UIDrawerTextInput* input = nullptr;
	CapacityStream<wchar_t>* update_stream = nullptr;
};

struct OSFileExplorerGetDirectoryActionData {
	OSFileExplorerGetDirectoryData get_directory_data;
	UIDrawerTextInput* input = nullptr;
	CapacityStream<wchar_t>* update_stream = nullptr;
};

void SetBasicErrorMessage(const char* error, CapacityStream<char>& message);

bool FileExplorerGetFile(OSFileExplorerGetFileData* data);

bool FileExplorerGetDirectory(OSFileExplorerGetDirectoryData* data);

void FileExplorerGetFileAction(ActionData* action_data);

void FileExplorerGetDirectoryAction(ActionData* action_data);