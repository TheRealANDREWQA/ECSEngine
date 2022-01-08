#pragma once
#include <stdint.h>

constexpr size_t SLEEP_AMOUNT[] = { 12, 8, 6, 4, 0 };
constexpr size_t GLOBAL_MEMORY_COUNT = 200'000'000;
constexpr size_t GLOBAL_MEMORY_RESERVE_COUNT = 75'000'000;

constexpr const wchar_t FONT_PATH[] = L"Resources/Font.tiff";
constexpr const wchar_t FONT_DESCRIPTOR_PATH[] = L"Resources/FontDescription_v2.txt";

constexpr const wchar_t UI_DESCRIPTOR_FILE[] = L"UIDescriptor.txt";
constexpr const wchar_t UI_SYSTEM_FILE[] = L"UISystemFile.txt";

constexpr const char VERSION_DESCRIPTION[] = "0.1.0";
constexpr unsigned char VERSION_INDEX = 1;
constexpr unsigned char PROJECT_FILE_METADATA_INDEX = 1;
constexpr unsigned char PROJECT_FILE_PLATFORM_INDEX = 1;

constexpr unsigned char COMPATIBLE_PROJECT_FILE_VERSION_INDEX = 1;

constexpr const char USER[] = "ANDREWQA";

constexpr size_t CREATE_PROJECT_DEFAULT_GLOBAL_MEMORY_SIZE = 60'000'000;
constexpr size_t CREATE_PROJECT_DEFAULT_GLOBAL_POOL_COUNT = 1024;
constexpr size_t CREATE_PROJECT_DEFAULT_GLOBAL_NEW_ALLOCATION = 20'000'000;
constexpr unsigned int CREATE_PROJECT_DEFAULT_TASK_MANAGER_MAX_DYNAMIC_TASKS = 64;
constexpr unsigned int CREATE_PROJECT_DEFAULT_ENTITY_MANAGER_MAX_DYNAMIC_ARCHETYPE_COUNT = 256;
constexpr unsigned int CREATE_PROJECT_ENTITY_MANAGER_MAX_STATIC_ARCHETYPE_COUNT = 256;
constexpr unsigned int CREATE_PROJECT_DEFAULT_ENTITY_POOL_POWER_OF_TWO = 20;
constexpr unsigned int CREATE_PROJECT_DEFAULT_ENTITY_POOL_ARENA_COUNT = 8;
constexpr unsigned int CREATE_PROJECT_DEFAULT_ENTITY_POOL_BLOCK_COUNT = 1024;
constexpr unsigned int CREATE_PROJECT_DEFAULT_SYSTEM_MANAGER_MAX_SYSTEMS = 1024;
constexpr size_t CREATE_PROJECT_DEFAULT_MEMORY_MANAGER_SIZE = 50'000'000;
constexpr size_t CREATE_PROJECT_DEFAULT_MEMORY_MANAGER_MAXIMUM_POOL_COUNT = 1024;
constexpr size_t CREATE_PROJECT_DEFAULT_MEMORY_MANAGER_NEW_ALLOCATION_SIZE = 20'000'000;

constexpr const wchar_t* CONSOLE_RELATIVE_DUMP_PATH = L"/Debug/ConsoleDump.txt";

constexpr const wchar_t* EDITOR_DEFAULT_PROJECT_UI_TEMPLATE = L"Resources/DefaultTemplate";
constexpr const wchar_t* EDITOR_SYSTEM_PROJECT_UI_TEMPLATE_PREFIX = L"Resources/EditorTemplate";

constexpr size_t EDITOR_HUB_PROJECT_CAPACITY = 32;
constexpr size_t EDITOR_EVENT_QUEUE_CAPACITY = 32;

constexpr size_t EDITOR_SCENE_BUFFERING_COUNT = 3;