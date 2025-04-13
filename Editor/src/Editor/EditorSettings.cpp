#include "editorpch.h"
#include "EditorSettings.h"
#include "EditorState.h"
#include "EditorFile.h"

using namespace ECSEngine;

bool ChangeCompilerVersion(EditorState* editor_state, Stream<wchar_t> path) {
	editor_state->settings.compiler_path.Deallocate(editor_state->EditorAllocator());
	// It needs to have an ending null terminator character
	editor_state->settings.compiler_path = StringCopy(editor_state->EditorAllocator(), path);
	return SaveEditorFile(editor_state);
}

bool ChangeEditingIdeExecutablePath(EditorState* editor_state, Stream<wchar_t> path) {
	editor_state->settings.editing_ide_path.Deallocate(editor_state->EditorAllocator());
	// It needs to have an ending null terminator character
	editor_state->settings.editing_ide_path = StringCopy(editor_state->EditorAllocator(), path);
	return SaveEditorFile(editor_state);
}

bool ChangeCompilerVersionAndEditingIdeExecutablePath(EditorState* editor_state, Stream<wchar_t> compiler_path, Stream<wchar_t> editing_ide_executable_path) {
	editor_state->settings.compiler_path.Deallocate(editor_state->EditorAllocator());
	// It needs to have an ending null terminator character
	editor_state->settings.compiler_path = StringCopy(editor_state->EditorAllocator(), compiler_path);

	editor_state->settings.editing_ide_path.Deallocate(editor_state->EditorAllocator());
	// It needs to have an ending null terminator character
	editor_state->settings.editing_ide_path = StringCopy(editor_state->EditorAllocator(), editing_ide_executable_path);
	return SaveEditorFile(editor_state);
}

void AutoDetectCompilers(AllocatorPolymorphic allocator, AdditionStream<CompilerVersion> compiler_versions) {
	// Use the default installation path
	// Start with the latest version of MSVC, which is 2022
	// And it is 64 bit only, located at C:\Program Files\Microsoft Visual Studio\2022

	// Then the other versions are in C:\Program Files (x86)\Microsoft Visual Studio\*
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB * 8);

	ECS_STACK_CAPACITY_STREAM(wchar_t, default_path, 512);
	default_path.CopyOther(L"C:\\Program Files\\Microsoft Visual Studio");

	ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, folders, 512);

	auto search_folder = [&]() {
		// The tier refers to the visual studio plans
		// Community, Professional and Enterprise at the current time
		// Return true if it found a tier and adds it to the path
		// Else false
		const Stream<wchar_t> tiers[] = {
			L"Enterprise",
			L"Professional",
			L"Community"
		};

		// Returns the available tier, 
		auto determine_visual_studio_tier = [tiers](CapacityStream<wchar_t>& path, CapacityStream<Stream<wchar_t>>& available_tiers) {
			unsigned int path_size = path.size;
			for (size_t index = 0; index < ECS_COUNTOF(tiers); index++) {
				path.size = path_size;
				if (path[path.size - 1] != ECS_OS_PATH_SEPARATOR) {
					path.AddAssert(ECS_OS_PATH_SEPARATOR);
				}
				path.AddStreamAssert(tiers[index]);
				if (ExistsFileOrFolder(path)) {
					available_tiers.AddAssert(tiers[index]);
				}
			}

			path.size = path_size;
		};

		if (ExistsFileOrFolder(default_path)) {
			GetDirectoriesOrFilesOptions options;
			options.relative_root = default_path;
			bool success = GetDirectories(default_path, &stack_allocator, &folders, options);
			if (success) {
				default_path.Add(ECS_OS_PATH_SEPARATOR);
				unsigned int initial_path_size = default_path.size;
				// Consider only the directories that are numbers
				for (unsigned int index = 0; index < folders.size; index++) {
					bool convert_success = false;
					int64_t year = ConvertCharactersToIntStrict(folders[index], convert_success);
					if (convert_success) {
						// Sanity Clamp
						if (year >= 2010 && year < 2100) {
							default_path.size = initial_path_size;
							ConvertIntToChars(default_path, year);
							default_path.Add(ECS_OS_PATH_SEPARATOR);
							ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, available_tiers, 32);
							determine_visual_studio_tier(default_path, available_tiers);
							if (available_tiers.size > 0) {
								for (size_t subindex = 0; subindex < available_tiers.size; subindex++) {
									default_path.AddStreamAssert(available_tiers[subindex]);

									ECS_STACK_CAPACITY_STREAM(wchar_t, editing_ide_executable_path, 1024);
									editing_ide_executable_path.CopyOther(default_path);

									default_path.AddStreamAssert(L"\\MSBuild\\Current\\Bin\\MSBuild.exe");
									editing_ide_executable_path.AddStreamAssert(L"\\Common7\\IDE\\devenv.exe");

									CapacityStream<char> aggregated;
									// Use a small capacity
									aggregated.Initialize(allocator, 0, sizeof(char) * 128);

									// At the moment, the only compiler is Visual Studio
									aggregated.AddStreamAssert("Visual Studio");
									Stream<char> compiler_name = aggregated;
									aggregated.AddAssert(' ');
									unsigned int tier_start_character = aggregated.size;
									ConvertWideCharsToASCII(available_tiers[subindex], aggregated);
									Stream<char> tier = aggregated.SliceAt(tier_start_character);
									aggregated.AddAssert(' ');

									size_t written_count = ConvertIntToChars(aggregated, year);
									Stream<char> year_string = aggregated.SliceAt(aggregated.size - written_count);
									compiler_versions.Add({ aggregated, year_string, tier, compiler_name, default_path.Copy(allocator), editing_ide_executable_path.Copy(allocator) });
								}
							};
						}
					}
				}
			}
		}
	};
	
	// Search the x64 path
	search_folder();
	
	// Search the x86 path
	stack_allocator.Clear();
	folders.size = 0;
	default_path.CopyOther(L"C:\\Program Files (x86)\\Microsoft Visual Studio");
	
	search_folder();
}