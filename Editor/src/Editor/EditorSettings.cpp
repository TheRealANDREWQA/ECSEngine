#include "editorpch.h"
#include "EditorSettings.h"
#include "EditorState.h"
#include "EditorFile.h"

using namespace ECSEngine;

bool ChangeCompilerVersion(EditorState* editor_state, Stream<wchar_t> path) {
	editor_state->settings.compiler_path.Deallocate(editor_state->EditorAllocator());
	editor_state->settings.compiler_path = path.Copy(editor_state->EditorAllocator());
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
					bool success = false;
					int64_t year = ConvertCharactersToIntStrict(folders[index], success);
					if (success) {
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
									default_path.AddStreamAssert(L"\\MSBuild\\Current\\Bin");

									CapacityStream<char> aggregated;
									// use a small capacity
									aggregated.Initialize(allocator, 0, sizeof(char) * 40);
									ConvertWideCharsToASCII(available_tiers[subindex], aggregated);
									Stream<char> tier = aggregated;
									aggregated.AddAssert(' ');
									size_t written_count = ConvertIntToChars(aggregated, year);
									Stream<char> year = aggregated.SliceAt(aggregated.size - written_count);

									compiler_versions.Add({ aggregated, year, tier, default_path.Copy(allocator) });
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
	
	// Search the x96 path
	stack_allocator.Clear();
	folders.size = 0;
	default_path.CopyOther(L"C:\\Program Files (x86)\\Microsoft Visual Studio");
	
	search_folder();
}