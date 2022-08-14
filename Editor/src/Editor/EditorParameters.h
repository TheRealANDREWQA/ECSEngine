#pragma once
#include <stdint.h>

constexpr size_t SLEEP_AMOUNT[] = { 20, 15, 10, 5, 0 };
constexpr size_t GLOBAL_MEMORY_COUNT = 200'000'000;
constexpr size_t GLOBAL_MEMORY_RESERVE_COUNT = 75'000'000;

constexpr const char VERSION_DESCRIPTION[] = "0.1.0";
constexpr unsigned char VERSION_INDEX = 1;
constexpr unsigned char PROJECT_FILE_METADATA_INDEX = 1;
constexpr unsigned char PROJECT_FILE_PLATFORM_INDEX = 1;

constexpr unsigned char COMPATIBLE_PROJECT_FILE_VERSION_INDEX = 1;

constexpr const char USER[] = "ANDREWQA";

constexpr const wchar_t* CONSOLE_RELATIVE_DUMP_PATH = L"\\Debug\\ConsoleDump.txt";

constexpr const wchar_t* EDITOR_DEFAULT_PROJECT_UI_TEMPLATE = L"Resources/DefaultTemplate";
constexpr const wchar_t* EDITOR_SYSTEM_PROJECT_UI_TEMPLATE_PREFIX = L"Resources/EditorTemplate";

constexpr size_t EDITOR_HUB_PROJECT_CAPACITY = 32;
constexpr size_t EDITOR_EVENT_QUEUE_CAPACITY = 32;