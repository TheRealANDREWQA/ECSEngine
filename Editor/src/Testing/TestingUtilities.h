#pragma once
#include "ECSEngineContainersCommon.h"

struct EditorState;

// Deserializes the test configuration from the provided file path and returns true if it succeeded, else false.
// The buffers that the test configuration might be using will be allocated from the provided allocator parameter.
// In case it fails, it will print the reason of the failure into the console
bool ReadTestConfigurationFile(
	const EditorState* editor_state, 
	void* test_configuration, 
	ECSEngine::Stream<char> test_struct_name, 
	ECSEngine::Stream<wchar_t> absolute_path, 
	AllocatorPolymorphic test_configuration_allocator
);

// Deserializes the test configuration from the provided file path and returns true if it succeeded, else false.
// The buffers that the test configuration might be using will be allocated from the provided allocator parameter.
// In case it fails, it will print the reason of the failure into the console
template<typename T>
ECS_INLINE bool ReadTestConfigurationFile(const EditorState* editor_state, T* test_configuration, ECSEngine::Stream<wchar_t> absolute_path, AllocatorPolymorphic test_configuration_allocator) {
	return ReadTestConfigurationFile(editor_state, test_configuration, ECSEngine::GetTemplateName<T>(), absolute_path, test_configuration_allocator);
}

// Serializes the test configuration structure into the provided file path and returns true if it succeeded, else false.
// It will print an error message in case it failed.
bool WriteTestConfigurationFile(const EditorState* editor_state, const void* test_configuration, ECSEngine::Stream<char> test_struct_name, ECSEngine::Stream<wchar_t> absolute_path);

// Serializes the test configuration structure into the provided file path and returns true if it succeeded, else false.
// It will print an error message in case it failed.
template<typename T>
ECS_INLINE bool WriteTestConfigurationFile(const EditorState* editor_state, const T* test_configuration, ECSEngine::Stream<wchar_t> absolute_path) {
	return WriteTestConfigurationFile(editor_state, test_configuration, ECSEngine::GetTemplateName<T>(), absolute_path);
}