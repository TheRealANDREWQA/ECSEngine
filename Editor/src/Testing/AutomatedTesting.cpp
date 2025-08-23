#include "editorpch.h"
#include "AutomatedTesting.h"
#include "../Editor/EditorState.h"

Optional<TestBundle> ReadTestBundle(const EditorState* editor_state, Stream<wchar_t> absolute_file_path, TestBundle& test_bundle, AllocatorPolymorphic allocator)
{
    Optional<TestBundle> bundle;

    Stream<wchar_t> extension = PathExtension(absolute_file_path);

    if (extension.size == 0) {

    }
    else if (extension != EDITOR_TEST_BUNDLE_FILE_EXTENSION) {
        EDITOR_LOG_FORMAT_ERROR("The provided file {#} is not a valid test bundle file - invalid extension", absolute_file_path);
        return bundle;
    }

    ECS_STACK_CAPACITY_STREAM(char, error_message, ECS_KB * 4);

    DeserializeOptions deserialize_options;
    deserialize_options.default_initialize_missing_fields = true;
    deserialize_options.error_message = &error_message;
    deserialize_options.field_allocator = allocator;

    TestBundleSerialize serialized_bundle;
    if (Deserialize(editor_state->GlobalReflectionManager(), editor_state->GlobalReflectionManager()->GetType(STRING(TestBundleSerialize)), &serialized_bundle, absolute_file_path, &deserialize_options) != ECS_DESERIALIZE_OK) {
        // Print the error message and fail
        EDITOR_LOG_FORMAT_ERROR("Failed to read test bundle file {#}. Reason: {#}", absolute_file_path, error_message);
        return bundle;
    }

    return bundle;
}

bool WriteTestBundle(const EditorState* editor_state, const TestBundle& test_bundle)
{
    return false;
}
