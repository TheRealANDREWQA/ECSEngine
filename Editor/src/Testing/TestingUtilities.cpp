#include "editorpch.h"
#include "TestingUtilities.h"
#include "../Editor/EditorState.h"

bool ReadTestConfigurationFile(
    const EditorState* editor_state,
    void* test_configuration,
    Stream<char> test_struct_name,
    Stream<wchar_t> absolute_path,
    AllocatorPolymorphic test_configuration_allocator
) {
    const Reflection::ReflectionManager* reflection_manager = editor_state->ModuleReflectionManager();

    ECS_STACK_CAPACITY_STREAM(char, error_message, ECS_KB);
    DeserializeOptions deserialize_options;
    deserialize_options.error_message = &error_message;
    deserialize_options.field_allocator = test_configuration_allocator;
    deserialize_options.default_initialize_missing_fields = true;
    deserialize_options.verify_dependent_types = false;
    if (Deserialize(reflection_manager, reflection_manager->GetType(test_struct_name), test_configuration, absolute_path, &deserialize_options) != ECS_DESERIALIZE_OK) {
        ECS_FORMAT_TEMP_STRING(final_error_message, "Faild to read test configuration from path {#}. Reason: {#}", absolute_path, error_message);
        return false;
    }

    return true;
}

bool WriteTestConfigurationFile(const EditorState* editor_state, const void* test_configuration, Stream<char> test_struct_name, Stream<wchar_t> absolute_path) {
    const Reflection::ReflectionManager* reflection_manager = editor_state->ModuleReflectionManager();

    ECS_STACK_CAPACITY_STREAM(char, error_message, ECS_KB);
    SerializeOptions serialize_options;
    serialize_options.error_message = &error_message;
    serialize_options.verify_dependent_types = false;
    if (Serialize(reflection_manager, reflection_manager->GetType(test_struct_name), test_configuration, absolute_path, &serialize_options) != ECS_SERIALIZE_OK) {
        ECS_FORMAT_TEMP_STRING(final_error_message, "Failed to write test configuration at path {#}. Reason: {#}", absolute_path, error_message);
        EditorSetConsoleError(final_error_message);
        return false;
    }

    return true;
}
