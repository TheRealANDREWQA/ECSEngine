// ECS_REFLECT
#pragma once
#include "DeterminismTest.h"

struct EditorState;

#define EDITOR_TEST_BUNDLE_FILE_EXTENSION L".testbundle"

// Contains the common fields between the normal TestBundle and its serialized version
struct ECS_REFLECT TestBundleBase {
	// The target file where the test bundle will be written to. It must be relative to the project root
	Stream<wchar_t> file;
	
	// Each test type will have an array here
	ResizableStream<InputStateDeterminismTestOptions> input_state_determinism;
};

// This structures combines multiple types of tests can be run together to form an overall test
struct TestBundle {
	// Multiple individual test bundles can be authored and includes into another
	// Bundle to make organizing tests easier. For example, for a project, you can
	// Have a test bundle that tests everything, and individual bundles that test
	// A specific functionality or a specific level. That level can be broken down
	// Into multiple bundles as well, it's up to the user how they decide how to
	// Structure the tests
	ResizableStream<TestBundle> nested_bundles;
};

// This structure is a mirror of the normal TestBundle, but adapted for the serialization context.
// It replaces the test bundle references with only the file references, such that only this data
// Is written
struct ECS_REFLECT TestBundleSerialize : TestBundleBase {
	ResizableStream<Stream<wchar_t>> nested_bundles;
};

// Returns a valid optional if the test bundle could be read, else an invalid optional. The given allocator
// Is used for all the buffer allocations needed for this entry. The file must be the absolute file path, not
// The relative path. If the absolute file path is that of a directory, then it will treat everything inside
// It as a separate entry and sub-directories are considered nested bundles
Optional<TestBundle> ReadTestBundle(const EditorState* editor_state, Stream<wchar_t> absolute_file_path, TestBundle& test_bundle, AllocatorPolymorphic allocator);

// Returns true if it succeeded, else false
bool WriteTestBundle(const EditorState* editor_state, const TestBundle& test_bundle);